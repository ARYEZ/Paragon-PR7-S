#include <catch2/catch_test_macros.hpp>

#include <set>

#include "engine/SoloEngine.h"
#include "engine/Instruments.h"
#include "engine/Studio.h"

using namespace pr7s;

// 0.66.0: solo styles (the owner asked: solos in the manner of players they love; our own names in the app).
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

    bool sameNotes (const LineIdea& a, const LineIdea& b)
    {
        if (a.notes.size() != b.notes.size())
            return false;
        for (size_t i = 0; i < a.notes.size(); ++i)
            if (a.notes[i].beat != b.notes[i].beat || a.notes[i].pitch != b.notes[i].pitch || a.notes[i].effect != b.notes[i].effect)
                return false;
        return true;
    }
}

TEST_CASE ("0.66.0: solo style names are ours, round trip, and never a player's", "[solo][solostyles]")
{
    REQUIRE (soloStyles().size() == 9);
    CHECK (soloStyles().front() == SoloStyle::automatic);
    for (const auto s : soloStyles())
    {
        CHECK (soloStyleNamed (soloStyleName (s)) == s);
        CHECK (soloStyleDescription (s).isNotEmpty());
        for (const auto* player : { "Gilmour", "Perry", "Van Halen", "Page", "EVH", "Zeppelin", "Floyd", "Aerosmith",
                                   "Dimebag", "Darrell", "Pantera", "Hammett", "Metallica", "Wylde", "Black Label", "Petrucci", "Dream Theater" })
        {
            CHECK_FALSE (soloStyleName (s).containsIgnoreCase (player));
            CHECK_FALSE (soloStyleDescription (s).containsIgnoreCase (player));
        }
    }
    CHECK (soloStyleNamed ("nothing like it") == SoloStyle::automatic);
}

TEST_CASE ("0.66.0: every solo style writes a whole solo that builds and ends on the root", "[solo][solostyles]")
{
    const auto spans = amBars (8);
    const auto& tuning = GuitarTuning::all()[0];
    for (const auto style : soloStyles())
    {
        if (style == SoloStyle::automatic)
            continue;
        for (int variation = 0; variation < 4; ++variation)
        {
            INFO (soloStyleName (style) << " variation " << variation);
            const auto solo = composeSolo (style, spans, 4.0, tuning, {}, variation);
            REQUIRE_FALSE (solo.notes.empty());
            CHECK (solo.part == LineIdea::Part::lead);
            CHECK (solo.style.startsWith ("Solo: "));
            CHECK (std::is_sorted (solo.notes.begin(), solo.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; }));
            // Builds: the second half at least as busy as the first.
            CHECK (onsetsIn (solo, 16.0, 32.0) >= onsetsIn (solo, 0.0, 16.0));
            auto bends = 0, vibratos = 0;
            for (const auto& n : solo.notes)
            {
                CHECK (n.pitch >= tuning.lowest());
                CHECK (n.pitch <= tuning.highest());
                CHECK (n.beat >= 0.0);
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
                CHECK (n.velocity > 0.0f);
                CHECK_FALSE (n.keyswitch);
                bends += n.effect == LineNote::Effect::bend ? 1 : 0;
                vibratos += n.effect == LineNote::Effect::vibrato ? 1 : 0;
            }
            CHECK (bends + vibratos > 0);
            // The last note: the last chord's root (E), to the end.
            auto last = solo.notes.front();
            for (const auto& n : solo.notes)
                if (n.beat >= last.beat)
                    last = n;
            CHECK (((last.pitch % 12) + 12) % 12 == 4);
            CHECK (last.beat + last.length >= 32.0 - 1.0e-6);
            // Repeatable, and another variation is another solo.
            CHECK (sameNotes (solo, composeSolo (style, spans, 4.0, tuning, {}, variation)));
            CHECK_FALSE (sameNotes (solo, composeSolo (style, spans, 4.0, tuning, {}, variation + 100)));
        }
    }
}

TEST_CASE ("0.66.0: each solo style has its own habits", "[solo][solostyles]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    const auto count = [] (const LineIdea& idea, auto&& test)
    {
        auto c = 0;
        for (const auto& n : idea.notes)
            c += test (n) ? 1 : 0;
        return c;
    };
    for (int variation = 0; variation < 3; ++variation)
    {
        INFO ("variation " << variation);
        const auto tap = composeSolo (SoloStyle::tapAndDive, spans, 4.0, tuning, {}, variation);
        CHECK (count (tap, [] (const LineNote& n) { return n.technique == LineNote::Technique::tapping; }) >= 6);
        CHECK (count (tap, [] (const LineNote& n) { return n.effect == LineNote::Effect::dive; }) == 1);
        CHECK (tap.notes.back().velocity == GuitarVelocity::of (127));   // (the pinch harmonic into the dive)

        // Soaring: sparser than the others at the start.
        const auto soar = composeSolo (SoloStyle::soaringBends, spans, 4.0, tuning, {}, variation);
        const auto blues = composeSolo (SoloStyle::bluesHammer, spans, 4.0, tuning, {}, variation);
        CHECK (onsetsIn (soar, 0.0, 16.0) < onsetsIn (blues, 0.0, 16.0));
        // No dives but in Tap & Dive.
        CHECK (count (soar, [] (const LineNote& n) { return n.effect == LineNote::Effect::dive; }) == 0);
        CHECK (count (blues, [] (const LineNote& n) { return n.effect == LineNote::Effect::dive; }) == 0);
        // Big bends (over a whole step) only after the start.
        for (const auto* solo : { &soar, &blues, &tap })
            for (const auto& n : solo->notes)
                if (n.effect == LineNote::Effect::bend && n.amount > 2.0f)
                    CHECK (n.beat > 64.0 * 0.3);
    }
}

