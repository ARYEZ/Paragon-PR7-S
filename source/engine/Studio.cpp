#include "Studio.h"

#include "MidiImport.h"

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
        engine.onPluginGoing = [this] (juce::AudioProcessor* plugin) { unwatch (plugin); };
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
        unwatchAll();
        engine.onPluginGoing = nullptr;   // (the watchers are gone first)
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
        watchPlugins();
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
                track.output = then->output;
                track.sends = then->sends;
                track.sendsBypass = then->sendsBypass;
            }
        restoreLanes (project.masterAutomation, state.masterAutomation);
        // The tracks' order as it was (tracks added since keep their places after them).
        const auto placeIn = [&state] (int id)
        {
            for (int i = 0; i < (int) state.tracks.size(); ++i)
                if (state.tracks[(size_t) i].id == id)
                    return i;
            return (int) state.tracks.size();
        };
        std::stable_sort (project.tracks.begin(), project.tracks.end(),
                          [&] (const Track& a, const Track& b) { return placeIn (a.id) < placeIn (b.id); });
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

    int Studio::addFxChannel (const juce::PluginDescription* effect, juce::String* error)
    {
        Insert insert;
        std::unique_ptr<juce::AudioPluginInstance> instance;
        if (effect != nullptr)
        {
            insert.plugin = *effect;
            juce::String problem;
            instance = host->createEffect (insert, engine.getSampleRate(), engine.getBlockSize(), problem);
            if (instance == nullptr)
            {
                if (error != nullptr)
                    *error = problem;
                return 0;
            }
        }
        return addFxChannel (insert, std::move (instance));
    }

    int Studio::addFxChannel (const Insert& insert, std::unique_ptr<juce::AudioPluginInstance> instance)
    {
        // Named as the manual's are: "FX 1-<effect>".
        int number = 1;
        for (const auto& other : project.tracks)
            if (other.isFx())
                ++number;
        Track track;
        track.id = project.nextTrackId++;
        track.kind = Track::Kind::fx;
        track.colour = trackColour ((int) project.tracks.size());
        track.name = "FX " + juce::String (number) + (insert.isEmpty() ? juce::String() : "-" + insert.plugin.name);
        project.tracks.push_back (track);
        engine.addChannel (track.id, false);
        if (! insert.isEmpty())
        {
            project.tracks.back().inserts.push_back (insert);
            engine.insertEffect (track.id, 0, std::move (instance));
        }
        selectTrack (track.id);
        recordUndoState();
        return track.id;
    }

    int Studio::addGroupChannel()
    {
        int number = 1;
        for (const auto& other : project.tracks)
            if (other.isGroup())
                ++number;
        Track track;
        track.id = project.nextTrackId++;
        track.kind = Track::Kind::group;
        track.colour = trackColour ((int) project.tracks.size());
        track.name = "Group " + juce::String (number);
        project.tracks.push_back (track);
        engine.addChannel (track.id, true);
        selectTrack (track.id);
        recordUndoState();
        return track.id;
    }

    void Studio::setOutput (int trackId, int groupId)
    {
        auto* track = project.findTrack (trackId);
        const auto* group = project.findTrack (groupId);
        if (track == nullptr || track->isChannel() || (groupId != 0 && (group == nullptr || ! group->isGroup())))
            return;   // (FX and group channels go to the master)
        track->output = groupId;
        changed();
    }

    void Studio::setSend (int trackId, int slot, const Send& send)
    {
        auto* track = project.findTrack (trackId);
        const auto* fx = project.findTrack (send.target);
        if (track == nullptr || track->isFx() || ! juce::isPositiveAndBelow (slot, Project::maxSends)
            || (send.target != 0 && (fx == nullptr || ! fx->isFx())))
            return;
        if ((int) track->sends.size() <= slot)
            track->sends.resize ((size_t) slot + 1);
        auto& kept = track->sends[(size_t) slot];
        kept = send;
        kept.levelDb = juce::jlimit (-60.0f, 6.0f, send.levelDb);
        while (! track->sends.empty() && track->sends.back().target == 0)
            track->sends.pop_back();
        changed();
    }

    void Studio::setSendLevel (int trackId, int slot, float db, bool undoable)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || ! juce::isPositiveAndBelow (slot, (int) track->sends.size()))
            return;
        track->sends[(size_t) slot].levelDb = juce::jlimit (-60.0f, 6.0f, db);
        const auto written = writeValue (trackId, control (AutomationLane::Target::send, slot), volumeToTravel (track->sends[(size_t) slot].levelDb));
        changed (undoable && ! written);
    }

    void Studio::removeSend (int trackId, int slot)
    {
        setSend (trackId, slot, {});
    }

    void Studio::setSendsBypass (int trackId, bool bypass)
    {
        if (auto* track = project.findTrack (trackId))
        {
            track->sendsBypass = bypass;
            changed();
        }
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
        unwatch (engine.getInstrument (trackId));
        for (int slot = 0; slot < (int) it->inserts.size(); ++slot)
            effectGoing (trackId, slot);
        const auto wasChannel = it->isChannel();
        project.tracks.erase (it);
        engine.removeTrack (trackId);
        if (wasChannel)   // what was sent or routed to it: back to the master, no send
            for (auto& track : project.tracks)
            {
                if (track.output == trackId)
                    track.output = 0;
                for (auto& send : track.sends)
                    if (send.target == trackId)
                        send = {};
                while (! track.sends.empty() && track.sends.back().target == 0)
                    track.sends.pop_back();
            }
        selectedTracks.erase (std::remove (selectedTracks.begin(), selectedTracks.end(), trackId), selectedTracks.end());
        if (selectedTrack == trackId)
        {
            selectedTrack = project.tracks.empty() ? 0 : project.tracks[(size_t) juce::jmin (index, (int) project.tracks.size() - 1)].id;
            if (selectedTrack != 0 && selectedTracks.empty())
                selectedTracks = { selectedTrack };
        }
        changed();
    }

    //==========================================================================
    void Studio::toggleTrackSelected (int trackId)
    {
        if (project.findTrack (trackId) == nullptr)
            return;
        if (isTrackSelected (trackId) && selectedTracks.size() > 1)
        {
            selectedTracks.erase (std::remove (selectedTracks.begin(), selectedTracks.end(), trackId), selectedTracks.end());
            if (selectedTrack == trackId)
                selectedTrack = selectedTracks.back();
            changed (false);
            return;
        }
        if (! isTrackSelected (trackId))
            selectedTracks.push_back (trackId);
        selectedTrack = trackId;
        changed (false);
    }

    void Studio::selectTrackRun (int trackId)
    {
        if (project.findTrack (trackId) == nullptr)
            return;
        // Shift-click on a selected one (not the only one): deselected.
        if (isTrackSelected (trackId) && selectedTracks.size() > 1)
        {
            toggleTrackSelected (trackId);
            return;
        }
        int from = -1, to = -1;
        for (int i = 0; i < (int) project.tracks.size(); ++i)
        {
            if (project.tracks[(size_t) i].id == selectedTrack) from = i;
            if (project.tracks[(size_t) i].id == trackId)       to = i;
        }
        if (from < 0)
            from = to;
        for (int i = juce::jmin (from, to); i <= juce::jmax (from, to); ++i)
            if (! isTrackSelected (project.tracks[(size_t) i].id))
                selectedTracks.push_back (project.tracks[(size_t) i].id);
        selectedTrack = trackId;
        changed (false);
    }

    bool Studio::isTrackSelected (int trackId) const noexcept
    {
        return trackId != 0 && (trackId == selectedTrack || std::find (selectedTracks.begin(), selectedTracks.end(), trackId) != selectedTracks.end());
    }

    std::vector<int> Studio::getSelectedTracks() const
    {
        std::vector<int> ids;
        for (const auto& track : project.tracks)
            if (isTrackSelected (track.id))
                ids.push_back (track.id);
        return ids;
    }

    void Studio::moveTracks (const std::vector<int>& trackIds, int beforeIndex)
    {
        auto& tracks = project.tracks;
        beforeIndex = juce::jlimit (0, (int) tracks.size(), beforeIndex);
        // The one they go before (if it isn't one of them), else the end.
        int anchor = 0;
        for (int i = beforeIndex; i < (int) tracks.size() && anchor == 0; ++i)
            if (std::find (trackIds.begin(), trackIds.end(), tracks[(size_t) i].id) == trackIds.end())
                anchor = tracks[(size_t) i].id;
        std::vector<Track> moving, staying;
        for (auto& track : tracks)
            (std::find (trackIds.begin(), trackIds.end(), track.id) != trackIds.end() ? moving : staying).push_back (std::move (track));
        if (moving.empty())
        {
            tracks = std::move (staying);
            return;
        }
        const auto at = std::find_if (staying.begin(), staying.end(), [anchor] (const Track& t) { return t.id == anchor; });
        const auto offset = std::distance (staying.begin(), anchor == 0 ? staying.end() : at);
        staying.insert (staying.begin() + offset, std::make_move_iterator (moving.begin()), std::make_move_iterator (moving.end()));
        tracks = std::move (staying);
        changed();
    }

    int Studio::duplicateTrack (int trackId)
    {
        const auto* original = project.findTrack (trackId);
        if (original == nullptr)
            return 0;
        auto copy = *original;
        copy.id = project.nextTrackId++;
        copy.name = original->name + " copy";
        copy.armed = false;
        for (auto& part : copy.parts)
            part.id = 0;
        // The instrument and the effects as they are now (their settings from the running ones).
        if (auto* instrument = engine.getInstrument (trackId))
            instrument->getStateInformation (copy.instrument.state);
        for (int slot = 0; slot < (int) copy.inserts.size(); ++slot)
            if (auto* effect = engine.getEffect (trackId, slot))
                effect->getStateInformation (copy.inserts[(size_t) slot].state);

        if (copy.isChannel())
            engine.addChannel (copy.id, copy.isGroup());
        else if (copy.isAudio())
            engine.addAudioTrack (copy.id);
        else
        {
            juce::String problem;
            auto instance = host->create (copy.instrument, engine.getSampleRate(), engine.getBlockSize(), problem);
            if (instance == nullptr)
            {
                --project.nextTrackId;
                return 0;
            }
            engine.setInstrument (copy.id, std::move (instance));
        }
        for (int slot = 0; slot < (int) copy.inserts.size(); ++slot)
        {
            if (copy.inserts[(size_t) slot].isEmpty())
            {
                engine.insertEffect (copy.id, slot, nullptr);
                continue;
            }
            juce::String problem;
            engine.insertEffect (copy.id, slot, host->createEffect (copy.inserts[(size_t) slot], engine.getSampleRate(), engine.getBlockSize(), problem));
        }
        // Straight under the original.
        const auto it = std::find_if (project.tracks.begin(), project.tracks.end(), [trackId] (const Track& t) { return t.id == trackId; });
        const auto id = copy.id;
        project.tracks.insert (it + 1, std::move (copy));
        selectTrack (id);   // (changed() there)
        recordUndoState();   // (as adding a track: what's done to it next can be undone)
        return id;
    }

    void Studio::duplicateTracks (const std::vector<int>& trackIds)
    {
        std::vector<int> copies;
        for (const auto id : trackIds)
            if (const auto copy = duplicateTrack (id))
                copies.push_back (copy);
        if (copies.empty())
            return;
        selectedTracks = copies;
        selectedTrack = copies.back();
        changed (false);
    }

    void Studio::removeTracks (const std::vector<int>& trackIds)
    {
        for (const auto id : trackIds)
            removeTrack (id);
    }

    bool Studio::isTrackEmpty (int trackId) const
    {
        const auto* track = project.findTrack (trackId);
        if (track == nullptr || ! track->parts.empty())
            return false;
        for (const auto& lane : track->automation)
            if (! lane.points.empty())
                return false;
        return true;
    }

    std::vector<int> Studio::getEmptyTracks() const
    {
        std::vector<int> ids;
        for (const auto& track : project.tracks)
            if (! track.isChannel() && isTrackEmpty (track.id))
                ids.push_back (track.id);
        return ids;
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
        unwatch (engine.getInstrument (trackId));
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
            // A click on M is a touch: with W on while playing, it writes (and latches).
            const auto muteControl = control (AutomationLane::Target::mute);
            grabControl (trackId, muteControl);
            track->mute = on;
            if (writeValue (trackId, muteControl, on ? 1.0f : 0.0f))
            {
                releaseControl (trackId, muteControl);
                changed (false);
                return;
            }
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
            // Being written: the pass is the undo step.
            const auto written = writeValue (trackId, control (AutomationLane::Target::volume), volumeToTravel (track->volumeDb));
            changed (undoable && ! written);
        }
    }

    void Studio::setPan (int trackId, float pan, bool undoable)
    {
        if (auto* track = project.findTrack (trackId))
        {
            track->pan = juce::jlimit (-1.0f, 1.0f, pan);
            const auto written = writeValue (trackId, control (AutomationLane::Target::pan), panToTravel (track->pan));
            changed (undoable && ! written);
        }
    }

    void Studio::setArmed (int trackId)
    {
        if (const auto* t = project.findTrack (trackId); t != nullptr && t->isChannel())
            return;   // an FX or group channel records nothing
        for (auto& track : project.tracks)
            track.armed = track.id == trackId;
        changed (false);
    }

    void Studio::selectTrack (int trackId)
    {
        selectedTrack = trackId;
        selectedTracks.clear();
        if (trackId != 0)
            selectedTracks.push_back (trackId);
        // Selecting arms it (when not recording), so you play and record the track you're on.
        const auto* selected = project.findTrack (trackId);
        if (! engine.isRecording() && (selected == nullptr || ! selected->isChannel()))
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

    namespace
    {
        // A part cut at `rel` beats from its start: it keeps what's before, and the rest is returned.
        Part cutPart (Part& part, double rel, double bpm)
        {
            auto second = part;
            second.id = 0;
            second.start = part.start + rel;
            second.length = part.length - rel;
            second.notes.clear();
            std::vector<Note> first;
            for (const auto& note : part.notes)
            {
                if (note.start < rel)
                {
                    auto kept = note;
                    if (note.start + note.length > rel && note.start >= 0.0)
                    {
                        kept.length = rel - note.start;   // cut in two: the rest starts the second part
                        auto rest = note;
                        rest.start = 0.0;
                        rest.length = note.start + note.length - rel;
                        second.notes.push_back (rest);
                    }
                    first.push_back (kept);
                }
                else
                {
                    auto moved = note;
                    moved.start -= rel;
                    second.notes.push_back (moved);
                }
            }
            std::stable_sort (second.notes.begin(), second.notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
            part.notes = std::move (first);
            part.length = rel;
            if (part.isAudio())
            {
                const auto seconds = rel * 60.0 / bpm;
                second.audioOffset = part.audioOffset + seconds;
                second.fadeIn = 0.0;
                part.fadeOut = 0.0;
                part.fadeIn = juce::jmin (part.fadeIn, seconds);
                second.fadeOut = juce::jmin (second.fadeOut, second.length * 60.0 / bpm);
            }
            return second;
        }

        // The parts on a track after `index`'s (by start), in order.
        std::vector<int> partsAfter (const Track& track, int index)
        {
            const auto& from = track.parts[(size_t) index];
            std::vector<int> after;
            for (int i = 0; i < (int) track.parts.size(); ++i)
                if (i != index && (track.parts[(size_t) i].start > from.start
                                   || (juce::exactlyEqual (track.parts[(size_t) i].start, from.start) && i > index)))
                    after.push_back (i);
            std::stable_sort (after.begin(), after.end(), [&] (int a, int b) { return track.parts[(size_t) a].start < track.parts[(size_t) b].start; });
            return after;
        }

        constexpr double shortestPiece = 1.0 / 64.0;   // beats: no split closer to a part's edge than this
    }

    int Studio::splitPart (int trackId, int index, double atBeat)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || ! juce::isPositiveAndBelow (index, (int) track->parts.size()))
            return -1;
        auto& part = track->parts[(size_t) index];
        const auto rel = atBeat - part.start;
        if (rel < shortestPiece || rel > part.length - shortestPiece)
            return -1;
        track->parts.push_back (cutPart (part, rel, project.bpm));
        changed();
        return (int) track->parts.size() - 1;
    }

    void Studio::splitPartRepeatedly (int trackId, int index, double atBeat)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || ! juce::isPositiveAndBelow (index, (int) track->parts.size()))
            return;
        const auto piece = atBeat - track->parts[(size_t) index].start;
        if (piece < shortestPiece || piece > track->parts[(size_t) index].length - shortestPiece)
            return;
        auto current = index;
        while (track->parts[(size_t) current].length - piece >= shortestPiece)
        {
            track->parts.push_back (cutPart (track->parts[(size_t) current], piece, project.bpm));
            current = (int) track->parts.size() - 1;
        }
        changed();
    }

    void Studio::splitAtCursor (int onlyPartId)
    {
        const auto at = engine.getPosition();
        auto any = false;
        for (auto& track : project.tracks)
        {
            const auto count = (int) track.parts.size();   // (not the new halves)
            for (int i = 0; i < count; ++i)
            {
                auto& part = track.parts[(size_t) i];
                if (onlyPartId != 0 && part.id != onlyPartId)
                    continue;
                const auto rel = at - part.start;
                if (rel < shortestPiece || rel > part.length - shortestPiece)
                    continue;
                track.parts.push_back (cutPart (part, rel, project.bpm));
                any = true;
            }
        }
        if (any)
            changed();
    }

    bool Studio::glueParts (int trackId, int index, bool toAllFollowing)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || ! juce::isPositiveAndBelow (index, (int) track->parts.size()))
            return false;
        auto after = partsAfter (*track, index);
        if (after.empty())
            return false;
        if (! toAllFollowing)
            after.resize (1);
        auto glued = track->parts[(size_t) index];
        if (glued.isAudio())
        {
            // Only clips cut from one recording, end to end (a split glued back).
            for (const auto i : after)
            {
                const auto& next = track->parts[(size_t) i];
                const auto secondsIn = glued.audioOffset + glued.length * 60.0 / project.bpm;
                if (next.audioFile != glued.audioFile || std::abs (next.start - glued.end()) > 1.0e-6
                    || std::abs (next.audioOffset - secondsIn) > 1.0e-4)
                    return false;
                glued.length = next.end() - glued.start;
                glued.fadeOut = next.fadeOut;
            }
        }
        else
        {
            // What was heard of each, in its place.
            std::vector<Note> notes;
            for (const auto& note : glued.notes)
                if (note.start >= 0.0 && note.start < glued.length)
                    notes.push_back ({ note.start, juce::jmin (note.length, glued.length - note.start), note.pitch, note.velocity });
            auto end = glued.end();
            for (const auto i : after)
            {
                const auto& next = track->parts[(size_t) i];
                for (const auto& note : next.notes)
                    if (note.start >= 0.0 && note.start < next.length)
                        notes.push_back ({ next.start - glued.start + note.start, juce::jmin (note.length, next.length - note.start), note.pitch, note.velocity });
                end = juce::jmax (end, next.end());
            }
            std::stable_sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
            glued.notes = std::move (notes);
            glued.length = end - glued.start;
        }
        track->parts[(size_t) index] = glued;
        std::sort (after.begin(), after.end(), std::greater<>());
        for (const auto i : after)
            track->parts.erase (track->parts.begin() + i);
        changed();
        return true;
    }

    void Studio::setPartMuted (int trackId, int index, bool muted)
    {
        if (auto* track = project.findTrack (trackId); track != nullptr && juce::isPositiveAndBelow (index, (int) track->parts.size())
                                                        && track->parts[(size_t) index].muted != muted)
        {
            track->parts[(size_t) index].muted = muted;
            changed();
        }
    }

    void Studio::setPartsMuted (const std::vector<int>& partIds, bool muted)
    {
        auto any = false;
        for (auto& track : project.tracks)
            for (auto& part : track.parts)
                if (std::find (partIds.begin(), partIds.end(), part.id) != partIds.end() && part.muted != muted)
                {
                    part.muted = muted;
                    any = true;
                }
        if (any)
            changed();
    }

    void Studio::deletePartsFrom (int trackId, int index)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || ! juce::isPositiveAndBelow (index, (int) track->parts.size()))
            return;
        auto doomed = partsAfter (*track, index);
        doomed.push_back (index);
        std::sort (doomed.begin(), doomed.end(), std::greater<>());
        for (const auto i : doomed)
            track->parts.erase (track->parts.begin() + i);
        changed();
    }

    namespace
    {
        constexpr double edge = 1.0e-9;

        // Every part on a track that `beat` is inside is cut there.
        void splitTrackAt (Track& track, double beat, double bpm)
        {
            const auto count = (int) track.parts.size();
            for (int i = 0; i < count; ++i)
            {
                auto& part = track.parts[(size_t) i];
                if (beat > part.start + edge && beat < part.end() - edge)
                    track.parts.push_back (cutPart (part, beat - part.start, bpm));
            }
        }

        bool inside (const Part& part, double start, double end)
        {
            return part.start >= start - edge && part.end() <= end + edge;
        }

        // What of a part lies between two beats (none if nothing).
        std::optional<Part> slice (Part part, double start, double end, double bpm)
        {
            if (part.end() <= start + edge || part.start >= end - edge)
                return std::nullopt;
            if (start > part.start + edge)
                part = cutPart (part, start - part.start, bpm);
            if (end < part.end() - edge)
                cutPart (part, end - part.start, bpm);   // (keeps the first half)
            part.id = 0;
            return part;
        }

        // The parts that start at or after a beat move by `delta`.
        void shiftFrom (Track& track, double beat, double delta)
        {
            for (auto& part : track.parts)
                if (part.start >= beat - edge)
                    part.start = juce::jmax (0.0, part.start + delta);
        }

        // The tracks below `firstTrackId`, in the song's order, `count` of them (missing ones as nullptr).
        std::vector<Track*> tracksFrom (Project& project, int firstTrackId, size_t count)
        {
            std::vector<Track*> found;
            auto started = false;
            for (auto& track : project.tracks)
            {
                started = started || track.id == firstTrackId;
                if (started && found.size() < count)
                    found.push_back (&track);
            }
            found.resize (count, nullptr);
            return found;
        }
    }

    void Studio::splitRange (const Range& range)
    {
        if (range.isEmpty())
            return;
        for (const auto id : range.trackIds)
            if (auto* track = project.findTrack (id))
            {
                splitTrackAt (*track, range.start, project.bpm);
                splitTrackAt (*track, range.end, project.bpm);
            }
        changed();
    }

    void Studio::cropRange (const Range& range)
    {
        if (range.isEmpty())
            return;
        for (const auto id : range.trackIds)
            if (auto* track = project.findTrack (id))
                for (auto& part : track->parts)
                {
                    const auto crosses = (range.start > part.start + edge && range.start < part.end() - edge)
                                      || (range.end > part.start + edge && range.end < part.end() - edge);
                    if (crosses)
                        if (auto cropped = slice (part, range.start, range.end, project.bpm))
                        {
                            const auto id = part.id;
                            part = *cropped;
                            part.id = id;
                        }
                }
        changed();
    }

    void Studio::deleteRange (const Range& range, bool closeGap)
    {
        if (range.isEmpty())
            return;
        for (const auto id : range.trackIds)
            if (auto* track = project.findTrack (id))
            {
                splitTrackAt (*track, range.start, project.bpm);
                splitTrackAt (*track, range.end, project.bpm);
                track->parts.erase (std::remove_if (track->parts.begin(), track->parts.end(),
                                                    [&] (const Part& part) { return inside (part, range.start, range.end); }),
                                    track->parts.end());
                if (closeGap)
                    shiftFrom (*track, range.end, -range.length());
            }
        changed();
    }

    Studio::RangeClip Studio::copyRange (const Range& range) const
    {
        RangeClip clip;
        if (range.isEmpty())
            return clip;
        clip.length = range.length();
        clip.origin = range.start;
        clip.originTrack = range.trackIds.front();
        for (const auto id : range.trackIds)
        {
            auto& slices = clip.tracks.emplace_back();
            if (const auto* track = project.findTrack (id))
                for (const auto& part : track->parts)
                    if (auto piece = slice (part, range.start, range.end, project.bpm))
                    {
                        piece->start -= range.start;
                        slices.push_back (*piece);
                    }
        }
        return clip;
    }

    Studio::RangeClip Studio::cutRange (const Range& range, bool closeGap)
    {
        auto clip = copyRange (range);
        deleteRange (range, closeGap);
        return clip;
    }

    void Studio::insertSilence (const Range& range)
    {
        if (range.isEmpty())
            return;
        for (const auto id : range.trackIds)
            if (auto* track = project.findTrack (id))
            {
                splitTrackAt (*track, range.start, project.bpm);
                shiftFrom (*track, range.start, range.length());
            }
        changed();
    }

    void Studio::pasteRange (const RangeClip& clip, double atBeat, int firstTrackId, bool insertTime)
    {
        if (clip.isEmpty())
            return;
        atBeat = juce::jmax (0.0, atBeat);
        const auto targets = tracksFrom (project, firstTrackId, clip.tracks.size());
        for (size_t t = 0; t < clip.tracks.size(); ++t)
        {
            auto* track = targets[t];
            if (track == nullptr || track->isChannel())
                continue;
            if (insertTime)
            {
                splitTrackAt (*track, atBeat, project.bpm);
                shiftFrom (*track, atBeat, clip.length);
            }
            for (auto part : clip.tracks[t])
                if (part.isAudio() == track->isAudio())
                {
                    part.start += atBeat;
                    part.id = 0;
                    track->parts.push_back (part);
                }
        }
        changed();
    }

    void Studio::moveRange (const Range& range, double newStart, int newFirstTrackId, bool copy)
    {
        if (range.isEmpty())
            return;
        const auto clip = copyRange (range);
        if (! copy)
            for (const auto id : range.trackIds)
                if (auto* track = project.findTrack (id))
                {
                    splitTrackAt (*track, range.start, project.bpm);
                    splitTrackAt (*track, range.end, project.bpm);
                    track->parts.erase (std::remove_if (track->parts.begin(), track->parts.end(),
                                                        [&] (const Part& part) { return inside (part, range.start, range.end); }),
                                        track->parts.end());
                }
        pasteRange (clip, newStart, newFirstTrackId, false);   // (one undo step: pasteRange's)
    }

    void Studio::duplicateRange (const Range& range)
    {
        if (! range.isEmpty())
            pasteRange (copyRange (range), range.end, range.trackIds.front(), false);
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
    bool Studio::isAudioFile (const juce::File& file)
    {
        return file.hasFileExtension ("wav;wave;aif;aiff;flac;ogg;mp3");
    }

    int Studio::importAudio (const juce::Array<juce::File>& files, int trackId, double atBeat, juce::String& error)
    {
        auto folder = getAudioFolder();
        folder.createDirectory();
        juce::StringArray problems;
        std::vector<Part> clips;
        auto stereo = false;
        auto at = juce::jmax (0.0, atBeat);
        for (const auto& file : files)
        {
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
            if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
            {
                problems.add (file.getFileName() + ": not an audio file PR7-S can read (WAV, AIFF, FLAC, Ogg or MP3)");
                continue;
            }
            // A name of its own in the folder ("Riff.wav", "Riff 2.wav"...).
            const auto plain = file.hasFileExtension ("wav;wave;aif;aiff");
            const auto base = juce::File::createLegalFileName (file.getFileNameWithoutExtension());
            const auto extension = plain ? file.getFileExtension() : juce::String (".wav");
            auto copy = folder.getChildFile (base + extension);
            for (int n = 2; copy.exists(); ++n)
                copy = folder.getChildFile (base + " " + juce::String (n) + extension);
            auto ok = false;
            if (plain)
                ok = file.copyFileTo (copy);
            else if (auto stream = std::unique_ptr<juce::OutputStream> (copy.createOutputStream()))
            {
                juce::WavAudioFormat wav;
                const auto channels = (int) juce::jlimit (1u, 2u, reader->numChannels);
                if (auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (reader->sampleRate)
                                                                                               .withNumChannels (channels)
                                                                                               .withBitsPerSample (24)))
                    ok = writer->writeFromAudioReader (*reader, 0, reader->lengthInSamples);
            }
            if (! ok)
            {
                copy.deleteFile();
                problems.add (file.getFileName() + ": couldn't copy it into " + folder.getFullPathName());
                continue;
            }
            if (clips.empty())
                stereo = reader->numChannels > 1;
            Part clip;
            clip.name = file.getFileNameWithoutExtension();
            clip.audioFile = copy.getFileName();
            clip.start = at;
            clip.length = ((double) reader->lengthInSamples / reader->sampleRate) / project.secondsPerBeat();
            at += clip.length;
            clips.push_back (clip);
        }
        error = problems.joinIntoString ("\n");
        if (clips.empty())
            return 0;
        const auto* target = project.findTrack (trackId);
        if (target == nullptr || ! target->isAudio())
            trackId = addAudioTrack (stereo);
        if (auto* track = project.findTrack (trackId))
            for (auto& clip : clips)
                track->parts.push_back (clip);
        changed();
        return trackId;
    }

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
        unwatch (engine.getEffect (busId, slot));
        if (onEffectGoing)
            if (auto* effect = engine.getEffect (busId, slot))
                onEffectGoing (effect);
    }

    void Studio::removeAllEffects()
    {
        unwatchAll();   // (a new or opened song: every plugin goes)
        if (onEffectGoing)
            onEffectGoing (nullptr);
        while (engine.getNumEffects (Engine::masterBus) > 0)
            engine.removeEffect (Engine::masterBus, 0);
    }

    bool Studio::addInsert (int busId, const juce::PluginDescription& description, juce::String* error, int slot)
    {
        const auto* inserts = insertsOf (busId);
        if (inserts == nullptr || (slot < 0 ? (int) inserts->size() >= Project::maxInserts
                                            : slot >= Project::maxInserts || (slot < (int) inserts->size() && ! (*inserts)[(size_t) slot].isEmpty())))
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
        return addInsert (busId, insert, std::move (instance), slot);
    }

    bool Studio::addInsert (int busId, const Insert& insert, std::unique_ptr<juce::AudioPluginInstance> instance, int slot)
    {
        auto* inserts = insertsOf (busId);
        if (inserts == nullptr)
            return false;
        if (slot < 0)
            slot = (int) inserts->size();
        if (slot >= Project::maxInserts || (slot < (int) inserts->size() && ! (*inserts)[(size_t) slot].isEmpty()))
            return false;
        // Empty slots up to it, if it's past the last one used.
        while ((int) inserts->size() <= slot)
        {
            inserts->push_back ({});
            engine.insertEffect (busId, (int) inserts->size() - 1, nullptr);
        }
        (*inserts)[(size_t) slot] = insert;
        engine.removeEffect (busId, slot);
        engine.insertEffect (busId, slot, std::move (instance));
        changed (false);
        return true;
    }

    void Studio::setLastPreFaderSlot (int busId, int slot)
    {
        const auto count = juce::jlimit (0, Project::maxInserts, slot + 1);
        if (busId == Engine::masterBus)
            project.masterPreFaderSlots = count;
        else if (auto* track = project.findTrack (busId))
            track->preFaderSlots = count;
        else
            return;
        changed (false);
    }

    int Studio::getPreFaderSlots (int busId) const
    {
        if (busId == Engine::masterBus)
            return project.masterPreFaderSlots;
        const auto* track = project.findTrack (busId);
        return track != nullptr ? track->preFaderSlots : Project::maxInserts;
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
        engine.insertEffect (busId, slot, nullptr);   // the slot stays, empty
        (*inserts)[(size_t) slot] = {};
        insertGone (busId, slot, false);
        // No empty slots after the last effect.
        while (! inserts->empty() && inserts->back().isEmpty())
        {
            engine.removeEffect (busId, (int) inserts->size() - 1);
            inserts->pop_back();
        }
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
        if (! on)   // W off ends its passes (as the manual has it)
            for (size_t i = passes.size(); i-- > 0;)
                if (passes[i].busId == busId)
                    punchOut (i, engine.getPosition());
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
        if (! on)
            punchOutAll();
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
            // Each send's level (to the FX channel it's on).
            const auto* track = project.findTrack (busId);
            for (int s = 0; s < (int) track->sends.size(); ++s)
                if (const auto* fx = project.findTrack (track->sends[(size_t) s].target))
                    controls.push_back ({ T::send, s, 0, "Send " + juce::String (s + 1) + ": " + fx->name, {} });
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
            case AutomationLane::Target::send:
            {
                const auto* track = project.findTrack (busId);
                const auto* fx = track != nullptr && juce::isPositiveAndBelow (lane.slot, (int) track->sends.size())
                                   ? project.findTrack (track->sends[(size_t) lane.slot].target) : nullptr;
                return "Send " + juce::String (lane.slot + 1) + (fx != nullptr ? ": " + fx->name : juce::String());
            }
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
            case AutomationLane::Target::send:
                return track != nullptr && juce::isPositiveAndBelow (lane.slot, (int) track->sends.size())
                         ? volumeToTravel (track->sends[(size_t) lane.slot].levelDb) : 0.0f;
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
        const auto written = writeValue (Engine::masterBus, control (AutomationLane::Target::volume), volumeToTravel (project.masterVolumeDb));
        changed (undoable && ! written);
    }

    //==========================================================================
    // Hears a plugin's knobs turned by hand (in its window) and passes them to
    // the studio's thread, where they're written (if a pass is going).
    class Studio::KnobWatcher final : public juce::AudioProcessorListener
    {
    public:
        KnobWatcher (Studio& s, juce::AudioProcessor& p, int bus, AutomationLane::Target target, int slot)
            : studio (s), alive (s.alive), plugin (p), busId (bus), kind (target), slotIndex (slot)
        {
            plugin.addListener (this);
        }
        ~KnobWatcher() override { plugin.removeListener (this); }

        void audioProcessorParameterChanged (juce::AudioProcessor*, int index, float value) override
        {
            post ([value] (Studio& s, int bus, const AutomationLane& knob) { s.setKnob (bus, knob, value); }, index);
        }
        void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int index) override
        {
            post ([] (Studio& s, int bus, const AutomationLane& knob) { s.grabControl (bus, knob); }, index);
        }
        void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int index) override
        {
            post ([] (Studio& s, int bus, const AutomationLane& knob) { s.releaseControl (bus, knob); }, index);
        }
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}

    private:
        template <typename Action>
        void post (Action action, int index)
        {
            if (juce::MessageManager::existsAndIsCurrentThread())   // (a plugin's window: here and now)
            {
                action (studio, busId, Studio::control (kind, slotIndex, index));
                return;
            }
            juce::MessageManager::callAsync ([weak = std::weak_ptr<bool> (alive), studioPtr = &studio, bus = busId,
                                              knob = Studio::control (kind, slotIndex, index), action]
            {
                if (weak.lock() != nullptr)
                    action (*studioPtr, bus, knob);
            });
        }

        Studio& studio;
        std::shared_ptr<bool> alive;
        juce::AudioProcessor& plugin;
        const int busId;
        const AutomationLane::Target kind;
        const int slotIndex;
    };

    void Studio::watchPlugins()
    {
        const auto watch = [this] (juce::AudioProcessor* plugin, int busId, AutomationLane::Target target, int slot)
        {
            if (plugin != nullptr && watchers.find (plugin) == watchers.end())
                watchers[plugin] = std::make_unique<KnobWatcher> (*this, *plugin, busId, target, slot);
        };
        for (const auto& track : project.tracks)
        {
            watch (engine.getInstrument (track.id), track.id, AutomationLane::Target::instrument, 0);
            for (int slot = 0; slot < engine.getNumEffects (track.id); ++slot)
                watch (engine.getEffect (track.id, slot), track.id, AutomationLane::Target::insert, slot);
        }
        for (int slot = 0; slot < engine.getNumEffects (Engine::masterBus); ++slot)
            watch (engine.getEffect (Engine::masterBus, slot), Engine::masterBus, AutomationLane::Target::insert, slot);
    }

    void Studio::unwatch (juce::AudioProcessor* plugin)
    {
        if (plugin != nullptr)
            watchers.erase (plugin);   // (before it's deleted: the engine says so, however it goes)
    }

    void Studio::unwatchAll()
    {
        watchers.clear();
    }

    AutomationLane Studio::control (AutomationLane::Target target, int slot, int parameter)
    {
        AutomationLane lane;
        lane.target = target;
        lane.slot = slot;
        lane.parameter = parameter;
        return lane;
    }

    void Studio::grabControl (int busId, const AutomationLane& which)
    {
        if (! engine.isPlaying() || ! getAutomationWrite (busId))
            return;
        for (auto& pass : passes)
            if (pass.busId == busId && pass.control.sameControl (which))
            {
                pass.released = false;   // touched again (an on/off switch still latched)
                return;
            }
        WritePass pass;
        pass.busId = busId;
        pass.control = which;
        pass.start = pass.lastBeat = engine.getPosition();
        pass.cycleCount = engine.getCycleCount();
        pass.before = getControlValue (busId, which);
        passes.push_back (pass);
        engine.setHeld (busId, which, true);   // the hand is heard, not the lane
        engine.update (project);
        sendChangeMessage();                   // (W shows it's writing)
    }

    bool Studio::writeValue (int busId, const AutomationLane& which, float value)
    {
        for (size_t i = 0; i < passes.size(); ++i)
        {
            auto& pass = passes[i];
            if (pass.busId != busId || ! pass.control.sameControl (which))
                continue;
            const auto beat = engine.getPosition();
            if (pass.cycleCount != engine.getCycleCount())
            {
                // The cycle came round: the pass ends at the cycle's end (as the manual has it).
                punchOut (i, juce::jmax (pass.lastBeat, project.loopEnd));
                return false;
            }
            if (beat + 1.0e-6 < pass.lastBeat)
            {
                punchOut (i, pass.lastBeat);   // it jumped back
                return false;
            }
            pass.lastBeat = beat;
            pass.points.push_back ({ beat, juce::jlimit (0.0f, 1.0f, value) });
            return true;
        }
        return false;
    }

    void Studio::releaseControl (int busId, const AutomationLane& which)
    {
        for (size_t i = 0; i < passes.size(); ++i)
            if (passes[i].busId == busId && passes[i].control.sameControl (which))
            {
                if (which.isStep())
                    passes[i].released = true;   // Auto-Latch: it keeps its last value until playing stops
                else
                    punchOut (i, engine.getPosition());   // Touch
                return;
            }
    }

    void Studio::setKnob (int busId, const AutomationLane& which, float value)
    {
        writeValue (busId, which, value);
    }

    bool Studio::isWritingAutomation (int busId) const
    {
        return std::any_of (passes.begin(), passes.end(), [busId] (const WritePass& p) { return p.busId == busId; });
    }

    void Studio::setControlValue (int busId, const AutomationLane& which, float value)
    {
        auto* track = project.findTrack (busId);
        switch (which.target)
        {
            case AutomationLane::Target::volume:
                if (busId == Engine::masterBus)
                    project.masterVolumeDb = travelToVolume (value);
                else if (track != nullptr)
                    track->volumeDb = travelToVolume (value);
                break;
            case AutomationLane::Target::pan:  if (track != nullptr) track->pan = travelToPan (value); break;
            case AutomationLane::Target::mute: if (track != nullptr) track->mute = value >= 0.5f; break;
            case AutomationLane::Target::send:
                if (track != nullptr && juce::isPositiveAndBelow (which.slot, (int) track->sends.size()))
                    track->sends[(size_t) which.slot].levelDb = travelToVolume (value);
                break;
            case AutomationLane::Target::instrument:
            case AutomationLane::Target::insert:
                if (auto* plugin = pluginFor (busId, which))
                    if (which.parameter < plugin->getParameters().size())
                        plugin->getParameters()[which.parameter]->setValueNotifyingHost (value);
                break;
        }
    }

    void Studio::punchOut (size_t index, double endBeat)
    {
        auto pass = passes[index];
        passes.erase (passes.begin() + (std::ptrdiff_t) index);
        engine.setHeld (pass.busId, pass.control, false);
        auto* lanes = lanesOf (pass.busId);
        if (lanes == nullptr)
            return;
        endBeat = juce::jmax (endBeat, pass.start);

        // Its lane (a new one if the control had none: every pass is kept, to open later).
        auto lane = std::find_if (lanes->begin(), lanes->end(), [&] (const AutomationLane& l) { return l.sameControl (pass.control); });
        if (lane == lanes->end())
        {
            auto added = pass.control;
            added.shown = true;
            if (auto* plugin = pluginFor (pass.busId, pass.control); plugin != nullptr && pass.control.parameter < plugin->getParameters().size())
                added.parameterName = plugin->getParameters()[pass.control.parameter]->getName (64);
            lanes->push_back (added);
            lane = lanes->end() - 1;
        }
        const auto old = *lane;
        const auto oldAt = [&] (double beat) { return old.points.empty() ? pass.before : *old.valueAt (beat); };

        // The written values, thinned: a point that's on the line between its
        // neighbours (to within a hair) adds nothing.
        auto written = pass.points;
        if (written.empty() || written.front().beat > pass.start)
            written.insert (written.begin(), { pass.start, written.empty() ? pass.before : written.front().value });
        if (pass.control.isStep() || written.back().beat < endBeat)
            written.push_back ({ endBeat, written.back().value });   // held to the end (a latched switch, or a still hand)
        std::vector<AutomationPoint> thin;
        for (size_t i = 0; i < written.size(); ++i)
        {
            if (i > 0 && i + 1 < written.size())
            {
                const auto& a = thin.back();
                const auto& b = written[i + 1];
                const auto along = b.beat > a.beat ? (written[i].beat - a.beat) / (b.beat - a.beat) : 0.0;
                const auto line = a.value + (b.value - a.value) * (float) along;
                if (std::abs (written[i].value - line) < 0.002f && ! pass.control.isStep())
                    continue;
                if (pass.control.isStep() && std::abs (written[i].value - a.value) < 1.0e-6f)
                    continue;
            }
            thin.push_back (written[i]);
        }

        // In place of what was there from the start to the end (and the return).
        const auto returnBeats = pass.control.isStep() ? 0.0 : returnSeconds / project.secondsPerBeat();
        const auto backAt = endBeat + returnBeats;
        std::vector<AutomationPoint> points;
        for (const auto& p : old.points)
            if (p.beat < pass.start)
                points.push_back (p);
        points.push_back ({ pass.start, oldAt (pass.start) });   // up to the start as it was, then a jump to the hand's value
        for (const auto& p : thin)
            points.push_back (p);
        points.push_back ({ backAt, oldAt (backAt) });            // and back to what was there (Touch's return)
        for (const auto& p : old.points)
            if (p.beat > backAt)
                points.push_back (p);
        lane->points = std::move (points);

        // The control as it was set before (the lane carries the pass now).
        setControlValue (pass.busId, pass.control, pass.before);
        if (pass.busId == Engine::masterBus)
            project.masterRead = true;
        else if (auto* track = project.findTrack (pass.busId))
            track->automationRead = true;
        changed();   // one undo step
    }

    void Studio::punchOutAll()
    {
        while (! passes.empty())
            punchOut (passes.size() - 1, juce::jmax (passes.back().lastBeat, engine.isPlaying() ? engine.getPosition() : passes.back().lastBeat));
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
        punchOutAll();   // writing automation stops with playing
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
        punchOutAll();   // a jump ends a pass of writing automation
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
        const auto rate = engine.getSampleRate();
        const auto spb = project.secondsPerBeat();
        Part part;
        part.start = (double) startSample * project.bpm / 60.0 / rate;
        part.audioFile = name;
        part.audioOffset = latency;
        part.length = (data->seconds() - latency) / spb;
        // In a cycle, the file holds every pass one after another: the clip
        // plays the last complete one (as the manual's Replace mode keeps the
        // last take), or what there is of the first if it wasn't finished.
        const auto toSamples = [rate, &project = project] (double beats) { return (juce::int64) std::llround (beats * 60.0 / project.bpm * rate); };
        const auto loopStart = toSamples (project.loopStart), loopEnd = toSamples (project.loopEnd);
        if (project.loopOn && startSample < loopEnd && loopEnd > loopStart)
        {
            const auto played = (juce::int64) data->sound.getNumSamples() - (juce::int64) std::llround (latency * rate);
            const auto firstPass = loopEnd - startSample, pass = loopEnd - loopStart;
            const auto laps = played > firstPass ? (played - firstPass) / pass : 0;   // complete passes after the first
            if (laps >= 1)
            {
                part.start = project.loopStart;
                part.audioOffset = latency + (double) (firstPass + (laps - 1) * pass) / rate;
                part.length = project.loopEnd - project.loopStart;
            }
            else
            {
                part.length = juce::jmin (part.length, (double) firstPass / rate / spb);
            }
        }
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
        // Writing automation: the position going back (the cycle came round)
        // ends a pass where it got to; a latched switch's pass moves on with
        // the song (it has no new values to say so).
        for (size_t i = passes.size(); i-- > 0;)
        {
            const auto now = engine.getPosition();
            if (passes[i].cycleCount != engine.getCycleCount())
                punchOut (i, juce::jmax (passes[i].lastBeat, project.loopEnd));
            else if (now + 1.0e-6 < passes[i].lastBeat)
                punchOut (i, passes[i].lastBeat);
            else
                passes[i].lastBeat = now;
        }

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
        punchOutAll();   // (writing automation ends with the song)
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
        selectedTracks.clear();
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

    int Studio::importMidi (const juce::File& source, bool newSong, double atBeat, juce::String& error)
    {
        const auto read = readMidiFile (source, error);
        return read ? importMidi (*read, newSong, atBeat) : 0;
    }

    int Studio::importMidi (const MidiImport& midi, bool newSong, double atBeat)
    {
        if (newSong)
        {
            newProject();
            // The new song's one empty track makes way for the file's.
            if (project.tracks.size() == 1 && project.tracks.front().parts.empty())
                removeTrack (project.tracks.front().id);
            setTempo (midi.bpm);
            setSignature (midi.numerator, midi.denominator);
            atBeat = 0.0;
        }
        const auto bar = project.beatsPerBar();
        auto added = 0;
        for (const auto& imported : midi.tracks)
        {
            const auto id = addTrack ({});
            if (id == 0)
                continue;
            renameTrack (id, imported.name);
            // One part over the bars its notes are in.
            auto first = std::numeric_limits<double>::max(), last = 0.0;
            for (const auto& note : imported.notes)
            {
                first = juce::jmin (first, atBeat + note.start);
                last = juce::jmax (last, atBeat + note.start + note.length);
            }
            Part part;
            part.start = juce::jmax (0.0, std::floor (first / bar + 1.0e-9) * bar);
            part.length = juce::jmax (bar, std::ceil (last / bar - 1.0e-9) * bar - part.start);
            part.name = imported.name;
            for (auto note : imported.notes)
            {
                note.start += atBeat - part.start;
                part.notes.push_back (note);
            }
            addPart (id, part);
            ++added;
        }
        if (newSong)
        {
            // The import is where the new song starts: nothing to undo before it.
            undoStates.clear();
            undoIndex = -1;
            recordUndoState();
        }
        return added;
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
        {
            const auto bits = options.bitsPerSample == 16 ? 16 : options.bitsPerSample == 32 ? 32 : 24;
            job->writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate)
                                                                                                        .withNumChannels (2)
                                                                                                        .withBitsPerSample (bits)
                                                                                                        .withSampleFormat (bits == 32 ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint
                                                                                                                                      : juce::AudioFormatWriterOptions::SampleFormat::integral));
        }
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
        if (options.prepareSong)
            options.prepareSong (playing);
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
        engine.settleMix();   // the levels set from the first sample (no glide from what was playing before)
        engine.setNonRealtime (true);

        job->total = (juce::int64) std::llround ((options.end - options.start) * 60.0 / project.bpm * rate);
        job->maxTail = (juce::int64) std::llround (juce::jmax (0.0, options.tailSeconds) * rate);
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
        const auto rate = engine.getSampleRate();
        const auto reportEvery = (juce::int64) (rate * 0.5);
        juce::int64 sinceReport = 0;
        const auto report = [&] (int length, double progress)
        {
            sinceReport += length;
            if (! keepGoing || sinceReport < reportEvery)
                return true;
            sinceReport = 0;
            return keepGoing (progress);
        };
        for (juce::int64 done = 0; done < job.total;)
        {
            const auto length = (int) juce::jmin ((juce::int64) blockSize, job.total - done);
            engine.render (block, length);
            if (! job.writer->writeFromAudioSampleBuffer (block, 0, length))
                return false;
            done += length;
            if (! report (length, 0.95 * (double) done / (double) job.total))
                return false;
        }

        // The ring-out: on until it's been silent (below -100 dB) for a
        // quarter of a second, at most maxTail; written up to its last sound.
        if (job.maxTail > 0)
        {
            juce::AudioBuffer<float> tail (2, (int) job.maxTail);
            const auto quietFor = (juce::int64) (rate * 0.25);
            juce::int64 done = 0, lastSound = -1;
            while (done < job.maxTail && done - lastSound <= quietFor)
            {
                const auto length = (int) juce::jmin ((juce::int64) blockSize, job.maxTail - done);
                juce::AudioBuffer<float> into (tail.getArrayOfWritePointers(), 2, (int) done, length);
                engine.render (into, length);
                for (int i = length - 1; i >= 0; --i)
                    if (std::abs (into.getSample (0, i)) > 1.0e-5f || std::abs (into.getSample (1, i)) > 1.0e-5f)
                    {
                        lastSound = done + i;
                        break;
                    }
                done += length;
                if (! report (length, 0.95 + 0.05 * (double) done / (double) job.maxTail))
                    return false;
            }
            if (lastSound >= 0 && ! job.writer->writeFromAudioSampleBuffer (tail, 0, (int) lastSound + 1))
                return false;
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

    std::vector<int> Studio::renderInPlace (const std::vector<RenderSource>& sources, const RenderOptions& options, juce::String& error)
    {
        using P = RenderOptions::Processing;
        std::vector<int> made;
        juce::StringArray problems;
        const auto folder = getAudioFolder();
        folder.createDirectory();
        const auto tailSeconds = options.tail == RenderOptions::Tail::off ? 0.0
                               : options.tail == RenderOptions::Tail::bars ? options.tailSize * project.beatsPerBar() * project.secondsPerBeat()
                                                                           : options.tailSize;
        for (const auto& source : sources)
        {
            const auto* track = project.findTrack (source.trackId);
            if (track == nullptr || track->isChannel() || source.end <= source.start)
                continue;
            // The stretches to render, from the parts in it (cut at its edges).
            std::vector<std::pair<double, double>> spans;
            for (const auto& part : track->parts)
                if (! part.muted)
                    if (const auto piece = slice (part, source.start, source.end, project.bpm))
                        spans.push_back ({ piece->start, piece->end() });
            std::sort (spans.begin(), spans.end());
            if (spans.empty())
            {
                problems.add (track->name + ": nothing to render there");
                continue;
            }
            if (options.mode == RenderOptions::Mode::one)
            {
                auto end = spans.front().second;
                for (const auto& span : spans)
                    end = juce::jmax (end, span.second);
                spans = { { spans.front().first, end } };
            }
            else if (options.mode == RenderOptions::Mode::blocks)
            {
                std::vector<std::pair<double, double>> blocks;
                for (const auto& span : spans)
                    if (! blocks.empty() && span.first <= blocks.back().second + 1.0e-9)
                        blocks.back().second = juce::jmax (blocks.back().second, span.second);
                    else
                        blocks.push_back (span);
                spans = std::move (blocks);
            }

            // Each stretch through the export, the song made to play only what's asked.
            const auto sourceId = track->id;
            const auto name = track->name;
            const auto processing = options.processing;
            std::vector<Part> clips;
            for (const auto& [from, to] : spans)
            {
                const auto base = juce::File::createLegalFileName (name) + " Render ";
                auto file = folder.getChildFile (base + "01.wav");
                for (int n = 2; file.exists(); ++n)
                    file = folder.getChildFile (base + juce::String (n).paddedLeft ('0', 2) + ".wav");
                ExportOptions render;
                render.start = from;
                render.end = to;
                render.bitsPerSample = options.bitsPerSample;
                render.tailSeconds = tailSeconds;
                render.prepareSong = [sourceId, from = from, to = to, processing, bpm = project.bpm] (Project& song)
                {
                    for (auto& t : song.tracks)
                    {
                        t.solo = false;
                        if (t.id != sourceId)
                        {
                            if (! t.isChannel())
                                t.parts.clear();   // the other tracks: silent (FX and group channels still take what's sent or routed)
                            continue;
                        }
                        std::vector<Part> kept;
                        for (const auto& part : t.parts)
                            if (auto piece = slice (part, from, to, bpm))
                                kept.push_back (*piece);
                        t.parts = std::move (kept);
                        t.mute = false;
                        if (processing == P::dry)
                        {
                            for (auto& insert : t.inserts)
                                insert.bypass = true;
                            t.sends.clear();
                            t.output = 0;
                            t.automationRead = false;
                        }
                        if (processing == P::dry || processing == P::channelSettings)
                        {
                            // The fader and pan aren't rendered (the new track gets them), nor their automation.
                            t.volumeDb = 0.0f;
                            t.pan = 0.0f;
                            t.automation.erase (std::remove_if (t.automation.begin(), t.automation.end(), [] (const AutomationLane& lane)
                            {
                                using Target = AutomationLane::Target;
                                return lane.target == Target::volume || lane.target == Target::pan || lane.target == Target::mute;
                            }), t.automation.end());
                        }
                    }
                    if (processing != P::completeWithMaster)
                    {
                        for (auto& insert : song.masterInserts)
                            insert.bypass = true;
                        song.masterVolumeDb = 0.0f;
                        song.masterRead = false;
                    }
                };
                juce::String problem;
                if (! exportMix (file, render, problem))
                {
                    problems.add (name + ": " + problem);
                    continue;
                }
                audioCache.erase (file.getFileName());
                const auto data = getAudio (file.getFileName());
                if (data == nullptr)
                {
                    problems.add (name + ": couldn't read back " + file.getFileName());
                    continue;
                }
                Part clip;
                clip.name = name + " (rendered)";
                clip.audioFile = file.getFileName();
                clip.start = from;
                clip.length = data->seconds() / project.secondsPerBeat();
                clips.push_back (clip);
            }
            if (clips.empty())
                continue;

            // A new audio track, straight under the source, with the clips.
            const auto newId = addAudioTrack (true);
            auto* added = project.findTrack (newId);
            const auto* from = project.findTrack (sourceId);
            added->name = from->name + " render";
            added->colour = from->colour;
            added->parts = clips;
            if (processing == P::dry || processing == P::channelSettings)
            {
                added->volumeDb = from->volumeDb;
                added->pan = from->pan;
            }
            if (processing == P::dry)
            {
                // The source's effects (as they are now), sends and output go with it.
                added->sends = from->sends;
                added->sendsBypass = from->sendsBypass;
                added->output = from->output;
                added->inserts = from->inserts;
                added->preFaderSlots = from->preFaderSlots;
                for (int slot = 0; slot < (int) added->inserts.size(); ++slot)
                {
                    auto& insert = added->inserts[(size_t) slot];
                    if (auto* effect = engine.getEffect (sourceId, slot))
                        effect->getStateInformation (insert.state);
                    if (insert.isEmpty())
                    {
                        engine.insertEffect (newId, slot, nullptr);
                        continue;
                    }
                    juce::String problem;
                    engine.insertEffect (newId, slot, host->createEffect (insert, engine.getSampleRate(), engine.getBlockSize(), problem));
                }
            }
            // Under the source.
            int sourceIndex = 0;
            for (int i = 0; i < (int) project.tracks.size(); ++i)
                if (project.tracks[(size_t) i].id == sourceId)
                    sourceIndex = i;
            std::vector<Track> reordered;
            for (auto& t : project.tracks)
                if (t.id != newId)
                    reordered.push_back (std::move (t));
            auto it = project.tracks.begin();
            for (; it != project.tracks.end() && it->id != newId; ++it) {}
            reordered.insert (reordered.begin() + sourceIndex + 1, std::move (*it));
            project.tracks = std::move (reordered);
            made.push_back (newId);

            // The source: kept, muted (the parts rendered, or the whole track), or removed (a whole track).
            if (options.source == RenderOptions::Source::mute)
            {
                if (source.wholeTrack)
                    project.findTrack (sourceId)->mute = true;
                else
                {
                    auto* t = project.findTrack (sourceId);
                    splitTrackAt (*t, source.start, project.bpm);
                    splitTrackAt (*t, source.end, project.bpm);
                    for (auto& part : t->parts)
                        if (inside (part, source.start, source.end))
                            part.muted = true;
                }
            }
            changed();
            if (options.source == RenderOptions::Source::remove && source.wholeTrack)
                removeTrack (sourceId);
        }
        error = problems.joinIntoString ("\n");
        if (! made.empty())
        {
            selectTrack (made.back());
            selectedTracks = made;
        }
        return made;
    }

    bool Studio::open (const juce::File& source, juce::String& problems)
    {
        const auto xml = juce::XmlDocument::parse (source);
        if (xml == nullptr)
            return false;
        auto loaded = Project::fromXml (*xml);
        if (! loaded)
            return false;
        punchOutAll();

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
            if (track.isChannel())
            {
                engine.addChannel (track.id, track.isGroup());
                continue;
            }
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
                if (inserts[(size_t) slot].isEmpty())
                {
                    engine.insertEffect (busId, slot, nullptr);   // an empty slot
                    continue;
                }
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
        selectedTracks.clear();
        if (selectedTrack != 0)
            selectedTracks.push_back (selectedTrack);
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
