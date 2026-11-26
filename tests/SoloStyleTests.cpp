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

    // (0.207.0) Round six's styles that are busy first or flat on purpose, as their songs' solos: a trill, a burst, a
    // cell stream, a tremolo scream or a looped triad at the start, an even stream, a seesaw dying away.
    bool busyFirstOrFlat207 (SoloStyle s)
    {
        return s == SoloStyle::trillThenHold || s == SoloStyle::wahWaves || s == SoloStyle::cellsUpShakeTheTop || s == SoloStyle::stopTimeDoubleStops
            || s == SoloStyle::unpluggedPentatonicStream || s == SoloStyle::seesawAndRing || s == SoloStyle::panelsOverTheChug
            || s == SoloStyle::tremoloThenFlurry;   // (0.246.0: the tremolo first, as its song)
    }

    // (0.207.0) The last note (as sounded) round six's styles choose over amBars (A minor; the last chord E major), as
    // their songs end: the tonic A, the fifth E, the flat seven G, the second B, the major third C sharp (bent into), the
    // minor third C, the last chord's fifth B. Empty: not one of them.
    std::set<int> roundSixEnding (SoloStyle s, int variation)
    {
        switch (s)
        {
            case SoloStyle::holdPedalTwinStorm: case SoloStyle::trillThenHold: case SoloStyle::wahWaves: case SoloStyle::lateCallsBentHome:
            case SoloStyle::cellsUpShakeTheTop: case SoloStyle::stopTimeDoubleStops: case SoloStyle::bluesCurlsUnderTheTonic:
            case SoloStyle::sungTwiceTurned: case SoloStyle::fourBendsAndOut:
                return { 9 };
            case SoloStyle::panelsOverTheChug: return { variation % 2 == 0 ? 7 : 9 };
            case SoloStyle::sequenceThenTwinArpeggios: case SoloStyle::octaveLeapsAndCries: case SoloStyle::riffOnAWarpedBar:
            case SoloStyle::seesawAndRing:
                return { 4 };
            case SoloStyle::majorRubOverMinorRiff: return { 1 };
            case SoloStyle::pickupIntoChordStabs: case SoloStyle::tuneAnOctaveDown: return { 11 };
            case SoloStyle::pickupQuarterBreak: case SoloStyle::tripletHammerFallingTune: return { 0 };
            case SoloStyle::unpluggedPentatonicStream: return { 9, 0, 2 };
            default: return {};
        }
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
    REQUIRE (soloStyles().size() == 126);   // (0.207.0: twenty more; 0.162.0: eight more; 0.136.0: seven more) (0.116.0: ten more; 0.128.0: four more; 0.129.0: seven more; 0.133.0: eleven more; 0.134.0: fifteen more; 0.135.0: twenty-one more)
    CHECK (soloStyles().front() == SoloStyle::automatic);
    for (const auto s : soloStyles())
    {
        CHECK (soloStyleNamed (soloStyleName (s)) == s);
        CHECK (soloStyleDescription (s).isNotEmpty());
        for (const auto* player : { "Gilmour", "Perry", "Van Halen", "Page", "EVH", "Zeppelin", "Floyd", "Aerosmith",
                                   "Dimebag", "Darrell", "Pantera", "Hammett", "Metallica", "Wylde", "Black Label", "Petrucci", "Dream Theater",
                                   "DeVille", "Poison", "Mick Mars", "Motley", "Slash", "Guns N", "Prince",
                                   "Synyster", "Avenged", "Halen", "Oasis", "Gallagher", "Stone Temple", "Pilots", "DeLeo", "Weiland", "Plush", "Vasoline", "Crue", "Feelgood", "Wonderwall", "Sixx", "Panama", "Poundcake", "Lowery", "Sevendust", "Cantrell", "Alice in Chains", "Cobain", "Nirvana", "Disturbed", "Godsmack", "Linkin", "Breaking Benjamin", "Chemical", "Stricken", "Awake", "In Pieces" })
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
            if (style != SoloStyle::shuffleConversation && style != SoloStyle::grungeMelody && style != SoloStyle::fallingMajorMelody
                && style != SoloStyle::hotStartCoolDown && style != SoloStyle::whammyFire
                && style != SoloStyle::holdAndAccelerate && style != SoloStyle::burstAndBend
                && style != SoloStyle::bottomToTopClimb && style != SoloStyle::burstBarBurst
                && style != SoloStyle::fixedHead && style != SoloStyle::slideInOutro
                && style != SoloStyle::tremoloThenTalk && style != SoloStyle::riffStutterBreak && style != SoloStyle::swayingThirdsTheme
                && style != SoloStyle::stillCellMovingChords && style != SoloStyle::riffDoubleStopUnison && style != SoloStyle::riffTalkCallStab
                && ! busyFirstOrFlat207 (style))   // (0.162.0: flat or hot first, as the round-four songs' solos)   // (0.135.0: the same head every other bar: flat on purpose; 0.136.0: an even outro melody)   // (0.133.0: busy early on purpose, as the files)   // (no busy waves: flat on purpose; 0.128.0: busy first on purpose)
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
                bends += n.effect == LineNote::Effect::bend || n.effect == LineNote::Effect::slowBend ? 1 : 0;
                vibratos += n.effect == LineNote::Effect::vibrato || n.effect == LineNote::Effect::slideIn
                         || n.effect == LineNote::Effect::fallOff || n.effect == LineNote::Effect::wobble
                         || n.effect == LineNote::Effect::dive ? 1 : 0;   // (0.128.0: slides and fall-offs too; 0.129.0: the whammy's)
            }
            CHECK (bends + vibratos > 0);
            // The last note: the last chord's root (E), to the end (0.128.0: as sounded; bent up into the root or
            // the fifth; or dropped to the low root for a beat, back into the riff).
            auto last = solo.notes.front();
            for (const auto& n : solo.notes)
                if (n.beat >= last.beat)
                    last = n;
            const auto bent = last.effect == LineNote::Effect::bend || last.effect == LineNote::Effect::slowBend;   // (0.136.0: slow too)
            const auto sounding = (((last.pitch + (bent ? juce::roundToInt (last.amount) : 0)) % 12) + 12) % 12;
            // (0.162.0) The round-four songs' styles end on the key's note: the tonic A, its minor third C, or the
            // last chord's (E major's) third G#.
            // (0.207.0) Round six's styles choose their own last note (as their songs end): see roundSixEnding.
            if (const auto six = roundSixEnding (style, variation); ! six.empty())
                CHECK (six.count (sounding) == 1);
            else
            {
            const auto wanted = style == SoloStyle::riffDoubleStopUnison ? 0
                              : style == SoloStyle::twinThirdsSingalong ? 8
                              : style == SoloStyle::threesAcrossBlocks || style == SoloStyle::riffTalkCallStab || style == SoloStyle::tremoloThenTalk
                                || style == SoloStyle::riffStutterBreak || style == SoloStyle::swayingThirdsTheme ? 9 : -1;
            if (wanted >= 0)
                CHECK (sounding == wanted);
            else
                CHECK ((sounding == 4 || (bent && sounding == 11)));
            }
            const auto droppedLow = last.length <= 1.0 + 1.0e-6 && last.pitch < tuning.strings[1] + 5 + 12;
            CHECK ((last.beat + last.length >= 32.0 - 1.0e-6 || droppedLow));
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
        // SCREAM & DIVE: a harmonic bent up a minor third before a dive at the end; (0.134.0) diminished cells (minor
        // thirds stacked) instead of licks shifted a fret at a time, and whammy squeals.
        const auto scream = composeSolo (SoloStyle::screamAndDive, spans, 4.0, tuning, {}, variation);
        CHECK (count (scream, [] (const LineNote& n) { return n.technique == LineNote::Technique::harmonic
                                                             && n.effect == LineNote::Effect::bend && n.amount == 3.0f; }) == 1);
        CHECK (count (scream, [] (const LineNote& n) { return n.effect == LineNote::Effect::dive; }) == 1);
        auto climbs = 0;   // three notes a minor third apart in a row
        for (size_t i = 2; i < scream.notes.size(); ++i)
            climbs += scream.notes[i].pitch == scream.notes[i - 1].pitch + 3 && scream.notes[i - 1].pitch == scream.notes[i - 2].pitch + 3 ? 1 : 0;
        CHECK (climbs >= 8);
        CHECK (count (scream, [] (const LineNote& n) { return n.effect == LineNote::Effect::pullUp; }) >= 1);

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

        // PROG PRECISION: the 2 is heard (B in A minor); (0.135.0, from the owner's Dream Theater files) long sextuplet
        // streams and hardly a rest.
        const auto prog = composeSolo (SoloStyle::progPrecision, spans, 4.0, tuning, {}, variation);
        CHECK (count (prog, [] (const LineNote& n) { return ((n.pitch % 12) + 12) % 12 == 11; }) > 0);
        auto run = 0, longest = 0;
        for (size_t i = 1; i < prog.notes.size(); ++i)
        {
            run = std::abs (prog.notes[i].beat - prog.notes[i - 1].beat - 1.0 / 6.0) < 1.0e-6 ? run + 1 : 0;
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
        // GLAM FLASH: no fast run in the first half; one near the end; (0.135.0, from the owner's Motley Crue files) no
        // double stops in thirds, the second phrase the first an octave up, a tremolo-picked climb (the same note again
        // and again in sixteenths), the top root let down from a bend to end.
        const auto glam = composeSolo (SoloStyle::glamFlash, spans, 4.0, tuning, {}, variation);
        CHECK (longestRun (glam, 0.0, 32.0) < 4);
        CHECK (longestRun (glam, 32.0, 64.0) >= 4);
        for (size_t i = 1; i < glam.notes.size(); ++i)
            if (std::abs (glam.notes[i].beat - glam.notes[i - 1].beat) < 1.0e-6)
                ++thirds;
        auto repeats = 0;
        for (size_t i = 1; i < glam.notes.size(); ++i)
            repeats += glam.notes[i].pitch == glam.notes[i - 1].pitch && glam.notes[i].beat - glam.notes[i - 1].beat < 0.3 ? 1 : 0;
        CHECK (repeats >= 8);
        CHECK (glam.notes.back().effect == LineNote::Effect::release);

        // SLEAZE BLUES: (0.135.0) a triplet shuffle (every note on an eighth or a triplet eighth), no flat five, no dip.
        const auto sleaze = composeSolo (SoloStyle::sleazeBlues, spans, 4.0, tuning, {}, variation);
        for (const auto& n : sleaze.notes)
        {
            const auto onTriplet = std::abs (n.beat * 3.0 - std::round (n.beat * 3.0)) < 0.02;
            const auto onEighth = std::abs (n.beat * 2.0 - std::round (n.beat * 2.0)) < 0.02;
            CHECK ((onTriplet || onEighth));
            const auto sounding = n.pitch + (n.effect == LineNote::Effect::bend ? juce::roundToInt (n.amount) : 0);
            CHECK (((sounding % 12) + 12) % 12 != 3);   // (no D# heard: the flat five in A)
            dips += n.effect == LineNote::Effect::dive ? 1 : 0;
        }

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
    CHECK (thirds == 0);
    CHECK (dips == 0);
}

TEST_CASE ("0.69.0: round four's styles have their own habits", "[solo][solostyles]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    auto harmonised = 0, shortBends = 0, gypsyNotes = 0, grungeHalf = 0, grungeWhole = 0;
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

        // GRUNGE MELODY: (0.136.0, put right from the owner's Stone Temple Pilots files) no whammy dives; vibrato on
        // held notes (not only the last); half-step bends as well as whole steps; no noise bursts (nothing busier than
        // sixteenths).
        const auto grunge = composeSolo (SoloStyle::grungeMelody, spans, 4.0, tuning, {}, variation);
        auto vibratos = 0;
        for (const auto& n : grunge.notes)
        {
            CHECK (n.effect != LineNote::Effect::dive);
            vibratos += n.effect == LineNote::Effect::vibrato ? 1 : 0;
            if (n.effect == LineNote::Effect::bend)
                (n.amount < 1.5f ? grungeHalf : grungeWhole) += 1;
        }
        CHECK (vibratos >= 3);
        for (size_t i = 1; i < grunge.notes.size(); ++i)
            CHECK (grunge.notes[i].beat - grunge.notes[i - 1].beat > 0.2);
    }
    CHECK (grungeHalf > 0);
    CHECK (grungeWhole > 0);
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
    REQUIRE (soloStyles().size() == 126);   // (0.207.0: twenty more; 0.162.0: eight more; 0.128.0: four more; 0.129.0: seven more; 0.133.0: eleven more; 0.134.0: fifteen more)
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
                    const auto sounding = n.pitch + (n.effect == LineNote::Effect::bend || n.effect == LineNote::Effect::slowBend ? juce::roundToInt (n.amount) : 0);
                    for (const auto pc : c.pitchClasses())
                        if (((sounding % 12) + 12) % 12 == pc)
                        {
                            ++heldOnChord;
                            break;
                        }
                }
            }
            // Never a ramp of 9-16 notes a bar: at most about three notes a beat over the solo (0.134.0: a twin's
            // harmony notes are the second guitar's).
            auto played = 0;
            for (const auto& n : solo.notes)
                played += n.harmony ? 0 : 1;
            if (style != SoloStyle::stillCellMovingChords && style != SoloStyle::tremoloThenTalk   // (0.162.0: streams with no rests, as the files)
                && style != SoloStyle::panelsOverTheChug && style != SoloStyle::sequenceThenTwinArpeggios && style != SoloStyle::wahWaves)   // (0.207.0: fast cells looped, sixteenths through the second half, bursts of eight a beat: as their songs)
                CHECK ((double) played / 64.0 <= 3.2);
        }
        INFO (soloStyleName (style));
        // Phrases end on held notes, most of them chord tones (as sounded).
        // (0.133.0: HOOK, THIRDS, HOOK's middle third is double stops in eighths and sixteenths, nothing held)
        // (0.162.0: the round-four songs' styles breathe through short holds and land on the key's notes: their own test)
        const auto roundFour = (int) style >= (int) SoloStyle::threesAcrossBlocks;
        if (! roundFour)
        {
            CHECK (held >= (style == SoloStyle::hookThirdsHook ? 4 * 3 : 4 * 5));   // (0.128.0: the long first and last notes take a phrase or two's room)
            CHECK (heldOnChord * 10 >= held * 5);
        }
        // Bends of a step and a half or more: rare (PURPLE SCREAM's grow on purpose: half, whole, a step and a half).
        // (0.134.0: HALF-SPEED BENT CELLS and SLOW STAIRCASE OF BENDS bend minor thirds on purpose, and STATED, RAISED,
        // DROPPED has its one over-bend among few bends, as the files)
        if (style != SoloStyle::purpleScream && style != SoloStyle::halfSpeedBentCells && style != SoloStyle::slowStaircaseBends
            && style != SoloStyle::statedRaisedDropped && style != SoloStyle::valleyLaunch)   // (0.136.0: its climax bends wider each time)
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


