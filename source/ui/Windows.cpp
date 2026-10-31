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
            SettingsContent (pr7s::Studio& studio, std::function<void()> toggleSpanToUse, std::function<bool()> isSpanningToUse)
                : devices (studio.getDeviceManager(), 0, 8, 0, 2, true, false, true, false),
                  toggleSpan (std::move (toggleSpanToUse)), isSpanning (std::move (isSpanningToUse))
            {
                span.setTooltip ("The studio's window across all your screens (again: back on one). It opens where you left it next time.");
                span.onClick = [this]
                {
                    if (toggleSpan)
                        toggleSpan();
                    refreshSpan();
                };
                refreshSpan();
                addAndMakeVisible (span);
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
                finishRow.removeFromLeft (16);
                span.setBounds (finishRow.removeFromLeft (180));
                note.setBounds (bottom.withTrimmedTop (8));
                devices.setBounds (area);
            }

            void paint (juce::Graphics& g) override { g.fillAll (windowColour); }

            void refreshSpan()
            {
                span.setButtonText (isSpanning && isSpanning() ? "BACK TO ONE SCREEN" : "SPAN ALL SCREENS");
            }

        private:
            juce::AudioDeviceSelectorComponent devices;
            juce::Label finishLabel, note;
            juce::ComboBox finish;
            juce::TextButton span;
            std::function<void()> toggleSpan;
            std::function<bool()> isSpanning;
        };
    }

    //==========================================================================
    namespace
    {
        juce::FontOptions font (float height, int style = juce::Font::bold) { return ParagonLookAndFeel::textFont (height, style); }

        // The bars a stretch covers, as a musician says it: 0 to 16 beats in 4/4 is "bars 1 to 4".
        juce::String bars (double start, double end, const pr7s::Project& project)
        {
            const auto first = (int) std::floor (start / project.beatsPerBar() + 1.0e-9) + 1;
            const auto last = juce::jmax (first, (int) std::ceil (end / project.beatsPerBar() - 1.0e-9));
            return first == last ? "bar " + juce::String (first) : "bars " + juce::String (first) + " to " + juce::String (last);
        }

        juce::String kHz (double rate)
        {
            return juce::String (rate / 1000.0, juce::approximatelyEqual (std::round (rate / 1000.0), rate / 1000.0) ? 0 : 1) + " kHz";
        }
    }

    ExportPage::ExportPage (pr7s::Studio& studioToUse) : studio (studioToUse)
    {
        const auto& project = studio.getProject();
        if (studio.getEngine().getSampleRate() > 0.0)
            interfaceRate = studio.getEngine().getSampleRate();

        for (auto* label : { &rangeLabel, &rateLabel, &bitsLabel })
        {
            label->setFont (font (13.0f));
            label->setColour (juce::Label::textColourId, ParagonColours::textDim);
            addAndMakeVisible (*label);
        }
        rangeLabel.setText ("WHAT", juce::dontSendNotification);
        rateLabel.setText ("SAMPLE RATE", juce::dontSendNotification);
        bitsLabel.setText ("BITS", juce::dontSendNotification);

        const auto end = project.lastBeat();
        range.addItem ("The whole song, to the end of its last part" + (end > 0.0 ? " (" + bars (0.0, end, project) + ")" : juce::String (" (it's empty)")), 1);
        range.addItem ("The cycle (" + bars (project.loopStart, project.loopEnd, project) + ")", 2);
        range.setSelectedId (project.loopOn || end <= 0.0 ? 2 : 1, juce::dontSendNotification);
        range.onChange = [this] { exportButton.setEnabled (canExport()); };

        rate.addItem ("As the interface (" + kHz (interfaceRate) + ")", 1);
        for (int i = 0; i < (int) std::size (rates); ++i)
            rate.addItem (kHz (rates[i]), i + 2);
        bits.addItem ("24-bit (for mastering)", 24);
        bits.addItem ("16-bit (CD quality, smaller)", 16);
        bits.addItem ("32-bit float (for more work in a DAW)", 32);

        auto* settings = studio.getSettings();
        rate.setSelectedId (settings != nullptr ? juce::jlimit (1, 5, settings->getIntValue ("exportRate", 1)) : 1, juce::dontSendNotification);
        const auto keptBits = settings != nullptr ? settings->getIntValue ("exportBits", 24) : 24;
        bits.setSelectedId (keptBits == 16 || keptBits == 32 ? keptBits : 24, juce::dontSendNotification);

        click.setColour (juce::ToggleButton::textColourId, ParagonColours::text);
        click.setColour (juce::ToggleButton::tickColourId, ParagonColours::accent);
        addAndMakeVisible (click);
        for (auto* box : { &range, &rate, &bits })
            addAndMakeVisible (*box);

        note.setText ("Everything you hear (every track through its effects, then the master) as a stereo WAV, written faster "
                      "than it plays. The whole song rings out after its end until reverbs and delays die away; the cycle "
                      "stops at its end. The interface is quiet meanwhile.", juce::dontSendNotification);
        note.setFont (font (13.0f, juce::Font::plain));
        note.setColour (juce::Label::textColourId, ParagonColours::textDim);
        addAndMakeVisible (note);

        for (auto* button : { &exportButton, &cancelButton })
        {
            button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
            button->setColour (juce::TextButton::textColourOffId, ParagonColours::text);
            addAndMakeVisible (*button);
        }
        exportButton.setColour (juce::TextButton::textColourOffId, ParagonColours::accent);
        exportButton.setEnabled (canExport());
        exportButton.onClick = [this]
        {
            keep();
            if (onExport)
                onExport (getOptions());
        };
        cancelButton.onClick = [this] { if (onCancel) onCancel(); };
        setSize (560, 290);
    }

    void ExportPage::setRange (Range r)
    {
        range.setSelectedId (r == Range::song ? 1 : 2, juce::dontSendNotification);
        exportButton.setEnabled (canExport());
    }

    pr7s::Studio::ExportOptions ExportPage::getOptions() const
    {
        const auto& project = studio.getProject();
        pr7s::Studio::ExportOptions options;
        if (range.getSelectedId() == 2)
        {
            options.start = project.loopStart;
            options.end = project.loopEnd;
        }
        else
        {
            options.end = project.lastBeat();
        }
        options.sampleRate = rate.getSelectedId() >= 2 ? rates[rate.getSelectedId() - 2] : 0.0;
        options.bitsPerSample = bits.getSelectedId();
        options.click = click.getToggleState();
        options.tailSeconds = range.getSelectedId() == 2 ? 0.0 : 30.0;   // the cycle: none (as the manual's locator export)
        return options;
    }

    bool ExportPage::canExport() const
    {
        const auto options = getOptions();
        return options.end > options.start;
    }

    void ExportPage::keep()
    {
        if (auto* settings = studio.getSettings())
        {
            settings->setValue ("exportRate", rate.getSelectedId());
            settings->setValue ("exportBits", bits.getSelectedId());
            settings->saveIfNeeded();
        }
    }

    void ExportPage::resized()
    {
        auto area = getLocalBounds().reduced (16);
        auto row = [&area] (int height) { auto r = area.removeFromTop (height); area.removeFromTop (10); return r; };
        auto labelled = [] (juce::Rectangle<int> r, juce::Label& label, juce::Component& field)
        {
            label.setBounds (r.removeFromLeft (110));
            field.setBounds (r);
        };
        labelled (row (28), rangeLabel, range);
        labelled (row (28), rateLabel, rate);
        labelled (row (28), bitsLabel, bits);
        click.setBounds (row (24).withTrimmedLeft (106));
        note.setBounds (row (52));
        auto buttons = area.removeFromBottom (32);
        cancelButton.setBounds (buttons.removeFromRight (96));
        buttons.removeFromRight (8);
        exportButton.setBounds (buttons.removeFromRight (110));
    }

    void ExportPage::paint (juce::Graphics& g)
    {
        g.fillAll (windowColour);
    }

    //==========================================================================
    namespace
    {
        using RO = pr7s::Studio::RenderOptions;
        // The tails offered: none, bars, seconds.
        const std::pair<RO::Tail, double> tails[] = { { RO::Tail::off, 0.0 }, { RO::Tail::bars, 1.0 }, { RO::Tail::bars, 2.0 }, { RO::Tail::bars, 4.0 },
                                                      { RO::Tail::seconds, 1.0 }, { RO::Tail::seconds, 2.0 }, { RO::Tail::seconds, 5.0 }, { RO::Tail::seconds, 10.0 } };
    }

    RO RenderPage::keptOptions (juce::PropertiesFile* settings)
    {
        RO options;
        if (settings == nullptr)
            return options;
        options.mode = (RO::Mode) juce::jlimit (0, 2, settings->getIntValue ("renderMode", (int) options.mode));
        options.processing = (RO::Processing) juce::jlimit (0, 3, settings->getIntValue ("renderProcessing", (int) options.processing));
        const auto tail = juce::jlimit (0, (int) std::size (tails) - 1, settings->getIntValue ("renderTail", 1));
        options.tail = tails[tail].first;
        options.tailSize = tails[tail].second;
        const auto kept = settings->getIntValue ("renderBits", 24);
        options.bitsPerSample = kept == 16 || kept == 32 ? kept : 24;
        options.source = (RO::Source) juce::jlimit (0, 2, settings->getIntValue ("renderSource", (int) options.source));
        return options;
    }

    RenderPage::RenderPage (pr7s::Studio& studioToUse, bool wholeTracks) : studio (studioToUse)
    {
        for (auto* label : { &modeLabel, &processingLabel, &tailLabel, &bitsLabel, &sourceLabel })
        {
            label->setFont (font (13.0f));
            label->setColour (juce::Label::textColourId, ParagonColours::textDim);
            addAndMakeVisible (*label);
        }
        modeLabel.setText ("CLIPS", juce::dontSendNotification);
        processingLabel.setText ("WHAT'S IN IT", juce::dontSendNotification);
        tailLabel.setText ("TAIL", juce::dontSendNotification);
        bitsLabel.setText ("BITS", juce::dontSendNotification);
        sourceLabel.setText ("THE ORIGINAL", juce::dontSendNotification);

        mode.addItem ("As separate events: a clip for each part", 1);
        mode.addItem ("As block events: parts end to end joined", 2);
        mode.addItem ("As one event: one clip over them all", 3);
        processing.addItem ("Dry: the instrument alone (the new track gets its effects, fader and pan)", 1);
        processing.addItem ("Channel settings: through its effects and sends (the new track gets the fader and pan)", 2);
        processing.addItem ("Complete signal path: effects, sends, fader and pan", 3);
        processing.addItem ("Complete signal path + the master's effects and fader", 4);
        tail.addItem ("No tail", 1);
        tail.addItem ("1 bar (reverbs and delays ring on: it stops early once silent)", 2);
        tail.addItem ("2 bars", 3);
        tail.addItem ("4 bars", 4);
        tail.addItem ("1 second", 5);
        tail.addItem ("2 seconds", 6);
        tail.addItem ("5 seconds", 7);
        tail.addItem ("10 seconds", 8);
        bits.addItem ("24-bit", 24);
        bits.addItem ("16-bit", 16);
        bits.addItem ("32-bit float", 32);
        source.addItem ("Keep it as it is", 1);
        source.addItem (wholeTracks ? "Mute the track" : "Mute the parts rendered", 2);
        source.addItem ("Remove the track", 3);
        source.setItemEnabled (3, wholeTracks);   // (only for tracks rendered whole)

        const auto kept = keptOptions (studio.getSettings());
        mode.setSelectedId ((int) kept.mode + 1, juce::dontSendNotification);
        processing.setSelectedId ((int) kept.processing + 1, juce::dontSendNotification);
        auto tailIndex = 0;
        for (int i = 0; i < (int) std::size (tails); ++i)
            if (tails[i].first == kept.tail && juce::approximatelyEqual (tails[i].second, kept.tailSize))
                tailIndex = i;
        tail.setSelectedId (tailIndex + 1, juce::dontSendNotification);
        bits.setSelectedId (kept.bitsPerSample, juce::dontSendNotification);
        source.setSelectedId (kept.source == RO::Source::remove && ! wholeTracks ? 1 : (int) kept.source + 1, juce::dontSendNotification);
        for (auto* box : { &mode, &processing, &tail, &bits, &source })
            addAndMakeVisible (*box);

        note.setText ("What's chosen becomes audio files (in the song's audio folder) on a new audio track under each track, "
                      "the clips where the parts were. Faster than it plays; the interface is quiet meanwhile.",
                      juce::dontSendNotification);
        note.setFont (font (13.0f, juce::Font::plain));
        note.setColour (juce::Label::textColourId, ParagonColours::textDim);
        addAndMakeVisible (note);

        for (auto* button : { &renderButton, &cancelButton })
        {
            button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
            button->setColour (juce::TextButton::textColourOffId, ParagonColours::text);
            addAndMakeVisible (*button);
        }
        renderButton.setColour (juce::TextButton::textColourOffId, ParagonColours::accent);
        renderButton.onClick = [this]
        {
            // Kept for next time (Render with the last settings).
            if (auto* settings = studio.getSettings())
            {
                settings->setValue ("renderMode", mode.getSelectedId() - 1);
                settings->setValue ("renderProcessing", processing.getSelectedId() - 1);
                settings->setValue ("renderTail", tail.getSelectedId() - 1);
                settings->setValue ("renderBits", bits.getSelectedId());
                settings->setValue ("renderSource", source.getSelectedId() - 1);
            }
            if (onRender)
                onRender (getOptions());
        };
        cancelButton.onClick = [this] { if (onCancel) onCancel(); };
        setSize (640, 330);
    }

    RO RenderPage::getOptions() const
    {
        RO options;
        options.mode = (RO::Mode) juce::jlimit (0, 2, mode.getSelectedId() - 1);
        options.processing = (RO::Processing) juce::jlimit (0, 3, processing.getSelectedId() - 1);
        const auto& [kind, size] = tails[(size_t) juce::jlimit (0, (int) std::size (tails) - 1, tail.getSelectedId() - 1)];
        options.tail = kind;
        options.tailSize = size;
        options.bitsPerSample = bits.getSelectedId();
        options.source = (RO::Source) juce::jlimit (0, 2, source.getSelectedId() - 1);
        return options;
    }

    void RenderPage::resized()
    {
        auto area = getLocalBounds().reduced (16);
        auto row = [&area] (int height) { auto r = area.removeFromTop (height); area.removeFromTop (10); return r; };
        auto labelled = [] (juce::Rectangle<int> r, juce::Label& label, juce::Component& field)
        {
            label.setBounds (r.removeFromLeft (120));
            field.setBounds (r);
        };
        labelled (row (28), modeLabel, mode);
        labelled (row (28), processingLabel, processing);
        labelled (row (28), tailLabel, tail);
        labelled (row (28), bitsLabel, bits);
        labelled (row (28), sourceLabel, source);
        note.setBounds (row (40));
        auto buttons = area.removeFromBottom (32);
        cancelButton.setBounds (buttons.removeFromRight (96));
        buttons.removeFromRight (8);
        renderButton.setBounds (buttons.removeFromRight (110));
    }

    void RenderPage::paint (juce::Graphics& g)
    {
        g.fillAll (windowColour);
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

    juce::Rectangle<int> spanOf (const juce::Array<juce::Rectangle<int>>& screens)
    {
        if (screens.isEmpty())
            return {};
        auto all = screens.getFirst();
        auto left = all.getX(), right = all.getRight(), top = all.getY(), bottom = all.getBottom();
        for (const auto& screen : screens)
        {
            all = all.getUnion (screen);
            left = juce::jmin (left, screen.getX());
            right = juce::jmax (right, screen.getRight());
            top = juce::jmax (top, screen.getY());
            bottom = juce::jmin (bottom, screen.getBottom());
        }
        // Side by side: the strip they share. Stacked (no height in common): the box round them.
        if (bottom - top < 300)
            return all;
        return { left, top, right - left, bottom - top };
    }

    juce::Rectangle<int> allScreensArea()
    {
        juce::Array<juce::Rectangle<int>> screens;
        for (const auto& display : juce::Desktop::getInstance().getDisplays().displays)
            screens.add (display.userBounds.toNearestInt());
        return spanOf (screens);
    }

    Settings::Settings (pr7s::Studio& studio, std::function<void()> toggleSpan, std::function<bool()> isSpanning)
        : DocumentWindow ("Paragon PR7-S: Settings", windowColour, DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new SettingsContent (studio, std::move (toggleSpan), std::move (isSpanning)), true);
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
