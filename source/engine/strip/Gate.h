#pragma once

#include <array>
#include <atomic>

#include "engine/strip/EffectUtils.h"

namespace pr7c
{
    // The channel gate / expander, in stereo. It listens to the sidechain
    // (the audio, through the SC FILTER and, on SC, the HPF / LPF) and turns
    // the audio down when it's quiet.
    //
    //   GATE  open or closed: closed turns down by RANGE. It opens when the
    //         level goes over THRESHOLD and closes when it falls 3 dB below
    //         it (so a note sitting on the threshold doesn't chatter).
    //   EXP   an expander: below the threshold, every dB quieter is turned
    //         down a further (RATIO - 1) dB, up to RANGE.
    //
    // ATTACK (FAST / MED / SLOW: 0.1, 1, 10 ms) is how quickly it opens.
    // HOLD keeps it open after the level drops, then RELEASE closes it:
    // exponential by default (quick at first, then easing, nearly all the
    // way in RELEASE), or LINEAR (a steady rate, all the way in RELEASE).
    // LINK opens and closes both channels together, from the louder one.
    class Gate
    {
    public:
        enum class Attack { fast, medium, slow };   // saved by index: only append

        struct Settings
        {
            bool on = false;
            float thresholdDb = -40.0f;   // dBFS, -30 already applied
            float rangeDb = 40.0f;
            bool expander = false;
            float ratio = 2.0f;
            Attack attack = Attack::medium;
            float holdMs = 50.0f, releaseMs = 200.0f;
            bool linearRelease = false;
            bool link = true;
        };

        void prepare (double sampleRate, int maxBlockSize);
        void reset();
        void process (juce::AudioBuffer<float>& audio, const juce::AudioBuffer<float>& sidechain, int numSamples,
                      const Settings&) noexcept;

        float getReductionDb (int channel) const noexcept { return reductionDb[(size_t) juce::jlimit (0, 1, channel)].load(); }

        static constexpr float hysteresisDb = 3.0f;
        static float attackMs (Attack) noexcept;

    private:
        double sampleRate = 48000.0;
        bool fresh = true;
        BypassFade bypass;
        std::array<float, 2> level {}, gainDb {}, holdLeft {};
        std::array<bool, 2> open {};
        std::array<std::atomic<float>, 2> reductionDb {};
    };
}
