// The MIDI editor's controller lanes (0.25.0, the manual's Controller Display): what each lane
// shows, its menus, drawing, selecting, moving and typing controller events, the LINE tool.
// (The velocity lane's own drawing and editing stay in KeyEditor.cpp; the line tool and the
// lane's header are here.)

#include "KeyEditor.h"

#include "engine/Controllers.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/TempoEditor.h"
#include "ui/ValueSlider.h"

#include <limits>

namespace K = pr7s::ControllerKinds;

namespace
{
    juce::FontOptions font (float height, int style = juce::Font::bold) { return ParagonLookAndFeel::textFont (height, style); }
    constexpr float pointGrab = 6.0f;
    constexpr double finestStep = 1.0 / 64.0;    // drawing without the grid (Ctrl)
}

//==============================================================================
juce::String KeyEditor::lineModeName (LineMode mode)
{
    switch (mode)
    {
        case LineMode::line:     return "LINE";
        case LineMode::parabola: return "PARABOLA";
        case LineMode::sine:     return "SINE";
        case LineMode::triangle: return "TRIANGLE";
        case LineMode::square:   return "SQUARE";
        case LineMode::paint:    return "PAINT";
    }
    return {};
}

void KeyEditor::setLineMode (LineMode mode)
{
    lineMode = mode;
    lineTool.setButtonText (mode == LineMode::line ? juce::String ("LINE") : "LINE: " + lineModeName (mode));
    if (auto* settings = studio.getSettings())
        settings->setValue ("midiEditorLineMode", (int) mode);
}

