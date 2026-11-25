#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/Studio.h"
#include "engine/Composer.h"
#include "ui/ComposerPage.h"
#include "ui/ComposerTracksPage.h"

// (0.206.0, the owner's two reports) ARCHITECT's state and riff belong to the song (a new song, another song or a
// template brings its own); the lane level bar is the lane's ARCHITECT level: what HEAR plays at, where a new or empty
// track starts; it never reads or moves a track's fader.

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    using L = ComposerLayer;

    ComposerRiff testRiff()
    {
        ComposerRiff r;
        r.bars = 1;
        r.section = DrumSection::chorus;
        r.name = "Test riff";
        for (int i = 0; i < 8; ++i)
            r.notes.push_back ({ i * 0.5, 0.25, i % 2 == 0 ? 33 : 36, 0.9f });
        return r;
    }

    juce::File tempSong (const juce::String& name)
    {
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("PR7S-architect-song");
        folder.createDirectory();
        return folder.getChildFile (name + Project::fileExtension);
    }
}

TEST_CASE ("0.206.0: ARCHITECT's state and riff are the song's", "[composer][architectsong]")
{
    Studio studio (false);
    ComposerPage page (studio);
    page.setRandomSeed (5);
    page.setStyle (L::bass, 2);
    page.setLocked (L::guitar, true);
    page.setOn (L::lead, true);
    REQUIRE (page.pasteRiff (testRiff()));
    const auto stateA = page.getState();
    REQUIRE (studio.getComposerRiff().has_value());
    CHECK (studio.isComposerRiffPlayed());
    CHECK (studio.getComposerState().isNotEmpty());
    CHECK (studio.isModified());

    const auto fileA = tempSong ("Song A");
    REQUIRE (studio.save (fileA));
    CHECK_FALSE (studio.isModified());

    // A new song: ARCHITECT clean (the page as a kept window: started over when it notices).
    const auto generation = studio.getSongGeneration();
    studio.newProject();
    CHECK (studio.getSongGeneration() != generation);
    CHECK (studio.getComposerState().isEmpty());
    CHECK_FALSE (studio.getComposerRiff().has_value());
    CHECK (page.startOverIfNewSong());
    CHECK_FALSE (page.startOverIfNewSong());
    const ComposerState fresh;
    CHECK (page.getState().lane (L::bass).style == fresh.lane (L::bass).style);
    CHECK_FALSE (page.getState().lane (L::guitar).locked);
    CHECK (page.getState().lane (L::lead).on == fresh.lane (L::lead).on);
    CHECK_FALSE (page.isPlayingMyRiff());
    CHECK (page.getSeenSongGeneration() == studio.getSongGeneration());

    // Song A again: its state and riff back.
    juce::String problems;
    REQUIRE (studio.open (fileA, problems));
    REQUIRE (studio.getComposerRiff().has_value());
    CHECK (*studio.getComposerRiff() == testRiff());
    CHECK (studio.isComposerRiffPlayed());
    CHECK (page.startOverIfNewSong());
    CHECK (page.getState().lane (L::bass).style == stateA.lane (L::bass).style);
    CHECK (page.getState().lane (L::guitar).locked);
    CHECK (page.getState().lane (L::lead).on);
    CHECK (page.isPlayingMyRiff());
    CHECK_FALSE (studio.isModified());   // (loading it back didn't count as a change)
    fileA.getParentDirectory().deleteRecursively();
}

TEST_CASE ("0.206.0: a template carries ARCHITECT's state", "[composer][architectsong]")
{
    Studio studio (false);
    {
        ComposerPage page (studio);
        page.setStyle (L::drums, 3);
        page.setOn (L::melody, true);
    }
    const auto saved = ComposerState::fromString (studio.getComposerState());
    CHECK (saved.lane (L::drums).style == 3);
    const auto templateFile = tempSong ("Architect template");
    REQUIRE (studio.save (templateFile));
    studio.newProject();
    CHECK (studio.getComposerState().isEmpty());
    juce::String problems;
    REQUIRE (studio.newFromTemplate (templateFile, problems));
    CHECK (ComposerState::fromString (studio.getComposerState()) == saved);
    ComposerPage page (studio);
    CHECK (page.getState().lane (L::drums).style == 3);
    CHECK (page.getState().lane (L::melody).on);
    templateFile.getParentDirectory().deleteRecursively();
}

