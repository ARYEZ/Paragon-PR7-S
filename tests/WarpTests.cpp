#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "engine/Warp.h"
#include "ui/Finish.h"
#include "ui/MainView.h"

// Hitpoints and audio warping (0.32.0): found hitpoints, the filters and hand edits, warp markers'
// arithmetic, the stretched sound (timing and pitch kept), the studio's actions and the engine.

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    juce::MouseEvent mouseAt (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt, juce::ModifierKeys mods = {})
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                                 mods.withFlags (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, now, downAt, now, 1, at != downAt);
    }

    constexpr double rate = 48000.0;

    // Plucks: a 440 Hz tone struck at each time (an instant attack, decaying over 0.15 s), else silence.
    juce::AudioBuffer<float> plucks (const std::vector<double>& times, double seconds, std::vector<float> levels = {})
    {
        juce::AudioBuffer<float> sound (2, (int) (seconds * rate));
        sound.clear();
        for (size_t p = 0; p < times.size(); ++p)
        {
            const auto level = p < levels.size() ? levels[p] : 0.8f;
            const auto start = (int) (times[p] * rate);
            for (int i = 0; start + i < sound.getNumSamples() && i < (int) (0.3 * rate); ++i)
            {
                const auto value = level * (float) (std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / rate) * std::exp (-i / (0.15 * rate)));
                for (int c = 0; c < 2; ++c)
                    sound.setSample (c, start + i, value);
            }
        }
        return sound;
    }

    void writeWav (const juce::File& file, const juce::AudioBuffer<float>& sound)
    {
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2)
                                                                           .withBitsPerSample (32)
                                                                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (sound, 0, sound.getNumSamples());
    }

    // Where the first sound at or after `from` seconds starts (a sample over a tenth of full scale).
    double firstSoundAfter (const juce::AudioBuffer<float>& sound, double from)
    {
        for (int i = (int) (from * rate); i < sound.getNumSamples(); ++i)
            if (std::abs (sound.getSample (0, i)) > 0.1f)
                return i / rate;
        return -1.0;
    }

    // Upward zero crossings per second over a stretch: the pitch of a steady tone.
    double frequencyOf (const juce::AudioBuffer<float>& sound, double from, double to)
    {
        auto crossings = 0;
        for (int i = (int) (from * rate) + 1; i < (int) (to * rate) && i < sound.getNumSamples(); ++i)
            if (sound.getSample (0, i - 1) < 0.0f && sound.getSample (0, i) >= 0.0f)
                ++crossings;
        return crossings / (to - from);
    }
}

TEST_CASE ("Hitpoints are found where the sound starts, filtered and edited by hand", "[warp][hitpoints]")
{
    const std::vector<double> times { 0.5, 1.0, 1.37, 2.0, 2.6 };
    const auto sound = plucks (times, 3.0, { 0.8f, 0.8f, 0.8f, 0.8f, 0.05f });
    const auto all = detectHitpoints (sound, rate);
    // Each pluck found within 3 ms of its start (the quiet last one too, weaker).
    for (const auto t : times)
        CHECK (std::any_of (all.begin(), all.end(), [t] (const Hitpoint& h) { return std::abs (h.seconds - t) < 0.003; }));
    const auto kept = filterHitpoints (all, 0.25, 0.06);
    REQUIRE (kept.size() == 4);   // the quiet one under the threshold
    for (size_t i = 0; i < 4; ++i)
        CHECK_THAT (kept[i], WithinAbs (times[i], 0.003));
    // Minimum length: none within half a second of a stronger one kept.
    CHECK (filterHitpoints (all, 0.25, 0.5).size() == 3);   // 1.37 is within 0.5 of 1.0
    // Hand edits: one removed, one added.
    Project::HitpointEdits edits;
    edits.removed = { kept[1] };
    edits.added = { 2.8 };
    const auto edited = filterHitpoints (all, 0.25, 0.06, &edits);
    REQUIRE (edited.size() == 4);
    CHECK (std::none_of (edited.begin(), edited.end(), [&] (double s) { return std::abs (s - kept[1]) < 0.001; }));
    CHECK_THAT (edited.back(), WithinAbs (2.8, 1.0e-9));
    // Silence: none.
    juce::AudioBuffer<float> quiet (2, (int) rate);
    quiet.clear();
    CHECK (detectHitpoints (quiet, rate).empty());
}

