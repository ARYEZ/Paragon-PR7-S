#include "Inspector.h"

#include "ui/AutomationKeys.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    juce::FontOptions font (float height, int style = juce::Font::bold) { return ParagonLookAndFeel::textFont (height, style); }

    juce::String dbText (double db) { return db <= -59.9 ? juce::String ("-inf") : (db > 0.04 ? "+" : "") + juce::String (db, 1) + " dB"; }

    juce::String panText (double pan)
    {
        const auto percent = juce::roundToInt (std::abs (pan) * 100.0);
        return percent == 0 ? juce::String ("C") : (pan < 0.0 ? "L " : "R ") + juce::String (percent);
    }

    constexpr const char* sectionNames[] = { "basics", "routing", "inserts", "sends", "fader" };
    constexpr const char* sectionTitles[] = { "", "ROUTING", "INSERTS", "SENDS", "FADER" };
    constexpr int headerHeight = 22, rowHeight = 22, gap = 4, slotHeight = 17, slotGap = 2, faderHeight = 300, margin = 6;
    const juce::Colour panelColour { 0xff0d0a0e }, slotColour { 0xff0b090c }, postFaderColour { 0xff1c1108 };
}

//==============================================================================
// A section's title (click: fold or open it). Basics' is the track's name, in its colour.
class Inspector::Header final : public juce::Component,
                                public juce::TooltipClient
{
public:
    Header (Inspector& ownerToUse, Section s) : owner (ownerToUse), section (s)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip();
    }

    void setTooltip()
    {
        tooltipText = section == Section::basics ? "The selected track: click to fold or open its basic settings"
                                                  : "Click to fold or open " + juce::String (sectionTitles[(int) section]);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto* t = owner.track();
        const auto isTrack = section == Section::basics;
        g.setColour (isTrack && t != nullptr ? t->colour.darker (0.55f) : juce::Colour (0xff17121a));
        g.fillRect (bounds);
        if (isTrack && t != nullptr)
        {
            g.setColour (t->colour);
            g.fillRect (bounds.withWidth (4.0f));
        }
        g.setColour (ParagonColours::panelEdge);
        g.fillRect (bounds.withY (bounds.getBottom() - 1.0f).withHeight (1.0f));
        // The fold arrow: down when open, right when folded.
        const auto folded = owner.isFolded (section);
        auto arrowArea = bounds.withWidth (headerHeight).translated (isTrack ? 4.0f : 0.0f, 0.0f).reduced (7.0f);
        juce::Path arrow;
        if (folded)
            arrow.addTriangle (arrowArea.getX(), arrowArea.getY(), arrowArea.getX(), arrowArea.getBottom(), arrowArea.getRight(), arrowArea.getCentreY());
        else
            arrow.addTriangle (arrowArea.getX(), arrowArea.getY(), arrowArea.getRight(), arrowArea.getY(), arrowArea.getCentreX(), arrowArea.getBottom());
        g.setColour (ParagonColours::accent);
        g.fillPath (arrow);
        g.setColour (ParagonColours::text);
        g.setFont (isTrack ? ParagonLookAndFeel::titleFont (12.5f) : font (12.0f));
        const auto title = isTrack ? (t != nullptr ? t->name : juce::String ("NO TRACK SELECTED")) : juce::String (sectionTitles[(int) section]);
        g.drawFittedText (title, getLocalBounds().withTrimmedLeft (headerHeight + (isTrack ? 6 : 2)).withTrimmedRight (4),
                          juce::Justification::centredLeft, 1, 0.8f);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains (e.getPosition()) && ! e.mods.isPopupMenu())
            owner.setFolded (section, ! owner.isFolded (section));
    }

    juce::String getTooltip() override { return tooltipText; }

private:
    Inspector& owner;
    const Section section;
    juce::String tooltipText;
};

