#include "engine/Riffs.h"

#include <cstring>
#include <functional>
#include <string>

namespace pr7s
{
    namespace
    {
        int pcOf (int note) { return ((note % 12) + 12) % 12; }

        bool has (const std::vector<int>& list, int value)
        {
            return std::find (list.begin(), list.end(), value) != list.end();
        }

        int nearestPitch (int pitchClass, double around, int low, int high)
        {
            auto best = -1;
            for (auto n = low; n <= high; ++n)
                if (pcOf (n) == pcOf (pitchClass) && (best < 0 || std::abs (n - around) < std::abs (best - around)))
                    best = n;
            return best < 0 ? low : best;
        }

        int nearestOf (const std::vector<int>& pitchClasses, double around, int low, int high)
        {
            auto best = -1;
            for (const auto p : pitchClasses)
            {
                const auto n = nearestPitch (p, around, low, high);
                if (pcOf (n) != pcOf (p))
                    continue;
                if (best < 0 || std::abs (n - around) < std::abs (best - around))
                    best = n;
            }
            return best < 0 ? juce::jlimit (low, high, juce::roundToInt (around)) : best;
        }

        int stepFrom (int pitch, int dir, const std::vector<int>& pitchClasses, int low, int high)
        {
            for (const auto d : { dir, -dir })
                for (auto n = pitch + d; n >= low && n <= high; n += d)
                    if (has (pitchClasses, pcOf (n)))
                        return n;
            return juce::jlimit (low, high, pitch);
        }

        int bassOf (const Chord& chord) { return chord.bass >= 0 ? chord.bass : chord.root; }

        std::vector<int> tonesOf (const Chord& c)
        {
            const auto t = c.pitchClasses();
            return { t.begin(), t.end() };
        }

        std::vector<int> scaleOn (int tonic, std::initializer_list<int> steps)
        {
            std::vector<int> out;
            for (const auto s : steps)
                out.push_back (pcOf (tonic + s));
            return out;
        }

        int fifthAbove (const Chord& c, int root)
        {
            const auto tones = tonesOf (c);
            for (const auto step : { 7, 6, 8 })
                if (has (tones, pcOf (root + step)))
                    return root + step;
            return root + 7;
        }

        // The chord's own third above its root (a sus chord's second or fourth).
        int thirdOf (const Chord& c)
        {
            const auto tones = tonesOf (c);
            for (const auto step : { 3, 4, 2, 5 })
                if (has (tones, pcOf (c.root + step)))
                    return step;
            return 4;
        }

        float of (int midi) { return (float) juce::jlimit (1, 127, midi) / 127.0f; }

        void add (std::vector<LineNote>& notes, double at, double length, int pitch, float velocity)
        {
            if (length > 1.0e-6)
                notes.push_back ({ at, length * 0.96, juce::jlimit (0, 127, pitch), velocity });
        }

        const ChordSpan& spanAt (const std::vector<ChordSpan>& spans, double songBeat)
        {
            for (const auto& s : spans)
                if (songBeat < s.end - 1.0e-9)
                    return s;
            return spans.back();
        }

        // A repeatable chance (0-1) for a place in the part: the same notes and settings deal the same.
        float chance (double at, int salt, int variation)
        {
            juce::Random r ((juce::int64) std::llround (at * 960.0) * 7919 + salt * 104729 + variation * 31337 + 17);
            r.nextInt();
            return r.nextFloat();
        }

