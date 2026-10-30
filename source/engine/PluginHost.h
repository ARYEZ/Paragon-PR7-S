#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Project.h"

namespace pr7s
{
    // Finding and loading instruments. The VST3 instruments found by a scan
    // are remembered in the studio's settings file, so the scan isn't needed
    // every time; the PLUGINS window (Windows.h) runs scans.
    class PluginHost final : private juce::ChangeListener
    {
    public:
        // `settings`: where the plugin list is kept (none: not kept, for tests).
        explicit PluginHost (juce::PropertiesFile* settings);
        ~PluginHost();

        juce::AudioPluginFormatManager& getFormats() noexcept { return formats; }
        juce::KnownPluginList& getKnownPlugins() noexcept { return known; }

        // The VST3 instruments found so far, sorted by maker then name.
        juce::Array<juce::PluginDescription> getInstruments() const;

        // Makes a track's instrument, with its saved settings put back (the
        // Paragon Synth, or the VST3 plugin; none, with `error` set, if it
        // can't be loaded, such as a project from a computer without it).
        // Message thread only.
        std::unique_ptr<juce::AudioPluginInstance> create (const Instrument&, double sampleRate, int blockSize,
                                                           juce::String& error);

        // Where a crashing plugin is noted during a scan, so it's skipped next time.
        static juce::File deadMansPedalFile();

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override;   // the list changed: keep it

        juce::AudioPluginFormatManager formats;
        juce::KnownPluginList known;
        juce::PropertiesFile* settings = nullptr;
    };
}
