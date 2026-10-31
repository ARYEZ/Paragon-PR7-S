#include "MixerView.h"

#include "ui/AutomationKeys.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    juce::FontOptions font (float height, int style = juce::Font::bold) { return ParagonLookAndFeel::textFont (height, style); }

    juce::String dbText (double db) { return db <= -59.9 ? juce::String ("-inf") : (db > 0.04 ? "+" : "") + juce::String (db, 1); }

    juce::String panText (double pan)
    {
        const auto percent = juce::roundToInt (std::abs (pan) * 100.0);
        return percent == 0 ? juce::String ("C") : (pan < 0.0 ? "L " : "R ") + juce::String (percent);
    }

    constexpr int slotHeight = 16, slotGap = 2;
}

//==============================================================================
MixerView::Strip::Strip (MixerView& ownerView, int id) : owner (ownerView), busId (id)
{
    for (int i = 0; i < (int) slots.size(); ++i)
    {
        auto& slot = slots[(size_t) i];
        slot.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0b090c));
        slot.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        slot.setWantsKeyboardFocus (false);
        slot.onClick = [this, i] { owner.showInsertMenu (busId, i); };
        addAndMakeVisible (slot);
    }

    for (int i = 0; i < pr7s::Project::maxSends; ++i)
    {
        sendSlots[(size_t) i] = std::make_unique<SendSlot> (owner, busId, i);
        addChildComponent (*sendSlots[(size_t) i]);
    }
    output.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0b090c));
    output.setColour (juce::TextButton::textColourOffId, ParagonColours::textDim);
    output.setTooltip ("Where its sound goes: the master, or a group channel (FX and group channels go to the master)");
    output.onClick = [this] { owner.showOutputMenu (busId); };
    if (! isMaster())
        addAndMakeVisible (output);

    volume.setSliderStyle (juce::Slider::LinearVertical);
    volume.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    volume.getProperties().set (ParagonLookAndFeel::eqFaderProperty, true);
    volume.getProperties().set (ParagonLookAndFeel::faderFromBottomProperty, true);
    volume.setColour (juce::Slider::trackColourId, ParagonColours::accent);
    volume.setRange (-60.0, 6.0, 0.1);
    volume.setSkewFactorFromMidPoint (-12.0);
    // Ctrl-click (or double-click) resets it; Shift-drag moves it finely (as the manual has it).
    volume.setDoubleClickReturnValue (true, 0.0, juce::ModifierKeys::commandModifier);
    volume.setVelocityModeParameters (0.2, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    volume.setPopupDisplayEnabled (true, true, nullptr);
    volume.textFromValueFunction = [] (double db) { return dbText (db) + " dB"; };
    volume.setTooltip ((isMaster() ? juce::String ("The master's volume: the whole mix") : juce::String ("The track's volume"))
                       + " (Ctrl-click or double-click for 0 dB; Shift-drag for fine moves)");
    // While dragging, it's heard as it moves; the drag is one undo step when let go.
    const auto setVolume = [this] (bool undoable)
    {
        if (isMaster())
            owner.studio.setMasterVolume ((float) volume.getValue(), undoable);
        else
            owner.studio.setVolume (busId, (float) volume.getValue(), undoable);
    };
    volume.onValueChange = [this, setVolume] { setVolume (! volume.isMouseButtonDown()); };
    // Grabbed and let go: writing automation (W on, while playing) follows the hand.
    const auto volumeControl = pr7s::Studio::control (pr7s::AutomationLane::Target::volume);
    volume.onDragStart = [this, volumeControl] { owner.studio.grabControl (busId, volumeControl); };
    volume.onDragEnd = [this, setVolume, volumeControl]
    {
        setVolume (true);
        owner.studio.releaseControl (busId, volumeControl);
    };
    addAndMakeVisible (volume);
    AutomationKeys::setUp (read, write, owner.studio, busId);
    addAndMakeVisible (read);
    addAndMakeVisible (write);

    if (! isMaster())
    {
        pan.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        pan.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        pan.setRange (-1.0, 1.0, 0.01);
        pan.setDoubleClickReturnValue (true, 0.0, juce::ModifierKeys::commandModifier);
        pan.setVelocityModeParameters (0.2, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        pan.setPopupDisplayEnabled (true, true, nullptr);
        pan.textFromValueFunction = [] (double value) { return panText (value); };
        pan.setTooltip ("Pan: left and right (Ctrl-click or double-click for the centre; Shift-drag for fine moves)");
        pan.onValueChange = [this] { owner.studio.setPan (busId, (float) pan.getValue(), ! pan.isMouseButtonDown()); };
        pan.onDragStart = [this] { owner.studio.grabControl (busId, pr7s::Studio::control (pr7s::AutomationLane::Target::pan)); };
        pan.onDragEnd = [this]
        {
            owner.studio.setPan (busId, (float) pan.getValue(), true);
            owner.studio.releaseControl (busId, pr7s::Studio::control (pr7s::AutomationLane::Target::pan));
        };
        addAndMakeVisible (pan);

        for (auto* key : { &mute, &solo })
        {
            key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
            key->setWantsKeyboardFocus (false);
            addAndMakeVisible (*key);
        }
        mute.setTooltip ("Mute this track");
        solo.setTooltip ("Solo: hear only the soloed tracks");
        mute.onClick = [this] { if (const auto* t = owner.studio.getProject().findTrack (busId)) owner.studio.setMute (busId, ! t->mute); };
        solo.onClick = [this] { if (const auto* t = owner.studio.getProject().findTrack (busId)) owner.studio.setSolo (busId, ! t->solo); };
    }
    refresh();
}

void MixerView::Strip::refresh()
{
    const auto& project = owner.studio.getProject();
    const auto* track = project.findTrack (busId);
    if (! isMaster() && track == nullptr)
        return;
    const auto* inserts = owner.studio.getInserts (busId);
    const auto used = inserts != nullptr ? (int) inserts->size() : 0;   // up to the last effect
    const auto shown = juce::jlimit (8, pr7s::Project::maxInserts, used + 1);
    if (shown != shownSlots)
    {
        shownSlots = shown;
        resized();
        if (owner.onMinimumHeightChange)   // the rack's room changed
            owner.onMinimumHeightChange();
    }
    const auto preFader = owner.studio.getPreFaderSlots (busId);
    const auto sendsShown = owner.isShowingSends();
    for (int i = 0; i < (int) sendSlots.size(); ++i)
    {
        sendSlots[(size_t) i]->setVisible (owner.isRackShown() && sendsShown && track != nullptr && ! track->isFx());
        sendSlots[(size_t) i]->refresh();
    }
    if (track != nullptr)
    {
        const auto* to = project.findTrack (track->output);
        output.setButtonText ("OUT: " + (to != nullptr ? to->name : juce::String ("MASTER")));
        output.setEnabled (! track->isChannel());
    }
    for (int i = 0; i < (int) slots.size(); ++i)
    {
        auto& slot = slots[(size_t) i];
        slot.setVisible (owner.isRackShown() && ! sendsShown && i < shownSlots);
        slot.setColour (juce::TextButton::buttonColourId, i < preFader ? juce::Colour (0xff0b090c) : juce::Colour (0xff1c1108));   // after the fader: tinted
        if (inserts != nullptr && i < (int) inserts->size() && ! (*inserts)[(size_t) i].isEmpty())
        {
            const auto& insert = (*inserts)[(size_t) i];
            const auto loaded = owner.studio.getEngine().getEffect (busId, i) != nullptr;
            slot.setButtonText (insert.plugin.name);
            slot.setColour (juce::TextButton::textColourOffId, insert.bypass || ! loaded ? ParagonColours::textDim : ParagonColours::text);
            slot.setTooltip (insert.plugin.name + (! loaded ? ": not on this computer (the sound goes past it)"
                                                            : insert.bypass ? ": bypassed (the sound goes past it). Click for its menu"
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
    if (track != nullptr)
    {
        mute.setToggleState (track->mute, juce::dontSendNotification);
        solo.setToggleState (track->solo, juce::dontSendNotification);
    }
    AutomationKeys::refresh (read, write, owner.studio, busId);
    followMix();
    repaint();
}

void MixerView::Strip::followMix()
{
    const auto& project = owner.studio.getProject();
    const auto* track = project.findTrack (busId);
    if (! isMaster() && track == nullptr)
        return;
    // As set, or, while R is on, as the automation has it now (the engine's).
    auto db = isMaster() ? project.masterVolumeDb : track->volumeDb;
    auto panNow = track != nullptr ? track->pan : 0.0f;
    if (owner.studio.getAutomationRead (busId))
    {
        const auto now = owner.studio.getEngine().getMixNow (busId);
        db = now.volumeDb;
        panNow = now.pan;
        if (track != nullptr)
            mute.setToggleState (now.mute, juce::dontSendNotification);
    }
    if (! volume.isMouseButtonDown() && std::abs (volume.getValue() - db) > 0.01)
    {
        volume.setValue (db, juce::dontSendNotification);
        repaint (readoutArea.toNearestInt());
    }
    if (track != nullptr && ! pan.isMouseButtonDown() && std::abs (pan.getValue() - panNow) > 0.001)
    {
        pan.setValue (panNow, juce::dontSendNotification);
        repaint (panReadoutArea.toNearestInt());
    }
}

void MixerView::Strip::setLevels (float leftPeak, float rightPeak)
{
    const auto changedLeft = left.push (leftPeak);
    const auto changedRight = right.push (rightPeak);
    if (changedLeft || changedRight)
        repaint (meterArea.toNearestInt().expanded (2));
}

void MixerView::Strip::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto* track = owner.studio.getProject().findTrack (busId);
    const auto selected = ! isMaster() && owner.studio.isTrackSelected (busId);
    g.setColour (isMaster() ? juce::Colour (0xff140f14) : selected ? juce::Colour (0xff1f1720) : juce::Colour (0xff100c11));
    g.fillRect (bounds.reduced (1.0f, 0.0f));
    g.setColour (juce::Colour (0xff050405));
    g.fillRect (bounds.withX (bounds.getRight() - 1.0f).withWidth (1.0f));
    g.setColour (isMaster() ? ParagonColours::accent : track != nullptr ? track->colour : ParagonColours::textDim);
    g.fillRect (bounds.withHeight (4.0f).reduced (1.0f, 0.0f));
    if (selected)
    {
        g.setColour (ParagonColours::accent.withAlpha (0.6f));
        g.drawRect (bounds.reduced (1.0f, 0.0f).withTrimmedTop (4.0f), 1.0f);
    }

    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (12.5f));
    g.drawFittedText (isMaster() ? juce::String ("MASTER") : track != nullptr ? track->name : juce::String(),
                      nameArea.toNearestInt(), juce::Justification::centred, 1, 0.8f);
    g.setColour (ParagonColours::textDim);
    g.setFont (font (8.5f));
    if (isMaster() && owner.isRackShown())
        g.drawText (owner.isShowingSends() ? "NO SENDS" : "INSERTS", nameArea.translated (0.0f, nameArea.getHeight()).withHeight (12.0f), juce::Justification::centred, false);
    else if (owner.isRackShown() && owner.isShowingSends() && track != nullptr && track->isFx())
        g.drawText ("NO SENDS (AN FX CHANNEL)", rackArea, juce::Justification::centredTop, true);
    // The line between the inserts before the fader and those after it.
    if (const auto preFader = owner.studio.getPreFaderSlots (busId); preFader < shownSlots && owner.isRackShown() && ! owner.isShowingSends())
    {
        const auto& first = slots[(size_t) preFader];
        const auto y = preFader == 0 ? (float) first.getY() - 2.0f : (float) first.getY() - (float) slotGap * 0.5f - 0.5f;
        g.setColour (ParagonColours::accent);
        g.fillRect ((float) first.getX(), y, (float) first.getWidth(), 1.5f);
    }
    if (track != nullptr)
        g.drawText (panText (track->pan), panReadoutArea, juce::Justification::centred, false);

    // The level: PR7-C's meter, a column for each side (the master's) or one for the track.
    const auto columns = isMaster() ? 2 : 1;
    const auto columnWidth = (meterArea.getWidth() - (float) (columns - 1) * 2.0f) / (float) columns;
    for (int c = 0; c < columns; ++c)
    {
        const auto& reading = c == 0 ? left : right;
        const auto column = meterArea.withX (meterArea.getX() + (float) c * (columnWidth + 2.0f)).withWidth (columnWidth);
        LevelMeter::paintSegments (g, column, LevelMeter::proportionForDb (reading.db),
                                   reading.heldDb > LevelMeter::minDb ? LevelMeter::proportionForDb (reading.heldDb) : -1.0f);
    }

    g.setColour (ParagonColours::text);
    g.setFont (font (11.0f));
    g.drawText (dbText (volume.getValue()), readoutArea, juce::Justification::centred, false);
}

void MixerView::Strip::resized()
{
    auto area = getLocalBounds().reduced (6, 0).withTrimmedTop (8).withTrimmedBottom (6);
    nameArea = area.removeFromTop (18).toFloat();
    output.setBounds (area.removeFromTop (14).reduced (2, 0));   // OUT (the master's: "INSERTS")
    rackArea = area.withHeight (40).toFloat().withTrimmedTop (4.0f);
    if (owner.isRackShown())
    {
        // The slots shown, smaller when there are many, so the fader keeps room.
        const auto below = (isMaster() ? 0 : 50) + 28 + 16 + 6 + 110;   // pan, the keys, the readout, a gap, the shortest fader
        const auto rowHeight = juce::jlimit (shortestSlot, slotHeight, (area.getHeight() - below) / shownSlots - slotGap);
        // The rack: the inserts shown, or the eight sends (in the same room).
        auto rack = area;
        for (int i = 0; i < (int) slots.size(); ++i)
        {
            if (i >= shownSlots)
                continue;
            slots[(size_t) i].setBounds (area.removeFromTop (rowHeight));
            area.removeFromTop (slotGap);
        }
        const auto sendRow = juce::jmax (shortestSlot, (shownSlots * (rowHeight + slotGap)) / pr7s::Project::maxSends - slotGap);
        for (auto& send : sendSlots)
        {
            send->setBounds (rack.removeFromTop (sendRow));
            rack.removeFromTop (slotGap);
        }
    }
    area.removeFromTop (6);
    if (! isMaster())
    {
        pan.setBounds (area.removeFromTop (34).withSizeKeepingCentre (34, 34));
        panReadoutArea = area.removeFromTop (12).toFloat();
        area.removeFromTop (4);
    }
    // M, S, R and W in a row (the master: R and W).
    auto keys = area.removeFromTop (22);
    const auto count = isMaster() ? 2 : 4;
    const auto keyWidth = (keys.getWidth() - 3 * (count - 1)) / count;
    for (auto* key : { &mute, &solo, &read, &write })
    {
        if (isMaster() && (key == &mute || key == &solo))
            continue;
        key->setBounds (keys.removeFromLeft (keyWidth));
        keys.removeFromLeft (3);
    }
    area.removeFromTop (6);
    readoutArea = area.removeFromBottom (16).toFloat();
    auto faderArea = area;
    const auto meterWidth = isMaster() ? 14 : 8;
    meterArea = faderArea.removeFromRight (meterWidth).reduced (0, ParagonLookAndFeel::eqFaderThumbRadius).toFloat();
    faderArea.removeFromRight (4);
    volume.setBounds (faderArea.withSizeKeepingCentre (juce::jmin (36, faderArea.getWidth()), faderArea.getHeight()));
}

void MixerView::Strip::mouseDown (const juce::MouseEvent&)
{
    if (! isMaster())
        owner.studio.selectTrack (busId);
}

//==============================================================================
MixerView::MixerView (pr7s::Studio& studioToUse) : studio (studioToUse)
{
    viewport.setViewedComponent (&holder, false);
    viewport.setScrollBarsShown (false, true);
    addAndMakeVisible (viewport);
    master = std::make_unique<Strip> (*this, pr7s::Engine::masterBus);
    addAndMakeVisible (*master);

    for (auto* button : { &windowButton, &closeButton })
    {
        button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }
    windowButton.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
    closeButton.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
    for (auto* button : { &insertsButton, &sendsButton })
    {
        button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        button->setColour (juce::TextButton::textColourOffId, ParagonColours::textDim);
        button->setColour (juce::TextButton::textColourOnId, ParagonColours::accent);
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }
    insertsButton.setTooltip ("Show each channel's inserts (its effects, top to bottom); click again to hide them (only the faders)");
    sendsButton.setTooltip ("Show each channel's sends (to the FX channels); click again to hide them (only the faders)");
    // The lit one again hides the rack; the other shows its own.
    insertsButton.onClick = [this] { rackShown && ! showSends ? setRackShown (false) : (setShowSends (false), setRackShown (true)); };
    sendsButton.onClick = [this] { rackShown && showSends ? setRackShown (false) : (setShowSends (true), setRackShown (true)); };
    if (auto* settings = studio.getSettings())
    {
        const auto rack = settings->getValue ("mixerRack", "inserts");
        showSends = rack == "sends";
        rackShown = rack != "none";
    }
    insertsButton.setToggleState (rackShown && ! showSends, juce::dontSendNotification);
    sendsButton.setToggleState (rackShown && showSends, juce::dontSendNotification);
    closeButton.setTooltip ("Close the mixer (F3 in its own window, Alt+F3 docked)");
    closeButton.onClick = [this] { if (onClose) onClose(); };
    windowButton.onClick = [this] { if (onWindowToggle) onWindowToggle(); };
    setInWindow (false);

    studio.addChangeListener (this);
    rebuild();
    startTimerHz (30);
}

MixerView::~MixerView()
{
    studio.removeChangeListener (this);
}

void MixerView::setInWindow (bool shouldBeInWindow)
{
    inWindow = shouldBeInWindow;
    windowButton.setButtonText (inWindow ? "DOCK" : "WINDOW");
    windowButton.setTooltip (inWindow ? "Back under the arrangement"
                                      : "Into a window of its own: maximize it, or put it on another screen (it remembers where it was)");
}

MixerView::Strip& MixerView::getStrip (int index)
{
    return index < (int) strips.size() ? *strips[(size_t) index] : *master;
}

void MixerView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuild();
}

void MixerView::rebuild()
{
    // A strip for each track, in the track list's order (kept if the tracks are the same).
    const auto& tracks = studio.getProject().tracks;
    auto same = strips.size() == tracks.size();
    for (size_t i = 0; same && i < tracks.size(); ++i)
        same = strips[i]->getBusId() == tracks[i].id;
    if (! same)
    {
        strips.clear();
        for (const auto& track : tracks)
        {
            strips.push_back (std::make_unique<Strip> (*this, track.id));
            holder.addAndMakeVisible (*strips.back());
        }
        resized();
    }
    for (auto& strip : strips)
        strip->refresh();
    master->refresh();
}

void MixerView::timerCallback()
{
    auto& engine = studio.getEngine();
    for (auto& strip : strips)
    {
        const auto peak = engine.takeTrackPeak (strip->getBusId(), 1);
        strip->setLevels (peak, peak);
        strip->followMix();
        if (showSends)
            for (int i = 0; i < pr7s::Project::maxSends; ++i)
                strip->getSendSlot (i).repaint();
    }
    master->followMix();
    master->setLevels (engine.getOutputMeter (0, 1).takePeak(), engine.getOutputMeter (1, 1).takePeak());
}

void MixerView::showInsertMenu (int busId, int slot)
{
    const auto* inserts = studio.getInserts (busId);
    if (inserts == nullptr)
        return;
    const auto effects = studio.getPluginHost().getEffects();
    juce::PopupMenu choose;
    for (int i = 0; i < effects.size(); ++i)
        choose.addItem (100 + i, effects[i].manufacturerName + ": " + effects[i].name);
    if (effects.isEmpty())
        choose.addItem (-1, "No VST3 effects found yet", false);

    juce::PopupMenu menu;
    const auto filled = slot < (int) inserts->size() && ! (*inserts)[(size_t) slot].isEmpty();
    const auto preFader = studio.getPreFaderSlots (busId);
    if (filled)
    {
        const auto& insert = (*inserts)[(size_t) slot];
        const auto* instance = studio.getEngine().getEffect (busId, slot);
        menu.addSectionHeader (insert.plugin.name + (slot < preFader ? " (before the fader)" : " (after the fader)"));
        menu.addItem (1, "Open its window", instance != nullptr && instance->hasEditor());
        menu.addItem (2, "Bypass", true, insert.bypass);
        menu.addSubMenu ("Replace with", choose);
        menu.addItem (3, "Remove");
        menu.addSeparator();
        menu.addItem (5, "Set as Last Pre-Fader Slot", slot + 1 != preFader);
        menu.addItem (6, "All Slots Before the Fader", preFader < pr7s::Project::maxInserts);
    }
    else
    {
        menu.addSectionHeader ("Insert " + juce::String (slot + 1) + ": add an effect" + (slot < preFader ? " (before the fader)" : " (after the fader)"));
        for (int i = 0; i < effects.size(); ++i)
            menu.addItem (100 + i, effects[i].manufacturerName + ": " + effects[i].name);
        if (effects.isEmpty())
            menu.addItem (-1, "No VST3 effects found yet", false);
    }
    menu.addSeparator();
    menu.addItem (4, effects.isEmpty() ? "Find my VST3 effects..." : "Find more VST3 effects...");

    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<MixerView> (this), busId, slot, filled, effects] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        auto& s = safe->studio;
        juce::String error;
        auto ok = true;
        if (chosen >= 100 && chosen - 100 < effects.size())
            ok = filled ? s.replaceInsert (busId, slot, effects[chosen - 100], &error)
                        : s.addInsert (busId, effects[chosen - 100], &error, slot);
        else if (chosen == 1 && safe->onOpenEffect)
            safe->onOpenEffect (busId, slot);
        else if (chosen == 2)
            s.setInsertBypass (busId, slot, ! s.getInserts (busId)->at ((size_t) slot).bypass);
        else if (chosen == 3)
            s.removeInsert (busId, slot);
        else if (chosen == 4 && safe->onFindPlugins)
            safe->onFindPlugins();
        else if (chosen == 5)
            s.setLastPreFaderSlot (busId, slot);
        else if (chosen == 6)
            s.setLastPreFaderSlot (busId, pr7s::Project::maxInserts - 1);
        if (! ok)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load the effect", error);
    });
}

