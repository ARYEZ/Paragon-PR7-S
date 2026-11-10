#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// The Edit History window (0.83.0, the manual's Edit History Dialog): every undo step, oldest at
// the top, with ACTION, TIME, STATE and DETAILS. The separator (the bar under the step the song is
// at) is dragged up to undo back to any step and down to redo; the song follows as it moves. The
// steps under it are undone (dimmed) until a new edit replaces them. Double-click DETAILS to type
// a note. Our additions: a click on a row's left edge puts the separator under that row, Up/Down
// move it a step, and MAX STEPS (the manual's Maximum Undo Steps, in its Preferences).
class HistoryPage final : public juce::Component,
                          private juce::Timer
{
public:
    explicit HistoryPage (pr7s::Studio&);
    ~HistoryPage() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // (Tests.) Row i is step i; the separator sits under row `getSeparatorRow()`.
    int getNumRows() const;
    int getSeparatorRow() const;
    juce::String getCellText (int row, int column) const;   // 0 action, 1 time, 2 state, 3 details
    juce::Rectangle<int> getSeparatorBounds() const;          // in this page
    juce::Rectangle<int> getRowBounds (int row) const;        // in this page
    void moveSeparatorTo (int row);                           // as a drag lets go there
    void setDetails (int row, const juce::String& text);
    void setMaxSteps (int steps);
    juce::ComboBox& getMaxStepsBox() noexcept { return maxSteps; }
    void refresh();
    std::function<bool (const juce::KeyPress&)> onOtherKey;   // keys it doesn't use: the studio's
    static constexpr int preferredWidth = 760, preferredHeight = 560, rowHeight = 26, separatorHeight = 8;

private:
    struct List;
    void timerCallback() override;
    void beginEdit (int row);

    pr7s::Studio& studio;
    std::unique_ptr<List> list;
    juce::Viewport viewport;
    juce::Label maxLabel { {}, "MAX STEPS" };
    juce::ComboBox maxSteps;
    std::unique_ptr<juce::TextEditor> editor;
    int editRow = -1;
    juce::uint64 seenChange = 0;
    int seenRows = -1, seenPosition = -2;
};

class HistoryWindow final : public juce::DocumentWindow
{
public:
    explicit HistoryWindow (pr7s::Studio&);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    HistoryPage& getPage() { return *page; }

private:
    HistoryPage* page = nullptr;
};
