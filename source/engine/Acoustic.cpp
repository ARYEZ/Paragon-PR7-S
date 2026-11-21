#include "engine/Acoustic.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace pr7s
{
    const juce::StringArray& acousticGuitarStyles()
    {
        // (0.193.0) Strums, then picking, then the two that follow the guitar lane's riff. Only append.
        static const juce::StringArray names {
            "Big strums on one and three, light tops between", "Growing fifths strum", "Split strum, fixed high triad",
            "Bass-string strum, tied every other bar", "Bass note and strum", "Strum, change pushed an eighth",
            "Sixteenth strum, change pushed", "Strum off the one", "Chord to open pedal", "Three-bar strum cycle, high shapes",
            "Embellished strum", "Strum with flicks: quarters", "Strum with flicks: eighths", "Strum with flicks: light ups",
            "Skipping strum", "Acoustic power eighths", "Lilting strum in threes", "Strummed triads in sixes",
            "High rolled triads, strum, breathe, pick", "Offbeat high strums",
            "Fingerpicked eighths over a held bass", "Falling cascade over the bass", "Bass hammer-on, drone pair above",
            "Add9 arpeggio", "Nylon two-bar picking", "Nylon arpeggio under a fixed top", "Arch arpeggio over the fifth",
            "Fixed-top fingerpicking", "Held bass, arch on top", "One-bar fingerpick, one bar of air",
            "Fixed high figure, walk on the fourth bar", "Stab cell, a bar on, a bar off", "Root chord and high cluster in turn",
            "Strummed tonic on the riff's accents", "Strum under the riff" };
        return names;
    }

    const juce::StringArray& secondAcousticParts()
    {
        static const juce::StringArray names { "LOOSER COPY", "HIGH PEDAL SEESAW", "HIGH ANSWERS OVER A RINGING STRING", "HIGH THIRDS",
                                               "AS THE BASS", "FILLS ONLY", "TWIN ARPEGGIO" };
        return names;
    }

    namespace
    {
        int pc12 (int p) { return ((p % 12) + 12) % 12; }

        int pitchIn (int pc, int low, int high)
        {
            for (auto p = low; p <= high; ++p)
                if (pc12 (p) == pc12 (pc))
                    return p;
            return low;
        }

        int thirdOf (const Chord& chord)
        {
            const auto iv = chord.intervals();
            return iv.size() > 1 ? iv[1] : 4;
        }

        int fifthOf (const Chord& chord)
        {
            for (const auto i : chord.intervals())
                if (i >= 6 && i <= 8)
                    return i;
            return 7;
        }

        int scaleStep (const Key& key, int pitch, int steps)
        {
            auto scale = key.scale();
            std::sort (scale.begin(), scale.end());
            const auto pc = pc12 (pitch);
            const auto at = std::find (scale.begin(), scale.end(), pc);
            if (at == scale.end())
                return pitch + steps * 2;   // (outside the key: whole steps)
            const auto index = (int) (at - scale.begin()) + steps;
            const auto octave = (int) std::floor ((double) index / 7.0);
            return pitch - pc + scale[(size_t) (index - octave * 7)] + 12 * octave;
        }

        // The key's seventh above a root (a semitone count; outside the key: a minor seventh).
        int diatonicSeventh (const Key& key, int rootPc)
        {
            const auto r = 48 + pc12 (rootPc);
            const auto s = scaleStep (key, r, 6);
            return s - r == 12 ? 10 : juce::jlimit (9, 11, s - r);
        }

        bool openStringClass (int pc) { return pc == 4 || pc == 9 || pc == 2 || pc == 7 || pc == 11; }

        Chord tonicChord (const Key& key)
        {
            Chord c;
            c.root = key.pitchClass();
            c.type = key.minor ? Chord::Type::minor : Chord::Type::major;
            return c;
        }
    }

    namespace AcousticVoicing
    {
        int root (const Chord& chord) { return pitchIn (chord.bass >= 0 ? chord.bass : chord.root, lowest, lowest + 11); }

        std::vector<int> open (const Chord& chord, bool colour)
        {
            const auto base = pitchIn (chord.root, lowest, lowest + 11);
            const auto third = thirdOf (chord), fifth = fifthOf (chord);
            std::vector<int> v { base, base + fifth, base + 12, base + 12 + third, base + 12 + fifth, base + 24 };
            // The chord's own tensions in place of the top octave.
            std::vector<int> tensions;
            for (const auto i : chord.intervals())
                if (i != 0 && i != third && i != fifth)
                    tensions.push_back (i % 12);
            if (! tensions.empty())
                v.back() = base + 12 + tensions.front() + (tensions.front() < third ? 12 : 0);
            else if (colour && (chord.type == Chord::Type::major || chord.type == Chord::Type::minor))
            {
                // An open string's colour where it fits: the ninth; on a major chord a major seventh or a sixth.
                if (openStringClass (chord.root + 2))
                    v.back() = base + 26 <= highest ? base + 26 : base + 14;
                else if (chord.type == Chord::Type::major && openStringClass (chord.root + 11))
                    v.back() = base + 23;
                else if (chord.type == Chord::Type::major && openStringClass (chord.root + 9))
                    v.back() = base + 21;
            }
            if (chord.bass >= 0 && pc12 (chord.bass) != pc12 (chord.root))
            {
                const auto bass = pitchIn (chord.bass, lowest, lowest + 11);
                v.front() = bass < base + fifth ? bass : bass - 12;
                if (v.front() < lowest)
                    v.front() += 12;
            }
            v.erase (std::remove_if (v.begin(), v.end(), [] (int p) { return p > highest || p < lowest; }), v.end());
            std::sort (v.begin(), v.end());
            v.erase (std::unique (v.begin(), v.end()), v.end());
            return v;
        }

        std::vector<int> power (const Chord& chord, int notes)
        {
            const auto base = root (chord);
            std::vector<int> v { base, base + 7, base + 12, base + 19, base + 24 };
            v.resize ((size_t) juce::jlimit (2, 5, notes));
            v.erase (std::remove_if (v.begin(), v.end(), [] (int p) { return p > highest; }), v.end());
            return v;
        }

        std::vector<int> high (const Chord& chord)
        {
            const auto pcs = chord.pitchClasses();
            const auto has = [&pcs] (int p) { return std::find (pcs.begin(), pcs.end(), pc12 (p)) != pcs.end(); };
            auto top = highest;
            while (top >= 69 && ! has (top))
                --top;
            if (top < 69)
                top = pitchIn (chord.root, 65, 76);
            std::vector<int> v { top };
            for (auto p = top - 1; p >= lowest && v.size() < 5; --p)
                if (has (p))
                    v.push_back (p);
            std::sort (v.begin(), v.end());
            return v;
        }

        std::vector<int> triad (const Chord& chord, int low)
        {
            const auto third = thirdOf (chord), fifth = fifthOf (chord);
            const int pcs[] { pc12 (chord.root), pc12 (chord.root + third), pc12 (chord.root + fifth) };
            std::vector<int> v;
            for (auto p = low; v.size() < 3 && p < low + 24; ++p)
                if (std::find (std::begin (pcs), std::end (pcs), pc12 (p)) != std::end (pcs))
                    v.push_back (p);
            return v;
        }
    }

    namespace
    {
        using namespace AcousticVoicing;

        struct Event
        {
            double at = 0.0;
            std::vector<int> pitches;
            float velocity = 0.8f;
            bool down = true, pick = false;
            double length = 0.0;     // 0: rings as the rules say
            double spreadMs = -1.0;  // -1: by the note count
        };

        struct Writer
        {
            const std::vector<ChordSpan>& spans;
            const AcousticRequest& r;
            double origin, length, bpb, s16, e8;
            int bars;
            Key key;
            std::vector<Event> main, second;

            Writer (const std::vector<ChordSpan>& sp, const AcousticRequest& rq)
                : spans (sp), r (rq), origin (sp.front().start), length (sp.back().end - sp.front().start), bpb (rq.beatsPerBar),
                  s16 (rq.beatsPerBar / 16.0), e8 (rq.beatsPerBar / 8.0),
                  bars (juce::jmax (1, (int) std::ceil ((sp.back().end - sp.front().start) / rq.beatsPerBar - 1.0e-6))), key (sp.front().key) {}

            const ChordSpan& spanAt (double t) const
            {
                for (const auto& s : spans)
                    if (origin + t >= s.start - 1.0e-9 && origin + t < s.end - 1.0e-9)
                        return s;
                return t < 0.0 ? spans.front() : spans.back();
            }
            const Chord& chordAt (double t) const { return spanAt (juce::jlimit (0.0, juce::jmax (0.0, length - 1.0e-6), t)).chord; }
            const Chord& chordOfBar (int bar) const { return chordAt (juce::jlimit (0, bars - 1, bar) * bpb); }
            Chord chordAfter (int bar) const
            {
                if (bar + 1 < bars)
                    return chordOfBar (bar + 1);
                return r.nextChord.value_or (spans.front().chord);
            }
            double chordEnd (double t) const { return spanAt (t).end - origin; }
            bool colour() const { return r.variation % 3 != 2; }
            std::vector<int> full (double t) const { return open (chordAt (t), colour()); }
            static std::vector<int> lowHalf (std::vector<int> v) { v.resize (juce::jmin ((size_t) 3, v.size())); return v; }
            static std::vector<int> top (const std::vector<int>& v, int n)
            {
                return std::vector<int> (v.end() - juce::jmin ((int) v.size(), n), v.end());
            }
            int tonicIn (int low, int high) const { return pitchIn (key.pitchClass(), low, high); }
            int keyNote (int degree, int low, int high) const { return pitchIn (key.scale()[(size_t) degree], low, high); }

            static float positional (int k) { return k % 4 == 0 ? 0.79f : k % 4 == 2 ? 0.63f : 0.51f; }
            static bool down (int k, bool feel16) { return k % 4 == 0 || (feel16 && k % 4 == 2); }

            void strum (std::vector<Event>& list, double at, std::vector<int> pitches, float velocity, bool isDown, double len = 0.0,
                        double spread = -1.0)
            {
                if (at < -1.0e-9 || at >= length - 1.0e-6 || pitches.empty())
                    return;
                list.push_back ({ at, std::move (pitches), velocity, isDown, false, len, spread });
            }
            void pick (std::vector<Event>& list, double at, int pitch, float velocity, double len = 0.0)
            {
                if (at < -1.0e-9 || at >= length - 1.0e-6)
                    return;
                list.push_back ({ at, { juce::jlimit (lowest, highest, pitch) }, velocity, true, true, len, 0.0 });
            }
            template <typename F> void each16 (const char* p, int bar, F&& f) const
            {
                for (int k = 0; k < 16; ++k)
                    if (p[k] != '-' && p[k] != '=')
                        f (p[k], k, bar * bpb + k * s16);
            }

            void render (std::vector<Event>& list, bool harmony, LineIdea& out) const
            {
                std::stable_sort (list.begin(), list.end(), [] (const Event& a, const Event& b) { return a.at < b.at; });
                const auto msToBeats = r.bpm / 60000.0;
                for (size_t i = 0; i < list.size(); ++i)
                {
                    const auto& ev = list[i];
                    auto end = ev.at + bpb;   // (a bar at most)
                    if (ev.length > 0.0)
                        end = ev.at + ev.length;
                    else
                        for (auto j = i + 1; j < list.size(); ++j)
                        {
                            const auto& next = list[j];
                            if (next.at <= ev.at + 1.0e-6)
                                continue;
                            const auto samePitch = std::find (next.pitches.begin(), next.pitches.end(), ev.pitches.front()) != next.pitches.end();
                            if (! ev.pick || ! next.pick || samePitch)
                            {
                                end = juce::jmin (end, next.at);   // (cut at the next strum's start)
                                break;
                            }
                        }
                    if (ev.pick && ev.length <= 0.0)
                        end = juce::jmin (end, chordEnd (ev.at));   // (picked notes ring until the chord changes)
                    end = juce::jmin (end, length);
                    auto pitches = ev.pitches;
                    std::sort (pitches.begin(), pitches.end());
                    if (! ev.down)
                        std::reverse (pitches.begin(), pitches.end());
                    const auto n = (int) pitches.size();
                    const auto spread = ev.spreadMs >= 0.0 ? ev.spreadMs : n <= 1 ? 0.0 : juce::jlimit (5.0, 15.0, 2.5 * (n - 1));
                    for (int m = 0; m < n; ++m)
                    {
                        const auto start = ev.at + (n > 1 ? spread * msToBeats * m / (n - 1) : 0.0);
                        if (start >= length - 1.0e-6)
                            continue;
                        LineNote note { start, juce::jmax (0.01, end - start - 0.001), pitches[(size_t) m], juce::jlimit (0.05f, 1.0f, ev.velocity) };
                        note.harmony = harmony;
                        out.notes.push_back (note);
                    }
                }
            }

            //==================================================================
            juce::String styleOf (int style)
            {
                const auto every = [this] (auto&& f) { for (int b = 0; b < bars; ++b) f (b); };
                switch (style)
                {
                    case 0:
                        every ([&] (int b)
                        {
                            if (b % 4 == 3)
                            {
                                strum (main, b * bpb, full (b * bpb), 0.7f, true, bpb * 0.98, 15.0);
                                return;
                            }
                            each16 ("X---o-o-X---o-o-", b, [&] (char ch, int k, double t)
                            {
                                if (ch == 'X')
                                    strum (main, t, full (t), 0.7f, true, 0.0, 15.0);
                                else
                                    strum (main, t, top (full (t), 3), 0.33f, k % 4 != 2, e8);
                            });
                        });
                        return "Full chords from the low root on 1 and 3, spread slowly; the top three strings softly on 2, 2&, 4 and 4&; every fourth bar one chord on 1, held";
                    case 1:
                        every ([&] (int b)
                        {
                            const auto& chord = chordOfBar (b);
                            const auto five = power (chord, 5);
                            if (bars >= 8 && b == bars - 1)
                            {
                                const auto next = chordAfter (b);
                                for (int beat = 0; beat < 4; ++beat)
                                    strum (main, b * bpb + beat * 4 * s16, power (beat < 2 ? chord : next, 5), 0.79f, true);
                                return;
                            }
                            if (b > 0 && ! (chord == chordOfBar (b - 1)))
                            {
                                each16 ("--x===x-x-x-x-x-", b, [&] (char, int k, double t) { strum (main, t, five, k == 2 ? 0.79f : positional (k), down (k, false)); });
                                return;
                            }
                            const auto phase = b % 4;
                            for (int k = 0; k < 16; k += 2)
                            {
                                const auto t = b * bpb + k * s16;
                                auto notes = (phase == 0 || phase == 2) && k < 8 ? power (chord, 3) : five;
                                if (phase == 1 && k < 8 && notes.size() == 5)
                                {
                                    const auto base = notes.front();
                                    notes.back() = chord.type == Chord::Type::minor ? (base + 26 <= highest ? base + 26 : base + 14) : base + 23;
                                    std::sort (notes.begin(), notes.end());
                                }
                                strum (main, t, notes, positional (k), down (k, false));
                            }
                        });
                        return "Every eighth strummed on a fifth stack (root, fifth, octave, no third), the ands lighter; the low three strings for half a bar, then all five; a major seventh (minor: a ninth) on top in the second bar; a new chord's downbeat left empty";
                    case 2:
                        every ([&] (int b)
                        {
                            const auto& chord = chordOfBar (b);
                            auto triadNotes = triad (tonicChord (key), 55);
                            const auto clash = std::any_of (triadNotes.begin(), triadNotes.end(), [&] (int p)
                                               { const auto d = pc12 (p - chord.root); return d == 1 || d == 11; });
                            if (clash)
                                for (auto& p : triadNotes)
                                    p = scaleStep (key, p, 1);
                            auto withSeventh = open (chord, false);
                            if (withSeventh.size() >= 5)
                            {
                                withSeventh.back() = withSeventh.front() + 12 + diatonicSeventh (key, chord.root);
                                std::sort (withSeventh.begin(), withSeventh.end());
                            }
                            const auto low = power (chord, 3);
                            const auto hold = b % 4 == 3;
                            each16 (hold ? "x---x-x---x-x---" : "x---x-x---x-x-x-", b, [&] (char, int k, double t)
                            {
                                if (k == 0 || k == 4)
                                    strum (main, t, low, 0.79f, true);
                                else if (k == 12)
                                    strum (main, t, withSeventh, 0.79f, true, hold ? 4 * s16 : 0.0);
                                else
                                    strum (main, t, triadNotes, 0.63f, false);
                            });
                        });
                        return "Split: the low strings on 1 and 2, the key's own triad high on 2&, 3& and 4& over every chord (a step up where it would rub), a five-string strum with the seventh on 4";
                    case 3:
                        every ([&] (int b)
                        {
                            const auto p = b % 8 == 7 ? "l-l-l-l-X-X-X-X-" : b % 2 == 0 ? "l-l-l-l-X---l-X-" : "--l-l-X---l-X-X-";
                            each16 (p, b, [&] (char ch, int k, double t)
                            {
                                const auto& chord = chordAt (t);
                                strum (main, t, ch == 'X' ? power (chord, 5) : power (chord, 3), ch == 'X' ? positional (k) + 0.08f : positional (k), down (k, false));
                            });
                        });
                        return "The three lowest strings (root, fifth, octave) in eighths, a five-string strum without the third on 3 and the and of 4; every other bar the last strum tied over the bar line; the eighth bar of a phrase all eighths";
                    case 4:
                    {
                        const auto strums = r.variation % 2 == 1;
                        every ([&] (int b)
                        {
                            each16 (strums ? "B---x---x-x-x-x-" : "B---s-s-b-s-s-s-", b, [&] (char ch, int k, double t)
                            {
                                const auto shape = full (t);
                                if (ch == 'B')
                                    pick (main, t, shape.front(), 0.79f, strums ? 4 * s16 : 8 * s16);
                                else if (ch == 'b')
                                    pick (main, t, shape.size() > 1 ? shape[1] : shape.front(), 0.7f, 8 * s16);
                                else if (ch == 's')
                                    strum (main, t, top (shape, 3), positional (k) * 0.9f, down (k, false));
                                else
                                    strum (main, t, shape, positional (k), down (k, false));
                            });
                        });
                        return strums ? juce::String ("The bass note alone on 1, then full strums on 2, 3, 3&, 4 and 4&")
                                      : juce::String ("The bass note alone on 1, the fifth on 3, the upper strings strummed on the other eighths");
                    }
                    case 5:
                    {
                        const auto pushed = [&] (int b) { return b >= 0 && b + 1 < bars && ! (chordOfBar (b + 1) == chordOfBar (b)); };
                        every ([&] (int b)
                        {
                            each16 (pushed (b - 1) ? "--x-x-x---x-x-x-" : "x-x-x-x---x-x-x-", b, [&] (char, int k, double t)
                            {
                                if (k == 14 && pushed (b))
                                    strum (main, t, open (chordOfBar (b + 1), colour()), 0.79f, true);
                                else
                                    strum (main, t, power (chordAt (t), 3), positional (k), down (k, false));
                            });
                        });
                        return "Root, fifth and octave strummed x-x-x-x---x-x-x- (the and of 2 rung over 3); the next chord a full open chord on the bar's last eighth, tied over, the next bar starting on its and";
                    }
                    case 6:
                        every ([&] (int b)
                        {
                            if (b % 2 == 0)
                                each16 ("x---x-xx-xxx-x-x", b, [&] (char, int k, double t)
                                {
                                    const auto chord = k >= 9 ? chordAfter (b) : chordOfBar (b);
                                    strum (main, t, open (chord, colour()), k == 12 ? 0.87f : positional (k), down (k, true));
                                });
                            else
                                each16 ("--o-X-oXo-X-X-o-", b, [&] (char ch, int k, double t)
                                {
                                    const auto shape = full (t);
                                    if (ch == 'X')
                                        strum (main, t, shape, positional (k) + 0.08f, down (k, true));
                                    else
                                        strum (main, t, top (shape, 3), 0.4f, down (k, true));
                                });
                        });
                        return "A sixteenth strum, the next chord pushed onto the e of 3 and struck full on 4; then full strums on 2, the a of 2, 3& and 4 with light ones between";
                    case 7:
                        every ([&] (int b)
                        {
                            each16 ("-x-x-x-x-x---x-x", b, [&] (char, int k, double t)
                            { strum (main, t, open (chordAt (t + bpb * 0.5), colour()), 0.7f, k % 4 == 1); });
                        });
                        return "Full open chords on the sixteenths off the beat, nothing on the downbeat; each chord comes in on beat 3, half a bar early";
                    case 8:
                    case 34:
                    {
                        const auto under = style == 34;
                        const auto pedal = tonicIn (lowest, lowest + 11);
                        every ([&] (int b)
                        {
                            if (under && b % 4 == 3)
                                return;   // (the riff alone)
                            const auto scale = under ? 0.8f : 1.0f;
                            each16 ("x---x-x---x-x-xx", b, [&] (char, int k, double t)
                            {
                                const auto shape = full (b * bpb);
                                std::vector<int> pedalShape { pedal };
                                for (auto it = shape.rbegin(); it != shape.rend() && pedalShape.size() < 5; ++it)
                                    if (*it > pedal + 4)
                                        pedalShape.push_back (*it);
                                if (k == 0)
                                    strum (main, t, shape, 0.79f * scale, true);
                                else if (k == 4)
                                    strum (main, t, lowHalf (shape), 0.79f * scale, true);
                                else if (k == 15)
                                    strum (main, t, top (pedalShape, 2), 0.5f * scale, false);
                                else
                                    strum (main, t, pedalShape, positional (k) * scale, down (k, true));
                            });
                        });
                        return under ? juce::String ("The chord-to-open-pedal strum kept low under the guitar's riff, every fourth bar left to the riff alone")
                                     : juce::String ("The chord full on 1 and its low half on 2; on 2& the bass string drops to the open tonic under the upper notes, strummed on 3&, 4 and 4&, the top two strings on the last sixteenth");
                    }
                    case 9:
                        every ([&] (int b)
                        {
                            const auto c = b % 3;
                            if (c == 0 && b > 0)
                                strum (main, b * bpb - s16, high (chordOfBar (b)), 0.63f, false);
                            const auto p = c == 0 ? "--x-x-xxx-xx-xx-" : c == 1 ? "x---x-xxx---x-xx" : "x---x-xxxxxx-xx-";
                            each16 (p, b, [&] (char, int k, double t)
                            {
                                const auto chord = c == 2 && k >= 14 ? chordAfter (b) : chordAt (t);
                                strum (main, t, high (chord), positional (k), down (k, true));
                            });
                        });
                        return "A three-bar cycle of busy sixteenths on high five-string shapes, gaps in each bar; the first struck a sixteenth before its bar and tied, the third ending on the next chord";
                    case 10:
                        every ([&] (int b)
                        {
                            if (b % 2 == 0)
                                each16 ("x---x-ttx-x-ttx-", b, [&] (char ch, int k, double t)
                                {
                                    const auto shape = full (t);
                                    strum (main, t, ch == 't' ? top (shape, 2) : shape, ch == 't' ? 0.55f : (k == 0 ? 0.87f : positional (k)), down (k, true));
                                });
                            else
                                each16 ("--x-x-xxxxx-x-x-", b, [&] (char, int k, double t)
                                {
                                    const auto& chord = chordAt (t);
                                    auto shape = full (t);
                                    if (k == 7 || k == 8)
                                        for (auto& p : shape)
                                        {
                                            const auto d = pc12 (p - chord.root);
                                            if (d == 3 || d == 4)
                                                p += 5 - d;   // (the third up to the fourth)
                                            else if (d == 7)
                                                p += 2;       // (the fifth up to the sixth)
                                        }
                                    strum (main, t, shape, positional (k), down (k, true));
                                });
                        });
                        return "A two-bar figure on each chord: full strums with the top two strings flicked between; in the second bar the inner voices flick up (the third to the fourth, the fifth to the sixth) and back";
                    case 11:
                    case 12:
                    case 13:
                    {
                        const auto p = style == 11 ? "X---X-xuX---X-xu" : style == 12 ? "X-x-x-xuX-x-x-xu" : "X-o-X-ouX-o-X-ou";
                        const auto soft = style == 11 ? 0.8f : 1.0f;
                        every ([&] (int b)
                        {
                            each16 (p, b, [&] (char ch, int k, double t)
                            {
                                const auto shape = full (t);
                                if (ch == 'X')
                                    strum (main, t, shape, 0.79f * soft, true);
                                else if (ch == 'u')
                                    strum (main, t, top (shape, 3), 0.51f * soft, false);
                                else if (ch == 'o')
                                    strum (main, t, top (shape, 3), 0.36f, k == 6 || k == 14);
                                else
                                    strum (main, t, shape, 0.63f * soft, true);
                            });
                        });
                        return style == 11 ? juce::String ("Full strums on the quarters, a down-up sixteenth flick into 2 and into 4")
                             : style == 12 ? juce::String ("Strummed eighths, a down-up sixteenth flick into each half bar")
                                           : juce::String ("Full strums on the quarters, light upstrokes on the top strings on the ands, a sixteenth flick before 3 and before 1");
                    }
                    case 14:
                        every ([&] (int b)
                        { each16 ("X--X-XX--X-XX-XX", b, [&] (char, int k, double t) { strum (main, t, full (t), positional (k) + 0.05f, down (k, true)); }); });
                        return "Full chords on a skipping sixteenth rhythm, X--X-XX--X-XX-XX";
                    case 15:
                        every ([&] (int b)
                        { each16 ("x-x-x-x-x-x-x-x-", b, [&] (char, int k, double t) { strum (main, t, power (chordAt (t), 3), positional (k), down (k, false)); }); });
                        return "Root, fifth and octave on every eighth, changing where the chords do";
                    case 16:
                        every ([&] (int b)
                        {
                            for (const auto g : { 0, 3, 6, 9 })
                            {
                                const auto t = b * bpb + g * s16;
                                const auto shape = full (t);
                                strum (main, t, b % 2 == 1 && g == 0 ? top (shape, 2) : shape, 0.87f, true);
                                strum (main, t + s16, top (shape, 3), 0.45f, true);
                                strum (main, t + 2 * s16, shape, 0.6f, false);
                            }
                            const auto t = b * bpb + 12 * s16;
                            const auto shape = full (t);
                            strum (main, t, shape, 0.87f, true);
                            strum (main, t + s16, top (shape, 3), 0.45f, true);
                            strum (main, t + 2 * s16, shape, 0.6f, false);
                            strum (main, t + 3 * s16, shape, 0.7f, true);
                        });
                        return "A lilting strum in threes (3+3+3+3+4 sixteenths): each three a full accented down, a light one, a full up; every other bar opening on a high dyad";
                    case 17:
                        every ([&] (int b)
                        {
                            for (const auto [g, n] : { std::pair { 0, 6 }, std::pair { 6, 6 }, std::pair { 12, 4 } })
                            {
                                const char* cell = n == 6 ? "x-xxxx" : "x-xx";
                                auto j = 0;
                                for (int i = 0; i < n; ++i)
                                    if (cell[i] == 'x')
                                    {
                                        const auto k = g + i;
                                        const auto t = b * bpb + k * s16;
                                        const auto chord = b % 4 == 3 && k >= 8 ? chordAfter (b) : chordAt (t);
                                        strum (main, t, triad (chord, 55), i == 0 ? 0.79f : 0.55f, j++ % 2 == 0);
                                    }
                            }
                        });
                        return "Close three-note triads in six-sixteenth cells (x-xxxx), 6+6+4 across the bar; a chord a bar, the fourth bar two";
                    case 18:
                        every ([&] (int b)
                        {
                            const auto o = b * bpb;
                            const auto shape = top (high (chordOfBar (b)), 4);
                            switch (b % 4)
                            {
                                case 0:
                                    for (const auto k : { 0, 6, 10, 12 })
                                        strum (main, o + k * s16, top (high (chordAt (o + k * s16)), 4), k == 0 ? 0.7f : 0.55f, k % 4 == 0, 0.0, 15.0);
                                    break;
                                case 1: strum (main, o + 15 * s16, shape, 0.5f, false, 0.0, 15.0); break;
                                case 2:
                                    for (size_t i = 0; i < shape.size(); ++i)
                                        pick (main, o + (1 + 2 * (int) i) * s16, shape[i], 0.55f);
                                    break;
                                default:
                                {
                                    const auto root = pitchIn (chordOfBar (b).root, 55, 64);
                                    for (int k = 1; k < 16; k += 2)
                                        strum (main, o + k * s16, { root, root + 12 }, 0.5f, false, s16 * 1.8);
                                    break;
                                }
                            }
                        });
                        return "High triads on the top strings, a four-bar cycle: rolled strums, then a bar of breath with one upstroke, then the chord picked note by note, then octave dyads off the beat";
                    case 19:
                        every ([&] (int b)
                        {
                            each16 ("X-x---x---x---x-", b, [&] (char, int k, double t)
                            {
                                if (b == bars - 1 && k >= 8)
                                    return;   // (the bar before the next section stops after beat 2)
                                strum (main, t, high (chordAt (t)), k == 0 ? 0.79f : positional (k), down (k, false));
                            });
                        });
                        return "High five-string shapes on 1, 1& and then every off-beat, a chord a bar; the last bar stops after beat 2";

                    // PICKING ---------------------------------------------------
                    case 20:
                    {
                        const auto fifthHigh = pitchIn (key.pitchClass() + 7, 54, 65);
                        const auto tonic = pitchIn (key.pitchClass(), fifthHigh + 1, fifthHigh + 12);
                        const auto colourNote = pitchIn (key.scale()[(size_t) (key.minor ? 2 : 1)], tonic + 1, tonic + 12);
                        every ([&] (int b)
                        {
                            const auto& chord = chordOfBar (b);
                            const auto pcs = chord.pitchClasses();
                            const auto fixed = [&] (int p)
                            {
                                for (const auto c : pcs)
                                    if (const auto d = pc12 (p - c); d == 1 || d == 11)
                                        return pitchIn (chord.root + thirdOf (chord), p - 6, p + 6);   // (a half-step rub: the chord's third)
                                return p;
                            };
                            const int i = fixed (fifthHigh), t = fixed (tonic), T = fixed (colourNote);
                            const auto o = b * bpb;
                            pick (main, o, root (chord), 0.67f, bpb * 0.98);
                            const int tops[] { i, t, i, T, t, i, t };
                            const float vels[] { 0.47f, 0.39f, 0.47f, 0.59f, 0.47f, 0.47f, 0.47f };
                            for (int e = 1; e < 8; ++e)
                                pick (main, o + e * e8, tops[e - 1], vels[e - 1]);
                        });
                        return "The root held on 1 the whole bar; above it eighths on a fixed top from the key (its fifth, octave and second), the third note accented on beat 3";
                    }
                    case 21:
                        every ([&] (int b)
                        {
                            for (int h = 0; h < 2; ++h)
                            {
                                const auto o = b * bpb + h * 8 * s16;
                                const auto shape = full (o);
                                const auto three = top (shape, 3);
                                pick (main, o, root (chordAt (o)), 0.7f, 4 * s16);
                                const int order[] { 2, 1, 2, 1, 0 };
                                const int at[] { 2, 3, 5, 6, 7 };
                                for (int i = 0; i < 5; ++i)
                                {
                                    const auto p = three[(size_t) juce::jmin ((int) three.size() - 1, order[i])];
                                    if (h == 0 && i == 0)
                                    {
                                        pick (main, o + at[i] * s16, scaleStep (key, p, -1), 0.5f, s16 * 0.5);   // (hammered on)
                                        pick (main, o + at[i] * s16 + s16 * 0.5, p, 0.5f);
                                    }
                                    else
                                        pick (main, o + at[i] * s16, p, 0.55f);
                                }
                            }
                        });
                        return "Each half bar the bass on the beat, then the chord's top, middle and low notes falling in sixteenths; once a bar the top hammered on from a step under";
                    case 22:
                    {
                        const auto t = key.minor ? keyNote (3, 53, 64) : keyNote (1, 53, 64);
                        const auto u = t + 2;
                        every ([&] (int b)
                        {
                            for (int h = 0; h < 2; ++h)
                            {
                                const auto o = b * bpb + h * 8 * s16;
                                const auto bass = root (chordAt (o));
                                const auto nextAt = o + 8 * s16;
                                const auto nextBass = root (nextAt < length - 1.0e-6 ? chordAt (nextAt) : r.nextChord.value_or (spans.front().chord));
                                pick (main, o, bass, 0.62f, 6 * s16);
                                pick (main, o + 2 * s16, t, 0.59f);
                                pick (main, o + 4 * s16, u, 0.59f);
                                pick (main, o + 6 * s16, t, 0.59f);
                                pick (main, o + 7 * s16, scaleStep (key, nextBass, -1), 0.5f, s16);   // (the grace, hammered onto the next bass note)
                            }
                        });
                        return "Two chords a bar: each half bar the root held three eighths, two notes a whole step apart above it in turn, and a grace a step under the next bass note hammered on";
                    }
                    case 23:
                        for (const auto& span : spans)
                        {
                            const auto from = span.start - origin, to = span.end - origin;
                            const auto rt = root (span.chord);
                            const auto colourNote = span.chord.type == Chord::Type::minor ? rt + 15 : rt + 14;
                            const int cycle[] { rt, rt + fifthOf (span.chord), rt + 12, colourNote, rt + 12, rt + fifthOf (span.chord) };
                            auto i = 0;
                            for (auto t = from; t < to - 1.0e-6; t += e8, ++i)
                                pick (main, t, cycle[i % 6], i % 6 == 0 ? 0.62f : 0.52f);
                        }
                        return "One note an eighth: root, fifth, octave, the ninth (minor: the third an octave up), octave, fifth; starting again on each new chord's root";
                    case 24:
                        every ([&] (int b)
                        {
                            const auto& chord = chordOfBar (b - b % 2);
                            const auto rt = root (chord);
                            const auto th = thirdOf (chord), sev = diatonicSeventh (key, chord.root);
                            const auto o = b * bpb;
                            if (b % 8 == 7)
                                return;   // (a bar left empty after the phrase's held chord)
                            if (b % 2 == 0)
                            {
                                pick (main, o, rt, 0.65f);
                                pick (main, o + 2 * s16, rt + 7, 0.55f);
                                pick (main, o + 4 * s16, rt + 12 + th, 0.55f, 4 * s16);
                                pick (main, o + 8 * s16, rt + sev, 0.52f);
                                pick (main, o + 10 * s16, rt + 12 + th, 0.52f);
                                pick (main, o + 12 * s16, rt + 14, 0.52f);
                                if (b % 8 == 6)
                                    strum (main, o + 14 * s16, open (chordAfter (b), colour()), 0.7f, true, bpb + 2 * s16);
                                else
                                    pick (main, o + 14 * s16, rt + th, 0.5f);
                            }
                            else
                            {
                                pick (main, o + 2 * s16, rt + 7, 0.52f);
                                pick (main, o + 4 * s16, rt + 12, 0.52f);
                                pick (main, o + 6 * s16, rt + 12 + th, 0.52f);
                                pick (main, o + 10 * s16, rt + 14, 0.5f);
                                pick (main, o + 14 * s16, rt + 7, 0.5f);
                            }
                        });
                        return "A two-bar picking cycle on a seventh-and-ninth shape spread from the low root, the second bar leaving its downbeat ringing; a chord every two bars, every eighth bar a full chord on 4& held into an empty bar";
                    case 25:
                    {
                        const auto mid = pitchIn (key.pitchClass() + 7, 55, 66);
                        const auto topNote = pitchIn (key.pitchClass(), mid + 1, mid + 12);
                        every ([&] (int b)
                        {
                            const auto o = b * bpb;
                            const auto& c1 = chordAt (o);
                            const auto& c2 = chordAt (o + 6 * s16);
                            const auto low1 = root (c1);
                            const auto low2 = c2 == c1 ? low1 + thirdOf (c1) : root (c2);
                            const int notes[] { low1, mid, topNote, low2, mid, topNote, mid, topNote };
                            for (int e = 0; e < (b % 2 == 1 ? 6 : 8); ++e)
                                pick (main, o + e * e8, notes[e], e == 0 ? 0.6f : 0.5f);
                            if (b % 2 == 1)
                            {
                                pick (main, o + 12 * s16, topNote, 0.55f, 2 * s16);
                                pick (main, o + 14 * s16, scaleStep (key, topNote, -1), 0.5f, s16);
                                pick (main, o + 15 * s16, scaleStep (key, topNote, -2), 0.5f, s16);
                            }
                        });
                        return "Eighths in 3+3+2: a low note, then the key's fifth and octave fixed on top; the low note moves on 1 and 2&; every second bar a held note and a falling turn";
                    }
                    case 26:
                    {
                        const auto fixedTop = pitchIn (key.pitchClass() + 7, 64, 75);
                        every ([&] (int b)
                        {
                            const auto& chord = chordOfBar (b);
                            const auto low = pitchIn (chord.root + fifthOf (chord), lowest, lowest + 11);
                            const auto r1 = pitchIn (chord.root, low + 1, low + 12);
                            const auto r2 = pitchIn (chord.root + thirdOf (chord), r1 + 1, r1 + 12);
                            const auto r3 = fixedTop > r2 ? fixedTop : fixedTop + 12;
                            const int ranks[] { low, r1, r2, r1, r3, r2, r1, r2 };
                            for (int e = 0; e < 8; ++e)
                                pick (main, b * bpb + e * e8, ranks[e], e == 4 ? 0.58f : 0.5f);
                        });
                        return "Rung eighths in an arch (0 1 2 1 3 2 1 2) from the chord's fifth in the bass, the top note the key's fifth for the whole section";
                    }
                    case 27:
                    {
                        const auto tonicHigh = tonicIn (62, 73);
                        const int tops[] { tonicHigh, scaleStep (key, tonicHigh, -1), scaleStep (key, tonicHigh, 1) };
                        for (int b = 0; b < bars; b += 2)
                        {
                            const auto cycle = b / 2;
                            for (int e = 0; e < 16; ++e)
                            {
                                const auto t = b * bpb + e * e8;
                                const auto rt = root (chordAt (t));
                                if (e == 0)
                                    pick (main, t, rt, 0.65f);
                                else if (e == 3 || e == 6 || e == 9)
                                    pick (main, t, tops[(size_t) ((e / 3 - 1 + cycle) % 3)], 0.58f);
                                else
                                    pick (main, t, e % 2 == 1 ? rt + 12 : rt + 7, 0.48f);
                            }
                        }
                        return "A two-bar eighth cycle: the root low only on its first beat, the octave and fifth in the middle, three fixed notes of the key on top (its tonic, the seventh under it, the ninth) in a changing order";
                    }
                    case 28:
                        every ([&] (int b)
                        {
                            const auto& chord = chordOfBar (b);
                            const auto rt = root (chord);
                            const auto o = b * bpb;
                            pick (main, o, rt, 0.65f, 6 * s16);
                            const int arch[] { rt + 7, rt + 12, rt + 12 + thirdOf (chord), rt + 12, rt + 7 };
                            for (int i = 0; i < 5; ++i)
                                pick (main, o + (6 + 2 * i) * s16, arch[i], 0.52f);
                        });
                        return "The root held a dotted quarter, then fifth, octave, tenth, octave, fifth in eighths from 2&; a chord a bar";
                    case 29:
                        for (int b = 0; b < bars; b += 2)
                        {
                            const auto& chord = chordOfBar (b);
                            const auto o = b * bpb;
                            const auto low = pitchIn (chord.root + fifthOf (chord), lowest, lowest + 11);
                            const int cycle[] { pitchIn (chord.root + thirdOf (chord), 55, 66), pitchIn (chord.root + fifthOf (chord), 55, 66), pitchIn (chord.root, 55, 66) };
                            pick (main, o, low, 0.6f);
                            for (int e = 1; e < 7; ++e)
                                pick (main, o + e * e8, cycle[(e - 1) % 3], 0.5f);
                            const auto next = b + 2 < bars ? chordOfBar (b + 2) : r.nextChord.value_or (spans.front().chord);
                            pick (main, o + 7 * e8, pitchIn (next.root, 52, 63), 0.5f);
                        }
                        return "A bar of picked eighths (the fifth low, three chord tones cycling on top, the next chord's root as a pickup), then a bar of air; a chord every two bars";
                    case 30:
                    {
                        const auto t = tonicIn (60, 71);
                        every ([&] (int b)
                        {
                            const auto o = b * bpb;
                            if (b % 4 == 3)
                            {
                                pick (main, o, t, 0.55f);
                                const auto rt = pitchIn (chordAfter (b).root, 55, 66);
                                for (int i = 0; i < 5; ++i)
                                    pick (main, o + (6 + 2 * i) * s16, scaleStep (key, rt, i - 4), 0.55f);
                                return;
                            }
                            pick (main, o, t, 0.58f);
                            pick (main, o + 6 * s16, t + 5, 0.52f);
                            pick (main, o + 8 * s16, t + 7, 0.55f);
                            pick (main, o + 10 * s16, t + 5, 0.52f, 4 * s16);
                            pick (main, o + 14 * s16, t, 0.5f);
                        });
                        return "A fixed high figure (the tonic, a fourth and a fifth above it) on 1, 2&, 3, 3& and 4&, kept over the chord changes; every fourth bar a five-note eighth climb into the next chord";
                    }
                    case 31:
                        every ([&] (int b)
                        {
                            const auto o = b * bpb;
                            if (b % 2 == 1)
                            {
                                if (b % 4 == 3)
                                {
                                    const auto target = pitchIn (chordAfter (b).root, 50, 61);
                                    for (int i = 0; i < 3; ++i)
                                        pick (main, o + (13 + i) * s16, scaleStep (key, target, i - 3), 0.5f, s16 * 0.9);
                                }
                                return;
                            }
                            const auto rt = pitchIn (chordOfBar (b).root, 50, 61);
                            for (const auto k : { 1, 7, 11, 13 })
                                strum (main, o + k * s16, { rt, rt + 7 }, 0.55f, true, s16 * 0.9);
                            for (const auto k : { 2, 8 })
                                pick (main, o + k * s16, rt + 7, 0.45f, s16 * 0.9);
                        });
                        return "A dyad and its top note a sixteenth later on the weak sixteenths, -xx----xx--x-x--, then a bar off; every four bars a three-note pickup into the next chord";
                    case 32:
                        every ([&] (int b)
                        {
                            const auto& chord = chordOfBar (b);
                            const auto rt = root (chord);
                            const auto shape = b % 2 == 0 ? power (chord, 3) : std::vector<int> { rt + 18, rt + 19, rt + 20, rt + 22 };
                            for (const auto k : { 0, 6, 12 })
                                strum (main, b * bpb + k * s16, shape, b % 2 == 0 ? 0.7f : 0.6f, true);
                        });
                        return "A two-bar cycle on the sixteenths 1, 2& and 4: the low power chord with its octave, then a high cluster over the root (flat five, five, flat six, flat seven), ringing";
                    case 33:
                    {
                        const auto shape = open (tonicChord (Key { key.letter, key.accidental, false }), colour());
                        auto flick = shape;
                        for (auto& p : flick)
                            if (pc12 (p - key.pitchClass()) == 4)
                                p += 1;   // (the third up to the fourth)
                        if (! flick.empty())
                            flick.back() = flick.front() + 22;   // (the flat seventh on top)
                        std::sort (flick.begin(), flick.end());
                        strum (main, 0.0, shape, 0.79f, true, bpb * 0.98);
                        auto accents = r.riffAccents;
                        if (accents.empty())
                            for (int b = 0; b < bars; ++b)
                                for (const auto k : { 0, 4, 6, 10, 14 })
                                    accents.push_back (b * bpb + k * s16);
                        std::sort (accents.begin(), accents.end());
                        for (size_t i = 0; i < accents.size(); ++i)
                        {
                            const auto a = accents[i];
                            if (a < bpb - 1.0e-6 || a >= length - 1.0e-6)
                                continue;
                            strum (main, a, shape, 0.79f, true);
                            const auto bar = (int) std::floor (a / bpb + 1.0e-6);
                            const auto lastInBar = i + 1 == accents.size() || accents[i + 1] >= (bar + 1) * bpb - 1.0e-6;
                            if (bar % 4 == 3 && lastInBar && a + 2 * s16 < (bar + 1) * bpb - 1.0e-6)
                            {
                                strum (main, a + s16, flick, 0.6f, false, s16 * 0.95);
                                strum (main, a + 2 * s16, shape, 0.55f, true);
                            }
                        }
                        return r.riffAccents.empty() ? juce::String ("The open tonic as a major chord, struck on the riff's usual hits (the guitar lane is empty); the first bar one held strum")
                                                     : juce::String ("The open tonic as a major chord, struck only on the guitar riff's accents (the first bar one held strum), a sixteenth sus4 and seventh flick every four bars");
                    }
                    default: break;
                }
                return {};
            }

            void secondOf (int part)
            {
                const auto every = [this] (auto&& f) { for (int b = 0; b < bars; ++b) f (b); };
                switch (part)
                {
                    case 1:
                    {
                        const auto pedal = pitchIn (key.pitchClass() + 7, 62, 69);
                        every ([&] (int b)
                        {
                            const auto pcs = chordOfBar (b).pitchClasses();
                            auto upper = pedal + 5;
                            for (auto p = pedal + 3; p <= pedal + 7; ++p)
                                if (std::find (pcs.begin(), pcs.end(), pc12 (p)) != pcs.end())
                                {
                                    upper = p;
                                    break;
                                }
                            for (int e = 0; e < 8; ++e)
                                pick (second, b * bpb + e * e8, e % 2 == 0 ? upper : pedal, 0.5f);
                        });
                        break;
                    }
                    case 2:
                    {
                        const auto ring = pitchIn (key.pitchClass() + 7, 55, 66);
                        every ([&] (int b)
                        {
                            const auto o = b * bpb;
                            const auto& chord = chordOfBar (b);
                            const auto pcs = chord.pitchClasses();
                            auto topNote = 72;
                            for (auto p = 72; p <= 76; ++p)
                                if (std::find (pcs.begin(), pcs.end(), pc12 (p)) != pcs.end()) { topNote = p; break; }
                            pick (second, o + 4 * s16, ring, 0.45f, 12 * s16 * 0.98);
                            const int steps[] { 0, -1, -2, -1, 0 };
                            const int at[] { 4, 8, 10, 12, 14 };
                            for (int i = 0; i < 5; ++i)
                            {
                                const auto p = scaleStep (key, topNote, steps[i]);
                                strum (second, o + at[i] * s16, { scaleStep (key, p, -2), p }, 0.5f, true, 2 * s16 * 0.95);
                            }
                        });
                        break;
                    }
                    case 3:
                    {
                        auto lower = 62;
                        for (int b = 0; b < bars; b += 2)
                        {
                            const auto pcs = chordOfBar (b).pitchClasses();
                            auto best = lower;
                            auto bestDistance = 99;
                            for (auto p = 57; p <= 66; ++p)
                                if (std::find (pcs.begin(), pcs.end(), pc12 (p)) != pcs.end() && std::abs (p - lower) < bestDistance)
                                {
                                    best = p;
                                    bestDistance = std::abs (p - lower);
                                }
                            lower = best;
                            for (int e = 0; e < 16; ++e)
                                strum (second, b * bpb + e * e8, { lower, scaleStep (key, lower, 2) }, 0.45f, e % 2 == 0, e8 * 0.95);
                        }
                        break;
                    }
                    case 4:
                        every ([&] (int b)
                        {
                            const auto rt = root (chordOfBar (b));
                            for (const auto k : { 0, 4, 6 })
                                strum (second, b * bpb + k * s16, { rt, rt + 7 }, 0.6f, true);
                        });
                        break;
                    case 5:
                        every ([&] (int b)
                        {
                            if (b % 4 != 3)
                                return;
                            const auto& chord = chordOfBar (b);
                            const auto p = pitchIn (chord.root + fifthOf (chord), 62, 69);
                            for (const auto k : { 10, 12 })
                                strum (second, b * bpb + k * s16, { p, p + 5 }, 0.5f, true, 2 * s16 * 0.95);
                            pick (second, b * bpb + 14 * s16, pitchIn (chordAfter (b).root, 69, 76), 0.5f, 6 * s16);
                        });
                        break;
                    case 6:
                    {
                        std::vector<double> onsets;
                        for (const auto& ev : main)
                            if (onsets.empty() || ev.at > onsets.back() + 0.06)
                                onsets.push_back (ev.at);
                        auto i = 0;
                        const Chord* last = nullptr;
                        for (size_t j = 0; j < onsets.size(); ++j)
                        {
                            const auto& chord = chordAt (onsets[j]);
                            if (last == nullptr || ! (chord == *last))
                                i = 0;
                            last = &chord;
                            const auto rt = root (chord);
                            const int cycle[] { rt + 12, rt + 12 + thirdOf (chord), rt + 12 + fifthOf (chord) };
                            const auto next = j + 1 < onsets.size() ? onsets[j + 1] : length;
                            pick (second, onsets[j], cycle[i++ % 3], 0.5f, juce::jmax (0.05, next - onsets[j]));
                        }
                        break;
                    }
                    default: break;
                }
            }
        };
    }

    LineIdea acousticGuitarPart (const std::vector<ChordSpan>& spans, const AcousticRequest& request)
    {
        LineIdea idea;
        idea.part = LineIdea::Part::acoustic;
        if (spans.empty() || request.beatsPerBar <= 0.0)
            return idea;
        const auto& styles = acousticGuitarStyles();
        const auto style = juce::jlimit (0, styles.size() - 1, request.style);
        Writer w (spans, request);
        idea.style = styles[style];
        idea.description = w.styleOf (style);
        std::stable_sort (w.main.begin(), w.main.end(), [] (const Event& a, const Event& b) { return a.at < b.at; });
        const auto second = juce::jlimit (0, secondAcousticParts().size() - 1, request.second);
        w.secondOf (second);
        w.render (w.main, false, idea);
        w.render (w.second, true, idea);
        if (second > 0)
            idea.description += ". The second acoustic: " + secondAcousticParts()[second].toLowerCase();
        std::stable_sort (idea.notes.begin(), idea.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        return idea;
    }
}
