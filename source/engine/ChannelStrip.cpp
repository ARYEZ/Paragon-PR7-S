#include "engine/ChannelStrip.h"
#include "engine/ChannelStripDsp.h"

namespace pr7s
{
    const juce::StringArray& ChannelStrip::characterNames()
    {
        static const juce::StringArray names { "VCA", "OPTO", "FET", "MODERN" };
        return names;
    }

    const juce::StringArray& ChannelStrip::distortionNames()
    {
        static const juce::StringArray names { "CLEAN", "2ND", "2ND + 3RD" };
        return names;
    }

    const juce::StringArray& ChannelStrip::attackNames()
    {
        static const juce::StringArray names { "FAST", "MED", "SLOW" };
        return names;
    }

    ChannelStrip ChannelStrip::limited() const
    {
        // Each value in PR7-C's range; steps snapped from 0 (PR7-C's `steppedRange`).
        const auto step = [] (float v, float lo, float hi, float by) { return juce::jlimit (lo, hi, std::round (v / by) * by); };
        auto s = *this;
        s.gate.thresholdDb = step (gate.thresholdDb, -60.0f, 0.0f, 0.1f);
        s.gate.rangeDb = step (gate.rangeDb, 0.0f, 80.0f, 0.1f);
        s.gate.ratio = juce::jlimit (1.0f, 20.0f, gate.ratio);
        s.gate.attack = juce::jlimit (0, 2, gate.attack);
        s.gate.holdMs = juce::jlimit (0.0f, 500.0f, gate.holdMs);
        s.gate.releaseMs = juce::jlimit (5.0f, 3000.0f, gate.releaseMs);
        s.sc.hpf = juce::jlimit (20.0f, 200.0f, sc.hpf);
        s.sc.lpf = juce::jlimit (1000.0f, 20000.0f, sc.lpf);
        s.comp.thresholdDb = step (comp.thresholdDb, -50.0f, 0.0f, 0.1f);
        s.comp.ratio = juce::jlimit (1.0f, 30.0f, comp.ratio);
        s.comp.attackMs = juce::jlimit (0.1f, 30.0f, comp.attackMs);
        s.comp.releaseMs = juce::jlimit (50.0f, 3000.0f, comp.releaseMs);
        s.comp.makeUpDb = step (comp.makeUpDb, 0.0f, 20.0f, 0.1f);
        s.comp.character = juce::jlimit (0, characterNames().size() - 1, comp.character);
        s.comp.distortion = juce::jlimit (0, distortionNames().size() - 1, comp.distortion);
        s.sat.driveDb = step (sat.driveDb, 0.0f, 24.0f, 0.1f);
        s.sat.harmonics = step (sat.harmonics, 0.0f, 100.0f, 1.0f);
        s.sat.mix = step (sat.mix, 0.0f, 100.0f, 1.0f);
        s.colour.thd = step (colour.thd, 0.0f, 100.0f, 1.0f);
        s.colour.vGain = step (colour.vGain, 0.0f, 100.0f, 1.0f);
        s.colour.channel = juce::jlimit (1, consoleChannels, colour.channel);
        s.stereo.monoHz = juce::jlimit (20.0f, 500.0f, stereo.monoHz);
        s.stereo.width = step (stereo.width, 0.0f, 200.0f, 1.0f);
        return s;
    }

    juce::String ChannelStrip::summary() const
    {
        juce::StringArray on;
        if (gate.on)
            on.add (gate.expander ? "EXP" : "GATE");
        if (comp.on)
            on.add ("COMP " + characterNames()[comp.character]);
        if (sat.on)
            on.add ("SAT");
        if (colour.on)
            on.add ("COLOUR CH " + juce::String (colour.channel));
        if (stereo.monoOn)
            on.add ("MONO");
        if (stereo.widthOn)
            on.add ("WIDTH");
        if (sc.listen)
            on.add ("LISTEN");
        return on.isEmpty() ? juce::String ("OFF") : on.joinIntoString (", ");
    }

