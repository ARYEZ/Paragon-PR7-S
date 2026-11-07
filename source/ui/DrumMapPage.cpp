#include "DrumMapPage.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    juce::FontOptions font (float height, int style = juce::Font::bold) { return ParagonLookAndFeel::textFont (height, style); }
    const juce::Colour pageColour { 0xff0d0a0e };

    juce::PopupMenu notesMenu (int current, std::function<void (int)> choose)
    {
        juce::PopupMenu menu;
        for (int octave = 0; octave < 11; ++octave)
        {
            juce::PopupMenu sub;
            for (int n = octave * 12; n < juce::jmin (128, octave * 12 + 12); ++n)
                sub.addItem (pr7s::noteName (n) + " (" + juce::String (n) + ")", true, n == current, [choose, n] { choose (n); });
            menu.addSubMenu (pr7s::noteName (octave * 12) + " - " + pr7s::noteName (juce::jmin (127, octave * 12 + 11)), sub, true, nullptr, current / 12 == octave);
        }
        return menu;
    }
}

struct DrumMapPage::MapsModel final : juce::ListBoxModel
{
    explicit MapsModel (DrumMapPage& p) : page (p) {}
    int getNumRows() override { return (int) page.studio.getProject().drumMaps.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool) override
    {
        const auto& list = page.studio.getProject().drumMaps;
        if (! juce::isPositiveAndBelow (row, (int) list.size()))
            return;
        const auto isSelected = list[(size_t) row].name == page.selectedMap;
        if (isSelected)
        {
            g.setColour (ParagonColours::accent.withAlpha (0.3f));
            g.fillRect (0, 0, width, height);
        }
        g.setColour (isSelected ? ParagonColours::text : ParagonColours::textDim);
        g.setFont (font (13.0f));
        g.drawText (list[(size_t) row].name.toUpperCase(), 8, 0, width - 12, height, juce::Justification::centredLeft, true);
    }
    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        const auto& list = page.studio.getProject().drumMaps;
        if (juce::isPositiveAndBelow (row, (int) list.size()))
            page.selectMap (list[(size_t) row].name);
    }
    DrumMapPage& page;
};

DrumMapPage::DrumMapPage (pr7s::Studio& s, std::function<int()> track)
    : studio (s), trackForMap (std::move (track))
{
    mapsModel = std::make_unique<MapsModel> (*this);
    maps.setModel (mapsModel.get());
    maps.setRowHeight (26);
    maps.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff120e13));
    addAndMakeVisible (maps);

    auto& header = table.getHeader();
    header.addColumn ("PITCH", pitchColumn, 70, 50, 120, juce::TableHeaderComponent::notSortable);
    header.addColumn ("DRUM SOUND", nameColumn, 240, 120, 600, juce::TableHeaderComponent::notSortable);
    header.addColumn ("I-NOTE", inColumn, 80, 60, 140, juce::TableHeaderComponent::notSortable);
    header.addColumn ("O-NOTE", outColumn, 80, 60, 140, juce::TableHeaderComponent::notSortable);
    header.addColumn ("CHANNEL", channelColumn, 80, 60, 140, juce::TableHeaderComponent::notSortable);
    header.addColumn ("MUTE", muteColumn, 60, 50, 100, juce::TableHeaderComponent::notSortable);
    header.setColour (juce::TableHeaderComponent::backgroundColourId, juce::Colour (0xff1b171c));
    header.setColour (juce::TableHeaderComponent::textColourId, ParagonColours::text);
    header.setColour (juce::TableHeaderComponent::outlineColourId, ParagonColours::panelEdge);
    table.setModel (this);
    table.setRowHeight (24);
    table.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff120e13));
    addAndMakeVisible (table);

    nameField.setTextToShowWhenEmpty ("The map's name", ParagonColours::textDim);
    nameField.onReturnKey = [this] { renameSelected (nameField.getText()); };
    nameField.onFocusLost = [this] { renameSelected (nameField.getText()); };
    addAndMakeVisible (nameField);

    for (auto* key : { &newKey, &copyKey, &removeKey, &loadKey, &saveKey, &upKey, &downKey, &assignKey })
    {
        key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        addAndMakeVisible (*key);
    }
    newKey.onClick = [this] { newMap(); };
    copyKey.onClick = [this] { newCopy(); };
    removeKey.onClick = [this] { removeSelected(); };
    upKey.onClick = [this] { if (selectedSound >= 0) moveSound (selectedSound, true); };
    downKey.onClick = [this] { if (selectedSound >= 0) moveSound (selectedSound, false); };
    assignKey.onClick = [this] { assignToTrack(); };
    assignKey.setTooltip ("Give this map to the track the MIDI editor shows");
    loadKey.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a drum map", pr7s::Studio::drumMapFolder(), "*.drm;*.xml");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe = juce::Component::SafePointer<DrumMapPage> (this)] (const juce::FileChooser& c)
                              {
                                  if (safe == nullptr || c.getResult() == juce::File())
                                      return;
                                  juce::String error;
                                  const auto name = safe->studio.loadDrumMap (c.getResult(), error);
                                  if (name.isEmpty())
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load the drum map", error);
                                  else
                                      safe->selectMap (name);
                              });
    };
    saveKey.onClick = [this]
    {
        if (map() == nullptr)
            return;
        const auto folder = pr7s::Studio::drumMapFolder();
        folder.createDirectory();
        chooser = std::make_unique<juce::FileChooser> ("Save the drum map", folder.getChildFile (juce::File::createLegalFileName (selectedMap) + ".drm"), "*.drm");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe = juce::Component::SafePointer<DrumMapPage> (this)] (const juce::FileChooser& c)
                              {
                                  if (safe != nullptr && c.getResult() != juce::File())
                                      if (const auto* m = safe->map())
                                          m->writeDrm (c.getResult().withFileExtension (".drm"));
                              });
    };

    // At first: the editor's track's map, else the first.
    if (trackForMap)
        if (const auto* m = studio.getDrumMapOf (trackForMap()))
            selectedMap = m->name;
    if (selectedMap.isEmpty() && ! studio.getProject().drumMaps.empty())
        selectedMap = studio.getProject().drumMaps.front().name;
    studio.addChangeListener (this);
    refresh();
    setSize (1000, 640);
}

