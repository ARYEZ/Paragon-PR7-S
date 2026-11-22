// CHORD ROLL (0.208.0, the owner): a section's chords drawn anew in its key, chord to chord (a Markov chain) by our
// progressions' weights, the section's feel weighting them; in ARCHITECT on the chord line and in the block editor.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TestHelpers.h"
#include <map>
#include <numeric>
#include <set>
#include "engine/ChordRoll.h"
#include "engine/Studio.h"
#include "ui/ComposerPage.h"
#include "ui/Finish.h"
#include "ui/MainView.h"
#include "ui/SectionEditor.h"

using namespace pr7s;

namespace
{
    Key keyNamed (int pitchClass, bool minor)
    {
        for (const auto& k : Key::all())
            if (k.minor == minor && k.pitchClass() == pitchClass)
                return k;
        return {};
    }

    Key dSharpMinor() { return Key { 1, 1, true }; }

    double totalOf (const std::vector<RolledChord>& chords)
    {
        auto t = 0.0;
        for (const auto& c : chords)
            t += c.beats;
        return t;
    }

    // How often each chord (by its index in the weights) opens, over many seeds.
    std::map<int, int> openings (const Key& key, const juce::String& section, int rolls)
    {
        const auto w = chordRollWeights (key, section);
        std::map<int, int> out;
        for (int seed = 1; seed <= rolls; ++seed)
            ++out[w.indexOf (rollChords (key, section, 4, 4.0, (juce::uint32) seed).front().chord)];
        return out;
    }

    // The chord sounding at a beat inside a block (from the chord track).
    std::optional<Chord> chordAt (const Studio& studio, double beat)
    {
        std::optional<Chord> out;
        for (const auto& e : withLengths (studio.getProject().chordTrack, 4.0))
            if (beat >= e.beat - 1.0e-9 && beat < e.beat + e.length - 1.0e-9)
                out = e.chord;
        return out;
    }
}

TEST_CASE ("0.208.0: CHORD ROLL: the same seed the same chords; every chord in the key; the bars filled exactly", "[chordroll]")
{
    const auto a = rollChords (dSharpMinor(), "Solo", 8, 4.0, 42);
    const auto b = rollChords (dSharpMinor(), "Solo", 8, 4.0, 42);
    CHECK (a == b);
    auto differs = false;
    for (juce::uint32 seed = 43; seed < 60; ++seed)
        differs = differs || rollChords (dSharpMinor(), "Solo", 8, 4.0, seed) != a;
    CHECK (differs);

    const juce::StringArray sections { "", "Intro", "Verse 2", "Pre-Chorus", "Chorus", "Post-Chorus", "Bridge", "Breakdown", "Outro", "Solo" };
    for (const auto& key : Key::all())
        for (const auto& section : sections)
            for (const auto bars : { 1, 2, 3, 4, 6, 8, 9, 16 })
                for (const auto bpb : { 3.0, 4.0 })
                {
                    const auto chords = rollChords (key, section, bars, bpb, (juce::uint32) (bars * 31 + key.pitchClass()));
                    REQUIRE_FALSE (chords.empty());
                    CHECK (totalOf (chords) == Catch::Approx (bars * bpb));
                    for (size_t i = 0; i < chords.size(); ++i)
                    {
                        CHECK (chords[i].chord.isInKey (key));
                        CHECK (chords[i].beats > 0.0);
                        if (i > 0)
                            CHECK (chords[i].chord != chords[i - 1].chord);   // (never twice in a row)
                    }
                }
}

TEST_CASE ("0.208.0: CHORD ROLL: the first chord drawn by weight: the tonic the most often, under 70 %, others open too", "[chordroll]")
{
    for (const auto& key : { dSharpMinor(), keyNamed (0, false), keyNamed (9, true) })
        for (const auto& section : { juce::String(), juce::String ("Verse"), juce::String ("Chorus"), juce::String ("Solo") })
        {
            const auto counts = openings (key, section, 600);
            const auto tonic = counts.count (0) > 0 ? counts.at (0) : 0;
            for (const auto& [chord, n] : counts)
                if (chord != 0)
                    CHECK (n < tonic);
            CHECK (tonic < 600 * 0.7);
            CHECK (tonic > 600 * 0.3);
            CHECK (counts.size() >= 4);
        }
}

