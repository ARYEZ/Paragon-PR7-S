#include <catch2/catch_test_macros.hpp>

#include "engine/DrumLearning.h"
#include "engine/MidiImport.h"
#include "DrumSongs.h"

// Learning a drummer (0.56.0): whole songs of drum MIDI read into bars, their sections found and
// named from the drums alone, the review page's edits, profiles learned, saved and played from.

using namespace pr7s;

namespace
{
    using namespace GmDrum;
    using namespace drumsongs;

    std::vector<std::pair<int, int>> barHits (const LineIdea& idea, int bar)   // (96ths of the bar, note)
    {
        std::vector<std::pair<int, int>> out;
        for (const auto& n : idea.notes)
            if (n.beat >= bar * 4.0 - 1.0e-9 && n.beat < bar * 4.0 + 4.0 - 1.0e-9)
                out.emplace_back ((int) std::lround ((n.beat - bar * 4.0) * 24.0), n.pitch);
        std::sort (out.begin(), out.end());
        return out;
    }

    std::vector<std::pair<int, int>> patternHits (const std::vector<LearnedHit>& hits)
    {
        std::vector<std::pair<int, int>> out;
        for (const auto& h : hits)
            out.emplace_back (h.step * 2, h.note);
        std::sort (out.begin(), out.end());
        return out;
    }

    std::vector<ChordSpan> eightBars()
    {
        const Key am { 5, 0, true };
        std::vector<ChordSpan> spans;
        for (int i = 0; i < 8; ++i)
            spans.push_back ({ i * 4.0, i * 4.0 + 4.0, Chord { 9, Chord::Type::minor, Chord::Tension::none, -1 }, am });
        return spans;
    }

    juce::String layout (const DrumSong& s)
    {
        juce::StringArray parts;
        for (const auto& x : s.sections)
            parts.add (juce::String (x.firstBar) + " " + drumSectionName (x.kind) + " " + juce::String (x.bars));
        return parts.joinIntoString (", ");
    }
}

TEST_CASE ("Learning: a song's bars read by what the drums play; any kit's notes as General MIDI's (0.56.0)", "[drumlearn]")
{
    CHECK (gmNoteOf (22) == closedHat);   // (Toontrack's hats)
    CHECK (gmNoteOf (26) == openHat);
    CHECK (gmNoteOf (35) == kick);
    CHECK (gmNoteOf (40) == snare);
    CHECK (gmNoteOf (59) == ride);
    CHECK (gmNoteOf (50) == tomHigh);
    CHECK (gmNoteOf (90) == -1);
    CHECK (drumRoleOf (38, 0.2f) == DrumRole::ghost);
    CHECK (drumRoleOf (38, 0.9f) == DrumRole::snare);
    CHECK (drumRoleOf (52, 0.9f) == DrumRole::china);

    const auto s = drumSongFromNotes ("Test", song(), 120.0, SignatureMap());
    REQUIRE (s.bars.size() == 56);
    const auto& verse = s.bars[4];
    CHECK (verse.kick[0]);
    CHECK (verse.kick[6]);
    CHECK (verse.snare[4]);
    CHECK (verse.snare[12]);
    CHECK (verse.ghosts == 2);
    CHECK (verse.cymbal == closedHat);
    CHECK (verse.crashAtStart);
    CHECK_FALSE (verse.fill);
    CHECK (s.bars[7].fill);             // (the verse's fourth bar)
    CHECK_FALSE (s.bars[7].twoBeatFill);
    CHECK (s.bars[12].cymbal == ride);
    CHECK (s.bars[36].halfTime);
    CHECK (s.bars[36].cymbal == china);
}