// 0.128.0: round one from the owner's Metallica and Def Leppard solos (read for their shapes only).
TEST_CASE ("0.128.0: fast passages run at 8 to 12 notes a second whatever the tempo, never past the shortest note",
           "[solo][solostyles][solo128]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    // The fastest run in seconds between onsets (bursts are the solo's fastest notes).
    const auto fastest = [] (const LineIdea& idea, double bpm)
    {
        std::vector<double> at;
        for (const auto& n : idea.notes)
            at.push_back (n.beat);
        std::sort (at.begin(), at.end());
        at.erase (std::unique (at.begin(), at.end(), [] (double a, double b) { return std::abs (a - b) < 1.0e-6; }), at.end());
        auto best = 1.0e9;
        for (size_t i = 1; i < at.size(); ++i)
            best = juce::jmin (best, (at[i] - at[i - 1]) * 60.0 / bpm);
        return best;
    };
    for (const auto style : { SoloStyle::rollingSequences, SoloStyle::repeatedBendClimb, SoloStyle::pedalDyadHook, SoloStyle::hotStartCoolDown })
    {
        INFO (soloStyleName (style));
        // At 66 BPM a 16th is 4.4 notes a second: the runs go to 32nds (8.8 a second).
        const auto slow = composeSolo (style, spans, 4.0, tuning, {}, 1, 0.0, 66.0);
        CHECK (fastest (slow, 66.0) <= 0.125 * 60.0 / 66.0 + 1.0e-6);
        // Without a tempo: as written (16ths), and the owner's shortest note always wins.
        CHECK (fastest (composeSolo (style, spans, 4.0, tuning, {}, 1), 66.0) >= 0.25 * 60.0 / 66.0 - 0.04 * 60.0 / 66.0);
        CHECK (fastest (composeSolo (style, spans, 4.0, tuning, {}, 1, 0.25, 66.0), 66.0) >= (0.25 - 0.04) * 60.0 / 66.0);
        // At 220 BPM 16ths would be 14.7 a second: they slow to 8ths (7.3) or triplets, never 16ths.
        const auto fast = composeSolo (style, spans, 4.0, tuning, {}, 1, 0.0, 220.0);
        CHECK (fastest (fast, 220.0) >= 0.3 * 60.0 / 220.0);
    }
}

TEST_CASE ("0.128.0: REPEATED BEND CLIMB strikes the same bend again and again, each group higher; bends are aimed",
           "[solo][solostyles][solo128]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    auto groups = 0, climbing = 0;
    for (int variation = 0; variation < 6; ++variation)
    {
        const auto solo = composeSolo (SoloStyle::repeatedBendClimb, spans, 4.0, tuning, {}, variation, 0.0, 120.0);
        // Runs of three or more bent notes on one pitch, one after another.
        int run = 0, lastPitch = -100, groupPitch = -100;
        for (const auto& n : solo.notes)
        {
            if (n.effect == LineNote::Effect::bend && n.pitch == lastPitch)
                ++run;
            else
            {
                if (run >= 2)
                {
                    ++groups;
                    climbing += lastPitch > groupPitch && groupPitch > 0 ? 1 : 0;
                    groupPitch = lastPitch;
                }
                run = 0;
            }
            lastPitch = n.effect == LineNote::Effect::bend ? n.pitch : -100;
        }
    }
    CHECK (groups >= 6);
    CHECK (climbing >= 2);

    // Across the styles, bends sing into the root, the fifth or the key's fourth (A minor: A, E, D; over the major
    // chords F, C and E their thirds and sixths too) far more often than not.
    auto aimed = 0, all = 0;
    for (const auto style : soloStyles())
    {
        if (style == SoloStyle::automatic)
            continue;
        for (int variation = 0; variation < 3; ++variation)
            for (const auto& n : composeSolo (style, spans, 4.0, tuning, {}, variation).notes)
                if (n.effect == LineNote::Effect::bend && n.length >= 0.75)
                {
                    const auto& c = spans[(size_t) juce::jlimit (0, 15, (int) (n.beat / 4.0))].chord;
                    const auto pc = ((n.pitch + juce::roundToInt (n.amount)) % 12 + 12) % 12;
                    std::set<int> aims { c.root % 12, (c.root + 7) % 12 };
                    if (c.type == T::major) { aims.insert ((c.root + 4) % 12); aims.insert ((c.root + 9) % 12); }
                    else aims.insert (2);
                    aimed += aims.count (pc) > 0 ? 1 : 0;
                    ++all;
                }
    }
    INFO (aimed << " of " << all);
    CHECK (aimed * 10 >= all * 6);
}

TEST_CASE ("0.128.0: slides in, fall-offs out, held first and last notes, the new endings", "[solo][solostyles][solo128]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    auto slides = 0, falls = 0, longFirst = 0, longLast = 0, solos = 0;
    for (const auto style : soloStyles())
    {
        if (style == SoloStyle::automatic || (int) style >= (int) SoloStyle::threesAcrossBlocks)   // (0.162.0: written their own way)
            continue;
        for (int variation = 0; variation < 3; ++variation)
        {
            const auto solo = composeSolo (style, spans, 4.0, tuning, {}, variation);
            ++solos;
            for (const auto& n : solo.notes)
            {
                slides += n.effect == LineNote::Effect::slideIn ? 1 : 0;
                falls += n.effect == LineNote::Effect::fallOff ? 1 : 0;
            }
            for (const auto& n : solo.notes)
                if (n.length >= 1.5)
                {
                    longFirst += n.length >= 5.5 ? 1 : 0;   // (the first held note)
                    break;
                }
            // (the last note held from the last landing; struck again where the last chord comes in)
            const auto& last = solo.notes.back();
            auto held = last.length;
            const auto& before = solo.notes[solo.notes.size() - 2];
            if (std::abs (before.beat + before.length + 0.02 - last.beat) < 0.05 && before.length >= 1.5)
                held += before.length;
            longLast += held >= 5.5 ? 1 : 0;
        }
    }
    CHECK (slides >= solos / 2);
    CHECK (falls >= solos / 4);
    CHECK (longFirst * 2 >= solos);
    CHECK (longLast * 2 >= solos);

    // Each new style's endings come up across takes.
    const auto endingOf = [&] (const LineIdea& solo)
    {
        const auto& last = solo.notes.back();
        if (last.effect == LineNote::Effect::bend) return juce::String ("bend up");
        if (last.effect == LineNote::Effect::fallOff) return juce::String ("fall-off");
        if (last.length <= 1.0 + 1.0e-6 && last.beat < 63.0) return juce::String ("drop low");
        auto repeated = 0, rising = 0;
        for (size_t i = solo.notes.size() - 1; i > 0 && solo.notes[i - 1].beat >= 60.0 - 1.0e-6; --i)
        {
            repeated += solo.notes[i - 1].pitch == last.pitch ? 1 : 0;
            rising += solo.notes[i - 1].pitch < last.pitch ? 1 : 0;
        }
        if (repeated >= 4) return juce::String ("repeated top");
        if (rising >= 4) return juce::String ("triplet climb");
        return juce::String ("held");
    };
    const auto endingsOf = [&] (SoloStyle style)
    {
        std::set<juce::String> seen;
        for (int variation = 0; variation < 12; ++variation)
            seen.insert (endingOf (composeSolo (style, spans, 4.0, tuning, {}, variation)));
        return seen;
    };
    CHECK (endingsOf (SoloStyle::repeatedBendClimb).count ("bend up") == 1);
    CHECK (endingsOf (SoloStyle::repeatedBendClimb).count ("repeated top") == 1);
    CHECK (endingsOf (SoloStyle::chordChasingArpeggios).count ("triplet climb") == 1);
    CHECK (endingsOf (SoloStyle::pedalDyadHook).count ("repeated top") == 1);
    const auto hot = endingsOf (SoloStyle::hotStartCoolDown);
    CHECK (hot.count ("fall-off") == 1);
    CHECK (hot.count ("drop low") == 1);
}

TEST_CASE ("0.128.0: the new styles' passages: arpeggios on the chords, a pedal over a moving line, hot then cool",
           "[solo][solostyles][solo128]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    // CHORD-CHASING ARPEGGIOS: its fast notes (a sixth of a beat apart at 120 BPM) are the chord's own notes.
    {
        auto fastNotes = 0, onChord = 0;
        for (int variation = 0; variation < 4; ++variation)
        {
            const auto solo = composeSolo (SoloStyle::chordChasingArpeggios, spans, 4.0, tuning, {}, variation, 0.0, 120.0);
            for (size_t i = 0; i + 1 < solo.notes.size(); ++i)
                if (solo.notes[i + 1].beat - solo.notes[i].beat < 0.2 && solo.notes[i].beat < 56.0)
                {
                    ++fastNotes;
                    const auto& c = spans[(size_t) juce::jlimit (0, 15, (int) (solo.notes[i].beat / 4.0))].chord;
                    for (const auto pc : c.pitchClasses())
                        if ((solo.notes[i].pitch % 12 + 12) % 12 == pc) { ++onChord; break; }
                }
        }
        INFO (onChord << " of " << fastNotes);
        CHECK (fastNotes >= 40);
        CHECK (onChord * 10 >= fastNotes * 8);
    }
    // PEDAL DYAD HOOK: a note struck every other 16th, the same pitch, with others between.
    {
        auto pedals = 0;
        for (int variation = 0; variation < 4; ++variation)
        {
            const auto solo = composeSolo (SoloStyle::pedalDyadHook, spans, 4.0, tuning, {}, variation, 0.0, 120.0);
            for (size_t i = 0; i + 4 < solo.notes.size(); ++i)
                if (solo.notes[i].pitch == solo.notes[i + 2].pitch && solo.notes[i].pitch == solo.notes[i + 4].pitch
                    && solo.notes[i + 1].pitch < solo.notes[i].pitch && solo.notes[i + 3].pitch < solo.notes[i].pitch
                    && std::abs (solo.notes[i + 4].beat - solo.notes[i].beat - 1.0) < 1.0e-6)
                    ++pedals;
        }
        CHECK (pedals >= 8);
    }
    // HOT START, COOL DOWN: the first two bars busier than any later two, and higher than the last four.
    for (int variation = 0; variation < 4; ++variation)
    {
        const auto solo = composeSolo (SoloStyle::hotStartCoolDown, spans, 4.0, tuning, {}, variation, 0.0, 120.0);
        const auto first = onsetsIn (solo, 0.0, 8.0);
        for (auto from = 16.0; from <= 56.0; from += 8.0)
            CHECK (first > onsetsIn (solo, from, from + 8.0));
    }
}

TEST_CASE ("0.128.0: slides and fall-offs as pitch bend; a bend rises slower on a longer note", "[solo][solo128]")
{
    LineNote n;
    n.beat = 4.0;
    n.length = 2.0;
    n.pitch = 64;
    n.effect = LineNote::Effect::slideIn;
    n.amount = 3.0f;
    {
        const auto c = Studio::effectControllers ({ n }, 2, 120.0);
        REQUIRE (c.size() == 2);
        CHECK (c[0].start == 4.0);
        CHECK (c[0].value == 0);                         // (3 semitones past a range of 2: as far as it reaches)
        CHECK (c[1].value == 8192);
        CHECK (std::abs (c[1].start - (4.0 + 0.24)) < 1.0e-6);   // (0.12 s at 120 BPM)
    }
    n.effect = LineNote::Effect::fallOff;
    n.amount = 5.0f;
    {
        const auto c = Studio::effectControllers ({ n }, 12, 120.0);
        REQUIRE (c.size() == 4);
        CHECK (c[1].value == 8192);
        CHECK (c[1].start > 4.0 + 1.0);                  // (held most of the note)
        CHECK (c[2].value < 8192 - 3000);                // (down five of twelve semitones)
        CHECK (c[3].value == 8192);
    }
    n.effect = LineNote::Effect::bend;
    n.amount = 2.0f;
    const auto riseOf = [&] (double length)
    {
        auto m = n;
        m.length = length;
        const auto c = Studio::effectControllers ({ m }, 2, 120.0);
        return c[1].start - c[0].start;
    };
    CHECK (riseOf (0.5) < riseOf (2.0));
    CHECK (riseOf (2.0) < riseOf (8.0));
    CHECK (riseOf (8.0) * 0.5 <= 0.7 + 1.0e-6);          // (0.7 s at most)
}

// 0.129.0: round two from the owner's Metallica and Def Leppard solos (shapes only).
TEST_CASE ("0.129.0: UNISON BEND ANTHEM strikes a note, then bends slowly up into it", "[solo][solostyles][solo129]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    auto anthems = 0;
    for (int variation = 0; variation < 4; ++variation)
    {
        const auto solo = composeSolo (SoloStyle::unisonBendAnthem, spans, 4.0, tuning, {}, variation, 0.0, 100.0);
        for (size_t i = 1; i < solo.notes.size(); ++i)
        {
            const auto& n = solo.notes[i];
            if (n.effect != LineNote::Effect::slowBend)
                continue;
            for (size_t k = 0; k < i; ++k)
            {
                const auto& before = solo.notes[k];
                if (before.pitch == n.pitch + 2 && std::abs (before.beat + before.length + 0.02 - n.beat) < 1.0e-6)
                    ++anthems;
            }
        }
    }
    CHECK (anthems >= 6);
    // The slow bend rises over at least half a second (1.3 s at most).
    LineNote n;
    n.beat = 0.0;
    n.length = 6.0;
    n.pitch = 62;
    n.effect = LineNote::Effect::slowBend;
    n.amount = 2.0f;
    const auto c = Studio::effectControllers ({ n }, 2, 120.0);
    REQUIRE (c.size() == 3);
    CHECK (c[1].value == 16383);
    CHECK (c[1].start * 0.5 >= 0.5 - 1.0e-6);
    CHECK (c[1].start * 0.5 <= 1.3 + 1.0e-6);
}

TEST_CASE ("0.129.0: OCTAVE MELODY BUILD doubles its notes an octave; OPEN-STRING PEDAL FIRE picks the low tonic between a climbing line",
           "[solo][solostyles][solo129]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    for (int variation = 0; variation < 3; ++variation)
    {
        const auto solo = composeSolo (SoloStyle::octaveMelodyBuild, spans, 4.0, tuning, {}, variation, 0.0, 120.0);
        auto doubled = 0;
        for (const auto& n : solo.notes)
            for (const auto& m : solo.notes)
                if (std::abs (m.beat - n.beat) < 1.0e-6 && std::abs (m.pitch - n.pitch) == 12)
                {
                    ++doubled;
                    break;
                }
        CHECK (doubled * 10 >= (int) solo.notes.size() * 9);
    }
    auto pedalled = 0;
    for (int variation = 0; variation < 3; ++variation)
    {
        const auto solo = composeSolo (SoloStyle::openStringPedalFire, spans, 4.0, tuning, {}, variation, 0.0, 120.0);
        for (size_t i = 0; i + 4 < solo.notes.size(); ++i)
            if (solo.notes[i].pitch == solo.notes[i + 2].pitch && solo.notes[i].pitch == solo.notes[i + 4].pitch
                && solo.notes[i + 1].pitch > solo.notes[i].pitch && solo.notes[i + 3].pitch > solo.notes[i].pitch
                && ((solo.notes[i].pitch % 12) + 12) % 12 == 9)   // (A: the key's tonic)
                ++pedalled;
    }
    CHECK (pedalled >= 10);
}

