#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

#include <juce_dsp/juce_dsp.h>

// The channel EQ on screen (0.43.0; the manual's Equalizer and Pre sections).
//
// EqCurve: the curve display, as the manual's: the EQ and the Pre's cuts and gain together
// (bypassed parts in grey), each band on as a numbered point. Click an empty spot: the nearest
// band that's off comes on there. Drag a point: frequency and gain (Ctrl: gain only, Alt:
// frequency only, Shift: its Q, up narrower). Double-click a point: that band off. Right-click:
// Invert EQ Settings, Reset, Bypass, presets. The mouse's frequency, note and level at the top.
// One undo step per drag (studio.setChannelEq undoable on release).
// (0.46.0, as the other DAW's Frequency 2) The full one also shows the channel's sound as a
// spectrum behind the curve and a keyboard under it, each band's mark on it (drag it: the band's
// frequency; let go: exactly a key's). The wheel over a point: its Q. Auto Listen: while a point
// is dragged, the channel plays only what that band works on.
class EqCurve final : public juce::Component,
                      public juce::SettableTooltipClient,
                      private juce::Timer
{
public:
    EqCurve (pr7s::Studio&, int busId, bool compact);
    ~EqCurve() override;
    void setBus (int busId);
    void setKeysShown (bool);
    void setSpectrumShown (bool);
    void setAutoListen (bool on) noexcept { autoListen = on; }
    bool areKeysShown() const noexcept { return showKeys; }
    bool isSpectrumShown() const noexcept { return showSpectrum; }
    bool isAutoListening() const noexcept { return autoListen; }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    int getBus() const noexcept { return bus; }

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    // The display's scales (tests use them).
    float xForFrequency (double hz) const;
    double frequencyForX (float x) const;
    float yForDb (double db) const;
    double dbForY (float y) const;
    juce::Rectangle<float> plotArea() const;
    juce::Point<float> bandPoint (int band) const;   // where band's point is drawn
    int bandAt (juce::Point<float>) const;           // -1: none
    juce::Rectangle<float> keysArea() const;         // (0.46.0) the keyboard (empty: none)
    juce::Rectangle<float> keyMark (int band) const; // a band's mark on it
    static double keyFrequency (int midi);           // (middle C = C3 = 60; A3 = 440 Hz)
    void updateSpectrum();                           // (the timer's; tests call it)
    float spectrumLevelAt (double hz) const;         // the shown spectrum's level there (dB; -120: none)
    static constexpr double rangeDb = 24.0;
    static constexpr int keysHeight = 26;

    std::function<void (const juce::MouseEvent&)> onMenu;   // (the editor's right-click menu)

private:
    void timerCallback() override;
    pr7s::ChannelEq current() const;
    void apply (const pr7s::ChannelEq&, bool undoable);
    void listenTo (int band);   // (Auto Listen; -1: stop)
    pr7s::Studio& studio;
    int bus;
    bool compact;
    bool showKeys = true, showSpectrum = true, autoListen = false, tapping = false;
    int dragging = -1, keyDragging = -1;
    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> heard = std::vector<float> ((size_t) fftSize, 0.0f), fftData = std::vector<float> ((size_t) fftSize * 2, 0.0f);
    std::vector<float> levels;   // per bin, dB, falling slowly
    double spectrumRate = 48000.0;
    pr7s::ChannelEq atDown;
    juce::Point<float> hover { -1.0f, -1.0f };
    pr7s::ChannelEq shown;   // (repainted when the studio's differs: undo, the other views)
};

// The whole EQ of a channel: BYPASS, PRESETS, the curve, a column per band (ON, TYPE, GAIN,
// FREQUENCY, Q) and the PRE row (LOW CUT and HIGH CUT, each on, frequency, slope; GAIN; PHASE).
// Frequencies can be typed in Hz, kHz or as a note with cents ("A3", "A5 -23").
class EqEditor final : public juce::Component,
                       private juce::Timer
{
public:
    EqEditor (pr7s::Studio&, int busId);
    ~EqEditor() override;
    void setBus (int busId);
    int getBus() const noexcept { return bus; }
    void paint (juce::Graphics&) override;
    void resized() override;

    // What the controls do (tests drive them; each one undo step).
    void setBandOn (int band, bool);
    void setBandType (int band, pr7s::EqBand::Type);
    void setBandGain (int band, double db);
    void setBandFrequency (int band, double hz);
    void setBandQ (int band, double q);
    void setBandCut (int band, int slope);         // (0.46.0) band 1 a low cut / band 4 a high cut of this slope
    void setBandInvert (int band, bool);           // (0.46.0) Invert Gain
    void setOutput (double db);                    // (0.46.0)
    void setAutoListen (bool);
    void setKeysShown (bool);
    void setSpectrumShown (bool);
    void setBypass (bool);
    void setLowCut (bool on, double hz, int slope);
    void setHighCut (bool on, double hz, int slope);
    void setPreGain (double db);
    void setPhase (bool inverted);
    void invert();
    void reset();
    void showMenu();   // presets, invert, reset
    EqCurve& getCurve() noexcept { return curve; }

    static void showPresetMenu (pr7s::Studio&, int busId, juce::Component* target);
    static juce::String channelName (pr7s::Studio&, int busId);
    static constexpr int width = 960, height = 660;

private:
    struct BandColumn;
    void timerCallback() override;
    void refresh();
    void change (const std::function<void (pr7s::ChannelEq&)>&, bool undoable = true);
    pr7s::Studio& studio;
    int bus;
    EqCurve curve;
    juce::TextButton bypassKey { "BYPASS" }, presetsKey { "PRESETS" }, lowCutKey { "LOW CUT" }, highCutKey { "HIGH CUT" },
                     lowSlopeKey, highSlopeKey, phaseKey { "PHASE" },
                     listenKey { "AUTO LISTEN" }, keysKey { "KEYS" }, spectrumKey { "SPECTRUM" };
    juce::Slider lowCutFreq, highCutFreq, preGain, output;
    juce::OwnedArray<BandColumn> bands;
    pr7s::ChannelEq shown;
};

// The EQ in its own window (one; the Inspector's EDIT and the mixer open it on a channel).
class EqWindow final : public juce::DocumentWindow
{
public:
    EqWindow (pr7s::Studio&, int busId);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    EqEditor& getEditor() { return *editor; }

private:
    EqEditor* editor = nullptr;
};
