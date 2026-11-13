#include "engine/Composer.h"
#include "engine/SectionTypes.h"

#include <algorithm>

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
            lanesText.add (juce::String (l.on ? 1 : 0) + "," + juce::String (l.locked ? 1 : 0) + "," + juce::String (l.style) + "," + juce::String (l.variation)
                           + "," + juce::String (l.busy, 2) + "," + juce::String (l.lowString ? 1 : 0) + "," + juce::String (l.onlyString)
                           + "," + juce::String (l.doubleSync ? 1 : 0) + "," + juce::String (l.approach));
        return juce::String (keyIndex) + "|" + genre + "|" + mood + "|" + juce::String (bars) + "|" + progression + "|"
             + (fromChordTrack ? "1" : "0") + "|" + (toChordTrack ? "1" : "0") + "|" + lanesText.joinIntoString (";")
             + "|" + juce::String (riffFlavour) + "," + (writtenRiffs ? "1" : "0") + "," + juce::String (syncLane)
             + "|" + sectionType.removeCharacters ("|") + "|" + transition.removeCharacters ("|")
             + "|" + (libraryDrums ? "1" : "0") + "|" + libraryGroove.removeCharacters ("|");
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
            if (v.size() >= 6)
            {
                l.busy = juce::jlimit (0.0f, 1.0f, v[4].getFloatValue());
                l.lowString = v[5] == "1";
            }
            if (v.size() >= 7)
                l.onlyString = juce::jlimit (0, 12, v[6].getIntValue());
            if (v.size() >= 8)
                l.doubleSync = v[7] == "1";
            if (v.size() >= 9)
                l.approach = juce::jlimit (-1, 15, v[8].getIntValue());   // (0.94.0)
        }
        if (parts.size() >= 9)
        {
            // (0.85.0) The guitar's flavour and written riffs (older text: the generator, ANY).
            const auto g = juce::StringArray::fromTokens (parts[8], ",", "");
            s.riffFlavour = juce::jlimit (0, (int) riffFlavours().size() - 1, g[0].getIntValue());
            s.writtenRiffs = g.size() >= 2 && g[1] == "1";
            s.syncLane = g.size() >= 3 ? juce::jlimit (-1, composerLayerCount - 1, g[2].getIntValue()) : -1;   // (0.86.0)
        }
        if (parts.size() >= 10)
            s.sectionType = parts[9];   // (0.94.0)
        if (parts.size() >= 11)
            s.transition = parts[10];   // (0.104.0)
        if (parts.size() >= 12)
            s.libraryDrums = parts[11] != "0";   // (0.106.0; older text: on)
        if (parts.size() >= 13)
            s.libraryGroove = parts[12];
        return s;
    }

    //==========================================================================
    std::vector<int> composerProgressions (const ComposerState& s)
    {
        ProgressionFilter filter;
        filter.genre = s.genre;
        filter.mood = s.mood;
        // (0.75.0, the owner: "only chords that fit the key") Progressions with a chord outside the key left out.
        auto found = findProgressions (s.key, filter);
        found.erase (std::remove_if (found.begin(), found.end(), [&s] (int i)
                     {
                         for (const auto& step : progressionLibrary()[(size_t) i].in (s.key))
                             if (! step.chord.isInKey (s.key))
                                 return true;
                         return false;
                     }), found.end());
        return found;
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
        // (0.65.0) The lead's seventh is the whole solo; the drums' seventh the solo section.
        return l == ComposerLayer::bass || l == ComposerLayer::lead ? 7 : l == ComposerLayer::drums ? (int) drumSections().size() : 6;
    }

    RiffRecipe guitarRecipe (const ComposerState& s, const ComposerContext& c)
    {
        // (0.85.0) The section is the drums lane's (SECTION); the take's seed from the variation and the style slot
        // (so a section suggested with "the same riff" gets this riff back, "a new riff" another).
        const auto& lane = s.lane (ComposerLayer::guitar);
        const auto& sections = drumSections();
        RiffRecipe r;
        r.style = c.style;
        r.flavour = riffFlavours()[(size_t) juce::jlimit (0, (int) riffFlavours().size() - 1, s.riffFlavour)];
        r.section = sections[(size_t) juce::jlimit (0, (int) sections.size() - 1, s.lane (ComposerLayer::drums).style)];
        r.bars = c.riff.limited().bars;
        r.busy = lane.busy;
        r.seed = (juce::uint32) (lane.variation * 7919 + lane.style * 131 + 7);
        if (const auto typed = typedGuitarFlavour (s))
            r.flavour = *typed;   // (0.94.0: a type of several riff kinds: the approach picks one)
        else if (r.flavour == RiffFlavour::droning || r.flavour == RiffFlavour::drivingEighths || r.flavour == RiffFlavour::syncopatedDrive
                 || r.flavour == RiffFlavour::mutedDrive)
            r.approach = lane.approach;   // (0.94.0; 0.98.0 DRIVING EIGHTHS too)
        return r;
    }

    juce::String guitarRiffFingerprint (const ComposerState& s, const ComposerContext& c)
    {
        if (s.writtenRiffs || c.myRiff.has_value())
            return {};
        return riffFingerprint (generateRiffBars (guitarRecipe (s, c), riffStepsPerBar (c.beatsPerBar)));
    }

    namespace
    {
        // A lane's take options: its seed (from the variation and the layer), BUSY, LOW STRING.
        TakeOptions takeOf (const ComposerLane& lane, ComposerLayer l, int lowest, int highest)
        {
            TakeOptions o;
            o.seed = (juce::uint32) (lane.variation * 7919 + (int) l * 131 + 1);
            o.busy = lane.busy;
            o.lowString = lane.lowString;
            o.lowest = lowest;
            o.highest = highest;
            return o;
        }

        LineIdea drumsOf (const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c, int section,
                          const std::vector<double>& riffHits, bool synced = false)
        {
            const auto& sections = drumSections();
            const auto which = sections[(size_t) juce::jlimit (0, (int) sections.size() - 1, section)];
            const auto& lane = s.lane (ComposerLayer::drums);
            const auto seed = (juce::uint32) (lane.variation * 7919 + 1);
            auto drums = c.drums;
            if (synced)
            {
                drums.lockToRiff = true;   // (0.86.0: SYNC: the kick on the sync lane's hits, in every section)
                drums.lockAlways = true;
            }
            drums.density = lane.busy;   // (BUSY: the generator's density, 0.5 the style's own)
            return c.drummer.has_value() ? generateBeat (*c.drummer, which, spans, c.beatsPerBar, drums, seed, riffHits)
                                         : generateBeat (c.style, which, spans, c.beatsPerBar, drums, seed, riffHits);
        }

        // (0.106.0) The drums lane from the owner's groove library: the section's groove (chosen by hand, else the
        // take picks one of the best few), repeated over the section, and a library fill on the last bar of every
        // phrase of DRUM SETTINGS' FILL EVERY bars and on the section's last bar. None: off or no grooves found.
        std::optional<LineIdea> libraryDrumsOf (const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
        {
            if (! s.libraryDrums || c.libraryGrooves.empty() || spans.empty())
                return std::nullopt;
            const auto groove = composerLibraryGroove (s, c);
            if (! groove.has_value())
                return std::nullopt;
            juce::String error;
            auto idea = grooveIdea (*groove, spans, c.beatsPerBar, error);
            if (! idea.has_value())
                return std::nullopt;
            idea->part = LineIdea::Part::drums;
            idea->style = "My library: " + groove->name;
            const auto bpb = c.beatsPerBar;
            const auto length = spans.back().end - spans.front().start;
            const auto bars = (int) std::ceil (length / bpb - 1.0e-6);
            const auto& lane = s.lane (ComposerLayer::drums);
            const auto fills = libraryGroovesFor (c.libraryGrooves, lane.style, true);
            if (! fills.empty() && bars >= 1)
            {
                const auto& fill = fills[(size_t) (lane.variation / 3) % juce::jmin ((size_t) 4, fills.size())];
                const auto notes = libraryFillBar (fill, bpb);
                if (! notes.empty())
                {
                    auto fillLength = 0.0;
                    for (const auto& n : notes)
                        fillLength = juce::jmax (fillLength, n.beat);
                    fillLength = juce::jlimit (bpb / 4.0, bpb, std::ceil ((fillLength + 1.0e-6) / (bpb / 4.0)) * (bpb / 4.0));
                    const auto every = c.drums.fillEvery;
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto last = bar == bars - 1;
                        if (! last && (every <= 0 || (bar + 1) % every != 0))
                            continue;
                        const auto end = juce::jmin (length, (bar + 1) * bpb);
                        const auto from = end - fillLength;
                        idea->notes.erase (std::remove_if (idea->notes.begin(), idea->notes.end(), [&] (const LineNote& n)
                                           { return n.beat >= from - 1.0e-6 && n.beat < end - 1.0e-6; }), idea->notes.end());
                        for (const auto& n : notes)
                            if (from + n.beat < end - 1.0e-6)
                                idea->notes.push_back ({ from + n.beat, juce::jmin (n.length, end - from - n.beat), n.pitch, n.velocity });
                    }
                    idea->description << "; fills from " << fill.name;
                    std::stable_sort (idea->notes.begin(), idea->notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
                }
            }
            return idea;
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
            auto riff = c.riff;
            riff.busyness = lane.busy;   // (BUSY: RIFF SETTINGS' busyness, 0.5 as written)
            const auto& profile = c.guitarProfile;
            const auto tuning = profile.has_value() ? profile->tuning() : c.tuning;
            const auto velocities = profile.has_value() && profile->velocities.has_value() ? *profile->velocities : c.velocities;
            if (c.myRiff.has_value())
            {
                // (0.75.0) Your riff, reshaped only as far as RIFF SETTINGS and BUSY move from their middles.
                auto mine = *c.myRiff;
                shapeRiff (mine, spans, c.beatsPerBar, riff, velocities, c.style, lane.variation, tuning);
                return mine;
            }
            const auto low = profile.has_value() ? profile->rangeLow() : tuning.lowest();
            const auto high = profile.has_value() ? profile->rangeHigh() : tuning.highest();
            if (! s.writtenRiffs)
            {
                // (0.85.0) A riff made for this take: its own rhythm and notes, every ROLL another.
                auto idea = generateRiff (guitarRecipe (s, c), spans, c.beatsPerBar, tuning, velocities);
                auto shaping = riff;
                shaping.busyness = 0.5f;   // (BUSY made the riff already)
                shapeRiff (idea, spans, c.beatsPerBar, shaping, velocities, c.style, lane.variation, tuning);
                if (lane.lowString)
                    for (auto& n : idea.notes)
                        if (n.effect == LineNote::Effect::none && GuitarVelocity::midiOf (n.velocity) < 120)
                            while (n.pitch > low + 12 && n.pitch - 12 >= low)
                                n.pitch -= 12;
                if (profile.has_value())
                    keepInRange (idea, low, high);
                return idea;
            }
            auto idea = pick (guitarRhythmIdeas (spans, c.beatsPerBar, lane.variation, tuning, velocities, c.style, riff), lane.style);
            riffTake (idea, spans, c.beatsPerBar, takeOf (lane, ComposerLayer::guitar, low, high), c.style);
            if (profile.has_value())
                keepInRange (idea, low, high);
            return idea;
        }

        LineIdea rawLayer (ComposerLayer l, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
        {
            const auto& lane = s.lane (l);
            switch (l)
            {
                case ComposerLayer::chords: return pianoTake (lane.style, spans, c.beatsPerBar, takeOf (lane, l, 21, 108));
                case ComposerLayer::melody:
                {
                    auto melody = c.melody;
                    melody.density = lane.busy * 0.9f;   // (BUSY: the MELODY SETTINGS' density; 0.5: its default 0.45)
                    return pick (composedMelodies (spans, c.beatsPerBar, melody, lane.variation), lane.style);
                }
                case ComposerLayer::guitar: return guitarOf (s, spans, c);
                case ComposerLayer::lead:
                {
                    const auto& profile = c.leadProfile.has_value() ? c.leadProfile : c.guitarProfile;
                    const auto tuning = profile.has_value() ? profile->tuning() : c.tuning;
                    const auto velocities = profile.has_value() && profile->velocities.has_value() ? *profile->velocities : c.velocities;
                    auto idea = pick (guitarLeadIdeas (spans, c.beatsPerBar, lane.variation, tuning, velocities, c.style, c.solo,
                                                       profile.has_value() ? profile->shortestNote : 0.0), lane.style);
                    thinTake (idea, c.beatsPerBar, takeOf (lane, l, 0, 127));
                    if (profile.has_value())
                        keepInRange (idea, profile->rangeLow(), profile->rangeHigh());
                    return idea;
                }
                case ComposerLayer::drums:
                {
                    const auto hits = c.drums.lockToRiff || c.myRiff.has_value() ? riffHits (guitarOf (s, spans, c)) : std::vector<double> {};
                    return drumsOf (s, spans, c, lane.style, hits);
                }
                case ComposerLayer::bass:
                {
                    const auto& profile = c.bassProfile;
                    const auto lowest = profile.has_value() ? profile->rangeLow() : c.bassTuning.lowest();
                    const auto highest = profile.has_value() ? profile->rangeHigh() : bassTop (c.bassTuning);
                    if (lane.style >= 6)
                    {
                        // The seventh: the roots on the drums' kick (the drums lane's beat, heard or not).
                        const auto hits = c.drums.lockToRiff || c.myRiff.has_value() ? riffHits (guitarOf (s, spans, c)) : std::vector<double> {};
                        const auto beat = drumsOf (s, spans, c, s.lane (ComposerLayer::drums).style, hits);
                        auto idea = bassOnHits (spans, kickHits (beat), "Locked to the kick", "The chords' roots on every kick of the drums' " + beat.style.toLowerCase());
                        if (lane.lowString)
                            keepInRange (idea, lowest, lowest + 12);
                        keepInRange (idea, lowest, highest);
                        return idea;
                    }
                    return bassTake (lane.style, spans, c.beatsPerBar, takeOf (lane, l, lowest, highest));
                }
            }
            return {};
        }
    }

    SyncRhythm syncRhythmOf (ComposerLayer leader, const LineIdea& idea, double beatsPerBar, double originBeat)
    {
        SyncRhythm r;
        const auto onBeat = [originBeat] (double beat, double every)
        {
            const auto along = std::fmod (beat - originBeat, every);
            return along < 0.02 || every - along < 0.02;
        };
        if (leader == ComposerLayer::drums)
        {
            r.all = kickHits (idea);
            for (const auto h : r.all)
                if (onBeat (h, 1.0))
                    r.accents.push_back (h);
        }
        else
        {
            // Hits: notes starting together (keyswitches aren't heard). An accent: open, held, or a bar's first.
            std::vector<LineNote> notes;
            for (const auto& n : idea.notes)
                if (! n.keyswitch)
                    notes.push_back (n);
            std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
            auto lastBar = -1;
            for (size_t i = 0; i < notes.size();)
            {
                auto j = i;
                auto loudest = 0.0f;
                auto longest = 0.0;
                while (j < notes.size() && notes[j].beat - notes[i].beat < HumanizeLimits::together)
                {
                    loudest = juce::jmax (loudest, notes[j].velocity);
                    longest = juce::jmax (longest, notes[j].length);
                    ++j;
                }
                const auto at = notes[i].beat;
                r.all.push_back (at);
                const auto bar = (int) std::floor ((at - originBeat) / juce::jmax (0.25, beatsPerBar) + 1.0e-9);
                if (GuitarVelocity::midiOf (loudest) >= 60 || longest >= 0.5 - 1.0e-9 || bar != lastBar)
                    r.accents.push_back (at);
                lastBar = bar;
                i = j;
            }
        }
        if (r.accents.empty())
            r.accents = r.all;
        return r;
    }

    namespace
    {
        const ChordSpan* spanAt (const std::vector<ChordSpan>& spans, double beat)
        {
            for (const auto& span : spans)
                if (beat >= span.start - 1.0e-9 && beat < span.end - 1.0e-9)
                    return &span;
            return spans.empty() ? nullptr : (beat < spans.front().start ? &spans.front() : &spans.back());
        }

        // (0.89.0) An idea's beats count from the first chord's start; the spans' from the song's. 0.86.0's SYNC looked
        // chords up with an idea's beat as if it were the song's (wrong wherever a section didn't start at beat 0).
        const ChordSpan* spanAtIdeaBeat (const std::vector<ChordSpan>& spans, double ideaBeat)
        {
            return spans.empty() ? nullptr : spanAt (spans, ideaBeat + spans.front().start);
        }

        double ideaEnd (const std::vector<ChordSpan>& spans, const ChordSpan& span) { return span.end - spans.front().start; }

        int rootNear (const Chord& chord, int lowest, int highest, bool useBass = true)
        {
            const auto pc = useBass && chord.bass >= 0 ? chord.bass : chord.root;
            auto p = lowest;
            while (((p % 12) + 12) % 12 != pc)
                ++p;
            return juce::jmin (p, highest);
        }

        // DOUBLE: the leader's notes on the bass, each hit's lowest; a high stab over the chugs: the chord's root.
        LineIdea doubledLine (const LineIdea& leader, const std::vector<ChordSpan>& spans, int lowest, int highest, const juce::String& leaderName)
        {
            LineIdea out { LineIdea::Part::bass, "Doubling the " + leaderName.toLowerCase(), "The sync lane's notes, hit for hit, low (DOUBLE THE SYNC TRACK)", {} };
            std::vector<LineNote> notes;
            auto lowestHeard = 128;
            for (const auto& n : leader.notes)
                if (! n.keyswitch)
                {
                    notes.push_back (n);
                    lowestHeard = juce::jmin (lowestHeard, n.pitch);
                }
            std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
            auto previous = -1;
            for (size_t i = 0; i < notes.size();)
            {
                auto j = i;
                auto low = notes[i];
                auto longest = 0.0;
                auto loudest = 0.0f;
                while (j < notes.size() && notes[j].beat - notes[i].beat < HumanizeLimits::together)
                {
                    if (notes[j].pitch < low.pitch)
                        low = notes[j];
                    longest = juce::jmax (longest, notes[j].length);
                    loudest = juce::jmax (loudest, notes[j].velocity);
                    ++j;
                }
                auto pitch = low.pitch;
                if (pitch > lowestHeard + 12)
                    if (const auto* span = spanAtIdeaBeat (spans, low.beat))
                        pitch = rootNear (span->chord, lowest, highest);   // (a stab up high: the bass holds the root)
                // The octave of that note nearest the bass's note before (the first: its lowest octave, under the
                // guitar): a line that moves by steps, not jumping an octave between neighbouring power chords.
                {
                    const auto pc = ((pitch % 12) + 12) % 12;
                    auto best = -1;
                    for (auto p = lowest; p <= highest; ++p)
                        if (p % 12 == pc && (best < 0 || (previous < 0 ? p < best : std::abs (p - previous) < std::abs (best - previous))))
                            best = p;
                    pitch = best >= 0 ? best : juce::jlimit (lowest, highest, pitch);
                }
                previous = pitch;
                // A muted chug's bass note isn't muted: it's played, a little softer than an open hit.
                const auto velocity = juce::jlimit (0.45f, 1.0f, GuitarVelocity::midiOf (loudest) >= 60 ? loudest : 0.7f);
                const auto next = j < notes.size() ? notes[j].beat : low.beat + longest;
                out.notes.push_back ({ low.beat, juce::jmax (0.05, juce::jmin (longest, next - low.beat)), pitch, velocity });
                i = j;
            }
            return out;
        }

        // CHORDS: a stab of the chord on each accent, held to the next accent (two beats at most).
        LineIdea stabsOn (const std::vector<ChordSpan>& spans, const std::vector<double>& accents, const juce::String& leaderName)
        {
            LineIdea out { LineIdea::Part::chords, "Stabs on the " + leaderName.toLowerCase(), "The chord on every accent of the sync lane", {} };
            std::vector<int> previous;
            for (size_t i = 0; i < accents.size(); ++i)
            {
                const auto* span = spanAtIdeaBeat (spans, accents[i]);
                if (span == nullptr)
                    continue;
                const auto voicings = voicingsFor (span->chord, 55);
                if (voicings.empty())
                    continue;
                const auto& voicing = voicings[(size_t) closestVoicing (voicings, previous, {})];
                previous = voicing;
                const auto end = ideaEnd (spans, *span);
                const auto next = i + 1 < accents.size() ? accents[i + 1] : end;
                const auto length = juce::jmax (0.1, juce::jmin (next - accents[i], 2.0, end - accents[i]));
                for (const auto p : voicing)
                    out.notes.push_back ({ accents[i], length * 0.95, p, 0.75f });
            }
            return out;
        }

        // GUITAR: chugs on every hit, the root palm-muted; power chords let ring on the accents.
        LineIdea chugsOn (const std::vector<ChordSpan>& spans, const SyncRhythm& rhythm, const GuitarTuning& tuning,
                          const GuitarVelocities& velocities, const juce::String& leaderName)
        {
            LineIdea out { LineIdea::Part::rhythm, "Chugs on the " + leaderName.toLowerCase(), "Muted roots on every hit, power chords on the accents", {} };
            for (size_t i = 0; i < rhythm.all.size(); ++i)
            {
                const auto at = rhythm.all[i];
                const auto* span = spanAtIdeaBeat (spans, at);
                if (span == nullptr)
                    continue;
                const auto root = rootNear (span->chord, tuning.lowest(), tuning.lowest() + 11, false);
                const auto accent = std::any_of (rhythm.accents.begin(), rhythm.accents.end(), [at] (double a) { return std::abs (a - at) < 1.0e-6; });
                const auto end = ideaEnd (spans, *span);
                const auto next = i + 1 < rhythm.all.size() ? rhythm.all[i + 1] : end;
                const auto room = juce::jmax (0.05, juce::jmin (next - at, end - at));
                if (accent)
                {
                    const auto v = GuitarVelocity::of (velocities.sustain);
                    const auto length = juce::jmin (room, 1.0);
                    for (const auto p : { root, root + 7, root + 12 })
                        out.notes.push_back ({ at, length, p, v });
                }
                else
                    out.notes.push_back ({ at, juce::jmin (room, 0.25), root, GuitarVelocity::of (velocities.muteMid) });
            }
            return out;
        }

        // MELODY / LEAD: their own line; a note starting within a sixteenth of an accent lands on it.
        void leanOnto (LineIdea& idea, const std::vector<double>& accents)
        {
            if (accents.empty())
                return;
            for (auto& n : idea.notes)
            {
                if (n.keyswitch)
                    continue;
                const auto it = std::lower_bound (accents.begin(), accents.end(), n.beat);
                auto best = n.beat;
                auto distance = 0.25 + 1.0e-9;
                for (auto k = it == accents.begin() ? it : std::prev (it); k != accents.end() && k <= it; ++k)
                    if (std::abs (*k - n.beat) < distance)
                    {
                        distance = std::abs (*k - n.beat);
                        best = *k;
                    }
                n.beat = juce::jmax (0.0, best);
            }
            std::stable_sort (idea.notes.begin(), idea.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        }

        //======================================================================
        // (0.89.0) The droning chorus's band: with the guitar ringing its chords (DRONING), the others carry the
        // movement. From the owner's examples' full MIDI (Crawling's and Clown's choruses, read for their shapes):
        //   BASS    the guitar's low strikes doubled, held as long as they ring (the slides and pushes with them).
        //   KEYS    up high on the key's fifth, over every chord: a steady pulse (16ths or 8ths, the beats leaning
        //           in), or a short motif round it that climbs a step at a time at the end of each four bars.
        //   LEAD    a shimmer of octaves in sixteenths on each chord's third (or root), the key's tonic over them
        //           in the first half of each four bars.
        //   DRUMS   open hats on the eighths, kick on 1 and 3, snare on 2 and the and of 3, the fourth bar busier
        //           with a crash on its last beat; or half time: hats on the beats, the snare on 3, a fill.
        bool droningBand (const ComposerState& s, const ComposerContext& c)
        {
            const auto& flavours = riffFlavours();
            return juce::isPositiveAndBelow (s.riffFlavour, (int) flavours.size())
                && flavours[(size_t) s.riffFlavour] == RiffFlavour::droning
                && ! s.writtenRiffs && ! c.myRiff.has_value() && s.lane (ComposerLayer::guitar).on;
        }

        double sectionLength (const std::vector<ChordSpan>& spans) { return spans.back().end - spans.front().start; }

        // The scale's pitch `steps` scale steps from `pitch` (which must be in the scale; else itself).
        int scaleStep (const Key& key, int pitch, int steps)
        {
            auto scale = key.scale();
            std::sort (scale.begin(), scale.end());
            const auto pc = ((pitch % 12) + 12) % 12;
            const auto at = std::find (scale.begin(), scale.end(), pc);
            if (at == scale.end())
                return pitch;
            const auto index = (int) (at - scale.begin()) + steps;
            const auto octave = (int) std::floor ((double) index / 7.0);
            const auto wrapped = index - octave * 7;
            return pitch - pc + scale[(size_t) wrapped] + 12 * octave;
        }

        int pc12 (int pitch) { return ((pitch % 12) + 12) % 12; }

        int pitchOfClassIn (int pc, int lowest, int highest)
        {
            for (auto p = lowest; p <= highest; ++p)
                if (((p % 12) + 12) % 12 == pc)
                    return p;
            return lowest;
        }

        // The guitar's low strikes: when each chord is struck (a push: a beat before its bar).
        std::vector<double> lowStrikes (const LineIdea& guitar)
        {
            auto lowestHeard = 128;
            for (const auto& n : guitar.notes)
                if (! n.keyswitch)
                    lowestHeard = juce::jmin (lowestHeard, n.pitch);
            std::vector<double> out;
            for (const auto& n : guitar.notes)
                if (! n.keyswitch && n.pitch <= lowestHeard + 11)
                    out.push_back (n.beat);
            std::sort (out.begin(), out.end());
            out.erase (std::unique (out.begin(), out.end(), [] (double a, double b) { return b - a < HumanizeLimits::together; }), out.end());
            return out;
        }

        // The chord a strike plays: a strike up to a beat before a chord's start is that chord pushed early.
        const ChordSpan& chordStruckAt (const std::vector<ChordSpan>& spans, double ideaBeat)
        {
            const auto beat = ideaBeat + spans.front().start;
            for (const auto& span : spans)
                if (span.start > beat + 1.0e-6 && span.start <= beat + 1.01)
                    return span;
            const auto* at = spanAt (spans, beat);
            return at != nullptr ? *at : spans.back();
        }

        // (0.89.1, Circle With Me's bass) The last bar of every eight: the root pulsed in eighths into the loop's start.
        void pulseLoopEnds (LineIdea& idea, double bpb, double length)
        {
            const auto e = bpb / 8.0;
            static const std::array<std::pair<int, int>, 7> pulse { { { 0, 1 }, { 1, 2 }, { 3, 1 }, { 4, 1 }, { 5, 1 }, { 6, 1 }, { 7, 1 } } };
            const auto bars = (int) std::floor (length / bpb + 1.0e-6);
            for (int bar = 7; bar < bars; bar += 8)
            {
                const auto from = bar * bpb, to = from + bpb;
                auto soundingAt = [&idea] (double t) -> int
                {
                    for (const auto& n : idea.notes)
                        if (n.beat <= t + 1.0e-6 && n.beat + n.length > t + 1.0e-6)
                            return n.pitch;
                    return -1;
                };
                std::vector<LineNote> hits;
                for (const auto& [at, len] : pulse)
                    if (const auto pitch = soundingAt (from + at * e); pitch >= 0)
                        hits.push_back ({ from + at * e, len * e * 0.9, pitch, at == 0 ? 0.9f : 0.75f });
                std::vector<LineNote> kept;
                for (auto n : idea.notes)
                {
                    const auto end = n.beat + n.length;
                    if (n.beat < from - 1.0e-6)
                    {
                        n.length = juce::jmin (n.length, from - n.beat);
                        kept.push_back (n);
                    }
                    else if (n.beat >= to - 1.0e-6)
                        kept.push_back (n);
                    if (n.beat < to - 1.0e-6 && end > to + 1.0e-6)
                        kept.push_back ({ to, end - to, n.pitch, n.velocity });   // (a push ringing on past the bar)
                }
                kept.insert (kept.end(), hits.begin(), hits.end());
                std::stable_sort (kept.begin(), kept.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
                idea.notes = std::move (kept);
            }
        }

        LineIdea droningBass (const LineIdea& guitar, const std::vector<ChordSpan>& spans, int lowest, int highest)
        {
            // The guitar's low strikes only: its top notes ringing over a held chord aren't the bass's.
            LineIdea low = guitar;
            auto lowestHeard = 128;
            for (const auto& n : guitar.notes)
                if (! n.keyswitch)
                    lowestHeard = juce::jmin (lowestHeard, n.pitch);
            low.notes.clear();
            for (const auto& n : guitar.notes)
                if (! n.keyswitch && n.pitch <= lowestHeard + 11)
                    low.notes.push_back (n);
            auto idea = doubledLine (low, spans, lowest, highest, "guitar");
            idea.style = "Droning with the guitar";
            idea.description = "The ringing chords' roots held as long as the guitar rings them, its slides and pushes doubled";
            return idea;
        }

        // (0.89.1, Circle With Me's keys) One note held over each chord, the chord's third, moving by the nearest step,
        // struck with the guitar (its pushes too). In the second half of every eight bars each new chord comes in
        // from two scale steps above, a quarter at a time, down onto its third.
        LineIdea guideTones (const std::vector<ChordSpan>& spans, double bpb, const std::vector<double>& strikes)
        {
            LineIdea out { LineIdea::Part::chords, "Droning: a held note a chord",
                           "Each chord's third held over it, moving by the nearest step, struck with the guitar; coming in from above in the second half of each eight bars", {} };
            const auto key = spans.front().key;
            const auto length = sectionLength (spans);
            const auto q = bpb / 4.0;
            auto previous = 71;
            for (size_t i = 0; i < strikes.size(); ++i)
            {
                const auto at = strikes[i];
                const auto next = i + 1 < strikes.size() ? strikes[i + 1] : length;
                const auto& span = chordStruckAt (spans, at);
                const auto pcs = span.chord.pitchClasses();
                const auto third = pcs.size() >= 2 ? pcs[1] : span.chord.root;
                auto pitch = -1;
                for (auto p = 64; p <= 79; ++p)
                    if (pc12 (p) == third && (pitch < 0 || std::abs (p - previous) < std::abs (pitch - previous)))
                        pitch = p;
                previous = pitch;
                auto from = at;
                const auto bar = (int) std::floor (at / bpb + 1.0e-6);
                if (bar % 8 >= 4 && next - at >= 3.0 * q - 1.0e-6)
                    for (int stepDown = 2; stepDown >= 1; --stepDown)
                    {
                        out.notes.push_back ({ from, q * 0.95, scaleStep (key, pitch, stepDown), 0.62f });
                        from += q;
                    }
                if (next > from + 1.0e-6)
                    out.notes.push_back ({ from, (next - from) * 0.96, pitch, 0.66f });
            }
            return out;
        }

        LineIdea droningKeysMore (int approach, const std::vector<ChordSpan>& spans, double bpb, const std::vector<double>& strikes);
        LineIdea droningDrumsMore (int approach, const std::vector<ChordSpan>& spans, double bpb);

        LineIdea droningKeys (const ComposerLane& lane, const std::vector<ChordSpan>& spans, double bpb, const std::vector<double>& strikes)
        {
            const auto approach = lane.approach >= 0 ? lane.approach % 7 : lane.style % 3;   // (0.94.0: chosen, else by style)
            if (approach >= 3)
                return droningKeysMore (approach, spans, bpb, strikes);   // (0.97.2)
            if (approach == 2 && ! strikes.empty())
                return guideTones (spans, bpb, strikes);
            const auto key = spans.front().key;
            const auto fifth = pitchOfClassIn ((key.pitchClass() + 7) % 12, 74, 85);
            const auto length = sectionLength (spans);
            const auto motif = approach == 1;
            LineIdea out { LineIdea::Part::chords, motif ? "Droning: a motif up high" : "Droning: a pulse up high",
                           motif ? "A short figure round the key's fifth over every chord, climbing a step at a time at the end of each four bars"
                                 : "The key's fifth up high over every chord, a steady pulse with the beats leaning in", {} };
            if (! motif)
            {
                const auto step = lane.busy > 0.6f ? 0.25 : lane.busy < 0.4f ? 0.5 : (lane.variation % 2 == 0 ? 0.25 : 0.5);
                for (auto t = 0.0; t < length - 1.0e-6; t += step)
                {
                    const auto onBeat = std::abs (t - std::round (t)) < 1.0e-6;
                    out.notes.push_back ({ t, step * 0.9, fifth, onBeat ? 0.72f : 0.52f });
                }
                return out;
            }
            // Two-bar figures in sixteenths: (place, scale steps from the fifth, length in sixteenths).
            struct Hit { int at, step, length; };
            static const std::vector<std::vector<Hit>> figures {
                { { 0, 0, 6 }, { 6, 0, 2 }, { 8, 1, 4 }, { 12, 0, 4 }, { 16, 0, 6 }, { 22, 0, 2 }, { 24, -1, 4 }, { 28, 0, 4 } },
                { { 0, 0, 3 }, { 3, 0, 3 }, { 6, 0, 2 }, { 8, 0, 8 }, { 16, 0, 3 }, { 19, 0, 3 }, { 22, 1, 2 }, { 24, 0, 8 } },
                { { 0, 0, 8 }, { 10, 1, 2 }, { 12, 0, 4 }, { 16, 0, 8 }, { 26, -1, 2 }, { 28, 0, 4 } },
            };
            const auto& figure = figures[(size_t) (lane.variation % (int) figures.size())];
            const auto bars = (int) std::ceil (length / bpb - 1.0e-6);
            const auto sixteenth = bpb / 16.0;   // (a 4/4 bar's sixteenth, scaled to the bar)
            for (int bar = 0; bar < bars; bar += 2)
            {
                const auto origin = bar * bpb;
                for (const auto& h : figure)
                {
                    const auto barOfHit = bar + h.at / 16;
                    if (barOfHit % 4 == 3)
                        continue;   // (the phrase's last bar climbs instead)
                    const auto at = origin + h.at * sixteenth;
                    if (at < length - 1.0e-6)
                        out.notes.push_back ({ at, juce::jmin (h.length * sixteenth * 0.95, length - at), scaleStep (key, fifth, h.step),
                                               h.at % 4 == 0 ? 0.72f : 0.58f });
                }
            }
            for (int bar = 3; bar < bars; bar += 4)
                for (int i = 0; i < 4; ++i)
                {
                    const auto at = bar * bpb + i * bpb / 4.0;
                    if (at < length - 1.0e-6)
                        out.notes.push_back ({ at, juce::jmin (bpb / 4.0 * 0.95, length - at), scaleStep (key, fifth, juce::jmin (i, 2)), 0.62f + 0.04f * (float) i });
                }
            std::stable_sort (out.notes.begin(), out.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
            return out;
        }

        // (0.89.1, Circle With Me's second guitar) Palm-muted eighths cycling the key's root, octave and fifth (three
        // notes against a bar of eight: the cell runs across the bar lines), the same under every chord; every fourth
        // bar's last two eighths slip a whole step down and back.
        LineIdea pedalCell (const std::vector<ChordSpan>& spans, double bpb, const GuitarVelocities& velocities, int lowest, int highest)
        {
            LineIdea out { LineIdea::Part::lead, "Droning: a muted pedal in threes",
                           "Palm-muted eighths on the key's root, octave and fifth, three against the bar, under every chord", {} };
            const auto key = spans.front().key;
            const auto root = pitchOfClassIn (key.pitchClass(), juce::jmax (lowest, 52), juce::jmax (lowest, 52) + 11);
            const std::array<int, 3> cell { root, root + 12 <= highest ? root + 12 : root, root + 7 <= highest ? root + 7 : root };
            const auto length = sectionLength (spans);
            const auto e = bpb / 8.0;
            int i = 0;
            for (auto t = 0.0; t < length - 1.0e-6; t += e, ++i)
            {
                auto pitch = cell[(size_t) (i % 3)];
                const auto bar = (int) std::floor (t / bpb + 1.0e-6);
                const auto place = (int) std::round ((t - bar * bpb) / e);
                if (bar % 4 == 3 && place == 6)
                    pitch -= 2;
                const auto velocity = GuitarVelocity::of (place % 2 == 0 ? velocities.muteMid : velocities.muteTight);
                out.notes.push_back ({ t, e * 0.9, pitch, velocity });
            }
            return out;
        }

        LineIdea droningLead (const ComposerLane& lane, const std::vector<ChordSpan>& spans, double bpb, const GuitarVelocities& velocities,
                              int lowest, int highest)
        {
            const auto approach = lane.approach >= 0 ? lane.approach % 3 : lane.style % 3;   // (0.94.0)
            if (approach == 2)
                return pedalCell (spans, bpb, velocities, lowest, highest);
            const auto onRoot = approach == 1;
            LineIdea out { LineIdea::Part::lead, onRoot ? "Droning: octaves on the roots" : "Droning: octaves on the thirds",
                           "A shimmer of octaves over each ringing chord, the key's note on top in the first half of each four bars", {} };
            const auto key = spans.front().key;
            const auto step = lane.busy < 0.3f ? 0.5 : 0.25;
            const auto low = juce::jlimit (lowest, juce::jmax (lowest, highest - 24), 52);
            const auto tonic = pitchOfClassIn (key.pitchClass(), low + 12, low + 23);
            const auto origin = spans.front().start;
            for (const auto& span : spans)
            {
                const auto& tones = span.chord.pitchClasses();
                const auto pc = onRoot || tones.size() < 2 ? span.chord.root : tones[1];
                const auto pitch = pitchOfClassIn (pc, low, low + 11);
                for (auto t = span.start - origin; t < span.end - origin - 1.0e-6; t += step)
                {
                    const auto inBar = std::fmod (t, bpb);
                    const auto bar = (int) std::floor (t / bpb + 1.0e-9);
                    const auto accent = inBar < 1.0e-6 || std::abs (inBar - std::round (inBar)) < 1.0e-6;
                    const auto velocity = GuitarVelocity::of (accent ? velocities.sustain : velocities.sustainSoft);
                    const auto length = juce::jmin (step * 0.9, span.end - origin - t);
                    out.notes.push_back ({ t, length, pitch, velocity });
                    if (pitch + 12 <= highest)
                        out.notes.push_back ({ t, length, pitch + 12, velocity });
                    if (bar % 4 < 2 && tonic > pitch + 12 && tonic <= highest)
                        out.notes.push_back ({ t, length, tonic, velocity });
                }
            }
            return out;
        }

        // (0.89.1, Circle With Me's drums: half time at a fast tempo) Cymbals on the half notes, the snare once every two
        // bars, the kick on the first bar's downbeat and on the guitar's pushes (a crash with each), the fourth bar's
        // kicks on beats 2, 3 and 4, the eighth's in eighths.
        LineIdea drumsOnPushes (const std::vector<ChordSpan>& spans, double bpb, const std::vector<double>& strikes)
        {
            LineIdea out { LineIdea::Part::drums, "Droning: locked to the pushes",
                           "Half time: cymbals on the half notes, the snare every second bar, the kick on the downbeats and the chords' pushes", {} };
            const auto length = sectionLength (spans);
            const auto bars = (int) std::ceil (length / bpb - 1.0e-6);
            const auto beat = bpb / 4.0;
            auto hit = [&out, length] (double at, int note, float velocity)
            {
                if (at >= -1.0e-9 && at < length - 1.0e-6
                    && std::none_of (out.notes.begin(), out.notes.end(), [&] (const LineNote& n) { return n.pitch == note && std::abs (n.beat - at) < 1.0e-6; }))
                    out.notes.push_back ({ at, 0.1, note, velocity });
            };
            for (int bar = 0; bar < bars; ++bar)
            {
                const auto o = bar * bpb;
                hit (o, bar % 2 == 0 ? 52 : 49, 0.85f);
                hit (o + 2 * beat, 52, 0.7f);
                if (bar % 2 == 0)
                    hit (o, 36, 0.95f);
                else
                    hit (o, 38, 0.95f);
                if (bar % 8 == 7)
                    for (int k = 2; k < 8; ++k)
                        hit (o + k * beat / 2.0, 36, 0.85f);
                else if (bar % 4 == 3)
                {
                    for (int k = 1; k < 4; ++k)
                        hit (o + k * beat, 36, 0.85f);
                    hit (o + 2 * beat, 49, 0.85f);
                }
            }
            for (const auto at : strikes)
            {
                const auto inBar = std::fmod (at, bpb);
                if (inBar > 1.0e-6 && bpb - inBar > 1.0e-6)
                {
                    hit (at, 36, 0.9f);
                    hit (at, 49, 0.88f);
                }
            }
            std::stable_sort (out.notes.begin(), out.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
            return out;
        }

        // (0.96.0, SPLIT CHUG: Call Me When You're Sober's verse) kick on 1 and 3, snare on 2 and 4, open hats on the
        // beats and the ride between them, a crash each four bars, the fourth bar's snare pushing into the next.
        LineIdea plainBackbeat (const std::vector<ChordSpan>& spans, double bpb)
        {
            LineIdea out { LineIdea::Part::drums, "Split: plain backbeat",
                           "Kick on 1 and 3, snare on 2 and 4, open hats on the beats with the ride between, the guitar's gaps left to the groove", {} };
            const auto length = sectionLength (spans);
            const auto bars = (int) std::ceil (length / bpb - 1.0e-6);
            const auto beat = bpb / 4.0;
            auto hit = [&out, length] (double at, int note, float velocity)
            {
                if (at >= -1.0e-9 && at < length - 1.0e-6)
                    out.notes.push_back ({ at, 0.1, note, velocity });
            };
            for (int bar = 0; bar < bars; ++bar)
            {
                const auto o = bar * bpb;
                if (bar % 4 == 0)
                    hit (o, 49, 0.9f);
                for (int q = 0; q < 4; ++q)
                {
                    hit (o + q * beat, 46, q == 0 ? 0.8f : 0.66f);
                    hit (o + (q + 0.5) * beat, 51, 0.55f);
                }
                hit (o, 36, 0.95f);
                hit (o + 2 * beat, 36, 0.9f);
                hit (o + beat, 38, 0.92f);
                hit (o + 3 * beat, 38, 0.92f);
                if (bar % 4 == 3)
                {
                    hit (o + 1.5 * beat, 38, 0.7f);
                    hit (o + 2.25 * beat, 38, 0.75f);
                }
            }
            return out;
        }

        // (0.96.0) Each bass note held through the gap to the next (a breath short), never past its chord's end.
        void holdThroughGaps (LineIdea& idea, const std::vector<ChordSpan>& spans)
        {
            auto& notes = idea.notes;
            std::sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
            const auto origin = spans.front().start;
            for (size_t i = 0; i < notes.size(); ++i)
            {
                if (notes[i].keyswitch)
                    continue;
                auto until = sectionLength (spans);
                for (size_t j = i + 1; j < notes.size(); ++j)
                    if (! notes[j].keyswitch && notes[j].beat > notes[i].beat + 1.0e-6)
                    {
                        until = notes[j].beat;
                        break;
                    }
                for (const auto& span : spans)
                    if (notes[i].beat + origin >= span.start - 1.0e-9 && notes[i].beat + origin < span.end - 1.0e-9)
                        until = juce::jmin (until, span.end - origin);
                notes[i].length = juce::jmax (notes[i].length, (until - notes[i].beat) * 0.96);
            }
            idea.style = "Split: held through the gaps";
            idea.description = "The guitar's hits doubled, each held through the gap to the next";
        }

        LineIdea droningDrums (const ComposerLane& lane, const std::vector<ChordSpan>& spans, double bpb, const std::vector<double>& strikes)
        {
            const auto approach = lane.approach >= 0 ? lane.approach % 7 : lane.variation % 3;   // (0.94.0: chosen, else by the take)
            if (approach >= 3)
                return droningDrumsMore (approach, spans, bpb);   // (0.97.2)
            if (approach == 2 && ! strikes.empty())
                return drumsOnPushes (spans, bpb, strikes);
            const auto halfTime = approach == 1;
            LineIdea out { LineIdea::Part::drums, halfTime ? "Droning: half time" : "Droning: open hats",
                           halfTime ? "Half time under the ringing chords: hats on the beats, the snare on 3, a fill into each four bars"
                                    : "Open hats on the eighths, kick on 1 and 3, snare on 2 and the and of 3, the fourth bar pushing", {} };
            const auto length = sectionLength (spans);
            const auto bars = (int) std::ceil (length / bpb - 1.0e-6);
            const auto beat = bpb / 4.0;   // (a 4/4 bar's beat, scaled to the bar)
            auto hit = [&out, length] (double at, int note, float velocity)
            {
                if (at >= -1.0e-9 && at < length - 1.0e-6)
                    out.notes.push_back ({ at, 0.1, note, velocity });
            };
            for (int bar = 0; bar < bars; ++bar)
            {
                const auto o = bar * bpb;
                const auto last = bar % 4 == 3;
                if (bar % 4 == 0)
                    hit (o, 49, 0.9f);
                if (! halfTime)
                {
                    for (int e = 0; e < 8; ++e)
                        hit (o + e * beat / 2.0, 46, e % 2 == 0 ? 0.78f : 0.6f);
                    hit (o, 36, 0.95f);
                    hit (o + 2 * beat, 36, 0.9f);
                    hit (o + beat, 38, 0.92f);
                    hit (o + 2.5 * beat, 38, 0.88f);
                    if (last)
                    {
                        hit (o + 1.5 * beat, 36, 0.85f);
                        hit (o + 3 * beat, 49, 0.88f);
                        hit (o + 3 * beat, 36, 0.9f);
                        hit (o + 3.5 * beat, 38, 0.9f);
                    }
                }
                else
                {
                    for (int q = 0; q < 4; ++q)
                        hit (o + q * beat, 42, q == 0 ? 0.8f : 0.62f);
                    hit (o, 36, 0.95f);
                    hit (o + 2 * beat, 38, 0.95f);
                    hit (o + (bar % 2 == 0 ? 0.75 : 1.5) * beat, 36, 0.8f);
                    if (bar % 2 == 1)
                        hit (o + 3.5 * beat, 36, 0.8f);
                    if (last)
                        for (int s = 0; s < 4; ++s)
                            hit (o + 3 * beat + s * beat / 4.0, s < 2 ? 38 : 45, 0.7f + 0.06f * (float) s);
                }
            }
            std::stable_sort (out.notes.begin(), out.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
            return out;
        }

        //======================================================================
        // (0.97.0) CLEAN VERSE's band, from the owner's verses (read for their shapes only):
        //   GUITAR  RINGING ARPEGGIO (Masterpiece): the chord picked a note at a time in a pushed eighth pattern, the
        //           low note on each chord's root, the top two notes the key's 2nd and 3rd ringing over every chord.
        //           PICKED CHORD (Circle With Me's second clean guitar): a close three-note shape picked in steady
        //           eighths, low-mid-high in threes against the bar. SPARSE CLEAN NOTES (Circle With Me's first clean
        //           guitar, Clown's clean line): one high chord tone struck twice a bar and held, stepping down every
        //           two bars, the last bar of eight picked in eighths. GUITARS OUT (Crawling, In The End): none.
        //           HELD CHORD & PULSE (The End of the Dream): a power chord struck and rung two bars.
        //   BASS    syncopated roots (Masterpiece), eighths changing an eighth late (Circle With Me), whole notes
        //           (Crawling), octave eighths (The End of the Dream).
        //   DRUMS   a light groove with a pushed kick (Masterpiece), half time with ghost snares and sixteenth hats
        //           (Crawling), none (Circle With Me).
        //   KEYS    held pads (Masterpiece), a pulse in sixteenths on the key's fifth (Crawling, The End of the
        //           Dream), a piano line in quarters (In The End), silent (Circle With Me).
        // Left on AUTOMATIC, the bass, drums and keys take the option that went with the guitar's in the songs.
        int cleanGuitarApproach (const ComposerState& s)
        {
            const auto& lane = s.lane (ComposerLayer::guitar);
            return lane.approach >= 0 ? lane.approach % 10 : lane.variation % 5;   // (0.100.0: two more, 0.101.0: three, chosen only)
        }

        int cleanApproach (const ComposerState& s, ComposerLayer l)
        {
            const auto& lane = s.lane (l);
            const auto count = l == ComposerLayer::bass ? 5 : l == ComposerLayer::drums ? 6 : 5;   // (0.100.0: the in-halfway and the drone; 0.101.0: two drums; chosen only)
            if (lane.approach >= 0)
                return lane.approach % count;
            const auto other = lane.variation % 2;
            switch (cleanGuitarApproach (s))
            {
                case 0: return 0;                                                      // Masterpiece
                case 1: return l == ComposerLayer::bass ? 1 : l == ComposerLayer::drums ? 2 : 3;   // Circle With Me
                case 2: return l == ComposerLayer::bass ? 1 : l == ComposerLayer::drums ? 2 : 0;   // its first guitar
                case 3: return l == ComposerLayer::bass ? (other == 0 ? 2 : 0) : l == ComposerLayer::drums ? 1 : (other == 0 ? 1 : 2);   // Crawling / In The End
                case 7:
                case 8: return l == ComposerLayer::bass ? 1 : l == ComposerLayer::drums ? 4 : 3;   // (0.101.0) Circle With Me, as the tab
                case 9: return l == ComposerLayer::bass ? 2 : l == ComposerLayer::drums ? 4 : 3;   // (0.101.0) The Void
                default: return l == ComposerLayer::bass ? 3 : l == ComposerLayer::drums ? 0 : 1;   // The End of the Dream
            }
        }

        // The places a part changes: every bar line and every chord change, in idea beats.
        std::vector<double> cleanSegments (const std::vector<ChordSpan>& spans, double bpb)
        {
            const auto length = sectionLength (spans);
            std::vector<double> at;
            for (auto t = 0.0; t < length - 1.0e-6; t += bpb)
                at.push_back (t);
            for (const auto& span : spans)
                if (const auto t = span.start - spans.front().start; t > 1.0e-6 && t < length - 1.0e-6)
                    at.push_back (t);
            std::sort (at.begin(), at.end());
            at.erase (std::unique (at.begin(), at.end(), [] (double a, double b) { return b - a < 1.0e-6; }), at.end());
            at.push_back (length);
            return at;
        }

        const Chord& chordAtIdea (const std::vector<ChordSpan>& spans, double t) { return spanAtIdeaBeat (spans, t)->chord; }

        void addNote (LineIdea& idea, double at, double length, int pitch, float velocity, double end)
        {
            if (at < end - 1.0e-6)
                idea.notes.push_back ({ at, juce::jmax (0.01, juce::jmin (length, end - at)), pitch, velocity });
        }

        void sortNotes (LineIdea& idea)
        {
            std::stable_sort (idea.notes.begin(), idea.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        }

        LineIdea cleanGuitar (int approach, const ComposerLane& lane, const std::vector<ChordSpan>& spans, double bpb,
                              const GuitarTuning& tuning, const GuitarVelocities& velocities)
        {
            const auto length = sectionLength (spans);
            const auto bars = (int) std::ceil (length / bpb - 1.0e-6);
            const auto s16 = bpb / 16.0;
            const auto key = spans.front().key;
            const auto low = tuning.lowest();
            const auto soft = GuitarVelocity::of (velocities.open (velocities.sustainSoft));
            const auto firm = GuitarVelocity::of (velocities.open (velocities.sustain));
            LineIdea out { LineIdea::Part::rhythm, {}, {}, {} };
            switch (approach)
            {
                case 0:
                {
                    out.style = "Clean: ringing arpeggio";
                    out.description = "The chord picked a note at a time, the root under it, the key's 2nd and 3rd ringing on top over every chord";
                    const auto tonic = pitchOfClassIn (key.pitchClass(), 55, 66);
                    const auto top1 = scaleStep (key, tonic, 1), top2 = scaleStep (key, tonic, 2);
                    // (place, which: 0 root, 1 top, 2 higher top, 3 the chord's fifth; length in sixteenths)
                    struct Pick { int at, which; double length; };
                    static const std::array<std::vector<Pick>, 2> patterns { {
                        { { 0, 0, 3.5 }, { 2, 1, 3 }, { 4, 0, 6 }, { 5, 1, 3 }, { 8, 2, 3 }, { 10, 0, 6 }, { 11, 3, 3 }, { 14, 2, 2 } },
                        { { 0, 0, 4 }, { 2, 1, 4 }, { 3, 2, 3 }, { 6, 0, 4 }, { 8, 1, 3 }, { 10, 2, 4 }, { 12, 0, 4 }, { 14, 3, 2 } },
                    } };
                    const auto& pattern = patterns[(size_t) ((lane.variation / 5) % 2)];
                    for (int bar = 0; bar < bars; ++bar)
                        for (const auto& p : pattern)
                        {
                            const auto at = bar * bpb + p.at * s16;
                            if (at >= length - 1.0e-6)
                                continue;
                            const auto& chord = chordAtIdea (spans, at);
                            const auto root = rootNear (chord, low + 7, low + 19);
                            const auto pcs = chord.pitchClasses();
                            const auto fifth = pcs.size() >= 3 ? root + ((pcs[2] - pc12 (root) + 12) % 12) : root + 7;
                            const int pitch = p.which == 0 ? root : p.which == 1 ? top1 : p.which == 2 ? top2 : fifth;
                            addNote (out, at, p.length * s16 * 0.96, pitch, p.which == 0 ? firm : soft, length);
                        }
                    break;
                }
                case 1:
                {
                    out.style = "Clean: picked chord";
                    out.description = "A close three-note shape of each chord picked in steady eighths, low, middle, high in threes across the bar";
                    std::vector<int> previous;
                    static const std::array<int, 8> order { 0, 1, 2, 0, 1, 2, 1, 2 };
                    const auto e = bpb / 8.0;
                    for (int i = 0; i * e < length - 1.0e-6; ++i)
                    {
                        const auto at = i * e;
                        const auto voicings = voicingsFor (chordAtIdea (spans, at), 55);
                        if (voicings.empty())
                            continue;
                        const auto& shape = voicings[(size_t) juce::jlimit (0, (int) voicings.size() - 1, closestVoicing (voicings, previous, previous))];
                        previous = shape;
                        const auto pick = juce::jmin ((int) shape.size() - 1, order[(size_t) (i % 8)]);
                        addNote (out, at, e * 2.0 * 0.96, shape[(size_t) pick], i % 2 == 0 ? firm : soft, length);
                    }
                    break;
                }
                case 2:
                {
                    out.style = "Clean: sparse notes";
                    out.description = "One high chord tone struck twice a bar and held, stepping down every two bars; the last bar of eight picked in eighths";
                    auto aim = 76 - (lane.variation / 5) % 3;
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        if (bar > 0 && bar % 2 == 0)
                            aim -= 2;
                        if (aim < 66)
                            aim = 76;
                        const auto from = bar * bpb;
                        const auto& chord = chordAtIdea (spans, from);
                        const auto pcs = chord.pitchClasses();   // (one list: begin and end from the same call)
                        auto pitch = -1;
                        for (auto p = 64; p <= 79; ++p)
                            if (std::find (pcs.begin(), pcs.end(), pc12 (p)) != pcs.end() && (pitch < 0 || std::abs (p - aim) < std::abs (pitch - aim)))
                                pitch = p;
                        if (pitch < 0)
                            pitch = aim;
                        const auto barEnd = juce::jmin (length, from + bpb);
                        if (bar % 8 == 7)
                        {
                            for (int i = 0; i < 8; ++i)
                                addNote (out, from + i * bpb / 8.0, bpb / 8.0 * 0.9, pitch, i == 0 ? firm : soft, barEnd);
                            continue;
                        }
                        addNote (out, from, 6 * s16 * 0.96, pitch, firm, barEnd);
                        addNote (out, from + 6 * s16, (barEnd - from - 6 * s16) * 0.96, pitch, soft, barEnd);
                    }
                    break;
                }
                case 3:
                    out.style = "Clean: guitars out";
                    out.description = "No guitar in the verse: the keys carry it";
                    break;
                default:
                {
                    out.style = "Clean: held chord";
                    out.description = "A power chord struck and left ringing for two bars, or until the chord changes";
                    const auto segments = cleanSegments (spans, bpb);
                    for (size_t i = 0; i + 1 < segments.size(); ++i)
                    {
                        const auto at = segments[i];
                        const auto bar = (int) std::floor (at / bpb + 1.0e-6);
                        const auto& chord = chordAtIdea (spans, at);
                        const auto onBar = std::abs (at - bar * bpb) < 1.0e-6;
                        if (onBar && bar % 2 == 1 && i > 0 && &chordAtIdea (spans, segments[i - 1]) == &chord)
                            continue;   // (held over from the bar before)
                        auto until = length;
                        for (size_t j = i + 1; j + 1 < segments.size(); ++j)
                        {
                            const auto nb = (int) std::floor (segments[j] / bpb + 1.0e-6);
                            const auto nOnBar = std::abs (segments[j] - nb * bpb) < 1.0e-6;
                            if (&chordAtIdea (spans, segments[j]) != &chord || (nOnBar && nb % 2 == 0))
                            {
                                until = segments[j];
                                break;
                            }
                        }
                        const auto root = rootNear (chord, low, low + 11);
                        for (const auto pitch : { root, root + 7, root + 12 })
                            addNote (out, at, (until - at) * 0.96, pitch, firm, length);
                    }
                    break;
                }
            }
            sortNotes (out);
            return out;
        }

        LineIdea cleanBass (int approach, const std::vector<ChordSpan>& spans, double bpb, int lowest, int highest)
        {
            const auto length = sectionLength (spans);
            const auto bars = (int) std::ceil (length / bpb - 1.0e-6);
            const auto s16 = bpb / 16.0;
            const auto top = juce::jmin (highest, lowest + 11);
            auto rootAt = [&] (double t) { return rootNear (chordAtIdea (spans, t), lowest, top); };
            LineIdea out { LineIdea::Part::bass, {}, {}, {} };
            switch (approach)
            {
                case 0:
                {
                    out.style = "Clean: syncopated roots";
                    out.description = "The chord's root pushed: a short hit and a long one on the beat, then two more off the beat";
                    struct Hit { int at; double length; float velocity; };
                    static const std::array<Hit, 4> hits { { { 0, 0.7, 0.85f }, { 1, 5.8, 0.78f }, { 7, 2.8, 0.75f }, { 10, 5.8, 0.78f } } };
                    for (int bar = 0; bar < bars; ++bar)
                        for (const auto& h : hits)
                        {
                            const auto at = bar * bpb + h.at * s16;
                            addNote (out, at, h.length * s16, rootAt (at), h.velocity, length);
                        }
                    break;
                }
                case 1:
                {
                    out.style = "Clean: eighths, changing late";
                    out.description = "Steady eighths on the root, each change landing an eighth after the chord's";
                    const auto e = bpb / 8.0;
                    for (int i = 0; i * e < length - 1.0e-6; ++i)
                        addNote (out, i * e, e * 0.9, rootAt (juce::jmax (0.0, (i - 1) * e)), i % 2 == 0 ? 0.8f : 0.68f, length);
                    break;
                }
                case 2:
                {
                    out.style = "Clean: whole notes";
                    out.description = "Each chord's root held under it, struck again on every bar";
                    const auto segments = cleanSegments (spans, bpb);
                    for (size_t i = 0; i + 1 < segments.size(); ++i)
                        addNote (out, segments[i], (segments[i + 1] - segments[i]) * 0.97, rootAt (segments[i]), 0.82f, length);
                    break;
                }
                default:
                {
                    out.style = "Clean: octave eighths";
                    out.description = "Eighths on the root, jumping to its octave and back";
                    const auto e = bpb / 8.0;
                    static const std::array<int, 8> jumps { 0, 0, 12, 0, 0, 12, 0, 12 };
                    for (int i = 0; i * e < length - 1.0e-6; ++i)
                    {
                        const auto root = rootAt (i * e);
                        const auto up = root + jumps[(size_t) (i % 8)] <= highest ? jumps[(size_t) (i % 8)] : 0;
                        addNote (out, i * e, e * 0.9, root + up, i % 2 == 0 ? 0.82f : 0.7f, length);
                    }
                    break;
                }
            }
            sortNotes (out);
            return out;
        }

        LineIdea cleanDrums (int approach, const std::vector<ChordSpan>& spans, double bpb)
        {
            const auto length = sectionLength (spans);
            const auto bars = (int) std::ceil (length / bpb - 1.0e-6);
            const auto s16 = bpb / 16.0;
            LineIdea out { LineIdea::Part::drums, {}, {}, {} };
            auto hit = [&] (double at, int note, float velocity)
            {
                if (at >= -1.0e-9 && at < length - 1.0e-6)
                    out.notes.push_back ({ at, 0.1, note, velocity });
            };
            switch (approach)
            {
                case 0:
                    out.style = "Clean: light groove";
                    out.description = "Kick on 1 with a push after it, snare on 2 and 4, closed hats on the eighths opening each four bars";
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        if (bar % 4 == 0)
                            hit (o, 49, 0.8f);
                        for (int i = 0; i < 8; ++i)
                            hit (o + i * 2 * s16, bar % 4 == 0 && i == 1 ? 46 : 42, i % 2 == 0 ? 0.62f : 0.48f);
                        for (const auto k : bar % 2 == 0 ? std::vector<int> { 0, 1, 9 } : std::vector<int> { 0, 1, 7, 10 })
                            hit (o + k * s16, 36, k == 0 ? 0.88f : 0.74f);
                        hit (o + 4 * s16, 38, 0.82f);
                        hit (o + 12 * s16, 38, 0.82f);
                    }
                    break;
                case 1:
                    out.style = "Clean: half time";
                    out.description = "Kick on 1 and 3, the snare only ghosted, sixteenth hats; every fourth bar's kick busier";
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        if (bar % 8 == 0)
                            hit (o, 49, 0.75f);
                        for (int i = 0; i < 16; ++i)
                            hit (o + i * s16, 42, i % 4 == 0 ? 0.6f : 0.42f);
                        hit (o, 36, 0.88f);
                        hit (o + 8 * s16, 36, 0.82f);
                        if (bar % 4 == 3)
                        {
                            hit (o + 2 * s16, 36, 0.7f);
                            hit (o + 9 * s16, 36, 0.7f);
                        }
                        hit (o + 4 * s16, 38, 0.32f);
                        hit (o + 10 * s16, 38, 0.32f);
                    }
                    break;
                case 4:
                    // (0.101.0, Circle With Me's first verse, the tab at its written tempo) half time: the hats on the
                    // eighths, every third one opened a little (x x o x), the snare on 3, a few kicks round the first
                    // beat; a crash opens each eight bars and the last bar's second half rolls on the toms.
                    out.style = "Clean: half-time hats";
                    out.description = "Hats on the eighths (x x o x), the snare on 3, a few kicks round beat 1; toms into the next section";
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto last = bar == bars - 1;
                        if (bar % 8 == 0)
                            hit (o, 49, 0.78f);
                        for (int i = 0; i < 8; ++i)
                            if (! (last && i >= 4) && ! (bar % 8 == 0 && i == 0))
                                hit (o + i * 2 * s16, i % 4 == 2 ? 46 : 42, i % 4 == 2 ? 0.5f : (i % 2 == 0 ? 0.62f : 0.46f));
                        for (const auto k : bar % 2 == 0 ? std::vector<int> { 0, 2, 6 } : std::vector<int> { 0, 6, 10 })
                            hit (o + k * s16, 36, k == 0 ? 0.88f : 0.72f);
                        hit (o + 8 * s16, 38, 0.86f);
                        if (last)
                            for (int i = 0; i < 4; ++i)
                                hit (o + (8 + 2 * i) * s16, i < 2 ? 45 : 41, 0.7f + 0.05f * (float) i);
                    }
                    break;
                case 5:
                    // (0.101.0, Circle With Me's second verse) the same energy in another colour: kick and toms on the
                    // eighths, the hat's foot on the beats, a flammed snare on 4.
                    out.style = "Clean: tom groove";
                    out.description = "Kick and toms on the eighths, the hat's foot on every beat, a flammed snare on beat 4";
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        if (bar % 8 == 0)
                            hit (o, 49, 0.72f);
                        for (int b = 0; b < 4; ++b)
                            hit (o + b * 4 * s16, 44, 0.55f);
                        static const std::array<int, 8> drum { 36, 45, 43, 36, 36, 43, 0, 45 };
                        for (int i = 0; i < 8; ++i)
                            if (drum[(size_t) i] != 0)
                                hit (o + i * 2 * s16, drum[(size_t) i], drum[(size_t) i] == 36 ? 0.84f : (i % 2 == 0 ? 0.7f : 0.6f));
                        hit (o + 12 * s16 - 0.03, 38, 0.45f);
                        hit (o + 12 * s16, 38, 0.86f);
                    }
                    break;
                default:
                    out.style = "Clean: no drums";
                    out.description = "The drums sit the verse out";
                    break;
            }
            sortNotes (out);
            return out;
        }

        LineIdea cleanKeys (int approach, const std::vector<ChordSpan>& spans, double bpb)
        {
            const auto length = sectionLength (spans);
            const auto bars = (int) std::ceil (length / bpb - 1.0e-6);
            const auto key = spans.front().key;
            LineIdea out { LineIdea::Part::chords, {}, {}, {} };
            switch (approach)
            {
                case 0:
                {
                    out.style = "Clean: held pads";
                    out.description = "Each chord held softly under the verse, moving to the nearest shape";
                    const auto segments = cleanSegments (spans, bpb);
                    std::vector<int> previous;
                    for (size_t i = 0; i + 1 < segments.size(); ++i)
                    {
                        const auto voicings = voicingsFor (chordAtIdea (spans, segments[i]), 55);
                        if (voicings.empty())
                            continue;
                        const auto& shape = voicings[(size_t) juce::jlimit (0, (int) voicings.size() - 1, closestVoicing (voicings, previous, previous))];
                        previous = shape;
                        for (const auto p : shape)
                            addNote (out, segments[i], (segments[i + 1] - segments[i]) * 0.98, p, 0.42f, length);
                    }
                    break;
                }
                case 1:
                {
                    out.style = "Clean: sixteenth pulse";
                    out.description = "The key's fifth pulsed in sixteenths over every chord, an octave down in the second half of each eight bars";
                    const auto fifth = pitchOfClassIn ((key.pitchClass() + 7) % 12, 68, 79);
                    const auto s16 = bpb / 16.0;
                    for (int i = 0; i * s16 < length - 1.0e-6; ++i)
                    {
                        const auto bar = (int) std::floor (i * s16 / bpb + 1.0e-6);
                        addNote (out, i * s16, s16 * 0.9, bar % 8 >= 4 ? fifth - 12 : fifth, i % 4 == 0 ? 0.66f : 0.52f, length);
                    }
                    break;
                }
                case 2:
                {
                    out.style = "Clean: piano line";
                    out.description = "A low piano line in quarters on each chord's root, fifth and third, the second bar of each pair pushing on eighths";
                    const auto q = bpb / 4.0;
                    for (int bar = 0; bar < bars; ++bar)
                        for (int beat = 0; beat < 4; ++beat)
                        {
                            const auto at = bar * bpb + beat * q;
                            const auto& chord = chordAtIdea (spans, at);
                            const auto pcs = chord.pitchClasses();
                            const auto root = pitchOfClassIn (chord.root, 41, 52);
                            auto above = [&] (size_t i) { return pcs.size() > i ? root + ((pcs[i] - chord.root + 12) % 12) : root + 7; };
                            if (bar % 2 == 0)
                                addNote (out, at, q * 0.95, beat == 0 ? root : beat == 3 ? above (1) : above (2), beat == 0 ? 0.66f : 0.58f, length);
                            else if (beat < 3)
                                addNote (out, at, q * 0.95, root, beat == 0 ? 0.66f : 0.56f, length);
                            else
                            {
                                addNote (out, at, q * 0.45, root, 0.56f, length);
                                addNote (out, at + q * 0.5, q * 0.45, above (1), 0.58f, length);
                            }
                        }
                    break;
                }
                default:
                    out.style = "Clean: keys out";
                    out.description = "No keys in the verse";
                    break;
            }
            sortNotes (out);
            return out;
        }

        // (0.100.0, the owner's Another Life verse; shapes only) PEDAL HOOK: the key's tonic picked low in eighths, a
        // short hook over it on the key's fifth, the step above it and the step under it, the same notes over every
        // chord; MUTED DYAD WALK: palm-muted two-note shapes in eighths, the root under and the top stepping inside the
        // chord (its fifth, the step above, back, the step under).
        LineIdea cleanGuitarMore (int approach, const std::vector<ChordSpan>& spans, double bpb, const GuitarTuning& tuning,
                                  const GuitarVelocities& velocities)
        {
            const auto length = sectionLength (spans);
            const auto bars = (int) std::ceil (length / bpb - 1.0e-6);
            const auto e = bpb / 8.0;
            const auto key = spans.front().key;
            const auto low = tuning.lowest();
            const auto soft = GuitarVelocity::of (velocities.open (velocities.sustainSoft));
            const auto firm = GuitarVelocity::of (velocities.open (velocities.sustain));
            const auto muted = GuitarVelocity::of (velocities.muteMid);
            LineIdea out { LineIdea::Part::rhythm, {}, {}, {} };
            if (approach == 5)
            {
                out.style = "Clean: pedal hook";
                out.description = "The key's note picked low in eighths, a short hook on top (the fifth, the step above, the step under) over every chord";
                const auto pedal = pitchOfClassIn (key.pitchClass(), low + 7, low + 18);
                const auto fifth = pitchOfClassIn ((key.pitchClass() + 7) % 12, 64, 75);
                const auto above = scaleStep (key, fifth, 1), under = scaleStep (key, fifth, -1);
                for (int i = 0; i * e < length - 1.0e-6; ++i)
                    addNote (out, i * e, e * 0.9, pedal, i % 2 == 0 ? soft : soft * 0.9f, length);
                for (int bar = 0; bar < bars; ++bar)
                {
                    const auto o = bar * bpb;
                    const auto q = bpb / 4.0;
                    if (bar % 2 == 0)
                    {
                        addNote (out, o + q, 1.5 * q, fifth, firm, length);
                        addNote (out, o + 2.5 * q, 0.5 * q, above, firm, length);
                        addNote (out, o + 3 * q, q * 0.95, fifth, firm, length);
                    }
                    else
                    {
                        addNote (out, o + q, 2 * q, under, firm, length);
                        addNote (out, o + 3 * q, q * 0.95, fifth, firm, length);
                    }
                }
            }
            else
            {
                out.style = "Clean: muted dyad walk";
                out.description = "Palm-muted two-note shapes in eighths: the root under, the top stepping inside the chord";
                static const std::array<int, 8> steps { 0, 0, 1, 0, 0, 0, -1, 0 };
                for (int i = 0; i * e < length - 1.0e-6; ++i)
                {
                    const auto at = i * e;
                    const auto* span = spanAtIdeaBeat (spans, at);
                    const auto root = rootNear (span->chord, low + 7, low + 18);
                    const auto& tones = span->chord.pitchClasses();
                    const auto fifth = tones.size() >= 3 ? pitchOfClassIn (tones[2], root + 1, root + 12) : root + 7;
                    auto top = scaleStep (span->key, fifth, steps[(size_t) (i % 8)]);
                    if (top <= root)
                        top = fifth;
                    addNote (out, at, e * 0.8, root, muted, length);
                    addNote (out, at, e * 0.8, top, muted, length);
                }
            }
            sortNotes (out);
            return out;
        }

        // (0.101.0, the owner's Circle With Me verse, its second clean guitar; shapes only) STILL SHAPE IN THREES: the
        // key's own tonic chord in a close shape up high (tonic, third, fifth), picked low, middle, high around and
        // around over straight eighths, palm-muted, and it never moves: the bass changes under it, so the same three
        // notes take a new colour on every chord. Three notes over eight eighths come round only every two bars, so
        // the cycle starts again there; every fourth bar's seventh eighth swaps the top note for the step under it
        // (the open top string in the song). SIXTEENTHS: the same for a song set at its half-time tempo (the tab's
        // eighths are sixteenths there), starting again every bar, the swap every second bar.
        // STILL CLUSTER (the owner's The Void and Jaded verses and bridges): the same idea on the key's third, fifth
        // and sixth (a minor key's b3, 5, b6: a close cluster inside a fourth), picked middle, top, low; the cycle's
        // last eighth drops to the key's note under it (The Void).
        LineIdea cleanGuitarStill (int kind, const std::vector<ChordSpan>& spans, double bpb, const GuitarVelocities& velocities)
        {
            const auto length = sectionLength (spans);
            const auto key = spans.front().key;
            const auto sixteenths = kind == 1, cluster = kind == 2;
            const auto tonic = pitchOfClassIn (key.pitchClass(), 55, 66);
            const auto third = pitchOfClassIn (pc12 (scaleStep (key, tonic, 2)), 57, 68);
            const std::array<int, 3> shape = cluster ? std::array<int, 3> { scaleStep (key, third, 2), scaleStep (key, third, 3), third }
                                                     : std::array<int, 3> { tonic, scaleStep (key, tonic, 2), scaleStep (key, tonic, 4) };
            const auto swap = scaleStep (key, shape[2], -1);
            const auto under = pitchOfClassIn (key.pitchClass(), third - 11, third - 1);
            const auto muted = GuitarVelocity::of (velocities.muteLoose);
            LineIdea out { LineIdea::Part::rhythm,
                           cluster ? "Clean: still cluster" : sixteenths ? "Clean: still shape, sixteenths" : "Clean: still shape in threes",
                           cluster ? juce::String ("The key's third, fifth and sixth held close and picked middle, top, low in threes over eighths, palm-muted; it never moves")
                                   : juce::String ("The key's own chord held up high and picked low, middle, high in threes over ")
                                         + (sixteenths ? "sixteenths" : "eighths") + ", palm-muted; it never moves, the bass changes under it", {} };
            const auto step = sixteenths ? bpb / 16.0 : bpb / 8.0;
            const auto perCycle = 16;   // (two bars of eighths, or one of sixteenths)
            const auto swapEvery = sixteenths ? 2 : 4;    // bars
            for (int i = 0; i * step < length - 1.0e-6; ++i)
            {
                const auto at = i * step;
                const auto inCycle = i % perCycle;
                const auto bar = (int) std::floor (at / bpb + 1.0e-6);
                const auto inBar = sixteenths ? i % 16 : i % 8;
                auto pitch = shape[(size_t) (inCycle % 3)];
                if (cluster && inCycle == perCycle - 1)
                    pitch = under;
                else if (! cluster && bar % swapEvery == swapEvery - 1 && inBar == (sixteenths ? 14 : 6) && inCycle % 3 == 2)
                    pitch = swap;
                addNote (out, at, step * 0.92, pitch, inBar % (sixteenths ? 4 : 2) == 0 ? muted : muted * 0.9f, length);
            }
            sortNotes (out);
            return out;
        }

        // (0.101.0, the owner's Circle With Me verse, its first clean guitar; shapes only) SWELL MELODY: single notes
        // faded in (a swell: expression from nothing) and held, stepping down the key's scale from its third: two
        // bars a note at first, then one bar each in the last quarter, down under the tonic (5, 4, 3, 2): the verse
        // quietly speeds up. Laid over the section's length whatever it is (16 units: 2 2 4 2 2 1 1 1 1). SWELL,
        // THEN WARBLE (its second verse): each note held for its first half, then picked again in eighths, every
        // other one bent a quarter tone, a shimmer.
        LineIdea cleanLead (int approach, const std::vector<ChordSpan>& spans, double bpb, const GuitarVelocities& velocities)
        {
            const auto length = sectionLength (spans);
            const auto key = spans.front().key;
            const auto tonic = pitchOfClassIn (key.pitchClass(), 64, 75);
            const auto warble = approach == 1;
            const auto held = GuitarVelocity::of (velocities.open (velocities.sustain));
            const auto soft = GuitarVelocity::of (velocities.open (velocities.sustainSoft));
            LineIdea out { LineIdea::Part::lead, warble ? "Clean: swell, then warble" : "Clean: swell melody",
                           warble ? "Swelled notes stepping down the scale, each picked again in eighths in its second half, every other one bent a quarter tone"
                                  : "Single notes swelled in and held, stepping down the key's scale; two bars a note, then one bar each near the end", {} };
            struct Step { int degree, units; };
            static const std::array<Step, 9> plan { { { 2, 2 }, { 1, 2 }, { 0, 4 }, { 2, 2 }, { 1, 2 }, { -3, 1 }, { -4, 1 }, { -5, 1 }, { -6, 1 } } };
            const auto unit = length / 16.0;
            const auto q = bpb / 4.0;
            auto snap = [&] (double t) { return juce::jmin (length, std::round (t / q) * q); };
            auto units = 0;
            for (const auto& p : plan)
            {
                const auto from = snap (units * unit);
                units += p.units;
                const auto to = snap (units * unit);
                if (to - from < 1.0e-6)
                    continue;
                const auto pitch = scaleStep (key, tonic, p.degree);
                if (! warble || to - from < bpb - 1.0e-6)
                {
                    addNote (out, from, (to - from) * 0.97, pitch, held, length);
                    out.notes.back().effect = LineNote::Effect::swell;
                    continue;
                }
                const auto half = snap (from + (to - from) * 0.5);
                addNote (out, from, (half - from) * 0.97, pitch, held, length);
                out.notes.back().effect = LineNote::Effect::swell;
                const auto e = bpb / 8.0;
                for (int i = 0; half + i * e < to - 1.0e-6; ++i)
                {
                    addNote (out, half + i * e, e * 0.9, pitch, soft, to);
                    if (i % 2 == 1)
                    {
                        out.notes.back().effect = LineNote::Effect::bend;
                        out.notes.back().amount = 0.5f;
                    }
                }
            }
            sortNotes (out);
            return out;
        }

        // (0.100.0, Another Life's keys) one octave of the key's note held two bars at a time through every chord.
        LineIdea cleanKeysDrone (const std::vector<ChordSpan>& spans, double bpb)
        {
            LineIdea out { LineIdea::Part::chords, "Clean: octave drone", "The key's note and its octave held two bars at a time through every chord", {} };
            const auto length = sectionLength (spans);
            const auto tonic = pitchOfClassIn (spans.front().key.pitchClass(), 60, 71);
            for (auto t = 0.0; t < length - 1.0e-6; t += 2 * bpb)
                for (const auto p : { tonic, tonic + 12 })
                    addNote (out, t, juce::jmin (2 * bpb, length - t) * 0.97, p, 0.44f, length);
            sortNotes (out);
            return out;
        }

        // (0.100.0, Another Life's first verse) the band out for the first half of the section, in for the second.
        void outUntilHalfway (LineIdea& idea, const std::vector<ChordSpan>& spans, double bpb)
        {
            const auto bars = (int) std::ceil (sectionLength (spans) / bpb - 1.0e-6);
            const auto half = (bars / 2) * bpb;
            idea.notes.erase (std::remove_if (idea.notes.begin(), idea.notes.end(), [half] (const LineNote& n) { return n.beat < half - 1.0e-6; }),
                              idea.notes.end());
            idea.style += ", in halfway";
            idea.description = "Out for the first half of the section, then: " + idea.description.substring (0, 1).toLowerCase() + idea.description.substring (1);
        }

        // CLEAN VERSE's part for a lane (none: the melody and the lead play as usual).
        std::optional<LineIdea> cleanLayer (ComposerLayer l, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
        {
            if (spans.empty() || sectionBand (s) != SectionType::Band::clean)
                return std::nullopt;
            switch (l)
            {
                case ComposerLayer::guitar:
                {
                    const auto& profile = c.guitarProfile;
                    const auto tuning = profile.has_value() ? profile->tuning() : c.tuning;
                    const auto velocities = profile.has_value() && profile->velocities.has_value() ? *profile->velocities : c.velocities;
                    const auto approach = cleanGuitarApproach (s);
                    auto idea = approach >= 7 ? cleanGuitarStill (approach - 7, spans, c.beatsPerBar, velocities)
                              : approach >= 5 ? cleanGuitarMore (approach, spans, c.beatsPerBar, tuning, velocities)
                                              : cleanGuitar (approach, s.lane (l), spans, c.beatsPerBar, tuning, velocities);
                    if (profile.has_value())
                        keepInRange (idea, profile->rangeLow(), profile->rangeHigh());
                    return idea;
                }
                case ComposerLayer::bass:
                {
                    const auto& profile = c.bassProfile;
                    const auto lowest = profile.has_value() ? profile->rangeLow() : c.bassTuning.lowest();
                    const auto highest = profile.has_value() ? profile->rangeHigh() : bassTop (c.bassTuning);
                    const auto approach = cleanApproach (s, l);
                    auto idea = cleanBass (approach == 4 ? 0 : approach, spans, c.beatsPerBar, lowest, highest);
                    if (approach == 4)
                        outUntilHalfway (idea, spans, c.beatsPerBar);
                    return idea;
                }
                case ComposerLayer::drums:
                {
                    const auto approach = cleanApproach (s, l);
                    auto idea = cleanDrums (approach == 3 ? 1 : approach, spans, c.beatsPerBar);
                    if (approach == 3)
                        outUntilHalfway (idea, spans, c.beatsPerBar);
                    return idea;
                }
                case ComposerLayer::chords:
                {
                    const auto approach = cleanApproach (s, l);
                    return approach == 4 ? cleanKeysDrone (spans, c.beatsPerBar) : cleanKeys (approach, spans, c.beatsPerBar);
                }
                case ComposerLayer::lead:
                {
                    // (0.101.0) only when an option is chosen: AUTOMATIC is the lead's own style, as before.
                    const auto approach = s.lane (l).approach;
                    if (approach < 0)
                        break;
                    const auto& profile = c.leadProfile.has_value() ? c.leadProfile : c.guitarProfile;
                    const auto velocities = profile.has_value() && profile->velocities.has_value() ? *profile->velocities : c.velocities;
                    auto idea = cleanLead (approach % 2, spans, c.beatsPerBar, velocities);
                    if (profile.has_value())
                        keepInRange (idea, profile->rangeLow(), profile->rangeHigh());
                    return idea;
                }
                case ComposerLayer::melody: break;
            }
            return std::nullopt;
        }

        //======================================================================
        // (0.102.0) The pre-chorus types, from the owner's songs (read for their shapes only; our names):
        //   BUILD & STOP (Faster, Left Behind, Call Me When You're Sober, The End of the Dream): the verse's muted drive,
        //     a build from roots to dyads to chords, or strike-chug-ring; the keys on the key's fifth; everything but a
        //     snare roll stops for the last beat (half a bar after the build).
        //   GUITARS OUT, BASS CLIMBS (Crawling, Going Under, Waffle): the guitars out (or a high dissonant cell), the
        //     bass's long roots pushed onto the and of 3, octave pads / busy piano / held strings, a high motif.
        //   CYCLE IN QUARTERS (Circle With Me): the verse's still shape on, a three-note cycle in quarters over it (three
        //     against four, a climb every fourth bar, the next phrase a step lower), ghost-note drums; the last two bars
        //     a stop: one hit, then only a held note.
        //   CHUGS UNDER THE SHAPE (The Void): muted root chugs doubled by the bass and the kick, the verse's still shape
        //     an octave up over them.
        //   STABS & GAPS (Sweet Sacrifice): sixteenth stabs, a step down, silence, a two-note pickup; the bass and the
        //     toms on the guitar's hits.
        //   RISING BASS (At the End of August): tremolo-picked or held chords while the bass climbs a scale step a bar.
        struct PreChorus
        {
            const ComposerState& s;
            const std::vector<ChordSpan>& spans;
            const ComposerContext& c;
            juce::String type;
            double length, bpb, s16, e, q;
            int bars;
            Key key;

            PreChorus (const ComposerState& state, const std::vector<ChordSpan>& sp, const ComposerContext& context)
                : s (state), spans (sp), c (context), type (state.sectionType.trim().toUpperCase()),
                  length (sectionLength (sp)), bpb (context.beatsPerBar), s16 (context.beatsPerBar / 16.0),
                  e (context.beatsPerBar / 8.0), q (context.beatsPerBar / 4.0),
                  bars ((int) std::ceil (sectionLength (sp) / context.beatsPerBar - 1.0e-6)), key (sp.front().key) {}

            int approach (ComposerLayer l) const
            {
                const auto n = juce::jmax (1, preChorusApproaches (type, l).size());
                const auto& lane = s.lane (l);
                return lane.approach >= 0 ? lane.approach % n : (l == ComposerLayer::guitar ? lane.variation % n : 0);
            }
            const Chord& chordAt (double t) const { return chordAtIdea (spans, t); }
            GuitarTuning tuning() const { return c.guitarProfile.has_value() ? c.guitarProfile->tuning() : c.tuning; }
            GuitarVelocities velocities() const
            {
                return c.guitarProfile.has_value() && c.guitarProfile->velocities.has_value() ? *c.guitarProfile->velocities : c.velocities;
            }
            float muted() const { return GuitarVelocity::of (velocities().muteMid); }
            float open() const { return GuitarVelocity::of (velocities().open (velocities().sustain)); }
            int bassLowest() const { return c.bassProfile.has_value() ? c.bassProfile->rangeLow() : c.bassTuning.lowest(); }
            int bassHighest() const { return c.bassProfile.has_value() ? c.bassProfile->rangeHigh() : bassTop (c.bassTuning); }
            int lowRoot (double t) const { const auto low = tuning().lowest(); return rootNear (chordAt (t), low, low + 11); }
            int bassRoot (double t) const { return rootNear (chordAt (t), bassLowest(), juce::jmin (bassHighest(), bassLowest() + 11)); }

            static LineIdea idea (LineIdea::Part part, const juce::String& style, const juce::String& description)
            {
                return { part, "Pre-chorus: " + style, description, {} };
            }
            void power (LineIdea& out, double at, double len, int root, float velocity, double end) const
            {
                for (const auto p : { root, root + 7, root + 12 })
                    addNote (out, at, len, p, velocity, end);
            }
            void hit (LineIdea& out, double at, int note, float velocity) const
            {
                if (at >= -1.0e-9 && at < length - 1.0e-6)
                    out.notes.push_back ({ at, 0.1, note, velocity });
            }
            // Notes at or after `from` gone, the others cut there.
            static void cutAt (LineIdea& part, double from)
            {
                part.notes.erase (std::remove_if (part.notes.begin(), part.notes.end(),
                                                  [from] (const LineNote& n) { return ! n.keyswitch && n.beat >= from - 1.0e-6; }),
                                  part.notes.end());
                for (auto& n : part.notes)
                    if (! n.keyswitch && n.beat + n.length > from)
                        n.length = juce::jmax (0.01, from - n.beat);
            }
            // The stop's start (CYCLE IN QUARTERS: the last two bars of eight or more, the last bar of three or more).
            double cycleStop() const
            {
                const auto stopBars = bars >= 8 ? 2 : (bars >= 3 ? 1 : 0);
                return length - stopBars * bpb;
            }
            LineIdea pads (LineIdea::Part part, int low, double until) const
            {
                auto out = idea (part, "held pads", "Each chord held softly, moving to the nearest shape");
                const auto segments = cleanSegments (spans, bpb);
                std::vector<int> previous;
                for (size_t i = 0; i + 1 < segments.size(); ++i)
                {
                    const auto voicings = voicingsFor (chordAt (segments[i]), low);
                    if (voicings.empty())
                        continue;
                    const auto& shape = voicings[(size_t) juce::jlimit (0, (int) voicings.size() - 1, closestVoicing (voicings, previous, previous))];
                    previous = shape;
                    for (const auto p : shape)
                        addNote (out, segments[i], (segments[i + 1] - segments[i]) * 0.98, p, 0.42f, until);
                }
                return out;
            }
            LineIdea silent (LineIdea::Part part, const juce::String& what) const
            {
                return idea (part, what + " out", "Nothing here: room for the others");
            }

            //------------------------------------------------------------------
            LineIdea guitar() const
            {
                const auto a = approach (ComposerLayer::guitar);
                const auto r = LineIdea::Part::rhythm;
                if (type == "BUILD & STOP")
                {
                    if (a == 0)
                    {
                        auto out = idea (r, "muted eighths, then the stop", "Muted power chords on every eighth, silent on the last beat before the chorus");
                        const auto stop = length - q;
                        for (int i = 0; i * e < stop - 1.0e-6; ++i)
                            power (out, i * e, e * 0.8, lowRoot (i * e), muted(), stop);
                        return out;
                    }
                    if (a == 1)
                    {
                        auto out = idea (r, "roots, dyads, chords", "Muted roots in eighths, then two-note shapes, then power chords in sixteenths; half a bar of silence before the chorus");
                        const auto stop = length - bpb / 2.0;
                        for (int i = 0; i * e < stop - 1.0e-6; ++i)
                        {
                            const auto at = i * e;
                            const auto root = lowRoot (at);
                            if (at < length * 0.5)
                                addNote (out, at, e * 0.8, root, muted(), stop);
                            else if (at < length * 0.75)
                            {
                                addNote (out, at, e * 0.8, root, muted(), stop);
                                addNote (out, at, e * 0.8, root + 7, muted(), stop);
                            }
                            else
                                for (int h = 0; h < 2; ++h)
                                    power (out, at + h * s16, s16 * 0.85, lowRoot (at + h * s16), muted(), stop);
                        }
                        return out;
                    }
                    auto out = idea (r, "strike, chug, ring", "A chord struck, three muted chugs, then struck again and left ringing to the bar's end; silent on the last beat");
                    const auto stop = length - q;
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        power (out, o, 2 * s16 * 0.9, lowRoot (o), open(), stop);
                        for (int k = 3; k <= 5; ++k)
                            power (out, o + k * s16, s16 * 0.8, lowRoot (o + k * s16), muted(), stop);
                        power (out, o + 6 * s16, 9 * s16 * 0.97, lowRoot (o + 6 * s16), open(), stop);
                    }
                    return out;
                }
                if (type == "GUITARS OUT, BASS CLIMBS")
                {
                    if (a == 0)
                        return silent (r, "guitars");
                    auto out = idea (r, "high three-note cell", "A dissonant three-note cell (the key's seventh, sixth and flat seventh) in muted sixteenths, against the beat");
                    const auto t = pitchOfClassIn (key.pitchClass(), 64, 75);
                    const std::array<int, 3> cell { t - 1, t - 4, t - 2 };
                    for (int i = 0; i * s16 < length - 1.0e-6; ++i)
                        addNote (out, i * s16, s16 * 0.85, cell[(size_t) (i % 3)], GuitarVelocity::of (velocities().muteLoose), length);
                    return out;
                }
                if (type == "CYCLE IN QUARTERS")
                {
                    if (a == 3)
                        return silent (r, "guitars");
                    auto out = cleanGuitarStill (a == 0 ? 0 : a == 1 ? 2 : 1, spans, bpb, velocities());
                    out.style = "Pre-chorus: " + out.style.fromFirstOccurrenceOf ("Clean: ", false, false);
                    return out;
                }
                if (type == "CHUGS UNDER THE SHAPE")
                {
                    if (a == 1)
                    {
                        auto out = idea (r, "chugs on every eighth", "Muted power chords on every eighth on each chord's root");
                        for (int i = 0; i * e < length - 1.0e-6; ++i)
                            power (out, i * e, e * 0.8, lowRoot (i * e), muted(), length);
                        return out;
                    }
                    auto out = idea (r, "root chugs", "Muted chugs on the root: hit, hit, hit, a push, then on the beats");
                    struct Hit { int at, len; };
                    static const std::array<Hit, 8> hits { { { 0, 2 }, { 2, 2 }, { 4, 2 }, { 6, 1 }, { 7, 3 }, { 10, 2 }, { 12, 2 }, { 14, 2 } } };
                    for (int bar = 0; bar < bars; ++bar)
                        for (const auto& h : hits)
                        {
                            const auto at = bar * bpb + h.at * s16;
                            power (out, at, h.len * s16 * 0.85, lowRoot (at), muted(), length);
                        }
                    return out;
                }
                if (type == "STABS & GAPS")
                {
                    auto out = idea (r, a == 0 ? "stabs & gaps" : "stabs every bar",
                                     "Sixteenth stabs on the chord and a step under it, silence for the voice, a two-note pickup");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = lowRoot (o);
                        const auto under = scaleStep (key, root, -1) != root ? scaleStep (key, root, -1) : root - 2;
                        std::vector<std::pair<int, int>> hits { { 0, root }, { 1, root }, { 2, under }, { 3, under }, { 14, under }, { 15, under } };
                        if (a == 0 && bar % 2 == 1)
                            for (int k = 6; k <= 11; ++k)
                                hits.push_back ({ k, root });
                        for (const auto& [at, p] : hits)
                            power (out, o + at * s16, s16 * 0.85, p, open(), length);
                    }
                    return out;
                }
                if (type == "RISING BASS")
                {
                    if (a == 1)
                    {
                        auto out = idea (r, "held chords", "A power chord held on each chord while the bass climbs");
                        const auto segments = cleanSegments (spans, bpb);
                        for (size_t i = 0; i + 1 < segments.size(); ++i)
                            power (out, segments[i], (segments[i + 1] - segments[i]) * 0.97, lowRoot (segments[i]), open(), length);
                        return out;
                    }
                    auto out = idea (r, "tremolo chords", "Power chords tremolo picked in sixteenths while the bass climbs");
                    const auto soft = GuitarVelocity::of (velocities().open (velocities().sustainSoft));
                    for (int i = 0; i * s16 < length - 1.0e-6; ++i)
                        power (out, i * s16, s16 * 0.85, lowRoot (i * s16), soft, length);
                    return out;
                }
                return {};
            }

            LineIdea bass() const
            {
                const auto a = approach (ComposerLayer::bass);
                const auto b = LineIdea::Part::bass;
                auto eighths = [&] (double until)
                {
                    auto out = idea (b, "root eighths", "The chord's root on every eighth");
                    for (int i = 0; i * e < until - 1.0e-6; ++i)
                        addNote (out, i * e, e * 0.88, bassRoot (i * e), i % 2 == 0 ? 0.82f : 0.7f, until);
                    return out;
                };
                auto doubled = [&] { return doubledLine (guitar(), spans, bassLowest(), bassHighest(), "guitar"); };
                if (type == "BUILD & STOP")
                {
                    if (a == 0)
                        return eighths (length - q);
                    if (a == 1)
                        return doubled();
                    auto out = idea (b, "pulses", "The root in pushed pulses: a hit, two quick ones, then on the and of 2, 3 and the and of 4");
                    for (int bar = 0; bar < bars; ++bar)
                        for (const auto k : { 0, 2, 3, 6, 10, 14 })
                        {
                            const auto at = bar * bpb + k * s16;
                            addNote (out, at, (k == 10 ? 2 : 1) * s16 * 0.9, bassRoot (at), k == 0 ? 0.85f : 0.72f, length - q);
                        }
                    return out;
                }
                if (type == "GUITARS OUT, BASS CLIMBS")
                {
                    if (a == 1)
                        return silent (b, "bass");
                    auto out = idea (b, "pushed roots", "Each bar's root held long, the next bar's root pushed in on the and of 3");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto next = juce::jmin (length - 1.0e-6, o + bpb);
                        addNote (out, o, 10 * s16 * 0.97, bassRoot (o), 0.84f, length);
                        addNote (out, o + 10 * s16, 6 * s16 * 0.97, bassRoot (next), 0.8f, length);
                    }
                    return out;
                }
                if (type == "CYCLE IN QUARTERS")
                {
                    LineIdea out;
                    if (a == 1)
                    {
                        out = idea (b, "whole notes", "Each chord's root held under it");
                        const auto segments = cleanSegments (spans, bpb);
                        for (size_t i = 0; i + 1 < segments.size(); ++i)
                            addNote (out, segments[i], (segments[i + 1] - segments[i]) * 0.97, bassRoot (segments[i]), 0.82f, length);
                    }
                    else
                        out = eighths (length);
                    return out;
                }
                if (type == "CHUGS UNDER THE SHAPE" || type == "STABS & GAPS")
                    return a == 0 ? doubled() : eighths (length);
                if (type == "RISING BASS")
                {
                    auto out = idea (b, a == 0 ? "climbs in sixteenths" : "climbs in eighths",
                                     "The key's note climbing a scale step a bar whatever the chords do, in " + juce::String (a == 0 ? "sixteenths" : "eighths"));
                    const auto start = pitchOfClassIn (key.pitchClass(), bassLowest(), bassLowest() + 11);
                    const auto step = a == 0 ? s16 : e;
                    for (int i = 0; i * step < length - 1.0e-6; ++i)
                    {
                        const auto bar = (int) std::floor (i * step / bpb + 1.0e-6);
                        auto p = scaleStep (key, start, bar);
                        while (p > bassHighest())
                            p -= 12;
                        addNote (out, i * step, step * 0.88, p, (i % 4 == 0) ? 0.82f : 0.7f, length);
                    }
                    return out;
                }
                return {};
            }

            LineIdea keys() const
            {
                const auto a = approach (ComposerLayer::chords);
                const auto k = LineIdea::Part::chords;
                if (type == "BUILD & STOP")
                {
                    if (a == 2)
                        return silent (k, "keys");
                    if (a == 1)
                        return pads (k, 55, length - q);
                    auto out = idea (k, "dominant pedal", "The key's fifth held in octaves over every chord: the pull toward the chorus");
                    const auto fifth = pitchOfClassIn ((key.pitchClass() + 7) % 12, 55, 66);
                    for (auto t = 0.0; t < length - q - 1.0e-6; t += 2 * bpb)
                        for (const auto p : { fifth, fifth + 12 })
                            addNote (out, t, juce::jmin (2 * bpb, length - q - t) * 0.98, p, 0.45f, length - q);
                    return out;
                }
                if (type == "GUITARS OUT, BASS CLIMBS")
                {
                    if (a == 2)
                    {
                        auto out = pads (k, 50, length);
                        out.style = "Pre-chorus: held strings";
                        out.description = "A low chord held a bar at a time, as strings";
                        return out;
                    }
                    if (a == 1)
                    {
                        auto out = idea (k, "piano sixteenths", "A busy piano figure in sixteenths through the chord's notes, from the and of 1 to the bar's end");
                        for (int bar = 0; bar < bars; ++bar)
                        {
                            const auto o = bar * bpb;
                            const auto pcs = chordAt (o).pitchClasses();
                            std::vector<int> tones;
                            for (auto p = 60; p <= 76; ++p)
                                if (std::find (pcs.begin(), pcs.end(), pc12 (p)) != pcs.end())
                                    tones.push_back (p);
                            if (tones.size() < 2)
                                continue;
                            const auto n = (int) tones.size();
                            for (int x = 2; x < 16; ++x)
                            {
                                const auto step = x - 2, period = 2 * (n - 1);
                                const auto i = step % period < n ? step % period : period - step % period;
                                addNote (out, o + x * s16, s16 * 0.9, tones[(size_t) juce::jlimit (0, n - 1, i)], x % 4 == 0 ? 0.6f : 0.5f, length);
                            }
                        }
                        return out;
                    }
                    auto out = idea (k, "octave pad", "Each chord's root held in octaves a bar at a time");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = pitchOfClassIn (chordAt (o).root, 60, 71);
                        for (const auto p : { root, root + 12 })
                            addNote (out, o, bpb * 0.98, p, 0.42f, length);
                    }
                    return out;
                }
                // The other types: the first silent, the second held pads (CYCLE IN QUARTERS: up to its stop).
                if (a == 0)
                    return silent (k, "keys");
                return pads (k, 55, type == "CYCLE IN QUARTERS" ? cycleStop() : length);
            }

            std::optional<LineIdea> lead() const
            {
                if (preChorusApproaches (type, ComposerLayer::lead).isEmpty())
                    return std::nullopt;
                const auto a = approach (ComposerLayer::lead);
                const auto l = LineIdea::Part::lead;
                const auto vel = open();
                if (type == "BUILD & STOP")
                {
                    if (a == 0)
                        return silent (l, "lead");
                    auto out = idea (l, "rising line", "A held note a bar, climbing the key's scale from its fifth into the chorus");
                    const auto fifth = pitchOfClassIn ((key.pitchClass() + 7) % 12, 67, 78);
                    for (int bar = 0; bar < bars; ++bar)
                        addNote (out, bar * bpb, bpb * 0.96, scaleStep (key, fifth, bar), vel, length - q);
                    return out;
                }
                if (type == "GUITARS OUT, BASS CLIMBS")
                {
                    if (a == 1)
                        return silent (l, "lead");
                    auto out = idea (l, "high motif", "Each bar's root on the beat, then the same high figure: the key's fifth, fourth, fifth, fifth");
                    const auto fifth = pitchOfClassIn ((key.pitchClass() + 7) % 12, 72, 83);
                    const auto fourth = scaleStep (key, fifth, -1);
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        addNote (out, o, 2 * s16 * 0.95, pitchOfClassIn (chordAt (o).root, 72, 83), vel, length);
                        addNote (out, o + 2 * s16, 4 * s16 * 0.95, fifth, vel, length);
                        addNote (out, o + 6 * s16, 4 * s16 * 0.95, fourth, vel, length);
                        addNote (out, o + 10 * s16, 4 * s16 * 0.95, fifth, vel, length);
                        addNote (out, o + 14 * s16, 2 * s16 * 0.95, fifth, vel, length);
                    }
                    return out;
                }
                if (type == "CYCLE IN QUARTERS")
                {
                    if (a == 1)
                        return silent (l, "lead");
                    auto out = idea (l, "quarter cycle", "The key's third, fifth and sixth cycled in quarters (three against four), a climb every fourth bar, the next phrase a step lower");
                    const auto third = pitchOfClassIn (pc12 (scaleStep (key, pitchOfClassIn (key.pitchClass(), 60, 71), 2)), 67, 78);
                    auto k = 0;
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto shift = (bar / 4) % 2 == 1 ? -1 : 0;
                        const auto base = scaleStep (key, third, shift);
                        for (int beat = 0; beat < 4; ++beat)
                        {
                            const auto at = bar * bpb + beat * q;
                            int p;
                            if (bar % 4 == 3)
                                p = scaleStep (key, base, std::array<int, 4> { 0, -2, 0, 1 }[(size_t) beat]);
                            else
                                p = scaleStep (key, base, std::array<int, 3> { 0, 2, 3 }[(size_t) (k++ % 3)]);
                            addNote (out, at, q * 0.95, p, vel, length);
                        }
                    }
                    return out;
                }
                if (type == "CHUGS UNDER THE SHAPE")
                {
                    if (a == 2)
                        return silent (l, "lead");
                    auto out = cleanGuitarStill (a == 0 ? 2 : 0, spans, bpb, velocities());
                    out.part = l;
                    out.style = "Pre-chorus: " + juce::String (a == 0 ? "still cluster, an octave up" : "still shape, an octave up");
                    out.description = "The verse's still shape played an octave up over the chugs, never moving with the chords";
                    for (auto& n : out.notes)
                    {
                        n.pitch += 12;
                        n.velocity = vel;
                    }
                    return out;
                }
                return std::nullopt;
            }

            LineIdea sortedGuitar() const
            {
                auto g = guitar();
                sortNotes (g);
                return g;
            }

            LineIdea drums() const
            {
                const auto a = approach (ComposerLayer::drums);
                auto out = idea (LineIdea::Part::drums, "drums", {});
                auto named = [&] (const juce::String& style, const juce::String& description)
                {
                    out.style = "Pre-chorus: " + style;
                    out.description = description;
                };
                auto fillInto = [&] (bool stop)
                {
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto last = bar == bars - 1;
                        if (bar == 0)
                            hit (out, o, 49, 0.78f);
                        hit (out, o, 36, 0.9f);
                        hit (out, o + 4 * s16, 38, 0.88f);
                        if (! last)
                        {
                            for (int i = 0; i < 8; ++i)
                                hit (out, o + i * e, 42, i % 2 == 0 ? 0.6f : 0.46f);
                            hit (out, o + 8 * s16, 36, 0.86f);
                            hit (out, o + 12 * s16, 38, 0.88f);
                            continue;
                        }
                        for (int x = 6; x < (stop ? 12 : 16); ++x)
                            hit (out, o + x * s16, x < 9 ? 38 : (x < 10 ? 45 : x < 11 ? 43 : 41), 0.7f + 0.025f * (float) (x - 6));
                    }
                };
                if (type == "BUILD & STOP" || (type == "RISING BASS" && a == 1))
                {
                    if (a == 0 || type == "RISING BASS")
                    {
                        named ("beat into a fill", "Kick and snare with hats in eighths, a snare and tom fill in the last bar, then silent on its last beat");
                        fillInto (true);
                    }
                    else if (a == 1)
                    {
                        named ("ride and a roll", "Ride in eighths, the bell on the beats, then a snare roll from the and of 3 into the chorus");
                        for (int bar = 0; bar < bars; ++bar)
                        {
                            const auto o = bar * bpb;
                            const auto last = bar == bars - 1;
                            for (int i = 0; i < 8; ++i)
                                hit (out, o + i * e, i % 2 == 0 ? 53 : 51, i % 2 == 0 ? 0.62f : 0.5f);
                            hit (out, o, 36, 0.9f);
                            hit (out, o + 8 * s16, 36, 0.86f);
                            hit (out, o + 4 * s16, 38, 0.88f);
                            if (! last)
                                hit (out, o + 12 * s16, 38, 0.88f);
                            else
                                for (int x = 10; x < 16; ++x)
                                    hit (out, o + x * s16, 38, 0.62f + 0.05f * (float) (x - 10));
                        }
                    }
                    else
                    {
                        named ("building snare", "Kick on every beat, the snare in eighths for the last two bars, then sixteenths, silent on the last beat");
                        for (int bar = 0; bar < bars; ++bar)
                        {
                            const auto o = bar * bpb;
                            if (bar == 0)
                                hit (out, o, 49, 0.78f);
                            for (int b = 0; b < 4; ++b)
                                if (! (bar == bars - 1 && b == 3))
                                    hit (out, o + b * q, 36, 0.86f);
                            if (bar < bars - 2)
                            {
                                for (int i = 0; i < 8; ++i)
                                    hit (out, o + i * e, 42, 0.5f);
                                hit (out, o + 4 * s16, 38, 0.86f);
                                hit (out, o + 12 * s16, 38, 0.86f);
                            }
                            else if (bar == bars - 2)
                                for (int i = 0; i < 8; ++i)
                                    hit (out, o + i * e, 38, 0.6f + 0.03f * (float) i);
                            else
                                for (int x = 0; x < 12; ++x)
                                    hit (out, o + x * s16, 38, 0.65f + 0.025f * (float) x);
                        }
                    }
                    return out;
                }
                if (type == "GUITARS OUT, BASS CLIMBS")
                {
                    if (a == 0)
                        named ("broken beat", "Sixteenth hats, the kick on 1 and 3 (and after 1 every other bar), the snare on 2 and the and of 3");
                    else if (a == 1)
                        named ("ghost groove", "Kick and backbeat with ghosted snares between, hats in eighths");
                    else
                        named ("half time", "Kick on 1 and the and of 3, the snare on 3, hats in eighths");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        if (bar == 0)
                            hit (out, o, 49, 0.72f);
                        if (a == 0)
                        {
                            for (int x = 0; x < 16; ++x)
                                hit (out, o + x * s16, 42, x % 4 == 0 ? 0.6f : 0.42f);
                            hit (out, o, 36, 0.88f);
                            if (bar % 2 == 1)
                                hit (out, o + 2 * s16, 36, 0.76f);
                            hit (out, o + 8 * s16, 36, 0.84f);
                            hit (out, o + 4 * s16, 38, 0.86f);
                            hit (out, o + 10 * s16, 38, 0.84f);
                        }
                        else
                        {
                            for (int i = 0; i < 8; ++i)
                                hit (out, o + i * e, 42, i % 2 == 0 ? 0.58f : 0.44f);
                            if (a == 1)
                            {
                                for (const auto k : { 0, 8, 11 })
                                    hit (out, o + k * s16, 36, k == 0 ? 0.88f : 0.76f);
                                hit (out, o + 4 * s16, 38, 0.86f);
                                hit (out, o + 12 * s16, 38, 0.86f);
                                for (const auto g : { 7, 9, 15 })
                                    hit (out, o + g * s16, 38, 0.3f);
                            }
                            else
                            {
                                hit (out, o, 36, 0.88f);
                                hit (out, o + 10 * s16, 36, 0.78f);
                                hit (out, o + 8 * s16, 38, 0.88f);
                            }
                        }
                    }
                    return out;
                }
                if (type == "CYCLE IN QUARTERS")
                {
                    if (a == 1 || a == 2)
                    {
                        auto clean = cleanDrums (a == 1 ? 4 : 5, spans, bpb);
                        clean.style = "Pre-chorus: " + clean.style.fromFirstOccurrenceOf ("Clean: ", false, false);
                        return clean;
                    }
                    named ("ghost groove", "Kick on 1, ghosted snares, a hard snare with the cymbal on 3, ghosts again at the bar's end");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        if (bar == 0)
                            hit (out, o, 49, 0.72f);
                        for (int i = 0; i < 8; ++i)
                            hit (out, o + i * e, 42, 0.4f);
                        hit (out, o, 36, 0.88f);
                        hit (out, o + 6 * s16, 36, 0.76f);
                        hit (out, o + 8 * s16, 38, 0.9f);
                        hit (out, o + 8 * s16, 46, 0.6f);
                        for (const auto g : { 2, 5, 11, 13, 14, 15 })
                            hit (out, o + g * s16, 38, 0.28f);
                    }
                    return out;
                }
                if (type == "CHUGS UNDER THE SHAPE")
                {
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        if (bar % 4 == 0)
                            hit (out, o, 49, 0.76f);
                        for (int i = 0; i < 8; ++i)
                            hit (out, o + i * e, 42, i % 2 == 0 ? 0.58f : 0.44f);
                        if (a == 1)
                        {
                            hit (out, o, 36, 0.88f);
                            hit (out, o + 10 * s16, 36, 0.78f);
                            hit (out, o + 8 * s16, 38, 0.88f);
                        }
                        else
                        {
                            hit (out, o + 4 * s16, 38, 0.88f);
                            hit (out, o + 12 * s16, 38, 0.88f);
                        }
                    }
                    if (a == 0)
                    {
                        named ("kick with the chugs", "The kick on every chug, the snare on 2 and 4, hats in eighths");
                        for (const auto t : riffHits (sortedGuitar()))
                            hit (out, t, 36, 0.86f);
                    }
                    else
                        named ("half time", "Kick on 1 and the and of 3, the snare on 3, hats in eighths");
                    return out;
                }
                if (type == "STABS & GAPS")
                {
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        if (bar == 0)
                            hit (out, o, 49, 0.78f);
                        hit (out, o + 4 * s16, 38, 0.88f);
                        hit (out, o + 12 * s16, 38, 0.88f);
                        if (a == 0)
                            for (const auto k : { 6, 9, 10 })
                                hit (out, o + k * s16, 36, 0.8f);
                        else
                            for (int i = 0; i < 8; ++i)
                                hit (out, o + i * e, 42, 0.5f);
                    }
                    const auto g = sortedGuitar();
                    auto lowest = 128;
                    for (const auto& n : g.notes)
                        lowest = juce::jmin (lowest, n.pitch);
                    for (const auto t : riffHits (g))
                    {
                        if (a == 1)
                        {
                            hit (out, t, 36, 0.86f);
                            continue;
                        }
                        // the toms on the hits: the high tom on the chord, the low on the step under it
                        auto low = 128;
                        for (const auto& n : g.notes)
                            if (std::abs (n.beat - t) < HumanizeLimits::together)
                                low = juce::jmin (low, n.pitch);
                        hit (out, t, pc12 (low) == pc12 (lowRoot (t)) ? 45 : 43, 0.82f);
                    }
                    if (a == 0)
                        named ("toms with the hits", "The toms on every stab with the guitar and the bass, the kick between, the snare on 2 and 4");
                    else
                        named ("kick with the hits", "The kick on every stab, the snare on 2 and 4, hats in eighths");
                    return out;
                }
                if (type == "RISING BASS")
                {
                    named ("building kick", "The kick in eighths, then sixteenths in the second half, the ride on the beats, crashes on every beat of the last bar");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto last = bar == bars - 1;
                        if (bar == 0)
                            hit (out, o, 49, 0.78f);
                        for (int b = 0; b < 4; ++b)
                            hit (out, o + b * q, last ? 49 : 51, last ? 0.7f : 0.55f);
                        const auto step = o >= length * 0.5 - 1.0e-6 ? s16 : e;
                        for (auto t = o; t < o + bpb - 1.0e-6; t += step)
                            hit (out, t, 36, 0.78f);
                        hit (out, o + 4 * s16, 38, 0.88f);
                        hit (out, o + 12 * s16, 38, 0.88f);
                    }
                    return out;
                }
                return out;
            }

            // CYCLE IN QUARTERS' stop: everything cut there, one hit with the whole band, the lead's one note held.
            void stopped (ComposerLayer l, LineIdea& part) const
            {
                if (type != "CYCLE IN QUARTERS")
                    return;
                const auto stop = cycleStop();
                if (stop >= length - 1.0e-6)
                    return;
                cutAt (part, stop);
                const auto rest = length - stop;
                switch (l)
                {
                    case ComposerLayer::guitar:
                        if (approach (l) != 3)
                            power (part, stop, q * 0.9, lowRoot (stop), open(), length);
                        break;
                    case ComposerLayer::bass: addNote (part, stop, q * 0.9, bassRoot (stop), 0.86f, length); break;
                    case ComposerLayer::drums:
                        hit (part, stop, 36, 0.92f);
                        hit (part, stop, 49, 0.86f);
                        break;
                    case ComposerLayer::lead:
                        if (approach (l) == 0)
                            addNote (part, stop, rest * 0.97, pitchOfClassIn ((key.pitchClass() + 7) % 12, 67, 78), open(), length);
                        break;
                    case ComposerLayer::chords:
                    case ComposerLayer::melody: break;
                }
                sortNotes (part);
            }
        };

        std::optional<LineIdea> preChorusLayer (ComposerLayer l, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
        {
            if (spans.empty() || sectionBand (s) != SectionType::Band::preChorus || l == ComposerLayer::melody)
                return std::nullopt;
            const PreChorus p (s, spans, c);
            if (preChorusApproaches (p.type, l).isEmpty())
                return std::nullopt;
            std::optional<LineIdea> idea;
            switch (l)
            {
                case ComposerLayer::guitar:
                {
                    idea = p.guitar();
                    if (c.guitarProfile.has_value())
                        keepInRange (*idea, c.guitarProfile->rangeLow(), c.guitarProfile->rangeHigh());
                    break;
                }
                case ComposerLayer::bass:
                    idea = p.bass();
                    keepInRange (*idea, p.bassLowest(), p.bassHighest());
                    break;
                case ComposerLayer::chords: idea = p.keys(); break;
                case ComposerLayer::lead:
                {
                    idea = p.lead();
                    const auto& profile = c.leadProfile.has_value() ? c.leadProfile : c.guitarProfile;
                    if (idea.has_value() && profile.has_value())
                        keepInRange (*idea, profile->rangeLow(), profile->rangeHigh());
                    break;
                }
                case ComposerLayer::drums: idea = p.drums(); break;
                case ComposerLayer::melody: break;
            }
            if (idea.has_value())
            {
                p.stopped (l, *idea);
                sortNotes (*idea);
            }
            return idea;
        }

        // (0.105.0) The bridge types (the owner's songs read for their shapes only; our names and rules):
        //   HALF-TIME MELODIC (Another Life): a ringing chord with a short melody on it, power-chord stabs in 3+3+2, or
        //     colour chords rung; the bass held, root octaves on the keys; a half-time ghost-note groove into a triplet
        //     fill over the last two bars. The lead plays its own (it carries the hook).
        //   HEAVY BUILD (Ten Ton Hammer, Clown, Faint): scratches then straight eighths; power chords on dotted quarters
        //     climbing root, b2, 3 then b7 and root; a syncopated siren chug going half time halfway; falling eighths
        //     (octave, 5th, b3, b3, 2, 2) into a harmonic. The kick with the guitar, a lead bending whole steps on 2 and 4.
        //   LATE-CHANGE CLIMB (Sweet Sacrifice): power chords in sixteenths (or eighths), each chord change a step late,
        //     the kick on every sixteenth, a sus4 pad on top.
        //   CLEAN BRIDGE (At the End of August, All Night Lights, Waterhaul): a fixed pair with the top note stepping down
        //     in 3+3+2, held octaves with a fifths pulse, or a pedal in 3+3+2 with a melody between; the bass and a snare
        //     march coming in halfway.
        //   SLIDING CHORDS (This Calling): a four-bar figure: a chord, four eighths, a chord; eighths and a lick; a line in
        //     quarters; a chord sliding down a half step. Or only the held chord and its slide.
        //   GUITARS OUT, THEN BACK (Left Behind): the guitars out until halfway (or the last quarter), then back, the last
        //     bar an octave up into the chorus; the drums half time (or toms) until then.
        //   SLIDE OVER A HELD ROOT (Another Life): a palm-muted root rung on 1 with a sliding triplet figure on top, in turn
        //     with syncopated stab bars.
        struct Bridge
        {
            const ComposerState& s;
            const std::vector<ChordSpan>& spans;
            const ComposerContext& c;
            juce::String type;
            double length, bpb, s16, e, q;
            int bars;
            Key key;

            Bridge (const ComposerState& state, const std::vector<ChordSpan>& sp, const ComposerContext& context)
                : s (state), spans (sp), c (context), type (state.sectionType.trim().toUpperCase()),
                  length (sectionLength (sp)), bpb (context.beatsPerBar), s16 (context.beatsPerBar / 16.0),
                  e (context.beatsPerBar / 8.0), q (context.beatsPerBar / 4.0),
                  bars ((int) std::ceil (sectionLength (sp) / context.beatsPerBar - 1.0e-6)), key (sp.front().key) {}

            int approach (ComposerLayer l) const
            {
                const auto n = juce::jmax (1, bridgeApproaches (type, l).size());
                const auto& lane = s.lane (l);
                return lane.approach >= 0 ? lane.approach % n : (l == ComposerLayer::guitar ? lane.variation % n : 0);
            }
            const Chord& chordAt (double t) const { return chordAtIdea (spans, juce::jlimit (0.0, juce::jmax (0.0, length - 1.0e-6), t)); }
            GuitarTuning tuning() const { return c.guitarProfile.has_value() ? c.guitarProfile->tuning() : c.tuning; }
            GuitarVelocities velocities() const
            {
                return c.guitarProfile.has_value() && c.guitarProfile->velocities.has_value() ? *c.guitarProfile->velocities : c.velocities;
            }
            float muted() const { return GuitarVelocity::of (velocities().muteMid); }
            float tight() const { return GuitarVelocity::of (velocities().muteTight); }
            float loose() const { return GuitarVelocity::of (velocities().muteLoose); }
            float open() const { return GuitarVelocity::of (velocities().open (velocities().sustain)); }
            float soft() const { return GuitarVelocity::of (velocities().open (velocities().sustainSoft)); }
            int bassLowest() const { return c.bassProfile.has_value() ? c.bassProfile->rangeLow() : c.bassTuning.lowest(); }
            int bassHighest() const { return c.bassProfile.has_value() ? c.bassProfile->rangeHigh() : bassTop (c.bassTuning); }
            int lowRoot (double t) const { const auto low = tuning().lowest(); return rootNear (chordAt (t), low, low + 11); }
            int bassRoot (double t) const { return rootNear (chordAt (t), bassLowest(), juce::jmin (bassHighest(), bassLowest() + 11)); }
            int tonicIn (int low, int high) const { return pitchOfClassIn (key.pitchClass(), low, high); }
            // Where GUITARS OUT, THEN BACK's guitar comes back (a bar line; at least a bar before the end).
            double backAt() const
            {
                const auto part = approach (ComposerLayer::guitar) == 0 ? 0.5 : 0.75;
                const auto bar = juce::jlimit (0, juce::jmax (0, bars - 1), (int) std::floor (bars * part));
                return bar * bpb;
            }

            static LineIdea idea (LineIdea::Part part, const juce::String& style, const juce::String& description)
            {
                return { part, "Bridge: " + style, description, {} };
            }
            void power (LineIdea& out, double at, double len, int root, float velocity, bool octave = true) const
            {
                addNote (out, at, len, root, velocity, length);
                addNote (out, at, len, root + 7, velocity, length);
                if (octave)
                    addNote (out, at, len, root + 12, velocity, length);
            }
            void hit (LineIdea& out, double at, int note, float velocity) const
            {
                if (at >= -1.0e-9 && at < length - 1.0e-6)
                    out.notes.push_back ({ at, 0.1, note, velocity });
            }
            LineIdea pads (LineIdea::Part part, int low) const
            {
                auto out = idea (part, "held pads", "Each chord held softly, moving to the nearest shape");
                const auto segments = cleanSegments (spans, bpb);
                std::vector<int> previous;
                for (size_t i = 0; i + 1 < segments.size(); ++i)
                {
                    const auto voicings = voicingsFor (chordAt (segments[i]), low);
                    if (voicings.empty())
                        continue;
                    const auto& shape = voicings[(size_t) juce::jlimit (0, (int) voicings.size() - 1, closestVoicing (voicings, previous, previous))];
                    previous = shape;
                    for (const auto p : shape)
                        addNote (out, segments[i], (segments[i + 1] - segments[i]) * 0.98, p, 0.42f, length);
                }
                return out;
            }
            LineIdea silent (LineIdea::Part part, const juce::String& what) const
            {
                return idea (part, what + " out", "Nothing here: room for the others");
            }
            LineIdea sortedGuitar() const
            {
                auto g = guitar();
                sortNotes (g);
                return g;
            }

            //------------------------------------------------------------------
            LineIdea guitar() const
            {
                const auto a = approach (ComposerLayer::guitar);
                const auto r = LineIdea::Part::rhythm;
                if (type == "HALF-TIME MELODIC")
                {
                    if (a == 1)
                    {
                        auto out = idea (r, "stabs in 3+3+2", "Power chords struck on 1, the and of 2 and 4, each ringing to the next");
                        for (int bar = 0; bar < bars; ++bar)
                            for (const auto [k, len] : { std::pair { 0, 3 }, std::pair { 3, 3 }, std::pair { 6, 2 } })
                                power (out, bar * bpb + k * e, len * e * 0.92, lowRoot (bar * bpb + k * e), open());
                        return out;
                    }
                    if (a == 2)
                    {
                        auto out = idea (r, "ringing colour chords", "Each bar's chord rung with its ninth added, let ring");
                        for (int bar = 0; bar < bars; ++bar)
                        {
                            const auto o = bar * bpb;
                            const auto root = lowRoot (o) + 12;
                            const auto pcs = chordAt (o).pitchClasses();
                            const auto third = pcs.size() > 1 ? pitchOfClassIn (pcs[1], root + 1, root + 12) : root + 4;
                            for (const auto p : { root, root + 7, third + 12, root + 14 })
                                addNote (out, o, bpb * 0.97, p, open(), length);
                        }
                        return out;
                    }
                    auto out = idea (r, "ringing chord & melody", "The bar's chord rung, then a short melody on it: its third, a step down");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto voicings = voicingsFor (chordAt (o), 52);
                        if (! voicings.empty())
                            for (const auto p : voicings[voicings.size() / 2])
                                addNote (out, o, 2 * q * 0.97, p, open(), length);
                        const auto pcs = chordAt (o).pitchClasses();
                        const auto third = pitchOfClassIn (pcs.size() > 1 ? pcs[1] : chordAt (o).root, 64, 75);
                        addNote (out, o + 2 * q, q * 0.95, third, open(), length);
                        addNote (out, o + 3 * q, q * 0.95, scaleStep (key, third, -1), open(), length);
                    }
                    return out;
                }
                if (type == "HEAVY BUILD")
                {
                    if (a == 0)
                    {
                        auto out = idea (r, "scratch build", "Sparse dead-string scratches, then muted power chords on every eighth from halfway");
                        const auto half = std::floor (bars / 2.0) * bpb;
                        for (int bar = 0; bar < bars; ++bar)
                        {
                            const auto o = bar * bpb;
                            if (o < half - 1.0e-6)
                                for (const auto k : { 0, 6, 10, 13 })
                                    addNote (out, o + k * s16, s16 * 0.8, lowRoot (o + k * s16), tight(), length);
                            else
                                for (int i = 0; i < 8; ++i)
                                    power (out, o + i * e, e * 0.8, lowRoot (o + i * e), muted(), false);
                        }
                        return out;
                    }
                    if (a == 1)
                    {
                        auto out = idea (r, "dotted climb", "Power chords on dotted quarters climbing root, flat second, third; then the seventh, the root and scratches");
                        for (int bar = 0; bar < bars; ++bar)
                        {
                            const auto o = bar * bpb;
                            const auto root = lowRoot (o);
                            if (bar % 2 == 0)
                            {
                                power (out, o, 3 * e * 0.92, root, open(), false);
                                power (out, o + 3 * e, 3 * e * 0.92, root + 1, open(), false);
                                power (out, o + 6 * e, 2 * e * 0.92, root + 4, open(), false);
                            }
                            else
                            {
                                power (out, o, 3 * e * 0.92, root - 2, open(), false);   // (the flat seventh, under the root)
                                power (out, o + 3 * e, 3 * e * 0.92, root, open(), false);
                                addNote (out, o + 6 * e, s16 * 0.8, root, tight(), length);
                                addNote (out, o + 7 * e, s16 * 0.8, root, tight(), length);
                            }
                        }
                        return out;
                    }
                    if (a == 2)
                    {
                        auto out = idea (r, "siren chug", "A syncopated muted chug on the root, going half time halfway");
                        const auto half = std::floor (bars / 2.0) * bpb;
                        for (int bar = 0; bar < bars; ++bar)
                        {
                            const auto o = bar * bpb;
                            const auto root = lowRoot (o);
                            if (o < half - 1.0e-6 || bars < 2)
                            {
                                power (out, o, s16 * 0.9, root, open(), false);
                                for (const auto k : { 1, 3, 6, 7, 10, 12, 13 })
                                    addNote (out, o + k * s16, s16 * 0.8, root, muted(), length);
                            }
                            else
                            {
                                power (out, o, 3 * s16 * 0.9, root, open(), false);
                                for (const auto k : { 3, 10 })
                                    addNote (out, o + k * s16, s16 * 0.8, root, muted(), length);
                            }
                        }
                        return out;
                    }
                    auto out = idea (r, "falling eighths & harmonic", "Eighths falling from the octave: 5th, minor third twice, second twice; then a harmonic rung");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = lowRoot (o);
                        const int steps[] { 12, 7, 3, 3, 2, 2 };
                        for (int i = 0; i < 6; ++i)
                            addNote (out, o + i * e, e * 0.9, root + steps[i], open(), length);
                        LineNote harmonic { o + 6 * e, 2 * e * 0.95, root + 24, open() };
                        harmonic.technique = LineNote::Technique::harmonic;
                        if (harmonic.beat < length - 1.0e-6)
                            out.notes.push_back (harmonic);
                    }
                    return out;
                }
                if (type == "LATE-CHANGE CLIMB")
                {
                    const auto step = a == 0 ? s16 : e;
                    auto out = idea (r, a == 0 ? "sixteenths, changing late" : "eighths, changing late",
                                     "Power chords on every " + juce::String (a == 0 ? "sixteenth" : "eighth") + ", each new chord coming in a step late");
                    for (int i = 0; i * step < length - 1.0e-6; ++i)
                    {
                        const auto at = i * step;
                        const auto onBeat = std::abs (std::fmod (at, q)) < 1.0e-6;
                        power (out, at, step * 0.85, lowRoot (juce::jmax (0.0, at - step)), onBeat ? open() : muted(), false);
                    }
                    return out;
                }
                if (type == "CLEAN BRIDGE")
                {
                    if (a == 1)
                    {
                        auto out = idea (r, "held octaves", "The key's tonic in octaves, rung for two bars at a time");
                        const auto tonic = tonicIn (52, 63);
                        for (int bar = 0; bar < bars; bar += 2)
                            for (const auto p : { tonic, tonic + 12 })
                                addNote (out, bar * bpb, juce::jmin (2.0, (double) (bars - bar)) * bpb * 0.97, p, soft(), length);
                        return out;
                    }
                    if (a == 2)
                    {
                        auto out = idea (r, "pedal in 3+3+2 & melody", "The tonic struck in 3+3+2, a melody of chord tones between the strikes");
                        const auto pedal = tonicIn (52, 63);
                        for (int bar = 0; bar < bars; ++bar)
                        {
                            const auto o = bar * bpb;
                            for (const auto k : { 0, 3, 6 })
                                addNote (out, o + k * e, e * 0.9, pedal, soft(), length);
                            const auto pcs = chordAt (o).pitchClasses();
                            addNote (out, o + e, 2 * e * 0.9, pitchOfClassIn (pcs.size() > 1 ? pcs[1] : pcs[0], 64, 75), soft(), length);
                            addNote (out, o + 4 * e, 2 * e * 0.9, pitchOfClassIn (pcs.size() > 2 ? pcs[2] : pcs[0], 64, 75), soft(), length);
                        }
                        return out;
                    }
                    auto out = idea (r, "fixed pair, top stepping down", "Two notes struck in 3+3+2: the tonic below, the top a scale step lower every two bars");
                    const auto low = tonicIn (55, 66);
                    const auto top = tonicIn (67, 78);
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto now = scaleStep (key, top, -(bar / 2));
                        for (const auto [k, len] : { std::pair { 0, 3 }, std::pair { 3, 3 }, std::pair { 6, 2 } })
                        {
                            addNote (out, o + k * e, len * e * 0.95, low, soft(), length);
                            addNote (out, o + k * e, len * e * 0.95, now, soft(), length);
                        }
                    }
                    return out;
                }
                if (type == "SLIDING CHORDS")
                {
                    if (a == 1)
                    {
                        auto out = idea (r, "held and sliding", "A power chord held a bar, then the same shape a half step down");
                        for (int bar = 0; bar < bars; ++bar)
                        {
                            const auto o = bar * bpb;
                            const auto root = lowRoot (bar % 2 == 0 ? o : o - bpb) - (bar % 2);
                            power (out, o, bpb * 0.96, root, open());
                        }
                        return out;
                    }
                    auto out = idea (r, "four-bar figure", "A chord, four eighths, a chord; eighths and a lick; a line in quarters; a chord sliding down a half step");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = lowRoot (o);
                        switch (bar % 4)
                        {
                            case 0:
                                power (out, o, e * 0.95, root, open());
                                for (int i = 1; i <= 4; ++i)
                                    power (out, o + i * e, e * 0.85, root, open(), false);
                                power (out, o + 6 * e, 2 * e * 0.95, root, open());
                                break;
                            case 1:
                            {
                                for (int i = 0; i < 4; ++i)
                                    power (out, o + i * e, e * 0.85, root, muted(), false);
                                const int lick[] { 3, 2, 0, 2, 3, 5, 3, 0 };
                                for (int k = 0; k < 8; ++k)
                                    addNote (out, o + 4 * e + k * s16, s16 * 0.95, root + 12 + lick[k], open(), length);
                                break;
                            }
                            case 2:
                            {
                                const int line[] { 0, 2, 0, -3 };
                                for (int k = 0; k < 4; ++k)
                                    addNote (out, o + k * q, q * 0.92, root + 12 + line[k], open(), length);
                                break;
                            }
                            default:
                                power (out, o, 2 * q * 0.96, root, open());
                                power (out, o + 2 * q, 2 * q * 0.96, root - 1, open());
                                break;
                        }
                    }
                    return out;
                }
                if (type == "GUITARS OUT, THEN BACK")
                {
                    const auto back = backAt();
                    auto out = idea (r, a == 0 ? "back halfway" : "back for the last quarter",
                                     "No guitar until " + juce::String (a == 0 ? "halfway" : "the last quarter") + ", then power chords on the eighths, the last bar an octave up");
                    for (int i = 0; i * e < length - 1.0e-6; ++i)
                    {
                        const auto at = i * e;
                        if (at < back - 1.0e-6)
                            continue;
                        const auto lift = at >= (bars - 1) * bpb - 1.0e-6 && back < (bars - 1) * bpb - 1.0e-6 ? 12 : 0;
                        power (out, at, e * 0.85, lowRoot (at) + lift, i % 2 == 0 ? open() : muted(), false);
                    }
                    return out;
                }
                // SLIDE OVER A HELD ROOT
                auto out = idea (r, a == 0 ? "held root & stabs in turn" : "held root only",
                                 a == 0 ? "A palm-muted root rung on 1 with a sliding triplet figure over it, then a bar of syncopated stabs"
                                        : "A palm-muted root rung on 1 with a sliding triplet figure over it, every bar");
                for (int bar = 0; bar < bars; ++bar)
                {
                    const auto o = bar * bpb;
                    const auto root = lowRoot (o);
                    if (a == 0 && bar % 2 == 1)
                    {
                        for (const auto k : { 0, 3, 6, 10 })
                            power (out, o + k * s16, 2 * s16 * 0.9, root, open(), false);
                        continue;
                    }
                    addNote (out, o, 2 * q * 0.95, root, loose(), length);
                    const auto fifth = root + 19;
                    const auto t3 = q / 3.0;
                    addNote (out, o + 2 * q, t3 * 0.95, fifth, open(), length);
                    addNote (out, o + 2 * q + t3, t3 * 0.95, scaleStep (key, fifth, 1), open(), length);
                    addNote (out, o + 2 * q + 2 * t3, t3 * 0.95, fifth, open(), length);
                    addNote (out, o + 3 * q, q * 0.95, fifth, open(), length);
                }
                return out;
            }

            LineIdea bass() const
            {
                const auto a = approach (ComposerLayer::bass);
                const auto b = LineIdea::Part::bass;
                auto held = [&] (double from)
                {
                    auto out = idea (b, from > 0.0 ? "out, then in halfway" : "roots held", from > 0.0 ? "Silent, then each bar's root held from halfway" : "Each bar's root held");
                    for (int bar = 0; bar < bars; ++bar)
                        if (bar * bpb >= from - 1.0e-6)
                            addNote (out, bar * bpb, bpb * 0.96, bassRoot (bar * bpb), 0.82f, length);
                    return out;
                };
                auto eighths = [&]
                {
                    auto out = idea (b, "root eighths", "The chord's root on every eighth");
                    for (int i = 0; i * e < length - 1.0e-6; ++i)
                        addNote (out, i * e, e * 0.88, bassRoot (i * e), i % 2 == 0 ? 0.82f : 0.7f, length);
                    return out;
                };
                auto doubled = [&] { return doubledLine (sortedGuitar(), spans, bassLowest(), bassHighest(), "guitar"); };
                if (type == "HALF-TIME MELODIC")
                    return a == 1 ? eighths() : held (0.0);
                if (type == "HEAVY BUILD" || type == "LATE-CHANGE CLIMB" || type == "SLIDING CHORDS")
                    return a == 0 ? doubled() : held (0.0);
                if (type == "CLEAN BRIDGE")
                {
                    if (a == 1)
                        return silent (b, "bass");
                    return held (a == 0 ? std::floor (bars / 2.0) * bpb : 0.0);
                }
                if (type == "GUITARS OUT, THEN BACK")
                    return a == 1 ? eighths() : held (0.0);
                return a == 1 ? doubled() : held (0.0);   // SLIDE OVER A HELD ROOT
            }

            LineIdea keys() const
            {
                const auto a = approach (ComposerLayer::chords);
                const auto k = LineIdea::Part::chords;
                if (type == "HALF-TIME MELODIC")
                {
                    if (a == 1)
                        return pads (k, 55);
                    if (a == 2)
                        return silent (k, "keys");
                    auto out = idea (k, "root octaves", "Each bar's root in octaves for half a bar (silent under the guitar's stabs)");
                    if (approach (ComposerLayer::guitar) == 1)
                        return silent (k, "keys");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto root = pitchOfClassIn (chordAt (bar * bpb).root, 48, 59);
                        for (const auto p : { root, root + 12 })
                            addNote (out, bar * bpb, 2 * q * 0.96, p, 0.45f, length);
                    }
                    return out;
                }
                if (type == "LATE-CHANGE CLIMB")
                {
                    if (a == 1)
                        return silent (k, "keys");
                    auto out = idea (k, "sus4 pad", "Each chord held as a sus4 on top (root, fourth, fifth)");
                    const auto segments = cleanSegments (spans, bpb);
                    for (size_t i = 0; i + 1 < segments.size(); ++i)
                    {
                        const auto root = pitchOfClassIn (chordAt (segments[i]).root, 60, 71);
                        for (const auto p : { root, root + 5, root + 7 })
                            addNote (out, segments[i], (segments[i + 1] - segments[i]) * 0.98, p, 0.4f, length);
                    }
                    return out;
                }
                if (type == "CLEAN BRIDGE")
                {
                    if (a == 1)
                        return pads (k, 55);
                    if (a == 2)
                        return silent (k, "keys");
                    auto out = idea (k, "fifths pulse", "The key's tonic and fifth pulsing in eighths");
                    const auto tonic = tonicIn (60, 71);
                    for (int i = 0; i * e < length - 1.0e-6; ++i)
                        for (const auto p : { tonic, tonic + 7 })
                            addNote (out, i * e, e * 0.8, p, i % 2 == 0 ? 0.42f : 0.34f, length);
                    return out;
                }
                if (type == "GUITARS OUT, THEN BACK")
                {
                    if (a == 0)
                        return pads (k, 55);
                    if (a == 2)
                        return silent (k, "keys");
                    auto out = idea (k, "arpeggio", "Each chord's notes picked up and down in eighths");
                    for (int i = 0; i * e < length - 1.0e-6; ++i)
                    {
                        const auto voicings = voicingsFor (chordAt (i * e), 60);
                        if (voicings.empty())
                            continue;
                        const auto& v = voicings.front();
                        const auto n = (int) v.size();
                        const auto period = juce::jmax (1, 2 * (n - 1));
                        const auto step = i % period;
                        addNote (out, i * e, e * 0.9, v[(size_t) juce::jlimit (0, n - 1, step < n ? step : period - step)], 0.45f, length);
                    }
                    return out;
                }
                if (type == "SLIDING CHORDS")
                    return a == 0 ? pads (k, 55) : silent (k, "keys");
                // HEAVY BUILD, SLIDE OVER A HELD ROOT: silent, then held pads.
                return a == 0 ? silent (k, "keys") : pads (k, 55);
            }

            std::optional<LineIdea> lead() const
            {
                if (bridgeApproaches (type, ComposerLayer::lead).isEmpty())
                    return std::nullopt;
                const auto l = LineIdea::Part::lead;
                if (approach (ComposerLayer::lead) == 1)
                    return silent (l, "lead");
                auto out = idea (l, "siren bends", "The chord's root high up, bent a whole step on 2 and 4");
                for (int bar = 0; bar < bars; ++bar)
                    for (const auto beat : { 1, 3 })
                    {
                        const auto at = bar * bpb + beat * q;
                        if (at >= length - 1.0e-6)
                            continue;
                        LineNote n { at, q * 0.9, pitchOfClassIn (chordAt (at).root, 69, 80) - 2, open() };
                        n.effect = LineNote::Effect::bend;
                        n.amount = 2.0f;
                        out.notes.push_back (n);
                    }
                return out;
            }

            LineIdea drums() const
            {
                const auto a = approach (ComposerLayer::drums);
                auto out = idea (LineIdea::Part::drums, {}, {});
                auto named = [&] (const juce::String& style, const juce::String& description)
                {
                    out.style = "Bridge: " + style;
                    out.description = description;
                };
                auto halfTimeBar = [&] (double o, bool crash)
                {
                    if (crash)
                        hit (out, o, 49, 0.72f);
                    for (int i = 0; i < 8; ++i)
                        hit (out, o + i * e, 42, i % 2 == 0 ? 0.56f : 0.42f);
                    hit (out, o, 36, 0.88f);
                    hit (out, o + 10 * s16, 36, 0.76f);
                    hit (out, o + 8 * s16, 38, 0.88f);
                };
                auto drivingBar = [&] (double o, bool crash)
                {
                    if (crash)
                        hit (out, o, 49, 0.76f);
                    for (int i = 0; i < 8; ++i)
                        hit (out, o + i * e, 42, i % 2 == 0 ? 0.6f : 0.46f);
                    hit (out, o, 36, 0.9f);
                    hit (out, o + 8 * s16, 36, 0.84f);
                    hit (out, o + 10 * s16, 36, 0.72f);
                    hit (out, o + 4 * s16, 38, 0.88f);
                    hit (out, o + 12 * s16, 38, 0.88f);
                };
                if (type == "HALF-TIME MELODIC")
                {
                    if (a == 1)
                    {
                        named ("half time", "Kick on 1 and the and of 3, the snare on 3, hats in eighths");
                        for (int bar = 0; bar < bars; ++bar)
                            halfTimeBar (bar * bpb, bar % 4 == 0);
                        return out;
                    }
                    named ("ghost groove, triplet fill", "A half-time groove with ghosted snares, the last two bars a triplet fill round the toms");
                    const auto fillFrom = bars >= 4 ? bars - 2 : bars;
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        if (bar < fillFrom)
                        {
                            halfTimeBar (o, bar % 4 == 0);
                            for (const auto g : { 3, 5, 11, 14 })
                                hit (out, o + g * s16, 38, 0.28f);
                            continue;
                        }
                        const int toms[] { 38, 48, 47, 45, 43, 41 };
                        const auto t3 = q / 3.0;
                        const auto count = (int) std::round (bpb / t3);
                        const auto done = (bar - fillFrom) * count;
                        for (int k = 0; k < count; ++k)
                        {
                            const auto i = done + k;
                            hit (out, o + k * t3, toms[(i / 2) % 6], 0.5f + 0.45f * (float) i / (float) juce::jmax (1, 2 * count - 1));
                            if (k % 3 == 0)
                                hit (out, o + k * t3, 36, 0.8f);
                        }
                    }
                    return out;
                }
                if (type == "HEAVY BUILD")
                {
                    if (a == 1)
                    {
                        named ("half time", "Kick on 1 and the and of 3, the snare on 3, the china on the beats");
                        for (int bar = 0; bar < bars; ++bar)
                        {
                            const auto o = bar * bpb;
                            if (bar % 4 == 0)
                                hit (out, o, 49, 0.76f);
                            for (int beat = 0; beat < 4; ++beat)
                                hit (out, o + beat * q, 52, 0.6f);
                            hit (out, o, 36, 0.9f);
                            hit (out, o + 10 * s16, 36, 0.78f);
                            hit (out, o + 8 * s16, 38, 0.9f);
                        }
                        return out;
                    }
                    if (a == 2)
                    {
                        named ("building snare", "The snare on the beats, then the eighths, then the sixteenths, louder each time; the kick on the beats");
                        const auto total = length;
                        for (int i = 0; i * s16 < total - 1.0e-6; ++i)
                        {
                            const auto at = i * s16;
                            const auto part = at / juce::jmax (1.0e-6, total);
                            const auto every = part < 1.0 / 3.0 ? 4 : (part < 2.0 / 3.0 ? 2 : 1);
                            if (i % every == 0)
                                hit (out, at, 38, (float) (0.45 + 0.5 * part));
                            if (i % 4 == 0)
                                hit (out, at, 36, 0.86f);
                        }
                        hit (out, 0.0, 49, 0.72f);
                        return out;
                    }
                    named ("kick with the guitar", "The kick on every guitar hit, the snare on 2 and 4, the china on the beats");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        if (bar % 4 == 0)
                            hit (out, o, 49, 0.76f);
                        for (int beat = 0; beat < 4; ++beat)
                            hit (out, o + beat * q, 52, 0.56f);
                        hit (out, o + 4 * s16, 38, 0.88f);
                        hit (out, o + 12 * s16, 38, 0.88f);
                    }
                    for (const auto t : riffHits (sortedGuitar()))
                        hit (out, t, 36, 0.86f);
                    return out;
                }
                if (type == "LATE-CHANGE CLIMB")
                {
                    if (a == 1)
                    {
                        named ("half time", "Kick on 1 and the and of 3, the snare on 3, hats in eighths");
                        for (int bar = 0; bar < bars; ++bar)
                            halfTimeBar (bar * bpb, bar % 4 == 0);
                        return out;
                    }
                    named ("kick on every sixteenth", "The kick on every sixteenth, the snare on 2 and 4, the china on the beats");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        if (bar % 4 == 0)
                            hit (out, o, 49, 0.76f);
                        for (int x = 0; x < 16; ++x)
                            hit (out, o + x * s16, 36, x % 4 == 0 ? 0.86f : 0.66f);
                        for (int beat = 0; beat < 4; ++beat)
                            hit (out, o + beat * q, 52, 0.58f);
                        hit (out, o + 4 * s16, 38, 0.9f);
                        hit (out, o + 12 * s16, 38, 0.9f);
                    }
                    return out;
                }
                if (type == "CLEAN BRIDGE")
                {
                    if (a == 1)
                    {
                        named ("no drums", "Nothing: the bridge bare");
                        return out;
                    }
                    if (a == 2)
                    {
                        named ("ride only", "The ride on the beats, a soft crash to start");
                        hit (out, 0.0, 49, 0.5f);
                        for (int i = 0; i * q < length - 1.0e-6; ++i)
                            hit (out, i * q, 51, 0.45f);
                        return out;
                    }
                    named ("snare march halfway", "Silent, then a soft snare march from halfway, the kick on 1 and 3");
                    const auto half = std::floor (bars / 2.0) * bpb;
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        if (o < half - 1.0e-6)
                            continue;
                        for (const auto k : { 0, 2, 3, 4, 6, 8, 10, 11, 12, 14 })
                            hit (out, o + k * s16, 38, k % 4 == 0 ? 0.6f : 0.36f);
                        hit (out, o, 36, 0.8f);
                        hit (out, o + 8 * s16, 36, 0.74f);
                    }
                    return out;
                }
                if (type == "SLIDING CHORDS")
                {
                    if (a == 1)
                    {
                        named ("half time", "Kick on 1 and the and of 3, the snare on 3, hats in eighths");
                        for (int bar = 0; bar < bars; ++bar)
                            halfTimeBar (bar * bpb, bar % 4 == 0);
                        return out;
                    }
                    named ("driving beat", "Kick on 1, 3 and the and of 3, the snare on 2 and 4, hats in eighths");
                    for (int bar = 0; bar < bars; ++bar)
                        drivingBar (bar * bpb, bar % 4 == 0);
                    return out;
                }
                if (type == "GUITARS OUT, THEN BACK")
                {
                    const auto back = backAt();
                    named (a == 0 ? "half time, then driving" : "toms, then driving",
                           (a == 0 ? juce::String ("Half time") : juce::String ("A tom pulse")) + " until the guitars come back, then a driving beat with a crash");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        if (o >= back - 1.0e-6)
                        {
                            drivingBar (o, std::abs (o - back) < 1.0e-6 || bar % 4 == 0);
                            continue;
                        }
                        if (a == 0)
                            halfTimeBar (o, bar == 0);
                        else
                        {
                            for (int beat = 0; beat < 4; ++beat)
                                hit (out, o + beat * q, 45, beat == 0 ? 0.7f : 0.55f);
                            hit (out, o + 14 * s16, 43, 0.6f);
                            hit (out, o, 36, 0.78f);
                        }
                    }
                    return out;
                }
                // SLIDE OVER A HELD ROOT
                if (a == 1)
                {
                    named ("half time", "Kick on 1 and the and of 3, the snare on 3, hats in eighths");
                    for (int bar = 0; bar < bars; ++bar)
                        halfTimeBar (bar * bpb, bar % 4 == 0);
                    return out;
                }
                named ("kick with the stabs", "The kick on the guitar's hits, the snare on 3, hats in eighths");
                for (int bar = 0; bar < bars; ++bar)
                {
                    const auto o = bar * bpb;
                    if (bar % 4 == 0)
                        hit (out, o, 49, 0.74f);
                    for (int i = 0; i < 8; ++i)
                        hit (out, o + i * e, 42, i % 2 == 0 ? 0.56f : 0.42f);
                    hit (out, o + 8 * s16, 38, 0.88f);
                }
                for (const auto t : riffHits (sortedGuitar()))
                    hit (out, t, 36, 0.84f);
                return out;
            }
        };

        std::optional<LineIdea> bridgeLayer (ComposerLayer l, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
        {
            if (spans.empty() || sectionBand (s) != SectionType::Band::bridge || l == ComposerLayer::melody)
                return std::nullopt;
            const Bridge b (s, spans, c);
            if (bridgeApproaches (b.type, l).isEmpty())
                return std::nullopt;
            std::optional<LineIdea> idea;
            switch (l)
            {
                case ComposerLayer::guitar:
                    idea = b.guitar();
                    if (c.guitarProfile.has_value())
                        keepInRange (*idea, c.guitarProfile->rangeLow(), c.guitarProfile->rangeHigh());
                    break;
                case ComposerLayer::bass:
                    idea = b.bass();
                    keepInRange (*idea, b.bassLowest(), b.bassHighest());
                    break;
                case ComposerLayer::chords: idea = b.keys(); break;
                case ComposerLayer::lead:
                {
                    idea = b.lead();
                    const auto& profile = c.leadProfile.has_value() ? c.leadProfile : c.guitarProfile;
                    if (idea.has_value() && profile.has_value())
                        keepInRange (*idea, profile->rangeLow(), profile->rangeHigh());
                    break;
                }
                case ComposerLayer::drums: idea = b.drums(); break;
                case ComposerLayer::melody: break;
            }
            if (idea.has_value())
                sortNotes (*idea);
            return idea;
        }

        //======================================================================
        // (0.109.0) The breakdown types (the owner's songs read for their shapes only; our names and our parts):
        // CHROMATIC CRAWL (Circle With Me), HIT & HOLD, THEN CARPET (This Fire Burns), CHUG & SQUEAL (Clown, Whole),
        // DOTTED TRITONE PUSH (Ten Ton Hammer, Capricorn), TRIPLET CLUSTER (Wildfire), GAPPED SIXTEENTHS (Parallels,
        // Stricken), THREES & TWOS (The Heart and the Shape, Another Life, the owner's band's drums), CALL & CHUG (The End
        // of Heartache, Waffle), HALF-TIME OPEN CHORDS (The Void), OFF-BEAT GALLOPS (This Calling). Pitches are over the
        // chord's root on the lowest string (the chord track's chords; the out-of-key steps are the breakdowns' own).
        struct Breakdown
        {
            const ComposerState& s;
            const std::vector<ChordSpan>& spans;
            const ComposerContext& c;
            juce::String type;
            double length, bpb, s16, e, q;
            int bars;
            Key key;

            Breakdown (const ComposerState& state, const std::vector<ChordSpan>& sp, const ComposerContext& context)
                : s (state), spans (sp), c (context), type (state.sectionType.trim().toUpperCase()),
                  length (sectionLength (sp)), bpb (context.beatsPerBar), s16 (context.beatsPerBar / 16.0),
                  e (context.beatsPerBar / 8.0), q (context.beatsPerBar / 4.0),
                  bars ((int) std::ceil (sectionLength (sp) / context.beatsPerBar - 1.0e-6)), key (sp.front().key) {}

            int approach (ComposerLayer l) const
            {
                const auto n = juce::jmax (1, breakdownApproaches (type, l).size());
                const auto& lane = s.lane (l);
                return lane.approach >= 0 ? lane.approach % n : (l == ComposerLayer::guitar ? lane.variation % n : 0);
            }
            const Chord& chordAt (double t) const { return chordAtIdea (spans, juce::jlimit (0.0, juce::jmax (0.0, length - 1.0e-6), t)); }
            GuitarTuning tuning() const { return c.guitarProfile.has_value() ? c.guitarProfile->tuning() : c.tuning; }
            GuitarVelocities velocities() const
            {
                return c.guitarProfile.has_value() && c.guitarProfile->velocities.has_value() ? *c.guitarProfile->velocities : c.velocities;
            }
            float muted() const { return GuitarVelocity::of (velocities().muteMid); }
            float tight() const { return GuitarVelocity::of (velocities().muteTight); }
            float open() const { return GuitarVelocity::of (velocities().open (velocities().sustain)); }
            float hard() const { return GuitarVelocity::of (velocities().open (velocities().sustainHard)); }
            float pinch() const { return GuitarVelocity::of (velocities().pinch); }
            float rake() const { return GuitarVelocity::of (velocities().rake); }
            int bassLowest() const { return c.bassProfile.has_value() ? c.bassProfile->rangeLow() : c.bassTuning.lowest(); }
            int bassHighest() const { return c.bassProfile.has_value() ? c.bassProfile->rangeHigh() : bassTop (c.bassTuning); }
            int lowRoot (double t) const { const auto low = tuning().lowest(); return rootNear (chordAt (t), low, low + 11); }
            int bassRoot (double t) const { return rootNear (chordAt (t), bassLowest(), juce::jmin (bassHighest(), bassLowest() + 11)); }
            int tonicIn (int low, int high) const { return pitchOfClassIn (key.pitchClass(), low, high); }
            // The bars the last part takes (the widening triplets, the open chords' build): two of four, four of eight.
            int lastBars() const { return bars >= 8 ? 4 : bars >= 4 ? 2 : 0; }

            static LineIdea idea (LineIdea::Part part, const juce::String& style, const juce::String& description)
            {
                return { part, "Breakdown: " + style, description, {} };
            }
            void add (LineIdea& out, double at, double len, int pitch, float velocity) const { addNote (out, at, len, pitch, velocity, length); }
            void power (LineIdea& out, double at, double len, int root, float velocity, bool octave = false) const
            {
                add (out, at, len, root, velocity);
                add (out, at, len, root + 7, velocity);
                if (octave)
                    add (out, at, len, root + 12, velocity);
            }
            // A dead-string scratch: the low strings struck muted, no pitch to speak of (short, the tightest mute).
            void scratch (LineIdea& out, double at, int root) const
            {
                for (const auto p : { root, root + 5, root + 10 })
                    add (out, at, s16 * 0.4, p, tight());
            }
            void hit (LineIdea& out, double at, int note, float velocity) const
            {
                if (at >= -1.0e-9 && at < length - 1.0e-6)
                    out.notes.push_back ({ at, 0.1, note, velocity });
            }
            LineIdea pads (LineIdea::Part part, int low) const
            {
                auto out = idea (part, "held pads", "Each chord held softly, moving to the nearest shape");
                const auto segments = cleanSegments (spans, bpb);
                std::vector<int> previous;
                for (size_t i = 0; i + 1 < segments.size(); ++i)
                {
                    const auto voicings = voicingsFor (chordAt (segments[i]), low);
                    if (voicings.empty())
                        continue;
                    const auto& shape = voicings[(size_t) juce::jlimit (0, (int) voicings.size() - 1, closestVoicing (voicings, previous, previous))];
                    previous = shape;
                    for (const auto p : shape)
                        add (out, segments[i], (segments[i + 1] - segments[i]) * 0.98, p, 0.4f);
                }
                return out;
            }
            LineIdea drone (LineIdea::Part part) const
            {
                auto out = idea (part, "low drone", "The key's tonic and its octave held low, two bars at a time");
                const auto tonic = tonicIn (36, 47);
                for (int bar = 0; bar < bars; bar += 2)
                    for (const auto p : { tonic, tonic + 12 })
                        add (out, bar * bpb, 2 * bpb * 0.98, p, 0.36f);
                return out;
            }
            LineIdea silent (LineIdea::Part part, const juce::String& what) const
            {
                return idea (part, what + " out", "Nothing here: the breakdown's weight is the guitars, bass and drums");
            }
            LineIdea sortedGuitar() const
            {
                auto g = guitar();
                sortNotes (g);
                return g;
            }
            // The guitar as the bass and the kick follow it: no harmonics, no scratches, nothing an octave over the low root.
            LineIdea lowGuitar() const
            {
                auto g = sortedGuitar();
                const auto top = tuning().lowest() + 12 + 7;
                g.notes.erase (std::remove_if (g.notes.begin(), g.notes.end(), [&] (const LineNote& n)
                {
                    return n.technique == LineNote::Technique::harmonic || n.length < s16 * 0.5 || n.pitch > top || n.velocity == rake();
                }), g.notes.end());
                return g;
            }

            //------------------------------------------------------------------
            LineIdea guitar() const
            {
                const auto a = approach (ComposerLayer::guitar);
                const auto r = LineIdea::Part::rhythm;
                if (type == "CHROMATIC CRAWL")
                {
                    auto out = idea (r, a == 0 ? "crawl & harmonics" : a == 1 ? "crawl & rakes" : "plain quarters",
                                     a == 0 ? "Low power chords in quarters creeping inside a minor third, back to the root on 4; high harmonics ring on 3"
                                   : a == 1 ? "Low power chords in quarters creeping inside a minor third, back to the root on 4; a raked chord rings on 3"
                                            : "Low power chords on every quarter, creeping up a half step at a time and back down to the root");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = lowRoot (o);
                        if (a == 2)
                        {
                            const int crawl[] { 3, 2, 1, 0 };
                            for (int k = 0; k < 4; ++k)
                                power (out, o + k * q, q * 0.92, root + (bar % 2 == 0 ? crawl[k] : juce::jmax (0, crawl[k] - 1)), k == 3 ? muted() : open());
                            continue;
                        }
                        if (bar % 8 == 7)
                        {
                            // The turn: a syncopated muted figure rocking between the root and the half step over it.
                            const int steps[] { 0, 3, 6, 8, 11, 14 };
                            for (int k = 0; k < 6; ++k)
                                power (out, o + steps[k] * s16, 2 * s16 * 0.9, root + (k % 2), muted());
                            continue;
                        }
                        const auto first = bar % 4 == 3 || bar % 2 == 1 ? 1 : 3;
                        power (out, o, q * 0.92, root + first, open());
                        power (out, o + q, q * 0.92, root + 2, open());
                        if (bar % 4 == 3)
                        {
                            // Harmonics in triplets over the last two beats (rakes: two raked chords).
                            for (int k = 0; k < 6; ++k)
                            {
                                LineNote n { o + 2 * q + k * q / 3.0, q / 3.0 * 0.95, root + (k % 2 == 0 ? 24 : 19), open() };
                                if (a == 0)
                                    n.technique = LineNote::Technique::harmonic;
                                else
                                    n.velocity = rake();
                                if (n.beat < length - 1.0e-6)
                                    out.notes.push_back (n);
                            }
                            continue;
                        }
                        if (a == 0)
                            for (const auto [at, p] : { std::pair { 0.0, 12 }, std::pair { 0.0, 19 }, std::pair { 0.0, 24 }, std::pair { 1.0, 19 }, std::pair { 1.0, 24 }, std::pair { 1.0, 28 } })
                            {
                                LineNote n { o + 2 * q + at * e, e * (at == 0.0 ? 2.0 : 1.0) * 0.95, root + p, open() };
                                n.technique = LineNote::Technique::harmonic;
                                if (n.beat < length - 1.0e-6)
                                    out.notes.push_back (n);
                            }
                        else
                        {
                            const int rakeNotes[] { 0, 7, 12, 15, 19 };
                            for (int k = 0; k < 5; ++k)   // a rake: the strings strummed slowly, then let ring
                                add (out, o + 2 * q + k * 0.02, q * 0.95 - k * 0.02, root + 12 + rakeNotes[k], rake());
                        }
                        power (out, o + 3 * q, q * 0.9, root, muted());
                    }
                    return out;
                }
                if (type == "HIT & HOLD, THEN CARPET")
                {
                    const auto half = a == 0 ? juce::jmax (1, bars / 2) * bpb : a == 1 ? length : 0.0;
                    auto out = idea (r, a == 0 ? "hold, then eighths" : a == 1 ? "hit & hold only" : "muted eighths only",
                                     a == 0 ? "A stab and a held chord, then four muted eighths; from halfway, muted eighths wandering inside a minor third"
                                   : a == 1 ? "A stab and a held chord, then four muted eighths, every fourth bar a half step lower"
                                            : "Palm-muted eighths wandering inside a minor third, every second bar a half step lower");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = lowRoot (o);
                        if (o < half - 1.0e-6)
                        {
                            const auto down = bar % 4 == 2 ? -1 : 0;
                            power (out, o, e * 0.9, root + 3 + down, open());
                            power (out, o + e, 3 * e * 0.95, root + 2 + down, open(), true);
                            const int tail[] { 3, 2, 3, 0 };
                            for (int k = 0; k < 4; ++k)
                                power (out, o + 4 * e + k * e, e * 0.85, root + juce::jmax (0, tail[k] + (k < 3 ? down : 0)), muted(), k == 2);
                            continue;
                        }
                        const int walk[] { 3, 2, 2, 2, 3, 2, 3, 0 };
                        for (int k = 0; k < 8; ++k)
                            power (out, o + k * e, e * 0.85, root + juce::jmax (0, walk[k] - (bar % 2 == 1 && k < 7 ? 1 : 0)), muted());
                    }
                    return out;
                }
                if (type == "CHUG & SQUEAL")
                {
                    auto out = idea (r, a == 0 ? "chugs & held squeal" : a == 1 ? "chug pairs & stab" : "squeal on 2 and 4",
                                     a == 0 ? "Muted root chugs answered by a high chord held a minor ninth up; every fourth bar ends in a chromatic pickup"
                                   : a == 1 ? "Pairs of muted chugs with gaps, a high clashing stab (a minor ninth and a ninth) rung at the bar's end; every fourth bar the stab in eighths"
                                            : "Sparse muted chugs, a high note bent up on 2 and 4");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = lowRoot (o);
                        if (a == 1)
                        {
                            if (bar % 4 == 3)
                            {
                                for (int k = 0; k < 8; ++k)
                                    for (const auto p : { root + 25, root + 26 })
                                        add (out, o + k * e, e * 0.85, p, open());
                                continue;
                            }
                            for (const auto k : { 0, 1, 3, 4 })
                                power (out, o + k * e, e * 0.85, root, muted());
                            for (const auto p : { root + 25, root + 26 })
                                add (out, o + 6 * e, 2 * e * 0.95, p, open());
                            continue;
                        }
                        if (a == 2)
                        {
                            for (const auto k : { 0, 6, 8, 14 })
                                power (out, o + k * s16, s16 * 1.6, root, muted());
                            for (const auto beat : { 1, 3 })
                            {
                                LineNote n { o + beat * q, q * 0.9, root + 24, hard() };
                                n.effect = LineNote::Effect::bend;
                                n.amount = 1.0f;
                                if (n.beat < length - 1.0e-6)
                                    out.notes.push_back (n);
                            }
                            continue;
                        }
                        for (const auto k : { 0, 2, 3, 9, 10 })
                            power (out, o + k * s16, s16 * 0.9, root, muted());
                        for (const auto k : { 4, bar % 4 == 3 ? -1 : 12 })
                            if (k >= 0)
                                for (const auto p : { root + 13, root + 20, root + 25 })
                                    add (out, o + k * s16, 4 * s16 * 0.95, p, open());
                        if (bar % 4 == 3)
                            for (const auto [k, up] : { std::pair { 12, 0 }, std::pair { 13, 1 }, std::pair { 14, 3 }, std::pair { 15, 3 } })
                                power (out, o + k * s16, s16 * 0.9, root + up, open());
                    }
                    return out;
                }
                if (type == "DOTTED TRITONE PUSH")
                {
                    if (a == 2)
                    {
                        auto out = idea (r, "root & tritone, scratches", "The root and the tritone on dotted eighths, dead-string scratches in the gaps; every fourth bar a high chord sliding down");
                        for (int bar = 0; bar < bars; ++bar)
                        {
                            const auto o = bar * bpb;
                            const auto root = lowRoot (o);
                            power (out, o, 3 * s16 * 0.9, root, open());
                            power (out, o + 3 * s16, s16 * 0.9, root + 6, open());
                            for (int k = 4; k < 8; ++k)
                                scratch (out, o + k * s16, root);
                            if (bar % 4 == 3)
                            {
                                for (const auto [k, up] : { std::pair { 8, 18 }, std::pair { 10, 17 }, std::pair { 12, 15 } })
                                    power (out, o + k * s16, 2 * s16 * 0.95, root + up, open());
                                scratch (out, o + 14 * s16, root);
                                scratch (out, o + 15 * s16, root);
                                continue;
                            }
                            power (out, o + 8 * s16, 3 * s16 * 0.9, root, open());
                            power (out, o + 11 * s16, s16 * 0.9, root + 6, open());
                            scratch (out, o + 12 * s16, root);
                        }
                        return out;
                    }
                    const auto build = a == 0 ? juce::jlimit (0, bars - 1, juce::jmax (1, bars / 4)) : 0;
                    auto out = idea (r, a == 0 ? "scratches, then the push" : "the push only",
                                     a == 0 ? "Dead-string scratches growing to eighths, then power chords in a dotted 3+3+2 push climbing to the tritone"
                                            : "Power chords in a dotted 3+3+2 push climbing to the tritone, two scratches closing every second bar");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = lowRoot (o);
                        if (bar < build)
                        {
                            if (bar == build - 1)
                                for (int k = 0; k < 8; ++k)
                                    scratch (out, o + k * e, root);
                            else
                                for (const auto k : { 0, 14, 15 })
                                    scratch (out, o + k * s16, root);
                            continue;
                        }
                        if ((bar - build) % 2 == 0)
                        {
                            power (out, o, 6 * s16 * 0.92, root + 2, open(), true);
                            power (out, o + 6 * s16, 6 * s16 * 0.92, root + 3, open(), true);
                            power (out, o + 12 * s16, 6 * s16 * 0.95, root + 6, open(), true);   // (tied over the bar line)
                        }
                        else
                        {
                            power (out, o + 2 * s16, 6 * s16 * 0.92, root, open(), true);
                            power (out, o + 8 * s16, 4 * s16 * 0.92, root + 2, open(), true);
                            scratch (out, o + 12 * s16, root);
                            scratch (out, o + 14 * s16, root);
                        }
                    }
                    return out;
                }
                if (type == "TRIPLET CLUSTER")
                {
                    const auto wide = a == 0 ? lastBars() : 0;
                    auto out = idea (r, a == 0 ? "triplets, then wider" : "triplets only",
                                     a == 0 ? "A low cluster (a half step over the root, its fifth and octave) in groups of triplets with gaps; the last bars in wider triplets, as if slowing"
                                            : "A low cluster (a half step over the root, its fifth and octave) in groups of triplets with gaps");
                    const auto t = q / 3.0;
                    const bool groups[3][4] { { true, false, true, true }, { true, true, true, false }, { true, false, true, true } };
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = lowRoot (o);
                        const auto cluster = [&] (double at, double len, float v) { for (const auto p : { root + 1, root + 7, root + 12 }) add (out, at, len, p, v); };
                        if (bar >= bars - wide)
                        {
                            for (int k = 0; k < 6; ++k)
                                cluster (o + k * 2 * t, 2 * t * 0.9, open());
                            continue;
                        }
                        const auto& g = groups[bar % 3];
                        for (int k = 0; k < 4; ++k)
                            if (g[k])
                                for (int i = 0; i < 3; ++i)
                                {
                                    if (bar % 3 == 2 && k == 0 && i > 0)
                                        break;   // (a lone hit to start the third bar)
                                    cluster (o + (k * 3 + i) * t, t * 0.85, i == 0 ? open() : muted());
                                }
                    }
                    return out;
                }
                if (type == "GAPPED SIXTEENTHS")
                {
                    if (a == 1)
                    {
                        auto out = idea (r, "chug stream & dark stabs", "Muted root chugs on every sixteenth, power chords stabbed through them on dark steps (minor third, flat sixth, fifth, flat second)");
                        for (int bar = 0; bar < bars; ++bar)
                        {
                            const auto o = bar * bpb;
                            const auto root = lowRoot (o);
                            std::map<int, int> stabs = bar % 2 == 0 ? std::map<int, int> { { 0, 3 }, { 3, 8 }, { 6, 7 }, { 10, 1 } }
                                                                    : std::map<int, int> { { 0, 3 }, { 14, 3 }, { 15, 5 } };
                            for (int k = 0; k < 16; ++k)
                            {
                                if (const auto stab = stabs.find (k); stab != stabs.end())
                                    power (out, o + k * s16, s16 * 0.9, root + stab->second, open(), true);
                                else
                                    power (out, o + k * s16, s16 * 0.8, root, muted());
                            }
                        }
                        return out;
                    }
                    auto out = idea (r, "gaps in the sixteenths", "One muted power chord on the sixteenths, the groove made only by where the gaps fall");
                    const char* patterns[] { "xxx-xxx-xxxx-xxx", "-xx-xxx-xxxx-xxx", "xxx-xx-xxx-xxxx-", "-xxx-xx-xxx-x-xx" };
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = lowRoot (o);
                        const auto* p = patterns[bar % 4];
                        for (int k = 0; k < 16; ++k)
                            if (p[k] == 'x')
                                power (out, o + k * s16, s16 * 0.85, root, k == 0 || p[k > 0 ? k - 1 : 0] == '-' ? open() : muted());
                    }
                    return out;
                }
                if (type == "THREES & TWOS")
                {
                    if (a == 1)
                    {
                        auto out = idea (r, "3+3+3+3+2+2", "A dark chord (a flat sixth up) taking turns with the root chug in threes, then two short hits stepping up");
                        for (int bar = 0; bar < bars; ++bar)
                        {
                            const auto o = bar * bpb;
                            const auto root = lowRoot (o);
                            power (out, o, 3 * s16 * 0.9, root + 8, open(), true);
                            power (out, o + 3 * s16, 3 * s16 * 0.9, root, muted());
                            power (out, o + 6 * s16, 3 * s16 * 0.9, root + 8, open(), true);
                            power (out, o + 9 * s16, 3 * s16 * 0.9, root, muted());
                            power (out, o + 12 * s16, 2 * s16 * 0.9, root + (bar % 2 == 0 ? 7 : 2), open());
                            power (out, o + 14 * s16, 2 * s16 * 0.9, root + (bar % 2 == 0 ? 0 : 3), open());
                        }
                        return out;
                    }
                    auto out = idea (r, "3+3+2 stabs & chugs", "Muted chugs in eighths grouped 3+3+2, the next bar the gaps filled; every fourth bar on a high clashing chord");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = lowRoot (o);
                        const auto* p = bar % 2 == 0 ? "xx-xx-xx" : "-xx-xxxx";
                        for (int k = 0; k < 8; ++k)
                            if (p[k] == 'x')
                            {
                                if (bar % 4 == 3)
                                {
                                    for (const auto up : { 5, 12, 19 })
                                        add (out, o + k * e, e * 0.85, root + up, k == 0 || p[k - 1] == '-' ? open() : muted());
                                }
                                else
                                    power (out, o + k * e, e * 0.85, root, k == 0 || p[k - 1] == '-' ? open() : muted());
                            }
                    }
                    return out;
                }
                if (type == "CALL & CHUG")
                {
                    auto out = idea (r, a == 0 ? "bent call, chug answer" : "octave stabs, walk down",
                                     a == 0 ? "A bar of single low notes bent and let go, answered by a bar of muted chugs ending on a flat-second accent"
                                            : "A bar of high octave stabs, answered by single notes walking down a half step at a time to the root");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = lowRoot (o);
                        if (bar % 2 == 0)
                        {
                            if (a == 0)
                            {
                                const std::tuple<int, int, int, float> call[] { { 0, 6, 5, 1.0f }, { 6, 2, 0, 0.0f }, { 8, 2, 5, 0.0f }, { 10, 2, 0, 0.0f }, { 12, 3, 3, 0.5f } };
                                for (const auto& [k, len, up, bend] : call)
                                {
                                    LineNote n { o + k * s16, len * s16 * 0.95, root + up, open() };
                                    if (bend > 0.0f)
                                    {
                                        n.effect = LineNote::Effect::bend;
                                        n.amount = bend;
                                    }
                                    if (n.beat < length - 1.0e-6)
                                        out.notes.push_back (n);
                                }
                            }
                            else
                            {
                                for (const auto [k, len, up] : { std::tuple { 0, 2, 10 }, std::tuple { 3, 3, 10 }, std::tuple { 6, 2, 12 }, std::tuple { 9, 2, 12 } })
                                    for (const auto p : { root + 12 + up, root + 24 + up })
                                        add (out, o + k * s16, len * s16 * 0.92, p, open());
                                power (out, o + 12 * s16, 2 * s16 * 0.9, root, muted());
                                power (out, o + 14 * s16, s16 * 0.9, root, muted());
                            }
                            continue;
                        }
                        if (a == 0)
                        {
                            for (const auto k : { 0, 3, 4, 7, 8, 11 })
                                power (out, o + k * s16, s16 * (k % 4 == 0 ? 2.6 : 0.9), root, muted());
                            power (out, o + 12 * s16, 4 * s16 * 0.92, root + 1, open(), true);
                        }
                        else
                            for (const auto [k, up] : { std::pair { 0, 5 }, std::pair { 3, 4 }, std::pair { 6, 3 }, std::pair { 9, 1 }, std::pair { 12, 0 } })
                                add (out, o + k * s16, (k == 12 ? 4 : 3) * s16 * 0.92, root + up, k == 12 ? open() : muted());
                    }
                    return out;
                }
                if (type == "HALF-TIME OPEN CHORDS")
                {
                    const auto build = a == 1 ? lastBars() : 0;
                    auto out = idea (r, a == 0 ? "open chords, sparse" : "open chords, then build",
                                     a == 0 ? "Wide open chords (root, fifth, octave and third) on a sparse half-time rhythm, moving with the chords"
                                            : "Wide open chords on a sparse half-time rhythm, the last bars in whole notes, then quarters, then eighths");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = lowRoot (o);
                        const auto pcs = chordAt (o).pitchClasses();
                        const auto third = pcs.size() > 1 ? pitchOfClassIn (pcs[1], root + 13, root + 24) : root + 16;
                        const auto chord = [&] (double at, double len) { for (const auto p : { root, root + 7, root + 12, third }) add (out, at, len, p, open()); };
                        if (bar >= bars - build)
                        {
                            const auto k = bar - (bars - build);
                            const auto step = k == 0 ? bpb : k == 1 ? q : e;
                            for (auto t = 0.0; t < bpb - 1.0e-6; t += step)
                                chord (o + t, step * 0.92);
                            continue;
                        }
                        for (const auto [k, len] : { std::pair { 0, 2 }, std::pair { 2, 4 }, std::pair { 6, 2 }, std::pair { 8, 6 }, std::pair { 14, 2 } })
                            chord (o + k * s16, len * s16 * 0.95);
                    }
                    return out;
                }
                // OFF-BEAT GALLOPS
                auto out = idea (r, a == 0 ? "gallop bursts & squeal" : "gallop bursts",
                                 a == 0 ? "Single-note gallops on the low root coming in after the downbeat, in bursts of three and two; a pinch squeal in a gap"
                                        : "Single-note gallops on the low root coming in after the downbeat, in bursts of three and two");
                const std::vector<int> cycle[4] { { 4, 8, 12 }, { 0, 4 }, { 4, 8, 12 }, { 8 } };
                for (int bar = 0; bar < bars; ++bar)
                {
                    const auto o = bar * bpb;
                    const auto root = lowRoot (o);
                    for (const auto k : cycle[bar % 4])
                    {
                        add (out, o + k * s16, s16 * 0.85, root, muted());
                        add (out, o + (k + 1) * s16, s16 * 0.85, root, muted());
                        add (out, o + (k + 2) * s16, 2 * s16 * 0.85, root, muted());
                    }
                    if (a == 0 && bar % 4 == 3)
                    {
                        LineNote n { o + 12 * s16, 4 * s16 * 0.95, root + 24, pinch() };
                        n.effect = LineNote::Effect::vibrato;
                        n.amount = 1.0f;
                        if (n.beat < length - 1.0e-6)
                            out.notes.push_back (n);
                    }
                }
                return out;
            }

            LineIdea bass() const
            {
                const auto a = approach (ComposerLayer::bass);
                const auto b = LineIdea::Part::bass;
                if (a == 1)
                {
                    auto out = idea (b, "roots held", "Each bar's root held under the guitar");
                    for (int bar = 0; bar < bars; ++bar)
                        add (out, bar * bpb, bpb * 0.96, bassRoot (bar * bpb), 0.82f);
                    return out;
                }
                auto out = doubledLine (lowGuitar(), spans, bassLowest(), bassHighest(), "guitar");
                out.style = "Breakdown: doubles the guitar";
                return out;
            }

            LineIdea keys() const
            {
                const auto a = approach (ComposerLayer::chords);
                const auto k = LineIdea::Part::chords;
                const auto names = breakdownApproaches (type, ComposerLayer::chords);
                const auto name = names[juce::jlimit (0, names.size() - 1, a)];
                if (name == "SILENT")
                    return silent (k, "keys");
                if (name == "HELD PADS")
                    return pads (k, 55);
                if (name == "LOW DRONE" || name == "LOW PAD")
                    return drone (k);
                if (name == "HIGH HELD NOTES")
                {
                    auto out = idea (k, "high held notes", "One high note at a time, each held two bars: the key's tonic, a step under, a step over, two under");
                    const auto top = tonicIn (72, 83);
                    const int steps[] { 0, -1, 1, -2 };
                    for (int bar = 0; bar < bars; bar += 2)
                        add (out, bar * bpb, 2 * bpb * 0.97, scaleStep (key, top, steps[(bar / 2) % 4]), 0.42f);
                    return out;
                }
                // STABS ON THE GROUPS: the chord voiced high on each of the guitar's group starts.
                auto out = idea (k, "stabs on the groups", "The chord voiced high, struck with each of the guitar's groups");
                std::vector<int> previous;
                for (const auto t : riffHits (lowGuitar()))
                {
                    const auto voicings = voicingsFor (chordAt (t), 60);
                    if (voicings.empty())
                        continue;
                    const auto& shape = voicings[(size_t) juce::jlimit (0, (int) voicings.size() - 1, closestVoicing (voicings, previous, previous))];
                    previous = shape;
                    for (const auto p : shape)
                        add (out, t, e * 0.8, p, 0.5f);
                }
                return out;
            }

            std::optional<LineIdea> lead() const
            {
                if (breakdownApproaches (type, ComposerLayer::lead).isEmpty())
                    return std::nullopt;
                const auto l = LineIdea::Part::lead;
                if (approach (ComposerLayer::lead) == 1)
                    return silent (l, "lead");
                auto out = idea (l, "melody on top", "A slow melody over the chords: a chord tone held each half bar, the nearest to the one before");
                auto last = tonicIn (67, 78);
                for (auto t = 0.0; t < length - 1.0e-6; t += 2 * q)
                {
                    const auto pcs = chordAt (t).pitchClasses();
                    auto best = last;
                    auto bestDistance = 99;
                    for (const auto pc : pcs)
                        for (auto p = 64; p <= 81; ++p)
                            if (((p % 12) + 12) % 12 == pc && std::abs (p - last) < bestDistance && (std::abs (p - last) > 0 || pcs.size() == 1))
                            {
                                best = p;
                                bestDistance = std::abs (p - last);
                            }
                    add (out, t, 2 * q * 0.96, best, 0.62f);
                    last = best;
                }
                return out;
            }

            LineIdea drums() const
            {
                const auto a = approach (ComposerLayer::drums);
                const auto names = breakdownApproaches (type, ComposerLayer::drums);
                const auto name = names[juce::jlimit (0, names.size() - 1, a)];
                auto out = idea (LineIdea::Part::drums, name.toLowerCase(), {});
                const auto hits = riffHits (lowGuitar());
                auto kicksInBar = [&] (double o, float v)
                {
                    for (const auto t : hits)
                        if (t >= o - 1.0e-6 && t < o + bpb - 1.0e-6)
                            hit (out, t, GmDrum::kick, v);
                };
                auto chinaQuarters = [&] (double o) { for (int k = 0; k < 4; ++k) hit (out, o + k * q, GmDrum::china, k == 0 ? 0.76f : 0.62f); };
                auto halfTime = [&] (double o, bool crash, bool china)
                {
                    if (crash)
                        hit (out, o, GmDrum::crash, 0.8f);
                    if (china)
                        chinaQuarters (o);
                    else
                        for (int k = 0; k < 8; ++k)
                            hit (out, o + k * e, GmDrum::closedHat, k % 2 == 0 ? 0.56f : 0.42f);
                    hit (out, o, GmDrum::kick, 0.9f);
                    hit (out, o + 3 * s16, GmDrum::kick, 0.72f);
                    hit (out, o + 10 * s16, GmDrum::kick, 0.76f);
                    hit (out, o + 8 * s16, GmDrum::snare, 0.9f);
                };
                auto carpet = [&] (double o, bool crash, bool snareEveryBeat)
                {
                    if (crash)
                        hit (out, o, GmDrum::crash, 0.86f);
                    chinaQuarters (o);
                    for (int k = 0; k < 16; ++k)
                        hit (out, o + k * s16, GmDrum::kick, k % 4 == 0 ? 0.9f : 0.74f);
                    if (snareEveryBeat)
                        for (int k = 0; k < 4; ++k)
                            hit (out, o + k * q, GmDrum::snare, 0.9f);
                    else
                        hit (out, o + 8 * s16, GmDrum::snare, 0.92f);
                };
                auto fill = [&] (double o)   // the bar's last beat: snare, snare, tom, tom in sixteenths
                {
                    const int notes[] { GmDrum::snare, GmDrum::snare, GmDrum::tomHigh, GmDrum::tomFloor };
                    for (int k = 0; k < 4; ++k)
                        hit (out, o + 12 * s16 + k * s16, notes[k], 0.7f + 0.06f * (float) k);
                };
                for (int bar = 0; bar < bars; ++bar)
                {
                    const auto o = bar * bpb;
                    const auto phrase = bar % 4 == 0;
                    if (name == "SNARE EVERY TWO BARS")
                    {
                        out.description = "The kick under the guitar, a china on 3, the snare only once every two bars";
                        if (bar % 2 == 0)
                        {
                            for (const auto k : { 0, 1, 3 })
                                hit (out, o + k * q, GmDrum::kick, 0.88f);
                            hit (out, o + 2 * q, GmDrum::china, 0.74f);
                        }
                        else
                        {
                            hit (out, o, GmDrum::snare, 0.94f);
                            hit (out, o + q, GmDrum::kick, 0.86f);
                            hit (out, o + 2 * q, GmDrum::china, 0.74f);
                            hit (out, o + 2 * q, GmDrum::crash, 0.7f);
                            for (const auto k : { 10, 12, 14 })
                                hit (out, o + k * s16, GmDrum::kick, 0.8f);
                        }
                        continue;
                    }
                    if (name == "HALF TIME, THEN CARPET")
                    {
                        out.description = "Half time with the kick on the guitar's hits and a china on 3, then from halfway a sixteenth double-kick carpet";
                        if (bar < juce::jmax (1, bars / 2))
                        {
                            kicksInBar (o, 0.88f);
                            hit (out, o + 2 * q, GmDrum::china, 0.76f);
                            hit (out, o + 2 * q, GmDrum::snare, 0.92f);
                            if (bar % 4 == 3)
                                fill (o);
                        }
                        else
                            carpet (o, bar == bars / 2 || phrase, bar % 4 == 3);
                        continue;
                    }
                    if (name == "DOUBLE-KICK CARPET" || name == "CARPET")
                    {
                        out.description = "Sixteenth kicks under everything, a china on every beat, the snare on 3; every fourth bar the snare on every beat";
                        carpet (o, phrase, bar % 4 == 3);
                        continue;
                    }
                    if (name == "KICK WITH THE CHUGS, RIDE" || name == "KICK WITH THE CHUGS, OPEN HATS")
                    {
                        const auto ride = name.endsWith ("RIDE");
                        out.description = ride ? "The kick on every chug, the snare on 2 and 4, the ride in sixteenths"
                                               : "The kick on every chug, the snare on 2 and 4, open hats on the eighths, a crash on every bar";
                        kicksInBar (o, 0.88f);
                        for (const auto k : { 1, 3 })
                            hit (out, o + k * q, GmDrum::snare, 0.9f);
                        if (ride)
                            for (int k = 0; k < 16; ++k)
                                hit (out, o + k * s16, GmDrum::ride, k % 4 == 0 ? 0.6f : 0.44f);
                        else
                            for (int k = 0; k < 8; ++k)
                                hit (out, o + k * e, GmDrum::openHat, k % 2 == 0 ? 0.62f : 0.5f);
                        if (phrase || ! ride)
                            hit (out, o, GmDrum::crash, 0.8f);
                        continue;
                    }
                    if (name == "KICK & CHINA ON THE GROUPS")
                    {
                        out.description = "The kick and a china on every group's start, the snare on 3, a sixteenth fill closing every two bars";
                        for (const auto t : hits)
                            if (t >= o - 1.0e-6 && t < o + bpb - 1.0e-6)
                            {
                                hit (out, t, GmDrum::kick, 0.88f);
                                hit (out, t, GmDrum::china, 0.66f);
                            }
                        hit (out, o + 2 * q, GmDrum::snare, 0.92f);
                        if (bar % 2 == 1)
                            fill (o);
                        continue;
                    }
                    if (name == "GROUPS, THEN CARPET IN TURN")
                    {
                        out.description = "A bar of kick and china in 3+3+2 with ghost snares, then a bar of double kick with the snare on every beat and the bell on the off-beats";
                        if (bar % 2 == 0)
                        {
                            hit (out, o, GmDrum::crash, 0.8f);
                            for (const auto k : { 0, 3, 6, 8, 11, 14 })
                            {
                                hit (out, o + k * s16, GmDrum::kick, 0.88f);
                                hit (out, o + k * s16, GmDrum::china, 0.66f);
                            }
                            for (const auto k : { 2, 5, 10, 13 })
                                hit (out, o + k * s16, GmDrum::snare, 0.26f);
                        }
                        else
                        {
                            for (int k = 0; k < 16; ++k)
                                hit (out, o + k * s16, GmDrum::kick, k % 4 == 0 ? 0.9f : 0.72f);
                            for (int k = 0; k < 4; ++k)
                            {
                                hit (out, o + k * q, GmDrum::snare, 0.88f);
                                hit (out, o + k * q + e, GmDrum::rideBell, 0.62f);
                            }
                        }
                        continue;
                    }
                    if (name == "HALF TIME, RIDE" || name == "HALF TIME, THEN BUILD")
                    {
                        const auto build = name.endsWith ("BUILD") ? lastBars() : 0;
                        out.description = build > 0 ? "Half time with the ride on the beats, the last bars building: kicks on the quarters, then the eighths, the snare with them"
                                                    : "Half time: the kick with the chords, one snare on 3, the ride on every beat";
                        if (bar >= bars - build)
                        {
                            const auto step = bar - (bars - build) < 2 ? q : e;
                            if (bar == bars - build)
                                hit (out, o, GmDrum::crash, 0.8f);
                            for (auto t = 0.0; t < bpb - 1.0e-6; t += step)
                            {
                                hit (out, o + t, GmDrum::kick, 0.84f);
                                if (bar - (bars - build) >= 1)
                                    hit (out, o + t, GmDrum::snare, 0.6f + 0.3f * (float) (t / bpb));
                            }
                            continue;
                        }
                        if (phrase)
                            hit (out, o, GmDrum::crash, 0.76f);
                        for (int k = 0; k < 4; ++k)
                            hit (out, o + k * q, GmDrum::ride, 0.6f);
                        for (const auto k : { 0, 2, 6, 14 })
                            hit (out, o + k * s16, GmDrum::kick, 0.86f);
                        hit (out, o + 2 * q, GmDrum::snare, 0.92f);
                        continue;
                    }
                    if (name == "DOUBLE KICK ON THE GALLOPS" || name == "KICK WITH THE GUITAR" || name == "KICK WITH THE CHUGS"
                        || name == "KICK ON THE PUSHES" || name == "KICK WITH THE TRIPLETS" || name == "KICK WITH THE ANSWER")
                    {
                        out.description = "The kick on every one of the guitar's hits, the snare on 3, a china on every beat";
                        if (name == "KICK WITH THE ANSWER")
                        {
                            out.description = "The call bars a crash and a kick on 1, the answer bars the kick on every chug; the snare on 2 and 4";
                            if (bar % 2 == 0)
                            {
                                hit (out, o, GmDrum::crash, 0.84f);
                                hit (out, o, GmDrum::kick, 0.9f);
                            }
                            else
                                kicksInBar (o, 0.86f);
                            for (const auto k : { 1, 3 })
                                hit (out, o + k * q, GmDrum::snare, 0.9f);
                            continue;
                        }
                        if (phrase)
                            hit (out, o, GmDrum::crash, 0.8f);
                        kicksInBar (o, 0.86f);
                        chinaQuarters (o);
                        hit (out, o + 2 * q, GmDrum::snare, 0.92f);
                        continue;
                    }
                    // HALF TIME
                    out.description = "Half time: the kick on 1, the snare on 3, a china on every beat";
                    halfTime (o, phrase, true);
                }
                return out;
            }
        };

        std::optional<LineIdea> breakdownLayer (ComposerLayer l, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
        {
            if (spans.empty() || sectionBand (s) != SectionType::Band::breakdown || l == ComposerLayer::melody)
                return std::nullopt;
            const Breakdown b (s, spans, c);
            if (breakdownApproaches (b.type, l).isEmpty())
                return std::nullopt;
            std::optional<LineIdea> idea;
            switch (l)
            {
                case ComposerLayer::guitar:
                    idea = b.guitar();
                    if (c.guitarProfile.has_value())
                        keepInRange (*idea, c.guitarProfile->rangeLow(), c.guitarProfile->rangeHigh());
                    break;
                case ComposerLayer::bass:
                    idea = b.bass();
                    keepInRange (*idea, b.bassLowest(), b.bassHighest());
                    break;
                case ComposerLayer::chords: idea = b.keys(); break;
                case ComposerLayer::lead:
                {
                    idea = b.lead();
                    const auto& profile = c.leadProfile.has_value() ? c.leadProfile : c.guitarProfile;
                    if (idea.has_value() && profile.has_value())
                        keepInRange (*idea, profile->rangeLow(), profile->rangeHigh());
                    break;
                }
                case ComposerLayer::drums: idea = b.drums(); break;
                case ComposerLayer::melody: break;
            }
            if (idea.has_value())
                sortNotes (*idea);
            return idea;
        }

        //======================================================================
        // (0.110.0) The interlude / post-chorus types (the owner's songs read for their shapes only; our names and parts):
        // HOOK RIFF (Faster), CHORUS CARRIES ON (Another Life, The End of the Dream), PEDAL IN THREES (Stricken, Black), HIGH
        // TRILL HOOK (The Heart and the Shape, The Void, The End of Heartache), PEDAL & HIGH MELODY (This Calling, Circle With
        // Me), BURST & ANSWER (Waffle, The End of Heartache), STAB & ANSWER (Call Me When You're Sober, Clown), TREMOLO OVER
        // CHUGS (All Night Lights, Jaded), CLEAN BREATH (Come Clarity, At the End of August), TRITONE AIR & BUILD (Ten Ton
        // Hammer, Capricorn), CHROMATIC WOBBLE (Wildfire).
        struct Interlude
        {
            const ComposerState& s;
            const std::vector<ChordSpan>& spans;
            const ComposerContext& c;
            juce::String type;
            double length, bpb, s16, e, q;
            int bars;
            Key key;

            Interlude (const ComposerState& state, const std::vector<ChordSpan>& sp, const ComposerContext& context)
                : s (state), spans (sp), c (context), type (state.sectionType.trim().toUpperCase()),
                  length (sectionLength (sp)), bpb (context.beatsPerBar), s16 (context.beatsPerBar / 16.0),
                  e (context.beatsPerBar / 8.0), q (context.beatsPerBar / 4.0),
                  bars ((int) std::ceil (sectionLength (sp) / context.beatsPerBar - 1.0e-6)), key (sp.front().key) {}

            int approach (ComposerLayer l) const
            {
                const auto n = juce::jmax (1, interludeApproaches (type, l).size());
                const auto& lane = s.lane (l);
                return lane.approach >= 0 ? lane.approach % n : (l == ComposerLayer::guitar ? lane.variation % n : 0);
            }
            const Chord& chordAt (double t) const { return chordAtIdea (spans, juce::jlimit (0.0, juce::jmax (0.0, length - 1.0e-6), t)); }
            GuitarTuning tuning() const { return c.guitarProfile.has_value() ? c.guitarProfile->tuning() : c.tuning; }
            GuitarVelocities velocities() const
            {
                return c.guitarProfile.has_value() && c.guitarProfile->velocities.has_value() ? *c.guitarProfile->velocities : c.velocities;
            }
            float muted() const { return GuitarVelocity::of (velocities().muteMid); }
            float tight() const { return GuitarVelocity::of (velocities().muteTight); }
            float open() const { return GuitarVelocity::of (velocities().open (velocities().sustain)); }
            float hard() const { return GuitarVelocity::of (velocities().open (velocities().sustainHard)); }
            float pinch() const { return GuitarVelocity::of (velocities().pinch); }
            float rake() const { return GuitarVelocity::of (velocities().rake); }
            int bassLowest() const { return c.bassProfile.has_value() ? c.bassProfile->rangeLow() : c.bassTuning.lowest(); }
            int bassHighest() const { return c.bassProfile.has_value() ? c.bassProfile->rangeHigh() : bassTop (c.bassTuning); }
            int lowRoot (double t) const { const auto low = tuning().lowest(); return rootNear (chordAt (t), low, low + 11); }
            int bassRoot (double t) const { return rootNear (chordAt (t), bassLowest(), juce::jmin (bassHighest(), bassLowest() + 11)); }
            int tonicIn (int low, int high) const { return pitchOfClassIn (key.pitchClass(), low, high); }
            // The bars the last part takes (the widening triplets, the open chords' build): two of four, four of eight.
            int lastBars() const { return bars >= 8 ? 4 : bars >= 4 ? 2 : 0; }

            static LineIdea idea (LineIdea::Part part, const juce::String& style, const juce::String& description)
            {
                return { part, "Interlude: " + style, description, {} };
            }
            void add (LineIdea& out, double at, double len, int pitch, float velocity) const { addNote (out, at, len, pitch, velocity, length); }
            void power (LineIdea& out, double at, double len, int root, float velocity, bool octave = false) const
            {
                add (out, at, len, root, velocity);
                add (out, at, len, root + 7, velocity);
                if (octave)
                    add (out, at, len, root + 12, velocity);
            }
            // A dead-string scratch: the low strings struck muted, no pitch to speak of (short, the tightest mute).
            void scratch (LineIdea& out, double at, int root) const
            {
                for (const auto p : { root, root + 5, root + 10 })
                    add (out, at, s16 * 0.4, p, tight());
            }
            void hit (LineIdea& out, double at, int note, float velocity) const
            {
                if (at >= -1.0e-9 && at < length - 1.0e-6)
                    out.notes.push_back ({ at, 0.1, note, velocity });
            }
            LineIdea pads (LineIdea::Part part, int low) const
            {
                auto out = idea (part, "held pads", "Each chord held softly, moving to the nearest shape");
                const auto segments = cleanSegments (spans, bpb);
                std::vector<int> previous;
                for (size_t i = 0; i + 1 < segments.size(); ++i)
                {
                    const auto voicings = voicingsFor (chordAt (segments[i]), low);
                    if (voicings.empty())
                        continue;
                    const auto& shape = voicings[(size_t) juce::jlimit (0, (int) voicings.size() - 1, closestVoicing (voicings, previous, previous))];
                    previous = shape;
                    for (const auto p : shape)
                        add (out, segments[i], (segments[i + 1] - segments[i]) * 0.98, p, 0.4f);
                }
                return out;
            }
            LineIdea drone (LineIdea::Part part) const
            {
                auto out = idea (part, "low drone", "The key's tonic and its octave held low, two bars at a time");
                const auto tonic = tonicIn (36, 47);
                for (int bar = 0; bar < bars; bar += 2)
                    for (const auto p : { tonic, tonic + 12 })
                        add (out, bar * bpb, 2 * bpb * 0.98, p, 0.36f);
                return out;
            }
            LineIdea silent (LineIdea::Part part, const juce::String& what) const
            {
                return idea (part, what + " out", "Nothing here: room for the others");
            }
            LineIdea sortedGuitar() const
            {
                auto g = guitar();
                sortNotes (g);
                return g;
            }
            // The guitar as the bass and the kick follow it: no harmonics, no scratches, nothing an octave over the low root.
            LineIdea lowGuitar() const
            {
                auto g = sortedGuitar();
                const auto top = tuning().lowest() + 12 + 7;
                g.notes.erase (std::remove_if (g.notes.begin(), g.notes.end(), [&] (const LineNote& n)
                {
                    return n.technique == LineNote::Technique::harmonic || n.length < s16 * 0.5 || n.pitch > top || n.velocity == rake();
                }), g.notes.end());
                return g;
            }

            //------------------------------------------------------------------
            float soft() const { return GuitarVelocity::of (velocities().open (velocities().sustainSoft)); }
            int highRoot() const { return tonicIn (69, 80); }
            const Chord& chordOfBar (int bar) const { return chordAt (juce::jlimit (0, juce::jmax (0, bars - 1), bar) * bpb); }
            int barRoot (int bar) const { const auto low = tuning().lowest(); return rootNear (chordOfBar (bar), low, low + 11); }
            void single (LineIdea& out, double at, double len, int pitch, float velocity, LineNote::Effect effect = LineNote::Effect::none,
                         float amount = 0.0f, LineNote::Technique technique = LineNote::Technique::normal) const
            {
                if (at < -1.0e-9 || at >= length - 1.0e-6)
                    return;
                LineNote n { at, juce::jmax (0.01, juce::jmin (len, length - at)), pitch, velocity };
                n.effect = effect;
                n.amount = amount;
                n.technique = technique;
                out.notes.push_back (n);
            }
            // The chord voiced from `low`, nearest the shape before.
            std::vector<int> voiced (const Chord& chord, int low, std::vector<int>& previous) const
            {
                const auto voicings = voicingsFor (chord, low);
                if (voicings.empty())
                    return {};
                const auto shape = voicings[(size_t) juce::jlimit (0, (int) voicings.size() - 1, closestVoicing (voicings, previous, previous))];
                previous = shape;
                return shape;
            }
            // The chord's tone nearest `aim` within [low, high].
            int toneNear (const Chord& chord, int aim, int low, int high) const
            {
                auto best = aim;
                auto distance = 999;
                for (const auto pc : chord.pitchClasses())
                    for (auto p = low; p <= high; ++p)
                        if (((p % 12) + 12) % 12 == pc && std::abs (p - aim) < distance)
                        {
                            best = p;
                            distance = std::abs (p - aim);
                        }
                return best;
            }
            bool stabEnding() const { return type == "CHORUS CARRIES ON" && bars >= 4; }
            // TRITONE AIR & BUILD's four parts by bar: 0 the slides, 1 the tritone held, 2 out, 3 the scratches building.
            int airPart (int bar) const { return juce::jlimit (0, 3, bar * 4 / juce::jmax (1, bars)); }

            //------------------------------------------------------------------
            LineIdea guitar() const
            {
                const auto a = approach (ComposerLayer::guitar);
                const auto r = LineIdea::Part::rhythm;
                if (type == "HOOK RIFF")
                {
                    if (a == 1)
                    {
                        auto out = idea (r, "muted pedal teaser", "The chord's root palm-muted in eighths, moving to the next chord's root on the bar's last eighth");
                        for (int bar = 0; bar < bars; ++bar)
                            for (int k = 0; k < 8; ++k)
                                add (out, bar * bpb + k * e, e * 0.85, barRoot (k == 7 && bar + 1 < bars ? bar + 1 : bar), muted());
                        return out;
                    }
                    if (a == 2)
                    {
                        auto out = idea (r, "high descant", "One high note a bar struck in eighths: the key's root, a step up, two steps up, the root again");
                        const auto top = highRoot();
                        const int steps[] { 0, 1, 2, 0 };
                        for (int bar = 0; bar < bars; ++bar)
                            for (int k = 0; k < 8; ++k)
                                add (out, bar * bpb + k * e, e * 0.85, scaleStep (key, top, steps[bar % 4]), open());
                        return out;
                    }
                    auto out = idea (r, "pushed power chords", "Open power chords in eighths, each new chord struck on the bar's last eighth and held over the bar line");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        for (int k = bar == 0 ? 0 : 1; k < 7; ++k)
                            power (out, o + k * e, e * 0.85, barRoot (bar), open());
                        const auto next = bar + 1 < bars ? bar + 1 : bar;
                        power (out, o + 7 * e, (bar + 1 < bars ? 2 : 1) * e * 0.92, barRoot (next), open(), true);
                    }
                    return out;
                }
                if (type == "CHORUS CARRIES ON")
                {
                    const auto frozen = a == 1;
                    auto out = idea (r, frozen ? "frozen on the tonic" : "driving eighths",
                                     frozen ? "The chorus's eighths kept going, held on the key's tonic chord"
                                            : "The chorus's open eighths kept going under the tag, moving with the chords");
                    const auto tonic = rootNear (Chord { key.pitchClass(), Chord::Type::major }, tuning().lowest(), tuning().lowest() + 11);
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = frozen ? tonic : barRoot (bar);
                        if (stabEnding() && bar >= bars - 2)
                        {
                            for (const auto k : { 0, 1 })
                                power (out, o + k * q, q * 0.85, barRoot (bar), hard(), true);
                            continue;
                        }
                        for (int k = 0; k < 8; ++k)
                            power (out, o + k * e, e * 0.85, root, open(), true);
                    }
                    return out;
                }
                if (type == "PEDAL IN THREES")
                {
                    auto out = idea (r, a == 0 ? "pedal in threes" : "pedal, then rung chords",
                                     a == 0 ? "Sixteenths with the low root on every third one and moving notes between, the pattern turning against the bar"
                                            : "Sixteenths with the low root on every third one; every fourth bar two chords rung instead");
                    const int moving[] { 15, 12, 8, 7, 2, 3, 5, 6 };
                    auto m = 0;
                    for (int i = 0; i * s16 < length - 1.0e-6; ++i)
                    {
                        const auto bar = (int) std::floor (i * s16 / bpb + 1.0e-9);
                        const auto root = barRoot (bar);
                        if (a == 1 && bar % 4 == 3)
                        {
                            if (i % 16 == 0)
                            {
                                power (out, bar * bpb, 2 * q * 0.95, root + 3, open(), true);
                                power (out, bar * bpb + 2 * q, 2 * q * 0.95, root + 7, open(), true);
                            }
                            continue;
                        }
                        if (i % 3 == 0)
                            power (out, i * s16, s16 * 0.85, root, muted());
                        else
                            add (out, i * s16, s16 * 0.85, root + moving[m++ % 8], open());
                    }
                    return out;
                }
                if (type == "HIGH TRILL HOOK")
                {
                    if (a == 1)
                    {
                        auto out = idea (r, "chug & trill", "A bar of root chugs in sixteenths, then chugs and a fast trill high on the neck, the trill coming down every two bars");
                        const int tops[4][2] { { 7, 10 }, { 6, 9 }, { 5, 8 }, { 4, 7 } };
                        for (int bar = 0; bar < bars; ++bar)
                        {
                            const auto o = bar * bpb;
                            const auto root = barRoot (bar);
                            const auto* p = bar % 2 == 0 ? "xxxx-xxxxxxx-xxx" : "xxxx-xxx--------";
                            for (int k = 0; k < 16; ++k)
                                if (p[k] == 'x')
                                    power (out, o + k * s16, s16 * 0.85, root, muted());
                            if (bar % 2 == 1)
                            {
                                const auto& t = tops[(bar / 2) % 4];
                                const auto t6 = q / 6.0;
                                for (int k = 0; k < 12; ++k)
                                    add (out, o + 2 * q + k * t6, t6 * 0.9, root + 12 + t[k % 2], open());
                            }
                        }
                        return out;
                    }
                    auto out = idea (r, "high trill", "A low chord tone on 1, then two high notes a half step apart wiggling in a 3-3-2 syncopation");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        power (out, o, 2 * s16 * 0.9, barRoot (bar), open());
                        const auto high = toneNear (chordOfBar (bar), 74, 67, 79);
                        auto k = 0;
                        for (const auto step : { 2, 3, 5, 8, 9, 11, 14 })
                            add (out, o + step * s16, s16 * 0.9, high + (k++ % 2), open());
                    }
                    return out;
                }
                if (type == "PEDAL & HIGH MELODY")
                {
                    if (a == 1)
                    {
                        auto out = idea (r, "gallops & dyads", "A high octave-and-fifth on 1 answered by low gallops; every second bar the dyads step down");
                        for (int bar = 0; bar < bars; ++bar)
                        {
                            const auto o = bar * bpb;
                            const auto root = barRoot (bar);
                            auto gallop = [&] (int k) { add (out, o + k * s16, s16 * 0.85, root, muted()); add (out, o + (k + 1) * s16, s16 * 0.85, root, muted()); add (out, o + (k + 2) * s16, 2 * s16 * 0.85, root, muted()); };
                            auto dyad = [&] (int k, int top) { add (out, o + k * s16, 4 * s16 * 0.9, root + 12, open()); add (out, o + k * s16, 4 * s16 * 0.9, root + top, open()); };
                            dyad (0, 19);
                            if (bar % 2 == 0)
                            {
                                for (const auto k : { 4, 8, 12 })
                                    gallop (k);
                            }
                            else
                            {
                                gallop (4);
                                dyad (8, 20);
                                dyad (12, 17);
                            }
                        }
                        return out;
                    }
                    auto out = idea (r, "pedal & melody", "The low root muted in eighths with a melody on the high strings answering it, stepping down every two bars, the phrase ending on a bend");
                    const int melody[] { 21, 19, 17, 14 };
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = barRoot (bar);
                        const auto m = root + melody[(bar / 2) % 4];
                        for (int k = 0; k < 8; ++k)
                        {
                            if (k == 2 || k == 5)
                                single (out, o + k * e, e * 0.95, m, open());
                            else if (k == 6 && bar % 4 == 3)
                                single (out, o + k * e, 2 * e * 0.95, m - 2, open(), LineNote::Effect::bend, 2.0f);
                            else if (! (k == 7 && bar % 4 == 3))
                                add (out, o + k * e, e * 0.8, root, muted());
                        }
                    }
                    return out;
                }
                if (type == "BURST & ANSWER")
                {
                    auto out = idea (r, a == 0 ? "burst, walk down" : "chugs, slide down",
                                     a == 0 ? "A bar of high power chords in sixteenths, answered by single notes walking down a half step at a time to the root"
                                            : "A bar of chugs ending in silence, answered by a high pinched dyad sliding down; a rising slide into the next section");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = barRoot (bar);
                        if (a == 0)
                        {
                            if (bar % 2 == 0)
                            {
                                const auto* p = "xxxx=xxxxx=xx=xx";
                                for (int k = 0; k < 16; ++k)
                                    if (p[k] == 'x')
                                        power (out, o + k * s16, (k + 1 < 16 && p[k + 1] == '=' ? 2 : 1) * s16 * 0.9, k == 15 ? root : root + (k < 8 ? 10 : 12), k == 0 ? open() : muted());
                            }
                            else
                                for (const auto [k, up] : { std::pair { 0, 5 }, std::pair { 3, 4 }, std::pair { 6, 3 }, std::pair { 9, 1 }, std::pair { 12, 0 } })
                                    add (out, o + k * s16, (k == 12 ? 4 : 3) * s16 * 0.92, root + up, k == 12 ? open() : muted());
                            continue;
                        }
                        if (bar == bars - 1 && bars > 1)
                        {
                            power (out, o + 8 * s16, 4 * s16 * 0.9, root + 8, open());
                            power (out, o + 12 * s16, 4 * s16 * 0.9, root + 10, open());
                            continue;
                        }
                        if (bar % 2 == 0)
                        {
                            const auto* p = "xxxx-xxxxx-x----";
                            const auto at = (bar / 2) % 2 == 1 ? root + 8 : root;
                            for (int k = 0; k < 16; ++k)
                                if (p[k] == 'x')
                                    power (out, o + k * s16, s16 * 0.85, at, muted());
                        }
                        else
                        {
                            const int slide[] { 10, 9, 5, 7 };
                            for (int k = 0; k < 4; ++k)
                                for (const auto p : { root + 12 + slide[k], root + 17 + slide[k] })
                                    add (out, o + (8 + 2 * k) * s16, 2 * s16 * 0.92, p, k == 0 ? pinch() : open());
                        }
                    }
                    return out;
                }
                if (type == "STAB & ANSWER")
                {
                    auto out = idea (r, a == 0 ? "stab, early change" : "unison burst, high chord",
                                     a == 0 ? "The chord struck on three sixteenths and stopped; the next chord comes in early, on the last sixteenth"
                                            : "Four sixteenths on one power chord, a chord a half step down held, then a high chord left ringing");
                    std::vector<int> previous;
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = barRoot (bar);
                        if (a == 1)
                        {
                            for (int k = 0; k < 4; ++k)
                                power (out, o + k * s16, s16 * 0.85, root, muted());
                            power (out, o + 4 * s16, 4 * s16 * 0.92, root - 1 >= tuning().lowest() ? root - 1 : root + 11, open());
                            power (out, o + 8 * s16, 8 * s16 * 0.95, root + (bar % 2 == 0 ? 12 : 11), open());
                            continue;
                        }
                        const auto shape = voiced (chordOfBar (bar), 45, previous);
                        if (bar == 0)
                            for (int k = 0; k < 3; ++k)
                                for (const auto p : shape)
                                    add (out, o + k * s16, s16 * 0.85, p, open());
                        else
                            for (int k = 1; k < 3; ++k)
                                for (const auto p : shape)
                                    add (out, o + k * s16, s16 * 0.85, p, open());
                        auto copy = previous;
                        const auto next = bar + 1 < bars ? voiced (chordOfBar (bar + 1), 45, copy) : std::vector<int> {};
                        for (const auto p : next.empty() ? shape : next)
                            add (out, o + 15 * s16, (bar + 1 < bars ? 2 : 1) * s16 * 0.92, next.empty() ? p - 2 : p, open());
                    }
                    return out;
                }
                if (type == "TREMOLO OVER CHUGS")
                {
                    auto out = idea (r, a == 0 ? "chugs in threes" : "chugs walking down",
                                     a == 0 ? "Muted root chugs in two-hits-one-rest cells running across the bar"
                                            : "Muted chugs in two-hits-one-rest cells, the root walking down a scale step each bar");
                    for (int i = 0; i * s16 < length - 1.0e-6; ++i)
                    {
                        if (i % 3 == 2)
                            continue;
                        const auto bar = (int) std::floor (i * s16 / bpb + 1.0e-9);
                        auto root = barRoot (bar);
                        if (a == 1)
                        {
                            root = scaleStep (key, root, -(bar % 4));
                            while (root < tuning().lowest())
                                root += 12;
                        }
                        power (out, i * s16, s16 * 0.85, root, muted());
                    }
                    return out;
                }
                if (type == "CLEAN BREATH")
                {
                    if (a == 1)
                    {
                        auto out = idea (r, "falling top note", "Three notes on a dotted-quarter grid: a still lower pair, the top a scale step lower each bar, the fourth answering higher");
                        const auto low = tonicIn (55, 66);
                        const auto top = scaleStep (key, low, 9);
                        for (int bar = 0; bar < bars; ++bar)
                        {
                            const auto o = bar * bpb;
                            const auto t = scaleStep (key, top, bar % 4 == 3 ? 1 : -(bar % 4));
                            for (const auto [k, p] : { std::pair { 0, low }, std::pair { 6, low + 7 }, std::pair { 12, t } })
                                add (out, o + k * s16, (16 - k) * s16 * 0.95, p, soft());
                        }
                        return out;
                    }
                    const auto climb = lastBars();
                    auto out = idea (r, "arch arpeggio", "A clean picked arpeggio arching over each bar's chord, let ring; the last bars climb on root, fifth and octave as the band comes back");
                    std::vector<int> previous;
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = barRoot (bar) + 12;
                        if (bar >= bars - climb)
                        {
                            for (int k = 0; k < 8; ++k)
                                add (out, o + k * e, e * 0.9, k % 4 == 0 ? root - 12 : root + (k % 2 == 1 ? 7 : 12), open());
                            continue;
                        }
                        auto shape = voiced (chordOfBar (bar), 55, previous);
                        while (shape.size() < 3)
                            shape.push_back (shape.empty() ? root + 12 : shape.back() + 5);
                        const int order[] { -1, 0, 1, 2, 1, 0, 1, -1 };
                        for (int k = 0; k < 8; ++k)
                            add (out, o + k * e, e * 1.8, order[k] < 0 ? root : shape[(size_t) order[k]], soft());
                    }
                    return out;
                }
                if (type == "TRITONE AIR & BUILD")
                {
                    auto out = idea (r, a == 0 ? "air, then scratches" : "tritone held",
                                     a == 0 ? "A chord sliding up to a high note and falling to the tritone, the tritone held, the guitar out, then dead-string scratches building back"
                                            : "A chord sliding up to a high note and falling to the tritone, then the tritone held with vibrato to the end");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto root = barRoot (bar);
                        const auto part = a == 1 ? juce::jmin (1, airPart (bar)) : airPart (bar);
                        if (part == 0)
                        {
                            const auto at = root + (bar % 2);
                            power (out, o, 2 * q * 0.95, at, open(), true);
                            single (out, o + 2 * q, q * 0.95, at + 19, hard());
                            single (out, o + 3 * q, q * 0.95, at + 18, hard(), LineNote::Effect::vibrato, 1.0f);
                        }
                        else if (part == 1)
                        {
                            single (out, o, bpb * 0.97, root + 6, open(), LineNote::Effect::vibrato, 1.0f);
                            single (out, o, bpb * 0.97, root + 18, open(), LineNote::Effect::vibrato, 1.0f);
                        }
                        else if (part == 3)
                        {
                            const auto last = bar == bars - 1 || airPart (bar + 1) != 3;
                            if (last || bar == bars - 1)
                                for (int k = 0; k < 8; ++k)
                                    scratch (out, o + k * e, root);
                            else
                                for (const auto k : { 0, 14, 15 })
                                    scratch (out, o + k * s16, root);
                        }
                    }
                    return out;
                }
                // CHROMATIC WOBBLE
                auto out = idea (r, a == 0 ? "wobble in odd groups" : "wobble & accents",
                                 a == 0 ? "Muted low sixteenths wobbling a half step around the second, in groups of odd lengths that slide across the bar line"
                                        : "Muted low sixteenths wobbling a half step around the second, each group answered by an accent on a higher string");
                const int groups[] { 5, 3, 4, 6, 3 };
                const int wobble[] { 2, 3, 2, 1 };
                const int accents[] { 9, 10, 8 };
                auto i = 0, g = 0, w = 0;
                while (i * s16 < length - 1.0e-6)
                {
                    const auto n = groups[g % 5];
                    for (int k = 0; k < n && i * s16 < length - 1.0e-6; ++k, ++i)
                    {
                        const auto root = barRoot ((int) std::floor (i * s16 / bpb + 1.0e-9));
                        if (a == 1 && k == n - 1)
                            add (out, i * s16, s16 * 0.9, root + 12 + accents[g % 3], hard());
                        else
                            add (out, i * s16, s16 * 0.85, root + wobble[w++ % 4], muted());
                    }
                    ++i;   // (the gap)
                    ++g;
                }
                return out;
            }

            LineIdea bass() const
            {
                const auto names = interludeApproaches (type, ComposerLayer::bass);
                const auto name = names[juce::jlimit (0, names.size() - 1, approach (ComposerLayer::bass))];
                const auto b = LineIdea::Part::bass;
                if (name == "ROOTS HELD" || name == "OUT, THEN IN HALFWAY")
                {
                    const auto from = name == "ROOTS HELD" ? 0 : juce::jmax (1, bars / 2);
                    auto out = idea (b, name.toLowerCase(), from == 0 ? "Each bar's root held" : "Silent, then each bar's root held from halfway");
                    for (int bar = from; bar < bars; ++bar)
                        add (out, bar * bpb, bpb * 0.96, bassRoot (bar * bpb), 0.82f);
                    return out;
                }
                auto out = doubledLine (lowGuitar(), spans, bassLowest(), bassHighest(), "guitar");
                out.style = "Interlude: doubles the guitar";
                return out;
            }

            LineIdea keys() const
            {
                const auto names = interludeApproaches (type, ComposerLayer::chords);
                const auto name = names[juce::jlimit (0, names.size() - 1, approach (ComposerLayer::chords))];
                const auto k = LineIdea::Part::chords;
                if (name == "SILENT")
                    return silent (k, "keys");
                if (name == "HELD PADS")
                    return pads (k, 55);
                if (name == "LOW DRONE")
                    return drone (k);
                if (name == "PUSHED TRIADS")
                {
                    auto out = idea (k, "pushed triads", "Each bar's chord held, struck again on the and of 3, the next one pushed in on the last eighth");
                    std::vector<int> previous;
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto shape = voiced (chordOfBar (bar), 60, previous);
                        const auto start = bar == 0 ? 0.0 : o - e;
                        for (const auto p : shape)
                        {
                            add (out, start, (o + 6 * e - start) * 0.97, p, 0.45f);
                            add (out, o + 6 * e, e * 0.9, p, 0.42f);
                        }
                    }
                    return out;
                }
                if (name == "FALLING HIGH NOTES")
                {
                    auto out = idea (k, "falling high notes", "Three of the chord's notes a bar, high and sparse, each lower than the last");
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        auto p = toneNear (chordOfBar (bar), 86, 79, 91);
                        for (const auto at : { 0.0, 1.5 * q, 3.0 * q })
                        {
                            add (out, bar * bpb + at, 1.5 * q * 0.95, p, 0.4f);
                            p = toneNear (chordOfBar (bar), p - 3, p - 9, p - 1);
                        }
                    }
                    return out;
                }
                if (name == "SLOW LINE DOWN")
                {
                    auto out = idea (k, "slow line down", "One high note held two bars at a time, stepping down the key's scale");
                    const auto top = tonicIn (72, 83);
                    for (int bar = 0; bar < bars; bar += 2)
                        add (out, bar * bpb, 2 * bpb * 0.97, scaleStep (key, top, -((bar / 2) % 4)), 0.42f);
                    return out;
                }
                // STABS WITH THE GUITAR
                auto out = idea (k, "stabs with the guitar", "The chord voiced high, struck with each of the guitar's stabs");
                std::vector<int> previous;
                for (const auto t : riffHits (lowGuitar()))
                    for (const auto p : voiced (chordAt (t), 60, previous))
                        add (out, t, e * 0.8, p, 0.5f);
                return out;
            }

            std::optional<LineIdea> lead() const
            {
                const auto names = interludeApproaches (type, ComposerLayer::lead);
                if (names.isEmpty())
                    return std::nullopt;
                const auto name = names[juce::jlimit (0, names.size() - 1, approach (ComposerLayer::lead))];
                const auto l = LineIdea::Part::lead;
                if (name == "SILENT")
                    return silent (l, "lead");
                if (name == "TURNING HOOK" || name == "HOOK, OCTAVE DOWN")
                {
                    const auto down = name == "HOOK, OCTAVE DOWN" ? -12 : 0;
                    auto out = idea (l, name.toLowerCase(), "A two-voice hook in eighths: a turn a step and two over the high root, against one low note a bar from the chord, pushed in early; the fourth bar peaks and steps home");
                    const auto hr = highRoot() + down;
                    const auto s1 = scaleStep (key, hr, 1), s2 = scaleStep (key, hr, 2), s3 = scaleStep (key, hr, 3);
                    auto anchor = [&] (int bar) { return toneNear (chordOfBar (bar), hr - 5, hr - 9, hr - 3); };
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto o = bar * bpb;
                        const auto phraseStart = bar % 4 == 0;
                        if (phraseStart)
                        {
                            const int start[] { 0, 0, 1, 0, 1, 2, 1 };
                            for (int k = 0; k < 7; ++k)
                                single (out, o + k * e, e * 0.9, start[k] == 0 ? hr : start[k] == 1 ? s1 : s2, open());
                        }
                        else
                        {
                            single (out, o + e, e * 0.9, anchor (bar), open());
                            single (out, o + 2 * e, e * 0.9, hr, open());
                            single (out, o + 3 * e, e * 0.9, anchor (bar), open());
                            if (bar % 4 == 3)
                            {
                                const int peak[] { 3, 2, 1, 0 };
                                for (int k = 0; k < 4; ++k)
                                    single (out, o + (4 + k) * e, e * (k == 3 ? 1.8 : 0.9), peak[k] == 3 ? s3 : peak[k] == 2 ? s2 : peak[k] == 1 ? s1 : hr, open(),
                                            k == 3 ? LineNote::Effect::vibrato : LineNote::Effect::none, k == 3 ? 1.0f : 0.0f);
                                continue;
                            }
                            single (out, o + 4 * e, e * 0.9, s1, open());
                            single (out, o + 5 * e, e * 0.9, s2, open());
                            single (out, o + 6 * e, e * 0.9, s1, open());
                        }
                        if (bar + 1 < bars)
                            single (out, o + 7 * e, 2 * e * 0.95, anchor (bar + 1), open(), LineNote::Effect::vibrato, 0.6f);
                    }
                    return out;
                }
                if (name == "THREE AGAINST FOUR")
                {
                    auto out = idea (l, "three against four", "A three-note cell in quarters, drifting across the bar line");
                    const auto t = tonicIn (64, 75);
                    const int cell[] { 2, 1, -3 };
                    for (int i = 0; i * q < length - 1.0e-6; ++i)
                        if (! (stabEnding() && i * q >= (bars - 2) * bpb - 1.0e-6))
                            single (out, i * q, q * 0.92, scaleStep (key, t, cell[i % 3]), open());
                    return out;
                }
                if (name == "ANSWER IN THE GAP")
                {
                    auto out = idea (l, "answer in the gap", "A five-note line zig-zagging down on the off-beats, in the gap after each stab");
                    const int zig[] { 2, 0, 1, -1, -2 };
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        const auto top = toneNear (chordOfBar (bar), 74, 67, 79);
                        for (int k = 0; k < 5; ++k)
                            single (out, bar * bpb + (6 + 2 * k) * s16, s16 * 1.8, scaleStep (key, top, zig[k]), open());
                    }
                    return out;
                }
                // TREMOLO OCTAVES
                auto out = idea (l, "tremolo octaves", "Long octaves tremolo-picked, one a bar, falling a scale step each bar");
                const auto top = tonicIn (67, 78);
                for (int bar = 0; bar < bars; ++bar)
                {
                    const auto p = scaleStep (key, top, -(bar % 4));
                    for (const auto o : { 0, -12 })
                        single (out, bar * bpb, bpb * 0.97, p + o, open(), LineNote::Effect::none, 0.0f, LineNote::Technique::tremolo);
                }
                return out;
            }

            LineIdea drums() const
            {
                const auto names = interludeApproaches (type, ComposerLayer::drums);
                const auto name = names[juce::jlimit (0, names.size() - 1, approach (ComposerLayer::drums))];
                auto out = idea (LineIdea::Part::drums, name.toLowerCase(), {});
                const auto hits = riffHits (lowGuitar());
                auto kicksInBar = [&] (double o, float v)
                {
                    for (const auto t : hits)
                        if (t >= o - 1.0e-6 && t < o + bpb - 1.0e-6)
                            hit (out, t, GmDrum::kick, v);
                };
                auto backbeat = [&] (double o) { for (const auto k : { 4, 12 }) hit (out, o + k * s16, GmDrum::snare, 0.9f); };
                auto fill = [&] (double o)
                {
                    const int notes[] { GmDrum::snare, GmDrum::snare, GmDrum::tomHigh, GmDrum::tomFloor };
                    for (int k = 0; k < 4; ++k)
                        hit (out, o + (12 + k) * s16, notes[k], 0.7f + 0.06f * (float) k);
                };
                for (int bar = 0; bar < bars; ++bar)
                {
                    const auto o = bar * bpb;
                    const auto phrase = bar % 4 == 0;
                    if (stabEnding() && bar >= bars - 2)
                    {
                        for (const auto k : { 0, 1 })
                        {
                            hit (out, o + k * q, GmDrum::kick, 0.92f);
                            hit (out, o + k * q, GmDrum::crash, 0.86f);
                        }
                        continue;
                    }
                    if (name == "CRASH QUARTERS" || name == "RIDE BELL EIGHTHS")
                    {
                        const auto bell = name == "RIDE BELL EIGHTHS";
                        out.description = bell ? "Ride bell eighths, the snare on 2 and 4, kicks pushing with the chords (the and of 1, 3, the and of 3, the and of 4)"
                                               : "A crash on every beat, the snare on 2 and 4, kicks pushing with the chords (the and of 1, 3, the and of 3, the and of 4)";
                        if (bell)
                        {
                            if (phrase)
                                hit (out, o, GmDrum::crash, 0.82f);
                            for (int k = 0; k < 8; ++k)
                                hit (out, o + k * e, GmDrum::rideBell, k % 2 == 0 ? 0.66f : 0.54f);
                        }
                        else
                            for (int k = 0; k < 4; ++k)
                                hit (out, o + k * q, k == 0 || ! phrase ? GmDrum::crash : GmDrum::china, k == 0 ? 0.84f : 0.7f);
                        if (bar == 0)
                            hit (out, o, GmDrum::kick, 0.9f);
                        for (const auto k : { 2, 8, 10, 14 })
                            hit (out, o + k * s16, GmDrum::kick, 0.84f);
                        if (bar % 4 == 3)
                            fill (o);
                        else
                            backbeat (o);
                        continue;
                    }
                    if (name == "SYNCOPATED KICK, RIDE")
                    {
                        out.description = "The kick on 1, the and of 2, 3 and the and of 3, the ride on the beats, the snare on 2 and 4";
                        if (phrase)
                            hit (out, o, GmDrum::crash, 0.8f);
                        for (int k = 0; k < 4; ++k)
                            hit (out, o + k * q, GmDrum::ride, 0.6f);
                        for (const auto k : { 0, 6, 8, 10 })
                            hit (out, o + k * s16, GmDrum::kick, 0.86f);
                        backbeat (o);
                        continue;
                    }
                    if (name == "DOUBLE KICK" || name == "DOUBLE KICK, HALF TIME")
                    {
                        const auto half = name.endsWith ("HALF TIME");
                        out.description = half ? "Sixteenth kicks, the snare on 3, crashes on 1 and 3" : "Sixteenth kicks, the snare on 2 and 4, a china on every beat";
                        for (int k = 0; k < 16; ++k)
                            hit (out, o + k * s16, GmDrum::kick, k % 4 == 0 ? 0.9f : 0.72f);
                        if (half)
                        {
                            hit (out, o, GmDrum::crash, 0.8f);
                            hit (out, o + 8 * s16, GmDrum::crash, 0.72f);
                            hit (out, o + 8 * s16, GmDrum::snare, 0.92f);
                        }
                        else
                        {
                            for (int k = 0; k < 4; ++k)
                                hit (out, o + k * q, GmDrum::china, 0.64f);
                            backbeat (o);
                        }
                        continue;
                    }
                    if (name == "TOM GROOVE")
                    {
                        out.description = "Toms on the off-sixteenths, the kick on 1 and 3, a crash on each bar's first beat";
                        hit (out, o, GmDrum::crash, phrase ? 0.82f : 0.66f);
                        for (const auto k : { 0, 8 })
                            hit (out, o + k * s16, GmDrum::kick, 0.86f);
                        auto t = 0;
                        for (const auto k : { 2, 6, 10, 14 })
                            hit (out, o + k * s16, t++ % 2 == 0 ? GmDrum::tomHigh : GmDrum::tomLow, 0.7f);
                        continue;
                    }
                    if (name == "ROLLING TOMS")
                    {
                        out.description = "A kick on 1 and a roll falling down the toms over the bar's second half";
                        hit (out, o, GmDrum::kick, 0.8f);
                        const int toms[] { GmDrum::tomHigh, GmDrum::tomHigh, GmDrum::tomMid, GmDrum::tomMid, GmDrum::tomLow, GmDrum::tomLow, GmDrum::tomFloor, GmDrum::tomFloor };
                        for (int k = 0; k < 8; ++k)
                            hit (out, o + (8 + k) * s16, toms[k], 0.5f + 0.04f * (float) k);
                        continue;
                    }
                    if (name == "RIDE ONLY" || name == "RIDE GROOVE")
                    {
                        out.description = name == "RIDE ONLY" ? "Only the ride on the beats" : "The ride on the beats, the kick on 1 and the and of 3, the snare on 2 and 4";
                        for (int k = 0; k < 4; ++k)
                            hit (out, o + k * q, GmDrum::ride, 0.58f);
                        if (name == "RIDE GROOVE")
                        {
                            for (const auto k : { 0, 10 })
                                hit (out, o + k * s16, GmDrum::kick, 0.84f);
                            backbeat (o);
                        }
                        continue;
                    }
                    if (name == "CARRY THE GAP, BUILD")
                    {
                        out.description = "Half time under the slides and the tritone, the ride alone while the guitar is out, the snare building under the scratches";
                        const auto part = airPart (bar);
                        if (part == 2)
                        {
                            for (int k = 0; k < 4; ++k)
                                hit (out, o + k * q, GmDrum::ride, 0.56f);
                            continue;
                        }
                        if (part == 3)
                        {
                            const auto step = bar == bars - 1 || airPart (bar + 1) != 3 ? e : q;
                            for (auto t = 0.0; t < bpb - 1.0e-6; t += step)
                            {
                                hit (out, o + t, GmDrum::snare, 0.6f + 0.3f * (float) (t / bpb));
                                hit (out, o + t, GmDrum::kick, 0.8f);
                            }
                            continue;
                        }
                        if (phrase)
                            hit (out, o, GmDrum::crash, 0.8f);
                        for (int k = 0; k < 4; ++k)
                            hit (out, o + k * q, GmDrum::china, 0.6f);
                        hit (out, o, GmDrum::kick, 0.9f);
                        hit (out, o + 8 * s16, GmDrum::snare, 0.9f);
                        continue;
                    }
                    if (name == "HALF TIME, PUSHED KICKS")
                    {
                        out.description = "Half time: the snare on 3, cymbals on 1 and 3, kicks on 1, the and of 2, 3 and the and of 4";
                        hit (out, o, phrase ? GmDrum::crash : GmDrum::ride, 0.76f);
                        hit (out, o + 8 * s16, GmDrum::ride, 0.66f);
                        for (const auto k : { 0, 6, 8, 14 })
                            hit (out, o + k * s16, GmDrum::kick, 0.86f);
                        hit (out, o + 8 * s16, GmDrum::snare, 0.92f);
                        continue;
                    }
                    if (name == "STRAIGHT BACKBEAT" || name == "DRIVING")
                    {
                        out.description = "The kick on 1 and 3, the snare on 2 and 4, hats on the eighths";
                        if (phrase)
                            hit (out, o, GmDrum::crash, 0.8f);
                        for (int k = 0; k < 8; ++k)
                            hit (out, o + k * e, GmDrum::closedHat, k % 2 == 0 ? 0.58f : 0.44f);
                        for (const auto k : { 0, 8 })
                            hit (out, o + k * s16, GmDrum::kick, 0.88f);
                        backbeat (o);
                        continue;
                    }
                    if (name == "TOMS WITH THE BURST")
                    {
                        out.description = "Toms with the burst, the kick and a crash on the high chord";
                        for (int k = 0; k < 4; ++k)
                            hit (out, o + k * s16, k % 2 == 0 ? GmDrum::tomHigh : GmDrum::tomMid, 0.78f);
                        hit (out, o + 4 * s16, GmDrum::kick, 0.86f);
                        hit (out, o + 8 * s16, GmDrum::kick, 0.9f);
                        hit (out, o + 8 * s16, GmDrum::crash, 0.84f);
                        continue;
                    }
                    if (name == "HITS WITH THE STABS")
                    {
                        out.description = "The kick and a crash on each stab, a snare in the gap";
                        kicksInBar (o, 0.88f);
                        for (const auto t : hits)
                            if (t >= o - 1.0e-6 && t < o + bpb - 1.0e-6 && std::fmod (t - o + 1.0e-6, q) < 2.0e-6)
                                hit (out, t, GmDrum::crash, 0.8f);
                        hit (out, o + 12 * s16, GmDrum::snare, 0.84f);
                        continue;
                    }
                    if (name == "HALF TIME")
                    {
                        out.description = "Half time: the kick on 1, the snare on 3, a china on every beat";
                        if (phrase)
                            hit (out, o, GmDrum::crash, 0.8f);
                        for (int k = 0; k < 4; ++k)
                            hit (out, o + k * q, GmDrum::china, 0.62f);
                        hit (out, o, GmDrum::kick, 0.9f);
                        hit (out, o + 10 * s16, GmDrum::kick, 0.76f);
                        hit (out, o + 8 * s16, GmDrum::snare, 0.9f);
                        continue;
                    }
                    // KICK ON THE PEDAL / KICK WITH THE GUITAR / KICK ON THE CHUGS / KICK ON EVERY CHUG
                    out.description = "The kick on the guitar's low hits, the snare on 2 and 4, a china on every beat";
                    if (phrase)
                        hit (out, o, GmDrum::crash, 0.8f);
                    kicksInBar (o, 0.86f);
                    for (int k = 0; k < 4; ++k)
                        hit (out, o + k * q, GmDrum::china, 0.6f);
                    backbeat (o);
                }
                return out;
            }
        };

        std::optional<LineIdea> interludeLayer (ComposerLayer l, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
        {
            if (spans.empty() || sectionBand (s) != SectionType::Band::interlude || l == ComposerLayer::melody)
                return std::nullopt;
            const Interlude b (s, spans, c);
            if (interludeApproaches (b.type, l).isEmpty())
                return std::nullopt;
            std::optional<LineIdea> idea;
            switch (l)
            {
                case ComposerLayer::guitar:
                    idea = b.guitar();
                    if (c.guitarProfile.has_value())
                        keepInRange (*idea, c.guitarProfile->rangeLow(), c.guitarProfile->rangeHigh());
                    break;
                case ComposerLayer::bass:
                    idea = b.bass();
                    keepInRange (*idea, b.bassLowest(), b.bassHighest());
                    break;
                case ComposerLayer::chords: idea = b.keys(); break;
                case ComposerLayer::lead:
                {
                    idea = b.lead();
                    const auto& profile = c.leadProfile.has_value() ? c.leadProfile : c.guitarProfile;
                    if (idea.has_value() && profile.has_value())
                        keepInRange (*idea, profile->rangeLow(), profile->rangeHigh());
                    break;
                }
                case ComposerLayer::drums: idea = b.drums(); break;
                case ComposerLayer::melody: break;
            }
            if (idea.has_value())
                sortNotes (*idea);
            return idea;
        }

        // (0.97.1) The droning chorus's other basslines, under the guitar's struck chords (a push a beat early takes
        // its chord early): EIGHTHS UNDER THE DRONE (the root in steady eighths, the beats leaning in), OCTAVE PULSE
        // (eighths jumping to the octave and back), WALKING INTO THE CHANGES (the root held, the last beat before a
        // new chord two eighths stepping into it through the key's scale), WHOLE NOTES (the root struck on each bar).
        LineIdea droningBassLine (int approach, const LineIdea& guitar, const std::vector<ChordSpan>& spans, double bpb, int lowest, int highest)
        {
            const auto length = sectionLength (spans);
            const auto top = juce::jmin (highest, lowest + 11);
            const auto strikes = lowStrikes (guitar);
            // The chord sounding at an idea beat: the last strike at or before it decides (a push takes the next chord).
            auto chordAt = [&] (double t) -> const Chord&
            {
                const ChordSpan* chosen = spanAtIdeaBeat (spans, t);
                for (const auto at : strikes)
                    if (at <= t + 1.0e-6)
                        chosen = &chordStruckAt (spans, at);
                return chosen->chord;
            };
            auto rootAt = [&] (double t) { return rootNear (chordAt (t), lowest, top); };
            const auto e = bpb / 8.0;
            LineIdea out { LineIdea::Part::bass, {}, {}, {} };
            switch (approach)
            {
                case 2:
                    out.style = "Droning: eighths under the drone";
                    out.description = "The struck chord's root in steady eighths under the ringing guitar, the beats leaning in";
                    for (int i = 0; i * e < length - 1.0e-6; ++i)
                        addNote (out, i * e, e * 0.9, rootAt (i * e), i % 2 == 0 ? 0.84f : 0.68f, length);
                    break;
                case 3:
                {
                    out.style = "Droning: octave pulse";
                    out.description = "Eighths on the struck chord's root jumping to its octave and back";
                    static const std::array<int, 8> jumps { 0, 12, 0, 12, 0, 12, 0, 12 };
                    for (int i = 0; i * e < length - 1.0e-6; ++i)
                    {
                        const auto root = rootAt (i * e);
                        const auto up = root + jumps[(size_t) (i % 8)] <= highest ? jumps[(size_t) (i % 8)] : 0;
                        addNote (out, i * e, e * 0.9, root + up, i % 2 == 0 ? 0.84f : 0.72f, length);
                    }
                    break;
                }
                case 4:
                {
                    out.style = "Droning: walking into the changes";
                    out.description = "The root held under the drone, the last beat before each new chord two eighths stepping into it";
                    // Where the chord changes (the struck chords' roots), then each stretch held, walked into the next.
                    std::vector<std::pair<double, int>> changes;
                    for (int i = 0; i * e < length - 1.0e-6; ++i)
                        if (const auto root = rootAt (i * e); changes.empty() || changes.back().second != root)
                            changes.push_back ({ i * e, root });
                    const auto key = spans.front().key;
                    for (size_t i = 0; i < changes.size(); ++i)
                    {
                        const auto from = changes[i].first;
                        const auto to = i + 1 < changes.size() ? changes[i + 1].first : length;
                        const auto walk = i + 1 < changes.size() && to - from >= 2.0 * e * 2.0 - 1.0e-6;
                        const auto heldTo = walk ? to - 2.0 * e : to;
                        addNote (out, from, (heldTo - from) * 0.96, changes[i].second, 0.84f, length);
                        if (walk)
                        {
                            const auto next = changes[i + 1].second;
                            const auto dir = next < changes[i].second ? 1 : -1;   // (from above when it falls, from below when it rises)
                            auto step = [&] (int steps)
                            {
                                const auto p = scaleStep (key, next, dir * steps);
                                return p != next ? p : next + dir * steps;
                            };
                            addNote (out, to - 2.0 * e, e * 0.9, juce::jlimit (lowest, highest, step (2)), 0.72f, length);
                            addNote (out, to - e, e * 0.9, juce::jlimit (lowest, highest, step (1)), 0.76f, length);
                        }
                    }
                    break;
                }
                default:
                {
                    out.style = "Droning: whole notes";
                    out.description = "The struck chord's root held, struck again on every bar and at every change";
                    std::vector<double> at;
                    for (auto t = 0.0; t < length - 1.0e-6; t += bpb)
                        at.push_back (t);
                    for (int i = 1; i * e < length - 1.0e-6; ++i)
                        if (rootAt (i * e) != rootAt ((i - 1) * e))
                            at.push_back (i * e);
                    std::sort (at.begin(), at.end());
                    at.erase (std::unique (at.begin(), at.end(), [] (double a, double b) { return b - a < 1.0e-6; }), at.end());
                    at.push_back (length);
                    for (size_t i = 0; i + 1 < at.size(); ++i)
                        addNote (out, at[i], (at[i + 1] - at[i]) * 0.97, rootAt (at[i] + 1.0e-6), 0.82f, length);
                    break;
                }
            }
            sortNotes (out);
            return out;
        }

        // (0.97.2) The chord sounding at an idea beat under a droning guitar: the last strike at or before it decides
        // (a push a beat early takes the next chord); no strikes: the chord track's.
        const Chord& struckChordAt (const std::vector<ChordSpan>& spans, const std::vector<double>& strikes, double t)
        {
            const ChordSpan* chosen = spanAtIdeaBeat (spans, t);
            for (const auto at : strikes)
                if (at <= t + 1.0e-6)
                    chosen = &chordStruckAt (spans, at);
            return chosen->chord;
        }

        // (0.97.2, the owner: more keys under the droning chorus) HELD PADS (each struck chord held softly to the next
        // strike), ARPEGGIO (the struck chord's notes up and down in eighths, high and soft), OCTAVE BELLS (the
        // chord's root with its octave up high on 1 and 3), SILENT.
        LineIdea droningKeysMore (int approach, const std::vector<ChordSpan>& spans, double bpb, const std::vector<double>& strikes)
        {
            const auto length = sectionLength (spans);
            LineIdea out { LineIdea::Part::chords, {}, {}, {} };
            switch (approach)
            {
                case 3:
                {
                    out.style = "Droning: held pads";
                    out.description = "Each chord the guitar strikes held softly under it until the next strike";
                    std::vector<double> at = strikes;
                    if (at.empty())
                        at = cleanSegments (spans, bpb), at.pop_back();
                    at.push_back (length);
                    std::vector<int> previous;
                    for (size_t i = 0; i + 1 < at.size(); ++i)
                    {
                        if (at[i + 1] - at[i] < 1.0e-6)
                            continue;
                        const auto voicings = voicingsFor (struckChordAt (spans, strikes, at[i] + 1.0e-6), 55);
                        if (voicings.empty())
                            continue;
                        const auto& shape = voicings[(size_t) juce::jlimit (0, (int) voicings.size() - 1, closestVoicing (voicings, previous, previous))];
                        previous = shape;
                        for (const auto p : shape)
                            addNote (out, at[i], (at[i + 1] - at[i]) * 0.96, p, 0.42f, length);
                    }
                    break;
                }
                case 4:
                {
                    out.style = "Droning: arpeggio";
                    out.description = "The struck chord's notes up and down in eighths, high and soft over the drone";
                    static const std::array<int, 8> order { 0, 1, 2, 3, 2, 1, 2, 3 };
                    const auto e = bpb / 8.0;
                    std::vector<int> previous;
                    for (int i = 0; i * e < length - 1.0e-6; ++i)
                    {
                        const auto voicings = voicingsFor (struckChordAt (spans, strikes, i * e), 64);
                        if (voicings.empty())
                            continue;
                        auto shape = voicings[(size_t) juce::jlimit (0, (int) voicings.size() - 1, closestVoicing (voicings, previous, previous))];
                        previous = shape;
                        shape.push_back (shape.front() + 12);   // (the root an octave up: four to climb through)
                        addNote (out, i * e, e * 0.9, shape[(size_t) juce::jmin ((int) shape.size() - 1, order[(size_t) (i % 8)])],
                                 i % 2 == 0 ? 0.56f : 0.46f, length);
                    }
                    break;
                }
                case 5:
                {
                    out.style = "Droning: octave bells";
                    out.description = "The struck chord's root with its octave, up high on beats 1 and 3, left to ring";
                    const auto half = bpb / 2.0;
                    for (int i = 0; i * half < length - 1.0e-6; ++i)
                    {
                        const auto root = pitchOfClassIn (struckChordAt (spans, strikes, i * half).root, 72, 83);
                        addNote (out, i * half, half * 0.95, root, i % 2 == 0 ? 0.6f : 0.5f, length);
                        addNote (out, i * half, half * 0.95, root + 12, i % 2 == 0 ? 0.5f : 0.42f, length);
                    }
                    break;
                }
                default:
                    out.style = "Droning: keys out";
                    out.description = "No keys: the ringing guitar on its own";
                    break;
            }
            sortNotes (out);
            return out;
        }

        // (0.97.2, the owner: more drums under the droning chorus) RIDE GROOVE (the ride in eighths with its bell on 1,
        // kick on 1, 3 and the and of 3, snare on 2 and 4), TOM PULSE (the floor tom in eighths, kick on 1, snare on 3,
        // a tom fill into each four bars), DOUBLE KICK (sixteenths on the kick, snare on 2 and 4, china on the beats),
        // CYMBALS ONLY (a crash on every other bar, the ride soft on the beats, the kick on each bar's 1).
        LineIdea droningDrumsMore (int approach, const std::vector<ChordSpan>& spans, double bpb)
        {
            const auto length = sectionLength (spans);
            const auto bars = (int) std::ceil (length / bpb - 1.0e-6);
            const auto beat = bpb / 4.0;
            LineIdea out { LineIdea::Part::drums, {}, {}, {} };
            auto hit = [&out, length] (double at, int note, float velocity)
            {
                if (at >= -1.0e-9 && at < length - 1.0e-6)
                    out.notes.push_back ({ at, 0.1, note, velocity });
            };
            for (int bar = 0; bar < bars; ++bar)
            {
                const auto o = bar * bpb;
                const auto last = bar % 4 == 3;
                switch (approach)
                {
                    case 3:
                        if (bar % 4 == 0)
                            hit (o, 49, 0.88f);
                        for (int e = 0; e < 8; ++e)
                            hit (o + e * beat / 2.0, e == 0 ? 53 : 51, e % 2 == 0 ? 0.76f : 0.58f);
                        hit (o, 36, 0.95f);
                        hit (o + 2 * beat, 36, 0.88f);
                        hit (o + 2.5 * beat, 36, 0.8f);
                        hit (o + beat, 38, 0.92f);
                        hit (o + 3 * beat, 38, 0.92f);
                        break;
                    case 4:
                        if (bar % 4 == 0)
                            hit (o, 49, 0.85f);
                        for (int e = 0; e < (last ? 6 : 8); ++e)
                            hit (o + e * beat / 2.0, 41, e % 2 == 0 ? 0.86f : 0.62f);
                        hit (o, 36, 0.95f);
                        hit (o + 2 * beat, 38, 0.92f);
                        if (last)
                            for (int s = 0; s < 4; ++s)
                                hit (o + 3 * beat + s * beat / 4.0, s < 2 ? 48 : 45, 0.74f + 0.05f * (float) s);
                        break;
                    case 5:
                        if (bar % 4 == 0)
                            hit (o, 49, 0.9f);
                        for (int q = 0; q < 4; ++q)
                            hit (o + q * beat, 52, q == 0 ? 0.82f : 0.7f);
                        for (int s16 = 0; s16 < 16; ++s16)
                            hit (o + s16 * beat / 4.0, 36, s16 % 4 == 0 ? 0.92f : 0.76f);
                        hit (o + beat, 38, 0.95f);
                        hit (o + 3 * beat, 38, 0.95f);
                        break;
                    default:
                        if (bar % 2 == 0)
                            hit (o, 49, 0.82f);
                        for (int q = 0; q < 4; ++q)
                            hit (o + q * beat, 51, q == 0 ? 0.6f : 0.45f);
                        hit (o, 36, 0.86f);
                        break;
                }
            }
            out.style = approach == 3 ? "Droning: ride groove" : approach == 4 ? "Droning: tom pulse"
                      : approach == 5 ? "Droning: double kick" : "Droning: cymbals only";
            out.description = approach == 3 ? "The ride in eighths with its bell on 1, kick on 1, 3 and the and of 3, snare on 2 and 4"
                            : approach == 4 ? "The floor tom in eighths, kick on 1, snare on 3, a tom fill into each four bars"
                            : approach == 5 ? "Sixteenths on the kick under the drone, snare on 2 and 4, the china on every beat"
                                            : "A crash every other bar, the ride soft on the beats, the kick on each bar's 1";
            sortNotes (out);
            return out;
        }

        // The droning chorus's part for a lane (none: the lane plays as usual: the melody, and the guitar itself).
        std::optional<LineIdea> droningLayer (ComposerLayer l, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
        {
            if (spans.empty() || ! droningBand (s, c))
                return std::nullopt;
            const auto& lane = s.lane (l);
            const auto guitar = guitarOf (s, spans, c);
            const auto strikes = lowStrikes (guitar);
            switch (l)
            {
                case ComposerLayer::bass:
                {
                    const auto& profile = c.bassProfile;
                    const auto lowest = profile.has_value() ? profile->rangeLow() : c.bassTuning.lowest();
                    const auto highest = profile.has_value() ? profile->rangeHigh() : bassTop (c.bassTuning);
                    // (0.97.1, the owner: more than one bassline) chosen, else by the take between the first two.
                    const auto approach = lane.approach >= 0 ? lane.approach % 6 : lane.variation % 2;
                    auto idea = approach >= 2 ? droningBassLine (approach, guitar, spans, c.beatsPerBar, lowest, highest)
                                              : droningBass (guitar, spans, lowest, highest);
                    if (approach == 1)   // (0.94.0: chosen, else by the take)
                    {
                        pulseLoopEnds (idea, c.beatsPerBar, sectionLength (spans));
                        idea.description += "; the last bar of every eight pulsed in eighths";
                    }
                    if (lane.lowString)
                        keepInRange (idea, lowest, lowest + 12);
                    keepInRange (idea, lowest, highest);
                    return idea;
                }
                case ComposerLayer::chords: return droningKeys (lane, spans, c.beatsPerBar, strikes);
                case ComposerLayer::lead:
                {
                    const auto& profile = c.leadProfile.has_value() ? c.leadProfile : c.guitarProfile;
                    const auto velocities = profile.has_value() && profile->velocities.has_value() ? *profile->velocities : c.velocities;
                    const auto lowest = profile.has_value() ? profile->rangeLow() : c.tuning.lowest();
                    const auto highest = profile.has_value() ? profile->rangeHigh() : c.tuning.highest();
                    return droningLead (lane, spans, c.beatsPerBar, velocities, lowest, highest);
                }
                case ComposerLayer::drums: return droningDrums (lane, spans, c.beatsPerBar, strikes);
                case ComposerLayer::melody:
                case ComposerLayer::guitar: break;
            }
            return std::nullopt;
        }

        // THE DRIVING CHORUS'S BAND (0.98.0, the owner's This Fire Burns and Faster choruses, read for their shapes):
        //   BASS    root eighths; doubling the guitar; eighths walking up the scale into each chord change (This Fire
        //           Burns); root and octave eighths; LOCKED WITH THE KICK (syncedLayer).
        //   DRUMS   a driving backbeat (the ride in eighths, kick on 1, 3 and the and of 3); half time with the kick
        //           on every eighth and the snare on 3 (This Fire Burns); cymbals on the beats with the kicks between
        //           (Faster); the kick on the guitar's hits; double kick. A crash on each four bars, a fill into them.
        //   KEYS    held pads; silent for two bars, then answering in bars 3 and 4 (Faster); a high eighth pulse; silent.
        //   LEAD    a ringing arpeggio up high (This Fire Burns); held notes with a pinch-harmonic squeal at the end of
        //           each four bars (This Fire Burns); the rhythm guitar doubled an octave up (Faster's verse).
        // Left on AUTOMATIC each takes one of its first options by the take (the bass four, drums and keys three).
        bool drivingBand (const ComposerState& s, const ComposerContext& c)
        {
            const auto& flavours = riffFlavours();
            return juce::isPositiveAndBelow (s.riffFlavour, (int) flavours.size())
                && flavours[(size_t) s.riffFlavour] == RiffFlavour::drivingEighths
                && ! s.writtenRiffs && ! c.myRiff.has_value() && s.lane (ComposerLayer::guitar).on;
        }

        int drivingApproach (const ComposerLane& lane, int count, int automatic)
        {
            return lane.approach >= 0 ? lane.approach % count : lane.variation % automatic;
        }

        LineIdea drivingBass (int approach, const LineIdea& guitar, const std::vector<ChordSpan>& spans, double bpb, int lowest, int highest)
        {
            if (approach == 1)
            {
                auto idea = doubledLine (guitar, spans, lowest, highest, "Guitar");
                idea.style = "Driving: doubles the guitar";
                idea.description = "The rhythm guitar's hits, note for note, low";
                return idea;
            }
            static const char* const names[] = { "Driving: root eighths", "", "Driving: walking into the changes", "Driving: octave eighths" };
            static const char* const what[] = { "The chord's root on every eighth under the guitar", "",
                                                "Root eighths, the last three eighths before each change walking up the scale into it",
                                                "The root and its octave in turn on every eighth" };
            LineIdea out { LineIdea::Part::bass, names[approach], what[approach], {} };
            const auto length = sectionLength (spans);
            const auto eighth = bpb / 8.0;
            const auto origin = spans.front().start;
            for (size_t i = 0; i < spans.size(); ++i)
            {
                const auto& span = spans[i];
                const auto from = span.start - origin, to = span.end - origin;
                const auto root = rootNear (span.chord, lowest, juce::jmin (highest, lowest + 11));
                // The walk: into the next chord when it has another root (the last chord walks into the first).
                const auto& next = i + 1 < spans.size() ? spans[i + 1] : spans.front();
                const auto walks = approach == 2 && next.chord.root != span.chord.root && to - from >= bpb - 1.0e-6;
                const auto target = rootNear (next.chord, lowest, juce::jmin (highest, lowest + 11));
                auto k = 0;
                for (auto t = from; t < to - 1.0e-6; t += eighth, ++k)
                {
                    auto pitch = root;
                    if (approach == 3 && k % 2 == 1 && root + 12 <= highest)
                        pitch = root + 12;
                    const auto left = (int) std::round ((to - t) / eighth);
                    if (walks && left <= 3 && left >= 1)
                    {
                        pitch = scaleStep (next.key, target, -left);
                        if (pitch < lowest)
                            pitch += 12;
                    }
                    const auto onBeat = k % 2 == 0;
                    addNote (out, t, eighth * 0.9, pitch, onBeat ? 0.86f : 0.74f, juce::jmin (to, length));
                }
            }
            return out;
        }

        LineIdea drivingDrums (int approach, const LineIdea& guitar, const std::vector<ChordSpan>& spans, double bpb)
        {
            static const char* const names[] = { "Driving: backbeat", "Driving: half time, kick on the eighths", "Driving: cymbals on the beats",
                                                 "Driving: kick with the guitar", "Driving: double kick" };
            static const char* const what[] = { "The ride in eighths, kick on 1, 3 and the and of 3, snare on 2 and 4",
                                                "Half time under the eighths: the kick on every eighth, the snare on 3, the china on the beats",
                                                "A crash on every beat, the kick on the ands between them, snare on 2 and 4",
                                                "The kick on every hit of the rhythm guitar, the snare on 2 and 4, hats in eighths",
                                                "Sixteenths on the kick, the snare on 2 and 4, the china on the beats" };
            LineIdea out { LineIdea::Part::drums, names[approach], what[approach], {} };
            const auto length = sectionLength (spans);
            const auto bars = (int) std::ceil (length / bpb - 1.0e-6);
            const auto beat = bpb / 4.0;
            auto hit = [&out, length] (double at, int note, float velocity)
            {
                if (at >= -1.0e-9 && at < length - 1.0e-6
                    && std::none_of (out.notes.begin(), out.notes.end(), [&] (const LineNote& n) { return n.pitch == note && std::abs (n.beat - at) < 1.0e-6; }))
                    out.notes.push_back ({ at, 0.1, note, velocity });
            };
            const auto guitarHits = approach == 3 ? riffHits (guitar) : std::vector<double> {};
            for (int bar = 0; bar < bars; ++bar)
            {
                const auto o = bar * bpb;
                const auto fill = bar % 4 == 3;
                // The last beat of every four bars a fill (snare, then down the toms), its kick left in.
                const auto playsUntil = fill ? o + 3 * beat : o + bpb;
                const auto inBar = [&] (double t) { return t < playsUntil - 1.0e-6; };
                if (bar % 4 == 0)
                {
                    hit (o, GmDrum::crash, 0.95f);
                    hit (o, GmDrum::kick, 0.95f);
                }
                switch (approach)
                {
                    case 0:
                        for (int e = 0; e < 8; ++e)
                            if (inBar (o + e * beat / 2))
                                hit (o + e * beat / 2, GmDrum::ride, e % 2 == 0 ? 0.72f : 0.58f);
                        for (const auto k : { 0.0, 2.0, 2.5 })
                            if (inBar (o + k * beat))
                                hit (o + k * beat, GmDrum::kick, 0.9f);
                        for (const auto sn : { 1.0, 3.0 })
                            if (inBar (o + sn * beat))
                                hit (o + sn * beat, GmDrum::snare, 0.92f);
                        break;
                    case 1:
                        for (int e = 0; e < 8; ++e)
                            if (inBar (o + e * beat / 2))
                                hit (o + e * beat / 2, GmDrum::kick, e % 2 == 0 ? 0.9f : 0.8f);
                        for (int q = 0; q < 4; ++q)
                            if (inBar (o + q * beat))
                                hit (o + q * beat, GmDrum::china, q == 0 ? 0.8f : 0.66f);
                        hit (o + 2 * beat, GmDrum::snare, 0.95f);
                        break;
                    case 2:
                        for (int q = 0; q < 4; ++q)
                            if (inBar (o + q * beat))
                                hit (o + q * beat, GmDrum::crash2, q == 0 ? 0.82f : 0.7f);
                        hit (o, GmDrum::kick, 0.92f);
                        for (int q = 0; q < 4; ++q)
                            if (inBar (o + (q + 0.5) * beat))
                                hit (o + (q + 0.5) * beat, GmDrum::kick, 0.86f);
                        for (const auto sn : { 1.0, 3.0 })
                            if (inBar (o + sn * beat))
                                hit (o + sn * beat, GmDrum::snare, 0.92f);
                        break;
                    case 3:
                        for (int e = 0; e < 8; ++e)
                            if (inBar (o + e * beat / 2))
                                hit (o + e * beat / 2, GmDrum::closedHat, e % 2 == 0 ? 0.7f : 0.55f);
                        for (const auto t : guitarHits)
                            if (t >= o - 1.0e-6 && inBar (t))
                                hit (t, GmDrum::kick, 0.88f);
                        hit (o, GmDrum::kick, 0.92f);
                        for (const auto sn : { 1.0, 3.0 })
                            if (inBar (o + sn * beat))
                                hit (o + sn * beat, GmDrum::snare, 0.92f);
                        break;
                    default:
                        for (int x = 0; x < 16; ++x)
                            if (inBar (o + x * beat / 4))
                                hit (o + x * beat / 4, GmDrum::kick, x % 4 == 0 ? 0.9f : 0.76f);
                        for (int q = 0; q < 4; ++q)
                            if (inBar (o + q * beat))
                                hit (o + q * beat, GmDrum::china, q == 0 ? 0.8f : 0.66f);
                        for (const auto sn : { 1.0, 3.0 })
                            if (inBar (o + sn * beat))
                                hit (o + sn * beat, GmDrum::snare, 0.92f);
                        break;
                }
                if (fill)
                {
                    const auto f = o + 3 * beat;
                    hit (f, GmDrum::snare, 0.8f);
                    hit (f + beat * 0.25, GmDrum::snare, 0.84f);
                    hit (f + beat * 0.5, GmDrum::tomHigh, 0.88f);
                    hit (f + beat * 0.75, GmDrum::tomFloor, 0.92f);
                    hit (f, GmDrum::kick, 0.85f);
                }
            }
            std::stable_sort (out.notes.begin(), out.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
            return out;
        }

        LineIdea drivingKeys (int approach, const std::vector<ChordSpan>& spans, double bpb)
        {
            static const char* const names[] = { "Driving: held pads", "Driving: answers in bars 3 and 4", "Driving: high eighth pulse", "Driving: silent" };
            static const char* const what[] = { "Each chord held softly under the guitars", "Two bars out, then a short figure on the chord's notes answering in bars 3 and 4",
                                                "The chord up high in soft eighths", "The keys sit this chorus out" };
            LineIdea out { LineIdea::Part::chords, names[approach], what[approach], {} };
            if (approach == 3)
                return out;
            const auto length = sectionLength (spans);
            const auto segments = cleanSegments (spans, bpb);
            std::vector<int> previous;
            for (size_t i = 0; i + 1 < segments.size(); ++i)
            {
                const auto from = segments[i], to = segments[i + 1];
                const auto& chord = chordAtIdea (spans, from);
                const auto voicings = voicingsFor (chord, approach == 2 ? 67 : approach == 1 ? 72 : 57);
                if (voicings.empty())
                    continue;
                const auto& shape = voicings[(size_t) juce::jlimit (0, (int) voicings.size() - 1, closestVoicing (voicings, previous, previous))];
                previous = shape;
                const auto bar = (int) std::floor (from / bpb + 1.0e-9);
                if (approach == 0)
                    for (const auto p : shape)
                        addNote (out, from, (to - from) * 0.96, p, 0.5f, length);
                else if (approach == 2)
                    for (auto t = from; t < to - 1.0e-6; t += bpb / 8.0)
                        for (const auto p : shape)
                            addNote (out, t, bpb / 8.0 * 0.8, p, std::abs (std::fmod (t, bpb / 4.0)) < 1.0e-6 ? 0.46f : 0.38f, to);
                else if (bar % 4 >= 2)
                {
                    // The answer: the chord's notes from the top down and back, a bar each, the last held.
                    auto tones = shape;
                    std::sort (tones.begin(), tones.end());
                    const auto top = (int) tones.size() - 1;
                    static const double at[] = { 0.0, 1.0, 1.5, 2.5 };
                    static const double held[] = { 0.9, 0.45, 0.9, 1.4 };
                    const int order[2][4] = { { top, top - 1, 0, top - 1 }, { 0, top - 1, top, top } };
                    const auto o = bar * bpb;
                    for (int k = 0; k < 4; ++k)
                    {
                        const auto t = o + at[k] * bpb / 4.0;
                        if (t >= from - 1.0e-6 && t < to - 1.0e-6)
                            addNote (out, t, held[k] * bpb / 4.0, tones[(size_t) juce::jlimit (0, top, order[bar % 2][k])], 0.62f, to);
                    }
                }
            }
            sortNotes (out);
            return out;
        }

        LineIdea drivingLead (int approach, const LineIdea& guitar, const std::vector<ChordSpan>& spans, double bpb,
                              const GuitarVelocities& velocities, int lowest, int highest)
        {
            static const char* const names[] = { "Driving: ringing arpeggio", "Driving: held notes and a squeal", "Driving: doubles the rhythm" };
            static const char* const what[] = { "The chord's notes picked up high in eighths, each left to ring",
                                                "A chord note held each bar with vibrato, a pinch-harmonic squeal at the end of every four bars",
                                                "The rhythm guitar's hits an octave up, one note each" };
            LineIdea out { LineIdea::Part::lead, names[approach], what[approach], {} };
            const auto length = sectionLength (spans);
            const auto low = juce::jlimit (lowest, juce::jmax (lowest, highest - 20), 59);
            if (approach == 2)
            {
                std::vector<LineNote> notes;
                for (const auto& n : guitar.notes)
                    if (! n.keyswitch)
                        notes.push_back (n);
                std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
                for (size_t i = 0; i < notes.size();)
                {
                    auto j = i;
                    auto lowNote = notes[i];
                    while (j < notes.size() && notes[j].beat - notes[i].beat < HumanizeLimits::together)
                    {
                        if (notes[j].pitch < lowNote.pitch)
                            lowNote = notes[j];
                        ++j;
                    }
                    auto pitch = lowNote.pitch + 12;
                    while (pitch < low)
                        pitch += 12;
                    while (pitch > highest)
                        pitch -= 12;
                    lowNote.pitch = pitch;
                    lowNote.effect = LineNote::Effect::none;
                    out.notes.push_back (lowNote);
                    i = j;
                }
                return out;
            }
            const auto sustain = GuitarVelocity::of (velocities.sustain);
            const auto origin = spans.front().start;
            if (approach == 0)
            {
                for (const auto& span : spans)
                {
                    const auto root = pitchOfClassIn (span.chord.root, low, low + 11);
                    const auto& tones = span.chord.pitchClasses();
                    const auto third = tones.size() >= 2 ? pitchOfClassIn (tones[1], root + 12, root + 23) : root + 12;
                    const auto fifth = tones.size() >= 3 ? pitchOfClassIn (tones[2], root, root + 11) : root + 7;
                    const int arp[] = { root, fifth, root + 12, third, root + 12, fifth };
                    auto k = 0;
                    for (auto t = span.start - origin; t < span.end - origin - 1.0e-6; t += bpb / 8.0, ++k)
                    {
                        auto p = arp[k % 6];
                        while (p > highest)
                            p -= 12;
                        LineNote n { t, juce::jmin (bpb / 4.0, span.end - origin - t), p, k % 2 == 0 ? sustain : GuitarVelocity::of (velocities.sustainSoft) };
                        n.effect = LineNote::Effect::ring;
                        out.notes.push_back (n);
                    }
                }
                return out;
            }
            // Held notes: each bar the chord's third (or fifth every other bar) held with vibrato; every fourth bar
            // held three beats, then the squeal: the root two octaves over the riff, a pinch harmonic bent up.
            const auto bars = (int) std::ceil (length / bpb - 1.0e-6);
            for (int bar = 0; bar < bars; ++bar)
            {
                const auto o = bar * bpb;
                const auto& chord = chordAtIdea (spans, o);
                const auto& tones = chord.pitchClasses();
                const auto pc = tones.size() >= 3 ? tones[(size_t) (bar % 2 == 0 ? 1 : 2)] : chord.root;
                auto pitch = pitchOfClassIn (pc, low + 7, low + 18);
                while (pitch > highest)
                    pitch -= 12;
                const auto squeal = bar % 4 == 3;
                LineNote held { o, (squeal ? 3.0 : 3.9) * bpb / 4.0, pitch, sustain };
                held.effect = LineNote::Effect::vibrato;
                if (o < length - 1.0e-6)
                {
                    held.length = juce::jmin (held.length, length - o);
                    out.notes.push_back (held);
                }
                if (squeal && o + 3 * bpb / 4.0 < length - 1.0e-6)
                {
                    auto top = pitchOfClassIn (chord.root, low + 19, low + 30);
                    while (top > highest)
                        top -= 12;
                    LineNote n { o + 3 * bpb / 4.0, juce::jmin (bpb / 4.0 * 0.95, length - o - 3 * bpb / 4.0), top, GuitarVelocity::of (velocities.pinch) };
                    n.effect = LineNote::Effect::bend;
                    n.amount = 2.0f;
                    out.notes.push_back (n);
                }
            }
            return out;
        }

        // The driving chorus's part for a lane (none: the lane plays as usual: the melody, and the guitar itself).
        std::optional<LineIdea> drivingLayer (ComposerLayer l, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
        {
            if (spans.empty() || ! drivingBand (s, c))
                return std::nullopt;
            const auto& lane = s.lane (l);
            switch (l)
            {
                case ComposerLayer::bass:
                {
                    const auto& profile = c.bassProfile;
                    const auto lowest = profile.has_value() ? profile->rangeLow() : c.bassTuning.lowest();
                    const auto highest = profile.has_value() ? profile->rangeHigh() : bassTop (c.bassTuning);
                    auto idea = drivingBass (drivingApproach (lane, 4, 4), guitarOf (s, spans, c), spans, c.beatsPerBar, lowest, highest);
                    if (lane.lowString)
                        keepInRange (idea, lowest, lowest + 12);
                    keepInRange (idea, lowest, highest);
                    return idea;
                }
                case ComposerLayer::chords: return drivingKeys (drivingApproach (lane, 4, 3), spans, c.beatsPerBar);
                case ComposerLayer::lead:
                {
                    const auto& profile = c.leadProfile.has_value() ? c.leadProfile : c.guitarProfile;
                    const auto velocities = profile.has_value() && profile->velocities.has_value() ? *profile->velocities : c.velocities;
                    const auto lowest = profile.has_value() ? profile->rangeLow() : c.tuning.lowest();
                    const auto highest = profile.has_value() ? profile->rangeHigh() : c.tuning.highest();
                    return drivingLead (drivingApproach (lane, 3, 3), guitarOf (s, spans, c), spans, c.beatsPerBar, velocities, lowest, highest);
                }
                case ComposerLayer::drums: return drivingDrums (drivingApproach (lane, 5, 3), guitarOf (s, spans, c), spans, c.beatsPerBar);
                case ComposerLayer::melody:
                case ComposerLayer::guitar: break;
            }
            return std::nullopt;
        }

        // SYNCOPATED DRIVE'S BAND (0.99.0, the owner's Another Life chorus with Faster's syncopated bar; shapes only):
        //   BASS    locked with the kick in a syncopated figure: 1, the and of 2 and 4 tied over the bar line, then a bar
        //           of off-beats (Another Life); root eighths under it (Faster); doubling the guitar; LOCKED WITH THE KICK.
        //   DRUMS   the kick on that figure with the snare on 3 (half time) or on 2 and 4; the kick on the guitar's hits;
        //           cymbals on the beats with the kicks between. A crash on each four bars, a fill into them.
        //   KEYS    held pads; stabs on the bass's figure; silent.
        //   LEAD    three notes cycling against the four beats (Another Life); held notes with a squeal; a ringing arpeggio.
        // Left on AUTOMATIC: the bass among its first three, the drums between their first two (the figure), the keys
        // between the first two, the lead among its three, by the take.
        bool syncopatedBand (const ComposerState& s, const ComposerContext& c)
        {
            const auto& flavours = riffFlavours();
            return juce::isPositiveAndBelow (s.riffFlavour, (int) flavours.size())
                && flavours[(size_t) s.riffFlavour] == RiffFlavour::syncopatedDrive
                && ! s.writtenRiffs && ! c.myRiff.has_value() && s.lane (ComposerLayer::guitar).on;
        }

        // The figure's hits (idea beats) with how long each rings: bar A on 1, the and of 2 and 4 (held over the bar line
        // to the next hit), bar B on every off-beat.
        std::vector<std::pair<double, double>> syncopatedFigure (const std::vector<ChordSpan>& spans, double bpb)
        {
            const auto length = sectionLength (spans);
            const auto beat = bpb / 4.0;
            std::vector<double> at;
            for (int bar = 0; bar * bpb < length - 1.0e-6; ++bar)
            {
                const auto o = bar * bpb;
                if (bar % 2 == 0)
                    for (const auto b : { 0.0, 1.5, 3.0 })
                        at.push_back (o + b * beat);
                else
                    for (const auto b : { 0.5, 1.5, 2.5, 3.5 })
                        at.push_back (o + b * beat);
            }
            std::vector<std::pair<double, double>> out;
            for (size_t i = 0; i < at.size(); ++i)
                if (at[i] < length - 1.0e-6)
                {
                    const auto next = i + 1 < at.size() ? juce::jmin (at[i + 1], length) : length;
                    out.push_back ({ at[i], juce::jmax (0.05, (next - at[i]) * 0.92) });
                }
            return out;
        }

        LineIdea syncopatedBass (const std::vector<ChordSpan>& spans, double bpb, int lowest, int highest)
        {
            LineIdea out { LineIdea::Part::bass, "Syncopated: locked figure",
                           "With the kick: 1, the and of 2 and 4 tied over the bar line, then a bar of off-beats", {} };
            const auto length = sectionLength (spans);
            for (const auto& [t, held] : syncopatedFigure (spans, bpb))
                addNote (out, t, held, rootNear (chordAtIdea (spans, t), lowest, juce::jmin (highest, lowest + 11)), 0.88f, length);
            return out;
        }

        LineIdea syncopatedDrums (int approach, const LineIdea& guitar, const std::vector<ChordSpan>& spans, double bpb)
        {
            if (approach == 2)
                return drivingDrums (3, guitar, spans, bpb);
            if (approach == 3)
                return drivingDrums (2, guitar, spans, bpb);
            const auto halfTime = approach == 0;
            LineIdea out { LineIdea::Part::drums, halfTime ? "Syncopated: kick figure, half-time snare" : "Syncopated: kick figure, backbeat",
                           halfTime ? "The kick on the bass's syncopated figure, the snare on 3, open hats on the beats"
                                    : "The kick on the bass's syncopated figure, the snare on 2 and 4, the ride in eighths", {} };
            const auto length = sectionLength (spans);
            const auto bars = (int) std::ceil (length / bpb - 1.0e-6);
            const auto beat = bpb / 4.0;
            auto hit = [&out, length] (double at, int note, float velocity)
            {
                if (at >= -1.0e-9 && at < length - 1.0e-6
                    && std::none_of (out.notes.begin(), out.notes.end(), [&] (const LineNote& n) { return n.pitch == note && std::abs (n.beat - at) < 1.0e-6; }))
                    out.notes.push_back ({ at, 0.1, note, velocity });
            };
            for (const auto& [t, held] : syncopatedFigure (spans, bpb))
            {
                juce::ignoreUnused (held);
                if ((int) std::floor (t / bpb + 1.0e-9) % 4 != 3 || std::fmod (t, bpb) < 3 * beat - 1.0e-6)
                    hit (t, GmDrum::kick, 0.92f);
            }
            for (int bar = 0; bar < bars; ++bar)
            {
                const auto o = bar * bpb;
                const auto fill = bar % 4 == 3;
                const auto until = fill ? o + 3 * beat : o + bpb;
                if (bar % 4 == 0)
                    hit (o, GmDrum::crash, 0.95f);
                if (halfTime)
                {
                    for (int q = 0; q < 4; ++q)
                        if (o + q * beat < until - 1.0e-6)
                            hit (o + q * beat, GmDrum::openHat, q == 0 ? 0.78f : 0.66f);
                    hit (o + 2 * beat, GmDrum::snare, 0.95f);
                }
                else
                {
                    for (int e = 0; e < 8; ++e)
                        if (o + e * beat / 2 < until - 1.0e-6)
                            hit (o + e * beat / 2, GmDrum::ride, e % 2 == 0 ? 0.72f : 0.58f);
                    hit (o + beat, GmDrum::snare, 0.92f);
                    if (! fill)
                        hit (o + 3 * beat, GmDrum::snare, 0.92f);
                }
                if (fill)
                {
                    const auto f = o + 3 * beat;
                    hit (f, GmDrum::snare, 0.8f);
                    hit (f + beat * 0.25, GmDrum::snare, 0.84f);
                    hit (f + beat * 0.5, GmDrum::tomHigh, 0.88f);
                    hit (f + beat * 0.75, GmDrum::tomFloor, 0.92f);
                }
            }
            std::stable_sort (out.notes.begin(), out.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
            return out;
        }

        LineIdea syncopatedKeys (int approach, const std::vector<ChordSpan>& spans, double bpb)
        {
            if (approach == 0)
                return drivingKeys (0, spans, bpb);
            if (approach == 2)
                return drivingKeys (3, spans, bpb);
            LineIdea out { LineIdea::Part::chords, "Syncopated: stabs with the bass", "The chord stabbed short on the bass and kick's figure", {} };
            const auto length = sectionLength (spans);
            std::vector<int> previous;
            for (const auto& [t, held] : syncopatedFigure (spans, bpb))
            {
                juce::ignoreUnused (held);
                const auto voicings = voicingsFor (chordAtIdea (spans, t), 60);
                if (voicings.empty())
                    continue;
                const auto& shape = voicings[(size_t) juce::jlimit (0, (int) voicings.size() - 1, closestVoicing (voicings, previous, previous))];
                previous = shape;
                for (const auto p : shape)
                    addNote (out, t, bpb / 8.0 * 0.9, p, 0.6f, length);
            }
            sortNotes (out);
            return out;
        }

        LineIdea syncopatedLead (int approach, const LineIdea& guitar, const std::vector<ChordSpan>& spans, double bpb,
                                 const GuitarVelocities& velocities, int lowest, int highest)
        {
            if (approach == 1)
                return drivingLead (1, guitar, spans, bpb, velocities, lowest, highest);
            if (approach == 2)
                return drivingLead (0, guitar, spans, bpb, velocities, lowest, highest);
            // Three notes cycling in eighths against the four beats: the accent lands somewhere else each beat.
            LineIdea out { LineIdea::Part::lead, "Syncopated: three against four",
                           "Three of the chord's notes cycling in eighths, so the figure crosses the beats", {} };
            const auto low = juce::jlimit (lowest, juce::jmax (lowest, highest - 20), 62);
            const auto origin = spans.front().start;
            auto k = 0;
            for (const auto& span : spans)
            {
                const auto& tones = span.chord.pitchClasses();
                const auto root = pitchOfClassIn (span.chord.root, low, low + 11);
                const auto fifth = tones.size() >= 3 ? pitchOfClassIn (tones[2], root, root + 11) : root + 7;
                const auto third = tones.size() >= 2 ? pitchOfClassIn (tones[1], root + 12, root + 23) : root + 12;
                const int cycle[] = { root + 12, fifth, third };
                for (auto t = span.start - origin; t < span.end - origin - 1.0e-6; t += bpb / 8.0, ++k)
                {
                    auto p = cycle[k % 3];
                    while (p > highest)
                        p -= 12;
                    const auto first = k % 3 == 0;
                    out.notes.push_back ({ t, juce::jmin (bpb / 8.0 * 0.92, span.end - origin - t), p,
                                           GuitarVelocity::of (first ? velocities.sustainHard : velocities.sustain) });
                }
            }
            return out;
        }

        std::optional<LineIdea> syncopatedLayer (ComposerLayer l, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
        {
            if (spans.empty() || ! syncopatedBand (s, c))
                return std::nullopt;
            const auto& lane = s.lane (l);
            switch (l)
            {
                case ComposerLayer::bass:
                {
                    const auto& profile = c.bassProfile;
                    const auto lowest = profile.has_value() ? profile->rangeLow() : c.bassTuning.lowest();
                    const auto highest = profile.has_value() ? profile->rangeHigh() : bassTop (c.bassTuning);
                    const auto approach = drivingApproach (lane, 3, 3);
                    auto idea = approach == 0 ? syncopatedBass (spans, c.beatsPerBar, lowest, highest)
                                              : drivingBass (approach == 1 ? 0 : 1, guitarOf (s, spans, c), spans, c.beatsPerBar, lowest, highest);
                    if (lane.lowString)
                        keepInRange (idea, lowest, lowest + 12);
                    keepInRange (idea, lowest, highest);
                    return idea;
                }
                case ComposerLayer::chords: return syncopatedKeys (drivingApproach (lane, 3, 2), spans, c.beatsPerBar);
                case ComposerLayer::lead:
                {
                    const auto& profile = c.leadProfile.has_value() ? c.leadProfile : c.guitarProfile;
                    const auto velocities = profile.has_value() && profile->velocities.has_value() ? *profile->velocities : c.velocities;
                    const auto lowest = profile.has_value() ? profile->rangeLow() : c.tuning.lowest();
                    const auto highest = profile.has_value() ? profile->rangeHigh() : c.tuning.highest();
                    return syncopatedLead (drivingApproach (lane, 3, 3), guitarOf (s, spans, c), spans, c.beatsPerBar, velocities, lowest, highest);
                }
                case ComposerLayer::drums: return syncopatedDrums (drivingApproach (lane, 4, 2), guitarOf (s, spans, c), spans, c.beatsPerBar);
                case ComposerLayer::melody:
                case ComposerLayer::guitar: break;
            }
            return std::nullopt;
        }

        // MUTED DRIVE'S BAND (0.100.0, the owner's Faster verse; shapes only): the bass on the roots in eighths, doubling
        // the guitar, or on the beats; a plain backbeat with closed hats in eighths, a driving kick, or half time; the
        // keys out or held low; the lead doubling the rhythm an octave up (Faster's), or out.
        bool mutedBand (const ComposerState& s, const ComposerContext& c)
        {
            const auto& flavours = riffFlavours();
            return juce::isPositiveAndBelow (s.riffFlavour, (int) flavours.size())
                && flavours[(size_t) s.riffFlavour] == RiffFlavour::mutedDrive
                && ! s.writtenRiffs && ! c.myRiff.has_value() && s.lane (ComposerLayer::guitar).on;
        }

        LineIdea mutedDrums (int approach, const std::vector<ChordSpan>& spans, double bpb)
        {
            static const char* const names[] = { "Muted: plain backbeat", "Muted: driving kick", "Muted: half time" };
            static const char* const what[] = { "Closed hats in eighths, kick on 1 and 3, snare on 2 and 4",
                                                "Closed hats in eighths, the kick on 1, the and of 1, 3 and the and of 3, snare on 2 and 4",
                                                "Closed hats in eighths, kick on 1 and the and of 2, the snare on 3" };
            LineIdea out { LineIdea::Part::drums, names[approach], what[approach], {} };
            const auto length = sectionLength (spans);
            const auto bars = (int) std::ceil (length / bpb - 1.0e-6);
            const auto beat = bpb / 4.0;
            auto hit = [&out, length] (double at, int note, float velocity)
            {
                if (at >= -1.0e-9 && at < length - 1.0e-6
                    && std::none_of (out.notes.begin(), out.notes.end(), [&] (const LineNote& n) { return n.pitch == note && std::abs (n.beat - at) < 1.0e-6; }))
                    out.notes.push_back ({ at, 0.1, note, velocity });
            };
            for (int bar = 0; bar < bars; ++bar)
            {
                const auto o = bar * bpb;
                const auto fill = bar % 4 == 3;
                const auto until = fill ? o + 3 * beat : o + bpb;
                if (bar % 4 == 0)
                    hit (o, GmDrum::crash, 0.86f);
                for (int e = 0; e < 8; ++e)
                    if (o + e * beat / 2 < until - 1.0e-6)
                        hit (o + e * beat / 2, GmDrum::closedHat, e % 2 == 0 ? 0.66f : 0.5f);
                const std::vector<double> kicks = approach == 0 ? std::vector<double> { 0.0, 2.0 }
                                                : approach == 1 ? std::vector<double> { 0.0, 0.5, 2.0, 2.5 } : std::vector<double> { 0.0, 1.5 };
                for (const auto k : kicks)
                    if (o + k * beat < until - 1.0e-6)
                        hit (o + k * beat, GmDrum::kick, 0.88f);
                for (const auto sn : approach == 2 ? std::vector<double> { 2.0 } : std::vector<double> { 1.0, 3.0 })
                    if (o + sn * beat < until - 1.0e-6)
                        hit (o + sn * beat, GmDrum::snare, 0.9f);
                if (fill)
                {
                    const auto f = o + 3 * beat;
                    hit (f, GmDrum::snare, 0.78f);
                    hit (f + beat * 0.5, GmDrum::snare, 0.84f);
                    hit (f + beat * 0.75, GmDrum::snare, 0.9f);
                    hit (f, GmDrum::kick, 0.84f);
                }
            }
            std::stable_sort (out.notes.begin(), out.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
            return out;
        }

        std::optional<LineIdea> mutedLayer (ComposerLayer l, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
        {
            if (spans.empty() || ! mutedBand (s, c))
                return std::nullopt;
            const auto& lane = s.lane (l);
            switch (l)
            {
                case ComposerLayer::bass:
                {
                    const auto& profile = c.bassProfile;
                    const auto lowest = profile.has_value() ? profile->rangeLow() : c.bassTuning.lowest();
                    const auto highest = profile.has_value() ? profile->rangeHigh() : bassTop (c.bassTuning);
                    const auto approach = drivingApproach (lane, 3, 3);
                    LineIdea idea;
                    if (approach == 2)
                    {
                        idea = { LineIdea::Part::bass, "Muted: roots on the beats", "The chord's root on every beat under the muted eighths", {} };
                        const auto length = sectionLength (spans);
                        for (auto t = 0.0; t < length - 1.0e-6; t += c.beatsPerBar / 4.0)
                            addNote (idea, t, c.beatsPerBar / 4.0 * 0.9, rootNear (chordAtIdea (spans, t), lowest, juce::jmin (highest, lowest + 11)), 0.84f, length);
                    }
                    else
                        idea = drivingBass (approach, guitarOf (s, spans, c), spans, c.beatsPerBar, lowest, highest);
                    if (lane.lowString)
                        keepInRange (idea, lowest, lowest + 12);
                    keepInRange (idea, lowest, highest);
                    return idea;
                }
                case ComposerLayer::chords:
                {
                    if (drivingApproach (lane, 2, 2) == 1)
                        return drivingKeys (0, spans, c.beatsPerBar);
                    return LineIdea { LineIdea::Part::chords, "Muted: keys out", "The keys leave the verse to the guitar", {} };
                }
                case ComposerLayer::lead:
                {
                    if (drivingApproach (lane, 2, 2) == 1)
                        return LineIdea { LineIdea::Part::lead, "Muted: lead out", "No lead in the verse", {} };
                    const auto& profile = c.leadProfile.has_value() ? c.leadProfile : c.guitarProfile;
                    const auto velocities = profile.has_value() && profile->velocities.has_value() ? *profile->velocities : c.velocities;
                    const auto lowest = profile.has_value() ? profile->rangeLow() : c.tuning.lowest();
                    const auto highest = profile.has_value() ? profile->rangeHigh() : c.tuning.highest();
                    return drivingLead (2, guitarOf (s, spans, c), spans, c.beatsPerBar, velocities, lowest, highest);
                }
                case ComposerLayer::drums: return mutedDrums (drivingApproach (lane, 3, 3), spans, c.beatsPerBar);
                case ComposerLayer::melody:
                case ComposerLayer::guitar: break;
            }
            return std::nullopt;
        }

        // (0.100.0, This Fire Burns' verse under STACCATO CHUG) the double-kick carpet: sixteenths on the kick and the
        // snare on every beat, whatever the guitar does, until the last bar of each four locks the kick to its hits.
        LineIdea doubleKickCarpet (const LineIdea& guitar, const std::vector<ChordSpan>& spans, double bpb)
        {
            LineIdea out { LineIdea::Part::drums, "Staccato: double-kick carpet",
                           "Sixteenth kicks and the snare on every beat; the last bar of each four the kick with the guitar's hits", {} };
            const auto length = sectionLength (spans);
            const auto bars = (int) std::ceil (length / bpb - 1.0e-6);
            const auto beat = bpb / 4.0;
            const auto hits = riffHits (guitar);
            auto hit = [&out, length] (double at, int note, float velocity)
            {
                if (at >= -1.0e-9 && at < length - 1.0e-6
                    && std::none_of (out.notes.begin(), out.notes.end(), [&] (const LineNote& n) { return n.pitch == note && std::abs (n.beat - at) < 1.0e-6; }))
                    out.notes.push_back ({ at, 0.1, note, velocity });
            };
            for (int bar = 0; bar < bars; ++bar)
            {
                const auto o = bar * bpb;
                if (bar % 4 == 0)
                    hit (o, GmDrum::crash, 0.92f);
                for (int q = 0; q < 4; ++q)
                    hit (o + q * beat, GmDrum::china, q == 0 ? 0.78f : 0.64f);
                if (bar % 4 == 3)
                {
                    for (const auto t : hits)
                        if (t >= o - 1.0e-6 && t < o + bpb - 1.0e-6)
                            hit (t, GmDrum::kick, 0.9f);
                    hit (o + beat, GmDrum::snare, 0.92f);
                    hit (o + 3 * beat, GmDrum::snare, 0.92f);
                }
                else
                {
                    for (int x = 0; x < 16; ++x)
                        hit (o + x * beat / 4, GmDrum::kick, x % 4 == 0 ? 0.9f : 0.74f);
                    for (int q = 0; q < 4; ++q)
                        hit (o + q * beat, GmDrum::snare, q % 2 == 1 ? 0.92f : 0.8f);
                }
            }
            std::stable_sort (out.notes.begin(), out.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
            return out;
        }

        // A lane as SYNC has it (0.86.0): the sync lane itself and every lane with SYNC off as before.
        LineIdea syncedLayer (ComposerLayer l, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
        {
            const auto leaderLayer = s.syncLayer();
            // (0.97.2, the owner: "the bass always needs the locked in with the kick option in every riff") a type that
            // writes the bass has LOCKED WITH THE KICK last: the chords' roots on every kick of that type's drums.
            if (! leaderLayer.has_value() && l == ComposerLayer::bass && ! spans.empty() && sectionTypeWritesLane (l, s, c))
                if (const auto names = sectionApproaches (s, l); ! names.isEmpty() && s.lane (l).approach == names.size() - 1)
                {
                    const auto drums = syncedLayer (ComposerLayer::drums, s, spans, c);
                    auto hits = kickHits (drums);
                    const auto noDrums = hits.empty();
                    if (noDrums)   // (no drums in the part: on every beat, so the bass still has a pulse)
                        for (auto t = 0.0; t < sectionLength (spans) - 1.0e-6; t += c.beatsPerBar / 4.0)
                            hits.push_back (t);
                    const auto& profile = c.bassProfile;
                    const auto lowest = profile.has_value() ? profile->rangeLow() : c.bassTuning.lowest();
                    const auto highest = profile.has_value() ? profile->rangeHigh() : bassTop (c.bassTuning);
                    auto idea = bassOnHits (spans, hits, "Locked with the kick",
                                            noDrums ? juce::String ("The chords' roots on every beat (the drums are out here)")
                                                    : "The chords' roots on every kick of the drums' " + drums.style.toLowerCase());
                    if (s.lane (l).lowString)
                        keepInRange (idea, lowest, lowest + 12);
                    keepInRange (idea, lowest, highest);
                    return idea;
                }
            // (0.106.0, the owner) The drums from the groove library first (not when SYNC makes them follow another lane).
            if (l == ComposerLayer::drums && (! leaderLayer.has_value() || *leaderLayer == ComposerLayer::drums))
                if (auto library = libraryDrumsOf (s, spans, c))
                    return std::move (*library);
            // (0.97.0, CLEAN VERSE's band) with SYNC off, every lane but the melody and the lead plays its clean part.
            if (! leaderLayer.has_value())
                if (auto clean = cleanLayer (l, s, spans, c))
                    return std::move (*clean);
            // (0.102.0) The pre-chorus types: every lane they list options for.
            if (! leaderLayer.has_value())
                if (auto pre = preChorusLayer (l, s, spans, c))
                    return std::move (*pre);
            // (0.105.0) The bridge types likewise.
            if (! leaderLayer.has_value())
                if (auto bridge = bridgeLayer (l, s, spans, c))
                    return std::move (*bridge);
            // (0.109.0) The breakdown types likewise.
            if (! leaderLayer.has_value())
                if (auto breakdown = breakdownLayer (l, s, spans, c))
                    return std::move (*breakdown);
            // (0.110.0) The interlude / post-chorus types likewise.
            if (! leaderLayer.has_value())
                if (auto interlude = interludeLayer (l, s, spans, c))
                    return std::move (*interlude);
            // (0.95.0, STACCATO CHUG's band: the owner's Black, Stricken, Waffle) with SYNC off the bass and the drums
            // follow the guitar: doubling it / the kick on every chug (approach 0 or automatic), or its accents (1).
            if (! leaderLayer.has_value() && (l == ComposerLayer::bass || l == ComposerLayer::drums) && ! spans.empty() && staccatoBand (s))
            {
                if (l == ComposerLayer::drums && s.lane (l).approach == 2)
                    return doubleKickCarpet (guitarOf (s, spans, c), spans, c.beatsPerBar);   // (0.100.0)
                auto locked = s;
                locked.syncLane = (int) ComposerLayer::guitar;
                locked.lane (l).doubleSync = s.lane (l).approach != 1;
                return syncedLayer (l, locked, spans, c);
            }
            // (0.96.0, SPLIT CHUG's band: the owner's Going Under, Papercut, Call Me When You're Sober) the bass doubles
            // the guitar's hits (Papercut, Going Under) or holds each through the gap to the next (Call Me); the kick
            // catches the hits under the section's snare and hats (Going Under, Papercut), or a plain backbeat (Call Me).
            if (! leaderLayer.has_value() && (l == ComposerLayer::bass || l == ComposerLayer::drums) && ! spans.empty()
                && sectionBand (s) == SectionType::Band::split)
            {
                const auto approach = s.lane (l).approach;
                if (l == ComposerLayer::drums && approach == 1)
                    return plainBackbeat (spans, c.beatsPerBar);
                auto locked = s;
                locked.syncLane = (int) ComposerLayer::guitar;
                locked.lane (l).doubleSync = true;
                auto idea = syncedLayer (l, locked, spans, c);
                if (l == ComposerLayer::bass && approach == 1)
                    holdThroughGaps (idea, spans);
                return idea;
            }
            if (! leaderLayer.has_value() || *leaderLayer == l || spans.empty())
            {
                if (! leaderLayer.has_value())
                    if (auto droning = droningLayer (l, s, spans, c))
                        return std::move (*droning);   // (0.89.0: SYNC on wins: the band follows the sync lane)
                if (! leaderLayer.has_value())
                    if (auto driving = drivingLayer (l, s, spans, c))
                        return std::move (*driving);   // (0.98.0)
                if (! leaderLayer.has_value())
                    if (auto syncopated = syncopatedLayer (l, s, spans, c))
                        return std::move (*syncopated);   // (0.99.0)
                if (! leaderLayer.has_value())
                    if (auto muted = mutedLayer (l, s, spans, c))
                        return std::move (*muted);   // (0.100.0)
                return rawLayer (l, s, spans, c);
            }
            auto plain = s;
            plain.syncLane = -1;
            const auto leader = rawLayer (*leaderLayer, plain, spans, c);
            const auto rhythm = syncRhythmOf (*leaderLayer, leader, c.beatsPerBar, 0.0);   // (an idea's beats: from its first chord)
            if (rhythm.all.empty())
                return rawLayer (l, s, spans, c);
            const auto& lane = s.lane (l);
            const auto leaderName = composerLayerName (*leaderLayer);
            switch (l)
            {
                case ComposerLayer::drums:
                {
                    auto beat = drumsOf (s, spans, c, lane.style, lane.doubleSync ? rhythm.all : rhythm.accents, true);
                    beat.style = (lane.doubleSync ? "Doubling: " : "Synced: ") + beat.style;
                    beat.description = (lane.doubleSync ? "The kick on every hit of the " : "The kick on the accents of the ")
                                     + leaderName.toLowerCase() + ". " + beat.description;
                    return beat;
                }
                case ComposerLayer::bass:
                {
                    const auto& profile = c.bassProfile;
                    const auto lowest = profile.has_value() ? profile->rangeLow() : c.bassTuning.lowest();
                    const auto highest = profile.has_value() ? profile->rangeHigh() : bassTop (c.bassTuning);
                    auto idea = lane.doubleSync && *leaderLayer != ComposerLayer::drums
                                  ? doubledLine (leader, spans, lowest, highest, leaderName)
                                  : bassOnHits (spans, lane.doubleSync ? rhythm.all : rhythm.accents,
                                                lane.doubleSync ? "Doubling the " + leaderName.toLowerCase() : "Synced to the " + leaderName.toLowerCase(),
                                                lane.doubleSync ? "The chords' roots on every hit of the sync lane (DOUBLE THE SYNC TRACK)"
                                                                : "The chords' roots on the sync lane's accents");
                    if (lane.lowString)
                        keepInRange (idea, lowest, lowest + 12);
                    keepInRange (idea, lowest, highest);
                    return idea;
                }
                case ComposerLayer::chords:
                    return stabsOn (spans, rhythm.accents, leaderName);
                case ComposerLayer::guitar:
                {
                    const auto& profile = c.guitarProfile;
                    const auto tuning = profile.has_value() ? profile->tuning() : c.tuning;
                    const auto velocities = profile.has_value() && profile->velocities.has_value() ? *profile->velocities : c.velocities;
                    return chugsOn (spans, rhythm, tuning, velocities, leaderName);
                }
                case ComposerLayer::melody:
                case ComposerLayer::lead:
                {
                    auto idea = rawLayer (l, s, spans, c);
                    leanOnto (idea, rhythm.accents);
                    idea.description = "Leaning on the " + leaderName.toLowerCase() + "'s accents. " + idea.description;
                    return idea;
                }
            }
            return rawLayer (l, s, spans, c);
        }
    }

    LaneStrings composerStrings (ComposerLayer l, const ComposerContext& c)
    {
        LaneStrings out;
        const auto& profile = l == ComposerLayer::bass ? c.bassProfile : l == ComposerLayer::lead && c.leadProfile.has_value() ? c.leadProfile : c.guitarProfile;
        if (profile.has_value() && (l == ComposerLayer::bass || l == ComposerLayer::guitar || l == ComposerLayer::lead))
        {
            out.strings = profile->strings;
            out.frets = profile->frets;
            out.profile = &*profile;
        }
        else
            out.strings = l == ComposerLayer::bass ? c.bassTuning.strings : c.tuning.strings;
        return out;
    }

    bool sectionTypeWritesLane (ComposerLayer l, const ComposerState& s, const ComposerContext& c)
    {
        if (s.syncLayer().has_value())
            return false;
        if (droningBand (s, c) || drivingBand (s, c) || syncopatedBand (s, c) || mutedBand (s, c))
            return l == ComposerLayer::bass || l == ComposerLayer::chords || l == ComposerLayer::lead || l == ComposerLayer::drums;
        switch (sectionBand (s))
        {
            case SectionType::Band::staccato:
            case SectionType::Band::split: return l == ComposerLayer::bass || l == ComposerLayer::drums;
            case SectionType::Band::clean: return l != ComposerLayer::melody && l != ComposerLayer::lead;
            case SectionType::Band::preChorus: return l != ComposerLayer::melody && ! sectionApproaches (s, l).isEmpty();
            case SectionType::Band::bridge: return l != ComposerLayer::melody && ! sectionApproaches (s, l).isEmpty();
            case SectionType::Band::breakdown: return l != ComposerLayer::melody && ! sectionApproaches (s, l).isEmpty();
            case SectionType::Band::interlude: return l != ComposerLayer::melody && ! sectionApproaches (s, l).isEmpty();
            case SectionType::Band::none: break;
        }
        return false;
    }

    std::vector<LineIdea> composerChoices (ComposerLayer l, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
    {
        std::vector<LineIdea> out;
        if (spans.empty())
            return out;
        auto each = s;
        // (0.97.1, the owner: every bass style under DRONING was the same bassline) the type's options, not the styles.
        if (sectionTypeWritesLane (l, s, c))
        {
            const auto approaches = sectionApproaches (s, l);
            for (int i = 0; i < approaches.size(); ++i)
            {
                each.lane (l).approach = i;
                out.push_back (syncedLayer (l, each, spans, c));
            }
            return out;
        }
        for (int i = 0; i < composerStyleCount (l); ++i)
        {
            each.lane (l).style = i;
            out.push_back (syncedLayer (l, each, spans, c));
        }
        return out;
    }

    namespace
    {
        // (0.104.0) A block's transition written onto one lane's idea: the last bar (or two) changed. Beats are the
        // idea's own (from the first span's start). Lanes with nothing in them stay empty; the melody is never touched.
        struct Transition
        {
            const SectionTransition& t;
            const std::vector<ChordSpan>& spans;
            const ComposerContext& c;
            double origin = 0.0, end = 0.0, bar = 4.0, lastBar = 0.0;

            Transition (const SectionTransition& tr, const std::vector<ChordSpan>& sp, const ComposerContext& co)
                : t (tr), spans (sp), c (co)
            {
                origin = spans.front().start;
                end = spans.back().end - origin;
                bar = c.lastBarBeats > 0.0 ? c.lastBarBeats : c.beatsPerBar;
                lastBar = juce::jmax (0.0, end - bar);
            }

            const Chord& chordAt (double rel) const
            {
                for (const auto& sp : spans)
                    if (origin + rel >= sp.start - 1.0e-9 && origin + rel < sp.end - 1.0e-9)
                        return sp.chord;
                return spans.back().chord;
            }
            Chord next() const { return c.nextChord.value_or (spans.front().chord); }
            int guitarLow() const
            {
                const auto strings = composerStrings (ComposerLayer::guitar, c).strings;
                return strings.empty() ? 40 : strings.front();
            }
            int bassLow() const { return c.bassProfile.has_value() ? c.bassProfile->rangeLow() : c.bassTuning.lowest(); }
            int guitarRoot (int pc) const { return pitchOfClassIn (pc, guitarLow(), guitarLow() + 11); }
            int bassRoot (int pc) const { return pitchOfClassIn (pc, bassLow(), bassLow() + 11); }
            float open() const { return GuitarVelocity::of (c.velocities.sustain); }

            static bool plays (const LineIdea& idea)
            {
                return std::any_of (idea.notes.begin(), idea.notes.end(), [] (const LineNote& n) { return ! n.keyswitch; });
            }
            // Notes starting from `from` go; notes ringing into it end there (keyswitches are held, so kept).
            static void cutAt (LineIdea& idea, double from)
            {
                std::vector<LineNote> kept;
                for (auto n : idea.notes)
                {
                    if (! n.keyswitch)
                    {
                        if (n.beat >= from - 1.0e-6)
                            continue;
                        if (n.beat + n.length > from)
                            n.length = from - n.beat;
                        if (n.length < 0.02)
                            continue;
                    }
                    kept.push_back (n);
                }
                idea.notes = std::move (kept);
            }
            static void add (LineIdea& idea, double at, double length, int pitch, float velocity)
            {
                LineNote n;
                n.beat = at;
                n.length = length;
                n.pitch = juce::jlimit (0, 127, pitch);
                n.velocity = velocity;
                idea.notes.push_back (n);
            }
            // The band's hit on a beat: a power chord, the root, the chord, the kick (and a crash or a snare).
            void hit (ComposerLayer l, LineIdea& idea, const Chord& chord, double at, double length, bool first) const
            {
                switch (l)
                {
                    case ComposerLayer::guitar:
                        for (const auto o : { 0, 7, 12 })
                            add (idea, at, length, guitarRoot (chord.root) + o, open());
                        break;
                    case ComposerLayer::bass: add (idea, at, length, bassRoot (chord.root), 0.85f); break;
                    case ComposerLayer::chords:
                        if (const auto v = voicingsFor (chord, 55); ! v.empty())
                            for (const auto p : v[v.size() / 2])
                                add (idea, at, length, p, 0.75f);
                        break;
                    case ComposerLayer::drums:
                        add (idea, at, juce::jmin (length, 0.25), 36, 0.95f);
                        add (idea, at, juce::jmin (length, 0.25), first ? 49 : 38, first ? 0.9f : 0.85f);
                        break;
                    case ComposerLayer::melody:
                    case ComposerLayer::lead: break;
                }
            }

            void apply (ComposerLayer l, LineIdea& idea) const
            {
                if (l == ComposerLayer::melody || end <= 0.0)
                    return;
                const auto playing = plays (idea);
                const auto& name = t.name;
                if (t.shortBeats > 0)   // the last bar hit beat by beat
                {
                    const auto unit = c.lastBarBeats > 0.0 ? bar / t.shortBeats : 1.0;
                    cutAt (idea, lastBar);
                    if (playing)
                        for (int k = 0; k < t.shortBeats; ++k)
                        {
                            const auto at = lastBar + k * unit;
                            const auto length = k == t.shortBeats - 1 && c.lastBarBeats > 0.0 ? end - at : unit * 0.5;
                            hit (l, idea, chordAt (at), at, length, k == 0);
                        }
                }
                else if (name == "STOP")
                {
                    cutAt (idea, lastBar);
                    if (playing)
                        hit (l, idea, chordAt (lastBar), lastBar, juce::jmin (1.0, bar), true);
                }
                else if (name == "BREATH")
                    cutAt (idea, end - juce::jmin (1.0, bar * 0.5));
                else if (name == "SNARE ROLL")
                {
                    if (l != ComposerLayer::drums)
                        return;
                    const auto from = end - juce::jmin (1.5, bar);
                    cutAt (idea, from);
                    const auto count = juce::jmax (1, juce::roundToInt ((end - from) / 0.25));
                    add (idea, from, 0.25, 36, 0.9f);
                    for (int k = 0; k < count; ++k)
                        add (idea, from + k * 0.25, 0.25, 38, 0.45f + 0.55f * (float) k / (float) juce::jmax (1, count - 1));
                }
                else if (name == "DRUMS BUILD")
                {
                    cutAt (idea, lastBar);
                    if (l != ComposerLayer::drums || ! playing)
                        return;
                    const auto half = lastBar + bar * 0.5;
                    std::vector<double> hits;
                    for (auto at = lastBar; at < half - 1.0e-6; at += 0.5)
                        hits.push_back (at);
                    for (auto at = half; at < end - 1.0e-6; at += 0.25)
                        hits.push_back (at);
                    for (size_t k = 0; k < hits.size(); ++k)
                        add (idea, hits[k], 0.25, 38, 0.5f + 0.5f * (float) k / (float) juce::jmax<size_t> (1, hits.size() - 1));
                    for (auto at = lastBar; at < end - 1.0e-6; at += 1.0)
                        add (idea, at, 0.25, 36, 0.9f);
                }
                else if (name == "BASS ALONE")
                {
                    if (l != ComposerLayer::bass)
                        cutAt (idea, lastBar);
                }
                else if (name == "SILENT BAR")
                    cutAt (idea, lastBar);
                else if (name == "WALK DOWN" || name == "WALK UP")
                {
                    const auto from = juce::jmax (lastBar, end - juce::jmin (2.0, bar));
                    const auto steps = juce::jmax (1, juce::roundToInt ((end - from) / 0.5));
                    const auto up = name == "WALK UP";
                    const auto target = next();
                    cutAt (idea, from);
                    if (! playing)
                        return;
                    for (int k = 0; k < steps; ++k)
                    {
                        const auto at = from + k * 0.5;
                        const auto away = steps - k;   // half steps from the next chord's root
                        if (l == ComposerLayer::bass)
                        {
                            auto base = bassRoot (target.root);
                            if (up && base - steps < bassLow())
                                base += 12;
                            add (idea, at, 0.45, up ? base - away : base + away, 0.85f);
                        }
                        else if (l == ComposerLayer::guitar)
                        {
                            auto base = guitarRoot (target.root);
                            if (up && base - steps < guitarLow())
                                base += 12;
                            const auto p = up ? base - away : base + away;
                            add (idea, at, 0.45, p, open());
                            add (idea, at, 0.45, p + 7, open());
                        }
                        else if (l == ComposerLayer::drums)
                        {
                            add (idea, at, 0.25, 36, 0.85f);
                            if (k % 2 == 1)
                                add (idea, at, 0.25, 38, 0.85f);
                        }
                    }
                }
                else if (name == "PINCH SLIDE")
                {
                    const auto from = juce::jmax (lastBar, end - juce::jmin (2.0, bar));
                    if (l == ComposerLayer::chords || l == ComposerLayer::lead)
                        cutAt (idea, from);
                    else if (l == ComposerLayer::guitar && playing)
                    {
                        cutAt (idea, from);
                        LineNote n;
                        n.beat = from;
                        n.length = end - from;
                        n.pitch = guitarRoot (chordAt (from).root) + 12;
                        n.velocity = GuitarVelocity::of (c.velocities.pinch);
                        n.effect = LineNote::Effect::dive;
                        n.amount = 7.0f;
                        idea.notes.push_back (n);
                    }
                }
                else if (name == "MUTED LINK")
                {
                    const auto from = end > c.beatsPerBar * 2.5 ? end - c.beatsPerBar * 2.0 : lastBar;
                    cutAt (idea, from);
                    if (! playing || l == ComposerLayer::chords || l == ComposerLayer::lead)
                        return;
                    for (auto at = from; at < end - 1.0e-6; at += 0.5)
                    {
                        const auto& chord = chordAt (at);
                        const auto inBar = std::fmod (at - from, c.beatsPerBar);
                        if (l == ComposerLayer::guitar)
                            add (idea, at, 0.4, guitarRoot (chord.root), GuitarVelocity::of (c.velocities.muteMid));
                        else if (l == ComposerLayer::bass)
                            add (idea, at, 0.45, bassRoot (chord.root), 0.8f);
                        else if (l == ComposerLayer::drums)
                        {
                            add (idea, at, 0.25, 42, 0.6f);
                            if (std::abs (inBar) < 1.0e-6 || std::abs (inBar - 2.0) < 1.0e-6)
                                add (idea, at, 0.25, 36, 0.9f);
                            if (std::abs (inBar - 1.0) < 1.0e-6 || std::abs (inBar - 3.0) < 1.0e-6)
                                add (idea, at, 0.25, 38, 0.85f);
                        }
                    }
                }
                std::stable_sort (idea.notes.begin(), idea.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
            }
        };
    }

    std::vector<LibraryGroove> composerLibraryGrooves (const ComposerState& s, const ComposerContext& c)
    {
        auto all = libraryGroovesFor (c.libraryGrooves, s.lane (ComposerLayer::drums).style, false);
        if (all.size() > 24)
            all.resize (24);
        return all;
    }

    std::optional<LibraryGroove> composerLibraryGroove (const ComposerState& s, const ComposerContext& c)
    {
        if (! s.libraryDrums)
            return std::nullopt;
        const auto grooves = composerLibraryGrooves (s, c);
        if (grooves.empty())
            return std::nullopt;
        if (s.libraryGroove.isNotEmpty())
            for (const auto& g : grooves)
                if (g.file.getFullPathName() == s.libraryGroove)
                    return g;
        // The take picks among the best few (within two points of the best, eight at most): ROLL gives another.
        size_t best = 1;
        while (best < grooves.size() && best < 8 && grooves[best].score >= grooves.front().score - 2)
            ++best;
        return grooves[(size_t) s.lane (ComposerLayer::drums).variation % best];
    }

    void applyTransition (ComposerLayer l, LineIdea& idea, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
    {
        if (spans.empty())
            return;
        if (const auto t = sectionTransitionNamed (s.transition))
            Transition (*t, spans, c).apply (l, idea);
    }

    std::array<LineIdea, composerLayerCount> composeLayers (const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
    {
        std::array<LineIdea, composerLayerCount> out;
        if (spans.empty())
            return out;
        for (const auto l : composerLayers())
        {
            auto idea = syncedLayer (l, s, spans, c);
            idea.part = composerPartOf (l);
            applyTransition (l, idea, s, spans, c);   // (0.104.0: the block's hand-over to the next)
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
            // (0.63.1) What the instrument can play: its shortest note, its chugs in a row.
            if (hasLowString (l) || l == ComposerLayer::lead)
                if (const auto* profile = composerStrings (l, c).profile)
                    keepPlayable (idea, profile->shortestNote, profile->chugRun, spans.back().end - spans.front().start);
            // (0.63.0) Power chords by the instrument's keyswitch (its root alone), single notes back on theirs.
            if (l == ComposerLayer::guitar || l == ComposerLayer::lead)
                if (const auto* profile = composerStrings (l, c).profile)
                    useArticulationKeys (idea, ArticulationKeys::of (*profile));   // (0.66.0: tapping, tremolo, harmonics too)
            // ONLY STRING (0.62.0): onto that string, its keyswitch (the INSTRUMENTS profile's) held through it.
            if (const auto string = s.lane (l).onlyString; string > 0 && hasLowString (l))
            {
                const auto on = composerStrings (l, c);
                playOnString (idea, on.strings, string, on.frets, on.profile != nullptr ? on.profile->keyFor (string) : -1,
                              spans.back().end - spans.front().start, on.profile != nullptr ? on.profile->releaseKey() : -1);
            }
            out[(size_t) l] = std::move (idea);
        }
        return out;
    }

    //==========================================================================
    void rollLane (ComposerState& s, ComposerLayer l, juce::Random& random)
    {
        // A new take, never the one it has (0.61.0: the style stays; the owner chooses it in the menu).
        auto& lane = s.lane (l);
        const auto old = lane.variation;
        do
            lane.variation = random.nextInt (10000);
        while (lane.variation == old);
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
