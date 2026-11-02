#include "ui/MediaBayPage.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    juce::Font interFont (float height, bool bold = false) { return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain)); }

    enum Column { ratingColumn = 1, nameColumn, typeColumn, lengthColumn, tempoColumn, keyColumn, formatColumn, folderColumn, sizeColumn, dateColumn };

    std::optional<pr7s::MediaColumn> mediaColumnOf (int column)
    {
        using C = pr7s::MediaColumn;
        switch (column)
        {
            case ratingColumn: return C::rating;
            case nameColumn: return C::name;
            case typeColumn: return C::type;
            case lengthColumn: return C::length;
            case tempoColumn: return C::tempo;
            case keyColumn: return C::key;
            case folderColumn: return C::folder;
            case sizeColumn: return C::size;
            case dateColumn: return C::date;
            default: return std::nullopt;
        }
    }

    // Each type in one of the family's colours (a small tab beside the name).
    juce::Colour typeColour (pr7s::MediaType type)
    {
        switch (type)
        {
            case pr7s::MediaType::audio: return pr7s::FamilyColours::orange;
            case pr7s::MediaType::midi: return pr7s::FamilyColours::pink;
            case pr7s::MediaType::trackPreset: return pr7s::FamilyColours::red;
            case pr7s::MediaType::song: return juce::Colour (0xfff4eeea);
        }
        return pr7s::FamilyColours::red;
    }

    juce::String lengthText (double seconds)
    {
        if (seconds <= 0.0)
            return {};
        const auto whole = (int) std::round (seconds * 10.0);
        const auto minutes = whole / 600;
        const auto rest = (whole % 600) / 10.0;
        return minutes > 0 ? juce::String (minutes) + ":" + juce::String (rest, 1).paddedLeft ('0', 4) : juce::String (rest, 1) + " s";
    }

    juce::String sizeText (juce::int64 bytes)
    {
        if (bytes >= 1024 * 1024)
            return juce::String ((double) bytes / (1024.0 * 1024.0), 1) + " MB";
        return juce::String (juce::jmax ((juce::int64) 1, bytes / 1024)) + " KB";
    }

    void drawStars (juce::Graphics& g, juce::Rectangle<float> area, int rating)
    {
        const auto size = juce::jmin (area.getHeight() * 0.62f, area.getWidth() / 5.0f);
        for (int i = 0; i < 5; ++i)
        {
            juce::Path star;
            const auto centre = juce::Point<float> (area.getX() + (float) i * area.getWidth() / 5.0f + area.getWidth() / 10.0f, area.getCentreY());
            star.addStar (centre, 5, size * 0.22f, size * 0.5f);   // (the first point at 12 o'clock)
            if (i < rating)
            {
                g.setColour (pr7s::FamilyColours::orange);
                g.fillPath (star);
            }
            else
            {
                g.setColour (ParagonColours::textDim.withAlpha (0.35f));
                g.strokePath (star, juce::PathStrokeType (1.0f));
            }
        }
    }

    void styleKey (juce::TextButton& key)
    {
        key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key.setWantsKeyboardFocus (false);
    }
}

