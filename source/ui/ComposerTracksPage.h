#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// (0.112.0, the owner asked) The Hub's COMPOSER TRACKS tab: for each composer lane the names of the
// tracks it plays on (several, by commas: the first track found with any of them; upper or lower case
// alike; the first name is what BUILD calls a new one), EVERY MATCHING TRACK (the owner's guitar left
// and right: the lane plays on each), and LOOSE COPIES (the copies after the first humanized again, as
// a second take). A track chosen in a lane's TRACK menu still wins for as long as the composer is open.
class ComposerTracksPage final : public juce::Component, private juce::Timer
{
public:
    explicit ComposerTracksPage (pr7s::Studio&);
    ~ComposerTracksPage() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();   // the rows from the studio's settings, and what each finds in the open song

    // (tests: as the boxes and keys)
    void setNames (int lane, const juce::String& commaSeparated);
    void setEvery (int lane, bool);
    void setLooseCopies (bool);
    void resetAll();
    juce::String getFoundText (int lane) const;

    struct Row
    {
        int lane = 0;
        juce::Label name;
        juce::TextEditor names;
        juce::ToggleButton every { "EVERY MATCHING TRACK" };
        juce::Label found;
    };
    Row& getRow (int lane) { return *rows[(size_t) lane]; }

    static juce::String laneName (int lane);   // "BASS", "GUITAR", ...

private:
    void timerCallback() override;
    pr7s::Studio& studio;
    std::array<std::unique_ptr<Row>, pr7s::Studio::numComposerTrackLanes> rows;
    juce::ToggleButton looseKey { "PLAY THE COPIES A LITTLE LOOSE (AS IF PLAYED TWICE)" };
    juce::TextButton resetKey { "ALL BACK TO THE USUAL NAMES" };
    juce::Rectangle<int> titleArea, noteArea, headArea;
    juce::uint64 seenChange = 0;
};