TEST_CASE ("0.208.0: CHORD ROLL: the weights counted from our progressions; each next chord follows them", "[chordroll]")
{
    // The library's moves are there: I to V, V to vi, vi to IV in major; i to VI, VI to VII in minor.
    const auto& major = chordRollCounts (false);
    CHECK (major[0][4] > 2.0);
    CHECK (major[4][5] > 2.0);
    CHECK (major[5][3] > 2.0);
    const auto& minor = chordRollCounts (true);
    CHECK (minor[0][5] > 2.0);
    CHECK (minor[5][6] > 2.0);

    // Every chord of the key can follow every other (smoothed), never itself.
    const auto key = keyNamed (0, false);
    const auto w = chordRollWeights (key, {});
    for (size_t a = 0; a < w.chords.size(); ++a)
        for (size_t b = 0; b < w.chords.size(); ++b)
            CHECK ((a == b ? w.next[a][b] == 0.0 : w.next[a][b] > 0.0));

    // The second chord after each first (no pull there) as often as the weights say.
    std::map<int, std::map<int, int>> seen;
    std::map<int, int> from;
    for (int seed = 1; seed <= 6000; ++seed)
    {
        const auto chords = rollChords (key, {}, 4, 4.0, (juce::uint32) seed);
        const auto a = w.indexOf (chords[0].chord), b = w.indexOf (chords[1].chord);
        ++seen[a][b];
        ++from[a];
    }
    const auto& row = w.next[0];
    const auto sum = std::accumulate (row.begin(), row.end(), 0.0);
    auto rare = 0.0, rareExpected = 0.0;
    for (size_t b = 0; b < row.size(); ++b)
    {
        const auto expected = row[b] / sum;
        const auto got = (double) seen[0][(int) b] / from[0];
        CHECK (std::abs (got - expected) < 0.05);
        if (major[0][b] == 0.0 && b != 0)
        {
            rare += got;
            rareExpected += expected;
        }
    }
    // Moves no progression makes: rare (only their small smoothing).
    CHECK (rare < rareExpected + 0.03);
    CHECK (rare < 0.15);
}

TEST_CASE ("0.208.0: CHORD ROLL: the section's feel changes what's drawn", "[chordroll]")
{
    const auto key = keyNamed (9, true);   // (A minor: VI is F, index 5)
    const auto verse = openings (key, "Verse", 800), chorus = openings (key, "Chorus", 800), bridge = openings (key, "Bridge", 800);
    const auto count = [] (const std::map<int, int>& m, int i) { return m.count (i) > 0 ? m.at (i) : 0; };
    CHECK (count (chorus, 5) > count (verse, 5) * 1.2);   // (the chorus opens on VI more)
    CHECK (count (bridge, 0) * 2 < count (verse, 0));      // (the bridge seldom opens home)
    // An outro ends on the tonic most of the time.
    auto home = 0;
    for (juce::uint32 seed = 1; seed <= 300; ++seed)
        home += rollChords (key, "Outro", 8, 4.0, seed).back().chord == keyChords (key).front() ? 1 : 0;
    CHECK (home > 150);
    // A solo's round comes back round: its last chord leads into its first more than a verse's does.
    const auto leads = [&key] (const juce::String& section)
    {
        const auto w = chordRollWeights (key, section);
        auto total = 0.0;
        for (juce::uint32 seed = 1; seed <= 400; ++seed)
        {
            const auto c = rollChords (key, section, 4, 4.0, seed);
            const auto& row = w.next[(size_t) w.indexOf (c.back().chord)];
            total += row[(size_t) w.indexOf (c.front().chord)] / *std::max_element (row.begin(), row.end());
        }
        return total;
    };
    CHECK (leads ("Solo") > leads (""));
}

TEST_CASE ("0.208.0: CHORD ROLL: rounds of four repeated; a layout kept; what to avoid drawn again", "[chordroll]")
{
    const auto key = keyNamed (0, false);
    // 16 bars of a verse: one chord a bar or two, the round of four over and over.
    for (juce::uint32 seed = 1; seed <= 40; ++seed)
    {
        const auto c = rollChords (key, "Verse", 16, 4.0, seed);
        REQUIRE ((c.size() == 16 || c.size() == 8));
        for (size_t i = 4; i < c.size(); ++i)
            CHECK (c[i].chord == c[i % 4].chord);
    }
    // Some long sections get two-bar chords, some don't.
    std::set<size_t> sizes;
    for (juce::uint32 seed = 1; seed <= 60; ++seed)
        sizes.insert (rollChords (key, "Chorus", 8, 4.0, seed).size());
    CHECK (sizes.count (8) == 1);
    CHECK (sizes.count (4) == 1);

    // The block's own rhythm kept.
    ChordRollRequest request;
    request.key = key;
    request.section = "Chorus";
    request.bars = 4;
    request.layout = { 8.0, 4.0, 2.0, 2.0 };
    const auto kept = rollChords (request, 7);
    REQUIRE (kept.size() == 4);
    CHECK (kept[0].beats == 8.0);
    CHECK (kept[3].beats == 2.0);

    // Avoid: the roll that seed gives is drawn again.
    request.layout.clear();
    const auto first = rollChords (request, 9);
    request.avoid = { rolledFingerprint (first) };
    const auto again = rollChords (request, 9);
    CHECK (again != first);
    CHECK (rolledFingerprint (again) != rolledFingerprint (first));
}