void KeyEditor::showLineModeMenu()
{
    juce::PopupMenu menu;
    for (const auto mode : { LineMode::line, LineMode::parabola, LineMode::sine, LineMode::triangle, LineMode::square, LineMode::paint })
        menu.addItem (lineModeName (mode), true, mode == lineMode, [this, mode] { setLineMode (mode); setTool (Tool::line); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (lineTool));
}

const std::vector<int>& KeyEditor::getLanes() const
{
    static const std::vector<int> fallback { K::velocity };
    if (const auto* track = studio.getProject().findTrack (getTrackId()))
        return track->controllerLanes;
    return fallback;
}

float KeyEditor::laneHeightNow() const
{
    const auto count = (int) getLanes().size();
    if (count == 0)
        return 0.0f;
    // As tall as set, but the notes keep some room.
    const auto room = ((float) getHeight() - notesTop() - (float) (scrollBarSize + 60)) / (float) count;
    return juce::jmax (24.0f, juce::jmin ((float) laneHeight, room));
}

float KeyEditor::lanesTop() const
{
    // (0.77.4) The lanes reach the bottom; the scroll bar across sits over them, under the notes.
    return (float) getHeight() - laneHeightNow() * (float) getLanes().size();
}

float KeyEditor::acrossTop() const
{
    return lanesTop() - (float) scrollBarSize;
}

juce::Rectangle<float> KeyEditor::getLaneArea (int lane) const
{
    if (! juce::isPositiveAndBelow (lane, getNumLanes()))
        return {};
    const auto h = laneHeightNow();
    return { (float) (sideWidth + listWidth()), lanesTop() + h * (float) lane, (float) (getWidth() - sideWidth - listWidth()), h };
}

juce::Rectangle<float> KeyEditor::getLaneHeader (int lane) const
{
    const auto area = getLaneArea (lane);
    return area.isEmpty() ? area : area.withX ((float) sideWidth).withWidth ((float) listWidth());
}

int KeyEditor::laneAt (juce::Point<float> p) const
{
    for (int i = 0; i < getNumLanes(); ++i)
        if (getLaneArea (i).contains (p) || getLaneHeader (i).contains (p))
            return i;
    return -1;
}

int KeyEditor::laneOf (int code) const
{
    const auto& lanes = getLanes();
    const auto it = std::find (lanes.begin(), lanes.end(), code);
    return it != lanes.end() ? (int) std::distance (lanes.begin(), it) : -1;
}

void KeyEditor::setLane (int lane, int code)
{
    auto lanes = getLanes();
    if (! juce::isPositiveAndBelow (lane, (int) lanes.size()) || laneOf (code) >= 0 || ! K::isValid (code) || getTrackId() == 0)
        return;
    lanes[(size_t) lane] = code;
    selectedControllers.clear();
    studio.setControllerLanes (getTrackId(), lanes);
    resized();
    repaint();
}

void KeyEditor::addLane (int code)
{
    if (laneOf (code) >= 0 || ! K::isValid (code) || getTrackId() == 0)
        return;
    auto lanes = getLanes();
    lanes.push_back (code);
    studio.setControllerLanes (getTrackId(), lanes);
    resized();
    repaint();
}

void KeyEditor::removeLane (int lane)
{
    auto lanes = getLanes();
    if (! juce::isPositiveAndBelow (lane, (int) lanes.size()) || getTrackId() == 0)
        return;
    hiddenLanes = lanes;
    lanes.erase (lanes.begin() + lane);
    if (activeLane == lane)
        activeLane = -1, selectedControllers.clear();
    studio.setControllerLanes (getTrackId(), lanes);
    resized();
    repaint();
}

void KeyEditor::showUsedControllers()
{
    // "Opens all controller lanes that have controller events" (velocity stays where it is).
    const auto* part = getPart();
    if (part == nullptr)
        return;
    auto lanes = getLanes();
    for (const auto code : K::usedCodes (*part))
        if (std::find (lanes.begin(), lanes.end(), code) == lanes.end())
            lanes.push_back (code);
    studio.setControllerLanes (getTrackId(), lanes);
    resized();
    repaint();
}

void KeyEditor::showVelocityLaneOnly()
{
    if (getTrackId() == 0)
        return;
    selectedControllers.clear();
    activeLane = -1;
    studio.setControllerLanes (getTrackId(), { K::velocity });
    resized();
    repaint();
}

void KeyEditor::toggleLanesShown()
{
    if (getTrackId() == 0)
        return;
    if (! getLanes().empty())
    {
        hiddenLanes = getLanes();
        selectedControllers.clear();
        studio.setControllerLanes (getTrackId(), {});
    }
    else
    {
        studio.setControllerLanes (getTrackId(), hiddenLanes.empty() ? std::vector<int> { K::velocity } : hiddenLanes);
    }
    resized();
    repaint();
}

void KeyEditor::setLaneHeight (int height)
{
    laneHeight = juce::jlimit (40, 400, height);
    if (auto* settings = studio.getSettings())
        settings->setValue ("midiEditorLaneHeight", laneHeight);
    resized();
    repaint();
}

//==============================================================================
float KeyEditor::yForValue (int lane, int value) const
{
    const auto inner = getLaneArea (lane).reduced (0.0f, 6.0f);
    const auto code = juce::isPositiveAndBelow (lane, getNumLanes()) ? getLanes()[(size_t) lane] : 0;
    return inner.getBottom() - inner.getHeight() * (float) value / (float) K::maxValue (code);
}

int KeyEditor::snapBend (int value) const
{
    const auto* track = studio.getProject().findTrack (getTrackId());
    if (track == nullptr)
        return value;
    // To the nearest semitone of the grid (up: 8191 over `up`, down: 8192 over `down`).
    const auto up = juce::jmax (1, track->pitchbendUp), downward = juce::jmax (1, track->pitchbendDown);
    if (value >= 8192)
        return juce::jlimit (0, 16383, 8192 + juce::roundToInt (std::round ((value - 8192) * up / 8191.0) * 8191.0 / up));
    return juce::jlimit (0, 16383, 8192 - juce::roundToInt (std::round ((8192 - value) * downward / 8192.0) * 8192.0 / downward));
}

int KeyEditor::valueForY (int lane, float y) const
{
    const auto inner = getLaneArea (lane).reduced (0.0f, 6.0f);
    const auto code = juce::isPositiveAndBelow (lane, getNumLanes()) ? getLanes()[(size_t) lane] : 0;
    const auto top = K::maxValue (code);
    auto value = juce::jlimit (0, top, juce::roundToInt ((inner.getBottom() - y) / juce::jmax (1.0f, inner.getHeight()) * (float) top));
    if (code == K::pitchbend)
        if (const auto* track = studio.getProject().findTrack (getTrackId()); track != nullptr && track->pitchbendSnap)
            value = snapBend (value);
    return value;
}

//==============================================================================
const std::vector<pr7s::Controller>& KeyEditor::controllers() const
{
    static const std::vector<pr7s::Controller> none;
    if (editingControllers())
        return workingControllers;
    const auto* part = getPart();
    return part != nullptr ? part->controllers : none;
}

bool KeyEditor::editingControllers() const noexcept
{
    return gesture == Gesture::controllerDraw || gesture == Gesture::controllerErase || gesture == Gesture::controllerMove
        || gesture == Gesture::controllerHandle;
}

void KeyEditor::beginControllers()
{
    workingControllers = controllers();
    originalControllers = workingControllers;
    moved = false;
}

void KeyEditor::commitControllers (std::vector<pr7s::Controller> edited, const std::vector<bool>& marks)
{
    const auto where = studio.findPart (partId);
    if (! where)
        return;
    // Sorted here (as the studio would), so the selection follows the events it means.
    std::vector<std::pair<pr7s::Controller, bool>> paired;
    for (size_t i = 0; i < edited.size(); ++i)
        paired.push_back ({ edited[i], i < marks.size() && marks[i] });
    std::stable_sort (paired.begin(), paired.end(), [] (const auto& a, const auto& b) { return a.first.start < b.first.start; });
    edited.clear();
    std::vector<int> nowSelected;
    for (size_t i = 0; i < paired.size(); ++i)
    {
        edited.push_back (paired[i].first);
        if (paired[i].second)
            nowSelected.push_back ((int) i);
    }
    gesture = Gesture::none;
    studio.setControllers (where->first, where->second, std::move (edited));
    if (const auto* part = getPart())
        knownControllers = part->controllers;
    selectControllers (nowSelected);
}

void KeyEditor::selectControllers (std::vector<int> indices)
{
    std::sort (indices.begin(), indices.end());
    indices.erase (std::unique (indices.begin(), indices.end()), indices.end());
    const auto count = (int) controllers().size();
    indices.erase (std::remove_if (indices.begin(), indices.end(), [count] (int i) { return ! juce::isPositiveAndBelow (i, count); }), indices.end());
    selectedControllers = std::move (indices);
    refreshPanel();
    repaint();
}

void KeyEditor::selectAllOnLane (int lane)
{
    if (! juce::isPositiveAndBelow (lane, getNumLanes()))
        return;
    const auto code = getLanes()[(size_t) lane];
    if (code == K::velocity)
    {
        selectAll();
        return;
    }
    activeLane = lane;
    lanesFocused = true;
    std::vector<int> all;
    const auto& list = controllers();
    for (int i = 0; i < (int) list.size(); ++i)
        if (K::codeOf (list[(size_t) i]) == code)
            all.push_back (i);
    selectControllers (all);
}

juce::Point<float> KeyEditor::getControllerPoint (int index) const
{
    const auto* part = getPart();
    const auto& list = controllers();
    if (part == nullptr || ! juce::isPositiveAndBelow (index, (int) list.size()))
        return {};
    const auto& c = list[(size_t) index];
    const auto lane = laneOf (K::codeOf (c));
    if (lane < 0)
        return {};
    return { xForBeat (part->start + c.start), yForValue (lane, c.value) };
}

int KeyEditor::controllerAt (int lane, juce::Point<float> p) const
{
    if (! juce::isPositiveAndBelow (lane, getNumLanes()))
        return -1;
    const auto code = getLanes()[(size_t) lane];
    const auto& list = controllers();
    auto best = -1;
    auto nearest = std::numeric_limits<float>::max();
    for (int i = 0; i < (int) list.size(); ++i)
    {
        if (K::codeOf (list[(size_t) i]) != code)
            continue;
        const auto point = getControllerPoint (i);
        const auto d = point.getDistanceFrom (p);
        if (std::abs (point.x - p.x) <= pointGrab && std::abs (point.y - p.y) <= pointGrab + 2.0f && d <= nearest)
            best = i, nearest = d;
    }
    return best;
}

bool KeyEditor::addControllerAt (int lane, double songBeat, int value, bool free)
{
    const auto* part = getPart();
    if (part == nullptr || ! juce::isPositiveAndBelow (lane, getNumLanes()))
        return false;
    const auto code = getLanes()[(size_t) lane];
    if (! K::isController (code))
        return false;
    const auto step = free ? finestStep : grid;
    const auto beat = free ? songBeat : std::floor (songBeat / step + 1.0e-9) * step;
    if (beat < part->start - 1.0e-9 || beat >= part->end())
        return false;
    auto edited = part->controllers;
    std::vector<bool> marks (edited.size(), false);
    const auto rel = juce::jmax (0.0, beat - part->start);
    for (size_t i = edited.size(); i-- > 0;)
        if (K::codeOf (edited[i]) == code && edited[i].start >= rel - 1.0e-9 && edited[i].start < rel + step - 1.0e-9)
            edited.erase (edited.begin() + (long) i), marks.erase (marks.begin() + (long) i);
    edited.push_back ({ K::typeOf (code), K::numberOf (code), rel, value, studio.getNewControllersRamp() && ! K::alwaysStep (code) });
    marks.push_back (true);
    activeLane = lane;
    lanesFocused = true;
    commitControllers (std::move (edited), marks);
    return true;
}

void KeyEditor::deleteSelectedControllers()
{
    const auto* part = getPart();
    if (part == nullptr || selectedControllers.empty())
        return;
    auto edited = part->controllers;
    for (auto it = selectedControllers.rbegin(); it != selectedControllers.rend(); ++it)
        if (juce::isPositiveAndBelow (*it, (int) edited.size()))
            edited.erase (edited.begin() + *it);
    selectedControllers.clear();
    commitControllers (std::move (edited));
}

void KeyEditor::setSelectedControllersRamp (bool ramp)
{
    const auto* part = getPart();
    if (part == nullptr || selectedControllers.empty())
        return;
    auto edited = part->controllers;
    std::vector<bool> marks (edited.size(), false);
    for (const auto i : selectedControllers)
        if (juce::isPositiveAndBelow (i, (int) edited.size()))
            edited[(size_t) i].ramp = ramp && ! K::alwaysStep (K::codeOf (edited[(size_t) i])), marks[(size_t) i] = true;
    commitControllers (std::move (edited), marks);
}

void KeyEditor::copySelectedControllers()
{
    const auto* part = getPart();
    if (part == nullptr || selectedControllers.empty())
        return;
    controllerClipboard.clear();
    auto first = std::numeric_limits<double>::max();
    for (const auto i : selectedControllers)
        first = juce::jmin (first, part->controllers[(size_t) i].start);
    for (const auto i : selectedControllers)
    {
        auto c = part->controllers[(size_t) i];
        c.start -= first;
        controllerClipboard.push_back (c);
    }
    clipboardCode = K::codeOf (part->controllers[(size_t) selectedControllers.front()]);
    clipboardOrigin = part->start + first;
}

void KeyEditor::pasteControllers (bool atOrigin)
{
    // Onto the lane last clicked (another CC lane takes them as its own; values follow the range).
    const auto* part = getPart();
    if (part == nullptr || controllerClipboard.empty())
        return;
    auto lane = activeLane;
    if (! juce::isPositiveAndBelow (lane, getNumLanes()) || ! K::isController (getLanes()[(size_t) lane]))
        lane = laneOf (clipboardCode);
    if (lane < 0)
        return;
    const auto code = getLanes()[(size_t) lane];
    const auto at = (atOrigin ? clipboardOrigin : studio.getEngine().getPosition()) - part->start;
    auto span = 0.0;
    for (const auto& c : controllerClipboard)
        span = juce::jmax (span, c.start);
    auto edited = part->controllers;
    std::vector<bool> marks (edited.size(), false);
    // What was there, replaced (the manual's Paste at Origin; ours for Paste too).
    for (size_t i = edited.size(); i-- > 0;)
        if (K::codeOf (edited[i]) == code && edited[i].start >= at - 1.0e-9 && edited[i].start <= at + span + 1.0e-9)
            edited.erase (edited.begin() + (long) i), marks.erase (marks.begin() + (long) i);
    const auto fromTop = K::maxValue (clipboardCode), toTop = K::maxValue (code);
    for (auto c : controllerClipboard)
    {
        c.start += at;
        if (c.start < 0.0 || c.start >= part->length)
            continue;
        c.type = K::typeOf (code);
        c.number = K::numberOf (code);
        if (fromTop != toTop)
            c.value = juce::roundToInt ((double) c.value * toTop / fromTop);
        edited.push_back (c);
        marks.push_back (true);
    }
    commitControllers (std::move (edited), marks);
}

std::optional<int> KeyEditor::parseControllerValue (int code, const juce::String& typed) const
{
    auto text = typed.trim().toUpperCase();
    if (text.isEmpty())
        return std::nullopt;
    const auto isNumber = [] (const juce::String& t)
    {
        return t.isNotEmpty() && t.containsOnly ("+-.,0123456789") && t.containsAnyOf ("0123456789");
    };
    if (code == K::pitchbend)
    {
        // "2ST" / "-1.5 st": semitones on the grid's range; else the bend from the middle (-8192 to +8191).
        if (text.endsWith ("ST") || text.endsWith ("SEMITONES"))
        {
            const auto number = text.upToFirstOccurrenceOf ("S", false, false).trim();
            if (! isNumber (number))
                return std::nullopt;
            const auto semitones = number.replaceCharacter (',', '.').getDoubleValue();
            const auto* track = studio.getProject().findTrack (getTrackId());
            const auto up = track != nullptr ? track->pitchbendUp : 2, downward = track != nullptr ? track->pitchbendDown : 2;
            return juce::jlimit (0, 16383, 8192 + juce::roundToInt (semitones >= 0.0 ? semitones * 8191.0 / up : semitones * 8192.0 / downward));
        }
        if (! isNumber (text))
            return std::nullopt;
        return juce::jlimit (0, 16383, 8192 + juce::roundToInt (text.replaceCharacter (',', '.').getDoubleValue()));
    }
    if ((code == 10 || code == 8) && ! isNumber (text))
    {
        if (const auto pan = ValueEntry::parsePan (text))
            return juce::jlimit (0, 127, 64 + juce::roundToInt (*pan * 100.0 * (*pan < 0.0 ? 64.0 : 63.0) / 100.0));
        return std::nullopt;
    }
    if (text == "ON")
        return 127;
    if (text == "OFF")
        return 0;
    text = text.removeCharacters (" ONF");
    if (! isNumber (text))
        return std::nullopt;
    return juce::jlimit (0, K::maxValue (code), juce::roundToInt (text.replaceCharacter (',', '.').getDoubleValue()));
}

bool KeyEditor::enterControllerValue (const juce::String& valueText, const juce::String& positionText, std::optional<bool> ramp)
{
    const auto* part = getPart();
    if (part == nullptr || selectedControllers.empty())
        return false;
    auto edited = part->controllers;
    std::vector<bool> marks (edited.size(), false);
    const auto code = K::codeOf (edited[(size_t) selectedControllers.front()]);
    std::optional<int> value;
    if (valueText.trim().isNotEmpty())
    {
        value = parseControllerValue (code, valueText);
        if (! value)
            return false;
    }
    std::optional<double> position;
    if (selectedControllers.size() == 1 && positionText.trim().isNotEmpty())
    {
        position = TempoEditor::parsePosition (positionText, studio.getProject());
        if (! position || *position < part->start - 1.0e-9 || *position >= part->end())
            return false;
    }
    for (const auto i : selectedControllers)
    {
        auto& c = edited[(size_t) i];
        if (value)
            c.value = *value;
        if (position)
            c.start = juce::jmax (0.0, *position - part->start);
        if (ramp)
            c.ramp = *ramp && ! K::alwaysStep (K::codeOf (c));
        marks[(size_t) i] = true;
    }
    commitControllers (std::move (edited), marks);
    return true;
}

void KeyEditor::showControllerEntry (int index)
{
    const auto* part = getPart();
    if (part == nullptr || ! juce::isPositiveAndBelow (index, (int) part->controllers.size()))
        return;
    if (! std::binary_search (selectedControllers.begin(), selectedControllers.end(), index))
        selectControllers ({ index });
    const auto& c = part->controllers[(size_t) index];
    const auto code = K::codeOf (c);

    // A box at the event: VALUE, AT (one event), RAMP; Enter sets them (one undo step), Escape leaves them.
    struct Entry final : juce::Component
    {
        Entry (KeyEditor& e, const juce::String& title, const juce::String& value, const juce::String& at, bool ramp, bool canRamp)
            : editor (&e)
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
                field->setJustification (juce::Justification::centredLeft);
                field->setSelectAllWhenFocused (true);
                field->onReturnKey = [this] { apply(); };
                field->onEscapeKey = [this] { close(); };
                addAndMakeVisible (*field);
            }
            valueField.setText (value, false);
            atField.setText (at, false);
            atField.setEnabled (at.isNotEmpty());
            rampKey.setButtonText ("RAMP");
            rampKey.setClickingTogglesState (true);
            rampKey.setToggleState (ramp, juce::dontSendNotification);
            rampKey.setEnabled (canRamp);
            rampKey.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
            addAndMakeVisible (rampKey);
            setKey.setButtonText ("SET");
            setKey.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
            setKey.onClick = [this] { apply(); };
            addAndMakeVisible (setKey);
            setSize (230, 128);
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
            row = r.removeFromTop (24);
            rampKey.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3));
            row.removeFromLeft (6);
            setKey.setBounds (row);
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
            if (editor == nullptr)
                return;
            if (! editor->enterControllerValue (valueField.getText(), atField.isEnabled() ? atField.getText() : juce::String(),
                                                rampKey.isEnabled() ? std::optional<bool> (rampKey.getToggleState()) : std::nullopt))
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
        juce::Component::SafePointer<KeyEditor> editor;
        juce::Label heading, valueLabel { {}, "VALUE" }, atLabel { {}, "AT" };
        juce::TextEditor valueField, atField;
        juce::TextButton rampKey, setKey;
    };
    const auto at = selectedControllers.size() == 1 ? pr7s::formatBarsBeats (part->start + c.start, studio.getProject()).replace ("  ", " ")
                                                    : juce::String();
    const auto title = K::name (code) + (selectedControllers.size() > 1 ? " (" + juce::String ((int) selectedControllers.size()) + ")" : juce::String());
    const auto point = getControllerPoint (index).toInt();
    auto content = std::make_unique<Entry> (*this, title, K::valueText (code, c.value), at, c.ramp, ! K::alwaysStep (code));
    juce::CallOutBox::launchAsynchronously (std::move (content), localAreaToGlobal (juce::Rectangle<int> (point.x - 4, point.y - 4, 8, 8)), nullptr);
}

