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
}
