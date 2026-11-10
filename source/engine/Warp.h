#pragma once

#include <string>

#include <juce_audio_basics/juce_audio_basics.h>

#include "Project.h"
#include "TempoMap.h"

// Hitpoints and audio warping (0.32.0, the manual's Hitpoints, Free Warp, AudioWarp Quantize and
// Flatten Real Time Processing). Plain functions on clips and sounds: no screen, no engine.
namespace pr7s
{
    //==========================================================================
    // Hitpoints: where something starts in a recording (a pick, a hit, a consonant), found from how
    // suddenly its sound changes (spectral flux: each moment's spectrum against the one before).
    struct Hitpoint
    {
        double seconds = 0.0;   // into the recording
        float strength = 0.0f;  // 0 to 1, the loudest change in the recording being 1
        bool operator== (const Hitpoint&) const = default;
    };
    // Every candidate in a sound (both sides mixed), strongest first in no order: sorted by time.
    std::vector<Hitpoint> detectHitpoints (const juce::AudioBuffer<float>&, double sampleRate);
    // The ones kept: at least `threshold` strong (0-1), none closer than `minimumSeconds` to a
    // stronger one kept; then the hand edits (removed ones within 10 ms dropped, added ones added).
    std::vector<double> filterHitpoints (const std::vector<Hitpoint>&, double threshold, double minimumSeconds,
                                         const Project::HitpointEdits* edits = nullptr);

    //==========================================================================
    // Warping. Where in its recording (seconds) a clip is at `beat` beats from its start: at its
    // own speed without markers; between markers in proportion to the song's time; outside them
    // (and past its ends) at its own speed from the nearest.
    double warpSecondsAt (const Part&, double beat, const TempoMap&);
    // The beat in the clip where its recording's `seconds` is heard (the inverse; first match).
    double warpBeatAt (const Part&, double seconds, const TempoMap&);
    // A clip given markers at its edges (at its own speed) if it had none.
    void ensureWarp (Part&, const TempoMap&);
    // A clip whose start or length changed keeps its audio where it was in the song: the markers
    // moved with the old clip's start, those outside dropped, its edges pinned where the old
    // clip had them. `updated` holds the new start and length (and gets the new markers and offset).
    void rebaseWarp (const Part& old, Part& updated, const TempoMap&);
    // How much the audio before each marker is stretched (the manual's factor shown beside it:
    // over 1 stretched, under 1 squeezed). One per marker; the first's is 1.
    std::vector<double> warpFactors (const Part&, const TempoMap&);
    // AudioWarp Quantize: each grid line (every `grid` beats of the song) takes the marker nearest
    // to it within half a step; the markers between move in proportion (the manual: "the remaining
    // warp markers are moved, keeping the relative distances"). The edges stay.
    void quantizeWarp (Part&, double grid, const TempoMap&);

    //==========================================================================
    // A warped clip's sound, from its start to its end, at the recording's rate: each stretch
    // between markers time-stretched to fit (copied as it is where the speed is its own), joined
    // with short crossfades. Stretches already made are kept (`Cache`), so moving one marker
    // remakes only the two beside it.
    class WarpRenderer
    {
    public:
        std::shared_ptr<const juce::AudioBuffer<float>> render (const Part&, const juce::AudioBuffer<float>& recording,
                                                                double sampleRate, const TempoMap&);
        void clear() { pieces.clear(); whole.clear(); samplesKept = 0; wholeSamplesKept = 0; }
        juce::int64 wholeSamplesHeld() const noexcept { return wholeSamplesKept; }
        size_t piecesKept() const noexcept { return pieces.size(); }

    private:
        std::map<juce::String, std::shared_ptr<const juce::AudioBuffer<float>>> pieces;
        // (0.83.1) The whole clips by an exact key of raw bytes (the numbers as they are, not written
        // out as text: the key is made on every engine update for every warped clip, a fader step too).
        std::map<std::string, std::shared_ptr<const juce::AudioBuffer<float>>> whole;
        juce::int64 samplesKept = 0, wholeSamplesKept = 0;
    };
}
