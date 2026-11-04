#include "FollowChords.h"

#include "Project.h"

namespace pr7s
{
    namespace
    {
        int pc (int p) noexcept { return ((p % 12) + 12) % 12; }

        // The short way from one pitch class to another: -6 to +5.
        int shortest (int from, int to) noexcept
        {
            auto d = pc (to - from);
            return d > 5 ? d - 12 : d;
        }

        int fold (int pitch) noexcept
        {
            while (pitch > 127)
                pitch -= 12;
            while (pitch < 0)
                pitch += 12;
            return pitch;
        }

        // A chord's tones by what they are: 0 root, 1 third (or the sus note), 2 fifth,
        // 3 seventh (or sixth), 4 ninth, 5 eleventh, 6 thirteenth; -1: none. Semitones above the root.
        std::array<int, 7> slotsOf (const Chord& chord)
        {
            std::array<int, 7> slots;
            slots.fill (-1);
            slots[0] = 0;
            using T = Chord::Type;
            switch (chord.type)
            {
                case T::major:      slots[1] = 4; slots[2] = 7; break;
                case T::minor:      slots[1] = 3; slots[2] = 7; break;
                case T::diminished: slots[1] = 3; slots[2] = 6; break;
                case T::augmented:  slots[1] = 4; slots[2] = 8; break;
                case T::sus2:       slots[1] = 2; slots[2] = 7; break;
                case T::sus4:       slots[1] = 5; slots[2] = 7; break;
            }
            for (const auto i : chord.intervals())
            {
                if (i == 0 || i == slots[1] || i == slots[2])
                    continue;
                if (i < 12)
                    slots[3] = i;                       // 6, b7, maj7, dim7
                else if (i - 12 <= 3)
                    slots[4] = i - 12;                  // b9, 9, #9
                else if (i - 12 <= 6)
                    slots[5] = i - 12;                  // 11, #11
                else
                    slots[6] = i - 12;                  // 13
            }
            return slots;
        }

        // The scale's seven notes as semitones above its tonic.
        std::array<int, 7> stepsOf (const Key& key)
        {
            std::array<int, 7> steps {};
            const auto scale = key.scale();
            for (int d = 0; d < 7; ++d)
                steps[(size_t) d] = pc (scale[(size_t) d] - key.pitchClass());
            return steps;
        }

        // A pitch class's degree in a key (-1: not in it).
        int degreeOf (int pitchClass, const Key& key)
        {
            const auto steps = stepsOf (key);
            for (int d = 0; d < 7; ++d)
                if (pc (key.pitchClass() + steps[(size_t) d]) == pc (pitchClass))
                    return d;
            return -1;
        }

        // Scales: moved by scale steps (the chords' roots' degrees apart) and into the new key.
        int byScale (int pitch, const Chord& from, const Key& fromKey, const Chord& to, const Key& toKey)
        {
            const auto degFrom = degreeOf (from.root, fromKey), degTo = degreeOf (to.root, toKey);
            if (degFrom < 0 || degTo < 0)
                return pitch + shortest (from.root, to.root);   // a borrowed chord: exactly by its root
            auto step = degTo - degFrom;
            while (step > 3)
                step -= 7;
            while (step < -3)
                step += 7;
            const auto fromSteps = stepsOf (fromKey), toSteps = stepsOf (toKey);
            const auto tonic = fromKey.pitchClass();
            const auto rel = pitch - tonic;
            const auto octave = (int) std::floor ((double) rel / 12.0);
            const auto within = rel - octave * 12;
            // Its degree, and how far above it (a note outside the scale: the degree under it, raised).
            auto degree = 0;
            for (int d = 0; d < 7; ++d)
                if (fromSteps[(size_t) d] <= within)
                    degree = d;
            const auto raised = within - fromSteps[(size_t) degree];
            auto target = octave * 7 + degree + step;
            const auto targetOctave = (int) std::floor ((double) target / 7.0);
            const auto targetDegree = target - targetOctave * 7;
            const auto newTonic = tonic + shortest (tonic, toKey.pitchClass());
            return newTonic + targetOctave * 12 + toSteps[(size_t) targetDegree] + raised;
        }

        int nearestOf (int pitch, const std::vector<int>& classes)
        {
            if (classes.empty())
                return pitch;
            for (int distance = 0; distance <= 6; ++distance)
                for (const auto candidate : { pitch - distance, pitch + distance })   // (ties: the lower)
                    if (std::find (classes.begin(), classes.end(), pc (candidate)) != classes.end())
                        return candidate;
            return pitch;
        }
    }