DrumMapPage::~DrumMapPage()
{
    studio.removeChangeListener (this);
    table.setModel (nullptr);
    maps.setModel (nullptr);
}

const pr7s::DrumMap* DrumMapPage::map() const
{
    return studio.getProject().findDrumMap (selectedMap);
}

void DrumMapPage::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

void DrumMapPage::refresh()
{
    if (map() == nullptr)
        selectedMap = studio.getProject().drumMaps.empty() ? juce::String() : studio.getProject().drumMaps.front().name;
    if (! nameField.hasKeyboardFocus (true))
        nameField.setText (selectedMap, false);
    const auto has = map() != nullptr;
    for (auto* key : { &copyKey, &removeKey, &saveKey, &upKey, &downKey, &assignKey })
        key->setEnabled (has);
    nameField.setEnabled (has);
    maps.updateContent();
    maps.repaint();
    table.updateContent();
    table.repaint();
}

void DrumMapPage::edit (std::function<void (pr7s::DrumMap&)> change)
{
    if (const auto* m = map())
    {
        auto edited = *m;
        change (edited);
        studio.setDrumMap (selectedMap, edited);
    }
}

void DrumMapPage::selectMap (const juce::String& name)
{
    if (studio.getProject().findDrumMap (name) == nullptr)
        return;
    selectedMap = name;
    selectedSound = -1;
    nameField.setText (name, false);
    refresh();
}

juce::String DrumMapPage::newMap()
{
    const auto name = studio.addDrumMap (pr7s::DrumMap::empty ("Empty Map"));
    selectMap (name);
    return name;
}

juce::String DrumMapPage::newCopy()
{
    const auto* m = map();
    if (m == nullptr)
        return {};
    const auto name = studio.addDrumMap (*m);
    selectMap (name);
    return name;
}

void DrumMapPage::removeSelected()
{
    if (map() == nullptr)
        return;
    studio.removeDrumMap (selectedMap);
    selectedMap = {};
    refresh();
}

bool DrumMapPage::renameSelected (const juce::String& name)
{
    const auto* m = map();
    const auto newName = name.trim();
    if (m == nullptr || newName.isEmpty() || newName == selectedMap)
        return false;
    auto edited = *m;
    edited.name = newName;
    if (! studio.setDrumMap (selectedMap, edited))
    {
        nameField.setText (selectedMap, false);
        return false;
    }
    selectedMap = newName;
    refresh();
    return true;
}

void DrumMapPage::setSoundName (int pitch, const juce::String& name)
{
    if (juce::isPositiveAndBelow (pitch, 128))
        edit ([pitch, name] (pr7s::DrumMap& m) { m.sounds[(size_t) pitch].name = name.trim(); });
}

