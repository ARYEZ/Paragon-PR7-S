#include "Windows.h"
#include "engine/SessionLog.h"

#include "engine/AudioAdvice.h"
#include "ui/Finish.h"
#include "ui/ParagonLookAndFeel.h"

namespace Windows
{
    namespace
    {
        const auto windowColour = juce::Colour (0xff0c0a0d);

        // The settings page: the device selector (in a viewport: with many
        // inputs it's taller than the page, and its CONTROL PANEL key, at its
        // foot, was hidden under the finish row before 0.8.5), what the buffer
        // is and how to raise it, RELOAD DRIVER, and the finish.
        class SettingsContent final : public juce::Component,
                                      private juce::ChangeListener
        {
        public:
            SettingsContent (pr7s::Studio& studioToUse, std::function<void()> toggleSpanToUse, std::function<bool()> isSpanningToUse)
                : studio (studioToUse),
                  devices (studio.getDeviceManager(), 0, 8, 0, 2, true, false, true, false),
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
                viewport.setViewedComponent (&devices, false);
                viewport.setScrollBarsShown (true, false);
                addAndMakeVisible (viewport);
                bufferText.setFont (juce::FontOptions (13.0f));
                bufferText.setColour (juce::Label::textColourId, ParagonColours::text);
                bufferText.setJustificationType (juce::Justification::topLeft);
                addAndMakeVisible (bufferText);
                reload.setButtonText ("RELOAD DRIVER");
                reload.setTooltip ("Closes the audio device and opens it again, so a buffer size set in the driver's own panel is read.");
                reload.onClick = [this] { studio.reloadAudioDevice(); refreshBuffer(); };
                addChildComponent (reload);
                finishLabel.setText ("FINISH", juce::dontSendNotification);
                finishLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
                finishLabel.setColour (juce::Label::textColourId, ParagonColours::textDim);
                addAndMakeVisible (finishLabel);
                finish.addItemList (Finish::names, 1);
                finish.setSelectedItemIndex ((int) Finish::get(), juce::dontSendNotification);
                finish.onChange = [this] { Finish::set ((Finish::Type) finish.getSelectedItemIndex()); };
                finish.setTooltip ("The unit's finish. Pearl White: a white unit with black keys in chrome surrounds. "
                                   "Pearl Black and Pearl White + Neon: black keys and knobs in neon pink-to-orange rings.");
                addAndMakeVisible (finish);
                note.setText ("Inputs: switch on the ones you record from (audio tracks count them IN 1, IN 2... in the order "
                              "they're switched on). On Windows, pick your interface's driver above: ASIO for the lowest delay "
                              "and the buffer sizes its driver offers; else Windows Audio (Exclusive Mode).",
                              juce::dontSendNotification);
                note.setFont (juce::FontOptions (12.0f));
                note.setColour (juce::Label::textColourId, ParagonColours::textDim);
                note.setJustificationType (juce::Justification::topLeft);
                addAndMakeVisible (note);
                // (0.34.5) Multi-core audio, on unless switched off here.
                multiCore.setColour (juce::ToggleButton::textColourId, ParagonColours::text);
                multiCore.setColour (juce::ToggleButton::tickColourId, ParagonColours::accent);
                multiCore.setToggleState (studio.getMultiCoreAudio(), juce::dontSendNotification);
                multiCore.setTooltip ("Tracks are processed side by side on your computer's cores, so heavy plug-ins have room. "
                                      "Off: everything on one core, as before 0.34.5, for a plug-in that misbehaves beside others. "
                                      "What you hear is the same either way.");
                multiCore.onClick = [this] { studio.setMultiCoreAudio (multiCore.getToggleState()); refreshMultiCore(); };
                refreshMultiCore();
                addAndMakeVisible (multiCore);
                studio.getDeviceManager().addChangeListener (this);
                refreshBuffer();
                setSize (600, 676);
            }

            ~SettingsContent() override { studio.getDeviceManager().removeChangeListener (this); }

