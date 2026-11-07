// The arrangement's marker track (0.49.0, the manual's Markers chapter): position markers and
// cycle markers drawn, clicked, dragged, named; the lines they draw down the arrangement. The
// markers live in the song (Studio's addPositionMarker and the rest).

#include "ArrangeView.h"

#include "ui/ParagonLookAndFeel.h"

#include <cmath>

namespace
{
    constexpr float edgeGrab = 5.0f;   // px either side of a cycle marker's edge that drags it
    const juce::Colour pearl { 0xfff4eeea };

    juce::String markerText (const pr7s::Marker& m, bool ids)
    {
        juce::String text;
        if (ids)
            text << pr7s::markerIdText (m);
        if (m.name.isNotEmpty())
            text << (text.isEmpty() ? "" : "  ") << m.name;
        return text;
    }
}

const std::vector<pr7s::Marker>& ArrangeView::shownMarkers() const
{
    return drag != Drag::none && ! markerDragged.empty() ? markerDragged : studio.getMarkerTrack().markers;
}

juce::Rectangle<float> ArrangeView::getMarkerArea() const
{
    std::vector<Timeline::Row> made;
    for (const auto& row : rowsNow (made))
        if (row.kind == Timeline::Row::Kind::markers)
            return juce::Rectangle<float> (0.0f, rowTop (row), (float) getWidth(), (float) row.height).reduced (0.0f, 2.0f);
    return {};
}

juce::Rectangle<float> ArrangeView::getMarkerBounds (int index) const
{
    const auto area = getMarkerArea();
    const auto& markers = shownMarkers();
    if (area.isEmpty() || ! juce::isPositiveAndBelow (index, (int) markers.size()))
        return {};
    const auto& m = markers[(size_t) index];
    const auto half = area.getHeight() * 0.5f;
    const auto x = timeline.xForBeat (m.start);
    if (m.cycle)
        return { x, area.getY() + 1.0f, juce::jmax (2.0f, timeline.xForBeat (m.end) - x), half - 2.0f };
    const auto font = ParagonLookAndFeel::titleFont (juce::jmin (15.0f, half * 0.75f));
    const auto text = markerText (m, studio.getMarkerTrack().showIds);
    const auto width = juce::jmax (16.0f, juce::GlyphArrangement::getStringWidth (juce::Font (font), text) + 12.0f);
    return { x, area.getY() + half + 1.0f, width, half - 2.0f };
}

int ArrangeView::markerAt (juce::Point<float> at) const
{
    // Position markers first (their flags are below the cycle bars), the latest first.
    const auto& markers = shownMarkers();
    for (auto pass = 0; pass < 2; ++pass)
        for (int i = (int) markers.size() - 1; i >= 0; --i)
            if (markers[(size_t) i].cycle == (pass == 1))
            {
                auto bounds = getMarkerBounds (i);
                if (markers[(size_t) i].cycle)
                    bounds = bounds.expanded (edgeGrab, 0.0f);
                if (bounds.contains (at))
                    return i;
            }
    return -1;
}

