#include "engine/strip/Compressor.h"

namespace pr7c
{
    namespace
    {
        float coefficientFor (double sampleRate, float seconds) noexcept
        {
            return 1.0f - std::exp (-1.0f / (juce::jmax (1.0e-5f, seconds) * (float) sampleRate));
        }

        // An RMS reading is 3 dB below a sine's peak; add it back, so the
        // threshold means the same for every character (a sine peaking at
        // -10 dBFS reads -10 dB).
        constexpr float rmsToPeakDb = 3.0103f;
    }

    float Compressor::evenFor (const Settings& settings) noexcept
    {
        if (settings.character != Character::modern)
            return 0.0f;
        return settings.distortion == Distortion::second ? 1.0f : settings.distortion == Distortion::secondAndThird ? 0.6f : 0.0f;
    }

    float Compressor::oddFor (const Settings& settings) noexcept
    {
        return settings.character == Character::modern && settings.distortion == Distortion::secondAndThird ? 1.0f : 0.0f;
    }

    float Compressor::kneeDbFor (Character character, float ratio) noexcept
    {
        switch (character)
        {
            case Character::opto: return 12.0f;
            case Character::fet:  return 3.0f;
            case Character::modern:
            {
                // The knee changes along the RATIO knob: very long and gentle
                // at the bottom (the ratio creeps up as it's driven harder),
                // shorter towards the middle, nearly hard at the top.
                constexpr std::pair<float, float> knees[] = { { 1.5f, 24.0f }, { 3.0f, 18.0f }, { 6.0f, 10.0f }, { 10.0f, 6.0f },
                                                              { 20.0f, 2.0f }, { 30.0f, 1.0f } };
                if (ratio <= knees[0].first)
                    return knees[0].second;
                for (size_t i = 1; i < std::size (knees); ++i)
                    if (ratio <= knees[i].first)
                    {
                        const auto t = std::log (ratio / knees[i - 1].first) / std::log (knees[i].first / knees[i - 1].first);
                        return knees[i - 1].second + (knees[i].second - knees[i - 1].second) * t;
                    }
                return knees[std::size (knees) - 1].second;
            }
            case Character::vca:  break;
        }
        return 1.0f;
    }

    float Compressor::gainComputerDb (float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept
    {
        const auto over = levelDb - thresholdDb;

        // SOFT: the output rises ever more slowly over the threshold, so the
        // ratio grows with level (over / out = 1 + over / 4).
        if (ratio < 1.25f)
            return over <= 0.0f ? 0.0f : 4.0f * over / (4.0f + over) - over;

        const auto slope = ratio >= infiniteRatio ? 1.0f : 1.0f - 1.0f / ratio;
        if (2.0f * over < -kneeDb)
            return 0.0f;
        if (2.0f * over <= kneeDb)
            return -slope * juce::square (over + kneeDb * 0.5f) / (2.0f * kneeDb);
        return -slope * over;
    }

    void Compressor::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        bypass.prepare (sampleRate, maxBlockSize);
        makeUp.reset (sampleRate, 0.02);
        fetGrit.reset (sampleRate, 0.02);
        modernEven.reset (sampleRate, 0.02);
        modernOdd.reset (sampleRate, 0.02);
        dcCoefficient = 1.0f - juce::MathConstants<float>::twoPi * 5.0f / (float) sampleRate;
        gritOversampling = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        gritOversampling->initProcessing ((size_t) maxBlockSize);
        for (auto* buffer : { &gritOdd, &gritEven, &gritLevel, &gritSignal })
            buffer->setSize (2, maxBlockSize);
        reset();
        fresh = true;
    }

    void Compressor::reset()
    {
        meanSquare = {};
        peak = {};
        reduction = {};
        memory = {};
        if (gritOversampling != nullptr)
            gritOversampling->reset();
        gritRunning = false;
        gritDcIn = {};
        gritDcOut = {};
        for (auto& r : reductionDb)
            r.store (0.0f);
    }

