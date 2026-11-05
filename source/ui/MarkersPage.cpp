#include "ui/MarkersPage.h"

#include "ui/InfoEntry.h"
#include "ui/KeyEditor.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/TempoEditor.h"

namespace
{
    const juce::Colour pageColour { 0xff0d0a0e };

    juce::String positionText (double beat, const pr7s::Project& project)
    {
        return pr7s::formatBarsBeats (beat, project).replace ("  ", " ");
    }
}

MarkersPage::MarkersPage (pr7s::Studio& s) : studio (s)
{
    const auto style = [this] (juce::TextButton& key, const juce::String& tip, std::function<void()> action)
    {
        key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key.setWantsKeyboardFocus (false);
        key.setTooltip (tip);
        key.onClick = std::move (action);
        addAndMakeVisible (key);
    };
    style (addPositionKey, "Add Position Marker: at the playhead", [this] { addPosition(); });
    style (addCycleKey, "Add Cycle Marker: over the locators' range", [this] { addCycle(); });
    style (removeKey, "Remove Marker: the selected ones", [this] { removeSelected(); });
    style (functionsKey, "Move Markers to Cursor; Reassign Position / Cycle Marker IDs (in the order they're in)", [this] { showFunctionsMenu(); });
    functionsKey.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    style (autoScrollKey, "Auto-Scroll with Project Cursor: the list keeps the locate arrow in view", [this]
    {
        autoScroll = ! autoScroll;
        refreshKeys();
    });
    style (idsKey, "Show Marker IDs on the Marker Track", [this]
    {
        const auto& t = studio.getMarkerTrack();
        studio.setMarkerSettings (t.showLines, ! t.showIds, t.cycleFollows);
    });
    style (followsKey, "Cycle Follows When Locating to Markers: going to a marker sets the locators to it", [this]
    {
        const auto& t = studio.getMarkerTrack();
        studio.setMarkerSettings (t.showLines, t.showIds, ! t.cycleFollows);
    });
    style (linesKey, "Show Marker Lines down the arrangement", [this]
    {
        const auto& t = studio.getMarkerTrack();
        studio.setMarkerSettings (! t.showLines, t.showIds, t.cycleFollows);
    });

    auto& header = table.getHeader();
    const auto flags = juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable | juce::TableHeaderComponent::sortable;
    header.addColumn ("", locate, 28, 28, 28, juce::TableHeaderComponent::visible);
    header.addColumn ("ID", id, 52, 40, 90, flags);
    header.addColumn ("TYPE", type, 80, 60, 120, flags);
    header.addColumn ("POSITION", position, 120, 80, 200, flags);
    header.addColumn ("END", end, 120, 80, 200, flags);
    header.addColumn ("LENGTH", length, 110, 80, 200, flags);
    header.addColumn ("DESCRIPTION", description, 260, 100, 1200, flags);
    header.setSortColumnId (position, true);
    table.setRowHeight (24);
    table.setMultipleSelectionEnabled (true);
    table.setColour (juce::ListBox::backgroundColourId, pageColour);
    table.setWantsKeyboardFocus (true);
    addAndMakeVisible (table);
    setWantsKeyboardFocus (true);
    refresh();
    startTimerHz (10);
    setSize (preferredWidth, preferredHeight);
}

MarkersPage::~MarkersPage()
{
    stopTimer();
}

juce::TextButton* MarkersPage::keyNamed (const juce::String& name)
{
    for (auto* key : { &addPositionKey, &addCycleKey, &removeKey, &functionsKey, &autoScrollKey, &idsKey, &followsKey, &linesKey })
        if (key->getButtonText() == name)
            return key;
    return nullptr;
}

