// Automation editing in the arrangement (0.27.0, the manual's Editing Automation Events): typed
// values, the Automation Event Editor's handles, bent ramps (Bézier curves), quick vertical
// scaling, the LINE tool's shapes.

#include "ArrangeView.h"

#include "ui/ParagonLookAndFeel.h"
#include "ui/TempoEditor.h"
#include "ui/ValueSlider.h"

#include <cmath>
#include <limits>

namespace
{
    constexpr float handleGrab = 6.0f, handleCorner = 10.0f, pointGrab = 6.0f;   // (pointGrab: as ArrangeView.cpp's)
    constexpr float scaleHandleHalfWidth = 10.0f;
}

juce::String ArrangeView::lineShapeName (LineShape shape)
{
    switch (shape)
    {
        case LineShape::line:     return "LINE";
        case LineShape::parabola: return "PARABOLA";
        case LineShape::sine:     return "SINE";
        case LineShape::triangle: return "TRIANGLE";
        case LineShape::square:   return "SQUARE";
    }
    return {};
}

void ArrangeView::setLineShape (LineShape shape)
{
    lineShape = shape;
    if (onToolChange)
        onToolChange();
}

void ArrangeView::showLineShapeMenu (juce::Component* target)
{
    juce::PopupMenu menu;
    for (const auto shape : { LineShape::line, LineShape::parabola, LineShape::sine, LineShape::triangle, LineShape::square })
        menu.addItem (lineShapeName (shape), true, shape == lineShape, [safe = juce::Component::SafePointer<ArrangeView> (this), shape]
        {
            if (safe != nullptr)
            {
                safe->setLineShape (shape);
                safe->setTool (Tool::line);
            }
        });
    auto options = juce::PopupMenu::Options();
    if (target != nullptr)
        options = options.withTargetComponent (target);
    menu.showMenuAsync (options);
}

void ArrangeView::selectPoints (int busId, int laneIndex, std::vector<int> points)
{
    std::sort (points.begin(), points.end());
    points.erase (std::unique (points.begin(), points.end()), points.end());
    const auto* lane = laneFor (busId, laneIndex);
    const auto count = lane != nullptr ? (int) editPointsOf (*lane).size() : 0;
    points.erase (std::remove_if (points.begin(), points.end(), [count] (int i) { return ! juce::isPositiveAndBelow (i, count); }), points.end());
    pointSelection = { busId, laneIndex, points };
    repaint();
}

bool ArrangeView::isTrimming (const pr7s::AutomationLane& lane) const
{
    return studio.getAutomationSettings().trim && lane.canTrim() && ! lane.points.empty();
}

//==============================================================================
std::optional<juce::String> ArrangeView::formatPointValue (int, const pr7s::AutomationLane& lane, float value) const
{
    using T = pr7s::AutomationLane::Target;
    if (isTrimming (lane))
    {
        // (0.28.0) A trim point: its dB up or down.
        const auto db = (value - 0.5f) * 2.0f * pr7s::AutomationLane::trimRange;
        return (db > 0.0f ? "+" : "") + juce::String (db, 1) + " dB TRIM";
    }
    switch (lane.target)
    {
        case T::volume:
        case T::send:
        {
            const auto db = pr7s::travelToVolume (value);
            return db <= -59.95f ? juce::String ("-inf dB") : juce::String (db, 1) + " dB";
        }
        case T::pan:
        {
            const auto pan = juce::roundToInt (pr7s::travelToPan (value) * 100.0f);
            return pan == 0 ? juce::String ("C") : pan < 0 ? "L" + juce::String (-pan) : "R" + juce::String (pan);
        }
        case T::mute:
            return value >= 0.5f ? juce::String ("ON") : juce::String ("OFF");
        case T::instrument:
        case T::insert:
            return juce::String (value * 100.0f, 1) + " %";
    }
    return std::nullopt;
}

