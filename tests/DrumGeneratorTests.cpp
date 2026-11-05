#include <catch2/catch_test_macros.hpp>

#include <set>

#include "engine/Studio.h"
#include "engine/DrumGenerator.h"
#include "ui/LinesPage.h"

// The beat generator (0.55.0): beats built by rules per style and section, shaped by DRUM SETTINGS,
// new for each seed, repeatable for the same one.

using namespace pr7s;

namespace
{
    using T = Chord::Type;
    using X = Chord::Tension;

    std::vector<ChordSpan> eightBars()
    {
        const Key am { 5, 0, true };
        std::vector<ChordSpan> spans;
        const Chord chords[] = { { 9, T::minor, X::none, -1 }, { 5, T::major, X::none, -1 }, { 0, T::major, X::none, -1 }, { 7, T::major, X::none, -1 } };
        for (int i = 0; i < 8; ++i)
            spans.push_back ({ i * 4.0, i * 4.0 + 4.0, chords[i % 4], am });
        return spans;
    }

    int count (const LineIdea& idea, int pitch)
    {
        return (int) std::count_if (idea.notes.begin(), idea.notes.end(), [pitch] (const LineNote& n) { return n.pitch == pitch; });
    }

    int countIn (const LineIdea& idea, int pitch, double from, double to)
    {
        return (int) std::count_if (idea.notes.begin(), idea.notes.end(), [&] (const LineNote& n) { return n.pitch == pitch && n.beat >= from - 1.0e-9 && n.beat < to - 1.0e-9; });
    }

    std::vector<std::pair<long, int>> barOf (const LineIdea& idea, int bar)
    {
        std::vector<std::pair<long, int>> out;
        for (const auto& n : idea.notes)
            if (n.beat >= bar * 4.0 - 1.0e-9 && n.beat < bar * 4.0 + 4.0 - 1.0e-9)
                out.emplace_back (std::lround ((n.beat - bar * 4.0) * 96.0), n.pitch);
        std::sort (out.begin(), out.end());
        return out;
    }
}

TEST_CASE ("The beat generator: every style and section, GM drums, repeatable per seed, new for another (0.55.0)", "[drumgen]")
{
    const auto spans = eightBars();
    for (const auto style : guitarStyles())
        for (const auto section : drumSections())
            for (const juce::uint32 seed : { 1u, 2u, 77u })
            {
                INFO (guitarStyleName (style) << " " << drumSectionName (section) << " seed " << seed);
                const auto beat = generateBeat (style, section, spans, 4.0, {}, seed);
                CHECK (beat.part == LineIdea::Part::drums);
                CHECK (beat.description.startsWith ("Generated: "));
                REQUIRE_FALSE (beat.notes.empty());
                CHECK (std::is_sorted (beat.notes.begin(), beat.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; }));
                for (const auto& n : beat.notes)
                {
                    CHECK ((n.pitch >= 35 && n.pitch <= 59));
                    CHECK ((n.beat >= 0.0 && n.beat < 32.0));
                    CHECK ((n.velocity > 0.0f && n.velocity <= 1.0f));
                }
                // The same seed: the same beat (HEAR plays what USE IT adds).
                CHECK (generateBeat (style, section, spans, 4.0, {}, seed).notes == beat.notes);
                if (section != DrumSection::ending)
                {
                    CHECK (count (beat, GmDrum::kick) > 0);
                    CHECK (count (beat, GmDrum::snare) > 0);
                }
            }
    // Other seeds, other beats (most of the time: at least some of ten differ from the first).
    const auto first = generateBeat (GuitarStyle::numetal, DrumSection::verse, spans, 4.0, {}, 1).notes;
    auto different = 0;
    for (juce::uint32 seed = 2; seed < 12; ++seed)
        different += generateBeat (GuitarStyle::numetal, DrumSection::verse, spans, 4.0, {}, seed).notes != first ? 1 : 0;
    CHECK (different >= 7);
}