TEST_CASE ("0.129.0: TWIN-LEAD HARMONY's second voice, onto the lane's second track", "[solo][solostyles][solo129]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    const int intervals[][2] = { { 3, 4 }, { 4, 6 }, { 12, 12 } };   // thirds, fourths, octaves (from the scale)
    for (int variation = 0; variation < 3; ++variation)
    {
        INFO ("variation " << variation);
        const auto solo = composeSolo (SoloStyle::twinLeadHarmony, spans, 4.0, tuning, {}, variation, 0.0, 120.0);
        auto harmony = 0, inStep = 0, early = 0;
        for (const auto& h : solo.notes)
        {
            if (! h.harmony)
                continue;
            ++harmony;
            early += h.beat < 64.0 * 0.4 - 1.0e-6 ? 1 : 0;
            CHECK (h.effect != LineNote::Effect::bend);
            for (const auto& n : solo.notes)
                if (! n.harmony && std::abs (n.beat - h.beat) < 1.0e-6)
                {
                    const auto gap = std::abs (h.pitch - n.pitch);
                    inStep += gap >= intervals[variation % 3][0] && gap <= intervals[variation % 3][1] ? 1 : 0;
                    CHECK (n.effect != LineNote::Effect::bend);
                    break;
                }
        }
        CHECK (harmony >= 10);
        CHECK (early == 0);
        CHECK (inStep * 10 >= harmony * 8);

        // One track: both voices. Two: the lead, then the harmony alone. A third: a looser copy of the lead.
        Studio studio (false);
        const auto one = studio.doubledCopy (solo, 0, 0.0, 1);
        CHECK (one.notes.size() == solo.notes.size());
        const auto lead = studio.doubledCopy (solo, 0, 0.0, 2);
        const auto second = studio.doubledCopy (solo, 1, 0.0, 2);
        CHECK (std::none_of (lead.notes.begin(), lead.notes.end(), [] (const LineNote& n) { return n.harmony; }));
        CHECK (std::all_of (second.notes.begin(), second.notes.end(), [] (const LineNote& n) { return n.harmony || n.keyswitch; }));
        CHECK ((int) second.notes.size() == harmony);
        CHECK (lead.notes.size() + second.notes.size() == solo.notes.size());
        const auto third = studio.doubledCopy (solo, 2, 0.0, 3);
        CHECK (std::none_of (third.notes.begin(), third.notes.end(), [] (const LineNote& n) { return n.harmony; }));
    }
}

TEST_CASE ("0.129.0: EASTERN TREMOLO STORM, WHAMMY WAIL, TRILL & FALL TAG", "[solo][solostyles][solo129]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    // Eastern: the flat second (A minor: B-flat) and tremolo: repeated picks, or the tremolo articulation when the
    // instrument plays nothing under a 16th.
    {
        auto flatSecond = 0, repeats = 0, articulated = 0;
        for (int variation = 0; variation < 4; ++variation)
        {
            const auto solo = composeSolo (SoloStyle::easternTremoloStorm, spans, 4.0, tuning, {}, variation, 0.0, 120.0);
            for (size_t i = 0; i < solo.notes.size(); ++i)
            {
                flatSecond += ((solo.notes[i].pitch % 12) + 12) % 12 == 10 ? 1 : 0;
                if (i + 2 < solo.notes.size() && solo.notes[i].pitch == solo.notes[i + 1].pitch && solo.notes[i].pitch == solo.notes[i + 2].pitch
                    && solo.notes[i + 2].beat - solo.notes[i].beat < 0.4)
                    ++repeats;
            }
            for (const auto& n : composeSolo (SoloStyle::easternTremoloStorm, spans, 4.0, tuning, {}, variation, 0.25, 120.0).notes)
                articulated += n.technique == LineNote::Technique::tremolo ? 1 : 0;
        }
        CHECK (flatSecond >= 8);
        CHECK (repeats >= 20);
        CHECK (articulated >= 8);
    }
    // Whammy: dives of 7 or more not only at the end, and wobbles; a wobble swings the wheel both ways.
    {
        auto dives = 0, wobbles = 0;
        for (int variation = 0; variation < 4; ++variation)
        {
            const auto solo = composeSolo (SoloStyle::whammyWail, spans, 4.0, tuning, {}, variation, 0.0, 120.0);
            for (size_t i = 0; i + 1 < solo.notes.size(); ++i)
            {
                dives += solo.notes[i].effect == LineNote::Effect::dive && solo.notes[i].amount >= 7.0f ? 1 : 0;
                wobbles += solo.notes[i].effect == LineNote::Effect::wobble ? 1 : 0;
            }
        }
        CHECK (dives >= 4);
        CHECK (wobbles >= 4);
        LineNote n;
        n.beat = 0.0;
        n.length = 2.0;
        n.pitch = 64;
        n.effect = LineNote::Effect::wobble;
        n.amount = 1.0f;
        const auto c = Studio::effectControllers ({ n }, 2, 120.0);
        auto up = 0, down = 0;
        for (const auto& x : c)
        {
            up += x.value > 8192 + 2000 ? 1 : 0;
            down += x.value < 8192 - 2000 ? 1 : 0;
        }
        CHECK (up >= 4);
        CHECK (down >= 4);
        CHECK (c.back().value == 8192);
    }
    // Trills: a note and the one above in turn; the tag ends trilled, then fallen off.
    {
        auto trills = 0, tags = 0;
        for (int variation = 0; variation < 4; ++variation)
        {
            const auto solo = composeSolo (SoloStyle::trillFallTag, spans, 4.0, tuning, {}, variation, 0.0, 120.0);
            for (size_t i = 0; i + 3 < solo.notes.size(); ++i)
                if (solo.notes[i].pitch == solo.notes[i + 2].pitch && solo.notes[i + 1].pitch == solo.notes[i + 3].pitch
                    && solo.notes[i + 1].pitch > solo.notes[i].pitch && solo.notes[i + 1].pitch - solo.notes[i].pitch <= 3
                    && solo.notes[i + 3].beat - solo.notes[i].beat < 1.0)
                    ++trills;
            const auto& last = solo.notes.back();
            const auto& before = solo.notes[solo.notes.size() - 2];
            tags += last.effect == LineNote::Effect::fallOff && before.beat > last.beat - 0.3 && before.length <= 0.25 + 1.0e-6 ? 1 : 0;
        }
        CHECK (trills >= 6);
        CHECK (tags >= 3);
    }
}

// 0.133.0: six styles from the owner's Van Halen solos (shapes only; our names).
namespace
{
    int countOf (const LineIdea& idea, LineNote::Effect e)
    {
        auto c = 0;
        for (const auto& n : idea.notes)
            c += n.effect == e ? 1 : 0;
        return c;
    }
    const LineNote& lastOf (const LineIdea& idea)
    {
        const auto* last = &idea.notes.front();
        for (const auto& n : idea.notes)
            if (n.beat >= last->beat)
                last = &n;
        return *last;
    }
}

TEST_CASE ("0.133.0: the scoop is quick and vibrato starts early", "[solo][solostyles][solo133]")
{
    LineNote n;
    n.beat = 0.0;
    n.length = 2.0;
    n.pitch = 64;
    n.effect = LineNote::Effect::scoop;
    n.amount = 12.0f;
    const auto c = Studio::effectControllers ({ n }, 12, 120.0);
    REQUIRE (c.size() == 2);
    CHECK (c[0].value == 0);                                      // (an octave under on a range of 12)
    CHECK (std::abs (c[1].start - 0.12) < 1.0e-6);                // (0.06 s at 120 BPM)
    CHECK (c[1].value == 8192);
    n.effect = LineNote::Effect::vibrato;
    n.amount = 1.0f;
    const auto v = Studio::effectControllers ({ n }, 2, 120.0);
    REQUIRE (v.size() == 4);
    CHECK (std::abs (v[1].start - 0.3) < 1.0e-6);                 // (15 % into a two-beat note)
    CHECK (std::abs (v[2].start - 1.3) < 1.0e-6);
}

TEST_CASE ("0.133.0: the Van Halen round's styles have their own habits", "[solo][solostyles][solo133]")
{
    const auto spans = amBars (16);   // (64 beats)
    const auto& tuning = GuitarTuning::all()[0];
    auto onThird = 0, accelerated = 0, stairs = 0, throws = 0, cycles = 0, fourths = 0, scoops = 0, dives = 0;
    for (int variation = 0; variation < 4; ++variation)
    {
        INFO ("variation " << variation);
        // CELL WITH THE CHANGES: the chord's third on chord changes; a dived double stop on the root to end.
        {
            const auto solo = composeSolo (SoloStyle::cellWithTheChanges, spans, 4.0, tuning, {}, variation, 0.0, 110.0);
            for (const auto& n : solo.notes)
                for (const auto& sp : spans)
                    if (std::abs (n.beat - sp.start) < 1.0e-6)
                    {
                        const auto third = (sp.chord.root + (sp.chord.type == T::minor ? 3 : 4)) % 12;
                        onThird += ((n.pitch % 12) + 12) % 12 == third ? 1 : 0;
                    }
            const auto& last = lastOf (solo);
            CHECK (last.effect == LineNote::Effect::dive);
            auto atLast = 0;
            for (const auto& n : solo.notes)
                atLast += std::abs (n.beat - last.beat) < 1.0e-6 && n.effect == LineNote::Effect::dive ? 1 : 0;
            CHECK (atLast == 2);
        }
        // HOLD & ACCELERATE: in on a harmonic scooped up into; one note shaken, then struck again and again; harmonics
        // dived away at the end.
        {
            const auto solo = composeSolo (SoloStyle::holdAndAccelerate, spans, 4.0, tuning, {}, variation, 0.0, 140.0);
            CHECK (solo.notes.front().beat == 0.0);
            CHECK (solo.notes.front().technique == LineNote::Technique::harmonic);
            CHECK (solo.notes.front().effect == LineNote::Effect::scoop);
            for (size_t i = 0; i < solo.notes.size(); ++i)
                if (solo.notes[i].effect == LineNote::Effect::wobble && solo.notes[i].length >= 1.5)
                {
                    auto same = 0;
                    for (size_t k = i + 1; k < solo.notes.size() && solo.notes[k].pitch == solo.notes[i].pitch; ++k)
                        ++same;
                    accelerated += same >= 6 ? 1 : 0;
                }
            auto harmonicDives = 0;
            for (const auto& n : solo.notes)
                harmonicDives += n.beat >= 60.0 - 1.0e-6 && n.technique == LineNote::Technique::harmonic && n.effect == LineNote::Effect::dive ? 1 : 0;
            CHECK (harmonicDives >= 2);
            CHECK (lastOf (solo).effect == LineNote::Effect::dive);
        }
        // WHAMMY FIRE: staircases of repeated notes, the whammy scooping in and diving out.
        {
            const auto solo = composeSolo (SoloStyle::whammyFire, spans, 4.0, tuning, {}, variation, 0.0, 200.0);
            auto run = 1;
            for (size_t i = 1; i < solo.notes.size(); ++i)
            {
                run = solo.notes[i].pitch == solo.notes[i - 1].pitch ? run + 1 : 1;
                stairs += run == 3 ? 1 : 0;
            }
            scoops += countOf (solo, LineNote::Effect::scoop);
            dives += countOf (solo, LineNote::Effect::dive);
        }
        // LEGATO THROWS: a pickup two octaves under; a run up into the last held note.
        {
            const auto solo = composeSolo (SoloStyle::legatoThrows, spans, 4.0, tuning, {}, variation, 0.0, 132.0);
            for (size_t i = 1; i < solo.notes.size(); ++i)
                throws += std::abs (solo.notes[i].pitch - solo.notes[i - 1].pitch) >= 19 ? 1 : 0;
            const auto& last = lastOf (solo);
            CHECK (last.beat == 63.0);
            CHECK (onsetsIn (solo, 60.0, 63.0) >= 6);
        }
        // SQUEALS & SEXTUPLETS: in on a harmonic; three-note cells cycled.
        {
            const auto solo = composeSolo (SoloStyle::squealsAndSextuplets, spans, 4.0, tuning, {}, variation, 0.0, 105.0);
            CHECK (solo.notes.front().technique == LineNote::Technique::harmonic);
            const auto& ns = solo.notes;
            for (size_t i = 0; i + 5 < ns.size(); ++i)
                if (ns[i].pitch == ns[i + 3].pitch && ns[i + 1].pitch == ns[i + 4].pitch && ns[i + 2].pitch == ns[i + 5].pitch
                    && ns[i].pitch > ns[i + 1].pitch && ns[i + 1].pitch > ns[i + 2].pitch)
                    ++cycles;
        }
        // CHIMING FOURTHS: fourths struck together; bends let down; a bend held to end.
        {
            const auto solo = composeSolo (SoloStyle::chimingFourths, spans, 4.0, tuning, {}, variation, 0.0, 96.0);
            for (size_t i = 1; i < solo.notes.size(); ++i)
                fourths += solo.notes[i].beat == solo.notes[i - 1].beat && std::abs (solo.notes[i].pitch - solo.notes[i - 1].pitch) == 5 ? 1 : 0;
            CHECK (countOf (solo, LineNote::Effect::release) >= 1);
            CHECK (lastOf (solo).effect == LineNote::Effect::bend);
        }
    }
    CHECK (scoops >= 6);
    CHECK (dives >= 6);
    CHECK (onThird >= 8);
    CHECK (accelerated >= 2);
    CHECK (stairs >= 8);
    CHECK (throws >= 2);
    CHECK (cycles >= 8);
    CHECK (fourths >= 12);
    // Phrases run into each other: hardly a rest of over a beat in these styles.
    for (const auto style : { SoloStyle::cellWithTheChanges, SoloStyle::holdAndAccelerate, SoloStyle::whammyFire,
                              SoloStyle::legatoThrows, SoloStyle::squealsAndSextuplets, SoloStyle::chimingFourths,
                              SoloStyle::dottedBendChain, SoloStyle::bentRiffSolo, SoloStyle::tremoloPedalOutro, SoloStyle::burstAndBend })
    {
        INFO (soloStyleName (style));
        auto rests = 0;
        for (int variation = 0; variation < 4; ++variation)
        {
            const auto solo = composeSolo (style, spans, 4.0, tuning, {}, variation, 0.0, 120.0);
            auto reach = 0.0;
            for (const auto& n : solo.notes)
            {
                rests += n.beat - reach > 1.0 + 1.0e-6 ? 1 : 0;
                reach = juce::jmax (reach, n.beat + n.length);
            }
        }
        CHECK (rests <= 8);   // (of four 16-bar solos: two at most in each)
    }
}