//==============================================================================
double KeyEditor::lineValueAt (double t, double v1, double v2, double beatInto, bool reverse) const
{
    // t: 0 to 1 along the line; for the periodic shapes, `beatInto` the beats from its start (a period: the grid step).
    const auto period = juce::jmax (1.0 / 128.0, grid);
    const auto phase = std::fmod (beatInto / period + (reverse ? 0.5 : 0.0), 1.0);
    const auto amplitude = v2 - v1;   // the periodic shapes swing round where the drag began, as far as it went
    switch (lineMode)
    {
        case LineMode::line:
        case LineMode::paint:    return v1 + (v2 - v1) * t;
        case LineMode::parabola: return reverse ? v1 + (v2 - v1) * (1.0 - (1.0 - t) * (1.0 - t)) : v1 + (v2 - v1) * t * t;
        case LineMode::sine:     return v1 + amplitude * std::sin (phase * juce::MathConstants<double>::twoPi);
        case LineMode::triangle: return v1 + amplitude * (phase < 0.25 ? phase * 4.0 : phase < 0.75 ? 2.0 - phase * 4.0 : phase * 4.0 - 4.0);
        case LineMode::square:   return v1 + amplitude * (phase < 0.5 ? 1.0 : -1.0);
    }
    return v1;
}