TEST_CASE ("The beat generator's shape: a phrase repeats its groove, the fill and crash, half time, the ending (0.55.0)", "[drumgen]")
{
    const auto spans = eightBars();
    const auto verse = generateBeat (GuitarStyle::rock, DrumSection::verse, spans, 4.0, {}, 3);
    // Bar 2 plays bar 1's groove again (bar 1 also has the phrase's crash).
    auto bar0 = barOf (verse, 0), bar1 = barOf (verse, 1);
    bar0.erase (std::remove_if (bar0.begin(), bar0.end(), [] (const auto& h) { return h.second == GmDrum::crash || h.second == GmDrum::crash2; }), bar0.end());
    CHECK (bar1 == bar0);
    // The fill on the phrase's last beat (bar 4: beat 15), then a crash at bar 5.
    CHECK (countIn (verse, GmDrum::closedHat, 15.0, 16.0) == 0);
    CHECK (countIn (verse, GmDrum::snare, 15.0, 16.0) + countIn (verse, GmDrum::tomHigh, 15.0, 16.0) + countIn (verse, GmDrum::tomLow, 15.0, 16.0)
           + countIn (verse, GmDrum::tomMid, 15.0, 16.0) + countIn (verse, GmDrum::tomFloor, 15.0, 16.0) >= 2);
    CHECK (countIn (verse, GmDrum::crash, 16.0, 16.01) + countIn (verse, GmDrum::crash2, 16.0, 16.01) == 1);
    // Rock's verse: the backbeat on 2 and 4.
    CHECK (countIn (verse, GmDrum::snare, 1.0, 1.01) == 1);
    CHECK (countIn (verse, GmDrum::snare, 3.0, 3.01) == 1);
    // A breakdown is half time: the loud snare on beat 3 only, the china on the beats.
    const auto breakdown = generateBeat (GuitarStyle::metalcore, DrumSection::breakdown, spans, 4.0, {}, 4);
    for (const auto& n : breakdown.notes)
        if (n.pitch == GmDrum::snare && n.beat < 12.0 && n.velocity > 0.8f)
            CHECK (std::abs (std::fmod (n.beat, 4.0) - 2.0) < 1.0e-6);
    CHECK (count (breakdown, GmDrum::china) >= 16);
    // The ending's last bar: one big hit.
    const auto ending = generateBeat (GuitarStyle::thrash, DrumSection::ending, spans, 4.0, {}, 5);
    CHECK (barOf (ending, 7).size() == 4);
    for (const auto& h : barOf (ending, 7))
        CHECK (h.first == 0);
}

