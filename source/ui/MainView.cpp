#include "MainView.h"

#include "ui/AutomationKeys.h"
#include "ui/Logo.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/Pearl.h"

namespace
{
    juce::FontOptions font (float height, int style = juce::Font::bold) { return ParagonLookAndFeel::textFont (height, style); }
}

MainView::MainView (pr7s::Studio& studioToUse)
    : studio (studioToUse),
      transport (studioToUse),
      trackList (studioToUse, timeline),
      arrange (studioToUse, timeline),
      keyEditor (studioToUse, arrange),
      mixer (studioToUse)
{
    for (auto* button : { &newButton, &openButton, &saveButton, &saveAsButton, &importButton, &exportButton, &undoButton, &redoButton, &editorButton, &mixerButton,
                          &pluginsButton, &settingsButton })
    {
        button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        button->setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }
    newButton.setTooltip ("A new song (Ctrl+N)");
    openButton.setTooltip ("Open a song (Ctrl+O)");
    saveButton.setTooltip ("Save the song (Ctrl+S)");
    saveAsButton.setTooltip ("Save the song under a new name (Ctrl+Shift+S)");
    exportButton.setTooltip ("Export the mix to a WAV file: the whole song or the cycle");
    importButton.setTooltip ("Import a MIDI file (.mid): a track for each of its tracks. Or drop one on the arrangement, at a bar");
    importButton.onClick = [this] { importMidiFile(); };
    undoButton.setTooltip ("Undo (Ctrl+Z)");
    redoButton.setTooltip ("Redo (Ctrl+Shift+Z)");
    editorButton.setTooltip ("The key editor: the selected part's notes (Ctrl+E; or double-click a part)");
    editorButton.setColour (juce::TextButton::textColourOnId, ParagonColours::accent);
    mixerButton.setTooltip ("The mixer under the arrangement (Alt+F3). F3: the mixer in a window of its own");
    mixerButton.setColour (juce::TextButton::textColourOnId, ParagonColours::accent);
    pluginsButton.setTooltip ("Find your VST3 instruments and effects");
    settingsButton.setTooltip ("Audio interface, MIDI inputs and the finish");
    newButton.onClick = [this] { newProject(); };
    openButton.onClick = [this] { openProject(); };
    saveButton.onClick = [this] { saveProject(); };
    saveAsButton.onClick = [this] { saveProjectAs(); };
    exportButton.onClick = [this] { exportMix(); };
    undoButton.onClick = [this] { studio.undo(); };
    redoButton.onClick = [this] { studio.redo(); };
    editorButton.onClick = [this] { isKeyEditorOpen() ? closeKeyEditor() : openKeyEditor(); };
    mixerButton.onClick = [this] { toggleMixerDocked(); };
    pluginsButton.onClick = [this] { openPlugins(); };
    settingsButton.onClick = [this] { openSettings(); };
    // Read and write automation for all tracks (the manual's toolbar buttons).
    for (auto* key : { &readAllButton, &writeAllButton })
    {
        key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key->setWantsKeyboardFocus (false);
        addAndMakeVisible (*key);
    }
    readAllButton.getProperties().set (ParagonLookAndFeel::litColourProperty, (juce::int64) AutomationKeys::readColour.getARGB());
    writeAllButton.getProperties().set (ParagonLookAndFeel::litColourProperty, (juce::int64) AutomationKeys::writeColour.getARGB());
    readAllButton.setTooltip ("Read automation for all tracks on or off (Alt+R): lit while any track's R is on");
    writeAllButton.setTooltip ("Write automation for all tracks on or off (Alt+W): lit while any track's W is on");
    readAllButton.onClick = [this] { studio.setAutomationReadAll (! studio.anyAutomationRead()); };
    writeAllButton.onClick = [this] { studio.setAutomationWriteAll (! studio.anyAutomationWrite()); };

    addAndMakeVisible (transport);
    addAndMakeVisible (trackList);
    addAndMakeVisible (arrange);
    addAndMakeVisible (tools);
    arrange.onToolChange = [this] { tools.refresh(); };
    addChildComponent (keyEditor);
    addChildComponent (mixer);
    keyEditor.onClose = [this] { closeKeyEditor(); };
    mixer.onClose = [this] { closeMixer(); };
    // Not straight away: the click that asked is still going on in the window.
    keyEditor.onWindowToggle = [safe = juce::Component::SafePointer<MainView> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->setKeyEditorInWindow (! safe->isKeyEditorInWindow()); });
    };
    mixer.onWindowToggle = [safe = juce::Component::SafePointer<MainView> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->setMixerInWindow (! safe->isMixerInWindow()); });
    };
    mixer.onOpenEffect = [this] (int busId, int slot) { openEffectWindow (busId, slot); };
    mixer.onFindPlugins = [this] { openPlugins(); };
    if (auto* settings = studio.getSettings())
    {
        editorZone.inWindow = settings->getBoolValue ("keyEditorInWindow", false);
        mixerZone.inWindow = settings->getBoolValue ("mixerInWindow", false);
        editorZone.dockedHeight = settings->getIntValue ("keyEditorHeight", 0);
        mixerZone.dockedHeight = settings->getIntValue ("mixerHeight", 0);
    }
    // Each view's Auto-Scroll and its settings, kept for next time.
    timeline.autoScroll.load (studio.getSettings(), "arrange");
    keyEditor.getAutoScrollSettings().load (studio.getSettings(), "keyEditor");
    keyEditor.autoScrollChanged();
    keyEditor.onAutoScrollChange = [this] { keyEditor.getAutoScrollSettings().save (studio.getSettings(), "keyEditor"); };
    timeline.onAutoScrollChange = [this]
    {
        trackList.refreshAutoScroll();
        timeline.autoScroll.save (studio.getSettings(), "arrange");
    };
    trackList.onAutoScrollToggle = [this]
    {
        timeline.autoScroll.toggle();
        timeline.onAutoScrollChange();
    };
    trackList.refreshAutoScroll();
    zoneDivider = std::make_unique<ZoneDivider> (*this);
    addChildComponent (*zoneDivider);
    mixer.onMinimumHeightChange = [this] { resized(); };   // (after the divider: resized() places it)
    for (auto* bar : { &arrangeAcross, &arrangeDown })
    {
        bar->setAutoHide (false);
        bar->setWantsKeyboardFocus (false);
        bar->addListener (this);
        addAndMakeVisible (*bar);
    }
    keyEditor.setInWindow (editorZone.inWindow);
    mixer.setInWindow (mixerZone.inWindow);
    arrange.onEditNotes = [this] { openKeyEditor(); };
    // Told of clicks in the work areas, to know where the keys go.
    arrange.addMouseListener (this, true);
    trackList.addMouseListener (this, true);
    keyEditor.addMouseListener (this, true);

    trackList.onOpenInstrument = [this] (int trackId) { openInstrumentWindow (trackId); };
    trackList.onFindPlugins = [this] { openPlugins(); };
    trackList.onOpenEffect = [this] (int busId, int slot) { openEffectWindow (busId, slot); };
    timeline.onScrollOrZoom = [this]
    {
        trackList.resized();
        trackList.repaint();
        arrange.repaint();
        refreshScrollBars();
    };

    // An instrument's window closes before the instrument goes.
    studio.onInstrumentGoing = [this] (int trackId)
    {
        if (trackId == 0)
            instrumentWindows.clear();
        else
            instrumentWindows.erase (trackId);
    };
    // And an effect's before the effect (nullptr: all of them).
    studio.onEffectGoing = [this] (juce::AudioProcessor* effect)
    {
        if (effect == nullptr)
            effectWindows.clear();
        else
            effectWindows.erase (effect);
    };

    studio.addChangeListener (this);
    Finish::changes().addChangeListener (this);
    setWantsKeyboardFocus (true);
    refreshHeader();
}

