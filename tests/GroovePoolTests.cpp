// (0.137.0, the owner's rules for the composer's drums lane) The section's 100 library grooves: two thirds from the
// Toontrack folder, only grooves naming the section, 4 to 16 bars, no left-out styles (electronic, brushes, jazz, big
// band, R&B, reggae), best fit of kick and snare to the guitar first, the kick fitted a little, and ROLL going
// through all 100 in a random order before any comes back.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TestHelpers.h"
#include "engine/Composer.h"
#include "engine/DrumIdeas.h"

#include <array>
#include <set>

using namespace pr7s;

namespace
{
    // A groove known only by its listing (the pool reads no files): path, bars, and a kick on every beat in `kicks`.
    LibraryGroove groove (const juce::String& path, int bars, std::vector<float> kicks = { 0.0f, 2.0f }, std::vector<float> snares = { 1.0f, 3.0f })
    {
        LibraryGroove g;
        g.file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("grooves").getChildFile (path);   // (an absolute path on every system)
        g.name = g.file.getFileNameWithoutExtension();
        g.folder = g.file.getParentDirectory().getFileName();
        g.bpm = 120.0;
        g.bars = bars;
        auto rhythm = std::make_shared<GrooveRhythm>();
        rhythm->lengthBeats = 4.0;
        rhythm->kicks = kicks;
        rhythm->snares = snares;
        g.rhythm = rhythm;
        classifyGroove (g);
        return g;
    }

    std::vector<ChordSpan> eightBars()
    {
        std::vector<ChordSpan> spans;
        for (int bar = 0; bar < 8; ++bar)
            spans.push_back ({ bar * 4.0, bar * 4.0 + 4.0, Chord { 9, Chord::Type::minor }, Key {} });
        return spans;
    }

    ComposerState verseState()
    {
        ComposerState s;
        s.lane (ComposerLayer::drums).style = 1;   // (a verse)
        s.lane (ComposerLayer::guitar).on = false;  // (no riff: the fit doesn't count)
        return s;
    }
}

