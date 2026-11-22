// (0.202.0, the owner: "lock bars 1-3 and 5-7, hit roll and get better fills for the end of each four bars") BAR LOCKS
// on ARCHITECT's lanes: bars chosen and locked keep their notes exactly; ROLL, GENERATE and every change rewrite the
// others; the drums' FILLS for the bars not locked.

#include <catch2/catch_test_macros.hpp>
#include <set>

#include "TestHelpers.h"
#include "engine/Composer.h"
#include "engine/Studio.h"
#include "ui/ComposerPage.h"

using namespace pr7s;
using L = ComposerLayer;

namespace
{
    constexpr double bpb = 4.0;
    int barOfNote (const LineNote& n) { return (int) std::floor (n.beat / bpb + 1.0e-9); }

    std::vector<LineNote> notesInBar (const LineIdea& idea, int bar)
    {
        std::vector<LineNote> out;
        for (const auto& n : idea.notes)
            if (barOfNote (n) == bar)
                out.push_back (n);
        return out;
    }

    // The notes as kept: those starting in the bar, cut at the end of the locked run they're in.
    std::vector<LineNote> keptBar (const LineIdea& idea, int bar, double runEnd)
    {
        auto out = notesInBar (idea, bar);
        for (auto& n : out)
            if (! n.keyswitch)
                n.length = juce::jmin (n.length, runEnd - n.beat);
        return out;
    }

    bool sameNotes (std::vector<LineNote> a, std::vector<LineNote> b)
    {
        const auto order = [] (const LineNote& x, const LineNote& y)
        { return x.beat != y.beat ? x.beat < y.beat : x.pitch != y.pitch ? x.pitch < y.pitch : x.velocity < y.velocity; };
        std::sort (a.begin(), a.end(), order);
        std::sort (b.begin(), b.end(), order);
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::abs (a[i].beat - b[i].beat) > 1.0e-6 || std::abs (a[i].length - b[i].length) > 1.0e-6 || a[i].pitch != b[i].pitch
                || std::abs (a[i].velocity - b[i].velocity) > 1.0e-4 || a[i].keyswitch != b[i].keyswitch)
                return false;
        return true;
    }

    juce::uint64 bits (std::initializer_list<int> bars)
    {
        juce::uint64 m = 0;
        for (const auto b : bars)
            m |= (juce::uint64) 1 << b;
        return m;
    }

    bool sameStarts (const std::vector<LineNote>& a, const std::vector<LineNote>& b)
    {
        std::multiset<std::pair<int, int>> x, y;
        for (const auto& n : a) if (! n.keyswitch) x.insert ({ juce::roundToInt (n.beat * 960.0), n.pitch });
        for (const auto& n : b) if (! n.keyswitch) y.insert ({ juce::roundToInt (n.beat * 960.0), n.pitch });
        return x == y;
    }

    bool isCrash (int p) { return p == GmDrum::crash || p == GmDrum::crash2 || p == GmDrum::china || p == 55; }
}

TEST_CASE ("0.202.0: applyBarLocks keeps the kept notes and cuts the new ones at a locked bar", "[barlocks]")
{
    ComposerLane lane;
    LineIdea shown;
    shown.notes = { { 0.0, 1.0, 40, 0.8f }, { 4.0, 6.0, 41, 0.8f }, { 9.0, 1.0, 42, 0.8f } };
    lockLaneBars (lane, shown, bits ({ 1 }), bpb, 4);
    CHECK (lane.lockedBars == bits ({ 1 }));
    REQUIRE (lane.keptNotes.size() == 1);
    CHECK (lane.keptNotes[0].pitch == 41);
    CHECK (lane.keptNotes[0].length == 4.0);   // (cut at the run's end, bar 3's start)

    LineIdea fresh;
    fresh.notes = { { 0.0, 1.0, 50, 0.8f }, { 3.0, 3.0, 51, 0.8f }, { 5.0, 1.0, 52, 0.8f }, { 12.0, 1.0, 53, 0.8f } };
    applyBarLocks (fresh, lane, bpb, 4);
    std::vector<int> pitches;
    for (const auto& n : fresh.notes)
        pitches.push_back (n.pitch);
    CHECK (pitches == std::vector<int> { 50, 51, 41, 53 });
    CHECK (fresh.notes[1].length == 1.0);   // 51 cut where bar 2 starts
    // Past the section's last bar: ignored.
    ComposerLane past;
    past.lockedBars = bits ({ 9 });
    LineIdea same = fresh;
    applyBarLocks (same, past, bpb, 4);
    CHECK (same.notes.size() == fresh.notes.size());
    // Let go again: nothing kept.
    unlockLaneBars (lane, bits ({ 1 }), bpb);
    CHECK_FALSE (lane.hasBarLocks());
    CHECK (lane.keptNotes.empty());
}

