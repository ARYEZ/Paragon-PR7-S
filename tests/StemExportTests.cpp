// Export, round two (0.51.0): channels a file each (stems), cycle markers' ranges a file each, and
// MIDI files (the manual's Export Audio Mixdown Channel Selection and Exporting MIDI Tracks).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/MidiExport.h"
#include "engine/MidiImport.h"
#include "engine/Studio.h"
#include "ui/ExportPages.h"
#include "ui/Finish.h"
#include "ui/Windows.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    float middleOf (const juce::File& file)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr)
            return -1.0f;
        juce::AudioBuffer<float> sound ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&sound, 0, (int) reader->lengthInSamples, 0, true, true);
        return sound.getSample (0, sound.getNumSamples() / 2);
    }

    // a (0.25) into a group at -6 dB; b (0.1) to the master with a send (0 dB, after its fader) to an
    // empty FX channel; the master at -6 dB.
    struct Desk
    {
        Studio studio { false };
        int a = 0, b = 0, group = 0, fx = 0;
        juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-stems");
        Desk()
        {
            folder.deleteRecursively();
            a = studio.getProject().tracks[0].id;
            b = studio.addTrack ({});
            group = studio.addGroupChannel();
            fx = studio.addFxChannel (nullptr);
            studio.renameTrack (a, "Keys");
            studio.renameTrack (b, "Bass");
            studio.getEngine().setInstrument (a, std::make_unique<SteadyTone> (0.25f, 0.25f));
            studio.getEngine().setInstrument (b, std::make_unique<SteadyTone> (0.1f, 0.1f));
            studio.setOutput (a, group);
            studio.setVolume (group, -6.0206f);
            Send send;
            send.target = fx;
            studio.setSend (b, 0, send);
            studio.setMasterVolume (-6.0206f);
        }
        ~Desk() { folder.deleteRecursively(); }
    };
}

TEST_CASE ("0.51.0: channels a file each: each channel's own sound, its group and the FX returns left out", "[export][stems]")
{
    Desk desk;
    auto& studio = desk.studio;
    Studio::ExportBatch batch;
    batch.options.start = 0.0;
    batch.options.end = 4.0;
    batch.options.tailSeconds = 0.0;
    batch.options.bitsPerSample = 32;
    batch.channels = { desk.a, desk.b, desk.group, desk.fx };
    juce::String error;
    std::vector<juce::File> written;
    REQUIRE (studio.exportBatch (desk.folder, "Song", batch, error, &written));
    REQUIRE (written.size() == 4);
    CHECK (written[0].getFileName() == "Song - Keys.wav");
    CHECK (written[1].getFileName() == "Song - Bass.wav");
    // Keys: its own 0.25 (not through its group); Bass: 0.1 (its send's return not in it); the group: Keys
    // through it (0.125); the FX channel: what Bass sends (0.1). The master's fader left out.
    CHECK_THAT (middleOf (written[0]), WithinAbs (0.25, 1.0e-3));
    CHECK_THAT (middleOf (written[1]), WithinAbs (0.1, 1.0e-3));
    CHECK_THAT (middleOf (written[2]), WithinAbs (0.125, 1.0e-3));
    CHECK_THAT (middleOf (written[3]), WithinAbs (0.1, 1.0e-3));

    // With the master: its -6 dB too.
    batch.channels = { desk.b };
    batch.includeMaster = true;
    written.clear();
    REQUIRE (studio.exportBatch (desk.folder, "Song", batch, error, &written));
    REQUIRE (written.size() == 1);
    CHECK (written[0].getFileName() == "Song - Bass 2.wav");   // (the name taken: a number added)
    CHECK_THAT (middleOf (written[0]), WithinAbs (0.05, 1.0e-3));

    // The main mix as ever: (0.125 + 0.1 + 0.1) at -6 dB. The song played normally after.
    batch.channels = {};
    written.clear();
    REQUIRE (studio.exportBatch (desk.folder, "Mix", batch, error, &written));
    CHECK_THAT (middleOf (written[0]), WithinAbs (0.1625, 1.0e-3));
    CHECK (studio.getEngine().getExportBus() == 0);
}

TEST_CASE ("0.51.0: cycle markers a file each, named after them", "[export][stems][markers]")
{
    Desk desk;
    auto& studio = desk.studio;
    studio.addCycleMarker (0.0, 4.0, "Verse");
    studio.addCycleMarker (4.0, 6.0, "Chorus");
    Studio::ExportBatch batch;
    batch.options.tailSeconds = 0.0;
    batch.cycleMarkers = { 0, 1 };
    juce::String error;
    std::vector<juce::File> written;
    REQUIRE (studio.exportBatch (desk.folder, "Song", batch, error, &written));
    REQUIRE (written.size() == 2);
    CHECK (written[0].getFileName() == "Song - Verse.wav");
    CHECK (written[1].getFileName() == "Song - Chorus.wav");
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (written[1]));
    REQUIRE (reader != nullptr);
    CHECK (std::abs ((double) reader->lengthInSamples / reader->sampleRate - 1.0) < 0.01);   // 2 beats at 120 BPM
}

