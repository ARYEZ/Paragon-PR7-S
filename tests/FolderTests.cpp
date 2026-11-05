// Folder tracks (0.50.0, the manual's Folder Tracks): holding tracks, nesting, folding, mute and
// solo for all of them, the group channel summing them, removing, saving, undo, the screen.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/ArrangeView.h"
#include "ui/Finish.h"
#include "ui/MainView.h"

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    std::vector<int> order (const Studio& studio)
    {
        std::vector<int> ids;
        for (const auto& t : studio.getProject().tracks)
            ids.push_back (t.id);
        return ids;
    }

    juce::MouseEvent mouseAt (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt, juce::ModifierKeys mods = {})
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                                 mods.withFlags (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, now, downAt, now, 1, at != downAt);
    }
}

TEST_CASE ("0.50.0: folders hold tracks: moving in and out, nesting, order, folding, removing, saving, undo", "[folders]")
{
    Studio studio (false);
    const auto a = studio.getProject().tracks[0].id;
    const auto b = studio.addTrack ({});
    const auto c = studio.addTrack ({});
    const auto d = studio.addTrack ({});
    const auto& project = studio.getProject();

    // Move Selected Tracks to New Folder: the folder where the first was, the tracks right after it.
    const auto folder = studio.moveTracksToNewFolder ({ b, d }, false);
    REQUIRE (folder != 0);
    CHECK (project.findTrack (folder)->isFolder());
    CHECK (project.findTrack (b)->folder == folder);
    CHECK (project.findTrack (d)->folder == folder);
    CHECK (order (studio) == std::vector<int> { a, folder, b, d, c });
    CHECK (project.folderMembers (folder, false) == std::vector<int> { b, d });

    // Nested: a folder in the folder, with c moved into it (it goes to the end of its folder).
    const auto inner = studio.moveTracksToNewFolder ({ d }, false);
    CHECK (project.findTrack (inner)->folder == folder);
    CHECK (studio.setTracksFolder ({ c }, inner));
    CHECK (order (studio) == std::vector<int> { a, folder, b, inner, d, c });
    CHECK (project.isInFolder (c, folder));
    CHECK (project.folderDepth (c) == 2);
    // Never into itself or its own folder inside it.
    CHECK_FALSE (studio.setTracksFolder ({ folder }, inner));

    // Folded: its tracks aren't shown (they still play), opened again: back.
    studio.setFolderOpen (folder, false);
    CHECK (studio.getShownTracks() == std::vector<int> { a, folder });
    studio.setFolderOpen (folder, true);
    CHECK (studio.getShownTracks().size() == 6);

    // Mute and solo on the folder: every track in it.
    studio.setMute (folder, true);
    CHECK (project.findTrack (b)->mute);
    CHECK (project.findTrack (c)->mute);
    CHECK_FALSE (project.findTrack (a)->mute);
    studio.setMute (folder, false);
    studio.setSolo (folder, true);
    CHECK (project.findTrack (d)->solo);
    CHECK_FALSE (project.findTrack (folder)->solo);   // (its own isn't kept: it would silence the FX channels)
    CHECK (studio.isFolderSoloed (folder));
    studio.setSolo (folder, false);
    CHECK_FALSE (studio.isFolderSoloed (folder));

    // Dragged (the track list's move): under an open folder's row: into it; to the top: out.
    studio.moveTracks ({ a }, 2);   // (before b, after the folder's row)
    CHECK (project.findTrack (a)->folder == folder);
    studio.moveTracks ({ a }, 0);
    CHECK (project.findTrack (a)->folder == 0);
    CHECK (order (studio).front() == a);

    // Saved and opened again.
    const auto opened = Project::fromXml (*project.toXml());
    REQUIRE (opened);
    CHECK (opened->findTrack (folder)->isFolder());
    CHECK (opened->findTrack (c)->folder == inner);
    CHECK (opened->findTrack (inner)->folder == folder);

    // Undo puts the membership back.
    studio.setTracksFolder ({ b }, 0);
    CHECK (project.findTrack (b)->folder == 0);
    studio.undo();
    CHECK (project.findTrack (b)->folder == folder);

    // Removing a folder keeps its tracks (in the folder it was in).
    studio.removeTrack (inner);
    CHECK (project.findTrack (c)->folder == folder);
    CHECK (project.findTrack (d)->folder == folder);

    // A loop read from a file is undone (a folder can't be in itself).
    auto broken = project;
    broken.findTrack (folder)->folder = b;   // (b isn't a folder: dropped)
    broken.tidyFolders();
    CHECK (broken.findTrack (folder)->folder == 0);
}

