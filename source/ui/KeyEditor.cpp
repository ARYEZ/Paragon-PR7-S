#include "KeyEditor.h"

#include "ui/ParagonLookAndFeel.h"
#include "ui/ToolIcons.h"

#include <limits>
#include <numeric>

namespace
{
    constexpr float edgeGrab = 6.0f;          // the right end of a note, for lengthening it
    constexpr double finest = 1.0 / 64.0;     // the shortest note (in beats) when not snapping
    const juce::Colour whiteRow { 0xff120e13 }, blackRow { 0xff0b090c }, gridBar { 0xff2e252c }, gridBeat { 0xff1d171c },
                       gridStep { 0xff161216 }, octaveLine { 0xff2a2229 };

    juce::FontOptions font (float height, int style = juce::Font::bold) { return ParagonLookAndFeel::textFont (height, style); }

    bool isBlackKey (int pitch) noexcept
    {
        const auto inOctave = pitch % 12;
        return inOctave == 1 || inOctave == 3 || inOctave == 6 || inOctave == 8 || inOctave == 10;
    }

    // A length as a musician reads it: "1 sixteenth", "3 sixteenths", or in beats.
    juce::String describeLength (double beats)
    {
        const auto sixteenths = beats * 4.0;
        if (std::abs (sixteenths - std::round (sixteenths)) < 1.0e-6)
        {
            const auto count = (int) std::round (sixteenths);
            return juce::String (count) + (count == 1 ? " sixteenth" : " sixteenths");
        }
        return juce::String (beats, 3) + " beats";
    }
}

KeyEditor::KeyEditor (pr7s::Studio& studioToUse, ArrangeView& arrangeToUse)
    : studio (studioToUse), arrange (arrangeToUse)
{
    for (auto* button : { &selectTool, &drawTool, &splitTool, &eraseTool, &quantizeButton })
    {
        button->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }
    selectTool.setTooltip ("SELECT (1): select, move, copy (Alt) and lengthen notes; double-click for a new one");
    drawTool.setTooltip ("DRAW (8): click for a new note (drag right to make it longer); click a note to remove it");
    eraseTool.setTooltip ("ERASE (5): click or drag across notes to remove them");
    splitTool.setTooltip ("SPLIT (3): click a note to cut it in two there (on the grid; Ctrl: anywhere); several selected: all of them that cross it");
    splitTool.onClick = [this] { setTool (Tool::split); };
    penCursor = ToolIcons::cursor (ArrangeView::Tool::draw);
    scissorsCursor = ToolIcons::cursor (ArrangeView::Tool::split);
    quantizeButton.setTooltip ("QUANTIZE (Q): moves the selected notes' starts (all the notes, with none selected) to the grid");
    selectTool.onClick = [this] { setTool (Tool::select); };
    drawTool.onClick = [this] { setTool (Tool::draw); };
    eraseTool.onClick = [this] { setTool (Tool::erase); };
    quantizeButton.onClick = [this] { quantizeSelected(); };

    closeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
    closeButton.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
    closeButton.setWantsKeyboardFocus (false);
    closeButton.setTooltip ("Close the key editor (Escape)");
    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);

    windowButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
    windowButton.setColour (juce::TextButton::textColourOffId, ParagonColours::accent);
    windowButton.setWantsKeyboardFocus (false);
    windowButton.onClick = [this] { if (onWindowToggle) onWindowToggle(); };
    addAndMakeVisible (windowButton);
    setInWindow (false);

    for (int i = 0; i < (int) std::size (grids); ++i)
        gridBox.addItem (grids[i].name, i + 1);
    gridBox.setTooltip ("The grid: where notes snap to, a new note's length, and what QUANTIZE moves notes to");
    gridBox.setWantsKeyboardFocus (false);
    gridBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff151217));
    gridBox.setColour (juce::ComboBox::outlineColourId, ParagonColours::panelEdge);
    gridBox.setColour (juce::ComboBox::textColourId, ParagonColours::text);
    gridBox.setColour (juce::ComboBox::arrowColourId, ParagonColours::accent);
    gridBox.onChange = [this] { setGrid (grids[(size_t) juce::jlimit (0, (int) std::size (grids) - 1, gridBox.getSelectedId() - 1)].beats); };
    addAndMakeVisible (gridBox);

    for (const auto percent : { 100, 150, 200 })
        sizeBox.addItem (juce::String (percent) + "%", percent);
    sizeBox.setTooltip ("The editor's size: everything in it (text, keys, notes, buttons) drawn bigger");
    sizeBox.setWantsKeyboardFocus (false);
    sizeBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff151217));
    sizeBox.setColour (juce::ComboBox::outlineColourId, ParagonColours::panelEdge);
    sizeBox.setColour (juce::ComboBox::textColourId, ParagonColours::text);
    sizeBox.setColour (juce::ComboBox::arrowColourId, ParagonColours::accent);
    sizeBox.onChange = [this] { if (onSizeChange) onSizeChange (sizeBox.getSelectedId()); };
    addAndMakeVisible (sizeBox);

    autoScrollButton.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    autoScrollButton.setWantsKeyboardFocus (false);
    autoScrollButton.onClick = [this] { autoScroll.toggle(); autoScrollChanged(); };
    autoScroll.refreshKey (autoScrollButton);
    addAndMakeVisible (autoScrollButton);
    autoScrollMenu.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    autoScrollMenu.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    autoScrollMenu.setWantsKeyboardFocus (false);
    autoScrollMenu.setTooltip ("Auto-Scroll's settings: Page Scroll or Stationary Cursor; Suspend Auto-Scroll when Editing");
    autoScrollMenu.onClick = [this] { autoScroll.showMenu (autoScrollMenu, [this] { autoScrollChanged(); }); };
    addAndMakeVisible (autoScrollMenu);

    for (auto* bar : { &across, &down })
    {
        bar->setAutoHide (false);
        bar->setWantsKeyboardFocus (false);
        bar->addListener (this);
        addAndMakeVisible (*bar);
    }

    setWantsKeyboardFocus (false);
    setGrid (0.25);
    setTool (Tool::select);
    studio.addChangeListener (this);
    startTimerHz (30);
}

KeyEditor::~KeyEditor()
{
    stopAudition();
    studio.removeChangeListener (this);
}

//==============================================================================
void KeyEditor::setSizeShown (int percent)
{
    sizeBox.setSelectedId (percent, juce::dontSendNotification);
}

