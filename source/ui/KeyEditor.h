#pragma once

#include "ui/InfoEntry.h"
#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"
#include "ui/ArrangeView.h"

// (0.9.3: SPLIT (3) cuts notes; DRAW and SPLIT show a pencil and scissors as
// the pointer; the grid goes to 1/1 and 1/2.)
//
// The key editor (piano roll), below the arrangement, inspired by Cubase's
// key editor in its lower zone, in our look. It shows the part last
// selected in the arrangement (double-click a part to open it).
//
//   left panel     the part's name; the tools SELECT (1), DRAW (8) and
//                  ERASE (5); the GRID; QUANTIZE (Q); the selected notes
//   keys           a piano down the side: click to hear a note; keys light
//                  while a note sounds
//   notes          SELECT: click a note to select it (Shift or Ctrl adds or
//                  removes), drag to move it (in time and pitch), Alt-drag
//                  to copy, drag its right end to lengthen or shorten it,
//                  drag across empty space to select a group, double-click
//                  empty space for a new note, double-click a note to
//                  delete it
//                  DRAW: click for a new note a grid step long (drag right
//                  to make it longer); click a note to remove it
//                  ERASE: click or drag across notes to remove them
//                  Everything snaps to the GRID; hold Ctrl while dragging
//                  for no snapping (Ctrl+Shift while moving notes: Ctrl
//                  alone keeps them to one direction, in time or in pitch)
//   velocity       a bar per note, as tall as it's played: click or drag
//                  across the bars to set them (only the selected ones, if
//                  any of those are under the mouse)
//   ruler          click to put the position there
//   keys (focus)   Delete removes the selected notes, Up / Down move them a
//                  semitone (Shift: an octave), Ctrl+A or Shift+A selects all,
//                  Q quantizes, Ctrl+C / Ctrl+X copy / cut the selected notes,
//                  Ctrl+V pastes them at the playhead (the part grows to
//                  hold them if they go past it), Ctrl+D duplicates the
//                  selected notes straight after themselves
//                  H / G zoom in / out a step, centred on the playhead;
//                  Shift+H / Shift+G taller or shorter rows; Shift+F the
//                  whole part; F turns Auto-Scroll on or off (AutoScroll)
//   wheel          scrolls the notes up and down; Shift+wheel along the song;
//                  Ctrl+wheel zooms (and the scroll bars, under and beside the notes)
//
// Each gesture is one edit (one undo step): while dragging the editor works
// on its own copy of the notes, and gives the studio the result on release.
class KeyEditor final : public juce::Component,
                        private juce::ChangeListener,
                        private juce::ScrollBar::Listener,
                        private juce::Timer
{
public:
    KeyEditor (pr7s::Studio&, ArrangeView&);
    ~KeyEditor() override;

    enum class Tool { select, draw, erase, split, line, mute };
    void setTool (Tool);
    Tool getTool() const noexcept { return tool; }

    // The grids offered, in beats (a beat is a quarter note).
    struct GridChoice { const char* name; double beats; };
    static constexpr GridChoice grids[] = { { "1/1", 4.0 }, { "1/2", 2.0 },   // (0.9.3: whole and half notes, the owner asked)
                                            { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 }, { "1/32", 0.125 },
                                            { "1/8 triplet", 1.0 / 3.0 }, { "1/16 triplet", 1.0 / 6.0 } };
    void setGrid (double beats);
    double getGrid() const noexcept { return grid; }

    // The part shown (none until one is selected in the arrangement, or if it's gone).
    const pr7s::Part* getPart() const;
    int getTrackId() const;

    // The selected notes, as indices into the part's notes (sorted).
    const std::vector<int>& getSelectedNotes() const noexcept { return selected; }
    void selectNotes (std::vector<int>);
    void selectAll();
    void deleteSelected();
    void transposeSelected (int semitones);
    static juce::String quantizeText (double beats);   // "1/16"
    void quantizeSelected();   // all the notes, with none selected; to the song's quantize value (the header's: 0.31.0)
    // SPLIT (3, as the manual's): the note at `beat` (song beats) cut in two
    // there; if it's one of several selected, all of those it crosses.
    // One edit. False if nothing was cut.
    bool splitAt (int noteIndex, double beat);
    // Random velocities (0.9.4, the owner's idea): the selected notes' velocities
    // set at random within a range: LOW (Alt+B), MID (Alt+N) or HIGH (Alt+M).
    // The ranges are set with the arrow beside the keys (kept for next time;
    // at first 40-70, 70-100, 100-127). Nothing selected: nothing changes.
    // One edit. False if nothing was selected.
    static constexpr int numVelocityRanges = 3;
    bool randomizeVelocities (int range);
    void setVelocityRange (int range, int low, int high);   // 1 to 127 (swapped if the wrong way round)
    std::pair<int, int> getVelocityRange (int range) const;
    juce::TextButton& getVelocityKey (int range) { return velocityKeys[(size_t) range]; }

    // Humanize (0.20.0, the owner asked): the selected notes loosened (pr7s::humanize: timing in
    // ms, velocity inside its layer when HUMANIZE's "keep layers" is on, length; chords together;
    // a bar's first beat moves least). The amount: Studio::getHumanizeAmount unless given. One edit;
    // the selection kept (press again to loosen further, Ctrl+Z to take it back). False if nothing
    // was selected.
    bool humanizeSelected (float amount = -1.0f);
    void showHumanizeMenu();
    juce::TextButton& getHumanizeKey() { return humanizeKey; }

    // The editor's clipboard: notes kept with their timing relative to one
    // another, so they paste into this part or any other.
    void copySelected();
    void cutSelected();
    void paste();   // the pasted notes are selected afterwards
    bool canPaste() const noexcept { return ! clipboard.empty(); }
    void duplicateSelected();   // the copies are selected afterwards

    // The editor's keys (the main window passes them on while the editor was last clicked).
    bool keyPressed (const juce::KeyPress&) override;

    std::function<void()> onClose;

    // SIZE (0.9.3): the whole editor drawn at 100, 150 or 200% (the main
    // window scales it; the owner found it hard to read at 100).
    std::function<void (int percent)> onSizeChange;
    void setSizeShown (int percent);

    // WINDOW: into a window of its own (to maximize, or stretch across
    // screens); DOCK: back under the arrangement. The main window moves it.
    std::function<void()> onWindowToggle;
    void setInWindow (bool);   // the button's name follows
    bool isInWindow() const noexcept { return inWindow; }

    // Zooms and scrolls so the whole part fits (after it moves to or from its window; Shift+F).
    void fitToPart();
    // H / G: one zoom step in or out, centred on the playhead.
    void zoom (bool in);
    // Auto-Scroll, the editor's own (F, or AUTO-SCROLL; its settings: the
    // arrow beside the key); on at first.
    void setAutoScroll (bool);   // (and no longer suspended)
    bool getAutoScroll() const noexcept { return autoScroll.on; }
    AutoScroll& getAutoScrollSettings() noexcept { return autoScroll; }
    void autoScrollChanged();   // the key follows; the main window keeps the settings
    std::function<void()> onAutoScrollChange;
    // Shift+H / Shift+G (Ctrl+Shift+wheel): the note rows a step taller or
    // shorter (from 6 to 36 pixels, our choice), the pitch in the middle staying put.
    void zoomRows (bool in);
    float getRowHeight() const noexcept { return rowHeight; }
    // Turns the page if the playhead is out of sight (playing, or moved while stopped). The timer calls it.
    void followPlayhead();
    double getPixelsPerBeat() const noexcept { return pixelsPerBeat; }
    double getFirstBeat() const noexcept { return firstBeat; }
    juce::ScrollBar& getScrollBarAcross() noexcept { return across; }
    juce::ScrollBar& getScrollBarDown() noexcept { return down; }

    // Where things are drawn, in the editor's coordinates (for tests).
    juce::Rectangle<float> getNoteBounds (int index) const;
    juce::Rectangle<float> getNotesArea() const;
    juce::Rectangle<float> getVelocityArea() const;
    float xForBeat (double songBeat) const noexcept;
    double beatForX (float x) const noexcept;
    float yForPitch (int pitch) const noexcept;   // the top of its row
    int pitchForY (float y) const noexcept;

    // The velocity handles (0.9.10, the manual's Controller Event Editor):
    // with two or more notes selected, a box round their velocity bars.
    // Drag its top edge to move them all up or down, the middle of the top to
    // scale them (from 0), a top corner to tilt that end, the middle of the
    // right edge to squash them towards their middle or spread them. One undo
    // step per drag.
    enum class VelocityHandle { none, move, scale, tiltLeft, tiltRight, scaleCentre };
    juce::Rectangle<float> getVelocityBox() const;   // empty: fewer than two notes selected
    VelocityHandle velocityHandleAt (juce::Point<float>) const;

    // Controller lanes (0.25.0, the manual's Controller Display): under the notes, one or more
    // lanes, each showing one kind (ControllerKinds' codes: velocity, a CC, pitchbend,
    // aftertouch, program change, poly pressure); the track keeps which (Track::controllerLanes).
    //   header      the lane's name and its value at the playhead; click: the lane's menu (what
    //               it shows, Select All, Remove This Lane, Create Controller Lane, Show Used
    //               Controllers, Show Velocity Lane Only, the type of new events (ramp or step),
    //               the selected events' curve, cut/copy/paste, pitchbend's semitone grid)
    //   SELECT      click an event (Shift/Ctrl: add or remove), drag it (in time and value),
    //               drag across empty space to select a group (two or more: the handles to tilt,
    //               scale or move them, as the velocities'); Alt-drag draws; double-click an
    //               event to type its value (and where it is, its curve); double-click empty
    //               space for a new event
    //   DRAW        click for an event on the grid step, drag to draw a curve
    //   ERASE       click or drag across events to remove them
    //   LINE        drag a line (and click LINE again for its modes: Line, Parabola (Ctrl: the
    //               other way), Sine, Triangle, Square (Ctrl: the other phase; one period a grid
    //               step), Paint); on the velocity lane it sets the notes' velocities along it
    //   divider     drag the line above the lanes to make them taller or shorter
    enum class LineMode { line, parabola, sine, triangle, square, paint };
    static juce::String lineModeName (LineMode);
    void setLineMode (LineMode);
    LineMode getLineMode() const noexcept { return lineMode; }
    void showLineModeMenu();
    const std::vector<int>& getLanes() const;          // the shown part's track's (velocity alone without one)
    int getNumLanes() const { return (int) getLanes().size(); }
    juce::Rectangle<float> getLaneArea (int lane) const;     // its events' area (right of its header)
    juce::Rectangle<float> getLaneHeader (int lane) const;
    int laneAt (juce::Point<float>) const;              // -1: not in a lane
    int laneOf (int code) const;                        // -1: not shown
    void setLane (int lane, int code);                  // what a lane shows (a kind shown already: nothing)
    void addLane (int code);                            // a lane at the bottom (shown already: nothing)
    void removeLane (int lane);
    void showUsedControllers();                         // a lane for each kind the part has events of
    void showVelocityLaneOnly();
    void toggleLanesShown();                            // hide them all / bring back the last ones
    void setLaneHeight (int);                           // 40 to 400 px each (as the divider)
    void showLaneMenu (int lane);
    void showLanesMenu();                               // LANES in the panel
    float yForValue (int lane, int value) const;
    int valueForY (int lane, float y) const;            // (pitchbend: snapped to its semitones with SNAP on)

    // The controller events (the part's, by index into Part::controllers, sorted).
    const std::vector<pr7s::Controller>& controllers() const;   // the working copy while a gesture edits them
    const std::vector<int>& getSelectedControllers() const noexcept { return selectedControllers; }
    void selectControllers (std::vector<int>);
    void selectAllOnLane (int lane);
    juce::Point<float> getControllerPoint (int index) const;     // where it's drawn (its lane's)
    int controllerAt (int lane, juce::Point<float>) const;        // -1: none
    // A new event (as a click with DRAW): at `songBeat` on the grid step (exactly, if `free`),
    // replacing one of its kind on that step. One edit. False outside the part.
    bool addControllerAt (int lane, double songBeat, int value, bool free = false);
    void deleteSelectedControllers();
    void setSelectedControllersRamp (bool ramp);
    // The LINE tool's result from one point to another in a lane (Ctrl: `reverse`). One edit.
    void drawLine (int lane, juce::Point<float> from, juce::Point<float> to, bool reverse);
    // Typing (double-click an event): the value as the lane shows it ("64", pitchbend "+4096" or
    // "2st" semitones, pan "L12"/"C"/"R12", sustain "ON"/"OFF"), and for one event where it is
    // ("bar.beat.16th.tick"; empty: left) and its curve. All selected get the value. False if not one.
    bool enterControllerValue (const juce::String& value, const juce::String& position, std::optional<bool> ramp);
    std::optional<int> parseControllerValue (int code, const juce::String&) const;
    void showControllerEntry (int index);
    void copySelectedControllers();
    void pasteControllers (bool atOrigin = false);       // onto the lane last clicked (at the playhead, or where they were)
    bool areLanesFocused() const noexcept { return lanesFocused; }
    void setLanesFocused (bool focused) noexcept { lanesFocused = focused; }
    // The handles for two or more selected events (the manual's Controller Event Editor, as the velocities').
    juce::Rectangle<float> getControllerBox() const;
    VelocityHandle controllerHandleAt (juce::Point<float>) const;

    // The drum editor (0.26.0, the manual's Drum Editor): on a track with a drum map (DRUMS in the
    // panel, on by itself when a map is chosen), the piano gives way to the drum sound list: each
    // drum's pitch and name (from the map), M to mute it, a click to hear it, a double-click to
    // rename it, a right-click for its notes (I-note, O-note, channel), the sounds shown (named,
    // all 128, or only those with notes). One row per drum, in the map's order; notes are drawn
    // as diamonds (LENGTHS: as boxes as long as they are). DRAW is the drumstick: a click puts
    // a hit on the grid step, a drag right a hit on every step. MAP chooses the track's map
    // (GM Map built in, the song's, those in the drum map folder), loads a .drm, saves one, and
    // opens the Drum Map Setup. With a map the piano roll names the drums too.
    enum class DrumRowsShown { named, all, withEvents };
    bool isDrumView() const noexcept { return drumViewOn; }
    void setDrumView (bool);
    const std::vector<int>& getDrumRows() const noexcept { return drumRows; }   // pitches, top to bottom
    void setDrumRowsShown (DrumRowsShown);
    DrumRowsShown getDrumRowsShown() const noexcept { return drumRowsShown; }
    void setShowDrumLengths (bool);
    bool getShowDrumLengths() const noexcept { return showDrumLengths; }
    juce::String drumNameOf (int pitch) const;          // the map's name for it ("" without a map)
    void toggleDrumMute (int pitch);
    void renameDrumSound (int pitch, const juce::String&);
    void selectNotesOfSound (int pitch);
    void showDrumMapMenu();
    void showDrumSoundMenu (int pitch);
    juce::Rectangle<float> getDrumMuteBox (int pitch) const;   // the M in its row
    int listWidth() const noexcept { return drumViewOn ? drumListWidth : keysWidth; }
    std::function<void()> onDrumMapSetup;                // MainView: the Drum Map Setup window
    std::function<void (int trackId)> onOpenInstrument;  // (0.31.0) INSTRUMENT: MainView opens the track's instrument window
    juce::TextButton& getInstrumentKey() noexcept { return instrumentKey; }
    juce::TextButton& getQuantizeKey() noexcept { return quantizeButton; }
    std::function<void (std::function<void (const juce::File&)>)> chooseDrumMapFile;   // (tests answer it; else a FileChooser)

    static constexpr int sideWidth = 150, keysWidth = 54, rulerHeight = 26, velocityHeight = 86, scrollBarSize = 12, drumListWidth = 220;
    // (0.38.0) The info line over the ruler: the first selected note's START, END, LENGTH, PITCH and
    // VELOCITY (the earliest; several: in pink with how many). Double-click a value to type it: every
    // selected note changes by what the first one changes (D#-1 typed as D-2: all 13 semitones down);
    // Ctrl held as Enter goes in: all set to it (the manual's Value Editing Rules on the Info Line).
    static constexpr int infoHeight = 26;
    float notesTop() const noexcept { return (float) (infoHeight + rulerHeight); }
    juce::String getInfoField (const juce::String& name) const;   // "START", "END", "LENGTH", "PITCH", "VELOCITY", "CHORD" (0.42.0: the selection's chord); "" with none selected
    bool enterInfo (const juce::String& field, const juce::String& text, bool absolute);   // false: not a value it takes
    static std::optional<int> parseNoteName (const juce::String&);   // "D#-1", "Eb3", "c4", or a MIDI number (middle C = C3)
    static juce::String lengthText (double beats, const pr7s::Project&);   // bars.beats.16ths.ticks, from 0 ("0.1.0.0")
    static std::optional<double> parseLength (const juce::String&, const pr7s::Project&);
    static constexpr int minimumHeight = 230;   // docked: the panel's keys and a few octaves
    static constexpr float minRowHeight = 6.0f, maxRowHeight = 36.0f;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::uint64 seenChange = 0;   // (0.57.1) the studio's change version last acted on
    enum class Gesture { none, position, audition, band, muteBand, move, resize, resizeStart, draw, erase, velocity, velocityHandle,
                         controllerDraw, controllerErase, controllerBand, controllerMove, controllerHandle, controllerLine, laneResize };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void refresh();
    void clampScroll();
    void refreshScrollBars();
    void refreshPanel();
    void paintInfo (juce::Graphics&, juce::Rectangle<float>);
    void paintInfoLine (juce::Graphics&);
    int firstSelectedNote() const;                         // the earliest selected (ties: the lowest), -1: none
    std::vector<std::pair<juce::String, juce::Rectangle<float>>> infoAreas() const;
    void beginInfoEdit (const juce::String& field);
    std::unique_ptr<juce::TextEditor> infoEditor;
    juce::String infoEditing;
public:
    juce::Colour velocityColour (float velocity) const;   // a note's colour by its velocity (0.9.12)
private:   // the selected notes, at the column's foot
    void layoutColumn();
    // The panel's keys in one column, all one size (0.9.10, the owner asked),
    // in a strip that scrolls when the editor is shorter than they are.
    struct InfoView final : juce::Component
    {
        explicit InfoView (KeyEditor& e) : owner (e) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override { owner.paintInfo (g, getLocalBounds().toFloat()); }
        KeyEditor& owner;
    };
    juce::Viewport panelView;
    juce::Component panelColumn;
    InfoView infoView { *this };
    static constexpr int columnKeyHeight = 24, columnGap = 4, panelHeaderHeight = 54;
    const std::vector<pr7s::Note>& notes() const;   // the working copy while a gesture edits them
    bool editing() const noexcept;
    void begin();
    void commit (std::vector<pr7s::Note>);
public:
    void setNotesMuted (const std::vector<int>& indices, bool muted);   // (0.77.1) the MUTE tool's work: one undo step
private:
    int noteAt (juce::Point<float>) const;
    double snapped (double songBeat, bool free) const noexcept;
    void eraseAt (juce::Point<float>);
    void setVelocities (float fromX, float toX, float y);
    void startAudition (int pitch, float velocity);
    void stopAudition();
    juce::Rectangle<float> rulerArea() const;
    juce::Rectangle<float> keysArea() const;
    void paintPanel (juce::Graphics&);
    void paintKeys (juce::Graphics&);
    void paintNotes (juce::Graphics&);
    void paintVelocities (juce::Graphics&);
    void paintRuler (juce::Graphics&);
    // The lanes (KeyEditorLanes.cpp).
    float laneHeightNow() const;
    float lanesTop() const;
    bool laneMouseDown (const juce::MouseEvent&);
    void laneMouseDrag (const juce::MouseEvent&);
    void laneMouseUp();
    bool laneDoubleClick (const juce::MouseEvent&);
    bool laneMouseMove (const juce::MouseEvent&);
    void paintLanes (juce::Graphics&);
    void paintControllerLane (juce::Graphics&, int lane);
    void paintLaneHeader (juce::Graphics&, int lane);
    bool editingControllers() const noexcept;
    void beginControllers();
    void commitControllers (std::vector<pr7s::Controller>, const std::vector<bool>& selectedFlags = {});
    void drawControllersTo (juce::Point<float>);
    void eraseControllersTo (juce::Point<float>);
    void dragControllerHandle (float dy);
    void applyVelocityLine (juce::Point<float> from, juce::Point<float> to, bool reverse);
    double lineValueAt (double t, double v1, double v2, double beatInto, bool reverse) const;
    int snapBend (int value) const;
    std::vector<pr7s::Controller> workingControllers, originalControllers, knownControllers, controllerClipboard;
    int clipboardCode = 0;
    double clipboardOrigin = 0.0;                    // the copied events' first beat in the song (Paste at Origin)
    std::vector<int> selectedControllers, controllersBefore, draggingControllers;
    int activeLane = -1, anchorController = -1;
    bool lanesFocused = false;
    int laneHeight = velocityHeight;
    std::vector<int> hiddenLanes;                    // what Show/Hide brings back
    LineMode lineMode = LineMode::line;
    juce::Point<float> lineFrom, lineTo, lastDrawPoint;
    float heightAtDown = 0.0f, lastLanesTop = -1.0f;
    // The drum view.
    bool drumViewOn = false, showDrumLengths = false, refitAfterDrumRows = false;
    DrumRowsShown drumRowsShown = DrumRowsShown::named;
    std::vector<int> drumRows;
    std::array<int, 128> drumRowOf {};
    float pianoRowHeight = 12.0f;
    double drumDrawStart = 0.0;
    int auditionKey = -1;
    void refreshDrumRows();
    int numRows() const noexcept { return drumViewOn ? (int) drumRows.size() : 128; }
    // The furthest left the view goes: the song's start (drums: a diamond's width before it, so a hit on beat 1 shows whole).
    double earliestFirstBeat() const noexcept { return drumViewOn ? -(double) rowHeight / pixelsPerBeat : 0.0; }
    int rowForY (float y) const noexcept;
    int pitchMovedBy (int pitch, int rows) const noexcept;   // a row (drums) or a semitone (piano) away
    void paintDrumList (juce::Graphics&);
    juce::TextButton drumKey { "DRUMS" }, mapKey { "MAP" }, lengthsKey { "LENGTHS" };
    std::unique_ptr<juce::FileChooser> drumChooser;
    juce::TextButton lineTool { "LINE" }, lanesKey { "LANES" };

    pr7s::Studio& studio;
    ArrangeView& arrange;
    int partId = 0;                          // the part shown (0: none yet)
    int lastArrangeTrack = 0, lastArrangeIndex = -1;
    std::vector<pr7s::Note> knownNotes;      // its notes as last seen, to notice changes from elsewhere (undo)
    std::vector<int> selected;
    std::vector<pr7s::Note> clipboard;        // starts from the first note copied (0)
    void insertNotes (const std::vector<pr7s::Note>&, double atSongBeat);   // the part grows to hold them

    Tool tool = Tool::select;
    double grid = 0.25;
    float newVelocity = 100.0f / 127.0f;      // a new note's velocity (100, as in most DAWs)

    // The view: zoom and scroll, of its own (independent of the arrangement's).
    double pixelsPerBeat = 80.0, firstBeat = 0.0;
    float scrollY = 0.0f;
    float rowHeight = 12.0f;   // a note row (the vertical zoom)
    AutoScroll autoScroll;
    double lastPosition = 0.0;   // the playhead as last seen (a jump while stopped is followed)
    float playheadX = -1.0e6f;
    // The playhead as drawn (0.17.5, the owner asked: the arrangement's glide here too).
    double shownBeat = 0.0;
    bool gliding = false;
public:
    void advancePlayhead();   // one frame of the glide (the timer's; tests call it)
    double getShownPlayhead() const noexcept { return shownBeat; }
private:
    juce::MouseCursor penCursor, scissorsCursor;   // DRAW's and SPLIT's pointers (made once: not statics, they mustn't outlive JUCE)   // where it was last drawn (0.9.2: only its strip is redrawn as it moves)
    juce::ScrollBar across { false }, down { true };

    // The gesture under way.
    Gesture gesture = Gesture::none;
    VelocityHandle handle = VelocityHandle::none;
    juce::Rectangle<float> boxAtDown;
    void dragVelocityHandle (float dy);
    std::vector<pr7s::Note> working, original;
    std::vector<int> dragging, selectionBefore;
    int anchor = -1, clicked = -1;
    bool moved = false;
    juce::Point<float> downAt;
    juce::Rectangle<float> band;
    float lastVelocityX = 0.0f, shownVelocity = -1.0f;
    int auditioning = -1;

    juce::ComboBox sizeBox;
    std::array<juce::TextButton, 3> velocityKeys;
    juce::TextButton velocityRangesKey;
    juce::TextButton humanizeKey, humanizeMenuKey { "HUMANIZE SET" };
    void refreshHumanizeKey();
    std::array<std::pair<int, int>, 3> velocityRanges { { { 40, 70 }, { 70, 100 }, { 100, 127 } } };
    juce::Random random;
    void refreshVelocityKeys();
    void showVelocityRanges();
    juce::TextButton selectTool { "SELECT" }, drawTool { "DRAW" }, eraseTool { "ERASE" }, muteTool { "MUTE" }, splitTool { "SPLIT" }, quantizeButton { "QUANTIZE" },
                     windowButton { "WINDOW" }, closeButton { "CLOSE" }, autoScrollButton { "AUTO-SCROLL" }, autoScrollMenu,
                     instrumentKey { "INSTRUMENT" };   // (0.31.0) the track's instrument window
    bool inWindow = false;
    juce::ComboBox gridBox;
};
