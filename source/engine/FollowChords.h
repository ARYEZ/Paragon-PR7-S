#pragma once

#include "Chords.h"

// Follow Chord Track (0.36.0, the manual's "Controlling MIDI or Audio Playback
// Using the Chord Track"): a MIDI track's notes heard moved to the chord
// track's chords, so a part played over one progression plays along with
// another; Live Input (keys played mapped to the chord or scale in force), and
// Map to Chord Track (the same, written into chosen parts).
//
// The manual names the modes and says what each is for, not its rules; the
// rules here are our own:
// - Root Notes: every note moved by the chord's root movement (the short way:
//   up to 5 semitones up or 6 down), as a transpose track would.
// - Scales: moved by scale steps, as far as the chord's root moved along the
//   scale (C E G over C, in C major, becomes F A C over F: diatonic), and into
//   the scale in force (C major to A minor: by degree). A chord whose root
//   isn't in its scale (a borrowed bVII) moves exactly by its root.
// - Chords: moved by the root, then every note onto the nearest of the new
//   chord's notes (ties: the lower).
// - Chords & Scales: a chord tone stays the same chord tone (the third stays
//   the third, the seventh the seventh, if the new chord has one), the others
//   move as in Scales: "the intervals of the original chord or scale as far as
//   possible".
//
// What the notes are moved *from* is the track's reference: the chords its
// notes were played over. Follow Directly takes the chord track as it was when
// following began; Analyze Chords finds them in the notes (as Create Chord
// Events); Apply a Known Chord, one chord for everything.
namespace pr7s
{
    struct Note;

    enum class FollowMode { off, chordsAndScales, chords, scales, rootNotes };
    juce::String followModeName (FollowMode);             // "CHORDS & SCALES", "ROOT NOTES"...
    const std::vector<FollowMode>& followModes();          // in the manual's order, off first
    juce::String followModeId (FollowMode);               // for the song file: "chordsAndScales"
    FollowMode followModeFromId (const juce::String&);

    // Live Input (the manual's): off, onto the chord's notes, or onto the scale's.
    enum class LiveFollow { off, chords, scales };
    juce::String liveFollowName (LiveFollow);
    juce::String liveFollowId (LiveFollow);
    LiveFollow liveFollowFromId (const juce::String&);

    // How following began (the manual's Follow Chord Track dialog).
    enum class FollowStart { directly, analyze, knownChord };

    // The chords a track's notes were played over (saved with the track).
    struct FollowReference
    {
        std::vector<ChordEvent> chords;
        std::vector<ScaleEvent> scales;
        bool operator== (const FollowReference& o) const noexcept { return chords == o.chords && scales == o.scales; }
        bool operator!= (const FollowReference& o) const noexcept { return ! (*this == o); }
        bool isEmpty() const noexcept { return chords.empty(); }
        // The chord and the key in force at a beat (before the first: the first; none: C major).
        Chord chordAt (double beat) const;
        Key keyAt (double beat) const;
        std::unique_ptr<juce::XmlElement> toXml() const;
        static FollowReference fromXml (const juce::XmlElement&);
        static FollowReference ofChord (const Chord&);               // one chord from the start, its own key
        static FollowReference ofChordTrack (const ChordTrack&);     // as it is now (Follow Directly)
    };

    // A chord's own key, for a reference with one chord (minor chords minor; the rest major).
    Key keyOfChord (const Chord&);

    // One note moved from one chord (in its key) to another. `pitch` out of 0-127 is
    // folded back an octave at a time.
    int followPitch (int pitch, FollowMode, const Chord& from, const Key& fromKey, const Chord& to, const Key& toKey);

    // Live Input: a key played onto the nearest note of the chord (or of the key's scale);
    // ties go down. Without a chord: unchanged.
    int livePitch (int pitch, LiveFollow, const Chord& chord, const Key& key);
    // The same on the audio thread (nothing allocated): pitch classes as bits (1 << pc).
    juce::uint16 pitchClassMask (const std::vector<int>& classes) noexcept;
    int nearestInMask (int pitch, juce::uint16 mask) noexcept;

    // The chord track as a note at `beat` hears it: Shift Chords (ticks, 480 to the quarter;
    // negative: notes a little early still take the next chord) moves where each chord takes
    // over. -1: no chord yet (before the first: the first, as the chord in force).
    int chordIndexForNote (const ChordTrack&, double beat, int shiftTicks);

    // Notes of one part followed: each note's new pitch (its start + part start = its beat in
    // the song), and which notes are dropped (two landing on the same pitch at the same start:
    // the softer goes, as the manual's "some of the original MIDI notes may be muted").
    struct FollowedNote { int pitch = 0; bool dropped = false; };
    std::vector<FollowedNote> followNotes (const std::vector<Note>& notes, double partStart, FollowMode,
                                           const FollowReference&, const ChordTrack&, int shiftTicks, const Key& fallback);
}
