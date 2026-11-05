// 0.45.0: signature changes, tap tempo and tempo detection.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/MidiImport.h"
#include "engine/Signature.h"
#include "engine/Studio.h"
#include "engine/TempoDetect.h"
#include "ui/ArrangeView.h"
#include "ui/TempoDetectionPanel.h"
#include "ui/TempoEditor.h"
#include "ui/Finish.h"
#include "ui/ParagonLookAndFeel.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    // A click on each beat: a 1 kHz burst of 25 ms, fading.
    std::vector<float> clicks (const std::vector<double>& beats, double seconds, double rate)
    {
        std::vector<float> sound ((size_t) (seconds * rate), 0.0f);
        for (const auto t : beats)
        {
            const auto start = (int) std::lround (t * rate);
            for (int i = 0; i < (int) (0.025 * rate); ++i)
                if (start + i >= 0 && start + i < (int) sound.size())
                    sound[(size_t) (start + i)] += 0.6f * std::exp (-i / (0.006 * rate)) * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / rate);
        }
        return sound;
    }

    // Beats from 100 to 112 BPM over the length (a drummer pushing), from `first` seconds.
    std::vector<double> driftingBeats (double first, double seconds)
    {
        std::vector<double> beats;
        for (auto t = first; t < seconds - 0.05;)
        {
            beats.push_back (t);
            const auto bpm = 100.0 + 12.0 * t / seconds;
            t += 60.0 / bpm;
        }
        return beats;
    }

    double nearestDistance (const std::vector<double>& found, double t)
    {
        auto best = 1.0e9;
        for (const auto f : found)
            best = std::min (best, std::abs (f - t));
        return best;
    }
}

TEST_CASE ("0.45.0: the signature map: bars and beats across changes, positions shown and typed", "[signature]")
{
    // 4/4, then 7/8 from bar 3 (index 2), then 3/4 from bar 5.
    const SignatureMap map (4, 4, { { 4, 3, 4 }, { 2, 7, 8 }, { 0, 5, 4 }, { 2, 7, 8 } });
    REQUIRE (map.segments().size() == 3);   // (bar 0 and the duplicate ignored)
    CHECK (map.beatOfBar (0) == 0.0);
    CHECK (map.beatOfBar (2) == 8.0);
    CHECK (map.beatOfBar (3) == 11.5);       // a 7/8 bar is 3.5 beats
    CHECK (map.beatOfBar (4) == 15.0);
    CHECK (map.beatOfBar (5) == 18.0);       // then 3/4
    CHECK (map.barAt (11.4) == 2);
    CHECK (map.barAt (11.5) == 3);
    CHECK (map.barAt (11.5 - 1.0e-9) == 3);  // (a hair before a bar line counts as on it)
    CHECK (map.barLengthAt (9.0) == 3.5);
    CHECK (map.unitAt (9.0) == 0.5);
    CHECK (map.isBarLine (15.0));
    CHECK_FALSE (map.isBarLine (16.0));
    const auto p = map.positionOf (9.25);
    CHECK (p.bar == 2);
    CHECK (p.beat == 2);                     // eighths: 8.0, 8.5, 9.0
    CHECK_THAT (p.rest, WithinAbs (0.25, 1.0e-9));
    CHECK_THAT (map.beatOf (2, 2, 0.25), WithinAbs (9.25, 1.0e-12));

    Project song;
    song.signatureChanges = { { 2, 7, 8 }, { 4, 3, 4 } };
    CHECK (formatBarsBeats (0.0, song) == "1. 1. 1.   0");
    CHECK (formatBarsBeats (9.25, song) == "3. 3. 2.   0");
    CHECK (formatBarsBeats (11.5, song) == "4. 1. 1.   0");
    CHECK (formatBarsBeats (18.0, song) == "6. 1. 1.   0");
    CHECK (TempoEditor::parsePosition ("3.3.2.0", song) == std::optional<double> (9.25));
    CHECK (TempoEditor::parsePosition ("6.1.1.0", song) == std::optional<double> (18.0));
    CHECK (song.barStartAt (12.0) == 11.5);
    CHECK (song.nextBarAfter (12.0) == 15.0);

    int n = 0, d = 0;
    CHECK (parseSignature ("7/8", n, d));
    CHECK ((n == 7 && d == 8));
    CHECK (parseSignature (" 12 / 16", n, d));
    CHECK ((n == 12 && d == 16));
    CHECK_FALSE (parseSignature ("7/9", n, d));
    CHECK_FALSE (parseSignature ("0/4", n, d));
    CHECK_FALSE (parseSignature ("seven", n, d));
}

