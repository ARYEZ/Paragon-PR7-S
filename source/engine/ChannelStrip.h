#pragma once

#include <juce_core/juce_core.h>

namespace pr7s
{
    // (0.248.0) The CHANNEL STRIP on every channel and the master: what PR7-C (the family's console channel
    // strip) does that the studio's own channel didn't, in PR7-C's fixed order (the owner's choice):
    //
    //   GATE / EXP (and its SC FILTER) -> COMPRESSOR -> [the channel's own EQ] -> SATURATION
    //     -> CONSOLE COLOUR -> MONO MAKER / STEREO WIDTH
    //
    // The gate and compressor share one detector: the sound at the gate's input, through the SC FILTER (and
    // EDGE); LISTEN plays it instead. PR7-C's own five-band EQ, HPF / LPF, limiter and output stage aren't
    // here: the studio's channel EQ, its Pre section and the BOOSTER cover them (and PR7-X has the limiter).
    // Every module is off to start with; while any is on, the strip delays the sound a constant 7 samples
    // (PR7-C's: SATURATION's oversampling 4 + THD's 3), told to the delay compensation (0.247.0).
    //
    // Plain data on the track (saved when not as it starts, restored by undo, taken by track presets); the
    // ranges and defaults are PR7-C's (its Parameters.cpp). Our design throughout, never tuned by ear.
    struct ChannelStrip
    {
        struct Gate
        {
            bool on = false;
            float thresholdDb = -40.0f;   // -60..0 dBFS (MINUS 30: -90..-30)
            bool minus30 = false;
            float rangeDb = 40.0f;        // 0..80
            bool expander = false;        // EXP: below the threshold, (ratio - 1) dB more per dB
            float ratio = 2.0f;           // 1..20
            int attack = 1;               // FAST / MED / SLOW (0.1, 1, 10 ms)
            float holdMs = 50.0f;         // 0..500
            float releaseMs = 200.0f;     // 5..3000
            bool linear = false;          // LINEAR release
            bool operator== (const Gate&) const = default;
        };
        struct Sidechain
        {
            bool on = false;              // the SC FILTER: filters what the gate and compressor hear (never the sound)
            float hpf = 60.0f;            // 20..200 Hz
            float lpf = 20000.0f;         // 1k..20k Hz
            bool highQ = false;           // resonance 2.5 instead of 0.707
            bool edge = false;            // EDGE: the detector hears 6 kHz 8 dB louder
            bool listen = false;          // LISTEN: hear what the detector hears
            bool operator== (const Sidechain&) const = default;
        };
        struct Comp
        {
            bool on = false;
            float thresholdDb = -18.0f;   // -50..0 dBFS
            float ratio = 4.0f;           // 1 = SOFT, 1.5..30 (30 and up: infinite)
            float attackMs = 10.0f;       // 0.1..30
            float releaseMs = 300.0f;     // 50..3000
            bool autoRelease = false;
            float makeUpDb = 0.0f;        // 0..20
            int character = 0;            // VCA / OPTO / FET / MODERN
            int distortion = 0;           // MODERN's CLEAN / 2ND / 2ND + 3RD
            bool operator== (const Comp&) const = default;
        };
        struct Saturation
        {
            bool on = false;
            float driveDb = 6.0f;         // 0..24 (the level kept)
            float harmonics = 30.0f;      // 0 (2nd) .. 100 (3rd) %
            bool vcaClip = false;
            float mix = 100.0f;           // %
            bool operator== (const Saturation&) const = default;
        };
        struct Colour
        {
            bool on = false;              // the console colour: THD, the noise and the channel's own differences
            float thd = 0.0f;             // 0..100 %
            float vGain = 0.0f;           // 0..100 %: the noise floor
            int channel = 1;              // 1..72: the left side's console channel (1: the exact design)
            bool digital = false;         // DIGITAL: both sides the same channel; ANALOG: the right the next one
            bool operator== (const Colour&) const = default;
        };
        struct Stereo
        {
            bool monoOn = false;          // MONO MAKER: mono below its frequency
            float monoHz = 100.0f;        // 20..500
            bool widthOn = false;
            float width = 100.0f;         // 0..200 %: the sides only
            bool operator== (const Stereo&) const = default;
        };

        Gate gate;
        Sidechain sc;
        Comp comp;
        bool link = true;                 // LINK: the gate and compressor work on both sides together
        Saturation sat;
        Colour colour;
        Stereo stereo;

        static constexpr int consoleChannels = 72;
        static constexpr int latencySamples = 7;   // while in use (PR7-C's constant: SATURATION 4 + THD 3)
        static const juce::StringArray& characterNames();    // "VCA", "OPTO", "FET", "MODERN" (by index: only append)
        static const juce::StringArray& distortionNames();   // "CLEAN", "2ND", "2ND + 3RD"
        static const juce::StringArray& attackNames();       // "FAST", "MED", "SLOW"

        bool operator== (const ChannelStrip&) const = default;
        // Anything on (a module, or LISTEN): the strip runs, delaying the sound its constant 7 samples.
        bool inUse() const noexcept
        {
            return gate.on || comp.on || sat.on || colour.on || stereo.monoOn || stereo.widthOn || sc.listen;
        }
        bool isDefault() const { return *this == ChannelStrip {}; }
        ChannelStrip limited() const;     // every value in its range
        // What's on, in a few words ("GATE, COMP FET, SAT"), or "OFF".
        juce::String summary() const;

        std::unique_ptr<juce::XmlElement> toXml() const;   // "Strip"
        static ChannelStrip fromXml (const juce::XmlElement&);
    };
}