    std::unique_ptr<juce::XmlElement> ChannelStrip::toXml() const
    {
        auto e = std::make_unique<juce::XmlElement> ("Strip");
        const auto b = [&e] (const char* name, bool v) { e->setAttribute (name, v ? 1 : 0); };
        const auto f = [&e] (const char* name, float v) { e->setAttribute (name, (double) v); };
        const auto i = [&e] (const char* name, int v) { e->setAttribute (name, v); };
        b ("gateOn", gate.on); f ("gateThreshold", gate.thresholdDb); b ("gateMinus30", gate.minus30); f ("gateRange", gate.rangeDb);
        b ("gateExpander", gate.expander); f ("gateRatio", gate.ratio); i ("gateAttack", gate.attack); f ("gateHold", gate.holdMs);
        f ("gateRelease", gate.releaseMs); b ("gateLinear", gate.linear);
        b ("scOn", sc.on); f ("scHpf", sc.hpf); f ("scLpf", sc.lpf); b ("scHighQ", sc.highQ); b ("scEdge", sc.edge); b ("scListen", sc.listen);
        b ("compOn", comp.on); f ("compThreshold", comp.thresholdDb); f ("compRatio", comp.ratio); f ("compAttack", comp.attackMs);
        f ("compRelease", comp.releaseMs); b ("compAuto", comp.autoRelease); f ("compMakeUp", comp.makeUpDb);
        i ("compCharacter", comp.character); i ("compDistortion", comp.distortion);
        b ("link", link);
        b ("satOn", sat.on); f ("satDrive", sat.driveDb); f ("satHarmonics", sat.harmonics); b ("satVcaClip", sat.vcaClip); f ("satMix", sat.mix);
        b ("colourOn", colour.on); f ("thd", colour.thd); f ("vGain", colour.vGain); i ("channel", colour.channel); b ("digital", colour.digital);
        b ("monoOn", stereo.monoOn); f ("monoFreq", stereo.monoHz); b ("widthOn", stereo.widthOn); f ("width", stereo.width);
        return e;
    }

    ChannelStrip ChannelStrip::fromXml (const juce::XmlElement& e)
    {
        ChannelStrip s;
        const auto b = [&e] (const char* name, bool fallback) { return e.getIntAttribute (name, fallback ? 1 : 0) != 0; };
        const auto f = [&e] (const char* name, float fallback) { return (float) e.getDoubleAttribute (name, fallback); };
        const auto i = [&e] (const char* name, int fallback) { return e.getIntAttribute (name, fallback); };
        const ChannelStrip d;
        s.gate = { b ("gateOn", d.gate.on), f ("gateThreshold", d.gate.thresholdDb), b ("gateMinus30", d.gate.minus30), f ("gateRange", d.gate.rangeDb),
                   b ("gateExpander", d.gate.expander), f ("gateRatio", d.gate.ratio), i ("gateAttack", d.gate.attack), f ("gateHold", d.gate.holdMs),
                   f ("gateRelease", d.gate.releaseMs), b ("gateLinear", d.gate.linear) };
        s.sc = { b ("scOn", d.sc.on), f ("scHpf", d.sc.hpf), f ("scLpf", d.sc.lpf), b ("scHighQ", d.sc.highQ), b ("scEdge", d.sc.edge), b ("scListen", d.sc.listen) };
        s.comp = { b ("compOn", d.comp.on), f ("compThreshold", d.comp.thresholdDb), f ("compRatio", d.comp.ratio), f ("compAttack", d.comp.attackMs),
                   f ("compRelease", d.comp.releaseMs), b ("compAuto", d.comp.autoRelease), f ("compMakeUp", d.comp.makeUpDb),
                   i ("compCharacter", d.comp.character), i ("compDistortion", d.comp.distortion) };
        s.link = b ("link", d.link);
        s.sat = { b ("satOn", d.sat.on), f ("satDrive", d.sat.driveDb), f ("satHarmonics", d.sat.harmonics), b ("satVcaClip", d.sat.vcaClip), f ("satMix", d.sat.mix) };
        s.colour = { b ("colourOn", d.colour.on), f ("thd", d.colour.thd), f ("vGain", d.colour.vGain), i ("channel", d.colour.channel), b ("digital", d.colour.digital) };
        s.stereo = { b ("monoOn", d.stereo.monoOn), f ("monoFreq", d.stereo.monoHz), b ("widthOn", d.stereo.widthOn), f ("width", d.stereo.width) };
        return s.limited();
    }

    //==========================================================================
    void StripProcessor::prepare (double sampleRate, int maxBlock)
    {
        maxBlockSize = juce::jmax (1, maxBlock);
        sidechain.setSize (2, maxBlockSize);
        gate.prepare (sampleRate, maxBlockSize);
        compressor.prepare (sampleRate, maxBlockSize);
        scFilter.prepare (sampleRate);
        scEdge.prepare (sampleRate);
        saturation.prepare (sampleRate, maxBlockSize);
        colour.prepare (sampleRate, maxBlockSize);
        stereoTools.prepare (sampleRate, maxBlockSize);
        listen.reset (sampleRate, 0.02);
        listen.setCurrentAndTargetValue (0.0f);
        listenDelay.prepare (ChannelStrip::latencySamples);
        sides = {};
        sideNumbers = { 1, 1 };
    }

    void StripProcessor::reset() noexcept
    {
        gate.reset();
        compressor.reset();
        scFilter.reset();
        scEdge.reset();
        saturation.reset();
        colour.reset();
        stereoTools.reset();
        listenDelay.reset();
        listen.setCurrentAndTargetValue (0.0f);
    }

    juce::AudioBuffer<float> StripProcessor::view (float* const* channels, int numChannels, int numSamples) noexcept
    {
        const auto n = juce::jlimit (1, 2, numChannels);
        for (int c = 0; c < n; ++c)
            pointers[(size_t) c] = channels[c];
        return juce::AudioBuffer<float> (pointers.data(), n, numSamples);
    }

