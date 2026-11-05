#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/Studio.h"
#include "engine/DragOut.h"

// Parts dragged out of the arrangement as files (0.53.1): a MIDI part as a .mid, an audio event
// as a .wav of what it plays; the song untouched.

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    // A recording whose every sample is told apart: sample i is i * 1e-6 (left), minus that (right).
    void writeRamp (Studio& studio, const juce::String& name, double rate, double seconds)
    {
        auto folder = studio.getAudioFolder();
        folder.createDirectory();
        const auto samples = (int) (rate * seconds);
        juce::AudioBuffer<float> sound (2, samples);
        for (int i = 0; i < samples; ++i)
        {
            sound.setSample (0, i, (float) i * 1.0e-6f);
            sound.setSample (1, i, (float) i * -1.0e-6f);
        }
        folder.getChildFile (name).deleteFile();
        std::unique_ptr<juce::OutputStream> stream (folder.getChildFile (name).createOutputStream());
        auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2)
                                                                           .withBitsPerSample (32)
                                                                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (sound, 0, samples);
    }

    juce::AudioBuffer<float> readWav (const juce::File& file, double& rate, juce::StringPairArray& metadata)
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (file.createInputStream().release(), true));
        REQUIRE (reader != nullptr);
        rate = reader->sampleRate;
        metadata = reader->metadataValues;
        juce::AudioBuffer<float> sound ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&sound, 0, (int) reader->lengthInSamples, 0, true, true);
        return sound;
    }

    juce::String songText (const Studio& studio) { return studio.getProject().toXml()->toString(); }
}

TEST_CASE ("Drag out: a MIDI part becomes a .mid of its own, from beat 0, as heard, with the tempo (0.53.1)", "[dragout]")
{
    Studio studio (false);
    studio.setTempo (140.0);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.renameTrack (trackId, "Rhythm Gtr");
    Part riff { 4.0, 4.0, "Verse riff", { { 0.5, 0.5, 40, 0.5f }, { 2.0, 1.0, 45, 0.8f }, { 3.5, 2.0, 47, 0.6f } } };
    riff.transpose = 2;
    riff.velocityOffset = 10;
    studio.addPart (trackId, riff);
    const auto partId = studio.getProject().tracks[0].parts.back().id;
    const auto before = songText (studio);
    const auto couldUndo = studio.canUndo();

    juce::String error;
    const auto file = studio.writePartForDrag (partId, error);
    INFO (error);
    REQUIRE (file.existsAsFile());
    CHECK (file.getFileName() == "Rhythm Gtr - Verse riff.mid");
    CHECK (file.isAChildOf (dragOutFolder()));
    // The song untouched, no undo step.
    CHECK (songText (studio) == before);
    CHECK (studio.canUndo() == couldUndo);

    juce::MidiFile midi;
    juce::FileInputStream in (file);
    REQUIRE (midi.readFrom (in));
    CHECK (midi.getTimeFormat() == 480);
    REQUIRE (midi.getNumTracks() == 2);
    // The tempo first, at 0.
    const auto* conductor = midi.getTrack (0);
    auto tempoAt0 = 0.0;
    for (int i = 0; i < conductor->getNumEvents(); ++i)
        if (const auto& m = conductor->getEventPointer (i)->message; m.isTempoMetaEvent() && m.getTimeStamp() == 0.0)
            tempoAt0 = 60.0 / m.getTempoSecondsPerQuarterNote();
    CHECK_THAT (tempoAt0, WithinAbs (140.0, 0.01));
    // The notes from 0, transposed and velocity-offset as heard, cut at the part's end.
    std::vector<std::tuple<double, double, int, int>> notes;
    const auto* seq = midi.getTrack (1);
    for (int i = 0; i < seq->getNumEvents(); ++i)
        if (const auto* e = seq->getEventPointer (i); e->message.isNoteOn())
            notes.emplace_back (e->message.getTimeStamp(), e->noteOffObject->message.getTimeStamp(), e->message.getNoteNumber(), e->message.getVelocity());
    REQUIRE (notes.size() == 3);
    CHECK (notes[0] == std::tuple<double, double, int, int> { 240.0, 480.0, 42, 74 });   // 0.5 x 127 + 10 = 73.5: 74
    CHECK (notes[1] == std::tuple<double, double, int, int> { 960.0, 1440.0, 47, 112 });
    CHECK (notes[2] == std::tuple<double, double, int, int> { 1680.0, 1920.0, 49, 86 });   // (cut at the part's end, beat 4)

    // A tempo track: the tempo at the part's start, and the changes inside it.
    studio.addTempoTrack();
    studio.setTempoEvents ({ { 0.0, 100.0, false }, { 6.0, 160.0, false } });
    const auto withChange = studio.writePartForDrag (partId, error);
    REQUIRE (withChange.existsAsFile());
    CHECK (withChange != file);   // (each drag its own folder: a file still being read is never overwritten)
    juce::MidiFile again;
    juce::FileInputStream in2 (withChange);
    REQUIRE (again.readFrom (in2));
    std::vector<std::pair<double, double>> tempos;
    for (int i = 0; i < again.getTrack (0)->getNumEvents(); ++i)
        if (const auto& m = again.getTrack (0)->getEventPointer (i)->message; m.isTempoMetaEvent())
            tempos.emplace_back (m.getTimeStamp(), 60.0 / m.getTempoSecondsPerQuarterNote());
    REQUIRE (tempos.size() == 2);
    CHECK_THAT (tempos[0].second, WithinAbs (100.0, 0.01));
    CHECK (tempos[1].first == 960.0);
    CHECK_THAT (tempos[1].second, WithinAbs (160.0, 0.01));
}

