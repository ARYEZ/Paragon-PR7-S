#pragma once

#include <vector>

// Tempo detection (0.45.0, the manual's Tempo Detection panel; the method is ours): the beats
// of a recording or a MIDI part played without a click, so a tempo track can follow them.
//
// 1. An onset envelope: how much new sound starts in each frame of 5 ms (audio: the rise of a
//    log-magnitude spectrum, summed over its bins; MIDI: each note's start, by its velocity).
// 2. The tempo overall: the envelope's autocorrelation, each lag weighted towards 120 BPM (a
//    log-normal of one octave) and helped by its double, between 50 and 220 BPM.
// 3. The beats: dynamic programming (a beat is worth the envelope there, and the step from the
//    beat before costs by how far it is from the tempo: drift is followed, jumps aren't).
namespace pr7s
{
    struct BeatAnalysis
    {
        std::vector<double> beats;   // seconds from the start of what was analysed
        double bpm = 0.0;            // the tempo overall (the median beat's)
        double confidence = 0.0;     // 0-1: how clearly the envelope repeats at that tempo
    };

    constexpr double tempoFrameSeconds = 0.005;
    constexpr double leastDetectSeconds = 7.0;   // the manual's: at least 7 seconds

    std::vector<float> onsetEnvelope (const float* mono, int numSamples, double sampleRate);
    struct DetectNote { double seconds = 0.0; float velocity = 1.0f; };
    std::vector<float> onsetEnvelope (const std::vector<DetectNote>&, double lengthSeconds);
    double tempoOfEnvelope (const std::vector<float>& envelope, double* confidence = nullptr);
    BeatAnalysis beatsOfEnvelope (const std::vector<float>& envelope, double bpm = 0.0);   // 0: tempoOfEnvelope's
    BeatAnalysis detectBeats (const float* mono, int numSamples, double sampleRate);
    BeatAnalysis detectBeats (const std::vector<DetectNote>&, double lengthSeconds);

    // The panel's corrections, on the beats themselves: `factor` beats where there was one
    // (2: a beat between each two; 0.5: every second; 4/3, 3/4), `shift` by part of a beat
    // (0.5: the offbeat correction). Beyond the ends: the nearest beat's step carried on.
    std::vector<double> rescaleBeats (const std::vector<double>& beats, double factor, double shift = 0.0);
    // Smooth Tempo: each step the median of the seven around it, the beats rebuilt from the
    // first; then halfway back towards where they were heard, no more than a tenth of a step.
    std::vector<double> smoothBeats (const std::vector<double>& beats);
    double medianBpm (const std::vector<double>& beats);
}