//==============================================================================
Inspector::Inspector (pr7s::Studio& studioToUse) : studio (studioToUse)
{
    if (auto* settings = studio.getSettings())
    {
        const auto kept = juce::StringArray::fromTokens (settings->getValue ("inspectorFolded"), ",", "");
        for (int i = 0; i < numSections; ++i)
            folded[(size_t) i] = kept.contains (sectionNames[i]);
    }
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    for (int i = 0; i < numSections; ++i)
    {
        headers[(size_t) i] = std::make_unique<Header> (*this, (Section) i);
        content.addAndMakeVisible (*headers[(size_t) i]);
    }

    // BASICS
    for (auto* key : { &mute, &solo, &read, &write, &arm, &monitor })
    {
        key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key->setWantsKeyboardFocus (false);
        content.addAndMakeVisible (*key);
    }
    mute.setTooltip ("Mute this track");
    solo.setTooltip ("Solo: hear only the soloed tracks");
    arm.setTooltip ("REC: record-arm: recording and live playing go to this track");
    monitor.setTooltip ("MON: hear what comes in from the interface (your guitar), through the track's effects");
    mute.onClick = [this] { if (const auto* t = track()) studio.setMute (trackId, ! t->mute); };
    solo.onClick = [this] { if (const auto* t = track()) studio.setSolo (trackId, ! t->solo); };
    arm.onClick = [this] { if (const auto* t = track()) studio.setArmed (t->armed ? 0 : trackId); };
    monitor.onClick = [this] { if (const auto* t = track()) studio.setMonitor (trackId, ! t->monitor); };

    for (auto* slider : { &volume, &pan })
    {
        slider->setSliderStyle (juce::Slider::LinearBar);
        slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider->setColour (juce::Slider::trackColourId, ParagonColours::accent.withAlpha (0.75f));
        slider->setColour (juce::Slider::backgroundColourId, slotColour);
        slider->setVelocityModeParameters (0.2, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        slider->setWantsKeyboardFocus (false);
        content.addAndMakeVisible (*slider);
    }
    volume.setRange (-60.0, 6.0, 0.1);
    volume.setSkewFactorFromMidPoint (-12.0);
    volume.setDoubleClickReturnValue (true, 0.0, juce::ModifierKeys::commandModifier);
    volume.setTooltip ("The track's volume (Ctrl-click or double-click for 0 dB; Shift-drag for fine moves)");
    volume.onValueChange = [this]
    {
        volumeReadout.setText (dbText (volume.getValue()), juce::dontSendNotification);
        studio.setVolume (trackId, (float) volume.getValue(), ! volume.isMouseButtonDown());
    };
    volume.onDragStart = [this] { studio.grabControl (trackId, pr7s::Studio::control (pr7s::AutomationLane::Target::volume)); };
    volume.onDragEnd = [this]
    {
        studio.setVolume (trackId, (float) volume.getValue(), true);
        studio.releaseControl (trackId, pr7s::Studio::control (pr7s::AutomationLane::Target::volume));
    };
    pan.setRange (-1.0, 1.0, 0.01);
    pan.getProperties().set (ParagonLookAndFeel::barFromCentreProperty, true);
    pan.setDoubleClickReturnValue (true, 0.0, juce::ModifierKeys::commandModifier);
    pan.setTooltip ("Pan: left and right (Ctrl-click or double-click for the centre; Shift-drag for fine moves)");
    pan.onValueChange = [this]
    {
        panReadout.setText (panText (pan.getValue()), juce::dontSendNotification);
        studio.setPan (trackId, (float) pan.getValue(), ! pan.isMouseButtonDown());
    };
    pan.onDragStart = [this] { studio.grabControl (trackId, pr7s::Studio::control (pr7s::AutomationLane::Target::pan)); };
    pan.onDragEnd = [this]
    {
        studio.setPan (trackId, (float) pan.getValue(), true);
        studio.releaseControl (trackId, pr7s::Studio::control (pr7s::AutomationLane::Target::pan));
    };

    for (auto* label : { &volumeLabel, &panLabel, &midiInputLabel, &channelLabel, &audioInputLabel, &outputLabel })
    {
        label->setFont (font (11.0f));
        label->setColour (juce::Label::textColourId, ParagonColours::textDim);
        label->setInterceptsMouseClicks (false, false);
        content.addAndMakeVisible (*label);
    }
    volumeLabel.setText ("VOL", juce::dontSendNotification);
    panLabel.setText ("PAN", juce::dontSendNotification);
    midiInputLabel.setText ("MIDI IN", juce::dontSendNotification);
    channelLabel.setText ("CHANNEL", juce::dontSendNotification);
    audioInputLabel.setText ("INPUT", juce::dontSendNotification);
    outputLabel.setText ("OUT", juce::dontSendNotification);
    for (auto* readout : { &volumeReadout, &panReadout })
    {
        readout->setFont (font (11.0f));
        readout->setColour (juce::Label::textColourId, ParagonColours::text);
        readout->setJustificationType (juce::Justification::centredRight);
        readout->setInterceptsMouseClicks (false, false);
        content.addAndMakeVisible (*readout);
    }

    for (auto* button : { &instrument, &edit, &output })
    {
        button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        button->setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        button->setWantsKeyboardFocus (false);
        content.addAndMakeVisible (*button);
    }
    instrument.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    instrument.onClick = [this] { if (onTrackMenu) onTrackMenu (trackId); };
    edit.setTooltip ("Open the instrument's window");
    edit.onClick = [this] { if (onOpenInstrument) onOpenInstrument (trackId); };
    output.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    output.setTooltip ("Where its sound goes: the master, or a group channel (FX and group channels go to the master)");
    output.onClick = [this] { fader.showOutputMenu (trackId); };

    // ROUTING
    for (auto* box : { &midiInput, &channel, &audioInput })
    {
        box->setWantsKeyboardFocus (false);
        content.addAndMakeVisible (*box);
    }
    midiInput.setTooltip ("The MIDI input this track listens to when it's armed or selected: all of them, or one keyboard");
    midiInput.onChange = [this]
    {
        const auto id = midiInput.getSelectedId();
        studio.setMidiInput (trackId, id <= 1 ? juce::String() : midiInput.getProperties()["device" + juce::String (id)].toString());
    };
    for (int c = 1; c <= 16; ++c)
        channel.addItem ("CHANNEL " + juce::String (c), c);
    channel.setTooltip ("The MIDI channel its notes (and what you play into it) go to the instrument on: "
                        "for an instrument that listens on one, as a Kontakt multi's instruments do");
    channel.onChange = [this] { studio.setMidiChannel (trackId, channel.getSelectedId()); };
    audioInput.setTooltip ("The interface's input(s) it records from: one (mono) or a pair (stereo)");
    audioInput.onChange = [this]
    {
        const auto id = audioInput.getSelectedId();
        if (id >= 1000)
            studio.setTrackInput (trackId, id - 1000, true);
        else if (id >= 1)
            studio.setTrackInput (trackId, id - 1, false);
    };

    // INSERTS
    for (int i = 0; i < (int) slots.size(); ++i)
    {
        auto& slot = slots[(size_t) i];
        slot.setColour (juce::TextButton::buttonColourId, slotColour);
        slot.setWantsKeyboardFocus (false);
        slot.onClick = [this, i] { fader.showInsertMenu (trackId, i); };
        content.addAndMakeVisible (slot);
    }
    noSends.setText ("NO SENDS (AN FX CHANNEL)", juce::dontSendNotification);
    noSends.setFont (font (10.0f));
    noSends.setColour (juce::Label::textColourId, ParagonColours::textDim);
    noSends.setJustificationType (juce::Justification::centred);
    content.addChildComponent (noSends);

    // FADER
    fader.onOpenEffect = [this] (int bus, int slot) { if (onOpenEffect) onOpenEffect (bus, slot); };
    fader.onFindPlugins = [this] { if (onFindPlugins) onFindPlugins(); };
    content.addAndMakeVisible (fader);

    content.over = [this] (juce::Graphics& g)
    {
        if (insertLineY < 0)
            return;
        g.setColour (ParagonColours::accent);
        g.fillRect ((float) margin, (float) insertLineY, (float) (content.getWidth() - 2 * margin), 1.5f);
    };
    studio.addChangeListener (this);
    showTrack (studio.getSelectedTrack());
    startTimerHz (20);
}

Inspector::~Inspector()
{
    studio.removeChangeListener (this);
}

const pr7s::Track* Inspector::track() const
{
    return studio.getProject().findTrack (trackId);
}

bool Inspector::isFolded (Section section) const
{
    return folded[(size_t) section];
}

void Inspector::setFolded (Section section, bool shouldFold)
{
    folded[(size_t) section] = shouldFold;
    if (auto* settings = studio.getSettings())
    {
        juce::StringArray kept;
        for (int i = 0; i < numSections; ++i)
            if (folded[(size_t) i])
                kept.add (sectionNames[i]);
        settings->setValue ("inspectorFolded", kept.joinIntoString (","));
    }
    layout();
    for (auto& header : headers)
        header->repaint();
}

juce::Component& Inspector::getHeader (Section section)
{
    return *headers[(size_t) section];
}

juce::TextButton& Inspector::getKey (const juce::String& name)
{
    for (auto* key : { &mute, &solo, &read, &write, &arm, &monitor })
        if (key->getButtonText() == name)
            return *key;
    return mute;
}

void Inspector::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (studio.getSelectedTrack() != trackId || track() == nullptr)
        showTrack (studio.getSelectedTrack());
    else
        refresh();
}