TEST_CASE ("0.137.0: the drums lane's pool: 100, two thirds Toontrack, the section's, 4 to 16 bars, no left-out styles", "[composer][library][groovepool]")
{
    std::vector<LibraryGroove> all;
    for (int i = 0; i < 150; ++i)
        all.push_back (groove ("Toontrack/Metal Pack/Verse/Verse " + juce::String (i) + ".mid", 4 + i % 13));
    for (int i = 0; i < 150; ++i)
        all.push_back (groove ("Other Packs/Rock/Verse/Verse " + juce::String (i) + ".mid", 8));
    for (int i = 0; i < 50; ++i)
        all.push_back (groove ("Toontrack/Metal Pack/Chorus/Chorus " + juce::String (i) + ".mid", 8));
    // Left out: the owner's styles, too short or too long, fills.
    all.push_back (groove ("Toontrack/Jazz Kit/Verse/Swing Verse.mid", 8));
    all.push_back (groove ("Toontrack/Brushes/Verse/Brush Verse.mid", 8));
    all.push_back (groove ("Toontrack/Electronic Beats/Verse/Synth Verse.mid", 8));
    all.push_back (groove ("Toontrack/Big Band/Verse/Band Verse.mid", 8));
    all.push_back (groove ("Toontrack/R&B Grooves/Verse/Soul Verse.mid", 8));
    all.push_back (groove ("Toontrack/Reggae/Verse/Skank Verse.mid", 8));
    all.push_back (groove ("Toontrack/Metal Pack/Verse/Short Verse.mid", 2));
    all.push_back (groove ("Toontrack/Metal Pack/Verse/Long Verse.mid", 32));
    all.push_back (groove ("Toontrack/Metal Pack/Verse Fills/Verse Fill 1.mid", 4));

    ComposerContext c;
    c.libraryGrooves = all;
    const auto pool = composerGroovePool (verseState(), eightBars(), c);
    REQUIRE (pool.size() == 200);   // (0.178.0: 200, 50 / 100 / 50; no favourite folders: their 50 to Toontrack)
    auto toontrack = 0;
    for (const auto& g : pool)
    {
        toontrack += g.toontrack ? 1 : 0;
        CHECK (g.file.getFullPathName().containsIgnoreCase ("verse"));
        CHECK_FALSE (g.fill);
        CHECK_FALSE (g.excluded);
        CHECK (g.bars >= 4);
        CHECK (g.bars <= 16);
        for (const auto* word : { "Jazz", "Brush", "Electronic", "Big Band", "R&B", "Reggae", "Chorus" })
            CHECK_FALSE (g.file.getFullPathName().containsIgnoreCase (word));
    }
    CHECK (toontrack == 150);

    // Few others: Toontrack takes the favourites' places, the others' missing ones go unfilled (Toontrack has no more).
    std::vector<LibraryGroove> fewOthers (all.begin(), all.begin() + 150);
    for (int i = 0; i < 10; ++i)
        fewOthers.push_back (all[150 + (size_t) i]);
    c.libraryGrooves = fewOthers;
    const auto mostly = composerGroovePool (verseState(), eightBars(), c);
    REQUIRE (mostly.size() == 160);
    auto t2 = 0;
    for (const auto& g : mostly)
        t2 += g.toontrack ? 1 : 0;
    CHECK (t2 == 150);

    // Few Toontrack verses: all of them, then the folder's others, then other folders' verses.
    std::vector<LibraryGroove> fewToontrack (all.begin(), all.begin() + 20);
    fewToontrack.insert (fewToontrack.end(), all.begin() + 150, all.begin() + 350);
    c.libraryGrooves = fewToontrack;
    const auto filled = composerGroovePool (verseState(), eightBars(), c);
    REQUIRE (filled.size() == 200);
    auto tVerse = 0, tOther = 0, oVerse = 0;
    for (const auto& g : filled)
    {
        if (g.toontrack && g.file.getParentDirectory().getFileName() == "Verse")   // (not "/Verse/": Windows writes backslashes)
            ++tVerse;
        else if (g.toontrack)
            ++tOther;
        else
            ++oVerse;
    }
    CHECK (tVerse == 20);
    CHECK (tOther == 50);    // (all the folder's others)
    CHECK (oVerse == 130);   // (their 50, the favourites' 50 and Toontrack's missing 30)
}

TEST_CASE ("0.137.0: a section with no groove of its own takes the nearest kind's", "[composer][library][groovepool]")
{
    std::vector<LibraryGroove> all;
    for (int i = 0; i < 10; ++i)
        all.push_back (groove ("Toontrack/Pack/Chorus/Chorus " + juce::String (i) + ".mid", 8));
    for (int i = 0; i < 10; ++i)
        all.push_back (groove ("Toontrack/Pack/Verse/Verse " + juce::String (i) + ".mid", 8));
    ComposerContext c;
    c.libraryGrooves = all;
    auto s = verseState();
    const auto count = [] (const std::vector<LibraryGroove>& pool, const char* word)
    {
        auto n = 0;
        for (const auto& g : pool)
            n += g.file.getFullPathName().contains (word) ? 1 : 0;
        return n;
    };
    // A solo (none named so): the chorus's all in (then, short of 66, the Toontrack folder's others).
    s.lane (ComposerLayer::drums).style = 6;
    const auto solo = composerGroovePool (s, eightBars(), c);
    CHECK (count (solo, "Chorus") == 10);
    CHECK (solo.size() == 20);
    // A bridge: the verse's.
    s.lane (ComposerLayer::drums).style = 3;
    CHECK (count (composerGroovePool (s, eightBars(), c), "Verse") == 10);
    // With a groove naming the section, no other kind's.
    all.push_back (groove ("Others/Bridge/Bridge 1.mid", 8));
    c.libraryGrooves = all;
    const auto bridge = composerGroovePool (s, eightBars(), c);
    CHECK (count (bridge, "Bridge") == 1);
    CHECK (bridge.size() == 21);   // (the bridge, then the Toontrack folder's 20 as the fill-up)
}