            void resized() override
            {
                auto area = getLocalBounds().reduced (12);
                auto bottom = area.removeFromBottom (226);
                auto bufferRow = bottom.removeFromTop (88);
                reload.setBounds (bufferRow.removeFromRight (130).removeFromTop (28));
                bufferRow.removeFromRight (8);
                bufferText.setBounds (bufferRow);
                bottom.removeFromTop (8);
                auto finishRow = bottom.removeFromTop (28);
                finishLabel.setBounds (finishRow.removeFromLeft (120));
                finish.setBounds (finishRow.removeFromLeft (220));
                finishRow.removeFromLeft (16);
                span.setBounds (finishRow.removeFromLeft (180));
                bottom.removeFromTop (8);
                multiCore.setBounds (bottom.removeFromTop (28));
                note.setBounds (bottom.withTrimmedTop (8));
                viewport.setBounds (area.withTrimmedBottom (8));
                // The selector sets its own height; its width is the viewport's.
                devices.setSize (viewport.getWidth() - viewport.getScrollBarThickness(), juce::jmax (devices.getHeight(), 10));
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (windowColour);
                g.setColour (ParagonColours::textDim.withAlpha (0.3f));
                g.drawHorizontalLine (bufferText.getY() - 6, 12.0f, (float) getWidth() - 12.0f);
            }

            void refreshSpan()
            {
                span.setButtonText (isSpanning && isSpanning() ? "BACK TO ONE SCREEN" : "SPAN ALL SCREENS");
            }

            void refreshMultiCore()
            {
                const auto helpers = studio.getAudioHelperThreads();
                multiCore.setButtonText ("MULTI-CORE AUDIO" + juce::String (helpers > 0 ? ": tracks on " + juce::String (helpers + 1) + " cores at once"
                                                                                      : juce::String (": off, everything on one core")));
            }

            juce::String getBufferText() const { return bufferText.getText(); }
            bool isReloadShown() const { return reload.isVisible(); }
            void clickReload() { if (reload.onClick) reload.onClick(); }   // (triggerClick waits for the message loop)

        private:
            void changeListenerCallback (juce::ChangeBroadcaster*) override { refreshBuffer(); }

            void refreshBuffer()
            {
                auto* device = studio.getDeviceManager().getCurrentAudioDevice();
                if (device == nullptr)
                {
                    bufferText.setText (pr7s::AudioAdvice::bufferText ({}, 0, 0.0, {}, false), juce::dontSendNotification);
                    reload.setVisible (false);
                    return;
                }
                bufferText.setText (pr7s::AudioAdvice::bufferText (device->getTypeName(), device->getCurrentBufferSizeSamples(),
                                                                   device->getCurrentSampleRate(), device->getAvailableBufferSizes(),
                                                                   device->hasControlPanel()),
                                    juce::dontSendNotification);
                reload.setVisible (device->hasControlPanel());
            }

