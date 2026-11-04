#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <optional>

#include "Chords.h"
#include "DrumMaps.h"
#include "FollowChords.h"
#include "TempoMap.h"
#include "Eq.h"

// A song: its tempo and time signature, the cycle (loop) range, and its
// tracks, each with an instrument and the MIDI parts on the timeline.
//
// Musical time is in beats (quarter notes), as doubles, from the song's
// start (0). Bars follow the time signature: in 4/4 a bar is 4 beats, in
// 6/8 it's 3. Everything here is plain data, used on the message thread;
// the Engine gets its own copy to play.
namespace pr7s
{
    struct Note
    {
        double start = 0.0;     // beats from the part's start
        double length = 0.25;   // beats
        int pitch = 60;         // MIDI note number, 60 = middle C
        float velocity = 0.8f;  // 0 to 1

        bool operator== (const Note&) const = default;
    };

    // A MIDI controller event in a part (0.25.0, the manual's Controller Display): a
    // continuous controller (CC), pitchbend, aftertouch, poly pressure or a program change.
    // It has no length: its value holds until the next event of its kind. `ramp`: the way
    // from the event before (of its kind, in the part) to this one is a ramp, else a step.
    struct Controller
    {
        enum class Type { cc, pitchbend, aftertouch, polyPressure, programChange };   // saved by name
        Type type = Type::cc;
        int number = 1;          // a CC's number (0-127), poly pressure's note; else 0
        double start = 0.0;      // beats from the part's start
        int value = 0;           // 0-127; pitchbend 0-16383 (8192: no bend)
        bool ramp = false;

        bool operator== (const Controller&) const = default;
    };

    // A warp marker (0.32.0, the manual's Free Warp): a moment of an audio clip's recording
    // (`seconds` into its file) pinned to a place in the clip (`beat`, from the clip's start).
    // Between markers the audio is stretched to fit; with any markers, the clip's first and
    // last are at its edges (0 and its length).
    struct WarpMarker
    {
        double beat = 0.0;
        double seconds = 0.0;
        bool operator== (const WarpMarker&) const = default;
    };

    struct Part
    {
        double start = 0.0;     // beats from the song's start
        double length = 4.0;    // beats
        juce::String name;
        std::vector<Note> notes;
        std::vector<Controller> controllers;   // (0.25.0) by start
        int id = 0;             // given by the studio, so the key editor keeps the same part however the list changes (not saved)

        // An audio track's part (a clip): the recording it plays, from
        // `audioOffset` seconds into it (the trimmed start), faded in and out.
        juce::String audioFile;      // its file's name, in the song's audio folder
        double audioOffset = 0.0;    // seconds
        double fadeIn = 0.0, fadeOut = 0.0;   // seconds
        bool muted = false;          // the Mute tool: kept and edited as usual, but not played
        // Takes (0.29.0, the manual's Lanes, Takes, and Overlapping Events): the lane it's on
        // (0 the first; shown under the track with Show Lanes), and where several audio clips
        // overlap, which one plays: the highest `order` (equal: the later in the list), as an
        // audio track plays one clip at a time. MIDI parts all play (only muting silences them).
        int lane = 0;
        int order = 0;
        // (0.32.0) Warp markers, sorted by beat (none: the recording plays at its own speed).
        std::vector<WarpMarker> warp;
        bool isWarped() const noexcept { return warp.size() >= 2; }

        bool isAudio() const noexcept { return audioFile.isNotEmpty(); }
        double end() const noexcept { return start + length; }
        bool operator== (const Part&) const = default;
    };

    // What plays a track: the studio's synth (PR7-K since 0.9.11; the kind
    // keeps its old name, so songs saved before open on PR7-K), or a VST3 instrument.
    struct Instrument
    {
        enum class Kind { paragonSynth, plugin };   // saved by name, not number
        Kind kind = Kind::paragonSynth;
        juce::PluginDescription plugin;   // for Kind::plugin: which one
        juce::MemoryBlock state;          // its settings, as it last saved them
        juce::String getName() const;
    };

    // An effect in a channel's inserts (a VST3 effect, such as our PR7
    // plugins). The sound goes through a channel's inserts in order. Each
    // is in a slot of its own (1 to 16); a slot can be empty.
    struct Insert
    {
        juce::PluginDescription plugin;
        juce::MemoryBlock state;     // its settings, as it last saved them
        bool bypass = false;         // off: the sound goes past it
        bool isEmpty() const noexcept { return plugin.name.isEmpty() && plugin.fileOrIdentifier.isEmpty(); }
    };

