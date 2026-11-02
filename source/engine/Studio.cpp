#include "Studio.h"

#include <set>

#include "AudioAdvice.h"
#include "DawProject.h"
#include "SessionLog.h"
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
        engine.onPluginGoing = [this] (juce::AudioProcessor* plugin) { unwatch (plugin); editorAnswers.erase (plugin); };
        // The chord pads play into the live track's input, as a keyboard would (0.14.0).
        padPlayer.send = [this] (const juce::MidiMessage& message)
        {
            auto m = message;
            m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
            engine.getMidiInput().addMessageToQueue (m);
        };
        padPlayer.onOtherKey = [weak = std::weak_ptr<bool> (alive), this] (int note, bool on)
        {
            juce::MessageManager::callAsync ([weak, this, note, on]
            {
                if (weak.lock() != nullptr)
                    padKeyPlayed (note, on);
            });
        };
        padPlayer.setPads (project.chordPads);
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
            // The first time: ASIO when there's an ASIO driver (JUCE starts on
            // Windows Audio, which runs at the one buffer size Windows sets).
            if (saved == nullptr)
                if (const auto type = AudioAdvice::firstDeviceType (AudioAdvice::typesWithDevices (devices)); type.isNotEmpty())
                    devices.setCurrentAudioDeviceType (type, true);
            for (const auto& input : juce::MidiInput::getAvailableDevices())
                devices.setMidiInputDeviceEnabled (input.identifier, true);
            devices.addMidiInputDeviceCallback ({}, &engine.getLiveInput());
            devices.addAudioCallback (&engine);
            devices.addChangeListener (this);
        }
        else
        {
            host = std::make_unique<PluginHost> (nullptr, false);   // (tests: the old synth, the same everywhere)
            engine.prepare (48000.0, 512);
        }
        library = std::make_unique<ProjectLibrary> (settings.get());

        newProject();
        startTimerHz (30);
    }

    Studio::~Studio()
    {
        stopPreview();
        linePreview.reset();
        engine.setPadPlayer (nullptr);
        // A save still writing finishes first (its call back here is dropped).
        saveAlive->store (false);
        saver.removeAllJobs (false, 60000);
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
            devices.removeMidiInputDeviceCallback ({}, &engine.getLiveInput());
            devices.closeAudioDevice();
        }
    }

    void Studio::reloadAudioDevice()
    {
        SessionLog::note ("RELOAD DRIVER");
        // Closed and opened again: the driver is asked afresh for its sizes
        // (a size set in its own panel replaces one it no longer offers).
        if (devices.getCurrentAudioDevice() == nullptr)
            return;
        devices.closeAudioDevice();
        devices.restartLastAudioDevice();
    }

    void Studio::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        // The device in the session log, when it changes.
        if (auto* device = devices.getCurrentAudioDevice())
        {
            const auto line = "Audio: " + device->getTypeName() + ", " + device->getName() + ", " + juce::String (device->getCurrentSampleRate(), 0)
                            + " Hz, " + juce::String (device->getCurrentBufferSizeSamples()) + " samples, in "
                            + juce::String (device->getInputLatencyInSamples()) + " / out " + juce::String (device->getOutputLatencyInSamples()) + " samples' latency";
            if (line != loggedDevice)
                SessionLog::note (loggedDevice = line);
        }
        else if (loggedDevice.isNotEmpty())
            SessionLog::note ((loggedDevice = {}) + "Audio: no device open");
        // A new sample rate: the recordings are loaded again at it.
        if (! juce::approximatelyEqual (audioCacheRate, engine.getSampleRate()) && exportJob == nullptr)
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
        if (mirroring)
            return;   // (a linked change: the change it came from does all this once, after)
        // A frozen track's parts are locked: whatever changed them is put back.
        for (auto& track : project.tracks)
            if (track.frozen)
                if (const auto kept = frozenParts.find (track.id); kept != frozenParts.end() && track.parts != kept->second)
                    track.parts = kept->second;
        numberParts();
        watchPlugins();
        if (exportJob == nullptr)   // (mid-render the engine plays the render's copy: endExport brings the song back)
            engine.update (project);
        modified = true;
        if (undoable)
            recordUndoState();
        updateLiveTrack();
        sendChangeMessage();
    }

    bool Studio::hasEditor (const juce::AudioProcessor* plugin)
    {
        if (plugin == nullptr)
            return false;
        // Asked once per plugin: a VST3 answers by building its editor's view
        // (JUCE's hasEditor), slow for Kontakt; the Inspector asked after every
        // change, and each click stuck for about a second (0.9.9).
        if (const auto known = editorAnswers.find (plugin); known != editorAnswers.end())
            return known->second;
        return editorAnswers[plugin] = plugin->hasEditor();
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
        const auto* live = project.findTrack (engine.getLiveTrack());
        engine.setLiveInputFilter (live != nullptr ? live->midiInput : juce::String());
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
        project.tempoTrackOn = state.tempoTrackOn;
        project.tempoEvents = state.tempoEvents;
        project.tempoTrackShown = state.tempoTrackShown;
        project.chordTrack = state.chordTrack;   // (0.16.0: its chords and scales are undo steps)
        project.linkGroups = state.linkGroups;   // (0.17.0)
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
                track.solo = then->solo && ! track.hidden && ! track.disabled;   // (hiding or disabling a track ended its solo: undo doesn't bring it back)
                track.parts = then->parts;
                restoreLanes (track.automation, then->automation);
                track.output = then->output;
                track.sends = then->sends;
                track.sendsBypass = then->sendsBypass;
                track.midiInput = then->midiInput;
                track.quickControls = then->quickControls;
                track.midiChannel = then->midiChannel;
                track.linkGroup = then->linkGroup;
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

    void Studio::setMidiInput (int trackId, const juce::String& deviceName)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || track->kind != Track::Kind::instrument || track->midiInput == deviceName)
            return;
        track->midiInput = deviceName;
        changed();
        updateLiveTrack();
    }

    void Studio::setMidiChannel (int trackId, int channel)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || track->kind != Track::Kind::instrument || ! juce::isPositiveAndNotGreaterThan (channel, 16) || channel == 0
            || track->midiChannel == channel)
            return;
        track->midiChannel = channel;
        changed();
    }

    void Studio::setOutput (int trackId, int groupId)
    {
        auto* track = project.findTrack (trackId);
        const auto* group = project.findTrack (groupId);
        if (track == nullptr || track->isChannel() || (groupId != 0 && (group == nullptr || ! group->isGroup())))
            return;   // (FX and group channels go to the master)
        track->output = groupId;
        passOn (trackId, LinkGroup::What::routing, [&] (int other) { if (other != groupId) setOutput (other, groupId); });
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
        if (send.target == 0 && kept.target != 0)
            sendGone (*track, slot);
        kept = send;
        kept.levelDb = juce::jlimit (-60.0f, 6.0f, send.levelDb);
        while (! track->sends.empty() && track->sends.back().target == 0)
            track->sends.pop_back();
        passOn (trackId, LinkGroup::What::sends, [&] (int other) { if (other != send.target) setSend (other, slot, send); });
        changed();
    }

    void Studio::sendGone (Track& track, int slot)
    {
        // Its automation and Quick Controls go with it (a send put in the slot
        // later never plays the old one's lane). Undo brings them back with it.
        track.automation.erase (std::remove_if (track.automation.begin(), track.automation.end(), [slot] (const AutomationLane& l)
        {
            return l.target == AutomationLane::Target::send && l.slot == slot;
        }), track.automation.end());
        for (auto& qc : track.quickControls)
            if (qc && qc->target == AutomationLane::Target::send && qc->slot == slot)
                qc.reset();
    }

    void Studio::setSendLevel (int trackId, int slot, float db, bool undoable)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || ! juce::isPositiveAndBelow (slot, (int) track->sends.size()))
            return;
        const auto before = track->sends[(size_t) slot].levelDb;
        track->sends[(size_t) slot].levelDb = juce::jlimit (-60.0f, 6.0f, db);
        const auto written = writeValue (trackId, control (AutomationLane::Target::send, slot), volumeToTravel (track->sends[(size_t) slot].levelDb));
        const auto after = track->sends[(size_t) slot].levelDb;
        passOn (trackId, LinkGroup::What::sends, [&] (int other)
        {
            if (const auto* t = project.findTrack (other); t != nullptr && slot < (int) t->sends.size() && t->sends[(size_t) slot].target != 0)
                setSendLevel (other, slot, linkedLevel (t->sends[(size_t) slot].levelDb, before, after, -60.0f, 6.0f), undoable);
        });
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
            passOn (trackId, LinkGroup::What::sends, [&] (int other) { setSendsBypass (other, bypass); });
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
            passOn (trackId, LinkGroup::What::recordMonitor, [&] (int other) { setMonitor (other, on); });
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
        pruneLinkGroups();   // (a link group left with one channel goes)
        engine.removeTrack (trackId);
        if (wasChannel)   // what was sent or routed to it: back to the master, no send
            for (auto& track : project.tracks)
            {
                if (track.output == trackId)
                    track.output = 0;
                for (size_t slot = 0; slot < track.sends.size(); ++slot)
                    if (track.sends[slot].target == trackId)
                    {
                        track.sends[slot] = {};
                        sendGone (track, (int) slot);
                    }
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
    //==========================================================================
    // Link groups (0.17.0)
    std::vector<int> Studio::linkedTracks (int trackId, LinkGroup::What what) const
    {
        std::vector<int> ids { trackId };
        const auto* track = project.findTrack (trackId);
        if (track == nullptr)
            return ids;
        const auto add = [&ids] (int id) { if (std::find (ids.begin(), ids.end(), id) == ids.end()) ids.push_back (id); };
        const auto quick = qLink || (isQLinkHeld != nullptr && isQLinkHeld());
        const auto suspended = ! quick && (linksSuspended || (isSuspendHeld != nullptr && isSuspendHeld()));
        if (! suspended)
            if (const auto* group = project.findLinkGroup (track->linkGroup); group != nullptr && group->links (what))
                for (const auto id : project.linkGroupMembers (group->id))
                    add (id);
        // Q-Link (the manual): whatever is touched on a selected channel, on all the selected ones.
        if (quick && what != LinkGroup::What::selection && isTrackSelected (trackId))
            for (const auto id : getSelectedTracks())
                add (id);
        return ids;
    }

    float Studio::linkedLevel (float now, float before, float after, float low, float high) const
    {
        return linksAbsolute ? after : juce::jlimit (low, high, now + (after - before));
    }

    int Studio::linkTracks (const std::vector<int>& trackIds, const LinkGroup& settings)
    {
        std::vector<int> ids;
        for (const auto id : trackIds)
            if (project.findTrack (id) != nullptr && std::find (ids.begin(), ids.end(), id) == ids.end())
                ids.push_back (id);
        if (ids.size() < 2)
            return 0;
        auto group = settings;
        group.id = 1;
        for (const auto& g : project.linkGroups)
            group.id = juce::jmax (group.id, g.id + 1);
        if (group.name.trim().isEmpty())
            group.name = "Link " + juce::String (group.id);
        project.linkGroups.push_back (group);
        for (const auto id : ids)
            project.findTrack (id)->linkGroup = group.id;
        pruneLinkGroups();
        changed();
        return group.id;
    }

    void Studio::removeLinkGroup (int groupId)
    {
        if (project.findLinkGroup (groupId) == nullptr)
            return;
        for (auto& track : project.tracks)
            if (track.linkGroup == groupId)
                track.linkGroup = 0;
        pruneLinkGroups();
        changed();
    }

    void Studio::setTrackLinkGroup (int trackId, int groupId)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || (groupId != 0 && project.findLinkGroup (groupId) == nullptr) || track->linkGroup == groupId)
            return;
        track->linkGroup = groupId;
        pruneLinkGroups();
        changed();
    }

    void Studio::setLinkGroupSettings (int groupId, const LinkGroup& settings)
    {
        auto* group = project.findLinkGroup (groupId);
        if (group == nullptr)
            return;
        auto kept = settings;
        kept.id = groupId;
        if (kept.name.trim().isEmpty())
            kept.name = group->name;
        *group = kept;
        changed();
    }

    const LinkGroup* Studio::getLinkGroupOf (int trackId) const
    {
        const auto* track = project.findTrack (trackId);
        return track != nullptr ? project.findLinkGroup (track->linkGroup) : nullptr;
    }

    void Studio::pruneLinkGroups()
    {
        // A group needs two channels: with fewer, it goes (and its last channel is in none).
        auto& groups = project.linkGroups;
        groups.erase (std::remove_if (groups.begin(), groups.end(), [this] (const LinkGroup& g) { return project.linkGroupMembers (g.id).size() < 2; }),
                      groups.end());
        for (auto& track : project.tracks)
            if (project.findLinkGroup (track.linkGroup) == nullptr)
                track.linkGroup = 0;
    }

    void Studio::toggleTrackSelected (int trackId)
    {
        masterSelected = false;
        if (project.findTrack (trackId) == nullptr)
            return;
        const auto group = linkedTracks (trackId, LinkGroup::What::selection);   // (it and its linked ones, together)
        if (isTrackSelected (trackId) && selectedTracks.size() > group.size())
        {
            for (const auto id : group)
                selectedTracks.erase (std::remove (selectedTracks.begin(), selectedTracks.end(), id), selectedTracks.end());
            if (std::find (group.begin(), group.end(), selectedTrack) != group.end())
                selectedTrack = selectedTracks.back();
            changed (false);
            return;
        }
        for (const auto id : group)
            if (! isTrackSelected (id))
                selectedTracks.push_back (id);
        selectedTrack = trackId;
        changed (false);
    }

    void Studio::selectTrackRun (int trackId)
    {
        masterSelected = false;
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
        return trackId != 0 && ! masterSelected && (trackId == selectedTrack || std::find (selectedTracks.begin(), selectedTracks.end(), trackId) != selectedTracks.end());
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
        copy.disabled = false;   // (a copy of a disabled track is loaded and heard: our choice)
        copy.hidden = false;
        copy.linkGroup = 0;      // (a copy is in no link group: our choice)
        // A copy of a frozen track is a live one (its instrument and effects loaded,
        // its parts editable): it never shares, or deletes, the original's freeze file.
        copy.frozen = false;
        copy.freezeFile = {};
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

    void Studio::setTracksDisabled (const std::vector<int>& trackIds, bool disabled)
    {
        auto any = false;
        for (const auto id : trackIds)
        {
            auto* track = project.findTrack (id);
            if (track == nullptr || track->isChannel() || track->disabled == disabled || track->frozen)
                continue;   // (a frozen track's plugins are already unloaded)
            any = true;
            if (disabled)
            {
                unloadPlugins (*track);
                track->disabled = true;
                track->armed = false;
                track->solo = false;   // (0.9.9: a soloed track out of sight silenced all the others)
            }
            else
            {
                track->disabled = false;
                loadPlugins (*track);
            }
        }
        if (any)
            changed (false);
    }

    void Studio::unloadPlugins (Track& track)
    {
        const auto id = track.id;
        // Its settings kept (as saving does), then its plugins unloaded.
        if (auto* instrument = engine.getInstrument (id))
        {
            juce::MemoryBlock state;
            instrument->getStateInformation (state);
            track.instrument.state = std::move (state);
        }
        for (int slot = 0; slot < (int) track.inserts.size(); ++slot)
            if (auto* effect = engine.getEffect (id, slot))
            {
                juce::MemoryBlock state;
                effect->getStateInformation (state);
                track.inserts[(size_t) slot].state = std::move (state);
            }
        for (int slot = engine.getNumEffects (id) - 1; slot >= 0; --slot)
        {
            effectGoing (id, slot);   // (its window closes first)
            engine.removeEffect (id, slot);
        }
        if (! track.isAudio())
        {
            if (onInstrumentGoing)
                onInstrumentGoing (id);
            engine.setInstrument (id, nullptr);
        }
    }

    void Studio::loadPlugins (Track& track)
    {
        // Loaded again with the settings kept (one that can't be loaded: the slot stays, the sound goes past it).
        const auto id = track.id;
        juce::String error;
        if (! track.isAudio())
            if (auto instance = host->create (track.instrument, engine.getSampleRate(), engine.getBlockSize(), error))
                engine.setInstrument (id, std::move (instance));
        for (int slot = 0; slot < (int) track.inserts.size(); ++slot)
        {
            const auto& insert = track.inserts[(size_t) slot];
            engine.insertEffect (id, slot, insert.isEmpty() ? nullptr
                                                             : host->createEffect (insert, engine.getSampleRate(), engine.getBlockSize(), error));
        }
    }

    bool Studio::isFrozen (int trackId) const
    {
        const auto* track = project.findTrack (trackId);
        return track != nullptr && track->frozen;
    }

    bool Studio::freezeTrack (int trackId, double tailSeconds, juce::String& error)
    {
        SessionLog::note ("Freezing track " + juce::String (trackId));
        auto* track = project.findTrack (trackId);
        if (track == nullptr || track->isChannel() || track->frozen || track->disabled)
        {
            error = "Only an enabled instrument or audio track can be frozen.";
            return false;
        }
        auto end = 0.0;
        for (const auto& part : track->parts)
            if (! part.muted)
                end = juce::jmax (end, part.end());
        if (end <= 0.0)
        {
            error = track->name + " has nothing to freeze.";
            return false;
        }
        const auto folder = getAudioFolder().getChildFile ("Freeze");
        folder.createDirectory();
        const auto file = folder.getChildFile (juce::File::createLegalFileName (track->name) + " " + juce::String (track->id) + ".wav");
        ExportOptions options;
        options.start = 0.0;
        options.end = end;
        options.tailSeconds = juce::jmax (0.0, tailSeconds);
        options.bitsPerSample = 32;
        // Its instrument and inserts only: the fader, pan and sends act on the frozen sound live.
        options.prepareSong = [trackId] (Project& song)
        {
            for (auto& t : song.tracks)
            {
                t.solo = false;
                if (t.id != trackId)
                {
                    silenceForRender (t);
                    continue;
                }
                t.mute = false;
                t.volumeDb = 0.0f;
                t.pan = 0.0f;
                t.sends.clear();
                t.output = 0;
                t.automation.erase (std::remove_if (t.automation.begin(), t.automation.end(), [] (const AutomationLane& lane)
                {
                    using Target = AutomationLane::Target;
                    return lane.target == Target::volume || lane.target == Target::pan || lane.target == Target::mute || lane.target == Target::send;
                }), t.automation.end());
            }
            for (auto& insert : song.masterInserts)
                insert.bypass = true;
            song.masterVolumeDb = 0.0f;
            song.masterRead = false;
        };
        if (! exportMix (file, options, error))
            return false;
        track = project.findTrack (trackId);
        const auto name = "Freeze/" + file.getFileName();
        audioCache.erase (name);
        unloadPlugins (*track);
        track->frozen = true;
        track->freezeFile = name;
        track->armed = false;
        track->monitor = false;
        frozenParts[trackId] = track->parts;
        changed (false);
        return true;
    }

    void Studio::unfreezeTrack (int trackId, bool deleteFile)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || ! track->frozen)
            return;
        track->frozen = false;
        frozenParts.erase (trackId);
        loadPlugins (*track);
        if (deleteFile)
        {
            audioCache.erase (track->freezeFile);
            getAudioFolder().getChildFile (track->freezeFile).deleteFile();
        }
        track->freezeFile = {};
        changed (false);
    }

    void Studio::setTracksHidden (const std::vector<int>& trackIds, bool hidden)
    {
        auto any = false;
        for (const auto id : trackIds)
            if (auto* track = project.findTrack (id); track != nullptr && track->hidden != hidden)
            {
                track->hidden = hidden;
                if (hidden)
                    track->solo = false;   // (0.9.9: hidden and soloed, it silenced every track you could see)
                any = true;
            }
        if (! any)
            return;
        // A hidden track isn't selected (the selection moves to the first shown one).
        selectedTracks.erase (std::remove_if (selectedTracks.begin(), selectedTracks.end(), [this] (int id) { return ! isTrackShown (id); }),
                              selectedTracks.end());
        if (! isTrackShown (selectedTrack))
        {
            const auto shown = getShownTracks();
            selectedTrack = shown.empty() ? 0 : shown.front();
            if (selectedTrack != 0 && selectedTracks.empty())
                selectedTracks.push_back (selectedTrack);
        }
        changed (false);
    }

    bool Studio::isTrackShown (int trackId) const
    {
        const auto* track = project.findTrack (trackId);
        return track != nullptr && ! track->hidden;
    }

    std::vector<int> Studio::getShownTracks() const
    {
        std::vector<int> ids;
        for (const auto& track : project.tracks)
            if (! track.hidden)
                ids.push_back (track.id);
        return ids;
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
        if (track->frozen || track->disabled)
        {
            if (error != nullptr)
                *error = "It's frozen: unfreeze it first (its instrument and effects are unloaded).";
            return false;
        }
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

    void Studio::setTracksColour (const std::vector<int>& trackIds, std::optional<juce::Colour> colour)
    {
        if (colour && ! FamilyColours::isFamily (*colour))
            colour = FamilyColours::nearest (*colour);   // (only the family's: the owner's rule)
        auto any = false;
        for (int i = 0; i < (int) project.tracks.size(); ++i)
        {
            auto& track = project.tracks[(size_t) i];
            if (std::find (trackIds.begin(), trackIds.end(), track.id) == trackIds.end())
                continue;
            const auto now = colour ? *colour : trackColour (i);
            any = any || track.colour != now;
            track.colour = now;
        }
        if (any)
            changed();
    }

    void Studio::setMute (int trackId, bool on)
    {
        if (auto* track = project.findTrack (trackId))
        {
            // A click on M is a touch: with W on while playing, it writes (and latches).
            const auto muteControl = control (AutomationLane::Target::mute);
            grabOne (trackId, muteControl);
            track->mute = on;
            const auto written = writeValue (trackId, muteControl, on ? 1.0f : 0.0f);
            if (written)
                releaseOne (trackId, muteControl);
            passOn (trackId, LinkGroup::What::muteSolo, [&] (int other) { setMute (other, on); });
            changed (! written);
        }
    }

    void Studio::setSolo (int trackId, bool on, bool exclusive)
    {
        if (auto* track = project.findTrack (trackId))
        {
            if (exclusive)   // Ctrl-click (the manual's exclusive solo): the others' solos off
                for (auto& other : project.tracks)
                    other.solo = false;
            project.findTrack (trackId)->solo = on;
            passOn (trackId, LinkGroup::What::muteSolo, [&] (int other) { setSolo (other, on, false); });
            changed();
        }
    }

    bool Studio::anySolo() const
    {
        return std::any_of (project.tracks.begin(), project.tracks.end(), [] (const Track& t) { return t.solo; });
    }

    bool Studio::anyMute() const
    {
        return std::any_of (project.tracks.begin(), project.tracks.end(), [] (const Track& t) { return t.mute; });
    }

    void Studio::deactivateAllSolo()
    {
        if (! anySolo())
            return;
        for (auto& track : project.tracks)
            track.solo = false;
        changed();
    }

    void Studio::deactivateAllMute()
    {
        if (! anyMute())
            return;
        for (auto& track : project.tracks)
            track.mute = false;
        changed();
    }

    void Studio::setVolume (int trackId, float db, bool undoable)
    {
        if (auto* track = project.findTrack (trackId))
        {
            const auto before = track->volumeDb;
            track->volumeDb = juce::jlimit (-60.0f, 6.0f, db);
            // Being written: the pass is the undo step.
            const auto written = writeValue (trackId, control (AutomationLane::Target::volume), volumeToTravel (track->volumeDb));
            const auto after = track->volumeDb;
            passOn (trackId, LinkGroup::What::volume, [&] (int other)
            {
                if (const auto* t = project.findTrack (other))
                    setVolume (other, linkedLevel (t->volumeDb, before, after, -60.0f, 6.0f), undoable);
            });
            changed (undoable && ! written);
        }
    }

    void Studio::setPan (int trackId, float pan, bool undoable)
    {
        if (auto* track = project.findTrack (trackId))
        {
            const auto before = track->pan;
            track->pan = juce::jlimit (-1.0f, 1.0f, pan);
            const auto written = writeValue (trackId, control (AutomationLane::Target::pan), panToTravel (track->pan));
            const auto after = track->pan;
            passOn (trackId, LinkGroup::What::pan, [&] (int other)
            {
                if (const auto* t = project.findTrack (other))
                    setPan (other, linkedLevel (t->pan, before, after, -1.0f, 1.0f), undoable);
            });
            changed (undoable && ! written);
        }
    }

    void Studio::setArmed (int trackId)
    {
        if (const auto* t = project.findTrack (trackId); t != nullptr && (t->isChannel() || t->disabled || t->frozen))
            return;   // an FX or group channel records nothing; nor a disabled track
        for (auto& track : project.tracks)
            track.armed = track.id == trackId;
        changed (false);
    }

    void Studio::selectMaster()
    {
        // The master's row (0.9.12): the Inspector shows the master; no track selected.
        masterSelected = true;
        selectedTracks.clear();   // (no track looks chosen; recording still goes to the one last chosen)
        sendChangeMessage();
    }

    void Studio::selectTrack (int trackId)
    {
        masterSelected = false;
        selectedTrack = trackId;
        selectedTracks.clear();
        if (trackId != 0)
            selectedTracks.push_back (trackId);
        // A channel of a link group selects the group (unless its Selection is off: the manual's).
        if (trackId != 0)
            for (const auto id : linkedTracks (trackId, LinkGroup::What::selection))
                if (! isTrackSelected (id))
                    selectedTracks.push_back (id);
        // Selecting arms it (when not recording), so you play and record the track you're on.
        const auto* selected = project.findTrack (trackId);
        if (! engine.isRecording() && (selected == nullptr || (! selected->isChannel() && ! selected->disabled && ! selected->frozen)))
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
        if (from->frozen || to->frozen)
            return;   // (a frozen track's parts are locked: nothing leaves or joins them)
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
        if (from == nullptr || to == nullptr || ! juce::isPositiveAndBelow (index, (int) from->parts.size()) || to->frozen)
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

    void Studio::deleteParts (const std::vector<int>& partIds)
    {
        auto any = false;
        for (auto& track : project.tracks)
        {
            const auto before = track.parts.size();
            track.parts.erase (std::remove_if (track.parts.begin(), track.parts.end(), [&] (const Part& p)
                                               { return std::find (partIds.begin(), partIds.end(), p.id) != partIds.end(); }),
                               track.parts.end());
            any = any || track.parts.size() != before;
        }
        if (any)
            changed();
    }

    void Studio::moveParts (const std::vector<int>& partIds, double beatDelta, int trackDelta, bool copy)
    {
        const auto shown = getShownTracks();
        const auto shownIndex = [&shown] (int id) { return (int) (std::find (shown.begin(), shown.end(), id) - shown.begin()); };
        // Where each goes (worked out before anything moves).
        struct Going { int from; int partId; int to; };
        std::vector<Going> going;
        for (const auto& track : project.tracks)
            for (const auto& part : track.parts)
                if (std::find (partIds.begin(), partIds.end(), part.id) != partIds.end())
                {
                    if (track.frozen && ! copy)
                        continue;   // (a frozen track's parts are locked: they stay; a copy may leave)
                    auto to = track.id;
                    const auto at = shownIndex (track.id) + trackDelta;
                    if (trackDelta != 0 && juce::isPositiveAndBelow (at, (int) shown.size()))
                        if (const auto* target = project.findTrack (shown[(size_t) at]); target != nullptr && ! target->isChannel()
                                                                                         && target->isAudio() == track.isAudio() && ! target->frozen)
                            to = target->id;
                    if (to == track.id && track.frozen)
                        continue;   // (nor does one join them)
                    going.push_back ({ track.id, part.id, to });
                }
        if (going.empty())
            return;
        for (const auto& g : going)
        {
            auto* from = project.findTrack (g.from);
            const auto it = std::find_if (from->parts.begin(), from->parts.end(), [&g] (const Part& p) { return p.id == g.partId; });
            if (it == from->parts.end())
                continue;
            auto part = *it;
            part.start = juce::jmax (0.0, part.start + beatDelta);
            if (copy)
                part.id = 0;   // a new part (numbered in changed())
            else
                from->parts.erase (it);
            project.findTrack (g.to)->parts.push_back (std::move (part));
        }
        changed();
    }

    Studio::RangeClip Studio::copyParts (const std::vector<int>& partIds) const
    {
        RangeClip clip;
        const auto shown = getShownTracks();
        auto first = (int) shown.size(), last = -1;
        auto start = std::numeric_limits<double>::max(), end = 0.0;
        for (int i = 0; i < (int) shown.size(); ++i)
            for (const auto& part : project.findTrack (shown[(size_t) i])->parts)
                if (std::find (partIds.begin(), partIds.end(), part.id) != partIds.end())
                {
                    first = juce::jmin (first, i);
                    last = juce::jmax (last, i);
                    start = juce::jmin (start, part.start);
                    end = juce::jmax (end, part.end());
                }
        if (last < 0)
            return clip;
        clip.origin = start;
        clip.length = end - start;
        clip.originTrack = shown[(size_t) first];
        for (int i = first; i <= last; ++i)
        {
            std::vector<Part> row;
            for (const auto& part : project.findTrack (shown[(size_t) i])->parts)
                if (std::find (partIds.begin(), partIds.end(), part.id) != partIds.end())
                {
                    auto copy = part;
                    copy.start -= start;
                    copy.id = 0;
                    row.push_back (std::move (copy));
                }
            clip.tracks.push_back (std::move (row));
        }
        return clip;
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
        Part cutPart (Part& part, double rel, const TempoMap& tempo)
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
                const auto seconds = tempo.secondsBetween (part.start, part.start + rel);
                second.audioOffset = part.audioOffset + seconds;
                second.fadeIn = 0.0;
                part.fadeOut = 0.0;
                part.fadeIn = juce::jmin (part.fadeIn, seconds);
                second.fadeOut = juce::jmin (second.fadeOut, tempo.secondsBetween (second.start, second.end()));
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
        track->parts.push_back (cutPart (part, rel, project.tempoMap()));
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
            track->parts.push_back (cutPart (track->parts[(size_t) current], piece, project.tempoMap()));
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
                track.parts.push_back (cutPart (part, rel, project.tempoMap()));
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
                const auto secondsIn = glued.audioOffset + project.tempoMap().secondsBetween (glued.start, glued.end());
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

    bool Studio::humanizeParts (const std::vector<int>& partIds, float amount)
    {
        if (amount < 0.0f)
            amount = getHumanizeAmount();
        if (amount <= 0.0f)
            return false;   // (nothing would change: no empty undo step)
        auto any = false;
        for (auto& track : project.tracks)
        {
            if (track.isAudio() || track.isChannel() || track.frozen)
                continue;
            for (auto& part : track.parts)
            {
                if (std::find (partIds.begin(), partIds.end(), part.id) == partIds.end() || part.notes.empty())
                    continue;
                std::vector<LineNote> notes;
                for (const auto& n : part.notes)
                    notes.push_back ({ n.start, n.length, n.pitch, n.velocity });
                HumanizeOptions o;
                o.amount = amount;
                o.bpm = project.bpmAt (part.start);
                o.beatsPerBar = project.beatsPerBar();
                o.originBeat = part.start;
                if (getHumanizeKeepsLayers())
                    o.layers = guitarLayers();
                o.seed = (juce::uint32) juce::Random::getSystemRandom().nextInt();
                notes = humanize (std::move (notes), o);
                part.notes.clear();
                for (const auto& n : notes)
                    part.notes.push_back ({ n.beat, n.length, n.pitch, n.velocity });
                any = true;
            }
        }
        if (any)
            changed();
        return any;
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
        void splitTrackAt (Track& track, double beat, const TempoMap& tempo)
        {
            const auto count = (int) track.parts.size();
            for (int i = 0; i < count; ++i)
            {
                auto& part = track.parts[(size_t) i];
                if (beat > part.start + edge && beat < part.end() - edge)
                    track.parts.push_back (cutPart (part, beat - part.start, tempo));
            }
        }

        bool inside (const Part& part, double start, double end)
        {
            return part.start >= start - edge && part.end() <= end + edge;
        }

        // What of a part lies between two beats (none if nothing).
        std::optional<Part> slice (Part part, double start, double end, const TempoMap& tempo)
        {
            if (part.end() <= start + edge || part.start >= end - edge)
                return std::nullopt;
            if (start > part.start + edge)
                part = cutPart (part, start - part.start, tempo);
            if (end < part.end() - edge)
                cutPart (part, end - part.start, tempo);   // (keeps the first half)
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
                if (started && found.size() < count && (! track.hidden || track.id == firstTrackId))   // (the tracks shown, as the arrangement has them)
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
                splitTrackAt (*track, range.start, project.tempoMap());
                splitTrackAt (*track, range.end, project.tempoMap());
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
                        if (auto cropped = slice (part, range.start, range.end, project.tempoMap()))
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
                splitTrackAt (*track, range.start, project.tempoMap());
                splitTrackAt (*track, range.end, project.tempoMap());
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
                    if (auto piece = slice (part, range.start, range.end, project.tempoMap()))
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
                splitTrackAt (*track, range.start, project.tempoMap());
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
            if (track == nullptr || track->isChannel() || track->frozen)
                continue;   // (nothing pasted onto a frozen track: its parts are locked)
            if (insertTime)
            {
                splitTrackAt (*track, atBeat, project.tempoMap());
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
                if (auto* track = project.findTrack (id); track != nullptr && ! track->frozen)
                {
                    splitTrackAt (*track, range.start, project.tempoMap());
                    splitTrackAt (*track, range.end, project.tempoMap());
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

    int Studio::importAudio (const juce::Array<juce::File>& files, int trackId, double atBeat, juce::String& error, bool oneTrack)
    {
        SessionLog::note ("Importing " + juce::String (files.size()) + " audio file(s), the first " + files.getFirst().getFullPathName());
        auto folder = getAudioFolder();
        folder.createDirectory();
        juce::StringArray problems;
        std::vector<Part> clips;
        std::vector<bool> stereoClips;
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
            stereoClips.push_back (reader->numChannels > 1);
            Part clip;
            clip.name = file.getFileNameWithoutExtension();
            clip.audioFile = copy.getFileName();
            clip.start = at;
            clip.length = project.tempoMap().beatAfter (at, (double) reader->lengthInSamples / reader->sampleRate) - at;
            if (oneTrack)
                at += clip.length;   // (else they all start at atBeat, each on its own track)
            clips.push_back (clip);
        }
        error = problems.joinIntoString ("\n");
        if (clips.empty())
            return 0;
        const auto* target = project.findTrack (trackId);
        if (target == nullptr || ! target->isAudio() || target->frozen)   // (a frozen track takes no new clips: a new track does)
            trackId = addAudioTrack (stereo);
        if (oneTrack)
        {
            if (auto* track = project.findTrack (trackId))
                for (auto& clip : clips)
                    track->parts.push_back (clip);
        }
        else
        {
            // A track each, named after its file.
            for (size_t i = 0; i < clips.size(); ++i)
            {
                const auto id = i == 0 ? trackId : addAudioTrack (stereoClips[i]);
                if (i > 0)
                    renameTrack (id, clips[i].name);
                if (auto* track = project.findTrack (id))
                    track->parts.push_back (clips[i]);
            }
        }
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
        if (isFrozen (busId))
        {
            if (error != nullptr)
                *error = "It's frozen: unfreeze it first (its instrument and effects are unloaded).";
            return false;
        }
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
        const auto landed = slot >= 0 ? slot : (int) inserts->size();
        if (! addInsert (busId, insert, std::move (instance), slot))
            return false;
        // Linked inserts (the manual): slot by slot; a linked channel's slot already used is left as it is.
        if (passOn (busId, LinkGroup::What::inserts, [&] (int other)
            {
                if (const auto* theirs = insertsOf (other); theirs != nullptr && (landed >= (int) theirs->size() || (*theirs)[(size_t) landed].isEmpty()))
                    addInsert (other, description, nullptr, landed);
            }))
            changed (false);
        return true;
    }

    bool Studio::addInsert (int busId, const Insert& insert, std::unique_ptr<juce::AudioPluginInstance> instance, int slot)
    {
        if (isFrozen (busId))
            return false;
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
        if (isFrozen (busId))
        {
            if (error != nullptr)
                *error = "It's frozen: unfreeze it first (its instrument and effects are unloaded).";
            return false;
        }
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
        passOn (busId, LinkGroup::What::inserts, [&] (int other)
        {
            if (const auto* theirs = insertsOf (other); theirs != nullptr && slot < (int) theirs->size() && ! (*theirs)[(size_t) slot].isEmpty())
                replaceInsert (other, slot, description, nullptr);
            else
                addInsert (other, description, nullptr, slot);
        });
        changed (false);
        return true;
    }

    void Studio::removeInsert (int busId, int slot)
    {
        if (isFrozen (busId))
            return;   // (a frozen track's effects are locked)
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
        passOn (busId, LinkGroup::What::inserts, [&] (int other) { removeInsert (other, slot); });
        changed (false);
    }

    void Studio::setInsertBypass (int busId, int slot, bool bypass)
    {
        auto* inserts = insertsOf (busId);
        if (inserts == nullptr || ! juce::isPositiveAndBelow (slot, (int) inserts->size()))
            return;
        (*inserts)[(size_t) slot].bypass = bypass;
        passOn (busId, LinkGroup::What::inserts, [&] (int other) { setInsertBypass (other, slot, bypass); });
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
        passOn (busId, LinkGroup::What::automation, [&] (int other) { setAutomationRead (other, on); });
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
        passOn (busId, LinkGroup::What::automation, [&] (int other) { setAutomationWrite (other, on); });
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
            {
                fix (track->automation);
                // Its Quick Controls go too (and those after it move up with it).
                for (auto& qc : track->quickControls)
                    if (qc && qc->target == AutomationLane::Target::insert)
                    {
                        if (qc->slot == slot)
                            qc.reset();
                        else if (shiftUp && qc->slot > slot)
                            --qc->slot;
                    }
            }
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
        ~KnobWatcher() override
        {
            if (! detached)
                plugin.removeListener (this);
        }
        // Off the plugin now; deleted later (the audio thread may be calling it this moment).
        void detach()
        {
            plugin.removeListener (this);
            detached = true;
        }

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
            // From the audio thread or a render's: a plugin's own output (a meter,
            // gain reduction), not a hand on a knob; nothing posted (it allocates).
            if (SessionLog::isAudioThread())
                return;
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
        bool detached = false;
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
        if (plugin == nullptr)
            return;
        // (Before it's deleted: the engine says so, however it goes.) Taken off it
        // now, deleted once this has finished (and the engine has let the plugin go).
        if (const auto found = watchers.find (plugin); found != watchers.end())
        {
            found->second->detach();
            std::shared_ptr<KnobWatcher> retired (std::move (found->second));
            watchers.erase (found);
            juce::MessageManager::callAsync ([retired] {});
        }
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

    std::optional<LinkGroup::What> Studio::linkFor (const AutomationLane& which)
    {
        using T = AutomationLane::Target;
        switch (which.target)
        {
            case T::volume: return LinkGroup::What::volume;
            case T::pan:    return LinkGroup::What::pan;
            case T::mute:   return LinkGroup::What::muteSolo;
            case T::send:   return LinkGroup::What::sends;
            case T::insert: return LinkGroup::What::inserts;
            default:        return std::nullopt;
        }
    }

    void Studio::grabControl (int busId, const AutomationLane& which)
    {
        // A hand on a linked control is on its linked ones too (they write what they're given).
        grabOne (busId, which);
        if (const auto what = linkFor (which))
            passOn (busId, *what, [&] (int other) { grabOne (other, which); });
    }

    void Studio::releaseControl (int busId, const AutomationLane& which)
    {
        releaseOne (busId, which);
        if (const auto what = linkFor (which))
            passOn (busId, *what, [&] (int other) { releaseOne (other, which); });
    }

    void Studio::grabOne (int busId, const AutomationLane& which)
    {
        // (Mid-render the engine is "playing" the render's copy: a knob turned in a
        // plug-in's window then writes nothing.)
        if (! engine.isPlaying() || exportJob != nullptr || ! getAutomationWrite (busId))
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

    void Studio::releaseOne (int busId, const AutomationLane& which)
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
        // A linked insert's knob (the manual: slot by slot): the same knob of the same
        // effect in that slot on the linked channels (its watcher writes it there).
        if (which.target != AutomationLane::Target::insert)
            return;
        const auto* source = engine.getEffect (busId, which.slot);
        if (source == nullptr)
            return;
        passOn (busId, LinkGroup::What::inserts, [&] (int other)
        {
            if (auto* theirs = engine.getEffect (other, which.slot); theirs != nullptr && theirs->getName() == source->getName())
                if (auto* parameter = theirs->getParameters()[which.parameter])
                    if (std::abs (parameter->getValue() - value) > 1.0e-6f)
                        parameter->setValueNotifyingHost (value);
        });
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

    juce::File Studio::trackPresetFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Paragon PR7-S").getChildFile ("Track Presets");
    }

    std::unique_ptr<juce::XmlElement> Studio::makeTrackPreset (int trackId) const
    {
        const auto* original = project.findTrack (trackId);
        if (original == nullptr)
            return nullptr;
        // The sound and channel settings, with the running plugins' settings now; not its parts, automation, sends or output.
        auto preset = *original;
        if (auto* instrument = engine.getInstrument (trackId))
            instrument->getStateInformation (preset.instrument.state);
        for (int slot = 0; slot < (int) preset.inserts.size(); ++slot)
            if (auto* effect = engine.getEffect (trackId, slot))
                effect->getStateInformation (preset.inserts[(size_t) slot].state);
        preset.parts.clear();
        preset.automation.clear();
        preset.sends.clear();
        preset.output = 0;
        preset.armed = preset.mute = preset.solo = false;
        preset.hidden = preset.disabled = preset.frozen = false;
        preset.freezeFile = {};
        preset.automationOpen = preset.automationRead = preset.automationWrite = false;
        Project holder;
        holder.tracks = { preset };
        auto xml = holder.toXml();
        xml->setAttribute ("trackPreset", true);
        return xml;
    }

    namespace
    {
        bool sameKind (const Track& a, const Track& b) { return a.kind == b.kind; }
    }

    bool Studio::applyTrackPreset (int trackId, const juce::XmlElement& xml, juce::String& error)
    {
        auto* track = project.findTrack (trackId);
        const auto loaded = Project::fromXml (xml);
        if (track == nullptr || ! loaded || loaded->tracks.empty())
        {
            error = "That isn't a Paragon PR7-S track preset.";
            return false;
        }
        const auto& preset = loaded->tracks.front();
        if (! sameKind (*track, preset))
        {
            error = "A track preset only goes on a track of its own kind (as the manual has it).";
            return false;
        }
        if (track->frozen || track->disabled)
        {
            error = track->name + " is frozen or disabled: unfreeze or enable it first.";
            return false;
        }
        // Its effects and their lanes and Quick Controls go (also from the undo history); the instrument's lanes too.
        for (int slot = engine.getNumEffects (trackId) - 1; slot >= 0; --slot)
        {
            effectGoing (trackId, slot);
            engine.removeEffect (trackId, slot);
        }
        for (int slot = (int) track->inserts.size() - 1; slot >= 0; --slot)
            insertGone (trackId, slot, false);
        const auto dropInstrumentLanes = [trackId] (Project& p)
        {
            if (auto* t = p.findTrack (trackId))
                t->automation.erase (std::remove_if (t->automation.begin(), t->automation.end(), [] (const AutomationLane& l)
                                     { return l.target == AutomationLane::Target::instrument; }), t->automation.end());
        };
        track = project.findTrack (trackId);
        if (! track->isAudio() && ! track->isChannel())
            dropInstrumentLanes (project);
        for (auto& state : undoStates)
            if (! track->isAudio() && ! track->isChannel())
                dropInstrumentLanes (state);

        // The preset's settings.
        const auto apply = [&preset] (Track& t)
        {
            t.volumeDb = preset.volumeDb;
            t.pan = preset.pan;
            t.colour = preset.colour;
            t.quickControls = preset.quickControls;
            t.midiChannel = preset.midiChannel;
            t.preFaderSlots = preset.preFaderSlots;
            if (t.isAudio())
            {
                t.input = preset.input;
                t.stereoInput = preset.stereoInput;
            }
        };
        apply (*track);
        for (auto& state : undoStates)   // (not undone: as the manual has it)
            if (auto* t = state.findTrack (trackId))
                apply (*t);
        track->inserts = preset.inserts;
        if (! track->isAudio() && ! track->isChannel())
        {
            if (onInstrumentGoing)
                onInstrumentGoing (trackId);
            track->instrument = preset.instrument;
            juce::String problem;
            auto instance = host->create (preset.instrument, engine.getSampleRate(), engine.getBlockSize(), problem);
            if (instance == nullptr)
                error = preset.instrument.getName() + ": " + problem;
            engine.setInstrument (trackId, std::move (instance));
        }
        for (int slot = 0; slot < (int) track->inserts.size(); ++slot)
        {
            const auto& insert = track->inserts[(size_t) slot];
            juce::String problem;
            auto instance = insert.isEmpty() ? nullptr : host->createEffect (insert, engine.getSampleRate(), engine.getBlockSize(), problem);
            if (! insert.isEmpty() && instance == nullptr)
                error += (error.isEmpty() ? "" : "\n") + insert.plugin.name + ": " + problem;
            engine.insertEffect (trackId, slot, std::move (instance));
        }
        changed (false);
        return true;
    }

    int Studio::addTrackFromPreset (const juce::XmlElement& xml, juce::String& error)
    {
        const auto loaded = Project::fromXml (xml);
        if (! loaded || loaded->tracks.empty())
        {
            error = "That isn't a Paragon PR7-S track preset.";
            return 0;
        }
        const auto& preset = loaded->tracks.front();
        const auto below = selectedTrack;
        int id = 0;
        switch (preset.kind)
        {
            case Track::Kind::instrument: id = addTrack ({}, &error); break;
            case Track::Kind::audio:      id = addAudioTrack (preset.stereoInput); break;
            case Track::Kind::fx:         id = addFxChannel (nullptr, &error); break;
            case Track::Kind::group:      id = addGroupChannel(); break;
        }
        if (id == 0)
            return 0;
        renameTrack (id, preset.name);
        juce::String problems;
        applyTrackPreset (id, xml, problems);
        error = problems;
        // Under the selected track (the manual's).
        for (int i = 0; i < (int) project.tracks.size(); ++i)
            if (project.tracks[(size_t) i].id == below)
            {
                moveTracks ({ id }, i + 1);
                break;
            }
        selectTrack (id);
        return id;
    }

    void Studio::setQuickControl (int trackId, int index, std::optional<AutomationControl> control)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || ! juce::isPositiveAndBelow (index, Track::numQuickControls))
            return;
        track->quickControls[(size_t) index] = control ? std::optional<AutomationLane> (control->asLane()) : std::nullopt;
        changed();
    }

    std::optional<AutomationLane> Studio::getQuickControl (int trackId, int index) const
    {
        const auto* track = project.findTrack (trackId);
        if (track == nullptr || ! juce::isPositiveAndBelow (index, Track::numQuickControls))
            return std::nullopt;
        return track->quickControls[(size_t) index];
    }

    void Studio::setQuickControlValue (int trackId, int index, float travel, bool undoable)
    {
        const auto qc = getQuickControl (trackId, index);
        if (! qc)
            return;
        travel = juce::jlimit (0.0f, 1.0f, travel);
        // The control itself, through its own action (so automation writing and undo follow it).
        switch (qc->target)
        {
            case AutomationLane::Target::volume: setVolume (trackId, travelToVolume (travel), undoable); break;
            case AutomationLane::Target::pan:    setPan (trackId, travelToPan (travel), undoable); break;
            case AutomationLane::Target::mute:
                if (const auto* t = project.findTrack (trackId); t != nullptr && t->mute != (travel >= 0.5f))
                    setMute (trackId, travel >= 0.5f);
                break;
            case AutomationLane::Target::send:   setSendLevel (trackId, qc->slot, travelToVolume (travel), undoable); break;
            case AutomationLane::Target::instrument:
            case AutomationLane::Target::insert:
                // The plugin's knob (its watcher then writes automation, as when it's turned in the plugin's window).
                if (auto* plugin = pluginFor (trackId, *qc))
                    if (qc->parameter < plugin->getParameters().size())
                        plugin->getParameters()[qc->parameter]->setValueNotifyingHost (travel);
                break;
        }
    }

    float Studio::getQuickControlValue (int trackId, int index) const
    {
        const auto qc = getQuickControl (trackId, index);
        return qc ? getControlValue (trackId, *qc) : 0.0f;
    }

    juce::String Studio::getQuickControlName (int trackId, int index) const
    {
        const auto qc = getQuickControl (trackId, index);
        return qc ? getLaneName (trackId, *qc) : juce::String();
    }

    void Studio::assignInstrumentQuickControls (int trackId)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr)
            return;
        auto q = 0;
        for (const auto& c : getAutomationControls (trackId))
            if (c.target == AutomationLane::Target::instrument && q < Track::numQuickControls)
                track->quickControls[(size_t) q++] = c.asLane();
        if (q > 0)
            changed();
    }

    void Studio::showAutomatedQuickControls (int trackId)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr)
            return;
        auto any = false;
        for (const auto& qc : track->quickControls)
            if (qc)
                for (auto& lane : track->automation)
                    if (lane.sameControl (*qc) && ! lane.points.empty())
                        lane.shown = any = true;
        if (any)
        {
            track->automationOpen = true;
            changed (false);
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
        const auto returnBeats = pass.control.isStep() ? 0.0 : project.tempoMap().beatAfter (endBeat, returnSeconds) - endBeat;
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

    //==========================================================================
    void Studio::addTempoTrack()
    {
        if (project.tempoTrackShown)
            return;
        project.tempoTrackShown = true;
        if (project.tempoEvents.empty())
            project.tempoEvents.push_back ({ 0.0, project.bpm, false });
        project.tempoTrackOn = true;
        changed();
    }

    void Studio::removeTempoTrack()
    {
        if (! project.tempoTrackShown)
            return;
        project.tempoTrackShown = false;
        project.tempoTrackOn = false;
        changed();
    }

    void Studio::setTempoTrackOn (bool on)
    {
        if (on && project.tempoEvents.empty())
            project.tempoEvents.push_back ({ 0.0, project.bpm, false });
        if (project.tempoTrackOn == on)
            return;
        project.tempoTrackOn = on;
        changed();
    }

    namespace
    {
        void tidyTempo (std::vector<TempoEvent>& events, double fallback)
        {
            for (auto& e : events)
            {
                e.beat = juce::jmax (0.0, e.beat);
                e.bpm = juce::jlimit (Project::minBpm, Project::maxBpm, e.bpm);
            }
            std::stable_sort (events.begin(), events.end(), [] (const TempoEvent& a, const TempoEvent& b) { return a.beat < b.beat; });
            if (events.empty())
                events.push_back ({ 0.0, fallback, false });
            events.front().beat = 0.0;
            events.front().ramp = false;
            // Two at one beat: the later one (a jump there).
            for (size_t i = 1; i < events.size();)
                if (events[i].beat - events[i - 1].beat < 1.0e-6)
                    events.erase (events.begin() + (std::ptrdiff_t) i - 1);
                else
                    ++i;
        }
    }

    int Studio::addTempoEvent (double beat, double bpm)
    {
        if (project.tempoLocked)
            return -1;
        auto events = project.tempoEvents;
        if (events.empty())
            events.push_back ({ 0.0, project.bpm, false });
        beat = juce::jmax (0.0, beat);
        bpm = juce::jlimit (Project::minBpm, Project::maxBpm, bpm);
        for (size_t i = 0; i < events.size(); ++i)
            if (std::abs (events[i].beat - beat) < 1.0e-6)
            {
                events[i].bpm = bpm;
                project.tempoEvents = std::move (events);
                changed();
                return (int) i;
            }
        // Its type: Ramp, Step, or (Automatic) as the event before it.
        auto ramp = project.newTempoType == Project::NewTempoType::ramp;
        if (project.newTempoType == Project::NewTempoType::automatic)
        {
            ramp = false;
            for (const auto& e : events)
                if (e.beat < beat)
                    ramp = e.ramp;
        }
        events.push_back ({ beat, bpm, ramp });
        tidyTempo (events, project.bpm);
        project.tempoEvents = std::move (events);
        changed();
        for (size_t i = 0; i < project.tempoEvents.size(); ++i)
            if (std::abs (project.tempoEvents[i].beat - beat) < 1.0e-6)
                return (int) i;
        return -1;
    }

    void Studio::setTempoEvents (std::vector<TempoEvent> events)
    {
        if (project.tempoLocked)
            return;
        tidyTempo (events, project.bpm);
        if (events == project.tempoEvents)
            return;
        project.tempoEvents = std::move (events);
        changed();
    }

    void Studio::removeTempoEvents (std::vector<int> indices)
    {
        if (project.tempoLocked)
            return;
        std::sort (indices.begin(), indices.end(), std::greater<>());
        auto events = project.tempoEvents;
        auto any = false;
        for (const auto i : indices)
            if (i > 0 && i < (int) events.size())
            {
                events.erase (events.begin() + i);
                any = true;
            }
        if (! any)
            return;
        project.tempoEvents = std::move (events);
        changed();
    }

    void Studio::setTempoAtCursor (double bpm)
    {
        if (! project.followsTempoTrack())
        {
            setTempo (bpm);
            return;
        }
        if (project.tempoLocked)
            return;
        const auto at = engine.getPosition();
        auto events = project.tempoEvents;
        size_t governing = 0;
        for (size_t i = 0; i < events.size(); ++i)
            if (events[i].beat <= at + 1.0e-9)
                governing = i;
        events[governing].bpm = juce::jlimit (Project::minBpm, Project::maxBpm, bpm);
        project.tempoEvents = std::move (events);
        changed();
    }

    int Studio::insertTempoEventAtCursor (double bpm)
    {
        if (project.tempoEvents.empty())
            project.tempoEvents.push_back ({ 0.0, project.bpm, false });
        return addTempoEvent (engine.getPosition(), bpm);
    }

    void Studio::setTempoLocked (bool locked)
    {
        project.tempoLocked = locked;
        changed (false);
    }

    void Studio::setNewTempoType (Project::NewTempoType type)
    {
        project.newTempoType = type;
        changed (false);
    }

    void Studio::setTempoView (double low, double high)
    {
        project.tempoViewLow = juce::jlimit (Project::minBpm, Project::maxBpm - 10.0, low);
        project.tempoViewHigh = juce::jlimit (project.tempoViewLow + 10.0, Project::maxBpm, high);
        changed (false);
    }

    double Studio::tempoScaling (const TempoMap& map, double start, double end, double newSeconds)
    {
        const auto seconds = map.secondsBetween (start, end);
        return newSeconds > 0.0 ? seconds / newSeconds : 0.0;
    }

    bool Studio::processTempo (double start, double end, double newSeconds, double* factorOut)
    {
        start = juce::jmax (0.0, start);
        if (end <= start + 1.0e-6 || newSeconds <= 0.0)
            return false;
        if (project.tempoEvents.empty())
            project.tempoEvents.push_back ({ 0.0, project.bpm, false });
        const TempoMap before (project.followsTempoTrack() ? project.tempoEvents : std::vector<TempoEvent> { { 0.0, project.bpm, false } });
        const auto factor = tempoScaling (before, start, end, newSeconds);
        if (factorOut != nullptr)
            *factorOut = factor;
        auto events = before.getEvents();
        // The curve's values at the edges, before scaling (what's outside stays).
        const auto atStart = before.bpmAt (start), atEnd = before.bpmAt (end);
        // The event in effect at the end, and whether it ramps into the end.
        auto rampsIntoEnd = false;
        for (size_t i = 0; i + 1 < events.size(); ++i)
            if (events[i].beat < end && events[i + 1].beat > end)
                rampsIntoEnd = events[i + 1].ramp;
        const auto hasEventAt = [&] (double beat)
        {
            return std::any_of (events.begin(), events.end(), [beat] (const TempoEvent& e) { return std::abs (e.beat - beat) < 1.0e-6; });
        };
        // Edges: an event at the start (its value as it was, so the curve before it is kept), one at the end.
        if (! hasEventAt (start))
        {
            // (A ramp crossing the start: it now arrives at the start's value.)
            auto rampsIntoStart = false;
            for (size_t i = 0; i + 1 < events.size(); ++i)
                if (events[i].beat < start && events[i + 1].beat > start)
                    rampsIntoStart = events[i + 1].ramp;
            events.push_back ({ start, atStart, rampsIntoStart });
        }
        if (! hasEventAt (end))
            events.push_back ({ end, atEnd, rampsIntoEnd });
        std::stable_sort (events.begin(), events.end(), [] (const TempoEvent& a, const TempoEvent& b) { return a.beat < b.beat; });
        // Inside (from the start to the end, both): scaled. The event at the
        // start keeps its arrival (a ramp into it now arrives at the scaled value:
        // so the curve before it doesn't change, a step there at the old value
        // first) and the tempo after the end comes back just after it.
        constexpr double just = 1.0 / 960.0;   // (a 960th of a beat: two events can't share a beat)
        std::vector<TempoEvent> processed;
        for (const auto& e : events)
        {
            const auto inside = e.beat >= start - 1.0e-6 && e.beat <= end + 1.0e-6;
            auto scaled = e;
            if (inside)
                scaled.bpm = e.bpm * factor;
            if (inside && std::abs (e.beat - start) < 1.0e-6 && e.ramp && start > 0.0)
            {
                // A ramp arriving at the start: it arrives (just before it) where it did, then the scaled tempo from the start.
                processed.push_back ({ start - just, before.bpmAt (start - just), true });
                scaled = { start, e.bpm * factor, false };
            }
            processed.push_back (scaled);
            if (inside && std::abs (e.beat - end) < 1.0e-6)
                processed.push_back ({ end + just, before.bpmAt (end + just), false });   // back on the curve as it was
        }
        for (const auto& e : processed)
            if (e.bpm < Project::minBpm - 1.0e-9 || e.bpm > Project::maxBpm + 1.0e-9)
                return false;
        std::stable_sort (processed.begin(), processed.end(), [] (const TempoEvent& a, const TempoEvent& b) { return a.beat < b.beat; });
        processed.front().beat = 0.0;
        project.tempoEvents = std::move (processed);
        project.tempoTrackShown = true;
        project.tempoTrackOn = true;
        changed();
        return true;
    }

    double Studio::getTempoNow() const
    {
        return project.bpmAt (engine.getPosition());
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

    void Studio::setSnap (bool on)
    {
        project.snapOn = on;
        changed (false);
    }

    void Studio::setQuantizeBeats (double beats)
    {
        project.quantizeBeats = juce::jlimit (1.0 / 16.0, 4.0, beats);
        changed (false);
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
            part.start = first >= 0 ? project.tempoMap().beatAt ((double) first / engine.getSampleRate()) : recordStart;
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
        const auto tempo = project.tempoMap();
        Part part;
        part.start = tempo.beatAt ((double) startSample / rate);
        part.audioFile = name;
        part.audioOffset = latency;
        part.length = tempo.beatAfter (part.start, data->seconds() - latency) - part.start;
        // In a cycle, the file holds every pass one after another: the clip
        // plays the last complete one (as the manual's Replace mode keeps the
        // last take), or what there is of the first if it wasn't finished.
        const auto toSamples = [rate, &tempo] (double beats) { return (juce::int64) std::llround (tempo.secondsAt (beats) * rate); };
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
                part.length = juce::jmin (part.length, tempo.beatAfter (part.start, (double) firstPass / rate) - part.start);
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
        if (rendering || exportJob != nullptr)
            return;   // (a render runs: the screen's thread carries on meanwhile, nothing to do here)
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
        frozenParts.clear();
        punchOutAll();   // (writing automation ends with the song)
        if (onInstrumentGoing)
            onInstrumentGoing (0);
        removeAllEffects();
        audioCache.clear();
        engine.stop();
        for (const auto& track : project.tracks)
            engine.removeTrack (track.id);
        project = {};
        padsChanged();
        file = juce::File();
        selectedTrack = 0;
        selectedTracks.clear();
        engine.setPosition (0.0);
        playStart = 0.0;
        // A new song starts with one track on the studio's synth (PR7-K), ready to play.
        addTrack ({});
        engine.update (project);
        undoStates.clear();
        undoIndex = -1;
        recordUndoState();
        modified = false;
        sendChangeMessage();
    }

    int Studio::importMidi (const juce::File& source, bool newSong, double atBeat, juce::String& error, bool matchTempo, int ontoTrackId)
    {
        SessionLog::note ("Importing the MIDI file " + source.getFullPathName());
        const auto read = readMidiFile (source, error);
        return read ? importMidi (*read, newSong, atBeat, matchTempo, ontoTrackId) : 0;
    }

    float Studio::getDropHighlight() const
    {
        if (settings != nullptr)
            return juce::jlimit (0.1f, 1.0f, (float) settings->getDoubleValue ("dropHighlight", defaultDropHighlight));
        return dropHighlight;
    }

    void Studio::setDropHighlight (float strength)
    {
        dropHighlight = juce::jlimit (0.1f, 1.0f, strength);
        if (settings != nullptr)
            settings->setValue ("dropHighlight", (double) dropHighlight);
    }

    bool Studio::canTakeMidi (int trackId) const
    {
        const auto* track = project.findTrack (trackId);
        return track != nullptr && ! track->isChannel() && ! track->isAudio() && ! track->frozen && ! track->disabled;
    }

    int Studio::importMidi (const MidiImport& midi, bool newSong, double atBeat, bool matchTempo, int ontoTrackId)
    {
        // Its tempo changes on the tempo track (0.10.0): a new song's, or (matching the
        // file) from where it goes in, the song's tempo events before that kept.
        // (A song following its tempo track takes even one tempo there: its fixed tempo isn't heard.)
        const auto changes = midi.tempoChanges.size() > 1 || (! newSong && matchTempo && project.followsTempoTrack());
        if (newSong || matchTempo)
        {
            if (! newSong && std::abs (project.bpm - midi.bpm) > 1.0e-6 && ! changes)
                setTempo (midi.bpm);
        }
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
        if (changes && (newSong || matchTempo))
        {
            std::vector<TempoEvent> events;
            for (const auto& e : project.tempoEvents)
                if (e.beat < atBeat - 1.0e-9)
                    events.push_back (e);
            if (events.empty() && atBeat > 0.0)
                events.push_back ({ 0.0, project.bpm, false });   // (the song's tempo up to where the file goes in)
            auto fromFile = midi.tempoChanges;
            if (fromFile.empty())
                fromFile.push_back ({ 0.0, midi.bpm, false });
            for (auto e : fromFile)
            {
                e.beat += atBeat;
                events.push_back (e);
            }
            project.tempoEvents = std::move (events);
            project.tempoTrackShown = true;
            project.tempoTrackOn = true;
            changed (! newSong);
        }
        const auto bar = project.beatsPerBar();
        auto added = 0;
        auto onto = ! newSong && canTakeMidi (ontoTrackId) ? ontoTrackId : 0;
        for (const auto& imported : midi.tracks)
        {
            // The first onto the track it was dropped on (0.18.1: it made a new one); the others new tracks.
            auto id = onto;
            onto = 0;
            if (id == 0)
            {
                id = addTrack ({});
                if (id == 0)
                    continue;
                renameTrack (id, imported.name);
            }
            // One part over the file's bars its notes are in, from where it went in (the drop: snapped).
            auto first = std::numeric_limits<double>::max(), last = 0.0;
            for (const auto& note : imported.notes)
            {
                first = juce::jmin (first, note.start);
                last = juce::jmax (last, note.start + note.length);
            }
            Part part;
            part.start = juce::jmax (0.0, atBeat + std::floor (first / bar + 1.0e-9) * bar);
            part.length = juce::jmax (bar, atBeat + std::ceil (last / bar - 1.0e-9) * bar - part.start);
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
        if (! prepareSave (destination))
            return false;
        const auto xml = project.toXml();
        if (! xml->writeTo (destination))
            return false;
        file = destination;
        modified = false;
        library->addRecent (destination);
        sendChangeMessage();
        return true;
    }

    bool Studio::createSong (const juce::String& name, juce::String& error)
    {
        const auto destination = library->newSongFile (name);
        if (! destination.getParentDirectory().createDirectory())
        {
            error = "Couldn't make the folder " + destination.getParentDirectory().getFullPathName();
            return false;
        }
        newProject();
        if (! save (destination))
        {
            error = "Couldn't save " + destination.getFullPathName();
            return false;
        }
        return true;
    }

    bool Studio::newFromTemplate (const juce::File& templateFile, juce::String& problems)
    {
        if (! open (templateFile, problems))
            return false;
        // Not the template itself: a new song (saving asks where), its recordings in a new unsaved folder.
        library->removeRecent (templateFile);
        const auto templateAudio = getAudioFolder();
        file = juce::File();
        const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
        unsavedAudioFolder = unsavedAudioFolder.getParentDirectory().getChildFile (stamp);
        // The template's recordings and freeze files: the new song's own copies
        // (they stayed in the template's folder, and the song never found them again).
        if (! copyAudioFiles (templateAudio, unsavedAudioFolder))
            problems << "Some of the template's recordings couldn't be copied.\n";
        audioCache.clear();
        engine.update (project);
        modified = false;
        sendChangeMessage();
        return true;
    }

    bool Studio::saveAsTemplate (const juce::String& name, juce::String& error)
    {
        const auto legal = juce::File::createLegalFileName (name.trim());
        if (legal.isEmpty())
        {
            error = "A template needs a name.";
            return false;
        }
        const auto folder = ProjectLibrary::templateFolder();
        folder.createDirectory();
        const auto destination = folder.getChildFile (legal + Project::fileExtension);
        // Saved as a song would be, then the song is put back as it was (its file, saved or not).
        const auto keptFile = file;
        const auto keptModified = modified;
        const auto written = save (destination);
        library->removeRecent (destination);
        file = keptFile;
        modified = keptModified;
        sendChangeMessage();
        if (! written)
            error = "Couldn't write " + destination.getFullPathName();
        return written;
    }

    void Studio::saveInBackground (const juce::File& destination, std::function<void (bool)> done)
    {
        // The plugins' settings are asked for here (a VST3 must be, on this
        // thread); the file is made and written on a thread of its own, so the
        // screen doesn't stop (0.9.11: a song with large Kontakt settings held
        // it about a second). Marked saved now; a change meanwhile marks it
        // unsaved again, and a failed write too.
        if (! prepareSave (destination))
        {
            if (done)
                done (false);
            return;
        }
        auto copy = std::make_shared<Project> (project);
        file = destination;
        modified = false;
        library->addRecent (destination);
        ++savesRunning;
        sendChangeMessage();
        saver.addJob ([copy, destination, done, life = saveAlive, this]
        {
            const auto written = copy->toXml()->writeTo (destination);
            juce::MessageManager::callAsync ([life, this, written, done]
            {
                if (! life->load())
                    return;
                --savesRunning;
                if (! written)
                    modified = true;
                sendChangeMessage();
                if (done)
                    done (written);
            });
        });
    }

    bool Studio::prepareSave (const juce::File& destination)
    {
        SessionLog::note ("Saving " + destination.getFullPathName());
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
        return copyAudioFiles (getAudioFolder(), destination.getSiblingFile (destination.getFileNameWithoutExtension() + " Audio"));
    }

    void Studio::silenceForRender (Track& t)
    {
        // Another track than the one rendered or frozen: nothing of it in the file.
        // Its parts go; a frozen one's freeze file isn't played either (it was:
        // freezing tracks one after another put every earlier one in each new
        // file); and it's muted, so an instrument that plays patterns of its own
        // while the song runs (a drum machine following the transport) isn't
        // heard. FX and group channels stay: they carry the rendered track's
        // sends and output.
        if (t.isChannel())
            return;
        t.parts.clear();
        t.frozen = false;
        t.freezeFile = {};
        t.mute = true;
        t.monitor = false;
    }

    bool Studio::copyAudioFiles (const juce::File& from, const juce::File& to)
    {
        if (from == to)
            return true;
        // Every file the song uses: its clips and freeze files, and those in its
        // undo history too (an undo may bring a deleted clip back after Save As).
        std::set<juce::String> used;
        const auto collect = [&used] (const Project& p)
        {
            for (const auto& track : p.tracks)
            {
                if (track.freezeFile.isNotEmpty())
                    used.insert (track.freezeFile);
                for (const auto& part : track.parts)
                    if (part.isAudio() && part.audioFile.isNotEmpty())
                        used.insert (part.audioFile);
            }
        };
        collect (project);
        for (const auto& state : undoStates)
            collect (state);
        for (const auto& [id, parts] : frozenParts)
            for (const auto& part : parts)
                if (part.isAudio() && part.audioFile.isNotEmpty())
                    used.insert (part.audioFile);

        std::map<juce::String, juce::String> renamed;
        for (const auto& name : used)
        {
            const auto source = from.getChildFile (name);
            if (! source.existsAsFile())
                continue;
            auto target = to.getChildFile (name);
            if (target.exists())
            {
                if (target.hasIdenticalContentTo (source))
                    continue;   // (already there: saved before)
                // Another song's file of the same name (saved over a song that had one):
                // ours goes beside it under a new name, never in its place, never skipped.
                target = target.getParentDirectory().getNonexistentChildFile (target.getFileNameWithoutExtension(), target.getFileExtension(), false);
                renamed[name] = (name.containsChar ('/') ? name.upToLastOccurrenceOf ("/", true, false) : juce::String()) + target.getFileName();
            }
            target.getParentDirectory().createDirectory();
            if (! source.copyFileTo (target))
                return false;
        }
        if (! renamed.empty())
        {
            const auto rename = [&renamed] (juce::String& name)
            {
                if (const auto found = renamed.find (name); found != renamed.end())
                    name = found->second;
            };
            const auto apply = [&rename] (Project& p)
            {
                for (auto& track : p.tracks)
                {
                    rename (track.freezeFile);
                    for (auto& part : track.parts)
                        if (part.isAudio())
                            rename (part.audioFile);
                }
            };
            apply (project);
            for (auto& state : undoStates)
                apply (state);
            for (auto& [id, parts] : frozenParts)
                for (auto& part : parts)
                    if (part.isAudio())
                        rename (part.audioFile);
        }
        return true;
    }

    bool Studio::beginExport (const juce::File& destination, const ExportOptions& options, juce::String& error)
    {
        SessionLog::note ("Rendering to " + destination.getFullPathName());
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
        // recordings are loaded again at it), stopped (writing automation ends
        // with it, as when the song stops).
        punchOutAll();
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
        engine.releaseNotes();   // (what the last render or the playback left sounding doesn't carry into this one)
        // Offline: only the plugins that will sound (0.9.9): the tracks with
        // parts in what's played, the channels they reach, the master's.
        std::set<int> sounding;
        for (const auto& track : playing.tracks)
            if (! track.isChannel() && ! track.disabled
                && std::any_of (track.parts.begin(), track.parts.end(), [] (const Part& p) { return ! p.muted; }))
                sounding.insert (track.id);
        for (auto grew = true; grew;)
        {
            grew = false;
            for (const auto& track : playing.tracks)
                if (sounding.count (track.id) > 0)
                {
                    std::vector<int> reaches { track.output };
                    for (const auto& send : track.sends)
                        if (send.on)
                            reaches.push_back (send.target);
                    for (const auto id : reaches)
                        if (id > 0 && sounding.insert (id).second)
                            grew = true;
                }
        }
        engine.setNonRealtime (true, [&sounding] (int id) { return sounding.count (id) > 0; });   // (set up again for it)
        engine.settleMix();   // the levels set from the first sample (no glide from what was playing before)

        job->total = (juce::int64) std::llround (project.tempoMap().secondsBetween (options.start, options.end) * rate);
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
            if (renderCancelled)
                return false;   // (looked at every block: an instrument rendering slowly still stops at once)
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
        SessionLog::note ("  Render done.");
        auto job = std::move (exportJob);
        engine.stop();
        job->writer.reset();   // the file finished
        if (! inBatch)   // (several renders in a row: back to realtime once, after the last)
            engine.setNonRealtime (false);
        if (! job->finished)
            job->file.deleteFile();
        // Everything as it was.
        if (! juce::approximatelyEqual (engine.getSampleRate(), job->previousRate))
            engine.prepare (job->previousRate, job->previousBlockSize);
        engine.update (project);
        engine.settleMix();   // (0.9.8: the song's own levels from the first sample, no glide from the export's copy)
        engine.setPosition (job->previousPosition);
        if (withDevices && ! inBatch)
            devices.addAudioCallback (&engine);
        sendChangeMessage();
    }

    void Studio::beginBatch()
    {
        inBatch = true;
        renderCancelled = false;
    }

    void Studio::endBatch()
    {
        if (! inBatch)
            return;
        inBatch = false;
        engine.setNonRealtime (false);
        engine.settleMix();
        if (withDevices)
            devices.addAudioCallback (&engine);
    }

    void Studio::setProgressStage (double base, double span, const juce::String& what)
    {
        progressBase = base;
        progressSpan = span;
        progressWhat = what;
        if (onProgress)
            onProgress (base, what);
    }

    void Studio::reportRenderProgress()
    {
        if (onProgress)
            onProgress (progressBase + progressSpan * renderDone.load(), progressWhat);
    }

    void Studio::endProgress()
    {
        progressBase = 0.0;
        progressSpan = 1.0;
        progressWhat = {};
        if (onProgress)
            onProgress (-1.0, {});
    }

    bool Studio::exportMix (const juce::File& destination, const ExportOptions& options, juce::String& error)
    {
        if (! beginExport (destination, options, error))
            return false;
        // Render in Place, Freeze: run by the renderRunner (the app: on a
        // thread of its own); its progress noted for the screen to show.
        rendering = true;
        renderDone = 0.0;
        if (! inBatch)
            renderCancelled = false;   // (in a batch, one CANCEL stops the rest too)
        const auto threaded = renderRunner != nullptr;
        const auto work = [this, threaded]
        {
            // (The render's thread is an audio thread too: plugins' own changes aren't
            // posted. Put back after: a runner may run it where it is.)
            const auto wasAudio = SessionLog::isAudioThread();
            SessionLog::markAudioThread (threaded || wasAudio);
            const juce::ScopeGuard restore { [wasAudio] { SessionLog::markAudioThread (wasAudio); } };
            return renderExport ([this, threaded] (double done)
            {
                renderDone = done;
                if (! threaded && onProgress)
                    onProgress (progressBase + progressSpan * done, progressWhat);
                return true;
            });
        };
        SessionLog::note (threaded ? "  Rendering on a thread of its own" : "  Rendering");
        const auto rendered = threaded ? renderRunner (work) : work();
        rendering = false;
        endExport();
        if (! rendered)
            error = renderCancelled ? juce::String ("Cancelled.") : "Couldn't write " + destination.getFullPathName();
        return rendered;
    }

    std::vector<int> Studio::renderInPlace (const std::vector<RenderSource>& sources, const RenderOptions& options, juce::String& error)
    {
        using P = RenderOptions::Processing;
        beginBatch();   // (the plugins offline once for all the stretches)
        const juce::ScopeGuard batch { [this] { endBatch(); } };
        std::vector<int> made;
        juce::StringArray problems;
        const auto folder = getAudioFolder();
        folder.createDirectory();
        const auto tailSeconds = options.tail == RenderOptions::Tail::off ? 0.0
                               : options.tail == RenderOptions::Tail::bars ? options.tailSize * project.beatsPerBar() * 60.0 / project.bpmAt (project.lastBeat())
                                                                           : options.tailSize;
        const juce::ScopeGuard progressDone { [this] { endProgress(); } };
        for (size_t index = 0; index < sources.size(); ++index)
        {
            const auto& source = sources[index];
            const auto* track = project.findTrack (source.trackId);
            if (track == nullptr || track->isChannel() || source.end <= source.start)
                continue;
            setProgressStage ((double) index / (double) sources.size(), 1.0 / (double) sources.size(),
                              "Rendering " + track->name + (sources.size() > 1 ? " (" + juce::String ((int) index + 1) + " of " + juce::String ((int) sources.size()) + ")" : juce::String()));
            // The stretches to render, from the parts in it (cut at its edges).
            std::vector<std::pair<double, double>> spans;
            for (const auto& part : track->parts)
                if (! part.muted)
                    if (const auto piece = slice (part, source.start, source.end, project.tempoMap()))
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
                render.prepareSong = [sourceId, from = from, to = to, processing, tempo = project.tempoMap()] (Project& song)
                {
                    for (auto& t : song.tracks)
                    {
                        t.solo = false;
                        if (t.id != sourceId)
                        {
                            silenceForRender (t);   // the other tracks: silent (FX and group channels still take what's sent or routed)
                            continue;
                        }
                        std::vector<Part> kept;
                        for (const auto& part : t.parts)
                            if (auto piece = slice (part, from, to, tempo))
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
                clip.length = project.tempoMap().beatAfter (from, data->seconds()) - from;
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

            // A soloed source going out of earshot (muted, disabled, hidden, removed): its solo
            // goes to the rendered track, else that one, and every other, stays silent (0.9.9).
            if (auto* from = project.findTrack (sourceId); from != nullptr && from->solo
                && (options.source != RenderOptions::Source::keep || (source.wholeTrack && options.hideSource)))
            {
                project.findTrack (newId)->solo = true;
                from->solo = false;
            }
            // The source: kept, muted (the parts rendered, or the whole track), or removed (a whole track).
            if (options.source == RenderOptions::Source::mute)
            {
                if (source.wholeTrack)
                    project.findTrack (sourceId)->mute = true;
                else
                {
                    auto* t = project.findTrack (sourceId);
                    splitTrackAt (*t, source.start, project.tempoMap());
                    splitTrackAt (*t, source.end, project.tempoMap());
                    for (auto& part : t->parts)
                        if (inside (part, source.start, source.end))
                            part.muted = true;
                }
            }
            changed();
            if (source.wholeTrack && options.source == RenderOptions::Source::disable)
                setTracksDisabled ({ sourceId }, true);
            if (source.wholeTrack && options.hideSource && options.source != RenderOptions::Source::remove)
                setTracksHidden ({ sourceId }, true);
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
        SessionLog::note ("Opening " + source.getFullPathName());
        const auto xml = juce::XmlDocument::parse (source);
        if (xml == nullptr)
            return false;
        auto loaded = Project::fromXml (*xml);
        if (! loaded)
            return false;
        openProject (std::move (*loaded), source, problems);
        library->addRecent (source);
        return true;
    }

    bool Studio::importDawProject (const juce::File& source, juce::String& report)
    {
        SessionLog::note ("Importing the DAWproject " + source.getFullPathName());
        // A new song (not saved yet), its audio in a folder of its own.
        const auto folder = unsavedAudioFolder.getParentDirectory().getNonexistentChildFile (source.getFileNameWithoutExtension(), {}, false);
        juce::String error;
        auto imported = DawProject::read (source, folder, host->getKnownPlugins().getTypes(), error);
        if (! imported)
        {
            report = error;
            return false;
        }
        unsavedAudioFolder = folder;
        juce::String problems;
        openProject (std::move (imported->project), {}, problems);

        // Plug-in knobs' automation: matched to the loaded plug-ins' parameters by their VST3 id.
        juce::StringArray notes = imported->notes;
        for (auto& pending : imported->pluginLanes)
        {
            auto& lane = pending.lane;
            auto* instance = lane.target == AutomationLane::Target::instrument ? engine.getInstrument (pending.busId)
                                                                               : engine.getEffect (pending.busId, lane.slot);
            auto index = -1;
            if (instance != nullptr)
            {
                const auto& params = instance->getParameters();
                for (int i = 0; i < params.size(); ++i)
                    if (auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*> (params[i]); hosted != nullptr && hosted->getParameterID() == pending.parameterId)
                        index = i;
            }
            if (index < 0)
            {
                notes.addIfNotAlreadyThere ("Automation of a plug-in's knob" + (lane.parameterName.isNotEmpty() ? " (" + lane.parameterName + ")" : juce::String())
                                            + " is left out: the plug-in isn't loaded, or hasn't that knob.");
                continue;
            }
            lane.parameter = index;
            if (lane.parameterName.isEmpty())
                lane.parameterName = instance->getParameters()[index]->getName (64);
            if (pending.busId == Engine::masterBus)
            {
                project.masterAutomation.push_back (lane);
                project.masterRead = true;
            }
            else if (auto* track = project.findTrack (pending.busId))
            {
                track->automation.push_back (lane);
                track->automationRead = true;
            }
        }
        engine.update (project);
        undoStates.clear();
        undoIndex = -1;
        recordUndoState();
        modified = true;   // (a new song: not saved yet)
        if (problems.isNotEmpty())
            notes.add (problems);
        report = notes.joinIntoString ("\n");
        sendChangeMessage();
        return true;
    }

    void Studio::openProject (Project&& loadedProject, const juce::File& source, juce::String& problems)
    {
        std::optional<Project> loaded { std::move (loadedProject) };
        punchOutAll();

        if (onInstrumentGoing)
            onInstrumentGoing (0);
        removeAllEffects();
        engine.stop();
        for (const auto& track : project.tracks)
            engine.removeTrack (track.id);
        project = std::move (*loaded);
        padsChanged();

        file = source;
        audioCache.clear();
        juce::StringArray missing;
        // Each plug-in loaded is a step of the progress box (0.11.1: Kontakt and its kind take a while).
        auto toLoad = 0, nowLoading = 0;
        for (const auto& track : project.tracks)
            if (! track.disabled && ! track.frozen)
            {
                toLoad += track.isAudio() || track.isChannel() ? 0 : 1;
                for (const auto& insert : track.inserts)
                    toLoad += insert.isEmpty() ? 0 : 1;
            }
        for (const auto& insert : project.masterInserts)
            toLoad += insert.isEmpty() ? 0 : 1;
        const auto loading = [&] (const juce::String& what)
        {
            if (toLoad > 0)
                setProgressStage ((double) nowLoading / (double) toLoad, 1.0 / (double) toLoad, what);
            ++nowLoading;
        };
        const juce::ScopeGuard progressDone { [&] { if (toLoad > 0) endProgress(); } };
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
            if (track.disabled || track.frozen)
            {
                engine.setInstrument (track.id, nullptr);   // disabled or frozen: its plugins aren't loaded (their settings kept)
                continue;
            }
            loading ("Loading " + track.instrument.getName() + " (" + track.name + ")");
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
                loading ("Loading " + inserts[(size_t) slot].plugin.name + " (" + where + ")");
                auto instance = host->createEffect (inserts[(size_t) slot], engine.getSampleRate(), engine.getBlockSize(), error);
                if (instance == nullptr)
                    missing.add (where + ", insert " + juce::String (slot + 1) + " (" + inserts[(size_t) slot].plugin.name + "): " + error);
                engine.insertEffect (busId, slot, std::move (instance));
            }
        };
        for (const auto& track : project.tracks)
            if (! track.disabled && ! track.frozen)
                loadEffects (track.id, track.name, track.inserts);
        loadEffects (Engine::masterBus, "Master", project.masterInserts);
        problems = missing.joinIntoString ("\n");
        numberParts();
        // Frozen tracks' parts locked as they were saved.
        frozenParts.clear();
        for (const auto& track : project.tracks)
            if (track.frozen)
                frozenParts[track.id] = track.parts;

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
    }

    //==========================================================================
    // Chord pads (0.14.0).
    void Studio::padsChanged()
    {
        padPlayer.setPads (project.chordPads);
    }

    void Studio::setChordPads (const ChordPads& pads)
    {
        if (pads == project.chordPads)
            return;
        project.chordPads = pads;
        padsChanged();
        modified = true;
        sendChangeMessage();
    }

    void Studio::setPadsActive (bool on)
    {
        if (on == padsActive)
            return;
        padsActive = on;
        if (! on)
            padPlayer.releaseAll();
        engine.setPadPlayer (on ? &padPlayer : nullptr);
    }

    std::vector<int> Studio::unlocked (const std::vector<int>& pads) const
    {
        std::vector<int> result;
        for (const auto p : pads)
            if (juce::isPositiveAndBelow (p, (int) project.chordPads.pads.size()) && ! project.chordPads.pads[(size_t) p].locked)
                result.push_back (p);
        return result;
    }

    void Studio::assignChord (int pad, const Chord& chord)
    {
        if (unlocked ({ pad }).empty())
            return;
        auto pads = project.chordPads;
        auto& p = pads.pads[(size_t) pad];
        p.chord = chord;
        p.notes.clear();
        p.voicing = -1;
        p.adaptive = true;
        setChordPads (pads);
    }

    void Studio::removeChords (const std::vector<int>& which)
    {
        auto pads = project.chordPads;
        for (const auto p : unlocked (which))
        {
            pads.pads[(size_t) p] = {};
            if (pads.adaptiveReference == p)
                pads.adaptiveReference = -1;
        }
        setChordPads (pads);
    }

    void Studio::transposePads (const std::vector<int>& which, int semitones)
    {
        auto pads = project.chordPads;
        for (const auto p : unlocked (which))
        {
            auto& pad = pads.pads[(size_t) p];
            if (! pad.chord)
                continue;
            pad.chord = pad.chord->transposed (semitones);
            for (auto& n : pad.notes)
                n = juce::jlimit (0, 127, n + semitones);
        }
        setChordPads (pads);
    }

    void Studio::changePadTensions (const std::vector<int>& which, int direction)
    {
        auto pads = project.chordPads;
        for (const auto p : unlocked (which))
        {
            auto& pad = pads.pads[(size_t) p];
            if (! pad.chord)
                continue;
            pad.chord = pad.chord->withTensionStep (direction);
            pad.notes.clear();   // (a voicing played in was of the old chord)
        }
        setChordPads (pads);
    }

    int Studio::getPadVoicingCount (int pad) const
    {
        if (! juce::isPositiveAndBelow (pad, (int) project.chordPads.pads.size()) || ! project.chordPads.pads[(size_t) pad].chord)
            return 0;
        return (int) voicingsFor (*project.chordPads.pads[(size_t) pad].chord, 48 + 12 * project.chordPads.octaveOffset).size();
    }

    int Studio::getPadVoicing (int pad) const
    {
        if (getPadVoicingCount (pad) == 0)
            return -1;
        const auto& p = project.chordPads.pads[(size_t) pad];
        const auto voicings = voicingsFor (*p.chord, 48 + 12 * project.chordPads.octaveOffset);
        const auto notes = getPadNotes (pad);
        for (size_t i = 0; i < voicings.size(); ++i)
            if (voicings[i] == notes)
                return (int) i;
        return -1;   // (a voicing played in)
    }

    std::vector<int> Studio::getPadNotes (int pad) const
    {
        return project.chordPads.notesFor (pad, padPlayer.getLastChord());
    }

    void Studio::changePadVoicings (const std::vector<int>& which, int direction)
    {
        // The manual: a voicing set by hand turns the pad's adaptive voicing off.
        auto pads = project.chordPads;
        for (const auto p : unlocked (which))
        {
            const auto count = getPadVoicingCount (p);
            if (count == 0)
                continue;
            auto now = getPadVoicing (p);
            if (now < 0)
                now = count / 2;
            auto& pad = pads.pads[(size_t) p];
            pad.voicing = juce::jlimit (0, count - 1, now + (direction > 0 ? 1 : -1));
            pad.adaptive = false;
            pad.notes.clear();
        }
        setChordPads (pads);
    }

    void Studio::setPadsLocked (const std::vector<int>& which, bool locked)
    {
        auto pads = project.chordPads;
        for (const auto p : which)
            if (juce::isPositiveAndBelow (p, (int) pads.pads.size()))
            {
                pads.pads[(size_t) p].locked = locked;
                if (locked)
                    pads.pads[(size_t) p].adaptive = false;   // (the manual: Lock deactivates Adaptive Voicing)
            }
        setChordPads (pads);
    }

    void Studio::setPadsAdaptive (const std::vector<int>& which, bool on)
    {
        auto pads = project.chordPads;
        for (const auto p : unlocked (which))
        {
            pads.pads[(size_t) p].adaptive = on;
            if (on)
                pads.pads[(size_t) p].notes.clear();
        }
        setChordPads (pads);
    }

    void Studio::setAdaptiveReference (int pad)
    {
        auto pads = project.chordPads;
        pads.adaptiveReference = juce::isPositiveAndBelow (pad, (int) pads.pads.size()) ? pad : -1;
        setChordPads (pads);
    }

    void Studio::swapPads (int a, int b)
    {
        auto pads = project.chordPads;
        if (a == b || ! juce::isPositiveAndBelow (a, (int) pads.pads.size()) || ! juce::isPositiveAndBelow (b, (int) pads.pads.size())
            || pads.pads[(size_t) a].locked || pads.pads[(size_t) b].locked)
            return;
        std::swap (pads.pads[(size_t) a], pads.pads[(size_t) b]);
        setChordPads (pads);
    }

    void Studio::copyPad (int from, int to)
    {
        auto pads = project.chordPads;
        if (from == to || ! juce::isPositiveAndBelow (from, (int) pads.pads.size()) || unlocked ({ to }).empty())
            return;
        pads.pads[(size_t) to] = pads.pads[(size_t) from];
        pads.pads[(size_t) to].locked = false;
        setChordPads (pads);
    }

    void Studio::setPadsRootKey (int key, bool transpose)
    {
        key = ((key % 12) + 12) % 12;
        if (transpose)
        {
            // The shorter way round (up to a fifth either way).
            auto by = ((key - project.chordPads.rootKey) % 12 + 12) % 12;
            if (by > 6)
                by -= 12;
            std::vector<int> all;
            for (int i = 0; i < (int) project.chordPads.pads.size(); ++i)
                all.push_back (i);
            transposePads (all, by);
        }
        auto pads = project.chordPads;
        pads.setKey (Key::fromPitchClass (key, pads.minor, pads.flats));   // (spelled as that key usually is)
        setChordPads (pads);
    }

    bool Studio::assignFromNotes (int pad, const std::vector<int>& notes)
    {
        const auto chord = recogniseChord (notes);
        if (! chord || unlocked ({ pad }).empty())
            return false;
        auto pads = project.chordPads;
        auto& p = pads.pads[(size_t) pad];
        p.chord = *chord;
        p.notes = notes;   // "the chord and the voicing are assigned" (the manual)
        std::sort (p.notes.begin(), p.notes.end());
        p.adaptive = false;
        p.voicing = -1;
        setChordPads (pads);
        return true;
    }

    void Studio::setAssignFromInput (std::vector<int> pads)
    {
        assignTargets = std::move (pads);
        assignHeld.clear();
        assignNotes.clear();
        sendChangeMessage();
    }

    void Studio::padKeyPlayed (int note, bool on)
    {
        // A chord played in: the keys down together, assigned when the last is let go.
        if (assignTargets.empty())
            return;
        if (on)
        {
            assignHeld.push_back (note);
            if (std::find (assignNotes.begin(), assignNotes.end(), note) == assignNotes.end())
                assignNotes.push_back (note);
            return;
        }
        assignHeld.erase (std::remove (assignHeld.begin(), assignHeld.end(), note), assignHeld.end());
        if (! assignHeld.empty())
            return;
        const auto pad = assignTargets.front();
        if (assignFromNotes (pad, assignNotes))
        {
            assignTargets.erase (assignTargets.begin());   // the next selected pad takes the next chord
            auditionPad (pad);
        }
        assignNotes.clear();
        sendChangeMessage();
    }

    void Studio::auditionPad (int pad)
    {
        padPlayer.press (pad, 100);
        juce::Timer::callAfterDelay (450, [weak = std::weak_ptr<bool> (alive), this, pad]
        {
            if (weak.lock() != nullptr)
                padPlayer.release (pad);
        });
    }

    int Studio::addPartFromPad (int pad, int trackId, double beat)
    {
        const auto* track = project.findTrack (trackId);
        const auto notes = getPadNotes (pad);
        if (track == nullptr || track->isAudio() || track->isChannel() || track->frozen || notes.empty())
            return 0;
        const auto bar = project.beatsPerBar();
        Part part;
        part.start = juce::jmax (0.0, std::floor (beat / bar + 1.0e-9) * bar);
        part.length = bar;
        part.name = project.chordPads.pads[(size_t) pad].chord->name (project.chordPads.flats);
        for (const auto n : notes)
            part.notes.push_back ({ 0.0, bar, n, 100.0f / 127.0f });
        addPart (trackId, part);
        const auto* now = project.findTrack (trackId);
        return now != nullptr && ! now->parts.empty() ? now->parts.back().id : 0;
    }

    void Studio::loadScalePreset (ChordPads::Scale scale, int key, PadsPresetLoad what)
    {
        auto loaded = project.chordPads;
        loaded.fillFromScale (scale, key);
        if (what == PadsPresetLoad::playersOnly)
            return;   // (a scale preset has no player of its own)
        auto pads = project.chordPads;
        pads.pads = loaded.pads;
        pads.rootKey = loaded.rootKey;
        pads.adaptiveReference = -1;
        setChordPads (pads);
    }

    juce::File Studio::chordPadsPresetFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Paragon PR7-S").getChildFile ("Chord Pads");
    }

    bool Studio::saveChordPadsPreset (const juce::File& destination) const
    {
        destination.getParentDirectory().createDirectory();
        auto xml = project.chordPads.toXml();
        xml->setAttribute ("preset", true);
        return xml->writeTo (destination);
    }

    bool Studio::loadChordPadsPreset (const juce::File& source, PadsPresetLoad what)
    {
        const auto xml = juce::XmlDocument::parse (source);
        if (xml == nullptr || ! xml->hasTagName ("ChordPads"))
            return false;
        const auto loaded = ChordPads::fromXml (*xml);
        auto pads = project.chordPads;
        if (what != PadsPresetLoad::playersOnly)
        {
            pads.pads = loaded.pads;
            pads.rootKey = loaded.rootKey;
            pads.flats = loaded.flats;
            pads.adaptiveReference = loaded.adaptiveReference;
        }
        if (what != PadsPresetLoad::chordsOnly)
        {
            // The player: its octave and how chords overlap (the Basic player's settings).
            pads.octaveOffset = loaded.octaveOffset;
            pads.overlap = loaded.overlap;
        }
        setChordPads (pads);
        return true;
    }

    void Studio::setPadsKey (const Key& key, PadsKeyChange change)
    {
        auto pads = project.chordPads;
        if (change == PadsKeyChange::fillWithChords)
            pads.fillFromKey (key);
        else
        {
            if (change == PadsKeyChange::transposePads)
            {
                auto by = ((key.pitchClass() - pads.rootKey) % 12 + 12) % 12;
                if (by > 6)
                    by -= 12;
                for (auto& pad : pads.pads)
                    if (! pad.locked && pad.chord)
                    {
                        pad.chord = pad.chord->transposed (by);
                        for (auto& n : pad.notes)
                            n = juce::jlimit (0, 127, n + by);
                    }
            }
            pads.setKey (key);
        }
        setChordPads (pads);
    }

    void Studio::auditionChord (const Chord& chord)
    {
        const auto voicings = namedVoicings (chord, 48 + 12 * project.chordPads.octaveOffset);
        if (voicings.empty())
            return;
        std::vector<int> notes = voicings.front().notes;
        for (const auto& v : voicings)
            if (v.close && v.notes.front() >= 53)   // (root position near the middle of the keyboard)
            {
                notes = v.notes;
                break;
            }
        for (const auto n : notes)
            padPlayer.send (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100));
        padPlayer.setLastChord (notes);
        juce::Timer::callAfterDelay (450, [weak = std::weak_ptr<bool> (alive), this, notes]
        {
            if (weak.lock() != nullptr)
                for (const auto n : notes)
                    padPlayer.send (juce::MidiMessage::noteOff (1, n));
        });
    }

    void Studio::setPadVoicing (int pad, int voicing)
    {
        if (unlocked ({ pad }).empty() || getPadVoicingCount (pad) == 0)
            return;
        auto pads = project.chordPads;
        auto& p = pads.pads[(size_t) pad];
        p.voicing = juce::jlimit (0, getPadVoicingCount (pad) - 1, voicing);
        p.adaptive = false;
        p.notes.clear();
        setChordPads (pads);
    }

    //==========================================================================
    // The chord track (0.16.0).
    void Studio::chordTrackChanged()
    {
        project.chordTrack.sort();
        if (project.chordTrack.autoScales)
            project.chordTrack.scales = project.chordTrack.events.empty() ? std::vector<ScaleEvent> {}
                                      : scalesFromChords (project.chordTrack.events, project.chordPads.key());
        changed();
    }

    void Studio::addChordTrack()
    {
        if (project.chordTrack.shown)
            return;
        project.chordTrack.shown = true;
        changed();
    }

    void Studio::removeChordTrack()
    {
        if (! project.chordTrack.shown)
            return;
        project.chordTrack.shown = false;
        changed();
    }

    int Studio::addChordEvent (double beat, const Chord& chord)
    {
        beat = juce::jmax (0.0, beat);
        project.chordTrack.shown = true;
        auto& events = project.chordTrack.events;
        events.erase (std::remove_if (events.begin(), events.end(), [beat] (const ChordEvent& e) { return std::abs (e.beat - beat) < 1.0e-6; }),
                      events.end());
        events.push_back ({ beat, chord });
        chordTrackChanged();
        for (int i = 0; i < (int) events.size(); ++i)
            if (std::abs (events[(size_t) i].beat - beat) < 1.0e-6)
                return i;
        return -1;
    }

    //==========================================================================
    // Bass and melody ideas (0.18.0)
    std::vector<ChordSpan> Studio::getLineChords (const std::vector<int>& selectedEvents) const
    {
        const auto& track = project.chordTrack;
        std::vector<int> which;
        for (const auto i : selectedEvents)
            if (juce::isPositiveAndBelow (i, (int) track.events.size()))
                which.push_back (i);
        if (which.empty() && project.loopOn)
            for (int i = 0; i < (int) track.events.size(); ++i)
                if (track.events[(size_t) i].beat >= project.loopStart - 1.0e-6 && track.events[(size_t) i].beat < project.loopEnd - 1.0e-6)
                    which.push_back (i);
        if (which.empty())
            for (int i = 0; i < (int) track.events.size(); ++i)
                which.push_back (i);
        std::sort (which.begin(), which.end());
        std::vector<ChordSpan> spans;
        for (const auto i : which)
        {
            const auto& e = track.events[(size_t) i];
            auto end = track.endOf (i, project.beatsPerBar());
            if (project.loopOn && e.beat < project.loopEnd && end > project.loopEnd && selectedEvents.empty())
                end = project.loopEnd;   // (the cycle's chords stop at its end)
            spans.push_back ({ e.beat, end, e.chord, chordTrackKeyAt (e.beat) });
        }
        return spans;
    }

    juce::String Studio::linePartName (LineIdea::Part part)
    {
        switch (part)
        {
            case LineIdea::Part::bass: return "Bass";
            case LineIdea::Part::melody: return "Melody";
            case LineIdea::Part::chords: return "Piano";
            case LineIdea::Part::rhythm: return "Guitar";
            case LineIdea::Part::lead: return "Lead Guitar";
        }
        return "Idea";
    }

    int Studio::addLinePart (const LineIdea& idea, const std::vector<ChordSpan>& spans, int ontoTrackId)
    {
        if (spans.empty() || idea.notes.empty())
            return 0;
        auto trackId = canTakeMidi (ontoTrackId) ? ontoTrackId : 0;
        if (trackId == 0)
        {
            trackId = addTrack (Instrument {});
            if (trackId == 0)
                return 0;
            renameTrack (trackId, linePartName (idea.part));
        }
        Part part;
        part.start = spans.front().start;
        part.length = spans.back().end - spans.front().start;
        part.name = linePartName (idea.part) + ": " + idea.style;
        for (const auto& n : idea.notes)
            part.notes.push_back ({ n.beat, n.length, n.pitch, n.velocity });
        addPart (trackId, part);
        return trackId;
    }

    juce::String Studio::getGuitarTuning() const
    {
        const auto name = settings != nullptr ? settings->getValue ("guitarTuning") : guitarTuning;
        return GuitarTuning::named (name).name;   // (unknown or empty: the first, 7-string drop A)
    }

    GuitarVelocities Studio::getGuitarVelocities() const
    {
        if (settings != nullptr && settings->containsKey ("guitarVelocities"))
            return GuitarVelocities::fromString (settings->getValue ("guitarVelocities"));
        return guitarVelocities;
    }

    void Studio::setGuitarVelocities (const GuitarVelocities& v)
    {
        guitarVelocities = v;
        if (settings != nullptr)
            settings->setValue ("guitarVelocities", v.toString());
    }

    float Studio::getIdeasHumanize() const
    {
        return settings != nullptr ? juce::jlimit (0.0f, 1.0f, (float) settings->getDoubleValue ("ideasHumanize", ideasHumanize)) : ideasHumanize;
    }

    void Studio::setIdeasHumanize (float amount)
    {
        ideasHumanize = juce::jlimit (0.0f, 1.0f, amount);
        if (settings != nullptr)
            settings->setValue ("ideasHumanize", (double) ideasHumanize);
    }

    bool Studio::getIdeasFeel() const
    {
        return settings != nullptr ? settings->getBoolValue ("ideasFeel", ideasFeel) : ideasFeel;
    }

    void Studio::setIdeasFeel (bool on)
    {
        ideasFeel = on;
        if (settings != nullptr)
            settings->setValue ("ideasFeel", on);
    }

    float Studio::getHumanizeAmount() const
    {
        return settings != nullptr ? juce::jlimit (0.05f, 1.0f, (float) settings->getDoubleValue ("humanizeAmount", humanizeAmount)) : humanizeAmount;
    }

    void Studio::setHumanizeAmount (float amount)
    {
        humanizeAmount = juce::jlimit (0.05f, 1.0f, amount);
        if (settings != nullptr)
            settings->setValue ("humanizeAmount", (double) humanizeAmount);
    }

    bool Studio::getHumanizeKeepsLayers() const
    {
        return settings != nullptr ? settings->getBoolValue ("humanizeLayers", humanizeLayers) : humanizeLayers;
    }

    void Studio::setHumanizeKeepsLayers (bool on)
    {
        humanizeLayers = on;
        if (settings != nullptr)
            settings->setValue ("humanizeLayers", on);
    }

    void Studio::setGuitarTuning (const juce::String& name)
    {
        guitarTuning = GuitarTuning::named (name).name;
        if (settings != nullptr)
            settings->setValue ("guitarTuning", guitarTuning);
    }

    // The preview: its notes in time on a thread of its own (a millisecond timer), sent
    // as the pads' are into the live track's input.
    class Studio::LinePreview final : private juce::HighResolutionTimer
    {
    public:
        explicit LinePreview (PadPlayer& p) : player (p) {}
        ~LinePreview() override { stop(); }

        struct Event { double ms; juce::MidiMessage message; };
        void start (std::vector<Event> list, double beatsPerMs)
        {
            stop();
            events = std::move (list);
            std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.ms < b.ms; });
            for (const auto& e : events)
                if (e.message.isNoteOn())
                    used.insert (e.message.getNoteNumber());
            next = 0;
            rate = beatsPerMs;
            started = juce::Time::getMillisecondCounterHiRes();
            running = true;
            startTimer (2);
        }
        void stop()
        {
            stopTimer();
            if (running.exchange (false) || ! used.empty())
                for (const auto n : used)
                    player.send (juce::MidiMessage::noteOff (1, n));
            used.clear();
        }
        bool isRunning() const noexcept { return running; }
        double beat() const noexcept { return running ? (juce::Time::getMillisecondCounterHiRes() - started) * rate : 0.0; }

    private:
        void hiResTimerCallback() override
        {
            const auto now = juce::Time::getMillisecondCounterHiRes() - started;
            while (next < events.size() && events[next].ms <= now)
                player.send (events[next++].message);
            if (next >= events.size())
            {
                running = false;
                stopTimer();
            }
        }

        PadPlayer& player;
        std::vector<Event> events;
        std::set<int> used;
        size_t next = 0;
        double started = 0.0, rate = 0.0;
        std::atomic<bool> running { false };
    };

    void Studio::previewLine (const LineIdea* idea, const std::vector<ChordSpan>& spans, bool withChords)
    {
        if (linePreview == nullptr)
            linePreview = std::make_unique<LinePreview> (padPlayer);
        if (spans.empty())
            return;
        const auto map = project.tempoMap();
        const auto first = spans.front().start;
        const auto ms = [&map, first] (double relativeBeat) { return map.secondsBetween (first, first + relativeBeat) * 1000.0; };
        std::vector<LinePreview::Event> events;
        const auto note = [&] (double at, double length, int pitch, float velocity)
        {
            events.push_back ({ ms (at), juce::MidiMessage::noteOn (1, pitch, (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (velocity * 127.0f))) });
            events.push_back ({ ms (at + length) - 1.0, juce::MidiMessage::noteOff (1, pitch) });
        };
        if (withChords)
        {
            // The chords underneath, softly, each voiced near the one before.
            std::vector<int> previous;
            for (const auto& s : spans)
            {
                const auto voicings = voicingsFor (s.chord, 48);
                if (voicings.empty())
                    continue;
                const auto& notes = voicings[(size_t) closestVoicing (voicings, previous, {})];
                for (const auto n : notes)
                    note (s.start - first, s.end - s.start, n, 0.5f);   // (64: soft, and above a guitar library's palm-mute layers)
                previous = notes;
            }
        }
        if (idea != nullptr)
            for (const auto& n : idea->notes)
                note (n.beat, n.length, n.pitch, n.velocity);
        linePreview->start (std::move (events), project.bpmAt (first) / 60000.0);
    }

    void Studio::stopPreview()
    {
        if (linePreview != nullptr)
            linePreview->stop();
    }

    bool Studio::isPreviewing() const noexcept
    {
        return linePreview != nullptr && linePreview->isRunning();
    }

    double Studio::getPreviewBeat() const noexcept
    {
        return linePreview != nullptr ? linePreview->beat() : 0.0;
    }

    void Studio::addProgressionToChordTrack (const std::vector<Chord>& chords, double atBeat)
    {
        if (chords.empty())
            return;
        const auto bar = project.beatsPerBar();
        const auto start = std::floor (juce::jmax (0.0, atBeat) / bar + 1.0e-9) * bar;
        const auto end = start + bar * (double) chords.size();
        // What was over those bars goes; then a chord a bar.
        std::vector<ChordEvent> events;
        for (const auto& e : project.chordTrack.events)
            if (e.beat < start - 1.0e-6 || e.beat >= end - 1.0e-6)
                events.push_back (e);
        for (size_t i = 0; i < chords.size(); ++i)
            events.push_back ({ start + bar * (double) i, chords[i] });
        std::sort (events.begin(), events.end(), [] (const ChordEvent& a, const ChordEvent& b) { return a.beat < b.beat; });
        project.chordTrack.shown = true;   // (in the same undo step)
        project.chordTrack.events = std::move (events);
        chordTrackChanged();
    }

    int Studio::assignProgressionToPads (const std::vector<Chord>& chords, int firstPad)
    {
        auto pads = project.chordPads;
        auto placed = 0;
        for (int pad = juce::jmax (0, firstPad); pad < (int) pads.pads.size() && placed < (int) chords.size(); ++pad)
        {
            auto& p = pads.pads[(size_t) pad];
            if (p.locked)
                continue;
            p.chord = chords[(size_t) placed++];
            p.notes.clear();
            p.voicing = -1;
            p.adaptive = true;
        }
        if (placed > 0)
            setChordPads (pads);
        return placed;
    }

    void Studio::setChordEvents (std::vector<ChordEvent> events)
    {
        for (auto& e : events)
            e.beat = juce::jmax (0.0, e.beat);
        // (Two at one beat: the later in the list wins.)
        std::vector<ChordEvent> kept;
        for (auto it = events.rbegin(); it != events.rend(); ++it)
            if (std::none_of (kept.begin(), kept.end(), [&it] (const ChordEvent& k) { return std::abs (k.beat - it->beat) < 1.0e-6; }))
                kept.push_back (*it);
        if (kept == project.chordTrack.events)
            return;
        project.chordTrack.events = std::move (kept);
        chordTrackChanged();
    }

    void Studio::setChordEventChord (int index, const Chord& chord)
    {
        if (! juce::isPositiveAndBelow (index, (int) project.chordTrack.events.size()) || project.chordTrack.events[(size_t) index].chord == chord)
            return;
        project.chordTrack.events[(size_t) index].chord = chord;
        chordTrackChanged();
    }

    void Studio::removeChordEvents (std::vector<int> indices)
    {
        std::sort (indices.rbegin(), indices.rend());
        indices.erase (std::unique (indices.begin(), indices.end()), indices.end());
        auto& events = project.chordTrack.events;
        auto any = false;
        for (const auto i : indices)
            if (juce::isPositiveAndBelow (i, (int) events.size()))
            {
                events.erase (events.begin() + i);
                any = true;
            }
        if (any)
            chordTrackChanged();
    }

    void Studio::setScaleEvents (std::vector<ScaleEvent> scales)
    {
        project.chordTrack.scales = std::move (scales);
        project.chordTrack.autoScales = false;
        chordTrackChanged();
    }

    void Studio::setAutoScales (bool on)
    {
        if (project.chordTrack.autoScales == on)
            return;
        project.chordTrack.autoScales = on;
        chordTrackChanged();
    }

    Key Studio::chordTrackKeyAt (double beat) const
    {
        return project.chordTrack.keyAt (beat, project.chordPads.key());
    }

    Chord Studio::suggestChordAt (double beat) const
    {
        const auto key = chordTrackKeyAt (beat);
        const auto before = project.chordTrack.indexAt (beat - 1.0e-6);
        if (before >= 0)
        {
            const auto next = suggestNext (project.chordTrack.events[(size_t) before].chord, key);
            if (! next.empty())
                return next.front().chord;
        }
        return keyChords (key).front();   // the key's I (or i)
    }

    int Studio::chordsToMidi (int trackId, std::vector<int> indices)
    {
        const auto& track = project.chordTrack;
        if (track.events.empty())
            return 0;
        if (indices.empty())
            for (int i = 0; i < (int) track.events.size(); ++i)
                indices.push_back (i);
        std::sort (indices.begin(), indices.end());
        indices.erase (std::remove_if (indices.begin(), indices.end(), [&track] (int i) { return ! juce::isPositiveAndBelow (i, (int) track.events.size()); }),
                       indices.end());
        if (indices.empty())
            return 0;
        // Onto an instrument track (that one, or a new one).
        const auto* target = project.findTrack (trackId);
        if (target == nullptr || target->isAudio() || target->isChannel() || target->frozen || target->disabled)
            trackId = addTrack ({});
        if (project.findTrack (trackId) == nullptr)
            return 0;
        const auto bar = project.beatsPerBar();
        const auto first = track.events[(size_t) indices.front()].beat;
        const auto start = std::floor (first / bar + 1.0e-9) * bar;
        Part part;
        part.start = start;
        part.name = "Chords";
        std::vector<int> previous;
        auto end = start;
        for (const auto i : indices)
        {
            const auto& e = track.events[(size_t) i];
            const auto until = track.endOf (i, bar);
            // Voiced as the pads would: the close voicing that moves least from the chord before.
            std::vector<std::vector<int>> close;
            for (const auto& v : namedVoicings (e.chord, 48 + 12 * project.chordPads.octaveOffset))
                if (v.close)
                    close.push_back (v.notes);
            if (close.empty())
                continue;
            const auto notes = close[(size_t) closestVoicing (close, previous, {})];
            for (const auto n : notes)
                part.notes.push_back ({ e.beat - start, until - e.beat, n, 100.0f / 127.0f });
            previous = notes;
            end = juce::jmax (end, until);
        }
        part.length = juce::jmax (bar, std::ceil ((end - start) / bar - 1.0e-9) * bar);
        addPart (trackId, part);
        const auto* now = project.findTrack (trackId);
        return now != nullptr && ! now->parts.empty() ? now->parts.back().id : 0;
    }

    int Studio::chordsFromParts (const std::vector<int>& partIds, double step)
    {
        std::vector<HeardNote> notes;
        auto from = std::numeric_limits<double>::max(), to = 0.0;
        for (const auto id : partIds)
            if (const auto at = findPart (id))
            {
                const auto& part = project.findTrack (at->first)->parts[(size_t) at->second];
                if (part.isAudio())
                    continue;
                from = juce::jmin (from, part.start);
                to = juce::jmax (to, part.end());
                for (const auto& n : part.notes)
                    if (n.start < part.length)
                        notes.push_back ({ part.start + n.start, part.start + juce::jmin (n.start + n.length, part.length), n.pitch, n.velocity });
            }
        if (notes.empty())
            return 0;
        const auto found = chordsFromNotes (notes, from, to, step);
        // Those over the parts' span replaced; the rest kept.
        auto events = project.chordTrack.events;
        events.erase (std::remove_if (events.begin(), events.end(), [from, to] (const ChordEvent& e) { return e.beat >= from - 1.0e-9 && e.beat < to - 1.0e-9; }),
                      events.end());
        for (const auto& e : found)
            events.push_back (e);
        project.chordTrack.events = std::move (events);
        project.chordTrack.shown = true;
        chordTrackChanged();
        return (int) found.size();
    }

    void Studio::assignPadsFromChordTrack()
    {
        // In the chord track's order, each chord once; pads beyond them left without (locked ones kept).
        std::vector<Chord> chords;
        for (const auto& e : project.chordTrack.events)
            if (std::find (chords.begin(), chords.end(), e.chord) == chords.end())
                chords.push_back (e.chord);
        auto pads = project.chordPads;
        size_t next = 0;
        for (auto& pad : pads.pads)
        {
            if (pad.locked)
                continue;
            pad = {};
            if (next < chords.size())
                pad.chord = chords[next++];
        }
        if (! project.chordTrack.scales.empty())
            pads.setKey (project.chordTrack.scales.front().key);
        setChordPads (pads);
    }
}