            pr7s::Studio& studio;
            juce::AudioDeviceSelectorComponent devices;
            juce::Viewport viewport;
            juce::Label bufferText, finishLabel, note;
            juce::TextButton reload;
            juce::ComboBox finish;
            juce::TextButton span;
            juce::ToggleButton multiCore;
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
        // (0.51.0) The cycle markers' ranges, a file each.
        for (int i = 0; i < (int) project.markerTrack.markers.size(); ++i)
            if (project.markerTrack.markers[(size_t) i].cycle)
                markerIndices.push_back (i);
        range.addItem ("Cycle markers (a file each: choose them below)", 3);
        range.setItemEnabled (3, ! markerIndices.empty());
        range.onChange = [this] { refreshLists(); };
        // (0.51.0) What: the main mix, or channels (a file each).
        channelsLabel.setText ("CHANNELS", juce::dontSendNotification);
        channelsLabel.setFont (font (13.0f));
        channelsLabel.setColour (juce::Label::textColourId, ParagonColours::textDim);
        addAndMakeVisible (channelsLabel);
        channelsMode.addItem ("The main mix (everything, through the master)", 1);
        channelsMode.addItem ("Channels: a file each (stems: choose them below)", 2);
        channelsMode.setSelectedId (1, juce::dontSendNotification);
        channelsMode.onChange = [this] { refreshLists(); };
        addAndMakeVisible (channelsMode);
        for (const auto id : studio.getExportableChannels())
        {
            const auto* t = project.findTrack (id);
            const auto kind = t->isFolder() ? " (folder group)" : t->isGroup() ? " (group)" : t->isFx() ? " (FX)" : t->isAudio() ? " (audio)" : " (instrument)";
            auto box = std::make_unique<juce::ToggleButton> (t->name + kind);
            box->setColour (juce::ToggleButton::textColourId, ParagonColours::text);
            box->setColour (juce::ToggleButton::tickColourId, ParagonColours::accent);
            box->onClick = [this] { exportButton.setEnabled (canExport()); };
            listContent.addChildComponent (*box);
            channelBoxes.push_back (std::move (box));
            channelIds.push_back (id);
        }
        for (const auto i : markerIndices)
        {
            const auto& m = project.markerTrack.markers[(size_t) i];
            auto box = std::make_unique<juce::ToggleButton> (pr7s::markerIdText (m) + "  " + (m.name.isNotEmpty() ? m.name : juce::String ("Cycle marker"))
                                                             + "  (" + bars (m.start, m.end, project) + ")");
            box->setColour (juce::ToggleButton::textColourId, ParagonColours::text);
            box->setColour (juce::ToggleButton::tickColourId, ParagonColours::accent);
            box->setToggleState (true, juce::dontSendNotification);
            box->onClick = [this] { exportButton.setEnabled (canExport()); };
            listContent.addChildComponent (*box);
            markerBoxes.push_back (std::move (box));
        }
        listView.setViewedComponent (&listContent, false);
        listView.setScrollBarsShown (true, false);
        addAndMakeVisible (listView);
        for (auto* key : { &allKey, &noneKey })
        {
            key->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
            key->setColour (juce::TextButton::textColourOffId, ParagonColours::text);
            addChildComponent (*key);
        }
        allKey.onClick = [this] { for (auto& b : channelBoxes) b->setToggleState (true, juce::dontSendNotification); exportButton.setEnabled (canExport()); };
        noneKey.onClick = [this] { for (auto& b : channelBoxes) b->setToggleState (false, juce::dontSendNotification); exportButton.setEnabled (canExport()); };
        includeMaster.setColour (juce::ToggleButton::textColourId, ParagonColours::text);
        includeMaster.setColour (juce::ToggleButton::tickColourId, ParagonColours::accent);
        addChildComponent (includeMaster);

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
                      "stops at its end. Channels: each one's own sound (its effects and fader) a file each, into a folder you choose.",
                      juce::dontSendNotification);
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
            if (isBatch())
            {
                if (onExportBatch)
                    onExportBatch (getBatch());
            }
            else if (onExport)
                onExport (getOptions());
        };
        cancelButton.onClick = [this] { if (onCancel) onCancel(); };
        setSize (620, 330);
        refreshLists();
    }

    void ExportPage::setRange (Range r)
    {
        range.setSelectedId (r == Range::song ? 1 : r == Range::cycle ? 2 : 3, juce::dontSendNotification);
        refreshLists();
    }

    void ExportPage::setChannelsMode (bool channels)
    {
        channelsMode.setSelectedId (channels ? 2 : 1, juce::dontSendNotification);
        refreshLists();
    }

    void ExportPage::setChannelChosen (int trackId, bool chosen)
    {
        for (size_t i = 0; i < channelIds.size(); ++i)
            if (channelIds[i] == trackId)
                channelBoxes[i]->setToggleState (chosen, juce::dontSendNotification);
        exportButton.setEnabled (canExport());
    }

    void ExportPage::setCycleMarkerChosen (int index, bool chosen)
    {
        for (size_t i = 0; i < markerIndices.size(); ++i)
            if (markerIndices[i] == index)
                markerBoxes[i]->setToggleState (chosen, juce::dontSendNotification);
        exportButton.setEnabled (canExport());
    }

    void ExportPage::refreshLists()
    {
        // The lists shown: the channels (stems), the cycle markers (their range chosen); the page grows to hold them.
        const auto channels = isChannelsMode(), markers = range.getSelectedId() == 3;
        click.setEnabled (! channels);
        includeMaster.setVisible (channels);
        allKey.setVisible (channels);
        noneKey.setVisible (channels);
        auto y = 0;
        const auto place = [&] (juce::ToggleButton& box, bool shown)
        {
            box.setVisible (shown);
            if (shown)
            {
                box.setBounds (4, y, 560, 22);
                y += 24;
            }
        };
        for (auto& box : channelBoxes)
            place (*box, channels);
        if (channels && markers)
            y += 8;
        for (auto& box : markerBoxes)
            place (*box, markers);
        listContent.setSize (580, y);
        listView.setVisible (y > 0);
        const auto listHeight = y > 0 ? juce::jmin (240, y + 4) : 0;
        setSize (620, 318 + (channels ? 34 : 0) + (listHeight > 0 ? listHeight + 10 : 0));
        exportButton.setEnabled (canExport());
        resized();
    }

    pr7s::Studio::ExportBatch ExportPage::getBatch() const
    {
        pr7s::Studio::ExportBatch batch;
        batch.options = getOptions();
        if (isChannelsMode())
            for (size_t i = 0; i < channelIds.size(); ++i)
                if (channelBoxes[i]->getToggleState())
                    batch.channels.push_back (channelIds[i]);
        if (range.getSelectedId() == 3)
            for (size_t i = 0; i < markerIndices.size(); ++i)
                if (markerBoxes[i]->getToggleState())
                    batch.cycleMarkers.push_back (markerIndices[i]);
        batch.includeMaster = includeMaster.getToggleState();
        return batch;
    }

    bool ExportPage::isBatch() const
    {
        return isChannelsMode() || range.getSelectedId() == 3;
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
        options.tailSeconds = range.getSelectedId() == 1 ? 30.0 : 0.0;   // the cycle, cycle markers: none (as the manual's locator export)
        return options;
    }

    bool ExportPage::canExport() const
    {
        if (range.getSelectedId() == 3)
        {
            if (std::none_of (markerBoxes.begin(), markerBoxes.end(), [] (const auto& b) { return b->getToggleState(); }))
                return false;
        }
        else
        {
            const auto options = getOptions();
            if (options.end <= options.start)
                return false;
        }
        if (isChannelsMode() && std::none_of (channelBoxes.begin(), channelBoxes.end(), [] (const auto& b) { return b->getToggleState(); }))
            return false;
        return true;
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
        labelled (row (28), channelsLabel, channelsMode);
        labelled (row (28), rangeLabel, range);
        labelled (row (28), rateLabel, rate);
        labelled (row (28), bitsLabel, bits);
        click.setBounds (row (24).withTrimmedLeft (106));
        if (includeMaster.isVisible())
        {
            auto r = row (24);
            noneKey.setBounds (r.removeFromRight (60));
            r.removeFromRight (6);
            allKey.setBounds (r.removeFromRight (60));
            includeMaster.setBounds (r.withTrimmedLeft (106));
        }
        if (listView.isVisible())
            listView.setBounds (row (juce::jmin (240, listContent.getHeight() + 4)));
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
        // (0.31.0, the owner asked) The new defaults once: 32-bit float, the original muted, disabled and
        // hidden. Kept settings from before were the old defaults (24-bit, kept), not choices.
        if (! settings->containsKey ("renderDefaults031"))
        {
            settings->setValue ("renderBits", 32);
            settings->setValue ("renderSource", (int) RO::Source::disable);
            settings->setValue ("renderHide", true);
            settings->setValue ("renderDefaults031", true);
        }
        options.mode = (RO::Mode) juce::jlimit (0, 2, settings->getIntValue ("renderMode", (int) options.mode));
        options.processing = (RO::Processing) juce::jlimit (0, 3, settings->getIntValue ("renderProcessing", (int) options.processing));
        const auto tail = juce::jlimit (0, (int) std::size (tails) - 1, settings->getIntValue ("renderTail", 1));
        options.tail = tails[tail].first;
        options.tailSize = tails[tail].second;
        const auto kept = settings->getIntValue ("renderBits", 32);
        options.bitsPerSample = kept == 16 || kept == 24 ? kept : 32;
        options.source = (RO::Source) juce::jlimit (0, 3, settings->getIntValue ("renderSource", (int) options.source));
        options.hideSource = settings->getBoolValue ("renderHide", true);
        return options;
    }

    RenderPage::RenderPage (pr7s::Studio& studioToUse, bool whole) : studio (studioToUse), wholeTracks (whole)
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
        bits.addItem ("32-bit float (nothing over 0 dB is cut off)", 32);
        bits.addItem ("24-bit", 24);
        bits.addItem ("16-bit", 16);
        source.addItem ("Keep it as it is", 1);
        source.addItem (wholeTracks ? "Mute the track" : "Mute the parts rendered", 2);
        source.addItem ("Mute and disable the track (no CPU; enable it again to change it)", 4);
        source.addItem ("Remove the track", 3);
        source.setItemEnabled (3, wholeTracks);   // (only for tracks rendered whole)
        source.setItemEnabled (4, wholeTracks);

        const auto kept = keptOptions (studio.getSettings());
        mode.setSelectedId ((int) kept.mode + 1, juce::dontSendNotification);
        processing.setSelectedId ((int) kept.processing + 1, juce::dontSendNotification);
        auto tailIndex = 0;
        for (int i = 0; i < (int) std::size (tails); ++i)
            if (tails[i].first == kept.tail && juce::approximatelyEqual (tails[i].second, kept.tailSize))
                tailIndex = i;
        tail.setSelectedId (tailIndex + 1, juce::dontSendNotification);
        bits.setSelectedId (kept.bitsPerSample, juce::dontSendNotification);
        // (A stretch of a track: disable or remove would silence the rest of it; the parts rendered are muted instead.)
        source.setSelectedId ((kept.source == RO::Source::remove || kept.source == RO::Source::disable) && ! wholeTracks ? 2 : (int) kept.source + 1,
                              juce::dontSendNotification);
        hideSource.setToggleState (kept.hideSource && wholeTracks, juce::dontSendNotification);
        hideSource.setEnabled (wholeTracks);
        hideSource.setTooltip ("As the manual's Hide Source Tracks: the track rendered goes out of the track list, the arrangement and the mixer");
        addAndMakeVisible (hideSource);
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
                if (wholeTracks)
                {
                    settings->setValue ("renderSource", source.getSelectedId() - 1);
                    settings->setValue ("renderHide", hideSource.getToggleState());
                }
            }
            if (onRender)
                onRender (getOptions());
        };
        cancelButton.onClick = [this] { if (onCancel) onCancel(); };
        warning.setFont (font (13.0f, juce::Font::plain));
        warning.setColour (juce::Label::textColourId, ParagonColours::accent);
        addChildComponent (warning);
        setSize (640, 364);
    }

    void RenderPage::setWarning (const juce::String& text)
    {
        warning.setText (text, juce::dontSendNotification);
        warning.setVisible (text.isNotEmpty());
        setSize (640, text.isNotEmpty() ? 414 : 364);
        resized();
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
        options.source = (RO::Source) juce::jlimit (0, 3, source.getSelectedId() - 1);
        options.hideSource = hideSource.getToggleState() && hideSource.isEnabled();
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
        hideSource.setBounds (row (24).withTrimmedLeft (120));
        if (warning.isVisible())
            warning.setBounds (row (40));
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
        {
            const pr7s::SessionLog::Step step ("Opening the window of " + instance.getName() + " (" + trackName + ")");
            if (auto* editor = instance.createEditorAndMakeActive())
                setContentOwned (editor, true);
        }
        pluginName = instance.getName();
        setResizable (instance.hasEditor() && getContentComponent() != nullptr
                          && dynamic_cast<juce::AudioProcessorEditor*> (getContentComponent())->isResizable(),
                      false);
        // (0.52.1, the owner asked) Centred on the right-hand screen (the owner's third); one screen: that one.
        setBounds (rightmostScreenArea().withSizeKeepingCentre (getWidth(), getHeight()));
        setVisible (true);
        timerCallback();
        startTimer (300);
    }

    juce::Rectangle<int> rightmostScreenArea()
    {
        const auto& displays = juce::Desktop::getInstance().getDisplays().displays;
        juce::Rectangle<int> area;
        for (const auto& d : displays)
            if (area.isEmpty() || d.userArea.getCentreX() > area.getCentreX())
                area = d.userArea;
        return area.isEmpty() ? juce::Rectangle<int> (0, 0, 1280, 800) : area;
    }

    Instrument::~Instrument()
    {
        // (0.52.1) A plug-in's window can take a while to go (or not go): a step.
        const pr7s::SessionLog::Step step ("Closing the window of " + pluginName);
        clearContentComponent();
    }

    void Instrument::timerCallback()
    {
        // Above the studio while it's in front; an ordinary window when another program is.
        const auto front = juce::Process::isForegroundProcess();
        if (isAlwaysOnTop() != front)
            setAlwaysOnTop (front);
    }

    bool Instrument::keyPressed (const juce::KeyPress& key)
    {
        return onKey != nullptr && onKey (key);
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

    namespace
    {
        SettingsContent* page (juce::DocumentWindow& window) { return dynamic_cast<SettingsContent*> (window.getContentComponent()); }
    }

    juce::String Settings::getBufferText() { return page (*this)->getBufferText(); }
    bool Settings::isReloadShown() { return page (*this)->isReloadShown(); }
    void Settings::clickReload() { page (*this)->clickReload(); }

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
        pr7s::SessionLog::note ("PLUGINS opened (scanning loads each plug-in to ask what it is)");
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