        void foldAndSort (LineIdea& idea, int low, int high)
        {
            for (auto& n : idea.notes)
            {
                while (n.pitch < low)
                    n.pitch += 12;
                while (n.pitch > high)
                    n.pitch -= 12;
            }
            std::stable_sort (idea.notes.begin(), idea.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        }

        //======================================================================
        // A written riff. The characters:
        //   -  rest            =  the note before held on
        //   lower case: palm muted, upper case: open (ringing), single notes above the riff's base
        //   (the chord's root low on the neck, or the pedal: the key's low note)
        //   x X  the base      c C  a half step up    d D  a minor third
        //   f F  a fourth      e E  the flat fifth    g G  a fifth        k K  a flat seventh
        //   t T  the chord's own third
        //   z Z  the pedal (the key's note on the lowest string, whatever the base)
        //   *    a palm-muted power dyad (root and fifth)     #  an open power chord (root, fifth, octave)
        //   o    an open octave shape                          b B  a root and flat fifth dyad
        //   h    a ringing chord high on the neck (its three notes)
        //   p    a pinch-harmonic squeal (the root an octave up)    r  a rake into the root an octave up
        //   (0.64.0) stabs high on the neck, two octaves over the base, always ringing:
        //   v    a tritone stab (the octave and its flat fifth) with vibrato
        //   w    a whole-step stab (the flat seventh and the octave) both bent up a whole step
        //   u    a unison ring (the ninth: two strings on one note, written as one note)
        enum class Base { root, pedal };
        struct RiffDef
        {
            const char* name;
            const char* description;
            Base base;
            const char* a;
            const char* b = nullptr;     // (none: A)
            const char* c = nullptr;     // (none: B)
            const char* altA = nullptr;  // the odd variations' A (none: A)
        };

        enum class FillKind { chromatic, burst, squeal, stop, slide };

        FillKind fillFor (GuitarStyle style, int variation)
        {
            const auto odd = variation % 2 == 1;
            switch (style)
            {
                case GuitarStyle::rock: return odd ? FillKind::chromatic : FillKind::slide;
                case GuitarStyle::thrash: return odd ? FillKind::burst : FillKind::chromatic;
                case GuitarStyle::numetal: return odd ? FillKind::squeal : FillKind::stop;
                case GuitarStyle::metalcore: return odd ? FillKind::squeal : FillKind::burst;
                case GuitarStyle::shred: return odd ? FillKind::burst : FillKind::chromatic;
                case GuitarStyle::groove: return odd ? FillKind::squeal : FillKind::slide;
            }
            return FillKind::chromatic;
        }

        const std::vector<RiffDef>& riffsOf (GuitarStyle style)
        {
            static const std::vector<RiffDef> thrash {
                { "Downpicked gallop", "Palm-muted gallops on the low string, the chord's power chord ringing at the bar's end (thrash, speed metal)",
                  Base::pedal, "z-zzz-zzz-zz#===", "z-zzz-zzz-zz*-*-", "z-zzz-zz#===#===", "zz-zzz-zzz-z#===" },
                { "Tremolo pedal", "Sixteenth-note chugs on the low string, the chord stabbed open between them (thrash at full speed)",
                  Base::pedal, "zzzz#===zzzz*-*-", "zzzz#===zzzz#===", "zzzzzzzz#=#=#===" },
                { "Chromatic riff", "The low note palm muted with the half step, minor third and flat fifth above it: the evil sound of thrash",
                  Base::pedal, "z-zcz-zdz-zez-zc", "z-zcz-zdz-zc#===", "z-zcz-zdzezdzczz", "zcz-zdz-zez-zdzc" },
                { "Skank beat", "Eighth-note chugs against power chords on the off-beats, with the drummer's thrash beat (crossover)",
                  Base::pedal, "z-*-z-*-z-*-z-*-", "z-*-z-*-#===#===", "z-*-z-*-z-*-zzzz" },
                { "Open-string stabs", "Power chords stabbed and cut short, the open string chugging in the gaps (Bay Area thrash)",
                  Base::pedal, "#-z-zz#-z-zz#===", "#-z-zz#-z-zz*-*-", nullptr, "#==-zzzz#==-zzzz" },
                { "Stop-time break", "Hits together with the drums and silence between them, then a run of chugs (thrash breaks)",
                  Base::pedal, "#===----#===----", "#-#-#-#-zzzzzzzz", "#===----#-#-zzzz" } };
            static const std::vector<RiffDef> numetal {
                { "Bounce riff", "Syncopated low notes with space between them, the half step above to push it (the nu-metal bounce)",
                  Base::pedal, "z--z--z-z--z--c-", "z--z--z-z--z-c-e", nullptr, "z-z--z--z-z--zc-" },
                { "Stop-start", "A burst of chugs, silence, then a pinch-harmonic squeal (nu-metal stops)",
                  Base::pedal, "z-z---z--------p", "z-z---z---z-z---", "zzzz----z-z----p" },
                { "Tritone chug", "Chugs against the flat fifth for a dark, dissonant grind",
                  Base::pedal, "z-e-z-e-zze-c---", "z-e-z-e-zzb===--" },
                { "Groove and octave", "Low chugs answered by a ringing octave shape on the chord",
                  Base::pedal, "z--zo===z--z-c--", "z--zo===z--zo===" },
                { "Half-step creep", "Heavy low notes left ringing, creeping a half step up and back (nu-metal verses)",
                  Base::pedal, "Z===C===Z===E===", "Z===C===D===C===" },
                { "Dissonant ring", "A power chord, then a chord high on the neck left ringing over it (the quiet-loud verse)",
                  Base::root, "#=======h=======", "#===----h=======", nullptr, "h=======#===z-z-" } };
            static const std::vector<RiffDef> metalcore {
                { "Breakdown", "Half-time chugs with space between them, on the kick drum's hits (metalcore breakdowns)",
                  Base::pedal, "z--z--z-----z-z-", "z--z--z---z-p---", "z--z--z-z-z-zzzz" },
                { "Chug and stop", "Bursts of chugs cut dead: the space as heavy as the notes",
                  Base::pedal, "zzzz----zzzz----", "zzzz----zz-zp---" },
                { "Melodic octave riff", "Ringing octave shapes moving with the chords (the melodic metalcore chorus)",
                  Base::root, "o===o===o=o=o===", "o===o===o=======" },
                { "Squeal breakdown", "Slow chugs, the bar ending on a held pinch-harmonic squeal",
                  Base::pedal, "z---z---z-z-p===", "z---z---zzzzp===" },
                { "Triplet burst", "Bursts of sixteenth-note-triplet chugs between the hits (modern metalcore)",
                  Base::pedal, "zzz---zzz---zzz---#=====", "zzz---zzz---#=====zzzzzz" },
                { "Open chorus chords", "Power chords left ringing, then chords high on the neck (big metalcore choruses)",
                  Base::root, "#=======#===#===", "h=======h===#===" } };
            static const std::vector<RiffDef> shred {
                { "Power metal gallop", "Gallops on the chord's root, ringing chords at the end of the phrase (power metal)",
                  Base::root, "x-xxx-xxx-xxx-xx", "x-xxx-xxx-xx#===", "x-xxx-xx#===#===" },
                { "Neoclassical pedal", "The low note picked between the fifth, fourth and minor third above it (neoclassical metal rhythm)",
                  Base::pedal, "zgzfzgzdzgzfzgzd", "zgzfzgzd#=======" },
                { "Driving eighths", "Palm-muted power dyads on every eighth note, opening up at the end of the phrase",
                  Base::root, "*-*-*-*-*-*-*-*-", "*-*-*-*-#===#===" },
                { "Double-time sixteenths", "Sixteenth-note power dyads, each beat's first open (double-time metal)",
                  Base::root, "#***#***#***#***", "#***#***#===#===" },
                { "Held chords", "Power chords held under the solo, out of its way",
                  Base::root, "#===============", "#=======#=======" },
                { "Arpeggiated power chords", "The chord's root, fifth and third picked one at a time and left ringing (melodic power metal)",
                  Base::root, "XGTGXGTGXGTGXGTG", "XGTGXGTG#=======" } };
            static const std::vector<RiffDef> groove {
                { "Tritone stab", "A high tritone left ringing with vibrato, then palm-muted chugs on the low string; power chords to turn it round",
                  Base::pedal, "v=====z-z-z-z---", "v=====z-z-z-zz#=", "z-z-z-z-#===#===" },
                { "Bent stab", "Chugs answered by a high whole step bent up a whole step; the phrase ends on a ringing unison",
                  Base::pedal, "z===w=z---z-w===", "z===w=z---z-u===" },
                { "Backbeat stabs", "High stabs on beats two and four, where the snare hits, chugs between them",
                  Base::pedal, "z-z-v=z-z-z-v=z-", "z-zzv=z-z-z-w===" },
                { "Off-beat stabs", "Short tritone stabs on the off-beats against eighth-note chugs",
                  Base::pedal, "z-v=z-v=zzv=z-v=", "z-v=z-v=zzw===--" },
                { "Stab gallop", "A gallop of chugs, then a ringing stab answering it",
                  Base::pedal, "z-zzv===z-zzv===", "z-zzv===z-zzw===" },
                { "Half-time stabs", "Heavy half-time chugs with a long bent stab, then a ringing unison (the breakdown)",
                  Base::pedal, "z--z--w=====z-z-", "z--z--v=====u===" } };
            static const std::vector<RiffDef> none;
            switch (style)
            {
                case GuitarStyle::thrash: return thrash;
                case GuitarStyle::numetal: return numetal;
                case GuitarStyle::metalcore: return metalcore;
                case GuitarStyle::shred: return shred;
                case GuitarStyle::groove: return groove;
                case GuitarStyle::rock: break;
            }
            return none;
        }

        //======================================================================
        // A written lead: per bar a rhythm (a character per step, as the riffs', 16 a bar or 12 / 24
        // for triplets) and the moves its line makes through the scale.
        //   n  the next note (the bar's first: a chord tone near the last bar's end; then each moves by
        //      the next of `moves`, in scale steps)
        //   r  the same note again (tremolo)     P  the bar's first note again (a pedal)
        //   p  the next note as a pinch harmonic  =  held on   -  rest
        enum class LeadScale { pentatonic, blues, natural, neo, chromatic, chordTones, available, diminished, dark };
        struct LeadDef
        {
            const char* name;
            const char* description;
            LeadScale scale;
            const char* rhythm;
            const char* rhythmB;          // every second bar's (none: the same)
            std::vector<int> moves;
            bool harmony = false;         // a third above in the key (twin guitars)
            bool octave = false;          // doubled an octave up
            bool startHigh = false;
            bool legato = false;          // the notes after each group's first softer (hammer-ons, pull-offs)
        };

        const std::vector<LeadDef>& leadsOf (GuitarStyle style)
        {
            using S = LeadScale;
            static const std::vector<LeadDef> thrash {
                { "Chromatic tremolo", "Each note tremolo picked for a beat, sliding down by half steps (thrash solos)",
                  S::chromatic, "nrrrnrrrnrrrnrrr", nullptr, { -1 }, false, false, true },
                { "Harmonic minor sixes", "Groups of six down the harmonic minor scale, each group a step lower (fast thrash runs)",
                  S::neo, "nnnnnnnnnnnnnnnnnnnnnnnn", nullptr, { -1, -1, -1, -1, -1, 4 }, false, false, true },
                { "Diminished climb", "Up a diminished arpeggio in sixteenths from a chord tone: the tense, menacing sound",
                  S::diminished, "nnnnnnnnnnnnnnnn", nullptr, { 1 } },
                { "Screaming tremolo", "A high note tremolo picked as fast as it can, a step up, then a held squeal",
                  S::available, "nrrrrrrrnrrrp===", nullptr, { 1 }, false, false, true },
                { "Pedal-point lick", "The bar's first note picked between notes stepping down the scale (thrash and neoclassical)",
                  S::neo, "PnPnPnPnPnPnPnPn", nullptr, { -1 }, false, false, true },
                { "Twin-guitar thirds", "An eighth-note melody with a second guitar a third above it (the twin lead)",
                  S::natural, "n-n-n-n-n-n-n===", nullptr, { 1, 1, -1, 1, 1, -2 }, true } };
            static const std::vector<LeadDef> numetal {
                { "Octave melody", "A slow melody doubled in octaves, ringing over the riff (nu-metal choruses)",
                  S::pentatonic, "n===n-n-n=======", nullptr, { 1, -1, -1, 2 }, false, true },
                { "Siren", "Two notes a half step apart, back and forth: the siren sound",
                  S::chromatic, "n-n-n-n-n-n-n-n-", nullptr, { 1, -1 }, false, false, true },
                { "Pinch pings", "Pinch harmonics dotted over the groove",
                  S::available, "p-------p---p---", nullptr, { 2, -1 } },
                { "Dissonant high line", "Long high notes on the flat second and flat fifth: eerie",
                  S::dark, "n===n===n=======", nullptr, { 1, -2, 1 }, false, false, true },
                { "Tremolo swell", "One high chord tone tremolo picked through the bar, following the chords",
                  S::available, "nrrrrrrrrrrrrrrr", nullptr, {}, false, false, true },
                { "Single-note hook", "A short single-note hook, repeated with the chords",
                  S::pentatonic, "n-n-nn--n-n-n---", nullptr, { 0, -1, 1, -1, -1, 1 } } };
            static const std::vector<LeadDef> metalcore {
                { "Twin harmony lead", "A melodic lead with a second guitar a third above it (melodic metalcore)",
                  S::natural, "n-nnn-n-n-nnn===", nullptr, { 1, 1, -1, -1, 2, -1, -1, 1 }, true },
                { "Octave chorus melody", "A big, slow melody doubled in octaves for the chorus",
                  S::natural, "n===n===n-n-n===", nullptr, { 1, 1, -2, 1 }, false, true },
                { "Tapped arpeggio", "The chord's notes tapped high on the neck in sextuplets (one, two, three, two)",
                  S::chordTones, "nnnnnnnnnnnnnnnnnnnnnnnn", nullptr, { 1, 1, -1, -1, 1, -1 }, false, false, true, true },
                { "Melodic sixteenth run", "Groups of four sixteenths down the scale, each group a step lower",
                  S::natural, "nnnnnnnnnnnnnnnn", nullptr, { -1, -1, -1, 2 }, false, false, true },
                { "Pinch and hold", "Long held notes, the bar's last a pinch-harmonic squeal",
                  S::available, "n=======n===p===", nullptr, { 1, 1 } },
                { "Call and answer", "A phrase, a bar's rest, then its answer (leaves room for the vocal)",
                  S::natural, "n-n-n===--------", "--------n-n-n===", { 1, -1 } } };
            static const std::vector<LeadDef> shred {
                { "Sweep arpeggio", "Up and down the chord's notes across the strings in one sweep, in sextuplets",
                  S::chordTones, "nnnnnnnnnnnnnnnnnnnnnnnn", nullptr, { 1, 1, 1, 1, 1, -1, -1, -1, -1, -1 } },
                { "Legato run", "Three notes a string, climbing: each group of six a step higher (hammer-ons and pull-offs)",
                  S::natural, "nnnnnnnnnnnnnnnnnnnnnnnn", nullptr, { 1, 1, 1, 1, 1, -4 }, false, false, false, true },
                { "Harmonic minor fours", "Groups of four up the harmonic minor scale, each group a step higher (neoclassical)",
                  S::neo, "nnnnnnnnnnnnnnnn", nullptr, { 1, 1, 1, -2 } },
                { "Diminished arpeggio", "Up and down a diminished arpeggio from a chord tone in sextuplets",
                  S::diminished, "nnnnnnnnnnnnnnnnnnnnnnnn", nullptr, { 1, 1, 1, -1, -1, -1 } },
                { "String skipping", "Big jumps between the chord's notes, a string skipped each time",
                  S::chordTones, "nnnnnnnnnnnnnnnn", nullptr, { 3, -1, -1, -1 } },
                { "Pedal-tone tapping", "A high note tapped between notes climbing the scale, in sextuplets",
                  S::neo, "PnPnPnPnPnPnPnPnPnPnPnPn", nullptr, { 1 }, false, false, false, true } };
            static const std::vector<LeadDef> none;
            switch (style)
            {
                case GuitarStyle::thrash: return thrash;
                case GuitarStyle::numetal: return numetal;
                case GuitarStyle::metalcore: return metalcore;
                case GuitarStyle::shred: return shred;
                case GuitarStyle::groove: return numetal;   // (0.64.0: the nu-metal leads: octave melodies, sirens, pinch pings)
                case GuitarStyle::rock: break;
            }
            return none;
        }

        bool isSounding (char c) { return c != '-' && c != '.' && c != '=' && c != ' '; }
    }

