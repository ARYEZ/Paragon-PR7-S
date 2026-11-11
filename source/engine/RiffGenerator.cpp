#include "engine/RiffGenerator.h"

#include <array>
#include <cmath>
#include <map>

namespace pr7s
{
    //==========================================================================
    const std::vector<RiffFlavour>& riffFlavours()
    {
        static const std::vector<RiffFlavour> all { RiffFlavour::any, RiffFlavour::chugs, RiffFlavour::gallop, RiffFlavour::tritone, RiffFlavour::creep,
                                                    RiffFlavour::stabs, RiffFlavour::octaves, RiffFlavour::stopStart, RiffFlavour::openChords };
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
                    return pick<char> ({ { 'q', 5 }, { 'j', 2 }, { 'y', 2 } });
                if (breakdown())
                    return pick<char> ({ { 'q', 4 }, { 'n', 2 }, { 'y', 1.5f } });
                return pick<char> ({ { 'q', 7 }, { 'y', 2 }, { 'j', 1.5f }, { 'n', 1 } });
            }

            // Chugs into [from, to) from the cells (a beat each, alternating as the pattern says).
            static void chugs (std::string& bar, int from, int to, const std::string& a, const std::string& b, const std::string& order,
                               int chromaticAt)
            {
                for (auto i = from; i < to; ++i)
                {
                    const auto beat = (i - from) / 4;
                    const auto& c = order[(size_t) beat % order.size()] == 'a' ? a : b;
                    const auto ch = c[(size_t) ((i - from) % 4)];
                    bar[(size_t) i] = ch == 'x' ? (i == chromaticAt ? 'c' : 'z') : '-';
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
                const auto place = pick<int> ({ { 0, 7 }, { 1, 2 }, { 2, 1 } });   // 0 the downbeat, 1 after a chug, 2 answering the chugs
                const auto a = cell(), b = random.nextFloat() < 0.45f ? cell() : a;
                const auto order = pick<std::string> ({ { "a", 3 }, { "ab", 2 }, { "aab", 1.5f }, { "aaab", 1 } });
                const auto chromatic = random.nextFloat() < 0.3f;

                const auto call = [&] (char voice)
                {
                    std::string bar ((size_t) steps, '-');
                    auto stabAt = 0;
                    if (place == 1)
                        stabAt = 2;                                   // a chug, then the grind on the "and"
                    else if (place == 2)
                        stabAt = juce::jmax (4, ((steps - ring) / 4) * 4);   // the chugs first, the grind last
                    stabAt = juce::jmin (stabAt, steps - 2);
                    const auto end = juce::jmin (steps, stabAt + ring);
                    if (place == 2)
                    {
                        chugs (bar, 0, stabAt, a, b, order, chromatic ? stabAt - 1 : -1);
                        bar[0] = 'z';
                    }
                    else
                    {
                        if (place == 1)
                            bar[0] = 'z', bar[1] = '-';
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
                    const auto kind = breakdown() ? pick<int> ({ { 0, 3 }, { 2, 2 }, { 3, 2 } }) : pick<int> ({ { 0, 6 }, { 1, 2 }, { 2, 2 }, { 3, 1 } });
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
            const auto& all = riffFlavours();
            const auto pick = random.nextInt ((int) all.size());
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
        if (flavour == RiffFlavour::tritone)
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
                else if (c == 'q' || c == 'y' || c == 'j' || c == 'n') ++made["high tritones"];
                else if (c == 'a') ++made["low tritone chugs"];
                else if (c == 'i') ++made["pinch harmonics"];
                else if (c == 'l' || c == 'm' || c == 's') ++made["bent notes"];
                else if (c >= '1' && c <= '9') ++made["power chords"];
                else if (c == 'v' || c == 'w' || c == 'u') ++made["high stabs"];
                else if (c == 'o') ++made["octaves"];
                else if (c == 'h') ++made["high chords"];
                else if (c == 'p') ++made["pinch squeals"];
                else if (c == 'c' || c == 'd') ++made["half steps and minor thirds"];
                else if (c == '*') ++made["muted dyads"];
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
        auto pedal = recipe.style != GuitarStyle::rock && recipe.style != GuitarStyle::shred;
        for (const auto& b : bars)
            pedal = pedal || b.find_first_of ("zZ") != std::string::npos;   // (TRITONE CHUG: the open low string in any style)
        return riffFromBars ([&bars] (int barIndex) { return bars[(size_t) (((barIndex % (int) bars.size()) + (int) bars.size()) % (int) bars.size())]; },
                             pedal, name, description, spans, beatsPerBar, (int) (recipe.seed % 3), tuning, vel);
    }
}