//==============================================================================
// LOCATIONS: "ALL LOCATIONS", then each scanned folder (its name, its path under it).
class MediaBayPage::Locations final : public juce::ListBox,
                                     private juce::ListBoxModel
{
public:
    explicit Locations (MediaBayPage& p) : page (p)
    {
        setModel (this);
        setRowHeight (40);
        setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff100d11));
        setOutlineThickness (0);
    }

    int getNumRows() override { return page.database.getLocations().size() + 1; }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (selected)
        {
            g.setColour (ParagonColours::accent.withAlpha (0.18f));
            g.fillRect (0, 0, width, height);
            g.setColour (ParagonColours::accent);
            g.fillRect (0, 0, 3, height);
        }
        auto area = juce::Rectangle<int> (0, 0, width, height).reduced (10, 4);
        if (row == 0)
        {
            g.setColour (ParagonColours::text);
            g.setFont (ParagonLookAndFeel::titleFont (14.0f));
            g.drawText ("ALL LOCATIONS", area, juce::Justification::centredLeft, false);
            return;
        }
        const juce::File folder (page.database.getLocations()[row - 1]);
        g.setColour (ParagonColours::text);
        g.setFont (ParagonLookAndFeel::titleFont (14.0f));
        g.drawText (folder.getFileName().toUpperCase(), area.removeFromTop (18), juce::Justification::centredLeft, true);
        g.setColour (ParagonColours::textDim);
        g.setFont (interFont (10.5f));
        g.drawText (folder.getParentDirectory().getFullPathName(), area, juce::Justification::centredLeft, true);
    }

    void selectedRowsChanged (int row) override
    {
        if (! updating)
            page.selectLocation (row <= 0 ? -1 : row - 1);
    }

    void show (int index)
    {
        const juce::ScopedValueSetter<bool> svs (updating, true);
        updateContent();
        selectRow (index + 1, true, true);
    }

private:
    MediaBayPage& page;
    bool updating = false;
};