void Inspector::showTrack (int id)
{
    trackId = studio.getProject().findTrack (id) != nullptr ? id : 0;
    AutomationKeys::setUp (read, write, studio, trackId);
    for (int i = 0; i < pr7s::Project::maxSends; ++i)
    {
        sendSlots[(size_t) i] = std::make_unique<MixerView::SendSlot> (fader, trackId, i);
        content.addAndMakeVisible (*sendSlots[(size_t) i]);
    }
    fader.setSingleTrack (trackId);
    refresh();
}

void Inspector::fillMidiInputs()
{
    // All inputs, then each connected device; one this song asks for that
    // isn't connected stays listed (and chosen), marked so.
    const auto* t = track();
    midiInput.clear (juce::dontSendNotification);
    midiInput.getProperties().clear();
    midiInput.addItem ("ALL MIDI INPUTS", 1);
    auto id = 2;
    auto chosen = 1;
    auto found = false;
    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        midiInput.addItem (device.name, id);
        midiInput.getProperties().set ("device" + juce::String (id), device.name);
        if (t != nullptr && t->midiInput == device.name)
            chosen = id, found = true;
        ++id;
    }
    if (t != nullptr && t->midiInput.isNotEmpty() && ! found)
    {
        midiInput.addItem (t->midiInput + " (NOT CONNECTED)", id);
        midiInput.getProperties().set ("device" + juce::String (id), t->midiInput);
        chosen = id;
    }
    midiInput.setSelectedId (chosen, juce::dontSendNotification);
}

