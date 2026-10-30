#include "ArrangeView.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    constexpr float edgeGrab = 6.0f;   // the right edge of a part, for resizing
    const juce::Colour laneDark { 0xff0d0a0e }, laneLight { 0xff120e13 }, gridBar { 0xff2a2229 }, gridBeat { 0xff1a1519 };

    juce::FontOptions font (float height, int style = juce::Font::bold) { return juce::FontOptions (height, style); }
}

ArrangeView::ArrangeView (pr7s::Studio& studioToUse, Timeline& timelineToUse)
    : studio (studioToUse), timeline (timelineToUse)
{
    setWantsKeyboardFocus (false);
    studio.addChangeListener (this);
    startTimerHz (30);
}

ArrangeView::~ArrangeView()
{
    studio.removeChangeListener (this);
}

void ArrangeView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // A part may have gone (undo, delete): drop a selection that no longer exists.
    if (selection.isValid())
    {
        const auto* track = studio.getProject().findTrack (selection.trackId);
        if (track == nullptr || selection.index >= (int) track->parts.size())
            selection = {};
    }
    repaint();
}

void ArrangeView::timerCallback()
{
    // While playing, the view turns the page when the playhead reaches the right edge.
    auto& engine = studio.getEngine();
    if (engine.isPlaying() && drag == Drag::none)
    {
        const auto x = timeline.xForBeat (engine.getPosition());
        if (x > (float) getWidth() - 30.0f || x < 0.0f)
        {
            timeline.firstBeat = juce::jmax (0.0, engine.getPosition() - 1.0);
            if (timeline.onScrollOrZoom)
                timeline.onScrollOrZoom();
        }
    }
    repaint();
}

float ArrangeView::laneTop (int trackIndex) const
{
    return (float) (Timeline::rulerHeight + trackIndex * Timeline::trackHeight - timeline.scrollY);
}

int ArrangeView::trackAtY (float y) const
{
    const auto& tracks = studio.getProject().tracks;
    const auto index = (int) std::floor ((y - (float) Timeline::rulerHeight + (float) timeline.scrollY) / (float) Timeline::trackHeight);
    return y >= (float) Timeline::rulerHeight && juce::isPositiveAndBelow (index, (int) tracks.size()) ? tracks[(size_t) index].id : 0;
}

juce::Rectangle<float> ArrangeView::getPartBounds (int trackId, int index) const
{
    const auto& tracks = studio.getProject().tracks;
    for (size_t t = 0; t < tracks.size(); ++t)
        if (tracks[t].id == trackId && juce::isPositiveAndBelow (index, (int) tracks[t].parts.size()))
        {
            const auto& part = tracks[t].parts[(size_t) index];
            const auto x = timeline.xForBeat (part.start);
            return { x, laneTop ((int) t) + 3.0f, (float) (part.length * timeline.pixelsPerBeat), (float) Timeline::trackHeight - 6.0f };
        }
    return {};
}

std::optional<std::pair<int, int>> ArrangeView::partAt (juce::Point<float> point) const
{
    const auto trackId = trackAtY (point.y);
    if (const auto* track = studio.getProject().findTrack (trackId))
        for (int i = (int) track->parts.size() - 1; i >= 0; --i)   // the one drawn on top first
            if (getPartBounds (trackId, i).contains (point))
                return std::pair<int, int> { trackId, i };
    return std::nullopt;
}

void ArrangeView::select (int trackId, int index)
{
    selection = { trackId, index };
    if (trackId != 0)
        studio.selectTrack (trackId);
    repaint();
}

void ArrangeView::deleteSelected()
{
    if (! selection.isValid())
        return;
    const auto doomed = selection;
    selection = {};
    studio.deletePart (doomed.trackId, doomed.index);
}

void ArrangeView::duplicateSelected()
{
    if (! selection.isValid())
        return;
    if (const auto* track = studio.getProject().findTrack (selection.trackId))
        if (juce::isPositiveAndBelow (selection.index, (int) track->parts.size()))
        {
            const auto& part = track->parts[(size_t) selection.index];
            studio.copyPart (selection.trackId, selection.index, selection.trackId, part.end());
            select (selection.trackId, (int) studio.getProject().findTrack (selection.trackId)->parts.size() - 1);
        }
}