MainView::~MainView()
{
    arrangeAcross.removeListener (this);
    arrangeDown.removeListener (this);
    mixer.onMinimumHeightChange = nullptr;
    timeline.onAutoScrollChange = nullptr;
    keyEditor.onAutoScrollChange = nullptr;
    editorZone.window.reset();
    mixerZone.window.reset();
    studio.onEffectGoing = nullptr;
    effectWindows.clear();
    arrange.removeMouseListener (this);
    trackList.removeMouseListener (this);
    keyEditor.removeMouseListener (this);
    studio.onInstrumentGoing = nullptr;
    instrumentWindows.clear();
    Finish::changes().removeChangeListener (this);
    studio.removeChangeListener (this);
}

void MainView::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &Finish::changes())
    {
        resized();
        repaint();
        return;
    }
    refreshHeader();
    refreshScrollBars();
}

void MainView::refreshScrollBars()
{
    // Across: from the song's start to past its end (or to what's in view); down: the rows and the room under them.
    const auto visibleBeats = (double) arrange.getWidth() / timeline.pixelsPerBeat;
    arrangeAcross.setRangeLimits (0.0, juce::jmax (Timeline::scrollLength (studio), timeline.firstBeat + visibleBeats), juce::dontSendNotification);
    arrangeAcross.setCurrentRange (timeline.firstBeat, visibleBeats, juce::dontSendNotification);
    const auto lanes = (double) juce::jmax (0, arrange.getHeight() - Timeline::rulerHeight);
    arrangeDown.setRangeLimits (0.0, juce::jmax (lanes, (double) (timeline.contentHeight (studio) + Timeline::bottomRoom)), juce::dontSendNotification);
    arrangeDown.setCurrentRange (timeline.scrollY, lanes, juce::dontSendNotification);
}