TEST_CASE ("0.202.0: drums with bars 1-3 and 5-7 locked: ROLL changes only bars 4 and 8, with a fill that lands", "[barlocks][ui]")
{
    Studio studio (false);
    ComposerPage page (studio);
    page.setRandomSeed (11);
    REQUIRE (page.getState().bars == 8);
    REQUIRE (page.barCount() == 8);
    const auto before = page.getIdea (L::drums);
    REQUIRE_FALSE (before.notes.empty());

    page.selectBars (L::drums, 0, 2);
    page.selectBars (L::drums, 4, 6, true);
    CHECK (page.getBarSelection (L::drums) == bits ({ 0, 1, 2, 4, 5, 6 }));
    CHECK (page.getBarsKey (L::drums).getButtonText() == "LOCK BARS");
    page.lockSelectedBars (L::drums);
    CHECK (page.getLockedBars (L::drums) == bits ({ 0, 1, 2, 4, 5, 6 }));
    CHECK (page.getBarSelection (L::drums) == 0);
    CHECK (page.getBarsKey (L::drums).getButtonText() == "UNLOCK ALL");
    // Locked: the lane as it was (the notes ringing out of a locked run cut at its end).
    for (const auto bar : { 0, 1, 2 })
        CHECK (sameNotes (notesInBar (page.getIdea (L::drums), bar), keptBar (before, bar, 12.0)));

    std::set<juce::String> fills;
    auto bar4Changed = 0, bar8Changed = 0;
    auto lastBar4 = notesInBar (page.getIdea (L::drums), 3), lastBar8 = notesInBar (page.getIdea (L::drums), 7);
    for (int i = 0; i < 6; ++i)
    {
        page.roll (L::drums);
        const auto& idea = page.getIdea (L::drums);
        fills.insert (page.getFill());
        for (const auto bar : { 0, 1, 2 })
            CHECK (sameNotes (notesInBar (idea, bar), keptBar (before, bar, 12.0)));
        for (const auto bar : { 5, 6 })
            CHECK (sameNotes (notesInBar (idea, bar), keptBar (before, bar, 28.0)));
        // Bar 5 (where the fill lands): its kept notes, a crash and a kick on its downbeat at most added.
        {
            auto now = notesInBar (idea, 4);
            const auto kept = keptBar (before, 4, 28.0);
            for (const auto& k : kept)
                CHECK (std::any_of (now.begin(), now.end(), [&] (const LineNote& n) { return sameNotes ({ n }, { k }); }));
            CHECK (now.size() <= kept.size() + 2);
            CHECK (std::any_of (now.begin(), now.end(), [] (const LineNote& n) { return std::abs (n.beat - 16.0) < 0.03 && isCrash (n.pitch); }));
            CHECK (std::any_of (now.begin(), now.end(), [] (const LineNote& n) { return std::abs (n.beat - 16.0) < 0.03 && n.pitch >= 33 && n.pitch <= 36; }));
        }
        // Nothing from bar 4 rings into bar 5.
        for (const auto& n : notesInBar (idea, 3))
            CHECK (n.beat + n.length <= 16.0 + 1.0e-6);
        const auto now4 = notesInBar (idea, 3), now8 = notesInBar (idea, 7);
        bar4Changed += sameNotes (now4, lastBar4) ? 0 : 1;
        bar8Changed += sameNotes (now8, lastBar8) ? 0 : 1;
        lastBar4 = now4;
        lastBar8 = now8;
    }
    CHECK (fills.size() >= 3);
    CHECK (bar4Changed >= 5);
    CHECK (bar8Changed >= 5);
    // GENERATE too leaves the locked bars.
    page.generate();
    for (const auto bar : { 0, 1, 2 })
        CHECK (sameNotes (notesInBar (page.getIdea (L::drums), bar), keptBar (before, bar, 12.0)));
    // Every bar let go: the fill too.
    page.unlockAllBars (L::drums);
    CHECK (page.getLockedBars (L::drums) == 0);
    CHECK (page.getFill().isEmpty());
}