TEST_CASE ("0.66.0: solos keep to the instrument's shortest note", "[solo][solostyles]")
{
    const auto spans = amBars (8);
    const auto& tuning = GuitarTuning::all()[0];
    for (const auto style : soloStyles())
    {
        if (style == SoloStyle::automatic)
            continue;
        const auto solo = composeSolo (style, spans, 4.0, tuning, {}, 1, 0.25);
        std::vector<double> starts;
        for (const auto& n : solo.notes)
            if (starts.empty() || n.beat - starts.back() > HumanizeLimits::together)
                starts.push_back (n.beat);
        for (size_t i = 1; i < starts.size(); ++i)
        {
            INFO (soloStyleName (style) << " at " << starts[i]);
            CHECK (starts[i] - starts[i - 1] >= 0.25 - 0.04);
        }
    }
}

TEST_CASE ("0.66.0: SOLO STYLE picks the leads' seventh", "[solo][solostyles]")
{
    const auto spans = amBars (8);
    const auto& tuning = GuitarTuning::all()[0];
    const auto byStyle = guitarLeadIdeas (spans, 4.0, 0, tuning, {}, GuitarStyle::shred);
    const auto tapped = guitarLeadIdeas (spans, 4.0, 0, tuning, {}, GuitarStyle::shred, SoloStyle::tapAndDive);
    REQUIRE (byStyle.size() == 7);
    REQUIRE (tapped.size() == 7);
    CHECK (byStyle.back().style == "Solo");
    CHECK (tapped.back().style == "Solo: Tap & Dive");
    for (size_t i = 0; i < 6; ++i)
        CHECK (sameNotes (byStyle[i], tapped[i]));

    Studio studio (false);
    CHECK (studio.getSoloStyle() == SoloStyle::automatic);
    studio.setSoloStyle (SoloStyle::bluesHammer);
    CHECK (studio.getSoloStyle() == SoloStyle::bluesHammer);
    CHECK (studio.getComposerContext (0.0).solo == SoloStyle::bluesHammer);
}

TEST_CASE ("0.66.0: tapping, tremolo and harmonics press their keys, plain notes Sustain", "[solo][solostyles][instruments]")
{
    const auto jupiter = InstrumentProfile::shreddage ("Jupiter");
    const auto keys = ArticulationKeys::of (jupiter);
    CHECK (keys.single == 12);
    CHECK (keys.power == 13);
    CHECK (keys.tremolo == 18);
    CHECK (keys.harmonics == 20);
    CHECK (keys.tapping == 21);

    LineIdea idea { LineIdea::Part::lead, "test", {}, {} };
    const auto add = [&idea] (double beat, int pitch, LineNote::Technique t)
    {
        LineNote n;
        n.beat = beat;
        n.length = 0.25;
        n.pitch = pitch;
        n.velocity = 0.75f;
        n.technique = t;
        idea.notes.push_back (n);
    };
    add (0.0, 64, LineNote::Technique::normal);
    add (1.0, 76, LineNote::Technique::tapping);
    add (1.5, 64, LineNote::Technique::tapping);
    add (2.0, 66, LineNote::Technique::normal);
    add (3.0, 69, LineNote::Technique::harmonic);
    useArticulationKeys (idea, keys);
    std::vector<std::pair<double, int>> pressed;
    for (const auto& n : idea.notes)
        if (n.keyswitch)
            pressed.push_back ({ n.beat, n.pitch });
    const std::vector<std::pair<double, int>> expected { { 0.0, 12 }, { 1.0, 21 }, { 2.0, 12 }, { 3.0, 20 } };
    CHECK (pressed == expected);
}