TEST_CASE ("0.133.0: round two's styles have their own habits", "[solo][solostyles][solo133]")
{
    const auto spans = amBars (16);   // (64 beats)
    const auto& tuning = GuitarTuning::all()[0];
    auto dotted = 0, falls = 0, thirdsOnTop = 0, tremolo = 0, dyadRepeats = 0;
    auto noThirdOk = true;
    for (int variation = 0; variation < 4; ++variation)
    {
        INFO ("variation " << variation);
        // DOTTED BEND CHAIN: bends struck a dotted quarter apart; a chromatic fall.
        {
            const auto solo = composeSolo (SoloStyle::dottedBendChain, spans, 4.0, tuning, {}, variation, 0.0, 105.0);
            std::vector<double> bends;
            for (const auto& n : solo.notes)
                if (n.effect == LineNote::Effect::bend)
                    bends.push_back (n.beat);
            for (size_t i = 1; i < bends.size(); ++i)
                dotted += std::abs (bends[i] - bends[i - 1] - 1.5) < 1.0e-6 ? 1 : 0;
            auto run = 1;
            for (size_t i = 1; i < solo.notes.size(); ++i)
            {
                run = solo.notes[i].pitch == solo.notes[i - 1].pitch - 1 ? run + 1 : 1;
                falls += run == 6 ? 1 : 0;
            }
        }
        // HOOK, THIRDS, HOOK: major thirds with the chord's third on top in its middle; nothing faster than 16ths.
        {
            const auto solo = composeSolo (SoloStyle::hookThirdsHook, spans, 4.0, tuning, {}, variation, 0.0, 89.0);
            for (size_t i = 1; i < solo.notes.size(); ++i)
            {
                const auto& a = solo.notes[i - 1];
                const auto& b = solo.notes[i];
                if (a.beat == b.beat && a.pitch - b.pitch == 4 && std::abs (a.beat - std::round (a.beat)) < 1.0e-6)
                {
                    const auto& c = spans[(size_t) juce::jlimit (0, 15, (int) (a.beat / 4.0))].chord;
                    thirdsOnTop += ((a.pitch % 12) + 12) % 12 == (c.root + (c.type == T::minor ? 3 : 4)) % 12 ? 1 : 0;
                }
            }
            std::vector<double> starts;
            for (const auto& n : solo.notes)
                if (starts.empty() || n.beat - starts.back() > 1.0e-6)
                    starts.push_back (n.beat);
            for (size_t i = 1; i < starts.size(); ++i)
                CHECK (starts[i] - starts[i - 1] >= 0.25 - 1.0e-6);
        }
        // BENT RIFF SOLO: the key's 1 2 4 5 b7 only (A: no C, C#, F#, G#), bends and fourths.
        {
            const auto solo = composeSolo (SoloStyle::bentRiffSolo, spans, 4.0, tuning, {}, variation, 0.0, 140.0);
            std::set<int> sounded;
            for (size_t i = 0; i + 1 < solo.notes.size(); ++i)   // (the last note: the last chord's root, E)
            {
                const auto& n = solo.notes[i];
                if (n.length < 0.75 || n.effect != LineNote::Effect::none)
                    continue;
                sounded.insert (((n.pitch % 12) + 12) % 12);
            }
            juce::ignoreUnused (sounded);
            auto inBox = 0, all = 0;
            for (const auto& n : solo.notes)
                if (n.effect == LineNote::Effect::none && n.length < 0.4)
                {
                    ++all;
                    const auto pc = ((n.pitch % 12) + 12) % 12;
                    inBox += pc == 9 || pc == 11 || pc == 2 || pc == 4 || pc == 7 ? 1 : 0;
                }
            noThirdOk = noThirdOk && inBox * 10 >= all * 7;
            CHECK (countOf (solo, LineNote::Effect::bend) >= 6);
        }
        // TREMOLO PEDAL OUTRO: one note picked over and over; held notes stepping down to end, each in a beat early.
        {
            const auto solo = composeSolo (SoloStyle::tremoloPedalOutro, spans, 4.0, tuning, {}, variation, 0.0, 120.0);
            auto run = 1;
            for (size_t i = 1; i < solo.notes.size(); ++i)
            {
                run = solo.notes[i].pitch == solo.notes[i - 1].pitch && solo.notes[i].length <= 0.25 + 1.0e-6 ? run + 1 : 1;
                tremolo += run == 16 ? 1 : 0;
            }
            std::vector<LineNote> tail;
            for (const auto& n : solo.notes)
                if (n.beat >= 64.0 - 4.0 * 4.0 - 1.0 - 1.0e-6)
                    tail.push_back (n);
            REQUIRE (tail.size() >= 4);
            for (size_t i = 1; i < tail.size(); ++i)
            {
                CHECK (std::abs (std::fmod (tail[i].beat, 4.0) - 3.0) < 1.0e-6);
                CHECK (tail[i].pitch < tail[i - 1].pitch);
            }
        }
        // BURST AND BEND: in on a scream; root-and-fifth double stops struck again and again; a dive to end.
        {
            const auto solo = composeSolo (SoloStyle::burstAndBend, spans, 4.0, tuning, {}, variation, 0.0, 141.0);
            CHECK (solo.notes.front().technique == LineNote::Technique::harmonic);
            for (size_t i = 3; i < solo.notes.size(); ++i)
                dyadRepeats += solo.notes[i].beat == solo.notes[i - 1].beat && solo.notes[i - 1].pitch - solo.notes[i].pitch == 5
                            && solo.notes[i - 3].pitch == solo.notes[i - 1].pitch && solo.notes[i - 2].pitch == solo.notes[i].pitch ? 1 : 0;
            CHECK (lastOf (solo).effect == LineNote::Effect::dive);
        }
    }
    CHECK (dotted >= 8);
    CHECK (falls >= 2);
    CHECK (thirdsOnTop >= 8);
    CHECK (noThirdOk);
    CHECK (tremolo >= 2);
    CHECK (dyadRepeats >= 8);
}

// 0.134.0: nine styles from the owner's Pantera files (shapes only; our names).
TEST_CASE ("0.134.0: the pull-up and the bend into a dive", "[solo][solostyles][solo134]")
{
    LineNote n;
    n.beat = 0.0;
    n.length = 4.0;
    n.pitch = 52;
    n.effect = LineNote::Effect::pullUp;
    n.amount = 7.0f;
    const auto up = Studio::effectControllers ({ n }, 12, 120.0);
    REQUIRE (up.size() == 4);
    CHECK (up[1].value == 8192);
    CHECK (std::abs (up[1].start - 0.3) < 1.0e-6);                      // (0.15 s at 120 BPM)
    CHECK (up[2].value == 8192 + juce::roundToInt (7.0 / 12.0 * 8191.0));
    CHECK (up[3].value == 8192);
    n.effect = LineNote::Effect::bendDive;
    n.amount = 12.0f;
    const auto dive = Studio::effectControllers ({ n }, 12, 120.0);
    REQUIRE (dive.size() == 5);
    CHECK (dive[0].value < 8192);                                       // (bent into the note from under it)
    CHECK (dive[1].value == 8192);
    CHECK (dive[3].value == 0);                                         // (an octave down on a range of 12)
    CHECK (std::abs (dive[3].start - 3.8) < 1.0e-6);
}

TEST_CASE ("0.134.0: the Pantera round's styles have their own habits", "[solo][solostyles][solo134]")
{
    const auto spans = amBars (16);   // (64 beats)
    const auto& tuning = GuitarTuning::all()[0];
    auto squeals = 0, dimCells = 0, climbs = 0, loops = 0, chromatic = 0, sameBends = 0, releases = 0, pedals = 0;
    for (int variation = 0; variation < 4; ++variation)
    {
        INFO ("variation " << variation);
        {
            const auto solo = composeSolo (SoloStyle::burstBlocksSqueals, spans, 4.0, tuning, {}, variation, 0.0, 142.0);
            squeals += countOf (solo, LineNote::Effect::pullUp);
            for (size_t i = 2; i < solo.notes.size(); ++i)
                dimCells += solo.notes[i].pitch == solo.notes[i - 1].pitch + 3 && solo.notes[i - 1].pitch == solo.notes[i - 2].pitch + 3 ? 1 : 0;
            // The busy passages start on four-bar lines.
            std::set<int> busyBars;
            for (int bar = 0; bar < 16; ++bar)
                if (onsetsIn (solo, bar * 4.0, bar * 4.0 + 4.0) >= 16)
                    busyBars.insert (bar);
            for (const auto bar : busyBars)
                if (busyBars.count (bar - 1) == 0)
                    CHECK (bar % 4 == 0);
        }
        {
            const auto solo = composeSolo (SoloStyle::bendsThenClimb, spans, 4.0, tuning, {}, variation, 0.0, 160.0);
            // Calm for most of it, busy late.
            CHECK (onsetsIn (solo, 0.0, 40.0) < onsetsIn (solo, 44.0, 64.0));
            CHECK (lastOf (solo).effect == LineNote::Effect::bendDive);
            // Cells climbing a scale step a beat.
            for (size_t i = 6; i < solo.notes.size(); ++i)
                climbs += solo.notes[i].pitch > solo.notes[i - 3].pitch && solo.notes[i - 3].pitch > solo.notes[i - 6].pitch
                       && solo.notes[i].beat - solo.notes[i - 3].beat <= 1.0 + 1.0e-6 ? 1 : 0;   // (a cell a beat or two: the grid follows the tempo)
        }
        {
            const auto solo = composeSolo (SoloStyle::bottomToTopClimb, spans, 4.0, tuning, {}, variation, 0.0, 125.0);
            const auto& last = lastOf (solo);
            CHECK (last.effect == LineNote::Effect::bendDive);
            CHECK (last.pitch >= tuning.strings.back() + 10);
            // The bend after three pulled-off notes, a beat at a time.
            for (size_t i = 3; i < solo.notes.size(); ++i)
                loops += solo.notes[i].effect == LineNote::Effect::bend && solo.notes[i - 1].pitch < solo.notes[i - 2].pitch
                      && solo.notes[i - 2].pitch < solo.notes[i - 3].pitch ? 1 : 0;
        }
        {
            const auto solo = composeSolo (SoloStyle::balladIntoFire, spans, 4.0, tuning, {}, variation, 0.0, 135.0);
            CHECK (onsetsIn (solo, 0.0, 28.0) * 2 < onsetsIn (solo, 32.0, 60.0));
            for (size_t i = 3; i < solo.notes.size(); ++i)
                chromatic += solo.notes[i].pitch == solo.notes[i - 3].pitch - 1 && std::abs (solo.notes[i].beat - solo.notes[i - 3].beat - 1.0) < 1.0e-6 ? 1 : 0;
        }
        {
            const auto solo = composeSolo (SoloStyle::slowStaircaseBends, spans, 4.0, tuning, {}, variation, 0.0, 103.0);
            auto run = 1;
            for (size_t i = 1; i < solo.notes.size(); ++i)
            {
                const auto& a = solo.notes[i - 1];
                const auto& b = solo.notes[i];
                run = a.effect == LineNote::Effect::bend && b.effect == LineNote::Effect::bend && a.amount == 3.0f && b.amount == 3.0f && a.pitch == b.pitch ? run + 1 : 1;
                sameBends += run == 4 ? 1 : 0;
            }
        }
        {
            const auto solo = composeSolo (SoloStyle::halfSpeedBentCells, spans, 4.0, tuning, {}, variation, 0.0, 60.0);
            releases += countOf (solo, LineNote::Effect::release);
        }
        {
            const auto solo = composeSolo (SoloStyle::pedalClimbTremolo, spans, 4.0, tuning, {}, variation, 0.0, 137.0);
            // Ends on the root with the fifth over it, held, after tremolo dyads.
            const auto& last = lastOf (solo);
            CHECK (((last.pitch % 12) + 12) % 12 == 4);
            CHECK (onsetsIn (solo, 56.0, 60.0) >= 16);
            for (size_t i = 2; i < solo.notes.size(); i += 1)
                pedals += solo.notes[i].pitch == solo.notes[i - 2].pitch && solo.notes[i - 1].pitch > solo.notes[i].pitch
                       && solo.notes[i].beat < 48.0 ? 1 : 0;
        }
        {
            const auto solo = composeSolo (SoloStyle::burstBarBurst, spans, 4.0, tuning, {}, variation, 0.0, 74.0);
            CHECK (countOf (solo, LineNote::Effect::wobble) + countOf (solo, LineNote::Effect::pullUp) >= 2);
            CHECK (lastOf (solo).effect == LineNote::Effect::bend);
        }
    }
    CHECK (squeals >= 8);
    CHECK (dimCells >= 16);
    CHECK (climbs >= 8);
    CHECK (loops >= 8);
    CHECK (chromatic >= 4);
    CHECK (sameBends >= 2);
    CHECK (releases >= 8);
    CHECK (pedals >= 16);
}

TEST_CASE ("0.134.0: the rest of the Pantera ideas have their own habits", "[solo][solostyles][solo134]")
{
    const auto spans = amBars (16);   // (64 beats)
    const auto& tuning = GuitarTuning::all()[0];
    auto tritoneCells = 0, dimTriplets = 0, harmonies = 0, tenths = 0, scoops = 0, rises = 0, earlyEntries = 0;
    for (int variation = 0; variation < 4; ++variation)
    {
        INFO ("variation " << variation);
        {
            // TRITONE STAIRCASE: four bars of held notes from beat 2, each a tritone over the one before.
            const auto solo = composeSolo (SoloStyle::tritoneStaircase, spans, 4.0, tuning, {}, variation, 0.0, 120.0);
            std::vector<int> held;
            for (const auto& n : solo.notes)
                if (n.beat < 16.0 && std::abs (std::fmod (n.beat, 4.0) - 1.0) < 1.0e-6 && n.length >= 2.5)
                    held.push_back (n.pitch);
            REQUIRE (held.size() == 4);
            for (size_t i = 1; i < held.size(); ++i)
                CHECK (held[i] - held[i - 1] == 6);
            for (size_t i = 2; i < solo.notes.size(); ++i)
                tritoneCells += solo.notes[i].pitch == solo.notes[i - 1].pitch + 4 && solo.notes[i - 1].pitch == solo.notes[i - 2].pitch + 1 ? 1 : 0;
        }
        {
            // SHUFFLE SNARL: diminished triplets; swung; a bend into a dive to end.
            const auto solo = composeSolo (SoloStyle::shuffleSnarl, spans, 4.0, tuning, {}, variation, 0.0, 120.0);
            for (size_t i = 2; i < solo.notes.size(); ++i)
                dimTriplets += solo.notes[i].pitch == solo.notes[i - 1].pitch + 3 && solo.notes[i - 1].pitch == solo.notes[i - 2].pitch + 3
                            && std::abs (solo.notes[i].beat - solo.notes[i - 1].beat - 1.0 / 3.0) < 1.0e-6 ? 1 : 0;
            CHECK (lastOf (solo).effect == LineNote::Effect::bendDive);
        }
        {
            // BREAKDOWN TRILL: harmony notes a minor third under.
            const auto solo = composeSolo (SoloStyle::breakdownTrill, spans, 4.0, tuning, {}, variation, 0.0, 82.0);
            for (size_t i = 1; i < solo.notes.size(); ++i)
                harmonies += solo.notes[i].harmony && solo.notes[i].beat == solo.notes[i - 1].beat
                          && solo.notes[i - 1].pitch - solo.notes[i].pitch == 3 ? 1 : 0;
        }
        {
            // SLOW EASTERN TWINS: a harmony an octave and a third over; long notes scooped from a minor third under.
            const auto solo = composeSolo (SoloStyle::slowEasternTwins, spans, 4.0, tuning, {}, variation, 0.0, 108.0);
            for (const auto& h : solo.notes)
                if (h.harmony)
                    for (const auto& n : solo.notes)
                        if (! n.harmony && n.beat == h.beat && h.pitch - n.pitch >= 15 && h.pitch - n.pitch <= 16)
                            ++tenths;
            for (const auto& n : solo.notes)
                scoops += n.effect == LineNote::Effect::scoop && n.amount == 3.0f ? 1 : 0;
        }
        {
            // HELD LINE & RISING CELLS: notes coming in an eighth before the bar line; cells bent a half step.
            const auto solo = composeSolo (SoloStyle::heldLineRisingCells, spans, 4.0, tuning, {}, variation, 0.0, 120.0);
            for (const auto& n : solo.notes)
            {
                earlyEntries += n.beat < 32.0 && std::abs (std::fmod (n.beat, 4.0) - 3.5) < 1.0e-6 && n.length >= 3.0 ? 1 : 0;
                rises += n.effect == LineNote::Effect::bend && n.amount == 1.0f ? 1 : 0;
            }
        }
        {
            // STATED, RAISED, DROPPED: silence after the first phrase; the last quarter low; dived to end.
            const auto solo = composeSolo (SoloStyle::statedRaisedDropped, spans, 4.0, tuning, {}, variation, 0.0, 103.0);
            auto longGap = false;
            auto reach = 0.0;
            for (const auto& n : solo.notes)
            {
                longGap = longGap || (n.beat < 24.0 && n.beat - reach >= 3.0);
                reach = juce::jmax (reach, n.beat + n.length);
            }
            CHECK (longGap);
            CHECK (medianPitch (solo, 50.0, 62.0) < medianPitch (solo, 16.0, 40.0));
            CHECK (lastOf (solo).effect == LineNote::Effect::dive);
        }
    }
    CHECK (tritoneCells >= 16);
    CHECK (dimTriplets >= 8);
    CHECK (harmonies >= 40);
    CHECK (tenths >= 8);
    CHECK (scoops >= 4);
    CHECK (rises >= 8);
    CHECK (earlyEntries >= 12);
}


