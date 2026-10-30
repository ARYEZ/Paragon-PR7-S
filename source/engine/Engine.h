#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "LevelMeterSource.h"
#include "Project.h"

#include <array>
#include <bitset>

namespace pr7s
{
    // A note on or off played while recording: when (beats from the song's
    // start) and the MIDI message.
    struct RecordedEvent
    {
        double beat = 0.0;
        juce::uint8 status = 0, data1 = 0, data2 = 0;
    };

    // The studio's sound: plays every track's parts through its instrument
    // in time, with the cycle and the click, mixes them (volume, pan, mute,
    // solo) and records what's played live. It's the audio device's
    // callback; tests drive render() directly.
    //
    // Each track: its instrument, then its inserts (effects) in order, then
    // its volume and pan, into the master bus; the master's inserts, then
    // its volume, to the output. The click joins after the master (so the
    // master's effects never colour it).
    //
    // Threads: the audio thread only ever tries the lock (if the screen is
    // swapping something in at that moment, that one block is silent rather
    // than the audio waiting). The message thread prepares everything
    // outside the lock and only swaps under it.
    class Engine final : public juce::AudioIODeviceCallback
    {
    public:
        Engine();
        ~Engine() override;

        // The sample rate and the largest block (the device's start calls it; tests too).
        void prepare (double sampleRate, int maxBlockSize);
        double getSampleRate() const noexcept { return sampleRate; }
        int getBlockSize() const noexcept { return maxBlockSize; }

        //======================================================================
        // Tracks (message thread)

        // Puts a track's instrument in place (prepared first), replacing any
        // it had. The engine owns it from now on.
        void setInstrument (int trackId, std::unique_ptr<juce::AudioPluginInstance>);
        void removeTrack (int trackId);
        // The track's instrument, for its editor window and saving its settings (or none).
        juce::AudioPluginInstance* getInstrument (int trackId) const;

        // Inserts (message thread). `busId`: a track's id, or masterBus.
        // An effect is put in at `slot` (the ones after it move down one),
        // prepared first; none (nullptr) keeps the slot for an effect that
        // couldn't be loaded (the sound goes past it).
        static constexpr int masterBus = 0;   // track ids start at 1
        void insertEffect (int busId, int slot, std::unique_ptr<juce::AudioPluginInstance>);
        // Every instrument and effect told it's rendering offline (an export:
        // a sampler can then wait for its samples instead of dropping notes),
        // or back to real time. Message thread, not while the interface plays.
        void setNonRealtime (bool);
        void removeEffect (int busId, int slot);   // deleted after it's out of the audio's way
        juce::AudioPluginInstance* getEffect (int busId, int slot) const;
        int getNumEffects (int busId) const;

        // Takes what it plays from the project: the tempo, signature, cycle
        // and click, each track's notes, volume, pan, mute and solo, which
        // inserts are bypassed, and the master's volume.
        // Call after every change.
        void update (const Project&);

        //======================================================================
        // Transport (any thread)
        void play();
        void stop();
        void record();   // plays, recording what's played into the armed track
        void stopRecording() noexcept { recording = false; }   // punch out: keeps playing
        bool isPlaying() const noexcept { return playing.load(); }
        bool isRecording() const noexcept { return recording.load(); }
        void setPosition (double beats);
        double getPosition() const noexcept { return position.load(); }

        //======================================================================
        // Playing live: MIDI from devices (the collector is their callback)
        // and the on-screen keyboard both go to the live track's instrument.
        juce::MidiMessageCollector& getMidiInput() noexcept { return midiInput; }
        juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
        void setLiveTrack (int trackId) noexcept { liveTrack = trackId; }
        int getLiveTrack() const noexcept { return liveTrack.load(); }

        // The notes played while recording since the last call (message thread).
        void takeRecorded (std::vector<RecordedEvent>& into);

