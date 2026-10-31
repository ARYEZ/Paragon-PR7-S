#include "ArrangeView.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    constexpr float edgeGrab = 6.0f;   // the right edge of a part, for resizing
    const juce::Colour laneDark { 0xff0d0a0e }, laneLight { 0xff120e13 }, gridBar { 0xff2a2229 }, gridBeat { 0xff1a1519 };

    juce::FontOptions font (float height, int style = juce::Font::bold) { return ParagonLookAndFeel::textFont (height, style); }
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
    // Likewise automation points (an undo can change them under the selection).
    if (drag != Drag::points && drag != Drag::draw && drag != Drag::band)
    {
        const auto* lane = laneFor (pointSelection.busId, pointSelection.laneIndex);
        if (lane == nullptr || ! lane->shown || (! pointSelection.points.empty() && pointSelection.points.back() >= (int) lane->points.size()))
            pointSelection = {};
    }
    repaint();
}

void ArrangeView::timerCallback()
{
    followPlayhead();
    repaint();
}

void ArrangeView::followPlayhead()
{
    auto& engine = studio.getEngine();
    const auto position = engine.getPosition();
    const auto jumped = position != lastPosition;
    lastPosition = position;
    // Suspended by a click while playing: until playback stops.
    if (timeline.autoScroll.suspended && ! engine.isPlaying())
    {
        timeline.autoScroll.suspended = false;
        if (timeline.onAutoScrollChange)
            timeline.onAutoScrollChange();
    }
    if (drag != Drag::none)
        return;
    if (const auto first = timeline.autoScroll.follow (position, timeline.firstBeat, timeline.pixelsPerBeat, (float) getWidth(),
                                                       engine.isPlaying(), jumped))
    {
        timeline.firstBeat = *first;
        scrolled();
    }
}

std::optional<double> AutoScroll::follow (double position, double firstBeat, double pixelsPerBeat, float width,
                                          bool playing, bool jumped) const
{
    if (! on || suspended || ! (playing || jumped))
        return std::nullopt;
    const auto x = (float) ((position - firstBeat) * pixelsPerBeat);
    const auto outOfSight = x > width - 30.0f || x < 0.0f;
    if (stationary && (playing || outOfSight))
    {
        // The playhead in the middle (near the song's start, where it can't be, left of it).
        const auto first = juce::jmax (0.0, position - (double) width * 0.5 / pixelsPerBeat);
        return std::abs (first - firstBeat) > 1.0e-9 ? std::optional<double> (first) : std::nullopt;
    }
    if (outOfSight)
        return juce::jmax (0.0, position - 1.0);   // the page turns: the playhead near the left
    return std::nullopt;
}

void AutoScroll::refreshKey (juce::TextButton& key) const
{
    key.setToggleState (on, juce::dontSendNotification);
    key.setButtonText (on && suspended ? "SUSPENDED" : "AUTO-SCROLL");
    if (on && suspended)
        key.getProperties().set (ParagonLookAndFeel::litColourProperty, (juce::int64) 0xffff2f7a);
    else
        key.getProperties().remove (ParagonLookAndFeel::litColourProperty);
    key.setTooltip (on && suspended ? "Auto-Scroll suspended by a click while playing (until playback stops): click (or F) to carry on following"
                                    : "AUTO-SCROLL (F): while it's lit, the view keeps the playhead in sight (the arrow beside it: how)");
    key.repaint();
}

void AutoScroll::showMenu (juce::Component& target, std::function<void()> changed)
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("AUTO-SCROLL");
    menu.addItem (1, "Page Scroll (the page turns at the right edge)", true, ! stationary);
    menu.addItem (2, "Stationary Cursor (the playhead stays in the middle)", true, stationary);
    menu.addSeparator();
    menu.addItem (3, "Suspend Auto-Scroll when Editing", true, suspendWhenEditing);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target),
                        [this, changed = std::move (changed)] (int chosen)
    {
        if (chosen == 1 || chosen == 2)
            stationary = chosen == 2;
        else if (chosen == 3)
            suspendWhenEditing = ! suspendWhenEditing;
        else
            return;
        if (changed)
            changed();
    });
}

void AutoScroll::load (juce::PropertiesFile* settings, const juce::String& prefix)
{
    if (settings == nullptr)
        return;
    on = settings->getBoolValue (prefix + "AutoScroll", true);
    stationary = settings->getBoolValue (prefix + "Stationary", false);
    suspendWhenEditing = settings->getBoolValue (prefix + "SuspendWhenEditing", false);
}

void AutoScroll::save (juce::PropertiesFile* settings, const juce::String& prefix) const
{
    if (settings == nullptr)
        return;
    settings->setValue (prefix + "AutoScroll", on);
    settings->setValue (prefix + "Stationary", stationary);
    settings->setValue (prefix + "SuspendWhenEditing", suspendWhenEditing);
}

void ArrangeView::zoom (bool in)
{
    const auto cursor = studio.getEngine().getPosition();
    timeline.pixelsPerBeat = juce::jlimit (2.0, 400.0, in ? timeline.pixelsPerBeat * Timeline::zoomStep : timeline.pixelsPerBeat / Timeline::zoomStep);
    timeline.firstBeat = juce::jmax (0.0, cursor - (double) getWidth() * 0.5 / timeline.pixelsPerBeat);
    scrolled();
    repaint();
}

void ArrangeView::zoomRows (bool in)
{
    // The rows' content under the top edge stays there (scaled with them).
    const auto before = timeline.rowZoom;
    timeline.rowZoom = juce::jlimit (Timeline::minRowZoom, Timeline::maxRowZoom,
                                     in ? before * Timeline::rowZoomStep : before / Timeline::rowZoomStep);
    const auto room = juce::jmax (0, timeline.contentHeight (studio) - (getHeight() - Timeline::rulerHeight - Timeline::bottomRoom));
    timeline.scrollY = juce::jlimit (0, room, juce::roundToInt ((double) timeline.scrollY * timeline.rowZoom / before));
    scrolled();
    repaint();
}

void ArrangeView::zoomFull()
{
    timeline.pixelsPerBeat = juce::jlimit (2.0, 400.0, (double) juce::jmax (100, getWidth()) / Timeline::scrollLength (studio));
    timeline.firstBeat = 0.0;
    scrolled();
    repaint();
}

void ArrangeView::scrolled()
{
    if (timeline.onScrollOrZoom)
        timeline.onScrollOrZoom();
}

double Timeline::scrollLength (pr7s::Studio& studio)
{
    const auto& project = studio.getProject();
    auto end = project.loopOn ? project.loopEnd : 0.0;
    end = juce::jmax (end, studio.getEngine().getPosition());
    for (const auto& track : project.tracks)
        for (const auto& part : track.parts)
            end = juce::jmax (end, part.start + part.length);
    const auto bar = project.beatsPerBar();
    return juce::jmax (32.0 * bar, std::ceil (end / bar) * bar + 8.0 * bar);
}

