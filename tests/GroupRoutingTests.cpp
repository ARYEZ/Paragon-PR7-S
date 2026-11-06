#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "engine/DragOut.h"
#include "engine/MidiImport.h"
#include "ui/ComposerPage.h"

// (0.59.0) Groups into groups (the owner's: "John verse vox" and "John chorus vox" into "John vox", into
// the master), and a composer lane dragged out as a MIDI file.

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    struct Desk
    {
        Engine engine;
        Project project;
        Desk() { engine.prepare (48000.0, 512); }
        Track& addTrack (int id, float level)
        {
            Track track;
            track.id = id;
            project.tracks.push_back (track);
            engine.setInstrument (id, std::make_unique<SteadyTone> (level, level));
            engine.update (project);
            return project.tracks.back();
        }
        Track& addGroup (int id)
        {
            Track track;
            track.id = id;
            track.kind = Track::Kind::group;
            project.tracks.push_back (track);
            engine.addChannel (id, true);
            engine.update (project);
            return project.tracks.back();
        }
        Track& track (int id) { return *project.findTrack (id); }
        float output()
        {
            engine.update (project);
            return renderSamples (engine, 4096).back();
        }
    };

    void groupsIntoGroups (Desk& desk)
    {
        // "John vox" first in the list: the order the groups are mixed in mustn't depend on the list's.
        desk.addGroup (12);
        desk.addTrack (1, 0.25f);   // verse vocal 1
        desk.addTrack (2, 0.125f);  // verse vocal 2
        desk.addTrack (3, 0.5f);    // chorus vocal
        desk.addGroup (10);         // John verse vox
        desk.addGroup (11);         // John chorus vox
        desk.track (1).output = 10;
        desk.track (2).output = 10;
        desk.track (3).output = 11;
        desk.track (10).output = 12;
        desk.track (11).output = 12;
        CHECK_THAT (desk.output(), WithinAbs (0.875, 1.0e-5));   // every group at 0 dB: the same

        // John vox's effect and fader: on everything his groups bring in.
        desk.engine.insertEffect (12, 0, timesEffect (0.5f));
        desk.track (12).inserts.resize (1);
        CHECK_THAT (desk.output(), WithinAbs (0.4375, 1.0e-5));
        desk.track (12).volumeDb = juce::Decibels::gainToDecibels (0.5f);
        CHECK_THAT (desk.output(), WithinAbs (0.21875, 1.0e-4));
        desk.track (12).volumeDb = 0.0f;

        // A verse group's own fader, before John vox's half.
        desk.track (10).volumeDb = -60.0f;
        CHECK_THAT (desk.output(), WithinAbs (0.25, 1.0e-4));   // the chorus alone, halved
        desk.track (10).volumeDb = 0.0f;

        // John vox muted: everything through him silent.
        desk.track (12).mute = true;
        CHECK_THAT (desk.output(), WithinAbs (0.0, 1.0e-6));
        desk.track (12).mute = false;

        // Three deep: John vox into "All vox".
        desk.addGroup (13);
        desk.track (12).output = 13;
        desk.track (13).volumeDb = juce::Decibels::gainToDecibels (0.5f);
        CHECK_THAT (desk.output(), WithinAbs (0.21875, 1.0e-4));
    }
}

TEST_CASE ("Groups into groups: verse and chorus groups into one, into the master (0.59.0)", "[engine][sends][groups]")
{
    Desk desk;
    groupsIntoGroups (desk);
}

TEST_CASE ("Groups into groups on several cores: the same (0.59.0)", "[engine][sends][groups]")
{
    Desk desk;
    desk.engine.setHelperThreads (3);
    groupsIntoGroups (desk);
}

