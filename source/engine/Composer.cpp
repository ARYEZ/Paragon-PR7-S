#include "engine/Composer.h"

#include "engine/DrumIdeas.h"

namespace pr7s
{
    const std::array<ComposerLayer, composerLayerCount>& composerLayers()
    {
        static const std::array<ComposerLayer, composerLayerCount> all { ComposerLayer::chords, ComposerLayer::melody, ComposerLayer::bass,
                                                                          ComposerLayer::guitar, ComposerLayer::lead, ComposerLayer::drums };
        return all;
    }

    juce::String composerLayerName (ComposerLayer l)
    {
        switch (l)
        {
            case ComposerLayer::chords: return "CHORDS";
            case ComposerLayer::melody: return "MELODY";
            case ComposerLayer::bass: return "BASS";
            case ComposerLayer::guitar: return "GUITAR";
            case ComposerLayer::lead: return "LEAD";
            case ComposerLayer::drums: return "DRUMS";
        }
        return {};
    }

    LineIdea::Part composerPartOf (ComposerLayer l)
    {
        switch (l)
        {
            case ComposerLayer::chords: return LineIdea::Part::chords;
            case ComposerLayer::melody: return LineIdea::Part::tune;
            case ComposerLayer::bass: return LineIdea::Part::bass;
            case ComposerLayer::guitar: return LineIdea::Part::rhythm;
            case ComposerLayer::lead: return LineIdea::Part::lead;
            case ComposerLayer::drums: return LineIdea::Part::drums;
        }
        return LineIdea::Part::melody;
    }

    //==========================================================================
    ComposerState::ComposerState()
    {
        lane (ComposerLayer::lead).on = false;
        // The bass on the drums' kick to start with (the two as one: the usual way a band locks up).
        lane (ComposerLayer::bass).style = composerStyleCount (ComposerLayer::bass) - 1;
        // The drums' verse.
        lane (ComposerLayer::drums).style = 1;
    }

    juce::String ComposerState::toString() const
    {
        // key index | genre | mood | bars | progression | fromChordTrack | toChordTrack | lanes (on,locked,style,variation;...)
        const auto& keys = Key::all();
        const auto keyIndex = (int) std::distance (keys.begin(), std::find (keys.begin(), keys.end(), key));
        juce::StringArray lanesText;
        for (const auto& l : lanes)
            lanesText.add (juce::String (l.on ? 1 : 0) + "," + juce::String (l.locked ? 1 : 0) + "," + juce::String (l.style) + "," + juce::String (l.variation));
        return juce::String (keyIndex) + "|" + genre + "|" + mood + "|" + juce::String (bars) + "|" + progression + "|"
             + (fromChordTrack ? "1" : "0") + "|" + (toChordTrack ? "1" : "0") + "|" + lanesText.joinIntoString (";");
    }

    ComposerState ComposerState::fromString (const juce::String& text)
    {
        ComposerState s;
        const auto parts = juce::StringArray::fromTokens (text, "|", "");
        if (parts.size() < 8)
            return s;
        const auto& keys = Key::all();
        if (const auto k = parts[0].getIntValue(); juce::isPositiveAndBelow (k, (int) keys.size()))
            s.key = keys[(size_t) k];
        s.genre = parts[1];
        s.mood = parts[2];
        if (const auto b = parts[3].getIntValue(); b == 4 || b == 8 || b == 16)
            s.bars = b;
        s.progression = parts[4];
        s.fromChordTrack = parts[5] == "1";
        s.toChordTrack = parts[6] == "1";
        const auto lanesText = juce::StringArray::fromTokens (parts[7], ";", "");
        for (int i = 0; i < juce::jmin (composerLayerCount, lanesText.size()); ++i)
        {
            const auto v = juce::StringArray::fromTokens (lanesText[i], ",", "");
            if (v.size() < 4)
                continue;
            auto& l = s.lanes[(size_t) i];
            l.on = v[0] == "1";
            l.locked = v[1] == "1";
            l.style = juce::jlimit (0, composerStyleCount (composerLayers()[(size_t) i]) - 1, v[2].getIntValue());
            l.variation = juce::jmax (0, v[3].getIntValue());
        }
        return s;
    }