std::vector<Timeline::Row> Timeline::layout (const pr7s::Studio& studio) const
{
    std::vector<Row> rows;
    auto top = 0;
    const auto add = [&] (Row::Kind kind, int busId, int laneIndex, int height)
    {
        rows.push_back ({ kind, busId, laneIndex, top, height });
        top += height;
    };
    const auto addLanes = [&] (int busId)
    {
        if (! studio.isAutomationOpen (busId))
            return;
        if (const auto* lanes = studio.getAutomation (busId))
            for (int i = 0; i < (int) lanes->size(); ++i)
                if ((*lanes)[(size_t) i].shown)
                    add (Row::Kind::lane, busId, i, laneRowHeight());
    };
    for (const auto& track : studio.getProject().tracks)
    {
        add (Row::Kind::track, track.id, -1, trackRowHeight());
        addLanes (track.id);
    }
    add (Row::Kind::master, pr7s::Engine::masterBus, -1, masterRowHeight());
    addLanes (pr7s::Engine::masterBus);
    return rows;
}

int Timeline::contentHeight (const pr7s::Studio& studio) const
{
    const auto rows = layout (studio);
    return rows.empty() ? 0 : rows.back().top + rows.back().height;
}

float ArrangeView::rowTop (const Timeline::Row& row) const
{
    return (float) (Timeline::rulerHeight + row.top - timeline.scrollY);
}

float ArrangeView::trackTop (int trackId) const
{
    for (const auto& row : timeline.layout (studio))
        if (row.kind == Timeline::Row::Kind::track && row.busId == trackId)
            return rowTop (row);
    return 0.0f;
}

std::optional<Timeline::Row> ArrangeView::rowAt (float y) const
{
    if (y < (float) Timeline::rulerHeight)
        return std::nullopt;
    for (const auto& row : timeline.layout (studio))
        if (y >= rowTop (row) && y < rowTop (row) + (float) row.height)
            return row;
    return std::nullopt;
}

int ArrangeView::trackAtY (float y) const
{
    const auto row = rowAt (y);
    return row && row->kind == Timeline::Row::Kind::track ? row->busId : 0;
}

