#include "engine/Lines.h"

namespace pr7s
{
    namespace
    {
        int pcOf (int note) { return ((note % 12) + 12) % 12; }

        bool has (const std::vector<int>& list, int value)
        {
            return std::find (list.begin(), list.end(), value) != list.end();
        }

        // The note of this pitch class nearest `around`, within [low, high] (ties: the lower).
        int nearestPitch (int pitchClass, double around, int low, int high)
        {
            auto best = -1;
            for (auto n = low; n <= high; ++n)
                if (pcOf (n) == pcOf (pitchClass) && (best < 0 || std::abs (n - around) < std::abs (best - around)))
                    best = n;
            return best < 0 ? low : best;
        }

        // The nearest of these pitch classes to `around`, within [low, high].
        int nearestOf (const std::vector<int>& pitchClasses, double around, int low, int high)
        {
            auto best = -1;
            for (const auto p : pitchClasses)
            {
                const auto n = nearestPitch (p, around, low, high);
                if (pcOf (n) != pcOf (p))
                    continue;   // (none of it in the range: nearestPitch fell back on `low`)
                if (best < 0 || std::abs (n - around) < std::abs (best - around))
                    best = n;
            }
            return best < 0 ? juce::jlimit (low, high, juce::roundToInt (around)) : best;
        }

        // The next note up (dir +1) or down (-1) from `pitch` among these pitch classes,
        // turning back at the range's edge.
        int stepFrom (int pitch, int dir, const std::vector<int>& pitchClasses, int low, int high)
        {
            for (const auto d : { dir, -dir })
                for (auto n = pitch + d; n >= low && n <= high; n += d)
                    if (has (pitchClasses, pcOf (n)))
                        return n;
            return juce::jlimit (low, high, pitch);
        }

        int bassOf (const Chord& chord) { return chord.bass >= 0 ? chord.bass : chord.root; }

        // A rhythm within a bar: onsets and lengths in beats of 4/4, scaled to the bar.
        struct Hit { double at, length; bool strong; };
        using Rhythm = std::vector<Hit>;

        Rhythm scaled (const Rhythm& r, double beatsPerBar)
        {
            Rhythm out;
            const auto k = beatsPerBar / 4.0;
            for (const auto& h : r)
                out.push_back ({ h.at * k, h.length * k, h.strong });
            return out;
        }

        // Where a rhythm lands over the chords: bar after bar from each chord's start, cut
        // at its end (span index, the onset in the song relative to the first chord).
        struct Onset { double at, length; bool strong; size_t span; };

        std::vector<Onset> place (const std::vector<ChordSpan>& spans, const Rhythm& bar, double beatsPerBar)
        {
            std::vector<Onset> out;
            const auto origin = spans.front().start;
            for (size_t s = 0; s < spans.size(); ++s)
                for (auto t = spans[s].start; t < spans[s].end - 1.0e-9; t += beatsPerBar)
                    for (const auto& h : bar)
                    {
                        const auto at = t + h.at;
                        if (at >= spans[s].end - 1.0e-9)
                            continue;
                        out.push_back ({ at - origin, juce::jmin (h.length, spans[s].end - at), h.strong || h.at == 0.0, s });
                    }
            return out;
        }

        void add (std::vector<LineNote>& notes, double at, double length, int pitch, float velocity)
        {
            if (length > 1.0e-6)
                notes.push_back ({ at, length * 0.96, juce::jlimit (0, 127, pitch), velocity });
        }
    }

    std::vector<int> availableNotes (const Chord& chord, const Key& key)
    {
        const auto tones = chord.pitchClasses();
        std::vector<int> out (tones.begin(), tones.end());
        const auto scale = key.scale();   // (one list: 0.16.1)
        for (const auto s : scale)
        {
            if (has (out, s))
                continue;
            // A key note a half step from a chord tone outside the key rubs against it (G over E major in A minor).
            auto rubs = false;
            for (const auto t : tones)
                if (! has (scale, t) && (pcOf (s - t) == 1 || pcOf (t - s) == 1))
                    rubs = true;
            if (! rubs)
                out.push_back (s);
        }
        std::sort (out.begin(), out.end());
        return out;
    }

