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
    REQUIRE (soloStyles().size() == 27);   // (0.116.0: ten more)
    CHECK (soloStyles().front() == SoloStyle::automatic);
    for (const auto s : soloStyles())
    {
        CHECK (soloStyleNamed (soloStyleName (s)) == s);
        CHECK (soloStyleDescription (s).isNotEmpty());
        for (const auto* player : { "Gilmour", "Perry", "Van Halen", "Page", "EVH", "Zeppelin", "Floyd", "Aerosmith",
                                   "Dimebag", "Darrell", "Pantera", "Hammett", "Metallica", "Wylde", "Black Label", "Petrucci", "Dream Theater",
                                   "DeVille", "Poison", "Mick Mars", "Motley", "Slash", "Guns N", "Prince",
                                   "Synyster", "Avenged", "Lowery", "Sevendust", "Cantrell", "Alice in Chains", "Cobain", "Nirvana" })
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
            // Builds: its busiest two bars past the first two (0.116.0: a peak past halfway, then calm, as real solos).
            auto busiest = 0;
            for (auto from = 8.0; from <= 24.0; from += 4.0)
                busiest = juce::jmax (busiest, onsetsIn (solo, from, from + 8.0));
            if (style != SoloStyle::shuffleConversation && style != SoloStyle::grungeMelody && style != SoloStyle::fallingMajorMelody)   // (no busy waves: flat on purpose)
                CHECK (busiest >= onsetsIn (solo, 0.0, 8.0));
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
    CHECK (byStyle.back().style == "Solo: Two Speeds");   // (0.116.0: BY STYLE is the guitar STYLE's solo style)
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

TEST_CASE ("0.68.0: round three's styles have their own habits", "[solo][solostyles]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    const auto longestRun = [] (const LineIdea& idea, double from, double to)
    {
        auto run = 0, longest = 0;
        for (size_t i = 1; i < idea.notes.size(); ++i)
        {
            if (idea.notes[i].beat < from || idea.notes[i].beat >= to)
                continue;
            run = std::abs (idea.notes[i].beat - idea.notes[i - 1].beat - 0.25) < 1.0e-6 ? run + 1 : 0;
            longest = juce::jmax (longest, run);
        }
        return longest;
    };
    auto thirds = 0, dips = 0;
    for (int variation = 0; variation < 4; ++variation)
    {
        INFO ("variation " << variation);
        // GLAM FLASH: no fast run in the first half; one near the end; double stops in thirds.
        const auto glam = composeSolo (SoloStyle::glamFlash, spans, 4.0, tuning, {}, variation);
        CHECK (longestRun (glam, 0.0, 32.0) < 4);
        CHECK (longestRun (glam, 32.0, 64.0) >= 4);
        for (size_t i = 1; i < glam.notes.size(); ++i)
            if (std::abs (glam.notes[i].beat - glam.notes[i - 1].beat) < 1.0e-6)
            {
                const auto gap = std::abs (glam.notes[i].pitch - glam.notes[i - 1].pitch);
                CHECK ((gap == 3 || gap == 4));
                ++thirds;
            }

        // SLEAZE BLUES: at most one whammy dip, a semitone; nothing busier than 8 notes a bar outside the run.
        const auto sleaze = composeSolo (SoloStyle::sleazeBlues, spans, 4.0, tuning, {}, variation);
        auto here = 0;
        for (const auto& n : sleaze.notes)
            if (n.effect == LineNote::Effect::dive)
            {
                CHECK (n.amount == 1.0f);
                ++here;
            }
        CHECK (here <= 1);
        dips += here;

        // TOP HAT MELODY: the melody laid back off the grid.
        const auto hat = composeSolo (SoloStyle::topHatMelody, spans, 4.0, tuning, {}, variation);
        auto late = 0;
        for (const auto& n : hat.notes)
        {
            const auto off = n.beat * 4.0 - std::floor (n.beat * 4.0 + 1.0e-6);
            late += std::abs (off - 0.12) < 1.0e-3 ? 1 : 0;
        }
        CHECK (late >= 8);

        // PURPLE SCREAM: bends grow through the solo, an outburst climbing by half steps, a high ending.
        const auto purple = composeSolo (SoloStyle::purpleScream, spans, 4.0, tuning, {}, variation);
        auto lastBend = 0.0f;
        for (const auto& n : purple.notes)
            if (n.effect == LineNote::Effect::bend && n.amount >= 1.0f)
            {
                CHECK (n.amount >= lastBend);
                lastBend = n.amount;
            }
        auto climb = 0, longestClimb = 0;
        for (size_t i = 1; i < purple.notes.size(); ++i)
        {
            climb = purple.notes[i].pitch == purple.notes[i - 1].pitch + 1 ? climb + 1 : 0;
            longestClimb = juce::jmax (longestClimb, climb);
        }
        CHECK (longestClimb >= 8);
        CHECK (purple.notes.back().pitch >= tuning.strings.back() + 10);
    }
    CHECK (thirds > 0);
    CHECK (dips > 0);
}

TEST_CASE ("0.69.0: round four's styles have their own habits", "[solo][solostyles]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    auto harmonised = 0, shortBends = 0, gypsyNotes = 0;
    for (int variation = 0; variation < 4; ++variation)
    {
        INFO ("variation " << variation);
        // GYPSY SHRED: harmony a third above (3 or 4 semitones, together), harmonic minor's G# heard, no dives.
        const auto gypsy = composeSolo (SoloStyle::gypsyShred, spans, 4.0, tuning, {}, variation);
        for (size_t i = 1; i < gypsy.notes.size(); ++i)
            if (std::abs (gypsy.notes[i].beat - gypsy.notes[i - 1].beat) < 1.0e-6)
            {
                const auto gap = std::abs (gypsy.notes[i].pitch - gypsy.notes[i - 1].pitch);
                harmonised += gap == 3 || gap == 4 ? 1 : 0;
            }
        for (const auto& n : gypsy.notes)
        {
            gypsyNotes += ((n.pitch % 12) + 12) % 12 == 8 ? 1 : 0;
            CHECK ((n.effect != LineNote::Effect::dive || n.amount == 1.0f));   // (a dip, never a dive bomb)
        }

        // TENSION & RELEASE: phrase ends approached from a half step above.
        const auto tension = composeSolo (SoloStyle::tensionRelease, spans, 4.0, tuning, {}, variation);
        auto resolved = 0;
        for (size_t i = 1; i < tension.notes.size(); ++i)
            resolved += tension.notes[i - 1].pitch == tension.notes[i].pitch + 1 ? 1 : 0;
        CHECK (resolved >= 4);

        // TORTURED BENDS: some bends left a quarter tone short.
        const auto tortured = composeSolo (SoloStyle::torturedBends, spans, 4.0, tuning, {}, variation);
        for (const auto& n : tortured.notes)
            shortBends += n.effect == LineNote::Effect::bend && std::abs (n.amount - std::round (n.amount)) > 0.1f ? 1 : 0;

        // GRUNGE MELODY: no whammy at all; vibrato only on the last note; the second half the first an octave up.
        const auto grunge = composeSolo (SoloStyle::grungeMelody, spans, 4.0, tuning, {}, variation);
        auto vibratos = 0;
        for (const auto& n : grunge.notes)
        {
            CHECK (n.effect != LineNote::Effect::dive);
            vibratos += n.effect == LineNote::Effect::vibrato ? 1 : 0;
        }
        CHECK (vibratos == 1);
        if (variation % 4 != 3)
        {
            // (the tune's first phrase, and the same phrase in the second half an octave higher: 0.116.0, the tune
            // comes back on its period, two bars or more, so 32 beats on it's there again)
            auto firstNotes = 0, again = 0, higher = 0;
            for (const auto& n : grunge.notes)
                if (n.beat < 8.0)
                {
                    ++firstNotes;
                    for (const auto& m : grunge.notes)
                        if (std::abs (m.beat - (n.beat + 32.0)) < 1.0e-6)
                        {
                            ++again;
                            higher += m.pitch > n.pitch ? 1 : 0;
                            break;
                        }
                }
            REQUIRE (firstNotes >= 2);
            CHECK (again * 2 >= firstNotes);
            CHECK (higher * 2 >= again);
        }
    }
    CHECK (harmonised > 0);
    CHECK (shortBends > 0);
    CHECK (gypsyNotes > 0);
}

//==============================================================================
// 0.116.0: solos phrased as the owner's Pink Floyd and Guns N' Roses solos (read for their shapes only).
namespace
{
    double medianPitch (const LineIdea& idea, double from, double to)
    {
        std::vector<int> p;
        for (const auto& n : idea.notes)
            if (n.beat >= from - 1.0e-9 && n.beat < to - 1.0e-9)
                p.push_back (n.pitch);
        if (p.empty())
            return 0.0;
        std::sort (p.begin(), p.end());
        return p[p.size() / 2];
    }
}

TEST_CASE ("0.116.0: the new solo styles are there, with our names", "[solo][solostyles][solo116]")
{
    REQUIRE (soloStyles().size() == 27);
    for (const auto* name : { "SINGING BENDS", "SHUFFLE CONVERSATION", "SLOW BUILD WITH RAKES", "FALLING MAJOR MELODY", "SUNG IN THE GAPS",
                              "ROLLING SEQUENCES", "LAID-BACK SWEET BENDS", "THEME EVERY ROUND", "TWO SPEEDS", "LICK TWICE TO END" })
        CHECK (soloStyleNamed (name) != SoloStyle::automatic);
    CHECK (soloStyleForGuitarStyle ((int) GuitarStyle::rock) == SoloStyle::singingBends);
    CHECK (soloStyleForGuitarStyle ((int) GuitarStyle::shred) == SoloStyle::twoSpeeds);
}

TEST_CASE ("0.116.0: every solo phrases as the real ones: held landings, off-beat starts, a peak past halfway, few big bends",
           "[solo][solostyles][solo116]")
{
    const auto spans = amBars (16);   // (64 beats)
    const auto& tuning = GuitarTuning::all()[0];
    for (const auto style : soloStyles())
    {
        if (style == SoloStyle::automatic)
            continue;
        auto big = 0, bends = 0, held = 0, heldOnChord = 0, notes = 0;
        for (int variation = 0; variation < 4; ++variation)
        {
            INFO (soloStyleName (style) << " variation " << variation);
            const auto solo = composeSolo (style, spans, 4.0, tuning, {}, variation);
            REQUIRE_FALSE (solo.notes.empty());
            notes += (int) solo.notes.size();
            for (const auto& n : solo.notes)
            {
                if (n.effect == LineNote::Effect::bend)
                {
                    ++bends;
                    big += n.amount >= 3.0f ? 1 : 0;
                }
                if (n.length >= 1.5)
                {
                    ++held;
                    const auto& c = spans[(size_t) juce::jlimit (0, 15, (int) (n.beat / 4.0))].chord;
                    const auto sounding = n.pitch + (n.effect == LineNote::Effect::bend ? juce::roundToInt (n.amount) : 0);
                    for (const auto pc : c.pitchClasses())
                        if (((sounding % 12) + 12) % 12 == pc)
                        {
                            ++heldOnChord;
                            break;
                        }
                }
            }
            // Never a ramp of 9-16 notes a bar: at most about three notes a beat over the solo.
            CHECK ((double) solo.notes.size() / 64.0 <= 3.2);
        }
        INFO (soloStyleName (style));
        // Phrases end on held notes, most of them chord tones (as sounded).
        CHECK (held >= 4 * 6);
        CHECK (heldOnChord * 10 >= held * 5);
        // Bends of a step and a half or more: rare (PURPLE SCREAM's grow on purpose: half, whole, a step and a half).
        if (style != SoloStyle::purpleScream)
            CHECK (big * 4 <= juce::jmax (1, bends));
    }
    // The melodic new styles: the register peaks past halfway and comes back down.
    for (const auto style : { SoloStyle::singingBends, SoloStyle::shuffleConversation, SoloStyle::laidBackBends, SoloStyle::rollingSequences })
    {
        auto higherMiddle = 0;
        for (int variation = 0; variation < 6; ++variation)
        {
            const auto solo = composeSolo (style, spans, 4.0, tuning, {}, variation);
            const auto start = medianPitch (solo, 0.0, 16.0), peak = medianPitch (solo, 36.0, 52.0);
            higherMiddle += peak > start ? 1 : 0;
        }
        INFO (soloStyleName (style));
        CHECK (higherMiddle >= 4);
    }
    // Most phrases start off beat 1: of the notes after a held one, few on a bar line.
    {
        auto afterHeld = 0, onOne = 0;
        for (int variation = 0; variation < 6; ++variation)
        {
            const auto solo = composeSolo (SoloStyle::singingBends, spans, 4.0, tuning, {}, variation);
            for (size_t i = 1; i < solo.notes.size(); ++i)
                if (solo.notes[i - 1].length >= 1.5)
                {
                    ++afterHeld;
                    onOne += std::abs (std::fmod (solo.notes[i].beat, 4.0)) < 1.0e-6 ? 1 : 0;
                }
        }
        CHECK (onOne * 10 <= afterHeld * 4);
    }
}

TEST_CASE ("0.116.0: the new styles' own habits", "[solo][solostyles][solo116]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    for (int variation = 0; variation < 3; ++variation)
    {
        INFO ("variation " << variation);
        // LICK TWICE TO END: the last four bars a two-bar lick played twice.
        {
            const auto solo = composeSolo (SoloStyle::lickTwice, spans, 4.0, tuning, {}, variation);
            std::vector<std::pair<long, int>> a, b;
            for (const auto& n : solo.notes)
            {
                if (n.beat >= 48.0 - 1.0e-6 && n.beat < 56.0 - 1.0e-6) a.push_back ({ std::lround ((n.beat - 48.0) * 96.0), n.pitch });
                if (n.beat >= 56.0 - 1.0e-6 && n.beat < 64.0 - 1.0e-6) b.push_back ({ std::lround ((n.beat - 56.0) * 96.0), n.pitch });
            }
            REQUIRE (a.size() >= 8);
            CHECK (a.size() == b.size());
            auto same = 0;
            for (size_t i = 0; i < juce::jmin (a.size(), b.size()); ++i)
                same += a[i] == b[i] ? 1 : 0;
            CHECK (same + 2 >= (int) a.size());   // (the very last notes may be the held root)
        }
        // SHUFFLE CONVERSATION: triplet feel, nothing on the sixteenths between.
        {
            const auto solo = composeSolo (SoloStyle::shuffleConversation, spans, 4.0, tuning, {}, variation);
            auto swung = 0;
            for (const auto& n : solo.notes)
            {
                const auto in = n.beat - std::floor (n.beat);
                swung += std::abs (in - 2.0 / 3.0) < 0.02 ? 1 : 0;
                CHECK (std::abs (in - 0.25) > 0.02);
                CHECK (std::abs (in - 0.75) > 0.02);
            }
            CHECK (swung > 0);
        }
        // ROLLING SEQUENCES: opens on one held low note.
        {
            const auto solo = composeSolo (SoloStyle::rollingSequences, spans, 4.0, tuning, {}, variation);
            CHECK (solo.notes.front().beat == 0.0);
            CHECK (solo.notes.front().length >= 4.0);
        }
        // FALLING MAJOR MELODY: the first quarter higher than the last.
        {
            const auto solo = composeSolo (SoloStyle::fallingMajorMelody, spans, 4.0, tuning, {}, variation);
            CHECK (medianPitch (solo, 0.0, 16.0) > medianPitch (solo, 48.0, 64.0));
        }
        // LAID-BACK SWEET BENDS: behind the beat, and the last bar one long low root.
        {
            const auto solo = composeSolo (SoloStyle::laidBackBends, spans, 4.0, tuning, {}, variation);
            auto late = 0;
            for (const auto& n : solo.notes)
                late += std::abs (n.beat * 2.0 - std::round (n.beat * 2.0)) > 0.05 ? 1 : 0;
            CHECK (late * 2 >= (int) solo.notes.size());
            const auto& last = solo.notes.back();
            CHECK (last.length >= 3.0);
            CHECK (((last.pitch % 12) + 12) % 12 == 4);
        }
        // THEME EVERY ROUND: the opening phrase comes back when the four chords come round.
        {
            const auto solo = composeSolo (SoloStyle::cycleTheme, spans, 4.0, tuning, {}, variation);
            std::set<std::pair<long, int>> first;
            for (const auto& n : solo.notes)
                if (n.beat < 3.0)
                    first.insert ({ std::lround (n.beat * 96.0), n.pitch });
            auto backAgain = 0;
            for (const auto& n : solo.notes)
                for (const auto shift : { 16.0, 32.0 })
                    backAgain += first.count ({ std::lround ((n.beat - shift) * 96.0), n.pitch }) > 0 ? 1 : 0;
            CHECK (backAgain >= (int) first.size());
        }
    }
    // A pre-bend let down: the wheel starts bent, holds, comes back to the note.
    {
        LineNote n;
        n.beat = 4.0;
        n.length = 2.0;
        n.pitch = 64;
        n.effect = LineNote::Effect::release;
        n.amount = 2.0f;
        const auto c = Studio::effectControllers ({ n }, 2, 120.0);
        REQUIRE (c.size() == 4);
        CHECK (c[0].start == 4.0);
        CHECK (c[0].value == 16383);
        CHECK (c[2].value == 8192);
        CHECK (c[2].start > c[1].start);
        // A bend at 120 BPM reaches its note in 0.19 s (0.38 beat).
        n.effect = LineNote::Effect::bend;
        const auto up = Studio::effectControllers ({ n }, 2, 120.0);
        CHECK (std::abs (up[1].start - (4.0 + 0.38)) < 1.0e-6);
    }
}

TEST_CASE ("debug falling", "[.dbgfall]")
{
    const auto spans = amBars (16);
    for (const auto st : { SoloStyle::grungeMelody, SoloStyle::cycleTheme })
        for (int v = 0; v < 3; ++v)
        {
            std::printf ("== %s %d\n", soloStyleName (st).toRawUTF8(), v);
            const auto solo = composeSolo (st, spans, 4.0, GuitarTuning::all()[0], {}, v);
            for (const auto& n : solo.notes)
                std::printf ("%6.2f %5.2f %d e%d\n", n.beat, n.length, n.pitch, (int) n.effect);
        }
}