juce::Rectangle<float> ArrangeView::getPartBounds (int trackId, int index) const
{
    if (const auto* track = studio.getProject().findTrack (trackId); track != nullptr && juce::isPositiveAndBelow (index, (int) track->parts.size()))
    {
        const auto& part = track->parts[(size_t) index];
        return { timeline.xForBeat (part.start), trackTop (trackId) + 3.0f, (float) (part.length * timeline.pixelsPerBeat), (float) timeline.trackRowHeight() - 6.0f };
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

    // The rows: each track's lane (the selected track's a little lighter),
    // its automation lanes, the master's row.
    const auto rows = timeline.layout (studio);
    auto trackNumber = 0;
    for (const auto& row : rows)
    {
        const auto lane = juce::Rectangle<float> (0.0f, rowTop (row), bounds.getWidth(), (float) row.height);
        if (row.kind == Timeline::Row::Kind::track)
            g.setColour (row.busId == studio.getSelectedTrack() ? juce::Colour (0xff1c151b) : (trackNumber++ % 2 == 0 ? laneDark : laneLight));
        else
            g.setColour (juce::Colour (0xff09070a));
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

    // The automation lanes' curves.
    for (const auto& row : rows)
        if (row.kind == Timeline::Row::Kind::lane)
            paintAutomation (g, row);

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
            const auto isClipDrag = drag == Drag::trimStart || drag == Drag::fadeIn || drag == Drag::fadeOut;
            const auto isDragged = (drag == Drag::move || drag == Drag::resize || isClipDrag) && dragMoved && track.id == dragTrack && i == selection.index;
            if (isDragged && isClipDrag)
            {
                const juce::Rectangle<float> area (timeline.xForBeat (dragPart.start), trackTop (track.id) + 3.0f,
                                                   (float) (dragPart.length * timeline.pixelsPerBeat), (float) timeline.trackRowHeight() - 6.0f);
                paintPart (g, area, dragPart, track.colour, true, false);
                continue;
            }
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
                const juce::Rectangle<float> area (timeline.xForBeat (part.start), trackTop (dragTargetTrack) + 3.0f,
                                                   (float) (part.length * timeline.pixelsPerBeat), (float) timeline.trackRowHeight() - 6.0f);
                paintPart (g, area, part, from->colour, true, false);
            }
    if (recording)
        for (size_t t = 0; t < project.tracks.size(); ++t)
            if (project.tracks[t].id == recording->first)
            {
                const auto& part = recording->second;
                paintPart (g, { timeline.xForBeat (part.start), trackTop (recording->first) + 3.0f, (float) (part.length * timeline.pixelsPerBeat),
                                (float) timeline.trackRowHeight() - 6.0f },
                           part, project.tracks[t].colour, false, true);
            }

    // The tools: the part being drawn, MUTE's rectangle, ZOOM's stretch; SPLIT's cut under the mouse.
    if (! toolBand.isEmpty())
    {
        if (drag == Drag::newPart)
        {
            const auto* track = project.findTrack (newPartTrack);
            const auto colour = track != nullptr ? track->colour : ParagonColours::accent;
            g.setColour (colour.withAlpha (0.35f));
            g.fillRoundedRectangle (toolBand, 4.0f);
            g.setColour (colour);
            g.drawRoundedRectangle (toolBand, 4.0f, 1.5f);
        }
        else
        {
            g.setColour (ParagonColours::accent.withAlpha (0.12f));
            g.fillRect (toolBand);
            g.setColour (ParagonColours::accent.withAlpha (0.8f));
            g.drawRect (toolBand, 1.0f);
        }
    }
    if (tool == Tool::split && drag == Drag::none && hover.x >= 0.0f)
        if (const auto hit = partAt (hover))
        {
            const auto bounds = getPartBounds (hit->first, hit->second);
            const auto beat = pr7s::snap (timeline.beatForX (hover.x), timeline.grid (project));
            const auto x = timeline.xForBeat (beat);
            if (x > bounds.getX() && x < bounds.getRight())
            {
                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.fillRect (juce::Rectangle<float> (x - 0.5f, bounds.getY(), 1.5f, bounds.getHeight()));
            }
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
    if (part.muted)
        colour = juce::Colour (0xff6a646c);   // muted: greyed out (pearl onyx), as the manual has it
    g.setColour (colour.withAlpha (0.32f));
    g.fillRoundedRectangle (area, 4.0f);
    auto band = area.withHeight (15.0f);
    g.setColour (colour.withAlpha (0.85f));
    g.fillRoundedRectangle (band, 4.0f);
    g.fillRect (band.withTrimmedTop (8.0f));
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.setFont (font (10.5f));
    g.drawText (recording ? juce::String ("RECORDING") : part.muted ? part.name + "  (MUTED)" : part.name,
                band.reduced (5.0f, 0.0f), juce::Justification::centredLeft, true);

    // An audio clip: its waveform (the loudest moment under each pixel, up
    // and down from the middle), and its fades with their handles.
    if (part.isAudio())
    {
        const auto wave = area.withTrimmedTop (16.0f).reduced (1.0f, 3.0f);
        if (const auto data = studio.getAudio (part.audioFile))
        {
            const auto secondsPerPixel = studio.getProject().secondsPerBeat() / timeline.pixelsPerBeat;
            const auto peaksPerSecond = data->sampleRate / pr7s::Studio::AudioData::peakStep;
            const auto clip = wave.getIntersection (getLocalBounds().toFloat());
            g.setColour (colour.brighter (0.7f).withAlpha (0.9f));
            for (auto x = std::floor (clip.getX()); x < clip.getRight(); x += 1.0f)
            {
                const auto from = part.audioOffset + (x - area.getX()) * secondsPerPixel;
                const auto first = (int) (from * peaksPerSecond), last = juce::jmax (first + 1, (int) ((from + secondsPerPixel) * peaksPerSecond));
                auto peak = 0.0f;
                for (int i = juce::jmax (0, first); i < juce::jmin (last, (int) data->peaks.size()); ++i)
                    peak = juce::jmax (peak, data->peaks[(size_t) i]);
                const auto height = juce::jmin (1.0f, peak) * wave.getHeight() * 0.5f;
                g.fillRect (juce::Rectangle<float> (x, wave.getCentreY() - height, 1.0f, juce::jmax (0.5f, height * 2.0f)));
            }
        }
        else
        {
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.setFont (font (10.0f, juce::Font::plain));
            g.drawText ("Recording missing: " + part.audioFile, wave, juce::Justification::centred, true);
        }

        // The fades: the faded corners darkened, a line along each, and its handle.
        const auto pixelsPerSecond = (float) (timeline.pixelsPerBeat / studio.getProject().secondsPerBeat());
        const auto inX = area.getX() + (float) part.fadeIn * pixelsPerSecond, outX = area.getRight() - (float) part.fadeOut * pixelsPerSecond;
        if (part.fadeIn > 0.0)
        {
            juce::Path faded;
            faded.addTriangle (area.getX(), wave.getY(), inX, wave.getY(), area.getX(), area.getBottom());
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillPath (faded);
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawLine (area.getX(), area.getBottom() - 2.0f, inX, wave.getY(), 1.2f);
        }
        if (part.fadeOut > 0.0)
        {
            juce::Path faded;
            faded.addTriangle (outX, wave.getY(), area.getRight(), wave.getY(), area.getRight(), area.getBottom());
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillPath (faded);
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawLine (outX, wave.getY(), area.getRight(), area.getBottom() - 2.0f, 1.2f);
        }
        g.setColour (juce::Colours::white);
        for (const auto in : { true, false })
            g.fillRect (fadeHandle (area, part, in));
    }

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

bool ArrangeView::isAudioTrack (int trackId) const
{
    const auto* track = studio.getProject().findTrack (trackId);
    return track != nullptr && track->isAudio();
}

juce::Rectangle<float> ArrangeView::fadeHandle (juce::Rectangle<float> bounds, const pr7s::Part& part, bool fadeIn) const
{
    // A small square just under the name band, at the fade's inner end.
    const auto pixelsPerSecond = (float) (timeline.pixelsPerBeat / studio.getProject().secondsPerBeat());
    const auto x = fadeIn ? bounds.getX() + (float) part.fadeIn * pixelsPerSecond + 4.0f
                          : bounds.getRight() - (float) part.fadeOut * pixelsPerSecond - 4.0f;
    return juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ x, bounds.getY() + 20.0f });
}

//==============================================================================
void ArrangeView::mouseMove (const juce::MouseEvent& event)
{
    updateCursor (event);
}

void ArrangeView::updateCursor (const juce::MouseEvent& event)
{
    if (tool == Tool::split)
    {
        hover = partAt (event.position) ? event.position : juce::Point<float> { -1.0f, -1.0f };
        repaint();
    }
    if (tool != Tool::select)
    {
        const auto overParts = event.position.y >= (float) Timeline::rulerHeight;
        setMouseCursor (! overParts ? juce::MouseCursor::NormalCursor
                        : tool == Tool::split ? juce::MouseCursor::IBeamCursor
                        : tool == Tool::draw || tool == Tool::zoom ? juce::MouseCursor::CrosshairCursor
                                                                   : juce::MouseCursor::PointingHandCursor);
        return;
    }
    if (const auto hit = partAt (event.position))
    {
        const auto bounds = getPartBounds (hit->first, hit->second);
        const auto& part = studio.getProject().findTrack (hit->first)->parts[(size_t) hit->second];
        const auto onHandle = part.isAudio() && (fadeHandle (bounds, part, true).expanded (3.0f).contains (event.position)
                                                 || fadeHandle (bounds, part, false).expanded (3.0f).contains (event.position));
        const auto onEdge = event.position.x > bounds.getRight() - edgeGrab || (part.isAudio() && event.position.x < bounds.getX() + edgeGrab);
        setMouseCursor (onHandle ? juce::MouseCursor::PointingHandCursor
                        : onEdge ? juce::MouseCursor::LeftRightResizeCursor
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
    // Suspend Auto-Scroll when Editing: a click in the tracks while playing.
    if (timeline.autoScroll.on && timeline.autoScroll.suspendWhenEditing && ! timeline.autoScroll.suspended
        && studio.getEngine().isPlaying() && event.position.y >= (float) Timeline::rulerHeight)
    {
        timeline.autoScroll.suspended = true;
        if (timeline.onAutoScrollChange)
            timeline.onAutoScrollChange();
    }
    if (const auto row = rowAt (event.position.y); row && row->kind == Timeline::Row::Kind::lane && automationDown (event, *row))
        return;
    if (! event.mods.isShiftDown())
        pointSelection = {};
    if (event.position.y < (float) Timeline::rulerHeight)
    {
        dragStartBeat = juce::jmax (0.0, pr7s::snap (timeline.beatForX (event.position.x), grid));
        if (event.position.y < (float) Timeline::cycleStripHeight)
        {
            // Inside the locator range: a click turns the cycle on or off, a
            // drag moves the range; outside it, a drag sets a new one.
            const auto at = timeline.beatForX (event.position.x);
            drag = at >= project.loopStart && at <= project.loopEnd ? Drag::cycleMove : Drag::cycle;
            dragStartBeat = drag == Drag::cycleMove ? at : dragStartBeat;
            dragOriginalStart = project.loopStart;
            dragOriginalLength = project.loopEnd - project.loopStart;
        }
        else
        {
            drag = Drag::position;
            studio.setPosition (dragStartBeat);
        }
        return;
    }

    if (toolDown (event))
        return;
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
    if (part.isAudio())
    {
        if (fadeHandle (bounds, part, true).expanded (3.0f).contains (event.position))
            drag = Drag::fadeIn;
        else if (fadeHandle (bounds, part, false).expanded (3.0f).contains (event.position))
            drag = Drag::fadeOut;
        else if (event.position.x < bounds.getX() + edgeGrab)
            drag = Drag::trimStart;
    }
    dragPart = part;
    dragCopies = drag == Drag::move && event.mods.isAltDown();   // Alt copies (Ctrl locks the direction)
    dragTrack = dragTargetTrack = hit->first;
    dragStartBeat = timeline.beatForX (event.position.x);
    dragOriginalStart = dragNewStart = part.start;
    dragOriginalLength = dragNewLength = part.length;
}

void ArrangeView::mouseDrag (const juce::MouseEvent& event)
{
    if (drag == Drag::points || drag == Drag::band || drag == Drag::draw)
    {
        automationDrag (event);
        return;
    }
    if (drag == Drag::newPart || drag == Drag::muteBand || drag == Drag::zoomBand)
    {
        if (event.getDistanceFromDragStart() > 3)
            dragMoved = true;
        const auto from = event.mouseDownPosition;
        if (drag == Drag::muteBand)
            toolBand = juce::Rectangle<float> (from, event.position);
        else
        {
            // Across the song: DRAW's on its track (snapped), ZOOM's down the view.
            auto start = timeline.beatForX (juce::jmin (from.x, event.position.x));
            auto end = timeline.beatForX (juce::jmax (from.x, event.position.x));
            if (drag == Drag::newPart && ! event.mods.isCommandDown())
            {
                const auto grid = timeline.grid (studio.getProject());
                start = std::floor (start / grid + 1.0e-9) * grid;
                end = juce::jmax (start + grid, std::ceil (end / grid - 1.0e-9) * grid);
            }
            start = juce::jmax (0.0, start);
            const auto x1 = timeline.xForBeat (start), x2 = timeline.xForBeat (end);
            toolBand = drag == Drag::newPart ? juce::Rectangle<float> (x1, trackTop (newPartTrack) + 3.0f, x2 - x1, (float) timeline.trackRowHeight() - 6.0f)
                                             : juce::Rectangle<float> (x1, (float) Timeline::rulerHeight, x2 - x1, (float) getHeight());
        }
        repaint();
        return;
    }
    const auto& project = studio.getProject();
    // Ctrl: no snapping (while moving a part, Ctrl+Shift: Ctrl alone keeps it to one direction).
    const auto noSnap = event.mods.isCommandDown() && (drag != Drag::move || event.mods.isShiftDown());
    const auto grid = noSnap ? 0.0 : timeline.grid (project);
    const auto beat = timeline.beatForX (event.position.x);
    switch (drag)
    {
        case Drag::position:
            studio.setPosition (juce::jmax (0.0, pr7s::snap (beat, grid)));
            break;
        case Drag::cycle:
        {
            const auto now = juce::jmax (0.0, pr7s::snap (beat, grid));
            if (std::abs (now - dragStartBeat) >= juce::jmax (grid, 1.0 / 16.0) * 0.5)
            {
                dragMoved = true;
                studio.setCycle (juce::jmin (dragStartBeat, now), juce::jmax (dragStartBeat, now));
            }
            break;
        }
        case Drag::cycleMove:
            if (event.getDistanceFromDragStart() > 3)
            {
                dragMoved = true;
                const auto start = juce::jmax (0.0, pr7s::snap (dragOriginalStart + beat - dragStartBeat, grid));
                studio.setCycle (start, start + dragOriginalLength);
            }
            break;
        case Drag::move:
        {
            if (event.getDistanceFromDragStart() > 3)
                dragMoved = true;
            // Ctrl: only along the song, or only to another track, whichever the mouse has gone further.
            const auto offset = event.getOffsetFromDragStart();
            const auto locked = event.mods.isCommandDown();
            const auto alongOnly = locked && std::abs (offset.x) >= std::abs (offset.y);
            const auto acrossOnly = locked && ! alongOnly;
            dragNewStart = acrossOnly ? dragOriginalStart : juce::jmax (0.0, pr7s::snap (dragOriginalStart + beat - dragStartBeat, grid));
            // To another track of the same kind (a clip onto an audio track, a MIDI part onto an instrument's).
            dragTargetTrack = dragTrack;
            const auto* overTrack = studio.getProject().findTrack (trackAtY (event.position.y));
            if (overTrack != nullptr && ! alongOnly && ! overTrack->isChannel() && overTrack->isAudio() == isAudioTrack (dragTrack))
                dragTargetTrack = overTrack->id;   // (not an FX or group channel: no parts there)
            repaint();
            break;
        }
        case Drag::resize:
            dragMoved = true;
            dragNewLength = juce::jmax (grid > 0.0 ? grid : 1.0 / 16.0, pr7s::snap (dragOriginalStart + dragOriginalLength + beat - dragStartBeat, grid) - dragOriginalStart);
            if (dragPart.isAudio())
                if (const auto data = studio.getAudio (dragPart.audioFile))   // no further than the recording goes
                    dragNewLength = juce::jmin (dragNewLength, (data->seconds() - dragPart.audioOffset) / project.secondsPerBeat());
            repaint();
            break;
        case Drag::trimStart:
        {
            // The start moves; the audio stays where it is in the song (so the offset into it moves too).
            dragMoved = true;
            const auto spb = project.secondsPerBeat();
            const auto end = dragOriginalStart + dragOriginalLength;
            const auto earliest = juce::jmax (0.0, dragOriginalStart - studio.getProject().findTrack (dragTrack)->parts[(size_t) selection.index].audioOffset / spb);
            const auto start = juce::jlimit (earliest, end - 1.0 / 16.0, pr7s::snap (dragOriginalStart + beat - dragStartBeat, grid));
            const auto& original = studio.getProject().findTrack (dragTrack)->parts[(size_t) selection.index];
            dragPart.start = start;
            dragPart.length = end - start;
            dragPart.audioOffset = juce::jmax (0.0, original.audioOffset + (start - dragOriginalStart) * spb);
            dragPart.fadeIn = juce::jmin (dragPart.fadeIn, dragPart.length * spb);
            repaint();
            break;
        }
        case Drag::fadeIn:
        case Drag::fadeOut:
        {
            dragMoved = true;
            const auto spb = project.secondsPerBeat();
            const auto clipSeconds = dragPart.length * spb;
            if (drag == Drag::fadeIn)
                dragPart.fadeIn = juce::jlimit (0.0, clipSeconds - dragPart.fadeOut, (beat - dragPart.start) * spb);
            else
                dragPart.fadeOut = juce::jlimit (0.0, clipSeconds - dragPart.fadeIn, (dragPart.end() - beat) * spb);
            repaint();
            break;
        }
        case Drag::points:
        case Drag::band:
        case Drag::draw:
        case Drag::newPart:
        case Drag::muteBand:
        case Drag::zoomBand:
        case Drag::play:
        case Drag::none:
            break;
    }
}

void ArrangeView::mouseUp (const juce::MouseEvent& event)
{
    if (drag == Drag::points || drag == Drag::band || drag == Drag::draw)
    {
        automationUp();
        return;
    }
    if (drag == Drag::newPart || drag == Drag::muteBand || drag == Drag::zoomBand || drag == Drag::play)
    {
        toolUp (event);
        return;
    }
    const auto finished = drag;
    drag = Drag::none;
    // SELECT, Alt-click (no drag) on a part: split it there (as the manual has it).
    if (! dragMoved && finished == Drag::move && dragCopies && selection.isValid())
    {
        if (const auto* track = studio.getProject().findTrack (dragTrack); track != nullptr && juce::isPositiveAndBelow (selection.index, (int) track->parts.size()))
            studio.splitPart (dragTrack, selection.index, splitBeat (event, track->parts[(size_t) selection.index]));
        repaint();
        return;
    }
    if (! dragMoved)
    {
        if (finished == Drag::cycleMove)   // a click in the range: cycle on or off
            studio.setCycleOn (! studio.getProject().loopOn);
        repaint();
        return;
    }
    switch (finished)
    {
        case Drag::cycle:
            studio.setCycleOn (true);
            break;
        case Drag::cycleMove:
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
        case Drag::trimStart:
        case Drag::fadeIn:
        case Drag::fadeOut:
            studio.updatePart (dragTrack, selection.index, dragPart);
            break;
        case Drag::position:
        case Drag::points:
        case Drag::band:
        case Drag::draw:
        case Drag::newPart:
        case Drag::muteBand:
        case Drag::zoomBand:
        case Drag::play:
        case Drag::none:
            break;
    }
    repaint();
}

//==============================================================================
const char* ArrangeView::toolName (Tool t)
{
    switch (t)
    {
        case Tool::select: return "SELECT";
        case Tool::split:  return "SPLIT";
        case Tool::glue:   return "GLUE";
        case Tool::erase:  return "ERASE";
        case Tool::zoom:   return "ZOOM";
        case Tool::mute:   return "MUTE";
        case Tool::draw:   return "DRAW";
        case Tool::play:   return "PLAY";
    }
    return "";
}

int ArrangeView::toolKey (Tool t)
{
    switch (t)
    {
        case Tool::select: return 1;
        case Tool::split:  return 3;
        case Tool::glue:   return 4;
        case Tool::erase:  return 5;
        case Tool::zoom:   return 6;
        case Tool::mute:   return 7;
        case Tool::draw:   return 8;
        case Tool::play:   return 9;
    }
    return 0;
}

void ArrangeView::setTool (Tool newTool)
{
    tool = newTool;
    hover = { -1.0f, -1.0f };
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
    if (onToolChange)
        onToolChange();
}

void ArrangeView::nextTool (bool forward)
{
    const auto count = (int) std::size (tools);
    auto at = 0;
    for (int i = 0; i < count; ++i)
        if (tools[i] == tool)
            at = i;
    setTool (tools[(at + (forward ? 1 : count - 1)) % count]);
}

void ArrangeView::splitAtCursor()
{
    // Only the selected part, if there is one (as the manual has it).
    auto only = 0;
    if (const auto* track = studio.getProject().findTrack (selection.trackId); track != nullptr && juce::isPositiveAndBelow (selection.index, (int) track->parts.size()))
        only = track->parts[(size_t) selection.index].id;
    studio.splitAtCursor (only);
}

double ArrangeView::splitBeat (const juce::MouseEvent& event, const pr7s::Part&) const
{
    // Snapped to the grid (Ctrl: not).
    const auto beat = timeline.beatForX (event.position.x);
    return event.mods.isCommandDown() ? beat : pr7s::snap (beat, timeline.grid (studio.getProject()));
}

void ArrangeView::cannotGlue()
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Not glued",
        "Only a MIDI part and the ones after it, or an audio clip and the one cut from the same recording "
        "right after it (a split glued back) can be glued. A clip of another recording would need an audio part "
        "holding several clips, which PR7-S doesn't have yet.");
}

bool ArrangeView::toolDown (const juce::MouseEvent& event)
{
    const auto hit = partAt (event.position);
    const auto* track = studio.getProject().findTrack (trackAtY (event.position.y));
    const auto canHoldNotes = track != nullptr && ! track->isAudio() && ! track->isChannel();
    dragMoved = false;
    toolBand = {};
    switch (tool)
    {
        case Tool::select:
            // Alt-drag on an empty instrument track draws a new part (the manual's).
            if (! hit && event.mods.isAltDown() && canHoldNotes)
            {
                drag = Drag::newPart;
                newPartTrack = track->id;
                return true;
            }
            return false;
        case Tool::split:
            if (hit)
            {
                const auto& part = studio.getProject().findTrack (hit->first)->parts[(size_t) hit->second];
                const auto at = splitBeat (event, part);
                if (event.mods.isAltDown())
                    studio.splitPartRepeatedly (hit->first, hit->second, at);
                else
                    studio.splitPart (hit->first, hit->second, at);
                selection = { hit->first, hit->second };   // the first half
            }
            break;
        case Tool::glue:
            if (hit)
            {
                const auto audio = isAudioTrack (hit->first);
                if (studio.glueParts (hit->first, hit->second, event.mods.isAltDown()))
                    selection = { hit->first, hit->second };
                else if (audio)
                    cannotGlue();
            }
            break;
        case Tool::erase:
            if (hit)
            {
                selection = {};
                if (event.mods.isAltDown())
                    studio.deletePartsFrom (hit->first, hit->second);
                else
                    studio.deletePart (hit->first, hit->second);
            }
            break;
        case Tool::mute:
            drag = Drag::muteBand;
            return true;
        case Tool::zoom:
            drag = Drag::zoomBand;
            return true;
        case Tool::draw:
            if (! hit && canHoldNotes)
            {
                drag = Drag::newPart;
                newPartTrack = track->id;
                return true;
            }
            if (hit)
                select (hit->first, hit->second);
            break;
        case Tool::play:
            // Held: plays from here; let go: stops, back where it was.
            playReturn = studio.getEngine().getPosition();
            if (studio.getEngine().isPlaying())
                studio.stop();
            studio.setPosition (juce::jmax (0.0, timeline.beatForX (event.position.x)));
            studio.play();
            drag = Drag::play;
            return true;
    }
    drag = Drag::none;
    repaint();
    return true;
}

void ArrangeView::toolUp (const juce::MouseEvent& event)
{
    const auto finished = drag;
    drag = Drag::none;
    switch (finished)
    {
        case Drag::play:
            studio.stop();
            studio.setPosition (playReturn);
            break;
        case Drag::newPart:
            if (dragMoved && ! toolBand.isEmpty())
            {
                pr7s::Part part;
                part.start = juce::jmax (0.0, timeline.beatForX (toolBand.getX()));
                part.length = juce::jmax (1.0 / 16.0, timeline.beatForX (toolBand.getRight()) - part.start);
                part.name = studio.getProject().findTrack (newPartTrack)->name;
                studio.addPart (newPartTrack, part);
                select (newPartTrack, (int) studio.getProject().findTrack (newPartTrack)->parts.size() - 1);
            }
            break;
        case Drag::muteBand:
        {
            // A click: that part muted or not; a rectangle: every part it touches
            // muted (or, if they all are already, unmuted).
            std::vector<int> ids;
            auto allMuted = true;
            const auto band = dragMoved ? toolBand : juce::Rectangle<float> (event.position, event.position).expanded (0.5f);
            for (const auto& track : studio.getProject().tracks)
                for (int i = 0; i < (int) track.parts.size(); ++i)
                    if (getPartBounds (track.id, i).intersects (band))
                    {
                        ids.push_back (track.parts[(size_t) i].id);
                        allMuted = allMuted && track.parts[(size_t) i].muted;
                    }
            if (! ids.empty())
                studio.setPartsMuted (ids, ! allMuted);
            break;
        }
        case Drag::zoomBand:
            if (dragMoved && toolBand.getWidth() > 4.0f)
            {
                // The stretch dragged across fills the view.
                const auto start = timeline.beatForX (toolBand.getX());
                const auto length = timeline.beatForX (toolBand.getRight()) - start;
                timeline.pixelsPerBeat = juce::jlimit (2.0, 400.0, (double) getWidth() / juce::jmax (1.0e-3, length));
                timeline.firstBeat = juce::jmax (0.0, start);
            }
            else
            {
                // A click: a step in (Alt: out), the beat clicked staying under the mouse.
                const auto beat = timeline.beatForX (event.position.x);
                timeline.pixelsPerBeat = juce::jlimit (2.0, 400.0, event.mods.isAltDown() ? timeline.pixelsPerBeat / Timeline::zoomStep
                                                                                         : timeline.pixelsPerBeat * Timeline::zoomStep);
                timeline.firstBeat = juce::jmax (0.0, beat - event.position.x / timeline.pixelsPerBeat);
            }
            scrolled();
            break;
        default:
            break;
    }
    toolBand = {};
    repaint();
}

void ArrangeView::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (event.position.y < (float) Timeline::rulerHeight || trackAtY (event.position.y) == 0)
        return;   // (the ruler, an automation lane, the master's row)
    if (const auto hit = partAt (event.position))
    {
        select (hit->first, hit->second);
        if (onEditNotes && ! isAudioTrack (hit->first))   // a clip has no notes
            onEditNotes();
        return;
    }
    const auto* clicked = studio.getProject().findTrack (trackAtY (event.position.y));
    if (clicked == nullptr || clicked->isAudio() || clicked->isChannel())
        return;   // an empty clip would play nothing; an FX or group channel has no parts
    // A new empty part: between the locators, from the left one to the
    // right; elsewhere, a bar long, from the bar clicked in.
    if (const auto trackId = trackAtY (event.position.y))
    {
        const auto& project = studio.getProject();
        const auto bar = project.beatsPerBar();
        const auto at = timeline.beatForX (event.position.x);
        pr7s::Part part;
        if (at >= project.loopStart && at < project.loopEnd)
        {
            part.start = project.loopStart;
            part.length = project.loopEnd - project.loopStart;
        }
        else
        {
            part.start = juce::jmax (0.0, std::floor (at / bar) * bar);
            part.length = bar;
        }
        part.name = project.findTrack (trackId)->name;
        studio.addPart (trackId, part);
        select (trackId, (int) studio.getProject().findTrack (trackId)->parts.size() - 1);
    }
}