void MainView::scrollBarMoved (juce::ScrollBar* bar, double newRangeStart)
{
    if (bar == &arrangeAcross)
        timeline.firstBeat = juce::jmax (0.0, newRangeStart);
    else
        timeline.scrollY = juce::jmax (0, juce::roundToInt (newRangeStart));
    if (timeline.onScrollOrZoom)
        timeline.onScrollOrZoom();
}

void MainView::setArrangeAutoScroll (bool on)
{
    timeline.autoScroll.on = on;
    timeline.autoScroll.suspended = false;
    timeline.onAutoScrollChange();
}

void MainView::refreshHeader()
{
    if (editorZone.window != nullptr)
    {
        const auto* part = keyEditor.getPart();
        editorZone.window->setName ("Key Editor: " + (part != nullptr && part->name.isNotEmpty() ? part->name : juce::String ("no part")));
    }
    undoButton.setEnabled (studio.canUndo());
    redoButton.setEnabled (studio.canRedo());
    readAllButton.setToggleState (studio.anyAutomationRead(), juce::dontSendNotification);
    writeAllButton.setToggleState (studio.anyAutomationWrite(), juce::dontSendNotification);
    const auto file = studio.getFile();
    const auto title = "Paragon PR7-S - " + (file == juce::File() ? juce::String ("Untitled") : file.getFileNameWithoutExtension())
                     + (studio.isModified() ? " *" : "");
    if (title != lastTitle)
    {
        lastTitle = title;
        if (onTitleChange)
            onTitleChange (title);
    }
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (ParagonColours::background);

    // The header: the emblem, PARAGON and the PR7-S mark.
    auto header = getLocalBounds().removeFromTop (headerHeight).toFloat();
    const juce::Rectangle<float> emblem { 16.0f, 12.0f, 30.0f, 28.0f };
    juce::Path triangle;
    triangle.addTriangle (emblem.getTopLeft(), emblem.getTopRight(), { emblem.getCentreX(), emblem.getBottom() });
    g.setColour (ParagonColours::glow.withAlpha (0.3f));
    g.strokePath (triangle, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (ParagonColours::glow);
    g.strokePath (triangle, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (ParagonColours::text);
    g.setFont (font (18.0f, juce::Font::plain));
    g.drawText ("PARAGON", juce::Rectangle<float> (56.0f, 0.0f, 110.0f, header.getHeight()), juce::Justification::centredLeft, false);
    Logo::drawMark (g, juce::Rectangle<float> (150.0f, 18.0f, 110.0f, 16.0f), juce::Justification::centredLeft,
                    { ParagonColours::glow, ParagonColours::glow, {}, {}, 1.0f, true });
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (header.withY (header.getBottom() - 1.0f).withHeight (1.0f));

    // The work area's frame.
    const auto middle = getLocalBounds().withTrimmedTop (headerHeight + transportHeight + toolbarHeight).toFloat().reduced (6.0f, 2.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRect (middle, 1.0f);
}

void MainView::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (headerHeight).reduced (8, 10);
    header.removeFromLeft (280);
    for (auto* button : { &newButton, &openButton, &saveButton, &saveAsButton, &importButton, &exportButton })
    {
        button->setBounds (header.removeFromLeft (button == &saveAsButton ? 84 : button == &exportButton || button == &importButton ? 76 : 66));
        header.removeFromLeft (6);
    }
    header.removeFromLeft (18);
    for (auto* button : { &undoButton, &redoButton })
    {
        button->setBounds (header.removeFromLeft (66));
        header.removeFromLeft (6);
    }
    header.removeFromLeft (18);
    editorButton.setBounds (header.removeFromLeft (100));
    header.removeFromLeft (6);
    mixerButton.setBounds (header.removeFromLeft (76));
    header.removeFromLeft (18);
    readAllButton.setBounds (header.removeFromLeft (34));
    header.removeFromLeft (4);
    writeAllButton.setBounds (header.removeFromLeft (34));
    settingsButton.setBounds (header.removeFromRight (92));
    header.removeFromRight (6);
    pluginsButton.setBounds (header.removeFromRight (86));

    transport.setBounds (area.removeFromTop (transportHeight).reduced (2, 0));
    tools.setBounds (area.removeFromTop (toolbarHeight).reduced (7, 3).withTrimmedTop (2));

    auto middle = area.reduced (7, 3);
    zoneDivider->setVisible (false);
    if (auto* zone = dockedZone())
    {
        zone->content.setBounds (middle.removeFromBottom (heightFor (*zone, middle.getHeight())));
        zoneDivider->setBounds (middle.removeFromBottom (dividerHeight));
        zoneDivider->setVisible (true);
    }
    // The scroll bars: across under the arrangement, down beside its rows.
    auto acrossRow = middle.removeFromBottom (scrollBarSize);
    trackList.setBounds (middle.removeFromLeft (trackListWidth));
    middle.removeFromLeft (2);
    acrossRow.removeFromLeft (trackListWidth + 2);
    arrangeDown.setBounds (middle.removeFromRight (scrollBarSize).withTrimmedTop (Timeline::rulerHeight));
    arrange.setBounds (middle);
    arrangeAcross.setBounds (acrossRow.withTrimmedRight (scrollBarSize));
    refreshScrollBars();
}

MainView::Zone* MainView::dockedZone() noexcept
{
    for (auto* zone : { &editorZone, &mixerZone })
        if (zone->open && ! zone->inWindow)
            return zone;
    return nullptr;
}

int MainView::minimumHeight (Zone& zone) const
{
    return &zone == &mixerZone ? mixer.getMinimumHeight() : KeyEditor::minimumHeight;
}

int MainView::heightFor (Zone& zone, int middleHeight) const
{
    // As dragged (at first a little over half), between its least and what
    // leaves the arrangement its room (its least wins if there isn't enough).
    const auto least = juce::jmin (minimumHeight (zone), middleHeight);
    const auto most = juce::jlimit (least, middleHeight, middleHeight - minimumArrangeHeight - dividerHeight);
    // (Not dragged yet: a little over half, or the mixer's whole rack and a full fader if that's more.)
    auto wanted = zone.dockedHeight > 0 ? zone.dockedHeight : juce::roundToInt ((float) middleHeight * 0.6f);
    if (zone.dockedHeight <= 0 && &zone == &mixerZone)
        wanted = juce::jmax (wanted, mixer.getPreferredHeight());
    return juce::jlimit (least, most, wanted);
}

void MainView::setDockedHeight (int height)
{
    auto* zone = dockedZone();
    if (zone == nullptr)
        return;
    const auto middleHeight = getHeight() - headerHeight - transportHeight - toolbarHeight - 6;
    zone->dockedHeight = juce::jmax (1, height);   // (0 would mean the default)
    zone->dockedHeight = heightFor (*zone, middleHeight);   // kept to what it can be
    if (auto* settings = studio.getSettings())
        settings->setValue (zone->settingsKey + "Height", zone->dockedHeight);
    resized();
}

//==============================================================================
MainView::ZoneDivider::ZoneDivider (MainView& ownerToUse) : owner (ownerToUse)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setRepaintsOnMouseActivity (true);
    setTooltip ("Drag to make the lower zone taller or shorter");
}

void MainView::ZoneDivider::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0b090c));
    g.fillRect (bounds);
    const auto lit = isMouseOverOrDragging();
    g.setColour (lit ? ParagonColours::accent : ParagonColours::panelEdge);
    g.fillRect (bounds.withSizeKeepingCentre (bounds.getWidth(), 1.0f));
    // The grip: three short lines in the middle.
    g.setColour (lit ? ParagonColours::accent : ParagonColours::textDim);
    for (int i = -1; i <= 1; ++i)
        g.fillRoundedRectangle (bounds.withSizeKeepingCentre (36.0f, 1.5f).translated (0.0f, (float) i * 2.5f), 0.75f);
}

