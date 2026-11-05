#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// LEARN A DRUMMER (0.56.0, the owner asked: beats learned from real drummers, from a folder of
// whole songs of drum MIDI, one song a file, no section names in them).
//
// CHOOSE FOLDER... reads every MIDI file in the folder and those under it (engine/DrumLearning.h):
// each song's bars, then its sections found from the drums and named (intro, verse, chorus,
// bridge, breakdown, ending). Each song is a row: its sections as blocks in the family's colours,
// as long as their bars. A guess from the drums, so everything can be put right:
//   a click on a block      its menu: what it is, Leave Out / Learn From It, Split Here (at the bar
//                           clicked), Join With the Next, Hear It
//   drag a block's left edge   move where it starts (by bars)
//   double-click a block    hear it (through the selected track: a drum instrument there)
//   the row's tick          learn from the song or not
// NAME and SAVE PROFILE learn the profile (a section's grooves, fills, chances, cymbal, crashes,
// feel) and save it among the drummer profiles; IDEAS' DRUMS tab then plays from it (DRUMMER).
// A saved profile opens here again (its songs read again, its sections as they were saved).
class DrumLearnPage final : public juce::Component,
                            private juce::Timer
{
public:
    explicit DrumLearnPage (pr7s::Studio&);
    ~DrumLearnPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // What the controls do (tests drive them).
    int loadFolder (const juce::File&);                // the songs read (files that weren't: getUnread)
    void openProfile (const pr7s::DrumProfile&);
    const std::vector<pr7s::DrumSong>& getSongs() const noexcept { return songs; }
    const juce::StringArray& getUnread() const noexcept { return unread; }
    void setName (const juce::String&);
    juce::String getName() const;
    bool save (juce::String& error);                   // learns and saves; onSaved (name)
    std::function<void (const juce::String&)> onSaved;

    void setKind (int song, int section, pr7s::DrumSection);
    void setUsed (int song, int section, bool);
    void setSongUsed (int song, bool);
    bool split (int song, int section, int atBar);
    bool join (int song, int section);
    bool moveStart (int song, int section, int firstBar);
    void hear (int song, int section);
    void stop();
    std::pair<int, int> getHearing() const noexcept { return hearing; }   // (-1, -1): none
    juce::String getStatusText() const;
    juce::Rectangle<int> getSectionBounds (int song, int section) const;  // in this page
    juce::Rectangle<int> getUseBox (int song) const;                      // in this page

    static juce::Colour colourOf (pr7s::DrumSection);
    class Rows;
    static constexpr int headerHeight = 128, rowHeight = 62, gap = 12, nameWidth = 270, useWidth = 30;

private:
    void timerCallback() override;
    void chooseFolder();
    void changed();

    pr7s::Studio& studio;
    std::vector<pr7s::DrumSong> songs;
    juce::StringArray unread;
    juce::File folder;
    std::pair<int, int> hearing { -1, -1 };
    juce::TextButton chooseKey { "CHOOSE FOLDER..." }, saveKey { "SAVE PROFILE" }, stopKey { "STOP" };
    juce::TextEditor nameEditor;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<Rows> rows;
    juce::Viewport rowsView;
};

class DrumLearnWindow final : public juce::DocumentWindow
{
public:
    explicit DrumLearnWindow (pr7s::Studio&);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    DrumLearnPage& getPage() { return *page; }

private:
    DrumLearnPage* page = nullptr;
};