std::optional<float> ArrangeView::parsePointValue (int, const pr7s::AutomationLane& lane, const juce::String& typed) const
{
    using T = pr7s::AutomationLane::Target;
    const auto text = typed.trim();
    if (text.isEmpty())
        return std::nullopt;
    if (isTrimming (lane))
    {
        if (const auto db = ValueEntry::parseDb (text.toLowerCase().replace ("trim", ""), -pr7s::AutomationLane::trimRange, pr7s::AutomationLane::trimRange))
            return 0.5f + (float) *db / (2.0f * pr7s::AutomationLane::trimRange);
        return std::nullopt;
    }
    switch (lane.target)
    {
        case T::volume:
        case T::send:
            if (const auto db = ValueEntry::parseDb (text))
                return pr7s::volumeToTravel ((float) *db);
            return std::nullopt;
        case T::pan:
            if (const auto pan = ValueEntry::parsePan (text))
                return pr7s::panToTravel ((float) *pan);
            return std::nullopt;
        case T::mute:
        {
            const auto t = text.toUpperCase();
            if (t == "ON" || t == "1" || t == "MUTE" || t == "MUTED")
                return 1.0f;
            if (t == "OFF" || t == "0")
                return 0.0f;
            return std::nullopt;
        }
        case T::instrument:
        case T::insert:
        {
            const auto number = text.removeCharacters ("% ").replaceCharacter (',', '.');
            if (number.isEmpty() || ! number.containsOnly ("+-.0123456789") || ! number.containsAnyOf ("0123456789"))
                return std::nullopt;
            return juce::jlimit (0.0f, 1.0f, (float) number.getDoubleValue() / 100.0f);
        }
    }
    return std::nullopt;
}

bool ArrangeView::enterPointValue (const juce::String& valueText, const juce::String& positionText)
{
    const auto* lane = laneFor (pointSelection.busId, pointSelection.laneIndex);
    if (lane == nullptr || pointSelection.points.empty())
        return false;
    std::optional<float> value;   // (Trim on: the trim's dB)
    if (valueText.trim().isNotEmpty())
    {
        value = parsePointValue (pointSelection.busId, *lane, valueText);
        if (! value)
            return false;
    }
    std::optional<double> position;
    if (pointSelection.points.size() == 1 && positionText.trim().isNotEmpty())
    {
        position = TempoEditor::parsePosition (positionText, studio.getProject());
        if (! position || *position < 0.0)
            return false;
    }
    auto points = editPointsOf (*lane);
    std::vector<pr7s::AutomationPoint> chosen;
    for (const auto i : pointSelection.points)
        if (juce::isPositiveAndBelow (i, (int) points.size()))
        {
            if (value)
                points[(size_t) i].value = *value;
            if (position)
                points[(size_t) i].beat = *position;
            chosen.push_back (points[(size_t) i]);
        }
    commitPoints (std::move (points), chosen);
    return true;
}

