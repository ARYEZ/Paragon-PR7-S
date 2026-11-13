#pragma once

#include "engine/Studio.h"
#include "ui/ChordUi.h"

// A block on the SECTIONS row edited where it is (0.103.0, the owner: "right click and get a quick menu ... name, type,
// length, and the ability to change or reorder that block's chords ... the scale set for the entire song, then for each
// block what chords from that scale we're going to use"). Opened by a right-click on a block (a CallOutBox). Every
// change goes to the studio at once (each one undo step): NAME, TYPE, LENGTH (what follows moves), the SONG KEY (the
// chord track's scale from the song's start), and the block's chords, end to end from its start, each chosen from the
// key's chords: click a chord to change it, its length, move or remove it; drag one sideways to put it somewhere else;
// + CHORD adds the chord the one before suggests. OPEN IN THE COMPOSER builds the block; MORE... is the block's old menu
// (insert, duplicate, link, locators, delete).
class SectionEditor final : public juce::Component
{
public:
    SectionEditor (pr7s::Studio&, int blockId);
    ~SectionEditor() override;

    std::function<void (int index, const juce::String& type)> onType;   // (ArrangeView::changeSectionType: may ask first)
    std::function<void (int index)> onOpen, onMore;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();                               // from the studio again (the block, its chords, the key)

    // Tests drive these as the keys and the strip do.
    int getIndex() const;                         // the block's index now (-1: gone)
    std::vector<pr7s::Studio::SectionChord> getChords() const { return chords; }
    pr7s::Key getKey() const;
    void setName (const juce::String&);
    void setType (const juce::String&);
    void setBars (int);
    void setSongKey (const pr7s::Key&);
    void changeChord (int i, const pr7s::Chord&);
    void setChordBars (int i, double bars);
    void moveChord (int from, int to);
    void removeChord (int i);
    void addChord();
    void clearChords();
    juce::Rectangle<int> getChordBounds (int i) const;   // (in this component)
    juce::ComboBox& getNameBox() { return nameBox; }
    juce::ComboBox& getTypeBox() { return typeBox; }
    juce::ComboBox& getLengthBox() { return lengthBox; }
    juce::ComboBox& getKeyBox() { return keyBox; }

    static constexpr int width = 720, height = 330;

private:
    class Strip;
    pr7s::Studio& studio;
    int blockId;
    std::vector<pr7s::Studio::SectionChord> chords;
    juce::Label title, info, chordsLabel, hint;
    juce::Label nameLabel { {}, "NAME" }, typeLabel { {}, "TYPE" }, lengthLabel { {}, "LENGTH" }, keyLabel { {}, "SONG KEY" };
    juce::ComboBox nameBox, typeBox, lengthBox, keyBox;
    std::unique_ptr<Strip> strip;
    juce::TextButton addKey { "+ CHORD" }, clearKey { "CLEAR CHORDS" }, openKey { "OPEN IN THE COMPOSER" }, moreKey { "MORE..." };
    bool refreshing = false;

    double beatsPerBar() const;
    double blockBeats() const;
    void write();                                 // chords -> the studio
    void showChordMenu (int i);
    friend class Strip;
};
