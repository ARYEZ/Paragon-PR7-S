#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "ProjectLibrary.h"
#include "Engine.h"
#include "PluginHost.h"
#include "Project.h"
#include "Lines.h"
#include "MediaBay.h"

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
        // How strongly a file dragged in lights where it will land (0.18.2, the owner asked: a
        // setting in the Hub): 0.1 faint to 1 solid; settings key "dropHighlight", default 0.6.
        float getDropHighlight() const;
        void setDropHighlight (float);
        static constexpr float defaultDropHighlight = 0.6f;
        // The Hub's songs (0.12.0): recent ones, the project location, templates.
        ProjectLibrary& getLibrary() noexcept { return *library; }
        // Create Empty (the Hub): a new song, saved at once in a folder of its
        // own at the project location (so its recordings go beside it).
        bool createSong (const juce::String& name, juce::String& error);
        // A new song from a template: the template's tracks, plug-ins and mix, not saved yet.
        bool newFromTemplate (const juce::File& templateFile, juce::String& problems);
        // This song as a template (Templates folder); the song itself stays as it is.
        bool saveAsTemplate (const juce::String& name, juce::String& error);
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
        // The colour menu (0.17.3): the tracks given in a family colour (one of the
        // palette's: FamilyColours), or with none, their automatic colour back (red,
        // pink, orange in turn by their place). One undo step.
        void setTracksColour (const std::vector<int>& trackIds, std::optional<juce::Colour>);
        void setMute (int trackId, bool);
        void setSolo (int trackId, bool, bool exclusive = false);   // exclusive: every other solo off (Ctrl-click, the manual's)
        // The manual's Deactivate All Solo States / Mute States (0.9.9).
        bool anySolo() const;
        bool anyMute() const;
        void deactivateAllSolo();
        void deactivateAllMute();
        // `undoable` off: heard straight away but not an undo step yet (a
        // fader being dragged); the drag's last value, undoable, makes it one.
        void setVolume (int trackId, float db, bool undoable = true);
        void setPan (int trackId, float pan, bool undoable = true);
        // Record-arms one track (or none, with 0): recording goes there.
        // Inspired by Cubase's workflow (record follows the selected track), selecting arms too.
        void setArmed (int trackId);
        void selectTrack (int trackId);   // (it alone: the others are deselected)
        // The master's row clicked (0.9.12): the Inspector shows the master until a track is chosen.
        void selectMaster();
        bool isMasterSelected() const noexcept { return masterSelected; }
        int getSelectedTrack() const noexcept { return selectedTrack; }   // the one clicked last (recording goes there)
        // Several selected (the manual's track selection): Ctrl-click adds or
        // removes one, Shift-click a run from the one clicked last (Shift-click
        // on a selected one deselects it). In the song's order.
        void toggleTrackSelected (int trackId);
        void selectTrackRun (int trackId);
        bool isTrackSelected (int trackId) const noexcept;
        std::vector<int> getSelectedTracks() const;

        // Link groups (0.17.0, the manual's Linking Channels). `linkTracks` makes
        // a group of the tracks given (two or more; FX and group channels too;
        // any already in a group leave it), with these settings (`name` empty:
        // "Link <n>"); its id, or 0. Unlinking a group: `removeLinkGroup`; one
        // channel in or out of a group: `setTrackLinkGroup` (0: none; a group
        // left with one channel goes). All undo steps; saved with the song.
        int linkTracks (const std::vector<int>& trackIds, const LinkGroup& settings);
        void removeLinkGroup (int groupId);
        void setTrackLinkGroup (int trackId, int groupId);
        void setLinkGroupSettings (int groupId, const LinkGroup& settings);   // (its id and channels kept)
        const LinkGroup* getLinkGroupOf (int trackId) const;
        // The mixer's link keys: SUS (links suspended: each channel on its own;
        // holding Alt does it for a moment), ABS (linked levels set the same
        // value, not moved by the same amount), Q-LINK (whatever is touched on a
        // selected channel goes to every selected channel; holding Shift+Alt does
        // it for a moment). Not saved (as the mixer's toolbar).
        void setLinksSuspended (bool on) { linksSuspended = on; sendChangeMessage(); }
        bool areLinksSuspended() const noexcept { return linksSuspended; }
        void setLinksAbsolute (bool on) { linksAbsolute = on; sendChangeMessage(); }
        bool areLinksAbsolute() const noexcept { return linksAbsolute; }
        void setQLink (bool on) { qLink = on; sendChangeMessage(); }
        bool isQLinkOn() const noexcept { return qLink; }
        // The keys held (the screen sets these; tests leave them unset).
        std::function<bool()> isSuspendHeld, isQLinkHeld;
        // The tracks a change of `what` on this one goes to as well: itself first,
        // then its group's (linking `what`, not suspended), then (Q-Link) the other
        // selected tracks when it's selected.
        std::vector<int> linkedTracks (int trackId, LinkGroup::What what) const;

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
        // Chord pads (0.14.0, the manual's "Using Chord Pads"). Kept with the song;
        // not undo steps (the manual: removing assignments can't be undone). A
        // locked pad takes no edits. `pads` are pad indices.
        const ChordPads& getChordPads() const noexcept { return project.chordPads; }
        void setChordPads (const ChordPads&);
        PadPlayer& getPadPlayer() noexcept { return padPlayer; }
        // The trigger keys (C1-B1) play the pads only while they're shown (our
        // choice: a drum part's C1 must stay a kick when the pads are put away).
        void setPadsActive (bool);
        bool arePadsActive() const noexcept { return padsActive; }
        void assignChord (int pad, const Chord&);
        void removeChords (const std::vector<int>& pads);
        void transposePads (const std::vector<int>& pads, int semitones);
        void changePadTensions (const std::vector<int>& pads, int direction);
        void changePadVoicings (const std::vector<int>& pads, int direction);
        void setPadsLocked (const std::vector<int>& pads, bool);
        void setPadsAdaptive (const std::vector<int>& pads, bool);
        void setAdaptiveReference (int pad);           // -1: none
        void swapPads (int a, int b);                  // the reference stays where it was (the manual)
        void copyPad (int from, int to);
        void setPadsRootKey (int key, bool transposePads);
        // A key for the pads (0.15.0): its chords onto them (locked pads keep theirs), or
        // the pads transposed to it, or just the names and numerals in it.
        enum class PadsKeyChange { fillWithChords, transposePads, renameOnly };
        void setPadsKey (const Key&, PadsKeyChange);
        void auditionChord (const Chord&);                 // played for a moment (the circle, the suggestions)
        void setPadVoicing (int pad, int voicing);        // an index into namedVoicings (adaptive off)
        bool assignFromNotes (int pad, const std::vector<int>& notes);   // recognised; false if no chord
        // Assign from MIDI Input: the keys played (outside the trigger range) go to these pads, one chord each.
        void setAssignFromInput (std::vector<int> pads);
        const std::vector<int>& getAssignFromInput() const noexcept { return assignTargets; }
        void padKeyPlayed (int note, bool on);          // (the message thread; from the devices, or tests)
        int getPadVoicing (int pad) const;              // the voicing it plays now (an index), -1: none
        int getPadVoicingCount (int pad) const;
        std::vector<int> getPadNotes (int pad) const;   // what it would play now
        void auditionPad (int pad);                     // played for a moment (acoustic feedback)
        // Drag a pad onto a track: a one-bar MIDI part there (the manual's Creating MIDI Parts from Chord Pads).
        int addPartFromPad (int pad, int trackId, double beat);
        enum class PadsPresetLoad { chordsAndPlayers, chordsOnly, playersOnly };
        void loadScalePreset (ChordPads::Scale, int key, PadsPresetLoad = PadsPresetLoad::chordsAndPlayers);
        bool saveChordPadsPreset (const juce::File&) const;
        bool loadChordPadsPreset (const juce::File&, PadsPresetLoad = PadsPresetLoad::chordsAndPlayers);
        static juce::File chordPadsPresetFolder();      // Documents/Paragon PR7-S/Chord Pads

        // The chord track (0.16.0, the manual's "Chord Functions"). Its chords and
        // scales are undo steps; its scales follow the chords until one is set by hand.
        const ChordTrack& getChordTrack() const noexcept { return project.chordTrack; }
        void addChordTrack();
        void removeChordTrack();                          // hidden; its chords kept
        int addChordEvent (double beat, const Chord&);    // its index (one at that beat replaced)
        void setChordEvents (std::vector<ChordEvent>);    // (a drag: one undo step)
        // A progression (0.17.2): one chord a bar from `atBeat` (the bar it's in), what was
        // there replaced; the chord track shown. One undo step.
        void addProgressionToChordTrack (const std::vector<Chord>&, double atBeat);

        // Bass and melody ideas (0.18.0, the owner asked). The chords they follow: the chord
        // track's events given (the selection), else those in the cycle when it's on and
        // holds some, else all; each until the next (the last a bar), with the key there.
        std::vector<ChordSpan> getLineChords (const std::vector<int>& selectedEvents = {}) const;
        // An idea as a part under its chords: onto `ontoTrackId` when it can take MIDI (0.19.0:
        // the selected track, the owner's guitar VST), else onto a new instrument track (the
        // studio's synth) named for the part (Bass, Melody, Piano, Guitar, Lead Guitar). The
        // track's id; one undo step.
        int addLinePart (const LineIdea&, const std::vector<ChordSpan>&, int ontoTrackId = 0);
        static juce::String linePartName (LineIdea::Part);
        // The guitar ideas' tuning (0.19.0; settings key "guitarTuning", default 7-string drop A).
        juce::String getGuitarTuning() const;
        void setGuitarTuning (const juce::String&);
        // The guitar ideas' velocities (0.19.3, the VELOCITIES panel; settings key "guitarVelocities").
        GuitarVelocities getGuitarVelocities() const;
        void setGuitarVelocities (const GuitarVelocities&);
        // Humanize (0.20.0). IDEAS: its amount (0-1, settings "ideasHumanize", default 0: on the
        // grid as before) and FEEL ("ideasFeel", default on). The MIDI editor: its amount
        // ("humanizeAmount", default 0.5) and whether velocities keep to the guitar library's
        // layers ("humanizeLayers", default on).
        float getIdeasHumanize() const;
        void setIdeasHumanize (float);
        bool getIdeasFeel() const;
        void setIdeasFeel (bool);
        float getHumanizeAmount() const;
        void setHumanizeAmount (float);
        bool getHumanizeKeepsLayers() const;
        void setHumanizeKeepsLayers (bool);
        // Every note of these MIDI parts humanized (0.20.1: the Humanize key with parts selected in
        // the arrangement): the MIDI editor's amount and keep-layers; each part its own way; audio
        // and frozen tracks' parts left alone. One undo step; false if nothing changed.
        bool humanizeParts (const std::vector<int>& partIds, float amount = -1.0f);
        // An idea heard with its chords (soft, underneath) through the live track's instrument,
        // at the song's tempo; the chords alone with no idea. Stopped by stopPreview (or the next).
        void previewLine (const LineIdea*, const std::vector<ChordSpan>&, bool withChords = true);
        void stopPreview();
        bool isPreviewing() const noexcept;
        double getPreviewBeat() const noexcept;   // beats from the first chord while previewing
        // A progression onto the pads from `firstPad` on (locked pads skipped); how many placed.
        int assignProgressionToPads (const std::vector<Chord>&, int firstPad);
        void setChordEventChord (int index, const Chord&);
        void removeChordEvents (std::vector<int> indices);
        void setScaleEvents (std::vector<ScaleEvent>);    // by hand: the scales stop following the chords
        void setAutoScales (bool);
        Key chordTrackKeyAt (double beat) const;          // the scale in force (else the pads' key)
        // A new chord at `beat`: the best that could follow the chord before (else the key's I).
        Chord suggestChordAt (double beat) const;
        // The chords (or those at these indices) as a MIDI part, voiced with the least
        // movement: onto that track, or a new instrument track (0); the part's id.
        int chordsToMidi (int trackId, std::vector<int> indices = {});
        // The chords heard in these parts onto the chord track (replacing those over their span).
        int chordsFromParts (const std::vector<int>& partIds, double step = 1.0);
        // The manual's Assign from Chord Track: its chords, each once, onto the pads in order.
        void assignPadsFromChordTrack();

        // Freeze (0.9.8, the manual's Freeze Channel): the track's instrument and
        // inserts (not its fader, pan or sends) rendered from the song's start to
        // its last part's end plus `tailSeconds`, into "<audio folder>/Freeze";
        // then its plugins unloaded and the file played instead; its parts
        // locked (any edit to them is put back), its inserts and instrument
        // can't be changed. Instrument and audio tracks. Not an undo step.
        bool freezeTrack (int trackId, double tailSeconds, juce::String& error);
        // Unfreeze: the plugins loaded again; the freeze file kept or deleted.
        void unfreezeTrack (int trackId, bool deleteFile);
        bool isFrozen (int trackId) const;
        // Several renders in a row (Render in Place's stretches, freezing the
        // selected tracks): between beginBatch and endBatch the plugins stay
        // offline and the interface off, switched back once at the end (0.9.9).
        void beginBatch();
        void endBatch();
        // Whether a plugin has a window of its own: asked of it once, then remembered (0.9.9).
        bool hasEditor (const juce::AudioProcessor*);
        bool isTrackShown (int trackId) const;   // (not hidden)
        std::vector<int> getShownTracks() const;   // in order
        std::vector<int> getEmptyTracks() const;

        //======================================================================
        // Parts (by track and index in its list).
        void addPart (int trackId, const Part&);
        void movePart (int trackId, int index, int toTrackId, double newStart);
        void copyPart (int trackId, int index, int toTrackId, double newStart);
        void deletePart (int trackId, int index);
        // Several parts at once (0.9.2, the Object Selection tool's selection),
        // by part id; each one undo step. Moved (or, `copy`, copied) by beats
        // and by tracks among those shown, keeping where they are to each
        // other; one whose target track is of another kind (or a channel)
        // stays on its own track, as the manual has it.
        void deleteParts (const std::vector<int>& partIds);
        void moveParts (const std::vector<int>& partIds, double beatDelta, int trackDelta, bool copy);
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
        // Parts copied as a clip (rows: the shown tracks from the first part's
        // to the last's; origin: where the earliest starts), for pasteRange
        // (Ctrl+V at the playhead on the selected track; Paste at Origin).
        RangeClip copyParts (const std::vector<int>& partIds) const;
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
        // A file read whole at the engine's rate, not kept (the MediaBay's previews, 0.21.0).
        std::shared_ptr<const AudioData> readAudio (const juce::File&);
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

        // Quick Controls (0.9.6, the manual's Track Quick Controls): a track's
        // 8 slots, each one of its controls (getAutomationControls) or none
        // (an undo step). Their values are the controls' travel (0 to 1); moving
        // one moves the control itself, so R/W automation writes it as usual
        // (grabControl / releaseControl around a drag).
        void setQuickControl (int trackId, int index, std::optional<AutomationControl>);
        // Track presets (0.9.8, the manual's Track Presets): a track's sound and
        // channel settings (its kind, instrument and inserts with their
        // settings, volume, pan, colour, Quick Controls, MIDI channel, input) as
        // XML, without its parts, automation, sends or output. Applied only to a
        // track of the same kind (not an undo step, as the manual has it); or a
        // new track from one, under the selected track.
        std::unique_ptr<juce::XmlElement> makeTrackPreset (int trackId) const;
        bool applyTrackPreset (int trackId, const juce::XmlElement& preset, juce::String& error);
        int addTrackFromPreset (const juce::XmlElement& preset, juce::String& error);
        static juce::File trackPresetFolder();   // Documents/Paragon PR7-S/Track Presets
        static constexpr const char* trackPresetExtension = ".pr7track";
        std::optional<AutomationLane> getQuickControl (int trackId, int index) const;
        void setQuickControlValue (int trackId, int index, float travel, bool undoable = true);
        float getQuickControlValue (int trackId, int index) const;
        juce::String getQuickControlName (int trackId, int index) const;   // empty: none
        // The instrument's first 8 knobs into the slots (as the manual has an
        // instrument track's main parameters assigned; our way: a menu item).
        void assignInstrumentQuickControls (int trackId);
        // Show Automated QC Assignments: the lanes of the slots' controls that have points, opened.
        void showAutomatedQuickControls (int trackId);
        static constexpr double returnSeconds = 0.1;  // our choice: the manual gives Return Time no default

        //======================================================================
        // The song.
        void setTempo (double bpm);   // the fixed tempo (the tempo track off)
        void setSignature (int numerator, int denominator);

        //======================================================================
        // The tempo track (0.10.0, the manual's Tempo Track): added once
        // (Add Track > Tempo), on (Activate Tempo Track: the song follows its
        // events) or off (the fixed tempo). Each edit is an undo step.
        // Adding it the first time: one event at the start with the fixed
        // tempo, and it's turned on (our choice: why add it otherwise).
        void addTempoTrack();
        void removeTempoTrack();   // hidden and off (its events kept: added again, they're back)
        void setTempoTrackOn (bool);
        // A new event (the Type of New Tempo Points decides ramp or step:
        // Automatic, as the event before it). At an event's beat: that one's tempo
        // changes. Returns its index. Not while the track is locked.
        int addTempoEvent (double beat, double bpm);
        // All of them at once (a drag, the list's edits: one undo step). Sorted,
        // the first kept at the start. Not while locked.
        void setTempoEvents (std::vector<TempoEvent>);
        // These (indices) removed; never the first (the manual's: it can't be).
        void removeTempoEvents (std::vector<int> indices);
        // The tempo where the cursor is: the tempo track on, the event in effect
        // there changes (the manual's Current Tempo); off, the fixed tempo.
        void setTempoAtCursor (double bpm);
        // The manual's Insert Tempo Event at Cursor.
        int insertTempoEventAtCursor (double bpm);
        void setTempoLocked (bool);
        void setNewTempoType (Project::NewTempoType);
        void setTempoView (double low, double high);   // the curve's shown range (not undo steps)
        // The tempo at the playhead (the transport shows it).
        double getTempoNow() const;
        // The manual's Process Tempo (0.13.0): the stretch from `start` to
        // `end` (beats; the locators) made to last `newSeconds`, by scaling the
        // tempo inside it (every event there, and its ramps, by one factor: the
        // time over a ramp scales exactly with it). Before and after, the tempo
        // stays as it was: events at the edges hold it (one just past the end
        // brings it back). The tempo track is turned on (and added if it isn't).
        // One undo step. False if the range is empty or the tempo would go
        // outside 20-300 BPM. `factor`: the Equivalent Tempo Scaling.
        bool processTempo (double start, double end, double newSeconds, double* factor = nullptr);
        static double tempoScaling (const TempoMap&, double start, double end, double newSeconds);
        void setCycle (double start, double end);
        void setCycleOn (bool);
        // Snap on/off (J) and the quantize value in beats (4: 1/1 ... 1/16: 1/64), the toolbar's (0.9.10).
        void setSnap (bool);
        void setQuantizeBeats (double);
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
        // SAVE on screen (0.9.11): the file written on a thread of its own; `done` here after.
        void saveInBackground (const juce::File&, std::function<void (bool written)> done = {});
        bool isSaving() const noexcept { return savesRunning > 0; }
        bool waitForSaves (int milliseconds)   // (tests: the writing thread done)
        {
            for (int waited = 0; saver.getNumJobs() > 0 && waited < milliseconds; waited += 5)
                juce::Thread::sleep (5);
            return saver.getNumJobs() == 0;
        }
        // `problems`: instruments that couldn't be loaded (the project opens anyway).
        bool open (const juce::File&, juce::String& problems);
        // A DAWproject (0.9.13: songs from Cubase, File > Export > DAWproject;
        // see DawProject.h) as a new song, not saved yet. `report`: what didn't
        // come across, or the error if it couldn't be read.
        bool importDawProject (const juce::File&, juce::String& report);
        // A standard MIDI file: a track on the studio's synth (PR7-K) for each of its tracks
        // (a type-0 file: each channel), its notes in a part (whole bars).
        // `newSong`: a new song first, with the file's tempo and time
        // signature; else into this one at `atBeat` (its tempo kept, the
        // notes on its beats: the manual's "Ignore Master Track Events on
        // Merge"), unless `matchTempo` (0.9.14, the owner asked): then the
        // song takes the file's tempo first (one undo step of its own), so the
        // notes play at the speed they were written at. Returns how many
        // tracks it added (0, with `error` set, if none).
        // `ontoTrackId` (0.18.1): dropped on an instrument track, the file's first track goes onto it (the others: new tracks).
        int importMidi (const juce::File&, bool newSong, double atBeat, juce::String& error, bool matchTempo = false, int ontoTrackId = 0);
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
        // `oneTrack` (the manual's Import Files on One Track): one after another on
        // the track; else each file on a track of its own, all at `atBeat` (the
        // first on `trackId` if that's an audio track). Returns the (first) track.
        int importAudio (const juce::Array<juce::File>&, int trackId, double atBeat, juce::String& error, bool oneTrack = true);
        int importMidi (const MidiImport&, bool newSong, double atBeat, bool matchTempo = false, int ontoTrackId = 0);
        bool canTakeMidi (int trackId) const;   // an instrument track, not frozen or disabled
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
        // How far a long job done on the screen's own thread has got (0.11.1:
        // Render in Place, Freeze, a song's plug-ins loading): `progress` 0 to 1
        // and what it is; -1 when it's over (the progress box goes).
        std::function<void (double progress, const juce::String& what)> onProgress;
        // Who runs a render's work (0.12.1, the owner's renders and freezes
        // never came back): nobody set, here on this thread (the tests); the
        // app sets one that runs it on a thread of its own while the screen's
        // thread carries on (an instrument rendering offline may wait for its
        // own loading threads, and they for the screen's thread: on the
        // screen's thread itself, that wait never ended). It returns `work`'s answer.
        std::function<bool (std::function<bool()> work)> renderRunner;
        bool isRendering() const noexcept { return rendering; }
        // CANCEL (any thread): the render running stops at its next block; its file is deleted.
        void cancelRender() noexcept { renderCancelled = true; }
        bool wasRenderCancelled() const noexcept { return renderCancelled; }
        // How far the render running has got (the runner's loop shows it).
        void reportRenderProgress();
        // The stage a job is at: its share of the whole, from `base`, `span` long.
        void setProgressStage (double base, double span, const juce::String& what);
        void endProgress();
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

        //======================================================================
        // The MediaBay (0.21.0): the media files in the folders scanned, kept in "MediaBay.xml" beside
        // the settings (in memory without a settings file). The first time, your Documents' Paragon
        // PR7-S folder (songs, track presets) is a location. The scan waits while playing or recording.
        MediaDatabase& getMediaBay();
        // A file heard: audio through the engine's preview (over the output, at the preview level,
        // looping when `loop` is on); a MIDI file through the live track's instrument at the song's
        // tempo (as IDEAS' previews). False for files it can't play (presets, songs, unreadable).
        // The previewer's options (0.23.0, the manual's Audio Files Previewer): LINK TEMPO (a loop
        // stretched to the song's tempo at the playhead, its pitch kept; not one-shots or files
        // without a tempo), WAIT FOR PLAY (heard only while the song plays; linked loops locked to
        // the beat from the left locator), semitones or a root key to move to (the short way; a
        // root key needs the file's key), half or double speed. Kept in the settings (mediaBay*).
        struct MediaPreviewOptions
        {
            bool linkTempo = false, waitForPlay = false;
            int semitones = 0;        // -12 to 12
            int toKey = -1;           // a pitch class 0-11 (C = 0), or -1: off
            double speed = 1.0;       // 0.5, 1 or 2
            bool operator== (const MediaPreviewOptions&) const = default;
        };
        const MediaPreviewOptions& getMediaPreviewOptions();
        void setMediaPreviewOptions (const MediaPreviewOptions&);
        // What a preview of this file does with the options: the length factor (output / input),
        // the semitones, and the beats it's locked to (0: not locked). (Worked out, not played: tests.)
        struct MediaPreviewPlan { double lengthFactor = 1.0, semitones = 0.0, lockBeats = 0.0, songBpm = 120.0; };
        MediaPreviewPlan planMediaPreview (const juce::File&, double seconds);
        static int keyPitchClass (const juce::String& key);   // "F#m" -> 6; "" or unknown -> -1
        bool previewMedia (const juce::File&, bool loop);

        // VST presets (0.24.0): the plug-in a .vstpreset is for (among the scanned ones and the
        // studio's synth: by its class id as JUCE hashes it, else by name), and using it: a track
        // of its own (an instrument's: an instrument track named after the preset; an effect's:
        // an FX channel), or onto a track (an instrument's: that track's instrument, loaded into it
        // when it's the same plug-in; an effect's: loaded into that effect if the track has it,
        // else added to its inserts). The preset's settings go in as the plug-in's state.
        struct PluginPreset { juce::PluginDescription plugin; juce::MemoryBlock state; juce::String name; bool instrument = false; };
        std::optional<PluginPreset> readPluginPreset (const juce::File&, juce::String& error);
        int addTrackFromPluginPreset (const juce::File&, juce::String& error);
        bool applyPluginPreset (int trackId, const juce::File&, juce::String& error);   // trackId 0: the master (effects only)
        void stopMediaPreview();
        bool isMediaPreviewing() const;
        double getMediaPreviewProgress() const;                      // 0 to 1
        std::shared_ptr<const AudioData> getMediaPreviewSound() const noexcept { return mediaPreviewSound; }   // (audio, for drawing)
        void setMediaPreviewLoop (bool);
        float getMediaPreviewLevel() const;                          // 0 to 1 (settings "mediaBayLevel", default 0.7)
        void setMediaPreviewLevel (float);

    private:
        PadPlayer padPlayer;
        class LinePreview;
        std::unique_ptr<LinePreview> linePreview;
        bool padsActive = false;
        std::vector<int> assignTargets;
        std::vector<int> assignHeld, assignNotes;
        void padsChanged();
        void chordTrackChanged();
        std::vector<int> unlocked (const std::vector<int>& pads) const;
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
        std::map<const juce::AudioProcessor*, bool> editorAnswers;   // hasEditor's
        void unloadPlugins (Track&);   // their settings kept first (disable, freeze)
        void loadPlugins (Track&);     // from the kept settings (enable, unfreeze)
        std::map<int, std::vector<Part>> frozenParts;   // the parts as they were frozen (locked)
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
        std::unique_ptr<ProjectLibrary> library;
        std::unique_ptr<PluginHost> host;
        juce::AudioDeviceManager devices;
        bool withDevices = false;
        Engine engine;

        Project project;
        juce::File file;
        bool modified = false;
        bool masterSelected = false;
        bool prepareSave (const juce::File&);
        // The song's recordings and freeze files (and its undo history's) copied from one folder to another.
        bool copyAudioFiles (const juce::File& from, const juce::File& to);
        void sendGone (Track&, int slot);
        bool linksSuspended = false, linksAbsolute = false, qLink = false;
        bool mirroring = false;
        juce::String guitarTuning;   // (without a settings file: the tests)
        GuitarVelocities guitarVelocities;
        float ideasHumanize = 0.0f, humanizeAmount = 0.5f;   // (without a settings file: the tests)
        bool ideasFeel = true, humanizeLayers = true;
        float dropHighlight = defaultDropHighlight;   // (without a settings file: the tests)   // (a linked change being passed on: not passed on again)
        void pruneLinkGroups();
        // `action` for each channel linked to this one for `what` (not itself); the
        // changes they make are this change's (one undo step, one engine update).
        template <typename Action>
        bool passOn (int trackId, LinkGroup::What what, Action&& action)
        {
            if (mirroring)
                return false;
            const juce::ScopedValueSetter<bool> passing (mirroring, true);
            auto any = false;
            for (const auto id : linkedTracks (trackId, what))
                if (id != trackId)
                {
                    action (id);
                    any = true;
                }
            return any;
        }
        void grabOne (int busId, const AutomationLane&);
        void releaseOne (int busId, const AutomationLane&);
        static std::optional<LinkGroup::What> linkFor (const AutomationLane&);
        // A level moved on one channel, on a linked one: by the same amount, or the same (ABS).
        float linkedLevel (float now, float before, float after, float low, float high) const;
        static void silenceForRender (Track&);
        void openProject (Project&&, const juce::File& source, juce::String& problems);   // open's and importDawProject's   // the plugins' settings into the song; the recordings beside it
        juce::ThreadPool saver { juce::ThreadPoolOptions{}.withThreadName ("Paragon PR7-S save").withNumberOfThreads (1) };
        std::shared_ptr<std::atomic<bool>> saveAlive = std::make_shared<std::atomic<bool>> (true);   // (false once the studio goes)
        int savesRunning = 0;
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
        juce::String loggedDevice;   // the audio device's last line in the session log
        double progressBase = 0.0, progressSpan = 1.0;
        bool rendering = false;
        std::atomic<double> renderDone { 0.0 };   // written by the render's thread
        std::atomic<bool> renderCancelled { false };
        juce::String progressWhat;
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
        std::unique_ptr<MediaDatabase> mediaBay;   // (0.21.0; made when first asked for)
        std::shared_ptr<const AudioData> mediaPreviewSound;
        double mediaPreviewBeats = 0.0;
        bool mediaPreviewMidi = false;
        float mediaPreviewLevel = 0.7f;
        MediaPreviewOptions mediaPreviewOptions;
        bool mediaPreviewOptionsLoaded = false;
        // Several renders in a row (Render in Place's stretches, Freeze's
        // tracks): the plugins stay offline and the interface off between
        // them, switched back once at the end (0.9.9).
        bool inBatch = false;

        std::vector<Project> undoStates;
        int undoIndex = -1;
    };
}