void KeyEditor::setTool (Tool newTool)
{
    tool = newTool;
    selectTool.setToggleState (tool == Tool::select, juce::dontSendNotification);
    drawTool.setToggleState (tool == Tool::draw, juce::dontSendNotification);
    eraseTool.setToggleState (tool == Tool::erase, juce::dontSendNotification);
    splitTool.setToggleState (tool == Tool::split, juce::dontSendNotification);
    // The tool as the pointer (0.9.3: the pencil, not crosshairs; the scissors).
    setMouseCursor (tool == Tool::draw ? penCursor : tool == Tool::split ? scissorsCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

void KeyEditor::setInWindow (bool shouldBeInWindow)
{
    inWindow = shouldBeInWindow;
    windowButton.setButtonText (inWindow ? "DOCK" : "WINDOW");
    windowButton.setTooltip (inWindow ? "Back under the arrangement"
                                      : "Into a window of its own: maximize it, or stretch it across your screens (it remembers where it was)");
}

void KeyEditor::setGrid (double beats)
{
    grid = beats;
    for (int i = 0; i < (int) std::size (grids); ++i)
        if (std::abs (grids[i].beats - beats) < 1.0e-9)
            gridBox.setSelectedId (i + 1, juce::dontSendNotification);
    repaint();
}

const pr7s::Part* KeyEditor::getPart() const
{
    if (const auto where = studio.findPart (partId))
        return &studio.getProject().findTrack (where->first)->parts[(size_t) where->second];
    return nullptr;
}

int KeyEditor::getTrackId() const
{
    const auto where = studio.findPart (partId);
    return where ? where->first : 0;
}

const std::vector<pr7s::Note>& KeyEditor::notes() const
{
    static const std::vector<pr7s::Note> none;
    if (editing())
        return working;
    const auto* part = getPart();
    return part != nullptr ? part->notes : none;
}

bool KeyEditor::editing() const noexcept
{
    return gesture == Gesture::move || gesture == Gesture::resize || gesture == Gesture::draw
        || gesture == Gesture::erase || gesture == Gesture::velocity;
}

//==============================================================================
void KeyEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

void KeyEditor::refresh()
{
    // A different part selected in the arrangement: show that one. (The
    // editor keeps its part when the arrangement's selection is cleared.)
    const auto selection = arrange.getSelection();
    if (selection.trackId != lastArrangeTrack || selection.index != lastArrangeIndex)
    {
        lastArrangeTrack = selection.trackId;
        lastArrangeIndex = selection.index;
        if (const auto* track = studio.getProject().findTrack (selection.trackId);
            track != nullptr && juce::isPositiveAndBelow (selection.index, (int) track->parts.size())
            && track->parts[(size_t) selection.index].id != partId
            && ! track->parts[(size_t) selection.index].isAudio())   // an audio clip has no notes: the editor stays on its part
        {
            partId = track->parts[(size_t) selection.index].id;
            selected.clear();
            gesture = Gesture::none;
            stopAudition();
            knownNotes = track->parts[(size_t) selection.index].notes;
            fitToPart();
        }
    }

    const auto* part = getPart();
    if (part == nullptr)
    {
        // Gone (deleted, or undone): the editor stays on it, in case undo brings it back.
        selected.clear();
        if (editing())
            gesture = Gesture::none;
    }
    else if (part->notes != knownNotes)
    {
        // Changed from elsewhere (undo, redo, recording into it): keep the
        // selection only if it still means the same notes.
        if (part->notes.size() != knownNotes.size())
            selected.clear();
        knownNotes = part->notes;
    }
    refreshPanel();
    repaint();
}

void KeyEditor::fitToPart()
{
    const auto* part = getPart();
    if (part == nullptr)
        return;
    const auto area = getNotesArea();
    const auto width = juce::jmax (100.0f, area.getWidth() - 20.0f);
    pixelsPerBeat = juce::jlimit (12.0, 400.0, (double) width / juce::jmax (1.0, part->length));
    firstBeat = part->start;

    // The notes in the middle, up and down (or round middle C for an empty part).
    auto lowest = 127, highest = 0;
    for (const auto& note : part->notes)
    {
        lowest = juce::jmin (lowest, note.pitch);
        highest = juce::jmax (highest, note.pitch);
    }
    const auto middle = part->notes.empty() ? 60.0f : (float) (lowest + highest) * 0.5f;
    scrollY = (127.0f - middle) * rowHeight - area.getHeight() * 0.5f + rowHeight * 0.5f;
    clampScroll();
}

void KeyEditor::clampScroll()
{
    const auto room = juce::jmax (0.0f, 128.0f * rowHeight - getNotesArea().getHeight());
    scrollY = juce::jlimit (0.0f, room, scrollY);
}

void KeyEditor::timerCallback()
{
    // A part selected by the arrangement since the last change message.
    const auto selection = arrange.getSelection();
    if (selection.trackId != lastArrangeTrack || selection.index != lastArrangeIndex)
        refresh();

    followPlayhead();
    refreshScrollBars();
    // The playhead's old and new strips (not the whole editor, 30 times a second: 0.9.2).
    if (const auto x = xForBeat (studio.getEngine().getPosition()); std::abs (x - playheadX) >= 0.05f)
    {
        const auto strip = [this] (float at) { return juce::Rectangle<float> (at - 7.0f, 0.0f, 15.0f, (float) getHeight()).toNearestIntEdges(); };
        repaint (strip (playheadX));
        repaint (strip (x));
        playheadX = x;
    }
}

void KeyEditor::followPlayhead()
{
    // Auto-Scroll (see AutoScroll): playing, or moved while stopped.
    auto& engine = studio.getEngine();
    const auto position = engine.getPosition();
    const auto jumped = position != lastPosition;
    lastPosition = position;
    if (autoScroll.suspended && ! engine.isPlaying())   // suspended until playback stops
    {
        autoScroll.suspended = false;
        autoScrollChanged();
    }
    if (gesture != Gesture::none)
        return;
    if (const auto first = autoScroll.follow (position, firstBeat, pixelsPerBeat, getNotesArea().getWidth(), engine.isPlaying(), jumped))
    {
        firstBeat = *first;
        repaint();   // (the page turned: the timer no longer redraws everything)
    }
}

void KeyEditor::zoomRows (bool in)
{
    const auto area = getNotesArea();
    const auto middle = (scrollY + area.getHeight() * 0.5f) / rowHeight;   // in rows from the top (pitch 127)
    rowHeight = juce::jlimit (minRowHeight, maxRowHeight, in ? rowHeight * (float) Timeline::rowZoomStep : rowHeight / (float) Timeline::rowZoomStep);
    scrollY = middle * rowHeight - area.getHeight() * 0.5f;
    clampScroll();
    refreshScrollBars();
    repaint();
}

void KeyEditor::zoom (bool in)
{
    const auto cursor = studio.getEngine().getPosition();
    pixelsPerBeat = juce::jlimit (12.0, 600.0, in ? pixelsPerBeat * Timeline::zoomStep : pixelsPerBeat / Timeline::zoomStep);
    firstBeat = juce::jmax (0.0, cursor - (double) getNotesArea().getWidth() * 0.5 / pixelsPerBeat);
    refreshScrollBars();
    repaint();
}

void KeyEditor::setAutoScroll (bool shouldScroll)
{
    autoScroll.on = shouldScroll;
    autoScroll.suspended = false;
    autoScrollChanged();
}

void KeyEditor::autoScrollChanged()
{
    autoScroll.refreshKey (autoScrollButton);
    if (onAutoScrollChange)
        onAutoScrollChange();
}

void KeyEditor::refreshScrollBars()
{
    // Across: the song from its start (as far as the arrangement's goes); down: the 128 notes.
    const auto area = getNotesArea();
    const auto visibleBeats = (double) area.getWidth() / pixelsPerBeat;
    across.setRangeLimits (0.0, juce::jmax (Timeline::scrollLength (studio), firstBeat + visibleBeats), juce::dontSendNotification);
    across.setCurrentRange (firstBeat, visibleBeats, juce::dontSendNotification);
    down.setRangeLimits (0.0, juce::jmax ((double) area.getHeight(), 128.0 * rowHeight), juce::dontSendNotification);
    down.setCurrentRange (scrollY, area.getHeight(), juce::dontSendNotification);
}

void KeyEditor::scrollBarMoved (juce::ScrollBar* bar, double newRangeStart)
{
    if (bar == &across)
        firstBeat = juce::jmax (0.0, newRangeStart);
    else
    {
        scrollY = (float) newRangeStart;
        clampScroll();
    }
    repaint();
}

//==============================================================================
juce::Rectangle<float> KeyEditor::rulerArea() const
{
    return { (float) (sideWidth + keysWidth), 0.0f, (float) (getWidth() - sideWidth - keysWidth), (float) rulerHeight };
}

juce::Rectangle<float> KeyEditor::keysArea() const
{
    return { (float) sideWidth, (float) rulerHeight, (float) keysWidth, (float) juce::jmax (0, getHeight() - rulerHeight - velocityHeight - scrollBarSize) };
}

juce::Rectangle<float> KeyEditor::getNotesArea() const
{
    // Room on the right for the scroll bar down, and at the bottom (under the velocities) for the one across.
    return { (float) (sideWidth + keysWidth), (float) rulerHeight, (float) (getWidth() - sideWidth - keysWidth - scrollBarSize),
             (float) juce::jmax (0, getHeight() - rulerHeight - velocityHeight - scrollBarSize) };
}

juce::Rectangle<float> KeyEditor::getVelocityArea() const
{
    return { (float) (sideWidth + keysWidth), (float) (getHeight() - velocityHeight - scrollBarSize), (float) (getWidth() - sideWidth - keysWidth),
             (float) velocityHeight };
}

float KeyEditor::xForBeat (double songBeat) const noexcept
{
    return (float) (sideWidth + keysWidth) + (float) ((songBeat - firstBeat) * pixelsPerBeat);
}

double KeyEditor::beatForX (float x) const noexcept
{
    return firstBeat + (x - (float) (sideWidth + keysWidth)) / pixelsPerBeat;
}

float KeyEditor::yForPitch (int pitch) const noexcept
{
    return (float) rulerHeight + (float) (127 - pitch) * rowHeight - scrollY;
}

int KeyEditor::pitchForY (float y) const noexcept
{
    return juce::jlimit (0, 127, 127 - (int) std::floor ((y - (float) rulerHeight + scrollY) / rowHeight));
}

juce::Rectangle<float> KeyEditor::getNoteBounds (int index) const
{
    const auto* part = getPart();
    const auto& list = notes();
    if (part == nullptr || ! juce::isPositiveAndBelow (index, (int) list.size()))
        return {};
    const auto& note = list[(size_t) index];
    const auto x = xForBeat (part->start + note.start);
    return { x, yForPitch (note.pitch) + 1.0f, juce::jmax (3.0f, (float) (note.length * pixelsPerBeat)), rowHeight - 1.0f };
}

int KeyEditor::noteAt (juce::Point<float> point) const
{
    // The one drawn on top (the last) first.
    for (int i = (int) notes().size() - 1; i >= 0; --i)
        if (getNoteBounds (i).contains (point))
            return i;
    return -1;
}

double KeyEditor::snapped (double songBeat, bool free) const noexcept
{
    return free ? songBeat : pr7s::snap (songBeat, grid);
}

//==============================================================================
void KeyEditor::selectNotes (std::vector<int> indices)
{
    std::sort (indices.begin(), indices.end());
    indices.erase (std::unique (indices.begin(), indices.end()), indices.end());
    const auto count = (int) notes().size();
    indices.erase (std::remove_if (indices.begin(), indices.end(), [count] (int i) { return ! juce::isPositiveAndBelow (i, count); }),
                   indices.end());
    selected = std::move (indices);
    refreshPanel();
    repaint();
}

void KeyEditor::selectAll()
{
    std::vector<int> all ((size_t) notes().size());
    std::iota (all.begin(), all.end(), 0);
    selectNotes (std::move (all));
}

void KeyEditor::begin()
{
    working = notes();
    original = working;
    moved = false;
}

void KeyEditor::commit (std::vector<pr7s::Note> edited)
{
    const auto where = studio.findPart (partId);
    if (! where)
        return;
    studio.setNotes (where->first, where->second, std::move (edited));
    if (const auto* part = getPart())
        knownNotes = part->notes;
    refreshPanel();
    repaint();
}

void KeyEditor::deleteSelected()
{
    if (selected.empty() || getPart() == nullptr)
        return;
    auto edited = getPart()->notes;
    for (auto it = selected.rbegin(); it != selected.rend(); ++it)
        if (juce::isPositiveAndBelow (*it, (int) edited.size()))
            edited.erase (edited.begin() + *it);
    selected.clear();
    commit (std::move (edited));
}

void KeyEditor::transposeSelected (int semitones)
{
    const auto* part = getPart();
    if (selected.empty() || part == nullptr)
        return;
    auto edited = part->notes;
    // As far as it goes while every selected note stays on the keyboard.
    auto lowest = 127, highest = 0;
    for (const auto i : selected)
    {
        lowest = juce::jmin (lowest, edited[(size_t) i].pitch);
        highest = juce::jmax (highest, edited[(size_t) i].pitch);
    }
    semitones = juce::jlimit (-lowest, 127 - highest, semitones);
    for (const auto i : selected)
        edited[(size_t) i].pitch += semitones;
    commit (std::move (edited));
    if (selected.size() == 1)
    {
        startAudition (part->notes[(size_t) selected.front()].pitch, part->notes[(size_t) selected.front()].velocity);
        juce::Timer::callAfterDelay (250, [safe = juce::Component::SafePointer<KeyEditor> (this)] { if (safe != nullptr) safe->stopAudition(); });
    }
}

bool KeyEditor::splitAt (int noteIndex, double beat)
{
    const auto* part = getPart();
    if (part == nullptr || ! juce::isPositiveAndBelow (noteIndex, (int) part->notes.size()))
        return false;
    // The clicked note; or, if it's selected with others, all the selected ones.
    std::vector<int> which { noteIndex };
    if (std::find (selected.begin(), selected.end(), noteIndex) != selected.end() && selected.size() > 1)
        which = selected;
    const auto at = beat - part->start;   // in the part
    auto edited = part->notes;
    std::vector<pr7s::Note> added;
    for (const auto i : which)
    {
        auto& note = edited[(size_t) i];
        if (at <= note.start + 1.0e-9 || at >= note.start + note.length - 1.0e-9)
            continue;   // the cut isn't inside this one
        auto second = note;
        second.start = at;
        second.length = note.start + note.length - at;
        note.length = at - note.start;
        added.push_back (second);
    }
    if (added.empty())
        return false;
    edited.insert (edited.end(), added.begin(), added.end());
    selected.clear();
    commit (std::move (edited));
    return true;
}

void KeyEditor::quantizeSelected()
{
    const auto* part = getPart();
    if (part == nullptr)
        return;
    auto edited = *part;
    pr7s::quantize (edited, grid, selected);
    commit (std::move (edited.notes));
}

void KeyEditor::copySelected()
{
    const auto* part = getPart();
    if (selected.empty() || part == nullptr)
        return;
    auto first = std::numeric_limits<double>::max();
    for (const auto i : selected)
        first = juce::jmin (first, part->notes[(size_t) i].start);
    clipboard.clear();
    for (const auto i : selected)
    {
        auto note = part->notes[(size_t) i];
        note.start -= first;
        clipboard.push_back (note);
    }
}

void KeyEditor::cutSelected()
{
    copySelected();
    deleteSelected();
}

void KeyEditor::paste()
{
    // At the playhead (as in Cubase). If that's outside the part, the part
    // grows (to whole bars) to hold them.
    if (clipboard.empty() || getPart() == nullptr)
        return;
    insertNotes (clipboard, studio.getEngine().getPosition());
}

void KeyEditor::duplicateSelected()
{
    // Ctrl+D: a copy of the selected notes straight after them, on the next
    // beat (or grid step, if that's coarser), so doing it again and again
    // repeats the phrase in time.
    const auto* part = getPart();
    if (selected.empty() || part == nullptr)
        return;
    auto first = std::numeric_limits<double>::max(), last = 0.0;
    std::vector<pr7s::Note> copies;
    for (const auto i : selected)
    {
        const auto& note = part->notes[(size_t) i];
        first = juce::jmin (first, note.start);
        last = juce::jmax (last, note.start + note.length);
    }
    for (const auto i : selected)
    {
        auto note = part->notes[(size_t) i];
        note.start -= first;
        copies.push_back (note);
    }
    const auto step = juce::jmax (grid, 4.0 / studio.getProject().denominator);
    insertNotes (copies, part->start + first + juce::jmax (step, std::ceil ((last - first) / step - 1.0e-9) * step));
}

void KeyEditor::insertNotes (const std::vector<pr7s::Note>& notesToAdd, double at)
{
    const auto* part = getPart();
    const auto where = studio.findPart (partId);
    if (part == nullptr || ! where || notesToAdd.empty())
        return;
    // Where they go in the song; the part grown to whole bars to hold them.
    auto first = std::numeric_limits<double>::max(), last = 0.0;
    for (const auto& note : notesToAdd)
    {
        first = juce::jmin (first, at + note.start);
        last = juce::jmax (last, at + note.start + note.length);
    }
    const auto bar = studio.getProject().beatsPerBar();
    auto grown = *part;
    if (first < part->start)
        grown.start = juce::jmax (0.0, std::floor (first / bar) * bar);
    if (last > part->end())
        grown.length = std::ceil (last / bar - 1.0e-9) * bar - grown.start;
    grown.length += part->start - grown.start;
    for (auto& note : grown.notes)
        note.start += part->start - grown.start;
    std::vector<int> added;
    for (auto note : notesToAdd)
    {
        note.start += at - grown.start;
        if (note.start < 0.0)
            continue;   // before the song's start
        grown.notes.push_back (note);
        added.push_back ((int) grown.notes.size() - 1);
    }
    if (added.empty())
        return;
    if (! juce::approximatelyEqual (grown.start, part->start) || ! juce::approximatelyEqual (grown.length, part->length))
        studio.updatePart (where->first, where->second, grown);   // the part and its notes: one undo step
    else
        studio.setNotes (where->first, where->second, grown.notes);
    if (const auto* now = getPart())
        knownNotes = now->notes;
    selectNotes (added);
    refreshPanel();
    repaint();
}

void KeyEditor::eraseAt (juce::Point<float> point)
{
    const auto hit = noteAt (point);
    if (hit < 0)
        return;
    working.erase (working.begin() + hit);
    moved = true;
    repaint();
}

void KeyEditor::setVelocities (float fromX, float toX, float y)
{
    const auto* part = getPart();
    if (part == nullptr)
        return;
    const auto area = getVelocityArea().reduced (0.0f, 6.0f);
    const auto value = juce::jlimit (1.0f / 127.0f, 1.0f, (area.getBottom() - y) / area.getHeight());
    const auto left = juce::jmin (fromX, toX) - 3.0f, right = juce::jmax (fromX, toX) + 3.0f;

    // The bars swept across; only the selected ones if any of those are among them.
    std::vector<int> under;
    auto anySelected = false;
    for (int i = 0; i < (int) working.size(); ++i)
    {
        const auto x = xForBeat (part->start + working[(size_t) i].start);
        if (x >= left && x <= right)
        {
            under.push_back (i);
            anySelected = anySelected || std::binary_search (selected.begin(), selected.end(), i);
        }
    }
    for (const auto i : under)
        if (! anySelected || std::binary_search (selected.begin(), selected.end(), i))
        {
            working[(size_t) i].velocity = value;
            moved = true;
        }
    shownVelocity = under.empty() ? -1.0f : value;
    repaint();
}

void KeyEditor::startAudition (int pitch, float velocity)
{
    // Heard through the track's instrument, as the on-screen keyboard is
    // (not while recording, or it would be recorded).
    stopAudition();
    if (studio.getEngine().isRecording())
        return;
    auditioning = pitch;
    studio.getEngine().getKeyboardState().noteOn (1, pitch, juce::jlimit (0.05f, 1.0f, velocity));
}

void KeyEditor::stopAudition()
{
    if (auditioning >= 0)
        studio.getEngine().getKeyboardState().noteOff (1, auditioning, 0.0f);
    auditioning = -1;
}

//==============================================================================
bool KeyEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const auto code = key.getKeyCode();   // a letter only if it is one (see MainView::keyPressed)
    const auto letter = (code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z')
                          ? juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) code) : (juce::juce_wchar) 0;
    if (mods.isCommandDown() && letter == 'A')                                   { selectAll(); return true; }
    if (mods.isCommandDown() && letter == 'C')                                   { copySelected(); return true; }
    if (mods.isCommandDown() && letter == 'X')                                   { cutSelected(); return true; }
    if (mods.isCommandDown() && letter == 'V')                                   { paste(); return true; }
    if (mods.isCommandDown() && letter == 'D')                                   { duplicateSelected(); return true; }
    if (mods.isCommandDown())
        return false;
    if (mods.isShiftDown() && letter == 'A')                                     { selectAll(); return true; }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) { deleteSelected(); return true; }
    if (key.getKeyCode() == juce::KeyPress::upKey)                               { transposeSelected (mods.isShiftDown() ? 12 : 1); return true; }
    if (key.getKeyCode() == juce::KeyPress::downKey)                             { transposeSelected (mods.isShiftDown() ? -12 : -1); return true; }
    if (letter == 'Q')                                                           { quantizeSelected(); return true; }
    // Zoom and Auto-Scroll (the manual's keys).
    if (! mods.isAltDown() && (letter == 'H' || letter == 'G'))
    {
        mods.isShiftDown() ? zoomRows (letter == 'H') : zoom (letter == 'H');
        return true;
    }
    if (! mods.isAltDown() && letter == 'F')
    {
        if (mods.isShiftDown())
            fitToPart();
        else
        {
            autoScroll.toggle();
            autoScrollChanged();
        }
        repaint();
        return true;
    }
    if (key.getTextCharacter() == '1')                                           { setTool (Tool::select); return true; }
    if (key.getTextCharacter() == '8')                                           { setTool (Tool::draw); return true; }
    if (key.getTextCharacter() == '5')                                           { setTool (Tool::erase); return true; }
    if (key.getTextCharacter() == '3')                                           { setTool (Tool::split); return true; }
    return false;
}