void MarkersPage::refresh()
{
    const auto& markers = studio.getMarkerTrack().markers;
    // The selection kept by kind and ID.
    std::vector<std::pair<int, bool>> chosen;
    for (const auto i : getSelectedMarkers())
        if (juce::isPositiveAndBelow (i, (int) markers.size()))
            chosen.push_back ({ markers[(size_t) i].id, markers[(size_t) i].cycle });
    order.resize (markers.size());
    std::iota (order.begin(), order.end(), 0);
    const auto& project = studio.getProject();
    const auto key = [this, &markers, &project] (int i) -> juce::String
    {
        const auto& m = markers[(size_t) i];
        switch (sortColumn)
        {
            case id:          return juce::String (m.cycle ? 1 : 0) + juce::String (m.id).paddedLeft ('0', 6);
            case type:        return m.cycle ? "1" : "0";
            case end:         return juce::String (m.end, 6).paddedLeft ('0', 20);
            case length:      return juce::String (m.length(), 6).paddedLeft ('0', 20);
            case description: return m.name.toLowerCase();
            default:          return {};
        }
        juce::ignoreUnused (project);
    };
    std::stable_sort (order.begin(), order.end(), [&] (int a, int b)
    {
        const auto ka = key (a), kb = key (b);
        if (ka != kb)
            return sortForwards ? ka < kb : kb < ka;
        return sortForwards || sortColumn != position ? a < b : b < a;   // (the position second: the list is in time order)
    });
    table.updateContent();
    juce::SparseSet<int> rows;
    for (const auto& [mid, cycle] : chosen)
        if (const auto i = studio.getMarkerTrack().indexOf (mid, cycle); i >= 0)
            for (int r = 0; r < (int) order.size(); ++r)
                if (order[(size_t) r] == i)
                    rows.addRange ({ r, r + 1 });
    table.setSelectedRows (rows, juce::dontSendNotification);
    refreshKeys();
    table.repaint();
}

void MarkersPage::refreshKeys()
{
    const auto& t = studio.getMarkerTrack();
    addPositionKey.setEnabled (! t.locked);
    addCycleKey.setEnabled (! t.locked);
    removeKey.setEnabled (! t.locked && table.getNumSelectedRows() > 0);
    functionsKey.setEnabled (! t.locked);
    autoScrollKey.setToggleState (autoScroll, juce::dontSendNotification);
    idsKey.setToggleState (t.showIds, juce::dontSendNotification);
    followsKey.setToggleState (t.cycleFollows, juce::dontSendNotification);
    linesKey.setToggleState (t.showLines, juce::dontSendNotification);
}

void MarkersPage::timerCallback()
{
    if (studio.getChangeVersion() != seenChange && editor == nullptr)
    {
        seenChange = studio.getChangeVersion();
        refresh();
    }
    const auto row = getLocateRow();
    if (row != shownLocate)
    {
        shownLocate = row;
        if (autoScroll && row >= 0)
            table.scrollToEnsureRowIsOnscreen (row);
        table.repaint();
    }
}

int MarkersPage::getNumRows()
{
    return (int) order.size();
}

int MarkersPage::markerOfRow (int row) const
{
    return juce::isPositiveAndBelow (row, (int) order.size()) ? order[(size_t) row] : -1;
}

int MarkersPage::getLocateRow() const
{
    const auto at = studio.getMarkerTrack().nearestBefore (studio.getEngine().getPosition());
    for (int r = 0; r < (int) order.size(); ++r)
        if (order[(size_t) r] == at)
            return r;
    return -1;
}

juce::String MarkersPage::getCellText (int row, int column) const
{
    const auto i = markerOfRow (row);
    const auto& markers = studio.getMarkerTrack().markers;
    if (i < 0 || i >= (int) markers.size())
        return {};
    const auto& m = markers[(size_t) i];
    const auto& project = studio.getProject();
    switch (column)
    {
        case id:          return pr7s::markerIdText (m);
        case type:        return m.cycle ? "CYCLE" : "POSITION";
        case position:    return positionText (m.start, project);
        case end:         return m.cycle ? positionText (m.end, project) : juce::String();
        case length:      return m.cycle ? KeyEditor::lengthText (m.length(), project) : juce::String();
        case description: return m.name;
        default:          return {};
    }
}

bool MarkersPage::setCell (int row, int column, const juce::String& text)
{
    const auto i = markerOfRow (row);
    auto markers = studio.getMarkerTrack().markers;
    if (i < 0 || i >= (int) markers.size() || studio.getMarkerTrack().locked)
        return false;
    auto& m = markers[(size_t) i];
    const auto& project = studio.getProject();
    switch (column)
    {
        case id:
        {
            const auto wanted = text.trim().removeCharacters ("()").getIntValue();
            if (wanted < 1)
                return false;
            // (Another of its kind with that ID: the two swap, so every ID stays unique.)
            if (const auto other = studio.getMarkerTrack().indexOf (wanted, m.cycle); other >= 0 && other != i)
                markers[(size_t) other].id = m.id;
            m.id = wanted;
            break;
        }
        case position:
        {
            const auto at = TempoEditor::parsePosition (text, project);
            if (! at || *at < 0.0)
                return false;
            const auto length = m.length();
            m.start = *at;
            m.end = m.cycle ? *at + length : *at;
            break;
        }
        case end:
        {
            const auto at = TempoEditor::parsePosition (text, project);
            if (! m.cycle || ! at || *at <= m.start)
                return false;
            m.end = *at;
            break;
        }
        case length:
        {
            const auto beats = KeyEditor::parseLength (text, project);
            if (! m.cycle || ! beats || *beats <= 0.0)
                return false;
            m.end = m.start + *beats;
            break;
        }
        case description:
            m.name = text.trim();
            break;
        default:
            return false;
    }
    return studio.setMarkers (std::move (markers));
}

