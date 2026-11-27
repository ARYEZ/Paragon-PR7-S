#include "engine/strip/Gate.h"

namespace pr7c
{
    namespace
    {
        float coefficientFor (double sampleRate, float seconds) noexcept
        {
            return 1.0f - std::exp (-1.0f / (juce::jmax (1.0e-5f, seconds) * (float) sampleRate));
        }
    }

    float Gate::attackMs (Attack attack) noexcept
    {
        switch (attack)
        {
            case Attack::fast: return 0.1f;
            case Attack::slow: return 10.0f;
            case Attack::medium: break;
        }
        return 1.0f;
    }

    void Gate::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        bypass.prepare (sampleRate, maxBlockSize);
        reset();
    }

    void Gate::reset()
    {
        level = {};
        gainDb = {};
        holdLeft = {};
        open = { true, true };
        for (auto& r : reductionDb)
            r.store (0.0f);
        fresh = true;
    }

    void Gate::process (juce::AudioBuffer<float>& audio, const juce::AudioBuffer<float>& sidechain, int numSamples,
                        const Settings& settings) noexcept
    {
        const auto numChannels = juce::jmin (2, audio.getNumChannels());
        const auto holdSamples = settings.holdMs * 0.001f * (float) sampleRate;
        if (fresh)
        {
            // Playback is starting: on or off at once, and open, held for
            // the hold time, so the first sound isn't clipped while the
            // detector catches up.
            fresh = false;
            bypass.setImmediately (settings.on);
            holdLeft = { holdSamples, holdSamples };
        }
        if (! settings.on && bypass.isSilent())
        {
            level = {};
            gainDb = {};
            open = { true, true };
            for (auto& r : reductionDb)
                r.store (0.0f);
            return;
        }

        const auto range = juce::jmax (0.0f, settings.rangeDb);
        const auto levelRelease = coefficientFor (sampleRate, 0.01f);   // the detector rides over the waveform's zero crossings
        const auto attack = coefficientFor (sampleRate, attackMs (settings.attack) * 0.001f);
        const auto releaseSeconds = juce::jmax (0.001f, settings.releaseMs * 0.001f);
        const auto exponentialRelease = coefficientFor (sampleRate, releaseSeconds / 3.0f);   // ~95% of the way in RELEASE
        const auto linearStep = juce::jmax (range, 1.0f) / (releaseSeconds * (float) sampleRate);   // all the way in RELEASE
        std::array<float, 2> most {};
        auto block = juce::dsp::AudioBlock<float> (audio).getSubBlock (0, (size_t) numSamples);
        bypass.process (block, settings.on, [&] (juce::dsp::AudioBlock<float> wet)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                // What the detector hears: a peak level riding over the waveform.
                std::array<float, 2> levelDb {};
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    const auto c = (size_t) channel;
                    const auto x = std::abs (sidechain.getSample (juce::jmin (channel, sidechain.getNumChannels() - 1), i));
                    level[c] = x > level[c] ? x : level[c] + (x - level[c]) * levelRelease;
                    levelDb[c] = juce::Decibels::gainToDecibels (level[c], -150.0f);
                }
                if (settings.link && numChannels == 2)
                    levelDb[0] = levelDb[1] = juce::jmax (levelDb[0], levelDb[1]);

                for (int channel = 0; channel < numChannels; ++channel)
                {
                    const auto c = (size_t) channel;

                    // Where the gain wants to be (dB, 0 or less).
                    float target = 0.0f;
                    if (settings.expander)
                    {
                        const auto under = settings.thresholdDb - levelDb[c];
                        target = under > 0.0f ? -juce::jmin (range, under * (settings.ratio - 1.0f)) : 0.0f;
                    }
                    else
                    {
                        if (levelDb[c] > settings.thresholdDb)
                            open[c] = true;
                        else if (levelDb[c] < settings.thresholdDb - hysteresisDb)
                            open[c] = false;
                        target = open[c] ? 0.0f : -range;
                    }

                    // HOLD: once it has opened, it stays put for the hold time
                    // before it starts to close again.
                    if (target >= gainDb[c] - 0.01f)
                        holdLeft[c] = holdSamples;
                    else if (holdLeft[c] > 0.0f)
                    {
                        holdLeft[c] -= 1.0f;
                        target = gainDb[c];
                    }

                    if (target > gainDb[c])
                        gainDb[c] += (target - gainDb[c]) * attack;
                    else if (settings.linearRelease)
                        gainDb[c] = juce::jmax (target, gainDb[c] - linearStep);
                    else
                        gainDb[c] += (target - gainDb[c]) * exponentialRelease;

                    most[c] = juce::jmax (most[c], -gainDb[c]);
                    wet.setSample (channel, i, wet.getSample (channel, i) * juce::Decibels::decibelsToGain (gainDb[c], -150.0f));
                }
            }
        });

        for (int channel = 0; channel < 2; ++channel)
            reductionDb[(size_t) channel].store (settings.on ? most[(size_t) juce::jmin (channel, numChannels - 1)] : 0.0f);
    }
}
