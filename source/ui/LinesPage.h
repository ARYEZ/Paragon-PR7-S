#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// BASS & MELODY (0.18.0, the owner asked): ideas for a bass line and a melody to go
// with the chord track's chords. Six of each, side by side, each drawn as a little
// piano roll over the chords; a click (or HEAR) plays it with the chords underneath,
// USE IT puts it on a new track as a MIDI part under them (one undo step), MORE IDEAS
// deals the next variation, HEAR CHORDS plays the chords alone.
class LinesPage final : public juce::Component,
                        private juce::Timer
{
public:
    LinesPage (pr7s::Studio&, std::vector<pr7s::ChordSpan>);
    ~LinesPage() override;
    void setChords (std::vector<pr7s::ChordSpan>);   // (the window opened again on other chords)

    void paint (juce::Graphics&) override;
    void resized() override;

    // What the keys do (tests drive them).
    void play (pr7s::LineIdea::Part, int index);
    void playChords();
    void stop();
    int use (pr7s::LineIdea::Part, int index);   // the new track's id
    void moreIdeas();
    int getVariation() const noexcept { return variation; }
    const std::vector<pr7s::LineIdea>& getIdeas (pr7s::LineIdea::Part part) const noexcept { return part == pr7s::LineIdea::Part::bass ? bass : melody; }
    std::optional<std::pair<pr7s::LineIdea::Part, int>> getPlaying() const noexcept { return playing; }

    class Card;
    static constexpr int headerHeight = 64, columnTitleHeight = 30, gap = 12;

private:
    void timerCallback() override;
    void rebuild();
    pr7s::Studio& studio;
    std::vector<pr7s::ChordSpan> spans;
    std::vector<pr7s::LineIdea> bass, melody;
    int variation = 0;
    std::optional<std::pair<pr7s::LineIdea::Part, int>> playing;
    bool playingChords = false;
    juce::OwnedArray<Card> cards;
    juce::TextButton hearChords { "HEAR CHORDS" }, more { "MORE IDEAS" }, stopKey { "STOP" };
    juce::Rectangle<int> bassTitle, melodyTitle;
    double barBeats = 4.0;
};

// Its window: kept open beside the studio while you listen.
class LinesWindow final : public juce::DocumentWindow
{
public:
    LinesWindow (pr7s::Studio&, std::vector<pr7s::ChordSpan>);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    LinesPage& getPage() { return *page; }

private:
    LinesPage* page = nullptr;
};
