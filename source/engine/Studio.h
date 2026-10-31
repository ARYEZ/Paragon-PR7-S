#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "Engine.h"
#include "PluginHost.h"
#include "Project.h"

#include <optional>

namespace pr7s
{
    struct MidiImport;   // (MidiImport.h)

    // The studio: the project, the engine that plays it, the instruments
    // and the audio and MIDI devices, and every action the screen offers
    // (tracks, parts, the transport, recording, undo, files). Message thread.
    // It tells its listeners (the screen) whenever anything changes.
    class Studio final : public juce::ChangeBroadcaster,
                         private juce::ChangeListener,
                         private juce::Timer
    {
    public:
        // `withDevices`: open the audio and MIDI devices and keep settings
        // (the app); off for tests, which call the engine's render themselves.
        explicit Studio (bool withDevices);
        ~Studio() override;

        const Project& getProject() const noexcept { return project; }
        Engine& getEngine() noexcept { return engine; }
        PluginHost& getPluginHost() noexcept { return *host; }
        juce::AudioDeviceManager& getDeviceManager() noexcept { return devices; }
        juce::PropertiesFile* getSettings() noexcept { return settings.get(); }
        // SETTINGS' RELOAD DRIVER: the audio device closed and opened again, so
        // a buffer size set in the driver's own panel is read (for drivers that
        // don't say they changed).
        void reloadAudioDevice();

        // Told just before a track's instrument is replaced or removed (0:
        // every track's, for a new or opened song), so its window can close
        // while the instrument still exists.
        std::function<void (int trackId)> onInstrumentGoing;

        //======================================================================
        // Tracks. A new track takes the next colour and is selected.
        // Returns its id (0, with `error` set, if the instrument didn't load).
        int addTrack (const Instrument&, juce::String* error = nullptr);
        // An audio track (mono: one input; stereo: a pair), recording from
        // the interface's first input(s). Returns its id.
        int addAudioTrack (bool stereo);
        void setTrackInput (int trackId, int input, bool stereo);
        void setMonitor (int trackId, bool);   // hear what comes in, through the track's effects
        // An instrument track's MIDI routing (the Inspector, 0.9.0): the input it
        // listens to when it's the live track (a device's name; empty: all
        // MIDI inputs), and the channel its notes go to its instrument on (1-16).
        void setMidiInput (int trackId, const juce::String& deviceName);
        void setMidiChannel (int trackId, int channel);
        void removeTrack (int trackId);   // (an FX or group channel: what was sent or routed to it goes back to the master)

        // FX and group channels (inspired by Cubase's workflow). An FX channel
        // takes what the tracks' sends give it, through its inserts (`effect`
        // goes in the first; none: empty); a group channel, the tracks whose
        // output is routed to it. Both go to the master. Returns its id (0, with
        // `error` set, if the effect didn't load).
        int addFxChannel (const juce::PluginDescription* effect, juce::String* error = nullptr);
        int addFxChannel (const Insert&, std::unique_ptr<juce::AudioPluginInstance>);   // one made elsewhere (tests)
        int addGroupChannel();
        // Where a track's sound goes: a group channel's id, or 0 for the master.
        void setOutput (int trackId, int groupId);
        // A track's (or group's) sends, by slot (0 to 7). The level can be
        // dragged (`undoable` off while dragging) and is written as automation
        // like a fader (grab and release Target::send with its slot).
        void setSend (int trackId, int slot, const Send&);
        void setSendLevel (int trackId, int slot, float db, bool undoable = true);
        void removeSend (int trackId, int slot);
        void setSendsBypass (int trackId, bool);
        bool changeInstrument (int trackId, const Instrument&, juce::String* error = nullptr);
        void renameTrack (int trackId, const juce::String&);
        void setMute (int trackId, bool);
        void setSolo (int trackId, bool);
        // `undoable` off: heard straight away but not an undo step yet (a
        // fader being dragged); the drag's last value, undoable, makes it one.
        void setVolume (int trackId, float db, bool undoable = true);
        void setPan (int trackId, float pan, bool undoable = true);
        // Record-arms one track (or none, with 0): recording goes there.
        // Inspired by Cubase's workflow (record follows the selected track), selecting arms too.
        void setArmed (int trackId);
        void selectTrack (int trackId);   // (it alone: the others are deselected)
        int getSelectedTrack() const noexcept { return selectedTrack; }   // the one clicked last (recording goes there)
        // Several selected (the manual's track selection): Ctrl-click adds or
        // removes one, Shift-click a run from the one clicked last (Shift-click
        // on a selected one deselects it). In the song's order.
        void toggleTrackSelected (int trackId);
        void selectTrackRun (int trackId);
        bool isTrackSelected (int trackId) const noexcept;
        std::vector<int> getSelectedTracks() const;

        // The tracks' order (the track list's, the mixer's): the tracks given
        // (kept in their order) move to before the track now at `beforeIndex`
        // in the list (the list's size: the end). One undo step; undo puts the
        // order back too.
        void moveTracks (const std::vector<int>& trackIds, int beforeIndex);
        // A copy of a track, straight under it: its parts, instrument and
        // effects (with their settings as they are now), sends, automation and
        // mix (the manual's Duplicate Tracks). Returns its id (0 if the
        // instrument didn't load). Several: each under its own; all selected.
        int duplicateTrack (int trackId);
        void duplicateTracks (const std::vector<int>& trackIds);
        void removeTracks (const std::vector<int>& trackIds);
        bool isTrackEmpty (int trackId) const;   // no parts and no automation points
        // Disable / enable tracks (the manual's Disable Selected Tracks): a
        // disabled one is silent and its instrument and effects are unloaded
        // (their settings kept, loaded again on Enable). Audio and instrument
        // tracks only (as the manual has it). Not an undo step.
        void setTracksDisabled (const std::vector<int>& trackIds, bool disabled);
        // Hide / show tracks (the left zone's VISIBILITY): out of the track
        // list, the arrangement and the mixer; still heard unless disabled.
        void setTracksHidden (const std::vector<int>& trackIds, bool hidden);
        bool isTrackShown (int trackId) const;   // (not hidden)
        std::vector<int> getShownTracks() const;   // in order
        std::vector<int> getEmptyTracks() const;

        //======================================================================
        // Parts (by track and index in its list).
        void addPart (int trackId, const Part&);
        void movePart (int trackId, int index, int toTrackId, double newStart);
        void copyPart (int trackId, int index, int toTrackId, double newStart);
        void deletePart (int trackId, int index);
        void resizePart (int trackId, int index, double newLength);
        // A part replaced as a whole (an audio clip trimmed or faded: start,
        // length, offset and fades together). One undo step.
        void updatePart (int trackId, int index, const Part&);
        // Where the part with this id is now: its track and index (none if it's gone).
        std::optional<std::pair<int, int>> findPart (int partId) const;

        // The editing tools (as the manual's), each one undo step:
        // Split a part at a song beat inside it. MIDI notes across the split
        // are cut in two (the manual's Split MIDI Events: our choice, so no
        // note is lost or left hanging past its part's end); an audio clip's
        // second half carries on from where the first stops (its fade-in goes
        // with the first, its fade-out with the second). The first half keeps
        // the part's place in the list; returns the second's index (or -1).
        int splitPart (int trackId, int index, double atBeat);
        // The Split tool with Alt: into equal pieces, as long as from its
        // start to `atBeat`, as many as fit (the last one what's left).
        void splitPartRepeatedly (int trackId, int index, double atBeat);
        // Split at Cursor (Alt+X): every part the playhead is inside, on every
        // track (or only `onlyPartId`'s, if given: the selected part).
        void splitAtCursor (int onlyPartId = 0);
        // Glue a part to the next one on its track (by start), or to all after
        // it: one part from its start to the last one's end, with their notes
        // (what was heard: the gaps between stay silent), named and muted as
        // the first. Audio: only a clip and the next one cut from the same
        // recording straight after it (a split glued back); anything else
        // returns false (an audio part holding several clips isn't there yet).
        bool glueParts (int trackId, int index, bool toAllFollowing);
        // The Mute tool (and the part's menu): muted parts stay, greyed, but aren't played.
        void setPartMuted (int trackId, int index, bool muted);
        void setPartsMuted (const std::vector<int>& partIds, bool muted);   // several at once (one step)
        // The Erase tool with Alt: the part and all after it on its track.
        void deletePartsFrom (int trackId, int index);

        // Range Selection (the manual's Range Editing): a stretch of time
        // across some tracks, independent of the parts. Whatever a range
        // edit touches is cut at its edges first (a note across an edge is
        // split, as Split does), so only what's inside is affected. Each one
        // undo step. FX and group channels (no parts) are passed over.
        struct Range
        {
            double start = 0.0, end = 0.0;   // beats
            std::vector<int> trackIds;       // top to bottom
            double length() const noexcept { return end - start; }
            bool isEmpty() const noexcept { return end <= start || trackIds.empty(); }
        };
        // What Copy or Cut keep: each track's slices, from the range's start.
        struct RangeClip
        {
            double length = 0.0, origin = 0.0;    // its length; where it came from (Paste at Origin)
            int originTrack = 0;                  // the first track it came from
            std::vector<std::vector<Part>> tracks;   // per track of the range, top to bottom
            bool isEmpty() const noexcept { return tracks.empty(); }
        };
        void splitRange (const Range&);                        // Shift+X: parts split at its edges
        void cropRange (const Range&);                         // parts across its edges cut down to it
        void deleteRange (const Range&, bool closeGap);        // Delete; Delete Time (Shift+Backspace): what's after moves back
        RangeClip copyRange (const Range&) const;              // Ctrl+C
        RangeClip cutRange (const Range&, bool closeGap);      // Ctrl+X; Cut Time (Ctrl+Shift+X)
        void insertSilence (const Range&);                     // Ctrl+Shift+E: what's at or after its start moves on by its length
        // Paste (Ctrl+V): at a beat, from a track down (a clip's tracks onto
        // the tracks below it in order; audio only onto audio tracks, MIDI
        // onto instrument tracks, else that track's slices are left out).
        // `insertTime` (Paste Time, Ctrl+Shift+V) makes room first.
        void pasteRange (const RangeClip&, double atBeat, int firstTrackId, bool insertTime);
        // Its contents dragged to a new start and first track (Alt: copied); Duplicate (Ctrl+D): copied straight after it.
        void moveRange (const Range&, double newStart, int newFirstTrackId, bool copy);
        void duplicateRange (const Range&);
        // A part's notes, replaced (the key editor's every edit: one undo step each).
        // Kept in the order given, so the editor's selection still points at the same notes.
        void setNotes (int trackId, int index, std::vector<Note>);

        //======================================================================
        // Recordings. They're files in the song's audio folder ("<song> Audio"
        // beside the song; before it's saved, a folder in Documents), loaded
        // whole, at the engine's sample rate.
        struct AudioData
        {
            juce::AudioBuffer<float> sound;
            std::vector<float> peaks;   // the loudest sample (either side) in each stretch of peakStep samples, for drawing
            static constexpr int peakStep = 256;
            double seconds() const noexcept;
            double sampleRate = 48000.0;
        };
        std::shared_ptr<const AudioData> getAudio (const juce::String& file);   // none if it can't be read
        juce::File getAudioFolder() const;
        // The interface's delay, in and out: recordings are moved earlier by it,
        // so they land where they were played. Tests set it; else the device's.
        void setLatencyForTests (double seconds) { latencyOverride = seconds; }

        //======================================================================
        // The mixer: each track's inserts and the master's (`busId`: a
        // track's id, or Engine::masterBus), 16 slots each, an effect in a
        // slot of its own (a slot can be empty). Adding, replacing and
        // removing effects isn't undoable (as with instruments); bypass isn't
        // either; the master's volume is.
        const std::vector<Insert>* getInserts (int busId) const;   // none if there's no such track
        // Into an empty slot (-1: the one after the last effect).
        bool addInsert (int busId, const juce::PluginDescription&, juce::String* error = nullptr, int slot = -1);
        bool addInsert (int busId, const Insert&, std::unique_ptr<juce::AudioPluginInstance>, int slot = -1);   // one made elsewhere (tests)
        // The slots up to this one (from 0) are before the fader, the rest after
        // it (as the manual's "Set as last Pre-Fader Slot"); -1: none before it.
        void setLastPreFaderSlot (int busId, int slot);
        int getPreFaderSlots (int busId) const;
        bool replaceInsert (int busId, int slot, const juce::PluginDescription&, juce::String* error = nullptr);
        void removeInsert (int busId, int slot);   // its slot left empty (the others stay where they are)
        void setInsertBypass (int busId, int slot, bool);
        void setMasterVolume (float db, bool undoable = true);
        // Told just before an effect is deleted (nullptr: every effect, for a
        // new or opened song), so its window closes while it still exists.
        std::function<void (juce::AudioProcessor*)> onEffectGoing;

        //======================================================================
        // Automation (inspired by Cubase's workflow). `busId`: a track's id, or
        // Engine::masterBus. A track's lanes ("automation tracks") are shown
        // under it while its automation is open; each lane shows one control.
        const std::vector<AutomationLane>* getAutomation (int busId) const;
        bool isAutomationOpen (int busId) const;
        bool getAutomationRead (int busId) const;
        bool getAutomationWrite (int busId) const;
        void setAutomationRead (int busId, bool);
        void setAutomationWrite (int busId, bool);   // turns R on too
        // The header's R and W: lit when any track's (or the master's) is; set them all.
        bool anyAutomationRead() const;
        bool anyAutomationWrite() const;
        void setAutomationReadAll (bool);
        void setAutomationWriteAll (bool);

        // A control that can be automated, as the lanes' menus list it.
        struct AutomationControl
        {
            AutomationLane::Target target = AutomationLane::Target::volume;
            int slot = 0, parameter = 0;
            juce::String name, group;   // group: "" for the mix, "Instrument: <name>", "Insert 2: <name>"
            AutomationLane asLane() const;
        };
        // Volume, pan and mute (the master: volume), then the instrument's knobs, then each insert's.
        std::vector<AutomationControl> getAutomationControls (int busId) const;
        juce::String getLaneName (int busId, const AutomationLane&) const;
        // The control's setting now (0 to 1), as the static value line shows it.
        float getControlValue (int busId, const AutomationLane&) const;

        // Show/Hide Automation. Opening shows the lanes that were open, or
        // those with points, or else a lane for the first control (volume).
        void setAutomationOpen (int busId, bool);
        void showAllUsedAutomation();          // every track's (and the master's) lanes with points
        // + (Append Automation Track): a lane for the next control not shown, after `laneIndex`.
        void appendAutomationLane (int busId, int laneIndex);
        // The lane at `laneIndex` shows another control instead. Non-destructive:
        // the control it showed keeps its points, hidden (shown again by choosing it).
        void setLaneControl (int busId, int laneIndex, const AutomationControl&);
        void removeAutomationParameter (int busId, int laneIndex);   // the lane and its points
        void removeUnusedAutomation (int busId);                      // lanes without points
        void setLaneMuted (int busId, int laneIndex, bool);           // Mute Automation
        // A lane's points replaced (an edit with the mouse: one undo step). Turns R on.
        void setAutomationPoints (int busId, int laneIndex, std::vector<AutomationPoint>);

        // Writing automation (as the manual has it): with W on, while playing,
        // touching a control writes it. The screen says when a control is
        // grabbed and let go; its values come through setVolume, setPan,
        // setMute, setMasterVolume (and a plugin's knobs through setKnob).
        // Touch: writing stops when it's let go, and the control returns to
        // what it was (over `returnSeconds`). On/off switches (mute) are always
        // Auto-Latch: they write until playing stops. Every pass also stops on
        // stop, W off, a jump (the cycle going round, a click in the ruler).
        // Each pass is one undo step; its points are thinned out.
        static AutomationLane control (AutomationLane::Target, int slot = 0, int parameter = 0);
        void grabControl (int busId, const AutomationLane& control);
        void releaseControl (int busId, const AutomationLane& control);
        void setKnob (int busId, const AutomationLane& control, float value);   // a plugin's knob moved by hand
        bool isWritingAutomation (int busId) const;   // a pass going on it (its W shows it)
        static constexpr double returnSeconds = 0.1;  // our choice: the manual gives Return Time no default

        //======================================================================
        // The song.
        void setTempo (double bpm);
        void setSignature (int numerator, int denominator);
        void setCycle (double start, double end);
        void setCycleOn (bool);
        void setClick (bool);

        //======================================================================
        // The transport.
        void play();
        void stop();      // a second stop goes back to where playing started, inspired by Cubase's workflow
        void record();    // records into the armed track (arming the selected one if none is)
        void togglePlay() { engine.isPlaying() ? stop() : play(); }
        void goToStart();
        void setPosition (double beats);

        // While recording: the part taking shape, for the screen (none otherwise).
        std::optional<std::pair<int, Part>> getRecordingPreview() const;

        //======================================================================
        // Undo covers the parts and the song's settings (tempo, signature,
        // cycle, click) and the tracks' names and mix; not adding or removing tracks.
        void undo();
        void redo();
        bool canUndo() const noexcept { return undoIndex > 0; }
        bool canRedo() const noexcept { return undoIndex + 1 < (int) undoStates.size(); }

        //======================================================================
        // Files.
        void newProject();
        bool save (const juce::File&);
        // `problems`: instruments that couldn't be loaded (the project opens anyway).
        bool open (const juce::File&, juce::String& problems);
        // A standard MIDI file: a Paragon Synth track for each of its tracks
        // (a type-0 file: each channel), its notes in a part (whole bars).
        // `newSong`: a new song first, with the file's tempo and time
        // signature; else into this one at `atBeat` (its tempo kept, the
        // notes on its beats: the manual's "Ignore Master Track Events on
        // Merge"). Returns how many tracks it added (0, with `error` set, if none).
        int importMidi (const juce::File&, bool newSong, double atBeat, juce::String& error);
        // Audio file import (the manual's File > Import > Audio File): WAV,
        // AIFF, FLAC, Ogg Vorbis or MP3. Each file is copied into the song's
        // audio folder (the manual's Copy File to Project Folder: a WAV or AIFF
        // as it is, a compressed one written as a 24-bit WAV) and becomes a
        // clip at `atBeat`, on `trackId` if it's an audio track, else on a new
        // audio track (stereo if the first file is); several files one after
        // another on that track. Returns the track's id (0, with `error` set,
        // if none could be read; files that couldn't are listed in `error`).
        // The clips are one undo step.
        static bool isAudioFile (const juce::File&);
        int importAudio (const juce::Array<juce::File>&, int trackId, double atBeat, juce::String& error);
        int importMidi (const MidiImport&, bool newSong, double atBeat);
        juce::File getFile() const noexcept { return file; }
        bool isModified() const noexcept { return modified; }

        //======================================================================
        // Export: the song (or part of it) to a stereo WAV, through everything
        // that's heard (instruments, clips, effects, the master), faster than
        // real time. The interface is let go meanwhile, and everything is as
        // it was afterwards.
        struct ExportOptions
        {
            double start = 0.0, end = 0.0;   // beats
            double sampleRate = 0.0;         // 0: the interface's
            int bitsPerSample = 24;          // 16, 24, or 32 (32-bit float)
            bool click = false;
            // After the end, until what's sounding (reverbs, delays, a held
            // note's release) has died away, at most this long; nothing new starts.
            double tailSeconds = 30.0;
            // What's played can be changed first (Render in Place: only what's rendered, as it asks).
            std::function<void (Project&)> prepareSong;
        };
        // In three steps, so the middle one can run on another thread behind a
        // progress bar: begin (message thread), render (any thread; false:
        // cancelled, or the disk failed), end (message thread; a file not
        // finished is deleted).
        bool beginExport (const juce::File&, const ExportOptions&, juce::String& error);
        bool renderExport (std::function<bool (double progress)> keepGoing = {});
        void endExport();
        bool isExporting() const noexcept { return exportJob != nullptr; }
        // All three, here and now (tests; and short songs).
        bool exportMix (const juce::File&, const ExportOptions&, juce::String& error);

        // Render in Place (the manual's Rendering Audio and MIDI): stretches
        // of instrument or audio tracks rendered to audio files (in the song's
        // audio folder) as clips on a new audio track under each source track.
        struct RenderOptions
        {
            // As Separate Events (a clip for each part), As Block Events (parts
            // end to end joined), As One Event (one clip from the first part's
            // start to the last one's end).
            enum class Mode { separate, blocks, one };
            Mode mode = Mode::separate;
            // Dry: the instrument (or the clips) alone; the new track gets the
            // source's effects (with their settings), fader, pan, sends and
            // output. Channel Settings: through the inserts, sends (their FX
            // channels) and groups; the new track gets the fader and pan.
            // Complete Signal Path: all of it, fader and pan too; the new track
            // at 0 dB, in the middle, no effects. + Master FX: the master's
            // effects and fader too.
            enum class Processing { dry, channelSettings, completeSignalPath, completeWithMaster };
            Processing processing = Processing::channelSettings;
            // The tail after each stretch (for reverbs and delays to die away): off, in bars, or in seconds.
            enum class Tail { off, bars, seconds };
            Tail tail = Tail::bars;
            double tailSize = 1.0;
            int bitsPerSample = 24;   // 16, 24 or 32 (float)
            // The source: kept as it was, muted (the parts rendered; whole tracks: the track), or removed (whole tracks).
            // Or (whole tracks) disabled: its plugins unloaded, as Freeze (0.9.1).
            enum class Source { keep, mute, remove, disable };
            Source source = Source::keep;
            bool hideSource = false;   // whole tracks: hidden after (the manual's Hide Source Tracks)
        };
        struct RenderSource
        {
            int trackId = 0;
            double start = 0.0, end = 0.0;   // beats: the stretch (its parts are cut at its edges)
            bool wholeTrack = false;         // a track chosen as a whole (Source: the track muted or removed)
        };
        // Returns the new audio tracks' ids (none, and `error` set, if nothing could be rendered).
        std::vector<int> renderInPlace (const std::vector<RenderSource>&, const RenderOptions&, juce::String& error);

    private:
        void changed (bool undoable = true);
        void numberParts();   // an id for every part that hasn't one
        std::vector<Insert>* insertsOf (int busId);
        std::vector<AutomationLane>* lanesOf (int busId);
        juce::AudioPluginInstance* pluginFor (int busId, const AutomationLane&) const;
        void insertGone (int busId, int slot, bool shiftUp);   // its lanes go, here and in the undo states

        // A pass of writing automation: the control, where it began, what was
        // set before, and the values written (song beats).
        struct WritePass
        {
            int busId = 0;
            AutomationLane control;
            double start = 0.0, lastBeat = 0.0;
            int cycleCount = 0;      // the engine's when it began: the cycle coming round ends it
            float before = 0.0f;
            std::vector<AutomationPoint> points;
            bool released = false;   // let go (an on/off switch latches on until stop)
        };
        std::vector<WritePass> passes;

        // Every plugin's knobs are listened to, so turning one in its window
        // (W on, playing) writes it. Stopped before a plugin is deleted.
        class KnobWatcher;
        std::map<juce::AudioProcessor*, std::unique_ptr<KnobWatcher>> watchers;
        std::shared_ptr<bool> alive = std::make_shared<bool> (true);   // for messages from plugins' threads
        void watchPlugins();
        void unwatch (juce::AudioProcessor*);
        void unwatchAll();
        bool writeValue (int busId, const AutomationLane& control, float value);   // false: no pass on it
        void punchOut (size_t pass, double endBeat);
        void punchOutAll();
        void setControlValue (int busId, const AutomationLane& control, float value);   // the static setting, without writing
        void effectGoing (int busId, int slot);
        void removeAllEffects();
        void recordUndoState();
        void restore (const Project&);
        static void restoreLanes (std::vector<AutomationLane>& lanes, const std::vector<AutomationLane>& then);
        void updateLiveTrack();
        void finishRecording();
        void finishAudioRecording();
        double recordingLatency() const;
        void buildRecordingNotes (std::vector<Note>& notes, double partStart, double endBeat) const;
        void timerCallback() override;
        void changeListenerCallback (juce::ChangeBroadcaster*) override;   // the device settings changed: keep them

        std::unique_ptr<juce::PropertiesFile> settings;
        std::unique_ptr<PluginHost> host;
        juce::AudioDeviceManager devices;
        bool withDevices = false;
        Engine engine;

        Project project;
        juce::File file;
        bool modified = false;
        int selectedTrack = 0;
        std::vector<int> selectedTracks;   // with selectedTrack in it (when there is one)
        int nextPartId = 1;

        // Recording: where it started, into which track, and what's come in so far.
        bool wasRecording = false;
        double recordStart = 0.0, playStart = 0.0;
        int recordTrack = 0;
        std::vector<RecordedEvent> recorded;

        // Recording audio: the file being written, and its writer.
        juce::AudioFormatManager formats;
        juce::TimeSliceThread writerThread { "Paragon PR7-S recorder" };
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> audioWriter;
        juce::File audioRecording;
        std::map<juce::String, std::shared_ptr<const AudioData>> audioCache;
        double audioCacheRate = 0.0;
        juce::File unsavedAudioFolder;
        std::optional<double> latencyOverride;

        struct ExportJob
        {
            juce::File file;
            std::unique_ptr<juce::AudioFormatWriter> writer;
            juce::int64 total = 0, maxTail = 0;   // samples: the range, and the most ring-out after it
            double previousRate = 0.0, previousPosition = 0.0;
            int previousBlockSize = 512;
            bool finished = false;
        };
        std::unique_ptr<ExportJob> exportJob;

        std::vector<Project> undoStates;
        int undoIndex = -1;
    };
}
