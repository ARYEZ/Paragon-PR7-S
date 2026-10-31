#pragma once

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

    enum class Tool { select, draw, erase, split };
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
    void quantizeSelected();   // all the notes, with none selected
    // SPLIT (3, as the manual's): the note at `beat` (song beats) cut in two
    // there; if it's one of several selected, all of those it crosses.
    // One edit. False if nothing was cut.
    bool splitAt (int noteIndex, double beat);

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

    static constexpr int sideWidth = 302, keysWidth = 54, rulerHeight = 26, velocityHeight = 86, scrollBarSize = 12;
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
    enum class Gesture { none, position, audition, band, move, resize, draw, erase, velocity };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void refresh();
    void clampScroll();
    void refreshScrollBars();
    void refreshPanel();
    const std::vector<pr7s::Note>& notes() const;   // the working copy while a gesture edits them
    bool editing() const noexcept;
    void begin();
    void commit (std::vector<pr7s::Note>);
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
    juce::MouseCursor penCursor, scissorsCursor;   // DRAW's and SPLIT's pointers (made once: not statics, they mustn't outlive JUCE)   // where it was last drawn (0.9.2: only its strip is redrawn as it moves)
    juce::ScrollBar across { false }, down { true };

    // The gesture under way.
    Gesture gesture = Gesture::none;
    std::vector<pr7s::Note> working, original;
    std::vector<int> dragging, selectionBefore;
    int anchor = -1, clicked = -1;
    bool moved = false;
    juce::Point<float> downAt;
    juce::Rectangle<float> band;
    float lastVelocityX = 0.0f, shownVelocity = -1.0f;
    int auditioning = -1;

    juce::ComboBox sizeBox;
    juce::TextButton selectTool { "SELECT" }, drawTool { "DRAW" }, eraseTool { "ERASE" }, splitTool { "SPLIT" }, quantizeButton { "QUANTIZE" },
                     windowButton { "WINDOW" }, closeButton { "CLOSE" }, autoScrollButton { "AUTO-SCROLL" }, autoScrollMenu;
    bool inWindow = false;
    juce::ComboBox gridBox;
};
