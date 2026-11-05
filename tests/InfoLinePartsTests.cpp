// The info line for parts and events (0.47.0): the fields, the manual's value editing rules
// (relative, Ctrl absolute), and what the new values do to playback (an event's volume, a MIDI
// part's Transpose and Velocity).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/ArrangeView.h"
#include "ui/Finish.h"
#include "ui/InfoLine.h"
#include "ui/MainView.h"

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double rate = 48000.0;

    // A WAV of one steady level (both sides) in the studio's audio folder.
    void writeSteady (Studio& studio, const juce::String& name, float level, double seconds)
    {
        auto folder = studio.getAudioFolder();
        folder.createDirectory();
        juce::AudioBuffer<float> sound (2, (int) (rate * seconds));
        for (int c = 0; c < 2; ++c)
            juce::FloatVectorOperations::fill (sound.getWritePointer (c), level, sound.getNumSamples());
        std::unique_ptr<juce::OutputStream> stream (folder.getChildFile (name).createOutputStream());
        auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2)
                                                                           .withBitsPerSample (32)
                                                                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (sound, 0, sound.getNumSamples());
    }

    Part clip (double start, double length, const juce::String& file)
    {
        Part p;
        p.start = start;
        p.length = length;
        p.audioFile = file;
        return p;
    }

    const Part& partOf (const Studio& studio, int id)
    {
        const auto where = studio.findPart (id);
        REQUIRE (where);
        return studio.getProject().findTrack (where->first)->parts[(size_t) where->second];
    }

    float playedLevel (Studio& studio, double fromBeat)
    {
        studio.setPosition (fromBeat);
        studio.play();
        const auto heard = renderSamples (studio.getEngine(), 4096);
        studio.stop();
        return heard[2048];
    }
}

TEST_CASE ("0.47.0: an audio event's volume, a MIDI part's Transpose and Velocity play (the notes stay as written)", "[infoline][engine]")
{
    SECTION ("Volume")
    {
        Studio studio (false);
        const auto trackId = studio.addAudioTrack (false);
        writeSteady (studio, "steady.wav", 0.5f, 10.0);
        studio.addPart (trackId, clip (0.0, 8.0, "steady.wav"));
        const auto id = studio.getProject().findTrack (trackId)->parts[0].id;
        const auto full = playedLevel (studio, 1.0);
        REQUIRE (std::abs (full) > 0.01f);
        REQUIRE (studio.setPartsField ({ id }, Studio::PartField::volume, -6.0206, true));
        CHECK_THAT (playedLevel (studio, 1.0) / full, WithinAbs (0.5, 1.0e-4));
        // Over a fade too (the edges' one-at-a-time samples carry the same gain): the event's start.
        REQUIRE (studio.setPartsField ({ id }, Studio::PartField::fadeIn, 0.5, true));
        studio.setPartsField ({ id }, Studio::PartField::volume, 0.0, true);
        const auto fadedFull = playedLevel (studio, 0.0);
        studio.setPartsField ({ id }, Studio::PartField::volume, -6.0206, true);
        CHECK_THAT (playedLevel (studio, 0.0) / fadedFull, WithinAbs (0.5, 1.0e-4));
        // -INF: silent; saved and opened again.
        studio.setPartsField ({ id }, Studio::PartField::volume, -100.0, true);
        CHECK_THAT (partOf (studio, id).volumeDb, WithinAbs (Part::leastVolumeDb, 1.0e-9));
        CHECK (playedLevel (studio, 1.0) == 0.0f);
        const auto reopened = Project::fromXml (*studio.getProject().toXml());
        REQUIRE (reopened);
        REQUIRE (reopened->findTrack (trackId) != nullptr);
        CHECK_THAT (reopened->findTrack (trackId)->parts[0].volumeDb, WithinAbs (Part::leastVolumeDb, 1.0e-9));
        studio.getAudioFolder().deleteRecursively();
    }

    SECTION ("Transpose and Velocity")
    {
        Studio studio (false);
        const auto trackId = studio.getProject().tracks[0].id;
        auto probe = std::make_unique<Probe>();
        auto* heard = probe.get();
        studio.getEngine().setInstrument (trackId, std::move (probe));
        Part part;
        part.start = 0.0;
        part.length = 4.0;
        part.notes = { { 0.5, 0.5, 60, 0.8f } };
        studio.addPart (trackId, part);
        const auto id = studio.getProject().findTrack (trackId)->parts[0].id;
        REQUIRE (studio.setPartsField ({ id }, Studio::PartField::transpose, 3.0, true));
        REQUIRE (studio.setPartsField ({ id }, Studio::PartField::velocity, 10.0, true));
        studio.setPosition (0.0);
        studio.play();
        renderSamples (studio.getEngine(), 48000);
        studio.stop();
        const auto on = heard->notesOn();
        REQUIRE (on.size() == 1);
        CHECK (on[0].message.getNoteNumber() == 63);
        CHECK (on[0].message.getVelocity() == juce::roundToInt (0.8f * 127.0f) + 10);
        CHECK (partOf (studio, id).notes[0].pitch == 60);   // (written as before)
        // Velocities stay 1-127; transpose -48..48.
        studio.setPartsField ({ id }, Studio::PartField::velocity, 500.0, true);
        CHECK (partOf (studio, id).velocityOffset == 127);
        CHECK (partOf (studio, id).heardVelocity (0.8f) == 1.0f);
        studio.setPartsField ({ id }, Studio::PartField::transpose, -100.0, true);
        CHECK (partOf (studio, id).transpose == -48);
        // Glued: the glued part's notes take each part's settings (it has none of its own).
        studio.setPartsField ({ id }, Studio::PartField::transpose, 2.0, true);
        studio.setPartsField ({ id }, Studio::PartField::velocity, 0.0, true);
        Part next;
        next.start = 4.0;
        next.length = 4.0;
        next.notes = { { 0.0, 1.0, 50, 0.5f } };
        studio.addPart (trackId, next);
        REQUIRE (studio.glueParts (trackId, 0, false));
        const auto& glued = studio.getProject().findTrack (trackId)->parts[0];
        CHECK (glued.transpose == 0);
        REQUIRE (glued.notes.size() == 2);
        CHECK (glued.notes[0].pitch == 62);
        CHECK (glued.notes[1].pitch == 50);
        // MIDI parts have no volume, audio events no transpose (none: not this kind's).
        CHECK_FALSE (studio.getPartField (glued.id, Studio::PartField::volume));
        CHECK_FALSE (studio.setPartsField ({ glued.id }, Studio::PartField::fadeIn, 1.0, true));
    }
}

