// The Edit History (0.83.0, the manual's Edit History Dialog): every edit named and timed, the
// separator dragged up to undo and down to redo, details typed, the maximum steps.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/EditHistory.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/HistoryPage.h"
#include "ui/MainView.h"
#include "ui/ParagonLookAndFeel.h"

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    juce::MouseEvent mouseAt (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt, juce::ModifierKeys mods = {}, int clicks = 1)
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                                 mods.withFlags (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, now, downAt, now, clicks, at != downAt);
    }

    Part partAt (double start, const juce::String& name)
    {
        Part part;
        part.start = start;
        part.length = 4.0;
        part.name = name;
        part.notes.push_back ({ 0.0, 1.0, 60, 0.8f });
        return part;
    }
}

TEST_CASE ("0.83.0: each edit is named from what changed", "[history]")
{
    Project before;
    Track track;
    track.id = 1;
    track.name = "Bass";
    track.parts.push_back (partAt (0.0, "Verse riff"));
    track.parts.back().id = 7;
    before.tracks.push_back (track);

    auto after = before;
    after.tracks[0].parts[0].start = 4.0;
    auto step = describeEdit (before, after);
    CHECK (step.action == "Move Part");
    CHECK (step.details == "Bass: Verse riff");

    after = before;
    after.tracks[0].volumeDb = -3.0f;
    CHECK (describeEdit (before, after).action == "Volume");

    after = before;
    after.tracks[0].parts[0].notes[0].pitch = 62;
    CHECK (describeEdit (before, after).action == "Edit Notes");

    after = before;
    after.tracks[0].parts.clear();
    CHECK (describeEdit (before, after).action == "Delete Part");

    after = before;
    after.bpm = 140.0;
    step = describeEdit (before, after);
    CHECK (step.action == "Tempo");
    CHECK (step.details == "140.00 BPM");

    // Two kinds at once: the first, and the rest named after.
    after = before;
    after.tracks[0].parts[0].start = 4.0;
    after.tracks[0].mute = true;
    step = describeEdit (before, after);
    CHECK (step.action == "Mute");
    CHECK (step.details.contains ("and move part"));
}

TEST_CASE ("0.83.0: the history follows undo, goes to any step, and keeps the steps undone until a new edit", "[history]")
{
    Studio studio (false);
    const auto trackId = studio.getProject().tracks.front().id;
    const auto start = (int) studio.getHistory().size();
    studio.addPart (trackId, partAt (0.0, "One"));
    studio.setTempo (100.0);
    studio.setTempo (110.0);
    studio.setVolume (trackId, -6.0f);
    const auto& steps = studio.getHistory();
    REQUIRE ((int) steps.size() == start + 4);
    CHECK (studio.getHistoryPosition() == (int) steps.size() - 1);
    CHECK (steps.back().action == "Volume");
    CHECK (steps[steps.size() - 2].action == "Tempo");
    CHECK (steps[steps.size() - 2].details == "110.00 BPM");
    CHECK (steps[(size_t) start].action == "Add Part");

    // Back to before the tempo changes: both undone at once, the part and the step list kept.
    const auto partStep = start;
    studio.goToHistory (partStep);
    CHECK (studio.getHistoryPosition() == partStep);
    CHECK_THAT (studio.getProject().bpm, WithinAbs (120.0, 1.0e-9));
    CHECK_THAT (studio.getProject().tracks.front().volumeDb, WithinAbs (0.0, 1.0e-6));
    CHECK (studio.getProject().tracks.front().parts.size() == 1);
    CHECK ((int) steps.size() == start + 4);

    // Down again to the second tempo: redone that far.
    studio.goToHistory (partStep + 2);
    CHECK_THAT (studio.getProject().bpm, WithinAbs (110.0, 1.0e-9));
    CHECK_THAT (studio.getProject().tracks.front().volumeDb, WithinAbs (0.0, 1.0e-6));

    // Undo and redo move the same separator.
    studio.undo();
    CHECK (studio.getHistoryPosition() == partStep + 1);
    studio.redo();
    CHECK (studio.getHistoryPosition() == partStep + 2);

    // A new edit replaces the undone steps.
    studio.setTempo (90.0);
    CHECK ((int) steps.size() == start + 4);
    CHECK (steps.back().details == "90.00 BPM");
    CHECK_FALSE (studio.canRedo());

    // Details: anything typed.
    studio.setHistoryDetails (start, "the good take");
    CHECK (steps[(size_t) start].details == "the good take");

    // Maximum steps: the oldest go, the song stays where it is.
    studio.setMaxUndoSteps (2);
    CHECK ((int) steps.size() == 3);
    CHECK (studio.getHistoryPosition() == 2);
    CHECK (steps.back().details == "90.00 BPM");
    for (int i = 0; i < 5; ++i)
        studio.setTempo (80.0 + i);
    CHECK ((int) steps.size() == 3);
    studio.setMaxUndoSteps (100);
}

