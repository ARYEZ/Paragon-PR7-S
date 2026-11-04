#include "Patterns.h"

#include <cmath>

namespace pr7s
{
    namespace
    {
        using S = PatternStep;
        constexpr float soft = 0.6f, mid = 0.72f, accent = 0.86f;

        // A run of eighths (or sixteenths) through voices, accents on the beats.
        std::vector<PatternStep> run (std::initializer_list<int> voices, double step, double length = -1.0)
        {
            std::vector<PatternStep> steps;
            auto beat = 0.0;
            for (const auto v : voices)
            {
                const auto onBeat = std::abs (beat - std::round (beat)) < 1.0e-9;
                steps.push_back ({ beat, length > 0.0 ? length : step, v, 0, beat == 0.0 ? accent : onBeat ? mid : soft, 0 });
                beat += step;
            }
            return steps;
        }

        std::vector<Pattern> makePatterns()
        {
            std::vector<Pattern> p;
            const auto add = [&p] (const char* group, const char* name, const char* description, std::vector<PatternStep> steps, double bars = 1.0)
            {
                p.push_back ({ name, group, description, bars, std::move (steps) });
            };
            // ARPEGGIOS: the chord's notes one at a time.
            add ("ARPEGGIOS", "Up", "Eighths climbing through the chord and on into the next octave.", run ({ 0, 1, 2, 3, 4, 5, 6, 7 }, 0.5));
            add ("ARPEGGIOS", "Down", "Eighths falling from two octaves up.", run ({ 7, 6, 5, 4, 3, 2, 1, 0 }, 0.5));
            add ("ARPEGGIOS", "Up and down", "Up the chord and back, in eighths.", run ({ 0, 1, 2, 3, 4, 3, 2, 1 }, 0.5));
            add ("ARPEGGIOS", "Sixteenths", "A busy sixteenth-note arpeggio over a held bass.",
                 [] { auto s = run ({ 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3 }, 0.25); s.push_back ({ 0.0, 4.0, S::bass, 0, mid, 0 }); return s; }());
            add ("ARPEGGIOS", "Broken thirds", "Each note then the one two above: 1-3, 2-4...", run ({ 0, 2, 1, 3, 2, 4, 3, 5 }, 0.5));
            add ("ARPEGGIOS", "Alberti", "Low, high, middle, high: the classical left hand.", run ({ 0, 2, 1, 2, 0, 2, 1, 2 }, 0.5));

            // STRUMS & PULSES: the whole chord, in rhythm.
            add ("STRUMS & PULSES", "Held", "The chord held for the bar over its root.",
                 { { 0.0, 4.0, S::all, 0, mid, 0 }, { 0.0, 4.0, S::bass, 0, mid, 0 } });
            add ("STRUMS & PULSES", "Quarter pulse", "The chord on every beat, the root on one and three.",
                 { { 0.0, 0.9, S::all, 0, accent, 0 }, { 1.0, 0.9, S::all, 0, soft, 0 }, { 2.0, 0.9, S::all, 0, mid, 0 }, { 3.0, 0.9, S::all, 0, soft, 0 },
                   { 0.0, 1.9, S::bass, 0, accent, 0 }, { 2.0, 1.9, S::bass, 0, mid, 0 } });
            add ("STRUMS & PULSES", "Eighth pulse", "Driving eighths, accented on the beats.",
                 [] { std::vector<PatternStep> s; for (int i = 0; i < 8; ++i) s.push_back ({ i * 0.5, 0.42, S::all, 0, i == 0 ? accent : i % 2 == 0 ? mid : soft, 0 });
                      s.push_back ({ 0.0, 4.0, S::bass, 0, mid, 0 }); return s; }());
            add ("STRUMS & PULSES", "Pop push", "Three, three, two: the chord pushed ahead of the beat.",
                 { { 0.0, 1.4, S::all, 0, accent, 0 }, { 1.5, 1.4, S::all, 0, mid, 0 }, { 3.0, 0.9, S::all, 0, mid, 0 },
                   { 0.0, 1.4, S::bass, 0, accent, 0 }, { 1.5, 1.4, S::bass, 0, mid, 0 }, { 3.0, 0.9, S::bass, 0, mid, 0 } });
            add ("STRUMS & PULSES", "Off-beat chops", "Short chords between the beats over a steady root.",
                 { { 0.5, 0.22, S::all, 0, mid, 0 }, { 1.5, 0.22, S::all, 0, mid, 0 }, { 2.5, 0.22, S::all, 0, mid, 0 }, { 3.5, 0.22, S::all, 0, mid, 0 },
                   { 0.0, 0.9, S::bass, 0, accent, 0 }, { 2.0, 0.9, S::bass, 0, mid, 0 } });
            add ("STRUMS & PULSES", "Strummed", "Down, down-up, down-up, up: a strummed bar.",
                 { { 0.0, 1.0, S::all, 0, accent, 1 }, { 1.0, 0.5, S::all, 0, mid, 1 }, { 1.5, 0.5, S::all, 0, soft, -1 },
                   { 2.0, 0.5, S::all, 0, mid, 1 }, { 2.5, 0.5, S::all, 0, soft, -1 }, { 3.5, 0.5, S::all, 0, soft, -1 } });

            // BALLAD & BROKEN: piano accompaniments.
            add ("BALLAD & BROKEN", "Ballad", "The root, then the chord rising in eighths, all held.",
                 { { 0.0, 4.0, S::bass, 0, accent, 0 }, { 0.5, 3.5, 0, 0, soft, 0 }, { 1.0, 3.0, 1, 0, mid, 0 }, { 1.5, 2.5, 2, 0, soft, 0 },
                   { 2.0, 2.0, 3, 0, mid, 0 }, { 2.5, 1.5, 2, 0, soft, 0 }, { 3.0, 1.0, 1, 0, mid, 0 }, { 3.5, 0.5, 2, 0, soft, 0 } });
            add ("BALLAD & BROKEN", "Oom-pah", "Root, chord, fifth, chord.",
                 { { 0.0, 0.9, S::bass, 0, accent, 0 }, { 1.0, 0.8, S::all, 0, soft, 0 }, { 2.0, 0.9, S::bassFifth, 0, mid, 0 }, { 3.0, 0.8, S::all, 0, soft, 0 } });
            add ("BALLAD & BROKEN", "Stride", "Low root, chord, octave-up fifth, chord: a striding left hand.",
                 { { 0.0, 0.9, S::bass, -1, accent, 0 }, { 1.0, 0.8, S::all, 0, mid, 0 }, { 2.0, 0.9, S::bassFifth, 0, mid, 0 }, { 3.0, 0.8, S::all, 0, mid, 0 } });
            add ("BALLAD & BROKEN", "Gospel", "Held chords with a push before three, over the root.",
                 { { 0.0, 1.5, S::all, 0, accent, 0 }, { 1.5, 0.45, S::all, 0, soft, 0 }, { 2.0, 1.5, S::all, 0, mid, 0 }, { 3.5, 0.45, S::all, 0, soft, 0 },
                   { 0.0, 2.0, S::bass, 0, accent, 0 }, { 2.0, 2.0, S::bass, 0, mid, 0 } });
            add ("BALLAD & BROKEN", "Rolled", "The chord rolled up from its root and held.",
                 { { 0.0, 4.0, S::bass, 0, mid, 0 }, { 0.12, 3.88, 0, 0, soft, 0 }, { 0.24, 3.76, 1, 0, soft, 0 }, { 0.36, 3.64, 2, 0, mid, 0 },
                   { 0.48, 3.52, 3, 0, mid, 0 } });
            add ("BALLAD & BROKEN", "Pad and bell", "A soft held chord with its top note ringing an octave up.",
                 { { 0.0, 4.0, S::all, 0, soft, 0 }, { 0.0, 4.0, S::bass, 0, soft, 0 }, { 0.0, 1.0, S::top, 1, mid, 0 }, { 1.5, 1.0, S::top, 1, soft, 0 },
                   { 3.0, 1.0, S::top, 1, soft, 0 } });

            // RHYTHMIC RIFFS: syncopated figures.
            add ("RHYTHMIC RIFFS", "Tresillo", "Three, three, two in short chords and roots.",
                 { { 0.0, 0.6, S::all, 0, accent, 0 }, { 1.5, 0.6, S::all, 0, mid, 0 }, { 3.0, 0.6, S::all, 0, mid, 0 },
                   { 0.0, 1.2, S::bass, 0, accent, 0 }, { 1.5, 1.2, S::bass, 0, mid, 0 }, { 3.0, 0.9, S::bass, 0, mid, 0 } });
            add ("RHYTHMIC RIFFS", "Synthwave", "Octave-jumping eighths in the bass under a held chord.",
                 [] { std::vector<PatternStep> s { { 0.0, 4.0, S::all, 0, soft, 0 } };
                      for (int i = 0; i < 8; ++i) s.push_back ({ i * 0.5, 0.42, S::bass, i % 2, i == 0 ? accent : mid, 0 }); return s; }());
            add ("RHYTHMIC RIFFS", "Gallop", "Eighth and two sixteenths on every beat.",
                 [] { std::vector<PatternStep> s; for (int b = 0; b < 4; ++b) { s.push_back ({ b + 0.0, 0.45, S::all, 0, b == 0 ? accent : mid, 0 });
                      s.push_back ({ b + 0.5, 0.22, S::all, 0, soft, 0 }); s.push_back ({ b + 0.75, 0.22, S::all, 0, soft, 0 }); }
                      s.push_back ({ 0.0, 4.0, S::bass, 0, mid, 0 }); return s; }());
            add ("RHYTHMIC RIFFS", "Funk stabs", "Short sixteenth stabs around the beat.",
                 { { 0.0, 0.2, S::all, 0, accent, 0 }, { 0.75, 0.2, S::all, 0, mid, 0 }, { 1.5, 0.2, S::all, 0, mid, 0 }, { 2.25, 0.2, S::all, 0, soft, 0 },
                   { 3.0, 0.2, S::all, 0, mid, 0 }, { 3.5, 0.2, S::all, 0, soft, 0 }, { 0.0, 0.45, S::bass, 0, accent, 0 }, { 2.5, 0.45, S::bass, 0, mid, 0 } });
            add ("RHYTHMIC RIFFS", "Driving octaves", "Eighth-note octaves in the bass, the chord on one and three.",
                 [] { std::vector<PatternStep> s { { 0.0, 1.8, S::all, 0, mid, 0 }, { 2.0, 1.8, S::all, 0, mid, 0 } };
                      for (int i = 0; i < 8; ++i) s.push_back ({ i * 0.5, 0.42, S::bass, i % 2 == 0 ? 0 : 1, i % 2 == 0 ? accent : mid, 0 }); return s; }());
            add ("RHYTHMIC RIFFS", "Montuno", "A two-bar syncopated figure over the root (Latin piano).",
                 { { 0.0, 0.45, 0, 0, accent, 0 }, { 0.5, 0.45, 2, 0, soft, 0 }, { 1.0, 0.45, 1, 0, mid, 0 }, { 1.5, 0.45, 3, 0, soft, 0 },
                   { 2.5, 0.45, 2, 0, mid, 0 }, { 3.0, 0.45, 1, 0, soft, 0 }, { 3.5, 0.45, 3, 0, mid, 0 },
                   { 4.5, 0.45, 3, 0, mid, 0 }, { 5.0, 0.45, 2, 0, soft, 0 }, { 5.5, 0.45, 1, 0, mid, 0 }, { 6.5, 0.45, 2, 0, soft, 0 }, { 7.0, 0.45, 0, 0, mid, 0 },
                   { 0.0, 1.5, S::bass, 0, accent, 0 }, { 2.5, 1.5, S::bass, 0, mid, 0 }, { 4.0, 1.5, S::bass, 0, accent, 0 }, { 6.5, 1.5, S::bass, 0, mid, 0 } },
                 2.0);
            return p;
        }

