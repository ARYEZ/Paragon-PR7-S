#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    int newTrack (Studio& studio)
    {
        return studio.addTrack ({});
    }
    float volumeOf (const Studio& studio, int id) { return studio.getProject().findTrack (id)->volumeDb; }
}

TEST_CASE ("Link groups: made from the selected channels, the manual's defaults, saved, undone (0.17.0)", "[link]")
{
    Studio studio (false);
    const auto a = studio.getProject().tracks[0].id, b = newTrack (studio), c = newTrack (studio);
    CHECK (studio.linkTracks ({ a }, {}) == 0);   // (two channels at least)

    const auto group = studio.linkTracks ({ a, b }, {});
    REQUIRE (group != 0);
    const auto* settings = studio.getLinkGroupOf (a);
    REQUIRE (settings != nullptr);
    CHECK (settings->name == "Link " + juce::String (group));
    // The manual: "By default, the volume, sends, routing, and selection settings are activated."
    CHECK (settings->volume);
    CHECK (settings->sends);
    CHECK (settings->routing);
    CHECK (settings->selection);
    CHECK_FALSE (settings->pan);
    CHECK_FALSE (settings->inserts);
    CHECK_FALSE (settings->muteSolo);
    CHECK_FALSE (settings->automation);
    CHECK_FALSE (settings->recordMonitor);
    CHECK (studio.getLinkGroupOf (c) == nullptr);

    // Saved with the song, and back.
    LinkGroup changed = *settings;
    changed.name = "Drums";
    changed.pan = true;
    studio.setLinkGroupSettings (group, changed);
    const auto xml = studio.getProject().toXml();
    const auto loaded = Project::fromXml (*xml);
    REQUIRE (loaded.has_value());
    const auto& back = *loaded;
    REQUIRE (back.linkGroups.size() == 1);
    CHECK (back.linkGroups[0].name == "Drums");
    CHECK (back.linkGroups[0].pan);
    CHECK (back.findTrack (a)->linkGroup == group);
    CHECK (back.findTrack (b)->linkGroup == group);
    CHECK (back.findTrack (c)->linkGroup == 0);

    // A channel out of the group: with one left, the group goes.
    studio.setTrackLinkGroup (b, 0);
    CHECK (studio.getLinkGroupOf (a) == nullptr);
    CHECK (studio.getProject().linkGroups.empty());
    // Undo: the group back.
    studio.undo();
    REQUIRE (studio.getLinkGroupOf (a) != nullptr);
    CHECK (studio.getLinkGroupOf (b) == studio.getLinkGroupOf (a));
    CHECK (studio.getLinkGroupOf (a)->name == "Drums");

    // A channel added to it, then the group unlinked.
    studio.setTrackLinkGroup (c, group);
    CHECK (studio.getProject().linkGroupMembers (group).size() == 3);
    studio.removeLinkGroup (group);
    CHECK (studio.getProject().linkGroups.empty());
    CHECK (studio.getLinkGroupOf (c) == nullptr);

    // A channel linked again into a new group leaves its old one.
    const auto first = studio.linkTracks ({ a, b, c }, {});
    const auto second = studio.linkTracks ({ b, c }, {});
    CHECK (studio.getProject().findTrack (b)->linkGroup == second);
    CHECK (studio.getProject().findLinkGroup (first) == nullptr);   // (a alone: gone)
    CHECK (studio.getProject().findTrack (a)->linkGroup == 0);
    // A removed track leaves its group (and a group of one goes).
    studio.removeTrack (c);
    CHECK (studio.getProject().linkGroups.empty());
}

