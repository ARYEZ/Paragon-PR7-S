#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    // A file written with JUCE's own writers: `seconds` of a ramp (every sample told apart), mono or stereo.
    juce::File writeFile (juce::AudioFormat& format, const juce::File& file, double rate, int channels, double seconds, int bits = 24)
    {
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate)
                                                                                     .withNumChannels (channels)
                                                                                     .withBitsPerSample (bits));
        REQUIRE (writer != nullptr);
        const auto samples = (int) (rate * seconds);
        juce::AudioBuffer<float> buffer (channels, samples);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < samples; ++i)
                buffer.setSample (c, i, (float) ((i % 1000) - 500) / 1000.0f * (c == 0 ? 1.0f : -1.0f));
        writer->writeFromAudioSampleBuffer (buffer, 0, samples);
        return file;
    }
}

TEST_CASE ("Audio file import: WAV, AIFF, FLAC, Ogg and MP3 copied into the song's audio folder as clips; onto an audio track or a new one; several one after another", "[import][audio]")
{
    Studio studio (false);
    const auto spb = studio.getProject().secondsPerBeat();   // 0.5 s at 120 BPM
    const auto from = juce::File::createTempFile ("pr7s-import");
    from.createDirectory();
    juce::WavAudioFormat wav;
    juce::AiffAudioFormat aiff;
    juce::FlacAudioFormat flac;
    juce::OggVorbisAudioFormat ogg;
    const auto stereoWav = writeFile (wav, from.getChildFile ("Drums.wav"), 44100.0, 2, 2.0);
    const auto monoAiff = writeFile (aiff, from.getChildFile ("Vox.aif"), 48000.0, 1, 1.0);
    const auto flacFile = writeFile (flac, from.getChildFile ("Keys.flac"), 44100.0, 2, 1.5);
    const auto oggFile = writeFile (ogg, from.getChildFile ("Pad.ogg"), 44100.0, 2, 1.0, 16);
    const auto mp3 = juce::File (PR7S_TEST_DATA).getChildFile ("tone.mp3");
    REQUIRE (mp3.existsAsFile());

    SECTION ("A stereo WAV with no audio track selected: a new stereo audio track, the clip at the beat, as long as the file")
    {
        juce::String error;
        const auto track = studio.importAudio ({ stereoWav }, studio.getProject().tracks[0].id, 4.0, error);
        REQUIRE (track != 0);
        CHECK (error.isEmpty());
        const auto* t = studio.getProject().findTrack (track);
        REQUIRE (t != nullptr);
        CHECK (t->isAudio());
        CHECK (t->stereoInput);
        REQUIRE (t->parts.size() == 1);
        const auto clip = t->parts[0];   // (a copy: importing again below moves the list)
        CHECK (clip.name == "Drums");
        CHECK (clip.start == 4.0);
        CHECK_THAT (clip.length, WithinAbs (2.0 / spb, 1.0e-6));
        // Copied into the song's audio folder (the original stays where it was), and played from there.
        const auto copy = studio.getAudioFolder().getChildFile (clip.audioFile);
        CHECK (copy.existsAsFile());
        CHECK (copy != stereoWav);
        CHECK (stereoWav.existsAsFile());
        REQUIRE (studio.getAudio (clip.audioFile) != nullptr);
        CHECK_THAT (studio.getAudio (clip.audioFile)->seconds(), WithinAbs (2.0, 0.01));
        // Again: a name of its own.
        studio.importAudio ({ stereoWav }, track, 8.0, error);
        REQUIRE (t->parts.size() == 2);
        CHECK (t->parts[1].audioFile != clip.audioFile);
        // One undo step takes the clips away.
        studio.undo();
        CHECK (studio.getProject().findTrack (track)->parts.size() == 1);
    }

    SECTION ("Onto a selected audio track; a mono AIFF; several files one after another")
    {
        const auto audio = studio.addAudioTrack (false);
        juce::String error;
        CHECK (studio.importAudio ({ monoAiff, flacFile, oggFile }, audio, 2.0, error) == audio);
        CHECK (error.isEmpty());
        const auto& parts = studio.getProject().findTrack (audio)->parts;
        REQUIRE (parts.size() == 3);
        CHECK (parts[0].start == 2.0);
        CHECK_THAT (parts[1].start, WithinAbs (2.0 + 1.0 / spb, 1.0e-6));
        CHECK_THAT (parts[2].start, WithinAbs (2.0 + 2.5 / spb, 1.0e-6));
        // The compressed ones were written as WAVs in the folder.
        CHECK (parts[1].audioFile.endsWithIgnoreCase (".wav"));
        CHECK (parts[2].audioFile.endsWithIgnoreCase (".wav"));
        CHECK (parts[0].audioFile.endsWithIgnoreCase (".aif"));
        for (const auto& part : parts)
            CHECK (studio.getAudio (part.audioFile) != nullptr);
        // The FLAC's samples came across as they were (lossless).
        const auto data = studio.getAudio (parts[1].audioFile);
        REQUIRE (data != nullptr);
        if (juce::approximatelyEqual (studio.getEngine().getSampleRate(), 44100.0))
            CHECK_THAT (data->sound.getSample (0, 123), WithinAbs ((float) (123 - 500) / 1000.0f, 1.0e-4));
    }

    SECTION ("MP3 (a half-second tone): read, written as a WAV, a clip half a second long")
    {
        juce::String error;
        const auto track = studio.importAudio ({ mp3 }, 0, 0.0, error);
        REQUIRE (track != 0);
        const auto& clip = studio.getProject().findTrack (track)->parts.at (0);
        CHECK (clip.audioFile == "tone.wav");
        CHECK_THAT (clip.length * spb, WithinAbs (0.5, 0.06));   // (MP3 pads its start and end a little)
        REQUIRE (studio.getAudio (clip.audioFile) != nullptr);
        auto peak = 0.0f;
        const auto& sound = studio.getAudio (clip.audioFile)->sound;
        for (int i = 0; i < sound.getNumSamples(); ++i)
            peak = juce::jmax (peak, std::abs (sound.getSample (0, i)));
        CHECK_THAT (peak, WithinAbs (0.0841f, 0.004f));   // the file's own peak (-21.5 dB, as ffmpeg measures it): not silence, nothing lost
    }

    SECTION ("A file that isn't audio: not imported, and said so; the others still are")
    {
        const auto junk = from.getChildFile ("notes.wav");
        junk.replaceWithText ("not audio");
        juce::String error;
        const auto track = studio.importAudio ({ junk, monoAiff }, 0, 0.0, error);
        CHECK (track != 0);
        CHECK (error.contains ("notes.wav"));
        CHECK (studio.getProject().findTrack (track)->parts.size() == 1);
        juce::String none;
        CHECK (studio.importAudio ({ junk }, 0, 0.0, none) == 0);
        CHECK (none.isNotEmpty());
    }

    from.deleteRecursively();
    studio.getAudioFolder().deleteRecursively();
}