void ArrangeView::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if ((event.mods.isCommandDown() || event.mods.isCtrlDown()) && event.mods.isShiftDown())
    {
        zoomRows (wheel.deltaY > 0.0f);   // Ctrl+Shift+wheel: taller or shorter rows (the manual's)
        return;
    }
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
        const auto room = juce::jmax (0, timeline.contentHeight (studio) - (getHeight() - Timeline::rulerHeight - Timeline::bottomRoom));
        timeline.scrollY = juce::jlimit (0, room, timeline.scrollY - juce::roundToInt (wheel.deltaY * 300.0f));
    }
    scrolled();
    repaint();
}

void ArrangeView::showPartMenu (int trackId, int index)
{
    juce::PopupMenu menu;
    menu.addItem (3, "Edit notes (key editor)", ! isAudioTrack (trackId));
    menu.addItem (1, "Duplicate (after it)");
    menu.addItem (2, "Delete");
    menu.addSeparator();
    const auto& part = studio.getProject().findTrack (trackId)->parts[(size_t) index];
    const auto at = studio.getEngine().getPosition();
    menu.addItem (4, part.muted ? "Unmute" : "Mute");
    menu.addItem (5, "Split at the playhead (Alt+X)", at > part.start && at < part.end());
    menu.addItem (6, "Glue to the next part");
    menu.showMenuAsync (juce::PopupMenu::Options(), [this, trackId, index, muted = part.muted] (int chosen)
    {
        selection = { trackId, index };
        if (chosen == 3 && onEditNotes)
            onEditNotes();
        else if (chosen == 1)
            duplicateSelected();
        else if (chosen == 2)
            deleteSelected();
        else if (chosen == 4)
            studio.setPartMuted (trackId, index, ! muted);
        else if (chosen == 5)
            splitAtCursor();
        else if (chosen == 6 && ! studio.glueParts (trackId, index, false) && isAudioTrack (trackId))
            cannotGlue();
    });
}