//==============================================================================
MediaBayPage::MediaBayPage (pr7s::Studio& s)
    : studio (s), database (s.getMediaBay())
{
    using pr7s::MediaType;
    // As last time.
    if (auto* settings = studio.getSettings())
    {
        const auto types = juce::StringArray::fromTokens (settings->getValue ("mediaBayTypes"), ",", "");
        for (const auto& t : types)
            for (const auto type : { MediaType::audio, MediaType::midi, MediaType::trackPreset, MediaType::song })
                if (t == pr7s::mediaTypeName (type))
                    query.types.insert (type);
        query.minimumRating = juce::jlimit (0, 5, settings->getIntValue ("mediaBayRating", 0));
        query.subfolders = settings->getBoolValue ("mediaBaySubfolders", true);
        autoPlay = settings->getBoolValue ("mediaBayAutoPlay", true);
        cycle = settings->getBoolValue ("mediaBayCycle", false);
    }

    search.setTextToShowWhenEmpty ("SEARCH: drum and (kick or snare) -808", ParagonColours::textDim);
    search.setFont (interFont (14.0f));
    search.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1b171c));
    search.setColour (juce::TextEditor::textColourId, ParagonColours::text);
    search.setColour (juce::TextEditor::outlineColourId, ParagonColours::panelEdge);
    search.setColour (juce::TextEditor::focusedOutlineColourId, ParagonColours::accent);
    search.setTooltip ("Words all found (and, +, or a space); either (or, a comma); none (not, a leading minus); (parentheses); \"a phrase\". "
                       "Looks in the name, the folder, the type, the tempo and the key");
    search.onTextChange = [this] { setSearch (search.getText()); };
    addAndMakeVisible (search);

    for (auto* key : { &audioKey, &midiKey, &presetsKey, &songsKey, &subfoldersKey, &shuffleKey, &resetKey, &rescanKey,
                       &addKey, &removeKey, &playKey, &cycleKey, &autoPlayKey, &useKey })
    {
        styleKey (*key);
        addAndMakeVisible (*key);
    }
    const auto typeKey = [this] (juce::TextButton& key, MediaType type, const char* tip)
    {
        key.setTooltip (tip);
        key.onClick = [this, &key, type] { setTypeShown (type, ! key.getToggleState()); };
    };
    typeKey (audioKey, MediaType::audio, "Audio files (WAV, AIFF, FLAC, Ogg, MP3). With no type lit, every type is shown");
    typeKey (midiKey, MediaType::midi, "MIDI files. With no type lit, every type is shown");
    typeKey (presetsKey, MediaType::trackPreset, "Track presets (.pr7track). With no type lit, every type is shown");
    typeKey (songsKey, MediaType::song, "Songs (.pr7s). With no type lit, every type is shown");
    subfoldersKey.setTooltip ("A location's subfolders too (off: only the files directly in it)");
    subfoldersKey.onClick = [this] { setSubfolders (! query.subfolders); };
    shuffleKey.setTooltip ("The results in a random order");
    shuffleKey.onClick = [this] { shuffle (juce::Random::getSystemRandom().nextInt()); };
    resetKey.setTooltip ("Clear the search, the types and the rating filter");
    resetKey.onClick = [this] { resetFilters(); };
    rescanKey.setTooltip ("Look through the locations again for new, changed or removed files (it waits while the song plays)");
    rescanKey.onClick = [this] { database.rescan(); refreshKeys(); };
    addKey.setTooltip ("A folder to scan, with its subfolders");
    addKey.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("A folder for the MediaBay to scan", juce::File::getSpecialLocation (juce::File::userHomeDirectory));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safe = juce::Component::SafePointer<MediaBayPage> (this)] (const juce::FileChooser& c)
                              {
                                  if (safe != nullptr && c.getResult() != juce::File())
                                      safe->addLocation (c.getResult());
                              });
    };
    removeKey.setTooltip ("Stop scanning the selected location (its files stay where they are; your ratings are kept)");
    removeKey.onClick = [this] { removeSelectedLocation(); };
    playKey.setTooltip ("Hear the selected file (audio over the output; MIDI through the selected track's instrument)");
    playKey.onClick = [this] { studio.isMediaPreviewing() ? stop() : (void) play(); };
    cycleKey.setTooltip ("Audio plays round and round");
    cycleKey.onClick = [this] { setCycle (! cycle); };
    autoPlayKey.setTooltip ("Play each file as it's selected");
    autoPlayKey.onClick = [this] { setAutoPlay (! autoPlay); };
    useKey.setTooltip ("Into the song (as a double-click): audio or MIDI on a new track at the playhead, a track preset as a new track, a song opened");
    useKey.onClick = [this] { use (getSelectedRow()); };

    ratingBox.addItem ("ANY RATING", 1);
    for (int stars = 1; stars <= 5; ++stars)
        ratingBox.addItem (juce::String::repeatedString (juce::String::fromUTF8 ("\xe2\x98\x85"), stars) + (stars < 5 ? " OR MORE" : ""), stars + 1);
    ratingBox.setSelectedId (query.minimumRating + 1, juce::dontSendNotification);
    ratingBox.setTooltip ("Only files rated this or better (rate them in the results' first column)");
    ratingBox.onChange = [this] { setMinimumRating (ratingBox.getSelectedId() - 1); };
    addAndMakeVisible (ratingBox);

    level.setRange (0.0, 1.0, 0.01);
    level.setValue (studio.getMediaPreviewLevel(), juce::dontSendNotification);
    level.setColour (juce::Slider::trackColourId, ParagonColours::accent);
    level.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff1b171c));
    level.setTooltip ("The preview's level");
    level.onValueChange = [this] { studio.setMediaPreviewLevel ((float) level.getValue()); };
    addAndMakeVisible (level);

    auto& header = table.getHeader();
    const auto flags = juce::TableHeaderComponent::defaultFlags;
    header.addColumn ("RATING", ratingColumn, 96, 80, 120, flags);
    header.addColumn ("NAME", nameColumn, 280, 120, 800, flags);
    header.addColumn ("TYPE", typeColumn, 110, 70, 160, flags);
    header.addColumn ("LENGTH", lengthColumn, 74, 60, 120, flags);
    header.addColumn ("TEMPO", tempoColumn, 66, 50, 100, flags);
    header.addColumn ("KEY", keyColumn, 54, 40, 90, flags);
    header.addColumn ("FORMAT", formatColumn, 110, 70, 180, flags & ~juce::TableHeaderComponent::sortable);
    header.addColumn ("FOLDER", folderColumn, 220, 100, 800, flags);
    header.addColumn ("SIZE", sizeColumn, 76, 60, 120, flags);
    header.addColumn ("DATE", dateColumn, 120, 90, 180, flags);
    header.setColour (juce::TableHeaderComponent::backgroundColourId, juce::Colour (0xff1b171c));
    header.setColour (juce::TableHeaderComponent::textColourId, ParagonColours::text);
    header.setColour (juce::TableHeaderComponent::outlineColourId, ParagonColours::panelEdge);
    table.setModel (this);
    table.setRowHeight (26);
    table.setMultipleSelectionEnabled (true);
    table.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff0c0a0d));
    table.setOutlineThickness (0);
    addAndMakeVisible (table);

    locations = std::make_unique<Locations> (*this);
    addAndMakeVisible (*locations);

    setSize (1400, 820);
    refreshLocations();
    refresh();
    refreshKeys();
    startTimerHz (20);
}

