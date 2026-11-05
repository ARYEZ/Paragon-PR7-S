#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"

// 0.44.0, the chord tool, round two: recording chords, sections, the remote keys and MIDI Learn,
// your own patterns, a track performing the chord track.

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    using T = Chord::Type;
    using X = Chord::Tension;
    const Chord C { 0, T::major, X::none, -1 }, Am { 9, T::minor, X::none, -1 }, F { 5, T::major, X::none, -1 }, G { 7, T::major, X::none, -1 };

    juce::String names (const std::vector<ChordEvent>& events)
    {
        juce::StringArray out;
        for (const auto& e : events)
            out.add (e.chord.name (Key {}));
        return out.joinIntoString (" ");
    }

    struct Sent
    {
        std::vector<juce::MidiMessage> messages;
        std::vector<int> ons() const
        {
            std::vector<int> out;
            for (const auto& m : messages)
                if (m.isNoteOn())
                    out.push_back (m.getNoteNumber());
            return out;
        }
    };
}

TEST_CASE ("0.44.0: chords recognised in what was played: three notes or more starting together; repeats once", "[chordtool2]")
{
    Studio studio (false);
    std::vector<HeardNote> played;
    const auto chord = [&played] (double at, std::initializer_list<int> notes, double length = 1.9)
    {
        for (const auto n : notes)
            played.push_back ({ at, at + length, n, 0.8f });
    };
    chord (0.0, { 48, 52, 55 });          // C
    played.push_back ({ 1.0, 1.4, 72, 0.8f });   // a melody note alone: no chord
    chord (2.02, { 45, 48, 52 });         // Am (a little late: still together)
    chord (4.0, { 41, 45, 48, 53 });      // F
    chord (6.0, { 41, 45, 48 });          // F again: once
    chord (8.0, { 43, 47, 50, 55 });      // G
    const auto found = studio.chordsFromPlayed (played);
    CHECK (names (found) == "C Am F G");
    REQUIRE (found.size() == 4);
    CHECK_THAT (found[1].beat, WithinAbs (2.02, 1.0e-9));
}

TEST_CASE ("0.44.0: recording the chord track: pads pressed land as chord events on the grid, replacing what was there", "[chordtool2]")
{
    Studio studio (false);
    auto pads = studio.getChordPads();
    pads.pads[0].chord = C;
    pads.pads[1].chord = Am;
    pads.pads[2].chord = F;
    studio.setChordPads (pads);
    studio.addChordEvent (4.0, G);       // (inside the take: replaced)
    studio.addChordEvent (64.0, Am);     // (after it: kept)
    studio.setChordTrackRecord (true);
    CHECK (studio.isChordTrackRecording());
    studio.setSnap (true);
    studio.setQuantizeBeats (1.0);
    auto& player = studio.getPadPlayer();
    player.send = [] (const juce::MidiMessage&) {};
    auto& engine = studio.getEngine();
    studio.setPosition (0.0);
    studio.record();
    REQUIRE (engine.isRecording());
    renderSamples (engine, 2048, 512);   // (the clock moves a little: the pad lands at beat 0 on the grid)
    // (A jump while recording is taken on the next block: one played after each.)
    const auto moveTo = [&] (double beat) { studio.setPosition (beat); renderSamples (engine, 512, 512); };
    player.press (0, 100);
    player.release (0);
    moveTo (4.1);
    player.press (1, 100);
    player.release (1);
    moveTo (7.9);
    player.press (2, 100);
    player.release (2);
    moveTo (12.0);
    studio.record();   // (punch out)
    const auto& events = studio.getChordTrack().events;
    CHECK (names (events) == "C Am F Am");
    REQUIRE (events.size() == 4);
    CHECK_THAT (events[1].beat, WithinAbs (4.0, 1.0e-9));
    CHECK_THAT (events[2].beat, WithinAbs (8.0, 1.0e-9));
    CHECK_THAT (events[3].beat, WithinAbs (64.0, 1.0e-9));
    // Not recording the chord track: nothing written.
    studio.setChordTrackRecord (false);
    studio.setPosition (20.0);
    studio.record();
    player.press (0, 100);
    player.release (0);
    studio.record();
    CHECK (studio.getChordTrack().events.size() == 4);
}