//==============================================================================
// Automation lanes.
namespace
{
    constexpr float pointSize = 7.0f, pointGrab = 6.0f, laneInset = 5.0f;
}

const pr7s::AutomationLane* ArrangeView::laneFor (int busId, int laneIndex) const
{
    const auto* lanes = studio.getAutomation (busId);
    return lanes != nullptr && juce::isPositiveAndBelow (laneIndex, (int) lanes->size()) ? &(*lanes)[(size_t) laneIndex] : nullptr;
}

juce::Rectangle<float> ArrangeView::getLaneArea (int busId, int laneIndex) const
{
    for (const auto& row : timeline.layout (studio))
        if (row.kind == Timeline::Row::Kind::lane && row.busId == busId && row.laneIndex == laneIndex)
            return juce::Rectangle<float> (0.0f, rowTop (row), (float) getWidth(), (float) row.height).reduced (0.0f, laneInset);
    return {};
}

float ArrangeView::yForValue (juce::Rectangle<float> area, float value) const
{
    return area.getBottom() - juce::jlimit (0.0f, 1.0f, value) * area.getHeight();
}

float ArrangeView::valueForY (juce::Rectangle<float> area, float y) const
{
    return juce::jlimit (0.0f, 1.0f, (area.getBottom() - y) / juce::jmax (1.0f, area.getHeight()));
}

