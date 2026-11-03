#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Align.h"
#include "engine/Studio.h"
#include "ui/AudioAlignmentPanel.h"
#include "ui/Finish.h"
#include "ui/MainView.h"
#include "ui/ParagonLookAndFeel.h"

// Audio Alignment (0.33.0): the matching itself (a plain move, a performance whose timing
// wanders, the maximum shift, sync points, protected ranges, precision) and the studio's action
// (split at the overlap, markers written, heard in place, one undo step).

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    constexpr double rate = 48000.0;

    // Notes: each a pitch struck at a time (an instant attack, a little second harmonic, decaying
    // over 0.12 s), silence between: a stand-in for sung syllables, each with its own spectrum.
    std::vector<float> notes (const std::vector<double>& times, double seconds, double startPitch = 220.0)
    {
        std::vector<float> sound ((size_t) (seconds * rate), 0.0f);
        for (size_t n = 0; n < times.size(); ++n)
        {
            const auto f = startPitch * std::pow (2.0, (double) ((n * 5) % 12) / 12.0);
            const auto start = (int) std::llround (times[n] * rate);
            for (int i = 0; start + i < (int) sound.size() && i < (int) (0.35 * rate); ++i)
            {
                if (start + i < 0)
                    continue;
                const auto phase = 2.0 * juce::MathConstants<double>::pi * f * i / rate;
                sound[(size_t) (start + i)] += (float) (0.7 * (std::sin (phase) + 0.3 * std::sin (2.0 * phase)) * std::exp (-i / (0.12 * rate)));
            }
        }
        return sound;
    }

    const std::vector<double> referenceTimes { 0.30, 0.80, 1.25, 1.90, 2.45, 3.05 };

    std::vector<double> shifted (const std::vector<double>& times, const std::vector<double>& by)
    {
        auto out = times;
        for (size_t i = 0; i < out.size(); ++i)
            out[i] += by[i % by.size()];
        return out;
    }

    AlignResult align (const std::vector<double>& targetTimes, AlignSettings settings,
                       std::vector<AlignSyncPoint> syncs = {}, std::vector<juce::Range<double>> held = {})
    {
        AlignSound reference { notes (referenceTimes, 3.8), 0.0 };
        AlignSound target { notes (targetTimes, 3.8), 0.0 };
        return alignSounds (reference, target, 0.1, 3.6, rate, settings, syncs, held);
    }

    void writeWav (const juce::File& file, const std::vector<float>& sound)
    {
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::AudioBuffer<float> buffer (1, (int) sound.size());
        for (int i = 0; i < (int) sound.size(); ++i)
            buffer.setSample (0, i, sound[(size_t) i]);
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (1)
                                                                           .withBitsPerSample (32)
                                                                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }

    juce::MouseEvent mouseAt (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt, juce::ModifierKeys mods = {})
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                                 mods.withFlags (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, now, downAt, now, 1, at != downAt);
    }

    // A guide from 1 s (beat 2) and a looser dub on the next track, starting at 0 and running past it.
    struct Song
    {
        int guideTrack = 0, dubTrack = 0, guideId = 0, dubId = 0;
    };
    Song makeSong (Studio& studio)
    {
        const std::vector<double> by { 0.04, -0.03, 0.06, 0.0, -0.05, 0.02 };
        const auto folder = studio.getAudioFolder();
        writeWav (folder.getChildFile ("guide.wav"), notes (referenceTimes, 3.8));
        std::vector<double> dubTimes { 0.4 };
        for (size_t n = 0; n < referenceTimes.size(); ++n)
            dubTimes.push_back (referenceTimes[n] + by[n] + 1.0);
        writeWav (folder.getChildFile ("dub.wav"), notes (dubTimes, 6.0, 330.0));
        Song song;
        song.guideTrack = studio.addAudioTrack (false);
        song.dubTrack = studio.addAudioTrack (false);
        Part guide;
        guide.start = 2.0;
        guide.length = 7.6;
        guide.audioFile = "guide.wav";
        studio.addPart (song.guideTrack, guide);
        Part dub;
        dub.length = 12.0;
        dub.audioFile = "dub.wav";
        studio.addPart (song.dubTrack, dub);
        song.guideId = studio.getProject().findTrack (song.guideTrack)->parts[0].id;
        song.dubId = studio.getProject().findTrack (song.dubTrack)->parts[0].id;
        return song;
    }

    // Where the first sound at or after `from` seconds starts in a buffer (over a tenth of full scale).
    double firstSoundAfter (const juce::AudioBuffer<float>& sound, double from)
    {
        for (int i = juce::jmax (0, (int) (from * rate)); i < sound.getNumSamples(); ++i)
            if (std::abs (sound.getSample (0, i)) > 0.1f)
                return i / rate;
        return -1.0;
    }
}