TEST_CASE ("0.50.0: a folder's group channel sums its tracks; they leave it when they leave the folder", "[folders][engine]")
{
    Studio studio (false);
    const auto a = studio.getProject().tracks[0].id;
    const auto b = studio.addTrack ({});
    const auto outside = studio.addTrack ({});
    auto& engine = studio.getEngine();
    engine.setInstrument (a, std::make_unique<SteadyTone> (0.25f, 0.25f));
    engine.setInstrument (b, std::make_unique<SteadyTone> (0.25f, 0.25f));
    engine.setInstrument (outside, std::make_unique<SteadyTone> (0.1f, 0.1f));
    const auto folder = studio.moveTracksToNewFolder ({ a, b }, true);
    const auto& project = studio.getProject();
    CHECK (project.findTrack (folder)->isGroup());
    CHECK (project.findTrack (a)->output == folder);
    CHECK (project.findTrack (b)->output == folder);
    CHECK (project.findTrack (outside)->output == 0);

    const auto level = [&]
    {
        std::vector<float> right;
        const auto left = renderSamples (engine, 8192, 512, &right);
        return left.back();
    };
    CHECK_THAT (level(), WithinAbs (0.6, 1.0e-4));   // 0.25 + 0.25 through the folder, 0.1 beside it
    // The folder's fader: both of its tracks at once.
    studio.setVolume (folder, -6.0206f);
    CHECK_THAT (level(), WithinAbs (0.35, 1.0e-3));
    studio.setVolume (folder, 0.0f);
    // Its group channel off: back to the master, the same level (no fader between).
    studio.setFolderGroup (folder, false);
    CHECK (project.findTrack (a)->output == 0);
    CHECK_FALSE (project.findTrack (folder)->isGroup());
    CHECK_THAT (level(), WithinAbs (0.6, 1.0e-4));
    // On again, then b leaves: it goes to the master.
    studio.setFolderGroup (folder, true);
    CHECK (project.findTrack (b)->output == folder);
    studio.setTracksFolder ({ b }, 0);
    CHECK (project.findTrack (b)->output == 0);
    // Moved in again: into its group.
    studio.setTracksFolder ({ b }, folder);
    CHECK (project.findTrack (b)->output == folder);
}

TEST_CASE ("0.50.0: folders on screen: rows indented, folding, the folder's blocks select and move its parts", "[folders][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto a = studio.getProject().tracks[0].id;
    const auto b = studio.addTrack ({});
    Part part;
    part.start = 4.0;
    part.length = 4.0;
    part.notes = { { 0.0, 1.0, 60, 0.8f } };
    studio.addPart (a, part);
    part.start = 6.0;
    studio.addPart (b, part);
    const auto folder = studio.moveTracksToNewFolder ({ a, b }, false);
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto& timeline = view.getTimeline();

    // One block over both parts (4 to 10).
    const auto blocks = arrange.folderBlocks (folder);
    REQUIRE (blocks.size() == 1);
    CHECK_THAT (blocks[0].first, WithinAbs (4.0, 1.0e-9));
    CHECK_THAT (blocks[0].second, WithinAbs (10.0, 1.0e-9));

    // A click on it selects both parts; a drag moves both, a bar on.
    float folderY = 0.0f;
    for (const auto& row : timeline.layout (studio))
        if (row.kind == Timeline::Row::Kind::track && row.busId == folder)
            folderY = (float) (Timeline::rulerHeight + row.top - timeline.scrollY + row.height / 2);
    REQUIRE (folderY > 0.0f);
    const juce::Point<float> from (timeline.xForBeat (5.0), folderY), to (timeline.xForBeat (9.0), folderY);
    arrange.mouseDown (mouseAt (arrange, from, from));
    CHECK (arrange.getSelectedParts().size() == 2);
    for (int step = 1; step <= 4; ++step)
        arrange.mouseDrag (mouseAt (arrange, from + (to - from) * ((float) step / 4.0f), from));
    arrange.mouseUp (mouseAt (arrange, to, from));
    CHECK_THAT (studio.getProject().findTrack (a)->parts[0].start, WithinAbs (8.0, 1.0e-9));
    CHECK_THAT (studio.getProject().findTrack (b)->parts[0].start, WithinAbs (10.0, 1.0e-9));

    // Folded: the tracks' rows go (the folder's stays, its blocks drawn on it).
    const auto rowsOpen = timeline.layout (studio).size();
    studio.setFolderOpen (folder, false);
    studio.dispatchPendingMessages();
    CHECK (timeline.layout (studio).size() == rowsOpen - 2);

    for (const auto type : { Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon, Finish::Type::pearlWhite })
    {
        Finish::set (type, false);
        CHECK (view.createComponentSnapshot (view.getLocalBounds()).isValid());
    }
}