        // The notes a step plays on a chord (its voicing sorted low to high, its bass).
        std::vector<int> stepNotes (const PatternStep& step, const std::vector<int>& voicing, int bass)
        {
            std::vector<int> notes;
            if (voicing.empty())
                return notes;
            const auto n = (int) voicing.size();
            const auto shift = 12 * step.octave;
            if (step.voice >= 0)
                notes.push_back (voicing[(size_t) (step.voice % n)] + 12 * (step.voice / n) + shift);
            else if (step.voice == PatternStep::bass)
                notes.push_back (bass + shift);
            else if (step.voice == PatternStep::bassFifth)
                notes.push_back (bass + 7 + shift);
            else if (step.voice == PatternStep::top)
                notes.push_back (voicing.back() + shift);
            else
                for (const auto v : voicing)
                    notes.push_back (v + shift);
            if (step.strum < 0)
                std::reverse (notes.begin(), notes.end());   // (an up-stroke: from the top down)
            for (auto& p : notes)
                while (p > 127) p -= 12;
            return notes;
        }

        constexpr double strumGap = 0.02;   // beats between strummed strings (as the guitar ideas')
    }

    const std::vector<Pattern>& patterns()
    {
        static const auto all = makePatterns();
        return all;
    }

    const juce::StringArray& patternGroups()
    {
        static const juce::StringArray groups { "ARPEGGIOS", "STRUMS & PULSES", "BALLAD & BROKEN", "RHYTHMIC RIFFS" };
        return groups;
    }

