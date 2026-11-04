#include "Inspector.h"
#include "FollowSetupPage.h"
#include "ui/TrackActions.h"

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

    constexpr const char* sectionNames[] = { "basics", "routing", "chords", "inserts", "sends", "fader", "quick" };
    constexpr const char* sectionTitles[] = { "", "ROUTING", "CHORDS", "INSERTS", "SENDS", "FADER", "QUICK CONTROLS" };
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
        const auto colour = owner.master ? pr7s::FamilyColours::red : t != nullptr ? t->colour : juce::Colour();
        const auto coloured = isTrack && (t != nullptr || owner.master);
        g.setColour (coloured ? colour.darker (0.55f) : juce::Colour (0xff17121a));
        g.fillRect (bounds);
        if (coloured)
        {
            g.setColour (colour);
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
        const auto title = isTrack ? (owner.master ? juce::String ("MASTER") : t != nullptr ? t->name : juce::String ("NO TRACK SELECTED"))
                                   : juce::String (sectionTitles[(int) section]);
        g.drawFittedText (title, getLocalBounds().withTrimmedLeft (headerHeight + (isTrack ? 6 : 2)).withTrimmedRight (4),
                          juce::Justification::centredLeft, 1, 0.8f);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && section == Section::quick)
            owner.showQuickControlsMenu();
        else if (getLocalBounds().contains (e.getPosition()) && ! e.mods.isPopupMenu())
            owner.setFolded (section, ! owner.isFolded (section));
    }

    juce::String getTooltip() override { return tooltipText; }

private:
    Inspector& owner;
    const Section section;
    juce::String tooltipText;
};

//==============================================================================
// VISIBILITY: a track, its box (ticked: shown), its colour; DISABLED if it is.
class Inspector::VisibilityRow final : public juce::Component
{
public:
    VisibilityRow (Inspector& ownerToUse, int id) : owner (ownerToUse), trackId (id)
    {
        box.setWantsKeyboardFocus (false);
        box.setTooltip ("Ticked: shown in the track list, the arrangement and the mixer. Unticked: hidden (still heard, unless it's disabled)");
        box.onClick = [this] { owner.studio.setTracksHidden ({ trackId }, ! box.getToggleState()); };
        addAndMakeVisible (box);
    }

    void refresh()
    {
        if (const auto* t = owner.studio.getProject().findTrack (trackId))
        {
            box.setButtonText (t->name);
            box.setToggleState (! t->hidden, juce::dontSendNotification);
            disabled = t->disabled;
            colour = t->colour;
        }
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (disabled ? juce::Colour (0xff4a454c) : colour);
        g.fillRect (0, 3, 4, getHeight() - 6);
        g.setColour (juce::Colour (0xff050405));
        g.fillRect (0, getHeight() - 1, getWidth(), 1);
        if (disabled)
        {
            g.setColour (ParagonColours::textDim);
            g.setFont (font (9.5f));
            g.drawText ("DISABLED", getLocalBounds().withTrimmedRight (4), juce::Justification::centredRight, false);
        }
    }

    void resized() override { box.setBounds (getLocalBounds().withTrimmedLeft (8).withTrimmedRight (disabled ? 58 : 4)); }

    const int trackId;
    juce::ToggleButton box;

private:
    Inspector& owner;
    juce::Colour colour;
    bool disabled = false;
};

