#include "ArrangeView.h"

#include <numeric>

#include "ui/ChordUi.h"
#include "ui/Finish.h"
#include "ui/ToolIcons.h"
#include "engine/Warp.h"
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
    setOpaque (true);   // (0.57.1: paint fills it all: what's behind isn't drawn under every playhead frame)
    if (auto* settings = studio.getSettings())
        pointBoxShown = settings->getBoolValue ("automationEditBox", false);
    studio.addChangeListener (this);
    startTimerHz (30);
}

ArrangeView::~ArrangeView()
{
    studio.removeChangeListener (this);
}

void ArrangeView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // (0.34.3) Only the mix moved (a fader, a pan, a send level): nothing here shows it. It repainted
    // the whole arrangement, across three screens, on every step of a fader drag.
    if (studio.getChangeVersion() == seenChange)
        return;
    seenChange = studio.getChangeVersion();
    // A range keeps only the tracks still there.
    if (range)
    {
        auto& ids = range->trackIds;
        ids.erase (std::remove_if (ids.begin(), ids.end(), [this] (int id) { return studio.getProject().findTrack (id) == nullptr; }), ids.end());
        if (ids.empty())
            range.reset();
    }
    // Parts selected that have gone (undo, delete) leave the selection.
    alsoSelected.erase (std::remove_if (alsoSelected.begin(), alsoSelected.end(), [this] (int id) { return ! studio.findPart (id).has_value(); }),
                        alsoSelected.end());
    // The part selected, found again by its id (an undo or a delete before it
    // moves it in its track's list, or to another track); gone: no selection.
    if (selection.isValid())
    {
        selection = {};
        if (selectedPartId != 0)
            if (const auto at = studio.findPart (selectedPartId))
                selection = selectionOf (at->first, at->second);
    }
    selectedPartId = selection.isValid() ? selectedPartId : 0;
    // (0.49.0) Markers selected that have gone (undo, delete) leave the selection.
    {
        const auto count = (int) studio.getMarkerTrack().markers.size();
        markerSelection.erase (std::remove_if (markerSelection.begin(), markerSelection.end(), [count] (int i) { return i >= count; }),
                               markerSelection.end());
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
    if (chordWarning.isNotEmpty() && juce::Time::getMillisecondCounter() > chordWarningUntil)
    {
        chordWarning.clear();
        const auto area = getChordArea();
        repaint (area.withHeight (area.getHeight() * 2.0f + 10.0f).getSmallestIntegerContainer().expanded (2));   // (the key strip under it too)
    }
    followPlayhead();
    advancePlayhead();
    refreshPlayhead();
    // 60 frames a second while it glides, else 30 (the screens' work, 0.9.2).
    // (0.78.3) And while playing: the play bar moves every frame now, smoothly (2 thin strips, ~0.3 ms).
    const auto playing = studio.getEngine().isPlaying() || studio.getEngine().isRecording();
    // (0.85.2) Not with Stationary Cursor: the play bar stands still and the whole view moves under it,
    // so every frame redraws all of it (at 5760 wide); 30 a second as before 0.78.3.
    const auto stationary = playing && timeline.autoScroll.on && timeline.autoScroll.stationary && ! timeline.autoScroll.suspended;
    const auto rate = gliding || (playing && ! stationary) ? 60 : 30;
    if (getTimerInterval() != 1000 / rate)
        startTimerHz (rate);
}

double ArrangeView::playingBeat (double shown, double target) noexcept
{
    // A new block can land a hair behind where the clock had carried the play bar: it waits there
    // rather than stepping back. A real jump (the cycle coming round, a click) goes at once.
    return target < shown && shown - target < smallestJumpBack ? shown : target;
}

void ArrangeView::advancePlayhead()
{
    auto& engine = studio.getEngine();
    if (engine.isPlaying() || engine.isRecording())
    {
        shownBeat = playingBeat (shownBeat, engine.getShownPosition());
        gliding = false;
        return;
    }
    const auto target = engine.getPosition();
    const auto pixels = std::abs (target - shownBeat) * timeline.pixelsPerBeat;
    // Playing (or recording): exactly where it is. Stopped: a click or a drag in the ruler
    // (each snapped to the grid) glides it there, a little less each frame; a long jump
    // (Home, half a screen and more) goes straight there.
    if (engine.isPlaying() || engine.isRecording() || pixels < 0.5 || pixels > (double) getWidth() * 0.5)
    {
        shownBeat = target;
        gliding = false;
        return;
    }
    shownBeat += (target - shownBeat) * glideStep;
    gliding = true;
}

juce::Rectangle<float> ArrangeView::dropPreviewBounds() const
{
    if (! dropPreview)
        return {};
    const auto x1 = timeline.xForBeat (dropPreview->start), x2 = timeline.xForBeat (dropPreview->start + dropPreview->length);
    return { x1, trackTop (dropPreview->trackId) + 2.0f, juce::jmax (4.0f, x2 - x1), (float) timeline.trackRowHeight() - 4.0f };
}

void ArrangeView::setDropPreview (std::optional<DropPreview> preview)
{
    const auto same = [] (const std::optional<DropPreview>& a, const std::optional<DropPreview>& b)
    {
        return a.has_value() == b.has_value() && (! a || (a->trackId == b->trackId && a->start == b->start && a->length == b->length));
    };
    if (same (preview, dropPreview))
        return;
    repaint (dropPreviewBounds().expanded (3.0f).toNearestInt());
    dropPreview = std::move (preview);
    repaint (dropPreviewBounds().expanded (3.0f).toNearestInt());
}

void ArrangeView::setRulerHover (std::optional<double> beat)
{
    if (beat == rulerHover)
        return;
    const auto strip = [this] (double at) { const auto x = timeline.xForBeat (at); return juce::Rectangle<float> (x - 8.0f, 0.0f, 17.0f, (float) Timeline::rulerHeight).toNearestIntEdges(); };
    if (rulerHover)
        repaint (strip (*rulerHover));
    rulerHover = beat;
    if (rulerHover)
        repaint (strip (*rulerHover));
}

