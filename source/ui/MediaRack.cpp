#include "ui/MediaRack.h"

#include "ui/MediaBayPage.h"
#include "ui/TrackActions.h"

using namespace MediaUi;

namespace
{
    const juce::String instrumentPrefix = "mediarack:instrument:", effectPrefix = "mediarack:effect:";

    juce::String pageName (MediaRack::Page page)
    {
        using P = MediaRack::Page;
        switch (page)
        {
            case P::home: return "HOME";
            case P::instruments: return "VST INSTRUMENTS";
            case P::effects: return "VST EFFECTS";
            case P::loops: return "LOOPS & SAMPLES";
            case P::presets: return "PRESETS";
            case P::favorites: return "FAVORITES";
            case P::fileBrowser: return "FILE BROWSER";
            case P::results: return "RESULTS";
        }
        return {};
    }

    MediaRack::Row headingRow (const juce::String& text)
    {
        MediaRack::Row row;
        row.text = text;
        return row;
    }

    juce::File userContent() { return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Paragon PR7-S"); }
}

//==============================================================================
MediaRack::MediaRack (pr7s::Studio& s) : studio (s), database (s.getMediaBay())
{
    using A = pr7s::MediaAttribute;
    filters[0].attribute = { A::category, {} };
    filters[1].attribute = { A::style, {} };
    if (auto* settings = studio.getSettings())
    {
        favorites = juce::StringArray::fromLines (settings->getValue ("mediaRackFavorites"));
        favorites.removeEmptyStrings();
        filtersShown = settings->getBoolValue ("mediaRackFilters", false);
        autoPlay = settings->getBoolValue ("mediaRackAutoPlay", true);
        byVendor = settings->getBoolValue ("mediaRackByVendor", false);
    }

    search.setTextToShowWhenEmpty ("SEARCH (NAME OR TAGS)", ParagonColours::textDim);
    search.setFont (interFont (13.0f));
    search.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1b171c));
    search.setColour (juce::TextEditor::textColourId, ParagonColours::text);
    search.setColour (juce::TextEditor::outlineColourId, ParagonColours::panelEdge);
    search.setColour (juce::TextEditor::focusedOutlineColourId, ParagonColours::accent);
    search.onTextChange = [this] { setSearch (search.getText()); };
    addAndMakeVisible (search);

    for (auto* key : { &resetSearchKey, &allKey, &backKey, &homeKey, &audioKey, &midiKey, &presetsKey, &songsKey, &shuffleKey, &filterKey,
                       &clearKey, &categoryKey, &vendorKey, &upKey, &favoriteKey, &playKey, &autoPlayKey, &linkKey, &waitKey, &pitchKey })
    {
        styleKey (*key);
        addChildComponent (*key);
    }
    for (auto* key : { &resetSearchKey, &allKey, &backKey, &homeKey })
        key->setVisible (true);
    resetSearchKey.setTooltip ("Clear the search");
    resetSearchKey.onClick = [this] { setSearch ({}); };
    allKey.setTooltip ("Show all results: every file of the page's kind (on HOME: every file)");
    allKey.onClick = [this] { showAllResults(); };
    backKey.setTooltip ("The page before");
    backKey.onClick = [this] { back(); };
    homeKey.setTooltip ("The HOME page's tiles");
    homeKey.onClick = [this] { home(); };
    const auto typeKey = [this] (juce::TextButton& key, pr7s::MediaType type)
    {
        key.onClick = [this, &key, type] { setTypeShown (type, ! key.getToggleState()); };
        key.setTooltip ("Only this kind of file (none lit: all the page's kinds)");
    };
    typeKey (audioKey, pr7s::MediaType::audio);
    typeKey (midiKey, pr7s::MediaType::midi);
    typeKey (presetsKey, pr7s::MediaType::trackPreset);
    typeKey (songsKey, pr7s::MediaType::song);
    shuffleKey.setTooltip ("The results in a random order");
    shuffleKey.onClick = [this] { shuffle ((juce::uint32) juce::Random::getSystemRandom().nextInt()); };
    filterKey.setTooltip ("Show or hide the attribute filter (two columns; click a column's title for another attribute)");
    filterKey.onClick = [this] { setFiltersShown (! filtersShown); };
    clearKey.setTooltip ("Lit while an attribute filter is set: click to clear it");
    clearKey.onClick = [this] { resetAttributeFilter(); };
    categoryKey.setTooltip ("Plug-ins grouped by their category");
    categoryKey.onClick = [this] { setPluginsByVendor (false); };
    vendorKey.setTooltip ("Plug-ins grouped by their maker");
    vendorKey.onClick = [this] { setPluginsByVendor (true); };
    upKey.setTooltip ("The folder this one is in");
    upKey.onClick = [this]
    {
        if (state.folder != juce::File())
            openFolder (state.folder.getParentDirectory() == state.folder ? juce::File() : state.folder.getParentDirectory());
    };
    favoriteKey.setTooltip ("A folder as a favorite: a tile on FAVORITES, and scanned into the MediaBay");
    favoriteKey.onClick = [this]
    {
        // The file browser: the selected folder, else the one it's in. FAVORITES: a chooser.
        if (state.page == Page::fileBrowser)
        {
            const auto* row = getRow (list.getSelectedRow());
            const auto folder = row != nullptr && row->kind == Row::Kind::folder ? row->file : state.folder;
            if (folder != juce::File())
                addFavorite (folder);
            return;
        }
        chooser = std::make_unique<juce::FileChooser> ("A favorite folder", juce::File::getSpecialLocation (juce::File::userHomeDirectory));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safe = juce::Component::SafePointer<MediaRack> (this)] (const juce::FileChooser& c)
                              {
                                  if (safe != nullptr && c.getResult() != juce::File())
                                      safe->addFavorite (c.getResult());
                              });
    };
    playKey.setTooltip ("Hear the selected file");
    playKey.onClick = [this] { studio.isMediaPreviewing() ? stop() : (void) play(); };
    autoPlayKey.setTooltip ("Play each file as it's selected");
    autoPlayKey.onClick = [this] { setAutoPlay (! autoPlay); };
    linkKey.setTooltip ("LINK TEMPO: a loop stretched to the song's tempo at the playhead (its pitch kept)");
    linkKey.onClick = [this] { auto o = studio.getMediaPreviewOptions(); o.linkTempo = ! o.linkTempo; setPreviewOptions (o); };
    waitKey.setTooltip ("WAIT FOR PLAY: heard only while the song plays; with LINK, locked to the bars from the left locator");
    waitKey.onClick = [this] { auto o = studio.getMediaPreviewOptions(); o.waitForPlay = ! o.waitForPlay; setPreviewOptions (o); };
    pitchKey.setTooltip ("Transpose, move to a root key, half or double speed");
    pitchKey.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    pitchKey.onClick = [this] { showPitchMenu(); };

    list.setModel (this);
    list.setRowHeight (rowHeight);
    list.setMultipleSelectionEnabled (true);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff0c0a0d));
    list.setOutlineThickness (0);
    addChildComponent (list);
    for (int i = 0; i < numFilterColumns; ++i)
    {
        filterColumns[(size_t) i] = std::make_unique<AttributeFilterColumn> (*this, i);
        addChildComponent (*filterColumns[(size_t) i]);
    }
    setSize (defaultWidth, 800);
    rebuild();
    startTimerHz (20);
}

