#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/Studio.h"
#include "engine/Composer.h"
#include "ui/ComposerPage.h"
#include "ui/ComposerTracksPage.h"

// (0.201.0, the owner's levels) The fader level each ARCHITECT lane's new tracks start at: chords -11.5 dB,
// melody -9.5, bass -5.5, the rest 0. Tracks found by name keep their own fader.

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    using L = ComposerLayer;

    float levelOf (const Studio& studio, int trackId)
    {
        const auto* t = studio.getProject().findTrack (trackId);
        return t != nullptr ? t->volumeDb : 999.0f;
    }

    const Track* trackNamed (const Studio& studio, const juce::String& name)
    {
        for (const auto& t : studio.getProject().tracks)
            if (t.name == name)
                return &t;
        return nullptr;
    }
}

TEST_CASE ("0.201.0: ARCHITECT's lane levels start at the owner's numbers", "[composer][architectlevels]")
{
    Studio studio (false);
    CHECK (studio.getLaneLevel (LineIdea::Part::chords) == -11.5f);
    CHECK (studio.getLaneLevel (LineIdea::Part::tune) == -9.5f);
    CHECK (studio.getLaneLevel (LineIdea::Part::melody) == -9.5f);
    CHECK (studio.getLaneLevel (LineIdea::Part::bass) == -5.5f);
    for (const auto part : { LineIdea::Part::rhythm, LineIdea::Part::lead, LineIdea::Part::clean, LineIdea::Part::acoustic,
                             LineIdea::Part::drums, LineIdea::Part::groove })
        CHECK (studio.getLaneLevel (part) == 0.0f);
    for (int i = 0; i < Studio::numComposerTrackLanes; ++i)
        CHECK (studio.getLaneLevels()[(size_t) i] == Studio::defaultLaneLevel (i));

    // Clamped, kept, reset.
    studio.setLaneLevel (0, -80.0f);
    CHECK (studio.getLaneLevel (LineIdea::Part::bass) == -60.0f);
    studio.setLaneLevel (0, 12.0f);
    CHECK (studio.getLaneLevel (LineIdea::Part::bass) == 6.0f);
    studio.resetLaneLevels();
    CHECK (studio.getLaneLevel (LineIdea::Part::bass) == -5.5f);
}

TEST_CASE ("0.201.0: a BUILD onto a new song makes its tracks at the lanes' levels", "[composer][architectlevels]")
{
    Studio studio (false);
    ComposerPage page (studio);
    page.setRandomSeed (3);
    for (const auto l : composerLayers())
        page.setOn (l, true);
    REQUIRE_FALSE (page.buildSection().empty());

    const std::pair<LineIdea::Part, float> expected[] { { LineIdea::Part::chords, -11.5f }, { LineIdea::Part::tune, -9.5f },
                                                        { LineIdea::Part::bass, -5.5f }, { LineIdea::Part::rhythm, 0.0f },
                                                        { LineIdea::Part::lead, 0.0f }, { LineIdea::Part::clean, 0.0f },
                                                        { LineIdea::Part::acoustic, 0.0f }, { LineIdea::Part::drums, 0.0f } };
    for (const auto& [part, db] : expected)
    {
        const auto name = studio.newIdeaTrackName (part);
        INFO (name);
        const auto* t = trackNamed (studio, name);
        REQUIRE (t != nullptr);
        CHECK_THAT (t->volumeDb, WithinAbs (db, 1.0e-4));
    }
}

TEST_CASE ("0.201.0: a track found by name keeps its own fader", "[composer][architectlevels]")
{
    Studio studio (false);
    const auto bass = studio.addTrack (Instrument {});
    studio.renameTrack (bass, "Bass");
    studio.setVolume (bass, 2.5f);
    const std::vector<ChordSpan> spans { { 0.0, 4.0, Chord { 0 }, Key {} } };
    LineIdea idea;
    idea.part = LineIdea::Part::bass;
    idea.notes.push_back ({ 0.0, 1.0, 40, 0.8f });
    const auto made = studio.buildSection ({ idea }, spans, "Verse");
    REQUIRE (made == std::vector<int> { bass });
    CHECK (levelOf (studio, bass) == 2.5f);

    // IDEAS' USE IT on a new track: the lane's level; onto a chosen track: its own.
    idea.part = LineIdea::Part::chords;
    idea.notes[0].pitch = 60;
    const auto keys = studio.addLinePart (idea, spans, 0);
    REQUIRE (keys != 0);
    CHECK (levelOf (studio, keys) == -11.5f);
    CHECK (studio.addLinePart (idea, spans, bass) == bass);
    CHECK (levelOf (studio, bass) == 2.5f);
}

