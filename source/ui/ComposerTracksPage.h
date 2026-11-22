#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"
#include "ui/ValueSlider.h"

// (0.112.0, the owner asked) The Hub's COMPOSER TRACKS tab: for each composer lane the names of the
// tracks it plays on (several, by commas: the first track found with any of them; upper or lower case
// alike; the first name is what BUILD calls a new one), EVERY MATCHING TRACK (the owner's guitar left
// and right: the lane plays on each), and LOOSE COPIES (the copies after the first humanized again, as
// a second take), and (0.201.0) LEVEL: the fader a track ARCHITECT makes for the lane starts at. A track chosen in a lane's TRACK menu still wins for as long as the composer is open.
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
    void setLevel (int lane, float db);   // (0.201.0) the lane's new tracks' fader level
    // (0.204.0) TONE (the lead, clean and acoustic lanes): PR7-G's tone BUILD puts on the lane's tracks.
    void setTone (int lane, const pr7s::Studio::ToneChoice&);
    juce::ComboBox* getToneBox (int lane);   // (none for the other lanes)
    void refreshTones();                     // the boxes' lists again (the owner's PR7-G rigs)
    void resetAll();
    juce::String getFoundText (int lane) const;

    struct Row
    {
        int lane = 0;
        juce::Label name;
        juce::TextEditor names;
        juce::ToggleButton every { "EVERY MATCHING TRACK" };
        juce::Label found;
        ValueSlider level;   // (0.201.0) LEVEL: a new track's fader, dB (double-click to type)
        std::unique_ptr<juce::ComboBox> tone;   // (0.204.0) TONE (lead, clean, acoustic)
        juce::Label toneLabel;
        juce::StringArray toneNames;            // the box's tones, by item (from toneItems)
        juce::Array<juce::File> rigs;           // the box's rigs, by item (from rigItems)
    };
    Row& getRow (int lane) { return *rows[(size_t) lane]; }
    juce::Viewport& getView() { return view; }

    static juce::String laneName (int lane);   // "BASS", "GUITAR", ...

private:
    void timerCallback() override;
    void visibilityChanged() override;
    void showToneChoice (Row&);   // the box on the studio's choice
    static constexpr int toneItems = 10, rigItems = 1000, missingRigItem = 999;
    pr7s::Studio& studio;
    std::array<std::unique_ptr<Row>, pr7s::Studio::numComposerTrackLanes> rows;
    juce::ToggleButton looseKey { "PLAY THE COPIES A LITTLE LOOSE (AS IF PLAYED TWICE)" };
    juce::TextButton resetKey { "ALL BACK TO THE USUAL NAMES AND LEVELS" };
    juce::Rectangle<int> titleArea, noteArea, headArea;
    juce::Component content;    // (0.202.0) the rows, in `view`: scrolled when the panel is short
    juce::Viewport view;
    juce::uint64 seenChange = 0;
};
