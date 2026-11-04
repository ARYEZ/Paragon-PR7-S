#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "Chords.h"
#include "Project.h"

// VariAudio, part one (0.34.0, the manual's Pitch Editing and Time Correction with VariAudio):
// a monophonic recording's pitch found every 5 ms, split into segments (its notes), and its
// sound made again with each segment's pitch moved, straightened or levelled. Plain functions
// on sounds: no screen, no engine.
namespace pr7s
{
    using VariSegment = Project::VariSegment;

    // The pitch every `hopSeconds` (each frame's centre at index x hop): a MIDI note number
    // (fractional; 0 where nothing pitched is heard), how clear it is (0-1) and how loud (dBFS).
    struct PitchFrame
    {
        float midi = 0.0f, clarity = 0.0f, levelDb = -120.0f;
        bool isVoiced() const noexcept { return midi > 0.0f; }
    };
    struct PitchAnalysis
    {
        double hopSeconds = 0.005;
        std::vector<PitchFrame> frames;
        // The pitch heard at a moment of the recording (0: none).
        float pitchAt (double seconds) const noexcept;
    };
    // YIN (de Cheveigné and Kawahara, 2002): the period where the sound repeats itself best,
    // between 60 Hz and 1100 Hz; both sides mixed.
    PitchAnalysis analysePitch (const juce::AudioBuffer<float>&, double sampleRate);

    // The notes: runs of pitched sound, split where the pitch settles on another note for
    // a while (a vibrato or a scoop doesn't split), none shorter than 60 ms. Each one's pitch is
    // the middle of its curve's middle part (its start and end often scoop).
    std::vector<VariSegment> segmentPitch (const PitchAnalysis&);

    // A segment's pitch at a moment as edited (its curve: moved by `shift`, its wobble around
    // its pitch scaled down by `straighten`), and how far that is from what's heard (semitones).
    float editedPitchAt (const VariSegment&, const PitchAnalysis&, double seconds);
    float pitchChangeAt (const VariSegment&, const PitchAnalysis&, double seconds);

    // How a segment's pitch snaps when it's moved (the manual's Pitch Snap Mode).
    enum class PitchSnap { absolute, relative, off };
    // Moved by `semitones` (absolute: onto a semitone; relative: by whole semitones, keeping its
    // cents; off: exactly). Returns the new shift.
    float movedShift (const VariSegment&, float semitones, PitchSnap);
    // Correct Pitch: `amount` (0-1) of the way to the nearest semitone.
    float correctedShift (const VariSegment&, float amount);
    // Onto the nearest note of a key's scale (absolute: exactly; relative: keeping its cents).
    float quantizedShift (const VariSegment&, const Key&, PitchSnap);
    // The Scale Assistant: keys whose scale holds the segments' notes best (longer notes count
    // more; the tonic heard often, and on the last note, counts more), best first.
    std::vector<Key> suggestScales (const std::vector<VariSegment>&, int count = 5);

    // The recording as edited: each edited segment's stretch (with 30 ms either side) shifted
    // in pitch moment by moment, its formants kept (a voice stays a voice), crossfaded back into
    // the recording over 10 ms; volume changes ramped over 5 ms. Unedited stretches are the
    // recording itself, sample for sample.
    juce::AudioBuffer<float> renderVariAudio (const juce::AudioBuffer<float>& recording, double sampleRate,
                                              const PitchAnalysis&, const std::vector<VariSegment>&);

    //==========================================================================
    // VariAudio, part two (0.35.0).

    // Pitch curve changes (the manual's Tilt, Rotate, Straighten range): what the tilt adds at a
    // moment (from the start's or the end's amount to none at the anchor), and how much Straighten
    // works there (inside its range, easing in over 20 ms at a range edge inside the segment).
    float tiltAt (const VariSegment&, double seconds);
    float straightenAt (const VariSegment&, double seconds);
    // Rotating (Alt on a Tilt control): the curve turned round its anchor, the start moved by
    // `semitones` (`atStart`; else the end), the other side the opposite way in proportion.
    void rotateSegment (VariSegment&, float semitones, bool atStart);
    // Formant shift: -100 to +100 % as semitones the formants move (our scale: 100 % = 12).
    inline float formantSemitones (float percent) noexcept { return percent * 0.12f; }

    // Segment editing (the manual's Segment Editing). The average pitch is found again from the
    // curve (the middle 70 %): an edited segment's pitch can change (the manual says so too).
    // Each returns false when it can't (too short, outside, overlapping) and leaves them alone.
    float averagePitch (const PitchAnalysis&, double start, double end, float fallback);
    bool splitSegment (std::vector<VariSegment>&, int index, double seconds, const PitchAnalysis&);
    bool glueSegments (std::vector<VariSegment>&, int first, int last, const PitchAnalysis&);
    bool setSegmentEdges (std::vector<VariSegment>&, int index, double start, double end, const PitchAnalysis&);
    constexpr double shortestSegment = 0.02;   // (by hand: 20 ms; the analysis makes none under 60)

    // Extract MIDI (the manual's Extract MIDI dialog): a note per segment at its pitch now, with
    // pitchbend (none, one per segment, or following the curve) and loudness (one velocity, each
    // segment's own, or a controller following the level). Times in seconds into the recording.
    struct ExtractMidiOptions
    {
        enum class Pitch { notes, staticBend, continuousBend };
        enum class Volume { fixed, dynamic, controller };
        Pitch pitch = Pitch::notes;
        int bendRange = 2;      // semitones (1-24), as the instrument must be set
        Volume volume = Volume::fixed;
        int velocity = 100;     // fixed velocity (1-127)
        int controller = 7;     // the controller curve's CC
    };
    struct ExtractedMidi
    {
        struct Note { double start, end; int pitch, velocity; };
        struct Event { double seconds; bool bend; int number, value; };   // pitchbend (0-16383) or a CC
        std::vector<Note> notes;
        std::vector<Event> events;   // in time order
    };
    ExtractedMidi extractMidi (const std::vector<VariSegment>&, const PitchAnalysis&, const ExtractMidiOptions&);

    // Harmony voices (the manual's Generate Harmony Voices): the note voice `voice` (0 soprano,
    // 1 alto, 2 tenor, 3 bass) sings against `sung`. With a chord (its pitch classes, its bass):
    // soprano the chord's next note at least a whole step up, alto the one at least a whole step
    // down, tenor the one under that, bass the chord's bass under the tenor (our voicing rules).
    // Without one: +3, -3, -6, -9 semitones (the manual's).
    int harmonyPitch (int sung, const std::vector<int>& chordPitchClasses, int bassPitchClass, int voice);
    inline const char* harmonyVoiceName (int voice) { static const char* names[] { "Soprano", "Alto", "Tenor", "Bass" }; return names[juce::jlimit (0, 3, voice)]; }
}