TEST_CASE ("Learning: sections found and named from the drums alone (no names in the file) (0.56.0)", "[drumlearn]")
{
    const auto s = drumSongFromNotes ("Test", song(), 120.0, SignatureMap());
    INFO (layout (s));
    REQUIRE (s.sections.size() == 8);
    const std::vector<std::tuple<int, DrumSection, int>> expected { { 0, DrumSection::intro, 4 }, { 4, DrumSection::verse, 8 }, { 12, DrumSection::chorus, 8 },
                                                                    { 20, DrumSection::verse, 8 }, { 28, DrumSection::chorus, 8 }, { 36, DrumSection::breakdown, 8 },
                                                                    { 44, DrumSection::chorus, 8 }, { 52, DrumSection::ending, 4 } };
    for (size_t i = 0; i < expected.size(); ++i)
    {
        CHECK (s.sections[i].firstBar == std::get<0> (expected[i]));
        CHECK (s.sections[i].kind == std::get<1> (expected[i]));
        CHECK (s.sections[i].bars == std::get<2> (expected[i]));
        CHECK (s.sections[i].used);
    }
    // The verses share a group, the choruses theirs.
    CHECK (s.sections[1].group == s.sections[3].group);
    CHECK (s.sections[2].group == s.sections[4].group);
    CHECK (s.sections[2].group == s.sections[6].group);
    CHECK (s.sections[1].group != s.sections[2].group);

    // A steady groove with a fill and a crash every four bars is one section.
    Writer w;
    for (auto b = 0; b < 16; ++b)
        verseBar (w, b % 4 == 3, b % 4 == 0);
    const auto steady = drumSongFromNotes ("Steady", w.notes, 120.0, SignatureMap());
    CHECK (steady.sections.size() == 1);

    // Silence: a section of its own, left out of learning.
    Writer q;
    for (auto b = 0; b < 4; ++b)
        q.next();
    for (auto b = 0; b < 8; ++b)
        verseBar (q, false, b == 0);
    const auto quiet = drumSongFromNotes ("Quiet", q.notes, 120.0, SignatureMap());
    REQUIRE (quiet.sections.size() == 2);
    CHECK_FALSE (quiet.sections[0].used);
    CHECK (quiet.sections[1].firstBar == 4);
}

TEST_CASE ("Learning: the review page's edits (name, split, join, move the start, leave out), and as text (0.56.0)", "[drumlearn]")
{
    auto s = drumSongFromNotes ("Test", song(), 120.0, SignatureMap());
    const auto before = s.sections;
    setSectionKind (s, 5, DrumSection::bridge);
    CHECK (s.sections[5].kind == DrumSection::bridge);
    CHECK (splitSection (s, 1, 8));
    REQUIRE (s.sections.size() == 9);
    CHECK (s.sections[1].bars == 4);
    CHECK (s.sections[2].firstBar == 8);
    CHECK (s.sections[2].kind == DrumSection::verse);
    CHECK_FALSE (splitSection (s, 1, 4));   // (its own first bar)
    CHECK (joinSections (s, 1));
    CHECK (s.sections[1].bars == 8);
    CHECK (moveSectionStart (s, 2, 14));
    CHECK (s.sections[1].bars == 10);
    CHECK (s.sections[2].firstBar == 14);
    CHECK (s.sections[2].bars == 6);
    CHECK_FALSE (moveSectionStart (s, 2, 4));    // (the section before keeps a bar)
    CHECK_FALSE (moveSectionStart (s, 0, 1));
    setSectionUsed (s, 0, false);
    const auto text = sectionsToText (s.sections);
    const auto back = sectionsFromText (text, (int) s.bars.size());
    REQUIRE (back.has_value());
    REQUIRE (back->size() == s.sections.size());
    for (size_t i = 0; i < back->size(); ++i)
    {
        CHECK ((*back)[i].firstBar == s.sections[i].firstBar);
        CHECK ((*back)[i].bars == s.sections[i].bars);
        CHECK ((*back)[i].kind == s.sections[i].kind);
        CHECK ((*back)[i].used == s.sections[i].used);
    }
    CHECK_FALSE (sectionsFromText (text, 40).has_value());   // (not the song's bars)
    CHECK_FALSE (sectionsFromText ("0:verse:4:1;5:chorus:4:1", 9).has_value());   // (a gap)
    // A section heard: its own notes from its first bar.
    const auto idea = sectionIdea (s, 3);
    CHECK_FALSE (idea.notes.empty());
    CHECK (idea.notes.front().beat >= 0.0);
    juce::ignoreUnused (before);
}