TEST_CASE ("0.206.0: an old song opens with ARCHITECT clean, the setting not read", "[composer][architectsong]")
{
    const auto settingsFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-architect-song.settings");
    settingsFile.deleteFile();
    Studio studio (false);
    studio.setSettingsForTests (std::make_unique<juce::PropertiesFile> (settingsFile, juce::PropertiesFile::Options {}));
    auto old = ComposerState {};
    old.lane (L::bass).style = 1;
    studio.getSettings()->setValue ("composer", old.toString());   // (what 0.205.0 kept for every song)
    studio.newProject();
    CHECK (studio.getComposerState().isEmpty());
    ComposerPage page (studio);
    CHECK (page.getState().lane (L::bass).style == ComposerState {}.lane (L::bass).style);
    studio.setSettingsForTests (nullptr);
    settingsFile.deleteFile();
}

TEST_CASE ("0.206.0: the lane bar is the ARCHITECT level, never a track's fader", "[composer][architectsong][architectlevels]")
{
    Studio studio (false);
    const auto bass = studio.addTrack (Instrument {});
    studio.renameTrack (bass, "Bass");
    studio.setVolume (bass, 0.0f);
    studio.selectTrack (bass);   // (the old bar read the hearing's stand-in: the selected track)
    ComposerPage page (studio);
    ComposerTracksPage hub (studio);
    CHECK_THAT (page.getLaneVolume (L::chords).getValue(), WithinAbs (-11.5, 1.0e-4));
    CHECK_THAT (page.getLaneVolume (L::bass).getValue(), WithinAbs (-5.5, 1.0e-4));   // (not Bass's 0 dB)
    page.getLaneVolume (L::chords).setValue (-8.0, juce::sendNotificationSync);
    CHECK_THAT (studio.getLaneLevel (LineIdea::Part::chords), WithinAbs (-8.0, 1.0e-4));
    page.getLaneVolume (L::bass).setValue (-2.0, juce::sendNotificationSync);
    CHECK_THAT (studio.getLaneLevel (LineIdea::Part::bass), WithinAbs (-2.0, 1.0e-4));
    CHECK (studio.getProject().findTrack (bass)->volumeDb == 0.0f);   // (the fader untouched)
    hub.refresh();
    CHECK_THAT (hub.getRow (Studio::composerTrackLane (LineIdea::Part::chords)).level.getValue(), WithinAbs (-8.0, 1.0e-4));
    // The Hub's change shows in ARCHITECT.
    hub.setLevel (Studio::composerTrackLane (LineIdea::Part::chords), -14.0f);
    page.refreshForTests();
    CHECK_THAT (page.getLaneVolume (L::chords).getValue(), WithinAbs (-14.0, 1.0e-4));
    // Typing a level is a setting: no undo step, the song not changed.
    const auto steps = studio.getHistory().size();
    page.setLaneVolume (L::chords, -9.0f);
    CHECK (studio.getHistory().size() == steps);
}