void MainView::ZoneDivider::mouseDown (const juce::MouseEvent&)
{
    auto* zone = owner.dockedZone();
    heightAtDown = zone != nullptr ? zone->content.getHeight() : 0;
}

void MainView::ZoneDivider::mouseDrag (const juce::MouseEvent& event)
{
    owner.setDockedHeight (heightAtDown - event.getDistanceFromDragStartY());
}

void MainView::ZoneDivider::mouseUp (const juce::MouseEvent&)
{
    repaint();
}

//==============================================================================
bool MainView::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const auto code = key.getKeyCode();
    // A letter only if the key is one (on Windows an F-key's or the number
    // pad's code, upper-cased, would come out as a letter: F3 as R).
    const auto letter = (code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z')
                          ? juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) code) : (juce::juce_wchar) 0;

    if (isKeyEditorOpen() && keysToEditor && keyEditor.keyPressed (key))
        return true;
    // Automation: Alt+R / Alt+W for all tracks (before R, which records); Esc puts dragged points back.
    if (mods.isAltDown() && ! mods.isCommandDown() && (letter == 'R' || letter == 'W'))
    {
        if (letter == 'R')
            studio.setAutomationReadAll (! studio.anyAutomationRead());
        else
            studio.setAutomationWriteAll (! studio.anyAutomationWrite());
        return true;
    }
    if (key == juce::KeyPress::escapeKey && arrange.cancelDrag())
        return true;
    // Copy, cut, paste and select all are the editor's wherever the last click
    // was (the arrangement has none of its own yet), so notes copied in one
    // part paste into another just selected in the arrangement.
    const auto editorsAnyway = (mods.isCommandDown() && (letter == 'C' || letter == 'X' || letter == 'V'))
                            || (! mods.isCommandDown() && mods.isShiftDown() && letter == 'A');
    if (isKeyEditorOpen() && editorsAnyway && keyEditor.keyPressed (key))
        return true;
    // Ctrl+D duplicates the notes after a click in the editor, the part otherwise.
    if (isKeyEditorOpen() && keysToEditor && mods.isCommandDown() && letter == 'D' && keyEditor.keyPressed (key))
        return true;

    if (mods.isCommandDown())
    {
        switch (letter)
        {
            case 'Z': mods.isShiftDown() ? studio.redo() : studio.undo(); return true;
            case 'Y': studio.redo(); return true;
            case 'S': mods.isShiftDown() ? saveProjectAs() : saveProject(); return true;
            case 'O': openProject(); return true;
            case 'N': newProject(); return true;
            case 'D': arrange.duplicateSelected(); return true;
            case 'E': isKeyEditorOpen() ? closeKeyEditor() : openKeyEditor(); return true;
            default: return false;
        }
    }

    if (key.getKeyCode() == juce::KeyPress::F3Key)      { mods.isAltDown() ? toggleMixerDocked() : toggleMixerWindow(); return true; }
    if (isKeyEditorOpen() && key == juce::KeyPress::escapeKey) { closeKeyEditor(); return true; }
    if (isMixerOpen() && key == juce::KeyPress::escapeKey)     { closeMixer(); return true; }
    if (key == juce::KeyPress::spaceKey)                { studio.togglePlay(); return true; }
    if (key == juce::KeyPress::homeKey)                 { studio.goToStart(); return true; }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        arrange.hasSelectedPoints() ? arrange.deleteSelectedPoints() : arrange.deleteSelected();
        return true;
    }
    // The arrangement's tools: 1 and 3 to 9 (not after a click in the key editor: it has its own), F9 / F10, Alt+X.
    if (! mods.isAnyModifierKeyDown() && ! (isKeyEditorOpen() && keysToEditor))
        for (const auto t : ArrangeView::tools)
            if (key.getTextCharacter() == (juce::juce_wchar) ('0' + ArrangeView::toolKey (t)))
            {
                arrange.setTool (t);
                return true;
            }
    if (key.getKeyCode() == juce::KeyPress::F9Key || key.getKeyCode() == juce::KeyPress::F10Key)
    {
        arrange.nextTool (key.getKeyCode() == juce::KeyPress::F10Key);
        return true;
    }
    if (mods.isAltDown() && ! mods.isCommandDown() && letter == 'X')
    {
        arrange.splitAtCursor();
        return true;
    }
    // Zoom and Auto-Scroll (the manual's keys: H in, G out, Shift+F the whole song, F Auto-Scroll).
    if (! mods.isAltDown() && (letter == 'H' || letter == 'G'))
    {
        mods.isShiftDown() ? arrange.zoomRows (letter == 'H') : arrange.zoom (letter == 'H');   // Shift: taller or shorter rows
        return true;
    }
    if (! mods.isAltDown() && letter == 'F')
    {
        if (mods.isShiftDown())
            arrange.zoomFull();
        else
            trackList.onAutoScrollToggle();   // (a suspended one carries on)
        return true;
    }
    if (letter == 'R' || key.getTextCharacter() == '*') { studio.record(); return true; }
    if (key.getTextCharacter() == '/')                  { studio.setCycleOn (! studio.getProject().loopOn); return true; }
    if (letter == 'C')                                  { studio.setClick (! studio.getProject().click); return true; }
    return false;
}

