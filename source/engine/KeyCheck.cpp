#include "KeyCheck.h"
#include "RiffPaste.h"
#include "Lines.h"

namespace pr7s
{
    namespace
    {
        int pcOf (int p) { return ((p % 12) + 12) % 12; }
    }

    bool isPitchClassInKey (int pitchClass, const Key& key)
    {
        // As Chord::isInKey: in the scale, or a minor key's raised seventh (harmonic minor).
        // (0.85.2) By the scale's steps from the tonic, nothing allocated (it made the key's scale, a
        // vector, for every note of the song on every check): major 0 2 4 5 7 9 11, natural minor
        // 0 2 3 5 7 8 10 plus the raised seventh 11.
        constexpr int majorMask = (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11);
        constexpr int minorMask = (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10) | (1 << 11);
        const auto step = pcOf (pitchClass - key.pitchClass());
        return ((key.minor ? minorMask : majorMask) >> step) & 1;
    }

    int nearestInKey (int pitch, const Key& key)
    {
        for (int d = 0; d <= 6; ++d)
        {
            if (pitch - d >= 0 && isPitchClassInKey (pitch - d, key))
                return pitch - d;
            if (pitch + d <= 127 && isPitchClassInKey (pitch + d, key))
                return pitch + d;
        }
        return pitch;
    }

    bool isPitchedTrack (const Track& track)
    {
        if (track.kind != Track::Kind::instrument)
            return false;
        if (track.drumMap.isNotEmpty() || track.drumEditor || track.midiChannel == 10)
            return false;
        const auto name = track.name.toLowerCase();
        return ! (name.contains ("drum") || name.contains ("perc"));
    }

    std::vector<KeyIssue> checkKey (const Project& project, const KeyAt& keyAt)
    {
        std::vector<KeyIssue> issues;
        for (const auto& track : project.tracks)
        {
            if (! isPitchedTrack (track))
                continue;
            for (const auto& part : track.parts)
            {
                if (part.muted)
                    continue;
                for (int i = 0; i < (int) part.notes.size(); ++i)
                {
                    const auto& note = part.notes[(size_t) i];
                    if (note.muted || note.start >= part.length)
                        continue;
                    const auto beat = part.start + note.start;
                    const auto pitch = part.heardPitch (note.pitch);
                    const auto key = keyAt (beat);
                    if (isPitchClassInKey (pitch, key))
                        continue;
                    KeyIssue issue;
                    issue.trackId = track.id;
                    issue.partId = part.id;
                    issue.noteIndex = i;
                    issue.beat = beat;
                    issue.pitch = pitch;
                    issue.suggested = nearestInKey (pitch, key);
                    issue.key = key;
                    issues.push_back (issue);
                }
            }
        }
        const auto& chords = project.chordTrack.events;
        for (int i = 0; i < (int) chords.size(); ++i)
        {
            const auto key = keyAt (chords[(size_t) i].beat);
            if (chords[(size_t) i].chord.isInKey (key))
                continue;
            KeyIssue issue;
            issue.kind = KeyIssue::Kind::chord;
            issue.chordIndex = i;
            issue.beat = chords[(size_t) i].beat;
            issue.key = key;
            issue.chord = chords[(size_t) i].chord;
            issues.push_back (issue);
        }
        std::stable_sort (issues.begin(), issues.end(), [] (const KeyIssue& a, const KeyIssue& b) { return a.beat < b.beat; });
        return issues;
    }

    std::optional<Key> guessSongKey (const Project& project)
    {
        std::vector<LineNote> notes;
        for (const auto& track : project.tracks)
            if (isPitchedTrack (track))
                for (const auto& part : track.parts)
                    if (! part.muted)
                        for (const auto& n : part.notes)
                            if (! n.muted && n.start < part.length)
                                notes.push_back ({ part.start + n.start, n.length, part.heardPitch (n.pitch), part.heardVelocity (n.velocity) });
        if (notes.empty())
            return std::nullopt;
        return guessKey (notes);
    }

    KeyAt keyCheckKeys (const Project& project, std::optional<Key> chosen)
    {
        if (chosen.has_value())
            return [key = *chosen] (double) { return key; };
        if (! project.chordTrack.scales.empty())
            return [track = project.chordTrack] (double beat) { return track.keyAt (beat, track.scales.front().key); };
        const auto guessed = guessSongKey (project).value_or (Key {});
        return [guessed] (double) { return guessed; };
    }

    //==========================================================================
    namespace
    {
        const int majorSteps[7] { 0, 2, 4, 5, 7, 9, 11 };
        const int minorSteps[7] { 0, 2, 3, 5, 7, 8, 10 };
        const int* stepsOf (const Key& k) noexcept { return k.minor ? minorSteps : majorSteps; }
        int degreeOf (int relative, const Key& k) noexcept
        {
            for (int i = 0; i < 7; ++i)
                if (stepsOf (k)[i] == relative)
                    return i;
            return -1;
        }
        int relativeIn (int pitchClass, const Key& k) noexcept { return ((pitchClass - k.pitchClass()) % 12 + 12) % 12; }
    }

    int keyShiftOf (const Key& from, const Key& to) noexcept
    {
        auto d = ((to.pitchClass() - from.pitchClass()) % 12 + 12) % 12;
        return d > 5 ? d - 12 : d;
    }

    int movePitchToKey (int pitch, const Key& from, const Key& to) noexcept
    {
        auto moved = pitch + keyShiftOf (from, to);
        if (from.minor != to.minor)
        {
            const auto rel = relativeIn (pitch, from);
            if (const auto degree = degreeOf (rel, from); degree >= 0)
                moved += stepsOf (to)[degree] - rel;
        }
        return juce::jlimit (0, 127, moved);
    }

    Chord moveChordToKey (const Chord& chord, const Key& from, const Key& to)
    {
        auto out = chord;
        out.root = ((movePitchToKey (60 + chord.root, from, to)) % 12 + 12) % 12;
        if (chord.bass >= 0)
            out.bass = ((movePitchToKey (60 + chord.bass, from, to)) % 12 + 12) % 12;
        if (from.minor == to.minor)
            return out;
        // A triad on the old key's own scale: the new key's triad on that degree.
        const auto degree = degreeOf (relativeIn (chord.root, from), from);
        if (degree < 0 || (chord.type != Chord::Type::major && chord.type != Chord::Type::minor && chord.type != Chord::Type::diminished
                           && chord.type != Chord::Type::augmented))
            return out;
        const auto third = [] (const int* steps, int d) { return ((steps[(d + 2) % 7] - steps[d]) % 12 + 12) % 12; };
        const auto fifth = [] (const int* steps, int d) { return ((steps[(d + 4) % 7] - steps[d]) % 12 + 12) % 12; };
        const auto typeOf = [&] (const int* steps) -> std::optional<Chord::Type>
        {
            const auto t = third (steps, degree), f = fifth (steps, degree);
            if (t == 4 && f == 7) return Chord::Type::major;
            if (t == 3 && f == 7) return Chord::Type::minor;
            if (t == 3 && f == 6) return Chord::Type::diminished;
            if (t == 4 && f == 8) return Chord::Type::augmented;
            return std::nullopt;
        };
        if (typeOf (stepsOf (from)) == chord.type)
            if (const auto now = typeOf (stepsOf (to)))
                out.type = *now;
        return out;
    }
}
