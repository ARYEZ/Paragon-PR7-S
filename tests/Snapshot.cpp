#include <catch2/catch_test_macros.hpp>

#include "DeviceTests.h"
#include "TestHelpers.h"
#include "engine/Studio.h"
#include "engine/Controllers.h"
#include "ui/Finish.h"
#include "ui/MainView.h"
#include "ui/DrumMapPage.h"
#include "ui/AutomationPanel.h"
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
// PARAGON_SNAPSHOT_STATES=1: the lead soloed, the bass muted, the keys armed (written under their names; the header's M and S lit).
// PARAGON_SNAPSHOT_MASTER=1: the master's row chosen (its Inspector).
// PARAGON_SNAPSHOT_RACK=home|loops|instruments: the Media rack in the right zone (0.22.0).
// PARAGON_SNAPSHOT_TEMPO=1: the tempo track, with a ramp and a step (0.10.0).
// PARAGON_SNAPSHOT_SIGNATURE=1: the signature track, 7/8 from bar 3 and 3/4 from bar 5, the playhead in the 7/8 (0.45.0).
// PARAGON_SNAPSHOT_TAP=1: the transport while tapping the tempo (0.45.0).
// PARAGON_SNAPSHOT_WARP=1: the guitar's clip warped (markers from its hitpoints, one dragged), selected, the WARP tool (0.32.0).
// PARAGON_SNAPSHOT_TAKES=1: the guitar's three takes on lanes (LANES), comped (0.29.0), the COMP tool chosen.
// PARAGON_SNAPSHOT_FOLLOW=1 (with PARAGON_SNAPSHOT_CHORDS): the lead following the chord track, two chords changed (0.36.0).
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
    // PARAGON_SNAPSHOT_AUTOEDIT=1 (0.27.0, with PARAGON_SNAPSHOT_AUTOMATION): the lead's volume taller, a bent ramp,
    // three points selected (the event editor's box), the LINE tool chosen.
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_AUTOEDIT", {}).isNotEmpty())
    {
        studio.setAutomationPoints (Engine::masterBus, 0, { { 16.0, 0.3f, 0.0f }, { 18.0, 0.9f, 0.6f }, { 21.0, 0.5f, 0.0f },
                                                            { 24.0, 0.8f, -0.5f }, { 26.0, 0.4f, 0.0f }, { 30.0, 0.7f, 0.0f } });
        for (int i = 0; i < 3; ++i)
            view.getArrangeView().zoomRows (true);
        view.getArrangeView().selectPoints (Engine::masterBus, 0, { 2, 3, 4 });
        view.getArrangeView().setTool (ArrangeView::Tool::line);
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
    // PARAGON_SNAPSHOT_TEMPO=1: the tempo track (0.10.0): a ramp up to bar 3, a step down at bar 5, the playhead in the ramp.
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_TEMPO", {}).isNotEmpty())
    {
        studio.addTempoTrack();
        studio.setTempoEvents ({ { 0.0, 120.0, false }, { 8.0, 140.0, true }, { 16.0, 96.0, false }, { 24.0, 104.0, true } });
        studio.getEngine().setPosition (5.0);
    }
    // PARAGON_SNAPSHOT_SIGNATURE=1 (0.45.0): the signature track with two changes, the 7/8 selected.
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_SIGNATURE", {}).isNotEmpty())
    {
        studio.addSignatureTrack();
        studio.setSignatureAt (2, 7, 8);
        studio.setSignatureAt (4, 3, 4);
        studio.getEngine().setPosition (9.0);
    }
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_TAP", {}).isNotEmpty())
    {
        const auto now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        for (int i = 3; i >= 0; --i)
            studio.tapTempo (now - 0.48 * i);   // 125 BPM
    }
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
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_STATES", {}).isNotEmpty())
    {
        studio.setSolo (lead, true);
        studio.setMute (studio.getProject().tracks[1].id, true);
        studio.setArmed (studio.getProject().tracks[0].id);
        studio.dispatchPendingMessages();
    }
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_WARP", {}).isNotEmpty())
    {
        const auto& clip = studio.getProject().findTrack (guitar)->parts.at (0);
        const auto id = clip.id;
        studio.createWarpFromHitpoints ({ id });
        if (const auto& warped = studio.getProject().findTrack (guitar)->parts.at (0); warped.warp.size() > 4)
            studio.moveWarpMarker (id, 3, warped.warp[3].beat + 0.4);
        view.getArrangeView().setTool (ArrangeView::Tool::warp);
        view.getArrangeView().select (guitar, 0);
        studio.selectTrack (guitar);
        studio.dispatchPendingMessages();
    }
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_TAKES", {}).isNotEmpty())
    {
        // Two more takes of the guitar's (the same recording, further in: stand-ins), then comped:
        // take 1 over bar 3, take 2 over bars 4 and 5, take 3 elsewhere.
        for (int i = 1; i < 3; ++i)
        {
            Part another { 4.0, 20.0, "Guitar take " + juce::String (i + 1), {} };
            another.audioFile = "Guitar 01.wav";
            another.audioOffset = 1.0 + i * 0.37;
            another.fadeIn = 0.4;
            another.fadeOut = 1.5;
            another.lane = i;
            another.order = i;
            studio.addPart (guitar, another);
        }
        studio.compRange (guitar, 0, 8.0, 12.0);
        studio.compRange (guitar, 1, 12.0, 20.0);
        studio.setShowLanes (guitar, true);
        view.getArrangeView().setTool (ArrangeView::Tool::comp);
        studio.selectTrack (guitar);
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
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_CHORDS", {}).isNotEmpty())
    {
        // The chord track (0.16.0): C Am F G7, then Eb major (its scale event follows).
        using CT = pr7s::Chord::Type;
        using CX = pr7s::Chord::Tension;
        const std::pair<double, pr7s::Chord> chords[] = { { 0.0, { 0, CT::major, CX::none, -1 } }, { 4.0, { 9, CT::minor, CX::none, -1 } },
                                                         { 8.0, { 5, CT::major, CX::none, -1 } }, { 12.0, { 7, CT::major, CX::seven, -1 } },
                                                         { 16.0, { 3, CT::major, CX::none, -1 } }, { 20.0, { 8, CT::major, CX::none, 0 } },
                                                         { 24.0, { 10, CT::major, CX::seven, -1 } }, { 28.0, { 3, CT::major, CX::none, -1 } } };
        for (const auto& [beat, chord] : chords)
            studio.addChordEvent (beat, chord);
        // (0.44.0) PARAGON_SNAPSHOT_PERFORM=1: the chord track recording, the lead performing a pattern.
        if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_PERFORM", {}).isNotEmpty())
        {
            studio.setChordTrackRecord (true);
            for (const auto& t : studio.getProject().tracks)
                if (t.name == "Lead")
                    studio.setPerformPattern (t.id, "Up and down");
        }
    }
    // PARAGON_SNAPSHOT_FOLLOW=1 (0.36.0, with PARAGON_SNAPSHOT_CHORDS): the lead follows the chord track
    // (Chords & Scales, from the chords as they were), then two chords change: Eb -> Fm, Bb7 -> Cm7;
    // its Live Input on chords; Shift Chords -30 ticks. (PARAGON_SNAPSHOT_SELECT=Lead for its Inspector.)
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FOLLOW", {}).isNotEmpty())
    {
        using CT = pr7s::Chord::Type;
        using CX = pr7s::Chord::Tension;
        // (=voice, 0.38.0: Single Voice, the alto of the piano voicings.)
        const auto voice = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FOLLOW", {}) == "voice";
        studio.startFollowing (lead, voice ? pr7s::FollowMode::singleVoice : pr7s::FollowMode::chordsAndScales, pr7s::FollowStart::directly);
        if (voice)
            studio.setFollowVoice (lead, pr7s::Voice::alto);
        const auto& events = studio.getChordTrack().events;
        for (int i = 0; i < (int) events.size(); ++i)
        {
            if (events[(size_t) i].beat == 16.0)
                studio.setChordEventChord (i, { 5, CT::minor, CX::none, -1 });
            if (events[(size_t) i].beat == 24.0)
                studio.setChordEventChord (i, { 0, CT::minor, CX::seven, -1 });
        }
        studio.setLiveFollow (lead, pr7s::LiveFollow::chords);
        studio.setChordShift (-30);
    }
    // PARAGON_SNAPSHOT_LONGNAMES=1 (0.36.1): long track names, to show them running up to the meter.
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_LONGNAMES", {}).isNotEmpty())
    {
        studio.renameTrack (keys, "Forever Found Keys Layer");
        studio.renameTrack (bass, "FFAW Main Vocals One");
        studio.renameTrack (lead, "Bridge Synth Lead (Right)");
        studio.renameTrack (guitar, "Forever Found Rhythm Guitar Double");
    }
    // PARAGON_SNAPSHOT_INFOLINE=1 (0.37.0, with PARAGON_SNAPSHOT_AUTOMATION): three of the lead's volume points selected (the info line shows them).
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_INFOLINE", {}).isNotEmpty())
        view.getArrangeView().selectPoints (lead, 0, { 0, 1, 2 });
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
    studio.getEngine().setPosition (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_SIGNATURE", {}).isNotEmpty() ? 9.0 : 21.0);
    // The key editor open on the lead part, a few of its notes selected;
    // or the mixer, with some effects in (stand-ins named after the family's
    // plugins: the tests can't load real VST3s).
    view.getArrangeView().select (lead, 0);
    // PARAGON_SNAPSHOT_INFOLINE=audio (0.47.0): the guitar's first event selected, at -3 dB with a fade-in (the info line shows it).
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_INFOLINE", {}) == "audio")
        if (const auto* track = studio.getProject().findTrack (guitar); track != nullptr && ! track->parts.empty())
        {
            const auto id = track->parts.front().id;
            studio.setPartsField ({ id }, Studio::PartField::volume, -3.0, true);
            studio.setPartsField ({ id }, Studio::PartField::fadeIn, 0.4, true);
            view.getArrangeView().selectParts ({ id });
        }
    view.getInfoLine().refresh();   // (its 10 Hz timer doesn't run before the picture)
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
        if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_GROUP", {}).isNotEmpty())
        {
            // A group channel (0.13.1): its strip at the right, next to the master's.
            const auto group = studio.addGroupChannel();
            studio.renameTrack (group, "Rhythm");
            studio.setOutput (keys, group);
            studio.setOutput (bass, group);
        }
        if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_LINK", {}).isNotEmpty())
        {
            // A link group (0.17.0): keys and lead, its name over theirs; Q-Link lit.
            LinkGroup settings;
            settings.name = "Leads";
            settings.pan = true;
            studio.linkTracks ({ keys, lead }, settings);
            studio.setQLink (true);
            studio.selectTrack (keys);
        }
        else
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
        // PARAGON_SNAPSHOT_DRUMS=1 (0.26.0): a drum track with a rock beat in the drum editor (the GM Map;
        // the crash muted, some hats selected; the velocity lane).
        if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_DRUMS", {}).isNotEmpty())
        {
            const auto drums = studio.addTrack ({});
            studio.renameTrack (drums, "Drums");
            Part beat { 16.0, 8.0, "Beat", {} };
            for (int eighth = 0; eighth < 16; ++eighth)
            {
                beat.notes.push_back ({ eighth * 0.5, 0.25, eighth % 4 == 3 ? 46 : 42, eighth % 2 == 0 ? 0.85f : 0.55f });
                if (eighth % 4 == 0)
                    beat.notes.push_back ({ eighth * 0.5, 0.25, 36, 0.95f });
                if (eighth % 4 == 2)
                    beat.notes.push_back ({ eighth * 0.5, 0.25, 38, 0.9f });
            }
            beat.notes.push_back ({ 3.75, 0.25, 36, 0.7f });
            beat.notes.push_back ({ 0.0, 0.5, 49, 1.0f });
            beat.notes.push_back ({ 6.5, 0.25, 48, 0.8f });
            beat.notes.push_back ({ 7.0, 0.25, 45, 0.8f });
            beat.notes.push_back ({ 7.5, 0.25, 43, 0.85f });
            studio.addPart (drums, beat);
            studio.setTrackDrumMap (drums, "GM Map");
            auto map = *studio.getDrumMapOf (drums);
            map.sounds[57].mute = true;
            studio.setDrumMap ("GM Map", map);
            studio.dispatchPendingMessages();
            const auto index = (int) studio.getProject().findTrack (drums)->parts.size() - 1;
            view.getArrangeView().select (drums, index);
            studio.dispatchPendingMessages();
            auto& editor = view.getKeyEditor();
            editor.setDrumRowsShown (KeyEditor::DrumRowsShown::named);
            std::vector<int> hats;
            const auto& notes = studio.getProject().findTrack (drums)->parts[(size_t) index].notes;
            for (int i = 0; i < (int) notes.size(); ++i)
                if (notes[(size_t) i].pitch == 42 && notes[(size_t) i].start >= 2.0 && notes[(size_t) i].start < 4.0)
                    hats.push_back (i);
            editor.selectNotes (hats);
            editor.setTool (KeyEditor::Tool::draw);
            view.setKeyEditorSize (150);
            view.setDockedHeight (760);
            editor.fitToPart();
        }
        // PARAGON_SNAPSHOT_LANES=1 (0.25.0): the lead with modulation and pitchbend lanes, a swell, a bend
        // with the semitone grid, some modulation events selected.
        if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_LANES", {}).isNotEmpty())
        {
            studio.setPosition (22.0);
            namespace K = pr7s::ControllerKinds;
            for (const auto& track : studio.getProject().tracks)
                if (track.name == "Lead" && ! track.parts.empty())
                {
                    std::vector<Controller> events;
                    for (int i = 0; i <= 24; ++i)   // modulation: a slow wave
                        events.push_back ({ Controller::Type::cc, 1, i * 0.5, 40 + juce::roundToInt (50.0 * std::sin (i * 0.45)), false });
                    events.push_back ({ Controller::Type::pitchbend, 0, 0.0, 8192, false });
                    events.push_back ({ Controller::Type::pitchbend, 0, 3.0, 8192, false });
                    events.push_back ({ Controller::Type::pitchbend, 0, 4.0, 16383, true });   // a whole tone up...
                    events.push_back ({ Controller::Type::pitchbend, 0, 6.0, 16383, false });
                    events.push_back ({ Controller::Type::pitchbend, 0, 6.5, 8192, true });    // ...and back
                    events.push_back ({ Controller::Type::cc, 11, 0.0, 70, false });
                    events.push_back ({ Controller::Type::cc, 11, 8.0, 127, true });
                    K::sort (events);
                    studio.setControllers (track.id, 0, events);
                    studio.setControllerLanes (track.id, { K::velocity, 1, K::pitchbend });
                    studio.setPitchbendGrid (track.id, 2, 2, false, true);
                    studio.dispatchPendingMessages();
                    auto& editor = view.getKeyEditor();
                    editor.setLaneHeight (64);
                    std::vector<int> some;
                    for (int i = 0; i < (int) studio.getProject().findTrack (track.id)->parts[0].controllers.size(); ++i)
                        if (const auto& c = studio.getProject().findTrack (track.id)->parts[0].controllers[(size_t) i];
                            c.type == Controller::Type::cc && c.number == 1 && c.start >= 4.0 && c.start <= 7.0)
                            some.push_back (i);
                    editor.setLanesFocused (true);
                    editor.selectAllOnLane (editor.laneOf (1));
                    editor.selectControllers (some);
                    editor.setTool (KeyEditor::Tool::line);
                }
            view.setDockedHeight (700);
        }
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

    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_MASTER", {}).isNotEmpty())
    {
        studio.selectMaster();   // (last: the set-up above chooses tracks)
        studio.dispatchPendingMessages();
    }
    // PARAGON_SNAPSHOT_SELECT=<track name>: that track chosen last (its Inspector: 0.35.0's LANES and MON).
    if (const auto name = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_SELECT", {}); name.isNotEmpty())
        for (const auto& t : studio.getProject().tracks)
            if (t.name.equalsIgnoreCase (name))
            {
                studio.selectTrack (t.id);
                studio.dispatchPendingMessages();
                break;
            }
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_DROP", {}).isNotEmpty())
    {
        // A MIDI file being dragged over the lead's track (0.18.1): where it will land, lit.
        std::vector<pr7s::Note> notes;
        for (int i = 0; i < 8; ++i)
            notes.push_back ({ i * 1.0, 0.9, 64 + (i % 4) * 3, 0.8f });
        view.getArrangeView().setDropPreview (ArrangeView::DropPreview { lead, 16.0, 8.0, "Strings Verse", 1, notes });
    }
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_RULERHOVER", {}).isNotEmpty())
    {
        // The ruler's hover marker (0.17.4): where a click would put the playhead.
        auto& arrange = view.getArrangeView();
        const juce::Point<float> at { view.getTimeline().xForBeat (6.5), (float) (Timeline::cycleStripHeight + 6) };
        arrange.mouseMove (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                             &arrange, &arrange, juce::Time::getCurrentTime(), at, juce::Time::getCurrentTime(), 0, false));
    }
    if (const auto rackPage = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_RACK", {}); rackPage.isNotEmpty())
    {
        // The Media rack in the right zone (0.22.0): home, loops or instruments.
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Rack Samples");
        root.deleteRecursively();
        using A = pr7s::MediaAttribute;
        auto& bay = studio.getMediaBay();
        const std::vector<std::tuple<const char*, const char*, const char*, const char*>> files {
            { "Drums", "Drop A Chug Loop 140bpm.wav", "Drums", "Metal" }, { "Drums", "Gallop Kit Loop 160bpm.wav", "Drums", "Metal" },
            { "Drums", "Kick 808 Long.wav", "Drums", "" }, { "Guitars", "Rogue Riff Am 140bpm.wav", "Guitar", "Metal" },
            { "Guitars", "Jupiter Lead F#m 120bpm.wav", "Guitar", "Metal" }, { "Bass", "Sub Bass E 128bpm.wav", "Bass", "" },
            { "MIDI", "Djent Chug Pattern 140bpm.mid", "Guitar", "Metal" }, { "MIDI", "Power Chord Riff Am.mid", "Guitar", "Rock" } };
        for (const auto& [sub, name, category, style] : files)
        {
            root.getChildFile (sub).createDirectory();
            root.getChildFile (sub).getChildFile (name).replaceWithText ("x");
        }
        bay.addLocation (root);
        bay.scanNow();
        for (const auto& [sub, name, category, style] : files)
        {
            const auto file = root.getChildFile (sub).getChildFile (name);
            bay.setAttribute ({ file }, { A::category, {} }, category);
            bay.setAttribute ({ file }, { A::style, {} }, style);
        }
        view.setRightZoneShown (true);
        auto* rack = view.getMediaRack();
        rack->setAutoPlay (false);
        if (rackPage == "loops")
        {
            rack->openPage (MediaRack::Page::loops);
            rack->setFiltersShown (true);
            rack->clickFilterValue (1, "Metal", false);
            rack->selectRow (1);
        }
        else if (rackPage == "instruments")
            rack->openPage (MediaRack::Page::instruments);
        else
            rack->home();
        studio.dispatchPendingMessages();
    }
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
    studio.setMultiCoreAudio (true);   // (0.34.5: as the app has it)
    Windows::Settings settings (studio, {}, {});
    auto* page = settings.getContentComponent();
    const auto image = page->createComponentSnapshot (page->getLocalBounds(), true, 1.5f);
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (path);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    REQUIRE (stream.openedOk());
    REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));
}

// A question box, as the app shows them (0.9.10: pearl black, white text, the PR7-S mark).
TEST_CASE ("Snapshot: a question box", "[.][snapshot-alert]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "alert.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        juce::AlertWindow box ("Delete track", "Delete \"lead guitars\" and its parts? This can't be undone.", juce::MessageBoxIconType::QuestionIcon);
        box.addButton ("Delete", 1);
        box.addButton ("Cancel", 0);
        const auto image = box.createComponentSnapshot (box.getLocalBounds(), true, 2.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

#include "ui/LinkGroupPage.h"
#include "ui/TrackColours.h"
#include "ui/LinesPage.h"
#include "ui/ProgressionsPage.h"
#include "ui/EqEditor.h"
#include "ui/PadsRemotePage.h"

TEST_CASE ("Snapshot: the IDEAS window (PARAGON_SNAPSHOT_IDEAS=piano|guitar|bass|patterns)", "[.][snapshot-lines]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "lines.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        using T = pr7s::Chord::Type;
        using X = pr7s::Chord::Tension;
        pr7s::Studio studio (false);
        studio.addChordEvent (0.0, { 9, T::minor, X::none, -1 });
        studio.addChordEvent (4.0, { 5, T::major, X::none, -1 });
        studio.addChordEvent (8.0, { 0, T::major, X::none, -1 });
        studio.addChordEvent (12.0, { 7, T::major, X::none, -1 });
        LinesPage page (studio, studio.getLineChords());
        const auto tab = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_IDEAS", "guitar");
        page.setTab (tab == "piano" ? LinesPage::Tab::piano : tab == "bass" ? LinesPage::Tab::bass
                   : tab == "patterns" ? LinesPage::Tab::patterns : tab == "melody" ? LinesPage::Tab::melody
                   : LinesPage::Tab::guitar);   // (0.39.0: =patterns; 0.41.0: =melody)
        page.play (tab == "piano" ? pr7s::LineIdea::Part::chords : tab == "bass" ? pr7s::LineIdea::Part::bass
                 : tab == "patterns" ? pr7s::LineIdea::Part::pattern : tab == "melody" ? pr7s::LineIdea::Part::tune
                 : pr7s::LineIdea::Part::rhythm, 1);
        const auto image = page.createComponentSnapshot (page.getLocalBounds(), true, 1.0f);
        page.stop();
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

TEST_CASE ("Snapshot: the PROGRESSIONS window (0.40.0; PARAGON_SNAPSHOT_GENRE, PARAGON_SNAPSHOT_KEY)", "[.][snapshot-progressions]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "progressions.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        pr7s::Studio studio (false);
        ProgressionsPage page (studio);
        page.setSize (1500, 860);
        const auto keyName = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_KEY", "A minor");
        for (const auto& k : pr7s::Key::all())
            if (k.name() == keyName)
                page.setKey (k);
        page.setGenre (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_GENRE", "METAL"));
        studio.setProgressionFavorite (pr7s::progressionLibrary()[(size_t) page.getShown()[0]].name, true);
        page.hear (page.getShown()[1]);
        const auto image = page.createComponentSnapshot (page.getLocalBounds(), true, 1.0f);
        page.stop();
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

TEST_CASE ("Snapshot: the guitar VELOCITIES panel", "[.][snapshot-velocities]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "velocities.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        // PARAGON_SNAPSHOT_HUMANIZE=1: IDEAS' HUMANIZE panel instead (0.20.0).
        std::unique_ptr<juce::Component> panel;
        if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_HUMANIZE", {}) == "1")
            panel = std::make_unique<HumanizePanel> (0.4f, true);
        else if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_HUMANIZE", {}) == "melody")   // (0.41.0: MELODY SETTINGS)
            panel = std::make_unique<MelodyPanel> (pr7s::MelodySettings {});
        else
            panel = std::make_unique<GuitarVelocityPanel> (pr7s::GuitarVelocities {});
        panel->resized();
        const auto image = panel->createComponentSnapshot (panel->getLocalBounds(), true, 2.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

#include "ui/MediaBayPage.h"

TEST_CASE ("Snapshot: the MediaBay window", "[.][snapshot-mediabay]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "mediabay.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Metal Samples");
    root.deleteRecursively();
    {
        // A sample library, as a producer's might be.
        const auto wav = [] (const juce::File& file, double seconds, int channels, float frequency)
        {
            file.getParentDirectory().createDirectory();
            const auto rate = 44100.0;
            const auto samples = (int) (seconds * rate);
            juce::AudioBuffer<float> sound (channels, samples);
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < samples; ++i)
                {
                    const auto beat = std::fmod ((double) i / rate, 0.5);
                    sound.setSample (c, i, (float) (std::exp (-beat * 9.0) * 0.8 * std::sin ((double) i * frequency / rate * 6.283)));
                }
            juce::WavAudioFormat format;
            auto stream = std::make_unique<juce::FileOutputStream> (file);
            std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (stream.get(), rate, (unsigned int) channels, 24, {}, 0));
            stream.release();
            writer->writeFromAudioSampleBuffer (sound, 0, samples);
        };
        wav (root.getChildFile ("Drums").getChildFile ("Drop A Chug Loop 140bpm.wav"), 3.43, 2, 110.0f);
        wav (root.getChildFile ("Drums").getChildFile ("Gallop Kit Loop 160bpm.wav"), 3.0, 2, 90.0f);
        wav (root.getChildFile ("Drums").getChildFile ("Kick 808 Long.wav"), 1.2, 1, 50.0f);
        wav (root.getChildFile ("Drums").getChildFile ("Snare Heavy Room.wav"), 0.8, 2, 220.0f);
        wav (root.getChildFile ("Guitars").getChildFile ("Rogue Riff Am 140bpm.wav"), 6.86, 2, 140.0f);
        wav (root.getChildFile ("Guitars").getChildFile ("Jupiter Lead F#m 120bpm.wav"), 8.0, 2, 330.0f);
        wav (root.getChildFile ("Bass").getChildFile ("Sub Bass E 128bpm.wav"), 3.75, 1, 41.0f);
        juce::MidiMessageSequence sequence;
        for (int i = 0; i < 16; ++i)
        {
            sequence.addEvent (juce::MidiMessage::noteOn (1, 33 + (i % 4) * 2, (juce::uint8) 100), i * 240.0);
            sequence.addEvent (juce::MidiMessage::noteOff (1, 33 + (i % 4) * 2), i * 240.0 + 200.0);
        }
        sequence.updateMatchedPairs();
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (480);
        midi.addTrack (sequence);
        for (const auto* name : { "Djent Chug Pattern 140bpm.mid", "Power Chord Riff Am.mid" })
        {
            juce::FileOutputStream out (root.getChildFile ("MIDI").getChildFile (name));
            root.getChildFile ("MIDI").createDirectory();
            midi.writeTo (out);
        }
        root.getChildFile ("Presets").createDirectory();
        root.getChildFile ("Presets").getChildFile ("Rogue Drop A Rhythm.pr7track").replaceWithText ("<PR7S/>");
        {
            // A VST preset (0.24.0): header, Info and the chunk list.
            const juce::String infoXml = "<MetaInfo><Attr id=\"PlugInName\" value=\"Rogue\" type=\"string\"/>"
                                         "<Attr id=\"PlugInCategory\" value=\"Instrument|Sampler\" type=\"string\"/>"
                                         "<Attr id=\"Name\" value=\"Djent Rhythm DI\" type=\"string\"/>"
                                         "<Attr id=\"MusicalInstrument\" value=\"Guitar|Rhythm\" type=\"string\"/>"
                                         "<Attr id=\"MusicalStyle\" value=\"Metal|Djent\" type=\"string\"/></MetaInfo>";
            juce::MemoryOutputStream out;
            out.write ("VST3", 4);
            out.writeInt (1);
            out.write ("0123456789ABCDEF00112233AABBCCDD", 32);
            const auto infoAt = (juce::int64) 48;
            const auto listAt = infoAt + (juce::int64) infoXml.getNumBytesAsUTF8();
            out.writeInt64 (listAt);
            out.write (infoXml.toRawUTF8(), infoXml.getNumBytesAsUTF8());
            out.write ("List", 4);
            out.writeInt (1);
            out.write ("Info", 4); out.writeInt64 (infoAt); out.writeInt64 ((juce::int64) infoXml.getNumBytesAsUTF8());
            root.getChildFile ("Presets").getChildFile ("Djent Rhythm.vstpreset").replaceWithData (out.getData(), out.getDataSize());
        }
        root.getChildFile ("Songs").createDirectory();
        root.getChildFile ("Songs").getChildFile ("Night Drive.pr7s").replaceWithText ("<PR7S/>");

        pr7s::Studio studio (false);
        auto& bay = studio.getMediaBay();
        bay.addLocation (root);
        bay.scanNow();
        bay.setRating (root.getChildFile ("Drums").getChildFile ("Drop A Chug Loop 140bpm.wav"), 5);
        bay.setRating (root.getChildFile ("Guitars").getChildFile ("Rogue Riff Am 140bpm.wav"), 4);
        bay.setRating (root.getChildFile ("Drums").getChildFile ("Kick 808 Long.wav"), 3);
        // Tags (0.21.1).
        using A = pr7s::MediaAttribute;
        const auto f = [&] (const char* folder, const char* name) { return root.getChildFile (folder).getChildFile (name); };
        const juce::Array<juce::File> drumLoops { f ("Drums", "Drop A Chug Loop 140bpm.wav"), f ("Drums", "Gallop Kit Loop 160bpm.wav") };
        bay.setAttribute (drumLoops, { A::category, {} }, "Drums");
        bay.setAttribute (drumLoops, { A::subCategory, {} }, "Kit");
        bay.setAttribute ({ f ("Drums", "Kick 808 Long.wav") }, { A::category, {} }, "Drums");
        bay.setAttribute ({ f ("Drums", "Kick 808 Long.wav") }, { A::subCategory, {} }, "Kick");
        bay.setAttribute ({ f ("Drums", "Snare Heavy Room.wav") }, { A::category, {} }, "Drums");
        bay.setAttribute ({ f ("Drums", "Snare Heavy Room.wav") }, { A::subCategory, {} }, "Snare");
        const juce::Array<juce::File> guitars { f ("Guitars", "Rogue Riff Am 140bpm.wav"), f ("Guitars", "Jupiter Lead F#m 120bpm.wav") };
        bay.setAttribute ({ guitars[0] }, { A::category, {} }, "Guitar");
        bay.setAttribute ({ guitars[0] }, { A::subCategory, {} }, "Riff");
        bay.setAttribute ({ guitars[1] }, { A::category, {} }, "Guitar");
        bay.setAttribute ({ guitars[1] }, { A::subCategory, {} }, "Lead");
        bay.setAttribute ({ f ("Bass", "Sub Bass E 128bpm.wav") }, { A::category, {} }, "Bass");
        bay.setAttribute ({ f ("Bass", "Sub Bass E 128bpm.wav") }, { A::subCategory, {} }, "Sub");
        const juce::Array<juce::File> metal { drumLoops[0], drumLoops[1], guitars[0], guitars[1], f ("MIDI", "Djent Chug Pattern 140bpm.mid") };
        bay.setAttribute (metal, { A::style, {} }, "Metal");
        bay.setAttribute ({ drumLoops[0], guitars[0], f ("MIDI", "Djent Chug Pattern 140bpm.mid") }, { A::subStyle, {} }, "Djent");
        bay.setAttribute ({ drumLoops[1], guitars[1] }, { A::subStyle, {} }, "Thrash");
        bay.setCharacter ({ drumLoops[0], guitars[0], guitars[1] }, 0, 1);   // Distorted
        bay.setCharacter ({ guitars[0], drumLoops[0] }, 7, -1);              // Tight
        bay.setCharacter ({ guitars[0] }, 8, 1);                             // Aggressive
        bay.setAttribute ({ guitars[0] }, { A::comment, {} }, "Shreddage Rogue, verse riff");
        bay.addUserAttribute ({ "Session", pr7s::UserAttributeType::text, 0 });
        bay.addUserAttribute ({ "Used in a song", pr7s::UserAttributeType::yesNo, 0 });
        bay.setAttribute ({ guitars[0] }, { A::user, "Session" }, "Album demos");
        bay.setAttribute ({ guitars[0] }, { A::user, "Used in a song" }, "yes");
        MediaBayPage page (studio);
        page.setSize (1600, 900);
        page.setFiltersShown (true);
        page.setInspectorShown (true);
        page.setInspectorDynamic (false);
        page.resetFilters();
        page.setAutoPlay (false);
        page.sortBy (pr7s::MediaColumn::rating, false);
        // The file browser (0.22.1): the library open, Bass left out (its box empty, the library's a square).
        page.getFolderChildren (root);
        page.setFolderScanned (root.getChildFile ("Bass"), false);
        page.clickFilterValue (2, "Metal", false);
        page.selectRow (1);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);   // (the tree lays out its rows on the message loop)
        page.play();
        studio.setMediaPreviewLevel (0.0f);   // (silent: only the playhead moves)
        juce::AudioBuffer<float> out (2, 44100);
        studio.getEngine().render (out, 44100);
        const auto image = page.createComponentSnapshot (page.getLocalBounds(), true, 1.0f);
        page.stop();
        juce::File (path).deleteFile();
        juce::FileOutputStream stream { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, stream);
    }
    root.deleteRecursively();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

TEST_CASE ("Snapshot: the colour menu", "[.][snapshot-colours]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "colours.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        pr7s::Studio studio (false);
        const auto a = studio.getProject().tracks[0].id;
        studio.setTracksColour ({ a }, pr7s::FamilyColours::paletteColour (2, 4));
        TrackColourMenu menu (studio, { a });
        menu.resized();
        const auto image = menu.createComponentSnapshot (menu.getLocalBounds(), true, 2.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

TEST_CASE ("Snapshot: the Link Group Settings page", "[.][snapshot-link]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "link.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        pr7s::Studio studio (false);
        const auto a = studio.getProject().tracks[0].id, b = studio.addTrack ({});
        LinkGroupPage page (studio, 0, { a, b });
        page.setLinks (pr7s::LinkGroup::What::pan, true);
        const auto image = page.createComponentSnapshot (page.getLocalBounds(), true, 2.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

#include "ui/KeyCommandsPage.h"

TEST_CASE ("Snapshot: the Key Commands window", "[.][snapshot-keys]")
{
    // PARAGON_SNAPSHOT_FINISH as the studio's: a Stream Deck key on Click, H moved, a macro.
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "keys.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        pr7s::Studio studio (false);
        MainView view (studio);
        view.setSize (1500, 900);
        auto& map = view.getKeyMap();
        map.assign ("Transport.Click", KeyCommands::parse ("Ctrl+Alt+F13"));
        const auto m = map.addMacro ("Practice loop");
        map.addToMacro (m, "Transport.LocatorsToSelection");
        map.addToMacro (m, "Transport.Cycle");
        map.addToMacro (m, "Transport.Click");
        map.addToMacro (m, "Transport.Start");
        map.assign (KeyCommands::Map::macroId ("Practice loop"), KeyCommands::parse ("Ctrl+Alt+F14"));
        KeyCommandsPage page (map);
        page.setSize (KeyCommandsPage::preferredWidth, KeyCommandsPage::preferredHeight);
        page.selectMacro (0);
        page.selectCommand ("Transport.Click");
        page.assignKey (KeyCommands::parse ("Ctrl+Alt+F13"));
        const auto image = page.createComponentSnapshot (page.getLocalBounds(), true, 1.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

#include "ui/ProgressBox.h"

TEST_CASE ("Snapshot: the progress box, and SAVE lit", "[.][snapshot-progress]")
{
    // PARAGON_SNAPSHOT: the box; with "-save" before .png, the header with SAVE lit.
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "progress.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        ProgressBox box;
        box.set (0.62, "Rendering KEYS (2 of 3)", nullptr);
        box.setOnCancel ([] {});   // (a render: its CANCEL key, 0.13.1)
        const auto image = box.createComponentSnapshot (box.getLocalBounds(), true, 2.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    {
        pr7s::Studio studio (false);
        MainView view (studio);
        view.setSize (1500, 900);
        view.flashSaved();
        const auto image = view.createComponentSnapshot ({ 0, 0, 1500, 56 }, true, 2.0f);
        const juce::File save (path.replace (".png", "-save.png"));
        save.deleteFile();
        juce::FileOutputStream out { save };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

#include "ui/Hub.h"

TEST_CASE ("Snapshot: the Hub", "[.][snapshot-hub]")
{
    // PARAGON_SNAPSHOT_FINISH as the studio's; PARAGON_SNAPSHOT_HUB=settings: its settings page.
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "hub.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    const auto root = juce::File::createTempFile ("pr7s-hub-snapshot").getChildFile ("Projects");
    {
        pr7s::Studio studio (false);
        auto& library = studio.getLibrary();
        library.setDefaultLocation (root);
        const char* names[] = { "Black", "Forever Found a Way", "When Doves Cry", "Signs of Life - Final Mix", "Before the Dawn v2", "To the Moon and Back" };
        auto minutes = 0;
        for (const auto* name : names)
        {
            const auto file = root.getChildFile (name).getChildFile (juce::String (name) + pr7s::Project::fileExtension);
            file.getParentDirectory().createDirectory();
            file.replaceWithText (pr7s::Project().toXml()->toString());
            file.setLastModificationTime (juce::Time::getCurrentTime() - juce::RelativeTime::minutes (minutes += 600));
        }
        for (int i = 5; i >= 0; --i)
            library.addRecent (root.getChildFile (names[i]).getChildFile (juce::String (names[i]) + pr7s::Project::fileExtension));
        Hub hub (studio);
        KeyCommands::Map keys;   // (HUMANIZE's key, 0.20.1, as the studio's map has it)
        keys.add ({ Hub::humanizeCommand, "MIDI", "Humanize", { juce::KeyPress ('H', juce::ModifierKeys::altModifier, 0) }, {} });
        hub.setKeyCommands (&keys);
        hub.setSize (Hub::preferredWidth, Hub::preferredHeight);
        if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_HUB_SMALL", {}) == "1")
            hub.setSize (980, 620);
        if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_HUB", {}) == "settings")
            hub.showSettingsPage (true);
        const auto image = hub.createComponentSnapshot (hub.getLocalBounds(), true, 1.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    root.getParentDirectory().deleteRecursively();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

#include "ui/TempoEditor.h"

TEST_CASE ("Snapshot: the Tempo Track Editor and Process Tempo", "[.][snapshot-tempo-editor]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "tempo-editor.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        pr7s::Studio studio (false);
        studio.setTempo (120.0);
        studio.addTempoTrack();
        studio.setTempoEvents ({ { 0.0, 120.0, false }, { 16.0, 140.0, true }, { 32.0, 96.0, false }, { 48.0, 104.0, true }, { 64.0, 128.0, false } });
        studio.setCycle (16.0, 32.0);
        studio.setPosition (22.0);
        TempoEditor editor (studio);
        editor.setSize (1400, 680);
        editor.selectEvents ({ 2 });
        const auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);

        ProcessTempoPage page (studio);
        page.setNewLength ("7.5");
        const auto pageImage = page.createComponentSnapshot (page.getLocalBounds(), true, 1.0f);
        const juce::File pageFile (path.replace (".png", "-process.png"));
        pageFile.deleteFile();
        juce::FileOutputStream pageOut { pageFile };
        juce::PNGImageFormat().writeImageToStream (pageImage, pageOut);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

TEST_CASE ("Snapshot: the chord pads zone", "[.][snapshot-pads]")
{
    // PARAGON_SNAPSHOT_PADS=grid for the grid layout; the studio at 1500x900 with the pads docked.
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "pads.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        pr7s::Studio studio (false);
        const auto track = studio.getProject().tracks[0].id;
        studio.addPart (track, { 0.0, 16.0, "Chords", { { 0.0, 4.0, 60, 0.8f }, { 0.0, 4.0, 64, 0.8f }, { 0.0, 4.0, 67, 0.8f } } });
        MainView view (studio);
        const auto mode = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_PADS", {});
        view.setSize (mode == "wide" ? 5760 : 1500, 900);
        view.openChordPads();
        auto& pads = view.getChordPads();
        if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_PADS", {}) == "grid")
        {
            auto p = studio.getChordPads();
            p.layout = pr7s::ChordPads::Layout::grid;
            p.rows = 2;
            p.columns = 8;
            p.pads.resize (16);
            studio.setChordPads (p);
        }
        studio.setPadsLocked ({ 6 }, true);
        studio.setAdaptiveReference (0);
        studio.changePadTensions ({ 9 }, 1);   // Am7
        pads.selectPads ({ 9 });
        pads.setEditorShown (true);
        if (mode == "circle" || mode == "wide")
        {
            // Eb minor, the circle, and what could follow Abm (iv).
            pads.chooseKey (pr7s::Key { 2, -1, true });
            pads.setCircleShown (true);
            pads.selectPads ({ 5 });
            pads.circleChordClicked ({ 8, pr7s::Chord::Type::minor, pr7s::Chord::Tension::none, -1 });
            studio.setAdaptiveReference (-1);
            if (mode == "wide")
                pads.setEditorShown (true);   // (the circle is on the right screen: the editor in the middle)
        }
        studio.getPadPlayer().press (9, 100);
        studio.dispatchPendingMessages();
        view.resized();
        juce::Thread::sleep (80);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        const auto image = view.createComponentSnapshot (view.getLocalBounds(), true, 1.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
        studio.getPadPlayer().releaseAll();
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

TEST_CASE ("Snapshot: the Drum Map Setup", "[.][snapshot-drummap]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "drummap.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        pr7s::Studio studio (false);
        studio.addDrumMap (pr7s::DrumMap::gm());
        auto kit = pr7s::DrumMap::gm();
        kit.name = "Metal Kit";
        kit.sounds[36].name = "Kick Left";
        kit.sounds[35].name = "Kick Right";
        kit.sounds[36].outNote = 24;
        kit.sounds[38].channel = 9;
        kit.sounds[39].mute = true;
        studio.addDrumMap (kit);
        const auto trackId = studio.getProject().tracks[0].id;
        DrumMapPage page (studio, [trackId] { return trackId; });
        page.setSize (1100, 700);
        page.selectMap ("Metal Kit");
        page.selectSound (36);
        page.getTable().scrollToEnsureRowIsOnscreen (50);
        const auto image = page.createComponentSnapshot (page.getLocalBounds(), true, 1.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

TEST_CASE ("Snapshot: the Automation panel", "[.][snapshot-autopanel]")
{
    // PARAGON_SNAPSHOT_TAB=settings: its SETTINGS tab.
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "autopanel.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        pr7s::Studio studio (false);
        studio.setAutomationMode (pr7s::Studio::AutomationSettings::Mode::autoLatch);
        studio.stepFill (pr7s::Studio::FillOption::toEnd);
        studio.stepFill (pr7s::Studio::FillOption::loop);
        studio.stepFill (pr7s::Studio::FillOption::loop);
        studio.setSuspendWrite (pr7s::Studio::AutomationSettings::Group::mute, true);
        studio.setTrim (true);
        AutomationPanel panel (studio);
        if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_TAB", {}) == "settings")
            panel.setTab (AutomationPanel::Tab::settings);
        panel.refresh();
        const auto image = panel.createComponentSnapshot (panel.getLocalBounds(), true, 1.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

#include "ui/FollowSetupPage.h"

// Follow Chord Track's dialog (0.36.0), Apply a Known Chord chosen.
TEST_CASE ("Snapshot: the Follow Chord Track dialog", "[.][snapshot-follow-dialog]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "follow.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        FollowSetupPage page;
        page.setStart (pr7s::FollowStart::knownChord);
        page.setKnownChord ({ 2, pr7s::Chord::Type::minor, pr7s::Chord::Tension::none, -1 });
        const auto image = page.createComponentSnapshot (page.getLocalBounds(), true, 2.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

TEST_CASE ("Snapshot: the MIDI editor's info line naming the selected notes' chord (0.42.0)", "[.][snapshot-chordinfo]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "chordinfo.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        pr7s::Studio studio (false);
        MainView view (studio);
        view.setSize (1500, 900);
        const auto trackId = studio.getProject().tracks[0].id;
        // Am7 with A in the bass, then F, G.
        studio.addPart (trackId, { 0.0, 8.0, "Keys", { { 0.0, 2.0, 45, 0.8f }, { 0.0, 2.0, 60, 0.7f }, { 0.0, 2.0, 64, 0.7f }, { 0.0, 2.0, 67, 0.7f },
                                                      { 2.0, 2.0, 41, 0.8f }, { 2.0, 2.0, 60, 0.7f }, { 2.0, 2.0, 65, 0.7f }, { 2.0, 2.0, 69, 0.7f } } });
        studio.dispatchPendingMessages();
        view.getArrangeView().select (trackId, 0);
        view.openKeyEditor();
        studio.dispatchPendingMessages();
        view.setKeyEditorSize (100);
        auto& editor = view.getKeyEditor();
        editor.selectNotes ({ 0, 1, 2, 3 });
        const auto image = view.createComponentSnapshot (view.getLocalArea (&view.getKeyEditorFrame(), view.getKeyEditorFrame().getLocalBounds()), true, 1.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

TEST_CASE ("Snapshot: the channel EQ (0.43.0; PARAGON_SNAPSHOT_EQ=inspector for the Inspector's section)", "[.][snapshot-eq]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "eq.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        using T = pr7s::EqBand::Type;
        pr7s::Studio studio (false);
        const auto id = studio.getProject().tracks[0].id;
        pr7s::ChannelEq eq;
        // (0.46.0) A low cut 24 on band 1, a notch, a peak, a high shelf with its corner shaped (Q 1.8), Output -1.5 dB.
        eq.bands[0] = { true, T::lowCut, 0.0, 70.0, 0.71 };
        eq.bands[0].slope = 24;
        eq.bands[1] = { true, T::notch, -9.0, 320.0, 1.6 };
        eq.bands[2] = { true, T::parametric, 2.5, 2500.0, 1.2 };
        eq.bands[3] = { true, T::highShelf, 4.0, 9000.0, 1.8 };
        eq.outputDb = -1.5;
        studio.setChannelEq (id, eq);
        // A sound to show the spectrum: a few tones over a little noise, through the channel.
        struct Mix final : TestPlugin
        {
            Mix() : TestPlugin (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)) { instrument = true; name = "Mix"; }
            void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
            {
                for (int i = 0; i < buffer.getNumSamples(); ++i, ++n)
                {
                    auto v = 0.04f * (random.nextFloat() * 2.0f - 1.0f);
                    for (const auto [hz, level] : { std::pair { 110.0, 0.3f }, std::pair { 220.0, 0.15f }, std::pair { 440.0, 0.12f },
                                                    std::pair { 1320.0, 0.05f }, std::pair { 3300.0, 0.02f } })
                        v += level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) n / 48000.0);
                    for (int c = 0; c < buffer.getNumChannels(); ++c)
                        buffer.setSample (c, i, v);
                }
            }
            juce::int64 n = 0;
            juce::Random random { 7 };
        };
        studio.getEngine().prepare (48000.0, 512);
        studio.getEngine().setInstrument (id, std::make_unique<Mix>());
        studio.setChannelEq (id, eq);
        juce::Image image;
        if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_EQ", {}) == "inspector")
        {
            MainView view (studio);
            view.setSize (1500, 1000);
            studio.dispatchPendingMessages();
            auto& inspector = view.getInspector();
            image = view.createComponentSnapshot (view.getLocalArea (&inspector, inspector.getLocalBounds()), true, 1.0f);
        }
        else
        {
            EqEditor editor (studio, id);
            editor.setSize (EqEditor::width, EqEditor::height);
            editor.setKeysShown (true);
            editor.setSpectrumShown (true);
            studio.getEngine().setSpectrumBus (id);
            for (int frame = 0; frame < 6; ++frame)
            {
                renderSamples (studio.getEngine(), 2048, 512);
                editor.getCurve().updateSpectrum();
            }
            image = editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.0f);
        }
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

TEST_CASE ("Snapshot: the chord pads' REMOTE page (0.44.0; PARAGON_SNAPSHOT_REMOTE=learn: a box learning)", "[.][snapshot-remote]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "remote.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        pr7s::Studio studio (false);
        auto pads = studio.getChordPads();
        pads.sectionsMode = true;
        pads.modifiersOn = true;
        studio.setChordPads (pads);
        PadsRemotePage page (studio);
        if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_REMOTE", {}) == "learn")
            page.learn (pr7s::Studio::PadLearn::section, 2);
        const auto image = page.createComponentSnapshot (page.getLocalBounds(), true, 1.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}