    // Automation (inspired by Cubase's workflow): an automation lane (an
    // "automation track" under its track) of points that moves one control
    // as the song plays: a track's volume, pan or mute, or a knob of its
    // instrument or of an effect in its inserts; the master's volume or its
    // effects'. The track's R (read) plays them back, its W (write) records
    // what's done to its controls while playing.
    // Values are the control's travel, 0 to 1, as plugins keep theirs: for
    // the volume, the fader's travel (so a straight line between two points
    // moves as the fader would); for pan, left (0) to right (1); for mute,
    // off below a half, on from it (a jump, not a ramp).
    struct AutomationPoint
    {
        double beat = 0.0;    // from the song's start
        float value = 0.0f;   // 0 to 1
        // (0.27.0, the manual's Bézier curves) How the ramp arriving at this point bends:
        // 0 straight; up to 1 bowed one way, down to -1 the other (`curveShape`).
        float curve = 0.0f;
        // (0.28.0, the manual's Defining a Terminator Point) The curve ends here: a gap (virgin
        // territory: the control stays as it's set) until the next point.
        bool terminator = false;
        bool operator== (const AutomationPoint&) const = default;
    };

    struct AutomationLane
    {
        enum class Target { volume, pan, mute, instrument, insert, send };   // saved by name
        Target target = Target::volume;
        int slot = 0;                 // Target::insert: which insert (0 to 15); Target::send: which send (0 to 7)
        int parameter = 0;            // Target::instrument and insert: the plugin's parameter, by index
        juce::String parameterName;   // as the plugin named it (shown if the plugin is missing)
        std::vector<AutomationPoint> points;   // in order of beat
        bool shown = true;     // its lane open under the track (a lane switched to another control keeps its points, hidden)
        bool muted = false;    // Mute Automation: not played back
        // (0.28.0, the manual's Trim) Volume and send levels: a trim curve over the points, in
        // the middle at 0 dB, up to `trimRange` dB either way; played on top of them.
        std::vector<AutomationPoint> trim;
        static constexpr float trimRange = 12.0f;   // our choice
        bool canTrim() const noexcept { return target == Target::volume || target == Target::send; }

        bool isStep() const noexcept { return target == Target::mute; }
        bool sameControl (const AutomationLane& other) const noexcept
        {
            return target == other.target && slot == other.slot && parameter == other.parameter;
        }
        // The value at a beat: the first point's before it, the last's after,
        // a straight line between (a jump at the next point, for a step lane).
        // None if there are no points.
        std::optional<float> valueAt (double beat) const noexcept;
        // (0.28.0) With virgin territory: nothing before the first point, nor in a gap after a
        // terminator (else as valueAt, which a gap doesn't stop).
        std::optional<float> valueAt (double beat, bool virginTerritory) const noexcept;
        // What plays: the value with the trim curve on top (volume and sends).
        std::optional<float> playedAt (double beat, bool virginTerritory) const noexcept;
        static float trimDb (const std::vector<AutomationPoint>& trim, double beat) noexcept;   // 0 without points
        // A ramped (or stepped) curve's value at a beat, read in place (points not empty).
        static float valueOf (const std::vector<AutomationPoint>& points, double beat, bool step) noexcept;
        // A ramp's shape (0.27.0): `along` (0 to 1) bent by `curve`: along ^ 4^-curve.
        static float curveShape (float along, float curve) noexcept;
        bool operator== (const AutomationLane&) const = default;
    };

    // The controls' travel and their values.
    float volumeToTravel (float db) noexcept;      // -60 dB (silent) at 0, +6 at 1, as the mixer's fader moves
    float travelToVolume (float travel) noexcept;
    inline float panToTravel (float pan) noexcept    { return (pan + 1.0f) * 0.5f; }
    inline float travelToPan (float travel) noexcept { return travel * 2.0f - 1.0f; }

    // A send (inspired by Cubase's workflow): some of a channel's sound to an
    // FX channel, before or after its fader. Its slot is its place in the
    // channel's list (8 of them).
    struct Send
    {
        int target = 0;           // the FX channel's track id (0: none)
        float levelDb = 0.0f;     // -60 (silent) to +6, as a fader
        bool on = true;
        bool preFader = false;    // before the fader (and pan); after it otherwise
        bool operator== (const Send&) const = default;
    };

