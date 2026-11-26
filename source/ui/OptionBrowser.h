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
class OptionBrowser final : public juce::Component, private juce::Timer
{
public:
    struct Option
    {
        int id = 0;                  // the caller's (an approach or style number)
        juce::String name, description, group;
        pr7s::OptionMeasure measure;
        pr7s::LineIdea idea;         // what it writes (none: not drawn, not measured)
        bool drums = false;
        // (0.179.0) Its part not made yet: the list shows it at once and `onFill` makes the parts a few at a time
        // (the chosen first, then the rows in view, then the rest).
        bool pending = false;
    };

    OptionBrowser (const juce::String& title, std::vector<Option>, int chosenId, pr7s::OptionMemory memory, const juce::String& listKey);

    std::function<void (int id)> onHear;
    std::function<void (int id)> onUse;
    std::function<void (const juce::String& name, bool favourite)> onFavourite;
    std::function<void()> onClosed;   // when it goes (its box closed)
    // (0.178.0) A right-click on an option in the list (its id; the mouse on the screen): the caller's menu.
    std::function<void (int id, juce::Point<int> screen)> onRightClick;
    // (0.178.0) The options again (their groups changed): the chosen, the group shown and the search kept.
    void setOptions (std::vector<Option>);
    // (0.179.0) Makes a pending option's part (idea, measure, description): true when it did. Set it before the
    // browser shows; the options still pending are filled on a timer, about a frame's worth of work at a time.
    std::function<bool (Option&)> onFill;
    int countPending() const;
    void fillAllNow();   // (tests)
    // (0.180.0, the owner: "the groove automatically play when we highlight it" and the arrow keys) Choosing an
    // option (a click on its row, Up / Down) hears it at once, as HEAR does; Enter is USE IT.
    void setAutoHear (bool on) { autoHear = on; }
    bool isAutoHear() const { return autoHear; }
    void pick (int id);          // as a click on its row: chosen, and heard when auto-hear is on
    void step (int by);          // the next / previous option shown (Up / Down)
    bool keyPressed (const juce::KeyPress&) override;
    ~OptionBrowser() override;

    static constexpr int width = 1000, height = 560;
    // (0.216.0) What the chosen option's panel shows: its description, then "Inspired by: ..." from the owner's
    // private file (pr7s::inspiredBy) when it names the option.
    static juce::String shownDescription (const Option&);
    static inline const juce::String favouritesGroup = "FAVOURITES", recentGroup = "RECENT", everythingGroup = "EVERYTHING";

    // For tests (and what the keys do).
    void setGroup (const juce::String&);
    void setSearch (const juce::String&);
    void setFeel (pr7s::OptionFeel);       // unknown: any
    void setEnergy (int energy);           // 0: any
    void choose (int id);
    // (0.157.0) No HEAR key (a list that can't be tried in place: INTO THE NEXT from the block menu).
    void setHearable (bool);
    // (0.158.0) The groups on the left in this order (others after, as they come).
    void setGroupOrder (const juce::StringArray&);
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
    juce::String getHeading() const { return title; }

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
    mutable int describedId = -1;                          // (0.229.0) the description last made for the paint
    mutable juce::String describedName, describedDescription, describedText;
    mutable juce::uint32 describedAt = 0;
    float scroll = 0.0f;
    bool measured = false, drawn = false, hearable = true;
    juce::StringArray groupOrder;

    // (0.180.0) Up / Down in the search box move through the list too (a text box keeps them for its caret).
    struct SearchBox final : public juce::TextEditor
    {
        std::function<bool (const juce::KeyPress&)> onListKey;
        bool keyPressed (const juce::KeyPress& k) override
        {
            if ((k.isKeyCode (juce::KeyPress::upKey) || k.isKeyCode (juce::KeyPress::downKey)) && onListKey && onListKey (k))
                return true;
            return juce::TextEditor::keyPressed (k);
        }
    };
    SearchBox search;
    bool autoHear = false;
    std::vector<std::unique_ptr<juce::TextButton>> feelKeys, energyKeys;
    juce::TextButton hearKey { "HEAR" }, useKey { "USE IT" }, favouriteKey { "FAVOURITE" };

    juce::Rectangle<int> groupsArea, listArea, detailArea, pictureArea;
    static constexpr int rowHeight = 26, groupHeight = 28;

    const Option* find (int id) const;
    bool inGroup (const Option&, const juce::String& g) const;
    bool passes (const Option&) const;
    bool passes (const Option&, const juce::StringArray& words) const;   // (0.191.0) the search's words split once
    void refreshKeys();
    void showChosen();
    juce::Rectangle<int> groupRow (int index) const;
    juce::Rectangle<int> listRow (int index) const;
    void paintPicture (juce::Graphics&, const Option&, juce::Rectangle<int>) const;
    void timerCallback() override;
    bool fillOne (Option&);
    void noteKinds();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptionBrowser)
};

// (0.160.0, the owner: "needs to open centered and maximized and needs to be freely resizable") The browser in a
// window of its own (it was a call-out box of a fixed size): centred on the screen the composer is on, then
// maximized; resizable down to `leastWidth` x `leastHeight`. Closing it ends a trial as closing the box did (the
// browser's own onClosed, when it goes with the window).
class OptionBrowserWindow final : public juce::DocumentWindow
{
public:
    OptionBrowserWindow (std::unique_ptr<OptionBrowser>, juce::Component* near);
    void closeButtonPressed() override;
    std::function<void()> onClose;   // (the owner deletes the window: never inside this call)
    OptionBrowser& getBrowser() { return *browser; }
    static constexpr int leastWidth = 820, leastHeight = 480;
    // The screen area it opens on: the display under `near`'s middle (the main display without one).
    static juce::Rectangle<int> screenFor (juce::Component* near);

private:
    OptionBrowser* browser = nullptr;
};