void MixerView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0a0e));
    auto bar = getLocalBounds().removeFromTop (barHeight).toFloat();
    g.setColour (juce::Colour (0xff100c11));
    g.fillRect (bar);
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (bar.withY (bar.getBottom() - 1.0f).withHeight (1.0f));
    g.fillRect (bar.withHeight (1.0f));
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (9.5f));
    g.drawText ("MIXER", bar.withTrimmedLeft (14.0f), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (font (11.0f, juce::Font::plain));
    g.drawText (! rackShown ? "Each track's fader, pan and keys (INSERTS or SENDS shows their racks again)"
              : showSends ? "Each track's sends: some of its sound to an FX channel (before or after its fader), back into the master"
                          : "Each track: its instrument, its inserts (top to bottom; those below an orange line after the fader), its fader, its output",
                bar.withTrimmedLeft (70.0f).withTrimmedRight (330.0f), juce::Justification::centredLeft, true);
    if (strips.empty())
    {
        g.setFont (font (13.0f, juce::Font::plain));
        g.drawText ("Add a track to see its channel here", getLocalBounds().withTrimmedTop (barHeight).withTrimmedRight (masterWidth),
                    juce::Justification::centred, false);
    }
}

void MixerView::resized()
{
    auto area = getLocalBounds();
    auto bar = area.removeFromTop (barHeight).reduced (8, 4);
    closeButton.setBounds (bar.removeFromRight (64));
    bar.removeFromRight (6);
    windowButton.setBounds (bar.removeFromRight (74));
    bar.removeFromRight (16);
    sendsButton.setBounds (bar.removeFromRight (62));
    bar.removeFromRight (4);
    insertsButton.setBounds (bar.removeFromRight (72));

    master->setBounds (area.removeFromRight (masterWidth));
    area.removeFromRight (3);
    viewport.setBounds (area);
    const auto height = area.getHeight() - (viewport.canScrollHorizontally() ? viewport.getScrollBarThickness() : 0);
    holder.setSize (juce::jmax (area.getWidth(), (int) strips.size() * stripWidth), juce::jmax (0, height));
    for (size_t i = 0; i < strips.size(); ++i)
        strips[i]->setBounds ((int) i * stripWidth, 0, stripWidth, holder.getHeight());
}