    // A link group (0.17.0, as the manual's Linking Channels): channels whose
    // settings follow each other. Which settings are linked is the group's
    // (the Link Group Settings dialog; the manual's defaults: volume, sends,
    // routing and selection). Volume (and pan, send levels) follow relatively,
    // keeping the channels' differences, unless the mixer's ABS is on.
    // No EQ or Dynamics (the studio has neither as a channel module).
    struct LinkGroup
    {
        enum class What { volume, pan, sends, inserts, muteSolo, automation, routing, selection, recordMonitor };
        int id = 0;
        juce::String name;
        bool volume = true, pan = false, sends = true, inserts = false, muteSolo = false,
             automation = false, routing = true, selection = true, recordMonitor = false;
        bool links (What) const noexcept;
        void setLinks (What, bool) noexcept;
        bool operator== (const LinkGroup&) const = default;
    };

    struct Track
    {
        // An instrument track plays MIDI parts through its instrument; an
        // audio track plays clips (recordings) and records from the interface.
        // An FX channel takes what the sends give it, through its inserts (a
        // reverb, a delay); a group channel takes the tracks routed to it (a
        // submix: one fader and the same effects for all of them). Neither
        // has parts.
        enum class Kind { instrument, audio, fx, group };   // saved by name

        int id = 0;                      // stays the same for the track's life (undo and the engine use it)
        Kind kind = Kind::instrument;
        juce::String name;
        juce::Colour colour;
        Instrument instrument;
        float volumeDb = 0.0f;           // -60 (silent) to +6
        float pan = 0.0f;                // -1 left to +1 right
        bool mute = false, solo = false;
        bool armed = false;              // record-enabled: recording goes here
        // Audio tracks: which of the interface's inputs it records (the first
        // of the pair, for stereo; counted from 0 among the inputs switched on
        // in SETTINGS), and whether what comes in is heard (monitored).
        int input = 0;
        bool stereoInput = false;
        bool monitor = false;
        // Instrument tracks: the MIDI input it listens to when it's the live
        // track (a device's name; empty: all inputs), and the channel its notes
        // go to the instrument on (1 to 16; the Inspector's Routing, 0.9.0).
        juce::String midiInput;
        int midiChannel = 1;
        // Disabled (0.9.1, as the manual's Disable Track): silent, its instrument
        // and effects unloaded (their settings kept) so it costs no CPU or RAM.
        // Hidden: not in the track list, the arrangement or the mixer (the left
        // zone's VISIBILITY shows it again). Both saved; neither an undo step.
        bool disabled = false, hidden = false;
        // Frozen (0.9.8, the manual's Freeze Channel): its instrument and inserts
        // rendered to `freezeFile` (in the song's audio folder, "Freeze/..."),
        // which plays instead; the plugins unloaded (their settings kept), the
        // parts locked; fader, pan and sends still live. Saved.
        bool frozen = false;
        juce::String freezeFile;
        // Quick Controls (0.9.6, as the manual's Track Quick Controls): 8 slots,
        // each any of the track's controls (as an automation lane names it:
        // volume, pan, mute, a send, an instrument's or an effect's knob), or none.
        static constexpr int numQuickControls = 8;
        std::array<std::optional<AutomationLane>, numQuickControls> quickControls;
        // The MIDI editor's controller lanes for this track (0.25.0; ControllerKinds' codes,
        // top to bottom; empty: the display hidden), and the pitchbend lane's semitone grid
        // (the manual's Grid Settings: 1-96 up and down, 2 at first; saved with the song).
        std::vector<int> controllerLanes { -1 };
        int pitchbendUp = 2, pitchbendDown = 2;
        // Its drum map (0.26.0; one of Project::drumMaps by name; empty: none), and whether the
        // MIDI editor shows its parts as drums (on whenever a map is chosen, at first).
        juce::String drumMap;
        bool drumEditor = false;
        bool pitchbendSnap = false, pitchbendGrid = false;
        // Takes (0.29.0): its lanes shown under it (Show Lanes), and a lane soloed (-1: none):
        // then only that lane's parts play.
        bool showLanes = false;
        int soloLane = -1;
        int numLanes() const noexcept;   // one more than the highest lane a part is on (at least 1)
        bool isAudio() const noexcept { return kind == Kind::audio; }
        bool isFx() const noexcept { return kind == Kind::fx; }
        bool isGroup() const noexcept { return kind == Kind::group; }
        bool isChannel() const noexcept { return kind == Kind::fx || kind == Kind::group; }   // no parts, no instrument
        int output = 0;                  // a group channel's id, or 0: the master (FX and group channels: the master)
        std::vector<Send> sends;         // by slot (none after the last used); FX channels have none
        bool sendsBypass = false;        // all of its sends off at once
        std::vector<Part> parts;
        std::vector<Insert> inserts;     // its effects, by slot (none after the last one used)
        int preFaderSlots = 16;          // slots before the fader and pan (the rest after them)
        std::vector<AutomationLane> automation;   // in the order shown
        bool automationOpen = false;     // its lanes shown under it
        bool automationRead = false, automationWrite = false;   // R and W (W turns R on)
        int linkGroup = 0;               // the link group it's in (0.17.0; 0: none)
        // Follow Chord Track (0.36.0, the Inspector's CHORDS): how its notes follow the chord
        // track, the chords they were played over (empty until following first begins: the
        // dialog), and Live Input (keys played onto the chord or scale in force).
        FollowMode follow = FollowMode::off;
        FollowReference followReference;
        LiveFollow liveFollow = LiveFollow::off;
        // (0.38.0) Single Voice's voice, and This Track's Voicings (none: the chord track's).
        Voice followVoice = Voice::soprano;
        std::optional<VoicingSettings> followVoicing;
        // (0.43.0) The channel EQ and Pre section (audio-carrying channels: all but MIDI-only).
        ChannelEq eq;
    };

