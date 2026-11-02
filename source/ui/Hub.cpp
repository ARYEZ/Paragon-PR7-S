#include "Hub.h"

#include "ui/Finish.h"
#include "ui/Logo.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/Pearl.h"

namespace
{
    const auto onyx = juce::Colour (0xff0c0a0d);
    const auto panelColour = juce::Colour (0xff120e13);
    juce::Font font (float size) { return ParagonLookAndFeel::textFont (size); }

    void styleKey (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1c1c20));
        b.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        b.setColour (juce::TextButton::textColourOnId, ParagonColours::accent);
        b.setWantsKeyboardFocus (false);
    }

    void styleToggle (juce::ToggleButton& t)
    {
        t.setColour (juce::ToggleButton::textColourId, ParagonColours::text);
        t.setColour (juce::ToggleButton::tickColourId, ParagonColours::accent);
        t.setColour (juce::ToggleButton::tickDisabledColourId, ParagonColours::textDim);
        t.setWantsKeyboardFocus (false);
    }

    // A small page with a folded corner (a song file), as the list's icon.
    void drawSongIcon (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
    {
        const auto box = area.withSizeKeepingCentre (area.getHeight() * 0.72f, area.getHeight());
        const auto fold = box.getWidth() * 0.35f;
        juce::Path page;
        page.startNewSubPath (box.getX(), box.getY());
        page.lineTo (box.getRight() - fold, box.getY());
        page.lineTo (box.getRight(), box.getY() + fold);
        page.lineTo (box.getRight(), box.getBottom());
        page.lineTo (box.getX(), box.getBottom());
        page.closeSubPath();
        g.setColour (colour);
        g.fillPath (page);
        g.setColour (onyx);
        g.fillRect (juce::Rectangle<float> (box.getRight() - fold, box.getY(), fold, fold).reduced (0.5f));
        g.setColour (colour.darker (0.3f));
        juce::Path corner;
        corner.addTriangle (box.getRight() - fold, box.getY(), box.getRight() - fold, box.getY() + fold, box.getRight(), box.getY() + fold);
        g.fillPath (corner);
    }
}

//==============================================================================
class Hub::SongList final : public juce::Component,
                            private juce::ListBoxModel
{
public:
    explicit SongList (Hub& h) : hub (h)
    {
        list.setModel (this);
        list.setRowHeight (40);
        list.setColour (juce::ListBox::backgroundColourId, panelColour);
        list.setWantsKeyboardFocus (true);
        addAndMakeVisible (list);
    }

    int getNumRows() override { return (int) hub.items.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, getNumRows()))
            return;
        const auto& item = hub.items[(size_t) row];
        auto area = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
        if (item.header)
        {
            g.setColour (juce::Colour (0xff1b1419));
            g.fillRect (area.reduced (0.0f, 2.0f));
            const auto isFolded = hub.folded.contains (item.section);
            juce::Path arrow;
            const auto c = juce::Point<float> (16.0f, area.getCentreY());
            if (isFolded)
                arrow.addTriangle (c.x - 3.0f, c.y - 5.0f, c.x - 3.0f, c.y + 5.0f, c.x + 4.0f, c.y);
            else
                arrow.addTriangle (c.x - 5.0f, c.y - 3.0f, c.x + 5.0f, c.y - 3.0f, c.x, c.y + 4.0f);
            g.setColour (ParagonColours::accent);
            g.fillPath (arrow);
            g.setFont (ParagonLookAndFeel::titleFont (15.0f));
            g.drawText (item.section, area.withTrimmedLeft (32.0f), juce::Justification::centredLeft, true);
            return;
        }
        if (selected)
        {
            g.setColour (ParagonColours::accent.withAlpha (0.22f));
            g.fillRect (area);
            g.setColour (ParagonColours::accent.withAlpha (0.7f));
            g.fillRect (area.withWidth (3.0f));
        }
        drawSongIcon (g, area.withX (22.0f).withWidth (20.0f).reduced (0.0f, 10.0f), ParagonColours::text);
        auto text = area.withTrimmedLeft (54.0f).withTrimmedRight (16.0f);
        const auto date = item.file.getLastModificationTime().formatted ("%m/%d/%Y  %I:%M %p");
        g.setColour (ParagonColours::textDim);
        g.setFont (font (13.0f));
        g.drawText (date, text.removeFromRight (170.0f), juce::Justification::centredRight, false);
        const auto paths = hub.studio.getLibrary().showsPaths();
        g.setColour (ParagonColours::text);
        g.setFont (ParagonLookAndFeel::titleFont (17.0f));
        const auto name = item.file.getFileNameWithoutExtension();
        if (paths)
        {
            g.drawText (name, text.removeFromTop (text.getHeight() * 0.58f), juce::Justification::bottomLeft, true);
            g.setColour (ParagonColours::textDim);
            g.setFont (font (11.0f));
            g.drawText (item.file.getParentDirectory().getFullPathName(), text, juce::Justification::topLeft, true);
        }
        else
            g.drawText (name, text, juce::Justification::centredLeft, true);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (! juce::isPositiveAndBelow (row, getNumRows()))
            return;
        const auto& item = hub.items[(size_t) row];
        if (item.header)
        {
            // Its title folds or unfolds the section.
            hub.folded.contains (item.section) ? hub.folded.removeString (item.section) : hub.folded.add (item.section);
            hub.refresh();
            return;
        }
        if (e.mods.isPopupMenu())
            hub.showMenu (row);
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { hub.openItem (row); }
    void returnKeyPressed (int row) override { hub.openItem (row); }
    void selectedRowsChanged (int) override { hub.open.setEnabled (getSelected() >= 0); }

    int getSelected() const
    {
        const auto row = list.getSelectedRow();
        return juce::isPositiveAndBelow (row, (int) hub.items.size()) && ! hub.items[(size_t) row].header ? row : -1;
    }

    void resized() override { list.setBounds (getLocalBounds()); }

    Hub& hub;
    juce::ListBox list;
};

