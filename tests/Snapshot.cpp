#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/MainView.h"
#include "ui/ParagonLookAndFeel.h"

// Hidden test (the "[.]" tag): renders the studio's window to a PNG, with a
// short demo song in it and the key editor open, for reviewing the UI. Run with:
//   PARAGON_SNAPSHOT=studio.png ./ParagonPR7STests "[snapshot]"
// PARAGON_SNAPSHOT_WIDTH sets the width (default 1500; 3000 for 2x).
// PARAGON_SNAPSHOT_FINISH=black or =neon picks the finish.
TEST_CASE ("Render the studio to a PNG", "[.][snapshot]")
{
    using namespace pr7s;
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "studio.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);

    // The app's look, as Main.cpp sets it.
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    struct Reset { ~Reset() { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); } } reset;

    Studio studio (false);
    // A demo: drums-ish, bass and keys, a cycle over bars 5 to 8, playing in bar 6.
    const auto keys = studio.getProject().tracks[0].id;
    studio.renameTrack (keys, "Keys");
    const auto bass = studio.addTrack ({});
    studio.renameTrack (bass, "Bass");
    const auto lead = studio.addTrack ({});
    studio.renameTrack (lead, "Lead");
    for (int bar = 0; bar < 8; bar += 2)
    {
        Part chords { bar * 4.0, 8.0, "Keys", {} };
        for (int beat = 0; beat < 8; beat += 2)
            for (int pitch : { 60, 64, 67 })
                chords.notes.push_back ({ (double) beat, 1.5, pitch + (bar % 4 == 2 ? 2 : 0), 0.7f });
        studio.addPart (keys, chords);
        Part line { bar * 4.0, 8.0, "Bass", {} };
        for (int eighth = 0; eighth < 16; ++eighth)
            line.notes.push_back ({ eighth * 0.5, 0.4, 36 + (eighth % 4 == 3 ? 7 : 0), 0.9f });
        studio.addPart (bass, line);
    }
    Part solo { 16.0, 16.0, "Lead", {} };
    for (int i = 0; i < 24; ++i)
        solo.notes.push_back ({ i * 0.6, 0.5, 72 + (i * 5) % 12, 0.45f + 0.5f * (float) ((i * 7) % 10) / 9.0f });
    studio.addPart (lead, solo);
    studio.setCycle (16.0, 32.0);
    studio.setCycleOn (true);
    studio.setClick (true);
    studio.selectTrack (bass);
    studio.setPosition (21.0);

    MainView view (studio);
    const auto width = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_WIDTH", "1500").getIntValue();
    const auto scale = (float) width / 1500.0f;
    view.setSize (1500, 900);
    // Play a little so the meters show.
    studio.play();
    for (int round = 0; round < 6; ++round)
    {
        renderSamples (studio.getEngine(), 4096);
        juce::Timer::callPendingTimersSynchronously();
        juce::Thread::sleep (30);
    }
    studio.stop();
    studio.getEngine().setPosition (21.0);
    // The key editor open on the lead part, a few of its notes selected.
    view.getArrangeView().select (lead, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    view.getKeyEditor().selectNotes ({ 3, 4, 5 });

    const auto image = view.createComponentSnapshot (view.getLocalBounds(), true, scale);
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (path);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    REQUIRE (stream.openedOk());
    REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));
}

// Hidden test: the key editor in its own window, stretched wide as across
// three screens (PARAGON_SNAPSHOT_SIZE, default 5760x1000), to a PNG.
TEST_CASE ("Render the key editor's own window to a PNG", "[.][snapshot-window]")
{
    using namespace pr7s;
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "key-editor.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    struct Reset { ~Reset() { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); } } reset;

    Studio studio (false);
    const auto track = studio.getProject().tracks[0].id;
    studio.renameTrack (track, "Clean Guitar");
    Part riff { 0.0, 64.0, "Clean Guitar", {} };
    for (int i = 0; i < 200; ++i)
        riff.notes.push_back ({ i * 0.3125, i % 5 == 0 ? 1.0 : 0.25, 52 + (i * 7) % 19, 0.4f + 0.6f * (float) ((i * 3) % 10) / 9.0f });
    studio.addPart (track, riff);
    studio.setPosition (21.0);

    MainView view (studio);
    view.setSize (1500, 900);
    view.getArrangeView().select (track, 0);
    view.openKeyEditor();
    view.setKeyEditorInWindow (true);
    studio.dispatchPendingMessages();
    const auto size = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_SIZE", "5760x1000"), "x", {});
    auto* window = view.getKeyEditorWindow();
    REQUIRE (window != nullptr);
    window->setSize (size[0].getIntValue(), size[1].getIntValue());
    auto& editor = view.getKeyEditor();
    editor.fitToPart();
    editor.selectNotes ({ 10, 11, 12 });

    const auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.0f);
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (path);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    REQUIRE (stream.openedOk());
    REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));
}