MediaBayPage::~MediaBayPage()
{
    studio.stopMediaPreview();
    table.setModel (nullptr);
}

//==============================================================================
void MediaBayPage::saveSettings()
{
    auto* settings = studio.getSettings();
    if (settings == nullptr)
        return;
    juce::StringArray types;
    for (const auto type : query.types)
        types.add (pr7s::mediaTypeName (type));
    settings->setValue ("mediaBayTypes", types.joinIntoString (","));
    settings->setValue ("mediaBayRating", query.minimumRating);
    settings->setValue ("mediaBaySubfolders", query.subfolders);
    settings->setValue ("mediaBayAutoPlay", autoPlay);
    settings->setValue ("mediaBayCycle", cycle);
}

void MediaBayPage::refreshKeys()
{
    using pr7s::MediaType;
    audioKey.setToggleState (query.types.count (MediaType::audio) > 0, juce::dontSendNotification);
    midiKey.setToggleState (query.types.count (MediaType::midi) > 0, juce::dontSendNotification);
    presetsKey.setToggleState (query.types.count (MediaType::trackPreset) > 0, juce::dontSendNotification);
    songsKey.setToggleState (query.types.count (MediaType::song) > 0, juce::dontSendNotification);
    subfoldersKey.setToggleState (query.subfolders, juce::dontSendNotification);
    cycleKey.setToggleState (cycle, juce::dontSendNotification);
    autoPlayKey.setToggleState (autoPlay, juce::dontSendNotification);
    const auto playing = studio.isMediaPreviewing();
    playKey.setButtonText (playing ? "STOP" : "PLAY");
    playKey.setToggleState (playing, juce::dontSendNotification);
    removeKey.setEnabled (locationIndex >= 0);
    useKey.setEnabled (getSelectedRow() >= 0);
    rescanKey.setEnabled (! database.isScanning());
    repaint (0, 0, getWidth(), titleHeight);
}

void MediaBayPage::refreshLocations()
{
    if (locationIndex >= database.getLocations().size())
        locationIndex = -1;
    locations->show (locationIndex);
}

void MediaBayPage::refresh()
{
    // The selected files stay selected when they're still there.
    juce::StringArray selectedPaths;
    const auto rows = table.getSelectedRows();
    for (int i = 0; i < rows.size(); ++i)
        if (const auto* item = getResult (rows[i]))
            selectedPaths.add (item->file.getFullPathName());

    query.folder = locationIndex >= 0 ? juce::File (database.getLocations()[locationIndex]) : juce::File();
    results = database.find (query);
    if (sortOrder)
        database.sort (results, sortOrder->first, sortOrder->second);
    table.updateContent();
    table.deselectAllRows();
    for (int row = 0; row < (int) results.size() && ! selectedPaths.isEmpty(); ++row)
        if (selectedPaths.contains (getResult (row)->file.getFullPathName()))
            table.selectRow (row, false, false);
    table.repaint();
    refreshKeys();
}

const pr7s::MediaItem* MediaBayPage::getResult (int row) const
{
    if (! juce::isPositiveAndBelow (row, (int) results.size()))
        return nullptr;
    return &database.getItems()[(size_t) results[(size_t) row]];
}

int MediaBayPage::getSelectedRow() const
{
    return table.getSelectedRow();
}

