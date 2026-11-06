#include "engine/Takes.h"

#include <functional>

namespace pr7s
{
    namespace
    {
        int pcOf (int note) { return ((note % 12) + 12) % 12; }

        template <typename List>
        bool has (const List& list, int value) { return std::find (list.begin(), list.end(), value) != list.end(); }

        int nearestPitch (int pitchClass, double around, int low, int high)
        {
            auto best = -1;
            for (auto n = low; n <= high; ++n)
                if (pcOf (n) == pcOf (pitchClass) && (best < 0 || std::abs (n - around) < std::abs (best - around)))
                    best = n;
            if (best >= 0)
                return best;
            // (a range under an octave without that note: the nearest outside it)
            for (auto d = 1; d < 12; ++d)
            {
                if (pcOf (low - d) == pcOf (pitchClass)) return low - d;
                if (pcOf (high + d) == pcOf (pitchClass)) return high + d;
            }
            return low;
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

        std::vector<int> tonesOf (const Chord& c) { return c.pitchClasses(); }

        int fold (int pitch, int low, int high)
        {
            while (pitch < low && pitch + 12 <= high + 11)
                pitch += 12;
            while (pitch > high && pitch - 12 >= low - 11)
                pitch -= 12;
            return juce::jlimit (0, 127, pitch);
        }

        void add (std::vector<LineNote>& notes, double at, double length, int pitch, float velocity)
        {
            if (length > 1.0e-6)
                notes.push_back ({ at, length * 0.96, juce::jlimit (0, 127, pitch), velocity });
        }

        bool chance (juce::Random& r, double p) { return r.nextDouble() < p; }

        //======================================================================
        // A hit of a one-bar cell: its sixteenth, what it plays, and how long it rings (1: to the next
        // hit of its hand; less: shorter).
        enum class Role { root, fifth, octave, seventh, third, lead, next, walk, skip,
                          chord, nextChord, arp, leftRoot, leftFifth, leftLow, leftOctaves };
        struct Hit
        {
            int step = 0;
            Role role = Role::root;
            int index = 0;          // arp: which note of the shape
            float velocity = 0.8f;
            float hold = 1.0f;
        };
        using Cell = std::vector<Hit>;

        bool leftHand (Role r) { return r == Role::leftRoot || r == Role::leftFifth || r == Role::leftLow || r == Role::leftOctaves; }

        float metric (int step)
        {
            return step % 4 == 0 ? 3.0f : step % 2 == 0 ? 1.5f : 0.4f;
        }

        // `count` sixteenths of a bar of `n`, drawn by weight; `forced` first.
        std::vector<int> drawSteps (juce::Random& r, int n, int count, const std::function<float (int)>& weight, std::vector<int> forced)
        {
            std::vector<int> out;
            for (const auto f : forced)
                if (f >= 0 && f < n && ! has (out, f))
                    out.push_back (f);
            count = juce::jlimit ((int) out.size(), n, count);
            while ((int) out.size() < count)
            {
                auto total = 0.0f;
                for (auto s = 0; s < n; ++s)
                    if (! has (out, s))
                        total += juce::jmax (0.0f, weight (s));
                if (total <= 0.0f)
                    break;
                auto pick = r.nextFloat() * total;
                auto chosen = -1;
                for (auto s = 0; s < n; ++s)
                    if (! has (out, s))
                    {
                        chosen = s;
                        pick -= juce::jmax (0.0f, weight (s));
                        if (pick <= 0.0f)
                            break;
                    }
                out.push_back (chosen);
            }
            std::sort (out.begin(), out.end());
            return out;
        }

        // How many hits: `usual` at busy 0.5, `least` at 0, `most` at 1; a little either way per take.
        int countFor (juce::Random& r, float busy, double least, double usual, double most)
        {
            const auto base = busy <= 0.5f ? juce::jmap ((double) busy, 0.0, 0.5, least, usual) : juce::jmap ((double) busy, 0.5, 1.0, usual, most);
            return juce::jlimit ((int) std::floor (least), (int) std::ceil (most), (int) std::lround (base + (r.nextDouble() - 0.5) * 1.1));
        }

        Cell sorted (Cell c)
        {
            std::stable_sort (c.begin(), c.end(), [] (const Hit& a, const Hit& b) { return a.step < b.step; });
            return c;
        }

        // B: the A cell a hit different (one moved, or its note changed, or one more or fewer).
        Cell varied (const Cell& a, juce::Random& r, int n, const std::function<Role (juce::Random&)>& role, bool canMove = true)
        {
            auto b = a;
            if (b.size() < 2)
                return b;
            const auto which = 1 + r.nextInt ((int) b.size() - 1);   // (never the bar's first hit)
            const auto kind = r.nextInt (3);
            if (kind == 0 && canMove)
            {
                for (auto tries = 0; tries < 12; ++tries)
                {
                    const auto step = 1 + r.nextInt (juce::jmax (1, n - 1));
                    if (std::none_of (b.begin(), b.end(), [step] (const Hit& h) { return h.step == step; }))
                    {
                        b[(size_t) which].step = step;
                        break;
                    }
                }
            }
            else if (kind == 1)
                b[(size_t) which].role = role (r);
            else if (b.size() > 2)
                b.erase (b.begin() + which);
            return sorted (b);
        }

        // The fill: the A cell's last beat replaced by lead-in notes into the next chord.
        Cell fillOf (const Cell& a, juce::Random& r, int n, float busy, Role leadRole)
        {
            if (busy < 0.15f || n < 8)
                return a;
            Cell f;
            const auto from = n - 4;
            for (const auto& h : a)
                if (h.step < from)
                    f.push_back (h);
            const auto count = juce::jlimit (1, 4, 1 + (int) std::lround (busy * 2.4 + r.nextDouble() * 0.8 - 0.4));
            const auto steps = drawSteps (r, 4, count, [] (int s) { return s % 2 == 0 ? 2.0f : 1.0f; }, {});
            for (const auto s : steps)
                f.push_back ({ from + s, leadRole, 0, 0.74f, 1.0f });
            return sorted (f);
        }

        struct Event
        {
            double at = 0.0;   // beats from the first chord's start
            size_t span = 0;
            Hit hit;
            bool lastOfChord = false;
            int leadsLeft = 0;   // lead hits after it in this chord
        };

        // Each bar of each chord played through the take's cells (A, A or B, A, fill per four bars).
        std::vector<Event> realise (const std::vector<ChordSpan>& spans, double beatsPerBar, const Cell& a, const Cell& b, const Cell& f, bool useB)
        {
            std::vector<Event> out;
            const auto origin = spans.front().start;
            for (size_t s = 0; s < spans.size(); ++s)
                for (auto t = spans[s].start; t < spans[s].end - 1.0e-9; t += beatsPerBar)
                {
                    const auto bar = (int) std::floor ((t - origin) / beatsPerBar + 1.0e-6);
                    // (the fill on a phrase's last bar, or on a chord's last bar before a change when the
                    // phrase is shorter: the end of the idea)
                    const auto lastBarOfChord = t + beatsPerBar >= spans[s].end - 1.0e-9;
                    const auto& cell = bar % 4 == 3 && lastBarOfChord ? f : bar % 4 == 1 && useB ? b : a;
                    for (const auto& h : cell)
                    {
                        const auto at = t + h.step * 0.25;
                        if (at >= spans[s].end - 1.0e-9)
                            continue;
                        out.push_back ({ at - origin, s, h });
                    }
                }
            for (size_t i = 0; i < out.size(); ++i)
            {
                out[i].lastOfChord = i + 1 == out.size() || out[i + 1].span != out[i].span;
                auto leads = 0;
                for (auto j = i + 1; j < out.size() && out[j].span == out[i].span; ++j)
                    if (out[j].hit.role == Role::lead)
                        ++leads;
                out[i].leadsLeft = leads;
            }
            return out;
        }

        // How long each event rings: to the next of its hand, or the chord's end.
        double lengthOf (const std::vector<Event>& events, size_t i, const std::vector<ChordSpan>& spans)
        {
            const auto origin = spans.front().start;
            const auto& e = events[i];
            const auto left = leftHand (e.hit.role);
            auto end = spans[e.span].end - origin;
            for (auto j = i + 1; j < events.size(); ++j)
                if (leftHand (events[j].hit.role) == left && events[j].at > e.at + 1.0e-6)
                {
                    end = juce::jmin (end, events[j].at);
                    break;
                }
            return juce::jmax (0.06, (end - e.at) * e.hit.hold);
        }
    }

