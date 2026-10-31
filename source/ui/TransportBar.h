#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"
#include "ui/LevelMeter.h"

// The transport: a pearl PR7 unit across the top of the studio. Its keys
// (back to the start, STOP, PLAY, RECORD, CYCLE, CLICK) light as on the
// floor units, and its LED screens show where the song is (bars and beats,
// and minutes and seconds), the tempo (drag it up or down, or double-click
// to type one) and the time signature (click for the list). The output's
// level on the right. CPU, a small screen between the keys and the position:
// the audio processing load (the manual's Audio Performance meter: the
// average and the peak, how much of each block's time the processing takes;
// OVER lights for an overload, a dropout, and stays lit until clicked).
// AUTO-SCROLL's key (the main window's) before the keys, by HOME.
//
// On a wide window (spanning screens: `setToolsWidth`) the position screen
// stops growing; the keys on its left, the tempo, the signature and room for
// the arrangement's tools (`getToolsArea`, which the main window puts there)
// on its right, all of them together in the middle (on three screens, all
// on the middle one).
class TransportBar final : public juce::Component,
                           public juce::TooltipClient,
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

    // The wide layout (0: the usual one). The tools' place, in this bar's coordinates.
    void setToolsWidth (int);
    juce::Rectangle<int> getToolsArea() const noexcept { return toolsArea; }
    juce::Rectangle<float> getPositionScreen() const noexcept { return positionScreen; }
    static constexpr int widePositionWidth = 440;

    // CPU: what the meter shows (the load, 0 to 1 and over), and whether its overload light is lit.
    float getShownAverage() const noexcept { return shownAverage; }
    float getShownPeak() const noexcept { return shownPeak; }
    bool isOverloadLit() const noexcept { return overloadLit; }
    juce::Rectangle<float> getCpuArea() const noexcept { return cpuScreen; }
    // Where the arrangement's AUTO-SCROLL key and its menu arrow go (the main window puts them there).
    juce::Rectangle<int> getAutoScrollArea() const noexcept { return autoScrollArea; }
    juce::String getTooltip() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refreshKeys();

    pr7s::Studio& studio;
    juce::TextButton toStart { "|<" }, stopKey { "STOP" }, playKey { "PLAY" }, recordKey { "REC" },
                     cycleKey { "CYCLE" }, clickKey { "CLICK" };
    LevelMeter leftMeter, rightMeter;
    juce::Rectangle<float> badge, positionScreen, tempoScreen, signatureScreen, keysArea, meterArea, cpuScreen, autoScrollStamp;
    juce::Rectangle<int> toolsArea, autoScrollArea;
    float keyPitchNow = 70.0f;
    int toolsWidth = 0;
    float shownAverage = 0.0f, shownPeak = 0.0f;
    bool overloadLit = false;
    void followPerformance();
    mutable std::vector<juce::Rectangle<float>> glasses;
    juce::Image satin;
    double dragStartBpm = 120.0;
    bool draggingTempo = false;
};