//==============================================================================
Hub::Hub (pr7s::Studio& s) : studio (s)
{
    list = std::make_unique<SongList> (*this);
    addAndMakeVisible (*list);
    for (auto* b : { &projectsTab, &settingsTab, &createEmpty, &chooseFile, &open, &driverPanel, &audioSettings, &chooseLocation, &addLocation, &removeLocation })
    {
        styleKey (*b);
        addAndMakeVisible (*b);
    }
    for (auto* t : { &useDefault, &showPaths, &showLocation, &atStart })
    {
        styleToggle (*t);
        addAndMakeVisible (*t);
    }
    // DRAG HIGHLIGHT (0.18.2, the owner asked): how strongly a file dragged in lights where it lands.
    dropLabel.setText ("DRAG HIGHLIGHT: how strongly a dragged-in file lights where it will land", juce::dontSendNotification);
    dropLabel.setColour (juce::Label::textColourId, ParagonColours::text);
    dropLabel.setFont (ParagonLookAndFeel::textFont (13.0f));
    addAndMakeVisible (dropLabel);
    dropStrength.setSliderStyle (juce::Slider::LinearBar);
    dropStrength.setRange (10.0, 100.0, 1.0);
    dropStrength.setTextValueSuffix ("%");
    dropStrength.setColour (juce::Slider::trackColourId, ParagonColours::accent);
    dropStrength.setColour (juce::Slider::textBoxTextColourId, ParagonColours::text);
    dropStrength.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff1b171c));   // (the bar's whole length shows)
    dropStrength.setColour (juce::Slider::textBoxOutlineColourId, ParagonColours::panelEdge);
    dropStrength.setValue (studio.getDropHighlight() * 100.0, juce::dontSendNotification);
    dropStrength.onValueChange = [this] { studio.setDropHighlight ((float) dropStrength.getValue() / 100.0f); };
    dropStrength.setTooltip ("10%: a faint wash; 100%: solid. The edge thickens with it");
    addAndMakeVisible (dropStrength);
    projectsTab.setClickingTogglesState (false);
    settingsTab.setClickingTogglesState (false);
    projectsTab.onClick = [this] { showSettingsPage (false); };
    settingsTab.onClick = [this] { showSettingsPage (true); };
    createEmpty.setTooltip ("Create Empty: a new song, in a folder of its own at the project location");
    createEmpty.onClick = [this]
    {
        auto& library = studio.getLibrary();
        if (! library.usesDefaultLocation())
        {
            if (onCreate)
                onCreate ({});   // (no location: a new song; saving asks where)
            return;
        }
        auto* box = new juce::AlertWindow ("Create Empty", "Its name (its folder goes in " + library.getDefaultLocation().getFullPathName() + "):",
                                           juce::MessageBoxIconType::NoIcon);
        box->addTextEditor ("name", library.getFolderName());
        box->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
        box->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        box->enterModalState (true, juce::ModalCallbackFunction::create ([box, safe = juce::Component::SafePointer<Hub> (this)] (int result)
        {
            if (safe != nullptr && result == 1 && safe->onCreate)
                safe->onCreate (box->getTextEditorContents ("name"));
        }), true);
    };
    chooseFile.onClick = [this] { if (onChooseFile) onChooseFile(); };
    open.setTooltip ("Open the song chosen (a template: a new song from it)");
    open.onClick = [this] { openItem (list->getSelected()); };
    open.setEnabled (false);
    audioSettings.setTooltip ("All the audio settings (inputs, buffer size, MIDI)");
    audioSettings.onClick = [this] { if (onSettings) onSettings(); };
    driverPanel.setTooltip ("The driver's own control panel (its buffer size, for example)");
    driverPanel.onClick = [this]
    {
        if (auto* device = studio.getDeviceManager().getCurrentAudioDevice(); device != nullptr && device->hasControlPanel())
            device->showControlPanel();
    };

    search.setTextToShowWhenEmpty ("Search", ParagonColours::textDim);
    search.onTextChange = [this] { refresh(); };
    addAndMakeVisible (search);
    folderName.setTooltip ("Project Folder Name: what a new song's folder is called at first");
    folderName.onTextChange = [this] { studio.getLibrary().setFolderName (folderName.getText()); };
    addAndMakeVisible (folderName);
    locationLabel.setColour (juce::Label::textColourId, ParagonColours::text);
    locationLabel.setFont (font (13.0f));
    addAndMakeVisible (locationLabel);
    useDefault.onClick = [this] { studio.getLibrary().setUseDefaultLocation (useDefault.getToggleState()); refreshSettings(); };
    showPaths.onClick = [this] { studio.getLibrary().setShowsPaths (showPaths.getToggleState()); list->list.setRowHeight (showPaths.getToggleState() ? 48 : 40); refresh(); };
    showLocation.onClick = [this] { studio.getLibrary().setShowsLocation (showLocation.getToggleState()); resized(); repaint(); };
    atStart.onClick = [this] { studio.getLibrary().setOpensAtStart (atStart.getToggleState()); };
    chooseLocation.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("The project location: where new songs go", studio.getLibrary().getDefaultLocation());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult().isDirectory())
            {
                studio.getLibrary().setDefaultLocation (fc.getResult());
                refreshSettings();
                refresh();
            }
        });
    };
    addLocation.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Another folder of songs to list", studio.getLibrary().getDefaultLocation());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult().isDirectory())
            {
                studio.getLibrary().addUserLocation (fc.getResult());
                refreshSettings();
                refresh();
            }
        });
    };
    removeLocation.onClick = [this]
    {
        const auto folders = studio.getLibrary().getUserLocations();
        if (juce::isPositiveAndBelow (locationsBox.getSelectedItemIndex(), folders.size()))
            studio.getLibrary().removeUserLocation (folders[locationsBox.getSelectedItemIndex()]);
        refreshSettings();
        refresh();
    };
    locationsBox.setTextWhenNothingSelected ("No other folders");
    addAndMakeVisible (locationsBox);

    driverBox.setTooltip ("Audio Driver: the audio device the studio plays through");
    driverBox.onChange = [this]
    {
        // "TYPE: device": that type, then that device.
        const auto text = driverBox.getText();
        const auto type = text.upToFirstOccurrenceOf (": ", false, false), name = text.fromFirstOccurrenceOf (": ", false, false);
        auto& devices = studio.getDeviceManager();
        if (devices.getCurrentAudioDeviceType() != type)
            devices.setCurrentAudioDeviceType (type, true);
        auto setup = devices.getAudioDeviceSetup();
        setup.outputDeviceName = name;
        if (type.containsIgnoreCase ("ASIO"))
            setup.inputDeviceName = name;
        devices.setAudioDeviceSetup (setup, true);
        refreshAudio();
    };
    outputBox.setTooltip ("Select Stereo Output: the pair of outputs the studio plays on");
    outputBox.onChange = [this]
    {
        auto& devices = studio.getDeviceManager();
        auto setup = devices.getAudioDeviceSetup();
        setup.useDefaultOutputChannels = false;
        setup.outputChannels.clear();
        setup.outputChannels.setRange ((outputBox.getSelectedId() - 1) * 2, 2, true);
        devices.setAudioDeviceSetup (setup, true);
    };
    addAndMakeVisible (driverBox);
    addAndMakeVisible (outputBox);
    studio.getDeviceManager().addChangeListener (this);

    setSize (preferredWidth, preferredHeight);
    refreshAudio();
    refreshSettings();
    refresh();
}

