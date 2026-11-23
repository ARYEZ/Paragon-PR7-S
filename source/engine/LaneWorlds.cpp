#include "engine/LaneWorlds.h"
#include "engine/KeyCheck.h"

#include <cmath>

namespace pr7s
{
    const juce::StringArray& laneWorldNames()
    {
        static const juce::StringArray names { "ROCK / METAL (THE SECTION'S)", "CYBERPUNK" };
        return names;
    }

    juce::String laneWorldDescription (int world)
    {
        switch (world)
        {
            case (int) LaneWorld::cyberpunk:
                return "Synth parts of the cyberpunk world over the section's chords: arpeggios with the filter opening, gated stabs, "
                       "pads pumping on the kick, octave hooks, risers, glitches and sub drops. The rest of the band stays on the section type.";
            default:
                return "The section type's own options and the lane's styles, as always.";
        }
    }

    const juce::StringArray& cyberpunkKeysOptions()
    {
        static const juce::StringArray names {
            "FILTER-OPENING ARPEGGIO", "GATED STABS IN 3+3+2", "PULSING OCTAVE BASS", "PUMPING PADS ON THE KICK", "OCTAVE LEAD HOOK",
            "RISER: ARPEGGIO CLIMBS, FILTER SWEEPS", "GLITCH STUTTER", "DARK PAD, THE FLAT SIX ON TOP", "SUB DROPS, STABS ON THE CHUGS",
            "SIXTEENTH SEQUENCE, SLOW FILTER WAVE", "CHORD ON EVERY KICK", "BELL MOTIF WITH ECHOES", "ARPEGGIO FADING OUT, FILTER CLOSING" };
        return names;
    }

    juce::String cyberpunkKeysDescription (int option)
    {
        switch (option)
        {
            case 0: return "Sixteenths over the chord's root, fifth, octave and third, the filter opening across every four bars";
            case 1: return "Short sus and add9 chords on a 3+3+2 grid, the filter jumping bar by bar";
            case 2: return "A sequenced bass: the root and its octave in turn on every sixteenth, accents on the beats";
            case 3: return "Wide pads held through each chord, their level ducked on every kick and swelling back (the pumping feel)";
            case 4: return "A two-bar hook high over the band, doubled an octave under, the fourth bar of each four turned home";
            case 5: return "An arpeggio climbing two octaves across the section while the filter sweeps open, into the next section's chord";
            case 6: return "Chord tones chopped into stutters, gaps and repeats a beat at a time, the filter stepping about";
            case 7: return "Dark held pads with the key's sixth (a minor key's flat six) ringing on top, the filter creeping open";
            case 8: return "A sub note dropping on every bar with the filter closing after it, short stabs on the guitar's chugs";
            case 9: return "A sixteenth sequence of the root and its octave in a pushing figure, the filter rising and falling over two bars";
            case 10: return "The chord stabbed on every kick of the drums (on the beats when there are none)";
            case 11: return "A three-note bell figure high up every two bars, each note echoed a dotted eighth later, softer";
            case 12: return "Eighth arpeggios fading away as the filter closes over the whole section";
            default: return {};
        }
    }

    int cyberpunkKeysAutomatic (WorldSection section, int variation)
    {
        static const std::vector<std::vector<int>> bySection {
            { 0, 11 },        // intro
            { 2, 9, 1 },      // verse
            { 5 },            // pre-chorus
            { 3, 4, 10 },     // chorus
            { 6, 7 },         // bridge
            { 8, 10 },        // breakdown
            { 12, 11 },       // outro
            { 9, 3 } };       // solo
        const auto& own = bySection[(size_t) juce::jlimit (0, (int) bySection.size() - 1, (int) section)];
        return own[(size_t) (juce::jmax (0, variation) % (int) own.size())];
    }

    namespace
    {
        int inRange (int pitchClass, int low, int high)
        {
            for (auto p = low; p <= high; ++p)
                if (((p % 12) + 12) % 12 == ((pitchClass % 12) + 12) % 12)
                    return p;
            return low;
        }

