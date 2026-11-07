#pragma once

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"
#include "ui/LevelMeter.h"

// The transport: a pearl PR7 unit across the top of the studio. Its keys
// (back to the start, STOP, PLAY, RECORD, CYCLE, CLICK) light as on the
// floor units, and its LED screens show where the song is (POSITION: bars
// and beats; TIME: minutes and seconds; two screens alike since 0.9.10), the tempo (drag it up or down, or double-click
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
    std::function<void()> onOpenTempoEditor;   // the tempo screen's menu (0.13.0)
    std::function<void()> onInsertSignature;   // (0.45.0) the signature screen's menu: the Insert Signature box
    std::function<std::optional<double>()> getSelectionStart;   // (0.45.0) for tap tempo's modes: what's selected
    // TAP (0.45.0, the manual's Tap Tempo): a tap now (the key, Shift+Space); right-click: the modes.
    void tap();
    void showTapModes (juce::Component* target = nullptr);   // (0.73.0: TAP's key is in the header now)
    bool isShowingTap() const;
    explicit TransportBar (pr7s::Studio&);
    ~TransportBar() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void showRecordModes();   // (0.29.0: REC's right-click) the manual's Audio, MIDI and MIDI Cycle Record Modes
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    // What the screens show now (for tests).
    static juce::String screenPosition (const juce::String& text, int numerator);   // padded for the screen: always as wide
    juce::String getPositionText() const;
    juce::String getTempoText() const;

    // The wide layout (0: the usual one). The tools' place, in this bar's coordinates.
    void setToolsWidth (int);
    juce::Rectangle<int> getToolsArea() const noexcept { return toolsArea; }
    juce::Rectangle<float> getPositionScreen() const noexcept { return positionScreen; }
    juce::Rectangle<float> getTimeScreen() const noexcept { return timeScreen; }
    static constexpr int widePositionWidth = 440;

    // CPU: what the meter shows (the load, 0 to 1 and over), and whether its overload light is lit.
    float getShownAverage() const noexcept { return shownAverage; }
    float getShownPeak() const noexcept { return shownPeak; }
    bool isOverloadLit() const noexcept { return overloadLit; }
    juce::Rectangle<float> getCpuArea() const noexcept { return cpuScreen; }
    juce::Rectangle<float> getOutArea() const noexcept { return outScreen; }   // (0.73.0)
    static constexpr int outRefreshTicks = 8, outWindows = 4;      // (0.73.1: see followOutput)
    float getShownOutDb() const noexcept { return shownOutDb; }   // (0.73.1) the number OUT shows
    void followOutputForTests() { followOutput(); }
    // Where the arrangement's AUTO-SCROLL key and its menu arrow go (the main window puts them there).
    juce::Rectangle<int> getAutoScrollArea() const noexcept { return autoScrollArea; }
    juce::String getTooltip() override;

private:
    void drawFixedPitch (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float height);
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refreshKeys();

    pr7s::Studio& studio;

    juce::uint64 seenChange = 0;   // (0.34.3) the studio's change version last acted on
    // REC: a right-click opens the record modes (0.29.0), so it notes which button clicked it.
    struct RecordKey final : juce::TextButton
    {
        using juce::TextButton::TextButton;
        bool popup = false;
        void clicked (const juce::ModifierKeys& mods) override { popup = mods.isPopupMenu(); }
    };
    juce::TextButton toStart { "|<" }, stopKey { "STOP" }, playKey { "PLAY" };
    RecordKey recordKey { "REC" };
    juce::TextButton cycleKey { "CYCLE" }, clickKey { "CLICK" };
    pr7s::SignatureEvent shownSignature { -1, 0, 0 };
    // (0.73.1) OUT's number: the loudest peak (both sides) of the last second, shown afresh
    // `outRefreshTicks` timer ticks apart (about four times a second), not every tick.
    float outWindowPeak = 0.0f;                     // this window's loudest so far (gain)
    std::array<float, outWindows> outRecent {};     // the last windows' loudest (gain)
    int outTicks = 0, outNext = 0;
    float shownOutDb = LevelMeter::minDb;
    void followOutput();
    juce::Rectangle<float> badge, positionScreen, timeScreen, tempoScreen, signatureScreen, keysArea, cpuScreen, outScreen, autoScrollStamp;
    juce::Rectangle<int> toolsArea, autoScrollArea;
    float keyPitchNow = 70.0f;
    int toolsWidth = 0;
    float shownAverage = 0.0f, shownPeak = 0.0f;
    bool overloadLit = false;
    // What the screens showed when last drawn (0.9.2: redrawn only when it changes, not 30 times a second).
    double shownPosition = -1.0;
    juce::String shownTempo;   // (repainted when it changes: the tempo track while playing)
    int shownCpu = -1;
    void followPerformance();
    mutable std::vector<juce::Rectangle<float>> glasses;
    juce::Image satin;
    double dragStartBpm = 120.0;
    bool draggingTempo = false;
};
