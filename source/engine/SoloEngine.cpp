#include "engine/SoloEngine.h"

#include <set>

namespace pr7s
{
    namespace
    {
        int pcOf (int n) { return ((n % 12) + 12) % 12; }

        // A style's rules (from docs/solo-styles: what the lessons say each player does; the numbers are
        // starting points to tune by ear).
        enum class Burst { run, cell, tapping, displaced, chromaticShift, repeatLick, sixes, sequence, fourSequence, risingFlurry };
        struct Profile
        {
            bool sixth = false, ninth = false, flatFive = false, majorOnMajor = false, curls = false;
            bool majorThird = false, aeolian = false;                   // (0.67.0) the major 3rd over minor; natural minor's 2 and b6
            bool pinchSqueals = false, scream = false, upbeatEntry = false, endHigh = false;   // (0.67.0)
            // (0.68.0) thirds for double stops (else 6ths); one whammy dip; melody behind the beat; the blue b3 and b7
            // over major chords; bends bigger as the solo goes; the last burst outside the key.
            bool thirdDoubleStops = false, dip = false, blueOnMajor = false, escalate = false, outburst = false;
            double lateBeats = 0.0;
            float startDensity = 4.0f, peakDensity = 10.0f;   // notes a bar
            double restBeats = 1.0;                            // between phrases
            int phraseBars = 2;
            Burst burst = Burst::run;
            float burstFrom = 0.5f;                            // along the solo
            bool burstEveryOther = true;                       // else only the phrase before the last
            float bendChance = 0.5f;
            std::vector<std::pair<float, float>> bends;        // semitones, weight (over a whole step: only late)
            float bigFrom = 0.6f;
            float vibrato = 1.0f, vibratoSkip = 0.0f;
            float swing = 0.5f;                                // 0.5 straight; 16ths unless swing8
            bool swing8 = false;
            int climb = 12;                                    // semitones up over the solo
            bool doubleStops = false, tremolo = false, harmonics = false, dive = false, legato = true;
            const char* description = "";
        };