MediaRack::~MediaRack()
{
    if (studio.isMediaPreviewing())
        studio.stopMediaPreview();
    list.setModel (nullptr);
}

void MediaRack::saveSettings()
{
    if (auto* settings = studio.getSettings())
    {
        settings->setValue ("mediaRackFavorites", favorites.joinIntoString ("\n"));
        settings->setValue ("mediaRackFilters", filtersShown);
        settings->setValue ("mediaRackAutoPlay", autoPlay);
        settings->setValue ("mediaRackByVendor", byVendor);
    }
}

void MediaRack::warn (const juce::String& title, const juce::String& text) const
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, title, text);
}

//==============================================================================
bool MediaRack::isResultsPage() const noexcept
{
    return state.page == Page::loops || state.page == Page::presets || state.page == Page::results
        || (state.page == Page::favorites && state.folder != juce::File());
}

bool MediaRack::hasPreviewer() const noexcept
{
    return isResultsPage() || state.page == Page::fileBrowser;
}

std::set<pr7s::MediaType> MediaRack::pageTypes() const
{
    using T = pr7s::MediaType;
    switch (state.page)
    {
        case Page::loops: return { T::audio, T::midi };
        case Page::presets: return { T::trackPreset };
        default: return { T::audio, T::midi, T::trackPreset, T::song };
    }
}

pr7s::MediaQuery MediaRack::currentQuery() const
{
    pr7s::MediaQuery query;
    query.text = search.getText();
    const auto base = pageTypes();
    for (const auto type : base)
        if (typesShown.empty() || typesShown.count (type) > 0)
            query.types.insert (type);
    if (query.types.empty())
        query.types = base;
    if (state.page == Page::favorites)
        query.folder = state.folder;
    query.attributes.assign (filters.begin(), filters.end());
    return query;
}

void MediaRack::go (State next)
{
    if (next.page == state.page && next.folder == state.folder)
        return rebuild();
    history.push_back (state);
    if (history.size() > 50)
        history.erase (history.begin());
    state = next;
    shuffleSeed.reset();
    typesShown.clear();
    list.deselectAllRows();
    list.setVerticalPosition (0.0);
    rebuild();
}

void MediaRack::openPage (Page page)          { go ({ page, {} }); }
void MediaRack::openFolder (const juce::File& folder) { go ({ Page::fileBrowser, folder }); }

void MediaRack::openFavorite (int index)
{
    if (juce::isPositiveAndBelow (index, favorites.size()))
        go ({ Page::favorites, juce::File (favorites[index]) });
}

void MediaRack::back()
{
    if (history.empty())
        return;
    state = history.back();
    history.pop_back();
    shuffleSeed.reset();
    list.deselectAllRows();
    rebuild();
}

void MediaRack::home()
{
    openPage (Page::home);
}

void MediaRack::showAllResults()
{
    // (On a page of files: that page's results again with nothing narrowed; elsewhere: every file.)
    if (state.page == Page::loops || state.page == Page::presets || (state.page == Page::favorites && state.folder != juce::File()))
    {
        typesShown.clear();
        return rebuild();
    }
    openPage (Page::results);
}

juce::String MediaRack::getAddress() const
{
    juce::String address = "HOME";
    if (state.page != Page::home)
        address << "  /  " << pageName (state.page);
    if (state.folder != juce::File())
        address << "  /  " << (state.folder.getFileName().isNotEmpty() ? state.folder.getFileName() : state.folder.getFullPathName()).toUpperCase();
    return address;
}

void MediaRack::setSearch (const juce::String& text)
{
    if (search.getText() != text)
        search.setText (text, false);
    // (A search on HOME looks through every file, as the manual's.)
    if (state.page == Page::home && text.isNotEmpty())
        return openPage (Page::results);
    rebuild();
}

void MediaRack::setTypeShown (pr7s::MediaType type, bool shown)
{
    if (shown)
        typesShown.insert (type);
    else
        typesShown.erase (type);
    rebuild();
}

void MediaRack::shuffle (juce::uint32 seed)
{
    shuffleSeed = seed;
    rebuild();
}

void MediaRack::setFiltersShown (bool shown)
{
    filtersShown = shown;
    saveSettings();
    layout();
    refreshKeys();
}

bool MediaRack::isAttributeFilterSet() const
{
    for (const auto& f : filters)
        if (! f.values.isEmpty())
            return true;
    return false;
}

void MediaRack::resetAttributeFilter()
{
    for (auto& f : filters)
        f.values.clear();
    rebuild();
}

void MediaRack::setPluginsByVendor (bool vendor)
{
    byVendor = vendor;
    saveSettings();
    rebuild();
}

//==============================================================================
bool MediaRack::addFavorite (const juce::File& folder)
{
    if (! folder.isDirectory() || favorites.contains (folder.getFullPathName()))
        return false;
    favorites.add (folder.getFullPathName());
    saveSettings();
    // (The manual: a favorite's files go into the MediaBay's database.)
    if (! database.isIncluded (folder))
    {
        database.setIncluded (folder, true);
        database.rescan();
    }
    rebuild();
    return true;
}

void MediaRack::removeFavorite (int index)
{
    if (! juce::isPositiveAndBelow (index, favorites.size()))
        return;
    favorites.remove (index);
    saveSettings();
    rebuild();
}

