#include "engine/Melody.h"

#include <map>

namespace pr7s
{
    namespace
    {
        int pcOf (int note) { return ((note % 12) + 12) % 12; }
        bool has (const std::vector<int>& list, int value) { return std::find (list.begin(), list.end(), value) != list.end(); }

        // The note nearest `around` whose pitch class is in the list, within [low, high] (ties: the lower).
        int nearestIn (const std::vector<int>& pitchClasses, double around, int low, int high)
        {
            auto best = -1;
            for (auto n = low; n <= high; ++n)
                if (has (pitchClasses, pcOf (n)) && (best < 0 || std::abs (n - around) < std::abs (best - around)))
                    best = n;
            return best < 0 ? juce::jlimit (low, high, juce::roundToInt (around)) : best;
        }

        // `steps` scale steps from `pitch` (up or down), without a range: the realisation clamps.
        int moveInScale (int pitch, int steps, const std::vector<int>& scale)
        {
            const auto dir = steps > 0 ? 1 : -1;
            for (auto k = 0; k < std::abs (steps); ++k)
            {
                auto n = pitch + dir;
                while (! has (scale, pcOf (n)))
                    n += dir;
                pitch = n;
            }
            return pitch;
        }

        const ChordSpan& spanAt (const std::vector<ChordSpan>& spans, double beat)
        {
            for (const auto& s : spans)
                if (beat < s.end - 1.0e-9)
                    return s;
            return spans.back();
        }

        std::vector<int> chordTonesOf (const Chord& chord)
        {
            const auto tones = chord.pitchClasses();
            return { tones.begin(), tones.end() };
        }

        // A phrase's material: where its notes fall (beats from the phrase's start), how long,
        // whether on a beat, and its shape (scale steps from the phrase's first note).
        struct MotifNote { double at, length; bool strong; int step; };
        using Motif = std::vector<MotifNote>;

        Motif makeMotif (const MelodySettings& m, double beatsPerBar, juce::Random& random)
        {
            const auto perBar = juce::jmax (4, juce::roundToInt (beatsPerBar * 4.0));   // sixteenths in a bar
            const auto phraseBeats = beatsPerBar * m.phraseBars;
            Motif motif;
            for (int bar = 0; bar < m.phraseBars; ++bar)
            {
                // Each sixteenth's pull: the bar's start most, then the beats, the eighths, the sixteenths.
                std::vector<std::pair<float, int>> scored;
                for (int s = 0; s < perBar; ++s)
                {
                    float w;
                    if (s == 0)                 w = 1.0f;
                    else if (s % 8 == 0)        w = m.syncopation ? 0.6f : 0.85f;
                    else if (s % 4 == 0)        w = m.syncopation ? 0.5f : 0.7f;
                    else if (s % 2 == 0)        w = m.syncopation ? 0.78f : 0.45f;
                    else                        w = m.syncopation ? 0.3f : 0.2f;
                    scored.push_back ({ w + random.nextFloat() * 0.35f, s });
                }
                std::sort (scored.begin(), scored.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });
                const auto count = juce::jlimit (1, perBar, juce::roundToInt (1.0f + m.density * 9.0f + (random.nextFloat() - 0.5f)));
                std::vector<int> chosen;
                for (int i = 0; i < count; ++i)
                    chosen.push_back (scored[(size_t) i].second);
                std::sort (chosen.begin(), chosen.end());
                for (const auto s : chosen)
                {
                    // Beats 1 and 3 land on chord tones; 2 and 4 may pass through the key (a run of
                    // quarters over one chord would otherwise sit on one note).
                    const auto onBeat = s % 8 == 0;
                    // Rests: some weak notes left out (never a phrase's first).
                    if (! motif.empty() && ! onBeat && random.nextFloat() < m.rests * 0.45f)
                        continue;
                    motif.push_back ({ bar * beatsPerBar + s / 4.0, 0.0, onBeat, 0 });
                }
            }
            if (motif.empty() || motif.front().at > 0.0)
                motif.insert (motif.begin(), { 0.0, 0.0, true, 0 });
            // Lengths: to the next note, at most two beats; the phrase's last a breath short of its end.
            for (size_t i = 0; i < motif.size(); ++i)
            {
                const auto next = i + 1 < motif.size() ? motif[i + 1].at : phraseBeats;
                auto length = juce::jmin (2.0, next - motif[i].at);
                if (i + 1 == motif.size())
                {
                    motif[i].strong = true;   // (a phrase ends on a chord tone)
                    length = juce::jmax (0.25, length * (1.0 - m.rests * 0.5));
                }
                motif[i].length = juce::jmax (0.125, length);
            }
            // The shape: up through the first half, down through the second; a leap (thirds to
            // sixths) as often as LEAPS asks, then a step back the other way.
            const auto reach = juce::jlimit (3, 6, (m.high - m.low) * 7 / 24);   // scale steps either side of the start (at most a sixth)
            auto at = 0, afterLeap = 0;
            for (size_t i = 1; i < motif.size(); ++i)
            {
                const auto along = motif[i].at / phraseBeats;
                auto dir = random.nextFloat() < (along < 0.5 ? 0.66f : 0.34f) ? 1 : -1;
                int move;
                if (afterLeap != 0)
                {
                    move = -afterLeap;   // (the gap filled)
                    afterLeap = 0;
                }
                else if (random.nextFloat() < m.leaps)
                {
                    // A third or a fourth; fifths and sixths too as LEAPS goes past 60%.
                    move = dir * (2 + random.nextInt (m.leaps > 0.6f ? 4 : 2));
                    afterLeap = dir;
                }
                else
                    move = random.nextFloat() < 0.12f ? 0 : dir;   // (a repeated note now and then)
                if (std::abs (at + move) > reach)
                {
                    move = -move;
                    afterLeap = -afterLeap;
                }
                at += move;
                motif[i].step = at;
            }
            return motif;
        }

