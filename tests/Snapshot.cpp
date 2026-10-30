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
// PARAGON_SNAPSHOT_ZONE=mixer shows the mixer in the lower zone, =none nothing there (the key editor otherwise).
// PARAGON_SNAPSHOT_EXPORT=1 draws the export page over it.
TEST_CASE ("Render the studio to a PNG", "[.][snapshot]")
{
    using namespace pr7s;
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "studio.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);

    // The app's look, as Main.cpp sets it (PARAGON_SNAPSHOT_FONT=inter or
    // =titles to compare the other font choices; Bebas Neue everywhere otherwise).
    const auto fontChoice = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FONT", {});
    ParagonLookAndFeel::setDisplayFont (fontChoice == "titles" ? ParagonLookAndFeel::DisplayFont::bebasTitles
                                        : fontChoice == "inter" ? ParagonLookAndFeel::DisplayFont::inter
                                                                : ParagonLookAndFeel::DisplayFont::bebasEverywhere);
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
    // A guitar: an audio track with a recorded take (a plucked-string stand-in), faded in and out.
    const auto guitar = studio.addAudioTrack (false);
    studio.renameTrack (guitar, "Guitar");
    writeWav (studio.getAudioFolder().getChildFile ("Guitar 01.wav"), pluckedTake (12.0, 48000.0, 7), 48000.0);
    Part take { 4.0, 20.0, "Guitar", {} };
    take.audioFile = "Guitar 01.wav";
    take.audioOffset = 1.0;
    take.fadeIn = 0.4;
    take.fadeOut = 1.5;
    studio.addPart (guitar, take);
    studio.setMonitor (guitar, true);
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
    // The key editor open on the lead part, a few of its notes selected;
    // or the mixer, with some effects in (stand-ins named after the family's
    // plugins: the tests can't load real VST3s).
    view.getArrangeView().select (lead, 0);
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_ZONE", {}) == "mixer")
    {
        const auto add = [&studio] (int busId, const juce::String& name, bool bypass = false)
        {
            Insert insert;
            insert.plugin.name = name;
            studio.addInsert (busId, insert, timesEffect (1.0f, name));
            if (bypass)
                studio.setInsertBypass (busId, (int) studio.getInserts (busId)->size() - 1, true);
        };
        add (keys, "Paragon PR7-C");
        add (bass, "Paragon PR7-B");
        add (bass, "Paragon PR7-C", true);
        add (lead, "Paragon PR7-G");
        add (Engine::masterBus, "Paragon PR7-C");
        studio.setVolume (keys, -4.5f);
        studio.setPan (keys, -0.3f);
        studio.setVolume (lead, -8.0f);
        studio.setPan (lead, 0.25f);
        studio.setMasterVolume (-1.5f);
        view.openMixer();
        studio.play();
        for (int round = 0; round < 4; ++round)
        {
            renderSamples (studio.getEngine(), 4096);
            juce::Timer::callPendingTimersSynchronously();
            juce::Thread::sleep (30);
        }
        studio.stop();
    }
    else if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_ZONE", {}) != "none")   // none: the lower zone closed
    {
        view.openKeyEditor();
        studio.dispatchPendingMessages();
        view.getKeyEditor().selectNotes ({ 3, 4, 5 });
    }
    studio.dispatchPendingMessages();

    auto image = view.createComponentSnapshot (view.getLocalBounds(), true, scale);
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_EXPORT", {}).isNotEmpty())
    {
        // The export page over the studio, as its dialog shows it (the title bar is the system's).
        Windows::ExportPage page (studio);
        const auto pageImage = page.createComponentSnapshot (page.getLocalBounds(), true, scale);
        juce::Graphics g (image);
        g.fillAll (juce::Colours::black.withAlpha (0.45f));
        const auto at = image.getBounds().withSizeKeepingCentre (pageImage.getWidth(), pageImage.getHeight());
        g.drawImageAt (pageImage, at.getX(), at.getY());
        g.setColour (juce::Colour (0xff3a3540));
        g.drawRect (at.expanded (1), 1);
    }
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (path);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    REQUIRE (stream.openedOk());
    REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));
}

// Hidden test: the key editor in its own window, stretched wide as across
// three screens (PARAGON_SNAPSHOT_SIZE, default 5760x1000), to a PNG.
// PARAGON_SNAPSHOT_ZONE=mixer: the mixer's own window (F3) instead, with a
// song of eight tracks (default size 1920x1000: one screen).
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

    const auto mixerWindow = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_ZONE", {}) == "mixer";
    if (mixerWindow)
    {
        // A song's worth of channels, with effects in some (stand-ins named after the family's plugins).
        const char* names[] = { "Drums", "Bass", "Rhythm L", "Rhythm R", "Lead Gtr", "Keys", "Pad", "Vocal" };
        const auto add = [&studio] (int busId, const juce::String& name)
        {
            Insert insert;
            insert.plugin.name = name;
            studio.addInsert (busId, insert, timesEffect (1.0f, name));
        };
        for (int i = 1; i < 8; ++i)
            studio.addTrack ({});
        for (int i = 0; i < 8; ++i)
        {
            const auto id = studio.getProject().tracks[(size_t) i].id;
            studio.renameTrack (id, names[i]);
            studio.setVolume (id, -3.0f - (float) ((i * 5) % 9));
            studio.setPan (id, i == 2 ? -0.8f : i == 3 ? 0.8f : (float) ((i % 3) - 1) * 0.2f);
            if (i >= 2 && i <= 4)
                add (id, "Paragon PR7-G");
            if (i == 1)
                add (id, "Paragon PR7-B");
            if (i == 7)
                add (id, "Paragon PR7-V");
            if (i % 2 == 0)
                add (id, "Paragon PR7-C");
        }
        add (Engine::masterBus, "Paragon PR7-C");
        studio.setMasterVolume (-1.0f);
    }

    MainView view (studio);
    view.setSize (1500, 900);
    juce::Component* shown = nullptr;
    const auto size = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_SIZE", mixerWindow ? "1920x1000" : "5760x1000"), "x", {});
    if (mixerWindow)
    {
        view.toggleMixerWindow();   // F3
        studio.selectTrack (studio.getProject().tracks[4].id);
        studio.dispatchPendingMessages();
        auto* window = view.getMixerWindow();
        REQUIRE (window != nullptr);
        window->setSize (size[0].getIntValue(), size[1].getIntValue());
        // Some level on the meters.
        studio.play();
        for (int round = 0; round < 4; ++round)
        {
            renderSamples (studio.getEngine(), 4096);
            juce::Timer::callPendingTimersSynchronously();
            juce::Thread::sleep (30);
        }
        studio.stop();
        shown = &view.getMixer();
    }
    else
    {
        view.getArrangeView().select (track, 0);
        view.openKeyEditor();
        view.setKeyEditorInWindow (true);
        studio.dispatchPendingMessages();
        auto* window = view.getKeyEditorWindow();
        REQUIRE (window != nullptr);
        window->setSize (size[0].getIntValue(), size[1].getIntValue());
        auto& editor = view.getKeyEditor();
        editor.fitToPart();
        editor.selectNotes ({ 10, 11, 12 });
        shown = &editor;
    }

    const auto image = shown->createComponentSnapshot (shown->getLocalBounds(), true, 1.0f);
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (path);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    REQUIRE (stream.openedOk());
    REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));
}
