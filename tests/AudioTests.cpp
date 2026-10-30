#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    constexpr double rate = 48000.0;
    constexpr juce::int64 beat = 24000;   // samples, at 120 BPM

    // A recording for tests: left rises a step each sample, right falls (so every sample is told apart).
    std::shared_ptr<juce::AudioBuffer<float>> ramp (int samples)
    {
        auto sound = std::make_shared<juce::AudioBuffer<float>> (2, samples);
        for (int i = 0; i < samples; ++i)
        {
            sound->setSample (0, i, (float) i * 1.0e-5f);
            sound->setSample (1, i, (float) i * -1.0e-5f);
        }
        return sound;
    }

    // An engine with one audio track (id 1) whose clips are made from `sound`.
    struct Tape
    {
        Engine engine;
        Project project;
        std::shared_ptr<juce::AudioBuffer<float>> sound = ramp ((int) rate * 4);

        Tape()
        {
            engine.prepare (rate, 512);
            engine.setAudioSource ([this] (const juce::String&) -> std::shared_ptr<const juce::AudioBuffer<float>> { return sound; });
            Track track;
            track.id = 1;
            track.kind = Track::Kind::audio;
            project.tracks.push_back (track);
            engine.addAudioTrack (1);
            engine.update (project);
        }
        void setClip (Part clip)
        {
            clip.audioFile = "take.wav";
            project.tracks[0].parts = { clip };
            engine.update (project);
        }
    };
}

TEST_CASE ("An audio clip plays from its beat, from its offset into the recording, to the sample", "[engine][audio]")
{
    Tape tape;
    Part clip { 1.0, 1.0, "Take", {} };
    clip.audioOffset = 0.1;   // 4800 samples in
    tape.setClip (clip);
    tape.engine.play();
    std::vector<float> right;
    const auto left = renderSamples (tape.engine, (int) beat * 3, 512, &right);

    const auto& source = *tape.sound;
    CHECK (left[(size_t) beat - 1] == 0.0f);                                      // nothing before the clip
    CHECK (left[(size_t) beat] == source.getSample (0, 4800));                    // its first sample, on the beat
    CHECK (right[(size_t) beat] == source.getSample (1, 4800));
    CHECK (left[(size_t) beat * 2 - 1] == source.getSample (0, 4800 + (int) beat - 1));   // its last
    CHECK (left[(size_t) beat * 2] == 0.0f);                                      // and nothing after
}

TEST_CASE ("Audio clips fade in and out, and replay round the cycle", "[engine][audio]")
{
    Tape tape;
    Part clip { 0.0, 2.0, "Take", {} };
    clip.fadeIn = 0.01;    // 480 samples
    clip.fadeOut = 0.02;   // 960 samples
    tape.setClip (clip);
    tape.engine.play();
    const auto left = renderSamples (tape.engine, (int) beat * 2);
    const auto& source = *tape.sound;
    CHECK (left[0] == 0.0f);
    CHECK_THAT (left[240], WithinAbs (source.getSample (0, 240) * 0.5, 1.0e-6));       // halfway through the fade in
    CHECK_THAT (left[1000], WithinAbs (source.getSample (0, 1000), 1.0e-6));           // full
    const auto last = (size_t) beat * 2 - 480;                                          // halfway through the fade out
    CHECK_THAT (left[last], WithinAbs (source.getSample (0, (int) last) * 0.5, 1.0e-6));

    // In a cycle over its first beat: the second pass starts from the clip's start again.
    Tape round;
    round.setClip ({ 0.0, 4.0, "Take", {} });
    round.project.loopOn = true;
    round.project.loopStart = 0.0;
    round.project.loopEnd = 1.0;
    round.engine.update (round.project);
    round.engine.play();
    const auto looped = renderSamples (round.engine, (int) beat * 2);
    CHECK (looped[(size_t) beat] == round.sound->getSample (0, 0));
    CHECK (looped[(size_t) beat + 100] == round.sound->getSample (0, 100));
}

TEST_CASE ("Monitoring: an audio track hears its input (one, or a pair), playing or not", "[engine][audio]")
{
    Tape tape;
    juce::AudioBuffer<float> input (2, 4096);
    for (int i = 0; i < 4096; ++i)
    {
        input.setSample (0, i, 0.3f);
        input.setSample (1, i, 0.6f);
    }
    juce::AudioBuffer<float> output (2, 4096);

    // Off: nothing.
    tape.engine.render (output, 4096, &input);
    CHECK (output.getMagnitude (0, 4096) == 0.0f);

    // On, the second input (mono): on both sides.
    tape.project.tracks[0].monitor = true;
    tape.project.tracks[0].input = 1;
    tape.engine.update (tape.project);
    tape.engine.render (output, 4096, &input);
    CHECK_THAT (output.getSample (0, 4000), WithinAbs (0.6, 1.0e-6));
    CHECK_THAT (output.getSample (1, 4000), WithinAbs (0.6, 1.0e-6));

    // A stereo pair from the first: left and right.
    tape.project.tracks[0].input = 0;
    tape.project.tracks[0].stereoInput = true;
    tape.engine.update (tape.project);
    tape.engine.render (output, 4096, &input);
    CHECK_THAT (output.getSample (0, 4000), WithinAbs (0.3, 1.0e-6));
    CHECK_THAT (output.getSample (1, 4000), WithinAbs (0.6, 1.0e-6));

    // Through the track's effects (a guitar amp, say) and its fader.
    tape.engine.insertEffect (1, 0, timesEffect (2.0f));
    tape.project.tracks[0].inserts.resize (1);
    tape.engine.update (tape.project);
    tape.engine.render (output, 4096, &input);
    CHECK_THAT (output.getSample (0, 4000), WithinAbs (0.6, 1.0e-6));
}

