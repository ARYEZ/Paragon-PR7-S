// 0.80.0 (the owner's tweaks): selected automation points lit (no editing box unless asked for),
// a default colour and track-list colour per track type (the Hub's TRACK COLOURS), and the
// BOOSTER's values typed in on a double-click as the other faders'.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/ArrangeView.h"
#include "ui/BoosterView.h"
#include "ui/Hub.h"
#include "ui/MainView.h"
#include "ui/TrackTypeColours.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

TEST_CASE ("0.80.0: several automation points selected: no editing box or handles unless it's switched on", "[automation][ui][080]")
{
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.setAutomationOpen (trackId, true);
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    for (int i = 0; i < 4; ++i)
        arrange.zoomRows (true);
    studio.setAutomationPoints (trackId, 0, { { 1.0, 0.2f, 0.0f }, { 3.0, 0.6f, 0.0f }, { 5.0, 0.4f, 0.0f } });
    arrange.selectPoints (trackId, 0, { 0, 1, 2 });
    CHECK_FALSE (arrange.isPointBoxShown());
    CHECK (arrange.getPointBox().isEmpty());
    const auto middle = arrange.getPointPosition (trackId, 0, 1);
    CHECK (arrange.pointHandleAt (middle.translated (0.0f, -40.0f), false) == ArrangeView::PointHandle::none);
    arrange.setPointBoxShown (true);
    CHECK_FALSE (arrange.getPointBox().isEmpty());
    arrange.setPointBoxShown (false);
    CHECK (arrange.getPointBox().isEmpty());
}

TEST_CASE ("0.80.0: each track type's default colour and track-list colour go onto new tracks", "[colour][080]")
{
    Studio studio (false);
    // AUTOMATIC by default: red, pink, orange by place, no list colour.
    const auto first = studio.addAudioTrack (false);
    CHECK (studio.getProject().findTrack (first)->colour == trackColour ((int) studio.getProject().tracks.size() - 1));
    CHECK_FALSE (studio.getProject().findTrack (first)->listColourOn);

    const auto audioColour = FamilyColours::paletteColour (6, 4);
    studio.setTrackTypeLook (Track::Kind::audio, { audioColour, true, 40 });
    studio.setTrackTypeLook (Track::Kind::group, { FamilyColours::orange, false, 25 });
    const auto audio = studio.addAudioTrack (true);
    const auto group = studio.addGroupChannel();
    const auto synth = studio.addTrack (Instrument {});
    const auto* a = studio.getProject().findTrack (audio);
    CHECK (a->colour == audioColour);
    CHECK (a->listColourOn);
    CHECK (a->listColourAmount == 40);
    CHECK (studio.getProject().findTrack (group)->colour == FamilyColours::orange);
    CHECK_FALSE (studio.getProject().findTrack (group)->listColourOn);
    CHECK (FamilyColours::isFamily (studio.getProject().findTrack (synth)->colour));   // (instrument: still automatic)

    // SET TRACK COLOUR TO DEFAULT gives the type's colour.
    studio.setTracksColour ({ audio }, FamilyColours::red);
    studio.setTracksColour ({ audio }, std::nullopt);
    CHECK (studio.getProject().findTrack (audio)->colour == audioColour);

    // Apply to this song: the first audio track (made before) takes it too; one undo step.
    studio.applyTrackTypeLooks();
    CHECK (studio.getProject().findTrack (first)->colour == audioColour);
    CHECK (studio.getProject().findTrack (first)->listColourOn);

    // Not one of the family's: its nearest.
    studio.setTrackTypeLook (Track::Kind::fx, { juce::Colours::blue, false, 25 });
    CHECK (FamilyColours::isFamily (*studio.getTrackTypeLook (Track::Kind::fx).colour));

    // COLOUR IN ORDER still wins.
    AutoColour rule;
    rule.on = true;
    studio.setAutoColour (rule);
    const auto later = studio.addAudioTrack (false);
    const auto shown = studio.getShownTracks();
    const auto place = (int) (std::find (shown.begin(), shown.end(), later) - shown.begin());
    CHECK (studio.getProject().findTrack (later)->colour == FamilyColours::numbered (place + 1));
}