//==============================================================================
void KeyEditor::mouseMove (const juce::MouseEvent& event)
{
    if (tool == Tool::select)
    {
        const auto hit = noteAt (event.position);
        const auto atEnd = hit >= 0 && event.position.x > getNoteBounds (hit).getRight() - juce::jmin (edgeGrab, getNoteBounds (hit).getWidth() / 3.0f);
        setMouseCursor (atEnd ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }
}

void KeyEditor::mouseDown (const juce::MouseEvent& event)
{
    const auto point = event.position;
    downAt = point;
    moved = false;
    gesture = Gesture::none;
    // Suspend Auto-Scroll when Editing: a click in the notes or the velocities while playing.
    if (autoScroll.on && autoScroll.suspendWhenEditing && ! autoScroll.suspended && studio.getEngine().isPlaying()
        && (getNotesArea().contains (point) || getVelocityArea().contains (point)))
    {
        autoScroll.suspended = true;
        autoScrollChanged();
    }

    if (rulerArea().contains (point))
    {
        gesture = Gesture::position;
        studio.setPosition (juce::jmax (0.0, pr7s::snap (beatForX (point.x), grid)));
        return;
    }
    const auto* part = getPart();
    if (part == nullptr)
        return;
    if (keysArea().contains (point))
    {
        gesture = Gesture::audition;
        startAudition (pitchForY (point.y), newVelocity);
        return;
    }
    if (getVelocityArea().contains (point))
    {
        begin();
        gesture = Gesture::velocity;
        lastVelocityX = point.x;
        setVelocities (point.x, point.x, point.y);
        return;
    }
    if (! getNotesArea().contains (point))
        return;

    const auto hit = noteAt (point);
    const auto free = event.mods.isCommandDown();   // Ctrl: no snapping
    switch (tool)
    {
        case Tool::erase:
            begin();
            selected.clear();
            gesture = Gesture::erase;
            eraseAt (point);
            return;

        case Tool::split:
            if (hit >= 0)
                splitAt (hit, free ? beatForX (point.x) : pr7s::snap (beatForX (point.x), grid));
            return;

        case Tool::draw:
        {
            if (hit >= 0)
            {
                // The pencil on a note removes it.
                auto edited = part->notes;
                edited.erase (edited.begin() + hit);
                selected.clear();
                commit (std::move (edited));
                return;
            }
            // A new note from the grid step clicked in, a step long; only inside the part (outside it wouldn't be heard).
            const auto at = beatForX (point.x);
            const auto start = free ? at : std::floor (at / grid + 1.0e-9) * grid;
            if (start < part->start - 1.0e-9 || start >= part->end())
                return;
            begin();
            gesture = Gesture::draw;
            working.push_back ({ start - part->start, free ? juce::jmax (finest, grid) : grid, pitchForY (point.y), newVelocity });
            anchor = (int) working.size() - 1;
            selected = { anchor };
            moved = true;
            startAudition (working.back().pitch, newVelocity);
            repaint();
            return;
        }

        case Tool::select:
            break;
    }

    if (hit < 0)
    {
        // A group, by dragging a box round it (Shift or Ctrl: added to what's selected).
        gesture = Gesture::band;
        selectionBefore = event.mods.isShiftDown() || event.mods.isCommandDown() ? selected : std::vector<int>();
        selected = selectionBefore;
        band = { point, point };
        refreshPanel();
        repaint();
        return;
    }
    if (event.mods.isShiftDown() || event.mods.isCommandDown())
    {
        auto now = selected;
        if (const auto it = std::find (now.begin(), now.end(), hit); it != now.end())
            now.erase (it);
        else
            now.push_back (hit);
        selectNotes (now);
        return;
    }
    if (! std::binary_search (selected.begin(), selected.end(), hit))
        selected = { hit };

    begin();
    clicked = anchor = hit;
    const auto bounds = getNoteBounds (hit);
    gesture = point.x > bounds.getRight() - juce::jmin (edgeGrab, bounds.getWidth() / 3.0f) ? Gesture::resize : Gesture::move;
    dragging = selected;
    if (gesture == Gesture::move && event.mods.isAltDown())
    {
        // Alt: the copies move; the originals stay.
        std::vector<int> copies;
        for (const auto i : selected)
        {
            working.push_back (working[(size_t) i]);
            copies.push_back ((int) working.size() - 1);
            if (i == hit)
                anchor = copies.back();
        }
        original = working;
        dragging = copies;
        selected = copies;
    }
    startAudition (working[(size_t) anchor].pitch, working[(size_t) anchor].velocity);
    refreshPanel();
    repaint();
}

void KeyEditor::mouseDrag (const juce::MouseEvent& event)
{
    const auto point = event.position;
    // Ctrl: no snapping (while moving notes, Ctrl+Shift: Ctrl alone keeps them to one direction).
    const auto free = event.mods.isCommandDown() && (gesture != Gesture::move || event.mods.isShiftDown());
    const auto* part = getPart();
    switch (gesture)
    {
        case Gesture::position:
            studio.setPosition (juce::jmax (0.0, pr7s::snap (beatForX (point.x), grid)));
            return;

        case Gesture::audition:
            if (const auto pitch = pitchForY (point.y); pitch != auditioning)
                startAudition (pitch, newVelocity);
            return;

        case Gesture::band:
        {
            band = juce::Rectangle<float> (downAt, point);
            auto now = selectionBefore;
            for (int i = 0; i < (int) notes().size(); ++i)
                if (getNoteBounds (i).intersects (band))
                    now.push_back (i);
            selectNotes (now);
            return;
        }

        case Gesture::move:
        {
            if (part == nullptr || (! moved && event.getDistanceFromDragStart() < 3))
                return;
            moved = true;
            const auto& a = original[(size_t) anchor];
            const auto from = part->start + a.start;
            auto shift = snapped (from + beatForX (point.x) - beatForX (downAt.x), free) - from;
            auto semitones = pitchForY (point.y) - pitchForY (downAt.y);
            if (event.mods.isCommandDown() && ! event.mods.isShiftDown())
            {
                // Ctrl: only along the song, or only up or down, whichever the mouse has gone further.
                const auto offset = point - downAt;
                if (std::abs (offset.x) >= std::abs (offset.y))
                    semitones = 0;
                else
                    shift = 0.0;
            }
            // Kept inside the part and on the keyboard, all together.
            auto earliest = part->length, latest = 0.0;
            auto lowest = 127, highest = 0;
            for (const auto i : dragging)
            {
                const auto& n = original[(size_t) i];
                earliest = juce::jmin (earliest, n.start);
                latest = juce::jmax (latest, n.start);
                lowest = juce::jmin (lowest, n.pitch);
                highest = juce::jmax (highest, n.pitch);
            }
            shift = juce::jmax (-earliest, juce::jmin (shift, part->length - finest - latest));
            semitones = juce::jlimit (-lowest, 127 - highest, semitones);
            for (const auto i : dragging)
            {
                working[(size_t) i] = original[(size_t) i];
                working[(size_t) i].start += shift;
                working[(size_t) i].pitch += semitones;
            }
            if (working[(size_t) anchor].pitch != auditioning)
                startAudition (working[(size_t) anchor].pitch, working[(size_t) anchor].velocity);
            refreshPanel();
            repaint();
            return;
        }

        case Gesture::resize:
        {
            if (part == nullptr)
                return;
            moved = true;
            const auto& a = original[(size_t) anchor];
            const auto end = part->start + a.start + a.length;
            const auto change = snapped (end + beatForX (point.x) - beatForX (downAt.x), free) - end;
            const auto shortest = free ? finest : grid;
            for (const auto i : dragging)
                working[(size_t) i].length = juce::jmax (shortest, original[(size_t) i].length + change);
            refreshPanel();
            repaint();
            return;
        }

        case Gesture::draw:
        {
            if (part == nullptr)
                return;
            auto& n = working[(size_t) anchor];
            const auto start = part->start + n.start;
            const auto end = free ? beatForX (point.x) : std::ceil (beatForX (point.x) / grid - 1.0e-9) * grid;
            n.length = juce::jmax (free ? finest : grid, end - start);
            refreshPanel();
            repaint();
            return;
        }

        case Gesture::erase:
            eraseAt (point);
            return;

        case Gesture::velocity:
            setVelocities (lastVelocityX, point.x, point.y);
            lastVelocityX = point.x;
            return;

        case Gesture::none:
            return;
    }
}

void KeyEditor::mouseUp (const juce::MouseEvent&)
{
    const auto finished = gesture;
    gesture = Gesture::none;
    stopAudition();
    shownVelocity = -1.0f;
    band = {};

    switch (finished)
    {
        case Gesture::move:
            if (! moved)
            {
                // A click on a note in a group (no drag): just that note.
                selected = { clicked };
                break;
            }
            selected = dragging;
            commit (working);
            break;
        case Gesture::resize:
        case Gesture::draw:
        case Gesture::erase:
        case Gesture::velocity:
            if (moved)
                commit (working);
            if (finished == Gesture::draw && getPart() != nullptr)
                selected = { anchor };
            break;
        case Gesture::position:
        case Gesture::audition:
        case Gesture::band:
        case Gesture::none:
            break;
    }
    selectNotes (selected);
}

void KeyEditor::mouseDoubleClick (const juce::MouseEvent& event)
{
    // SELECT: a double-click on a note deletes it; on empty space, draws a note there.
    const auto* part = getPart();
    if (tool != Tool::select || part == nullptr || ! getNotesArea().contains (event.position))
        return;
    if (const auto hit = noteAt (event.position); hit >= 0)
    {
        auto edited = part->notes;
        edited.erase (edited.begin() + hit);
        selected.clear();
        commit (std::move (edited));
        return;
    }
    const auto at = std::floor (beatForX (event.position.x) / grid + 1.0e-9) * grid;
    if (at < part->start - 1.0e-9 || at >= part->end())
        return;
    auto edited = part->notes;
    edited.push_back ({ at - part->start, grid, pitchForY (event.position.y), newVelocity });
    commit (std::move (edited));
    selectNotes ({ (int) getPart()->notes.size() - 1 });
}

void KeyEditor::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if ((event.mods.isCommandDown() || event.mods.isCtrlDown()) && event.mods.isShiftDown())
    {
        zoomRows (wheel.deltaY > 0.0f);   // Ctrl+Shift+wheel: taller or shorter rows (the manual's)
        return;
    }
    if (event.mods.isCommandDown() || event.mods.isCtrlDown())
    {
        // Zoom round the mouse: the beat under it stays put.
        const auto x = juce::jmax (getNotesArea().getX(), event.position.x);
        const auto beat = beatForX (x);
        pixelsPerBeat = juce::jlimit (12.0, 600.0, pixelsPerBeat * (wheel.deltaY > 0.0f ? 1.25 : 0.8));
        firstBeat = juce::jmax (0.0, beat - (x - getNotesArea().getX()) / pixelsPerBeat);
    }
    else if (event.mods.isShiftDown() || std::abs (wheel.deltaX) > std::abs (wheel.deltaY))
    {
        const auto delta = std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? wheel.deltaX : wheel.deltaY;
        firstBeat = juce::jmax (0.0, firstBeat - delta * 400.0 / pixelsPerBeat);
    }
    else
    {
        scrollY -= wheel.deltaY * 300.0f;
        clampScroll();
    }
    refreshScrollBars();
    repaint();
}