TEST_CASE ("Drag out: an audio event becomes a .wav of what it plays: offset, length, fades, volume, warping; its tempo in the name and the ACID chunk (0.53.1)", "[dragout]")
{
    Studio studio (false);
    const auto trackId = studio.addAudioTrack (false);
    studio.renameTrack (trackId, "Rhythm Gtr");
    writeRamp (studio, "take.wav", 48000.0, 8.0);
    const auto heard = studio.getHeardAudio ("take.wav");
    REQUIRE (heard != nullptr);
    const auto rate = heard->sampleRate;
    Part clip;
    clip.start = 2.0;
    clip.length = 2.0;          // a second at 120 BPM
    clip.audioFile = "take.wav";
    clip.audioOffset = 0.5;
    clip.fadeIn = 0.1;
    clip.fadeOut = 0.2;
    clip.volumeDb = -6.0;
    clip.name = "Verse riff";
    studio.addPart (trackId, clip);
    const auto partId = studio.getProject().findTrack (trackId)->parts.back().id;
    const auto before = songText (studio);
    const auto couldUndo = studio.canUndo();

    juce::String error;
    const auto file = studio.writePartForDrag (partId, error);
    INFO (error);
    REQUIRE (file.existsAsFile());
    CHECK (file.getFileName() == "Rhythm Gtr - Verse riff 120 BPM.wav");
    CHECK (songText (studio) == before);
    CHECK (studio.canUndo() == couldUndo);

    double fileRate = 0.0;
    juce::StringPairArray metadata;
    const auto sound = readWav (file, fileRate, metadata);
    CHECK (fileRate == rate);
    CHECK (sound.getNumChannels() == 2);
    CHECK (std::abs (sound.getNumSamples() - (int) std::llround (rate)) <= 1);
    CHECK (metadata.getValue (juce::WavAudioFormat::acidTempo, {}).getDoubleValue() == 120.0);
    CHECK (metadata.getValue (juce::WavAudioFormat::acidBeats, {}).getIntValue() == 2);
    // Sample for sample: the recording from its offset, times the volume and the clip's fades.
    const auto gain = juce::Decibels::decibelsToGain (-6.0);
    const auto offset = (int) std::llround (0.5 * rate);
    for (const auto s : { 0, (int) (0.05 * rate), (int) (0.1 * rate) + 3, (int) (0.5 * rate), (int) (0.9 * rate), sound.getNumSamples() - 1 })
    {
        INFO ("sample " << s);
        const auto into = s / rate, left = 1.0 - s / rate;
        auto expected = (double) heard->sound.getSample (0, offset + s) * gain;
        if (into < 0.1) expected *= into / 0.1;
        if (left < 0.2) expected *= left / 0.2;
        CHECK_THAT (sound.getSample (0, s), WithinAbs (expected, 1.0e-6));
        CHECK_THAT (sound.getSample (1, s), WithinAbs (-expected, 1.0e-6));
    }

    // Warped: the stretched sound it plays.
    studio.setWarpMarkers (partId, { { 0.0, 0.5 }, { 1.0, 0.9 }, { 2.0, 1.5 } });
    REQUIRE (studio.getProject().findTrack (trackId)->parts.back().isWarped());
    const auto warpedFile = studio.writePartForDrag (partId, error);
    REQUIRE (warpedFile.existsAsFile());
    const auto warped = studio.getWarpedSound (*[&] { const auto where = studio.findPart (partId); return &studio.getProject().findTrack (where->first)->parts[(size_t) where->second]; }(),
                                               studio.getProject().tempoMap());
    REQUIRE (warped != nullptr);
    const auto stretched = readWav (warpedFile, fileRate, metadata);
    const auto mid = stretched.getNumSamples() / 2;
    CHECK_THAT (stretched.getSample (0, mid), WithinAbs (warped->getSample (0, mid) * gain, 1.0e-6));

    // At most five minutes (what PR7-D reads).
    auto& longClip = clip;
    longClip.start = 0.0;
    longClip.length = 800.0;   // 400 s at 120 BPM
    longClip.audioOffset = 0.0;
    longClip.fadeIn = longClip.fadeOut = 0.0;
    writeRamp (studio, "long.wav", 48000.0, 401.0);
    longClip.audioFile = "long.wav";
    studio.addPart (trackId, longClip);
    const auto longFile = studio.writePartForDrag (studio.getProject().findTrack (trackId)->parts.back().id, error);
    REQUIRE (longFile.existsAsFile());
    const auto capped = readWav (longFile, fileRate, metadata);
    CHECK (capped.getNumSamples() <= (int) std::ceil (dragOutMostSeconds * fileRate));
    studio.getAudioFolder().deleteRecursively();
}

