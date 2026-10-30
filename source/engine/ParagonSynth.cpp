#include "ParagonSynth.h"

namespace pr7s
{
    namespace
    {
        struct AnyNote final : juce::SynthesiserSound
        {
            bool appliesToNote (int) override { return true; }
            bool appliesToChannel (int) override { return true; }
        };

        // One voice: two saws 7 cents apart, a resonant low-pass (TPT
        // state-variable) from about 5 kHz at the strike down to 1.6 kHz as
        // the note holds, and a 4 ms / 0.5 s / 70% / 0.25 s envelope.
        class Voice final : public juce::SynthesiserVoice
        {
        public:
            bool canPlaySound (juce::SynthesiserSound* sound) override { return dynamic_cast<AnyNote*> (sound) != nullptr; }

            void startNote (int note, float velocity, juce::SynthesiserSound*, int) override
            {
                const auto hz = juce::MidiMessage::getMidiNoteInHertz (note);
                const auto rate = getSampleRate();
                // Each note starts its saws from the same place, so it sounds the same every time.
                phase[0] = 0.0;
                phase[1] = 0.37;
                increment[0] = hz * std::pow (2.0, -3.5 / 1200.0) / rate;
                increment[1] = hz * std::pow (2.0, 3.5 / 1200.0) / rate;
                level = 0.12f * (0.35f + 0.65f * velocity);   // louder the harder you play
                envelope.setSampleRate (rate);
                envelope.setParameters ({ 0.004f, 0.5f, 0.7f, 0.25f });
                envelope.noteOn();
                filterTime = 0.0f;
            }

            void stopNote (float, bool allowTailOff) override
            {
                if (allowTailOff)
                {
                    envelope.noteOff();
                }
                else
                {
                    envelope.reset();
                    clearCurrentNote();
                }
            }

            void pitchWheelMoved (int) override {}
            void controllerMoved (int, int) override {}

            void renderNextBlock (juce::AudioBuffer<float>& output, int start, int numSamples) override
            {
                if (! isVoiceActive())
                    return;
                const auto rate = (float) getSampleRate();
                for (int i = 0; i < numSamples; ++i)
                {
                    float saws = 0.0f;
                    for (int s = 0; s < 2; ++s)
                    {
                        // A band-limited saw (polyBLEP), so high notes don't whistle.
                        auto& p = phase[s];
                        const auto dt = increment[s];
                        p += dt;
                        if (p >= 1.0) p -= 1.0;
                        auto value = 2.0 * p - 1.0;
                        if (p < dt)              { const auto t = p / dt; value -= t + t - t * t - 1.0; }
                        else if (p > 1.0 - dt)   { const auto t = (p - 1.0) / dt; value -= t * t + t + t + 1.0; }
                        saws += (float) value;
                    }

                    // The filter closes over the first half second of the note.
                    filterTime += 1.0f / rate;
                    const auto cutoff = 1600.0f + 3400.0f * std::exp (-filterTime / 0.35f);
                    const auto g = std::tan (juce::MathConstants<float>::pi * juce::jmin (cutoff, 0.45f * rate) / rate);
                    constexpr float k = 1.0f / 1.2f;   // Q 1.2: a touch of resonance
                    const auto a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
                    const auto v3 = saws - ic2;
                    const auto v1 = a1 * ic1 + a2 * v3;
                    const auto v2 = ic2 + a2 * ic1 + a3 * v3;
                    ic1 = 2.0f * v1 - ic1;
                    ic2 = 2.0f * v2 - ic2;

                    const auto sample = v2 * level * envelope.getNextSample();
                    for (int channel = 0; channel < output.getNumChannels(); ++channel)
                        output.addSample (channel, start + i, sample);

                    if (! envelope.isActive())
                    {
                        clearCurrentNote();
                        ic1 = ic2 = 0.0f;
                        break;
                    }
                }
            }

        private:
            double phase[2] { 0.0, 0.37 }, increment[2] {};
            float level = 0.1f, filterTime = 0.0f, ic1 = 0.0f, ic2 = 0.0f;
            juce::ADSR envelope;
        };
    }

    ParagonSynth::ParagonSynth()
        : AudioPluginInstance (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
        for (int i = 0; i < numVoices; ++i)
            synth.addVoice (new Voice());
        synth.addSound (new AnyNote());
    }

    void ParagonSynth::fillInPluginDescription (juce::PluginDescription& description) const
    {
        description.name = getName();
        description.descriptiveName = "The Paragon PR7-S's built-in synth";
        description.pluginFormatName = "Internal";
        description.category = "Synth";
        description.manufacturerName = "Paragon";
        description.version = PR7S_VERSION;
        description.fileOrIdentifier = "paragon-synth";
        description.isInstrument = true;
        description.numInputChannels = 0;
        description.numOutputChannels = 2;
    }

    void ParagonSynth::prepareToPlay (double sampleRate, int)
    {
        synth.setCurrentPlaybackSampleRate (sampleRate);
    }

    void ParagonSynth::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        buffer.clear();
        synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
    }
}
