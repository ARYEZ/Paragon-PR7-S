#pragma once

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace pr7c
{
    constexpr int maxChannels = 2;

    // Switches an effect on and off without clicks by crossfading between the
    // dry signal and the effect's output over 20 ms. While fully off the
    // effect isn't run at all, so a bypassed effect costs no CPU.
    class BypassFade
    {
    public:
        void prepare (double sampleRate, int maxBlockSize)
        {
            amount.reset (sampleRate, 0.02);
            dry.setSize (maxChannels, maxBlockSize);
            ramp.resize ((size_t) maxBlockSize);
        }

        // Snap to on/off without fading: when the host restarts playback.
        void setImmediately (bool on) { amount.setCurrentAndTargetValue (on ? 1.0f : 0.0f); }

        // Switched off and finished fading out: the effect isn't heard at all.
        bool isSilent() const noexcept { return ! amount.isSmoothing() && amount.getTargetValue() <= 0.0f; }

        // Runs `processWet (block)` (which processes the block in place) and
        // blends it with the dry signal as needed.
        template <typename ProcessWet>
        void process (juce::dsp::AudioBlock<float> block, bool on, ProcessWet&& processWet)
        {
            amount.setTargetValue (on ? 1.0f : 0.0f);
            const auto numSamples = (int) block.getNumSamples();

            if (! amount.isSmoothing())
            {
                if (on)
                    processWet (block);
                return;
            }

            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
                dry.copyFrom ((int) ch, 0, block.getChannelPointer (ch), numSamples);
            blend (block, getDry (block), processWet);
        }

        // The same, with the dry signal given: for an effect that delays the
        // sound, its input delayed by as much (`FixedDelay`), so the two line
        // up while it fades. Off and finished fading, the block becomes `dry`.
        template <typename ProcessWet>
        void process (juce::dsp::AudioBlock<float> block, bool on, juce::dsp::AudioBlock<const float> dryBlock, ProcessWet&& processWet)
        {
            amount.setTargetValue (on ? 1.0f : 0.0f);
            if (! amount.isSmoothing())
            {
                if (on)
                    processWet (block);
                else
                    block.copyFrom (dryBlock);
                return;
            }
            blend (block, dryBlock, processWet);
        }

    private:
        juce::dsp::AudioBlock<const float> getDry (const juce::dsp::AudioBlock<float>& block) const
        {
            return juce::dsp::AudioBlock<const float> (dry).getSubBlock (0, block.getNumSamples()).getSubsetChannelBlock (0, block.getNumChannels());
        }

        template <typename ProcessWet>
        void blend (juce::dsp::AudioBlock<float> block, juce::dsp::AudioBlock<const float> dryBlock, ProcessWet& processWet)
        {
            const auto numSamples = (int) block.getNumSamples();
            for (int i = 0; i < numSamples; ++i)
                ramp[(size_t) i] = amount.getNextValue();

            processWet (block);

            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto* wet = block.getChannelPointer (ch);
                const auto* drySamples = dryBlock.getChannelPointer (ch);
                for (int i = 0; i < numSamples; ++i)
                    wet[i] = drySamples[i] + (wet[i] - drySamples[i]) * ramp[(size_t) i];
            }
        }

        juce::SmoothedValue<float> amount;
        juce::AudioBuffer<float> dry;
        std::vector<float> ramp;
    };

    // The last stretch of a module's input, to warm up its oversampling
    // filters at the moment it's switched on: run cold, they ring (a small
    // click). Running them on a copy all the while it's off would do too,
    // but costs CPU on every track where it's off. The filters forget
    // anything older than 256 samples (primed with the last 256, they come
    // out exactly as if they'd run all along), so 512 is plenty.
    class RecentInput
    {
    public:
        static constexpr int length = 512;

        void prepare()
        {
            recent.setSize (maxChannels, length);
            reset();
        }

        void reset() noexcept { recent.clear(); }

        // After each block: keep its last samples.
        void push (const juce::AudioBuffer<float>& audio, int numChannels, int numSamples) noexcept
        {
            const auto keep = juce::jmax (0, length - numSamples);
            for (int channel = 0; channel < numChannels; ++channel)
            {
                auto* samples = recent.getWritePointer (channel);
                std::memmove (samples, samples + (length - keep), (size_t) keep * sizeof (float));
                const auto fresh = length - keep;
                std::memcpy (samples + keep, audio.getReadPointer (channel, numSamples - fresh), (size_t) fresh * sizeof (float));
            }
        }

        // Plays the kept samples through `process` (which processes a block
        // in place), at most `scratch`'s length at a time.
        template <typename Process>
        void replay (juce::AudioBuffer<float>& scratch, int numChannels, Process&& process)
        {
            const auto chunk = scratch.getNumSamples();
            for (int start = 0; start < length; start += chunk)
            {
                const auto count = juce::jmin (chunk, length - start);
                for (int channel = 0; channel < numChannels; ++channel)
                    scratch.copyFrom (channel, 0, recent, channel, start, count);
                process (juce::dsp::AudioBlock<float> (scratch).getSubBlock (0, (size_t) count).getSubsetChannelBlock (0, (size_t) numChannels));
            }
        }

    private:
        juce::AudioBuffer<float> recent;
    };

    // A short delay of a whole number of samples, for up to two channels.
    // A module whose oversampling filters shift the sound while it's on
    // shifts it by as much while it's off, so the plugin's delay (reported
    // to the host, which lines the tracks up) never changes.
    class FixedDelay
    {
    public:
        void prepare (int samples)
        {
            length = juce::jmax (0, samples);
            history.assign ((size_t) (maxChannels * length), 0.0f);
            reset();
        }

        void reset() noexcept
        {
            std::fill (history.begin(), history.end(), 0.0f);
            position = 0;
        }

        void process (juce::dsp::AudioBlock<float> block) noexcept
        {
            if (length == 0)
                return;
            const auto numSamples = (int) block.getNumSamples();
            auto end = position;
            for (size_t ch = 0; ch < juce::jmin (block.getNumChannels(), (size_t) maxChannels); ++ch)
            {
                auto* samples = block.getChannelPointer (ch);
                auto* line = history.data() + ch * (size_t) length;
                auto at = position;
                for (int i = 0; i < numSamples; ++i)
                {
                    std::swap (samples[i], line[at]);
                    if (++at == length)
                        at = 0;
                }
                end = at;
            }
            position = end;
        }

    private:
        int length = 0, position = 0;
        std::vector<float> history;
    };

    // For delay and reverb "trails": after the effect is switched off, its
    // input is muted but the echoes or reverb keep ringing out naturally.
    // This tells the effect when the tail has died away, so it can stop
    // running and save CPU.
    class TailWatch
    {
    public:
        // `silentSeconds` must be longer than any silent gap inside a tail
        // (for a delay, longer than the delay time), or the tail gets cut.
        void prepare (double sampleRate, double silentSeconds) { silentSamplesNeeded = (int) (sampleRate * silentSeconds); }
        void reset() { silentSamples = 0; }

        // Call with the effect's wet output each block while switched off.
        // Returns true once it has been silent for `silentSeconds`.
        bool isFinished (const juce::dsp::AudioBlock<const float>& wet)
        {
            float peak = 0.0f;
            for (size_t ch = 0; ch < wet.getNumChannels(); ++ch)
            {
                const auto range = juce::FloatVectorOperations::findMinAndMax (wet.getChannelPointer (ch), (int) wet.getNumSamples());
                peak = std::max ({ peak, range.getEnd(), -range.getStart() });
            }

            if (peak > silenceThreshold)
                silentSamples = 0;
            else
                silentSamples += (int) wet.getNumSamples();

            return silentSamples >= silentSamplesNeeded;
        }

    private:
        static constexpr float silenceThreshold = 1.0e-5f;   // -100 dB
        int silentSamples = 0;
        int silentSamplesNeeded = 24000;
    };

    // DUCK: turns an effect's wet signal down while you play and lets it
    // swell back in the gaps, so big delays and reverbs don't blur the
    // notes. It follows the level of the playing (fast up, a fifth of a
    // second down) and dips the wet signal by up to 24 dB at full amount.
    class Ducker
    {
    public:
        static constexpr float maxDepth = 0.937f;   // gain 0.063 at full: -24 dB

        void prepare (double sampleRate)
        {
            attack = 1.0f - std::exp (-1.0f / (0.005f * (float) sampleRate));
            release = 1.0f - std::exp (-1.0f / (0.12f * (float) sampleRate));
            reset();
        }
        void reset() noexcept { envelope = 0.0f; gain = 1.0f; }

        // Call every sample with the playing's level (0 to 1) and the DUCK
        // amount (0 to 1); returns the gain for the wet signal.
        float next (float level, float amount) noexcept
        {
            envelope += (level - envelope) * (level > envelope ? attack : release);
            // Fully ducked from about -18 dBFS of playing.
            const auto target = 1.0f - juce::jlimit (0.0f, 1.0f, amount) * maxDepth * juce::jlimit (0.0f, 1.0f, envelope * 8.0f);
            gain += (target - gain) * (target < gain ? attack : release);
            return gain;
        }

    private:
        float attack = 0.1f, release = 0.001f, envelope = 0.0f, gain = 1.0f;
    };

    // Knob "0 to 10" to a frequency, spaced evenly by ear (logarithmically).
    inline float knobToFrequency (float knob0to10, float lowHz, float highHz)
    {
        return lowHz * std::pow (highHz / lowHz, juce::jlimit (0.0f, 10.0f, knob0to10) / 10.0f);
    }
}