TEST_CASE ("0.80.0: the Hub's TRACK COLOURS page sets and shows each type's look", "[colour][hub][080]")
{
    Studio studio (false);
    Hub hub (studio);
    hub.setSize (Hub::preferredWidth, Hub::preferredHeight);
    hub.showColoursPage();
    REQUIRE (hub.isShowingColours());
    auto& page = *hub.getColoursPage();
    CHECK (page.isVisible());
    page.setColour (Track::Kind::folder, FamilyColours::pink);
    page.setListColour (Track::Kind::folder, true, 70);
    CHECK (studio.getTrackTypeLook (Track::Kind::folder).colour == FamilyColours::pink);
    CHECK (page.getRow (Track::Kind::folder).swatch.colour == FamilyColours::pink);
    CHECK (page.getRow (Track::Kind::folder).listKey.getToggleState());
    CHECK ((int) page.getRow (Track::Kind::folder).amount.getValue() == 70);
    const auto folder = studio.addFolderTrack();
    CHECK (studio.getProject().findTrack (folder)->colour == FamilyColours::pink);
    page.resetAll();
    CHECK_FALSE (studio.getTrackTypeLook (Track::Kind::folder).colour.has_value());
    hub.showSettingsPage (true);
    CHECK_FALSE (hub.isShowingColours());
    CHECK_FALSE (page.isVisible());

    FamilyColourPicker picker (std::nullopt);
    picker.resized();
    std::optional<juce::Colour> picked { juce::Colours::black };
    picker.onPick = [&] (std::optional<juce::Colour> c) { picked = c; };
    picker.automaticKey.onClick();
    CHECK_FALSE (picked.has_value());
}

TEST_CASE ("0.80.0: the BOOSTER's trim and level are typed in, as the other faders'", "[booster][ui][080]")
{
    Studio studio (false);
    const auto trackId = studio.getProject().tracks.front().id;
    BoosterView view (studio);
    view.setBus (trackId);
    view.setSize (200, 210);
    CHECK (view.getTrim().enter ("+2.5 dB"));
    CHECK_THAT (studio.getBooster (trackId)->trimDb, WithinAbs (2.5, 1.0e-4));
    CHECK (view.getFader().enter ("-6"));
    CHECK_THAT (studio.getBooster (trackId)->levelDb, WithinAbs (-6.0, 0.051));
    CHECK (view.getFader().enter ("-inf"));
    CHECK (studio.getBooster (trackId)->levelDb <= Booster::leastLevel);
    CHECK (view.getFader().enter ("+30"));   // (held to the top)
    CHECK_THAT (studio.getBooster (trackId)->levelDb, WithinAbs (Booster::mostLevel, 0.051));
    CHECK_FALSE (view.getTrim().enter ("loud"));
    CHECK (view.getTrim().enter ("-40"));    // (held to -12)
    CHECK_THAT (studio.getBooster (trackId)->trimDb, WithinAbs (-12.0, 1.0e-4));
    // One undo step each.
    studio.undo();
    CHECK_THAT (studio.getBooster (trackId)->trimDb, WithinAbs (2.5, 1.0e-4));
    // The entry box shows the value as it reads (the view follows the song: its timer, here at once).
    view.refresh();
    CHECK (view.getTrim().getTextFromValue (view.getTrim().getValue()) == juce::String ("+2.5 DB"));
}

#include "engine/KeepLoaded.h"

TEST_CASE ("0.80.1: keeping a plug-in's file loaded: nothing to keep for a path that isn't loaded (and nothing at all off Windows)", "[plugins][080]")
{
    CHECK (keepPluginLoaded ({}) == 0);
    CHECK (keepPluginLoaded (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("No Such Plug-in.vst3").getFullPathName()) == 0);
}
