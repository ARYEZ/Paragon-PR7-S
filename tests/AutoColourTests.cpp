// 0.79.0 (the owner asked): COLOUR IN ORDER. Track n takes the colour menu's colour n (1-3 the
// gradient's three, then the palette row by row: 75, then round again), over a range of track
// numbers, only the kinds ticked (the others skipped and not counted), and the rule wins.

#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/TrackColours.h"

using namespace pr7s;

namespace
{
    juce::Colour colourOf (Studio& studio, int id) { return studio.getProject().findTrack (id)->colour; }

    std::vector<int> makeTracks (Studio& studio, int count)
    {
        while ((int) studio.getProject().tracks.size() < count)
            studio.addTrack (Instrument {});
        std::vector<int> ids;
        for (const auto& t : studio.getProject().tracks)
            ids.push_back (t.id);
        return ids;
    }
}

TEST_CASE ("0.79.0: the colour menu's colours are numbered as laid out, 75 of them, round again after", "[colour][079]")
{
    CHECK (FamilyColours::numberedCount == 75);
    CHECK (FamilyColours::numbered (1) == FamilyColours::pink);
    CHECK (FamilyColours::numbered (2) == FamilyColours::red);
    CHECK (FamilyColours::numbered (3) == FamilyColours::orange);
    CHECK (FamilyColours::numbered (4) == FamilyColours::paletteColour (0, 0));    // the first row, left
    CHECK (FamilyColours::numbered (12) == FamilyColours::paletteColour (8, 0));   // the first row, right
    CHECK (FamilyColours::numbered (13) == FamilyColours::paletteColour (0, 1));   // the next row
    CHECK (FamilyColours::numbered (75) == FamilyColours::paletteColour (8, 7));
    CHECK (FamilyColours::numbered (76) == FamilyColours::numbered (1));
    for (int n = 1; n <= 75; ++n)
        CHECK (FamilyColours::numberOf (FamilyColours::numbered (n)) == n);

    // The menu lays them out in that order: each swatch's number is its place.
    Studio studio (false);
    TrackColourMenu menu (studio, { studio.getProject().tracks.front().id });
    menu.resized();
    for (int n = 2; n <= 75; ++n)
    {
        const auto before = menu.swatchFor (FamilyColours::numbered (n - 1)), now = menu.swatchFor (FamilyColours::numbered (n));
        CHECK ((now.getY() > before.getY() || (now.getY() == before.getY() && now.getX() > before.getX())));
    }
}

TEST_CASE ("0.79.0: COLOUR IN ORDER: track n = colour n, kept as tracks move, are added and recoloured by hand; wraps at 76", "[colour][079]")
{
    Studio studio (false);
    auto ids = makeTracks (studio, 6);
    const auto handPicked = FamilyColours::paletteColour (4, 6);
    studio.setTracksColour ({ ids[2] }, handPicked);
    REQUIRE (colourOf (studio, ids[2]) == handPicked);

    AutoColour rule;
    rule.on = true;
    studio.setAutoColour (rule);
    for (int i = 0; i < 6; ++i)
        CHECK (colourOf (studio, ids[(size_t) i]) == FamilyColours::numbered (i + 1));

    // Moved: the track takes its new number's colour, the others too.
    REQUIRE (studio.moveTrackToSlot (ids[5], 1));
    const auto order = studio.getShownTracks();
    for (int i = 0; i < 6; ++i)
        CHECK (colourOf (studio, order[(size_t) i]) == FamilyColours::numbered (i + 1));

    // The rule wins over a colour picked by hand, and over undo.
    studio.setTracksColour ({ order[3] }, handPicked);
    CHECK (colourOf (studio, order[3]) == FamilyColours::numbered (4));
    studio.undo();
    for (int i = 0; i < 6; ++i)
        CHECK (colourOf (studio, studio.getShownTracks()[(size_t) i]) == FamilyColours::numbered (i + 1));

    // A new track takes the next colour; past 75 it starts again.
    makeTracks (studio, 77);
    const auto all = studio.getShownTracks();
    CHECK (colourOf (studio, all[74]) == FamilyColours::numbered (75));
    CHECK (colourOf (studio, all[75]) == FamilyColours::numbered (1));
    CHECK (colourOf (studio, all[76]) == FamilyColours::numbered (2));

    // Off: colours stay as they were and a hand-picked one sticks again.
    rule.on = false;
    studio.setAutoColour (rule);
    studio.setTracksColour ({ all[0] }, handPicked);
    CHECK (colourOf (studio, all[0]) == handPicked);
    CHECK (colourOf (studio, all[1]) == FamilyColours::numbered (2));
}

TEST_CASE ("0.79.0: COLOUR IN ORDER over a range, unticked kinds skipped and not counted; saved with the song", "[colour][079]")
{
    Studio studio (false);
    auto ids = makeTracks (studio, 2);                 // 1, 2: instruments
    const auto audio = studio.addAudioTrack (false);   // 3
    const auto group = studio.addGroupChannel();       // 4
    const auto more = studio.addTrack (Instrument {}); // 5
    const auto last = studio.addTrack (Instrument {}); // 6
    const auto shown = studio.getShownTracks();
    REQUIRE (shown.size() == 6);
    REQUIRE (shown[2] == audio);
    REQUIRE (shown[3] == group);
    const auto audioBefore = colourOf (studio, audio), firstBefore = colourOf (studio, ids[0]), lastBefore = colourOf (studio, last);

    AutoColour rule;
    rule.on = true;
    rule.from = 2;
    rule.to = 5;
    rule.audio = false;
    studio.setAutoColour (rule);
    CHECK (colourOf (studio, ids[0]) == firstBefore);              // before the range
    CHECK (colourOf (studio, ids[1]) == FamilyColours::numbered (2));   // track 2: colour 2
    CHECK (colourOf (studio, audio) == audioBefore);               // skipped
    CHECK (colourOf (studio, group) == FamilyColours::numbered (3));    // the count closes up
    CHECK (colourOf (studio, more) == FamilyColours::numbered (4));
    CHECK (colourOf (studio, last) == lastBefore);                 // after the range
    CHECK (studio.autoColourNumbers().size() == 3);

    // Saved and opened again.
    const auto xml = studio.getProject().toXml();
    const auto back = Project::fromXml (*xml);
    REQUIRE (back.has_value());
    CHECK (back->autoColour == studio.getProject().autoColour);

    // The menu's keys show it and set it.
    TrackColourMenu menu (studio, { ids[0] });
    CHECK (menu.orderKey.getToggleState());
    CHECK ((int) menu.orderFrom.getValue() == 2);
    CHECK ((int) menu.orderTo.getValue() == 5);
    CHECK_FALSE (menu.kindKeys[1].getToggleState());
    CHECK (menu.getOrderText().startsWith ("3 TRACKS"));
    menu.orderTo.setValue (6, juce::sendNotificationSync);   // all the way up: to the last
    CHECK (studio.getProject().autoColour.to == 0);
    CHECK (colourOf (studio, last) == FamilyColours::numbered (5));
    menu.kindKeys[1].setToggleState (true, juce::sendNotificationSync);
    menu.kindKeys[1].onClick();
    CHECK (colourOf (studio, audio) == FamilyColours::numbered (3));
    CHECK (colourOf (studio, last) == FamilyColours::numbered (6));
}