TEST_CASE ("Recording audio: what comes in becomes a clip where it was played, its file in the song's audio folder", "[studio][audio]")
{
    Studio studio (false);
    const auto trackId = studio.addAudioTrack (false);
    CHECK (studio.getProject().findTrack (trackId)->name == "Audio 1");
    CHECK (studio.getProject().armedTrack()->id == trackId);
    studio.setLatencyForTests (0.01);   // the interface's delay: 480 samples

    // Recording from beat 4 (sample 96000): a second of the input.
    const auto played = ramp ((int) rate);
    studio.setPosition (4.0);
    studio.record();
    juce::AudioBuffer<float> output (2, (int) rate);
    studio.getEngine().render (output, (int) rate, played.get());
    studio.stop();

    REQUIRE (studio.getProject().findTrack (trackId)->parts.size() == 1);
    const auto clip = studio.getProject().findTrack (trackId)->parts[0];
    CHECK (clip.isAudio());
    CHECK_THAT (clip.start, WithinAbs (4.0, 1.0e-9));
    CHECK_THAT (clip.audioOffset, WithinAbs (0.01, 1.0e-9));               // the delay skipped
    CHECK_THAT (clip.length, WithinAbs ((1.0 - 0.01) / 0.5, 1.0e-6));       // the rest of the second, in beats
    const auto file = studio.getAudioFolder().getChildFile (clip.audioFile);
    CHECK (file.existsAsFile());
    CHECK (clip.audioFile == "Audio 1 01.wav");

    // The file holds exactly what came in (to 24-bit precision).
    const auto data = studio.getAudio (clip.audioFile);
    REQUIRE (data != nullptr);
    REQUIRE (data->sound.getNumSamples() == (int) rate);
    for (const int i : { 0, 1, 480, 20000, 47999 })
        CHECK_THAT (data->sound.getSample (0, i), WithinAbs (played->getSample (0, i), 1.0e-6));

    // Played back: at its beat, from what came in 480 samples after recording began.
    studio.setPosition (4.0);
    studio.play();
    const auto heard = renderSamples (studio.getEngine(), 4096);
    studio.stop();
    CHECK_THAT (heard[0], WithinAbs (played->getSample (0, 480), 1.0e-6));

    // One undo takes it away; redo brings it back.
    studio.undo();
    CHECK (studio.getProject().findTrack (trackId)->parts.empty());
    studio.redo();
    CHECK (studio.getProject().findTrack (trackId)->parts.size() == 1);

    // Saved: the recording goes into "<song> Audio" beside the song; opened, it plays.
    const auto folder = juce::File::createTempFile ("songs");
    folder.createDirectory();
    const auto song = folder.getChildFile ("Riff.pr7s");
    REQUIRE (studio.save (song));
    CHECK (folder.getChildFile ("Riff Audio").getChildFile ("Audio 1 01.wav").existsAsFile());
    Studio other (false);
    juce::String problems;
    REQUIRE (other.open (song, problems));
    CHECK (problems.isEmpty());
    CHECK (other.getProject().findTrack (trackId)->isAudio());
    CHECK (other.getAudio ("Audio 1 01.wav") != nullptr);

    // A song whose recording has gone: it opens, and says so.
    folder.getChildFile ("Riff Audio").getChildFile ("Audio 1 01.wav").deleteFile();
    Studio third (false);
    REQUIRE (third.open (song, problems));
    CHECK (problems.contains ("Audio 1 01.wav"));
    CHECK (third.getProject().findTrack (trackId)->parts.size() == 1);   // the clip stays, silent
    folder.deleteRecursively();
    studio.getAudioFolder().deleteRecursively();
}

TEST_CASE ("Recording audio in a cycle keeps the first pass; recording nothing leaves no clip", "[studio][audio]")
{
    Studio studio (false);
    const auto trackId = studio.addAudioTrack (true);
    studio.setCycle (0.0, 2.0);
    studio.setCycleOn (true);
    const auto played = ramp ((int) beat * 4);
    studio.record();
    juce::AudioBuffer<float> output (2, (int) beat * 4);
    studio.getEngine().render (output, (int) beat * 4, played.get());
    studio.stop();
    REQUIRE (studio.getProject().findTrack (trackId)->parts.size() == 1);
    const auto clip = studio.getProject().findTrack (trackId)->parts[0];
    CHECK_THAT (clip.start, WithinAbs (0.0, 1.0e-9));
    CHECK_THAT (clip.length, WithinAbs (2.0, 1.0e-6));   // the cycle's two beats, not four
    const auto data = studio.getAudio (clip.audioFile);
    REQUIRE (data != nullptr);
    CHECK (data->sound.getNumSamples() == (int) beat * 2);
    CHECK (data->sound.getNumChannels() == 2);
    CHECK_THAT (data->sound.getSample (1, 100), WithinAbs (played->getSample (1, 100), 1.0e-6));   // stereo: the second input on the right

    // Record pressed and stopped at once (no audio went by): no clip, no file.
    studio.setCycleOn (false);
    const auto before = studio.getAudioFolder().getNumberOfChildFiles (juce::File::findFiles);
    studio.record();
    studio.stop();
    CHECK (studio.getProject().findTrack (trackId)->parts.size() == 1);
    CHECK (studio.getAudioFolder().getNumberOfChildFiles (juce::File::findFiles) == before);
    studio.getAudioFolder().deleteRecursively();
}
