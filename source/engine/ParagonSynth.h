#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace pr7s
{
    // The Paragon Synth: the studio's own instrument, there before any VST3
    // instrument is installed. A warm polyphonic synth (our own design):
    // per voice two slightly detuned saws through a resonant low-pass that
    // closes from bright to warm as the note holds, with a quick attack and
    // a short release. Sixteen voices; louder the harder you play.
    //
    // It's an AudioPluginInstance, so the engine plays it exactly as it
    // plays a VST3 instrument. No settings to edit yet (no editor).
    class ParagonSynth final : public juce::AudioPluginInstance
    {
    public:
        ParagonSynth();

        static constexpr int numVoices = 16;

        const juce::String getName() const override { return "Paragon Synth"; }
        void fillInPluginDescription (juce::PluginDescription&) const override;

        void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
        using AudioPluginInstance::processBlock;

        double getTailLengthSeconds() const override { return 0.4; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }

        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return "Default"; }
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

    private:
        juce::Synthesiser synth;
    };
}
