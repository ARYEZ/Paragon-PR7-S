#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

// The buffer size and the driver types (0.8.5). The buffer sizes a program can
// offer come from the driver: Windows Audio (shared) runs at the one size
// Windows sets; Exclusive Mode offers 64 to 2048; an ASIO driver reports its
// own range, and many (the AXE I/O's among them) report only the size set in
// their own control panel. So: start on ASIO when there's one, and say on the
// settings page where a bigger buffer is set.
namespace pr7s::AudioAdvice
{
    // The driver type to start on the first time (no saved settings): ASIO when
    // an ASIO driver is installed, else empty (JUCE's choice: the first type
    // with devices, Windows Audio on Windows; Core Audio, ALSA elsewhere).
    juce::String firstDeviceType (const juce::StringArray& typesWithDevices);

    // What the settings page says under the device choices: the buffer now
    // (samples and milliseconds) and, per driver type, how to get a bigger one.
    juce::String bufferText (const juce::String& typeName, int bufferSize, double sampleRate,
                             const juce::Array<int>& available, bool hasControlPanel);

    // The device manager's driver types that have a device, by name.
    juce::StringArray typesWithDevices (juce::AudioDeviceManager&);
}
