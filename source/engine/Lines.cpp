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

//==============================================================================
// Piano chords, guitar rhythm and lead (0.19.0, the owner asked: ideas "geared towards
// guitars in one category and piano in another"; rock and metal guitar).
namespace pr7s
{
    namespace
    {
        std::vector<int> tonesOf (const Chord& c)
        {
            const auto t = c.pitchClasses();
            return { t.begin(), t.end() };
        }

        // The notes of a scale built on `tonic` from these steps (pitch classes).
        std::vector<int> scaleOn (int tonic, std::initializer_list<int> steps)
        {
            std::vector<int> out;
            for (const auto s : steps)
                out.push_back (pcOf (tonic + s));
            return out;
        }

        // Each span's chord voiced in the right hand (close, the least movement from the one before).
        std::vector<std::vector<int>> rightHand (const std::vector<ChordSpan>& spans, int low)
        {
            std::vector<std::vector<int>> out;
            std::vector<int> previous;
            for (const auto& s : spans)
            {
                const auto voicings = voicingsFor (s.chord, low);
                auto notes = voicings.empty() ? std::vector<int> { 60 } : voicings[(size_t) closestVoicing (voicings, previous, {})];
                out.push_back (notes);
                previous = notes;
            }
            return out;
        }

        // The chord's own fifth above `root` (a diminished chord's flat fifth, an augmented one's
        // sharp fifth; a perfect fifth when it has none, as a sus or slash chord may not).
        int fifthAbove (const Chord& c, int root)
        {
            const auto tones = tonesOf (c);
            for (const auto step : { 7, 6, 8 })
                if (has (tones, pcOf (root + step)))
                    return root + step;
            return root + 7;
        }

        void addChord (std::vector<LineNote>& notes, double at, double length, const std::vector<int>& pitches, float velocity, double strum = 0.0)
        {
            for (size_t i = 0; i < pitches.size(); ++i)
                add (notes, at + strum * (double) i, juce::jmax (0.05, length - strum * (double) i), pitches[i], velocity);
        }
    }

    const std::vector<GuitarTuning>& GuitarTuning::all()
    {
        static const std::vector<GuitarTuning> tunings {
            { "7-string drop A", { 33, 40, 45, 50, 55, 59, 64 } },
            { "7-string standard (B)", { 35, 40, 45, 50, 55, 59, 64 } },
            { "6-string standard (E)", { 40, 45, 50, 55, 59, 64 } },
            { "6-string drop D", { 38, 45, 50, 55, 59, 64 } },
            { "6-string drop C", { 36, 43, 48, 53, 57, 62 } },
            { "8-string standard (F#)", { 30, 35, 40, 45, 50, 55, 59, 64 } },
            { "8-string drop E", { 28, 35, 40, 45, 50, 55, 59, 64 } } };
        return tunings;
    }

    const GuitarTuning& GuitarTuning::named (const juce::String& name)
    {
        for (const auto& t : all())
            if (t.name == name)
                return t;
        return all().front();
    }