juce::Point<float> ArrangeView::getPointPosition (int busId, int laneIndex, int point) const
{
    const auto* lane = laneFor (busId, laneIndex);
    if (lane == nullptr || ! juce::isPositiveAndBelow (point, (int) lane->points.size()))
        return {};
    const auto area = getLaneArea (busId, laneIndex);
    const auto& p = lane->points[(size_t) point];
    return { timeline.xForBeat (p.beat), yForValue (area, p.value) };
}

void ArrangeView::paintAutomation (juce::Graphics& g, const Timeline::Row& row) const
{
    const auto* lane = laneFor (row.busId, row.laneIndex);
    if (lane == nullptr)
        return;
    const auto area = juce::Rectangle<float> (0.0f, rowTop (row), (float) getWidth(), (float) row.height).reduced (0.0f, laneInset);
    const auto* track = studio.getProject().findTrack (row.busId);
    const auto active = studio.getAutomationRead (row.busId) && ! lane->muted;
    const auto colour = active ? (track != nullptr ? track->colour : ParagonColours::accent) : ParagonColours::textDim.withAlpha (0.6f);
    const auto editing = (drag == Drag::points || drag == Drag::draw) && pointSelection.busId == row.busId && pointSelection.laneIndex == row.laneIndex;
    const auto& points = editing ? workingPoints : lane->points;

    if (points.empty())
    {
        // The static value line: the control's setting, dotted.
        const auto y = yForValue (area, studio.getControlValue (row.busId, *lane));
        g.setColour (ParagonColours::textDim);
        for (auto x = 0.0f; x < area.getRight(); x += 6.0f)
            g.fillRect (x, y, 3.0f, 1.0f);
        return;
    }

    // The curve: a ramp between points (a step for an on/off switch), flat before the first and after the last.
    pr7s::AutomationLane shape = *lane;
    shape.points = points;
    juce::Path curve;
    curve.startNewSubPath (0.0f, yForValue (area, points.front().value));
    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto x = timeline.xForBeat (points[i].beat);
        if (lane->isStep() && i > 0)
            curve.lineTo (x, yForValue (area, points[i - 1].value));
        curve.lineTo (x, yForValue (area, points[i].value));
    }
    curve.lineTo (area.getRight(), yForValue (area, points.back().value));
    auto fill = curve;
    fill.lineTo (area.getRight(), area.getBottom());
    fill.lineTo (0.0f, area.getBottom());
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (colour);
    g.strokePath (curve, juce::PathStrokeType (1.6f));

    // The points: selected ones dark (as the manual has them).
    const auto selectedHere = pointSelection.busId == row.busId && pointSelection.laneIndex == row.laneIndex;
    for (int i = 0; i < (int) points.size(); ++i)
    {
        const auto at = juce::Point<float> (timeline.xForBeat (points[(size_t) i].beat), yForValue (area, points[(size_t) i].value));
        if (at.x < -pointSize || at.x > (float) getWidth() + pointSize)
            continue;
        const auto box = juce::Rectangle<float> (pointSize, pointSize).withCentre (at);
        const auto selected = selectedHere && std::binary_search (pointSelection.points.begin(), pointSelection.points.end(), i);
        g.setColour (selected ? juce::Colour (0xff050405) : colour);
        g.fillRect (box);
        g.setColour (selected ? colour : juce::Colour (0xff050405));
        g.drawRect (box, 1.0f);
    }
    if (drag == Drag::band && selectedHere)
    {
        g.setColour (ParagonColours::accent.withAlpha (0.15f));
        g.fillRect (pointBand);
        g.setColour (ParagonColours::accent);
        g.drawRect (pointBand, 1.0f);
    }
}

