#include "Stretch.h"

#include "signalsmith-stretch/signalsmith-stretch.h"

namespace pr7s
{
    juce::AudioBuffer<float> stretchAudio (const juce::AudioBuffer<float>& input, double sampleRate, int outputLength, double semitones)
    {
        const auto channels = juce::jmax (1, input.getNumChannels());
        juce::AudioBuffer<float> output (channels, juce::jmax (0, outputLength));
        output.clear();
        if (outputLength <= 0 || input.getNumSamples() <= 0)
            return output;
        signalsmith::stretch::SignalsmithStretch<float> stretch;
        stretch.presetDefault (channels, (float) sampleRate);
        stretch.setTransposeSemitones ((float) semitones);
        std::vector<const float*> in ((size_t) channels);
        std::vector<float*> out ((size_t) channels);
        for (int c = 0; c < channels; ++c)
        {
            in[(size_t) c] = input.getReadPointer (c);
            out[(size_t) c] = output.getWritePointer (c);
        }
        // (exact: the whole input in, exactly outputLength out, its latency dealt with.)
        stretch.exact (in.data(), input.getNumSamples(), out.data(), outputLength);
        return output;
    }
}