void MainView::mouseDown (const juce::MouseEvent& event)
{
    keysToEditor = event.eventComponent == &keyEditor || keyEditor.isParentOf (event.eventComponent);
}

//==============================================================================
void MainView::openZone (Zone& zone)
{
    zone.open = true;
    if (&zone == &editorZone)
        keysToEditor = true;
    placeZone (zone);
}

void MainView::closeZone (Zone& zone)
{
    zone.open = false;
    if (&zone == &editorZone)
        keysToEditor = false;
    placeZone (zone);
}

void MainView::setZoneInWindow (Zone& zone, bool inWindow)
{
    zone.inWindow = inWindow;
    if (&zone == &editorZone)
        keyEditor.setInWindow (inWindow);
    else
        mixer.setInWindow (inWindow);
    if (auto* settings = studio.getSettings())
        settings->setValue (zone.settingsKey + "InWindow", inWindow);
    placeZone (zone);
    if (&zone == &editorZone)
        keyEditor.fitToPart();   // its size changed a lot: the whole part again
}

void MainView::placeZone (Zone& zone)
{
    zone.button.setToggleState (zone.open, juce::dontSendNotification);
    if (zone.open && zone.inWindow)
    {
        if (zone.window == nullptr)
        {
            zone.window = std::make_unique<Windows::ZoneWindow> (zone.title, zone.content, studio.getSettings(),
                                                                 zone.settingsKey + "Window", getScreenBounds());
            const auto isEditor = &zone == &editorZone;
            zone.window->onKey = [this, isEditor] (const juce::KeyPress& key)
            {
                keysToEditor = isEditor;
                return keyPressed (key);
            };
            zone.window->onClose = [safe = juce::Component::SafePointer<MainView> (this), isEditor]
            {
                juce::MessageManager::callAsync ([safe, isEditor]
                {
                    if (safe != nullptr)
                        isEditor ? safe->closeKeyEditor() : safe->closeMixer();
                });
            };
            refreshHeader();
        }
        zone.window->toFront (true);
    }
    else
    {
        // Back here (hidden if closed). The window gives it up as it goes.
        zone.window.reset();
        if (zone.content.getParentComponent() != this)
            addChildComponent (zone.content);
        zone.content.setVisible (zone.open);
        // One docked at a time: the other one, if it's docked, is put away.
        auto& other = &zone == &editorZone ? mixerZone : editorZone;
        if (zone.open && other.open && ! other.inWindow)
        {
            other.open = false;
            if (&other == &editorZone)
                keysToEditor = false;
            other.button.setToggleState (false, juce::dontSendNotification);
            other.content.setVisible (false);
        }
    }
    resized();
    repaint();
}