juce::String MediaBayPage::getStatusText() const
{
    juce::String text;
    text << juce::String (results.size()) << (results.size() == 1 ? " FILE" : " FILES");
    if ((int) results.size() != (int) database.getItems().size())
        text << " OF " << juce::String (database.getItems().size());
    if (database.isScanning())
        text << "  -  SCANNING (" << juce::String (database.getFilesSeen()) << " SEEN)";
    else if (database.getLocations().isEmpty())
        text << "  -  ADD A FOLDER TO SCAN";
    return text;
}

//==============================================================================
void MediaBayPage::setSearch (const juce::String& text)
{
    query.text = text;
    if (search.getText() != text)
        search.setText (text, false);
    refresh();
}

void MediaBayPage::setTypeShown (pr7s::MediaType type, bool shown)
{
    if (shown)
        query.types.insert (type);
    else
        query.types.erase (type);
    saveSettings();
    refresh();
}

void MediaBayPage::setMinimumRating (int stars)
{
    query.minimumRating = juce::jlimit (0, 5, stars);
    ratingBox.setSelectedId (query.minimumRating + 1, juce::dontSendNotification);
    saveSettings();
    refresh();
}

void MediaBayPage::setSubfolders (bool on)
{
    query.subfolders = on;
    saveSettings();
    refresh();
}

void MediaBayPage::selectLocation (int index)
{
    locationIndex = juce::isPositiveAndBelow (index, database.getLocations().size()) ? index : -1;
    refresh();
}

void MediaBayPage::resetFilters()
{
    query.types.clear();
    query.minimumRating = 0;
    ratingBox.setSelectedId (1, juce::dontSendNotification);
    sortOrder.reset();
    table.getHeader().setSortColumnId (0, true);
    saveSettings();
    setSearch ({});
}

void MediaBayPage::shuffle (juce::uint32 seed)
{
    sortOrder.reset();
    table.getHeader().setSortColumnId (0, true);
    pr7s::MediaDatabase::shuffle (results, seed);
    table.updateContent();
    table.repaint();
}

void MediaBayPage::sortBy (pr7s::MediaColumn column, bool ascending)
{
    sortOrder = std::pair { column, ascending };
    refresh();
}

void MediaBayPage::selectRow (int row)
{
    table.selectRow (row);
    selectedRowsChanged (row);
}

void MediaBayPage::setRating (int row, int stars)
{
    if (const auto* item = getResult (row))
    {
        database.setRating (item->file, stars);
        if (query.minimumRating > 0)
            refresh();
        else
            table.repaintRow (row);
    }
}

bool MediaBayPage::play()
{
    const auto* item = getResult (getSelectedRow());
    if (item == nullptr)
        return false;
    previewing = item->file;
    const auto ok = studio.previewMedia (item->file, cycle);
    refreshKeys();
    repaint (previewArea);
    return ok;
}

void MediaBayPage::stop()
{
    studio.stopMediaPreview();
    refreshKeys();
    repaint (previewArea);
}

void MediaBayPage::setAutoPlay (bool on)
{
    autoPlay = on;
    saveSettings();
    refreshKeys();
}

void MediaBayPage::setCycle (bool on)
{
    cycle = on;
    studio.setMediaPreviewLoop (on);
    saveSettings();
    refreshKeys();
}

void MediaBayPage::use (int row)
{
    if (const auto* item = getResult (row))
    {
        const auto file = item->file;
        stop();
        if (onUse)
            onUse (file);
    }
}

bool MediaBayPage::addLocation (const juce::File& folder)
{
    if (! database.addLocation (folder))
        return false;
    locationIndex = database.getLocations().size() - 1;
    refreshLocations();
    database.rescan();
    refresh();
    return true;
}

void MediaBayPage::removeSelectedLocation()
{
    if (locationIndex < 0)
        return;
    database.removeLocation (juce::File (database.getLocations()[locationIndex]));
    locationIndex = -1;
    refreshLocations();
    refresh();
}

