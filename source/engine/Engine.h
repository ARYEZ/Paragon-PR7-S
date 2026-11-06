#pragma once

#include <optional>

#include <juce_audio_utils/juce_audio_utils.h>

#include "AudioWorkers.h"
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
        int pass = 0;   // the cycle's pass it was played in (0.25.0: controllers keep the last pass's)
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
        void setNonRealtime (bool, const std::function<bool (int trackId)>& sounding = {});
        void removeEffect (int busId, int slot);   // deleted after it's out of the audio's way
        juce::AudioPluginInstance* getEffect (int busId, int slot) const;
        int getNumEffects (int busId) const;

        // A control being written (held by the hand, while W is on): its lane
        // isn't played back until it's let go, so what's heard is the hand.
        // Message thread; takes effect on the next update().
        void setHeld (int busId, const AutomationLane& control, bool held);

        // Told just before a plugin (an instrument or an effect) is deleted,
        // however it goes (replaced, removed, its track removed, the engine
        // closing): whatever listens to it lets go first. Message thread.
        std::function<void (juce::AudioProcessor*)> onPluginGoing;

        // Takes what it plays from the project: the tempo, signature, cycle
        // and click, each track's notes, volume, pan, mute and solo, which
        // inserts are bypassed, and the master's volume.
        // Call after every change.
        void update (const Project&);
        int getEventBuilds() const noexcept { return eventBuilds; }   // (0.34.2, tests) tracks' event lists built so far
        int getClipBuilds() const noexcept { return clipBuilds; }     // (0.57.2, tests) audio tracks' clip lists built

        //======================================================================
        // Transport (any thread)
        void play();
        void stop();
        // Every note the song left sounding ended at the next block (a render's start:
        // no block may have been played since the last render or the playback).
        void releaseNotes() noexcept { releaseAll = true; }
        void record();   // plays, recording what's played into the armed track
        void stopRecording() noexcept { recording = false; }   // punch out: keeps playing
        bool isPlaying() const noexcept { return playing.load(); }
        bool isRecording() const noexcept { return recording.load(); }
        void setPosition (double beats);
        double getPosition() const noexcept { return position.load(); }
        // How many times the cycle has come round (any thread): a pass of
        // writing automation ends when it does.
        int getCycleCount() const noexcept { return cycleCount.load(); }

        //======================================================================
        // Playing live: MIDI from devices (the collector is their callback)
        // and the on-screen keyboard both go to the live track's instrument.
        juce::MidiMessageCollector& getMidiInput() noexcept { return midiInput; }
        juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
        void setLiveTrack (int trackId) noexcept { liveTrack = trackId; }
        int getLiveTrack() const noexcept { return liveTrack.load(); }

        // MIDI devices' callback (0.9.0): what each device plays goes to the
        // collector unless the live track listens to one input only (its
        // Routing: a device's name; empty: all).
        juce::MidiInputCallback& getLiveInput() noexcept { return liveInput; }
        // The chord pads (0.14.0): while set, their trigger keys from the devices play them.
        void setPadPlayer (PadPlayer* player) noexcept { padPlayer = player; }
        void setLiveInputFilter (const juce::String& deviceName);
        bool acceptsLiveFrom (const juce::String& deviceName) const;

        // The notes played while recording since the last call (message thread).
        void takeRecorded (std::vector<RecordedEvent>& into);

        //======================================================================
        //======================================================================
        // Audio tracks (message thread). A clip's sound comes from `source`
        // (the studio's loaded recordings, at the engine's sample rate).
        using AudioSource = std::function<std::shared_ptr<const juce::AudioBuffer<float>> (const juce::String& file)>;
        void setAudioSource (AudioSource source) { audioSource = std::move (source); }
        // (0.32.0) A warped clip's sound from its start (its warp markers applied): asked for
        // instead of the recording when a clip has markers.
        using WarpSource = std::function<std::shared_ptr<const juce::AudioBuffer<float>> (const Part&, const TempoMap&)>;
        void setWarpSource (WarpSource source) { warpSource = std::move (source); }
        void addAudioTrack (int trackId);
        // An FX channel (what sends give it, through its inserts) or a group
        // channel (the tracks routed to it): no instrument, no parts.
        void addChannel (int trackId, bool group);
        // A send's level now, its automation included (for the screen).
        float getSendNowDb (int trackId, int slot) const;
        // Where a recording into this track goes while recording (its input,
        // one or two channels), written from the audio thread (the writer's
        // own thread puts it on disk). nullptr: none (before deleting it).
        void setRecorder (int trackId, juce::AudioFormatWriter::ThreadedWriter*);
        // The song position (in samples) of the first sample recorded, or -1.
        juce::int64 getAudioRecordStart() const noexcept { return audioRecordStart.load(); }

        // The MediaBay's preview (0.21.0): a sound (stereo, at the engine's rate) played over the
        // output after the click, whether the song plays or not. The message thread hands it over
        // under the lock (and keeps the old one alive: the audio thread never frees anything); the
        // audio thread only reads it. Loops when `loop` is on, else stops at its end.
        void setPreview (std::shared_ptr<const juce::AudioBuffer<float>>, bool loop);
        void stopPreview();
        void setPreviewLoop (bool on) noexcept { previewLoop = on; }
        void setPreviewGain (float gain) noexcept { previewGain = gain; }

        // (0.46.0) The EQ window's live sound: the channel's after its EQ (0: the master; -1: none),
        // mono, into a lock-free ring the screen empties (`readSpectrum`: what came since, at most
        // `into`'s size, the newest kept).
        // (0.51.0, the manual's Channel Selection in Export Audio Mixdown) A channel export: everything plays
        // as usual (groups sum, sends feed FX channels), but only this channel reaches the output (a track
        // routed to a group: straight to the output, its group left out). 0: the whole mix.
        void setExportBus (int busId) noexcept { exportBus = busId; }
        int getExportBus() const noexcept { return exportBus.load(); }
        void setSpectrumBus (int busId) noexcept { spectrumBus = busId; }
        int getSpectrumBus() const noexcept { return spectrumBus.load(); }
        int readSpectrum (std::vector<float>& into);
        // (0.46.0) Auto Listen: this channel's sound through `design` after its EQ (a band alone),
        // or nothing (-1). Under the lock (message thread).
        void setListen (int busId, const EqDesign& design);
        void clearListen();
        int getListenBus() const noexcept { return listenBus.load(); }
        // In time with the song (0.23.0, the manual's Wait for Project Play): the preview is heard
        // only while the song plays; with `lockBeats` (the sound's length in beats at the song's
        // tempo) its place follows the song's beat from `anchorBeat` (the left locator), so a loop
        // lands on the bars, the cycle coming round too.
        void setPreviewSync (bool waitForPlay, double anchorBeat, double lockBeats) noexcept
        {
            previewAnchor = anchorBeat;
            previewLockBeats = lockBeats;
            previewWait = waitForPlay;
        }
        bool isPreviewWaiting() const noexcept { return previewWait.load() && ! playing.load(); }
        bool isPreviewPlaying() const noexcept { return previewPlaying.load(); }
        double getPreviewProgress() const noexcept;   // 0 to 1 through the sound

        // One block of the studio's output: every track and the click,
        // stereo. `input`: the interface's inputs for the block (tests).
        void render (juce::AudioBuffer<float>& output, int numSamples, const juce::AudioBuffer<float>* input = nullptr);

        // The output's level, and each track's (the highest since last asked).
        // Each has two readers, so the transport and the track list (0) and
        // the mixer (1) don't take each other's peaks; a track's, a third (2:
        // the Inspector's fader).
        LevelMeterSource& getOutputMeter (int channel, int reader = 0) noexcept
        {
            return outputMeters[(size_t) juce::jlimit (0, 2, reader)][(size_t) juce::jlimit (0, 1, channel)];
        }
        float takeTrackPeak (int trackId, int reader = 0);

        // Audio performance (the manual's Audio Performance Monitor, as its
        // transport meter shows it): how long the interface's blocks take to
        // process, against how long they last (1 = all the time there is).
        // The average (smoothed over a tenth of a second or so), the peak since
        // the last look, and the overloads (blocks that took longer than they
        // last: a dropout) since the last reset.
        struct Performance { float average = 0.0f, peak = 0.0f; int overloads = 0; };
        Performance takePerformance();
        void resetOverloads() noexcept { overloads = 0; }
        int getOverloads() const noexcept { return overloads.load(); }   // (without taking the peak: a tooltip looks too)
        // Every fader, pan and send straight at its setting, with no glide from
        // where it was (an export or a render starts here: its first moments
        // at the levels set, not still moving from the last playback's).
        void settleMix();
        void setCatchingNotes (bool on) { catchingNotes = on; if (! on) takeCaughtNotes(); }
        std::vector<int> takeCaughtNotes()
        {
            const juce::SpinLock::ScopedLockType sl (caughtLock);
            std::vector<int> notes (caught.begin(), caught.begin() + numCaught);
            numCaught = 0;
            return notes;
        }
        // Multi-core audio (0.34.5): helper threads beside the audio thread (0: everything on it).
        // Message thread; waits for the block being played.
        void setHelperThreads (int count);
        int getHelperThreads() const noexcept { return workers.getHelpers(); }

        // One block measured (the audio callback calls it; tests too).
        void notePerformance (double secondsTaken, int numSamples) noexcept;

        // (0.57.3) The spike report: a block of the interface's that took 90% of its time or more,
        // played silence because the engine was busy taking a change in (the lock), or was called
        // late by the driver. The audio thread fills one report (nothing allocated, nothing waited
        // for) and the screen's thread takes it (Studio writes it into the session log); spikes
        // while one waits to be taken are counted. Each track's own time is measured every block
        // (its instrument and inserts), with its usual time beside it.
        struct SpikeReport
        {
            static constexpr int mostTracks = 6;
            float load = 0.0f;                 // the block's time against its length (1 = all of it)
            double blockMs = 0.0, budgetMs = 0.0;
            double sinceLastMs = 0.0;          // since the driver's call before (about budgetMs when on time)
            bool missedLock = false;           // silence: the engine was taking a change in
            bool lateCall = false;             // the driver called over 1.5 blocks after the last
            double tracksMs = 0.0;             // the tracks' own work, all added up
            double masterMs = 0.0;             // the master's EQ and inserts
            int helpers = 0;                   // multi-core helper threads
            int numTracks = 0;
            // (0.57.4) Each slow track's plug-ins: its instrument, and its slowest inserts (by slot).
            struct Plugin { int slot = -1; float ms = 0.0f, usualMs = 0.0f; };   // slot -1: the instrument
            static constexpr int mostPlugins = 4;
            struct Slow { int trackId = 0; float ms = 0.0f, usualMs = 0.0f; std::array<Plugin, mostPlugins> plugins {}; int numPlugins = 0; };
            std::array<Slow, mostTracks> slowest {};
            int numSlowest = 0;
            int missedSince = 0;               // spikes while this one waited to be taken
        };
        std::optional<SpikeReport> takeSpikeReport();   // (message thread)
        // How a block is judged (tests set them): its share of the time; lateness.
        void noteSpike (double secondsTaken, int numSamples, double secondsSinceLast) noexcept;

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
            double ownSeconds = 0.0;   // (0.57.4, audio thread) its time this call, and usually
            float usualSeconds = 0.0f;
        };

        // An audio clip as it plays: from `start` to `end` (beats), its sound
        // from `offset` samples in, faded over so many samples.
        struct Clip
        {
            double start = 0.0, end = 0.0;
            std::shared_ptr<const juce::AudioBuffer<float>> sound;
            juce::int64 offset = 0;
            double fadeIn = 0.0, fadeOut = 0.0;
            float gain = 1.0f;   // (0.47.0) the event's own volume
            // (0.52.0) Auto fades: at the heard stretch's own ends (samples; 0: none), Equal Power or straight.
            double autoIn = 0.0, autoOut = 0.0;
            bool equalPower = true;
            // (0.29.0) The stretch of it heard (beats): another clip in front hides the rest.
            double from = 0.0, to = 1.0e12;
            // (0.34.1) Where those are in the song's samples, worked out in update (by the same tempo
            // map and rate as playback: they were worked out again for every clip every block).
            juce::int64 startSample = 0, endSample = 0, fromSample = 0, toSample = 0;
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
            double ownSeconds = 0.0;     // (0.57.3) its own work this call (any core), and usually
            float usualSeconds = 0.0f;
            double instrumentSeconds = 0.0;   // (0.57.4) its instrument's part of it, and usually
            float instrumentUsual = 0.0f;
            bool audio = false;          // an audio track: clips and input, no instrument
            bool frozen = false;         // its freeze file plays as a clip (no instrument, no inserts)
            std::vector<Clip> clips;
            int input = 0;
            int midiChannel = 1;         // the channel its notes (and what's played live into it) go to the instrument on
            bool stereoInput = false, monitor = false;
            juce::AudioFormatWriter::ThreadedWriter* recorder = nullptr;
            std::unique_ptr<juce::AudioPluginInstance> instrument;
            int instrumentChannels = 2;  // what the instrument is given (its buffer may have more, for the inserts)
            std::vector<Slot> inserts;
            std::vector<Event> events;   // every note on and off, by beat
            float gainLeft = 1.0f, gainRight = 1.0f;
            juce::SmoothedValue<float> left, right;
            std::bitset<128> held;       // notes sounding, to stop them on a jump or stop
            int heldChannel = 1;         // the channel they were started on (the track's may change meanwhile)
            // Its drum map (0.26.0): each pitch's O-note (-1: muted), its channel (-1: the track's), and each key's sound.
            std::array<juce::int16, 128> drumOut {};
            std::array<juce::int8, 128> drumChannel {};
            std::array<juce::uint8, 128> drumIn {};
            TrackState()
            {
                for (int n = 0; n < 128; ++n)
                    drumOut[(size_t) n] = (juce::int16) n, drumChannel[(size_t) n] = -1, drumIn[(size_t) n] = (juce::uint8) n;
            }
            bool chase = false;          // (0.25.0) play started, jumped or came round: each controller's value there first
            bool bent = false, sustained = false;   // a bend or the sustain pedal sent: reset on stop
            bool checkHeld = false;      // the events changed (an edit while playing): the notes they no longer end, ended
            juce::MidiBuffer midi;
            juce::AudioBuffer<float> buffer;
            std::array<LevelMeterSource, 3> meters;   // one per reader (2: the Inspector's fader)
            // The mix as set, and the lanes read (those with points and R on).
            float volumeDb = 0.0f, pan = 0.0f;
            bool mute = false, soloedOut = false;   // soloedOut: another track is soloed
            int preFader = Project::maxInserts;     // the inserts before the fader (the rest after)
            ChannelEq eqSettings;                   // (0.43.0) its EQ as set (a new rate designs it again)
            EqProcessor eq;
            // What it is, where its sound goes, and its sends.
            enum class Role { source, group, fx };  // processed in this order: tracks, groups, FX
            Role role = Role::source;
            int output = 0;                         // a group channel's id, or 0: the master
            struct SendState
            {
                int target = 0;                     // an FX channel's id (0: none)
                float levelDb = 0.0f, lastGain = 0.0f;
                bool on = false, preFader = false;
            };
            std::array<SendState, Project::maxSends> sends;
            std::array<std::atomic<float>, Project::maxSends> nowSendDb {};
            juce::AudioBuffer<float> bus;           // groups and FX: what comes in, this block
            std::vector<LaneState> automation;
            bool mixAutomated = false;              // a volume, pan or mute lane among them
            std::atomic<float> nowVolumeDb { 0.0f }, nowPan { 0.0f };
            std::atomic<bool> nowMute { false };
            // (0.34.1, message thread only) What its events were last built from: the same again (a
            // fader moved, a send, a knob: anything but its notes), the events are kept as they are,
            // not built, sorted and swapped in again.
            struct BuiltFrom
            {
                bool valid = false;
                std::vector<Part> parts;
                int midiChannel = 1, soloLane = -1;
                std::optional<DrumMap> map;
                // (0.36.0) Following the chord track: its mode, reference, and the chord track as it was.
                FollowMode follow = FollowMode::off;
                FollowReference reference;
                ChordTrack chords;
                Key fallback;
                Voice voice = Voice::soprano;              // (0.38.0)
                std::optional<VoicingSettings> voicing;
                juce::String pattern;                      // (0.44.0) the pattern it performs
                // (0.57.2) An audio track's clips: kept while its parts, solo lane, auto fades, the
                // tempo, the rate and the very sounds its clips play are the same.
                std::vector<const void*> sounds;
                AutoFades fades;
                std::vector<TempoEvent> tempo;
                double rate = 0.0;
            };
            BuiltFrom builtFrom;
            // (0.34.5) Its own work in a block may run on any core: what its effects are given, the
            // sound before its fader for pre-fader sends, whether it played and how loud.
            juce::MidiBuffer effectMidi;
            juce::AudioBuffer<float> preFaderCopy;
            bool playedThisBlock = false;
            float blockPeak = 0.0f;
        };

        struct Settings
        {
            TempoMap tempo;            // the fixed tempo, or the tempo track's (0.10.0)
            int tempoVersion = 0;      // changes when the tempo does (the clock re-bases)
            SignatureMap signatures;   // (0.45.0) the bars: the click's accents, the play head's signature
            bool loopOn = false;
            double loopStart = 0.0, loopEnd = 16.0;
            bool click = false;
            float masterGain = 1.0f;
            float masterVolumeDb = 0.0f;
            bool virginTerritory = false;   // (0.28.0) gaps and the stretch before a lane's first point leave the control alone
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
        void runInserts (std::vector<Slot>&, juce::AudioBuffer<float>&, int numSamples, int fromSlot, int toSlot, juce::MidiBuffer& effectMidi);
        // The block's segments (up to the cycle's end, then from its start): their song position, place in the block and length.
        struct Segment { juce::int64 clock; int offset, length; };
        // Beats to the song's samples and back, by the tempo map (0.10.0: it was one beats-per-sample).
        struct Timing
        {
            const TempoMap* map;
            double rate;
            juce::int64 sampleAt (double beat) const noexcept { return (juce::int64) std::llround (map->secondsAt (beat) * rate); }
            double beatAt (double sample) const noexcept { return map->beatAt (sample / rate); }
        };
        void schedule (TrackState&, juce::int64 fromSample, juce::int64 toSample, int sampleOffset, const Timing&);
        void chaseControllers (TrackState&, double beat, int sampleOffset);
        void resetControllers (TrackState&, int sampleOffset);
        void stopHeldNotes (TrackState&, int sampleOffset);
        void endNotesNoLongerEnded (TrackState&, double beatNow, int sampleOffset);
        void renderBlock (juce::AudioBuffer<float>& output, int numSamples);
        const float* inputChannel (int channel, int offset) const noexcept;
        void playClips (TrackState&, juce::int64 clock, int offset, int length, const Timing&);
        static void placeClip (Clip&, const Timing&);   // (0.34.1) its samples, worked out once
        void runSends (TrackState&, bool preFader, int numSamples, double beat, const juce::AudioBuffer<float>& from);

        // (0.34.5) A block's work, split in two. A track's own work (its instrument or clips, its
        // inserts, fader and pan) touches nothing of any other track's, so tracks of one kind run
        // side by side on the cores. Then, on the audio thread in the tracks' order as before, each
        // track's sends and its sound into its group or the master: the same sums in the same
        // order, so the output is the same to the last bit.
        struct BlockContext
        {
            int numSamples = 0;
            const Segment* spans = nullptr;
            int numSpans = 0;
            const Timing* timing = nullptr;
            bool playing = false;
            double blockBeat = 0.0;
            int liveId = 0;
            double spanBeat (const Segment& span, int sample) const
            {
                return playing ? timing->beatAt ((double) (span.clock + (sample - span.offset))) : blockBeat;
            }
        };
        void processTrack (TrackState&, const BlockContext&);
        void timedTrack (TrackState&, const BlockContext&);   // (0.57.3) processTrack, timed
        void mixTrackOut (TrackState&, const BlockContext&, juce::AudioBuffer<float>& master);
        AudioWorkers workers;
        std::vector<TrackState*> wave;   // (room kept by the message thread: the audio thread never grows it)

        std::array<Segment, 16> segments;
        int numSegments = 0;

        // The interface's inputs this block.
        const float* const* blockInputs = nullptr;
        int blockNumInputs = 0, blockInputOffset = 0;
        std::vector<float> silence;   // for an input that isn't there
        AudioSource audioSource;
        WarpSource warpSource;
        std::atomic<juce::int64> audioRecordStart { -1 };

        juce::CriticalSection lock;
        std::vector<std::unique_ptr<TrackState>> tracks;
        int eventBuilds = 0;   // (message thread)
        int clipBuilds = 0;    // (message thread)
        Settings settings;
        double sampleRate = 48000.0;
        int maxBlockSize = 512;

        std::atomic<bool> playing { false }, recording { false };
        std::atomic<double> position { 0.0 }, jumpTo { -1.0 };
        std::atomic<bool> rebase { false };
        std::atomic<bool> releaseAll { false };
        std::atomic<int> cycleCount { 0 };   // the position was moved while stopped: the clock starts from it
        bool wasPlaying = false;
        // The song position in whole samples while playing (audio thread), so
        // notes land on exactly their sample however long the song plays.
        juce::int64 sampleClock = 0;
        int clockTempoVersion = -1;
        std::atomic<int> liveTrack { 0 };
        std::atomic<float> averageLoad { 0.0f }, peakLoad { 0.0f };
        std::atomic<int> overloads { 0 };
        // (0.57.3) The spike report (audio thread writes; message thread takes when `spikeReady`).
        SpikeReport spike;
        std::atomic<bool> spikeReady { false };
        std::atomic<int> spikesMissed { 0 };
        bool lockMissedThisCall = false;    // (audio thread)
        bool newCall = false;               // (audio thread) each track's own time starts again
        double masterSecondsThisCall = 0.0; // (audio thread)
        std::atomic<juce::int64> lastCallTicks { 0 };

        juce::MidiMessageCollector midiInput;
        struct LiveInput final : juce::MidiInputCallback
        {
            explicit LiveInput (Engine& e) : engine (e) {}
            void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override
            {
                if (source == nullptr || engine.acceptsLiveFrom (source->getName()))
                {
                    // (0.35.0) Keys played, noted while VariAudio's MIDI Input asks (they still play on).
                    if (engine.catchingNotes.load() && message.isNoteOn())
                        engine.catchNote (message.getNoteNumber());
                    // The chord pads' trigger keys play the pads, not themselves (0.14.0).
                    if (auto* pads = engine.padPlayer.load(); pads != nullptr && pads->handle (message))
                        return;
                    engine.midiInput.handleIncomingMidiMessage (source, message);
                }
            }
            Engine& engine;
        };
        LiveInput liveInput { *this };
        // (0.35.0) Keys played, for VariAudio's MIDI Input: from the devices' threads (a spin lock between
        // them: never the audio thread), taken on the message thread.
        std::atomic<bool> catchingNotes { false };
        juce::SpinLock caughtLock;
        std::array<int, 64> caught {};
        int numCaught = 0;
        void catchNote (int note)
        {
            const juce::SpinLock::ScopedLockType sl (caughtLock);
            if (numCaught < (int) caught.size())
                caught[(size_t) numCaught++] = note;
        }
        std::atomic<PadPlayer*> padPlayer { nullptr };
        mutable juce::SpinLock liveFilterLock;
        juce::String liveFilter;
        juce::MidiKeyboardState keyboardState;
        juce::MidiBuffer live;
        // Live Input (0.36.0): the live track's mode, the chord track's chords and scales as
        // pitch-class bits by beat (built by `update`), and each key's note as it was sent
        // (so its note off goes to the same note). Audio thread: mapLive.
        struct LiveChord { double beat; juce::uint16 chord, scale; };
        std::vector<LiveChord> liveChords;
        LiveFollow liveFollowMode = LiveFollow::off;
        int liveFollowTrack = 0;
        std::array<std::array<juce::int16, 128>, 16> liveSent {};   // the note sent + 1 (0: none)
        juce::MidiBuffer liveMapped;
        void mapLive (double beat) noexcept;

        // Recorded notes, from the audio thread to the message thread.
        juce::AbstractFifo recordedFifo { 8192 };
        std::array<RecordedEvent, 8192> recordedEvents;

        // The preview (0.21.0).
        std::shared_ptr<const juce::AudioBuffer<float>> preview;
        std::atomic<int> previewPosition { 0 }, previewLength { 0 };
        std::atomic<bool> previewPlaying { false }, previewLoop { false };
        std::atomic<float> previewGain { 0.7f };
        std::atomic<bool> previewWait { false };
        std::atomic<double> previewAnchor { 0.0 }, previewLockBeats { 0.0 };

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
        ChannelEq masterEqSettings;   // (0.43.0)
        EqProcessor masterEq;
        // (0.46.0) The spectrum's tap and Auto Listen.
        std::atomic<int> spectrumBus { -1 }, listenBus { -1 };
        std::atomic<int> exportBus { 0 };   // (0.51.0) a channel export: only this channel reaches the output (0: all)
        static constexpr int spectrumRingSize = 32768;
        juce::AbstractFifo spectrumFifo { spectrumRingSize };
        std::vector<float> spectrumRing = std::vector<float> ((size_t) spectrumRingSize, 0.0f);
        EqProcessor listenEq;
        void afterEq (int busId, float* const* channels, int numChannels, int numSamples) noexcept;   // (audio thread)
        std::vector<std::pair<int, AutomationLane>> held;   // message thread: the controls being written
        bool masterAutomated = false;   // a volume lane among them
        std::atomic<float> nowMasterVolumeDb { 0.0f };

        void applyKnobs (std::vector<LaneState>&, juce::AudioPluginInstance* instrument, std::vector<Slot>& inserts, double beat);
        static MixNow mixAt (const std::vector<LaneState>&, MixNow set, double beat, bool virginTerritory = false);
        juce::MidiBuffer effectMidi;   // effects get no MIDI

        Head head;
        juce::AudioBuffer<float> deviceBuffer;
        std::array<std::array<LevelMeterSource, 2>, 3> outputMeters;   // [reader][channel] (2: the Inspector's master, 0.9.12)
    };
}