void ArrangeView::paintMarkers (juce::Graphics& g, const Timeline::Row& row)
{
    const auto area = juce::Rectangle<float> (0.0f, rowTop (row), (float) getWidth(), (float) row.height).reduced (0.0f, 2.0f);
    const auto& track = studio.getMarkerTrack();
    const auto& markers = shownMarkers();
    const auto half = area.getHeight() * 0.5f;
    // The two strips: cycle markers above, position markers below.
    g.setColour (ParagonColours::textDim.withAlpha (0.08f));
    g.fillRect (area.withY (area.getY() + half).withHeight (1.0f));
    const auto font = ParagonLookAndFeel::titleFont (juce::jmin (15.0f, half * 0.75f));
    g.setFont (font);
    const auto isSelected = [this] (int i) { return std::find (markerSelection.begin(), markerSelection.end(), i) != markerSelection.end(); };
    // (0.57.1) Only those in the stretch being redrawn: a flag's width is measured from its text, and
    // that was done for every marker in the song on every playhead frame.
    const auto clip = g.getClipBounds().toFloat();

    for (int i = 0; i < (int) markers.size(); ++i)
    {
        const auto& m = markers[(size_t) i];
        if (! m.cycle)
            continue;
        const auto bar = getMarkerBounds (i);
        if (bar.getRight() < clip.getX() || bar.getX() > clip.getRight())
            continue;
        const auto selected = isSelected (i);
        const auto tint = pr7s::FamilyColours::pink;
        g.setColour (tint.withAlpha (selected ? 0.55f : 0.28f));
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (tint.withAlpha (selected ? 1.0f : 0.8f));
        g.fillRect (bar.withWidth (2.0f));
        g.fillRect (bar.withX (bar.getRight() - 2.0f).withWidth (2.0f));
        g.setColour (selected ? juce::Colours::white : ParagonColours::text);
        g.drawText (markerText (m, track.showIds), bar.reduced (6.0f, 0.0f), juce::Justification::centredLeft, true);
    }
    for (int i = 0; i < (int) markers.size(); ++i)
    {
        const auto& m = markers[(size_t) i];
        if (m.cycle)
            continue;
        const auto at = timeline.xForBeat (m.start);
        if (at > clip.getRight() || at + 800.0f < clip.getX())   // (a flag is narrower than 800 px)
            continue;
        const auto flag = getMarkerBounds (i);
        if (flag.getRight() < clip.getX() || flag.getX() > clip.getRight())
            continue;
        const auto selected = isSelected (i);
        const auto tint = pr7s::FamilyColours::orange;
        g.setColour (tint.withAlpha (selected ? 1.0f : 0.75f));
        g.fillRect (juce::Rectangle<float> (flag.getX(), area.getY(), 1.5f, area.getHeight()));
        g.setColour (tint.withAlpha (selected ? 0.95f : 0.5f));
        g.fillRoundedRectangle (flag, 3.0f);
        g.setColour (selected ? juce::Colours::black : ParagonColours::text);
        g.drawText (markerText (m, track.showIds), flag.reduced (6.0f, 0.0f), juce::Justification::centredLeft, false);
    }
    if (track.locked)
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::titleFont (11.0f));
        g.drawText ("LOCKED", area.reduced (8.0f, 0.0f), juce::Justification::topRight, false);
    }
}

void ArrangeView::paintMarkerLines (juce::Graphics& g)
{
    const auto& track = studio.getMarkerTrack();
    if (! track.shown || ! track.showLines)
        return;
    const auto top = (float) Timeline::rulerHeight;
    const auto height = (float) getHeight() - top;
    const auto clip = g.getClipBounds().toFloat();
    const auto line = [&] (double beat, juce::Colour colour)
    {
        const auto x = timeline.xForBeat (beat);
        if (x >= clip.getX() - 2.0f && x <= clip.getRight() + 2.0f)
        {
            g.setColour (colour);
            g.fillRect (juce::Rectangle<float> (x, top, 1.0f, height));
        }
    };
    for (const auto& m : shownMarkers())
    {
        line (m.start, (m.cycle ? pr7s::FamilyColours::pink : pearl).withAlpha (0.16f));
        if (m.cycle)
            line (m.end, pr7s::FamilyColours::pink.withAlpha (0.16f));
    }
}

