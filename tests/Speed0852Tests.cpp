// Speed, round four (0.85.2): each change checked to give the same result as before.

#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/Chords.h"
#include "engine/KeyCheck.h"
#include "engine/Studio.h"

using namespace pr7s;

TEST_CASE ("0.85.2: an auto-save copy is written out on the saver's thread, and still only when something changed", "[speed0852][autosave]")
{
    const auto folder = juce::File::createTempFile ("pr7s-backup0852");
    folder.createDirectory();
    Studio studio (false);
    studio.setAutoSaveMinutes (5);
    REQUIRE (studio.save (folder.getChildFile ("Song.pr7s")));
    const auto id = studio.getProject().tracks[0].id;

    studio.renameTrack (id, "First");
    REQUIRE (studio.writeBackup());
    REQUIRE (studio.waitForSaves (5000));
    const auto first = studio.getLastBackup();
    REQUIRE (first.existsAsFile());
    // The copy is the song as it was when asked for, with where its recordings are.
    const auto xml = juce::parseXML (first);
    REQUIRE (xml != nullptr);
    CHECK (xml->getStringAttribute ("backupAudioFolder") == studio.getAudioFolder().getFullPathName());
    CHECK (xml->toString().contains ("First"));
    CHECK_FALSE (studio.writeBackup());   // (nothing changed)

    // A fader drag's steps (not undo steps, no change counted) still make the next copy.
    studio.setVolume (id, -7.5f, false);
    CHECK (studio.writeBackup());
    REQUIRE (studio.waitForSaves (5000));
    CHECK_FALSE (studio.writeBackup());
    folder.deleteRecursively();
}

TEST_CASE ("0.85.2: a drummer profile read once is kept, and read again when its file changes", "[speed0852][drummer]")
{
    const auto folder = juce::File::createTempFile ("pr7s-drummer0852");
    folder.createDirectory();
    Studio studio (false);
    studio.setDrummerFolderForTests (folder);
    DrumProfile profile;
    profile.name = "Kept";
    profile.songs.push_back ({ "one.mid", "", true });
    juce::String error;
    REQUIRE (studio.saveDrummer (profile, error));
    const auto a = studio.loadDrummer ("Kept");
    REQUIRE (a.has_value());
    CHECK (a->songs.size() == 1);
    CHECK (studio.loadDrummer ("Kept")->songs.size() == 1);   // (kept)

    profile.songs.push_back ({ "two.mid", "", true });
    REQUIRE (studio.saveDrummer (profile, error));
    const auto b = studio.loadDrummer ("Kept");
    REQUIRE (b.has_value());
    CHECK (b->songs.size() == 2);   // (the file changed: read again)

    REQUIRE (studio.deleteDrummer ("Kept"));
    CHECK_FALSE (studio.loadDrummer ("Kept").has_value());
    folder.deleteRecursively();
}

TEST_CASE ("0.85.2: a note in or out of a key by the scale's steps, as the key's own scale says", "[speed0852][keycheck]")
{
    for (const auto& key : Key::all())
    {
        const auto scale = key.scale();
        for (int p = -24; p < 140; ++p)
        {
            const auto pc = ((p % 12) + 12) % 12;
            const auto inScale = std::find (scale.begin(), scale.end(), pc) != scale.end();
            const auto raised = key.minor && pc == (key.pitchClass() + 11) % 12;
            CHECK (isPitchClassInKey (p, key) == (inScale || raised));
        }
    }
}
