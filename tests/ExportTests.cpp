#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    constexpr int beat = 24000;   // samples at 48 kHz, 120 BPM

    struct Exported
    {
        juce::AudioBuffer<float> sound;
        double sampleRate = 0.0;
        unsigned int bits = 0;
    };

    Exported readBack (const juce::File& file)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        Exported e;
        if (reader == nullptr)
            return e;
        e.sampleRate = reader->sampleRate;
        e.bits = reader->bitsPerSample;
        e.sound.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&e.sound, 0, (int) reader->lengthInSamples, 0, true, true);
        return e;
    }

    // A studio with an audio track whose clip (a ramp: every sample told
    // apart) runs from beat 1 for `beats`, and its recording on disk.
    struct Song
    {
        Studio studio { false };
        int track = studio.addAudioTrack (false);
        juce::AudioBuffer<float> take;

        explicit Song (double beats = 1.0)
        {
            take.setSize (1, beat * 8);
            for (int i = 0; i < take.getNumSamples(); ++i)
                take.setSample (0, i, (float) i * 1.0e-6f);
            writeWav (studio.getAudioFolder().getChildFile ("take.wav"), take, 48000.0);
            Part clip { 1.0, beats, "Take", {} };
            clip.audioFile = "take.wav";
            studio.addPart (track, clip);
        }
        ~Song() { studio.getAudioFolder().deleteRecursively(); }
    };

    juce::File exportFile (const juce::String& name)
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-export-" + name + ".wav");
    }
}

TEST_CASE ("Export: the range to a WAV, to the sample, through what's heard; no click unless asked; the cycle ignored", "[export]")
{
    Song song;
    song.studio.setClick (true);          // on in the song: off in the export (the default)
    song.studio.setCycle (0.0, 1.0);
    song.studio.setCycleOn (true);        // ignored: the export plays straight through
    song.studio.setPosition (3.5);

    Studio::ExportOptions options;
    options.start = 0.0;
    options.end = 2.0;
    options.tailSeconds = 0.0;
    const auto file = exportFile ("range");
    juce::String error;
    REQUIRE (song.studio.exportMix (file, options, error));
    const auto result = readBack (file);
    CHECK (result.sampleRate == 48000.0);
    CHECK (result.bits == 24);
    REQUIRE (result.sound.getNumChannels() == 2);
    REQUIRE (result.sound.getNumSamples() == beat * 2);   // exactly two beats
    CHECK (result.sound.getMagnitude (0, 0, beat) == 0.0f);   // no click, nothing before the clip
    CHECK_THAT (result.sound.getSample (0, beat), WithinAbs (song.take.getSample (0, 0), 1.0e-6));
    CHECK_THAT (result.sound.getSample (0, beat + 1000), WithinAbs (song.take.getSample (0, 1000), 1.0e-6));
    CHECK_THAT (result.sound.getSample (1, beat + 1000), WithinAbs (song.take.getSample (0, 1000), 1.0e-6));   // mono: both sides

    // Everything as it was.
    CHECK (song.studio.getEngine().getSampleRate() == 48000.0);
    CHECK_THAT (song.studio.getEngine().getPosition(), WithinAbs (3.5, 1.0e-9));
    CHECK (song.studio.getProject().loopOn);
    CHECK (song.studio.getProject().click);
    CHECK_FALSE (song.studio.isExporting());

    // With the click asked for: the first beat's tick is there.
    options.click = true;
    REQUIRE (song.studio.exportMix (file, options, error));
    CHECK (readBack (file).sound.getMagnitude (0, 0, 1440) > 0.01f);
    file.deleteFile();
}

TEST_CASE ("Export: the ring-out lets what's sounding finish, as long as it lasts, and nothing new starts in it", "[export]")
{
    // A clip running on past the end: cut at the end, so nothing rings out (no tail at all).
    Song song (4.0);
    Studio::ExportOptions options;
    options.end = 2.0;
    const auto file = exportFile ("tail");
    juce::String error;
    REQUIRE (song.studio.exportMix (file, options, error));
    auto result = readBack (file);
    CHECK (result.sound.getNumSamples() == beat * 2);
    CHECK (result.sound.getMagnitude (0, beat + 100, 1000) > 0.0f);

    // A synth note held across the end rings on into the ring-out; a part after the end isn't heard.
    Studio synth (false);
    const auto keys = synth.getProject().tracks[0].id;
    synth.addPart (keys, { 0.0, 4.0, "Keys", { { 1.5, 2.0, 60, 0.9f } } });
    synth.addPart (keys, { 3.0, 4.0, "Later", { { 0.0, 1.0, 72, 0.9f } } });
    // The instruments are told it's offline while it's written, and back to real time after.
    REQUIRE (synth.beginExport (file, options, error));
    bool offline = false;
    REQUIRE (synth.renderExport ([&] (double) { offline = synth.getEngine().getInstrument (keys)->isNonRealtime(); return true; }));
    synth.endExport();
    CHECK (offline);
    CHECK_FALSE (synth.getEngine().getInstrument (keys)->isNonRealtime());
    result = readBack (file);
    CHECK (rmsOf (std::vector<float> (result.sound.getReadPointer (0), result.sound.getReadPointer (0) + result.sound.getNumSamples()),
                  (size_t) beat * 2, (size_t) beat * 2 + 2000) > 0.001f);
    // It ends when the note's release has died away (the synth's: a quarter of a second), not 30 s on.
    CHECK (result.sound.getNumSamples() > beat * 2 + 2000);
    CHECK (result.sound.getNumSamples() < beat * 2 + 48000);
    CHECK (result.sound.getMagnitude (0, result.sound.getNumSamples() - 1, 1) > 0.0f);   // up to its last sound

    // 32-bit float.
    options.bitsPerSample = 32;
    REQUIRE (synth.exportMix (file, options, error));
    CHECK (readBack (file).bits == 32);
    file.deleteFile();
}

