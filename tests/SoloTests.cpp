#include <catch2/catch_test_macros.hpp>

#include <set>

#include "engine/SectionPlan.h"
#include "engine/Composer.h"

using namespace pr7s;

// 0.65.0: the SOLO section (the owner asked): a whole guitar solo, the drums on the ride, offered after the choruses.
namespace
{
    using T = Chord::Type;
    using X = Chord::Tension;

    std::vector<ChordSpan> amBars (int count)
    {
        const Key am { 5, 0, true };
        const Chord chords[] = { { 9, T::minor, X::none, -1 }, { 5, T::major, X::none, -1 }, { 0, T::major, X::none, -1 }, { 4, T::major, X::none, -1 } };
        std::vector<ChordSpan> spans;
        for (int i = 0; i < count; ++i)
            spans.push_back ({ i * 4.0, i * 4.0 + 4.0, chords[i % 4], am });
        return spans;
    }

    int onsetsIn (const LineIdea& idea, double from, double to)
    {
        std::set<long> at;
        for (const auto& n : idea.notes)
            if (n.beat >= from - 1.0e-9 && n.beat < to - 1.0e-9)
                at.insert (std::lround (n.beat * 96.0));
        return (int) at.size();
    }
}

TEST_CASE ("0.65.0: every style's leads end with a whole solo that builds up, with bends and vibrato", "[lines][riffs][solo]")
{
    const auto spans = amBars (8);
    const auto& tuning = GuitarTuning::all()[0];
    for (const auto style : guitarStyles())
        for (int variation = 0; variation < 3; ++variation)
        {
            INFO (guitarStyleName (style) << " variation " << variation);
            const auto leads = guitarLeadIdeas (spans, 4.0, variation, tuning, {}, style);
            REQUIRE (leads.size() == 7);
            const auto& solo = leads.back();
            CHECK (solo.style.startsWith ("Solo: "));   // (0.116.0: BY STYLE names the style it plays)
            CHECK (solo.part == LineIdea::Part::lead);
            REQUIRE_FALSE (solo.notes.empty());
            CHECK (std::is_sorted (solo.notes.begin(), solo.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; }));
            // It builds: its busiest two bars after the opening ones (0.116.0: the last bars calm again, as real solos).
            auto busiest = 0;
            for (auto from = 8.0; from <= 24.0; from += 4.0)
                busiest = juce::jmax (busiest, onsetsIn (solo, from, from + 8.0));
            CHECK (busiest >= onsetsIn (solo, 0.0, 8.0));
            auto bends = 0, vibratos = 0;
            for (const auto& n : solo.notes)
            {
                CHECK (n.pitch >= tuning.lowest());
                CHECK (n.pitch <= tuning.highest());
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
                bends += n.effect == LineNote::Effect::bend ? 1 : 0;
                vibratos += n.effect == LineNote::Effect::vibrato ? 1 : 0;
                if (n.effect != LineNote::Effect::none)
                {
                    const auto m = GuitarVelocity::midiOf (n.velocity);
                    CHECK (m >= 60);    // never a palm mute, a pinch or a rake
                    CHECK (m <= 119);
                }
            }
            CHECK (bends + vibratos > 0);
            // The last note rings to the end.
            CHECK (solo.notes.back().beat + solo.notes.back().length > 31.0);
        }
    // The same deal gives the same solo.
    CHECK (guitarLeadIdeas (spans, 4.0, 1, tuning, {}, GuitarStyle::groove).back().notes
           == guitarLeadIdeas (spans, 4.0, 1, tuning, {}, GuitarStyle::groove).back().notes);
}

TEST_CASE ("0.65.0: SOLO is a section: its drums ride, the composer and the suggestions offer it", "[solo][drums]")
{
    CHECK (drumSections().back() == DrumSection::solo);
    CHECK (drumSectionName (DrumSection::solo) == "SOLO");
    const auto spans = amBars (4);
    for (const auto style : guitarStyles())
    {
        const auto beat = generateBeat (style, DrumSection::solo, spans, 4.0, {}, 3);
        auto rides = 0, hats = 0;
        for (const auto& n : beat.notes)
        {
            rides += n.pitch == GmDrum::ride ? 1 : 0;
            hats += n.pitch == GmDrum::closedHat ? 1 : 0;
        }
        INFO (guitarStyleName (style));
        CHECK (rides > hats);
    }
    CHECK (generatedBeats (GuitarStyle::groove, spans, 4.0, {}, 0).size() == drumSections().size());

    CHECK (composerStyleCount (ComposerLayer::lead) == 7);
    CHECK (composerStyleCount (ComposerLayer::drums) == 7);

    // After the second chorus the NEXT list offers the solo; after it, the chorus comes back.
    std::vector<BuiltSection> song { { DrumSection::verse, 0, 16 }, { DrumSection::chorus, 16, 32 }, { DrumSection::verse, 32, 48 }, { DrumSection::chorus, 48, 64 } };
    const auto has = [] (const std::vector<SectionSuggestion>& list, DrumSection kind)
    {
        return std::any_of (list.begin(), list.end(), [kind] (const SectionSuggestion& s) { return s.kind == kind; });
    };
    CHECK (has (suggestNextSections (song, GuitarStyle::groove, 6), DrumSection::solo));
    song.push_back ({ DrumSection::solo, 64, 80 });
    const auto after = suggestNextSections (song, GuitarStyle::groove, 6);
    REQUIRE_FALSE (after.empty());
    CHECK (after.front().kind == DrumSection::chorus);
    CHECK_FALSE (has (after, DrumSection::solo));   // (one solo a song)
}