//==============================================================================
Inspector::Inspector (pr7s::Studio& studioToUse) : studio (studioToUse)
{
    // The left zone's tabs: INSPECTOR and VISIBILITY (as the manual's).
    for (auto* key : { &inspectorTab, &visibilityTab })
    {
        key->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        key->setColour (juce::TextButton::textColourOffId, ParagonColours::textDim);
        key->setColour (juce::TextButton::textColourOnId, ParagonColours::accent);
        key->setWantsKeyboardFocus (false);
        addAndMakeVisible (*key);
    }
    inspectorTab.setTooltip ("The selected track's settings");
    visibilityTab.setTooltip ("Every track, to show or hide it (hidden: out of the track list, the arrangement and the mixer)");
    inspectorTab.onClick = [this] { setTab (Tab::inspector); };
    visibilityTab.onClick = [this] { setTab (Tab::visibility); };
    showAll.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
    showAll.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
    showAll.setWantsKeyboardFocus (false);
    showAll.setTooltip ("Show every hidden track again");
    showAll.onClick = [this]
    {
        std::vector<int> all;
        for (const auto& t : studio.getProject().tracks)
            all.push_back (t.id);
        studio.setTracksHidden (all, false);
    };
    addChildComponent (showAll);
    visibilityView.setViewedComponent (&visibilityList, false);
    visibilityView.setScrollBarsShown (true, false);
    visibilityView.setScrollBarThickness (8);
    addChildComponent (visibilityView);
    if (auto* settings = studio.getSettings())
        tab = settings->getValue ("leftZoneTab") == "visibility" ? Tab::visibility : Tab::inspector;

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
    for (auto* key : { &mute, &solo, &read, &write, &arm, &monitor, &lanes })
    {
        key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key->setWantsKeyboardFocus (false);
        content.addAndMakeVisible (*key);
    }
    mute.setTooltip ("Mute this track");
    solo.setTooltip ("Solo: hear only the soloed tracks (Ctrl-click: this one alone, every other solo off)");
    arm.setTooltip ("REC: record-arm: recording and live playing go to this track");
    monitor.setTooltip ("MON: hear what comes in from the interface (your guitar), through the track's effects");
    mute.onClick = [this] { if (const auto* t = track()) studio.setMute (trackId, ! t->mute); };
    solo.onClick = [this] { if (const auto* t = track()) studio.setSolo (trackId, ! t->solo || juce::ModifierKeys::currentModifiers.isCommandDown(), juce::ModifierKeys::currentModifiers.isCommandDown()); };
    arm.onClick = [this] { if (const auto* t = track()) studio.setArmed (t->armed ? 0 : trackId); };
    monitor.onClick = [this] { if (const auto* t = track()) studio.setMonitor (trackId, ! t->monitor); };
    lanes.setTooltip ("LANES (Show Lanes): the track's takes, each on a lane under it; the track's row shows what's heard");
    lanes.onClick = [this] { if (const auto* t = track()) studio.setShowLanes (trackId, ! t->showLanes); };

    // RENDER (0.17.1, the owner asked: Render in Place as last time, in FREEZE's place; Freeze stays in
    // the track's menu) and PRESET (Load/Save Track Preset).
    for (auto* key : { &render, &preset })
    {
        key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key->setWantsKeyboardFocus (false);
        content.addAndMakeVisible (*key);
    }
    render.setTooltip ("RENDER: Render in Place of the whole track, with the settings you used last time "
                       "(a new audio track under it). For the settings page: the track's menu, Render in Place...");
    preset.setTooltip ("Save this track's sound (instrument, effects and their settings, fader, pan, colour, Quick Controls) "
                       "as a track preset, or load one onto it");
    render.onClick = [this] { if (onRender) onRender (trackId); };
    preset.onClick = [this]
    {
        juce::PopupMenu menu;
        TrackActions::addPresetItems (menu, studio, trackId);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&preset), [safe = juce::Component::SafePointer<Inspector> (this), id = trackId] (int chosen)
        {
            if (safe != nullptr)
                TrackActions::handle (chosen, safe->studio, id, safe->chooser);
        });
    };

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
    volume.textFromValueFunction = [] (double db) { return dbText (db); };
    volume.parse = [] (const juce::String& text) { return ValueEntry::parseDb (text); };
    volume.setTooltip ("The track's volume (double-click it or its number to type a value; Ctrl-click for 0 dB; Shift-drag for fine moves)");
    volume.onValueChange = [this]
    {
        volumeReadout.setText (dbText (volume.getValue()), juce::dontSendNotification);
        if (master)   // (0.9.12: the master's, in its Inspector)
            studio.setMasterVolume ((float) volume.getValue(), ! volume.isMouseButtonDown());
        else
            studio.setVolume (trackId, (float) volume.getValue(), ! volume.isMouseButtonDown());
    };
    volume.onDragStart = [this] { studio.grabControl (trackId, pr7s::Studio::control (pr7s::AutomationLane::Target::volume)); };
    volume.onDragEnd = [this]
    {
        if (master)
            studio.setMasterVolume ((float) volume.getValue(), true);
        else
            studio.setVolume (trackId, (float) volume.getValue(), true);
        studio.releaseControl (trackId, pr7s::Studio::control (pr7s::AutomationLane::Target::volume));
    };
    pan.setRange (-1.0, 1.0, 0.01);
    pan.getProperties().set (ParagonLookAndFeel::barFromCentreProperty, true);
    pan.setDoubleClickReturnValue (true, 0.0, juce::ModifierKeys::commandModifier);
    pan.textFromValueFunction = [] (double value) { return panText (value); };
    pan.parse = [] (const juce::String& text) { return ValueEntry::parsePan (text); };
    pan.setTooltip ("Pan: left and right (double-click it or its number to type a value: L30, R15, C; Ctrl-click for the centre; Shift-drag for fine moves)");
    // The numbers beside them: double-click to type (0.9.9).
    for (auto [readout, slider] : { std::pair<juce::Label*, ValueSlider*> { &volumeReadout, &volume }, std::pair<juce::Label*, ValueSlider*> { &panReadout, &pan } })
    {
        readout->setEditable (false, true, false);
        readout->setTooltip ("Double-click to type a value");
        readout->onTextChange = [readout, slider]
        {
            slider->enter (readout->getText());
            readout->setText (slider->getTextFromValue (slider->getValue()), juce::dontSendNotification);   // (as it is, if refused)
        };
    }
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

    for (auto* label : { &volumeLabel, &panLabel, &midiInputLabel, &channelLabel, &audioInputLabel, &outputLabel, &drumMapLabel,
                         &followLabel, &liveLabel, &shiftLabel })
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
    drumMapLabel.setText ("DRUMS", juce::dontSendNotification);
    followLabel.setText ("FOLLOW", juce::dontSendNotification);
    liveLabel.setText ("LIVE IN", juce::dontSendNotification);
    shiftLabel.setText ("SHIFT", juce::dontSendNotification);
    for (auto* readout : { &volumeReadout, &panReadout })
    {
        readout->setFont (font (11.0f));
        readout->setColour (juce::Label::textColourId, ParagonColours::text);
        readout->setJustificationType (juce::Justification::centredRight);
        readout->setInterceptsMouseClicks (false, false);
        content.addAndMakeVisible (*readout);
    }

    for (auto* button : { &instrument, &edit, &output, &drumMapKey, &followKey, &liveKey, &shiftKey })
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
    drumMapKey.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    drumMapKey.setTooltip ("The track's drum map: names its notes after the drums they play (the MIDI editor's drum view) and plays them through it");
    drumMapKey.onClick = [this] { showDrumMapMenu(); };
    // CHORDS (0.36.0)
    for (auto* key : { &followKey, &liveKey, &shiftKey })
        key->getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    followKey.setTooltip ("Follow Chord Track: the track's notes heard moved onto the chord track's chords (change the chords, the notes follow). "
                          "Its notes stay as played: the MIDI editor shows where they sound");
    followKey.onClick = [this] { showFollowMenu(); };
    liveKey.setTooltip ("Live Input: the keys you play into this track go out on the chord track's chord (or scale) at the playhead, and are recorded so");
    liveKey.onClick = [this] { showLiveMenu(); };
    shiftKey.setTooltip ("Shift Chords (the chord track's): notes played a little before a chord (negative) or after it (positive) still take that chord");
    shiftKey.onClick = [this] { showShiftMenu(); };

    // ROUTING
    for (auto* box : std::initializer_list<juce::ComboBox*> { &midiInput, &channel, &audioInput })
    {
        box->setWantsKeyboardFocus (false);
        content.addAndMakeVisible (*box);
    }
    midiDevices = juce::MidiInput::getAvailableDevices();
    midiInput.beforePopup = [this]
    {
        midiDevices = juce::MidiInput::getAvailableDevices();
        fillMidiInputs();
    };
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
        insertsHolder.addAndMakeVisible (slot);
    }
    insertsView.setViewedComponent (&insertsHolder, false);
    insertsView.setScrollBarsShown (true, false);
    insertsView.setScrollBarThickness (7);
    insertsView.setSingleStepSizes (slotHeight + slotGap, slotHeight + slotGap);
    content.addAndMakeVisible (insertsView);
    noSends.setText ("NO SENDS (AN FX CHANNEL)", juce::dontSendNotification);
    noSends.setFont (font (10.0f));
    noSends.setColour (juce::Label::textColourId, ParagonColours::textDim);
    noSends.setJustificationType (juce::Justification::centred);
    content.addChildComponent (noSends);

    // QUICK CONTROLS: each slot's name key and its bar.
    for (int i = 0; i < pr7s::Track::numQuickControls; ++i)
    {
        auto& key = qcKeys[(size_t) i];
        key.setColour (juce::TextButton::buttonColourId, slotColour);
        key.setWantsKeyboardFocus (false);
        key.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
        key.onClick = [this, i] { showQuickControlMenu (i); };
        content.addAndMakeVisible (key);
        auto& bar = qcBars[(size_t) i];
        bar.setSliderStyle (juce::Slider::LinearBar);
        bar.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        bar.setRange (0.0, 1.0, 0.0);
        bar.setColour (juce::Slider::trackColourId, ParagonColours::accent.withAlpha (0.75f));
        bar.setColour (juce::Slider::backgroundColourId, slotColour);
        bar.setVelocityModeParameters (0.2, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        bar.setWantsKeyboardFocus (false);
        bar.setPopupDisplayEnabled (true, true, nullptr);
        bar.textFromValueFunction = [this, i] (double) { return studio.getQuickControlName (trackId, i); };
        bar.onDragStart = [this, i] { if (const auto qc = studio.getQuickControl (trackId, i)) studio.grabControl (trackId, *qc); };
        bar.onValueChange = [this, i] { studio.setQuickControlValue (trackId, i, (float) qcBars[(size_t) i].getValue(), ! qcBars[(size_t) i].isMouseButtonDown()); };
        bar.onDragEnd = [this, i]
        {
            studio.setQuickControlValue (trackId, i, (float) qcBars[(size_t) i].getValue(), true);
            if (const auto qc = studio.getQuickControl (trackId, i))
                studio.releaseControl (trackId, *qc);
        };
        content.addAndMakeVisible (bar);
    }

    enable.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
    enable.setWantsKeyboardFocus (false);
    enable.onClick = [this] { if (const auto* t = track()) studio.setTracksDisabled ({ trackId }, ! t->disabled); };
    content.addChildComponent (enable);
    colourKey.setWantsKeyboardFocus (false);
    colourKey.onClick = [this]
    {
        TrackColourMenu::show (colourKey, studio, studio.isTrackSelected (trackId) ? studio.getSelectedTracks() : std::vector<int> { trackId });
    };
    content.addChildComponent (colourKey);

    // FADER
    fader.onOpenEffect = [this] (int bus, int slot) { if (onOpenEffect) onOpenEffect (bus, slot); };
    fader.onFindPlugins = [this] { if (onFindPlugins) onFindPlugins(); };
    content.addAndMakeVisible (fader);

    insertsHolder.over = [this] (juce::Graphics& g)
    {
        if (insertLineY < 0)
            return;
        g.setColour (ParagonColours::accent);
        g.fillRect (0.0f, (float) insertLineY, (float) insertsHolder.getWidth(), 1.5f);
    };
    studio.addChangeListener (this);
    showTrack (studio.getSelectedTrack());
    setTab (tab);
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
    for (auto* key : { &mute, &solo, &read, &write, &arm, &monitor, &render, &preset, &lanes })
        if (key->getButtonText() == name)
            return *key;
    return mute;
}

