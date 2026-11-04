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
// VELOCITIES (0.19.3, the owner asked): the guitar ideas' eight velocities, matched by ear to
// the guitar library's layers (Shreddage: 1-59 palm mute, 60-119 sustain, 120-126 rakes, 127
// pinch harmonics). A bar per value (drag, or double-click to type), its layer beside it, RESET.
class GuitarVelocityPanel final : public juce::Component
{
public:
    explicit GuitarVelocityPanel (pr7s::GuitarVelocities);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setValue (int index, int value);   // (as a drag would: onChange called)
    void reset();                           // the defaults (Shreddage's layout)
    const pr7s::GuitarVelocities& getValues() const noexcept { return values; }
    std::function<void (const pr7s::GuitarVelocities&)> onChange;
    static constexpr int rowHeight = 30, width = 440, titleHeight = 54;

private:
    void refresh();
    pr7s::GuitarVelocities values;
    juce::OwnedArray<juce::Slider> bars;
    juce::TextButton resetKey { "RESET (SHREDDAGE)" };
};

// HUMANIZE (0.20.0, the owner asked): how much the ideas are loosened (0: on the grid) and FEEL
// (rhythm and bass a little ahead of the beat, leads and melodies a little behind).
class HumanizePanel final : public juce::Component
{
public:
    HumanizePanel (float amount, bool feel);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setAmount (float);   // (as a drag would: onChange called)
    void setFeel (bool);
    float getAmount() const noexcept { return amount; }
    bool getFeel() const noexcept { return feel; }
    std::function<void (float amount, bool feel)> onChange;
    static constexpr int width = 420, height = 170;

private:
    float amount;
    bool feel;
    juce::Slider bar { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };
    juce::TextButton feelKey { "FEEL" };
};

class LinesPage final : public juce::Component,
                        private juce::Timer
{
public:
    LinesPage (pr7s::Studio&, std::vector<pr7s::ChordSpan>);
    ~LinesPage() override;
    void setChords (std::vector<pr7s::ChordSpan>);   // (the window opened again on other chords)

    void paint (juce::Graphics&) override;
    void resized() override;

    enum class Tab { piano, guitar, bass, patterns };   // (0.39.0: PATTERNS)
    void setPatternGroup (const juce::String&);   // the PATTERNS tab's group (settings key "ideasPatternGroup")
    juce::String getPatternGroup() const noexcept { return patternGroup; }
    static std::vector<pr7s::LineIdea::Part> partsOf (Tab);
    static juce::String titleOf (pr7s::LineIdea::Part);
    void setTab (Tab);   // (remembered: settings key "ideasTab")
    Tab getTab() const noexcept { return tab; }
    void setTuning (const juce::String&);   // the guitar ideas' (Studio::setGuitarTuning), dealt again
    const pr7s::GuitarTuning& getTuning() const;
    void setGuitarVelocities (const pr7s::GuitarVelocities&);   // (Studio::setGuitarVelocities), guitar ideas dealt again
    void showVelocities();                                     // the panel in a call-out under VELOCITIES
    void setHumanize (float amount, bool feel);                // (Studio's ideas humanize), every tab dealt again
    void showHumanize();
    static juce::String humanizeText (float amount);           // "HUMANIZE 40%" / "HUMANIZE OFF"

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
    std::array<std::vector<pr7s::LineIdea>, 6> ideas;
    juce::String patternGroup { "ARPEGGIOS" };
    juce::ComboBox groupBox;
    int variation = 0;
    Tab tab = Tab::piano;
    std::optional<std::pair<pr7s::LineIdea::Part, int>> playing;
    bool playingChords = false;
    juce::OwnedArray<Card> cards;
    juce::TextButton hearChords { "HEAR CHORDS" }, more { "MORE IDEAS" }, stopKey { "STOP" };
    juce::TextButton pianoTab { "PIANO" }, guitarTab { "GUITAR" }, bassTab { "BASS" }, patternsTab { "PATTERNS" };
    juce::Label tuningLabel { {}, "TUNING" };
    juce::ComboBox tuningBox;
    juce::TextButton velocitiesKey { "VELOCITIES" };
    juce::TextButton humanizeKey;
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