TEST_CASE ("0.44.0: sections: a pad's key picks the chord, the section keys play its notes from the bottom, subsections an octave up", "[chordtool2]")
{
    Studio studio (false);
    auto pads = studio.getChordPads();
    pads.pads[0].chord = C;
    pads.sectionsMode = true;
    studio.setChordPads (pads);
    studio.setPadsActive (true);
    auto& player = studio.getPadPlayer();
    Sent sent;
    player.send = [&sent] (const juce::MidiMessage& m) { sent.messages.push_back (m); };
    const auto voicing = [&] { auto v = studio.getPadNotes (0); std::sort (v.begin(), v.end()); return v; }();
    REQUIRE (voicing.size() >= 3);
    // The pad key alone: silent in sections mode.
    CHECK (player.handle (juce::MidiMessage::noteOn (1, pads.triggerStart, (juce::uint8) 100)));
    CHECK (sent.ons().empty());
    // Section 1 (C0): the lowest note; section 3: the third from the bottom; subsection 1: the lowest an octave up.
    CHECK (player.handle (juce::MidiMessage::noteOn (1, pads.sectionKeys[0], (juce::uint8) 90)));
    CHECK (player.handle (juce::MidiMessage::noteOn (1, pads.sectionKeys[2], (juce::uint8) 90)));
    CHECK (player.handle (juce::MidiMessage::noteOn (1, pads.subsectionKeys[0], (juce::uint8) 90)));
    CHECK (sent.ons() == std::vector<int> { voicing[0], voicing[2], voicing[0] + 12 });
    // A section key let go: its note ends.
    CHECK (player.handle (juce::MidiMessage::noteOff (1, pads.sectionKeys[0], (juce::uint8) 0)));
    CHECK (sent.messages.back().isNoteOff());
    CHECK (sent.messages.back().getNoteNumber() == voicing[0]);
    // Sections off: the pad plays its chord again.
    player.releaseAll();
    sent.messages.clear();
    pads.sectionsMode = false;
    studio.setChordPads (pads);
    player.handle (juce::MidiMessage::noteOn (1, pads.triggerStart, (juce::uint8) 100));
    CHECK (sent.ons().size() == voicing.size());
    // Saved with the song.
    pads.sectionKeys[4] = 70;
    pads.modifiersOn = true;
    const auto back = ChordPads::fromXml (*pads.toXml());
    CHECK (back == pads);
}

TEST_CASE ("0.44.0: the modifiers act on the last pad played; MIDI Learn takes keys for the range, a modifier, a section", "[chordtool2]")
{
    Studio studio (false);
    auto pads = studio.getChordPads();
    pads.pads[0].chord = C;
    pads.modifiersOn = true;
    studio.setChordPads (pads);
    studio.setPadsActive (true);
    auto& player = studio.getPadPlayer();
    player.send = [] (const juce::MidiMessage&) {};
    player.press (0, 100);
    player.release (0);
    CHECK (studio.getLastPlayedPad() == 0);
    // Transpose up / more tensions on it.
    studio.applyPadModifier (ChordPads::Modifier::transposeUp);
    CHECK (studio.getChordPads().pads[0].chord->root == 1);
    studio.applyPadModifier (ChordPads::Modifier::moreTensions);
    CHECK (studio.getChordPads().pads[0].chord->tension != X::none);
    // A modifier key is taken (not played) and reported.
    std::optional<ChordPads::Modifier> heard;
    player.onModifier = [&heard] (ChordPads::Modifier m) { heard = m; };
    CHECK (player.handle (juce::MidiMessage::noteOn (1, pads.modifierKeys[5], (juce::uint8) 100)));
    REQUIRE (heard.has_value());
    CHECK (*heard == ChordPads::Modifier::transposeDown);

    // MIDI Learn: the range from two keys (either order), then a modifier, a section.
    auto learnedCount = 0;
    studio.onLearned = [&learnedCount] { ++learnedCount; };
    studio.startPadLearn (Studio::PadLearn::range);
    CHECK (player.learning.load());
    studio.learnedKey (60);
    CHECK (studio.getPadLearn() == Studio::PadLearn::range);   // (the second key next)
    studio.learnedKey (48);
    CHECK (studio.getChordPads().triggerStart == 48);
    CHECK (studio.getChordPads().triggerEnd == 60);
    studio.startPadLearn (Studio::PadLearn::modifier, 2);
    studio.learnedKey (90);
    CHECK (studio.getChordPads().modifierKeys[2] == 90);
    studio.startPadLearn (Studio::PadLearn::section, 4);
    studio.learnedKey (20);
    CHECK (studio.getChordPads().sectionKeys[4] == 20);
    CHECK (learnedCount == 3);
    CHECK_FALSE (player.learning.load());
    // While learning, the pad player takes the key and reports it (not played).
    std::optional<int> reported;
    player.onLearn = [&reported] (int note) { reported = note; };
    player.learning = true;
    CHECK (player.handle (juce::MidiMessage::noteOn (1, 77, (juce::uint8) 100)));
    CHECK (reported == 77);
    CHECK_FALSE (player.learning.load());
    studio.cancelPadLearn();
}

