#include "Studio.h"

namespace pr7s
{
    namespace
    {
        const juce::String audioSetupKey { "audioSetup" };
        constexpr int maxUndoSteps = 100;

        juce::PropertiesFile::Options settingsOptions()
        {
            juce::PropertiesFile::Options options;
            options.applicationName = "Paragon PR7-S";
            options.filenameSuffix = ".settings";
            options.folderName = "Paragon PR7-S";
            options.osxLibrarySubFolder = "Application Support";
            return options;
        }
    }

    Studio::Studio (bool useDevices) : withDevices (useDevices)
    {
        if (withDevices)
        {
            settings = std::make_unique<juce::PropertiesFile> (settingsOptions());
            host = std::make_unique<PluginHost> (settings.get());

            // Output only for now (audio recording is a later stage); every
            // MIDI input on, so a keyboard plugged in just plays.
            const auto saved = settings->getXmlValue (audioSetupKey);
            devices.initialise (0, 2, saved.get(), true);
            for (const auto& input : juce::MidiInput::getAvailableDevices())
                devices.setMidiInputDeviceEnabled (input.identifier, true);
            devices.addMidiInputDeviceCallback ({}, &engine.getMidiInput());
            devices.addAudioCallback (&engine);
            devices.addChangeListener (this);
        }
        else
        {
            host = std::make_unique<PluginHost> (nullptr);
            engine.prepare (48000.0, 512);
        }

        newProject();
        startTimerHz (30);
    }

    Studio::~Studio()
    {
        stopTimer();
        if (withDevices)
        {
            devices.removeChangeListener (this);
            devices.removeAudioCallback (&engine);
            devices.removeMidiInputDeviceCallback ({}, &engine.getMidiInput());
            devices.closeAudioDevice();
        }
    }