void KeyEditor::applyVelocityLine (juce::Point<float> from, juce::Point<float> to, bool reverse)
{
    // The velocity lane: the notes the line spans (the selected ones, if any are) take its value.
    const auto* part = getPart();
    const auto lane = laneOf (K::velocity);
    if (part == nullptr || lane < 0)
        return;
    if (from.x > to.x)
        std::swap (from, to);
    const auto inner = getLaneArea (lane).reduced (0.0f, 6.0f);
    const auto level = [&] (float y) { return (double) juce::jlimit (0.0f, 1.0f, (inner.getBottom() - y) / juce::jmax (1.0f, inner.getHeight())); };
    auto edited = part->notes;
    const auto only = ! selected.empty();
    const auto b1 = beatForX (from.x);
    auto changedAny = false;
    for (int i = 0; i < (int) edited.size(); ++i)
    {
        auto& n = edited[(size_t) i];
        const auto x = xForBeat (part->start + n.start);
        if (x < from.x - 1.0f || x > to.x + 1.0f || (only && ! std::binary_search (selected.begin(), selected.end(), i)))
            continue;
        const auto t = to.x > from.x ? (double) ((x - from.x) / (to.x - from.x)) : 0.0;
        n.velocity = (float) juce::jlimit (1.0 / 127.0, 1.0, lineValueAt (t, level (from.y), level (to.y), part->start + n.start - b1, reverse));
        changedAny = true;
    }
    if (changedAny)
        commit (std::move (edited));
}

void KeyEditor::drawLine (int lane, juce::Point<float> from, juce::Point<float> to, bool reverse)
{
    const auto* part = getPart();
    if (part == nullptr || ! juce::isPositiveAndBelow (lane, getNumLanes()))
        return;
    const auto code = getLanes()[(size_t) lane];
    if (code == K::velocity)
    {
        applyVelocityLine (from, to, reverse);
        return;
    }
    if (from.x > to.x && lineMode != LineMode::sine && lineMode != LineMode::triangle && lineMode != LineMode::square)
        std::swap (from, to);
    const auto left = juce::jmin (from.x, to.x), right = juce::jmax (from.x, to.x);
    const auto periodic = lineMode == LineMode::sine || lineMode == LineMode::triangle || lineMode == LineMode::square;
    // On the grid (Length Quantize: our grid); a periodic shape: 16 to a period.
    const auto step = periodic ? juce::jmax (1.0 / 128.0, grid / 16.0) : grid;
    const auto b1 = juce::jmax (part->start, std::floor (beatForX (left) / step + 1.0e-9) * step);
    const auto b2 = juce::jmin (part->end() - 1.0e-6, std::ceil (beatForX (right) / step - 1.0e-9) * step);
    if (b2 < b1)
        return;
    const auto v1 = (double) valueForY (lane, from.y), v2 = (double) valueForY (lane, to.y);
    auto edited = part->controllers;
    std::vector<bool> marks (edited.size(), false);
    for (size_t i = edited.size(); i-- > 0;)
        if (K::codeOf (edited[i]) == code && edited[i].start >= b1 - part->start - 1.0e-9 && edited[i].start <= b2 - part->start + 1.0e-9)
            edited.erase (edited.begin() + (long) i), marks.erase (marks.begin() + (long) i);
    const auto ramp = studio.getNewControllersRamp() && ! K::alwaysStep (code);
    const auto top = K::maxValue (code);
    const auto add = [&] (double beat, double value, bool isRamp)
    {
        auto v = juce::jlimit (0, top, juce::roundToInt (value));
        if (code == K::pitchbend && studio.getProject().findTrack (getTrackId()) != nullptr
            && studio.getProject().findTrack (getTrackId())->pitchbendSnap)
            v = snapBend (v);
        edited.push_back ({ K::typeOf (code), K::numberOf (code), beat - part->start, v, isRamp });
        marks.push_back (true);
    };
    if (lineMode == LineMode::line && ramp)
    {
        // A ramp needs only its two ends.
        add (b1, v1, false);
        if (b2 > b1)
            add (b2, v2, true);
    }
    else
    {
        // (A drag right to left draws the periodic shapes from where it began, as it went.)
        const auto origin = periodic ? beatForX (from.x) : b1;
        int sent = std::numeric_limits<int>::min();
        for (auto beat = b1; beat <= b2 + 1.0e-9; beat += step)
        {
            const auto t = b2 > b1 ? (beat - b1) / (b2 - b1) : 0.0;
            const auto value = lineValueAt (periodic ? 0.0 : t, v1, v2, std::abs (beat - origin), reverse);
            const auto rounded = juce::roundToInt (value);
            if (rounded == sent && beat + step <= b2 + 1.0e-9)
                continue;   // (the same value again adds nothing)
            add (beat, value, ramp && ! periodic && beat > b1);
            sent = rounded;
        }
    }
    activeLane = lane;
    lanesFocused = true;
    commitControllers (std::move (edited), marks);
}

void KeyEditor::drawControllersTo (juce::Point<float> to)
{
    // DRAW (or SELECT with Alt, or LINE's Paint): every step the mouse crossed takes the value it was at.
    const auto* part = getPart();
    if (part == nullptr || ! juce::isPositiveAndBelow (activeLane, getNumLanes()))
        return;
    const auto code = getLanes()[(size_t) activeLane];
    const auto step = juce::ModifierKeys::currentModifiers.isCommandDown() ? finestStep : grid;
    auto from = lastDrawPoint;
    const auto bFrom = beatForX (from.x), bTo = beatForX (to.x);
    auto first = std::floor (juce::jmin (bFrom, bTo) / step + 1.0e-9) * step;
    const auto last = std::floor (juce::jmax (bFrom, bTo) / step + 1.0e-9) * step;
    const auto ramp = studio.getNewControllersRamp() && ! K::alwaysStep (code);
    for (auto beat = first; beat <= last + 1.0e-9; beat += step)
    {
        if (beat < part->start - 1.0e-9 || beat >= part->end())
            continue;
        const auto x = xForBeat (beat);
        const auto t = std::abs (to.x - from.x) > 0.5f ? juce::jlimit (0.0f, 1.0f, (x - from.x) / (to.x - from.x)) : 1.0f;
        const auto value = valueForY (activeLane, from.y + (to.y - from.y) * t);
        const auto rel = beat - part->start;
        for (size_t i = workingControllers.size(); i-- > 0;)
            if (K::codeOf (workingControllers[i]) == code && workingControllers[i].start >= rel - 1.0e-9 && workingControllers[i].start < rel + step - 1.0e-9)
                workingControllers.erase (workingControllers.begin() + (long) i);
        workingControllers.push_back ({ K::typeOf (code), K::numberOf (code), juce::jmax (0.0, rel), value, ramp });
        moved = true;
    }
    pr7s::ControllerKinds::sort (workingControllers);
    lastDrawPoint = to;
    repaint (getLaneArea (activeLane).getSmallestIntegerContainer());
}

void KeyEditor::eraseControllersTo (juce::Point<float> to)
{
    const auto* part = getPart();
    if (part == nullptr || ! juce::isPositiveAndBelow (activeLane, getNumLanes()))
        return;
    const auto code = getLanes()[(size_t) activeLane];
    const auto left = juce::jmin (lastDrawPoint.x, to.x) - 3.0f, right = juce::jmax (lastDrawPoint.x, to.x) + 3.0f;
    for (size_t i = workingControllers.size(); i-- > 0;)
    {
        const auto x = xForBeat (part->start + workingControllers[i].start);
        if (K::codeOf (workingControllers[i]) == code && x >= left && x <= right)
            workingControllers.erase (workingControllers.begin() + (long) i), moved = true;
    }
    lastDrawPoint = to;
    repaint (getLaneArea (activeLane).getSmallestIntegerContainer());
}