//==============================================================================
void KeyEditor::refreshPanel()
{
    const auto hasPart = getPart() != nullptr;
    for (auto* button : { &selectTool, &drawTool, &eraseTool, &quantizeButton })
        button->setEnabled (hasPart);
    gridBox.setEnabled (hasPart);
    repaint (0, 0, sideWidth, getHeight());
}

void KeyEditor::resized()
{
    auto panel = juce::Rectangle<int> (0, 0, sideWidth - 2, getHeight()).withTrimmedLeft (12).reduced (6, 8);
    auto top = panel.removeFromTop (40);
    closeButton.setBounds (top.removeFromRight (64).withSizeKeepingCentre (64, 24).withY (top.getY()));
    top.removeFromRight (6);
    windowButton.setBounds (top.removeFromRight (74).withSizeKeepingCentre (74, 24).withY (top.getY()));
    panel.removeFromTop (8);
    auto tools = panel.removeFromTop (28);
    const auto toolWidth = (tools.getWidth() - 12) / 4;
    selectTool.setBounds (tools.removeFromLeft (toolWidth));
    tools.removeFromLeft (4);
    drawTool.setBounds (tools.removeFromLeft (toolWidth));
    tools.removeFromLeft (4);
    splitTool.setBounds (tools.removeFromLeft (toolWidth));
    tools.removeFromLeft (4);
    eraseTool.setBounds (tools);
    panel.removeFromTop (10);
    auto gridRow = panel.removeFromTop (26);
    gridRow.removeFromLeft (42);   // "GRID"
    gridBox.setBounds (gridRow.removeFromLeft (122));
    gridRow.removeFromLeft (6);
    quantizeButton.setBounds (gridRow);
    panel.removeFromTop (8);
    auto autoScrollRow = panel.removeFromTop (24).withTrimmedLeft (42);
    autoScrollButton.setBounds (autoScrollRow.removeFromLeft (96));
    autoScrollRow.removeFromLeft (3);
    autoScrollMenu.setBounds (autoScrollRow.removeFromLeft (22));
    autoScrollRow.removeFromLeft (10);
    sizeBox.setBounds (autoScrollRow.removeFromRight (juce::jmin (72, autoScrollRow.getWidth())));

    const auto notesArea = getNotesArea().toNearestInt();
    down.setBounds (notesArea.getRight(), notesArea.getY(), scrollBarSize, notesArea.getHeight());
    across.setBounds (notesArea.getX() - keysWidth, getHeight() - scrollBarSize, getWidth() - sideWidth, scrollBarSize);
    clampScroll();
    refreshScrollBars();
}

void KeyEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0a0e));
    paintPanel (g);
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (getNotesArea().getSmallestIntegerContainer());
        paintNotes (g);
    }
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (keysArea().getSmallestIntegerContainer());
        paintKeys (g);
    }
    paintVelocities (g);
    paintRuler (g);

    // The playhead, down the notes and the velocities.
    const auto x = xForBeat (studio.getEngine().getPosition());
    if (x >= getNotesArea().getX() - 2.0f && x <= (float) getWidth() + 2.0f)
    {
        const auto colour = studio.getEngine().isRecording() ? juce::Colour (0xffff3b2f) : ParagonColours::accent;
        g.setColour (colour.withAlpha (0.25f));
        g.fillRect (juce::Rectangle<float> (x - 2.0f, (float) rulerHeight, 5.0f, (float) (getHeight() - rulerHeight)));
        g.setColour (colour);
        g.fillRect (juce::Rectangle<float> (x, (float) rulerHeight - 6.0f, 1.2f, (float) (getHeight() - rulerHeight + 6)));
        juce::Path marker;
        marker.addTriangle (x - 5.0f, (float) rulerHeight - 11.0f, x + 5.0f, (float) rulerHeight - 11.0f, x, (float) rulerHeight - 2.0f);
        g.fillPath (marker);
    }

    // The editor's frame and the line between the notes and the velocities.
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (juce::Rectangle<float> ((float) sideWidth, getVelocityArea().getY() - 1.0f, (float) (getWidth() - sideWidth), 1.0f));
    g.fillRect (juce::Rectangle<float> (0.0f, 0.0f, (float) getWidth(), 1.0f));
}

