#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/LinesPage.h"

#include <mutex>
#include <set>

// 0.39.0, performances: rhythm and arpeggio patterns that play any chords.

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    using T = Chord::Type;
    using X = Chord::Tension;
    const Chord C { 0, T::major, X::none, -1 }, F { 5, T::major, X::none, -1 }, G { 7, T::major, X::none, -1 };
    const Key cMajor {};
    const Pattern& named (const char* name) { return patterns()[(size_t) patternNamed (name)]; }
}

TEST_CASE ("0.39.0: the patterns: four groups of six, every step inside its bars", "[patterns]")
{
    REQUIRE (patterns().size() == 24);
    REQUIRE (patternGroups().size() == 4);
    std::set<juce::String> names;
    for (const auto& group : patternGroups())
        CHECK (patternsInGroup (group).size() == 6);
    for (const auto& p : patterns())
    {
        names.insert (p.name);
        CHECK (p.description.isNotEmpty());
        REQUIRE_FALSE (p.steps.empty());
        for (const auto& s : p.steps)
        {
            CHECK (s.beat >= 0.0);
            CHECK (s.beat < p.bars * 4.0);
            CHECK (s.length > 0.0);
            CHECK (s.velocity > 0.0f);
            CHECK (s.velocity <= 1.0f);
        }
    }
    CHECK (names.size() == patterns().size());   // (names unique)
    CHECK (patternNamed ("up and down") >= 0);
    CHECK (patternNamed ("nothing") == -1);
}

TEST_CASE ("0.39.0: a pattern's notes on a chord: voices from the bottom (on up the octaves), the bass, strums, cut at the end, scaled to the bar", "[patterns]")
{
    // Up: eighths through C E G and on into the next octaves.
    const auto up = patternNotes (named ("Up"), { 67, 60, 64 }, 48, 0.0, 4.0, 0.0, 4.0);
    REQUIRE (up.size() == 8);
    std::vector<int> pitches;
    for (const auto& n : up)
        pitches.push_back (n.pitch);
    CHECK (pitches == std::vector<int> { 60, 64, 67, 72, 76, 79, 84, 88 });
    for (int i = 0; i < 8; ++i)
        CHECK_THAT (up[(size_t) i].beat, WithinAbs (i * 0.5, 1.0e-9));
    CHECK (up[0].velocity > up[1].velocity);   // (the bar's first accented)

    // Held: the chord and the bass for the bar; cut where the chord ends (beat 2).
    const auto held = patternNotes (named ("Held"), { 60, 64, 67 }, 48, 0.0, 2.0, 0.0, 4.0);
    REQUIRE (held.size() == 4);
    for (const auto& n : held)
        CHECK_THAT (n.length, WithinAbs (2.0, 1.0e-9));
    // No bass given: an octave under the lowest voice.
    const auto noBass = patternNotes (named ("Held"), { 60, 64, 67 }, -1, 0.0, 4.0, 0.0, 4.0);
    CHECK (std::any_of (noBass.begin(), noBass.end(), [] (const LineNote& n) { return n.pitch == 48; }));

    // Strummed: the down-stroke spread up from the lowest, the up-stroke down from the top.
    const auto strum = patternNotes (named ("Strummed"), { 60, 64, 67 }, 48, 0.0, 4.0, 0.0, 4.0);
    REQUIRE (strum.size() >= 6);
    CHECK (strum[0].pitch == 60);
    CHECK (strum[1].pitch == 64);
    CHECK_THAT (strum[1].beat - strum[0].beat, WithinAbs (0.02, 1.0e-9));
    const auto upStroke = std::find_if (strum.begin(), strum.end(), [] (const LineNote& n) { return std::abs (n.beat - 1.5) < 1.0e-9; });
    REQUIRE (upStroke != strum.end());
    CHECK (upStroke->pitch == 67);   // (from the top)

    // 3/4: the bar's steps scaled (a quarter pulse's beats at 0, 0.75, 1.5, 2.25 of a 3-beat bar).
    const auto waltz = patternNotes (named ("Quarter pulse"), { 60, 64, 67 }, 48, 0.0, 3.0, 0.0, 3.0);
    std::set<double> starts;
    for (const auto& n : waltz)
        starts.insert (std::round (n.beat * 100.0) / 100.0);
    CHECK (starts == std::set<double> { 0.0, 0.75, 1.5, 2.25 });

    // From a bar's middle: only what starts there, in the bar's own places (counted from the origin).
    const auto late = patternNotes (named ("Up"), { 60, 64, 67 }, 48, 2.0, 4.0, 0.0, 4.0);
    REQUIRE (late.size() == 4);
    CHECK (late[0].pitch == 76);   // (the fifth step: E an octave up, at beat 2)

    // A two-bar pattern repeats every two bars.
    const auto montuno = patternNotes (named ("Montuno"), { 60, 64, 67 }, 48, 0.0, 16.0, 0.0, 4.0);
    const auto firstHalf = std::count_if (montuno.begin(), montuno.end(), [] (const LineNote& n) { return n.beat < 8.0; });
    CHECK ((int) montuno.size() == 2 * (int) firstHalf);
}