Hub::~Hub()
{
    studio.getDeviceManager().removeChangeListener (this);
}

void Hub::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshAudio();
}

void Hub::refreshAudio()
{
    auto& devices = studio.getDeviceManager();
    driverBox.clear (juce::dontSendNotification);
    int id = 1, chosen = 0;
    const auto current = devices.getCurrentAudioDevice();
    for (auto* type : devices.getAvailableDeviceTypes())
    {
        type->scanForDevices();
        for (const auto& name : type->getDeviceNames (false))
        {
            driverBox.addItem (type->getTypeName() + ": " + name, id);
            if (current != nullptr && current->getTypeName() == type->getTypeName() && current->getName() == name)
                chosen = id;
            ++id;
        }
    }
    if (chosen != 0)
        driverBox.setSelectedId (chosen, juce::dontSendNotification);
    else
        driverBox.setTextWhenNothingSelected (current == nullptr ? "No audio device" : current->getName());

    outputBox.clear (juce::dontSendNotification);
    if (current != nullptr)
    {
        const auto names = current->getOutputChannelNames();
        const auto active = current->getActiveOutputChannels();
        for (int pair = 0; pair * 2 + 1 < names.size(); ++pair)
            outputBox.addItem ("Out " + juce::String (pair * 2 + 1) + " + " + juce::String (pair * 2 + 2), pair + 1);
        const auto first = active.findNextSetBit (0);
        if (first >= 0)
            outputBox.setSelectedId (first / 2 + 1, juce::dontSendNotification);
        driverPanel.setEnabled (current->hasControlPanel());
    }
    else
        driverPanel.setEnabled (false);
}

