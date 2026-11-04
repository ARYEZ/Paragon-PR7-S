#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/VariAudio.h"
#include "engine/Studio.h"
#include "engine/Warp.h"
#include "ui/Finish.h"
#include "ui/VariAudioEditor.h"

// VariAudio, part two (0.35.0): pitch curves tilted, rotated and straightened over a range,
// formants shifted, segments split, glued, resized and deleted (their pitch found again), the
// resets, Warp Start/End, Extract MIDI, harmony voices, MIDI Input's keys.

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    constexpr double rate = 48000.0;

    struct Sung
    {
        double seconds = 0.4;
        float pitch = 60.0f;
        double gapAfter = 0.05;
        float level = 0.3f;
    };

    // Steady sung notes (eight harmonics), one after another.
    juce::AudioBuffer<float> sing (const std::vector<Sung>& notes, double lead = 0.1)
    {
        auto total = lead + 0.1;
        for (const auto& n : notes)
            total += n.seconds + n.gapAfter;
        juce::AudioBuffer<float> sound (1, (int) (total * rate));
        sound.clear();
        auto at = (int) (lead * rate);
        auto phase = 0.0;
        for (const auto& n : notes)
        {
            const auto length = (int) (n.seconds * rate);
            const auto hz = 440.0 * std::pow (2.0, (n.pitch - 69.0) / 12.0);
            for (int i = 0; i < length; ++i)
            {
                const auto t = i / rate;
                phase += 2.0 * juce::MathConstants<double>::pi * hz / rate;
                auto v = 0.0;
                for (int k = 1; k <= 8; ++k)
                    v += std::sin (k * phase) / k;
                const auto envelope = juce::jmin (1.0, juce::jmin (t / 0.01, (n.seconds - t) / 0.01));
                sound.setSample (0, at + i, (float) (n.level * v * envelope));
            }
            at += length + (int) (n.gapAfter * rate);
        }
        return sound;
    }

    void writeWav (const juce::File& file, const juce::AudioBuffer<float>& sound)
    {
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (1)
                                                                           .withBitsPerSample (32)
                                                                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (sound, 0, sound.getNumSamples());
    }

    float heardBetween (const PitchAnalysis& a, double from, double to)
    {
        std::vector<float> values;
        for (auto t = from; t < to; t += a.hopSeconds)
            if (const auto p = a.pitchAt (t); p > 0.0f)
                values.push_back (p);
        if (values.empty())
            return 0.0f;
        std::nth_element (values.begin(), values.begin() + (long) values.size() / 2, values.end());
        return values[values.size() / 2];
    }

    juce::MouseEvent mouseAt (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt, juce::ModifierKeys mods = {})
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                                 mods.withFlags (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, now, downAt, now, 1, at != downAt);
    }

    // A press, a drag (or none) and a release, the segment hovered first (its smart controls shown).
    void gesture (VariAudioEditor& editor, int segment, juce::Point<float> from, juce::Point<float> to, juce::ModifierKeys mods = {})
    {
        const auto centre = editor.getSegmentBounds (segment).getCentre();
        editor.mouseMove (mouseAt (editor, centre, centre));
        editor.mouseMove (mouseAt (editor, from, from));
        editor.mouseDown (mouseAt (editor, from, from, mods));
        if (to != from)
            editor.mouseDrag (mouseAt (editor, to, from, mods));
        editor.mouseUp (mouseAt (editor, to, from, mods));
    }

    // A clip of C4, E4, G4 (a little flat, quieter) and C4, on an audio track; its id.
    int makeVocal (Studio& studio, const juce::String& name = "vocal.wav")
    {
        writeWav (studio.getAudioFolder().getChildFile (name),
                  sing ({ { 0.4, 60.0f, 0.15, 0.3f }, { 0.5, 64.0f, 0.15, 0.3f }, { 0.4, 66.75f, 0.15, 0.08f }, { 0.5, 60.0f, 0.15, 0.3f } }));
        const auto track = studio.addAudioTrack (false);
        Part clip;
        clip.length = 6.0;
        clip.audioFile = name;
        studio.addPart (track, clip);
        return studio.getProject().findTrack (track)->parts[0].id;
    }
}