TEST_CASE ("Audio Alignment: a target late by a steady amount is moved (Prefer Time Shifting) to the sample, else warped to match", "[align]")
{
    const auto late = shifted (referenceTimes, { 0.037 });
    AlignSettings settings;
    settings.preferShifting = true;
    auto moved = align (late, settings);
    REQUIRE (moved.ok);
    CHECK (moved.shiftOnly);
    CHECK_THAT (moved.shift, WithinAbs (0.037, 0.0005));   // (the waveforms themselves: to the sample)
    CHECK_THAT (moved.before, WithinAbs (0.037, 0.006));
    CHECK (moved.after <= 0.005);   // (measured on the 10 ms grid: a true 37 ms reads as 30 or 40)

    settings.preferShifting = false;
    settings.precision = 1.0;
    const auto warped = align (late, settings);
    REQUIRE (warped.ok);
    CHECK_FALSE (warped.shiftOnly);
    for (const auto t : referenceTimes)
        CHECK_THAT (alignedTargetAt (warped, t), WithinAbs (t + 0.037, 0.004));

    // Settings kept as text (the settings file).
    settings.flexibility = AlignSettings::Flexibility::high;
    settings.maxShiftSeconds = 0.25;
    settings.matchWords = true;
    CHECK (AlignSettings::fromString (settings.toString()) == settings);
}

TEST_CASE ("Audio Alignment: a performance whose timing wanders lands on the reference's notes; the maximum shift limits it", "[align]")
{
    const std::vector<double> by { 0.04, -0.03, 0.06, 0.0, -0.05, 0.02 };
    const auto loose = shifted (referenceTimes, by);
    AlignSettings settings;
    settings.precision = 1.0;
    const auto result = align (loose, settings);
    REQUIRE (result.ok);
    for (size_t n = 0; n < referenceTimes.size(); ++n)
        CHECK_THAT (alignedTargetAt (result, referenceTimes[n]), WithinAbs (loose[n], 0.005));
    CHECK (result.before > 0.025);
    CHECK (result.after < 0.008);
    // The points keep their order, both ways.
    for (size_t p = 1; p < result.points.size(); ++p)
    {
        CHECK (result.points[p].first > result.points[p - 1].first);
        CHECK (result.points[p].second > result.points[p - 1].second);
    }

    // Lower precision: fewer points (a looser fit, a more natural sound).
    settings.precision = 0.0;
    const auto loose0 = align (loose, settings);
    REQUIRE (loose0.ok);
    CHECK (loose0.points.size() < result.points.size());

    // Too far for the maximum shift: no moment moves more than it allows.
    settings.precision = 1.0;
    settings.maxShiftSeconds = 0.03;
    const auto limited = align (loose, settings);
    REQUIRE (limited.ok);
    for (const auto& [t, g] : limited.points)
        CHECK (std::abs (g - t) <= 0.03 + 0.012 + 1.0e-9);   // (the band, and the to-the-sample placing)
}

