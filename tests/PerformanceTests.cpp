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

TEST_CASE ("0.34.2: audio clips mixed in one go in the middle give exactly the samples they gave one at a time", "[performance][engine]")
{
    // An awkward clip: off the beat, into its recording, fades ending between samples, a tempo
    // ramp under it (so every position comes from the map), played in odd block sizes.
    const auto sound = std::make_shared<juce::AudioBuffer<float>> (2, (int) (rate * 4));
    for (int i = 0; i < sound->getNumSamples(); ++i)
    {
        sound->setSample (0, i, (float) std::sin (i * 0.0123) * 0.7f + (float) i * 1.0e-7f);
        sound->setSample (1, i, (float) std::cos (i * 0.0071) * -0.6f);
    }
    for (const auto block : { 512, 333, 97 })
    {
        Engine engine;
        engine.prepare (rate, 512);
        engine.setAudioSource ([&sound] (const juce::String&) -> std::shared_ptr<const juce::AudioBuffer<float>> { return sound; });
        Project project;
        project.tempoTrackOn = true;
        project.tempoEvents = { { 0.0, 120.0, false }, { 2.0, 150.0, true }, { 4.0, 110.0, true } };
        Track track;
        track.id = 1;
        track.kind = Track::Kind::audio;
        Part clip;
        clip.start = 0.73;
        clip.length = 3.11;
        clip.audioFile = "take.wav";
        clip.audioOffset = 0.0133;
        clip.fadeIn = 0.01234;    // (592.3 samples)
        clip.fadeOut = 0.03117;   // (1496.2 samples)
        track.parts = { clip };
        project.tracks.push_back (track);
        engine.addAudioTrack (1);
        engine.update (project);
        engine.play();
        const auto total = (int) (rate * 3);
        std::vector<float> right;
        const auto left = renderSamples (engine, total, block, &right);

        // The old way, sample by sample (Engine::playClips before 0.34.2), through a 0 dB centre fader.
        const auto tempo = project.tempoMap();
        const auto sampleAt = [&] (double b) { return (juce::int64) std::llround (tempo.secondsAt (b) * rate); };
        const auto startSample = sampleAt (clip.start), endSample = sampleAt (clip.end());
        const auto offset = (juce::int64) std::llround (clip.audioOffset * rate);
        const auto fadeIn = clip.fadeIn * rate, fadeOut = clip.fadeOut * rate;
        const auto clipLength = (double) (endSample - startSample);
        auto mismatches = 0;
        for (juce::int64 s = 0; s < total; ++s)
        {
            float expectL = 0.0f, expectR = 0.0f;
            if (s >= startSample && s < endSample)
            {
                const auto into = (double) (s - startSample);
                const auto at = offset + (s - startSample);
                if (at >= 0 && at < sound->getNumSamples())
                {
                    auto gain = 1.0;
                    if (fadeIn > 0.0 && into < fadeIn) gain *= into / fadeIn;
                    if (fadeOut > 0.0 && clipLength - into < fadeOut) gain *= (clipLength - into) / fadeOut;
                    expectL = 0.0f + (float) (sound->getSample (0, (int) at) * gain);
                    expectR = 0.0f + (float) (sound->getSample (1, (int) at) * gain);
                }
            }
            if (left[(size_t) s] != expectL || right[(size_t) s] != expectR)
                ++mismatches;
        }
        CHECK (mismatches == 0);   // (exactly: not within a tolerance)
    }
}

TEST_CASE ("0.34.2: a fader moved keeps a track's events as they were; an edit to its notes builds them again", "[performance][engine]")
{
    Engine engine;
    engine.prepare (rate, 512);
    Project project;
    Track track;
    track.id = 1;
    Part part;
    part.length = 8.0;
    part.notes = { { 0.0, 1.0, 60, 0.8f }, { 2.0, 1.0, 64, 0.8f } };
    Controller bend;
    bend.type = Controller::Type::cc;
    bend.number = 1;
    bend.start = 0.5;
    bend.value = 100;
    part.controllers = { bend };
    track.parts = { part };
    project.tracks.push_back (track);
    auto probe = std::make_unique<Probe>();
    auto* heard = probe.get();
    engine.setInstrument (1, std::move (probe));
    engine.update (project);
    engine.play();
    renderSamples (engine, 12000, 512);   // (into the first note)

    // The fader moved: the same notes still play where they are (the list kept, nothing ended early).
    const auto builds = engine.getEventBuilds();
    project.tracks[0].volumeDb = -6.0f;
    engine.update (project);
    CHECK (engine.getEventBuilds() == builds);   // (kept, not built again)
    renderSamples (engine, 24000 * 3, 512);
    const auto on = heard->notesOn();
    REQUIRE (on.size() == 2);
    CHECK (on[0].sample == 0);
    CHECK (on[1].sample == 48000);              // beat 2 at 120 BPM
    REQUIRE (heard->notesOff().size() >= 1);
    CHECK (heard->notesOff()[0].sample == 24000);   // the first note's own end, not cut at the update

    // An edit: the second note moved up an octave plays as edited (built again).
    project.tracks[0].parts[0].notes = { { 0.0, 1.0, 60, 0.8f }, { 6.0, 1.0, 76, 0.8f } };
    engine.update (project);
    CHECK (engine.getEventBuilds() == builds + 1);
    renderSamples (engine, 24000 * 4, 512);
    CHECK (heard->notesOn().back().message.getNoteNumber() == 76);
    CHECK (heard->notesOn().back().sample == 24000 * 6);
}