juce::String MediaBayPage::dragDescriptionFor (const juce::StringArray& paths)
{
    return "mediabay:" + paths.joinIntoString ("\n");
}

juce::StringArray MediaBayPage::filesFromDragDescription (const juce::String& description)
{
    if (! description.startsWith ("mediabay:"))
        return {};
    juce::StringArray paths;
    paths.addLines (description.fromFirstOccurrenceOf ("mediabay:", false, false));
    paths.removeEmptyStrings();
    return paths;
}

void MediaBayPage::timerCallback()
{
    if (database.takeScanResults())
    {
        refreshLocations();
        refresh();
    }
    const auto playing = studio.isMediaPreviewing();
    if ((playKey.getButtonText() == "STOP") != playing || database.isScanning())
        refreshKeys();
    if (playing)
        repaint (waveArea.expanded (2));
}

//==============================================================================
int MediaBayPage::getNumRows()
{
    return (int) results.size();
}

void MediaBayPage::paintRowBackground (juce::Graphics& g, int row, int width, int height, bool selected)
{
    if (selected)
        g.fillAll (ParagonColours::accent.withAlpha (0.22f));
    else if (row % 2 == 1)
        g.fillAll (juce::Colour (0xff120f13));
    if (const auto* item = getResult (row); item != nullptr && item->file == previewing && studio.isMediaPreviewing())
    {
        g.setColour (ParagonColours::accent);
        g.fillRect (0, 0, 3, height);
    }
    juce::ignoreUnused (width);
}

void MediaBayPage::paintCell (juce::Graphics& g, int row, int column, int width, int height, bool selected)
{
    const auto* item = getResult (row);
    if (item == nullptr)
        return;
    auto area = juce::Rectangle<int> (0, 0, width, height).reduced (6, 0);
    g.setColour (selected ? ParagonColours::text : ParagonColours::text.withAlpha (0.9f));
    g.setFont (interFont (12.5f));
    switch (column)
    {
        case ratingColumn: drawStars (g, area.toFloat(), item->rating); return;
        case nameColumn:
        {
            g.setColour (typeColour (item->type));
            g.fillRoundedRectangle (area.removeFromLeft (4).withSizeKeepingCentre (4, 14).toFloat(), 1.5f);
            area.removeFromLeft (6);
            g.setColour (ParagonColours::text);
            g.drawText (item->name, area, juce::Justification::centredLeft, true);
            return;
        }
        case typeColumn: g.drawText (pr7s::mediaTypeName (item->type), area, juce::Justification::centredLeft, true); return;
        case lengthColumn:
            g.drawText (item->type == pr7s::MediaType::midi && item->beats > 0.0 ? juce::String (item->beats / 4.0, 1) + " bars" : lengthText (item->seconds),
                        area, juce::Justification::centredLeft, true);
            return;
        case tempoColumn: g.drawText (item->bpm > 0.0 ? juce::String (item->bpm, item->bpm == std::round (item->bpm) ? 0 : 1) : juce::String(), area, juce::Justification::centredLeft, true); return;
        case keyColumn: g.drawText (item->key, area, juce::Justification::centredLeft, true); return;
        case formatColumn:
        {
            juce::String text;
            if (item->type == pr7s::MediaType::audio && item->sampleRate > 0.0)
                text << juce::String (item->sampleRate / 1000.0, item->sampleRate == 44100.0 ? 1 : 0) << " kHz " << (item->channels == 1 ? "mono" : item->channels == 2 ? "stereo" : juce::String (item->channels) + " ch");
            else if (item->type == pr7s::MediaType::midi)
                text << juce::String (item->notes) << " notes";
            g.drawText (text, area, juce::Justification::centredLeft, true);
            return;
        }
        case folderColumn:
            g.setColour (ParagonColours::textDim);
            g.drawText (item->file.getParentDirectory().getFullPathName(), area, juce::Justification::centredLeft, true);
            return;
        case sizeColumn: g.drawText (sizeText (item->size), area, juce::Justification::centredLeft, true); return;
        case dateColumn: g.drawText (item->modified.formatted ("%Y-%m-%d %H:%M"), area, juce::Justification::centredLeft, true); return;
        default: return;
    }
}