    //==========================================================================
    const juce::StringArray& bassApproaches()
    {
        static const juce::StringArray all { "Roots", "Root and fifth", "Driving eighths", "Octaves", "Walking", "Syncopated" };
        return all;
    }

    const juce::StringArray& pianoApproaches()
    {
        static const juce::StringArray all { "Block chords", "Broken chords", "Ballad arpeggio", "Stride", "Octaves and chords", "Pop pulse" };
        return all;
    }

    const std::vector<GuitarTuning>& bassTunings()
    {
        static const std::vector<GuitarTuning> all {
            { "4-string standard (E)", { 28, 33, 38, 43 } },
            { "5-string standard (B)", { 23, 28, 33, 38, 43 } },
            { "4-string drop D", { 26, 33, 38, 43 } },
            { "4-string drop C", { 24, 31, 36, 41 } },
            { "5-string drop A", { 21, 28, 33, 38, 43 } },
            { "6-string standard (B)", { 23, 28, 33, 38, 43, 48 } } };
        return all;
    }

    const GuitarTuning& bassTuningNamed (const juce::String& name)
    {
        for (const auto& t : bassTunings())
            if (t.name == name)
                return t;
        return bassTunings().front();
    }

    void keepInRange (LineIdea& idea, int lowest, int highest)
    {
        if (highest - lowest < 11)
            return;
        for (auto& n : idea.notes)
        {
            while (n.pitch < lowest)
                n.pitch += 12;
            while (n.pitch > highest)
                n.pitch -= 12;
        }
    }