TEST_CASE ("0.34.2 timing: an update after a fader move against one that builds every track's events", "[.][perf-update]")
{
    Engine engine;
    engine.prepare (rate, 512);
    Project project;
    for (int t = 1; t <= 16; ++t)
    {
        Track track;
        track.id = t;
        Part part;
        part.length = 512.0;
        for (int n = 0; n < 2000; ++n)
            part.notes.push_back ({ n * 0.25, 0.2, 40 + (n * 7 + t) % 40, 0.8f });
        for (int c = 0; c < 64; ++c)
        {
            Controller cc;
            cc.type = Controller::Type::cc;
            cc.number = 1;
            cc.start = c * 8.0;
            cc.value = (c % 2) * 127;
            cc.ramp = true;
            part.controllers.push_back (cc);
        }
        track.parts = { part };
        project.tracks.push_back (track);
        engine.setInstrument (t, std::make_unique<Probe>());
    }
    engine.update (project);
    const auto time = [&] (const std::function<void (int)>& change)
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < 30; ++i)
        {
            change (i);
            engine.update (project);
        }
        return (juce::Time::getMillisecondCounterHiRes() - start) / 30.0;
    };
    const auto fader = time ([&] (int i) { project.tracks[3].volumeDb = -1.0f * (float) (i % 10); });
    const auto rebuild = time ([&] (int i) { for (auto& t : project.tracks) t.parts[0].notes[0].pitch = 40 + i % 2; });
    WARN ("per update: fader move " << fader << " ms; every track's events built (as before 0.34.2) " << rebuild << " ms");
}

TEST_CASE ("0.57.2: an audio track's clips are kept after a fader move, built again when what they come from changes", "[engine][audio]")
{
    Engine engine;
    engine.prepare (rate, 512);
    // Every sample told apart: a ramp; another sound for the "new recording".
    auto ramp = std::make_shared<juce::AudioBuffer<float>> (2, 48000 * 8);
    auto other = std::make_shared<juce::AudioBuffer<float>> (2, 48000 * 8);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < ramp->getNumSamples(); ++i)
        {
            ramp->setSample (c, i, (float) i / (float) ramp->getNumSamples());
            other->setSample (c, i, 0.25f);
        }
    auto current = ramp;
    engine.setAudioSource ([&] (const juce::String&) -> std::shared_ptr<const juce::AudioBuffer<float>> { return current; });
    Project project;
    project.autoFades.crossfades = true;
    Track track;
    track.id = 1;
    track.kind = Track::Kind::audio;
    Part a, b;
    a.audioFile = b.audioFile = "take.wav";
    a.length = 4.0;
    b.start = 2.0;
    b.length = 4.0;
    b.audioOffset = 1.0;
    b.lane = 1;
    b.order = 1;
    track.parts = { a, b };
    project.tracks.push_back (track);
    engine.addAudioTrack (1);
    const auto play = [&]
    {
        engine.stop();
        engine.setPosition (0.0);
        engine.play();
        return renderSamples (engine, 24000 * 6);
    };
    engine.update (project);
    const auto first = play();
    const auto builds = engine.getClipBuilds();

    // A fader move: the same clips (kept), the same sound but the gain.
    project.tracks[0].volumeDb = -6.0f;
    engine.update (project);
    CHECK (engine.getClipBuilds() == builds);
    project.tracks[0].volumeDb = 0.0f;
    engine.update (project);
    CHECK (engine.getClipBuilds() == builds);
    CHECK (play() == first);

    // Each thing a clip is made from builds them again.
    project.tracks[0].parts[1].fadeIn = 0.05;
    engine.update (project);
    CHECK (engine.getClipBuilds() == builds + 1);
    project.tracks[0].soloLane = 0;
    engine.update (project);
    CHECK (engine.getClipBuilds() == builds + 2);
    project.autoFades.milliseconds = 40.0;
    engine.update (project);
    CHECK (engine.getClipBuilds() == builds + 3);
    project.tempoTrackOn = true;
    project.tempoEvents = { { 0.0, 100.0, false } };
    engine.update (project);
    CHECK (engine.getClipBuilds() == builds + 4);
    current = other;   // (a VariAudio edit: a new heard sound for the same file)
    engine.update (project);
    CHECK (engine.getClipBuilds() == builds + 5);
    const auto now = play();
    CHECK (std::abs (now[24000] - 0.25f) < 1.0e-6f);   // (the new sound plays)

    // Disabled and enabled again: built afresh.
    project.tracks[0].disabled = true;
    engine.update (project);
    project.tracks[0].disabled = false;
    engine.update (project);
    CHECK (engine.getClipBuilds() == builds + 6);
}