    struct Project
    {
        double bpm = 120.0;                   // the fixed tempo (the tempo track off)
        // The tempo track (0.10.0, the manual's): on (Activate Tempo Track),
        // the song follows its events (the first at beat 0); off, `bpm`.
        bool tempoTrackOn = false;
        std::vector<TempoEvent> tempoEvents;  // sorted by beat; empty until the track is first added
        bool tempoTrackShown = false;         // the tempo track in the track list (it can be added once)
        bool tempoLocked = false;             // its Lock: its events can't be edited
        enum class NewTempoType { ramp, step, automatic };
        NewTempoType newTempoType = NewTempoType::ramp;   // the Type of New Tempo Points
        double tempoViewLow = 60.0, tempoViewHigh = 180.0;   // the curve's shown range (Visible Tempo Lower/Upper Limit)
        ChordPads chordPads = ChordPads::defaults();         // the chord pads (0.14.0): kept with the song, as the manual's
        ChordTrack chordTrack;                                // the chord track (0.16.0)
        std::vector<DrumMap> drumMaps;                        // (0.26.0) the song's (the manual: saved with the song); names unique
        std::vector<LinkGroup> linkGroups;                    // (0.17.0) by id; each with two channels or more
        int numerator = 4, denominator = 4;   // the time signature
        bool loopOn = false;                  // CYCLE
        double loopStart = 0.0, loopEnd = 16.0;
        bool click = false;                   // the metronome
        // Snap (J) and the quantize value, in beats (0.25: 1/16), the toolbar's (0.9.10, the manual's Snap and Quantize Presets).
        bool snapOn = true;
        double quantizeBeats = 0.25;
        std::vector<Track> tracks;
        float masterVolumeDb = 0.0f;          // the master bus: every track, then these effects, then this volume
        std::vector<Insert> masterInserts;
        int masterPreFaderSlots = 16;
        ChannelEq masterEq;                   // (0.43.0)
        std::vector<AutomationLane> masterAutomation;   // volume and the master's effects
        // The Automation panel's settings (0.28.0; saved with the song, not undo steps).
        struct AutomationSettings
        {
            enum class Mode { touch, autoLatch, crossOver };
            Mode mode = Mode::touch;
            // Fill (on punching out): off, once (the next pass), or locked (every pass).
            enum class Fill { off, once, locked };
            Fill toPunch = Fill::off, toStart = Fill::off, toEnd = Fill::off, loop = Fill::off, gaps = Fill::off;
            // Suspend Read / Write, by group (AutomationSettings::Group), and all at once.
            enum class Group { volume, pan, mute, sends, plugins };
            static constexpr int numGroups = 5;
            std::array<bool, numGroups> suspendRead {}, suspendWrite {};
            bool trim = false;
            enum class FreezeTrim { manually, onPassEnd, onLeavingTrim };
            FreezeTrim freezeTrim = FreezeTrim::manually;
            bool virginTerritory = false, continueOnJump = false, revealOnWrite = true;
            double returnSeconds = 0.1;   // our choice (the manual gives none)
            int reductionLevel = 50;      // % (the manual's default)
            static Group groupOf (const AutomationLane&) noexcept;
            bool operator== (const AutomationSettings&) const = default;
        };
        AutomationSettings automationSettings;
        // The record modes (0.29.0, the manual's Audio Record Modes, MIDI Record Modes and MIDI
        // Cycle Record Modes; saved with the song). The manual's defaults.
        struct RecordModes
        {
            enum class Audio { keepHistory, cycleHistoryReplace, replace };
            enum class Midi { newParts, merge, replace };
            enum class MidiCycle { mix, overwrite, keepLast, stacked, mixStacked };
            Audio audio = Audio::keepHistory;
            Midi midi = Midi::newParts;
            MidiCycle midiCycle = MidiCycle::mix;
            bool operator== (const RecordModes&) const = default;
        };
        RecordModes recordModes;
        // Hitpoints (0.32.0, the manual's Hitpoints section): found in each recording; how
        // strict the finding is (Threshold 0-1: the quieter ones dropped; Minimum Length: none
        // closer than this), and what was changed by hand per recording (seconds added / removed).
        struct HitpointSettings
        {
            double threshold = 0.25, minimumSeconds = 0.06;
            bool operator== (const HitpointSettings&) const = default;
        };
        HitpointSettings hitpointSettings;
        struct HitpointEdits
        {
            std::vector<double> added, removed;
            bool operator== (const HitpointEdits&) const = default;
        };
        std::map<juce::String, HitpointEdits> hitpointEdits;   // by recording (file name)
        bool audioWarpQuantize = false;   // Q on audio: stretched to the grid (else the clip's start moved)
        // VariAudio (0.34.0, the manual's Pitch Editing and Time Correction with VariAudio): per
        // recording, its segments (the notes found in it: where they are in the recording, the
        // pitch heard, as a MIDI note number with cents) and what was done to each.
        struct VariSegment
        {
            double start = 0.0, end = 0.0;   // seconds into the recording
            float pitch = 60.0f;             // the average pitch heard (MIDI note, fractional)
            float shift = 0.0f;              // semitones moved (the segment's pitch is pitch + shift)
            float straighten = 0.0f;         // 0-1: how much the curve's wobble around its pitch is flattened
            float volumeDb = 0.0f;           // the segment's level
            // (0.35.0) Its curve tilted: semitones at its start and its end, none at the anchor
            // (0-1 along it; the manual's Tilt/Rotate Anchor, the middle by default).
            float tiltStart = 0.0f, tiltEnd = 0.0f, anchor = 0.5f;
            // (0.35.0) The part of it Straighten works on (0-1 along it: the manual's range).
            float straightenFrom = 0.0f, straightenTo = 1.0f;
            float formant = 0.0f;            // (0.35.0) the formant shift, -100 to +100 %
            float pitchNow() const noexcept { return pitch + shift; }
            bool changesPitch() const noexcept   // (what the pitch shifter has to work on)
            {
                return std::abs (shift) > 1.0e-4f || straighten > 1.0e-4f || std::abs (tiltStart) > 1.0e-4f || std::abs (tiltEnd) > 1.0e-4f
                    || std::abs (formant) > 1.0e-3f;
            }
            bool isEdited() const noexcept { return changesPitch() || std::abs (volumeDb) > 1.0e-3f; }
            bool operator== (const VariSegment&) const = default;
        };
        struct VariAudio
        {
            std::vector<VariSegment> segments;   // in time order
            bool bypass = false;                 // the manual's VariAudio Changes Bypass
            bool isEdited() const noexcept { return std::any_of (segments.begin(), segments.end(), [] (const VariSegment& s) { return s.isEdited(); }); }
            bool operator== (const VariAudio&) const = default;
        };
        std::map<juce::String, VariAudio> variAudio;   // by recording (file name)
        int variAudioReference = 0;   // (0.35.0) the MIDI track shown in the VariAudio editor as lines (0: none)
        bool masterAutomationOpen = false, masterRead = false, masterWrite = false;
        int nextTrackId = 1;