TEST_CASE ("VariAudio 2: tilt, rotate round the anchor, straighten over a range", "[variaudio]")
{
    VariSegment s;
    s.start = 1.0;
    s.end = 2.0;
    s.pitch = 60.0f;
    s.tiltStart = 2.0f;   // the start up two semitones, nothing at the anchor (the middle) and after it
    CHECK_THAT (tiltAt (s, 1.0), WithinAbs (2.0, 1.0e-5));
    CHECK_THAT (tiltAt (s, 1.25), WithinAbs (1.0, 1.0e-5));
    CHECK_THAT (tiltAt (s, 1.5), WithinAbs (0.0, 1.0e-5));
    CHECK_THAT (tiltAt (s, 2.0), WithinAbs (0.0, 1.0e-5));

    // Rotated: one straight line through the anchor (here at a quarter).
    VariSegment r = s;
    r.tiltStart = r.tiltEnd = 0.0f;
    r.anchor = 0.25f;
    rotateSegment (r, 1.0f, true);
    CHECK_THAT (tiltAt (r, 1.0), WithinAbs (1.0, 1.0e-5));
    CHECK_THAT (tiltAt (r, 1.25), WithinAbs (0.0, 1.0e-5));
    CHECK_THAT (tiltAt (r, 2.0), WithinAbs (-3.0, 1.0e-5));   // (the slope carried on: three times as far)
    const auto slopeLeft = (tiltAt (r, 1.25) - tiltAt (r, 1.0)) / 0.25f, slopeRight = (tiltAt (r, 2.0) - tiltAt (r, 1.25)) / 0.75f;
    CHECK_THAT (slopeLeft, WithinAbs (slopeRight, 1.0e-4));

    // Straighten over its middle only: none outside, full inside (past the 20 ms easing).
    VariSegment st = s;
    st.tiltStart = 0.0f;
    st.straighten = 1.0f;
    st.straightenFrom = 0.25f;
    st.straightenTo = 0.75f;
    CHECK (straightenAt (st, 1.1) == 0.0f);
    CHECK (straightenAt (st, 1.5) == 1.0f);
    CHECK (straightenAt (st, 1.9) == 0.0f);
    CHECK_THAT (straightenAt (st, 1.26), WithinAbs (0.5, 1.0e-3));   // (easing in)
    // The whole segment: no easing at its own edges (as part one straightened).
    st.straightenFrom = 0.0f;
    st.straightenTo = 1.0f;
    CHECK (straightenAt (st, 1.0) == 1.0f);
    CHECK (straightenAt (st, 2.0) == 1.0f);
}

TEST_CASE ("VariAudio 2: a tilted curve and shifted formants heard; segments split, glued, resized (their pitch found again)", "[variaudio]")
{
    const auto recording = sing ({ { 0.6, 60.0f, 0.15 }, { 0.6, 64.0f, 0.15 } });
    const auto analysis = analysePitch (recording, rate);
    auto segments = segmentPitch (analysis);
    REQUIRE (segments.size() == 2);

    // Tilted: the end two semitones up, the start untouched (the anchor at the start).
    auto tilted = segments;
    tilted[0].anchor = 0.02f;
    tilted[0].tiltEnd = 2.0f;
    const auto heard = analysePitch (renderVariAudio (recording, rate, analysis, tilted), rate);
    const auto& s = tilted[0];
    const auto length = s.end - s.start;
    CHECK_THAT (heardBetween (heard, s.start + 0.05 * length, s.start + 0.15 * length), WithinAbs (60.0 + 2.0 * 0.08, 0.2));
    CHECK_THAT (heardBetween (heard, s.end - 0.15 * length, s.end - 0.05 * length), WithinAbs (60.0 + 2.0 * 0.88, 0.25));

    // Formants moved: the pitch stays, the sound (its colour) changes.
    auto shifted = segments;
    shifted[1].formant = 60.0f;
    const auto formanted = renderVariAudio (recording, rate, analysis, shifted);
    const auto formantedPitch = analysePitch (formanted, rate);
    const auto& f = shifted[1];
    CHECK_THAT (heardBetween (formantedPitch, f.start + 0.1, f.end - 0.1), WithinAbs (64.0, 0.15));
    auto difference = 0.0;
    for (int i = (int) ((f.start + 0.1) * rate); i < (int) ((f.end - 0.1) * rate); ++i)
        difference += std::abs (formanted.getSample (0, i) - recording.getSample (0, i));
    CHECK (difference > 100.0);
    // (the other note: untouched)
    for (int i = (int) ((segments[0].start + 0.05) * rate); i < (int) ((segments[0].end - 0.05) * rate); i += 97)
        CHECK (formanted.getSample (0, i) == recording.getSample (0, i));

    // Glued: one segment over both notes; split again where the second starts: their own pitches back.
    auto edited = segments;
    edited[1].shift = 1.0f;
    REQUIRE (glueSegments (edited, 0, 1, analysis));
    REQUIRE (edited.size() == 1);
    CHECK (edited[0].start == segments[0].start);
    CHECK (edited[0].end == segments[1].end);
    const auto boundary = segments[1].start;
    REQUIRE (splitSegment (edited, 0, boundary, analysis));
    REQUIRE (edited.size() == 2);
    CHECK_THAT (edited[0].pitch, WithinAbs (60.0, 0.1));
    CHECK_THAT (edited[1].pitch, WithinAbs (64.0, 0.25));
    CHECK_FALSE (splitSegment (edited, 0, edited[0].start + 0.005, analysis));   // (too short a piece)
    CHECK_FALSE (glueSegments (edited, 1, 1, analysis));

    // Edges: up to the neighbours, never overlapping; too short refused.
    auto resized = segments;
    REQUIRE (setSegmentEdges (resized, 0, resized[0].start + 0.1, resized[1].start + 0.5, analysis));
    CHECK (resized[0].end == segments[1].start);   // (held at the next one's start)
    CHECK_FALSE (setSegmentEdges (resized, 0, resized[0].start, resized[0].start + 0.01, analysis));
}