TEST_CASE ("0.83.0: the Edit History window: rows, the separator dragged up and down, keys, details", "[history][ui]")
{
    Studio studio (false);
    const auto trackId = studio.getProject().tracks.front().id;
    for (int i = 0; i < 4; ++i)
        studio.setTempo (100.0 + 10.0 * i);
    studio.setVolume (trackId, -3.0f);
    HistoryPage page (studio);
    page.setSize (HistoryPage::preferredWidth, HistoryPage::preferredHeight);
    const auto rows = page.getNumRows();
    REQUIRE (rows >= 5);
    CHECK (page.getSeparatorRow() == rows - 1);
    CHECK (page.getCellText (rows - 1, 0) == "Volume");
    CHECK (page.getCellText (rows - 1, 2) == "DONE");
    CHECK (page.getCellText (0, 2) == "START");

    // Drag the bar up two rows: two steps undone, as it moves.
    auto* list = page.getChildComponent (0)->getChildComponent (0)->getChildComponent (0);   // viewport > holder > list
    REQUIRE (list != nullptr);
    const auto bar = page.getSeparatorBounds();
    const auto inList = [&] (juce::Point<int> p) { return list->getLocalPoint (&page, p).toFloat(); };
    const auto down = inList (bar.getCentre());
    list->mouseDown (mouseAt (*list, down, down));
    const auto upTwo = inList (bar.getCentre().translated (0, -2 * HistoryPage::rowHeight));
    list->mouseDrag (mouseAt (*list, upTwo, down));
    list->mouseUp (mouseAt (*list, upTwo, down));
    CHECK (page.getSeparatorRow() == rows - 3);
    CHECK_THAT (studio.getProject().bpm, WithinAbs (120.0, 1.0e-9));
    CHECK (page.getCellText (rows - 1, 2) == "UNDONE");
    CHECK (page.getCellText (rows - 2, 2) == "UNDONE");

    // And down one: redone.
    const auto bar2 = page.getSeparatorBounds();
    const auto down2 = inList (bar2.getCentre());
    const auto downOne = inList (bar2.getCentre().translated (0, HistoryPage::rowHeight));
    list->mouseDown (mouseAt (*list, down2, down2));
    list->mouseDrag (mouseAt (*list, downOne, down2));
    list->mouseUp (mouseAt (*list, downOne, down2));
    CHECK (page.getSeparatorRow() == rows - 2);
    CHECK_THAT (studio.getProject().bpm, WithinAbs (130.0, 1.0e-9));

    // Up / Down keys move it a step; it never goes above the start.
    CHECK (page.keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
    CHECK (page.getSeparatorRow() == rows - 1);
    for (int i = 0; i < rows + 3; ++i)
        page.keyPressed (juce::KeyPress (juce::KeyPress::upKey));
    CHECK (page.getSeparatorRow() == 0);
    page.moveSeparatorTo (rows - 1);
    CHECK_THAT (studio.getProject().tracks.front().volumeDb, WithinAbs (-3.0, 1.0e-6));

    page.setDetails (1, "slower");
    CHECK (page.getCellText (1, 3) == "slower");

    // Drawn in every finish.
    for (const auto type : { Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon, Finish::Type::pearlWhite })
    {
        Finish::set (type, false);
        CHECK (page.createComponentSnapshot (page.getLocalBounds()).isValid());
    }
}

TEST_CASE ("0.83.0: a right-click on UNDO opens the Edit History without undoing", "[history][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    studio.setTempo (100.0);
    studio.dispatchPendingMessages();
    const auto position = studio.getHistoryPosition();
    view.toggleEditHistory();
    CHECK (view.getHistoryWindow() != nullptr);
    CHECK (studio.getHistoryPosition() == position);
    view.toggleEditHistory();
    CHECK (view.getHistoryWindow() == nullptr);
}

TEST_CASE ("0.83.0: the Edit History snapshot", "[.snapshot-history]")
{
    const auto folder = juce::File (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_DIR", juce::File::getCurrentWorkingDirectory().getFullPathName()));
    // The app's look, as Main.cpp sets it (Bebas Neue everywhere).
    ParagonLookAndFeel::setDisplayFont (ParagonLookAndFeel::DisplayFont::bebasEverywhere);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    struct Reset { ~Reset() { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); } } reset;
    Studio studio (false);
    const auto trackId = studio.getProject().tracks.front().id;
    studio.renameTrack (trackId, "Bass");
    studio.addPart (trackId, partAt (0.0, "Verse riff"));
    studio.addPart (trackId, partAt (8.0, "Chorus riff"));
    studio.setVolume (trackId, -4.5f);
    studio.setTempo (128.0);
    studio.setPan (trackId, 0.2f);
    studio.setTempo (132.0);
    studio.setVolume (trackId, -2.0f);
    studio.goToHistory (studio.getHistoryPosition() - 2);
    for (const auto& [type, name] : { std::pair { Finish::Type::pearlBlack, "black" }, { Finish::Type::pearlWhiteNeon, "neon" }, { Finish::Type::pearlWhite, "white" } })
    {
        Finish::set (type, false);
        HistoryPage page (studio);
        page.setSize (HistoryPage::preferredWidth, 360);
        page.setDetails (2, "the good one");
        const auto image = page.createComponentSnapshot (page.getLocalBounds(), true, 2.0f);
        juce::PNGImageFormat png;
        auto file = folder.getChildFile (juce::String ("history-") + name + ".png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        CHECK (png.writeImageToStream (image, out));
    }
}
