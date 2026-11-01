#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Project.h"

#include <optional>

namespace pr7s
{
    // Finding and loading instruments. The VST3 instruments found by a scan
    // are remembered in the studio's settings file, so the scan isn't needed
    // every time; the PLUGINS window (Windows.h) runs scans.
    class PluginHost final : private juce::ChangeListener
    {
    public:
        // `settings`: where the plugin list is kept (none: not kept, for tests).
        // `findStudioSynth`: look for PR7-K (off for tests, so they hear the same synth everywhere).
        explicit PluginHost (juce::PropertiesFile* settings, bool findStudioSynth = true);
        ~PluginHost();

        juce::AudioPluginFormatManager& getFormats() noexcept { return formats; }
        juce::KnownPluginList& getKnownPlugins() noexcept { return known; }

        // The VST3 instruments found so far, sorted by maker then name; and the effects.
        juce::Array<juce::PluginDescription> getInstruments() const;
        juce::Array<juce::PluginDescription> getEffects() const;

        // Makes a track's instrument, with its saved settings put back (the
        // Paragon Synth, or the VST3 plugin; none, with `error` set, if it
        // can't be loaded, such as a project from a computer without it).
        // Message thread only.
        std::unique_ptr<juce::AudioPluginInstance> create (const Instrument&, double sampleRate, int blockSize,
                                                           juce::String& error);

        // Makes an insert's effect, with its saved settings put back (none,
        // with `error` set, if it can't be loaded). Message thread only.
        std::unique_ptr<juce::AudioPluginInstance> createEffect (const Insert&, double sampleRate, int blockSize, juce::String& error);

        // Where a crashing plugin is noted during a scan, so it's skipped next time.
        static juce::File deadMansPedalFile();

        // The studio's synth (0.9.11, the owner asked): Paragon PR7-K, the
        // family's synth, from beside the app (it ships with it) or the VST3
        // folders, the newest found. A new track plays it; a song's "built
        // in" tracks open on it. Not found (or not looked for: tests), the
        // old Paragon Synth stands in so a track never comes up silent.
        const juce::PluginDescription* getStudioSynth();
        bool isStudioSynthFound() { return getStudioSynth() != nullptr; }
        static juce::String studioSynthName();   // what the menus call it
        static juce::String describe (const juce::PluginDescription&);   // name, maker, version, file (for the session log)
        static constexpr const char* studioSynthFile = "Paragon PR7-K.vst3";

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override;   // the list changed: keep it

        juce::AudioPluginFormatManager formats;
        juce::KnownPluginList known;
        juce::PropertiesFile* settings = nullptr;
        bool findStudioSynth = true, studioSynthLooked = false;
        std::optional<juce::PluginDescription> studioSynth;
    };
}