TEST_CASE ("VariAudio 2 in the studio: segment editing and the resets are undo steps; reanalyse; delete", "[variaudio]")
{
    Studio studio (false);
    const auto partId = makeVocal (studio);
    REQUIRE (studio.editVariAudio ("vocal.wav"));
    const auto count = studio.getVariAudio ("vocal.wav")->segments.size();
    REQUIRE (count == 4);
    const auto& segs = [&] () -> const std::vector<VariSegment>& { return studio.getVariAudio ("vocal.wav")->segments; };

    REQUIRE (studio.glueVariSegments ("vocal.wav", 1, 2));
    CHECK (segs().size() == 3);
    studio.undo();
    CHECK (segs().size() == 4);
    REQUIRE (studio.splitVariSegment ("vocal.wav", 0, (segs()[0].start + segs()[0].end) * 0.5));
    CHECK (segs().size() == 5);
    studio.undo();
    REQUIRE (studio.deleteVariSegments ("vocal.wav", { 3, 0 }));
    CHECK (segs().size() == 2);
    CHECK_THAT (segs()[0].pitch, WithinAbs (64.0, 0.2));
    studio.undo();
    REQUIRE (segs().size() == 4);

    // Every kind of change, then each reset takes only its own.
    studio.editVariSegments ("vocal.wav", { 0, 1 }, [] (VariSegment& s)
    {
        s.shift = 2.0f;
        s.straighten = 0.5f;
        s.tiltEnd = 1.0f;
        s.formant = 30.0f;
        s.volumeDb = -6.0f;
    });
    studio.resetVariAudio (partId, { 0 }, Studio::VariReset::pitch);
    CHECK (segs()[0].shift == 0.0f);
    CHECK (segs()[1].shift == 2.0f);   // (only the one given)
    studio.resetVariAudio (partId, {}, Studio::VariReset::curve);
    CHECK (segs()[1].straighten == 0.0f);
    CHECK (segs()[1].tiltEnd == 0.0f);
    CHECK (segs()[1].formant == 30.0f);
    studio.resetVariAudio (partId, {}, Studio::VariReset::formant);
    CHECK (segs()[1].formant == 0.0f);
    CHECK (segs()[1].volumeDb == -6.0f);
    studio.resetVariAudio (partId, {}, Studio::VariReset::all);
    CHECK_FALSE (studio.getVariAudio ("vocal.wav")->isEdited());

    // Reanalyse: the segments found afresh (a glue undone by it).
    studio.glueVariSegments ("vocal.wav", 0, 3);
    REQUIRE (segs().size() == 1);
    REQUIRE (studio.reanalyseVariAudio ("vocal.wav"));
    CHECK (segs().size() == 4);

    // The new values saved with the song.
    studio.editVariSegments ("vocal.wav", { 2 }, [] (VariSegment& s)
    {
        s.tiltStart = -1.5f;
        s.anchor = 0.3f;
        s.straighten = 0.4f;
        s.straightenFrom = 0.2f;
        s.straightenTo = 0.7f;
        s.formant = -25.0f;
    });
    const auto songFile = studio.getAudioFolder().getParentDirectory().getChildFile ("vari2 test.pr7s");
    REQUIRE (studio.save (songFile));
    Studio other (false);
    juce::String problems;
    REQUIRE (other.open (songFile, problems));
    REQUIRE (other.getVariAudio ("vocal.wav") != nullptr);
    CHECK (other.getVariAudio ("vocal.wav")->segments == segs());
}