TEST_CASE ("Audio Alignment: sync points are met exactly, or reported when they can't be; protected ranges keep their pace", "[align]")
{
    const std::vector<double> by { 0.04, -0.03, 0.06, 0.0, -0.05, 0.02 };
    const auto loose = shifted (referenceTimes, by);
    AlignSettings settings;
    settings.precision = 1.0;
    // The third note: the user says it lands 20 ms after the reference's (not where it matches).
    const AlignSyncPoint wanted { referenceTimes[2] + 0.02, loose[2] };
    const AlignSyncPoint outOfReach { referenceTimes[4], referenceTimes[4] + 0.4 };   // (past the maximum shift)
    const auto result = align (loose, settings, { wanted, outOfReach });
    REQUIRE (result.ok);
    CHECK_THAT (alignedTargetAt (result, wanted.reference), WithinAbs (wanted.target, 1.0e-6));
    REQUIRE (result.unmetSyncPoints.size() == 1);
    CHECK (result.unmetSyncPoints[0] == 1);

    // Protected: the second and third notes' stretch at one offset (moved, not stretched).
    const juce::Range<double> held { 0.7, 1.5 };
    const auto kept = align (loose, settings, {}, { held });
    REQUIRE (kept.ok);
    const auto offsetAt = [&] (double t) { return alignedTargetAt (kept, t) - t; };
    CHECK_THAT (offsetAt (0.75), WithinAbs (offsetAt (1.45), 1.0e-6));
    CHECK_THAT (offsetAt (1.0), WithinAbs (offsetAt (1.3), 1.0e-6));
    // Outside it the notes still land.
    CHECK_THAT (alignedTargetAt (kept, referenceTimes[4]), WithinAbs (loose[4], 0.006));
}

TEST_CASE ("Audio Alignment in the studio: the target split at the overlap, warped to the reference, heard on its notes; one undo step", "[align]")
{
    Studio studio (false);
    const auto folder = studio.getAudioFolder();
    const std::vector<double> by { 0.04, -0.03, 0.06, 0.0, -0.05, 0.02 };
    // The reference from 1 s of the song (bar 1, beat 3 at 120 BPM: beat 2); the target starts a
    // second earlier and runs a second past it.
    writeWav (folder.getChildFile ("guide.wav"), notes (referenceTimes, 3.8));
    std::vector<double> dubTimes;
    for (size_t n = 0; n < referenceTimes.size(); ++n)
        dubTimes.push_back (referenceTimes[n] + by[n] + 1.0);
    dubTimes.insert (dubTimes.begin(), 0.4);   // (a note before the reference starts: left alone)
    writeWav (folder.getChildFile ("dub.wav"), notes (dubTimes, 6.0, 330.0));
    const auto guideTrack = studio.addAudioTrack (false);
    const auto dubTrack = studio.addAudioTrack (false);
    REQUIRE (studio.getProject().bpm == 120.0);
    Part guide;
    guide.start = 2.0;           // 1 s
    guide.length = 7.6;          // 3.8 s
    guide.audioFile = "guide.wav";
    studio.addPart (guideTrack, guide);
    Part dub;
    dub.start = 0.0;
    dub.length = 12.0;           // 6 s
    dub.audioFile = "dub.wav";
    studio.addPart (dubTrack, dub);
    const auto dubId = studio.getProject().findTrack (dubTrack)->parts[0].id;

    Studio::AlignReport report;
    auto settings = studio.getAlignSettings();
    settings.precision = 1.0;
    const auto statesBefore = studio.getProject().findTrack (dubTrack)->parts;
    REQUIRE (studio.alignAudio ({ guideTrack, 2.0, 9.6 }, { { dubId } }, settings, {}, {}, report));
    CHECK (report.aligned == 1);
    CHECK (report.stretched == 1);
    CHECK (report.after < report.before);

    // Three pieces: before, the overlap (warped), after; crossfades at the cuts.
    const auto& parts = studio.getProject().findTrack (dubTrack)->parts;
    REQUIRE (parts.size() == 3);
    REQUIRE (report.alignedParts.size() == 1);
    const Part* piece = nullptr;
    for (const auto& p : parts)
        if (p.id == report.alignedParts[0])
            piece = &p;
    REQUIRE (piece != nullptr);
    CHECK_THAT (piece->start, WithinAbs (2.0, 1.0e-9));
    CHECK_THAT (piece->length, WithinAbs (7.6, 1.0e-9));
    CHECK (piece->isWarped());
    CHECK_THAT (piece->fadeIn, WithinAbs (0.01, 1.0e-9));
    CHECK_THAT (piece->fadeOut, WithinAbs (0.01, 1.0e-9));

    // Heard: each note where the reference has it (the warped sound from the piece's start).
    const auto sound = studio.getWarpedSound (*piece, studio.getProject().tempoMap());
    REQUIRE (sound != nullptr);
    for (const auto t : referenceTimes)
        CHECK_THAT (firstSoundAfter (*sound, t - 0.03), WithinAbs (t, 0.006));
    // No click where a stretch starts: never louder than the recording itself.
    CHECK (sound->getMagnitude (0, 0, sound->getNumSamples()) <= 0.7f * 1.22f * 1.03f);   // (a note's own peak: 0.7 x 1.22)
    auto stray = 0.0f;   // (and nothing between the notes: each note's sound is over by 0.35 s)
    for (size_t n = 0; n + 1 < referenceTimes.size(); ++n)
        if (referenceTimes[n + 1] - referenceTimes[n] > 0.45)
            stray = juce::jmax (stray, sound->getMagnitude (0, (int) ((referenceTimes[n] + 0.40) * rate), (int) (0.03 * rate)));
    CHECK (stray < 0.01f);

    // One undo step puts the clip back as it was.
    studio.undo();
    CHECK (studio.getProject().findTrack (dubTrack)->parts == statesBefore);

    // Not on the reference's own track, not without an overlap.
    CHECK_FALSE (studio.alignAudio ({ dubTrack, 0.0, 12.0 }, { { dubId } }, settings, {}, {}, report));
    CHECK (report.notes.size() == 1);
}