void Hub::refreshSettings()
{
    auto& library = studio.getLibrary();
    useDefault.setToggleState (library.usesDefaultLocation(), juce::dontSendNotification);
    showPaths.setToggleState (library.showsPaths(), juce::dontSendNotification);
    showLocation.setToggleState (library.showsLocation(), juce::dontSendNotification);
    atStart.setToggleState (library.opensAtStart(), juce::dontSendNotification);
    locationLabel.setText (library.getDefaultLocation().getFullPathName(), juce::dontSendNotification);
    if (! folderName.hasKeyboardFocus (true))
        folderName.setText (library.getFolderName(), juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> { &locationLabel, &folderName, &chooseLocation })
        c->setEnabled (library.usesDefaultLocation());
    locationsBox.clear (juce::dontSendNotification);
    const auto folders = library.getUserLocations();
    for (int i = 0; i < folders.size(); ++i)
        locationsBox.addItem (folders[i].getFullPathName(), i + 1);
    if (! folders.isEmpty())
        locationsBox.setSelectedItemIndex (0, juce::dontSendNotification);
    removeLocation.setEnabled (! folders.isEmpty());
    resized();
    repaint();
}

void Hub::refresh()
{
    // The sections: RECENT, TEMPLATES, then each folder's songs; the search narrows them all.
    auto& library = studio.getLibrary();
    const auto query = search.getText().trim();
    const auto keep = [&] (const juce::File& f) { return query.isEmpty() || f.getFileNameWithoutExtension().containsIgnoreCase (query); };
    items.clear();
    const auto section = [&] (const juce::String& title, const juce::Array<juce::File>& files)
    {
        juce::Array<juce::File> shown;
        for (const auto& f : files)
            if (keep (f))
                shown.add (f);
        if (shown.isEmpty() && query.isNotEmpty())
            return;
        items.push_back ({ true, title, {} });
        if (folded.contains (title))
            return;
        for (const auto& f : shown)
            items.push_back ({ false, title, f });
    };
    section ("RECENT", library.getRecent());
    section ("TEMPLATES", library.getTemplates());
    section ("IN " + library.getDefaultLocation().getFileName().toUpperCase(), pr7s::ProjectLibrary::findSongs (library.getDefaultLocation()));
    for (const auto& folder : library.getUserLocations())
        section ("IN " + folder.getFileName().toUpperCase(), pr7s::ProjectLibrary::findSongs (folder));
    list->list.updateContent();
    list->list.repaint();
    open.setEnabled (list->getSelected() >= 0);
}