//==============================================================================
std::vector<MediaRack::Tile> MediaRack::tiles() const
{
    std::vector<Tile> found;
    auto* self = const_cast<MediaRack*> (this);
    if (state.page == Page::home)
    {
        const auto count = [this] (std::set<pr7s::MediaType> types)
        {
            pr7s::MediaQuery q;
            q.types = types;
            return (int) database.find (q).size();
        };
        auto& host = studio.getPluginHost();
        const auto instruments = host.getInstruments().size(), effects = host.getEffects().size();
        found.push_back ({ "VST INSTRUMENTS", instruments == 0 ? juce::String ("THE STUDIO'S SYNTH (PLUGINS > SCAN FOR YOURS)")
                                                               : juce::String (instruments + 1) + " WITH THE STUDIO'S SYNTH", [self] { self->openPage (Page::instruments); } });
        found.push_back ({ "VST EFFECTS", effects == 0 ? juce::String ("NONE FOUND YET (PLUGINS > SCAN)") : juce::String (effects) + " EFFECTS",
                           [self] { self->openPage (Page::effects); } });
        found.push_back ({ "LOOPS & SAMPLES", juce::String (count ({ pr7s::MediaType::audio, pr7s::MediaType::midi })) + " AUDIO AND MIDI FILES", [self] { self->openPage (Page::loops); } });
        found.push_back ({ "PRESETS", juce::String (count ({ pr7s::MediaType::trackPreset })) + " TRACK PRESETS", [self] { self->openPage (Page::presets); } });
        found.push_back ({ "FAVORITES", juce::String (favorites.size()) + " FOLDERS", [self] { self->openPage (Page::favorites); } });
        found.push_back ({ "FILE BROWSER", "YOUR FOLDERS AND DRIVES", [self] { self->openPage (Page::fileBrowser); } });
    }
    else if (state.page == Page::favorites && state.folder == juce::File())
    {
        for (int i = 0; i < favorites.size(); ++i)
        {
            const juce::File folder (favorites[i]);
            found.push_back ({ folder.getFileName().toUpperCase(), folder.getParentDirectory().getFullPathName(), [self, i] { self->openFavorite (i); }, i });
        }
    }
    return found;
}

std::vector<juce::Rectangle<int>> MediaRack::tileBounds (int count) const
{
    std::vector<juce::Rectangle<int>> bounds;
    auto area = contentArea.reduced (2, 4);
    const auto columns = area.getWidth() >= 300 ? 2 : 1;
    const auto w = (area.getWidth() - (columns - 1) * 8) / columns;
    const auto h = 74;
    for (int i = 0; i < count; ++i)
        bounds.push_back ({ area.getX() + (i % columns) * (w + 8), area.getY() + (i / columns) * (h + 8), w, h });
    return bounds;
}

juce::StringArray MediaRack::getTileNames() const
{
    juce::StringArray names;
    for (const auto& tile : tiles())
        names.add (tile.name);
    return names;
}

void MediaRack::clickTile (int index)
{
    const auto all = tiles();
    if (juce::isPositiveAndBelow (index, (int) all.size()))
        all[(size_t) index].action();
}

void MediaRack::showTileMenu (int index)
{
    const auto all = tiles();
    if (! juce::isPositiveAndBelow (index, (int) all.size()) || all[(size_t) index].favorite < 0)
        return;
    juce::PopupMenu menu;
    menu.addItem (1, "Delete (the favorite only: the folder stays)");
    menu.showMenuAsync ({}, [safe = juce::Component::SafePointer<MediaRack> (this), favorite = all[(size_t) index].favorite] (int chosen)
    {
        if (safe != nullptr && chosen == 1)
            safe->removeFavorite (favorite);
    });
}

void MediaRack::mouseDown (const juce::MouseEvent& e)
{
    const auto all = tiles();
    const auto bounds = tileBounds ((int) all.size());
    for (int i = 0; i < (int) bounds.size(); ++i)
        if (bounds[(size_t) i].contains (e.getPosition()))
        {
            if (e.mods.isPopupMenu())
                showTileMenu (i);
            else
                all[(size_t) i].action();
            return;
        }
}