TEST_CASE ("0.45.0: signature events in the studio: added at bars, moved, removed, locked, undone, saved", "[signature]")
{
    Studio studio (false);
    studio.addSignatureTrack();
    CHECK (studio.getProject().signatureTrackShown);
    CHECK (studio.setSignatureAt (2, 7, 8));
    CHECK (studio.setSignatureAt (4, 4, 4));
    REQUIRE (studio.getProject().signatureChanges.size() == 2);
    // At a bar already holding one: it changes.
    CHECK (studio.setSignatureAt (2, 5, 8));
    CHECK (studio.getProject().signatureChanges[0] == SignatureEvent { 2, 5, 8 });
    CHECK_FALSE (studio.setSignatureAt (3, 7, 9));   // not a signature
    // The first is the song's own.
    CHECK (studio.setSignatureAt (0, 3, 4));
    CHECK ((studio.getProject().numerator == 3 && studio.getProject().denominator == 4));
    CHECK_FALSE (studio.removeSignatures ({ 0 }));   // (never the first)
    // Moved: onto another bar.
    CHECK (studio.moveSignature (4, 6));
    CHECK (studio.getProject().signatureChanges[1].bar == 6);
    // Undo: back where it was.
    studio.undo();
    CHECK (studio.getProject().signatureChanges[1].bar == 4);
    // Locked: no edits (the first, the transport's, still changes).
    studio.setSignatureLocked (true);
    CHECK_FALSE (studio.setSignatureAt (8, 6, 8));
    CHECK_FALSE (studio.removeSignatures ({ 2 }));
    CHECK_FALSE (studio.moveSignature (2, 3));
    CHECK (studio.setSignatureAt (0, 4, 4));
    studio.setSignatureLocked (false);
    CHECK (studio.removeSignatures ({ 2 }));
    REQUIRE (studio.getProject().signatureChanges.size() == 1);

    // At the cursor's bar: the bar the playhead is in.
    studio.setPosition (13.0);   // bar 4 (from beat 12) in 4/4
    CHECK (studio.insertSignatureAtCursor (6, 8));
    CHECK (studio.getProject().signatureChanges[0] == SignatureEvent { 3, 6, 8 });
    CHECK (studio.getSignatureNow() == SignatureEvent { 3, 6, 8 });

    // Saved and read back.
    const auto xml = studio.getProject().toXml();
    const auto back = Project::fromXml (*xml);
    REQUIRE (back);
    CHECK (back->signatureChanges == studio.getProject().signatureChanges);
    CHECK (back->signatureTrackShown);
    // A song from before: no signature element, one signature.
    xml->removeChildElement (xml->getChildByName ("Signatures"), true);
    CHECK (Project::fromXml (*xml)->signatureChanges.empty());

    // Removing the track keeps the bars (the events stay).
    studio.removeSignatureTrack();
    CHECK_FALSE (studio.getProject().signatureTrackShown);
    CHECK (studio.getProject().signatureChanges.size() == 2);
}