void Inspector::setTab (Tab newTab)
{
    tab = newTab;
    inspectorTab.setToggleState (tab == Tab::inspector, juce::dontSendNotification);
    visibilityTab.setToggleState (tab == Tab::visibility, juce::dontSendNotification);
    viewport.setVisible (tab == Tab::inspector);
    visibilityView.setVisible (tab == Tab::visibility);
    showAll.setVisible (tab == Tab::visibility);
    if (auto* settings = studio.getSettings())
        settings->setValue ("leftZoneTab", tab == Tab::visibility ? "visibility" : "inspector");
    refreshVisibility();
    resized();
    repaint();
}

juce::ToggleButton* Inspector::getVisibilityBox (int id)
{
    for (auto& row : visibilityRows)
        if (row->trackId == id)
            return &row->box;
    return nullptr;
}

void Inspector::refreshVisibility()
{
    // A row for every track, in order (made again when the tracks change).
    const auto& tracks = studio.getProject().tracks;
    auto same = visibilityRows.size() == tracks.size();
    for (size_t i = 0; same && i < tracks.size(); ++i)
        same = visibilityRows[i]->trackId == tracks[i].id;
    if (! same)
    {
        visibilityRows.clear();
        for (const auto& t : tracks)
        {
            visibilityRows.push_back (std::make_unique<VisibilityRow> (*this, t.id));
            visibilityList.addAndMakeVisible (*visibilityRows.back());
        }
    }
    auto anyHidden = false;
    for (const auto& t : tracks)
        anyHidden = anyHidden || t.hidden;
    showAll.setEnabled (anyHidden);
    const auto w = juce::jmax (100, visibilityView.getWidth() - visibilityView.getScrollBarThickness());
    auto y = 0;
    for (auto& row : visibilityRows)
    {
        row->refresh();
        row->setBounds (0, y, w, 26);
        y += 26;
    }
    visibilityList.setSize (w, y + margin);
}