    //==========================================================================
    LineIdea bassTake (int approach, const std::vector<ChordSpan>& spans, double beatsPerBar, const TakeOptions& o)
    {
        approach = juce::jlimit (0, bassApproaches().size() - 1, approach);
        static const char* descriptions[] = {
            "Each chord's root: the steadiest foundation (a new rhythm each take)",
            "The root, then its fifth (and the octave or root between, per take)",
            "Eighth notes on the root, with octave pops and lead-ins of the take's own",
            "The root and its octave in a pattern of the take's own",
            "A note a beat through the chord and the key, stepping into each next chord",
            "Off-beat hits on the root, octave, fifth and seventh, leading into each chord" };
        LineIdea idea { LineIdea::Part::bass, bassApproaches()[approach], descriptions[approach], {} };
        if (spans.empty() || beatsPerBar <= 0.0)
            return idea;
        juce::Random r ((juce::int64) o.seed * 2654435761LL + approach * 977 + 13);
        const auto busy = juce::jlimit (0.0f, 1.0f, o.busy);
        const auto n = juce::jmax (1, (int) std::lround (beatsPerBar * 4.0));
        const auto lowest = o.lowest, highest = juce::jmax (o.lowest + 12, o.highest);

        // The take's own choices.
        const auto rootLow = lowest, rootHigh = o.lowString ? lowest + 12 : juce::jmax (lowest + 12, highest - 10);
        const double centres[] = { (double) lowest + 6.0, (double) lowest + 9.0, (double) lowest + 12.0 };
        const auto centre = o.lowString ? (double) lowest + 5.0 : centres[r.nextInt (3)];
        const auto fifthBelow = chance (r, 0.5);
        const auto approachType = r.nextInt (4);      // 0 half step under, 1 over, 2 scale step, 3 the fifth
        const auto approachChance = 0.35 + r.nextDouble() * 0.6;
        const auto useB = chance (r, 0.6);
        const auto anticipate = chance (r, 0.5);

        std::vector<int> roots;
        {
            double prev = centre;
            for (const auto& s : spans)
            {
                const auto note = nearestPitch (bassOf (s.chord), (prev * 2.0 + centre) / 3.0, rootLow, rootHigh);
                roots.push_back (note);
                prev = note;
            }
        }
        const auto nextOf = [&] (size_t s) { return s + 1 < spans.size() ? s + 1 : (size_t) 0; };   // (the last leads back to the first: ideas loop)
        const auto fifthOf = [&] (size_t s)
        {
            const auto root = roots[s];
            const auto& tones = spans[s].chord.pitchClasses();
            auto step = 7;
            for (const auto st : { 7, 6, 8 })
                if (has (tones, pcOf (root + st)))
                {
                    step = st;
                    break;
                }
            const auto below = root + step - 12, above = root + step;
            if (o.lowString || fifthBelow)
                return below >= lowest ? below : above;
            return above <= highest ? above : below;
        };
        const auto octaveOf = [&] (size_t s) { return roots[s] + 12 <= highest ? roots[s] + 12 : roots[s]; };
        const auto seventhOf = [&] (size_t s)
        {
            const auto& tones = spans[s].chord.pitchClasses();
            for (const auto st : { 10, 11 })
                if (has (tones, pcOf (roots[s] + st)))
                    return roots[s] + st <= highest ? roots[s] + st : roots[s] + st - 12;
            return spans[s].chord.type == Chord::Type::minor ? (roots[s] + 10 <= highest ? roots[s] + 10 : roots[s] - 2) : octaveOf (s);
        };
        const auto thirdOf = [&] (size_t s)
        {
            const auto& tones = spans[s].chord.pitchClasses();
            for (const auto st : { 3, 4, 2, 5 })
                if (has (tones, pcOf (roots[s] + st)))
                    return roots[s] + st;
            return fifthOf (s);
        };
        // Into the next chord's root: `k` notes before it (1: the note just before).
        const auto leadIn = [&] (size_t s, int k, int type)
        {
            const auto target = roots[nextOf (s)];
            const auto& key = spans[nextOf (s)].key;
            const auto scale = key.scale();
            switch (type)
            {
                case 0: return target - k;
                case 1: return target + k;
                case 2:
                {
                    auto p = target;
                    const auto dir = roots[s] <= target ? -1 : 1;   // from the side we're coming from
                    for (auto i = 0; i < k; ++i)
                        p = stepFrom (p, dir, scale, lowest, highest + 5);
                    return p;
                }
                default:
                {
                    const auto fifth = target + 7 <= highest ? target + 7 : target - 5;
                    return k == 1 ? fifth : fifth + (fifth > target ? k - 1 : 1 - k);
                }
            }
        };
        const auto pickRole = [] (juce::Random& rr, std::initializer_list<std::pair<Role, float>> weights)
        {
            auto total = 0.0f;
            for (const auto& w : weights)
                total += w.second;
            auto pick = rr.nextFloat() * total;
            for (const auto& w : weights)
            {
                pick -= w.second;
                if (pick <= 0.0f)
                    return w.first;
            }
            return weights.begin()->first;
        };

        Cell a;
        std::function<Role (juce::Random&)> otherRole = [] (juce::Random&) { return Role::root; };
        auto leadRole = Role::lead;
        auto canMove = true;
        auto approaches = false;
        switch (approach)
        {
            case 0:   // Roots
            {
                const auto steps = drawSteps (r, n, countFor (r, busy, 1, 1.6, 6),
                                              [n] (int s) { return metric (s) + (s == n - 2 ? 1.5f : 0.0f); }, { 0 });
                for (const auto s : steps)
                    a.push_back ({ s, Role::root, 0, s % 4 == 0 ? 0.86f : 0.74f, 1.0f });
                leadRole = Role::next;
                break;
            }
            case 1:   // Root and fifth
            {
                otherRole = [&pickRole] (juce::Random& rr) { return pickRole (rr, { { Role::root, 0.45f }, { Role::fifth, 0.35f }, { Role::octave, 0.2f } }); };
                const auto steps = drawSteps (r, n, countFor (r, busy, 2, 2.3, 6), metric, { 0, n / 2 });
                for (const auto s : steps)
                    a.push_back ({ s, s == 0 ? Role::root : s == n / 2 ? Role::fifth : otherRole (r), 0, s % 4 == 0 ? 0.85f : 0.74f, 1.0f });
                approaches = true;
                break;
            }
            case 2:   // Driving eighths
            {
                std::vector<int> steps;
                for (auto s = 0; s < n; s += 2)
                    steps.push_back (s);
                if (busy < 0.4f)
                {
                    auto drop = (int) std::lround ((0.4f - busy) / 0.4f * 3.0f);
                    for (auto tries = 0; drop > 0 && tries < 40; ++tries)
                    {
                        const auto i = 1 + r.nextInt (juce::jmax (1, (int) steps.size() - 1));
                        if (i < (int) steps.size() && steps[(size_t) i] != n / 2)
                        {
                            steps.erase (steps.begin() + i);
                            --drop;
                        }
                    }
                }
                if (busy > 0.6f)
                    steps = drawSteps (r, n, (int) steps.size() + (int) std::lround ((busy - 0.6f) / 0.4f * 4.0f),
                                       [n] (int s) { return s % 2 == 1 ? 1.0f + (float) s / (float) n : 0.0f; }, steps);
                // The take's octave pops (off the beat) and a fifth.
                std::vector<int> offs;
                for (const auto s : steps)
                    if (s % 4 == 2)
                        offs.push_back (s);
                std::vector<int> pops;
                for (auto k = r.nextInt (3); k > 0 && ! offs.empty(); --k)
                {
                    const auto i = r.nextInt ((int) offs.size());
                    pops.push_back (offs[(size_t) i]);
                    offs.erase (offs.begin() + i);
                }
                const auto fifthAt = ! offs.empty() && chance (r, 0.35) ? offs[(size_t) r.nextInt ((int) offs.size())] : -1;
                for (const auto s : steps)
                    a.push_back ({ s, has (pops, s) ? Role::octave : s == fifthAt ? Role::fifth : Role::root, 0,
                                   s % 4 == 0 ? 0.9f : s % 2 == 0 ? 0.74f : 0.64f, 0.9f });
                otherRole = [&pickRole] (juce::Random& rr) { return pickRole (rr, { { Role::root, 0.5f }, { Role::octave, 0.35f }, { Role::fifth, 0.15f } }); };
                canMove = false;
                approaches = true;
                break;
            }
            case 3:   // Octaves
            {
                static const char* patterns[] = { "LHLHLHLH", "LLHLLHLH", "LHHLLHHL", "LHLLLHLH", "LLHHLLHH", "LHLHLLHL", "LHLHHLHL" };
                const juce::String pattern (patterns[r.nextInt (7)]);
                for (auto s = 0, i = 0; s < n; s += 2, ++i)
                {
                    const auto high = pattern[i % pattern.length()] == 'H';
                    if (high && busy < 0.4f && chance (r, (0.4f - busy) / 0.4f * 0.6f))
                        continue;
                    a.push_back ({ s, high ? Role::octave : Role::root, 0, high ? 0.7f : (s % 4 == 0 ? 0.88f : 0.78f), 0.85f });
                    if (busy > 0.6f && s + 1 < n && chance (r, (busy - 0.6f) / 0.4f * 0.5f))
                        a.push_back ({ s + 1, high ? Role::octave : Role::root, 0, 0.62f, 0.85f });
                }
                a = sorted (a);
                otherRole = [] (juce::Random& rr) { return rr.nextBool() ? Role::octave : Role::root; };
                canMove = false;
                break;
            }
            case 4:   // Walking
            {
                if (busy < 0.3f && n >= 8)
                {
                    a.push_back ({ 0, Role::root, 0, 0.86f, 1.0f });
                    a.push_back ({ n / 2, Role::walk, 0, 0.76f, 1.0f });
                }
                else
                {
                    for (auto s = 0; s < n; s += 4)
                        a.push_back ({ s, s == 0 ? Role::root : Role::walk, 0, s == 0 ? 0.86f : 0.74f, 1.0f });
                    if (busy > 0.65f)
                        for (auto k = (int) std::lround ((busy - 0.65f) / 0.35f * 3.0f + r.nextDouble() * 0.6); k > 0; --k)
                        {
                            const auto beat = r.nextInt (juce::jmax (1, n / 4));
                            const auto s = beat * 4 + 2;
                            if (s < n && std::none_of (a.begin(), a.end(), [s] (const Hit& h) { return h.step == s; }))
                                a.push_back ({ s, Role::skip, 0, 0.55f, 0.5f });
                        }
                    a = sorted (a);
                }
                otherRole = [] (juce::Random&) { return Role::walk; };
                canMove = false;
                break;
            }
            default:  // Syncopated
            {
                const auto steps = drawSteps (r, n, countFor (r, busy, 3, 5, 9),
                                              [] (int s) { return s % 4 == 2 ? 3.0f : s % 4 == 3 ? 2.2f : s % 4 == 0 ? 1.0f : 0.6f; }, { 0 });
                otherRole = [&pickRole] (juce::Random& rr)
                { return pickRole (rr, { { Role::root, 0.4f }, { Role::octave, 0.25f }, { Role::fifth, 0.2f }, { Role::seventh, 0.15f } }); };
                for (const auto s : steps)
                    a.push_back ({ s, s == 0 ? Role::root : otherRole (r), 0, s % 4 == 0 ? 0.9f : 0.75f, chance (r, 0.5) ? 1.0f : 0.55f });
                approaches = true;
                break;
            }
        }
        const auto b = varied (a, r, n, otherRole, canMove);
        const auto f = approach == 4 ? a : fillOf (a, r, n, busy, leadRole);
        const auto events = realise (spans, beatsPerBar, a, b, f, useB);

        // The notes.
        auto walkPitch = roots[0];
        const auto walkLow = lowest, walkHigh = o.lowString ? lowest + 14 : highest;
        auto walkDir = r.nextBool() ? 1 : -1;
        const auto toneBias = 0.3 + r.nextDouble() * 0.5;
        for (size_t i = 0; i < events.size(); ++i)
        {
            const auto& e = events[i];
            const auto s = e.span;
            const auto firstOfChord = i == 0 || events[i - 1].span != s;
            const auto hasNext = spans.size() > 1;
            int pitch = roots[s];
            switch (e.hit.role)
            {
                case Role::root: pitch = roots[s]; break;
                case Role::fifth: pitch = fifthOf (s); break;
                case Role::octave: pitch = octaveOf (s); break;
                case Role::seventh: pitch = seventhOf (s); break;
                case Role::third: pitch = thirdOf (s); break;
                case Role::next: pitch = hasNext ? roots[nextOf (s)] : roots[s]; break;
                case Role::lead: pitch = hasNext ? leadIn (s, e.leadsLeft + 1, approachType) : roots[s]; break;
                case Role::skip: pitch = walkPitch; break;
                case Role::walk:
                default:
                {
                    if (e.lastOfChord && hasNext)
                        pitch = leadIn (s, 1, chance (r, 0.7) ? approachType : r.nextInt (4));
                    else
                    {
                        const auto target = roots[nextOf (s)];
                        if (firstOfChord || chance (r, 0.15))
                            walkDir = target > walkPitch ? 1 : target < walkPitch ? -1 : walkDir;
                        const auto& span = spans[s];
                        const auto tones = tonesOf (span.chord);
                        const auto next = stepFrom (walkPitch, walkDir, availableNotes (span.chord, span.key), walkLow, walkHigh + 5);
                        const auto chordTone = stepFrom (walkPitch, walkDir, tones, walkLow, walkHigh + 5);
                        pitch = std::abs (chordTone - walkPitch) <= 4 && chance (r, toneBias) ? chordTone : next;
                    }
                    break;
                }
            }
            if (firstOfChord && e.hit.step == 0 && e.hit.role != Role::skip)
                pitch = roots[s];
            // Roots' anticipation: a hit in the chord's last half beat plays the next chord's root.
            if (approach == 0 && anticipate && e.lastOfChord && ! firstOfChord && hasNext
                && e.at >= spans[s].end - spans.front().start - 0.5 - 1.0e-6)
                pitch = roots[nextOf (s)];
            // A lead into the next chord on the chord's last hit (the take's way, now and then another).
            if (approaches && e.lastOfChord && ! firstOfChord && hasNext && e.hit.role != Role::lead && chance (r, approachChance))
                pitch = leadIn (s, 1, chance (r, 0.75) ? approachType : r.nextInt (4));
            if (e.hit.role == Role::walk || e.hit.role == Role::root || firstOfChord)
                walkPitch = pitch;
            if (e.hit.role != Role::skip)
                walkPitch = pitch;
            // LOW STRING: anything over its 12th fret down an octave where that's still on the bass (but the
            // Octaves approach's high notes: the approach itself).
            if (o.lowString && ! (approach == 3 && e.hit.role == Role::octave))
                while (pitch > lowest + 12 && pitch - 12 >= lowest)
                    pitch -= 12;
            pitch = fold (pitch, lowest, highest);
            add (idea.notes, e.at, lengthOf (events, i, spans), pitch, e.hit.velocity);
        }
        keepInRange (idea, lowest, highest);
        return idea;
    }