    //==========================================================================
    std::vector<LineIdea> pianoChordIdeas (const std::vector<ChordSpan>& spans, double beatsPerBar, int variation)
    {
        std::vector<LineIdea> ideas;
        if (spans.empty())
            return ideas;
        const auto origin = spans.front().start;
        const auto make = [&] (const char* style, const char* description) -> LineIdea&
        {
            ideas.push_back ({ LineIdea::Part::chords, style, description, {} });
            return ideas.back();
        };
        const auto right = rightHand (spans, variation % 2 == 0 ? 55 : 60);
        std::vector<int> lefts;   // each chord's root (or slash bass) for the left hand, near the last
        {
            double prev = 41.0;
            for (const auto& s : spans)
            {
                const auto n = nearestPitch (bassOf (s.chord), (prev * 2.0 + 41.0) / 3.0, 33, 52);
                lefts.push_back (n);
                prev = n;
            }
        }
        const auto bars = [&] (auto&& each)   // each bar of each chord: (span index, its start in the song, its length)
        {
            for (size_t s = 0; s < spans.size(); ++s)
                for (auto t = spans[s].start; t < spans[s].end - 1.0e-9; t += beatsPerBar)
                    each (s, t, juce::jmin (beatsPerBar, spans[s].end - t));
        };
        {
            auto& idea = make ("Block chords", "The whole chord held, each voiced to move as little as it can from the last (hymns, ballads, pads)");
            bars ([&] (size_t s, double t, double len)
            {
                if (variation % 2 == 1 && len > beatsPerBar * 0.75)
                {
                    addChord (idea.notes, t - origin, len * 0.5, right[s], 0.78f);
                    addChord (idea.notes, t - origin + len * 0.5, len * 0.5, right[s], 0.7f);
                }
                else
                    addChord (idea.notes, t - origin, len, right[s], 0.78f);
                add (idea.notes, t - origin, len, lefts[s], 0.8f);
            });
        }
        {
            auto& idea = make ("Broken chords", "The root held low, the chord's notes low, high, middle, high on eighth notes above it (the classical Alberti pattern)");
            bars ([&] (size_t s, double t, double len)
            {
                add (idea.notes, t - origin, len, lefts[s], 0.76f);
                const auto& v = right[s];
                const int order[] = { 0, (int) v.size() - 1, (int) v.size() / 2, (int) v.size() - 1 };
                auto i = 0;
                for (auto at = 0.0; at < len - 1.0e-9; at += 0.5, ++i)
                    add (idea.notes, t - origin + at, 0.5, v[(size_t) order[i % 4]], i % 4 == 0 ? 0.8f : 0.65f);
            });
        }
        {
            auto& idea = make ("Ballad arpeggio", "The root low and held, then the chord rising on eighth notes above it (piano ballads)");
            bars ([&] (size_t s, double t, double len)
            {
                add (idea.notes, t - origin, len, lefts[s], 0.82f);
                const auto root = lefts[s];
                const auto tones = tonesOf (spans[s].chord);
                const auto fifth = fifthAbove (spans[s].chord, root);
                std::vector<int> up { fifth, root + 12 };
                auto third = nearestOf (tones, root + 16, root + 13, root + 24);
                up.push_back (third);
                up.push_back (fifth + 12);
                std::sort (up.begin(), up.end());
                auto i = 0;
                for (auto at = 0.5; at < len - 1.0e-9; at += 0.5, ++i)
                {
                    const auto step = i % 6;   // up four, down two: a gentle wave
                    const auto index = step < 4 ? step : 6 - step;
                    add (idea.notes, t - origin + at, 0.5, up[(size_t) juce::jlimit (0, 3, index)], 0.62f);
                }
            });
        }
        {
            auto& idea = make ("Stride", "A low bass note on one and three, the chord on two and four (ragtime, old-school piano)");
            bars ([&] (size_t s, double t, double len)
            {
                for (auto beat = 0.0; beat < len - 1.0e-9; beat += 1.0)
                {
                    const auto onBass = std::fmod (beat, 2.0) < 1.0e-9;
                    if (onBass)
                        add (idea.notes, t - origin + beat, 0.9, std::fmod (beat, 4.0) < 1.0e-9 ? lefts[s] : fifthAbove (spans[s].chord, lefts[s]), 0.82f);
                    else
                        addChord (idea.notes, t - origin + beat, 0.8, right[s], 0.7f);
                }
            });
        }
        {
            auto& idea = make ("Octaves and chords", "Left-hand octaves held, the chord pulsing on every beat (rock and pop piano)");
            bars ([&] (size_t s, double t, double len)
            {
                add (idea.notes, t - origin, len, lefts[s], 0.85f);
                add (idea.notes, t - origin, len, lefts[s] + 12, 0.8f);
                for (auto beat = 0.0; beat < len - 1.0e-9; beat += 1.0)
                    addChord (idea.notes, t - origin + beat, 0.9, right[s], std::fmod (beat, 2.0) < 1.0e-9 ? 0.78f : 0.66f);
            });
        }
        {
            auto& idea = make ("Pop pulse", "Eighth-note chords, the off-beats softer, over the root (modern pop)");
            bars ([&] (size_t s, double t, double len)
            {
                add (idea.notes, t - origin, len, lefts[s], 0.8f);
                auto i = 0;
                for (auto at = 0.0; at < len - 1.0e-9; at += 0.5, ++i)
                    addChord (idea.notes, t - origin + at, 0.45, right[s], i % 2 == 0 ? 0.74f : 0.56f);
            });
        }
        return ideas;
    }