    //==========================================================================
    std::vector<LineIdea> bassIdeas (const std::vector<ChordSpan>& spans, double beatsPerBar, int variation)
    {
        std::vector<LineIdea> ideas;
        if (spans.empty())
            return ideas;
        using namespace LineRanges;
        const auto origin = spans.front().start;
        const auto up = variation % 2 == 0;
        const auto make = [&] (const char* style, const char* description) -> LineIdea&
        {
            ideas.push_back ({ LineIdea::Part::bass, style, description, {} });
            return ideas.back();
        };
        // Each chord's root in the octave nearest the last one, drawn back to the middle of the range.
        const auto roots = [&]
        {
            std::vector<int> r;
            double prev = 40.0 + (variation % 3) * 2.0;
            for (const auto& s : spans)
            {
                const auto n = nearestPitch (bassOf (s.chord), (prev * 2.0 + 40.0) / 3.0, bassLow, bassHigh);
                r.push_back (n);
                prev = n;
            }
            return r;
        }();
        const auto fifthOf = [&] (int root)
        {
            return up ? (root + 7 <= bassHigh + 5 ? root + 7 : root - 5) : (root - 5 >= bassLow ? root - 5 : root + 7);
        };
        const auto approach = [&] (size_t s)   // a half step into the next chord's root (none after the last)
        {
            if (s + 1 >= spans.size())
                return roots[s];
            return roots[s + 1] + (up ? -1 : 1);
        };

        {
            auto& idea = make ("Roots", "Each chord's root, held: the steadiest foundation");
            for (size_t s = 0; s < spans.size(); ++s)
                for (auto t = spans[s].start; t < spans[s].end - 1.0e-9; t += beatsPerBar)
                    add (idea.notes, t - origin, juce::jmin (beatsPerBar, spans[s].end - t), roots[s], 0.85f);
        }
        {
            auto& idea = make ("Root and fifth", "The root, then its fifth halfway through the bar (country, pop ballads)");
            for (const auto& o : place (spans, scaled ({ { 0.0, 2.0, true }, { 2.0, 2.0, false } }, beatsPerBar), beatsPerBar))
                add (idea.notes, o.at, o.length, o.strong ? roots[o.span] : fifthOf (roots[o.span]), o.strong ? 0.85f : 0.75f);
        }
        {
            auto& idea = make ("Driving eighths", "The root on every eighth note, the last one leading into the next chord (rock, pop)");
            const auto hits = place (spans, scaled ({ { 0, .5, true }, { .5, .5, false }, { 1, .5, false }, { 1.5, .5, false },
                                                      { 2, .5, true }, { 2.5, .5, false }, { 3, .5, false }, { 3.5, .5, false } }, beatsPerBar), beatsPerBar);
            for (size_t i = 0; i < hits.size(); ++i)
            {
                const auto& o = hits[i];
                const auto lastOfChord = i + 1 == hits.size() || hits[i + 1].span != o.span;
                add (idea.notes, o.at, o.length, lastOfChord && variation % 3 != 2 ? approach (o.span) : roots[o.span], o.strong ? 0.9f : 0.72f);
            }
        }
        {
            auto& idea = make ("Octaves", "The root and the octave above it in turn, on eighth notes (disco, dance)");
            const auto hits = place (spans, scaled ({ { 0, .5, true }, { .5, .5, false }, { 1, .5, false }, { 1.5, .5, false },
                                                      { 2, .5, true }, { 2.5, .5, false }, { 3, .5, false }, { 3.5, .5, false } }, beatsPerBar), beatsPerBar);
            auto high = false;
            for (const auto& o : hits)
            {
                if (o.strong)
                    high = false;
                add (idea.notes, o.at, o.length, roots[o.span] + (high ? 12 : 0), high ? 0.7f : 0.88f);
                high = ! high;
            }
        }
        {
            auto& idea = make ("Walking", "A note on every beat, through the chord and the key, stepping into each next chord (jazz, blues, gospel)");
            const auto hits = place (spans, scaled ({ { 0, 1, true }, { 1, 1, false }, { 2, 1, false }, { 3, 1, false } }, beatsPerBar), beatsPerBar);
            auto pitch = roots[0];
            for (size_t i = 0; i < hits.size(); ++i)
            {
                const auto& o = hits[i];
                const auto& span = spans[o.span];
                const auto lastOfChord = i + 1 == hits.size() || hits[i + 1].span != o.span;
                if (o.strong && (i == 0 || hits[i - 1].span != o.span))
                    pitch = roots[o.span];                          // the chord's first beat: its root
                else if (lastOfChord && o.span + 1 < spans.size())
                    pitch = approach (o.span);                      // into the next root
                else
                {
                    // Towards the next root (or up, with none), through the chord's notes and the key's.
                    const auto target = o.span + 1 < spans.size() ? roots[o.span + 1] : roots[o.span] + (up ? 7 : -5);
                    const auto dir = target > pitch ? 1 : target < pitch ? -1 : (up ? 1 : -1);
                    const auto tones = span.chord.pitchClasses();
                    const auto next = stepFrom (pitch, dir, availableNotes (span.chord, span.key), bassLow, bassHigh + 5);
                    // On the bar's middle beat a chord tone if one is a step or two away.
                    const auto chordTone = stepFrom (pitch, dir, std::vector<int> (tones.begin(), tones.end()), bassLow, bassHigh + 5);
                    pitch = std::abs (chordTone - pitch) <= 4 ? chordTone : next;
                }
                add (idea.notes, o.at, o.length, pitch, o.strong ? 0.86f : 0.74f);
            }
        }
        {
            auto& idea = make ("Syncopated", "The root on one and off the beat, the octave, then a lead into the next chord (R&B, funk, soul)");
            const auto hits = place (spans, scaled ({ { 0, 1.25, true }, { 1.5, .5, false }, { 2.5, .5, false }, { 3.5, .5, false } }, beatsPerBar), beatsPerBar);
            for (size_t i = 0; i < hits.size(); ++i)
            {
                const auto& o = hits[i];
                const auto lastOfChord = i + 1 == hits.size() || hits[i + 1].span != o.span;
                const auto root = roots[o.span];
                const auto pitch = lastOfChord && o.at > 0.0 && ! o.strong ? approach (o.span)
                                 : o.at - std::floor (o.at / beatsPerBar) * beatsPerBar >= 2.4 * beatsPerBar / 4.0 && ! o.strong ? (variation % 2 == 0 ? root + 12 : fifthOf (root))
                                 : root;
                add (idea.notes, o.at, o.length, pitch, o.strong ? 0.9f : 0.75f);
            }
        }
        return ideas;
    }