void Inspector::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshVisibility();
    if (studio.isMasterSelected())   // the master's row (0.9.12)
    {
        if (! master)
            showMaster();
        else
            refresh();
        return;
    }
    if (master || studio.getSelectedTrack() != trackId || track() == nullptr)
        showTrack (studio.getSelectedTrack());
    else
        refresh();
}

void Inspector::showMaster()
{
    // The master (0.9.12, the owner asked): its R and W, volume, inserts and fader.
    master = true;
    trackId = pr7s::Engine::masterBus;
    AutomationKeys::setUp (read, write, studio, trackId);
    fader.setSingleTrack (trackId, true);
    refresh();
}

void Inspector::showTrack (int id)
{
    master = false;
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
    for (const auto& device : midiDevices)
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
    if (master)
    {
        AutomationKeys::refresh (read, write, studio, trackId);
        refreshInserts();
        followMix();
        layout();
        return;
    }
    if (t == nullptr)
    {
        layout();
        return;
    }
    mute.setToggleState (t->mute, juce::dontSendNotification);
    solo.setToggleState (t->solo, juce::dontSendNotification);
    arm.setToggleState (t->armed, juce::dontSendNotification);
    monitor.setToggleState (t->monitor, juce::dontSendNotification);
    lanes.setToggleState (t->showLanes, juce::dontSendNotification);
    AutomationKeys::refresh (read, write, studio, trackId);
    // RENDER: a track with parts, live (a frozen one plays its file: unfreeze it first).
    render.setEnabled (! t->disabled && ! t->frozen && ! t->parts.empty());
    // DISABLE / ENABLE TRACK, always there (0.17.1, the owner asked).
    enable.setButtonText (t->disabled ? "ENABLE TRACK" : "DISABLE TRACK");
    enable.setColour (juce::TextButton::buttonColourId, t->disabled ? juce::Colour (0xff3a0f18) : juce::Colour (0xff151217));
    enable.setEnabled (! t->frozen);
    enable.setTooltip (t->disabled ? "This track is disabled: silent, its instrument and effects unloaded (their settings kept). Click to load them and hear it again"
                     : t->frozen ? "A frozen track's plugins are already unloaded: unfreeze it first (the track's menu)"
                                 : "Disable the track: silent, its instrument and effects unloaded (their settings kept), so it costs no power");
    arm.setEnabled (! t->frozen);   // (a frozen track can't record)
    preset.setEnabled (! t->disabled);

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
    edit.setEnabled (studio.hasEditor (instance));

    fillMidiInputs();
    channel.setSelectedId (t->midiChannel, juce::dontSendNotification);
    drumMapKey.setButtonText (t->drumMap.isNotEmpty() ? t->drumMap.toUpperCase() : juce::String ("NO DRUM MAP"));
    followKey.setButtonText (pr7s::followModeName (t->follow));
    followKey.setColour (juce::TextButton::buttonColourId, t->follow != pr7s::FollowMode::off ? juce::Colour (0xff3a0f18) : juce::Colour (0xff151217));
    liveKey.setButtonText (pr7s::liveFollowName (t->liveFollow));
    liveKey.setColour (juce::TextButton::buttonColourId, t->liveFollow != pr7s::LiveFollow::off ? juce::Colour (0xff3a0f18) : juce::Colour (0xff151217));
    {
        const auto ticks = studio.getChordTrack().shiftTicks;
        shiftKey.setButtonText (ticks == 0 ? juce::String ("0 TICKS") : (ticks > 0 ? "+" : "") + juce::String (ticks) + " TICKS");
    }
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

    refreshInserts();
    for (auto& send : sendSlots)
        send->refresh();
    refreshQuickControls();
    followMix();
    layout();
}

