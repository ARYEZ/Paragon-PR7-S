#include "MainView.h"
#include "ui/AutoFadesPage.h"

// Every action the studio has, as a key command (0.11.0): its category, its
// name, its default keys (the studio's keys as they were) and what it does.
// A command with default keys does what its first key does (the built-in
// handling, so where the last click was still counts); the others call the
// action directly. The categories are the manual's Key Commands folders.
void MainView::registerCommands()
{
    using KP = juce::KeyPress;
    const int ctrl = juce::ModifierKeys::commandModifier, alt = juce::ModifierKeys::altModifier, shift = juce::ModifierKeys::shiftModifier;
    // A key as the keyboard sends it (a printable key carries its character: some built-in checks read it).
    const auto key = [] (int code, int mods = 0)
    {
        const auto printable = code > 32 && code < 127 && (mods & juce::ModifierKeys::commandModifier) == 0;
        const auto character = printable ? (juce::juce_wchar) juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) code) : (juce::juce_wchar) 0;
        return KP (code, juce::ModifierKeys (mods), character);
    };
    const auto add = [this] (const char* id, const char* category, const char* name, juce::Array<KP> keys, std::function<void()> action = {})
    {
        if (action == nullptr && ! keys.isEmpty())
            action = [this, first = keys.getFirst()] { handleKey (first); };
        keyMap.add ({ id, category, name, keys, std::move (action) });
    };

    // Transport.
    add ("Transport.StartStop", "Transport", "Start / Stop", { KP (KP::spaceKey) });
    add ("Transport.Start", "Transport", "Start", {}, [this] { if (! studio.getEngine().isPlaying()) studio.play(); });
    add ("Transport.Stop", "Transport", "Stop", {}, [this] { if (studio.getEngine().isPlaying()) studio.togglePlay(); });
    add ("Transport.Record", "Transport", "Record", { key ('R') });
    add ("Transport.Cycle", "Transport", "Cycle On/Off", { key ('/') });
    add ("Transport.Click", "Transport", "Click On/Off", { key ('C') });
    add ("Transport.GoToStart", "Transport", "Go to Project Start", { KP (KP::homeKey) });
    add ("Transport.ToLeft", "Transport", "To Left Locator", { KP (KP::numberPad1) });
    add ("Transport.ToRight", "Transport", "To Right Locator", { KP (KP::numberPad2) });
    add ("Transport.SetLeft", "Transport", "Set Left Locator", { KP (KP::numberPad1, juce::ModifierKeys (ctrl), 0) });
    add ("Transport.SetRight", "Transport", "Set Right Locator", { KP (KP::numberPad2, juce::ModifierKeys (ctrl), 0) });
    add ("Transport.LocatorsToSelection", "Transport", "Locators to Selection", { key ('P') });
    add ("Transport.LoopSelection", "Transport", "Loop Selection", { key ('P', alt) });
    add ("Transport.TempoTrack", "Transport", "Activate Tempo Track", {}, [this] { studio.setTempoTrackOn (! studio.getProject().followsTempoTrack()); });
    add ("Transport.TapTempo", "Transport", "Tap Tempo", { KP (KP::spaceKey, juce::ModifierKeys::shiftModifier, 0) });   // (0.45.0, the manual's)
    add ("Edit.InsertSignature", "Edit", "Insert Time Signature Event", {}, [this] { insertSignatureAtCursor(); });   // (0.45.0: no key, as the manual's)
    add ("Project.AddSignatureTrack", "Project", "Add Signature Track", {}, [this] { studio.addSignatureTrack(); });
    add ("Window.TempoDetection", "Window", "Tempo Detection", {}, [this] { openTempoDetection (0); });
    // Markers (0.49.0, the manual's keys).
    add ("Transport.InsertMarker", "Transport", "Insert Marker", { KP (KP::insertKey) });
    add ("Transport.InsertCycleMarker", "Transport", "Insert Cycle Marker", {}, [this] { studio.addCycleMarkerAtLocators(); });
    add ("Transport.NextMarker", "Transport", "Locate Next Marker", { key ('N', shift) });
    add ("Transport.PreviousMarker", "Transport", "Locate Previous Marker", { key ('B', shift) });
    for (int n = 1; n <= 9; ++n)
    {
        static const char* toIds[] = { "", "Transport.ToMarker1", "Transport.ToMarker2", "Transport.ToMarker3", "Transport.ToMarker4", "Transport.ToMarker5",
                                       "Transport.ToMarker6", "Transport.ToMarker7", "Transport.ToMarker8", "Transport.ToMarker9" };
        static const char* setIds[] = { "", "Transport.SetMarker1", "Transport.SetMarker2", "Transport.SetMarker3", "Transport.SetMarker4", "Transport.SetMarker5",
                                        "Transport.SetMarker6", "Transport.SetMarker7", "Transport.SetMarker8", "Transport.SetMarker9" };
        static const char* toNames[] = { "", "To Marker 1", "To Marker 2", "To Marker 3", "To Marker 4", "To Marker 5", "To Marker 6", "To Marker 7", "To Marker 8", "To Marker 9" };
        static const char* setNames[] = { "", "Set Marker 1", "Set Marker 2", "Set Marker 3", "Set Marker 4", "Set Marker 5", "Set Marker 6", "Set Marker 7", "Set Marker 8", "Set Marker 9" };
        juce::Array<KP> toKeys { KP ('0' + n, juce::ModifierKeys (shift), 0) };
        if (n >= 3)
            toKeys.add (KP (KP::numberPad0 + n));
        add (toIds[n], "Transport", toNames[n], toKeys, [this, n] { studio.locateMarkerId (n); });
        add (setIds[n], "Transport", setNames[n], { KP ('0' + n, juce::ModifierKeys (ctrl), 0) }, [this, n] { studio.setMarkerIdAtCursor (n); });
    }
    add ("Window.Markers", "Window", "Markers", { key ('M', ctrl) });
    add ("Project.AddMarkerTrack", "Project", "Add Marker Track", {}, [this] { studio.addMarkerTrack(); });
    add ("Project.AutoFades", "Project", "Auto Fades Settings", {}, [this] { AutoFadesPage::show (*this, studio, 0); });   // (0.52.0)

    // File.
    add ("File.New", "File", "New Song", { key ('N', ctrl) });
    add ("File.Open", "File", "Open...", { key ('O', ctrl) });
    add ("File.Save", "File", "Save", { key ('S', ctrl) });
    add ("File.SaveAs", "File", "Save As...", { key ('S', ctrl | shift) });
    add ("File.Export", "File", "Export Audio Mixdown...", {}, [this] { exportMix(); });
    add ("File.ExportMidi", "File", "Export MIDI File...", {}, [this] { exportMidiFile(); });   // (0.51.0)
    add ("File.Hub", "File", "Open the Hub", {}, [this] { openHub(); });
    add ("File.SaveTemplate", "File", "Save as Template...", {}, [this] { saveAsTemplate(); });
    add ("File.ImportMidi", "File", "Import MIDI File into This Song...", {}, [this] { importMidiFile (false); });
    add ("File.ImportMidiNew", "File", "Import MIDI File as a New Song...", {}, [this] { importMidiFile (true); });
    add ("File.ImportAudio", "File", "Import Audio Files (a Track Each)...", {}, [this] { importAudioFiles (false); });
    add ("File.ImportAudioOne", "File", "Import Audio Files (One Track)...", {}, [this] { importAudioFiles (true); });
    add ("File.ImportDawProject", "File", "Import DAWproject...", {}, [this] { importDawProjectFile(); });

    // Edit.
    // (0.70.0, the owner asked: the manual's Navigate category; no default keys, Home is Go to Start here)
    add ("Navigate.Top", "Navigate", "Top: Select Top Track in the Track List", {}, [this] { selectTopTrack(); });
    add ("Navigate.Bottom", "Navigate", "Bottom: Select Bottom Track in the Track List", {}, [this] { selectBottomTrack(); });
    add ("Edit.Undo", "Edit", "Undo", { key ('Z', ctrl) });
    add ("Edit.Redo", "Edit", "Redo", { key ('Z', ctrl | shift), key ('Y', ctrl) });
    add ("Edit.Cut", "Edit", "Cut", { key ('X', ctrl) });
    add ("Edit.Copy", "Edit", "Copy", { key ('C', ctrl) });
    add ("Edit.Paste", "Edit", "Paste", { key ('V', ctrl) });
    add ("Edit.PasteAtOrigin", "Edit", "Paste at Origin", { key ('V', alt) });
    add ("Edit.Duplicate", "Edit", "Duplicate", { key ('D', ctrl) });
    add ("Edit.Delete", "Edit", "Delete", { KP (KP::deleteKey), KP (KP::backspaceKey) });
    add ("Edit.SelectAll", "Edit", "Select All", { key ('A', ctrl) });
    add ("Edit.SplitAtCursor", "Edit", "Split at Cursor", { key ('X', alt) });
    add ("Edit.Snap", "Edit", "Snap On/Off", { key ('J') });
    add ("Edit.InsertTempoEvent", "Edit", "Insert Tempo Event at Cursor", {}, [this]
    {
        if (! studio.getProject().tempoTrackShown)
            studio.addTempoTrack();
        studio.insertTempoEventAtCursor (studio.getTempoNow());
    });
    add ("Edit.ReadAll", "Edit", "Automation: Read All On/Off", {}, [this] { studio.setAutomationReadAll (! studio.anyAutomationRead()); });
    add ("Edit.RenderInPlace", "Edit", "Render in Place (as last time)", { key ('R', alt) });   // (0.31.0, the owner's key)
    add ("Edit.RenderInPlaceSetup", "Edit", "Render in Place...", {}, [this] { renderInPlaceNow (true); });
    add ("MIDI.Quantize", "MIDI", "Quantize", { key ('Q') });
    // (0.32.0) Alt+N / Alt+B do these on an audio track (MainView's handleKey: the velocities' keys elsewhere).
    for (const auto forward : { true, false })
        add (forward ? "Audio.NextHitpoint" : "Audio.PreviousHitpoint", "Audio", forward ? "Locate Next Hitpoint" : "Locate Previous Hitpoint", {},
             [this, forward]
             {
                 if (const auto to = studio.nextHitpoint (studio.getSelectedTrack(), studio.getEngine().getPosition(), forward))
                     studio.setPosition (*to);
             });
    add ("Audio.AudioWarpQuantize", "Audio", "AudioWarp Quantize On/Off", {}, [this] { studio.setAudioWarpQuantize (! studio.isAudioWarpQuantize()); });
    add ("Audio.ResetWarp", "Audio", "Reset Warp", {}, [this] { studio.resetWarp (arrange.getSelectedParts()); });
    add ("Edit.WriteAll", "Edit", "Automation: Write All On/Off", { key ('W', alt) });

    // Range (the manual's Range Editing).
    add ("Range.Split", "Range", "Split Range", { key ('X', shift) });
    add ("Range.CutTime", "Range", "Cut Time", { key ('X', ctrl | shift) });
    add ("Range.DeleteTime", "Range", "Delete Time", { KP (KP::deleteKey, juce::ModifierKeys (shift), 0) });
    add ("Range.PasteTime", "Range", "Paste Time", { key ('V', ctrl | shift) });
    add ("Range.InsertSilence", "Range", "Insert Silence", { key ('E', ctrl | shift) });

    // MIDI editor.
    add ("MIDI.VelocityLow", "MIDI", "Random Velocities: Low", { key ('B', alt) });
    add ("MIDI.VelocityMid", "MIDI", "Random Velocities: Mid", { key ('N', alt) });
    add ("MIDI.VelocityHigh", "MIDI", "Random Velocities: High", { key ('M', alt) });
    add ("MIDI.Humanize", "MIDI", "Humanize", { key ('H', alt) });   // (0.20.1: the editor's selected notes, else the selected parts)
    add ("MIDI.SoloEditor", "MIDI", "Solo Editor", {}, [this] { keyEditor.setSoloEditor (! keyEditor.isSoloEditorOn()); });   // (0.78.0)

    // Tools.
    for (const auto t : ArrangeView::tools)
    {
        const juce::String upper (ArrangeView::toolName (t));
        const auto name = upper.substring (0, 1) + upper.substring (1).toLowerCase();   // "Select"
        keyMap.add ({ "Tool." + name, "Tool", name + " Tool",
                      { KP ('0' + ArrangeView::toolKey (t), {}, (juce::juce_wchar) ('0' + ArrangeView::toolKey (t))) },
                      [this, t] { arrange.setTool (t); } });
    }
    add ("Tool.Previous", "Tool", "Previous Tool", { KP (KP::F9Key) });
    add ("Tool.Next", "Tool", "Next Tool", { KP (KP::F10Key) });

    // Zoom.
    add ("Zoom.In", "Zoom", "Zoom In", { key ('H') });
    add ("Zoom.Out", "Zoom", "Zoom Out", { key ('G') });
    add ("Zoom.Full", "Zoom", "Zoom Full", { key ('F', shift) });
    add ("Zoom.TracksTaller", "Zoom", "Zoom In Tracks", { key ('H', shift) });
    add ("Zoom.TracksShorter", "Zoom", "Zoom Out Tracks", { key ('G', shift) });
    add ("Zoom.AutoScroll", "Zoom", "Auto-Scroll On/Off", { key ('F') });

    // Project.
    add ("Project.TrackUp", "Project", "Select Track Above", { KP (KP::upKey) });
    add ("Project.TrackDown", "Project", "Select Track Below", { KP (KP::downKey) });
    add ("Project.RemoveTracks", "Project", "Remove Selected Tracks", {}, [this] { trackList.removeSelectedTracks(); });
    add ("Project.AddInstrument", "Project", "Add Instrument Track (PR7-K)", {}, [this] { studio.addTrack ({}); });
    add ("Project.AddAudioMono", "Project", "Add Audio Track (Mono)", {}, [this] { studio.addAudioTrack (false); });
    add ("Project.AddAudioStereo", "Project", "Add Audio Track (Stereo)", {}, [this] { studio.addAudioTrack (true); });
    add ("Project.AddFx", "Project", "Add FX Channel", {}, [this] { studio.addFxChannel (nullptr); });
    add ("Project.AddGroup", "Project", "Add Group Channel", {}, [this] { studio.addGroupChannel(); });
    add ("Project.AddTempo", "Project", "Add Tempo Track", {}, [this] { studio.addTempoTrack(); });

    // Window zones.
    add ("Window.Inspector", "Window", "Show/Hide Inspector", { KP (KP::F1Key), key ('L', ctrl | alt) });
    add ("Window.MidiEditor", "Window", "Open/Close MIDI Editor", { KP (KP::F2Key) });
    add ("Window.Mixer", "Window", "Open/Close Mixer", { KP (KP::F3Key) });
    add ("Window.MidiEditorPlace", "Window", "MIDI Editor Docked / Full Screen", { KP (KP::F2Key, juce::ModifierKeys (alt), 0) });
    add ("Window.MixerPlace", "Window", "Mixer Docked / Full Screen", { KP (KP::F3Key, juce::ModifierKeys (alt), 0) });
    add ("Window.ChordPads", "Window", "Show/Hide Chord Pads", { KP (KP::F4Key) });
    add ("Window.ChordPadsPlace", "Window", "Chord Pads Docked / Full Screen", {}, [this] { toggleChordPadsPlace(); });
    add ("Window.MediaBay", "Window", "Open/Close MediaBay", { KP (KP::F5Key) });   // (0.21.0)
    add ("Window.RightZone", "Window", "Show/Hide Right Zone (Media Rack)", { key ('R', ctrl | alt) });   // (0.22.0)
    add ("Window.Editor", "Window", "Open/Close Editor", { key ('E', ctrl) });
    add ("Window.InfoLine", "Window", "Show/Hide Info Line", {}, [this] { toggleInfoLine(); });   // (0.37.0)
    add ("Window.KeyCommands", "Window", "Key Commands...", {}, [this] { openKeyCommands(); });
    add ("Window.TempoEditor", "Window", "Open/Close Tempo Track Editor", { key ('T', ctrl) });
    add ("Window.AutomationPanel", "Window", "Open/Close Automation Panel", { KP (KP::F6Key) });   // (0.28.0)
    add ("Audio.VariAudio", "Audio", "VariAudio (the selected audio clip)", {}, [this]   // (0.34.0)
    {
        for (const auto id : arrange.getSelectedParts())
            if (const auto found = studio.findPart (id))
                if (studio.getProject().findTrack (found->first)->parts[(size_t) found->second].isAudio())
                {
                    openVariAudio (id);
                    return;
                }
    });
    add ("Window.ChannelEq", "Window", "Channel EQ (the selected track's, or the master's)", {}, [this]   // (0.43.0)
    {
        openEq (studio.isMasterSelected() ? pr7s::Engine::masterBus : studio.getSelectedTrack());
    });
    add ("Window.Progressions", "Window", "Progression Library", {}, [this] { openProgressions(); });   // (0.40.0)
    add ("Window.Composer", "Window", "Composer", {}, [this] { openComposer(); });   // (0.58.0)
    add ("Window.LearnMyBand", "Window", "Learn My Band (Read the Record)", {}, [this] { openBandRecord(); });   // (0.82.0)
    add ("Window.AudioAlignment", "Window", "Open/Close Audio Alignment Panel", {}, [this] { toggleAudioAlignment(); });   // (0.33.0)
    add ("Automation.Touch", "Automation", "Automation Mode: Touch", {}, [this] { studio.setAutomationMode (pr7s::Studio::AutomationSettings::Mode::touch); });
    add ("Automation.AutoLatch", "Automation", "Automation Mode: Auto-Latch", {}, [this] { studio.setAutomationMode (pr7s::Studio::AutomationSettings::Mode::autoLatch); });
    add ("Automation.CrossOver", "Automation", "Automation Mode: Cross-Over", {}, [this] { studio.setAutomationMode (pr7s::Studio::AutomationSettings::Mode::crossOver); });
    add ("Automation.Trim", "Automation", "Trim On/Off", {}, [this] { studio.setTrim (! studio.getAutomationSettings().trim); });
    add ("Automation.SuspendAll", "Automation", "Suspend All Read/Write", {}, [this] { studio.setAllSuspended (! studio.isAllSuspended()); });
    add ("Automation.FillToEnd", "Automation", "Fill to End", {}, [this] { studio.stepFill (pr7s::Studio::FillOption::toEnd); });
    add ("Automation.FillToPunch", "Automation", "Fill to Punch", {}, [this] { studio.stepFill (pr7s::Studio::FillOption::toPunch); });
    add ("Automation.FillLoop", "Automation", "Fill Loop", {}, [this] { studio.stepFill (pr7s::Studio::FillOption::loop); });
    add ("Window.Plugins", "Window", "Plug-ins...", {}, [this] { openPlugins(); });
    add ("Window.Settings", "Window", "Settings...", {}, [this] { openSettings(); });

    // The owner's assignments and macros, as last time; saved on each change.
    if (auto* settings = studio.getSettings())
        if (const auto xml = settings->getXmlValue ("keyCommands"))
            keyMap.fromXml (*xml);
    keyMap.onChange = [this] { saveKeyCommands(); };
}

void MainView::saveKeyCommands()
{
    if (auto* settings = studio.getSettings())
    {
        const auto xml = keyMap.toXml();
        settings->setValue ("keyCommands", xml.get());
        settings->saveIfNeeded();
    }
}