        //======================================================================
        //======================================================================
        // Audio tracks (message thread). A clip's sound comes from `source`
        // (the studio's loaded recordings, at the engine's sample rate).
        using AudioSource = std::function<std::shared_ptr<const juce::AudioBuffer<float>> (const juce::String& file)>;
        void setAudioSource (AudioSource source) { audioSource = std::move (source); }
        void addAudioTrack (int trackId);
        // Where a recording into this track goes while recording (its input,
        // one or two channels), written from the audio thread (the writer's
        // own thread puts it on disk). nullptr: none (before deleting it).
        void setRecorder (int trackId, juce::AudioFormatWriter::ThreadedWriter*);
        // The song position (in samples) of the first sample recorded, or -1.
        juce::int64 getAudioRecordStart() const noexcept { return audioRecordStart.load(); }

        // One block of the studio's output: every track and the click,
        // stereo. `input`: the interface's inputs for the block (tests).
        void render (juce::AudioBuffer<float>& output, int numSamples, const juce::AudioBuffer<float>* input = nullptr);

        // The output's level, and each track's (the highest since last asked).
        // Each has two readers, so the transport and the track list (0) and
        // the mixer (1) don't take each other's peaks.
        LevelMeterSource& getOutputMeter (int channel, int reader = 0) noexcept
        {
            return outputMeters[(size_t) juce::jlimit (0, 1, reader)][(size_t) juce::jlimit (0, 1, channel)];
        }
        float takeTrackPeak (int trackId, int reader = 0);

        // What a track's (or the master's: masterBus) mix is at now, its
        // automation included, for the screen's faders to follow (any thread).
        struct MixNow { float volumeDb = 0.0f, pan = 0.0f; bool mute = false; };
        MixNow getMixNow (int busId) const;

        // AudioIODeviceCallback
        void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                               float* const* outputChannelData, int numOutputChannels, int numSamples,
                                               const juce::AudioIODeviceCallbackContext&) override;
        void audioDeviceAboutToStart (juce::AudioIODevice*) override;
        void audioDeviceStopped() override;

    private:
        struct Event
        {
            double beat = 0.0;
            juce::uint8 status = 0, data1 = 0, data2 = 0;
        };

        struct Slot
        {
            std::unique_ptr<juce::AudioPluginInstance> plugin;   // none: couldn't be loaded
            bool bypass = false;
        };

        // An audio clip as it plays: from `start` to `end` (beats), its sound
        // from `offset` samples in, faded over so many samples.
        struct Clip
        {
            double start = 0.0, end = 0.0;
            std::shared_ptr<const juce::AudioBuffer<float>> sound;
            juce::int64 offset = 0;
            double fadeIn = 0.0, fadeOut = 0.0;
        };

        // An automation lane being played back. Plugin knobs are set once a
        // block (only when they change); volume, pan and mute every 32 samples.
        struct LaneState
        {
            AutomationLane lane;
            float applied = -1.0f;   // the knob's value as last set
        };

        struct TrackState
        {
            int id = 0;
            bool audio = false;          // an audio track: clips and input, no instrument
            std::vector<Clip> clips;
            int input = 0;
            bool stereoInput = false, monitor = false;
            juce::AudioFormatWriter::ThreadedWriter* recorder = nullptr;
            std::unique_ptr<juce::AudioPluginInstance> instrument;
            int instrumentChannels = 2;  // what the instrument is given (its buffer may have more, for the inserts)
            std::vector<Slot> inserts;
            std::vector<Event> events;   // every note on and off, by beat
            float gainLeft = 1.0f, gainRight = 1.0f;
            juce::SmoothedValue<float> left, right;
            std::bitset<128> held;       // notes sounding, to stop them on a jump or stop
            juce::MidiBuffer midi;
            juce::AudioBuffer<float> buffer;
            std::array<LevelMeterSource, 2> meters;   // one per reader
            // The mix as set, and the lanes read (those with points and R on).
            float volumeDb = 0.0f, pan = 0.0f;
            bool mute = false, soloedOut = false;   // soloedOut: another track is soloed
            int preFader = Project::maxInserts;     // the inserts before the fader (the rest after)
            std::vector<LaneState> automation;
            bool mixAutomated = false;              // a volume, pan or mute lane among them
            std::atomic<float> nowVolumeDb { 0.0f }, nowPan { 0.0f };
            std::atomic<bool> nowMute { false };
        };