//==============================================================================
void MixerView::setShowSends (bool shouldShow)
{
    showSends = shouldShow;
    rackChanged();
}

void MixerView::setRackShown (bool shouldShow)
{
    rackShown = shouldShow;
    rackChanged();
    if (onMinimumHeightChange)
        onMinimumHeightChange();
}

void MixerView::rackChanged()
{
    insertsButton.setToggleState (rackShown && ! showSends, juce::dontSendNotification);
    sendsButton.setToggleState (rackShown && showSends, juce::dontSendNotification);
    if (auto* settings = studio.getSettings())
        settings->setValue ("mixerRack", ! rackShown ? "none" : showSends ? "sends" : "inserts");
    for (auto& strip : strips)
    {
        strip->refresh();
        strip->resized();
    }
    master->refresh();
    master->resized();
    repaint();
}

int MixerView::getPreferredHeight() const
{
    auto most = master->getShownSlots();
    for (const auto& strip : strips)
        most = juce::jmax (most, strip->getShownSlots());
    return barHeight + Strip::fixedHeight + 110 + (rackShown ? most * (slotHeight + slotGap) : 0) + viewport.getScrollBarThickness();
}

int MixerView::getMinimumHeight() const
{
    // The rack's room: the most slots any strip shows, at their smallest.
    auto rack = 0;
    if (rackShown)
    {
        auto most = master->getShownSlots();
        for (const auto& strip : strips)
            most = juce::jmax (most, strip->getShownSlots());
        rack = most * (Strip::shortestSlot + slotGap);
    }
    return barHeight + Strip::fixedHeight + Strip::shortestFader + rack + viewport.getScrollBarThickness();
}