    std::vector<int> patternsInGroup (const juce::String& group)
    {
        std::vector<int> found;
        for (int i = 0; i < (int) patterns().size(); ++i)
            if (patterns()[(size_t) i].group == group)
                found.push_back (i);
        return found;
    }

    int patternNamed (const juce::String& name)
    {
        for (int i = 0; i < (int) patterns().size(); ++i)
            if (patterns()[(size_t) i].name.equalsIgnoreCase (name))
                return i;
        return -1;
    }

    std::vector<LineNote> patternNotes (const Pattern& pattern, std::vector<int> voicing, int bassNote,
                                        double from, double to, double barOrigin, double beatsPerBar)
    {
        std::vector<LineNote> notes;
        if (voicing.empty() || to <= from)
            return notes;
        std::sort (voicing.begin(), voicing.end());
        if (bassNote < 0)
            bassNote = voicing.front() - 12;
        const auto scale = beatsPerBar / 4.0;
        const auto length = pattern.bars * beatsPerBar;
        const auto first = (int) std::floor ((from - barOrigin) / length + 1.0e-9);
        for (auto k = first; barOrigin + k * length < to - 1.0e-9; ++k)
            for (const auto& step : pattern.steps)
            {
                const auto played = stepNotes (step, voicing, bassNote);
                for (size_t i = 0; i < played.size(); ++i)
                {
                    const auto at = barOrigin + k * length + step.beat * scale + (step.strum != 0 ? (double) i * strumGap : 0.0);
                    if (at < from - 1.0e-9 || at >= to - 1.0e-9)
                        continue;
                    notes.push_back ({ at, juce::jmin (step.length * scale, to - at), played[i], step.velocity });
                }
            }
        std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        return notes;
    }