        struct Settings
        {
            double bpm = 120.0;
            int numerator = 4, denominator = 4;
            bool loopOn = false;
            double loopStart = 0.0, loopEnd = 16.0;
            bool click = false;
            float masterGain = 1.0f;
            float masterVolumeDb = 0.0f;
        };

        // Gives instruments the tempo and position, as a DAW does.
        struct Head final : juce::AudioPlayHead
        {
            juce::Optional<PositionInfo> getPosition() const override { return info; }
            PositionInfo info;
        };

        TrackState* findTrack (int id) const;
        void prepareTrack (TrackState&);
        void prepareEffect (juce::AudioPluginInstance&);
        static int channelsFor (const juce::AudioPluginInstance&);
        int channelsNeeded (const TrackState&) const;
        void runInserts (std::vector<Slot>&, juce::AudioBuffer<float>&, int numSamples, int fromSlot = 0, int toSlot = Project::maxInserts);
        void schedule (TrackState&, juce::int64 fromSample, juce::int64 toSample, int sampleOffset, double beatsPerSample);
        void stopHeldNotes (TrackState&, int sampleOffset);
        void renderBlock (juce::AudioBuffer<float>& output, int numSamples);
        const float* inputChannel (int channel, int offset) const noexcept;
        void playClips (TrackState&, juce::int64 clock, int offset, int length, double beatsPerSample);

        // The block's segments (up to the cycle's end, then from its start): their song position, place in the block and length.
        struct Segment { juce::int64 clock; int offset, length; };
        std::array<Segment, 16> segments;
        int numSegments = 0;

        // The interface's inputs this block.
        const float* const* blockInputs = nullptr;
        int blockNumInputs = 0, blockInputOffset = 0;
        std::vector<float> silence;   // for an input that isn't there
        AudioSource audioSource;
        std::atomic<juce::int64> audioRecordStart { -1 };

        juce::CriticalSection lock;
        std::vector<std::unique_ptr<TrackState>> tracks;
        Settings settings;
        double sampleRate = 48000.0;
        int maxBlockSize = 512;

        std::atomic<bool> playing { false }, recording { false };
        std::atomic<double> position { 0.0 }, jumpTo { -1.0 };
        std::atomic<bool> rebase { false };   // the position was moved while stopped: the clock starts from it
        bool wasPlaying = false;
        // The song position in whole samples while playing (audio thread), so
        // notes land on exactly their sample however long the song plays.
        juce::int64 sampleClock = 0;
        double clockBeatsPerSample = 0.0;
        std::atomic<int> liveTrack { 0 };

        juce::MidiMessageCollector midiInput;
        juce::MidiKeyboardState keyboardState;
        juce::MidiBuffer live;

        // Recorded notes, from the audio thread to the message thread.
        juce::AbstractFifo recordedFifo { 8192 };
        std::array<RecordedEvent, 8192> recordedEvents;

        // The click: up to 64 ticks a block, each a short burst of a tone.
        struct Tick { int sample; bool accent; };
        std::array<Tick, 64> ticks;
        int numTicks = 0;
        int clickRemaining = 0;
        double clickPhase = 0.0, clickIncrement = 0.0;
        float clickLevel = 0.0f;

        // The master bus.
        std::vector<Slot> masterInserts;
        juce::AudioBuffer<float> masterBuffer;
        juce::SmoothedValue<float> masterGain;
        std::vector<LaneState> masterAutomation;
        int masterPreFader = Project::maxInserts;
        bool masterAutomated = false;   // a volume lane among them
        std::atomic<float> nowMasterVolumeDb { 0.0f };

        void applyKnobs (std::vector<LaneState>&, juce::AudioPluginInstance* instrument, std::vector<Slot>& inserts, double beat);
        static MixNow mixAt (const std::vector<LaneState>&, MixNow set, double beat);
        juce::MidiBuffer effectMidi;   // effects get no MIDI

        Head head;
        juce::AudioBuffer<float> deviceBuffer;
        std::array<std::array<LevelMeterSource, 2>, 2> outputMeters;   // [reader][channel]
    };
}