TEST_CASE ("VariAudio 2: Warp Start moves a segment's start in time (warp markers), the neighbours stretch", "[variaudio]")
{
    Studio studio (false);
    const auto partId = makeVocal (studio);
    REQUIRE (studio.editVariAudio ("vocal.wav"));
    const auto seg = studio.getVariAudio ("vocal.wav")->segments[1];
    const auto tempo = studio.getProject().tempoMap();
    const auto partOf = [&] { const auto found = studio.findPart (partId); return studio.getProject().findTrack (found->first)->parts[(size_t) found->second]; };
    const auto before = warpBeatAt (partOf(), seg.start, tempo);
    REQUIRE (studio.warpVariSegment (partId, 1, true, partOf().start + before + 0.25));
    const auto part = partOf();
    CHECK (part.isWarped());
    CHECK_THAT (warpBeatAt (part, seg.start, tempo), WithinAbs (before + 0.25, 1.0e-3));
    // The segment's end and the one before's start stay where they were.
    CHECK_THAT (warpBeatAt (part, seg.end, tempo), WithinAbs (warpBeatAt (Part {}, 0.0, tempo) + seg.end * 2.0, 0.02));   // (120 BPM: 2 beats a second)
    studio.resetVariAudio (partId, {}, Studio::VariReset::warp);
    CHECK_FALSE (partOf().isWarped());
}

TEST_CASE ("VariAudio 2: Extract MIDI (notes, bends, velocities, a controller) onto a new track, then replaced on the same track", "[variaudio]")
{
    Studio studio (false);
    const auto partId = makeVocal (studio);
    juce::String error;
    CHECK (studio.extractMidi (partId, {}, Studio::ExtractDestination::newTrack, error) == 0);   // (not analysed yet)
    CHECK (error.contains ("EDIT VARIAUDIO"));
    REQUIRE (studio.editVariAudio ("vocal.wav"));
    const auto segs = studio.getVariAudio ("vocal.wav")->segments;

    ExtractMidiOptions options;
    options.pitch = ExtractMidiOptions::Pitch::staticBend;
    options.bendRange = 2;
    options.volume = ExtractMidiOptions::Volume::dynamic;
    error = {};
    const auto made = studio.extractMidi (partId, options, Studio::ExtractDestination::newTrack, error);
    REQUIRE (made > 0);
    const auto found = studio.findPart (made);
    const auto& track = *studio.getProject().findTrack (found->first);
    CHECK (track.name.endsWith (" MIDI"));
    CHECK (! track.isAudio());
    const auto& part = track.parts[(size_t) found->second];
    REQUIRE (part.notes.size() == 4);
    CHECK (part.notes[0].pitch == 60);
    CHECK (part.notes[1].pitch == 64);
    CHECK (part.notes[2].pitch == 67);   // (66.75: G, bent down a quarter tone)
    CHECK (part.notes[3].pitch == 60);
    CHECK_THAT (part.notes[1].start, WithinAbs (segs[1].start * 2.0, 0.02));   // (120 BPM)
    CHECK (part.notes[2].velocity < part.notes[1].velocity);                   // (the quieter note)
    const auto bend = std::find_if (part.controllers.begin(), part.controllers.end(),
                                    [&] (const Controller& c) { return c.type == Controller::Type::pitchbend && c.start > part.notes[2].start - 0.01; });
    REQUIRE (bend != part.controllers.end());
    CHECK (std::abs (bend->value - (8192 - 1024)) < 120);   // (-0.25 of a 2-semitone range)
    CHECK (track.pitchbendUp == 2);
    CHECK (std::find (track.controllerLanes.begin(), track.controllerLanes.end(), 128) != track.controllerLanes.end());

    // Onto the selected track: the earlier extraction replaced, a controller curve this time.
    studio.selectTrack (track.id);
    options.pitch = ExtractMidiOptions::Pitch::continuousBend;
    options.volume = ExtractMidiOptions::Volume::controller;
    options.controller = 11;
    const auto again = studio.extractMidi (partId, options, Studio::ExtractDestination::selectedTrack, error);
    REQUIRE (again > 0);
    const auto& same = *studio.getProject().findTrack (track.id);
    CHECK (std::count_if (same.parts.begin(), same.parts.end(), [] (const Part& p) { return p.name.startsWith ("Extracted"); }) == 1);
    const auto& replaced = same.parts[0];
    CHECK (std::any_of (replaced.controllers.begin(), replaced.controllers.end(), [] (const Controller& c) { return c.type == Controller::Type::cc && c.number == 11; }));
    CHECK (std::count_if (replaced.controllers.begin(), replaced.controllers.end(), [] (const Controller& c) { return c.type == Controller::Type::pitchbend; }) >= 4);
    // No instrument track selected: refused.
    studio.selectTrack (studio.getProject().tracks[0].id == track.id ? studio.getProject().tracks[1].id : studio.getProject().tracks[0].id);
    if (! studio.canTakeMidi (studio.getSelectedTrack()))
        CHECK (studio.extractMidi (partId, options, Studio::ExtractDestination::selectedTrack, error) == 0);
}