TEST_CASE ("0.45.0: the click and the plug-ins' bars follow the signature changes", "[signature][engine]")
{
    constexpr double rate = 48000.0;
    Engine engine;
    engine.prepare (rate, 512);
    Project song;
    song.bpm = 120.0;
    song.click = true;
    song.signatureChanges = { { 1, 3, 4 } };   // 4/4 for bar 1, then 3/4: bars at 0, 4, 7, 10
    engine.update (song);
    engine.play();
    const auto out = renderSamples (engine, (int) (6.0 * rate));
    const auto window = (size_t) (0.01 * rate);
    // Every beat clicks (a beat each half second); the accent is a higher pitch (1760 against 1320 Hz).
    const auto accented = [&] (double beat)
    {
        const auto at = (size_t) (beat * 0.5 * rate);
        return levelAt (out, 1760.0, rate, at, at + window) > levelAt (out, 1320.0, rate, at, at + window);
    };
    CHECK (accented (0.0));
    CHECK_FALSE (accented (1.0));
    CHECK_FALSE (accented (3.0));
    CHECK (accented (4.0));
    CHECK_FALSE (accented (5.0));
    CHECK_FALSE (accented (6.0));
    CHECK (accented (7.0));      // (4/4 throughout would have it on 8)
    CHECK_FALSE (accented (8.0));
    CHECK (accented (10.0));
}

TEST_CASE ("0.45.0: tap tempo: the average of the taps, by the mode; one undo step for a run of taps", "[tap]")
{
    Studio studio (false);
    studio.setTempo (120.0);
    // The tempo track off: Automatic sets the project tempo.
    studio.setTapMode (Studio::TapMode::automatic);
    auto r = studio.tapTempo (10.0);
    CHECK (r.bpm == 0.0);   // (one tap: not yet)
    for (int i = 1; i <= 4; ++i)
        r = studio.tapTempo (10.0 + 0.6 * i);   // 100 BPM
    CHECK_THAT (r.bpm, WithinAbs (100.0, 0.01));
    CHECK_THAT (studio.getProject().bpm, WithinAbs (100.0, 0.01));
    CHECK (r.where == "Project tempo");
    CHECK (studio.isTapping (12.5));
    CHECK_FALSE (studio.isTapping (16.0));
    studio.endTapping();
    // One undo step for the whole run.
    studio.undo();
    CHECK_THAT (studio.getProject().bpm, WithinAbs (120.0, 0.01));

    // The tempo track on, at the cursor: an event there.
    studio.addTempoTrack();
    studio.setPosition (8.0);
    studio.setTapMode (Studio::TapMode::cursor);
    for (int i = 0; i < 4; ++i)
        r = studio.tapTempo (100.0 + 0.4 * i);   // 150 BPM
    CHECK (r.at == 8.0);
    REQUIRE (studio.getProject().tempoEvents.size() == 2);
    CHECK_THAT (studio.getProject().tempoEvents[1].bpm, WithinAbs (150.0, 0.01));
    CHECK (studio.getProject().tempoEvents[1].beat == 8.0);

    // A gap of 2 s starts again; Display Only changes nothing.
    studio.setTapMode (Studio::TapMode::displayOnly);
    for (int i = 0; i < 3; ++i)
        r = studio.tapTempo (200.0 + 0.5 * i);
    CHECK_THAT (r.bpm, WithinAbs (120.0, 0.01));
    CHECK (r.where == "Display only");
    CHECK_THAT (studio.getProject().tempoEvents[1].bpm, WithinAbs (150.0, 0.01));

    // At the selection's start, with nothing selected: shown only.
    studio.setTapMode (Studio::TapMode::selectionStart);
    for (int i = 0; i < 3; ++i)
        r = studio.tapTempo (300.0 + 0.5 * i);
    CHECK (r.where.startsWith ("Nothing selected"));
    // ... and with a selection from beat 16.
    for (int i = 0; i < 3; ++i)
        r = studio.tapTempo (400.0 + 0.5 * i, 16.0);
    CHECK (r.at == 16.0);
    CHECK (studio.getProject().tempoEvents.back().beat == 16.0);
}

