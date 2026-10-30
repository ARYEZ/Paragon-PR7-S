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

    // The key editor or the mixer in a window of its own (their WINDOW
    // button): resizable, with a maximize button, so it can fill a screen or
    // be stretched across several. It shows its content but doesn't own it
    // (MainView does, and takes it back when this closes). Its keys go to the
    // studio's (onKey). Where it was (and whether maximized) is kept in the
    // settings file under `stateKey`, so it opens there again; the first time,
    // it fills the screen the studio is on.
    class ZoneWindow final : public juce::DocumentWindow
    {
    public:
        ZoneWindow (const juce::String& title, juce::Component& content, juce::PropertiesFile* settings, const juce::String& stateKey,
                    juce::Rectangle<int> studioOnScreen);
        ~ZoneWindow() override;
        void closeButtonPressed() override;
        bool keyPressed (const juce::KeyPress&) override;
        std::function<void()> onClose;
        std::function<bool (const juce::KeyPress&)> onKey;

    private:
        juce::PropertiesFile* settings;
        juce::String stateKey;
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

    // EXPORT: what to write the mix as (which stretch of the song, the
    // sample rate, 16, 24 or 32-bit float, the click). The whole song rings
    // out after its end until the sound has died away (up to 30 s); the
    // cycle stops at its end, as the manual's exports do. It only asks:
    // onExport gets the choices, and MainView asks where and writes it. Its
    // choices but the range are kept in the settings file for next time.
    class ExportPage final : public juce::Component
    {
    public:
        ExportPage (pr7s::Studio&);
        void resized() override;
        void paint (juce::Graphics&) override;

        enum class Range { song, cycle };
        void setRange (Range);
        pr7s::Studio::ExportOptions getOptions() const;   // as chosen now
        bool canExport() const;                           // the chosen range isn't empty

        std::function<void (const pr7s::Studio::ExportOptions&)> onExport;
        std::function<void()> onCancel;

        juce::ComboBox range, rate, bits;
        juce::ToggleButton click { "Include the click" };
        juce::TextButton exportButton { "EXPORT..." }, cancelButton { "CANCEL" };

    private:
        void keep();
        pr7s::Studio& studio;
        juce::Label rangeLabel, rateLabel, bitsLabel, note;
        double interfaceRate = 48000.0;
        static constexpr double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0 };
    };

    // PLUGINS: finds the VST3 instruments and effects on this computer
    // (OPTIONS > Scan for new or updated VST3 plug-ins) and lists them.
    class Plugins final : public juce::DocumentWindow
    {
    public:
        explicit Plugins (pr7s::Studio&);
        void closeButtonPressed() override;
        std::function<void()> onClose;
    };
}
