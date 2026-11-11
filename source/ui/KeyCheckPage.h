#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// KEY CHECK (0.84.0, the owner asked): every note on the song's pitched tracks and every chord on the chord track
// that's outside the key, listed by time. The key: AUTOMATIC (the chord track's keys where it has them, else the song's
// key guessed from its notes) or one chosen. A click on a row shows the note in the MIDI editor (`onShowIssue`); FIX
// moves a note to the nearest note of the key, FIX ALL NOTES every one listed (one undo step each time). Chords are
// listed but changed on the chord track. Checked again whenever the song changes.
class KeyCheckPage final : public juce::Component,
                           private juce::ListBoxModel,
                           private juce::Timer
{
public:
    explicit KeyCheckPage (pr7s::Studio&);
    ~KeyCheckPage() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    void setKey (std::optional<pr7s::Key>);       // none: AUTOMATIC
    std::optional<pr7s::Key> getChosenKey() const noexcept { return chosen; }
    void check();                                  // again now
    const std::vector<pr7s::KeyIssue>& getIssues() const noexcept { return issues; }
    bool fix (int row);                            // a note's row moved to its suggestion
    bool fixAll();
    juce::String getRowText (int row) const;       // "BAR 5.1.1.0  BASS  C#1 -> C1  (A MINOR)" for tests
    juce::String getSummary() const;
    std::function<void (const pr7s::KeyIssue&)> onShowIssue;
    std::function<void()> onKeyChange;   // (MainView: the MIDI editor's rings follow)   // (MainView: the part in the MIDI editor, the note selected)
    juce::Button& getFixAllKey() noexcept { return fixAllKey; }
    juce::ComboBox& getKeyBox() noexcept { return keyBox; }

    static constexpr int preferredWidth = 900, preferredHeight = 640, headerHeight = 96, rowHeight = 34, gap = 12;

private:
    int getNumRows() override { return (int) issues.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void timerCallback() override;
    juce::Rectangle<int> fixArea (int width) const { return { width - 86, 5, 76, rowHeight - 10 }; }

    pr7s::Studio& studio;
    std::optional<pr7s::Key> chosen;
    std::vector<pr7s::KeyIssue> issues;
    juce::ListBox list;
    juce::ComboBox keyBox;
    juce::TextButton fixAllKey { "FIX ALL NOTES" }, checkKey { "CHECK AGAIN" };
    juce::uint64 seenVersion = 0;
    juce::String keyText;   // what the automatic key is
};

class KeyCheckWindow final : public juce::DocumentWindow
{
public:
    explicit KeyCheckWindow (pr7s::Studio&);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    KeyCheckPage& getPage() { return *page; }

private:
    KeyCheckPage* page = nullptr;
};
