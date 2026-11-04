#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/VariAudio.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/MainView.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/VariAudioEditor.h"

// VariAudio, part one (0.34.0): the pitch found, the notes split into segments (not by a
// vibrato), the edited sound analysed again (moved, straightened, levelled; the rest untouched),
// the snap modes, the scale suggestions.

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    constexpr double rate = 48000.0;

    // A sung note: a pitch path (MIDI note at each moment), eight harmonics falling off, 10 ms in
    // and out. Notes are laid one after another; `gapAfter` seconds of silence after each.
    struct Note
    {
        double seconds = 0.4;
        float from = 60.0f, to = 60.0f;   // a glide from one to the other (the same: steady)
        float vibrato = 0.0f;              // semitones either way, at 5.5 Hz
        double gapAfter = 0.05;
    };

    juce::AudioBuffer<float> sing (const std::vector<Note>& notes, double lead = 0.1)
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
            for (int i = 0; i < length; ++i)
            {
                const auto t = i / rate;
                const auto midi = n.from + (n.to - n.from) * (float) (t / n.seconds)
                                + n.vibrato * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 5.5 * t);
                const auto hz = 440.0 * std::pow (2.0, (midi - 69.0) / 12.0);
                phase += 2.0 * juce::MathConstants<double>::pi * hz / rate;
                auto v = 0.0;
                for (int k = 1; k <= 8; ++k)
                    v += std::sin (k * phase) / k;
                const auto envelope = juce::jmin (1.0, juce::jmin (t / 0.01, (n.seconds - t) / 0.01));
                sound.setSample (0, at + i, (float) (0.3 * v * envelope));
            }
            at += length + (int) (n.gapAfter * rate);
        }
        return sound;
    }

    // The median pitch heard between two moments.
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

    float spreadBetween (const PitchAnalysis& a, double from, double to)
    {
        auto low = 200.0f, high = 0.0f;
        for (auto t = from; t < to; t += a.hopSeconds)
            if (const auto p = a.pitchAt (t); p > 0.0f)
                low = juce::jmin (low, p), high = juce::jmax (high, p);
        return high - low;
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

    juce::MouseEvent mouseAt (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt, juce::ModifierKeys mods = {})
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                                 mods.withFlags (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, now, downAt, now, 1, at != downAt);
    }

    // A song with one sung clip: C4, E4 (with vibrato), G4 slightly flat, C4 again.
    int makeVocal (Studio& studio)
    {
        writeWav (studio.getAudioFolder().getChildFile ("vocal.wav"),
                  sing ({ { 0.4, 60.0f, 60.0f, 0.0f, 0.15 }, { 0.6, 64.0f, 64.0f, 0.45f, 0.15 },
                          { 0.4, 66.75f, 66.75f, 0.0f, 0.15 }, { 0.5, 60.1f, 60.1f, 0.0f, 0.15 } }));
        const auto track = studio.addAudioTrack (false);
        Part clip;
        clip.length = 6.0;
        clip.audioFile = "vocal.wav";
        studio.addPart (track, clip);
        return studio.getProject().findTrack (track)->parts[0].id;
    }

    float rmsOf (const juce::AudioBuffer<float>& sound, double from, double to)
    {
        return sound.getRMSLevel (0, (int) (from * rate), (int) ((to - from) * rate));
    }
}

TEST_CASE ("VariAudio: the pitch is found every 5 ms; silence has none", "[variaudio]")
{
    const auto sound = sing ({ { 0.5, 57.0f, 57.0f }, { 0.5, 69.3f, 69.3f } });
    const auto analysis = analysePitch (sound, rate);
    CHECK_THAT (analysis.hopSeconds, WithinAbs (0.005, 1.0e-12));
    CHECK_THAT (heardBetween (analysis, 0.2, 0.5), WithinAbs (57.0, 0.05));     // A3, 220 Hz
    CHECK_THAT (heardBetween (analysis, 0.75, 1.05), WithinAbs (69.3, 0.05));   // a little sharp of A4
    CHECK (analysis.pitchAt (0.03) == 0.0f);   // (the silence before)
    CHECK (analysis.pitchAt (0.625) == 0.0f);  // (the gap between)
}

