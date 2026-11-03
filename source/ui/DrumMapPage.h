#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// The Drum Map Setup (0.26.0, the manual's Drum Map Setup Dialog): the song's drum maps on the
// left (NEW MAP: every sound "Sound n" on its own note; NEW COPY; REMOVE; LOAD a .drm or our own;
// SAVE as a .drm; the name above the list renames the chosen map); the chosen map's drum sounds
// on the right, in its order: PITCH, DRUM SOUND (double-click to rename), I-NOTE, O-NOTE, CHANNEL
// (click for a menu), MUTE (click). UP / DOWN move the selected sound in the order (the drum
// editor's rows). FOR THE TRACK gives the chosen map to the track the MIDI editor shows.
// Changes act at once on every track using the map (as the manual's), and are saved with the song.
class DrumMapPage final : public juce::Component,
                          private juce::ChangeListener,
                          private juce::TableListBoxModel
{
public:
    DrumMapPage (pr7s::Studio&, std::function<int()> trackForMap);
    ~DrumMapPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // For the tests (the keys do these).
    void selectMap (const juce::String& name);
    juce::String getSelectedMapName() const { return selectedMap; }
    juce::String newMap();
    juce::String newCopy();
    void removeSelected();
    bool renameSelected (const juce::String&);
    void setSoundName (int pitch, const juce::String&);
    void setInNote (int pitch, int note);
    void setOutNote (int pitch, int note);
    void setChannel (int pitch, int channel);   // -1: the track's
    void toggleMute (int pitch);
    void moveSound (int pitch, bool up);        // in the map's order
    void selectSound (int pitch);
    int getSelectedSound() const noexcept { return selectedSound; }
    void assignToTrack();
    juce::TableListBox& getTable() noexcept { return table; }

    enum Columns { pitchColumn = 1, nameColumn, inColumn, outColumn, channelColumn, muteColumn };

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    // The sounds' table.
    int getNumRows() override;
    void paintRowBackground (juce::Graphics&, int row, int width, int height, bool selected) override;
    void paintCell (juce::Graphics&, int row, int column, int width, int height, bool selected) override;
    void cellClicked (int row, int column, const juce::MouseEvent&) override;
    void cellDoubleClicked (int row, int column, const juce::MouseEvent&) override;
    struct MapsModel;   // the maps' list
    const pr7s::DrumMap* map() const;
    void edit (std::function<void (pr7s::DrumMap&)>);
    void refresh();

    pr7s::Studio& studio;
    std::function<int()> trackForMap;
    juce::String selectedMap;
    int selectedSound = -1;
    juce::ListBox maps;
    std::unique_ptr<MapsModel> mapsModel;
    juce::TableListBox table;
    juce::TextEditor nameField;
    juce::TextButton newKey { "NEW MAP" }, copyKey { "NEW COPY" }, removeKey { "REMOVE" }, loadKey { "LOAD..." }, saveKey { "SAVE..." },
                     upKey { "UP" }, downKey { "DOWN" }, assignKey { "FOR THE TRACK" };
    std::unique_ptr<juce::FileChooser> chooser;
};

class DrumMapWindow final : public juce::DocumentWindow
{
public:
    DrumMapWindow (pr7s::Studio&, std::function<int()> trackForMap);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    DrumMapPage& getPage() { return *page; }

private:
    DrumMapPage* page = nullptr;
};
