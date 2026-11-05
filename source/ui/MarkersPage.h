#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// The Markers window (0.49.0, the manual's Markers Window; Ctrl+M): the marker track's markers
// listed with the locate arrow (the marker at or before the playhead: click it to go there),
// ID, TYPE, POSITION, END, LENGTH and DESCRIPTION. Double-click a cell to type (Enter applies);
// click a column's header to sort by it (the position always second). ADD POSITION, ADD CYCLE,
// REMOVE, FUNCTIONS (Move Markers to Cursor, Reassign Position / Cycle Marker IDs), AUTO-SCROLL,
// and the Marker Settings: SHOW IDS, CYCLE FOLLOWS, LINES. Keys in the list: Up/Down, Home/End,
// Page Up/Down move the selection; Return locates to it.
class MarkersPage final : public juce::Component,
                          private juce::TableListBoxModel,
                          private juce::Timer
{
public:
    explicit MarkersPage (pr7s::Studio&);
    ~MarkersPage() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    enum Column { locate = 1, id, type, position, end, length, description };
    // (Tests drive these as the keys and cells do.) Rows are in the list's shown order.
    int getNumRows() override;
    juce::String getCellText (int row, int column) const;
    bool setCell (int row, int column, const juce::String& text);   // as typed into the cell
    int markerOfRow (int row) const;   // its index in the marker track, -1: none
    int getLocateRow() const;          // the arrow's row, -1: none
    void selectRows (const std::vector<int>& rows);
    std::vector<int> getSelectedMarkers() const;   // marker indices
    void addPosition();
    void addCycle();
    bool removeSelected();
    bool locateRow (int row);
    void moveSelectedToCursor();
    void sortBy (int column, bool forwards);
    juce::TextButton* keyNamed (const juce::String&);
    void refresh();
    std::function<bool (const juce::KeyPress&)> onOtherKey;   // keys it doesn't use: the studio's
    static constexpr int preferredWidth = 820, preferredHeight = 520;

private:
    void paintRowBackground (juce::Graphics&, int row, int width, int height, bool selected) override;
    void paintCell (juce::Graphics&, int row, int column, int width, int height, bool selected) override;
    void cellClicked (int row, int column, const juce::MouseEvent&) override;
    void cellDoubleClicked (int row, int column, const juce::MouseEvent&) override;
    void sortOrderChanged (int column, bool forwards) override;
    void returnKeyPressed (int row) override;
    void timerCallback() override;
    void beginEdit (int row, int column);
    void showFunctionsMenu();
    void refreshKeys();

    pr7s::Studio& studio;
    juce::TableListBox table { "Markers", this };
    std::vector<int> order;   // rows -> marker indices
    int sortColumn = position;
    bool sortForwards = true;
    std::unique_ptr<juce::TextEditor> editor;
    int editRow = -1, editColumn = 0;
    int shownLocate = -2;
    juce::uint64 seenChange = 0;
    bool autoScroll = true;
    juce::TextButton addPositionKey { "ADD POSITION" }, addCycleKey { "ADD CYCLE" }, removeKey { "REMOVE" },
                     functionsKey { "FUNCTIONS" }, autoScrollKey { "AUTO-SCROLL" },
                     idsKey { "SHOW IDS" }, followsKey { "CYCLE FOLLOWS" }, linesKey { "LINES" };
};

class MarkersWindow final : public juce::DocumentWindow
{
public:
    explicit MarkersWindow (pr7s::Studio&);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    MarkersPage& getPage() { return *page; }

private:
    MarkersPage* page = nullptr;
};