    void Compressor::process (juce::AudioBuffer<float>& audio, const juce::AudioBuffer<float>& sidechain, int numSamples,
                              const Settings& settings) noexcept
    {
        const auto numChannels = juce::jmin (2, audio.getNumChannels());
        if (fresh)
        {
            // Playback is starting: on or off from the first sample.
            fresh = false;
            bypass.setImmediately (settings.on);
            makeUp.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (settings.makeUpDb));
            fetGrit.setCurrentAndTargetValue (settings.character == Character::fet ? 1.0f : 0.0f);
            modernEven.setCurrentAndTargetValue (evenFor (settings));
            modernOdd.setCurrentAndTargetValue (oddFor (settings));
        }
        if (! settings.on && bypass.isSilent())
        {
            // Off: forget the state, so it starts fresh when switched on.
            reset();
            return;
        }

        const auto character = settings.character;
        const auto modern = character == Character::modern;
        const auto peakDetector = character == Character::fet || modern;
        const auto knee = kneeDbFor (character, settings.ratio);
        const auto rmsTime = character == Character::opto ? 0.02f : 0.005f;
        const auto attackSeconds = settings.attackMs * 0.001f * (character == Character::opto ? 2.0f : modern ? 0.5f : 1.0f);
        const auto rmsCoefficient = coefficientFor (sampleRate, rmsTime);
        const auto attack = coefficientFor (sampleRate, attackSeconds);
        const auto peakRelease = coefficientFor (sampleRate, 0.01f);
        const auto memoryCoefficient = coefficientFor (sampleRate, 1.5f);
        // MODERN's optical-style ratios (around 10:1) stretch their release too.
        const auto stretches = settings.autoRelease || character == Character::opto || (modern && settings.ratio >= 8.0f && settings.ratio <= 14.0f);
        makeUp.setTargetValue (juce::Decibels::decibelsToGain (settings.makeUpDb));
        fetGrit.setTargetValue (character == Character::fet ? 1.0f : 0.0f);
        modernEven.setTargetValue (evenFor (settings));
        modernOdd.setTargetValue (oddFor (settings));

        std::array<float, 2> most {};
        auto gritNow = fetGrit.getCurrentValue();   // FET grit's fade, this sample
        auto evenNow = modernEven.getCurrentValue(), oddNow = modernOdd.getCurrentValue();   // MODERN's distortion's fades
        auto block = juce::dsp::AudioBlock<float> (audio).getSubBlock (0, (size_t) numSamples);
        bypass.process (block, settings.on, [&] (juce::dsp::AudioBlock<float> wet)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                // What the detector hears, per channel, in dB. Both detectors
                // always run, so switching character starts from a live reading.
                std::array<float, 2> levelDb {};
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    const auto x = sidechain.getSample (juce::jmin (channel, sidechain.getNumChannels() - 1), i);
                    const auto c = (size_t) channel;
                    const auto level = std::abs (x);
                    peak[c] = level > peak[c] ? level : peak[c] + (level - peak[c]) * peakRelease;
                    meanSquare[c] += (x * x - meanSquare[c]) * rmsCoefficient;
                    levelDb[c] = peakDetector ? juce::Decibels::gainToDecibels (peak[c], -120.0f)
                                                             : 10.0f * std::log10 (meanSquare[c] + 1.0e-12f) + rmsToPeakDb;
                }
                if (settings.link && numChannels == 2)
                    levelDb[0] = levelDb[1] = juce::jmax (levelDb[0], levelDb[1]);

                const auto makeUpGain = makeUp.getNextValue();
                gritNow = fetGrit.getNextValue();
                evenNow = modernEven.getNextValue();
                oddNow = modernOdd.getNextValue();
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    const auto c = (size_t) channel;
                    const auto target = -gainComputerDb (levelDb[c], settings.thresholdDb, settings.ratio, knee);   // reduction, 0 or more

                    // The release stretches with how long and how hard it has
                    // been compressing: 0.35x after a short peak, up to 3x.
                    memory[c] += (juce::jmin (1.0f, reduction[c] / 10.0f) - memory[c]) * memoryCoefficient;
                    auto releaseSeconds = settings.releaseMs * 0.001f * (stretches ? 0.35f + 2.65f * memory[c] : 1.0f);
                    if (modern)
                        releaseSeconds /= 1.0f + reduction[c] / 6.0f;   // lets go quickly from deep compression, then slows
                    const auto release = coefficientFor (sampleRate, releaseSeconds);
                    reduction[c] += (target - reduction[c]) * (target > reduction[c] ? attack : release);
                    most[c] = juce::jmax (most[c], reduction[c]);