//==============================================================================
juce::Rectangle<float> KeyEditor::getControllerBox() const
{
    const auto* part = getPart();
    const auto& list = controllers();
    if (part == nullptr || selectedControllers.size() < 2 || ! juce::isPositiveAndBelow (activeLane, getNumLanes()))
        return {};
    const auto inner = getLaneArea (activeLane).reduced (0.0f, 6.0f);
    auto left = std::numeric_limits<float>::max(), right = std::numeric_limits<float>::lowest(), top = inner.getBottom() - 8.0f;
    for (const auto i : selectedControllers)
        if (juce::isPositiveAndBelow (i, (int) list.size()))
        {
            const auto p = getControllerPoint (i);
            left = juce::jmin (left, p.x);
            right = juce::jmax (right, p.x);
            top = juce::jmin (top, p.y);
        }
    if (right < left)
        return {};
    const auto x1 = juce::jmax (inner.getX() + 4.0f, left - 6.0f), x2 = juce::jmin (inner.getRight() - 4.0f, right + 6.0f);
    return x2 > x1 ? juce::Rectangle<float> (x1, top, x2 - x1, inner.getBottom() - top) : juce::Rectangle<float>();
}

KeyEditor::VelocityHandle KeyEditor::controllerHandleAt (juce::Point<float> p) const
{
    const auto box = getControllerBox();
    if (box.isEmpty() || ! getLaneArea (activeLane).expanded (0.0f, 2.0f).contains (p))
        return VelocityHandle::none;
    constexpr auto grab = 6.0f, corner = 10.0f;
    if (std::abs (p.x - box.getRight()) <= grab && std::abs (p.y - box.getCentreY()) <= corner)
        return VelocityHandle::scaleCentre;
    if (std::abs (p.y - box.getY()) > grab || p.x < box.getX() - grab || p.x > box.getRight() + grab)
        return VelocityHandle::none;
    if (p.x <= box.getX() + corner)
        return VelocityHandle::tiltLeft;
    if (p.x >= box.getRight() - corner)
        return VelocityHandle::tiltRight;
    if (std::abs (p.x - box.getCentreX()) <= corner)
        return VelocityHandle::scale;
    return VelocityHandle::move;
}

void KeyEditor::dragControllerHandle (float dy)
{
    // From the values as the drag began (`originalControllers`), so it can go back.
    const auto* part = getPart();
    if (part == nullptr || ! juce::isPositiveAndBelow (activeLane, getNumLanes()))
        return;
    const auto code = getLanes()[(size_t) activeLane];
    const auto top = (double) K::maxValue (code);
    const auto inner = getLaneArea (activeLane).reduced (0.0f, 6.0f);
    const auto delta = -(double) dy / juce::jmax (1.0f, inner.getHeight());   // up: higher
    auto highest = 0.0, lowest = 1.0;
    for (const auto i : selectedControllers)
        if (juce::isPositiveAndBelow (i, (int) originalControllers.size()))
        {
            const auto v = originalControllers[(size_t) i].value / top;
            highest = juce::jmax (highest, v);
            lowest = juce::jmin (lowest, v);
        }
    const auto middle = (highest + lowest) * 0.5;
    const auto base = code == K::pitchbend ? 0.5 : 0.0;   // (pitchbend scales from no bend)
    for (const auto i : selectedControllers)
    {
        if (! juce::isPositiveAndBelow (i, (int) originalControllers.size()))
            continue;
        const auto was = originalControllers[(size_t) i].value / top;
        const auto x = xForBeat (part->start + originalControllers[(size_t) i].start);
        const auto t = (double) juce::jlimit (0.0f, 1.0f, (x - boxAtDown.getX()) / juce::jmax (1.0f, boxAtDown.getWidth()));
        auto now = was;
        switch (handle)
        {
            case VelocityHandle::move:        now = was + delta; break;
            case VelocityHandle::scale:       now = highest - base > 1.0e-9 ? base + (was - base) * juce::jmax (0.0, (highest - base + delta) / (highest - base)) : was; break;
            case VelocityHandle::tiltLeft:    now = was + delta * (1.0 - t); break;
            case VelocityHandle::tiltRight:   now = was + delta * t; break;
            case VelocityHandle::scaleCentre: now = middle + (was - middle) * juce::jmax (0.0, 1.0 + delta * 2.0); break;
            case VelocityHandle::none:        break;
        }
        workingControllers[(size_t) i].value = juce::jlimit (0, (int) top, juce::roundToInt (now * top));
    }
    moved = true;
    repaint (getLaneArea (activeLane).expanded (2.0f).getSmallestIntegerContainer());
}

//==============================================================================
bool KeyEditor::laneMouseMove (const juce::MouseEvent& event)
{
    const auto p = event.position;
    if (getNumLanes() > 0 && p.y >= acrossTop() - 4.0f && p.y <= acrossTop() + 1.0f && p.x >= (float) sideWidth)
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        return true;
    }
    switch (controllerHandleAt (p))
    {
        case VelocityHandle::move:
        case VelocityHandle::scale:
        case VelocityHandle::tiltLeft:
        case VelocityHandle::tiltRight:   setMouseCursor (juce::MouseCursor::UpDownResizeCursor); return true;
        case VelocityHandle::scaleCentre: setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor); return true;
        case VelocityHandle::none:        break;
    }
    const auto lane = laneAt (p);
    if (lane < 0)
        return false;
    if (getLaneHeader (lane).contains (p))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        return true;
    }
    if (getLanes()[(size_t) lane] == K::velocity)
        return false;
    if (tool == Tool::select)
        setMouseCursor (controllerAt (lane, p) >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    else
        setMouseCursor (tool == Tool::draw ? penCursor : juce::MouseCursor::CrosshairCursor);
    return true;
}

bool KeyEditor::laneMouseDown (const juce::MouseEvent& event)
{
    const auto p = event.position;
    // The divider above the lanes.
    if (getNumLanes() > 0 && p.y >= acrossTop() - 4.0f && p.y <= acrossTop() + 1.0f && p.x >= (float) sideWidth)
    {
        gesture = Gesture::laneResize;
        heightAtDown = laneHeightNow();
        return true;
    }
    const auto lane = laneAt (p);
    if (lane < 0)
    {
        if (getNotesArea().contains (p))
            lanesFocused = false;
        return false;
    }
    lanesFocused = true;
    if (getLaneHeader (lane).contains (p))
    {
        showLaneMenu (lane);
        return true;
    }
    const auto code = getLanes()[(size_t) lane];
    if (event.mods.isPopupMenu())
    {
        showLaneMenu (lane);
        return true;
    }
    if (tool == Tool::line && lineMode != LineMode::paint)
    {
        activeLane = lane;
        gesture = Gesture::controllerLine;
        lineFrom = lineTo = p;
        return true;
    }
    if (code == K::velocity)
    {
        if (tool == Tool::line)   // (Paint: as dragging across the bars)
            return false;
        return false;   // the velocity bars: KeyEditor.cpp's own
    }
    if (activeLane != lane)
        selectedControllers.clear();
    activeLane = lane;
    if (getPart() == nullptr)
        return true;

    if (const auto h = controllerHandleAt (p); h != VelocityHandle::none && tool == Tool::select)
    {
        beginControllers();
        gesture = Gesture::controllerHandle;
        handle = h;
        boxAtDown = getControllerBox();
        return true;
    }
    const auto hit = controllerAt (lane, p);
    const auto drawing = tool == Tool::draw || (tool == Tool::line && lineMode == LineMode::paint) || (tool == Tool::select && event.mods.isAltDown());
    if (drawing)
    {
        beginControllers();
        selectedControllers.clear();
        gesture = Gesture::controllerDraw;
        lastDrawPoint = p;
        drawControllersTo (p);
        return true;
    }
    if (tool == Tool::erase)
    {
        beginControllers();
        selectedControllers.clear();
        gesture = Gesture::controllerErase;
        lastDrawPoint = p;
        eraseControllersTo (p);
        return true;
    }
    if (tool != Tool::select)
        return true;
    if (hit < 0)
    {
        gesture = Gesture::controllerBand;
        controllersBefore = event.mods.isShiftDown() || event.mods.isCommandDown() ? selectedControllers : std::vector<int>();
        selectControllers (controllersBefore);
        band = { p, p };
        return true;
    }
    if (event.mods.isShiftDown() || event.mods.isCommandDown())
    {
        auto now = selectedControllers;
        if (const auto it = std::find (now.begin(), now.end(), hit); it != now.end())
            now.erase (it);
        else
            now.push_back (hit);
        selectControllers (now);
        return true;
    }
    if (! std::binary_search (selectedControllers.begin(), selectedControllers.end(), hit))
        selectControllers ({ hit });
    beginControllers();
    gesture = Gesture::controllerMove;
    anchorController = hit;
    draggingControllers = selectedControllers;
    return true;
}