TEST_CASE ("0.202.0: the drums' FILLS options differ and each lands a crash on bar 5", "[barlocks][ui][options]")
{
    Studio studio (false);
    ComposerPage page (studio);
    page.setRandomSeed (3);
    CHECK (page.fillOptions().empty());   // (no bar locks: no fills)
    page.lockBars (L::drums, bits ({ 0, 1, 2, 4, 5, 6 }));
    const auto options = page.fillOptions();
    REQUIRE (options.size() >= 8);   // (the beat maker's four kinds over one and two beats)
    std::set<juce::String> shapes;
    for (const auto& o : options)
    {
        CHECK (o.group == ComposerPage::fillsGroup);
        CHECK (o.id >= ComposerPage::fillId);
        juce::String shape;
        for (const auto& n : notesInBar (o.idea, 3))
            shape << juce::String (n.beat, 3) << ":" << n.pitch << ",";
        shapes.insert (shape);
        CHECK (std::any_of (o.idea.notes.begin(), o.idea.notes.end(), [] (const LineNote& n) { return std::abs (n.beat - 16.0) < 0.03 && isCrash (n.pitch); }));
    }
    CHECK (shapes.size() == options.size());
    // The browser lists them first, in their own group; USE IT keeps one.
    const auto all = page.laneOptions (L::drums);
    REQUIRE (all.size() > options.size());
    CHECK (all.front().id == ComposerPage::fillId);
    page.useOption (L::drums, ComposerPage::fillId + 2);
    CHECK (page.getFill().isNotEmpty());
    CHECK (sameNotes (page.getIdea (L::drums).notes, options[2].idea.notes));
    // HEAR in the browser tries one and puts it back unless used.
    const auto fill = page.getFill();
    page.tryOption (L::drums, ComposerPage::fillId + 5);
    CHECK (page.getFill() != fill);
    page.endTrial (false);
    CHECK (page.getFill() == fill);
    page.stop();
}

TEST_CASE ("0.202.0: a bass lane with its last bar unlocked: only that bar is rolled", "[barlocks][ui]")
{
    Studio studio (false);
    ComposerPage page (studio);
    page.setRandomSeed (7);
    page.setStyle (L::bass, 0);   // (a style of its own: the default follows the drums' kick)
    const auto before = page.getIdea (L::bass);
    REQUIRE_FALSE (before.notes.empty());
    page.selectBars (L::bass, 0, 6);
    page.lockSelectedBars (L::bass);
    CHECK (page.getLockedBars (L::bass) == bits ({ 0, 1, 2, 3, 4, 5, 6 }));
    auto changed = 0;
    auto last = notesInBar (page.getIdea (L::bass), 7);
    for (int i = 0; i < 5; ++i)
    {
        page.roll (L::bass);
        const auto& idea = page.getIdea (L::bass);
        for (int bar = 0; bar < 7; ++bar)
            CHECK (sameNotes (notesInBar (idea, bar), keptBar (before, bar, 28.0)));
        const auto now = notesInBar (idea, 7);
        changed += sameNotes (now, last) ? 0 : 1;
        last = now;
    }
    CHECK (changed >= 3);
    // The whole-lane LOCK as before: GENERATE leaves the lane exactly as it is.
    page.setLocked (L::bass, true);
    const auto held = page.getIdea (L::bass).notes;
    page.generate();
    CHECK (sameNotes (page.getIdea (L::bass).notes, held));
}

