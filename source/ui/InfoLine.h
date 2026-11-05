#pragma once

#include "ui/InfoEntry.h"
#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

class ArrangeView;

// The info line (0.37.0, the owner asked: as the other DAW's, the manual's "Info Line" and its
// "Value Editing Rules on the Info Line"): a strip over the track list and the arrangement showing
// the selected automation points as the first of them: TYPE (the lane), START, VALUE, TERMINATOR.
// Several selected: shown in pink with how many. Double-click START or VALUE to type: every
// selected point moves by what the first one moves, in the control's own units (four points at
// -5, -1, 0 and +1 dB, the first typed as 0 dB: 0, +4, +5, +6). Hold Ctrl when pressing Enter to
// set them all to the value typed instead (the manual's modifier). TERMINATOR's click switches them.
//
// (0.47.0) With no points selected: the selected parts and audio events, the earliest first
// (the one shown). An audio event: FILE, NAME, START, END, LENGTH, OFFSET, FADE-IN, FADE-OUT,
// VOLUME, MUTE; a MIDI part: NAME, START, END, LENGTH, TRANSPOSE, VELOCITY, MUTE. The same
// rules (relative, Ctrl+Enter absolute; NAME names them all, MUTE's click switches them all).
class InfoLine final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    InfoLine (pr7s::Studio&, ArrangeView&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    static constexpr int height = 40;

    // What's shown (for tests: the field's text, "" when nothing is selected).
    juce::String getField (const juce::String& name) const;   // points: "TYPE", "START", "VALUE", "TERMINATOR"; parts: see above
    // A field typed (as Enter on it does); `absolute`: as with Ctrl held. False: not a value it takes.
    bool enter (const juce::String& field, const juce::String& text, bool absolute);
    void toggleTerminator();
    void toggleMute();                 // (the parts' MUTE)
    bool isShowingParts() const noexcept { return showingParts; }
    std::vector<int> partsInOrder() const;   // the selected parts, the shown one first
    void refresh();
    void beginEdit (const juce::String& field);   // (a double-click's; tests type into `getEditor`)
    juce::TextEditor* getEditor() noexcept { return editor.get(); }
    std::function<void()> onHide;   // right-click: Hide Info Line

private:
    void timerCallback() override;
    struct Field
    {
        juce::String name, value;
        juce::Rectangle<int> area;
        bool editable = false;
    };
    pr7s::Studio& studio;
    ArrangeView& arrange;
    std::vector<Field> fields;
    int count = 0;
    bool showingParts = false;
    juce::String shownKey;
    // (0.57.1) What the last refresh was made from (skipped when all is the same).
    juce::uint64 seenChange = 0;
    std::vector<int> seenParts, seenPoints;
    int seenBus = -1, seenLane = -2;
    bool fieldsDirty = true;
    void refreshParts();
    bool enterPart (const juce::String& field, const juce::String& text, bool absolute);
    std::unique_ptr<juce::TextEditor> editor;
    juce::String editing;
    void mouseDoubleClick (const juce::MouseEvent&) override;
};
