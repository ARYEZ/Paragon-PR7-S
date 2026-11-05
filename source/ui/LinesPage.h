#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"
#include "engine/DrumIdeas.h"

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

// MELODY SETTINGS (0.41.0, the owner asked: as their chord tool's melody controls): what the
// MELODY tab's composer makes: DENSITY (notes a bar), LOWEST / HIGHEST (the range), LEAPS (steps
// vs leaps), PHRASE (1, 2 or 4 bars: how long the motif is), REPEAT (how closely a returning
// phrase copies the first), RESTS (room to breathe), SYNCOPATION (off-beat pushes), DEFAULTS.
class MelodyPanel final : public juce::Component
{
public:
    explicit MelodyPanel (pr7s::MelodySettings);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setSettings (const pr7s::MelodySettings&);   // (as the controls would: onChange called)
    const pr7s::MelodySettings& getSettings() const noexcept { return settings; }
    std::function<void (const pr7s::MelodySettings&)> onChange;
    static juce::String summary (const pr7s::MelodySettings&);   // "MELODY: C3-G4, 2 BARS"
    static constexpr int width = 460, rowHeight = 32, titleHeight = 60;

private:
    void refresh();
    pr7s::MelodySettings settings;
    juce::Slider density, low, high, leaps, repeat, rests;
    juce::TextButton oneBar { "1 BAR" }, twoBars { "2 BARS" }, fourBars { "4 BARS" }, syncopation { "SYNCOPATION" }, defaults { "DEFAULTS" };
};

// RIFF SETTINGS (0.53.0, the owner asked): how the guitar tab's rhythm ideas are shaped, every
// STYLE's: LENGTH (a riff of 1, 2 or 4 bars before it repeats), PALM MUTES (fewer or more against
// open notes), TIGHTNESS (looser or tighter mutes), BUSYNESS (sparse to constant sixteenths), FILLS
// (the last beat of each phrase), DEFAULTS. At 50% the riffs are as written.
class RiffPanel final : public juce::Component
{
public:
    explicit RiffPanel (pr7s::RiffSettings);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setSettings (const pr7s::RiffSettings&);   // (as the controls would: onChange called)
    const pr7s::RiffSettings& getSettings() const noexcept { return settings; }
    std::function<void (const pr7s::RiffSettings&)> onChange;
    static juce::String summary (const pr7s::RiffSettings&);   // "RIFF: 2 BARS, FILLS"
    static constexpr int width = 460, rowHeight = 32, titleHeight = 60;

private:
    void refresh();
    pr7s::RiffSettings settings;
    juce::Slider palm, tight, busy;
    juce::TextButton oneBar { "1 BAR" }, twoBars { "2 BARS" }, fourBars { "4 BARS" }, fills { "FILLS" }, defaults { "DEFAULTS" };
};

// DRUM SETTINGS (0.55.0): the beat generator's DENSITY, DOUBLE KICK, GHOST NOTES (50%: the style's
// own), FILLS (off, every 2 / 4 / 8 bars), FILL LENGTH (1 or 2 beats), CYMBAL (the style's, hats,
// ride, china), LOCK KICK TO RIFF, DEFAULTS.
class DrumPanel final : public juce::Component
{
public:
    explicit DrumPanel (pr7s::DrumSettings);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setSettings (const pr7s::DrumSettings&);   // (as the controls would: onChange called)
    const pr7s::DrumSettings& getSettings() const noexcept { return settings; }
    std::function<void (const pr7s::DrumSettings&)> onChange;
    static juce::String summary (const pr7s::DrumSettings&);   // "DRUMS: FILL EVERY 4"
    static constexpr int width = 480, rowHeight = 32, titleHeight = 60;

private:
    void refresh();
    pr7s::DrumSettings settings;
    juce::Slider density, doubleKick, ghosts;
    juce::TextButton fillsOff { "OFF" }, fills2 { "2 BARS" }, fills4 { "4 BARS" }, fills8 { "8 BARS" };
    juce::TextButton oneBeat { "1 BEAT" }, twoBeats { "2 BEATS" };
    juce::TextButton cymAuto { "STYLE'S" }, cymHats { "HATS" }, cymRide { "RIDE" }, cymChina { "CHINA" };
    juce::TextButton lock { "LOCK KICK TO RIFF" }, defaults { "DEFAULTS" };
};