TEST_CASE ("Export: another sample rate, 16-bit, and a cancelled export leaves no file", "[export]")
{
    Song song;
    Studio::ExportOptions options;
    options.end = 2.0;
    options.tailSeconds = 0.0;
    options.sampleRate = 44100.0;
    options.bitsPerSample = 16;
    const auto file = exportFile ("rate");
    juce::String error;
    REQUIRE (song.studio.exportMix (file, options, error));
    const auto result = readBack (file);
    CHECK (result.sampleRate == 44100.0);
    CHECK (result.bits == 16);
    CHECK (result.sound.getNumSamples() == 44100);   // two beats: one second
    // The clip (from beat 1, half a second in) resampled: 100 samples into it at 44.1 kHz is 108.8 at 48.
    CHECK_THAT (result.sound.getSample (0, 22050 + 100), WithinAbs (song.take.getSample (0, 0) + 108.84e-6, 4.0e-5));
    CHECK (song.studio.getEngine().getSampleRate() == 48000.0);   // back
    // And it plays as before.
    song.studio.setPosition (1.0);
    song.studio.play();
    const auto heard = renderSamples (song.studio.getEngine(), 512);
    song.studio.stop();
    CHECK_THAT (heard[100], WithinAbs (song.take.getSample (0, 100), 1.0e-6));

    // Cancelled half way: no file left.
    options.sampleRate = 0.0;
    options.end = 6.0;
    REQUIRE (song.studio.beginExport (file, options, error));
    CHECK (song.studio.isExporting());
    CHECK_FALSE (song.studio.renderExport ([] (double progress) { return progress < 0.3; }));
    song.studio.endExport();
    CHECK_FALSE (file.exists());
    CHECK_FALSE (song.studio.isExporting());

    // An empty range: nothing to export.
    options.end = options.start;
    CHECK_FALSE (song.studio.exportMix (file, options, error));
    CHECK (error.isNotEmpty());
}

TEST_CASE ("Export: the levels as set from the first sample (a fader just moved doesn't glide in the file)", "[export]")
{
    Song song;
    song.studio.setVolume (song.track, -12.0f);   // straight before exporting: the engine was still at 0 dB
    Studio::ExportOptions options;
    options.start = 1.0;
    options.end = 2.0;
    options.tailSeconds = 0.0;
    const auto file = exportFile ("settled");
    juce::String error;
    REQUIRE (song.studio.exportMix (file, options, error));
    const auto exported = readBack (file);
    REQUIRE (exported.sound.getNumSamples() >= 100);
    const auto gain = std::pow (10.0f, -12.0f / 20.0f);
    for (const auto i : { 10, 50, 99 })
        CHECK_THAT (exported.sound.getSample (0, i), WithinAbs (song.take.getSample (0, i) * gain, 1.0e-6));
    file.deleteFile();
}

namespace
{
    // As Kontakt or Superior Drummer: it reads realtime or offline when it's set
    // up (prepareToPlay), and plays nothing if it's marked otherwise later
    // without being set up again.
    struct ModeMinded final : TestPlugin
    {
        ModeMinded() : TestPlugin (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)) { instrument = true; }
        void prepareToPlay (double, int) override { preparedOffline = isNonRealtime(); ++prepared; }
        void releaseResources() override { ++released; }
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
        {
            buffer.clear();
            if (isNonRealtime() == preparedOffline)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    buffer.setSample (0, i, 0.5f), buffer.setSample (1, i, 0.5f);
        }
        bool preparedOffline = false;
        int prepared = 0, released = 0;
    };
}

TEST_CASE ("Export: plugins are set up again for offline rendering, and back after (Kontakt rendered silence)", "[export]")
{
    Studio studio (false);
    const auto track = studio.getProject().tracks[0].id;
    studio.addPart (track, { 1.0, 2.0, "Riff", { { 0.0, 2.0, 60, 0.8f } } });
    auto plugin = std::make_unique<ModeMinded>();
    auto* minded = plugin.get();
    studio.getEngine().setInstrument (track, std::move (plugin));
    REQUIRE_FALSE (minded->preparedOffline);

    Studio::ExportOptions options;
    options.start = 1.0;
    options.end = 2.0;
    options.tailSeconds = 0.0;   // the same rate as the engine's: no re-prepare for that
    const auto file = exportFile ("offline");
    juce::String error;
    REQUIRE (studio.exportMix (file, options, error));
    const auto exported = readBack (file);
    REQUIRE (exported.sound.getNumSamples() >= 100);
    CHECK (exported.sound.getMagnitude (0, 0, exported.sound.getNumSamples()) > 0.1f);
    // Back to realtime, set up for it.
    CHECK_FALSE (minded->isNonRealtime());
    CHECK_FALSE (minded->preparedOffline);
    CHECK (minded->released >= 2);
    file.deleteFile();
}