//==============================================================================
void ArrangeView::paint (juce::Graphics& g)
{
    const auto& project = studio.getProject();
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (laneDark);
    g.fillRect (bounds);

    // The lanes, the selected track's a little lighter.
    for (size_t t = 0; t < project.tracks.size(); ++t)
    {
        const auto top = laneTop ((int) t);
        const auto lane = juce::Rectangle<float> (0.0f, top, bounds.getWidth(), (float) Timeline::trackHeight);
        g.setColour (project.tracks[t].id == studio.getSelectedTrack() ? juce::Colour (0xff1c151b) : (t % 2 == 0 ? laneDark : laneLight));
        g.fillRect (lane);
        g.setColour (juce::Colour (0xff050405));
        g.fillRect (lane.withHeight (1.0f).withY (lane.getBottom() - 1.0f));
    }

    // The grid: bars brighter than beats (beats only when there's room).
    const auto beatUnit = 4.0 / project.denominator;
    const auto bar = project.beatsPerBar();
    const auto lastVisible = timeline.beatForX (bounds.getWidth());
    const auto showBeats = beatUnit * timeline.pixelsPerBeat >= 10.0;
    for (auto beat = std::floor (timeline.firstBeat / beatUnit) * beatUnit; beat <= lastVisible; beat += beatUnit)
    {
        const auto isBar = std::abs (std::fmod (beat, bar)) < 1.0e-6;
        if (! isBar && ! showBeats)
            continue;
        g.setColour (isBar ? gridBar : gridBeat);
        g.fillRect (juce::Rectangle<float> (timeline.xForBeat (beat), (float) Timeline::rulerHeight, 1.0f, bounds.getHeight()));
    }

    // The cycle range over the lanes, faintly.
    {
        const auto x1 = timeline.xForBeat (project.loopStart), x2 = timeline.xForBeat (project.loopEnd);
        g.setColour (ParagonColours::accent.withAlpha (project.loopOn ? 0.07f : 0.025f));
        g.fillRect (juce::Rectangle<float> (x1, (float) Timeline::rulerHeight, x2 - x1, bounds.getHeight()));
    }

    // The parts (the one being dragged drawn where it's going).
    const auto recording = studio.getRecordingPreview();
    for (size_t t = 0; t < project.tracks.size(); ++t)
    {
        const auto& track = project.tracks[t];
        for (int i = 0; i < (int) track.parts.size(); ++i)
        {
            const auto isDragged = (drag == Drag::move || drag == Drag::resize) && dragMoved && track.id == dragTrack && i == selection.index;
            if (isDragged && ! dragCopies)
                continue;
            paintPart (g, getPartBounds (track.id, i), track.parts[(size_t) i], track.colour,
                       selection.trackId == track.id && selection.index == i, false);
        }
    }
    if ((drag == Drag::move || drag == Drag::resize) && dragMoved && selection.isValid())
        if (const auto* from = project.findTrack (dragTrack))
            if (juce::isPositiveAndBelow (selection.index, (int) from->parts.size()))
            {
                auto part = from->parts[(size_t) selection.index];
                part.start = dragNewStart;
                part.length = dragNewLength;
                int targetIndex = 0;
                for (size_t t = 0; t < project.tracks.size(); ++t)
                    if (project.tracks[t].id == dragTargetTrack)
                        targetIndex = (int) t;
                const juce::Rectangle<float> area (timeline.xForBeat (part.start), laneTop (targetIndex) + 3.0f,
                                                   (float) (part.length * timeline.pixelsPerBeat), (float) Timeline::trackHeight - 6.0f);
                paintPart (g, area, part, from->colour, true, false);
            }
    if (recording)
        for (size_t t = 0; t < project.tracks.size(); ++t)
            if (project.tracks[t].id == recording->first)
            {
                const auto& part = recording->second;
                paintPart (g, { timeline.xForBeat (part.start), laneTop ((int) t) + 3.0f, (float) (part.length * timeline.pixelsPerBeat),
                                (float) Timeline::trackHeight - 6.0f },
                           part, project.tracks[t].colour, false, true);
            }

    // The ruler: bar numbers, beat ticks, the cycle strip.
    const auto ruler = bounds.withHeight ((float) Timeline::rulerHeight);
    g.setColour (juce::Colour (0xff151116));
    g.fillRect (ruler);
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (ruler.withY (ruler.getBottom() - 1.0f).withHeight (1.0f));
    {
        const auto x1 = timeline.xForBeat (project.loopStart), x2 = timeline.xForBeat (project.loopEnd);
        const auto strip = juce::Rectangle<float> (x1, 2.0f, x2 - x1, (float) Timeline::cycleStripHeight - 3.0f);
        g.setColour (project.loopOn ? ParagonColours::accent : ParagonColours::textDim.withAlpha (0.45f));
        g.fillRoundedRectangle (strip, 2.0f);
        if (project.loopOn)
        {
            g.setColour (ParagonColours::accent.withAlpha (0.3f));
            g.fillRoundedRectangle (strip.expanded (2.0f), 3.0f);
        }
    }
    // Bar numbers every bar if there's room, else every 2, 4, 8...
    auto every = 1;
    while (bar * every * timeline.pixelsPerBeat < 36.0)
        every *= 2;
    g.setFont (font (11.0f));
    for (auto b = (int) std::floor (timeline.firstBeat / bar); b * bar <= lastVisible; ++b)
    {
        const auto x = timeline.xForBeat (b * bar);
        const auto numbered = b % every == 0;
        g.setColour (numbered ? ParagonColours::textDim : ParagonColours::panelEdge);
        g.fillRect (juce::Rectangle<float> (x, numbered ? 16.0f : 26.0f, 1.0f, ruler.getBottom() - (numbered ? 16.0f : 26.0f)));
        if (numbered)
        {
            g.setColour (ParagonColours::text);
            g.drawText (juce::String (b + 1), juce::Rectangle<float> (x + 4.0f, 15.0f, 40.0f, 16.0f), juce::Justification::centredLeft, false);
        }
    }

    // The playhead: a line down the lanes and a marker on the ruler.
    const auto x = timeline.xForBeat (studio.getEngine().getPosition());
    if (x >= -2.0f && x <= bounds.getWidth() + 2.0f)
    {
        const auto red = studio.getEngine().isRecording();
        const auto colour = red ? juce::Colour (0xffff3b2f) : ParagonColours::accent;
        g.setColour (colour.withAlpha (0.25f));
        g.fillRect (juce::Rectangle<float> (x - 2.0f, (float) Timeline::rulerHeight, 5.0f, bounds.getHeight()));
        g.setColour (colour);
        g.fillRect (juce::Rectangle<float> (x, (float) Timeline::rulerHeight - 6.0f, 1.2f, bounds.getHeight()));
        juce::Path marker;
        marker.addTriangle (x - 6.0f, (float) Timeline::rulerHeight - 12.0f, x + 6.0f, (float) Timeline::rulerHeight - 12.0f,
                            x, (float) Timeline::rulerHeight - 2.0f);
        g.fillPath (marker);
    }

    if (project.tracks.empty())
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (font (14.0f, juce::Font::plain));
        g.drawText ("Add a track (+ ADD TRACK, bottom left) to start", bounds.withTrimmedTop ((float) Timeline::rulerHeight),
                    juce::Justification::centred, false);
    }
}