TEST_CASE ("0.45.0: tempo detection finds the beats of a drifting recording and of a MIDI part", "[tempodetect]")
{
    constexpr double rate = 44100.0, seconds = 14.0;
    const auto beats = driftingBeats (0.3, seconds);
    const auto sound = clicks (beats, seconds, rate);
    const auto found = detectBeats (sound.data(), (int) sound.size(), rate);
    REQUIRE (found.beats.size() >= beats.size() - 2);
    CHECK (found.beats.size() <= beats.size() + 1);
    auto worst = 0.0;
    for (const auto t : beats)
        if (t > 1.0 && t < seconds - 1.0)
            worst = std::max (worst, nearestDistance (found.beats, t));
    CHECK (worst < 0.015);   // within 15 ms everywhere but the ends
    CHECK (found.bpm > 100.0);
    CHECK (found.bpm < 112.0);
    CHECK (found.confidence > 0.2);

    // MIDI: notes on the beats (hi-hat eighths between, softer).
    std::vector<DetectNote> notes;
    for (size_t i = 0; i < beats.size(); ++i)
    {
        notes.push_back ({ beats[i], 1.0f });
        if (i + 1 < beats.size())
            notes.push_back ({ 0.5 * (beats[i] + beats[i + 1]), 0.35f });
    }
    const auto midi = detectBeats (notes, seconds);
    worst = 0.0;
    for (const auto t : beats)
        if (t > 1.0 && t < seconds - 1.0)
            worst = std::max (worst, nearestDistance (midi.beats, t));
    CHECK (worst < 0.012);

    // The corrections on the beats.
    const std::vector<double> steady { 0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0 };
    CHECK (rescaleBeats (steady, 2.0).size() == 13);
    CHECK_THAT (rescaleBeats (steady, 2.0)[1], WithinAbs (0.25, 1.0e-9));
    CHECK (rescaleBeats (steady, 0.5).size() == 4);
    CHECK_THAT (rescaleBeats (steady, 0.75)[1], WithinAbs (2.0 / 3.0, 1.0e-9));
    CHECK_THAT (rescaleBeats (steady, 4.0 / 3.0)[1], WithinAbs (0.375, 1.0e-9));
    CHECK_THAT (rescaleBeats (steady, 1.0, 0.5)[0], WithinAbs (0.25, 1.0e-9));
    const std::vector<double> uneven { 0.0, 0.5, 1.02, 1.5, 1.98, 2.5, 3.0, 3.5 };
    const auto smooth = smoothBeats (uneven);
    REQUIRE (smooth.size() == uneven.size());
    CHECK (std::abs (smooth[2] - 1.0) < std::abs (uneven[2] - 1.0));
    CHECK_THAT (medianBpm (steady), WithinAbs (120.0, 1.0e-9));

    // Too little: nothing.
    std::vector<float> shortSound ((size_t) (2.0 * rate), 0.0f);
    CHECK (detectBeats (shortSound.data(), (int) shortSound.size(), rate).beats.empty());
}

