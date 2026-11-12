// The droning chorus's band (0.89.0, the owner: with the rhythm guitar ringing its chords, "most of the rhythmic or
// interesting stuff comes from the lead guitar the bass guitar the keyboards in the drums"; their examples' full
// MIDI, Crawling and Clown, read for their shapes only).

#include <catch2/catch_test_macros.hpp>

#include "engine/Composer.h"
#include "engine/RiffGenerator.h"

using namespace pr7s;

namespace
{
    using L = ComposerLayer;

    ComposerContext plainContext()
    {
        ComposerContext c;
        c.humanize = 0.0f;
        return c;
    }

    ComposerState stateWith (RiffFlavour flavour)
    {
        ComposerState s;
        for (auto& lane : s.lanes)
            lane.on = true;
        s.bars = 8;
        for (int i = 0; i < (int) riffFlavours().size(); ++i)
            if (riffFlavours()[(size_t) i] == flavour)
                s.riffFlavour = i;
        return s;
    }

    int pc (int pitch) { return ((pitch % 12) + 12) % 12; }

    const ChordSpan& chordAt (const std::vector<ChordSpan>& spans, double ideaBeat)
    {
        const auto beat = ideaBeat + spans.front().start;
        for (const auto& span : spans)
            if (beat >= span.start - 1.0e-6 && beat < span.end - 1.0e-6)
                return span;
        return spans.back();
    }

    bool inChord (const Chord& chord, int pitch)
    {
        const auto pcs = chord.pitchClasses();
        return std::find (pcs.begin(), pcs.end(), pc (pitch)) != pcs.end();
    }
}

TEST_CASE ("0.89.0: with the guitar DRONING the bass holds the guitar's low strikes", "[composer][droning]")
{
    for (int variation = 0; variation < 6; ++variation)
    {
        auto s = stateWith (RiffFlavour::droning);
        s.lane (L::guitar).variation = variation;
        s.lane (L::drums).style = 2;   // (a chorus)
        const auto c = plainContext();
        const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
        REQUIRE_FALSE (spans.empty());
        const auto layers = composeLayers (s, spans, c);
        const auto& bass = layers[(size_t) L::bass];
        const auto& guitar = layers[(size_t) L::guitar];
        CHECK (bass.style == "Droning with the guitar");
        REQUIRE_FALSE (bass.notes.empty());
        auto lowest = 128;
        for (const auto& n : guitar.notes)
            if (! n.keyswitch)
                lowest = juce::jmin (lowest, n.pitch);
        auto held = 0.0;
        for (const auto& n : bass.notes)
        {
            // Every bass note starts with a low strike of the guitar.
            const auto with = std::any_of (guitar.notes.begin(), guitar.notes.end(), [&n, lowest] (const LineNote& g)
                                           { return ! g.keyswitch && g.pitch <= lowest + 11 && std::abs (g.beat - n.beat) < 0.02; });
            CHECK (with);
            held += n.length;
        }
        // Rung, not picked: on average a beat and a half or more each.
        CHECK (held / (double) bass.notes.size() >= 1.5);
    }
}