void KeyEditor::laneMouseDrag (const juce::MouseEvent& event)
{
    const auto p = event.position;
    const auto* part = getPart();
    if (gesture == Gesture::laneResize)
        setLaneHeight (juce::roundToInt (heightAtDown - (p.y - downAt.y) / (float) juce::jmax (1, getNumLanes())));
    else if (gesture == Gesture::controllerDraw)
        drawControllersTo (p);
    else if (gesture == Gesture::controllerErase)
        eraseControllersTo (p);
    else if (gesture == Gesture::controllerHandle)
        dragControllerHandle (p.y - downAt.y);
    else if (gesture == Gesture::controllerLine)
    {
        lineTo = p;
        repaint (getLaneArea (activeLane).getSmallestIntegerContainer());
    }
    else if (gesture == Gesture::controllerBand)
    {
        {
            band = juce::Rectangle<float> (downAt, p);
            auto now = controllersBefore;
            const auto& list = controllers();
            const auto code = juce::isPositiveAndBelow (activeLane, getNumLanes()) ? getLanes()[(size_t) activeLane] : -2;
            for (int i = 0; i < (int) list.size(); ++i)
                if (K::codeOf (list[(size_t) i]) == code && band.expanded (2.0f).contains (getControllerPoint (i)))
                    now.push_back (i);
            selectControllers (now);
        }
    }
    else if (gesture == Gesture::controllerMove)
    {
        {
            if (part == nullptr || ! juce::isPositiveAndBelow (anchorController, (int) originalControllers.size())
                || (! moved && event.getDistanceFromDragStart() < 3))
                return;
            moved = true;
            const auto free = event.mods.isCommandDown() || ! snapOn;
            const auto& a = originalControllers[(size_t) anchorController];
            const auto from = part->start + a.start;
            auto shift = snapped (from + beatForX (p.x) - beatForX (downAt.x), free) - from;
            const auto change = valueForY (activeLane, p.y) - valueForY (activeLane, downAt.y);
            // All of them inside the part.
            auto earliest = part->length, latest = 0.0;
            for (const auto i : draggingControllers)
            {
                earliest = juce::jmin (earliest, originalControllers[(size_t) i].start);
                latest = juce::jmax (latest, originalControllers[(size_t) i].start);
            }
            shift = juce::jmax (-earliest, juce::jmin (shift, part->length - 1.0e-6 - latest));
            for (const auto i : draggingControllers)
            {
                auto& c = workingControllers[(size_t) i];
                c = originalControllers[(size_t) i];
                c.start += shift;
                c.value = juce::jlimit (0, K::maxValue (K::codeOf (c)), c.value + change);
            }
            repaint (getLaneArea (activeLane).expanded (2.0f).getSmallestIntegerContainer());
        }
    }
}

void KeyEditor::laneMouseUp()
{
    const auto finished = gesture;
    gesture = Gesture::none;
    band = {};
    if (finished == Gesture::controllerDraw || finished == Gesture::controllerErase)
    {
        if (moved)
            commitControllers (workingControllers);
    }
    else if (finished == Gesture::controllerHandle || finished == Gesture::controllerMove)
    {
            if (moved)
            {
                std::vector<bool> marks (workingControllers.size(), false);
                for (const auto i : selectedControllers)
                    if (juce::isPositiveAndBelow (i, (int) marks.size()))
                        marks[(size_t) i] = true;
                commitControllers (workingControllers, marks);
            }
            else if (finished == Gesture::controllerMove && anchorController >= 0)
                selectControllers ({ anchorController });   // a click on one of a group, no drag: just that one
    }
    else if (finished == Gesture::controllerLine)
    {
        if (lineFrom.getDistanceFrom (lineTo) >= 2.0f || lineMode == LineMode::line)
            drawLine (activeLane, lineFrom, lineTo, juce::ModifierKeys::currentModifiers.isCommandDown());
    }
    repaint();
}

bool KeyEditor::laneDoubleClick (const juce::MouseEvent& event)
{
    const auto lane = laneAt (event.position);
    if (lane < 0 || getLaneHeader (lane).contains (event.position))
        return lane >= 0;
    const auto code = getLanes()[(size_t) lane];
    if (code == K::velocity || tool != Tool::select)
        return code != K::velocity;
    activeLane = lane;
    if (const auto hit = controllerAt (lane, event.position); hit >= 0)
        showControllerEntry (hit);
    else
        addControllerAt (lane, beatForX (event.position.x), valueForY (lane, event.position.y));
    return true;
}

//==============================================================================
namespace
{
    // Every kind a lane can show, for the menus: the most used first, the rest in submenus.
    void addKinds (juce::PopupMenu& menu, const KeyEditor& editor, const pr7s::Part* part, int current, std::function<void (int)> choose)
    {
        const auto used = part != nullptr ? K::usedCodes (*part) : std::vector<int>();
        const auto isUsed = [&] (int code) { return std::find (used.begin(), used.end(), code) != used.end(); };
        const auto item = [&] (juce::PopupMenu& into, int code)
        {
            const auto shownElsewhere = code != current && editor.laneOf (code) >= 0;
            into.addItem (K::name (code) + (isUsed (code) ? "  (USED)" : ""), ! shownElsewhere, code == current, [choose, code] { choose (code); });
        };
        for (const auto code : K::commonCodes())
            item (menu, code);
        juce::PopupMenu poly;
        for (int note = 0; note < 128; ++note)
            if (isUsed (K::polyBase + note))
                item (poly, K::polyBase + note);
        if (const auto& sel = editor.getSelectedNotes(); ! sel.empty() && part != nullptr && juce::isPositiveAndBelow (sel.front(), (int) part->notes.size()))
            if (! isUsed (K::polyBase + part->notes[(size_t) sel.front()].pitch))
                item (poly, K::polyBase + part->notes[(size_t) sel.front()].pitch);
        if (poly.getNumItems() == 0)
            poly.addItem ("Select a note first: its pitch", false, false, [] {});
        menu.addSubMenu ("POLY PRESSURE", poly);
        juce::PopupMenu all;
        for (int first = 0; first < 128; first += 32)
        {
            juce::PopupMenu range;
            for (int n = first; n < first + 32; ++n)
                item (range, n);
            all.addSubMenu ("CC " + juce::String (first) + " - " + juce::String (first + 31), range);
        }
        menu.addSubMenu ("ALL CONTROLLERS", all);
    }
}