void MainView::toggleMixerWindow()
{
    if (isMixerOpen() && isMixerInWindow())
    {
        closeMixer();
        return;
    }
    if (! isMixerInWindow())
        setMixerInWindow (true);   // docked and open: it moves into its window
    openMixer();
}

void MainView::toggleMixerDocked()
{
    if (isMixerOpen() && ! isMixerInWindow())
    {
        closeMixer();
        return;
    }
    if (isMixerInWindow())
        setMixerInWindow (false);   // in its window: it comes back under the arrangement
    openMixer();
}

void MainView::openEffectWindow (int busId, int slot)
{
    auto* instance = studio.getEngine().getEffect (busId, slot);
    if (instance == nullptr || ! instance->hasEditor())
        return;
    if (auto existing = effectWindows.find (instance); existing != effectWindows.end())
    {
        existing->second->toFront (true);
        return;
    }
    const auto* track = studio.getProject().findTrack (busId);
    auto window = std::make_unique<Windows::Instrument> (track != nullptr ? track->name : juce::String ("Master"), *instance);
    window->onClose = [safe = juce::Component::SafePointer<MainView> (this), instance]
    {
        juce::MessageManager::callAsync ([safe, instance] { if (safe != nullptr) safe->effectWindows.erase (instance); });
    };
    effectWindows[instance] = std::move (window);
}