        Profile profileOf (SoloStyle s)
        {
            Profile p;
            switch (s)
            {
                case SoloStyle::soaringBends:
                    p.sixth = p.ninth = p.majorOnMajor = true;
                    p.startDensity = 3.0f; p.peakDensity = 9.0f; p.restBeats = 1.5; p.phraseBars = 2;
                    p.burst = Burst::run; p.burstFrom = 0.7f; p.burstEveryOther = false;
                    p.bendChance = 0.65f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.4f }, { 3.0f, 0.3f }, { 4.0f, 0.15f } }; p.bigFrom = 0.55f;
                    p.vibrato = 0.6f; p.vibratoSkip = 0.3f; p.climb = 12;
                    p.description = "Slow and spacious: a singing motif that keeps coming back, long notes bent slowly into their pitch, "
                                    "restrained vibrato, climbing to big bends and one short fast run near the end";
                    break;
                case SoloStyle::rockSwagger:
                    p.sixth = p.ninth = p.flatFive = p.majorOnMajor = true;
                    p.startDensity = 5.0f; p.peakDensity = 12.0f; p.restBeats = 1.0; p.phraseBars = 1;
                    p.burst = Burst::cell; p.burstFrom = 0.45f;
                    p.bendChance = 0.55f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.6f }, { 3.0f, 0.2f } };
                    p.vibrato = 1.0f; p.swing = 0.62f; p.climb = 7; p.doubleStops = true; p.tremolo = true;
                    p.description = "Loose, swung bluesy rock: short call-and-response phrases, the major 6th and both thirds, "
                                    "6th double stops, slides and pull-offs, wide vibrato, repeated licks midway";
                    break;
                case SoloStyle::tapAndDive:
                    p.sixth = p.flatFive = p.majorOnMajor = true;
                    p.startDensity = 4.0f; p.peakDensity = 14.0f; p.restBeats = 1.0; p.phraseBars = 1;
                    p.burst = Burst::tapping; p.burstFrom = 0.25f;
                    p.bendChance = 0.45f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.3f }, { 3.0f, 0.3f }, { 4.0f, 0.3f } }; p.bigFrom = 0.4f;
                    p.vibrato = 1.0f; p.swing = 0.56f; p.swing8 = true; p.climb = 12; p.tremolo = p.harmonics = p.dive = true;
                    p.description = "Melodic phrases traded with tapped arpeggios that follow the chords, legato runs, harmonics, "
                                    "wide vibrato and big bends, ending on a pinch harmonic and a whammy dive";
                    break;
                case SoloStyle::bluesHammer:
                    p.sixth = p.ninth = p.flatFive = p.majorOnMajor = p.curls = true;
                    p.startDensity = 5.0f; p.peakDensity = 14.0f; p.restBeats = 0.75; p.phraseBars = 1;
                    p.burst = Burst::displaced; p.burstFrom = 0.35f;
                    p.bendChance = 0.55f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.4f }, { 3.0f, 0.3f }, { 4.0f, 0.2f }, { 5.0f, 0.1f } };
                    p.vibrato = 0.9f; p.climb = 12;
                    p.description = "Bluesy and loose: major and minor mixed, flat thirds curled sharp, licks repeated so they "
                                    "shift against the bar, crammed bursts, big over-bends and a climax of repeated bends";
                    break;
                case SoloStyle::screamAndDive:
                    p.sixth = p.flatFive = p.majorThird = true;
                    p.startDensity = 4.0f; p.peakDensity = 14.0f; p.restBeats = 1.0; p.phraseBars = 1;
                    p.burst = Burst::chromaticShift; p.burstFrom = 0.3f;
                    p.bendChance = 0.6f; p.bends = { { 1.0f, 1.0f }, { 2.0f, 0.8f }, { 3.0f, 0.3f }, { 4.0f, 0.2f } }; p.bigFrom = 0.5f;
                    p.vibrato = 1.3f; p.climb = 12; p.dive = p.scream = true;
                    p.description = "Aggressive groove-metal lead: the blues scale with the major 3rd and 6th, slow half-step bends "
                                    "from below, licks shifted up a fret at a time, a harmonic screamed up with the bar and a dive bomb to end";
                    break;
                case SoloStyle::thrashFire:
                    p.flatFive = true;
                    p.startDensity = 5.0f; p.peakDensity = 14.0f; p.restBeats = 0.75; p.phraseBars = 2;
                    p.burst = Burst::repeatLick; p.burstFrom = 0.3f;
                    p.bendChance = 0.45f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.5f }, { 3.0f, 0.2f } };
                    p.vibrato = 1.2f; p.climb = 14; p.upbeatEntry = p.endHigh = true;
                    p.description = "Thrash pentatonic fire: in on an upbeat with a vocal-like melody, then a picked triplet lick "
                                    "repeated so it drifts across the bar, wide vibrato, ending high";
                    break;
                case SoloStyle::pinchSqueal:
                    p.flatFive = p.majorOnMajor = true;
                    p.startDensity = 4.0f; p.peakDensity = 16.0f; p.restBeats = 1.0; p.phraseBars = 1;
                    p.burst = Burst::sixes; p.burstFrom = 0.45f;
                    p.bendChance = 0.5f; p.bends = { { 2.0f, 1.0f }, { 3.0f, 0.3f }, { 1.0f, 0.3f } };
                    p.vibrato = 1.6f; p.climb = 12; p.pinchSqueals = true;
                    p.description = "Heavy pentatonic: a melody first, then picked six-note runs down the boxes near the end, "
                                    "pinch-harmonic squeals in the gaps, very wide vibrato on every phrase end";
                    break;
                case SoloStyle::progPrecision:
                    p.aeolian = true;
                    p.startDensity = 3.5f; p.peakDensity = 16.0f; p.restBeats = 1.0; p.phraseBars = 1;
                    p.burst = Burst::sequence; p.burstFrom = 0.3f;
                    p.bendChance = 0.5f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.3f }, { 3.0f, 0.15f } };
                    p.vibrato = 0.9f; p.climb = 14;
                    p.description = "Precise and composed: singing in-tune bends traded with picked six-note scale sequences and "
                                    "seventh-chord arpeggios, building in curves to something outside at the climax";
                    break;
                case SoloStyle::glamFlash:
                    p.majorOnMajor = p.flatFive = true;
                    p.startDensity = 3.0f; p.peakDensity = 12.0f; p.restBeats = 1.0; p.phraseBars = 1;
                    p.burst = Burst::run; p.burstFrom = 0.5f; p.burstEveryOther = false;
                    p.bendChance = 0.65f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.3f }, { 3.0f, 0.2f } }; p.bigFrom = 0.5f;
                    p.vibrato = 1.3f; p.climb = 10; p.doubleStops = p.thirdDoubleStops = true;
                    p.description = "Glam hard rock: sustained notes and long bends held with vibrato, lyrical double stops in thirds, "
                                    "melodic answers, and the fast picked run saved for the very end";
                    break;
                case SoloStyle::sleazeBlues:
                    p.flatFive = p.majorOnMajor = p.curls = true;
                    p.startDensity = 3.0f; p.peakDensity = 8.0f; p.restBeats = 1.5; p.phraseBars = 2;
                    p.burst = Burst::run; p.burstFrom = 0.5f; p.burstEveryOther = false;
                    p.bendChance = 0.7f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.3f } };
                    p.vibrato = 1.1f; p.climb = 7; p.dip = true;
                    p.description = "A bluesman's melody you could hum: two-bar phrases and their answers, a bend held on every bar, "
                                    "space for the singer, one whammy dip, a quick run near the end and a long held note to finish";
                    break;
                case SoloStyle::topHatMelody:
                    p.flatFive = p.majorOnMajor = true;
                    p.startDensity = 3.5f; p.peakDensity = 12.0f; p.restBeats = 1.0; p.phraseBars = 1;
                    p.burst = Burst::fourSequence; p.burstFrom = 0.5f;
                    p.bendChance = 0.6f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.4f }, { 3.0f, 0.3f } }; p.bigFrom = 0.5f;
                    p.vibrato = 1.2f; p.climb = 12; p.lateBeats = 0.03;
                    p.description = "Melody first: slow, loose bends and held notes laid back behind the beat, a motif played again "
                                    "higher, then a wilder second half of four-note sequences climbing the scale";
                    break;
                case SoloStyle::purpleScream:
                    p.majorOnMajor = p.blueOnMajor = true;
                    p.startDensity = 2.5f; p.peakDensity = 12.0f; p.restBeats = 1.5; p.phraseBars = 2;
                    p.burst = Burst::risingFlurry; p.burstFrom = 0.4f;
                    p.bendChance = 0.7f; p.bends = { { 2.0f, 1.0f } };
                    p.vibrato = 1.4f; p.climb = 14; p.escalate = p.outburst = p.endHigh = true;
                    p.description = "Restrained, then soaring: a motif brought back higher each time, bends bigger every time, "
                                    "rising flurries, one outburst outside the key at the peak and a screaming held high note";
                    break;
                case SoloStyle::automatic: break;
            }
            return p;
        }

        const ChordSpan& spanAt (const std::vector<ChordSpan>& spans, double songBeat)
        {
            for (const auto& s : spans)
                if (songBeat < s.end - 1.0e-9)
                    return s;
            return spans.back();
        }

        bool has (const std::vector<int>& list, int v) { return std::find (list.begin(), list.end(), v) != list.end(); }

        std::vector<int> chordPcs (const Chord& c)
        {
            const auto t = c.pitchClasses();
            return { t.begin(), t.end() };
        }

        // The scale over a chord: minor pentatonic on the key's minor root (its relative minor in a major key:
        // the major pentatonic) and the style's added notes; over a major chord, where the style does it, the
        // chord's own major pentatonic.
        std::vector<int> scaleOver (const ChordSpan& span, const Profile& p)
        {
            std::vector<int> out;
            const auto add = [&out] (int pc) { if (! has (out, pcOf (pc))) out.push_back (pcOf (pc)); };
            if (p.majorOnMajor && span.chord.type == Chord::Type::major)
            {
                for (const auto s : { 0, 2, 4, 7, 9 })
                    add (span.chord.root + s);
                if (p.blueOnMajor) { add (span.chord.root + 3); add (span.chord.root + 10); }
                return out;
            }
            const auto minorRoot = span.key.minor ? span.key.pitchClass() : span.key.pitchClass() + 9;
            for (const auto s : { 0, 3, 5, 7, 10 })
                add (minorRoot + s);
            if (p.ninth) add (minorRoot + 2);
            if (p.sixth) add (minorRoot + 9);
            if (p.majorThird) add (minorRoot + 4);
            if (p.aeolian) { add (minorRoot + 2); add (minorRoot + 8); }
            if (p.flatFive) add (minorRoot + 6);
            return out;
        }

        std::vector<int> poolOf (const std::vector<int>& pcs, int low, int high)
        {
            std::vector<int> out;
            for (auto n = low; n <= high; ++n)
                if (has (pcs, pcOf (n)))
                    out.push_back (n);
            return out;
        }

        int nearestIndex (const std::vector<int>& pool, double pitch)
        {
            auto best = 0;
            for (size_t i = 1; i < pool.size(); ++i)
                if (std::abs (pool[i] - pitch) < std::abs (pool[(size_t) best] - pitch))
                    best = (int) i;
            return best;
        }

        // The nearest pitch in [low, high] with one of `pcs`.
        int nearestOf (const std::vector<int>& pcs, double around, int low, int high)
        {
            auto best = -1;
            for (auto n = low; n <= high; ++n)
                if (has (pcs, pcOf (n)) && (best < 0 || std::abs (n - around) < std::abs (best - around)))
                    best = n;
            return best < 0 ? juce::jlimit (low, high, juce::roundToInt (around)) : best;
        }

        struct Motif
        {
            std::vector<double> at;   // beats into the phrase
            std::vector<int> steps;   // scale steps from the note before (the first: 0)
        };

        // A phrase's rhythm: `count` notes on the grid inside `playable` beats, strong places first.
        std::vector<double> rhythmOf (int count, double playable, double grid, juce::Random& r, bool pickup)
        {
            std::vector<double> slots;
            for (auto x = 0.0; x < playable - 1.0e-9; x += grid)
                slots.push_back (x);
            count = juce::jlimit (1, juce::jmax (1, (int) slots.size()), count);
            std::vector<std::pair<double, double>> scored;   // (score, place)
            for (const auto x : slots)
            {
                const auto onBeat = std::abs (x - std::round (x)) < 1.0e-6;
                const auto onHalf = std::abs (x * 2.0 - std::round (x * 2.0)) < 1.0e-6;
                auto w = onBeat ? 1.0 : onHalf ? 0.7 : 0.45;
                if (pickup && x < 1.0e-6)
                    w *= 0.4;   // (a pickup start: off the first beat)
                scored.push_back ({ w * (0.6 + 0.8 * r.nextDouble()), x });
            }
            std::sort (scored.begin(), scored.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });
            std::vector<double> out;
            for (int i = 0; i < count; ++i)
                out.push_back (scored[(size_t) i].second);
            std::sort (out.begin(), out.end());
            return out;
        }

        // An arch: up through the first half, down the second; steps of one, now and then two.
        std::vector<int> contourOf (int count, juce::Random& r)
        {
            std::vector<int> steps { 0 };
            for (int i = 1; i < count; ++i)
            {
                const auto rising = i < (count + 1) / 2;
                auto s = r.nextFloat() < 0.25f ? 2 : 1;
                if (r.nextFloat() < 0.2f)
                    s = -s;   // (a turn)
                steps.push_back (rising ? s : -s);
            }
            return steps;
        }

        void addNote (std::vector<LineNote>& notes, double at, double length, int pitch, int velocity)
        {
            LineNote n;
            n.beat = at;
            n.length = juce::jmax (0.05, length);
            n.pitch = pitch;
            n.velocity = GuitarVelocity::of (juce::jlimit (1, 127, velocity));
            notes.push_back (n);
        }
    }

    //==========================================================================
    const std::vector<SoloStyle>& soloStyles()
    {
        static const std::vector<SoloStyle> all { SoloStyle::automatic, SoloStyle::soaringBends, SoloStyle::rockSwagger,
                                                  SoloStyle::tapAndDive, SoloStyle::bluesHammer,
                                                  SoloStyle::screamAndDive, SoloStyle::thrashFire, SoloStyle::pinchSqueal,
                                                  SoloStyle::progPrecision, SoloStyle::glamFlash, SoloStyle::sleazeBlues,
                                                  SoloStyle::topHatMelody, SoloStyle::purpleScream };
        return all;
    }

    juce::String soloStyleName (SoloStyle s)
    {
        switch (s)
        {
            case SoloStyle::automatic: return "BY STYLE";
            case SoloStyle::soaringBends: return "SOARING BENDS";
            case SoloStyle::rockSwagger: return "ROCK 'N' ROLL SWAGGER";
            case SoloStyle::tapAndDive: return "TAP & DIVE";
            case SoloStyle::bluesHammer: return "BLUES HAMMER";
            case SoloStyle::screamAndDive: return "SCREAM & DIVE";
            case SoloStyle::thrashFire: return "THRASH FIRE";
            case SoloStyle::pinchSqueal: return "PINCH SQUEAL";
            case SoloStyle::progPrecision: return "PROG PRECISION";
            case SoloStyle::glamFlash: return "GLAM FLASH";
            case SoloStyle::sleazeBlues: return "SLEAZE BLUES";
            case SoloStyle::topHatMelody: return "TOP HAT MELODY";
            case SoloStyle::purpleScream: return "PURPLE SCREAM";
        }
        return "BY STYLE";
    }

    SoloStyle soloStyleNamed (const juce::String& name)
    {
        for (const auto s : soloStyles())
            if (soloStyleName (s).equalsIgnoreCase (name.trim()))
                return s;
        return SoloStyle::automatic;
    }

    juce::String soloStyleDescription (SoloStyle s)
    {
        if (s == SoloStyle::automatic)
            return "The guitar STYLE's own six leads, from the calmest to the busiest, with bends and vibrato on the long notes";
        return profileOf (s).description;
    }

    //==========================================================================
    LineIdea composeSolo (SoloStyle style, const std::vector<ChordSpan>& spans, double beatsPerBar, const GuitarTuning& tuning,
                          const GuitarVelocities& vel, int variation, double shortest)
    {
        const auto titled = [] (SoloStyle st) -> juce::String
        {
            switch (st)
            {
                case SoloStyle::soaringBends: return "Soaring Bends";
                case SoloStyle::rockSwagger: return "Rock 'n' Roll Swagger";
                case SoloStyle::tapAndDive: return "Tap & Dive";
                case SoloStyle::bluesHammer: return "Blues Hammer";
                case SoloStyle::screamAndDive: return "Scream & Dive";
                case SoloStyle::thrashFire: return "Thrash Fire";
                case SoloStyle::pinchSqueal: return "Pinch Squeal";
                case SoloStyle::progPrecision: return "Prog Precision";
                case SoloStyle::glamFlash: return "Glam Flash";
                case SoloStyle::sleazeBlues: return "Sleaze Blues";
                case SoloStyle::topHatMelody: return "Top Hat Melody";
                case SoloStyle::purpleScream: return "Purple Scream";
                case SoloStyle::automatic: break;
            }
            return "By Style";
        };
        LineIdea solo { LineIdea::Part::lead, "Solo: " + titled (style), {}, {} };
        if (spans.empty() || beatsPerBar <= 0.0 || style == SoloStyle::automatic)
            return solo;
        const auto p = profileOf (style);
        juce::Random r ((juce::int64) variation * 7919 + (int) style * 131 + 17);
        const auto origin = spans.front().start;
        const auto total = spans.back().end - origin;
        const auto low = juce::jmax (tuning.strings.size() > 1 ? tuning.strings[1] + 5 : tuning.lowest(), 45);
        const auto high = tuning.strings.back() + 22;
        const auto chordAt = [&] (double beat) -> const ChordSpan& { return spanAt (spans, beat + origin); };
        const auto chordTones = [&] (double beat) { return chordPcs (chordAt (beat).chord); };
        const auto scaleAt = [&] (double beat) { return scaleOver (chordAt (beat), p); };
        const auto phraseLength = p.phraseBars * beatsPerBar;
        const auto phrases = juce::jmax (1, (int) std::ceil (total / phraseLength - 1.0e-6));
        const auto shortestStep = shortest > 0.0 ? shortest : 0.0;
        const auto gridFor = [shortestStep] (double wanted) { return juce::jmax (wanted, shortestStep); };
        auto& notes = solo.notes;

        // The motif: its rhythm and its contour, made once.
        Motif motif;
        {
            const auto count = juce::jmax (2, juce::roundToInt (p.startDensity * p.phraseBars));
            motif.at = rhythmOf (count, phraseLength - p.restBeats, gridFor (p.startDensity > 4.0f ? 0.25 : 0.5), r, true);
            motif.steps = contourOf ((int) motif.at.size(), r);
            // (0.67.0) In on an upbeat: the first note off the beat, where there's room.
            if (p.upbeatEntry && motif.at.size() > 1 && motif.at[0] < 0.5 && motif.at[1] >= 0.5 + gridFor (0.25) - 1.0e-9)
                motif.at[0] = 0.5;
        }
        auto burstCount = 0;
        const auto velocityAt = [&] (float t, bool accent)
        {
            const auto base = (float) vel.sustainSoft + ((float) vel.sustainHard - (float) vel.sustainSoft) * t;
            return vel.open (juce::roundToInt (base) + (accent ? 6 : 0));
        };

        std::vector<std::pair<double, double>> burstSpans;   // (from, to): no bends or vibrato added there
        for (int ph = 0; ph < phrases; ++ph)
        {
            const auto t = phrases == 1 ? 1.0f : (float) ph / (float) (phrases - 1);
            const auto from = ph * phraseLength;
            const auto to = juce::jmin (total, from + phraseLength);
            const auto centre = (double) low + 7.0 + p.climb * t;
            const auto lastPhrase = ph == phrases - 1;
            const auto burstHere = ph > 0 && ! lastPhrase && t >= p.burstFrom
                                && (p.burstEveryOther ? (phrases - 1 - ph) % 2 == 1 : ph == phrases - 2);   // (0.67.0: counted back from the end, so the busy phrases come late)
            if (burstHere)
            {
                const auto burstIndex = burstCount++;
                // The style's signature passage, then a held landing note on the last beat.
                const auto landing = juce::jmax (from, to - juce::jmin (1.0, beatsPerBar * 0.25) - (p.burstEveryOther ? 0.0 : p.restBeats));
                const auto burstFrom = p.burstEveryOther ? from : juce::jmax (from, landing - 2.0);
                burstSpans.push_back ({ burstFrom, landing });
                auto pool = poolOf (scaleAt (burstFrom), low, high);
                if (pool.empty())
                    continue;
                auto idx = nearestIndex (pool, centre + 5.0);
                switch (p.burst)
                {
                    case Burst::run:
                    {
                        // Down the scale in 16ths.
                        const auto g = gridFor (0.25);
                        for (auto x = burstFrom; x < landing - 1.0e-9; x += g)
                        {
                            addNote (notes, x, g, pool[(size_t) idx], velocityAt (t, false));
                            idx = juce::jmax (0, idx - 1);
                        }
                        break;
                    }
                    case Burst::cell:
                    {
                        // A three-note pull-off cell in triplets, repeated (the scale under the chord as it changes).
                        const auto g = gridFor (1.0 / 3.0);
                        auto k = 0;
                        for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                        {
                            const auto here = poolOf (scaleAt (x), low, high);
                            const auto base = nearestIndex (here, centre + 3.0);
                            const auto at = juce::jlimit (0, (int) here.size() - 1, base + 2 - (k % 3));
                            addNote (notes, x, g, here[(size_t) at], velocityAt (t, k % 3 == 0));
                        }
                        break;
                    }
                    case Burst::tapping:
                    {
                        // Tap, pull off, hammer on: the chord's triad, the tap an octave over its root, following the chords.
                        const auto g = gridFor (1.0 / 3.0);
                        auto k = 0;
                        for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                        {
                            const auto& span = chordAt (x);
                            const auto rootNote = nearestOf ({ span.chord.root }, centre, low, high - 12);
                            const auto third = nearestOf ({ pcOf (span.chord.root + (span.chord.type == Chord::Type::minor
                                                                                      || span.chord.type == Chord::Type::diminished ? 3 : 4)) },
                                                          rootNote + 4, low, high);
                            const int shape[] = { rootNote + 12, rootNote, third };
                            addNote (notes, x, g, juce::jlimit (low, high, shape[k % 3]), velocityAt (t, k % 3 == 0));
                            notes.back().technique = LineNote::Technique::tapping;
                        }
                        break;
                    }
                    case Burst::displaced:
                    {
                        // A six-note cell in 16ths (a beat and a half), looped so it lands somewhere new in each bar.
                        const auto g = gridFor (0.25);
                        const int cell[] = { 2, 1, 0, 1, 0, -1 };
                        auto k = 0;
                        for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                        {
                            const auto here = poolOf (scaleAt (x), low, high);
                            const auto base = nearestIndex (here, centre + 2.0);
                            const auto at = juce::jlimit (0, (int) here.size() - 1, base + cell[k % 6]);
                            addNote (notes, x, g, here[(size_t) at], velocityAt (t, k % 6 == 0));
                        }
                        break;
                    }
                    case Burst::chromaticShift:
                    {
                        // A four-note cell moved up a fret every beat, outside the scale as it goes.
                        const auto g = gridFor (0.25);
                        const auto per = juce::jmax (1, juce::roundToInt (1.0 / g));
                        const auto here = poolOf (scaleAt (burstFrom), low, high);
                        const auto base = nearestIndex (here, centre);
                        int cell[4];
                        const int shape[] = { 0, 2, 1, 2 };
                        for (int c = 0; c < 4; ++c)
                            cell[c] = here[(size_t) juce::jlimit (0, (int) here.size() - 1, base + shape[c])];
                        auto k = 0;
                        for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                            addNote (notes, x, g, juce::jlimit (low, high, cell[k % 4] + k / per), velocityAt (t, k % 4 == 0));
                        break;
                    }
                    case Burst::repeatLick:
                    {
                        // A three-beat lick in picked triplets, repeated so it starts somewhere new in each bar.
                        const auto g = gridFor (1.0 / 3.0);
                        const int lick[] = { 2, 1, 0, 2, 1, 0, 1, 0, -1 };
                        auto k = 0;
                        for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                        {
                            const auto here = poolOf (scaleAt (x), low, high);
                            const auto base = nearestIndex (here, centre + 3.0);
                            const auto at = juce::jlimit (0, (int) here.size() - 1, base + lick[k % 9]);
                            addNote (notes, x, g, here[(size_t) at], velocityAt (t, k % 9 == 0));
                        }
                        break;
                    }
                    case Burst::sixes:
                    {
                        // Six-note groups down the scale in 16ths (each a step lower), drifting against the beat.
                        const auto g = gridFor (0.25);
                        const int six[] = { 0, -1, -2, -1, -2, -3 };
                        auto k = 0;
                        for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                        {
                            const auto here = poolOf (scaleAt (x), low, high);
                            const auto top = nearestIndex (here, centre + 9.0);
                            const auto at = juce::jlimit (0, (int) here.size() - 1, top - k / 6 + six[k % 6]);
                            addNote (notes, x, g, here[(size_t) at], velocityAt (t, k % 6 == 0));
                        }
                        break;
                    }
                    case Burst::fourSequence:
                    {
                        // A four-note run down from a scale note, each one starting a step higher, in 16ths.
                        const auto g = gridFor (0.25);
                        const int four[] = { 0, -1, -2, -3 };
                        auto k = 0;
                        for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                        {
                            const auto here = poolOf (scaleAt (x), low, high);
                            const auto base = nearestIndex (here, centre);
                            const auto at = juce::jlimit (0, (int) here.size() - 1, base + k / 4 + four[k % 4]);
                            addNote (notes, x, g, here[(size_t) at], velocityAt (t, k % 4 == 0));
                        }
                        break;
                    }
                    case Burst::risingFlurry:
                    {
                        const auto g = gridFor (0.25);
                        auto k = 0;
                        if (p.outburst && ph == phrases - 2)
                        {
                            // The peak: an outburst outside the key, climbing by half steps.
                            const auto start = nearestOf (chordTones (burstFrom), centre + 2.0, low, high);
                            for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                                addNote (notes, x, g, juce::jlimit (low, high, start + k), velocityAt (t, k % 4 == 0));
                            break;
                        }
                        // Up the scale, turning back down a step every fourth note.
                        for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                        {
                            const auto here = poolOf (scaleAt (x), low, high);
                            const auto base = nearestIndex (here, centre - 2.0);
                            const auto at = juce::jlimit (0, (int) here.size() - 1, base + k - 2 * (k / 4));
                            addNote (notes, x, g, here[(size_t) at], velocityAt (t, k % 4 == 0));
                        }
                        break;
                    }
                    case Burst::sequence:
                    {
                        const auto g = gridFor (0.25);
                        auto k = 0;
                        if (t > 0.75f && burstIndex > 0)
                        {
                            // The climax: something outside, a chromatic climb.
                            const auto start = nearestOf (chordTones (burstFrom), centre, low, high);
                            for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                                addNote (notes, x, g, juce::jlimit (low, high, start + k), velocityAt (t, k % 4 == 0));
                        }
                        else if (burstIndex % 2 == 0)
                        {
                            // Six-note scale cells, each starting two steps higher (turning round at the top).
                            const int cell[] = { 0, 1, 2, 3, 2, 1 };
                            for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                            {
                                const auto here = poolOf (scaleAt (x), low, high);
                                const auto base = nearestIndex (here, centre - 4.0);
                                const auto span = juce::jmax (1, (int) here.size() - 4 - base);
                                auto climb = (k / 6) * 2;
                                climb = climb % (2 * span);
                                if (climb > span) climb = 2 * span - climb;
                                const auto at = juce::jlimit (0, (int) here.size() - 1, base + climb + cell[k % 6]);
                                addNote (notes, x, g, here[(size_t) at], velocityAt (t, k % 6 == 0));
                            }
                        }
                        else
                        {
                            // The chord's seventh arpeggio, one note each, up and back down.
                            for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                            {
                                const auto& span = chordAt (x);
                                auto tones = chordTones (x);
                                const auto seventh = pcOf (span.chord.root + (span.chord.type == Chord::Type::major
                                                                              && has (scaleAt (x), pcOf (span.chord.root + 11)) ? 11 : 10));
                                if (! has (tones, seventh))
                                    tones.push_back (seventh);
                                const auto arp = poolOf (tones, low, high);
                                const auto base = nearestIndex (arp, centre - 3.0);
                                const auto up = juce::jmin (6, (int) arp.size() - 1 - base);
                                const auto pos = k % juce::jmax (1, 2 * up);
                                const auto at = base + (pos <= up ? pos : 2 * up - pos);
                                addNote (notes, x, g, arp[(size_t) juce::jlimit (0, (int) arp.size() - 1, at)], velocityAt (t, pos == 0));
                            }
                        }
                        break;
                    }
                }
                // The landing: a chord tone held.
                const auto land = nearestOf (chordTones (landing), centre + 4.0, low, high);
                addNote (notes, landing, juce::jmax (0.25, to - landing - (lastPhrase ? 0.0 : 0.05)), land, velocityAt (t, true));
                continue;
            }

            // A melodic phrase: the motif stated, answered, stated higher, developed.
            const auto kind = lastPhrase ? 0 : ph % 4;
            std::vector<double> at = motif.at;
            std::vector<int> steps = motif.steps;
            auto lift = 0;
            if (kind == 1)
            {
                // The answer: its rhythm, another ending.
                for (size_t i = steps.size() > 2 ? steps.size() - 2 : 1; i < steps.size(); ++i)
                    steps[i] = -steps[i] + (r.nextBool() ? 1 : -1);
                lift = 1;
            }
            else if (kind == 2)
                lift = 3;
            else if (kind == 3)
            {
                // Developed: as busy as this point of the solo is, the motif's shape repeated.
                const auto density = p.startDensity + (p.peakDensity - p.startDensity) * std::pow (t, 1.3f);
                at = rhythmOf (juce::roundToInt (density * p.phraseBars), phraseLength - p.restBeats,
                               gridFor (density > 8.0f ? 0.25 : 0.5), r, false);
                std::vector<int> grown;
                for (size_t i = 0; i < at.size(); ++i)
                    grown.push_back (i == 0 ? 0 : motif.steps[i % motif.steps.size()] == 0 ? 1 : motif.steps[i % motif.steps.size()]);
                steps = grown;
            }
            const auto first = nearestOf (chordTones (from), centre, low, high);
            auto pool = poolOf (scaleAt (from), low, high);
            if (pool.empty())
                continue;
            auto idx = juce::jlimit (0, (int) pool.size() - 1, nearestIndex (pool, first) + lift);
            const auto played = juce::jmin (to, total) - from;
            for (size_t i = 0; i < at.size(); ++i)
            {
                const auto x = from + at[i];
                if (at[i] >= played - 1.0e-9)
                    break;
                pool = poolOf (scaleAt (x), low, high);
                idx = juce::jlimit (0, (int) pool.size() - 1, idx + (i == 0 ? 0 : steps[i]));
                auto pitch = pool[(size_t) idx];
                const auto onBeat = std::abs (at[i] - std::round (at[i])) < 1.0e-6;
                const auto lastNote = i + 1 == at.size() || at[i + 1] >= played - 1.0e-9;
                if (lastNote)
                {
                    // The phrase ends on the chord's root or fifth.
                    const auto& c = chordAt (x).chord;
                    pitch = nearestOf ({ c.root, pcOf (c.root + 7) }, pitch, low, high);
                }
                else if (onBeat && ! has (chordTones (x), pcOf (pitch)))
                    pitch = nearestOf (chordTones (x), pitch, low, high);
                idx = nearestIndex (pool, pitch);
                const auto next = lastNote ? juce::jmin (played, phraseLength - (lastPhrase ? 0.0 : p.restBeats) + 0.0) : at[i + 1];
                const auto length = juce::jmax (0.1, (lastPhrase && lastNote ? played : next) - at[i]);
                addNote (notes, x, length, pitch, velocityAt (t, i == 0));
            }
        }

        std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        if (notes.empty())
            return solo;

        // The ending: the last note on the last chord's root, held to the end.
        {
            auto& last = notes.back();
            const auto& c = chordAt (last.beat).chord;
            last.pitch = nearestOf ({ c.root }, p.endHigh ? (double) high - 4.0 : (double) last.pitch, low, high);   // (0.67.0: or high)
            last.length = juce::jmax (last.length, total - last.beat);
            last.technique = LineNote::Technique::normal;
        }

        // Expression: on long single notes outside the bursts, a bend into the note or vibrato.
        const auto inBurst = [&burstSpans] (double beat)
        {
            for (const auto& [a, b] : burstSpans)
                if (beat >= a - 1.0e-9 && beat < b - 1.0e-9)
                    return true;
            return false;
        };
        const auto single = [&notes] (size_t i)
        {
            for (size_t k = 0; k < notes.size(); ++k)
                if (k != i && std::abs (notes[k].beat - notes[i].beat) < HumanizeLimits::together)
                    return false;
            return true;
        };
        std::vector<LineNote> added;
        auto tremoloUsed = false;
        auto pinchBar = -2;
        auto dipUsed = false;
        auto harmonicBar = -1;
        for (size_t i = 0; i < notes.size(); ++i)
        {
            auto& n = notes[i];
            if (inBurst (n.beat) || n.technique != LineNote::Technique::normal || ! single (i))
                continue;
            const auto t = (float) (n.beat / juce::jmax (1.0e-6, total));
            const auto isLast = i + 1 == notes.size();
            const auto minorRoot = chordAt (n.beat).key.minor ? chordAt (n.beat).key.pitchClass() : chordAt (n.beat).key.pitchClass() + 9;
            // Blues curls: a flat third bent a little sharp.
            if (p.curls && ! isLast && pcOf (n.pitch) == pcOf (minorRoot + 3) && n.length < 0.75 && r.nextFloat() < 0.5f)
            {
                n.effect = LineNote::Effect::bend;
                n.amount = 0.5f;
                continue;
            }
            if (n.length < 0.75 || isLast)
                continue;
            const auto bar = (int) std::floor (n.beat / beatsPerBar);
            if (p.harmonics && bar % 4 == 0 && bar != harmonicBar && t > 0.15f)
            {
                harmonicBar = bar;
                n.technique = LineNote::Technique::harmonic;
                continue;
            }
            if (p.pinchSqueals && bar - pinchBar >= 2)
            {
                // A pinch-harmonic squeal shaken wide, every two bars at most.
                pinchBar = bar;
                n.velocity = GuitarVelocity::of (vel.pinch);
                n.effect = LineNote::Effect::vibrato;
                n.amount = p.vibrato;
                continue;
            }
            if (p.dip && ! dipUsed && t > 0.5f && n.length >= 1.0)
            {
                // (0.68.0) A whammy dip: down a semitone and back, like a voice.
                dipUsed = true;
                n.effect = LineNote::Effect::dive;
                n.amount = 1.0f;
                continue;
            }
            if (p.tremolo && ! tremoloUsed && t > 0.7f)
            {
                tremoloUsed = true;
                n.technique = LineNote::Technique::tremolo;
                continue;
            }
            if (r.nextFloat() < p.bendChance)
            {
                // A bend: written under its note, bent up into it.
                auto weightSum = 0.0f;
                for (const auto& [size, w] : p.bends)
                    if (size <= 2.0f || t >= p.bigFrom)
                        weightSum += w;
                auto pick = r.nextFloat() * weightSum;
                auto size = 2.0f;
                for (const auto& [s, w] : p.bends)
                    if (s <= 2.0f || t >= p.bigFrom)
                    {
                        if (pick < w) { size = s; break; }
                        pick -= w;
                    }
                if (p.escalate)
                    size = t < 0.34f ? 1.0f : t < 0.67f ? 2.0f : 3.0f;   // (0.68.0: half, whole, a step and a half)
                const auto semis = juce::roundToInt (size);
                if (n.pitch - semis >= low)
                {
                    n.pitch -= semis;
                    n.effect = LineNote::Effect::bend;
                    n.amount = size;
                    continue;
                }
            }
            if (p.doubleStops && r.nextFloat() < 0.3f)
            {
                // A 6th double stop: a note a sixth under it.
                LineNote under = n;
                under.pitch = p.thirdDoubleStops ? n.pitch - (has (scaleAt (n.beat), pcOf (n.pitch - 4)) ? 4 : 3)   // (0.68.0: thirds)
                                                 : n.pitch - (has (scaleAt (n.beat), pcOf (n.pitch - 9)) ? 9 : 8);
                if (under.pitch >= low)
                {
                    added.push_back (under);
                    continue;
                }
            }
            if (r.nextFloat() >= p.vibratoSkip || n.pitch < low + 12)
            {
                n.effect = LineNote::Effect::vibrato;
                n.amount = p.vibrato;
            }
        }
        notes.insert (notes.end(), added.begin(), added.end());

        // The very last note: a dive (a pinch harmonic first), or held with vibrato.
        {
            auto* last = &notes.front();
            for (auto& n : notes)
                if (n.beat >= last->beat)
                    last = &n;
            if (p.scream)
            {
                // (0.67.0) Before it, a harmonic screamed up with the bar: the last held note before the end.
                LineNote* held = nullptr;
                for (auto& n : notes)
                    if (&n != last && n.length >= 0.5 && n.beat < last->beat - 1.0e-6 && ! inBurst (n.beat) && n.pitch - 3 >= low)
                        held = &n;
                if (held != nullptr)
                {
                    held->technique = LineNote::Technique::harmonic;
                    // (written 3 under its note; a note already bent: from where its own bend began)
                    held->pitch += (held->effect == LineNote::Effect::bend ? juce::roundToInt (held->amount) : 0) - 3;
                    held->effect = LineNote::Effect::bend;
                    held->amount = 3.0f;
                }
            }
            if (p.dive)
            {
                last->velocity = GuitarVelocity::of (vel.pinch);
                last->effect = LineNote::Effect::dive;
                last->amount = 12.0f;
            }
            else
            {
                last->effect = LineNote::Effect::vibrato;
                last->amount = p.vibrato;
            }
        }

        // Swing (16ths, or 8ths), and legato: a note running a hair into the next a step or two away
        // (a hammer-on or pull-off on a sampled guitar).
        if (p.swing > 0.5f)
        {
            // Only where the next note leaves room (nothing closer than the shortest note after it).
            std::vector<double> starts;
            for (const auto& n : notes)
                starts.push_back (n.beat);
            std::sort (starts.begin(), starts.end());
            const auto room = juce::jmax (0.05, shortestStep - 0.04);
            const auto unit = p.swing8 ? 0.5 : 0.25;
            const auto shift = unit * (p.swing - 0.5f) * 2.0;
            for (auto& n : notes)
            {
                const auto pos = n.beat / unit;
                const auto k = std::llround (pos);
                if (std::abs (pos - (double) k) > 1.0e-6 || k % 2 == 0)
                    continue;
                const auto next = std::upper_bound (starts.begin(), starts.end(), n.beat + 1.0e-6);
                if (next == starts.end() || *next - (n.beat + shift) >= room)
                    n.beat += shift;
            }
        }
        // (0.68.0) Laid back: the melody (not the bursts) a little behind the beat.
        if (p.lateBeats > 0.0)
            for (auto& n : notes)
                if (! inBurst (n.beat))
                    n.beat += p.lateBeats;
        std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        if (p.legato)
            for (size_t i = 0; i + 1 < notes.size(); ++i)
            {
                auto& n = notes[i];
                const auto& next = notes[i + 1];
                const auto gap = next.beat - (n.beat + n.length);
                if (n.effect == LineNote::Effect::none && n.technique != LineNote::Technique::tremolo && single (i)
                    && std::abs (next.pitch - n.pitch) <= 2 && next.pitch != n.pitch && gap < 0.05 && gap > -0.01)
                    n.length = next.beat - n.beat + 0.02;
            }
        for (auto& n : notes)
        {
            n.pitch = juce::jlimit (low - 5, high, n.pitch);
            n.length = juce::jmin (n.length, juce::jmax (0.05, total - n.beat));
        }
        solo.description = juce::String (p.description) + " (" + juce::String (phrases) + " phrases, building from about "
                         + juce::String (juce::roundToInt (p.startDensity)) + " to " + juce::String (juce::roundToInt (p.peakDensity)) + " notes a bar)";
        return solo;
    }
}
