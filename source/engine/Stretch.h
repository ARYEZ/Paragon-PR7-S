#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <functional>

// Time-stretching and pitch-shifting (0.23.0: the MediaBay's previews in time with the song),
// through Signalsmith Stretch (MIT licence). Done ahead, not while playing: a loop is stretched
// whole before it's heard, so nothing heavy runs on the audio thread.
namespace pr7s
{
    // The sound stretched to `outputLength` samples (its pitch kept) and moved by `semitones`.
    // (0.187.0) `mode`: 0 CLEAN (Signalsmith's default), 1 VOICE (its formants kept as well), 2 LIGHT (its
    // cheaper preset), 3 TAPE (no stretcher: the sound resampled, so its pitch follows the speed; `semitones`
    // is ignored then).
    juce::AudioBuffer<float> stretchAudio (const juce::AudioBuffer<float>& input, double sampleRate, int outputLength, double semitones,
                                           int mode = 0);
    // TAPE: the input read faster or slower to fill `outputLength` (cubic interpolation).
    juce::AudioBuffer<float> resampleAudio (const juce::AudioBuffer<float>& input, int outputLength);
    // (0.34.0, VariAudio) The sound at its own length, its pitch moved moment by moment by
    // `semitonesAt (sample)` (worked out every 128 samples), its formants kept (a voice stays a
    // voice). Aligned with the input: the stretcher's delay taken off.
    // (0.35.0) `formantAt`: the formants moved too, by that many semitones (none: kept where they are).
    juce::AudioBuffer<float> shiftPitchAlong (const juce::AudioBuffer<float>& input, double sampleRate,
                                              const std::function<float (int sample)>& semitonesAt,
                                              const std::function<float (int sample)>& formantAt = {});
}