void Hub::setSearch (const juce::String& text)
{
    search.setText (text, false);
    refresh();
}

void Hub::openItem (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) items.size()) || items[(size_t) index].header)
        return;
    const auto& item = items[(size_t) index];
    if (item.section == "TEMPLATES")
    {
        if (onTemplate)
            onTemplate (item.file);
    }
    else if (onOpen)
        onOpen (item.file);
}

void Hub::showSettingsPage (bool on)
{
    settingsPage = on;
    resized();
    repaint();
}

void Hub::showMenu (int index)
{
    const auto item = items[(size_t) index];
    const auto isRecent = item.section == "RECENT";
    juce::PopupMenu menu;
    menu.addItem (1, item.section == "TEMPLATES" ? "New Song from This Template" : "Open");
    menu.addSeparator();
    if (isRecent)
        menu.addItem (2, "Remove from the List");
    menu.addItem (3, "Show in Explorer");
    // (Not the song that's open: the studio would go on saving it under its old name.)
    const auto isOpen = item.file == studio.getFile();
    menu.addItem (4, isOpen ? "Rename... (it's open)" : "Rename...", ! isOpen);
    menu.addItem (5, isOpen ? "Delete... (it's open)" : "Delete...", ! isOpen);
    if (isRecent)
    {
        menu.addSeparator();
        menu.addItem (6, "Clear the List");
    }
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<Hub> (this), item, index] (int chosen)
    {
        if (safe == nullptr)
            return;
        auto& library = safe->studio.getLibrary();
        switch (chosen)
        {
            case 1: safe->openItem (index); break;
            case 2: library.removeRecent (item.file); safe->refresh(); break;
            case 3: item.file.revealToUser(); break;
            case 4: safe->rename (item.file); break;
            case 5:
                juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Delete",
                    "Put " + item.file.getFileName() + " in the Recycle Bin? Its recordings folder stays where it is.", "Delete", "Cancel", nullptr,
                    juce::ModalCallbackFunction::create ([safe, item] (int result)
                    {
                        if (safe == nullptr || result != 1)
                            return;
                        item.file.moveToTrash();
                        safe->studio.getLibrary().removeRecent (item.file);
                        safe->refresh();
                    }));
                break;
            case 6: library.clearRecent(); safe->refresh(); break;
            default: break;
        }
    });
}

void Hub::rename (const juce::File& file)
{
    auto* box = new juce::AlertWindow ("Rename", "Its new name:", juce::MessageBoxIconType::NoIcon);
    box->addTextEditor ("name", file.getFileNameWithoutExtension());
    box->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
    box->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    box->enterModalState (true, juce::ModalCallbackFunction::create ([box, file, safe = juce::Component::SafePointer<Hub> (this)] (int result)
    {
        if (safe == nullptr || result != 1)
            return;
        const auto name = juce::File::createLegalFileName (box->getTextEditorContents ("name").trim());
        if (name.isEmpty())
            return;
        const auto renamed = file.getSiblingFile (name + file.getFileExtension());
        // Its recordings folder, "<song> Audio", goes with it: never onto another's
        // (a deleted song's folder stays behind), and the song goes back if it can't.
        const auto audio = file.getSiblingFile (file.getFileNameWithoutExtension() + " Audio");
        const auto renamedAudio = renamed.getSiblingFile (name + " Audio");
        if (renamed.exists() || (audio.isDirectory() && renamedAudio.exists()))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Rename",
                                                    "There's already a song or a recordings folder called " + name + " here.");
            return;
        }
        if (file.moveFileTo (renamed))
        {
            if (audio.isDirectory() && ! audio.moveFileTo (renamedAudio))
            {
                renamed.moveFileTo (file);
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Rename",
                                                        "Its recordings folder couldn't be renamed (is a file in it open?), so the song keeps its name.");
                safe->refresh();
                return;
            }
            auto& library = safe->studio.getLibrary();
            const auto wasRecent = library.getRecent().contains (file);
            library.removeRecent (file);
            if (wasRecent)
                library.addRecent (renamed);
        }
        safe->refresh();
    }), true);
}

