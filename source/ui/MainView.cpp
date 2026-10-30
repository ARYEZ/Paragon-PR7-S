#include "MainView.h"

#include "ui/Logo.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/Pearl.h"

namespace
{
    juce::FontOptions font (float height, int style = juce::Font::bold) { return juce::FontOptions (height, style); }
}

MainView::MainView (pr7s::Studio& studioToUse)
    : studio (studioToUse),
      transport (studioToUse),
      trackList (studioToUse, timeline),
      arrange (studioToUse, timeline),
      keyEditor (studioToUse, arrange),
      keyboard (studioToUse.getEngine().getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    for (auto* button : { &newButton, &openButton, &saveButton, &saveAsButton, &undoButton, &redoButton, &editorButton, &pluginsButton, &settingsButton })
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
    undoButton.setTooltip ("Undo (Ctrl+Z)");
    redoButton.setTooltip ("Redo (Ctrl+Shift+Z)");
    editorButton.setTooltip ("The key editor: the selected part's notes (Ctrl+E; or double-click a part)");
    editorButton.setColour (juce::TextButton::textColourOnId, ParagonColours::accent);
    pluginsButton.setTooltip ("Find your VST3 instruments");
    settingsButton.setTooltip ("Audio interface, MIDI inputs and the finish");
    newButton.onClick = [this] { newProject(); };
    openButton.onClick = [this] { openProject(); };
    saveButton.onClick = [this] { saveProject(); };
    saveAsButton.onClick = [this] { saveProjectAs(); };
    undoButton.onClick = [this] { studio.undo(); };
    redoButton.onClick = [this] { studio.redo(); };
    editorButton.onClick = [this] { keyEditorOpen ? closeKeyEditor() : openKeyEditor(); };
    pluginsButton.onClick = [this] { openPlugins(); };
    settingsButton.onClick = [this] { openSettings(); };

    addAndMakeVisible (transport);
    addAndMakeVisible (trackList);
    addAndMakeVisible (arrange);
    addChildComponent (keyEditor);
    keyEditor.onClose = [this] { closeKeyEditor(); };
    arrange.onEditNotes = [this] { openKeyEditor(); };
    // Told of clicks in the work areas, to know where the keys go.
    arrange.addMouseListener (this, true);
    trackList.addMouseListener (this, true);
    keyEditor.addMouseListener (this, true);

    // The on-screen keyboard: keys in the PR7's colours, middle C marked.
    keyboard.setAvailableRange (24, 108);
    keyboard.setKeyWidth (18.0f);
    keyboard.setLowestVisibleKey (36);
    keyboard.setWantsKeyboardFocus (false);   // the computer's keys stay the studio's shortcuts
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, ParagonColours::accent.withAlpha (0.8f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, ParagonColours::accent.withAlpha (0.25f));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff3a3236));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.3f));
    addAndMakeVisible (keyboard);

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

    studio.addChangeListener (this);
    Finish::changes().addChangeListener (this);
    setWantsKeyboardFocus (true);
    refreshHeader();
}

MainView::~MainView()
{
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

    // The keyboard on its own pearl strip.
    const auto strip = getLocalBounds().removeFromBottom (keyboardHeight).toFloat().reduced (6.0f, 4.0f);
    Pearl::paintUnit (g, strip, keyboardSatin, 8.0f);
    g.setColour (Finish::printedRed());
    g.setFont (font (9.0f));
    g.drawText ("KEYBOARD", strip.withWidth (90.0f).reduced (12.0f, 8.0f), juce::Justification::topLeft, false);

    // The work area's frame.
    const auto middle = getLocalBounds().withTrimmedTop (headerHeight + transportHeight).withTrimmedBottom (keyboardHeight).toFloat().reduced (6.0f, 2.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRect (middle, 1.0f);
}

void MainView::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (headerHeight).reduced (8, 10);
    header.removeFromLeft (280);
    for (auto* button : { &newButton, &openButton, &saveButton, &saveAsButton })
    {
        button->setBounds (header.removeFromLeft (button == &saveAsButton ? 84 : 66));
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
    settingsButton.setBounds (header.removeFromRight (92));
    header.removeFromRight (6);
    pluginsButton.setBounds (header.removeFromRight (86));

    transport.setBounds (area.removeFromTop (transportHeight).reduced (2, 0));

    auto bottom = area.removeFromBottom (keyboardHeight);
    keyboardSatin = Finish::makeSatinGrain (juce::jmax (1, bottom.getWidth() * 2), juce::jmax (1, bottom.getHeight() * 2), 4242);
    keyboard.setBounds (bottom.reduced (6, 4).withTrimmedLeft (90).reduced (6, 10));
    // The whole range (C1 to C8: 50 white keys) fills the strip.
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 50.0f);
    keyboard.setLowestVisibleKey (24);

    auto middle = area.reduced (7, 3);
    if (keyEditorOpen)
    {
        // The editor takes the lower part (a little over half), the arrangement keeps the rest.
        const auto height = juce::jlimit (juce::jmin (240, middle.getHeight()), juce::jmax (240, middle.getHeight() - 170),
                                          juce::roundToInt ((float) middle.getHeight() * 0.56f));
        keyEditor.setBounds (middle.removeFromBottom (height));
        middle.removeFromBottom (3);
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

    if (keyEditorOpen && keysToEditor && keyEditor.keyPressed (key))
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
            case 'E': keyEditorOpen ? closeKeyEditor() : openKeyEditor(); return true;
            default: return false;
        }
    }

    if (keyEditorOpen && key == juce::KeyPress::escapeKey) { closeKeyEditor(); return true; }
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
void MainView::openKeyEditor()
{
    keyEditorOpen = keysToEditor = true;
    editorButton.setToggleState (true, juce::dontSendNotification);
    keyEditor.setVisible (true);
    resized();
    repaint();
}

void MainView::closeKeyEditor()
{
    keyEditorOpen = keysToEditor = false;
    editorButton.setToggleState (false, juce::dontSendNotification);
    keyEditor.setVisible (false);
    resized();
    repaint();
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
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Some instruments are missing",
                                                    "These tracks are silent until their instrument is found (Find my VST3 instruments...):\n\n" + problems);
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
