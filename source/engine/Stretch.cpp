#include "Stretch.h"

#include "signalsmith-stretch/signalsmith-stretch.h"

#include <functional>

namespace pr7s
{
    juce::AudioBuffer<float> resampleAudio (const juce::AudioBuffer<float>& input, int outputLength)
    {
        const auto channels = juce::jmax (1, input.getNumChannels());
        juce::AudioBuffer<float> output (channels, juce::jmax (0, outputLength));
        output.clear();
        const auto length = input.getNumSamples();
        if (outputLength <= 0 || length <= 0 || input.getNumChannels() == 0)
            return output;
        const auto step = (double) length / (double) outputLength;
        for (int c = 0; c < channels; ++c)
        {
            const auto* in = input.getReadPointer (juce::jmin (c, input.getNumChannels() - 1));
            auto* out = output.getWritePointer (c);
            const auto at = [in, length] (int i) { return in[juce::jlimit (0, length - 1, i)]; };
            for (int i = 0; i < outputLength; ++i)
            {
                const auto x = i * step;
                const auto k = (int) std::floor (x);
                const auto t = (float) (x - k);
                const auto y0 = at (k - 1), y1 = at (k), y2 = at (k + 1), y3 = at (k + 2);
                // (Catmull-Rom: through every sample, smooth between them.)
                out[i] = y1 + 0.5f * t * (y2 - y0 + t * (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3 + t * (3.0f * (y1 - y2) + y3 - y0)));
            }
        }
        return output;
    }

    juce::AudioBuffer<float> stretchAudio (const juce::AudioBuffer<float>& input, double sampleRate, int outputLength, double semitones,
                                           int mode)
    {
        if (mode == 3)
            return resampleAudio (input, outputLength);
        const auto channels = juce::jmax (1, input.getNumChannels());
        juce::AudioBuffer<float> output (channels, juce::jmax (0, outputLength));
        output.clear();
        if (outputLength <= 0 || input.getNumSamples() <= 0)
            return output;
        const juce::ScopedNoDenormals noDenormals;   // (0.34.1)
        signalsmith::stretch::SignalsmithStretch<float> stretch;
        if (mode == 2)
            stretch.presetCheaper (channels, (float) sampleRate);
        else
            stretch.presetDefault (channels, (float) sampleRate);
        if (mode == 1)
            stretch.setFormantFactor (1.0f, true);
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
                                              const std::function<float (int sample)>& semitonesAt,
                                              const std::function<float (int sample)>& formantAt)
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
            if (formantAt)
                stretch.setFormantSemitones (formantAt (juce::jlimit (0, length - 1, done - stretch.inputLatency())), true);
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