void ArrangeView::showPointEntry (int busId, int laneIndex, int point)
{
    const auto* lane = laneFor (busId, laneIndex);
    if (lane == nullptr || ! juce::isPositiveAndBelow (point, (int) editPointsOf (*lane).size()))
        return;
    if (pointSelection.busId != busId || pointSelection.laneIndex != laneIndex
        || ! std::binary_search (pointSelection.points.begin(), pointSelection.points.end(), point))
        selectPoints (busId, laneIndex, { point });

    // A box at the point: VALUE (as its control reads), AT (one point); Enter or SET, Escape leaves it.
    struct Entry final : juce::Component
    {
        Entry (ArrangeView& v, const juce::String& title, const juce::String& value, const juce::String& at) : view (&v)
        {
            heading.setText (title, juce::dontSendNotification);
            heading.setFont (ParagonLookAndFeel::titleFont (13.0f));
            heading.setColour (juce::Label::textColourId, ParagonColours::accent);
            addAndMakeVisible (heading);
            for (auto* label : { &valueLabel, &atLabel })
            {
                label->setFont (ParagonLookAndFeel::textFont (12.0f));
                label->setColour (juce::Label::textColourId, ParagonColours::textDim);
                addAndMakeVisible (*label);
            }
            for (auto* field : { &valueField, &atField })
            {
                field->setSelectAllWhenFocused (true);
                field->onReturnKey = [this] { apply(); };
                field->onEscapeKey = [this] { close(); };
                addAndMakeVisible (*field);
            }
            valueField.setText (value, false);
            atField.setText (at, false);
            atField.setEnabled (at.isNotEmpty());
            setKey.setButtonText ("SET");
            setKey.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
            setKey.onClick = [this] { apply(); };
            addAndMakeVisible (setKey);
            setSize (240, 124);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            heading.setBounds (r.removeFromTop (20));
            r.removeFromTop (4);
            auto row = r.removeFromTop (26);
            valueLabel.setBounds (row.removeFromLeft (50));
            valueField.setBounds (row);
            r.removeFromTop (4);
            row = r.removeFromTop (26);
            atLabel.setBounds (row.removeFromLeft (50));
            atField.setBounds (row);
            r.removeFromTop (6);
            setKey.setBounds (r.removeFromTop (24));
        }
        void parentHierarchyChanged() override
        {
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<juce::TextEditor> (&valueField)]
            {
                if (safe != nullptr && safe->isShowing())
                    safe->grabKeyboardFocus();
            });
        }
        void apply()
        {
            if (view == nullptr)
                return;
            if (! view->enterPointValue (valueField.getText(), atField.isEnabled() ? atField.getText() : juce::String()))
            {
                heading.setText ("NOT A VALUE IT TAKES", juce::dontSendNotification);
                valueField.selectAll();
                return;
            }
            close();
        }
        void close()
        {
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
            {
                box->exitModalState (0);
                box->setVisible (false);
            }
        }
        juce::Component::SafePointer<ArrangeView> view;
        juce::Label heading, valueLabel { {}, "VALUE" }, atLabel { {}, "AT" };
        juce::TextEditor valueField, atField;
        juce::TextButton setKey;
    };
    const auto& p = editPointsOf (*lane)[(size_t) point];
    const auto count = (int) pointSelection.points.size();
    const auto title = studio.getLaneName (busId, *lane).toUpperCase() + (count > 1 ? "  (" + juce::String (count) + " POINTS)" : juce::String());
    const auto at = count == 1 ? pr7s::formatBarsBeats (p.beat, studio.getProject()).replace ("  ", " ") : juce::String();
    const auto where = getPointPosition (busId, laneIndex, point).toInt();
    juce::CallOutBox::launchAsynchronously (std::make_unique<Entry> (*this, title, formatPointValue (busId, *lane, p.value).value_or (""), at),
                                            localAreaToGlobal (juce::Rectangle<int> (where.x - 4, where.y - 4, 8, 8)), nullptr);
}

//==============================================================================
void ArrangeView::setPointBoxShown (bool shown)
{
    pointBoxShown = shown;
    if (auto* settings = studio.getSettings())
        settings->setValue ("automationEditBox", shown);
    repaint();
}

juce::Rectangle<float> ArrangeView::getPointBox() const
{
    const auto* lane = laneFor (pointSelection.busId, pointSelection.laneIndex);
    if (! pointBoxShown || lane == nullptr || lane->isStep() || pointSelection.points.size() < 2)
        return {};
    const auto area = getLaneArea (pointSelection.busId, pointSelection.laneIndex);
    const auto& points = (drag == Drag::pointHandle) ? workingPoints : editPointsOf (*lane);
    auto left = std::numeric_limits<float>::max(), right = std::numeric_limits<float>::lowest(), top = area.getBottom() - 8.0f;
    for (const auto i : pointSelection.points)
        if (juce::isPositiveAndBelow (i, (int) points.size()))
        {
            const auto x = timeline.xForBeat (points[(size_t) i].beat);
            left = juce::jmin (left, x);
            right = juce::jmax (right, x);
            top = juce::jmin (top, yForValue (area, points[(size_t) i].value));
        }
    if (right < left)
        return {};
    left -= 6.0f;
    right += 6.0f;
    top = juce::jmax (area.getY() + 1.0f, top - 4.0f);
    return { left, top, right - left, area.getBottom() - 1.0f - top };
}