    void StripProcessor::updateSides (const ChannelStrip& s) noexcept
    {
        // PR7-C's: off, both sides the design itself (channel 1); on, the left the chosen channel, the right
        // the next (ANALOG) or the same (DIGITAL).
        const auto left = s.colour.on ? s.colour.channel : 1;
        const std::array<int, 2> wanted { left, s.colour.on && ! s.colour.digital ? pr7c::ConsoleChannel::nextTo (left) : left };
        for (size_t side = 0; side < 2; ++side)
            if (wanted[side] != sideNumbers[side])
            {
                sides[side] = pr7c::ConsoleChannel::number (wanted[side]);
                sideNumbers[side] = wanted[side];
            }
    }

    void StripProcessor::processDynamics (float* const* channels, int numChannels, int numSamples, const ChannelStrip& s) noexcept
    {
        if (numSamples <= 0 || numSamples > maxBlockSize)
            return;
        auto audio = view (channels, numChannels, numSamples);
        updateSides (s);
        // What the gate and compressor listen to (PR7-C's processModules): the sound here, through the SC
        // FILTER, and EDGE.
        for (int c = 0; c < 2; ++c)
            juce::FloatVectorOperations::copy (sidechain.getWritePointer (c), audio.getReadPointer (juce::jmin (c, audio.getNumChannels() - 1)), numSamples);
        juce::AudioBuffer<float> detector (sidechain.getArrayOfWritePointers(), 2, 0, numSamples);
        scFilter.process (detector, numSamples, { s.sc.on, s.sc.on, s.sc.hpf, s.sc.lpf, s.sc.highQ ? 2.5f : pr7c::CutFilters::butterworthQ });
        scEdge.process (detector, numSamples, { s.sc.on && s.sc.edge, 6000.0f, 8.0f, 1.0f, false });
        gate.process (audio, detector, numSamples,
                      { s.gate.on, s.gate.thresholdDb - (s.gate.minus30 ? 30.0f : 0.0f), s.gate.rangeDb, s.gate.expander, s.gate.ratio,
                        (pr7c::Gate::Attack) juce::jlimit (0, 2, s.gate.attack), s.gate.holdMs, s.gate.releaseMs, s.gate.linear, s.link });
        compressor.process (audio, detector, numSamples,
                            { s.comp.on, s.comp.thresholdDb, s.comp.ratio, s.comp.attackMs, s.comp.releaseMs, s.comp.autoRelease, s.comp.makeUpDb,
                              (pr7c::Compressor::Character) juce::jlimit (0, 3, s.comp.character), s.link,
                              (pr7c::Compressor::Distortion) juce::jlimit (0, 2, s.comp.distortion) });
    }

    void StripProcessor::processTone (float* const* channels, int numChannels, int numSamples, const ChannelStrip& s) noexcept
    {
        if (numSamples <= 0 || numSamples > maxBlockSize)
            return;
        auto audio = view (channels, numChannels, numSamples);
        saturation.process (audio, numSamples, { s.sat.on, s.sat.driveDb, s.sat.harmonics / 100.0f, s.sat.vcaClip, s.sat.mix / 100.0f });
        pr7c::ConsoleColour::Settings tone;
        tone.thd = s.colour.on ? s.colour.thd / 100.0f : 0.0f;
        tone.noise = s.colour.on ? s.colour.vGain / 100.0f : 0.0f;
        for (size_t side = 0; side < 2; ++side)
        {
            tone.second[side] = sides[side].second;
            tone.third[side] = sides[side].third;
            tone.hissDb[side] = sides[side].hissDb;
            tone.humDb[side] = sides[side].humDb;
        }
        colour.process (audio, numSamples, tone);
        stereoTools.process (audio, numSamples, { s.stereo.monoOn, s.stereo.monoHz, s.stereo.widthOn, s.stereo.width / 100.0f });
        // LISTEN: what the gate and compressor hear (a 20 ms crossfade), delayed as much as the sound.
        juce::AudioBuffer<float> detector (sidechain.getArrayOfWritePointers(), 2, 0, numSamples);
        listenDelay.process (juce::dsp::AudioBlock<float> (detector));
        listen.setTargetValue (s.sc.listen ? 1.0f : 0.0f);
        if (listen.isSmoothing() || listen.getCurrentValue() > 0.0f)
            for (int i = 0; i < numSamples; ++i)
            {
                const auto amount = listen.getNextValue();
                for (int c = 0; c < audio.getNumChannels(); ++c)
                {
                    auto* samples = audio.getWritePointer (c);
                    samples[i] += (detector.getSample (juce::jmin (c, 1), i) - samples[i]) * amount;
                }
            }
    }

    float StripProcessor::getGateReductionDb() const noexcept
    {
        return juce::jmax (gate.getReductionDb (0), gate.getReductionDb (1));
    }

    float StripProcessor::getCompReductionDb() const noexcept
    {
        return juce::jmax (compressor.getReductionDb (0), compressor.getReductionDb (1));
    }
}