        static constexpr double minBpm = 20.0, maxBpm = 300.0;
        static constexpr int maxInserts = 16;  // insert slots on a channel (and on the master), as the manual has
        static constexpr int maxSends = 8;     // send slots on a channel (our choice: the manual gives no number)

        double beatsPerBar() const noexcept { return numerator * 4.0 / denominator; }
        double secondsPerBeat() const noexcept { return 60.0 / bpm; }   // the fixed tempo's (with the tempo track on: tempoMap)
        // Beats to seconds and back, by the tempo track when it's on.
        TempoMap tempoMap() const;
        double bpmAt (double beat) const;
        bool followsTempoTrack() const noexcept { return tempoTrackOn && ! tempoEvents.empty(); }

        LinkGroup* findLinkGroup (int id);
        const LinkGroup* findLinkGroup (int id) const;
        std::vector<int> linkGroupMembers (int groupId) const;   // in the tracks' order
        Track* findTrack (int id);
        const Track* findTrack (int id) const;
        const DrumMap* findDrumMap (const juce::String& name) const;
        const DrumMap* drumMapOf (const Track&) const;   // its map, if it has one
        Track* armedTrack();
        const Track* armedTrack() const;

        // The song's length: up to the end of its last part (0 if empty).
        double lastBeat() const;