ArrangeView::PointHandle ArrangeView::pointHandleAt (juce::Point<float> p, bool alt) const
{
    const auto box = getPointBox();
    if (box.isEmpty())
        return PointHandle::none;
    if (std::abs (p.x - box.getRight()) <= handleGrab && std::abs (p.y - box.getCentreY()) <= handleCorner)
        return alt ? PointHandle::scaleRelative : PointHandle::scaleAbsolute;
    if (std::abs (p.y - box.getBottom()) <= handleGrab && p.x >= box.getX() && p.x <= box.getRight())
        return PointHandle::stretch;
    if (std::abs (p.y - box.getY()) > handleGrab || p.x < box.getX() - handleGrab || p.x > box.getRight() + handleGrab)
        return PointHandle::none;
    if (p.x <= box.getX() + handleCorner)
        return alt ? PointHandle::compressLeft : PointHandle::tiltLeft;
    if (p.x >= box.getRight() - handleCorner)
        return alt ? PointHandle::compressRight : PointHandle::tiltRight;
    if (std::abs (p.x - box.getCentreX()) <= handleCorner)
        return PointHandle::scale;
    return PointHandle::move;
}

void ArrangeView::dragPointHandle (PointHandle h, juce::Point<float> offset)
{
    // From the points as the drag began (`originalPoints`), so a drag can go back.
    const auto* lane = laneFor (pointSelection.busId, pointSelection.laneIndex);
    if (lane == nullptr || h == PointHandle::none)
        return;
    if (boxAtDown.isEmpty())
        boxAtDown = getPointBox();
    if (drag != Drag::pointHandle || selectionAtDown.empty())
    {
        // (Called directly: from the lane as it is.)
        originalPoints = editPointsOf (*lane);
        selectionAtDown = pointSelection.points;
    }
    const auto& chosen = selectionAtDown;   // (indices into originalPoints: the selection as the drag began)
    const auto area = getLaneArea (pointSelection.busId, pointSelection.laneIndex);
    const auto delta = -offset.y / juce::jmax (1.0f, area.getHeight());
    auto highest = 0.0f, lowest = 1.0f;
    for (const auto i : chosen)
        if (juce::isPositiveAndBelow (i, (int) originalPoints.size()))
            highest = juce::jmax (highest, originalPoints[(size_t) i].value), lowest = juce::jmin (lowest, originalPoints[(size_t) i].value);
    const auto middle = (highest + lowest) * 0.5f;
    const auto leftBeat = timeline.beatForX (boxAtDown.getX() + 6.0f);
    const auto width = juce::jmax (1.0f, boxAtDown.getWidth() - 12.0f);
    std::vector<std::pair<pr7s::AutomationPoint, bool>> paired;
    for (int i = 0; i < (int) originalPoints.size(); ++i)
        paired.push_back ({ originalPoints[(size_t) i], std::binary_search (chosen.begin(), chosen.end(), i) });
    for (auto& [p, isSelected] : paired)
    {
        if (! isSelected)
            continue;
        const auto x = timeline.xForBeat (p.beat);
        const auto t = juce::jlimit (0.0f, 1.0f, (x - (boxAtDown.getX() + 6.0f)) / width);
        const auto was = p.value;
        switch (h)
        {
            case PointHandle::tiltLeft:      p.value = was + delta * (1.0f - t); break;
            case PointHandle::tiltRight:     p.value = was + delta * t; break;
            case PointHandle::compressLeft:  p.value = middle + (was - middle) * juce::jmax (0.0f, 1.0f + 2.0f * delta * (1.0f - t)); break;
            case PointHandle::compressRight: p.value = middle + (was - middle) * juce::jmax (0.0f, 1.0f + 2.0f * delta * t); break;
            case PointHandle::scale:         p.value = highest > 1.0e-6f ? was * juce::jmax (0.0f, (highest + delta) / highest) : was; break;
            case PointHandle::move:          p.value = was + delta; break;
            case PointHandle::scaleAbsolute: p.value = 0.5f + (was - 0.5f) * juce::jmax (0.0f, 1.0f + 2.0f * delta); break;
            case PointHandle::scaleRelative: p.value = middle + (was - middle) * juce::jmax (0.0f, 1.0f + 2.0f * delta); break;
            case PointHandle::stretch:
                p.beat = juce::jmax (0.0, leftBeat + (p.beat - leftBeat) * juce::jmax (0.0, (double) (width + offset.x) / width));
                break;
            case PointHandle::none: break;
        }
        p.value = juce::jlimit (0.0f, 1.0f, p.value);
    }
    std::stable_sort (paired.begin(), paired.end(), [] (const auto& a, const auto& b) { return a.first.beat < b.first.beat; });
    workingPoints.clear();
    pointSelection.points.clear();
    for (int i = 0; i < (int) paired.size(); ++i)
    {
        workingPoints.push_back (paired[(size_t) i].first);
        if (paired[(size_t) i].second)
            pointSelection.points.push_back (i);
    }
    dragMoved = true;
    repaint (area.expanded (2.0f, 6.0f).toNearestIntEdges());
}