TEST_CASE ("Linked volume moves by the same amount (ABS: the same value); SUS and Alt suspend; Q-Link takes the selected (0.17.0)", "[link]")
{
    Studio studio (false);
    const auto a = studio.getProject().tracks[0].id, b = newTrack (studio), c = newTrack (studio);
    studio.setVolume (a, -6.0f);
    studio.setVolume (b, -12.0f);
    studio.setVolume (c, 0.0f);
    studio.linkTracks ({ a, b }, {});

    // Relative: the difference kept (the manual's default; ABS for absolute).
    studio.setVolume (a, -3.0f);
    CHECK_THAT (volumeOf (studio, a), WithinAbs (-3.0, 1e-4));
    CHECK_THAT (volumeOf (studio, b), WithinAbs (-9.0, 1e-4));
    CHECK_THAT (volumeOf (studio, c), WithinAbs (0.0, 1e-4));   // (not in the group)
    // One undo step for the whole move.
    studio.undo();
    CHECK_THAT (volumeOf (studio, a), WithinAbs (-6.0, 1e-4));
    CHECK_THAT (volumeOf (studio, b), WithinAbs (-12.0, 1e-4));
    // Dragged (not an undo step until the end), from the other channel.
    studio.setVolume (b, -11.0f, false);
    studio.setVolume (b, -10.0f, false);
    studio.setVolume (b, -10.0f, true);
    CHECK_THAT (volumeOf (studio, a), WithinAbs (-4.0, 1e-4));
    // At the top, the others stop at +6.
    studio.setVolume (a, 6.0f);
    CHECK_THAT (volumeOf (studio, b), WithinAbs (0.0, 1e-4));

    // ABS: the same value.
    studio.setLinksAbsolute (true);
    studio.setVolume (a, -20.0f);
    CHECK_THAT (volumeOf (studio, b), WithinAbs (-20.0, 1e-4));
    studio.setLinksAbsolute (false);

    // SUS (and Alt held): each on its own.
    studio.setLinksSuspended (true);
    studio.setVolume (a, -30.0f);
    CHECK_THAT (volumeOf (studio, b), WithinAbs (-20.0, 1e-4));
    studio.setLinksSuspended (false);
    auto alt = true;
    studio.isSuspendHeld = [&alt] { return alt; };
    studio.setVolume (a, -25.0f);
    CHECK_THAT (volumeOf (studio, b), WithinAbs (-20.0, 1e-4));
    alt = false;
    studio.setVolume (a, -24.0f);
    CHECK_THAT (volumeOf (studio, b), WithinAbs (-19.0, 1e-4));

    // Pan isn't linked by default; with it on, it is.
    studio.setPan (a, -0.5f);
    CHECK_THAT (studio.getProject().findTrack (b)->pan, WithinAbs (0.0, 1e-4));
    auto settings = *studio.getLinkGroupOf (a);
    settings.pan = true;
    studio.setLinkGroupSettings (settings.id, settings);
    studio.setPan (a, -0.25f);
    CHECK_THAT (studio.getProject().findTrack (b)->pan, WithinAbs (0.25, 1e-4));

    // Q-Link: whatever is touched on a selected channel, on every selected channel (here c, not linked).
    studio.selectTrack (a);   // (selects a and b: the group's selection)
    studio.toggleTrackSelected (c);
    CHECK (studio.getSelectedTracks().size() == 3);
    studio.setMute (c, true);
    CHECK_FALSE (studio.getProject().findTrack (a)->mute);
    studio.setQLink (true);
    studio.setMute (c, true);
    CHECK (studio.getProject().findTrack (a)->mute);
    CHECK (studio.getProject().findTrack (b)->mute);
    studio.setQLink (false);
    // Shift+Alt held: Q-Link for the moment (Alt alone would suspend).
    auto shiftAlt = true;
    studio.isQLinkHeld = [&shiftAlt] { return shiftAlt; };
    alt = true;
    studio.setSolo (c, true);
    CHECK (studio.getProject().findTrack (a)->solo);
    CHECK (studio.getProject().findTrack (b)->solo);
    shiftAlt = false;
    alt = false;
    studio.setSolo (c, false);
    CHECK (studio.getProject().findTrack (a)->solo);
}

