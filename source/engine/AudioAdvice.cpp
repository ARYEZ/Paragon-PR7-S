#include "AudioAdvice.h"

namespace pr7s::AudioAdvice
{
    juce::String firstDeviceType (const juce::StringArray& typesWithDevices)
    {
        return typesWithDevices.contains ("ASIO") ? juce::String ("ASIO") : juce::String();
    }

    juce::String bufferText (const juce::String& typeName, int bufferSize, double sampleRate,
                             const juce::Array<int>& available, bool hasControlPanel)
    {
        if (bufferSize <= 0 || sampleRate <= 0.0)
            return "BUFFER: no audio device is open.";

        const auto ms = 1000.0 * bufferSize / sampleRate;
        auto text = "BUFFER NOW: " + juce::String (bufferSize) + " samples (" + juce::String (ms, 1) + " ms at "
                  + juce::String (sampleRate / 1000.0, sampleRate == std::floor (sampleRate / 1000.0) * 1000.0 ? 0 : 1)
                  + " kHz). ";
        const auto largest = available.isEmpty() ? bufferSize : available.getLast();

        if (typeName == "Windows Audio")
            return text + "Windows Audio runs at the one size Windows sets, so it can't be raised here. "
                          "For a bigger buffer pick ASIO above (your interface's own driver), "
                          "or Windows Audio (Exclusive Mode): 64 to 2048.";

        if (typeName == "ASIO")
            return text + (available.size() > 1 ? "Your ASIO driver offers " + juce::String (available.getFirst()) + " to "
                                                      + juce::String (largest) + " here. "
                                                : juce::String ("Your ASIO driver offers only this size here. "))
                 + "Its own panel sets the rest: click CONTROL PANEL, choose the size there (2048...) and close it; "
                   "the new size is picked up (if it isn't, click RELOAD DRIVER). Changing it in the interface's "
                   "own app while PR7-S is open may not work: the driver is in use.";

        if (available.size() <= 1)
            return text + "This driver offers only this size."
                 + (hasControlPanel ? " Its CONTROL PANEL may set others." : juce::String());

        return text + "Up to " + juce::String (largest) + " here: a bigger buffer gives the plug-ins more room "
                      "(fewer dropouts), a smaller one less delay when you play.";
    }

    juce::StringArray typesWithDevices (juce::AudioDeviceManager& devices)
    {
        juce::StringArray names;
        for (auto* type : devices.getAvailableDeviceTypes())
        {
            type->scanForDevices();
            if (! type->getDeviceNames (false).isEmpty() || ! type->getDeviceNames (true).isEmpty())
                names.add (type->getTypeName());
        }
        return names;
    }
}