void MediaBayPage::cellClicked (int row, int column, const juce::MouseEvent& event)
{
    if (column != ratingColumn)
        return;
    const auto width = (float) table.getHeader().getColumnWidth (ratingColumn) - 12.0f;
    const auto star = juce::jlimit (1, 5, (int) ((event.position.x - 6.0f) / (width / 5.0f)) + 1);
    const auto* item = getResult (row);
    // Clicking the stars it already has takes them all away.
    setRating (row, item != nullptr && item->rating == star ? 0 : star);
}

void MediaBayPage::cellDoubleClicked (int row, int column, const juce::MouseEvent&)
{
    if (column != ratingColumn)
        use (row);
}

void MediaBayPage::selectedRowsChanged (int lastRowSelected)
{
    refreshKeys();
    repaint (previewArea);
    if (autoPlay && lastRowSelected >= 0 && table.getNumSelectedRows() == 1)
        play();
}

void MediaBayPage::sortOrderChanged (int columnId, bool forwards)
{
    if (const auto column = mediaColumnOf (columnId))
        sortBy (*column, forwards);
}

juce::var MediaBayPage::getDragSourceDescription (const juce::SparseSet<int>& rows)
{
    juce::StringArray paths;
    for (int i = 0; i < rows.size(); ++i)
        if (const auto* item = getResult (rows[i]))
            paths.add (item->file.getFullPathName());
    return paths.isEmpty() ? juce::var() : juce::var (dragDescriptionFor (paths));
}

juce::String MediaBayPage::getCellTooltip (int row, int column)
{
    if (column == ratingColumn)
        return "Click a star to rate (click the same star again for none)";
    if (const auto* item = getResult (row))
        return item->file.getFullPathName() + "\nDouble-click: into the song. Drag onto a track.";
    return {};
}

//==============================================================================
void MediaBayPage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0a0d));
    g.setColour (ParagonColours::accent);
    g.fillRect (0, 0, getWidth(), 3);
    auto title = getLocalBounds().removeFromTop (titleHeight).reduced (gap, 6);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (22.0f));
    g.drawText ("MEDIABAY", title.removeFromLeft (150), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::titleFont (14.0f));
    g.drawText (getStatusText(), title.withTrimmedRight (110), juce::Justification::centredLeft, true);

    // LOCATIONS' heading.
    const auto left = juce::Rectangle<int> (gap, titleHeight + toolbarHeight, locationsWidth, 28);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (14.0f));
    g.drawText ("LOCATIONS", left, juce::Justification::centredLeft, false);

    paintPreviewer (g, previewArea);
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::titleFont (13.0f));
    g.drawText ("LEVEL", levelLabel, juce::Justification::centredRight, false);
}

void MediaBayPage::paintPreviewer (juce::Graphics& g, juce::Rectangle<int> area)
{
    if (area.isEmpty())
        return;
    g.setColour (juce::Colour (0xff141015));
    g.fillRoundedRectangle (area.toFloat(), 5.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 5.0f, 1.0f);
    auto inner = area.reduced (12, 8);
    auto top = inner.removeFromTop (24);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (15.0f));
    const auto* item = getResult (getSelectedRow());
    g.drawText (item != nullptr ? "PREVIEWER  -  " + item->name.toUpperCase() : juce::String ("PREVIEWER  -  SELECT A FILE"), top, juce::Justification::centredLeft, true);

    // The sound's shape (audio being heard) or a plain bar, the playhead over it.
    const auto wave = waveArea.toFloat();
    g.setColour (juce::Colour (0xff0b090c));
    g.fillRoundedRectangle (wave, 3.0f);
    const auto sound = studio.getMediaPreviewSound();
    const auto heardIsAudio = item != nullptr && item->file == previewing && item->type == pr7s::MediaType::audio && sound != nullptr;
    if (heardIsAudio && ! sound->peaks.empty())
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
    else if (item != nullptr)
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (interFont (12.0f));
        const auto text = item->type == pr7s::MediaType::midi ? "MIDI: plays through the selected track's instrument"
                        : item->type == pr7s::MediaType::audio ? "Audio: PLAY to hear it" : "No preview for this type: USE IT or drag it onto the song";
        g.drawText (text, waveArea.reduced (10, 0), juce::Justification::centredLeft, true);
    }
    if (item != nullptr && item->file == previewing && studio.isMediaPreviewing())
    {
        const auto x = wave.getX() + (float) studio.getMediaPreviewProgress() * wave.getWidth();
        g.setColour (juce::Colour (0xfff4eeea));
        g.fillRect (x, wave.getY(), 1.5f, wave.getHeight());
    }
}