// BUILD SECTION (0.57.0, the owner asked): a whole section onto tracks in one go. Which section
// (its drums: the generated beat for it, from the learned drummer or the style's rules), which parts
// (the drums, the bass locked to their kick, the guitar riff last played or used, the lead, piano
// and melody cards last played or used), then BUILD. After the first, NEXT suggests what follows
// (engine/SectionPlan.h) as the chord suggestions do: each with why and how it relates to what's
// built, built at once after the last section with one click.
struct BuildChoice
{
    enum Part { drums, bass, guitar, lead, piano, melody, count };
    pr7s::DrumSection section = pr7s::DrumSection::verse;
    std::array<bool, count> parts { true, true, true, true, true, false };
    juce::String toString() const;                   // "verse,111110"
    static BuildChoice fromString (const juce::String&);
    bool operator== (const BuildChoice&) const = default;
    static juce::String partName (Part);              // "DRUMS"
};

class BuildSectionPanel final : public juce::Component
{
public:
    // `describe`: what a part will use; `suggestions`: what may come next (empty before the first).
    BuildSectionPanel (BuildChoice, std::function<juce::String (BuildChoice::Part)> describe,
                       std::vector<pr7s::SectionSuggestion> suggestions);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setChoice (const BuildChoice&);   // (as the controls would: onChange called)
    const BuildChoice& getChoice() const noexcept { return choice; }
    std::function<void (const BuildChoice&)> onChange;
    std::function<void()> onBuild;
    std::function<void (const pr7s::SectionSuggestion&)> onBuildNext;
    juce::TextButton& getBuildKey() noexcept { return build; }
    int getNumNextKeys() const noexcept { return nextKeys.size(); }
    juce::TextButton* getNextKey (int i) const { return nextKeys[i]; }
    static constexpr int width = 640, rowHeight = 32, titleHeight = 62, nextHeight = 46;

private:
    void refresh();
    BuildChoice choice;
    std::function<juce::String (BuildChoice::Part)> describe;
    std::vector<pr7s::SectionSuggestion> suggestions;
    juce::OwnedArray<juce::TextButton> sectionKeys, partKeys, nextKeys;
    juce::TextButton build { "BUILD" };
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