        std::unique_ptr<juce::XmlElement> toXml() const;
        // Nothing if the XML isn't a PR7-S project (or is from a newer version).
        static std::optional<Project> fromXml (const juce::XmlElement&);

        static constexpr const char* fileExtension = ".pr7s";
    };

    // Positions as the transport shows them: "bar. beat. sixteenth. tick" from
    // 1.1.1.0 (ticks: 120 to a sixteenth, 480 to a beat), and minutes:seconds.
    juce::String formatBarsBeats (double beats, const Project&);

    // Takes (0.29.0): the stretches (beats) of a track's part that are heard. Muted parts and,
    // with a lane soloed, other lanes' parts: none. A MIDI part: all of it (overlapping MIDI
    // parts all play). An audio clip: what no clip in front of it covers (the manual: "each
    // track can only play back a single audio event at a time ... only the one that is in the
    // front is played back").
    std::vector<std::pair<double, double>> heardStretches (const Track&, int partIndex);
    // Whether part a is in front of part b (higher order; equal: later in the list).
    bool isInFront (const Track&, int a, int b);
    juce::String formatTime (double seconds);

    // The family's track colours, taken in turn: shades along its red, pink
    // and orange only (the chain's red, hot pink, orange, then lighter and
    // deeper ones).
    // The family's colours (0.9.12, the owner's rule: exactly the gradient's,
    // nothing else): its pink, red and orange, as the keys' gradient defines
    // them (Finish::neonGradient runs pink, through red, to orange).
    namespace FamilyColours
    {
        inline const juce::Colour pink { 0xffde0639 };
        inline const juce::Colour red { 0xffd42e2a };
        inline const juce::Colour orange { 0xffcd6619 };
        // The nearest of the three (songs from before 0.9.12 had other shades).
        juce::Colour nearest (juce::Colour);
        // The colour menu's choices (0.17.3, the owner asked: every tint of the family's
        // colours, nothing else): `paletteHues` hues from the pink through the red to the
        // orange, each in `paletteShades` shades from deep to pale, column by column.
        static constexpr int paletteHues = 9, paletteShades = 8;
        juce::Colour paletteColour (int hue, int shade);
        // One of the three, or one of the palette's (what a song keeps as it is).
        bool isFamily (juce::Colour);
    }
    juce::Colour trackColour (int index);   // red, pink, orange, in turn

    // Rounds to the nearest multiple of `grid` beats (0: no rounding).
    double snap (double beats, double grid) noexcept;

    // Moves the starts of the part's notes (those listed, or all with none
    // listed) to the nearest `grid` line of the song (not of the part, so a
    // part that doesn't start on a bar still lines up with the bars).
    // Lengths stay, and every note stays (as in Cubase: none are merged).
    void quantize (Part&, double grid, const std::vector<int>& which = {});

    // A note's name, with middle C (60) as C3, as Cubase and Yamaha count: "C3", "F#4", "C-2".
    juce::String noteName (int pitch);
}
