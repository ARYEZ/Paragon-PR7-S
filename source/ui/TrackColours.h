#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// The colour menu (0.17.3, the owner asked, from the other DAW's "Colorize Selected
// Tracks"): only the PR7 family's colours, in every tint. The gradient's three on
// top, then the palette (FamilyColours: hues from the pink through the red to the
// orange, deep to pale), then SET TRACK COLOUR TO DEFAULT (the automatic one back).
// A click colours the tracks it was opened for and leaves it open to try another.
class TrackColourMenu final : public juce::Component
{
public:
    TrackColourMenu (pr7s::Studio&, std::vector<int> trackIds);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    std::optional<juce::Colour> colourAt (juce::Point<float>) const;   // (tests)
    juce::Rectangle<float> swatchFor (juce::Colour) const;             // (tests: where a colour is)
    juce::TextButton defaultKey { "SET TRACK COLOUR TO DEFAULT" };
    // (0.73.0, the owner asked) The colour over the tracks' rows in the track list: on, and how much.
    juce::ToggleButton listKey { "COLOUR IN THE TRACK LIST" };
    juce::Slider listAmount { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };
    void setListColour (bool on, int amount);   // (tests: as the toggle and the bar)
    // (0.79.0, the owner asked) COLOUR IN ORDER: track n takes colour n (the swatches numbered
    // 1-75 as they're laid out: the three, then the palette row by row), over a range of track
    // numbers, only the kinds ticked (the others skipped, the counting closing up over them).
    juce::ToggleButton orderKey { "COLOUR TRACKS IN ORDER: TRACK N = COLOUR N" };
    juce::Slider orderFrom { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };
    juce::Slider orderTo { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };
    juce::ToggleButton kindKeys[5] { juce::ToggleButton ("INSTRUMENT"), juce::ToggleButton ("AUDIO"), juce::ToggleButton ("GROUP"),
                                     juce::ToggleButton ("FX"), juce::ToggleButton ("FOLDER") };
    void setAutoColour (pr7s::AutoColour);   // (tests: as the keys and bars)
    juce::String getOrderText() const;       // the line under them

    // Opened beside `around` for the tracks given (the selected ones, else the one clicked).
    static void show (juce::Component& around, pr7s::Studio&, std::vector<int> trackIds);

    static constexpr int cell = 30, gap = 4, margin = 12, titleHeight = 30, familyHeight = 40, footerHeight = 44, listHeight = 70, orderHeight = 198;

private:
    pr7s::Studio& studio;
    const std::vector<int> trackIds;
    std::vector<std::pair<juce::Rectangle<float>, juce::Colour>> swatches;
    std::optional<juce::Colour> hover;
    juce::Colour current() const;   // the first track's
    int lastNumber() const;          // how many tracks the list numbers (the TO bar's top = "the last")
    void refreshOrder();             // the keys and bars from the song's rule
    void orderChanged();             // the keys and bars into the song's rule
    bool refreshing = false;
};

// The colour menu's key: a small swatch of the family's gradient (the track list's header).
class ColourKey final : public juce::Button
{
public:
    ColourKey() : juce::Button ("Colour") { setTooltip ("Colour the selected tracks (the PR7 family's colours, every tint)"); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};
