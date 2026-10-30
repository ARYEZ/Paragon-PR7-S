#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"
#include "ui/LevelMeter.h"

// The transport: a pearl PR7 unit across the top of the studio. Its keys
// (back to the start, STOP, PLAY, RECORD, CYCLE, CLICK) light as on the
// floor units, and its LED screens show where the song is (bars and beats,
// and minutes and seconds), the tempo (drag it up or down, or double-click
// to type one) and the time signature (click for the list). The output's
// level on the right.
class TransportBar final : public juce::Component,
                           private juce::ChangeListener,
                           private juce::Timer
{
public:
    explicit TransportBar (pr7s::Studio&);
    ~TransportBar() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    // What the screens show now (for tests).
    juce::String getPositionText() const;
    juce::String getTempoText() const;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refreshKeys();

    pr7s::Studio& studio;
    juce::TextButton toStart { "|<" }, stopKey { "STOP" }, playKey { "PLAY" }, recordKey { "REC" },
                     cycleKey { "CYCLE" }, clickKey { "CLICK" };
    LevelMeter leftMeter, rightMeter;
    juce::Rectangle<float> badge, positionScreen, tempoScreen, signatureScreen, keysArea, meterArea;
    mutable std::vector<juce::Rectangle<float>> glasses;
    juce::Image satin;
    double dragStartBpm = 120.0;
    bool draggingTempo = false;
};