//==============================================================================
void MainView::newProject()
{
    if (studio.isModified())
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "New song",
                                            "Start a new song? Changes to this one that aren't saved will be lost.",
                                            "New song", "Cancel", nullptr,
                                            juce::ModalCallbackFunction::create ([this] (int result)
                                            {
                                                if (result == 1)
                                                    studio.newProject();
                                            }));
        return;
    }
    studio.newProject();
}

void MainView::openProject()
{
    chooser = std::make_unique<juce::FileChooser> ("Open a song", studio.getFile().getParentDirectory(),
                                                   juce::String ("*") + pr7s::Project::fileExtension);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file == juce::File())
            return;
        juce::String problems;
        if (! studio.open (file, problems))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't open the song",
                                                    file.getFullPathName() + " isn't a Paragon PR7-S song (or is from a newer version).");
        else if (problems.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Some instruments, effects or recordings are missing",
                                                    "These aren't on this computer. Tracks without their instrument and clips without their "
                                                    "recording are silent, and the sound goes past missing effects, until they're found:\n\n" + problems);
    });
}

void MainView::saveProject (std::function<void (bool)> then)
{
    if (studio.getFile() == juce::File())
    {
        saveProjectAs (std::move (then));
        return;
    }
    const auto saved = studio.save (studio.getFile());
    if (! saved)
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't save", studio.getFile().getFullPathName());
    if (then)
        then (saved);
}

void MainView::saveProjectAs (std::function<void (bool)> then)
{
    const auto start = studio.getFile() != juce::File() ? studio.getFile()
                     : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (juce::String ("Song") + pr7s::Project::fileExtension);
    chooser = std::make_unique<juce::FileChooser> ("Save the song", start, juce::String ("*") + pr7s::Project::fileExtension);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, then] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file == juce::File())
                              {
                                  if (then)
                                      then (false);
                                  return;
                              }
                              file = file.withFileExtension (pr7s::Project::fileExtension);
                              const auto saved = studio.save (file);
                              if (! saved)
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't save", file.getFullPathName());
                              if (then)
                                  then (saved);
                          });
}

//==============================================================================
void MainView::importMidiFile()
{
    const auto chooseFile = [this] (bool newSong)
    {
        chooser = std::make_unique<juce::FileChooser> ("Import a MIDI file", studio.getFile().getParentDirectory(), "*.mid;*.midi");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, newSong] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();
                                  if (file == juce::File())
                                      return;
                                  juce::String error;
                                  if (studio.importMidi (file, newSong, 0.0, error) == 0)
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't import the MIDI file",
                                                                              file.getFileName() + ": " + error);
                              });
    };
    // As the manual has it: into a new song, or this one? (Only asked if this one has anything in it.)
    const auto& tracks = studio.getProject().tracks;
    const auto empty = std::all_of (tracks.begin(), tracks.end(), [] (const pr7s::Track& t) { return t.parts.empty(); });
    if (empty)
    {
        chooseFile (true);
        return;
    }
    juce::AlertWindow::showYesNoCancelBox (juce::MessageBoxIconType::QuestionIcon, "Import a MIDI file",
                                           juce::String ("Into a new song (with the file's tempo and time signature), or into this one (at bar 1, keeping its tempo)?")
                                               + (studio.isModified() ? "\n\nA new song: changes to this one that aren't saved will be lost." : ""),
                                           "New song", "This song", "Cancel", nullptr,
                                           juce::ModalCallbackFunction::create ([chooseFile] (int result)
                                           {
                                               if (result == 1)
                                                   chooseFile (true);
                                               else if (result == 2)
                                                   chooseFile (false);
                                           }));
}