void MediaBayPage::resized()
{
    auto area = getLocalBounds();
    auto title = area.removeFromTop (titleHeight).reduced (gap, 8);
    rescanKey.setBounds (title.removeFromRight (96));

    auto bar = area.removeFromTop (toolbarHeight).reduced (gap, 4);
    search.setBounds (bar.removeFromLeft (juce::jmin (380, bar.getWidth() / 3)));
    bar.removeFromLeft (10);
    for (auto* key : { &audioKey, &midiKey, &presetsKey, &songsKey })
    {
        key->setBounds (bar.removeFromLeft (key == &presetsKey ? 84 : 70));
        bar.removeFromLeft (4);
    }
    bar.removeFromLeft (8);
    ratingBox.setBounds (bar.removeFromLeft (150));
    bar.removeFromLeft (8);
    subfoldersKey.setBounds (bar.removeFromLeft (100));
    resetKey.setBounds (bar.removeFromRight (70));
    bar.removeFromRight (4);
    shuffleKey.setBounds (bar.removeFromRight (84));

    area.removeFromBottom (gap);
    previewArea = area.removeFromBottom (previewerHeight).reduced (gap, 0);
    area.removeFromBottom (gap);
    {
        auto inner = previewArea.reduced (12, 8);
        inner.removeFromTop (28);
        auto keys = inner.removeFromBottom (28);
        playKey.setBounds (keys.removeFromLeft (80));
        keys.removeFromLeft (6);
        cycleKey.setBounds (keys.removeFromLeft (76));
        keys.removeFromLeft (6);
        autoPlayKey.setBounds (keys.removeFromLeft (96));
        keys.removeFromLeft (14);
        useKey.setBounds (keys.removeFromRight (90));
        keys.removeFromRight (14);
        level.setBounds (keys.removeFromRight (juce::jmin (220, keys.getWidth())).reduced (0, 6));
        keys.removeFromRight (8);
        levelLabel = keys.removeFromRight (50);
        inner.removeFromBottom (6);
        waveArea = inner;
    }

    auto left = area.removeFromLeft (locationsWidth + gap).withTrimmedLeft (gap);
    area.removeFromLeft (gap);
    left.removeFromTop (30);
    auto leftKeys = left.removeFromBottom (28);
    removeKey.setBounds (leftKeys.removeFromRight (80));
    leftKeys.removeFromRight (6);
    addKey.setBounds (leftKeys);
    left.removeFromBottom (6);
    locations->setBounds (left);
    table.setBounds (area.withTrimmedRight (gap).withTrimmedTop (2));
}

//==============================================================================
MediaBayWindow::MediaBayWindow (pr7s::Studio& studio)
    : juce::DocumentWindow ("Paragon PR7-S: MediaBay", juce::Colour (0xff0c0a0d), juce::DocumentWindow::closeButton)
{
    page = new MediaBayPage (studio);
    setContentOwned (page, true);
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (980, 560, 3840, 2160);
}

void MediaBayWindow::closeButtonPressed()
{
    page->stop();
    if (onClose)
        onClose();
}