TEST_CASE ("0.39.0: pattern ideas under the chord track's chords: a group's six, voiced with the least movement, roots in the bass", "[patterns]")
{
    const std::vector<ChordSpan> spans { { 8.0, 12.0, C, cMajor }, { 12.0, 16.0, F, cMajor }, { 16.0, 20.0, G, cMajor } };
    const auto ideas = patternIdeas (spans, 4.0, "STRUMS & PULSES");
    REQUIRE (ideas.size() == 6);
    for (const auto& idea : ideas)
    {
        CHECK (idea.part == LineIdea::Part::pattern);
        REQUIRE_FALSE (idea.notes.empty());
        CHECK (idea.notes.front().beat >= 0.0);   // (from the first chord's start)
        CHECK (idea.notes.back().beat < 12.0);
    }
    // Held: each chord's lowest note is its root in C1-B1 (36-47): C 36, F 41, G 43.
    const auto& held = ideas[0];
    REQUIRE (held.style == "Held");
    std::array<int, 3> lowest { 127, 127, 127 };
    for (const auto& n : held.notes)
        lowest[(size_t) juce::jlimit (0, 2, (int) (n.beat / 4.0))] = juce::jmin (lowest[(size_t) juce::jlimit (0, 2, (int) (n.beat / 4.0))], n.pitch);
    CHECK (lowest == std::array<int, 3> { 36, 41, 43 });
    for (const auto& n : held.notes)
    {
        const auto& chord = n.beat < 4.0 ? C : n.beat < 8.0 ? F : G;
        const auto pcs = chord.pitchClasses();
        CHECK (std::find (pcs.begin(), pcs.end(), n.pitch % 12) != pcs.end());
    }
}