TEST_CASE ("Warp markers: where the clip is in its recording, both ways; trims keep the audio in place; quantize", "[warp]")
{
    const TempoMap tempo (120.0);   // (a beat is half a second)
    Part clip;
    clip.start = 4.0;
    clip.length = 8.0;
    clip.audioFile = "take.wav";
    clip.audioOffset = 1.0;
    // Unwarped: its own speed.
    CHECK_THAT (warpSecondsAt (clip, 2.0, tempo), WithinAbs (2.0, 1.0e-9));
    CHECK_THAT (warpBeatAt (clip, 2.0, tempo), WithinAbs (2.0, 1.0e-9));
    ensureWarp (clip, tempo);
    REQUIRE (clip.warp.size() == 2);
    CHECK_THAT (clip.warp.back().seconds, WithinAbs (5.0, 1.0e-9));
    // A marker: the recording's 2 s (beat 2 at its own speed) heard at beat 3 instead.
    clip.warp.insert (clip.warp.begin() + 1, { 3.0, 2.0 });
    CHECK_THAT (warpSecondsAt (clip, 3.0, tempo), WithinAbs (2.0, 1.0e-9));
    CHECK_THAT (warpSecondsAt (clip, 1.5, tempo), WithinAbs (1.5, 1.0e-9));       // halfway to it: stretched (slower)
    CHECK_THAT (warpSecondsAt (clip, 5.5, tempo), WithinAbs (3.5, 1.0e-9));       // halfway after it: squeezed
    for (const auto beat : { 0.5, 2.0, 3.0, 4.7, 7.9 })
        CHECK_THAT (warpBeatAt (clip, warpSecondsAt (clip, beat, tempo), tempo), WithinAbs (beat, 1.0e-9));
    const auto factors = warpFactors (clip, tempo);
    REQUIRE (factors.size() == 3);
    CHECK_THAT (factors[1], WithinAbs (1.5, 1.0e-9));   // 1.5 s of song for 1 s of recording
    CHECK_THAT (factors[2], WithinAbs (2.5 / 3.0, 1.0e-9));

    // Trimmed at the start by a beat: the audio stays where it was in the song.
    auto trimmed = clip;
    trimmed.start = 5.0;
    trimmed.length = 7.0;
    rebaseWarp (clip, trimmed, tempo);
    for (const auto songBeat : { 5.0, 6.0, 7.0, 9.0, 11.5 })
        CHECK_THAT (warpSecondsAt (trimmed, songBeat - trimmed.start, tempo), WithinAbs (warpSecondsAt (clip, songBeat - clip.start, tempo), 1.0e-9));
    CHECK_THAT (trimmed.warp.front().beat, WithinAbs (0.0, 1.0e-12));
    CHECK_THAT (trimmed.warp.back().beat, WithinAbs (7.0, 1.0e-12));

    // AudioWarp Quantize: markers near beats (0.9 and 2.1 into a clip on the bar) to them; the edges stay.
    Part loose;
    loose.start = 0.0;
    loose.length = 4.0;
    loose.audioFile = "take.wav";
    loose.warp = { { 0.0, 0.0 }, { 0.9, 0.45 }, { 1.2, 0.6 }, { 2.1, 1.05 }, { 4.0, 2.0 } };
    quantizeWarp (loose, 1.0, tempo);
    CHECK_THAT (loose.warp[1].beat, WithinAbs (1.0, 1.0e-9));
    CHECK_THAT (loose.warp[3].beat, WithinAbs (2.0, 1.0e-9));
    CHECK (loose.warp[2].beat > 1.0);          // (between: in proportion)
    CHECK (loose.warp[2].beat < 2.0);
    CHECK_THAT (loose.warp.back().beat, WithinAbs (4.0, 1.0e-9));
    CHECK_THAT (loose.warp[1].seconds, WithinAbs (0.45, 1.0e-12));   // (the audio's moments stay theirs)
}