void KeyEditor::showLaneMenu (int lane)
{
    if (! juce::isPositiveAndBelow (lane, getNumLanes()))
        return;
    const auto code = getLanes()[(size_t) lane];
    const auto* part = getPart();
    juce::Component::SafePointer<KeyEditor> safe (this);
    juce::PopupMenu menu;
    menu.addSectionHeader ("THIS LANE SHOWS");
    addKinds (menu, *this, part, code, [safe, lane] (int chosen) { if (safe != nullptr) safe->setLane (lane, chosen); });
    menu.addSeparator();
    menu.addItem ("Select All Events on Lane", part != nullptr, false, [safe, lane] { if (safe != nullptr) safe->selectAllOnLane (lane); });
    menu.addItem ("Remove This Lane", true, false, [safe, lane] { if (safe != nullptr) safe->removeLane (lane); });
    if (K::isController (code))
    {
        const auto hasSelection = ! selectedControllers.empty() && activeLane == lane;
        menu.addSeparator();
        menu.addItem ("Cut", hasSelection, false, [safe] { if (safe != nullptr) { safe->copySelectedControllers(); safe->deleteSelectedControllers(); } });
        menu.addItem ("Copy", hasSelection, false, [safe] { if (safe != nullptr) safe->copySelectedControllers(); });
        menu.addItem ("Paste", ! controllerClipboard.empty(), false, [safe, lane] { if (safe != nullptr) { safe->activeLane = lane; safe->pasteControllers (false); } });
        menu.addItem ("Paste at Origin", ! controllerClipboard.empty(), false, [safe, lane] { if (safe != nullptr) { safe->activeLane = lane; safe->pasteControllers (true); } });
        if (! K::alwaysStep (code))
        {
            juce::PopupMenu curve;
            curve.addItem ("Step", hasSelection, false, [safe] { if (safe != nullptr) safe->setSelectedControllersRamp (false); });
            curve.addItem ("Ramp", hasSelection, false, [safe] { if (safe != nullptr) safe->setSelectedControllersRamp (true); });
            menu.addSubMenu ("Curve of the Selected Events", curve, hasSelection);
        }
        if (hasSelection)
            menu.addItem ("Type a Value...", true, false, [safe] { if (safe != nullptr && ! safe->selectedControllers.empty()) safe->showControllerEntry (safe->selectedControllers.front()); });
    }
    if (code == K::pitchbend)
        if (const auto* track = studio.getProject().findTrack (getTrackId()))
        {
            const auto id = track->id;
            const auto up = track->pitchbendUp, downward = track->pitchbendDown;
            const auto snapping = track->pitchbendSnap, shown = track->pitchbendGrid;
            auto& s = studio;
            menu.addSeparator();
            menu.addSectionHeader ("PITCHBEND GRID");
            menu.addItem ("Snap Pitchbend Events", true, snapping, [&s, id, up, downward, snapping, shown] { s.setPitchbendGrid (id, up, downward, ! snapping, shown); });
            menu.addItem ("Show Semitones Grid", ! snapping, shown, [&s, id, up, downward, snapping, shown] { s.setPitchbendGrid (id, up, downward, snapping, ! shown); });
            juce::PopupMenu both, upMenu, downMenu;
            for (const auto n : { 1, 2, 3, 4, 5, 7, 12, 24, 48, 96 })
            {
                both.addItem (juce::String (n) + " semitones", true, up == n && downward == n, [&s, id, n, snapping, shown] { s.setPitchbendGrid (id, n, n, snapping, shown); });
                upMenu.addItem ("+" + juce::String (n), true, up == n, [&s, id, n, downward, snapping, shown] { s.setPitchbendGrid (id, n, downward, snapping, shown); });
                downMenu.addItem ("-" + juce::String (n), true, downward == n, [&s, id, n, up, snapping, shown] { s.setPitchbendGrid (id, up, n, snapping, shown); });
            }
            menu.addSubMenu ("Range (up and down: +/-" + juce::String (up) + (up == downward ? juce::String() : "/" + juce::String (downward)) + ")", both);
            menu.addSubMenu ("Range Up", upMenu);
            menu.addSubMenu ("Range Down", downMenu);
        }
    menu.addSeparator();
    juce::PopupMenu create;
    addKinds (create, *this, part, -2, [safe] (int chosen) { if (safe != nullptr) safe->addLane (chosen); });
    menu.addSubMenu ("Create Controller Lane", create);
    menu.addItem ("Show Used Controllers", part != nullptr, false, [safe] { if (safe != nullptr) safe->showUsedControllers(); });
    menu.addItem ("Show Velocity Lane Only", true, false, [safe] { if (safe != nullptr) safe->showVelocityLaneOnly(); });
    menu.addItem ("Hide Controller Lanes", true, false, [safe] { if (safe != nullptr) safe->toggleLanesShown(); });
    const auto ramp = studio.getNewControllersRamp();
    juce::PopupMenu type;
    type.addItem ("Ramp", true, ramp, [this, safe] { if (safe != nullptr) studio.setNewControllersRamp (true); });
    type.addItem ("Step", true, ! ramp, [this, safe] { if (safe != nullptr) studio.setNewControllersRamp (false); });
    menu.addSubMenu ("Type of New Controller Events", type);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (getLaneHeader (lane).toNearestInt())));
}

void KeyEditor::showLanesMenu()
{
    const auto* part = getPart();
    juce::Component::SafePointer<KeyEditor> safe (this);
    juce::PopupMenu menu;
    juce::PopupMenu create;
    addKinds (create, *this, part, -2, [safe] (int chosen) { if (safe != nullptr) safe->addLane (chosen); });
    menu.addSubMenu ("Create Controller Lane", create, getTrackId() != 0);
    menu.addItem ("Show Used Controllers", part != nullptr, false, [safe] { if (safe != nullptr) safe->showUsedControllers(); });
    menu.addItem ("Show Velocity Lane Only", getTrackId() != 0, false, [safe] { if (safe != nullptr) safe->showVelocityLaneOnly(); });
    menu.addItem (getLanes().empty() ? "Show Controller Lanes" : "Hide Controller Lanes", getTrackId() != 0, false,
                  [safe] { if (safe != nullptr) safe->toggleLanesShown(); });
    const auto ramp = studio.getNewControllersRamp();
    juce::PopupMenu type;
    type.addItem ("Ramp", true, ramp, [this, safe] { if (safe != nullptr) studio.setNewControllersRamp (true); });
    type.addItem ("Step", true, ! ramp, [this, safe] { if (safe != nullptr) studio.setNewControllersRamp (false); });
    menu.addSubMenu ("Type of New Controller Events", type);
    juce::PopupMenu heights;
    for (const auto h : { 50, 86, 120, 160, 220 })
        heights.addItem (juce::String (h) + " px", true, laneHeight == h, [safe, h] { if (safe != nullptr) safe->setLaneHeight (h); });
    menu.addSubMenu ("Lane Height", heights);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (lanesKey));
}

//==============================================================================
void KeyEditor::paintLanes (juce::Graphics& g)
{
    for (int lane = 0; lane < getNumLanes(); ++lane)
    {
        if (getLanes()[(size_t) lane] == K::velocity)
            paintVelocities (g);
        else
            paintControllerLane (g, lane);
        paintLaneHeader (g, lane);
        if (lane > 0)
        {
            g.setColour (ParagonColours::panelEdge);
            g.fillRect (getLaneArea (lane).withX ((float) sideWidth).withWidth ((float) (getWidth() - sideWidth)).withHeight (1.0f));
        }
    }
    // The line being drawn.
    if (gesture == Gesture::controllerLine && juce::isPositiveAndBelow (activeLane, getNumLanes()))
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (getLaneArea (activeLane).getSmallestIntegerContainer());
        g.setColour (juce::Colour (0xfff4eeea));
        const auto left = juce::jmin (lineFrom.x, lineTo.x), right = juce::jmax (lineFrom.x, lineTo.x);
        const auto periodic = lineMode == LineMode::sine || lineMode == LineMode::triangle || lineMode == LineMode::square;
        juce::Path path;
        const auto reverse = juce::ModifierKeys::currentModifiers.isCommandDown();
        const auto beatFrom = beatForX (lineFrom.x);
        for (auto x = left; x <= right + 0.5f; x += 2.0f)
        {
            const auto t = right > left ? (double) ((x - left) / (right - left)) : 0.0;
            const auto ends = lineFrom.x <= lineTo.x ? std::pair { lineFrom.y, lineTo.y } : std::pair { lineTo.y, lineFrom.y };
            const auto y = periodic ? (float) lineValueAt (0.0, lineFrom.y, lineTo.y, std::abs (beatForX (x) - beatFrom), reverse)
                                    : (float) lineValueAt (t, ends.first, ends.second, 0.0, reverse);
            if (path.isEmpty())
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }
        g.strokePath (path, juce::PathStrokeType (1.6f));
    }
}