    //==========================================================================
    const std::vector<GuitarStyle>& guitarStyles()
    {
        static const std::vector<GuitarStyle> styles { GuitarStyle::rock, GuitarStyle::thrash, GuitarStyle::numetal, GuitarStyle::metalcore, GuitarStyle::shred, GuitarStyle::groove };
        return styles;
    }

    juce::String guitarStyleName (GuitarStyle style)
    {
        switch (style)
        {
            case GuitarStyle::rock: return "ROCK";
            case GuitarStyle::thrash: return "THRASH";
            case GuitarStyle::numetal: return "NU-METAL";
            case GuitarStyle::metalcore: return "METALCORE";
            case GuitarStyle::shred: return "SHRED";
            case GuitarStyle::groove: return "GROOVE";
        }
        return "ROCK";
    }

    GuitarStyle guitarStyleNamed (const juce::String& name)
    {
        for (const auto s : guitarStyles())
            if (guitarStyleName (s).equalsIgnoreCase (name.trim()))
                return s;
        return GuitarStyle::rock;
    }

    juce::String guitarStyleDescription (GuitarStyle style)
    {
        switch (style)
        {
            case GuitarStyle::rock: return "Rock and classic metal: power chords, chugs, gallops, strums; pentatonic and blues leads";
            case GuitarStyle::thrash: return "Thrash: fast downpicked gallops, sixteenth pedals, chromatic riffs; tremolo and harmonic minor leads";
            case GuitarStyle::numetal: return "Nu-metal: syncopated bounce, stops, the flat fifth and half step; octave melodies, sirens, pinch pings";
            case GuitarStyle::metalcore: return "Metalcore: breakdowns, chug-and-stop, triplet bursts, octave choruses; twin harmony and tapped leads";
            case GuitarStyle::shred: return "Shred: power metal rhythm under sweeps, legato runs, harmonic minor and diminished arpeggios, tapping";
            case GuitarStyle::groove: return "Groove: low chugs answered by high dissonant stabs (tritones with vibrato, bent whole steps, unisons); nu-metal leads";
        }
        return {};
    }