TEST_CASE ("0.89.0: the keys' pulse is the key's fifth up high, evenly; the motif stays in the key and climbs", "[composer][droning]")
{
    auto s = stateWith (RiffFlavour::droning);
    const auto c = plainContext();
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    const auto key = spans.front().key;
    const auto length = spans.back().end - spans.front().start;

    s.lane (L::chords).style = 0;
    for (int variation = 0; variation < 2; ++variation)
    {
        s.lane (L::chords).variation = variation;
        const auto keys = composeLayers (s, spans, c)[(size_t) L::chords];
        REQUIRE (keys.notes.size() > 8);
        const auto pitch = keys.notes.front().pitch;
        CHECK (pc (pitch) == (key.pitchClass() + 7) % 12);
        CHECK (pitch >= 74);
        CHECK (pitch <= 85);
        const auto step = keys.notes[1].beat - keys.notes[0].beat;
        CHECK ((std::abs (step - 0.25) < 1.0e-6 || std::abs (step - 0.5) < 1.0e-6));
        for (size_t i = 0; i < keys.notes.size(); ++i)
        {
            CHECK (keys.notes[i].pitch == pitch);
            CHECK (std::abs (keys.notes[i].beat - (double) i * step) < 1.0e-6);
        }
        CHECK (keys.notes.back().beat + keys.notes.back().length <= length + 1.0e-6);
        CHECK (keys.notes.back().beat >= length - 0.5 - 1.0e-6);   // (to the end)
    }

    s.lane (L::chords).style = 1;
    for (int variation = 0; variation < 3; ++variation)
    {
        s.lane (L::chords).variation = variation;
        const auto keys = composeLayers (s, spans, c)[(size_t) L::chords];
        REQUIRE (keys.notes.size() > 8);
        const auto scale = key.scale();
        for (const auto& n : keys.notes)
            CHECK (std::find (scale.begin(), scale.end(), pc (n.pitch)) != scale.end());
        // The fourth bar climbs: its notes rise.
        std::vector<int> bar4;
        for (const auto& n : keys.notes)
            if (n.beat >= 3 * c.beatsPerBar - 1.0e-6 && n.beat < 4 * c.beatsPerBar - 1.0e-6)
                bar4.push_back (n.pitch);
        REQUIRE (bar4.size() >= 3);
        CHECK (bar4[1] > bar4[0]);
        CHECK (bar4[2] > bar4[1]);
    }
}

TEST_CASE ("0.89.0: the lead shimmers in octaves on each chord's third (or root)", "[composer][droning]")
{
    auto s = stateWith (RiffFlavour::droning);
    const auto c = plainContext();
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    for (int style = 0; style < 2; ++style)
    {
        s.lane (L::lead).style = style;
        const auto lead = composeLayers (s, spans, c)[(size_t) L::lead];
        REQUIRE (lead.notes.size() > 16);
        CHECK (lead.style.startsWith ("Droning"));
        auto lowerOfPair = 0, pairs = 0;
        for (const auto& n : lead.notes)
        {
            const auto& span = chordAt (spans, n.beat);
            CHECK ((inChord (span.chord, n.pitch) || pc (n.pitch) == span.key.pitchClass()));
            if (std::any_of (lead.notes.begin(), lead.notes.end(), [&n] (const LineNote& m) { return std::abs (m.beat - n.beat) < 1.0e-6 && m.pitch == n.pitch + 12; }))
            {
                ++pairs;
                const auto pcs = span.chord.pitchClasses();
                const auto wanted = style == 1 ? span.chord.root : pcs[1];
                if (pc (n.pitch) == wanted)
                    ++lowerOfPair;
            }
        }
        CHECK (pairs > 8);
        CHECK (lowerOfPair >= pairs / 2);
    }
}

TEST_CASE ("0.89.0: the droning beats: open hats with kick 1 and 3, or half time with the snare on 3", "[composer][droning]")
{
    auto s = stateWith (RiffFlavour::droning);
    const auto c = plainContext();
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    const auto bpb = c.beatsPerBar;
    auto has = [] (const LineIdea& idea, double beat, int note)
    {
        return std::any_of (idea.notes.begin(), idea.notes.end(), [&] (const LineNote& n) { return n.pitch == note && std::abs (n.beat - beat) < 1.0e-6; });
    };

    s.lane (L::drums).variation = 0;
    const auto open = composeLayers (s, spans, c)[(size_t) L::drums];
    CHECK (open.style == "Droning: open hats");
    for (int bar = 0; bar < 8; ++bar)
    {
        const auto o = bar * bpb;
        for (int e = 0; e < 8; ++e)
            CHECK (has (open, o + e * 0.5, 46));
        CHECK (has (open, o, 36));
        CHECK (has (open, o + 2, 36));
        CHECK (has (open, o + 1, 38));
        CHECK (has (open, o + 2.5, 38));
        CHECK (has (open, o, 49) == (bar % 4 == 0));
        CHECK (has (open, o + 3, 49) == (bar % 4 == 3));
    }

    s.lane (L::drums).variation = 1;
    const auto half = composeLayers (s, spans, c)[(size_t) L::drums];
    CHECK (half.style == "Droning: half time");
    for (int bar = 0; bar < 8; ++bar)
    {
        const auto o = bar * bpb;
        CHECK (has (half, o + 2, 38));
        CHECK_FALSE (has (half, o + 1, 38));
        for (int q = 0; q < 4; ++q)
            CHECK (has (half, o + q, 42));
    }
}