void Inspector::refresh()
{
    const auto* t = track();
    for (auto& header : headers)
        header->repaint();
    if (t == nullptr)
    {
        layout();
        return;
    }
    mute.setToggleState (t->mute, juce::dontSendNotification);
    solo.setToggleState (t->solo, juce::dontSendNotification);
    arm.setToggleState (t->armed, juce::dontSendNotification);
    monitor.setToggleState (t->monitor, juce::dontSendNotification);
    AutomationKeys::refresh (read, write, studio, trackId);

    // The instrument key: its name (a menu); an FX channel's effect; a group.
    if (t->isGroup())
        instrument.setButtonText ("GROUP CHANNEL");
    else if (t->isFx())
        instrument.setButtonText (! t->inserts.empty() && ! t->inserts.front().isEmpty() ? "FX: " + t->inserts.front().plugin.name : juce::String ("FX CHANNEL"));
    else
        instrument.setButtonText (t->instrument.getName());
    instrument.setTooltip (t->isChannel() ? "Click for its menu (its effect's window, Output, delete it)"
                                          : "The instrument: click to open its window, change it, choose its Output, or delete the track");
    const auto* instance = studio.getEngine().getInstrument (trackId);
    edit.setEnabled (instance != nullptr && instance->hasEditor());

    fillMidiInputs();
    channel.setSelectedId (t->midiChannel, juce::dontSendNotification);
    // The inputs switched on in SETTINGS (two, with none to ask).
    auto inputs = 2;
    if (auto* device = studio.getDeviceManager().getCurrentAudioDevice())
        inputs = juce::jmax (1, device->getActiveInputChannels().countNumberOfSetBits());
    audioInput.clear (juce::dontSendNotification);
    for (int i = 0; i < inputs; ++i)
        audioInput.addItem ("IN " + juce::String (i + 1) + " (MONO)", 1 + i);
    for (int i = 0; i + 1 < inputs; i += 2)
        audioInput.addItem ("IN " + juce::String (i + 1) + "+" + juce::String (i + 2) + " (STEREO)", 1000 + i);
    audioInput.setSelectedId (t->stereoInput ? 1000 + t->input : 1 + t->input, juce::dontSendNotification);
    const auto* to = studio.getProject().findTrack (t->output);
    output.setButtonText (to != nullptr ? to->name : juce::String ("MASTER"));
    output.setEnabled (! t->isChannel());

    // The inserts, as the mixer's strip has them.
    const auto* inserts = studio.getInserts (trackId);
    const auto used = inserts != nullptr ? (int) inserts->size() : 0;
    shownSlots = juce::jlimit (8, pr7s::Project::maxInserts, used + 1);
    preFader = studio.getPreFaderSlots (trackId);
    for (int i = 0; i < (int) slots.size(); ++i)
    {
        auto& slot = slots[(size_t) i];
        slot.setColour (juce::TextButton::buttonColourId, i < preFader ? slotColour : postFaderColour);
        if (inserts != nullptr && i < (int) inserts->size() && ! (*inserts)[(size_t) i].isEmpty())
        {
            const auto& insert = (*inserts)[(size_t) i];
            const auto loaded = studio.getEngine().getEffect (trackId, i) != nullptr;
            slot.setButtonText (insert.plugin.name);
            slot.setColour (juce::TextButton::textColourOffId, insert.bypass || ! loaded ? ParagonColours::textDim : ParagonColours::text);
            slot.setTooltip (insert.plugin.name + (! loaded ? ": not on this computer (the sound goes past it)"
                                                            : insert.bypass ? ": bypassed. Click for its menu"
                                                                            : ". Click to open its window, bypass, replace or remove it"));
        }
        else
        {
            slot.setButtonText ("+");
            slot.setColour (juce::TextButton::textColourOffId, ParagonColours::textDim.withAlpha (0.6f));
            slot.setTooltip ("Insert " + juce::String (i + 1) + (i < preFader ? " (before the fader)" : " (after the fader)")
                             + ": add an effect (one of your VST3 effects)");
        }
    }
    for (auto& send : sendSlots)
        send->refresh();
    followMix();
    layout();
}