TEST_CASE ("The warped sound: a hit moved to its new place, the pitch kept; unmoved stretches copied as they are", "[warp][render]")
{
    const TempoMap tempo (120.0);
    const auto sound = plucks ({ 0.5, 1.0, 1.5 }, 2.5);
    Part clip;
    clip.length = 5.0;   // 2.5 s
    clip.audioFile = "take.wav";
    ensureWarp (clip, tempo);
    WarpRenderer renderer;
    // At its own speed: the recording, sample for sample.
    const auto same = renderer.render (clip, sound, rate, tempo);
    REQUIRE (same != nullptr);
    REQUIRE (same->getNumSamples() == (int) (2.5 * rate));
    CHECK_THAT (same->getSample (0, (int) (1.0 * rate) + 100), WithinAbs (sound.getSample (0, (int) (1.0 * rate) + 100), 1.0e-6));

    // The hit at 1.0 s (beat 2) moved to beat 2.4 (1.2 s); the ones at 0.5 and 1.5 pinned.
    clip.warp = { { 0.0, 0.0 }, { 1.0, 0.5 }, { 2.4, 1.0 }, { 3.0, 1.5 }, { 5.0, 2.5 } };
    const auto moved = renderer.render (clip, sound, rate, tempo);
    REQUIRE (moved != nullptr);
    CHECK (moved->getNumSamples() == (int) (2.5 * rate));
    CHECK_THAT (firstSoundAfter (*moved, 0.4), WithinAbs (0.5, 0.004));
    CHECK_THAT (firstSoundAfter (*moved, 1.1), WithinAbs (1.2, 0.006));
    CHECK_THAT (firstSoundAfter (*moved, 1.4), WithinAbs (1.5, 0.006));
    // Stretched, its pitch stays (440 Hz, within 3%).
    CHECK_THAT (frequencyOf (*moved, 1.22, 1.38), WithinAbs (440.0, 13.0));
    // Moving one marker again remakes only the stretches beside it.
    const auto before = renderer.piecesKept();
    clip.warp[2].beat = 2.5;
    renderer.render (clip, sound, rate, tempo);
    CHECK (renderer.piecesKept() == before + 2);
}

