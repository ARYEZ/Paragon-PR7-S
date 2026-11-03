#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "engine/Warp.h"

// 0.34.1, the review's safe fixes (nothing heard or seen changes): the trim curve read in place
// (no allocation on the audio thread), undo snapshots without the plug-ins' settings (moved out,
// not copied), the recordings' caches emptied with the song (a same-named file in the next song
// is analysed afresh), the warped clips' cache counted by size.

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    constexpr double rate = 48000.0;

    void writeTones (const juce::File& file, const std::vector<double>& times, double seconds)
    {
        juce::AudioBuffer<float> sound (2, (int) (seconds * rate));
        sound.clear();
        for (const auto t : times)
            for (int i = 0; i < (int) (0.2 * rate) && (int) (t * rate) + i < sound.getNumSamples(); ++i)
            {
                const auto v = 0.8f * (float) (std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / rate) * std::exp (-i / (0.05 * rate)));
                sound.setSample (0, (int) (t * rate) + i, v);
                sound.setSample (1, (int) (t * rate) + i, v);
            }
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2)
                                                                           .withBitsPerSample (32)
                                                                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (sound, 0, sound.getNumSamples());
    }
}

TEST_CASE ("0.34.1: the trim curve reads the same as before, in place", "[performance]")
{
    // The old way: a lane made of the trim's points, read with valueAt (bent curves too).
    std::vector<AutomationPoint> trim { { 0.0, 0.5f }, { 2.0, 0.8f }, { 3.0, 0.2f }, { 5.0, 0.6f } };
    trim[2].curve = 0.7f;
    trim[3].curve = -0.4f;
    AutomationLane old;
    old.points = trim;
    for (auto beat = -1.0; beat <= 6.0; beat += 0.0625)
    {
        const auto before = (*old.valueAt (beat) - 0.5f) * 2.0f * AutomationLane::trimRange;
        CHECK (AutomationLane::trimDb (trim, beat) == before);   // (exactly: the same arithmetic)
    }
    CHECK (AutomationLane::trimDb ({}, 1.0) == 0.0f);
}

TEST_CASE ("0.34.1: an undo step leaves the plug-ins' settings in the song, and undo still works", "[performance]")
{
    Studio studio (false);
    studio.addTrack ({});
    // A song whose instrument has a megabyte of settings (as a sampler's would), opened.
    const auto songFile = studio.getAudioFolder().getParentDirectory().getChildFile ("undo states.pr7s");
    REQUIRE (studio.save (songFile));
    auto xml = juce::parseXML (songFile);
    REQUIRE (xml != nullptr);
    juce::MemoryBlock big (1 << 20);
    for (size_t i = 0; i < big.getSize(); ++i)
        big[i] = (char) (i * 7);
    auto* trackXml = xml->getChildByName ("Track");
    REQUIRE (trackXml != nullptr);
    trackXml->createNewChildElement ("InstrumentState")->addTextElement (big.toBase64Encoding());
    REQUIRE (xml->writeTo (songFile));
    juce::String problems;
    REQUIRE (studio.open (songFile, problems));
    const auto opened = studio.getProject().tracks.front().id;
    const auto state = studio.getProject().findTrack (opened)->instrument.state;
    REQUIRE (state == big);
    const auto trackIdOpened = opened;

    studio.setVolume (trackIdOpened, -6.0f);   // an undo step
    CHECK (studio.getProject().findTrack (trackIdOpened)->instrument.state == state);
    studio.setVolume (trackIdOpened, -12.0f);
    CHECK (studio.getProject().findTrack (trackIdOpened)->instrument.state == state);
    studio.undo();
    CHECK_THAT (studio.getProject().findTrack (trackIdOpened)->volumeDb, WithinAbs (-6.0, 1.0e-6));
    CHECK (studio.getProject().findTrack (trackIdOpened)->instrument.state == state);   // (undo doesn't take them either)
    studio.redo();
    CHECK_THAT (studio.getProject().findTrack (trackIdOpened)->volumeDb, WithinAbs (-12.0, 1.0e-6));
}

TEST_CASE ("0.34.1: a new song's recording of the same name is analysed afresh (hitpoints, pitch)", "[performance]")
{
    Studio studio (false);
    writeTones (studio.getAudioFolder().getChildFile ("Guitar 01.wav"), { 0.5, 1.5 }, 3.0);
    const auto first = studio.getHitpoints ("Guitar 01.wav");
    REQUIRE (first.size() == 2);
    REQUIRE (studio.getPitchAnalysis ("Guitar 01.wav") != nullptr);

    studio.newProject();
    writeTones (studio.getAudioFolder().getChildFile ("Guitar 01.wav"), { 0.25, 1.0, 2.0 }, 3.0);   // (another song's take)
    const auto second = studio.getHitpoints ("Guitar 01.wav");
    REQUIRE (second.size() == 3);
    CHECK_THAT (second[0], WithinAbs (0.25, 0.01));
}

TEST_CASE ("0.34.1: the warped clips' cache counts what it holds and is emptied with the rest", "[performance]")
{
    juce::AudioBuffer<float> recording (2, (int) (4.0 * rate));
    recording.clear();
    for (int i = 0; i < recording.getNumSamples(); ++i)
        recording.setSample (0, i, 0.1f * (float) std::sin (i * 0.05)), recording.setSample (1, i, recording.getSample (0, i));
    const auto tempo = Project().tempoMap();
    Part part;
    part.audioFile = "x.wav";
    part.length = 6.0;
    part.warp = { { 0.0, 0.0 }, { 3.0, 1.2 }, { 6.0, 3.0 } };
    WarpRenderer renderer;
    const auto a = renderer.render (part, recording, rate, tempo);
    REQUIRE (a != nullptr);
    CHECK (renderer.wholeSamplesHeld() == (juce::int64) a->getNumSamples() * a->getNumChannels());
    part.warp[1].beat = 2.5;   // (a marker dragged: a new whole clip)
    const auto b = renderer.render (part, recording, rate, tempo);
    CHECK (renderer.wholeSamplesHeld() == (juce::int64) (a->getNumSamples() + b->getNumSamples()) * 2);
    CHECK (renderer.render (part, recording, rate, tempo) == b);   // (the same one again: kept)
    renderer.clear();
    CHECK (renderer.wholeSamplesHeld() == 0);
}