TEST_CASE ("0.51.0: a MIDI file: notes as heard, tempo changes, signatures, markers, the locators, type 0", "[export][midi]")
{
    Project project;
    Track keys;
    keys.id = 1;
    keys.name = "Keys";
    keys.midiChannel = 3;
    Part part;
    part.start = 4.0;
    part.length = 4.0;
    part.transpose = 2;
    part.notes = { { 0.0, 1.0, 60, 0.5f }, { 2.0, 4.0, 64, 0.5f } };   // (the second cut at the part's end)
    keys.parts.push_back (part);
    Part muted = part;
    muted.start = 12.0;
    muted.muted = true;
    keys.parts.push_back (muted);
    project.tracks.push_back (keys);
    project.tempoTrackOn = project.tempoTrackShown = true;
    project.tempoEvents = { { 0.0, 100.0, false }, { 8.0, 140.0, false } };
    project.signatureChanges = { { 2, 3, 4 } };
    project.markerTrack.markers = { { 1, false, 4.0, 4.0, "Verse" } };

    MidiExportOptions options;
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-export.mid");
    juce::String error;
    REQUIRE (writeMidiFile (project, options, file, error));
    const auto read = readMidiFile (file, error);
    REQUIRE (read);
    REQUIRE (read->tracks.size() == 1);
    const auto& t = read->tracks[0];
    CHECK (t.name == "Keys");
    CHECK (t.channel == 3);
    REQUIRE (t.notes.size() == 2);
    CHECK_THAT (t.notes[0].start, WithinAbs (4.0, 1.0e-3));
    CHECK (t.notes[0].pitch == 62);   // (transposed: as heard)
    CHECK_THAT (t.notes[1].length, WithinAbs (2.0, 1.0e-3));   // (cut at the part's end)
    CHECK_THAT (read->bpm, WithinAbs (100.0, 0.01));
    REQUIRE (read->tempoChanges.size() == 2);
    CHECK_THAT (read->tempoChanges[1].bpm, WithinAbs (140.0, 0.01));
    REQUIRE (read->signatureChanges.size() == 1);
    CHECK (read->signatureChanges[0].bar == 2);
    CHECK (read->signatureChanges[0].numerator == 3);
    REQUIRE (read->markers.size() == 1);
    CHECK (read->markers[0].name == "Verse");

    // The locator range only: from the left locator (it becomes 0); type 0: one track; resolution kept.
    project.loopStart = 4.0;
    project.loopEnd = 6.0;
    options.locatorsOnly = true;
    options.type0 = true;
    options.resolution = 96;
    const auto midi = makeMidiFile (project, options);
    CHECK (midi.getNumTracks() == 1);
    CHECK (midi.getTimeFormat() == 96);
    REQUIRE (writeMidiFile (project, options, file, error));
    const auto cut = readMidiFile (file, error);
    REQUIRE (cut);
    REQUIRE (cut->tracks.size() == 1);
    REQUIRE (cut->tracks[0].notes.size() == 1);   // (only the first note starts between the locators)
    CHECK_THAT (cut->tracks[0].notes[0].start, WithinAbs (0.0, 1.0e-3));
    file.deleteFile();
}

TEST_CASE ("0.51.0: the export pages: channels and cycle markers chosen; the MIDI export's options", "[export][ui]")
{
    Desk desk;
    desk.studio.addCycleMarker (0.0, 4.0, "Verse");
    Windows::ExportPage page (desk.studio);
    CHECK_FALSE (page.isBatch());
    page.setChannelsMode (true);
    CHECK (page.isBatch());
    CHECK_FALSE (page.canExport());   // (no channel chosen yet)
    page.setChannelChosen (desk.a, true);
    page.setChannelChosen (desk.fx, true);
    CHECK (page.canExport());
    page.setRange (Windows::ExportPage::Range::cycleMarkers);
    const auto batch = page.getBatch();
    CHECK (batch.channels == std::vector<int> { desk.a, desk.fx });
    CHECK (batch.cycleMarkers == std::vector<int> { 0 });
    for (const auto type : { Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon, Finish::Type::pearlWhite })
    {
        Finish::set (type, false);
        CHECK (page.createComponentSnapshot (page.getLocalBounds()).isValid());
    }

    MidiExportPage midi (desk.studio);
    midi.type0.setToggleState (true, juce::dontSendNotification);
    midi.resolution.setSelectedId (960, juce::dontSendNotification);
    CHECK (midi.getOptions().type0);
    CHECK (midi.getOptions().resolution == 960);
    CHECK (midi.getOptions().markers);
}