TEST_CASE ("0.45.0: tempo detection in the studio: the tempo track follows a clip's beats and everything keeps its place in time", "[tempodetect]")
{
    constexpr double rate = 44100.0, seconds = 14.0;
    Studio studio (false);
    studio.setTempo (120.0);
    const auto beats = driftingBeats (0.3, seconds);
    const auto sound = clicks (beats, seconds, rate);
    juce::AudioBuffer<float> buffer (1, (int) sound.size());
    buffer.copyFrom (0, 0, sound.data(), (int) sound.size());
    REQUIRE (writeWav (studio.getAudioFolder().getChildFile ("drums.wav"), buffer, rate));
    const auto audioTrack = studio.addAudioTrack (false);
    Part clip;
    clip.start = 4.0;   // (at 120: two seconds in)
    clip.length = seconds * 2.0;
    clip.audioFile = "drums.wav";
    studio.addPart (audioTrack, clip);
    const auto clipId = studio.getProject().findTrack (audioTrack)->parts[0].id;

    // A MIDI part later in the song, and an automation point: they must keep their seconds.
    const auto midiTrack = studio.addTrack ({});
    Part riff;
    riff.start = 20.0;
    riff.length = 4.0;
    riff.notes = { { 1.0, 1.0, 60, 0.8f } };
    studio.addPart (midiTrack, riff);
    const auto noteSeconds = studio.getProject().tempoMap().secondsAt (21.0);

    TempoDetectionPanel panel (studio, [clipId] { return std::vector<int> { clipId }; });
    const auto analysed = panel.analyse();
    INFO (panel.getStatus());
    REQUIRE (analysed);
    const auto& song = studio.getProject();
    CHECK (song.followsTempoTrack());
    CHECK (song.tempoTrackShown);
    // Before the clip: the old tempo; then an event on each beat.
    CHECK (song.tempoEvents.front().bpm == 120.0);
    const auto map = song.tempoMap();
    // Each beat heard in the clip lands on a whole beat of the song (within 15 ms; the ends left out).
    const auto clipStart = map.secondsAt (4.0);
    CHECK_THAT (clipStart, WithinAbs (2.0, 1.0e-9));   // (the tempo before it unchanged)
    auto worst = 0.0;
    for (const auto t : beats)
        if (t > 1.0 && t < seconds - 1.0)
        {
            const auto beat = map.beatAt (clipStart + t);
            worst = std::max (worst, std::abs (beat - std::round (beat)) * 60.0 / map.bpmAt (beat));
        }
    CHECK (worst < 0.015);
    // The MIDI note sounds where it did.
    const auto* moved = song.findTrack (midiTrack);
    REQUIRE (moved != nullptr);
    CHECK_THAT (map.secondsAt (moved->parts[0].start + moved->parts[0].notes[0].start), WithinAbs (noteSeconds, 1.0e-6));
    // The clip still lasts its 14 s.
    const auto& clipNow = song.findTrack (audioTrack)->parts[0];
    CHECK_THAT (map.secondsAt (clipNow.start + clipNow.length) - map.secondsAt (clipNow.start), WithinAbs (seconds, 1.0e-6));

    // x2: twice the events; the note still where it was.
    const auto count = song.tempoEvents.size();
    REQUIRE (panel.correct (TempoDetectionPanel::Correction::times2));
    CHECK (song.tempoEvents.size() > count + count / 2);
    CHECK_THAT (song.tempoMap().secondsAt (song.findTrack (midiTrack)->parts[0].start + song.findTrack (midiTrack)->parts[0].notes[0].start),
                WithinAbs (noteSeconds, 1.0e-6));
    // RESET: the tempo as before (off), the note back on beat 21.
    REQUIRE (panel.reset());
    CHECK_FALSE (song.followsTempoTrack());
    CHECK_THAT (song.findTrack (midiTrack)->parts[0].start + song.findTrack (midiTrack)->parts[0].notes[0].start, WithinAbs (21.0, 1.0e-6));

    // Too short: refused with the reason.
    Part tiny;
    tiny.start = 60.0;
    tiny.length = 4.0;   // two seconds
    tiny.audioFile = "drums.wav";
    studio.addPart (audioTrack, tiny);
    BeatAnalysis none;
    juce::String error;
    CHECK_FALSE (studio.analyseTempo (studio.getProject().findTrack (audioTrack)->parts.back().id, none, error));
    CHECK (error.contains ("7 seconds"));
}