TEST_CASE ("0.202.0: bar locks in the Architect's saved state (older text: none)", "[barlocks]")
{
    ComposerState s;
    auto& drums = s.lane (L::drums);
    drums.lockedBars = bits ({ 0, 2, 63 });
    LineNote ks { 3.9, 0.1, 13, 0.5f };
    ks.keyswitch = true;
    LineNote bent { 8.25, 0.75, 52, 0.71f };
    bent.effect = LineNote::Effect::bend;
    bent.amount = 2.0f;
    bent.harmony = true;
    drums.keptNotes = { { 0.0, 0.5, 36, 0.9f }, ks, bent };
    drums.fill = "lib:C:\\Grooves, mine; 1|2\\Fill 3.mid";
    const auto text = s.toString();
    const auto back = ComposerState::fromString (text);
    CHECK (back == s);
    CHECK (back.lane (L::drums).fill == drums.fill);
    CHECK (back.lane (L::drums).lockedBars == drums.lockedBars);
    REQUIRE (back.lane (L::drums).keptNotes.size() == 3);
    CHECK (back.lane (L::drums).keptNotes[1].keyswitch);
    CHECK (back.lane (L::drums).keptNotes[2].effect == LineNote::Effect::bend);
    CHECK (back.lane (L::drums).keptNotes[2].harmony);
    CHECK_FALSE (back.lane (L::bass).hasBarLocks());

    // Text from before 0.202.0 (13 fields a lane): no locks.
    ComposerState plain;
    plain.lane (L::bass).octave = 1;
    auto old = plain.toString();
    old = old.replace (",0,-,-", "");
    CHECK_FALSE (old.contains (",-,-"));
    const auto read = ComposerState::fromString (old);
    CHECK (read.lane (L::bass).octave == 1);
    for (const auto l : composerLayers())
        CHECK_FALSE (read.lane (l).hasBarLocks());
}

TEST_CASE ("0.202.0: USE ALL writes the lane with its locked bars; SYNC followers read them", "[barlocks][ui]")
{
    Studio studio (false);
    ComposerPage page (studio);
    page.setRandomSeed (21);
    page.lockBars (L::bass, bits ({ 0, 1, 2, 3 }));
    page.roll (L::bass);
    const auto idea = page.getIdea (L::bass);
    const auto tracks = page.useAll();
    REQUIRE_FALSE (tracks.empty());
    const auto bassTrack = studio.findIdeaTrack (LineIdea::Part::bass);
    const auto* t = studio.getProject().findTrack (bassTrack);
    REQUIRE (t != nullptr);
    REQUIRE_FALSE (t->parts.empty());
    const auto& part = t->parts.back();
    auto heard = 0;
    for (const auto& n : idea.notes)
        if (! n.keyswitch)
            ++heard;
    CHECK ((int) part.notes.size() == heard);
    for (const auto& n : notesInBar (idea, 1))
        CHECK (std::any_of (part.notes.begin(), part.notes.end(), [&] (const Note& p)
                            { return std::abs (p.start - n.beat) < 1.0e-4 && p.pitch == n.pitch; }));

    // SYNC on the guitar with its first bars locked: the bass (following it) keeps those bars as the guitar is rolled.
    ComposerState s;
    s.syncLane = (int) L::guitar;
    const auto spans = composerSpans (s, 0.0, bpb);
    ComposerContext c;
    const auto guitar = composeLayer (L::guitar, s, spans, c);
    const auto bass = composeLayer (L::bass, s, spans, c);
    lockLaneBars (s.lane (L::guitar), guitar, bits ({ 0, 1 }), bpb, composerBarCount (spans, bpb));
    auto rolled = s;
    juce::Random random (4);
    rollLane (rolled, L::guitar, random);
    const auto bassAfter = composeLayer (L::bass, rolled, spans, c);
    CHECK (sameStarts (notesInBar (bassAfter, 0), notesInBar (bass, 0)));
    CHECK (sameNotes (notesInBar (composeLayer (L::guitar, rolled, spans, c), 1), keptBar (guitar, 1, 8.0)));
}