void MixerView::showOutputMenu (int busId)
{
    const auto* track = studio.getProject().findTrack (busId);
    if (track == nullptr || track->isChannel())
        return;
    juce::PopupMenu menu;
    menu.addSectionHeader ("OUTPUT");
    menu.addItem (1, "Master", true, track->output == 0);
    for (const auto& other : studio.getProject().tracks)
        if (other.isGroup())
            menu.addItem (1000 + other.id, other.name, true, track->output == other.id);
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<MixerView> (this), busId] (int chosen)
    {
        if (safe != nullptr && chosen > 0)
            safe->studio.setOutput (busId, chosen == 1 ? 0 : chosen - 1000);
    });
}

void MixerView::showSendMenu (int busId, int slot)
{
    const auto* track = studio.getProject().findTrack (busId);
    if (track == nullptr || track->isFx())
        return;
    const auto current = juce::isPositiveAndBelow (slot, (int) track->sends.size()) ? track->sends[(size_t) slot] : pr7s::Send {};
    const auto effects = studio.getPluginHost().getEffects();
    juce::PopupMenu menu, add;
    menu.addSectionHeader ("SEND " + juce::String (slot + 1));
    auto anyFx = false;
    for (const auto& other : studio.getProject().tracks)
        if (other.isFx())
        {
            menu.addItem (10000 + other.id, other.name, true, current.target == other.id);
            anyFx = true;
        }
    if (! anyFx)
        menu.addItem (-1, "No FX channels yet: add one below", false);
    menu.addSeparator();
    if (current.target != 0)
    {
        menu.addItem (1, "On", true, current.on);
        menu.addItem (2, current.preFader ? "Move to Post-Fader" : "Move to Pre-Fader");
        menu.addItem (3, "Remove");
        menu.addItem (4, "Bypass All Sends of This Channel", true, track->sendsBypass);
        menu.addSeparator();
    }
    // (As the manual's "Add Effect Track to Send": a new FX channel, and this send to it.)
    add.addItem (5, "Empty");
    for (int i = 0; i < effects.size(); ++i)
        add.addItem (20000 + i, effects[i].manufacturerName + ": " + effects[i].name);
    menu.addSubMenu ("Add FX Channel to Send", add);
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<MixerView> (this), busId, slot, current, effects] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        auto& s = safe->studio;
        auto changed = current;
        if (chosen >= 20000 || chosen == 5)
        {
            juce::String error;
            const auto fx = chosen == 5 ? s.addFxChannel (nullptr) : s.addFxChannel (&effects.getReference (chosen - 20000), &error);
            if (fx == 0)
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load the effect", error);
                return;
            }
            s.selectTrack (busId);
            changed = { fx, current.target != 0 ? current.levelDb : 0.0f, true, current.preFader };
        }
        else if (chosen >= 10000)
        {
            changed.target = chosen - 10000;
            if (current.target == 0)
                changed = { chosen - 10000, 0.0f, true, false };   // a new send: on, 0 dB, after the fader (our choice)
        }
        else if (chosen == 1) changed.on = ! current.on;
        else if (chosen == 2) changed.preFader = ! current.preFader;
        else if (chosen == 3) changed = {};
        else if (chosen == 4)
        {
            if (const auto* t = s.getProject().findTrack (busId))
                s.setSendsBypass (busId, ! t->sendsBypass);
            return;
        }
        s.setSend (busId, slot, changed);
    });
}