TEST_CASE ("VariAudio 2: harmony voices: their notes against the sung one, with and without a chord", "[variaudio]")
{
    // Without a chord: +3, -3, -6, -9 (the manual's).
    CHECK (harmonyPitch (60, {}, -1, 0) == 63);
    CHECK (harmonyPitch (60, {}, -1, 1) == 57);
    CHECK (harmonyPitch (60, {}, -1, 2) == 54);
    CHECK (harmonyPitch (60, {}, -1, 3) == 51);
    // Over C major, C sung: E above, G below, E below that, C under it.
    const std::vector<int> c { 0, 4, 7 };
    CHECK (harmonyPitch (60, c, 0, 0) == 64);
    CHECK (harmonyPitch (60, c, 0, 1) == 55);
    CHECK (harmonyPitch (60, c, 0, 2) == 52);
    CHECK (harmonyPitch (60, c, 0, 3) == 48);
    // E sung over C/G: G above, C below; the bass the G.
    CHECK (harmonyPitch (64, c, 7, 0) == 67);
    CHECK (harmonyPitch (64, c, 7, 1) == 60);
    CHECK ((harmonyPitch (64, c, 7, 3) % 12) == 7);
}

TEST_CASE ("VariAudio 2 in the studio: Generate Harmony Voices makes tracks with their own recordings, heard at their notes", "[variaudio]")
{
    Studio studio (false);
    const auto partId = makeVocal (studio);
    const auto sourceTrack = studio.findPart (partId)->first;
    juce::String error;
    const auto voices = studio.generateHarmonyVoices (partId, 4, 0.5f, error);
    REQUIRE (voices.size() == 4);
    const auto& tracks = studio.getProject().tracks;
    const auto at = [&] (int id) { return (int) (std::find_if (tracks.begin(), tracks.end(), [id] (const Track& t) { return t.id == id; }) - tracks.begin()); };
    const char* names[] { "Soprano", "Alto", "Tenor", "Bass" };
    for (int v = 0; v < 4; ++v)
    {
        const auto& track = *studio.getProject().findTrack (voices[(size_t) v]);
        CHECK (track.name == names[v]);
        CHECK (at (track.id) == at (sourceTrack) + 1 + v);   // (in order under the original)
        REQUIRE (track.parts.size() == 1);
        const auto file = track.parts[0].audioFile;
        CHECK (file != "vocal.wav");
        CHECK (studio.getAudioFolder().getChildFile (file).existsAsFile());
        const auto* vari = studio.getVariAudio (file);
        REQUIRE (vari != nullptr);
        REQUIRE (vari->segments.size() == 4);
        static const int apart[] { 3, -3, -6, -9 };
        CHECK_THAT (vari->segments[0].pitchNow(), WithinAbs (60.0 + apart[v], 1.0e-3));
        CHECK (vari->segments[0].straighten == 0.5f);
    }
    // The original untouched; the soprano heard a minor third up.
    CHECK_FALSE (studio.getVariAudio ("vocal.wav")->isEdited());
    const auto& soprano = *studio.getProject().findTrack (voices[0]);
    const auto heard = studio.getHeardAudio (soprano.parts[0].audioFile);
    REQUIRE (heard != nullptr);
    const auto pitch = analysePitch (heard->sound, rate);
    const auto& first = studio.getVariAudio (soprano.parts[0].audioFile)->segments[0];
    CHECK_THAT (heardBetween (pitch, first.start + 0.08, first.end - 0.08), WithinAbs (63.0, 0.15));

    // With a chord on the chord track (C major over the clip): the soprano sings E over the C.
    Studio chorded (false);
    const auto chordedPart = makeVocal (chorded);
    chorded.addChordTrack();
    Chord cMajor;
    chorded.addChordEvent (0.0, cMajor);
    const auto made = chorded.generateHarmonyVoices (chordedPart, 1, 0.0f, error);
    REQUIRE (made.size() == 1);
    const auto* sopranoVari = chorded.getVariAudio (chorded.getProject().findTrack (made[0])->parts[0].audioFile);
    REQUIRE (sopranoVari != nullptr);
    CHECK_THAT (sopranoVari->segments[0].pitchNow(), WithinAbs (64.0, 1.0e-3));
    CHECK_THAT (sopranoVari->segments[1].pitchNow(), WithinAbs (67.0, 1.0e-3));   // (E sung: G above)
}

