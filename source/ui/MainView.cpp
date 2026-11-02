#include "MainView.h"
#include "engine/MidiImport.h"
#include "ui/KeyCommandsPage.h"
#include "ui/Hub.h"
#include "ui/TempoEditor.h"

#include <thread>

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
      mixer (studioToUse),
      chordPads (studioToUse),
      inspector (studioToUse)
{
    askMidiTempo = [this] (const juce::File& file, double fileBpm, std::function<void (int)> answer)
    {
        const auto bpm = [] (double v) { return juce::String (v, std::abs (v - std::round (v)) < 0.005 ? 0 : 2) + " BPM"; };
        const auto songBpm = studio.getProject().bpm;
        juce::AlertWindow::showYesNoCancelBox (juce::MessageBoxIconType::QuestionIcon, "The MIDI file's tempo",
            file.getFileName() + " was written at " + bpm (fileBpm) + "; this song is at " + bpm (songBpm) + ".\n\n"
            "(If the file's tempo changes as it goes, matching puts its changes on the tempo track from where it goes in.)\n\n"
            "Match the song to the file: the song changes to " + bpm (fileBpm) + ", so the notes play at the speed they were written at "
            "(audio already in the song doesn't stretch with it). Keep the song's tempo: the notes go on the same beats, played at "
            + bpm (songBpm) + ".",
            "Match to " + bpm (fileBpm), "Keep " + bpm (songBpm), "Cancel", nullptr,
            juce::ModalCallbackFunction::create ([answer] (int result) { answer (result); }));
    };
    for (auto* button : { &newButton, &openButton, &saveButton, &saveAsButton, &importButton, &exportButton, &undoButton, &redoButton, &editorButton, &mixerButton,
                          &inspectorButton, &pluginsButton, &settingsButton })
    {
        button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        button->setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        button->setWantsKeyboardFocus (false);
        // (0.9.10: the editor's, mixer's and Inspector's keys are kept for their lit state but not shown: F1, F2, F3.)
        if (button == &editorButton || button == &mixerButton || button == &inspectorButton)
            addChildComponent (*button);
        else
            addAndMakeVisible (*button);
    }
    newButton.setTooltip ("A new song (Ctrl+N)");
    openButton.setTooltip ("Open a song (Ctrl+O)");
    saveButton.setTooltip ("Save the song (Ctrl+S)");
    saveAsButton.setTooltip ("Save the song under a new name (Ctrl+Shift+S)");
    exportButton.setTooltip ("Export the mix to a WAV file: the whole song or the cycle");
    importButton.setTooltip ("Import a MIDI file (a track for each of its tracks), audio files (WAV, AIFF, FLAC, Ogg, MP3: clips at the playhead), "
                             "or a DAWproject (a song from Cubase or another studio). Or drop them on the arrangement, at a bar");
    importButton.onClick = [this]
    {
        // Every choice here (0.9.7): no question box afterwards. (A file chooser opened from
        // inside a question box's answer closed with it on Windows: "This song" did nothing.)
        juce::PopupMenu menu;
        menu.addItem (1, "MIDI files into this song...");
        menu.addItem (2, "MIDI files as a new song...");
        menu.addSeparator();
        menu.addItem (3, "Audio files, a track each... (WAV, AIFF, FLAC, Ogg, MP3)");
        menu.addItem (4, "Audio files, one after another on one track...");
        menu.addSeparator();
        menu.addItem (5, "DAWproject as a new song... (from Cubase: File > Export > DAWproject)");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&importButton), [safe = juce::Component::SafePointer<MainView> (this)] (int chosen)
        {
            if (safe == nullptr || chosen == 0)
                return;
            // After the menu has gone.
            juce::MessageManager::callAsync ([safe, chosen]
            {
                if (safe == nullptr)
                    return;
                if (chosen == 1 || chosen == 2)
                    safe->importMidiFile (chosen == 2);
                else if (chosen == 5)
                    safe->importDawProjectFile();
                else
                    safe->importAudioFiles (chosen == 4);
            });
        });
    };
    undoButton.setTooltip ("Undo (Ctrl+Z)");
    redoButton.setTooltip ("Redo (Ctrl+Shift+Z)");
    editorButton.setTooltip ("The MIDI editor: the selected part's notes (F2 or Ctrl+E; or double-click a part)");
    editorButton.setColour (juce::TextButton::textColourOnId, ParagonColours::accent);
    mixerButton.setTooltip ("The mixer (F3; Alt+F3: docked or full screen)");
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
    inspectorButton.setTooltip ("The Inspector: the selected track's settings, left of the track list (Ctrl+Alt+L)");
    inspectorButton.setColour (juce::TextButton::textColourOnId, ParagonColours::accent);
    inspectorButton.onClick = [this] { toggleInspector(); };
    if (auto* settings = studio.getSettings())
        inspectorShown = settings->getBoolValue ("inspectorShown", true);
    inspectorButton.setToggleState (inspectorShown, juce::dontSendNotification);
    addAndMakeVisible (inspector);
    inspector.setVisible (inspectorShown);
    if (auto* settings = studio.getSettings())
        inspectorWidth = juce::jlimit (minimumInspectorWidth, maximumInspectorWidth, settings->getIntValue ("inspectorWidth", Inspector::width));
    inspectorEdge = std::make_unique<InspectorEdge> (*this);
    addAndMakeVisible (*inspectorEdge);
    inspector.onTrackMenu = [this] (int trackId) { trackList.showInstrumentMenu (trackId); };
    inspector.onOpenInstrument = [this] (int trackId) { openInstrumentWindow (trackId); };
    inspector.onOpenEffect = [this] (int busId, int slot) { openEffectWindow (busId, slot); };
    inspector.onFindPlugins = [this] { openPlugins(); };
    pluginsButton.onClick = [this] { openPlugins(); };
    keysButton.onClick = [this] { openKeyCommands(); };
    keysButton.setTooltip ("Key Commands: give any action or macro a key (a Stream Deck's Hotkey buttons press them)");
    hubButton.onClick = [this] { openHub(); };
    padsButton.onClick = [this] { toggleChordPads(); };
    padsButton.setTooltip ("The chord pads: chords to play with, their voicings and tensions; played by clicking or from your MIDI keyboard's C1 to B1");
    padsButton.setColour (juce::TextButton::textColourOnId, ParagonColours::accent);
    hubButton.setTooltip ("The Hub: your songs (recent, templates, the projects folder), a new one, the audio driver");
    addChildComponent (padsButton);   // (kept for its lit state, as the editor's and mixer's: F4 opens the pads)
    for (auto* key : { &keysButton, &hubButton })
    {
        key->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        key->setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        key->setWantsKeyboardFocus (false);
        addAndMakeVisible (*key);
    }
    registerCommands();
    // A long job's progress (Render in Place, Freeze): the box over the studio while it lasts.
    // Holding Alt changes a linked channel on its own; Shift+Alt is Q-Link for the moment (0.17.0, the manual's).
    studio.isSuspendHeld = [] { const auto m = juce::ModifierKeys::currentModifiers; return m.isAltDown() && ! m.isShiftDown(); };
    studio.isQLinkHeld = [] { const auto m = juce::ModifierKeys::currentModifiers; return m.isAltDown() && m.isShiftDown(); };
    studio.onProgress = [this] (double progress, const juce::String& what)
    {
        if (progress < 0.0)
        {
            progressBox = nullptr;
            return;
        }
        if (progressBox == nullptr)
            progressBox = std::make_unique<ProgressBox>();
        progressBox->set (progress, what, this);
    };
    // Renders (Render in Place, Freeze) on a thread of their own while this
    // one carries on (0.12.1: an instrument rendering offline can wait for
    // its loading threads, which can wait for this thread; rendered here, the
    // studio never came back). The progress box takes the clicks and keys
    // meanwhile, so nothing changes the song mid-render. (Only in the app:
    // the tests render where they are.)
    if (juce::JUCEApplicationBase::getInstance() != nullptr)
        studio.renderRunner = [this] (std::function<bool()> work)
        {
            std::atomic<bool> done { false };
            auto result = false;
            std::thread worker ([&] { result = work(); done = true; });
            if (progressBox != nullptr && ! progressBox->isCurrentlyModal())
                progressBox->enterModalState (false);
            if (progressBox != nullptr)
                progressBox->setOnCancel ([this] { studio.cancelRender(); });
            while (! done.load())
            {
                juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
                studio.reportRenderProgress();
            }
            worker.join();
            return result;
        };
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
    // Snap on/off and the quantize value (0.9.10: the manual's Snap and Quantize Presets on the toolbar).
    for (auto* key : { &snapButton, &quantizeButton })
    {
        key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key->setWantsKeyboardFocus (false);
        addAndMakeVisible (*key);
    }
    quantizeButton.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    snapButton.setTooltip ("Snap (J): parts, ranges, notes' positions and the playhead snap to the quantize value (lit: on)");
    quantizeButton.setTooltip ("The quantize value: what Snap snaps to (1/1 to 1/64)");
    snapButton.onClick = [this] { studio.setSnap (! studio.getProject().snapOn); };
    quantizeButton.onClick = [this] { showQuantizeMenu(); };
    readAllButton.onClick = [this] { studio.setAutomationReadAll (! studio.anyAutomationRead()); };
    writeAllButton.onClick = [this] { studio.setAutomationWriteAll (! studio.anyAutomationWrite()); };

    addAndMakeVisible (transport);
    addAndMakeVisible (trackList);
    addAndMakeVisible (arrange);
    addAndMakeVisible (tools);
    arrange.onToolChange = [this] { tools.refresh(); };
    addChildComponent (keyEditorHolder);
    addChildComponent (mixer);
    keyEditor.onSizeChange = [this] (int percent) { setKeyEditorSize (percent); };
    {
        // (Only the frame here: the window isn't laid out yet.)
        const auto size = studio.getSettings() != nullptr ? studio.getSettings()->getIntValue ("keyEditorSize", 200) : 200;
        const auto percent = size <= 100 ? 100 : size <= 150 ? 150 : 200;
        keyEditorHolder.setScale ((float) percent / 100.0f);
        keyEditor.setSizeShown (percent);
    }
    keyEditor.onClose = [this] { closeKeyEditor(); };
    mixer.onClose = [this] { closeMixer(); };
    chordPads.onClose = [this] { closeZone (padsZone); };
    chordPads.onWindowToggle = [safe = juce::Component::SafePointer<MainView> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->setZoneInWindow (safe->padsZone, ! safe->padsZone.inWindow); });
    };
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
        padsZone.inWindow = settings->getBoolValue ("chordPadsInWindow", false);
        padsZone.dockedHeight = settings->getIntValue ("chordPadsHeight", 0);
    }
    // Each view's Auto-Scroll and its settings, kept for next time.
    timeline.autoScroll.load (studio.getSettings(), "arrange");
    keyEditor.getAutoScrollSettings().load (studio.getSettings(), "keyEditor");
    keyEditor.autoScrollChanged();
    keyEditor.onAutoScrollChange = [this] { keyEditor.getAutoScrollSettings().save (studio.getSettings(), "keyEditor"); };
    timeline.onAutoScrollChange = [this]
    {
        timeline.autoScroll.refreshKey (autoScrollKey);
        timeline.autoScroll.save (studio.getSettings(), "arrange");
    };
    // AUTO-SCROLL and its settings arrow: on the transport, by HOME (the owner's choice).
    autoScrollKey.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    autoScrollKey.setWantsKeyboardFocus (false);
    autoScrollKey.onClick = [this] { toggleArrangeAutoScroll(); };
    autoScrollMenu.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    autoScrollMenu.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    autoScrollMenu.setWantsKeyboardFocus (false);
    autoScrollMenu.setTooltip ("Auto-Scroll's settings: Page Scroll or Stationary Cursor; Suspend Auto-Scroll when Editing");
    autoScrollMenu.onClick = [this] { timeline.autoScroll.showMenu (autoScrollMenu, [this] { timeline.onAutoScrollChange(); }); };
    addAndMakeVisible (autoScrollKey);
    addAndMakeVisible (autoScrollMenu);
    timeline.autoScroll.refreshKey (autoScrollKey);
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
    chordPads.setInWindow (padsZone.inWindow);
    arrange.onEditNotes = [this] { openKeyEditor(); };
    arrange.onPartsCopied = [this] { copiedLast = Copied::parts; rangeCopiedLast = false; };
    arrange.onRenderInPlace = [this] (std::vector<pr7s::Studio::RenderSource> sources, bool ask) { renderInPlace (std::move (sources), ask); };
    trackList.onRenderInPlace = [this] (std::vector<pr7s::Studio::RenderSource> sources, bool ask) { renderInPlace (std::move (sources), ask); };
    // Told of clicks in the work areas, to know where the keys go.
    arrange.addMouseListener (this, true);
    trackList.addMouseListener (this, true);
    keyEditor.addMouseListener (this, true);

    trackList.onOpenInstrument = [this] (int trackId) { openInstrumentWindow (trackId); };
    trackList.onFindPlugins = [this] { openPlugins(); };
    trackList.onTempoMenu = [this] { arrange.showTempoMenu(); };
    arrange.onOpenTempoEditor = [this] { if (tempoEditorWindow == nullptr) toggleTempoEditor(); else tempoEditorWindow->toFront (true); };
    transport.onOpenTempoEditor = arrange.onOpenTempoEditor;
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
    studio.renderRunner = nullptr;
    studio.onProgress = nullptr;
    studio.isSuspendHeld = nullptr;
    studio.isQLinkHeld = nullptr;
    arrangeAcross.removeListener (this);
    arrangeDown.removeListener (this);
    mixer.onMinimumHeightChange = nullptr;
    timeline.onAutoScrollChange = nullptr;
    keyEditor.onAutoScrollChange = nullptr;
    editorZone.window.reset();
    mixerZone.window.reset();
    padsZone.window.reset();
    studio.setPadsActive (false);
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