//==============================================================================
void Hub::paint (juce::Graphics& g)
{
    g.fillAll (onyx);
    // The pearl unit across the top, the PR7-S mark and what this is.
    Pearl::paintUnit (g, header.toFloat().reduced (4.0f), satin);
    auto badge = header.toFloat().reduced (24.0f, 14.0f).withWidth (300.0f);
    Logo::drawMark (g, badge.removeFromTop (badge.getHeight() * 0.66f), juce::Justification::centredLeft,
                    { Finish::ink(), juce::Colour (0xffe0103c), Finish::stampEdge(), {}, 1.0f, false });
    g.setColour (Finish::printedRed());
    g.setFont (font (12.0f));
    g.drawText ("HUB  -  RECORDING STUDIO " + juce::String (PR7S_VERSION), badge, juce::Justification::topLeft, false);
    g.setColour (Finish::ink());
    g.setFont (font (10.0f));
    g.drawText ("AUDIO DRIVER", driverBox.getBounds().translated (0, -14).withHeight (12).toFloat(), juce::Justification::bottomLeft, false);
    g.drawText ("STEREO OUTPUT", outputBox.getBounds().translated (0, -14).withHeight (12).toFloat(), juce::Justification::bottomLeft, false);

    // The page: a panel with the question boxes' red edge.
    const auto pageArea = page.toFloat();
    g.setColour (panelColour);
    g.fillRoundedRectangle (pageArea, 6.0f);
    g.setColour (ParagonColours::glow.withAlpha (0.3f));
    g.drawRoundedRectangle (pageArea.reduced (1.5f), 5.0f, 3.0f);
    g.setColour (ParagonColours::glow);
    g.drawRoundedRectangle (pageArea, 6.0f, 1.2f);

    g.setColour (ParagonColours::textDim);
    g.setFont (font (12.0f));
    if (! settingsPage && studio.getLibrary().showsLocation())
    {
        g.drawText ("PROJECT LOCATION", locationRow.withHeight (14).toFloat(), juce::Justification::topLeft, false);
        g.drawText ("FOLDER NAME", folderName.getBounds().translated (0, -16).withHeight (14).toFloat(), juce::Justification::bottomLeft, false);
    }
    if (settingsPage)
    {
        g.setColour (ParagonColours::text);
        g.setFont (ParagonLookAndFeel::titleFont (18.0f));
        auto top = page.reduced (24, 18);
        g.drawText ("HUB SETTINGS", top.removeFromTop (26).toFloat(), juce::Justification::centredLeft, false);
        g.setColour (ParagonColours::textDim);
        g.setFont (font (12.0f));
        g.drawText ("PROJECT LOCATION (NEW SONGS GO HERE, EACH IN A FOLDER OF ITS OWN)", locationRow.withHeight (14).toFloat(), juce::Justification::topLeft, false);
        g.drawText ("FOLDER NAME", folderName.getBounds().translated (0, -16).withHeight (14).toFloat(), juce::Justification::bottomLeft, false);
        g.drawText ("USER LOCATIONS (MORE FOLDERS WHOSE SONGS THE HUB LISTS)", locationsBox.getBounds().translated (0, -18).withHeight (14).toFloat(),
                    juce::Justification::bottomLeft, false);
    }
}