void KeyEditor::paintPanel (juce::Graphics& g)
{
    const auto panel = juce::Rectangle<float> (0.0f, 0.0f, (float) sideWidth - 2.0f, (float) getHeight());
    g.setColour (juce::Colour (0xff100c11));
    g.fillRect (panel);
    const auto* part = getPart();
    const auto* track = studio.getProject().findTrack (getTrackId());
    const auto colour = track != nullptr ? track->colour : ParagonColours::textDim;
    g.setColour (colour);
    g.fillRect (panel.withWidth (6.0f).reduced (0.0f, 1.0f));

    auto area = panel.withTrimmedLeft (18.0f).reduced (0.0f, 8.0f).withTrimmedRight (156.0f);
    g.setColour (ParagonColours::accent);
    g.setFont (ParagonLookAndFeel::titleFont (9.5f));
    g.drawText ("KEY EDITOR", area.removeFromTop (12.0f), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (15.0f));
    g.drawText (part != nullptr ? (part->name.isNotEmpty() ? part->name : juce::String ("Part")) : juce::String ("No part"),
                area.removeFromTop (20.0f), juce::Justification::centredLeft, true);
    g.setColour (ParagonColours::textDim);
    g.setFont (font (11.0f, juce::Font::plain));
    if (track != nullptr && part != nullptr)
        g.drawText (track->name + ", from bar " + juce::String ((int) std::floor (part->start / studio.getProject().beatsPerBar()) + 1),
                    area.removeFromTop (14.0f), juce::Justification::centredLeft, true);

    g.setColour (ParagonColours::textDim);
    g.setFont (font (10.0f));
    g.drawText ("GRID", juce::Rectangle<float> (18.0f, (float) gridBox.getY(), 40.0f, (float) gridBox.getHeight()),
                juce::Justification::centredLeft, false);

    // What's selected.
    auto info = panel.withTrimmedLeft (18.0f).withTrimmedRight (12.0f).withTrimmedTop ((float) autoScrollButton.getBottom() + 14.0f);
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (info.withHeight (1.0f));
    info.removeFromTop (10.0f);
    if (part == nullptr)
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (font (12.0f, juce::Font::plain));
        g.drawFittedText ("Select a part in the arrangement (double-click it) to edit its notes.", info.removeFromTop (40.0f).toNearestInt(),
                          juce::Justification::topLeft, 3);
        return;
    }
    const auto& list = notes();
    juce::String line1, line2;
    if (selected.size() == 1 && juce::isPositiveAndBelow (selected.front(), (int) list.size()))
    {
        const auto& n = list[(size_t) selected.front()];
        line1 = pr7s::noteName (n.pitch) + "    velocity " + juce::String (juce::roundToInt (n.velocity * 127.0f));
        line2 = "at " + pr7s::formatBarsBeats (part->start + n.start, studio.getProject()).replace ("  ", " ")
              + ",  " + describeLength (n.length);
    }
    else if (! selected.empty())
    {
        line1 = juce::String ((int) selected.size()) + " notes selected";
        line2 = "of " + juce::String ((int) list.size());
    }
    else
    {
        line1 = list.empty() ? juce::String ("No notes yet") : juce::String ((int) list.size()) + (list.size() == 1 ? " note" : " notes");
        line2 = tool == Tool::draw ? "Click to draw a note" : tool == Tool::erase ? "Click or drag across notes to erase"
                                                                                 : "Double-click for a new note";
    }
    g.setColour (ParagonColours::text);
    g.setFont (font (13.0f));
    g.drawText (line1, info.removeFromTop (18.0f), juce::Justification::centredLeft, true);
    g.setColour (ParagonColours::textDim);
    g.setFont (font (11.5f, juce::Font::plain));
    g.drawText (line2, info.removeFromTop (16.0f), juce::Justification::centredLeft, true);
}