TEST_CASE ("VariAudio 2: the keys played reach the studio while MIDI Input asks for them", "[variaudio]")
{
    Studio studio (false);
    auto& input = studio.getEngine().getLiveInput();
    input.handleIncomingMidiMessage (nullptr, juce::MidiMessage::noteOn (1, 62, 0.8f));
    CHECK (studio.takePlayedNotes().empty());   // (not asked for)
    studio.setCatchingNotes (true);
    input.handleIncomingMidiMessage (nullptr, juce::MidiMessage::noteOn (1, 64, 0.8f));
    input.handleIncomingMidiMessage (nullptr, juce::MidiMessage::noteOff (1, 64));
    input.handleIncomingMidiMessage (nullptr, juce::MidiMessage::noteOn (1, 67, 0.8f));
    CHECK (studio.takePlayedNotes() == std::vector<int> { 64, 67 });
    CHECK (studio.takePlayedNotes().empty());
    studio.setCatchingNotes (false);
}

TEST_CASE ("The VariAudio editor, part two: the smart controls by hand (default and all), Alt-click, Delete", "[variaudio][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    const auto partId = makeVocal (studio);
    VariAudioEditor editor (studio, partId);
    editor.setSize (1400, 720);
    REQUIRE (editor.analyse());
    editor.fitView();
    const auto segs = [&] () -> const std::vector<VariSegment>& { return studio.getVariAudio ("vocal.wav")->segments; };
    REQUIRE (segs().size() == 4);
    using Control = VariAudioEditor::SmartControl;

    // The default set: no tilt, formant or volume; all of them with ALL.
    editor.setAllSmartControls (false);
    for (const auto c : { Control::straighten, Control::correct, Control::glueLeft, Control::glueRight, Control::split, Control::warpStart, Control::warpEnd })
        CHECK_FALSE (editor.getSmartControl (1, c).isEmpty());
    for (const auto c : { Control::tiltLeft, Control::tiltRight, Control::anchor, Control::rangeFrom, Control::rangeTo, Control::formant, Control::volume })
        CHECK (editor.getSmartControl (1, c).isEmpty());
    editor.setAllSmartControls (true);
    for (const auto c : { Control::tiltLeft, Control::tiltRight, Control::anchor, Control::rangeFrom, Control::rangeTo, Control::formant, Control::volume })
        CHECK_FALSE (editor.getSmartControl (1, c).isEmpty());
    for (const auto c : { Control::straighten, Control::correct, Control::glueLeft, Control::split, Control::warpStart, Control::tiltLeft, Control::anchor,
                          Control::rangeTo, Control::formant, Control::volume })
        CHECK (editor.smartControlAt (1, editor.getSmartControl (1, c).getCentre()) == c);

    // Tilt the end up two rows' worth; then Alt on the start: rotated round the anchor.
    const auto rows = editor.getSegmentBounds (1).getHeight();
    auto at = editor.getSmartControl (1, Control::tiltRight).getCentre();
    gesture (editor, 1, at, at.translated (0.0f, -2.0f * rows));
    CHECK_THAT (segs()[1].tiltEnd, WithinAbs (2.0, 0.01));
    CHECK (segs()[1].tiltStart == 0.0f);
    at = editor.getSmartControl (1, Control::tiltLeft).getCentre();
    gesture (editor, 1, at, at.translated (0.0f, -1.0f * rows), juce::ModifierKeys (juce::ModifierKeys::altModifier));
    CHECK_THAT (segs()[1].tiltStart, WithinAbs (1.0, 0.01));
    CHECK_THAT (segs()[1].tiltEnd, WithinAbs (1.0, 0.01));   // (2, less the rotation's 1 at the other end)

    // The anchor dragged to a quarter in; Alt-click: back to the middle.
    auto r = editor.getSegmentBounds (1);
    at = editor.getSmartControl (1, Control::anchor).getCentre();
    gesture (editor, 1, at, { r.getX() + r.getWidth() * 0.25f, at.y });
    CHECK_THAT (segs()[1].anchor, WithinAbs (0.25, 0.02));
    at = editor.getSmartControl (1, Control::anchor).getCentre();
    gesture (editor, 1, at, at, juce::ModifierKeys (juce::ModifierKeys::altModifier));
    CHECK (segs()[1].anchor == 0.5f);

    // The straighten range's start dragged to the middle; formant up 40 px (20 %); volume down 36 px (-6 dB).
    r = editor.getSegmentBounds (1);
    at = editor.getSmartControl (1, Control::rangeFrom).getCentre();
    gesture (editor, 1, at, { r.getCentreX(), at.y });
    CHECK_THAT (segs()[1].straightenFrom, WithinAbs (0.5, 0.03));
    at = editor.getSmartControl (1, Control::formant).getCentre();
    gesture (editor, 1, at, at.translated (0.0f, -40.0f));
    CHECK_THAT (segs()[1].formant, WithinAbs (20.0, 0.01));
    at = editor.getSmartControl (1, Control::volume).getCentre();
    gesture (editor, 1, at, at.translated (0.0f, 36.0f));
    CHECK_THAT (segs()[1].volumeDb, WithinAbs (-6.0, 0.01));
    CHECK (editor.getInfoText().contains ("FORMANT +20 %"));

    // Glue the second to the next (its control below its right corner): three left; split it again on its bottom edge.
    editor.selectSegments ({});
    at = editor.getSmartControl (1, Control::glueRight).getCentre();
    gesture (editor, 1, at, at);
    REQUIRE (segs().size() == 3);
    const auto glued = segs()[1];
    const auto bottom = editor.getSmartControl (1, Control::split);
    const juce::Point<float> splitAt { editor.xForSeconds ((glued.start + glued.end) * 0.5), bottom.getCentreY() };
    gesture (editor, 1, splitAt, splitAt);
    REQUIRE (segs().size() == 4);
    CHECK_THAT (segs()[1].end, WithinAbs ((glued.start + glued.end) * 0.5, 0.01));

    // Alt-click on a segment: split there too. Delete: the selected ones gone.
    const auto last = segs()[3];
    const juce::Point<float> inside { editor.xForSeconds (last.start + 0.2), editor.getSegmentBounds (3).getCentreY() };
    gesture (editor, 3, inside, inside, juce::ModifierKeys (juce::ModifierKeys::altModifier));
    REQUIRE (segs().size() == 5);
    editor.selectSegments ({ 3, 4 });
    CHECK (editor.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    CHECK (segs().size() == 3);
}

TEST_CASE ("The VariAudio editor, part two: warp start (in time) and Alt: the segment's edge", "[variaudio][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    const auto partId = makeVocal (studio);
    VariAudioEditor editor (studio, partId);
    editor.setSize (1400, 720);
    REQUIRE (editor.analyse());
    editor.fitView();
    using Control = VariAudioEditor::SmartControl;
    const auto seg = studio.getVariAudio ("vocal.wav")->segments[2];

    // Alt + the warp start control: the segment's own start moves (later), no warp.
    auto at = editor.getSmartControl (2, Control::warpStart).getCentre();
    gesture (editor, 2, at, at.translated (12.0f, 0.0f), juce::ModifierKeys (juce::ModifierKeys::altModifier));
    const auto moved = studio.getVariAudio ("vocal.wav")->segments[2];
    CHECK (moved.start > seg.start + 0.01);
    CHECK (moved.end == seg.end);
    const auto partOf = [&] { const auto found = studio.findPart (partId); return studio.getProject().findTrack (found->first)->parts[(size_t) found->second]; };
    CHECK_FALSE (partOf().isWarped());
    studio.undo();

    // Without Alt: its start heard 20 px later (warp markers).
    at = editor.getSmartControl (2, Control::warpStart).getCentre();
    gesture (editor, 2, at, at.translated (20.0f, 0.0f));
    REQUIRE (partOf().isWarped());
    const auto heardAt = partOf().start + warpBeatAt (partOf(), seg.start, studio.getProject().tempoMap());
    CHECK_THAT (editor.xForBeat (heardAt), WithinAbs (at.x + 20.0f, 1.5));
}

TEST_CASE ("The VariAudio editor, part two: MIDI Input and Step, the colours, the reference track, the panel, the dialogs", "[variaudio][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    const auto partId = makeVocal (studio);
    VariAudioEditor editor (studio, partId);
    editor.setSize (1400, 720);
    REQUIRE (editor.analyse());
    const auto segs = [&] () -> const std::vector<VariSegment>& { return studio.getVariAudio ("vocal.wav")->segments; };

    // MIDI Input: the first selected to the note played, the others as far; Step: then the next.
    editor.setMidiInput (true);
    CHECK (editor.keyNamed ("MIDI INPUT")->getToggleState());
    editor.selectSegments ({ 0, 1 });
    editor.notePlayed (62);
    CHECK_THAT (segs()[0].pitchNow(), WithinAbs (62.0, 1.0e-3));
    CHECK_THAT (segs()[1].pitchNow(), WithinAbs (66.0, 0.05));
    editor.setStepMode (true);
    editor.selectSegments ({ 2 });
    editor.notePlayed (67);
    CHECK_THAT (segs()[2].pitchNow(), WithinAbs (67.0, 1.0e-3));
    CHECK (editor.getSelection() == std::vector<int> { 3 });
    editor.notePlayed (100);   // (above C5: C5)
    CHECK_THAT (segs()[3].pitchNow(), WithinAbs (84.0, 1.0e-3));
    editor.setMidiInput (false);

    // The colours: by pitch (the family's palette), by scale (C major: the C in orange, an F# in pink).
    editor.setColours (VariAudioEditor::Colours::pitch);
    CHECK (editor.colourOf (0) == FamilyColours::paletteColour (2 * 8 / 11, 3));   // (D: pitch class 2)
    editor.setColours (VariAudioEditor::Colours::scale);
    editor.setEditorScale (Key::fromPitchClass (0, false));
    studio.editVariSegments ("vocal.wav", { 1 }, [] (VariSegment& s) { s.shift = 66.0f - s.pitch; });
    CHECK (editor.colourOf (2) == FamilyColours::orange);   // G
    CHECK (editor.colourOf (1) == FamilyColours::pink);     // F#
    editor.setColours (VariAudioEditor::Colours::automatic);

    // The reference track: an instrument track's notes; saved with the song.
    const auto melody = studio.addTrack ({});
    Part notes;
    notes.length = 4.0;
    notes.notes = { Note { 0.0, 1.0, 64, 0.8f } };
    studio.addPart (melody, notes);
    editor.setReferenceTrack (melody);
    CHECK (studio.getProject().variAudioReference == melody);
    CHECK (Project::fromXml (*studio.getProject().toXml())->variAudioReference == melody);
    CHECK (editor.createComponentSnapshot (editor.getLocalBounds()).isValid());

    // The panel: its keys there, scrolling in a short window.
    for (const auto* name : { "FUNCTIONS", "DEFAULT", "ALL", "MIDI INPUT", "STEP", "EDIT VARIAUDIO", "QUANTIZE PITCHES", "AUDITION" })
        CHECK (editor.keyNamed (name) != nullptr);
    editor.setSize (1100, 520);
    CHECK (editor.keyNamed ("AUDITION")->getParentComponent()->getHeight() > 520);

    // The dialogs' choices.
    ExtractMidiPage extract;
    CHECK (extract.getOptions().pitch == ExtractMidiOptions::Pitch::notes);
    CHECK (extract.getOptions().bendRange == 2);
    CHECK (extract.getOptions().velocity == 100);
    CHECK (extract.getDestination() == Studio::ExtractDestination::newTrack);
    HarmonyVoicesPage harmony;
    CHECK (harmony.getVoices() == 4);
    CHECK_THAT (harmony.getVibratoReduction(), WithinAbs (0.5, 1.0e-6));
    CHECK_FALSE (harmony.getOpenAfter());

    // Extract MIDI and the resets through the editor.
    juce::String error;
    CHECK (editor.extractMidi ({}, Studio::ExtractDestination::newTrack, error) > 0);
    editor.selectSegments ({});
    editor.resetSelected (Studio::VariReset::pitch);
    CHECK_FALSE (studio.getVariAudio ("vocal.wav")->isEdited());
    CHECK (editor.reanalyse());
}