TEST_CASE ("Learning: a profile per section (their grooves, fills, chances, cymbal, crashes, half time), saved and opened (0.56.0)", "[drumlearn]")
{
    const auto s = drumSongFromNotes ("Test", song(), 120.0, SignatureMap());
    const auto profile = learnProfile ("Session drummer", { s });
    CHECK (profile.name == "Session drummer");
    REQUIRE (profile.songs.size() == 1);
    for (const auto kind : { DrumSection::intro, DrumSection::verse, DrumSection::chorus, DrumSection::breakdown, DrumSection::ending })
        CHECK (profile.has (kind));
    CHECK_FALSE (profile.has (DrumSection::bridge));

    const auto& verse = profile.of (DrumSection::verse);
    CHECK (verse.bars == 16);
    CHECK (verse.sections == 2);
    REQUIRE_FALSE (verse.grooves.empty());
    CHECK (verse.grooves[0].count == 12);   // (16 bars, 4 of them fills; the phrase crash left out of the groove)
    // The groove is the verse bar as played (no crash in it).
    Writer w;
    verseBar (w, false, false);
    const auto played = drumSongFromNotes ("One", w.notes, 120.0, SignatureMap()).bars[0].hits;
    CHECK (verse.grooves[0].hits == played);
    REQUIRE_FALSE (verse.fills.empty());
    CHECK (verse.fills[0].beats == 1);
    CHECK (verse.fillChance == 0.25f);
    CHECK (verse.crashChance == 1.0f);
    CHECK (verse.cymbal == closedHat);
    CHECK (verse.kick[6] == 1.0f);
    CHECK (verse.snare[4] == 1.0f);
    CHECK (verse.ghost[7] == 1.0f);
    CHECK (profile.of (DrumSection::chorus).cymbal == ride);
    CHECK (profile.of (DrumSection::breakdown).halfTime == 1.0f);
    CHECK (profile.of (DrumSection::breakdown).cymbal == china);

    // Left out: not learned.
    auto edited = s;
    setSectionUsed (edited, 5, false);
    CHECK_FALSE (learnProfile ("x", { edited }).has (DrumSection::breakdown));
    auto unused = s;
    unused.use = false;
    CHECK (learnProfile ("x", { unused }).barsLearned() == 0);

    // Saved and opened again.
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-drummer-test").getChildFile ("Session drummer.pr7drummer");
    file.getParentDirectory().deleteRecursively();
    REQUIRE (profile.save (file));
    const auto loaded = DrumProfile::load (file);
    REQUIRE (loaded.has_value());
    CHECK (loaded->name == profile.name);
    CHECK (loaded->songs.size() == 1);
    CHECK (loaded->songs[0].sections == sectionsToText (s.sections));
    for (const auto kind : drumSections())
    {
        const auto& a = profile.of (kind);
        const auto& b = loaded->of (kind);
        CHECK (a.bars == b.bars);
        CHECK (a.grooves == b.grooves);
        CHECK (a.fills == b.fills);
        CHECK (a.cymbal == b.cymbal);
        CHECK (std::abs (a.crashChance - b.crashChance) < 1.0e-3f);
        for (size_t i = 0; i < 16; ++i)
            CHECK (std::abs (a.kick[i] - b.kick[i]) < 1.0e-3f);
    }
    CHECK (drummerNames (file.getParentDirectory()) == std::vector<juce::String> { "Session drummer" });
    CHECK (drummerFile (file.getParentDirectory(), "Session drummer") == file);
    file.getParentDirectory().deleteRecursively();
}

