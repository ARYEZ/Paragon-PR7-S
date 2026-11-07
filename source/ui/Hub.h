#pragma once

#include "engine/Studio.h"
#include "ui/KeyCommands.h"

class InstrumentsPage;

// The Hub (0.12.0, the owner asked: a start window like the other DAW's Hub,
// opening on the folder where the songs are kept), in the PR7 family's look.
//
// Top: the pearl unit with the PR7-S mark; the audio driver and the stereo
// output (the manual's audio settings section), the driver's own panel, and
// SETTINGS for the rest. Left: PROJECTS and HUB SETTINGS. The projects page:
// CREATE EMPTY (a name, then the song in a folder of its own at the project
// location: the manual's Use Default Location / Default Location / Project
// Folder Name, shown on the page), a search, and the songs in sections (RECENT,
// TEMPLATES, IN <each project folder>), each folded by clicking its title;
// double-click a song to open it (a template: a new song from it); right-click
// for Remove from the List, Show in Explorer, Rename, Delete, Clear the List.
// Foot: CHOOSE FILE... and OPEN. The Hub's settings: the project location, user
// locations (more folders listed), show the paths, show the location on the
// page, open the Hub at start.
class Hub final : public juce::Component,
                  private juce::ChangeListener
{
public:
    explicit Hub (pr7s::Studio&);
    ~Hub() override;

    // What the main window does for it (with the unsaved-changes question first).
    std::function<void (const juce::File&)> onOpen;
    std::function<void (const juce::String& name)> onCreate;      // Create Empty (a name; empty: the folder name)
    std::function<void (const juce::File&)> onTemplate;
    std::function<void()> onChooseFile, onSettings;

    // The list (tests): what's shown, by section; a search.
    struct Item { bool header = false; juce::String section; juce::File file; };
    const std::vector<Item>& getItems() const noexcept { return items; }
    void setSearch (const juce::String&);
    void openItem (int index);   // as a double-click
    void showSettingsPage (bool);
    bool isShowingSettings() const noexcept { return settingsPage; }
    // INSTRUMENTS (0.62.0): the third tab (ui/InstrumentsPage.h), made the first time it's shown.
    void showInstrumentsPage();
    bool isShowingInstruments() const noexcept { return instrumentsPage; }
    InstrumentsPage* getInstrumentsPage() noexcept { return instruments.get(); }
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int preferredWidth = 1180, preferredHeight = 760;

private:
    class SongList;
    pr7s::Studio& studio;
    std::unique_ptr<SongList> list;
    std::vector<Item> items;
    juce::StringArray folded;
    bool settingsPage = false, instrumentsPage = false;
    std::unique_ptr<InstrumentsPage> instruments;
    juce::Image satin;

    juce::TextButton projectsTab { "PROJECTS" }, settingsTab { "HUB SETTINGS" }, instrumentsTab { "INSTRUMENTS" },
                     createEmpty { "+  CREATE EMPTY" }, chooseFile { "CHOOSE FILE..." }, open { "OPEN" },
                     driverPanel { "DRIVER PANEL" }, audioSettings { "SETTINGS" },
                     chooseLocation { "CHOOSE..." }, addLocation { "ADD A FOLDER..." }, removeLocation { "REMOVE THE FOLDER" };
    juce::ComboBox driverBox, outputBox, locationsBox;
    juce::TextEditor search, folderName;
    juce::ToggleButton useDefault { "Use Default Location" }, showPaths { "Show File Paths" },
                       showLocation { "Show the Project Location on the Projects Page" }, atStart { "Open the Hub When the Studio Starts" };
    juce::Label locationLabel, dropLabel;
    juce::Slider dropStrength;   // DRAG HIGHLIGHT (0.18.2)
    juce::Label humanizeTitle, humanizeKeyLabel, humanizeAmountLabel, humanizeNote;
    juce::TextButton humanizeSetKey { "SET KEY" }, humanizeClearKey { "CLEAR" };
    juce::Slider humanizeAmount;
    juce::ToggleButton humanizeLayers { "Keep velocities in their layers (1-59 / 60-119 / 120-126 / 127)" };
    // AUTO-SAVE (0.72.0): how often, how many copies kept, the folder.
    juce::Label autoSaveTitle, autoSaveNote;
    juce::ComboBox autoSaveBox, keepBox;
    juce::TextButton showBackups { "SHOW BACKUPS" };
    static constexpr int autoSaveChoices[] = { 0, 1, 2, 5, 10, 15, 30 };
    static constexpr int keepChoices[] = { 3, 5, 10, 20, 50 };
    KeyCommands::Map* keyMap = nullptr;
    bool listeningForKey = false;
    void refreshHumanize();
public:
    juce::Slider& getDropStrength() noexcept { return dropStrength; }
    juce::ComboBox& getAutoSaveBox() noexcept { return autoSaveBox; }   // (0.72.0: tests)
    juce::ComboBox& getKeepBox() noexcept { return keepBox; }

    // HUMANIZE (0.20.1, the owner asked: the key and the settings in the Hub). The key map the
    // studio uses (MainView's: the same assignments as KEYS); its command "MIDI.Humanize".
    void setKeyCommands (KeyCommands::Map*);
    void listenForHumanizeKey();                    // SET KEY: the next key pressed is the one
    bool isListeningForHumanizeKey() const noexcept { return listeningForKey; }
    bool takeHumanizeKey (const juce::KeyPress&);   // (as a key pressed while listening; Escape cancels)
    void clearHumanizeKey();
    juce::String getHumanizeKeyText() const;        // "ALT+H", "NONE"
    juce::Slider& getHumanizeAmount() noexcept { return humanizeAmount; }
    juce::ToggleButton& getHumanizeLayers() noexcept { return humanizeLayers; }
    bool keyPressed (const juce::KeyPress&) override;
    static constexpr const char* humanizeCommand = "MIDI.Humanize";
private:
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> header, nav, page, locationRow;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refreshAudio();
    void refreshSettings();
    void showMenu (int index);
    void rename (const juce::File&);
    friend class SongList;
};

class HubWindow final : public juce::DocumentWindow
{
public:
    explicit HubWindow (pr7s::Studio&);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    Hub& getHub() { return *hub; }

private:
    Hub* hub = nullptr;
};