TEST_CASE ("VariAudio: the notes become segments; a glide to a new note splits, a vibrato or a scoop doesn't", "[variaudio]")
{
    // C4, E4 (a 0.4-semitone vibrato), then G4 straight into A4 (legato, no gap), then C5 with a scoop into it.
    const auto sound = sing ({ { 0.4, 60.0f, 60.0f, 0.0f, 0.05 },
                               { 0.6, 64.0f, 64.0f, 0.4f, 0.05 },
                               { 0.4, 67.0f, 67.0f, 0.0f, 0.0 },
                               { 0.4, 69.0f, 69.0f, 0.0f, 0.05 },
                               { 0.06, 70.0f, 72.0f, 0.0f, 0.0 },
                               { 0.4, 72.0f, 72.0f, 0.0f, 0.05 } });
    const auto analysis = analysePitch (sound, rate);
    const auto segments = segmentPitch (analysis);
    REQUIRE (segments.size() == 5);
    const float pitches[] = { 60.0f, 64.0f, 67.0f, 69.0f, 72.0f };
    for (size_t i = 0; i < 5; ++i)
        CHECK_THAT (segments[i].pitch, WithinAbs (pitches[i], 0.12));
    // Where they are: the first from 0.1 s to 0.5 s; the legato pair cut where G becomes A.
    CHECK_THAT (segments[0].start, WithinAbs (0.1, 0.02));
    CHECK_THAT (segments[0].end, WithinAbs (0.5, 0.02));
    CHECK_THAT (segments[2].end, WithinAbs (segments[3].start, 1.0e-9));
    CHECK_THAT (segments[3].start, WithinAbs (0.1 + 0.45 + 0.65 + 0.4, 0.03));
    for (const auto& s : segments)
        CHECK_FALSE (s.isEdited());
}

TEST_CASE ("VariAudio: moved, straightened and levelled segments heard again; the rest is the recording itself", "[variaudio]")
{
    const auto sound = sing ({ { 0.4, 60.0f, 60.0f, 0.0f, 0.2 },
                               { 0.6, 64.0f, 64.0f, 0.5f, 0.2 },
                               { 0.4, 67.0f, 67.0f, 0.0f, 0.2 } });
    const auto analysis = analysePitch (sound, rate);
    auto segments = segmentPitch (analysis);
    REQUIRE (segments.size() == 3);

    // The first two semitones up; the second straightened; the third 6 dB down.
    segments[0].shift = 2.0f;
    segments[1].straighten = 1.0f;
    segments[2].volumeDb = -6.0f;
    const auto edited = renderVariAudio (sound, rate, analysis, segments);
    REQUIRE (edited.getNumSamples() == sound.getNumSamples());
    const auto again = analysePitch (edited, rate);
    const auto middle = [] (const VariSegment& s, double& a, double& b) { a = s.start + 0.08; b = s.end - 0.08; };
    double a = 0, b = 0;
    middle (segments[0], a, b);
    CHECK_THAT (heardBetween (again, a, b), WithinAbs (62.0, 0.1));
    middle (segments[1], a, b);
    CHECK_THAT (heardBetween (again, a, b), WithinAbs (64.0, 0.15));
    CHECK (spreadBetween (analysis, a, b) > 0.8f);     // (it had its vibrato)
    CHECK (spreadBetween (again, a, b) < 0.35f);       // (now mostly straight)
    middle (segments[2], a, b);
    CHECK_THAT (rmsOf (edited, a, b) / rmsOf (sound, a, b), WithinAbs (0.501, 0.01));
    CHECK_THAT (heardBetween (again, a, b), WithinAbs (67.0, 0.05));
    // Between the edited regions (the gap's middle), the recording sample for sample.
    const auto quiet = (int) ((segments[0].end + 0.1) * rate);
    for (int i = quiet - 100; i < quiet + 100; ++i)
        CHECK (edited.getSample (0, i) == sound.getSample (0, i));
    const auto lastStart = (int) (segments[2].start * rate) - 400;   // (before the third's ramp)
    CHECK (edited.getSample (0, lastStart) == sound.getSample (0, lastStart));
    // Nothing edited: the recording itself.
    for (auto& s : segments)
        s = { s.start, s.end, s.pitch };
    const auto same = renderVariAudio (sound, rate, analysis, segments);
    for (int i = 0; i < sound.getNumSamples(); i += 97)
        CHECK (same.getSample (0, i) == sound.getSample (0, i));
}