TEST_CASE ("0.137.0: the kick and snare's fit to the guitar ranks the pool", "[composer][library][groovepool]")
{
    // A riff hitting 0, 0.75, 1.5, 2.5 every bar, accents on 0 and 1.5.
    SyncRhythm riff;
    for (int bar = 0; bar < 8; ++bar)
    {
        for (const auto b : { 0.0, 0.75, 1.5, 2.5 })
            riff.all.push_back (bar * 4.0 + b);
        riff.accents.push_back (bar * 4.0);
        riff.accents.push_back (bar * 4.0 + 1.5);
    }
    GrooveRhythm locked;
    locked.lengthBeats = 4.0;
    locked.kicks = { 0.0f, 0.75f, 1.5f, 2.5f };
    locked.snares = { 1.0f, 3.0f };
    GrooveRhythm elsewhere;
    elsewhere.lengthBeats = 4.0;
    elsewhere.kicks = { 0.5f, 1.25f, 3.25f };
    elsewhere.snares = { 0.25f, 2.25f };
    const auto good = composerGrooveFit (locked, riff, 4.0, 32.0);
    const auto bad = composerGrooveFit (elsewhere, riff, 4.0, 32.0);
    CHECK (good == Catch::Approx (1.0));
    CHECK (bad < 0.2);
    CHECK (composerGrooveFit (locked, SyncRhythm {}, 4.0, 32.0) == Catch::Approx (0.5));

    // In the pool: the grooves locked to the guitar first (the guitar lane on).
    std::vector<LibraryGroove> all;
    for (int i = 0; i < 30; ++i)
        all.push_back (groove ("Toontrack/Pack/Verse/Off " + juce::String (i) + ".mid", 8, elsewhere.kicks, elsewhere.snares));
    all.push_back (groove ("Toontrack/Pack/Verse/On The Riff.mid", 8, locked.kicks, locked.snares));
    ComposerContext c;
    c.libraryGrooves = all;
    ComposerState s;
    s.lane (ComposerLayer::drums).style = 1;
    const auto pool = composerGroovePool (s, eightBars(), c);
    REQUIRE_FALSE (pool.empty());
    // (which wins depends on the guitar's riff: its own fit at least as good as any other's)
    const auto riffNow = composerDrumRiff (s, eightBars(), c);
    REQUIRE_FALSE (riffNow.all.empty());
    CHECK (composerGrooveFit (*pool.front().rhythm, riffNow, 4.0, 32.0) >= composerGrooveFit (*pool.back().rhythm, riffNow, 4.0, 32.0));
}

TEST_CASE ("0.137.0: ROLL goes through all 100 in a random order, none again until all have been", "[composer][library][groovepool]")
{
    std::vector<LibraryGroove> all;
    for (int i = 0; i < 70; ++i)   // (0.178.0: 70 + 30, all of them: a pool of 100)
        all.push_back (groove ("Toontrack/Pack/Verse/Verse " + juce::String (i) + ".mid", 8));
    for (int i = 0; i < 30; ++i)
        all.push_back (groove ("Others/Verse/Verse " + juce::String (i) + ".mid", 8));
    ComposerContext c;
    c.libraryGrooves = all;
    auto s = verseState();
    const auto spans = eightBars();
    const auto pool = composerGroovePool (s, spans, c);
    REQUIRE (pool.size() == 100);
    juce::Random random (7);
    std::set<juce::String> seen { composerLibraryGroove (s, spans, c)->file.getFullPathName() };
    std::vector<juce::String> order;
    for (int i = 0; i < 99; ++i)
    {
        REQUIRE (rollLibraryGroove (s, spans, c, random));
        const auto now = composerLibraryGroove (s, spans, c)->file.getFullPathName();
        CHECK (seen.count (now) == 0);   // (never one already heard)
        seen.insert (now);
        order.push_back (now);
    }
    CHECK (seen.size() == 100);
    // Not the list's order.
    auto inOrder = 0;
    for (size_t i = 0; i < order.size(); ++i)
        inOrder += order[i] == pool[(i + 1) % pool.size()].file.getFullPathName() ? 1 : 0;
    CHECK (inOrder < 10);
    // Round again: never the one playing first, then 99 more before any repeat.
    const auto last = order.back();
    REQUIRE (rollLibraryGroove (s, spans, c, random));
    std::set<juce::String> second { composerLibraryGroove (s, spans, c)->file.getFullPathName() };
    CHECK (second.count (last) == 0);
    for (int i = 0; i < 98; ++i)
    {
        REQUIRE (rollLibraryGroove (s, spans, c, random));
        const auto now = composerLibraryGroove (s, spans, c)->file.getFullPathName();
        CHECK (second.count (now) == 0);
        second.insert (now);
    }
    // Kept with the song.
    const auto back = ComposerState::fromString (s.toString());
    CHECK (back == s);
    CHECK (back.libraryPick == s.libraryPick);
    CHECK (back.groovesHeard.isNotEmpty());
    // No pool: false (the take rolls as before).
    ComposerContext none;
    CHECK_FALSE (rollLibraryGroove (s, spans, none, random));
}