    std::vector<LineIdea> patternIdeas (const std::vector<ChordSpan>& spans, double beatsPerBar, const juce::String& group)
    {
        std::vector<LineIdea> ideas;
        if (spans.empty())
            return ideas;
        const auto origin = std::floor (spans.front().start / beatsPerBar + 1.0e-9) * beatsPerBar;
        for (const auto index : patternsInGroup (group))
        {
            const auto& pattern = patterns()[(size_t) index];
            LineIdea idea;
            idea.part = LineIdea::Part::pattern;
            idea.style = pattern.name;
            idea.description = pattern.description;
            std::vector<int> previous;
            for (const auto& span : spans)
            {
                const auto voicings = voicingsFor (span.chord, 55);
                if (voicings.empty())
                    continue;
                const auto& voicing = voicings[(size_t) closestVoicing (voicings, previous, {})];
                previous = voicing;
                auto bass = 36;
                while ((bass % 12) != ((span.chord.bass >= 0 ? span.chord.bass : span.chord.root) % 12))
                    ++bass;
                for (auto note : patternNotes (pattern, voicing, bass, span.start, span.end, origin, beatsPerBar))
                {
                    note.beat -= spans.front().start;
                    idea.notes.push_back (note);
                }
            }
            ideas.push_back (std::move (idea));
        }
        return ideas;
    }

    //==========================================================================
    PatternPerformer::PatternPerformer (std::function<void (const juce::MidiMessage&)> send) : sendTo (std::move (send)) {}

    PatternPerformer::~PatternPerformer()
    {
        stopTimer();
        stop();
    }

