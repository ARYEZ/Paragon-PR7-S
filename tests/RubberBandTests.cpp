#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "engine/Sizing.h"
#include "engine/Stretch.h"
#include "engine/Warp.h"

#include <chrono>
#include <functional>
#include <iostream>

// (0.188.0) RUBBER BAND, the fifth Time Stretch Algorithm (Rubber Band Library's R3 "finer" engine):
// the pitch kept, the length exactly as asked, nothing broken in it, about as loud; a clip set to it
// plays its stretched sound; saved; a mode from a later version read as CLEAN.

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    constexpr double rate = 48000.0;

    juce::AudioBuffer<float> tone (double hz, double seconds, int channels = 2)
    {
        juce::AudioBuffer<float> sound (channels, (int) (seconds * rate));
        for (int i = 0; i < sound.getNumSamples(); ++i)
            for (int c = 0; c < channels; ++c)
                sound.setSample (c, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / rate));
        return sound;
    }

    void writeWav (const juce::File& file, const juce::AudioBuffer<float>& sound)
    {
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2)
                                                                           .withBitsPerSample (32)
                                                                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (sound, 0, sound.getNumSamples());
    }

    double frequencyOf (const juce::AudioBuffer<float>& sound, int channel, double from, double to)
    {
        auto crossings = 0;
        for (int i = (int) (from * rate) + 1; i < (int) (to * rate) && i < sound.getNumSamples(); ++i)
            if (sound.getSample (channel, i - 1) < 0.0f && sound.getSample (channel, i) >= 0.0f)
                ++crossings;
        return crossings / (to - from);
    }

    double rmsOf (const juce::AudioBuffer<float>& sound, int channel, double from, double to)
    {
        double sum = 0.0;
        int n = 0;
        for (int i = (int) (from * rate); i < (int) (to * rate) && i < sound.getNumSamples(); ++i, ++n)
            sum += (double) sound.getSample (channel, i) * sound.getSample (channel, i);
        return n > 0 ? std::sqrt (sum / n) : 0.0;
    }

    bool allFinite (const juce::AudioBuffer<float>& sound)
    {
        for (int c = 0; c < sound.getNumChannels(); ++c)
            for (int i = 0; i < sound.getNumSamples(); ++i)
                if (! std::isfinite (sound.getSample (c, i)))
                    return false;
        return true;
    }
}

TEST_CASE ("RUBBER BAND 0.188.0: a 440 Hz tone stretched 1.5x keeps its pitch, its level and the exact length", "[rubberband]")
{
    const auto input = tone (440.0, 2.0);
    const auto wanted = (int) (3.0 * rate) + 7;   // (not a round number: the length must still be exact)
    const auto out = stretchAudio (input, rate, wanted, 0.0, StretchModes::rubberBand);
    CHECK (out.getNumChannels() == 2);
    CHECK (out.getNumSamples() == wanted);
    CHECK (allFinite (out));
    for (int c = 0; c < 2; ++c)
    {
        CHECK_THAT (frequencyOf (out, c, 0.5, 2.5), WithinAbs (440.0, 6.0));
        const auto db = juce::Decibels::gainToDecibels (rmsOf (out, c, 0.5, 2.5) / rmsOf (input, c, 0.5, 1.5));
        CHECK (std::abs (db) < 3.0);
    }

    // In time: a click 1 s in comes out 1.5 s in (no delay of the engine's left in front).
    {
        juce::AudioBuffer<float> click (1, (int) (2.0 * rate));
        click.clear();
        for (int i = 0; i < 48; ++i)
            click.setSample (0, (int) rate + i, 0.8f * (1.0f - i / 48.0f));
        const auto moved = stretchAudio (click, rate, (int) (3.0 * rate), 0.0, StretchModes::rubberBand);
        int loudest = 0;
        for (int i = 0; i < moved.getNumSamples(); ++i)
            if (std::abs (moved.getSample (0, i)) > std::abs (moved.getSample (0, loudest)))
                loudest = i;
        CHECK (std::abs (loudest / rate - 1.5) < 0.02);
    }

    // Squeezed too (2 s into 1.2 s), and a mono sound.
    const auto mono = tone (440.0, 2.0, 1);
    const auto shorter = stretchAudio (mono, rate, (int) (1.2 * rate), 0.0, StretchModes::rubberBand);
    CHECK (shorter.getNumSamples() == (int) (1.2 * rate));
    CHECK (allFinite (shorter));
    CHECK_THAT (frequencyOf (shorter, 0, 0.2, 1.0), WithinAbs (440.0, 8.0));

    // Moved up an octave (semitones, as the MediaBay's TRANSPOSE would ask): 880 Hz.
    const auto up = stretchAudio (input, rate, (int) (2.0 * rate), 12.0, StretchModes::rubberBand);
    CHECK (up.getNumSamples() == (int) (2.0 * rate));
    CHECK_THAT (frequencyOf (up, 0, 0.3, 1.7), WithinAbs (880.0, 12.0));

    // Nothing in, nothing asked: silence of the asked length, no crash.
    CHECK (stretchAudio (juce::AudioBuffer<float> (2, 0), rate, 100, 0.0, StretchModes::rubberBand).getNumSamples() == 100);
    CHECK (stretchAudio (input, rate, 0, 0.0, StretchModes::rubberBand).getNumSamples() == 0);
}

