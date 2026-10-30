#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "engine/Studio.h"

// The studio's other windows. Each closes with its close button and is
// deleted by its owner (MainView) when it does.
namespace Windows
{
    // An instrument's own window (its editor, as its maker drew it).
    class Instrument final : public juce::DocumentWindow
    {
    public:
        Instrument (const juce::String& trackName, juce::AudioPluginInstance&);
        void closeButtonPressed() override;
        std::function<void()> onClose;
    };

    // SETTINGS: the audio interface (which, its sample rate and buffer
    // size), the MIDI inputs, and the unit's finish.
    class Settings final : public juce::DocumentWindow
    {
    public:
        explicit Settings (pr7s::Studio&);
        void closeButtonPressed() override;
        std::function<void()> onClose;
    };

    // PLUGINS: finds the VST3 instruments on this computer (OPTIONS > Scan
    // for new or updated VST3 plug-ins) and lists them.
    class Plugins final : public juce::DocumentWindow
    {
    public:
        explicit Plugins (pr7s::Studio&);
        void closeButtonPressed() override;
        std::function<void()> onClose;
    };
}