void MainView::toggleArrangeAutoScroll()
{
    timeline.autoScroll.toggle();
    timeline.onAutoScrollChange();
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
        editorZone.window->setName ("MIDI Editor: " + (part != nullptr && part->name.isNotEmpty() ? part->name : juce::String ("no part")));
    }
    undoButton.setEnabled (studio.canUndo());
    redoButton.setEnabled (studio.canRedo());
    readAllButton.setToggleState (studio.anyAutomationRead(), juce::dontSendNotification);
    writeAllButton.setToggleState (studio.anyAutomationWrite(), juce::dontSendNotification);
    snapButton.setToggleState (studio.getProject().snapOn, juce::dontSendNotification);
    quantizeButton.setButtonText (quantizeName (studio.getProject().quantizeBeats));
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
    const auto middle = getLocalBounds().withTrimmedTop (headerHeight + transportHeight + (isWide() ? 0 : toolbarHeight)).toFloat().reduced (6.0f, 2.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRect (middle, 1.0f);
}

void MainView::resized()
{
    // Turning wide (spanning screens, at start-up or by SPAN ALL SCREENS): the
    // mixer docked under the arrangement, its racks hidden, as short as it
    // goes (the owner's choice). Only as it turns wide: what's changed after stays.
    if (isWide() && ! wasWide)
    {
        wasWide = true;
        mixer.setRackShown (false);
        mixerZone.dockedHeight = 1;   // (kept to its least)
        if (mixerZone.inWindow)
            setMixerInWindow (false);
        if (! isMixerOpen())
            openMixer();   // (lays out again)
        return resized();
    }
    wasWide = isWide();

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
    // (0.9.10: no INSPECTOR, KEY EDITOR or MIXER keys, the owner asked: F1, F2, F3.)
    readAllButton.setBounds (header.removeFromLeft (34));
    header.removeFromLeft (4);
    writeAllButton.setBounds (header.removeFromLeft (34));
    header.removeFromLeft (18);
    snapButton.setBounds (header.removeFromLeft (56));
    header.removeFromLeft (4);
    quantizeButton.setBounds (header.removeFromLeft (66));
    settingsButton.setBounds (header.removeFromRight (92));
    header.removeFromRight (6);
    pluginsButton.setBounds (header.removeFromRight (86));
    header.removeFromRight (6);
    keysButton.setBounds (header.removeFromRight (66));
    header.removeFromRight (6);
    hubButton.setBounds (header.removeFromRight (60));
    // Wide: the header's keys (NEW to W) in the middle of the window.
    if (isWide())
    {
        const auto first = newButton.getX(), last = quantizeButton.getRight();
        const auto shift = juce::jmax (0, (getWidth() - (last - first)) / 2 - first);
        for (auto* key : { &newButton, &openButton, &saveButton, &saveAsButton, &importButton, &exportButton, &undoButton, &redoButton,
                           &readAllButton, &writeAllButton, &snapButton, &quantizeButton })
            key->setTopLeftPosition (key->getX() + shift, key->getY());
    }

    transport.setToolsWidth (isWide() ? ArrangeTools::unitWidth() : 0);
    transport.setBounds (area.removeFromTop (transportHeight).reduced (2, 0));
    tools.setOnUnit (isWide());
    {
        auto autoArea = transport.getAutoScrollArea() + transport.getPosition();
        autoScrollMenu.setBounds (autoArea.removeFromRight (20));
        autoArea.removeFromRight (3);
        autoScrollKey.setBounds (autoArea);
    }
    if (isWide())
        tools.setBounds (transport.getToolsArea() + transport.getPosition());   // on the transport, right of its screens
    else
        tools.setBounds (area.removeFromTop (toolbarHeight).reduced (7, 3).withTrimmedTop (2));

    auto middle = area.reduced (7, 3);
    // The Inspector: the whole height on the left, beside the lower zone too (as the manual's left zone).
    inspector.setVisible (inspectorShown);
    inspectorEdge->setVisible (inspectorShown);
    if (inspectorShown)
    {
        inspector.setBounds (middle.removeFromLeft (inspectorWidth));
        inspectorEdge->setBounds (middle.removeFromLeft (5));
    }
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
    for (auto* zone : zones())
        if (zone->open && ! zone->inWindow)
            return zone;
    return nullptr;
}

