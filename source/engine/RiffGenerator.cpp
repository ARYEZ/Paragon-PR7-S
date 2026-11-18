#include "engine/RiffGenerator.h"

#include <array>
#include <cmath>
#include <map>
#include <numeric>

namespace pr7s
{
    //==========================================================================
    const std::vector<RiffFlavour>& riffFlavours()
    {
        static const std::vector<RiffFlavour> all { RiffFlavour::any, RiffFlavour::chugs, RiffFlavour::gallop, RiffFlavour::tritone, RiffFlavour::creep,
                                                    RiffFlavour::stabs, RiffFlavour::octaves, RiffFlavour::stopStart, RiffFlavour::openChords,
                                                    RiffFlavour::drivingEighths, RiffFlavour::droning,
                                                    RiffFlavour::pedalStabs, RiffFlavour::chugThrees, RiffFlavour::burstSilence,
                                                    RiffFlavour::secondHalfStabs, RiffFlavour::firstHalfBurst, RiffFlavour::hitPush,
                                                    RiffFlavour::syncopatedDrive, RiffFlavour::mutedDrive, RiffFlavour::gallopSqueal };
        return all;
    }

    juce::String riffFlavourName (RiffFlavour f)
    {
        switch (f)
        {
            case RiffFlavour::any: return "ANY";
            case RiffFlavour::chugs: return "CHUGS";
            case RiffFlavour::gallop: return "GALLOP";
            case RiffFlavour::tritone: return "TRITONE CHUG";
            case RiffFlavour::creep: return "CREEP";
            case RiffFlavour::stabs: return "STABS";
            case RiffFlavour::octaves: return "OCTAVES";
            case RiffFlavour::stopStart: return "STOP-START";
            case RiffFlavour::openChords: return "OPEN CHORDS";
            case RiffFlavour::drivingEighths: return "DRIVING EIGHTHS";
            case RiffFlavour::droning: return "DRONING";
            case RiffFlavour::pedalStabs: return "PEDAL & STABS";
            case RiffFlavour::chugThrees: return "CHUGS IN THREES";
            case RiffFlavour::burstSilence: return "BURST & SILENCE";
            case RiffFlavour::secondHalfStabs: return "SECOND-HALF STABS";
            case RiffFlavour::firstHalfBurst: return "FIRST-HALF BURST";
            case RiffFlavour::hitPush: return "HIT & PUSH";
            case RiffFlavour::syncopatedDrive: return "SYNCOPATED DRIVE";
            case RiffFlavour::mutedDrive: return "MUTED DRIVE";
            case RiffFlavour::gallopSqueal: return "GALLOP & SQUEAL";
        }
        return "ANY";
    }

    juce::String riffFlavourDescription (RiffFlavour f)
    {
        switch (f)
        {
            case RiffFlavour::any: return "Every kind of riff: each GENERATE leans a different way";
            case RiffFlavour::chugs: return "Palm-muted chugs and chug runs on the low string";
            case RiffFlavour::gallop: return "Gallops (an eighth and two sixteenths) driving the riff";
            case RiffFlavour::tritone: return "Chugs on the low string answered by the high tritone grind, or chugging the low tritone itself";
            case RiffFlavour::creep: return "Heavy low notes creeping a half step or a minor third up and back";
            case RiffFlavour::stabs: return "Chugs answered by high, dissonant stabs (tritones with vibrato, bent whole steps)";
            case RiffFlavour::octaves: return "Ringing octave shapes between the chugs";
            case RiffFlavour::stopStart: return "Bursts of hits and dead silence between them";
            case RiffFlavour::openChords: return "Power chords and chords left ringing";
            case RiffFlavour::drivingEighths: return "The big chorus: power chords ringing on every eighth, a palm-muted stretch, a walk into the next chord";
            case RiffFlavour::droning: return "The ringing chorus: chords struck and left to ring across the bar, leaving the space to the band";
            case RiffFlavour::pedalStabs: return "The low string chugged in sixteenths, a power chord stabbed every third one, climbing every bar or two";
            case RiffFlavour::chugThrees: return "A rest and two chugs over and over, a chromatic walk up at the bar's end, power chords climbing every fourth bar";
            case RiffFlavour::burstSilence: return "A hit, a burst of chugs, a pair, then silence; a dissonant stab every other bar";
            case RiffFlavour::secondHalfStabs: return "A chord on 1, silence, then short stabs through the second half of the bar";
            case RiffFlavour::firstHalfBurst: return "A burst of chugs stepping down in the first half, the second half silent, a pickup every fourth bar";
            case RiffFlavour::hitPush: return "A chord on 1, the bar left open, the next chord pushed in early from a step under";
            case RiffFlavour::syncopatedDrive: return "Driving eighths opened by stabs, a bar rung across the beats, the changes pushed in an eighth early";
            case RiffFlavour::mutedDrive: return "Palm-muted eighths: single root notes, then muted power chords, a bar rocking between two chords, a walk lifting out";
            case RiffFlavour::gallopSqueal: return "The root galloped, answered by a pinch-harmonic squeal and a stab on the sixth, turning round on the seventh";
        }
        return {};
    }

    RiffFlavour riffFlavourNamed (const juce::String& name)
    {
        for (const auto f : riffFlavours())
            if (riffFlavourName (f).equalsIgnoreCase (name.trim()))
                return f;
        if (name.trim().equalsIgnoreCase ("TRITONE"))
            return RiffFlavour::tritone;   // (its first name)
        return RiffFlavour::any;
    }

    int riffStepsPerBar (double beatsPerBar)
    {
        return juce::jlimit (4, 64, (int) std::lround (beatsPerBar * 4.0));
    }

    juce::String riffFingerprint (const std::vector<std::string>& bars)
    {
        juce::String out;
        for (const auto& b : bars)
            out << juce::String (b) << "|";
        return out;
    }

    namespace
    {
        // The building blocks. Each writes its characters (lower case muted, upper case open; z the low
        // string's note, the letters intervals over it; see Riffs.cpp).
        enum class Cell { chug, eighthChug, chugRun, gallop, rest, powerChord, dyad, interval, tritone, creep, stab, octave, highChord, pinch, stop, count };
        constexpr int cellCount = (int) Cell::count;
        using Weights = std::array<float, cellCount>;

        bool sustains (Cell c)
        {
            return c == Cell::powerChord || c == Cell::tritone || c == Cell::creep || c == Cell::stab || c == Cell::octave || c == Cell::highChord;
        }

        bool sounds (Cell c) { return c != Cell::rest && c != Cell::stop; }

        Weights styleWeights (GuitarStyle style)
        {
            //                      chug eight run  gallop rest power dyad intv tri  creep stab oct  high pinch stop
            switch (style)
            {
                case GuitarStyle::thrash:    return { 3.0f, 1.5f, 2.5f, 3.0f, 1.2f, 1.0f, 1.0f, 2.5f, 1.0f, 0.4f, 0.0f, 0.3f, 0.1f, 0.2f, 0.4f };
                case GuitarStyle::numetal:   return { 2.5f, 2.0f, 0.8f, 0.4f, 2.6f, 1.0f, 0.6f, 2.0f, 1.2f, 1.2f, 0.4f, 0.8f, 0.4f, 0.6f, 1.4f };
                case GuitarStyle::metalcore: return { 3.0f, 1.6f, 1.6f, 0.6f, 2.4f, 1.4f, 0.6f, 1.2f, 0.8f, 0.4f, 0.2f, 1.0f, 0.3f, 0.7f, 1.2f };
                case GuitarStyle::shred:     return { 2.2f, 1.6f, 1.6f, 1.8f, 0.8f, 1.8f, 2.2f, 1.6f, 0.2f, 0.2f, 0.0f, 0.6f, 0.3f, 0.1f, 0.2f };
                case GuitarStyle::groove:    return { 2.8f, 1.6f, 1.0f, 0.8f, 1.8f, 0.8f, 0.6f, 1.2f, 1.0f, 0.6f, 2.2f, 0.4f, 0.2f, 0.4f, 0.8f };
                case GuitarStyle::rock:      return { 1.8f, 2.4f, 1.0f, 1.2f, 1.2f, 2.6f, 1.6f, 1.4f, 0.1f, 0.3f, 0.0f, 0.8f, 0.6f, 0.1f, 0.3f };
            }
            return {};
        }

        void lean (Weights& w, RiffFlavour f)
        {
            const auto boost = [&w] (Cell c, float by) { w[(size_t) c] *= by; };
            switch (f)
            {
                case RiffFlavour::any: break;
                case RiffFlavour::chugs: boost (Cell::chug, 2.5f); boost (Cell::eighthChug, 2.0f); boost (Cell::chugRun, 3.0f); boost (Cell::interval, 0.6f); boost (Cell::powerChord, 0.6f); break;
                case RiffFlavour::gallop: boost (Cell::gallop, 6.0f); boost (Cell::chugRun, 0.6f); break;
                case RiffFlavour::tritone: boost (Cell::tritone, 5.0f); boost (Cell::interval, 1.5f); boost (Cell::stab, 1.5f); break;
                case RiffFlavour::creep: boost (Cell::creep, 6.0f); boost (Cell::interval, 1.5f); boost (Cell::chugRun, 0.5f); break;
                case RiffFlavour::stabs: w[(size_t) Cell::stab] = juce::jmax (w[(size_t) Cell::stab], 1.0f) * 4.0f; break;
                case RiffFlavour::octaves: boost (Cell::octave, 6.0f); break;
                case RiffFlavour::stopStart: boost (Cell::stop, 5.0f); boost (Cell::rest, 1.6f); boost (Cell::chugRun, 1.6f); boost (Cell::gallop, 0.5f); break;
                case RiffFlavour::openChords: boost (Cell::powerChord, 4.0f); boost (Cell::highChord, 3.0f); boost (Cell::chug, 0.6f); boost (Cell::chugRun, 0.4f); break;
                case RiffFlavour::drivingEighths: case RiffFlavour::syncopatedDrive: case RiffFlavour::mutedDrive: break;   // (made by DrivingEighths below, not from cells)
                case RiffFlavour::gallopSqueal: break;   // (StaccatoChug)
                case RiffFlavour::droning: break;          // (made by Droning below)
                case RiffFlavour::pedalStabs: case RiffFlavour::chugThrees: case RiffFlavour::burstSilence:
                case RiffFlavour::secondHalfStabs: case RiffFlavour::firstHalfBurst: case RiffFlavour::hitPush: break;   // (StaccatoChug)
            }
        }

