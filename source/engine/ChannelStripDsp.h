#pragma once

#include <juce_dsp/juce_dsp.h>

#include "engine/ChannelStrip.h"
#include "engine/strip/ChannelEq.h"
#include "engine/strip/Compressor.h"
#include "engine/strip/ConsoleChannel.h"
#include "engine/strip/ConsoleColour.h"
#include "engine/strip/Gate.h"
#include "engine/strip/Saturation.h"
#include "engine/strip/StereoTools.h"

namespace pr7s
{
    // (0.248.0) A channel's CHANNEL STRIP at work: PR7-C's modules (copied unchanged from PR7-C 0.4.7 / 0.5.0,
    // namespace pr7c) in PR7-C's order, in two halves around the channel's own EQ (where PR7-C's EQ sits):
    //   processDynamics: the detector (the sound here, through the SC FILTER and EDGE), the GATE, the COMPRESSOR;
    //   processTone:     SATURATION, the CONSOLE COLOUR (THD, V GAIN, each side's console channel), MONO MAKER /
    //                    STEREO WIDTH, then LISTEN (the detector, delayed as much as the sound, crossfaded in).
    // Call both on the same block (stereo, at most the prepared block size). Audio thread: nothing allocated
    // or locked. While the strip isn't in use nothing is called; coming back into use, it starts afresh (the
    // modules jump to their settings), as PR7-C does after a prepare.
    class StripProcessor
    {
    public:
        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void processDynamics (float* const* channels, int numChannels, int numSamples, const ChannelStrip&) noexcept;
        void processTone (float* const* channels, int numChannels, int numSamples, const ChannelStrip&) noexcept;
        // The gain reduction (dB, 0 or more) over the last block: the gate's and the compressor's, the louder side.
        float getGateReductionDb() const noexcept;
        float getCompReductionDb() const noexcept;

    private:
        void updateSides (const ChannelStrip&) noexcept;
        juce::AudioBuffer<float> view (float* const* channels, int numChannels, int numSamples) noexcept;

        int maxBlockSize = 0;
        pr7c::Gate gate;
        pr7c::Compressor compressor;
        pr7c::CutFilters scFilter;
        pr7c::EqBand scEdge { pr7c::EqBand::Shelf::none };
        juce::AudioBuffer<float> sidechain;
        pr7c::Saturation saturation;
        pr7c::ConsoleColour colour;
        pr7c::StereoTools stereoTools;
        std::array<pr7c::ConsoleChannel, 2> sides;
        std::array<int, 2> sideNumbers { 1, 1 };
        juce::SmoothedValue<float> listen { 0.0f };
        pr7c::FixedDelay listenDelay;
        std::array<float*, 2> pointers {};
    };
}
