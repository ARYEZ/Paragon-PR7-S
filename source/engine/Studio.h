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
        void removeTrack (int trackId);
        bool changeInstrument (int trackId, const Instrument&, juce::String* error = nullptr);
        void renameTrack (int trackId, const juce::String&);
        void setMute (int trackId, bool);
        void setSolo (int trackId, bool);
        void setVolume (int trackId, float db);
        void setPan (int trackId, float pan);
        // Record-arms one track (or none, with 0): recording goes there. As in
        // Cubase with "record enable selected track", selecting arms too.
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
        void stop();      // a second stop goes back to where playing started, as in Cubase
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
        void recordUndoState();
        void restore (const Project&);
        void updateLiveTrack();
        void finishRecording();
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

        // Recording: where it started, into which track, and what's come in so far.
        bool wasRecording = false;
        double recordStart = 0.0, playStart = 0.0;
        int recordTrack = 0;
        std::vector<RecordedEvent> recorded;

        std::vector<Project> undoStates;
        int undoIndex = -1;
    };
}