void DrumMapPage::setInNote (int pitch, int note)
{
    if (juce::isPositiveAndBelow (pitch, 128))
        edit ([pitch, note] (pr7s::DrumMap& m) { m.sounds[(size_t) pitch].inNote = juce::jlimit (0, 127, note); });
}

void DrumMapPage::setOutNote (int pitch, int note)
{
    if (juce::isPositiveAndBelow (pitch, 128))
        edit ([pitch, note] (pr7s::DrumMap& m) { m.sounds[(size_t) pitch].outNote = juce::jlimit (0, 127, note); });
}

void DrumMapPage::setChannel (int pitch, int channel)
{
    if (juce::isPositiveAndBelow (pitch, 128))
        edit ([pitch, channel] (pr7s::DrumMap& m) { m.sounds[(size_t) pitch].channel = juce::jlimit (-1, 15, channel); });
}

void DrumMapPage::toggleMute (int pitch)
{
    if (juce::isPositiveAndBelow (pitch, 128))
        edit ([pitch] (pr7s::DrumMap& m) { m.sounds[(size_t) pitch].mute = ! m.sounds[(size_t) pitch].mute; });
}

void DrumMapPage::moveSound (int pitch, bool up)
{
    edit ([pitch, up] (pr7s::DrumMap& m)
    {
        const auto it = std::find (m.order.begin(), m.order.end(), pitch);
        if (it == m.order.end())
            return;
        const auto i = std::distance (m.order.begin(), it);
        const auto j = up ? i - 1 : i + 1;
        if (j >= 0 && j < (long) m.order.size())
            std::swap (m.order[(size_t) i], m.order[(size_t) j]);
    });
    if (const auto* m = map())
        for (int row = 0; row < (int) m->order.size(); ++row)
            if (m->order[(size_t) row] == pitch)
                table.selectRow (row);
}

void DrumMapPage::selectSound (int pitch)
{
    selectedSound = pitch;
    if (const auto* m = map())
        for (int row = 0; row < (int) m->order.size(); ++row)
            if (m->order[(size_t) row] == pitch)
                table.selectRow (row);
}

void DrumMapPage::assignToTrack()
{
    if (trackForMap && map() != nullptr)
        studio.setTrackDrumMap (trackForMap(), selectedMap);
}

//==============================================================================
int DrumMapPage::getNumRows()
{
    const auto* m = map();
    return m != nullptr ? (int) m->order.size() : 0;
}

void DrumMapPage::paintRowBackground (juce::Graphics& g, int row, int width, int height, bool isSelected)
{
    g.setColour (isSelected ? ParagonColours::accent.withAlpha (0.28f) : row % 2 == 0 ? juce::Colour (0xff181318) : juce::Colour (0xff120e13));
    g.fillRect (0, 0, width, height);
}

void DrumMapPage::paintCell (juce::Graphics& g, int row, int column, int width, int height, bool)
{
    const auto* m = map();
    if (m == nullptr || ! juce::isPositiveAndBelow (row, (int) m->order.size()))
        return;
    const auto pitch = m->order[(size_t) row];
    const auto& s = m->sounds[(size_t) pitch];
    juce::String text;
    switch (column)
    {
        case pitchColumn:   text = pr7s::noteName (pitch) + "  (" + juce::String (pitch) + ")"; break;
        case nameColumn:    text = s.name.isNotEmpty() ? s.name : "-"; break;
        case inColumn:      text = pr7s::noteName (s.inNote); break;
        case outColumn:     text = pr7s::noteName (s.outNote); break;
        case channelColumn: text = s.channel < 0 ? juce::String ("ANY") : juce::String (s.channel + 1); break;
        case muteColumn:    text = s.mute ? "MUTED" : ""; break;
        default: break;
    }
    const auto changedNote = (column == inColumn && s.inNote != pitch) || (column == outColumn && s.outNote != pitch);
    g.setColour (column == muteColumn ? ParagonColours::accent : column == nameColumn ? ParagonColours::text
                 : changedNote ? ParagonColours::accent : ParagonColours::textDim);
    g.setFont (column == nameColumn ? font (13.0f) : font (12.0f, juce::Font::plain));
    g.drawText (text, 6, 0, width - 10, height, juce::Justification::centredLeft, true);
}