TEST_CASE ("Audio Alignment in the studio: Prefer Time Shifting moves a steady target's audio, no markers", "[align]")
{
    Studio studio (false);
    const auto folder = studio.getAudioFolder();
    writeWav (folder.getChildFile ("mic1.wav"), notes (referenceTimes, 3.8));
    writeWav (folder.getChildFile ("mic2.wav"), notes (shifted (referenceTimes, { 0.012 }), 3.8));   // (a far microphone)
    const auto a = studio.addAudioTrack (false), b = studio.addAudioTrack (false);
    Part one;
    one.length = 7.6;
    one.audioFile = "mic1.wav";
    studio.addPart (a, one);
    auto two = one;
    two.audioFile = "mic2.wav";
    studio.addPart (b, two);
    const auto id = studio.getProject().findTrack (b)->parts[0].id;
    auto settings = studio.getAlignSettings();
    settings.preferShifting = true;
    Studio::AlignReport report;
    REQUIRE (studio.alignAudio ({ a, 0.0, 7.6 }, { { id } }, settings, {}, {}, report));
    CHECK (report.moved == 1);
    const auto& part = studio.getProject().findTrack (b)->parts[0];
    CHECK_FALSE (part.isWarped());
    CHECK_THAT (part.audioOffset, WithinAbs (0.012, 0.0005));
    CHECK (studio.getProject().findTrack (b)->parts.size() == 1);   // (the whole clip overlapped: no splits)
}

