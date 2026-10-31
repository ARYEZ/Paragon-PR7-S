#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <optional>

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

    struct Part
    {
        double start = 0.0;     // beats from the song's start
        double length = 4.0;    // beats
        juce::String name;
        std::vector<Note> notes;
        int id = 0;             // given by the studio, so the key editor keeps the same part however the list changes (not saved)

        // An audio track's part (a clip): the recording it plays, from
        // `audioOffset` seconds into it (the trimmed start), faded in and out.
        juce::String audioFile;      // its file's name, in the song's audio folder
        double audioOffset = 0.0;    // seconds
        double fadeIn = 0.0, fadeOut = 0.0;   // seconds
        bool muted = false;          // the Mute tool: kept and edited as usual, but not played

        bool isAudio() const noexcept { return audioFile.isNotEmpty(); }
        double end() const noexcept { return start + length; }
        bool operator== (const Part&) const = default;
    };

    // What plays a track: the built-in Paragon Synth, or a VST3 instrument.
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

        bool isStep() const noexcept { return target == Target::mute; }
        bool sameControl (const AutomationLane& other) const noexcept
        {
            return target == other.target && slot == other.slot && parameter == other.parameter;
        }
        // The value at a beat: the first point's before it, the last's after,
        // a straight line between (a jump at the next point, for a step lane).
        // None if there are no points.
        std::optional<float> valueAt (double beat) const noexcept;
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
    };

    struct Project
    {
        double bpm = 120.0;
        int numerator = 4, denominator = 4;   // the time signature
        bool loopOn = false;                  // CYCLE
        double loopStart = 0.0, loopEnd = 16.0;
        bool click = false;                   // the metronome
        std::vector<Track> tracks;
        float masterVolumeDb = 0.0f;          // the master bus: every track, then these effects, then this volume
        std::vector<Insert> masterInserts;
        int masterPreFaderSlots = 16;
        std::vector<AutomationLane> masterAutomation;   // volume and the master's effects
        bool masterAutomationOpen = false, masterRead = false, masterWrite = false;
        int nextTrackId = 1;

        static constexpr double minBpm = 20.0, maxBpm = 300.0;
        static constexpr int maxInserts = 16;  // insert slots on a channel (and on the master), as the manual has
        static constexpr int maxSends = 8;     // send slots on a channel (our choice: the manual gives no number)

        double beatsPerBar() const noexcept { return numerator * 4.0 / denominator; }
        double secondsPerBeat() const noexcept { return 60.0 / bpm; }

        Track* findTrack (int id);
        const Track* findTrack (int id) const;
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
    juce::String formatTime (double seconds);

    // The family's track colours, taken in turn: shades along its red, pink
    // and orange only (the chain's red, hot pink, orange, then lighter and
    // deeper ones).
    juce::Colour trackColour (int index);

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