void DrumMapPage::cellClicked (int row, int column, const juce::MouseEvent&)
{
    const auto* m = map();
    if (m == nullptr || ! juce::isPositiveAndBelow (row, (int) m->order.size()))
        return;
    const auto pitch = m->order[(size_t) row];
    selectedSound = pitch;
    const auto& s = m->sounds[(size_t) pitch];
    juce::Component::SafePointer<DrumMapPage> safe (this);
    if (column == muteColumn)
        toggleMute (pitch);
    else if (column == inColumn)
        notesMenu (s.inNote, [safe, pitch] (int n) { if (safe != nullptr) safe->setInNote (pitch, n); }).showMenuAsync ({});
    else if (column == outColumn)
        notesMenu (s.outNote, [safe, pitch] (int n) { if (safe != nullptr) safe->setOutNote (pitch, n); }).showMenuAsync ({});
    else if (column == channelColumn)
    {
        juce::PopupMenu menu;
        menu.addItem ("ANY (the track's)", true, s.channel < 0, [safe, pitch] { if (safe != nullptr) safe->setChannel (pitch, -1); });
        for (int c = 0; c < 16; ++c)
            menu.addItem (juce::String (c + 1), true, s.channel == c, [safe, pitch, c] { if (safe != nullptr) safe->setChannel (pitch, c); });
        menu.showMenuAsync ({});
    }
}

void DrumMapPage::cellDoubleClicked (int row, int column, const juce::MouseEvent&)
{
    const auto* m = map();
    if (m == nullptr || column != nameColumn || ! juce::isPositiveAndBelow (row, (int) m->order.size()))
        return;
    const auto pitch = m->order[(size_t) row];
    auto* box = new FamilyAlertWindow ("RENAME " + pr7s::noteName (pitch), "The drum's name:", juce::MessageBoxIconType::NoIcon);
    box->addTextEditor ("name", m->sounds[(size_t) pitch].name);
    box->addButton ("RENAME", 1, juce::KeyPress (juce::KeyPress::returnKey));
    box->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<DrumMapPage> safe (this);
    box->enterModalState (true, juce::ModalCallbackFunction::create ([safe, box, pitch] (int result)
    {
        if (result == 1 && safe != nullptr)
            safe->setSoundName (pitch, box->getTextEditorContents ("name"));
    }), true);
}

//==============================================================================
void DrumMapPage::paint (juce::Graphics& g)
{
    g.fillAll (pageColour);
    g.setColour (ParagonColours::accent);
    g.setFont (ParagonLookAndFeel::titleFont (20.0f));
    g.drawText ("DRUM MAP SETUP", 16, 10, 400, 28, juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (font (12.0f, juce::Font::plain));
    g.drawText ("Changes act at once on every track that uses the map. Click I-NOTE, O-NOTE or CHANNEL to change them; double-click a name to rename it.",
                16, 38, getWidth() - 32, 18, juce::Justification::centredLeft, true);
}

void DrumMapPage::resized()
{
    auto r = getLocalBounds().reduced (16).withTrimmedTop (48);
    auto left = r.removeFromLeft (260);
    r.removeFromLeft (14);
    nameField.setBounds (left.removeFromTop (30));
    left.removeFromTop (6);
    auto keys = left.removeFromBottom (3 * 34);
    maps.setBounds (left.withTrimmedBottom (6));
    const auto row = [&keys] { return keys.removeFromTop (34).reduced (0, 3); };
    auto r1 = row();
    newKey.setBounds (r1.removeFromLeft (r1.getWidth() / 2 - 3));
    r1.removeFromLeft (6);
    copyKey.setBounds (r1);
    auto r2 = row();
    loadKey.setBounds (r2.removeFromLeft (r2.getWidth() / 2 - 3));
    r2.removeFromLeft (6);
    saveKey.setBounds (r2);
    removeKey.setBounds (row());
    auto bottom = r.removeFromBottom (34).reduced (0, 3);
    upKey.setBounds (bottom.removeFromLeft (90));
    bottom.removeFromLeft (6);
    downKey.setBounds (bottom.removeFromLeft (90));
    assignKey.setBounds (bottom.removeFromRight (180));
    r.removeFromBottom (6);
    table.setBounds (r);
}

//==============================================================================
DrumMapWindow::DrumMapWindow (pr7s::Studio& s, std::function<int()> trackForMap)
    : DocumentWindow ("Paragon PR7-S: drum map setup", pageColour, DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    page = new DrumMapPage (s, std::move (trackForMap));
    setContentOwned (page, true);
    setResizable (true, false);
    setResizeLimits (800, 480, 4000, 3000);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
}

void DrumMapWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}