int MainView::minimumHeight (Zone& zone) const
{
    if (&zone == &padsZone)
        return chordPads.getMinimumHeight();
    return &zone == &mixerZone ? mixer.getMinimumHeight() : juce::roundToInt ((float) KeyEditor::minimumHeight * keyEditorHolder.getScale());
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
    if (zone.dockedHeight <= 0 && &zone == &padsZone)
        wanted = 320;   // (the pads at a comfortable size, the arrangement kept large: our choice)
    return juce::jlimit (least, most, wanted);
}

void MainView::setDockedHeight (int height)
{
    auto* zone = dockedZone();
    if (zone == nullptr)
        return;
    const auto middleHeight = getHeight() - headerHeight - transportHeight - (isWide() ? 0 : toolbarHeight) - 6;
    zone->dockedHeight = juce::jmax (1, height);   // (0 would mean the default)
    zone->dockedHeight = heightFor (*zone, middleHeight);   // kept to what it can be
    if (auto* settings = studio.getSettings())
        settings->setValue (zone->settingsKey + "Height", zone->dockedHeight);
    resized();
}

//==============================================================================
MainView::InspectorEdge::InspectorEdge (MainView& ownerToUse) : owner (ownerToUse)
{
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    setRepaintsOnMouseActivity (true);
    setTooltip ("Drag to make the Inspector wider or narrower");
}

void MainView::InspectorEdge::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto lit = isMouseOverOrDragging();
    g.setColour (lit ? ParagonColours::accent : ParagonColours::panelEdge);
    g.fillRect (bounds.withSizeKeepingCentre (lit ? 2.0f : 1.0f, bounds.getHeight()));
}

void MainView::InspectorEdge::mouseDown (const juce::MouseEvent&)
{
    widthAtDown = owner.getInspectorWidth();
}