void ArrangeView::paintPart (juce::Graphics& g, juce::Rectangle<float> area, const pr7s::Part& part, juce::Colour colour,
                             bool selected, bool recording) const
{
    if (area.getRight() < 0.0f || area.getX() > (float) getWidth())
        return;
    g.setColour (colour.withAlpha (0.32f));
    g.fillRoundedRectangle (area, 4.0f);
    auto band = area.withHeight (15.0f);
    g.setColour (colour.withAlpha (0.85f));
    g.fillRoundedRectangle (band, 4.0f);
    g.fillRect (band.withTrimmedTop (8.0f));
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.setFont (font (10.5f));
    g.drawText (recording ? juce::String ("RECORDING") : part.name, band.reduced (5.0f, 0.0f), juce::Justification::centredLeft, true);

    // Its notes, scaled to the part's range of pitches.
    if (! part.notes.empty())
    {
        auto lowest = 127, highest = 0;
        for (const auto& note : part.notes)
        {
            lowest = juce::jmin (lowest, note.pitch);
            highest = juce::jmax (highest, note.pitch);
        }
        const auto span = juce::jmax (12, highest - lowest + 1);
        const auto notesArea = area.withTrimmedTop (17.0f).reduced (2.0f, 3.0f);
        const auto rowHeight = notesArea.getHeight() / (float) span;
        g.setColour (colour.brighter (0.6f));
        for (const auto& note : part.notes)
        {
            const auto nx = area.getX() + (float) (note.start * timeline.pixelsPerBeat);
            const auto nw = juce::jmax (1.5f, (float) (juce::jmin (note.length, part.length - note.start) * timeline.pixelsPerBeat));
            const auto ny = notesArea.getBottom() - (float) (note.pitch - lowest + 1) * rowHeight
                          - (float) (span - (highest - lowest + 1)) * rowHeight * 0.5f;
            g.fillRect (juce::Rectangle<float> (nx, ny, nw, juce::jmax (1.5f, rowHeight - 0.5f)).getIntersection (notesArea));
        }
    }

    if (recording)
    {
        g.setColour (juce::Colour (0xffff3b2f));
        g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.5f);
    }
    else if (selected)
    {
        g.setColour (juce::Colours::white);
        g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.5f);
    }
}

