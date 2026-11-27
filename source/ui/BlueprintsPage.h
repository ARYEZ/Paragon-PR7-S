#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// BLUEPRINTS (0.152.0, the owner: song layouts as presets "like we do with expansion rigs"): ours and the owner's own
// listed on the left; the chosen one's blocks drawn as the SECTIONS row would show them, its description, tempo and
// chords; a KEY (the blueprint's own to start), CHORDS and TEMPO (each a choice: the owner's "carry chords but give the
// option to turn the chords off"), USE IT (asks first when the row already has blocks), SAVE THIS SONG'S SECTIONS AS A
// BLUEPRINT..., DELETE (the owner's own only).
class BlueprintsPage final : public juce::Component,
                             private juce::ListBoxModel
{
public:
    explicit BlueprintsPage (pr7s::Studio&);
    ~BlueprintsPage() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    void refreshList();                       // the folder read again
    void choose (int row);
    int getChosen() const noexcept { return chosen; }
    const std::vector<pr7s::Blueprint>& getBlueprints() const noexcept { return blueprints; }
    void setKey (const pr7s::Key&);
    pr7s::Key getKey() const;
    void setWithChords (bool);
    void setWithTempo (bool);
    // (0.262.0) BUILD THE MUSIC (on: every block built, as BUILD INTO does, with the song's lanes and options).
    void setWithMusic (bool);
    bool isWithMusic() const { return musicKey.getToggleState(); }
    // (0.262.0) The picker: words (name, description, group, types), a group, a length, a tempo and a mode; favourites.
    enum class Length { any, shortSong, medium, longSong };
    enum class Tempo { any, slow, mid, fast };
    enum class Mode { any, minor, major };
    void setSearch (const juce::String&);
    void setGroup (const juce::String&);      // "" all; favouritesGroup; myGroup; else a blueprint's group
    void setLength (Length);
    void setTempo (Tempo);
    void setMode (Mode);
    void setFavourite (int blueprint, bool);
    bool isFavourite (int blueprint) const;
    std::vector<int> getShown() const;        // the blueprints the filters let through, as listed
    juce::StringArray getGroups() const;      // every blueprint's group, A to Z
    static constexpr const char* favouritesGroup = "FAVOURITES";
    static constexpr const char* myGroup = "MY BLUEPRINTS";
    bool use();                               // asks first when the row has blocks (askToReplace)
    bool saveSongAs (const juce::String& name);
    bool deleteChosen();
    // A question (tests answer it; else a box): the row already has blocks, replace them?
    std::function<void (std::function<void (bool)>)> askToReplace;
    std::function<void()> onUsed;
    juce::Button& getUseKey() noexcept { return useKey; }
    juce::Button& getDeleteKey() noexcept { return deleteKey; }
    juce::Rectangle<int> getStripArea() const noexcept { return stripArea; }

    static constexpr int preferredWidth = 1280, preferredHeight = 720, listWidth = 380, rowHeight = 46, gap = 12, starWidth = 22;

private:
    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int row) override;
    void refreshKeys();
    void askName();

    pr7s::Studio& studio;
    std::vector<pr7s::Blueprint> blueprints;
    int chosen = -1;
    juce::ListBox list;
    juce::ComboBox keyBox;
    juce::ToggleButton chordsKey { "CHORDS" }, tempoKey { "TEMPO" }, musicKey { "BUILD THE MUSIC" };
    juce::TextEditor searchBox;
    juce::ComboBox groupBox, lengthBox, tempoBox, modeBox;
    struct Row { int blueprint = -1; juce::String heading; };
    std::vector<Row> rows;                    // the list: group headings and blueprints
    void refreshRows();
    bool passes (const pr7s::Blueprint&) const;
    juce::String groupOf (const pr7s::Blueprint&) const;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    juce::TextButton useKey { "USE IT" }, saveKey { "SAVE THIS SONG'S SECTIONS AS A BLUEPRINT..." }, deleteKey { "DELETE" };
    juce::Rectangle<int> stripArea, infoArea;
    juce::String status;
    std::unique_ptr<juce::AlertWindow> nameBox;
};

class BlueprintsWindow final : public juce::DocumentWindow
{
public:
    explicit BlueprintsWindow (pr7s::Studio&);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    BlueprintsPage& getPage() { return *page; }

private:
    BlueprintsPage* page = nullptr;
};