void Inspector::refreshInserts()
{
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
}

void Inspector::refreshQuickControls()
{
    for (int i = 0; i < pr7s::Track::numQuickControls; ++i)
    {
        const auto name = studio.getQuickControlName (trackId, i);
        auto& key = qcKeys[(size_t) i];
        key.setButtonText (name.isNotEmpty() ? name : "QC " + juce::String (i + 1));
        key.setColour (juce::TextButton::textColourOffId, name.isNotEmpty() ? ParagonColours::text : ParagonColours::textDim.withAlpha (0.7f));
        key.setTooltip (name.isNotEmpty() ? "Quick Control " + juce::String (i + 1) + ": " + name + ". Click to choose another control, or none"
                                          : "Quick Control " + juce::String (i + 1) + ": click to choose one of the track's controls");
        const auto qc = studio.getQuickControl (trackId, i);
        qcBars[(size_t) i].getProperties().set (ParagonLookAndFeel::barFromCentreProperty, qc && qc->target == pr7s::AutomationLane::Target::pan);
        qcBars[(size_t) i].setValue (studio.getQuickControlValue (trackId, i), juce::dontSendNotification);
        qcBars[(size_t) i].repaint();
    }
}

void Inspector::showQuickControlMenu (int index)
{
    // Every control of the track, grouped as the automation lanes' menus have them.
    const auto controls = studio.getAutomationControls (trackId);
    const auto current = studio.getQuickControl (trackId, index);
    juce::PopupMenu menu, sub;
    juce::String group;
    const auto flush = [&]
    {
        if (group.isNotEmpty() && sub.getNumItems() > 0)
            menu.addSubMenu (group, sub);
        sub = {};
    };
    menu.addSectionHeader ("QUICK CONTROL " + juce::String (index + 1));
    for (int i = 0; i < (int) controls.size(); ++i)
    {
        const auto& c = controls[(size_t) i];
        if (c.group != group)
        {
            flush();
            group = c.group;
        }
        (group.isEmpty() ? menu : sub).addItem (100 + i, c.name, true, current && current->sameControl (c.asLane()));
    }
    flush();
    menu.addSeparator();
    menu.addItem (1, "None", current.has_value());
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&qcKeys[(size_t) index]),
                        [safe = juce::Component::SafePointer<Inspector> (this), index, controls, id = trackId] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        if (chosen == 1)
            safe->studio.setQuickControl (id, index, std::nullopt);
        else if (chosen >= 100 && chosen - 100 < (int) controls.size())
            safe->studio.setQuickControl (id, index, controls[(size_t) (chosen - 100)]);
    });
}