//==============================================================================
void MediaRack::rebuild()
{
    const juce::ScopedValueSetter<bool> svs (refreshing, true);
    // The selection kept by what it is.
    juce::StringArray selectedKeys;
    for (int i = 0; i < list.getNumSelectedRows(); ++i)
        if (const auto* row = getRow (list.getSelectedRow (i)))
            selectedKeys.add (row->text + "|" + row->file.getFullPathName());
    rows.clear();
    const auto text = search.getText().trim();
    const pr7s::TextSearch words (text);

    const auto addPlugins = [&] (bool instruments)
    {
        auto& host = studio.getPluginHost();
        if (instruments)
        {
            Row synth;
            synth.kind = Row::Kind::plugin;
            synth.instrument = synth.studioSynth = true;
            synth.text = host.isStudioSynthFound() ? "Paragon PR7-K" : "Paragon Synth";
            synth.detail = "THE STUDIO'S SYNTH";
            if (words.matches ((synth.text + " paragon synth").toLowerCase()))
            {
                rows.push_back (headingRow ("THE STUDIO'S SYNTH"));
                rows.push_back (synth);
            }
        }
        auto plugins = instruments ? host.getInstruments() : host.getEffects();
        std::map<juce::String, std::vector<juce::PluginDescription>> groups;
        for (const auto& p : plugins)
        {
            if (! words.matches ((p.name + " " + p.manufacturerName + " " + p.category).toLowerCase()))
                continue;
            auto group = byVendor ? p.manufacturerName : p.category.upToFirstOccurrenceOf ("|", false, false);
            if (group.isEmpty())
                group = byVendor ? "OTHER MAKERS" : (instruments ? "INSTRUMENTS" : "EFFECTS");
            groups[group.toUpperCase()].push_back (p);
        }
        for (auto& [group, members] : groups)
        {
            std::sort (members.begin(), members.end(), [] (const auto& a, const auto& b) { return a.name.compareNatural (b.name) < 0; });
            rows.push_back (headingRow (group));
            for (const auto& p : members)
            {
                Row row;
                row.kind = Row::Kind::plugin;
                row.plugin = p;
                row.instrument = instruments;
                row.text = p.name;
                row.detail = (byVendor ? p.category : p.manufacturerName).toUpperCase();
                rows.push_back (row);
            }
        }
    };

    switch (state.page)
    {
        case Page::home: break;
        case Page::instruments: addPlugins (true); break;
        case Page::effects: addPlugins (false); break;
        case Page::fileBrowser:
        {
            if (state.folder == juce::File())
            {
                const auto folderRow = [] (const juce::File& f, const juce::String& name)
                {
                    Row row;
                    row.kind = Row::Kind::folder;
                    row.file = f;
                    row.text = name;
                    row.detail = f.getFullPathName();
                    return row;
                };
                rows.push_back (headingRow ("FAVORITES"));
                for (const auto& f : favorites)
                    rows.push_back (folderRow (juce::File (f), juce::File (f).getFileName()));
                rows.push_back (headingRow ("THIS COMPUTER"));
                rows.push_back (folderRow (juce::File::getSpecialLocation (juce::File::userHomeDirectory), "Home"));
                rows.push_back (folderRow (juce::File::getSpecialLocation (juce::File::userDesktopDirectory), "Desktop"));
                juce::Array<juce::File> roots;
                juce::File::findFileSystemRoots (roots);
                for (const auto& root : roots)
                    rows.push_back (folderRow (root, root.getFullPathName()));
                rows.push_back (headingRow ("USER CONTENT"));
                rows.push_back (folderRow (userContent(), "Paragon PR7-S"));
                break;
            }
            Row up;
            up.kind = Row::Kind::up;
            up.text = "..";
            up.detail = "UP: " + state.folder.getParentDirectory().getFullPathName();
            up.file = state.folder.getParentDirectory();
            rows.push_back (up);
            auto folders = state.folder.findChildFiles (juce::File::findDirectories, false);
            auto files = state.folder.findChildFiles (juce::File::findFiles, false);
            const auto byName = [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; };
            std::sort (folders.begin(), folders.end(), byName);
            std::sort (files.begin(), files.end(), byName);
            for (const auto& f : folders)
            {
                if (f.isHidden() || f.getFileName().startsWithChar ('.'))
                    continue;
                if (! words.matches (f.getFileName().toLowerCase()))
                    continue;
                Row row;
                row.kind = Row::Kind::folder;
                row.file = f;
                row.text = f.getFileName();
                rows.push_back (row);
            }
            for (const auto& f : files)
            {
                const auto type = pr7s::mediaTypeOf (f);
                if (! type || ! words.matches (f.getFileName().toLowerCase()))
                    continue;
                Row row;
                row.kind = Row::Kind::file;
                row.file = f;
                row.type = *type;
                row.text = f.getFileNameWithoutExtension();
                row.detail = pr7s::mediaTypeName (*type) + "  " + sizeText (f.getSize());
                rows.push_back (row);
            }
            break;
        }
        case Page::favorites:
            if (state.folder == juce::File())
                break;
            [[fallthrough]];
        case Page::loops:
        case Page::presets:
        case Page::results:
        {
            auto found = database.find (currentQuery());
            if (shuffleSeed)
                pr7s::MediaDatabase::shuffle (found, *shuffleSeed);
            for (const auto index : found)
            {
                const auto& item = database.getItems()[(size_t) index];
                Row row;
                row.kind = Row::Kind::media;
                row.file = item.file;
                row.type = item.type;
                row.text = item.name;
                juce::StringArray detail;
                detail.add (pr7s::mediaTypeName (item.type));
                if (item.tempo() > 0.0)
                    detail.add (juce::String (item.tempo(), 0) + " BPM");
                if (item.keyName().isNotEmpty())
                    detail.add (item.keyName());
                if (item.tags.category.isNotEmpty())
                    detail.add (joinedPair (item.tags.category, item.tags.subCategory).toUpperCase());
                if (item.tags.style.isNotEmpty())
                    detail.add (item.tags.style.toUpperCase());
                row.detail = detail.joinIntoString ("  ");
                rows.push_back (row);
            }
            break;
        }
    }

    list.updateContent();
    list.deselectAllRows();
    for (int i = 0; i < (int) rows.size() && ! selectedKeys.isEmpty(); ++i)
        if (selectedKeys.contains (rows[(size_t) i].text + "|" + rows[(size_t) i].file.getFullPathName()))
            list.selectRow (i, false, false);
    list.repaint();
    for (auto& column : filterColumns)
        column->refresh();
    layout();
    refreshKeys();
    repaint();
}

void MediaRack::refresh()
{
    rebuild();
}

int MediaRack::getNumRows()
{
    return (int) rows.size();
}

const MediaRack::Row* MediaRack::getRow (int index) const
{
    return juce::isPositiveAndBelow (index, (int) rows.size()) ? &rows[(size_t) index] : nullptr;
}

int MediaRack::findRow (const juce::String& text) const
{
    for (int i = 0; i < (int) rows.size(); ++i)
        if (rows[(size_t) i].text == text)
            return i;
    return -1;
}

void MediaRack::selectRow (int index)
{
    list.selectRow (index);
    selectedRowsChanged (index);
}

//==============================================================================
void MediaRack::use (int index)
{
    const auto* row = getRow (index);
    if (row == nullptr)
        return;
    switch (row->kind)
    {
        case Row::Kind::heading: return;
        case Row::Kind::up:
            return openFolder (state.folder.getParentDirectory() == state.folder ? juce::File() : state.folder.getParentDirectory());
        case Row::Kind::folder:
            return openFolder (row->file);
        case Row::Kind::plugin:
        {
            juce::String error;
            if (row->instrument)
            {
                pr7s::Instrument instrument;
                if (! row->studioSynth)
                {
                    instrument.kind = pr7s::Instrument::Kind::plugin;
                    instrument.plugin = row->plugin;
                }
                if (studio.addTrack (instrument, &error) == 0 || error.isNotEmpty())
                    warn ("Couldn't load the instrument", error.isNotEmpty() ? error : row->text);
            }
            else if (studio.addFxChannel (&row->plugin, &error) == 0)
                warn ("Couldn't load the effect", error);
            return;
        }
        case Row::Kind::media:
        case Row::Kind::file:
        {
            const auto file = row->file;
            stop();
            if (onUseFile)
                onUseFile (file);
            return;
        }
    }
}

bool MediaRack::insertOnSelectedTrack (int index)
{
    const auto* row = getRow (index);
    if (row == nullptr || row->kind != Row::Kind::plugin || row->instrument)
        return false;
    const auto bus = studio.isMasterSelected() ? 0 : studio.getSelectedTrack();
    if (bus != 0 && studio.getProject().findTrack (bus) == nullptr)
        return false;
    juce::String error;
    if (! studio.addInsert (bus, row->plugin, &error))
    {
        warn ("Couldn't insert the effect", error);
        return false;
    }
    return true;
}

