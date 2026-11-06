#include <catch2/catch_test_macros.hpp>

#include "engine/Studio.h"
#include "engine/Takes.h"

using namespace pr7s;

// 0.64.0: GROOVE riffs (low chugs answered by high dissonant stabs) and their bends and vibrato as controllers.
namespace
{
    using T = Chord::Type;
    using X = Chord::Tension;
    using E = LineNote::Effect;

    std::vector<ChordSpan> amBars (int count)
    {
        const Key am { 5, 0, true };
        std::vector<ChordSpan> spans;
        for (int i = 0; i < count; ++i)
            spans.push_back ({ i * 4.0, i * 4.0 + 4.0, { 9, T::minor, X::none, -1 }, am });
        return spans;
    }

    const LineIdea& named (const std::vector<LineIdea>& ideas, const juce::String& name)
    {
        for (const auto& i : ideas)
            if (i.style == name)
                return i;
        FAIL ("no idea named " << name);
        return ideas.front();
    }

    std::vector<const LineNote*> at (const LineIdea& idea, double beat)
    {
        std::vector<const LineNote*> out;
        for (const auto& n : idea.notes)
            if (std::abs (n.beat - beat) < 1.0e-6)
                out.push_back (&n);
        return out;
    }

    int midi (const LineNote& n) { return GuitarVelocity::midiOf (n.velocity); }
}

TEST_CASE ("0.64.0: GROOVE writes chugs answered by high stabs, two octaves over the low string", "[lines][riffs][groove]")
{
    CHECK (guitarStyleNamed ("groove") == GuitarStyle::groove);
    CHECK (guitarStyleName (GuitarStyle::groove) == "GROOVE");
    const auto spans = amBars (4);
    const auto& dropA = GuitarTuning::all()[0];   // 7-string drop A: the low A is 33
    REQUIRE (dropA.lowest() == 33);
    RiffSettings two;
    two.bars = 2;
    const auto ideas = guitarRhythmIdeas (spans, 4.0, 0, dropA, {}, GuitarStyle::groove, two);
    REQUIRE (ideas.size() == 6);

    // The tritone stab (as Ten Ton Hammer's intro on the owner's guitar): A2 and D#3 together with vibrato, then chugs on the low A.
    const auto& tritone = named (ideas, "Tritone stab");
    const auto first = at (tritone, 0.0);
    REQUIRE (first.size() == 2);
    CHECK (first[0]->pitch + first[1]->pitch == 57 + 63);
    for (const auto* n : first)
    {
        CHECK (n->effect == E::vibrato);
        CHECK (midi (*n) >= 60);
        CHECK (midi (*n) <= 119);
    }
    const auto chug = at (tritone, 1.5);
    REQUIRE (chug.size() == 1);
    CHECK (chug[0]->pitch == 33);
    CHECK (midi (*chug[0]) <= 59);

    // The bent stab (as Brackish's bridge): a chug, then G2 and A2 bent up a whole step on beat two, the second bar ending on a ringing B2.
    const auto& bent = named (ideas, "Bent stab");
    const auto stab = at (bent, 1.0);
    REQUIRE (stab.size() == 2);
    CHECK (stab[0]->pitch + stab[1]->pitch == 55 + 57);
    CHECK (stab[0]->effect == E::bend);
    const auto unison = at (bent, 7.0);
    REQUIRE (unison.size() == 1);
    CHECK (unison[0]->pitch == 59);
    CHECK (unison[0]->effect == E::ring);
}

TEST_CASE ("0.64.0: RIFF SETTINGS and the composer's takes leave the stabs as written", "[lines][riffs][groove]")
{
    const auto spans = amBars (4);
    const auto& dropA = GuitarTuning::all()[0];
    RiffSettings shaped;
    shaped.palmMutes = 1.0f;   // every open hit off the beat muted, for the others
    shaped.busyness = 0.0f;    // weak hits left out, for the others
    const auto ideas = guitarRhythmIdeas (spans, 4.0, 1, dropA, {}, GuitarStyle::groove, shaped);
    for (const auto& idea : ideas)
    {
        INFO (idea.style);
        auto stabs = 0;
        for (const auto& n : idea.notes)
            if (n.effect != E::none)
            {
                ++stabs;
                CHECK (midi (n) >= 60);   // never palm muted
                CHECK (n.pitch >= 55);    // still high
            }
        CHECK (stabs > 0);

        // A take on LOW STRING folds chords down and moves roots: never the stabs.
        for (juce::uint32 seed = 1; seed < 8; ++seed)
        {
            auto take = idea;
            TakeOptions o;
            o.seed = seed;
            o.lowString = true;
            o.lowest = 33;
            o.highest = 88;
            riffTake (take, spans, 4.0, o, GuitarStyle::groove);
            for (const auto& n : take.notes)
                if (n.effect != E::none)
                {
                    CHECK (midi (n) >= 60);
                    CHECK (n.pitch >= 55);
                }
        }
    }
}

TEST_CASE ("0.64.0: a stab's bend and vibrato become pitch bend and mod wheel in the part", "[lines][riffs][groove]")
{
    std::vector<LineNote> notes;
    notes.push_back ({ 1.0, 0.48, 55, 0.8f });
    notes.back().effect = E::bend;
    notes.push_back ({ 1.0, 0.48, 57, 0.8f });
    notes.back().effect = E::bend;
    notes.push_back ({ 2.0, 1.0, 57, 0.8f });
    notes.back().effect = E::vibrato;
    notes.push_back ({ 3.0, 0.5, 33, 0.3f });   // a chug: nothing

    const auto c = Studio::effectControllers (notes, 2);
    std::vector<Controller> bends, wheel;
    for (const auto& k : c)
        (k.type == Controller::Type::pitchbend ? bends : wheel).push_back (k);
    REQUIRE (bends.size() == 3);   // one bend for the two notes together
    CHECK (bends[0].start == 1.0);
    CHECK (bends[0].value == 8192);
    CHECK (bends[1].value == 16383);   // a whole step on a bend range of 2: all the way
    CHECK (bends[1].ramp);
    CHECK (bends[1].start > 1.0);
    CHECK (bends[2].value == 8192);    // back before the next chug
    CHECK (bends[2].start <= 1.48 + 1.0e-9);
    REQUIRE (wheel.size() == 4);
    CHECK (wheel[0].number == 1);
    CHECK (wheel[0].value == 0);
    CHECK (wheel[2].value > 0);
    CHECK (wheel[2].ramp);
    CHECK (wheel[3].value == 0);
    CHECK (wheel[3].start <= 3.0);
    CHECK (std::is_sorted (c.begin(), c.end(), [] (const Controller& a, const Controller& b) { return a.start < b.start; }));

    // A bend range of 12: a whole step is a sixth of the way.
    const auto wide = Studio::effectControllers (notes, 12);
    for (const auto& k : wide)
        if (k.type == Controller::Type::pitchbend && k.ramp)
            CHECK (k.value == 8192 + juce::roundToInt (8191.0 / 6.0));

    // Onto a track: the part has them, and keeps them through undo's copy.
    Studio studio (false);
    LineIdea idea { LineIdea::Part::rhythm, "Bent stab", "", notes };
    const auto spans = amBars (1);
    const auto trackId = studio.addLinePart (idea, spans);
    REQUIRE (trackId != 0);
    const auto* track = studio.getProject().findTrack (trackId);
    REQUIRE (track != nullptr);
    REQUIRE (track->parts.size() == 1);
    CHECK (track->parts[0].controllers.size() == 7);
}
