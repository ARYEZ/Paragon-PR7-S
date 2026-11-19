#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// (0.178.0, the owner asked) The Hub's GROOVE FOLDERS tab: the groove library's folders as a tree (PR7-D's linked
// folders, else the MediaBay's locations), each with how many grooves ARCHITECT can use in it. The star on a folder
// marks it as a favourite (its subfolders with it): a quarter of a section's library grooves come from the
// favourite folders, AUTOMATIC picks among them first, and fills too. A folder inside a favourite one shows a dim
// star (already counted). The grooves counted are the ones ARCHITECT draws from (the song's bar length).
class GrooveFoldersPage final : public juce::Component
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

    static constexpr int starWidth = 24;

private:
    class FolderItem;
    pr7s::Studio& studio;
    std::vector<juce::String> paths;   // every groove's file, sorted
    juce::TreeView tree;
    std::unique_ptr<juce::TreeViewItem> root;
    juce::TextButton clearKey { "CLEAR ALL FAVOURITES" };
    juce::Label summary;
    juce::Rectangle<int> titleArea, noteArea;

    std::pair<size_t, size_t> rangeOf (const juce::String& prefix) const;
    void refreshSummary();
};