void MarkersPage::selectRows (const std::vector<int>& rows)
{
    juce::SparseSet<int> set;
    for (const auto r : rows)
        set.addRange ({ r, r + 1 });
    table.setSelectedRows (set, juce::dontSendNotification);
    refreshKeys();
}

std::vector<int> MarkersPage::getSelectedMarkers() const
{
    std::vector<int> out;
    const auto rows = table.getSelectedRows();
    for (int k = 0; k < rows.size(); ++k)
        if (const auto i = markerOfRow (rows[k]); i >= 0)
            out.push_back (i);
    return out;
}

void MarkersPage::addPosition()
{
    if (const auto i = studio.addPositionMarkerAtCursor(); i >= 0)
    {
        refresh();
        for (int r = 0; r < (int) order.size(); ++r)
            if (order[(size_t) r] == i)
                selectRows ({ r });
    }
}

void MarkersPage::addCycle()
{
    if (const auto i = studio.addCycleMarkerAtLocators(); i >= 0)
    {
        refresh();
        for (int r = 0; r < (int) order.size(); ++r)
            if (order[(size_t) r] == i)
                selectRows ({ r });
    }
}

bool MarkersPage::removeSelected()
{
    const auto chosen = getSelectedMarkers();
    if (chosen.empty() || ! studio.removeMarkers (chosen))
        return false;
    table.deselectAllRows();
    refresh();
    return true;
}

bool MarkersPage::locateRow (int row)
{
    return studio.locateMarker (markerOfRow (row));
}

void MarkersPage::moveSelectedToCursor()
{
    studio.moveMarkersToCursor (getSelectedMarkers());
    refresh();
}

void MarkersPage::sortBy (int column, bool forwards)
{
    sortColumn = column;
    sortForwards = forwards;
    refresh();
}

void MarkersPage::sortOrderChanged (int column, bool forwards)
{
    sortBy (column, forwards);
}

void MarkersPage::showFunctionsMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Move Markers to Cursor", table.getNumSelectedRows() > 0);
    menu.addItem (2, "Reassign Position Marker IDs");
    menu.addItem (3, "Reassign Cycle Marker IDs");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&functionsKey),
                        [safe = juce::Component::SafePointer<MarkersPage> (this)] (int chosen)
    {
        if (safe == nullptr)
            return;
        if (chosen == 1)
            safe->moveSelectedToCursor();
        else if (chosen == 2 || chosen == 3)
        {
            safe->studio.reassignMarkerIds (chosen == 3);
            safe->refresh();
        }
    });
}

void MarkersPage::paint (juce::Graphics& g)
{
    g.fillAll (pageColour);
    auto area = getLocalBounds().reduced (16, 12);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (18.0f));
    g.drawText ("MARKERS", area.removeFromTop (24), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::textFont (12.0f));
    const auto& t = studio.getMarkerTrack();
    g.drawText (t.name.toUpperCase() + (t.locked ? "  (LOCKED)" : "") + "  -  DOUBLE-CLICK A CELL TO TYPE; RETURN GOES TO THE SELECTED MARKER",
                getLocalBounds().reduced (16, 12).withTrimmedLeft (110).removeFromTop (24), juce::Justification::centredLeft, true);
}

void MarkersPage::resized()
{
    auto area = getLocalBounds().reduced (16, 12);
    area.removeFromTop (30);
    auto keys = area.removeFromTop (28);
    for (auto* key : { &addPositionKey, &addCycleKey, &removeKey, &functionsKey })
    {
        key->setBounds (keys.removeFromLeft (key == &functionsKey ? 110 : 104));
        keys.removeFromLeft (6);
    }
    for (auto* key : { &linesKey, &followsKey, &idsKey, &autoScrollKey })
    {
        key->setBounds (keys.removeFromRight (key == &followsKey ? 116 : key == &linesKey ? 64 : 96));
        keys.removeFromRight (6);
    }
    area.removeFromTop (8);
    table.setBounds (area);
    if (editor != nullptr && editRow >= 0)
        editor->setBounds (table.getCellPosition (editColumn, editRow, true).translated (table.getX(), table.getY()));
}

void MarkersPage::paintRowBackground (juce::Graphics& g, int row, int width, int height, bool selected)
{
    g.setColour (selected ? pr7s::FamilyColours::pink.withAlpha (0.35f) : (row % 2 == 0 ? juce::Colour (0xff151016) : pageColour));
    g.fillRect (0, 0, width, height);
}