        void shapeForSection (Weights& w, DrumSection s)
        {
            const auto boost = [&w] (Cell c, float by) { w[(size_t) c] *= by; };
            switch (s)
            {
                case DrumSection::verse: boost (Cell::chug, 1.3f); boost (Cell::chugRun, 1.2f); boost (Cell::powerChord, 0.7f); boost (Cell::highChord, 0.6f); break;
                case DrumSection::chorus: boost (Cell::powerChord, 2.4f); boost (Cell::octave, 1.8f); boost (Cell::highChord, 1.6f); boost (Cell::chug, 0.6f);
                                          boost (Cell::chugRun, 0.6f); boost (Cell::rest, 0.6f); boost (Cell::stop, 0.5f); break;
                case DrumSection::breakdown: boost (Cell::rest, 2.2f); boost (Cell::stop, 2.0f); boost (Cell::chug, 1.4f); boost (Cell::chugRun, 0.6f);
                                             boost (Cell::gallop, 0.3f); boost (Cell::pinch, 1.6f); boost (Cell::interval, 0.6f); break;
                case DrumSection::bridge: boost (Cell::creep, 2.0f); boost (Cell::octave, 1.5f); boost (Cell::highChord, 1.8f); boost (Cell::chug, 0.7f); break;
                case DrumSection::intro: boost (Cell::rest, 1.5f); boost (Cell::powerChord, 1.5f); boost (Cell::chugRun, 0.6f); break;
                case DrumSection::ending: boost (Cell::powerChord, 2.0f); boost (Cell::rest, 1.2f); boost (Cell::gallop, 0.6f); break;
                case DrumSection::solo: boost (Cell::interval, 0.5f); boost (Cell::stab, 0.4f); boost (Cell::pinch, 0.3f); boost (Cell::powerChord, 1.3f); break;
            }
        }

        // How the style likes a long note to start off the beat (0 never .. 1 as often as on it).
        float syncopationOf (GuitarStyle style)
        {
            switch (style)
            {
                case GuitarStyle::numetal: return 0.9f;
                case GuitarStyle::groove: return 0.7f;
                case GuitarStyle::metalcore: return 0.6f;
                case GuitarStyle::thrash: return 0.45f;
                case GuitarStyle::shred: return 0.35f;
                case GuitarStyle::rock: return 0.45f;
            }
            return 0.5f;
        }

        // The single-note intervals over the low note a style picks from, with weights.
        std::vector<std::pair<char, float>> intervalsOf (GuitarStyle style, RiffFlavour f)
        {
            std::vector<std::pair<char, float>> out;
            switch (style)
            {
                case GuitarStyle::thrash: out = { { 'c', 3 }, { 'd', 3 }, { 'e', 2.5f }, { 'f', 1 }, { 'g', 1 } }; break;
                case GuitarStyle::numetal: out = { { 'c', 3 }, { 'e', 3 }, { 'd', 1.5f }, { 'k', 0.5f } }; break;
                case GuitarStyle::metalcore: out = { { 'c', 2 }, { 'd', 2 }, { 'f', 1 }, { 'g', 2 }, { 'e', 1 } }; break;
                case GuitarStyle::groove: out = { { 'e', 3 }, { 'c', 2 }, { 'd', 2 }, { 'k', 1 } }; break;
                case GuitarStyle::shred: out = { { 'g', 3 }, { 'f', 2 }, { 'd', 2 }, { 'k', 1 }, { 't', 1 } }; break;
                case GuitarStyle::rock: out = { { 'g', 3 }, { 'k', 2 }, { 'd', 2 }, { 'f', 1.5f }, { 't', 1 } }; break;
            }
            for (auto& [c, w] : out)
            {
                if (f == RiffFlavour::tritone && c == 'e')
                    w *= 5.0f;
                if (f == RiffFlavour::creep && (c == 'c' || c == 'd'))
                    w *= 3.0f;
            }
            if (f == RiffFlavour::tritone && std::none_of (out.begin(), out.end(), [] (const auto& p) { return p.first == 'e'; }))
                out.push_back ({ 'e', 8.0f });
            return out;
        }

        // How strong a sixteenth is: the bar's first 4, the half 3, beats 2, eighths 1, sixteenths 0.
        int strengthOf (int step, int stepsPerBar)
        {
            if (step == 0)
                return 4;
            if (stepsPerBar % 2 == 0 && step == stepsPerBar / 2 && stepsPerBar % 8 == 0)
                return 3;
            if (step % 4 == 0)
                return 2;
            if (step % 2 == 0)
                return 1;
            return 0;
        }

        struct Maker
        {
            const RiffRecipe& recipe;
            juce::Random& random;
            Weights weights;
            bool pedal;   // (the low string's note: 'z'; else the chord's root: 'x')

            char low (bool open) const { return pedal ? (open ? 'Z' : 'z') : (open ? 'X' : 'x'); }

            template <typename T>
            const T& choose (const std::vector<std::pair<T, float>>& options)
            {
                auto total = 0.0f;
                for (const auto& o : options)
                    total += o.second;
                auto r = random.nextFloat() * total;
                for (const auto& o : options)
                {
                    if (r < o.second)
                        return o.first;
                    r -= o.second;
                }
                return options.back().first;
            }

            int between (int least, int most) { return least + random.nextInt (juce::jmax (1, most - least + 1)); }

            char interval (bool open)
            {
                const auto options = intervalsOf (recipe.style, recipe.flavour);
                const auto c = choose (options);
                return open ? (char) juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) c) : c;
            }

            // A held note: its character then '=' to `length`.
            static std::string held (char c, int length)
            {
                std::string s (1, c);
                s.append ((size_t) juce::jmax (0, length - 1), '=');
                return s;
            }

            // The cell's characters, at most `room` long.
            std::string write (Cell cell, int room, int step, int stepsPerBar)
            {
                std::string s;
                const auto onBeat = step % 4 == 0;
                switch (cell)
                {
                    case Cell::chug: s = std::string (1, low (false)); break;
                    case Cell::eighthChug: s = std::string (1, low (false)) + "-"; break;
                    case Cell::chugRun: s = std::string ((size_t) between (2, 4), low (false)); break;
                    case Cell::gallop: s = random.nextFloat() < 0.7f ? std::string (1, low (false)) + "-" + low (false) + low (false)
                                                                     : std::string (2, low (false)) + low (false) + "-";
                                       if (recipe.flavour != RiffFlavour::gallop && random.nextFloat() < 0.3f)
                                           s[0] = interval (false);
                                       break;
                    case Cell::rest: s = std::string ((size_t) between (1, onBeat ? 2 : 3), '-'); break;
                    case Cell::powerChord: s = held ('#', onBeat ? between (2, 8) : between (2, 4)); break;
                    case Cell::dyad: s = random.nextFloat() < 0.5f ? "*-" : "**"; break;
                    case Cell::interval:
                    {
                        const auto open = random.nextFloat() < (recipe.section == DrumSection::chorus ? 0.55f : 0.25f);
                        s = open ? held (interval (true), between (2, 4)) : (random.nextFloat() < 0.5f ? std::string (1, interval (false))
                                                                                                         : std::string (1, interval (false)) + "-");
                        break;
                    }
                    case Cell::tritone:
                        s = random.nextFloat() < 0.6f ? held ('B', between (2, 4)) : std::string ("b") + (random.nextFloat() < 0.5f ? "-" : "b");
                        break;
                    case Cell::creep:
                    {
                        const auto len = between (2, 4);
                        const char moves[] = { 'C', 'D', 'C', 'E' };
                        s = held (low (true), len) + held (moves[random.nextInt (4)], len);
                        break;
                    }
                    case Cell::stab:
                    {
                        const auto pick = random.nextFloat();
                        s = held (pick < 0.55f ? 'v' : pick < 0.85f ? 'w' : 'u', between (2, 6));
                        break;
                    }
                    case Cell::octave: s = held ('o', between (2, 4)); break;
                    case Cell::highChord: s = held ('h', between (4, 8)); break;
                    case Cell::pinch: s = held ('p', between (1, 4)); break;
                    case Cell::stop: s = std::string ((size_t) between (2, 6), '-'); break;
                    case Cell::count: break;
                }
                juce::ignoreUnused (stepsPerBar);
                if ((int) s.size() > room)
                    s.resize ((size_t) room);
                return s;
            }

