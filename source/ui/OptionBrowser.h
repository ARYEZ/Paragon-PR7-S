#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/OptionFeel.h"

// THE OPTION BROWSER (0.156.0, the owner chose mock-up D with C's feel filters): a lane's whole list of options when
// the short menu isn't enough (MORE OPTIONS...).
//
//   left     the groups with how many each has: ★ FAVOURITES, RECENT, EVERYTHING, then the list's own groups
//            (THIS TYPE'S OWN, FROM YOUR SONGS, STYLES...).
//   middle   a search box (any word of a name or description, across every group), the FEEL keys (STRAIGHT, HALF
//            TIME, DOUBLE TIME, BROKEN) and the ENERGY keys (CALM, MEDIUM, HEAVY): one of each at most, a key clicked
//            again lets go; then the options that pass, the star on the left adds or takes a favourite.
//   right    the chosen option: its name, group, feel and energy, what it does, its first two bars drawn (a drum
//            grid: cymbals, snare, toms, kick by sixteenths; else the notes as a small piano roll), HEAR (the
//            composer plays it with the other lanes, nothing kept yet), USE IT, ★ FAVOURITE.
//
// The feel and energy are measured from the part each option writes (engine/OptionFeel.h): a guide, not a label
// anyone wrote. The browser keeps no settings of its own: the favourites and recent are the Studio's.
class OptionBrowser final : public juce::Component
{
public:
    struct Option
    {
        int id = 0;                  // the caller's (an approach or style number)
        juce::String name, description, group;
        pr7s::OptionMeasure measure;
        pr7s::LineIdea idea;         // what it writes (none: not drawn, not measured)
        bool drums = false;
    };

    OptionBrowser (const juce::String& title, std::vector<Option>, int chosenId, pr7s::OptionMemory memory, const juce::String& listKey);

    std::function<void (int id)> onHear;
    std::function<void (int id)> onUse;
    std::function<void (const juce::String& name, bool favourite)> onFavourite;
    std::function<void()> onClosed;   // when it goes (its box closed)
    ~OptionBrowser() override;

    static constexpr int width = 1000, height = 560;
    static inline const juce::String favouritesGroup = "FAVOURITES", recentGroup = "RECENT", everythingGroup = "EVERYTHING";

    // For tests (and what the keys do).
    void setGroup (const juce::String&);
    void setSearch (const juce::String&);
    void setFeel (pr7s::OptionFeel);       // unknown: any
    void setEnergy (int energy);           // 0: any
    void choose (int id);
    // (0.157.0) No HEAR key (a list that can't be tried in place: INTO THE NEXT from the block menu).
    void setHearable (bool);
    bool isMeasured() const { return measured; }   // any option has a feel: the FEEL / ENERGY keys show
    void hear();
    void use();
    void toggleFavourite (int id);
    std::vector<int> getShown() const;     // ids in the list, in order
    int getChosen() const { return chosen; }
    juce::String getGroup() const { return group; }
    juce::StringArray getGroups() const;   // in order, as shown on the left
    int countIn (const juce::String& group) const;
    bool isFavourite (int id) const;
    juce::TextButton* keyNamed (const juce::String&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::String title, listKey;
    std::vector<Option> options;
    pr7s::OptionMemory memory;
    juce::String group = everythingGroup;
    pr7s::OptionFeel feel = pr7s::OptionFeel::unknown;
    int energy = 0;
    int chosen = 0;
    float scroll = 0.0f;
    bool measured = false, drawn = false, hearable = true;

    juce::TextEditor search;
    std::vector<std::unique_ptr<juce::TextButton>> feelKeys, energyKeys;
    juce::TextButton hearKey { "HEAR" }, useKey { "USE IT" }, favouriteKey { "FAVOURITE" };

    juce::Rectangle<int> groupsArea, listArea, detailArea, pictureArea;
    static constexpr int rowHeight = 26, groupHeight = 28;

    const Option* find (int id) const;
    bool inGroup (const Option&, const juce::String& g) const;
    bool passes (const Option&) const;
    void refreshKeys();
    void showChosen();
    juce::Rectangle<int> groupRow (int index) const;
    juce::Rectangle<int> listRow (int index) const;
    void paintPicture (juce::Graphics&, const Option&, juce::Rectangle<int>) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptionBrowser)
};