bool ArrangeView::markerDown (const juce::MouseEvent& event)
{
    const auto& track = studio.getMarkerTrack();
    const auto hit = markerAt (event.position);
    if (event.mods.isPopupMenu())
    {
        if (hit >= 0 && std::find (markerSelection.begin(), markerSelection.end(), hit) == markerSelection.end())
            markerSelection = { hit };
        repaint();
        showMarkerMenu (hit);
        return true;
    }
    const auto& project = studio.getProject();
    const auto raw = juce::jmax (0.0, timeline.beatForX (event.position.x));
    const auto beat = juce::jmax (0.0, pr7s::snap (raw, timeline.snapGrid (project)));
    if (tool == Tool::erase)
    {
        if (event.mods.isAltDown())
            studio.removeMarkersFrom (hit >= 0 ? track.markers[(size_t) hit].start : beat);   // (all after it)
        else if (hit >= 0)
            studio.removeMarkers ({ hit });
        markerSelection.clear();
        repaint();
        return true;
    }
    if (hit < 0)
    {
        if (! event.mods.isShiftDown() || event.mods.isAltDown() || event.mods.isCommandDown())
            markerSelection.clear();
        if (track.locked)
        {
            repaint();
            return true;
        }
        if (tool == Tool::draw || (event.mods.isAltDown() && ! event.mods.isCommandDown()))
        {
            const auto index = studio.addPositionMarker (beat);
            if (index >= 0)
            {
                markerSelection = { index };
                if (event.mods.isShiftDown())
                    editMarkerName (index);   // (Shift+Alt: and name it)
            }
            repaint();
            return true;
        }
        if (event.mods.isCommandDown())
        {
            // Ctrl-drag: a cycle marker over what's dragged (Shift+Ctrl: and name it).
            drag = Drag::markerNewCycle;
            dragMoved = false;
            markerDownBeat = beat;
            markerNameAfter = event.mods.isShiftDown();
            markerDragged = track.markers;
            pr7s::Marker m;
            m.cycle = true;
            m.id = track.nextId (true);
            m.start = m.end = beat;
            markerDragged.push_back (m);
            markerDragIndex = (int) markerDragged.size() - 1;
            return true;
        }
        repaint();
        return true;
    }
    if (event.mods.isShiftDown())
    {
        const auto it = std::find (markerSelection.begin(), markerSelection.end(), hit);
        if (it != markerSelection.end())
            markerSelection.erase (it);
        else
            markerSelection.push_back (hit);
    }
    else if (std::find (markerSelection.begin(), markerSelection.end(), hit) == markerSelection.end())
        markerSelection = { hit };
    if (! track.locked)
    {
        const auto& m = track.markers[(size_t) hit];
        drag = Drag::markerMove;
        if (m.cycle)
        {
            if (std::abs (event.position.x - timeline.xForBeat (m.start)) <= edgeGrab)
                drag = Drag::markerStart, markerSelection = { hit };
            else if (std::abs (event.position.x - timeline.xForBeat (m.end)) <= edgeGrab)
                drag = Drag::markerEnd, markerSelection = { hit };
        }
        dragMoved = false;
        markerDownBeat = raw;
        markerDragIndex = hit;
        markerOriginal = markerDragged = track.markers;
    }
    repaint();
    return true;
}

void ArrangeView::markerDrag (const juce::MouseEvent& event)
{
    const auto& project = studio.getProject();
    const auto grid = event.mods.isCommandDown() && drag != Drag::markerNewCycle ? 0.0 : timeline.snapGrid (project);   // (Ctrl: free)
    const auto raw = juce::jmax (0.0, timeline.beatForX (event.position.x));
    if (drag == Drag::markerNewCycle)
    {
        auto& m = markerDragged[(size_t) markerDragIndex];
        const auto at = juce::jmax (0.0, pr7s::snap (raw, grid));
        m.start = juce::jmin (markerDownBeat, at);
        m.end = juce::jmax (markerDownBeat, at);
        dragMoved = m.end > m.start;
        repaint();
        return;
    }
    if (! juce::isPositiveAndBelow (markerDragIndex, (int) markerOriginal.size()))
        return;
    markerDragged = markerOriginal;
    const auto& grabbed = markerOriginal[(size_t) markerDragIndex];
    if (drag == Drag::markerStart || drag == Drag::markerEnd)
    {
        auto& m = markerDragged[(size_t) markerDragIndex];
        const auto at = juce::jmax (0.0, pr7s::snap (raw, grid));
        const auto least = juce::jmax (1.0 / 64.0, grid);
        if (drag == Drag::markerStart)
            m.start = juce::jmin (at, m.end - least);
        else
            m.end = juce::jmax (at, m.start + least);
        dragMoved = ! (m == grabbed);
        repaint();
        return;
    }
    // Moved: every selected marker by what the grabbed one moves (snapped where it lands).
    const auto landed = juce::jmax (0.0, pr7s::snap (grabbed.start + (raw - markerDownBeat), grid));
    auto delta = landed - grabbed.start;
    for (const auto i : markerSelection)
        if (juce::isPositiveAndBelow (i, (int) markerOriginal.size()))
            delta = juce::jmax (delta, -markerOriginal[(size_t) i].start);   // (none before the song's start)
    for (const auto i : markerSelection)
        if (juce::isPositiveAndBelow (i, (int) markerDragged.size()))
        {
            auto& m = markerDragged[(size_t) i];
            m.start += delta;
            m.end = m.cycle ? m.end + delta : m.start;
        }
    dragMoved = std::abs (delta) > 1.0e-9;
    repaint();
}