    //==========================================================================
    namespace
    {
        // Pitches for a melody's onsets: on a strong one the chord tone nearest where the
        // line is heading; between them a step through the key towards the next one.
        std::vector<int> singLine (const std::vector<Onset>& onsets, const std::vector<ChordSpan>& spans, int start, int variation)
        {
            using namespace LineRanges;
            std::vector<int> pitches;
            auto prev = start;
            const auto total = onsets.empty() ? 0.0 : onsets.back().at;
            for (size_t i = 0; i < onsets.size(); ++i)
            {
                const auto& o = onsets[i];
                const auto& span = spans[o.span];
                const auto tones = span.chord.pitchClasses();
                // The phrase's arch: up through its first half, down through the second (or the other way).
                const auto rising = (o.at < total * 0.5) == (variation % 2 == 0);
                if (o.strong)
                    prev = nearestOf (std::vector<int> (tones.begin(), tones.end()), prev + (rising ? 2.0 : -2.0), melodyLow, melodyHigh);
                else
                {
                    // The next strong onset's chord tone, from here.
                    auto target = prev;
                    for (size_t j = i + 1; j < onsets.size(); ++j)
                        if (onsets[j].strong)
                        {
                            const auto next = spans[onsets[j].span].chord.pitchClasses();
                            target = nearestOf (std::vector<int> (next.begin(), next.end()), prev + (rising ? 2.0 : -2.0), melodyLow, melodyHigh);
                            break;
                        }
                    const auto dir = target > prev ? 1 : target < prev ? -1 : (rising ? 1 : -1);
                    prev = stepFrom (prev, dir, availableNotes (span.chord, span.key), melodyLow, melodyHigh);
                }
                pitches.push_back (prev);
            }
            return pitches;
        }

        // The line's last note on the chord (its root if it's near), held to the chord's end.
        void cadence (std::vector<LineNote>& notes, const std::vector<ChordSpan>& spans)
        {
            if (notes.empty())
                return;
            auto& last = notes.back();
            const auto& span = spans.back();
            const auto root = nearestPitch (span.chord.root, last.pitch, LineRanges::melodyLow, LineRanges::melodyHigh);
            const auto tones = span.chord.pitchClasses();
            last.pitch = std::abs (root - last.pitch) <= 4 ? root
                       : nearestOf (std::vector<int> (tones.begin(), tones.end()), last.pitch, LineRanges::melodyLow, LineRanges::melodyHigh);
            last.length = juce::jmax (last.length, (span.end - spans.front().start - last.beat) * 0.96);
        }
    }