    //==========================================================================
    std::vector<int> composerProgressions (const ComposerState& s)
    {
        ProgressionFilter filter;
        filter.genre = s.genre;
        filter.mood = s.mood;
        return findProgressions (s.key, filter);
    }

    int composerProgressionIndex (const ComposerState& s)
    {
        const auto found = composerProgressions (s);
        if (found.empty())
            return -1;
        const auto named = progressionNamed (s.progression);
        return std::find (found.begin(), found.end(), named) != found.end() ? named : found.front();
    }

    std::vector<ProgressionStep> composerSteps (const ComposerState& s)
    {
        const auto index = composerProgressionIndex (s);
        if (index < 0)
            return {};
        const auto& p = progressionLibrary()[(size_t) index];
        const auto once = p.in (s.key);
        const auto length = p.totalBars();
        if (once.empty() || length <= 0.0)
            return once;
        const auto times = juce::jmax (1, (int) std::ceil ((double) s.bars / length - 1.0e-9));
        std::vector<ProgressionStep> steps;
        for (int t = 0; t < times; ++t)
            steps.insert (steps.end(), once.begin(), once.end());
        return steps;
    }

    std::vector<ChordSpan> composerSpans (const ComposerState& s, double startBeat, double beatsPerBar)
    {
        std::vector<ChordSpan> spans;
        auto beat = startBeat;
        for (const auto& step : composerSteps (s))
        {
            spans.push_back ({ beat, beat + step.bars * beatsPerBar, step.chord, s.key });
            beat += step.bars * beatsPerBar;
        }
        return spans;
    }

    //==========================================================================
    int composerStyleCount (ComposerLayer l)
    {
        return l == ComposerLayer::bass ? 7 : 6;
    }

    namespace
    {
        LineIdea drumsOf (const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c, int section,
                          const std::vector<double>& riffHits)
        {
            const auto& sections = drumSections();
            const auto which = sections[(size_t) juce::jlimit (0, (int) sections.size() - 1, section)];
            const auto seed = (juce::uint32) (s.lane (ComposerLayer::drums).variation * 7919 + 1);
            return c.drummer.has_value() ? generateBeat (*c.drummer, which, spans, c.beatsPerBar, c.drums, seed, riffHits)
                                         : generateBeat (c.style, which, spans, c.beatsPerBar, c.drums, seed, riffHits);
        }

        LineIdea pick (std::vector<LineIdea> list, int style)
        {
            if (list.empty())
                return {};
            return std::move (list[(size_t) juce::jlimit (0, (int) list.size() - 1, style)]);
        }

