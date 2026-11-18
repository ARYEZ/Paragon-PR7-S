#pragma once

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"
#include "ui/LevelMeter.h"
#include "ui/Pearl.h"

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
    float getShownOutDb() const noexcept { return shownOutDb; }   // (0.73.1) the number OUT shows
    void followOutputForTests() { followOutput(); }
    void resetOutPeak();   // (0.73.2) a click on OUT: a new window starts now (0.131.0: shown at once)
    // (0.131.0, the owner asked) OUT shows the level going out now, as the channel meters do, but as a
    // number refreshed once every `outWindowTicks` timer ticks (5 s at 30 Hz): the loudest peak of those 5 s.
    static constexpr int outWindowTicks = 150;
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
    // (0.73.2, the owner asked) OUT's number: the loudest peak (both sides) since OUT was last clicked.
    float outPeak = 0.0f;   // gain: the loudest in this window
    int outTicks = 0;   // (ticks into it)
    float shownOutDb = LevelMeter::minDb;
    void followOutput();
    void showOutPeak();
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
    Pearl::UnitCache unitCache;   // (0.83.1)
    // (0.174.0) Each screen's frame (its shadow, bezel and empty glass) and the reflection drawn over its
    // glass, kept as pictures at the screen's pixel scale: the POSITION and TIME screens drew both again
    // 30 times a second while playing (about 0.8 ms a frame). Index: the screen's place in `screenCaches`.
public:
    struct ScreenPicture
    {
        juce::Image image;
        juce::Rectangle<float> screen, area;
        float scale = 0.0f, size = 0.0f;
    };
private:
    mutable std::array<ScreenPicture, 6> screenCaches, glassCaches;
    mutable std::vector<std::pair<int, juce::Rectangle<float>>> glassIndex;   // (each glass's screen)
    juce::Rectangle<float> paintScreenCached (juce::Graphics&, int index, juce::Rectangle<float> screen) const;
    void paintGlassCached (juce::Graphics&, int index, juce::Rectangle<float> glass) const;
    int screenPicturesMade = 0;
public:
    int getScreenPicturesMade() const noexcept { return screenPicturesMade; }
private:
    double dragStartBpm = 120.0;
    bool draggingTempo = false;
};