bool MediaRack::changeSelectedInstrument (int index)
{
    const auto* row = getRow (index);
    const auto* track = studio.getProject().findTrack (studio.getSelectedTrack());
    if (row == nullptr || row->kind != Row::Kind::plugin || ! row->instrument || track == nullptr || track->kind != pr7s::Track::Kind::instrument)
        return false;
    pr7s::Instrument instrument;
    if (! row->studioSynth)
    {
        instrument.kind = pr7s::Instrument::Kind::plugin;
        instrument.plugin = row->plugin;
    }
    juce::String error;
    if (! studio.changeInstrument (track->id, instrument, &error))
    {
        warn ("Couldn't change the instrument", error);
        return false;
    }
    return true;
}

bool MediaRack::applyToSelectedTrack (int index)
{
    const auto* row = getRow (index);
    if (row == nullptr || row->type != pr7s::MediaType::trackPreset || (row->kind != Row::Kind::media && row->kind != Row::Kind::file))
        return false;
    juce::String error;
    if (! TrackActions::loadPresetFrom (studio, studio.getSelectedTrack(), row->file, error) || error.isNotEmpty())
    {
        warn ("Track preset", error);
        return false;
    }
    return true;
}

void MediaRack::showRowMenu (int index)
{
    const auto* row = getRow (index);
    if (row == nullptr || row->kind == Row::Kind::heading || row->kind == Row::Kind::up)
        return;
    const auto* track = studio.getProject().findTrack (studio.getSelectedTrack());
    const auto trackName = studio.isMasterSelected() ? juce::String ("the master") : track != nullptr ? "\"" + track->name + "\"" : juce::String ("the selected track");
    juce::PopupMenu menu;
    switch (row->kind)
    {
        case Row::Kind::plugin:
            if (row->instrument)
            {
                menu.addItem (1, "Add an Instrument Track with it");
                menu.addItem (2, "Change the Instrument of " + trackName, track != nullptr && track->kind == pr7s::Track::Kind::instrument && ! track->frozen && ! track->disabled);
            }
            else
            {
                menu.addItem (1, "Add an FX Channel with it");
                menu.addItem (3, "Insert on " + trackName, studio.isMasterSelected() || (track != nullptr && ! track->frozen && ! track->disabled));
            }
            break;
        case Row::Kind::folder:
            menu.addItem (5, "Open");
            menu.addItem (6, "Add Favorite", ! favorites.contains (row->file.getFullPathName()));
            break;
        default:
            if (row->type == pr7s::MediaType::trackPreset)
            {
                menu.addItem (1, "Add a Track from the Preset");
                menu.addItem (4, "Apply to " + trackName, track != nullptr);
            }
            else
                menu.addItem (1, row->type == pr7s::MediaType::song ? "Open the Song" : "Into the Song (a new track at the playhead)");
            menu.addItem (7, "Play");
            break;
    }
    menu.showMenuAsync ({}, [safe = juce::Component::SafePointer<MediaRack> (this), index, file = row->file] (int chosen)
    {
        if (safe == nullptr)
            return;
        switch (chosen)
        {
            case 1: safe->use (index); break;
            case 2: safe->changeSelectedInstrument (index); break;
            case 3: safe->insertOnSelectedTrack (index); break;
            case 4: safe->applyToSelectedTrack (index); break;
            case 5: safe->openFolder (file); break;
            case 6: safe->addFavorite (file); break;
            case 7: safe->selectRow (index); safe->play(); break;
            default: break;
        }
    });
}

//==============================================================================
bool MediaRack::play()
{
    const auto* row = getRow (list.getSelectedRow());
    if (row == nullptr || (row->kind != Row::Kind::media && row->kind != Row::Kind::file))
        return false;
    if (row->type != pr7s::MediaType::audio && row->type != pr7s::MediaType::midi)
        return false;
    previewing = row->file;
    const auto ok = studio.previewMedia (row->file, false);
    refreshKeys();
    repaint (previewArea);
    return ok;
}

void MediaRack::stop()
{
    studio.stopMediaPreview();
    refreshKeys();
    repaint (previewArea);
}

void MediaRack::setPreviewOptions (const pr7s::Studio::MediaPreviewOptions& options)
{
    const auto wasPlaying = studio.isMediaPreviewing();
    studio.setMediaPreviewOptions (options);
    refreshKeys();
    if (wasPlaying)
        play();
    repaint (previewArea);
}

void MediaRack::showPitchMenu()
{
    const auto o = studio.getMediaPreviewOptions();
    juce::PopupMenu menu, transpose, toKey;
    for (int semis = -12; semis <= 12; ++semis)
        transpose.addItem (1000 + semis + 12, semis == 0 ? juce::String ("0") : (semis > 0 ? "+" : "") + juce::String (semis), true, o.semitones == semis);
    toKey.addItem (2000, "Off", true, o.toKey < 0);
    static const char* const roots[] { "C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B" };
    for (int pc = 0; pc < 12; ++pc)
        toKey.addItem (2001 + pc, roots[pc], true, o.toKey == pc);
    menu.addSubMenu ("Transpose (semitones)", transpose);
    menu.addSubMenu ("To Root Key", toKey);
    menu.addSeparator();
    menu.addItem (3, "Half Speed", true, o.speed < 0.75);
    menu.addItem (4, "Normal Speed", true, o.speed > 0.75 && o.speed < 1.5);
    menu.addItem (5, "Double Speed", true, o.speed > 1.5);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&pitchKey), [safe = juce::Component::SafePointer<MediaRack> (this)] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        auto options = safe->studio.getMediaPreviewOptions();
        if (chosen >= 2000) options.toKey = chosen - 2001;
        else if (chosen >= 1000) options.semitones = chosen - 1012;
        else options.speed = chosen == 3 ? 0.5 : chosen == 5 ? 2.0 : 1.0;
        safe->setPreviewOptions (options);
    });
}

void MediaRack::setAutoPlay (bool on)
{
    autoPlay = on;
    saveSettings();
    refreshKeys();
}

