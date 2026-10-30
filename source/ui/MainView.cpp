#include "MainView.h"

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
    for (auto* button : { &newButton, &openButton, &saveButton, &saveAsButton, &exportButton, &undoButton, &redoButton, &editorButton, &mixerButton,
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

    addAndMakeVisible (transport);
    addAndMakeVisible (trackList);
    addAndMakeVisible (arrange);
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
    timeline.onScrollOrZoom = [this]
    {
        trackList.resized();
        trackList.repaint();
        arrange.repaint();
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
    const auto middle = getLocalBounds().withTrimmedTop (headerHeight + transportHeight).toFloat().reduced (6.0f, 2.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRect (middle, 1.0f);
}

void MainView::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (headerHeight).reduced (8, 10);
    header.removeFromLeft (280);
    for (auto* button : { &newButton, &openButton, &saveButton, &saveAsButton, &exportButton })
    {
        button->setBounds (header.removeFromLeft (button == &saveAsButton ? 84 : button == &exportButton ? 76 : 66));
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
    settingsButton.setBounds (header.removeFromRight (92));
    header.removeFromRight (6);
    pluginsButton.setBounds (header.removeFromRight (86));

    transport.setBounds (area.removeFromTop (transportHeight).reduced (2, 0));

    auto middle = area.reduced (7, 3);
    for (auto* zone : { &editorZone, &mixerZone })
        if (zone->open && ! zone->inWindow)
        {
            // The lower zone takes a little over half; the arrangement keeps the rest.
            const auto height = juce::jlimit (juce::jmin (240, middle.getHeight()), juce::jmax (240, middle.getHeight() - 170),
                                              juce::roundToInt ((float) middle.getHeight() * 0.6f));
            zone->content.setBounds (middle.removeFromBottom (height));
            middle.removeFromBottom (3);
            break;
        }
    trackList.setBounds (middle.removeFromLeft (trackListWidth));
    middle.removeFromLeft (2);
    arrange.setBounds (middle);
}

//==============================================================================
bool MainView::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const auto code = key.getKeyCode();
    const auto letter = juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) code);

    if (isKeyEditorOpen() && keysToEditor && keyEditor.keyPressed (key))
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
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) { arrange.deleteSelected(); return true; }
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