    //==========================================================================
    std::vector<LineIdea> guitarRhythmIdeas (const std::vector<ChordSpan>& spans, double beatsPerBar, int variation, const GuitarTuning& tuning)
    {
        std::vector<LineIdea> ideas;
        if (spans.empty())
            return ideas;
        const auto origin = spans.front().start;
        namespace V = GuitarVelocity;
        const auto low = tuning.lowest(), high = tuning.highest();
        const auto make = [&] (const char* style, const char* description) -> LineIdea&
        {
            ideas.push_back ({ LineIdea::Part::rhythm, style, description, {} });
            return ideas.back();
        };
        // Each chord's root on the lowest string (its first twelve frets), near the one before.
        const auto roots = [&] (int from, int span)
        {
            std::vector<int> r;
            double prev = from + 3.0 + (variation % 3);
            for (const auto& s : spans)
            {
                const auto n = nearestPitch (bassOf (s.chord), prev, from, from + span);
                r.push_back (n);
                prev = n;
            }
            return r;
        };
        const auto lowRoots = roots (low, 11);
        // Root and fifth (and the octave): no third, so one shape fits major and minor.
        const auto powerOf = [] (const Chord& c, int root, bool octave)
        {
            const auto fifth = fifthAbove (c, root);
            return octave ? std::vector<int> { root, fifth, root + 12 } : std::vector<int> { root, fifth };
        };
        const auto each = [&] (const Rhythm& bar, auto&& note)
        {
            for (const auto& o : place (spans, scaled (bar, beatsPerBar), beatsPerBar))
                note (o);
        };
        {
            auto& idea = make ("Power chords", "Root, fifth and octave on the low strings, held or pushed (rock, punk, metal)");
            each (variation % 2 == 0 ? Rhythm { { 0, 2, true }, { 2, 2, true } } : Rhythm { { 0, 1.5, true }, { 1.5, 1, false }, { 2.5, 1.5, false } },
                  [&] (const Onset& o) { addChord (idea.notes, o.at, o.length, powerOf (spans[o.span].chord, lowRoots[o.span], true), V::of (o.strong ? V::sustainHard : V::sustain)); });
        }
        {
            // An octave shape higher up (root on the third string up from the bottom, give or take).
            const auto mid = roots (juce::jmin (high - 24, low + 12), 11);
            auto& idea = make ("Octave chords", "The root and its octave on eighth notes, higher up the neck (indie and alternative rock)");
            each (variation % 2 == 0 ? Rhythm { { 0, .5, true }, { .5, .5, false }, { 1, .5, false }, { 1.5, .5, false }, { 2, .5, true }, { 2.5, .5, false }, { 3, .5, false }, { 3.5, .5, false } }
                                     : Rhythm { { 0, .75, true }, { .75, .75, false }, { 1.5, 1, false }, { 2.5, .5, false }, { 3, 1, false } },
                  [&] (const Onset& o) { addChord (idea.notes, o.at, o.length, { mid[o.span], mid[o.span] + 12 }, V::of (o.strong ? V::sustain + 8 : V::sustainSoft + 4)); });
        }
        {
            auto& idea = make ("Palm-muted chugs", "Palm-muted root-and-fifth chugs, a full chord ringing open on the strong beats (metal, hard rock)");
            const Rhythm eighths { { 0, .5, true }, { .5, .5, false }, { 1, .5, false }, { 1.5, .5, false }, { 2, .5, true }, { 2.5, .5, false }, { 3, .5, false }, { 3.5, .5, false } };
            Rhythm sixteenths;
            for (auto i = 0; i < 16; ++i)
                sixteenths.push_back ({ i * 0.25, 0.25, i % 8 == 0 });
            each (variation % 2 == 0 ? eighths : sixteenths, [&] (const Onset& o)
            {
                // The accents open (sustained), the chugs between palm muted: tighter on sixteenths.
                if (o.strong)
                    addChord (idea.notes, o.at, o.length * 1.6, powerOf (spans[o.span].chord, lowRoots[o.span], true), V::of (V::sustainHard));
                else
                    addChord (idea.notes, o.at, o.length * 0.8, powerOf (spans[o.span].chord, lowRoots[o.span], false),
                              V::of (o.length < 0.3 ? V::muteTight : V::muteMid));
            });
        }
        {
            auto& idea = make ("Gallop", "An eighth and two sixteenths on every beat, palm muted: the galloping rhythm (classic heavy metal)");
            Rhythm gallop;
            for (auto beat = 0; beat < 4; ++beat)
            {
                gallop.push_back ({ beat + 0.0, 0.5, beat % 2 == 0 });
                gallop.push_back ({ beat + 0.5, 0.25, false });
                gallop.push_back ({ beat + 0.75, 0.25, false });
            }
            // Palm muted throughout (the eighth looser than the two sixteenths); the odd variations
            // open the bar's first hit as a ringing power chord.
            each (gallop, [&] (const Onset& o)
            {
                const auto barStart = std::fmod (o.at + origin - spans[o.span].start, beatsPerBar) < 1.0e-9;
                if (barStart && variation % 2 == 1)
                    addChord (idea.notes, o.at, o.length * 0.9, powerOf (spans[o.span].chord, lowRoots[o.span], true), V::of (V::sustainHard));
                else
                    addChord (idea.notes, o.at, o.length * 0.85, powerOf (spans[o.span].chord, lowRoots[o.span], false),
                              V::of (o.length > 0.3 ? V::muteLoose : V::muteMid));
            });
        }
        {
            // The key's tonic on the lowest string as a pedal, the chord's power chord above it.
            auto& idea = make ("Pedal-point riff", "The key's low note palm-muted under the chord's power chord, a pinch harmonic every second bar (thrash, djent, metalcore)");
            const auto upper = roots (low + 5, 11);
            const Rhythm riff { { 0, .5, true }, { .5, .5, false }, { 1, .5, false }, { 1.5, .5, false }, { 2, .5, false }, { 2.5, .5, false }, { 3, .5, false }, { 3.5, .5, false } };
            const bool chordOn[2][8] = { { false, false, true, false, false, true, false, true }, { false, true, false, false, true, false, true, true } };
            each (riff, [&] (const Onset& o)
            {
                const auto& s = spans[o.span];
                const auto pedal = nearestPitch (s.key.pitchClass(), low, low, low + 11);
                const auto step = (int) std::round (std::fmod (o.at + origin - s.start, beatsPerBar) / 0.5) % 8;
                // The pedal palm muted (the bar's first looser), the stabs open; every second bar's
                // last stab a pinch harmonic on its root (the metal "squeal").
                const auto bar = (int) std::floor ((o.at + 1.0e-9) / beatsPerBar);
                if (step == 7 && bar % 2 == 1)
                    add (idea.notes, o.at, o.length * 0.95, upper[o.span], V::of (V::pinch));
                else if (chordOn[variation % 2][step] && step != 0)
                    addChord (idea.notes, o.at, o.length * 0.9, powerOf (s.chord, upper[o.span], false), V::of (V::sustainHard));
                else
                    add (idea.notes, o.at, o.length * 0.8, pedal, V::of (step == 0 ? V::muteLoose : V::muteTight));
            });
        }
        {
            // Six strings (the top six) fretted as a barre shape from the root: the chord as a guitar holds it.
            auto& idea = make ("Open strummed shapes", "Full chord shapes as a guitar holds them, strummed down, down-up, up-down-up");
            const auto six = std::vector<int> (tuning.strings.end() - 6, tuning.strings.end());
            const auto shape = [&] (const Chord& chord)
            {
                const auto tones = tonesOf (chord);
                auto first = 0;   // the string the root is on: the lowest of the six, else the next
                auto fret = pcOf (bassOf (chord) - six[0]);
                if (fret > 7)
                {
                    first = 1;
                    fret = pcOf (bassOf (chord) - six[1]);
                }
                std::vector<int> notes;
                for (auto s = first; s < 6; ++s)
                {
                    if (s == first)
                    {
                        notes.push_back (six[(size_t) s] + fret);
                        continue;
                    }
                    // The note under the barre (or up to three frets above it) that's in the chord.
                    for (auto f = fret; f <= fret + 3; ++f)
                        if (has (tones, pcOf (six[(size_t) s] + f)))
                        {
                            notes.push_back (six[(size_t) s] + f);
                            break;
                        }
                }
                return notes;
            };
            struct Stroke { double at, length; bool down; };
            const std::vector<Stroke> pattern = variation % 2 == 0
                ? std::vector<Stroke> { { 0, 1, true }, { 1, .5, true }, { 1.5, .5, false }, { 2.5, .5, false }, { 3, .5, true }, { 3.5, .5, false } }
                : std::vector<Stroke> { { 0, 1.5, true }, { 1.5, 1, true }, { 2.5, .5, false }, { 3, 1, true } };
            for (size_t s = 0; s < spans.size(); ++s)
            {
                const auto notes = shape (spans[s].chord);
                for (auto t = spans[s].start; t < spans[s].end - 1.0e-9; t += beatsPerBar)
                    for (const auto& st : pattern)
                    {
                        const auto at = t + st.at * beatsPerBar / 4.0;
                        if (at >= spans[s].end - 1.0e-9)
                            continue;
                        const auto len = juce::jmin (st.length * beatsPerBar / 4.0, spans[s].end - at);
                        // Down: low to high, every string; up: high to low, the top four.
                        std::vector<int> strummed = notes;
                        if (! st.down)
                        {
                            strummed.assign (notes.end() - juce::jmin<std::ptrdiff_t> (4, (std::ptrdiff_t) notes.size()), notes.end());
                            std::reverse (strummed.begin(), strummed.end());
                        }
                        addChord (idea.notes, at - origin, len, strummed, V::of (st.down ? V::sustain : V::sustainSoft), 0.02);
                    }
            }
        }
        // Nothing under the lowest string (a library's keyswitches live there) nor past the 24th fret.
        for (auto& idea : ideas)
            for (auto& n : idea.notes)
                while (n.pitch < low)
                    n.pitch += 12;
        for (auto& idea : ideas)
            for (auto& n : idea.notes)
                while (n.pitch > high)
                    n.pitch -= 12;
        for (auto& idea : ideas)
            std::stable_sort (idea.notes.begin(), idea.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        return ideas;
    }

    //==========================================================================
    std::vector<LineIdea> guitarLeadIdeas (const std::vector<ChordSpan>& spans, double beatsPerBar, int variation, const GuitarTuning& tuning)
    {
        std::vector<LineIdea> ideas;
        if (spans.empty())
            return ideas;
        const auto origin = spans.front().start;
        // A lead's range: from the second string's fifth fret to the top string's 22nd.
        const auto low = juce::jmax (tuning.strings[1] + 5, 45), high = tuning.strings.back() + 22;
        const auto centre = (low + high) / 2 - 2;
        namespace V = GuitarVelocity;
        juce::Random random (0x1ead + variation * 104729);
        const auto make = [&] (const char* style, const char* description) -> LineIdea&
        {
            ideas.push_back ({ LineIdea::Part::lead, style, description, {} });
            return ideas.back();
        };
        // The scales a rock or metal lead uses, on the key's tonic (a major key: its relative minor's
        // notes for the minor pentatonic and blues: the same notes as the major pentatonic plus colour).
        const auto pentatonicOf = [] (const Key& k) { return k.minor ? scaleOn (k.pitchClass(), { 0, 3, 5, 7, 10 }) : scaleOn (k.pitchClass(), { 0, 2, 4, 7, 9 }); };
        const auto bluesOf = [] (const Key& k) { return k.minor ? scaleOn (k.pitchClass(), { 0, 3, 5, 6, 7, 10 }) : scaleOn (k.pitchClass(), { 0, 2, 3, 4, 7, 9 }); };
        const auto neoOf = [] (const Key& k) { return k.minor ? scaleOn (k.pitchClass(), { 0, 2, 3, 5, 7, 8, 11 }) : scaleOn (k.pitchClass(), { 0, 2, 4, 5, 7, 9, 11 }); };
        // Bar by bar: the bar's first note on a chord tone near `prev`, then `fill` writes the rest.
        struct Bar { size_t span; double start, length; };
        std::vector<Bar> barsList;
        for (size_t s = 0; s < spans.size(); ++s)
            for (auto t = spans[s].start; t < spans[s].end - 1.0e-9; t += beatsPerBar)
                barsList.push_back ({ s, t - origin, juce::jmin (beatsPerBar, spans[s].end - t) });
        // A rake into a phrase's first note: every second bar's first, from the first (0.19.2).
        const auto raked = [&] (const Bar& b) { return ((&b - barsList.data()) % 2) == 0; };
        const auto target = [&] (const Bar& b, int prev) { return nearestOf (tonesOf (spans[b.span].chord), prev, low, high); };
        const auto lastOnChord = [&] (LineIdea& idea)
        {
            if (idea.notes.empty())
                return;
            auto& last = idea.notes.back();
            last.pitch = nearestOf (tonesOf (spans.back().chord), last.pitch, low, high);
            last.length = juce::jmax (last.length, spans.back().end - origin - last.beat);
        };
        {
            auto& idea = make ("Pentatonic box", "Raked into, up and down a pentatonic box, on the chord each bar, ending on a pinch-harmonic squeal (rock solos)");
            auto prev = centre;
            for (const auto& b : barsList)
            {
                const auto scale = pentatonicOf (spans[b.span].key);
                auto pitch = target (b, prev);
                auto dir = variation % 2 == 0 ? 1 : -1;
                auto i = 0;
                for (auto at = 0.0; at < b.length - 1.0e-9; at += 0.5, ++i)
                {
                    const auto len = (at + 0.5 >= b.length - 1.0e-9) ? b.length - at : 0.5;
                    add (idea.notes, b.start + at, i == 0 ? 0.5 : len, pitch, V::of (i == 0 ? (raked (b) ? V::rake : V::sustainHard) : V::sustain - 6));
                    if (i % 3 == 2)
                        dir = -dir;
                    pitch = stepFrom (pitch, dir, scale, juce::jmax (low, centre - 9), juce::jmin (high, centre + 12));
                }
                prev = pitch;
            }
            lastOnChord (idea);
            idea.notes.back().velocity = V::of (V::pinch);   // (held to the end: a pinch-harmonic squeal)
        }
        {
            auto& idea = make ("Blues scale run", "Raked into, down the blues scale in triplets through its \"blue\" note, landing on the chord (blues rock)");
            auto prev = centre + 7;
            for (const auto& b : barsList)
            {
                const auto scale = bluesOf (spans[b.span].key);
                auto pitch = target (b, prev);
                auto i = 0;
                for (auto at = 0.0; at < b.length - 1.0e-9; at += 1.0 / 3.0, ++i)
                {
                    const auto restOfBar = at + 1.0 / 3.0 >= b.length - 1.0e-9;
                    add (idea.notes, b.start + at, restOfBar ? b.length - at : 1.0 / 3.0, pitch, V::of (i == 0 && raked (b) ? V::rake : i % 3 == 0 ? V::sustainHard - 4 : V::sustainSoft + 8));
                    pitch = stepFrom (pitch, (i / 6) % 2 == 0 ? -1 : 1, scale, low, high);
                }
                prev = pitch;
            }
            lastOnChord (idea);
        }
        {
            // A three-note cell from the pentatonic, repeated a step lower each beat.
            auto& idea = make ("Repeating triplet lick", "Raked into, a three-note pentatonic cell in triplets, a step lower every beat (the classic rock run)");
            auto prev = centre + 9;
            for (const auto& b : barsList)
            {
                const auto scale = pentatonicOf (spans[b.span].key);
                auto top = target (b, prev);
                for (auto beat = 0.0; beat < b.length - 1.0e-9; beat += 1.0)
                {
                    const auto second = stepFrom (top, -1, scale, low, high);
                    const auto third = stepFrom (second, -1, scale, low, high);
                    const int cell[3] = { top, second, variation % 2 == 0 ? third : top };
                    for (auto k = 0; k < 3; ++k)
                        add (idea.notes, b.start + beat + k / 3.0, 1.0 / 3.0, cell[k], V::of (k == 0 ? (&b == &barsList.front() && beat == 0.0 ? V::rake : V::sustainHard) : V::sustain - 6));
                    top = stepFrom (top, -1, scale, low + 4, high);
                }
                prev = top;
            }
            lastOnChord (idea);
        }
        {
            // Groups of four sixteenths, each group starting a step below the last.
            auto& idea = make ("Descending fours", "Groups of four sixteenth notes, each group starting a scale step lower (a metal sequence)");
            auto prev = high - 6;
            for (const auto& b : barsList)
            {
                const auto scale = availableNotes (spans[b.span].chord, spans[b.span].key);
                auto start = target (b, prev);
                for (auto beat = 0.0; beat < b.length - 1.0e-9; beat += 1.0)
                {
                    auto pitch = start;
                    for (auto k = 0; k < 4; ++k)
                    {
                        add (idea.notes, b.start + beat + k * 0.25, 0.25, pitch, V::of (k == 0 ? V::sustainHard : V::sustain - 4));
                        pitch = stepFrom (pitch, -1, scale, low, high);
                    }
                    start = stepFrom (start, -1, scale, low + 6, high);
                }
                prev = start + 7;
            }
            lastOnChord (idea);
        }
        {
            // Up the chord's notes, then down the (harmonic) minor scale, in sixteenths.
            auto& idea = make ("Neoclassical run", "Up the chord in an arpeggio, then down the harmonic minor scale in sixteenths (shred, neoclassical metal)");
            auto prev = centre - 3;
            for (const auto& b : barsList)
            {
                const auto& s = spans[b.span];
                auto scale = neoOf (s.key);
                // The raised seventh (G# in A minor) is the sound of it, but not over a chord holding
                // the natural one (C major's G): there the natural minor's seventh instead.
                if (s.key.minor && has (tonesOf (s.chord), pcOf (s.key.pitchClass() + 10)))
                    for (auto& n : scale)
                        if (n == pcOf (s.key.pitchClass() + 11))
                            n = pcOf (s.key.pitchClass() + 10);
                for (const auto t : tonesOf (s.chord))   // (the chord's notes always in it: V's raised seventh)
                    if (! has (scale, t))
                        scale.push_back (t);
                auto pitch = target (b, prev);
                const auto tones = tonesOf (s.chord);
                const auto half = b.length * 0.5;
                for (auto at = 0.0; at < b.length - 1.0e-9; at += 0.25)
                {
                    add (idea.notes, b.start + at, 0.25, pitch, V::of (std::fmod (at, 1.0) < 1.0e-9 ? V::sustainHard : V::sustain - 6));
                    pitch = at < half - 1.0e-9 ? stepFrom (pitch, 1, tones, low, high) : stepFrom (pitch, -1, scale, low, high);
                }
                prev = pitch;
            }
            lastOnChord (idea);
        }
        {
            // The same note picked on every sixteenth for a beat; the line moves by chord tones and steps.
            auto& idea = make ("Tremolo picking", "One note picked on every sixteenth, the line moving through the chords (black metal, post-rock)");
            auto prev = centre;
            for (const auto& b : barsList)
            {
                const auto& s = spans[b.span];
                auto pitch = target (b, prev);
                const auto av = availableNotes (s.chord, s.key);
                for (auto beat = 0.0; beat < b.length - 1.0e-9; beat += 1.0)
                {
                    for (auto k = 0; k < 4 && beat + k * 0.25 < b.length - 1.0e-9; ++k)
                        add (idea.notes, b.start + beat + k * 0.25, 0.25, pitch, V::of (k == 0 ? V::sustain + 8 : V::sustainSoft + 8));
                    pitch = stepFrom (pitch, random.nextBool() ? 1 : -1, av, low, high);
                }
                prev = pitch;
            }
            lastOnChord (idea);
        }
        return ideas;
    }
}