                    const auto gain = juce::Decibels::decibelsToGain (-reduction[c]) * makeUpGain;
                    wet.setSample (channel, i, wet.getSample (channel, i) * gain);

                    // FET grit: a little third harmonic, growing with how hard
                    // it works (about 1% at 20 dB of reduction), at any level:
                    // the waveform is measured against its own peak level, as
                    // a FET's distortion follows its gain reduction rather
                    // than the signal level. MODERN's distortion the same way:
                    // about 1% 2nd (and 1% 3rd with 2ND + 3RD) with no
                    // reduction, rising to 4% 2nd and 3% 3rd at 20 dB. Added
                    // below, at twice the rate.
                    const auto working = juce::jmin (1.0f, reduction[c] / 20.0f);
                    gritOdd.setSample (channel, i, gritNow * 0.04f * working + oddNow * (0.04f + 0.08f * working));
                    gritEven.setSample (channel, i, evenNow * (0.02f + 0.06f * working));
                    gritLevel.setSample (channel, i, peak[c] * gain + 1.0e-6f);
                }
            }

            const auto anyGrit = [] (const juce::SmoothedValue<float>& v, float now) { return now > 0.0f || v.isSmoothing() || v.getTargetValue() > 0.0f; };
            if (anyGrit (fetGrit, gritNow) || anyGrit (modernEven, evenNow) || anyGrit (modernOdd, oddNow))
                addGrit (wet, numSamples);
            else if (gritRunning)
            {
                gritOversampling->reset();
                gritRunning = false;
            }
        });

        for (int channel = 0; channel < 2; ++channel)
            reductionDb[(size_t) channel].store (settings.on ? most[(size_t) juce::jmin (channel, numChannels - 1)] : 0.0f);
    }

    void Compressor::addGrit (juce::dsp::AudioBlock<float> wet, int numSamples) noexcept
    {
        // The compressed sound, up to twice the rate; the grit worked out
        // there (a cubic: twice the rate is enough that nothing folds back);
        // then down again and added to the sound.
        const auto numChannels = wet.getNumChannels();
        for (size_t channel = 0; channel < numChannels; ++channel)
            gritSignal.copyFrom ((int) channel, 0, wet.getChannelPointer (channel), numSamples);
        auto grit = juce::dsp::AudioBlock<float> (gritSignal).getSubBlock (0, (size_t) numSamples).getSubsetChannelBlock (0, numChannels);
        auto up = gritOversampling->processSamplesUp (grit);
        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            auto* samples = up.getChannelPointer (channel);
            const auto* odd = gritOdd.getReadPointer ((int) channel);
            const auto* even = gritEven.getReadPointer ((int) channel);
            const auto* levels = gritLevel.getReadPointer ((int) channel);
            for (int i = 0; i < (int) up.getNumSamples(); ++i)
            {
                const auto y = samples[i];
                const auto shape = juce::jlimit (-1.5f, 1.5f, y / levels[i / 2]);
                // Odd: flattens the peaks (3rd). Even: lifts one side (2nd).
                samples[i] = -odd[i / 2] * y * shape * shape + even[i / 2] * levels[i / 2] * shape * shape;
            }
        }
        gritOversampling->processSamplesDown (grit);
        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            auto* added = grit.getChannelPointer (channel);
            for (int i = 0; i < numSamples; ++i)
            {
                // The even part's DC, removed below 5 Hz.
                const auto x = added[i];
                gritDcOut[channel] = x - gritDcIn[channel] + dcCoefficient * gritDcOut[channel];
                gritDcIn[channel] = x;
                added[i] = gritDcOut[channel];
            }
            juce::FloatVectorOperations::add (wet.getChannelPointer (channel), added, numSamples);
        }
        gritRunning = true;
    }
}
