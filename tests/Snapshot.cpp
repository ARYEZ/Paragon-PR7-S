#include <catch2/catch_test_macros.hpp>

#include "DeviceTests.h"
#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/MainView.h"
#include "ui/ToolIcons.h"
#include "ui/Windows.h"
#include "ui/ParagonLookAndFeel.h"

// Hidden test (the "[.]" tag): renders the studio's window to a PNG, with a
// short demo song in it and the key editor open, for reviewing the UI. Run with:
//   PARAGON_SNAPSHOT=studio.png ./ParagonPR7STests "[snapshot]"
// PARAGON_SNAPSHOT_WIDTH sets the width (default 1500; 3000 for 2x).
// PARAGON_SNAPSHOT_FINISH=black or =neon picks the finish.
// PARAGON_SNAPSHOT_ZONE=mixer shows the mixer in the lower zone (with an FX channel the tracks send to;
// PARAGON_SNAPSHOT_SENDS=1 shows the sends rack; PARAGON_SNAPSHOT_RACK=none hides the rack, the mixer as short
// as it goes), =none nothing there (the key editor otherwise).
// PARAGON_SNAPSHOT_EXPORT=1 draws the export page over it. PARAGON_SNAPSHOT_EDIT=1: the SPLIT tool, a muted part.
// PARAGON_SNAPSHOT_RANGE=1: the RANGE tool and a range over two tracks. PARAGON_SNAPSHOT_RENDER=1: the RENDER IN PLACE page.
// PARAGON_SNAPSHOT_VIEW=5760x1000: the window that size, 1:1 (the wide layout, spanning three screens).
// PARAGON_SNAPSHOT_ROWS=0.5 (to 3) the arrangement's rows shorter or taller; PARAGON_SNAPSHOT_SUSPENDED=1 its
// Auto-Scroll suspended.
// PARAGON_SNAPSHOT_QC=1: the Inspector's Quick Controls, three assigned.
// PARAGON_SNAPSHOT_MULTI=1: parts selected on two tracks.
// PARAGON_SNAPSHOT_FREEZE=1: the lead frozen (FREEZE lit in its Inspector, its parts stamped).
// PARAGON_SNAPSHOT_VISIBILITY=1: the VISIBILITY tab, the bass hidden, the keys disabled.
// PARAGON_SNAPSHOT_INSPECTOR=fader folds the Inspector's inserts and sends (=none hides it).
// PARAGON_SNAPSHOT_AUTOMATION=1 opens automation lanes (the lead's volume, the bass's pan, the master's fade).
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
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_AUTOMATION", {}).isNotEmpty())
    {
        // Automation: the lead's volume swelling in and dipping, the bass's pan
        // lane open with no points yet (the dotted line; W on), the master
        // fading out at the end.
        studio.setAutomationOpen (lead, true);
        studio.setAutomationPoints (lead, 0, { { 16.0, volumeToTravel (-30.0f) }, { 20.0, volumeToTravel (0.0f) }, { 24.0, volumeToTravel (-2.0f) },
                                               { 26.0, volumeToTravel (-12.0f) }, { 28.0, volumeToTravel (0.0f) } });
        studio.setAutomationOpen (bass, true);
        studio.setLaneControl (bass, 0, studio.getAutomationControls (bass)[1]);
        studio.setAutomationWrite (bass, true);
        studio.setAutomationOpen (Engine::masterBus, true);
        studio.setAutomationPoints (Engine::masterBus, 0, { { 28.0, volumeToTravel (0.0f) }, { 32.0, 0.0f } });
    }

    MainView view (studio);
    const auto width = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_WIDTH", "1500").getIntValue();
    auto scale = (float) width / 1500.0f;
    view.setSize (1500, 900);
    // PARAGON_SNAPSHOT_VIEW=5760x1000: the window that size (three screens: the wide layout), drawn 1:1.
    if (const auto size = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_VIEW", {}), "x", {});
        size.size() == 2)
    {
        view.setSize (size[0].getIntValue(), size[1].getIntValue());
        scale = 1.0f;
    }
    // PARAGON_SNAPSHOT_INSPECTOR=fader: the Inspector's inserts and sends folded (its fader shows); =none: hidden.
    if (const auto inspector = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_INSPECTOR", {}); inspector == "fader")
    {
        view.getInspector().setFolded (Inspector::Section::inserts, true);
        view.getInspector().setFolded (Inspector::Section::sends, true);
    }
    else if (inspector == "none")
        view.toggleInspector();
    // PARAGON_SNAPSHOT_MULTI=1: the keys' and the bass's parts in bars 5 to 8 selected together.
    // PARAGON_SNAPSHOT_QC=1: the lead's Quick Controls (volume, pan, a send-less mix: mute), the other sections folded.
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_QC", {}).isNotEmpty())
    {
        const auto controls = studio.getAutomationControls (lead);
        for (int i = 0; i < juce::jmin (3, (int) controls.size()); ++i)
            studio.setQuickControl (lead, i, controls[(size_t) i]);
        for (const auto section : { Inspector::Section::routing, Inspector::Section::inserts, Inspector::Section::sends, Inspector::Section::fader })
            view.getInspector().setFolded (section, true);
        studio.dispatchPendingMessages();
    }
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FREEZE", {}).isNotEmpty())
    {
        juce::String error;
        studio.freezeTrack (lead, 1.0, error);
        studio.selectTrack (lead);
        for (const auto section : { Inspector::Section::routing, Inspector::Section::sends, Inspector::Section::quick })
            view.getInspector().setFolded (section, true);
        studio.dispatchPendingMessages();
    }
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_MULTI", {}).isNotEmpty())
    {
        std::vector<int> ids;
        for (const auto id : { keys, bass })
            for (const auto& part : studio.getProject().findTrack (id)->parts)
                if (part.start >= 16.0 && part.start < 32.0)
                    ids.push_back (part.id);
        view.getArrangeView().selectParts (ids);
    }
    // PARAGON_SNAPSHOT_VISIBILITY=1: the left zone's VISIBILITY, the bass hidden and the keys disabled.
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_VISIBILITY", {}).isNotEmpty())
    {
        studio.setTracksHidden ({ bass }, true);
        studio.setTracksDisabled ({ keys }, true);
        view.getInspector().setTab (Inspector::Tab::visibility);
    }
    // Play a little so the meters show.
    studio.play();
    for (int round = 0; round < 6; ++round)
    {
        renderSamples (studio.getEngine(), 4096);
        juce::Timer::callPendingTimersSynchronously();
        juce::Thread::sleep (40);   // (the meters refresh every 33 ms: give them a turn)
    }
    studio.stop();
    // CPU: a load to show (a third on average, peaks to 60%: made up for the picture).
    for (int i = 0; i < 60; ++i)
        studio.getEngine().notePerformance ((i % 10 == 0 ? 0.6 : 0.33) * 512.0 / studio.getEngine().getSampleRate(), 512);
    juce::Thread::sleep (40);
    juce::Timer::callPendingTimersSynchronously();
    studio.getEngine().setPosition (21.0);
    // The key editor open on the lead part, a few of its notes selected;
    // or the mixer, with some effects in (stand-ins named after the family's
    // plugins: the tests can't load real VST3s).
    view.getArrangeView().select (lead, 0);
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_MULTI", {}).isNotEmpty())
    {
        std::vector<int> ids;
        for (const auto id : { keys, bass })
            for (const auto& part : studio.getProject().findTrack (id)->parts)
                if (part.start >= 16.0 && part.start < 32.0)
                    ids.push_back (part.id);
        view.getArrangeView().selectParts (ids);
    }
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
        studio.setLastPreFaderSlot (bass, 0);   // the bass's second effect after its fader
        add (Engine::masterBus, "Paragon PR7-C");
        studio.setVolume (keys, -4.5f);
        studio.setPan (keys, -0.3f);
        studio.setVolume (lead, -8.0f);
        studio.setPan (lead, 0.25f);
        studio.setMasterVolume (-1.5f);
        // An FX channel (a reverb: a stand-in named after the family's plugin) that keys, lead and guitar send to.
        Insert reverb;
        reverb.plugin.name = "Paragon PR7-V";
        const auto fx = studio.addFxChannel (reverb, timesEffect (0.5f, "Paragon PR7-V"));
        studio.setSend (keys, 0, { fx, -14.0f, true, false });
        studio.setSend (lead, 0, { fx, -6.0f, true, true });
        studio.setSend (guitar, 0, { fx, -10.0f, true, false });
        studio.setVolume (fx, -3.0f);
        studio.selectTrack (bass);
        view.openMixer();
        if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_SENDS", {}).isNotEmpty())
            view.getMixer().setShowSends (true);
        if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_RACK", {}) == "none")
        {
            view.getMixer().setRackShown (false);   // and the mixer dragged as short as it goes
            view.setDockedHeight (1);
        }
        studio.dispatchPendingMessages();   // (the new FX channel's strip made now, so its meters see the playing)
        studio.play();
        for (int round = 0; round < 4; ++round)
        {
            renderSamples (studio.getEngine(), 4096);
            juce::Timer::callPendingTimersSynchronously();
            juce::Thread::sleep (40);   // (the meters refresh every 33 ms: give them a turn)
        }
        studio.stop();
    }
    else if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_ZONE", {}) != "none")   // none: the lower zone closed
    {
        view.openKeyEditor();
        studio.dispatchPendingMessages();
        view.getKeyEditor().selectNotes ({ 3, 4, 5 });
    }
    // PARAGON_SNAPSHOT_EDIT=1: the SPLIT tool chosen, and the bass's second part muted.
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_EDIT", {}).isNotEmpty())
    {
        view.getArrangeView().setTool (ArrangeView::Tool::split);
        for (const auto& track : studio.getProject().tracks)
            if (track.name.equalsIgnoreCase ("Bass") && track.parts.size() > 1)
                studio.setPartMuted (track.id, 1, true);
    }
    // PARAGON_SNAPSHOT_RANGE=1: the RANGE tool, a range over the keys and the bass, bars 3 to 5.
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_RANGE", {}).isNotEmpty())
    {
        auto& arrange = view.getArrangeView();
        arrange.setTool (ArrangeView::Tool::range);
        const auto& tracks = studio.getProject().tracks;
        if (tracks.size() >= 2)
            arrange.setRange (pr7s::Studio::Range { 8.0, 18.0, { tracks[0].id, tracks[1].id } });
    }
    // PARAGON_SNAPSHOT_TRACKS=1: two tracks selected (the keys and the lead), the lead being dragged up (the drop line).
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_TRACKS", {}).isNotEmpty())
    {
        const auto& tracks = studio.getProject().tracks;
        if (tracks.size() >= 3)
        {
            studio.selectTrack (tracks[0].id);
            studio.toggleTrackSelected (tracks[2].id);
            studio.dispatchPendingMessages();
            view.getTrackList().dragTrack (tracks[2].id, { 100, Timeline::rulerHeight + Timeline::trackHeight + 10 });
        }
    }
    // PARAGON_SNAPSHOT_ROWS=<zoom>: the arrangement's rows that much taller (or shorter, below 1).
    if (const auto rows = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_ROWS", {}); rows.isNotEmpty())
    {
        view.getTimeline().rowZoom = juce::jlimit (Timeline::minRowZoom, Timeline::maxRowZoom, rows.getDoubleValue());
        view.getTimeline().onScrollOrZoom();
    }
    // PARAGON_SNAPSHOT_SUSPENDED=1: the arrangement's Auto-Scroll suspended (its key pink, SUSPENDED).
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_SUSPENDED", {}).isNotEmpty())
    {
        view.getTimeline().autoScroll.suspended = true;
        view.getTimeline().onAutoScrollChange();
    }
    studio.dispatchPendingMessages();

    auto image = view.createComponentSnapshot (view.getLocalBounds(), true, scale);
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_RENDER", {}).isNotEmpty())
    {
        // The RENDER IN PLACE page over the studio, as its dialog shows it.
        Windows::RenderPage page (studio, false);
        const auto pageImage = page.createComponentSnapshot (page.getLocalBounds(), true, scale);
        juce::Graphics g (image);
        g.fillAll (juce::Colours::black.withAlpha (0.45f));
        const auto at = image.getBounds().withSizeKeepingCentre (pageImage.getWidth(), pageImage.getHeight());
        g.drawImageAt (pageImage, at.getX(), at.getY());
        g.setColour (juce::Colour (0xff3a3540));
        g.drawRect (at.expanded (1), 1);
    }
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
            juce::Thread::sleep (40);   // (the meters refresh every 33 ms: give them a turn)
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

// The tools' pointers, as they look over the tracks (on a part and on an empty
// lane), enlarged 3x: PARAGON_SNAPSHOT=cursors.png ./ParagonPR7STests "[snapshot-cursors]"
TEST_CASE ("Snapshot: the tools' mouse pointers", "[.][snapshot-cursors]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "cursors.png");
    const auto count = (int) std::size (ArrangeView::tools);
    juce::Image image (juce::Image::RGB, count * 36 * 3, 2 * 36 * 3, true);
    {
        juce::Graphics g (image);
        g.addTransform (juce::AffineTransform::scale (3.0f));
        g.fillAll (juce::Colour (0xff0d0a0e));
        g.setColour (juce::Colour (0xffe0103c).withAlpha (0.6f));
        g.fillRect (0, 36, count * 36, 36);
        for (int i = 0; i < count; ++i)
            for (int row = 0; row < 2; ++row)
            {
                // Drawn as the pointer is: dark edge, white over it.
                const juce::Rectangle<float> box ((float) i * 36.0f + 6.0f, (float) row * 36.0f + 6.0f, 24.0f, 24.0f);
                for (const auto& offset : { juce::Point<float> (-1.0f, 0.0f), { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } })
                    ToolIcons::draw (g, ArrangeView::tools[i], box.translated (offset.x, offset.y), juce::Colour (0xff120c10));
                ToolIcons::draw (g, ArrangeView::tools[i], box, juce::Colours::white);
            }
    }
    juce::File (path).deleteFile();
    juce::FileOutputStream out { juce::File (path) };
    REQUIRE (out.openedOk());
    REQUIRE (juce::PNGImageFormat().writeImageToStream (image, out));
}

// The settings page with a stand-in ASIO driver (as the AXE I/O's: one buffer
// size, the one its panel sets): PARAGON_SNAPSHOT=settings.png ./ParagonPR7STests "[snapshot-settings]"
// (PARAGON_SNAPSHOT_FINISH as above; PARAGON_SNAPSHOT_DRIVER="Windows Audio" for that type).
TEST_CASE ("Snapshot: the settings page", "[.][snapshot-settings]")
{
    using namespace pr7s;
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "settings.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    struct Reset { ~Reset() { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); } } reset;

    Studio studio (false);
    const auto type = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_DRIVER", "ASIO");
    studio.getDeviceManager().addAudioDeviceType (std::make_unique<FakeDriver> (type, type == "ASIO" ? juce::Array<int> { 64 } : juce::Array<int> { 480 },
                                                                                 type == "ASIO"));
    studio.getDeviceManager().setCurrentAudioDeviceType (type, true);
    Windows::Settings settings (studio, {}, {});
    auto* page = settings.getContentComponent();
    const auto image = page->createComponentSnapshot (page->getLocalBounds(), true, 1.5f);
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (path);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    REQUIRE (stream.openedOk());
    REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));
}