// 0.135.0: the owner's Oasis, Motley Crue and Dream Theater files (read for their solos' shapes only; our names).
namespace
{
    int longestGapRun (const LineIdea& idea, double gap, double from = 0.0, double to = 1.0e9)
    {
        auto run = 0, longest = 0;
        for (size_t i = 1; i < idea.notes.size(); ++i)
        {
            if (idea.notes[i].beat < from || idea.notes[i].beat >= to || idea.notes[i].harmony)
                continue;
            run = std::abs (idea.notes[i].beat - idea.notes[i - 1].beat - gap) < 1.0e-6 ? run + 1 : 0;
            longest = juce::jmax (longest, run);
        }
        return longest;
    }

    std::vector<LineNote> notesIn (const LineIdea& idea, double from, double to)
    {
        std::vector<LineNote> out;
        for (const auto& n : idea.notes)
            if (n.beat >= from - 1.0e-9 && n.beat < to - 1.0e-9)
                out.push_back (n);
        return out;
    }

    bool sameShifted (const std::vector<LineNote>& a, const std::vector<LineNote>& b, double shift)
    {
        if (a.size() != b.size() || a.empty())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::abs (a[i].beat + shift - b[i].beat) > 1.0e-6 || a[i].pitch != b[i].pitch || a[i].effect != b[i].effect)
                return false;
        return true;
    }
}

TEST_CASE ("0.135.0: the Oasis round's styles have their own habits", "[solo][solostyles][solo135]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    for (int variation = 0; variation < 3; ++variation)
    {
        INFO ("variation " << variation);
        // TRADED TURNS: the first eight bars the first guitar's, the next eight the second's (an octave down).
        const auto traded = composeSolo (SoloStyle::tradedTurns, spans, 4.0, tuning, {}, variation);
        auto second = 0, firstPitches = 0, secondPitches = 0, firstCount = 0;
        for (const auto& n : traded.notes)
        {
            if (n.harmony)
            {
                ++second;
                CHECK (n.beat >= 31.5 - 1.0e-6);
                secondPitches += n.pitch;
            }
            else
            {
                CHECK (n.beat < 31.5 + 1.0e-6);
                firstPitches += n.pitch;
                ++firstCount;
            }
        }
        REQUIRE (second >= 8);
        REQUIRE (firstCount >= 8);
        CHECK (secondPitches / second < firstPitches / firstCount);
        CHECK (longestGapRun (traded, 0.25) == 0);   // (eighths and quarters)

        // CALL & UNISON CHANT: three bends on beats one, two and three, the call and answer again exactly; the end
        // the top root bent into again and again in eighths.
        const auto chant = composeSolo (SoloStyle::callAndChant, spans, 4.0, tuning, {}, variation);
        for (auto b = 0; b < 3; ++b)
        {
            CHECK (std::abs (chant.notes[(size_t) b].beat - b) < 1.0e-6);
            CHECK (chant.notes[(size_t) b].effect == LineNote::Effect::bend);
            CHECK (chant.notes[(size_t) b].pitch == chant.notes[0].pitch);
        }
        CHECK (sameShifted (notesIn (chant, 0.0, 8.0), notesIn (chant, 8.0, 16.0), 8.0));
        auto struck = 0;
        for (const auto& n : notesIn (chant, 60.0, 64.0))
            struck += n.effect == LineNote::Effect::bend && n.pitch == chant.notes.back().pitch ? 1 : 0;
        CHECK (struck >= 4);

        // FIXED HEAD, NEW ANSWERS: bars 1, 3, 5... the same; the answers between them not.
        const auto head = composeSolo (SoloStyle::fixedHead, spans, 4.0, tuning, {}, variation);
        CHECK (sameShifted (notesIn (head, 0.0, 4.0), notesIn (head, 8.0, 12.0), 8.0));
        CHECK (sameShifted (notesIn (head, 0.0, 4.0), notesIn (head, 16.0, 20.0), 16.0));
        CHECK_FALSE (sameShifted (notesIn (head, 4.0, 8.0), notesIn (head, 12.0, 16.0), 8.0));
        CHECK (head.notes.front().effect == LineNote::Effect::bend);
    }
}

TEST_CASE ("0.135.0: the Motley Crue round's styles have their own habits", "[solo][solostyles][solo135]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    for (int variation = 0; variation < 3; ++variation)
    {
        INFO ("variation " << variation);
        // SHUFFLE TRADE: triplet shuffle, the second guitar's two-bar turns (beats 8 to 16) lower.
        const auto shuffle = composeSolo (SoloStyle::shuffleTrade, spans, 4.0, tuning, {}, variation);
        auto secondTurn = 0;
        for (const auto& n : shuffle.notes)
        {
            CHECK ((std::abs (n.beat * 3.0 - std::round (n.beat * 3.0)) < 0.02 || std::abs (n.beat * 2.0 - std::round (n.beat * 2.0)) < 0.02));
            if (n.harmony)
            {
                CHECK (((n.beat >= 7.5 && n.beat < 16.5) || (n.beat >= 31.5 && n.beat < 48.5) || n.beat >= 55.5));
                ++secondTurn;
            }
        }
        CHECK (secondTurn >= 6);

        // SEXTUPLET LOOPS & DIVES: wide-shaken pre-bends first, a six-note cell looped, the low root dived.
        const auto loops = composeSolo (SoloStyle::sextupletLoopsDives, spans, 4.0, tuning, {}, variation);
        CHECK (loops.notes.front().effect == LineNote::Effect::wobble);
        auto looped = 0, longestLoop = 0;
        for (size_t i = 6; i < loops.notes.size(); ++i)
        {
            looped = loops.notes[i].pitch == loops.notes[i - 6].pitch && std::abs (loops.notes[i].beat - loops.notes[i - 1].beat - 1.0 / 6.0) < 1.0e-6
                   ? looped + 1 : 0;
            longestLoop = juce::jmax (longestLoop, looped);
        }
        CHECK (longestLoop >= 12);
        CHECK (loops.notes.back().effect == LineNote::Effect::dive);
        CHECK (loops.notes.back().amount == 7.0f);

        // PICKED HYMN: each theme note picked four times (runs of four of a pitch in sixteenths), octaves, the root in
        // octaves with the bar pulsing.
        const auto hymn = composeSolo (SoloStyle::pickedHymn, spans, 4.0, tuning, {}, variation);
        auto picked = 0;
        for (size_t i = 3; i < hymn.notes.size(); ++i)
        {
            const auto& a = hymn.notes[i - 3];
            const auto& b = hymn.notes[i];
            picked += ! a.harmony && ! b.harmony && a.pitch == b.pitch && std::abs (b.beat - a.beat - 0.75) < 1.0e-6
                      && std::abs (a.beat - std::round (a.beat)) < 1.0e-6 ? 1 : 0;
        }
        CHECK (picked >= 18);
        CHECK (std::count_if (hymn.notes.begin(), hymn.notes.end(), [] (const LineNote& n) { return n.harmony; }) >= 20);
        CHECK (hymn.notes.back().effect == LineNote::Effect::wobble);

        // ONE TRICK A BLOCK: a slow first half; later the same note picked in thirty-seconds; let down at the end.
        const auto trick = composeSolo (SoloStyle::oneTrickABlock, spans, 4.0, tuning, {}, variation);
        CHECK (onsetsIn (trick, 0.0, 32.0) <= 40);
        auto tremolo = 0;
        for (size_t i = 1; i < trick.notes.size(); ++i)
            tremolo += trick.notes[i].pitch == trick.notes[i - 1].pitch && std::abs (trick.notes[i].beat - trick.notes[i - 1].beat - 0.125) < 1.0e-6 ? 1 : 0;
        CHECK (tremolo >= 8);
        CHECK (trick.notes.back().effect == LineNote::Effect::release);

        // TURNS DOWN THE SCALE: an eighth, then the note above, the note, the note under in triplet sixteenths.
        const auto turns = composeSolo (SoloStyle::turnsDownTheScale, spans, 4.0, tuning, {}, variation);
        auto turned = 0;
        for (size_t i = 0; i + 3 < turns.notes.size(); ++i)
        {
            const auto& a = turns.notes[i];
            turned += std::abs (turns.notes[i + 1].beat - a.beat - 0.5) < 1.0e-6 && std::abs (turns.notes[i + 2].beat - a.beat - 2.0 / 3.0) < 1.0e-6
                      && turns.notes[i + 1].pitch > a.pitch && turns.notes[i + 2].pitch == a.pitch && turns.notes[i + 3].pitch < a.pitch ? 1 : 0;
        }
        CHECK (turned >= 4);

        // OCTAVE BALLAD: no bends at all; the restated phrase in octaves; four quarters walking up into the root.
        const auto ballad = composeSolo (SoloStyle::octaveBallad, spans, 4.0, tuning, {}, variation);
        for (const auto& n : ballad.notes)
        {
            CHECK (n.effect != LineNote::Effect::bend);
            CHECK (n.effect != LineNote::Effect::release);
            CHECK (n.effect != LineNote::Effect::slowBend);
        }
        CHECK (std::count_if (ballad.notes.begin(), ballad.notes.end(), [] (const LineNote& n) { return n.harmony; }) >= 3);
        const auto walk = notesIn (ballad, 60.0, 64.0);
        REQUIRE (walk.size() == 4);
        for (size_t i = 1; i < walk.size(); ++i)
            CHECK (walk[i].pitch > walk[i - 1].pitch);

        // STEPPING BEND CALLS: bends on beats two and four; a minor-third trill.
        const auto calls = composeSolo (SoloStyle::steppingBendCalls, spans, 4.0, tuning, {}, variation);
        auto onTwoAndFour = 0, trill = 0;
        for (const auto& n : calls.notes)
            onTwoAndFour += n.effect == LineNote::Effect::bend && (std::abs (std::fmod (n.beat, 4.0) - 1.0) < 1.0e-6 || std::abs (std::fmod (n.beat, 4.0) - 3.0) < 1.0e-6) ? 1 : 0;
        for (size_t i = 1; i < calls.notes.size(); ++i)
            trill += std::abs (calls.notes[i].pitch - calls.notes[i - 1].pitch) == 3 && calls.notes[i].beat - calls.notes[i - 1].beat <= 0.5 + 1.0e-6 ? 1 : 0;
        CHECK (onTwoAndFour >= 8);
        CHECK (trill >= 6);
    }
}

TEST_CASE ("0.135.0: the Dream Theater round's styles have their own habits", "[solo][solostyles][solo135]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    auto halfBends = 0, wholeBends = 0;
    for (int variation = 0; variation < 4; ++variation)
    {
        INFO ("variation " << variation);
        // CONTOUR CELLS: long sextuplet streams; a long top root at the end.
        const auto contour = composeSolo (SoloStyle::contourCells, spans, 4.0, tuning, {}, variation);
        CHECK (longestGapRun (contour, 1.0 / 6.0) >= 24);
        CHECK (contour.notes.back().length >= 4.0 - 1.0e-6);

        // LOOPED CELLS ACROSS THE BEAT: a six-note cell looped exactly; a four-note one across the sextuplets.
        const auto across = composeSolo (SoloStyle::loopedCellsAcross, spans, 4.0, tuning, {}, variation);
        auto period6 = 0, period4 = 0;
        for (size_t i = 6; i < across.notes.size(); ++i)
        {
            const auto sext = std::abs (across.notes[i].beat - across.notes[i - 1].beat - 1.0 / 6.0) < 1.0e-6;
            period6 += sext && across.notes[i].pitch == across.notes[i - 6].pitch ? 1 : 0;
            period4 += sext && across.notes[i].pitch == across.notes[i - 4].pitch && across.notes[i].pitch != across.notes[i - 6].pitch ? 1 : 0;
        }
        CHECK (period6 >= 18);
        CHECK (period4 >= 8);

        // FALLING PEDAL SEXTUPLETS: a low note far under the pair around it, again and again.
        const auto pedal = composeSolo (SoloStyle::fallingPedalSextuplets, spans, 4.0, tuning, {}, variation);
        auto dips = 0;
        for (size_t i = 1; i + 1 < pedal.notes.size(); ++i)
            dips += pedal.notes[i - 1].pitch - pedal.notes[i].pitch >= 12 && pedal.notes[i + 1].pitch - pedal.notes[i].pitch >= 12 ? 1 : 0;
        CHECK (dips >= 8);

        // EASTERN LEGATO RUNS: thirty-second runs; the b2 of each chord's root (Bb over Am, F over E).
        const auto eastern = composeSolo (SoloStyle::easternLegatoRuns, spans, 4.0, tuning, {}, variation);
        CHECK (longestGapRun (eastern, 0.125) >= 16);
        auto flatTwo = 0;
        for (const auto& n : eastern.notes)
        {
            const auto bar = (int) (n.beat / 4.0) % 4;
            const auto pc = ((n.pitch % 12) + 12) % 12;
            flatTwo += (bar == 0 && pc == 10) || (bar == 3 && pc == 5) ? 1 : 0;
        }
        CHECK (flatTwo >= 3);

        // CRYING HALF STEPS: half-step bends more often than whole steps; held notes fallen away seven semitones.
        const auto crying = composeSolo (SoloStyle::cryingHalfSteps, spans, 4.0, tuning, {}, variation);
        for (const auto& n : crying.notes)
            if (n.effect == LineNote::Effect::bend)
                (n.amount < 1.5f ? halfBends : wholeBends) += 1;

        // FLOATING LINE: held notes tied over the bar lines.
        const auto floating = composeSolo (SoloStyle::floatingLine, spans, 4.0, tuning, {}, variation);
        auto tied = 0;
        for (const auto& n : floating.notes)
            tied += n.length >= 1.0 && std::floor (n.beat / 4.0) < std::floor ((n.beat + n.length - 0.05) / 4.0) && std::fmod (n.beat, 4.0) > 1.0e-6 ? 1 : 0;
        CHECK (tied >= 5);

        // ADD-NINE MELODY: a calm first third.
        const auto nine = composeSolo (SoloStyle::addNineMelody, spans, 4.0, tuning, {}, variation);
        CHECK (onsetsIn (nine, 0.0, 24.0) <= 26);

        // GEAR-SHIFT STREAM: from halfway sixteenths, then sextuplets, then thirty-seconds.
        const auto stream = composeSolo (SoloStyle::gearShiftStream, spans, 4.0, tuning, {}, variation);
        const auto firstGap = [&stream] (double gap)
        {
            for (size_t i = 1; i < stream.notes.size(); ++i)
                if (stream.notes[i].beat >= 32.0 && std::abs (stream.notes[i].beat - stream.notes[i - 1].beat - gap) < 1.0e-6)
                    return stream.notes[i].beat;
            return 1.0e9;
        };
        CHECK (firstGap (0.25) < firstGap (1.0 / 6.0));
        CHECK (firstGap (1.0 / 6.0) < firstGap (0.125));
        CHECK (firstGap (0.125) < 64.0);

        // BAR OSTINATO: the first four bars the same bar of sixteenths.
        const auto ostinato = composeSolo (SoloStyle::barOstinato, spans, 4.0, tuning, {}, variation);
        CHECK (notesIn (ostinato, 0.0, 4.0).size() == 16);
        CHECK (sameShifted (notesIn (ostinato, 0.0, 4.0), notesIn (ostinato, 12.0, 16.0), 12.0));

        // TRIAD CELL SWEEPS: the chord's three notes up and back down in sextuplets.
        const auto triads = composeSolo (SoloStyle::triadCellSweeps, spans, 4.0, tuning, {}, variation);
        auto cells = 0;
        for (size_t i = 0; i + 5 < triads.notes.size(); ++i)
        {
            const auto& t = triads.notes;
            cells += t[i].pitch < t[i + 1].pitch && t[i + 1].pitch < t[i + 2].pitch && t[i + 2].pitch == t[i + 3].pitch
                     && t[i + 4].pitch == t[i + 1].pitch && t[i + 5].pitch == t[i].pitch
                     && std::abs (t[i + 5].beat - t[i].beat - 5.0 / 6.0) < 1.0e-6 ? 1 : 0;
        }
        CHECK (cells >= 6);

        // HELD OVER A FALLING BASS: the last bars held notes falling a step a bar, each let down from a bend.
        const auto falling = composeSolo (SoloStyle::heldOverFallingBass, spans, 4.0, tuning, {}, variation);
        const auto last = notesIn (falling, 44.0, 64.0);
        REQUIRE (last.size() >= 3);
        for (size_t i = 1; i < last.size(); ++i)
        {
            CHECK (last[i].effect == LineNote::Effect::release);
            CHECK (last[i].pitch < last[i - 1].pitch);
        }
    }
    CHECK (halfBends > wholeBends);   // (CRYING HALF STEPS: half steps twice as often as whole steps, as the files)
}

