#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "engine/Studio.h"
#include "ui/ArrangeView.h"
#include "ui/KeyEditor.h"
#include "ui/MixerView.h"
#include "ui/TrackList.h"
#include "ui/TransportBar.h"
#include "ui/Windows.h"

// The studio's window, top to bottom:
//
//   header       the PR7-S mark; NEW, OPEN, SAVE, SAVE AS, EXPORT; UNDO, REDO;
//                KEY EDITOR, MIXER; PLUGINS and SETTINGS
//   transport    the pearl unit: keys, position, tempo, signature, meters
//   middle       the track list | the arrangement, and below them, in the
//                lower zone, the key editor or the mixer (one at a time;
//                either can go into a window of its own instead)
//
// Keys (inspired by Cubase's workflow): Space play / stop, R (or * on the number pad) record,
// Home to the start, / cycle, C click, Delete the selected part, Ctrl+D
// duplicate it, Ctrl+Z / Ctrl+Shift+Z undo / redo, Ctrl+N / O / S new, open
// and save (Ctrl+Shift+S save as), Ctrl+E the key editor (Escape closes it),
// F3 the mixer in a window of its own, Alt+F3 (or MIXER) the mixer docked
// (as Cubase's F3 and Alt+F3 do).
// After a click in the key editor its keys go to it (Delete removes notes,
// not the part); after a click elsewhere, to the arrangement. Ctrl+C / X / V
// and Shift+A go to the editor whenever it's open.
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
    // EXPORT: the mix to a WAV. Asks what (the export page), then where,
    // then writes it behind a progress bar (CANCEL stops it, leaving no file).
    void exportMix();
    Windows::ExportPage* getExportPage() noexcept { return exportPage.getComponent(); }

    void openInstrumentWindow (int trackId);
    void openSettings();
    void openPlugins();

    void openEffectWindow (int busId, int slot);

    // The lower zone's two: the key editor (for the arrangement's selected
    // part) and the mixer. Each is under the arrangement (DOCK) or in a
    // window of its own (WINDOW), remembered for next time, as is where the
    // window was. Only one is docked at a time: opening one docked puts away
    // the other if it's docked too.
    void openKeyEditor()                       { openZone (editorZone); }
    void closeKeyEditor()                      { closeZone (editorZone); }
    bool isKeyEditorOpen() const noexcept      { return editorZone.open; }
    void setKeyEditorInWindow (bool inWindow)  { setZoneInWindow (editorZone, inWindow); }
    bool isKeyEditorInWindow() const noexcept  { return editorZone.inWindow; }
    Windows::ZoneWindow* getKeyEditorWindow() noexcept { return editorZone.window.get(); }
    void openMixer()                           { openZone (mixerZone); }
    void closeMixer()                          { closeZone (mixerZone); }
    bool isMixerOpen() const noexcept          { return mixerZone.open; }
    void setMixerInWindow (bool inWindow)      { setZoneInWindow (mixerZone, inWindow); }
    bool isMixerInWindow() const noexcept      { return mixerZone.inWindow; }
    Windows::ZoneWindow* getMixerWindow() noexcept { return mixerZone.window.get(); }
    // F3: the mixer in its own window (closed if it's there already);
    // Alt+F3 and MIXER: docked under the arrangement (likewise).
    void toggleMixerWindow();
    void toggleMixerDocked();

    // The window's title: the project's name, * when changed.
    std::function<void (const juce::String&)> onTitleChange;

    ArrangeView& getArrangeView() noexcept { return arrange; }
    TrackList& getTrackList() noexcept { return trackList; }
    TransportBar& getTransport() noexcept { return transport; }
    KeyEditor& getKeyEditor() noexcept { return keyEditor; }
    MixerView& getMixer() noexcept { return mixer; }

    static constexpr int headerHeight = 52, transportHeight = 112, trackListWidth = 300;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refreshHeader();

    struct Zone
    {
        juce::Component& content;
        juce::TextButton& button;           // its key in the header (lit while open)
        juce::String title, settingsKey;    // the window's title; settingsKey + "InWindow" and + "Window" in the settings file
        bool open = false, inWindow = false;
        std::unique_ptr<Windows::ZoneWindow> window;
    };
    void openZone (Zone&);
    void closeZone (Zone&);
    void setZoneInWindow (Zone&, bool);
    void placeZone (Zone&);

    pr7s::Studio& studio;
    Timeline timeline;
    TransportBar transport;
    TrackList trackList;
    ArrangeView arrange;
    KeyEditor keyEditor;
    MixerView mixer;
    bool keysToEditor = false;

    juce::TextButton newButton { "NEW" }, openButton { "OPEN" }, saveButton { "SAVE" }, saveAsButton { "SAVE AS" }, exportButton { "EXPORT" },
                     undoButton { "UNDO" }, redoButton { "REDO" }, editorButton { "KEY EDITOR" }, mixerButton { "MIXER" },
                     pluginsButton { "PLUGINS" }, settingsButton { "SETTINGS" };
    // After the editor and the mixer: their windows go first.
    Zone editorZone { keyEditor, editorButton, "Key Editor", "keyEditor", false, false, {} },
         mixerZone { mixer, mixerButton, "Mixer", "mixer", false, false, {} };
    std::map<int, std::unique_ptr<Windows::Instrument>> instrumentWindows;
    std::map<juce::AudioProcessor*, std::unique_ptr<Windows::Instrument>> effectWindows;
    std::unique_ptr<Windows::Settings> settingsWindow;
    std::unique_ptr<Windows::Plugins> pluginsWindow;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<Windows::ExportPage> exportPage;
    void writeExport (const juce::File&, const pr7s::Studio::ExportOptions&);
    std::unique_ptr<juce::ThreadWithProgressWindow> exportProgress;
    juce::String lastTitle;
};