//==============================================================================
MixerView::SendSlot::SendSlot (MixerView& ownerView, int bus, int index) : owner (ownerView), busId (bus), slot (index) {}

const pr7s::Send* MixerView::SendSlot::send() const
{
    const auto* track = owner.studio.getProject().findTrack (busId);
    return track != nullptr && juce::isPositiveAndBelow (slot, (int) track->sends.size()) && track->sends[(size_t) slot].target != 0
             ? &track->sends[(size_t) slot] : nullptr;
}

void MixerView::SendSlot::refresh()
{
    const auto* s = send();
    const auto* fx = s != nullptr ? owner.studio.getProject().findTrack (s->target) : nullptr;
    setTooltip (fx == nullptr ? "Send " + juce::String (slot + 1) + ": click to send to an FX channel"
                              : "Send " + juce::String (slot + 1) + " to " + fx->name
                                  + ": drag for its level (Shift: fine, Ctrl-click: 0 dB); the dot turns it on or off; right-click for more");
    repaint();
}

void MixerView::SendSlot::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colour (0xff0b090c));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
    const auto* s = send();
    const auto* track = owner.studio.getProject().findTrack (busId);
    if (s == nullptr || track == nullptr)
    {
        g.setColour (ParagonColours::textDim.withAlpha (0.6f));
        g.setFont (font (11.0f, juce::Font::plain));
        g.drawText ("+", bounds, juce::Justification::centred, false);
        return;
    }
    const auto* fx = owner.studio.getProject().findTrack (s->target);
    const auto active = s->on && ! track->sendsBypass;
    // Its level: a bar from the left (the fader's travel), in the FX channel's colour.
    const auto db = track->automationRead ? owner.studio.getEngine().getSendNowDb (busId, slot) : s->levelDb;
    auto bar = bounds.reduced (1.5f).withTrimmedLeft (12.0f);
    const auto colour = fx != nullptr ? fx->colour : ParagonColours::accent;
    g.setColour (colour.withAlpha (active ? 0.45f : 0.15f));
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * pr7s::volumeToTravel (db)), 2.0f);
    // The on/off dot.
    const auto dot = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ bounds.getX() + 7.0f, bounds.getCentreY() });
    g.setColour (active ? ParagonColours::accent : ParagonColours::accentDim.darker (0.6f));
    g.fillEllipse (dot);
    g.setColour (active ? ParagonColours::text : ParagonColours::textDim);
    g.setFont (font (juce::jmin (11.0f, bounds.getHeight() * 0.8f)));
    g.drawFittedText (fx != nullptr ? fx->name : juce::String ("?"), bar.toNearestInt().withTrimmedLeft (2).withTrimmedRight (s->preFader ? 22 : 2),
                      juce::Justification::centredLeft, 1, 0.7f);
    if (s->preFader)
    {
        g.setColour (ParagonColours::accent);
        g.drawText ("PRE", bar.removeFromRight (22.0f), juce::Justification::centred, false);
    }
}