void ArrangeView::markerUp()
{
    const auto was = drag;
    drag = Drag::none;
    auto dragged = std::exchange (markerDragged, {});
    markerOriginal.clear();
    if (was == Drag::markerNewCycle)
    {
        if (dragMoved && juce::isPositiveAndBelow (markerDragIndex, (int) dragged.size()))
        {
            const auto& m = dragged[(size_t) markerDragIndex];
            const auto index = studio.addCycleMarker (m.start, m.end);
            if (index >= 0)
            {
                markerSelection = { index };
                if (markerNameAfter)
                    editMarkerName (index);
            }
        }
    }
    else if (dragMoved)
    {
        // The selection found again by kind and ID (the list is sorted again).
        std::vector<std::pair<int, bool>> chosen;
        for (const auto i : markerSelection)
            if (juce::isPositiveAndBelow (i, (int) dragged.size()))
                chosen.push_back ({ dragged[(size_t) i].id, dragged[(size_t) i].cycle });
        studio.setMarkers (std::move (dragged));
        markerSelection.clear();
        for (const auto& [id, cycle] : chosen)
            if (const auto i = studio.getMarkerTrack().indexOf (id, cycle); i >= 0)
                markerSelection.push_back (i);
    }
    markerDragIndex = -1;
    dragMoved = false;
    repaint();
}

bool ArrangeView::markerDoubleClick (const juce::MouseEvent& event)
{
    const auto row = rowAt (event.position.y);
    if (! row || row->kind != Timeline::Row::Kind::markers)
        return false;
    const auto hit = markerAt (event.position);
    if (hit < 0)
    {
        if (tool == Tool::range)
            selectRangeBetweenMarkers (timeline.beatForX (event.position.x));
        return true;
    }
    const auto& m = studio.getMarkerTrack().markers[(size_t) hit];
    if (m.cycle)
    {
        if (event.mods.isAltDown())
            zoomTo (m.start, m.end);   // (the manual: Alt + double-click zooms to it)
        studio.cycleToMarker (hit);
        return true;
    }
    editMarkerName (hit);
    return true;
}

bool ArrangeView::selectRangeBetweenMarkers (double beat)
{
    // The markers' edges either side of the click (a cycle marker's end counts as an edge too).
    std::optional<double> before, after;
    for (const auto& m : studio.getMarkerTrack().markers)
        for (const auto edge : { m.start, m.end })
        {
            if (edge <= beat && (! before || edge > *before))
                before = edge;
            if (edge > beat && (! after || edge < *after))
                after = edge;
        }
    if (! after)
        return false;
    pr7s::Studio::Range r;
    r.start = before.value_or (0.0);
    r.end = *after;
    r.trackIds = studio.getShownTracks();
    if (r.isEmpty())
        return false;
    setRange (r);
    return true;
}

void ArrangeView::zoomTo (double start, double end)
{
    const auto length = juce::jmax (1.0 / 16.0, end - start);
    timeline.pixelsPerBeat = juce::jlimit (2.0, 400.0, (double) juce::jmax (100, getWidth()) / (length * 1.1));
    timeline.firstBeat = juce::jmax (0.0, start - length * 0.05);
    scrolled();
    repaint();
}

void ArrangeView::deleteSelectedMarkers()
{
    if (markerSelection.empty())
        return;
    studio.removeMarkers (markerSelection);
    markerSelection.clear();
    repaint();
}