TEST_CASE ("0.208.0: CHORD ROLL in the studio: a block's chords in its key, one undo step, rolled again differently", "[chordroll]")
{
    Studio studio (false);
    studio.appendSongSection ("Verse", 4);
    studio.appendSongSection ("Solo", 8);
    studio.setSongKey (dSharpMinor());
    REQUIRE (studio.rollSectionChords (1, 5));
    auto chords = studio.getSectionChords (1);
    REQUIRE_FALSE (chords.empty());
    auto total = 0.0;
    for (const auto& c : chords)
    {
        CHECK (c.chord.isInKey (dSharpMinor()));
        total += c.beats;
    }
    CHECK (total == Catch::Approx (32.0));
    CHECK (studio.getSectionChords (0).empty());   // (the verse untouched)
    CHECK (studio.getProject().chordTrack.shown);
    studio.undo();
    CHECK (studio.getSectionChords (1).empty());
    CHECK_FALSE (studio.rollSectionChords (5, 1));   // (no such block)

    // The block's rhythm kept; every roll another.
    const Chord dsm { 3, Chord::Type::minor }, b { 11, Chord::Type::major }, fs { 6, Chord::Type::major };
    REQUIRE (studio.setSectionChords (1, { { dsm, 16.0 }, { b, 8.0 }, { fs, 8.0 } }));
    auto before = studio.getSectionChords (1);
    for (juce::uint32 seed = 1; seed <= 20; ++seed)
    {
        REQUIRE (studio.rollSectionChords (1, seed));
        const auto now = studio.getSectionChords (1);
        REQUIRE (now.size() == 3);
        CHECK (now[0].beats == Catch::Approx (16.0));
        CHECK (now[2].beats == Catch::Approx (8.0));
        CHECK (now != before);
        before = now;
    }
}

TEST_CASE ("0.208.0: CHORD ROLL: linked copies follow; a built block's lanes built again over the new chords, their recipe kept", "[chordroll]")
{
    Studio studio (false);
    studio.appendSongSection ("Verse", 4);
    studio.appendSongSection ("Chorus", 4);
    studio.appendSongSection ("Verse", 4);   // (a linked copy of the first)
    REQUIRE (studio.isSongSectionLinked (2));
    REQUIRE (studio.rollSectionChords (0, 11));
    CHECK_FALSE (studio.getSectionChords (0).empty());
    CHECK (studio.getSectionChords (2) == studio.getSectionChords (0));

    // The chorus built (its bass), then rolled: the bass is written again on the new chords, the recipe the same.
    const auto place = *studio.getSongSectionPlace (1);
    const Chord g { 7 };
    REQUIRE (studio.setSectionChords (1, { { g, 16.0 } }));
    std::vector<ChordSpan> spans { { place.start, place.end, g, Key {} } };
    ComposerState state;   // (the bass on the kick, the chord's root: its default style)
    const auto layers = composeLayers (state, spans, studio.getComposerContext (place.start));
    const auto& bass = layers[(size_t) ComposerLayer::bass];
    REQUIRE_FALSE (bass.notes.empty());
    const auto tracks = studio.buildSection ({ bass }, spans, "Chorus", {});
    REQUIRE (tracks.size() == 1);
    const auto blockId = studio.getSongSections().blocks[1].id;
    studio.noteSectionBuilt (blockId, state.toString(), { { (int) ComposerLayer::bass, tracks[0] } });
    const auto recipe = studio.getSongSections().blocks[1].recipe;
    const auto notesNow = [&]
    {
        std::vector<std::pair<double, int>> out;
        for (const auto& p : studio.getProject().findTrack (tracks[0])->parts)
            for (const auto& n : p.notes)
                if (p.start + n.start >= place.start - 1.0e-9 && p.start + n.start < place.end - 1.0e-9)
                    out.push_back ({ p.start + n.start, n.pitch });
        return out;
    };
    const auto before = notesNow();
    REQUIRE_FALSE (before.empty());
    REQUIRE (studio.rollSectionChords (1, 3));
    const auto after = notesNow();
    REQUIRE_FALSE (after.empty());
    CHECK (after != before);
    CHECK (studio.getSongSections().blocks[1].recipe == recipe);
    CHECK (ComposerState::fromString (recipe).lane (ComposerLayer::bass).style == ComposerState {}.lane (ComposerLayer::bass).style);
    // Every bass note on its chord's notes (built over them).
    for (const auto& [beat, pitch] : after)
    {
        const auto chord = chordAt (studio, beat);
        REQUIRE (chord.has_value());
        const auto pcs = chord->pitchClasses();
        CHECK (std::find (pcs.begin(), pcs.end(), ((pitch % 12) + 12) % 12) != pcs.end());
    }
    // One undo step: the chords and the bass back together.
    studio.undo();
    CHECK (studio.getSectionChords (1).size() == 1);
    CHECK (notesNow() == before);
}