    //==========================================================================
    LineIdea pianoTake (int approach, const std::vector<ChordSpan>& spans, double beatsPerBar, const TakeOptions& o)
    {
        approach = juce::jlimit (0, pianoApproaches().size() - 1, approach);
        static const char* descriptions[] = {
            "The whole chord, voiced to move as little as it can, in a rhythm of the take's own",
            "The root held low, the chord's notes broken above it in an order of the take's own",
            "The root low and held, the chord rising and falling above it",
            "A low bass note on one and three, the chord on two and four",
            "Left-hand octaves, the chord pulsing above them",
            "Eighth-note chords with accents and rests of the take's own, over the root" };
        LineIdea idea { LineIdea::Part::chords, pianoApproaches()[approach], descriptions[approach], {} };
        if (spans.empty() || beatsPerBar <= 0.0)
            return idea;
        juce::Random r ((juce::int64) o.seed * 2654435761LL + approach * 1931 + 71);
        const auto busy = juce::jlimit (0.0f, 1.0f, o.busy);
        const auto n = juce::jmax (1, (int) std::lround (beatsPerBar * 4.0));

        // The right hand: voiced from a low note of the take's, the first voicing its own, then the least movement.
        const int lows[] = { 52, 55, 57, 60, 62 };
        const auto low = lows[r.nextInt (5)];
        std::vector<std::vector<int>> right;
        {
            std::vector<int> previous;
            for (const auto& s : spans)
            {
                const auto voicings = voicingsFor (s.chord, low);
                std::vector<int> notes { 60 };
                if (! voicings.empty())
                    notes = voicings[(size_t) (previous.empty() ? r.nextInt ((int) voicings.size()) : closestVoicing (voicings, previous, {}))];
                std::sort (notes.begin(), notes.end());
                right.push_back (notes);
                previous = notes;
            }
        }
        std::vector<int> lefts;
        {
            const double centres[] = { 38.0, 41.0, 44.0 };
            const auto centre = centres[r.nextInt (3)];
            double prev = centre;
            for (const auto& s : spans)
            {
                const auto note = nearestPitch (bassOf (s.chord), (prev * 2.0 + centre) / 3.0, 33, 52);
                lefts.push_back (note);
                prev = note;
            }
        }
        const auto fifthAbove = [&] (size_t s)
        {
            const auto tones = tonesOf (spans[s].chord);
            for (const auto st : { 7, 6, 8 })
                if (has (tones, pcOf (lefts[s] + st)))
                    return lefts[s] + st;
            return lefts[s] + 7;
        };
        // The ballad's notes above the left hand: fifth, octave, tenth, the fifth above (and the ninth, per take).
        const auto withNinth = chance (r, 0.35);
        const auto arpNotes = [&] (size_t s)
        {
            const auto root = lefts[s];
            const auto tones = tonesOf (spans[s].chord);
            std::vector<int> up { fifthAbove (s), root + 12 };
            up.push_back (nearestPitch (tones.size() > 1 ? tones[1] : tones[0], root + 16, root + 13, root + 24));
            up.push_back (fifthAbove (s) + 12);
            if (withNinth && has (spans[s].key.scale(), pcOf (root + 2)))
                up.push_back (root + 14);
            std::sort (up.begin(), up.end());
            up.erase (std::unique (up.begin(), up.end()), up.end());
            return up;
        };

        Cell a;
        const auto chordRole = [] (juce::Random&) { return Role::chord; };
        std::function<Role (juce::Random&)> otherRole = chordRole;
        auto canMove = true;
        const auto leftHalf = chance (r, 0.4);
        switch (approach)
        {
            case 0:   // Block chords
            {
                const auto steps = drawSteps (r, n, countFor (r, busy, 1, 1.6, 5),
                                              [n] (int s) { return metric (s) + (s == n - 2 ? 1.5f : 0.0f); }, { 0 });
                for (const auto st : steps)
                    a.push_back ({ st, Role::chord, 0, st == 0 ? 0.78f : 0.7f, 1.0f });
                a.push_back ({ 0, Role::leftRoot, 0, 0.8f, 1.0f });
                if (leftHalf && n >= 8)
                    a.push_back ({ n / 2, Role::leftRoot, 0, 0.72f, 1.0f });
                break;
            }
            case 1:   // Broken chords
            case 2:   // Ballad arpeggio
            {
                const auto grid = busy < 0.3f ? 4 : 2;
                static const std::vector<std::vector<int>> broken { { 0, 9, 1, 9 }, { 0, 1, 9, 1 }, { 0, 1, 2, 9 }, { 9, 2, 1, 0 }, { 0, 9, 1, 9, 2, 9 }, { 0, 2, 1, 9 } };
                static const std::vector<std::vector<int>> waves { { 0, 1, 2, 3, 2, 1 }, { 0, 1, 2, 3 }, { 0, 2, 1, 3, 2, 1 }, { 0, 1, 2, 3, 4, 3, 2, 1 }, { 1, 0, 2, 1, 3, 2 } };
                const auto& shapes = approach == 1 ? broken : waves;
                const auto& shape = shapes[(size_t) r.nextInt ((int) shapes.size())];
                auto i = 0;
                for (auto st = approach == 2 ? grid : 0; st < n; st += grid, ++i)
                    a.push_back ({ st, Role::arp, shape[(size_t) i % shape.size()], i == 0 ? 0.78f : 0.64f, 1.0f });
                if (busy > 0.75f)
                    for (auto st = n - 3; st < n; st += 2)
                        if (st > 0)
                            a.push_back ({ st, Role::arp, shape[(size_t) (i++) % shape.size()], 0.58f, 1.0f });
                a.push_back ({ 0, Role::leftRoot, 0, approach == 2 ? 0.82f : 0.76f, 1.0f });
                if (leftHalf && approach == 1 && n >= 8)
                    a.push_back ({ n / 2, Role::leftFifth, 0, 0.68f, 1.0f });
                a = sorted (a);
                otherRole = [] (juce::Random& rr) { return rr.nextInt (4) == 0 ? Role::chord : Role::arp; };
                canMove = false;
                break;
            }
            case 3:   // Stride
            {
                const auto second = r.nextInt (4);   // the bass on three: the fifth, the low root, the root again, the fifth low
                for (auto st = 0; st < n; st += 4)
                {
                    const auto onBass = (st / 4) % 2 == 0;
                    if (onBass)
                        a.push_back ({ st, st % 8 == 0 && st == 0 ? Role::leftRoot : (second == 0 ? Role::leftFifth : second == 1 ? Role::leftLow : second == 2 ? Role::leftRoot : Role::leftFifth),
                                       0, 0.82f, 0.9f });
                    else
                        a.push_back ({ st, Role::chord, 0, 0.7f, 0.8f });
                }
                if (busy > 0.6f && n >= 8 && chance (r, 0.4 + (busy - 0.6) * 1.5))
                    a.push_back ({ n - 2, Role::chord, 0, 0.62f, 0.8f });
                if (busy < 0.3f)
                    a.erase (std::remove_if (a.begin(), a.end(), [n] (const Hit& h) { return h.role == Role::chord && h.step == n - 4 && n > 8; }), a.end());
                a = sorted (a);
                canMove = false;
                break;
            }
            case 4:   // Octaves and chords
            {
                const auto steps = drawSteps (r, n, countFor (r, busy, 2, 4, 8), metric, { 0 });
                for (const auto st : steps)
                    a.push_back ({ st, Role::chord, 0, st % 8 == 0 ? 0.78f : 0.66f, 0.9f });
                a.push_back ({ 0, Role::leftOctaves, 0, 0.85f, 1.0f });
                if (leftHalf && n >= 8)
                    a.push_back ({ n / 2, Role::leftOctaves, 0, 0.76f, 1.0f });
                a = sorted (a);
                break;
            }
            default:  // Pop pulse
            {
                std::vector<int> steps;
                for (auto st = 0; st < n; st += 2)
                    steps.push_back (st);
                if (busy < 0.6f)
                    for (auto k = (int) std::lround ((0.6f - busy) / 0.6f * 4.0f); k > 0 && steps.size() > 2; --k)
                        steps.erase (steps.begin() + 1 + r.nextInt ((int) steps.size() - 1));
                if (busy > 0.7f)
                    steps = drawSteps (r, n, (int) steps.size() + (int) std::lround ((busy - 0.7f) / 0.3f * 3.0f),
                                       [] (int st) { return st % 2 == 1 ? 1.0f : 0.0f; }, steps);
                static const std::vector<std::vector<int>> accents { { 0, 6, 12 }, { 0, 8 }, { 0, 4, 8, 12 }, { 0, 6, 10 }, { 0, 10 } };
                const auto& accent = accents[(size_t) r.nextInt ((int) accents.size())];
                for (const auto st : steps)
                    a.push_back ({ st, Role::chord, 0, has (accent, st) ? 0.76f : 0.56f, 0.9f });
                a.push_back ({ 0, Role::leftRoot, 0, 0.8f, 1.0f });
                a = sorted (a);
                canMove = false;
                break;
            }
        }
        const auto b = varied (a, r, n, otherRole, canMove);
        // The fill: a push into the next chord (the next chord a sixteenth or eighth early), per take.
        auto f = a;
        if (busy >= 0.15f && n >= 8 && chance (r, 0.7))
        {
            const auto at = n - (r.nextBool() ? 2 : 1);
            f.erase (std::remove_if (f.begin(), f.end(), [at] (const Hit& h) { return ! leftHand (h.role) && h.step >= at; }), f.end());
            f.push_back ({ at, Role::nextChord, 0, 0.74f, 1.0f });
            f = sorted (f);
        }
        const auto events = realise (spans, beatsPerBar, a, b, f, chance (r, 0.6));
        const auto nextOf = [&] (size_t s) { return s + 1 < spans.size() ? s + 1 : (size_t) 0; };
        for (size_t i = 0; i < events.size(); ++i)
        {
            const auto& e = events[i];
            const auto s = e.span;
            const auto length = lengthOf (events, i, spans);
            switch (e.hit.role)
            {
                case Role::leftRoot: add (idea.notes, e.at, length, lefts[s], e.hit.velocity); break;
                case Role::leftFifth: add (idea.notes, e.at, length, fifthAbove (s), e.hit.velocity); break;
                case Role::leftLow: add (idea.notes, e.at, length, lefts[s] - 12 >= 28 ? lefts[s] - 12 : lefts[s], e.hit.velocity); break;
                case Role::leftOctaves:
                    add (idea.notes, e.at, length, lefts[s], e.hit.velocity);
                    add (idea.notes, e.at, length, lefts[s] + 12, e.hit.velocity - 0.05f);
                    break;
                case Role::nextChord:
                    for (const auto p : right[nextOf (s)])
                        add (idea.notes, e.at, length, p, e.hit.velocity);
                    break;
                case Role::arp:
                {
                    if (approach == 2)
                    {
                        const auto up = arpNotes (s);
                        add (idea.notes, e.at, juce::jmin (length, 1.0), up[(size_t) juce::jlimit (0, (int) up.size() - 1, e.hit.index)], e.hit.velocity);
                    }
                    else
                    {
                        const auto& v = right[s];
                        const auto index = e.hit.index == 9 ? (int) v.size() - 1 : juce::jlimit (0, (int) v.size() - 1, e.hit.index);
                        add (idea.notes, e.at, juce::jmin (length, 1.0), v[(size_t) index], e.hit.velocity);
                    }
                    break;
                }
                default:
                    for (const auto p : right[s])
                        add (idea.notes, e.at, length, p, e.hit.velocity);
                    break;
            }
        }
        return idea;
    }