TEST_CASE ("The Audio Alignment panel: reference and targets from the selection, sync points and protected ranges by mouse, ALIGN AUDIO", "[align][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    const auto song = makeSong (studio);
    std::vector<int> selected { song.guideId };
    AudioAlignmentPanel panel (studio, [&] { return selected; }, [] { return std::optional<Studio::Range> {}; });
    CHECK_FALSE (panel.keyNamed ("ALIGN AUDIO")->isEnabled());

    REQUIRE (panel.setReferenceFromSelection());
    REQUIRE (panel.getReference().has_value());
    CHECK (panel.getReference()->trackId == song.guideTrack);
    CHECK_THAT (panel.getReference()->end, WithinAbs (9.6, 1.0e-9));
    selected = { song.guideId, song.dubId };   // (the reference's own track is never a target)
    CHECK (panel.addTargetsFromSelection() == 1);
    CHECK (panel.getTargets().at (0).partId == song.dubId);
    CHECK (panel.keyNamed ("ALIGN AUDIO")->isEnabled());

    // A drag from the reference down to the target: a sync point there.
    auto& view = panel.getView();
    const auto refY = view.getHeight() * 0.25f, tgtY = view.getHeight() * 0.75f;
    const juce::Point<float> from (panel.xForBeat (4.5), refY), to (panel.xForBeat (4.6), tgtY);
    view.mouseDown (mouseAt (view, from, from));
    view.mouseDrag (mouseAt (view, to, from));
    view.mouseUp (mouseAt (view, to, from));
    REQUIRE (panel.getSyncPoints().size() == 1);
    CHECK_THAT (panel.getSyncPoints()[0].referenceBeat, WithinAbs (4.5, 0.02));
    CHECK_THAT (panel.getSyncPoints()[0].targetBeat, WithinAbs (4.6, 0.02));
    // Shift-click on it: gone.
    const juce::Point<float> middle ((from.x + to.x) * 0.5f, (refY + tgtY) * 0.5f);
    view.mouseDown (mouseAt (view, middle, middle, juce::ModifierKeys (juce::ModifierKeys::shiftModifier)));
    CHECK (panel.getSyncPoints().empty());
    // Ctrl-drag on the target: a protected range; Shift-click inside: gone.
    const juce::Point<float> a (panel.xForBeat (6.0), tgtY), b (panel.xForBeat (7.0), tgtY);
    view.mouseDown (mouseAt (view, a, a, juce::ModifierKeys (juce::ModifierKeys::commandModifier)));
    view.mouseDrag (mouseAt (view, b, a, juce::ModifierKeys (juce::ModifierKeys::commandModifier)));
    view.mouseUp (mouseAt (view, b, a, juce::ModifierKeys (juce::ModifierKeys::commandModifier)));
    REQUIRE (panel.getProtectedRanges().size() == 1);
    CHECK_THAT (panel.getProtectedRanges()[0].getStart(), WithinAbs (6.0, 0.02));
    const juce::Point<float> inside (panel.xForBeat (6.5), tgtY);
    view.mouseDown (mouseAt (view, inside, inside, juce::ModifierKeys (juce::ModifierKeys::shiftModifier)));
    CHECK (panel.getProtectedRanges().empty());

    // Settings by the keys (kept in the studio).
    panel.keyNamed ("MATCH WORDS")->onClick();
    CHECK (studio.getAlignSettings().matchWords);
    panel.keyNamed ("HIGH")->onClick();
    CHECK (studio.getAlignSettings().flexibility == AlignSettings::Flexibility::high);

    // ALIGN AUDIO: the target is now the aligned piece; the report says so.
    panel.keyNamed ("ALIGN AUDIO")->onClick();
    CHECK (panel.getReport().aligned == 1);
    CHECK (panel.getReportText().startsWith ("ALIGNED 1 EVENT"));
    const auto found = studio.findPart (panel.getTargets().at (0).partId);
    REQUIRE (found.has_value());
    CHECK (studio.getProject().findTrack (found->first)->parts[(size_t) found->second].isWarped());

    // Undone: the aligned piece gone, so is the target (nothing left pointing nowhere).
    studio.undo();
    studio.dispatchPendingMessages();
    CHECK (panel.getTargets().empty());
}

TEST_CASE ("The Audio Alignment panel opens from the studio's window (the menus' hook and the command)", "[align][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    CHECK (view.getAudioAlignmentWindow() == nullptr);
    view.getArrangeView().onOpenAudioAlignment();
    REQUIRE (view.getAudioAlignmentWindow() != nullptr);
    view.getArrangeView().onOpenAudioAlignment();   // (again: brought to the front, not closed)
    CHECK (view.getAudioAlignmentWindow() != nullptr);
    view.toggleAudioAlignment();
    CHECK (view.getAudioAlignmentWindow() == nullptr);
}

TEST_CASE ("Snapshot: the Audio Alignment panel", "[.][snapshot-align]")
{
    // PARAGON_SNAPSHOT_TAB=before: before ALIGN AUDIO.
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "align.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        Studio studio (false);
        const auto song = makeSong (studio);
        std::vector<int> selected { song.guideId };
        AudioAlignmentPanel panel (studio, [&] { return selected; }, [] { return std::optional<Studio::Range> {}; });
        panel.setReferenceFromSelection();
        selected = { song.dubId };
        panel.addTargetsFromSelection();
        panel.addSyncPoint (2.0 + referenceTimes[2] * 2.0, 2.0 + (referenceTimes[2] + 0.06) * 2.0);
        panel.addSyncPoint (2.0 + referenceTimes[4] * 2.0, 2.0 + (referenceTimes[4] + 0.6) * 2.0);   // (too far: red)
        panel.addProtectedRange ({ 2.0 + 1.75 * 2.0, 2.0 + 2.3 * 2.0 });
        if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_TAB", {}) != "before")
            panel.alignNow();
        const auto image = panel.createComponentSnapshot (panel.getLocalBounds(), true, 1.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}