TEST_CASE ("Linked selection, mute/solo, sends, routing, R/W, inserts slot by slot, monitoring (0.17.0)", "[link]")
{
    Studio studio (false);
    const auto a = studio.getProject().tracks[0].id, b = newTrack (studio), c = newTrack (studio);
    LinkGroup all;
    all.pan = all.inserts = all.muteSolo = all.automation = all.recordMonitor = true;
    const auto group = studio.linkTracks ({ a, b }, all);
    REQUIRE (group != 0);

    // Selection: one clicked selects the group; clicked again with Ctrl, the group goes.
    studio.selectTrack (b);
    CHECK (studio.getSelectedTrack() == b);
    CHECK (studio.isTrackSelected (a));
    CHECK (studio.isTrackSelected (b));
    CHECK_FALSE (studio.isTrackSelected (c));
    studio.toggleTrackSelected (c);
    studio.toggleTrackSelected (a);
    CHECK_FALSE (studio.isTrackSelected (a));
    CHECK_FALSE (studio.isTrackSelected (b));
    CHECK (studio.isTrackSelected (c));
    // Selection off: on its own.
    auto noSelection = *studio.getLinkGroupOf (a);
    noSelection.selection = false;
    studio.setLinkGroupSettings (group, noSelection);
    studio.selectTrack (a);
    CHECK_FALSE (studio.isTrackSelected (b));

    // Mute and solo.
    studio.setMute (a, true);
    CHECK (studio.getProject().findTrack (b)->mute);
    studio.setSolo (b, true);
    CHECK (studio.getProject().findTrack (a)->solo);
    studio.undo();
    CHECK_FALSE (studio.getProject().findTrack (a)->solo);
    CHECK_FALSE (studio.getProject().findTrack (b)->solo);
    CHECK (studio.getProject().findTrack (b)->mute);   // (only the solo undone: one step)

    // Routing: to a group channel together.
    const auto bus = studio.addGroupChannel();
    studio.setOutput (a, bus);
    CHECK (studio.getProject().findTrack (b)->output == bus);
    CHECK (studio.getProject().findTrack (c)->output == 0);

    // Sends: the same send, its level by the same amount, bypass.
    Insert reverb;
    reverb.plugin.name = "Reverb";
    const auto fx = studio.addFxChannel (reverb, timesEffect (1.0f, "Reverb"));
    studio.setSend (a, 0, { fx, -10.0f, true, false });
    REQUIRE (studio.getProject().findTrack (b)->sends.size() == 1);
    CHECK (studio.getProject().findTrack (b)->sends[0].target == fx);
    studio.setSendLevel (b, 0, -4.0f);
    CHECK_THAT (studio.getProject().findTrack (a)->sends[0].levelDb, WithinAbs (-4.0, 1e-4));
    studio.setSendsBypass (a, true);
    CHECK (studio.getProject().findTrack (b)->sendsBypass);
    studio.removeSend (b, 0);
    CHECK (studio.getProject().findTrack (a)->sends.empty());

    // Automation R/W.
    studio.setAutomationWrite (a, true);
    CHECK (studio.getProject().findTrack (b)->automationWrite);
    CHECK (studio.getProject().findTrack (b)->automationRead);
    studio.setAutomationRead (b, false);
    CHECK_FALSE (studio.getProject().findTrack (a)->automationRead);

    // Inserts, slot by slot: bypass and removal of the same slot.
    Insert eq;
    eq.plugin.name = "EQ";
    REQUIRE (studio.addInsert (a, eq, timesEffect (1.0f, "EQ"), 2));
    REQUIRE (studio.addInsert (b, eq, timesEffect (1.0f, "EQ"), 2));
    studio.setInsertBypass (a, 2, true);
    CHECK ((*studio.getInserts (b))[2].bypass);
    studio.removeInsert (b, 2);
    CHECK (studio.getInserts (a)->empty());
    CHECK (studio.getInserts (b)->empty());

    // Monitoring (audio tracks).
    const auto x = studio.addAudioTrack (false), y = studio.addAudioTrack (true);
    studio.linkTracks ({ x, y }, all);
    studio.setMonitor (x, true);
    CHECK (studio.getProject().findTrack (y)->monitor);
}

#include "ui/LinkGroupPage.h"
#include "ui/MixerView.h"
#include "ui/ParagonLookAndFeel.h"

TEST_CASE ("The Link Group Settings page and the mixer's LINK key (0.17.0)", "[link][ui]")
{
    Studio studio (false);
    const auto a = studio.getProject().tracks[0].id, b = newTrack (studio), c = newTrack (studio);
    MixerView mixer (studio);
    mixer.setSize (1200, 600);
    studio.selectTrack (a);
    studio.dispatchPendingMessages();
    CHECK_FALSE (mixer.getLinkKey().isEnabled());   // (one channel: nothing to link)
    studio.toggleTrackSelected (c);
    studio.dispatchPendingMessages();
    CHECK (mixer.getLinkKey().isEnabled());
    CHECK (mixer.getLinkKey().getButtonText() == "LINK");

    // The page: the manual's defaults ticked; a name and pan, then LINK.
    LinkGroupPage page (studio, 0, studio.getSelectedTracks());
    CHECK (page.getSettings().volume);
    CHECK (page.getSettings().selection);
    CHECK_FALSE (page.getSettings().pan);
    CHECK (page.okKey.getButtonText() == "LINK");
    page.name.setText ("Backing Vocals", false);
    page.setLinks (LinkGroup::What::pan, true);
    page.apply();
    const auto* group = studio.getLinkGroupOf (a);
    REQUIRE (group != nullptr);
    CHECK (group->name == "Backing Vocals");
    CHECK (group->pan);
    CHECK (studio.getProject().findTrack (c)->linkGroup == group->id);
    CHECK (studio.getProject().findTrack (b)->linkGroup == 0);
    studio.dispatchPendingMessages();
    CHECK (mixer.getLinkKey().getButtonText() == "UNLINK");

    // Its settings changed (OK).
    LinkGroupPage edit (studio, group->id, {});
    CHECK (edit.okKey.getButtonText() == "OK");
    CHECK (edit.name.getText() == "Backing Vocals");
    edit.setLinks (LinkGroup::What::muteSolo, true);
    edit.apply();
    CHECK (studio.getLinkGroupOf (a)->muteSolo);
    CHECK (studio.getProject().linkGroups.size() == 1);

    // LINK again with the group selected: unlinked.
    studio.selectTrack (a);   // (selects the group)
    CHECK (LinkGroupPage::linkOrUnlinkSelected (mixer, studio));
    CHECK (studio.getProject().linkGroups.empty());
    studio.dispatchPendingMessages();
    CHECK (mixer.getLinkKey().getButtonText() == "LINK");
}