TEST_CASE ("0.66.0: bends by their size, dives down by the bend range", "[solo][solostyles]")
{
    std::vector<LineNote> notes (3);
    notes[0].beat = 0.0; notes[0].length = 1.0; notes[0].pitch = 60; notes[0].effect = LineNote::Effect::bend; notes[0].amount = 1.0f;
    notes[1].beat = 2.0; notes[1].length = 2.0; notes[1].pitch = 60; notes[1].effect = LineNote::Effect::dive; notes[1].amount = 12.0f;
    notes[2].beat = 5.0; notes[2].length = 1.0; notes[2].pitch = 60; notes[2].effect = LineNote::Effect::vibrato; notes[2].amount = 0.5f;
    const auto c = Studio::effectControllers (notes, 12);
    std::vector<Controller> bends, wheel;
    for (const auto& k : c)
        (k.type == Controller::Type::pitchbend ? bends : wheel).push_back (k);
    REQUIRE (bends.size() == 7);
    CHECK (bends[1].value == 8192 + juce::roundToInt (8191.0 / 12.0));   // a half step on a 12-semitone range
    CHECK (bends[3].value == 8192);                                        // the dive starts level,
    CHECK (bends[4].value == 8192);                                        // holds for a quarter,
    CHECK (bends[4].start == 2.0 + 2.0 * 0.25);
    CHECK (bends[5].value == 0);                                           // goes an octave down,
    CHECK (bends[5].start == 2.0 + 2.0 * 0.9);
    CHECK (bends[6].value == 8192);                                        // and comes back as the note ends
    REQUIRE (wheel.size() == 4);
    CHECK (wheel[2].value == 48);                                          // half the usual depth

    // A bend past a 2-semitone range: written higher, bent only a whole step.
    std::vector<LineNote> big (1);
    big[0].pitch = 60; big[0].length = 1.0; big[0].effect = LineNote::Effect::bend; big[0].amount = 4.0f;
    Studio::fitBends (big, 2);
    CHECK (big[0].pitch == 62);
    CHECK (big[0].amount == 2.0f);
}

TEST_CASE ("0.67.0: round two's styles have their own habits", "[solo][solostyles]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    const auto count = [] (const LineIdea& idea, auto&& test)
    {
        auto c = 0;
        for (const auto& n : idea.notes)
            c += test (n) ? 1 : 0;
        return c;
    };
    const auto pinch = GuitarVelocity::of (127);
    for (int variation = 0; variation < 3; ++variation)
    {
        INFO ("variation " << variation);
        // SCREAM & DIVE: a harmonic bent up a minor third before a dive at the end; licks shifted a fret at a time.
        const auto scream = composeSolo (SoloStyle::screamAndDive, spans, 4.0, tuning, {}, variation);
        CHECK (count (scream, [] (const LineNote& n) { return n.technique == LineNote::Technique::harmonic
                                                             && n.effect == LineNote::Effect::bend && n.amount == 3.0f; }) == 1);
        CHECK (count (scream, [] (const LineNote& n) { return n.effect == LineNote::Effect::dive; }) == 1);
        auto climbs = 0;   // four 16ths a semitone over the four before
        for (size_t i = 4; i < scream.notes.size(); ++i)
            climbs += scream.notes[i].pitch == scream.notes[i - 4].pitch + 1
                      && std::abs (scream.notes[i].beat - scream.notes[i - 4].beat - 1.0) < 1.0e-6 ? 1 : 0;
        CHECK (climbs >= 8);

        // THRASH FIRE: in on an upbeat, ends high.
        const auto thrash = composeSolo (SoloStyle::thrashFire, spans, 4.0, tuning, {}, variation);
        CHECK (thrash.notes.front().beat >= 0.5 - 1.0e-9);
        CHECK (thrash.notes.back().pitch >= tuning.strings.back() + 10);

        // PINCH SQUEAL: pinch harmonics shaken wide, at most one every two bars.
        const auto squeal = composeSolo (SoloStyle::pinchSqueal, spans, 4.0, tuning, {}, variation);
        std::vector<double> pinches;
        for (const auto& n : squeal.notes)
            if (n.velocity == pinch && n.effect == LineNote::Effect::vibrato && n.amount > 1.5f)
                pinches.push_back (n.beat);
        CHECK (pinches.size() >= 2);
        for (size_t i = 1; i < pinches.size(); ++i)
            CHECK (pinches[i] - pinches[i - 1] >= 4.0);   // (different bars two apart at least: a bar is 4 beats here)

        // PROG PRECISION: natural minor's 2 and b6 are heard (B and F in A minor), and long 16th runs.
        const auto prog = composeSolo (SoloStyle::progPrecision, spans, 4.0, tuning, {}, variation);
        CHECK (count (prog, [] (const LineNote& n) { return ((n.pitch % 12) + 12) % 12 == 11; }) > 0);
        auto run = 0, longest = 0;
        for (size_t i = 1; i < prog.notes.size(); ++i)
        {
            run = std::abs (prog.notes[i].beat - prog.notes[i - 1].beat - 0.25) < 1.0e-6 ? run + 1 : 0;
            longest = juce::jmax (longest, run);
        }
        CHECK (longest >= 12);
    }
}
