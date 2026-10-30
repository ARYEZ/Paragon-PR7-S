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

        // Takes what it plays from the project: the tempo, signature, cycle
        // and click, and each track's notes, volume, pan, mute and solo.
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
        // One block of the studio's output: every track and the click, stereo.
        void render (juce::AudioBuffer<float>& output, int numSamples);

        // The output's level, and each track's (the highest since last asked).
        LevelMeterSource& getOutputMeter (int channel) noexcept { return outputMeters[(size_t) juce::jlimit (0, 1, channel)]; }
        float takeTrackPeak (int trackId);

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

        struct TrackState
        {
            int id = 0;
            std::unique_ptr<juce::AudioPluginInstance> instrument;
            std::vector<Event> events;   // every note on and off, by beat
            float gainLeft = 1.0f, gainRight = 1.0f;
            juce::SmoothedValue<float> left, right;
            std::bitset<128> held;       // notes sounding, to stop them on a jump or stop
            juce::MidiBuffer midi;
            juce::AudioBuffer<float> buffer;
            LevelMeterSource meter;
        };

        struct Settings
        {
            double bpm = 120.0;
            int numerator = 4, denominator = 4;
            bool loopOn = false;
            double loopStart = 0.0, loopEnd = 16.0;
            bool click = false;
        };

        // Gives instruments the tempo and position, as a DAW does.
        struct Head final : juce::AudioPlayHead
        {
            juce::Optional<PositionInfo> getPosition() const override { return info; }
            PositionInfo info;
        };

        TrackState* findTrack (int id) const;
        void prepareTrack (TrackState&);
        void schedule (TrackState&, juce::int64 fromSample, juce::int64 toSample, int sampleOffset, double beatsPerSample);
        void stopHeldNotes (TrackState&, int sampleOffset);
        void renderBlock (juce::AudioBuffer<float>& output, int numSamples);

        juce::CriticalSection lock;
        std::vector<std::unique_ptr<TrackState>> tracks;
        Settings settings;
        double sampleRate = 48000.0;
        int maxBlockSize = 512;

        std::atomic<bool> playing { false }, recording { false };
        std::atomic<double> position { 0.0 }, jumpTo { -1.0 };
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

        Head head;
        juce::AudioBuffer<float> deviceBuffer;
        std::array<LevelMeterSource, 2> outputMeters;
    };
}
