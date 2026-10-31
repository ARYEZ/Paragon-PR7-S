#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "engine/Studio.h"
#include "ui/ArrangeView.h"
#include "ui/KeyEditor.h"
#include "ui/Inspector.h"
#include "ui/MixerView.h"
#include "ui/TrackList.h"
#include "ui/TransportBar.h"
#include "ui/Windows.h"

// The studio's window, top to bottom:
//
//   header       the PR7-S mark; NEW, OPEN, SAVE, SAVE AS, IMPORT, EXPORT; UNDO, REDO;
//                KEY EDITOR, MIXER; R and W (read and write automation for
//                all tracks, lit while any track's are on); PLUGINS and SETTINGS
//   transport    the pearl unit: keys, position, tempo, signature, meters
//   tools        the arrangement's editing tools (see ArrangeView)
//   middle       the track list | the arrangement, and below them, in the
//                lower zone, the key editor or the mixer (one at a time;
//                either can go into a window of its own instead)
//
// Keys (inspired by Cubase's workflow): Space play / stop, R (or * on the number pad) record,
// Home to the start, / cycle, C click, Delete the selected part, Ctrl+D
// duplicate it, Ctrl+Z / Ctrl+Shift+Z undo / redo, Ctrl+N / O / S new, open
// and save (Ctrl+Shift+S save as), Ctrl+E the key editor (Escape closes it),
// F3 the mixer in a window of its own, Alt+F3 (or MIXER) the mixer docked
// (as Cubase's F3 and Alt+F3 do), Alt+R / Alt+W read / write automation for
// all tracks (the manual's keys). Delete removes the selected automation
// points if there are any (else the selected part); Esc while dragging
// points puts them back. H / G zoom in / out (a step, centred on the
// playhead), Shift+H / Shift+G taller or shorter rows, Shift+F the whole
// song, F Auto-Scroll on or off (or carry on, if suspended): in the
// arrangement, or in the key editor after a click in it (the manual's keys;
// each has its own Auto-Scroll, as in the manual). The arrangement's tools:
// 1 and 3 to 9 (the key editor's own 1, 5 and 8 after a click in it), F9 /
// F10 the previous / next; Alt+X splits at the playhead. With a range (RANGE,
// 2): Shift+X, Delete, Shift+Backspace, Ctrl+X / C, Ctrl+Shift+X, Ctrl+D,
// Ctrl+Shift+E, Esc; Ctrl+V / Ctrl+Shift+V paste a range's contents if that
// was copied last (else the key editor's notes).
// Scroll bars across and down the arrangement. Drag the line above the
// lower zone to make it taller or shorter (remembered for each of the two).
// After a click in the key editor its keys go to it (Delete removes notes,
// not the part); after a click elsewhere, to the arrangement. Ctrl+C / X / V
// and Shift+A go to the editor whenever it's open.
class MainView final : public juce::Component,
                       public juce::FileDragAndDropTarget,
                       private juce::ChangeListener,
                       private juce::ScrollBar::Listener
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
    // RENDER IN PLACE: the sources (a part, a range, selected tracks) rendered
    // to audio; `ask`: the render page first (else as last time).
    void renderInPlace (std::vector<pr7s::Studio::RenderSource>, bool ask);
    Windows::RenderPage* getRenderPage() noexcept { return renderPage.getComponent(); }
    Windows::ExportPage* getExportPage() noexcept { return exportPage.getComponent(); }

    // IMPORT: a standard MIDI file (as the manual's File > Import > MIDI
    // File): asks whether into a new song (if this one has anything in it),
    // then which file. A .mid file dropped on the window goes into this song
    // at the bar it's dropped on (bar 1 if not over the arrangement).
    void importMidiFile();
    // IMPORT > Audio files: WAV, AIFF, FLAC, Ogg or MP3, as clips at the
    // playhead on the selected audio track (else a new one); dropped on the
    // arrangement, on the audio track under them (else a new one) at the bar.
    void importAudioFiles();
    void importAudio (const juce::Array<juce::File>&, int trackId, double atBeat);
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int x, int y) override;

    void openInstrumentWindow (int trackId);
    void openSettings();
    // SPAN ALL SCREENS (in SETTINGS): the window across every screen (Windows'
    // maximize fills only one); again, back where it was. The window opens
    // where it was left next time (Main.cpp keeps its place).
    void toggleSpanAllScreens();
    bool isSpanningAllScreens() const;
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
    // The Inspector (the header's INSPECTOR, Ctrl+Alt+L: the manual's Show/Hide Left Zone). Kept for next time.
    void toggleInspector();
    bool isInspectorShown() const noexcept { return inspectorShown; }
    Inspector& getInspector() noexcept { return inspector; }

    // The line between the arrangement and the docked lower zone: drag it
    // up or down. Each of the two remembers its height (in the settings file).
    class ZoneDivider;
    ZoneDivider& getZoneDivider() noexcept { return *zoneDivider; }
    // The Inspector's width (0.9.2): drag its right edge; kept for next time.
    class InspectorEdge;
    void setInspectorWidth (int);
    int getInspectorWidth() const noexcept { return inspectorWidth; }
    static constexpr int minimumInspectorWidth = 180, maximumInspectorWidth = 560;
    void setDockedHeight (int);   // the docked one's (kept between its least and what leaves the arrangement room)
    static constexpr int dividerHeight = 7, minimumArrangeHeight = 170;

    // The arrangement's Auto-Scroll (F, or AUTO-SCROLL over the track list), remembered for next time.
    void setArrangeAutoScroll (bool);
    juce::TextButton& getAutoScrollButton() noexcept { return autoScrollKey; }   // (on the transport, by HOME)
    juce::ScrollBar& getArrangeScrollBarAcross() noexcept { return arrangeAcross; }
    juce::ScrollBar& getArrangeScrollBarDown() noexcept { return arrangeDown; }
    static constexpr int scrollBarSize = 12;

    // The window's title: the project's name, * when changed.
    std::function<void (const juce::String&)> onTitleChange;

    ArrangeView& getArrangeView() noexcept { return arrange; }
    Timeline& getTimeline() noexcept { return timeline; }
    TrackList& getTrackList() noexcept { return trackList; }
    TransportBar& getTransport() noexcept { return transport; }
    KeyEditor& getKeyEditor() noexcept { return keyEditor; }
    MixerView& getMixer() noexcept { return mixer; }

    static constexpr int headerHeight = 52, transportHeight = 112, toolbarHeight = 32, trackListWidth = 300;
    // From this wide (spanning screens), the wide layout: the header's keys and the transport's position
    // screen in the middle, the tools on the transport (no row of their own).
    static constexpr int wideWidth = 2400;
    bool isWide() const noexcept { return getWidth() >= wideWidth; }
    ArrangeTools& getTools() noexcept { return tools; }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void refreshHeader();
    void refreshScrollBars();

    struct Zone
    {
        juce::Component& content;
        juce::TextButton& button;           // its key in the header (lit while open)
        juce::String title, settingsKey;    // the window's title; settingsKey + "InWindow" and + "Window" in the settings file
        bool open = false, inWindow = false;
        std::unique_ptr<Windows::ZoneWindow> window;
        int dockedHeight = 0;   // dragged to (0: the default, a little over half); settingsKey + "Height"
    };
    Zone* dockedZone() noexcept;   // the one open under the arrangement, if any
    int heightFor (Zone&, int middleHeight) const;
    int minimumHeight (Zone&) const;
    void openZone (Zone&);
    void closeZone (Zone&);
    void setZoneInWindow (Zone&, bool);
    void placeZone (Zone&);

    pr7s::Studio& studio;
    Timeline timeline;
    TransportBar transport;
    TrackList trackList;
    ArrangeView arrange;
    ArrangeTools tools { arrange };
    juce::TextButton autoScrollKey { "AUTO-SCROLL" }, autoScrollMenu;   // the arrangement's Auto-Scroll, on the transport
    void toggleArrangeAutoScroll();
    KeyEditor keyEditor;
    MixerView mixer;
    Inspector inspector;             // left of the track list (0.9.0)
    bool inspectorShown = true;
    bool keysToEditor = false;
    bool rangeCopiedLast = false;
    enum class Copied { none, range, notes, parts };
    Copied copiedLast = Copied::none;   // what Ctrl+V pastes in the arrangement
    bool wasWide = false;            // the last layout's: turning wide opens the mixer, short, racks hidden   // Ctrl+V: a range's contents (in the arrangement), else notes (the key editor)
    juce::ScrollBar arrangeAcross { false }, arrangeDown { true };
    std::unique_ptr<ZoneDivider> zoneDivider;
    std::unique_ptr<InspectorEdge> inspectorEdge;
    int inspectorWidth = Inspector::width;

    juce::TextButton newButton { "NEW" }, openButton { "OPEN" }, saveButton { "SAVE" }, saveAsButton { "SAVE AS" }, importButton { "IMPORT" }, exportButton { "EXPORT" },
                     undoButton { "UNDO" }, redoButton { "REDO" }, editorButton { "KEY EDITOR" }, mixerButton { "MIXER" }, inspectorButton { "INSPECTOR" },
                     pluginsButton { "PLUGINS" }, settingsButton { "SETTINGS" }, readAllButton { "R" }, writeAllButton { "W" };
    // After the editor and the mixer: their windows go first.
    Zone editorZone { keyEditor, editorButton, "Key Editor", "keyEditor", false, false, {} },
         mixerZone { mixer, mixerButton, "Mixer", "mixer", false, false, {} };
    std::map<int, std::unique_ptr<Windows::Instrument>> instrumentWindows;
    std::map<juce::AudioProcessor*, std::unique_ptr<Windows::Instrument>> effectWindows;
    std::unique_ptr<Windows::Settings> settingsWindow;
    std::unique_ptr<Windows::Plugins> pluginsWindow;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<Windows::ExportPage> exportPage;
    juce::Component::SafePointer<Windows::RenderPage> renderPage;
    void render (const std::vector<pr7s::Studio::RenderSource>&, const pr7s::Studio::RenderOptions&);
    void writeExport (const juce::File&, const pr7s::Studio::ExportOptions&);
    std::unique_ptr<juce::ThreadWithProgressWindow> exportProgress;
    juce::String lastTitle;
};

// The Inspector's right edge: drag it to make the Inspector wider or narrower.
class MainView::InspectorEdge final : public juce::Component,
                                     public juce::SettableTooltipClient
{
public:
    explicit InspectorEdge (MainView&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    MainView& owner;
    int widthAtDown = 0;
};

// The line above the docked lower zone (a grip in the middle): drag it.
class MainView::ZoneDivider final : public juce::Component,
                                   public juce::SettableTooltipClient
{
public:
    explicit ZoneDivider (MainView&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    MainView& owner;
    int heightAtDown = 0;
};
