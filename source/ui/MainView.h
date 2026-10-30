#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "engine/Studio.h"
#include "ui/ArrangeView.h"
#include "ui/KeyEditor.h"
#include "ui/TrackList.h"
#include "ui/TransportBar.h"
#include "ui/Windows.h"

// The studio's window, top to bottom:
//
//   header       the PR7-S mark; NEW, OPEN, SAVE, SAVE AS; UNDO, REDO;
//                PLUGINS and SETTINGS
//   transport    the pearl unit: keys, position, tempo, signature, meters
//   middle       the track list | the arrangement, and below them (when
//                open) the key editor for the selected part
//   keyboard     an on-screen keyboard (click or play your computer keys),
//                into the armed track
//
// Keys (inspired by Cubase's workflow): Space play / stop, R (or * on the number pad) record,
// Home to the start, / cycle, C click, Delete the selected part, Ctrl+D
// duplicate it, Ctrl+Z / Ctrl+Shift+Z undo / redo, Ctrl+N / O / S new, open
// and save (Ctrl+Shift+S save as), Ctrl+E the key editor (Escape closes it).
// After a click in the key editor its keys go to it (Delete removes notes,
// not the part); after a click elsewhere, to the arrangement.
class MainView final : public juce::Component,
                       private juce::ChangeListener
{
public:
    explicit MainView (pr7s::Studio&);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;   // also hears clicks in the arrangement, track list and editor

    // File actions (the window's close asks about unsaved changes through these).
    void newProject();
    void openProject();
    void saveProject (std::function<void (bool saved)> then = {});
    void saveProjectAs (std::function<void (bool saved)> then = {});

    void openInstrumentWindow (int trackId);
    void openSettings();
    void openPlugins();

    // The key editor, for the arrangement's selected part.
    void openKeyEditor();
    void closeKeyEditor();
    bool isKeyEditorOpen() const noexcept { return keyEditorOpen; }
    // In a window of its own (WINDOW), or under the arrangement (DOCK).
    // Remembered for next time, as is where the window was.
    void setKeyEditorInWindow (bool);
    bool isKeyEditorInWindow() const noexcept { return keyEditorInWindow; }
    Windows::KeyEditorWindow* getKeyEditorWindow() noexcept { return keyEditorWindow.get(); }

    // The window's title: the project's name, * when changed.
    std::function<void (const juce::String&)> onTitleChange;

    ArrangeView& getArrangeView() noexcept { return arrange; }
    TrackList& getTrackList() noexcept { return trackList; }
    TransportBar& getTransport() noexcept { return transport; }
    KeyEditor& getKeyEditor() noexcept { return keyEditor; }

    static constexpr int headerHeight = 52, transportHeight = 112, keyboardHeight = 84, trackListWidth = 300;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refreshHeader();
    void placeKeyEditor();

    pr7s::Studio& studio;
    Timeline timeline;
    TransportBar transport;
    TrackList trackList;
    ArrangeView arrange;
    KeyEditor keyEditor;
    bool keyEditorOpen = false, keysToEditor = false, keyEditorInWindow = false;
    std::unique_ptr<Windows::KeyEditorWindow> keyEditorWindow;   // after the editor: goes first
    juce::MidiKeyboardComponent keyboard;

    juce::TextButton newButton { "NEW" }, openButton { "OPEN" }, saveButton { "SAVE" }, saveAsButton { "SAVE AS" },
                     undoButton { "UNDO" }, redoButton { "REDO" }, editorButton { "KEY EDITOR" }, pluginsButton { "PLUGINS" }, settingsButton { "SETTINGS" };
    juce::Image keyboardSatin;
    std::map<int, std::unique_ptr<Windows::Instrument>> instrumentWindows;
    std::unique_ptr<Windows::Settings> settingsWindow;
    std::unique_ptr<Windows::Plugins> pluginsWindow;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastTitle;
};