    const std::vector<FollowMode>& followModes()
    {
        static const std::vector<FollowMode> modes { FollowMode::off, FollowMode::chordsAndScales, FollowMode::chords,
                                                     FollowMode::scales, FollowMode::rootNotes };
        return modes;
    }

    juce::String followModeName (FollowMode mode)
    {
        switch (mode)
        {
            case FollowMode::off:             return "OFF";
            case FollowMode::chordsAndScales: return "CHORDS & SCALES";
            case FollowMode::chords:          return "CHORDS";
            case FollowMode::scales:          return "SCALES";
            case FollowMode::rootNotes:       return "ROOT NOTES";
        }
        return "OFF";
    }

    juce::String followModeId (FollowMode mode)
    {
        switch (mode)
        {
            case FollowMode::off:             return "off";
            case FollowMode::chordsAndScales: return "chordsAndScales";
            case FollowMode::chords:          return "chords";
            case FollowMode::scales:          return "scales";
            case FollowMode::rootNotes:       return "rootNotes";
        }
        return "off";
    }

    FollowMode followModeFromId (const juce::String& id)
    {
        for (const auto mode : followModes())
            if (followModeId (mode) == id)
                return mode;
        return FollowMode::off;
    }

    juce::String liveFollowName (LiveFollow live)
    {
        return live == LiveFollow::chords ? "CHORDS" : live == LiveFollow::scales ? "SCALES" : "OFF";
    }

    juce::String liveFollowId (LiveFollow live)
    {
        return live == LiveFollow::chords ? "chords" : live == LiveFollow::scales ? "scales" : "off";
    }

    LiveFollow liveFollowFromId (const juce::String& id)
    {
        return id == "chords" ? LiveFollow::chords : id == "scales" ? LiveFollow::scales : LiveFollow::off;
    }

    Key keyOfChord (const Chord& chord)
    {
        const auto minor = chord.type == Chord::Type::minor || chord.type == Chord::Type::diminished;   // (ours: simple)
        return Key::fromPitchClass (chord.root, minor);
    }

    //==========================================================================
    Chord FollowReference::chordAt (double beat) const
    {
        if (chords.empty())
            return {};   // C major (the manual: no chords found, "C")
        auto found = 0;
        for (int i = 0; i < (int) chords.size(); ++i)
            if (chords[(size_t) i].beat <= beat + 1.0e-9)
                found = i;
        return chords[(size_t) found].chord;
    }

    Key FollowReference::keyAt (double beat) const
    {
        if (scales.empty())
            return chords.empty() ? Key {} : keyOfChord (chordAt (beat));
        auto key = scales.front().key;
        for (const auto& s : scales)
            if (s.beat <= beat + 1.0e-9)
                key = s.key;
        return key;
    }