    //==========================================================================
    void riffTake (LineIdea& idea, const std::vector<ChordSpan>& spans, double beatsPerBar, const TakeOptions& o, GuitarStyle style)
    {
        if (spans.empty() || idea.notes.empty() || beatsPerBar <= 0.0)
            return;
        const auto origin = spans.front().start;
        const auto metal = style == GuitarStyle::thrash || style == GuitarStyle::numetal || style == GuitarStyle::metalcore;
        // The choices at a place of the bar (the fill bars their own): the same for every bar.
        const auto decide = [&] (int bar, int step, int salt)
        {
            juce::Random r ((juce::int64) o.seed * 2654435761LL + (bar % 4 == 3 ? 7919 : 0) + step * 131 + salt * 104729);
            return r.nextDouble();
        };
        const auto spanAt = [&] (double beat)
        {
            for (size_t s = 0; s < spans.size(); ++s)
                if (beat + origin < spans[s].end - 1.0e-9)
                    return s;
            return spans.size() - 1;
        };
        auto& notes = idea.notes;
        std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        for (size_t i = 0; i < notes.size();)
        {
            auto j = i + 1;
            while (j < notes.size() && std::abs (notes[j].beat - notes[i].beat) < 1.0e-4)
                ++j;
            const auto single = j == i + 1;
            auto& note = notes[i];
            const auto s = spanAt (note.beat);
            const auto barStart = spans[s].start - origin + std::floor ((note.beat - (spans[s].start - origin)) / beatsPerBar + 1.0e-6) * beatsPerBar;
            const auto bar = (int) std::floor (barStart / beatsPerBar + 1.0e-6);
            const auto pos = (note.beat - barStart) / 0.25;
            const auto step = (int) std::lround (pos);
            const auto midi = (int) std::lround (note.velocity * 127.0f);
            const auto muted = midi <= 59, special = midi >= 120;
            if (single && std::abs (pos - step) < 1.0e-3 && step != 0 && ! special)
            {
                const auto root = spans[s].chord.root;
                if (pcOf (note.pitch) == pcOf (root) && decide (bar, step, 1) < 0.22)
                {
                    const auto pick = decide (bar, step, 2);
                    int moved;
                    if (metal)
                        moved = pick < 0.35 ? note.pitch + 12 : pick < 0.6 ? note.pitch + 1 : pick < 0.8 ? note.pitch + 3 : note.pitch + 7;
                    else
                        moved = pick < 0.5 ? note.pitch + 12 : note.pitch + 7;
                    if (moved <= o.highest)
                        note.pitch = moved;
                }
                if (step % 2 == 0 && decide (bar, step, 3) < 0.12)
                    note.velocity = muted ? 96.0f / 127.0f : 44.0f / 127.0f;
            }
            if (o.lowString && ! special)
            {
                auto lowestNote = 127;
                for (auto k = i; k < j; ++k)
                    lowestNote = juce::jmin (lowestNote, notes[k].pitch);
                while (lowestNote > o.lowest + 12 && lowestNote - 12 >= o.lowest)
                {
                    for (auto k = i; k < j; ++k)
                        notes[k].pitch -= 12;
                    lowestNote -= 12;
                }
            }
            i = j;
        }
    }

    void thinTake (LineIdea& idea, double beatsPerBar, const TakeOptions& o)
    {
        if (o.busy >= 0.5f || idea.notes.size() < 3 || beatsPerBar <= 0.0)
            return;
        const auto p = (0.5f - o.busy) * 1.6f;
        auto& notes = idea.notes;
        std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        std::vector<LineNote> kept;
        for (const auto& n : notes)
        {
            const auto inBar = n.beat - std::floor (n.beat / beatsPerBar + 1.0e-6) * beatsPerBar;
            const auto onBeat = std::abs (inBar - std::round (inBar)) < 1.0e-3;
            juce::Random r ((juce::int64) o.seed * 2654435761LL + (juce::int64) std::llround (inBar * 960.0) * 31 + 5);
            if (! kept.empty() && ! onBeat && r.nextFloat() < p && n.beat > kept.back().beat + 1.0e-4)
            {
                kept.back().length = juce::jmax (kept.back().length, n.beat + n.length - kept.back().beat);
                continue;
            }
            kept.push_back (n);
        }
        notes = std::move (kept);
    }
}