    //==========================================================================
    RiffSettings RiffSettings::limited() const
    {
        RiffSettings r = *this;
        r.bars = bars <= 1 ? 1 : bars <= 2 ? 2 : 4;
        r.palmMutes = juce::jlimit (0.0f, 1.0f, palmMutes);
        r.tightness = juce::jlimit (0.0f, 1.0f, tightness);
        r.busyness = juce::jlimit (0.0f, 1.0f, busyness);
        return r;
    }

    juce::String RiffSettings::toString() const
    {
        return juce::String (bars) + "," + juce::String (palmMutes, 2) + "," + juce::String (tightness, 2) + "," + juce::String (busyness, 2) + "," + (fills ? "1" : "0");
    }

    RiffSettings RiffSettings::fromString (const juce::String& text)
    {
        const auto parts = juce::StringArray::fromTokens (text, ",", "");
        if (parts.size() != 5)
            return {};
        for (const auto& p : parts)
            if (p.trim().isEmpty() || ! p.trim().containsOnly ("0123456789."))
                return {};
        RiffSettings r;
        r.bars = parts[0].getIntValue();
        r.palmMutes = parts[1].getFloatValue();
        r.tightness = parts[2].getFloatValue();
        r.busyness = parts[3].getFloatValue();
        r.fills = parts[4].trim() == "1";
        return r.limited();
    }