    std::unique_ptr<juce::XmlElement> FollowReference::toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement> ("FollowReference");
        for (const auto& e : chords)
        {
            auto* x = xml->createNewChildElement ("Chord");
            x->setAttribute ("beat", e.beat);
            x->setAttribute ("chord", e.chord.toText());
        }
        for (const auto& s : scales)
        {
            auto* x = xml->createNewChildElement ("Scale");
            x->setAttribute ("beat", s.beat);
            x->setAttribute ("letter", s.key.letter);
            x->setAttribute ("accidental", s.key.accidental);
            x->setAttribute ("minor", s.key.minor);
        }
        return xml;
    }

    FollowReference FollowReference::fromXml (const juce::XmlElement& xml)
    {
        FollowReference r;
        for (const auto* x : xml.getChildWithTagNameIterator ("Chord"))
            if (const auto chord = Chord::fromText (x->getStringAttribute ("chord")))
                r.chords.push_back ({ juce::jmax (0.0, x->getDoubleAttribute ("beat")), *chord });
        for (const auto* x : xml.getChildWithTagNameIterator ("Scale"))
            r.scales.push_back ({ juce::jmax (0.0, x->getDoubleAttribute ("beat")),
                                  Key { juce::jlimit (0, 6, x->getIntAttribute ("letter")), juce::jlimit (-1, 1, x->getIntAttribute ("accidental")),
                                        x->getBoolAttribute ("minor") } });
        std::stable_sort (r.chords.begin(), r.chords.end(), [] (const ChordEvent& a, const ChordEvent& b) { return a.beat < b.beat; });
        std::stable_sort (r.scales.begin(), r.scales.end(), [] (const ScaleEvent& a, const ScaleEvent& b) { return a.beat < b.beat; });
        return r;
    }

    FollowReference FollowReference::ofChord (const Chord& chord)
    {
        FollowReference r;
        r.chords.push_back ({ 0.0, chord });
        r.scales.push_back ({ 0.0, keyOfChord (chord) });
        return r;
    }

    FollowReference FollowReference::ofChordTrack (const ChordTrack& track)
    {
        FollowReference r;
        r.chords = track.events;
        r.scales = track.scales;
        return r;
    }

    //==========================================================================
    int followPitch (int pitch, FollowMode mode, const Chord& from, const Key& fromKey, const Chord& to, const Key& toKey)
    {
        if (mode == FollowMode::off || (from == to && fromKey == toKey))
            return pitch;
        const auto d = shortest (from.root, to.root);
        auto result = pitch;
        switch (mode)
        {
            case FollowMode::off:
                break;
            case FollowMode::rootNotes:
                result = pitch + d;
                break;
            case FollowMode::scales:
                result = byScale (pitch, from, fromKey, to, toKey);
                break;
            case FollowMode::chords:
                result = nearestOf (pitch + d, to.pitchClasses());
                break;
            case FollowMode::chordsAndScales:
            {
                const auto r = pc (pitch - from.root);
                const auto fromSlots = slotsOf (from), toSlots = slotsOf (to);
                auto slot = -1;
                for (int s = 0; s < 7; ++s)
                    if (fromSlots[(size_t) s] >= 0 && pc (fromSlots[(size_t) s]) == r)
                    {
                        slot = s;
                        break;
                    }
                if (slot >= 0 && toSlots[(size_t) slot] >= 0)
                    result = pitch - r + d + pc (toSlots[(size_t) slot]);   // the same chord tone of the new chord
                else
                    result = byScale (pitch, from, fromKey, to, toKey);
                break;
            }
        }
        return fold (result);
    }

    int livePitch (int pitch, LiveFollow live, const Chord& chord, const Key& key)
    {
        if (live == LiveFollow::off)
            return pitch;
        return nearestInMask (pitch, pitchClassMask (live == LiveFollow::chords ? chord.pitchClasses() : key.scale()));
    }

    juce::uint16 pitchClassMask (const std::vector<int>& classes) noexcept
    {
        juce::uint16 mask = 0;
        for (const auto c : classes)
            mask = (juce::uint16) (mask | (1u << pc (c)));
        return mask;
    }

    int nearestInMask (int pitch, juce::uint16 mask) noexcept
    {
        if (mask == 0)
            return pitch;
        for (int distance = 0; distance <= 6; ++distance)
            for (const auto candidate : { pitch - distance, pitch + distance })   // (ties: the lower)
                if ((mask & (1u << pc (candidate))) != 0)
                    return fold (candidate);
        return pitch;
    }

    int chordIndexForNote (const ChordTrack& track, double beat, int shiftTicks)
    {
        if (track.events.empty())
            return -1;
        const auto index = track.indexAt (beat - shiftTicks / 480.0);
        return juce::jmax (0, index);
    }

    std::vector<FollowedNote> followNotes (const std::vector<Note>& notes, double partStart, FollowMode mode,
                                           const FollowReference& reference, const ChordTrack& chords, int shiftTicks, const Key& fallback)
    {
        std::vector<FollowedNote> result (notes.size());
        for (size_t i = 0; i < notes.size(); ++i)
        {
            result[i].pitch = notes[i].pitch;
            if (mode == FollowMode::off)
                continue;
            const auto beat = partStart + notes[i].start;
            const auto index = chordIndexForNote (chords, beat, shiftTicks);
            if (index < 0)
                continue;   // no chord track: as played
            const auto look = beat - shiftTicks / 480.0;
            const auto& to = chords.events[(size_t) index].chord;
            const auto toKey = chords.keyAt (look, chords.scales.empty() ? fallback : chords.scales.front().key);
            result[i].pitch = followPitch (notes[i].pitch, mode, reference.chordAt (look), reference.keyAt (look), to, toKey);
        }
        // Two on one pitch at one start: the softer dropped (it would only double the other).
        std::vector<size_t> order (notes.size());
        for (size_t i = 0; i < order.size(); ++i)
            order[i] = i;
        std::sort (order.begin(), order.end(), [&] (size_t a, size_t b)
        {
            if (result[a].pitch != result[b].pitch)
                return result[a].pitch < result[b].pitch;
            return notes[a].start < notes[b].start;
        });
        for (size_t k = 1; k < order.size(); ++k)
        {
            const auto a = order[k - 1], b = order[k];
            if (result[a].pitch == result[b].pitch && std::abs (notes[a].start - notes[b].start) < 1.0e-6)
            {
                // (the louder one goes on: it takes the place in the order for the next comparison)
                const auto softer = notes[b].velocity > notes[a].velocity ? a : b;
                result[softer].dropped = true;
                if (softer == b)
                    order[k] = a;
            }
        }
        return result;
    }
}