//==============================================================================
void MediaRack::paintListBoxItem (int index, juce::Graphics& g, int width, int height, bool selected)
{
    const auto* row = getRow (index);
    if (row == nullptr)
        return;
    auto area = juce::Rectangle<int> (0, 0, width, height);
    if (row->kind == Row::Kind::heading)
    {
        g.setColour (ParagonColours::accent);
        g.setFont (ParagonLookAndFeel::titleFont (13.0f));
        g.drawText (row->text, area.reduced (8, 0).withTrimmedTop (6), juce::Justification::centredLeft, true);
        g.setColour (ParagonColours::panelEdge);
        g.fillRect (8, height - 1, width - 16, 1);
        return;
    }
    if (selected)
        g.fillAll (ParagonColours::accent.withAlpha (0.22f));
    else if (index % 2 == 1)
        g.fillAll (juce::Colour (0xff120f13));
    if (row->file == previewing && studio.isMediaPreviewing() && row->file != juce::File())
    {
        g.setColour (ParagonColours::accent);
        g.fillRect (0, 0, 3, height);
    }
    area.reduce (8, 2);
    // A tab: the type's colour (media), red (instruments), pink (effects), pearl (folders).
    auto tab = area.removeFromLeft (4).withSizeKeepingCentre (4, 16).toFloat();
    switch (row->kind)
    {
        case Row::Kind::plugin: g.setColour (row->instrument ? pr7s::FamilyColours::red : pr7s::FamilyColours::pink); break;
        case Row::Kind::folder: case Row::Kind::up: g.setColour (juce::Colour (0xfff4eeea).withAlpha (0.6f)); break;
        default: g.setColour (typeColour (row->type)); break;
    }
    g.fillRoundedRectangle (tab, 1.5f);
    area.removeFromLeft (8);
    const auto twoLines = row->detail.isNotEmpty() && height >= 26;
    g.setColour (ParagonColours::text);
    g.setFont (interFont (12.5f, row->kind == Row::Kind::folder || row->kind == Row::Kind::up));
    if (! twoLines)
    {
        g.drawText (row->text, area, juce::Justification::centredLeft, true);
        return;
    }
    // Name on the left; what it is on the right (dim), if there's room.
    const auto detailWidth = juce::jmin (area.getWidth() / 2, juce::GlyphArrangement::getStringWidthInt (interFont (10.5f), row->detail) + 4);
    auto detail = area.removeFromRight (detailWidth);
    g.drawText (row->text, area.withTrimmedRight (6), juce::Justification::centredLeft, true);
    g.setColour (ParagonColours::textDim);
    g.setFont (interFont (10.5f));
    g.drawText (row->detail, detail, juce::Justification::centredRight, true);
}

void MediaRack::listBoxItemClicked (int index, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (! list.isRowSelected (index))
            list.selectRow (index);
        showRowMenu (index);
    }
}

void MediaRack::listBoxItemDoubleClicked (int index, const juce::MouseEvent&)
{
    use (index);
}

void MediaRack::selectedRowsChanged (int lastRowSelected)
{
    refreshKeys();
    repaint (previewArea);
    if (autoPlay && ! refreshing && lastRowSelected >= 0 && list.getNumSelectedRows() == 1)
        if (const auto* row = getRow (lastRowSelected); row != nullptr && (row->type == pr7s::MediaType::audio || row->type == pr7s::MediaType::midi)
                                                        && (row->kind == Row::Kind::media || row->kind == Row::Kind::file))
            play();
}

juce::String MediaRack::getDragDescription (int index) const
{
    const auto* row = getRow (index);
    if (row == nullptr)
        return {};
    if (row->kind == Row::Kind::plugin)
        return (row->instrument ? instrumentPrefix : effectPrefix) + (row->studioSynth ? juce::String ("studio") : row->plugin.createIdentifierString());
    if (row->kind == Row::Kind::media || row->kind == Row::Kind::file)
        return MediaBayPage::dragDescriptionFor ({ row->file.getFullPathName() });
    return {};
}

juce::var MediaRack::getDragSourceDescription (const juce::SparseSet<int>& selected)
{
    // Files: all the selected ones; a plug-in: the first.
    juce::StringArray paths;
    for (int i = 0; i < selected.size(); ++i)
        if (const auto* row = getRow (selected[i]))
        {
            if (row->kind == Row::Kind::plugin)
                return getDragDescription (selected[i]);
            if (row->kind == Row::Kind::media || row->kind == Row::Kind::file)
                paths.add (row->file.getFullPathName());
        }
    return paths.isEmpty() ? juce::var() : juce::var (MediaBayPage::dragDescriptionFor (paths));
}

juce::String MediaRack::getTooltipForRow (int index)
{
    const auto* row = getRow (index);
    if (row == nullptr || row->kind == Row::Kind::heading)
        return {};
    if (row->kind == Row::Kind::plugin)
        return row->instrument ? "Double-click or drag onto the track list or the arrangement: a new instrument track. Right-click: change the selected track's instrument"
                               : "Double-click or drag onto the track list or the arrangement: a new FX channel. Right-click: insert it on the selected track";
    if (row->kind == Row::Kind::folder || row->kind == Row::Kind::up)
        return row->file.getFullPathName() + "\nDouble-click to open";
    return row->file.getFullPathName() + "\nDouble-click: into the song. Drag onto a track.";
}

//==============================================================================
bool MediaRack::isRackDescription (const juce::String& description)
{
    return description.startsWith (instrumentPrefix) || description.startsWith (effectPrefix);
}

std::optional<pr7s::Instrument> MediaRack::instrumentFor (pr7s::Studio& studio, const juce::String& description)
{
    if (! description.startsWith (instrumentPrefix))
        return std::nullopt;
    const auto id = description.substring (instrumentPrefix.length());
    if (id == "studio")
        return pr7s::Instrument {};
    for (const auto& p : studio.getPluginHost().getInstruments())
        if (p.createIdentifierString() == id)
        {
            pr7s::Instrument instrument;
            instrument.kind = pr7s::Instrument::Kind::plugin;
            instrument.plugin = p;
            return instrument;
        }
    return std::nullopt;
}

std::optional<juce::PluginDescription> MediaRack::effectFor (pr7s::Studio& studio, const juce::String& description)
{
    if (! description.startsWith (effectPrefix))
        return std::nullopt;
    const auto id = description.substring (effectPrefix.length());
    for (const auto& p : studio.getPluginHost().getEffects())
        if (p.createIdentifierString() == id)
            return p;
    return std::nullopt;
}

//==============================================================================
pr7s::AttributeId MediaRack::getFilterColumn (int column) const
{
    return juce::isPositiveAndBelow (column, numFilterColumns) ? filters[(size_t) column].attribute : pr7s::AttributeId {};
}