    //==========================================================================
    //==========================================================================
    // (0.85.0) A riff's bars (the characters above) played on the chords: the written riffs' and the generator's.
    LineIdea riffFromBars (const std::function<std::string (int barIndex)>& barOf, bool pedalBase, const juce::String& name,
                           const juce::String& description, const std::vector<ChordSpan>& spans, double beatsPerBar, int variation,
                           const GuitarTuning& tuning, const GuitarVelocities& vel)
    {
        LineIdea idea { LineIdea::Part::rhythm, name, description, {} };
        if (spans.empty() || beatsPerBar <= 0.0)
            return idea;
        const auto origin = spans.front().start;
        const auto low = tuning.lowest(), high = tuning.highest();
        std::vector<int> roots;
        {
            double prev = low + 3.0 + (variation % 3);
            for (const auto& s : spans)
            {
                const auto n = nearestPitch (bassOf (s.chord), prev, low, low + 11);
                roots.push_back (n);
                prev = n;
            }
        }
        std::vector<int> lastHigh;   // the high chord's voicing, kept moving little
        for (size_t si = 0; si < spans.size(); ++si)
        {
            const auto& span = spans[si];
            const auto root = roots[si];
            const auto pedal = nearestPitch (span.key.pitchClass(), low, low, low + 11);
            const auto base = pedalBase ? pedal : root;
            const auto fifth = fifthAbove (span.chord, root);
            for (auto t = span.start; t < span.end - 1.0e-9; t += beatsPerBar)
            {
                const auto barIndex = (int) std::floor ((t - origin) / beatsPerBar + 1.0e-9);
                const auto barText = barOf (barIndex);
                const char* bar = barText.c_str();
                const auto steps = (int) barText.size();
                if (steps == 0)
                    continue;
                const auto step = beatsPerBar / steps;
                for (auto i = 0; i < steps; ++i)
                {
                    const auto ch = bar[i];
                    if (! isSounding (ch))
                        continue;
                    auto held = 1;
                    while (i + held < steps && bar[i + held] == '=')
                        ++held;
                    const auto at = t + i * step;
                    if (at >= span.end - 1.0e-9)
                        break;
                    const auto length = juce::jmin (held * step, span.end - at);
                    const auto pos = at - t;
                    const auto barStart = i == 0;
                    const auto onBeat = std::abs (pos - std::round (pos)) < 1.0e-6;
                    const auto mutedVelocity = of (barStart ? vel.muteLoose : step * held < 0.3 ? vel.muteTight : vel.muteMid);
                    const auto openVelocity = of (barStart ? vel.sustainHard : onBeat ? vel.sustain : vel.open (vel.sustain - 6));
                    const auto when = at - origin;
                    const auto lower = (char) juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) ch);
                    const auto open = ch != lower;   // (upper case)
                    const auto single = [&] (int pitch)
                    {
                        add (idea.notes, when, open ? length : length * 0.9, pitch, open ? openVelocity : mutedVelocity);
                    };
                    switch (lower)
                    {
                        case 'x': single (base); break;
                        case 'c': single (base + 1); break;
                        case 'd': single (base + 3); break;
                        case 'f': single (base + 5); break;
                        case 'e': single (base + 6); break;
                        case 'g': single (base + 7); break;
                        case 'k': single (base + 10); break;
                        case 't': single (base == root ? root + thirdOf (span.chord) : base + thirdOf (span.chord)); break;
                        case 'z': single (pedal); break;
                        case 'b':
                            for (const auto p : { root, root + 6 })
                                add (idea.notes, when, open ? length : length * 0.9, p, open ? openVelocity : mutedVelocity);
                            break;
                        case '*':
                            for (const auto p : { root, fifth })
                                add (idea.notes, when, length * 0.9, p, mutedVelocity);
                            break;
                        case '#':
                            for (const auto p : { root, fifth, root + 12 })
                                add (idea.notes, when, length, p, of (barStart || onBeat ? vel.sustainHard : vel.sustain));
                            break;
                        case 'o':
                            for (const auto p : { root, root + 12 })
                                add (idea.notes, when, length, p, openVelocity);
                            break;
                        case 'h':
                        {
                            const auto voicings = voicingsFor (span.chord, 52);
                            const auto notes = voicings.empty() ? std::vector<int> { root + 24 } : voicings[(size_t) closestVoicing (voicings, lastHigh, {})];
                            lastHigh = notes;
                            for (const auto p : notes)
                                add (idea.notes, when, length, p, of (vel.sustain));
                            break;
                        }
                        case 'p': add (idea.notes, when, length, root + 12, of (vel.pinch)); break;
                        case 'r': add (idea.notes, when, length, root + 12, of (vel.rake)); break;
                        case 'v': case 'w': case 'u':
                        {
                            const auto stab = base + 24;
                            const auto pitches = lower == 'v' ? std::vector<int> { stab, stab + 6 }
                                               : lower == 'w' ? std::vector<int> { stab - 2, stab }
                                                              : std::vector<int> { stab + 2 };
                            for (const auto p : pitches)
                            {
                                add (idea.notes, when, length, p, of (barStart || onBeat ? vel.sustainHard : vel.sustain));
                                idea.notes.back().effect = lower == 'v' ? LineNote::Effect::vibrato : lower == 'w' ? LineNote::Effect::bend : LineNote::Effect::ring;
                            }
                            break;
                        }
                        // (0.85.0, TRITONE CHUG: the owner's Ten Ton Hammer intro) the high tritone chords, open and
                        // left to ring over the low string: q the flat fifth and the fifth together two octaves up (a
                        // half step apart: the grind), y the flat fifth doubled (an octave apart), j the root and the
                        // flat fifth two octaves up, n the grind an octave lower.
                        // (0.85.0, the owner's third example: Flaw's Whole, its breakdown) ^: the grind a ninth over
                        // the chugged chord's root (the flat ninth and the ninth, a half step apart), ringing.
                        case '^':
                            for (const auto p : { base + 25, base + 26 })
                                add (idea.notes, when, length, p, of (barStart || onBeat ? vel.sustainHard : vel.sustain));
                            break;
                        case 'q': case 'y': case 'j': case 'n':
                        {
                            const auto pitches = lower == 'q' ? std::vector<int> { base + 30, base + 31 }
                                               : lower == 'y' ? std::vector<int> { base + 18, base + 30 }
                                               : lower == 'j' ? std::vector<int> { base + 24, base + 30 }
                                                              : std::vector<int> { base + 18, base + 19 };
                            for (const auto p : pitches)
                                add (idea.notes, when, length, p, of (barStart || onBeat ? vel.sustainHard : vel.sustain));
                            break;
                        }
                        // (0.85.0, the djent kind: the owner's Periphery's Wildfire) a: the low tritone chord, the low
                        // string a half step up under the open fifth and octave (b2, 5, 8: the b2 and the 5 a tritone
                        // apart; lower case muted, upper case ringing); i: its pinch harmonic (the pinch velocity);
                        // l / m / s: a single note on the low string a half step / whole step / minor third up, bent
                        // up a half step.
                        case 'a':
                            for (const auto p : { base + 1, base + 7, base + 12 })
                                add (idea.notes, when, open ? length : length * 0.9, p, open ? of (barStart || onBeat ? vel.sustainHard : vel.sustain) : mutedVelocity);
                            break;
                        case 'i':
                            for (const auto p : { base + 1, base + 7 })
                                add (idea.notes, when, length, p, of (vel.pinch));
                            break;
                        case 'l': case 'm': case 's':
                            add (idea.notes, when, length, base + (lower == 'l' ? 1 : lower == 'm' ? 2 : 3), of (vel.sustain));
                            if (! idea.notes.empty())
                            {
                                idea.notes.back().effect = LineNote::Effect::bend;
                                idea.notes.back().amount = 1.0f;
                            }
                            break;
                        // (0.85.0) A power chord on the degree the digit names (semitones over the low note: 3 the flat
                        // third, 6 the flat fifth...): the answer bar's dark walk.
                        case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                        {
                            const auto r = base + (lower - '0');
                            for (const auto p : { r, r + 7, r + 12 })
                                add (idea.notes, when, length, p, of (barStart || onBeat ? vel.sustainHard : vel.sustain));
                            break;
                        }
                        // (0.87.0, DRIVING EIGHTHS, Tears Don't Fall) % a full chord strummed open (root, fifth,
                        // octave and the chord's third over it), & the same with the fourth in the third's place (the
                        // top voice moving), ! a dead-string scratch (the chord's notes, choked: quiet and short).
                        case '%': case '&':
                        {
                            const auto top = root + 12 + (lower == '&' ? 5 : thirdOf (span.chord));
                            for (const auto p : { root, fifth, root + 12, top })
                                add (idea.notes, when, length, p, of (barStart || onBeat ? vel.sustainHard : vel.sustain));
                            break;
                        }
                        // (0.87.0, DRIVING EIGHTHS' picked bars) + the root an octave up, ; the fifth over that, open singles.
                        case '+': single (root + 12); break;
                        case ';': single (fifth + 12); break;
                        case '!':
                            for (const auto p : { root, fifth, root + 12 })
                                add (idea.notes, when, juce::jmin (length, 0.1), p, of (vel.muteTight));
                            break;
                        // (0.87.0, DRIVING EIGHTHS) the walk into the chord at the next bar line (or this chord's end):
                        // > a whole step and < a half step over its root, ( and ) under it; open single notes ({ } [ ]
                        // the same as power chords). The
                        // same chord going on: a muted power chord instead. (After the last chord: the first, as the
                        // riff goes round.)
                        case '>': case '<': case '(': case ')':
                        case '{': case '}': case '[': case ']':
                        {
                            const auto nextAt = juce::jmin (t + beatsPerBar, span.end);
                            auto next = (size_t) 0;
                            auto found = false;
                            for (size_t j = 0; j < spans.size(); ++j)
                                if (nextAt >= spans[j].start - 1.0e-9 && nextAt < spans[j].end - 1.0e-9)
                                {
                                    next = j;
                                    found = true;
                                    break;
                                }
                            if (! found)
                                next = 0;
                            if (next == si || spans[next].chord == span.chord)
                            {
                                for (const auto p : { root, fifth })
                                    add (idea.notes, when, length * 0.9, p, mutedVelocity);
                                break;
                            }
                            const auto by = (lower == '>' || lower == '{') ? 2 : (lower == '<' || lower == '}') ? 1 : (lower == '(' || lower == '[') ? -2 : -1;
                            const auto to = roots[next] + by;
                            if (lower == '{' || lower == '}' || lower == '[' || lower == ']')
                                for (const auto p : { to, to + 7, to + 12 })   // (the power chord itself steps: The End of Heartache)
                                    add (idea.notes, when, length, p, of (barStart || onBeat ? vel.sustainHard : vel.sustain));
                            else
                                add (idea.notes, when, length, to, openVelocity);
                            break;
                        }
                        default: break;
                    }
                }
            }
        }
        foldAndSort (idea, low, high);
        return idea;
    }

    std::vector<LineIdea> styleRhythmIdeas (GuitarStyle style, const std::vector<ChordSpan>& spans, double beatsPerBar, int variation,
                                            const GuitarTuning& tuning, const GuitarVelocities& vel, const RiffSettings& settings)
    {
        std::vector<LineIdea> ideas;
        if (spans.empty() || beatsPerBar <= 0.0)
            return ideas;
        const auto riff = settings.limited();
        for (const auto& def : riffsOf (style))
        {
            const auto barOf = [&def, &riff, variation] (int barIndex)
            {
                const char* a = variation % 2 == 1 && def.altA != nullptr ? def.altA : def.a;
                const char* b = def.b != nullptr ? def.b : a;
                const char* c = def.c != nullptr ? def.c : b;
                return std::string (riff.bars == 1 ? a : riff.bars == 2 ? (barIndex % 2 == 0 ? a : b)
                                                   : (barIndex % 4 == 1 ? b : barIndex % 4 == 3 ? c : a));
            };
            ideas.push_back (riffFromBars (barOf, def.base == Base::pedal, def.name, def.description, spans, beatsPerBar, variation, tuning, vel));
        }
        for (auto& idea : ideas)
            shapeRiff (idea, spans, beatsPerBar, riff, vel, style, variation, tuning);
        return ideas;
    }

    //==========================================================================
    void shapeRiff (LineIdea& idea, const std::vector<ChordSpan>& spans, double beatsPerBar, const RiffSettings& settings, const GuitarVelocities& vel,
                    GuitarStyle style, int variation, const GuitarTuning& tuning, bool strummed)
    {
        if (spans.empty() || idea.notes.empty() || beatsPerBar <= 0.0)
            return;
        const auto riff = settings.limited();
        const auto low = tuning.lowest(), high = tuning.highest();
        const auto origin = spans.front().start;
        const auto total = spans.back().end - origin;
        auto& notes = idea.notes;
        std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        const auto muted = [] (const LineNote& n) { const auto m = GuitarVelocity::midiOf (n.velocity); return m >= 1 && m <= GuitarVelocity::mutedMost; };
        const auto openNote = [] (const LineNote& n) { const auto m = GuitarVelocity::midiOf (n.velocity); return m > GuitarVelocity::mutedMost && m <= GuitarVelocity::sustainMost; };
        const auto barPos = [&] (double beat) { auto p = std::fmod (beat, beatsPerBar); return p > beatsPerBar - 1.0e-6 ? 0.0 : p; };
        const auto isBarStart = [&] (double beat) { return barPos (beat) < 1.0e-6; };
        const auto isOnBeat = [&] (double beat) { const auto p = barPos (beat); return std::abs (p - std::round (p)) < 1.0e-6; };
        const auto plain = [] (const LineNote& n) { return n.effect == LineNote::Effect::none; };   // (0.64.0: stabs left as written)

        // Groups: notes starting together (a chord, a strum).
        struct Group { size_t first, last; double at; };
        const auto groupsOf = [&]
        {
            std::vector<Group> groups;
            for (size_t i = 0; i < notes.size(); ++i)
                if (! groups.empty() && notes[i].beat - groups.back().at < HumanizeLimits::together)
                    groups.back().last = i;
                else
                    groups.push_back ({ i, i, notes[i].beat });
            return groups;
        };
        const auto all = [&] (const Group& g, auto&& is)
        {
            for (auto i = g.first; i <= g.last; ++i)
                if (! is (notes[i]))
                    return false;
            return true;
        };

        // PALM MUTES.
        if (riff.palmMutes > 0.5f)
        {
            const auto p = (riff.palmMutes - 0.5f) * 2.0f;
            for (const auto& g : groupsOf())
                if (! isBarStart (g.at) && all (g, openNote) && all (g, plain) && chance (g.at, 1, variation) < (isOnBeat (g.at) ? p * 0.5f : p))
                    for (auto i = g.first; i <= g.last; ++i)
                    {
                        notes[i].velocity = of (vel.muteMid);
                        notes[i].length = juce::jmin (notes[i].length, 0.45);
                    }
        }
        else if (riff.palmMutes < 0.5f)
        {
            const auto p = (0.5f - riff.palmMutes) * 2.0f;
            const auto groups = groupsOf();
            for (size_t k = 0; k < groups.size(); ++k)
            {
                const auto& g = groups[k];
                if (! all (g, muted) || chance (g.at, 2, variation) >= (isBarStart (g.at) || isOnBeat (g.at) ? p : p * 0.6f))
                    continue;
                // Let ring: open, sounding until the next hit.
                const auto until = k + 1 < groups.size() ? groups[k + 1].at : total;
                for (auto i = g.first; i <= g.last; ++i)
                {
                    notes[i].velocity = of (vel.open (vel.sustain - 6));
                    notes[i].length = juce::jmax (notes[i].length, (until - g.at) * 0.96);
                }
            }
        }

        // TIGHTNESS: the palm mutes' velocity toward the tight or the loose one; tighter, shorter.
        if (std::abs (riff.tightness - 0.5f) > 1.0e-4f)
        {
            const auto k = std::abs (riff.tightness - 0.5f) * 2.0f;
            const auto target = riff.tightness > 0.5f ? vel.muteTight : vel.muteLoose;
            for (auto& n : notes)
                if (muted (n))
                {
                    const auto m = GuitarVelocity::midiOf (n.velocity);
                    n.velocity = of (juce::jlimit (1, GuitarVelocity::mutedMost, juce::roundToInt ((float) m + ((float) target - (float) m) * k)));
                    if (riff.tightness > 0.5f)
                        n.length = juce::jmax (0.05, n.length * (1.0 - 0.3 * k));
                }
        }

        // BUSYNESS, fewer: weak hits left out (the open note before rings on into the space).
        if (riff.busyness < 0.5f)
        {
            const auto p = (0.5f - riff.busyness) * 2.0f;
            std::vector<bool> gone (notes.size(), false);
            const auto groups = groupsOf();
            int lastKept = -1;
            for (int k = 0; k < (int) groups.size(); ++k)
            {
                const auto& g = groups[(size_t) k];
                const auto special = ! all (g, [&] (const LineNote& n) { return (muted (n) || openNote (n)) && plain (n); });
                if (isBarStart (g.at) || special || chance (g.at, 3, variation) >= (isOnBeat (g.at) ? p * 0.35f : p))
                {
                    lastKept = k;
                    continue;
                }
                for (auto i = g.first; i <= g.last; ++i)
                    gone[i] = true;
                if (lastKept >= 0)
                {
                    const auto& before = groups[(size_t) lastKept];
                    if (all (before, openNote))
                    {
                        auto end = g.at;
                        for (auto i = g.first; i <= g.last; ++i)
                            end = juce::jmax (end, notes[i].beat + notes[i].length);
                        for (auto i = before.first; i <= before.last; ++i)
                            notes[i].length = juce::jmax (notes[i].length, end - notes[i].beat);
                    }
                }
            }
            std::vector<LineNote> kept;
            for (size_t i = 0; i < notes.size(); ++i)
                if (! gone[i])
                    kept.push_back (notes[i]);
            notes = std::move (kept);
        }
        // BUSYNESS, more: palm-muted chugs in the silent sixteenths (not over a ringing chord, not in
        // a bar of triplets, not in a strum).
        else if (riff.busyness > 0.5f && ! strummed)
        {
            const auto p = (riff.busyness - 0.5f) * 2.0f;
            const auto sixteenths = (int) std::round (beatsPerBar * 4.0);
            std::vector<LineNote> added;
            for (auto barStart = 0.0; barStart < total - 1.0e-9; barStart += beatsPerBar)
            {
                auto triplets = false;
                for (const auto& n : notes)
                    if (n.beat >= barStart - 1.0e-9 && n.beat < barStart + beatsPerBar - 1.0e-9)
                    {
                        const auto q = (n.beat - barStart) * 4.0;
                        if (std::abs (q - std::round (q)) > 0.01)
                            triplets = true;
                    }
                if (triplets)
                    continue;
                for (auto i = 1; i < sixteenths; ++i)
                {
                    const auto at = barStart + i * 0.25;
                    if (at >= total - 1.0e-9)
                        break;
                    auto taken = false;
                    int lowestBefore = -1;
                    for (const auto& n : notes)
                    {
                        if (std::abs (n.beat - at) < 0.01 || (openNote (n) && n.beat < at && n.beat + n.length > at + 0.01))
                            taken = true;
                        if (n.beat < at && (lowestBefore < 0 || n.beat > notes[(size_t) lowestBefore].beat
                                            || (std::abs (n.beat - notes[(size_t) lowestBefore].beat) < 0.01 && n.pitch < notes[(size_t) lowestBefore].pitch)))
                            lowestBefore = (int) (&n - notes.data());
                    }
                    if (taken || chance (at, 5, variation) >= (isOnBeat (at) ? p * 0.5f : p))
                        continue;
                    const auto& chordThere = spanAt (spans, at + origin).chord;
                    const auto around = lowestBefore >= 0 ? (double) notes[(size_t) lowestBefore].pitch : (double) low + 5.0;
                    added.push_back ({ at, 0.22, nearestPitch (bassOf (chordThere), around, low, low + 11), of (vel.muteTight) });
                }
            }
            for (const auto& a : added)
                for (auto& n : notes)   // a chug cuts a palm-muted note still sounding
                    if (n.beat < a.beat && n.beat + n.length > a.beat && muted (n))
                        n.length = juce::jmax (0.05, a.beat - n.beat);
            notes.insert (notes.end(), added.begin(), added.end());
            std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        }

        // FILLS: the last beat of each phrase.
        if (riff.fills)
        {
            const auto phrase = riff.bars == 1 ? 4 : riff.bars;
            const auto kind = fillFor (style, variation);
            for (auto bar = phrase - 1; (bar + 1) * beatsPerBar <= total + 1.0e-9; bar += phrase)
            {
                const auto end = (bar + 1) * beatsPerBar;
                const auto from = end - (kind == FillKind::stop ? juce::jmin (2.0, beatsPerBar) : juce::jmin (1.0, beatsPerBar));
                std::vector<LineNote> kept;
                for (auto n : notes)
                {
                    if (n.beat >= from - 1.0e-9 && n.beat < end - 1.0e-9)
                        continue;
                    if (n.beat < from && n.beat + n.length > from)
                        n.length = juce::jmax (0.05, from - n.beat);
                    kept.push_back (n);
                }
                notes = std::move (kept);
                const auto& now = spanAt (spans, from + origin).chord;
                const auto& next = end < total - 1.0e-9 ? spanAt (spans, end + origin).chord : spans.front().chord;
                const auto rootNow = nearestPitch (bassOf (now), low + 5.0, low, low + 11);
                const auto nextRoot = nearestPitch (bassOf (next), low + 5.0, low, low + 11);
                switch (kind)
                {
                    case FillKind::chromatic:   // down by half steps into the next chord's root
                        for (auto k = 0; k < 4; ++k)
                            add (notes, from + k * 0.25, 0.22, nextRoot + 4 - k, of (vel.muteMid));
                        break;
                    case FillKind::burst:
                        for (auto k = 0; k < 6; ++k)
                            add (notes, from + k / 6.0, 1.0 / 6.0 * 0.9, rootNow, of (vel.muteTight));
                        break;
                    case FillKind::squeal:
                        add (notes, from, 1.0, rootNow + 12, of (vel.pinch));
                        break;
                    case FillKind::stop:
                        add (notes, end - 0.5, 0.5, rootNow + 12, of (vel.pinch));
                        break;
                    case FillKind::slide:   // two power chords climbing into the next one
                        for (auto k = 0; k < 2; ++k)
                        {
                            const auto r = nextRoot - 2 + k;
                            for (const auto p : { r, r + 7 })
                                add (notes, from + k * 0.5, 0.5, p, of (vel.sustain));
                        }
                        break;
                }
            }
        }
        foldAndSort (idea, low, high);
    }

    //==========================================================================
    LineIdea guitarSolo (const std::vector<LineIdea>& leads, const std::vector<ChordSpan>& spans, double beatsPerBar, const GuitarTuning& tuning)
    {
        LineIdea solo { LineIdea::Part::lead, "Solo", {}, {} };
        if (leads.empty() || spans.empty() || beatsPerBar <= 0.0)
            return solo;
        const auto origin = spans.front().start;
        const auto total = spans.back().end - origin;
        // The leads from the calmest to the busiest.
        std::vector<const LineIdea*> order;
        for (const auto& l : leads)
            if (! l.notes.empty())
                order.push_back (&l);
        if (order.empty())
            return solo;
        const auto onsetsPerBeat = [total] (const LineIdea* l)
        {
            std::vector<long> starts;
            for (const auto& n : l->notes)
                if (! n.keyswitch)
                    starts.push_back (std::lround (n.beat * 96.0));
            std::sort (starts.begin(), starts.end());
            starts.erase (std::unique (starts.begin(), starts.end()), starts.end());
            return (double) starts.size() / juce::jmax (1.0, total);
        };
        std::stable_sort (order.begin(), order.end(), [&] (const LineIdea* a, const LineIdea* b) { return onsetsPerBeat (a) < onsetsPerBeat (b); });

        const auto bars = juce::jmax (1, (int) std::ceil (total / beatsPerBar - 1.0e-6));
        const auto phrases = (bars + 1) / 2;
        const auto low = juce::jmax (tuning.strings.size() > 1 ? tuning.strings[1] + 5 : tuning.lowest(), 45);
        juce::StringArray used;
        for (int p = 0; p < phrases; ++p)
        {
            const auto along = phrases == 1 ? 1.0 : (double) p / (double) (phrases - 1);
            const auto* lead = order[(size_t) juce::roundToInt (along * (double) (order.size() - 1))];
            if (! used.contains (lead->style))
                used.add (lead->style);
            const auto from = p * 2 * beatsPerBar, to = juce::jmin (total, (p + 1) * 2 * beatsPerBar);
            std::vector<LineNote> phrase;
            for (auto n : lead->notes)
                if (! n.keyswitch && n.beat >= from - 1.0e-9 && n.beat < to - 1.0e-9)
                {
                    n.length = juce::jmin (n.length, to - n.beat);
                    phrase.push_back (n);
                }
            // Long single notes (alone at their start, not a pinch or rake): the longest bent into, the others with vibrato.
            const auto single = [&phrase] (size_t i)
            {
                for (size_t k = 0; k < phrase.size(); ++k)
                    if (k != i && std::abs (phrase[k].beat - phrase[i].beat) < HumanizeLimits::together)
                        return false;
                return true;
            };
            const auto plain = [] (const LineNote& n) { const auto m = GuitarVelocity::midiOf (n.velocity); return m > GuitarVelocity::mutedMost && m <= GuitarVelocity::sustainMost; };
            int longest = -1;
            for (size_t i = 0; i < phrase.size(); ++i)
                if (phrase[i].length >= 0.75 - 1.0e-9 && single (i) && plain (phrase[i]))
                {
                    phrase[i].effect = LineNote::Effect::vibrato;
                    if (longest < 0 || phrase[i].length > phrase[(size_t) longest].length)
                        longest = (int) i;
                }
            if (longest >= 0 && phrase[(size_t) longest].pitch - 2 >= low)
            {
                phrase[(size_t) longest].pitch -= 2;
                phrase[(size_t) longest].effect = LineNote::Effect::bend;
            }
            solo.notes.insert (solo.notes.end(), phrase.begin(), phrase.end());
        }
        std::stable_sort (solo.notes.begin(), solo.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        // The last note (or chord) held to the end, with vibrato when it's a single note.
        if (! solo.notes.empty())
        {
            const auto lastStart = solo.notes.back().beat;
            auto count = 0;
            for (auto& n : solo.notes)
                if (std::abs (n.beat - lastStart) < HumanizeLimits::together)
                {
                    n.length = juce::jmax (n.length, (total - n.beat) * 0.96);
                    ++count;
                }
            if (count == 1 && solo.notes.back().effect == LineNote::Effect::none
                && GuitarVelocity::midiOf (solo.notes.back().velocity) <= GuitarVelocity::sustainMost)
                solo.notes.back().effect = LineNote::Effect::vibrato;
        }
        solo.description = "A whole solo building up: " + used.joinIntoString (", then ").toLowerCase()
                         + "; long notes bent into and held with vibrato, the last one left ringing";
        return solo;
    }

    std::vector<LineIdea> styleLeadIdeas (GuitarStyle style, const std::vector<ChordSpan>& spans, double beatsPerBar, int variation,
                                          const GuitarTuning& tuning, const GuitarVelocities& vel)
    {
        std::vector<LineIdea> ideas;
        if (spans.empty() || beatsPerBar <= 0.0)
            return ideas;
        const auto origin = spans.front().start;
        // A lead's range (as 0.19.0's): from the second string's fifth fret to the top string's 22nd.
        const auto low = juce::jmax (tuning.strings[1] + 5, 45), high = tuning.strings.back() + 22;
        const auto centre = (low + high) / 2 - 2;
        struct Bar { size_t span; double start, length; int index; };
        std::vector<Bar> barsList;
        for (size_t s = 0; s < spans.size(); ++s)
            for (auto t = spans[s].start; t < spans[s].end - 1.0e-9; t += beatsPerBar)
                barsList.push_back ({ s, t - origin, juce::jmin (beatsPerBar, spans[s].end - t), (int) std::floor ((t - origin) / beatsPerBar + 1.0e-9) });
        const auto minorTonic = [] (const Key& k) { return k.minor ? k.pitchClass() : pcOf (k.pitchClass() + 9); };
        const auto scaleFor = [&] (LeadScale kind, const ChordSpan& s) -> std::vector<int>
        {
            const auto& k = s.key;
            switch (kind)
            {
                case LeadScale::pentatonic: return scaleOn (minorTonic (k), { 0, 3, 5, 7, 10 });
                case LeadScale::blues: return scaleOn (minorTonic (k), { 0, 3, 5, 6, 7, 10 });
                case LeadScale::natural: { const auto sc = k.scale(); return { sc.begin(), sc.end() }; }
                case LeadScale::neo:
                {
                    auto scale = k.minor ? scaleOn (k.pitchClass(), { 0, 2, 3, 5, 7, 8, 11 }) : scaleOn (k.pitchClass(), { 0, 2, 4, 5, 7, 9, 11 });
                    // (as 0.19.0's neoclassical run: not the raised seventh over a chord holding the natural one)
                    if (k.minor && has (tonesOf (s.chord), pcOf (k.pitchClass() + 10)))
                        for (auto& n : scale)
                            if (n == pcOf (k.pitchClass() + 11))
                                n = pcOf (k.pitchClass() + 10);
                    for (const auto t : tonesOf (s.chord))
                        if (! has (scale, t))
                            scale.push_back (t);
                    return scale;
                }
                case LeadScale::chromatic: return { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
                case LeadScale::chordTones: return tonesOf (s.chord);
                case LeadScale::available: return availableNotes (s.chord, k);
                case LeadScale::diminished:   // the chord's root and the diminished seventh a half step under it
                    return scaleOn (s.chord.root, { 0, 11, 2, 5, 8 });
                case LeadScale::dark: return scaleOn (minorTonic (k), { 0, 1, 3, 5, 6, 8, 10 });
            }
            return tonesOf (s.chord);
        };
        // (MORE IDEAS: the odd variations turn every line the other way: up for down.)
        const auto flip = variation % 2 == 1 ? -1 : 1;
        // (0.61.0: the composer's takes, every two variations another: where in its moves a line
        // starts, the two rhythms swapped, and a start a few steps higher or lower.)
        const auto take = variation / 2;
        for (const auto& def : leadsOf (style))
        {
            LineIdea idea { LineIdea::Part::lead, def.name, def.description, {} };
            auto prev = def.startHigh ? high - 5 : centre;
            if (take > 0)
                prev = juce::jlimit (low, high, prev + ((take / 3) % 5 - 2) * 2);
            const auto swapRhythms = take > 0 && (take / 2) % 2 == 1 && def.rhythmB != nullptr;
            auto bounce = 1;        // -1 once a line has turned round at the range's edge
            auto turned = false;
            for (const auto& b : barsList)
            {
                const auto& s = spans[b.span];
                const auto scale = scaleFor (def.scale, s);
                const auto keyScale = s.key.scale();
                const std::vector<int> keyNotes (keyScale.begin(), keyScale.end());
                const char* rhythm = def.rhythmB != nullptr && (b.index % 2 == 1) != swapRhythms ? def.rhythmB : def.rhythm;
                const auto steps = (int) std::strlen (rhythm);
                if (steps == 0)
                    continue;
                const auto step = beatsPerBar / steps;
                auto cur = prev, pedal = prev;
                auto first = true;
                size_t moveIndex = def.moves.empty() ? 0 : (size_t) take % def.moves.size();
                for (auto i = 0; i < steps; ++i)
                {
                    const auto ch = rhythm[i];
                    if (! isSounding (ch))
                        continue;
                    const auto at = i * step;
                    if (at >= b.length - 1.0e-9)
                        break;
                    auto held = 1;
                    while (i + held < steps && rhythm[i + held] == '=')
                        ++held;
                    const auto length = juce::jmin (held * step, b.length - at);
                    int pitch;
                    if (first)
                    {
                        pitch = nearestOf (tonesOf (s.chord), prev, low, high);   // (a chord tone on the bar's first note)
                        pedal = cur = pitch;
                    }
                    else if (ch == 'r')
                        pitch = cur;
                    else if (ch == 'P')
                        pitch = pedal;
                    else
                    {
                        const auto move = def.moves.empty() ? 0 : flip * bounce * def.moves[moveIndex++ % def.moves.size()];
                        pitch = cur;
                        for (auto k = 0; k < std::abs (move); ++k)
                        {
                            auto d = (move > 0 ? 1 : -1) * (turned ? -1 : 1);
                            auto next = stepFrom (pitch, d, scale, low, high);
                            if ((d > 0 && next <= pitch) || (d < 0 && next >= pitch))
                            {
                                // The range's edge: the line turns round, and every move after it with it.
                                turned = ! turned;
                                d = -d;
                                next = stepFrom (pitch, d, scale, low, high);
                            }
                            pitch = next;
                        }
                        if (turned)
                        {
                            bounce = -bounce;
                            turned = false;
                        }
                        cur = pitch;
                    }
                    // (legato: each group's first note picked, the rest hammered or pulled off; a group
                    // starts where the moves go round)
                    const auto groupStart = ch == 'n' && ! def.moves.empty() && moveIndex % def.moves.size() == 0;
                    const auto velocity = ch == 'p' ? of (vel.pinch)
                                        : first ? of (vel.sustainHard)
                                        : def.legato && ! groupStart ? of (vel.open (vel.sustainSoft + 4))
                                        : std::abs (std::fmod (at, 1.0)) < 1.0e-6 ? of (vel.open (vel.sustain + 4)) : of (vel.open (vel.sustain - 6));
                    add (idea.notes, b.start + at, length, pitch, velocity);
                    if (def.harmony && ch != 'p')
                    {
                        auto third = stepFrom (stepFrom (pitch, 1, keyNotes, low, high + 12), 1, keyNotes, low, high + 12);
                        if (third > high)
                            third = stepFrom (stepFrom (pitch, -1, keyNotes, low, high), -1, keyNotes, low, high);
                        if (third != pitch)
                            add (idea.notes, b.start + at, length, third, of (vel.open (vel.sustain - 10)));
                    }
                    if (def.octave && ch != 'p')
                    {
                        const auto o = pitch + 12 <= high ? pitch + 12 : pitch - 12;
                        if (o >= low)
                            add (idea.notes, b.start + at, length, o, of (vel.open (vel.sustain - 8)));
                    }
                    first = false;
                }
                prev = cur;
            }
            // The last notes on the last chord, held to its end.
            if (! idea.notes.empty())
            {
                std::stable_sort (idea.notes.begin(), idea.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
                const auto lastAt = idea.notes.back().beat;
                const auto tones = tonesOf (spans.back().chord);
                for (auto& n : idea.notes)
                    if (std::abs (n.beat - lastAt) < 1.0e-6)
                    {
                        n.pitch = nearestOf (tones, n.pitch, low, high);
                        n.length = juce::jmax (n.length, spans.back().end - origin - n.beat);
                    }
            }
            ideas.push_back (std::move (idea));
        }
        return ideas;
    }
}