TEST_CASE ("0.206.0: HEAR plays a lane's tracks at its ARCHITECT level, the song's fader kept", "[composer][architectsong][architectlevels]")
{
    Studio studio (false);
    const auto bass = studio.addTrack (Instrument {});
    studio.renameTrack (bass, "Bass");
    studio.setVolume (bass, 3.0f);
    LineIdea idea;
    idea.part = LineIdea::Part::bass;
    idea.notes.push_back ({ 0.0, 1.0, 40, 0.8f });
    REQUIRE (studio.auditionComposer ({ { bass, idea } }, 0.0, 4.0));
    auto heard = studio.getComposerAuditionCopyForTests();
    REQUIRE (heard.has_value());
    CHECK_THAT (heard->findTrack (bass)->volumeDb, WithinAbs (-5.5, 1.0e-4));
    CHECK (studio.getProject().findTrack (bass)->volumeDb == 3.0f);
    studio.setLaneLevel (Studio::composerTrackLane (LineIdea::Part::bass), -12.0f);
    heard = studio.getComposerAuditionCopyForTests();
    CHECK_THAT (heard->findTrack (bass)->volumeDb, WithinAbs (-12.0, 1.0e-4));
    CHECK (studio.getProject().findTrack (bass)->volumeDb == 3.0f);
    studio.stopComposerAudition();
    CHECK (studio.getProject().findTrack (bass)->volumeDb == 3.0f);
}

TEST_CASE ("0.206.0: an empty track found for a lane starts at its level on BUILD", "[composer][architectsong][architectlevels]")
{
    Studio studio (false);
    const auto bass = studio.addTrack (Instrument {});
    studio.renameTrack (bass, "Bass");
    studio.setVolume (bass, 0.0f);
    const auto keys = studio.addTrack (Instrument {});
    studio.renameTrack (keys, "Keys");   // (the chords lane's name)
    studio.setVolume (keys, 1.0f);
    Part p;
    p.start = 16.0;
    p.length = 4.0;
    studio.addPart (keys, p);
    const std::vector<ChordSpan> spans { { 0.0, 4.0, Chord { 0 }, Key {} } };
    LineIdea b, c;
    b.part = LineIdea::Part::bass;
    b.notes.push_back ({ 0.0, 1.0, 40, 0.8f });
    c.part = LineIdea::Part::chords;
    c.notes.push_back ({ 0.0, 1.0, 60, 0.8f });
    const auto steps = studio.getHistoryPosition();
    const auto made = studio.buildSection ({ b, c }, spans, "Verse", { bass, keys });
    REQUIRE (made.size() == 2);
    CHECK_THAT (studio.getProject().findTrack (bass)->volumeDb, WithinAbs (-5.5, 1.0e-4));
    CHECK (studio.getProject().findTrack (keys)->volumeDb == 1.0f);
    CHECK (studio.getHistoryPosition() == steps + 1);   // (one step: the level with the parts)
    studio.undo();
    CHECK (studio.getProject().findTrack (bass)->volumeDb == 0.0f);
    CHECK (studio.getProject().findTrack (bass)->parts.empty());
}

TEST_CASE ("0.229.0: a mix drag while ARCHITECT plays keeps the lanes at their levels", "[composer][architectsong][architectlevels]")
{
    Studio studio (false);
    const auto bass = studio.addTrack (Instrument {});
    studio.renameTrack (bass, "Bass");
    studio.setVolume (bass, 3.0f);
    const auto other = studio.addTrack (Instrument {});
    LineIdea idea;
    idea.part = LineIdea::Part::bass;
    idea.notes.push_back ({ 0.0, 1.0, 40, 0.8f });
    REQUIRE (studio.auditionComposer ({ { bass, idea } }, 0.0, 4.0));
    // A fader drag's step on another track: the mix the engine is given has the lane's level on the bass, not the
    // song's +3 dB (the jump before), and no layers' parts made for it.
    studio.setVolume (other, -6.0f, false);
    const auto mix = studio.getComposerMixCopyForTests();
    REQUIRE (mix.has_value());
    CHECK_THAT (mix->findTrack (bass)->volumeDb, WithinAbs (-5.5, 1.0e-4));
    CHECK_THAT (mix->findTrack (other)->volumeDb, WithinAbs (-6.0, 1.0e-4));
    CHECK (mix->findTrack (bass)->parts.size() == studio.getProject().findTrack (bass)->parts.size());
    CHECK (mix->findTrack (bass)->solo);
    CHECK_FALSE (mix->findTrack (other)->solo);
    CHECK (studio.getProject().findTrack (bass)->volumeDb == 3.0f);
    studio.stopComposerAudition();
}