TEST_CASE ("0.208.0: CHORD ROLL's keys: ARCHITECT's ROLL CHORDS (a block, or the progression's chords), the block editor's ROLL, POPULAR IN THE KEY", "[chordroll][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    {
        Studio studio (false);
        studio.appendSongSection ("Intro", 4);
        studio.appendSongSection ("Solo", 8);
        studio.setSongKey (dSharpMinor());
        MainView view (studio);
        view.setSize (1600, 900);
        view.openComposer();
        auto* window = view.getComposerWindow();
        REQUIRE (window != nullptr);
        auto& page = window->getPage();
        page.setSize (1700, 940);
        page.targetSection (studio.getSongSections().blocks[1].id);
        page.setRandomSeed (1234);
        REQUIRE (page.getRollChordsKey().isEnabled());
        REQUIRE (page.keyNamed ("ROLL CHORDS") == &page.getRollChordsKey());
        page.getRollChordsKey().onClick();
        const auto chords = studio.getSectionChords (1);
        REQUIRE_FALSE (chords.empty());
        for (const auto& c : chords)
            CHECK (c.chord.isInKey (dSharpMinor()));
        CHECK (page.getState().fromChordTrack);
        // The composer reads them: its spans are the block's chords.
        REQUIRE (page.getSpans().size() == chords.size());
        for (size_t i = 0; i < chords.size(); ++i)
            CHECK (page.getSpans()[i].chord == chords[i].chord);
        page.getRollChordsKey().onClick();
        CHECK (studio.getSectionChords (1) != chords);
        window->setVisible (false);
    }
    {
        // No block: the progression's chords swapped, its rhythm kept, the song untouched.
        Studio studio (false);
        MainView view (studio);
        view.setSize (1600, 900);
        view.openComposer();
        auto& page = view.getComposerWindow()->getPage();
        page.setSize (1700, 940);
        const auto before = page.getSpans();
        REQUIRE_FALSE (before.empty());
        const auto key = page.getState().key;
        auto changed = false;
        for (int i = 0; i < 3; ++i)
        {
            REQUIRE (page.rollChords());
            const auto now = page.getSpans();
            REQUIRE (now.size() == before.size());
            for (size_t j = 0; j < now.size(); ++j)
            {
                CHECK (now[j].chord.isInKey (key));
                CHECK (now[j].end - now[j].start == Catch::Approx (before[j].end - before[j].start));
                changed = changed || now[j].chord != before[j].chord;
            }
        }
        CHECK (changed);
        CHECK (studio.getProject().chordTrack.events.empty());

        // POPULAR IN THE KEY: the pads' list (those all in the key), chosen: its chords in the composer.
        const auto popular = page.getPopularInKey();
        REQUIRE_FALSE (popular.empty());
        for (int i = 0; i < (int) popular.size(); ++i)
        {
            page.usePopularProgression (i);
            const auto spans = page.getSpans();
            REQUIRE (spans.size() >= popular[(size_t) i].chords.size());
            for (size_t j = 0; j < spans.size(); ++j)
                CHECK (spans[j].chord == popular[(size_t) i].chords[j % popular[(size_t) i].chords.size()]);
        }
        view.getComposerWindow()->setVisible (false);
    }
    {
        // The block editor's ROLL.
        Studio studio (false);
        studio.appendSongSection ("Verse", 4);
        studio.appendSongSection ("Chorus", 4);
        SectionEditor editor (studio, studio.getSongSections().blocks[1].id);
        editor.getRollKey().onClick();
        CHECK_FALSE (editor.getChords().empty());
        CHECK (editor.getChords() == studio.getSectionChords (1));
        REQUIRE (editor.rollChords (77));
        for (const auto& c : editor.getChords())
            CHECK (c.chord.isInKey (studio.getSongSectionKey (1)));
    }
}
