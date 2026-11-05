#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// The Audio Alignment panel (0.33.0, the manual's Audio Alignment Panel; the part's and the range's
// menus, the command Window.AudioAlignment).
//
//   SELECTION  SET REFERENCE (the selected event, or the range on its first audio track), ADD TARGETS
//              (the selected events on other tracks, or the range's events on them), CLEAR; with
//              several targets, which one is shown.
//   The view   The reference above, the shown target below, the overlap lit. Drag from the reference
//              down to the target: a sync point (that moment of the target lands there; red when it
//              can't). Ctrl-drag on the target: a protected range (moved, never stretched). Shift-click
//              one: gone.
//   SETTINGS   MATCH WORDS, PREFER TIME SHIFTING (the manual's); PRECISION (the manual's Alignment
//              Precision); ONLY WHERE BOTH PLAY, FLEXIBILITY, MAX SHIFT (ours, after a dedicated
//              alignment tool's). Kept in the settings file.
//   ALIGN AUDIO, and what it did.
class AudioAlignmentPanel final : public juce::Component, private juce::ChangeListener
{
public:
    // Where the selection comes from (the arrangement's selected parts and range).
    AudioAlignmentPanel (pr7s::Studio&, std::function<std::vector<int>()> selectedParts,
                         std::function<std::optional<pr7s::Studio::Range>()> range);
    ~AudioAlignmentPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // The panel's state (and what its keys do; the tests drive these).
    bool setReferenceFromSelection();
    int addTargetsFromSelection();   // how many were added
    void clearTargets();
    void showTarget (int index);
    bool alignNow();
    void addSyncPoint (double referenceBeat, double targetBeat);   // on the shown target
    void addProtectedRange (juce::Range<double> beats);
    void removeAt (juce::Point<float>);   // the sync point or protected range under a point of the view
    void setSettings (const pr7s::AlignSettings&);

    const std::optional<pr7s::Studio::AlignReference>& getReference() const noexcept { return reference; }
    const std::vector<pr7s::Studio::AlignTarget>& getTargets() const noexcept { return targets; }
    const std::vector<pr7s::Studio::AlignSync>& getSyncPoints() const noexcept { return syncs; }
    const std::vector<juce::Range<double>>& getProtectedRanges() const noexcept { return protectedRanges; }
    const pr7s::Studio::AlignReport& getReport() const noexcept { return report; }
    juce::String getReportText() const;
    juce::TextButton* keyNamed (const juce::String&);
    juce::Component& getView() noexcept;
    // The view's mapping (for the tests and the mouse).
    float xForBeat (double beat) const;
    double beatForX (float x) const;

    static constexpr int preferredWidth = 1100, preferredHeight = 640;

    class View;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refresh();       // keys lit as the settings are, the target box, the view's sounds again
    juce::TextButton& makeKey (const juce::String& text, std::function<void()> onClick, const juce::String& tip = {});
    juce::String describe (int trackId, double start, double end) const;
    std::optional<std::pair<double, double>> shownSpan() const;   // beats the view covers

    pr7s::Studio& studio;
    std::function<std::vector<int>()> selectedParts;
    std::function<std::optional<pr7s::Studio::Range>()> rangeOf;
    std::optional<pr7s::Studio::AlignReference> reference;
    std::vector<pr7s::Studio::AlignTarget> targets;
    std::vector<pr7s::Studio::AlignSync> syncs;
    std::vector<juce::Range<double>> protectedRanges;
    pr7s::Studio::AlignReport report;
    bool reported = false;
    int shown = 0;

    std::vector<std::unique_ptr<juce::TextButton>> keys;
    juce::TextButton* referenceKey = nullptr, *targetsKey = nullptr, *clearKey = nullptr, *alignKey = nullptr;
    juce::TextButton* wordsKey = nullptr, *shiftKey = nullptr, *bothKey = nullptr, *clearMarksKey = nullptr;
    std::array<juce::TextButton*, 3> flexKeys {};
    juce::ComboBox targetBox;
    juce::Slider precision, maxShift;
    juce::Label referenceText, targetText, precisionLabel, maxShiftLabel, flexLabel, reportText, hint;
    std::unique_ptr<View> view;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> headings;
};

class AudioAlignmentWindow final : public juce::DocumentWindow
{
public:
    AudioAlignmentWindow (pr7s::Studio&, std::function<std::vector<int>()> selectedParts,
                          std::function<std::optional<pr7s::Studio::Range>()> range);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    AudioAlignmentPanel& getPanel() { return *panel; }

private:
    AudioAlignmentPanel* panel = nullptr;
};
