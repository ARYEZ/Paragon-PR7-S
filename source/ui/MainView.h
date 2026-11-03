#pragma once

#include "ui/LinesPage.h"
#include "ui/MediaBayPage.h"
#include "ui/MediaRack.h"
#include "ui/KeyCommands.h"
#include "ui/ProgressBox.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>

#include "engine/Studio.h"
#include "ui/ArrangeView.h"
#include "ui/ChordPadsView.h"
#include "ui/KeyEditor.h"
#include "ui/Inspector.h"
#include "ui/ScaledHolder.h"
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
                       public juce::DragAndDropTarget,
                       private juce::ChangeListener,
                       private juce::ScrollBar::Listener
{
public:
    explicit MainView (pr7s::Studio&);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    // Keys (0.11.0): one given to a command in KEYS runs it; a default key
    // taken away does nothing; the rest are the studio's built-in keys (handleKey).
    bool keyPressed (const juce::KeyPress&) override;
    bool handleKey (const juce::KeyPress&);
    KeyCommands::Map& getKeyMap() noexcept { return keyMap; }
    void openKeyCommands();   // the Key Commands window (KEYS)
    // The Hub (0.12.0): the start window, HUB in the header. What it asks for
    // is done here, after the unsaved-changes question; it closes once a song is open.
    void openHub();
    // The Tempo Track Editor (0.13.0, Ctrl+T as the manual has it): opened, or closed if it's open.
    void toggleTempoEditor();
    // The Drum Map Setup (0.26.0): its window (the MIDI editor's MAP menu, the drum list's menu, the Inspector's DRUM MAP).
    void openDrumMapSetup();
    // The Automation panel (0.28.0, F6): its window.
    void toggleAutomationPanel();
    juce::DocumentWindow* getAutomationPanelWindow() noexcept { return automationWindow.get(); }
    juce::DocumentWindow* getDrumMapWindow() noexcept { return drumMapWindow.get(); }
    // BASS & MELODY (0.18.0): ideas for these chord events (none: the cycle's, else all), in a window of its own.
    void openLineIdeas (std::vector<int> chordEvents = {});
    LinesWindow* getLinesWindow() noexcept { return linesWindow.get(); }
    // The MediaBay (0.21.0): its window (F5 opens and closes it), and a file from it into the
    // song: audio or MIDI on a new track at the playhead, a track preset as a new track, a song
    // opened (after the unsaved-changes question). Its drags land as files dragged from the system do.
    void toggleMediaBay();
    MediaBayWindow* getMediaBayWindow() noexcept { return mediaBayWindow.get(); }
    void useMedia (const juce::File&);
    void mediaDropped (const juce::StringArray& files, int x, int y);
    // The right zone (0.22.0): the Media rack at the right, the whole height (Ctrl+Alt+R, the
    // manual's Show/Hide Right Zone). Made the first time it's shown. Its drags: instruments a new
    // instrument track, effects a new FX channel, files as the MediaBay's (a track preset onto a
    // track of its kind is applied to it).
    void toggleRightZone();
    void setRightZoneShown (bool);
    bool isRightZoneShown() const noexcept { return rightZoneShown; }
    MediaRack* getMediaRack() noexcept { return mediaRack.get(); }
    class RightZoneEdge;
    void setRightZoneWidth (int);
    int getRightZoneWidth() const noexcept { return rightZoneWidth; }
    bool rackDropped (const juce::String& description, int x, int y);
    int trackUnder (int x, int y) const;   // the track under a point of the track list or the arrangement (0: none)
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;
    juce::DocumentWindow* getTempoEditorWindow() noexcept { return tempoEditorWindow.get(); }
    juce::DocumentWindow* getHubWindow() noexcept { return hubWindow.get(); }
    void openSong (const juce::File&);
    void saveAsTemplate();
    // Changes not saved: asks first (Save / Don't save / Cancel), then `then`.
    void whenSongCanGo (std::function<void()> then);
    juce::DocumentWindow* getKeyCommandsWindow() noexcept { return keysWindow.get(); }
    void mouseDown (const juce::MouseEvent&) override;   // also hears clicks in the arrangement, track list and editor

    // File actions (the window's close asks about unsaved changes through these).
    void newProject();
    void openProject();
    void saveProject (std::function<void (bool saved)> then = {});
    void saveProjectAs (std::function<void (bool saved)> then = {});
    void flashSaved();          // SAVE lit, reading SAVED, for a moment
    bool isSaveLit() const { return saveButton.getButtonText() == "SAVED"; }
    ProgressBox* getProgressBox() noexcept { return progressBox.get(); }
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
    void importMidiFile (bool newSong);   // IMPORT's MIDI items: the file chooser (several files)
    void importDawProjectFile();                  // IMPORT > DAWproject: the chooser (0.9.13)
    void importDawProject (const juce::File&);    // as a new song (asks about unsaved changes); what didn't come, shown
    // IMPORT > Audio files: WAV, AIFF, FLAC, Ogg or MP3, as clips at the
    // playhead on the selected audio track (else a new one); dropped on the
    // arrangement, on the audio track under them (else a new one) at the bar.
    void importAudioFiles (bool oneTrack);
    void importAudio (const juce::Array<juce::File>&, int trackId, double atBeat, bool oneTrack);
    // Into this song: if the (first) file's tempo isn't the song's, asks whether
    // the song takes the file's tempo (0.9.14), then importMidiFilesNow.
    void importMidiFiles (const juce::Array<juce::File>&, bool newSong, double atBeat, int ontoTrackId = 0);
    void importMidiFilesNow (const juce::Array<juce::File>&, bool newSong, double atBeat, bool matchTempo, int ontoTrackId = 0);
    // The question itself (a box; tests answer it): `answer` 1 match, 2 keep, 0 cancel.
    std::function<void (const juce::File&, double fileBpm, std::function<void (int)> answer)> askMidiTempo;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int x, int y) override;
    void fileDragMove (const juce::StringArray&, int x, int y) override;
    void fileDragExit (const juce::StringArray&) override;
    // Where a dropped file lands (0.18.1): the track under (x, y) in this view's coordinates
    // (0: none) and the beat, snapped as the arrangement snaps (off: where it is).
    std::pair<int, double> dropPlace (int x, int y) const;
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
    // The chord pads (0.14.0): the PADS key, or a command of your own.
    void toggleChordPads()                     { toggleZone (padsZone); }
    void toggleChordPadsPlace()                { toggleZonePlace (padsZone); }
    void openChordPads()                       { openZone (padsZone); }
    bool isChordPadsOpen() const noexcept      { return padsZone.open; }
    bool isChordPadsInWindow() const noexcept  { return padsZone.inWindow; }
    ChordPadsView& getChordPads() noexcept     { return chordPads; }
    // F3: the mixer in its own window (closed if it's there already);
    // Alt+F3 and MIXER: docked under the arrangement (likewise).
    void toggleMixerWindow();
    void toggleMixerDocked();
    // The Inspector (the header's INSPECTOR, Ctrl+Alt+L: the manual's Show/Hide Left Zone). Kept for next time.
    void toggleInspector();
    bool isInspectorShown() const noexcept { return inspectorShown; }
    Inspector& getInspector() noexcept { return inspector; }
    // The key editor's SIZE (100, 150 or 200%; kept for next time).
    void setKeyEditorSize (int percent);
    int getKeyEditorSize() const noexcept { return juce::roundToInt (keyEditorHolder.getScale() * 100.0f); }
    juce::Component& getKeyEditorFrame() noexcept { return keyEditorHolder; }   // what's docked or in the window (the editor scaled in it)

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
    juce::TextButton& getSnapButton() noexcept { return snapButton; }
    juce::TextButton& getQuantizeButton() noexcept { return quantizeButton; }

    static constexpr int headerHeight = 52, transportHeight = 112, toolbarHeight = 32, trackListWidth = 300;
    // From this wide (spanning screens), the wide layout: the header's keys and the transport's position
    // screen in the middle, the tools on the transport (no row of their own).
    static constexpr int wideWidth = 2400;
    bool isWide() const noexcept { return getWidth() >= wideWidth; }
    ArrangeTools& getTools() noexcept { return tools; }