// 0.136.0: the owner's Stone Temple Pilots files (read for their solos' shapes only; our names).
TEST_CASE ("0.136.0: the Stone Temple Pilots round's styles have their own habits", "[solo][solostyles][solo136]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    auto minorThirdsFromFifth = 0, halfFromSixth = 0;
    for (int variation = 0; variation < 4; ++variation)
    {
        INFO ("variation " << variation);
        // HELD-BEND TALKER: bends sized by the note they start on (in A minor: the fifth E bent a minor third, the
        // sixth F# a half step); octave double stops.
        const auto talker = composeSolo (SoloStyle::heldBendTalker, spans, 4.0, tuning, {}, variation);
        for (const auto& n : talker.notes)
            if (n.effect == LineNote::Effect::bend)
            {
                const auto from = ((n.pitch % 12) + 12) % 12;
                minorThirdsFromFifth += from == 4 && n.amount == 3.0f ? 1 : 0;
                halfFromSixth += from == 6 && n.amount == 1.0f ? 1 : 0;
            }

        // SPRINT, BEND, RUN DOWN: four sixteenths leaping up into a bend struck again and again; two stabs to end.
        const auto sprint = composeSolo (SoloStyle::sprintBendRunDown, spans, 4.0, tuning, {}, variation);
        auto restruck = 0;
        for (size_t i = 1; i < sprint.notes.size(); ++i)
            restruck += sprint.notes[i].effect == LineNote::Effect::bend && sprint.notes[i - 1].effect == LineNote::Effect::bend
                        && sprint.notes[i].pitch == sprint.notes[i - 1].pitch && std::abs (sprint.notes[i].beat - sprint.notes[i - 1].beat - 0.5) < 1.0e-6 ? 1 : 0;
        CHECK (restruck >= 2);
        CHECK (notesIn (sprint, 62.9, 63.1).size() == 3);
        CHECK (sprint.notes.back().length < 0.3);

        // SLIDE-IN OUTRO MELODY: a grace note under the long notes, vibrato on every long note, small dips.
        const auto outro = composeSolo (SoloStyle::slideInOutro, spans, 4.0, tuning, {}, variation);
        auto graces = 0, dips = 0, plainLong = 0;
        for (size_t i = 0; i + 1 < outro.notes.size(); ++i)
        {
            const auto& n = outro.notes[i];
            graces += n.length < 0.15 && outro.notes[i + 1].pitch == n.pitch + 2 ? 1 : 0;
            dips += n.effect == LineNote::Effect::dive && n.amount == 1.0f ? 1 : 0;
            plainLong += n.length >= 1.0 && n.effect == LineNote::Effect::none ? 1 : 0;
        }
        CHECK (graces >= 2);
        CHECK (dips >= 1);

        // SOUR BENDS & STABS: no third and no flat seven heard (C, G, G# over A); low stabs between phrases; the fifth
        // held over the last bars.
        const auto sour = composeSolo (SoloStyle::sourBendsStabs, spans, 4.0, tuning, {}, variation);
        auto stabs = 0;
        for (const auto& n : sour.notes)
        {
            stabs += n.length == 0.25 && n.pitch < tuning.strings[1] + 5 + 12 ? 1 : 0;
            if (n.beat < 48.0 && n.length > 0.25 && n.length < 1.0)   // (the pickups and runs: landings are chord notes)
            {
                const auto pc = ((n.pitch + (n.effect == LineNote::Effect::bend ? juce::roundToInt (n.amount) : 0)) % 12 + 12) % 12;
                CHECK (pc != 0);    // (C: A minor's third)
            }
        }
        CHECK (stabs >= 6);
        CHECK (sour.notes.back().effect == LineNote::Effect::bend);

        // VALLEY LAUNCH: one high note struck six times, bent wider each time; one low note to finish.
        const auto valley = composeSolo (SoloStyle::valleyLaunch, spans, 4.0, tuning, {}, variation);
        std::vector<float> widening;
        for (size_t i = 0; i < valley.notes.size(); ++i)
            if (valley.notes[i].effect == LineNote::Effect::bend && i > 0 && valley.notes[i - 1].pitch == valley.notes[i].pitch
                && valley.notes[i - 1].effect == LineNote::Effect::bend && valley.notes[i].amount > valley.notes[i - 1].amount)
                widening.push_back (valley.notes[i].amount);
        CHECK (widening.size() >= 2);
        CHECK (valley.notes.back().length <= 1.0 + 1.0e-6);

        // CHROMATIC VAMP FLOW: sixteenth runs by step, a half step bent into the chord on each held bar.
        const auto vamp = composeSolo (SoloStyle::chromaticVampFlow, spans, 4.0, tuning, {}, variation);
        auto heldHalfBends = 0, steps = 0, moves = 0;
        for (size_t i = 1; i < vamp.notes.size(); ++i)
        {
            const auto& n = vamp.notes[i];
            heldHalfBends += n.effect == LineNote::Effect::bend && n.amount == 1.0f && n.length >= 2.0 ? 1 : 0;
            if (std::abs (n.beat - vamp.notes[i - 1].beat - 0.25) < 1.0e-6)
            {
                ++moves;
                steps += std::abs (n.pitch - vamp.notes[i - 1].pitch) <= 3 ? 1 : 0;
            }
        }
        CHECK (heldHalfBends >= 3);
        CHECK (steps * 10 >= moves * 8);

        // RIFF-REGISTER BREAK: low; the last note the flat seven bent slowly into the root.
        const auto riff = composeSolo (SoloStyle::riffRegisterBreak, spans, 4.0, tuning, {}, variation);
        CHECK (riff.notes.back().effect == LineNote::Effect::slowBend);
        CHECK (medianPitch (riff, 0.0, 64.0) <= tuning.strings[1] + 5 + 12);
    }
    CHECK (minorThirdsFromFifth >= 2);
    CHECK (halfFromSixth >= 1);
}

TEST_CASE ("0.162.0: the round-four songs' solo styles have their own habits", "[solo][solostyles][solo162]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    const GuitarVelocities vel;
    const auto ghostVelocity = GuitarVelocity::of (vel.muteLoose);
    const auto together = [] (const LineIdea& idea, int interval)
    {
        auto c = 0;
        for (size_t i = 0; i + 1 < idea.notes.size(); ++i)
            for (size_t k = i + 1; k < idea.notes.size() && idea.notes[k].beat - idea.notes[i].beat < 1.0e-6; ++k)
                c += std::abs (idea.notes[k].pitch - idea.notes[i].pitch) == interval ? 1 : 0;
        return c;
    };
    for (int variation = 0; variation < 4; ++variation)
    {
        INFO ("variation " << variation);
        // THREES ACROSS THE BLOCKS: no bends, double stops, one note picked in sixteenths for a bar.
        {
            const auto solo = composeSolo (SoloStyle::threesAcrossBlocks, spans, 4.0, tuning, vel, variation);
            auto bends = 0, longestRepeat = 0, repeat = 0;
            for (size_t i = 0; i < solo.notes.size(); ++i)
            {
                bends += solo.notes[i].effect == LineNote::Effect::bend ? 1 : 0;
                repeat = i > 0 && solo.notes[i].pitch == solo.notes[i - 1].pitch && solo.notes[i].length <= 0.25 + 1.0e-6 ? repeat + 1 : 0;
                longestRepeat = juce::jmax (longestRepeat, repeat);
            }
            CHECK (bends == 0);
            CHECK (together (solo, 3) + together (solo, 4) + together (solo, 5) >= 4);
            CHECK (longestRepeat >= 12);
        }
        // RIFF-TALK CALL & STAB: fourth dyads with ghost hits, then quarters on the off-beats.
        {
            const auto solo = composeSolo (SoloStyle::riffTalkCallStab, spans, 4.0, tuning, vel, variation);
            auto ghosts = 0, offBeats = 0;
            for (const auto& n : solo.notes)
            {
                ghosts += n.velocity == ghostVelocity ? 1 : 0;
                offBeats += n.beat >= 32.0 && std::abs (n.beat - std::floor (n.beat) - 0.5) < 1.0e-6 && n.length > 0.9 ? 1 : 0;
            }
            CHECK (together (solo, 5) >= 6);
            CHECK (ghosts >= 6);
            CHECK (offBeats >= 8);
        }
        // RIFF DOUBLE-STOP, UNISON PEAK: minor-third double stops, a release, a unison bend held.
        {
            const auto solo = composeSolo (SoloStyle::riffDoubleStopUnison, spans, 4.0, tuning, vel, variation);
            auto releases = 0, unison = 0;
            for (const auto& n : solo.notes)
            {
                releases += n.effect == LineNote::Effect::release ? 1 : 0;
                unison += n.harmony && n.length >= 2.5 ? 1 : 0;
            }
            CHECK (together (solo, 3) >= 8);
            CHECK (releases >= 2);
            CHECK (unison >= 2);
            CHECK (solo.notes.back().length >= 3.9);
        }
        // TREMOLO, THEN THE TALK: the first half tremolo-picked (sixteenths or faster), the second half bends.
        {
            const auto solo = composeSolo (SoloStyle::tremoloThenTalk, spans, 4.0, tuning, vel, variation);
            auto fastFirst = 0, bendsSecond = 0;
            for (const auto& n : solo.notes)
            {
                fastFirst += n.beat < 32.0 && n.length <= 0.25 + 1.0e-6 ? 1 : 0;
                bendsSecond += n.beat >= 32.0 && n.effect == LineNote::Effect::bend ? 1 : 0;
            }
            CHECK (fastFirst >= 100);
            CHECK (bendsSecond >= 2);
            // With an instrument whose shortest note is an eighth: one note held with the tremolo key.
            const auto slow = composeSolo (SoloStyle::tremoloThenTalk, spans, 4.0, tuning, vel, variation, 0.5);
            auto tremolo = 0;
            for (const auto& n : slow.notes)
                tremolo += n.technique == LineNote::Technique::tremolo ? 1 : 0;
            CHECK (tremolo >= 4);
        }
        // RIFF STUTTER BREAK: an octave dive and swoop, ghost hits, no rest over half a beat.
        {
            const auto solo = composeSolo (SoloStyle::riffStutterBreak, spans, 4.0, tuning, vel, variation);
            auto dives = 0, scoops = 0, ghosts = 0;
            auto longestGap = 0.0;
            for (size_t i = 0; i < solo.notes.size(); ++i)
            {
                const auto& n = solo.notes[i];
                dives += n.effect == LineNote::Effect::dive && n.amount == 12.0f ? 1 : 0;
                scoops += n.effect == LineNote::Effect::scoop && n.amount == 12.0f ? 1 : 0;
                ghosts += n.velocity == ghostVelocity ? 1 : 0;
                if (i > 0)
                    longestGap = juce::jmax (longestGap, n.beat - (solo.notes[i - 1].beat + solo.notes[i - 1].length));
            }
            CHECK (dives >= 2);
            CHECK (scoops >= 2);
            CHECK (ghosts >= 6);
            CHECK (longestGap <= 0.55);
            CHECK (solo.notes.back().effect == LineNote::Effect::bend);
        }
        // TWIN THIRDS SINGALONG: a second voice from the first note; the opening bend struck three times; the
        // melody stays mostly within an octave and a half.
        {
            const auto solo = composeSolo (SoloStyle::twinThirdsSingalong, spans, 4.0, tuning, vel, variation);
            auto melody = 0, harmony = 0, low = 127, high = 0;
            for (const auto& n : solo.notes)
            {
                (n.harmony ? harmony : melody) += 1;
                if (! n.harmony)
                {
                    low = juce::jmin (low, n.pitch);
                    high = juce::jmax (high, n.pitch);
                }
                if (n.effect == LineNote::Effect::bend)
                    CHECK (n.amount == 2.0f);
            }
            CHECK (harmony >= melody - 2);
            REQUIRE (solo.notes.size() >= 4);
            CHECK (solo.notes[1].harmony);
            CHECK (solo.notes[0].effect == LineNote::Effect::bend);
            CHECK (solo.notes[2].effect == LineNote::Effect::bend);
            CHECK (solo.notes[2].pitch == solo.notes[0].pitch);
            CHECK (high - low <= 19);
            CHECK_FALSE (solo.notes.back().harmony);
        }
        // SWAYING THIRDS THEME: within an octave and a bit, a note or so a beat, the theme back each round.
        {
            const auto solo = composeSolo (SoloStyle::swayingThirdsTheme, spans, 4.0, tuning, vel, variation);
            auto low = 127, high = 0;
            for (const auto& n : solo.notes)
            {
                low = juce::jmin (low, n.pitch);
                high = juce::jmax (high, n.pitch);
            }
            CHECK (high - low <= 15);   // (about an octave: the roots, the pickups under them and the tonic at the end)
            CHECK ((double) solo.notes.size() / 64.0 <= 1.6);
            std::vector<int> first, second;
            for (const auto& n : solo.notes)
            {
                if (n.beat < 12.0) first.push_back (n.pitch);
                else if (n.beat >= 16.0 && n.beat < 28.0) second.push_back (n.pitch);
            }
            CHECK (first == second);
        }
        // STILL CELL, MOVING CHORDS: the cell's pitches the same over A minor and F; a gallop on the dominant (E).
        {
            const auto solo = composeSolo (SoloStyle::stillCellMovingChords, spans, 4.0, tuning, vel, variation);
            std::vector<int> barOne, barTwo;
            std::set<int> onE;
            for (const auto& n : solo.notes)
            {
                if (n.beat < 1.5 - 1.0e-6) barOne.push_back (n.pitch);
                else if (n.beat >= 4.0 && n.beat < 8.0 - 1.0e-6) barTwo.push_back (n.pitch);
                if (n.beat >= 12.0 && n.beat < 16.0) onE.insert (n.pitch);
            }
            CHECK (barOne.size() == 6);
            CHECK (std::set<int> (barOne.begin(), barOne.end()) == std::set<int> (barTwo.begin(), barTwo.end()));
            CHECK (onE.size() <= 2);
        }
    }
    // BY STYLE with the nu-metal guitar picks the eight by the take; rock stays SINGING BENDS.
    std::set<SoloStyle> picked;
    for (int v = 0; v < 16; ++v)
        picked.insert (soloStyleForGuitarStyle (2, v));
    CHECK (picked.size() == 8);
    CHECK (picked.count (SoloStyle::twinThirdsSingalong) == 1);
    CHECK (soloStyleForGuitarStyle (0, 3) == SoloStyle::singingBends);
}

