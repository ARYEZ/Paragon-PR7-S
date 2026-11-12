#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "ProjectLibrary.h"
#include "Engine.h"
#include "PluginHost.h"
#include "Project.h"
#include "EditHistory.h"
#include "Warp.h"
#include "Align.h"
#include "VariAudio.h"
#include "Lines.h"
#include "Riffs.h"
#include "DragOut.h"
#include "DrumGenerator.h"
#include "DrumLearning.h"
#include "SectionPlan.h"
#include "RiffPaste.h"
#include "Patterns.h"
#include "Progressions.h"
#include "Melody.h"
#include "ChordDetect.h"
#include "KeyCheck.h"
#include "TempoDetect.h"
#include "MediaBay.h"
#include "Composer.h"
#include "SoloEngine.h"

#include <optional>
#include <mutex>

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
        // (0.34.3) Goes up with every change announced except a fader, pan or send level moved alone
        // (not writing automation): a view that saw this version already has only the mix to follow.
        juce::uint64 getChangeVersion() const noexcept { return changeVersion; }
        Engine& getEngine() noexcept { return engine; }
        PluginHost& getPluginHost() noexcept { return *host; }
        juce::AudioDeviceManager& getDeviceManager() noexcept { return devices; }
        juce::PropertiesFile* getSettings() noexcept { return settings.get(); }
        // How strongly a file dragged in lights where it will land (0.18.2, the owner asked: a
        // setting in the Hub): 0.1 faint to 1 solid; settings key "dropHighlight", default 0.6.
        float getDropHighlight() const;
        void setDropHighlight (float);
        static constexpr float defaultDropHighlight = 0.6f;
        // (0.72.0, the owner asked) AUTO-SAVE: every so many minutes (0: off) while the song has
        // unsaved changes, a backup copy of it is written into `backupFolder()`: "Backups" beside the
        // song, or Documents/Paragon PR7-S/Backups for a song not saved yet. The song's own file is
        // never written by it (only SAVE does that). The newest `getAutoSaveKeep()` copies of a song are
        // kept, older ones deleted. Not while recording or rendering; not when nothing changed since
        // the last copy. A backup remembers where the song's recordings are, so it opens with them.
        // Settings keys "autoSaveMinutes" (default 5) and "autoSaveKeep" (default 10).
        // (0.80.0, the owner asked: the other DAW's Track Type Default Colors, one step further) What
        // a new track of each type starts with: a colour (none: AUTOMATIC, red, pink, orange in turn
        // by its place, as before) and its colour in the track list (on, 1-100 %). Settings key
        // "trackTypeLooks"; also SET TRACK COLOUR TO DEFAULT's colour. COLOUR IN ORDER still wins.
        struct TrackTypeLook
        {
            std::optional<juce::Colour> colour;
            bool listColour = false;
            int listAmount = Track::defaultListColourAmount;
            bool operator== (const TrackTypeLook&) const = default;
        };
        static constexpr int numTrackTypes = 5;   // (by Track::Kind: instrument, audio, fx, group, folder)
        static juce::String trackTypeName (Track::Kind);
        TrackTypeLook getTrackTypeLook (Track::Kind) const;
        void setTrackTypeLook (Track::Kind, TrackTypeLook);
        // The open song's tracks given their type's look now (one undo step).
        void applyTrackTypeLooks();
        int getAutoSaveMinutes() const;
        void setAutoSaveMinutes (int minutes);
        int getAutoSaveKeep() const;
        void setAutoSaveKeep (int copies);
        static constexpr int defaultAutoSaveMinutes = 5, defaultAutoSaveKeep = 10;
        juce::File backupFolder() const;
        // Writes a backup now (the timer calls it when due). False: nothing written (nothing changed
        // since the last one, or it couldn't be). `waitForSaves` waits for it (it's written on the save thread).
        bool writeBackup();
        juce::File getLastBackup() const noexcept { return lastBackup; }
        // The Hub's songs (0.12.0): recent ones, the project location, templates.
        ProjectLibrary& getLibrary() noexcept { return *library; }
        // Create Empty (the Hub): a new song, saved at once in a folder of its
        // own at the project location (so its recordings go beside it).
        bool createSong (const juce::String& name, juce::String& error);
        // A new song from a template: the template's tracks, plug-ins and mix, not saved yet.
        bool newFromTemplate (const juce::File& templateFile, juce::String& problems);
        // (0.78.2) A new song: from HUB SETTINGS' DEFAULT TEMPLATE when one is chosen, else empty.
        // False (problems set) when the template couldn't be opened: an empty song then.
        bool startNewSong (juce::String& problems);
        // This song as a template (Templates folder); the song itself stays as it is.
        bool saveAsTemplate (const juce::String& name, juce::String& error);
        // SETTINGS' RELOAD DRIVER: the audio device closed and opened again, so
        // a buffer size set in the driver's own panel is read (for drivers that
        // don't say they changed).
        void reloadAudioDevice();
        // Multi-core audio (0.34.5): tracks processed side by side on the computer's cores (settings
        // `multiCoreAudio`, on; tests without devices: off). Off: everything on the audio thread, for a
        // plug-in that doesn't like being run beside others. The output is the same either way.
        bool getMultiCoreAudio() const;
        void setMultiCoreAudio (bool);
        // (0.60.0) Drawing the windows with the processor instead of the graphics card (settings key "softwareDrawing",
        // default off): the owner's NEW froze inside the graphics driver; this tells whether the driver is to blame.
        bool getSoftwareDrawing() const;
        void setSoftwareDrawing (bool);
        int getAudioHelperThreads() const noexcept { return engine.getHelperThreads(); }
        // (0.57.3) A spike report in words (the session log's line); how many were written.
        juce::String describeSpike (const Engine::SpikeReport&) const;
        int getSpikesNoted() const noexcept { return spikesNoted; }

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
        // (0.73.0) The tracks' colour over their rows in the track list: on/off and 1-100 %. Not an undo step.
        void setTracksListColour (const std::vector<int>& trackIds, bool on, int amount);
        // (0.79.0, the owner asked) COLOUR IN ORDER (Project::AutoColour): set, then applied at
        // once and after every change while it's on. Not an undo step (as the list colour).
        void setAutoColour (AutoColour);
        // The colour number each track the rule covers takes now (track id -> 1-75), in order.
        std::vector<std::pair<int, int>> autoColourNumbers() const;
        // (0.73.0, the owner asked) The track moved to that slot of the track list (1 = the top, counted
        // over the tracks shown); the others move down (or up) a slot. A folder takes its tracks with it.
        // One undo step; false if nothing moved.
        bool moveTrackToSlot (int trackId, int slot);
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
        // (0.78.0, the owner: "solo the instrument I'm in the editor", the other DAW's Solo Editor) While set, only
        // that track is heard (and what it's routed into); the tracks' own solos are untouched and nothing is
        // saved or undone. 0: off. Renders and exports never hear it.
        void setEditorSolo (int trackId);
        int getEditorSolo() const noexcept { return editorSoloTrack; }
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
        // (0.44.0) The chord modifiers (the manual's Pad Remote Control): on the last pad played.
        void applyPadModifier (ChordPads::Modifier);
        int getLastPlayedPad() const noexcept { return lastPlayedPad.load(); }
        // MIDI Learn (the manual's Chord Pads Remote Settings): the next key pressed becomes this
        // (the range takes two keys: its start, then its end); `onLearned` when done (message thread).
        enum class PadLearn { none, range, modifier, section, subsection };
        void startPadLearn (PadLearn, int index = 0);
        void cancelPadLearn();
        PadLearn getPadLearn() const noexcept { return padLearn; }
        int getPadLearnIndex() const noexcept { return padLearnIndex; }
        std::function<void()> onLearned;
        void learnedKey (int note);                     // (the message thread; from the devices, or tests)
        // (0.44.0) Recording chord events (the manual's Recording Chord Events with a MIDI Keyboard and
        // Recording Chords on the Chord Track): the chord track's Record Enable; while recording,
        // each pad pressed is a chord event at that beat; with none pressed, the chords recognised
        // in what was played (three notes or more starting together). Snapped to the grid when
        // snapping. They replace the chord events where recording ran; with the take's undo step.
        void setChordTrackRecord (bool);
        bool isChordTrackRecording() const noexcept { return project.chordTrack.record; }
        std::vector<ChordEvent> chordsFromPlayed (const std::vector<HeardNote>&) const;   // (tests)
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
        // The progression library (0.40.0): its steps (each chord its own bars) from the bar
        // `atBeat` is in, what was over them replaced; the chord track shown. One undo step.
        void addProgressionToChordTrack (const std::vector<ProgressionStep>&, double atBeat);
        // The steps as chord spans from `startBeat` (the bars in the song's bars), in `key`: what
        // previewLine plays and IDEAS writes under.
        std::vector<ChordSpan> progressionSpans (const std::vector<ProgressionStep>&, double startBeat, const Key&) const;
        // The library's favourites by name (settings key "progressionFavorites", a name a line).
        juce::StringArray getProgressionFavorites() const;
        void setProgressionFavorite (const juce::String& name, bool);

        // Bass and melody ideas (0.18.0, the owner asked). The chords they follow: the chord
        // track's events given (the selection), else those in the cycle when it's on and
        // holds some, else all; each until the next (the last a bar), with the key there.
        std::vector<ChordSpan> getLineChords (const std::vector<int>& selectedEvents = {}) const;
        // An idea as a part under its chords: onto `ontoTrackId` when it can take MIDI (0.19.0:
        // the selected track, the owner's guitar VST), else onto a new instrument track (the
        // studio's synth) named for the part (Bass, Melody, Piano, Guitar, Lead Guitar). The
        // track's id; one undo step.
        int addLinePart (const LineIdea&, const std::vector<ChordSpan>&, int ontoTrackId = 0);
        // (0.64.0) An idea as a part (from `start`, `length` beats long): its notes (keyswitches too), and
        // its notes' effects as controllers: vibrato as the mod wheel (CC 1) rising after the attack and
        // back to 0 as it ends, a bend as pitch bend up a whole step by the instrument's bend range (the
        // INSTRUMENTS profile on the idea's lane, else 2 semitones) and back to none as it ends.
        Part partFromIdea (const LineIdea&, double start, double length, const juce::String& name) const;
        static std::vector<Controller> effectControllers (const std::vector<LineNote>&, int bendRange);
        static void fitBends (std::vector<LineNote>&, int bendRange);   // (0.66.0) bends past the range made smaller
        // (0.57.0) BUILD SECTION: each idea onto the track named after its part (Drums, Bass, Guitar,
        // Lead Guitar, Piano, Melody: an instrument track of that name is reused, so section after
        // section lands on the same band; else a new one), each part over the chords' span, named
        // "<section>: <idea>". Parts lying wholly inside the span on those tracks are replaced
        // (building the same stretch again). One undo step for the parts. The tracks used, in order.
        // (0.58.0) `onto`: per idea, a track to write it on instead (when it can take MIDI; 0: as above).
        std::vector<int> buildSection (const std::vector<LineIdea>&, const std::vector<ChordSpan>&, const juce::String& sectionName,
                                       const std::vector<int>& onto = {});
        int findIdeaTrack (LineIdea::Part) const;   // the instrument track named after the part (0: none)
        // THE COMPOSER (0.58.0): what its layers are made with (IDEAS' settings; the tempo and bar at `atBeat`).
        ComposerContext getComposerContext (double atBeat) const;
        juce::String getComposerState() const;            // (settings key "composer"; in memory without a settings file)
        void setComposerState (const juce::String&);
        // Hearing the composer: its layers played through the song's own tracks (each onto `trackId`), looped over
        // [start, end), those tracks alone (soloed), while the song itself stays as it is (the engine plays a copy,
        // as the comp tool's audition). Called again while it plays: the new layers from where it is. False when
        // there's nothing to play them on. It ends on stopComposerAudition, on the transport's stop, and on any
        // change to the song (the engine then plays the song again).
        bool auditionComposer (const std::vector<std::pair<int, LineIdea>>& layers, double start, double end);
        void stopComposerAudition();
        bool isComposerAuditioning() const noexcept { return composerAudition.has_value(); }
        double getComposerAuditionStart() const noexcept { return composerAudition.has_value() ? composerAudition->start : 0.0; }
        double getComposerAuditionEnd() const noexcept { return composerAudition.has_value() ? composerAudition->end : 0.0; }
        // The sections built so far in this song (for what comes next; forgotten with the song).
        const std::vector<BuiltSection>& getBuiltSections() const noexcept { return builtSections; }
        void noteBuiltSection (const BuiltSection&);   // (replaces one built over the same stretch)
        void clearBuiltSections() { builtSections.clear(); }
        // (0.75.0) Your own riff in the composer (engine/RiffPaste.h; forgotten with the song, as the sections).
        const std::optional<ComposerRiff>& getComposerRiff() const noexcept { return composerRiff; }
        void setComposerRiff (std::optional<ComposerRiff> r) { composerRiff = std::move (r); }
        // The chords for the section after `from`, as long as `spans`: the chord track's there when it
        // has a chord starting in that stretch; else `spans` again from `from`, written onto the
        // chord track (one undo step) so the song keeps its chords.
        std::vector<ChordSpan> chordsForNextSection (const std::vector<ChordSpan>& spans, double from);
        static juce::String linePartName (LineIdea::Part);
        // The guitar ideas' tuning (0.19.0; settings key "guitarTuning", default 7-string drop A).
        // The MELODY tab's settings (0.41.0; settings key "melodySettings").
        MelodySettings getMelodySettings() const;
        void setMelodySettings (const MelodySettings&);
        juce::String getGuitarTuning() const;
        void setGuitarTuning (const juce::String&);
        // The bass's tuning (0.61.0: the composer's LOW STRING and the bass's range; settings key
        // "bassTuning", default 4-string standard; engine/Takes.h's bassTunings).
        juce::String getBassTuning() const;
        void setBassTuning (const juce::String&);
        // The Hub's INSTRUMENTS (0.62.0; settings key "instrumentProfiles"; engine/Instruments.h). The first
        // time: Djinn Bass 2 (the owner's) for the bass. `profileFor`: the one marked for that lane.
        std::vector<InstrumentProfile> getInstrumentProfiles() const;
        void setInstrumentProfiles (const std::vector<InstrumentProfile>&);
        enum class ProfileUse { bass, guitar, lead };
        std::optional<InstrumentProfile> profileFor (ProfileUse) const;
        // The guitar ideas' velocities (0.19.3, the VELOCITIES panel; settings key "guitarVelocities").
        GuitarVelocities getGuitarVelocities() const;
        void setGuitarVelocities (const GuitarVelocities&);
        // The guitar tab's STYLE and RIFF SETTINGS (0.53.0; settings keys "guitarStyle", "riffSettings").
        GuitarStyle getGuitarStyle() const;
        void setGuitarStyle (GuitarStyle);
        // The guitar tab's SOLO STYLE (0.66.0; settings key "soloStyle"; automatic: by the STYLE).
        SoloStyle getSoloStyle() const;
        void setSoloStyle (SoloStyle);
        RiffSettings getRiffSettings() const;
        void setRiffSettings (const RiffSettings&);
        // The DRUMS tab's beat generator settings (0.55.0; settings key "drumSettings").
        DrumSettings getDrumSettings() const;
        void setDrumSettings (const DrumSettings&);
        // Drummer profiles (0.56.0): their folder (Documents/Paragon PR7-S/Drummer Profiles), the one
        // the DRUMS tab plays from (settings key "ideasDrummer"; empty: the style's rules).
        juce::File getDrummerFolder() const { return drummerFolderOverride != juce::File() ? drummerFolderOverride : defaultDrummerFolder(); }
        void setDrummerFolderForTests (const juce::File& f) { drummerFolderOverride = f; }
        std::vector<juce::String> getDrummerNames() const { return drummerNames (getDrummerFolder()); }
        std::optional<DrumProfile> loadDrummer (const juce::String& name) const;
        // (0.85.2) The last profile read, kept while its file's size and date are the same: the composer
        // asks for it on every change of the song, and read the whole file each time.
        struct KeptDrummer { juce::File file; juce::Time modified; juce::int64 size = -1; std::optional<DrumProfile> profile; };
        mutable KeptDrummer keptDrummer;
        bool saveDrummer (const DrumProfile&, juce::String& error);
        bool deleteDrummer (const juce::String& name);
        juce::String getIdeasDrummer() const;
        void setIdeasDrummer (const juce::String&);
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
        void removeChordEvents (std::vector<int> indices);   // (0.85.3: a gap left where they were, as clips)
        // (0.85.3, the owner) Chords edited like clips, each one undo step; every chord gets its own
        // length first (`withLengths`), so a chord moved away leaves a gap. What a chord is placed over
        // is cut out of the chords there. Each returns where the chords it placed are now.
        std::vector<int> moveChordEvents (const std::vector<int>& indices, double delta, bool copy);   // delta in beats
        int resizeChordEvent (int index, double newStart, double newEnd);
        std::vector<int> splitChordEvents (double beat, const std::vector<int>& indices);   // empty: every chord there
        int glueChordEvent (int index);                       // it takes over the next one's place
        struct ChordClip { std::vector<ChordEvent> events; double length = 0.0; };   // beats from the first's start
        ChordClip copyChordEvents (const std::vector<int>& indices) const;
        std::vector<int> pasteChordEvents (const ChordClip&, double atBeat);
        std::vector<int> duplicateChordEvents (const std::vector<int>& indices);       // right after them
        // `placed` laid over `others` (placeChords), the chord track set to the result.
        std::vector<int> placeChordEvents (const std::vector<ChordEvent>& placed, std::vector<ChordEvent> others);
        void setScaleEvents (std::vector<ScaleEvent>);    // by hand: the scales stop following the chords
        // (0.85.3) The key chosen (the composer's) on the key strip from `fromBeat` on: the strip set by hand
        // from there (what was worked out before it kept), so a chord moved out of that key shows it.
        void setChordTrackKey (double fromBeat, const Key&);
        void setAutoScales (bool);
        Key chordTrackKeyAt (double beat) const;          // the scale in force (else the pads' key)
        // A new chord at `beat`: the best that could follow the chord before (else the key's I).
        Chord suggestChordAt (double beat) const;
        // The chords (or those at these indices) as a MIDI part, voiced with the least
        // movement: onto that track, or a new instrument track (0); the part's id.
        int chordsToMidi (int trackId, std::vector<int> indices = {});
        // The chords heard in these parts onto the chord track (replacing those over their span).
        int chordsFromParts (const std::vector<int>& partIds, double step = 1.0);
        // Chord detection (0.42.0): the chords of these parts, MIDI and audio together (each
        // source weighed alike), from the step grid at or before the first part's start.
        std::vector<ChordEvent> detectChords (const std::vector<int>& partIds, const DetectSettings&);
        // ... onto the chord track (what was over the parts' span replaced; one undo step) or the
        // pads (each chord once, in order, from the first unlocked pad). How many chords.
        int detectChordsToTrack (const std::vector<int>& partIds, const DetectSettings&);
        // (0.84.0) KEY CHECK: the notes and chords outside the key (KeyCheck.h); a chosen key, else the chord
        // track's keys, else the song's key guessed. The fix moves each listed note to its suggestion (as heard:
        // the part's transpose taken back off), all as one undo step; chords aren't changed (the chord track's own
        // menus swap them). False if nothing was moved (a note since edited or gone is left alone).
        std::vector<KeyIssue> checkSongKey (std::optional<Key> chosen = std::nullopt) const;
        bool fixOutOfKeyNotes (const std::vector<KeyIssue>&);
        int detectChordsToPads (const std::vector<int>& partIds, const DetectSettings&);
        // The settings last used (settings key "chordDetect").
        DetectSettings getDetectSettings() const;
        void setDetectSettings (const DetectSettings&);
        // The manual's Assign from Chord Track: its chords, each once, onto the pads in order.
        void assignPadsFromChordTrack();

        // Follow Chord Track (0.36.0, the manual's): a MIDI track's notes heard moved to the chord
        // track's chords (FollowChords.h has the rules). The first time (no reference yet) it
        // needs how to begin (the manual's dialog): Follow Directly (the chord track as it is now
        // is what the notes were played over), Analyze Chords (found in the track's notes) or
        // Apply a Known Chord. Later modes keep that reference. One undo step each.
        bool needsFollowSetup (int trackId) const;
        bool startFollowing (int trackId, FollowMode, FollowStart, const Chord& known = {});
        void setFollowMode (int trackId, FollowMode);          // (with a reference: else Follow Directly)
        void resetFollowing (int trackId);                     // off, the reference forgotten (the dialog again next time)
        // Live Input: the keys played into this track onto the chord or the scale in force.
        void setLiveFollow (int trackId, LiveFollow);
        // Performances (0.39.0): the chord pads play their chords through a pattern (Patterns.h;
        // -1: as chords, as before). Kept in the settings by name (settings key `padPattern`).
        void setPadPattern (int index);
        int getPadPattern() const;
        // (0.44.0) A track performing a pattern over the chord track (the Inspector's PERFORM; empty: none;
        // instrument tracks). One undo step.
        void setPerformPattern (int trackId, const juce::String& patternName);
        // A MIDI part as one of your patterns (MY PATTERNS; settings key "userPatterns"); its name
        // (made unique), empty if it's not a MIDI part with notes.
        juce::String addPatternFromPart (int partId, juce::String name = {});
        void removeUserPattern (const juce::String& name);
        PatternPerformer& getPerformer() { return *performer; }
        void setChordShift (int ticks);                        // Shift Chords (-480 to 480 ticks), an undo step
        // (0.38.0) Voicings and Single Voice: the voice, This Track's Voicings (none: the chord
        // track's), the chord track's voicing library; each an undo step. The chord track's chords
        // as voiced for a track (its voicings, else the chord track's).
        void setFollowVoice (int trackId, Voice);
        void setTrackVoicing (int trackId, std::optional<VoicingSettings>);
        void setChordTrackVoicing (const VoicingSettings&);
        std::vector<std::vector<int>> getChordTrackVoicings (int trackId) const;
        // A part's notes as they're heard (pitch, or -1: dropped as a double), for the MIDI editor.
        std::vector<int> followedPitches (int partId) const;
        // Map to Chord Track (the manual's): the chosen MIDI parts' notes written moved to the
        // chord track, each part's own chords found in it first ("C" if none); doubles removed.
        // One undo step; how many notes changed pitch (-1: nothing to map to).
        int mapToChordTrack (const std::vector<int>& partIds, FollowMode);

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
        // (0.29.0) toLane: the lane it goes onto (-1: its own on the same track, else the first);
        // it lands in front of what it overlaps there.
        void movePart (int trackId, int index, int toTrackId, double newStart, int toLane = -1);
        void copyPart (int trackId, int index, int toTrackId, double newStart, int toLane = -1);
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
        // recording straight after it (a split glued back) become one clip again;
        // any other clips (0.82.6) are written as heard into one new file
        // ("<track> glued NN.wav") and become one clip of it.
        bool glueParts (int trackId, int index, bool toAllFollowing);
        // Quantize (Q in the arrangement, 0.31.0: the manual's "In the Project window, select a MIDI part"):
        // every note of these MIDI parts to the nearest step of `grid` beats (the song's quantize value), its
        // length kept. Audio and frozen parts are passed over. One undo step; false if nothing was there.
        bool quantizeParts (const std::vector<int>& partIds, double grid);
        // The Mute tool (and the part's menu): muted parts stay, greyed, but aren't played.
        void setPartMuted (int trackId, int index, bool muted);
        void setPartsMuted (const std::vector<int>& partIds, bool muted);   // several at once (one step)
        // (0.47.0) The info line for parts and events (the manual's Info Line and its Value
        // Editing Rules): a field as a part has it (start/end/length in beats, offset and fades
        // in seconds, volume in dB, transpose in semitones, velocity added; none: not this part's
        // kind: offset, fades and volume are audio events', transpose and velocity MIDI parts').
        enum class PartField { start, end, length, offset, fadeIn, fadeOut, volume, transpose, velocity };
        std::optional<double> getPartField (int partId, PartField) const;
        // The parts' field moved by what the first one's (the first that has it) moves to `value`
        // (two events 1 and 2 bars long, LENGTH typed as 3 bars: 3 and 4); `absolute` (Ctrl): all
        // set to it. Each limited as it can go (not before the song's start, an event not past its
        // recording, fades within it); frozen tracks' parts left alone. One undo step; false: none moved.
        bool setPartsField (const std::vector<int>& partIds, PartField, double value, bool absolute);
        bool renameParts (const std::vector<int>& partIds, const juce::String& name);
        // The Erase tool with Alt: the part and all after it on its track.
        void deletePartsFrom (int trackId, int index);

        // Takes and comping (0.29.0, the manual's Lanes, Takes, and Overlapping Events and
        // Assembling a Perfect Take). A track's parts sit on lanes (Part::lane); where audio
        // clips overlap, the one in front plays (Part::order). Each edit one undo step but
        // Show Lanes and a lane's solo (as Hide, not steps).
        void setShowLanes (int trackId, bool shown);
        void setLaneSolo (int trackId, int lane);   // -1: none (the lane's S again: off)
        void setRecordModes (const Project::RecordModes&);
        // (0.52.0, the manual's Auto Fades dialog) The song's auto fades, and a track's own
        // (none: Use Project Settings). Not undo steps (as the record modes).
        void setAutoFades (const AutoFades&);
        void setTrackAutoFades (int trackId, std::optional<AutoFades>);
        // To Front (the part's menu, the Comp tool's click): audio, in front of every clip it
        // overlaps (and not muted); MIDI (all overlapping parts play), it's unmuted and the
        // parts it overlaps on other lanes are muted. `toggle` (a click on the take in front
        // already): audio, the one it hides comes back to the front; MIDI, it's muted.
        void bringToFront (int partId, bool toggle = false);
        // The Comp tool's drag on a lane: every part on the track crossing beats `start` or
        // `end` is cut there, and the lane's part in between brought to front (none there:
        // nothing). Adjacent pieces of one recording, played end to end, are glued back.
        void compRange (int trackId, int lane, double start, double end);
        // A part onto another lane of its track (a drag between lanes).
        void setPartLane (int partId, int lane);
        // The track's menu: Clean Up Lanes (what isn't heard is removed: audio, the hidden
        // stretches of clips; MIDI, muted parts under an unmuted one; then empty lanes go),
        // and Create Tracks from Lanes (each lane after the first a track of its own, a copy
        // of this one with that lane's parts). Returns the new tracks' ids.
        void cleanUpLanes (int trackId);
        std::vector<int> createTracksFromLanes (int trackId);
        // Every part on the lane, and the lane's parts that are heard at a beat.
        std::vector<int> partsOnLane (int trackId, int lane) const;
        // The part (id) heard at a beat on a track (audio: the clip in front; MIDI: the first
        // unmuted part there), 0: none.
        int frontPartAt (int trackId, double beat) const;

        // Hitpoints and warping (0.32.0, the manual's Hitpoints, Free Warp, AudioWarp Quantize and Flatten
        // Real Time Processing). Hitpoints are found in each recording once (kept while the song is open),
        // filtered by the song's settings and the hand edits per recording.
        std::vector<double> getHitpoints (const juce::String& file);        // seconds into the recording
        std::vector<double> getPartHitpoints (int partId);                  // beats from the clip's start, inside it
        const Project::HitpointSettings& getHitpointSettings() const noexcept { return project.hitpointSettings; }
        void setHitpointSettings (const Project::HitpointSettings&);        // (not undo steps, as hand edits)
        void addHitpoint (const juce::String& file, double seconds);        // the manual's Alt-click
        void removeHitpoint (const juce::String& file, double seconds);     // Shift-click: disabled
        void moveHitpoint (const juce::String& file, double from, double to);
        void resetHitpoints (const juce::String& file);                     // the hand edits gone
        // The next (or previous) hitpoint after `fromBeat` on a track's clips heard (Alt+N / Alt+B), a song beat.
        std::optional<double> nextHitpoint (int trackId, double fromBeat, bool forward);
        // Warp markers (each an undo step; frozen tracks left alone; markers keep their order, at least a
        // 256th of a beat and 5 ms apart, inside the recording):
        bool createWarpFromHitpoints (const std::vector<int>& partIds);     // a marker at each hitpoint
        bool addWarpMarker (int partId, double beat);                       // where the clip is heard at `beat` now
        bool moveWarpMarker (int partId, int index, double toBeat);         // the audio at it heard at `toBeat`
        bool removeWarpMarkers (int partId, std::vector<int> indices);      // (not its edges)
        void setWarpMarkers (int partId, std::vector<WarpMarker>);          // checked as above
        void resetWarp (const std::vector<int>& partIds);                   // the manual's Reset: at its own speed again
        // Flatten: each warped clip written as it sounds to a new 32-bit file (the original kept), its markers gone.
        int flattenWarp (const std::vector<int>& partIds, juce::String& error);
        // Q on audio clips (the manual's Quantizing Audio Event Starts, or with AudioWarp Quantize on, AudioWarp
        // Quantizing: markers from the hitpoints when it has none, then to the grid). One undo step.
        bool quantizeAudio (const std::vector<int>& partIds, double grid);
        void setAudioWarpQuantize (bool on);
        bool isAudioWarpQuantize() const noexcept { return project.audioWarpQuantize; }
        std::shared_ptr<const juce::AudioBuffer<float>> getWarpedSound (const Part&, const TempoMap&);

        // Audio Alignment (0.33.0, the manual's Audio Alignment Panel). The reference: what a track plays
        // over a stretch (an event's, or a range's); the targets: clips on other audio tracks, each limited
        // to a stretch if a range chose it. Each target is aligned where it overlaps the reference: split at
        // the overlap's edges first (short crossfades there, as the manual), then warp markers written (or,
        // when a plain move does, its audio moved). Sync points and protected ranges are in song beats.
        struct AlignReference { int trackId = 0; double start = 0.0, end = 0.0; };
        struct AlignTarget { int partId = 0; double start = -1.0, end = -1.0; };   // (-1: the whole clip)
        struct AlignSync { int partId = 0; double referenceBeat = 0.0, targetBeat = 0.0; };
        struct AlignReport
        {
            int aligned = 0, moved = 0, stretched = 0;
            std::vector<int> unmetSyncs;          // indices into the sync points given
            std::vector<int> alignedParts;        // per target: the clip aligned (the overlapping piece), 0: not aligned
            double before = 0.0, after = 0.0;     // average distance from the reference (seconds), all targets
            juce::StringArray notes;              // what couldn't be done and why
        };
        // One undo step. False when nothing was aligned (the notes say why).
        bool alignAudio (const AlignReference&, const std::vector<AlignTarget>&, const AlignSettings&,
                         const std::vector<AlignSync>&, const std::vector<juce::Range<double>>& protectedBeats, AlignReport&);
        // What a track plays (its heard clips, warped as heard, both sides mixed) from `from` to `to`
        // song seconds, at the recordings' rate (`rate` set; 0: nothing to hear).
        std::vector<float> heardMono (int trackId, double from, double to, double& rate, int skipPartId = 0);
        // A clip's recording as it's placed now, at its own speed (warps ignored), from `from` to `to`
        // song seconds, reading on past its edges where the recording goes on.
        std::vector<float> clipMono (const Part&, double from, double to, double& rate);
        // VariAudio (0.34.0, the manual's Pitch Editing and Time Correction with VariAudio). A recording's
        // pitch is found once (kept while the song is open); Edit VariAudio splits it into segments (an undo
        // step), kept in the song by file; each edit of the segments is one undo step. What plays (and is
        // bounced, rendered, warped, aligned) is the recording as edited, unless its bypass is on.
        bool editVariAudio (const juce::String& file);                         // false: the file can't be read
        const Project::VariAudio* getVariAudio (const juce::String& file) const;
        std::shared_ptr<const PitchAnalysis> getPitchAnalysis (const juce::String& file);
        void setVariSegments (const juce::String& file, std::vector<VariSegment>);
        void editVariSegments (const juce::String& file, const std::vector<int>& indices, const std::function<void (VariSegment&)>&);
        void setVariAudioBypass (const juce::String& file, bool);              // (not an undo step)
        void removeVariAudio (const juce::String& file);                       // every segment and edit gone
        // Audition: what the recording plays (as edited) between two moments of it, through the preview.
        void auditionRecording (const juce::String& file, double fromSeconds, double toSeconds, bool loop = false);

        // VariAudio, part two (0.35.0). Segment editing, each one undo step (false: it couldn't).
        bool splitVariSegment (const juce::String& file, int index, double seconds);
        bool glueVariSegments (const juce::String& file, int first, int last);
        bool deleteVariSegments (const juce::String& file, std::vector<int> indices);
        bool setVariSegmentEdges (const juce::String& file, int index, double start, double end);
        // The Functions menu's resets (the given segments, or all when none) and Reanalyze Audio.
        enum class VariReset { pitch, curve, formant, volume, warp, all };
        void resetVariAudio (int partId, std::vector<int> indices, VariReset);
        bool reanalyseVariAudio (const juce::String& file);
        // Warp Start / Warp End: a segment's start (or end) heard at `songBeat`, the segment and the
        // one beside it stretched to fit (warp markers on the clip, as the manual has it).
        bool warpVariSegment (int partId, int index, bool atStart, double songBeat);
        // Extract MIDI: onto the first selected instrument track (its parts from earlier extractions
        // replaced) or a new one. The new part's id (0: none, `error` says why).
        enum class ExtractDestination { selectedTrack, newTrack };
        int extractMidi (int partId, const ExtractMidiOptions&, ExtractDestination, juce::String& error);
        // Generate Harmony Voices: up to four tracks (Soprano, Alto, Tenor, Bass), each with its own copy
        // of the clip and of its recording, VariAudio moving every segment to its voice (the chord
        // track's chord there, else +3, -3, -6, -9 semitones), straightened by `vibratoReduction` (0-1).
        std::vector<int> generateHarmonyVoices (int partId, int voices, float vibratoReduction, juce::String& error);
        // The MIDI reference track (its notes as lines in the editor; saved with the song).
        void setVariAudioReference (int trackId);
        // MIDI Input: the keys played (from any device) while asked for, taken on the message thread.
        void setCatchingNotes (bool);
        std::vector<int> takePlayedNotes();
        AlignSettings getAlignSettings() const;   // settings key "audioAlignment"
        void setAlignSettings (const AlignSettings&);

        // Comping, part two (0.30.0, the manual's Assembling Operations and Assembling a Perfect Take).
        // The takes on a part's track with its start and end (stacked: what the Comp tool moves together).
        std::vector<int> stackedWith (int partId) const;
        // Split (the Comp tool's Alt-click): every take on the track cut at the beat.
        void splitTakesAt (int trackId, double beat);
        // Adjust splits / Resize (the Comp tool): the edge at `beat` moved to `newBeat` on every take on
        // the track that starts or ends there (all takes with the same start and end), held so it never
        // passes another take on a lane, shortens one to nothing, or runs off the end of a recording.
        // Returns where it went (one undo step; `beat` when it couldn't move).
        double moveTakeEdge (int trackId, double beat, double newBeat);
        // Slip Event (Alt+Shift-drag): what plays inside the part slides by `beats` (later: positive),
        // the part staying put; audio held within its recording. One undo step.
        void slipPart (int partId, double beats);
        // Bounce (the manual's Bounce Selection for audio, Bounce MIDI): what's heard of the takes into
        // one new clip or part over them all, on the first of their lanes, in front; the takes go.
        // Audio: a new 32-bit file in the audio folder. Returns the new part's id (0: nothing, `error`).
        int bounceTakes (const std::vector<int>& partIds, juce::String& error);
        // A part as a file for another program (0.53.1, drag out): a MIDI part as "<track> - <part>.mid"
        // (DragOut.h's makePartMidiFile), an audio event as "<track> - <part> <tempo> BPM.wav": what it
        // plays (offset, length, warping, its volume and fades, the auto fades; not the track's fader or
        // effects), 32-bit float at its recording's rate, at most five minutes, the tempo and beats in its
        // ACID chunk. In a folder of its own under dragOutFolder(). The song isn't changed. None: an empty File.
        juce::File writePartForDrag (int partId, juce::String& error);
        // (0.59.0) A composer lane as a MIDI file for a drag: the idea's notes as a part over `spans` (the tempo
        // and signature there), on `channel` (the drums: 10, General MIDI's), "<name>.mid" in the composer's
        // drag folder. The song untouched.
        juce::File writeLineForDrag (const LineIdea&, const std::vector<ChordSpan>& spans, int channel, const juce::String& name, juce::String& error);
        // Delete Overlaps: of these takes only what's heard stays, all on the first lane.
        void deleteOverlaps (const std::vector<int>& partIds);
        // Merge MIDI in Loop: what's heard of these tracks' MIDI between the locators, one part on a new
        // track (a copy of the first, its instrument and effects). Returns its id (0: nothing there).
        int mergeMidiInLoop (const std::vector<int>& trackIds);
        // Audition (the Comp tool with Ctrl: the manual's Speaker tool): the engine plays only this
        // track (and only `lane` when it's 0 or more) until stopAudition; the song isn't changed.
        void startAudition (int trackId, int lane);
        void stopAudition();
        bool isAuditioning() const noexcept { return auditioning; }

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
        // MIDI controller lanes (0.25.0, the manual's Controller Display): a part's controller
        // events replaced (one undo step; sorted by start, values kept in range), and the
        // track's lanes as shown (codes in ControllerKinds; saved, not undo steps), its
        // pitchbend grid (1-96 semitones up and down, SNAP, the grid shown), and whether new
        // events ramp (the manual's Type of New Controller Events; settings "controllerRamp",
        // on at first: our choice).
        void setControllers (int trackId, int index, std::vector<Controller>);
        // Drum maps (0.26.0, the manual's Drum Maps): a track's map (by name; "" none; "GM Map"
        // or a map from the drum map folder is added to the song when chosen), the song's maps
        // (edited, renamed (the tracks follow), removed (the tracks lose it)), loading a .drm
        // (or our own map XML), and the drum map folder (Documents/Paragon PR7-S/Drum Maps):
        // maps saved there are offered in every song. None of these are undo steps (as the
        // manual's Drum Map Setup); all are saved with the song.
        static constexpr const char* noDrumMap = "";
        const DrumMap* getDrumMapOf (int trackId) const;
        void setTrackDrumMap (int trackId, const juce::String& mapName);
        // (0.77.2) The map a track gets when one is chosen for it: the PR7-D Map on a track playing PR7-D
        // (unless PR7-D is set to GENERAL MIDI), else the GM Map.
        juce::String defaultDrumMapFor (int trackId) const;
        bool playsPr7d (int trackId) const;
        // A track on a built-in map switched to the one it should have (on open, on Change Instrument).
        void followDefaultDrumMap (int trackId);
        // (0.77.3) HUB SETTINGS' DEFAULT DRUM MAP: a map's name (GM Map, PR7-D Map or one in the drum map
        // folder), or "" for automatic (GM, PR7-D's on PR7-D tracks). Used wherever a map is chosen for you.
        juce::String getDefaultDrumMap() const;
        void setDefaultDrumMap (const juce::String& name);
        bool isDrumMapAvailable (const juce::String& name) const;   // in the song, built in, or in the folder
        void setTrackDrumEditor (int trackId, bool drums);   // the MIDI editor shows its parts as drums
        juce::String addDrumMap (DrumMap);                    // its name, made unique ("... 2")
        bool setDrumMap (const juce::String& name, DrumMap edited);
        void removeDrumMap (const juce::String& name);
        juce::String loadDrumMap (const juce::File&, juce::String& error);   // the name it has in the song ("": not read)
        static juce::File drumMapFolder();
        juce::Array<juce::File> getLibraryDrumMaps() const;   // .drm files in the folder (and its folders), by name
        void setDrumMapFolderForTests (const juce::File& f) { drumMapFolderOverride = f; }
        void setControllerLanes (int trackId, std::vector<int> lanes);
        void setPitchbendGrid (int trackId, int up, int down, bool snap, bool shown);
        bool getNewControllersRamp() const;
        void setNewControllersRamp (bool);

        //======================================================================
        // Recordings. They're files in the song's audio folder ("<song> Audio"
        // beside the song; before it's saved, a folder in Documents), loaded
        // whole, at the engine's sample rate.
        struct AudioData
        {
            juce::AudioBuffer<float> sound;
            std::vector<float> peaks;   // the loudest sample (either side) in each stretch of peakStep samples, for drawing
            // (0.81.0) The file had two sides (a mono file is kept on both): the arrangement draws each
            // side's peaks in its own half, as the other DAW does, so a stereo clip looks stereo.
            bool stereo = false;
            std::vector<float> leftPeaks, rightPeaks;
            void makePeaks();   // peaks, leftPeaks, rightPeaks from `sound`
            static constexpr int peakStep = 256;
            double seconds() const noexcept;
            double sampleRate = 48000.0;
        };
        std::shared_ptr<const AudioData> getAudio (const juce::String& file);   // none if it can't be read
        // A file read whole at the engine's rate, not kept (the MediaBay's previews, 0.21.0).
        std::shared_ptr<const AudioData> readAudio (const juce::File&);
        // (0.34.0) A recording as VariAudio edited it (else getAudio's): what plays.
        std::shared_ptr<const AudioData> getHeardAudio (const juce::String& file);   // as edited (else getAudio's)
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
        // The channel EQ and Pre section (0.43.0; bus id 0 = the master). A drag sets it with
        // undoable false, then once more with true on release (one undo step).
        const ChannelEq* getChannelEq (int busId) const;
        void setChannelEq (int busId, const ChannelEq&, bool undoable = true);
        // (0.78.0) The BOOSTER after the EQ (0 = the master). Drags pass undoable = false and
        // the release true: one undo step.
        const Booster* getBooster (int busId) const;
        void setBooster (int busId, const Booster&, bool undoable = true);
        double getEqResponseDb (int busId, double frequency, bool withPre = true) const;   // the curve, as it plays
        static juce::File eqPresetFolder();   // Documents/Paragon PR7-S/EQ Presets (.pr7eq)
        bool saveEqPreset (int busId, const juce::File&) const;
        bool loadEqPreset (int busId, const juce::File&);   // its bands and Pre onto the channel (one undo step)
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
        // Copy Automation to Other Tracks (0.27.0, the manual's): the automation between the
        // locators, of all tracks (and the master), the selected tracks, or one lane (a parameter);
        // pasted between the locators, there: all tracks to the same tracks and controls, the
        // selected tracks' in order onto the selected tracks, a lane's onto the lane given. What
        // was there is overwritten; a shorter target cuts it, a longer one keeps the last value to
        // the right locator (the manual's); the curve on either side carries on. One undo step.
        enum class AutomationScope { allTracks, selectedTracks, parameter };
        bool copyAutomation (AutomationScope, int busId = 0, int laneIndex = -1);   // false: nothing there
        bool pasteAutomation (AutomationScope, int busId = 0, int laneIndex = -1);
        bool hasAutomationClip() const noexcept { return ! automationClip.empty(); }

        // The Automation panel (0.28.0, the manual's): its settings (saved with the song; none
        // are undo steps) and functions.
        using AutomationSettings = Project::AutomationSettings;
        const AutomationSettings& getAutomationSettings() const noexcept { return project.automationSettings; }
        void setAutomationSettings (const AutomationSettings&);   // (Trim off with Freeze Trim "On Leaving Trim Mode": frozen)
        void setAutomationMode (AutomationSettings::Mode);
        // FILL's keys: off -> once (the next pass) -> locked (every pass) -> off (the manual's One Shot vs Continuous).
        enum class FillOption { toPunch, toStart, toEnd, loop, gaps };
        void stepFill (FillOption);
        AutomationSettings::Fill getFill (FillOption) const noexcept;
        void setSuspendRead (AutomationSettings::Group, bool);
        void setSuspendWrite (AutomationSettings::Group, bool);
        void setAllSuspended (bool read, bool on);   // SET ALL (on: all; off: none)
        bool isAllSuspended() const noexcept;          // Suspend All Read/Write (every group, both)
        void setAllSuspended (bool on);
        void setTrim (bool);
        // Functions.
        void deleteAllAutomation();
        void deleteAutomationOfSelectedTracks();
        void deleteAutomationInRange();                  // between the locators, every track and the master
        void fillGaps (bool withCurrentValue);           // the selected tracks' gaps (virgin territory)
        void freezeTrim (bool selectedTracksOnly);
        void freezeTrim (int busId, int laneIndex);
        void setTrimPoints (int busId, int laneIndex, std::vector<AutomationPoint>);   // one undo step
        // Show: a group's lanes on every track (`usedOnly`: only those with points), Show Used, Hide All.
        void showAutomationGroup (AutomationSettings::Group, bool usedOnly);
        void hideAllAutomation();

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
        void setSignature (int numerator, int denominator);   // the first (bar 1)

        //======================================================================
        // Signature changes (0.45.0, the manual's Time Signature Events and Signature Track).
        // Events sit at bars (from 0: bar 1 on screen) and hold until the next; the first, at
        // bar 1, is the song's own signature and can't be moved or removed. What's placed in
        // the song stays at its beats: the bar lines move (as the manual's, in musical time).
        // Each edit is an undo step; none while the signature track is locked (but the first,
        // the transport's).
        void addSignatureTrack();      // shown (it can be added once)
        void removeSignatureTrack();   // hidden; its events stay (they're the song's bars; the Tempo Track Editor shows them)
        void setSignatureLocked (bool);
        bool setSignatureAt (int bar, int numerator, int denominator);   // bar 0: the first; one already there changes
        bool removeSignatures (std::vector<int> bars);                    // never bar 0
        bool moveSignature (int fromBar, int toBar);                      // onto another: it replaces it
        bool insertSignatureAtCursor (int numerator, int denominator);    // at the bar the playhead is in
        SignatureEvent getSignatureNow() const;                           // at the playhead

        //======================================================================
        // Folder tracks (0.50.0, the manual's Folder Tracks and Organizing Tracks in Folder Tracks).
        // A folder holds tracks (folders too); they sit right after it. Its M and S act on all of
        // them; with its group channel enabled (`setFolderGroup`) it's a group channel: its tracks'
        // outputs go to it (and back to the master when it's switched off or they leave). Removing
        // a folder keeps its tracks (in the folder it was in: our choice). Tracks in a closed
        // folder aren't shown (`getShownTracks`) but play as usual.
        int addFolderTrack (const juce::String& name = {}, bool withGroup = false);
        int moveTracksToNewFolder (std::vector<int> trackIds, bool withGroup);   // the folder where the first was
        bool setTracksFolder (std::vector<int> trackIds, int folderId);          // 0: out of folders
        void setFolderOpen (int folderId, bool open);                            // not an undo step
        void setFolderGroup (int folderId, bool on);
        void setFolderData (int folderId, Track::FolderData);                    // not an undo step
        bool isFolderSoloed (int folderId) const;                                // every track in it soloed
        int folderGroupOf (int trackId) const;   // the nearest folder round it with its group channel on (0: none)

        //======================================================================
        // Markers (0.49.0, the manual's Markers chapter). One marker track (our choice: the manual
        // allows ten). Every edit is an undo step and is refused while the track is locked
        // (locating isn't an edit). Indices are into `getMarkerTrack().markers` (sorted by start).
        const MarkerTrack& getMarkerTrack() const noexcept { return project.markerTrack; }
        void addMarkerTrack();       // shown (added once)
        void removeMarkerTrack();    // hidden; its markers are kept (and come back with it)
        void setMarkerTrackLocked (bool);
        void renameMarkerTrack (const juce::String&);
        void setMarkerSettings (bool showLines, bool showIds, bool cycleFollows);   // not undo steps
        // Added markers show the track. Each returns the marker's index (-1: refused).
        int addPositionMarker (double beat, const juce::String& name = {});
        int addCycleMarker (double start, double end, const juce::String& name = {});
        int addPositionMarkerAtCursor();   // the manual's Insert Marker (Insert)
        int addCycleMarkerAtLocators();    // Insert Cycle Marker: the locators' range
        bool setMarkers (std::vector<Marker>);   // all of them (drags, the Markers window); sorted, IDs kept unique
        bool removeMarkers (std::vector<int> indices);
        bool removeMarkersFrom (double beat);    // the Erase tool's Alt-click: every marker starting there or later
        bool moveMarkersToCursor (const std::vector<int>& indices);   // a cycle marker keeps its length
        void reassignMarkerIds (bool cycle);
        // Locating: the playhead to the marker (a cycle marker's start); with Cycle Follows the
        // locators too (a cycle marker's range; a position marker's: to the next marker, else a bar on).
        bool locateMarker (int index);
        bool cycleToMarker (int index);    // the locators to a cycle marker (a double-click on it)
        bool locateMarkerId (int id);      // To Marker n (Shift+n, number pad 3-9): the position marker with ID n
        bool setMarkerIdAtCursor (int id); // Set Marker n (Ctrl+n): that position marker to the playhead (added if none)
        bool locateNextMarker (bool forward);   // Shift+N / Shift+B

        //======================================================================
        // The SECTIONS row (0.90.0, engine/SongSections.h): the song as blocks end to end from bar 1. Each action is
        // one undo step. Blocks that change place carry their music with them: every part on every track (frozen
        // tracks stay as they are), the chords and scales, markers, automation, tempo and signature changes inside
        // them; what comes after the last block moves by what the row grew or shrank. A block shortened loses
        // what was past its new end; a block removed takes its music with it and the gap closes.
        const SongSections& getSongSections() const noexcept { return project.songSections; }
        std::vector<SectionPlace> getSongSectionPlaces() const;
        void addSongSectionTrack();          // the row shown
        void removeSongSectionTrack();       // hidden, the blocks kept
        // A block after the last, over the bars that follow (nothing moves: it names what's there). Its index.
        int appendSongSection (const juce::String& name, int bars);
        // A block at `index` (0 the first); everything from there on moves later by its length.
        bool insertSongSection (int index, const juce::String& name, int bars);
        bool moveSongSection (int from, int to);          // to: its index once moved
        bool setSongSectionBars (int index, int bars);    // what follows moves by the change
        bool renameSongSection (int index, const juce::String& name);
        bool duplicateSongSection (int index);            // a copy after it, its music copied (linked copies: next release)
        bool removeSongSection (int index);               // and its music; the gap closes
        // A block's place in beats (none: no such block).
        std::optional<SectionPlace> getSongSectionPlace (int index) const;

        //======================================================================
        // Tap tempo (0.45.0, the manual's Tap Tempo Modes): each tap's time in seconds (the
        // transport's TAP, Shift+Space). From the second tap the tempo is the average step of
        // the last eight; a gap of 2 s starts again. Applied at each tap by the mode (not an
        // undo step each: one when the taps stop, so Undo goes back to before tapping).
        enum class TapMode { automatic, selectionStart, playbackStart, cursor, projectTempo, displayOnly };
        static juce::String tapModeName (TapMode);
        TapMode getTapMode() const noexcept { return tapMode; }
        void setTapMode (TapMode);   // (kept in the settings)
        struct TapResult
        {
            double bpm = 0.0;     // 0: not yet (one tap)
            int taps = 0;
            juce::String where;   // what it did ("Tempo event at 5. 1. 1.   0", "Project tempo", "Display only"...)
            double at = -1.0;     // the beat of the event it changed (-1: none)
        };
        // `selectionStart`: the start of what's selected (a range, parts), for Automatic and At Selection Start.
        TapResult tapTempo (double nowSeconds, std::optional<double> selectionStart = std::nullopt);
        const TapResult& getLastTap() const noexcept { return lastTap; }
        bool isTapping (double nowSeconds) const noexcept { return lastTap.taps > 0 && nowSeconds - tapTimes.back() < tapGapSeconds; }
        void endTapping();   // (Studio's timer: the taps stopped; the undo step)
        static constexpr double tapGapSeconds = 2.0;

        //======================================================================
        // Tempo detection (0.45.0, the manual's Tempo Detection panel): the beats of an audio clip
        // (as heard) or a MIDI part, at least 7 s long. `applyDetectedBeats` makes the tempo
        // track follow them from the part's start: a tempo event on each beat (the first on
        // the whole beat that keeps the tempo nearest; the stretch from the part's start to it
        // set so it lands), the tempo track on and shown, events from the part's start on
        // replaced. Everything in the song keeps its place in time (the manual's linear time
        // base while it analyses): parts, notes, controllers, automation, chords and the
        // locators move to the beats they now sound at. One undo step.
        bool analyseTempo (int partId, BeatAnalysis& result, juce::String& error);
        bool applyDetectedBeats (int partId, const std::vector<double>& beatSeconds, juce::String& report);
        // The tempo events put back (Reset): everything moved to where it sounds by those.
        void restoreTempo (bool on, std::vector<TempoEvent> events);
        // Every beat position moved so it sounds where it did under `before` (the song now follows `after`).
        void keepInTime (const TempoMap& before, const TempoMap& after);

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
        // (0.83.0) The Edit History (the manual's Edit History Dialog): every step kept, oldest first;
        // `getHistoryPosition` the one the song is at now (the separator under it: the ones after are
        // undone); `goToHistory` puts any step back (undo or redo as far as it takes, one engine update).
        // Step 0 is where the song started (opened, new, imported): it can't be undone.
        // (0.83.2) Each step's name is worked out here, when the history is looked at, not on every edit
        // (comparing the whole song with the step before cost more than the undo copy itself).
        const std::vector<HistoryStep>& getHistory() const;
        int getHistoryPosition() const noexcept { return undoIndex; }
        void goToHistory (int index);
        void setHistoryDetails (int index, const juce::String& text);   // the Details column: anything typed
        // The manual's Maximum Undo Steps (Preferences > General): 1-1000, default 100; settings `maxUndoSteps`.
        int getMaxUndoSteps() const;
        void setMaxUndoSteps (int steps);

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
        // (0.71.0, the owner asked) What the song opened without: an instrument, an effect or a recording
        // not on this computer. `removeMissing` takes them out of the song: missing effects' slots emptied,
        // clips whose recording is missing deleted, and a missing instrument's track given the studio's synth
        // (its parts kept). One undo step; returns how many were taken out.
        struct MissingItem
        {
            enum class Kind { instrument, effect, recording };
            Kind kind = Kind::effect;
            int trackId = 0;     // (effects: 0 = the master)
            int slot = -1;       // (effects)
            juce::String file;   // (recordings)
        };
        const std::vector<MissingItem>& getMissing() const noexcept { return missingItems; }
        int removeMissing();
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
            int channel = 0;   // (0.51.0) a channel export: only this track or channel reaches the file (0: the mix)
            bool mono = false; // (0.78.0) a one-channel file: the two sides averaged (Render in Place of a mono track)
        };
        // (0.51.0, the manual's Export Audio Mixdown: Channel Selection Multiple, Export Range Cycle
        // Markers) Several files in one go: each channel chosen (none: the main mix) over each cycle
        // marker chosen (none: the options' own range), named "<base> - <channel> - <marker>.wav" in
        // `folder` (an existing name: a number added). Channels' files leave out the master's inserts,
        // EQ and fader unless `includeMaster`. Plug-ins stay offline across them (one batch).
        struct ExportBatch
        {
            ExportOptions options;
            std::vector<int> channels;
            std::vector<int> cycleMarkers;   // indices into the marker track
            bool includeMaster = false;
        };
        bool exportBatch (const juce::File& folder, const juce::String& baseName, const ExportBatch&, juce::String& error,
                          std::vector<juce::File>* written = nullptr);
        std::vector<int> getExportableChannels() const;   // tracks and channels a file can be made of (not plain folders)
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
            int bitsPerSample = 32;   // 16, 24 or 32 (float: 0.31.0's default, the owner's: nothing over 0 dB is cut off)
            // The source: kept as it was, muted (the parts rendered; whole tracks: the track), or removed (whole tracks).
            // Or (whole tracks) disabled: muted and its plugins unloaded, as Freeze (0.9.1; muted too since 0.31.0).
            // A stretch of a track (a part, a range, the cycle): disable and remove mute the parts rendered there.
            enum class Source { keep, mute, remove, disable };
            Source source = Source::disable;   // (0.31.0's default, the owner's: muted, disabled, hidden)
            bool hideSource = true;   // whole tracks: hidden after (the manual's Hide Source Tracks)
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
        // (0.90.0) The SECTIONS row's moves: the new blocks, each from an old block's index (its music carried, cut to
        // its new length) or -1 (new and empty: what was there moves on). One undo step.
        struct SectionMove { int from = -1; SongSection block; };
        bool relayoutSections (const std::vector<SectionMove>&);
        PadPlayer padPlayer;
        std::unique_ptr<PatternPerformer> performer;   // (0.39.0) the pads' performance
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
            bool released = false;   // let go (an on/off switch latches on until stop; Auto-Latch, Cross-Over: every control)
            bool retouched = false;  // Cross-Over: touched again after letting go (crossing the old curve punches out)
            std::optional<bool> aboveOld;   // Cross-Over: which side of the old curve the hand was on
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
        void punchOut (size_t pass, double endBeat, bool returnToOld = true);
        void startPass (int busId, const AutomationLane& control);
        static void freezeTrimOf (AutomationLane&);
        void punchOutAll();
        void setControlValue (int busId, const AutomationLane& control, float value);   // the static setting, without writing
        void effectGoing (int busId, int slot);
        void removeAllEffects();
        void recordUndoState();
        Project copyWithoutPluginStates (bool forUndo) const;   // (0.83.1)
        bool applyAutoColour();   // (0.79.0) the rule's colours onto its tracks; true if any changed
        void restore (const Project&);
        void joinFolderGroup (Track&);    // (0.50.0)
        void leaveFolderGroup (Track&);
        static void restoreLanes (std::vector<AutomationLane>& lanes, const std::vector<AutomationLane>& then);
        void updateLiveTrack();
        void finishRecording();
        void finishAudioRecording();
        bool auditioning = false;   // (0.30.0) the engine plays a soloed copy
        struct ComposerAudition
        {
            double start = 0.0, end = 0.0, wasAt = 0.0;
            std::vector<std::pair<int, LineIdea>> layers;   // (0.76.0: kept, so a change to the mix plays on)
        };
        // (0.76.0) The song as the composer is heard: the layers as parts on their tracks, those alone (or the
        // ones among them you soloed), each track's own fader, pan, inserts and mute. Empty: none can play.
        std::optional<Project> composerAuditionCopy() const;
        std::optional<ComposerAudition> composerAudition;   // (0.58.0)
        juce::String composerState;                         // (without a settings file)
        void endComposerAudition (bool backToSong);
        std::map<juce::String, std::pair<std::shared_ptr<const AudioData>, std::vector<Hitpoint>>> hitpointCache;   // (0.32.0) every candidate, by recording and the data they came from
        void forgetAudio (const juce::String& name);   // (0.34.1) a recording and everything made from it
        void forgetAllAudio();
        WarpRenderer warpRenderer;
        // (0.34.0) VariAudio: each recording's pitch, and its sound as edited (by what was edited).
        void noteSpikes();                  // (0.57.3) the engine's spike report into the session log
        juce::uint32 lastSpikeNote = 0;
        int spikesNoted = 0;
        juce::String variSignature (const juce::String& file) const;
        // (0.57.2) Each recording's signature with the edits it was made from: written out again only
        // when they differ (it was written for every clip of the file on every change in the song).
        mutable std::map<juce::String, std::pair<Project::VariAudio, juce::String>> variSignatures;
        std::map<juce::String, std::pair<std::shared_ptr<const AudioData>, std::shared_ptr<const PitchAnalysis>>> pitchCache;
        struct HeardSound { juce::String signature; std::shared_ptr<const AudioData> source, heard; };
        std::map<juce::String, HeardSound> heardCache;
        std::map<juce::String, juce::String> warpedVariSignatures;
        std::pair<Track*, int> findPartById (int partId);
        bool editWarp (int partId, const std::function<bool (Part&)>&);
        void tidyWarp (Part&);   // markers in order, apart, inside the recording, edges at the clip's
        double recordingLatency() const;
        void buildRecordingNotes (std::vector<Note>& notes, double partStart, double endBeat, int onlyPass = -1) const;
        // (0.29.0) New takes onto the track by the record modes: each on the first lane it
        // fits without overlapping (or `lane` if given), in front, in order; `replace`:
        // what's there over [from, to) cut out first.
        void placeTakes (Track&, std::vector<Part> takes, double from, double to, bool replace, int lane = -1);
        void buildRecordingControllers (std::vector<Controller>&, double partStart) const;   // (0.25.0: each kind's last pass in a cycle)
        bool newControllersRamp = true;   // (without a settings file)
        juce::File drumMapFolderOverride;
        juce::String defaultDrumMapHere;   // (0.77.3: without a settings file)
        int editorSoloTrack = 0;           // (0.78.0) setEditorSolo
        bool addDrumMapToSong (const juce::String& name);   // false: not found
        // Copied automation: each lane's control, its bus, and its points from the range's start.
        struct AutomationClipLane { int busId = 0; AutomationLane lane; };
        std::vector<AutomationClipLane> automationClip;
        double automationClipLength = 0.0;
        void timerCallback() override;
        void changeListenerCallback (juce::ChangeBroadcaster*) override;   // the device settings changed: keep them

        std::unique_ptr<juce::PropertiesFile> settings;
        std::unique_ptr<ProjectLibrary> library;
        std::unique_ptr<PluginHost> host;
        juce::AudioDeviceManager devices;
        bool withDevices = false;
        bool multiCoreAudio = false;   // (without a settings file)
        bool softwareDrawing = false;  // (without a settings file)
        void applyMultiCore();
        Engine engine;

        Project project;
        juce::File file;
        bool modified = false;
        bool masterSelected = false;
        bool prepareSave (const juce::File&);
        // The song's recordings and freeze files (and its undo history's) copied from one folder to another.
        bool copyAudioFiles (const juce::File& from, const juce::File& to);
        void sendGone (Track&, int slot);
        void mixHeardClip (juce::AudioBuffer<float>&, const Track& among, int index, double startSeconds, double rate);
        // (0.82.6) Clips of `track` (by index) as heard, written into one new file; `glued` becomes one clip of it.
        bool glueIntoFile (const Track&, const std::vector<int>& indices, Part& glued, juce::String& error);
        bool linksSuspended = false, linksAbsolute = false, qLink = false;
        bool mirroring = false;
        juce::StringArray progressionFavorites;   // (without a settings file)
        MelodySettings melodySettings;   // (without a settings file)
        std::atomic<int> lastPlayedPad { -1 };
        PadLearn padLearn = PadLearn::none;
        int padLearnIndex = 0, rangeFirst = -1;
        std::mutex chordLogLock;
        std::vector<ChordEvent> chordLog;   // pads pressed while recording (beats)
        std::atomic<bool> loggingChords { false };
        void takeRecordedChords (double from, double to, const std::vector<HeardNote>& played);
        DetectSettings detectSettings;
        juce::String guitarTuning;   // (without a settings file: the tests)
        juce::String bassTuning;
        std::optional<juce::String> instrumentProfiles;   // (without a settings file: the tests)
        GuitarVelocities guitarVelocities;
        GuitarStyle guitarStyle = GuitarStyle::rock;   // (without a settings file)
        SoloStyle soloStyle = SoloStyle::automatic;    // (without a settings file)
        RiffSettings riffSettings;
        DrumSettings drumSettings;
        juce::File drummerFolderOverride;
        std::vector<BuiltSection> builtSections;
        std::optional<ComposerRiff> composerRiff;
        juce::String ideasDrummer;
        float ideasHumanize = 0.0f, humanizeAmount = 0.5f;   // (without a settings file: the tests)
        bool ideasFeel = true, humanizeLayers = true;
        void announce();               // (0.34.3) sendChangeMessage, the version counted
        void mixOnly (bool onlyTheMix);
        juce::uint64 changeVersion = 1;
        bool mixOnlyAsked = false, mixOnlyBroken = false;
        AlignSettings alignSettings;   // (without a settings file: the tests)
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
        std::vector<MissingItem> missingItems;
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
        TapMode tapMode = TapMode::automatic;
        std::vector<double> tapTimes;
        TapResult lastTap;
        bool tapUnrecorded = false;
        int recordTrack = 0;
        std::vector<RecordedEvent> recorded;
        int quietRecordingTicks = 0;

        // Recording audio: the file being written, and its writer.
        juce::AudioFormatManager formats;
        juce::TimeSliceThread writerThread { "Paragon PR7-S recorder" };
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> audioWriter;
        juce::File audioRecording;
        std::map<juce::String, std::shared_ptr<const AudioData>> audioCache;
        double audioCacheRate = 0.0;
        juce::File unsavedAudioFolder;
        juce::File audioFolderOverride;   // (0.72.0: a backup opened: its song's recordings, where they are)
        juce::File lastBackup;
        juce::int64 lastBackupHash = 0;
        juce::uint32 lastBackupTime = juce::Time::getMillisecondCounter();
        int autoSaveMinutesHere = 0, autoSaveKeepHere = defaultAutoSaveKeep;
        std::array<TrackTypeLook, numTrackTypes> trackTypeLooksHere;   // (0.80.0; without a settings file)
        std::array<TrackTypeLook, numTrackTypes> readTrackTypeLooks() const;
        void giveTypeLook (Track&, int index) const;   // a new track's colour and list colour   // (without a settings file: the tests)
        void collectPluginStates();
        void backupIfDue();
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
            bool mono = false;   // (0.78.0) written as one channel
        };
        std::unique_ptr<ExportJob> exportJob;
        static bool writeBlock (ExportJob&, juce::AudioBuffer<float>&, int start, int length);
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
        mutable std::vector<HistoryStep> undoInfo;   // (0.83.0) one per undo state: its name, time, details
        void dropOldestStep();
        int undoIndex = -1;
        int maxUndoHere = 100;               // (without a settings file)
        void clearHistory();
    };
}
