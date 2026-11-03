#include "Stretch.h"

#include "signalsmith-stretch/signalsmith-stretch.h"

#include <functional>

namespace pr7s
{
    juce::AudioBuffer<float> stretchAudio (const juce::AudioBuffer<float>& input, double sampleRate, int outputLength, double semitones)
    {
        const auto channels = juce::jmax (1, input.getNumChannels());
        juce::AudioBuffer<float> output (channels, juce::jmax (0, outputLength));
        output.clear();
        if (outputLength <= 0 || input.getNumSamples() <= 0)
            return output;
        const juce::ScopedNoDenormals noDenormals;   // (0.34.1)
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

    juce::AudioBuffer<float> shiftPitchAlong (const juce::AudioBuffer<float>& input, double sampleRate,
                                              const std::function<float (int sample)>& semitonesAt)
    {
        const auto channels = juce::jmax (1, input.getNumChannels());
        const auto length = input.getNumSamples();
        juce::AudioBuffer<float> output (channels, juce::jmax (0, length));
        output.clear();
        if (length <= 0 || input.getNumChannels() == 0)
            return output;
        const juce::ScopedNoDenormals noDenormals;   // (0.34.1)
        signalsmith::stretch::SignalsmithStretch<float> stretch;
        stretch.presetDefault (channels, (float) sampleRate);
        stretch.setFormantFactor (1.0f, true);
        const auto latency = stretch.inputLatency() + stretch.outputLatency();
        const auto total = length + latency;   // (silence after it, to bring its end out)
        juce::AudioBuffer<float> fed (channels, total), made (channels, total);
        fed.clear();
        made.clear();
        for (int c = 0; c < channels; ++c)
            fed.copyFrom (c, 0, input, juce::jmin (c, input.getNumChannels() - 1), 0, length);
        constexpr int block = 128;
        std::vector<const float*> in ((size_t) channels);
        std::vector<float*> out ((size_t) channels);
        for (int done = 0; done < total; done += block)
        {
            const auto n = juce::jmin (block, total - done);
            // The change for the sound being worked on now (the input's latency behind).
            stretch.setTransposeSemitones (semitonesAt (juce::jlimit (0, length - 1, done - stretch.inputLatency())));
            for (int c = 0; c < channels; ++c)
            {
                in[(size_t) c] = fed.getReadPointer (c, done);
                out[(size_t) c] = made.getWritePointer (c, done);
            }
            stretch.process (in.data(), n, out.data(), n);
        }
        for (int c = 0; c < channels; ++c)
            output.copyFrom (c, 0, made, c, latency, length);
        return output;
    }
}