void ArrangeView::refreshPlayhead()
{
    const auto x = timeline.xForBeat (shownBeat);
    const auto red = studio.getEngine().isRecording();
    if (std::abs (x - playheadX) < 0.05f && red == playheadRed)
        return;
    const auto strip = [this] (float at) { return juce::Rectangle<float> (at - 8.0f, 0.0f, 17.0f, (float) getHeight()).toNearestIntEdges(); };
    repaint (strip (playheadX));
    repaint (strip (x));
    playheadX = x;
    playheadRed = red;
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

bool ArrangeView::scrollTowards (juce::Point<float> at)
{
    // As far into the edge's zone (or past the edge) as the mouse is, a little each time (faster further out).
    // (0.84.1) The zone starts `edgeScrollZone` inside the view: the owner's window runs to the screens' edges,
    // where the mouse can't go past the view at all, so only "past the edge" never scrolled there.
    const auto speed = [] (float past) { return juce::jlimit (4.0f, 60.0f, past * 0.5f + 4.0f); };
    const auto zone = edgeScrollZone;
    const auto right = (float) getWidth() - zone, bottom = (float) getHeight() - zone;
    const auto top = (float) Timeline::rulerHeight;
    auto moved = false;
    if (at.x < zone && timeline.firstBeat > 0.0)
    {
        timeline.firstBeat = juce::jmax (0.0, timeline.firstBeat - speed (zone - at.x) / timeline.pixelsPerBeat);
        moved = true;
    }
    else if (at.x > right)
    {
        timeline.firstBeat += speed (at.x - right) / timeline.pixelsPerBeat;
        moved = true;
    }
    const auto room = juce::jmax (0, timeline.contentHeight (studio) - (getHeight() - Timeline::rulerHeight - Timeline::bottomRoom));
    if (at.y < top + zone && timeline.scrollY > 0)
    {
        timeline.scrollY = juce::jmax (0, timeline.scrollY - juce::roundToInt (speed (top + zone - at.y)));
        moved = true;
    }
    else if (at.y > bottom && timeline.scrollY < room)
    {
        timeline.scrollY = juce::jmin (room, timeline.scrollY + juce::roundToInt (speed (at.y - bottom)));
        moved = true;
    }
    if (moved)
        scrolled();
    return moved;
}

void ArrangeView::repaintBandChange (juce::Rectangle<float> before, juce::Rectangle<float> after)
{
    // (0.84.1, the owner: a big box lagged) The box's fill is see-through and doesn't change inside, so only
    // what it newly covers or uncovers, and its old and new edges, are drawn again: a box across three screens
    // used to redraw the whole arrangement on every mouse move.
    const auto whole = [] (juce::Rectangle<float> r) { return r.getSmallestIntegerContainer(); };
    juce::RectangleList<int> dirty;
    const auto edgesOf = [&dirty, &whole] (juce::Rectangle<float> r)
    {
        if (r.isEmpty())
            return;
        const auto b = whole (r).expanded (2);
        dirty.add (b.withHeight (4));
        dirty.add (b.withTop (b.getBottom() - 4));
        dirty.add (b.withWidth (4));
        dirty.add (b.withLeft (b.getRight() - 4));
    };
    for (const auto& [a, b] : { std::pair { before, after }, std::pair { after, before } })
        if (! a.isEmpty())
        {
            juce::RectangleList<int> only (whole (a));
            if (! b.isEmpty())
                only.subtract (whole (b));
            dirty.add (only);
        }
    edgesOf (before);
    edgesOf (after);
    dirty.clipTo (getLocalBounds());
    bandRepaintArea = 0.0;
    for (const auto& r : dirty)
    {
        bandRepaintArea += (double) r.getWidth() * r.getHeight();
        repaint (r);
    }
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

namespace
{
    // Automation points and tempo events: their size, how near a click grabs one, the gap at a lane's top and bottom.
    constexpr float pointSize = 7.0f, pointGrab = 6.0f, laneInset = 5.0f;
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
    // The master first, always at the very top (0.9.12, the owner asked), then
    // the tempo track if it's been added (0.10.0; our place for it: it's one
    // of a kind, as the master is), then the tracks.
    add (Row::Kind::master, pr7s::Engine::masterBus, -1, masterRowHeight());
    addLanes (pr7s::Engine::masterBus);
    if (studio.getProject().tempoTrackShown)
        add (Row::Kind::tempo, 0, -1, tempoRowHeight());
    if (studio.getProject().signatureTrackShown)
        add (Row::Kind::signature, 0, -1, signatureRowHeight());
    if (studio.getProject().markerTrack.shown)
        add (Row::Kind::markers, 0, -1, markerRowHeight());   // (0.49.0)
    if (studio.getProject().chordTrack.shown)
        add (Row::Kind::chords, 0, -1, chordRowHeight());   // (0.16.0: under the tempo track's)
    for (const auto& track : studio.getProject().tracks)
    {
        if (track.hidden || studio.getProject().isFoldedAway (track))
            continue;   // (VISIBILITY shows it again; 0.50.0: so does opening its folder)
        add (Row::Kind::track, track.id, -1, trackRowHeight());
        // Its lanes of takes (0.29.0, Show Lanes): each lane, and an empty one under them to drag onto.
        if (track.showLanes && ! track.isChannel())
            for (int lane = 0; lane <= track.numLanes(); ++lane)
                add (Row::Kind::take, track.id, lane, takeRowHeight());
        addLanes (track.id);
    }
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

// The rows: while painting, the list worked out once for the whole paint (0.34.4: every track,
// lane and part asked for it again, a fresh list each time); else worked out now into `made`.
const std::vector<Timeline::Row>& ArrangeView::rowsNow (std::vector<Timeline::Row>& made) const
{
    if (rowsInPaint != nullptr)
        return *rowsInPaint;
    made = timeline.layout (studio);
    return made;
}

float ArrangeView::trackTop (int trackId) const
{
    std::vector<Timeline::Row> made;
    for (const auto& row : rowsNow (made))
        if (row.kind == Timeline::Row::Kind::track && row.busId == trackId)
            return rowTop (row);
    return 0.0f;
}

std::optional<Timeline::Row> ArrangeView::rowAt (float y) const
{
    std::vector<Timeline::Row> made;
    if (y < (float) Timeline::rulerHeight)
        return std::nullopt;
    for (const auto& row : rowsNow (made))
        if (y >= rowTop (row) && y < rowTop (row) + (float) row.height)
            return row;
    return std::nullopt;
}

int ArrangeView::trackAtY (float y) const
{
    const auto row = rowAt (y);
    return row && (row->kind == Timeline::Row::Kind::track || row->kind == Timeline::Row::Kind::take) ? row->busId : 0;
}

int ArrangeView::laneAtY (float y) const
{
    const auto row = rowAt (y);
    return row && row->kind == Timeline::Row::Kind::take ? row->laneIndex : -1;
}

juce::Rectangle<float> ArrangeView::partRow (int trackId, int lane) const
{
    // A part's strip: its lane's row when the track shows its lanes (0.29.0), else the track's row.
    const auto* track = studio.getProject().findTrack (trackId);
    std::vector<Timeline::Row> made;
    if (track != nullptr && track->showLanes && lane >= 0)
        for (const auto& row : rowsNow (made))
            if (row.kind == Timeline::Row::Kind::take && row.busId == trackId && row.laneIndex == lane)
                return { 0.0f, rowTop (row) + 2.0f, 0.0f, (float) row.height - 4.0f };
    return { 0.0f, trackTop (trackId) + 3.0f, 0.0f, (float) timeline.trackRowHeight() - 6.0f };
}

juce::Rectangle<float> ArrangeView::getPartBounds (int trackId, int index) const
{
    if (const auto* track = studio.getProject().findTrack (trackId); track != nullptr && ! track->hidden && ! studio.getProject().isFoldedAway (*track)
                                                                      && juce::isPositiveAndBelow (index, (int) track->parts.size()))
    {
        const auto& part = track->parts[(size_t) index];
        const auto strip = partRow (trackId, part.lane);
        return { timeline.xForBeat (part.start), strip.getY(), (float) (part.length * timeline.pixelsPerBeat), strip.getHeight() };
    }
    return {};
}

std::optional<std::pair<int, int>> ArrangeView::partAt (juce::Point<float> point) const
{
    const auto trackId = trackAtY (point.y);
    if (const auto* track = studio.getProject().findTrack (trackId))
    {
        // (The one in front first: as drawn.)
        std::vector<int> order ((size_t) track->parts.size());
        std::iota (order.begin(), order.end(), 0);
        std::sort (order.begin(), order.end(), [track] (int a, int b) { return pr7s::isInFront (*track, a, b); });
        for (const auto i : order)
            if (getPartBounds (trackId, i).contains (point))
                return std::pair<int, int> { trackId, i };
        // Lanes shown: the track's own row is what's heard there (0.29.0).
        if (track->showLanes && laneAtY (point.y) < 0)
            if (const auto id = studio.frontPartAt (trackId, timeline.beatForX (point.x)))
                for (int i = 0; i < (int) track->parts.size(); ++i)
                    if (track->parts[(size_t) i].id == id)
                        return std::pair<int, int> { trackId, i };
    }
    return std::nullopt;
}

int ArrangeView::partIdAt (const Selection& s) const
{
    const auto* track = studio.getProject().findTrack (s.trackId);
    return track != nullptr && juce::isPositiveAndBelow (s.index, (int) track->parts.size()) ? track->parts[(size_t) s.index].id : 0;
}

bool ArrangeView::isSelected (int trackId, int index) const
{
    if (selection.trackId == trackId && selection.index == index)
        return true;
    const auto id = partIdAt ({ trackId, index });
    return id != 0 && std::find (alsoSelected.begin(), alsoSelected.end(), id) != alsoSelected.end();
}

std::vector<int> ArrangeView::getSelectedParts() const
{
    auto ids = alsoSelected;
    if (const auto id = partIdAt (selection); id != 0 && std::find (ids.begin(), ids.end(), id) == ids.end())
        ids.push_back (id);
    return ids;
}

bool ArrangeView::isPartSelected (int partId) const
{
    const auto ids = getSelectedParts();
    return std::find (ids.begin(), ids.end(), partId) != ids.end();
}

void ArrangeView::selectParts (const std::vector<int>& partIds)
{
    selection = {};
    pointSelection = {};   // (0.47.0: the info line shows what was chosen last)
    alsoSelected.clear();
    for (const auto id : partIds)
        if (const auto at = studio.findPart (id))
        {
            if (! selection.isValid())
                selection = selectionOf (at->first, at->second);
            else
                alsoSelected.push_back (id);
        }
    if (selection.isValid())
        studio.selectTrack (selection.trackId);
    repaint();
}

void ArrangeView::selectAllParts()
{
    std::vector<int> ids;
    for (const auto trackId : studio.getShownTracks())
        for (const auto& part : studio.getProject().findTrack (trackId)->parts)
            ids.push_back (part.id);
    selectParts (ids);
}

void ArrangeView::copySelected (bool cut)
{
    const auto ids = getSelectedParts();
    if (ids.empty())
        return;
    partsClip = studio.copyParts (ids);
    if (cut)
    {
        selection = {};
        alsoSelected.clear();
        studio.deleteParts (ids);
    }
}

void ArrangeView::pasteParts (bool atOrigin)
{
    if (partsClip.isEmpty())
        return;
    // At the playhead, from the selected track down (as the manual's Paste); or where they were (Paste at Origin).
    auto first = atOrigin ? partsClip.originTrack : studio.getSelectedTrack();
    if (! studio.isTrackShown (first))
        first = partsClip.originTrack;
    studio.pasteRange (partsClip, atOrigin ? partsClip.origin : studio.getEngine().getPosition(), first, false);
}

ArrangeView::Selection ArrangeView::selectionOf (int trackId, int index)
{
    // (The part's id kept beside it: the index alone may point at another part after an edit.)
    selectedPartId = 0;
    if (const auto* track = studio.getProject().findTrack (trackId); track != nullptr && juce::isPositiveAndBelow (index, (int) track->parts.size()))
        selectedPartId = track->parts[(size_t) index].id;
    return { trackId, index };
}

void ArrangeView::select (int trackId, int index)
{
    alsoSelected.clear();
    selection = selectionOf (trackId, index);
    if (trackId != 0)
        studio.selectTrack (trackId);
    repaint();
}

void ArrangeView::deleteSelected()
{
    if (const auto ids = getSelectedParts(); ids.size() > 1)
    {
        selection = {};
        alsoSelected.clear();
        studio.deleteParts (ids);
        return;
    }
    if (! selection.isValid())
        return;
    const auto doomed = selection;
    selection = {};
    studio.deletePart (doomed.trackId, doomed.index);
}

void ArrangeView::duplicateSelected()
{
    // Several: copied as one unit straight after them, keeping where they are to each other (the manual's Duplicate).
    if (const auto ids = getSelectedParts(); ids.size() > 1)
    {
        const auto clip = studio.copyParts (ids);
        studio.pasteRange (clip, clip.origin + clip.length, clip.originTrack, false);
        return;
    }
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
    rowsInPaint = &rows;
    const struct Forget { const std::vector<Timeline::Row>*& rows; ~Forget() { rows = nullptr; } } forget { rowsInPaint };
    auto trackNumber = 0;
    for (const auto& row : rows)
    {
        const auto lane = juce::Rectangle<float> (0.0f, rowTop (row), bounds.getWidth(), (float) row.height);
        if (row.kind == Timeline::Row::Kind::track)
            g.setColour (row.busId == studio.getSelectedTrack() ? juce::Colour (0xff1c151b) : (trackNumber++ % 2 == 0 ? laneDark : laneLight));
        else if (row.kind == Timeline::Row::Kind::take)
            g.setColour (juce::Colour (0xff0e0b0e));   // (a lane of takes: darker than its track, lighter than automation)
        else
            g.setColour (juce::Colour (0xff09070a));
        g.fillRect (lane);
        g.setColour (juce::Colour (0xff050405));
        g.fillRect (lane.withHeight (1.0f).withY (lane.getBottom() - 1.0f));
    }

    // The grid: bars brighter than beats (beats only when there's room), bar by bar by the
    // signature in force there (0.45.0).
    const auto signatures = project.signatures();
    const auto lastVisible = timeline.beatForX (bounds.getWidth());
    // (0.57.1) Only the stretch being redrawn (a playhead strip is 17 px of a 5760 px view), with room
    // for a bar number that starts just left of it: these loops ran the whole width every frame.
    const auto paintClip = g.getClipBounds().toFloat();
    const auto paintFrom = juce::jmax (timeline.firstBeat, timeline.beatForX (paintClip.getX() - 80.0f));
    const auto paintTo = juce::jmin (lastVisible, timeline.beatForX (paintClip.getRight() + 4.0f));
    for (auto barIndex = signatures.barAt (paintFrom); signatures.beatOfBar (barIndex) <= paintTo; ++barIndex)
    {
        const auto& signature = signatures.segmentOfBar (barIndex).event;
        const auto start = signatures.beatOfBar (barIndex);
        const auto showBeats = signature.unit() * timeline.pixelsPerBeat >= 10.0;
        if (signature.barLength() * timeline.pixelsPerBeat < 3.0 && barIndex % 4 != 0)
            continue;   // (zoomed far out: every fourth bar)
        for (int b = 0; b < (showBeats ? signature.numerator : 1); ++b)
        {
            g.setColour (b == 0 ? gridBar : gridBeat);
            g.fillRect (juce::Rectangle<float> (timeline.xForBeat (start + b * signature.unit()), (float) Timeline::rulerHeight, 1.0f, bounds.getHeight()));
        }
    }

    // The automation lanes' curves (only the lanes in what's being redrawn: a lane's drawing stays
    // within its row but for the handles, well inside the margin).
    const auto redrawn = g.getClipBounds().toFloat();
    for (const auto& row : rows)
        if (rowTop (row) + (float) row.height + rowMargin < redrawn.getY() || rowTop (row) - rowMargin > redrawn.getBottom())
            continue;
        else if (row.kind == Timeline::Row::Kind::lane)
            paintAutomation (g, row);
        else if (row.kind == Timeline::Row::Kind::tempo)
            paintTempo (g, row);
        else if (row.kind == Timeline::Row::Kind::chords)
            paintChords (g, row);
        else if (row.kind == Timeline::Row::Kind::signature)
            paintSignature (g, row);
        else if (row.kind == Timeline::Row::Kind::markers)
            paintMarkers (g, row);

    // (0.49.0) Marker lines down the arrangement, behind the parts (the manual's Show Marker Lines).
    paintMarkerLines (g);
    paintFolders (g);   // (0.50.0) folders' blocks

    // The cycle range over the lanes, faintly.
    {
        const auto x1 = timeline.xForBeat (project.loopStart), x2 = timeline.xForBeat (project.loopEnd);
        g.setColour (ParagonColours::accent.withAlpha (project.loopOn ? 0.07f : 0.025f));
        g.fillRect (juce::Rectangle<float> (x1, (float) Timeline::rulerHeight, x2 - x1, bounds.getHeight()));
    }

    // The parts (the one being dragged drawn where it's going; several selected: all of them).
    const auto recording = studio.getRecordingPreview();
    const auto multiDrag = drag == Drag::move && dragMoved && getSelectedParts().size() > 1;
    for (size_t t = 0; t < project.tracks.size(); ++t)
    {
        const auto& track = project.tracks[t];
        if (track.hidden || project.isFoldedAway (track))
            continue;
        // A disabled track's parts: greyed out (its colour changes, as the manual has it).
        const auto colour = track.disabled ? juce::Colour (0xff4a454c) : track.colour;
        // Its row, once (not for every part: that worked the layout out again each time); with
        // its lanes shown (0.29.0), each lane's strip too.
        const auto top = trackTop (track.id) + 3.0f, height = (float) timeline.trackRowHeight() - 6.0f;
        const auto clip = g.getClipBounds().toFloat();
        std::vector<juce::Rectangle<float>> laneStrips;
        if (track.showLanes)
            for (int lane = 0; lane < track.numLanes(); ++lane)
                laneStrips.push_back (partRow (track.id, lane));
        const auto bottom = laneStrips.empty() ? top + height : laneStrips.back().getBottom();
        if (bottom < clip.getY() || top > clip.getBottom())
            continue;   // the rows aren't in what's being redrawn
        const auto stripOf = [&] (const pr7s::Part& part)
        {
            return juce::isPositiveAndBelow (part.lane, (int) laneStrips.size()) ? laneStrips[(size_t) part.lane]
                                                                                  : juce::Rectangle<float> (0.0f, top, 0.0f, height);
        };
        // Drawn back to front (the clip in front on top).
        std::vector<int> drawOrder ((size_t) track.parts.size());
        std::iota (drawOrder.begin(), drawOrder.end(), 0);
        std::sort (drawOrder.begin(), drawOrder.end(), [&track] (int a, int b) { return pr7s::isInFront (track, b, a); });
        if (track.showLanes)
        {
            // The track's own row: what's heard (the comp), each part only where it plays.
            for (const auto i : drawOrder)
            {
                const auto& part = track.parts[(size_t) i];
                const juce::Rectangle<float> area (timeline.xForBeat (part.start), top, (float) (part.length * timeline.pixelsPerBeat), height);
                if (area.getRight() < clip.getX() || area.getX() > clip.getRight())
                    continue;
                for (const auto& [from, to] : pr7s::heardStretches (track, i))
                {
                    juce::Graphics::ScopedSaveState saved (g);
                    const auto x1 = timeline.xForBeat (from), x2 = timeline.xForBeat (to);
                    g.reduceClipRegion (juce::Rectangle<float> (x1, top - 1.0f, x2 - x1, height + 2.0f).getSmallestIntegerContainer());
                    paintPart (g, area, part, colour, false, false);
                }
            }
        }
        for (const auto i : drawOrder)
        {
            const auto isClipDrag = drag == Drag::trimStart || drag == Drag::fadeIn || drag == Drag::fadeOut;
            const auto isDragged = (drag == Drag::move || drag == Drag::resize || isClipDrag) && dragMoved && track.id == dragTrack && i == selection.index;
            if (isDragged && isClipDrag)
            {
                const auto strip = stripOf (dragPart);
                const juce::Rectangle<float> area (timeline.xForBeat (dragPart.start), strip.getY(),
                                                   (float) (dragPart.length * timeline.pixelsPerBeat), strip.getHeight());
                paintPart (g, area, dragPart, colour, true, false);
                continue;
            }
            if (isDragged && ! dragCopies)
                continue;
            if (multiDrag && ! dragCopies && isSelected (track.id, i))
                continue;   // (drawn where they're going, below)
            const auto& part = track.parts[(size_t) i];
            const auto strip = stripOf (part);
            const juce::Rectangle<float> area (timeline.xForBeat (part.start), strip.getY(), (float) (part.length * timeline.pixelsPerBeat), strip.getHeight());
            if (area.getRight() < clip.getX() || area.getX() > clip.getRight())
                continue;
            paintPart (g, area, part, colour, isSelected (track.id, i), false);
            if (track.showLanes)
            {
                // On its lane: what isn't heard of it veiled (another take is in front there).
                auto x = part.start;
                auto veil = [&] (double from, double to)
                {
                    if (to > from + 1.0e-9)
                    {
                        const auto x1 = timeline.xForBeat (from), x2 = timeline.xForBeat (to);
                        g.setColour (juce::Colours::black.withAlpha (0.5f));
                        g.fillRect (juce::Rectangle<float> (x1, area.getY(), x2 - x1, area.getHeight()).getIntersection (area));
                    }
                };
                for (const auto& [from, to] : pr7s::heardStretches (track, i))
                {
                    veil (x, from);
                    x = to;
                }
                veil (x, part.end());
            }
            // The comp handle (0.30.0) on a stacked take's lower edge: click it to bring the take forward.
            if (const auto handle = compHandleBounds (track.id, i); ! handle.isEmpty())
            {
                g.setColour (juce::Colour (0xfff4eeea).withAlpha (0.85f));
                g.fillRoundedRectangle (handle, 2.0f);
                g.setColour (colour.darker (0.4f));
                g.fillRect (handle.reduced (5.0f, 2.5f));
            }
            if (track.frozen)
            {
                // Frozen (0.9.8): veiled in pearl white and stamped; its parts can't be edited.
                g.setColour (juce::Colour (0xfff4eeea).withAlpha (0.16f));
                g.fillRoundedRectangle (area, 4.0f);
                if (area.getWidth() > 60.0f && area.getHeight() > 34.0f)
                {
                    g.setColour (juce::Colour (0xfff4eeea).withAlpha (0.8f));
                    g.setFont (font (13.0f));
                    g.drawText ("FROZEN", area.reduced (6.0f, 3.0f), juce::Justification::bottomRight, false);
                }
            }
        }
    }
    if (multiDrag)
    {
        const auto shown = studio.getShownTracks();
        const auto delta = dragNewStart - dragOriginalStart;
        const auto trackDelta = trackIndex (dragTargetTrack) - trackIndex (dragTrack);
        for (int s = 0; s < (int) shown.size(); ++s)
        {
            const auto& track = *project.findTrack (shown[(size_t) s]);
            for (int i = 0; i < (int) track.parts.size(); ++i)
                if (isSelected (track.id, i))
                {
                    // To the track as many below (of the same kind), else its own (as moveParts does).
                    auto to = track.id;
                    if (juce::isPositiveAndBelow (s + trackDelta, (int) shown.size()))
                        if (const auto* target = project.findTrack (shown[(size_t) (s + trackDelta)]);
                            target != nullptr && ! target->isChannel() && target->isAudio() == track.isAudio())
                            to = target->id;
                    auto part = track.parts[(size_t) i];
                    part.start = juce::jmax (0.0, part.start + delta);
                    // (0.84.1) On the lane it's on (it was drawn on the track's own row: several takes moving
                    // together looked like one, all on the same row).
                    const auto strip = partRow (to, part.lane);
                    const juce::Rectangle<float> area (timeline.xForBeat (part.start), strip.getY(),
                                                       (float) (part.length * timeline.pixelsPerBeat), strip.getHeight());
                    paintPart (g, area, part, track.colour, true, false);
                }
        }
    }
    else if ((drag == Drag::move || drag == Drag::resize) && dragMoved && selection.isValid())
        if (const auto* from = project.findTrack (dragTrack))
            if (juce::isPositiveAndBelow (selection.index, (int) from->parts.size()))
            {
                auto part = from->parts[(size_t) selection.index];
                part.start = dragNewStart;
                part.length = dragNewLength;
                const auto strip = partRow (dragTargetTrack, dragTargetLane >= 0 ? dragTargetLane : dragTargetTrack == dragTrack ? part.lane : 0);
                const juce::Rectangle<float> area (timeline.xForBeat (part.start), strip.getY(),
                                                   (float) (part.length * timeline.pixelsPerBeat), strip.getHeight());
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

    paintRange (g);

    // (0.30.0) A split or edge being moved: where it goes, down the track and its lanes; a slip: by how much.
    if (drag == Drag::takeEdge && dragMoved)
    {
        const auto x = timeline.xForBeat (edgeTo);
        const auto top = trackTop (compTrack);
        auto bottom = top + (float) timeline.trackRowHeight();
        if (const auto* t = project.findTrack (compTrack); t != nullptr && t->showLanes)
            bottom = partRow (compTrack, t->numLanes()).getBottom();
        g.setColour (ParagonColours::accent);
        g.fillRect (juce::Rectangle<float> (x - 1.0f, top, 2.0f, bottom - top));
    }
    if (drag == Drag::slip && dragMoved)
        if (const auto at = studio.findPart (slipPartId))
        {
            const auto area = getPartBounds (at->first, at->second);
            g.setColour (ParagonColours::accent);
            g.drawRoundedRectangle (area, 4.0f, 2.0f);
            g.setFont (font (12.0f));
            g.drawText ("SLIP " + juce::String (slipDelta >= 0.0 ? "+" : "") + juce::String (slipDelta, 3) + " BEATS",
                        area.withTrimmedTop (16.0f).reduced (6.0f, 2.0f), juce::Justification::topRight, false);
        }

    // (0.32.0) WARP: the marker being dragged, where it'll go.
    if (drag == Drag::warpMarker && dragMoved)
        if (const auto at = studio.findPart (warpPartId))
        {
            const auto& p = project.findTrack (at->first)->parts[(size_t) at->second];
            const auto area = getPartBounds (at->first, at->second);
            const auto x = timeline.xForBeat (p.start + warpTo);
            g.setColour (ParagonColours::accent);
            g.fillRect (juce::Rectangle<float> (x - 1.0f, area.getY(), 2.0f, area.getHeight()));
        }

    // The Comp tool's stretch on its lane (0.29.0).
    if (drag == Drag::comp && dragMoved && compTo > compFrom)
    {
        const auto strip = partRow (compTrack, compLane);
        const auto x1 = timeline.xForBeat (compFrom), x2 = timeline.xForBeat (compTo);
        const juce::Rectangle<float> band (x1, strip.getY(), x2 - x1, strip.getHeight());
        g.setColour (ParagonColours::accent.withAlpha (0.25f));
        g.fillRect (band);
        g.setColour (ParagonColours::accent);
        g.drawRect (band, 1.5f);
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
    if (tool == Tool::split && drag == Drag::none)
        if (const auto x = splitLineX(); x >= 0.0f)
            if (const auto hit = partAt (hover))
            {
                const auto bounds = getPartBounds (hit->first, hit->second);
                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.fillRect (juce::Rectangle<float> (x - 0.5f, bounds.getY(), 1.5f, bounds.getHeight()));
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
        if (project.loopOn)
        {
            // On: the keys' pink, red and orange across it (0.9.11, the owner asked: not a flat orange), with a soft glow.
            auto glow = Finish::neonGradient (x1, x2);
            glow.multiplyOpacity (0.3f);
            g.setGradientFill (glow);
            g.fillRoundedRectangle (strip.expanded (2.0f), 3.0f);
            g.setGradientFill (Finish::neonGradient (x1, x2));
        }
        else
            g.setColour (ParagonColours::textDim.withAlpha (0.45f));
        g.fillRoundedRectangle (strip, 2.0f);
        // The locators' handles: a flag at each end, pointing in.
        g.setColour (ParagonColours::metalLight);
        for (const auto [x, inward] : { std::pair<float, float> { x1, 1.0f }, std::pair<float, float> { x2, -1.0f } })
        {
            g.fillRect (juce::Rectangle<float> (x - 1.0f, 1.0f, 2.0f, (float) Timeline::cycleStripHeight - 1.0f));
            juce::Path flag;
            flag.addTriangle (x, 1.0f, x + inward * 7.0f, 1.0f + 4.5f, x, 10.0f);
            g.fillPath (flag);
        }
    }
    // Note ticks along the ruler's foot (0.17.3, the owner asked): quarters tallest, then
    // eighths, then sixteenths, each only when the zoom leaves room between them.
    for (const auto [step, height, alpha] : { std::tuple { 0.25, 3.0f, 0.35f }, std::tuple { 0.5, 5.0f, 0.5f }, std::tuple { 1.0, 8.0f, 0.75f } })
    {
        if (step * timeline.pixelsPerBeat < Timeline::rulerTickRoom)
            continue;
        g.setColour (ParagonColours::textDim.withAlpha (alpha));
        const auto first = std::floor (paintFrom / step) * step;
        for (auto i = 0; first + step * i <= paintTo; ++i)
        {
            const auto beat = first + step * i;
            const auto coarser = step < 1.0 && std::abs (std::fmod (beat, step * 2.0)) < 1.0e-6;   // (drawn by the longer tick)
            if (coarser || signatures.isBarLine (beat))
                continue;
            g.fillRect (juce::Rectangle<float> (timeline.xForBeat (beat), ruler.getBottom() - 1.0f - height, 1.0f, height));
        }
    }
    // Bar numbers every bar if there's room, else every 2, 4, 8... (bar by bar by the signature
    // track, 0.45.0: a bar where the signature changes is always numbered, with its signature).
    auto every = 1;
    while (signatures.barLengthAt (timeline.firstBeat) * every * timeline.pixelsPerBeat < 36.0 && every < 4096)
        every *= 2;
    g.setFont (font (11.0f));
    for (auto b = signatures.barAt (paintFrom); signatures.beatOfBar (b) <= paintTo; ++b)
    {
        const auto x = timeline.xForBeat (signatures.beatOfBar (b));
        const auto& segment = signatures.segmentOfBar (b);
        const auto changes = b > 0 && segment.event.bar == b;
        const auto numbered = b % every == 0 || changes;
        if (! numbered && signatures.barLengthAt (signatures.beatOfBar (b)) * timeline.pixelsPerBeat < 3.0)
            continue;
        g.setColour (numbered ? ParagonColours::textDim : ParagonColours::panelEdge);
        g.fillRect (juce::Rectangle<float> (x, numbered ? 16.0f : 26.0f, 1.0f, ruler.getBottom() - (numbered ? 16.0f : 26.0f)));
        if (numbered)
        {
            g.setColour (ParagonColours::text);
            auto text = juce::String (b + 1);
            if (changes && ! project.signatureTrackShown)
                text << "  " << segment.event.numerator << "/" << segment.event.denominator;
            g.drawText (text, juce::Rectangle<float> (x + 4.0f, 15.0f, 60.0f, 16.0f), juce::Justification::centredLeft, false);
        }
    }

    // Where a click in the ruler would put it (0.17.4): a faint marker on the grid line.
    if (rulerHover && drag == Drag::none)
    {
        const auto hx = timeline.xForBeat (*rulerHover);
        g.setColour (juce::Colour (0xfff4eeea).withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (hx, (float) Timeline::cycleStripHeight, 1.0f, (float) (Timeline::rulerHeight - Timeline::cycleStripHeight)));
        juce::Path ghost;
        ghost.addTriangle (hx - 5.0f, (float) Timeline::rulerHeight - 11.0f, hx + 5.0f, (float) Timeline::rulerHeight - 11.0f, hx, (float) Timeline::rulerHeight - 3.0f);
        g.setColour (pr7s::FamilyColours::pink.withAlpha (0.55f));
        g.fillPath (ghost);
    }

    // A MIDI file being dragged in (0.18.1): where it lands, in the family's gradient.
    if (dropPreview)
    {
        // As strong as the Hub's setting (0.18.2): the fill's opacity, the edge's width with it.
        const auto strength = studio.getDropHighlight();
        const auto r = dropPreviewBounds();
        auto fill = Finish::neonGradient (r.getX(), r.getRight());
        fill.multiplyOpacity (strength);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (r, 3.0f);
        g.setGradientFill (Finish::neonGradient (r.getX(), r.getRight()));
        g.drawRoundedRectangle (r.reduced (1.0f), 3.0f, 1.5f + 2.5f * strength);
        // The notes coming in, low to high in the row (0.18.2).
        if (! dropPreview->notes.empty())
        {
            auto low = 127, high = 0;
            for (const auto& n : dropPreview->notes)
            {
                low = juce::jmin (low, n.pitch);
                high = juce::jmax (high, n.pitch);
            }
            const auto body = r.reduced (2.0f, 4.0f).withTrimmedTop (14.0f);
            const auto row = body.getHeight() / (float) juce::jmax (6, high - low + 1);
            g.setColour (juce::Colour (0xfff4eeea).withAlpha (0.85f));
            for (const auto& n : dropPreview->notes)
            {
                const auto x1 = timeline.xForBeat (dropPreview->start + n.start), x2 = timeline.xForBeat (dropPreview->start + n.start + n.length);
                g.fillRect (juce::Rectangle<float> (x1, body.getBottom() - (float) (n.pitch - low + 1) * row, juce::jmax (1.5f, x2 - x1 - 1.0f),
                                                    juce::jmax (1.5f, row - 1.0f)));
            }
        }
        g.setColour (juce::Colour (0xfff4eeea));
        g.setFont (font (11.0f));
        g.drawText (dropPreview->name + (dropPreview->moreTracks > 0 ? "  (+" + juce::String (dropPreview->moreTracks) + " more on new tracks)" : juce::String()),
                    r.reduced (6.0f, 3.0f), juce::Justification::topLeft, true);
    }

    // The playhead: a line down the lanes and a marker on the ruler (where it's drawn: it glides, 0.17.4).
    const auto x = timeline.xForBeat (shownBeat);
    if (x >= -2.0f && x <= bounds.getWidth() + 2.0f)
    {
        const auto red = studio.getEngine().isRecording();
        // Pearl white with a magenta-pink glow (0.9.12, the owner asked); red while recording.
        const auto glowColour = red ? juce::Colour (0xffff3b2f) : pr7s::FamilyColours::pink;
        const auto lineColour = red ? juce::Colour (0xffff3b2f) : juce::Colour (0xfff4eeea);
        g.setColour (glowColour.withAlpha (0.18f));
        g.fillRect (juce::Rectangle<float> (x - 4.0f, (float) Timeline::rulerHeight, 9.2f, bounds.getHeight()));
        g.setColour (glowColour.withAlpha (0.4f));
        g.fillRect (juce::Rectangle<float> (x - 1.5f, (float) Timeline::rulerHeight, 4.2f, bounds.getHeight()));
        const auto colour = lineColour;
        g.setColour (colour);
        g.fillRect (juce::Rectangle<float> (x, (float) Timeline::rulerHeight - 6.0f, 1.4f, bounds.getHeight()));
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
    if (area.getRight() < 0.0f || area.getX() > (float) getWidth() || ! g.clipRegionIntersects (area.toNearestIntEdges().expanded (2)))
        return;   // (off screen, or outside what's being redrawn: the playhead's strip)
    if (part.muted)
        colour = juce::Colour (0xff6a646c);   // muted: greyed out (pearl onyx), as the manual has it
    // The body subdued (0.9.11, the owner asked: about 15% as bright as it
    // was), its edge in the track's colour; the title strip, the notes and
    // the waveform as bright as before.
    g.setColour (colour.withAlpha (0.05f));
    g.fillRoundedRectangle (area, 4.0f);
    g.setColour (colour.withAlpha (0.45f));
    g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.0f);
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
        if (const auto data = studio.getHeardAudio (part.audioFile))
        {
            // (Each pixel's seconds by the tempo map: with the tempo track, a pixel isn't always as long.)
            const auto tempo = studio.getProject().tempoMap();
            const auto startSeconds = tempo.secondsAt (part.start);
            const auto secondsAtX = [&] (float x)
            {
                // (0.32.0) A warped clip: where its markers put the recording.
                const auto beat = (x - area.getX()) / timeline.pixelsPerBeat;
                return part.isWarped() ? pr7s::warpSecondsAt (part, beat, tempo)
                                       : part.audioOffset + tempo.secondsAt (part.start + beat) - startSeconds;
            };
            const auto peaksPerSecond = data->sampleRate / pr7s::Studio::AudioData::peakStep;
            const auto clip = wave.getIntersection (getLocalBounds().toFloat()).getIntersection (g.getClipBounds().toFloat().expanded (1.0f));
            g.setColour (colour.brighter (0.7f).withAlpha (0.9f));
            // (0.81.0) A stereo recording: each side in its own half (left over right), with room for both;
            // a mono one: one waveform, as before.
            const auto twoSides = data->stereo && wave.getHeight() >= 24.0f && data->leftPeaks.size() == data->peaks.size();
            const auto lanes = twoSides ? 2 : 1;
            for (int side = 0; side < lanes; ++side)
            {
                const auto& list = ! twoSides ? data->peaks : side == 0 ? data->leftPeaks : data->rightPeaks;
                const auto lane = twoSides ? (side == 0 ? wave.withHeight (wave.getHeight() * 0.5f).withTrimmedBottom (1.0f)
                                                        : wave.withTrimmedTop (wave.getHeight() * 0.5f).withTrimmedTop (1.0f))
                                           : wave;
                for (auto x = std::floor (clip.getX()); x < clip.getRight(); x += 1.0f)
                {
                    const auto from = secondsAtX (x), to = secondsAtX (x + 1.0f);
                    const auto first = (int) (from * peaksPerSecond), last = juce::jmax (first + 1, (int) (to * peaksPerSecond));
                    auto peak = 0.0f;
                    for (int i = juce::jmax (0, first); i < juce::jmin (last, (int) list.size()); ++i)
                        peak = juce::jmax (peak, list[(size_t) i]);
                    const auto height = juce::jmin (1.0f, peak * part.gain()) * lane.getHeight() * 0.5f;
                    g.fillRect (juce::Rectangle<float> (x, lane.getCentreY() - height, 1.0f, juce::jmax (0.5f, height * 2.0f)));
                }
            }
            if (twoSides)
            {
                g.setColour (colour.withAlpha (0.35f));
                g.fillRect (juce::Rectangle<float> (clip.getX(), wave.getCentreY() - 0.5f, clip.getWidth(), 1.0f));
            }
        }
        else
        {
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.setFont (font (10.0f, juce::Font::plain));
            g.drawText ("Recording missing: " + part.audioFile, wave, juce::Justification::centred, true);
        }

        // The fades: the faded corners darkened, a line along each, and its handle.
        const auto inX = area.getX() + fadeInWidth (part), outX = area.getRight() - fadeOutWidth (part);
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

        // (0.32.0) The selected clip's hitpoints (the manual: "shown for the selected event, provided
        // that the zoom factor is high enough"): thin pearl lines, a small triangle at their top.
        if ((selected || tool == Tool::warp) && part.id != 0)
        {
            const auto hits = studio.getPartHitpoints (part.id);
            auto roomy = true;
            for (size_t i = 1; i < hits.size(); ++i)
                roomy = roomy && (hits[i] - hits[i - 1]) * timeline.pixelsPerBeat >= hitpointRoom;
            if (roomy)
                for (const auto beat : hits)
                {
                    const auto x = area.getX() + (float) (beat * timeline.pixelsPerBeat);
                    g.setColour (juce::Colour (0xfff4eeea).withAlpha (0.35f));
                    g.fillRect (juce::Rectangle<float> (x, wave.getY(), 1.0f, wave.getHeight()));
                    juce::Path tip;
                    tip.addTriangle (x - 3.0f, wave.getY(), x + 3.0f, wave.getY(), x, wave.getY() + 4.0f);
                    g.setColour (juce::Colour (0xfff4eeea).withAlpha (0.7f));
                    g.fillPath (tip);
                }
        }
        // Its warp markers: an orange line, a handle at the top, and how much the audio before it is stretched.
        if (part.isWarped())
        {
            const auto factors = pr7s::warpFactors (part, studio.getProject().tempoMap());
            for (size_t k = 1; k + 1 < part.warp.size(); ++k)
            {
                const auto x = area.getX() + (float) (part.warp[k].beat * timeline.pixelsPerBeat);
                g.setColour (ParagonColours::accent);
                g.fillRect (juce::Rectangle<float> (x - 0.75f, wave.getY(), 1.5f, wave.getHeight()));
                g.fillRect (juce::Rectangle<float> (x - 4.0f, wave.getY(), 8.0f, 6.0f));
                // (Only where it's stretched or squeezed, and where the label has room before it.)
                const auto room = (part.warp[k].beat - part.warp[k - 1].beat) * timeline.pixelsPerBeat;
                if (area.getHeight() > 34.0f && room > 36.0 && std::abs (factors[k] - 1.0) > 0.005)
                {
                    g.setFont (font (10.0f));
                    g.drawText (juce::String (factors[k], 2), juce::Rectangle<float> (x - 34.0f, wave.getY() + 7.0f, 31.0f, 12.0f),
                                juce::Justification::centredRight, false);
                }
            }
            g.setColour (ParagonColours::accent.withAlpha (0.8f));
            g.setFont (font (10.0f));
            g.drawText ("WARP", area.withTrimmedTop (1.0f).withHeight (13.0f).reduced (5.0f, 0.0f), juce::Justification::centredRight, false);
        }
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
        const auto clipX = g.getClipBounds().toFloat().expanded (2.0f, 0.0f);   // only the notes in what's being redrawn
        for (const auto& note : part.notes)
        {
            const auto nx = area.getX() + (float) (note.start * timeline.pixelsPerBeat);
            const auto nw = juce::jmax (1.5f, (float) (juce::jmin (note.length, part.length - note.start) * timeline.pixelsPerBeat));
            if (nx > clipX.getRight() || nx + nw < clipX.getX())
                continue;
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

float ArrangeView::fadeInWidth (const pr7s::Part& part) const
{
    const auto tempo = studio.getProject().tempoMap();
    return (float) ((tempo.beatAfter (part.start, part.fadeIn) - part.start) * timeline.pixelsPerBeat);
}

float ArrangeView::fadeOutWidth (const pr7s::Part& part) const
{
    const auto tempo = studio.getProject().tempoMap();
    return (float) ((part.end() - tempo.beatAt (tempo.secondsAt (part.end()) - part.fadeOut)) * timeline.pixelsPerBeat);
}

bool ArrangeView::isAudioTrack (int trackId) const
{
    const auto* track = studio.getProject().findTrack (trackId);
    return track != nullptr && track->isAudio();
}

juce::Rectangle<float> ArrangeView::fadeHandle (juce::Rectangle<float> bounds, const pr7s::Part& part, bool fadeIn) const
{
    // A small square just under the name band, at the fade's inner end.
    const auto x = fadeIn ? bounds.getX() + fadeInWidth (part) + 4.0f
                          : bounds.getRight() - fadeOutWidth (part) - 4.0f;
    return juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ x, bounds.getY() + 20.0f });
}

//==============================================================================
void ArrangeView::mouseMove (const juce::MouseEvent& event)
{
    // The ruler's lower part (under the cycle strip): where a click would put the playhead, snapped (Ctrl: free).
    if (event.position.y >= (float) Timeline::cycleStripHeight && event.position.y < (float) Timeline::rulerHeight
        && ! event.mods.isAltDown() && ! event.mods.isCtrlDown())
    {
        const auto grid = event.mods.isCommandDown() ? 0.0 : timeline.snapGrid (studio.getProject());
        setRulerHover (juce::jmax (0.0, pr7s::snap (timeline.beatForX (event.position.x), grid)));
    }
    else
        setRulerHover (std::nullopt);
    updateCursor (event);
}

void ArrangeView::mouseExit (const juce::MouseEvent&)
{
    setRulerHover (std::nullopt);
}

void ArrangeView::updateCursor (const juce::MouseEvent& event)
{
    // The ruler's top strip: a locator's handle, or the range (a hand: drag to move it).
    if (event.position.y < (float) Timeline::cycleStripHeight)
    {
        const auto& project = studio.getProject();
        const auto x1 = timeline.xForBeat (project.loopStart), x2 = timeline.xForBeat (project.loopEnd);
        const auto x = event.position.x;
        setMouseCursor (std::abs (x - x1) <= 6.0f || std::abs (x - x2) <= 6.0f ? juce::MouseCursor::LeftRightResizeCursor
                        : x > x1 && x < x2 ? juce::MouseCursor::DraggingHandCursor
                                           : juce::MouseCursor::NormalCursor);
        return;
    }
    // (0.85.3) A chord's edge: it can be dragged longer or shorter.
    if (const auto row = rowAt (event.position.y); row && row->kind == Timeline::Row::Kind::chords && tool != Tool::split && tool != Tool::glue)
    {
        auto atStart = false;
        if (chordEdgeAt (event.position, atStart) >= 0)
        {
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
    }
    // (0.30.0) COMP over a split or a take's edge: it can be dragged.
    if (tool == Tool::comp && ! event.mods.isCommandDown() && takeEdgeAt (event.position))
    {
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        return;
    }
    // (0.27.0) An automation lane: its handles' pointers, and the bend handle under the mouse.
    if (const auto row = rowAt (event.position.y); row && row->kind == Timeline::Row::Kind::lane)
    {
        const auto bend = tool == Tool::select ? curveHandleAt (row->busId, row->laneIndex, event.position) : -1;
        if (bend != hoverCurve.point || row->busId != hoverCurve.busId || row->laneIndex != hoverCurve.laneIndex)
        {
            hoverCurve = { row->busId, row->laneIndex, bend };
            repaint (getLaneArea (row->busId, row->laneIndex).expanded (0.0f, 4.0f).toNearestIntEdges());
        }
        const auto h = pointSelection.busId == row->busId && pointSelection.laneIndex == row->laneIndex && tool == Tool::select
                           ? pointHandleAt (event.position, event.mods.isAltDown()) : PointHandle::none;
        if (h == PointHandle::stretch)
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        else if (h != PointHandle::none || bend >= 0)
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        else if (tool == Tool::line)
            setMouseCursor (juce::MouseCursor::CrosshairCursor);
        else if (tool != Tool::select)
        {
            auto found = toolCursors.find ((int) tool);
            if (found == toolCursors.end())
                found = toolCursors.emplace ((int) tool, ToolIcons::cursor (tool)).first;
            setMouseCursor (found->second);
        }
        else
            setMouseCursor (juce::MouseCursor::NormalCursor);
        return;
    }
    if (hoverCurve.point >= 0)
    {
        hoverCurve.point = -1;
        repaint();
    }
    if (tool == Tool::split)
    {
        // The cut line's old and new strips only: where the line is drawn
        // (snapped to the grid), not where the mouse is (0.9.11: they
        // differed, and the old lines stayed on screen).
        const auto strip = [this] (float x) { return juce::Rectangle<float> (x - 3.0f, 0.0f, 7.0f, (float) getHeight()).toNearestIntEdges(); };
        if (const auto was = splitLineX(); was >= 0.0f)
            repaint (strip (was));
        hover = partAt (event.position) ? event.position : juce::Point<float> { -1.0f, -1.0f };
        if (const auto now = splitLineX(); now >= 0.0f)
            repaint (strip (now));
    }
    if (tool != Tool::select)
    {
        // Over the tracks: the tool's icon as the pointer (so the tool in use shows).
        if (event.position.y < (float) Timeline::rulerHeight)
        {
            setMouseCursor (juce::MouseCursor::NormalCursor);
            return;
        }
        auto found = toolCursors.find ((int) tool);
        if (found == toolCursors.end())
            found = toolCursors.emplace ((int) tool, ToolIcons::cursor (tool)).first;
        setMouseCursor (found->second);
        return;
    }
    if (const auto hit = partAt (event.position))
    {
        const auto bounds = getPartBounds (hit->first, hit->second);
        const auto& part = studio.getProject().findTrack (hit->first)->parts[(size_t) hit->second];
        const auto onHandle = part.isAudio() && (fadeHandle (bounds, part, true).expanded (3.0f).contains (event.position)
                                                 || fadeHandle (bounds, part, false).expanded (3.0f).contains (event.position));
        const auto onEdge = event.position.x > bounds.getRight() - edgeGrab || event.position.x < bounds.getX() + edgeGrab;
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
    if (const auto row = rowAt (event.position.y); row && row->kind == Timeline::Row::Kind::tempo && tempoDown (event))
        return;
    if (const auto row = rowAt (event.position.y); row && row->kind == Timeline::Row::Kind::chords && chordDown (event))
        return;
    if (const auto row = rowAt (event.position.y); row && row->kind == Timeline::Row::Kind::signature && signatureDown (event))
        return;
    if (const auto row = rowAt (event.position.y); row && row->kind == Timeline::Row::Kind::markers && markerDown (event))
        return;
    if (! event.mods.isShiftDown())
        markerSelection.clear();
    if (! event.mods.isShiftDown())
        signatureSelection.clear();
    if (! event.mods.isShiftDown())
        chordSelection.clear();
    if (! event.mods.isShiftDown())
        tempoSelection.clear();
    if (! event.mods.isShiftDown())
        pointSelection = {};
    if (event.position.y < (float) Timeline::rulerHeight)
    {
        dragStartBeat = juce::jmax (0.0, pr7s::snap (timeline.beatForX (event.position.x), timeline.snapGrid (project)));
        // Ctrl-click: the left locator here; Alt-click: the right one; both:
        // round the grid step clicked in (the manual's). Anywhere in the ruler:
        // with Ctrl or Alt held, a click never moves the playhead.
        const auto ctrl = event.mods.isCommandDown(), alt = event.mods.isAltDown();
        if (ctrl || alt)
        {
            drag = Drag::none;
            if (ctrl && alt)
            {
                const auto from = std::floor (timeline.beatForX (event.position.x) / grid + 1.0e-9) * grid;
                studio.setCycle (juce::jmax (0.0, from), juce::jmax (0.0, from) + grid);
            }
            else if (ctrl)
                studio.setCycle (juce::jmin (dragStartBeat, project.loopEnd), juce::jmax (dragStartBeat, project.loopEnd));
            else
                studio.setCycle (juce::jmin (project.loopStart, dragStartBeat), juce::jmax (project.loopStart, dragStartBeat));
            return;
        }
        if (event.position.y < (float) Timeline::cycleStripHeight)
        {
            // A locator's handle (either end): just that one moves.
            const auto x1 = timeline.xForBeat (project.loopStart), x2 = timeline.xForBeat (project.loopEnd);
            if (std::abs (event.position.x - x1) <= 6.0f || std::abs (event.position.x - x2) <= 6.0f)
            {
                drag = std::abs (event.position.x - x1) <= std::abs (event.position.x - x2) ? Drag::cycleLeft : Drag::cycleRight;
                dragOriginalStart = project.loopStart;
                dragOriginalLength = project.loopEnd - project.loopStart;
                return;
            }
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
            // (0.78.0) Held and dragged up or down: a zoom instead (see mouseDrag).
            rulerZooming = false;
            rulerDownX = event.position.x;
            rulerDownY = event.position.y;
            rulerZoomBeat = timeline.beatForX (event.position.x);
            rulerZoomFrom = timeline.pixelsPerBeat;
        }
        return;
    }

    if ((tool == Tool::select || tool == Tool::comp) && ! event.mods.isPopupMenu() && compHandleDown (event))
        return;
    if (tool == Tool::select && ! event.mods.isPopupMenu() && folderDown (event))
        return;
    if (toolDown (event))
        return;
    const auto hit = partAt (event.position);
    if (! hit)
    {
        drag = Drag::none;
        if (! event.mods.isShiftDown())
        {
            selection = {};
            alsoSelected.clear();
        }
        if (const auto trackId = trackAtY (event.position.y))
            studio.selectTrack (trackId);
        // SELECT: a drag from here draws a rectangle; the parts it touches are selected.
        if (tool == Tool::select && ! event.mods.isPopupMenu())
        {
            drag = Drag::marquee;
            toolBand = {};
            marqueeBeat = timeline.beatForX (event.position.x);
            marqueeRowY = event.position.y - (float) Timeline::rulerHeight + (float) timeline.scrollY;
            beginDragAutoRepeat (30);   // (drag events keep coming while the mouse is held past an edge)
        }
        repaint();
        return;
    }

    // Slip Event (0.30.0, the manual's Alt+Shift drag): what plays inside the part slides.
    if ((tool == Tool::select || tool == Tool::comp) && event.mods.isAltDown() && event.mods.isShiftDown() && ! event.mods.isPopupMenu()
        && ! project.findTrack (hit->first)->frozen)
    {
        select (hit->first, hit->second);
        slipPartId = project.findTrack (hit->first)->parts[(size_t) hit->second].id;
        slipFrom = timeline.beatForX (event.position.x);
        slipDelta = 0.0;
        drag = Drag::slip;
        return;
    }

    // Shift-click: that part added to the selection, or taken out of it.
    if (event.mods.isShiftDown() && ! event.mods.isPopupMenu())
    {
        drag = Drag::none;
        const auto id = partIdAt ({ hit->first, hit->second });
        if (selection.trackId == hit->first && selection.index == hit->second)
        {
            selection = {};
            if (! alsoSelected.empty())
            {
                if (const auto at = studio.findPart (alsoSelected.back()))
                    selection = selectionOf (at->first, at->second);
                alsoSelected.pop_back();
            }
        }
        else if (const auto it = std::find (alsoSelected.begin(), alsoSelected.end(), id); it != alsoSelected.end())
            alsoSelected.erase (it);
        else
        {
            if (const auto primary = partIdAt (selection))
                alsoSelected.push_back (primary);
            selection = selectionOf (hit->first, hit->second);
            studio.selectTrack (hit->first);
        }
        repaint();
        return;
    }

    // (0.84.1, the other DAW's habit) Ctrl-click adds a part to the selection; Ctrl-click on a selected one takes
    // it out when let go without a drag (a Ctrl-drag still moves them all, along one direction).
    ctrlToggledPart = 0;
    if (event.mods.isCommandDown() && ! event.mods.isPopupMenu() && tool == Tool::select)
    {
        const auto id = partIdAt ({ hit->first, hit->second });
        if (! isSelected (hit->first, hit->second))
        {
            if (const auto primary = partIdAt (selection); primary != 0
                && std::find (alsoSelected.begin(), alsoSelected.end(), primary) == alsoSelected.end())
                alsoSelected.push_back (primary);
            selection = selectionOf (hit->first, hit->second);
            studio.selectTrack (hit->first);
        }
        else
            ctrlToggledPart = id;
    }

    // A part among several selected: they stay selected (it's the one shown in the key editor now).
    if (isSelected (hit->first, hit->second) && getSelectedParts().size() > 1)
    {
        const auto id = partIdAt ({ hit->first, hit->second });
        if (const auto primary = partIdAt (selection); primary != 0 && primary != id)
            alsoSelected.push_back (primary);
        alsoSelected.erase (std::remove (alsoSelected.begin(), alsoSelected.end(), id), alsoSelected.end());
        selection = selectionOf (hit->first, hit->second);
        studio.selectTrack (hit->first);
    }
    else
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
    // (0.77.1, the owner: "why can't I grab the left side of anything") A MIDI part's left edge too: its
    // start moves, its notes stay where they are in the song.
    else if (drag == Drag::move && event.position.x < bounds.getX() + edgeGrab)
        drag = Drag::trimStart;
    dragPart = part;
    dragCopies = drag == Drag::move && event.mods.isAltDown();   // Alt copies (Ctrl locks the direction)
    dragTrack = dragTargetTrack = hit->first;
    dragTargetLane = -1;
    dragStartBeat = timeline.beatForX (event.position.x);
    dragOriginalStart = dragNewStart = part.start;
    dragOriginalLength = dragNewLength = part.length;
}

void ArrangeView::mouseDrag (const juce::MouseEvent& event)
{
    if (drag == Drag::folderMove)
    {
        const auto grid = event.mods.isCommandDown() ? 0.0 : timeline.snapGrid (studio.getProject());
        auto delta = timeline.beatForX (event.position.x) - folderDownBeat;
        delta = grid > 0.0 ? std::round (delta / grid) * grid : delta;
        // (none before the song's start)
        for (const auto id : folderDragIds)
            if (const auto where = studio.findPart (id))
                delta = juce::jmax (delta, -studio.getProject().findTrack (where->first)->parts[(size_t) where->second].start);
        if (std::abs (delta - folderDelta) > 1.0e-9)
        {
            folderDelta = delta;
            dragMoved = std::abs (delta) > 1.0e-9;
            repaint();
        }
        return;
    }
    if (drag == Drag::markerMove || drag == Drag::markerStart || drag == Drag::markerEnd || drag == Drag::markerNewCycle)
    {
        markerDrag (event);
        return;
    }
    if (drag == Drag::signatureMove)
    {
        signatureDrag (event);
        return;
    }
    if (drag == Drag::chordMove || drag == Drag::chordStart || drag == Drag::chordEnd)
    {
        chordDragTo (event);
        return;
    }
    if (drag == Drag::tempoPoints || drag == Drag::tempoBand || drag == Drag::tempoDraw)
    {
        tempoDrag (event);
        return;
    }
    if (drag == Drag::points || drag == Drag::band || drag == Drag::draw || drag == Drag::erasePoints || drag == Drag::linePoints
        || drag == Drag::pointHandle || drag == Drag::curve || drag == Drag::segmentScale)
    {
        automationDrag (event);
        return;
    }
    if (drag == Drag::rangeNew || drag == Drag::rangeStart || drag == Drag::rangeEnd || drag == Drag::rangeMove)
    {
        rangeDrag (event);
        return;
    }
    if (drag == Drag::marquee)
    {
        if (event.getDistanceFromDragStart() > 3)
            dragMoved = true;
        // Past an edge the view scrolls (along the song and down the tracks),
        // so parts off the screen can be taken in; the box's start stays in the song.
        const auto scrolledNow = dragMoved && scrollTowards (event.position);
        const juce::Point<float> anchor (timeline.xForBeat (marqueeBeat), marqueeRowY + (float) Timeline::rulerHeight - (float) timeline.scrollY);
        const auto before = toolBand;
        toolBand = juce::Rectangle<float> (anchor, event.position);
        if (scrolledNow)
            repaint();   // (the view moved: all of it)
        else if (toolBand != before)
            repaintBandChange (before, toolBand);
        return;
    }
    if (drag == Drag::comp || drag == Drag::takeEdge || drag == Drag::slip || drag == Drag::audition)
    {
        compDrag (event);
        return;
    }
    if (drag == Drag::warpMarker)
    {
        warpDrag (event);
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
                if (const auto grid = timeline.snapGrid (studio.getProject()); grid > 0.0)
                {
                    start = std::floor (start / grid + 1.0e-9) * grid;
                    end = juce::jmax (start + grid, std::ceil (end / grid - 1.0e-9) * grid);
                }
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
    const auto grid = noSnap ? 0.0 : timeline.snapGrid (project);
    const auto beat = timeline.beatForX (event.position.x);
    switch (drag)
    {
        case Drag::position:
        {
            // (0.78.0) Up or down past a few pixels: zoom (down out, up in), the clicked beat held under
            // the mouse's first place; the playhead stays where the click put it.
            const auto dy = event.position.y - rulerDownY;
            if (! rulerZooming && std::abs (dy) >= rulerZoomStart && std::abs (dy) > std::abs (event.position.x - rulerDownX))
                rulerZooming = true;
            if (rulerZooming)
            {
                timeline.pixelsPerBeat = juce::jlimit (2.0, 400.0, rulerZoomFrom * std::pow (2.0, (double) dy / rulerZoomPixelsPerDouble)   /* (0.78.1: down zooms in, as the manual) */);
                timeline.firstBeat = juce::jmax (0.0, rulerZoomBeat - (double) rulerDownX / timeline.pixelsPerBeat);
                setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
                scrolled();
                repaint();
                break;
            }
            studio.setPosition (juce::jmax (0.0, pr7s::snap (beat, grid)));
            break;
        }
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
        case Drag::cycleLeft:
        case Drag::cycleRight:
        {
            // One locator; past the other, they swap.
            dragMoved = true;
            const auto now = juce::jmax (0.0, pr7s::snap (beat, grid));
            const auto other = drag == Drag::cycleLeft ? dragOriginalStart + dragOriginalLength : dragOriginalStart;
            if (std::abs (now - other) > 1.0e-9)
                studio.setCycle (juce::jmin (now, other), juce::jmax (now, other));
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
            // (0.83.4) Over a window of ours that takes parts (the composer): held there, handed over on release.
            if (dragMoved && getSelectedParts().size() <= 1 && selection.isValid())
            {
                const auto screen = event.getScreenPosition().toFloat();
                PartDropTarget* target = nullptr;
                if (findPartTarget)
                    target = findPartTarget (screen);
                else if (auto* found = PartDropTarget::at (screen))
                    if (auto* c = dynamic_cast<juce::Component*> (found); c != nullptr && c->getTopLevelComponent() != getTopLevelComponent())
                        target = found;   // (never one in the arrangement's own window)
                if (target != nullptr && ! target->isInterestedInPart (partIdAt (selection)))
                    target = nullptr;
                setPartTarget (target);
                if (target != nullptr)
                    break;
            }
            if (dragMoved && wantsDragOut (event))
            {
                dragPartOut();
                break;
            }
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
            // Onto a lane of takes (0.29.0): that lane.
            dragTargetLane = dragTargetTrack == trackAtY (event.position.y) && ! alongOnly ? laneAtY (event.position.y) : -1;
            repaint();
            break;
        }
        case Drag::resize:
            dragMoved = true;
            dragNewLength = juce::jmax (grid > 0.0 ? grid : 1.0 / 16.0, pr7s::snap (dragOriginalStart + dragOriginalLength + beat - dragStartBeat, grid) - dragOriginalStart);
            if (dragPart.isAudio())
                if (const auto data = studio.getAudio (dragPart.audioFile))   // no further than the recording goes
                    dragNewLength = juce::jmin (dragNewLength, project.tempoMap().beatAfter (dragPart.start, data->seconds() - dragPart.audioOffset) - dragPart.start);
            repaint();
            break;
        case Drag::trimStart:
        {
            // The start moves; the audio stays where it is in the song (so the offset into it moves too).
            dragMoved = true;
            const auto tempo = project.tempoMap();
            const auto end = dragOriginalStart + dragOriginalLength;
            const auto* dragged = studio.getProject().findTrack (dragTrack);
            if (dragged == nullptr || ! juce::isPositiveAndBelow (selection.index, (int) dragged->parts.size()))
            {
                drag = Drag::none;   // (the part went mid-drag)
                break;
            }
            const auto& original = dragged->parts[(size_t) selection.index];
            if (! original.isAudio())
            {
                // A MIDI part: as far left as the song's start; the notes and controllers keep their places in
                // the song (those before the new start, which wouldn't sound, are left out).
                const auto start = juce::jlimit (0.0, end - 1.0 / 16.0, pr7s::snap (dragOriginalStart + beat - dragStartBeat, grid));
                const auto delta = start - original.start;
                dragPart = original;
                dragPart.start = start;
                dragPart.length = end - start;
                dragPart.notes.clear();
                for (auto n : original.notes)
                    if (n.start - delta >= -1.0e-9)
                    {
                        n.start = juce::jmax (0.0, n.start - delta);
                        dragPart.notes.push_back (n);
                    }
                dragPart.controllers.clear();
                for (auto c : original.controllers)
                    if (c.start - delta >= -1.0e-9)
                    {
                        c.start = juce::jmax (0.0, c.start - delta);
                        dragPart.controllers.push_back (c);
                    }
                repaint();
                break;
            }
            const auto earliest = juce::jmax (0.0, tempo.beatAt (tempo.secondsAt (dragOriginalStart) - original.audioOffset));
            const auto start = juce::jlimit (earliest, end - 1.0 / 16.0, pr7s::snap (dragOriginalStart + beat - dragStartBeat, grid));
            dragPart.start = start;
            dragPart.length = end - start;
            dragPart.audioOffset = juce::jmax (0.0, original.audioOffset + tempo.secondsBetween (dragOriginalStart, start));
            dragPart.fadeIn = juce::jmin (dragPart.fadeIn, tempo.secondsBetween (start, end));
            repaint();
            break;
        }
        case Drag::fadeIn:
        case Drag::fadeOut:
        {
            dragMoved = true;
            const auto tempo = project.tempoMap();
            const auto clipSeconds = tempo.secondsBetween (dragPart.start, dragPart.end());
            if (drag == Drag::fadeIn)
                dragPart.fadeIn = juce::jlimit (0.0, clipSeconds - dragPart.fadeOut, tempo.secondsBetween (dragPart.start, beat));
            else
                dragPart.fadeOut = juce::jlimit (0.0, clipSeconds - dragPart.fadeIn, tempo.secondsBetween (beat, dragPart.end()));
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
        case Drag::rangeNew:
        case Drag::rangeStart:
        case Drag::rangeEnd:
        case Drag::rangeMove:
        case Drag::marquee:
        case Drag::comp:
        case Drag::takeEdge:
        case Drag::slip:
        case Drag::audition:
        case Drag::warpMarker:
        case Drag::none:
            break;
    }
}

void ArrangeView::setPartTarget (PartDropTarget* target)
{
    auto* component = dynamic_cast<juce::Component*> (target);
    if (partTarget.getComponent() == component)
        return;
    if (auto* old = dynamic_cast<PartDropTarget*> (partTarget.getComponent()))
        old->partDragHover (false);
    partTarget = component;
    if (target != nullptr)
        target->partDragHover (true);
}

void ArrangeView::mouseUp (const juce::MouseEvent& event)
{
    // (0.83.4) A part let go over the composer: it stays where it was and is handed over.
    if (drag == Drag::move && partTarget != nullptr)
    {
        auto* target = dynamic_cast<PartDropTarget*> (partTarget.getComponent());
        const auto partId = selection.isValid() ? partIdAt (selection) : 0;
        setPartTarget (nullptr);
        drag = Drag::none;
        dragMoved = false;
        repaint();
        if (target != nullptr && partId != 0)
            target->partDropped (partId);
        return;
    }
    setPartTarget (nullptr);
    if (std::exchange (rulerZooming, false))
        setMouseCursor (juce::MouseCursor::NormalCursor);
    if (drag == Drag::folderMove)
    {
        drag = Drag::none;
        const auto delta = std::exchange (folderDelta, 0.0);
        const auto ids = std::exchange (folderDragIds, {});
        folderDragId = 0;
        if (dragMoved && std::abs (delta) > 1.0e-9)
            studio.moveParts (ids, delta, 0, event.mods.isAltDown());   // (Alt: copies)
        dragMoved = false;
        repaint();
        return;
    }
    if (drag == Drag::markerMove || drag == Drag::markerStart || drag == Drag::markerEnd || drag == Drag::markerNewCycle)
    {
        markerUp();
        return;
    }
    if (drag == Drag::signatureMove)
    {
        signatureUp();
        return;
    }
    if (drag == Drag::chordMove || drag == Drag::chordStart || drag == Drag::chordEnd)
    {
        chordUp();
        return;
    }
    if (drag == Drag::tempoPoints || drag == Drag::tempoBand || drag == Drag::tempoDraw)
    {
        tempoUp();
        return;
    }
    if (drag == Drag::points || drag == Drag::band || drag == Drag::draw || drag == Drag::erasePoints || drag == Drag::linePoints
        || drag == Drag::pointHandle || drag == Drag::curve || drag == Drag::segmentScale)
    {
        automationUp();
        return;
    }
    if (drag == Drag::comp || drag == Drag::takeEdge || drag == Drag::slip || drag == Drag::audition)
    {
        compUp();
        return;
    }
    if (drag == Drag::warpMarker)
    {
        warpUp();
        return;
    }
    if (drag == Drag::newPart || drag == Drag::muteBand || drag == Drag::zoomBand || drag == Drag::play)
    {
        toolUp (event);
        return;
    }
    if (drag == Drag::rangeNew || drag == Drag::rangeStart || drag == Drag::rangeEnd || drag == Drag::rangeMove)
    {
        rangeUp (event);
        return;
    }
    if (drag == Drag::marquee)
    {
        beginDragAutoRepeat (0);
        drag = Drag::none;
        if (dragMoved)
        {
            // Every part the rectangle touches (Shift: added to those already selected).
            auto ids = event.mods.isShiftDown() ? getSelectedParts() : std::vector<int> {};
            for (const auto trackId : studio.getShownTracks())
            {
                const auto& parts = studio.getProject().findTrack (trackId)->parts;
                for (int i = 0; i < (int) parts.size(); ++i)
                    if (getPartBounds (trackId, i).intersects (toolBand) && std::find (ids.begin(), ids.end(), parts[(size_t) i].id) == ids.end())
                        ids.push_back (parts[(size_t) i].id);
            }
            selectParts (ids);
        }
        toolBand = {};
        repaint();
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
    if (! dragMoved && finished == Drag::move && ctrlToggledPart != 0)
    {
        // (0.84.1) A selected part Ctrl-clicked: out of the selection (another selected one leads).
        const auto id = std::exchange (ctrlToggledPart, 0);
        auto ids = getSelectedParts();
        ids.erase (std::remove (ids.begin(), ids.end(), id), ids.end());
        selectParts (ids);
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
        case Drag::cycleLeft:
        case Drag::cycleRight:
            break;
        case Drag::move:
            if (const auto ids = getSelectedParts(); ids.size() > 1)
            {
                // All of them, by as much as the one dragged (and as many tracks).
                const auto primary = partIdAt (selection);
                studio.moveParts (ids, dragNewStart - dragOriginalStart, trackIndex (dragTargetTrack) - trackIndex (dragTrack), dragCopies);
                if (! dragCopies)
                    if (const auto at = studio.findPart (primary))
                        selection = selectionOf (at->first, at->second);
            }
            else if (dragCopies)
            {
                studio.copyPart (dragTrack, selection.index, dragTargetTrack, dragNewStart, dragTargetLane);
                select (dragTargetTrack, (int) studio.getProject().findTrack (dragTargetTrack)->parts.size() - 1);
            }
            else if (std::abs (dragNewStart - dragOriginalStart) > 1.0e-9 || dragTargetTrack != dragTrack
                     || (dragTargetLane >= 0 && dragTargetLane != dragPart.lane))
            {
                studio.movePart (dragTrack, selection.index, dragTargetTrack, dragNewStart, dragTargetLane);
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
        case Drag::rangeNew:
        case Drag::rangeStart:
        case Drag::rangeEnd:
        case Drag::rangeMove:
        case Drag::marquee:
        case Drag::comp:
        case Drag::takeEdge:
        case Drag::slip:
        case Drag::audition:
        case Drag::warpMarker:
        case Drag::none:
            break;
    }
    repaint();
}

//==============================================================================
// The Comp tool (0.29.0, the manual's Assembling Operations: "Bring to front: click on a take.
// Click twice to toggle"; "Comping: click and drag on a lane. All takes are split at the start
// and end of the range"). Shift-click selects and Alt-click splits, as SELECT; on the track's
// own row it moves parts as SELECT does.
bool ArrangeView::compDown (const juce::MouseEvent& event)
{
    const auto trackId = trackAtY (event.position.y);
    const auto* track = studio.getProject().findTrack (trackId);
    if (track == nullptr || track->isChannel())
        return false;
    const auto lane = laneAtY (event.position.y);
    const auto beat = juce::jmax (0.0, timeline.beatForX (event.position.x));
    // Ctrl: audition (the manual's Speaker tool), from here, only this track (a lane: only it) while held.
    if (event.mods.isCommandDown())
    {
        playReturn = studio.getEngine().getPosition();
        if (studio.getEngine().isPlaying())
            studio.stop();
        studio.setPosition (beat);
        studio.startAudition (trackId, lane);
        studio.play();
        drag = Drag::audition;
        return true;
    }
    if (event.mods.isShiftDown() || track->frozen)
        return false;   // (Shift-click selects; Alt+Shift slips: SELECT's)
    // Alt-click: every take cut there.
    if (event.mods.isAltDown())
    {
        studio.splitTakesAt (trackId, pr7s::snap (beat, timeline.snapGrid (studio.getProject())));
        drag = Drag::none;
        return true;
    }
    // A split, or a take's edge: moved (all the takes sharing it).
    if (const auto edge = takeEdgeAt (event.position))
    {
        compTrack = trackId;
        edgeFrom = edgeTo = *edge;
        drag = Drag::takeEdge;
        return true;
    }
    if (lane < 0)
    {
        // The track's own row: the takes stacked there move together (SELECT's move does it).
        if (const auto hit = partAt (event.position); hit && hit->first == trackId)
        {
            const auto id = track->parts[(size_t) hit->second].id;
            auto ids = studio.stackedWith (id);
            ids.erase (std::remove (ids.begin(), ids.end(), id), ids.end());
            ids.insert (ids.begin(), id);
            selectParts (ids);
        }
        return false;
    }
    compTrack = trackId;
    compLane = lane;
    compFrom = compTo = beat;
    compPartId = 0;
    if (const auto hit = partAt (event.position); hit && hit->first == trackId)
        compPartId = track->parts[(size_t) hit->second].id;
    drag = Drag::comp;
    return true;
}

void ArrangeView::compDrag (const juce::MouseEvent& event)
{
    if (event.getDistanceFromDragStart() > 3)
        dragMoved = true;
    const auto grid = timeline.snapGrid (studio.getProject());
    const auto beat = timeline.beatForX (event.position.x);
    if (drag == Drag::takeEdge)
    {
        edgeTo = juce::jmax (0.0, pr7s::snap (beat, grid));
        repaint();
        return;
    }
    if (drag == Drag::slip)
    {
        slipDelta = pr7s::snap (beat - slipFrom, grid);
        repaint();
        return;
    }
    if (drag == Drag::audition)
        return;
    const auto a = timeline.beatForX (event.mouseDownPosition.x);
    compFrom = juce::jmax (0.0, pr7s::snap (juce::jmin (a, beat), grid));
    compTo = juce::jmax (0.0, pr7s::snap (juce::jmax (a, beat), grid));
    repaint();
}

void ArrangeView::frontClick (int trackId, int partId, double beat)
{
    // To the front; on the one heard there already, back (the one it hid): "click twice to toggle".
    const auto* track = studio.getProject().findTrack (trackId);
    auto heardHere = false;
    if (track != nullptr)
        for (int i = 0; i < (int) track->parts.size(); ++i)
            if (track->parts[(size_t) i].id == partId)
                for (const auto& [from, to] : pr7s::heardStretches (*track, i))
                    heardHere = heardHere || (beat >= from && beat < to);
    studio.bringToFront (partId, heardHere);
}

void ArrangeView::compUp()
{
    const auto finished = drag;
    drag = Drag::none;
    if (finished == Drag::audition)
    {
        studio.stop();
        studio.setPosition (playReturn);
        studio.stopAudition();
    }
    else if (finished == Drag::takeEdge)
    {
        if (dragMoved && std::abs (edgeTo - edgeFrom) > 1.0e-9)
            studio.moveTakeEdge (compTrack, edgeFrom, edgeTo);
        edgeFrom = edgeTo = 0.0;
    }
    else if (finished == Drag::slip)
    {
        if (dragMoved && std::abs (slipDelta) > 1.0e-9)
            studio.slipPart (slipPartId, slipDelta);
        slipPartId = 0;
        slipDelta = 0.0;
    }
    else if (dragMoved)
    {
        if (compTo > compFrom + 1.0e-9)
            studio.compRange (compTrack, compLane, compFrom, compTo);
    }
    else if (compPartId != 0)
    {
        frontClick (compTrack, compPartId, compFrom);
    }
    compFrom = compTo = 0.0;
    compPartId = 0;
    repaint();
}

std::optional<double> ArrangeView::takeEdgeAt (juce::Point<float> point) const
{
    const auto* track = studio.getProject().findTrack (trackAtY (point.y));
    if (track == nullptr || track->isChannel())
        return std::nullopt;
    const auto lane = laneAtY (point.y);
    if (lane < 0 && ! track->showLanes)
        return std::nullopt;   // (lanes hidden: the parts' own edges are SELECT's)
    std::optional<double> best;
    auto nearest = 5.0f;   // px
    for (const auto& p : track->parts)
    {
        if (lane >= 0 && p.lane != lane)
            continue;
        for (const auto b : { p.start, p.end() })
            if (const auto d = std::abs (timeline.xForBeat (b) - point.x); d < nearest)
            {
                nearest = d;
                best = b;
            }
    }
    return best;
}

juce::Rectangle<float> ArrangeView::compHandleBounds (int trackId, int index) const
{
    const auto* track = studio.getProject().findTrack (trackId);
    if (track == nullptr || ! juce::isPositiveAndBelow (index, (int) track->parts.size()))
        return {};
    const auto& part = track->parts[(size_t) index];
    const auto stacked = std::any_of (track->parts.begin(), track->parts.end(), [&part] (const pr7s::Part& o)
                                      { return &o != &part && o.start < part.end() - 1.0e-9 && part.start < o.end() - 1.0e-9; });
    const auto bounds = getPartBounds (trackId, index);
    if (! stacked || bounds.getWidth() < 30.0f)
        return {};
    return { bounds.getCentreX() - 9.0f, bounds.getBottom() - 7.0f, 18.0f, 7.0f };
}

bool ArrangeView::compHandleDown (const juce::MouseEvent& event)
{
    const auto hit = partAt (event.position);
    if (! hit || ! compHandleBounds (hit->first, hit->second).expanded (2.0f).contains (event.position))
        return false;
    const auto* track = studio.getProject().findTrack (hit->first);
    const auto& part = track->parts[(size_t) hit->second];
    drag = Drag::none;
    if (track->showLanes)
    {
        frontClick (hit->first, part.id, timeline.beatForX (event.position.x));
        return true;
    }
    // Lanes hidden (the manual's Handling Overlapping Audio): the takes stacked there, to choose from.
    juce::PopupMenu menu;
    menu.addSectionHeader ("TO FRONT");
    for (int i = 0; i < (int) track->parts.size(); ++i)
    {
        const auto& o = track->parts[(size_t) i];
        if (o.start < part.end() - 1.0e-9 && part.start < o.end() - 1.0e-9)
            menu.addItem (o.id, o.name + "  (lane " + juce::String (o.lane + 1) + ")", true, ! pr7s::heardStretches (*track, i).empty());
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (compHandleBounds (hit->first, hit->second)).toNearestInt()),
                        [this] (int chosen) { if (chosen > 0) studio.bringToFront (chosen); });
    return true;
}

//==============================================================================
const char* ArrangeView::toolName (Tool t)
{
    switch (t)
    {
        case Tool::select: return "SELECT";
        case Tool::range:  return "RANGE";
        case Tool::split:  return "SPLIT";
        case Tool::glue:   return "GLUE";
        case Tool::erase:  return "ERASE";
        case Tool::zoom:   return "ZOOM";
        case Tool::mute:   return "MUTE";
        case Tool::draw:   return "DRAW";
        case Tool::line:   return "LINE";
        case Tool::play:   return "PLAY";
        case Tool::comp:   return "COMP";
        case Tool::warp:   return "WARP";
    }
    return "";
}

int ArrangeView::toolKey (Tool t)
{
    switch (t)
    {
        case Tool::select: return 1;
        case Tool::range:  return 2;
        case Tool::split:  return 3;
        case Tool::glue:   return 4;
        case Tool::erase:  return 5;
        case Tool::zoom:   return 6;
        case Tool::mute:   return 7;
        case Tool::draw:   return 8;
        case Tool::line:   return 0;   // (no number key: the manual gives it none)
        case Tool::play:   return 9;
        case Tool::comp:   return 0;   // (no number key: the manual gives it none)
        case Tool::warp:   return 0;   // (likewise)
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
    return event.mods.isCommandDown() ? beat : pr7s::snap (beat, timeline.snapGrid (studio.getProject()));
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
    if (event.mods.isPopupMenu())
        return false;   // a right-click: the part's menu, whatever the tool
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
        case Tool::range:
            return rangeDown (event);
        case Tool::split:
            if (hit)
            {
                const auto& part = studio.getProject().findTrack (hit->first)->parts[(size_t) hit->second];
                const auto at = splitBeat (event, part);
                if (event.mods.isAltDown())
                    studio.splitPartRepeatedly (hit->first, hit->second, at);
                else
                    studio.splitPart (hit->first, hit->second, at);
                selection = selectionOf (hit->first, hit->second);   // the first half
            }
            break;
        case Tool::glue:
            if (hit)
            {
                const auto audio = isAudioTrack (hit->first);
                if (studio.glueParts (hit->first, hit->second, event.mods.isAltDown()))
                    selection = selectionOf (hit->first, hit->second);
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
        case Tool::line:
            break;   // (on the automation lanes only)
        case Tool::comp:
            return compDown (event);
        case Tool::warp:
            return warpDown (event);
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
            if (dragMoved && ! toolBand.isEmpty() && studio.getProject().findTrack (newPartTrack) != nullptr)
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
    if (automationDoubleClick (event))   // (0.27.0) a point's value typed in, a bend straightened
        return;
    if (markerDoubleClick (event))   // (0.49.0)
        return;
    // The chord track: a chord's editor; an empty spot, the chord the one before suggests (0.16.0).
    if (const auto row = rowAt (event.position.y); row && row->kind == Timeline::Row::Kind::chords)
    {
        if (event.position.y > getChordArea().getBottom())
            return;   // (the scales' strip: right-click it)
        for (int i = 0; i < (int) studio.getChordTrack().events.size(); ++i)
            if (getChordEventBounds (i).contains (event.position))
            {
                editChordEvent (i);
                return;
            }
        const auto beat = juce::jmax (0.0, pr7s::snap (timeline.beatForX (event.position.x), timeline.snapGrid (studio.getProject())));
        const auto index = studio.addChordEvent (beat, studio.suggestChordAt (beat));
        chordSelection = { index };
        studio.auditionChord (studio.getChordTrack().events[(size_t) index].chord);
        return;
    }
    // The signature track (0.45.0): an event's box, or one added at the bar.
    if (const auto row = rowAt (event.position.y); row && row->kind == Timeline::Row::Kind::signature)
    {
        const auto bar = signatureAt (event.position);
        if (bar >= 0)
            editSignature (bar, false);
        else
            editSignature (studio.getProject().signatures().barAt (timeline.beatForX (event.position.x) + 0.5 * studio.getProject().beatsPerBarAt (timeline.beatForX (event.position.x))), true);
        return;
    }
    // A tempo event: type its tempo.
    if (const auto row = rowAt (event.position.y); row && row->kind == Timeline::Row::Kind::tempo)
    {
        const auto& events = studio.getProject().tempoEvents;
        for (int i = (int) events.size() - 1; i >= 0; --i)
            if (getTempoEventPosition (i).getDistanceFrom (event.position) <= pointGrab)
            {
                editTempoEvent (i);
                return;
            }
        return;
    }
    if (tool == Tool::range && rangeDoubleClick (event))
        return;
    if (event.position.y < (float) Timeline::rulerHeight || trackAtY (event.position.y) == 0)
        return;   // (the ruler, an automation lane, the master's row)
    if (const auto hit = partAt (event.position))
    {
        select (hit->first, hit->second);
        if (onEditNotes && ! isAudioTrack (hit->first))   // a clip has no notes
            onEditNotes();
        else if (onEditAudio && isAudioTrack (hit->first))   // (0.34.0) a clip: VariAudio
            if (const auto* t = studio.getProject().findTrack (hit->first); t != nullptr && juce::isPositiveAndBelow (hit->second, (int) t->parts.size()))
                onEditAudio (t->parts[(size_t) hit->second].id);
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
        const auto at = timeline.beatForX (event.position.x);
        pr7s::Part part;
        if (at >= project.loopStart && at < project.loopEnd)
        {
            part.start = project.loopStart;
            part.length = project.loopEnd - project.loopStart;
        }
        else
        {
            part.start = juce::jmax (0.0, project.barStartAt (at));   // (by the signature there, 0.45.0)
            part.length = project.beatsPerBarAt (at);
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
    const auto ids = getSelectedParts();
    const auto several = ids.size() > 1;
    if (several)
        menu.addSectionHeader (juce::String ((int) ids.size()) + " PARTS SELECTED");
    menu.addItem (3, "Edit notes (MIDI editor)", ! isAudioTrack (trackId));
    menu.addItem (1, several ? "Duplicate them (after them, Ctrl+D)" : "Duplicate (after it)");
    menu.addItem (9, several ? "Cut them (Ctrl+X)" : "Cut (Ctrl+X)");
    menu.addItem (10, several ? "Copy them (Ctrl+C)" : "Copy (Ctrl+C)");
    menu.addItem (11, "Paste at the playhead (Ctrl+V)", hasPartsClip());
    menu.addItem (2, several ? "Delete them" : "Delete");
    menu.addSeparator();
    const auto& part = studio.getProject().findTrack (trackId)->parts[(size_t) index];
    const auto at = studio.getEngine().getPosition();
    menu.addItem (4, part.muted ? "Unmute" : "Mute");
    menu.addItem (5, "Split at the playhead (Alt+X)", at > part.start && at < part.end());
    menu.addItem (6, "Glue to the next part");
    menu.addSeparator();
    menu.addItem (7, "Render in Place...");
    menu.addItem (8, "Render in Place (as last time)");
    menu.addSeparator();
    {
        // Detect Chords (0.42.0): from MIDI or audio, onto the chord track or the pads.
        const auto detect = studio.getDetectSettings();
        juce::PopupMenu chords;
        chords.addItem (120, several ? "Onto the Chord Track (from these parts)" : "Onto the Chord Track");
        chords.addItem (121, "Onto the Chord Pads (each chord once)");
        chords.addSeparator();
        chords.addSectionHeader ("A CHORD AT MOST EVERY");
        chords.addItem (122, "Beat", true, detect.stepBeats == 1.0);
        chords.addItem (123, "Half Bar", true, detect.stepBeats == studio.getProject().beatsPerBar() / 2.0);
        chords.addItem (124, "Bar", true, detect.stepBeats == studio.getProject().beatsPerBar());
        chords.addSeparator();
        chords.addItem (125, "Plain Chords Only (major, minor)", true, detect.plainOnly);
        menu.addSubMenu (isAudioTrack (trackId) ? "Detect Chords (from the audio: a best guess)" : "Detect Chords", chords);
        // (0.44.0) A MIDI part as one of your patterns (PERFORM, IDEAS' PATTERNS: MY PATTERNS).
        menu.addItem (126, "Make a Pattern from This Part (MY PATTERNS)", ! isAudioTrack (trackId) && ! part.notes.empty());
    }
    // (0.45.0) The manual's Tempo Detection: the tempo track follows this part's beats.
    menu.addItem (127, "Tempo Detection...", onTempoDetection != nullptr);
    menu.addItem (128, "Key Check: Notes Outside the Key...", onKeyCheck != nullptr);   // (0.84.0)
    {
        // (0.36.0) The manual's Map to Chord Track: the notes written moved onto the chord track's chords.
        juce::PopupMenu map;
        for (const auto mode : pr7s::followModes())
            if (mode != pr7s::FollowMode::off)
                map.addItem (39000 + (int) mode, pr7s::followModeName (mode));
        menu.addSubMenu ("Map to Chord Track", map, ! isAudioTrack (trackId) && ! studio.getChordTrack().events.empty());
    }
    // Takes (0.29.0, the manual's Handling Overlapping Audio: the To Front submenu).
    {
        const auto& track = *studio.getProject().findTrack (trackId);
        juce::PopupMenu toFront;
        for (int i = 0; i < (int) track.parts.size(); ++i)
        {
            const auto& other = track.parts[(size_t) i];
            if (other.start < part.end() - 1.0e-9 && part.start < other.end() - 1.0e-9)
                toFront.addItem (40000 + other.id, other.name + "  (lane " + juce::String (other.lane + 1) + ")", true,
                                 ! pr7s::heardStretches (track, i).empty() && ! other.muted);
        }
        menu.addSeparator();
        menu.addSubMenu ("To Front", toFront, toFront.getNumItems() > 1);
        menu.addItem (13, track.showLanes ? "Hide Lanes" : "Show Lanes");
        menu.addItem (14, "Clean Up Lanes", track.numLanes() > 1 || track.parts.size() > 1);
        // (0.30.0) The manual's last steps of a perfect take.
        menu.addItem (15, track.isAudio() ? "Bounce Selection (the takes heard, into one clip)" : "Bounce MIDI (the takes heard, into one part)");
        menu.addItem (16, "Delete Overlaps (only what's heard, on one lane)");
        menu.addItem (17, "Merge MIDI in Loop (between the locators, onto a new track)", ! track.isAudio() && studio.getProject().loopEnd > studio.getProject().loopStart);
        if (part.isAudio())
        {
            // (0.32.0) Hitpoints and warping.
            juce::PopupMenu warp;
            warp.addItem (18, "Create Warp Markers from Hitpoints");
            warp.addItem (22, "Quantize (Q: to the quantize value)");
            warp.addItem (19, "Reset Warp (its own speed again)", part.isWarped());
            warp.addItem (20, "Flatten (written as it sounds: a new file, the markers gone)", part.isWarped());
            warp.addSeparator();
            warp.addItem (21, "Hitpoint Settings...");
            menu.addSubMenu ("Hitpoints and Warp", warp);
            menu.addItem (23, "Audio Alignment...");   // (0.33.0)
            menu.addItem (24, "VariAudio... (pitch: double-click the clip)");   // (0.34.0)
            menu.addItem (25, "Generate Harmony Voices...");                     // (0.35.0)
        }
    }
    menu.showMenuAsync (juce::PopupMenu::Options(), [this, trackId, index, ids, muted = part.muted, start = part.start, end = part.end(), partId = part.id] (int chosen)
    {
        if (chosen > 39000 && chosen < 39010)
        {
            studio.mapToChordTrack (ids.empty() ? std::vector<int> { partId } : ids, (pr7s::FollowMode) (chosen - 39000));
            return;
        }
        if (chosen > 40000)
        {
            studio.bringToFront (chosen - 40000);
            return;
        }
        if (chosen == 13)
        {
            if (const auto* t = studio.getProject().findTrack (trackId))
                studio.setShowLanes (trackId, ! t->showLanes);
            return;
        }
        if (chosen == 14)
        {
            studio.cleanUpLanes (trackId);
            return;
        }
        if (chosen == 15 || chosen == 16)
        {
            const auto chosenIds = ids.empty() ? std::vector<int> { partId } : ids;
            if (chosen == 16)
            {
                studio.deleteOverlaps (chosenIds);
                return;
            }
            juce::String error;
            if (studio.bounceTakes (chosenIds, error) == 0 && error.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Not bounced", error);
            return;
        }
        if (chosen == 24)
        {
            if (onEditAudio)
                onEditAudio (partId);
            return;
        }
        if (chosen == 25)
        {
            if (onHarmonyVoices)
                onHarmonyVoices (partId);
            return;
        }
        if (chosen == 23)
        {
            if (onOpenAudioAlignment)
                onOpenAudioAlignment();
            return;
        }
        if (chosen >= 18 && chosen <= 22)
        {
            const auto chosenIds = ids.empty() ? std::vector<int> { partId } : ids;
            if (chosen == 18)
                studio.createWarpFromHitpoints (chosenIds);
            else if (chosen == 19)
                studio.resetWarp (chosenIds);
            else if (chosen == 20)
            {
                juce::String error;
                studio.flattenWarp (chosenIds, error);
                if (error.isNotEmpty())
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Not flattened", error);
            }
            else if (chosen == 21)
                showHitpointSettings (this);
            else
                studio.quantizeAudio (chosenIds, studio.getProject().quantizeBeats);
            return;
        }
        if (chosen == 17)
        {
            auto tracks = studio.getSelectedTracks();
            if (std::find (tracks.begin(), tracks.end(), trackId) == tracks.end())
                tracks.insert (tracks.begin(), trackId);
            studio.mergeMidiInLoop (tracks);
            return;
        }
        if (chosen == 128)
        {
            if (onKeyCheck)
                onKeyCheck();
            return;
        }
        if (chosen == 127)
        {
            if (onTempoDetection)
                onTempoDetection (partId);
            return;
        }
        if (chosen == 126)
        {
            const auto name = studio.addPatternFromPart (partId);
            if (name.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Pattern made",
                                                        "\"" + name + "\" is in MY PATTERNS: in IDEAS' PATTERNS tab, the chord pads' PERFORM, "
                                                        "and the Inspector's PERFORM. Its notes become voices from the lowest up "
                                                        "(the lowest as the bass when it sits a fifth or more under the rest).");
            return;
        }
        if (chosen >= 120 && chosen <= 125)
        {
            auto detect = studio.getDetectSettings();
            const auto bar = studio.getProject().beatsPerBar();
            const auto chosenIds = ids.empty() ? std::vector<int> { partId } : ids;
            switch (chosen)
            {
                case 120:
                    if (studio.detectChordsToTrack (chosenIds, detect) == 0)
                        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "No chords found",
                                                                "Nothing in it read as a chord: it needs at least two notes sounding together (or close together).");
                    break;
                case 121: studio.detectChordsToPads (chosenIds, detect); break;
                case 122: detect.stepBeats = 1.0; studio.setDetectSettings (detect); break;
                case 123: detect.stepBeats = bar / 2.0; studio.setDetectSettings (detect); break;
                case 124: detect.stepBeats = bar; studio.setDetectSettings (detect); break;
                case 125: detect.plainOnly = ! detect.plainOnly; studio.setDetectSettings (detect); break;
                default: break;
            }
            return;
        }
        if ((chosen == 7 || chosen == 8) && onRenderInPlace)
        {
            // Each part selected (several: each its own stretch).
            std::vector<pr7s::Studio::RenderSource> sources;
            for (const auto id : ids)
                if (const auto at = studio.findPart (id))
                {
                    const auto& p = studio.getProject().findTrack (at->first)->parts[(size_t) at->second];
                    sources.push_back ({ at->first, p.start, p.end(), false });
                }
            if (sources.empty())
                sources.push_back ({ trackId, start, end, false });
            onRenderInPlace (sources, chosen == 7);
            return;
        }
        selection = selectionOf (trackId, index);
        if (chosen == 9 || chosen == 10)
        {
            copySelected (chosen == 9);
            if (onPartsCopied)
                onPartsCopied();
            return;
        }
        if (chosen == 11)
        {
            pasteParts (false);
            return;
        }
        if (chosen == 4 && ids.size() > 1)
        {
            studio.setPartsMuted (ids, ! muted);
            return;
        }
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
const pr7s::AutomationLane* ArrangeView::laneFor (int busId, int laneIndex) const
{
    const auto* lanes = studio.getAutomation (busId);
    return lanes != nullptr && juce::isPositiveAndBelow (laneIndex, (int) lanes->size()) ? &(*lanes)[(size_t) laneIndex] : nullptr;
}

juce::Rectangle<float> ArrangeView::getLaneArea (int busId, int laneIndex) const
{
    std::vector<Timeline::Row> made;
    for (const auto& row : rowsNow (made))
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
    if (lane == nullptr || ! juce::isPositiveAndBelow (point, (int) editPointsOf (*lane).size()))
        return {};
    const auto area = getLaneArea (busId, laneIndex);
    const auto& p = editPointsOf (*lane)[(size_t) point];
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
    const auto editing = (drag == Drag::points || drag == Drag::draw || drag == Drag::erasePoints || drag == Drag::pointHandle
                          || drag == Drag::curve || drag == Drag::segmentScale)
                         && pointSelection.busId == row.busId && pointSelection.laneIndex == row.laneIndex;
    const auto& points = editing ? workingPoints : editPointsOf (*lane);

    const auto virgin = studio.getAutomationSettings().virginTerritory;
    const auto trimming = isTrimming (*lane);
    // (0.34.4) Only what's being redrawn is drawn, and a margin either side of it: every line
    // that crosses into it is the same line, so the pixels are the same (the playhead's strip
    // used to rebuild and stroke the lane's whole curve over the whole song).
    const auto redrawn = g.getClipBounds().toFloat();
    const auto left = redrawn.getX() - curveMargin, right = redrawn.getRight() + curveMargin;
    const auto dotted = [&] (float y, juce::Colour c)
    {
        g.setColour (c);
        for (auto x = juce::jmax (0.0f, std::floor (left / 6.0f) * 6.0f); x < area.getRight() && x <= right; x += 6.0f)   // (multiples of 6: the same dashes)
            g.fillRect (x, y, 3.0f, 1.0f);
    };
    // A curve through points (a step for an on/off switch), flat before the first and after the
    // last; with virgin territory (0.28.0) nothing there, nor after a terminator (a gap): so it's
    // a list of stretches, each with where it starts and ends across.
    struct Stretch { juce::Path path; float x0 = 0.0f, x1 = 0.0f; };
    struct Run { std::vector<juce::Point<float>> at; };
    const auto curveOf = [&] (const std::vector<pr7s::AutomationPoint>& pts, bool step, bool gaps)
    {
        std::vector<Stretch> stretches;
        if (pts.empty())
            return stretches;
        // Each stretch's corners in order (left to right), then the path through those the redrawn
        // strip needs: those inside the margins, and the one either side of them.
        std::vector<Run> runs;
        const auto begin = [&] (float x, float y) { runs.push_back ({}); runs.back().at.push_back ({ x, y }); };
        const auto to = [&] (float x, float y) { runs.back().at.push_back ({ x, y }); };
        const auto inView = [&] (float x) { return x >= left && x <= right; };
        begin (gaps ? timeline.xForBeat (pts.front().beat) : 0.0f, yForValue (area, pts.front().value));
        for (size_t i = 0; i < pts.size(); ++i)
        {
            const auto x = timeline.xForBeat (pts[i].beat);
            if (i > 0 && gaps && pts[i - 1].terminator)
            {
                begin (x, yForValue (area, pts[i].value));   // (the gap's end: a new stretch)
                continue;
            }
            if (step && i > 0)
                to (x, yForValue (area, pts[i - 1].value));
            else if (i > 0 && std::abs (pts[i].curve) > 1.0e-4f)
            {
                // (0.27.0) A bent ramp, drawn a few pixels at a time.
                const auto x0 = timeline.xForBeat (pts[i - 1].beat);
                for (auto px = x0 + 3.0f; px < x; px += 3.0f)
                {
                    if (px < left - 3.0f || px > right + 3.0f)
                    {
                        // (Far outside: a corner only if it's the one next to the margins.)
                        if ((px < left && px + 3.0f >= left - 3.0f) || (px > right && px - 3.0f <= right + 3.0f))
                        {
                            const auto edge = (px - x0) / juce::jmax (1.0f, x - x0);
                            to (px, yForValue (area, pts[i - 1].value + (pts[i].value - pts[i - 1].value) * pr7s::AutomationLane::curveShape (edge, pts[i].curve)));
                        }
                        continue;
                    }
                    const auto along = (px - x0) / juce::jmax (1.0f, x - x0);
                    to (px, yForValue (area, pts[i - 1].value + (pts[i].value - pts[i - 1].value) * pr7s::AutomationLane::curveShape (along, pts[i].curve)));
                }
            }
            to (x, yForValue (area, pts[i].value));
        }
        if (! gaps)
            to (area.getRight(), yForValue (area, pts.back().value));
        for (const auto& run : runs)
        {
            const auto& at = run.at;
            Stretch st;
            auto started = false;
            for (size_t i = 0; i < at.size(); ++i)
            {
                const auto keep = inView (at[i].x)
                               || (at[i].x < left && i + 1 < at.size() && at[i + 1].x >= left)
                               || (at[i].x > right && i > 0 && at[i - 1].x <= right);
                if (! keep)
                    continue;
                if (! started)
                {
                    st.path.startNewSubPath (at[i]);
                    st.x0 = at[i].x;
                    started = true;
                }
                else
                    st.path.lineTo (at[i]);
                st.x1 = at[i].x;
            }
            if (started)
                stretches.push_back (std::move (st));
        }
        return stretches;
    };
    const auto stroke = [&] (const std::vector<Stretch>& stretches, juce::Colour c, float width, bool filled)
    {
        for (const auto& st : stretches)
        {
            if (filled)
            {
                auto fill = st.path;
                fill.lineTo (st.x1, area.getBottom());
                fill.lineTo (st.x0, area.getBottom());
                fill.closeSubPath();
                g.setColour (c.withAlpha (0.12f));
                g.fillPath (fill);
            }
            g.setColour (c);
            g.strokePath (st.path, juce::PathStrokeType (width));
        }
    };

    if (trimming)
    {
        // (0.28.0, Trim) The automation as it was, light; what plays (with the trim), darker; the trim curve
        // round the middle (0 dB), its points the ones edited.
        stroke (curveOf (lane->points, false, virgin), colour.withAlpha (0.3f), 1.2f, false);
        pr7s::AutomationLane result = *lane;
        result.trim = points;
        juce::Path played;
        auto started = false;
        // (Every 3 px from 0, as before, but from the last value before the strip to the first after it.)
        auto from = juce::jmax (0.0f, std::floor (left / 3.0f) * 3.0f);
        for (auto x = from - 3.0f; x >= 0.0f; x -= 3.0f)
            if (result.playedAt (timeline.beatForX (x), virgin))
            {
                from = x;
                break;
            }
        for (auto x = from; x <= area.getRight(); x += 3.0f)
            if (const auto v = result.playedAt (timeline.beatForX (x), virgin))
            {
                if (! started)
                    played.startNewSubPath (x, yForValue (area, *v)), started = true;
                else
                    played.lineTo (x, yForValue (area, *v));
                if (x > right)
                    break;
            }
        g.setColour (colour);
        g.strokePath (played, juce::PathStrokeType (2.0f));
        if (points.empty())
        {
            dotted (yForValue (area, 0.5f), juce::Colour (0xfff4eeea).withAlpha (0.8f));
            return;
        }
        stroke (curveOf (points, false, false), juce::Colour (0xfff4eeea).withAlpha (0.8f), 1.2f, false);
    }
    else
    {
        if (points.empty())
        {
            // The static value line: the control's setting, dotted.
            dotted (yForValue (area, studio.getControlValue (row.busId, *lane)), ParagonColours::textDim);
            return;
        }
        if (virgin)
            dotted (yForValue (area, studio.getControlValue (row.busId, *lane)), ParagonColours::textDim.withAlpha (0.5f));   // (where it's virgin: the setting)
        stroke (curveOf (points, lane->isStep(), virgin), colour, 1.6f, true);
    }

    // The points: selected ones dark (as the manual has them).
    const auto selectedHere = pointSelection.busId == row.busId && pointSelection.laneIndex == row.laneIndex;
    for (int i = 0; i < (int) points.size(); ++i)
    {
        const auto at = juce::Point<float> (timeline.xForBeat (points[(size_t) i].beat), yForValue (area, points[(size_t) i].value));
        if (at.x < -pointSize || at.x > (float) getWidth() + pointSize || at.x < left || at.x > right)
            continue;
        const auto box = juce::Rectangle<float> (pointSize, pointSize).withCentre (at);
        const auto selected = selectedHere && std::binary_search (pointSelection.points.begin(), pointSelection.points.end(), i);
        if (selected)
        {
            // (0.80.0, the owner: as the other DAW) Lit: pearl white, a size bigger, a dark edge.
            // (0.80.2, the owner asked) A circle, not a square: 2 px bigger each side, as a circle
            // looks smaller than the square it fits in.
            const auto lit = box.expanded (2.0f);
            g.setColour (juce::Colour (0xfff4eeea));
            g.fillEllipse (lit);
            g.setColour (juce::Colour (0xff050405));
            g.drawEllipse (lit, 1.0f);
            continue;
        }
        g.setColour (colour);
        g.fillRect (box);
        g.setColour (juce::Colour (0xff050405));
        g.drawRect (box, 1.0f);
    }
    if (drag == Drag::band && selectedHere)
    {
        g.setColour (ParagonColours::accent.withAlpha (0.15f));
        g.fillRect (pointBand);
        g.setColour (ParagonColours::accent);
        g.drawRect (pointBand, 1.0f);
    }
    paintPointHandles (g, row);
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
    originalPoints = workingPoints = editPointsOf (*lane);
    pointDownAt = event.position;
    dragMoved = false;
    copyingPoints = false;
    ctrlToggle = false;

    // (0.27.0) The tools: DRAW draws, ERASE erases, LINE draws a shape.
    if (tool == Tool::draw)
    {
        drag = Drag::draw;
        pointSelection.points.clear();
        automationDrag (event);
        return true;
    }
    if (tool == Tool::erase)
    {
        drag = Drag::erasePoints;
        pointSelection.points.clear();
        automationDrag (event);
        return true;
    }
    if (tool == Tool::line)
    {
        drag = Drag::linePoints;
        lineEnd = event.position;
        pointSelection.points.clear();
        repaint();
        return true;
    }
    // The event editor's handles (two or more selected), a bend's handle, the quick scaling handle
    // (a point under the mouse comes first: the box's edges run through the outer points).
    auto onPoint = false;
    for (int i = 0; i < (int) editPointsOf (*lane).size() && ! onPoint; ++i)
        onPoint = getPointPosition (row.busId, row.laneIndex, i).getDistanceFrom (event.position) <= pointGrab;
    if (sameLane && ! onPoint)
        if (const auto h = pointHandleAt (event.position, event.mods.isAltDown()); h != PointHandle::none)
        {
            drag = Drag::pointHandle;
            activeHandle = h;
            boxAtDown = getPointBox();
            selectionAtDown = pointSelection.points;
            return true;
        }
    if (const auto c = onPoint ? -1 : curveHandleAt (row.busId, row.laneIndex, event.position); c >= 0)
    {
        drag = event.position.y <= area.getY() + 6.0f ? Drag::segmentScale : Drag::curve;
        curvePoint = c;
        return true;
    }

    // On a point: select it (Shift adds or removes) and drag it (with the others selected).
    anchor = -1;
    for (int i = (int) editPointsOf (*lane).size() - 1; i >= 0; --i)
        if (getPointPosition (row.busId, row.laneIndex, i).getDistanceFrom (event.position) <= pointGrab)
        {
            anchor = i;
            break;
        }
    auto& chosen = pointSelection.points;
    if (anchor >= 0)
    {
        const auto already = std::binary_search (chosen.begin(), chosen.end(), anchor);
        // (0.27.0) Ctrl-click adds or removes too (as Shift), unless it's dragged (Ctrl keeps a move to one direction).
        ctrlToggle = event.mods.isCommandDown() && ! event.mods.isShiftDown();
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
        if (! already && ! ctrlToggle)
            chosen = { anchor };
        if (! already && ctrlToggle && chosen.empty())
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
    if (! event.mods.isShiftDown() && ! event.mods.isCommandDown())
        chosen.clear();
    pointBand = { event.position, event.position };
    bandBefore = chosen;
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
            auto inside = bandBefore;   // (Shift / Ctrl: added to what was selected)
            for (int i = 0; i < (int) editPointsOf (*lane).size(); ++i)
                if (pointBand.contains (getPointPosition (pointSelection.busId, pointSelection.laneIndex, i)))
                    inside.push_back (i);
            std::sort (inside.begin(), inside.end());
            inside.erase (std::unique (inside.begin(), inside.end()), inside.end());
            pointSelection.points = inside;
        }
    }
    else if (drag == Drag::points)
    {
        {
            if (! dragMoved || anchor < 0)
                return;
            if (! std::binary_search (pointSelection.points.begin(), pointSelection.points.end(), anchor))
            {
                // (Ctrl-dragging a point not selected: it joins the selection and moves with it.)
                pointSelection.points.insert (std::upper_bound (pointSelection.points.begin(), pointSelection.points.end(), anchor), anchor);
            }
            // The grabbed point to the mouse (snapped along the song), the others with it.
            const auto& grabbed = originalPoints[(size_t) anchor];
            const auto ctrl = event.mods.isCommandDown();
            const auto noSnap = ctrl && (event.mods.isShiftDown() || event.mods.isAltDown());
            const auto offset = event.position - pointDownAt;
            const auto alongOnly = ctrl && std::abs (offset.x) >= std::abs (offset.y);
            const auto upDownOnly = ctrl && ! alongOnly;
            auto beat = grabbed.beat + offset.x / timeline.pixelsPerBeat;
            if (! noSnap)
                beat = pr7s::snap (beat, timeline.snapGrid (studio.getProject()));
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
    else if (drag == Drag::erasePoints)
    {
        // (0.27.0) ERASE: the points the mouse has crossed since it went down.
        const auto left = juce::jmin (pointDownAt.x, event.position.x) - pointGrab, right = juce::jmax (pointDownAt.x, event.position.x) + pointGrab;
        workingPoints.clear();
        for (const auto& p : originalPoints)
        {
            const auto x = timeline.xForBeat (p.beat);
            if (x < left || x > right)
                workingPoints.push_back (p);
        }
        dragMoved = true;
    }
    else if (drag == Drag::linePoints)
    {
        lineEnd = event.position;
    }
    else if (drag == Drag::pointHandle)
    {
        dragPointHandle (activeHandle, event.position - pointDownAt);
    }
    else if (drag == Drag::curve)
    {
        dragMoved = true;
        bendSegment (pointSelection.busId, pointSelection.laneIndex, curvePoint, event.position.y);
    }
    else if (drag == Drag::segmentScale)
    {
        // Quick vertical scaling: the segment's two points, scaled from the bottom by the drag (up: higher).
        dragMoved = true;
        workingPoints = originalPoints;
        const auto factor = juce::jmax (0.0f, 1.0f - (event.position.y - pointDownAt.y) / juce::jmax (1.0f, area.getHeight()));
        for (const auto i : { curvePoint - 1, curvePoint })
            if (juce::isPositiveAndBelow (i, (int) workingPoints.size()))
                workingPoints[(size_t) i].value = juce::jlimit (0.0f, 1.0f, originalPoints[(size_t) i].value * factor);
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
    if (finished == Drag::points && ! dragMoved && ctrlToggle && anchor >= 0)
    {
        // (0.27.0) A Ctrl-click (no drag): the point in or out of the selection.
        auto& chosen = pointSelection.points;
        if (const auto it = std::find (chosen.begin(), chosen.end(), anchor); it != chosen.end() && chosen.size() > 1)
            chosen.erase (it);
        else if (it == chosen.end())
            chosen.insert (std::upper_bound (chosen.begin(), chosen.end(), anchor), anchor);
        repaint();
        return;
    }
    if (finished == Drag::erasePoints)
    {
        if (workingPoints.size() != originalPoints.size())
            commitPoints (workingPoints, {});
        repaint();
        return;
    }
    if (finished == Drag::linePoints)
    {
        drawLineShape (pointSelection.busId, pointSelection.laneIndex, pointDownAt, lineEnd, juce::ModifierKeys::currentModifiers.isCommandDown());
        return;
    }
    if (finished == Drag::pointHandle || finished == Drag::curve || finished == Drag::segmentScale)
    {
        if (dragMoved)
        {
            std::vector<pr7s::AutomationPoint> stillSelected;
            for (const auto i : pointSelection.points)
                if (juce::isPositiveAndBelow (i, (int) workingPoints.size()))
                    stillSelected.push_back (workingPoints[(size_t) i]);
            commitPoints (workingPoints, stillSelected);
        }
        activeHandle = PointHandle::none;
        selectionAtDown.clear();
        curvePoint = -1;
        repaint();
        return;
    }
    if (finished == Drag::band && ! dragMoved && ! pointDownAt.isOrigin())
    {
        // A click: a point there (snapped along the song) on the line as it is: the curve's value
        // there, or with no points yet the control's own (the owner's report: points took the
        // mouse's height, so four clicks along a flat line gave four values; in the other DAW
        // they all keep the line's). Moved up or down afterwards, or drawn with DRAW at a height.
        // (0.46.1, the owner asked) Between two points too: it's added on the line there, to drag
        // up or down (the manual drops a point that doesn't leave the line; the owner wants it).
        auto points = editPointsOf (*lane);
        const auto beat = juce::jmax (0.0, pr7s::snap (timeline.beatForX (pointDownAt.x), timeline.snapGrid (studio.getProject())));
        auto value = isTrimming (*lane) ? 0.5f : studio.getControlValue (pointSelection.busId, *lane);
        if (! points.empty())
        {
            pr7s::AutomationLane line;
            line.points = points;
            if (const auto onLine = line.valueAt (beat))
                value = *onLine;
        }
        const pr7s::AutomationPoint p { beat, juce::jlimit (0.0f, 1.0f, value) };
        if (std::any_of (points.begin(), points.end(), [&p] (const pr7s::AutomationPoint& q) { return std::abs (q.beat - p.beat) < 1.0e-9; }))
            return;   // (one there already)
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
    if (const auto* lane = laneFor (busId, laneIndex); lane != nullptr && isTrimming (*lane))
        studio.setTrimPoints (busId, laneIndex, std::move (points));      // (0.28.0: Trim on: its trim curve)
    else
        studio.setAutomationPoints (busId, laneIndex, std::move (points));   // one undo step; turns R on
    std::vector<int> chosen;
    if (const auto* lane = laneFor (busId, laneIndex))
        for (int i = 0; i < (int) editPointsOf (*lane).size(); ++i)
            if (std::find (selected.begin(), selected.end(), editPointsOf (*lane)[(size_t) i]) != selected.end())
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
    const auto virgin = studio.getAutomationSettings().virginTerritory && ! isTrimming (*lane);
    const auto& points = editPointsOf (*lane);
    for (int i = 0; i < (int) points.size(); ++i)
    {
        const auto deleted = std::binary_search (pointSelection.points.begin(), pointSelection.points.end(), i);
        if (! deleted)
            kept.push_back (points[(size_t) i]);
        else if (virgin && ! kept.empty() && i + 1 < (int) points.size())
            kept.back().terminator = true;   // (0.28.0, virgin territory: deleting makes a gap)
    }
    // (0.46.1, the owner asked) The next point after what went is selected, so Delete again takes
    // it; at the end of the lane, the one before.
    std::vector<pr7s::AutomationPoint> next;
    const auto last = pointSelection.points.back();
    if (last + 1 < (int) points.size())
    {
        const auto after = (size_t) (last + 1 - (int) pointSelection.points.size());
        if (after < kept.size())
            next.push_back (kept[after]);
    }
    else if (! kept.empty())
        next.push_back (kept.back());
    commitPoints (std::move (kept), std::move (next));
}

void ArrangeView::selectAllPoints (int busId, int laneIndex)
{
    std::vector<int> all;
    if (const auto* lane = laneFor (busId, laneIndex))
        for (int i = 0; i < (int) editPointsOf (*lane).size(); ++i)
            all.push_back (i);
    pointSelection = { busId, laneIndex, all };
    repaint();
}

bool ArrangeView::cancelDrag()
{
    if (drag != Drag::points && drag != Drag::draw && drag != Drag::band && drag != Drag::erasePoints && drag != Drag::linePoints
        && drag != Drag::pointHandle && drag != Drag::curve && drag != Drag::segmentScale)
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
    const auto selectedHere = pointSelection.busId == busId && pointSelection.laneIndex == laneIndex && hasSelectedPoints();
    menu.addItem (1, "Select All Events");
    menu.addItem (2, "Delete", selectedHere);
    menu.addItem (4, "Type a Value...", selectedHere);
    menu.addItem (5, "Straighten the Selected Ramps", selectedHere);
    // (0.28.0) A terminator: the curve stops at the selected point, a gap until the next (virgin territory).
    const auto* thisLane = laneFor (busId, laneIndex);
    auto terminatorOn = false;
    if (thisLane != nullptr && selectedHere)
        for (const auto i : pointSelection.points)
            if (juce::isPositiveAndBelow (i, (int) thisLane->points.size()))
                terminatorOn = terminatorOn || thisLane->points[(size_t) i].terminator;
    menu.addItem (8, "Terminator (a Gap After It)", selectedHere && studio.getAutomationSettings().virginTerritory && thisLane != nullptr && ! isTrimming (*thisLane), terminatorOn);
    menu.addItem (9, "Freeze Trim", thisLane != nullptr && ! thisLane->trim.empty());
    menu.addSeparator();
    juce::PopupMenu copyMenu, pasteMenu;
    copyMenu.addItem (10, "Copy Automation of All Tracks (Locator Range)");
    copyMenu.addItem (11, "Copy Automation of Selected Tracks (Locator Range)");
    copyMenu.addItem (12, "Copy Automation of This Parameter (Locator Range)");
    const auto canPaste = studio.hasAutomationClip();
    pasteMenu.addItem (20, "Paste Automation of All Tracks (Locator Range)", canPaste);
    pasteMenu.addItem (21, "Paste Automation onto Selected Tracks (Locator Range)", canPaste);
    pasteMenu.addItem (22, "Paste Automation onto This Parameter (Locator Range)", canPaste);
    menu.addSubMenu ("Copy Automation", copyMenu);
    menu.addSubMenu ("Paste Automation", pasteMenu);
    menu.addSeparator();
    menu.addItem (6, "Show All Used Automation");
    menu.addItem (7, "Remove Unused Parameters");
    menu.addItem (3, "Remove Parameter");
    menu.addSeparator();
    menu.addItem (30, "Editing Box for Selected Points (Tilt, Scale, Stretch)", true, pointBoxShown);
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<ArrangeView> (this), busId, laneIndex] (int chosen)
    {
        if (safe == nullptr)
            return;
        using Scope = pr7s::Studio::AutomationScope;
        if (chosen == 1)
            safe->selectAllPoints (busId, laneIndex);
        else if (chosen == 2)
            safe->deleteSelectedPoints();
        else if (chosen == 4 && safe->hasSelectedPoints())
            safe->showPointEntry (busId, laneIndex, safe->pointSelection.points.front());
        else if (chosen == 5)
        {
            if (const auto* lane = safe->laneFor (busId, laneIndex))
            {
                auto points = safe->editPointsOf (*lane);
                std::vector<pr7s::AutomationPoint> kept;
                for (const auto i : safe->pointSelection.points)
                    if (juce::isPositiveAndBelow (i, (int) points.size()))
                    {
                        points[(size_t) i].curve = 0.0f;
                        if (i + 1 < (int) points.size())
                            points[(size_t) i + 1].curve = 0.0f;   // (the ramps either side of it)
                    }
                for (const auto i : safe->pointSelection.points)
                    if (juce::isPositiveAndBelow (i, (int) points.size()))
                        kept.push_back (points[(size_t) i]);
                safe->commitPoints (points, kept);
            }
        }
        else if (chosen == 8)
        {
            if (const auto* lane = safe->laneFor (busId, laneIndex))
            {
                auto points = lane->points;
                auto anyOn = false;
                for (const auto i : safe->pointSelection.points)
                    if (juce::isPositiveAndBelow (i, (int) points.size()))
                        anyOn = anyOn || points[(size_t) i].terminator;
                std::vector<pr7s::AutomationPoint> kept;
                for (const auto i : safe->pointSelection.points)
                    if (juce::isPositiveAndBelow (i, (int) points.size()))
                    {
                        points[(size_t) i].terminator = ! anyOn;
                        kept.push_back (points[(size_t) i]);
                    }
                safe->studio.setAutomationPoints (busId, laneIndex, points);
                safe->pointSelection = { busId, laneIndex, safe->pointSelection.points };
                safe->repaint();
            }
        }
        else if (chosen == 9)
            safe->studio.freezeTrim (busId, laneIndex);
        else if (chosen == 6)
            safe->studio.showAllUsedAutomation();
        else if (chosen == 7)
            safe->studio.removeUnusedAutomation (busId);
        else if (chosen >= 10 && chosen <= 12)
            safe->studio.copyAutomation (chosen == 10 ? Scope::allTracks : chosen == 11 ? Scope::selectedTracks : Scope::parameter, busId, laneIndex);
        else if (chosen >= 20 && chosen <= 22)
            safe->studio.pasteAutomation (chosen == 20 ? Scope::allTracks : chosen == 21 ? Scope::selectedTracks : Scope::parameter, busId, laneIndex);
        else if (chosen == 3)
        {
            safe->pointSelection = {};
            safe->studio.removeAutomationParameter (busId, laneIndex);
        }
        else if (chosen == 30)
            safe->setPointBoxShown (! safe->pointBoxShown);
    });
}

//==============================================================================
ArrangeTools::ArrangeTools (ArrangeView& arrangeToUse) : arrange (arrangeToUse)
{
    static const char* const tips[] = {
        "SELECT (1): select, move (Alt: copy) and resize parts; Alt-click splits a part there; Alt-drag on an empty instrument track draws a part",
        "COMP: on a track's lanes (LANES), click a take to bring it to the front (again: back); drag across a lane to cut every take there and bring that stretch of this lane to the front; Shift-click selects; Alt-click splits",
        "RANGE (2): drag across the tracks to select a stretch of time; then Delete, Ctrl+X / C / V, Shift+X (split at its edges), drag to move what's in it (Alt: copy), right-click for more",
        "SPLIT (3): click a part to cut it there (on the grid; Ctrl: anywhere); Alt-click: into equal pieces that long",
        "GLUE (4): click a part to join it to the next one on its track; Alt-click: to all after it",
        "ERASE (5): click a part to delete it; Alt-click: it and all after it on its track",
        "ZOOM (6): click to zoom in there, Alt-click to zoom out; drag across a stretch to fill the view with it",
        "MUTE (7): click a part to mute or unmute it (muted parts aren't played); drag a rectangle over several",
        "DRAW (8): drag on an instrument track to draw a new, empty part; on an automation lane, draw points",
        "LINE: drag on an automation lane for a line of points; click LINE again for its shapes (Line, Parabola, Sine, Triangle, Square)",
        "WARP (Free Warp): on an audio clip, click where a beat is (it catches the hitpoint nearby; Ctrl: exactly there) and drag it where it belongs: "
        "the audio either side is stretched. Drag a marker to move it; a modifier key and click removes it. Alt-click adds a hitpoint, Shift-click one removes it",
        "PLAY (9): hold the mouse down on the tracks to play from there; let go to stop" };
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        auto& button = buttons[i];
        const auto t = ArrangeView::tools[i];
        button.setButtonText (ArrangeView::toolName (t));
        button.tool = t;
        button.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        button.setWantsKeyboardFocus (false);
        button.setTooltip (tips[i]);
        button.onClick = [this, t, &button]
        {
            if (t == ArrangeView::Tool::line && arrange.getTool() == t)
                arrange.showLineShapeMenu (&button);   // (LINE again: its shapes, as the manual's)
            else
                arrange.setTool (t);
        };
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

void ToolKey::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    getLookAndFeel().drawButtonBackground (g, *this, findColour (buttonColourId), highlighted, down);
    // Lettering as the other keys': lit orange, else light (the keys are black in every finish since 0.48.0).
    const auto on = getToggleState();
    const auto colour = on ? ParagonColours::accent : ParagonColours::text;
    auto area = getLocalBounds().toFloat().reduced (5.0f, 3.0f).translated (0.0f, down ? 0.5f : -0.5f);
    if (iconOnly)
    {
        ToolIcons::draw (g, tool, area.reduced (3.0f, 4.0f), colour);
        return;
    }
    ToolIcons::draw (g, tool, area.removeFromLeft (area.getHeight()).reduced (3.0f), colour);
    area.removeFromLeft (2.0f);
    g.setColour (colour);
    g.setFont (ParagonLookAndFeel::titleFont (juce::jmin (12.0f, area.getHeight() * 0.7f)));
    g.drawFittedText (getButtonText(), area.toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

void ArrangeTools::setOnUnit (bool shouldBeOnUnit)
{
    if (onUnit == shouldBeOnUnit)
        return;
    onUnit = shouldBeOnUnit;
    for (auto& button : buttons)
        button.iconOnly = onUnit;
    resized();
    repaint();
}

void ArrangeTools::paint (juce::Graphics& g)
{
    if (onUnit)
        return;   // (on the unit: no label; the icons say what they are)
    const auto label = getLocalBounds().withWidth (60).withTrimmedLeft (14).toFloat();
    g.setFont (ParagonLookAndFeel::textFont (11.0f));
    g.setColour (ParagonColours::textDim);
    g.drawText ("TOOLS", label, juce::Justification::centredLeft, false);
}

void ArrangeTools::resized()
{
    auto area = getLocalBounds().withTrimmedLeft (onUnit ? 0 : 64);
    const auto count = (int) buttons.size();
    const auto keyWidth = juce::jmin (onUnit ? 60 : 70, (area.getWidth() - 4 * (count - 1)) / count);
    for (auto& button : buttons)
    {
        button.setBounds (area.removeFromLeft (keyWidth));
        area.removeFromLeft (4);
    }
}

float ArrangeView::splitLineX() const
{
    // Where SPLIT's cut line is drawn for the mouse at `hover`: on the grid, inside the part (else none: -1).
    if (hover.x < 0.0f)
        return -1.0f;
    const auto hit = partAt (hover);
    if (! hit)
        return -1.0f;
    const auto bounds = getPartBounds (hit->first, hit->second);
    const auto x = timeline.xForBeat (pr7s::snap (timeline.beatForX (hover.x), timeline.snapGrid (studio.getProject())));
    return x > bounds.getX() && x < bounds.getRight() ? x : -1.0f;
}

//==============================================================================
// (Among the tracks shown: a range never takes in a hidden one.)
int ArrangeView::trackIndex (int trackId) const
{
    const auto shown = studio.getShownTracks();
    for (int i = 0; i < (int) shown.size(); ++i)
        if (shown[(size_t) i] == trackId)
            return i;
    return -1;
}

std::vector<int> ArrangeView::tracksBetween (int fromIndex, int toIndex) const
{
    const auto shown = studio.getShownTracks();
    std::vector<int> ids;
    for (int i = juce::jmin (fromIndex, toIndex); i <= juce::jmax (fromIndex, toIndex); ++i)
        if (juce::isPositiveAndBelow (i, (int) shown.size()))
            ids.push_back (shown[(size_t) i]);
    return ids;
}

double ArrangeView::rangeBeat (const juce::MouseEvent& event) const
{
    const auto beat = juce::jmax (0.0, timeline.beatForX (event.position.x));
    return event.mods.isCommandDown() ? beat : pr7s::snap (beat, timeline.snapGrid (studio.getProject()));
}

void ArrangeView::setRange (std::optional<pr7s::Studio::Range> newRange)
{
    if (newRange && newRange->isEmpty())
        newRange.reset();
    range = std::move (newRange);
    repaint();
}

bool ArrangeView::rangeDown (const juce::MouseEvent& event)
{
    const auto trackId = trackAtY (event.position.y);
    if (event.mods.isPopupMenu())
    {
        const auto beat = timeline.beatForX (event.position.x);
        if (range && beat >= range->start && beat <= range->end
            && std::find (range->trackIds.begin(), range->trackIds.end(), trackId) != range->trackIds.end())
        {
            showRangeMenu();
            return true;
        }
        return false;   // the part's menu
    }
    if (trackId == 0)
    {
        setRange (std::nullopt);   // (the master's row)
        return true;
    }
    const auto beat = rangeBeat (event);
    const auto index = trackIndex (trackId);
    rangeAnchor = beat;
    rangeAnchorTrack = index;
    dragMoved = false;
    if (range)
    {
        rangeBefore = *range;
        const auto x = event.position.x;
        const auto onTrack = std::find (range->trackIds.begin(), range->trackIds.end(), trackId) != range->trackIds.end();
        const auto startX = timeline.xForBeat (range->start), endX = timeline.xForBeat (range->end);
        if (onTrack && std::abs (x - startX) <= 5.0f)   { drag = Drag::rangeStart; return true; }
        if (onTrack && std::abs (x - endX) <= 5.0f)     { drag = Drag::rangeEnd; return true; }
        const auto within = onTrack && x > startX && x < endX;
        if (within && ! event.mods.isShiftDown())
        {
            // Its contents, dragged (Alt: copied).
            drag = Drag::rangeMove;
            rangeAnchor = timeline.beatForX (event.position.x);
            rangeMoveStart = range->start;
            rangeMoveTrack = trackIndex (range->trackIds.front());
            return true;
        }
        if (! within && event.mods.isShiftDown())
        {
            // Shift outside it: it grows (or shrinks) to here, tracks and all.
            const auto top = juce::jmin (index, trackIndex (range->trackIds.front()));
            const auto bottom = juce::jmax (index, trackIndex (range->trackIds.back()));
            range->trackIds = tracksBetween (top, bottom);
            const auto toStart = std::abs (beat - range->start) < std::abs (beat - range->end);
            (toStart ? range->start : range->end) = beat;
            if (range->end < range->start)
                std::swap (range->start, range->end);
            rangeBefore = *range;
            drag = toStart ? Drag::rangeStart : Drag::rangeEnd;
            repaint();
            return true;
        }
    }
    // A new one from here (a click alone clears it).
    range = pr7s::Studio::Range { beat, beat, { trackId } };
    rangeBefore = *range;
    drag = Drag::rangeNew;
    studio.selectTrack (trackId);
    repaint();
    return true;
}

void ArrangeView::rangeDrag (const juce::MouseEvent& event)
{
    if (event.getDistanceFromDragStart() > 3)
        dragMoved = true;
    if (! range)
        return;
    const auto beat = rangeBeat (event);
    const auto overTrack = trackAtY (event.position.y);
    const auto overIndex = overTrack != 0 ? trackIndex (overTrack) : -1;
    switch (drag)
    {
        case Drag::rangeNew:
            range->start = juce::jmin (rangeAnchor, beat);
            range->end = juce::jmax (rangeAnchor, beat);
            if (overIndex >= 0)
                range->trackIds = tracksBetween (rangeAnchorTrack, overIndex);
            break;
        case Drag::rangeStart:
        case Drag::rangeEnd:
        {
            // An edge (past the other one, they swap).
            const auto other = drag == Drag::rangeStart ? rangeBefore.end : rangeBefore.start;
            range->start = juce::jmin (other, beat);
            range->end = juce::jmax (other, beat);
            break;
        }
        case Drag::rangeMove:
        {
            const auto grid = event.mods.isCommandDown() ? 0.0 : timeline.snapGrid (studio.getProject());
            rangeMoveStart = juce::jmax (0.0, pr7s::snap (rangeBefore.start + timeline.beatForX (event.position.x) - rangeAnchor, grid));
            const auto count = (int) studio.getShownTracks().size();
            const auto span = (int) rangeBefore.trackIds.size();
            const auto first = trackIndex (rangeBefore.trackIds.front());
            rangeMoveTrack = overIndex >= 0 ? juce::jlimit (0, juce::jmax (0, count - span), first + overIndex - rangeAnchorTrack) : first;
            break;
        }
        default:
            break;
    }
    repaint();
}

void ArrangeView::rangeUp (const juce::MouseEvent& event)
{
    const auto finished = drag;
    drag = Drag::none;
    if (! range)
        return;
    if (finished == Drag::rangeNew && (! dragMoved || range->end - range->start < 1.0e-9))
    {
        setRange (std::nullopt);   // a click: no range
        return;
    }
    if (finished == Drag::rangeMove)
    {
        const auto first = trackIndex (rangeBefore.trackIds.front());
        if (dragMoved && (std::abs (rangeMoveStart - rangeBefore.start) > 1.0e-9 || rangeMoveTrack != first))
        {
            const auto tracks = studio.getShownTracks();
            const auto newFirst = tracks[(size_t) juce::jlimit (0, (int) tracks.size() - 1, rangeMoveTrack)];
            studio.moveRange (rangeBefore, rangeMoveStart, newFirst, event.mods.isAltDown());
            // The range goes with what it held.
            range = pr7s::Studio::Range { rangeMoveStart, rangeMoveStart + rangeBefore.length(),
                                          tracksBetween (rangeMoveTrack, rangeMoveTrack + (int) rangeBefore.trackIds.size() - 1) };
        }
    }
    setRange (range);
}

bool ArrangeView::rangeDoubleClick (const juce::MouseEvent& event)
{
    const auto trackId = trackAtY (event.position.y);
    const auto* track = studio.getProject().findTrack (trackId);
    if (track == nullptr)
        return false;
    // On a part: a range over it; between parts: over the gap.
    if (const auto hit = partAt (event.position))
    {
        const auto& part = track->parts[(size_t) hit->second];
        setRange (pr7s::Studio::Range { part.start, part.end(), { trackId } });
        return true;
    }
    const auto at = timeline.beatForX (event.position.x);
    auto before = 0.0, after = -1.0;
    for (const auto& part : track->parts)
    {
        if (part.end() <= at)
            before = juce::jmax (before, part.end());
        if (part.start >= at && (after < 0.0 || part.start < after))
            after = part.start;
    }
    if (after > before)
        setRange (pr7s::Studio::Range { before, after, { trackId } });
    return true;
}

void ArrangeView::rangeCommand (RangeCommand command)
{
    using C = RangeCommand;
    if (command == C::paste || command == C::pasteTime)
    {
        // At the playhead, on the selected track down (or the range's first track).
        if (rangeClip.isEmpty())
            return;
        auto first = studio.getSelectedTrack();
        if (studio.getProject().findTrack (first) == nullptr)
            first = range ? range->trackIds.front() : (studio.getShownTracks().empty() ? 0 : studio.getShownTracks().front());
        studio.pasteRange (rangeClip, studio.getEngine().getPosition(), first, command == C::pasteTime);
        return;
    }
    if (! range)
        return;
    const auto r = *range;
    switch (command)
    {
        case C::split:          studio.splitRange (r); break;
        case C::crop:           studio.cropRange (r); break;
        case C::deleteContents: studio.deleteRange (r, false); break;
        case C::deleteTime:     studio.deleteRange (r, true); break;
        case C::cut:            rangeClip = studio.cutRange (r, false); break;
        case C::cutTime:        rangeClip = studio.cutRange (r, true); break;
        case C::copy:           rangeClip = studio.copyRange (r); break;
        case C::insertSilence:  studio.insertSilence (r); break;
        case C::duplicate:      studio.duplicateRange (r); break;
        case C::paste:
        case C::pasteTime:      break;
    }
    repaint();
}

void ArrangeView::showRangeMenu()
{
    using C = RangeCommand;
    juce::PopupMenu menu;
    menu.addSectionHeader ("RANGE");
    menu.addItem (1, "Split at its edges (Shift+X)");
    menu.addItem (2, "Crop to it");
    menu.addSeparator();
    menu.addItem (3, "Cut (Ctrl+X)");
    menu.addItem (4, "Copy (Ctrl+C)");
    menu.addItem (5, "Paste at the playhead (Ctrl+V)", hasRangeClip());
    menu.addItem (6, "Delete (Delete)");
    menu.addItem (7, "Duplicate (Ctrl+D)");
    menu.addSeparator();
    menu.addItem (8, "Cut Time (Ctrl+Shift+X)");
    menu.addItem (9, "Delete Time (Shift+Backspace)");
    menu.addItem (10, "Paste Time (Ctrl+Shift+V)", hasRangeClip());
    menu.addItem (11, "Insert Silence (Ctrl+Shift+E)");
    menu.addSeparator();
    menu.addItem (20, "Render in Place...");
    menu.addItem (21, "Render in Place (as last time)");
    menu.addSeparator();
    menu.addItem (22, "Audio Alignment...");   // (0.33.0)
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<ArrangeView> (this)] (int chosen)
    {
        if (safe != nullptr && chosen == 22)
        {
            if (safe->onOpenAudioAlignment)
                safe->onOpenAudioAlignment();
            return;
        }
        if (safe != nullptr && (chosen == 20 || chosen == 21) && safe->range && safe->onRenderInPlace)
        {
            std::vector<pr7s::Studio::RenderSource> sources;
            for (const auto id : safe->range->trackIds)
                sources.push_back ({ id, safe->range->start, safe->range->end, false });
            safe->onRenderInPlace (sources, chosen == 20);
            return;
        }
        static const C commands[] = { C::split, C::crop, C::cut, C::copy, C::paste, C::deleteContents, C::duplicate,
                                      C::cutTime, C::deleteTime, C::pasteTime, C::insertSilence };
        if (safe != nullptr && chosen >= 1 && chosen <= (int) std::size (commands))
            safe->rangeCommand (commands[chosen - 1]);
    });
}

void ArrangeView::paintRange (juce::Graphics& g) const
{
    if (! range)
        return;
    const auto paintSpan = [&] (double start, double end, const std::vector<int>& ids, bool ghost)
    {
        const auto x1 = timeline.xForBeat (start), x2 = timeline.xForBeat (end);
        for (const auto id : ids)
        {
            const auto top = trackTop (id);
            const juce::Rectangle<float> area (x1, top, x2 - x1, (float) timeline.trackRowHeight());
            g.setColour (juce::Colours::white.withAlpha (ghost ? 0.08f : 0.16f));
            g.fillRect (area);
            g.setColour (ParagonColours::accent.withAlpha (ghost ? 0.5f : 0.9f));
            g.fillRect (area.withWidth (1.5f));
            g.fillRect (area.withX (area.getRight() - 1.5f).withWidth (1.5f));
        }
    };
    paintSpan (range->start, range->end, range->trackIds, false);
    if (drag == Drag::rangeMove && dragMoved)
        paintSpan (rangeMoveStart, rangeMoveStart + rangeBefore.length(),
                   tracksBetween (rangeMoveTrack, rangeMoveTrack + (int) rangeBefore.trackIds.size() - 1), true);
}

bool ArrangeView::locatorsToSelection()
{
    if (range)
    {
        studio.setCycle (range->start, range->end);
        return true;
    }
    if (const auto* track = studio.getProject().findTrack (selection.trackId); track != nullptr && juce::isPositiveAndBelow (selection.index, (int) track->parts.size()))
    {
        const auto& part = track->parts[(size_t) selection.index];
        studio.setCycle (part.start, part.end());
        return true;
    }
    return false;
}

//==============================================================================
// The tempo track (0.10.0, the manual's): its curve between the shown limits,
// its events as points (selected ones dark); a click on the curve adds an
// event (snapped), Draw draws them, Erase removes them, a selected one is
// dragged along the song and up and down (the first stays at the start).
juce::Rectangle<float> ArrangeView::getTempoArea() const
{
    std::vector<Timeline::Row> made;
    for (const auto& row : rowsNow (made))
        if (row.kind == Timeline::Row::Kind::tempo)
            return juce::Rectangle<float> (0.0f, rowTop (row), (float) getWidth(), (float) row.height).reduced (0.0f, laneInset);
    return {};
}

float ArrangeView::yForBpm (const juce::Rectangle<float>& area, double bpm) const
{
    const auto& project = studio.getProject();
    const auto proportion = (bpm - project.tempoViewLow) / juce::jmax (1.0, project.tempoViewHigh - project.tempoViewLow);
    return area.getBottom() - (float) juce::jlimit (0.0, 1.0, proportion) * area.getHeight();
}

double ArrangeView::bpmForY (const juce::Rectangle<float>& area, float y) const
{
    const auto& project = studio.getProject();
    const auto proportion = juce::jlimit (0.0, 1.0, (double) (area.getBottom() - y) / juce::jmax (1.0f, area.getHeight()));
    // Whole BPM while dragging (finer: type it in, double-click).
    return std::round (project.tempoViewLow + proportion * (project.tempoViewHigh - project.tempoViewLow));
}

juce::Point<float> ArrangeView::getTempoEventPosition (int index) const
{
    const auto editing = drag == Drag::tempoPoints || drag == Drag::tempoDraw;
    const auto& events = editing ? tempoWorking : studio.getProject().tempoEvents;
    if (! juce::isPositiveAndBelow (index, (int) events.size()))
        return {};
    const auto area = getTempoArea();
    return { timeline.xForBeat (events[(size_t) index].beat), yForBpm (area, events[(size_t) index].bpm) };
}

void ArrangeView::paintTempo (juce::Graphics& g, const Timeline::Row& row) const
{
    const auto& project = studio.getProject();
    const auto area = juce::Rectangle<float> (0.0f, rowTop (row), (float) getWidth(), (float) row.height).reduced (0.0f, laneInset);
    const auto on = project.tempoTrackOn;
    const auto colour = on ? pr7s::FamilyColours::orange : ParagonColours::textDim.withAlpha (0.6f);   // off: greyed out (the manual's)
    const auto editing = drag == Drag::tempoPoints || drag == Drag::tempoDraw;
    const auto& events = editing ? tempoWorking : project.tempoEvents;

    // The scale's ends, faintly at the right (the first event's tempo is at the left).
    g.setColour (ParagonColours::textDim.withAlpha (0.5f));
    g.setFont (ParagonLookAndFeel::textFont (9.0f));
    g.drawText (juce::String (juce::roundToInt (project.tempoViewHigh)) + " BPM", area.withHeight (11.0f).withTrimmedRight (8.0f), juce::Justification::topRight, false);
    g.drawText (juce::String (juce::roundToInt (project.tempoViewLow)) + " BPM", area.withTop (area.getBottom() - 11.0f).withTrimmedRight (8.0f), juce::Justification::bottomRight, false);

    // Off: the fixed tempo as a level line (the manual's), the events greyed out behind it.
    if (! on)
    {
        const auto y = yForBpm (area, project.bpm);
        g.setColour (ParagonColours::text.withAlpha (0.7f));
        const auto redrawn = g.getClipBounds().toFloat();
        for (auto x = juce::jmax (0.0f, std::floor ((redrawn.getX() - curveMargin) / 6.0f) * 6.0f); x < area.getRight() && x <= redrawn.getRight() + curveMargin; x += 6.0f)
            g.fillRect (x, y, 3.0f, 1.0f);
    }
    if (events.empty())
        return;

    // The curve: level after a step's jump, straight along a ramp (the tempo changes evenly beat by beat), level after the last.
    juce::Path curve;
    curve.startNewSubPath (0.0f, yForBpm (area, events.front().bpm));
    for (size_t i = 1; i < events.size(); ++i)
    {
        const auto x = timeline.xForBeat (events[i].beat);
        if (! events[i].ramp)
            curve.lineTo (x, yForBpm (area, events[i - 1].bpm));
        curve.lineTo (x, yForBpm (area, events[i].bpm));
    }
    curve.lineTo (area.getRight(), yForBpm (area, events.back().bpm));
    auto fill = curve;
    fill.lineTo (area.getRight(), area.getBottom());
    fill.lineTo (0.0f, area.getBottom());
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.10f));
    g.fillPath (fill);
    g.setColour (colour);
    g.strokePath (curve, juce::PathStrokeType (1.6f));

    // The events, their tempo beside them.
    g.setFont (ParagonLookAndFeel::textFont (9.5f));
    for (int i = 0; i < (int) events.size(); ++i)
    {
        const juce::Point<float> at (timeline.xForBeat (events[(size_t) i].beat), yForBpm (area, events[(size_t) i].bpm));
        if (at.x < -60.0f || at.x > (float) getWidth() + pointSize)
            continue;
        const auto box = juce::Rectangle<float> (pointSize, pointSize).withCentre (at);
        const auto selected = std::find (tempoSelection.begin(), tempoSelection.end(), i) != tempoSelection.end();
        g.setColour (selected ? juce::Colour (0xff050405) : colour);
        g.fillRect (box);
        g.setColour (selected ? colour : juce::Colour (0xff050405));
        g.drawRect (box, 1.0f);
        g.setColour (on ? ParagonColours::text : ParagonColours::textDim);
        const auto text = juce::String (events[(size_t) i].bpm, std::abs (events[(size_t) i].bpm - std::round (events[(size_t) i].bpm)) < 0.005 ? 0 : 2);
        const auto above = at.y - area.getY() > 14.0f;
        g.drawText (text, juce::Rectangle<float> (at.x + 5.0f, above ? at.y - 14.0f : at.y + 3.0f, 60.0f, 11.0f), juce::Justification::centredLeft, false);
    }
    if (drag == Drag::tempoBand)
    {
        g.setColour (ParagonColours::accent.withAlpha (0.15f));
        g.fillRect (pointBand);
        g.setColour (ParagonColours::accent);
        g.drawRect (pointBand, 1.0f);
    }
}

bool ArrangeView::tempoDown (const juce::MouseEvent& event)
{
    const auto& project = studio.getProject();
    selection = {};
    alsoSelected.clear();
    pointSelection = {};
    if (event.mods.isPopupMenu())
    {
        drag = Drag::none;
        showTempoMenu();
        return true;
    }
    const auto area = getTempoArea();
    tempoOriginal = tempoWorking = project.tempoEvents;
    pointDownAt = event.position;
    dragMoved = false;
    drawn.clear();

    tempoAnchor = -1;
    for (int i = (int) tempoOriginal.size() - 1; i >= 0; --i)
        if (getTempoEventPosition (i).getDistanceFrom (event.position) <= pointGrab)
        {
            tempoAnchor = i;
            break;
        }

    // Erase: the event clicked goes (not the first).
    if (tool == Tool::erase)
    {
        drag = Drag::none;
        if (tempoAnchor > 0)
            studio.removeTempoEvents ({ tempoAnchor });
        tempoSelection.clear();
        repaint();
        return true;
    }
    // Draw: events where the mouse goes (snapped), replacing what was there.
    if (tool == Tool::draw && ! project.tempoLocked)
    {
        drag = Drag::tempoDraw;
        tempoSelection.clear();
        tempoDrag (event);
        return true;
    }
    if (tempoAnchor >= 0)
    {
        const auto already = std::find (tempoSelection.begin(), tempoSelection.end(), tempoAnchor) != tempoSelection.end();
        if (event.mods.isShiftDown())
        {
            if (already)
                tempoSelection.erase (std::find (tempoSelection.begin(), tempoSelection.end(), tempoAnchor));
            else
                tempoSelection.push_back (tempoAnchor);
            std::sort (tempoSelection.begin(), tempoSelection.end());
            drag = Drag::none;
            repaint();
            return true;
        }
        if (! already)
            tempoSelection = { tempoAnchor };
        drag = project.tempoLocked ? Drag::none : Drag::tempoPoints;
        repaint();
        return true;
    }
    // Elsewhere: a click adds an event there (on release); a drag selects with a rectangle.
    if (! event.mods.isShiftDown())
        tempoSelection.clear();
    pointBand = { event.position, event.position };
    drag = Drag::tempoBand;
    juce::ignoreUnused (area);
    repaint();
    return true;
}

void ArrangeView::tempoDrag (const juce::MouseEvent& event)
{
    const auto& project = studio.getProject();
    const auto area = getTempoArea();
    if (event.getDistanceFromDragStart() > 2)
        dragMoved = true;
    if (drag == Drag::tempoBand)
    {
        pointBand = juce::Rectangle<float> (pointDownAt, event.position);
        tempoSelection.clear();
        for (int i = 0; i < (int) project.tempoEvents.size(); ++i)
            if (pointBand.contains (getTempoEventPosition (i)))
                tempoSelection.push_back (i);
    }
    else if (drag == Drag::tempoPoints)
    {
        if (! dragMoved || tempoAnchor < 0)
            return;
        // The grabbed event to the mouse (snapped along the song), the selected ones with it;
        // Ctrl keeps to one direction. The first event stays at the start.
        const auto& grabbed = tempoOriginal[(size_t) tempoAnchor];
        const auto offset = event.position - pointDownAt;
        const auto ctrl = event.mods.isCommandDown();
        const auto alongOnly = ctrl && std::abs (offset.x) >= std::abs (offset.y);
        const auto upDownOnly = ctrl && ! alongOnly;
        auto beat = pr7s::snap (grabbed.beat + offset.x / timeline.pixelsPerBeat, timeline.snapGrid (project));
        const auto beatShift = upDownOnly ? 0.0 : beat - grabbed.beat;
        const auto bpmShift = alongOnly ? 0.0 : bpmForY (area, yForBpm (area, grabbed.bpm) + offset.y) - grabbed.bpm;
        tempoWorking = tempoOriginal;
        for (const auto i : tempoSelection)
            if (juce::isPositiveAndBelow (i, (int) tempoWorking.size()))
            {
                auto& e = tempoWorking[(size_t) i];
                if (i > 0)
                    e.beat = juce::jmax (1.0e-3, tempoOriginal[(size_t) i].beat + beatShift);
                e.bpm = juce::jlimit (pr7s::Project::minBpm, pr7s::Project::maxBpm, tempoOriginal[(size_t) i].bpm + bpmShift);
            }
    }
    else if (drag == Drag::tempoDraw)
    {
        // An event where the mouse is (snapped); between where the stroke has been, only what it drew.
        const auto beat = juce::jmax (0.0, pr7s::snap (timeline.beatForX (event.position.x), timeline.snapGrid (project)));
        const auto bpm = bpmForY (area, event.position.y);
        const auto startBeat = juce::jmax (0.0, timeline.beatForX (pointDownAt.x));
        const auto from = juce::jmin (startBeat, beat), to = juce::jmax (startBeat, beat);
        std::vector<pr7s::TempoEvent> kept;
        for (const auto& e : tempoWorking)
        {
            const auto wasDrawn = std::any_of (drawn.begin(), drawn.end(), [&e] (const pr7s::AutomationPoint& p) { return std::abs (p.beat - e.beat) < 1.0e-9; });
            if (e.beat <= 0.0 || e.beat < from - 1.0e-9 || e.beat > to + 1.0e-9 || wasDrawn)
                if (! (std::abs (e.beat - beat) < 1.0e-9 && e.beat > 0.0))
                    kept.push_back (e);
        }
        auto ramp = project.newTempoType == pr7s::Project::NewTempoType::ramp;
        if (project.newTempoType == pr7s::Project::NewTempoType::automatic)
            for (const auto& e : kept)
                if (e.beat < beat)
                    ramp = e.ramp;
        if (beat <= 0.0)
            kept.front().bpm = bpm;   // (at the start: the first event's tempo)
        else
        {
            kept.push_back ({ beat, bpm, ramp });
            drawn.push_back ({ beat, 0.0f });
        }
        std::stable_sort (kept.begin(), kept.end(), [] (const auto& a, const auto& b) { return a.beat < b.beat; });
        tempoWorking = std::move (kept);
    }
    repaint();
}

void ArrangeView::tempoUp()
{
    const auto finished = drag;
    drag = Drag::none;
    const auto& project = studio.getProject();
    if (finished == Drag::tempoBand && ! dragMoved)
    {
        // A click on the curve: an event there (snapped), as the manual has the Object Selection tool do.
        if (project.tempoLocked)
            return;
        const auto area = getTempoArea();
        const auto beat = juce::jmax (0.0, pr7s::snap (timeline.beatForX (pointDownAt.x), timeline.snapGrid (project)));
        const auto index = studio.addTempoEvent (beat, bpmForY (area, pointDownAt.y));
        tempoSelection.clear();
        if (index >= 0)
            tempoSelection.push_back (index);
        repaint();
        return;
    }
    if ((finished == Drag::tempoPoints && dragMoved) || finished == Drag::tempoDraw)
    {
        // The moved ones selected again where they went (by beat and tempo).
        std::vector<pr7s::TempoEvent> moved;
        for (const auto i : tempoSelection)
            if (juce::isPositiveAndBelow (i, (int) tempoWorking.size()))
                moved.push_back (tempoWorking[(size_t) i]);
        studio.setTempoEvents (tempoWorking);
        tempoSelection.clear();
        const auto& events = studio.getProject().tempoEvents;
        for (int i = 0; i < (int) events.size(); ++i)
            if (std::find (moved.begin(), moved.end(), events[(size_t) i]) != moved.end())
                tempoSelection.push_back (i);
    }
    repaint();
}

void ArrangeView::deleteSelectedTempoEvents()
{
    studio.removeTempoEvents (tempoSelection);
    tempoSelection.clear();
    repaint();
}

void ArrangeView::selectAllTempoEvents()
{
    tempoSelection.clear();
    for (int i = 0; i < (int) studio.getProject().tempoEvents.size(); ++i)
        tempoSelection.push_back (i);
    repaint();
}

void ArrangeView::editTempoEvent (int index)
{
    const auto& events = studio.getProject().tempoEvents;
    if (! juce::isPositiveAndBelow (index, (int) events.size()) || studio.getProject().tempoLocked)
        return;
    auto* box = new FamilyAlertWindow ("Tempo event", "At " + pr7s::formatBarsBeats (events[(size_t) index].beat, studio.getProject())
                                                          + ": its tempo in BPM, and whether it ramps there from the event before.",
                                       juce::MessageBoxIconType::NoIcon);
    box->addTextEditor ("bpm", juce::String (events[(size_t) index].bpm, 2), "BPM");
    if (index > 0)
        box->addComboBox ("type", { "Step: jumps there", "Ramp: gradually from the event before" }, "Type");
    if (auto* type = box->getComboBoxComponent ("type"))
        type->setSelectedItemIndex (events[(size_t) index].ramp ? 1 : 0, juce::dontSendNotification);
    box->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    box->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    box->enterModalState (true, juce::ModalCallbackFunction::create ([this, box, index, safe = juce::Component::SafePointer<ArrangeView> (this)] (int result)
    {
        if (safe == nullptr || result != 1)
            return;
        auto list = studio.getProject().tempoEvents;
        if (! juce::isPositiveAndBelow (index, (int) list.size()))
            return;
        if (const auto bpm = box->getTextEditorContents ("bpm").replaceCharacter (',', '.').getDoubleValue(); bpm > 0.0)
            list[(size_t) index].bpm = bpm;
        if (auto* type = box->getComboBoxComponent ("type"))
            list[(size_t) index].ramp = type->getSelectedItemIndex() == 1;
        studio.setTempoEvents (list);
    }), true);
}

void ArrangeView::showTempoMenu()
{
    const auto& project = studio.getProject();
    juce::PopupMenu menu, type, view;
    menu.addSectionHeader ("TEMPO TRACK");
    menu.addItem (7, "Open Tempo Track Editor (Ctrl+T)");
    menu.addSeparator();
    menu.addItem (1, "Activate Tempo Track", true, project.tempoTrackOn);
    menu.addItem (2, "Lock", true, project.tempoLocked);
    type.addItem (10, "Ramp: gradually from the point before", true, project.newTempoType == pr7s::Project::NewTempoType::ramp);
    type.addItem (11, "Step: a jump", true, project.newTempoType == pr7s::Project::NewTempoType::step);
    type.addItem (12, "Automatic: as the point before", true, project.newTempoType == pr7s::Project::NewTempoType::automatic);
    menu.addSubMenu ("Type of New Tempo Points", type);
    menu.addSeparator();
    menu.addItem (3, "Insert Tempo Event at Cursor...", ! project.tempoLocked);
    menu.addItem (4, "Select All Events");
    menu.addItem (5, "Remove Selected Events", ! tempoSelection.empty() && ! project.tempoLocked);
    const std::pair<int, int> ranges[] = { { 60, 180 }, { 40, 240 }, { 20, 300 }, { 80, 160 }, { 100, 140 } };
    for (int i = 0; i < 5; ++i)
        view.addItem (20 + i, juce::String (ranges[i].first) + " to " + juce::String (ranges[i].second) + " BPM", true,
                      juce::roundToInt (project.tempoViewLow) == ranges[i].first && juce::roundToInt (project.tempoViewHigh) == ranges[i].second);
    menu.addSubMenu ("Visible Tempo Range", view);
    menu.addSeparator();
    menu.addItem (6, "Remove Tempo Track");
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<ArrangeView> (this), ranges] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        auto& s = safe->studio;
        const auto& p = s.getProject();
        switch (chosen)
        {
            case 1: s.setTempoTrackOn (! p.tempoTrackOn); break;
            case 2: s.setTempoLocked (! p.tempoLocked); break;
            case 3: safe->editTempoEvent (s.insertTempoEventAtCursor (p.bpmAt (s.getEngine().getPosition()))); break;
            case 4: safe->selectAllTempoEvents(); break;
            case 5: safe->deleteSelectedTempoEvents(); break;
            case 6: s.removeTempoTrack(); safe->tempoSelection.clear(); break;
            case 7: if (safe->onOpenTempoEditor) safe->onOpenTempoEditor(); break;
            case 10: s.setNewTempoType (pr7s::Project::NewTempoType::ramp); break;
            case 11: s.setNewTempoType (pr7s::Project::NewTempoType::step); break;
            case 12: s.setNewTempoType (pr7s::Project::NewTempoType::automatic); break;
            default:
                if (chosen >= 20 && chosen < 25)
                    s.setTempoView (ranges[chosen - 20].first, ranges[chosen - 20].second);
                break;
        }
    });
}

//==============================================================================
bool ArrangeView::isInterestedInDragSource (const SourceDetails& details)
{
    const auto d = details.description.toString();
    return d.startsWith ("chordpad:") || d.startsWith ("chord:") || d.startsWith ("progression:");
}

void ArrangeView::itemDropped (const SourceDetails& details)
{
    const auto d = details.description.toString();
    const auto at = details.localPosition.toFloat();
    const auto beat = juce::jmax (0.0, timeline.beatForX (at.x));
    // Onto the chord track: a chord event at the bar (a pad's chord, or the circle's).
    if (const auto row = rowAt (at.y); row && row->kind == Timeline::Row::Kind::chords)
    {
        // A progression from the library (0.40.0): from the bar dropped on.
        if (d.startsWith ("progression:"))
        {
            studio.addProgressionToChordTrack (pr7s::progressionFromText (d.fromFirstOccurrenceOf ("progression:", false, false)), beat);
            return;
        }
        std::optional<pr7s::Chord> chord;
        if (d.startsWith ("chordpad:"))
        {
            const auto pad = d.fromFirstOccurrenceOf ("chordpad:", false, false).getIntValue();
            if (juce::isPositiveAndBelow (pad, (int) studio.getChordPads().pads.size()))
                chord = studio.getChordPads().pads[(size_t) pad].chord;
        }
        else
            chord = pr7s::Chord::fromText (d.fromFirstOccurrenceOf ("chord:", false, false));
        if (chord)
        {
            const auto snapped = timeline.snapGrid (studio.getProject()) > 0.0 ? studio.getProject().barStartAt (beat) : beat;
            chordSelection = { studio.addChordEvent (snapped, *chord) };
        }
        return;
    }
    if (d.startsWith ("chordpad:"))
    {
        const auto pad = d.fromFirstOccurrenceOf ("chordpad:", false, false).getIntValue();
        if (const auto track = trackAtY (at.y); track != 0)
            studio.addPartFromPad (pad, track, beat);
    }
}

//==============================================================================
// The chord track (0.16.0, the manual's chord events and scale events): its chords
// as blocks, each until the next (the last for a bar), named and numbered in the
// scale in force; the scales in a strip below. Click: select and hear; drag: move
// (snapped); double-click: edit (an empty spot: the chord the one before suggests);
// right-click: its menu; Delete: the selected go.
juce::Rectangle<float> ArrangeView::getChordArea() const
{
    std::vector<Timeline::Row> made;
    for (const auto& row : rowsNow (made))
        if (row.kind == Timeline::Row::Kind::chords)
        {
            auto r = juce::Rectangle<float> (0.0f, rowTop (row), (float) getWidth(), (float) row.height).reduced (0.0f, 3.0f);
            return r.withTrimmedBottom (juce::jmax (14.0f, r.getHeight() * 0.3f));
        }
    return {};
}

const pr7s::ChordTrack& ArrangeView::shownChordTrack() const
{
    return chordPreview.has_value() ? *chordPreview : studio.getChordTrack();
}

juce::Rectangle<float> ArrangeView::getChordEventBounds (int index) const
{
    const auto& track = shownChordTrack();
    if (! juce::isPositiveAndBelow (index, (int) track.events.size()))
        return {};
    const auto area = getChordArea();
    const auto x1 = timeline.xForBeat (track.events[(size_t) index].beat);
    const auto x2 = timeline.xForBeat (track.endOf (index, studio.getProject().beatsPerBar()));
    return { x1, area.getY(), juce::jmax (8.0f, x2 - x1), area.getHeight() };
}

void ArrangeView::paintChords (juce::Graphics& g, const Timeline::Row& row)
{
    const auto& track = shownChordTrack();
    const auto& project = studio.getProject();
    const auto rowArea = juce::Rectangle<float> (0.0f, rowTop (row), (float) getWidth(), (float) row.height);
    const auto chords = getChordArea();
    const auto clip = g.getClipBounds().toFloat();
    for (int i = 0; i < (int) track.events.size(); ++i)
    {
        auto r = getChordEventBounds (i);
        if (r.getRight() < clip.getX() || r.getX() > clip.getRight())
            continue;
        const auto& e = track.events[(size_t) i];
        const auto key = studio.chordTrackKeyAt (e.beat);
        const auto selected = std::find (chordSelection.begin(), chordSelection.end(), i) != chordSelection.end();
        const auto inKey = e.chord.isInKey (key);
        r = r.reduced (1.0f, 0.0f);
        g.setColour (pr7s::FamilyColours::orange.withAlpha (inKey ? 0.22f : 0.12f));
        g.fillRoundedRectangle (r, 3.0f);
        // (0.85.3) Out of the key in force: a pink edge (and OUT OF KEY when there's room), as well as the pink numeral.
        g.setColour (selected ? juce::Colour (0xfff4eeea) : inKey ? pr7s::FamilyColours::orange.withAlpha (0.7f) : pr7s::FamilyColours::pink);
        g.drawRoundedRectangle (r, 3.0f, selected || ! inKey ? 2.0f : 1.0f);
        g.fillRect (r.withWidth (2.0f));   // (where it starts)
        if (! inKey && r.getWidth() > 110.0f)
        {
            g.setColour (pr7s::FamilyColours::pink);
            g.setFont (juce::Font (ParagonLookAndFeel::textFont (10.0f)));
            g.drawText ("OUT OF KEY", r.reduced (6.0f, 3.0f), juce::Justification::topRight, false);
        }
        // The edges a drag resizes: a faint grip on the selected one.
        if (selected && r.getWidth() >= 16.0f)
        {
            g.setColour (juce::Colour (0xfff4eeea).withAlpha (0.5f));
            g.fillRect (r.withX (r.getRight() - 3.0f).withWidth (2.0f).reduced (0.0f, r.getHeight() * 0.3f));
        }
        auto text = r.reduced (6.0f, 2.0f);
        g.setColour (ParagonColours::text);
        g.setFont (ChordUi::chordFont (juce::jlimit (11.0f, 18.0f, text.getHeight() * 0.5f)));
        g.drawText (e.chord.name (key), text.removeFromTop (text.getHeight() * 0.6f), juce::Justification::bottomLeft, true);
        g.setColour (inKey ? ParagonColours::textDim : pr7s::FamilyColours::pink);
        g.setFont (ChordUi::chordFont (juce::jlimit (9.0f, 12.0f, text.getHeight() * 0.8f)));
        g.drawText (e.chord.romanName (key), text, juce::Justification::topLeft, true);
    }
    if (track.events.empty())
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (juce::Font (ParagonLookAndFeel::textFont (12.0f)));
        g.drawText ("DOUBLE-CLICK TO ADD A CHORD, DROP A CHORD PAD HERE, OR RIGHT-CLICK A MIDI PART: DETECT CHORDS",
                    chords.withTrimmedLeft (12.0f), juce::Justification::centredLeft, true);
    }
    // The scales: a strip under the chords.
    const auto strip = rowArea.withTop (chords.getBottom() + 1.0f).withTrimmedBottom (3.0f);
    for (size_t i = 0; i < track.scales.size(); ++i)
    {
        const auto x1 = timeline.xForBeat (track.scales[i].beat);
        const auto x2 = i + 1 < track.scales.size() ? timeline.xForBeat (track.scales[i + 1].beat) : (float) getWidth();
        const auto r = juce::Rectangle<float> (x1, strip.getY(), juce::jmax (4.0f, x2 - x1), strip.getHeight()).reduced (1.0f, 0.0f);
        if (r.getRight() < clip.getX() || r.getX() > clip.getRight())
            continue;
        g.setColour (juce::Colour (0xff1b171c));
        g.fillRect (r);
        g.setColour (pr7s::FamilyColours::red.withAlpha (0.8f));
        g.fillRect (r.withWidth (2.0f));
        g.setColour (ParagonColours::textDim);
        g.setFont (juce::Font (ParagonLookAndFeel::textFont (juce::jlimit (9.0f, 11.0f, r.getHeight() * 0.75f))));
        g.drawText ((ChordUi::spokenKey (track.scales[i].key) + (track.autoScales ? "" : "  (set by hand)")).toUpperCase(), r.withTrimmedLeft (6.0f),
                    juce::Justification::centredLeft, true);
    }
    // (0.85.3) What the last edit did to the key, for a few seconds, in the key strip under the chords
    // (over the chords it hid them). The chord's name in the chords' font (Bebas has no small letters).
    if (chordWarning.isNotEmpty())
    {
        const auto visible = getLocalBounds().toFloat();
        auto banner = juce::Rectangle<float> (visible.getX(), strip.getY() - 1.0f, visible.getWidth(), strip.getHeight() + 2.0f)
                          .withTrimmedLeft (juce::jmax (0.0f, visible.getWidth() - 760.0f)).withTrimmedRight (8.0f);
        g.setColour (juce::Colour (0xff0b090c));
        g.fillRect (banner);
        g.setColour (pr7s::FamilyColours::pink);
        g.drawRect (banner, 1.0f);
        const auto height = juce::jlimit (9.0f, 13.0f, banner.getHeight() * 0.8f);
        juce::AttributedString text;
        const auto at = chordWarningName.isNotEmpty() ? chordWarning.indexOf (chordWarningName) : -1;
        if (at >= 0)
        {
            text.append (chordWarning.substring (0, at), juce::Font (ParagonLookAndFeel::textFont (height)), pr7s::FamilyColours::pink);
            text.append (chordWarningName, ChordUi::chordFont (height * 1.0f), pr7s::FamilyColours::pink);
            text.append (chordWarning.substring (at + chordWarningName.length()), juce::Font (ParagonLookAndFeel::textFont (height)), pr7s::FamilyColours::pink);
        }
        else
            text.append (chordWarning, juce::Font (ParagonLookAndFeel::textFont (height)), pr7s::FamilyColours::pink);
        text.setJustification (juce::Justification::centredLeft);
        text.setWordWrap (juce::AttributedString::none);
        text.draw (g, banner.reduced (8.0f, 0.0f));
    }
    juce::ignoreUnused (project);
}

int ArrangeView::chordEdgeAt (juce::Point<float> at, bool& atStart) const
{
    const auto& track = shownChordTrack();
    for (int i = (int) track.events.size(); --i >= 0;)
    {
        const auto r = getChordEventBounds (i);
        if (! r.contains (at) || r.getWidth() < 16.0f)
            continue;
        if (at.x >= r.getRight() - chordEdgeGrab)
        {
            atStart = false;
            return i;
        }
        if (at.x <= r.getX() + chordEdgeGrab)
        {
            atStart = true;
            return i;
        }
    }
    return -1;
}

void ArrangeView::selectChordsAt (const std::vector<int>& indices)
{
    chordSelection.clear();
    for (const auto i : indices)
        if (i >= 0)
            chordSelection.push_back (i);
}

void ArrangeView::noteChordKeyChange (const pr7s::ChordTrack& before)
{
    // (0.85.3, the owner: "it needs to tell me if something I do makes it go out of key")
    const auto& after = studio.getChordTrack();
    const auto keyAt = [] (const pr7s::ChordTrack& t, double beat, const pr7s::Key& fallback) { return t.keyAt (beat, fallback); };
    const auto fallback = studio.getProject().chordPads.key();
    const auto bar = [this] (double beat) { return studio.getProject().signatures().barAt (beat) + 1; };
    // A chord out of the key that wasn't before (or wasn't there).
    std::vector<juce::String> outside;
    auto name = juce::String();
    for (const auto& e : after.events)
    {
        const auto key = keyAt (after, e.beat, fallback);
        if (e.chord.isInKey (key))
            continue;
        const auto wasOut = std::any_of (before.events.begin(), before.events.end(), [&] (const pr7s::ChordEvent& b)
        {
            return std::abs (b.beat - e.beat) < 1.0e-6 && b.chord == e.chord && ! b.chord.isInKey (keyAt (before, b.beat, fallback));
        });
        if (! wasOut)
        {
            if (outside.empty())
                name = e.chord.name (key);
            outside.push_back (e.chord.name (key) + " (BAR " + juce::String (bar (e.beat)) + ") ISN'T IN " + ChordUi::spokenKey (key).toUpperCase());
        }
    }
    juce::String text;
    if (! outside.empty())
        text = "OUT OF KEY: " + outside.front() + (outside.size() > 1 ? ", AND " + juce::String ((int) outside.size() - 1) + " MORE" : juce::String());
    // The scales following the chords: the key itself moved.
    else if (after.autoScales && ! (after.scales == before.scales) && ! after.scales.empty())
    {
        for (const auto& sc : after.scales)
        {
            const auto was = before.scales.empty() ? std::optional<pr7s::Key> {} : std::optional<pr7s::Key> (keyAt (before, sc.beat, fallback));
            if (! was.has_value() || ! (*was == sc.key))
            {
                text = "THE KEY CHANGES: " + ChordUi::spokenKey (sc.key).toUpperCase() + " FROM BAR " + juce::String (bar (sc.beat))
                     + " (THE SCALES FOLLOW THE CHORDS)";
                break;
            }
        }
    }
    if (text.isEmpty())
        return;
    chordWarning = text;
    chordWarningName = name;
    chordWarningUntil = juce::Time::getMillisecondCounter() + 5000;
    repaint();
}

void ArrangeView::copySelectedChords (bool cut)
{
    if (chordSelection.empty())
        return;
    chordClip = studio.copyChordEvents (chordSelection);
    if (cut)
        deleteSelectedChordEvents();
}

bool ArrangeView::pasteChords()
{
    if (chordClip.events.empty())
        return false;
    const auto before = studio.getChordTrack();
    const auto at = juce::jmax (0.0, pr7s::snap (studio.getEngine().getPosition(), timeline.snapGrid (studio.getProject())));
    selectChordsAt (studio.pasteChordEvents (chordClip, at));
    noteChordKeyChange (before);
    repaint();
    return true;
}

void ArrangeView::duplicateSelectedChords()
{
    if (chordSelection.empty())
        return;
    const auto before = studio.getChordTrack();
    selectChordsAt (studio.duplicateChordEvents (chordSelection));
    noteChordKeyChange (before);
    repaint();
}

void ArrangeView::splitChordsAt (double beat)
{
    const auto before = studio.getChordTrack();
    selectChordsAt (studio.splitChordEvents (beat, chordSelection));
    noteChordKeyChange (before);
    repaint();
}

bool ArrangeView::chordDown (const juce::MouseEvent& event)
{
    selection = {};
    alsoSelected.clear();
    pointSelection = {};
    tempoSelection.clear();
    const auto& track = studio.getChordTrack();
    const auto beat = juce::jmax (0.0, timeline.beatForX (event.position.x));
    auto hit = -1;
    for (int i = 0; i < (int) track.events.size(); ++i)
        if (getChordEventBounds (i).contains (event.position))
            hit = i;
    if (event.mods.isPopupMenu())
    {
        drag = Drag::none;
        if (hit >= 0 && std::find (chordSelection.begin(), chordSelection.end(), hit) == chordSelection.end())
            chordSelection = { hit };
        if (event.position.y > getChordArea().getBottom())
            showScaleMenu (beat);
        else
            showChordMenu (beat, hit);
        repaint();
        return true;
    }
    if (hit < 0)
    {
        // An empty spot: the draw tool (or Alt) adds the chord suggested; else the selection clears.
        chordSelection.clear();
        drag = Drag::none;
        if ((tool == Tool::draw || event.mods.isAltDown()) && event.position.y <= getChordArea().getBottom())
        {
            const auto at = juce::jmax (0.0, pr7s::snap (beat, timeline.snapGrid (studio.getProject())));
            const auto index = studio.addChordEvent (at, studio.suggestChordAt (at));
            chordSelection = { index };
            studio.auditionChord (studio.getChordTrack().events[(size_t) index].chord);
        }
        repaint();
        return true;
    }
    if (tool == Tool::erase)
    {
        studio.removeChordEvents ({ hit });
        chordSelection.clear();
        drag = Drag::none;
        return true;
    }
    // (0.85.3) SPLIT cuts the chord in two where clicked (snapped unless Ctrl); GLUE joins it to the next.
    if (tool == Tool::split)
    {
        const auto at = event.mods.isCommandDown() ? beat : pr7s::snap (beat, timeline.snapGrid (studio.getProject()));
        const auto before = studio.getChordTrack();
        selectChordsAt (studio.splitChordEvents (at, { hit }));
        noteChordKeyChange (before);
        drag = Drag::none;
        repaint();
        return true;
    }
    if (tool == Tool::glue)
    {
        const auto before = studio.getChordTrack();
        selectChordsAt ({ studio.glueChordEvent (hit) });
        noteChordKeyChange (before);
        drag = Drag::none;
        repaint();
        return true;
    }
    // (0.85.3) An edge: dragged, the chord gets longer or shorter (what it grows over gives way).
    {
        auto atStart = false;
        if (const auto edge = chordEdgeAt (event.position, atStart); edge >= 0 && ! event.mods.isShiftDown())
        {
            chordSelection = { edge };
            chordOriginal = pr7s::withLengths (track, studio.getProject().beatsPerBar());
            chordEdited = edge;
            chordNewStart = chordOriginal[(size_t) edge].beat;
            chordNewEnd = chordNewStart + chordOriginal[(size_t) edge].length;
            chordDownBeat = beat;
            dragMoved = false;
            drag = atStart ? Drag::chordStart : Drag::chordEnd;
            repaint();
            return true;
        }
    }
    if (event.mods.isShiftDown())
    {
        const auto it = std::find (chordSelection.begin(), chordSelection.end(), hit);
        if (it != chordSelection.end())
            chordSelection.erase (it);
        else
            chordSelection.push_back (hit);
    }
    else if (std::find (chordSelection.begin(), chordSelection.end(), hit) == chordSelection.end())
        chordSelection = { hit };
    studio.auditionChord (track.events[(size_t) hit].chord);
    chordOriginal = pr7s::withLengths (track, studio.getProject().beatsPerBar());
    chordDownBeat = beat;
    chordDelta = 0.0;
    chordCopying = event.mods.isAltDown();   // (Alt: a copy moves, the chords stay)
    chordSelectionAtDown = chordSelection;
    dragMoved = false;
    drag = Drag::chordMove;
    repaint();
    return true;
}

void ArrangeView::chordDragTo (const juce::MouseEvent& event)
{
    if (event.getDistanceFromDragStart() < 4 && ! dragMoved)
        return;
    dragMoved = true;
    // (0.85.3) Worked out from the chords as they were, drawn as it goes; one undo step when let go.
    const auto grid = event.mods.isCommandDown() ? 0.0 : timeline.snapGrid (studio.getProject());
    const auto beat = timeline.beatForX (event.position.x);
    std::vector<pr7s::ChordEvent> others, placed;
    if (drag == Drag::chordMove)
    {
        // The selected ones moved together, snapped by the first's start; not before the song's start.
        auto delta = beat - chordDownBeat;
        if (! chordSelectionAtDown.empty())
        {
            const auto first = chordOriginal[(size_t) chordSelectionAtDown.front()].beat;
            delta = juce::jmax (0.0, pr7s::snap (first + delta, grid)) - first;
            for (const auto i : chordSelectionAtDown)
                delta = juce::jmax (delta, -chordOriginal[(size_t) i].beat);
        }
        chordDelta = delta;
        for (int i = 0; i < (int) chordOriginal.size(); ++i)
        {
            const auto chosen = std::find (chordSelectionAtDown.begin(), chordSelectionAtDown.end(), i) != chordSelectionAtDown.end();
            if (chosen)
            {
                auto e = chordOriginal[(size_t) i];
                e.beat += delta;
                placed.push_back (e);
            }
            if (! chosen || chordCopying)
                others.push_back (chordOriginal[(size_t) i]);
        }
    }
    else if (juce::isPositiveAndBelow (chordEdited, (int) chordOriginal.size()))
    {
        // An edge: the other one stays; at least a grid step (or a 32nd) long.
        const auto& e = chordOriginal[(size_t) chordEdited];
        const auto least = juce::jmax (pr7s::shortestChord, grid);
        const auto at = juce::jmax (0.0, grid > 0.0 ? pr7s::snap (beat, grid) : beat);
        chordNewStart = e.beat;
        chordNewEnd = e.beat + e.length;
        if (drag == Drag::chordStart)
            chordNewStart = juce::jmin (at, chordNewEnd - least);
        else
            chordNewEnd = juce::jmax (at, chordNewStart + least);
        for (int i = 0; i < (int) chordOriginal.size(); ++i)
            if (i != chordEdited)
                others.push_back (chordOriginal[(size_t) i]);
        auto resized = e;
        resized.beat = juce::jmax (0.0, chordNewStart);
        resized.length = chordNewEnd - resized.beat;
        placed.push_back (resized);
    }
    auto result = pr7s::placeChords (std::move (others), placed);
    auto preview = studio.getChordTrack();
    preview.events = std::move (result.events);
    chordPreview = std::move (preview);
    if (drag == Drag::chordMove)
        selectChordsAt (result.placed);
    else if (! result.placed.empty())
        chordSelection = { result.placed.front() };
    repaint();
}

void ArrangeView::chordUp()
{
    const auto wasDrag = drag;
    drag = Drag::none;
    chordPreview.reset();
    if (! dragMoved)
    {
        repaint();
        return;
    }
    const auto before = studio.getChordTrack();
    if (wasDrag == Drag::chordMove)
    {
        // The selection as it was when the drag began: those indices are the song's own.
        std::vector<int> chosen;
        for (int i = 0; i < (int) chordOriginal.size(); ++i)
            for (const auto j : chordSelectionAtDown)
                if (i == j)
                    chosen.push_back (i);
        if (std::abs (chordDelta) > 1.0e-9 || chordCopying)
            selectChordsAt (studio.moveChordEvents (chosen, chordDelta, chordCopying));
        else
            selectChordsAt (chosen);
    }
    else if (juce::isPositiveAndBelow (chordEdited, (int) chordOriginal.size()))
        chordSelection = { studio.resizeChordEvent (chordEdited, chordNewStart, chordNewEnd) };
    noteChordKeyChange (before);
    repaint();
}

void ArrangeView::deleteSelectedChordEvents()
{
    const auto before = studio.getChordTrack();
    studio.removeChordEvents (chordSelection);
    noteChordKeyChange (before);
    chordSelection.clear();
    repaint();
}

void ArrangeView::editChordEvent (int index)
{
    const auto& track = studio.getChordTrack();
    if (! juce::isPositiveAndBelow (index, (int) track.events.size()))
        return;
    const auto& e = track.events[(size_t) index];
    std::optional<pr7s::Chord> before;
    if (index > 0)
        before = track.events[(size_t) index - 1].chord;
    auto editor = std::make_unique<ChordEventEditor> (e.chord, studio.chordTrackKeyAt (e.beat), before);
    const auto beat = e.beat;
    // (Found by its beat: the list may change while the box is open.)
    const auto indexOf = [this, beat]
    {
        const auto& events = studio.getChordTrack().events;
        for (int i = 0; i < (int) events.size(); ++i)
            if (std::abs (events[(size_t) i].beat - beat) < 1.0e-6)
                return i;
        return -1;
    };
    editor->onChange = [this, indexOf] (const pr7s::Chord& chord)
    {
        const auto before = studio.getChordTrack();
        studio.setChordEventChord (indexOf(), chord);
        noteChordKeyChange (before);
        studio.auditionChord (chord);
    };
    editor->onDelete = [this, indexOf]
    {
        studio.removeChordEvents ({ indexOf() });
        chordSelection.clear();
        if (auto* box = chordEditor != nullptr ? chordEditor->findParentComponentOfClass<juce::CallOutBox>() : nullptr)
        {
            box->exitModalState (0);
            box->setVisible (false);
        }
    };
    chordEditor = editor.get();
    chordSelection = { index };
    const auto r = getChordEventBounds (index).toNearestInt();
    juce::CallOutBox::launchAsynchronously (std::move (editor), localAreaToGlobal (r), nullptr);
}

void ArrangeView::showChordMenu (double beat, int index)
{
    const auto& track = studio.getChordTrack();
    const auto key = studio.chordTrackKeyAt (beat);
    juce::PopupMenu menu, suggestions, keyChords;
    if (index >= 0)
    {
        const auto& e = track.events[(size_t) index];
        menu.addSectionHeader (e.chord.name (key) + "  (" + e.chord.romanName (key) + ")");
        menu.addItem (1, "Edit Chord...");
        menu.addItem (2, "Hear It");
        menu.addItem (3, chordSelection.size() > 1 ? "Delete Them" : "Delete");
        // In its place, what the chord before suggests.
        if (index > 0)
        {
            const auto next = pr7s::suggestNext (track.events[(size_t) index - 1].chord, key);
            for (int i = 0; i < (int) next.size() && i < 10; ++i)
                suggestions.addItem (100 + i, next[(size_t) i].chord.name (key) + "  " + next[(size_t) i].chord.romanName (key) + "   "
                                                  + juce::String (next[(size_t) i].score));
            menu.addSubMenu ("Replace with a Suggestion", suggestions);
        }
        // (0.85.3) As clips (also: drag to move, Alt-drag to copy, drag an edge to lengthen or shorten).
        menu.addSeparator();
        menu.addItem (40, chordSelection.size() > 1 ? "Cut Them" : "Cut");
        menu.addItem (41, chordSelection.size() > 1 ? "Copy Them" : "Copy");
        menu.addItem (43, chordSelection.size() > 1 ? "Duplicate Them" : "Duplicate");
        menu.addItem (44, "Split Here", beat > e.beat + pr7s::shortestChord && beat < track.endOf (index, studio.getProject().beatsPerBar()) - pr7s::shortestChord);
        menu.addItem (45, "Glue to the Next Chord", index + 1 < (int) track.events.size());
        menu.addSeparator();
        menu.addItem (4, chordSelection.size() > 1 ? "Chords to MIDI (these)" : "Chord to MIDI (this one)");
    }
    else
    {
        const auto suggested = studio.suggestChordAt (beat);
        menu.addItem (10, "Add " + suggested.name (key) + " Here (suggested)");
        const auto chords = pr7s::keyChords (key);
        for (int i = 0; i < (int) chords.size(); ++i)
            keyChords.addItem (200 + i, chords[(size_t) i].name (key) + "  " + chords[(size_t) i].romanName (key));
        menu.addSubMenu ("Add a Chord of " + ChordUi::spokenKey (key), keyChords);
    }
    menu.addItem (42, "Paste at the Playhead", hasChordClip());
    menu.addSeparator();
    menu.addItem (5, "Chords to MIDI (all, onto the selected track)", ! track.events.empty());
    menu.addItem (6, "Assign the Chords to the Pads", ! track.events.empty());
    menu.addItem (9, chordSelection.size() > 1 ? "Piano, Guitar and Bass Ideas (for these)..." : "Piano, Guitar and Bass Ideas...", ! track.events.empty());
    menu.addItem (13, chordSelection.size() > 1 ? "Build a Section (for these)..." : "Build a Section...", ! track.events.empty());
    menu.addItem (11, "Progression Library...");
    menu.addItem (16, "Key Check: Notes Outside the Key...", onKeyCheck != nullptr);   // (0.84.0)
    menu.addItem (7, "Scales Follow the Chords", true, track.autoScales);
    menu.addSeparator();
    menu.addItem (8, "Remove the Chord Track (its chords kept)");
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<ArrangeView> (this), beat, index, key] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        auto& studio = safe->studio;
        const auto& events = studio.getChordTrack().events;
        const auto grid = safe->timeline.snapGrid (studio.getProject());
        switch (chosen)
        {
            case 1: safe->editChordEvent (index); return;
            case 2: if (juce::isPositiveAndBelow (index, (int) events.size())) studio.auditionChord (events[(size_t) index].chord); return;
            case 3: safe->deleteSelectedChordEvents(); return;
            case 4: studio.chordsToMidi (studio.getSelectedTrack(), safe->chordSelection); return;
            case 5: studio.chordsToMidi (studio.getSelectedTrack()); return;
            case 6: studio.assignPadsFromChordTrack(); if (safe->onChordsToPads) safe->onChordsToPads(); return;
            case 7: studio.setAutoScales (! studio.getChordTrack().autoScales); return;
            case 8: studio.removeChordTrack(); return;
            case 11: if (safe->onProgressions) safe->onProgressions(); return;
            case 16: if (safe->onKeyCheck) safe->onKeyCheck(); return;
            case 40: safe->copySelectedChords (true); return;
            case 41: safe->copySelectedChords (false); return;
            case 42: safe->pasteChords(); return;
            case 43: safe->duplicateSelectedChords(); return;
            case 44: safe->chordSelection = { index }; safe->splitChordsAt (juce::jmax (0.0, pr7s::snap (beat, grid))); return;
            case 45:
            {
                const auto was = studio.getChordTrack();
                safe->selectChordsAt ({ studio.glueChordEvent (index) });
                safe->noteChordKeyChange (was);
                return;
            }
            case 9: if (safe->onLineIdeas) safe->onLineIdeas (safe->chordSelection.size() > 1 ? safe->chordSelection : std::vector<int> {}); return;
            case 13: if (safe->onBuildSection) safe->onBuildSection (safe->chordSelection.size() > 1 ? safe->chordSelection : std::vector<int> {}); return;
            case 10:
            {
                const auto at = juce::jmax (0.0, pr7s::snap (beat, grid));
                safe->chordSelection = { studio.addChordEvent (at, studio.suggestChordAt (at)) };
                return;
            }
            default: break;
        }
        const auto before = studio.getChordTrack();
        if (chosen >= 100 && chosen < 200 && index > 0)
        {
            const auto next = pr7s::suggestNext (events[(size_t) index - 1].chord, key);
            if (juce::isPositiveAndBelow (chosen - 100, (int) next.size()))
                studio.setChordEventChord (index, next[(size_t) (chosen - 100)].chord);
        }
        if (chosen >= 200)
        {
            const auto chords = pr7s::keyChords (key);
            if (juce::isPositiveAndBelow (chosen - 200, (int) chords.size()))
                safe->chordSelection = { studio.addChordEvent (juce::jmax (0.0, pr7s::snap (beat, grid)), chords[(size_t) (chosen - 200)]) };
        }
        safe->noteChordKeyChange (before);
    });
}

void ArrangeView::showScaleMenu (double beat)
{
    // The scale from here on, set by hand (the scales then stay as set), or following the chords again.
    const auto& track = studio.getChordTrack();
    const auto now = studio.chordTrackKeyAt (beat);
    juce::PopupMenu menu, majors, minors;
    const auto& keys = pr7s::Key::all();
    for (int i = 0; i < (int) keys.size(); ++i)
        (keys[(size_t) i].minor ? minors : majors).addItem (1 + i, ChordUi::spokenKey (keys[(size_t) i]), true, keys[(size_t) i] == now);
    menu.addSectionHeader ("THE SCALE FROM HERE: " + ChordUi::spokenKey (now).toUpperCase());
    menu.addSubMenu ("Major Keys", majors);
    menu.addSubMenu ("Minor Keys", minors);
    menu.addSeparator();
    menu.addItem (100, "Scales Follow the Chords", true, track.autoScales);
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<ArrangeView> (this), beat] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        auto& studio = safe->studio;
        if (chosen == 100)
        {
            studio.setAutoScales (! studio.getChordTrack().autoScales);
            return;
        }
        const auto& keys = pr7s::Key::all();
        if (! juce::isPositiveAndBelow (chosen - 1, (int) keys.size()))
            return;
        // At the chord sounding here (else the bar).
        auto scales = studio.getChordTrack().scales;
        const auto index = studio.getChordTrack().indexAt (beat);
        const auto at = index >= 0 ? studio.getChordTrack().events[(size_t) index].beat : studio.getProject().barStartAt (beat);
        scales.erase (std::remove_if (scales.begin(), scales.end(), [at] (const pr7s::ScaleEvent& s) { return std::abs (s.beat - at) < 1.0e-6; }),
                      scales.end());
        scales.push_back ({ at, keys[(size_t) (chosen - 1)] });
        if (std::none_of (scales.begin(), scales.end(), [] (const pr7s::ScaleEvent& s) { return s.beat <= 1.0e-6; }))
            scales.push_back ({ 0.0, studio.chordTrackKeyAt (0.0) });
        studio.setScaleEvents (scales);
    });
}

//==============================================================================
// WARP (0.32.0, the manual's Free Warp: "click, and hold. A warp marker is inserted. Drag the warp
// marker to the new position"; it snaps to hitpoints, Ctrl not; any modifier key and a click on a
// marker removes it). Alt-click adds a hitpoint there, Shift-click one disables it (the manual's
// Edit Hitpoints, which we keep in the arrangement: there's no Sample Editor).
int ArrangeView::warpMarkerAt (int trackId, int index, juce::Point<float> point) const
{
    const auto* track = studio.getProject().findTrack (trackId);
    if (track == nullptr || ! juce::isPositiveAndBelow (index, (int) track->parts.size()))
        return -1;
    const auto& part = track->parts[(size_t) index];
    const auto area = getPartBounds (trackId, index);
    for (int k = 1; k + 1 < (int) part.warp.size(); ++k)
        if (std::abs (area.getX() + (float) (part.warp[(size_t) k].beat * timeline.pixelsPerBeat) - point.x) <= 5.0f)
            return k;
    return -1;
}

bool ArrangeView::warpDown (const juce::MouseEvent& event)
{
    const auto hit = partAt (event.position);
    if (! hit)
        return false;
    const auto* track = studio.getProject().findTrack (hit->first);
    const auto& part = track->parts[(size_t) hit->second];
    if (! part.isAudio() || track->frozen)
        return false;
    select (hit->first, hit->second);
    const auto area = getPartBounds (hit->first, hit->second);
    const auto beat = (event.position.x - area.getX()) / timeline.pixelsPerBeat;
    const auto tempo = studio.getProject().tempoMap();
    drag = Drag::none;
    const auto marker = warpMarkerAt (hit->first, hit->second, event.position);
    const auto anyModifier = event.mods.isAltDown() || event.mods.isShiftDown() || event.mods.isCommandDown();
    if (marker > 0 && anyModifier)
    {
        studio.removeWarpMarkers (part.id, { marker });
        return true;
    }
    if (marker < 0 && event.mods.isAltDown())
    {
        studio.addHitpoint (part.audioFile, pr7s::warpSecondsAt (part, beat, tempo));
        return true;
    }
    const auto hits = studio.getPartHitpoints (part.id);
    if (marker < 0 && event.mods.isShiftDown())
    {
        for (const auto h : hits)
            if (std::abs ((h - beat) * timeline.pixelsPerBeat) <= 4.0)
            {
                studio.removeHitpoint (part.audioFile, pr7s::warpSecondsAt (part, h, tempo));
                break;
            }
        return true;
    }
    warpPartId = part.id;
    warpIndex = marker;
    if (marker < 0)
    {
        // A new marker: at the hitpoint within 8 px (Ctrl: exactly where clicked).
        auto at = beat;
        auto nearest = 8.0;   // px
        if (! event.mods.isCommandDown())
            for (const auto h : hits)
                if (const auto d = std::abs ((h - beat) * timeline.pixelsPerBeat); d <= nearest)
                {
                    nearest = d;
                    at = h;
                }
        if (! studio.addWarpMarker (part.id, at))
            return true;
        if (const auto found = studio.findPart (part.id))
        {
            const auto& now = studio.getProject().findTrack (found->first)->parts[(size_t) found->second];
            for (int k = 1; k + 1 < (int) now.warp.size(); ++k)
                if (std::abs (now.warp[(size_t) k].beat - at) < 1.0e-6)
                    warpIndex = k;
        }
        if (warpIndex < 0)
            return true;
    }
    warpTo = beat;
    drag = Drag::warpMarker;
    return true;
}

void ArrangeView::warpDrag (const juce::MouseEvent& event)
{
    if (event.getDistanceFromDragStart() > 2)
        dragMoved = true;
    if (const auto at = studio.findPart (warpPartId))
    {
        const auto& part = studio.getProject().findTrack (at->first)->parts[(size_t) at->second];
        // The grid pulls it near (the manual: "magnetic"); Ctrl: free.
        auto beat = timeline.beatForX (event.position.x);
        if (! event.mods.isCommandDown())
            if (const auto grid = timeline.snapGrid (studio.getProject()); grid > 0.0)
            {
                const auto snapped = pr7s::snap (beat, grid);
                if (std::abs ((snapped - beat) * timeline.pixelsPerBeat) <= 8.0)
                    beat = snapped;
            }
        warpTo = beat - part.start;
    }
    repaint();
}

void ArrangeView::warpUp()
{
    drag = Drag::none;
    if (dragMoved)
        studio.moveWarpMarker (warpPartId, warpIndex, warpTo);
    warpPartId = 0;
    warpIndex = -1;
    repaint();
}

void ArrangeView::showHitpointSettings (juce::Component* target)
{
    // Threshold and Minimum Length (the manual's hitpoint filters), and the hand edits put back.
    struct Panel final : juce::Component
    {
        explicit Panel (pr7s::Studio& s) : studio (s)
        {
            for (auto* slider : { &threshold, &minimum })
            {
                slider->setSliderStyle (juce::Slider::LinearHorizontal);
                slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
                addAndMakeVisible (*slider);
            }
            threshold.setRange (0.0, 1.0, 0.01);
            threshold.setValue (studio.getHitpointSettings().threshold, juce::dontSendNotification);
            minimum.setRange (0.0, 500.0, 1.0);
            minimum.setTextValueSuffix (" ms");
            minimum.setValue (studio.getHitpointSettings().minimumSeconds * 1000.0, juce::dontSendNotification);
            const auto apply = [this] { studio.setHitpointSettings ({ threshold.getValue(), minimum.getValue() / 1000.0 }); };
            threshold.onValueChange = apply;
            minimum.onValueChange = apply;
            for (auto* label : { &thresholdLabel, &minimumLabel })
            {
                label->setColour (juce::Label::textColourId, ParagonColours::textDim);
                addAndMakeVisible (*label);
            }
            setSize (360, 76);
        }
        void resized() override
        {
            auto area = getLocalBounds().reduced (8);
            auto row = area.removeFromTop (28);
            thresholdLabel.setBounds (row.removeFromLeft (110));
            threshold.setBounds (row);
            area.removeFromTop (4);
            row = area.removeFromTop (28);
            minimumLabel.setBounds (row.removeFromLeft (110));
            minimum.setBounds (row);
        }
        pr7s::Studio& studio;
        juce::Slider threshold, minimum;
        juce::Label thresholdLabel { {}, "THRESHOLD" }, minimumLabel { {}, "MINIMUM LENGTH" };
    };
    auto panel = std::make_unique<Panel> (studio);
    juce::CallOutBox::launchAsynchronously (std::move (panel), target->getScreenBounds(), nullptr);
}

//==============================================================================
// Folder tracks (0.50.0).
std::vector<std::pair<double, double>> ArrangeView::folderBlocks (int folderId) const
{
    const auto& project = studio.getProject();
    std::vector<std::pair<double, double>> spans;
    for (const auto id : project.folderMembers (folderId, true))
        if (const auto* t = project.findTrack (id))
            for (const auto& part : t->parts)
                spans.push_back ({ part.start, part.end() });
    std::sort (spans.begin(), spans.end());
    std::vector<std::pair<double, double>> merged;
    for (const auto& span : spans)
        if (! merged.empty() && span.first <= merged.back().second + 1.0e-9)
            merged.back().second = juce::jmax (merged.back().second, span.second);
        else
            merged.push_back (span);
    return merged;
}

std::vector<int> ArrangeView::folderPartsIn (int folderId, double start, double end) const
{
    const auto& project = studio.getProject();
    std::vector<int> ids;
    for (const auto id : project.folderMembers (folderId, true))
        if (const auto* t = project.findTrack (id); t != nullptr && ! t->frozen)
            for (const auto& part : t->parts)
                if (part.start < end - 1.0e-9 && part.end() > start + 1.0e-9)
                    ids.push_back (part.id);
    return ids;
}

void ArrangeView::paintFolders (juce::Graphics& g)
{
    const auto& project = studio.getProject();
    const auto clip = g.getClipBounds().toFloat();
    for (const auto& folder : project.tracks)
    {
        if (! folder.isFolder() || folder.hidden || project.isFoldedAway (folder))
            continue;
        if (folder.folderData == pr7s::Track::FolderData::never
            || (folder.folderData == pr7s::Track::FolderData::hideWhenExpanded && folder.folderOpen))
            continue;
        const auto top = trackTop (folder.id) + 4.0f, height = (float) timeline.trackRowHeight() - 8.0f;
        if (top > clip.getBottom() || top + height < clip.getY())
            continue;
        // The tracks in it with parts: a strip each inside the blocks.
        std::vector<const pr7s::Track*> members;
        for (const auto id : project.folderMembers (folder.id, true))
            if (const auto* t = project.findTrack (id); t != nullptr && ! t->parts.empty())
                members.push_back (t);
        const auto shift = folderDragId == folder.id ? folderDelta : 0.0;
        for (const auto& [start, end] : folderBlocks (folder.id))
        {
            const auto x1 = timeline.xForBeat (start + shift), x2 = timeline.xForBeat (end + shift);
            if (x2 < clip.getX() || x1 > clip.getRight())
                continue;
            const auto block = juce::Rectangle<float> (x1, top, juce::jmax (2.0f, x2 - x1), height);
            g.setColour (folder.colour.withAlpha (0.16f));
            g.fillRoundedRectangle (block, 3.0f);
            g.setColour (folder.colour.withAlpha (0.6f));
            g.drawRoundedRectangle (block, 3.0f, 1.0f);
            const auto strip = members.empty() ? 0.0f : (height - 6.0f) / (float) members.size();
            for (size_t m = 0; m < members.size(); ++m)
                for (const auto& part : members[m]->parts)
                {
                    if (part.end() <= start || part.start >= end)
                        continue;
                    const auto px1 = timeline.xForBeat (part.start + shift), px2 = timeline.xForBeat (part.end() + shift);
                    g.setColour ((part.muted ? ParagonColours::textDim : members[m]->colour).withAlpha (0.8f));
                    g.fillRect (juce::Rectangle<float> (px1, top + 3.0f + strip * (float) m + strip * 0.2f,
                                                        juce::jmax (1.0f, px2 - px1), juce::jmax (1.0f, strip * 0.6f)));
                }
        }
    }
}

bool ArrangeView::folderDown (const juce::MouseEvent& event)
{
    const auto row = rowAt (event.position.y);
    if (! row || row->kind != Timeline::Row::Kind::track)
        return false;
    const auto* folder = studio.getProject().findTrack (row->busId);
    if (folder == nullptr || ! folder->isFolder())
        return false;
    const auto beat = timeline.beatForX (event.position.x);
    for (const auto& [start, end] : folderBlocks (folder->id))
        if (beat >= start && beat <= end)
        {
            auto ids = folderPartsIn (folder->id, start, end);
            if (event.mods.isShiftDown())
                for (const auto id : getSelectedParts())
                    if (std::find (ids.begin(), ids.end(), id) == ids.end())
                        ids.push_back (id);
            selectParts (ids);
            studio.selectTrack (folder->id);
            drag = Drag::folderMove;
            dragMoved = false;
            folderDragIds = ids;
            folderDragId = folder->id;
            folderDownBeat = beat;
            folderDelta = 0.0;
            repaint();
            return true;
        }
    return false;
}

//==============================================================================
// Drag out (0.53.1).
bool ArrangeView::wantsDragOut (const juce::MouseEvent& event) const
{
    // (Only on screen: off it, nothing is "under the mouse", which would read as another window.)
    if (draggingOut || ! isShowing() || getSelectedParts().size() > 1 || ! selection.isValid())
        return false;
    // A copy (Alt) leaving the arrangement: for a program's window lying over the studio, which the
    // studio can't tell from its own.
    if (dragCopies && ! getLocalBounds().contains (event.position.toInt()))
        return true;
    // Over a window that isn't the studio's main one: a plug-in's, or none of ours (another program, the desktop).
    const auto* under = juce::Desktop::getInstance().findComponentAt (event.getScreenPosition());
    return under == nullptr || under->getTopLevelComponent() != getTopLevelComponent();
}

juce::File ArrangeView::dragPartOut()
{
    if (draggingOut || ! selection.isValid())
        return {};
    const auto partId = partIdAt (selection);
    // The move is over: the part stays where it was.
    drag = Drag::none;
    dragMoved = false;
    repaint();
    juce::String error;
    const auto file = studio.writePartForDrag (partId, error);
    if (file == juce::File())
        return {};
    const juce::ScopedValueSetter<bool> busy (draggingOut, true);
    if (onExternalDrag)
        onExternalDrag ({ file.getFullPathName() });
    else
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);   // (a copy, never a move)
    return file;
}
