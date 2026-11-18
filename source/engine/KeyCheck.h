#pragma once

#include "Project.h"
#include "Chords.h"

#include <functional>
#include <optional>

// KEY CHECK (0.84.0, the owner asked: make sure everything in a song is in the right key).
//
// Every note on the song's pitched tracks (the drums skipped: their notes are drums, not pitches) and every chord on
// the chord track, checked against the key in force where it sounds. The rule is the chord track's own
// (`Chord::isInKey`): a note is in the key when it's in the key's scale, or, in a minor key, its raised seventh
// (harmonic minor), so a note passes here exactly when the chord track would pass a chord made of it.
//
// It only shows what's outside: in metal the flat second, the tritone and chromatic runs are often meant. A fix moves a
// note to the nearest note of the key (a tie: down), as one undo step, and only when asked.
namespace pr7s
{
    // The chord track's rule, for one pitch class (0-11).
    bool isPitchClassInKey (int pitchClass, const Key&);
    // The nearest pitch in the key (a tie: the lower one).
    int nearestInKey (int pitch, const Key&);
    // A track whose notes are pitches: an instrument track that isn't a drum track (a drum map, the drum editor,
    // MIDI channel 10, or "drum"/"perc" in its name).
    bool isPitchedTrack (const Track&);

    struct KeyIssue
    {
        enum class Kind { note, chord };
        Kind kind = Kind::note;
        int trackId = 0, partId = 0, noteIndex = -1, chordIndex = -1;
        double beat = 0.0;     // where it sounds (song beats)
        int pitch = 0;         // as heard (the part's transpose in)
        int suggested = 0;     // the nearest in the key (notes)
        Key key;               // the key in force there
        Chord chord;           // (chords)
    };

    using KeyAt = std::function<Key (double beat)>;
    // Everything outside the key, in time order (notes and chords together; muted parts and notes left out).
    std::vector<KeyIssue> checkKey (const Project&, const KeyAt&);
    // The song's key guessed from all its pitched notes (the riff guesser's rule); none without notes.
    std::optional<Key> guessSongKey (const Project&);
    // The key at a beat for the check: a chosen key, else the chord track's keys when it has any, else the guess.
    KeyAt keyCheckKeys (const Project&, std::optional<Key> chosen);

    // (0.114.0, the owner: "set the key of the song and all parts need to be in that key") Music moved from one key
    // to another. The tonic moves the short way (-6..+5 semitones); between major and minor each scale degree goes to
    // the new key's same degree (C major's E becomes C minor's Eb), notes outside the scale move with the tonic.
    int keyShiftOf (const Key& from, const Key& to) noexcept;
    int movePitchToKey (int pitch, const Key& from, const Key& to) noexcept;
    // A chord the same way: its root and bass as pitches are; a chord built on the old key's own scale (its triad
    // diatonic there) takes the new key's triad on that degree.
    Chord moveChordToKey (const Chord&, const Key& from, const Key& to);
}
