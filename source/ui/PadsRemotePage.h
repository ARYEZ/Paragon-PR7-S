#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// REMOTE on the chord pads (0.44.0; the manual's Chord Pads Remote Settings dialog, its Pad and
// Player Remote Control tabs): the keys on your MIDI keyboard that play the pads (a range: MIDI
// Learn takes its first and last key), Latch Chords, the chord modifiers (next/previous voicing,
// more/fewer tensions, transpose up/down: on with ACTIVATE), the five section keys and five
// subsection keys (Sections mode). Click a key's box, then press the key on your keyboard.
class PadsRemotePage final : public juce::Component,
                             private juce::Timer
{
public:
    explicit PadsRemotePage (pr7s::Studio&);
    ~PadsRemotePage() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    void learn (pr7s::Studio::PadLearn, int index = 0);   // (what a box's click does)
    juce::String getKeyText (pr7s::Studio::PadLearn, int index) const;   // what a box shows
    static constexpr int width = 640, height = 430;

private:
    void timerCallback() override;
    void refresh();
    pr7s::Studio& studio;
    juce::TextButton rangeKey, latchKey { "LATCH CHORDS" }, sectionsKey { "SECTIONS MODE" }, modifiersKey { "ACTIVATE" }, cancelKey { "CANCEL LEARN" };
    juce::OwnedArray<juce::TextButton> sectionKeys, subsectionKeys, modifierKeys;
};