TEST_CASE ("The studio: warp markers from hitpoints, moved and heard where they go; cut, reset, flattened, quantized, saved", "[warp][studio]")
{
    Studio studio (false);
    const auto trackId = studio.addAudioTrack (true);
    writeWav (studio.getAudioFolder().getChildFile ("take.wav"), plucks ({ 0.5, 1.0, 1.5 }, 2.5));
    Part clip;
    clip.length = 5.0;
    clip.audioFile = "take.wav";
    clip.name = "take";
    studio.addPart (trackId, clip);
    const auto id = studio.getProject().findTrack (trackId)->parts[0].id;
    const auto part = [&]() -> const Part& { return studio.getProject().findTrack (trackId)->parts.at (0); };

    // Hitpoints in the clip, as beats (120 BPM: 1, 2, 3).
    const auto hits = studio.getPartHitpoints (id);
    REQUIRE (hits.size() == 3);
    CHECK_THAT (hits[1], WithinAbs (2.0, 0.01));
    // Alt+N's next hitpoint from beat 1.5.
    REQUIRE (studio.nextHitpoint (trackId, 1.5, true).has_value());
    CHECK_THAT (*studio.nextHitpoint (trackId, 1.5, true), WithinAbs (2.0, 0.01));
    CHECK_THAT (*studio.nextHitpoint (trackId, 1.5, false), WithinAbs (1.0, 0.01));

    // Markers at the hitpoints, then the middle one moved to beat 2.4: the engine plays the hit there.
    REQUIRE (studio.createWarpFromHitpoints ({ id }));
    REQUIRE (part().warp.size() == 5);
    REQUIRE (studio.moveWarpMarker (id, 2, 2.4));
    CHECK_THAT (part().warp[2].beat, WithinAbs (2.4, 1.0e-9));
    studio.setPosition (0.0);
    studio.play();
    std::vector<float> right;
    const auto heard = renderSamples (studio.getEngine(), (int) (2.0 * rate), 512, &right);
    studio.stop();
    juce::AudioBuffer<float> heardBuffer (1, (int) heard.size());
    for (size_t i = 0; i < heard.size(); ++i)
        heardBuffer.setSample (0, (int) i, heard[i]);
    CHECK_THAT (firstSoundAfter (heardBuffer, 1.1), WithinAbs (1.2, 0.01));

    // A marker can't pass its neighbours, nor the edges move.
    studio.moveWarpMarker (id, 2, 10.0);
    CHECK (part().warp[2].beat < part().warp[3].beat);
    CHECK_FALSE (studio.moveWarpMarker (id, 0, 1.0));

    // Cut at beat 2.2: both halves keep the audio where it was.
    const auto tempo = studio.getProject().tempoMap();
    const auto whole = part();
    studio.splitPart (trackId, 0, 2.2);
    const auto& halves = studio.getProject().findTrack (trackId)->parts;
    REQUIRE (halves.size() == 2);
    for (const auto& half : halves)
    {
        CHECK (half.isWarped());
        for (const auto beat : { 0.1, 0.6, 1.0 })
            CHECK_THAT (warpSecondsAt (half, beat, tempo), WithinAbs (warpSecondsAt (whole, half.start + beat - whole.start, tempo), 1.0e-9));
    }
    studio.undo();
    REQUIRE (studio.getProject().findTrack (trackId)->parts.size() == 1);

    // Flatten: a new file that sounds as the warp did; no markers.
    juce::String error;
    const auto warpedSound = studio.getWarpedSound (part(), tempo);
    REQUIRE (warpedSound != nullptr);
    CHECK (studio.flattenWarp ({ id }, error) == 1);
    CHECK_FALSE (part().isWarped());
    CHECK (part().audioFile.contains ("flattened"));
    const auto flat = studio.getAudio (part().audioFile);
    REQUIRE (flat != nullptr);
    CHECK_THAT (flat->sound.getSample (0, (int) (1.25 * rate)), WithinAbs (warpedSound->getSample (0, (int) (1.25 * rate)), 1.0e-5));
    studio.undo();
    CHECK (part().isWarped());

    // Reset: at its own speed again.
    studio.resetWarp ({ id });
    CHECK_FALSE (part().isWarped());

    // Quantize: the start to the grid; with AudioWarp Quantize, the hitpoints' markers to it.
    studio.updatePart (trackId, 0, [&] { auto p = part(); p.start = 0.1; return p; }());
    CHECK (studio.quantizeAudio ({ id }, 1.0));
    CHECK_THAT (part().start, WithinAbs (0.0, 1.0e-9));
    studio.setAudioWarpQuantize (true);
    studio.setWarpMarkers (id, { { 0.0, 0.0 }, { 1.1, 0.5 }, { 1.9, 1.0 }, { 3.2, 1.5 }, { 5.0, 2.5 } });
    CHECK (studio.quantizeAudio ({ id }, 1.0));
    CHECK_THAT (part().warp[1].beat, WithinAbs (1.0, 1.0e-9));
    CHECK_THAT (part().warp[2].beat, WithinAbs (2.0, 1.0e-9));
    CHECK_THAT (part().warp[3].beat, WithinAbs (3.0, 1.0e-9));

    // Saved and opened with the song: the markers, the settings, the hand edits.
    studio.setHitpointSettings ({ 0.4, 0.1 });
    studio.removeHitpoint ("take.wav", 1.0);
    const auto again = Project::fromXml (*studio.getProject().toXml());
    REQUIRE (again.has_value());
    CHECK (again->tracks.back().parts.at (0).warp == part().warp);
    CHECK (again->hitpointSettings == studio.getProject().hitpointSettings);
    CHECK (again->hitpointEdits == studio.getProject().hitpointEdits);
    CHECK (again->audioWarpQuantize);
    studio.getAudioFolder().deleteRecursively();
}