    void PatternPerformer::setPattern (int index)
    {
        if (! juce::isPositiveAndBelow (index, (int) patterns().size()))
            index = -1;
        if (pattern.exchange (index) != index)
            stop();
    }

    double PatternPerformer::now() const
    {
        if (songBeat)
            if (const auto s = songBeat(); s >= 0.0)
                return s;
        return (juce::Time::getMillisecondCounterHiRes() - startedMs) / 60000.0 * bpm.load();
    }

    void PatternPerformer::play (std::vector<int> voicing, juce::uint8 pressVelocity, int playChannel)
    {
        if (pattern.load() < 0 || voicing.empty())
            return;
        std::sort (voicing.begin(), voicing.end());
        {
            const juce::ScopedLock sl (lock);
            endDue (0.0, true);   // (a new chord: the old one's notes end; the pattern carries on in time)
            chord = std::move (voicing);
            velocity = pressVelocity;
            channel = playChannel;
            if (! active)
            {
                active = true;
                startedMs = juce::Time::getMillisecondCounterHiRes();
                const auto s = songBeat ? songBeat() : -1.0;
                freeClock = s < 0.0;
                lastBeat = now() - 1.0e-6;   // (a step right now plays)
            }
        }
        if (! isTimerRunning())
            startTimer (2);
    }

    void PatternPerformer::stop()
    {
        const juce::ScopedLock sl (lock);
        endDue (0.0, true);
        active = false;
        chord.clear();
    }

    bool PatternPerformer::isPlaying() const
    {
        const juce::ScopedLock sl (lock);
        return active;
    }

    std::vector<int> PatternPerformer::getSounding() const
    {
        const juce::ScopedLock sl (lock);
        std::vector<int> notes;
        for (const auto& s : sounding)
            notes.push_back (s.note);
        return notes;
    }

    void PatternPerformer::endDue (double beat, bool all)
    {
        for (auto it = sounding.begin(); it != sounding.end();)
            if (all || it->endBeat <= beat)
            {
                if (sendTo)
                    sendTo (juce::MidiMessage::noteOff (channel, it->note));
                it = sounding.erase (it);
            }
            else
                ++it;
    }

    void PatternPerformer::process()
    {
        const juce::ScopedLock sl (lock);
        const auto index = pattern.load();
        if (! active || index < 0)
        {
            if (sounding.empty())
                stopTimer();
            return;
        }
        // The clock changed hands (the song started or stopped): carry on from here.
        const auto s = songBeat ? songBeat() : -1.0;
        const auto free = s < 0.0;
        if (free != freeClock)
        {
            freeClock = free;
            startedMs = juce::Time::getMillisecondCounterHiRes();
            lastBeat = now() - 1.0e-6;
        }
        const auto beat = now();
        if (beat < lastBeat)
            lastBeat = beat - 1.0e-6;   // (a jump back: from here)
        endDue (beat, false);
        const auto& p = patterns()[(size_t) index];
        const auto barBeats = beatsPerBar.load();
        const auto scale = barBeats / 4.0, length = p.bars * barBeats;
        const auto bass = chord.front() - 12;
        for (auto k = (int) std::floor (lastBeat / length); k <= (int) std::floor (beat / length); ++k)
            for (const auto& step : p.steps)
            {
                const auto played = stepNotes (step, chord, bass);
                for (size_t i = 0; i < played.size(); ++i)
                {
                    const auto at = k * length + step.beat * scale + (step.strum != 0 ? (double) i * strumGap : 0.0);
                    if (at <= lastBeat || at > beat)
                        continue;
                    const auto note = played[i];
                    for (auto it = sounding.begin(); it != sounding.end(); ++it)
                        if (it->note == note)
                        {
                            sendTo (juce::MidiMessage::noteOff (channel, note));   // (the same note again: restarted)
                            sounding.erase (it);
                            break;
                        }
                    const auto v = juce::jlimit (1, 127, juce::roundToInt ((float) velocity * step.velocity / mid));
                    sendTo (juce::MidiMessage::noteOn (channel, note, (juce::uint8) v));
                    sounding.push_back ({ note, at + step.length * scale });
                }
            }
        lastBeat = beat;
    }
}
