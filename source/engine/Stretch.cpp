#include "Stretch.h"

#include "signalsmith-stretch/signalsmith-stretch.h"
#include "rubberband/RubberBandStretcher.h"

#include <cmath>
#include <functional>
#include <vector>

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

    juce::AudioBuffer<float> rubberBandStretch (const juce::AudioBuffer<float>& input, double sampleRate, int outputLength, double semitones)
    {
        const auto channels = juce::jmax (1, input.getNumChannels());
        juce::AudioBuffer<float> output (channels, juce::jmax (0, outputLength));
        output.clear();
        const auto length = input.getNumSamples();
        if (outputLength <= 0 || length <= 0 || input.getNumChannels() == 0)
            return output;
        const juce::ScopedNoDenormals noDenormals;
        using RB = RubberBand::RubberBandStretcher;
        auto options = RB::OptionProcessOffline | RB::OptionEngineFiner;
        if (std::abs (semitones) > 1.0e-6)
            options |= RB::OptionFormantPreserved;
        RB stretcher ((size_t) juce::jmax (1.0, sampleRate), (size_t) channels, options);
        stretcher.setTimeRatio ((double) outputLength / (double) length);
        stretcher.setPitchScale (std::pow (2.0, semitones / 12.0));
        stretcher.setExpectedInputDuration ((size_t) length);
        constexpr int block = 4096;
        stretcher.setMaxProcessSize ((size_t) block);

        // Every channel read from the input (a channel it lacks: its last one).
        std::vector<const float*> in ((size_t) channels);
        const auto feed = [&] (int from) {
            for (int c = 0; c < channels; ++c)
                in[(size_t) c] = input.getReadPointer (juce::jmin (c, input.getNumChannels() - 1), from);
        };
        // Offline: the whole sound studied, then processed; what it gives back is gathered as it comes.
        for (int done = 0; done < length; done += block)
        {
            const auto n = juce::jmin (block, length - done);
            feed (done);
            stretcher.study (in.data(), (size_t) n, done + n >= length);
        }
        std::vector<float> scratch ((size_t) channels * (size_t) block);
        std::vector<float*> out ((size_t) channels);
        int written = 0;
        const auto take = [&] {
            for (;;)
            {
                const auto ready = stretcher.available();
                if (ready <= 0)
                    return;
                const auto n = juce::jmin (ready, block);
                for (int c = 0; c < channels; ++c)
                    out[(size_t) c] = scratch.data() + (size_t) c * (size_t) block;
                const auto got = (int) stretcher.retrieve (out.data(), (size_t) n);
                if (got <= 0)
                    return;
                const auto kept = juce::jmin (got, outputLength - written);
                for (int c = 0; c < channels && kept > 0; ++c)
                    output.copyFrom (c, written, out[(size_t) c], kept);
                written += juce::jmax (0, kept);
            }
        };
        for (int done = 0; done < length; done += block)
        {
            const auto n = juce::jmin (block, length - done);
            feed (done);
            stretcher.process (in.data(), (size_t) n, done + n >= length);
            take();
        }
        take();
        // (Offline, it gives back the input's length times the ratio, give or take a sample: anything short
        // stays silent, anything over is left out, so the length is exactly the one asked for.)
        for (int c = 0; c < channels; ++c)
        {
            auto* d = output.getWritePointer (c);
            for (int i = 0; i < outputLength; ++i)
                if (! std::isfinite (d[i]))
                    d[i] = 0.0f;
        }
        return output;
    }

    juce::AudioBuffer<float> stretchAudio (const juce::AudioBuffer<float>& input, double sampleRate, int outputLength, double semitones,
                                           int mode)
    {
        if (mode == 4)
            return rubberBandStretch (input, sampleRate, outputLength, semitones);
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