void KeyEditor::paintLaneHeader (juce::Graphics& g, int lane)
{
    // At the header's foot: the lane's name and the menu's arrow; above it, the value at the playhead.
    const auto header = getLaneHeader (lane);
    const auto code = getLanes()[(size_t) lane];
    auto r = header.reduced (3.0f, 3.0f);
    auto title = r.removeFromBottom (12.0f);
    juce::Path arrow;
    const auto ax = title.getRight() - 4.0f, ay = title.getCentreY();
    arrow.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
    g.setColour (ParagonColours::accent);
    g.fillPath (arrow);
    g.setColour (lane == activeLane && lanesFocused ? ParagonColours::text : ParagonColours::textDim);
    g.setFont (font (8.5f));
    g.drawText (K::shortName (code), title.withTrimmedRight (9.0f), juce::Justification::centredLeft, true);
    if (K::isController (code) && r.getHeight() >= 18.0f)
        if (const auto* part = getPart())
        {
            const auto beat = studio.getEngine().getPosition() - part->start;
            if (const auto value = beat >= 0.0 && beat < part->length ? K::valueAt (part->controllers, code, beat) : std::nullopt)
            {
                g.setColour (ParagonColours::text);
                g.setFont (font (9.0f, juce::Font::plain));
                g.drawText (K::valueText (code, *value), r.removeFromBottom (12.0f), juce::Justification::centredLeft, true);
            }
        }
}

void KeyEditor::paintControllerLane (juce::Graphics& g, int lane)
{
    const auto area = getLaneArea (lane);
    const auto header = getLaneHeader (lane);
    const auto code = getLanes()[(size_t) lane];
    g.setColour (juce::Colour (0xff0b090c));
    g.fillRect (area);
    g.setColour (juce::Colour (0xff100c11));
    g.fillRect (header);
    const auto inner = area.reduced (0.0f, 6.0f);
    const auto* part = getPart();
    const auto* track = studio.getProject().findTrack (getTrackId());

    // Guides: the top, the middle (pitchbend: no bend; pan: centre), and the semitone grid.
    const auto top = K::maxValue (code);
    g.setFont (font (8.5f, juce::Font::plain));
    const auto guide = [&] (int value, juce::String text, float alpha)
    {
        const auto y = yForValue (lane, value);
        g.setColour (ParagonColours::panelEdge.withAlpha (alpha));
        g.fillRect (juce::Rectangle<float> (area.getX(), y, area.getWidth(), 1.0f));
        if (text.isNotEmpty() && inner.getHeight() >= 30.0f && y < header.getBottom() - 30.0f)   // (clear of the name and the value)
        {
            g.setColour (ParagonColours::textDim);
            g.drawText (text, juce::Rectangle<float> (header.getX(), y - 6.0f, header.getWidth() - 4.0f, 12.0f), juce::Justification::centredRight, false);
        }
    };
    if (code == K::pitchbend)
    {
        if (track != nullptr && (track->pitchbendGrid || track->pitchbendSnap))
        {
            for (int s = 1; s <= track->pitchbendUp; ++s)
                guide (8192 + juce::roundToInt (s * 8191.0 / track->pitchbendUp), inner.getHeight() / (float) track->pitchbendUp > 14.0f ? "+" + juce::String (s) : juce::String(), 0.45f);
            for (int s = 1; s <= track->pitchbendDown; ++s)
                guide (8192 - juce::roundToInt (s * 8192.0 / track->pitchbendDown), inner.getHeight() / (float) track->pitchbendDown > 14.0f ? "-" + juce::String (s) : juce::String(), 0.45f);
        }
        guide (8192, "0", 0.9f);
    }
    else
    {
        guide (top, juce::String (top), 0.7f);
        guide (64, code == 10 || code == 8 ? "C" : "64", 0.5f);
    }
    if (part == nullptr)
        return;

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (area.getSmallestIntegerContainer());
    // Outside the part: shaded.
    const auto partLeft = xForBeat (part->start), partRight = xForBeat (part->end());
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (area.withRight (partLeft));
    g.fillRect (area.withLeft (partRight));

    const auto& list = controllers();
    std::vector<int> indices;
    for (int i = 0; i < (int) list.size(); ++i)
        if (K::codeOf (list[(size_t) i]) == code)
            indices.push_back (i);
    const auto colour = pr7s::FamilyColours::pink, pointColour = pr7s::FamilyColours::orange;
    // The value the part starts with (from the parts before it on the track), dim, up to its first event.
    std::optional<int> before;
    if (track != nullptr)
        for (const auto& other : track->parts)
            if (other.end() <= part->start + 1.0e-9 && ! other.isAudio())
                if (const auto v = K::valueAt (other.controllers, code, other.length); v)
                    before = v;   // (parts in order of start, mostly; the last one before it wins)
    if (before)
    {
        const auto end = indices.empty() ? partRight : xForBeat (part->start + list[(size_t) indices.front()].start);
        g.setColour (colour.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (partLeft, yForValue (lane, *before), juce::jmax (0.0f, end - partLeft), 1.2f));
    }
    if (indices.empty())
        return;
    // The curve: each value until the next (or a ramp to it), filled below.
    juce::Path line, fill;
    const auto baseY = code == K::pitchbend ? yForValue (lane, 8192) : inner.getBottom();
    const auto firstX = xForBeat (part->start + list[(size_t) indices.front()].start);
    line.startNewSubPath (firstX, yForValue (lane, list[(size_t) indices.front()].value));
    fill.startNewSubPath (firstX, baseY);
    fill.lineTo (firstX, yForValue (lane, list[(size_t) indices.front()].value));
    for (size_t k = 1; k < indices.size(); ++k)
    {
        const auto& c = list[(size_t) indices[k]];
        const auto x = xForBeat (part->start + c.start), y = yForValue (lane, c.value);
        if (! c.ramp)
        {
            const auto prevY = yForValue (lane, list[(size_t) indices[k - 1]].value);
            line.lineTo (x, prevY);
            fill.lineTo (x, prevY);
        }
        line.lineTo (x, y);
        fill.lineTo (x, y);
    }
    const auto lastY = yForValue (lane, list[(size_t) indices.back()].value);
    line.lineTo (partRight, lastY);
    fill.lineTo (partRight, lastY);
    fill.lineTo (partRight, baseY);
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.16f));
    g.fillPath (fill);
    g.setColour (colour.withAlpha (0.9f));
    g.strokePath (line, juce::PathStrokeType (1.4f));
    // The points (selected: pearl white, larger).
    const auto clip = g.getClipBounds().toFloat();
    for (const auto i : indices)
    {
        const auto p = getControllerPoint (i);
        if (p.x < clip.getX() - 6.0f || p.x > clip.getRight() + 6.0f)
            continue;
        const auto isSelected = std::binary_search (selectedControllers.begin(), selectedControllers.end(), i);
        const auto size = isSelected ? 7.0f : 5.0f;
        g.setColour (isSelected ? juce::Colour (0xfff4eeea) : pointColour);
        g.fillRect (juce::Rectangle<float> (size, size).withCentre (p));
    }
    // The handles' box (two or more selected on this lane).
    if (lane == activeLane)
        if (const auto box = getControllerBox(); ! box.isEmpty())
        {
            g.setColour (ParagonColours::accent.withAlpha (0.08f));
            g.fillRect (box);
            g.setColour (ParagonColours::accent.withAlpha (0.8f));
            g.drawRect (box, 1.0f);
            for (const auto c : { box.getTopLeft(), juce::Point<float> (box.getCentreX(), box.getY()), box.getTopRight(),
                                  juce::Point<float> (box.getRight(), box.getCentreY()) })
            {
                const auto r = juce::Rectangle<float> (7.0f, 7.0f).withCentre (c);
                g.setColour (juce::Colour (0xff0b090c));
                g.fillRect (r);
                g.setColour (ParagonColours::accent);
                g.drawRect (r, 1.2f);
            }
        }
    // A rectangle being dragged to select.
    if (gesture == Gesture::controllerBand && lane == activeLane && ! band.isEmpty())
    {
        g.setColour (ParagonColours::accent.withAlpha (0.12f));
        g.fillRect (band);
        g.setColour (ParagonColours::accent.withAlpha (0.7f));
        g.drawRect (band, 1.0f);
    }
}