TEST_CASE ("0.89.0: the droning band only with the guitar DRONING, and SYNC wins over it", "[composer][droning]")
{
    const auto c = plainContext();
    {
        auto s = stateWith (RiffFlavour::chugs);
        const auto layers = composeLayers (s, composerSpans (s, 0.0, c.beatsPerBar), c);
        for (const auto l : { L::bass, L::chords, L::lead, L::drums })
            CHECK_FALSE (layers[(size_t) l].style.startsWith ("Droning"));
    }
    {
        auto s = stateWith (RiffFlavour::droning);
        s.writtenRiffs = true;
        const auto layers = composeLayers (s, composerSpans (s, 0.0, c.beatsPerBar), c);
        CHECK (layers[(size_t) L::bass].style != "Droning with the guitar");
    }
    {
        auto s = stateWith (RiffFlavour::droning);
        s.lane (L::guitar).on = false;
        const auto layers = composeLayers (s, composerSpans (s, 0.0, c.beatsPerBar), c);
        CHECK (layers[(size_t) L::bass].style != "Droning with the guitar");
    }
    {
        auto s = stateWith (RiffFlavour::droning);
        s.syncLane = (int) L::guitar;
        const auto layers = composeLayers (s, composerSpans (s, 0.0, c.beatsPerBar), c);
        CHECK (layers[(size_t) L::bass].style.startsWith ("Synced"));
        CHECK (layers[(size_t) L::chords].style.startsWith ("Stabs"));
    }
}

TEST_CASE ("0.89.0: SYNC's stabs and doubled bass take the chord where the section is, not from the song's start", "[composer][sync]")
{
    auto s = stateWith (RiffFlavour::chugs);
    s.syncLane = (int) L::guitar;
    s.lane (L::bass).doubleSync = true;
    const auto c = plainContext();
    // A section starting at bar 5 (beat 16): the old lookup read the chords as if it started at 0.
    const auto spans = composerSpans (s, 16.0, c.beatsPerBar);
    REQUIRE (spans.front().start == 16.0);
    const auto layers = composeLayers (s, spans, c);
    const auto& stabs = layers[(size_t) L::chords];
    REQUIRE_FALSE (stabs.notes.empty());
    for (const auto& n : stabs.notes)
        CHECK (inChord (chordAt (spans, n.beat).chord, n.pitch));
}

namespace
{
    std::vector<double> guitarStrikes (const LineIdea& guitar)
    {
        auto lowest = 128;
        for (const auto& n : guitar.notes)
            if (! n.keyswitch)
                lowest = juce::jmin (lowest, n.pitch);
        std::vector<double> out;
        for (const auto& n : guitar.notes)
            if (! n.keyswitch && n.pitch <= lowest + 11 && (out.empty() || n.beat - out.back() > 0.05))
                out.push_back (n.beat);
        return out;
    }
}