bool ArrangeView::automationDown (const juce::MouseEvent& event, const Timeline::Row& row)
{
    const auto* lane = laneFor (row.busId, row.laneIndex);
    if (lane == nullptr)
        return false;
    selection = {};
    if (event.mods.isPopupMenu())
    {
        drag = Drag::none;
        showLaneMenu (row.busId, row.laneIndex);
        return true;
    }
    const auto area = getLaneArea (row.busId, row.laneIndex);
    const auto sameLane = pointSelection.busId == row.busId && pointSelection.laneIndex == row.laneIndex;
    if (! sameLane)
        pointSelection = { row.busId, row.laneIndex, {} };
    originalPoints = workingPoints = lane->points;
    pointDownAt = event.position;
    dragMoved = false;
    copyingPoints = false;

    // On a point: select it (Shift adds or removes) and drag it (with the others selected).
    anchor = -1;
    for (int i = (int) lane->points.size() - 1; i >= 0; --i)
        if (getPointPosition (row.busId, row.laneIndex, i).getDistanceFrom (event.position) <= pointGrab)
        {
            anchor = i;
            break;
        }
    auto& chosen = pointSelection.points;
    if (anchor >= 0)
    {
        const auto already = std::binary_search (chosen.begin(), chosen.end(), anchor);
        if (event.mods.isShiftDown())
        {
            if (already)
                chosen.erase (std::find (chosen.begin(), chosen.end(), anchor));
            else
                chosen.insert (std::upper_bound (chosen.begin(), chosen.end(), anchor), anchor);
            drag = Drag::none;
            repaint();
            return true;
        }
        if (! already)
            chosen = { anchor };
        copyingPoints = event.mods.isAltDown();
        drag = Drag::points;
        repaint();
        return true;
    }

    // Alt: draw several; otherwise a click adds one (on release), a drag selects.
    if (event.mods.isAltDown())
    {
        drag = Drag::draw;
        chosen.clear();
        automationDrag (event);
        return true;
    }
    if (! event.mods.isShiftDown())
        chosen.clear();
    pointBand = { event.position, event.position };
    drag = Drag::band;
    juce::ignoreUnused (area);
    repaint();
    return true;
}

void ArrangeView::automationDrag (const juce::MouseEvent& event)
{
    const auto* lane = laneFor (pointSelection.busId, pointSelection.laneIndex);
    if (lane == nullptr)
        return;
    const auto area = getLaneArea (pointSelection.busId, pointSelection.laneIndex);
    if (event.getDistanceFromDragStart() > 2)
        dragMoved = true;
    if (drag == Drag::band)
    {
        {
            pointBand = juce::Rectangle<float> (pointDownAt, event.position);
            std::vector<int> inside;
            for (int i = 0; i < (int) lane->points.size(); ++i)
                if (pointBand.contains (getPointPosition (pointSelection.busId, pointSelection.laneIndex, i)))
                    inside.push_back (i);
            pointSelection.points = inside;
        }
    }
    else if (drag == Drag::points)
    {
        {
            if (! dragMoved || anchor < 0)
                return;
            // The grabbed point to the mouse (snapped along the song), the others with it.
            const auto& grabbed = originalPoints[(size_t) anchor];
            const auto ctrl = event.mods.isCommandDown();
            const auto noSnap = ctrl && (event.mods.isShiftDown() || event.mods.isAltDown());
            const auto offset = event.position - pointDownAt;
            const auto alongOnly = ctrl && std::abs (offset.x) >= std::abs (offset.y);
            const auto upDownOnly = ctrl && ! alongOnly;
            auto beat = grabbed.beat + offset.x / timeline.pixelsPerBeat;
            if (! noSnap)
                beat = pr7s::snap (beat, timeline.grid (studio.getProject()));
            const auto beatShift = upDownOnly ? 0.0 : beat - grabbed.beat;
            const auto valueShift = alongOnly ? 0.0f : valueForY (area, yForValue (area, grabbed.value) + offset.y) - grabbed.value;
            // The moved points replace what's in the stretch they land on (as the manual has it).
            std::vector<pr7s::AutomationPoint> moved;
            for (const auto i : pointSelection.points)
            {
                auto p = originalPoints[(size_t) i];
                p.beat = juce::jmax (0.0, p.beat + beatShift);
                p.value = juce::jlimit (0.0f, 1.0f, p.value + valueShift);
                moved.push_back (p);
            }
            auto from = std::numeric_limits<double>::max(), to = -1.0;
            for (const auto& p : moved)
            {
                from = juce::jmin (from, p.beat);
                to = juce::jmax (to, p.beat);
            }
            workingPoints.clear();
            for (int i = 0; i < (int) originalPoints.size(); ++i)
            {
                const auto isMoved = std::binary_search (pointSelection.points.begin(), pointSelection.points.end(), i);
                const auto& p = originalPoints[(size_t) i];
                if ((isMoved && ! copyingPoints) || (p.beat >= from && p.beat <= to && ! isMoved))
                    continue;
                workingPoints.push_back (p);
            }
            for (const auto& p : moved)
                workingPoints.push_back (p);
            std::stable_sort (workingPoints.begin(), workingPoints.end(), [] (const auto& a, const auto& b) { return a.beat < b.beat; });
        }
    }
    else if (drag == Drag::draw)
    {
        {
            // Drawing: a point where the mouse is; it replaces what was between where the stroke has been.
            const pr7s::AutomationPoint p { juce::jmax (0.0, timeline.beatForX (event.position.x)), valueForY (area, event.position.y) };
            const auto startBeat = juce::jmax (0.0, timeline.beatForX (pointDownAt.x));
            const auto from = juce::jmin (startBeat, p.beat), to = juce::jmax (startBeat, p.beat);
            std::vector<pr7s::AutomationPoint> kept;
            for (const auto& q : workingPoints)
                if (q.beat < from || q.beat > to || std::find (drawn.begin(), drawn.end(), q) != drawn.end())
                    kept.push_back (q);
            drawn.push_back (p);
            kept.push_back (p);
            std::stable_sort (kept.begin(), kept.end(), [] (const auto& a, const auto& b) { return a.beat < b.beat; });
            workingPoints = std::move (kept);
        }
    }
    repaint();
}