void Hub::resized()
{
    satin = Finish::makeSatinGrain (juce::jmax (1, getWidth() * 2), 220, 4242);
    auto area = getLocalBounds();
    header = area.removeFromTop (96);
    {
        auto h = header.reduced (24, 0).withTrimmedTop (36).withHeight (30);
        audioSettings.setBounds (h.removeFromRight (96));
        h.removeFromRight (8);
        driverPanel.setBounds (h.removeFromRight (120));
        h.removeFromRight (14);
        outputBox.setBounds (h.removeFromRight (150));
        h.removeFromRight (10);
        driverBox.setBounds (h.removeFromRight (320));
    }
    area.reduce (18, 14);
    nav = area.removeFromLeft (200);
    {
        auto n = nav;
        projectsTab.setBounds (n.removeFromTop (34));
        n.removeFromTop (8);
        settingsTab.setBounds (n.removeFromTop (34));
        projectsTab.setToggleState (! settingsPage, juce::dontSendNotification);
        settingsTab.setToggleState (settingsPage, juce::dontSendNotification);
    }
    area.removeFromLeft (16);
    page = area;

    const auto showProjects = ! settingsPage;
    const auto& library = studio.getLibrary();
    const auto locationOnPage = showProjects && library.showsLocation();
    for (auto* c : std::initializer_list<juce::Component*> { list.get(), &search, &createEmpty, &chooseFile, &open })
        c->setVisible (showProjects);
    for (auto* c : std::initializer_list<juce::Component*> { &showPaths, &showLocation, &atStart, &addLocation, &removeLocation, &locationsBox, &dropLabel, &dropStrength })
        c->setVisible (settingsPage);
    for (auto* c : std::initializer_list<juce::Component*> { &useDefault, &locationLabel, &chooseLocation, &folderName })
        c->setVisible (settingsPage || locationOnPage);

    auto p = page.reduced (20, 16);
    if (showProjects)
    {
        auto top = p.removeFromTop (40);
        createEmpty.setBounds (top.removeFromLeft (200));
        search.setBounds (top.removeFromRight (280).withSizeKeepingCentre (280, 30));
        p.removeFromTop (12);
        if (locationOnPage)
        {
            locationRow = p.removeFromTop (58);
            auto row = locationRow.withTrimmedTop (16);
            useDefault.setBounds (row.removeFromLeft (190).withHeight (28));
            folderName.setBounds (row.removeFromRight (200).withHeight (28));
            row.removeFromRight (16);
            chooseLocation.setBounds (row.removeFromRight (100).withHeight (28));
            row.removeFromRight (8);
            locationLabel.setBounds (row.withHeight (28));
            p.removeFromTop (10);
        }
        auto foot = p.removeFromBottom (36);
        chooseFile.setBounds (foot.removeFromLeft (180));
        open.setBounds (foot.removeFromRight (180));
        p.removeFromBottom (10);
        list->setBounds (p);
    }
    else
    {
        p.removeFromTop (44);
        locationRow = p.removeFromTop (60);
        auto row = locationRow.withTrimmedTop (18);
        useDefault.setBounds (row.removeFromLeft (190).withHeight (28));
        folderName.setBounds (row.removeFromRight (200).withHeight (28));
        row.removeFromRight (16);
        chooseLocation.setBounds (row.removeFromRight (100).withHeight (28));
        row.removeFromRight (8);
        locationLabel.setBounds (row.withHeight (28));
        p.removeFromTop (34);
        auto locations = p.removeFromTop (30);
        removeLocation.setBounds (locations.removeFromRight (170));
        locations.removeFromRight (8);
        addLocation.setBounds (locations.removeFromRight (150));
        locations.removeFromRight (8);
        locationsBox.setBounds (locations);
        p.removeFromTop (24);
        for (auto* t : { &showPaths, &showLocation, &atStart })
        {
            t->setBounds (p.removeFromTop (28).withWidth (520));
            p.removeFromTop (6);
        }
        p.removeFromTop (18);
        dropLabel.setBounds (p.removeFromTop (24).withWidth (560));
        p.removeFromTop (4);
        dropStrength.setBounds (p.removeFromTop (26).withWidth (360));
    }
}

//==============================================================================
HubWindow::HubWindow (pr7s::Studio& studio)
    : DocumentWindow ("Paragon PR7-S Hub", onyx, DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    hub = new Hub (studio);
    setContentOwned (hub, true);
    setResizable (true, false);
    setResizeLimits (980, 620, 4000, 3000);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
}

void HubWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}
