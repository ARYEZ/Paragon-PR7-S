#include "FollowChords.h"

#include "Project.h"

#include <numeric>

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
                                                     FollowMode::scales, FollowMode::rootNotes, FollowMode::voicings, FollowMode::singleVoice };
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
            case FollowMode::voicings:        return "VOICINGS";
            case FollowMode::singleVoice:     return "SINGLE VOICE";
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
            case FollowMode::voicings:        return "voicings";
            case FollowMode::singleVoice:     return "singleVoice";
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
            case FollowMode::voicings:      // (followNotes voices these, seeing the notes together; one asked alone: as Scales)
            case FollowMode::singleVoice:
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
                                           const FollowReference& reference, const ChordTrack& chords, int shiftTicks, const Key& fallback,
                                           const VoicingSettings* voicing, Voice voice)
    {
        std::vector<FollowedNote> result (notes.size());
        for (size_t i = 0; i < notes.size(); ++i)
            result[i].pitch = notes[i].pitch;
        if (mode == FollowMode::off || chords.events.empty())
            return result;
        const auto voiced = mode == FollowMode::voicings || mode == FollowMode::singleVoice;
        const auto& settings = voicing != nullptr ? *voicing : chords.voicing;
        // (Voicings: the chord track's chords voiced in order, and the reference's, once.)
        const auto newVoicings = voiced ? voiceChords (chords.events, settings) : std::vector<std::vector<int>> {};
        const auto oldVoicings = mode == FollowMode::voicings ? voiceChords (reference.chords, settings) : std::vector<std::vector<int>> {};
        const auto oldVoicingAt = [&] (double look) -> std::vector<int>
        {
            if (oldVoicings.empty())
                return voicingCandidates (reference.chordAt (look), settings).front();
            auto found = 0;
            for (int i = 0; i < (int) reference.chords.size(); ++i)
                if (reference.chords[(size_t) i].beat <= look + 1.0e-9)
                    found = i;
            return oldVoicings[(size_t) found];
        };
        // Notes starting together, by start (Voicings voices them as one chord).
        std::vector<size_t> order (notes.size());
        for (size_t i = 0; i < order.size(); ++i)
            order[i] = i;
        std::stable_sort (order.begin(), order.end(), [&] (size_t x, size_t y)
        {
            if (std::abs (notes[x].start - notes[y].start) > 1.0e-6)
                return notes[x].start < notes[y].start;
            return notes[x].pitch < notes[y].pitch;
        });
        for (size_t k = 0; k < order.size();)
        {
            auto end = k + 1;
            while (end < order.size() && std::abs (notes[order[end]].start - notes[order[k]].start) < 1.0 / 64.0)
                ++end;
            const auto beat = partStart + notes[order[k]].start;
            const auto look = beat - shiftTicks / 480.0;
            const auto index = chordIndexForNote (chords, beat, shiftTicks);
            if (index >= 0)
            {
                const auto& to = chords.events[(size_t) index].chord;
                if (mode == FollowMode::singleVoice)
                {
                    const auto pitch = voiceOf (newVoicings[(size_t) index], voice);
                    for (auto g = k; g < end; ++g)
                        result[order[g]].pitch = pitch >= 0 ? pitch : notes[order[g]].pitch;
                }
                else if (mode == FollowMode::voicings)
                {
                    const auto& v = newVoicings[(size_t) index];
                    const auto n = (int) (end - k), m = (int) v.size();
                    if (m > 0 && n > 1)
                    {
                        // A chord: the lowest note the lowest voice... (spread over them, or the outer ones kept).
                        for (int g = 0; g < n; ++g)
                            result[order[k + (size_t) g]].pitch = v[(size_t) juce::roundToInt ((double) g * (m - 1) / (n - 1))];
                    }
                    else if (m > 0)
                    {
                        // A note alone: the voice nearest it in the old voicing, as far above or below as it was.
                        const auto old = oldVoicingAt (look);
                        const auto pitch = notes[order[k]].pitch;
                        auto nearest = 0;
                        for (int i = 1; i < (int) old.size(); ++i)
                            if (std::abs (old[(size_t) i] - pitch) < std::abs (old[(size_t) nearest] - pitch))
                                nearest = i;
                        const auto r = (int) old.size();
                        const auto j = r > 1 ? juce::roundToInt ((double) nearest * (m - 1) / (r - 1)) : m - 1;
                        const auto octaves = old.empty() ? 0 : juce::roundToInt ((pitch - old[(size_t) nearest]) / 12.0);
                        result[order[k]].pitch = fold (v[(size_t) j] + 12 * octaves);
                    }
                }
                else
                {
                    const auto toKey = chords.keyAt (look, chords.scales.empty() ? fallback : chords.scales.front().key);
                    for (auto g = k; g < end; ++g)
                        result[order[g]].pitch = followPitch (notes[order[g]].pitch, mode, reference.chordAt (look), reference.keyAt (look), to, toKey);
                }
            }
            k = end;
        }
        // Two on one pitch at one start: the softer dropped (it would only double the other).
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

//==============================================================================
// Voicings (0.38.0)
namespace pr7s
{
    namespace
    {
        using Lib = VoicingSettings::Library;
        using PS = VoicingSettings::PianoStyle;
        using GS = VoicingSettings::GuitarStyle;

        // A triad's third and fifth (sus: its 2nd or 4th as the third).
        std::pair<int, int> thirdAndFifth (const Chord& chord)
        {
            switch (chord.type)
            {
                case Chord::Type::major:      return { 4, 7 };
                case Chord::Type::minor:      return { 3, 7 };
                case Chord::Type::diminished: return { 3, 6 };
                case Chord::Type::augmented:  return { 4, 8 };
                case Chord::Type::sus2:       return { 2, 7 };
                case Chord::Type::sus4:       return { 5, 7 };
            }
            return { 4, 7 };
        }

        // Its seventh: its own if it has one; else major 7 for major and augmented, b7 for minor
        // and sus, bb7 for diminished (our choices).
        int seventhOf (const Chord& chord)
        {
            for (const auto i : chord.intervals())
                if (i >= 9 && i <= 11)
                    return i;
            switch (chord.type)
            {
                case Chord::Type::major:
                case Chord::Type::augmented:  return 11;
                case Chord::Type::diminished: return 9;
                default:                      return 10;
            }
        }

        // Pitch classes (relative to the root) stacked close upwards, each inversion, inside [low, high].
        std::vector<std::vector<int>> closeStacks (int root, const std::vector<int>& tones, int low, int high)
        {
            std::vector<std::vector<int>> stacks;
            if (tones.empty())
                return stacks;
            for (size_t first = 0; first < tones.size(); ++first)
                for (int start = low; start < low + 12; ++start)
                {
                    if (pc (start - root - tones[first]) != 0)
                        continue;
                    std::vector<int> notes { start };
                    for (size_t k = 1; k < tones.size(); ++k)
                    {
                        auto next = notes.back() + 1;
                        while (pc (next - root - tones[(first + k) % tones.size()]) != 0)
                            ++next;
                        notes.push_back (next);
                    }
                    if (notes.back() <= high)
                        stacks.push_back (notes);
                }
            std::sort (stacks.begin(), stacks.end());
            stacks.erase (std::unique (stacks.begin(), stacks.end()), stacks.end());
            return stacks;
        }

        std::vector<std::vector<int>> pianoVoicings (const Chord& chord, const VoicingSettings& s)
        {
            const auto [third, fifth] = thirdAndFifth (chord);
            const auto seventh = seventhOf (chord);
            std::vector<int> tones;
            const auto plain = chord.tension == Chord::Tension::none;
            if (! plain && s.piano == PS::triads)
            {
                for (const auto i : chord.intervals())   // (the manual: chords with more notes aren't changed)
                    if (std::find (tones.begin(), tones.end(), pc (i)) == tones.end())
                        tones.push_back (pc (i));
            }
            else
                switch (s.piano)
                {
                    case PS::triads:       tones = { 0, third, fifth }; break;
                    case PS::triadsNine:   tones = { third, fifth, 2 }; break;
                    case PS::fourNote:     tones = { third, fifth, seventh, 2 }; break;
                    case PS::fourNoteOpen: tones = { third, seventh, 2, 9 }; break;
                    case PS::fiveNote:     tones = { 0, third, fifth, seventh, 2 }; break;
                }
            std::sort (tones.begin(), tones.end());
            tones.erase (std::unique (tones.begin(), tones.end()), tones.end());
            const auto low = juce::jlimit (24, 96, s.lowest), high = juce::jlimit (low + 12, 108, s.highest);
            auto stacks = closeStacks (chord.root, tones, low, high);
            if (stacks.empty())
                stacks = closeStacks (chord.root, tones, low, high + 12);
            // The left hand: the root (or a slash chord's bass) from lowestRoot up an octave.
            const auto bass = chord.bass >= 0 ? chord.bass : chord.root;
            auto lh = juce::jlimit (12, 84, s.lowestRoot);
            while (pc (lh) != pc (bass))
                ++lh;
            for (auto& v : stacks)
            {
                if (s.fattenUp && v.front() - 12 > lh)
                    v.insert (v.begin(), v.front() - 12);
                if (s.addRoot || chord.bass >= 0)
                {
                    if (s.duplicateRoot && lh + 12 < v.front())
                        v.insert (v.begin(), lh + 12);
                    v.insert (v.begin(), lh);
                }
            }
            return stacks;
        }

        std::vector<std::vector<int>> guitarVoicings (const Chord& chord, const VoicingSettings& s)
        {
            const auto [third, fifth] = thirdAndFifth (chord);
            std::vector<std::vector<int>> shapes;
            if (s.guitar == GS::threeString)
                return closeStacks (chord.root, { 0, third, fifth }, 55, 79);   // the G, B and E strings
            const auto octave = s.guitar == GS::fourNote ? seventhOf (chord) : 12;
            // Barre shapes: the root on the low E string (frets 0-11), or on the A string.
            for (int fret = 0; fret < 12; ++fret)
            {
                const auto e = 40 + fret;
                if (pc (e) == pc (chord.root))
                    shapes.push_back ({ e, e + fifth, e + octave, e + 12 + third, e + 12 + fifth, e + 24 });
                const auto a = 45 + fret;
                if (pc (a) == pc (chord.root))
                    shapes.push_back ({ a, a + fifth, a + octave, a + 12 + third, a + 12 + fifth });
            }
            for (auto& v : shapes)
                std::sort (v.begin(), v.end());
            std::sort (shapes.begin(), shapes.end());
            return shapes;
        }

        std::vector<std::vector<int>> basicVoicings (const Chord& chord, const VoicingSettings& s)
        {
            std::vector<int> tones;
            for (const auto i : chord.intervals())
                tones.push_back (pc (i));
            std::sort (tones.begin(), tones.end());
            tones.erase (std::unique (tones.begin(), tones.end()), tones.end());
            const auto c3 = 60 + 12 * juce::jlimit (-3, 3, s.basicOctave);
            // Root position from C3 first; then its inversions round it.
            auto stacks = closeStacks (chord.root, tones, c3 - 6, c3 + 30);
            std::stable_sort (stacks.begin(), stacks.end(), [&] (const auto& a, const auto& b)
            {
                const auto rootA = pc (a.front()) == pc (chord.root) && a.front() >= c3, rootB = pc (b.front()) == pc (chord.root) && b.front() >= c3;
                if (rootA != rootB)
                    return rootA;
                return std::abs (a.front() - c3) < std::abs (b.front() - c3);
            });
            if (chord.bass >= 0)
                for (auto& v : stacks)
                {
                    auto b = v.front() - 12;
                    while (pc (b) != pc (chord.bass))
                        ++b;
                    if (b >= v.front())
                        b -= 12;
                    v.insert (v.begin(), b);
                }
            return stacks;
        }
    }

    std::vector<std::vector<int>> voicingCandidates (const Chord& chord, const VoicingSettings& s)
    {
        auto found = s.library == Lib::piano ? pianoVoicings (chord, s) : s.library == Lib::guitar ? guitarVoicings (chord, s) : basicVoicings (chord, s);
        if (found.empty())
            found.push_back (voicingsFor (chord).front());
        return found;
    }

    std::vector<std::vector<int>> voiceChords (const std::vector<ChordEvent>& events, const VoicingSettings& s)
    {
        std::vector<std::vector<int>> voiced;
        std::vector<int> previous;
        for (const auto& e : events)
        {
            const auto candidates = voicingCandidates (e.chord, s);
            auto chosen = 0;
            if (previous.empty() || ! s.adaptive)
            {
                // The first (or not adaptive): piano and basic the one nearest the middle of the range, guitar the lowest shape.
                if (s.library != Lib::guitar)
                {
                    const auto middle = s.library == Lib::piano ? (s.lowest + s.highest) / 2.0 : 66.0 + 12 * s.basicOctave;
                    auto best = std::numeric_limits<double>::max();
                    for (int i = 0; i < (int) candidates.size(); ++i)
                    {
                        const auto& v = candidates[(size_t) i];
                        const auto top = v.size() > 1 && s.library == Lib::piano ? std::vector<int> (v.begin() + 1, v.end()) : v;
                        const auto centre = std::accumulate (top.begin(), top.end(), 0.0) / (double) top.size();
                        if (s.library == Lib::basic)
                        {
                            chosen = 0;   // (root position from C3)
                            break;
                        }
                        if (std::abs (centre - middle) < best)
                            best = std::abs (centre - middle), chosen = i;
                    }
                }
            }
            else
                chosen = closestVoicing (candidates, previous, {});
            chosen = juce::jlimit (0, (int) candidates.size() - 1, chosen);
            voiced.push_back (candidates[(size_t) chosen]);
            previous = voiced.back();
        }
        return voiced;
    }

    int voiceOf (const std::vector<int>& voicing, Voice voice)
    {
        if (voicing.empty())
            return -1;
        const auto n = (int) voicing.size();
        switch (voice)
        {
            case Voice::soprano: return voicing[(size_t) n - 1];
            case Voice::alto:    return voicing[(size_t) juce::jmax (0, n - 2)];
            case Voice::tenor:   return voicing[(size_t) juce::jmax (0, n - 3)];
            case Voice::bass:    return voicing.front();
        }
        return -1;
    }

    juce::String voiceName (Voice v)
    {
        return v == Voice::soprano ? "SOPRANO" : v == Voice::alto ? "ALTO" : v == Voice::tenor ? "TENOR" : "BASS";
    }

    juce::String voiceId (Voice v)
    {
        return voiceName (v).toLowerCase();
    }

    Voice voiceFromId (const juce::String& id)
    {
        return id == "alto" ? Voice::alto : id == "tenor" ? Voice::tenor : id == "bass" ? Voice::bass : Voice::soprano;
    }

    juce::String voicingLibraryName (VoicingSettings::Library l)
    {
        return l == Lib::piano ? "PIANO" : l == Lib::guitar ? "GUITAR" : "BASIC";
    }

    juce::String pianoStyleName (VoicingSettings::PianoStyle p)
    {
        switch (p)
        {
            case PS::triads:       return "TRIADS";
            case PS::triadsNine:   return "TRIADS WITH 9 (NO ROOT)";
            case PS::fourNote:     return "4-NOTE CHORDS (NO ROOT)";
            case PS::fourNoteOpen: return "4-NOTE CHORDS (OPEN JAZZ)";
            case PS::fiveNote:     return "5-NOTE CHORDS";
        }
        return {};
    }

    juce::String guitarStyleName (VoicingSettings::GuitarStyle g)
    {
        return g == GS::triads ? "TRIADS" : g == GS::fourNote ? "4-NOTE CHORDS" : "3-STRING TRIADS";
    }

    juce::String voicingSummary (const VoicingSettings& s)
    {
        return voicingLibraryName (s.library) + (s.library == Lib::piano ? ": " + pianoStyleName (s.piano)
                                               : s.library == Lib::guitar ? ": " + guitarStyleName (s.guitar)
                                               : s.basicOctave == 0 ? juce::String() : ": " + juce::String (s.basicOctave > 0 ? "+" : "") + juce::String (s.basicOctave) + " OCT");
    }

    std::unique_ptr<juce::XmlElement> VoicingSettings::toXml (const juce::String& tag) const
    {
        auto x = std::make_unique<juce::XmlElement> (tag);
        x->setAttribute ("library", (int) library);
        x->setAttribute ("piano", (int) piano);
        x->setAttribute ("guitar", (int) guitar);
        x->setAttribute ("addRoot", addRoot);
        x->setAttribute ("duplicateRoot", duplicateRoot);
        x->setAttribute ("fattenUp", fattenUp);
        x->setAttribute ("lowestRoot", lowestRoot);
        x->setAttribute ("lowest", lowest);
        x->setAttribute ("highest", highest);
        x->setAttribute ("basicOctave", basicOctave);
        x->setAttribute ("adaptive", adaptive);
        return x;
    }

    VoicingSettings VoicingSettings::fromXml (const juce::XmlElement& x)
    {
        VoicingSettings s;
        s.library = (Library) juce::jlimit (0, 2, x.getIntAttribute ("library", (int) s.library));
        s.piano = (PianoStyle) juce::jlimit (0, 4, x.getIntAttribute ("piano", 0));
        s.guitar = (GuitarStyle) juce::jlimit (0, 2, x.getIntAttribute ("guitar", 0));
        s.addRoot = x.getBoolAttribute ("addRoot", true);
        s.duplicateRoot = x.getBoolAttribute ("duplicateRoot", false);
        s.fattenUp = x.getBoolAttribute ("fattenUp", false);
        s.lowestRoot = juce::jlimit (12, 84, x.getIntAttribute ("lowestRoot", s.lowestRoot));
        s.lowest = juce::jlimit (24, 96, x.getIntAttribute ("lowest", s.lowest));
        s.highest = juce::jlimit (s.lowest + 12, 108, x.getIntAttribute ("highest", s.highest));
        s.basicOctave = juce::jlimit (-3, 3, x.getIntAttribute ("basicOctave", 0));
        s.adaptive = x.getBoolAttribute ("adaptive", true);
        return s;
    }
}
