#include "Windows.h"

#include "ui/Finish.h"
#include "ui/ParagonLookAndFeel.h"

namespace Windows
{
    namespace
    {
        const auto windowColour = juce::Colour (0xff0c0a0d);

        // The settings page: the device selector, and the finish under it.
        class SettingsContent final : public juce::Component
        {
        public:
            explicit SettingsContent (pr7s::Studio& studio)
                : devices (studio.getDeviceManager(), 0, 8, 0, 2, true, false, true, false)
            {
                addAndMakeVisible (devices);
                finishLabel.setText ("FINISH", juce::dontSendNotification);
                finishLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
                finishLabel.setColour (juce::Label::textColourId, ParagonColours::textDim);
                addAndMakeVisible (finishLabel);
                finish.addItemList (Finish::names, 1);
                finish.setSelectedItemIndex ((int) Finish::get(), juce::dontSendNotification);
                finish.onChange = [this] { Finish::set ((Finish::Type) finish.getSelectedItemIndex()); };
                addAndMakeVisible (finish);
                note.setText ("Inputs: switch on the ones you record from (audio tracks count them IN 1, IN 2... in the order "
                              "they're switched on). On Windows, pick your interface's driver above: ASIO for the lowest delay, "
                              "when your interface has an ASIO driver; else Windows Audio (Exclusive Mode).",
                              juce::dontSendNotification);
                note.setFont (juce::FontOptions (12.0f));
                note.setColour (juce::Label::textColourId, ParagonColours::textDim);
                addAndMakeVisible (note);
                setSize (560, 520);
            }

            void resized() override
            {
                auto area = getLocalBounds().reduced (12);
                auto bottom = area.removeFromBottom (80);
                auto finishRow = bottom.removeFromTop (28);
                finishLabel.setBounds (finishRow.removeFromLeft (120));
                finish.setBounds (finishRow.removeFromLeft (220));
                note.setBounds (bottom.withTrimmedTop (8));
                devices.setBounds (area);
            }

            void paint (juce::Graphics& g) override { g.fillAll (windowColour); }

        private:
            juce::AudioDeviceSelectorComponent devices;
            juce::Label finishLabel, note;
            juce::ComboBox finish;
        };
    }

    //==========================================================================
    Instrument::Instrument (const juce::String& trackName, juce::AudioPluginInstance& instance)
        : DocumentWindow (trackName + ": " + instance.getName(), windowColour, DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        if (auto* editor = instance.createEditorAndMakeActive())
            setContentOwned (editor, true);
        setResizable (instance.hasEditor() && getContentComponent() != nullptr
                          && dynamic_cast<juce::AudioProcessorEditor*> (getContentComponent())->isResizable(),
                      false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    ZoneWindow::ZoneWindow (const juce::String& title, juce::Component& content, juce::PropertiesFile* settingsToUse,
                            const juce::String& stateKeyToUse, juce::Rectangle<int> studioOnScreen)
        : DocumentWindow (title, windowColour, DocumentWindow::allButtons), settings (settingsToUse), stateKey (stateKeyToUse)
    {
        setUsingNativeTitleBar (true);
        setContentNonOwned (&content, false);
        setResizable (true, false);
        setResizeLimits (720, 360, 32768, 16384);
        const auto saved = settings != nullptr ? settings->getValue (stateKey) : juce::String();
        if (saved.isEmpty() || ! restoreWindowStateFromString (saved))
        {
            const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (studioOnScreen);
            setBounds (display != nullptr ? display->userBounds.toNearestInt().reduced (24) : juce::Rectangle<int> (60, 60, 1400, 800));
        }
        setWantsKeyboardFocus (true);
        setVisible (true);
        toFront (true);
    }

    ZoneWindow::~ZoneWindow()
    {
        if (settings != nullptr)
        {
            settings->setValue (stateKey, getWindowStateAsString());
            settings->saveIfNeeded();
        }
        clearContentComponent();   // the content goes back to its owner, not deleted
    }

    bool ZoneWindow::keyPressed (const juce::KeyPress& key)
    {
        return onKey != nullptr && onKey (key);
    }

    void ZoneWindow::closeButtonPressed()
    {
        if (onClose)
            onClose();
    }

    void Instrument::closeButtonPressed()
    {
        if (onClose)
            onClose();
    }

    Settings::Settings (pr7s::Studio& studio)
        : DocumentWindow ("Paragon PR7-S: Settings", windowColour, DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new SettingsContent (studio), true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    void Settings::closeButtonPressed()
    {
        if (onClose)
            onClose();
    }

    Plugins::Plugins (pr7s::Studio& studio)
        : DocumentWindow ("Paragon PR7-S: VST3 instruments and effects", windowColour, DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        auto& host = studio.getPluginHost();
        auto* list = new juce::PluginListComponent (host.getFormats(), host.getKnownPlugins(), pr7s::PluginHost::deadMansPedalFile(),
                                                    studio.getSettings());
        list->setOptionsButtonText ("SCAN / OPTIONS");
        list->setSize (720, 520);
        setContentOwned (list, true);
        setResizable (true, false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    void Plugins::closeButtonPressed()
    {
        if (onClose)
            onClose();
    }
}
