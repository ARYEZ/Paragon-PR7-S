#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

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

    int getNumResults() const noexcept { return (int) results.size(); }
    const pr7s::MediaItem* getResult (int row) const;
    int getSelectedRow() const;
    juce::String getStatusText() const;
    static juce::String dragDescriptionFor (const juce::StringArray& paths);   // "mediabay:" + paths, a line each
    static juce::StringArray filesFromDragDescription (const juce::String&);

    std::function<void (const juce::File&)> onUse;   // MainView: into the song

    static constexpr int titleHeight = 46, toolbarHeight = 40, previewerHeight = 118, locationsWidth = 250, gap = 10;

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
