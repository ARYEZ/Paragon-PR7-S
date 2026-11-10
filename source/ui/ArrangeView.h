#pragma once

#include "ui/ChordEventEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// Auto-Scroll, as the manual has it (the arrangement and the key editor
// each have their own): on or off (F, or AUTO-SCROLL), and its settings menu
// (the arrow beside the key): Page Scroll (the page turns when the playhead
// reaches the right edge) or Stationary Cursor (the playhead stays in the
// middle while playing, the song moving under it); Suspend Auto-Scroll when
// Editing (a click in the view while playing stops it following until
// playback stops or Auto-Scroll is clicked again; the key lights pink and
// reads SUSPENDED meanwhile).
struct AutoScroll
{
    bool on = true, stationary = false, suspendWhenEditing = false;
    bool suspended = false;   // (not kept: only until playback stops)

    // Where the view should start now (its first beat), if it should move:
    // playing, or the playhead moved while stopped (`jumped`) out of sight.
    std::optional<double> follow (double position, double firstBeat, double pixelsPerBeat, float width,
                                  bool playing, bool jumped) const;
    // F or a click on the key: a suspended one carries on; else on or off.
    void toggle() noexcept { suspended ? suspended = false : (on = ! on); }
    // The key: lit while on, pink and SUSPENDED while suspended.
    void refreshKey (juce::TextButton&) const;
    // The settings menu, under `target`; `changed` after a choice.
    void showMenu (juce::Component& target, std::function<void()> changed);
    // Kept in the settings file under `prefix` + "AutoScroll", "Stationary", "SuspendWhenEditing".
    void load (juce::PropertiesFile*, const juce::String& prefix);
    void save (juce::PropertiesFile*, const juce::String& prefix) const;
};

// Where the arrangement is scrolled and zoomed, shared by the track list
// (which must line up with the lanes) and the arrangement.
struct Timeline
{
    double pixelsPerBeat = 28.0;   // zoom
    double firstBeat = 0.0;        // the beat at the arrangement's left edge
    int scrollY = 0;               // pixels the tracks are scrolled up by
    AutoScroll autoScroll;         // the arrangement's (F)
    std::function<void()> onAutoScrollChange;   // suspended by a click, or back when playback stops: the key follows

    // The vertical zoom (Shift+H in, Shift+G out, Ctrl+Shift+wheel): every
    // row's height scaled together, so their differences stay (as the manual
    // has it), from half to three times (our choice). Lanes and the master's
    // row stay tall enough for their controls.
    double rowZoom = 1.0;
    static constexpr double rulerTickRoom = 5.0;   // (px between note ticks on the ruler for them to show: 0.17.3)
    static constexpr double minRowZoom = 0.5, maxRowZoom = 3.0, rowZoomStep = 1.25;
    int trackRowHeight() const noexcept { return juce::roundToInt (trackHeight * rowZoom); }
    int laneRowHeight() const noexcept { return juce::jmax (34, juce::roundToInt (laneHeight * rowZoom)); }
    int masterRowHeight() const noexcept { return juce::jmax (34, juce::roundToInt (masterHeight * rowZoom)); }

    // How far along the scroll bar goes: 8 bars past the last part (or the
    // cycle's end, or the playhead), and at least 32 bars (our choice: there
    // is no project length to set yet).
    static double scrollLength (pr7s::Studio&);
    // One step of G (out) or H (in), as the manual's Zoom Out / Zoom In.
    static constexpr double zoomStep = 1.5;

    static constexpr int trackHeight = 72;   // a track's row at the vertical zoom's 1 (trackRowHeight: as it is now)
    static constexpr int rulerHeight = 36;
    static constexpr int cycleStripHeight = 14;   // the ruler's top strip, where the cycle is dragged
    static constexpr int bottomRoom = 40;         // under the last track: the track list's + ADD TRACK, so it never hides a track

    float xForBeat (double beat) const noexcept { return (float) ((beat - firstBeat) * pixelsPerBeat); }
    double beatForX (float x) const noexcept { return firstBeat + x / pixelsPerBeat; }

    // The grid (0.9.10, the manual's Snap with Grid Type "Use Quantize"): the
    // song's quantize value (1/1 to 1/64, the toolbar's), a step for the
    // locators; `snapGrid` what parts, ranges, points and the position snap
    // to: the grid with Snap on (J), else nothing.
    double grid (const pr7s::Project& project) const noexcept
    {
        return project.quantizeBeats > 0.0 ? project.quantizeBeats : 0.25;
    }
    double snapGrid (const pr7s::Project& project) const noexcept
    {
        return project.snapOn ? grid (project) : 0.0;
    }

    std::function<void()> onScrollOrZoom;   // the track list follows