TEST_CASE ("0.39.0: the performer: a held chord through a pattern, in time with the song; a new chord carries on; stop ends every note", "[patterns][performer]")
{
    std::vector<juce::MidiMessage> sent;
    std::mutex sentLock;
    PatternPerformer performer ([&] (const juce::MidiMessage& m) { const std::scoped_lock l (sentLock); sent.push_back (m); });
    std::atomic<double> beat { 0.0 };
    performer.songBeat = [&] { return beat.load(); };
    performer.beatsPerBar = 4.0;
    performer.play ({ 60, 64, 67 }, 100, 1);   // (no pattern: nothing)
    CHECK_FALSE (performer.isPlaying());
    performer.setPattern (patternNamed ("Quarter pulse"));
    const auto ons = [&] { const std::scoped_lock l (sentLock); return (int) std::count_if (sent.begin(), sent.end(), [] (const auto& m) { return m.isNoteOn(); }); };
    const auto offs = [&] { const std::scoped_lock l (sentLock); return (int) std::count_if (sent.begin(), sent.end(), [] (const auto& m) { return m.isNoteOff(); }); };

    performer.play ({ 60, 64, 67 }, 100, 1);
    performer.process();
    CHECK (ons() == 4);            // the chord and its bass on beat one
    beat = 0.95;
    performer.process();
    CHECK (offs() == 3);           // the chord's 0.9 is over; the bass (1.9) goes on
    beat = 1.0;
    performer.process();
    CHECK (ons() == 7);            // the chord again on beat two
    // A new chord (F) at 1.5: the old notes end, the pattern carries on in time (beat three next).
    beat = 1.5;
    performer.play ({ 65, 69, 72 }, 100, 1);
    CHECK (performer.getSounding().empty());
    beat = 2.0;
    performer.process();
    {
        const std::scoped_lock l (sentLock);
        CHECK (sent.back().getNoteNumber() == 53);   // (F's bass: an octave under its lowest voice, sent last on beat three)
    }
    performer.stop();
    CHECK (ons() == offs());       // every note ended
    CHECK_FALSE (performer.isPlaying());
}

TEST_CASE ("0.39.0: the pads perform: a held pad plays its chord through the pattern; released, it stops; off, plain chords again", "[patterns][pads]")
{
    Studio studio (false);
    auto& pads = studio.getPadPlayer();
    std::vector<juce::MidiMessage> sent;
    pads.send = [&] (const juce::MidiMessage& m) { sent.push_back (m); };
    // Plain: the pad's chord at once.
    pads.press (0, 100);
    const auto plain = sent.size();
    CHECK (plain >= 3);
    pads.release (0);
    sent.clear();
    // Performing: nothing straight from the pad player; the performer plays it (stopped song: its own clock).
    studio.setPadPattern (patternNamed ("Held"));
    CHECK (studio.getPadPattern() == patternNamed ("Held"));
    pads.press (0, 100);
    CHECK (studio.getPerformer().isPlaying());
    studio.getPerformer().process();
    CHECK (std::count_if (sent.begin(), sent.end(), [] (const auto& m) { return m.isNoteOn(); }) == (int) plain + 1);   // (the chord and its bass)
    pads.release (0);
    CHECK_FALSE (studio.getPerformer().isPlaying());
    CHECK (std::count_if (sent.begin(), sent.end(), [] (const auto& m) { return m.isNoteOn(); })
           == std::count_if (sent.begin(), sent.end(), [] (const auto& m) { return m.isNoteOff(); }));
    studio.setPadPattern (-1);
    CHECK (studio.getPadPattern() == -1);
}

TEST_CASE ("0.39.0: IDEAS' PATTERNS tab: a group's six cards, heard, used onto a track", "[patterns][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    const std::vector<ChordSpan> spans { { 0.0, 4.0, C, cMajor }, { 4.0, 8.0, F, cMajor } };
    LinesPage page (studio, spans);
    page.setSize (1240, 760);
    page.setTab (LinesPage::Tab::patterns);
    CHECK (page.getNumCardsShown() == 6);
    CHECK (page.getIdeas (LineIdea::Part::pattern).size() == 6);
    page.setPatternGroup ("RHYTHMIC RIFFS");
    CHECK (page.getPatternGroup() == "RHYTHMIC RIFFS");
    CHECK (page.getIdeas (LineIdea::Part::pattern)[0].style == "Tresillo");
    page.play (LineIdea::Part::pattern, 0);
    CHECK (studio.isPreviewing());
    page.stop();
    const auto trackId = page.use (LineIdea::Part::pattern, 1);
    const auto* track = studio.getProject().findTrack (trackId);
    REQUIRE (track != nullptr);
    CHECK (track->name.startsWith ("Pattern"));
    REQUIRE (track->parts.size() == 1);
    CHECK_FALSE (track->parts[0].notes.empty());
    CHECK (page.createComponentSnapshot (page.getLocalBounds()).isValid());
}