TEST_CASE ("0.137.0: the kick fitted to the riff (two changes a bar at most)", "[composer][library][groovepool]")
{
    // The groove: kicks on 0 and 2 of every bar (36), a snare on 1 and 3.
    LineIdea drums { LineIdea::Part::drums, "", "", {} };
    for (int bar = 0; bar < 2; ++bar)
    {
        drums.notes.push_back ({ bar * 4.0, 0.25, 36, 0.8f });
        drums.notes.push_back ({ bar * 4.0 + 1.0, 0.25, 38, 0.8f });
        drums.notes.push_back ({ bar * 4.0 + 2.0, 0.25, 36, 0.8f });
        drums.notes.push_back ({ bar * 4.0 + 3.0, 0.25, 38, 0.8f });
    }
    // The riff: accents on 0, 1.5, 2.75 and 3.5; hits there only.
    SyncRhythm riff;
    for (int bar = 0; bar < 2; ++bar)
        for (const auto b : { 0.0, 1.5, 2.75, 3.5 })
        {
            riff.all.push_back (bar * 4.0 + b);
            riff.accents.push_back (bar * 4.0 + b);
        }
    const auto changes = fitKicksToRiff (drums, riff, 4.0);
    CHECK (changes == 4);
    std::vector<double> kicks;
    for (const auto& n : drums.notes)
        if (n.pitch == 36)
            kicks.push_back (n.beat);
    // Each bar: the kick on 2 stays (no riff hit within a 16th of it), kicks added on 1.5 and 2.75, none on 3.5 (two a bar).
    CHECK (std::count (kicks.begin(), kicks.end(), 1.5) == 1);
    CHECK (std::count (kicks.begin(), kicks.end(), 0.0) == 1);
    CHECK (kicks.size() == 8);
    // The snares as they were.
    auto snares = 0;
    for (const auto& n : drums.notes)
        snares += n.pitch == 38 ? 1 : 0;
    CHECK (snares == 4);
}

TEST_CASE ("0.137.0: PR7-D's index rhythm read as kicks and snares (ghosts left out)", "[composer][library][groovepool]")
{
    const auto r = grooveRhythmFromText ("16;4;0:0,24,48|1:12,36~40,60|2:0,6,12");
    CHECK (r.lengthBeats == Catch::Approx (16.0));
    REQUIRE (r.kicks.size() == 3);
    CHECK (r.kicks[1] == Catch::Approx (2.0f));
    REQUIRE (r.snares.size() == 2);   // (the 40 under 55% of 127 left out)
    CHECK (r.snares[0] == Catch::Approx (1.0f));
    CHECK (r.snares[1] == Catch::Approx (5.0f));
}