void Inspector::showQuickControlsMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Show Automated QC Assignments");
    menu.addItem (2, "The instrument's first 8 knobs", studio.getEngine().getInstrument (trackId) != nullptr);
    menu.addItem (3, "Clear all");
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<Inspector> (this), id = trackId] (int chosen)
    {
        if (safe == nullptr)
            return;
        if (chosen == 1)
            safe->studio.showAutomatedQuickControls (id);
        else if (chosen == 2)
            safe->studio.assignInstrumentQuickControls (id);
        else if (chosen == 3)
            for (int i = 0; i < pr7s::Track::numQuickControls; ++i)
                safe->studio.setQuickControl (id, i, std::nullopt);
    });
}

void Inspector::followMix()
{
    if (master)
    {
        const auto db = studio.getAutomationRead (trackId) ? studio.getEngine().getMixNow (trackId).volumeDb : studio.getProject().masterVolumeDb;
        if (! volume.isMouseButtonDown() && std::abs (volume.getValue() - db) > 0.01)
            volume.setValue (db, juce::dontSendNotification);
        volumeReadout.setText (dbText (volume.getValue()), juce::dontSendNotification);
        return;
    }
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
    // The Quick Controls' bars as their controls are now (automation playing, the plugin's own window).
    for (int i = 0; i < pr7s::Track::numQuickControls; ++i)
        if (auto& bar = qcBars[(size_t) i]; bar.isVisible() && ! bar.isMouseButtonDown())
            bar.setValue (studio.getQuickControlValue (trackId, i), juce::dontSendNotification);
    AutomationKeys::refresh (read, write, studio, trackId);
    for (auto& send : sendSlots)
        send->repaintIfChanged();   // (0.34.3: only when what it shows has changed)
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
        // The master (0.9.12): its basics (R W, volume), inserts and fader.
        const auto applies = master ? section == Section::basics || section == Section::inserts || section == Section::fader
                                    : t != nullptr && (section != Section::sends || ! t->isFx())
                                          && (section != Section::chords || t->kind == pr7s::Track::Kind::instrument);   // (0.36.0: MIDI only)
        if (! applies && section != Section::basics)
            continue;
        header.setBounds (0, y, w, headerHeight);
        shown.push_back (&header);
        y += headerHeight + gap;
        if ((t == nullptr && ! master) || folded[(size_t) i])
            continue;
        if (master && section == Section::basics)
        {
            auto r = row (rowHeight);
            const auto keyWidth = (r.getWidth() - 3) / 2;
            place (read, r.removeFromLeft (keyWidth));
            r.removeFromLeft (3);
            place (write, r);
            auto line = row (rowHeight - 4);
            place (volumeLabel, line.removeFromLeft (34));
            place (volumeReadout, line.removeFromRight (56));
            line.removeFromRight (4);
            place (volume, line);
            continue;
        }
        switch (section)
        {
            case Section::basics:
            {
                {
                    // DISABLE TRACK (0.17.1: always; FX and group channels can't be disabled) and the colour key.
                    auto line = row (rowHeight + 2);
                    place (colourKey, line.removeFromRight (36));
                    if (! t->isChannel())
                    {
                        line.removeFromRight (4);
                        place (enable, line);
                    }
                }
                // M S R W (and REC).
                std::vector<juce::TextButton*> keys { &mute, &solo, &read, &write };
                if (! t->isChannel())
                    keys.push_back (&arm);
                auto r = row (rowHeight);
                const auto keyWidth = (r.getWidth() - 3 * ((int) keys.size() - 1)) / (int) keys.size();
                for (auto* key : keys)
                {
                    place (*key, r.removeFromLeft (keyWidth));
                    r.removeFromLeft (3);
                }
                if (! t->isGroup())
                {
                    auto line = row (rowHeight);
                    if (! t->isChannel())
                    {
                        place (render, line.removeFromLeft ((line.getWidth() - 3) / 2));
                        line.removeFromLeft (3);
                    }
                    place (preset, line);
                }
                // (0.35.0, the owner asked) LANES and MON (an audio track's), under RENDER and PRESET.
                if (! t->isChannel())
                {
                    auto line = row (rowHeight);
                    if (t->isAudio())
                    {
                        place (lanes, line.removeFromLeft ((line.getWidth() - 3) / 2));
                        line.removeFromLeft (3);
                        place (monitor, line);
                    }
                    else
                        place (lanes, line);
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
                    labelled (drumMapLabel, drumMapKey);
                }
                if (t->isAudio())
                    labelled (audioInputLabel, audioInput);
                labelled (outputLabel, output);
                break;
            case Section::chords:
                labelled (followLabel, followKey);
                labelled (liveLabel, liveKey);
                labelled (shiftLabel, shiftKey);
                break;
            case Section::inserts:
            {
                // All 16 in a holder; five in view, the rest scrolled to.
                const auto step = slotHeight + slotGap;
                const auto viewHeight = insertsInView * step;
                place (insertsView, juce::Rectangle<int> (margin, y, w - 2 * margin, viewHeight));
                const auto holderWidth = insertsView.getWidth() - insertsView.getScrollBarThickness() - 2;
                auto top = 0;
                for (int s = 0; s < (int) slots.size(); ++s)
                {
                    if (s == preFader)
                        insertLineY = s == 0 ? 0 : top - slotGap / 2 - 1;
                    slots[(size_t) s].setBounds (0, top + (s == 0 && preFader == 0 ? 2 : 0), holderWidth, slotHeight);
                    top += step;
                }
                insertsHolder.setSize (holderWidth, top);
                insertsHolder.repaint();
                y += viewHeight + gap;
                break;
            }
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
            case Section::quick:
                for (int q = 0; q < pr7s::Track::numQuickControls; ++q)
                {
                    auto line = juce::Rectangle<int> (margin, y, w - 2 * margin, slotHeight + 1);
                    const auto assigned = studio.getQuickControl (trackId, q).has_value();
                    place (qcKeys[(size_t) q], assigned ? line.removeFromLeft (line.getWidth() * 11 / 20) : line);
                    if (assigned)
                    {
                        line.removeFromLeft (4);
                        place (qcBars[(size_t) q], line);
                    }
                    y += slotHeight + 1 + slotGap;
                }
                y += gap;
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
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (0, tabHeight - 1, getWidth(), 1);
    if (tab == Tab::visibility)
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (font (10.5f, juce::Font::plain));
        g.drawFittedText ("Ticked: shown. Unticked: hidden (still heard unless disabled)", getLocalBounds().withTop (tabHeight + 4).withHeight (30).reduced (8, 0),
                          juce::Justification::centredLeft, 2);
        return;
    }
    if (track() == nullptr && ! master)
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (font (12.0f, juce::Font::plain));
        g.drawFittedText ("Select a track to see its settings here", getLocalBounds().reduced (16).withTrimmedTop (tabHeight + headerHeight + 8),
                          juce::Justification::centredTop, 3);
    }
}

