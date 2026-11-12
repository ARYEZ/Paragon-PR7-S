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
             + "|" + sectionType.removeCharacters ("|");
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
        else if (r.flavour == RiffFlavour::droning)
            r.approach = lane.approach;   // (0.94.0)
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

        LineIdea droningKeys (const ComposerLane& lane, const std::vector<ChordSpan>& spans, double bpb, const std::vector<double>& strikes)
        {
            const auto approach = lane.approach >= 0 ? lane.approach % 3 : lane.style % 3;   // (0.94.0: chosen, else by style)
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
            const auto approach = lane.approach >= 0 ? lane.approach % 3 : lane.variation % 3;   // (0.94.0: chosen, else by the take)
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
                    auto idea = droningBass (guitar, spans, lowest, highest);
                    if ((lane.approach >= 0 ? lane.approach % 2 : lane.variation % 2) == 1)   // (0.94.0: chosen, else by the take)
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

        // A lane as SYNC has it (0.86.0): the sync lane itself and every lane with SYNC off as before.
        LineIdea syncedLayer (ComposerLayer l, const ComposerState& s, const std::vector<ChordSpan>& spans, const ComposerContext& c)
        {
            const auto leaderLayer = s.syncLayer();
            // (0.95.0, STACCATO CHUG's band: the owner's Black, Stricken, Waffle) with SYNC off the bass and the drums
            // follow the guitar: doubling it / the kick on every chug (approach 0 or automatic), or its accents (1).
            if (! leaderLayer.has_value() && (l == ComposerLayer::bass || l == ComposerLayer::drums) && ! spans.empty() && staccatoBand (s))
            {
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
