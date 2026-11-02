#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// IDEAS (0.18.0 as BASS & MELODY, the owner asked; 0.19.0: tabs for piano, guitar and bass).
// Ideas to go with the chord track's chords, each drawn as a little piano roll over the
// chords; a click (or HEAR) plays it with the chords underneath, USE IT puts it in a MIDI
// part under them (onto the selected track or a new one; one undo step), MORE IDEAS deals
// the next variation, HEAR CHORDS plays the chords alone.
//   PIANO   CHORDS | MELODY
//   GUITAR  RHYTHM | LEAD   (TUNING: nothing below the lowest string, where guitar libraries
//                            keep their keyswitches; the owner's: 7-string drop A)
//   BASS    six bass lines across
class LinesPage final : public juce::Component,
                        private juce::Timer
{
public:
    LinesPage (pr7s::Studio&, std::vector<pr7s::ChordSpan>);
    ~LinesPage() override;
    void setChords (std::vector<pr7s::ChordSpan>);   // (the window opened again on other chords)

    void paint (juce::Graphics&) override;
    void resized() override;

    enum class Tab { piano, guitar, bass };
    static std::vector<pr7s::LineIdea::Part> partsOf (Tab);
    static juce::String titleOf (pr7s::LineIdea::Part);
    void setTab (Tab);   // (remembered: settings key "ideasTab")
    Tab getTab() const noexcept { return tab; }
    void setTuning (const juce::String&);   // the guitar ideas' (Studio::setGuitarTuning), dealt again
    const pr7s::GuitarTuning& getTuning() const;

    // What the keys do (tests drive them).
    void play (pr7s::LineIdea::Part, int index);
    void playChords();
    void stop();
    int use (pr7s::LineIdea::Part, int index, int ontoTrackId = 0);   // the track's id (0: a new one)
    void moreIdeas();
    int getVariation() const noexcept { return variation; }
    const std::vector<pr7s::LineIdea>& getIdeas (pr7s::LineIdea::Part part) const noexcept { return ideas[(size_t) part]; }
    std::optional<std::pair<pr7s::LineIdea::Part, int>> getPlaying() const noexcept { return playing; }
    int getNumCardsShown() const noexcept { return cards.size(); }

    class Card;
    static constexpr int headerHeight = 64, tabsHeight = 36, columnTitleHeight = 30, gap = 12;

private:
    void timerCallback() override;
    void rebuild();
    void makeCards();
    void showUseMenu (pr7s::LineIdea::Part, int index, Card&);
    pr7s::Studio& studio;
    std::vector<pr7s::ChordSpan> spans;
    std::array<std::vector<pr7s::LineIdea>, 5> ideas;
    int variation = 0;
    Tab tab = Tab::piano;
    std::optional<std::pair<pr7s::LineIdea::Part, int>> playing;
    bool playingChords = false;
    juce::OwnedArray<Card> cards;
    juce::TextButton hearChords { "HEAR CHORDS" }, more { "MORE IDEAS" }, stopKey { "STOP" };
    juce::TextButton pianoTab { "PIANO" }, guitarTab { "GUITAR" }, bassTab { "BASS" };
    juce::Label tuningLabel { {}, "TUNING" };
    juce::ComboBox tuningBox;
    std::vector<juce::Rectangle<int>> columnTitles;
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