void MediaRack::setFilterColumn (int column, const pr7s::AttributeId& id)
{
    if (! juce::isPositiveAndBelow (column, numFilterColumns))
        return;
    filters[(size_t) column] = { id, {} };
    rebuild();
}

std::vector<std::pair<juce::String, int>> MediaRack::getFilterValues (int column) const
{
    if (! juce::isPositiveAndBelow (column, numFilterColumns))
        return {};
    auto values = database.valueCounts (currentQuery(), filters[(size_t) column].attribute);
    for (const auto& chosen : filters[(size_t) column].values)
        if (std::none_of (values.begin(), values.end(), [&] (const auto& v) { return v.first == chosen; }))
            values.push_back ({ chosen, 0 });
    return values;
}

juce::StringArray MediaRack::getFilterSelection (int column) const
{
    return juce::isPositiveAndBelow (column, numFilterColumns) ? filters[(size_t) column].values : juce::StringArray();
}

void MediaRack::clickFilterValue (int column, const juce::String& value, bool addToOthers)
{
    if (! juce::isPositiveAndBelow (column, numFilterColumns))
        return;
    auto& values = filters[(size_t) column].values;
    if (addToOthers)
    {
        if (values.contains (value))
            values.removeString (value);
        else
            values.add (value);
    }
    else if (values.size() == 1 && values[0] == value)
        values.clear();
    else
        values = juce::StringArray (value);
    rebuild();
}

//==============================================================================
void MediaRack::refreshKeys()
{
    using T = pr7s::MediaType;
    const auto results = isResultsPage();
    const auto base = pageTypes();
    for (auto [key, type] : { std::pair { &audioKey, T::audio }, { &midiKey, T::midi }, { &presetsKey, T::trackPreset }, { &songsKey, T::song } })
    {
        key->setVisible (results);
        key->setEnabled (base.count (type) > 0);
        key->setToggleState (typesShown.count (type) > 0, juce::dontSendNotification);
    }
    for (auto* key : { &shuffleKey, &filterKey, &clearKey })
        key->setVisible (results);
    filterKey.setToggleState (filtersShown, juce::dontSendNotification);
    clearKey.setToggleState (isAttributeFilterSet(), juce::dontSendNotification);
    clearKey.setEnabled (isAttributeFilterSet());
    const auto plugins = state.page == Page::instruments || state.page == Page::effects;
    categoryKey.setVisible (plugins);
    vendorKey.setVisible (plugins);
    categoryKey.setToggleState (! byVendor, juce::dontSendNotification);
    vendorKey.setToggleState (byVendor, juce::dontSendNotification);
    upKey.setVisible (state.page == Page::fileBrowser);
    upKey.setEnabled (state.folder != juce::File());
    favoriteKey.setVisible (state.page == Page::fileBrowser || (state.page == Page::favorites && state.folder == juce::File()));
    backKey.setEnabled (! history.empty());
    for (auto* key : { &playKey, &autoPlayKey, &linkKey, &waitKey, &pitchKey })
        key->setVisible (hasPreviewer());
    {
        const auto& o = studio.getMediaPreviewOptions();
        linkKey.setToggleState (o.linkTempo, juce::dontSendNotification);
        waitKey.setToggleState (o.waitForPlay, juce::dontSendNotification);
        pitchKey.setToggleState (o.semitones != 0 || o.toKey >= 0 || o.speed != 1.0, juce::dontSendNotification);
    }
    const auto playing = studio.isMediaPreviewing();
    playKey.setButtonText (playing ? "STOP" : "PLAY");
    playKey.setToggleState (playing, juce::dontSendNotification);
    autoPlayKey.setToggleState (autoPlay, juce::dontSendNotification);
    resetSearchKey.setEnabled (search.getText().isNotEmpty());
}

void MediaRack::timerCallback()
{
    if (database.takeScanResults())
        rebuild();
    // Favorites added in the MediaBay window's file browser (0.22.1) show here too.
    if (auto* settings = studio.getSettings(); settings != nullptr && ++favoritesCheck % 20 == 0)
    {
        auto saved = juce::StringArray::fromLines (settings->getValue ("mediaRackFavorites"));
        saved.removeEmptyStrings();
        if (saved != favorites)
        {
            favorites = saved;
            rebuild();
        }
    }
    const auto playing = studio.isMediaPreviewing();
    if ((playKey.getButtonText() == "STOP") != playing)
        refreshKeys();
    if (playing && hasPreviewer())
        repaint (waveArea.expanded (2));
}

//==============================================================================
void MediaRack::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff100d11));
    // The tab row: MEDIA (the only tab), lit.
    auto tabRow = getLocalBounds().removeFromTop (tabHeight);
    auto tab = tabRow.removeFromLeft (96).reduced (4, 4);
    g.setColour (ParagonColours::accent.withAlpha (0.2f));
    g.fillRoundedRectangle (tab.toFloat(), 3.0f);
    g.setColour (ParagonColours::accent);
    g.fillRect (tab.getX(), tab.getBottom() - 2, tab.getWidth(), 2);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (15.0f));
    g.drawText ("MEDIA", tab, juce::Justification::centred, false);
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (0, tabHeight - 1, getWidth(), 1);

    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::titleFont (13.0f));
    g.drawText (getAddress(), addressArea, juce::Justification::centredLeft, true);

    if (state.page == Page::home || (state.page == Page::favorites && state.folder == juce::File()))
    {
        const auto all = tiles();
        const auto bounds = tileBounds ((int) all.size());
        for (int i = 0; i < (int) all.size(); ++i)
        {
            const auto b = bounds[(size_t) i].toFloat();
            juce::ColourGradient glow (pr7s::FamilyColours::red.withAlpha (0.22f), b.getTopLeft(), pr7s::FamilyColours::orange.withAlpha (0.10f), b.getBottomRight(), false);
            g.setGradientFill (glow);
            g.fillRoundedRectangle (b, 6.0f);
            g.setColour (ParagonColours::panelEdge);
            g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (juce::Colour (0xfff4eeea));
            g.setFont (ParagonLookAndFeel::titleFont (17.0f));
            g.drawText (all[(size_t) i].name, b.reduced (12, 10).removeFromTop (26), juce::Justification::centredLeft, true);
            g.setColour (ParagonColours::textDim);
            g.setFont (interFont (11.0f));
            g.drawFittedText (all[(size_t) i].detail, b.reduced (12, 10).withTrimmedTop (28).toNearestInt(), juce::Justification::topLeft, 2);
        }
        if (all.empty())
        {
            g.setColour (ParagonColours::textDim);
            g.setFont (interFont (12.5f));
            g.drawFittedText ("No favorites yet: ADD FAVORITE, or right-click a folder in the FILE BROWSER.", contentArea.reduced (12), juce::Justification::topLeft, 3);
        }
    }
    if (hasPreviewer())
        paintPreviewer (g);
}

