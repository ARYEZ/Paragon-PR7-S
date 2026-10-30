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
    }
    const auto preFader = owner.studio.getPreFaderSlots (busId);
    for (int i = 0; i < (int) slots.size(); ++i)
    {
        auto& slot = slots[(size_t) i];
        slot.setVisible (i < shownSlots);
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
    const auto selected = ! isMaster() && owner.studio.getSelectedTrack() == busId;
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
    g.drawText ("INSERTS", nameArea.translated (0.0f, nameArea.getHeight()).withHeight (12.0f), juce::Justification::centred, false);
    // The line between the inserts before the fader and those after it.
    if (const auto preFader = owner.studio.getPreFaderSlots (busId); preFader < shownSlots)
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
    area.removeFromTop (14);   // "INSERTS"
    // The slots shown, smaller when there are many, so the fader keeps room.
    const auto below = (isMaster() ? 0 : 50) + 28 + 16 + 6 + 110;   // pan, the keys, the readout, a gap, the shortest fader
    const auto rowHeight = juce::jlimit (11, slotHeight, (area.getHeight() - below) / shownSlots - slotGap);
    for (int i = 0; i < (int) slots.size(); ++i)
    {
        if (i >= shownSlots)
            continue;
        slots[(size_t) i].setBounds (area.removeFromTop (rowHeight));
        area.removeFromTop (slotGap);
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
    g.drawText ("Each track: its instrument, its inserts (top to bottom; those below an orange line after the fader), its fader, into the master",
                bar.withTrimmedLeft (70.0f).withTrimmedRight (170.0f), juce::Justification::centredLeft, true);
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

    master->setBounds (area.removeFromRight (masterWidth));
    area.removeFromRight (3);
    viewport.setBounds (area);
    const auto height = area.getHeight() - (viewport.canScrollHorizontally() ? viewport.getScrollBarThickness() : 0);
    holder.setSize (juce::jmax (area.getWidth(), (int) strips.size() * stripWidth), juce::jmax (0, height));
    for (size_t i = 0; i < strips.size(); ++i)
        strips[i]->setBounds ((int) i * stripWidth, 0, stripWidth, holder.getHeight());
}