void ArrangeView::editMarkerName (int index)
{
    const auto& track = studio.getMarkerTrack();
    if (track.locked || ! juce::isPositiveAndBelow (index, (int) track.markers.size()))
        return;
    const auto marker = track.markers[(size_t) index];
    auto* box = new FamilyAlertWindow (marker.cycle ? "Cycle Marker " + pr7s::markerIdText (marker) : "Marker " + pr7s::markerIdText (marker),
                                       "Its description (shown on the marker track and in the Markers window).",
                                       juce::MessageBoxIconType::NoIcon);
    box->addTextEditor ("name", marker.name, "DESCRIPTION");
    box->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    box->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    box->enterModalState (true, juce::ModalCallbackFunction::create ([box, marker, safe = juce::Component::SafePointer<ArrangeView> (this)] (int result)
    {
        if (safe == nullptr || result != 1)
            return;
        auto& studio = safe->studio;
        auto markers = studio.getMarkerTrack().markers;
        const auto i = studio.getMarkerTrack().indexOf (marker.id, marker.cycle);
        if (i < 0)
            return;
        markers[(size_t) i].name = box->getTextEditorContents ("name").trim();
        studio.setMarkers (std::move (markers));
        safe->repaint();
    }), true);
}

void ArrangeView::showMarkerMenu (int index)
{
    const auto& track = studio.getMarkerTrack();
    const auto valid = juce::isPositiveAndBelow (index, (int) track.markers.size());
    const auto cycle = valid && track.markers[(size_t) index].cycle;
    juce::PopupMenu menu;
    menu.addSectionHeader ("MARKER TRACK");
    if (valid)
    {
        menu.addItem (1, "Locate (the playhead to it)");
        menu.addItem (2, "Set the Locators to This Cycle Marker", cycle);
        menu.addItem (3, "Zoom to This Cycle Marker", cycle);
        menu.addItem (4, "Description...", ! track.locked);
        menu.addItem (5, markerSelection.size() > 1 ? "Delete Them" : "Delete", ! track.locked);
        menu.addSeparator();
    }
    menu.addItem (10, "Add a Position Marker at the Cursor (Insert)", ! track.locked);
    menu.addItem (11, "Add a Cycle Marker at the Locators", ! track.locked);
    menu.addItem (12, "Markers Window... (Ctrl+M)", onOpenMarkers != nullptr);
    menu.addSeparator();
    menu.addItem (13, "Show Marker Lines", true, track.showLines);
    menu.addItem (14, "Show Marker IDs", true, track.showIds);
    menu.addItem (15, "Cycle Follows When Locating to Markers", true, track.cycleFollows);
    menu.addItem (16, "Lock", true, track.locked);
    menu.addSeparator();
    menu.addItem (17, "Remove the Marker Track (its markers stay)");
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<ArrangeView> (this), index] (int chosen)
    {
        if (safe == nullptr)
            return;
        auto& studio = safe->studio;
        const auto& t = studio.getMarkerTrack();
        switch (chosen)
        {
            case 1: studio.locateMarker (index); break;
            case 2: studio.cycleToMarker (index); break;
            case 3: if (juce::isPositiveAndBelow (index, (int) t.markers.size())) safe->zoomTo (t.markers[(size_t) index].start, t.markers[(size_t) index].end); break;
            case 4: safe->editMarkerName (index); break;
            case 5: safe->deleteSelectedMarkers(); break;
            case 10: if (const auto i = studio.addPositionMarkerAtCursor(); i >= 0) safe->markerSelection = { i }; break;
            case 11: if (const auto i = studio.addCycleMarkerAtLocators(); i >= 0) safe->markerSelection = { i }; break;
            case 12: if (safe->onOpenMarkers) safe->onOpenMarkers(); break;
            case 13: studio.setMarkerSettings (! t.showLines, t.showIds, t.cycleFollows); break;
            case 14: studio.setMarkerSettings (t.showLines, ! t.showIds, t.cycleFollows); break;
            case 15: studio.setMarkerSettings (t.showLines, t.showIds, ! t.cycleFollows); break;
            case 16: studio.setMarkerTrackLocked (! t.locked); break;
            case 17: studio.removeMarkerTrack(); break;
            default: return;
        }
        safe->repaint();
    });
}
