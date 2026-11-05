// Auto fades and crossfades (0.52.0, the manual's Auto Fades and Crossfades): where comped takes
// meet, a crossfade centred on the cut (or moved to the side that has sound); none between the
// halves of one split recording; fades at the events' ends when asked; Equal Power or Equal Gain;
// the song's settings and a track's own; the dialog; the bounce as it plays.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/AutoFadesPage.h"
#include "ui/Finish.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    constexpr double rate = 48000.0;
    constexpr int beat = 24000;   // samples at 120 BPM

    Part clip (double start, double length, const juce::String& file, double offset = 0.0, int order = 0)
    {
        Part p;
        p.start = start;
        p.length = length;
        p.audioFile = file;
        p.audioOffset = offset;
        p.order = order;
        return p;
    }

    Track audioTrack (std::vector<Part> parts)
    {
        Track t;
        t.id = 1;
        t.kind = Track::Kind::audio;
        t.parts = std::move (parts);
        return t;
    }

    const auto eightSeconds = [] (const Part&) { return 8.0; };

    std::shared_ptr<juce::AudioBuffer<float>> level (float value, double seconds)
    {
        auto sound = std::make_shared<juce::AudioBuffer<float>> (2, (int) (rate * seconds));
        for (int c = 0; c < 2; ++c)
            juce::FloatVectorOperations::fill (sound->getWritePointer (c), value, sound->getNumSamples());
        return sound;
    }

    void writeLevel (Studio& studio, const juce::String& name, float value, double seconds)
    {
        auto folder = studio.getAudioFolder();
        folder.createDirectory();
        const auto sound = level (value, seconds);
        std::unique_ptr<juce::OutputStream> stream (folder.getChildFile (name).createOutputStream());
        auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2)
                                                                           .withBitsPerSample (32)
                                                                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (*sound, 0, sound->getNumSamples());
    }

    // The biggest jump from one sample to the next between two beats.
    double biggestStep (const std::vector<float>& heard, double fromBeat, double toBeat)
    {
        double most = 0.0;
        for (auto i = (size_t) (fromBeat * beat) + 1; i < (size_t) (toBeat * beat); ++i)
            most = juce::jmax (most, (double) std::abs (heard[i] - heard[i - 1]));
        return most;
    }
}