private:
    KeyCommands::Map keyMap;
    juce::TextButton keysButton { "KEYS" };
    int saveFlashes = 0;
    std::unique_ptr<ProgressBox> progressBox;   // a long job's progress (0.11.1)
    std::unique_ptr<juce::DocumentWindow> keysWindow, hubWindow, tempoEditorWindow, drumMapWindow, automationWindow;
    std::unique_ptr<LinesWindow> linesWindow;
    std::unique_ptr<MediaBayWindow> mediaBayWindow;
    std::unique_ptr<MediaRack> mediaRack;
    std::unique_ptr<RightZoneEdge> rightZoneEdge;
    bool rightZoneShown = false;
    int rightZoneWidth = MediaRack::defaultWidth;
    // The MIDI file being dragged in: read once when it enters (its length in beats, its tracks).
    struct DraggedMidi { juce::String path, name; double length = 0.0; int tracks = 0; std::vector<pr7s::Note> notes; };
    std::optional<DraggedMidi> draggedMidi;
    juce::TextButton hubButton { "HUB" };
    juce::TextButton padsButton { "PADS" };   // the chord pads (0.14.0), lit while open
    void registerCommands();
    void saveKeyCommands();
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
    void showQuantizeMenu();
    static juce::String quantizeName (double beats);
    void toggleZone (Zone&);        // F2 / F3: open or closed, where it was
    void toggleZonePlace (Zone&);   // Alt+F2 / Alt+F3: docked or full screen

    pr7s::Studio& studio;
    Timeline timeline;
    TransportBar transport;
    TrackList trackList;
    ArrangeView arrange;
    ArrangeTools tools { arrange };
    juce::TextButton autoScrollKey { "AUTO-SCROLL" }, autoScrollMenu;   // the arrangement's Auto-Scroll, on the transport
    void toggleArrangeAutoScroll();
    KeyEditor keyEditor;
    ScaledHolder keyEditorHolder { keyEditor };   // the editor at its SIZE (0.9.3: 200% unless changed)
    MixerView mixer;
    ChordPadsView chordPads;          // the chord pads zone (0.14.0)
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
                     pluginsButton { "PLUGINS" }, settingsButton { "SETTINGS" }, readAllButton { "R" }, writeAllButton { "W" },
                     snapButton { "SNAP" }, quantizeButton { "1/16" };   // (0.9.10)
    // After the editor and the mixer: their windows go first.
    Zone editorZone { keyEditorHolder, editorButton, "MIDI Editor", "keyEditor", false, false, {} },
         mixerZone { mixer, mixerButton, "Mixer", "mixer", false, false, {} },
         padsZone { chordPads, padsButton, "Chord Pads", "chordPads", false, false, {} };
    std::array<Zone*, 3> zones() noexcept { return { &editorZone, &mixerZone, &padsZone }; }
    std::map<int, std::unique_ptr<Windows::Instrument>> instrumentWindows;
    std::map<juce::AudioProcessor*, std::unique_ptr<Windows::Instrument>> effectWindows;
    std::unique_ptr<Windows::Settings> settingsWindow;
    std::unique_ptr<Windows::Plugins> pluginsWindow;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<Windows::ExportPage> exportPage;
    juce::Component::SafePointer<Windows::RenderPage> renderPage;
    void render (const std::vector<pr7s::Studio::RenderSource>&, const pr7s::Studio::RenderOptions&);
    void chooseExportFile (const pr7s::Studio::ExportOptions&);
    void writeExport (const juce::File&, const pr7s::Studio::ExportOptions&);
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

// The right zone's left edge: drag it to make the Media rack wider or narrower.
class MainView::RightZoneEdge final : public juce::Component,
                                     public juce::SettableTooltipClient
{
public:
    explicit RightZoneEdge (MainView&);
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