void MixerView::SendSlot::mouseDown (const juce::MouseEvent& event)
{
    dragging = false;
    const auto* s = send();
    if (event.mods.isPopupMenu() || s == nullptr)
    {
        owner.showSendMenu (busId, slot);
        return;
    }
    if (event.position.x < 14.0f)   // the dot: on or off
    {
        auto changed = *s;
        changed.on = ! s->on;
        owner.studio.setSend (busId, slot, changed);
        return;
    }
    if (event.mods.isCommandDown())   // Ctrl-click: 0 dB (the manual's default send level; ours is 0 dB)
    {
        owner.studio.setSendLevel (busId, slot, 0.0f);
        return;
    }
    startTravel = pr7s::volumeToTravel (s->levelDb);
    dragging = true;
    owner.studio.grabControl (busId, pr7s::Studio::control (pr7s::AutomationLane::Target::send, slot));
}

void MixerView::SendSlot::mouseDrag (const juce::MouseEvent& event)
{
    if (! dragging)
        return;
    const auto width = juce::jmax (20.0f, (float) getWidth() - 14.0f);
    const auto travel = startTravel + (float) event.getDistanceFromDragStartX() / width * (event.mods.isShiftDown() ? 0.2f : 1.0f);
    owner.studio.setSendLevel (busId, slot, pr7s::travelToVolume (juce::jlimit (0.0f, 1.0f, travel)), false);
}

void MixerView::SendSlot::mouseUp (const juce::MouseEvent&)
{
    if (! dragging)
        return;
    dragging = false;
    if (const auto* s = send())
        owner.studio.setSendLevel (busId, slot, s->levelDb, true);   // the drag: one undo step
    owner.studio.releaseControl (busId, pr7s::Studio::control (pr7s::AutomationLane::Target::send, slot));
}