TEST_CASE ("The studio's groups into groups: allowed, never a loop, kept in the song, reset when the group goes (0.59.0)", "[studio][sends][groups]")
{
    Studio studio (false);
    const auto vocal = studio.getProject().tracks.front().id;
    const auto verse = studio.addGroupChannel();
    const auto chorus = studio.addGroupChannel();
    const auto john = studio.addGroupChannel();
    const auto fx = studio.addFxChannel ({});
    studio.setOutput (vocal, verse);
    studio.setOutput (verse, john);
    studio.setOutput (chorus, john);
    CHECK (studio.getProject().findTrack (verse)->output == john);
    CHECK (studio.getProject().findTrack (chorus)->output == john);

    // Loops are refused: John vox into his own verse group, or a group into itself.
    studio.setOutput (john, verse);
    CHECK (studio.getProject().findTrack (john)->output == 0);
    studio.setOutput (john, john);
    CHECK (studio.getProject().findTrack (john)->output == 0);
    CHECK_FALSE (studio.getProject().canRouteTo (john, verse));
    CHECK (studio.getProject().canRouteTo (verse, chorus));   // (sideways is fine)
    // An FX channel still goes to the master.
    if (fx != 0)
    {
        studio.setOutput (fx, john);
        CHECK (studio.getProject().findTrack (fx)->output == 0);
    }

    // Saved and opened again.
    const auto file = juce::File::createTempFile (".pr7s");
    REQUIRE (studio.save (file));
    Studio other (false);
    juce::String problems;
    REQUIRE (other.open (file, problems));
    CHECK (other.getProject().findTrack (verse)->output == john);
    CHECK (other.getProject().findTrack (chorus)->output == john);
    file.deleteFile();

    // A song with a loop in it (written by hand): opened with the loop broken, back to the master.
    auto xml = studio.getProject().toXml();
    for (auto* t : xml->getChildIterator())
        if (t->getIntAttribute ("id") == john)
            t->setAttribute ("output", verse);
    const auto looped = Project::fromXml (*xml);
    REQUIRE (looped.has_value());
    const auto* v = looped->findTrack (verse);
    const auto* j = looped->findTrack (john);
    REQUIRE ((v != nullptr && j != nullptr));
    CHECK ((v->output == 0 || j->output == 0));

    // John vox removed: what went to him goes to the master.
    studio.removeTrack (john);
    CHECK (studio.getProject().findTrack (verse)->output == 0);
    CHECK (studio.getProject().findTrack (chorus)->output == 0);
    CHECK (studio.getProject().findTrack (vocal)->output == verse);
}

TEST_CASE ("A composer lane dragged out as a MIDI file: its notes, the drums on channel 10, droppable back on the studio (0.59.0)", "[composer][dragout]")
{
    Studio studio (false);
    ComposerPage page (studio);
    page.setRandomSeed (3);
    juce::StringArray dragged;
    page.onExternalDrag = [&dragged] (const juce::StringArray& files) { dragged = files; };

    const auto file = page.dragLaneOut (ComposerLayer::bass);
    REQUIRE (file.existsAsFile());
    CHECK (dragged == juce::StringArray { file.getFullPathName() });
    CHECK (file.getFileName().startsWith ("Composer bass - "));
    CHECK (file.isAChildOf (composerDragFolder()));
    CHECK_FALSE (file.isAChildOf (dragOutFolder()));   // (so the arrangement takes it when dropped back)
    juce::String error;
    const auto bass = readMidiFile (file, error);
    REQUIRE (bass.has_value());
    REQUIRE (bass->tracks.size() == 1);
    CHECK (bass->tracks.front().notes.size() == page.getIdea (ComposerLayer::bass).notes.size());
    CHECK (bass->tracks.front().notes.front().pitch == page.getIdea (ComposerLayer::bass).notes.front().pitch);

    // The drums on General MIDI's channel 10.
    const auto drums = page.dragLaneOut (ComposerLayer::drums);
    REQUIRE (drums.existsAsFile());
    juce::MidiFile midi;
    juce::FileInputStream in (drums);
    REQUIRE (midi.readFrom (in));
    auto channel = 0;
    for (int t = 0; t < midi.getNumTracks(); ++t)
        for (const auto* e : *midi.getTrack (t))
            if (e->message.isNoteOn())
                channel = e->message.getChannel();
    CHECK (channel == 10);

    // Cleared with the parts' drag folder when the studio starts.
    clearDragOutFolder();
    CHECK_FALSE (composerDragFolder().exists());
}