//==============================================================================
juce::Point<float> ArrangeView::getCurveHandle (int busId, int laneIndex, int point) const
{
    const auto* lane = laneFor (busId, laneIndex);
    if (lane == nullptr || point < 1 || point >= (int) editPointsOf (*lane).size())
        return {};
    const auto area = getLaneArea (busId, laneIndex);
    const auto& a = editPointsOf (*lane)[(size_t) point - 1];
    const auto& b = editPointsOf (*lane)[(size_t) point];
    const auto x = (timeline.xForBeat (a.beat) + timeline.xForBeat (b.beat)) * 0.5f;
    const auto value = a.value + (b.value - a.value) * pr7s::AutomationLane::curveShape (0.5f, b.curve);
    return { x, yForValue (area, value) };
}

int ArrangeView::curveHandleAt (int busId, int laneIndex, juce::Point<float> p) const
{
    // A ramp's segments wide and steep enough (the manual: none on a nearly flat one, or a short one).
    const auto* lane = laneFor (busId, laneIndex);
    if (lane == nullptr || lane->isStep() || tool != Tool::select)
        return -1;
    const auto area = getLaneArea (busId, laneIndex);
    for (int i = 1; i < (int) editPointsOf (*lane).size(); ++i)
    {
        const auto& a = editPointsOf (*lane)[(size_t) i - 1];
        const auto& b = editPointsOf (*lane)[(size_t) i];
        const auto x1 = timeline.xForBeat (a.beat), x2 = timeline.xForBeat (b.beat);
        if (x2 - x1 < 24.0f)
            continue;
        // The quick vertical scaling handle: the lane's top edge, at the segment's middle. (0.37.0: a
        // handle 20 px wide; it was the segment's whole middle half, so a box dragged from above a
        // curve near the top (-3 dB sits there) scaled it instead of selecting, the owner found.)
        if (p.y <= area.getY() + 6.0f && p.y >= area.getY() - 2.0f && std::abs (p.x - (x1 + x2) * 0.5f) <= scaleHandleHalfWidth)
            return i;
        if (std::abs (b.value - a.value) < 0.02f)
            continue;
        if (getCurveHandle (busId, laneIndex, i).getDistanceFrom (p) <= handleGrab)
            return i;
    }
    return -1;
}

void ArrangeView::bendSegment (int busId, int laneIndex, int point, float towardsY)
{
    const auto* lane = laneFor (busId, laneIndex);
    if (lane == nullptr || point < 1 || point >= (int) editPointsOf (*lane).size())
        return;
    if (drag != Drag::curve)
        originalPoints = editPointsOf (*lane);
    workingPoints = originalPoints;
    const auto area = getLaneArea (busId, laneIndex);
    const auto a = workingPoints[(size_t) point - 1].value, b = workingPoints[(size_t) point].value;
    if (std::abs (b - a) < 1.0e-4f)
        return;
    // Where the middle should be, as a share of the way from a to b; the bend that puts it there.
    const auto share = juce::jlimit (0.03f, 0.97f, (valueForY (area, towardsY) - a) / (b - a));
    const auto exponent = std::log (share) / std::log (0.5f);
    workingPoints[(size_t) point].curve = juce::jlimit (-1.0f, 1.0f, -std::log (exponent) / std::log (4.0f));
    if (drag != Drag::curve)
    {
        // (Not dragging: as one edit.)
        pointSelection = { busId, laneIndex, pointSelection.busId == busId && pointSelection.laneIndex == laneIndex ? pointSelection.points : std::vector<int>() };
        commitPoints (workingPoints, {});
        return;
    }
    repaint (area.expanded (2.0f, 6.0f).toNearestIntEdges());
}

