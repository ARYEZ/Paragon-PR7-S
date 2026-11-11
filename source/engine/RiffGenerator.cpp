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
            case RiffFlavour::tritone: return "TRITONE";
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
            case RiffFlavour::tritone: return "The flat fifth against the low string: chugs, dyads and notes a tritone up";
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
                    const auto muted = ch == 'x' || ch == 'z' || ch == '*' || ch == 'c' || ch == 'd' || ch == 'f' || ch == 'g' || ch == 'k';
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
        const auto pedal = recipe.style != GuitarStyle::rock && recipe.style != GuitarStyle::shred;
        return riffFromBars ([&bars] (int barIndex) { return bars[(size_t) (((barIndex % (int) bars.size()) + (int) bars.size()) % (int) bars.size())]; },
                             pedal, name, description, spans, beatsPerBar, (int) (recipe.seed % 3), tuning, vel);
    }
}