void MainView::InspectorEdge::mouseDrag (const juce::MouseEvent& event)
{
    owner.setInspectorWidth (widthAtDown + event.getDistanceFromDragStartX());
}

void MainView::setInspectorWidth (int newWidth)
{
    inspectorWidth = juce::jlimit (minimumInspectorWidth, maximumInspectorWidth, newWidth);
    if (auto* settings = studio.getSettings())
        settings->setValue ("inspectorWidth", inspectorWidth);
    resized();
}

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
    // While a mouse button is held (a part, note, fader or point being dragged),
    // only Space: an edit by key mid-drag pulled the song from under the drag
    // (a part deleted or undone while its edge moved; a note's change undone by
    // the drag's release; a fader's drag going on to the next track).
    if (juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown() && key != juce::KeyPress::spaceKey)
        return true;
    // The owner's key commands and macros first (KEYS), then the built-in keys.
    auto swallowed = false;
    if (keyMap.handle (key, swallowed))
        return true;
    if (swallowed && ! (isKeyEditorOpen() && keysToEditor))
        return true;   // a default key the owner took away from its command (the MIDI editor's own keys stay its own)
    return handleKey (key);
}

bool MainView::handleKey (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const auto code = key.getKeyCode();
    // A letter only if the key is one (on Windows an F-key's or the number
    // pad's code, upper-cased, would come out as a letter: F3 as R).
    const auto letter = (code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z')
                          ? juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) code) : (juce::juce_wchar) 0;

    // Ctrl+Alt+L: the Inspector (the manual's Show/Hide Left Zone).
    if (mods.isCommandDown() && mods.isAltDown() && letter == 'L')
    {
        toggleInspector();
        return true;
    }
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
    // Range Selection's keys (the manual's), unless the last click was in the key editor.
    if (! (isKeyEditorOpen() && keysToEditor))
    {
        using C = ArrangeView::RangeCommand;
        const auto cmd = mods.isCommandDown(), shift = mods.isShiftDown();
        const auto code = key.getKeyCode();
        std::optional<C> command;
        if (arrange.hasRange())
        {
            if (code == juce::KeyPress::escapeKey && ! mods.isAnyModifierKeyDown())
            {
                arrange.setRange (std::nullopt);
                return true;
            }
            if (! cmd && shift && ! mods.isAltDown() && letter == 'X')                    command = C::split;
            else if (cmd && letter == 'X')                                                command = shift ? C::cutTime : C::cut;
            else if (cmd && ! shift && letter == 'C')                                     command = C::copy;
            else if (cmd && letter == 'D')                                                command = C::duplicate;
            else if (cmd && shift && letter == 'E')                                       command = C::insertSilence;
            else if (! cmd && (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey))
                command = shift ? C::deleteTime : C::deleteContents;
        }
        if (cmd && letter == 'V' && rangeCopiedLast && arrange.hasRangeClip())
            command = shift ? C::pasteTime : C::paste;
        if (command)
        {
            if (*command == C::cut || *command == C::cutTime || *command == C::copy)
            {
                rangeCopiedLast = true;
                copiedLast = Copied::range;
            }
            arrange.rangeCommand (*command);
            return true;
        }
    }
    // Parts (0.9.2): Ctrl+C / X with parts selected (unless the last click was
    // in the key editor) copies or cuts them; Ctrl+V pastes what was copied
    // last (parts at the playhead on the selected track), Alt+V at its origin.
    if (! (isKeyEditorOpen() && keysToEditor))
    {
        const auto cmd = mods.isCommandDown();
        if (cmd && ! mods.isShiftDown() && (letter == 'C' || letter == 'X') && ! arrange.getSelectedParts().empty())
        {
            arrange.copySelected (letter == 'X');
            copiedLast = Copied::parts;
            rangeCopiedLast = false;
            return true;
        }
        if (cmd && ! mods.isShiftDown() && letter == 'V' && copiedLast == Copied::parts && arrange.hasPartsClip())
        {
            arrange.pasteParts (false);
            return true;
        }
        if (! cmd && mods.isAltDown() && letter == 'V' && arrange.hasPartsClip())
        {
            arrange.pasteParts (true);   // Paste at Origin
            return true;
        }
    }
    // Otherwise copy, cut, paste and select all are the editor's wherever the
    // last click was, so notes copied in one part paste into another just
    // selected in the arrangement.
    const auto editorsAnyway = (mods.isCommandDown() && (letter == 'C' || letter == 'X' || letter == 'V'))
                            || (! mods.isCommandDown() && mods.isShiftDown() && letter == 'A')
                            || (! mods.isCommandDown() && mods.isAltDown() && (letter == 'B' || letter == 'N' || letter == 'M'));   // random velocities
    if (isKeyEditorOpen() && editorsAnyway && keyEditor.keyPressed (key))
    {
        if (letter == 'C' || letter == 'X')
        {
            rangeCopiedLast = false;   // notes copied: Ctrl+V pastes them
            copiedLast = Copied::notes;
        }
        return true;
    }
    // Ctrl+D duplicates the notes after a click in the editor, the part otherwise.
    if (isKeyEditorOpen() && keysToEditor && mods.isCommandDown() && letter == 'D' && keyEditor.keyPressed (key))
        return true;

    // The locators (the manual's keys): number pad 1 / 2 to the left / right locator (Ctrl: set it to the
    // playhead); P to the selection (below). Before the Ctrl keys and the tool keys (the pad types 1 and 2 too).
    if (code == juce::KeyPress::numberPad1 || code == juce::KeyPress::numberPad2)
    {
        const auto& project = studio.getProject();
        const auto left = code == juce::KeyPress::numberPad1;
        const auto at = studio.getEngine().getPosition();
        if (mods.isCommandDown())
            left ? studio.setCycle (juce::jmin (at, project.loopEnd), juce::jmax (at, project.loopEnd))
                 : studio.setCycle (juce::jmin (project.loopStart, at), juce::jmax (project.loopStart, at));
        else
            studio.setPosition (left ? project.loopStart : project.loopEnd);
        return true;
    }
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
            case 'A': arrange.selectAllParts(); return true;   // (in the key editor after a click there: its notes, above)
            case 'E': isKeyEditorOpen() ? closeKeyEditor() : openKeyEditor(); return true;
            case 'T': toggleTempoEditor(); return true;   // (the manual's Open Tempo Track Editor)
            default: return false;
        }
    }

    // F1 the Inspector, F2 the MIDI editor, F3 the mixer (0.9.10, the owner's
    // keys); Alt+F2 / Alt+F3 move the editor / mixer between docked and full screen.
    if (key.getKeyCode() == juce::KeyPress::F1Key)      { toggleInspector(); return true; }
    if (key.getKeyCode() == juce::KeyPress::F2Key)      { mods.isAltDown() ? toggleZonePlace (editorZone) : toggleZone (editorZone); return true; }
    if (key.getKeyCode() == juce::KeyPress::F3Key)      { mods.isAltDown() ? toggleZonePlace (mixerZone) : toggleZone (mixerZone); return true; }
    // F4: the chord pads (0.14.0, our choice: the manual gives them no key; never Alt+F4, which closes the window).
    if (key.getKeyCode() == juce::KeyPress::F4Key && ! mods.isAltDown()) { toggleZone (padsZone); return true; }
    if (isKeyEditorOpen() && key == juce::KeyPress::escapeKey) { closeKeyEditor(); return true; }
    if (isMixerOpen() && key == juce::KeyPress::escapeKey)     { closeMixer(); return true; }
    if (key == juce::KeyPress::spaceKey)                { studio.togglePlay(); return true; }
    if (key == juce::KeyPress::homeKey)                 { studio.goToStart(); return true; }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (arrange.hasSelectedTempoEvents())
            arrange.deleteSelectedTempoEvents();
        else if (arrange.hasSelectedChordEvents())
            arrange.deleteSelectedChordEvents();
        else
            arrange.hasSelectedPoints() ? arrange.deleteSelectedPoints() : arrange.deleteSelected();
        return true;
    }
    // Shift+Delete: the selected tracks (Remove Selected Tracks); Up / Down: the track above / below
    // (and, if a part was selected, the first part on it), as the manual has them.
    if (code == juce::KeyPress::deleteKey && mods.isShiftDown() && ! mods.isCommandDown())
    {
        trackList.removeSelectedTracks();
        return true;
    }
    if ((code == juce::KeyPress::upKey || code == juce::KeyPress::downKey) && ! mods.isAnyModifierKeyDown())
    {
        const auto shown = studio.getShownTracks();   // (the tracks shown: hidden ones are skipped)
        int at = -1;
        for (int i = 0; i < (int) shown.size(); ++i)
            if (shown[(size_t) i] == studio.getSelectedTrack())
                at = i;
        const auto next = juce::jlimit (0, (int) shown.size() - 1, at + (code == juce::KeyPress::upKey ? -1 : 1));
        if (shown.empty() || next == at)
            return true;
        const auto& track = *studio.getProject().findTrack (shown[(size_t) next]);
        const auto partWasSelected = arrange.getSelection().isValid();
        studio.selectTrack (track.id);
        if (partWasSelected)
        {
            int first = -1;
            for (int i = 0; i < (int) track.parts.size(); ++i)
                if (first < 0 || track.parts[(size_t) i].start < track.parts[(size_t) first].start)
                    first = i;
            first >= 0 ? arrange.select (track.id, first) : arrange.select (track.id, -1);
        }
        return true;
    }
    // P: the locators to the selection (Alt+P: and play it round).
    if (letter == 'P' && ! mods.isCommandDown() && ! mods.isShiftDown())
    {
        if (arrange.locatorsToSelection() && mods.isAltDown())
        {
            // Loop Selection: round and round it.
            studio.setCycleOn (true);
            studio.setPosition (studio.getProject().loopStart);
            if (! studio.getEngine().isPlaying())
                studio.play();
        }
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
            toggleArrangeAutoScroll();   // (a suspended one carries on)
        return true;
    }
    if (letter == 'R' || key.getTextCharacter() == '*') { studio.record(); return true; }
    if (key.getTextCharacter() == '/')                  { studio.setCycleOn (! studio.getProject().loopOn); return true; }
    if (letter == 'C')                                  { studio.setClick (! studio.getProject().click); return true; }
    if (letter == 'J')                                  { studio.setSnap (! studio.getProject().snapOn); return true; }   // the manual's Snap On/Off
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
    else if (&zone == &mixerZone)
        mixer.setInWindow (inWindow);
    else
        chordPads.setInWindow (inWindow);
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
            zone.window->onClose = [safe = juce::Component::SafePointer<MainView> (this), which = &zone]
            {
                juce::MessageManager::callAsync ([safe, which]
                {
                    if (safe != nullptr)
                        safe->closeZone (*which);
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
        // One docked at a time: another, if it's docked, is put away.
        for (auto* other : zones())
            if (other != &zone && zone.open && other->open && ! other->inWindow)
            {
                other->open = false;
                if (other == &editorZone)
                    keysToEditor = false;
                other->button.setToggleState (false, juce::dontSendNotification);
                other->content.setVisible (false);
            }
    }
    // The pads' trigger keys play them only while they're open (docked or in their window).
    studio.setPadsActive (padsZone.open);
    resized();
    repaint();
}

juce::String MainView::quantizeName (double beats)
{
    // In notes: 4 beats is 1/1, a beat 1/4, a quarter beat 1/16.
    const auto denominator = juce::roundToInt (4.0 / beats);
    return "1/" + juce::String (denominator);
}

void MainView::showQuantizeMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("QUANTIZE (WHAT SNAP SNAPS TO)");
    for (const auto denominator : { 1, 2, 4, 8, 16, 32, 64 })
    {
        const auto beats = 4.0 / denominator;
        menu.addItem (denominator, "1/" + juce::String (denominator), true, std::abs (studio.getProject().quantizeBeats - beats) < 1.0e-9);
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&quantizeButton),
                        [safe = juce::Component::SafePointer<MainView> (this)] (int chosen)
                        {
                            if (safe != nullptr && chosen > 0)
                                safe->studio.setQuantizeBeats (4.0 / chosen);
                        });
}