// 0.207.0: round six's solo styles (the owner's songs read for their solos' shapes only; our names).
namespace
{
    const std::vector<SoloStyle>& roundSix()
    {
        static const std::vector<SoloStyle> all { SoloStyle::holdPedalTwinStorm, SoloStyle::panelsOverTheChug, SoloStyle::sequenceThenTwinArpeggios,
                                                  SoloStyle::trillThenHold, SoloStyle::wahWaves, SoloStyle::majorRubOverMinorRiff,
                                                  SoloStyle::octaveLeapsAndCries, SoloStyle::lateCallsBentHome, SoloStyle::cellsUpShakeTheTop,
                                                  SoloStyle::stopTimeDoubleStops, SoloStyle::pickupIntoChordStabs, SoloStyle::bluesCurlsUnderTheTonic,
                                                  SoloStyle::pickupQuarterBreak, SoloStyle::unpluggedPentatonicStream, SoloStyle::tripletHammerFallingTune,
                                                  SoloStyle::tuneAnOctaveDown, SoloStyle::riffOnAWarpedBar, SoloStyle::sungTwiceTurned,
                                                  SoloStyle::seesawAndRing, SoloStyle::fourBendsAndOut };
        return all;
    }

    // Four keys' progressions, a chord a bar: minor i bVI bVII V, major I V vi IV.
    std::vector<ChordSpan> barsIn (int keyChoice, int count)
    {
        struct K { Key key; int tonic; bool minor; };
        const K keys[] = { { { 5, 0, true }, 9, true }, { { 2, 0, true }, 4, true }, { { 0, 0, false }, 0, false }, { { 3, 1, true }, 6, true } };
        const auto& k = keys[keyChoice % 4];
        std::vector<Chord> chords;
        if (k.minor)
            chords = { { k.tonic, T::minor, X::none, -1 }, { (k.tonic + 8) % 12, T::major, X::none, -1 },
                       { (k.tonic + 10) % 12, T::major, X::none, -1 }, { (k.tonic + 7) % 12, T::major, X::none, -1 } };
        else
            chords = { { k.tonic, T::major, X::none, -1 }, { (k.tonic + 7) % 12, T::major, X::none, -1 },
                       { (k.tonic + 9) % 12, T::minor, X::none, -1 }, { (k.tonic + 5) % 12, T::major, X::none, -1 } };
        std::vector<ChordSpan> spans;
        for (int i = 0; i < count; ++i)
            spans.push_back ({ i * 4.0, i * 4.0 + 4.0, chords[(size_t) i % 4], k.key });
        return spans;
    }

    int pcOfPitch (int pitch) { return ((pitch % 12) + 12) % 12; }

    const LineNote& lastOf207 (const LineIdea& idea)
    {
        const LineNote* last = &idea.notes.front();
        for (const auto& n : idea.notes)
            if (n.beat >= last->beat)
                last = &n;
        return *last;
    }

    int notesTogether (const LineIdea& idea, double beat)
    {
        auto c = 0;
        for (const auto& n : idea.notes)
            c += std::abs (n.beat - beat) < 1.0e-6 ? 1 : 0;
        return c;
    }

    std::vector<std::pair<double, int>> melodyIn (const LineIdea& idea, double from, double to)
    {
        std::vector<std::pair<double, int>> out;
        for (const auto& n : idea.notes)
            if (! n.harmony && n.beat >= from - 1.0e-9 && n.beat < to - 1.0e-9)
                out.push_back ({ std::round ((n.beat - from) * 96.0) / 96.0, n.pitch });   // (to a 96th: copies of triplet places add up a hair apart)
        return out;
    }
}

TEST_CASE ("0.207.0: round six's solo styles are there, with our names and BY STYLE's metalcore takes", "[solo][solostyles][solo207]")
{
    REQUIRE (soloStyles().size() == 126);
    for (const auto* name : { "HOLD, PEDAL, TWIN, STORM", "PANELS OVER THE CHUG", "SEQUENCE SOLO, THEN TWIN ARPEGGIOS", "TRILL, THEN HOLD",
                              "WAH WAVES", "MAJOR RUB OVER A MINOR RIFF", "OCTAVE LEAPS & CRIES", "LATE CALLS, BENT HOME",
                              "CELLS UP, SHAKE THE TOP", "STOP-TIME DOUBLE STOPS", "PICKUP INTO CHORD STABS, RUNS TO LAND",
                              "BLUES CURLS UNDER THE TONIC", "PICKUP-QUARTER BREAK, LONG RING", "UNPLUGGED PENTATONIC STREAM",
                              "TRIPLET HAMMER, THEN FALLING TUNE", "THE TUNE AN OCTAVE DOWN", "THE RIFF ON A WARPED BAR",
                              "SUNG TWICE, TURNED", "SEESAW AND RING", "FOUR BENDS AND OUT" })
        CHECK (soloStyleNamed (name) != SoloStyle::automatic);
    // Metalcore's BY STYLE: ROLLING SEQUENCES on the first take (as before), round six's three metal styles on the others.
    CHECK (soloStyleForGuitarStyle ((int) GuitarStyle::metalcore) == SoloStyle::rollingSequences);
    CHECK (soloStyleForGuitarStyle ((int) GuitarStyle::metalcore, 0) == SoloStyle::rollingSequences);
    CHECK (soloStyleForGuitarStyle ((int) GuitarStyle::metalcore, 1) == SoloStyle::holdPedalTwinStorm);
    CHECK (soloStyleForGuitarStyle ((int) GuitarStyle::metalcore, 2) == SoloStyle::panelsOverTheChug);
    CHECK (soloStyleForGuitarStyle ((int) GuitarStyle::metalcore, 3) == SoloStyle::sequenceThenTwinArpeggios);
    CHECK (soloStyleForGuitarStyle ((int) GuitarStyle::metalcore, 4) == SoloStyle::rollingSequences);
    CHECK (soloStyleForGuitarStyle ((int) GuitarStyle::thrash, 3) == SoloStyle::lickTwice);
    CHECK (soloStyleForGuitarStyle ((int) GuitarStyle::rock, 1) == SoloStyle::singingBends);
}

TEST_CASE ("0.207.0: round six's styles keep to the range and the shortest note in every key, length and tempo",
           "[solo][solostyles][solo207]")
{
    const auto& tuning = GuitarTuning::all()[0];
    for (const auto style : roundSix())
        for (const auto bars : { 4, 8, 16 })
            for (const auto bpm : { 70.0, 120.0, 190.0 })
                for (int keyChoice = 0; keyChoice < 4; ++keyChoice)
                {
                    const auto variation = keyChoice + (bars == 8 ? 1 : 0);
                    INFO (soloStyleName (style) << ", " << bars << " bars at " << bpm << " BPM, key " << keyChoice << ", take " << variation);
                    const auto spans = barsIn (keyChoice, bars);
                    const auto total = bars * 4.0;
                    const auto solo = composeSolo (style, spans, 4.0, tuning, {}, variation, 0.25, bpm);
                    REQUIRE (solo.notes.size() >= 4);
                    std::vector<double> starts;
                    for (const auto& n : solo.notes)
                    {
                        CHECK (n.pitch >= tuning.lowest());
                        CHECK (n.pitch <= tuning.highest());
                        CHECK (n.beat >= 0.0);
                        CHECK (n.beat + n.length <= total + 1.0e-6);
                        if (starts.empty() || n.beat - starts.back() > HumanizeLimits::together)
                            starts.push_back (n.beat);
                    }
                    for (size_t i = 1; i < starts.size(); ++i)
                        CHECK (starts[i] - starts[i - 1] >= 0.25 - 0.04);
                    const auto& last = lastOf207 (solo);
                    CHECK (last.beat + last.length >= total - 1.0e-6);
                    CHECK (sameNotes (solo, composeSolo (style, spans, 4.0, tuning, {}, variation, 0.25, bpm)));
                }
}

