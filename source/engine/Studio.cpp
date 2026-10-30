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
        numberParts();
        engine.update (project);
        modified = true;
        if (undoable)
            recordUndoState();
        updateLiveTrack();
        sendChangeMessage();
    }

    void Studio::numberParts()
    {
        for (auto& track : project.tracks)
            for (auto& part : track.parts)
                if (part.id == 0)
                    part.id = nextPartId++;
    }

    std::optional<std::pair<int, int>> Studio::findPart (int partId) const
    {
        for (const auto& track : project.tracks)
            for (size_t i = 0; i < track.parts.size(); ++i)
                if (partId != 0 && track.parts[i].id == partId)
                    return std::pair<int, int> { track.id, (int) i };
        return std::nullopt;
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
        {
            track.instrument.state.reset();
            for (auto& insert : track.inserts)
                insert.state.reset();
        }
        for (auto& insert : state.masterInserts)
            insert.state.reset();
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
        project.masterVolumeDb = state.masterVolumeDb;
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
        // Adding a track isn't undone, but what's done to it next is: undo
        // needs a step that has the track in it to go back to.
        recordUndoState();
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
        for (int slot = 0; slot < (int) it->inserts.size(); ++slot)
            effectGoing (trackId, slot);
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

    void Studio::setVolume (int trackId, float db, bool undoable)
    {
        if (auto* track = project.findTrack (trackId))
        {
            track->volumeDb = juce::jlimit (-60.0f, 6.0f, db);
            changed (undoable);
        }
    }

    void Studio::setPan (int trackId, float pan, bool undoable)
    {
        if (auto* track = project.findTrack (trackId))
        {
            track->pan = juce::jlimit (-1.0f, 1.0f, pan);
            changed (undoable);
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
            track->parts.back().id = 0;
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
        part.id = 0;   // a new part: numbered in changed()
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

    void Studio::setNotes (int trackId, int index, std::vector<Note> notes)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || ! juce::isPositiveAndBelow (index, (int) track->parts.size()))
            return;
        for (auto& note : notes)
        {
            note.start = juce::jmax (0.0, note.start);
            note.length = juce::jmax (0.001, note.length);
            note.pitch = juce::jlimit (0, 127, note.pitch);
            note.velocity = juce::jlimit (1.0f / 127.0f, 1.0f, note.velocity);
        }
        auto& part = track->parts[(size_t) index];
        if (part.notes == notes)
            return;   // nothing changed: no undo step
        part.notes = std::move (notes);
        changed();
    }

    //==========================================================================
    std::vector<Insert>* Studio::insertsOf (int busId)
    {
        if (busId == Engine::masterBus)
            return &project.masterInserts;
        auto* track = project.findTrack (busId);
        return track != nullptr ? &track->inserts : nullptr;
    }

    const std::vector<Insert>* Studio::getInserts (int busId) const
    {
        return const_cast<Studio*> (this)->insertsOf (busId);
    }

    void Studio::effectGoing (int busId, int slot)
    {
        if (onEffectGoing)
            if (auto* effect = engine.getEffect (busId, slot))
                onEffectGoing (effect);
    }

    void Studio::removeAllEffects()
    {
        if (onEffectGoing)
            onEffectGoing (nullptr);
        while (engine.getNumEffects (Engine::masterBus) > 0)
            engine.removeEffect (Engine::masterBus, 0);
    }

    bool Studio::addInsert (int busId, const juce::PluginDescription& description, juce::String* error)
    {
        const auto* inserts = insertsOf (busId);
        if (inserts == nullptr || (int) inserts->size() >= Project::maxInserts)
        {
            if (error != nullptr)
                *error = "There's no room for another effect here (" + juce::String (Project::maxInserts) + " at most).";
            return false;
        }
        Insert insert;
        insert.plugin = description;
        juce::String problem;
        auto instance = host->createEffect (insert, engine.getSampleRate(), engine.getBlockSize(), problem);
        if (instance == nullptr)
        {
            if (error != nullptr)
                *error = problem;
            return false;
        }
        return addInsert (busId, insert, std::move (instance));
    }

    bool Studio::addInsert (int busId, const Insert& insert, std::unique_ptr<juce::AudioPluginInstance> instance)
    {
        auto* inserts = insertsOf (busId);
        if (inserts == nullptr || (int) inserts->size() >= Project::maxInserts)
            return false;
        inserts->push_back (insert);
        engine.insertEffect (busId, (int) inserts->size() - 1, std::move (instance));
        changed (false);
        return true;
    }

    bool Studio::replaceInsert (int busId, int slot, const juce::PluginDescription& description, juce::String* error)
    {
        auto* inserts = insertsOf (busId);
        if (inserts == nullptr || ! juce::isPositiveAndBelow (slot, (int) inserts->size()))
            return false;
        Insert insert;
        insert.plugin = description;
        juce::String problem;
        auto instance = host->createEffect (insert, engine.getSampleRate(), engine.getBlockSize(), problem);
        if (instance == nullptr)
        {
            if (error != nullptr)
                *error = problem;
            return false;
        }
        effectGoing (busId, slot);
        engine.removeEffect (busId, slot);
        engine.insertEffect (busId, slot, std::move (instance));
        (*inserts)[(size_t) slot] = insert;
        changed (false);
        return true;
    }

    void Studio::removeInsert (int busId, int slot)
    {
        auto* inserts = insertsOf (busId);
        if (inserts == nullptr || ! juce::isPositiveAndBelow (slot, (int) inserts->size()))
            return;
        effectGoing (busId, slot);
        engine.removeEffect (busId, slot);
        inserts->erase (inserts->begin() + slot);
        changed (false);
    }

    void Studio::setInsertBypass (int busId, int slot, bool bypass)
    {
        auto* inserts = insertsOf (busId);
        if (inserts == nullptr || ! juce::isPositiveAndBelow (slot, (int) inserts->size()))
            return;
        (*inserts)[(size_t) slot].bypass = bypass;
        changed (false);
    }

    void Studio::setMasterVolume (float db, bool undoable)
    {
        project.masterVolumeDb = juce::jlimit (-60.0f, 6.0f, db);
        changed (undoable);
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
            // Stopped already: back to where playing started (inspired by Cubase's workflow).
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
        removeAllEffects();
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
        // And each effect's.
        const auto keepEffectStates = [this] (int busId, std::vector<Insert>& inserts)
        {
            for (int slot = 0; slot < (int) inserts.size(); ++slot)
                if (auto* effect = engine.getEffect (busId, slot))
                {
                    juce::MemoryBlock state;
                    effect->getStateInformation (state);
                    inserts[(size_t) slot].state = std::move (state);
                }
        };
        for (auto& track : project.tracks)
            keepEffectStates (track.id, track.inserts);
        keepEffectStates (Engine::masterBus, project.masterInserts);

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
        removeAllEffects();
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
        // The effects: one that can't be loaded keeps its place (and its
        // settings, for when it's back), and the sound goes past it.
        const auto loadEffects = [&] (int busId, const juce::String& where, const std::vector<Insert>& inserts)
        {
            for (int slot = 0; slot < (int) inserts.size(); ++slot)
            {
                juce::String error;
                auto instance = host->createEffect (inserts[(size_t) slot], engine.getSampleRate(), engine.getBlockSize(), error);
                if (instance == nullptr)
                    missing.add (where + ", insert " + juce::String (slot + 1) + " (" + inserts[(size_t) slot].plugin.name + "): " + error);
                engine.insertEffect (busId, slot, std::move (instance));
            }
        };
        for (const auto& track : project.tracks)
            loadEffects (track.id, track.name, track.inserts);
        loadEffects (Engine::masterBus, "Master", project.masterInserts);
        problems = missing.joinIntoString ("\n");
        numberParts();

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