//==============================================================================
float ArrangeView::lineValueAt (double t, float v1, float v2, double beatsInto, bool reverse) const
{
    const auto period = juce::jmax (1.0 / 64.0, timeline.grid (studio.getProject()));
    const auto phase = std::fmod (beatsInto / period + (reverse ? 0.5 : 0.0), 1.0);
    const auto amplitude = (double) (v2 - v1);
    switch (lineShape)
    {
        case LineShape::line:     return (float) (v1 + (v2 - v1) * t);
        case LineShape::parabola: return (float) (reverse ? v1 + (v2 - v1) * (1.0 - (1.0 - t) * (1.0 - t)) : v1 + (v2 - v1) * t * t);
        case LineShape::sine:     return (float) (v1 + amplitude * std::sin (phase * juce::MathConstants<double>::twoPi));
        case LineShape::triangle: return (float) (v1 + amplitude * (phase < 0.25 ? phase * 4.0 : phase < 0.75 ? 2.0 - phase * 4.0 : phase * 4.0 - 4.0));
        case LineShape::square:   return (float) (v1 + amplitude * (phase < 0.5 ? 1.0 : -1.0));
    }
    return v1;
}

void ArrangeView::drawLineShape (int busId, int laneIndex, juce::Point<float> from, juce::Point<float> to, bool reverse)
{
    // The LINE tool (the manual's Line, Parabola, Sine, Triangle, Square; ramp lanes only).
    const auto* lane = laneFor (busId, laneIndex);
    if (lane == nullptr || lane->isStep())
        return;
    const auto area = getLaneArea (busId, laneIndex);
    const auto periodic = lineShape == LineShape::sine || lineShape == LineShape::triangle || lineShape == LineShape::square;
    if (! periodic && from.x > to.x)
        std::swap (from, to);
    const auto grid = juce::jmax (1.0 / 64.0, timeline.grid (studio.getProject()));
    const auto b1 = juce::jmax (0.0, pr7s::snap (timeline.beatForX (juce::jmin (from.x, to.x)), grid));
    const auto b2 = juce::jmax (0.0, pr7s::snap (timeline.beatForX (juce::jmax (from.x, to.x)), grid));
    if (b2 <= b1)
        return;
    const auto v1 = valueForY (area, from.y), v2 = valueForY (area, to.y);
    std::vector<pr7s::AutomationPoint> points;
    for (const auto& p : editPointsOf (*lane))
        if (p.beat < b1 - 1.0e-9 || p.beat > b2 + 1.0e-9)
            points.push_back (p);
    std::vector<pr7s::AutomationPoint> added;
    if (lineShape == LineShape::line)
        added = { { b1, v1, 0.0f }, { b2, v2, 0.0f } };
    else if (lineShape == LineShape::parabola)
        added = { { b1, v1, 0.0f }, { b2, v2, reverse ? 0.5f : -0.5f } };   // (t^2 or its mirror: one bent ramp)
    else
    {
        // A point a sixteenth of a period apart (the square: its jumps as two points at one beat).
        const auto step = grid / 16.0;
        const auto origin = timeline.beatForX (from.x);
        auto last = std::numeric_limits<float>::quiet_NaN();
        for (auto beat = b1; beat <= b2 + 1.0e-9; beat += step)
        {
            const auto value = juce::jlimit (0.0f, 1.0f, lineValueAt (0.0, v1, v2, std::abs (beat - origin), reverse));
            if (lineShape == LineShape::square && ! std::isnan (last) && std::abs (value - last) > 1.0e-4f)
                added.push_back ({ beat, last, 0.0f });
            if (lineShape != LineShape::square || std::isnan (last) || std::abs (value - last) > 1.0e-4f || beat + step > b2 + 1.0e-9)
                added.push_back ({ beat, value, 0.0f });
            last = value;
        }
    }
    for (auto& p : added)
        p.value = juce::jlimit (0.0f, 1.0f, p.value);
    points.insert (points.end(), added.begin(), added.end());
    pointSelection = { busId, laneIndex, {} };
    commitPoints (std::move (points), {});
}

