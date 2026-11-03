#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

// Audio Alignment (0.33.0, the manual's Audio Alignment and Audio Alignment Panel; the way of
// working from a dedicated vocal-alignment tool's manual: the energy patterns of the two sounds
// matched continuously, a band the target may move within, a limit on how much it may stretch,
// sync points and protected ranges). Plain functions on sounds: no screen, no engine, no song.
//
// How it works: both sounds are measured every 10 ms (loudness, how suddenly it rises, and the
// spectrum's shape in 24 bands, which follows syllables). The cheapest path through the grid of
// "this moment of the reference against that moment of the target" (dynamic time warping) is
// the alignment: within +/- the maximum shift of where the target is now, each step either
// keeping pace or one of the two going twice as fast (a cost on those: FLEXIBILITY), through
// every sync point. The path is smoothed (PRECISION), held steady over protected ranges, and
// reduced to as few points as keep every moment within the precision's tolerance.
namespace pr7s
{
    struct AlignSettings
    {
        enum class Flexibility { low, normal, high };
        double precision = 0.75;            // 0-1: the manual's Alignment Precision
        Flexibility flexibility = Flexibility::normal;
        double maxShiftSeconds = 0.15;      // how far a moment of the target may move
        bool matchWords = false;            // the spectrum's shape counts more (syllables, words)
        bool preferShifting = false;        // the manual's Prefer Time Shifting: moved, not stretched, when that's close enough
        bool onlyWhereBothPlay = true;      // where either is silent, the target keeps its pace

        static constexpr double leastShift = 0.01, mostShift = 0.5;
        // How far a moment may land from where the reference has it: 2 ms at 100 %, 30 ms at 0 %.
        double tolerance() const noexcept { return 0.002 + (1.0 - juce::jlimit (0.0, 1.0, precision)) * 0.028; }
        juce::String toString() const;
        static AlignSettings fromString (const juce::String&);
        bool operator== (const AlignSettings&) const = default;
    };

    // A sound to align, mono: its first sample is heard at `start` (song seconds).
    struct AlignSound
    {
        std::vector<float> samples;
        double start = 0.0;
    };

    // A sync point: what the reference has at `reference` must land there in the target, which
    // has it at `target` now (song seconds).
    struct AlignSyncPoint
    {
        double reference = 0.0, target = 0.0;
        bool operator== (const AlignSyncPoint&) const = default;
    };

    struct AlignResult
    {
        bool ok = false;
        juce::String error;
        // Where the target should play what it plays now at g (song seconds): at t. Pairs (t, g),
        // both increasing, the first at the span's start and the last at its end.
        std::vector<std::pair<double, double>> points;
        bool shiftOnly = false;     // a plain move: g = t + shift everywhere
        double shift = 0.0;
        std::vector<int> unmetSyncPoints;   // indices of the sync points that couldn't be met
        // How far apart they were (on average, where both play) before and after, in seconds.
        double before = 0.0, after = 0.0;
    };

    // The target aligned to the reference over [from, to] (song seconds). The target sound
    // should reach past both ends by the maximum shift (silence where there's none).
    AlignResult alignSounds (const AlignSound& reference, const AlignSound& target, double from, double to,
                             double sampleRate, const AlignSettings&,
                             const std::vector<AlignSyncPoint>& syncPoints = {},
                             const std::vector<juce::Range<double>>& protectedRanges = {});

    // g at t (in the result's points; a straight line between them, past the ends at their offset).
    double alignedTargetAt (const AlignResult&, double t);
}