void Inspector::followMix()
{
    const auto* t = track();
    if (t == nullptr)
        return;
    auto db = t->volumeDb;
    auto panNow = t->pan;
    if (studio.getAutomationRead (trackId))
    {
        const auto now = studio.getEngine().getMixNow (trackId);
        db = now.volumeDb;
        panNow = now.pan;
    }
    if (! volume.isMouseButtonDown() && std::abs (volume.getValue() - db) > 0.01)
        volume.setValue (db, juce::dontSendNotification);
    if (! pan.isMouseButtonDown() && std::abs (pan.getValue() - panNow) > 0.001)
        pan.setValue (panNow, juce::dontSendNotification);
    volumeReadout.setText (dbText (volume.getValue()), juce::dontSendNotification);
    panReadout.setText (panText (pan.getValue()), juce::dontSendNotification);
}

void Inspector::timerCallback()
{
    followMix();
    AutomationKeys::refresh (read, write, studio, trackId);
    for (auto& send : sendSlots)
        send->repaint();
}

void Inspector::layout()
{
    // The sections top to bottom, each its title and (open) its controls; what
    // a track of this kind doesn't have is hidden.
    const auto* t = track();
    const auto w = juce::jmax (100, viewport.getWidth() - viewport.getScrollBarThickness());
    auto y = 0;
    std::vector<juce::Component*> shown;
    const auto place = [&] (juce::Component& c, juce::Rectangle<int> r) { c.setBounds (r); shown.push_back (&c); };
    const auto row = [&] (int height) { auto r = juce::Rectangle<int> (margin, y, w - 2 * margin, height); y += height + gap; return r; };
    const auto labelled = [&] (juce::Label& label, juce::Component& control)
    {
        auto r = row (rowHeight);
        place (label, r.removeFromLeft (58));
        place (control, r);
    };
    insertLineY = -1;

    for (int i = 0; i < numSections; ++i)
    {
        const auto section = (Section) i;
        auto& header = *headers[(size_t) i];
        const auto applies = t != nullptr && (section != Section::sends || ! t->isFx());
        if (! applies && section != Section::basics)
            continue;
        header.setBounds (0, y, w, headerHeight);
        shown.push_back (&header);
        y += headerHeight + gap;
        if (t == nullptr || folded[(size_t) i])
            continue;
        switch (section)
        {
            case Section::basics:
            {
                // M S R W (and REC, MON).
                std::vector<juce::TextButton*> keys { &mute, &solo, &read, &write };
                if (! t->isChannel())
                    keys.push_back (&arm);
                if (t->isAudio())
                    keys.push_back (&monitor);
                auto r = row (rowHeight);
                const auto keyWidth = (r.getWidth() - 3 * ((int) keys.size() - 1)) / (int) keys.size();
                for (auto* key : keys)
                {
                    place (*key, r.removeFromLeft (keyWidth));
                    r.removeFromLeft (3);
                }
                for (auto [label, slider, readout] : { std::tuple { &volumeLabel, &volume, &volumeReadout }, std::tuple { &panLabel, &pan, &panReadout } })
                {
                    auto line = row (rowHeight - 4);
                    place (*label, line.removeFromLeft (34));
                    place (*readout, line.removeFromRight (56));
                    line.removeFromRight (4);
                    place (*slider, line);
                }
                if (! t->isAudio())
                {
                    auto line = row (rowHeight + 2);
                    if (! t->isChannel())
                    {
                        place (edit, line.removeFromRight (44));
                        line.removeFromRight (4);
                    }
                    place (instrument, line);
                }
                break;
            }
            case Section::routing:
                if (t->kind == pr7s::Track::Kind::instrument)
                {
                    labelled (midiInputLabel, midiInput);
                    labelled (channelLabel, channel);
                }
                if (t->isAudio())
                    labelled (audioInputLabel, audioInput);
                labelled (outputLabel, output);
                break;
            case Section::inserts:
                for (int s = 0; s < shownSlots; ++s)
                {
                    if (s == preFader && s > 0 && ! folded[(size_t) Section::inserts])
                        insertLineY = y - slotGap / 2 - 1;
                    else if (s == preFader)
                        insertLineY = y - 2;
                    place (slots[(size_t) s], juce::Rectangle<int> (margin, y, w - 2 * margin, slotHeight));
                    y += slotHeight + slotGap;
                }
                y += gap;
                break;
            case Section::sends:
                for (auto& send : sendSlots)
                {
                    place (*send, juce::Rectangle<int> (margin, y, w - 2 * margin, slotHeight));
                    y += slotHeight + slotGap;
                }
                y += gap;
                break;
            case Section::fader:
                place (fader, juce::Rectangle<int> (0, y, w, faderHeight));
                y += faderHeight + gap;
                break;
        }
    }

    // Everything not placed is hidden.
    for (auto* child : content.getChildren())
        child->setVisible (std::find (shown.begin(), shown.end(), child) != shown.end());
    content.setSize (w, y + margin);
    content.repaint();
}

void Inspector::paint (juce::Graphics& g)
{
    g.fillAll (panelColour);
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (getWidth() - 1, 0, 1, getHeight());
    if (track() == nullptr)
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (font (12.0f, juce::Font::plain));
        g.drawFittedText ("Select a track to see its settings here", getLocalBounds().reduced (16).withTrimmedTop (headerHeight + 8),
                          juce::Justification::centredTop, 3);
    }
}

void Inspector::resized()
{
    viewport.setBounds (getLocalBounds().withTrimmedRight (1));
    layout();
}
