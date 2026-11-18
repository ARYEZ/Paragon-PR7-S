// Speed, round six (0.173.0): the changes give exactly what they gave before.

#include <catch2/catch_test_macros.hpp>

#include <set>

#include "engine/Composer.h"
#include "engine/Eq.h"
#include "engine/Melody.h"
#include "engine/SectionTypes.h"

using namespace pr7s;

namespace
{
    std::vector<ChordSpan> aMinorSpans()
    {
        Key aMinor;
        for (const auto& k : Key::all())
            if (k.minor && k.pitchClass() == 9)
                aMinor = k;
        const std::array<Chord, 4> chords { Chord { 9, Chord::Type::minor }, Chord { 5, Chord::Type::major },
                                            Chord { 0, Chord::Type::major }, Chord { 7, Chord::Type::major } };
        std::vector<ChordSpan> spans;
        for (int bar = 0; bar < 8; ++bar)
            spans.push_back ({ bar * 4.0, bar * 4.0 + 4.0, chords[(size_t) (bar % 4)], aMinor });
        return spans;
    }

    bool sameNotes (const LineIdea& a, const LineIdea& b)
    {
        if (a.notes.size() != b.notes.size())
            return false;
        for (size_t i = 0; i < a.notes.size(); ++i)
            if (a.notes[i].beat != b.notes[i].beat || a.notes[i].length != b.notes[i].length || a.notes[i].pitch != b.notes[i].pitch
                || a.notes[i].velocity != b.notes[i].velocity)
                return false;
        return true;
    }
}

TEST_CASE ("0.173.0: the EQ's two sides together match one side at a time for every number of stages", "[speed173]")
{
    // One stage alone (a 12 dB low cut), two, three, the bands' four and five and more.
    for (int bandsOn = 0; bandsOn <= 4; ++bandsOn)
        for (const auto cut : { false, true })
        {
            ChannelEq eq;
            for (int b = 0; b < bandsOn; ++b)
            {
                eq.bands[(size_t) b].on = true;
                eq.bands[(size_t) b].gainDb = 3.0 + b;
            }
            eq.lowCutOn = cut;
            eq.highCutOn = cut && bandsOn > 2;
            const auto design = designEq (eq, 44100.0);
            EqProcessor together, left, right;
            together.setDesign (design);
            left.setDesign (design);
            right.setDesign (design);
            juce::Random random (bandsOn * 2 + (cut ? 1 : 0));
            for (int block = 0; block < 4; ++block)
            {
                const auto n = block % 2 == 0 ? 512 : 300;   // (more than one 256-sample piece)
                std::vector<float> l ((size_t) n), r ((size_t) n);
                for (int i = 0; i < n; ++i)
                {
                    l[(size_t) i] = random.nextFloat() * 2.0f - 1.0f;
                    r[(size_t) i] = random.nextFloat() * 2.0f - 1.0f;
                }
                auto l1 = l, r1 = r;
                float* both[] = { l.data(), r.data() };
                together.processPre (both, 2, n);
                together.processEq (both, 2, n);
                float* one[] = { l1.data() };
                float* two[] = { r1.data() };
                left.processPre (one, 1, n);
                left.processEq (one, 1, n);
                right.processPre (two, 1, n);
                right.processEq (two, 1, n);
                INFO (bandsOn << " bands, cut " << (int) cut << ", block " << block);
                CHECK (l == l1);
                CHECK (r == r1);
            }
        }
}

TEST_CASE ("0.173.0: the melody lane composes only its own form, the same melody as before", "[speed173]")
{
    const auto spans = aMinorSpans();
    const ComposerContext c;
    for (int style = 0; style < 6; ++style)
        for (const auto variation : { 0, 3, 17 })
        {
            ComposerState s;
            auto& lane = s.lane (ComposerLayer::melody);
            lane.on = true;
            lane.style = style;
            lane.variation = variation;
            auto melody = c.melody;
            melody.density = lane.busy * 0.9f;
            const auto all = composedMelodies (spans, c.beatsPerBar, melody, variation);
            const auto choices = composerChoices (ComposerLayer::melody, s, spans, c);
            REQUIRE ((int) choices.size() > style);
            INFO ("style " << style << " variation " << variation);
            CHECK (sameNotes (choices[(size_t) style], all[(size_t) style]));
        }
}

TEST_CASE ("0.173.0: section types looked up from the cache are the lists as built", "[speed173]")
{
    for (const auto* name : { "Intro", "Verse", "Pre-Chorus", "Chorus", "Post-Chorus", "Interlude", "Bridge", "Breakdown", "Outro" })
    {
        const auto first = sectionTypesFor (name), again = sectionTypesFor (name);
        REQUIRE (first.size() == again.size());
        REQUIRE (! first.empty());
        for (size_t i = 0; i < first.size(); ++i)
        {
            CHECK (first[i].name == again[i].name);
            const auto called = sectionTypeCalled (first[i].name);
            REQUIRE (called.has_value());
            CHECK (called->name == first[i].name);
        }
    }
    CHECK_FALSE (sectionTypeCalled ("NO SUCH TYPE").has_value());
    CHECK_FALSE (sectionTypeCalled ("").has_value());
}
