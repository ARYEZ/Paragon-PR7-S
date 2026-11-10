#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// (0.80.0, the owner asked: the other DAW's Track Type Default Colors, "one step further") The Hub's
// TRACK COLOURS tab: for each type of track (instrument, audio, FX channel, group channel, folder)
// the colour a new one starts with (AUTOMATIC: red, pink and orange in turn, as before, or one of
// the family's 75) and whether its colour shows over its row in the track list, and how much.
// APPLY TO THIS SONG gives the open song's tracks their type's look (one undo step).

// The family's colours to pick from: AUTOMATIC, the gradient's three, the palette (as the colour menu).
class FamilyColourPicker final : public juce::Component
{
public:
    explicit FamilyColourPicker (std::optional<juce::Colour> current);
    std::function<void (std::optional<juce::Colour>)> onPick;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    std::optional<juce::Colour> colourAt (juce::Point<float>) const;   // (tests)
    juce::TextButton automaticKey { "AUTOMATIC: RED, PINK, ORANGE IN TURN" };

    // Opened beside `around`; `onPick` is called with the choice (it stays open to try another).
    static void show (juce::Component& around, std::optional<juce::Colour> current, std::function<void (std::optional<juce::Colour>)> onPick);

    static constexpr int cell = 26, gap = 4, margin = 12;

private:
    std::optional<juce::Colour> current;
    std::vector<std::pair<juce::Rectangle<float>, juce::Colour>> swatches;
};

class TrackTypeColoursPage final : public juce::Component
{
public:
    explicit TrackTypeColoursPage (pr7s::Studio&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();   // the rows from the studio's settings

    // (tests: as the keys)
    void setColour (pr7s::Track::Kind, std::optional<juce::Colour>);
    void setListColour (pr7s::Track::Kind, bool on, int amount);
    void applyToSong();
    void resetAll();

    // A type's colour: the colour itself, or AUTOMATIC's three in turn.
    struct SwatchKey final : juce::Button
    {
        SwatchKey() : juce::Button ("Colour") {}
        std::optional<juce::Colour> colour;
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };
    struct Row
    {
        pr7s::Track::Kind kind;
        juce::Label name;
        SwatchKey swatch;
        juce::ToggleButton listKey { "COLOUR IN THE TRACK LIST" };
        juce::Slider amount { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };
    };
    Row& getRow (pr7s::Track::Kind kind) { return *rows[(size_t) kind]; }

private:
    pr7s::Studio& studio;
    std::array<std::unique_ptr<Row>, pr7s::Studio::numTrackTypes> rows;
    juce::TextButton applyKey { "APPLY TO THIS SONG'S TRACKS" }, resetKey { "ALL AUTOMATIC" };
    juce::Rectangle<int> titleArea, noteArea, headArea;
};