void Inspector::resized()
{
    auto area = getLocalBounds().withTrimmedRight (1);
    auto tabs = area.removeFromTop (tabHeight).reduced (4, 3);
    inspectorTab.setBounds (tabs.removeFromLeft (tabs.getWidth() / 2 - 2));
    tabs.removeFromLeft (4);
    visibilityTab.setBounds (tabs);
    viewport.setBounds (area);
    auto visibility = area;
    visibility.removeFromTop (34);   // (the note)
    showAll.setBounds (visibility.removeFromBottom (30).reduced (6, 3));
    visibilityView.setBounds (visibility);
    layout();
    refreshVisibility();
}

void Inspector::showDrumMapMenu()
{
    // (0.26.0) As the MIDI editor's MAP menu: none, the GM Map, the song's, the drum map folder's, the setup.
    const auto* t = studio.getProject().findTrack (trackId);
    if (t == nullptr)
        return;
    const auto id = trackId;
    const auto current = t->drumMap;
    auto& s = studio;
    juce::PopupMenu menu;
    menu.addItem ("No Drum Map", true, current.isEmpty(), [&s, id] { s.setTrackDrumMap (id, {}); });
    const auto gm = pr7s::DrumMap::gm().name;
    menu.addItem (gm + " (built in)", true, current == gm, [&s, id, gm] { s.setTrackDrumMap (id, gm); });
    for (const auto& map : studio.getProject().drumMaps)
        if (map.name != gm)
            menu.addItem (map.name, true, current == map.name, [&s, id, name = map.name] { s.setTrackDrumMap (id, name); });
    juce::PopupMenu fromFolder;
    for (const auto& file : studio.getLibraryDrumMaps())
        if (const auto name = file.getFileNameWithoutExtension(); studio.getProject().findDrumMap (name) == nullptr)
            fromFolder.addItem (name, true, false, [&s, id, name] { s.setTrackDrumMap (id, name); });
    if (fromFolder.getNumItems() > 0)
        menu.addSubMenu ("From the Drum Map Folder", fromFolder);
    menu.addSeparator();
    menu.addItem ("Drum Map Setup...", true, false, [safe = juce::Component::SafePointer<Inspector> (this)]
    {
        if (safe != nullptr && safe->onDrumMapSetup)
            safe->onDrumMapSetup();
    });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (drumMapKey));
}