TEST_CASE ("0.44.0: your own patterns: a MIDI part made a pattern (voices by pitch, the bass under), in MY PATTERNS, saved and removed", "[chordtool2]")
{
    setUserPatterns ({});
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    // A bar: a bass on C2 then three voices up and down in eighths.
    Part part;
    part.start = 0.0;
    part.length = 4.0;
    part.name = "My Arp";
    part.notes.push_back ({ 0.0, 4.0, 36, 0.9f });
    const int voices[] { 60, 64, 67, 64, 60, 64, 67, 72 };
    for (int i = 0; i < 8; ++i)
        part.notes.push_back ({ i * 0.5, 0.5, voices[i], 0.7f });
    studio.addPart (trackId, part);
    const auto partId = studio.getProject().findTrack (trackId)->parts[0].id;
    const auto name = studio.addPatternFromPart (partId);
    CHECK (name == "My Arp");
    REQUIRE (patternNamed ("My Arp") >= 0);
    CHECK (patternGroups().contains (userPatternGroup));
    const auto& made = patterns()[(size_t) patternNamed ("My Arp")];
    CHECK (made.group == userPatternGroup);
    CHECK (made.bars == 1.0);
    REQUIRE (made.steps.size() == 9);
    CHECK (made.steps.front().voice == PatternStep::bass);   // (C2 a fifth and more under the rest: the bass)
    // The second one by the same name: numbered.
    CHECK (studio.addPatternFromPart (partId) == "My Arp 2");
    // Played over an A minor chord: the voices land on its notes, the bass on its root.
    const auto notes = patternNotes (made, { 57, 60, 64 }, 45, 0.0, 4.0, 0.0, 4.0);
    std::set<int> heard;
    for (const auto& n : notes)
        heard.insert (n.pitch % 12);
    CHECK (heard == std::set<int> { 9, 0, 4 });
    // As text and back.
    const auto again = patternFromXml (*patternToXml (made));
    REQUIRE (again.has_value());
    CHECK (again->steps.size() == made.steps.size());
    studio.removeUserPattern ("My Arp");
    studio.removeUserPattern ("My Arp 2");
    CHECK (patternNamed ("My Arp") == -1);
    CHECK_FALSE (patternGroups().contains (userPatternGroup));
    CHECK (studio.addPatternFromPart (999999).isEmpty());
}

TEST_CASE ("0.44.0: a track performing the chord track: the pattern's notes on each chord as the song plays; follows chord edits; off again", "[chordtool2]")
{
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addChordEvent (0.0, C);
    studio.addChordEvent (4.0, G);
    auto probe = std::make_unique<Probe>();
    auto* log = probe.get();
    studio.getEngine().setInstrument (trackId, std::move (probe));
    studio.setPerformPattern (trackId, "Held");
    CHECK (studio.getProject().findTrack (trackId)->performPattern == "Held");
    auto& engine = studio.getEngine();
    studio.setPosition (0.0);
    studio.play();
    const auto rate = (int) engine.getSampleRate();
    renderSamples (engine, rate * 4, 512);   // (4 s at 120: two bars)
    studio.stop();
    std::set<int> firstBar, secondBar;
    for (const auto& e : log->notesOn())
        (e.sample < rate * 2 ? firstBar : secondBar).insert (e.message.getNoteNumber() % 12);
    CHECK (firstBar == std::set<int> { 0, 4, 7 });
    CHECK (secondBar == std::set<int> { 7, 11, 2 });
    // An undo step; undone: no performance.
    studio.undo();
    CHECK (studio.getProject().findTrack (trackId)->performPattern.isEmpty());
    studio.setPerformPattern (trackId, "no such pattern");
    CHECK (studio.getProject().findTrack (trackId)->performPattern.isEmpty());
    // Saved with the song.
    studio.setPerformPattern (trackId, "Up");
    const auto back = Project::fromXml (*studio.getProject().toXml());
    REQUIRE (back);
    CHECK (back->findTrack (trackId)->performPattern == "Up");
}

#include "ui/Finish.h"
#include "ui/PadsRemotePage.h"

TEST_CASE ("0.44.0: the pads' REMOTE page: a box clicked learns its key; the range from two keys; latch, sections, modifiers", "[chordtool2][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    PadsRemotePage page (studio);
    CHECK (page.getKeyText (Studio::PadLearn::range, 0) == "PADS: C1 TO B1");
    page.learn (Studio::PadLearn::range);
    CHECK (studio.getPadLearn() == Studio::PadLearn::range);
    studio.learnedKey (72);
    studio.learnedKey (60);
    CHECK (page.getKeyText (Studio::PadLearn::range, 0) == "PADS: C3 TO C4");
    page.learn (Studio::PadLearn::subsection, 1);
    studio.learnedKey (100);
    CHECK (studio.getChordPads().subsectionKeys[1] == 100);
    // A box clicked twice: learning stops.
    page.learn (Studio::PadLearn::modifier, 0);
    page.learn (Studio::PadLearn::modifier, 0);
    CHECK (studio.getPadLearn() == Studio::PadLearn::none);
    CHECK (page.getKeyText (Studio::PadLearn::modifier, 4) == "TRANSPOSE UP: E2");
    CHECK (page.createComponentSnapshot (page.getLocalBounds()).isValid());
}