TEST_CASE ("0.201.0: a changed level is saved and used", "[composer][architectlevels]")
{
    const auto settingsFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-lane-levels.settings");
    settingsFile.deleteFile();
    {
        Studio studio (false);
        studio.setSettingsForTests (std::make_unique<juce::PropertiesFile> (settingsFile, juce::PropertiesFile::Options {}));
        CHECK (studio.getLaneLevel (LineIdea::Part::chords) == -11.5f);   // (no value saved yet: the owner's)
        studio.setLaneLevel (Studio::composerTrackLane (LineIdea::Part::drums), -3.0f);
        studio.setSettingsForTests (nullptr);
    }
    Studio studio (false);
    studio.setSettingsForTests (std::make_unique<juce::PropertiesFile> (settingsFile, juce::PropertiesFile::Options {}));
    CHECK (studio.getLaneLevel (LineIdea::Part::drums) == -3.0f);
    CHECK (studio.getLaneLevel (LineIdea::Part::chords) == -11.5f);
    const auto id = studio.addIdeaTrack (LineIdea::Part::drums);
    REQUIRE (id != 0);
    CHECK (levelOf (studio, id) == -3.0f);
    CHECK (studio.getProject().findTrack (id)->name == "Drums");
    studio.setSettingsForTests (nullptr);
    settingsFile.deleteFile();
}

TEST_CASE ("0.201.0: addIdeaTrack (the TRACK menu's new track)", "[composer][architectlevels]")
{
    Studio studio (false);
    const auto id = studio.addIdeaTrack (LineIdea::Part::tune);
    CHECK (levelOf (studio, id) == -9.5f);
    CHECK (studio.getProject().findTrack (id)->name == "Melody");
    // The engine has it too (a change was made after the level was set).
    CHECK (studio.getProject().findTrack (id)->volumeDb == -9.5f);
    const auto drums = studio.addIdeaTrack (LineIdea::Part::groove);
    CHECK (levelOf (studio, drums) == 0.0f);
    CHECK (studio.getProject().findTrack (drums)->name == "Drums");
}

TEST_CASE ("0.201.0: ARCHITECT TRACKS' LEVEL row", "[composer][architectlevels][ui]")
{
    Studio studio (false);
    ComposerTracksPage page (studio);
    page.setSize (1200, 900);
    const auto chords = Studio::composerTrackLane (LineIdea::Part::chords);
    CHECK (page.getRow (chords).level.getValue() == -11.5);
    page.getRow (chords).level.setValue (-7.0, juce::sendNotificationSync);
    CHECK (studio.getLaneLevel (LineIdea::Part::chords) == -7.0f);
    CHECK (page.getRow (chords).level.enter ("-4.5 dB"));
    CHECK (studio.getLaneLevel (LineIdea::Part::chords) == -4.5f);
    page.setLevel (0, -20.0f);
    CHECK (page.getRow (0).level.getValue() == -20.0);
    CHECK (page.getRow (chords).level.getTextFromValue (-11.5) == "-11.5 DB");
    page.resetAll();
    CHECK (studio.getLaneLevel (LineIdea::Part::chords) == -11.5f);
    CHECK (studio.getLaneLevel (LineIdea::Part::bass) == -5.5f);
    CHECK (page.getRow (chords).level.getValue() == -11.5);
    CHECK (page.getRow (chords).level.isVisible());
    CHECK (page.getRow (Studio::composerTrackLane (LineIdea::Part::drums)).level.getTextFromValue (0.0) == "0.0 DB");
}