void MainView::toggleZone (Zone& zone)
{
    zone.open ? closeZone (zone) : openZone (zone);
}

void MainView::toggleZonePlace (Zone& zone)
{
    // Docked <-> its own window, full screen; open either way.
    const auto toWindow = ! zone.inWindow;
    if (! zone.open)
        zone.open = true;
    setZoneInWindow (zone, toWindow);
    if (&zone == &editorZone)
        keysToEditor = true;
    if (toWindow && zone.window != nullptr)
    {
        // Across all the screens (0.9.11: the owner's three), as SPAN ALL SCREENS does the studio.
        zone.window->setBounds (Windows::allScreensArea());
    }
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

void MainView::setKeyEditorSize (int percent)
{
    percent = percent <= 100 ? 100 : percent <= 150 ? 150 : 200;
    keyEditorHolder.setScale ((float) percent / 100.0f);
    keyEditor.setSizeShown (percent);
    if (auto* settings = studio.getSettings())
        settings->setValue ("keyEditorSize", percent);
    resized();
    keyEditor.fitToPart();
}

void MainView::toggleInspector()
{
    inspectorShown = ! inspectorShown;
    inspectorButton.setToggleState (inspectorShown, juce::dontSendNotification);
    if (auto* settings = studio.getSettings())
        settings->setValue ("inspectorShown", inspectorShown);
    resized();
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
    if (instance == nullptr || ! studio.hasEditor (instance))
        return;
    if (auto existing = effectWindows.find (instance); existing != effectWindows.end())
    {
        existing->second->toFront (true);
        return;
    }
    const auto* track = studio.getProject().findTrack (busId);
    auto window = std::make_unique<Windows::Instrument> (track != nullptr ? track->name : juce::String ("Master"), *instance);
    window->onKey = [this] (const juce::KeyPress& key) { return keyPressed (key); };   // (the studio's keys work there too)
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
    // Written on a thread of its own (0.9.11): the screen carries on.
    const auto where = studio.getFile();
    studio.saveInBackground (where, [then, where, safe = juce::Component::SafePointer<MainView> (this)] (bool saved)
    {
        if (saved && safe != nullptr)
            safe->flashSaved();
        if (! saved)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't save", where.getFullPathName());
        if (then)
            then (saved);
    });
}

void MainView::flashSaved()
{
    // SAVE lights up and says SAVED for a moment once the song is on the disk
    // (0.11.1, the owner asked: Ctrl+S had nothing to show for it).
    saveButton.getProperties().set ("lit", true);
    saveButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    saveButton.setButtonText ("SAVED");
    ++saveFlashes;
    juce::Timer::callAfterDelay (1200, [safe = juce::Component::SafePointer<MainView> (this), flash = saveFlashes]
    {
        if (safe == nullptr || safe->saveFlashes != flash)
            return;   // (saved again meanwhile: that one puts it back)
        safe->saveButton.getProperties().remove ("lit");
        safe->saveButton.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        safe->saveButton.setButtonText ("SAVE");
    });
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
                              studio.saveInBackground (file, [then, file, safe = juce::Component::SafePointer<MainView> (this)] (bool saved)
                              {
                                  if (saved && safe != nullptr)
                                      safe->flashSaved();
                                  if (! saved)
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't save", file.getFullPathName());
                                  if (then)
                                      then (saved);
                              });
                          });
}