        struct Tones
        {
            int root = 0, third = 4, fifth = 7;
        };

        Tones tonesOf (const Chord& c)
        {
            const auto in = c.intervals();
            Tones t;
            t.root = c.root;
            t.third = in.size() > 1 ? in[1] : 4;
            t.fifth = in.size() > 2 ? in[2] : 7;
            return t;
        }

        struct Writer
        {
            const std::vector<ChordSpan>& spans;
            const CyberpunkRequest& r;
            LineIdea out;
            double origin = 0.0, length = 0.0, bpb = 4.0, s16 = 0.25;
            int bars = 1;

            Writer (const std::vector<ChordSpan>& sp, const CyberpunkRequest& rq) : spans (sp), r (rq)
            {
                out.part = LineIdea::Part::chords;
                origin = spans.front().start;
                length = spans.back().end - origin;
                bpb = r.beatsPerBar > 0.0 ? r.beatsPerBar : 4.0;
                s16 = bpb / 16.0;
                bars = juce::jmax (1, (int) std::ceil (length / bpb - 1.0e-6));
            }

            const ChordSpan& spanAt (double rel) const
            {
                for (const auto& s : spans)
                    if (rel + origin < s.end - 1.0e-9)
                        return s;
                return spans.back();
            }

            void note (double at, double len, int pitch, float velocity)
            {
                if (at < -1.0e-9 || at >= length - 1.0e-9)
                    return;
                len = juce::jmin (len, length - at);
                if (len <= 1.0e-4)
                    return;
                LineNote n;
                n.beat = juce::jmax (0.0, at);
                n.length = len;
                n.pitch = juce::jlimit (24, 108, pitch);
                n.velocity = juce::jlimit (0.05f, 1.0f, velocity);
                out.notes.push_back (n);
            }

            void chord (double at, double len, const std::vector<int>& pitches, float velocity)
            {
                for (const auto p : pitches)
                    note (at, len, p, velocity);
            }

            void control (int number, double at, int value, bool ramp = true)
            {
                if (at < -1.0e-9 || at > length + 1.0e-9)
                    return;
                out.controls.push_back ({ number, juce::jlimit (0.0, length, at), juce::jlimit (0, 127, value), ramp });
            }

            // Root, fifth, octave, the third above it, and the add9 when the key holds it (a sus chord keeps its own).
            std::vector<int> pad (const ChordSpan& s, int low, bool addNine = true) const
            {
                const auto t = tonesOf (s.chord);
                const auto r0 = inRange (t.root, low, low + 11);
                std::vector<int> p { r0, r0 + t.fifth, r0 + 12, r0 + 12 + t.third };
                if (addNine && s.chord.type != Chord::Type::diminished && isPitchClassInKey ((t.root + 2) % 12, s.key))
                    p.push_back (r0 + 14);
                return p;
            }

            std::vector<int> arpeggio (const ChordSpan& s, int low) const
            {
                const auto t = tonesOf (s.chord);
                const auto r0 = inRange (t.root, low, low + 11);
                return { r0, r0 + t.fifth, r0 + 12, r0 + 12 + t.third, r0 + 24, r0 + 12 + t.third, r0 + 12, r0 + t.fifth };
            }

            // The kicks, else every beat.
            std::vector<double> kicksOrBeats() const
            {
                std::vector<double> hits;
                for (const auto k : r.kicks)
                    if (k >= -1.0e-9 && k < length - 1.0e-9 && (hits.empty() || k - hits.back() > s16 * 0.5))
                        hits.push_back (k);
                if (hits.empty())
                    for (auto t = 0.0; t < length - 1.0e-9; t += bpb / 4.0)
                        hits.push_back (t);
                return hits;
            }

            unsigned hash (int a, int b) const
            {
                auto h = (unsigned) (r.variation * 7919 + a * 131 + b * 17 + 3);
                h ^= h >> 13;
                h *= 0x5bd1e995u;
                h ^= h >> 15;
                return h;
            }
        };
    }

