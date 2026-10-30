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
        formats.registerBasicFormats();
        writerThread.startThread();
        // Recordings before a song is saved: a folder of their own in Documents (tests: a temporary one).
        const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
        unsavedAudioFolder = withDevices ? juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                               .getChildFile ("Paragon PR7-S").getChildFile ("Unsaved Recordings").getChildFile (stamp)
                                         : juce::File::getSpecialLocation (juce::File::tempDirectory)
                                               .getChildFile ("PR7S-test-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
        engine.setAudioSource ([this] (const juce::String& name) -> std::shared_ptr<const juce::AudioBuffer<float>>
        {
            if (auto data = getAudio (name))
                return { data, &data->sound };
            return nullptr;
        });
        if (withDevices)
        {
            settings = std::make_unique<juce::PropertiesFile> (settingsOptions());
            host = std::make_unique<PluginHost> (settings.get());

            // Output only for now (audio recording is a later stage); every
            // MIDI input on, so a keyboard plugged in just plays.
            // Two inputs and two outputs to start with (SETTINGS turns on more).
            const auto saved = settings->getXmlValue (audioSetupKey);
            devices.initialise (2, 2, saved.get(), true);
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
        endExport();
        if (audioWriter != nullptr)
        {
            engine.setRecorder (recordTrack, nullptr);
            audioWriter.reset();
        }
        writerThread.stopThread (2000);
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
        // A new sample rate: the recordings are loaded again at it.
        if (! juce::approximatelyEqual (audioCacheRate, engine.getSampleRate()))
        {
            audioCache.clear();
            engine.update (project);
        }
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

    // The lanes and their points as they were; which are shown, and the R and
    // W, stay as they are now (how the screen is, not the song).
    void Studio::restoreLanes (std::vector<AutomationLane>& lanes, const std::vector<AutomationLane>& then)
    {
        auto restored = then;
        for (auto& lane : restored)
            for (const auto& now : lanes)
                if (now.sameControl (lane))
                    lane.shown = now.shown;
        lanes = std::move (restored);
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
                restoreLanes (track.automation, then->automation);
            }
        restoreLanes (project.masterAutomation, state.masterAutomation);
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

    int Studio::addAudioTrack (bool stereo)
    {
        Track track;
        track.id = project.nextTrackId++;
        track.kind = Track::Kind::audio;
        track.stereoInput = stereo;
        track.colour = trackColour ((int) project.tracks.size());
        int number = 1;
        for (const auto& other : project.tracks)
            if (other.name.startsWith ("Audio "))
                number = juce::jmax (number, other.name.fromLastOccurrenceOf (" ", false, false).getIntValue() + 1);
        track.name = "Audio " + juce::String (number);
        project.tracks.push_back (track);
        engine.addAudioTrack (track.id);
        selectTrack (track.id);
        recordUndoState();
        return track.id;
    }

    void Studio::setTrackInput (int trackId, int input, bool stereo)
    {
        if (auto* track = project.findTrack (trackId); track != nullptr && track->isAudio())
        {
            track->input = juce::jmax (0, input);
            track->stereoInput = stereo;
            changed (false);
        }
    }

    void Studio::setMonitor (int trackId, bool on)
    {
        if (auto* track = project.findTrack (trackId); track != nullptr && track->isAudio())
        {
            track->monitor = on;
            changed (false);
        }
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
    void Studio::updatePart (int trackId, int index, const Part& part)
    {
        if (auto* track = project.findTrack (trackId); track != nullptr && juce::isPositiveAndBelow (index, (int) track->parts.size()))
        {
            auto& existing = track->parts[(size_t) index];
            const auto id = existing.id;
            existing = part;
            existing.id = id;
            existing.start = juce::jmax (0.0, existing.start);
            existing.length = juce::jmax (1.0 / 64.0, existing.length);
            existing.audioOffset = juce::jmax (0.0, existing.audioOffset);
            existing.fadeIn = juce::jmax (0.0, existing.fadeIn);
            existing.fadeOut = juce::jmax (0.0, existing.fadeOut);
            changed();
        }
    }

    //==========================================================================
    double Studio::AudioData::seconds() const noexcept
    {
        return (double) sound.getNumSamples() / sampleRate;
    }

    juce::File Studio::getAudioFolder() const
    {
        return file != juce::File() ? file.getSiblingFile (file.getFileNameWithoutExtension() + " Audio") : unsavedAudioFolder;
    }

    std::shared_ptr<const Studio::AudioData> Studio::getAudio (const juce::String& name)
    {
        const auto rate = engine.getSampleRate();
        if (! juce::approximatelyEqual (audioCacheRate, rate))
        {
            audioCache.clear();
            audioCacheRate = rate;
        }
        if (auto found = audioCache.find (name); found != audioCache.end())
            return found->second;

        // Read whole, both sides (a mono file on both), at the engine's sample rate.
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (getAudioFolder().getChildFile (name)));
        if (reader == nullptr || reader->lengthInSamples <= 0)
            return nullptr;
        const auto length = (int) juce::jmin ((juce::int64) std::numeric_limits<int>::max() / 2, reader->lengthInSamples);
        juce::AudioBuffer<float> read (2, length);
        reader->read (&read, 0, length, 0, true, true);
        if (reader->numChannels == 1)
            read.copyFrom (1, 0, read, 0, 0, length);

        auto data = std::make_shared<AudioData>();
        data->sampleRate = rate;
        if (juce::approximatelyEqual (reader->sampleRate, rate))
        {
            data->sound = std::move (read);
        }
        else
        {
            const auto ratio = reader->sampleRate / rate;
            const auto converted = (int) std::floor ((double) length / ratio);
            data->sound.setSize (2, converted);
            for (int channel = 0; channel < 2; ++channel)
            {
                juce::LagrangeInterpolator interpolator;
                interpolator.process (ratio, read.getReadPointer (channel), data->sound.getWritePointer (channel), converted);
            }
        }
        const auto samples = data->sound.getNumSamples();
        for (int from = 0; from < samples; from += AudioData::peakStep)
        {
            const auto count = juce::jmin (AudioData::peakStep, samples - from);
            const auto left = data->sound.getMagnitude (0, from, count), right = data->sound.getMagnitude (1, from, count);
            data->peaks.push_back (juce::jmax (left, right));
        }
        audioCache[name] = data;
        return data;
    }

    double Studio::recordingLatency() const
    {
        if (latencyOverride)
            return *latencyOverride;
        if (auto* device = devices.getCurrentAudioDevice())
            return (double) (device->getInputLatencyInSamples() + device->getOutputLatencyInSamples()) / juce::jmax (1.0, device->getCurrentSampleRate());
        return 0.0;
    }

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
        insertGone (busId, slot, false);   // another plugin: the old one's automation means nothing to it
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
        insertGone (busId, slot, true);
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

    //==========================================================================
    std::vector<AutomationLane>* Studio::lanesOf (int busId)
    {
        if (busId == Engine::masterBus)
            return &project.masterAutomation;
        auto* track = project.findTrack (busId);
        return track != nullptr ? &track->automation : nullptr;
    }

    const std::vector<AutomationLane>* Studio::getAutomation (int busId) const
    {
        return const_cast<Studio*> (this)->lanesOf (busId);
    }

    bool Studio::isAutomationOpen (int busId) const
    {
        if (busId == Engine::masterBus)
            return project.masterAutomationOpen;
        const auto* track = project.findTrack (busId);
        return track != nullptr && track->automationOpen;
    }

    bool Studio::getAutomationRead (int busId) const
    {
        if (busId == Engine::masterBus)
            return project.masterRead;
        const auto* track = project.findTrack (busId);
        return track != nullptr && track->automationRead;
    }

    bool Studio::getAutomationWrite (int busId) const
    {
        if (busId == Engine::masterBus)
            return project.masterWrite;
        const auto* track = project.findTrack (busId);
        return track != nullptr && track->automationWrite;
    }

    void Studio::setAutomationRead (int busId, bool on)
    {
        if (busId == Engine::masterBus)
            project.masterRead = on;
        else if (auto* track = project.findTrack (busId))
            track->automationRead = on;
        else
            return;
        changed (false);
    }

    void Studio::setAutomationWrite (int busId, bool on)
    {
        if (busId == Engine::masterBus)
        {
            project.masterWrite = on;
            project.masterRead = project.masterRead || on;
        }
        else if (auto* track = project.findTrack (busId))
        {
            track->automationWrite = on;
            track->automationRead = track->automationRead || on;   // R comes on with W (and stays when W goes off)
        }
        else
        {
            return;
        }
        changed (false);
    }

    bool Studio::anyAutomationRead() const
    {
        return project.masterRead || std::any_of (project.tracks.begin(), project.tracks.end(), [] (const Track& t) { return t.automationRead; });
    }

    bool Studio::anyAutomationWrite() const
    {
        return project.masterWrite || std::any_of (project.tracks.begin(), project.tracks.end(), [] (const Track& t) { return t.automationWrite; });
    }

    void Studio::setAutomationReadAll (bool on)
    {
        project.masterRead = on;
        for (auto& track : project.tracks)
            track.automationRead = on;
        changed (false);
    }

    void Studio::setAutomationWriteAll (bool on)
    {
        project.masterWrite = on;
        project.masterRead = project.masterRead || on;
        for (auto& track : project.tracks)
        {
            track.automationWrite = on;
            track.automationRead = track.automationRead || on;
        }
        changed (false);
    }

    AutomationLane Studio::AutomationControl::asLane() const
    {
        AutomationLane lane;
        lane.target = target;
        lane.slot = slot;
        lane.parameter = parameter;
        lane.parameterName = name;
        return lane;
    }

    std::vector<Studio::AutomationControl> Studio::getAutomationControls (int busId) const
    {
        using T = AutomationLane::Target;
        std::vector<AutomationControl> controls;
        const auto addKnobs = [&controls] (juce::AudioPluginInstance* plugin, T target, int slot, const juce::String& group)
        {
            if (plugin == nullptr)
                return;
            const auto& parameters = plugin->getParameters();
            for (int i = 0; i < parameters.size(); ++i)
                if (parameters[i]->isAutomatable())
                    controls.push_back ({ target, slot, i, parameters[i]->getName (64), group });
        };
        if (busId == Engine::masterBus)
        {
            controls.push_back ({ T::volume, 0, 0, "Volume", {} });
        }
        else
        {
            if (project.findTrack (busId) == nullptr)
                return controls;
            controls.push_back ({ T::volume, 0, 0, "Volume", {} });
            controls.push_back ({ T::pan, 0, 0, "Pan", {} });
            controls.push_back ({ T::mute, 0, 0, "Mute", {} });
            if (auto* instrument = engine.getInstrument (busId))
                addKnobs (instrument, T::instrument, 0, "Instrument: " + instrument->getName());
        }
        for (int slot = 0; slot < engine.getNumEffects (busId); ++slot)
            if (auto* effect = engine.getEffect (busId, slot))
                addKnobs (effect, T::insert, slot, "Insert " + juce::String (slot + 1) + ": " + effect->getName());
        return controls;
    }

    juce::AudioPluginInstance* Studio::pluginFor (int busId, const AutomationLane& lane) const
    {
        if (lane.target == AutomationLane::Target::instrument)
            return busId == Engine::masterBus ? nullptr : engine.getInstrument (busId);
        if (lane.target == AutomationLane::Target::insert)
            return engine.getEffect (busId, lane.slot);
        return nullptr;
    }

    juce::String Studio::getLaneName (int busId, const AutomationLane& lane) const
    {
        switch (lane.target)
        {
            case AutomationLane::Target::volume: return "Volume";
            case AutomationLane::Target::pan:    return "Pan";
            case AutomationLane::Target::mute:   return "Mute";
            case AutomationLane::Target::instrument:
            case AutomationLane::Target::insert: break;
        }
        const auto prefix = lane.target == AutomationLane::Target::insert ? "Ins. " + juce::String (lane.slot + 1) + ": " : juce::String();
        if (auto* plugin = pluginFor (busId, lane))
            if (lane.parameter < plugin->getParameters().size())
                return prefix + plugin->getParameters()[lane.parameter]->getName (64);
        return prefix + lane.parameterName;
    }

    float Studio::getControlValue (int busId, const AutomationLane& lane) const
    {
        const auto* track = project.findTrack (busId);
        switch (lane.target)
        {
            case AutomationLane::Target::volume: return volumeToTravel (busId == Engine::masterBus ? project.masterVolumeDb : track != nullptr ? track->volumeDb : 0.0f);
            case AutomationLane::Target::pan:    return panToTravel (track != nullptr ? track->pan : 0.0f);
            case AutomationLane::Target::mute:   return track != nullptr && track->mute ? 1.0f : 0.0f;
            case AutomationLane::Target::instrument:
            case AutomationLane::Target::insert: break;
        }
        if (auto* plugin = pluginFor (busId, lane))
            if (lane.parameter < plugin->getParameters().size())
                return plugin->getParameters()[lane.parameter]->getValue();
        return 0.0f;
    }

    void Studio::setAutomationOpen (int busId, bool open)
    {
        auto* lanes = lanesOf (busId);
        if (lanes == nullptr)
            return;
        if (open && std::none_of (lanes->begin(), lanes->end(), [] (const AutomationLane& l) { return l.shown; }))
        {
            // Nothing was open: the lanes with points, or else the first control.
            for (auto& lane : *lanes)
                lane.shown = ! lane.points.empty();
            if (std::none_of (lanes->begin(), lanes->end(), [] (const AutomationLane& l) { return l.shown; }))
            {
                const auto controls = getAutomationControls (busId);
                if (! controls.empty())
                    lanes->insert (lanes->begin(), controls.front().asLane());
            }
        }
        if (busId == Engine::masterBus)
            project.masterAutomationOpen = open;
        else
            project.findTrack (busId)->automationOpen = open;
        changed();   // (a new lane is an edit; undo leaves what's shown as it is)
    }

    void Studio::showAllUsedAutomation()
    {
        const auto showUsed = [] (std::vector<AutomationLane>& lanes, bool& open)
        {
            for (auto& lane : lanes)
                if (! lane.points.empty())
                    lane.shown = open = true;
        };
        showUsed (project.masterAutomation, project.masterAutomationOpen);
        for (auto& track : project.tracks)
            showUsed (track.automation, track.automationOpen);
        changed (false);
    }

    void Studio::appendAutomationLane (int busId, int laneIndex)
    {
        auto* lanes = lanesOf (busId);
        if (lanes == nullptr)
            return;
        for (const auto& control : getAutomationControls (busId))
        {
            const auto lane = control.asLane();
            const auto existing = std::find_if (lanes->begin(), lanes->end(), [&] (const AutomationLane& l) { return l.sameControl (lane); });
            if (existing != lanes->end() && existing->shown)
                continue;
            // The next control not shown: its lane (kept points and all) after this one.
            auto moved = existing != lanes->end() ? *existing : lane;
            moved.shown = true;
            if (existing != lanes->end())
            {
                const auto from = (int) std::distance (lanes->begin(), existing);
                lanes->erase (existing);
                if (from <= laneIndex)
                    --laneIndex;
            }
            lanes->insert (lanes->begin() + juce::jlimit (0, (int) lanes->size(), laneIndex + 1), moved);
            changed();
            return;
        }
    }

    void Studio::setLaneControl (int busId, int laneIndex, const AutomationControl& control)
    {
        auto* lanes = lanesOf (busId);
        if (lanes == nullptr || ! juce::isPositiveAndBelow (laneIndex, (int) lanes->size()))
            return;
        const auto lane = control.asLane();
        if ((*lanes)[(size_t) laneIndex].sameControl (lane))
            return;
        // The control it showed: kept, hidden, if it has points.
        auto previous = (*lanes)[(size_t) laneIndex];
        previous.shown = false;
        auto chosen = lane;
        if (const auto existing = std::find_if (lanes->begin(), lanes->end(), [&] (const AutomationLane& l) { return l.sameControl (lane); });
            existing != lanes->end())
        {
            chosen = *existing;
            const auto from = (int) std::distance (lanes->begin(), existing);
            lanes->erase (existing);
            if (from < laneIndex)
                --laneIndex;
        }
        chosen.shown = true;
        (*lanes)[(size_t) laneIndex] = chosen;
        if (! previous.points.empty())
            lanes->push_back (previous);
        changed();
    }

    void Studio::removeAutomationParameter (int busId, int laneIndex)
    {
        auto* lanes = lanesOf (busId);
        if (lanes == nullptr || ! juce::isPositiveAndBelow (laneIndex, (int) lanes->size()))
            return;
        lanes->erase (lanes->begin() + laneIndex);
        if (std::none_of (lanes->begin(), lanes->end(), [] (const AutomationLane& l) { return l.shown; }))
        {
            if (busId == Engine::masterBus)
                project.masterAutomationOpen = false;
            else
                project.findTrack (busId)->automationOpen = false;
        }
        changed();
    }

    void Studio::removeUnusedAutomation (int busId)
    {
        auto* lanes = lanesOf (busId);
        if (lanes == nullptr)
            return;
        lanes->erase (std::remove_if (lanes->begin(), lanes->end(), [] (const AutomationLane& l) { return l.points.empty(); }), lanes->end());
        if (lanes->empty())
        {
            if (busId == Engine::masterBus)
                project.masterAutomationOpen = false;
            else
                project.findTrack (busId)->automationOpen = false;
        }
        changed();
    }

    void Studio::setLaneMuted (int busId, int laneIndex, bool muted)
    {
        auto* lanes = lanesOf (busId);
        if (lanes == nullptr || ! juce::isPositiveAndBelow (laneIndex, (int) lanes->size()))
            return;
        (*lanes)[(size_t) laneIndex].muted = muted;
        changed();
    }

    void Studio::setAutomationPoints (int busId, int laneIndex, std::vector<AutomationPoint> points)
    {
        auto* lanes = lanesOf (busId);
        if (lanes == nullptr || ! juce::isPositiveAndBelow (laneIndex, (int) lanes->size()))
            return;
        for (auto& point : points)
        {
            point.beat = juce::jmax (0.0, point.beat);
            point.value = juce::jlimit (0.0f, 1.0f, point.value);
        }
        std::stable_sort (points.begin(), points.end(), [] (const auto& a, const auto& b) { return a.beat < b.beat; });
        (*lanes)[(size_t) laneIndex].points = std::move (points);
        // Editing a lane turns R on (as clicking in an automation track does).
        if (busId == Engine::masterBus)
            project.masterRead = true;
        else
            project.findTrack (busId)->automationRead = true;
        changed();
    }

    void Studio::insertGone (int busId, int slot, bool shiftUp)
    {
        const auto fix = [slot, shiftUp] (std::vector<AutomationLane>& lanes)
        {
            lanes.erase (std::remove_if (lanes.begin(), lanes.end(), [slot] (const AutomationLane& l)
            {
                return l.target == AutomationLane::Target::insert && l.slot == slot;
            }), lanes.end());
            if (shiftUp)
                for (auto& lane : lanes)
                    if (lane.target == AutomationLane::Target::insert && lane.slot > slot)
                        --lane.slot;
        };
        const auto fixIn = [&] (Project& p)
        {
            if (busId == Engine::masterBus)
                fix (p.masterAutomation);
            else if (auto* track = p.findTrack (busId))
                fix (track->automation);
        };
        fixIn (project);
        for (auto& state : undoStates)
            fixIn (state);
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

        if (armed->isAudio())
        {
            // A new file: "<track> 01.wav", 24-bit, at the engine's rate, its
            // writer fed from the audio thread and putting it on disk on its own.
            auto folder = getAudioFolder();
            folder.createDirectory();
            int take = 1;
            do
                audioRecording = folder.getChildFile (juce::File::createLegalFileName (armed->name) + " " + juce::String (take++).paddedLeft ('0', 2) + ".wav");
            while (audioRecording.exists());
            std::unique_ptr<juce::OutputStream> stream (audioRecording.createOutputStream());
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> writer;
            if (stream != nullptr)
                writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (engine.getSampleRate())
                                                                                     .withNumChannels (armed->stereoInput ? 2 : 1)
                                                                                     .withBitsPerSample (24));
            if (writer == nullptr)
                return;   // couldn't write there
            audioWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer.release(), writerThread,
                                                                                     (int) (engine.getSampleRate() * 8.0));
            engine.setRecorder (recordTrack, audioWriter.get());
        }
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
        if (engine.isRecording() && audioWriter != nullptr)
        {
            // Audio: a block from where recording began to now.
            Part part;
            const auto first = engine.getAudioRecordStart();
            part.start = first >= 0 ? (double) first * project.bpm / 60.0 / engine.getSampleRate() : recordStart;
            part.length = juce::jmax (0.0, engine.getPosition() - part.start);
            if (project.loopOn && engine.getPosition() < part.start)
                part.length = project.loopEnd - part.start;   // round the cycle: the first pass is kept
            return std::pair<int, Part> { recordTrack, part };
        }
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

    void Studio::finishAudioRecording()
    {
        // Its writer out of the audio's way, then finished (the rest goes to disk).
        engine.setRecorder (recordTrack, nullptr);
        audioWriter.reset();
        const auto startSample = engine.getAudioRecordStart();
        const auto name = audioRecording.getFileName();
        audioCache.erase (name);
        auto data = startSample >= 0 ? getAudio (name) : nullptr;
        if (data == nullptr || data->sound.getNumSamples() == 0)
        {
            audioRecording.deleteFile();   // nothing recorded
            return;
        }
        // At the sample recording began, and its first stretch (the
        // interface's delay) skipped: what was played lands where it was played.
        const auto latency = juce::jmin (recordingLatency(), data->seconds() * 0.5);
        Part part;
        part.start = (double) startSample * project.bpm / 60.0 / engine.getSampleRate();
        part.audioFile = name;
        part.audioOffset = latency;
        part.length = (data->seconds() - latency) / project.secondsPerBeat();
        if (auto* track = project.findTrack (recordTrack))
        {
            part.name = track->name;
            track->parts.push_back (std::move (part));
            changed();
        }
    }

    void Studio::finishRecording()
    {
        const auto endBeat = engine.getPosition();
        engine.stopRecording();
        wasRecording = false;
        if (audioWriter != nullptr)
        {
            finishAudioRecording();
            return;
        }
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
        audioCache.clear();
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

        // The recordings go with the song: into "<song> Audio" beside it
        // (copied from where they were: the unsaved folder, or the song's old place).
        const auto from = getAudioFolder();
        const auto to = destination.getSiblingFile (destination.getFileNameWithoutExtension() + " Audio");
        if (from != to)
            for (const auto& track : project.tracks)
                for (const auto& part : track.parts)
                    if (part.isAudio() && from.getChildFile (part.audioFile).existsAsFile() && ! to.getChildFile (part.audioFile).exists())
                    {
                        to.createDirectory();
                        if (! from.getChildFile (part.audioFile).copyFileTo (to.getChildFile (part.audioFile)))
                            return false;
                    }

        const auto xml = project.toXml();
        if (! xml->writeTo (destination))
            return false;
        file = destination;
        modified = false;
        sendChangeMessage();
        return true;
    }

    bool Studio::beginExport (const juce::File& destination, const ExportOptions& options, juce::String& error)
    {
        if (exportJob != nullptr || engine.isRecording())
        {
            error = "Not while recording or exporting.";
            return false;
        }
        if (options.end <= options.start)
        {
            error = "There's nothing to export: the range is empty.";
            return false;
        }
        auto job = std::make_unique<ExportJob>();
        job->file = destination;
        job->previousRate = engine.getSampleRate();
        job->previousBlockSize = engine.getBlockSize();
        job->previousPosition = engine.getPosition();

        destination.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (destination.createOutputStream());
        const auto rate = options.sampleRate > 0.0 ? options.sampleRate : job->previousRate;
        if (stream != nullptr)
            job->writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate)
                                                                                                        .withNumChannels (2)
                                                                                                        .withBitsPerSample (options.bitsPerSample == 16 ? 16 : 24));
        if (job->writer == nullptr)
        {
            error = "Couldn't write " + destination.getFullPathName();
            return false;
        }

        // The interface let go; the engine at the export's rate (the
        // recordings are loaded again at it), stopped.
        engine.stop();
        if (withDevices)
            devices.removeAudioCallback (&engine);
        if (! juce::approximatelyEqual (rate, job->previousRate))
            engine.prepare (rate, job->previousBlockSize);

        // What's played: the song with the click as asked, no cycle, and
        // nothing starting at or after the end (so the ring-out is only
        // what's already sounding).
        auto playing = project;
        playing.loopOn = false;
        playing.click = options.click;
        for (auto& track : playing.tracks)
        {
            track.parts.erase (std::remove_if (track.parts.begin(), track.parts.end(),
                                               [&options] (const Part& p) { return p.start >= options.end; }),
                               track.parts.end());
            for (auto& part : track.parts)
                part.length = juce::jmin (part.length, options.end - part.start);
        }
        engine.update (playing);
        engine.setNonRealtime (true);

        job->total = (juce::int64) std::llround (((options.end - options.start) * 60.0 / project.bpm + juce::jmax (0.0, options.tailSeconds)) * rate);
        engine.setPosition (options.start);
        engine.play();
        exportJob = std::move (job);
        return true;
    }

    bool Studio::renderExport (std::function<bool (double)> keepGoing)
    {
        if (exportJob == nullptr)
            return false;
        auto& job = *exportJob;
        const auto blockSize = juce::jmax (64, engine.getBlockSize());
        juce::AudioBuffer<float> block (2, blockSize);
        const auto reportEvery = (juce::int64) (engine.getSampleRate() * 0.5);
        juce::int64 sinceReport = 0;
        for (juce::int64 done = 0; done < job.total;)
        {
            const auto length = (int) juce::jmin ((juce::int64) blockSize, job.total - done);
            engine.render (block, length);
            if (! job.writer->writeFromAudioSampleBuffer (block, 0, length))
                return false;
            done += length;
            sinceReport += length;
            if (keepGoing && sinceReport >= reportEvery)
            {
                sinceReport = 0;
                if (! keepGoing ((double) done / (double) job.total))
                    return false;
            }
        }
        job.finished = true;
        return true;
    }

    void Studio::endExport()
    {
        if (exportJob == nullptr)
            return;
        auto job = std::move (exportJob);
        engine.stop();
        job->writer.reset();   // the file finished
        engine.setNonRealtime (false);
        if (! job->finished)
            job->file.deleteFile();
        // Everything as it was.
        if (! juce::approximatelyEqual (engine.getSampleRate(), job->previousRate))
            engine.prepare (job->previousRate, job->previousBlockSize);
        engine.update (project);
        engine.setPosition (job->previousPosition);
        if (withDevices)
            devices.addAudioCallback (&engine);
        sendChangeMessage();
    }

    bool Studio::exportMix (const juce::File& destination, const ExportOptions& options, juce::String& error)
    {
        if (! beginExport (destination, options, error))
            return false;
        const auto rendered = renderExport();
        endExport();
        if (! rendered)
            error = "Couldn't write " + destination.getFullPathName();
        return rendered;
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

        file = source;
        audioCache.clear();
        juce::StringArray missing;
        for (auto& track : project.tracks)
        {
            if (track.isAudio())
            {
                engine.addAudioTrack (track.id);
                for (const auto& part : track.parts)
                    if (part.isAudio() && getAudio (part.audioFile) == nullptr)
                        missing.add (track.name + ": the recording " + part.audioFile + " isn't in " + getAudioFolder().getFullPathName());
                continue;
            }
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