TEST_CASE ("Several audio files, a track each (the manual's Import Files on One Track off): all at the same place, each track named after its file", "[import][audio]")
{
    Studio studio (false);
    const auto from = juce::File::createTempFile ("pr7s-import-each");
    from.createDirectory();
    juce::WavAudioFormat wav;
    const juce::Array<juce::File> files { writeFile (wav, from.getChildFile ("Kick.wav"), 48000.0, 1, 1.0),
                                          writeFile (wav, from.getChildFile ("Snare.wav"), 48000.0, 2, 2.0),
                                          writeFile (wav, from.getChildFile ("Hats.wav"), 48000.0, 2, 0.5) };
    const auto before = studio.getProject().tracks.size();
    juce::String error;
    const auto first = studio.importAudio (files, 0, 8.0, error, false);
    CHECK (error.isEmpty());
    REQUIRE (first != 0);
    const auto& tracks = studio.getProject().tracks;
    REQUIRE (tracks.size() == before + 3);
    for (size_t i = before; i < tracks.size(); ++i)
    {
        CHECK (tracks[i].isAudio());
        REQUIRE (tracks[i].parts.size() == 1);
        CHECK (tracks[i].parts[0].start == 8.0);
    }
    CHECK (tracks[before + 1].name == "Snare");
    CHECK (tracks[before + 1].stereoInput);
    CHECK (tracks[before + 2].name == "Hats");
    CHECK_FALSE (tracks[before].stereoInput);   // the first: mono, as Kick is
    from.deleteRecursively();
    studio.getAudioFolder().deleteRecursively();
}
