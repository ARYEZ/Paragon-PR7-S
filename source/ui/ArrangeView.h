#pragma once

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

    // The grid parts and the position snap to: the beat when zoomed in far
    // enough to see it, else the bar.
    double grid (const pr7s::Project& project) const noexcept
    {
        return pixelsPerBeat >= 14.0 ? 4.0 / project.denominator : project.beatsPerBar();
    }

    std::function<void()> onScrollOrZoom;   // the track list follows

    // The rows, top to bottom, shared by the track list and the arrangement:
    // each track, its open automation lanes under it, then the master's row
    // and its lanes. `top` is from the first row's top, before scrolling.
    static constexpr int laneHeight = 48, masterHeight = 40;   // at the vertical zoom's 1
    struct Row
    {
        enum class Kind { track, lane, master };
        Kind kind = Kind::track;
        int busId = 0;          // a track's id, or the master's (0)
        int laneIndex = -1;     // a lane: its index in the bus's automation
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
//                   on or off, drag inside it to move it
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
//   wheel           scrolls the tracks; Shift+wheel scrolls along the song;
//                   Ctrl+wheel zooms (and the scroll bars, under and beside it)
//   keys            (as the manual has them) H zooms in, G out, a step
//                   centred on the playhead; Shift+H / Shift+G taller or
//                   shorter rows (Ctrl+Shift+wheel too); Shift+F shows the
//                   whole song; F turns Auto-Scroll on or off (AutoScroll)
class ArrangeView final : public juce::Component,
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

    // H / G: one zoom step in or out, centred on the playhead; Shift+F: the
    // whole song (as far as the scroll bar goes) across the view.
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
    void selectAllPoints (int busId, int laneIndex);
    bool cancelDrag();   // Esc while dragging points: back where they were
    // Where a lane, and a point on it, are drawn (for tests).
    juce::Rectangle<float> getLaneArea (int busId, int laneIndex) const;
    juce::Point<float> getPointPosition (int busId, int laneIndex, int point) const;

    // Asked to edit the selected part's notes (double-click, or its menu): the key editor opens.
    std::function<void()> onEditNotes;

    // Where a part is drawn (for tests).
    juce::Rectangle<float> getPartBounds (int trackId, int index) const;
    int trackAtY (float y) const;   // 0 if none (a lane or the master isn't a track)
    std::optional<Timeline::Row> rowAt (float y) const;
    float rowTop (const Timeline::Row&) const;   // where a row is drawn now
    float trackTop (int trackId) const;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void paintPart (juce::Graphics&, juce::Rectangle<float>, const pr7s::Part&, juce::Colour, bool selected, bool recording) const;
    std::optional<std::pair<int, int>> partAt (juce::Point<float>) const;   // track id and index
    void showPartMenu (int trackId, int index);

    pr7s::Studio& studio;
    Timeline& timeline;
    Selection selection;

    enum class Drag { none, position, cycle, cycleMove, move, resize, trimStart, fadeIn, fadeOut, points, band, draw };

    // Automation editing.
    PointSelection pointSelection;
    std::vector<pr7s::AutomationPoint> workingPoints, originalPoints;   // the lane's points during a gesture
    juce::Rectangle<float> pointBand;
    juce::Point<float> pointDownAt;
    int anchor = -1;               // the point grabbed
    bool copyingPoints = false;
    std::vector<pr7s::AutomationPoint> drawn;   // the points a drawing stroke has put down
    void paintAutomation (juce::Graphics&, const Timeline::Row&) const;
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
    juce::Rectangle<float> fadeHandle (juce::Rectangle<float> bounds, const pr7s::Part&, bool fadeIn) const;
    Drag drag = Drag::none;
    double dragStartBeat = 0.0, dragOriginalStart = 0.0, dragOriginalLength = 0.0;
    int dragTrack = 0, dragTargetTrack = 0;
    double dragNewStart = 0.0, dragNewLength = 0.0;
    bool dragCopies = false, dragMoved = false;
    double lastPosition = 0.0;   // the playhead as last seen (Auto-Scroll follows a jump while stopped)
    void scrolled();             // the track list and the scroll bars follow
};
