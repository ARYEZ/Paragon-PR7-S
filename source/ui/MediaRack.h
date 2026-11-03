#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"
#include "ui/MediaUi.h"

#include <array>

// The Media rack (0.22.0, the manual's Media Rack in the Right Zone): the MediaBay's things in a
// panel at the right of the project window (Ctrl+Alt+R), so you see your tracks while you drag
// sounds, instruments, effects and presets into the song.
//
//   MEDIA     the tab (the only one: no VSTi rack, control room or meter tab here)
//   search    a text search on the page (name or tags), RESET (x), ALL (every result)
//   address   BACK, HOME and where you are ("HOME / LOOPS & SAMPLES")
//   pages     HOME's tiles: VST INSTRUMENTS, VST EFFECTS, LOOPS & SAMPLES, PRESETS, FAVORITES,
//             FILE BROWSER; each opens its page. Results pages have the type keys, SHUFFLE,
//             FILTER (two attribute filter columns) and CLEAR (lit while a filter is set).
//   bottom    the previewer: the selected file's shape and playhead, PLAY / STOP, AUTO PLAY
//
// Into the song: double-click, or drag onto the track list or the arrangement. Instruments make
// an instrument track, effects an FX channel; audio and MIDI land as from the MediaBay; a track
// preset dropped on a track of its kind is applied to it, elsewhere it makes a new track.
class MediaRack final : public juce::Component,
                        public juce::DragAndDropContainer,
                        public MediaUi::AttributeFilterHost,
                        private juce::ListBoxModel,
                        private juce::Timer
{
public:
    explicit MediaRack (pr7s::Studio&);
    ~MediaRack() override;

    enum class Page { home, instruments, effects, loops, presets, favorites, fileBrowser, results };

    struct Row
    {
        enum class Kind { heading, plugin, media, folder, file, up } kind = Kind::heading;
        juce::String text, detail;
        juce::File file;                       // media, folder, file
        juce::PluginDescription plugin;        // plugin
        bool studioSynth = false, instrument = false;
        pr7s::MediaType type = pr7s::MediaType::audio;
    };

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;   // the tiles

    // Getting around (the tests drive these).
    void openPage (Page);
    void openFavorite (int index);                  // its files, as results
    void openFolder (const juce::File&);            // the file browser in that folder (none: its roots)
    void back();
    void home();
    Page getPage() const noexcept { return state.page; }
    juce::File getFolder() const noexcept { return state.folder; }
    juce::String getAddress() const;
    juce::StringArray getTileNames() const;
    void clickTile (int index);
    void setSearch (const juce::String&);
    void showAllResults();

    // Results pages.
    void setTypeShown (pr7s::MediaType, bool shown);
    void shuffle (juce::uint32 seed);
    void setFiltersShown (bool);
    bool areFiltersShown() const noexcept { return filtersShown; }
    bool isAttributeFilterSet() const;
    void resetAttributeFilter();
    void setPluginsByVendor (bool);

    // Favorites (kept in the settings; each is a MediaBay location too, as the manual's).
    const juce::StringArray& getFavorites() const noexcept { return favorites; }
    bool addFavorite (const juce::File& folder);
    void removeFavorite (int index);

    // The list.
    int getNumRows() override;
    const Row* getRow (int index) const;
    int findRow (const juce::String& text) const;   // the first row with that text (-1: none)
    void selectRow (int index);
    void use (int index);                              // as a double-click
    bool insertOnSelectedTrack (int index);            // an effect into the selected track's (or master's) inserts
    bool changeSelectedInstrument (int index);         // the selected instrument track's instrument
    bool applyToSelectedTrack (int index);             // a track preset onto the selected track
    juce::String getDragDescription (int index) const;
    bool play();
    void stop();
    void setAutoPlay (bool);
    void setPreviewOptions (const pr7s::Studio::MediaPreviewOptions&);   // (0.23.0: shared with the MediaBay window's)
    void showPitchMenu();
    void refresh();                                    // the rows again (after a scan, a new plug-in...)

    // A drag's description for a plug-in ("mediarack:instrument:<id>" or "...effect:<id>"; the
    // studio's synth is "mediarack:instrument:studio"), and what it names.
    static bool isRackDescription (const juce::String&);
    static std::optional<pr7s::Instrument> instrumentFor (pr7s::Studio&, const juce::String& description);
    static std::optional<juce::PluginDescription> effectFor (pr7s::Studio&, const juce::String& description);

    std::function<void (const juce::File&)> onUseFile;   // MainView: audio, MIDI, a song (as the MediaBay's USE IT)

    // AttributeFilterHost
    const pr7s::MediaDatabase& getFilterDatabase() const override { return database; }
    pr7s::AttributeId getFilterColumn (int column) const override;
    void setFilterColumn (int column, const pr7s::AttributeId&) override;
    std::vector<std::pair<juce::String, int>> getFilterValues (int column) const override;
    juce::StringArray getFilterSelection (int column) const override;
    void clickFilterValue (int column, const juce::String& value, bool addToOthers) override;

    static constexpr int numFilterColumns = 2, tabHeight = 30, rowHeight = 28, previewerHeight = 84, filtersHeight = 150;
    static constexpr int defaultWidth = 360, minimumWidth = 300, maximumWidth = 720;

private:
    struct State { Page page = Page::home; juce::File folder; };

    // ListBoxModel
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int lastRowSelected) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>&) override;
    juce::String getTooltipForRow (int row) override;

    void timerCallback() override;
    void go (State);              // to a page, remembered for BACK
    void rebuild();               // rows for the page
    void layout();
    void refreshKeys();
    void saveSettings();
    void showRowMenu (int index);
    void showTileMenu (int index);
    bool isResultsPage() const noexcept;
    bool hasPreviewer() const noexcept;
    std::set<pr7s::MediaType> pageTypes() const;   // what a results page can show
    pr7s::MediaQuery currentQuery() const;
    void paintPreviewer (juce::Graphics&);
    void warn (const juce::String& title, const juce::String& text) const;

    struct Tile { juce::String name, detail; std::function<void()> action; int favorite = -1; };
    std::vector<Tile> tiles() const;
    std::vector<juce::Rectangle<int>> tileBounds (int count) const;

    pr7s::Studio& studio;
    pr7s::MediaDatabase& database;
    State state;
    std::vector<State> history;
    std::vector<Row> rows;
    juce::StringArray favorites;
    std::set<pr7s::MediaType> typesShown;          // empty: all the page's types
    std::array<pr7s::AttributeFilter, numFilterColumns> filters;
    std::optional<juce::uint32> shuffleSeed;
    bool filtersShown = false, autoPlay = true, byVendor = false, refreshing = false;
    juce::File previewing;
    int favoritesCheck = 0;

    juce::TextEditor search;
    juce::TextButton resetSearchKey { "X" }, allKey { "ALL" }, backKey { "BACK" }, homeKey { "HOME" },
                     audioKey { "AUDIO" }, midiKey { "MIDI" }, presetsKey { "PRESETS" }, songsKey { "SONGS" },
                     shuffleKey { "SHUFFLE" }, filterKey { "FILTER" }, clearKey { "CLEAR" },
                     categoryKey { "BY CATEGORY" }, vendorKey { "BY VENDOR" }, upKey { "UP" }, favoriteKey { "ADD FAVORITE" },
                     playKey { "PLAY" }, autoPlayKey { "AUTO PLAY" },
                     linkKey { "LINK" }, waitKey { "WAIT" }, pitchKey { "PITCH" };
    juce::ListBox list;
    std::array<std::unique_ptr<MediaUi::AttributeFilterColumn>, numFilterColumns> filterColumns;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> addressArea, contentArea, previewArea, waveArea;
};