            // Fill [from, to) of a bar's sixteenths; the bar's first a hit.
            void fill (std::string& bar, int from, int to, int stepsPerBar, bool turnaround)
            {
                auto pinches = 0;
                auto silent = 0;   // (sixteenths since the last hit)
                for (auto step = from; step < to;)
                {
                    const auto room = to - step;
                    const auto strength = strengthOf (step, stepsPerBar);
                    std::vector<std::pair<Cell, float>> options;
                    for (int c = 0; c < cellCount; ++c)
                    {
                        const auto cell = (Cell) c;
                        auto w = weights[(size_t) c];
                        if (w <= 0.0f)
                            continue;
                        if (step == 0 && ! sounds (cell))
                            continue;   // (the bar's first is a hit)
                        if (sustains (cell))
                            w *= strength >= 2 ? 1.6f : strength == 1 ? 0.6f + syncopationOf (recipe.style) * 0.6f : syncopationOf (recipe.style) * 0.7f;
                        if (! sounds (cell))
                        {
                            w *= strength >= 3 ? 0.2f : strength == 2 ? 0.6f : 1.2f;
                            if (silent >= 4)
                                w *= 0.25f;                     // (not a whole beat of nothing too often)
                        }
                        if (cell == Cell::pinch && (pinches > 0 || step < stepsPerBar / 2))
                            continue;                           // (one squeal a bar, in its second half)
                        if (cell == Cell::highChord && room < 4)
                            continue;
                        if (cell == Cell::creep && room < 4)
                            continue;
                        if (turnaround && step >= stepsPerBar - 4 && (cell == Cell::powerChord || cell == Cell::pinch || cell == Cell::chugRun))
                            w *= 3.0f;
                        options.push_back ({ cell, w });
                    }
                    if (options.empty())
                        options.push_back ({ Cell::chug, 1.0f });
                    const auto cell = choose (options);
                    const auto written = write (cell, room, step, stepsPerBar);
                    if (written.empty())
                        break;
                    for (size_t i = 0; i < written.size(); ++i)
                        bar[(size_t) step + i] = written[i];
                    pinches += cell == Cell::pinch ? 1 : 0;
                    silent = sounds (cell) ? 0 : silent + (int) written.size();
                    step += (int) written.size();
                }
            }
        };

    }

    namespace
    {
        // TRITONE CHUG (0.85.0), from the owner's "ultimate tritone chug riff" (Machine Head, Ten Ton Hammer's intro:
        // the shape learned, nothing of it copied): a CALL bar, the high tritone grind (the flat fifth and the
        // fifth two octaves up) struck and left to ring over the open low string, answered by chugs on the low
        // string; played three times, then an ANSWER bar (power chords walking dark degrees). Every seed draws
        // each part again: the voicing, how long it rings, where it lands, the chug rhythm (one cell kept bar
        // to bar, as a riff repeats), a half-step chug or not, the answer.
        struct TritoneChug
        {
            juce::Random& random;
            const RiffRecipe& recipe;
            int steps;

            template <typename T>
            T pick (std::initializer_list<std::pair<T, float>> options)
            {
                auto total = 0.0f;
                for (const auto& o : options)
                    total += o.second;
                auto r = random.nextFloat() * total;
                for (const auto& o : options)
                {
                    if (r < o.second)
                        return o.first;
                    r -= o.second;
                }
                return options.begin()->first;
            }

            bool breakdown() const { return recipe.section == DrumSection::breakdown; }
            bool chorus() const { return recipe.section == DrumSection::chorus; }

            // The chug rhythm: one or two one-beat cells (x a chug, - a rest).
            std::string cell()
            {
                if (breakdown())
                    return pick<std::string> ({ { "x---", 3 }, { "x--x", 2 }, { "--x-", 1 }, { "x-x-", 2 }, { "xx--", 1.5f }, { "-xx-", 1 } });
                return pick<std::string> ({ { "x-x-", 4 }, { "xxxx", 2 }, { "x-xx", 3 }, { "xx-x", 2 }, { "xxx-", 1.5f }, { "x--x", 1.5f }, { "-xxx", 1 } });
            }

            char voicing()
            {
                if (chorus())
                    return pick<char> ({ { 'q', 5 }, { 'j', 2 }, { 'y', 2 }, { '^', 2 } });
                if (breakdown())
                    return pick<char> ({ { 'q', 4 }, { 'n', 2 }, { 'y', 1.5f }, { '^', 3 } });
                return pick<char> ({ { 'q', 7 }, { 'y', 2 }, { 'j', 1.5f }, { 'n', 1 }, { '^', 2.5f } });
            }

            // Chugs into [from, to) from the cells (a beat each, alternating as the pattern says).
            char chugChar = 'z';   // (the open low string; '*' a muted power chord: Flaw's breakdown)

            void chugs (std::string& bar, int from, int to, const std::string& a, const std::string& b, const std::string& order,
                        int chromaticAt) const
            {
                for (auto i = from; i < to; ++i)
                {
                    const auto beat = (i - from) / 4;
                    const auto& c = order[(size_t) beat % order.size()] == 'a' ? a : b;
                    const auto ch = c[(size_t) ((i - from) % 4)];
                    bar[(size_t) i] = ch == 'x' ? (i == chromaticAt ? 'c' : chugChar) : '-';
                }
            }

            // THE DJENT KIND (the owner's second example, Periphery's Wildfire: the shape learned, nothing copied):
            // the chug is the tritone itself (a: the low string a half step up under the open fifth), palm-muted in
            // groups of 2, 3 or 4 sixteenths that run on across the bar line, pinch harmonics (i) on the same shape,
            // and a bar of open-string chugs with bent single notes (l, m, s) or the chord let ring.
            std::vector<std::string> djent (int bars)
            {
                // The groups (sixteenths each): a cycle that needn't fit the bar.
                std::vector<int> groups;
                const auto count = pick<int> ({ { 3, 3 }, { 4, 3 }, { 5, 2 }, { 2, 1 } });
                for (int k = 0; k < count; ++k)
                    groups.push_back (breakdown() ? pick<int> ({ { 3, 3 }, { 4, 3 }, { 2, 1 }, { 6, 1 } })
                                                  : pick<int> ({ { 3, 4 }, { 2, 3 }, { 4, 2 }, { 5, 1 } }));
                // What fills a group after its first hit.
                std::vector<std::string> fills;
                for (const auto g : groups)
                {
                    std::string f ((size_t) g, '-');
                    f[0] = 'a';
                    for (int i = 1; i < g; ++i)
                        f[(size_t) i] = random.nextFloat() < (breakdown() ? 0.25f : 0.55f) ? 'a' : '-';
                    fills.push_back (f);
                }
                const auto pinchEvery = pick<int> ({ { 0, 2 }, { 2, 2 }, { 3, 2 }, { 4, 1 } });   // a group in so many with a pinch
                const auto ringAt = random.nextFloat() < 0.4f;    // the chord let ring at the phrase's end
                auto cursor = 0;   // where in the cycle (in groups)
                auto groupsDone = 0;
                const auto groupBar = [&] (bool restart)
                {
                    std::string bar ((size_t) steps, '-');
                    if (restart)
                        cursor = 0;
                    auto i = 0;
                    while (i < steps)
                    {
                        const auto& f = fills[(size_t) cursor % fills.size()];
                        for (size_t k = 0; k < f.size() && i + (int) k < steps; ++k)
                            bar[(size_t) i + k] = f[k];
                        if (pinchEvery > 0 && ++groupsDone % pinchEvery == 0 && i > 0)
                            bar[(size_t) i] = 'i';
                        i += (int) f.size();
                        ++cursor;
                    }
                    bar[0] = bar[0] == '-' || bar[0] == 'i' ? 'a' : bar[0];
                    return bar;
                };
                const auto bentBar = [&]
                {
                    // Open-string sixteenths, each beat or two a bent note (the half step, whole step, minor third).
                    std::string bar ((size_t) steps, '-');
                    for (auto i = 0; i < steps; ++i)
                        bar[(size_t) i] = random.nextFloat() < 0.8f ? 'z' : '-';
                    const auto every = pick<int> ({ { 4, 2 }, { 6, 2 }, { 8, 1 } });
                    for (auto i = 0; i < steps; i += every)
                    {
                        const auto at = juce::jmin (steps - 1, i + (i == 0 ? 0 : random.nextInt (2)));
                        bar[(size_t) at] = pick<char> ({ { 'l', 3 }, { 'm', 2 }, { 's', 2 } });
                        if (at + 1 < steps)
                            bar[(size_t) at + 1] = '=';
                    }
                    return bar;
                };
                const auto ringBar = [&]
                {
                    auto bar = groupBar (false);
                    const auto from = juce::jmax (4, steps / 2);
                    bar[(size_t) from] = 'A';
                    for (auto i = from + 1; i < steps; ++i)
                        bar[(size_t) i] = '=';
                    return bar;
                };
                std::vector<std::string> out;
                out.push_back (groupBar (true));
                if (bars == 2)
                    out.push_back (ringAt ? ringBar() : groupBar (false));
                else if (bars >= 4)
                {
                    out.push_back (groupBar (false));
                    out.push_back (random.nextFloat() < 0.5f ? bentBar() : groupBar (false));
                    out.push_back (ringAt ? ringBar() : groupBar (false));
                }
                return out;
            }

            std::vector<std::string> make (int bars)
            {
                // Which kind: the hammer kind, or the djent kind (nu-metal / metalcore lean djent, groove / thrash hammer).
                const auto djentWeight = recipe.style == GuitarStyle::metalcore ? 1.4f : recipe.style == GuitarStyle::numetal ? 1.0f
                                       : recipe.style == GuitarStyle::shred ? 0.8f : 0.6f;
                if (random.nextFloat() * (1.0f + djentWeight) >= 1.0f)
                    return djent (bars);
                const auto stab = voicing();
                // How long the high chord rings (sixteenths) and where it lands.
                auto ring = breakdown() ? pick<int> ({ { 8, 3 }, { 6, 1 }, { 12, 1 } })
                                        : pick<int> ({ { 4, 5 }, { 6, 2 }, { 8, 2 }, { 2, 1 } });
                ring = juce::jlimit (2, juce::jmax (2, steps - 4), ring);
                // 0 the downbeat, 1 after a chug, 2 answering the chugs, 3 the bar's last eighth (ringing on: Flaw's)
                const auto place = pick<int> ({ { 0, 7 }, { 1, 2 }, { 2, 1 }, { 3, stab == '^' ? 4.0f : 1.5f } });
                chugChar = random.nextFloat() < (stab == '^' || place == 3 ? 0.65f : 0.2f) ? '*' : 'z';
                const auto a = cell(), b = random.nextFloat() < 0.45f ? cell() : a;
                const auto order = pick<std::string> ({ { "a", 3 }, { "ab", 2 }, { "aab", 1.5f }, { "aaab", 1 } });
                const auto chromatic = random.nextFloat() < 0.3f;

                const auto call = [&] (char voice)
                {
                    std::string bar ((size_t) steps, '-');
                    auto stabAt = 0;
                    auto ringFor = ring;
                    if (place == 3)
                    {
                        // The chugs fill the bar, the grind on its last eighth ringing out.
                        chugs (bar, 0, steps - 2, a, b, order, chromatic ? steps - 3 : -1);
                        bar[0] = chugChar;
                        bar[(size_t) (steps - 2)] = voice;
                        bar[(size_t) (steps - 1)] = '=';
                        return bar;
                    }
                    if (place == 1)
                        stabAt = 2;                                   // a chug, then the grind on the "and"
                    else if (place == 2)
                        stabAt = juce::jmax (4, ((steps - ring) / 4) * 4);   // the chugs first, the grind last
                    stabAt = juce::jmin (stabAt, steps - 2);
                    const auto end = juce::jmin (steps, stabAt + ringFor);
                    if (place == 2)
                    {
                        chugs (bar, 0, stabAt, a, b, order, chromatic ? stabAt - 1 : -1);
                        bar[0] = chugChar;
                    }
                    else
                    {
                        if (place == 1)
                            bar[0] = chugChar, bar[1] = '-';
                        chugs (bar, end, steps, a, b, order, chromatic ? steps - 1 : -1);
                    }
                    bar[(size_t) stabAt] = voice;
                    for (auto i = stabAt + 1; i < end; ++i)
                        bar[(size_t) i] = '=';
                    return bar;
                };

                const auto answer = [&]
                {
                    std::string bar ((size_t) steps, '-');
                    const auto grind = stab == '^' || place == 3;
                    const auto kind = breakdown() ? pick<int> ({ { 0, 3 }, { 2, 2 }, { 3, 2 }, { 4, grind ? 4.0f : 1.0f } })
                                                  : pick<int> ({ { 0, 6 }, { 1, 2 }, { 2, 2 }, { 3, 1 }, { 4, grind ? 5.0f : 1.0f } });
                    if (kind == 4)
                    {
                        // The grind struck in straight eighths, the whole bar (Flaw's every fourth bar).
                        for (auto i = 0; i < steps; ++i)
                            bar[(size_t) i] = i % 2 == 0 ? stab : '-';
                        return bar;
                    }
                    if (kind == 0)
                    {
                        // Power chords walking dark degrees (the flat second, flat third, fourth, flat fifth, fifth, flat sixth).
                        std::vector<int> degrees { 1, 3, 5, 6, 7, 8 };
                        const auto count = juce::jlimit (2, 4, steps / 4 >= 4 ? pick<int> ({ { 3, 4 }, { 2, 2 }, { 4, 2 } }) : pick<int> ({ { 3, 3 }, { 2, 2 } }));
                        std::vector<int> walk;
                        for (int k = 0; k < count; ++k)
                        {
                            int d;
                            do d = degrees[(size_t) random.nextInt ((int) degrees.size())];
                            while (! walk.empty() && d == walk.back());
                            walk.push_back (d);
                        }
                        // Each a beat (the last rings to the bar's end), or pushed: the first two on an eighth's room.
                        const auto beatSteps = 4;
                        auto at = 0;
                        for (int k = 0; k < count && at < steps; ++k)
                        {
                            const auto last = k == count - 1;
                            const auto len = last ? steps - at : beatSteps;
                            bar[(size_t) at] = (char) ('0' + walk[(size_t) k]);
                            for (auto i = at + 1; i < at + len && i < steps; ++i)
                                bar[(size_t) i] = '=';
                            at += len;
                        }
                        if (random.nextFloat() < 0.4f && steps >= 8)
                        {
                            // The last beat chugged again, back into the call.
                            for (auto i = steps - 4; i < steps; ++i)
                                bar[(size_t) i] = a[(size_t) (i % 4)] == 'x' ? 'z' : '-';
                            bar[(size_t) (steps - 4)] = 'z';
                        }
                    }
                    else if (kind == 1)
                    {
                        // The call with its chugs turned into a half-step creep (open notes, the low note and a half step up).
                        bar = call (stab);
                        auto up = false;
                        for (auto i = 0; i < steps; ++i)
                            if (bar[(size_t) i] == 'z' || bar[(size_t) i] == 'c')
                            {
                                bar[(size_t) i] = up ? 'C' : 'Z';
                                up = ! up;
                            }
                    }
                    else if (kind == 2)
                    {
                        // Stop: the grind (or a low power chord) held, then nothing; the chugs come back hard.
                        const auto hold = juce::jmin (steps - 2, pick<int> ({ { 8, 2 }, { steps / 2, 1 }, { steps - 4, 1 } }));
                        bar[0] = random.nextFloat() < 0.5f ? stab : (char) ('0' + pick<int> ({ { 1, 1 }, { 6, 2 }, { 3, 1 } }));
                        for (auto i = 1; i < hold; ++i)
                            bar[(size_t) i] = '=';
                        for (auto i = steps - 2; i < steps; ++i)
                            bar[(size_t) i] = 'z';
                    }
                    else
                    {
                        // All chugs, the cell doubled up, the last beat a sixteenth run.
                        chugs (bar, 0, steps, a, cell(), "ab", -1);
                        for (auto i = juce::jmax (0, steps - 4); i < steps; ++i)
                            bar[(size_t) i] = 'z';
                        bar[0] = 'z';
                    }
                    return bar;
                };

                std::vector<std::string> out;
                const auto first = call (stab);
                out.push_back (first);
                if (bars == 2)
                    out.push_back (answer());
                else if (bars >= 4)
                {
                    out.push_back (first);
                    // The third call: the same, or the voicing turned (the flat fifth doubled), or its chugs varied.
                    const auto turn = random.nextFloat();
                    out.push_back (turn < 0.3f ? call (stab == 'y' ? 'q' : 'y') : turn < 0.5f ? [&] {
                        auto bar = first;
                        for (auto i = juce::jmax (1, steps - 4); i < steps; ++i)
                            if (bar[(size_t) i] == '-' || bar[(size_t) i] == 'z')
                                bar[(size_t) i] = 'z';
                        return bar;
                    }() : first);
                    out.push_back (answer());
                }
                return out;
            }
        };
    }

    namespace
    {
        // DRIVING EIGHTHS (0.87.0): see RiffGenerator.h. A bar is one of:
        //   O open power chords on every eighth            M the same palm-muted
        //   R power chords moving inside the bar, in groups of eighths (3+3+2, 3+2+3...), each group a power
        //     chord on another degree over the root (the digits: so many semitones up)
        //   P punch: ringing hits with space between them   H half and half: the root, then another degree
        //   S strummed full chords on every eighth ('%': root, fifth, octave, the chord's third over it), the top
        //     voice moving a step on the half bar ('&': the fourth in the third's place), maybe a dead-string scratch
        //     ('!') on the beats (Tears Don't Fall)
        //   W one big chord rung for the whole bar (This Calling: at a fast tempo the drums drive)
        //   A picked: the power chord one note an eighth, left to ring (X the root, G the fifth, + the octave; Come
        //     Clarity), or the muted root as a pedal under high notes (+ the octave, ; the fifth over it, T the
        //     chord's third; Parallels, its chorus in double time)
        // and a bar written in lower case ends in a WALK into the chord at the next bar line: single notes ('>'
        // a whole step and '<' a half step over its root, '(' and ')' under it) or whole power chords on those
        // degrees ('{' '}' over, '[' ']' under). riffFromBars plays them from the next chord (the same chord
        // going on: muted power chords instead).
        struct DrivingEighths
        {
            juce::Random& random;
            const RiffRecipe& recipe;
            int steps;

            template <typename T>
            T pick (std::initializer_list<std::pair<T, float>> options)
            {
                auto total = 0.0f;
                for (const auto& o : options)
                    total += o.second;
                auto r = random.nextFloat() * total;
                for (const auto& o : options)
                {
                    if (r < o.second)
                        return o.first;
                    r -= o.second;
                }
                return options.begin()->first;
            }

            bool openLed() const
            {
                return recipe.section == DrumSection::chorus || recipe.section == DrumSection::solo || recipe.section == DrumSection::ending
                    || recipe.section == DrumSection::bridge;
            }

            bool breakdown() const { return recipe.section == DrumSection::breakdown; }
            int every() const { return breakdown() ? 4 : 2; }   // sixteenths a hit
            int hitsInBar() const { return (steps + every() - 1) / every(); }

            static char degreeChar (int semitones) { return semitones == 0 ? '#' : (char) ('0' + juce::jlimit (1, 9, semitones)); }

            // Hits (in eighths) from `fromHit` on, each `ch`, the rest of each hit held.
            void hits (std::string& bar, int fromHit, int toHit, char ch) const
            {
                for (auto h = fromHit; h < toHit; ++h)
                {
                    const auto at = h * every();
                    if (at >= steps)
                        break;
                    bar[(size_t) at] = ch;
                    for (int j = 1; j < every() && at + j < steps; ++j)
                        bar[(size_t) at + (size_t) j] = '=';
                }
            }

            // The groups of a moving bar (eighths each, adding up to the bar).
            std::vector<int> grouping()
            {
                const auto n = hitsInBar();
                std::vector<std::vector<int>> all { { 3, 3, 2 }, { 3, 2, 3 }, { 2, 3, 3 }, { 4, 4 }, { 3, 3 }, { 2, 2, 2 }, { 4, 2 }, { 3, 2, 2 }, { 3, 4 }, { 2, 3, 2 } };
                std::vector<std::vector<int>> fit;
                for (const auto& g : all)
                    if (std::accumulate (g.begin(), g.end(), 0) == n)
                        fit.push_back (g);
                if (fit.empty())
                    return { n };
                // (3+3+2 the most: the owner's Faint)
                if (n == 8 && random.nextFloat() < 0.45f)
                    return { 3, 3, 2 };
                return fit[(size_t) random.nextInt ((int) fit.size())];
            }

            // A moving bar's degrees, a group each (the first bar of a pair from the root).
            std::vector<int> degrees (size_t groups, bool fromRoot)
            {
                static const int choices[] = { 5, 7, 3, 8, 2 };
                std::vector<int> out;
                for (size_t g = 0; g < groups; ++g)
                {
                    if (g == 0 && fromRoot)
                    {
                        out.push_back (0);
                        continue;
                    }
                    int d;
                    do
                        d = random.nextFloat() < 0.75f ? choices[random.nextInt (3)] : choices[random.nextInt (5)];
                    while (! out.empty() && d == out.back());
                    out.push_back (d);
                }
                return out;
            }

            std::vector<std::string> make (int bars)
            {
                const auto openChar = pick<char> ({ { '#', 6 }, { 'o', 1 } });          // power chords, or octaves
                const auto mutedChar = pick<char> ({ { '*', 4 }, { 'x', 1 } });         // muted power chords, or the root chugged
                // The walk: single notes (This Fire Burns) or the power chord itself (The End of Heartache).
                const auto chordWalk = random.nextFloat() < 0.4f;
                auto walk = pick<std::string> ({ { ">><<", 3 }, { "><", 2 }, { ">>><<", 1.5f }, { "(())", 1 }, { "()", 1 } });
                if (chordWalk)
                    for (auto& c : walk)
                        c = c == '>' ? '{' : c == '<' ? '}' : c == '(' ? '[' : ']';
                const auto mutedBefore = ! chordWalk && random.nextFloat() < 0.5f;   // a muted chord just before a single-note walk
                // A moving bar's groups (kept bar to bar, as a riff repeats) and its two sets of degrees.
                const auto groups = grouping();
                const auto first = degrees (groups.size(), true);
                const auto second = degrees (groups.size(), random.nextFloat() < 0.3f);
                // A punch bar's hits (in eighths: x a hit, = rings on).
                const auto punch = pick<std::string> ({ { "xx=x=x=x", 3 }, { "x=x=xx=x", 2 }, { "xx=x==x=", 2 }, { "x==x==x=", 2 }, { "x=x==x=x", 1 } });
                const auto halfDegree = pick<int> ({ { 5, 3 }, { 7, 2 }, { 3, 2 }, { 8, 1 }, { 2, 1 } });
                const auto scratch = pick<int> ({ { 0, 3 }, { 1, 1 }, { 2, 1 } });    // a strummed bar's scratches: none, on 1, on 1 and 3
                const auto moves = random.nextFloat() < 0.7f;                         // the top voice moves on its half bar
                const auto bigChord = pick<char> ({ { '%', 3 }, { '#', 1 } });       // a ringing bar's chord
                const auto picking = pick<std::string> ({ { "XG+GXG+G", 3 }, { "X+G+X+G+", 2 }, { "XGX+XGX+", 2 },   // a picked bar's eighths
                                                          { "x+x;x+xT", 2 }, { "x;x+x;x+", 1.5f }, { "x+;+x+;T", 1 } });
                // (0.98.0) A syncopated bar's hits (in eighths), drawn after the others so the old draws stay as they were.
                const auto syncopated = recipe.flavour == RiffFlavour::syncopatedDrive;
                const auto mutedDrive = recipe.flavour == RiffFlavour::mutedDrive;
                const auto stab = syncopated ? pick<std::string> ({ { "x=x=----", 3 }, { "x==x==--", 2 }, { "x=x==---", 1 } }) : std::string ("x=x=----");
                const auto syncopation = recipe.approach >= 0 || syncopated
                    ? pick<std::string> ({ { "x==x==x=", 3 }, { "x==x=x==", 2 }, { "x=x==x==", 2 } }) : std::string ("x==x==x=");

                int moving = 0;   // moving bars so far (they alternate their degrees)
                const auto bar = [&] (char kind, bool walks)
                {
                    std::string b ((size_t) steps, '-');
                    const auto n = hitsInBar();
                    switch (kind)
                    {
                        case 'R':
                        {
                            const auto& degs = moving++ % 2 == 0 ? first : second;
                            auto h = 0;
                            for (size_t g = 0; g < groups.size(); ++g)
                            {
                                hits (b, h, h + groups[g], degreeChar (degs[g]));
                                h += groups[g];
                            }
                            break;
                        }
                        case 'P':
                            for (int h = 0; h < n; ++h)
                            {
                                const auto c = punch[(size_t) h % punch.size()];
                                if (c == 'x' || h == 0)
                                    hits (b, h, h + 1, openChar);
                                else
                                    for (int j = 0; j < every() && h * every() + j < steps; ++j)
                                        b[(size_t) (h * every() + j)] = '=';
                            }
                            break;
                        case 'H':
                            hits (b, 0, n / 2, openChar);
                            hits (b, n / 2, n, degreeChar (halfDegree));
                            break;
                        case 'S':
                            hits (b, 0, n / 2, '%');
                            hits (b, n / 2, n, moves ? '&' : '%');
                            if (scratch >= 1 && ! breakdown())
                                hits (b, 0, 1, '!');
                            if (scratch >= 2 && ! breakdown() && n >= 8)
                                hits (b, n / 2, n / 2 + 1, '!');
                            break;
                        case 'A':
                            for (int h = 0; h < n; ++h)
                                hits (b, h, h + 1, picking[(size_t) h % picking.size()]);
                            break;
                        case 'W':
                            hits (b, 0, 1, bigChord);
                            for (int i = 1; i < steps; ++i)
                                b[(size_t) i] = '=';
                            break;
                        case 'Y':
                            // (0.98.0, Faster's third bar) syncopated: open hits rung across the beats.
                            for (int h = 0; h < n; ++h)
                            {
                                if (syncopation[(size_t) h % syncopation.size()] == 'x' || h == 0)
                                    hits (b, h, h + 1, openChar);
                                else
                                    for (int j = 0; j < every() && h * every() + j < steps; ++j)
                                        b[(size_t) (h * every() + j)] = '=';
                            }
                            break;
                        case 'K':
                            // (0.99.0, Another Life's chorus) stabs: chord, chord, silence (holds ring each one on).
                            for (int h = 0; h < n; ++h)
                            {
                                const auto c = stab[(size_t) h % stab.size()];
                                if (c == 'x' || h == 0)
                                    hits (b, h, h + 1, openChar);
                                else if (c == '=')
                                    for (int j = 0; j < every() && h * every() + j < steps; ++j)
                                        b[(size_t) (h * every() + j)] = '=';
                            }
                            break;
                        case 'N':
                            // (0.100.0, Faster's verse) palm-muted single root notes on every eighth.
                            hits (b, 0, n, 'x');
                            break;
                        case 'J':
                            // (0.100.0) rocking: the muted power chord a beat, another degree a beat, in turn.
                            for (int h = 0; h < n; ++h)
                                hits (b, h, h + 1, (h / 2) % 2 == 0 ? '*' : degreeChar (halfDegree));
                            break;
                        case 'U':
                            // (0.99.0) eighths, the next chord pushed in on the bar's last eighth ('_').
                            hits (b, 0, juce::jmax (1, n - 1), openChar);
                            if (n >= 2)
                                hits (b, n - 1, n, '_');
                            break;
                        case 'L':
                            // (0.98.0, Faster's slides) open eighths, the last eighth a slide into the next chord:
                            // power chords a whole step and a half step under its root, a sixteenth each.
                            hits (b, 0, juce::jmax (1, n - 1), openChar);
                            if (steps >= 4 && ! breakdown())
                            {
                                b[(size_t) steps - 2] = '[';
                                b[(size_t) steps - 1] = ']';
                            }
                            break;
                        default:
                            // (0.100.0: MUTED DRIVE's chord bars are always muted power chords: its single notes are the N bars)
                            hits (b, 0, n, kind == 'O' ? openChar : mutedDrive ? '*' : mutedChar);
                            break;
                    }
                    if (walks)
                    {
                        const auto count = juce::jmin ((int) walk.size(), n - 1);
                        const auto from = n - count;
                        for (int k = 0; k < count; ++k)
                            hits (b, from + k, from + k + 1, walk[(size_t) k]);
                        if (mutedBefore && kind == 'O' && from >= 2)
                            hits (b, from - 1, from, mutedChar);
                    }
                    return b;
                };

                // The bars: upper case no walk, lower case the walk at its end.
                std::string shape;
                if (mutedDrive)
                {
                    // (0.100.0) MUTED DRIVE's approaches: single notes then chords, muted power chords, rocking chords;
                    // each ends walking into what comes next. AUTOMATIC: one by the take.
                    static const char* const one[] = { "n", "m", "j" };
                    static const char* const two[] = { "Nj", "Mm", "Jj" };
                    static const char* const four[] = { "NNMj", "MMMm", "JJJj" };
                    const auto a = (size_t) (recipe.approach >= 0 ? recipe.approach % 3 : pick<int> ({ { 0, 4 }, { 1, 2 }, { 2, 2 } }));
                    shape = bars <= 1 ? one[a] : bars == 2 ? two[a] : four[a];
                }
                else if (syncopated)
                {
                    // (0.99.0) SYNCOPATED DRIVE's approaches: stabs then eighths, a syncopated third bar, pushed changes,
                    // stabs then pushes. AUTOMATIC: one of them by the take.
                    static const char* const one[] = { "K", "Y", "U", "K" };
                    static const char* const two[] = { "KO", "OY", "UU", "KU" };
                    static const char* const four[] = { "KKOO", "OOYO", "UUUU", "KKUU" };
                    const auto a = (size_t) (recipe.approach >= 0 ? recipe.approach % 4 : pick<int> ({ { 0, 3 }, { 1, 2 }, { 2, 2 }, { 3, 2 } }));
                    shape = bars <= 1 ? one[a] : bars == 2 ? two[a] : four[a];
                }
                else if (recipe.approach >= 0 && ! breakdown())
                {
                    // (0.98.0) The DRIVING chorus's guitar approach chosen: its own shape (sectionApproaches).
                    static const char* const one[] = { "O", "Y", "O", "L", "S", "R" };
                    static const char* const two[] = { "Oo", "OY", "OM", "OL", "SS", "RR" };
                    static const char* const four[] = { "OOOo", "OOYo", "OMOM", "OLOL", "SSSs", "RRRR" };
                    const auto a = (size_t) (recipe.approach % 6);
                    shape = bars <= 1 ? one[a] : bars == 2 ? two[a] : four[a];
                }
                else if (breakdown())
                    shape = bars <= 1 ? pick<std::string> ({ { "O", 1 }, { "o", 1 } })
                          : bars == 2 ? pick<std::string> ({ { "Oo", 2 }, { "Om", 1 } })
                                      : pick<std::string> ({ { "OOMm", 2 }, { "OOOo", 1 } });
                else if (bars <= 1)
                    shape = pick<std::string> ({ { "O", 2 }, { "o", 2 }, { "R", 1.5f }, { "H", 1 }, { "S", 1 }, { "A", 1 } });
                else if (bars == 2)
                    shape = pick<std::string> ({ { "Oo", 3 }, { "Om", 2 }, { "RR", 2 }, { "OH", 1.5f }, { "OP", 1 }, { "SS", 1.5f }, { "WW", 0.5f }, { "AA", 1 }, { "OO", 0.5f } });
                else
                    shape = pick<std::string> ({ { "OOMm", 3 },            // the wall, then a muted stretch and a walk (This Fire Burns)
                                                 { "OOOo", 2 },            // all open, walking into the next chord (The End of Heartache)
                                                 { "RRRR", 2 },            // power chords moving inside the bar (Faint)
                                                 { "OOPH", 2 },            // a punch bar, then half and half (Faster)
                                                 { "OOOH", 2 },            // a chord a bar, the last bar half and half (Left Behind)
                                                 { "SSSs", 2 },            // full chords strummed, the top voice moving (Tears Don't Fall)
                                                 { "WWWW", 0.8f },         // big chords rung a bar each (This Calling)
                                                 { "AAAA", 1.5f },         // the chord picked a note an eighth (Come Clarity, Parallels)
                                                 { "AAOo", 1 },
                                                 { "OMOm", 1 } });
                std::vector<std::string> out;
                for (const auto c : shape)
                {
                    if ((int) out.size() >= bars)
                        break;
                    const auto upper = (char) juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) c);
                    auto kind = upper;
                    if (! openLed() && ! mutedDrive)
                        kind = kind == 'O' ? 'M' : kind == 'M' ? 'O' : (kind == 'S' || kind == 'W') ? 'M' : kind;   // a verse: the muted stretch first
                    out.push_back (bar (kind, c != upper));
                }
                return out;
            }
        };
    }

    namespace
    {
        // DRONING (0.88.0): see RiffGenerator.h. A bar is one of:
        //   W one chord struck on the bar line and rung the whole bar ('%' a full chord, '#' a power chord, 'o' the octave)
        //   V struck again halfway (the second time the same, or the fourth in the third's place: '&')
        //   T the power chord held the whole bar ('@': rings to the bar's end whatever comes after) while a top note
        //     moves over it ('+' the octave, '$' the third over it, ';' the fifth over it): Capricorn
        //   D rung, then the shape sliding down a half step at a time to the next bar (digits: power chords that many
        //     semitones over the root): Clown
        //   Q no strike: the chord before rings on, and the next chord is pushed on beat 4 (or the and of 4): '~' the
        //     next chord's full voicing struck early and rung across the bar line through the bar after; H a bar held,
        //     nothing struck (rung by the push before it): Circle With Me (the owner's rhythm guitar part); a bar
        //     before Q strikes '|' (the full chord rung through the bar after, tied over as in the example)
        // and a bar in lower case ends with the power chord stepping into the next chord on its last beat ('}' / ']').
        struct Droning
        {
            juce::Random& random;
            const RiffRecipe& recipe;
            int steps;

            template <typename T>
            T pick (std::initializer_list<std::pair<T, float>> options)
            {
                auto total = 0.0f;
                for (const auto& o : options)
                    total += o.second;
                auto r = random.nextFloat() * total;
                for (const auto& o : options)
                {
                    if (r < o.second)
                        return o.first;
                    r -= o.second;
                }
                return options.begin()->first;
            }

            bool verse() const { return recipe.section == DrumSection::verse || recipe.section == DrumSection::intro; }

            std::vector<std::string> make (int bars)
            {
                const auto chord = verse() ? pick<char> ({ { '#', 4 }, { 'o', 1 } }) : pick<char> ({ { '%', 3 }, { '#', 2 }, { 'o', 0.5f } });
                const auto again = verse() ? chord : pick<char> ({ { chord, 2 }, { '&', 1 } });
                // The top voice's moves (sixteenths: a character where it moves, the rest left to ring).
                const auto top = pick<std::string> ({ { "@-------+-------", 2 }, { "@---+---$---+---", 2 }, { "@-------+---;---", 1.5f },
                                                      { "@---$-------;---", 1 }, { "@-------$---+---", 1 } });
                const auto slide = pick<std::string> ({ { "#=======4===3===", 2 }, { "#=====5=4=3=2=1=", 1 }, { "#=======3=2=1===", 1 } });
                const auto into = random.nextFloat() < 0.5f ? '}' : ']';
                // Where the next chord is pushed: beat 4 (the owner's example), or the and of 4.
                const auto pushAt = random.nextFloat() < 0.75f ? steps - steps / 4 : steps - steps / 8;

                const auto bar = [&] (char kind, bool walks)
                {
                    std::string b ((size_t) steps, '=');
                    const auto half = steps / 2;
                    switch (kind)
                    {
                        case 'V':
                            b[0] = chord;
                            b[(size_t) half] = again;
                            break;
                        case 'T':
                        {
                            // '@' rings to the bar's end on its own; each top note rings on to the next.
                            b = std::string ((size_t) steps, '-');
                            for (int i = 0; i < 16; ++i)
                                if (top[(size_t) i] != '-')
                                    b[(size_t) (i * steps / 16)] = top[(size_t) i];
                            auto ringing = false;
                            for (int i = 1; i < steps; ++i)
                            {
                                if (b[(size_t) i] != '-')
                                    ringing = b[(size_t) i] != '@';
                                else if (ringing)
                                    b[(size_t) i] = '=';
                            }
                            break;
                        }
                        case 'D':
                            for (int i = 0; i < steps; ++i)
                                b[(size_t) i] = slide[(size_t) (i * 16 / steps)];
                            for (int i = 1; i < steps; ++i)   // (a character stretched over several steps: its first, then holds)
                                if (b[(size_t) i] != '=' && b[(size_t) i] == b[(size_t) i - 1])
                                    b[(size_t) i] = '=';
                            break;
                        case 'H':
                            break;   // (all held: the push before rings through it)
                        case 'P':
                        case 'Q':
                        {
                            if (kind == 'P')
                                b[0] = chord;
                            const auto at = juce::jlimit (1, steps - 1, pushAt);
                            b[(size_t) at] = '~';
                            break;
                        }
                        default:
                            b[0] = chord;
                            break;
                    }
                    if (walks && steps >= 8)
                    {
                        const auto at = steps - steps / 4;   // the last beat
                        b[(size_t) at] = into;
                        for (int i = at + 1; i < steps; ++i)
                            b[(size_t) i] = '=';
                    }
                    return b;
                };

                // The shape (drawn the same at any BUSY: BUSY only strikes more or fewer, below).
                std::string shape;
                if (recipe.approach >= 0)
                {
                    // (0.94.0) The approach chosen: only its shapes. 0 rung whole, 1 pushed, 2 moving top, 3 struck twice, 4 sliding.
                    switch (recipe.approach % 5)
                    {
                        case 1: shape = bars <= 1 ? "P" : bars == 2 ? "WQ" : "WQHQ"; break;
                        case 2: shape = bars <= 1 ? "T" : bars == 2 ? "TT" : "TTTT"; break;
                        case 3: shape = bars <= 1 ? "V" : bars == 2 ? "VV" : pick<std::string> ({ { "VVVV", 2 }, { "WWVV", 1 } }); break;
                        case 4: shape = bars <= 1 ? "D" : bars == 2 ? "WD" : pick<std::string> ({ { "DDDD", 1 }, { "WDWD", 1 } }); break;
                        default: shape = bars <= 1 ? "W" : bars == 2 ? pick<std::string> ({ { "WW", 3 }, { "Ww", 1 } })
                                                                    : pick<std::string> ({ { "WWWW", 3 }, { "WWWw", 1.5f } }); break;
                    }
                }
                else if (bars <= 1)
                    shape = pick<std::string> ({ { "W", 3 }, { "T", 2 }, { "V", 1.5f }, { "D", 1 }, { "P", 1.5f } });
                else if (bars == 2)
                    shape = pick<std::string> ({ { "WW", 3 }, { "WQ", 2.5f }, { "Ww", 1 }, { "TT", 2 }, { "VV", 1.5f }, { "WD", 1 } });
                else
                    shape = pick<std::string> ({ { "WWWW", 3 },     // a chord a bar, rung (Without Me)
                                                 { "WQHQ", 3 },     // struck, rung, the next chord pushed a beat early and held (Circle With Me)
                                                 { "WWWw", 1.5f },  // the last stepping into the next chord
                                                 { "TTTT", 2 },     // the power chord held, a top note moving over it (Capricorn)
                                                 { "VVVV", 1.5f },  // struck again halfway
                                                 { "WWVV", 1 },
                                                 { "DDDD", 1 },     // rung, then sliding down to the next bar (Clown)
                                                 { "WDWD", 1 } });
                if (verse() && recipe.approach % 5 != 2)   // (a verse rings power chords, no moving top: the chorus keeps those; unless asked for)
                    for (auto& c : shape)
                        c = c == 'T' ? 'W' : c == 't' ? 'w' : c;
                // BUSY: a fixed chance per bar; over the middle, a bar rung whole is struck again halfway when its chance
                // is under the amount; under it, a bar struck twice (or with a moving top) rings whole instead. Each step
                // of BUSY only adds to (or takes from) the one before; the riff stays the riff.
                const auto busy = juce::jlimit (0.0f, 1.0f, recipe.busy);
                for (auto& c : shape)
                {
                    const auto chance = random.nextFloat();
                    if (busy > 0.5f && c == 'W' && chance < (busy - 0.5f) * 2.0f)
                        c = 'V';
                    else if (busy < 0.5f && (c == 'V' || c == 'T') && chance < (0.5f - busy) * 2.0f)
                        c = 'W';
                }
                std::vector<std::string> out;
                for (size_t k = 0; k < shape.size(); ++k)
                {
                    const auto c = shape[k];
                    const auto upper = (char) juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) c);
                    auto b = bar (upper, c != upper);
                    // The bar before a push bar rings into it: its chord struck as '|' (rung through the bar after).
                    if (k + 1 < shape.size() && shape[k + 1] == 'Q' && b[0] != '=' && b[0] != '-')
                    {
                        b = std::string ((size_t) steps, '=');
                        b[0] = '|';
                    }
                    out.push_back (b);
                }
                return out;
            }
        };

        //======================================================================
        // (0.95.0) STACCATO CHUG's riff kinds (the owner's verses: Black, Stricken, Waffle; their shapes, our bars).
        struct StaccatoChug
        {
            juce::Random& random;
            const RiffRecipe& recipe;
            int steps;

            template <typename T>
            T pick (std::initializer_list<std::pair<T, float>> options)
            {
                auto total = 0.0f;
                for (const auto& o : options)
                    total += o.second;
                auto r = random.nextFloat() * total;
                for (const auto& o : options)
                {
                    if (r < o.second)
                        return o.first;
                    r -= o.second;
                }
                return options.begin()->first;
            }

            static char digit (int d) { return (char) ('0' + juce::jlimit (1, 9, d)); }

            // Black: x x x 5 x x 5 x x 5 ... (a stab every third sixteenth), the phrase's last bar ending on a rest and
            // two muted power chords into the next.
            std::vector<std::string> pedalStabs (int bars)
            {
                const auto plan = pick<std::string> ({ { "5577", 3 }, { "5557", 2 }, { "5578", 1.5f }, { "7755", 1 }, { "5535", 1 }, { "3355", 1 } });
                std::vector<std::string> out;
                for (int k = 0; k < bars; ++k)
                {
                    const auto degree = plan[(size_t) (bars == 1 ? 0 : bars == 2 ? k * 2 + 1 : k)] - '0';
                    std::string b ((size_t) steps, 'x');
                    for (int i = 3; i < steps; i += 3)
                        b[(size_t) i] = digit (degree);
                    if (k == bars - 1 && bars >= 2 && steps >= 8)
                    {
                        b[(size_t) steps - 3] = '-';
                        b[(size_t) steps - 2] = '*';
                        b[(size_t) steps - 1] = '*';
                    }
                    out.push_back (b);
                }
                return out;
            }

            // Stricken: - * * - * * ... then a walk of power chords up (w1, w2) and the root; the second bar walks twice;
            // the fourth climbs (d1 held in sixteenths, then d2).
            std::vector<std::string> chugThrees (int bars)
            {
                const auto walk = pick<std::pair<int, int>> ({ { { 3, 6 }, 3 }, { { 3, 5 }, 2 }, { { 1, 3 }, 1.5f }, { { 5, 6 }, 1 } });
                const auto climb = pick<std::pair<int, int>> ({ { { 3, 7 }, 3 }, { { 5, 7 }, 2 }, { { 3, 5 }, 1 }, { { 8, 7 }, 1 } });
                const auto threes = [this] (int upTo)
                {
                    std::string b ((size_t) steps, '-');
                    for (int i = 0; i < upTo; ++i)
                        b[(size_t) i] = i % 3 == 0 ? '-' : '*';
                    return b;
                };
                auto a = threes (steps - 3);
                if (steps >= 6)
                {
                    a[(size_t) steps - 3] = digit (walk.first);
                    a[(size_t) steps - 2] = digit (walk.second);
                    a[(size_t) steps - 1] = '*';
                }
                auto b = threes (juce::jmax (0, steps - 8));
                if (steps >= 10)
                {
                    b[(size_t) steps - 8] = '-';
                    b[(size_t) steps - 7] = '*';
                    b[(size_t) steps - 6] = digit (walk.first);
                    b[(size_t) steps - 5] = digit (walk.second);
                    b[(size_t) steps - 4] = '*';
                    b[(size_t) steps - 3] = digit (walk.first);
                    b[(size_t) steps - 2] = digit (walk.first);
                    b[(size_t) steps - 1] = '*';
                }
                std::string c ((size_t) steps, '-');
                for (int i = 1; i < steps; ++i)
                    c[(size_t) i] = i < steps / 2 - 1 ? digit (climb.first) : i == steps / 2 - 1 ? '*' : digit (climb.second);
                if (bars <= 1)
                    return { a };
                if (bars == 2)
                    return { a, c };
                return { a, b, a, c };
            }

            // Waffle: x - - x x x x = - x x = - - - - (single low notes, the last of the burst held), the cluster stab on
            // the last two eighths every other bar, the phrase's last bar picking up into the next.
            std::vector<std::string> burstSilence (int bars)
            {
                auto cell = pick<std::string> ({ { "x--xxxx=-xx=----", 3 }, { "x--xxx=-x-xx=---", 1.5f }, { "x-x--xxx=-x=----", 1 } });
                const auto stab = pick<char> ({ { '^', 2 }, { 'n', 1 } });
                cell.resize ((size_t) steps, '-');
                auto withStab = cell;
                if (steps >= 16)
                {
                    withStab[12] = stab;
                    withStab[13] = '=';
                    withStab[14] = stab;
                    withStab[15] = '=';
                }
                auto pickup = cell;
                if (steps >= 4)
                {
                    pickup[(size_t) steps - 2] = 'x';
                    pickup[(size_t) steps - 1] = 'x';
                }
                if (bars <= 1)
                    return { cell };
                if (bars == 2)
                    return { cell, withStab };
                return { cell, withStab, cell, pickup };
            }

            // (0.100.0, This Fire Burns' verse; shapes only) the root galloped (an eighth and two sixteenths, muted), the
            // second bar answered by a pinch squeal and a stab on the sixth, the fourth turning round on the seventh.
            std::vector<std::string> gallopSqueal (int bars)
            {
                const auto a = fitted ("x-xxx-xxx-xxx-xx");
                const auto b = fitted (pick<std::string> ({ { "x-xxx-xxp===8===", 3 }, { "x-xxx-xx8===p===", 2 }, { "x-xxx-xxp=======", 1 } }));
                const auto c = fitted (pick<std::string> ({ { "x-xxx-xxx-xxK===", 3 }, { "x-xxx-xxK===x-xx", 2 } }));
                if (bars <= 1)
                    return { a };
                if (bars == 2)
                    return { a, b };
                return { a, b, a, c };
            }

            // (0.96.0, SPLIT CHUG) A 16-step cell fitted to the bar: cut, or held out with rests.
            std::string fitted (std::string cell) const
            {
                cell.resize ((size_t) steps, '-');
                return cell;
            }

            // Going Under: # - - - - - - - # - # - - # - # (open power chords, each a sixteenth), the phrase's last bar a
            // single chord and silence (a breath before the next four).
            std::vector<std::string> secondHalfStabs (int bars)
            {
                const auto cell = fitted (pick<std::string> ({ { "#-------#-#--#-#", 3 }, { "#-------##-#-#--", 1.5f }, { "#------#-#-#--#-", 1 } }));
                const auto breath = fitted ("#---------------");
                std::vector<std::string> out;
                for (int k = 0; k < bars; ++k)
                    out.push_back (bars == 4 && k == 3 ? breath : cell);
                return out;
            }

            // Papercut: * * * - - 8 - 8 8 7 7 - - - - - (muted chugs on the root, then power chords stepping down: the
            // flat sixth to the fifth, or another pair), the fourth bar's pickup climbing into the next (2 2 3).
            std::vector<std::string> firstHalfBurst (int bars)
            {
                const auto moves = pick<std::pair<int, int>> ({ { { 8, 7 }, 3 }, { { 10, 8 }, 1.5f }, { { 5, 3 }, 1.5f }, { { 7, 5 }, 1 } });
                auto cell = fitted ("***--8-8877-----");
                for (auto& c : cell)
                    c = c == '8' ? digit (moves.first) : c == '7' ? digit (moves.second) : c;
                // The intro's way (bars 4-7): two chugs, a climb, the last chord held through the second half.
                if (random.nextFloat() < 0.3f)
                    cell = fitted ("**23-2-5========");
                auto pickup = cell;
                if (steps >= 16)
                {
                    pickup[13] = '2';
                    pickup[14] = '2';
                    pickup[15] = '3';
                }
                std::vector<std::string> out;
                for (int k = 0; k < bars; ++k)
                    out.push_back (bars >= 2 && k == bars - 1 ? pickup : cell);
                return out;
            }

            // Call Me When You're Sober: # = - - - - - - - - - - - [ = = (the chord on 1 held an eighth, the chord a whole
            // step under the next one on the last sixteenths, ringing into the bar line); its first bars open on three
            // quick muted chugs instead.
            std::vector<std::string> hitPush (int bars)
            {
                auto a = fitted ("#=--------------");
                auto b = fitted ("***-------------");
                if (steps >= 4)
                {
                    a[(size_t) steps - 3] = b[(size_t) steps - 3] = '[';
                    a[(size_t) steps - 2] = b[(size_t) steps - 2] = '=';
                    a[(size_t) steps - 1] = b[(size_t) steps - 1] = '=';
                }
                const auto shape = bars <= 1 ? std::string ("A") : bars == 2 ? pick<std::string> ({ { "AA", 2 }, { "BA", 1 } })
                                                                             : pick<std::string> ({ { "AAAA", 3 }, { "BBAA", 2 }, { "BABA", 1 } });
                std::vector<std::string> out;
                for (const auto c : shape)
                    out.push_back (c == 'B' ? b : a);
                return out;
            }

            std::vector<std::string> make (int bars)
            {
                switch (recipe.flavour)
                {
                    case RiffFlavour::pedalStabs: return pedalStabs (bars);
                    case RiffFlavour::chugThrees: return chugThrees (bars);
                    case RiffFlavour::secondHalfStabs: return secondHalfStabs (bars);
                    case RiffFlavour::firstHalfBurst: return firstHalfBurst (bars);
                    case RiffFlavour::hitPush: return hitPush (bars);
                    case RiffFlavour::gallopSqueal: return gallopSqueal (bars);
                    default: return burstSilence (bars);
                }
            }
        };
    }

    //==========================================================================
    std::vector<std::string> generateRiffBars (const RiffRecipe& recipe, int stepsPerBar)
    {
        const auto steps = juce::jlimit (4, 64, stepsPerBar);
        const auto bars = recipe.bars <= 1 ? 1 : recipe.bars <= 2 ? 2 : 4;
        juce::Random random ((juce::int64) recipe.seed * 2654435761LL + 977);
        random.nextInt();

        auto weights = styleWeights (recipe.style);
        auto flavour = recipe.flavour;
        if (flavour == RiffFlavour::any)
        {
            // ANY: lean towards a different flavour each seed (or none), so ANY covers everything.
            // (DRIVING EIGHTHS and DRONING left out: whole chorus shapes, chosen on purpose; the draw is as before 0.87.0.)
            const auto& all = riffFlavours();
            const auto pick = random.nextInt (9);   // (the nine before DRIVING EIGHTHS and DRONING)
            flavour = all[(size_t) pick];
            if (flavour == RiffFlavour::stabs && recipe.style != GuitarStyle::groove && random.nextFloat() < 0.6f)
                flavour = RiffFlavour::any;
        }
        lean (weights, flavour);
        shapeForSection (weights, recipe.section);
        auto leaned = recipe;
        leaned.flavour = flavour;
        Maker maker { leaned, random, weights, recipe.style != GuitarStyle::rock && recipe.style != GuitarStyle::shred };

        const auto newBar = [&] (bool turnaround)
        {
            std::string bar ((size_t) steps, '-');
            maker.fill (bar, 0, steps, steps, turnaround);
            return bar;
        };
        // A bar keeping `from`'s first half, the second half new.
        const auto answer = [&] (const std::string& from, bool turnaround)
        {
            auto bar = from;
            auto half = steps / 2;
            // (a held note crossing the half: cut there)
            for (auto i = half; i < steps; ++i)
                bar[(size_t) i] = '-';
            maker.fill (bar, half, steps, steps, turnaround);
            return bar;
        };

        std::vector<std::string> out;
        if (flavour == RiffFlavour::droning)
        {
            auto mixed = (juce::uint64) recipe.seed * 0xD1B54A32D192ED03ull;
            mixed ^= mixed >> 31;
            juce::Random own ((juce::int64) (mixed * 0x94D049BB133111EBull >> 1));
            Droning droning { own, leaned, steps };
            return droning.make (bars);   // (BUSY is in the shapes: the pass below would chug into the rings)
        }
        if (flavour == RiffFlavour::pedalStabs || flavour == RiffFlavour::chugThrees || flavour == RiffFlavour::burstSilence
            || flavour == RiffFlavour::secondHalfStabs || flavour == RiffFlavour::firstHalfBurst || flavour == RiffFlavour::hitPush
            || flavour == RiffFlavour::gallopSqueal)
        {
            // (0.95.0: its own random numbers from the seed; BUSY through the pass below, as every riff)
            auto mixed = (juce::uint64) recipe.seed * 0xA24BAED4963EE407ull;
            mixed ^= mixed >> 30;
            juce::Random own ((juce::int64) (mixed * 0x9FB21C651E98DF25ull >> 1));
            StaccatoChug staccato { own, leaned, steps };
            out = staccato.make (bars);
        }
        else if (flavour == RiffFlavour::drivingEighths || flavour == RiffFlavour::syncopatedDrive || flavour == RiffFlavour::mutedDrive)
        {
            // (its own random numbers, well mixed from the seed: neighbouring seeds drew alike from the shared one)
            auto mixed = (juce::uint64) recipe.seed * 0x9E3779B97F4A7C15ull;
            mixed ^= mixed >> 29;
            juce::Random own ((juce::int64) (mixed * 0xBF58476D1CE4E5B9ull >> 1));
            DrivingEighths driving { own, leaned, steps };
            out = driving.make (bars);
        }
        else if (flavour == RiffFlavour::tritone)
        {
            TritoneChug chug { random, leaned, steps };
            out = chug.make (bars);
            maker.pedal = true;
        }
        else
        {
        const auto a = newBar (false);
        // (BUSY is applied after, below: the same seed the same riff, only thinner or thicker.)
        out.push_back (a);
        if (bars >= 2)
            out.push_back (answer (a, bars == 2));
        if (bars == 4)
        {
            out.push_back (a);
            out.push_back (answer (a, true));
        }
        }

        // BUSY: one fixed chance per sixteenth (the same in every bar, so a bar answering another keeps the
        // half it shares). Under the middle, muted chugs on weak sixteenths whose chance is under the amount
        // drop out; over it, weak gaps (rests, or the tail of a held note) whose chance is under the amount
        // get a muted chug. Each step of BUSY only adds to (or takes from) the one before.
        const auto busy = juce::jlimit (0.0f, 1.0f, recipe.busy);
        if (std::abs (busy - 0.5f) > 1.0e-4f)
        {
            juce::Random chances ((juce::int64) recipe.seed * 40503LL + 11);
            std::vector<float> chance ((size_t) steps);
            for (auto& c : chance)
                c = chances.nextFloat();
            const auto thin = busy < 0.5f ? (0.5f - busy) * 2.0f * 0.75f : 0.0f;
            const auto thick = busy > 0.5f ? (busy - 0.5f) * 2.0f * 0.6f : 0.0f;
            const auto chugChar = maker.low (false);
            for (auto& bar : out)
            {
                for (int i = 1; i < steps; ++i)
                {
                    if (strengthOf (i, steps) > 1)
                        continue;
                    auto& ch = bar[(size_t) i];
                    const auto muted = ch == 'x' || ch == 'z' || ch == 'a' || ch == '*' || ch == 'c' || ch == 'd' || ch == 'f' || ch == 'g' || ch == 'k';
                    if (thin > 0.0f && muted && chance[(size_t) i] < thin)
                    {
                        ch = '-';
                        for (auto j = i + 1; j < steps && bar[(size_t) j] == '='; ++j)
                            bar[(size_t) j] = '-';
                    }
                    else if (thick > 0.0f && (ch == '-' || ch == '=') && chance[(size_t) i] < thick)
                    {
                        ch = chugChar;
                        for (auto j = i + 1; j < steps && bar[(size_t) j] == '='; ++j)
                            bar[(size_t) j] = '-';
                    }
                }
            }
        }
        return out;
    }

    LineIdea generateRiff (const RiffRecipe& recipe, const std::vector<ChordSpan>& spans, double beatsPerBar, const GuitarTuning& tuning,
                           const GuitarVelocities& vel)
    {
        const auto bars = generateRiffBars (recipe, riffStepsPerBar (beatsPerBar));
        // What it's made of, for its card and the style menu.
        std::map<juce::String, int> made;
        for (const auto& bar : bars)
            for (const auto ch : bar)
            {
                const auto c = (char) juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) ch);
                if (c == 'z' || c == 'x') ++made["chugs"];
                else if (c == '#') ++made["power chords"];
                else if (c == 'b' || c == 'e') ++made["tritones"];
                else if (c == 'q' || c == 'y' || c == 'j' || c == 'n' || c == '^') ++made["high grinds"];
                else if (c == 'a') ++made["low tritone chugs"];
                else if (c == 'i') ++made["pinch harmonics"];
                else if (c == 'l' || c == 'm' || c == 's') ++made["bent notes"];
                else if (c >= '1' && c <= '9') ++made["power chords"];
                else if (c == 'v' || c == 'w' || c == 'u') ++made["high stabs"];
                else if (c == 'o') ++made["octaves"];
                else if (c == 'h') ++made["high chords"];
                else if (c == 'p') ++made["pinch squeals"];
                else if (c == 'c' || c == 'd') ++made["half steps and minor thirds"];
                else if (c == '*') ++made["muted power chords"];
                else if (c == '%' || c == '&') ++made["full chords"];
                else if (c == '@') ++made["held power chords"];
                else if (c == '~') ++made["chords pushed a beat early"];
                else if (c == '|') ++made["full chords"];
                else if (c == '$') ++made["picked notes"];
                else if (c == '+' || c == ';') ++made["picked notes"];
                else if (c == '>' || c == '<' || c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']') ++made["walks into the next chord"];
            }
        std::vector<std::pair<int, juce::String>> ranked;
        for (const auto& [name, count] : made)
            ranked.push_back ({ count, name });
        std::sort (ranked.begin(), ranked.end(), [] (const auto& x, const auto& y) { return x.first > y.first || (x.first == y.first && x.second < y.second); });
        juce::StringArray words;
        for (size_t i = 0; i < juce::jmin ((size_t) 3, ranked.size()); ++i)
            words.add (ranked[i].second);
        auto description = words.joinIntoString (", ");
        if (description.isNotEmpty())
            description = description.substring (0, 1).toUpperCase() + description.substring (1);
        description << " (" << drumSectionName (recipe.section).toLowerCase() << ", " << (int) bars.size() << (bars.size() == 1 ? " bar)" : " bars)");

        const auto name = "Generated: " + (recipe.flavour == RiffFlavour::any ? juce::String ("any flavour") : riffFlavourName (recipe.flavour).toLowerCase());
        // (DRIVING EIGHTHS follows the chords' roots, never the key's pedal.)
        auto pedal = recipe.flavour != RiffFlavour::drivingEighths && recipe.flavour != RiffFlavour::syncopatedDrive
                  && recipe.flavour != RiffFlavour::mutedDrive && recipe.flavour != RiffFlavour::gallopSqueal
                  && recipe.flavour != RiffFlavour::droning && recipe.style != GuitarStyle::rock && recipe.style != GuitarStyle::shred;
        // (0.95.0) Black's stabs climb over the low string; Stricken's and Waffle's chugs follow each chord's root.
        pedal = (pedal || recipe.flavour == RiffFlavour::pedalStabs) && recipe.flavour != RiffFlavour::chugThrees && recipe.flavour != RiffFlavour::burstSilence
             && recipe.flavour != RiffFlavour::secondHalfStabs && recipe.flavour != RiffFlavour::firstHalfBurst && recipe.flavour != RiffFlavour::hitPush;   // (0.96.0: SPLIT CHUG's follow the roots)
        for (const auto& b : bars)
            pedal = pedal || b.find_first_of ("zZ") != std::string::npos;   // (TRITONE CHUG: the open low string in any style)
        return riffFromBars ([&bars] (int barIndex) { return bars[(size_t) (((barIndex % (int) bars.size()) + (int) bars.size()) % (int) bars.size())]; },
                             pedal, name, description, spans, beatsPerBar, (int) (recipe.seed % 3), tuning, vel);
    }
}