    enum class Tab { piano, guitar, bass, patterns, melody, drums };   // (0.39.0: PATTERNS; 0.41.0: MELODY; 0.54.0: DRUMS)
    void setPatternGroup (const juce::String&);   // the PATTERNS tab's group (settings key "ideasPatternGroup")
    juce::String getPatternGroup() const noexcept { return patternGroup; }
    static std::vector<pr7s::LineIdea::Part> partsOf (Tab);
    static juce::String titleOf (pr7s::LineIdea::Part);
    void setTab (Tab);   // (remembered: settings key "ideasTab")
    Tab getTab() const noexcept { return tab; }
    void setTuning (const juce::String&);   // the guitar ideas' (Studio::setGuitarTuning), dealt again
    const pr7s::GuitarTuning& getTuning() const;
    void setGuitarVelocities (const pr7s::GuitarVelocities&);   // (Studio::setGuitarVelocities), guitar ideas dealt again
    void setGuitarStyle (pr7s::GuitarStyle);                   // STYLE (0.53.0; Studio::setGuitarStyle), dealt again
    pr7s::GuitarStyle getGuitarStyle() const;
    void setRiffSettings (const pr7s::RiffSettings&);          // RIFF SETTINGS (Studio's), dealt again
    void showRiffSettings();
    // DRUMS (0.54.0): the library's grooves six at a time (MORE GROOVES: the next six, round again).
    void moreGrooves();
    // (0.55.0) The generated column: the beat generator's six (one per section), or the written beats.
    void setDrumSettings (const pr7s::DrumSettings&);
    void showDrumSettings();
    void setWrittenBeats (bool);   // (settings key "ideasWrittenBeats")
    bool isShowingWrittenBeats() const noexcept { return writtenBeats; }
    // (0.56.0) DRUMMER: the generated column played from a learned drummer's profile ("": the
    // style's rules; Studio::setIdeasDrummer), the LEARN A DRUMMER window (a profile to put right, or none).
    void setDrummer (const juce::String& name);
    juce::String getDrummer() const { return drummer.has_value() ? drummer->name : juce::String(); }
    void showDrummerMenu();
    // (0.57.0) BUILD SECTION.
    void setBuildChoice (const BuildChoice&);   // (settings key "ideasBuild")
    const BuildChoice& getBuildChoice() const noexcept { return buildChoice; }
    std::vector<pr7s::LineIdea> sectionIdeas (const BuildChoice&) const;   // what BUILD writes, in part order
    juce::String describeBuildPart (BuildChoice::Part) const;
    std::vector<int> buildSection();            // the current choice over these chords: the tracks written
    std::vector<pr7s::SectionSuggestion> getNextSuggestions() const;
    std::vector<int> buildNext (const pr7s::SectionSuggestion&);   // after the last built section
    void showBuildSection();
    int getChosen (pr7s::LineIdea::Part p) const noexcept { return chosen[(size_t) p]; }
    const std::vector<pr7s::ChordSpan>& getChords() const noexcept { return spans; }
    void openLearnWindow (const pr7s::DrumProfile* toEdit);
    class DrumLearnWindow* getLearnWindow() const noexcept { return learnWindow.get(); }
    int getNumLibraryGrooves() const noexcept { return (int) libraryGrooves.size(); }
    int getGroovePage() const noexcept { return groovePage; }
    // The guitar riff the drums' and bass's locked ideas follow, and the beat the bass's kick-locked
    // one follows: the rhythm / drums card last played or used (else the first).
    int getLockedRiff() const noexcept { return riffIndex; }
    int getLockedBeat() const noexcept { return beatIndex; }
    void showVelocities();                                     // the panel in a call-out under VELOCITIES
    void setHumanize (float amount, bool feel);                // (Studio's ideas humanize), every tab dealt again
    void showHumanize();
    static juce::String humanizeText (float amount);           // "HUMANIZE 40%" / "HUMANIZE OFF"
    void setMelodySettings (const pr7s::MelodySettings&);      // (Studio's), the MELODY tab dealt again
    void showMelodySettings();

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
    std::array<std::vector<pr7s::LineIdea>, 9> ideas;
    juce::String patternGroup { "ARPEGGIOS" };
    juce::ComboBox groupBox;
    int variation = 0;
    Tab tab = Tab::piano;
    std::optional<std::pair<pr7s::LineIdea::Part, int>> playing;
    bool playingChords = false;
    juce::OwnedArray<Card> cards;
    juce::TextButton hearChords { "HEAR CHORDS" }, more { "MORE IDEAS" }, stopKey { "STOP" };
    juce::TextButton pianoTab { "PIANO" }, guitarTab { "GUITAR" }, bassTab { "BASS" }, patternsTab { "PATTERNS" }, melodyTab { "MELODY" }, drumsTab { "DRUMS" };
    juce::TextButton melodyKey;   // MELODY SETTINGS (the MELODY tab)
    juce::Label tuningLabel { {}, "TUNING" };
    juce::ComboBox tuningBox;
    juce::TextButton velocitiesKey { "VELOCITIES" };
    juce::ComboBox styleBox;     // STYLE (the guitar tab)
    juce::TextButton riffKey;    // RIFF SETTINGS (the guitar tab)
    juce::TextButton groovesKey { "MORE GROOVES" };   // (the drums tab)
    juce::TextButton drumKey;                           // DRUM SETTINGS (the drums tab)
    juce::TextButton writtenKey { "WRITTEN BEATS" };    // (the drums tab)
    bool writtenBeats = false;
    juce::TextButton drummerKey;                        // DRUMMER (the drums tab's generated column)
    juce::TextButton buildKey { "BUILD SECTION" };
    BuildChoice buildChoice;
    std::array<int, 9> chosen {};                       // the card last played or used, per part
    std::optional<pr7s::DrumProfile> drummer;
    std::unique_ptr<class DrumLearnWindow> learnWindow;
    std::vector<pr7s::LibraryGroove> libraryGrooves;
    bool groovesLooked = false;
    int groovePage = 0, riffIndex = 0, beatIndex = 0;
    void lookForGrooves();
    void noteChosen (pr7s::LineIdea::Part, int index);
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
