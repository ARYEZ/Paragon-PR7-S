#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Time-stretching and pitch-shifting (0.23.0: the MediaBay's previews in time with the song),
// through Signalsmith Stretch (MIT licence). Done ahead, not while playing: a loop is stretched
// whole before it's heard, so nothing heavy runs on the audio thread.
namespace pr7s
{
    // The sound stretched to `outputLength` samples (its pitch kept) and moved by `semitones`.
    juce::AudioBuffer<float> stretchAudio (const juce::AudioBuffer<float>& input, double sampleRate, int outputLength, double semitones);
}
