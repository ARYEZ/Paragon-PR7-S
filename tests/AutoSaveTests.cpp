// 0.72.0: AUTO-SAVE (backup copies on a timer, the Hub's choices) and an automation lane's X.

#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/Hub.h"
#include "ui/MainView.h"
#include "ui/TrackList.h"

using namespace pr7s;

TEST_CASE ("0.72.0: auto-save writes backup copies beside the song, never the song itself; old copies go", "[autosave]")
{
    const auto folder = juce::File::createTempFile ("pr7s-autosave");
    folder.createDirectory();
    const auto song = folder.getChildFile ("Riff.pr7s");
    Studio studio (false);
    CHECK (studio.getAutoSaveMinutes() == 0);   // (without a settings file: off)
    studio.setAutoSaveMinutes (5);
    CHECK (studio.getAutoSaveMinutes() == 5);
    studio.setAutoSaveKeep (3);
    CHECK (studio.getAutoSaveKeep() == 3);
    studio.setAutoSaveKeep (0);
    CHECK (studio.getAutoSaveKeep() == 1);   // (at least one)
    studio.setAutoSaveKeep (3);

    // A recording, so the backup must find it again.
    const auto tone = juce::File (PR7S_TEST_DATA).getChildFile ("tone.mp3");
    juce::String error;
    REQUIRE (studio.importAudio ({ tone }, 0, 0.0, error) > 0);
    REQUIRE (studio.save (song));
    const auto songText = song.loadFileAsString();
    CHECK (studio.backupFolder() == folder.getChildFile ("Backups"));

    // Older copies already there (three), and another song's (kept: not this song's).
    folder.getChildFile ("Backups").createDirectory();
    for (const auto* stamp : { "2020-01-01 00-00-01", "2020-01-01 00-00-02", "2020-01-01 00-00-03" })
        folder.getChildFile ("Backups").getChildFile (juce::String ("Riff backup ") + stamp + ".pr7s").replaceWithText ("<old/>");
    folder.getChildFile ("Backups").getChildFile ("Other backup 2020-01-01 00-00-01.pr7s").replaceWithText ("<old/>");

    studio.renameTrack (studio.getProject().tracks[0].id, "Changed");
    REQUIRE (studio.writeBackup());
    REQUIRE (studio.waitForSaves (5000));
    const auto backup = studio.getLastBackup();
    CHECK (backup.existsAsFile());
    CHECK (backup.getParentDirectory() == folder.getChildFile ("Backups"));
    CHECK (backup.getFileName().startsWith ("Riff backup "));
    CHECK (song.loadFileAsString() == songText);   // (the song's own file untouched)
    // The newest three of this song's copies: ours and the two latest old ones; the other song's stays.
    auto copies = folder.getChildFile ("Backups").findChildFiles (juce::File::findFiles, false, "Riff backup *.pr7s");
    CHECK (copies.size() == 3);
    CHECK_FALSE (folder.getChildFile ("Backups").getChildFile ("Riff backup 2020-01-01 00-00-01.pr7s").exists());
    CHECK (folder.getChildFile ("Backups").getChildFile ("Other backup 2020-01-01 00-00-01.pr7s").exists());

    // Nothing changed since: no new copy.
    CHECK_FALSE (studio.writeBackup());

    // The backup opens with its recordings (they're beside the song, not the backup).
    Studio other (false);
    juce::String problems;
    REQUIRE (other.open (backup, problems));
    CHECK (problems.isEmpty());
    CHECK (other.getMissing().empty());
    CHECK (other.getAudioFolder() == folder.getChildFile ("Riff Audio"));
    CHECK (other.getProject().tracks[0].name == "Changed");
    // Saved as a song somewhere else: its recordings go with it, and it's an ordinary song again.
    const auto rescued = folder.getChildFile ("Rescued.pr7s");
    REQUIRE (other.save (rescued));
    CHECK (other.getAudioFolder() == folder.getChildFile ("Rescued Audio"));
    CHECK (folder.getChildFile ("Rescued Audio").getNumberOfChildFiles (juce::File::findFiles) > 0);
    folder.deleteRecursively();
}

TEST_CASE ("0.72.0: HUB SETTINGS has AUTO-SAVE's choices", "[autosave][ui]")
{
    Studio studio (false);
    Hub hub (studio);
    hub.setSize (1500, 900);
    hub.getAutoSaveBox().setSelectedId (10 + 1, juce::sendNotificationSync);   // (every 10 minutes)
    CHECK (studio.getAutoSaveMinutes() == 10);
    hub.getAutoSaveBox().setSelectedId (0 + 1, juce::sendNotificationSync);   // (off)
    CHECK (studio.getAutoSaveMinutes() == 0);
    hub.getKeepBox().setSelectedId (20, juce::sendNotificationSync);
    CHECK (studio.getAutoSaveKeep() == 20);
}

TEST_CASE ("0.72.0: an automation lane's X removes that lane only", "[automation][ui][autosave]")
{
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.setAutomationOpen (trackId, true);
    studio.appendAutomationLane (trackId, 0);
    studio.dispatchPendingMessages();
    REQUIRE (studio.getAutomation (trackId)->size() == 2);
    const auto second = (*studio.getAutomation (trackId))[1];
    auto* x = view.getTrackList().getLaneRemoveKey (trackId, 0);
    REQUIRE (x != nullptr);
    x->onClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    studio.dispatchPendingMessages();
    REQUIRE (studio.getAutomation (trackId)->size() == 1);
    CHECK ((*studio.getAutomation (trackId))[0].sameControl (second));
    studio.undo();
    CHECK (studio.getAutomation (trackId)->size() == 2);
}