    std::vector<LineIdea> melodyIdeas (const std::vector<ChordSpan>& spans, double beatsPerBar, int variation)
    {
        std::vector<LineIdea> ideas;
        if (spans.empty())
            return ideas;
        using namespace LineRanges;
        juce::Random random (0x5eed + variation * 7919);
        const auto origin = spans.front().start;
        const auto start = variation % 3 == 0 ? 67 : variation % 3 == 1 ? 72 : 64;   // where the line begins (G3, C4, E3)
        const auto make = [&] (const char* style, const char* description) -> LineIdea&
        {
            ideas.push_back ({ LineIdea::Part::melody, style, description, {} });
            return ideas.back();
        };
        const auto fromRhythm = [&] (LineIdea& idea, const Rhythm& bar)
        {
            const auto onsets = place (spans, scaled (bar, beatsPerBar), beatsPerBar);
            const auto pitches = singLine (onsets, spans, start, variation);
            for (size_t i = 0; i < onsets.size(); ++i)
                add (idea.notes, onsets[i].at, onsets[i].length, pitches[i], onsets[i].strong ? 0.85f : 0.72f);
            cadence (idea.notes, spans);
        };

        {
            // One note a bar, moving as little as it can: a common tone stays, else the nearest step.
            auto& idea = make ("Long tones", "One held note a bar, moving as little as it can from chord to chord (pads, ballads, a top line)");
            auto prev = start;
            for (const auto& s : spans)
                for (auto t = s.start; t < s.end - 1.0e-9; t += beatsPerBar)
                {
                    const auto tones = s.chord.pitchClasses();
                    std::vector<int> list (tones.begin(), tones.end());
                    if (! has (list, pcOf (prev)) || variation % 2 == 1)
                        prev = nearestOf (list, prev + (variation % 2 == 1 ? 1.5 : 0.0), melodyLow, melodyHigh);
                    add (idea.notes, t - origin, juce::jmin (beatsPerBar, s.end - t), prev, 0.8f);
                }
            cadence (idea.notes, spans);
        }
        {
            auto& idea = make ("Singable", "Steps through the key with chord notes on the strong beats, rising then falling (a vocal line)");
            fromRhythm (idea, variation % 2 == 0 ? Rhythm { { 0, 1, true }, { 1, .5, false }, { 1.5, .5, false }, { 2, 1, true }, { 3, 1, false } }
                                                 : Rhythm { { 0, 1.5, true }, { 1.5, .5, false }, { 2, 1, true }, { 3, .5, false }, { 3.5, .5, false } });
        }
        {
            // A short motif (rhythm and shape) made once, then fitted to each bar's chord.
            auto& idea = make ("Hook", "A short motif repeated every bar and bent to fit each chord (pop and dance hooks)");
            static const Rhythm rhythms[] = {
                { { 0, .5, true }, { .5, .5, false }, { 1, 1, false }, { 2.5, .5, false }, { 3, 1, false } },
                { { 0, .75, true }, { .75, .75, false }, { 1.5, .5, false }, { 2, 2, true } },
                { { 0, .5, true }, { .5, .5, false }, { 1, .5, false }, { 1.5, 1.5, false } } };
            const auto& rhythm = rhythms[(size_t) (variation % 3)];
            std::vector<int> shape { 0 };
            for (size_t i = 1; i < rhythm.size(); ++i)
                shape.push_back (shape.back() + (random.nextInt (5) - 2));   // -2..+2 steps through the key
            auto anchor = start;
            for (const auto& s : spans)
                for (auto t = s.start; t < s.end - 1.0e-9; t += beatsPerBar)
                {
                    const auto tones = s.chord.pitchClasses();
                    std::vector<int> chordTones (tones.begin(), tones.end());
                    anchor = nearestOf (chordTones, anchor, melodyLow + 2, melodyHigh - 7);
                    const auto av = availableNotes (s.chord, s.key);
                    for (size_t i = 0; i < rhythm.size(); ++i)
                    {
                        const auto at = t + rhythm[i].at * beatsPerBar / 4.0;
                        if (at >= s.end - 1.0e-9)
                            continue;
                        auto pitch = anchor;
                        for (auto k = 0; k < std::abs (shape[i]); ++k)
                            pitch = stepFrom (pitch, shape[i] > 0 ? 1 : -1, av, melodyLow, melodyHigh);
                        if (rhythm[i].strong)
                            pitch = nearestOf (chordTones, pitch, melodyLow, melodyHigh);
                        add (idea.notes, at - origin, juce::jmin (rhythm[i].length * beatsPerBar / 4.0, s.end - at), pitch, i == 0 ? 0.88f : 0.74f);
                    }
                }
            cadence (idea.notes, spans);
        }
        {
            // The chord's notes up and down on eighths, from where the last chord left off.
            auto& idea = make ("Arpeggio", "The chord's notes up and down on eighth notes, each chord picked up where the last left off");
            auto prev = start;
            for (const auto& s : spans)
            {
                const auto tones = s.chord.pitchClasses();
                std::vector<int> chordTones (tones.begin(), tones.end());
                auto pitch = nearestOf (chordTones, prev, melodyLow, melodyHigh - 12);
                auto dir = variation % 2 == 0 ? 1 : -1;
                auto count = 0;
                for (auto t = s.start; t < s.end - 1.0e-9; t += 0.5)
                {
                    add (idea.notes, t - origin, juce::jmin (0.5, s.end - t), pitch, count % 4 == 0 ? 0.85f : 0.7f);
                    if (++count % 3 == 0)
                        dir = -dir;   // (up three, down three: a wave)
                    pitch = stepFrom (pitch, dir, chordTones, melodyLow, melodyHigh);
                }
                prev = pitch;
            }
            cadence (idea.notes, spans);
        }
        {
            // Bar by bar in pairs: a question rising to a held chord tone, an answer falling home.
            auto& idea = make ("Call and response", "A rising phrase held open, then an answer falling back home, bar by bar in pairs");
            const Rhythm call { { 0, .5, true }, { .5, .5, false }, { 1, .5, false }, { 1.5, 2, false } };
            const Rhythm answer { { 0, .5, true }, { .5, .5, false }, { 1, 1, false }, { 2, 2, true } };
            auto prev = start;
            auto barIndex = 0;
            for (const auto& s : spans)
                for (auto t = s.start; t < s.end - 1.0e-9; t += beatsPerBar, ++barIndex)
                {
                    const auto isCall = barIndex % 2 == 0;
                    const auto& rhythm = isCall ? call : answer;
                    const auto tones = s.chord.pitchClasses();
                    std::vector<int> chordTones (tones.begin(), tones.end());
                    const auto av = availableNotes (s.chord, s.key);
                    prev = nearestOf (chordTones, prev + (isCall ? -2.0 : 3.0), melodyLow, melodyHigh);
                    for (size_t i = 0; i < rhythm.size(); ++i)
                    {
                        const auto at = t + rhythm[i].at * beatsPerBar / 4.0;
                        if (at >= s.end - 1.0e-9)
                            continue;
                        if (i > 0)
                            prev = stepFrom (prev, isCall ? 1 : -1, av, melodyLow, melodyHigh);
                        const auto last = i + 1 == rhythm.size();
                        const auto pitch = last ? (isCall ? nearestOf (chordTones, prev, melodyLow, melodyHigh)
                                                          : nearestPitch (s.chord.root, prev, melodyLow, melodyHigh))
                                                : prev;
                        prev = pitch;
                        add (idea.notes, at - origin, juce::jmin (rhythm[i].length * beatsPerBar / 4.0, s.end - at), pitch, i == 0 ? 0.86f : 0.74f);
                    }
                }
            cadence (idea.notes, spans);
        }
        {
            auto& idea = make ("Syncopated pop", "Pushed off the beat, with the next chord's note arriving early (modern pop, R&B)");
            fromRhythm (idea, variation % 2 == 0 ? Rhythm { { 0, .75, true }, { .75, .75, false }, { 1.5, 1, true }, { 3, .5, false }, { 3.5, .5, false } }
                                                 : Rhythm { { .5, .5, true }, { 1, .75, false }, { 1.75, .75, false }, { 2.5, 1, true }, { 3.5, .5, false } });
            // The anticipation: a note in a chord's last half beat already on the next chord's tone.
            for (auto& n : idea.notes)
                for (size_t s = 0; s + 1 < spans.size(); ++s)
                {
                    const auto end = spans[s].end - origin;
                    if (n.beat >= end - 0.5 - 1.0e-9 && n.beat < end - 1.0e-9 && n.beat >= spans[s].start - origin)
                    {
                        const auto tones = spans[s + 1].chord.pitchClasses();
                        n.pitch = nearestOf (std::vector<int> (tones.begin(), tones.end()), n.pitch, melodyLow, melodyHigh);
                    }
                }
        }
        return ideas;
    }
}
