#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// Where the arrangement is scrolled and zoomed, shared by the track list
// (which must line up with the lanes) and the arrangement.
struct Timeline
{
    double pixelsPerBeat = 28.0;   // zoom
    double firstBeat = 0.0;        // the beat at the arrangement's left edge
    int scrollY = 0;               // pixels the tracks are scrolled up by

    static constexpr int trackHeight = 72;
    static constexpr int rulerHeight = 36;
    static constexpr int cycleStripHeight = 14;   // the ruler's top strip, where the cycle is dragged

    float xForBeat (double beat) const noexcept { return (float) ((beat - firstBeat) * pixelsPerBeat); }
    double beatForX (float x) const noexcept { return firstBeat + x / pixelsPerBeat; }

    // The grid parts and the position snap to: the beat when zoomed in far
    // enough to see it, else the bar.
    double grid (const pr7s::Project& project) const noexcept
    {
        return pixelsPerBeat >= 14.0 ? 4.0 / project.denominator : project.beatsPerBar();
    }

    std::function<void()> onScrollOrZoom;   // the track list follows
};

// The arrangement (inspired by Cubase's project window, in our look): the ruler with
// bar numbers and the cycle range on top, a lane for each track, its parts
// as coloured blocks showing their notes, and the playhead.
//
//   ruler           click to put the position there; drag across its top
//                   strip to set the cycle (and turn CYCLE on)
//   a part          click to select; drag to move (to another track too);
//                   Alt-drag to copy; drag its right edge to lengthen or
//                   shorten it; double-click to edit its notes (the key
//                   editor); right-click for Edit notes, Duplicate and Delete
//   an empty lane   click to select the track; double-click for a new
//                   empty bar-long part
//   wheel           scrolls the tracks; Shift+wheel scrolls along the song;
//                   Ctrl+wheel zooms
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

    // Asked to edit the selected part's notes (double-click, or its menu): the key editor opens.
    std::function<void()> onEditNotes;

    // Where a part is drawn (for tests).
    juce::Rectangle<float> getPartBounds (int trackId, int index) const;
    int trackAtY (float y) const;   // 0 if none

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
    float laneTop (int trackIndex) const;
    void paintPart (juce::Graphics&, juce::Rectangle<float>, const pr7s::Part&, juce::Colour, bool selected, bool recording) const;
    std::optional<std::pair<int, int>> partAt (juce::Point<float>) const;   // track id and index
    void showPartMenu (int trackId, int index);

    pr7s::Studio& studio;
    Timeline& timeline;
    Selection selection;

    enum class Drag { none, position, cycle, move, resize };
    Drag drag = Drag::none;
    double dragStartBeat = 0.0, dragOriginalStart = 0.0, dragOriginalLength = 0.0;
    int dragTrack = 0, dragTargetTrack = 0;
    double dragNewStart = 0.0, dragNewLength = 0.0;
    bool dragCopies = false, dragMoved = false;
};
