#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

// A stand-in driver for the device tests and the settings page's render: named
// as a real type ("ASIO", "Windows Audio"), with the buffer sizes it reports
// and, like many ASIO drivers (the AXE I/O's), a size its own control panel
// sets (`panelSize`: what the driver reports after its panel was used). It
// never runs a callback.
struct FakeDriver final : juce::AudioIODeviceType
{
    struct Device final : juce::AudioIODevice
    {
        Device (FakeDriver& d) : AudioIODevice ("AXE I/O", d.getTypeName()), driver (d) {}
        juce::StringArray getOutputChannelNames() override { return { "Out 1", "Out 2" }; }
        juce::StringArray getInputChannelNames() override { return { "In 1", "In 2", "In 3", "In 4" }; }
        juce::Array<double> getAvailableSampleRates() override { return { 44100.0, 48000.0 }; }
        juce::Array<int> getAvailableBufferSizes() override { return driver.sizes(); }
        int getDefaultBufferSize() override { return driver.sizes().getLast(); }
        juce::String open (const juce::BigInteger& in, const juce::BigInteger& out, double rate, int size) override
        {
            // As JUCE's ASIO: a size the driver doesn't offer gives its own.
            inputs = in; outputs = out; sampleRate = rate;
            buffer = driver.sizes().contains (size) ? size : getDefaultBufferSize();
            opened = true;
            return {};
        }
        void close() override { opened = false; }
        bool isOpen() override { return opened; }
        void start (juce::AudioIODeviceCallback*) override { playing = true; }
        void stop() override { playing = false; }
        bool isPlaying() override { return playing; }
        juce::String getLastError() override { return {}; }
        int getCurrentBufferSizeSamples() override { return buffer; }
        double getCurrentSampleRate() override { return sampleRate; }
        int getCurrentBitDepth() override { return 24; }
        juce::BigInteger getActiveOutputChannels() const override { return outputs; }
        juce::BigInteger getActiveInputChannels() const override { return inputs; }
        int getOutputLatencyInSamples() override { return 0; }
        int getInputLatencyInSamples() override { return 0; }
        bool hasControlPanel() const override { return driver.panel; }
        bool showControlPanel() override { return false; }

        FakeDriver& driver;
        juce::BigInteger inputs, outputs;
        double sampleRate = 48000.0;
        int buffer = 0;
        bool opened = false, playing = false;
    };

    FakeDriver (const juce::String& name, juce::Array<int> reported, bool hasPanel)
        : AudioIODeviceType (name), reportedSizes (std::move (reported)), panel (hasPanel) {}

    juce::Array<int> sizes() const { return panelSize > 0 ? juce::Array<int> { panelSize } : reportedSizes; }

    void scanForDevices() override {}
    juce::StringArray getDeviceNames (bool) const override { return { "AXE I/O" }; }
    int getDefaultDeviceIndex (bool) const override { return 0; }
    int getIndexOfDevice (juce::AudioIODevice* d, bool) const override { return d != nullptr ? 0 : -1; }
    bool hasSeparateInputsAndOutputs() const override { return false; }
    juce::AudioIODevice* createDevice (const juce::String&, const juce::String&) override { return new Device (*this); }

    juce::Array<int> reportedSizes;
    bool panel;
    int panelSize = 0;
};