        // A returning phrase: the same material, changed a little (less as REPEAT goes up).
        Motif varied (Motif motif, float repetition, juce::Random& random)
        {
            const auto change = 1.0f - repetition;
            Motif out;
            for (size_t i = 0; i < motif.size(); ++i)
            {
                auto n = motif[i];
                const auto last = i + 1 == motif.size();
                if (i > 0 && ! last && ! n.strong && random.nextFloat() < change * 0.25f)
                {
                    if (! out.empty())
                        out.back().length += n.length;   // (left out: the note before held over it)
                    continue;
                }
                if (i > 0 && random.nextFloat() < change * 0.6f)
                    n.step += random.nextBool() ? 1 : -1;
                out.push_back (n);
            }
            return out;
        }
    }

    MelodySettings MelodySettings::limited() const
    {
        auto m = *this;
        m.density = juce::jlimit (0.0f, 1.0f, m.density);
        m.leaps = juce::jlimit (0.0f, 1.0f, m.leaps);
        m.repetition = juce::jlimit (0.0f, 1.0f, m.repetition);
        m.rests = juce::jlimit (0.0f, 1.0f, m.rests);
        m.phraseBars = m.phraseBars >= 4 ? 4 : m.phraseBars >= 2 ? 2 : 1;
        m.low = juce::jlimit (lowest, highest - leastSpan, m.low);
        m.high = juce::jlimit (m.low + leastSpan, highest, m.high);
        return m;
    }

    juce::String MelodySettings::toString() const
    {
        return juce::String (density, 3) + " " + juce::String (low) + " " + juce::String (high) + " " + juce::String (leaps, 3) + " "
             + juce::String (phraseBars) + " " + juce::String (repetition, 3) + " " + juce::String (rests, 3) + " " + (syncopation ? "1" : "0");
    }

    MelodySettings MelodySettings::fromString (const juce::String& text)
    {
        const auto v = juce::StringArray::fromTokens (text, " ", {});
        MelodySettings m;
        if (v.size() != 8)
            return m;
        m.density = v[0].getFloatValue();
        m.low = v[1].getIntValue();
        m.high = v[2].getIntValue();
        m.leaps = v[3].getFloatValue();
        m.phraseBars = v[4].getIntValue();
        m.repetition = v[5].getFloatValue();
        m.rests = v[6].getFloatValue();
        m.syncopation = v[7] == "1";
        return m.limited();
    }

    bool MelodySettings::operator== (const MelodySettings& o) const noexcept
    {
        return toString() == o.toString();
    }

    const std::vector<MelodyForm>& melodyForms()
    {
        static const std::vector<MelodyForm> forms {
            { "Song form", "AABA", "A phrase, again, something new, then the first one home (the classic verse and chorus shape)", false },
            { "Question and answer", "ABAB", "A phrase that asks, one that answers, and both again", false },
            { "Hook", "AAAB", "One catchy phrase three times, then a turn at the end (pop and dance hooks)", false },
            { "Two answers", "ABAC", "The same question twice, answered differently each time", false },
            { "Climbing motif", "AAAA", "One motif, each time higher in the key (a sequence: builds and lifts)", true },
            { "Always new", "ABCD", "Every phrase new, no repeats (a free, through-composed line)", false } };
        return forms;
    }

    LineIdea composeMelody (const std::vector<ChordSpan>& spans, double beatsPerBar, const MelodySettings& settings,
                            const MelodyForm& form, juce::uint32 seed)
    {
        LineIdea idea { LineIdea::Part::tune, form.name, form.description, {} };
        if (spans.empty() || beatsPerBar <= 0.0)
            return idea;
        const auto m = settings.limited();
        juce::Random random ((juce::int64) seed * 7919 + 17);
        const auto origin = spans.front().start;
        const auto end = spans.back().end;
        const auto phraseBeats = beatsPerBar * m.phraseBars;
        const juce::String letters (form.letters);

        std::map<juce::juce_wchar, Motif> motifs;
        std::map<juce::juce_wchar, int> anchors, returns;
        auto lastPitch = juce::roundToInt ((m.low + m.high) * 0.5) - 2;
        auto phrase = 0;
        for (auto start = origin; start < end - 1.0e-9; start += phraseBeats, ++phrase)
        {
            const auto letter = letters[phrase % letters.length()];
            const auto& opening = spanAt (spans, start);
            const auto openingTones = chordTonesOf (opening.chord);
            Motif motif;
            int anchor;
            if (const auto known = motifs.find (letter); known != motifs.end())
            {
                motif = varied (known->second, m.repetition, random);
                auto at = anchors[letter];
                if (form.rising)
                    at = moveInScale (at, 2 * ++returns[letter], opening.key.scale());   // (a third each time: still a chord tone over one chord)
                anchor = nearestIn (openingTones, at, m.low, m.high);
            }
            else
            {
                motif = makeMotif (m, beatsPerBar, random);
                motifs[letter] = motif;
                // A new phrase starts near where the last left off; the first one a little under the middle.
                // (A second letter starts a little higher: contrast.)
                const auto around = phrase == 0 ? (double) lastPitch : lastPitch + (motifs.size() > 1 ? 2.0 : 0.0);
                anchor = nearestIn (openingTones, around, m.low, m.high);
                anchors[letter] = anchor;
            }

            for (const auto& n : motif)
            {
                const auto at = start + n.at;
                if (at >= end - 1.0e-9)
                    break;
                const auto& span = spanAt (spans, at);
                auto target = moveInScale (anchor, n.step, span.key.scale());
                // Past the range's edge: turned back from it, as far as it went past (an octave's
                // fold would jump; holding it at the edge would repeat one note).
                if (target > m.high) target = m.high - (target - m.high);
                if (target < m.low)  target = m.low + (m.low - target);
                target = juce::jlimit (m.low, m.high, target);
                const auto pitch = n.strong ? nearestIn (chordTonesOf (span.chord), target, m.low, m.high)
                                            : nearestIn (availableNotes (span.chord, span.key), target, m.low, m.high);
                const auto length = juce::jmin (n.length, span.end - at);   // (not held into the next chord)
                const auto velocity = n.at == 0.0 ? 0.88f : n.strong ? 0.82f : 0.7f;
                idea.notes.push_back ({ at - origin, juce::jmax (0.05, length * 0.96), pitch, velocity });
                lastPitch = pitch;
            }
        }
        // The last note home: the last chord's root (or its nearest chord tone), held to the end.
        if (! idea.notes.empty())
        {
            auto& last = idea.notes.back();
            const auto& span = spans.back();
            const auto root = nearestIn ({ pcOf (span.chord.root) }, last.pitch, m.low, m.high);
            last.pitch = std::abs (root - last.pitch) <= 5 ? root : nearestIn (chordTonesOf (span.chord), last.pitch, m.low, m.high);
            last.length = juce::jmax (last.length, (end - origin - last.beat) * 0.96);
        }
        return idea;
    }

    std::vector<LineIdea> composedMelodies (const std::vector<ChordSpan>& spans, double beatsPerBar, const MelodySettings& settings, int variation)
    {
        std::vector<LineIdea> ideas;
        const auto& forms = melodyForms();
        for (size_t i = 0; i < forms.size(); ++i)
            ideas.push_back (composeMelody (spans, beatsPerBar, settings, forms[i], (juce::uint32) (variation * 31 + (int) i * 7 + 1)));
        return ideas;
    }
}