TEST_CASE ("0.207.0: round six's styles have their own habits", "[solo][solostyles][solo207]")
{
    const auto spans = amBars (16);   // (A minor; the last chord E major)
    const auto& tuning = GuitarTuning::all()[0];
    const GuitarVelocities vel;
    const auto low = juce::jmax (tuning.strings[1] + 5, 45);
    const auto count = [] (const LineIdea& idea, auto&& test)
    {
        auto c = 0;
        for (const auto& n : idea.notes)
            c += test (n) ? 1 : 0;
        return c;
    };
    const auto harmonies = [&count] (const LineIdea& idea) { return count (idea, [] (const LineNote& n) { return n.harmony; }); };
    for (int variation = 0; variation < 4; ++variation)
    {
        INFO ("variation " << variation);
        const auto compose = [&] (SoloStyle s) { return composeSolo (s, spans, 4.0, tuning, vel, variation); };
        // HOLD, PEDAL, TWIN, STORM: a pedal (one note struck again and again), a twin a third above, the last block's
        // fast cells; the odd takes fall in power chords into the low tonic.
        {
            const auto solo = compose (SoloStyle::holdPedalTwinStorm);
            auto run = 0, longest = 0;
            for (size_t i = 1; i < solo.notes.size(); ++i)
            {
                run = solo.notes[i].pitch == solo.notes[i - 1].pitch && solo.notes[i].length <= 0.26 ? run + 1 : 0;
                longest = juce::jmax (longest, run);
            }
            CHECK (longest >= 2);
            CHECK (harmonies (solo) >= 12);
            for (const auto& n : solo.notes)
                if (n.harmony)
                {
                    auto found = false;
                    for (const auto& m : solo.notes)
                        found = found || (! m.harmony && std::abs (m.beat - n.beat) < 1.0e-6 && n.pitch - m.pitch >= 3 && n.pitch - m.pitch <= 4);
                    CHECK (found);   // (a diatonic third above, in the same rhythm)
                }
            CHECK (onsetsIn (solo, 48.0, 56.0) >= 40);   // (the cells in sixteenth triplets)
            const auto& last = lastOf207 (solo);
            CHECK (pcOfPitch (last.pitch) == 9);
            if (variation % 2 == 1)
            {
                CHECK (notesTogether (solo, last.beat) == 2);
                CHECK (onsetsIn (solo, 59.0, 63.0) >= 15);   // (power chords a half step lower each sixteenth)
                CHECK (last.pitch < low + 12);
            }
        }
        // PANELS OVER THE CHUG: muted chugs, a leap of two octaves, a triad looped, a half-step trill, the flat-seven stab.
        {
            const auto solo = compose (SoloStyle::panelsOverTheChug);
            CHECK (count (solo, [&vel] (const LineNote& n) { return n.velocity == GuitarVelocity::of (vel.muteMid); }) >= 8);
            auto leap = false;
            auto trill = 0;
            for (size_t i = 1; i < solo.notes.size(); ++i)
            {
                leap = leap || solo.notes[i].pitch - solo.notes[i - 1].pitch == 24;
                trill += std::abs (solo.notes[i].pitch - solo.notes[i - 1].pitch) == 1 && solo.notes[i].length <= 0.3 ? 1 : 0;
            }
            CHECK (leap);
            CHECK (trill >= 10);
            const auto& last = lastOf207 (solo);
            CHECK (pcOfPitch (last.pitch) == (variation % 2 == 0 ? 7 : 9));
        }
        // SEQUENCE SOLO, THEN TWIN ARPEGGIOS: four-note groups climbing in the first half; the second half's triads
        // twinned (every note with a second guitar's).
        {
            const auto solo = compose (SoloStyle::sequenceThenTwinArpeggios);
            auto climbing = 0;
            for (size_t i = 3; i < solo.notes.size(); ++i)
                if (solo.notes[i].beat < 32.0 && solo.notes[i - 3].pitch < solo.notes[i - 2].pitch && solo.notes[i - 2].pitch < solo.notes[i - 1].pitch
                    && solo.notes[i - 1].pitch < solo.notes[i].pitch)
                    ++climbing;
            CHECK (climbing >= 6);
            auto melody = 0, twins = 0;
            for (const auto& n : solo.notes)
                if (n.beat >= 32.0 && n.beat < 60.0)
                    (n.harmony ? twins : melody) += 1;
            CHECK (twins == melody);
            CHECK (twins >= 100);
            CHECK (pcOfPitch (lastOf207 (solo).pitch) == 4);
        }
        // TRILL, THEN HOLD: a trill at once, then few notes (long held ones), the last back up.
        {
            const auto solo = compose (SoloStyle::trillThenHold);
            CHECK (onsetsIn (solo, 0.0, 3.0) >= 20);
            CHECK (onsetsIn (solo, 12.0, 56.0) <= 44 * 1.3);
            CHECK (count (solo, [] (const LineNote& n) { return n.length >= 2.4; }) >= 4);
            CHECK (pcOfPitch (lastOf207 (solo).pitch) == 9);
        }
        // WAH WAVES: three bursts (eight notes a beat), one slow release, down to the low root.
        {
            const auto solo = compose (SoloStyle::wahWaves);
            auto busy = 0;
            for (auto from = 0.0; from < 60.0; from += 4.0)
                busy += onsetsIn (solo, from, from + 4.0) >= 22 ? 1 : 0;
            CHECK (busy >= 3);
            CHECK (count (solo, [] (const LineNote& n) { return n.effect == LineNote::Effect::release; }) == 1);
            CHECK (count (solo, [] (const LineNote& n) { return n.effect == LineNote::Effect::bend; }) == 0);
            const auto& last = lastOf207 (solo);
            CHECK (pcOfPitch (last.pitch) == 9);
            CHECK (last.pitch < low + 12);
            for (const auto& n : solo.notes)
                CHECK ((pcOfPitch (n.pitch) == 9 || pcOfPitch (n.pitch) == 0 || pcOfPitch (n.pitch) == 2 || pcOfPitch (n.pitch) == 4 || pcOfPitch (n.pitch) == 7));
        }
        // MAJOR RUB OVER A MINOR RIFF: the major pentatonic on A (C sharp and F sharp) over A minor, two slow bends to end.
        {
            const auto solo = compose (SoloStyle::majorRubOverMinorRiff);
            for (const auto& n : solo.notes)
            {
                const auto pc = pcOfPitch (n.pitch);
                CHECK ((pc == 9 || pc == 11 || pc == 1 || pc == 4 || pc == 6 || pc == 5));   // (F: the double stop's under note on C sharp)
            }
            CHECK (count (solo, [] (const LineNote& n) { return pcOfPitch (n.pitch) == 1; }) >= 2);
            CHECK (count (solo, [] (const LineNote& n) { return n.effect == LineNote::Effect::slowBend; }) == 2);
            CHECK (count (solo, [] (const LineNote& n) { return n.effect == LineNote::Effect::bendBack; }) >= 1);
            const auto& last = lastOf207 (solo);
            CHECK (last.effect == LineNote::Effect::slowBend);
            CHECK (pcOfPitch (last.pitch + 2) == 1);   // (the second bent up into the major third)
        }
        // OCTAVE LEAPS & CRIES: octave pairs (a second guitar an octave under) each dipped, slow wide bends, a bend and back.
        {
            const auto solo = compose (SoloStyle::octaveLeapsAndCries);
            auto pairs = 0;
            for (const auto& n : solo.notes)
                if (n.harmony)
                    for (const auto& m : solo.notes)
                        pairs += ! m.harmony && std::abs (m.beat - n.beat) < 1.0e-6 && m.pitch - n.pitch == 12 && m.effect == LineNote::Effect::dive ? 1 : 0;
            CHECK (pairs >= 8);
            CHECK (count (solo, [] (const LineNote& n) { return n.effect == LineNote::Effect::slowBend && n.amount == 3.0f; }) >= 2);
            auto low3 = 127, high3 = 0;
            for (const auto& n : solo.notes)
            {
                low3 = juce::jmin (low3, n.pitch);
                high3 = juce::jmax (high3, n.pitch);
            }
            CHECK (high3 - low3 >= 30);   // (two octaves and more)
            const auto& last = lastOf207 (solo);
            CHECK (last.effect == LineNote::Effect::bendBack);
            CHECK (last.amount == 5.0f);
        }
        // LATE CALLS, BENT HOME: in late in the bar, most held notes bent slowly, the build at the end, bent home.
        {
            const auto solo = compose (SoloStyle::lateCallsBentHome);
            CHECK (solo.notes.front().beat >= 1.75 - 1.0e-6);
            CHECK (count (solo, [] (const LineNote& n) { return n.effect == LineNote::Effect::slowBend; }) >= 5);
            CHECK (onsetsIn (solo, 52.0, 62.0) > onsetsIn (solo, 24.0, 34.0));
            const auto& last = lastOf207 (solo);
            CHECK (last.effect == LineNote::Effect::slowBend);
            CHECK (pcOfPitch (last.pitch + 2) == 9);
        }
        // CELLS UP, SHAKE THE TOP: each cell played three or four times, the top shaken very wide.
        {
            const auto solo = compose (SoloStyle::cellsUpShakeTheTop);
            auto repeats = 0;
            for (size_t i = 4; i < solo.notes.size() && solo.notes[i].beat < 12.0; ++i)
                repeats += solo.notes[i].pitch == solo.notes[i - 4].pitch ? 1 : 0;
            CHECK (repeats >= 40);
            CHECK (count (solo, [] (const LineNote& n) { return n.effect == LineNote::Effect::wobble && n.amount >= 1.5f; }) >= 2);
            CHECK (pcOfPitch (lastOf207 (solo).pitch) == 9);
        }
        // STOP-TIME DOUBLE STOPS: the screaming octaves, held double stops, the last a fourth.
        {
            const auto solo = compose (SoloStyle::stopTimeDoubleStops);
            CHECK (onsetsIn (solo, 0.0, 4.0) >= 14);
            auto dyads = 0;
            for (const auto& n : solo.notes)
                dyads += n.length >= 1.9 && notesTogether (solo, n.beat) == 2 ? 1 : 0;
            CHECK (dyads >= 6);
            const auto& last = lastOf207 (solo);
            CHECK (pcOfPitch (last.pitch) == 9);
            CHECK (notesTogether (solo, last.beat) == 2);
            const auto slow = composeSolo (SoloStyle::stopTimeDoubleStops, spans, 4.0, tuning, vel, variation, 0.5);
            CHECK (count (slow, [] (const LineNote& n) { return n.technique == LineNote::Technique::tremolo; }) == 2);
        }
        // PICKUP INTO CHORD STABS, RUNS TO LAND: four-note seventh chords, the last note the second.
        {
            const auto solo = compose (SoloStyle::pickupIntoChordStabs);
            auto chords = 0;
            for (const auto& n : solo.notes)
                chords += notesTogether (solo, n.beat) == 4 ? 1 : 0;
            CHECK (chords >= 3 * 4);
            CHECK (pcOfPitch (lastOf207 (solo).pitch) == 11);
        }
        // BLUES CURLS UNDER THE TONIC: down in the octave under the tonic, mostly half-step curls, off-beat starts.
        {
            const auto solo = compose (SoloStyle::bluesCurlsUnderTheTonic);
            auto lowest = 127, highest = 0, half = 0, bends = 0, ands = 0, all = 0;
            for (const auto& n : solo.notes)
            {
                if (n.harmony)
                    continue;
                lowest = juce::jmin (lowest, n.pitch);
                highest = juce::jmax (highest, n.pitch);
                if (n.effect == LineNote::Effect::bend)
                {
                    ++bends;
                    half += n.amount == 1.0f ? 1 : 0;
                }
                ands += std::abs (std::fmod (n.beat, 1.0) - 0.5) < 1.0e-6 ? 1 : 0;
                ++all;
            }
            CHECK (highest - lowest <= 16);
            CHECK (half * 10 >= bends * 6);
            CHECK (ands * 10 >= all * 3);
            CHECK (pcOfPitch (lastOf207 (solo).pitch) == 9);
        }
        // PICKUP-QUARTER BREAK, LONG RING: the cell (three sixteenths into a quarter), the long top note.
        {
            const auto solo = compose (SoloStyle::pickupQuarterBreak);
            auto cells = 0;
            for (size_t i = 3; i < solo.notes.size(); ++i)
                cells += std::abs (solo.notes[i].beat - std::round (solo.notes[i].beat)) < 1.0e-6 && solo.notes[i].length >= 0.9
                         && std::abs (solo.notes[i - 3].beat - (solo.notes[i].beat - 0.75)) < 1.0e-6 ? 1 : 0;
            CHECK (cells >= 8);
            const auto& last = lastOf207 (solo);
            CHECK (last.length >= 16.0 - 1.0e-6);
            CHECK (pcOfPitch (last.pitch) == 0);
        }
        // UNPLUGGED PENTATONIC STREAM: no rest over half a beat, about half the starts on the and, bends on the fourth only.
        {
            const auto solo = compose (SoloStyle::unpluggedPentatonicStream);
            auto ands = 0;
            auto gap = 0.0;
            for (size_t i = 0; i < solo.notes.size(); ++i)
            {
                const auto& n = solo.notes[i];
                ands += std::abs (std::fmod (n.beat, 1.0) - 0.5) < 1.0e-6 ? 1 : 0;
                if (i > 0)
                    gap = juce::jmax (gap, n.beat - (solo.notes[i - 1].beat + solo.notes[i - 1].length));
                if (n.effect == LineNote::Effect::bend)
                    CHECK (pcOfPitch (n.pitch) == 2);
            }
            CHECK (gap <= 0.55);
            CHECK (ands * 100 >= (int) solo.notes.size() * 30);   // (the sixteenth pairs sit between)
            CHECK (ands * 100 <= (int) solo.notes.size() * 65);
            const auto pc = pcOfPitch (lastOf207 (solo).pitch);
            CHECK ((pc == 9 || pc == 0 || pc == 2));
        }
        // TRIPLET HAMMER, THEN FALLING TUNE: one note in quarter-note triplets, then the tune, ending on the minor third.
        {
            const auto solo = compose (SoloStyle::tripletHammerFallingTune);
            std::set<int> hammered;
            for (const auto& n : solo.notes)
                if (n.beat < 16.0)
                {
                    hammered.insert (n.pitch);
                    CHECK (std::abs (n.beat * 1.5 - std::round (n.beat * 1.5)) < 1.0e-6);
                }
            CHECK (hammered.size() == 1);
            CHECK (pcOfPitch (*hammered.begin()) == 4);
            CHECK (pcOfPitch (lastOf207 (solo).pitch) == 0);
        }
        // THE TUNE AN OCTAVE DOWN: two-bar units played as exact copies, in ten semitones; the rocking B shaken.
        {
            const auto solo = compose (SoloStyle::tuneAnOctaveDown);
            CHECK (melodyIn (solo, 0.0, 8.0) == melodyIn (solo, 16.0, 24.0));
            CHECK (melodyIn (solo, 32.0, 40.0) == melodyIn (solo, 40.0, 48.0));
            auto lowest = 127, highest = 0;
            for (const auto& n : solo.notes)
                if (n.beat < 60.0)
                {
                    lowest = juce::jmin (lowest, n.pitch);
                    highest = juce::jmax (highest, n.pitch);
                }
            CHECK (highest - lowest <= 10);
            CHECK (count (solo, [] (const LineNote& n) { return n.effect == LineNote::Effect::fastWobble; }) >= 8);
            CHECK (onsetsIn (solo, 0.0, 60.0) <= 60 * 1.25);
            const auto& last = lastOf207 (solo);
            CHECK (last.pitch < lowest);   // (the last bar an octave lower)
            CHECK (pcOfPitch (last.pitch) == 11);
        }
        // THE RIFF ON A WARPED BAR: three notes, a four-bar cycle the same each time, most notes shimmering.
        {
            const auto solo = compose (SoloStyle::riffOnAWarpedBar);
            std::set<int> pcs;
            for (const auto& n : solo.notes)
                pcs.insert (pcOfPitch (n.pitch));
            CHECK (pcs == std::set<int> { 9, 4, 7 });
            CHECK (melodyIn (solo, 0.0, 16.0) == melodyIn (solo, 16.0, 32.0));
            CHECK (count (solo, [] (const LineNote& n) { return n.effect == LineNote::Effect::shimmer; }) * 2 >= (int) solo.notes.size());
            CHECK (count (solo, [] (const LineNote& n) { return n.effect == LineNote::Effect::fallOff; }) >= 6);
            CHECK (pcOfPitch (lastOf207 (solo).pitch) == 4);
        }
        // SUNG TWICE, TURNED: the tune again in the second half (bar for bar, till its landing), doubled an octave down.
        {
            const auto solo = compose (SoloStyle::sungTwiceTurned);
            CHECK (melodyIn (solo, 0.0, 24.0) == melodyIn (solo, 32.0, 56.0));
            auto doubled = 0, melody = 0;
            for (const auto& n : solo.notes)
                if (! n.harmony)
                {
                    ++melody;
                    for (const auto& m : solo.notes)
                        doubled += m.harmony && std::abs (m.beat - n.beat) < 1.0e-6 && n.pitch - m.pitch == 12 ? 1 : 0;
                }
            CHECK (doubled >= melody - 1);
            CHECK (pcOfPitch (lastOf207 (solo).pitch) == 9);
        }
        // SEESAW AND RING: on the beat, busiest first, a four-note chord rung at the end.
        {
            const auto solo = compose (SoloStyle::seesawAndRing);
            auto onBeat = 0;
            for (const auto& n : solo.notes)
                onBeat += std::abs (n.beat - std::round (n.beat)) < 1.0e-6 ? 1 : 0;
            CHECK (onBeat * 10 >= (int) solo.notes.size() * 7);
            CHECK (onsetsIn (solo, 0.0, 14.0) > onsetsIn (solo, 42.0, 56.0));
            const auto& last = lastOf207 (solo);
            CHECK (notesTogether (solo, last.beat) == 4);
            CHECK (pcOfPitch (last.pitch) == 4);
        }
        // FOUR BENDS AND OUT: one note held bent and let down, slides in, out on power chords in sixteenths.
        {
            const auto solo = compose (SoloStyle::fourBendsAndOut);
            CHECK (solo.notes.front().effect == LineNote::Effect::bendBack);
            CHECK (solo.notes.front().length >= 9.0);
            CHECK (count (solo, [] (const LineNote& n) { return n.effect == LineNote::Effect::slideIn; }) == 2);
            CHECK (onsetsIn (solo, 62.0, 64.0) == 8);
            const auto& last = lastOf207 (solo);
            CHECK (notesTogether (solo, last.beat) == 3);
            CHECK (pcOfPitch (last.pitch) == 9);
        }
    }
}

TEST_CASE ("0.207.0: the shimmer, the fast shake and the bend let back down as pitch bend", "[solo][solo207]")
{
    const auto bendsOf = [] (LineNote::Effect effect, float amount)
    {
        LineNote n;
        n.beat = 0.0;
        n.length = 2.0;
        n.pitch = 60;
        n.effect = effect;
        n.amount = amount;
        std::vector<Controller> out;
        for (const auto& c : Studio::effectControllers ({ n }, 2, 120.0))
            if (c.type == Controller::Type::pitchbend)
                out.push_back (c);
        return out;
    };
    // The shimmer: from the start, a quarter cycle every 1/12 s (a sixth of a beat at 120), 0.4 semitone either way.
    const auto shimmer = bendsOf (LineNote::Effect::shimmer, 0.4f);
    REQUIRE (shimmer.size() >= 10);
    CHECK (std::abs (shimmer[1].start - 1.0 / 6.0) < 1.0e-6);
    auto top = 0;
    for (const auto& c : shimmer)
        top = juce::jmax (top, c.value);
    CHECK (top == 8192 + juce::roundToInt (0.2 * 8191.0));
    CHECK (shimmer.back().value == 8192);
    // The fast shake: three times as many points, wider.
    const auto shake = bendsOf (LineNote::Effect::fastWobble, 1.5f);
    CHECK (shake.size() > shimmer.size() * 2);
    auto wide = 0;
    for (const auto& c : shake)
        wide = juce::jmax (wide, c.value);
    CHECK (wide > top);
    // Bent and let back down: up early, held, back to none before the end.
    const auto back = bendsOf (LineNote::Effect::bendBack, 2.0f);
    REQUIRE (back.size() == 5);
    CHECK (back[1].value == 16383);
    CHECK (back[2].value == 16383);
    CHECK (back[3].value == 8192);
    CHECK (back[3].start < 2.0 - 1.0e-6);
}


TEST_CASE ("0.246.0: round ten's solo styles", "[solo][solostyles][solo246]")
{
    const auto spans = amBars (16);
    const auto& tuning = GuitarTuning::all()[0];
    const SoloStyle styles[] { SoloStyle::slideBluesCalls, SoloStyle::twinMelodyInOctaves, SoloStyle::cleanScoopedBallad,
                               SoloStyle::eighthHookOnTheChanges, SoloStyle::tremoloThenFlurry, SoloStyle::heldBendsAnsweredByBursts };
    for (int variation = 0; variation < 3; ++variation)
    {
        INFO ("variation " << variation);
        const auto count = [] (const LineIdea& idea, auto&& test)
        {
            auto c = 0;
            for (const auto& n : idea.notes)
                c += test (n) ? 1 : 0;
            return c;
        };
        // Slides open the calls.
        CHECK (count (composeSolo (SoloStyle::slideBluesCalls, spans, 4.0, tuning, {}, variation),
                      [] (const LineNote& n) { return n.effect == LineNote::Effect::slideIn; }) >= 4);
        // The twin: every played note has its octave on the second track.
        {
            const auto solo = composeSolo (SoloStyle::twinMelodyInOctaves, spans, 4.0, tuning, {}, variation);
            const auto played = count (solo, [] (const LineNote& n) { return ! n.harmony; });
            const auto twin = count (solo, [] (const LineNote& n) { return n.harmony; });
            CHECK (twin >= played - 2);
        }
        // Scoops in the ballad.
        CHECK (count (composeSolo (SoloStyle::cleanScoopedBallad, spans, 4.0, tuning, {}, variation),
                      [] (const LineNote& n) { return n.effect == LineNote::Effect::scoop; }) >= 4);
        // The tremolo: the first bars one pitch.
        {
            const auto solo = composeSolo (SoloStyle::tremoloThenFlurry, spans, 4.0, tuning, {}, variation);
            std::set<int> pitches;
            for (const auto& n : solo.notes)
                if (n.beat < 20.0)
                    pitches.insert (n.pitch);
            CHECK (pitches.size() == 1u);
        }
        // Bursts of sixteenths after held bends.
        CHECK (count (composeSolo (SoloStyle::heldBendsAnsweredByBursts, spans, 4.0, tuning, {}, variation),
                      [] (const LineNote& n) { return n.effect == LineNote::Effect::bend; }) >= 4);
    }
    for (const auto s : styles)
    {
        CHECK (soloStyleNamed (soloStyleName (s)) == s);
        CHECK (soloGroupOrder().contains (soloGroupOf (s)));
    }
}
