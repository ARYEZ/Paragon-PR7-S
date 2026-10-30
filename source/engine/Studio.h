#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "Engine.h"
#include "PluginHost.h"
#include "Project.h"

#include <optional>

namespace pr7s
{
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
        void removeTrack (int trackId);
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
        void selectTrack (int trackId);
        int getSelectedTrack() const noexcept { return selectedTrack; }

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
        // track's id, or Engine::masterBus). Adding, replacing and removing
        // effects isn't undoable (as with instruments); bypass isn't either;
        // the master's volume is.
        const std::vector<Insert>* getInserts (int busId) const;   // none if there's no such track
        bool addInsert (int busId, const juce::PluginDescription&, juce::String* error = nullptr);   // at the end
        bool addInsert (int busId, const Insert&, std::unique_ptr<juce::AudioPluginInstance>);     // one made elsewhere (tests)
        bool replaceInsert (int busId, int slot, const juce::PluginDescription&, juce::String* error = nullptr);
        void removeInsert (int busId, int slot);
        void setInsertBypass (int busId, int slot, bool);
        void setMasterVolume (float db, bool undoable = true);
        // Told just before an effect is deleted (nullptr: every effect, for a
        // new or opened song), so its window closes while it still exists.
        std::function<void (juce::AudioProcessor*)> onEffectGoing;

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
        juce::File getFile() const noexcept { return file; }
        bool isModified() const noexcept { return modified; }

    private:
        void changed (bool undoable = true);
        void numberParts();   // an id for every part that hasn't one
        std::vector<Insert>* insertsOf (int busId);
        void effectGoing (int busId, int slot);
        void removeAllEffects();
        void recordUndoState();
        void restore (const Project&);
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

        std::vector<Project> undoStates;
        int undoIndex = -1;
    };
}
