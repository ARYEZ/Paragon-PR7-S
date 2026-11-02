#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

#include <array>

// The MediaBay window (0.21.0, the owner asked: as the manual's MediaBay, F5). Your media files
// (audio, MIDI files, track presets, songs) in the folders you scan, searched and filtered,
// heard in the previewer, rated, and put into the song by a double-click (a new track at the
// playhead) or a drag onto a track.
//
//   title    MEDIABAY, how many files, the scan's progress; RESCAN
//   toolbar  search (and/+, or/comma, not/-, parentheses, "phrases"); AUDIO MIDI PRESETS SONGS;
//            the rating filter; SUBFOLDERS; SHUFFLE; RESET
//   left     LOCATIONS: all of them or one; ADD FOLDER, REMOVE
//   middle   the results: rating (click the stars), name, type, length, tempo, key, format,
//            folder, size, date; click a heading to sort
//   bottom   the previewer: PLAY / STOP, CYCLE, AUTO PLAY, LEVEL, the sound's shape with the
//            playhead, USE IT
//   0.21.1   tags: the ATTRIBUTE FILTER above the results (four columns of values with how many
//            files have each; click one, Ctrl-click for either; a column's title picks its
//            attribute) and the ATTRIBUTE INSPECTOR at the right (the selected files' attributes:
//            click a value to set it for all of them, right-click to remove it; DYNAMIC shows only
//            those set; + USER ATTRIBUTE). FILTERS and INSPECTOR in the title row show or hide them.
class MediaBayPage final : public juce::Component,
                           public juce::DragAndDropContainer,
                           private juce::TableListBoxModel,
                           private juce::Timer
{
public:
    explicit MediaBayPage (pr7s::Studio&);
    ~MediaBayPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // What the controls do (the tests drive them).
    void setSearch (const juce::String&);
    void setTypeShown (pr7s::MediaType, bool shown);
    void setMinimumRating (int stars);
    void setSubfolders (bool);
    void selectLocation (int index);              // -1: all locations
    void resetFilters();
    void shuffle (juce::uint32 seed);
    void sortBy (pr7s::MediaColumn, bool ascending);
    void refresh();                               // the results again (after a scan, a filter)
    void selectRow (int row);                     // (and, with AUTO PLAY on, plays it)
    void setRating (int row, int stars);
    bool play();                                  // the selected file; false if it can't be heard
    void stop();
    void setAutoPlay (bool);
    void setCycle (bool);
    void use (int row);                           // as a double-click
    bool addLocation (const juce::File&);
    void removeSelectedLocation();

    // Tags (0.21.1): on the selected files.
    juce::Array<juce::File> getSelectedFiles() const;
    void selectRows (const juce::Array<int>& rows);
    void setAttribute (const pr7s::AttributeId&, const juce::String& value);
    void removeAttribute (const pr7s::AttributeId&);
    void setCharacter (int pair, int side);
    juce::String getAttributeText (const pr7s::AttributeId&) const;   // as the inspector shows it ("" none, "(MIXED)")
    bool addUserAttribute (const pr7s::UserAttribute&);
    void deleteUserAttribute (const juce::String& name);   // (from every file; a filter column on it goes back to CATEGORY)
    void addListValue (bool style, const juce::String& name, const juce::String& sub);   // added to the lists, set on the selected files
    void setInspectorShown (bool);
    bool isInspectorShown() const noexcept { return inspectorShown; }
    void setInspectorDynamic (bool);
    juce::StringArray getInspectorRows() const;    // the attributes' names it shows
    // The attribute filter.
    void setFiltersShown (bool);
    bool areFiltersShown() const noexcept { return filtersShown; }
    static constexpr int numFilterColumns = 4;
    void setFilterColumn (int column, const pr7s::AttributeId&);
    pr7s::AttributeId getFilterColumn (int column) const;
    void clickFilterValue (int column, const juce::String& value, bool addToOthers);   // (as a click; Ctrl: addToOthers)
    std::vector<std::pair<juce::String, int>> getFilterValues (int column) const;
    juce::StringArray getFilterSelection (int column) const;

    int getNumResults() const noexcept { return (int) results.size(); }
    const pr7s::MediaItem* getResult (int row) const;
    int getSelectedRow() const;
    juce::String getStatusText() const;
    static juce::String dragDescriptionFor (const juce::StringArray& paths);   // "mediabay:" + paths, a line each
    static juce::StringArray filesFromDragDescription (const juce::String&);

    std::function<void (const juce::File&)> onUse;   // MainView: into the song

    static constexpr int titleHeight = 46, toolbarHeight = 40, previewerHeight = 118, locationsWidth = 250, gap = 10,
                         inspectorWidth = 320, filtersHeight = 170;

private:
    // TableListBoxModel
    int getNumRows() override;
    void paintRowBackground (juce::Graphics&, int row, int width, int height, bool selected) override;
    void paintCell (juce::Graphics&, int row, int column, int width, int height, bool selected) override;
    void cellClicked (int row, int column, const juce::MouseEvent&) override;
    void cellDoubleClicked (int row, int column, const juce::MouseEvent&) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void sortOrderChanged (int columnId, bool forwards) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>&) override;
    juce::String getCellTooltip (int row, int column) override;

    void timerCallback() override;
    void refreshLocations();
    void refreshKeys();
    void saveSettings();
    void paintPreviewer (juce::Graphics&, juce::Rectangle<int>);

    class Locations;
    class FilterColumn;
    class Inspector;
    void refreshTags();          // the filter's columns and the inspector, after an edit or a new selection
    void attributesEdited();     // results again (a filter may drop a file), the tags shown
    pr7s::Studio& studio;
    pr7s::MediaDatabase& database;
    pr7s::MediaQuery query;
    std::vector<int> results;
    std::optional<std::pair<pr7s::MediaColumn, bool>> sortOrder;
    int locationIndex = -1;
    bool autoPlay = true, cycle = false;
    juce::File previewing;   // the file the previewer holds

    juce::TextEditor search;
    juce::TextButton audioKey { "AUDIO" }, midiKey { "MIDI" }, presetsKey { "PRESETS" }, songsKey { "SONGS" },
                     subfoldersKey { "SUBFOLDERS" }, shuffleKey { "SHUFFLE" }, resetKey { "RESET" }, rescanKey { "RESCAN" },
                     addKey { "ADD FOLDER..." }, removeKey { "REMOVE" },
                     playKey { "PLAY" }, cycleKey { "CYCLE" }, autoPlayKey { "AUTO PLAY" }, useKey { "USE IT" };
    juce::ComboBox ratingBox;
    juce::Slider level { juce::Slider::LinearBar, juce::Slider::NoTextBox };
    juce::TableListBox table;
    std::unique_ptr<Locations> locations;
    std::array<std::unique_ptr<FilterColumn>, numFilterColumns> filterColumns;
    std::unique_ptr<Inspector> inspector;
    std::unique_ptr<juce::Viewport> inspectorView;
    juce::TextButton filtersKey { "FILTERS" }, inspectorKey { "INSPECTOR" },
                     dynamicKey { "DYNAMIC" }, definedKey { "DEFINED" }, userKey { "+ USER ATTRIBUTE" };
    bool filtersShown = true, inspectorShown = true, dynamic = false, reselecting = false;
    juce::Rectangle<int> filtersArea, inspectorArea;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> previewArea, waveArea, levelLabel;
};

// Its window: kept open beside the studio (F5 opens and closes it).
class MediaBayWindow final : public juce::DocumentWindow
{
public:
    explicit MediaBayWindow (pr7s::Studio&);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    MediaBayPage& getPage() { return *page; }

private:
    MediaBayPage* page = nullptr;
};