TEST_CASE ("0.57.3: the spike report: a slow block, the tracks that took the time, a late call, a run counted", "[engine]")
{
    Engine engine;
    engine.prepare (rate, 512);
    Project project;
    for (int t = 1; t <= 4; ++t)
    {
        Track track;
        track.id = t;
        track.name = "Track " + juce::String (t);
        project.tracks.push_back (track);
        auto probe = std::make_unique<Probe>();
        probe->busyMs = t == 3 ? 4.0 : 0.0;   // (track 3: the heavy plug-in)
        engine.setInstrument (t, std::move (probe));
    }
    engine.update (project);
    const auto budget = 512.0 / rate;
    juce::AudioBuffer<float> buffer (2, 512);

    // An easy block: no report.
    engine.render (buffer, 512);
    engine.noteSpike (budget * 0.5, 512, budget);
    CHECK_FALSE (engine.takeSpikeReport().has_value());

    // A slow one: the report, track 3 first.
    engine.render (buffer, 512);
    engine.noteSpike (budget * 2.0, 512, budget);
    auto report = engine.takeSpikeReport();
    REQUIRE (report.has_value());
    CHECK (report->load == 2.0f);
    CHECK_FALSE (report->missedLock);
    CHECK_FALSE (report->lateCall);
    CHECK (report->numTracks == 4);
    REQUIRE (report->numSlowest >= 1);
    CHECK (report->slowest[0].trackId == 3);
    CHECK (report->slowest[0].ms >= 4.0f);
    CHECK (report->tracksMs >= 4.0);
    CHECK_FALSE (engine.takeSpikeReport().has_value());   // (taken)

    // The driver late: reported though the block itself was quick; a second's pause isn't.
    engine.render (buffer, 512);
    engine.noteSpike (budget * 0.1, 512, budget * 3.0);
    report = engine.takeSpikeReport();
    REQUIRE (report.has_value());
    CHECK (report->lateCall);
    engine.noteSpike (budget * 0.1, 512, 2.0);
    CHECK_FALSE (engine.takeSpikeReport().has_value());

    // A run while one waits: the waiting one kept, the others counted in the next.
    engine.noteSpike (budget * 1.5, 512, budget);
    engine.noteSpike (budget * 3.0, 512, budget);
    engine.noteSpike (budget * 3.0, 512, budget);
    report = engine.takeSpikeReport();
    REQUIRE (report.has_value());
    CHECK (report->load == 1.5f);
    engine.noteSpike (budget, 512, budget);
    report = engine.takeSpikeReport();
    REQUIRE (report.has_value());
    CHECK (report->missedSince == 2);
}

TEST_CASE ("0.57.3: the spike report in words (the session log's line)", "[studio]")
{
    Studio studio (false);
    const auto id = studio.addTrack ({});
    Engine::SpikeReport r;
    r.load = 1.6f;
    r.blockMs = 17.1;
    r.budgetMs = 10.7;
    r.tracksMs = 15.0;
    r.numTracks = 1;
    r.masterMs = 0.4;
    r.helpers = 3;
    r.numSlowest = 1;
    r.slowest[0] = { id, 14.2f, 1.1f };
    const auto text = studio.describeSpike (r);
    CHECK (text.startsWith ("SPIKE: a block took 17.1 ms of its 10.7 ms (160%)"));
    CHECK (text.contains ("\"" + studio.getProject().findTrack (id)->name + "\""));
    CHECK (text.contains ("14.2 ms (usually 1.10 ms)"));
    CHECK (text.contains ("3 helper threads"));
    r.missedLock = true;
    CHECK (studio.describeSpike (r).startsWith ("SPIKE: one block of silence"));
    r.missedLock = false;
    r.lateCall = true;
    r.sinceLastMs = 32.0;
    CHECK (studio.describeSpike (r).contains ("the driver called 32.0 ms after the last block"));
}