//==============================================================================
// CHORDS (0.36.0)
void Inspector::showFollowMenu()
{
    const auto* t = studio.getProject().findTrack (trackId);
    if (t == nullptr)
        return;
    juce::Component::SafePointer<Inspector> safe (this);
    juce::PopupMenu menu;
    menu.addSectionHeader ("FOLLOW CHORD TRACK");
    for (const auto mode : pr7s::followModes())
        menu.addItem (pr7s::followModeName (mode), ! t->frozen, t->follow == mode, [safe, mode]
        {
            if (safe != nullptr)
                safe->chooseFollowMode (mode);
        });
    menu.addSeparator();
    menu.addItem ("Set Up Again...", ! t->followReference.isEmpty() && ! t->frozen, false, [safe]
    {
        if (safe == nullptr)
            return;
        const auto* now = safe->studio.getProject().findTrack (safe->trackId);
        const auto mode = now != nullptr && now->follow != pr7s::FollowMode::off ? now->follow : pr7s::FollowMode::chordsAndScales;
        FollowSetupPage::show (safe->studio, safe->trackId, mode, safe->followKey);
    });
    if (studio.getChordTrack().events.empty())
        menu.addItem ("(The chord track has no chords yet: nothing to follow)", false, false, [] {});
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (followKey));
}

void Inspector::chooseFollowMode (pr7s::FollowMode mode)
{
    // The first time (the manual): the dialog asks how the notes relate to the chord track.
    if (mode != pr7s::FollowMode::off && studio.needsFollowSetup (trackId))
        FollowSetupPage::show (studio, trackId, mode, followKey);
    else
        studio.setFollowMode (trackId, mode);
}

void Inspector::showLiveMenu()
{
    const auto* t = studio.getProject().findTrack (trackId);
    if (t == nullptr)
        return;
    auto& s = studio;
    const auto id = trackId;
    juce::PopupMenu menu;
    menu.addSectionHeader ("LIVE INPUT");
    for (const auto live : { pr7s::LiveFollow::off, pr7s::LiveFollow::chords, pr7s::LiveFollow::scales })
        menu.addItem (pr7s::liveFollowName (live), true, t->liveFollow == live, [&s, id, live] { s.setLiveFollow (id, live); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (liveKey));
}

void Inspector::showShiftMenu()
{
    auto& s = studio;
    const auto now = studio.getChordTrack().shiftTicks;
    juce::PopupMenu menu;
    menu.addSectionHeader ("SHIFT CHORDS (THE CHORD TRACK'S; 480 TICKS A BEAT)");
    for (const auto ticks : { -120, -60, -30, -15, 0, 15, 30, 60, 120 })
        menu.addItem ((ticks > 0 ? "+" : "") + juce::String (ticks) + " ticks" + (ticks < 0 ? " (notes played early)" : ticks > 0 ? " (notes played late)" : ""),
                      true, now == ticks, [&s, ticks] { s.setChordShift (ticks); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (shiftKey));
}