TEST_CASE ("0.52.0: the crossfade's window: centred on the cut, moved to the side with sound, none inside one recording", "[autofades]")
{
    const TempoMap tempo (120.0);
    AutoFades fades;   // (the defaults: crossfades on, 10 ms, Equal Power)
    // Take b in front of take a from beat 4: a heard 0-4, b 4-8. A 10 ms crossfade, 5 ms (0.01 beat) each side.
    {
        const auto track = audioTrack ({ clip (0.0, 8.0, "a.wav"), clip (4.0, 4.0, "b.wav", 2.0, 1) });
        const auto a = fadedStretches (track, 0, fades, tempo, eightSeconds);
        const auto b = fadedStretches (track, 1, fades, tempo, eightSeconds);
        REQUIRE (a.size() == 1);
        REQUIRE (b.size() == 1);
        CHECK_THAT (a[0].from, WithinAbs (0.0, 1.0e-12));
        CHECK_THAT (a[0].fadeIn, WithinAbs (0.0, 1.0e-12));        // (its own start: no fade asked)
        CHECK_THAT (a[0].to, WithinAbs (4.01, 1.0e-9));
        CHECK_THAT (a[0].fadeOut, WithinAbs (0.01, 1.0e-12));
        CHECK_THAT (b[0].from, WithinAbs (3.99, 1.0e-9));
        CHECK_THAT (b[0].fadeIn, WithinAbs (0.01, 1.0e-12));
        CHECK_THAT (b[0].to, WithinAbs (8.0, 1.0e-12));
    }
    // b's recording begins at the cut (offset 0): nothing of it before 4, so the whole fade after the cut, on both.
    {
        const auto track = audioTrack ({ clip (0.0, 8.0, "a.wav"), clip (4.0, 4.0, "b.wav", 0.0, 1) });
        const auto a = fadedStretches (track, 0, fades, tempo, eightSeconds);
        const auto b = fadedStretches (track, 1, fades, tempo, eightSeconds);
        CHECK_THAT (a[0].to, WithinAbs (4.02, 1.0e-9));
        CHECK_THAT (a[0].fadeOut, WithinAbs (0.01, 1.0e-12));
        CHECK_THAT (b[0].from, WithinAbs (4.0, 1.0e-12));
        CHECK_THAT (b[0].fadeIn, WithinAbs (0.01, 1.0e-12));
    }
    // a heard before b's front stretch and heard again after it: a crossfade at both of b's ends.
    {
        const auto track = audioTrack ({ clip (0.0, 8.0, "a.wav"), clip (2.0, 2.0, "b.wav", 2.0, 1) });
        const auto a = fadedStretches (track, 0, fades, tempo, eightSeconds);
        REQUIRE (a.size() == 2);
        CHECK_THAT (a[0].to, WithinAbs (2.01, 1.0e-9));
        CHECK_THAT (a[1].from, WithinAbs (3.99, 1.0e-9));
        CHECK_THAT (a[1].fadeIn, WithinAbs (0.01, 1.0e-12));
    }
    // The halves of one split recording go on into each other: no fade.
    {
        const auto track = audioTrack ({ clip (0.0, 4.0, "a.wav"), clip (4.0, 4.0, "a.wav", 2.0) });
        const auto a = fadedStretches (track, 0, fades, tempo, eightSeconds);
        const auto b = fadedStretches (track, 1, fades, tempo, eightSeconds);
        CHECK_THAT (a[0].to, WithinAbs (4.0, 1.0e-12));
        CHECK_THAT (a[0].fadeOut, WithinAbs (0.0, 1.0e-12));
        CHECK_THAT (b[0].fadeIn, WithinAbs (0.0, 1.0e-12));
    }
    // Crossfades off: as before (a hard cut); fade in and out on: at each event's own ends, but not
    // where it has a fade of its own, and never longer than half of what's heard.
    {
        const auto track = audioTrack ({ clip (0.0, 8.0, "a.wav"), clip (4.0, 4.0, "b.wav", 2.0, 1) });
        AutoFades off;
        off.crossfades = false;
        CHECK_THAT (fadedStretches (track, 0, off, tempo, eightSeconds)[0].to, WithinAbs (4.0, 1.0e-12));
        off.fadeIn = off.fadeOut = true;
        off.milliseconds = 40.0;
        const auto a = fadedStretches (track, 0, off, tempo, eightSeconds);
        CHECK_THAT (a[0].fadeIn, WithinAbs (0.04, 1.0e-12));
        CHECK_THAT (a[0].fadeOut, WithinAbs (0.04, 1.0e-12));
        auto faded = track;
        faded.parts[0].fadeIn = 0.5;
        CHECK_THAT (fadedStretches (faded, 0, off, tempo, eightSeconds)[0].fadeIn, WithinAbs (0.0, 1.0e-12));
        const auto tiny = audioTrack ({ clip (0.0, 0.04, "a.wav") });   // 20 ms heard
        CHECK_THAT (fadedStretches (tiny, 0, off, tempo, eightSeconds)[0].fadeIn, WithinAbs (0.01, 1.0e-9));
    }
}

TEST_CASE ("0.52.0: the engine crossfades a comp cut: no jump, Equal Gain level, Equal Power's lift", "[autofades][engine]")
{
    Engine engine;
    engine.prepare (rate, 512);
    const auto quiet = level (0.25f, 4.0), loud = level (0.5f, 4.0), same = level (0.25f, 4.0);
    engine.setAudioSource ([&] (const juce::String& name) -> std::shared_ptr<const juce::AudioBuffer<float>>
                           { return name == "loud.wav" ? loud : name == "same.wav" ? same : quiet; });
    Project project;
    project.tracks.push_back (audioTrack ({ clip (0.0, 8.0, "quiet.wav"), clip (2.0, 2.0, "loud.wav", 1.0, 1) }));
    engine.addAudioTrack (1);
    const auto play = [&]
    {
        engine.stop();
        engine.update (project);
        engine.setPosition (0.0);
        engine.play();
        return renderSamples (engine, beat * 5);
    };

    // Crossfades off (as 0.51): a jump of 0.25 at the cut.
    project.autoFades.crossfades = false;
    CHECK (biggestStep (play(), 1.5, 2.5) > 0.24);

    // On (10 ms, Equal Power): from 0.25 to 0.5 over 480 samples, no step bigger than a few thousandths;
    // the level either side as it was.
    project.autoFades = {};
    auto heard = play();
    CHECK (biggestStep (heard, 1.5, 2.5) < 0.003);
    CHECK (biggestStep (heard, 3.5, 4.5) < 0.003);
    CHECK_THAT (heard[(size_t) (beat * 1.5)], WithinAbs (0.25, 1.0e-5));
    CHECK_THAT (heard[(size_t) (beat * 3)], WithinAbs (0.5, 1.0e-5));
    CHECK_THAT (heard[(size_t) (beat * 4.5)], WithinAbs (0.25, 1.0e-5));

    // The same level both sides: Equal Gain stays at it all through; Equal Power rises by up to
    // 1.414 in the middle (constant power, for takes that aren't the same sound).
    project.tracks[0].parts[1].audioFile = "same.wav";
    project.autoFades.equalPower = false;
    heard = play();
    for (auto i = (size_t) (beat * 2) - 240; i < (size_t) (beat * 2) + 240; ++i)
        CHECK_THAT (heard[i], WithinAbs (0.25, 1.0e-4));
    project.autoFades.equalPower = true;
    heard = play();
    CHECK_THAT (heard[(size_t) (beat * 2)], WithinAbs (0.25 * std::sqrt (2.0), 2.0e-3));

    // A track's own settings over the song's: crossfades off on it, the jump comes back.
    project.tracks[0].parts[1].audioFile = "loud.wav";
    auto own = AutoFades {};
    own.crossfades = false;
    project.tracks[0].autoFades = own;
    CHECK (biggestStep (play(), 1.5, 2.5) > 0.24);
}