TEST_CASE ("Learning: beats played from a profile (their groove kept, their fills, crashes, feel; DRUM SETTINGS) (0.56.0)", "[drumlearn]")
{
    const auto profile = learnProfile ("Session drummer", { drumSongFromNotes ("Test", song(), 120.0, SignatureMap()) });
    const auto spans = eightBars();
    const auto& verse = profile.of (DrumSection::verse);
    for (const juce::uint32 seed : { 1u, 5u, 9u })
    {
        const auto beat = generateBeat (profile, DrumSection::verse, spans, 4.0, {}, seed);
        CHECK (beat.part == LineIdea::Part::drums);
        CHECK (beat.description.startsWith ("Session drummer: 16 verse bars learned"));
        REQUIRE_FALSE (beat.notes.empty());
        for (const auto& n : beat.notes)
            CHECK ((n.pitch >= 35 && n.pitch <= 59 && n.beat >= 0.0 && n.beat < 32.0));
        // Bar 2 is the drummer's groove as played (one groove learned for the verse).
        CHECK (barHits (beat, 1) == patternHits (verse.grooves[0].hits));
        // The fill (bar 4's last beat) is theirs: toms, no hats.
        for (const auto& n : beat.notes)
            if (n.beat >= 15.0 - 1.0e-9 && n.beat < 16.0 - 1.0e-9)
                CHECK (n.pitch != closedHat);
        auto toms = 0;
        for (const auto& n : beat.notes)
            toms += n.beat >= 15.0 - 1.0e-9 && n.beat < 16.0 - 1.0e-9 && (n.pitch == tomHigh || n.pitch == tomFloor || n.pitch == tomMid || n.pitch == tomLow) ? 1 : 0;
        CHECK (toms == 4);
        // A crash where the next phrase starts (after the fill).
        CHECK (std::any_of (beat.notes.begin(), beat.notes.end(), [] (const LineNote& n) { return std::abs (n.beat - 16.0) < 1.0e-9 && (n.pitch == crash || n.pitch == crash2); }));
        CHECK (generateBeat (profile, DrumSection::verse, spans, 4.0, {}, seed).notes == beat.notes);
    }
    // The chorus on the ride, the breakdown in half time.
    const auto chorus = generateBeat (profile, DrumSection::chorus, spans, 4.0, {}, 2);
    CHECK (std::count_if (chorus.notes.begin(), chorus.notes.end(), [] (const LineNote& n) { return n.pitch == ride; }) > 20);
    const auto breakdown = generateBeat (profile, DrumSection::breakdown, spans, 4.0, {}, 2);
    for (const auto& n : breakdown.notes)
        if (n.pitch == snare && n.beat < 12.0)
            CHECK (std::abs (std::fmod (n.beat, 4.0) - 2.0) < 1.0e-6);
    // No bridge learned: from their verse, said so.
    const auto bridge = generateBeat (profile, DrumSection::bridge, spans, 4.0, {}, 2);
    CHECK (bridge.description.contains ("no bridge learned"));
    CHECK_FALSE (bridge.notes.empty());
    // Nothing learned: nothing.
    CHECK (generateBeat (DrumProfile {}, DrumSection::verse, spans, 4.0, {}, 1).notes.empty());
    CHECK (generatedBeats (profile, spans, 4.0, {}, 0).size() == 6);

    // DRUM SETTINGS: the cymbal, no ghosts, the kick on the riff.
    DrumSettings rideAll;
    rideAll.cymbal = DrumSettings::Cymbal::ride;
    const auto onRide = generateBeat (profile, DrumSection::verse, spans, 4.0, rideAll, 1);
    CHECK (std::none_of (onRide.notes.begin(), onRide.notes.end(), [] (const LineNote& n) { return n.pitch == closedHat; }));
    DrumSettings noGhosts;
    noGhosts.ghostNotes = 0.0f;
    const auto clean = generateBeat (profile, DrumSection::verse, spans, 4.0, noGhosts, 1);
    CHECK (std::none_of (clean.notes.begin(), clean.notes.end(), [] (const LineNote& n) { return n.pitch == snare && n.velocity < 0.4f; }));
    DrumSettings locked;
    locked.lockToRiff = true;
    locked.fillEvery = 0;
    const std::vector<double> hits { 0.0, 1.25, 2.5, 4.0, 5.75 };
    const auto onRiff = generateBeat (profile, DrumSection::verse, spans, 4.0, locked, 1, hits);
    for (const auto h : hits)
        CHECK (std::count_if (onRiff.notes.begin(), onRiff.notes.end(), [h] (const LineNote& n) { return n.pitch == kick && std::abs (n.beat - h) < 1.0e-6; }) == 1);

    // The feel: a drummer who plays the snare late is played late.
    auto notes = song();
    for (auto& n : notes)
        if (n.pitch == snare && n.velocity > 0.5f)
            n.start += 0.02;
    const auto late = learnProfile ("Late", { drumSongFromNotes ("Late", notes, 120.0, SignatureMap()) });
    CHECK (std::abs (late.of (DrumSection::verse).push[1] - 0.02) < 1.0e-6);
    CHECK (std::abs (late.of (DrumSection::verse).push[0]) < 1.0e-6);
    const auto lateBeat = generateBeat (late, DrumSection::verse, spans, 4.0, {}, 1);
    CHECK (std::any_of (lateBeat.notes.begin(), lateBeat.notes.end(), [] (const LineNote& n) { return n.pitch == snare && std::abs (n.beat - 5.02) < 1.0e-6; }));
}