void KeyEditor::paintKeys (juce::Graphics& g)
{
    // A piano on its side: white keys full width, black keys shorter, C's named.
    const auto area = keysArea();
    auto& keyboard = studio.getEngine().getKeyboardState();
    g.setColour (juce::Colour (0xffe9e4e7));
    g.fillRect (area);
    for (int pitch = pitchForY (area.getBottom()); pitch <= pitchForY (area.getY()); ++pitch)
    {
        const auto row = juce::Rectangle<float> (area.getX(), yForPitch (pitch), area.getWidth(), rowHeight);
        const auto lit = keyboard.isNoteOnForChannels (0xffff, pitch);
        if (isBlackKey (pitch))
        {
            const auto key = row.withWidth (area.getWidth() * 0.6f).reduced (0.0f, 0.5f);
            g.setColour (lit ? ParagonColours::accent : juce::Colour (0xff1a1519));
            g.fillRect (key);
            continue;
        }
        if (lit)
        {
            g.setColour (ParagonColours::accent);
            g.fillRect (row.reduced (0.0f, 0.5f));
        }
        const auto inOctave = pitch % 12;
        if (inOctave == 0 || inOctave == 5)   // the gap under C and F, between white keys with no black key between
        {
            g.setColour (juce::Colour (0xffa59ea2));
            g.fillRect (row.withY (row.getBottom() - 1.0f).withHeight (1.0f));
        }
        if (inOctave == 0)
        {
            g.setColour (ParagonColours::ink);
            g.setFont (font (9.0f));
            g.drawText (pr7s::noteName (pitch), row.withTrimmedRight (4.0f), juce::Justification::centredRight, false);
        }
    }
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRect (area.withX (area.getRight() - 1.0f).withWidth (1.0f));
}