//==============================================================================
bool ArrangeView::automationDoubleClick (const juce::MouseEvent& event)
{
    const auto row = rowAt (event.position.y);
    if (! row || row->kind != Timeline::Row::Kind::lane)
        return false;
    const auto* lane = laneFor (row->busId, row->laneIndex);
    if (lane == nullptr)
        return false;
    // A bend's handle: straightened (the manual's).
    if (const auto c = curveHandleAt (row->busId, row->laneIndex, event.position); c >= 0 && event.position.y > getLaneArea (row->busId, row->laneIndex).getY() + 6.0f)
    {
        auto points = editPointsOf (*lane);
        points[(size_t) c].curve = 0.0f;
        pointSelection = { row->busId, row->laneIndex, {} };
        commitPoints (std::move (points), {});
        return true;
    }
    for (int i = (int) editPointsOf (*lane).size() - 1; i >= 0; --i)
        if (getPointPosition (row->busId, row->laneIndex, i).getDistanceFrom (event.position) <= pointGrab)
        {
            showPointEntry (row->busId, row->laneIndex, i);
            return true;
        }
    return false;
}

void ArrangeView::paintPointHandles (juce::Graphics& g, const Timeline::Row& row) const
{
    // The bend handle under the mouse.
    if (hoverCurve.point >= 0 && hoverCurve.busId == row.busId && hoverCurve.laneIndex == row.laneIndex && drag == Drag::none)
    {
        const auto area = getLaneArea (row.busId, row.laneIndex);
        const auto* lane = laneFor (row.busId, row.laneIndex);
        if (lane != nullptr && hoverCurve.point < (int) editPointsOf (*lane).size())
        {
            const auto x = (timeline.xForBeat (editPointsOf (*lane)[(size_t) hoverCurve.point - 1].beat) + timeline.xForBeat (editPointsOf (*lane)[(size_t) hoverCurve.point].beat)) * 0.5f;
            const auto handle = getCurveHandle (row.busId, row.laneIndex, hoverCurve.point);
            g.setColour (ParagonColours::accent);
            g.drawEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (handle), 1.5f);
            // The quick scaling handle at the top edge.
            g.fillRect (juce::Rectangle<float> (x - 8.0f, area.getY(), 16.0f, 3.0f));
        }
    }
    // The event editor's box and handles (two or more selected here).
    if (pointSelection.busId != row.busId || pointSelection.laneIndex != row.laneIndex)
        return;
    if (drag == Drag::linePoints)
    {
        // The LINE tool's line, as dragged.
        g.setColour (juce::Colour (0xfff4eeea));
        g.drawLine (juce::Line<float> (pointDownAt, lineEnd), 1.6f);
        return;
    }
    const auto box = getPointBox();
    if (box.isEmpty())
        return;
    g.setColour (ParagonColours::accent.withAlpha (0.08f));
    g.fillRect (box);
    g.setColour (ParagonColours::accent.withAlpha (0.8f));
    g.drawRect (box, 1.0f);
    for (const auto c : { box.getTopLeft(), juce::Point<float> (box.getCentreX(), box.getY()), box.getTopRight(),
                          juce::Point<float> (box.getRight(), box.getCentreY()), juce::Point<float> (box.getCentreX(), box.getBottom()) })
    {
        const auto r = juce::Rectangle<float> (7.0f, 7.0f).withCentre (c);
        g.setColour (juce::Colour (0xff0b090c));
        g.fillRect (r);
        g.setColour (ParagonColours::accent);
        g.drawRect (r, 1.2f);
    }
}

void ArrangeView::showAutomationFunctions (int busId, int laneIndex)
{
    showLaneMenu (busId, laneIndex);
}

//==============================================================================
// The info line (0.37.0, the manual's Info Line and its Value Editing Rules): the selected points
// shown as the first one, and changed together, relative to their own values (Ctrl: all to it).
std::optional<double> ArrangeView::pointDisplay (const pr7s::AutomationLane& lane, float value) const
{
    using T = pr7s::AutomationLane::Target;
    if (isTrimming (lane))
        return (value - 0.5f) * 2.0f * pr7s::AutomationLane::trimRange;
    switch (lane.target)
    {
        case T::volume:
        case T::send:       return juce::jmax (-60.0f, pr7s::travelToVolume (value));
        case T::pan:        return pr7s::travelToPan (value) * 100.0f;
        case T::mute:       return value >= 0.5f ? 1.0 : 0.0;
        case T::instrument:
        case T::insert:     return value * 100.0f;
    }
    return std::nullopt;
}

