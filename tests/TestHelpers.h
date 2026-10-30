#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "engine/Engine.h"

// An instrument for tests: makes no sound, but keeps every MIDI event it's
// given with the sample (counted from its first block) it arrived at.
struct Probe final : juce::AudioPluginInstance
{
    Probe() : AudioPluginInstance (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)) {}

    struct Received { juce::int64 sample; juce::MidiMessage message; };
    std::vector<Received> received;
    juce::int64 samplesSoFar = 0;

    std::vector<Received> notesOn() const
    {
        std::vector<Received> on;
        for (const auto& r : received)
            if (r.message.isNoteOn())
                on.push_back (r);
        return on;
    }
    std::vector<Received> notesOff() const
    {
        std::vector<Received> off;
        for (const auto& r : received)
            if (r.message.isNoteOff())
                off.push_back (r);
        return off;
    }

    const juce::String getName() const override { return "Probe"; }
    void fillInPluginDescription (juce::PluginDescription& d) const override { d.name = "Probe"; d.isInstrument = true; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        for (const auto metadata : midi)
            received.push_back ({ samplesSoFar + metadata.samplePosition, metadata.getMessage() });
        samplesSoFar += buffer.getNumSamples();
        buffer.clear();
    }
    using AudioPluginInstance::processBlock;
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
};

// Plays the engine for `samples`, in blocks of `blockSize`; returns the left channel.
inline std::vector<float> renderSamples (pr7s::Engine& engine, int samples, int blockSize = 512, std::vector<float>* right = nullptr)
{
    std::vector<float> left;
    juce::AudioBuffer<float> buffer (2, blockSize);
    for (int done = 0; done < samples; done += blockSize)
    {
        const auto length = juce::jmin (blockSize, samples - done);
        engine.render (buffer, length);
        left.insert (left.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + length);
        if (right != nullptr)
            right->insert (right->end(), buffer.getReadPointer (1), buffer.getReadPointer (1) + length);
    }
    return left;
}

inline float rmsOf (const std::vector<float>& signal, size_t from = 0, size_t to = 0)
{
    to = to == 0 ? signal.size() : juce::jmin (to, signal.size());
    double sum = 0.0;
    for (auto i = from; i < to; ++i)
        sum += (double) signal[i] * signal[i];
    return to > from ? (float) std::sqrt (sum / (double) (to - from)) : 0.0f;
}

// One frequency's amplitude in part of a signal (Goertzel).
inline float levelAt (const std::vector<float>& signal, double hz, double sampleRate, size_t from = 0, size_t to = 0)
{
    to = to == 0 ? signal.size() : juce::jmin (to, signal.size());
    const auto coefficient = 2.0 * std::cos (juce::MathConstants<double>::twoPi * hz / sampleRate);
    double s1 = 0.0, s2 = 0.0;
    for (auto i = from; i < to; ++i)
    {
        const auto s = signal[i] + coefficient * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    const auto n = (double) (to - from);
    return (float) (2.0 * std::sqrt (juce::jmax (0.0, s1 * s1 + s2 * s2 - coefficient * s1 * s2)) / n);
}
