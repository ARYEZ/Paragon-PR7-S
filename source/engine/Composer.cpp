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
            lanesText.add (juce::String (l.on ? 1 : 0) + "," + juce::String (l.locked ? 1 : 0) + "," + juce::String (l.style) + "," + juce::String (l.variation)
                           + "," + juce::String (l.busy, 2) + "," + juce::String (l.lowString ? 1 : 0) + "," + juce::String (l.onlyString)
                           + "," + juce::String (l.doubleSync ? 1 : 0));
        return juce::String (keyIndex) + "|" + genre + "|" + mood + "|" + juce::String (bars) + "|" + progression + "|"
             + (fromChordTrack ? "1" : "0") + "|" + (toChordTrack ? "1" : "0") + "|" + lanesText.joinIntoString (";")
             + "|" + juce::String (riffFlavour) + "," + (writtenRiffs ? "1" : "0") + "," + juce::String (syncLane);
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
        }
        if (parts.size() >= 9)
        {
            // (0.85.0) The guitar's flavour and written riffs (older text: the generator, ANY).
            const auto g = juce::StringArray::fromTokens (parts[8], ",", "");
            s.riffFlavour = juce::jlimit (0, (int) riffFlavours().size() - 1, g[0].getIntValue());
            s.writtenRiffs = g.size() >= 2 && g[1] == "1";
            s.syncLane = g.size() >= 3 ? juce::jlimit (-1, composerLayerCount - 1, g[2].getIntValue()) : -1;   // (0.86.0)
        }
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
                    if (const auto* span = spanAt (spans, low.beat))
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
                const auto* span = spanAt (spans, accents[i]);
                if (span == nullptr)
                    continue;
                const auto voicings = voicingsFor (span->chord, 55);
                if (voicings.empty())
                    continue;
                const auto& voicing = voicings[(size_t) closestVoicing (voicings, previous, {})];
                previous = voicing;
                const auto next = i + 1 < accents.size() ? accents[i + 1] : span->end;
                const auto length = juce::jmax (0.1, juce::jmin (next - accents[i], 2.0, span->end - accents[i]));
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
                const auto* span = spanAt (spans, at);
                if (span == nullptr)
                    continue;
                const auto root = rootNear (span->chord, tuning.lowest(), tuning.lowest() + 11, false);
                const auto accent = std::any_of (rhythm.accents.begin(), rhythm.accents.end(), [at] (double a) { return std::abs (a - at) < 1.0e-6; });
                const auto next = i + 1 < rhythm.all.size() ? rhythm.all[i + 1] : span->end;
                const auto room = juce::jmax (0.05, juce::jmin (next - at, span->end - at));
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

        // A lane as SYNC has it (0.86.0): the sync lane itself and every lane with SYNC off as before.
        LineIdea syncedLayer (ComposerLayer l, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
        {
            const auto leaderLayer = s.syncLayer();
            if (! leaderLayer.has_value() || *leaderLayer == l || spans.empty())
                return rawLayer (l, s, spans, c);
            auto plain = s;
            plain.syncLane = -1;
            const auto leader = rawLayer (*leaderLayer, plain, spans, c);
            const auto rhythm = syncRhythmOf (*leaderLayer, leader, c.beatsPerBar, spans.front().start);
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

    std::vector<LineIdea> composerChoices (ComposerLayer l, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
    {
        std::vector<LineIdea> out;
        if (spans.empty())
            return out;
        auto each = s;
        for (int i = 0; i < composerStyleCount (l); ++i)
        {
            each.lane (l).style = i;
            out.push_back (syncedLayer (l, each, spans, c));
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
            auto idea = syncedLayer (l, s, spans, c);
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