        // The riff the drums may lock to, and the beat the bass's seventh style follows.
        LineIdea guitarOf (const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
        {
            const auto& lane = s.lane (ComposerLayer::guitar);
            return pick (guitarRhythmIdeas (spans, c.beatsPerBar, lane.variation, c.tuning, c.velocities, c.style, c.riff), lane.style);
        }

        LineIdea rawLayer (ComposerLayer l, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
        {
            const auto& lane = s.lane (l);
            switch (l)
            {
                case ComposerLayer::chords: return pick (pianoChordIdeas (spans, c.beatsPerBar, lane.variation), lane.style);
                case ComposerLayer::melody: return pick (composedMelodies (spans, c.beatsPerBar, c.melody, lane.variation), lane.style);
                case ComposerLayer::guitar: return guitarOf (s, spans, c);
                case ComposerLayer::lead: return pick (guitarLeadIdeas (spans, c.beatsPerBar, lane.variation, c.tuning, c.velocities, c.style), lane.style);
                case ComposerLayer::drums:
                {
                    const auto hits = c.drums.lockToRiff ? riffHits (guitarOf (s, spans, c)) : std::vector<double> {};
                    return drumsOf (s, spans, c, lane.style, hits);
                }
                case ComposerLayer::bass:
                {
                    if (lane.style >= 6)
                    {
                        // The seventh: the roots on the drums' kick (the drums lane's beat, heard or not).
                        const auto hits = c.drums.lockToRiff ? riffHits (guitarOf (s, spans, c)) : std::vector<double> {};
                        const auto beat = drumsOf (s, spans, c, s.lane (ComposerLayer::drums).style, hits);
                        return bassOnHits (spans, kickHits (beat), "Locked to the kick", "The chords' roots on every kick of the drums' " + beat.style.toLowerCase());
                    }
                    return pick (bassIdeas (spans, c.beatsPerBar, lane.variation), lane.style);
                }
            }
            return {};
        }
    }

    std::vector<LineIdea> composerChoices (ComposerLayer l, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
    {
        std::vector<LineIdea> out;
        if (spans.empty())
            return out;
        auto each = s;
        for (int i = 0; i < composerStyleCount (l); ++i)
        {
            each.lane (l).style = i;
            out.push_back (rawLayer (l, each, spans, c));
        }
        return out;
    }

    std::array<LineIdea, composerLayerCount> composeLayers (const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
    {
        std::array<LineIdea, composerLayerCount> out;
        if (spans.empty())
            return out;
        for (const auto l : composerLayers())
        {
            auto idea = rawLayer (l, s, spans, c);
            idea.part = composerPartOf (l);
            // IDEAS' HUMANIZE, the same rules (a guitar's velocity layers kept; FEEL: rhythm and bass ahead,
            // leads and melodies behind).
            if (c.humanize > 0.0f && ! idea.notes.empty())
            {
                HumanizeOptions o;
                o.amount = c.humanize;
                o.bpm = c.bpm;
                o.beatsPerBar = c.beatsPerBar;
                o.originBeat = spans.front().start;
                if (l == ComposerLayer::guitar || l == ComposerLayer::lead)
                    o.layers = guitarLayers();
                if (c.feel)
                    o.feelMs = c.humanize * (l == ComposerLayer::guitar || l == ComposerLayer::bass ? HumanizeLimits::feelAheadMs
                                             : l == ComposerLayer::lead || l == ComposerLayer::melody ? HumanizeLimits::feelBehindMs : 0.0);
                const auto& lane = s.lane (l);
                o.seed = (juce::uint32) (lane.variation * 1009 + (int) l * 101 + lane.style + 1);
                idea.notes = humanize (std::move (idea.notes), o);
            }
            out[(size_t) l] = std::move (idea);
        }
        return out;
    }

    //==========================================================================
    void rollLane (ComposerState& s, ComposerLayer l, juce::Random& random)
    {
        auto& lane = s.lane (l);
        const auto count = composerStyleCount (l);
        if (count > 1)
        {
            auto next = random.nextInt (count - 1);   // another style: never the one it has
            lane.style = next >= lane.style ? next + 1 : next;
        }
        lane.variation = random.nextInt (10000);
        if (l == ComposerLayer::chords && ! s.fromChordTrack)
        {
            // Another progression, in the key's own mode when there are any (a minor key's minor ones).
            auto found = composerProgressions (s);
            const auto& library = progressionLibrary();
            std::vector<int> own;
            for (const auto i : found)
                if (library[(size_t) i].minor == s.key.minor)
                    own.push_back (i);
            auto& from = own.empty() ? found : own;
            const auto current = composerProgressionIndex (s);
            from.erase (std::remove (from.begin(), from.end(), current), from.end());
            if (! from.empty())
                s.progression = library[(size_t) from[(size_t) random.nextInt ((int) from.size())]].name;
        }
    }

    int generateComposer (ComposerState& s, juce::Random& random)
    {
        auto rolled = 0;
        for (const auto l : composerLayers())
            if (! s.lane (l).locked)
            {
                rollLane (s, l, random);
                ++rolled;
            }
        return rolled;
    }
}
