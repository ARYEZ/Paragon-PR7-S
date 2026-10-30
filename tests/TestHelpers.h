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

// A plugin for tests that makes no noise of its own: subclasses fill in the sound.
struct TestPlugin : juce::AudioPluginInstance
{
    using AudioPluginInstance::AudioPluginInstance;
    const juce::String getName() const override { return name; }
    void fillInPluginDescription (juce::PluginDescription& d) const override { d.name = name; d.pluginFormatName = "Test"; d.isInstrument = instrument; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    using AudioPluginInstance::processBlock;
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return instrument; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& block) override { block.replaceAll (state.toRawUTF8(), state.getNumBytesAsUTF8()); }
    void setStateInformation (const void* data, int size) override { state = juce::String::fromUTF8 ((const char*) data, size); }

    juce::String name { "Test" }, state;
    bool instrument = false;
};

// An instrument that plays a steady level on each side (a DC "tone"), so levels can be read off exactly.
struct SteadyTone final : TestPlugin
{
    SteadyTone (float leftLevel, float rightLevel)
        : TestPlugin (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)), left (leftLevel), right (rightLevel)
    {
        instrument = true;
        name = "Steady Tone";
    }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        buffer.clear();
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            buffer.setSample (0, i, left);
            buffer.setSample (1, i, right);
        }
    }
    float left, right;
};

// An effect that does what it's given to each block, with `channels` in and
// out only (1: a mono effect; it refuses stereo, as a real mono plugin would).
struct TestEffect final : TestPlugin
{
    TestEffect (std::function<void (juce::AudioBuffer<float>&)> processToUse, int channelsToUse = 2, const juce::String& nameToUse = "Test Effect",
                bool* aliveFlag = nullptr)
        : TestPlugin (BusesProperties().withInput ("Input", juce::AudioChannelSet::canonicalChannelSet (channelsToUse), true)
                                       .withOutput ("Output", juce::AudioChannelSet::canonicalChannelSet (channelsToUse), true)),
          process (std::move (processToUse)), channels (channelsToUse), alive (aliveFlag)
    {
        name = nameToUse;
        if (alive != nullptr)
            *alive = true;
    }
    ~TestEffect() override
    {
        if (alive != nullptr)
            *alive = false;
    }
    bool isBusesLayoutSupported (const BusesLayout& layout) const override
    {
        return layout.getMainInputChannels() == channels && layout.getMainOutputChannels() == channels;
    }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        channelsSeen = buffer.getNumChannels();
        process (buffer);
    }
    std::function<void (juce::AudioBuffer<float>&)> process;
    int channels, channelsSeen = 0;
    bool* alive;
};

// Effects for the tests: times a number, plus a number.
inline std::unique_ptr<TestEffect> timesEffect (float gain, const juce::String& name = "Times", bool* alive = nullptr)
{
    return std::make_unique<TestEffect> ([gain] (juce::AudioBuffer<float>& b) { b.applyGain (gain); }, 2, name, alive);
}

inline std::unique_ptr<TestEffect> plusEffect (float amount, const juce::String& name = "Plus")
{
    return std::make_unique<TestEffect> ([amount] (juce::AudioBuffer<float>& b)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.addSample (c, i, amount);
    }, 2, name);
}

// Writes `sound` to a WAV file (24-bit), as a recording would be.
inline bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& sound, double sampleRate)
{
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
    if (stream == nullptr)
        return false;
    auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate)
                                                                                                 .withNumChannels (sound.getNumChannels())
                                                                                                 .withBitsPerSample (24));
    return writer != nullptr && writer->writeFromAudioSampleBuffer (sound, 0, sound.getNumSamples());
}

// A plucked-string-like take for tests and renders: notes that strike and die away.
inline juce::AudioBuffer<float> pluckedTake (double seconds, double sampleRate, int seed = 1)
{
    juce::AudioBuffer<float> sound (1, (int) (seconds * sampleRate));
    juce::Random random (seed);
    double phase = 0.0, frequency = 110.0, level = 0.0;
    for (int i = 0; i < sound.getNumSamples(); ++i)
    {
        if (i % (int) (sampleRate * 0.5) == 0)   // a new note every half second
        {
            frequency = 82.4 * std::pow (2.0, random.nextInt (12) / 12.0);
            level = 0.4 + 0.5 * random.nextDouble();
        }
        level *= 0.99985;
        phase += frequency / sampleRate;
        sound.setSample (0, i, (float) (level * (std::sin (juce::MathConstants<double>::twoPi * phase) * 0.7
                                                 + std::sin (juce::MathConstants<double>::twoPi * phase * 2.0) * 0.3)));
    }
    return sound;
}
