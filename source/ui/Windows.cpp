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
                : devices (studio.getDeviceManager(), 0, 0, 0, 2, true, false, true, false)
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
                note.setText ("On Windows, pick your interface's driver above (Windows Audio, or Windows Audio (Exclusive Mode) "
                              "for the lowest delay). ASIO comes in a later version.",
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
        : DocumentWindow ("Paragon PR7-S: VST3 instruments", windowColour, DocumentWindow::closeButton)
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