TEST_CASE ("0.89.1: Circle With Me's shapes: a held third a chord, a muted pedal in threes, drums on the pushes, the bass's loop-end pulse", "[composer][droning]")
{
    const auto c = plainContext();
    for (int guitarTake = 0; guitarTake < 4; ++guitarTake)
    {
        auto s = stateWith (RiffFlavour::droning);
        s.lane (L::guitar).variation = guitarTake;
        s.lane (L::drums).style = 2;
        s.lane (L::chords).style = 2;
        s.lane (L::lead).style = 2;
        s.lane (L::drums).variation = 2;
        s.lane (L::bass).variation = 1;
        const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
        const auto bpb = c.beatsPerBar;
        const auto layers = composeLayers (s, spans, c);
        const auto strikes = guitarStrikes (layers[(size_t) L::guitar]);
        REQUIRE_FALSE (strikes.empty());

        // Keys: a note starting with every strike; the held ones (over a beat) are chord tones of a chord there.
        const auto& keys = layers[(size_t) L::chords];
        CHECK (keys.style == "Droning: a held note a chord");
        for (const auto at : strikes)
            CHECK (std::any_of (keys.notes.begin(), keys.notes.end(), [at] (const LineNote& n) { return std::abs (n.beat - at) < 1.0e-6; }));
        for (const auto& n : keys.notes)
        {
            CHECK (n.pitch >= 64);
            CHECK (n.pitch <= 86);
            if (n.length > 1.0)
                CHECK ((inChord (chordAt (spans, n.beat).chord, n.pitch) || inChord (chordAt (spans, n.beat + 1.0).chord, n.pitch)));
        }

        // Lead: palm-muted eighths, three notes cycling (root, octave, fifth of the key).
        const auto& lead = layers[(size_t) L::lead];
        CHECK (lead.style == "Droning: a muted pedal in threes");
        REQUIRE (lead.notes.size() >= 64);
        const auto root = lead.notes.front().pitch;
        CHECK (pc (root) == spans.front().key.pitchClass());
        for (size_t i = 0; i < lead.notes.size(); ++i)
        {
            const auto& n = lead.notes[i];
            CHECK (std::abs (n.beat - (double) i * 0.5) < 1.0e-6);
            CHECK (GuitarVelocity::midiOf (n.velocity) < 60);
            const auto expected = std::array<int, 3> { root, root + 12, root + 7 }[(size_t) (i % 3)];
            CHECK ((n.pitch == expected || n.pitch == expected - 2));
        }

        // Drums: the snare only on the downbeat of every second bar; a kick on every push.
        const auto& drums = layers[(size_t) L::drums];
        CHECK (drums.style == "Droning: locked to the pushes");
        for (const auto& n : drums.notes)
            if (n.pitch == 38)
            {
                const auto bar = (int) std::floor (n.beat / bpb + 1.0e-6);
                CHECK (std::abs (n.beat - bar * bpb) < 1.0e-6);
                CHECK (bar % 2 == 1);
            }
        for (const auto at : strikes)
            if (std::fmod (at, bpb) > 1.0e-6)
                CHECK (std::any_of (drums.notes.begin(), drums.notes.end(), [at] (const LineNote& n) { return n.pitch == 36 && std::abs (n.beat - at) < 1.0e-6; }));

        // Bass: the eighth bar pulsed in eighths, the rest held.
        const auto& bass = layers[(size_t) L::bass];
        auto inBar8 = 0;
        for (const auto& n : bass.notes)
            if (n.beat >= 7 * bpb - 1.0e-6 && n.beat < 8 * bpb - 1.0e-6)
            {
                ++inBar8;
                CHECK (n.length <= 1.0);
            }
        CHECK (inBar8 >= 6);
        auto heldElsewhere = 0.0;
        auto countElsewhere = 0;
        for (const auto& n : bass.notes)
            if (n.beat < 7 * bpb - 1.0e-6)
            {
                heldElsewhere += n.length;
                ++countElsewhere;
            }
        REQUIRE (countElsewhere > 0);
        CHECK (heldElsewhere / countElsewhere >= 1.5);
    }
}