TEST_CASE ("Learning: a whole song read from a MIDI file (the drum channel only), a folder's songs found (0.56.0)", "[drumlearn]")
{
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-drum-songs");
    folder.deleteRecursively();
    folder.getChildFile ("Album").createDirectory();
    const auto file = folder.getChildFile ("Album").getChildFile ("Song one.mid");
    {
        juce::MidiMessageSequence drums, keys;
        for (const auto& n : song())
        {
            const auto pitch = n.pitch == closedHat ? 22 : n.pitch;   // (Toontrack's closed hat)
            drums.addEvent (juce::MidiMessage::noteOn (10, pitch, n.velocity), n.start * 480.0);
            drums.addEvent (juce::MidiMessage::noteOff (10, pitch), (n.start + n.length) * 480.0);
        }
        for (auto b = 0; b < 56; ++b)   // (a keyboard part on channel 1: not drums, though 45 is a tom's note)
        {
            keys.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), b * 4 * 480.0);
            keys.addEvent (juce::MidiMessage::noteOff (1, 45), b * 4 * 480.0 + 400.0);
        }
        drums.updateMatchedPairs();
        keys.updateMatchedPairs();
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (480);
        juce::MidiMessageSequence conductor;
        conductor.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);
        midi.addTrack (conductor);
        midi.addTrack (drums);
        midi.addTrack (keys);
        juce::FileOutputStream out (file);
        REQUIRE (midi.writeTo (out));
    }
    folder.getChildFile ("notes.txt").replaceWithText ("x");
    const auto files = findDrumSongs (folder);
    REQUIRE (files.size() == 1);
    CHECK (files[0] == file);

    juce::String error;
    const auto read = readDrumSong (file, error);
    INFO (error);
    REQUIRE (read.has_value());
    CHECK (read->name == "Song one");
    CHECK (read->bars.size() == 56);
    CHECK (read->bars[4].cymbal == closedHat);   // (22 read as the hats)
    CHECK (read->sections.size() == 8);
    CHECK (std::none_of (read->bars[0].hits.begin(), read->bars[0].hits.end(), [] (const LearnedHit& h) { return h.note == tomMid; }));
    CHECK_FALSE (readDrumSong (folder.getChildFile ("notes.txt"), error).has_value());
    folder.deleteRecursively();
}

TEST_CASE ("Learning: other layouts (a bridge on the ride bell, a song without an intro) (0.56.0)", "[drumlearn]")
{
    CHECK (layout (drumSongFromNotes ("A", songOf ("I4 V8 C8 V8 C8 B8 C8 E4"), 120.0, SignatureMap()))
           == layout (drumSongFromNotes ("B", song(), 120.0, SignatureMap())));
    const auto bridge = drumSongFromNotes ("Bridge", songOf ("I4 V8 C8 V8 C8 R8 C8 E4"), 120.0, SignatureMap());
    INFO (layout (bridge));
    REQUIRE (bridge.sections.size() == 8);
    CHECK (bridge.sections[5].kind == DrumSection::bridge);
    const auto noIntro = drumSongFromNotes ("Straight in", songOf ("V8 C8 V8 C8 E4"), 120.0, SignatureMap());
    INFO (layout (noIntro));
    REQUIRE (noIntro.sections.size() == 5);
    CHECK (noIntro.sections[0].kind == DrumSection::verse);
    CHECK (noIntro.sections[1].kind == DrumSection::chorus);
    CHECK (noIntro.sections[4].kind == DrumSection::ending);
}