TEST_CASE ("On screen: WARP catches a hitpoint and drags it; a modifier click removes a marker; Alt+N goes to the next hitpoint", "[warp][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.addAudioTrack (true);
    writeWav (studio.getAudioFolder().getChildFile ("take.wav"), plucks ({ 0.5, 1.0, 1.5 }, 2.5));
    Part clip;
    clip.length = 5.0;
    clip.audioFile = "take.wav";
    studio.addPart (trackId, clip);
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto& timeline = arrange.getTimeline();
    timeline.pixelsPerBeat = 120.0;
    timeline.firstBeat = 0.0;
    const auto& part = [&]() -> const Part& { return studio.getProject().findTrack (trackId)->parts.at (0); };
    const auto bounds = arrange.getPartBounds (trackId, 0);
    const auto y = bounds.getCentreY();

    // A click 5 px from the hitpoint at beat 2 catches it; dragged to beat 2.5.
    arrange.setTool (ArrangeView::Tool::warp);
    const juce::Point<float> from (timeline.xForBeat (2.0) + 5.0f, y), to (timeline.xForBeat (2.5) + 5.0f, y);
    arrange.mouseDown (mouseAt (arrange, from, from));
    for (int step = 1; step <= 4; ++step)
        arrange.mouseDrag (mouseAt (arrange, from + (to - from) * ((float) step / 4.0f), from, juce::ModifierKeys (juce::ModifierKeys::ctrlModifier)));
    arrange.mouseUp (mouseAt (arrange, to, from, juce::ModifierKeys (juce::ModifierKeys::ctrlModifier)));
    REQUIRE (part().isWarped());
    REQUIRE (part().warp.size() == 3);
    CHECK_THAT (part().warp[1].seconds, WithinAbs (1.0, 0.004));                 // (the hit's own moment)
    CHECK_THAT (part().warp[1].beat, WithinAbs (2.5 + 5.0 / 120.0, 0.01));       // (Ctrl: no grid pull)
    CHECK (arrange.warpMarkerAt (trackId, 0, { timeline.xForBeat (part().warp[1].beat), y }) == 1);

    // Shift and a click on it: removed (back at its own speed: no markers left).
    const juce::Point<float> onMarker (timeline.xForBeat (part().warp[1].beat), y);
    arrange.mouseDown (mouseAt (arrange, onMarker, onMarker, juce::ModifierKeys (juce::ModifierKeys::shiftModifier)));
    arrange.mouseUp (mouseAt (arrange, onMarker, onMarker, juce::ModifierKeys (juce::ModifierKeys::shiftModifier)));
    CHECK_FALSE (part().isWarped());

    // Alt-click adds a hitpoint (beat 4: 2 s into the recording).
    const auto before = studio.getPartHitpoints (part().id).size();
    const juce::Point<float> at4 (timeline.xForBeat (4.0), y);
    arrange.mouseDown (mouseAt (arrange, at4, at4, juce::ModifierKeys (juce::ModifierKeys::altModifier)));
    arrange.mouseUp (mouseAt (arrange, at4, at4, juce::ModifierKeys (juce::ModifierKeys::altModifier)));
    CHECK (studio.getPartHitpoints (part().id).size() == before + 1);

    // Alt+N on the audio track: the playhead to the next hitpoint.
    studio.selectTrack (trackId);
    studio.setPosition (1.5);
    CHECK (view.keyPressed (juce::KeyPress ('n', juce::ModifierKeys::altModifier, 'n')));
    CHECK_THAT (studio.getEngine().getPosition(), WithinAbs (2.0, 0.01));

    // It draws in every finish, markers and hitpoints on the selected clip.
    studio.createWarpFromHitpoints ({ part().id });
    arrange.select (trackId, 0);
    for (const auto type : { Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon, Finish::Type::pearlWhite })
    {
        Finish::set (type, false);
        CHECK (view.createComponentSnapshot (view.getLocalBounds()).isValid());
    }
    studio.getAudioFolder().deleteRecursively();
}