TEST_CASE ("Snapshot: the Tempo Detection panel after an analysis (0.45.0)", "[.][snapshot-tempodetect]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "tempodetect.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        constexpr double rate = 44100.0, seconds = 20.0;
        Studio studio (false);
        const auto sound = clicks (driftingBeats (0.3, seconds), seconds, rate);
        juce::AudioBuffer<float> buffer (1, (int) sound.size());
        buffer.copyFrom (0, 0, sound.data(), (int) sound.size());
        writeWav (studio.getAudioFolder().getChildFile ("Drums take 1.wav"), buffer, rate);
        const auto track = studio.addAudioTrack (false);
        Part clip;
        clip.start = 0.0;
        clip.length = seconds * 2.0;
        clip.audioFile = "Drums take 1.wav";
        clip.name = "Drums take 1";
        studio.addPart (track, clip);
        const auto id = studio.getProject().findTrack (track)->parts[0].id;
        TempoDetectionPanel panel (studio, [id] { return std::vector<int> { id }; });
        panel.analyse();
        const auto image = panel.createComponentSnapshot (panel.getLocalBounds(), true, 1.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

TEST_CASE ("0.45.0: a MIDI file's signature changes come in with a new song", "[signature][midi]")
{
    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
    track.addEvent (juce::MidiMessage::timeSignatureMetaEvent (7, 8), 960.0 * 4 * 2);           // bar 3
    track.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 960.0 * (8 + 3.5 * 2));   // bar 5
    for (int i = 0; i < 16; ++i)
    {
        track.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 960.0 * i);
        track.addEvent (juce::MidiMessage::noteOff (1, 60), 960.0 * i + 480.0);
    }
    track.updateMatchedPairs();
    juce::MidiFile file;
    file.setTicksPerQuarterNote (960);
    file.addTrack (track);
    const auto midiFile = juce::File::createTempFile (".mid");
    {
        juce::FileOutputStream out (midiFile);
        REQUIRE (file.writeTo (out));
    }
    juce::String error;
    const auto read = readMidiFile (midiFile, error);
    REQUIRE (read);
    REQUIRE (read->signatureChanges.size() == 2);
    CHECK (read->signatureChanges[0] == SignatureEvent { 2, 7, 8 });
    CHECK (read->signatureChanges[1] == SignatureEvent { 4, 4, 4 });
    Studio studio (false);
    REQUIRE (studio.importMidi (midiFile, true, 0.0, error));
    CHECK (studio.getProject().signatureChanges.size() == 2);
    CHECK (studio.getProject().signatureTrackShown);
    midiFile.deleteFile();
}

TEST_CASE ("0.45.0: the signature track on screen: its row, select, drag along the bars, erase, Delete", "[signature][ui]")
{
    Studio studio (false);
    studio.addSignatureTrack();
    studio.setSignatureAt (2, 7, 8);
    Timeline timelineOwned;
    ArrangeView view (studio, timelineOwned);
    view.setBounds (0, 0, 1600, 600);
    const auto area = view.getSignatureArea();
    REQUIRE_FALSE (area.isEmpty());
    const auto label = view.getSignatureBounds (2);
    REQUIRE_FALSE (label.isEmpty());
    CHECK (view.signatureAt (label.getCentre()) == 2);
    // A click selects it; a drag moves it two bars on.
    auto& timeline = view.getTimeline();
    const auto map = studio.getProject().signatures();
    const auto target = juce::Point<float> (timeline.xForBeat (map.beatOfBar (4)) + 3.0f, label.getCentreY());
    auto downAt = label.getCentre();
    const auto event = [&] (juce::Point<float> at, juce::ModifierKeys mods)
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, mods, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &view, &view,
                                 now, downAt, now, 1, at != downAt);
    };
    view.mouseDown (event (label.getCentre(), juce::ModifierKeys::leftButtonModifier));
    CHECK (view.getSignatureSelection() == std::vector<int> { 2 });
    view.mouseDrag (event (target, juce::ModifierKeys::leftButtonModifier));
    view.mouseUp (event (target, {}));
    REQUIRE (studio.getProject().signatureChanges.size() == 1);
    CHECK (studio.getProject().signatureChanges[0].bar == 4);
    CHECK (view.getSignatureSelection() == std::vector<int> { 4 });
    // Delete: gone; the first can't be.
    view.selectSignatures ({ 0, 4 });
    view.deleteSelectedSignatures();
    CHECK (studio.getProject().signatureChanges.empty());
    CHECK (studio.getProject().numerator == 4);
    // ERASE.
    studio.setSignatureAt (3, 5, 4);
    view.setTool (ArrangeView::Tool::erase);
    downAt = view.getSignatureBounds (3).getCentre();
    view.mouseDown (event (view.getSignatureBounds (3).getCentre(), juce::ModifierKeys::leftButtonModifier));
    view.mouseUp (event (view.getSignatureBounds (3).getCentre(), {}));
    CHECK (studio.getProject().signatureChanges.empty());
}