TEST_CASE ("VariAudio: the snap modes, Correct Pitch, quantize to a scale, scale suggestions", "[variaudio]")
{
    VariSegment s;
    s.pitch = 60.3f;   // 30 cents sharp of C4
    CHECK_THAT (movedShift (s, 1.0f, PitchSnap::absolute), WithinAbs (61.0 - 60.3, 1.0e-5));   // onto C#4
    CHECK_THAT (movedShift (s, 1.0f, PitchSnap::relative), WithinAbs (1.0, 1.0e-5));           // C#4, 30 cents sharp
    CHECK_THAT (movedShift (s, 0.4f, PitchSnap::off), WithinAbs (0.4, 1.0e-5));
    CHECK_THAT (movedShift (s, 0.4f, PitchSnap::relative), WithinAbs (0.0, 1.0e-5));           // (less than half: stays)
    CHECK_THAT (correctedShift (s, 1.0f), WithinAbs (-0.3, 1.0e-5));
    CHECK_THAT (correctedShift (s, 0.5f), WithinAbs (-0.15, 1.0e-5));
    // C#4 isn't in C major: to the nearer of C and D (C).
    s.pitch = 61.2f;
    CHECK_THAT (s.pitch + quantizedShift (s, Key::fromPitchClass (0, false), PitchSnap::absolute), WithinAbs (62.0, 1.0e-5));
    s.pitch = 60.9f;
    CHECK_THAT (s.pitch + quantizedShift (s, Key::fromPitchClass (0, false), PitchSnap::absolute), WithinAbs (60.0, 1.0e-5));
    s.pitch = 61.1f;   // relative keeps its 10 cents
    CHECK_THAT (s.pitch + quantizedShift (s, Key::fromPitchClass (0, false), PitchSnap::relative), WithinAbs (62.1, 1.0e-5));

    // A melody in G major (F# and the G at the end): G major first.
    std::vector<VariSegment> melody;
    const float notes[] = { 67, 69, 71, 72, 74, 76, 78, 79 };
    double t = 0.0;
    for (const auto n : notes)
    {
        melody.push_back ({ t, t + 0.4, n });
        t += 0.5;
    }
    const auto keys = suggestScales (melody);
    REQUIRE_FALSE (keys.empty());
    CHECK (keys.front() == Key::fromPitchClass (7, false));
}

TEST_CASE ("VariAudio in the studio: Edit VariAudio finds the segments; an edit is heard (played, warped), undone, bypassed, saved", "[variaudio]")
{
    Studio studio (false);
    REQUIRE (juce::approximatelyEqual (studio.getEngine().getSampleRate(), rate));
    writeWav (studio.getAudioFolder().getChildFile ("vocal.wav"), sing ({ { 0.4, 60.0f, 60.0f, 0.0f, 0.2 }, { 0.4, 64.0f, 64.0f, 0.0f, 0.2 } }));
    const auto track = studio.addAudioTrack (false);
    Part clip;
    clip.length = 3.0;
    clip.audioFile = "vocal.wav";
    studio.addPart (track, clip);
    CHECK (studio.getVariAudio ("vocal.wav") == nullptr);
    REQUIRE (studio.editVariAudio ("vocal.wav"));
    const auto* vari = studio.getVariAudio ("vocal.wav");
    REQUIRE (vari != nullptr);
    REQUIRE (vari->segments.size() == 2);
    const auto original = studio.getAudio ("vocal.wav");
    CHECK (studio.getHeardAudio ("vocal.wav") == original);   // (nothing edited: the recording itself)

    // The second note up a minor third (Correct Pitch first, then three semitones).
    studio.editVariSegments ("vocal.wav", { 1 }, [] (VariSegment& s) { s.shift = movedShift (s, 3.0f, PitchSnap::absolute); });
    const auto& second = studio.getVariAudio ("vocal.wav")->segments[1];
    CHECK_THAT (second.pitchNow(), WithinAbs (67.0, 1.0e-4));
    const auto heard = studio.getHeardAudio ("vocal.wav");
    REQUIRE (heard != original);
    const auto heardPitch = analysePitch (heard->sound, rate);
    CHECK_THAT (heardBetween (heardPitch, second.start + 0.08, second.end - 0.08), WithinAbs (67.0, 0.1));
    CHECK (studio.getHeardAudio ("vocal.wav") == heard);   // (made once)

    // A warped clip plays the edited sound too (not a stretch made before the edit).
    Part warped = clip;
    warped.warp = { { 0.0, 0.0 }, { 3.0, 1.5 } };   // (at its own speed: 3 beats at 120 = 1.5 s)
    const auto warpedSound = studio.getWarpedSound (warped, studio.getProject().tempoMap());
    REQUIRE (warpedSound != nullptr);
    const auto warpedPitch = analysePitch (*warpedSound, rate);
    CHECK_THAT (heardBetween (warpedPitch, second.start + 0.08, second.end - 0.08), WithinAbs (67.0, 0.15));

    // Bypass: the recording again (and back).
    studio.setVariAudioBypass ("vocal.wav", true);
    CHECK (studio.getHeardAudio ("vocal.wav") == original);
    studio.setVariAudioBypass ("vocal.wav", false);
    CHECK (studio.getHeardAudio ("vocal.wav") != original);

    // Saved and opened with the song.
    const auto songFile = studio.getAudioFolder().getParentDirectory().getChildFile ("vari test.pr7s");
    REQUIRE (studio.save (songFile));
    {
        Studio other (false);
        juce::String problems;
        REQUIRE (other.open (songFile, problems));
        const auto* kept = other.getVariAudio ("vocal.wav");
        REQUIRE (kept != nullptr);
        CHECK (kept->segments == studio.getVariAudio ("vocal.wav")->segments);
    }

    // Undo: the edit gone (the segments still there), then the analysis.
    studio.undo();
    CHECK_FALSE (studio.getVariAudio ("vocal.wav")->isEdited());
    CHECK (studio.getHeardAudio ("vocal.wav") == original);
    studio.undo();
    CHECK (studio.getVariAudio ("vocal.wav") == nullptr);
}

