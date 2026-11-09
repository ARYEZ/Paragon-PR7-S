#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/Studio.h"

// (0.78.0, the owner asked) The BOOSTER: PR7-C's OUTPUT TRIM module in the Inspector, right
// under EQUALIZERS. A TRIM knob (-12 to +12 dB, 0.1 dB steps) above PR7-C's chrome channel
// fader (+10 dB down to silence at the bottom; the scale 10 5 0 -5 -10 -20 -30 -40 -inf, even
// spacing between marks as PR7-C's). The gain is the two added, after the channel's EQ and
// before its sends and fader. Double-click either for 0 dB; Shift-drag the fader: fine.
class BoosterView final : public juce::Component,
                          private juce::Timer
{
public:
    explicit BoosterView (pr7s::Studio&);
    void setBus (int busId);
    int getBus() const noexcept { return bus; }

    static float faderProportion (float db);   // PR7-C's law: marks evenly spaced, linear between
    static float faderDb (float proportion);
    static juce::String dbText (float db, bool trim);

    juce::Slider& getTrim() noexcept { return trim; }
    juce::Slider& getFader() noexcept { return fader; }
    void refresh();
    int getPreferredHeight (int width) const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { refresh(); }
    void send (bool undoable);
    pr7s::Booster current() const;

    pr7s::Studio& studio;
    int bus = 0;
    bool dragging = false;
    juce::Slider trim { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Slider fader { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Rectangle<int> trimArea, trimReadout, faderArea, faderReadout, scaleArea;
};