bool MainView::isInterestedInFileDrag (const juce::StringArray& files)
{
    return std::any_of (files.begin(), files.end(), [] (const juce::String& f) { return f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi"); });
}

void MainView::filesDropped (const juce::StringArray& files, int x, int y)
{
    // Into this song, at the bar it's dropped on (over the arrangement), else bar 1.
    auto atBeat = 0.0;
    const auto inArrange = arrange.getLocalPoint (this, juce::Point<int> (x, y));
    if (arrange.getLocalBounds().contains (inArrange))
    {
        const auto bar = studio.getProject().beatsPerBar();
        atBeat = juce::jmax (0.0, std::floor (timeline.beatForX ((float) inArrange.x) / bar) * bar);
    }
    for (const auto& path : files)
    {
        if (! (path.endsWithIgnoreCase (".mid") || path.endsWithIgnoreCase (".midi")))
            continue;
        juce::String error;
        if (studio.importMidi (juce::File (path), false, atBeat, error) == 0)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't import the MIDI file",
                                                    juce::File (path).getFileName() + ": " + error);
    }
}

//==============================================================================
namespace
{
    // The export, written on a thread of its own behind a progress bar.
    class ExportProgress final : public juce::ThreadWithProgressWindow
    {
    public:
        ExportProgress (pr7s::Studio& s, std::function<void (bool written, bool cancelled)> done)
            : ThreadWithProgressWindow ("Exporting the mix...", true, true, 10000, "CANCEL"), studio (s), onDone (std::move (done)) {}

        void run() override
        {
            written = studio.renderExport ([this] (double done)
            {
                setProgress (done);
                return ! threadShouldExit();
            });
        }

        void threadComplete (bool userPressedCancel) override
        {
            studio.endExport();   // here, on the message thread
            if (onDone)
                onDone (written, userPressedCancel);
        }

    private:
        pr7s::Studio& studio;
        std::function<void (bool, bool)> onDone;
        bool written = false;
    };
}

void MainView::exportMix()
{
    if (studio.isExporting() || studio.getEngine().isRecording() || exportPage != nullptr)
        return;
    auto* page = new Windows::ExportPage (studio);
    exportPage = page;
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (page);
    options.dialogTitle = "Paragon PR7-S: Export the mix";
    options.dialogBackgroundColour = juce::Colour (0xff0c0a0d);
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.componentToCentreAround = this;
    options.launchAsync();

    auto close = [page]
    {
        if (auto* dialog = page->findParentComponentOfClass<juce::DialogWindow>())
            dialog->exitModalState (0);   // deleted once its click is over
    };
    page->onCancel = close;
    page->onExport = [this, close] (const pr7s::Studio::ExportOptions& chosen)
    {
        close();
        // Where: beside the song, named after it.
        const auto song = studio.getFile();
        const auto start = song != juce::File() ? song.withFileExtension (".wav")
                         : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mix.wav");
        chooser = std::make_unique<juce::FileChooser> ("Export the mix", start, "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, chosen] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();
                                  if (file != juce::File())
                                      writeExport (file.withFileExtension (".wav"), chosen);
                              });
    };
}

void MainView::writeExport (const juce::File& file, const pr7s::Studio::ExportOptions& options)
{
    juce::String error;
    if (! studio.beginExport (file, options, error))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't export", error);
        return;
    }
    exportProgress = std::make_unique<ExportProgress> (studio, [safe = juce::Component::SafePointer<MainView> (this), file] (bool written, bool cancelled)
    {
        if (safe == nullptr)
            return;
        if (! written && ! cancelled)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't export",
                                                    "Couldn't write " + file.getFullPathName() + " (is the disk full?)");
        // Not inside its own callback.
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->exportProgress.reset(); });
    });
    exportProgress->launchThread();
}

//==============================================================================
void MainView::openInstrumentWindow (int trackId)
{
    if (auto existing = instrumentWindows.find (trackId); existing != instrumentWindows.end())
    {
        existing->second->toFront (true);
        return;
    }
    auto* instance = studio.getEngine().getInstrument (trackId);
    const auto* track = studio.getProject().findTrack (trackId);
    if (instance == nullptr || track == nullptr || ! instance->hasEditor())
        return;
    auto window = std::make_unique<Windows::Instrument> (track->name, *instance);
    // Closed after its own click has finished (a window can't delete itself mid-click).
    window->onClose = [safe = juce::Component::SafePointer<MainView> (this), trackId]
    {
        juce::MessageManager::callAsync ([safe, trackId] { if (safe != nullptr) safe->instrumentWindows.erase (trackId); });
    };
    instrumentWindows[trackId] = std::move (window);
}

void MainView::openSettings()
{
    if (settingsWindow != nullptr)
    {
        settingsWindow->toFront (true);
        return;
    }
    settingsWindow = std::make_unique<Windows::Settings> (studio);
    settingsWindow->onClose = [safe = juce::Component::SafePointer<MainView> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->settingsWindow.reset(); });
    };
}

void MainView::openPlugins()
{
    if (pluginsWindow != nullptr)
    {
        pluginsWindow->toFront (true);
        return;
    }
    pluginsWindow = std::make_unique<Windows::Plugins> (studio);
    pluginsWindow->onClose = [safe = juce::Component::SafePointer<MainView> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->pluginsWindow.reset(); });
    };
}