TEST_CASE ("0.47.0: the info line shows the selected parts and changes them as the manual's rules (the manual's example)", "[infoline][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1800, 900);
    auto& arrange = view.getArrangeView();
    auto& info = view.getInfoLine();
    const auto trackId = studio.addAudioTrack (false);
    writeSteady (studio, "a.wav", 0.25f, 30.0);
    // The manual's example: two audio events, 1 bar and 2 bars long.
    studio.addPart (trackId, clip (0.0, 4.0, "a.wav"));
    studio.addPart (trackId, clip (8.0, 8.0, "a.wav"));
    const auto& parts = [&] () -> const std::vector<Part>& { return studio.getProject().findTrack (trackId)->parts; };
    const auto first = parts()[0].id, second = parts()[1].id;
    studio.dispatchPendingMessages();

    info.refresh();
    CHECK (info.getField ("START").isEmpty());
    CHECK_FALSE (info.isShowingParts());

    // Selected (the later one clicked first: the earliest is still the one shown).
    arrange.selectParts ({ second, first });
    info.refresh();
    REQUIRE (info.isShowingParts());
    CHECK (info.partsInOrder() == std::vector<int> { first, second });
    CHECK (info.getField ("FILE") == "a.wav");
    CHECK (info.getField ("START") == formatBarsBeats (0.0, studio.getProject()).replace ("  ", " "));
    CHECK (info.getField ("LENGTH") == "1.0.0.0");
    CHECK (info.getField ("VOLUME") == "0.0 dB");
    CHECK (info.getField ("MUTE") == "NO");
    CHECK (info.getField ("TRANSPOSE").isEmpty());   // (MIDI parts')

    // LENGTH typed as 3 bars: 3 and 4 bars (relative); Ctrl: both 3.
    REQUIRE (info.enter ("LENGTH", "3.0.0.0", false));
    CHECK_THAT (parts()[0].length, WithinAbs (12.0, 1.0e-9));
    CHECK_THAT (parts()[1].length, WithinAbs (16.0, 1.0e-9));
    REQUIRE (info.enter ("LENGTH", "3.0.0.0", true));
    CHECK_THAT (parts()[0].length, WithinAbs (12.0, 1.0e-9));
    CHECK_THAT (parts()[1].length, WithinAbs (12.0, 1.0e-9));
    // One undo step each.
    studio.undo();
    CHECK_THAT (parts()[1].length, WithinAbs (16.0, 1.0e-9));
    studio.redo();

    // START: both move by what the first moves (bar 1 -> bar 2: 4 beats).
    REQUIRE (info.enter ("START", "2.1.1.0", false));
    CHECK_THAT (parts()[0].start, WithinAbs (4.0, 1.0e-9));
    CHECK_THAT (parts()[1].start, WithinAbs (12.0, 1.0e-9));
    // END: lengths change (the first's end 4 beats later: both 4 beats longer).
    REQUIRE (info.enter ("END", formatBarsBeats (20.0, studio.getProject()), false));
    CHECK_THAT (parts()[0].length, WithinAbs (16.0, 1.0e-9));
    CHECK_THAT (parts()[1].length, WithinAbs (16.0, 1.0e-9));
    // Not past the recording (30 s = 60 beats at 120 BPM).
    REQUIRE (info.enter ("LENGTH", "40.0.0.0", true));
    CHECK_THAT (parts()[0].length, WithinAbs (60.0, 1.0e-6));

    // OFFSET, fades, VOLUME: typed in their own units; relative from the first.
    REQUIRE (info.enter ("LENGTH", "2.0.0.0", true));
    REQUIRE (info.enter ("FADE-IN", "250 ms", true));
    CHECK_THAT (parts()[1].fadeIn, WithinAbs (0.25, 1.0e-9));
    REQUIRE (info.enter ("FADE-OUT", "0.5", false));
    CHECK_THAT (parts()[0].fadeOut, WithinAbs (0.5, 1.0e-9));
    REQUIRE (info.enter ("OFFSET", "1.5 s", false));
    CHECK_THAT (parts()[0].audioOffset, WithinAbs (1.5, 1.0e-9));
    studio.setPartsField ({ second }, Studio::PartField::volume, -3.0, true);
    REQUIRE (info.enter ("VOLUME", "-6 dB", false));   // first 0 -> -6: the second -3 -> -9
    CHECK_THAT (parts()[0].volumeDb, WithinAbs (-6.0, 1.0e-9));
    CHECK_THAT (parts()[1].volumeDb, WithinAbs (-9.0, 1.0e-9));
    REQUIRE (info.enter ("VOLUME", "-inf", true));
    CHECK (info.getField ("VOLUME") == "-INF");
    CHECK_FALSE (info.enter ("VOLUME", "loud", false));

    // NAME: both; MUTE's click: both.
    REQUIRE (info.enter ("NAME", "Verse gtr", false));
    CHECK (parts()[0].name == "Verse gtr");
    CHECK (parts()[1].name == "Verse gtr");
    info.toggleMute();
    CHECK (parts()[0].muted);
    CHECK (parts()[1].muted);

    // A MIDI part: TRANSPOSE and VELOCITY, no audio fields; typed with Ctrl+Enter in the box.
    const auto midiTrack = studio.getProject().tracks[0].id;
    Part part;
    part.length = 4.0;
    part.notes = { { 0.0, 1.0, 60, 0.8f } };
    studio.addPart (midiTrack, part);
    const auto midiId = studio.getProject().findTrack (midiTrack)->parts.back().id;
    arrange.selectParts ({ midiId });
    info.refresh();
    CHECK (info.getField ("FILE").isEmpty());
    CHECK (info.getField ("TRANSPOSE") == "0");
    CHECK (info.getField ("OFFSET").isEmpty());
    info.beginEdit ("TRANSPOSE");
    REQUIRE (info.getEditor() != nullptr);
    info.getEditor()->setText ("+5", false);
    CHECK (info.getEditor()->keyPressed (juce::KeyPress (juce::KeyPress::returnKey, juce::ModifierKeys::commandModifier, 0)));
    CHECK (studio.getProject().findTrack (midiTrack)->parts.back().transpose == 5);
    studio.dispatchPendingMessages();
    CHECK (info.getField ("TRANSPOSE") == "+5");
    REQUIRE (info.enter ("VELOCITY", "-20", false));
    CHECK (studio.getProject().findTrack (midiTrack)->parts.back().velocityOffset == -20);

    // Points selected win over parts (the last thing chosen).
    studio.setAutomationOpen (midiTrack, true);
    studio.setAutomationPoints (midiTrack, 0, { { 4.0, 0.5f, 0.0f } });
    arrange.selectPoints (midiTrack, 0, { 0 });
    info.refresh();
    CHECK_FALSE (info.isShowingParts());
    CHECK (info.getField ("VALUE").isNotEmpty());

    // Drawn in every finish.
    arrange.selectParts ({ first, second });
    arrange.selectPoints (midiTrack, 0, {});
    info.refresh();
    for (const auto type : { Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon, Finish::Type::pearlWhite })
    {
        Finish::set (type, false);
        CHECK (view.createComponentSnapshot (view.getLocalBounds()).isValid());
    }
    studio.getAudioFolder().deleteRecursively();
}
