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
             + "|" + sectionType.removeCharacters ("|") + "|" + transition.removeCharacters ("|");
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
            // (0.97.0, CLEAN VERSE's band) with SYNC off, every lane but the melody and the lead plays its clean part.
            if (! leaderLayer.has_value())
                if (auto clean = cleanLayer (l, s, spans, c))
                    return std::move (*clean);
            // (0.102.0) The pre-chorus types: every lane they list options for.
            if (! leaderLayer.has_value())
                if (auto pre = preChorusLayer (l, s, spans, c))
                    return std::move (*pre);
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
