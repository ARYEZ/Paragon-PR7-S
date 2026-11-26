#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>

#include "engine/Studio.h"

// (0.178.0, the owner asked) The Hub's GROOVE FOLDERS tab: the groove library's folders as a tree (PR7-D's linked
// folders, else the MediaBay's locations), each with how many grooves ARCHITECT can use in it. The star on a folder
// marks it as a favourite (its subfolders with it): a quarter of a section's library grooves come from the
// favourite folders, AUTOMATIC picks among them first, and fills too. A folder inside a favourite one shows a dim
// star (already counted). The grooves counted are the ones ARCHITECT draws from (the song's bar length).
class GrooveFoldersPage final : public juce::Component, private juce::Timer
{
public:
    explicit GrooveFoldersPage (pr7s::Studio&);
    ~GrooveFoldersPage() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();   // the library read again, the tree made again (open folders kept)

    // (tests)
    int countIn (const juce::File& folder) const;              // the grooves under it
    juce::StringArray childrenOf (const juce::File& folder) const;   // its subfolders' names holding grooves
    void toggle (const juce::File& folder);                   // as its star
    juce::TreeView& getTree() { return tree; }
    juce::String getSummary() const;
    juce::String getSourceText() const { return source.getText(); }
    void addFolder (const juce::File&);   // as ADD A GROOVE FOLDER... with that folder chosen
    void followLibrary();                 // (as the timer) made again when PR7-D's library or a MediaBay scan comes in

    static constexpr int starWidth = 24;

private:
    class FolderItem;
    pr7s::Studio& studio;
    std::vector<juce::String> paths;   // every groove's file, sorted
    juce::TreeView tree;
    std::unique_ptr<juce::TreeViewItem> root;
    juce::TextButton clearKey { "CLEAR ALL FAVOURITES" };
    juce::TextButton addKey { "ADD A GROOVE FOLDER..." };   // (0.211.0) into the MediaBay, used when PR7-D has none
    juce::Label summary;
    juce::Label source;   // (0.211.0) where the grooves come from, and why
    int shownGeneration = -1, shownScans = -1;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> titleArea, noteArea;

    juce::StringArray favourites;   // (0.179.0) the studio's, read when they change (not on every row drawn)
    mutable std::map<juce::String, int> counts;                  // (0.179.0) by folder, until the library is read again
    mutable std::map<juce::String, juce::StringArray> children;
    bool isFavourite (const juce::File&) const;
    void timerCallback() override;   // (0.179.0) favourites marked elsewhere (ARCHITECT's right-click) followed
    std::pair<size_t, size_t> rangeOf (const juce::String& prefix) const;
    void refreshSummary();
};