TEST_CASE ("DRUM SETTINGS: density, double kick, ghost notes, fills, the cymbal, the kick locked to the riff (0.55.0)", "[drumgen]")
{
    const auto spans = eightBars();
    const auto kicks = [&] (const DrumSettings& s)
    {
        auto total = 0;
        for (juce::uint32 seed = 1; seed <= 20; ++seed)
            total += count (generateBeat (GuitarStyle::numetal, DrumSection::verse, spans, 4.0, s, seed), GmDrum::kick);
        return total;
    };
    DrumSettings sparse, busy;
    sparse.density = 0.0f;
    busy.density = 1.0f;
    CHECK (kicks (sparse) < kicks ({}));
    CHECK (kicks (busy) > kicks ({}));

    DrumSettings noDouble, allDouble;
    noDouble.doubleKick = 0.0f;
    allDouble.doubleKick = 1.0f;
    const auto shredKicks = [&] (const DrumSettings& s) { return count (generateBeat (GuitarStyle::shred, DrumSection::verse, spans, 4.0, s, 9), GmDrum::kick); };
    CHECK (shredKicks (noDouble) < shredKicks (allDouble));
    CHECK (shredKicks (allDouble) >= 16 * 7);   // (sixteenths, all but the fills)

    const auto softSnares = [&] (const DrumSettings& s)
    {
        auto total = 0;
        for (juce::uint32 seed = 1; seed <= 20; ++seed)
            for (const auto& n : generateBeat (GuitarStyle::rock, DrumSection::verse, spans, 4.0, s, seed).notes)
                total += n.pitch == GmDrum::snare && n.velocity < 0.4f ? 1 : 0;
        return total;
    };
    DrumSettings noGhosts, ghosts;
    noGhosts.ghostNotes = 0.0f;
    ghosts.ghostNotes = 1.0f;
    CHECK (softSnares (noGhosts) == 0);
    CHECK (softSnares (ghosts) > softSnares ({}));

    // Fills: none, or every 2 bars over two beats.
    DrumSettings noFills, twoBars;
    noFills.fillEvery = 0;
    twoBars.fillEvery = 2;
    twoBars.fillBeats = 2;
    const auto plain = generateBeat (GuitarStyle::rock, DrumSection::verse, spans, 4.0, noFills, 6);
    for (int bar = 0; bar < 8; ++bar)
        CHECK (countIn (plain, GmDrum::tomHigh, bar * 4.0, bar * 4.0 + 4.0) + countIn (plain, GmDrum::tomLow, bar * 4.0, bar * 4.0 + 4.0) == 0);
    const auto filled = generateBeat (GuitarStyle::rock, DrumSection::verse, spans, 4.0, twoBars, 6);
    for (const auto bar : { 1, 3, 5 })
        CHECK (countIn (filled, GmDrum::closedHat, bar * 4.0 + 2.0, bar * 4.0 + 4.0) == 0);

    // The cymbal for every section.
    DrumSettings china;
    china.cymbal = DrumSettings::Cymbal::china;
    const auto chorus = generateBeat (GuitarStyle::rock, DrumSection::chorus, spans, 4.0, china, 2);
    CHECK (count (chorus, GmDrum::china) > 0);
    CHECK (count (chorus, GmDrum::ride) == 0);
    CHECK (count (chorus, GmDrum::closedHat) == 0);

    // Locked to the riff: a kick on each of the riff's hits (outside fills).
    DrumSettings locked;
    locked.lockToRiff = true;
    locked.fillEvery = 0;
    const std::vector<double> hits { 0.0, 0.75, 1.5, 2.5, 4.0, 5.25, 6.0, 7.5 };
    const auto lockedBeat = generateBeat (GuitarStyle::metalcore, DrumSection::verse, spans, 4.0, locked, 3, hits);
    for (const auto h : hits)
        CHECK (countIn (lockedBeat, GmDrum::kick, h, h + 0.01) == 1);

    // Text and limits.
    DrumSettings d;
    CHECK (DrumSettings::fromString (d.toString()) == d);
    d.fillEvery = 5;
    d.density = 3.0f;
    CHECK (d.limited().fillEvery == 8);
    CHECK (d.limited().density == 1.0f);
    CHECK (DrumSettings::fromString ("x") == DrumSettings {});
}

TEST_CASE ("IDEAS' DRUMS tab: one generated beat per section, MORE IDEAS deals new ones, DRUM SETTINGS, the written ones behind a key (0.55.0)", "[drumgen][ui]")
{
    Studio studio (false);
    const auto spans = eightBars();
    LinesPage page (studio, spans);
    page.setTab (LinesPage::Tab::drums);
    CHECK_FALSE (page.isShowingWrittenBeats());
    const auto& beats = page.getIdeas (LineIdea::Part::drums);
    REQUIRE (beats.size() == 6);
    CHECK (beats[0].style == "Intro");
    CHECK (beats[4].style == "Breakdown");
    const auto before = beats[1].notes;
    page.moreIdeas();
    CHECK (page.getIdeas (LineIdea::Part::drums)[1].notes != before);
    DrumSettings d;
    d.fillEvery = 8;
    page.setDrumSettings (d);
    CHECK (studio.getDrumSettings() == d);
    CHECK (DrumPanel::summary (d) == "DRUMS: FILL EVERY 8");
    page.setWrittenBeats (true);
    CHECK (page.getIdeas (LineIdea::Part::drums).back().style == "Locked to the riff");
    page.setWrittenBeats (false);
    CHECK (page.getIdeas (LineIdea::Part::drums)[0].style == "Intro");

    DrumPanel panel (d);
    DrumSettings seen;
    panel.onChange = [&] (const DrumSettings& s) { seen = s; };
    panel.setSettings ({});
    CHECK (seen == DrumSettings {});
}