    LineIdea cyberpunkKeys (int option, const std::vector<ChordSpan>& spans, const CyberpunkRequest& r)
    {
        if (spans.empty())
            return {};
        Writer w (spans, r);
        const auto option13 = juce::jlimit (0, cyberpunkKeysOptions().size() - 1, option);
        w.out.style = "Cyberpunk: " + cyberpunkKeysOptions()[option13].toLowerCase();
        w.out.description = cyberpunkKeysDescription (option13);
        const auto step = r.busy < 0.35f ? w.s16 * 2.0 : w.s16;   // BUSY under a third: eighths
        const auto L = w.length;

        switch (option13)
        {
            case 0:   // FILTER-OPENING ARPEGGIO
            case 12:  // ARPEGGIO FADING OUT, FILTER CLOSING
            {
                const auto fading = option13 == 12;
                const auto every = fading ? w.s16 * 2.0 : step;
                int i = 0;
                for (auto t = 0.0; t < L - 1.0e-9; t += every, ++i)
                {
                    const auto& s = w.spanAt (t);
                    const auto line = w.arpeggio (s, 48);
                    const auto accent = std::fmod (t, w.bpb / 4.0) < 1.0e-6;
                    auto v = accent ? 0.82f : 0.6f;
                    if (fading)
                        v *= (float) (1.0 - 0.65 * t / L);
                    w.note (t, every * 0.9, line[(size_t) (i % (int) line.size())], v);
                }
                if (fading)
                {
                    w.control (74, 0.0, 110, false);
                    w.control (74, L, 15);
                }
                else
                    for (auto bar = 0; bar < w.bars; bar += 4)
                    {
                        w.control (74, bar * w.bpb, 20, false);
                        w.control (74, juce::jmin (L, (bar + 4) * w.bpb) - w.s16, 110);
                    }
                break;
            }
            case 1:   // GATED STABS IN 3+3+2
                for (int bar = 0; bar < w.bars; ++bar)
                {
                    for (const auto k : { 0, 3, 6, 8, 11, 14 })
                    {
                        const auto t = bar * w.bpb + k * w.s16;
                        w.chord (t, w.s16 * 1.5, w.pad (w.spanAt (t), 55), k % 8 == 0 ? 0.85f : 0.7f);
                    }
                    w.control (74, bar * w.bpb, bar % 2 == 0 ? 62 : 98, false);
                }
                break;
            case 2:   // PULSING OCTAVE BASS
            {
                int i = 0;
                for (auto t = 0.0; t < L - 1.0e-9; t += step, ++i)
                {
                    const auto r0 = inRange (w.spanAt (t).chord.root, 36, 47);
                    const auto accent = std::fmod (t, w.bpb / 4.0) < 1.0e-6;
                    w.note (t, step * 0.8, i % 2 == 0 ? r0 : r0 + 12, accent ? 0.9f : 0.66f);
                }
                for (int bar = 0; bar < w.bars; ++bar)
                {
                    w.control (74, bar * w.bpb, 40, false);
                    w.control (74, (bar + 1) * w.bpb - w.s16, 85);
                }
                break;
            }
            case 3:   // PUMPING PADS ON THE KICK
            {
                for (const auto& s : spans)
                    w.chord (s.start - w.origin, (s.end - s.start) * 0.98, w.pad (s, 52), 0.62f);
                const auto hits = w.kicksOrBeats();
                w.control (11, 0.0, 120, false);
                for (size_t i = 0; i < hits.size(); ++i)
                {
                    const auto next = i + 1 < hits.size() ? hits[i + 1] : L;
                    const auto back = juce::jmin (w.bpb / 4.0, next - hits[i]) * 0.75;
                    w.control (11, hits[i], 25, false);
                    w.control (11, hits[i] + back, 120);
                }
                break;
            }
            case 4:   // OCTAVE LEAD HOOK
                for (int bar = 0; bar < w.bars; ++bar)
                {
                    const auto o = bar * w.bpb;
                    const auto& s = w.spanAt (o);
                    const auto t = tonesOf (s.chord);
                    const auto r0 = inRange (t.root, 72, 83);
                    const auto home = bar % 4 == 3;
                    struct Hit { int step; int pitch; int len; };
                    const std::vector<Hit> hook = bar % 2 == 0
                        ? std::vector<Hit> { { 0, r0 + t.fifth - 12, 3 }, { 3, r0, 3 }, { 6, r0 + t.third, 2 }, { 8, r0, 2 }, { 10, r0 + t.fifth - 12, 2 }, { 12, r0, 4 } }
                        : (home ? std::vector<Hit> { { 0, r0 + t.third, 2 }, { 2, r0 + t.fifth - 12, 2 }, { 4, r0, 4 }, { 8, r0 + 12, 8 } }
                                : std::vector<Hit> { { 0, r0 + t.third, 2 }, { 2, r0, 2 }, { 4, r0 + t.fifth - 12, 4 }, { 8, r0 + t.third, 8 } });
                    for (const auto& h : hook)
                    {
                        const auto at = o + h.step * w.s16;
                        w.note (at, h.len * w.s16 + 0.02, h.pitch, 0.86f);
                        w.note (at, h.len * w.s16 + 0.02, h.pitch - 12, 0.58f);
                    }
                }
                w.control (74, 0.0, 96, false);
                break;
            case 5:   // RISER: ARPEGGIO CLIMBS, FILTER SWEEPS
            {
                int i = 0;
                for (auto t = 0.0; t < L - w.s16 - 1.0e-9; t += w.s16, ++i)
                {
                    const auto p = t / L;
                    const auto low = 45 + (int) std::floor (p * 4.0) * 8;   // (two octaves over the section)
                    const auto line = w.arpeggio (w.spanAt (t), low);
                    w.note (t, w.s16 * 0.85, line[(size_t) (i % (int) line.size())], (float) (0.45 + 0.45 * p));
                }
                const auto aim = r.nextChord.has_value() ? r.nextChord->root : spans.front().chord.root;
                w.note (L - w.s16, w.s16 * 0.95, inRange (aim, 84, 95), 0.95f);
                w.control (74, 0.0, 5, false);
                w.control (74, L - w.s16, 127);
                break;
            }
            case 6:   // GLITCH STUTTER
            {
                const auto beats = (int) std::ceil (L / (w.bpb / 4.0) - 1.0e-6);
                for (int b = 0; b < beats; ++b)
                {
                    const auto o = b * (w.bpb / 4.0);
                    const auto& s = w.spanAt (o);
                    const auto t = tonesOf (s.chord);
                    const auto r0 = inRange (t.root, 60, 71);
                    const int tones[] { r0, r0 + t.third, r0 + t.fifth, r0 + 12 };
                    const auto h = w.hash (b, 1);
                    const auto p = tones[h % 4];
                    switch ((h >> 4) % 4)
                    {
                        case 0: for (int k = 0; k < 4; ++k) w.note (o + k * w.s16, w.s16 * 0.6, p, k == 0 ? 0.85f : 0.6f); break;   // a stutter
                        case 1: w.note (o, w.s16 * 0.9, p, 0.8f); break;                                                           // one, then a gap
                        case 2: for (int k = 0; k < 4; ++k) w.note (o + k * w.s16, w.s16 * 0.7, k % 2 == 0 ? p : tones[(h >> 8) % 4], 0.7f); break;
                        default: w.note (o, w.s16 * 0.5, p, 0.8f); w.note (o + w.s16 * 2.0, w.s16 * 1.8, p + 12, 0.7f); break;
                    }
                    w.control (74, o, 20 + (int) ((h >> 12) % 100), false);
                }
                break;
            }
            case 7:   // DARK PAD, THE FLAT SIX ON TOP
                for (const auto& s : spans)
                {
                    auto p = w.pad (s, 52, false);
                    const auto sixth = s.key.scale()[5];
                    p.push_back (inRange (sixth, p.back() + 1, p.back() + 12));
                    w.chord (s.start - w.origin, (s.end - s.start) * 0.98, p, 0.6f);
                }
                for (auto bar = 0; bar < w.bars; bar += 4)
                {
                    w.control (74, bar * w.bpb, 30, false);
                    w.control (74, juce::jmin (L, (bar + 4) * w.bpb) - w.s16, 72);
                }
                break;
            case 8:   // SUB DROPS, STABS ON THE CHUGS
            {
                for (int bar = 0; bar < w.bars; ++bar)
                {
                    const auto o = bar * w.bpb;
                    w.note (o, w.bpb / 2.0, inRange (w.spanAt (o).chord.root, 24, 35), 0.95f);
                    w.control (74, o, 127, false);
                    w.control (74, o + w.bpb / 4.0, 30);
                }
                auto hits = r.guitarHits;
                if (hits.empty())
                    hits = r.kicks;
                if (hits.empty())
                    for (int bar = 0; bar < w.bars; ++bar)
                        for (const auto k : { 0, 6 })
                            hits.push_back (bar * w.bpb + k * w.s16);
                double last = -1.0;
                for (const auto h : hits)
                    if (h - last > w.s16 * 0.5)
                    {
                        w.chord (h, w.s16 * 1.2, w.pad (w.spanAt (h), 60, false), 0.78f);
                        last = h;
                    }
                break;
            }
            case 9:   // SIXTEENTH SEQUENCE, SLOW FILTER WAVE
            {
                const char* figure = "x-xx-xx-x-xx-xxx";
                for (int bar = 0; bar < w.bars; ++bar)
                {
                    int n = 0;
                    for (int k = 0; k < 16; ++k)
                        if (figure[k] == 'x')
                        {
                            const auto t = bar * w.bpb + k * w.s16;
                            const auto r0 = inRange (w.spanAt (t).chord.root, 48, 59);
                            w.note (t, w.s16 * 0.8, n % 4 == 3 ? r0 + 12 : r0, k % 4 == 0 ? 0.85f : 0.62f);
                            ++n;
                        }
                }
                for (auto t = 0.0; t <= L + 1.0e-9; t += w.bpb / 4.0)
                    w.control (74, t, 64 + (int) std::lround (40.0 * std::sin (juce::MathConstants<double>::twoPi * t / (w.bpb * 2.0))));
                break;
            }
            case 10:  // CHORD ON EVERY KICK
            {
                const auto hits = w.kicksOrBeats();
                for (size_t i = 0; i < hits.size(); ++i)
                {
                    const auto next = i + 1 < hits.size() ? hits[i + 1] : L;
                    w.chord (hits[i], juce::jmin (w.bpb / 4.0, next - hits[i]) * 0.85, w.pad (w.spanAt (hits[i]), 55), 0.8f);
                }
                w.control (74, 0.0, 88, false);
                break;
            }
            case 11:  // BELL MOTIF WITH ECHOES
                for (int bar = 0; bar < w.bars; bar += 2)
                {
                    const auto o = bar * w.bpb;
                    const auto& s = w.spanAt (o);
                    const auto t = tonesOf (s.chord);
                    const auto r0 = inRange (t.root, 84, 95);
                    const int bell[] { r0 + t.third - 12, r0 + t.fifth - 12, r0 };
                    const auto dotted = w.s16 * 3.0;
                    for (int i = 0; i < 3; ++i)
                        for (int e = 0; e < 3; ++e)
                            w.note (o + i * dotted * 2.0 + e * dotted, w.s16 * 2.0, bell[i], 0.8f * (e == 0 ? 1.0f : e == 1 ? 0.55f : 0.3f));
                }
                w.control (74, 0.0, 100, false);
                break;
            default: break;
        }
        std::stable_sort (w.out.notes.begin(), w.out.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        std::stable_sort (w.out.controls.begin(), w.out.controls.end(), [] (const LineControl& a, const LineControl& b) { return a.beat < b.beat; });
        return w.out;
    }
}