void MediaRack::paintPreviewer (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff141015));
    g.fillRoundedRectangle (previewArea.toFloat(), 5.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (previewArea.toFloat().reduced (0.5f), 5.0f, 1.0f);
    const auto* row = getRow (list.getSelectedRow());
    const auto wave = waveArea.toFloat();
    g.setColour (juce::Colour (0xff0b090c));
    g.fillRoundedRectangle (wave, 3.0f);
    const auto sound = studio.getMediaPreviewSound();
    const auto heard = row != nullptr && row->file == previewing && row->type == pr7s::MediaType::audio && sound != nullptr && ! sound->peaks.empty();
    if (heard)
    {
        juce::Path shape;
        const auto n = (int) sound->peaks.size();
        const auto columns = juce::jmax (1, (int) wave.getWidth());
        for (int x = 0; x < columns; ++x)
        {
            const auto from = (size_t) ((double) x / columns * n), to = (size_t) juce::jmax ((double) from + 1, (double) (x + 1) / columns * n);
            float peak = 0.0f;
            for (auto i = from; i < juce::jmin (to, (size_t) n); ++i)
                peak = juce::jmax (peak, sound->peaks[i]);
            const auto h = juce::jmin (1.0f, peak) * wave.getHeight() * 0.48f;
            shape.addRectangle (wave.getX() + (float) x, wave.getCentreY() - h, 1.0f, juce::jmax (1.0f, h * 2.0f));
        }
        g.setColour (pr7s::FamilyColours::orange.withAlpha (0.8f));
        g.fillPath (shape);
    }
    else
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (interFont (11.0f));
        const auto text = row == nullptr || (row->kind != Row::Kind::media && row->kind != Row::Kind::file) ? juce::String ("Select a file to hear it")
                        : row->type == pr7s::MediaType::midi ? juce::String ("MIDI: through the selected track's instrument")
                        : row->type == pr7s::MediaType::audio ? juce::String ("PLAY to hear it") : juce::String ("No preview: double-click or drag it in");
        g.drawText (text, waveArea.reduced (8, 0), juce::Justification::centredLeft, true);
    }
    if (row != nullptr && row->file == previewing && studio.isMediaPreviewing())
    {
        const auto x = wave.getX() + (float) studio.getMediaPreviewProgress() * wave.getWidth();
        g.setColour (juce::Colour (0xfff4eeea));
        g.fillRect (x, wave.getY(), 1.5f, wave.getHeight());
    }
}

void MediaRack::resized()
{
    layout();
}

void MediaRack::layout()
{
    auto area = getLocalBounds().reduced (6, 0);
    area.removeFromTop (tabHeight + 6);
    auto nav = area.removeFromTop (28);
    allKey.setBounds (nav.removeFromRight (44));
    nav.removeFromRight (4);
    resetSearchKey.setBounds (nav.removeFromRight (30));
    nav.removeFromRight (4);
    search.setBounds (nav);
    area.removeFromTop (6);
    auto address = area.removeFromTop (26);
    backKey.setBounds (address.removeFromLeft (52));
    address.removeFromLeft (4);
    homeKey.setBounds (address.removeFromLeft (54));
    address.removeFromLeft (8);
    addressArea = address;
    area.removeFromTop (6);

    // The page's own keys, shared out across the width.
    const auto row = [&] (std::initializer_list<std::pair<juce::TextButton*, int>> keys)
    {
        auto line = area.removeFromTop (26);
        int total = 0;
        for (const auto& k : keys)
            total += k.second;
        const auto gaps = 4 * ((int) keys.size() - 1);
        auto x = line.getX();
        for (const auto& k : keys)
        {
            const auto w = (line.getWidth() - gaps) * k.second / juce::jmax (1, total);
            k.first->setBounds (x, line.getY(), w, line.getHeight());
            x += w + 4;
        }
        area.removeFromTop (6);
    };
    if (isResultsPage())
    {
        row ({ { &audioKey, 52 }, { &midiKey, 44 }, { &presetsKey, 62 }, { &songsKey, 54 } });
        row ({ { &shuffleKey, 66 }, { &filterKey, 60 }, { &clearKey, 56 } });
    }
    else if (state.page == Page::instruments || state.page == Page::effects)
        row ({ { &categoryKey, 1 }, { &vendorKey, 1 } });
    else if (state.page == Page::fileBrowser)
        row ({ { &upKey, 1 }, { &favoriteKey, 2 } });
    else if (state.page == Page::favorites)
        row ({ { &favoriteKey, 1 } });

    previewArea = waveArea = {};
    if (hasPreviewer())
    {
        previewArea = area.removeFromBottom (previewerHeight).withTrimmedBottom (6);
        auto inner = previewArea.reduced (8, 8);
        auto keys = inner.removeFromBottom (24);
        // (Shared out: PLAY, AUTO PLAY, LINK, WAIT, PITCH.)
        const auto w = (keys.getWidth() - 4 * 4) / 5;
        for (auto* key : { &playKey, &autoPlayKey, &linkKey, &waitKey, &pitchKey })
        {
            key->setBounds (keys.removeFromLeft (key == &autoPlayKey ? w + 12 : key == &playKey ? w - 4 : w - 2));
            keys.removeFromLeft (4);
        }
        inner.removeFromBottom (6);
        waveArea = inner;
        area.removeFromBottom (6);
    }
    const auto showFilters = filtersShown && isResultsPage();
    for (auto& column : filterColumns)
        column->setVisible (showFilters);
    if (showFilters)
    {
        auto strip = area.removeFromTop (filtersHeight);
        const auto w = (strip.getWidth() - 6) / 2;
        filterColumns[0]->setBounds (strip.removeFromLeft (w));
        filterColumns[1]->setBounds (strip.removeFromRight (w));
        area.removeFromTop (6);
    }
    contentArea = area;
    const auto tilesShown = state.page == Page::home || (state.page == Page::favorites && state.folder == juce::File());
    list.setVisible (! tilesShown);
    list.setBounds (area);
    repaint();
}