    // The rows, top to bottom, shared by the track list and the arrangement:
    // each track, its open automation lanes under it, then the master's row
    // and its lanes. `top` is from the first row's top, before scrolling.
    static constexpr int laneHeight = 48, masterHeight = 40, tempoHeight = 72;   // at the vertical zoom's 1
    int tempoRowHeight() const noexcept { return juce::jmax (48, juce::roundToInt (tempoHeight * rowZoom)); }
    static constexpr int chordsHeight = 60;   // the chord track (0.16.0): chords above, the scales below
    int chordRowHeight() const noexcept { return juce::jmax (46, juce::roundToInt (chordsHeight * rowZoom)); }
    static constexpr int signatureHeight = 34;   // the signature track (0.45.0)
    int signatureRowHeight() const noexcept { return juce::jmax (28, juce::roundToInt (signatureHeight * rowZoom)); }
    static constexpr int markerHeight = 40;   // the marker track (0.49.0): cycle markers above, position markers below
    int markerRowHeight() const noexcept { return juce::jmax (32, juce::roundToInt (markerHeight * rowZoom)); }
    static constexpr int takeHeight = 52;   // a lane of takes (0.29.0): smaller than its track's row (our choice)
    int takeRowHeight() const noexcept { return juce::jmax (36, juce::roundToInt (takeHeight * rowZoom)); }
    struct Row
    {
        // (0.29.0) take: one of a track's lanes of takes (Show Lanes), under its row.
        enum class Kind { track, lane, master, tempo, chords, take, signature, markers };   // (0.45.0: the signature track, under the tempo track; 0.49.0: the marker track, under it)
        Kind kind = Kind::track;
        int busId = 0;          // a track's id, or the master's (0)
        int laneIndex = -1;     // a lane: its index in the bus's automation; a take lane: its lane
        int top = 0, height = 0;
        bool operator== (const Row&) const = default;
    };
    std::vector<Row> layout (const pr7s::Studio&) const;
    int contentHeight (const pr7s::Studio&) const;
};