//==============================================================================
void ArrangeView::mouseMove (const juce::MouseEvent& event)
{
    if (const auto hit = partAt (event.position))
    {
        const auto bounds = getPartBounds (hit->first, hit->second);
        setMouseCursor (event.position.x > bounds.getRight() - edgeGrab ? juce::MouseCursor::LeftRightResizeCursor
                                                                          : juce::MouseCursor::NormalCursor);
        return;
    }
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void ArrangeView::mouseDown (const juce::MouseEvent& event)
{
    const auto& project = studio.getProject();
    const auto grid = timeline.grid (project);
    dragMoved = false;
    if (event.position.y < (float) Timeline::rulerHeight)
    {
        dragStartBeat = juce::jmax (0.0, pr7s::snap (timeline.beatForX (event.position.x), grid));
        if (event.position.y < (float) Timeline::cycleStripHeight)
        {
            drag = Drag::cycle;
        }
        else
        {
            drag = Drag::position;
            studio.setPosition (dragStartBeat);
        }
        return;
    }

    const auto hit = partAt (event.position);
    if (! hit)
    {
        drag = Drag::none;
        selection = {};
        if (const auto trackId = trackAtY (event.position.y))
            studio.selectTrack (trackId);
        repaint();
        return;
    }

    select (hit->first, hit->second);
    if (event.mods.isPopupMenu())
    {
        drag = Drag::none;
        showPartMenu (hit->first, hit->second);
        return;
    }
    const auto& part = project.findTrack (hit->first)->parts[(size_t) hit->second];
    const auto bounds = getPartBounds (hit->first, hit->second);
    drag = event.position.x > bounds.getRight() - edgeGrab ? Drag::resize : Drag::move;
    dragCopies = drag == Drag::move && (event.mods.isAltDown() || event.mods.isCommandDown());
    dragTrack = dragTargetTrack = hit->first;
    dragStartBeat = timeline.beatForX (event.position.x);
    dragOriginalStart = dragNewStart = part.start;
    dragOriginalLength = dragNewLength = part.length;
}

void ArrangeView::mouseDrag (const juce::MouseEvent& event)
{
    const auto& project = studio.getProject();
    const auto grid = event.mods.isShiftDown() ? 0.25 : timeline.grid (project);   // Shift: finer, to the sixteenth
    const auto beat = timeline.beatForX (event.position.x);
    switch (drag)
    {
        case Drag::position:
            studio.setPosition (juce::jmax (0.0, pr7s::snap (beat, grid)));
            break;
        case Drag::cycle:
        {
            const auto now = juce::jmax (0.0, pr7s::snap (beat, grid));
            if (std::abs (now - dragStartBeat) >= grid * 0.5)
            {
                dragMoved = true;
                studio.setCycle (juce::jmin (dragStartBeat, now), juce::jmax (dragStartBeat, now));
            }
            break;
        }
        case Drag::move:
            if (event.getDistanceFromDragStart() > 3)
                dragMoved = true;
            dragNewStart = juce::jmax (0.0, pr7s::snap (dragOriginalStart + beat - dragStartBeat, grid));
            if (const auto over = trackAtY (event.position.y))
                dragTargetTrack = over;
            repaint();
            break;
        case Drag::resize:
            dragMoved = true;
            dragNewLength = juce::jmax (grid, pr7s::snap (dragOriginalStart + dragOriginalLength + beat - dragStartBeat, grid) - dragOriginalStart);
            repaint();
            break;
        case Drag::none:
            break;
    }
}

void ArrangeView::mouseUp (const juce::MouseEvent&)
{
    const auto finished = drag;
    drag = Drag::none;
    if (! dragMoved)
    {
        repaint();
        return;
    }
    switch (finished)
    {
        case Drag::cycle:
            studio.setCycleOn (true);
            break;
        case Drag::move:
            if (dragCopies)
            {
                studio.copyPart (dragTrack, selection.index, dragTargetTrack, dragNewStart);
                select (dragTargetTrack, (int) studio.getProject().findTrack (dragTargetTrack)->parts.size() - 1);
            }
            else if (std::abs (dragNewStart - dragOriginalStart) > 1.0e-9 || dragTargetTrack != dragTrack)
            {
                studio.movePart (dragTrack, selection.index, dragTargetTrack, dragNewStart);
                select (dragTargetTrack, (int) studio.getProject().findTrack (dragTargetTrack)->parts.size() - 1);
            }
            break;
        case Drag::resize:
            studio.resizePart (dragTrack, selection.index, dragNewLength);
            break;
        case Drag::position:
        case Drag::none:
            break;
    }
    repaint();
}

void ArrangeView::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (event.position.y < (float) Timeline::rulerHeight || partAt (event.position))
        return;
    // A new empty part, a bar long, from the bar clicked in.
    if (const auto trackId = trackAtY (event.position.y))
    {
        const auto& project = studio.getProject();
        const auto bar = project.beatsPerBar();
        pr7s::Part part;
        part.start = juce::jmax (0.0, std::floor (timeline.beatForX (event.position.x) / bar) * bar);
        part.length = bar;
        part.name = project.findTrack (trackId)->name;
        studio.addPart (trackId, part);
        select (trackId, (int) studio.getProject().findTrack (trackId)->parts.size() - 1);
    }
}