TEST_CASE ("0.203.0: each open bar gets its own fill", "[barlocks][ui]")
{
    // The owner: "make each open bar get its own fill".
    Studio studio (false);
    ComposerPage page (studio);
    page.setRandomSeed (5);
    REQUIRE (page.barCount() == 8);
    page.selectBars (L::drums, 0, 2);
    page.selectBars (L::drums, 4, 6, true);
    page.lockSelectedBars (L::drums);
    REQUIRE (page.getLockedBars (L::drums) == bits ({ 0, 1, 2, 4, 5, 6 }));

    auto differ = 0;
    for (int i = 0; i < 8; ++i)
    {
        page.roll (L::drums);
        const auto fills = barFills (page.getState().lane (L::drums), page.barCount());
        REQUIRE (fills.size() == 2);   // bars 4 and 8, the open ones
        REQUIRE (fills.count (3) == 1);
        REQUIRE (fills.count (7) == 1);
        if (fills.at (3) != fills.at (7))
        {
            ++differ;
            // Different fills: different notes over the end of the two bars (placed from each bar's start).
            const auto& idea = page.getIdea (L::drums);
            std::vector<LineNote> a, b;
            for (const auto& n : idea.notes)
            {
                if (n.beat >= 14.0 - 1.0e-6 && n.beat < 16.0) { auto m = n; m.beat -= 12.0; a.push_back (m); }
                if (n.beat >= 30.0 - 1.0e-6 && n.beat < 32.0) { auto m = n; m.beat -= 28.0; b.push_back (m); }
            }
            CHECK_FALSE (sameStarts (a, b));
        }
        // Each lands: bar 5 starts on a crash.
        const auto bar5 = notesInBar (page.getIdea (L::drums), 4);
        CHECK (std::any_of (bar5.begin(), bar5.end(), [] (const LineNote& n) { return std::abs (n.beat - 16.0) < 0.03 && isCrash (n.pitch); }));
    }
    CHECK (differ == 8);   // (with three fills or more, never the open bar before's)

    // A fill chosen with only bar 8 chosen: bar 4 keeps its own.
    const auto before = barFills (page.getState().lane (L::drums), page.barCount());
    int picked = -1;
    const auto all = composerFills (page.getState(), page.getContext());
    for (int i = 0; i < (int) all.size() && picked < 0; ++i)
        if (all[(size_t) i].id != before.at (3) && all[(size_t) i].id != before.at (7))
            picked = i;
    REQUIRE (picked >= 0);
    page.selectBars (L::drums, 7, 7);
    page.useOption (L::drums, ComposerPage::fillId + picked);
    auto after = barFills (page.getState().lane (L::drums), page.barCount());
    CHECK (after.at (3) == before.at (3));
    CHECK (after.at (7) == all[(size_t) picked].id);
    // Nothing chosen: every open bar.
    page.clearBarSelection (L::drums);
    page.useOption (L::drums, ComposerPage::fillId + picked);
    after = barFills (page.getState().lane (L::drums), page.barCount());
    CHECK (after.at (3) == all[(size_t) picked].id);
    CHECK (after.at (7) == all[(size_t) picked].id);

    // 0.202.0's one fill id still reads as every open bar's.
    ComposerLane old;
    old.lockedBars = bits ({ 0, 1, 2 });
    old.fill = "gen:0:1";
    const auto read = barFills (old, 4);
    REQUIRE (read.size() == 1);
    CHECK (read.at (3) == "gen:0:1");
    // And the per-bar text survives the saved state.
    auto s = page.getState();
    const auto text = s.toString();
    const auto back = ComposerState::fromString (text);
    CHECK (barFills (back.lane (L::drums), 8) == barFills (s.lane (L::drums), 8));
}