//==============================================================================
void MainView::importMidiFile (bool newSong)
{
    // Several at once: each file's tracks become new tracks (into this song at bar 1, or the first file as a new song).
    chooser = std::make_unique<juce::FileChooser> (newSong ? "Import MIDI files as a new song" : "Import MIDI files into this song",
                                                   studio.getFile().getParentDirectory(), "*.mid;*.midi");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                          [this, newSong] (const juce::FileChooser& fc)
                          {
                              importMidiFiles (fc.getResults(), newSong, 0.0);
                          });
}

void MainView::importDawProjectFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Import a DAWproject as a new song", studio.getFile().getParentDirectory(), "*.dawproject");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              if (fc.getResult() != juce::File())
                                  importDawProject (fc.getResult());
                          });
}

void MainView::importDawProject (const juce::File& file)
{
    // A new song: unsaved changes to this one asked about first (after the chooser, never before it: 0.9.7).
    const auto go = [safe = juce::Component::SafePointer<MainView> (this), file]
    {
        if (safe == nullptr)
            return;
        juce::MouseCursor::showWaitCursor();
        juce::String report;
        const auto done = safe->studio.importDawProject (file, report);
        juce::MouseCursor::hideWaitCursor();
        if (! done)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't import", report);
            return;
        }
        safe->arrange.zoomFull();
        auto lines = juce::StringArray::fromLines (report);
        lines.removeEmptyStrings();
        if (lines.size() > 24)
        {
            const auto more = lines.size() - 24;
            lines.removeRange (24, lines.size());
            lines.add ("...and " + juce::String (more) + " more.");
        }
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Imported " + file.getFileNameWithoutExtension(),
                                                lines.isEmpty() ? juce::String ("Everything came across.")
                                                                : "Brought across, except:\n\n- " + lines.joinIntoString ("\n- ")
                                                                  + "\n\nIt's a new song: SAVE it to keep it.");
    };
    if (! studio.isModified())
    {
        go();
        return;
    }
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Import a DAWproject",
                                        "Open " + file.getFileName() + " as a new song? Changes to this one that aren't saved will be lost.",
                                        "Import", "Cancel", nullptr,
                                        juce::ModalCallbackFunction::create ([go] (int result) { if (result == 1) go(); }));
}

bool MainView::isInterestedInFileDrag (const juce::StringArray& files)
{
    return std::any_of (files.begin(), files.end(), [] (const juce::String& f)
    {
        return f.endsWithIgnoreCase (".dawproject") || f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi") || pr7s::Studio::isAudioFile (juce::File (f));
    });
}

void MainView::importAudioFiles (bool oneTrack)
{
    chooser = std::make_unique<juce::FileChooser> ("Import audio files", studio.getFile().getParentDirectory(), "*.wav;*.wave;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                          [this, oneTrack] (const juce::FileChooser& fc)
                          {
                              const auto files = fc.getResults();
                              if (! files.isEmpty())
                                  importAudio (files, studio.getSelectedTrack(), studio.getEngine().getPosition(), oneTrack);
                          });
}

void MainView::importMidiFiles (const juce::Array<juce::File>& files, bool newSong, double atBeat)
{
    // Into this song: the file's tempo, or the song's? (Only asked when they differ;
    // a new song always takes the file's.)
    if (! newSong && ! files.isEmpty())
    {
        juce::String error;
        if (const auto midi = pr7s::readMidiFile (files.getFirst(), error);
            midi && (std::abs (midi->bpm - studio.getProject().bpm) > 0.005 || midi->tempoChanges.size() > 1))
        {
            askMidiTempo (files.getFirst(), midi->bpm, [safe = juce::Component::SafePointer<MainView> (this), files, atBeat] (int result)
            {
                // (1: match, 2: keep, 0: cancel)
                if (safe != nullptr && result != 0)
                    safe->importMidiFilesNow (files, false, atBeat, result == 1);
            });
            return;
        }
    }
    importMidiFilesNow (files, newSong, atBeat, false);
}

void MainView::importMidiFilesNow (const juce::Array<juce::File>& files, bool newSong, double atBeat, bool matchTempo)
{
    // The first into a new song if asked (its tempo and signature); the others into the song, every track of each a new track.
    juce::StringArray problems;
    auto first = true;
    for (const auto& file : files)
    {
        juce::String error;
        if (studio.importMidi (file, newSong && first, atBeat, error, matchTempo && first) == 0)
            problems.add (file.getFileName() + ": " + error);
        else
            first = false;
    }
    if (! problems.isEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                problems.size() == 1 ? "Couldn't import the MIDI file" : "Couldn't import every MIDI file",
                                                problems.joinIntoString ("\n"));
}