void MarkersPage::paintCell (juce::Graphics& g, int row, int column, int width, int height, bool selected)
{
    if (column == locate)
    {
        if (row == getLocateRow())
        {
            juce::Path arrow;
            const auto cy = (float) height * 0.5f;
            arrow.addTriangle (7.0f, cy - 6.0f, 7.0f, cy + 6.0f, 19.0f, cy);
            g.setColour (pr7s::FamilyColours::orange);
            g.fillPath (arrow);
        }
        return;
    }
    const auto i = markerOfRow (row);
    const auto cycle = i >= 0 && studio.getMarkerTrack().markers[(size_t) i].cycle;
    g.setColour (column == type ? (cycle ? pr7s::FamilyColours::pink : pr7s::FamilyColours::orange).brighter (0.3f)
                                : (selected ? juce::Colours::white : ParagonColours::text));
    g.setFont (column == description ? ParagonLookAndFeel::textFont (14.0f) : ParagonLookAndFeel::titleFont (14.0f));
    g.drawText (getCellText (row, column), juce::Rectangle<int> (6, 0, width - 10, height), juce::Justification::centredLeft, true);
}

void MarkersPage::cellClicked (int row, int column, const juce::MouseEvent&)
{
    if (column == locate)
        locateRow (row);   // (the manual: click in the Locate column to go there)
    refreshKeys();
}

void MarkersPage::cellDoubleClicked (int row, int column, const juce::MouseEvent&)
{
    if (column != locate && column != type)
        beginEdit (row, column);
}

void MarkersPage::returnKeyPressed (int row)
{
    locateRow (row);
}

void MarkersPage::beginEdit (int row, int column)
{
    if (studio.getMarkerTrack().locked)
        return;
    const auto i = markerOfRow (row);
    if (i < 0 || ((column == end || column == length) && ! studio.getMarkerTrack().markers[(size_t) i].cycle))
        return;
    editRow = row;
    editColumn = column;
    auto entry = std::make_unique<InfoEntry>();
    entry->setFont (ParagonLookAndFeel::textFont (14.0f));
    entry->setText (getCellText (row, column), false);
    entry->setSelectAllWhenFocused (true);
    auto finish = [this] (bool apply)
    {
        if (editor == nullptr)
            return;
        const auto text = editor->getText();
        const auto row = editRow, column = editColumn;
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MarkersPage> (this)]
        {
            if (safe != nullptr)
            {
                safe->editor.reset();
                safe->editRow = -1;
                safe->refresh();
            }
        });
        if (apply)
            setCell (row, column, text);
    };
    entry->onEnter = [finish] (bool) { finish (true); };
    entry->onEscapeKey = [finish] { finish (false); };
    entry->onFocusLost = [finish] { finish (true); };
    editor = std::move (entry);
    addAndMakeVisible (*editor);
    resized();
    editor->grabKeyboardFocus();
}

bool MarkersPage::keyPressed (const juce::KeyPress& key)
{
    const auto rows = getNumRows();
    if (rows > 0)
    {
        const auto now = table.getSelectedRow();
        const auto code = key.getKeyCode();
        auto to = -1;
        if (code == juce::KeyPress::upKey)            to = juce::jmax (0, now - 1);
        else if (code == juce::KeyPress::downKey)     to = juce::jmin (rows - 1, now + 1);
        else if (code == juce::KeyPress::homeKey)     to = 0;
        else if (code == juce::KeyPress::endKey)      to = rows - 1;
        else if (code == juce::KeyPress::pageUpKey)   to = juce::jmax (0, now - 10);
        else if (code == juce::KeyPress::pageDownKey) to = juce::jmin (rows - 1, now + 10);
        if (to >= 0 && ! key.getModifiers().isAnyModifierKeyDown())
        {
            selectRows ({ to });
            table.scrollToEnsureRowIsOnscreen (to);
            return true;
        }
        if (code == juce::KeyPress::returnKey && now >= 0)
            return locateRow (now);
        if ((code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) && table.getNumSelectedRows() > 0)
            return removeSelected();
    }
    return onOtherKey != nullptr && onOtherKey (key);
}

//==============================================================================
MarkersWindow::MarkersWindow (pr7s::Studio& s)
    : DocumentWindow ("Paragon PR7-S: markers", pageColour, DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    page = new MarkersPage (s);
    setContentOwned (page, true);
    setResizable (true, false);
    setResizeLimits (640, 300, 3000, 2000);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
}

void MarkersWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}
