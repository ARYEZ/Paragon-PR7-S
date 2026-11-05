#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// PROGRESSIONS (0.40.0, the owner asked: a library by genre and mood, as their chord tool's).
// Our own library (engine/Progressions.h), in the key chosen at the top (at first the chord
// track's at the playhead, else the pads'): the genres and moods at the left (each with how many
// it holds), a search, FAVORITES. Each row: a star, the name with its genre and moods, its
// chords spelled in the key (numerals under them, each as wide as its bars), and keys:
//   HEAR      plays it at the song's tempo (the chord lit as it sounds); again: stops
//   TO TRACK  onto the chord track from the playhead's bar (one undo step)
//   TO PADS   onto the pads from the first (locked pads skipped)
//   IDEAS     onto the chord track, then IDEAS for those chords
// A row can also be dragged onto the chord track's lane (from the bar dropped on); a click on
// the chords hears it, a double-click puts it on the chord track; a right-click has all of it.
// The other mode's progressions (a major one in a minor key) are played on the relative key,
// so every chord stays in the key's notes; KEY'S MODE ONLY hides them.
class ProgressionsPage final : public juce::Component,
                               public juce::DragAndDropContainer,   // (rows dragged onto the chord track)
                               private juce::Timer
{
public:
    explicit ProgressionsPage (pr7s::Studio&);
    ~ProgressionsPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // What the controls do (tests drive them).
    void setKey (const pr7s::Key&);
    const pr7s::Key& getKey() const noexcept { return key; }
    void setGenre (const juce::String&);   // "": all
    void setMood (const juce::String&);
    void setSearch (const juce::String&);
    void setOwnModeOnly (bool);
    void setFavoritesOnly (bool);
    void toggleFavorite (int libraryIndex);
    const std::vector<int>& getShown() const noexcept { return shown; }   // library indices, in order
    std::vector<pr7s::ProgressionStep> stepsOf (int libraryIndex) const;
    void hear (int libraryIndex);          // (again while it plays: stop)
    void stop();
    int getHearing() const noexcept { return hearing; }   // -1: none
    int getLitStep() const noexcept { return litStep; }
    void toChordTrack (int libraryIndex);  // from the playhead's bar
    int toPads (int libraryIndex);         // how many pads were filled
    void toIdeas (int libraryIndex);       // onto the chord track, then onIdeas with those events
    std::function<void (std::vector<int> chordEvents)> onIdeas;

    // The genre and mood lists at the left: "" (ALL) first; counts in the current key and search.
    juce::StringArray getGenreItems() const;
    juce::StringArray getMoodItems() const;
    int countFor (const juce::String& genre, const juce::String& mood) const;

    class Rows;
    class Filters;
    static constexpr int headerHeight = 96, filtersWidth = 230, rowHeight = 58, gap = 12;

private:
    void timerCallback() override;
    void refresh();
    pr7s::ProgressionFilter filterFor (const juce::String& genre, const juce::String& mood) const;
    pr7s::Studio& studio;
    pr7s::Key key;
    juce::String genre, mood;
    bool ownModeOnly = false, favoritesOnly = false;
    std::vector<int> shown;
    int hearing = -1, litStep = -1;
    double hearingStart = 0.0;
    juce::ComboBox keyBox;
    juce::TextEditor search;
    juce::TextButton ownModeKey { "KEY'S MODE ONLY" }, favoritesKey { "FAVORITES" }, stopKey { "STOP" }, followKey { "FROM THE SONG" };
    std::unique_ptr<Filters> filters;
    std::unique_ptr<Rows> rows;
    juce::Viewport rowsView, filtersView;
};

class ProgressionsWindow final : public juce::DocumentWindow
{
public:
    explicit ProgressionsWindow (pr7s::Studio&);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    ProgressionsPage& getPage() { return *page; }

private:
    ProgressionsPage* page = nullptr;
};
