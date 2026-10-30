#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/LevelMeterSource.h"

// The PR7 family's level meter, as on PR7-C: fine LED segments on black
// glass, orange at the bottom shading to pink-red at the top, unlit ones
// faintly there, and the highest recent peak held lit as one segment for a
// moment. The scale is digital (dBFS): -60 at the bottom, +6 at the top (the
// faders' range). The studio's meters all draw with paintSegments: this
// component (the transport's OUT), the mixer's and the track list's.
class LevelMeter final : public juce::Component,
                         private juce::Timer
{
public:
    explicit LevelMeter (LevelMeterSource& source);

    void paint (juce::Graphics&) override;

    // Kept for the callers from before; the meter has no printed scale now.
    void setShowScale (bool) {}

    static constexpr float minDb = -60.0f;
    static constexpr float maxDb = 6.0f;
    static float proportionForDb (float db) noexcept;   // 0 at the bottom, 1 at the top

    // The segments in `area`, lit up to `lit` (0 to 1), the one at `held`
    // lit too (below 0: none). Vertical: from the bottom up; else from the
    // left. `pitch`: a segment every so many pixels.
    static void paintSegments (juce::Graphics&, juce::Rectangle<float> area, float lit, float held, bool vertical = true, float pitch = 3.0f);

    // A meter's reading as it's shown: falling back after a peak, and the
    // peak held for a moment. Fed 30 times a second.
    struct Ballistics
    {
        float db = minDb, heldDb = minDb;
        int heldFor = 0;
        bool push (float peakGain);   // true if what's shown changed
    };

private:
    void timerCallback() override;

    LevelMeterSource& source;
    Ballistics reading;
};
