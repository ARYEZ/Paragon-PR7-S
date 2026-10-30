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
    // plugins). The sound goes through a channel's inserts in order.
    struct Insert
    {
        juce::PluginDescription plugin;
        juce::MemoryBlock state;     // its settings, as it last saved them
        bool bypass = false;         // off: the sound goes past it
    };

    struct Track
    {
        // An instrument track plays MIDI parts through its instrument; an
        // audio track plays clips (recordings) and records from the interface.
        enum class Kind { instrument, audio };   // saved by name

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
        bool isAudio() const noexcept { return kind == Kind::audio; }
        std::vector<Part> parts;
        std::vector<Insert> inserts;     // its effects, before its volume and pan
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
        int nextTrackId = 1;

        static constexpr double minBpm = 20.0, maxBpm = 300.0;
        static constexpr int maxInserts = 8;   // effects on a channel (and on the master)

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

    // The family's track colours: the chain's red, pink and orange, then
    // other neon colours, taken in turn.
    juce::Colour trackColour (int index);

    // Rounds to the nearest multiple of `grid` beats (0: no rounding).
    double snap (double beats, double grid) noexcept;

    // Moves the starts of the part's notes (those listed, or all with none
    // listed) to the nearest `grid` line of the song (not of the part, so a
    // part that doesn't start on a bar still lines up with the bars).
    // Lengths stay. Two notes of the same pitch that land on the same start
    // become one (the longer), since they'd only cut each other off.
    // Returns false if any notes were merged (their order and number changed).
    bool quantize (Part&, double grid, const std::vector<int>& which = {});

    // A note's name, with middle C (60) as C3, as Cubase and Yamaha count: "C3", "F#4", "C-2".
    juce::String noteName (int pitch);
}