void MainView::importAudio (const juce::Array<juce::File>& files, int trackId, double atBeat, bool oneTrack)
{
    juce::String error;
    studio.importAudio (files, trackId, atBeat, error, oneTrack);
    if (error.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't import every file", error);
}

void MainView::filesDropped (const juce::StringArray& files, int x, int y)
{
    // A DAWproject: a new song (0.9.13).
    for (const auto& path : files)
        if (path.endsWithIgnoreCase (".dawproject"))
        {
            importDawProject (juce::File (path));
            return;
        }
    // Audio files: on the audio track dropped on (else a new one), at the bar dropped on (else at the playhead).
    juce::Array<juce::File> audio;
    for (const auto& path : files)
        if (pr7s::Studio::isAudioFile (juce::File (path)))
            audio.add (juce::File (path));
    if (! audio.isEmpty())
    {
        const auto inArrange = arrange.getLocalPoint (this, juce::Point<int> (x, y));
        auto at = studio.getEngine().getPosition();
        auto trackId = 0;
        if (arrange.getLocalBounds().contains (inArrange))
        {
            const auto bar = studio.getProject().beatsPerBar();
            at = juce::jmax (0.0, std::floor (timeline.beatForX ((float) inArrange.x) / bar) * bar);
            trackId = arrange.trackAtY ((float) inArrange.y);
        }
        if (audio.size() == 1)
            importAudio (audio, trackId, at, true);
        else
        {
            // Several: a track each, or one after another (a menu where they were dropped, not a question box).
            juce::PopupMenu menu;
            menu.addSectionHeader ("IMPORT " + juce::String (audio.size()) + " AUDIO FILES");
            menu.addItem (1, "A track each");
            menu.addItem (2, "One after another on one track");
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (juce::Rectangle<int> (x, y, 1, 1) + getScreenPosition()),
                                [safe = juce::Component::SafePointer<MainView> (this), audio, trackId, at] (int chosen)
                                {
                                    if (safe != nullptr && chosen != 0)
                                        safe->importAudio (audio, trackId, at, chosen == 2);
                                });
        }
    }

    // Into this song, at the bar it's dropped on (over the arrangement), else bar 1.
    auto atBeat = 0.0;
    const auto inArrange = arrange.getLocalPoint (this, juce::Point<int> (x, y));
    if (arrange.getLocalBounds().contains (inArrange))
    {
        const auto bar = studio.getProject().beatsPerBar();
        atBeat = juce::jmax (0.0, std::floor (timeline.beatForX ((float) inArrange.x) / bar) * bar);
    }
    juce::Array<juce::File> midi;
    for (const auto& path : files)
        if (path.endsWithIgnoreCase (".mid") || path.endsWithIgnoreCase (".midi"))
            midi.add (juce::File (path));
    importMidiFiles (midi, false, atBeat);
}

//==============================================================================
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
        // The chooser after the page has gone (never opened inside a closing box's click: 0.9.7).
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainView> (this), chosen]
        {
            if (safe != nullptr)
                safe->chooseExportFile (chosen);
        });
    };
}

void MainView::chooseExportFile (const pr7s::Studio::ExportOptions& chosen)
{
    {
        // Where: beside the song, named after it.
        const auto song = studio.getFile();
        const auto start = song != juce::File() ? song.withFileExtension (".wav")
                         : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mix.wav");
        chooser = std::make_unique<juce::FileChooser> ("Export the mix", start, "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe = juce::Component::SafePointer<MainView> (this), chosen] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();
                                  // (After the chooser has gone: the render's loop isn't run inside its callback.)
                                  if (file != juce::File())
                                      juce::MessageManager::callAsync ([safe, chosen, file]
                                      {
                                          if (safe != nullptr)
                                              safe->writeExport (file.withFileExtension (".wav"), chosen);
                                      });
                              });
    }
}

void MainView::renderInPlace (std::vector<pr7s::Studio::RenderSource> sources, bool ask)
{
    if (sources.empty() || studio.isExporting() || studio.getEngine().isRecording() || renderPage != nullptr)
        return;
    if (! ask)
    {
        render (sources, Windows::RenderPage::keptOptions (studio.getSettings()));
        return;
    }
    auto* page = new Windows::RenderPage (studio, sources.front().wholeTrack);
    renderPage = page;
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (page);
    options.dialogTitle = "Paragon PR7-S: Render in Place";
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
    page->onRender = [this, close, sources] (const pr7s::Studio::RenderOptions& chosen)
    {
        close();
        // After the page has gone (its click is over).
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainView> (this), sources, chosen]
        {
            if (safe != nullptr)
                safe->render (sources, chosen);
        });
    };
}

void MainView::render (const std::vector<pr7s::Studio::RenderSource>& sources, const pr7s::Studio::RenderOptions& options)
{
    juce::MouseCursor::showWaitCursor();
    juce::String error;
    const auto made = studio.renderInPlace (sources, options, error);
    juce::MouseCursor::hideWaitCursor();
    if (error.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                made.empty() ? "Nothing rendered" : "Not everything was rendered", error);
}

void MainView::writeExport (const juce::File& file, const pr7s::Studio::ExportOptions& options)
{
    // As Render in Place (0.13.1): on a thread of its own behind the progress
    // box, the screen carrying on; CANCEL stops it at its next block. (It had
    // a window of its own whose CANCEL waited for the thread on the screen's
    // thread: an instrument rendering offline that waited for the screen
    // never let it stop, and after 10 s the thread was killed mid-block.)
    studio.setProgressStage (0.0, 1.0, "Exporting the mix");
    juce::String error;
    const auto written = studio.exportMix (file, options, error);
    const auto cancelled = studio.wasRenderCancelled();
    studio.endProgress();
    if (! written && ! cancelled)
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't export",
                                                error.isNotEmpty() ? error : "Couldn't write " + file.getFullPathName() + " (is the disk full?)");
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
    if (instance == nullptr || track == nullptr || ! studio.hasEditor (instance))
        return;
    auto window = std::make_unique<Windows::Instrument> (track->name, *instance);
    window->onKey = [this] (const juce::KeyPress& key) { return keyPressed (key); };   // (the studio's keys work there too)
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
    settingsWindow = std::make_unique<Windows::Settings> (studio, [this] { toggleSpanAllScreens(); },
                                                          [this] { return isSpanningAllScreens(); });
    settingsWindow->onClose = [safe = juce::Component::SafePointer<MainView> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->settingsWindow.reset(); });
    };
}