void ArrangeView::automationUp()
{
    const auto finished = drag;
    drag = Drag::none;
    const auto* lane = laneFor (pointSelection.busId, pointSelection.laneIndex);
    if (lane == nullptr)
        return;
    if (finished == Drag::band && ! dragMoved && ! pointDownAt.isOrigin())
    {
        // A click: a point there (snapped along the song), unless it's on the
        // line between two points already (as the manual has it).
        const auto area = getLaneArea (pointSelection.busId, pointSelection.laneIndex);
        const pr7s::AutomationPoint p { juce::jmax (0.0, pr7s::snap (timeline.beatForX (pointDownAt.x), timeline.grid (studio.getProject()))),
                                        valueForY (area, pointDownAt.y) };
        auto points = lane->points;
        const auto between = ! points.empty() && p.beat > points.front().beat && p.beat < points.back().beat;
        if (between && ! lane->isStep() && std::abs (*lane->valueAt (p.beat) - p.value) < 0.015f)
            return;
        points.push_back (p);
        commitPoints (std::move (points), { p });
        return;
    }
    if (finished == Drag::points && dragMoved)
    {
        std::vector<pr7s::AutomationPoint> selected;
        for (const auto i : pointSelection.points)
            if (juce::isPositiveAndBelow (i, (int) originalPoints.size()))
                selected.push_back (originalPoints[(size_t) i]);
        // (The moved ones are found again by where they went.)
        std::vector<pr7s::AutomationPoint> movedTo;
        for (const auto& p : workingPoints)
            if (std::find (originalPoints.begin(), originalPoints.end(), p) == originalPoints.end())
                movedTo.push_back (p);
        commitPoints (workingPoints, movedTo);
        return;
    }
    if (finished == Drag::draw)
    {
        // Thinned: a drawn point on the line between its neighbours adds nothing.
        std::vector<pr7s::AutomationPoint> thin;
        for (size_t i = 0; i < workingPoints.size(); ++i)
        {
            const auto isDrawn = std::find (drawn.begin(), drawn.end(), workingPoints[i]) != drawn.end();
            if (isDrawn && ! thin.empty() && i + 1 < workingPoints.size())
            {
                const auto& a = thin.back();
                const auto& b = workingPoints[i + 1];
                const auto along = b.beat > a.beat ? (workingPoints[i].beat - a.beat) / (b.beat - a.beat) : 0.0;
                if (std::abs (workingPoints[i].value - (a.value + (b.value - a.value) * (float) along)) < 0.01f)
                    continue;
            }
            thin.push_back (workingPoints[i]);
        }
        drawn.clear();
        commitPoints (std::move (thin), {});
        return;
    }
    repaint();
}

void ArrangeView::commitPoints (std::vector<pr7s::AutomationPoint> points, const std::vector<pr7s::AutomationPoint>& selected)
{
    const auto busId = pointSelection.busId, laneIndex = pointSelection.laneIndex;
    studio.setAutomationPoints (busId, laneIndex, std::move (points));   // one undo step; turns R on
    std::vector<int> chosen;
    if (const auto* lane = laneFor (busId, laneIndex))
        for (int i = 0; i < (int) lane->points.size(); ++i)
            if (std::find (selected.begin(), selected.end(), lane->points[(size_t) i]) != selected.end())
                chosen.push_back (i);
    pointSelection = { busId, laneIndex, chosen };
    repaint();
}

void ArrangeView::deleteSelectedPoints()
{
    const auto* lane = laneFor (pointSelection.busId, pointSelection.laneIndex);
    if (lane == nullptr || pointSelection.points.empty())
        return;
    std::vector<pr7s::AutomationPoint> kept;
    for (int i = 0; i < (int) lane->points.size(); ++i)
        if (! std::binary_search (pointSelection.points.begin(), pointSelection.points.end(), i))
            kept.push_back (lane->points[(size_t) i]);
    commitPoints (std::move (kept), {});
}

void ArrangeView::selectAllPoints (int busId, int laneIndex)
{
    std::vector<int> all;
    if (const auto* lane = laneFor (busId, laneIndex))
        for (int i = 0; i < (int) lane->points.size(); ++i)
            all.push_back (i);
    pointSelection = { busId, laneIndex, all };
    repaint();
}

bool ArrangeView::cancelDrag()
{
    if (drag != Drag::points && drag != Drag::draw && drag != Drag::band)
        return false;
    drag = Drag::none;
    workingPoints = originalPoints;
    drawn.clear();
    repaint();
    return true;
}

void ArrangeView::showLaneMenu (int busId, int laneIndex)
{
    juce::PopupMenu menu;
    menu.addItem (1, "Select All Events");
    menu.addItem (2, "Delete", pointSelection.busId == busId && pointSelection.laneIndex == laneIndex && hasSelectedPoints());
    menu.addSeparator();
    menu.addItem (3, "Remove Parameter");
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<ArrangeView> (this), busId, laneIndex] (int chosen)
    {
        if (safe == nullptr)
            return;
        if (chosen == 1)
            safe->selectAllPoints (busId, laneIndex);
        else if (chosen == 2)
            safe->deleteSelectedPoints();
        else if (chosen == 3)
        {
            safe->pointSelection = {};
            safe->studio.removeAutomationParameter (busId, laneIndex);
        }
    });
}

//==============================================================================
ArrangeTools::ArrangeTools (ArrangeView& arrangeToUse) : arrange (arrangeToUse)
{
    static const char* const tips[] = {
        "SELECT (1): select, move (Alt: copy) and resize parts; Alt-click splits a part there; Alt-drag on an empty instrument track draws a part",
        "SPLIT (3): click a part to cut it there (on the grid; Ctrl: anywhere); Alt-click: into equal pieces that long",
        "GLUE (4): click a part to join it to the next one on its track; Alt-click: to all after it",
        "ERASE (5): click a part to delete it; Alt-click: it and all after it on its track",
        "ZOOM (6): click to zoom in there, Alt-click to zoom out; drag across a stretch to fill the view with it",
        "MUTE (7): click a part to mute or unmute it (muted parts aren't played); drag a rectangle over several",
        "DRAW (8): drag on an instrument track to draw a new, empty part",
        "PLAY (9): hold the mouse down on the tracks to play from there; let go to stop" };
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        auto& button = buttons[i];
        const auto t = ArrangeView::tools[i];
        button.setButtonText (ArrangeView::toolName (t));
        button.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        button.setWantsKeyboardFocus (false);
        button.setTooltip (tips[i]);
        button.onClick = [this, t] { arrange.setTool (t); };
        addAndMakeVisible (button);
    }
    refresh();
}

void ArrangeTools::refresh()
{
    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i].setToggleState (ArrangeView::tools[i] == arrange.getTool(), juce::dontSendNotification);
}

juce::TextButton& ArrangeTools::getButton (ArrangeView::Tool t)
{
    for (size_t i = 0; i < buttons.size(); ++i)
        if (ArrangeView::tools[i] == t)
            return buttons[i];
    return buttons[0];
}

void ArrangeTools::paint (juce::Graphics& g)
{
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::textFont (11.0f));
    g.drawText ("TOOLS", getLocalBounds().withWidth (60).withTrimmedLeft (14), juce::Justification::centredLeft, false);
}

void ArrangeTools::resized()
{
    auto area = getLocalBounds().withTrimmedLeft (64);
    for (auto& button : buttons)
    {
        button.setBounds (area.removeFromLeft (70));
        area.removeFromLeft (4);
    }
}