float ArrangeView::pointFromDisplay (const pr7s::AutomationLane& lane, double display) const
{
    using T = pr7s::AutomationLane::Target;
    if (isTrimming (lane))
        return juce::jlimit (0.0f, 1.0f, 0.5f + (float) display / (2.0f * pr7s::AutomationLane::trimRange));
    switch (lane.target)
    {
        case T::volume:
        case T::send:       return juce::jlimit (0.0f, 1.0f, pr7s::volumeToTravel ((float) juce::jlimit (-60.0, 6.0, display)));
        case T::pan:        return juce::jlimit (0.0f, 1.0f, pr7s::panToTravel ((float) juce::jlimit (-100.0, 100.0, display) / 100.0f));
        case T::mute:       return display >= 0.5 ? 1.0f : 0.0f;
        case T::instrument:
        case T::insert:     return juce::jlimit (0.0f, 1.0f, (float) display / 100.0f);
    }
    return 0.0f;
}

bool ArrangeView::setSelectedPointsValue (const juce::String& text, bool absolute)
{
    const auto* lane = laneFor (pointSelection.busId, pointSelection.laneIndex);
    if (lane == nullptr || pointSelection.points.empty())
        return false;
    const auto typed = parsePointValue (pointSelection.busId, *lane, text);
    if (! typed)
        return false;
    auto points = editPointsOf (*lane);
    const auto first = pointSelection.points.front();
    if (! juce::isPositiveAndBelow (first, (int) points.size()))
        return false;
    // Relative: each moves by what the first moves, in the control's own units (dB, pan, %).
    const auto from = pointDisplay (*lane, points[(size_t) first].value);
    const auto to = pointDisplay (*lane, *typed);
    if (! from || ! to)
        return false;
    const auto delta = *to - *from;
    std::vector<pr7s::AutomationPoint> chosen;
    for (const auto i : pointSelection.points)
        if (juce::isPositiveAndBelow (i, (int) points.size()))
        {
            auto& p = points[(size_t) i];
            if (absolute)
                p.value = *typed;
            else if (const auto own = pointDisplay (*lane, p.value))
                p.value = pointFromDisplay (*lane, *own + delta);
            chosen.push_back (p);
        }
    commitPoints (std::move (points), chosen);
    return true;
}

bool ArrangeView::setSelectedPointsStart (const juce::String& text, bool absolute)
{
    const auto* lane = laneFor (pointSelection.busId, pointSelection.laneIndex);
    if (lane == nullptr || pointSelection.points.empty())
        return false;
    const auto typed = TempoEditor::parsePosition (text, studio.getProject());
    if (! typed || *typed < 0.0)
        return false;
    auto points = editPointsOf (*lane);
    const auto first = pointSelection.points.front();
    if (! juce::isPositiveAndBelow (first, (int) points.size()))
        return false;
    const auto delta = *typed - points[(size_t) first].beat;
    std::vector<pr7s::AutomationPoint> chosen;
    for (const auto i : pointSelection.points)
        if (juce::isPositiveAndBelow (i, (int) points.size()))
        {
            auto& p = points[(size_t) i];
            p.beat = absolute ? *typed : juce::jmax (0.0, p.beat + delta);
            chosen.push_back (p);
        }
    std::stable_sort (points.begin(), points.end(), [] (const auto& a, const auto& b) { return a.beat < b.beat; });
    commitPoints (std::move (points), chosen);
    return true;
}

void ArrangeView::setSelectedPointsTerminator (bool on)
{
    const auto* lane = laneFor (pointSelection.busId, pointSelection.laneIndex);
    if (lane == nullptr || pointSelection.points.empty() || isTrimming (*lane))
        return;
    auto points = editPointsOf (*lane);
    std::vector<pr7s::AutomationPoint> chosen;
    for (const auto i : pointSelection.points)
        if (juce::isPositiveAndBelow (i, (int) points.size()))
        {
            points[(size_t) i].terminator = on;
            chosen.push_back (points[(size_t) i]);
        }
    commitPoints (std::move (points), chosen);
}