TEST_CASE ("Drag out: names, the folder, nothing for a missing part (0.53.1)", "[dragout]")
{
    CHECK (dragOutTempoText (140.0) == "140");
    CHECK (dragOutTempoText (122.5) == "122.5");
    CHECK (dragOutTempoText (99.96) == "100");
    const auto a = newDragOutFolder(), b = newDragOutFolder();
    CHECK (a != b);
    CHECK (a.isAChildOf (dragOutFolder()));
    clearDragOutFolder();
    CHECK_FALSE (dragOutFolder().exists());
    Studio studio (false);
    juce::String error;
    CHECK (studio.writePartForDrag (12345, error) == juce::File());
}

#include "ui/Finish.h"
#include "ui/MainView.h"

TEST_CASE ("Drag out from the arrangement: the part leaves as a file, stays where it was, no undo step (0.53.1)", "[dragout][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addPart (trackId, { 4.0, 4.0, "Riff", { { 0.0, 1.0, 40, 0.8f } } });
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    arrange.select (trackId, 0);
    juce::StringArray dragged;
    arrange.onExternalDrag = [&] (const juce::StringArray& files) { dragged = files; CHECK (arrange.isDraggingOut()); };
    const auto before = songText (studio);
    const auto couldUndo = studio.canUndo();
    const auto file = arrange.dragPartOut();
    REQUIRE (dragged.size() == 1);
    CHECK (dragged[0] == file.getFullPathName());
    CHECK (juce::File (dragged[0]).existsAsFile());
    CHECK (file.getFileExtension() == ".mid");
    CHECK_FALSE (arrange.isDraggingOut());
    CHECK (songText (studio) == before);
    CHECK (studio.canUndo() == couldUndo);
    // Dropped back on the studio: nothing.
    CHECK_FALSE (view.isInterestedInFileDrag (dragged));
}