void KeyEditor::paintNotes (juce::Graphics& g)
{
    const auto area = getNotesArea();
    const auto& project = studio.getProject();

    // The rows: darker for the black keys, a line at every C.
    for (int pitch = pitchForY (area.getBottom()); pitch <= pitchForY (area.getY()); ++pitch)
    {
        const auto row = juce::Rectangle<float> (area.getX(), yForPitch (pitch), area.getWidth(), rowHeight);
        g.setColour (isBlackKey (pitch) ? blackRow : whiteRow);
        g.fillRect (row);
        if (pitch % 12 == 0)
        {
            g.setColour (octaveLine);
            g.fillRect (row.withY (row.getBottom() - 1.0f).withHeight (1.0f));
        }
    }

    // Bars, beats and the grid's steps (each when there's room to see it).
    const auto beatUnit = 4.0 / project.denominator;
    const auto bar = project.beatsPerBar();
    const auto last = beatForX (area.getRight());
    if (grid * pixelsPerBeat >= 6.0)
        for (auto beat = std::floor (firstBeat / grid) * grid; beat <= last; beat += grid)
        {
            g.setColour (gridStep);
            g.fillRect (juce::Rectangle<float> (xForBeat (beat), area.getY(), 1.0f, area.getHeight()));
        }
    for (auto beat = std::floor (firstBeat / beatUnit) * beatUnit; beat <= last; beat += beatUnit)
    {
        const auto isBar = std::abs (std::fmod (beat, bar)) < 1.0e-6;
        if (! isBar && beatUnit * pixelsPerBeat < 10.0)
            continue;
        g.setColour (isBar ? gridBar : gridBeat);
        g.fillRect (juce::Rectangle<float> (xForBeat (beat), area.getY(), 1.0f, area.getHeight()));
    }

    const auto* part = getPart();
    const auto* track = project.findTrack (getTrackId());
    if (part == nullptr || track == nullptr)
        return;

    // Outside the part: shaded (notes there aren't heard).
    const auto partLeft = xForBeat (part->start), partRight = xForBeat (part->end());
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRect (juce::Rectangle<float> (area.getX(), area.getY(), juce::jmax (0.0f, partLeft - area.getX()), area.getHeight()));
    g.fillRect (juce::Rectangle<float> (partRight, area.getY(), juce::jmax (0.0f, area.getRight() - partRight), area.getHeight()));
    g.setColour (track->colour.withAlpha (0.7f));
    g.fillRect (juce::Rectangle<float> (partLeft, area.getY(), 1.5f, area.getHeight()));
    g.fillRect (juce::Rectangle<float> (partRight - 1.5f, area.getY(), 1.5f, area.getHeight()));

    // The notes: the track's colour, brighter the harder they're played; selected ones outlined.
    const auto& list = notes();
    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto bounds = getNoteBounds (i);
        if (bounds.getRight() < area.getX() || bounds.getX() > area.getRight() || bounds.getBottom() < area.getY() || bounds.getY() > area.getBottom())
            continue;
        const auto& note = list[(size_t) i];
        const auto isSelected = std::binary_search (selected.begin(), selected.end(), i);
        const auto fill = track->colour.interpolatedWith (juce::Colour (0xff2a1f26), (1.0f - note.velocity) * 0.65f);
        g.setColour (isSelected ? fill.brighter (0.35f) : fill);
        g.fillRoundedRectangle (bounds, 2.0f);
        g.setColour (isSelected ? juce::Colours::white : juce::Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, isSelected ? 1.5f : 1.0f);
        if (bounds.getWidth() > 30.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.75f));
            g.setFont (font (9.0f));
            g.drawText (pr7s::noteName (note.pitch), bounds.withTrimmedLeft (4.0f), juce::Justification::centredLeft, false);
        }
    }

    if (gesture == Gesture::band && ! band.isEmpty())
    {
        g.setColour (ParagonColours::accent.withAlpha (0.12f));
        g.fillRect (band);
        g.setColour (ParagonColours::accent.withAlpha (0.8f));
        g.drawRect (band, 1.0f);
    }
}

void KeyEditor::paintVelocities (juce::Graphics& g)
{
    const auto area = getVelocityArea();
    g.setColour (juce::Colour (0xff0b090c));
    g.fillRect (area);
    const auto label = juce::Rectangle<float> ((float) sideWidth, area.getY(), (float) keysWidth, area.getHeight());
    g.setColour (juce::Colour (0xff100c11));
    g.fillRect (label);
    g.setColour (ParagonColours::textDim);
    g.setFont (font (8.5f));
    g.drawText ("VELOCITY", label.reduced (2.0f, 4.0f), juce::Justification::centredBottom, false);
    g.setFont (font (8.5f, juce::Font::plain));
    const auto inner = area.reduced (0.0f, 6.0f);
    for (const auto level : { 127, 64 })
    {
        const auto y = inner.getBottom() - inner.getHeight() * (float) level / 127.0f;
        g.setColour (ParagonColours::panelEdge.withAlpha (0.7f));
        g.fillRect (juce::Rectangle<float> (area.getX(), y, area.getWidth(), 1.0f));
        g.setColour (ParagonColours::textDim);
        g.drawText (juce::String (level), juce::Rectangle<float> (label.getX(), y - 6.0f, label.getWidth() - 6.0f, 12.0f),
                    juce::Justification::centredRight, false);
    }

    const auto* part = getPart();
    const auto* track = studio.getProject().findTrack (getTrackId());
    if (part == nullptr || track == nullptr)
        return;
    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (area.getSmallestIntegerContainer());
    const auto& list = notes();
    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto& note = list[(size_t) i];
        const auto x = xForBeat (part->start + note.start);
        if (x < area.getX() - 4.0f || x > area.getRight())
            continue;
        const auto isSelected = std::binary_search (selected.begin(), selected.end(), i);
        const auto height = inner.getHeight() * note.velocity;
        const auto bar = juce::Rectangle<float> (x, inner.getBottom() - height, 4.0f, height);
        g.setColour ((isSelected ? track->colour.brighter (0.35f) : track->colour).withAlpha (0.9f));
        g.fillRect (bar);
        g.setColour (isSelected ? juce::Colours::white : track->colour.brighter (0.6f));
        g.fillRect (bar.withHeight (2.0f).withWidth (juce::jmax (4.0f, juce::jmin (12.0f, (float) (note.length * pixelsPerBeat) - 2.0f))));
    }
    if (shownVelocity >= 0.0f)
    {
        g.setColour (ParagonColours::text);
        g.setFont (font (11.0f));
        g.drawText (juce::String (juce::roundToInt (shownVelocity * 127.0f)),
                    juce::Rectangle<float> (lastVelocityX + 8.0f, area.getY() + 4.0f, 40.0f, 14.0f), juce::Justification::centredLeft, false);
    }
}

void KeyEditor::paintRuler (juce::Graphics& g)
{
    const auto& project = studio.getProject();
    const auto ruler = juce::Rectangle<float> ((float) sideWidth, 0.0f, (float) (getWidth() - sideWidth), (float) rulerHeight);
    g.setColour (juce::Colour (0xff151116));
    g.fillRect (ruler);
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (ruler.withY (ruler.getBottom() - 1.0f).withHeight (1.0f));

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (rulerArea().getSmallestIntegerContainer());
    const auto bar = project.beatsPerBar();
    const auto beatUnit = 4.0 / project.denominator;
    const auto last = beatForX ((float) getWidth());
    if (beatUnit * pixelsPerBeat >= 10.0)
        for (auto beat = std::floor (firstBeat / beatUnit) * beatUnit; beat <= last; beat += beatUnit)
        {
            g.setColour (ParagonColours::panelEdge);
            g.fillRect (juce::Rectangle<float> (xForBeat (beat), ruler.getBottom() - 6.0f, 1.0f, 6.0f));
        }
    auto every = 1;
    while (bar * every * pixelsPerBeat < 36.0)
        every *= 2;
    g.setFont (font (11.0f));
    for (auto b = (int) std::floor (firstBeat / bar); b * bar <= last; ++b)
    {
        if (b % every != 0)
            continue;
        const auto x = xForBeat (b * bar);
        g.setColour (ParagonColours::textDim);
        g.fillRect (juce::Rectangle<float> (x, 8.0f, 1.0f, ruler.getBottom() - 8.0f));
        g.setColour (ParagonColours::text);
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (x + 4.0f, 6.0f, 40.0f, 16.0f), juce::Justification::centredLeft, false);
    }
}