TEST_CASE ("RUBBER BAND 0.188.0: a clip set to it plays its stretched sound; it's in the menus' list", "[rubberband]")
{
    Studio studio (false);
    const auto trackId = studio.addAudioTrack (true);
    writeWav (studio.getAudioFolder().getChildFile ("tone.wav"), tone (440.0, 2.0));
    Part clip;
    clip.length = 4.0;
    clip.audioFile = "tone.wav";
    studio.addPart (trackId, clip);
    const auto part = [&]() -> const Part& { return studio.getProject().findTrack (trackId)->parts.at (0); };
    const auto id = part().id;
    const auto tempo = studio.getProject().tempoMap();
    REQUIRE (studio.sizeParts ({ id }, id, 0.0, 6.0, Sizing::timeStretch));
    REQUIRE (studio.setStretchMode ({ id }, StretchModes::rubberBand));
    CHECK (part().stretchMode == StretchModes::rubberBand);
    const auto sound = studio.getWarpedSound (part(), tempo);
    REQUIRE (sound != nullptr);
    CHECK (std::abs (sound->getNumSamples() - (int) (3.0 * rate)) <= 1);
    CHECK (allFinite (*sound));
    CHECK_THAT (frequencyOf (*sound, 0, 0.5, 2.5), WithinAbs (440.0, 6.0));

    // The default for new stretches can be it too.
    studio.setDefaultStretchMode (StretchModes::rubberBand);
    CHECK (studio.getDefaultStretchMode() == StretchModes::rubberBand);
    studio.setDefaultStretchMode (StretchModes::clean);

    CHECK (StretchModes::count == 5);
    CHECK (juce::String (StretchModes::name (StretchModes::rubberBand)) == "RUBBER BAND");
    CHECK (juce::String (StretchModes::description (StretchModes::rubberBand)).startsWith ("RUBBER BAND"));
}

TEST_CASE ("RUBBER BAND 0.188.0: saved and read back; a mode from a later version reads as CLEAN", "[rubberband]")
{
    Project project;
    Track track;
    track.id = 1;
    track.kind = Track::Kind::audio;
    Part clip;
    clip.audioFile = "a.wav";
    clip.stretchMode = StretchModes::rubberBand;
    clip.warp = { { 0.0, 0.0 }, { 4.0, 1.0 } };
    track.parts = std::vector<Part> { clip };
    project.tracks.push_back (track);
    auto xml = project.toXml();
    auto back = Project::fromXml (*xml);
    REQUIRE (back.has_value());
    REQUIRE (back->findTrack (1) != nullptr);
    CHECK (back->findTrack (1)->parts.at (0).stretchMode == StretchModes::rubberBand);

    // A later version's mode 9 (or a broken -2): CLEAN, never past the list.
    for (const auto unknown : { 9, -2 })
    {
        auto copy = project;
        auto edited = copy.toXml();
        auto parts = 0;
        std::function<void (juce::XmlElement&)> mark = [&] (juce::XmlElement& e) {
            if (e.hasTagName ("Part"))
            {
                e.setAttribute ("stretch", unknown);
                ++parts;
            }
            for (auto* child : e.getChildIterator())
                mark (*child);
        };
        mark (*edited);
        REQUIRE (parts == 1);
        const auto read = Project::fromXml (*edited);
        REQUIRE (read.has_value());
        REQUIRE (read->findTrack (1) != nullptr);
        CHECK (read->findTrack (1)->parts.at (0).stretchMode == StretchModes::clean);
    }
    // stretchAudio itself takes an unknown mode as CLEAN (the pitch kept).
    const auto out = stretchAudio (tone (440.0, 1.0), rate, (int) (1.4 * rate), 0.0, 17);
    CHECK (out.getNumSamples() == (int) (1.4 * rate));
    CHECK_THAT (frequencyOf (out, 0, 0.3, 1.1), WithinAbs (440.0, 8.0));
}

TEST_CASE ("RUBBER BAND 0.188.0: timing against CLEAN (10 s of stereo, 1.4x)", "[.perf-rubberband]")
{
    juce::AudioBuffer<float> sound (2, (int) (10.0 * rate));
    juce::Random random (7);
    for (int i = 0; i < sound.getNumSamples(); ++i)
    {
        const auto t = i / rate;
        const auto v = 0.3 * std::sin (2.0 * juce::MathConstants<double>::pi * 110.0 * t) + 0.2 * std::sin (2.0 * juce::MathConstants<double>::pi * 330.0 * t)
                     + (std::fmod (t, 0.5) < 0.02 ? 0.3 * (random.nextFloat() * 2.0 - 1.0) : 0.0);
        sound.setSample (0, i, (float) v);
        sound.setSample (1, i, (float) (v * 0.9));
    }
    const auto wanted = (int) (14.0 * rate);
    for (const auto mode : { StretchModes::clean, StretchModes::rubberBand, StretchModes::clean, StretchModes::rubberBand })
    {
        const auto start = std::chrono::steady_clock::now();
        const auto out = stretchAudio (sound, rate, wanted, 0.0, mode);
        const auto ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - start).count();
        std::cout << StretchModes::name (mode) << ": " << ms << " ms (" << out.getNumSamples() << " samples)" << std::endl;
    }
}