TEST_CASE ("0.52.0: the settings: saved, a track's own, the dialog, and the bounce sounds as it plays", "[autofades][studio]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    const auto trackId = studio.addAudioTrack (false);
    const auto instrument = studio.getProject().tracks[0].id;
    AutoFades song;
    song.fadeIn = true;
    song.milliseconds = 900.0;   // (held to 500)
    studio.setAutoFades (song);
    CHECK_THAT (studio.getProject().autoFades.milliseconds, WithinAbs (500.0, 1.0e-9));
    AutoFades own;
    own.equalPower = false;
    studio.setTrackAutoFades (trackId, own);
    studio.setTrackAutoFades (instrument, own);   // (not an audio track: none)
    CHECK_FALSE (studio.getProject().findTrack (instrument)->autoFades);
    {
        const auto opened = Project::fromXml (*studio.getProject().toXml());
        REQUIRE (opened);
        CHECK (opened->autoFades.fadeIn);
        CHECK_THAT (opened->autoFades.milliseconds, WithinAbs (500.0, 1.0e-9));
        REQUIRE (opened->findTrack (trackId)->autoFades);
        CHECK_FALSE (opened->findTrack (trackId)->autoFades->equalPower);
        CHECK_FALSE (opened->findTrack (instrument)->autoFades);
    }

    // The dialog: the track's own settings shown; USE PROJECT SETTINGS back on: the song's again.
    {
        AutoFadesPage page (studio, trackId);
        CHECK (page.useProject.isVisible());
        CHECK_FALSE (page.usesProjectSettings());
        CHECK (page.equalGain.getToggleState());
        page.useProject.setToggleState (true, juce::dontSendNotification);
        page.apply();
        CHECK_FALSE (studio.getProject().findTrack (trackId)->autoFades);
        AutoFadesPage songPage (studio, 0);
        CHECK_FALSE (songPage.useProject.isVisible());
        songPage.fadeIn.setToggleState (false, juce::dontSendNotification);
        songPage.setLength (20.0);
        songPage.apply();
        CHECK_FALSE (studio.getProject().autoFades.fadeIn);
        CHECK_THAT (studio.getProject().autoFades.milliseconds, WithinAbs (20.0, 1.0e-9));
        CHECK (songPage.createComponentSnapshot (songPage.getLocalBounds()).isValid());
    }

    // Bounce: two takes comped (0.25 and 0.5): the new file crossfades at the cut, as playback does.
    writeLevel (studio, "a.wav", 0.25f, 4.0);
    writeLevel (studio, "b.wav", 0.5f, 4.0);
    studio.addPart (trackId, clip (0.0, 8.0, "a.wav"));
    auto take = clip (0.0, 8.0, "b.wav");
    take.lane = 1;
    studio.addPart (trackId, take);
    studio.compRange (trackId, 0, 0.0, 4.0);   // a over 0-4 (b, added last, in front after it)
    std::vector<int> all;
    for (const auto& p : studio.getProject().findTrack (trackId)->parts)
        all.push_back (p.id);
    juce::String error;
    REQUIRE (studio.bounceTakes (all, error) != 0);
    const auto& result = studio.getProject().findTrack (trackId)->parts[0];
    const auto data = studio.getAudio (result.audioFile);
    REQUIRE (data != nullptr);
    std::vector<float> left ((size_t) data->sound.getNumSamples());
    for (size_t i = 0; i < left.size(); ++i)
        left[i] = data->sound.getSample (0, (int) i);
    CHECK_THAT (left[(size_t) beat * 3], WithinAbs (0.25, 1.0e-5));
    CHECK_THAT (left[(size_t) beat * 5], WithinAbs (0.5, 1.0e-5));
    CHECK (biggestStep (left, 3.5, 4.5) < 0.003);
    studio.getAudioFolder().deleteRecursively();
}