// The arrangement (inspired by Cubase's project window, in our look): the ruler with
// bar numbers and the cycle range on top, a lane for each track, its parts
// as coloured blocks showing their notes, and the playhead.
//
//   ruler           click to put the position there; drag across its top
//                   strip to set the cycle (the locators; CYCLE turns on);
//                   in the top strip, click inside the range to turn CYCLE
//                   on or off, drag inside it to move it, drag a locator's
//                   handle (either end) to move just that one; Ctrl-click
//                   sets the left locator there, Alt-click the right, Ctrl+
//                   Alt-click both round the grid step clicked in (the manual's)
//   a part          click to select; drag to move (to another track of its
//                   kind too); Ctrl-drag keeps to one direction; Alt-drag to
//                   copy; drag its right edge to lengthen or shorten it;
//                   double-click to edit its notes (the key editor);
//                   right-click for Edit notes, Duplicate and Delete.
//                   Ctrl turns snapping off while resizing, trimming and
//                   fading (Ctrl+Shift while moving)
//   an audio clip   its waveform; drag its left edge to trim its start (the
//                   audio stays where it is), its right edge to trim its end;
//                   drag the handles at its top corners to fade in and out
//   an empty lane   click to select the track; double-click for a new
//                   empty part: from the left locator to the right if it's
//                   between them, else a bar long
//   an automation   (as the manual has it) a click adds a point there (and
//   lane            turns R on; one on the line between two points adds
//                   nothing); Alt-drag draws several; drag from an empty
//                   spot to select points in a rectangle, Shift-click to add
//                   or remove one; drag selected points to move them (Ctrl:
//                   one direction only; Ctrl and another key: no snapping),
//                   Alt-drag to copy them; Delete removes them; Esc while
//                   dragging puts them back; right-click: Select All Events.
//                   With no points, a dotted line shows the control's
//                   setting; R off or the lane muted, the curve is greyed
//   RANGE (2)       drag across the tracks for a range (a stretch of time on
//                   the tracks dragged over), shaded; double-click a part for
//                   one over it, or between parts for the gap; drag its edges
//                   (Shift-drag outside it extends it); drag inside it to
//                   move what's in it (Alt: a copy); right-click it for its
//                   menu. Keys: Shift+X split at its edges, Delete (Shift+
//                   Backspace: Delete Time, closing the gap), Ctrl+X / C / V
//                   (Ctrl+Shift+X Cut Time, Ctrl+Shift+V Paste Time; paste at
//                   the playhead on the selected track down), Ctrl+D
//                   duplicate, Ctrl+Shift+E Insert Silence, Esc clears it
//   tools           (the manual's, keys 1 to 9; F9 / F10 the previous
//                   / next) what a click on a track does: SELECT (1, all of
//                   the above; Alt-click a part splits it there; Alt-drag
//                   on an empty instrument track draws a new part), SPLIT (3:
//                   click a part to cut it there, snapped; Alt: into equal
//                   pieces that long), GLUE (4: click a part to join it to
//                   the next; Alt: to all after it), ERASE (5: click a part
//                   to delete it; Alt: it and all after it), ZOOM (6: click
//                   to zoom in there, Alt-click out, drag across a stretch to
//                   fill the view with it), MUTE (7: click a part to mute or
//                   unmute it, or drag a rectangle over several), DRAW (8:
//                   drag on an instrument track for a new empty part), PLAY
//                   (9: hold the mouse down to play from there; let go to
//                   stop, back where it was). Alt+X splits the parts the
//                   playhead is in (the selected one only, if there is one).
//                   Right-click a part for its menu, whatever the tool.
//   wheel           scrolls the tracks; Shift+wheel scrolls along the song;
//                   Ctrl+wheel zooms (and the scroll bars, under and beside it)
//   keys            (as the manual has them) H zooms in, G out, a step
//                   centred on the playhead; Shift+H / Shift+G taller or
//                   shorter rows (Ctrl+Shift+wheel too); Shift+F shows the
//                   whole song; F turns Auto-Scroll on or off (AutoScroll)
class ArrangeView final : public juce::Component,
                          public juce::DragAndDropTarget,
                          private juce::ChangeListener,
                          private juce::Timer
{
public:
    ArrangeView (pr7s::Studio&, Timeline&);
    ~ArrangeView() override;

    struct Selection
    {
        int trackId = 0, index = -1;
        bool isValid() const noexcept { return index >= 0; }
    };
    Selection getSelection() const noexcept { return selection; }
    void select (int trackId, int index);
    void deleteSelected();
    void duplicateSelected();

    // Several parts selected (0.9.2, the manual's Object Selection tool):
    // Shift-click adds or takes one away, a drag on empty space draws a
    // rectangle that selects every part it touches (Shift: added to them),
    // Ctrl+A all of them. Dragging one moves them all (Alt: copies);
    // Delete, Ctrl+D, Ctrl+X / C / V, Alt+V (Paste at Origin) and the
    // part's menu act on them all. The part clicked last is the one the
    // key editor shows (getSelection).
    std::vector<int> getSelectedParts() const;   // part ids, in no order
    bool isPartSelected (int partId) const;
    void selectParts (const std::vector<int>& partIds);
    void selectAllParts();
    void copySelected (bool cut);   // into the parts clipboard
    void pasteParts (bool atOrigin);   // at the playhead on the selected track (or where they came from)
    bool hasPartsClip() const noexcept { return ! partsClip.isEmpty(); }
    std::function<void()> onPartsCopied;   // (Ctrl+V then pastes parts: MainView)

    // The editing tools (see above).
    enum class Tool { select, range, split, glue, erase, zoom, mute, draw, line, play, comp, warp };
    static constexpr Tool tools[] = { Tool::select, Tool::comp, Tool::range, Tool::split, Tool::glue, Tool::erase, Tool::zoom, Tool::mute, Tool::draw, Tool::line, Tool::warp, Tool::play };
    static const char* toolName (Tool);
    static int toolKey (Tool);   // its number key
    void setTool (Tool);
    Tool getTool() const noexcept { return tool; }
    void nextTool (bool forward);   // F10 / F9
    std::function<void()> onToolChange;
    void splitAtCursor();           // Alt+X
    // P (Locators to Selection): the range, else the selected part (none: false).
    bool locatorsToSelection();

    // Range Selection (RANGE, 2; the manual's Range Editing): one range at a
    // time, a stretch of time across tracks. It stays when another tool is
    // chosen; Esc (or a click with RANGE) clears it.
    const std::optional<pr7s::Studio::Range>& getRange() const noexcept { return range; }
    void setRange (std::optional<pr7s::Studio::Range>);
    bool hasRange() const noexcept { return range.has_value(); }
    enum class RangeCommand { split, crop, deleteContents, deleteTime, cut, cutTime, copy, paste, pasteTime, insertSilence, duplicate };
    void rangeCommand (RangeCommand);   // (the keys MainView sends, and the range's menu)
    bool hasRangeClip() const noexcept { return ! rangeClip.isEmpty(); }

    // H / G: one zoom step in or out, centred on the playhead; Shift+F: the
    // whole song (as far as the scroll bar goes) across the view.
    // The playhead's glide (0.17.4): one frame of it (the timer's; tests call it), and where it's drawn.
    void advancePlayhead();
    double getShownPlayhead() const noexcept { return shownBeat; }
    std::optional<double> getRulerHover() const noexcept { return rulerHover; }
    static constexpr double glideStep = 0.45;   // the share of the way left covered each frame (60 a second)
    // (0.78.3) The play bar while playing: `target` (Engine::getShownPosition) unless it's a hair
    // behind what's shown (under `smallestJumpBack` beats: the next block catching up), kept then.
    static double playingBeat (double shown, double target) noexcept;
    static constexpr double smallestJumpBack = 0.25;
    void zoom (bool in);
    void zoomFull();
    // Shift+H / Shift+G: the rows a step taller or shorter (the top row in view stays at the top).
    void zoomRows (bool in);
    // Auto-Scroll (Timeline::autoScroll): playing, or the playhead moved
    // while stopped (Home, the end of a recording), and it's out of sight:
    // the page turns so it's at the left. Called by the view's timer.
    void followPlayhead();

    // Automation points selected (in one lane at a time).
    struct PointSelection
    {
        int busId = 0, laneIndex = -1;
        std::vector<int> points;   // indices into the lane's points, sorted
    };
    const PointSelection& getPointSelection() const noexcept { return pointSelection; }
    bool hasSelectedPoints() const noexcept { return ! pointSelection.points.empty(); }
    void deleteSelectedPoints();
    // The tempo track's events (0.10.0): selected ones (indices), Delete removes them (never the first).
    const std::vector<int>& getTempoSelection() const noexcept { return tempoSelection; }
    bool hasSelectedTempoEvents() const noexcept { return ! tempoSelection.empty(); }
    void deleteSelectedTempoEvents();
    void selectAllTempoEvents();
    void showTempoMenu();
    std::function<void (int partId)> onEditAudio;   // an audio clip double-clicked: VariAudio (0.34.0)
    // Drag out (0.53.1): one part dragged over another window (a plug-in's, another program's, the
    // desktop), or Alt-dragged (a copy) out of the arrangement, leaves as a file (.mid, or a .wav of
    // what the event plays: Studio::writePartForDrag) through the system's file drag, which plug-ins
    // with their own JUCE and other programs take. The move is cancelled: the part stays where it was,
    // no undo step. `onExternalDrag` stands in for the system's drag (tests); returns the file.
    juce::File dragPartOut();
    std::function<void (const juce::StringArray&)> onExternalDrag;
    bool isDraggingOut() const noexcept { return draggingOut; }
    std::function<void (int partId)> onHarmonyVoices;   // its menu's Generate Harmony Voices (0.35.0)
    std::function<void()> onOpenAudioAlignment;   // the part's and range's menus (0.33.0)
    std::function<void()> onOpenTempoEditor;   // the menu's Open Tempo Track Editor (0.13.0)
    std::function<void (int partId)> onTempoDetection;   // the part menu's Tempo Detection (0.45.0)

    // The signature track (0.45.0, the manual's): its events at their bars (the first, bar 1, always
    // there), click to select, drag along the bars, DRAW (or a double-click on an empty bar) adds one
    // there, ERASE or Delete removes, a double-click types a new signature or bar.
    const std::vector<int>& getSignatureSelection() const noexcept { return signatureSelection; }
    void selectSignatures (std::vector<int> bars) { signatureSelection = std::move (bars); repaint(); }
    bool hasSelectedSignatures() const noexcept { return ! signatureSelection.empty(); }
    void deleteSelectedSignatures();
    void editSignature (int bar, bool adding);   // the box (an AlertWindow): signature and bar
    void showSignatureMenu();
    juce::Rectangle<float> getSignatureArea() const;
    juce::Rectangle<float> getSignatureBounds (int bar) const;   // an event's label
    int signatureAt (juce::Point<float>) const;                  // the bar of the event there, -1: none

    // The marker track (0.49.0, the manual's Markers): position markers (a line and a flag with
    // their ID and description) and cycle markers (a bar between two flags). Click selects
    // (Shift: more), drag moves (snapped), a cycle marker's edges drag its start or end;
    // Alt-click adds a position marker (Shift+Alt: and names it), Ctrl-drag a cycle marker
    // (Shift+Ctrl: and names it); DRAW adds, ERASE removes (Alt: all after it); a double-click
    // on a cycle marker sets the locators to it (Alt: zooms to it), on a position marker names
    // it; with the Range tool, a double-click between markers selects that range on every track.
    const std::vector<int>& getMarkerSelection() const noexcept { return markerSelection; }
    void selectMarkers (std::vector<int> indices) { markerSelection = std::move (indices); repaint(); }
    bool hasSelectedMarkers() const noexcept { return ! markerSelection.empty(); }
    void deleteSelectedMarkers();
    void editMarkerName (int index);   // a box to type its description
    void showMarkerMenu (int index);   // -1: the track's own
    // (0.82.3) A marker named as a song section ("Verse"), the sections numbered again ("Verse 1", "Verse 2").
    bool nameMarkerAsSection (int index, const juce::String& section);
    static constexpr int sectionItems = 300;
    void zoomTo (double start, double end);   // the view to a range (a cycle marker's)
    bool selectRangeBetweenMarkers (double beat);   // the Range tool's double-click
    juce::Rectangle<float> getMarkerArea() const;
    juce::Rectangle<float> getMarkerBounds (int index) const;   // a marker's flag (a cycle marker's: its bar)
    int markerAt (juce::Point<float>) const;                     // the marker there, -1: none
    std::function<void()> onOpenMarkers;   // the menu's Markers window (Ctrl+M)

    // Folder tracks (0.50.0, the manual's Folder Parts): a folder's row shows its tracks' parts as
    // blocks (where any of them has a part; each track a strip inside). A click on a block selects
    // every part in it (Shift: adds them), a drag moves them all along the song (Alt: copies them).
    std::vector<std::pair<double, double>> folderBlocks (int folderId) const;   // beats, merged
    std::vector<int> folderPartsIn (int folderId, double start, double end) const;   // part ids
    // Where an event is drawn, and the tempo row's curve area (tests).
    juce::Point<float> getTempoEventPosition (int index) const;
    juce::Rectangle<float> getTempoArea() const;
    float yForBpm (const juce::Rectangle<float>& area, double bpm) const;
    double bpmForY (const juce::Rectangle<float>& area, float y) const;
    void selectAllPoints (int busId, int laneIndex);
    bool cancelDrag();   // Esc while dragging points: back where they were
    // Where a lane, and a point on it, are drawn (for tests).
    juce::Rectangle<float> getLaneArea (int busId, int laneIndex) const;
    juce::Point<float> getPointPosition (int busId, int laneIndex, int point) const;

    // Automation editing (0.27.0, the manual's Editing Automation Events):
    //   SELECT    click a point; Shift- or Ctrl-click adds or removes one; drag a box round
    //             several; drag them (Ctrl: one direction only; Alt: a copy); double-click one
    //             to type its value (as its control reads: dB, L/C/R, ON/OFF, %) and where it is
    //             (all the selected get the value); Alt-drag draws.
    //   handles   two or more selected on a ramp: the manual's Automation Event Editor: the top
    //             corners tilt that end (Alt: compress it), the top's middle scales, the top edge
    //             moves them, the right edge's middle scales round the lane's middle (Alt: round
    //             theirs), the bottom edge stretches them along the song
    //   bends     on a ramp, the handle in a segment's middle bends it (the manual's Bézier
    //             curves); double-click the handle straightens it. At the lane's top edge above a
    //             segment: the quick vertical scaling handle
    //   DRAW      draws; ERASE removes the points it crosses; LINE draws a shape (LINE again: Line,
    //             Parabola (Ctrl: the other way), Sine, Triangle, Square: a period a grid step)
    enum class LineShape { line, parabola, sine, triangle, square };
    static juce::String lineShapeName (LineShape);
    void setLineShape (LineShape);
    LineShape getLineShape() const noexcept { return lineShape; }
    void showLineShapeMenu (juce::Component* target);
    void selectPoints (int busId, int laneIndex, std::vector<int> points);
    std::optional<juce::String> formatPointValue (int busId, const pr7s::AutomationLane&, float value) const;
    std::optional<float> parsePointValue (int busId, const pr7s::AutomationLane&, const juce::String&) const;
    // Typing (as the box's SET): the selected points' value, and for one its position. False if not one.
    bool enterPointValue (const juce::String& value, const juce::String& position);
    // The info line (0.37.0, the manual's Value Editing Rules): the selected points changed together,
    // relative to their own values (each by what the first one moves, in the control's units: dB,
    // pan, %), or all to the value typed (`absolute`: Ctrl held). Start moves them in time alike.
    bool setSelectedPointsValue (const juce::String& text, bool absolute);
    bool setSelectedPointsStart (const juce::String& text, bool absolute);
    void setSelectedPointsTerminator (bool);
    std::optional<double> pointDisplay (const pr7s::AutomationLane&, float value) const;   // dB, pan -100..100, %, 0/1
    float pointFromDisplay (const pr7s::AutomationLane&, double display) const;
    const pr7s::AutomationLane* getSelectedPointsLane() const { return laneFor (pointSelection.busId, pointSelection.laneIndex); }
    void showPointEntry (int busId, int laneIndex, int point);
    juce::Rectangle<float> getPointBox() const;              // the event editor's (empty: fewer than two, a step lane, or hidden)
    // (0.80.0, the owner: as the other DAW, only the selected points light up) The box and its
    // handles (tilt, compress, scale, stretch) are off unless switched on in the lane's menu
    // ("Editing Box for Selected Points"; settings key "automationEditBox").
    bool isPointBoxShown() const noexcept { return pointBoxShown; }
    void setPointBoxShown (bool shown);
    enum class PointHandle { none, tiltLeft, tiltRight, compressLeft, compressRight, scale, move, scaleAbsolute, scaleRelative, stretch };
    PointHandle pointHandleAt (juce::Point<float>, bool alt) const;
    void dragPointHandle (PointHandle, juce::Point<float> offset);   // from the points as the drag began (tests drive it)
    int curveHandleAt (int busId, int laneIndex, juce::Point<float>) const;   // the segment's end point (-1: none)
    juce::Point<float> getCurveHandle (int busId, int laneIndex, int point) const;
    void bendSegment (int busId, int laneIndex, int point, float towardsY);  // a segment's bend, so its middle is at that height
    void drawLineShape (int busId, int laneIndex, juce::Point<float> from, juce::Point<float> to, bool reverse);
    void showAutomationFunctions (int busId, int laneIndex);   // the lane menu's Copy / Paste Automation and the rest
    // (0.28.0) Trim on and a volume or send lane with points: its trim curve is what's edited.
    bool isTrimming (const pr7s::AutomationLane&) const;
    const std::vector<pr7s::AutomationPoint>& editPointsOf (const pr7s::AutomationLane& lane) const { return isTrimming (lane) ? lane.trim : lane.points; }

    // Asked to edit the selected part's notes (double-click, or its menu): the key editor opens.
    std::function<void()> onEditNotes;
    // Render in Place (a part's menu, the range's): what to render, and
    // whether to ask how (else as last time).
    std::function<void (std::vector<pr7s::Studio::RenderSource>, bool ask)> onRenderInPlace;

    // Where a part is drawn (for tests).
    juce::Rectangle<float> getPartBounds (int trackId, int index) const;
    float splitLineX() const;   // SPLIT: where its cut line is drawn now (snapped to the grid), -1: none
    int trackAtY (float y) const;   // 0 if none (a lane or the master isn't a track); a lane of takes: its track
    int laneAtY (float y) const;    // a lane of takes (0.29.0): its lane; else -1
    // For the tests (0.29.0): the Comp tool's band while dragged (beats; none: equal).
    std::pair<double, double> getCompBand() const noexcept { return { compFrom, compTo }; }
    // (0.30.0) The edge (a split between takes, or a take's own edge) under a point, if near one: its beat.
    std::optional<double> takeEdgeAt (juce::Point<float>) const;
    // The comp handle on a stacked take's lower edge (the manual's Comp symbol): none if not stacked.
    juce::Rectangle<float> compHandleBounds (int trackId, int index) const;
    bool compHandleDown (const juce::MouseEvent&);
    double getEdgeTarget() const noexcept { return edgeTo; }
    // (0.32.0) A warp marker's handle under a point on a clip: its index (-1: none).
    int warpMarkerAt (int trackId, int index, juce::Point<float>) const;
    // Hitpoints are drawn on the selected audio clip when they're at least this far apart (px).
    static constexpr float hitpointRoom = 4.0f;
    void showHitpointSettings (juce::Component* target);
    juce::Rectangle<float> partRow (int trackId, int lane) const;   // the strip (y and height) a part on that lane is drawn in
    // A chord pad dropped on a track: a one-bar MIDI part at the bar (0.14.0, the manual's
    // Creating MIDI Parts from Chord Pads).
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    // The chord track (0.16.0): its chords' blocks, selected ones (indices), edited.
    const std::vector<int>& getChordSelection() const noexcept { return chordSelection; }
    bool hasSelectedChordEvents() const noexcept { return ! chordSelection.empty(); }
    void deleteSelectedChordEvents();
    juce::Rectangle<float> getChordArea() const;           // the chords' strip (the scales' below it)
    juce::Rectangle<float> getChordEventBounds (int index) const;
    void editChordEvent (int index);                      // the chord editor box (a double-click)
    ChordEventEditor* getChordEditor() noexcept { return chordEditor.getComponent(); }
    std::function<void()> onChordsToPads;                 // (the pads' view takes them)
    std::function<void()> onProgressions;   // the progression library (0.40.0)
    std::function<void (std::vector<int> chordEvents)> onLineIdeas;
    std::function<void (std::vector<int> chordEvents)> onBuildSection;   // (0.57.0) IDEAS with BUILD SECTION open   // BASS & MELODY (0.18.0): for these chord events (none: the cycle's or all)
    std::optional<Timeline::Row> rowAt (float y) const;
    float rowTop (const Timeline::Row&) const;   // where a row is drawn now
    float trackTop (int trackId) const;
    const std::vector<Timeline::Row>& rowsNow (std::vector<Timeline::Row>& made) const;
    mutable const std::vector<Timeline::Row>* rowsInPaint = nullptr;   // (set while painting)
    // What's drawn beyond what's being redrawn, so the lines crossing into it are the same (0.34.4).
    static constexpr float rowMargin = 40.0f, curveMargin = 64.0f;
    // A MIDI file being dragged in (0.18.1): where it will land, lit in the family's gradient.
    struct DropPreview { int trackId = 0; double start = 0.0, length = 0.0; juce::String name; int moreTracks = 0;
                         std::vector<pr7s::Note> notes; };   // (the first track's, from the part's start: drawn inside it)
    void setDropPreview (std::optional<DropPreview>);
    const std::optional<DropPreview>& getDropPreview() const noexcept { return dropPreview; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void paintPart (juce::Graphics&, juce::Rectangle<float>, const pr7s::Part&, juce::Colour, bool selected, bool recording) const;
public:
    // The timer's work (for tests): the playhead's old and new strips redrawn
    // when it moved, nothing when nothing did (0.9.2: it was the whole view, 30
    // times a second, which on a big window kept the computer busy: the lag).
    void refreshPlayhead();
    std::optional<std::pair<int, int>> partAt (juce::Point<float>) const;   // track id and index (public for the tests: 0.29.0)
    Timeline& getTimeline() noexcept { return timeline; }
private:
    void showPartMenu (int trackId, int index);

    pr7s::Studio& studio;

    juce::uint64 seenChange = 0;   // (0.34.3) the studio's change version last acted on
    Timeline& timeline;
    Selection selection;
    int selectedPartId = 0;                     // its part's id (found again after the song changes)
    Selection selectionOf (int trackId, int index);

    enum class Drag { none, signatureMove, position, cycle, cycleMove, cycleLeft, cycleRight, move, resize, trimStart, fadeIn, fadeOut, points, band, draw,
                      erasePoints, linePoints, pointHandle, curve, segmentScale,
                      newPart, muteBand, zoomBand, play, rangeNew, rangeStart, rangeEnd, rangeMove, marquee,
                      tempoPoints, tempoBand, tempoDraw, chordMove, comp, takeEdge, slip, audition, warpMarker,
                      markerMove, markerStart, markerEnd, markerNewCycle,   // (0.49.0)
                      folderMove };   // (0.50.0)
    // WARP (0.32.0, the manual's Free Warp in the event display): the marker being dragged.
    int warpPartId = 0, warpIndex = -1;
    double warpTo = 0.0;
    bool warpDown (const juce::MouseEvent&);
    void warpDrag (const juce::MouseEvent&);
    void warpUp();
    // The Comp tool (0.29.0): its drag across a lane of takes.
    int compTrack = 0, compLane = -1, compPartId = 0;
    double compFrom = 0.0, compTo = 0.0;
    bool compDown (const juce::MouseEvent&);
    void compDrag (const juce::MouseEvent&);
    void compUp();
    // (0.30.0) Adjusting a split / resizing (takeEdge), Slip Event (slip), the Comp tool's Ctrl (audition).
    double edgeFrom = 0.0, edgeTo = 0.0, slipFrom = 0.0, slipDelta = 0.0;
    int slipPartId = 0;
    void frontClick (int trackId, int partId, double beat);   // to the front; heard there already: back
    std::vector<int> alsoSelected;              // the parts selected besides `selection` (by id)
    pr7s::Studio::RangeClip partsClip;          // parts copied (Ctrl+C / X)
    int partIdAt (const Selection&) const;
    bool isSelected (int trackId, int index) const;
    std::optional<pr7s::Studio::Range> range;
    pr7s::Studio::RangeClip rangeClip;
    pr7s::Studio::Range rangeBefore;            // as it was when the drag began
    double rangeAnchor = 0.0;                   // the beat the drag began at
    int rangeAnchorTrack = 0;                   // the track index it began on
    double rangeMoveStart = 0.0;                // dragging its contents: where they'd go
    int rangeMoveTrack = 0;                     // ... and the first track
    bool rangeDown (const juce::MouseEvent&);
    void rangeDrag (const juce::MouseEvent&);
    void rangeUp (const juce::MouseEvent&);
    bool rangeDoubleClick (const juce::MouseEvent&);
    void showRangeMenu();
    void paintRange (juce::Graphics&) const;
    int trackIndex (int trackId) const;         // in the song's order (-1 if none)
    std::vector<int> tracksBetween (int fromIndex, int toIndex) const;
    double rangeBeat (const juce::MouseEvent&) const;   // snapped (Ctrl: not)
    Tool tool = Tool::select;
    bool toolDown (const juce::MouseEvent&);   // the tools other than SELECT (and SELECT's Alt gestures); true if it took the click
    void toolUp (const juce::MouseEvent&);
    juce::Rectangle<float> toolBand;           // MUTE's rectangle, ZOOM's stretch, DRAW's new part, as dragged
    int newPartTrack = 0;
    double playReturn = 0.0;                   // PLAY: where the playhead goes back to
    juce::Point<float> hover { -1.0f, -1.0f }; // SPLIT: where the mouse is over a part
    std::map<int, juce::MouseCursor> toolCursors;   // each tool's icon as the pointer, made once
    double splitBeat (const juce::MouseEvent&, const pr7s::Part&) const;
    void cannotGlue();
    void updateCursor (const juce::MouseEvent&);

    // Automation editing.
    PointSelection pointSelection;
    std::vector<pr7s::AutomationPoint> workingPoints, originalPoints;   // the lane's points during a gesture
    juce::Rectangle<float> pointBand;
    // The select box's corner where it began, in the song (beat, pixels down
    // the rows), so it stays put while the view scrolls under it (0.11.1).
    double marqueeBeat = 0.0;
    float marqueeRowY = 0.0f;
    bool scrollTowards (juce::Point<float>);   // past an edge: the view moves that way (true if it did)
    juce::Point<float> pointDownAt;
    int anchor = -1;               // the point grabbed
    bool copyingPoints = false;
    std::vector<pr7s::AutomationPoint> drawn;   // the points a drawing stroke has put down
    LineShape lineShape = LineShape::line;
    PointHandle activeHandle = PointHandle::none;
    juce::Rectangle<float> boxAtDown;
    std::vector<int> selectionAtDown;          // the handles' points, as indices into `originalPoints`
    int curvePoint = -1;                       // the segment being bent or scaled (its end point)
    bool ctrlToggle = false;                   // a Ctrl-click on a point: toggled on release if it didn't move
    std::vector<int> bandBefore;               // the points selected when a Shift/Ctrl box began
    struct HoverCurve { int busId = 0, laneIndex = -1, point = -1; } hoverCurve;   // the bend handle the mouse is on
    juce::Point<float> lineEnd;
    bool automationDoubleClick (const juce::MouseEvent&);
    void paintPointHandles (juce::Graphics&, const Timeline::Row&) const;
    bool pointBoxShown = false;
    float lineValueAt (double t, float v1, float v2, double beatsInto, bool reverse) const;
    void paintAutomation (juce::Graphics&, const Timeline::Row&) const;
    void paintTempo (juce::Graphics&, const Timeline::Row&) const;
    bool tempoDown (const juce::MouseEvent&);
    void tempoDrag (const juce::MouseEvent&);
    void tempoUp();
    void editTempoEvent (int index);   // a box to type its tempo (double-click)
    std::vector<int> tempoSelection;
    std::vector<int> chordSelection;
    std::vector<int> signatureSelection;   // (0.45.0) bars
    int signatureDragFrom = -1, signatureDragTo = -1;
    void paintSignature (juce::Graphics&, const Timeline::Row&);
    std::vector<int> markerSelection;   // (0.49.0) indices into the marker track
    std::vector<pr7s::Marker> markerOriginal, markerDragged;   // a drag's start, and where it's going (drawn)
    double markerDownBeat = 0.0;
    int markerDragIndex = -1;
    bool markerNameAfter = false;   // Shift held when adding: the name box follows
    void paintMarkers (juce::Graphics&, const Timeline::Row&);
    void paintFolders (juce::Graphics&);
    bool folderDown (const juce::MouseEvent&);
    std::vector<int> folderDragIds;
    int folderDragId = 0;
    double folderDownBeat = 0.0, folderDelta = 0.0;
    void paintMarkerLines (juce::Graphics&);
    bool markerDown (const juce::MouseEvent&);
    void markerDrag (const juce::MouseEvent&);
    void markerUp();
    bool markerDoubleClick (const juce::MouseEvent&);
    const std::vector<pr7s::Marker>& shownMarkers() const;
    bool signatureDown (const juce::MouseEvent&);
    void signatureDrag (const juce::MouseEvent&);
    void signatureUp();
    std::vector<pr7s::ChordEvent> chordOriginal;
    double chordDownBeat = 0.0;
    juce::Component::SafePointer<ChordEventEditor> chordEditor;
    bool chordDown (const juce::MouseEvent&);
    void chordDragTo (const juce::MouseEvent&);
    void chordUp();
    void paintChords (juce::Graphics&, const Timeline::Row&);
    void showChordMenu (double beat, int index);
    void showScaleMenu (double beat);
    std::vector<pr7s::TempoEvent> tempoWorking, tempoOriginal;   // during a gesture
    int tempoAnchor = -1;
    const pr7s::AutomationLane* laneFor (int busId, int laneIndex) const;
    float yForValue (juce::Rectangle<float> area, float value) const;
    float valueForY (juce::Rectangle<float> area, float y) const;
    bool automationDown (const juce::MouseEvent&, const Timeline::Row&);
    void automationDrag (const juce::MouseEvent&);
    void automationUp();
    void commitPoints (std::vector<pr7s::AutomationPoint> points, const std::vector<pr7s::AutomationPoint>& selected);
    void showLaneMenu (int busId, int laneIndex);
    pr7s::Part dragPart;   // an audio clip as it's being trimmed or faded
    bool isAudioTrack (int trackId) const;
    float fadeInWidth (const pr7s::Part&) const;    // a clip's fades in pixels (by the tempo map)
    float fadeOutWidth (const pr7s::Part&) const;
    juce::Rectangle<float> fadeHandle (juce::Rectangle<float> bounds, const pr7s::Part&, bool fadeIn) const;
    Drag drag = Drag::none;
    double dragStartBeat = 0.0, dragOriginalStart = 0.0, dragOriginalLength = 0.0;
    // (0.78.0, the owner) The ruler: a click sets the position; dragged up or down it zooms instead
    // (0.78.1: down zooms in, up out, as the manual; 0.78.0 had it the other way, the owner asked it flipped), the clicked beat kept under the mouse.
    bool rulerZooming = false;
    float rulerDownX = 0.0f, rulerDownY = 0.0f;
    double rulerZoomBeat = 0.0, rulerZoomFrom = 0.0;
public:
    static constexpr float rulerZoomStart = 4.0f, rulerZoomPixelsPerDouble = 80.0f;   // a doubling (or halving) per 80 px
    bool isRulerZooming() const noexcept { return rulerZooming; }
private:
    int dragTrack = 0, dragTargetTrack = 0;
    int dragTargetLane = -1;   // (0.29.0) the lane of takes it's dragged onto (-1: not over one)
    double dragNewStart = 0.0, dragNewLength = 0.0;
    bool dragCopies = false, dragMoved = false;
    bool draggingOut = false;
    bool wantsDragOut (const juce::MouseEvent&) const;
    double lastPosition = 0.0;   // the playhead as last seen (Auto-Scroll follows a jump while stopped)
    float playheadX = -1.0e6f;   // where the playhead was last drawn (0.9.2: only its strip is redrawn as it moves)
    // The playhead as drawn (0.17.4, the owner asked: it glides into a new place while
    // stopped, as it snaps to the grid, instead of jumping; exact while playing).
    double shownBeat = 0.0;
    bool gliding = false;
    // Where a click in the ruler would put the playhead (snapped), shown while hovering.
    std::optional<double> rulerHover;
    std::optional<DropPreview> dropPreview;
    juce::Rectangle<float> dropPreviewBounds() const;
    void setRulerHover (std::optional<double>);
    bool playheadRed = false;
    void scrolled();             // the track list and the scroll bars follow
};

// A tool's key: its icon (ToolIcons), and its name unless `iconOnly` (on the
// transport, where the keys are narrower; the name's in the tooltip).
class ToolKey final : public juce::TextButton
{
public:
    ArrangeView::Tool tool = ArrangeView::Tool::select;
    bool iconOnly = false;
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// The arrangement's tools in a row over it (as the manual's toolbar): a key
// each (its number key in the tooltip), lit while it's the one chosen.
class ArrangeTools final : public juce::Component
{
public:
    explicit ArrangeTools (ArrangeView&);
    void refresh();   // the chosen tool lit
    // On the transport unit (a wide window): TOOLS stamped in the unit's ink, as the keys' names are.
    void setOnUnit (bool);
    int getPreferredWidth() const noexcept { return onUnit ? unitWidth() : 64 + (int) std::size (ArrangeView::tools) * 74; }
    static int unitWidth() noexcept { return 576; }   // on the unit: no label (the icons tell them apart); the keys share the 9 x 64 it always had (0.27.0: LINE, 0.29.0: COMP)
    juce::TextButton& getButton (ArrangeView::Tool);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ArrangeView& arrange;
    std::array<ToolKey, std::size (ArrangeView::tools)> buttons;
    bool onUnit = false;
};