TEST_CASE ("0.178.0: favourite folders: 50 of the 200, the rest of Toontrack 100, the other packs 50", "[composer][library][groovepool][favourites]")
{
    std::vector<LibraryGroove> all;
    for (int i = 0; i < 150; ++i)
        all.push_back (groove ("Toontrack/Metal Pack/Verse/Verse " + juce::String (i) + ".mid", 8));
    for (int i = 0; i < 60; ++i)
        all.push_back (groove ("Toontrack/Favourite Pack/Verse/Fav Verse " + juce::String (i) + ".mid", 8));
    for (int i = 0; i < 120; ++i)
        all.push_back (groove ("Other Packs/Rock/Verse/Rock Verse " + juce::String (i) + ".mid", 8));
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("grooves");
    ComposerContext c;
    c.libraryGrooves = all;
    c.favouriteFolders = { root.getChildFile ("Toontrack/Favourite Pack").getFullPathName() };
    const auto counts = [] (const std::vector<LibraryGroove>& pool)
    {
        std::array<int, 3> n {};
        for (const auto& g : pool)
            ++n[g.favourite ? 0 : g.toontrack ? 1 : 2];
        return n;
    };
    auto s = verseState();
    const auto spans = eightBars();
    const auto pool = composerGroovePool (s, spans, c);
    REQUIRE (pool.size() == 200);
    CHECK (counts (pool) == std::array<int, 3> { 50, 100, 50 });
    for (const auto& g : pool)
        CHECK (g.favourite == g.file.getFullPathName().contains ("Favourite Pack"));

    // AUTOMATIC prefers the favourites: every take picks one.
    for (int v = 0; v < 12; ++v)
    {
        s.lane (ComposerLayer::drums).variation = v;
        const auto picked = composerLibraryGroove (s, spans, c);
        REQUIRE (picked.has_value());
        CHECK (picked->favourite);
    }
    // ROLL still goes through all 200 (favourites a quarter of them).
    juce::Random random (3);
    std::set<juce::String> rolled;
    auto favouriteRolls = 0;
    for (int i = 0; i < 199; ++i)   // (every one but the one playing first, a favourite, before any comes back)
    {
        REQUIRE (rollLibraryGroove (s, spans, c, random));
        const auto now = composerLibraryGroove (s, spans, c);
        rolled.insert (now->file.getFullPathName());
        favouriteRolls += now->favourite ? 1 : 0;
    }
    CHECK (rolled.size() == 199);
    CHECK (favouriteRolls == 49);

    // Favourites short: their places to Toontrack first.
    std::vector<LibraryGroove> fewFavourites (all.begin(), all.begin() + 170);   // (150 Toontrack, 20 favourites)
    fewFavourites.insert (fewFavourites.end(), all.begin() + 210, all.end());
    c.libraryGrooves = fewFavourites;
    CHECK (counts (composerGroovePool (verseState(), spans, c)) == std::array<int, 3> { 20, 130, 50 });

    // Other packs short: their places to Toontrack, then the favourites.
    std::vector<LibraryGroove> fewOthers (all.begin(), all.begin() + 210);
    for (int i = 0; i < 10; ++i)
        fewOthers.push_back (all[210 + (size_t) i]);
    c.libraryGrooves = fewOthers;
    CHECK (counts (composerGroovePool (verseState(), spans, c)) == std::array<int, 3> { 50, 140, 10 });

    // No favourite folders: as before them, Toontrack 150 and the others 50.
    c.libraryGrooves = all;
    c.favouriteFolders = {};
    CHECK (counts (composerGroovePool (verseState(), spans, c)) == std::array<int, 3> { 0, 150, 50 });
}

TEST_CASE ("0.178.0: a file in a favourite folder: subfolders count, a name only starting the same doesn't", "[groovepool][favourites]")
{
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("grooves");
    const juce::StringArray folders { root.getChildFile ("Packs/Fav").getFullPathName() };
    CHECK (isInFavouriteFolder (root.getChildFile ("Packs/Fav/Verse/One.mid"), folders));
    CHECK (isInFavouriteFolder (root.getChildFile ("Packs/Fav/One.mid"), folders));
    CHECK (isInFavouriteFolder (root.getChildFile ("Packs/FAV/One.mid"), folders));   // (upper or lower case alike)
    CHECK_FALSE (isInFavouriteFolder (root.getChildFile ("Packs/Favourite/One.mid"), folders));
    CHECK_FALSE (isInFavouriteFolder (root.getChildFile ("Packs/One.mid"), folders));
    CHECK_FALSE (isInFavouriteFolder (root.getChildFile ("Packs/Fav/One.mid"), {}));
}
