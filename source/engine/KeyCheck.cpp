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
        const auto p = pcOf (pitchClass);
        const auto scale = key.scale();
        if (std::find (scale.begin(), scale.end(), p) != scale.end())
            return true;
        return key.minor && p == pcOf (key.pitchClass() + 11);
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
}