void ArrangeView::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (event.mods.isCommandDown() || event.mods.isCtrlDown())
    {
        // Zoom round the mouse: the beat under it stays put.
        const auto beat = timeline.beatForX (event.position.x);
        timeline.pixelsPerBeat = juce::jlimit (2.0, 400.0, timeline.pixelsPerBeat * (wheel.deltaY > 0.0f ? 1.25 : 0.8));
        timeline.firstBeat = juce::jmax (0.0, beat - event.position.x / timeline.pixelsPerBeat);
    }
    else if (event.mods.isShiftDown() || std::abs (wheel.deltaX) > std::abs (wheel.deltaY))
    {
        const auto delta = std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? wheel.deltaX : wheel.deltaY;
        timeline.firstBeat = juce::jmax (0.0, timeline.firstBeat - delta * 400.0 / timeline.pixelsPerBeat);
    }
    else
    {
        const auto content = (int) studio.getProject().tracks.size() * Timeline::trackHeight;
        const auto room = juce::jmax (0, content - (getHeight() - Timeline::rulerHeight));
        timeline.scrollY = juce::jlimit (0, room, timeline.scrollY - juce::roundToInt (wheel.deltaY * 300.0f));
    }
    if (timeline.onScrollOrZoom)
        timeline.onScrollOrZoom();
    repaint();
}

void ArrangeView::showPartMenu (int trackId, int index)
{
    juce::PopupMenu menu;
    menu.addItem (1, "Duplicate (after it)");
    menu.addItem (2, "Delete");
    menu.showMenuAsync (juce::PopupMenu::Options(), [this, trackId, index] (int chosen)
    {
        selection = { trackId, index };
        if (chosen == 1)
            duplicateSelected();
        else if (chosen == 2)
            deleteSelected();
    });
}