void MainView::whenSongCanGo (std::function<void()> then)
{
    if (! studio.isModified())
    {
        then();
        return;
    }
    juce::AlertWindow::showYesNoCancelBox (juce::MessageBoxIconType::QuestionIcon, "Save the song?",
                                           "This song has changes that aren't saved.", "Save", "Don't save", "Cancel", nullptr,
                                           juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<MainView> (this), then] (int result)
    {
        if (safe == nullptr || result == 0)
            return;
        if (result == 2)
        {
            then();
            return;
        }
        // (After the box has gone: a chooser opened inside its answer closes with it on Windows, 0.9.7.)
        juce::MessageManager::callAsync ([safe, then]
        {
            if (safe != nullptr)
                safe->saveProject ([then] (bool saved) { if (saved) then(); });
        });
    }));
}

void MainView::openSong (const juce::File& file)
{
    juce::String problems;
    if (! studio.open (file, problems))
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't open the song",
                                                file.getFullPathName() + " isn't a Paragon PR7-S song (or is from a newer version).");
    else if (problems.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Some instruments, effects or recordings are missing",
                                                "These aren't on this computer. Tracks without their instrument and clips without their "
                                                "recording are silent, and the sound goes past missing effects, until they're found:\n\n" + problems);
}

void MainView::toggleTempoEditor()
{
    if (tempoEditorWindow != nullptr)
    {
        tempoEditorWindow = nullptr;
        return;
    }
    auto window = std::make_unique<TempoEditorWindow> (studio, studio.getSettings());
    window->onClose = [safe = juce::Component::SafePointer<MainView> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->tempoEditorWindow = nullptr; });
    };
    // Keys it doesn't use are the studio's (Space plays, Ctrl+T closes it...).
    window->getEditor().onOtherKey = [safe = juce::Component::SafePointer<MainView> (this)] (const juce::KeyPress& key)
    {
        if (safe == nullptr)
            return false;
        if (key.getModifiers().isCommandDown() && (key.getKeyCode() == 'T' || key.getKeyCode() == 't'))
        {
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->tempoEditorWindow = nullptr; });
            return true;
        }
        return safe->keyPressed (key);
    };
    tempoEditorWindow = std::move (window);
}

void MainView::openHub()
{
    if (hubWindow != nullptr)
    {
        hubWindow->toFront (true);
        return;
    }
    auto window = std::make_unique<HubWindow> (studio);
    // Centred on the middle screen (the owner's three: Windows' own centring
    // put it on the main display, the left one): the screen under the studio
    // window's centre (it spans them all), else under the middle of all of them.
    {
        const auto* top = getTopLevelComponent();
        const auto centre = top != nullptr && top->isShowing() ? top->getScreenBounds().getCentre()
                                                               : Windows::allScreensArea().getCentre();
        if (const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForPoint (centre))
            window->setCentrePosition (display->userArea.getCentre());
    }
    auto& hub = window->getHub();
    const auto closeHub = [safe = juce::Component::SafePointer<MainView> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->hubWindow.reset(); });
    };
    window->onClose = closeHub;
    hub.onOpen = [this, closeHub] (const juce::File& file)
    {
        whenSongCanGo ([this, file, closeHub] { openSong (file); closeHub(); });
    };
    hub.onTemplate = [this, closeHub] (const juce::File& file)
    {
        whenSongCanGo ([this, file, closeHub]
        {
            juce::String problems;
            if (! studio.newFromTemplate (file, problems))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't open the template", file.getFullPathName());
            closeHub();
        });
    };
    hub.onCreate = [this, closeHub] (const juce::String& name)
    {
        whenSongCanGo ([this, name, closeHub]
        {
            juce::String error;
            if (! studio.getLibrary().usesDefaultLocation())
                studio.newProject();
            else if (! studio.createSong (name, error))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't create the song", error);
            closeHub();
        });
    };
    hub.onChooseFile = [this, closeHub]
    {
        chooser = std::make_unique<juce::FileChooser> ("Open a song", studio.getLibrary().getDefaultLocation(),
                                                       juce::String ("*") + pr7s::Project::fileExtension);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, closeHub] (const juce::FileChooser& fc)
        {
            if (fc.getResult().existsAsFile())
                whenSongCanGo ([this, file = fc.getResult(), closeHub] { openSong (file); closeHub(); });
        });
    };
    hub.onSettings = [this] { openSettings(); };
    hubWindow = std::move (window);
}

void MainView::saveAsTemplate()
{
    auto* box = new juce::AlertWindow ("Save as Template", "A name for the template (the Hub lists it under TEMPLATES):", juce::MessageBoxIconType::NoIcon);
    box->addTextEditor ("name", studio.getFile() != juce::File() ? studio.getFile().getFileNameWithoutExtension() : juce::String ("My template"));
    box->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    box->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    box->enterModalState (true, juce::ModalCallbackFunction::create ([box, safe = juce::Component::SafePointer<MainView> (this)] (int result)
    {
        if (safe == nullptr || result != 1)
            return;
        juce::String error;
        if (! safe->studio.saveAsTemplate (box->getTextEditorContents ("name"), error))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't save the template", error);
    }), true);
}

void MainView::openKeyCommands()
{
    if (keysWindow != nullptr)
    {
        keysWindow->toFront (true);
        return;
    }
    auto window = std::make_unique<KeyCommandsWindow> (keyMap);
    window->onClose = [safe = juce::Component::SafePointer<MainView> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->keysWindow.reset(); });
    };
    keysWindow = std::move (window);
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

//==============================================================================
bool MainView::isSpanningAllScreens() const
{
    // Wider than the widest screen: across more than one.
    auto widest = 0;
    for (const auto& display : juce::Desktop::getInstance().getDisplays().displays)
        widest = juce::jmax (widest, juce::roundToInt (display.userBounds.getWidth()));
    const auto* window = getTopLevelComponent();
    return window != nullptr && juce::Desktop::getInstance().getDisplays().displays.size() > 1 && window->getWidth() > widest + 50;
}

void MainView::toggleSpanAllScreens()
{
    auto* window = dynamic_cast<juce::ResizableWindow*> (getTopLevelComponent());
    if (window == nullptr)
        return;
    auto* settings = studio.getSettings();
    if (isSpanningAllScreens())
    {
        // Back where it was before (else the middle of the main screen).
        const auto before = settings != nullptr ? settings->getValue ("mainWindowOneScreen") : juce::String();
        if (before.isEmpty() || ! window->restoreWindowStateFromString (before))
            window->centreWithSize (1500, 900);
        return;
    }
    if (settings != nullptr)
        settings->setValue ("mainWindowOneScreen", window->getWindowStateAsString());
    window->setFullScreen (false);   // (maximized fills one screen only)
    auto area = Windows::allScreensArea();
    if (auto* peer = window->getPeer())
        if (const auto frame = peer->getFrameSizeIfPresent())
            area = frame->subtractedFrom (area);   // the title bar and edges inside it too
    window->setBounds (area);
}