    void Studio::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        if (settings == nullptr)
            return;
        if (auto xml = devices.createStateXml())
            settings->setValue (audioSetupKey, xml.get());
        settings->saveIfNeeded();
    }

    //==========================================================================
    void Studio::changed (bool undoable)
    {
        engine.update (project);
        modified = true;
        if (undoable)
            recordUndoState();
        updateLiveTrack();
        sendChangeMessage();
    }

    void Studio::updateLiveTrack()
    {
        // You hear what you play through the armed track's instrument (or the selected one's).
        auto* armed = project.armedTrack();
        engine.setLiveTrack (armed != nullptr ? armed->id : selectedTrack);
    }

    void Studio::recordUndoState()
    {
        // A copy without the instruments' settings (they aren't undone, and can be large).
        auto state = project;
        for (auto& track : state.tracks)
            track.instrument.state.reset();
        undoStates.resize ((size_t) (undoIndex + 1));
        undoStates.push_back (std::move (state));
        if ((int) undoStates.size() > maxUndoSteps)
            undoStates.erase (undoStates.begin());
        undoIndex = (int) undoStates.size() - 1;
    }

    void Studio::restore (const Project& state)
    {
        project.bpm = state.bpm;
        project.numerator = state.numerator;
        project.denominator = state.denominator;
        project.loopOn = state.loopOn;
        project.loopStart = state.loopStart;
        project.loopEnd = state.loopEnd;
        project.click = state.click;
        for (auto& track : project.tracks)
            if (const auto* then = state.findTrack (track.id))
            {
                track.name = then->name;
                track.colour = then->colour;
                track.volumeDb = then->volumeDb;
                track.pan = then->pan;
                track.mute = then->mute;
                track.solo = then->solo;
                track.parts = then->parts;
            }
        changed (false);
    }

    void Studio::undo()
    {
        if (! canUndo())
            return;
        restore (undoStates[(size_t) --undoIndex]);
    }

    void Studio::redo()
    {
        if (! canRedo())
            return;
        restore (undoStates[(size_t) ++undoIndex]);
    }

    //==========================================================================
    int Studio::addTrack (const Instrument& instrument, juce::String* error)
    {
        juce::String problem;
        auto instance = host->create (instrument, engine.getSampleRate(), engine.getBlockSize(), problem);
        if (instance == nullptr)
        {
            if (error != nullptr)
                *error = problem;
            return 0;
        }

        Track track;
        track.id = project.nextTrackId++;
        track.instrument = instrument;
        track.colour = trackColour ((int) project.tracks.size());
        // Named after its instrument, numbered: "Synth 1", "Pianoteq 2"...
        const auto base = instrument.kind == Instrument::Kind::paragonSynth ? juce::String ("Synth") : instrument.getName();
        int number = 1;
        for (const auto& other : project.tracks)
            if (other.name.startsWith (base + " "))
                number = juce::jmax (number, other.name.fromLastOccurrenceOf (" ", false, false).getIntValue() + 1);
        track.name = base + " " + juce::String (number);
        project.tracks.push_back (track);

        engine.setInstrument (track.id, std::move (instance));
        selectTrack (track.id);   // and arms it (changed() is called there)
        return track.id;
    }

    void Studio::removeTrack (int trackId)
    {
        const auto it = std::find_if (project.tracks.begin(), project.tracks.end(), [trackId] (const Track& t) { return t.id == trackId; });
        if (it == project.tracks.end())
            return;
        const auto index = (int) std::distance (project.tracks.begin(), it);
        if (onInstrumentGoing)
            onInstrumentGoing (trackId);
        project.tracks.erase (it);
        engine.removeTrack (trackId);
        if (selectedTrack == trackId)
            selectedTrack = project.tracks.empty() ? 0 : project.tracks[(size_t) juce::jmin (index, (int) project.tracks.size() - 1)].id;
        changed();
    }

    bool Studio::changeInstrument (int trackId, const Instrument& instrument, juce::String* error)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr)
            return false;
        juce::String problem;
        auto instance = host->create (instrument, engine.getSampleRate(), engine.getBlockSize(), problem);
        if (instance == nullptr)
        {
            if (error != nullptr)
                *error = problem;
            return false;
        }
        track->instrument = instrument;
        if (onInstrumentGoing)
            onInstrumentGoing (trackId);
        engine.setInstrument (trackId, std::move (instance));
        changed (false);
        return true;
    }

    void Studio::renameTrack (int trackId, const juce::String& name)
    {
        if (auto* track = project.findTrack (trackId); track != nullptr && name.trim().isNotEmpty())
        {
            track->name = name.trim();
            changed();
        }
    }

    void Studio::setMute (int trackId, bool on)
    {
        if (auto* track = project.findTrack (trackId))
        {
            track->mute = on;
            changed();
        }
    }

    void Studio::setSolo (int trackId, bool on)
    {
        if (auto* track = project.findTrack (trackId))
        {
            track->solo = on;
            changed();
        }
    }

    void Studio::setVolume (int trackId, float db)
    {
        if (auto* track = project.findTrack (trackId))
        {
            track->volumeDb = juce::jlimit (-60.0f, 6.0f, db);
            changed();
        }
    }

    void Studio::setPan (int trackId, float pan)
    {
        if (auto* track = project.findTrack (trackId))
        {
            track->pan = juce::jlimit (-1.0f, 1.0f, pan);
            changed();
        }
    }

    void Studio::setArmed (int trackId)
    {
        for (auto& track : project.tracks)
            track.armed = track.id == trackId;
        changed (false);
    }

    void Studio::selectTrack (int trackId)
    {
        selectedTrack = trackId;
        // Selecting arms it (when not recording), so you play and record the track you're on.
        if (! engine.isRecording())
            for (auto& track : project.tracks)
                track.armed = track.id == trackId;
        changed (false);
    }

    //==========================================================================
    void Studio::addPart (int trackId, const Part& part)
    {
        if (auto* track = project.findTrack (trackId))
        {
            track->parts.push_back (part);
            changed();
        }
    }

    void Studio::movePart (int trackId, int index, int toTrackId, double newStart)
    {
        auto* from = project.findTrack (trackId);
        auto* to = project.findTrack (toTrackId);
        if (from == nullptr || to == nullptr || ! juce::isPositiveAndBelow (index, (int) from->parts.size()))
            return;
        auto part = from->parts[(size_t) index];
        part.start = juce::jmax (0.0, newStart);
        from->parts.erase (from->parts.begin() + index);
        to->parts.push_back (std::move (part));
        changed();
    }

    void Studio::copyPart (int trackId, int index, int toTrackId, double newStart)
    {
        const auto* from = project.findTrack (trackId);
        auto* to = project.findTrack (toTrackId);
        if (from == nullptr || to == nullptr || ! juce::isPositiveAndBelow (index, (int) from->parts.size()))
            return;
        auto part = from->parts[(size_t) index];
        part.start = juce::jmax (0.0, newStart);
        to->parts.push_back (std::move (part));
        changed();
    }

    void Studio::deletePart (int trackId, int index)
    {
        if (auto* track = project.findTrack (trackId); track != nullptr && juce::isPositiveAndBelow (index, (int) track->parts.size()))
        {
            track->parts.erase (track->parts.begin() + index);
            changed();
        }
    }

    void Studio::resizePart (int trackId, int index, double newLength)
    {
        if (auto* track = project.findTrack (trackId); track != nullptr && juce::isPositiveAndBelow (index, (int) track->parts.size()))
        {
            track->parts[(size_t) index].length = juce::jmax (0.25, newLength);
            changed();
        }
    }

    //==========================================================================
    void Studio::setTempo (double bpm)
    {
        project.bpm = juce::jlimit (Project::minBpm, Project::maxBpm, bpm);
        changed();
    }

    void Studio::setSignature (int numerator, int denominator)
    {
        project.numerator = juce::jlimit (1, 32, numerator);
        project.denominator = denominator == 2 || denominator == 8 || denominator == 16 ? denominator : 4;
        changed();
    }

    void Studio::setCycle (double start, double end)
    {
        start = juce::jmax (0.0, start);
        project.loopStart = start;
        project.loopEnd = juce::jmax (start + 0.25, end);
        changed();
    }

    void Studio::setCycleOn (bool on)
    {
        project.loopOn = on;
        changed();
    }

    void Studio::setClick (bool on)
    {
        project.click = on;
        changed (false);
    }

    //==========================================================================
    void Studio::play()
    {
        if (engine.isPlaying())
            return;
        playStart = engine.getPosition();
        engine.play();
        sendChangeMessage();
    }

    void Studio::stop()
    {
        if (! engine.isPlaying())
        {
            // Stopped already: back to where playing started (as in Cubase).
            engine.setPosition (playStart);
            sendChangeMessage();
            return;
        }
        if (engine.isRecording())
            finishRecording();
        engine.stop();
        sendChangeMessage();
    }

    void Studio::record()
    {
        if (engine.isRecording())
        {
            finishRecording();   // punch out: playing carries on
            sendChangeMessage();
            return;
        }
        auto* armed = project.armedTrack();
        if (armed == nullptr)
        {
            setArmed (selectedTrack);
            armed = project.armedTrack();
        }
        if (armed == nullptr)
            return;   // no tracks

        recordTrack = armed->id;
        recordStart = engine.getPosition();
        if (! engine.isPlaying())
            playStart = recordStart;
        recorded.clear();
        std::vector<RecordedEvent> stale;
        engine.takeRecorded (stale);   // anything left from before
        wasRecording = true;
        engine.record();
        sendChangeMessage();
    }

    void Studio::goToStart()
    {
        setPosition (project.loopOn ? project.loopStart : 0.0);
    }

    void Studio::setPosition (double beats)
    {
        engine.setPosition (beats);
        if (! engine.isPlaying())
            playStart = juce::jmax (0.0, beats);
        sendChangeMessage();
    }

    //==========================================================================
    void Studio::buildRecordingNotes (std::vector<Note>& notes, double partStart, double endBeat) const
    {
        // Pairs each note on with its note off. A note still held at the end
        // ends there; one held across the cycle's end ends at the cycle's end.
        struct Open { double beat; float velocity; };
        std::array<std::optional<Open>, 128> open;
        const auto cycleEnd = project.loopOn ? project.loopEnd : endBeat;
        const auto close = [&] (int pitch, double at)
        {
            auto& started = open[(size_t) pitch];
            if (! started)
                return;
            auto end = at >= started->beat ? at : cycleEnd;
            notes.push_back ({ started->beat - partStart, juce::jmax (0.01, end - started->beat), pitch, started->velocity });
            started.reset();
        };

        for (const auto& event : recorded)
        {
            const auto pitch = event.data1 & 0x7f;
            const auto isOn = (event.status & 0xf0) == 0x90 && event.data2 > 0;
            close (pitch, event.beat);
            if (isOn)
                open[(size_t) pitch] = Open { event.beat, event.data2 / 127.0f };
        }
        for (int pitch = 0; pitch < 128; ++pitch)
            close (pitch, endBeat);
        std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    }

    std::optional<std::pair<int, Part>> Studio::getRecordingPreview() const
    {
        if (! engine.isRecording() || recorded.empty())
            return std::nullopt;
        Part part;
        const auto bar = project.beatsPerBar();
        part.start = project.loopOn ? project.loopStart : std::floor (recordStart / bar) * bar;
        const auto now = engine.getPosition();
        buildRecordingNotes (part.notes, part.start, now);
        part.length = project.loopOn ? project.loopEnd - project.loopStart : juce::jmax (bar, now - part.start);
        return std::pair<int, Part> { recordTrack, part };
    }

    void Studio::finishRecording()
    {
        const auto endBeat = engine.getPosition();
        engine.stopRecording();
        engine.takeRecorded (recorded);
        wasRecording = false;

        Part part;
        const auto bar = project.beatsPerBar();
        const auto cycled = project.loopOn;
        part.start = cycled ? project.loopStart : std::floor (recordStart / bar) * bar;
        buildRecordingNotes (part.notes, part.start, endBeat);
        recorded.clear();
        if (part.notes.empty())
            return;   // nothing played: no part

        // The part: the cycle, or whole bars from where recording started to where it stopped.
        if (cycled)
        {
            part.length = project.loopEnd - project.loopStart;
        }
        else
        {
            auto end = endBeat;
            for (const auto& note : part.notes)
                end = juce::jmax (end, part.start + note.start + note.length);
            part.length = juce::jmax (bar, std::ceil ((end - part.start) / bar - 1.0e-9) * bar);
        }
        if (auto* track = project.findTrack (recordTrack))
        {
            part.name = track->name;
            track->parts.push_back (std::move (part));
            changed();
        }
    }

    void Studio::timerCallback()
    {
        // Notes come in while recording: collect them (the engine's list is
        // only so long) and redraw the part taking shape.
        if (engine.isRecording())
        {
            engine.takeRecorded (recorded);
            sendChangeMessage();
        }
        else if (wasRecording)
        {
            // Recording ended by itself (shouldn't happen, but never lose what was played).
            finishRecording();
        }
    }

    //==========================================================================
    void Studio::newProject()
    {
        if (onInstrumentGoing)
            onInstrumentGoing (0);
        engine.stop();
        for (const auto& track : project.tracks)
            engine.removeTrack (track.id);
        project = {};
        file = juce::File();
        selectedTrack = 0;
        engine.setPosition (0.0);
        playStart = 0.0;
        // A new song starts with one track on the Paragon Synth, ready to play.
        addTrack ({});
        engine.update (project);
        undoStates.clear();
        undoIndex = -1;
        recordUndoState();
        modified = false;
        sendChangeMessage();
    }

    bool Studio::save (const juce::File& destination)
    {
        // Each instrument's current settings go into the file with it.
        for (auto& track : project.tracks)
            if (auto* instrument = engine.getInstrument (track.id))
            {
                juce::MemoryBlock state;
                instrument->getStateInformation (state);
                track.instrument.state = std::move (state);
            }

        const auto xml = project.toXml();
        if (! xml->writeTo (destination))
            return false;
        file = destination;
        modified = false;
        sendChangeMessage();
        return true;
    }

    bool Studio::open (const juce::File& source, juce::String& problems)
    {
        const auto xml = juce::XmlDocument::parse (source);
        if (xml == nullptr)
            return false;
        auto loaded = Project::fromXml (*xml);
        if (! loaded)
            return false;

        if (onInstrumentGoing)
            onInstrumentGoing (0);
        engine.stop();
        for (const auto& track : project.tracks)
            engine.removeTrack (track.id);
        project = std::move (*loaded);

        juce::StringArray missing;
        for (auto& track : project.tracks)
        {
            juce::String error;
            if (auto instance = host->create (track.instrument, engine.getSampleRate(), engine.getBlockSize(), error))
                engine.setInstrument (track.id, std::move (instance));
            else
                missing.add (track.name + " (" + track.instrument.getName() + "): " + error);   // the track stays (its parts too), silent
        }
        problems = missing.joinIntoString ("\n");

        file = source;
        selectedTrack = project.tracks.empty() ? 0 : project.tracks.front().id;
        if (auto* armed = project.armedTrack())
            selectedTrack = armed->id;
        engine.setPosition (0.0);
        playStart = 0.0;
        engine.update (project);
        undoStates.clear();
        undoIndex = -1;
        recordUndoState();
        updateLiveTrack();
        modified = false;
        sendChangeMessage();
        return true;
    }
}