TEST_CASE ("The VariAudio editor: analyse, select, drag a segment up (snapped), straighten by its smart control, keys, quantize to the scale", "[variaudio][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    const auto partId = makeVocal (studio);
    VariAudioEditor editor (studio, partId);
    editor.setSize (1400, 720);
    REQUIRE (editor.analyse());
    const auto segments = [&] { return studio.getVariAudio ("vocal.wav")->segments; };
    REQUIRE (segments().size() == 4);
    editor.fitView();
    editor.setSnap (PitchSnap::absolute);

    // A click selects the second; dragged up two semitones' rows: on F#4 (66), exactly.
    const auto r = editor.getSegmentBounds (1);
    const auto from = r.getCentre(), to = from.translated (0.0f, -2.0f * r.getHeight());
    editor.mouseDown (mouseAt (editor, from, from));
    editor.mouseDrag (mouseAt (editor, to, from));
    editor.mouseUp (mouseAt (editor, to, from));
    CHECK (editor.getSelection() == std::vector<int> { 1 });
    CHECK_THAT (segments()[1].pitchNow(), WithinAbs (66.0, 1.0e-4));
    studio.undo();
    studio.dispatchPendingMessages();
    CHECK_THAT (segments()[1].shift, WithinAbs (0.0, 1.0e-6));

    // Its straighten control (top middle), dragged up 60 px: half straightened.
    editor.mouseMove (mouseAt (editor, r.getCentre(), r.getCentre()));
    const auto handle = editor.getStraightenHandle (1).getCentre();
    editor.mouseMove (mouseAt (editor, handle, handle));
    editor.mouseDown (mouseAt (editor, handle, handle));
    editor.mouseDrag (mouseAt (editor, handle.translated (0.0f, -60.0f), handle));
    editor.mouseUp (mouseAt (editor, handle.translated (0.0f, -60.0f), handle));
    CHECK_THAT (segments()[1].straighten, WithinAbs (0.5, 0.01));

    // Up: a semitone (absolute: onto F4); Shift+Up: a cent; Right: the next segment.
    CHECK (editor.keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
    CHECK_THAT (segments()[1].pitchNow(), WithinAbs (65.0, 1.0e-4));
    CHECK (editor.keyPressed (juce::KeyPress (juce::KeyPress::upKey, juce::ModifierKeys::shiftModifier, 0)));
    CHECK_THAT (segments()[1].pitchNow(), WithinAbs (65.01, 1.0e-4));
    CHECK (editor.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    CHECK (editor.getSelection() == std::vector<int> { 2 });

    // Ctrl-click C4's key: both C4s.
    const auto keyArea = editor.getKeyboardArea().toFloat();
    const juce::Point<float> onC4 (keyArea.getCentreX(), editor.yForPitch (60.0f));
    editor.mouseDown (mouseAt (editor, onC4, onC4, juce::ModifierKeys (juce::ModifierKeys::commandModifier)));
    editor.mouseUp (mouseAt (editor, onC4, onC4, juce::ModifierKeys (juce::ModifierKeys::commandModifier)));
    CHECK (editor.getSelection() == std::vector<int> { 0, 3 });

    // The flat G (66.75) quantized to C major: G4 (67), absolute.
    editor.selectSegments ({ 2 });
    editor.setEditorScale (Key::fromPitchClass (0, false));
    editor.keyNamed ("QUANTIZE PITCHES")->onClick();
    CHECK_THAT (segments()[2].pitchNow(), WithinAbs (67.0, 1.0e-4));
    // Correct Pitch by the panel's key path (100 %): the last C4 (60.1) onto C4.
    editor.selectSegments ({ 3 });
    editor.correctSelected (1.0f);
    CHECK_THAT (segments()[3].pitchNow(), WithinAbs (60.0, 1.0e-4));
    // Suggestions for all of them: C major (or its relative) first.
    editor.selectSegments ({});
    const auto suggestions = editor.getScaleSuggestions();
    REQUIRE_FALSE (suggestions.empty());
    CHECK ((suggestions.front() == Key::fromPitchClass (0, false) || suggestions.front() == Key::fromPitchClass (9, true)));
    // Bypass and back.
    editor.keyNamed ("BYPASS")->onClick();
    CHECK (studio.getVariAudio ("vocal.wav")->bypass);
}

TEST_CASE ("The VariAudio editor opens from the arrangement (double-click an audio clip) and follows the clip chosen", "[variaudio][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto partId = makeVocal (studio);
    CHECK (view.getVariAudioWindow() == nullptr);
    view.getArrangeView().onEditAudio (partId);
    auto* window = dynamic_cast<VariAudioWindow*> (view.getVariAudioWindow());
    REQUIRE (window != nullptr);
    CHECK (window->getEditor().getPartId() == partId);
}

TEST_CASE ("Snapshot: the VariAudio editor", "[.][snapshot-variaudio]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "variaudio.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        Studio studio (false);
        const auto partId = makeVocal (studio);
        VariAudioEditor editor (studio, partId);
        editor.setSize (1400, 720);
        editor.analyse();
        editor.fitView();
        editor.selectSegments ({ 2 });
        editor.setEditorScale (Key::fromPitchClass (0, false));
        editor.setScaleGuides (true);
        editor.quantizeSelected();                   // (the flat G onto G)
        editor.selectSegments ({ 1 });
        editor.straightenSelected (0.8f);             // (most of the vibrato gone)
        editor.selectSegments ({ 3 });
        editor.moveSelected (2.0f, PitchSnap::absolute);   // (the last C up to D)
        // (0.35.0, part two) Every smart control; the first note's curve tilted up at its end, the
        // second straightened over its middle only, the third's formants moved; a melody track's
        // notes as the reference lines.
        editor.setAllSmartControls (true);
        studio.editVariSegments ("vocal.wav", { 0 }, [] (VariSegment& s) { s.tiltEnd = 1.5f; s.anchor = 0.3f; });
        studio.editVariSegments ("vocal.wav", { 1 }, [] (VariSegment& s) { s.straightenFrom = 0.3f; s.straightenTo = 0.8f; });
        studio.editVariSegments ("vocal.wav", { 2 }, [] (VariSegment& s) { s.formant = 30.0f; });
        const auto melody = studio.addTrack ({});
        Part line;
        line.length = 6.0;
        line.notes = { { 0.2, 0.8, 60, 0.8f }, { 1.1, 1.2, 64, 0.8f }, { 2.6, 0.8, 67, 0.8f }, { 3.6, 1.0, 62, 0.8f } };
        studio.addPart (melody, line);
        editor.setReferenceTrack (melody);
        editor.selectSegments ({ 3 });
        const auto r = editor.getSegmentBounds (3);
        editor.mouseMove (mouseAt (editor, r.getCentre(), r.getCentre()));   // (its smart controls shown)
        const auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}
