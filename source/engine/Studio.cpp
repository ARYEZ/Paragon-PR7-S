#include "Studio.h"
#include "Controllers.h"
#include "Stretch.h"
#include "Warp.h"

#include <map>
#include <set>
#include <atomic>
#include <thread>
#include <bit>
#include <string_view>

#include "AudioAdvice.h"
#include "DawProject.h"
#include "SessionLog.h"
#include "DrumIdeas.h"
#include "MidiImport.h"

namespace pr7s
{
    namespace
    {
        const juce::String audioSetupKey { "audioSetup" };

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
        // (0.44.0) A pad played: the modifiers' target; while recording chords, a chord event.
        padPlayer.onPadChanged = [this] (int pad, bool on)
        {
            if (! on)
                return;
            lastPlayedPad = pad;
            if (loggingChords.load() && engine.isRecording())
                if (const auto chord = padPlayer.chordOf (pad))
                {
                    const std::scoped_lock l (chordLogLock);
                    chordLog.push_back ({ engine.getPosition(), *chord });
                }
        };
        padPlayer.onLearn = [weak = std::weak_ptr<bool> (alive), this] (int note)
        {
            juce::MessageManager::callAsync ([weak, this, note] { if (weak.lock() != nullptr) learnedKey (note); });
        };
        padPlayer.onModifier = [weak = std::weak_ptr<bool> (alive), this] (ChordPads::Modifier m)
        {
            juce::MessageManager::callAsync ([weak, this, m] { if (weak.lock() != nullptr) applyPadModifier (m); });
        };
        // (0.39.0) A performance: a held pad plays its chord through a pattern, in time.
        performer = std::make_unique<PatternPerformer> ([this] (const juce::MidiMessage& m) { padPlayer.send (m); });
        performer->songBeat = [this] { return engine.isPlaying() ? engine.getPosition() : -1.0; };
        padPlayer.onPerform = [this] (const std::vector<int>& notes, juce::uint8 velocity, int channel)
        {
            if (performer->getPattern() < 0)
                return false;
            performer->play (notes, velocity, channel);
            return true;
        };
        padPlayer.onPerformStop = [this] { performer->stop(); };
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
            if (auto data = getHeardAudio (name))   // (0.34.0: VariAudio's edits heard)
                return { data, &data->sound };
            return nullptr;
        });
        engine.setWarpSource ([this] (const Part& part, const TempoMap& tempo) { return getWarpedSound (part, tempo); });
        if (withDevices)
        {
            settings = std::make_unique<juce::PropertiesFile> (settingsOptions());
            host = std::make_unique<PluginHost> (settings.get());
            // (0.44.0) The user's patterns, then the pads' pattern as last time (0.39.0).
            if (const auto xml = juce::parseXML (settings->getValue ("userPatterns")))
            {
                std::vector<Pattern> mine;
                for (auto* p : xml->getChildWithTagNameIterator ("Pattern"))
                    if (auto pattern = patternFromXml (*p))
                        mine.push_back (std::move (*pattern));
                setUserPatterns (std::move (mine));
            }
            performer->setPattern (patternNamed (settings->getValue ("padPattern")));
            tapMode = (TapMode) juce::jlimit (0, (int) TapMode::displayOnly, settings->getIntValue ("tapTempoMode", 0));

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
        multiCoreAudio = withDevices;
        applyMultiCore();

        newProject();
        startTimerHz (30);
    }

    bool Studio::getSoftwareDrawing() const
    {
        return settings != nullptr ? settings->getBoolValue ("softwareDrawing", false) : softwareDrawing;
    }

    void Studio::setSoftwareDrawing (bool on)
    {
        if (settings != nullptr)
        {
            settings->setValue ("softwareDrawing", on);
            settings->saveIfNeeded();
        }
        softwareDrawing = on;
        SessionLog::note (on ? "Drawing: with the processor (SETTINGS)" : "Drawing: with the graphics card");
    }

    bool Studio::getMultiCoreAudio() const
    {
        return settings != nullptr ? settings->getBoolValue ("multiCoreAudio", true) : multiCoreAudio;
    }

    void Studio::setMultiCoreAudio (bool on)
    {
        if (settings != nullptr)
        {
            settings->setValue ("multiCoreAudio", on);
            settings->saveIfNeeded();
        }
        multiCoreAudio = on;
        applyMultiCore();
        announce();
    }

    void Studio::applyMultiCore()
    {
        // A helper per core but the audio thread's. (PARAGON_HELPERS=<n>: that many, for running the
        // tests on several cores.)
        auto helpers = getMultiCoreAudio() ? AudioWorkers::helpersForThisComputer() : 0;
        if (const auto forced = juce::SystemStats::getEnvironmentVariable ("PARAGON_HELPERS", {}); forced.isNotEmpty())
            helpers = forced.getIntValue();
        engine.setHelperThreads (helpers);
        SessionLog::note ("Multi-core audio: " + (helpers > 0 ? juce::String (helpers) + " helper threads beside the audio thread ("
                                                                 + juce::String (juce::SystemStats::getNumPhysicalCpus()) + " cores)"
                                                              : juce::String ("off (everything on the audio thread)")));
    }

    Studio::~Studio()
    {
        padPlayer.onPerform = nullptr;
        padPlayer.onPerformStop = nullptr;
        performer.reset();   // (its timer sends through the pad player)
        mediaBay.reset();   // (its scan asks the engine whether it plays)
        engine.stopPreview();
        stopPreview();
        linePreview.reset();
        engine.setPadPlayer (nullptr);
        // A save still writing finishes first (its call back here is dropped).
        saveAlive->store (false);
        {
            const SessionLog::Step step ("Waiting for a save still writing");
            saver.removeAllJobs (false, 60000);
        }
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
            const SessionLog::Step step ("Closing the audio device and MIDI inputs");
            devices.removeChangeListener (this);
            devices.removeAudioCallback (&engine);
            devices.removeMidiInputDeviceCallback ({}, &engine.getLiveInput());
            devices.closeAudioDevice();
        }
        // (The plug-ins are unloaded with the engine, after this: each a step of its own.)
    }

    void Studio::reloadAudioDevice()
    {
        SessionLog::note ("RELOAD DRIVER");
        // Closed and opened again: the driver is asked afresh for its sizes
        // (a size set in its own panel replaces one it no longer offers).
        if (devices.getCurrentAudioDevice() == nullptr)
            return;
        const SessionLog::Step step ("Closing and opening the audio device again");
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
            forgetAllAudio();
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
        // (0.76.0, the owner: the composer's instruments too loud, the mixer couldn't be used while hearing them)
        // While the composer is heard, a change (a fader, pan, an insert, a mute) is played in its copy and the
        // hearing goes on; it ends only when its tracks can no longer play it.
        std::optional<Project> heardCopy;
        // (0.85.2) A drag's mix-only step reads only levels, which the copy shares with the song: no copy then
        // (it copied the whole song and made the six layers' parts again on every step of a fader drag).
        const auto mixStepOnly = mixOnlyAsked && ! mixOnlyBroken && ! undoable;
        if (composerAudition.has_value() && ! mixStepOnly)
        {
            heardCopy = composerAuditionCopy();
            if (! heardCopy.has_value())
                endComposerAudition (false);
        }
        // A frozen track's parts are locked: whatever changed them is put back.
        for (auto& track : project.tracks)
            if (track.frozen)
                if (const auto kept = frozenParts.find (track.id); kept != frozenParts.end() && track.parts != kept->second)
                    track.parts = kept->second;
        applyAutoColour();   // (0.79.0: the rule wins over whatever moved or recoloured a track)
        numberParts();
        watchPlugins();
        if (exportJob == nullptr)   // (mid-render the engine plays the render's copy: endExport brings the song back)
        {
            // (0.83.1) A drag's step that moved only the mix (a fader, pan, send level, EQ or BOOSTER, nothing
            // written): just the levels, not every track's notes and clips again. The release (an undo step)
            // does the full update, so nothing else can be left behind.
            const auto& playing = heardCopy.has_value() ? *heardCopy : project;
            if (mixStepOnly)
                engine.updateMix (project);
            else
                engine.update (playing);
        }
        modified = true;
        if (undoable)
            recordUndoState();
        updateLiveTrack();
        announce();
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

    // (0.83.1) A copy of the song without the plug-ins' saved settings (several MB each: nothing
    // that plays or undoes a copy reads them), made by moving them out of the song for the copy and
    // back: the song is the same after. With `forUndo`, also without what undo never puts back.
    Project Studio::copyWithoutPluginStates (bool forUndo) const
    {
        auto& song = const_cast<Project&> (project);   // (message thread; left exactly as it was)
        std::vector<juce::MemoryBlock> keptStates;
        const auto takeOut = [&keptStates] (juce::MemoryBlock& state) { keptStates.push_back (std::move (state)); state.reset(); };
        for (auto& track : song.tracks)
        {
            takeOut (track.instrument.state);
            for (auto& insert : track.inserts)
                takeOut (insert.state);
        }
        for (auto& insert : song.masterInserts)
            takeOut (insert.state);
        std::vector<DrumMap> drumMaps;
        decltype (song.hitpointEdits) hitpointEdits;
        if (forUndo)
        {
            drumMaps = std::move (song.drumMaps);
            hitpointEdits = std::move (song.hitpointEdits);
            song.drumMaps.clear();
            song.hitpointEdits.clear();
        }
        auto copy = song;
        if (forUndo)
        {
            song.drumMaps = std::move (drumMaps);
            song.hitpointEdits = std::move (hitpointEdits);
        }
        auto next = keptStates.begin();
        for (auto& track : song.tracks)
        {
            track.instrument.state = std::move (*next++);
            for (auto& insert : track.inserts)
                insert.state = std::move (*next++);
        }
        for (auto& insert : song.masterInserts)
            insert.state = std::move (*next++);
        return copy;
    }

    void Studio::recordUndoState()
    {
        // A copy without the plug-ins' settings (they aren't undone, and can be several MB each:
        // since 0.34.1 they're moved out of the song while it's copied, not copied and thrown away),
        // nor what undo never puts back (the drum maps, the hand-edited hitpoints).
        auto state = copyWithoutPluginStates (true);
        // (0.83.0) Its place in the Edit History: the time now; its name is worked out when the history is
        // looked at (0.83.2, getHistory), from what differs from the step it follows.
        HistoryStep step;
        step.time = juce::Time::getCurrentTime();
        if (undoIndex < 0 || undoIndex >= (int) undoStates.size())
        {
            step.action = "Start";
            step.details = file.existsAsFile() ? file.getFileNameWithoutExtension() : juce::String ("New song");
            step.named = true;
        }
        undoStates.resize ((size_t) (undoIndex + 1));
        undoInfo.resize ((size_t) (undoIndex + 1));
        undoStates.push_back (std::move (state));
        undoInfo.push_back (std::move (step));
        const auto most = getMaxUndoSteps() + 1;   // (the start, and that many steps after it)
        while ((int) undoStates.size() > most)
            dropOldestStep();
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
        project.variAudio = state.variAudio;     // (0.34.0)
        project.linkGroups = state.linkGroups;   // (0.17.0)
        project.numerator = state.numerator;
        project.denominator = state.denominator;
        project.signatureChanges = state.signatureChanges;   // (0.45.0)
        project.signatureTrackShown = state.signatureTrackShown;
        project.markerTrack.markers = state.markerTrack.markers;   // (0.49.0: marker edits are undo steps;
        project.markerTrack.shown = state.markerTrack.shown;       //  the lock and the window's settings aren't)
        project.songSections = state.songSections;                 // (0.90.0: the blocks and their order are undo steps)
        project.loopOn = state.loopOn;
        project.loopStart = state.loopStart;
        project.loopEnd = state.loopEnd;
        project.click = state.click;
        project.masterVolumeDb = state.masterVolumeDb;
        project.masterEq = state.masterEq;
        project.masterBoost = state.masterBoost;
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
                track.follow = then->follow;                     // (0.36.0)
                track.followReference = then->followReference;
                track.liveFollow = then->liveFollow;
                track.followVoice = then->followVoice;           // (0.38.0)
                track.followVoicing = then->followVoicing;
                track.eq = then->eq;                             // (0.43.0)
                track.boost = then->boost;                       // (0.78.0)
                track.performPattern = then->performPattern;     // (0.44.0)
                track.folder = then->folder;                     // (0.50.0)
                track.folderGroup = then->folderGroup;
                track.folderData = then->folderData;
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
        project.tidyFolders();
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

    void Studio::goToHistory (int index)
    {
        index = juce::jlimit (0, (int) undoStates.size() - 1, index);
        if (index < 0 || index == undoIndex)
            return;
        undoIndex = index;
        restore (undoStates[(size_t) undoIndex]);
    }

    const std::vector<HistoryStep>& Studio::getHistory() const
    {
        for (size_t i = 0; i < undoInfo.size(); ++i)
            if (! undoInfo[i].named)
            {
                auto& step = undoInfo[i];
                if (i > 0 && i < undoStates.size())
                {
                    const auto described = describeEdit (undoStates[i - 1], undoStates[i]);
                    step.action = described.action;
                    if (step.details.isEmpty())
                        step.details = described.details;
                }
                else
                    step.action = "Edit";
                step.named = true;
            }
        return undoInfo;
    }

    void Studio::dropOldestStep()
    {
        // The oldest copy goes. The one after it becomes the first: if its name wasn't worked out, the copy
        // it would be compared with has just gone, so it says so (honest, and nothing compared on every edit).
        undoStates.erase (undoStates.begin());
        undoInfo.erase (undoInfo.begin());
        if (! undoInfo.empty() && ! undoInfo.front().named)
        {
            undoInfo.front().action = "Earliest Kept";
            undoInfo.front().details = "older steps dropped (MAX STEPS)";
            undoInfo.front().named = true;
        }
    }

    void Studio::setHistoryDetails (int index, const juce::String& text)
    {
        if (index >= 0 && index < (int) undoInfo.size())
        {
            getHistory();   // (named first: a typed note isn't then replaced by the name's details)
            undoInfo[(size_t) index].details = text;
            announce();
        }
    }

    int Studio::getMaxUndoSteps() const
    {
        if (settings != nullptr)
            return juce::jlimit (1, 1000, settings->getIntValue ("maxUndoSteps", 100));
        return maxUndoHere;
    }

    void Studio::setMaxUndoSteps (int steps)
    {
        maxUndoHere = juce::jlimit (1, 1000, steps);
        if (settings != nullptr)
            settings->setValue ("maxUndoSteps", maxUndoHere);
        // Fewer kept: the oldest go (never the one the song is at).
        while ((int) undoStates.size() > maxUndoHere + 1 && undoIndex > 0)
        {
            dropOldestStep();
            --undoIndex;
        }
        announce();
    }

    void Studio::clearHistory()
    {
        undoStates.clear();
        undoInfo.clear();
        undoIndex = -1;
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
        giveTypeLook (track, (int) project.tracks.size());
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
        giveTypeLook (track, (int) project.tracks.size());
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
        giveTypeLook (track, (int) project.tracks.size());
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
        giveTypeLook (track, (int) project.tracks.size());
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
        if (track == nullptr || ! project.canRouteTo (trackId, groupId))
            return;   // (0.60.0: any channel to any group or FX channel, never itself, never round in a loop)
        track->output = groupId;
        passOn (trackId, LinkGroup::What::routing, [&] (int other) { if (other != groupId) setOutput (other, groupId); });
        changed();
    }

    void Studio::setSend (int trackId, int slot, const Send& send)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || ! juce::isPositiveAndBelow (slot, Project::maxSends))
            return;
        if (send.target != 0)
        {
            // (0.60.0) Any track that makes sound, FX channels too, to any FX channel that doesn't lead back to it
            // (the slot's own send left out of the check: it's being replaced).
            const auto hasSlot = juce::isPositiveAndBelow (slot, (int) track->sends.size());
            const auto was = hasSlot ? std::exchange (track->sends[(size_t) slot].target, 0) : 0;
            const auto allowed = project.canSendTo (trackId, send.target);
            if (hasSlot)
                track->sends[(size_t) slot].target = was;
            if (! allowed)
                return;
        }
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
        mixOnly (! written);
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
        const auto parentFolder = it->folder;
        project.tracks.erase (it);
        for (auto& track : project.tracks)
            if (track.folder == trackId)
                track.folder = parentFolder;   // (0.50.0: a folder removed: its tracks stay, in the folder it was in)
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
        // (0.50.0) Where they land decides their folder: under an open folder's row, in it; else in the
        // folder of the track above (the top level at the top). A folder moves with its tracks.
        auto landed = 0;
        if (offset > 0)
        {
            const auto& above = staying[(size_t) offset - 1];
            landed = above.isFolder() && above.folderOpen ? above.id : above.folder;
        }
        std::vector<int> movedIds;
        for (const auto& t : moving)
            movedIds.push_back (t.id);
        staying.insert (staying.begin() + offset, std::make_move_iterator (moving.begin()), std::make_move_iterator (moving.end()));
        tracks = std::move (staying);
        for (const auto id : movedIds)
            if (auto* t = project.findTrack (id))
            {
                // (only those whose folder isn't among the moved: a folder's tracks keep their folder)
                if (std::find (movedIds.begin(), movedIds.end(), t->folder) == movedIds.end() && ! project.isInFolder (landed, t->id) && landed != t->id)
                {
                    leaveFolderGroup (*t);
                    t->folder = landed;
                    joinFolderGroup (*t);
                }
            }
        project.tidyFolders();
        changed();
    }

    //==========================================================================
    // Folder tracks (0.50.0).
    int Studio::folderGroupOf (int trackId) const
    {
        const auto* track = project.findTrack (trackId);
        for (const auto* up = track != nullptr ? project.findTrack (track->folder) : nullptr; up != nullptr; up = project.findTrack (up->folder))
            if (up->folderGroup)
                return up->id;
        return 0;
    }

    void Studio::joinFolderGroup (Track& track)
    {
        // Into a folder with its group channel: its output goes to that group (the manual's).
        if (track.isChannel() && ! track.isFolder())
            return;
        if (track.isFolder() && track.folderGroup)
            return;   // (a folder's own group goes to the master: groups can't feed groups here)
        if (const auto group = folderGroupOf (track.id); group != 0 && ! track.isFolder())
            track.output = group;
    }

    void Studio::leaveFolderGroup (Track& track)
    {
        // Out of a folder's group: back to the master if it went to that group.
        if (const auto group = folderGroupOf (track.id); group != 0 && track.output == group)
            track.output = 0;
    }

    int Studio::addFolderTrack (const juce::String& name, bool withGroup)
    {
        int number = 1;
        for (const auto& other : project.tracks)
            if (other.isFolder())
                ++number;
        Track track;
        track.id = project.nextTrackId++;
        track.kind = Track::Kind::folder;
        giveTypeLook (track, (int) project.tracks.size());
        track.name = name.isNotEmpty() ? name : "Folder " + juce::String (number);
        track.folderGroup = withGroup;
        project.tracks.push_back (track);
        engine.addChannel (track.id, withGroup);
        selectTrack (track.id);
        changed();
        return track.id;
    }

    int Studio::moveTracksToNewFolder (std::vector<int> trackIds, bool withGroup)
    {
        // (The manual's Move Selected Tracks to New Folder: the folder where the first of them was.)
        trackIds.erase (std::remove_if (trackIds.begin(), trackIds.end(), [this] (int id) { return project.findTrack (id) == nullptr; }), trackIds.end());
        if (trackIds.empty())
            return 0;
        int firstIndex = (int) project.tracks.size(), parent = 0;
        for (int i = 0; i < (int) project.tracks.size(); ++i)
            if (std::find (trackIds.begin(), trackIds.end(), project.tracks[(size_t) i].id) != trackIds.end() && i < firstIndex)
                firstIndex = i, parent = project.tracks[(size_t) i].folder;
        int number = 1;
        for (const auto& other : project.tracks)
            if (other.isFolder())
                ++number;
        Track folder;
        folder.id = project.nextTrackId++;
        folder.kind = Track::Kind::folder;
        folder.colour = project.tracks[(size_t) firstIndex].colour;
        folder.name = "Folder " + juce::String (number);
        folder.folder = parent;
        folder.folderGroup = withGroup;
        project.tracks.insert (project.tracks.begin() + firstIndex, folder);
        engine.addChannel (folder.id, withGroup);
        for (const auto id : trackIds)
            if (auto* t = project.findTrack (id); t != nullptr && ! (t->isFolder() && project.isInFolder (parent, t->id)))
            {
                // (a folder chosen with one of its own tracks: the folder moves, its tracks with it)
                if (std::find (trackIds.begin(), trackIds.end(), t->folder) != trackIds.end())
                    continue;
                leaveFolderGroup (*t);
                t->folder = folder.id;
                joinFolderGroup (*t);
            }
        project.tidyFolders();
        selectTrack (folder.id);
        changed();
        return folder.id;
    }

    bool Studio::setTracksFolder (std::vector<int> trackIds, int folderId)
    {
        const auto* target = project.findTrack (folderId);
        if (folderId != 0 && (target == nullptr || ! target->isFolder()))
            return false;
        auto any = false;
        for (const auto id : trackIds)
            if (auto* t = project.findTrack (id); t != nullptr && t->folder != folderId && id != folderId && ! project.isInFolder (folderId, id))
            {
                leaveFolderGroup (*t);
                t->folder = folderId;
                joinFolderGroup (*t);
                any = true;
            }
        if (! any)
            return false;
        // At the end of the folder (after its last track), or, out of folders, kept where they are.
        if (folderId != 0)
        {
            std::vector<Track> moving, staying;
            for (auto& t : project.tracks)
                (std::find (trackIds.begin(), trackIds.end(), t.id) != trackIds.end() ? moving : staying).push_back (std::move (t));
            project.tracks = std::move (staying);
            int last = -1;
            for (int i = 0; i < (int) project.tracks.size(); ++i)
                if (project.tracks[(size_t) i].id == folderId || project.isInFolder (project.tracks[(size_t) i].id, folderId))
                    last = i;
            project.tracks.insert (project.tracks.begin() + (last + 1), std::make_move_iterator (moving.begin()), std::make_move_iterator (moving.end()));
        }
        project.tidyFolders();
        changed();
        return true;
    }

    void Studio::setFolderOpen (int folderId, bool open)
    {
        auto* folder = project.findTrack (folderId);
        if (folder == nullptr || ! folder->isFolder() || folder->folderOpen == open)
            return;
        folder->folderOpen = open;
        changed (false);
    }

    void Studio::setFolderGroup (int folderId, bool on)
    {
        auto* folder = project.findTrack (folderId);
        if (folder == nullptr || ! folder->isFolder() || folder->folderGroup == on)
            return;
        if (! on)
            for (auto& t : project.tracks)
                if (t.output == folderId)
                    t.output = 0;   // (what went to its group: back to the master)
        folder->folderGroup = on;
        if (on)
            for (const auto id : project.folderMembers (folderId, true))
                if (auto* t = project.findTrack (id); t != nullptr && t->output == 0 && folderGroupOf (id) == folderId)
                    joinFolderGroup (*t);
        changed();
    }

    void Studio::setFolderData (int folderId, Track::FolderData data)
    {
        auto* folder = project.findTrack (folderId);
        if (folder == nullptr || ! folder->isFolder() || folder->folderData == data)
            return;
        folder->folderData = data;
        changed (false);
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
            const SessionLog::Step step ((disabled ? "Disabling the track " : "Enabling the track ") + track->name);
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
            const SessionLog::Step step ("Asking " + instrument->getName() + " for its settings");
            juce::MemoryBlock state;
            instrument->getStateInformation (state);
            track.instrument.state = std::move (state);
        }
        for (int slot = 0; slot < (int) track.inserts.size(); ++slot)
            if (auto* effect = engine.getEffect (id, slot))
            {
                const SessionLog::Step step ("Asking " + effect->getName() + " for its settings");
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
        forgetAudio (name);
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
            forgetAudio (track->freezeFile);
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
        return track != nullptr && ! track->hidden && ! project.isFoldedAway (*track);   // (0.50.0: in a closed folder: not shown)
    }

    std::vector<int> Studio::getShownTracks() const
    {
        std::vector<int> ids;
        for (const auto& track : project.tracks)
            if (! track.hidden && ! project.isFoldedAway (track))
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
        followDefaultDrumMap (trackId);
        changed (false);
        return true;
    }

    void Studio::followDefaultDrumMap (int trackId)
    {
        // (0.77.2) A track on a built-in map (GM or PR7-D) takes the one it should have: the Hub's
        // DEFAULT DRUM MAP (0.77.3), else PR7-D's names on a PR7-D track and GM's on any other. A map of
        // the owner's own (not built in) is never touched, nor a track with none.
        auto* track = project.findTrack (trackId);
        if (track == nullptr || ! DrumMap::builtIn (track->drumMap))
            return;
        const auto wanted = defaultDrumMapFor (trackId);
        if (wanted == track->drumMap || ! addDrumMapToSong (wanted))
            return;
        if (auto* again = project.findTrack (trackId))
            again->drumMap = wanted;
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
            const auto now = colour ? *colour : getTrackTypeLook (track.kind).colour.value_or (trackColour (i));
            any = any || track.colour != now;
            track.colour = now;
        }
        if (any)
            changed();
    }

    void Studio::setTracksListColour (const std::vector<int>& trackIds, bool on, int amount)
    {
        amount = juce::jlimit (1, 100, amount);
        auto any = false;
        for (auto& track : project.tracks)
            if (std::find (trackIds.begin(), trackIds.end(), track.id) != trackIds.end()
                && (track.listColourOn != on || track.listColourAmount != amount))
            {
                track.listColourOn = on;
                track.listColourAmount = amount;
                any = true;
            }
        if (any)
            changed (false);
    }

    std::vector<std::pair<int, int>> Studio::autoColourNumbers() const
    {
        std::vector<std::pair<int, int>> numbers;
        const auto& rule = project.autoColour;
        if (! rule.on)
            return numbers;
        const auto shown = getShownTracks();   // (the track list's numbers: 1 at the top)
        auto next = rule.from;
        for (int slot = 1; slot <= (int) shown.size(); ++slot)
        {
            if (slot < rule.from || (rule.to > 0 && slot > rule.to))
                continue;
            if (const auto* t = project.findTrack (shown[(size_t) slot - 1]); t != nullptr && project.autoColourTakes (*t))
                numbers.push_back ({ t->id, ((next++ - 1) % FamilyColours::numberedCount) + 1 });
        }
        return numbers;
    }

    bool Studio::applyAutoColour()
    {
        auto any = false;
        for (const auto& [id, number] : autoColourNumbers())
            if (auto* t = project.findTrack (id))
                if (const auto colour = FamilyColours::numbered (number); t->colour != colour)
                {
                    t->colour = colour;
                    any = true;
                }
        return any;
    }

    void Studio::setAutoColour (AutoColour rule)
    {
        rule.from = juce::jlimit (1, 9999, rule.from);
        rule.to = rule.to <= 0 ? 0 : juce::jlimit (rule.from, 9999, rule.to);
        if (rule == project.autoColour)
            return;
        project.autoColour = rule;
        changed (false);
    }

    bool Studio::moveTrackToSlot (int trackId, int slot)
    {
        const auto shown = getShownTracks();
        const auto from = std::find (shown.begin(), shown.end(), trackId);
        if (from == shown.end() || shown.empty())
            return false;
        slot = juce::jlimit (1, (int) shown.size(), slot);
        const auto target = shown[(size_t) slot - 1];
        if (target == trackId)
            return false;
        std::vector<int> moving { trackId };
        if (const auto* t = project.findTrack (trackId); t != nullptr && t->isFolder())
            for (const auto member : project.folderMembers (trackId, true))
                moving.push_back (member);
        if (std::find (moving.begin(), moving.end(), target) != moving.end())
            return false;   // (into its own folder)
        const auto indexOf = [this] (int id)
        {
            for (int i = 0; i < (int) project.tracks.size(); ++i)
                if (project.tracks[(size_t) i].id == id)
                    return i;
            return -1;
        };
        const auto down = std::distance (shown.begin(), from) < slot - 1;
        auto before = indexOf (target);
        if (down)
        {
            // After the track in that slot (and, a folder there, after its tracks), so it lands in the slot.
            auto last = before;
            if (const auto* t = project.findTrack (target); t != nullptr && t->isFolder())
                for (const auto member : project.folderMembers (target, true))
                    last = juce::jmax (last, indexOf (member));
            before = last + 1;
        }
        moveTracks (moving, before);
        return true;
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
            // (0.50.0) A folder's M: every track in it (the manual: Mute mutes all tracks in the folder).
            if (track->isFolder() && ! mirroring)
                for (const auto id : project.folderMembers (trackId, true))
                    if (auto* member = project.findTrack (id); member != nullptr && ! member->isFolder())
                        member->mute = on;
            changed (! written);
        }
    }

    bool Studio::isFolderSoloed (int folderId) const
    {
        auto any = false;
        for (const auto id : project.folderMembers (folderId, true))
            if (const auto* member = project.findTrack (id); member != nullptr && ! member->isFolder())
            {
                if (! member->solo)
                    return false;
                any = true;
            }
        return any;
    }

    void Studio::setSolo (int trackId, bool on, bool exclusive)
    {
        if (auto* track = project.findTrack (trackId))
        {
            if (exclusive)   // Ctrl-click (the manual's exclusive solo): the others' solos off
                for (auto& other : project.tracks)
                    other.solo = false;
            if (track->isFolder() && ! track->folderGroup)
            {
                // (0.50.0) A folder's S: every track in it (its own solo isn't kept: it would silence the FX channels).
                for (const auto id : project.folderMembers (trackId, true))
                    if (auto* member = project.findTrack (id); member != nullptr && ! member->isFolder())
                        member->solo = on && ! member->hidden && ! member->disabled;
            }
            else
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
            mixOnly (! written);   // (0.34.3: nothing but the mix moved, unless it wrote automation)
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
            mixOnly (! written);   // (0.34.3: nothing but the mix moved, unless it wrote automation)
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
        announce();
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

    namespace
    {
        // (0.29.0) A part moved or copied onto a track: on the lane asked for (else its own on
        // the same track, the first on another), and in front of what it lands on.
        void landOn (Part& part, const Track& to, bool sameTrack, int toLane)
        {
            part.lane = toLane >= 0 ? juce::jmin (255, toLane) : sameTrack ? part.lane : 0;
            auto order = 0;
            for (const auto& p : to.parts)
                order = juce::jmax (order, p.order);
            part.order = order + 1;
        }
    }

    void Studio::movePart (int trackId, int index, int toTrackId, double newStart, int toLane)
    {
        auto* from = project.findTrack (trackId);
        auto* to = project.findTrack (toTrackId);
        if (from == nullptr || to == nullptr || ! juce::isPositiveAndBelow (index, (int) from->parts.size()))
            return;
        if (from->frozen || to->frozen)
            return;   // (a frozen track's parts are locked: nothing leaves or joins them)
        auto part = from->parts[(size_t) index];
        part.start = juce::jmax (0.0, newStart);
        landOn (part, *to, from == to, toLane);
        from->parts.erase (from->parts.begin() + index);
        to->parts.push_back (std::move (part));
        changed();
    }

    void Studio::copyPart (int trackId, int index, int toTrackId, double newStart, int toLane)
    {
        const auto* from = project.findTrack (trackId);
        auto* to = project.findTrack (toTrackId);
        if (from == nullptr || to == nullptr || ! juce::isPositiveAndBelow (index, (int) from->parts.size()) || to->frozen)
            return;
        auto part = from->parts[(size_t) index];
        part.start = juce::jmax (0.0, newStart);
        landOn (part, *to, from == to, toLane);
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
            auto& part = track->parts[(size_t) index];
            const auto old = part;
            part.length = juce::jmax (0.25, newLength);
            rebaseWarp (old, part, project.tempoMap());   // (0.32.0: a warped clip's end pinned where it now is)
            changed();
        }
    }

    namespace
    {
        // A part cut at `rel` beats from its start: it keeps what's before, and the rest is returned.
        Part cutPart (Part& part, double rel, const TempoMap& tempo)
        {
            const auto original = part;   // (0.32.0: a warped clip's halves keep their audio where it was)
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
            // Controllers (0.25.0): each half its own; where a ramp crosses the cut, it ends there
            // in the first half and the second starts from the value it had reached.
            second.controllers.clear();
            std::vector<Controller> firstControllers;
            for (const auto code : ControllerKinds::usedCodes (part))
            {
                const auto at = ControllerKinds::valueAt (part.controllers, code, rel);
                auto hasAtCut = false, crossed = false;
                for (const auto& c : part.controllers)
                    if (ControllerKinds::codeOf (c) == code && c.start >= rel)
                    {
                        hasAtCut = c.start <= rel + 1.0e-9;
                        crossed = c.ramp && ! hasAtCut;
                        break;
                    }
                if (at && ! hasAtCut)
                {
                    const auto type = ControllerKinds::typeOf (code);
                    const auto number = ControllerKinds::numberOf (code);
                    if (crossed && rel > 1.0 / 960.0)
                        firstControllers.push_back ({ type, number, rel - 1.0 / 960.0,
                                                      *ControllerKinds::valueAt (part.controllers, code, rel - 1.0 / 960.0), true });
                    second.controllers.push_back ({ type, number, 0.0, *at, false });
                }
            }
            for (const auto& c : part.controllers)
            {
                if (c.start < rel)
                    firstControllers.push_back (c);
                else
                {
                    auto moved = c;
                    moved.start -= rel;
                    second.controllers.push_back (moved);
                }
            }
            ControllerKinds::sort (firstControllers);
            ControllerKinds::sort (second.controllers);
            part.controllers = std::move (firstControllers);
            part.length = rel;
            if (part.isAudio())
            {
                const auto seconds = tempo.secondsBetween (part.start, part.start + rel);
                second.audioOffset = part.audioOffset + seconds;
                second.fadeIn = 0.0;
                part.fadeOut = 0.0;
                part.fadeIn = juce::jmin (part.fadeIn, seconds);
                second.fadeOut = juce::jmin (second.fadeOut, tempo.secondsBetween (second.start, second.end()));
                if (original.isWarped())
                {
                    rebaseWarp (original, part, tempo);
                    rebaseWarp (original, second, tempo);
                }
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
            // Clips cut from one recording, end to end (a split glued back): one clip again, no new file.
            auto oneRecording = true;
            for (const auto i : after)
            {
                const auto& next = track->parts[(size_t) i];
                const auto secondsIn = glued.audioOffset + project.tempoMap().secondsBetween (glued.start, glued.end());
                if (next.audioFile != glued.audioFile || std::abs (next.start - glued.end()) > 1.0e-6 || next.volumeDb != glued.volumeDb
                    || std::abs (next.audioOffset - secondsIn) > 1.0e-4 || glued.isWarped() || next.isWarped())
                {
                    oneRecording = false;
                    break;
                }
                glued.length = next.end() - glued.start;
                glued.fadeOut = next.fadeOut;
            }
            // (0.82.6, the owner asked: glue any clips) Else the clips as they're heard (their volumes, fades, warps,
            // the crossfades between them; silence in the gaps) written into one new file, "<track> glued NN.wav",
            // and one clip of it in their place. The recordings themselves stay as they were on disk.
            if (! oneRecording)
            {
                std::vector<int> chosen { index };
                chosen.insert (chosen.end(), after.begin(), after.end());
                juce::String error;
                if (! glueIntoFile (*track, chosen, glued, error))
                {
                    SessionLog::note ("Glue: " + error);
                    return false;
                }
            }
        }
        else
        {
            // What was heard of each, in its place.
            std::vector<Note> notes;
            // ((0.47.0) each part's info-line Transpose and Velocity written into its notes: the glued part has none.)
            for (const auto& note : glued.notes)
                if (note.start >= 0.0 && note.start < glued.length)
                    notes.push_back ({ note.start, juce::jmin (note.length, glued.length - note.start), glued.heardPitch (note.pitch), glued.heardVelocity (note.velocity), note.muted });
            auto end = glued.end();
            for (const auto i : after)
            {
                const auto& next = track->parts[(size_t) i];
                for (const auto& note : next.notes)
                    if (note.start >= 0.0 && note.start < next.length)
                        notes.push_back ({ next.start - glued.start + note.start, juce::jmin (note.length, next.length - note.start),
                                           next.heardPitch (note.pitch), next.heardVelocity (note.velocity), note.muted });
                end = juce::jmax (end, next.end());
            }
            std::stable_sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
            glued.notes = std::move (notes);
            // Their controllers too (0.25.0), each in its place.
            std::vector<Controller> controllers;
            for (const auto& c : glued.controllers)
                if (c.start < glued.length)
                    controllers.push_back (c);
            for (const auto i : after)
            {
                const auto& next = track->parts[(size_t) i];
                for (auto c : next.controllers)
                    if (c.start < next.length)
                        c.start += next.start - glued.start, controllers.push_back (c);
            }
            ControllerKinds::sort (controllers);
            glued.controllers = std::move (controllers);
            glued.length = end - glued.start;
            glued.transpose = 0;
            glued.velocityOffset = 0;
        }
        track->parts[(size_t) index] = glued;
        std::sort (after.begin(), after.end(), std::greater<>());
        for (const auto i : after)
            track->parts.erase (track->parts.begin() + i);
        changed();
        return true;
    }

    bool Studio::glueIntoFile (const Track& track, const std::vector<int>& indices, Part& glued, juce::String& error)
    {
        Track among = track;
        among.soloLane = -1;
        among.parts.clear();
        auto from = std::numeric_limits<double>::max(), to = 0.0;
        auto rate = 0.0;
        auto channels = 1;
        for (const auto i : indices)
        {
            auto p = track.parts[(size_t) i];
            if (! p.isAudio())
                continue;
            p.muted = false;
            from = juce::jmin (from, p.start);
            to = juce::jmax (to, p.end());
            const auto data = getHeardAudio (p.audioFile);
            if (data == nullptr)
            {
                error = p.audioFile + " couldn't be read.";
                return false;   // (a missing recording: nothing glued rather than a gap where it was)
            }
            if (rate <= 0.0)
                rate = data->sampleRate;
            channels = juce::jmax (channels, data->sound.getNumChannels());
            among.parts.push_back (p);
        }
        if (among.parts.empty() || rate <= 0.0 || to <= from)
        {
            error = "the clips' recordings couldn't be read.";
            return false;
        }
        const auto tempo = project.tempoMap();
        const auto startSeconds = tempo.secondsAt (from);
        const auto total = (int) std::ceil ((tempo.secondsAt (to) - startSeconds) * rate);
        juce::AudioBuffer<float> sound (juce::jmin (2, channels), juce::jmax (1, total));
        sound.clear();
        for (int i = 0; i < (int) among.parts.size(); ++i)
            mixHeardClip (sound, among, i, startSeconds, rate);
        auto folder = getAudioFolder();
        folder.createDirectory();
        juce::File file;
        int n = 1;
        do
            file = folder.getChildFile (juce::File::createLegalFileName (track.name) + " glued " + juce::String (n++).paddedLeft ('0', 2) + ".wav");
        while (file.exists());
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        std::unique_ptr<juce::AudioFormatWriter> writer;
        if (stream != nullptr)
            writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate)
                                                                                   .withNumChannels (sound.getNumChannels())
                                                                                   .withBitsPerSample (32)
                                                                                   .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (sound, 0, sound.getNumSamples()))
        {
            error = "the glued file couldn't be written to " + file.getFullPathName();
            return false;
        }
        writer.reset();
        forgetAudio (file.getFileName());
        const auto keepId = glued.id;
        const auto lane = glued.lane, order = glued.order;
        glued = Part();
        glued.id = keepId;
        glued.lane = lane;
        glued.order = order;
        glued.start = from;
        glued.length = to - from;
        glued.audioFile = file.getFileName();
        glued.name = track.name + " glued";
        SessionLog::note ("Glue: " + juce::String ((int) among.parts.size()) + " clips into " + file.getFileName());
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

    std::optional<double> Studio::getPartField (int partId, PartField field) const
    {
        const auto where = findPart (partId);
        if (! where)
            return std::nullopt;
        const auto* track = project.findTrack (where->first);
        const auto& part = track->parts[(size_t) where->second];
        const auto audio = part.isAudio();
        switch (field)
        {
            case PartField::start:     return part.start;
            case PartField::end:       return part.end();
            case PartField::length:    return part.length;
            case PartField::offset:    if (audio && ! part.isWarped()) return part.audioOffset; break;
            case PartField::fadeIn:    if (audio) return part.fadeIn; break;
            case PartField::fadeOut:   if (audio) return part.fadeOut; break;
            case PartField::volume:    if (audio) return part.volumeDb; break;
            case PartField::transpose: if (! audio) return (double) part.transpose; break;
            case PartField::velocity:  if (! audio) return (double) part.velocityOffset; break;
        }
        return std::nullopt;
    }

    bool Studio::setPartsField (const std::vector<int>& partIds, PartField field, double value, bool absolute)
    {
        // The change the first one makes (relative), worked out before anything moves.
        std::optional<double> firstNow;
        for (const auto id : partIds)
            if ((firstNow = getPartField (id, field)))
                break;
        if (! firstNow || ! std::isfinite (value))
            return false;
        const auto delta = value - *firstNow;
        const auto tempo = project.tempoMap();
        auto any = false;
        for (const auto id : partIds)
        {
            const auto now = getPartField (id, field);
            const auto where = findPart (id);
            if (! now || ! where)
                continue;
            auto* track = project.findTrack (where->first);
            if (track->frozen)
                continue;
            auto& part = track->parts[(size_t) where->second];
            const auto old = part;
            const auto wanted = absolute ? value : *now + delta;
            const auto data = part.isAudio() ? getAudio (part.audioFile) : nullptr;
            const auto clipSeconds = [&] { return tempo.secondsBetween (part.start, part.end()); };
            // How long it can be: an event not past its recording's end (a warped one stretches: free).
            const auto setLength = [&] (double length)
            {
                length = juce::jmax (1.0 / 64.0, length);
                if (data != nullptr && ! part.isWarped())
                    length = juce::jmin (length, tempo.beatAfter (part.start, juce::jmax (0.0, data->seconds() - part.audioOffset)) - part.start);
                if (length <= 0.0)
                    return;
                part.length = length;
                if (old.isWarped())
                    rebaseWarp (old, part, tempo);
                const auto seconds = clipSeconds();
                part.fadeIn = juce::jmin (part.fadeIn, seconds);
                part.fadeOut = juce::jmin (part.fadeOut, seconds - part.fadeIn);
            };
            switch (field)
            {
                case PartField::start:  part.start = juce::jmax (0.0, wanted); break;
                case PartField::end:    setLength (wanted - part.start); break;
                case PartField::length: setLength (wanted); break;
                case PartField::offset:
                {
                    const auto most = data != nullptr ? juce::jmax (0.0, data->seconds() - clipSeconds()) : wanted;
                    part.audioOffset = juce::jlimit (0.0, juce::jmax (0.0, most), wanted);
                    break;
                }
                case PartField::fadeIn:   part.fadeIn = juce::jlimit (0.0, juce::jmax (0.0, clipSeconds() - part.fadeOut), wanted); break;
                case PartField::fadeOut:  part.fadeOut = juce::jlimit (0.0, juce::jmax (0.0, clipSeconds() - part.fadeIn), wanted); break;
                case PartField::volume:   part.volumeDb = juce::jlimit (Part::leastVolumeDb, Part::mostVolumeDb, wanted); break;
                case PartField::transpose: part.transpose = juce::jlimit (-48, 48, juce::roundToInt (wanted)); break;
                case PartField::velocity: part.velocityOffset = juce::jlimit (-127, 127, juce::roundToInt (wanted)); break;
            }
            any = any || ! (part == old);
        }
        if (any)
            changed();
        return any;
    }

    bool Studio::renameParts (const std::vector<int>& partIds, const juce::String& name)
    {
        auto any = false;
        for (const auto id : partIds)
            if (const auto where = findPart (id))
            {
                auto* track = project.findTrack (where->first);
                auto& part = track->parts[(size_t) where->second];
                if (! track->frozen && part.name != name)
                {
                    part.name = name;
                    any = true;
                }
            }
        if (any)
            changed();
        return any;
    }

    bool Studio::quantizeParts (const std::vector<int>& partIds, double grid)
    {
        if (grid <= 0.0)
            return false;
        auto any = false;
        for (auto& track : project.tracks)
        {
            if (track.isAudio() || track.isChannel() || track.frozen)
                continue;
            for (auto& part : track.parts)
                if (! part.notes.empty() && std::find (partIds.begin(), partIds.end(), part.id) != partIds.end())
                {
                    const auto before = part.notes;
                    quantize (part, grid);
                    any = any || part.notes != before;
                }
        }
        if (any)
            changed();
        return any;
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
                // (0.77.1) Muted notes stay as they are (they aren't heard); the others are humanized.
                std::vector<LineNote> notes;
                std::vector<Note> mutedNotes;
                for (const auto& n : part.notes)
                    if (n.muted)
                        mutedNotes.push_back (n);
                    else
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
                part.notes = std::move (mutedNotes);
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

    void Studio::setControllers (int trackId, int index, std::vector<Controller> events)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || ! juce::isPositiveAndBelow (index, (int) track->parts.size()))
            return;
        for (auto& c : events)
        {
            const auto code = ControllerKinds::codeOf (c);
            c.start = juce::jmax (0.0, c.start);
            c.value = juce::jlimit (0, ControllerKinds::maxValue (code), c.value);
            c.number = ControllerKinds::numberOf (code);
        }
        ControllerKinds::sort (events);
        auto& part = track->parts[(size_t) index];
        if (part.controllers == events)
            return;   // nothing changed: no undo step
        part.controllers = std::move (events);
        changed();
    }

    void Studio::setControllerLanes (int trackId, std::vector<int> lanes)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr)
            return;
        lanes.erase (std::remove_if (lanes.begin(), lanes.end(), [] (int code) { return ! ControllerKinds::isValid (code); }), lanes.end());
        if (track->controllerLanes == lanes)
            return;
        track->controllerLanes = std::move (lanes);
        changed (false);
    }

    void Studio::setPitchbendGrid (int trackId, int up, int down, bool snapOn, bool shown)
    {
        if (auto* track = project.findTrack (trackId))
        {
            track->pitchbendUp = juce::jlimit (1, 96, up);
            track->pitchbendDown = juce::jlimit (1, 96, down);
            track->pitchbendSnap = snapOn;
            track->pitchbendGrid = shown || snapOn;   // (the manual: Snap shows the grid it snaps to)
            changed (false);
        }
    }

    const DrumMap* Studio::getDrumMapOf (int trackId) const
    {
        const auto* track = project.findTrack (trackId);
        return track != nullptr ? project.drumMapOf (*track) : nullptr;
    }

    void Studio::setTrackDrumMap (int trackId, const juce::String& mapName)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || track->isAudio() || track->isChannel())
            return;
        const auto name = mapName;
        if (name.isNotEmpty() && ! addDrumMapToSong (name))
            return;
        const auto hadNone = track->drumMap.isEmpty();
        track->drumMap = name;
        if (name.isEmpty())
            track->drumEditor = false;
        else if (hadNone)
            track->drumEditor = true;   // (the manual's "Use Drum Editor when Drum Map is assigned")
        SessionLog::note ("Drum map for track " + juce::String (trackId) + ": " + (name.isEmpty() ? juce::String ("none") : name));
        changed (false);
    }

    bool Studio::addDrumMapToSong (const juce::String& name)
    {
        // In the song already, or a built-in one (GM, PR7-D), or one from the drum map folder.
        if (project.findDrumMap (name) != nullptr)
            return true;
        if (auto builtIn = DrumMap::builtIn (name))
        {
            project.drumMaps.push_back (std::move (*builtIn));
            return true;
        }
        juce::String error;
        for (const auto& libraryFile : getLibraryDrumMaps())
            if (libraryFile.getFileNameWithoutExtension() == name)
                if (auto map = DrumMap::readDrm (libraryFile, error))
                {
                    map->name = name;
                    project.drumMaps.push_back (std::move (*map));
                    return true;
                }
        return false;
    }

    juce::String Studio::getDefaultDrumMap() const
    {
        return settings != nullptr ? settings->getValue ("defaultDrumMap") : defaultDrumMapHere;
    }

    void Studio::setDefaultDrumMap (const juce::String& name)
    {
        defaultDrumMapHere = name;
        if (settings != nullptr)
            settings->setValue ("defaultDrumMap", name);
    }

    bool Studio::isDrumMapAvailable (const juce::String& name) const
    {
        if (name.isEmpty())
            return false;
        if (project.findDrumMap (name) != nullptr || DrumMap::builtIn (name))
            return true;
        for (const auto& libraryFile : getLibraryDrumMaps())
            if (libraryFile.getFileNameWithoutExtension() == name)
                return true;
        return false;
    }

    void Studio::setEditorSolo (int trackId)
    {
        if (trackId != 0 && project.findTrack (trackId) == nullptr)
            trackId = 0;
        if (trackId == editorSoloTrack)
            return;
        editorSoloTrack = trackId;
        if (exportJob == nullptr)
            engine.setEditorSolo (trackId);
        const auto wasModified = modified;   // (hearing, not an edit: the song isn't marked changed)
        changed (false);
        modified = wasModified;
    }

    bool Studio::playsPr7d (int trackId) const
    {
        const auto* track = project.findTrack (trackId);
        return track != nullptr && track->instrument.kind == Instrument::Kind::plugin
               && track->instrument.plugin.name.containsIgnoreCase ("PR7-D");
    }

    juce::String Studio::defaultDrumMapFor (int trackId) const
    {
        // (0.77.3) The Hub's DEFAULT DRUM MAP wins when one is chosen (and can still be found).
        if (const auto chosen = getDefaultDrumMap(); isDrumMapAvailable (chosen))
            return chosen;
        // (0.77.2) A track playing PR7-D: the PR7-D Map (its GROOVE LIBRARY layout, its default),
        // unless PR7-D itself is set to GENERAL MIDI (its "Drums Note Map" parameter).
        if (! playsPr7d (trackId))
            return DrumMap::gm().name;
        if (auto* instance = engine.getInstrument (trackId))
            for (auto* parameter : instance->getParameters())
                if (parameter->getName (64) == "Drums Note Map")
                    return parameter->getCurrentValueAsText().containsIgnoreCase ("General MIDI") ? DrumMap::gm().name
                                                                                                    : DrumMap::pr7d().name;
        return DrumMap::pr7d().name;
    }

    void Studio::setTrackDrumEditor (int trackId, bool drums)
    {
        if (auto* track = project.findTrack (trackId); track != nullptr && track->drumEditor != drums)
        {
            if (drums && track->drumMap.isEmpty())
                setTrackDrumMap (trackId, defaultDrumMapFor (trackId));   // (drums need names: the GM Map, or PR7-D's)
            if (auto* again = project.findTrack (trackId))
                again->drumEditor = drums;
            changed (false);
        }
    }

    juce::String Studio::addDrumMap (DrumMap map)
    {
        auto base = map.name.trim().isNotEmpty() ? map.name.trim() : juce::String ("Drum Map");
        auto name = base;
        for (int n = 2; project.findDrumMap (name) != nullptr; ++n)
            name = base + " " + juce::String (n);
        map.name = name;
        project.drumMaps.push_back (std::move (map));
        changed (false);
        return name;
    }

    bool Studio::setDrumMap (const juce::String& name, DrumMap edited)
    {
        auto it = std::find_if (project.drumMaps.begin(), project.drumMaps.end(), [&] (const DrumMap& m) { return m.name == name; });
        if (it == project.drumMaps.end())
            return false;
        edited.name = edited.name.trim();
        if (edited.name.isEmpty() || (edited.name != name && project.findDrumMap (edited.name) != nullptr))
            return false;   // (a name another map has)
        for (auto& track : project.tracks)
            if (track.drumMap == name)
                track.drumMap = edited.name;
        *it = std::move (edited);
        changed (false);
        return true;
    }

    void Studio::removeDrumMap (const juce::String& name)
    {
        const auto before = project.drumMaps.size();
        project.drumMaps.erase (std::remove_if (project.drumMaps.begin(), project.drumMaps.end(), [&] (const DrumMap& m) { return m.name == name; }),
                                project.drumMaps.end());
        if (project.drumMaps.size() == before)
            return;
        for (auto& track : project.tracks)
            if (track.drumMap == name)
                track.drumMap = {}, track.drumEditor = false;
        changed (false);
    }

    juce::String Studio::loadDrumMap (const juce::File& mapFile, juce::String& error)
    {
        std::optional<DrumMap> map;
        if (const auto xml = juce::XmlDocument::parse (mapFile); xml != nullptr && xml->hasTagName ("DrumMap") && xml->hasAttribute ("name"))
            map = DrumMap::fromXml (*xml);   // (ours)
        if (! map)
            map = DrumMap::readDrm (mapFile, error);
        if (! map)
            return {};
        SessionLog::note ("Drum map loaded: " + mapFile.getFullPathName());
        return addDrumMap (std::move (*map));
    }

    juce::File Studio::drumMapFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Paragon PR7-S").getChildFile ("Drum Maps");
    }

    juce::Array<juce::File> Studio::getLibraryDrumMaps() const
    {
        const auto folder = drumMapFolderOverride != juce::File() ? drumMapFolderOverride : drumMapFolder();
        auto files = folder.findChildFiles (juce::File::findFiles, true, "*.drm");
        files.sort();
        return files;
    }

    bool Studio::getNewControllersRamp() const
    {
        return settings != nullptr ? settings->getBoolValue ("controllerRamp", true) : newControllersRamp;
    }

    void Studio::setNewControllersRamp (bool ramp)
    {
        newControllersRamp = ramp;
        if (settings != nullptr)
            settings->setValue ("controllerRamp", ramp);
    }

    //==========================================================================
    void Studio::updatePart (int trackId, int index, const Part& part)
    {
        if (auto* track = project.findTrack (trackId); track != nullptr && juce::isPositiveAndBelow (index, (int) track->parts.size()))
        {
            auto& existing = track->parts[(size_t) index];
            const auto id = existing.id;
            const auto old = existing;
            existing = part;
            // (0.32.0) A warped clip trimmed (its start or end moved, the audio staying put): markers follow.
            if (old.isWarped() && part.warp == old.warp && part.audioFile == old.audioFile
                && (std::abs (part.start - old.start) > 1.0e-9 || std::abs (part.length - old.length) > 1.0e-9))
                rebaseWarp (old, existing, project.tempoMap());
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
        if (audioFolderOverride != juce::File())
            return audioFolderOverride;   // (0.72.0: a backup's song's recordings)
        return file != juce::File() ? file.getSiblingFile (file.getFileNameWithoutExtension() + " Audio") : unsavedAudioFolder;
    }

    std::shared_ptr<const Studio::AudioData> Studio::getAudio (const juce::String& name)
    {
        const auto rate = engine.getSampleRate();
        if (! juce::approximatelyEqual (audioCacheRate, rate))
        {
            forgetAllAudio();
            audioCacheRate = rate;
        }
        if (auto found = audioCache.find (name); found != audioCache.end())
            return found->second;

        auto data = readAudio (getAudioFolder().getChildFile (name));
        if (data == nullptr)
            return nullptr;
        audioCache[name] = data;
        return data;
    }

    std::shared_ptr<const Studio::AudioData> Studio::readAudio (const juce::File& file)
    {
        const auto rate = engine.getSampleRate();
        // Read whole, both sides (a mono file on both), at the engine's sample rate.
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
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
        data->stereo = reader->numChannels >= 2;
        data->makePeaks();
        return data;
    }

    void Studio::AudioData::makePeaks()
    {
        peaks.clear();
        leftPeaks.clear();
        rightPeaks.clear();
        const auto samples = sound.getNumSamples(), channels = sound.getNumChannels();
        for (int from = 0; from < samples; from += peakStep)
        {
            const auto count = juce::jmin (peakStep, samples - from);
            const auto left = channels > 0 ? sound.getMagnitude (0, from, count) : 0.0f;
            const auto right = channels > 1 ? sound.getMagnitude (1, from, count) : left;
            leftPeaks.push_back (left);
            rightPeaks.push_back (right);
            peaks.push_back (juce::jmax (left, right));
        }
    }

    //==========================================================================
    // The MediaBay (0.21.0).
    MediaDatabase& Studio::getMediaBay()
    {
        if (mediaBay == nullptr)
        {
            const auto store = settings != nullptr ? settings->getFile().getSiblingFile ("MediaBay.xml") : juce::File();
            mediaBay = std::make_unique<MediaDatabase> (store);
            mediaBay->shouldPause = [this] { return engine.isPlaying() || engine.isRecording(); };
            if (settings != nullptr && ! mediaBay->hadStore())
            {
                const auto ours = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Paragon PR7-S");
                ours.createDirectory();
                mediaBay->addLocation (ours);
                mediaBay->rescan();
            }
            if (settings != nullptr)
                mediaPreviewLevel = (float) settings->getDoubleValue ("mediaBayLevel", mediaPreviewLevel);
            engine.setPreviewGain (mediaPreviewLevel);
        }
        return *mediaBay;
    }

    const Studio::MediaPreviewOptions& Studio::getMediaPreviewOptions()
    {
        if (! mediaPreviewOptionsLoaded && settings != nullptr)
        {
            mediaPreviewOptions.linkTempo = settings->getBoolValue ("mediaBayLinkTempo", false);
            mediaPreviewOptions.waitForPlay = settings->getBoolValue ("mediaBayWaitForPlay", false);
            mediaPreviewOptions.semitones = juce::jlimit (-12, 12, settings->getIntValue ("mediaBaySemitones", 0));
            mediaPreviewOptions.toKey = juce::jlimit (-1, 11, settings->getIntValue ("mediaBayToKey", -1));
            const auto speed = settings->getDoubleValue ("mediaBaySpeed", 1.0);
            mediaPreviewOptions.speed = speed < 0.75 ? 0.5 : speed > 1.5 ? 2.0 : 1.0;
        }
        mediaPreviewOptionsLoaded = true;
        return mediaPreviewOptions;
    }

    void Studio::setMediaPreviewOptions (const MediaPreviewOptions& options)
    {
        mediaPreviewOptions = options;
        mediaPreviewOptions.semitones = juce::jlimit (-12, 12, options.semitones);
        mediaPreviewOptions.toKey = juce::jlimit (-1, 11, options.toKey);
        mediaPreviewOptions.speed = options.speed < 0.75 ? 0.5 : options.speed > 1.5 ? 2.0 : 1.0;
        mediaPreviewOptionsLoaded = true;
        if (settings != nullptr)
        {
            settings->setValue ("mediaBayLinkTempo", mediaPreviewOptions.linkTempo);
            settings->setValue ("mediaBayWaitForPlay", mediaPreviewOptions.waitForPlay);
            settings->setValue ("mediaBaySemitones", mediaPreviewOptions.semitones);
            settings->setValue ("mediaBayToKey", mediaPreviewOptions.toKey);
            settings->setValue ("mediaBaySpeed", mediaPreviewOptions.speed);
        }
    }

    int Studio::keyPitchClass (const juce::String& key)
    {
        if (key.isEmpty())
            return -1;
        static const int letters[] { 9, 11, 0, 2, 4, 5, 7 };   // A to G
        const auto letter = juce::CharacterFunctions::toUpperCase (key[0]);
        if (letter < 'A' || letter > 'G')
            return -1;
        auto pc = letters[letter - 'A'];
        if (key.length() > 1 && key[1] == '#') ++pc;
        if (key.length() > 1 && key[1] == 'b') --pc;
        return (pc + 12) % 12;
    }

    Studio::MediaPreviewPlan Studio::planMediaPreview (const juce::File& file, double seconds)
    {
        const auto& options = getMediaPreviewOptions();
        MediaPreviewPlan plan;
        plan.songBpm = juce::jmax (20.0, project.bpmAt (engine.getPosition()));
        // What the MediaBay knows of the file (else its name).
        double fileBpm = 0.0, fileBeats = 0.0;
        juce::String fileKey;
        bool known = false;
        for (const auto& item : getMediaBay().getItems())
            if (item.file == file)
            {
                fileBpm = item.tempo();
                fileBeats = item.beats;
                fileKey = item.keyName();
                known = true;
                break;
            }
        if (! known)
        {
            fileBpm = MediaDatabase::bpmFromName (file.getFileNameWithoutExtension());
            fileKey = MediaDatabase::keyFromName (file.getFileNameWithoutExtension());
        }
        // The pitch: to a root key (the short way) when the file's key is known, else semitones.
        plan.semitones = options.semitones;
        if (const auto from = keyPitchClass (fileKey); options.toKey >= 0 && from >= 0)
        {
            auto move = (options.toKey - from + 12) % 12;
            plan.semitones = move > 6 ? move - 12 : move;
        }
        // The length: to the song's tempo (a loop's beats: as the file says, else its length at its
        // tempo, to the nearest whole beat when it's that close), then the speed.
        if (options.linkTempo && fileBpm > 0.0 && seconds > 0.0)
        {
            auto beats = fileBeats > 0.0 ? fileBeats : seconds * fileBpm / 60.0;
            if (fileBeats <= 0.0 && std::abs (beats - std::round (beats)) < 0.05 * beats)
                beats = juce::jmax (1.0, std::round (beats));
            plan.lockBeats = beats / options.speed;
            plan.lengthFactor = (plan.lockBeats * 60.0 / plan.songBpm) / seconds;
        }
        else
            plan.lengthFactor = 1.0 / options.speed;
        return plan;
    }

    bool Studio::previewMedia (const juce::File& file, bool loop)
    {
        stopMediaPreview();
        const auto type = mediaTypeOf (file);
        if (type == MediaType::audio)
        {
            auto sound = readAudio (file);
            if (sound == nullptr)
                return false;
            const auto& options = getMediaPreviewOptions();
            const auto plan = planMediaPreview (file, sound->seconds());
            const auto length = sound->sound.getNumSamples();
            // Stretched or moved: the whole sound, ahead (up to 90 seconds: longer files play as they are).
            const auto changes = std::abs (plan.lengthFactor - 1.0) > 0.0005 || plan.semitones != 0.0;
            if (changes && length <= (int) (90.0 * sound->sampleRate))
            {
                const auto outLength = plan.lockBeats > 0.0 ? (int) std::llround (plan.lockBeats * 60.0 / plan.songBpm * sound->sampleRate)
                                                            : (int) std::llround ((double) length * plan.lengthFactor);
                auto stretched = std::make_shared<AudioData>();
                stretched->sampleRate = sound->sampleRate;
                stretched->sound = stretchAudio (sound->sound, sound->sampleRate, outLength, plan.semitones);
                if (stretched->sound.getNumChannels() == 1)
                {
                    stretched->sound.setSize (2, outLength, true);
                    stretched->sound.copyFrom (1, 0, stretched->sound, 0, 0, outLength);
                }
                for (int from = 0; from < outLength; from += AudioData::peakStep)
                {
                    const auto count = juce::jmin (AudioData::peakStep, outLength - from);
                    stretched->peaks.push_back (juce::jmax (stretched->sound.getMagnitude (0, from, count), stretched->sound.getMagnitude (1, from, count)));
                }
                sound = stretched;
            }
            mediaPreviewSound = sound;
            engine.setPreviewGain (mediaPreviewLevel);
            const auto locked = options.waitForPlay && plan.lockBeats > 0.0;
            engine.setPreviewSync (options.waitForPlay, project.loopStart, locked ? plan.lockBeats : 0.0);
            engine.setPreview (std::shared_ptr<const juce::AudioBuffer<float>> (sound, &sound->sound), loop || locked);
            return true;
        }
        if (type == MediaType::midi)
        {
            juce::String error;
            const auto midi = readMidiFile (file, error);
            if (! midi)
                return false;
            LineIdea idea;
            idea.part = LineIdea::Part::melody;
            idea.style = file.getFileNameWithoutExtension();
            // (0.23.0: moved by the previewer's semitones or to its root key, at its speed.)
            const auto plan = planMediaPreview (file, 0.0);
            const auto speed = getMediaPreviewOptions().speed;
            for (const auto& track : midi->tracks)
                for (const auto& n : track.notes)
                {
                    const auto pitch = juce::jlimit (0, 127, n.pitch + (int) plan.semitones);
                    idea.notes.push_back ({ n.start / speed, n.length / speed, pitch, n.velocity * juce::jlimit (0.05f, 1.0f, mediaPreviewLevel / 0.7f) });
                    mediaPreviewBeats = juce::jmax (mediaPreviewBeats, (n.start + n.length) / speed);
                }
            if (idea.notes.empty())
                return false;
            std::stable_sort (idea.notes.begin(), idea.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
            ChordSpan span;
            span.start = 0.0;
            span.end = juce::jmax (0.25, mediaPreviewBeats);
            previewLine (&idea, { span }, false);
            mediaPreviewMidi = true;
            return true;
        }
        return false;
    }

    std::optional<Studio::PluginPreset> Studio::readPluginPreset (const juce::File& presetFile, juce::String& error)
    {
        juce::String classId;
        juce::StringPairArray info;
        if (! MediaDatabase::readVstPresetInfo (presetFile, classId, info))
        {
            error = presetFile.getFileName() + " isn't a VST3 preset.";
            return std::nullopt;
        }
        juce::MemoryBlock data;
        presetFile.loadFileAsData (data);
        PluginPreset preset;
        preset.state = DawProject::vstPresetToHostState (data);
        if (preset.state.isEmpty())
        {
            error = presetFile.getFileName() + " has no plug-in settings inside.";
            return std::nullopt;
        }
        preset.name = info.getValue ("Name", presetFile.getFileNameWithoutExtension());
        auto pluginName = info.getValue ("PlugInName", {});
        if (pluginName.isEmpty())
            pluginName = presetFile.getParentDirectory().getFileName();
        // The plug-in: by its class id (as JUCE hashes it), else by name; the studio's synth too.
        juce::Array<juce::PluginDescription> candidates = host->getInstruments();
        candidates.addArray (host->getEffects());
        if (const auto* synth = host->getStudioSynth())
            candidates.add (*synth);
        const auto uid = MediaDatabase::vst3UniqueIdFor (classId);
        const juce::PluginDescription* found = nullptr;
        for (const auto& d : candidates)
            if (uid != 0 && d.pluginFormatName == "VST3" && d.uniqueId == uid)
                found = &d;
        if (found == nullptr)
            for (const auto& d : candidates)
                if (found == nullptr && pluginName.isNotEmpty() && d.name.equalsIgnoreCase (pluginName))
                    found = &d;
        if (found == nullptr)
        {
            error = "This preset is for " + (pluginName.isNotEmpty() ? pluginName : juce::String ("a plug-in"))
                  + ", which isn't among your scanned plug-ins: PLUGINS > SCAN / OPTIONS, then try again.";
            return std::nullopt;
        }
        preset.plugin = *found;
        preset.instrument = found->isInstrument;
        return preset;
    }

    int Studio::addTrackFromPluginPreset (const juce::File& presetFile, juce::String& error)
    {
        const auto preset = readPluginPreset (presetFile, error);
        if (! preset)
            return 0;
        if (preset->instrument)
        {
            Instrument instrument;
            instrument.kind = Instrument::Kind::plugin;
            instrument.plugin = preset->plugin;
            instrument.state = preset->state;
            const auto id = addTrack (instrument, &error);
            if (id != 0)
                renameTrack (id, preset->name);
            return id;
        }
        const auto id = addFxChannel (&preset->plugin, &error);
        if (id != 0)
            if (auto* effect = engine.getEffect (id, 0))
                effect->setStateInformation (preset->state.getData(), (int) preset->state.getSize());
        return id;
    }

    bool Studio::applyPluginPreset (int trackId, const juce::File& presetFile, juce::String& error)
    {
        const auto preset = readPluginPreset (presetFile, error);
        if (! preset)
            return false;
        if (isFrozen (trackId))
        {
            error = "It's frozen: unfreeze it first.";
            return false;
        }
        if (preset->instrument)
        {
            const auto* track = project.findTrack (trackId);
            if (track == nullptr || track->kind != Track::Kind::instrument)
            {
                error = "An instrument's preset goes on an instrument track.";
                return false;
            }
            // The same plug-in: the preset loaded into it; another: the instrument changed, with the preset.
            if (auto* running = engine.getInstrument (trackId);
                running != nullptr && track->instrument.kind == Instrument::Kind::plugin && track->instrument.plugin.uniqueId == preset->plugin.uniqueId
                && track->instrument.plugin.name == preset->plugin.name)
            {
                running->setStateInformation (preset->state.getData(), (int) preset->state.getSize());
                changed();
                return true;
            }
            Instrument instrument;
            instrument.kind = Instrument::Kind::plugin;
            instrument.plugin = preset->plugin;
            instrument.state = preset->state;
            return changeInstrument (trackId, instrument, &error);
        }
        // An effect's: into that effect when the track has it, else a new insert with it.
        if (const auto* inserts = insertsOf (trackId))
            for (int slot = 0; slot < (int) inserts->size(); ++slot)
                if (const auto& insert = (*inserts)[(size_t) slot];
                    ! insert.isEmpty() && insert.plugin.uniqueId == preset->plugin.uniqueId && insert.plugin.name == preset->plugin.name)
                    if (auto* effect = engine.getEffect (trackId, slot))
                    {
                        effect->setStateInformation (preset->state.getData(), (int) preset->state.getSize());
                        changed();
                        return true;
                    }
        Insert insert;
        insert.plugin = preset->plugin;
        insert.state = preset->state;
        juce::String problem;
        auto instance = host->createEffect (insert, engine.getSampleRate(), engine.getBlockSize(), problem);
        if (instance == nullptr)
        {
            error = problem;
            return false;
        }
        return addInsert (trackId, insert, std::move (instance));
    }

    void Studio::stopMediaPreview()
    {
        engine.stopPreview();
        if (mediaPreviewMidi)
            stopPreview();
        mediaPreviewMidi = false;
        mediaPreviewBeats = 0.0;
    }

    bool Studio::isMediaPreviewing() const
    {
        return engine.isPreviewPlaying() || (mediaPreviewMidi && isPreviewing());
    }

    double Studio::getMediaPreviewProgress() const
    {
        if (mediaPreviewMidi)
            return mediaPreviewBeats > 0.0 ? juce::jlimit (0.0, 1.0, getPreviewBeat() / mediaPreviewBeats) : 0.0;
        return engine.getPreviewProgress();
    }

    void Studio::setMediaPreviewLoop (bool on)
    {
        engine.setPreviewLoop (on);
    }

    float Studio::getMediaPreviewLevel() const
    {
        return mediaPreviewLevel;
    }

    void Studio::setMediaPreviewLevel (float level)
    {
        mediaPreviewLevel = juce::jlimit (0.0f, 1.0f, level);
        engine.setPreviewGain (mediaPreviewLevel);
        if (settings != nullptr)
            settings->setValue ("mediaBayLevel", (double) mediaPreviewLevel);
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

    namespace
    {
        // A lane's points from `from` to `to` (beats), from 0, with its values at both ends.
        std::vector<AutomationPoint> cutOut (const AutomationLane& lane, double from, double to)
        {
            std::vector<AutomationPoint> out;
            if (lane.points.empty())
                return out;
            out.push_back ({ 0.0, *lane.valueAt (from), 0.0f });
            for (const auto& p : lane.points)
                if (p.beat > from + 1.0e-9 && p.beat < to - 1.0e-9)
                    out.push_back ({ p.beat - from, p.value, p.curve });
            out.push_back ({ to - from, *lane.valueAt (to), 0.0f });
            return out;
        }

        // `clip` (points from 0) laid over `from` to `to`: what was there goes; the value at
        // either side is kept a 960th of a beat outside; a longer range holds the last value.
        void pasteOver (AutomationLane& lane, const std::vector<AutomationPoint>& clip, double from, double to)
        {
            constexpr double just = 1.0 / 960.0;
            const auto hadPoints = ! lane.points.empty();
            const auto before = hadPoints ? lane.valueAt (from - just) : std::nullopt;
            const auto after = hadPoints ? lane.valueAt (to + just) : std::nullopt;
            std::vector<AutomationPoint> kept;
            for (const auto& p : lane.points)
                if (p.beat < from - just * 0.5 || p.beat > to + just * 0.5)
                    kept.push_back (p);
            if (before && from - just > 0.0)
                kept.push_back ({ from - just, *before, 0.0f });
            auto last = clip.empty() ? 0.0f : clip.front().value;
            for (const auto& p : clip)
                if (p.beat <= to - from + 1.0e-9)
                {
                    kept.push_back ({ from + p.beat, p.value, p.curve });
                    last = p.value;
                }
                else
                {
                    // Cut where the range ends: the value it had reached there.
                    AutomationLane l;
                    l.points = clip;
                    kept.push_back ({ to, *l.valueAt (to - from), p.curve });
                    last = kept.back().value;
                    break;
                }
            if (! clip.empty() && clip.back().beat < to - from - 1.0e-9)
                kept.push_back ({ to, last, 0.0f });   // (longer: the last value to the right locator)
            if (after)
                kept.push_back ({ to + just, *after, 0.0f });
            std::stable_sort (kept.begin(), kept.end(), [] (const auto& a, const auto& b) { return a.beat < b.beat; });
            lane.points = std::move (kept);
        }
    }

    bool Studio::copyAutomation (AutomationScope scope, int busId, int laneIndex)
    {
        const auto from = project.loopStart, to = project.loopEnd;
        if (to <= from)
            return false;
        std::vector<AutomationClipLane> clip;
        const auto take = [&] (int bus)
        {
            if (const auto* lanes = getAutomation (bus))
                for (const auto& lane : *lanes)
                    if (! lane.points.empty())
                    {
                        auto copy = lane;
                        copy.points = cutOut (lane, from, to);
                        clip.push_back ({ bus, copy });
                    }
        };
        if (scope == AutomationScope::parameter)
        {
            const auto* lanes = getAutomation (busId);
            if (lanes == nullptr || ! juce::isPositiveAndBelow (laneIndex, (int) lanes->size()) || (*lanes)[(size_t) laneIndex].points.empty())
                return false;
            auto copy = (*lanes)[(size_t) laneIndex];
            copy.points = cutOut (copy, from, to);
            clip.push_back ({ busId, copy });
        }
        else
        {
            for (const auto& track : project.tracks)
                if (scope == AutomationScope::allTracks || isTrackSelected (track.id))
                    take (track.id);
            if (scope == AutomationScope::allTracks || isMasterSelected())
                take (Engine::masterBus);
        }
        if (clip.empty())
            return false;
        automationClip = std::move (clip);
        automationClipLength = to - from;
        return true;
    }

    bool Studio::pasteAutomation (AutomationScope scope, int busId, int laneIndex)
    {
        const auto from = project.loopStart, to = project.loopEnd;
        if (automationClip.empty() || to <= from)
            return false;
        const auto laneFor = [this] (int bus, const AutomationLane& control) -> AutomationLane*
        {
            // The bus's lane for that control (a new one, hidden, if it has none).
            auto* lanes = lanesOf (bus);
            if (lanes == nullptr)
                return nullptr;
            for (auto& lane : *lanes)
                if (lane.sameControl (control))
                    return &lane;
            auto added = control;
            added.points.clear();
            added.muted = false;
            lanes->push_back (added);
            return &lanes->back();
        };
        auto pasted = false;
        if (scope == AutomationScope::parameter)
        {
            auto* lanes = lanesOf (busId);
            if (lanes == nullptr || ! juce::isPositiveAndBelow (laneIndex, (int) lanes->size()))
                return false;
            pasteOver ((*lanes)[(size_t) laneIndex], automationClip.front().lane.points, from, to);
            pasted = true;
        }
        else if (scope == AutomationScope::allTracks)
        {
            for (const auto& c : automationClip)
                if (auto* lane = laneFor (c.busId, c.lane))
                    pasteOver (*lane, c.lane.points, from, to), pasted = true;
        }
        else
        {
            // The copied tracks in order, onto the selected tracks in order.
            std::vector<int> sources;
            for (const auto& c : automationClip)
                if (std::find (sources.begin(), sources.end(), c.busId) == sources.end())
                    sources.push_back (c.busId);
            const auto targets = getSelectedTracks();
            for (size_t i = 0; i < targets.size() && ! sources.empty(); ++i)
            {
                const auto source = sources[i % sources.size()];
                for (const auto& c : automationClip)
                    if (c.busId == source)
                        if (auto* lane = laneFor (targets[i], c.lane))
                            pasteOver (*lane, c.lane.points, from, to), pasted = true;
            }
        }
        if (! pasted)
            return false;
        changed();
        return true;
    }

    //==========================================================================
    void Studio::setAutomationSettings (const AutomationSettings& newSettings)
    {
        const auto leavingTrim = project.automationSettings.trim && ! newSettings.trim;
        project.automationSettings = newSettings;
        project.automationSettings.reductionLevel = juce::jlimit (0, 100, newSettings.reductionLevel);
        project.automationSettings.returnSeconds = juce::jlimit (0.0, 5.0, newSettings.returnSeconds);
        if (leavingTrim && newSettings.freezeTrim == AutomationSettings::FreezeTrim::onLeavingTrim)
        {
            for (auto& lane : project.masterAutomation)
                freezeTrimOf (lane);
            for (auto& track : project.tracks)
                for (auto& lane : track.automation)
                    freezeTrimOf (lane);
            changed();
            return;
        }
        changed (false);
    }

    void Studio::setAutomationMode (AutomationSettings::Mode mode)
    {
        auto s = project.automationSettings;
        s.mode = mode;
        setAutomationSettings (s);
    }

    Studio::AutomationSettings::Fill Studio::getFill (FillOption option) const noexcept
    {
        const auto& s = project.automationSettings;
        switch (option)
        {
            case FillOption::toPunch: return s.toPunch;
            case FillOption::toStart: return s.toStart;
            case FillOption::toEnd:   return s.toEnd;
            case FillOption::loop:    return s.loop;
            case FillOption::gaps:    return s.gaps;
        }
        return AutomationSettings::Fill::off;
    }

    void Studio::stepFill (FillOption option)
    {
        using F = AutomationSettings::Fill;
        auto s = project.automationSettings;
        auto& f = option == FillOption::toPunch ? s.toPunch : option == FillOption::toStart ? s.toStart
                : option == FillOption::toEnd ? s.toEnd : option == FillOption::loop ? s.loop : s.gaps;
        f = f == F::off ? F::once : f == F::once ? F::locked : F::off;
        setAutomationSettings (s);
    }

    void Studio::setSuspendRead (AutomationSettings::Group group, bool on)
    {
        auto s = project.automationSettings;
        s.suspendRead[(size_t) group] = on;
        setAutomationSettings (s);
    }

    void Studio::setSuspendWrite (AutomationSettings::Group group, bool on)
    {
        auto s = project.automationSettings;
        s.suspendWrite[(size_t) group] = on;
        if (on)
            for (size_t i = passes.size(); i-- > 0;)
                if (AutomationSettings::groupOf (passes[i].control) == group)
                    punchOut (i, engine.getPosition());   // (the manual: it punches out of the pass)
        setAutomationSettings (s);
    }

    void Studio::setAllSuspended (bool read, bool on)
    {
        auto s = project.automationSettings;
        (read ? s.suspendRead : s.suspendWrite).fill (on);
        if (! read && on)
            punchOutAll();
        setAutomationSettings (s);
    }

    bool Studio::isAllSuspended() const noexcept
    {
        const auto& s = project.automationSettings;
        return std::all_of (s.suspendRead.begin(), s.suspendRead.end(), [] (bool b) { return b; })
            && std::all_of (s.suspendWrite.begin(), s.suspendWrite.end(), [] (bool b) { return b; });
    }

    void Studio::setAllSuspended (bool on)
    {
        auto s = project.automationSettings;
        s.suspendRead.fill (on);
        s.suspendWrite.fill (on);
        if (on)
            punchOutAll();
        setAutomationSettings (s);
    }

    void Studio::setTrim (bool on)
    {
        auto s = project.automationSettings;
        s.trim = on;
        setAutomationSettings (s);
    }

    void Studio::freezeTrimOf (AutomationLane& lane)
    {
        // The trim rendered into the points: a point wherever either curve has one.
        if (lane.trim.empty() || lane.points.empty() || ! lane.canTrim())
        {
            lane.trim.clear();
            return;
        }
        std::vector<double> beats;
        for (const auto& p : lane.points)
            beats.push_back (p.beat);
        for (const auto& p : lane.trim)
            beats.push_back (p.beat);
        std::sort (beats.begin(), beats.end());
        beats.erase (std::unique (beats.begin(), beats.end(), [] (double a, double b) { return std::abs (a - b) < 1.0e-9; }), beats.end());
        std::vector<AutomationPoint> frozen;
        for (const auto beat : beats)
            frozen.push_back ({ beat, *lane.playedAt (beat, false), 0.0f });
        lane.points = std::move (frozen);
        lane.trim.clear();
    }

    void Studio::freezeTrim (bool selectedTracksOnly)
    {
        if (! selectedTracksOnly || isMasterSelected())
            for (auto& lane : project.masterAutomation)
                freezeTrimOf (lane);
        for (auto& track : project.tracks)
            if (! selectedTracksOnly || isTrackSelected (track.id))
                for (auto& lane : track.automation)
                    freezeTrimOf (lane);
        changed();
    }

    void Studio::freezeTrim (int busId, int laneIndex)
    {
        auto* lanes = lanesOf (busId);
        if (lanes == nullptr || ! juce::isPositiveAndBelow (laneIndex, (int) lanes->size()))
            return;
        freezeTrimOf ((*lanes)[(size_t) laneIndex]);
        changed();
    }

    void Studio::setTrimPoints (int busId, int laneIndex, std::vector<AutomationPoint> points)
    {
        auto* lanes = lanesOf (busId);
        if (lanes == nullptr || ! juce::isPositiveAndBelow (laneIndex, (int) lanes->size()) || ! (*lanes)[(size_t) laneIndex].canTrim())
            return;
        for (auto& p : points)
            p.beat = juce::jmax (0.0, p.beat), p.value = juce::jlimit (0.0f, 1.0f, p.value), p.terminator = false;
        std::stable_sort (points.begin(), points.end(), [] (const auto& a, const auto& b) { return a.beat < b.beat; });
        (*lanes)[(size_t) laneIndex].trim = std::move (points);
        changed();
    }

    void Studio::deleteAllAutomation()
    {
        for (auto& lane : project.masterAutomation)
            lane.points.clear(), lane.trim.clear();
        for (auto& track : project.tracks)
            for (auto& lane : track.automation)
                lane.points.clear(), lane.trim.clear();
        changed();
    }

    void Studio::deleteAutomationOfSelectedTracks()
    {
        if (isMasterSelected())
            for (auto& lane : project.masterAutomation)
                lane.points.clear(), lane.trim.clear();
        for (auto& track : project.tracks)
            if (isTrackSelected (track.id))
                for (auto& lane : track.automation)
                    lane.points.clear(), lane.trim.clear();
        changed();
    }

    void Studio::deleteAutomationInRange()
    {
        // Between the locators: gone, the curve either side as it was (a 960th of a beat inside the range).
        const auto from = project.loopStart, to = project.loopEnd;
        if (to <= from)
            return;
        const auto cut = [&] (AutomationLane& lane)
        {
            if (lane.points.empty())
                return;
            constexpr double just = 1.0 / 960.0;
            const auto before = lane.valueAt (from), after = lane.valueAt (to);
            std::vector<AutomationPoint> kept;
            for (const auto& p : lane.points)
                if (p.beat < from || p.beat > to)
                    kept.push_back (p);
            if (std::any_of (lane.points.begin(), lane.points.end(), [&] (const AutomationPoint& p) { return p.beat < from; }))
                kept.push_back ({ from, *before, 0.0f, project.automationSettings.virginTerritory });
            if (std::any_of (lane.points.begin(), lane.points.end(), [&] (const AutomationPoint& p) { return p.beat > to; }))
                kept.push_back ({ to + just, *after });
            std::stable_sort (kept.begin(), kept.end(), [] (const auto& a, const auto& b) { return a.beat < b.beat; });
            lane.points = std::move (kept);
        };
        for (auto& lane : project.masterAutomation)
            cut (lane);
        for (auto& track : project.tracks)
            for (auto& lane : track.automation)
                cut (lane);
        changed();
    }

    void Studio::fillGaps (bool withCurrentValue)
    {
        // The manual's Fill Gaps on Selected Tracks: a gap filled with its section's last value
        // (or the control's setting now), up to a millisecond before the next section, then a ramp.
        const auto fill = [&] (int busId, AutomationLane& lane)
        {
            std::vector<AutomationPoint> filled;
            for (size_t i = 0; i < lane.points.size(); ++i)
            {
                auto p = lane.points[i];
                const auto gap = p.terminator && i + 1 < lane.points.size();
                p.terminator = false;
                filled.push_back (p);
                if (gap)
                {
                    const auto value = withCurrentValue ? getControlValue (busId, lane) : p.value;
                    const auto next = lane.points[i + 1].beat;
                    const auto ms = project.tempoMap().beatAfter (next, 0.0) - project.tempoMap().beatAt (juce::jmax (0.0, project.tempoMap().secondsAt (next) - 0.001));
                    if (withCurrentValue)
                        filled.push_back ({ p.beat + 1.0e-3, value });
                    filled.push_back ({ juce::jmax (p.beat + 2.0e-3, next - ms), value });
                }
            }
            lane.points = std::move (filled);
        };
        if (isMasterSelected())
            for (auto& lane : project.masterAutomation)
                fill (Engine::masterBus, lane);
        for (auto& track : project.tracks)
            if (isTrackSelected (track.id))
                for (auto& lane : track.automation)
                    fill (track.id, lane);
        changed();
    }

    void Studio::showAutomationGroup (AutomationSettings::Group group, bool usedOnly)
    {
        // Every track's lanes of that kind (volume, pan, mute; sends and plug-in knobs: those it has lanes for).
        const auto show = [&] (int busId, std::vector<AutomationLane>& lanes, bool& open)
        {
            auto any = false;
            for (auto& lane : lanes)
                if (AutomationSettings::groupOf (lane) == group && (! usedOnly || ! lane.points.empty()))
                    lane.shown = any = true;
            if (! any && ! usedOnly && group != AutomationSettings::Group::plugins)
                for (const auto& control : getAutomationControls (busId))
                {
                    const auto lane = control.asLane();
                    if (AutomationSettings::groupOf (lane) != group)
                        continue;
                    if (std::none_of (lanes.begin(), lanes.end(), [&] (const AutomationLane& l) { return l.sameControl (lane); }))
                        lanes.push_back (lane);
                    any = true;
                    if (group != AutomationSettings::Group::sends)
                        break;   // (one each: volume, pan, mute; every send)
                }
            if (any)
                open = true;
        };
        show (Engine::masterBus, project.masterAutomation, project.masterAutomationOpen);
        for (auto& track : project.tracks)
            show (track.id, track.automation, track.automationOpen);
        changed (false);
    }

    void Studio::hideAllAutomation()
    {
        project.masterAutomationOpen = false;
        for (auto& track : project.tracks)
            track.automationOpen = false;
        changed (false);
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
        mixOnly (! written);
        changed (undoable && ! written);
    }

    const ChannelEq* Studio::getChannelEq (int busId) const
    {
        if (busId == Engine::masterBus)
            return &project.masterEq;
        const auto* track = project.findTrack (busId);
        return track != nullptr ? &track->eq : nullptr;
    }

    void Studio::setChannelEq (int busId, const ChannelEq& eq, bool undoable)
    {
        const auto limited = eq.limited();
        auto* target = busId == Engine::masterBus ? &project.masterEq : nullptr;
        if (target == nullptr)
            if (auto* track = project.findTrack (busId))
                target = &track->eq;
        if (target == nullptr || (*target == limited && ! undoable))
            return;
        *target = limited;
        mixOnly (! undoable);   // (0.83.1: a drag's steps redraw no other view; the release does)
        changed (undoable);
    }

    const Booster* Studio::getBooster (int busId) const
    {
        if (busId == Engine::masterBus)
            return &project.masterBoost;
        const auto* track = project.findTrack (busId);
        return track != nullptr ? &track->boost : nullptr;
    }

    void Studio::setBooster (int busId, const Booster& boost, bool undoable)
    {
        const auto limited = boost.limited();
        auto* target = busId == Engine::masterBus ? &project.masterBoost : nullptr;
        if (target == nullptr)
            if (auto* track = project.findTrack (busId))
                target = &track->boost;
        if (target == nullptr || (*target == limited && ! undoable))
            return;
        *target = limited;
        mixOnly (! undoable);   // (0.83.1: a drag's steps redraw no other view; the release does)
        changed (undoable);
    }

    double Studio::getEqResponseDb (int busId, double frequency, bool withPre) const
    {
        const auto* eq = getChannelEq (busId);
        if (eq == nullptr)
            return 0.0;
        const auto rate = juce::jmax (44100.0, engine.getSampleRate());
        return eqResponseDb (designEq (*eq, rate), frequency, rate, withPre);
    }

    juce::File Studio::eqPresetFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Paragon PR7-S").getChildFile ("EQ Presets");
    }

    bool Studio::saveEqPreset (int busId, const juce::File& file) const
    {
        const auto* eq = getChannelEq (busId);
        if (eq == nullptr)
            return false;
        file.getParentDirectory().createDirectory();
        return eq->toXml ("EqPreset")->writeTo (file);
    }

    bool Studio::loadEqPreset (int busId, const juce::File& file)
    {
        const auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName ("EqPreset") || getChannelEq (busId) == nullptr)
            return false;
        setChannelEq (busId, ChannelEq::fromXml (*xml));
        return true;
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
        if (project.automationSettings.suspendWrite[(size_t) AutomationSettings::groupOf (which)])
            return;   // (0.28.0: Suspend Write: this kind of control isn't written)
        for (auto& pass : passes)
            if (pass.busId == busId && pass.control.sameControl (which))
            {
                pass.retouched = pass.released;   // touched again (latched: Cross-Over watches for the old curve)
                if (pass.released && ! pass.points.empty())
                    pass.points.push_back ({ engine.getPosition(), pass.points.back().value });   // (held flat while let go)
                pass.released = false;
                pass.aboveOld.reset();
                return;
            }
        startPass (busId, which);
    }

    void Studio::startPass (int busId, const AutomationLane& which)
    {
        WritePass pass;
        pass.busId = busId;
        pass.control = which;
        pass.start = pass.lastBeat = engine.getPosition();
        pass.cycleCount = engine.getCycleCount();
        pass.before = getControlValue (busId, which);
        passes.push_back (pass);
        engine.setHeld (busId, which, true);   // the hand is heard, not the lane
        engine.update (project);
        announce();                   // (W shows it's writing)
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
                if (project.automationSettings.continueOnJump && engine.isPlaying())
                {
                    // (0.28.0, Continue Writing on Transport Jump) A new pass from where it jumped to.
                    startPass (busId, which);
                    passes.back().points.push_back ({ beat, juce::jlimit (0.0f, 1.0f, value) });
                    return true;
                }
                return false;
            }
            if (pass.retouched && project.automationSettings.mode == AutomationSettings::Mode::crossOver)
            {
                // (0.28.0) Cross-Over: touched again, it punches out where the hand crosses the old curve.
                const AutomationLane* old = nullptr;
                if (const auto* lanes = getAutomation (busId))
                    for (const auto& l : *lanes)
                        if (l.sameControl (which))
                            old = &l;
                const auto oldValue = old != nullptr && ! old->points.empty() ? *old->valueAt (beat) : pass.before;
                const auto above = value >= oldValue;
                if (pass.aboveOld && *pass.aboveOld != above)
                {
                    pass.lastBeat = beat;
                    pass.points.push_back ({ beat, oldValue });
                    punchOut (i, beat, false);   // (met the old curve: no return)
                    return false;
                }
                pass.aboveOld = above;
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
                if (which.isStep() || project.automationSettings.mode != AutomationSettings::Mode::touch)
                    passes[i].released = true;   // Auto-Latch (and Cross-Over): its last value kept until it punches out
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
            t.boost = preset.boost;   // (0.78.0)
            t.eq = preset.eq;   // (0.43.0: the manual's "Loading Inserts and EQ from Track Presets")
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

    void Studio::punchOut (size_t index, double endBeat, bool returnToOld)
    {
        auto pass = passes[index];
        passes.erase (passes.begin() + (std::ptrdiff_t) index);
        engine.setHeld (pass.busId, pass.control, false);
        auto* lanes = lanesOf (pass.busId);
        if (lanes == nullptr)
            return;
        auto& as = project.automationSettings;
        endBeat = juce::jmax (endBeat, pass.start);

        // Its lane (a new one if the control had none: every pass is kept, to open later).
        auto lane = std::find_if (lanes->begin(), lanes->end(), [&] (const AutomationLane& l) { return l.sameControl (pass.control); });
        if (lane == lanes->end())
        {
            auto added = pass.control;
            added.shown = true;
            added.trim.clear();
            if (auto* plugin = pluginFor (pass.busId, pass.control); plugin != nullptr && pass.control.parameter < plugin->getParameters().size())
                added.parameterName = plugin->getParameters()[pass.control.parameter]->getName (64);
            lanes->push_back (added);
            lane = lanes->end() - 1;
        }
        // (0.28.0, Trim) A volume or send pass with Trim on writes the trim curve: the hand's
        // level less the curve's, around the middle.
        const auto trimming = as.trim && lane->canTrim() && ! lane->points.empty();
        const auto old = *lane;
        const auto& oldCurve = trimming ? old.trim : old.points;
        const auto oldAt = [&] (double beat) -> std::optional<float>
        {
            if (trimming)
            {
                if (old.trim.empty())
                    return 0.5f;
                AutomationLane t;
                t.points = old.trim;
                return *t.valueAt (beat);
            }
            if (old.points.empty())
                return as.virginTerritory ? std::optional<float>() : std::optional<float> (pass.before);
            return old.valueAt (beat, as.virginTerritory);
        };
        auto written = pass.points;
        if (trimming)
            for (auto& p : written)
            {
                const auto base = travelToVolume (*old.valueAt (p.beat));
                p.value = juce::jlimit (0.0f, 1.0f, 0.5f + (travelToVolume (p.value) - base) / (2.0f * AutomationLane::trimRange));
            }
        const auto startValue = written.empty() ? oldAt (pass.start).value_or (pass.before) : written.front().value;
        if (written.empty() || written.front().beat > pass.start)
            written.insert (written.begin(), { pass.start, startValue });
        if (pass.control.isStep() || written.back().beat < endBeat)
            written.push_back ({ endBeat, written.back().value });   // held to the end (a latched control, or a still hand)

        // Thinned (the Reduction Level: 0% only repeats; up to 100% smoother).
        const auto tolerance = 0.0002f + 0.02f * (float) as.reductionLevel / 100.0f;
        std::vector<AutomationPoint> thin;
        for (size_t i = 0; i < written.size(); ++i)
        {
            if (i > 0 && i + 1 < written.size())
            {
                const auto& a = thin.back();
                const auto& b = written[i + 1];
                const auto along = b.beat > a.beat ? (written[i].beat - a.beat) / (b.beat - a.beat) : 0.0;
                const auto line = a.value + (b.value - a.value) * (float) along;
                if (std::abs (written[i].value - line) < tolerance && ! pass.control.isStep())
                    continue;
                if (pass.control.isStep() && std::abs (written[i].value - a.value) < 1.0e-6f)
                    continue;
            }
            thin.push_back (written[i]);
        }

        // Fill (0.28.0, the manual's Fill Options): the value found, written across a stretch.
        using F = AutomationSettings::Fill;
        const auto fillValue = thin.back().value;
        auto from = pass.start, to = endBeat;
        auto doReturn = returnToOld;   // (an on/off switch jumps back: no ramp)
        auto filled = false, toTheEnd = false;
        if (! pass.control.isStep())
        {
            if (as.toPunch != F::off)
                thin = { { pass.start, fillValue }, { endBeat, fillValue } }, filled = true;
            if (as.toStart != F::off)
                from = 0.0, thin = { { 0.0, fillValue }, { endBeat, fillValue } }, filled = true;
            if (as.toEnd != F::off)
            {
                toTheEnd = true;   // (everything after goes: the value holds to the end)
                if (thin.empty() || thin.back().beat < endBeat)
                    thin.push_back ({ endBeat, fillValue });
                doReturn = false;
                filled = true;
            }
            if (as.loop != F::off && project.loopEnd > project.loopStart)
            {
                from = juce::jmin (from, project.loopStart);
                to = juce::jmax (to, project.loopEnd);
                std::vector<AutomationPoint> kept;
                for (const auto& p : thin)
                    if (p.beat < project.loopStart || p.beat > project.loopEnd)
                        kept.push_back (p);
                kept.push_back ({ project.loopStart, fillValue });
                kept.push_back ({ project.loopEnd, fillValue });
                std::stable_sort (kept.begin(), kept.end(), [] (const auto& a, const auto& b) { return a.beat < b.beat; });
                thin = std::move (kept);
                filled = true;
            }
        }
        if (filled)
            doReturn = doReturn && as.toEnd == F::off;

        // In place of what was there from the start to the end (and the return).
        const auto returnBeats = doReturn && ! pass.control.isStep() ? project.tempoMap().beatAfter (endBeat, as.returnSeconds) - endBeat : 0.0;
        const auto backAt = endBeat + returnBeats;
        const auto clearTo = juce::jmax (backAt, filled ? to : backAt);
        std::vector<AutomationPoint> points;
        for (const auto& p : oldCurve)
            if (p.beat < from)
                points.push_back (p);
        if (const auto before = oldAt (from); before && from > 0.0)
            points.push_back ({ from, *before });   // up to the start as it was, then a jump to the hand's value
        for (const auto& p : thin)
            points.push_back (p);
        if (! toTheEnd)
        {
            if (const auto after = oldAt (clearTo); after && doReturn)
                points.push_back ({ clearTo, *after });            // and back to what was there (Touch's return)
            else if (as.virginTerritory && ! points.empty() && ! oldAt (clearTo))
                points.back().terminator = true;                    // (virgin territory after it: a gap, as the manual's)
            for (const auto& p : oldCurve)
                if (p.beat > clearTo)
                    points.push_back (p);
        }
        std::stable_sort (points.begin(), points.end(), [] (const auto& a, const auto& b) { return a.beat < b.beat; });
        if (as.gaps != F::off && as.virginTerritory && ! trimming && ! pass.control.isStep())
        {
            // Gaps: the curve's gaps filled with the value found.
            std::vector<AutomationPoint> closed;
            for (size_t i = 0; i < points.size(); ++i)
            {
                auto p = points[i];
                const auto gap = p.terminator && i + 1 < points.size();
                p.terminator = false;
                closed.push_back (p);
                if (gap)
                {
                    closed.push_back ({ p.beat + 1.0e-3, fillValue });
                    closed.push_back ({ juce::jmax (p.beat + 2.0e-3, points[i + 1].beat - 1.0e-3), fillValue });
                }
            }
            points = std::move (closed);
        }
        (trimming ? lane->trim : lane->points) = std::move (points);

        // One-shot fills are used up; locked ones stay (the manual's One Shot vs Continuous).
        for (auto* f : { &as.toPunch, &as.toStart, &as.toEnd, &as.loop, &as.gaps })
            if (*f == F::once)
                *f = F::off;
        if (trimming && as.freezeTrim == AutomationSettings::FreezeTrim::onPassEnd)
            freezeTrimOf (*lane);

        // The control as it was set before (the lane carries the pass now); the lane shown (Reveal Parameter on Write).
        setControlValue (pass.busId, pass.control, pass.before);
        if (as.revealOnWrite)
        {
            lane->shown = true;
            if (pass.busId == Engine::masterBus)
                project.masterAutomationOpen = true;
            else if (auto* track = project.findTrack (pass.busId))
                track->automationOpen = true;
        }
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
        setSignatureAt (0, juce::jlimit (1, 32, numerator), isValidSignature (4, denominator) ? denominator : 4);
    }

    //==========================================================================
    // Signature changes (0.45.0).
    void Studio::addSignatureTrack()
    {
        if (project.signatureTrackShown)
            return;
        project.signatureTrackShown = true;
        changed();
    }

    void Studio::removeSignatureTrack()
    {
        if (! project.signatureTrackShown)
            return;
        project.signatureTrackShown = false;
        changed();
    }

    void Studio::setSignatureLocked (bool locked)
    {
        if (project.signatureLocked == locked)
            return;
        project.signatureLocked = locked;
        changed (false);
    }

    namespace
    {
        // The changes after bar 1, tidied by the map (sorted, at bars 1 and later, one a bar, valid);
        // a change to what the bar before already has is left out (it changes nothing).
        std::vector<SignatureEvent> tidySignatures (int numerator, int denominator, const std::vector<SignatureEvent>& changes)
        {
            std::vector<SignatureEvent> out;
            const SignatureMap map (numerator, denominator, changes);
            for (size_t i = 1; i < map.segments().size(); ++i)
            {
                const auto& e = map.segments()[i].event;
                const auto& before = map.segments()[i - 1].event;
                if (e.numerator != before.numerator || e.denominator != before.denominator)
                    out.push_back (e);
            }
            return out;
        }
    }

    bool Studio::setSignatureAt (int bar, int numerator, int denominator)
    {
        if (! isValidSignature (numerator, denominator) || bar < 0)
            return false;
        if (bar == 0)
        {
            if (project.numerator == numerator && project.denominator == denominator)
                return true;
            project.numerator = numerator;
            project.denominator = denominator;
            project.signatureChanges = tidySignatures (numerator, denominator, project.signatureChanges);
            changed();
            return true;
        }
        if (project.signatureLocked)
            return false;
        auto changes = project.signatureChanges;
        changes.push_back ({ bar, numerator, denominator });
        auto tidy = tidySignatures (project.numerator, project.denominator, changes);
        // (The same as the bar before: kept anyway when asked for, so it can be changed later, as an event you see.)
        if (std::none_of (tidy.begin(), tidy.end(), [bar] (const SignatureEvent& e) { return e.bar == bar; }))
        {
            tidy.push_back ({ bar, numerator, denominator });
            std::sort (tidy.begin(), tidy.end(), [] (const SignatureEvent& a, const SignatureEvent& b) { return a.bar < b.bar; });
        }
        if (tidy == project.signatureChanges)
            return true;
        project.signatureChanges = std::move (tidy);
        changed();
        return true;
    }

    bool Studio::removeSignatures (std::vector<int> bars)
    {
        if (project.signatureLocked)
            return false;
        auto changes = project.signatureChanges;
        const auto before = changes.size();
        changes.erase (std::remove_if (changes.begin(), changes.end(), [&] (const SignatureEvent& e)
                                       { return std::find (bars.begin(), bars.end(), e.bar) != bars.end(); }), changes.end());
        if (changes.size() == before)
            return false;
        project.signatureChanges = std::move (changes);
        changed();
        return true;
    }

    bool Studio::moveSignature (int fromBar, int toBar)
    {
        if (project.signatureLocked || fromBar <= 0 || toBar <= 0)
            return false;
        auto changes = project.signatureChanges;
        auto it = std::find_if (changes.begin(), changes.end(), [fromBar] (const SignatureEvent& e) { return e.bar == fromBar; });
        if (it == changes.end())
            return false;
        if (fromBar == toBar)
            return true;
        auto moved = *it;
        changes.erase (it);
        changes.erase (std::remove_if (changes.begin(), changes.end(), [toBar] (const SignatureEvent& e) { return e.bar == toBar; }), changes.end());
        moved.bar = toBar;
        changes.push_back (moved);
        std::sort (changes.begin(), changes.end(), [] (const SignatureEvent& a, const SignatureEvent& b) { return a.bar < b.bar; });
        project.signatureChanges = std::move (changes);
        changed();
        return true;
    }

    bool Studio::insertSignatureAtCursor (int numerator, int denominator)
    {
        return setSignatureAt (project.signatures().barAt (engine.getPosition()), numerator, denominator);
    }

    SignatureEvent Studio::getSignatureNow() const
    {
        return project.signatures().segmentAt (engine.getPosition()).event;
    }

    //==========================================================================
    // Markers (0.49.0).
    void Studio::addMarkerTrack()
    {
        if (project.markerTrack.shown)
            return;
        project.markerTrack.shown = true;
        changed();
    }

    void Studio::removeMarkerTrack()
    {
        if (! project.markerTrack.shown)
            return;
        project.markerTrack.shown = false;
        changed();
    }

    void Studio::setMarkerTrackLocked (bool locked)
    {
        if (project.markerTrack.locked == locked)
            return;
        project.markerTrack.locked = locked;
        changed (false);
    }

    void Studio::renameMarkerTrack (const juce::String& name)
    {
        const auto trimmed = name.trim().isEmpty() ? juce::String ("Markers") : name.trim();
        if (project.markerTrack.name == trimmed)
            return;
        project.markerTrack.name = trimmed;
        changed (false);
    }

    void Studio::setMarkerSettings (bool showLines, bool showIds, bool cycleFollows)
    {
        auto& track = project.markerTrack;
        if (track.showLines == showLines && track.showIds == showIds && track.cycleFollows == cycleFollows)
            return;
        track.showLines = showLines;
        track.showIds = showIds;
        track.cycleFollows = cycleFollows;
        changed (false);
    }

    namespace
    {
        int indexOfMarker (const MarkerTrack& track, const Marker& m)
        {
            return track.indexOf (m.id, m.cycle);
        }
    }

    int Studio::addPositionMarker (double beat, const juce::String& name)
    {
        auto& track = project.markerTrack;
        if (track.locked || ! std::isfinite (beat))
            return -1;
        Marker m;
        m.id = track.nextId (false);
        m.start = m.end = juce::jmax (0.0, beat);
        m.name = name;
        track.markers.push_back (m);
        track.sort();
        track.shown = true;
        changed();
        return indexOfMarker (track, m);
    }

    int Studio::addCycleMarker (double start, double end, const juce::String& name)
    {
        auto& track = project.markerTrack;
        if (track.locked || ! std::isfinite (start) || ! std::isfinite (end))
            return -1;
        if (end < start)
            std::swap (start, end);
        start = juce::jmax (0.0, start);
        if (end - start < 1.0e-3)
            return -1;
        Marker m;
        m.cycle = true;
        m.id = track.nextId (true);
        m.start = start;
        m.end = end;
        m.name = name;
        track.markers.push_back (m);
        track.sort();
        track.shown = true;
        changed();
        return indexOfMarker (track, m);
    }

    int Studio::addPositionMarkerAtCursor()
    {
        return addPositionMarker (engine.getPosition());
    }

    int Studio::addCycleMarkerAtLocators()
    {
        return addCycleMarker (project.loopStart, project.loopEnd);
    }

    bool Studio::setMarkers (std::vector<Marker> markers)
    {
        auto& track = project.markerTrack;
        if (track.locked)
            return false;
        MarkerTrack tidy;
        for (auto m : markers)
        {
            if (m.cycle && m.end - m.start < 1.0e-3)
                continue;
            if (m.id < 1 || tidy.indexOf (m.id, m.cycle) >= 0)
                m.id = tidy.nextId (m.cycle);   // (the manual: a marker dropped where its ID is taken gets the first free one)
            tidy.markers.push_back (m);
        }
        tidy.sort();
        if (tidy.markers == track.markers)
            return true;
        track.markers = std::move (tidy.markers);
        changed();
        return true;
    }

    bool Studio::removeMarkers (std::vector<int> indices)
    {
        auto& track = project.markerTrack;
        if (track.locked)
            return false;
        std::sort (indices.begin(), indices.end(), std::greater<>());
        indices.erase (std::unique (indices.begin(), indices.end()), indices.end());
        auto any = false;
        for (const auto i : indices)
            if (juce::isPositiveAndBelow (i, (int) track.markers.size()))
            {
                track.markers.erase (track.markers.begin() + i);
                any = true;
            }
        if (any)
            changed();
        return any;
    }

    bool Studio::removeMarkersFrom (double beat)
    {
        std::vector<int> later;
        for (int i = 0; i < (int) project.markerTrack.markers.size(); ++i)
            if (project.markerTrack.markers[(size_t) i].start >= beat - 1.0e-6)
                later.push_back (i);
        return ! later.empty() && removeMarkers (later);
    }

    bool Studio::moveMarkersToCursor (const std::vector<int>& indices)
    {
        auto markers = project.markerTrack.markers;
        const auto at = engine.getPosition();
        auto any = false;
        for (const auto i : indices)
            if (juce::isPositiveAndBelow (i, (int) markers.size()))
            {
                auto& m = markers[(size_t) i];
                const auto length = m.length();
                m.start = at;
                m.end = m.cycle ? at + length : at;
                any = true;
            }
        return any && setMarkers (std::move (markers));
    }

    void Studio::reassignMarkerIds (bool cycle)
    {
        auto& track = project.markerTrack;
        if (track.locked)
            return;
        const auto before = track.markers;
        track.reassignIds (cycle);
        if (track.markers != before)
            changed();
    }

    bool Studio::locateMarker (int index)
    {
        const auto& track = project.markerTrack;
        if (! juce::isPositiveAndBelow (index, (int) track.markers.size()))
            return false;
        const auto m = track.markers[(size_t) index];
        setPosition (m.start);
        if (track.cycleFollows)
        {
            // (A position marker's range: to the next marker's start, else a bar on: our reading.)
            const auto end = m.cycle ? m.end : track.nextStart (m.start, true).value_or (project.nextBarAfter (m.start));
            setCycle (m.start, end);
        }
        return true;
    }

    bool Studio::cycleToMarker (int index)
    {
        const auto& track = project.markerTrack;
        if (! juce::isPositiveAndBelow (index, (int) track.markers.size()) || ! track.markers[(size_t) index].cycle)
            return false;
        const auto m = track.markers[(size_t) index];
        setCycle (m.start, m.end);
        return true;
    }

    bool Studio::locateMarkerId (int id)
    {
        return locateMarker (project.markerTrack.indexOf (id, false));
    }

    bool Studio::setMarkerIdAtCursor (int id)
    {
        auto& track = project.markerTrack;
        if (track.locked || id < 1)
            return false;
        auto markers = track.markers;
        const auto at = engine.getPosition();
        if (const auto i = track.indexOf (id, false); i >= 0)
            markers[(size_t) i].start = markers[(size_t) i].end = at;
        else
        {
            Marker m;
            m.id = id;
            m.start = m.end = at;
            markers.push_back (m);
        }
        const auto wasShown = track.shown;
        const auto before = track.markers;
        track.shown = true;
        if (! setMarkers (std::move (markers)))
        {
            track.shown = wasShown;
            return false;
        }
        if (! wasShown && track.markers == before)
            changed();   // (shown, nothing else changed: still one undo step)
        return true;
    }

    bool Studio::locateNextMarker (bool forward)
    {
        const auto to = project.markerTrack.nextStart (engine.getPosition(), forward);
        if (! to)
            return false;
        for (int i = 0; i < (int) project.markerTrack.markers.size(); ++i)
            if (std::abs (project.markerTrack.markers[(size_t) i].start - *to) < 1.0e-9)
                return locateMarker (i);
        return false;
    }

    //==========================================================================
    // Tap tempo (0.45.0).
    juce::String Studio::tapModeName (TapMode mode)
    {
        switch (mode)
        {
            case TapMode::automatic:      return "Automatic";
            case TapMode::selectionStart: return "Insert Tempo Event at Selection Start";
            case TapMode::playbackStart:  return "Insert Tempo Event at Last Playback Start Position";
            case TapMode::cursor:         return "Insert Tempo Event at Cursor Position";
            case TapMode::projectTempo:   return "Set Project Tempo";
            case TapMode::displayOnly:    return "Display Only";
        }
        return {};
    }

    void Studio::setTapMode (TapMode mode)
    {
        tapMode = mode;
        if (settings != nullptr)
            settings->setValue ("tapTempoMode", (int) mode);
    }

    Studio::TapResult Studio::tapTempo (double now, std::optional<double> selectionStart)
    {
        if (! tapTimes.empty() && (now - tapTimes.back() >= tapGapSeconds || now <= tapTimes.back()))
        {
            endTapping();
            tapTimes.clear();
        }
        if (tapTimes.empty())
            lastTap = {};   // (a new run of taps: its place is chosen again)
        tapTimes.push_back (now);
        if (tapTimes.size() > 9)
            tapTimes.erase (tapTimes.begin());
        lastTap.taps = juce::jmin (lastTap.taps + 1, 1000000);
        if (tapTimes.size() < 2)
        {
            lastTap.bpm = 0.0;
            lastTap.where = "Tap again...";
            announce();
            return lastTap;
        }
        const auto seconds = (tapTimes.back() - tapTimes.front()) / (double) (tapTimes.size() - 1);
        const auto bpm = juce::jlimit (Project::minBpm, Project::maxBpm, std::round (60.0 / seconds * 100.0) / 100.0);
        lastTap.bpm = bpm;

        // Where it goes (the manual's Tap Tempo Modes), chosen at the second tap and kept for the run.
        if (lastTap.taps == 2)
        {
            auto mode = tapMode;
            lastTap.at = -1.0;
            if (mode == TapMode::automatic)
                mode = ! project.followsTempoTrack() ? TapMode::projectTempo
                     : selectionStart ? TapMode::selectionStart
                     : engine.isPlaying() ? TapMode::playbackStart : TapMode::cursor;
            if (project.followsTempoTrack() || mode == TapMode::displayOnly)
            {
                if (mode == TapMode::selectionStart)
                    lastTap.at = selectionStart ? juce::jmax (0.0, *selectionStart) : -2.0;
                else if (mode == TapMode::playbackStart)
                    lastTap.at = playStart;
                else if (mode == TapMode::cursor)
                    lastTap.at = engine.getPosition();
                else if (mode == TapMode::projectTempo)
                    lastTap.at = 0.0;
            }
            if (mode == TapMode::displayOnly || lastTap.at == -2.0)
                lastTap.where = mode == TapMode::displayOnly ? "Display only" : "Nothing selected: display only";
            else if (! project.followsTempoTrack())
                lastTap.where = "Project tempo";
            else
                lastTap.where = lastTap.at == 0.0 ? juce::String ("The first tempo event") : "Tempo event at " + formatBarsBeats (lastTap.at, project).replace ("  ", " ");
            if (mode == TapMode::displayOnly || lastTap.at == -2.0)
                lastTap.at = -3.0;   // (shown only)
        }
        if (lastTap.at == -3.0 || (project.tempoLocked && project.followsTempoTrack()))
        {
            announce();
            return lastTap;
        }
        if (! project.followsTempoTrack())
        {
            project.bpm = bpm;
        }
        else
        {
            auto events = project.tempoEvents;
            auto found = false;
            for (auto& e : events)
                if (std::abs (e.beat - lastTap.at) < 1.0e-6)
                    e.bpm = bpm, found = true;
            if (! found)
                events.push_back ({ lastTap.at, bpm, false });
            tidyTempo (events, project.bpm);
            project.tempoEvents = std::move (events);
        }
        tapUnrecorded = true;
        changed (false);
        return lastTap;
    }

    void Studio::endTapping()
    {
        if (std::exchange (tapUnrecorded, false))
            recordUndoState();
    }

    //==========================================================================
    // Tempo detection (0.45.0).
    void Studio::keepInTime (const TempoMap& before, const TempoMap& after)
    {
        const auto to = [&] (double beat) { return after.beatAt (before.secondsAt (beat)); };
        const auto movePart = [&] (Part& part)
        {
            const auto start = part.start, end = part.start + part.length;
            const auto newStart = to (start);
            for (auto& n : part.notes)
            {
                const auto a = to (start + n.start), b = to (start + n.start + n.length);
                n.start = a - newStart;
                n.length = juce::jmax (1.0e-6, b - a);
            }
            for (auto& c : part.controllers)
                c.start = to (start + c.start) - newStart;
            for (auto& m : part.warp)
                m.beat = to (start + m.beat) - newStart;
            part.start = newStart;
            part.length = juce::jmax (1.0e-6, to (end) - newStart);
            if (! part.warp.empty())
                part.warp.back().beat = part.length, part.warp.front().beat = 0.0;
        };
        const auto moveLanes = [&] (std::vector<AutomationLane>& lanes)
        {
            for (auto& lane : lanes)
            {
                for (auto& p : lane.points)
                    p.beat = to (p.beat);
                for (auto& p : lane.trim)
                    p.beat = to (p.beat);
            }
        };
        for (auto& track : project.tracks)
        {
            for (auto& part : track.parts)
                movePart (part);
            moveLanes (track.automation);
        }
        for (auto& [id, parts] : frozenParts)
            for (auto& part : parts)
                movePart (part);
        moveLanes (project.masterAutomation);
        for (auto& e : project.chordTrack.events)
            e.beat = to (e.beat);
        for (auto& e : project.chordTrack.scales)
            e.beat = to (e.beat);
        project.loopStart = to (project.loopStart);
        project.loopEnd = juce::jmax (project.loopStart + 0.0625, to (project.loopEnd));
        playStart = to (playStart);
        recordStart = to (recordStart);
        engine.setPosition (juce::jmax (0.0, to (engine.getPosition())));
    }

    bool Studio::analyseTempo (int partId, BeatAnalysis& result, juce::String& error)
    {
        result = {};
        const auto found = findPart (partId);
        const auto* owner = found ? project.findTrack (found->first) : nullptr;   // (findPart gives the track's id)
        const auto* part = owner != nullptr ? &owner->parts[(size_t) found->second] : nullptr;
        if (part == nullptr)
        {
            error = "Select an audio event or a MIDI part first.";
            return false;
        }
        const auto tempo = project.tempoMap();
        const auto from = tempo.secondsAt (part->start), to = tempo.secondsAt (part->start + part->length);
        if (to - from < leastDetectSeconds - 1.0e-6)
        {
            error = "It's " + juce::String (to - from, 1) + " s long: tempo detection needs at least " + juce::String ((int) leastDetectSeconds) + " seconds.";
            return false;
        }
        if (part->isAudio())
        {
            double rate = 0.0;
            const auto sound = clipMono (*part, from, to, rate);
            if (rate <= 0.0 || sound.empty())
            {
                error = "The recording couldn't be read.";
                return false;
            }
            result = detectBeats (sound.data(), (int) sound.size(), rate);
        }
        else
        {
            std::vector<DetectNote> notes;
            for (const auto& n : part->notes)
                if (n.start >= 0.0 && n.start < part->length)
                    notes.push_back ({ tempo.secondsAt (part->start + n.start) - from, n.velocity });
            if (notes.size() < 8)
            {
                error = "There aren't enough notes in it to find a beat.";
                return false;
            }
            result = detectBeats (notes, to - from);
        }
        if (result.beats.size() < 4)
        {
            error = "No steady beat was found in it.";
            return false;
        }
        SessionLog::note ("Tempo detection: " + juce::String (result.beats.size()) + " beats, " + juce::String (result.bpm, 2) + " BPM");
        return true;
    }

    bool Studio::applyDetectedBeats (int partId, const std::vector<double>& beatSeconds, juce::String& report)
    {
        const auto found = findPart (partId);
        const auto* owner = found ? project.findTrack (found->first) : nullptr;   // (findPart gives the track's id)
        const auto* part = owner != nullptr ? &owner->parts[(size_t) found->second] : nullptr;
        if (part == nullptr || beatSeconds.size() < 2)
        {
            report = "Nothing to apply.";
            return false;
        }
        if (project.tempoLocked)
        {
            report = "The tempo track is locked.";
            return false;
        }
        const auto before = project.tempoMap();
        const auto start = part->start;
        const auto overall = medianBpm (beatSeconds);
        const auto clamp = [] (double bpm) { return juce::jlimit (Project::minBpm, Project::maxBpm, bpm); };
        // The first beat: on the whole beat at or after the part's start that keeps the tempo
        // into it nearest the tempo overall (it can't be heard before the part starts).
        const auto first = beatSeconds.front();
        auto landing = std::ceil (start - 1.0e-6);
        auto lead = -1.0, bestMiss = 1.0e9;
        for (int m = 0; m < 8; ++m)
        {
            const auto k = std::ceil (start - 1.0e-6) + m;
            if (k - start < 1.0e-6)
            {
                if (first < 0.012)
                {
                    landing = k, lead = 0.0;   // (it's on the part's start already)
                    break;
                }
                continue;
            }
            const auto bpm = 60.0 * (k - start) / juce::jmax (1.0e-6, first);
            if (bpm < Project::minBpm || bpm > Project::maxBpm)
                continue;
            const auto miss = std::abs (std::log (bpm / overall));
            if (miss < bestMiss)
                bestMiss = miss, landing = k, lead = bpm;
        }
        std::vector<TempoEvent> events;
        for (const auto& e : project.followsTempoTrack() ? project.tempoEvents : std::vector<TempoEvent> { { 0.0, project.bpm, false } })
            if (e.beat < start - 1.0e-6)
                events.push_back (e);
        if (events.empty() && start > 1.0e-6)
            events.push_back ({ 0.0, project.bpm, false });
        auto offset = 0.0;
        if (landing > start + 1.0e-6)
        {
            if (lead <= 0.0)   // (no tempo in range lands it: it lands as near as it can, the rest is reported)
                lead = clamp (60.0 * (landing - start) / juce::jmax (1.0e-6, first));
            events.push_back ({ start, lead, false });
            offset = first - 60.0 * (landing - start) / lead;
        }
        else
            offset = first;
        for (size_t i = 0; i + 1 < beatSeconds.size(); ++i)
            events.push_back ({ landing + (double) i, clamp (60.0 / juce::jmax (1.0e-4, beatSeconds[i + 1] - beatSeconds[i])), false });
        tidyTempo (events, project.bpm);
        const TempoMap after (events);
        keepInTime (before, after);
        project.tempoEvents = std::move (events);
        project.tempoTrackOn = true;
        project.tempoTrackShown = true;
        changed();
        report = juce::String ((int) beatSeconds.size()) + " beats from " + formatBarsBeats (landing, project).replace ("  ", " ")
               + ", " + juce::String (overall, 1) + " BPM on average";
        if (std::abs (offset) > 0.005)
            report << ". The first beat is " << juce::roundToInt (std::abs (offset) * 1000.0) << " ms " << (offset > 0.0 ? "after" : "before") << " its beat line";
        SessionLog::note ("Tempo detection applied: " + report);
        return true;
    }

    void Studio::restoreTempo (bool on, std::vector<TempoEvent> events)
    {
        const auto before = project.tempoMap();
        tidyTempo (events, project.bpm);
        const auto after = on ? TempoMap (events) : TempoMap (project.bpm);
        keepInTime (before, after);
        project.tempoEvents = std::move (events);
        project.tempoTrackOn = on;
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
        if (composerAudition.has_value())
            stopComposerAudition();   // (0.58.0: the song's own play, from where it was)
        if (engine.isPlaying())
            return;
        playStart = engine.getPosition();
        engine.play();
        announce();
    }

    void Studio::stop()
    {
        if (composerAudition.has_value())
        {
            stopComposerAudition();   // (0.58.0: the transport's stop ends hearing the composer)
            return;
        }
        if (! engine.isPlaying())
        {
            // Stopped already: back to where playing started (inspired by Cubase's workflow).
            engine.setPosition (playStart);
            announce();
            return;
        }
        if (engine.isRecording())
            finishRecording();
        punchOutAll();   // writing automation stops with playing
        engine.stop();
        announce();
    }

    void Studio::record()
    {
        endComposerAudition (true);   // (0.58.0: the song again, then recording)
        if (engine.isRecording())
        {
            finishRecording();   // punch out: playing carries on
            announce();
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
        {
            const std::scoped_lock l (chordLogLock);
            chordLog.clear();
        }
        loggingChords = project.chordTrack.record;   // (0.44.0)

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
        announce();
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
        announce();
    }

    //==========================================================================
    void Studio::buildRecordingNotes (std::vector<Note>& notes, double partStart, double endBeat, int onlyPass) const
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
            if (onlyPass >= 0 && event.pass != onlyPass)
                continue;   // (a lap of its own: 0.29.0's stacked takes)
            const auto status = event.status & 0xf0;
            if (status != 0x90 && status != 0x80)
                continue;   // (controllers: buildRecordingControllers)
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

    void Studio::buildRecordingControllers (std::vector<Controller>& events, double partStart) const
    {
        // What was played on the controllers (the mod wheel, the bend, the pedal...). In a
        // cycle each kind keeps only the last pass it was played in (as notes are merged,
        // passes of a sweep would cross one another).
        std::map<int, int> lastPass;
        const auto codeFor = [] (const RecordedEvent& e) -> int
        {
            switch (e.status & 0xf0)
            {
                case 0xb0: return e.data1 & 0x7f;
                case 0xe0: return ControllerKinds::pitchbend;
                case 0xd0: return ControllerKinds::aftertouch;
                case 0xc0: return ControllerKinds::programChange;
                case 0xa0: return ControllerKinds::polyBase + (e.data1 & 0x7f);
                default:   return -2;
            }
        };
        for (const auto& e : recorded)
            if (const auto code = codeFor (e); code != -2)
                lastPass[code] = juce::jmax (lastPass[code], e.pass);
        const auto ramp = getNewControllersRamp();
        for (const auto& e : recorded)
        {
            const auto code = codeFor (e);
            if (code == -2 || e.pass != lastPass[code])
                continue;
            const auto value = code == ControllerKinds::pitchbend ? (e.data1 & 0x7f) | ((e.data2 & 0x7f) << 7)
                             : (e.status & 0xf0) == 0xd0 || (e.status & 0xf0) == 0xc0 ? (e.data1 & 0x7f) : (e.data2 & 0x7f);
            events.push_back ({ ControllerKinds::typeOf (code), ControllerKinds::numberOf (code), juce::jmax (0.0, e.beat - partStart),
                                value, ramp && ! ControllerKinds::alwaysStep (code) });
        }
        ControllerKinds::sort (events);
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
        part.start = project.loopOn ? project.loopStart : project.barStartAt (recordStart);
        const auto now = engine.getPosition();
        buildRecordingNotes (part.notes, part.start, now);
        buildRecordingControllers (part.controllers, part.start);
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
        forgetAudio (name);
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
        // In a cycle, the file holds every lap one after another (0.29.0): each lap a take of its
        // own, cut from it (the first from where recording began; a last lap stopped on the way,
        // if a beat or more was played); the record mode decides what's kept: Keep History all
        // of them and what was there, Cycle History + Replace all of them in place of what was
        // there, Replace only the last complete lap (the manual's Audio Record Modes).
        const auto toSamples = [rate, &tempo] (double beats) { return (juce::int64) std::llround (tempo.secondsAt (beats) * rate); };
        const auto loopStart = toSamples (project.loopStart), loopEnd = toSamples (project.loopEnd);
        using Audio = Project::RecordModes::Audio;
        const auto mode = project.recordModes.audio;
        std::vector<Part> takes;
        if (project.loopOn && startSample < loopEnd && loopEnd > loopStart)
        {
            const auto played = (juce::int64) data->sound.getNumSamples() - (juce::int64) std::llround (latency * rate);
            const auto firstPass = loopEnd - startSample, pass = loopEnd - loopStart;
            const auto laps = played > firstPass ? (played - firstPass) / pass : 0;   // complete laps after the first
            const auto lapAt = [&] (juce::int64 into) // a lap from `into` samples into what was played
            {
                Part lap = part;
                lap.start = project.loopStart;
                lap.audioOffset = latency + (double) into / rate;
                lap.length = project.loopEnd - project.loopStart;
                return lap;
            };
            auto first = part;
            first.length = juce::jmin (part.length, tempo.beatAfter (part.start, (double) firstPass / rate) - part.start);
            if (mode != Audio::replace || laps == 0)
                takes.push_back (first);
            for (juce::int64 l = 0; l < laps; ++l)
                if (mode != Audio::replace || l == laps - 1)
                    takes.push_back (lapAt (firstPass + l * pass));
            const auto rest = played - firstPass - laps * pass;
            if (mode != Audio::replace && laps >= 1 && rest > 0)
            {
                auto last = lapAt (firstPass + laps * pass);
                last.length = tempo.beatAfter (last.start, (double) rest / rate) - last.start;
                if (last.length >= 1.0)
                    takes.push_back (last);
            }
        }
        else
        {
            takes.push_back (part);
        }
        if (auto* track = project.findTrack (recordTrack))
        {
            for (size_t t = 0; t < takes.size(); ++t)
            {
                takes[t].name = track->name;
                if (takes.size() > 1)
                    takes[t].name << " take " << (int) (t + 1);
            }
            auto from = takes.front().start, to = takes.front().end();
            for (const auto& take : takes)
                from = juce::jmin (from, take.start), to = juce::jmax (to, take.end());
            placeTakes (*track, std::move (takes), from, to, mode != Audio::keepHistory);
            changed();
        }
    }

    void Studio::placeTakes (Track& track, std::vector<Part> takes, double from, double to, bool replace, int lane)
    {
        if (replace)
        {
            // What's there over the recording's stretch is cut out.
            splitTrackAt (track, from, project.tempoMap());
            splitTrackAt (track, to, project.tempoMap());
            track.parts.erase (std::remove_if (track.parts.begin(), track.parts.end(), [&] (const Part& p)
                               {
                                   return p.isAudio() == track.isAudio() && inside (p, from, to);
                               }), track.parts.end());
        }
        auto order = 0;
        for (const auto& p : track.parts)
            order = juce::jmax (order, p.order);
        const auto overlaps = [] (const Part& a, const Part& b) { return a.start < b.end() - 1.0e-9 && b.start < a.end() - 1.0e-9; };
        auto stacked = false;
        for (auto& take : takes)
        {
            take.order = ++order;
            take.lane = juce::jmax (0, lane);
            if (lane < 0)
                for (;; ++take.lane)
                {
                    const auto taken = std::any_of (track.parts.begin(), track.parts.end(), [&] (const Part& p)
                                                    {
                                                        return p.lane == take.lane && overlaps (p, take);
                                                    });
                    if (! taken)
                        break;
                }
            stacked = stacked || take.lane > 0;
            track.parts.push_back (std::move (take));
        }
        // Takes stacked up on lanes: shown (our choice: the manual has Show Lanes clicked by hand).
        if (stacked)
            track.showLanes = true;
    }

    //==========================================================================
    // Takes and comping (0.29.0)
    namespace
    {
        bool overlapping (const Part& a, const Part& b)
        {
            return a.start < b.end() - edge && b.start < a.end() - edge;
        }

        int topOrder (const Track& track)
        {
            auto order = 0;
            for (const auto& p : track.parts)
                order = juce::jmax (order, p.order);
            return order;
        }

        // Pieces on one lane cut from one recording (or MIDI) end to end, alike, meeting
        // between `from` and `to`: one again.
        void glueAdjacent (Track& track, const TempoMap& tempo, double from, double to)
        {
            std::sort (track.parts.begin(), track.parts.end(), [] (const Part& a, const Part& b)
                       { return a.lane != b.lane ? a.lane < b.lane : a.start < b.start; });
            for (size_t i = 0; i + 1 < track.parts.size();)
            {
                auto& a = track.parts[i];
                const auto& b = track.parts[i + 1];
                auto joins = a.lane == b.lane && std::abs (a.end() - b.start) < 1.0e-6 && a.muted == b.muted && a.order == b.order
                          && ! a.isWarped() && ! b.isWarped() && a.transpose == b.transpose && a.velocityOffset == b.velocityOffset
                          && a.isAudio() == b.isAudio() && b.start > from + edge && b.start < to - edge;
                if (joins && a.isAudio())
                {
                    const auto seconds = tempo.secondsBetween (a.start, a.end());
                    joins = a.audioFile == b.audioFile && a.fadeOut <= 0.0 && b.fadeIn <= 0.0 && a.volumeDb == b.volumeDb
                         && std::abs (b.audioOffset - (a.audioOffset + seconds)) < 1.0e-4;
                }
                if (! joins)
                {
                    ++i;
                    continue;
                }
                const auto shift = a.length;
                for (auto n : b.notes) { n.start += shift; a.notes.push_back (n); }
                for (auto c : b.controllers) { c.start += shift; a.controllers.push_back (c); }
                ControllerKinds::sort (a.controllers);
                a.length += b.length;
                a.fadeOut = b.fadeOut;
                track.parts.erase (track.parts.begin() + (std::ptrdiff_t) i + 1);
            }
        }
    }

    void Studio::setShowLanes (int trackId, bool shown)
    {
        if (auto* track = project.findTrack (trackId); track != nullptr && ! track->isChannel() && track->showLanes != shown)
        {
            track->showLanes = shown;
            changed (false);
        }
    }

    void Studio::setLaneSolo (int trackId, int lane)
    {
        if (auto* track = project.findTrack (trackId); track != nullptr && ! track->isChannel())
        {
            track->soloLane = track->soloLane == lane ? -1 : juce::jmax (-1, lane);
            changed (false);
        }
    }

    void Studio::setRecordModes (const Project::RecordModes& modes)
    {
        project.recordModes = modes;
        changed (false);
    }

    namespace
    {
        AutoFades limitedFades (AutoFades f)
        {
            f.milliseconds = juce::jlimit (1.0, 500.0, f.milliseconds);
            return f;
        }
    }

    void Studio::setAutoFades (const AutoFades& fades)
    {
        project.autoFades = limitedFades (fades);
        changed (false);
    }

    void Studio::setTrackAutoFades (int trackId, std::optional<AutoFades> fades)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || ! track->isAudio())
            return;
        if (fades)
            fades = limitedFades (*fades);
        track->autoFades = fades;
        changed (false);
    }

    std::vector<int> Studio::partsOnLane (int trackId, int lane) const
    {
        std::vector<int> ids;
        if (const auto* track = project.findTrack (trackId))
            for (const auto& p : track->parts)
                if (p.lane == lane)
                    ids.push_back (p.id);
        return ids;
    }

    int Studio::frontPartAt (int trackId, double beat) const
    {
        if (const auto* track = project.findTrack (trackId))
            for (int i = 0; i < (int) track->parts.size(); ++i)
                for (const auto& [from, to] : heardStretches (*track, i))
                    if (beat >= from && beat < to)
                        return track->parts[(size_t) i].id;
        return 0;
    }

    void Studio::bringToFront (int partId, bool toggle)
    {
        Track* track = nullptr;
        auto index = -1;
        for (auto& t : project.tracks)
            for (int i = 0; i < (int) t.parts.size(); ++i)
                if (t.parts[(size_t) i].id == partId)
                    track = &t, index = i;
        if (track == nullptr || track->frozen)
            return;
        auto& part = track->parts[(size_t) index];
        if (track->isAudio())
        {
            auto inFront = ! part.muted;
            auto behind = -1;   // the clip in front of the rest under it
            for (int j = 0; j < (int) track->parts.size(); ++j)
                if (j != index && overlapping (track->parts[(size_t) j], part))
                {
                    if (isInFront (*track, j, index))
                        inFront = false;
                    else if (behind < 0 || isInFront (*track, j, behind))
                        behind = j;
                }
            if (toggle && inFront && behind >= 0)
            {
                track->parts[(size_t) behind].order = topOrder (*track) + 1;   // the one it hid comes back
                track->parts[(size_t) behind].muted = false;
            }
            else
            {
                part.order = topOrder (*track) + 1;
                part.muted = false;
            }
        }
        else if (toggle && ! part.muted)
        {
            part.muted = true;   // (MIDI: To Front clicked again mutes it, as the manual)
        }
        else
        {
            part.muted = false;
            for (auto& other : track->parts)
                if (other.id != partId && other.lane != part.lane && overlapping (other, part))
                    other.muted = true;
        }
        changed();
    }

    void Studio::compRange (int trackId, int lane, double start, double end)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || track->isChannel() || track->frozen || end <= start + edge)
            return;
        const auto tempo = project.tempoMap();
        splitTrackAt (*track, start, tempo);
        splitTrackAt (*track, end, tempo);
        const auto order = topOrder (*track) + 1;
        auto any = false;
        for (auto& p : track->parts)
            if (p.lane == lane && inside (p, start, end))
            {
                p.order = order;   // (all the range's pieces alike: they glue back together)
                p.muted = false;
                any = true;
            }
        if (! any)
        {
            changed();   // (cut anyway, as the manual: "all takes are split")
            return;
        }
        if (! track->isAudio())
            for (auto& p : track->parts)
                if (p.lane != lane && inside (p, start, end))
                    p.muted = true;
        glueAdjacent (*track, tempo, start, end);
        changed();
    }

    void Studio::setPartLane (int partId, int lane)
    {
        for (auto& t : project.tracks)
            for (auto& p : t.parts)
                if (p.id == partId && ! t.frozen && p.lane != juce::jlimit (0, 255, lane))
                {
                    p.lane = juce::jlimit (0, 255, lane);
                    changed();
                    return;
                }
    }

    void Studio::cleanUpLanes (int trackId)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || track->isChannel() || track->frozen)
            return;
        const auto tempo = project.tempoMap();
        auto heard = *track;
        heard.soloLane = -1;   // (what's heard with no lane soloed)
        std::vector<Part> kept;
        for (int i = 0; i < (int) track->parts.size(); ++i)
        {
            const auto& part = track->parts[(size_t) i];
            const auto covered = std::any_of (track->parts.begin(), track->parts.end(), [&] (const Part& o)
                                              { return &o != &part && overlapping (o, part); });
            if (! covered)
            {
                kept.push_back (part);
                continue;
            }
            if (track->isAudio())
            {
                // Only the stretches heard stay.
                for (const auto& [from, to] : heardStretches (heard, i))
                    if (auto piece = slice (part, from, to, tempo))
                    {
                        piece->id = from <= part.start + edge ? part.id : 0;
                        kept.push_back (*piece);
                    }
            }
            else if (! part.muted || ! std::any_of (track->parts.begin(), track->parts.end(), [&] (const Part& o)
                                                    { return &o != &part && ! o.muted && overlapping (o, part); }))
            {
                kept.push_back (part);   // (MIDI: a muted part under one heard goes)
            }
        }
        // The lanes left, numbered from 0 again.
        std::vector<int> used;
        for (const auto& p : kept)
            if (std::find (used.begin(), used.end(), p.lane) == used.end())
                used.push_back (p.lane);
        std::sort (used.begin(), used.end());
        for (auto& p : kept)
            p.lane = (int) (std::find (used.begin(), used.end(), p.lane) - used.begin());
        track->parts = std::move (kept);
        if (track->soloLane >= (int) used.size())
            track->soloLane = -1;
        changed();
    }

    std::vector<int> Studio::createTracksFromLanes (int trackId)
    {
        std::vector<int> made;
        const auto* track = project.findTrack (trackId);
        if (track == nullptr || track->isChannel() || track->frozen)
            return made;
        const auto lanes = track->numLanes();
        for (int lane = lanes - 1; lane >= 1; --lane)   // (each straight under the original: the last first)
        {
            if (partsOnLane (trackId, lane).empty())
                continue;
            const auto id = duplicateTrack (trackId);
            if (id == 0)
                continue;
            auto* copy = project.findTrack (id);
            copy->name = project.findTrack (trackId)->name + " lane " + juce::String (lane + 1);
            copy->showLanes = false;
            copy->soloLane = -1;
            copy->solo = false;
            copy->parts.erase (std::remove_if (copy->parts.begin(), copy->parts.end(), [lane] (const Part& p) { return p.lane != lane; }),
                               copy->parts.end());
            for (auto& p : copy->parts)
                p.lane = 0;
            made.insert (made.begin(), id);
        }
        if (auto* original = project.findTrack (trackId))
        {
            original->parts.erase (std::remove_if (original->parts.begin(), original->parts.end(), [] (const Part& p) { return p.lane != 0; }),
                                   original->parts.end());
            original->soloLane = -1;
        }
        changed();
        return made;
    }

    //==========================================================================
    // Hitpoints and warping (0.32.0)
    std::pair<Track*, int> Studio::findPartById (int partId)
    {
        for (auto& t : project.tracks)
            for (int i = 0; i < (int) t.parts.size(); ++i)
                if (t.parts[(size_t) i].id == partId)
                    return { &t, i };
        return { nullptr, -1 };
    }

    std::shared_ptr<const juce::AudioBuffer<float>> Studio::getWarpedSound (const Part& part, const TempoMap& tempo)
    {
        // (0.34.0) The recording as VariAudio left it; the stretches made from an earlier version forgotten.
        const auto signature = variSignature (part.audioFile);
        if (auto& known = warpedVariSignatures[part.audioFile]; known != signature)
        {
            warpRenderer.clear();
            known = signature;
        }
        const auto data = getHeardAudio (part.audioFile);
        if (data == nullptr)
            return nullptr;
        return warpRenderer.render (part, data->sound, data->sampleRate, tempo);
    }

    std::vector<double> Studio::getHitpoints (const juce::String& recording)
    {
        // (0.34.1) Kept with the recording they came from: a file of the same name in another song,
        // or written again, is found afresh.
        const auto data = getAudio (recording);
        auto found = hitpointCache.find (recording);
        if (found == hitpointCache.end() || found->second.first != data)
        {
            std::vector<Hitpoint> all;
            if (data != nullptr)
                all = detectHitpoints (data->sound, data->sampleRate);
            found = hitpointCache.insert_or_assign (recording, std::pair { data, std::move (all) }).first;
        }
        const auto edits = project.hitpointEdits.find (recording);
        return filterHitpoints (found->second.second, project.hitpointSettings.threshold, project.hitpointSettings.minimumSeconds,
                                edits != project.hitpointEdits.end() ? &edits->second : nullptr);
    }

    std::vector<double> Studio::getPartHitpoints (int partId)
    {
        std::vector<double> beats;
        const auto [track, index] = findPartById (partId);
        if (track == nullptr || ! track->parts[(size_t) index].isAudio())
            return beats;
        const auto part = track->parts[(size_t) index];
        const auto tempo = project.tempoMap();
        for (const auto seconds : getHitpoints (part.audioFile))
        {
            const auto beat = warpBeatAt (part, seconds, tempo);
            if (beat > 1.0e-6 && beat < part.length - 1.0e-6)
                beats.push_back (beat);
        }
        std::sort (beats.begin(), beats.end());
        return beats;
    }

    void Studio::setHitpointSettings (const Project::HitpointSettings& chosen)
    {
        project.hitpointSettings = { juce::jlimit (0.0, 1.0, chosen.threshold), juce::jlimit (0.0, 2.0, chosen.minimumSeconds) };
        changed (false);
    }

    void Studio::addHitpoint (const juce::String& recording, double seconds)
    {
        auto& edits = project.hitpointEdits[recording];
        edits.removed.erase (std::remove_if (edits.removed.begin(), edits.removed.end(), [seconds] (double r) { return std::abs (r - seconds) < 0.01; }),
                             edits.removed.end());
        edits.added.push_back (juce::jmax (0.0, seconds));
        changed (false);
    }

    void Studio::removeHitpoint (const juce::String& recording, double seconds)
    {
        auto& edits = project.hitpointEdits[recording];
        const auto added = std::find_if (edits.added.begin(), edits.added.end(), [seconds] (double a) { return std::abs (a - seconds) < 0.01; });
        if (added != edits.added.end())
            edits.added.erase (added);
        else
            edits.removed.push_back (seconds);
        changed (false);
    }

    void Studio::moveHitpoint (const juce::String& recording, double from, double to)
    {
        removeHitpoint (recording, from);
        addHitpoint (recording, to);
    }

    void Studio::resetHitpoints (const juce::String& recording)
    {
        project.hitpointEdits.erase (recording);
        changed (false);
    }

    std::optional<double> Studio::nextHitpoint (int trackId, double fromBeat, bool forward)
    {
        const auto* track = project.findTrack (trackId);
        if (track == nullptr || ! track->isAudio())
            return std::nullopt;
        std::optional<double> best;
        for (int i = 0; i < (int) track->parts.size(); ++i)
        {
            const auto& part = track->parts[(size_t) i];
            if (! part.isAudio())
                continue;
            const auto heard = heardStretches (*track, i);
            for (const auto beat : getPartHitpoints (part.id))
            {
                const auto at = part.start + beat;
                const auto audible = std::any_of (heard.begin(), heard.end(), [at] (const auto& h) { return at >= h.first && at < h.second; });
                if (! audible)
                    continue;
                if (forward ? at > fromBeat + 1.0e-6 && (! best || at < *best)
                            : at < fromBeat - 1.0e-6 && (! best || at > *best))
                    best = at;
            }
        }
        return best;
    }

    void Studio::tidyWarp (Part& part)
    {
        // Checked: in order, apart, inside the recording; the edges at the clip's.
        auto& m = part.warp;
        std::sort (m.begin(), m.end(), [] (const WarpMarker& a, const WarpMarker& b) { return a.beat < b.beat; });
        const auto data = getAudio (part.audioFile);
        const auto fileSeconds = data != nullptr ? data->seconds() : 1.0e9;
        std::vector<WarpMarker> kept;
        for (const auto& marker : m)
        {
            if (! kept.empty() && (marker.beat < kept.back().beat + 1.0 / 256.0 || marker.seconds < kept.back().seconds + 0.005))
                continue;
            kept.push_back ({ marker.beat, juce::jlimit (0.0, fileSeconds, marker.seconds) });
        }
        if (kept.size() >= 2)
        {
            kept.front().beat = 0.0;
            kept.back().beat = part.length;
            part.audioOffset = kept.front().seconds;
        }
        else
            kept.clear();
        m = std::move (kept);
    }

    bool Studio::editWarp (int partId, const std::function<bool (Part&)>& edit)
    {
        auto [track, index] = findPartById (partId);
        if (track == nullptr || track->frozen || ! track->parts[(size_t) index].isAudio())
            return false;
        auto part = track->parts[(size_t) index];
        if (! edit (part))
            return false;
        tidyWarp (part);
        track->parts[(size_t) index] = part;
        changed();
        return true;
    }

    bool Studio::createWarpFromHitpoints (const std::vector<int>& partIds)
    {
        auto any = false;
        for (const auto id : partIds)
        {
            const auto beats = getPartHitpoints (id);
            any = editWarp (id, [&] (Part& part)
            {
                ensureWarp (part, project.tempoMap());
                const auto tempo = project.tempoMap();
                for (const auto beat : beats)
                    if (std::none_of (part.warp.begin(), part.warp.end(), [beat] (const WarpMarker& m) { return std::abs (m.beat - beat) < 1.0 / 256.0; }))
                        part.warp.push_back ({ beat, warpSecondsAt (part, beat, tempo) });
                return true;
            }) || any;
        }
        return any;
    }

    bool Studio::addWarpMarker (int partId, double beat)
    {
        return editWarp (partId, [&] (Part& part)
        {
            if (beat <= 1.0e-6 || beat >= part.length - 1.0e-6)
                return false;
            const auto tempo = project.tempoMap();
            ensureWarp (part, tempo);
            const auto seconds = warpSecondsAt (part, beat, tempo);
            part.warp.push_back ({ beat, seconds });
            return true;
        });
    }

    bool Studio::moveWarpMarker (int partId, int index, double toBeat)
    {
        return editWarp (partId, [&] (Part& part)
        {
            if (! part.isWarped() || index <= 0 || index >= (int) part.warp.size() - 1)
                return false;   // (the edges stay)
            const auto low = part.warp[(size_t) index - 1].beat + 1.0 / 256.0, high = part.warp[(size_t) index + 1].beat - 1.0 / 256.0;
            if (low >= high)
                return false;
            part.warp[(size_t) index].beat = juce::jlimit (low, high, toBeat);
            return true;
        });
    }

    bool Studio::removeWarpMarkers (int partId, std::vector<int> indices)
    {
        return editWarp (partId, [&] (Part& part)
        {
            std::sort (indices.begin(), indices.end(), std::greater<>());
            auto any = false;
            for (const auto i : indices)
                if (i > 0 && i < (int) part.warp.size() - 1)
                {
                    part.warp.erase (part.warp.begin() + i);
                    any = true;
                }
            if (part.warp.size() == 2 && std::abs (warpFactors (part, project.tempoMap()).back() - 1.0) < 1.0e-6)
                part.warp.clear();   // (back at its own speed: no markers needed)
            return any;
        });
    }

    void Studio::setWarpMarkers (int partId, std::vector<WarpMarker> markers)
    {
        editWarp (partId, [&] (Part& part)
        {
            part.warp = std::move (markers);
            return true;
        });
    }

    void Studio::resetWarp (const std::vector<int>& partIds)
    {
        auto any = false;
        for (auto& track : project.tracks)
            if (! track.frozen)
                for (auto& part : track.parts)
                    if (part.isWarped() && std::find (partIds.begin(), partIds.end(), part.id) != partIds.end())
                    {
                        part.audioOffset = part.warp.front().seconds;
                        part.warp.clear();
                        any = true;
                    }
        if (any)
            changed();
    }

    int Studio::flattenWarp (const std::vector<int>& partIds, juce::String& error)
    {
        auto count = 0;
        const auto tempo = project.tempoMap();
        for (auto& track : project.tracks)
        {
            if (track.frozen)
                continue;
            for (auto& part : track.parts)
            {
                if (! part.isWarped() || std::find (partIds.begin(), partIds.end(), part.id) == partIds.end())
                    continue;
                const auto data = getAudio (part.audioFile);
                const auto sound = getWarpedSound (part, tempo);
                if (data == nullptr || sound == nullptr)
                {
                    error << part.name << ": its recording couldn't be read\n";
                    continue;
                }
                auto folder = getAudioFolder();
                folder.createDirectory();
                juce::File flattened;
                int n = 1;
                do
                    flattened = folder.getChildFile (juce::File::createLegalFileName (track.name) + " flattened " + juce::String (n++).paddedLeft ('0', 2) + ".wav");
                while (flattened.exists());
                std::unique_ptr<juce::OutputStream> stream (flattened.createOutputStream());
                std::unique_ptr<juce::AudioFormatWriter> writer;
                if (stream != nullptr)
                    writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (data->sampleRate)
                                                                                           .withNumChannels (sound->getNumChannels())
                                                                                           .withBitsPerSample (32)
                                                                                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
                if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (*sound, 0, sound->getNumSamples()))
                {
                    error << part.name << ": couldn't write " << flattened.getFullPathName() << "\n";
                    continue;
                }
                writer.reset();
                forgetAudio (flattened.getFileName());
                part.audioFile = flattened.getFileName();
                part.audioOffset = 0.0;
                part.warp.clear();
                ++count;
            }
        }
        if (count > 0)
            changed();
        return count;
    }

    bool Studio::quantizeAudio (const std::vector<int>& partIds, double grid)
    {
        if (grid <= 0.0)
            return false;
        auto any = false;
        const auto tempo = project.tempoMap();
        if (project.audioWarpQuantize)
        {
            // The markers (from the hitpoints, the first time) to the grid, in one undo step.
            std::vector<std::pair<int, std::vector<double>>> hits;
            for (const auto id : partIds)
                hits.push_back ({ id, getPartHitpoints (id) });
            for (auto& track : project.tracks)
            {
                if (track.frozen || ! track.isAudio())
                    continue;
                for (auto& part : track.parts)
                {
                    const auto it = std::find_if (hits.begin(), hits.end(), [&part] (const auto& h) { return h.first == part.id; });
                    if (it == hits.end() || ! part.isAudio())
                        continue;
                    const auto before = part.warp;
                    ensureWarp (part, tempo);
                    if (part.warp.size() <= 2)
                        for (const auto beat : it->second)
                            part.warp.push_back ({ beat, warpSecondsAt (part, beat, tempo) });
                    std::sort (part.warp.begin(), part.warp.end(), [] (const WarpMarker& a, const WarpMarker& b) { return a.beat < b.beat; });
                    quantizeWarp (part, grid, tempo);
                    any = any || part.warp != before;
                }
            }
        }
        else
        {
            // The clips' starts to the nearest grid line (the manual's Quantizing Audio Event Starts).
            for (auto& track : project.tracks)
                if (! track.frozen && track.isAudio())
                    for (auto& part : track.parts)
                        if (part.isAudio() && std::find (partIds.begin(), partIds.end(), part.id) != partIds.end())
                        {
                            const auto to = juce::jmax (0.0, snap (part.start, grid));
                            any = any || std::abs (to - part.start) > 1.0e-9;
                            part.start = to;
                        }
        }
        if (any)
            changed();
        return any;
    }

    void Studio::setAudioWarpQuantize (bool on)
    {
        project.audioWarpQuantize = on;
        changed (false);
    }

    //==========================================================================
    // Comping, part two (0.30.0)
    namespace
    {
        constexpr double shortestTake = 1.0 / 64.0;   // (beats: a take is never cut shorter)

        // A part's track and index by its id.
        std::pair<Track*, int> partById (Project& project, int partId)
        {
            for (auto& t : project.tracks)
                for (int i = 0; i < (int) t.parts.size(); ++i)
                    if (t.parts[(size_t) i].id == partId)
                        return { &t, i };
            return { nullptr, -1 };
        }

        // A part moved to start at `newStart` (its end staying): what it plays stays where it was.
        void moveStart (Part& part, double newStart, const TempoMap& tempo)
        {
            if (part.isWarped())
            {
                auto moved = part;
                moved.length = part.end() - newStart;
                moved.start = newStart;
                rebaseWarp (part, moved, tempo);
                part = moved;
                return;
            }
            const auto delta = newStart - part.start;
            if (part.isAudio())
                part.audioOffset = juce::jmax (0.0, part.audioOffset + (delta >= 0.0 ? tempo.secondsBetween (part.start, newStart)
                                                                                      : -tempo.secondsBetween (newStart, part.start)));
            for (auto& n : part.notes) n.start -= delta;
            for (auto& c : part.controllers) c.start -= delta;
            part.notes.erase (std::remove_if (part.notes.begin(), part.notes.end(), [] (const Note& n) { return n.start + n.length <= 0.0; }), part.notes.end());
            part.start = newStart;
            part.length -= delta;
        }
    }

    std::vector<int> Studio::stackedWith (int partId) const
    {
        std::vector<int> ids;
        for (const auto& t : project.tracks)
            for (const auto& p : t.parts)
                if (p.id == partId)
                    for (const auto& o : t.parts)
                        if (std::abs (o.start - p.start) < 1.0e-6 && std::abs (o.end() - p.end()) < 1.0e-6)
                            ids.push_back (o.id);
        return ids;
    }

    void Studio::splitTakesAt (int trackId, double beat)
    {
        if (auto* track = project.findTrack (trackId); track != nullptr && ! track->frozen && ! track->isChannel())
        {
            splitTrackAt (*track, beat, project.tempoMap());
            changed();
        }
    }

    double Studio::moveTakeEdge (int trackId, double beat, double newBeat)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || track->frozen || track->isChannel())
            return beat;
        const auto tempo = project.tempoMap();
        const auto at = [beat] (double b) { return std::abs (b - beat) < 1.0e-6; };
        auto low = 0.0, high = 1.0e9;
        auto any = false;
        for (const auto& p : track->parts)
        {
            const auto starts = at (p.start), ends = at (p.end());
            if (! starts && ! ends)
                continue;
            any = true;
            if (ends)
            {
                low = juce::jmax (low, p.start + shortestTake);
                if (p.isAudio())
                    if (const auto data = getAudio (p.audioFile))
                        high = juce::jmin (high, tempo.beatAfter (p.start, data->seconds() - p.audioOffset));
                // Not past the next take on its lane (one starting here moves with it).
                for (const auto& o : track->parts)
                    if (&o != &p && o.lane == p.lane && o.start > beat + 1.0e-6)
                        high = juce::jmin (high, o.start);
            }
            if (starts)
            {
                high = juce::jmin (high, p.end() - shortestTake);
                if (p.isAudio())
                    low = juce::jmax (low, tempo.beatAt (juce::jmax (0.0, tempo.secondsAt (p.start) - p.audioOffset)));
                for (const auto& o : track->parts)
                    if (&o != &p && o.lane == p.lane && o.end() < beat - 1.0e-6)
                        low = juce::jmax (low, o.end());
            }
        }
        if (! any || low > high)
            return beat;
        newBeat = juce::jlimit (low, high, newBeat);
        if (at (newBeat))
            return beat;
        for (auto& p : track->parts)
        {
            const auto starts = at (p.start), ends = at (p.end());
            if (ends && ! starts)
            {
                const auto before = p;
                p.length = newBeat - p.start;
                rebaseWarp (before, p, tempo);   // (0.32.0)
            }
            else if (starts && ! ends)
                moveStart (p, newBeat, tempo);
        }
        changed();
        return newBeat;
    }

    void Studio::slipPart (int partId, double beats)
    {
        auto [track, index] = partById (project, partId);
        if (track == nullptr || track->frozen || std::abs (beats) < 1.0e-9)
            return;
        auto& part = track->parts[(size_t) index];
        if (part.isAudio())
        {
            const auto tempo = project.tempoMap();
            // Later (positive): the recording starts further back, so less offset.
            const auto seconds = beats >= 0.0 ? tempo.secondsBetween (part.start, part.start + beats) : -tempo.secondsBetween (part.start + beats, part.start);
            auto offset = part.audioOffset - seconds;
            if (const auto data = getAudio (part.audioFile))
                offset = juce::jmin (offset, juce::jmax (0.0, data->seconds() - tempo.secondsBetween (part.start, part.end())));
            offset = juce::jmax (0.0, offset);
            if (std::abs (offset - part.audioOffset) < 1.0e-9)
                return;
            part.audioOffset = offset;
        }
        else
        {
            for (auto& n : part.notes) n.start += beats;
            for (auto& c : part.controllers) c.start = juce::jmax (0.0, c.start + beats);
            std::stable_sort (part.notes.begin(), part.notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
        }
        changed();
    }

    // One clip of `among` (a track's copy) added into `sound` as it plays: its stretched sound when
    // warped, its volume, its own fades, the auto fades and crossfades (0.52.0); `sound` starts at
    // `startSeconds` in the song (Bounce, 0.30.0; a part dragged out, 0.53.1).
    void Studio::mixHeardClip (juce::AudioBuffer<float>& sound, const Track& among, int i, double startSeconds, double rate)
    {
        const auto tempo = project.tempoMap();
        const auto total = sound.getNumSamples();
        const auto& p = among.parts[(size_t) i];
        const auto data = getHeardAudio (p.audioFile);
        if (data == nullptr || ! p.isAudio())
            return;
        // (0.32.0) A warped clip: its stretched sound, from its start.
        const auto warped = p.isWarped() ? getWarpedSound (p, tempo) : nullptr;
        const auto& heardSound = warped != nullptr ? *warped : data->sound;
        const auto fromOffset = warped != nullptr ? 0.0 : p.audioOffset;
        const auto clipStart = tempo.secondsAt (p.start), clipEnd = tempo.secondsAt (p.end());
        // (0.52.0) With the auto fades and crossfades, as it plays.
        const auto& fades = project.autoFadesOf (among);
        const auto curve = [&fades] (double x)
        {
            x = juce::jlimit (0.0, 1.0, x);
            return fades.equalPower ? std::sin (x * juce::MathConstants<double>::halfPi) : x;
        };
        const auto recording = [this, rate] (const Part& other)
        {
            const auto heard = getHeardAudio (other.audioFile);
            return heard != nullptr ? (double) heard->sound.getNumSamples() / rate : 0.0;
        };
        for (const auto& f : fadedStretches (among, i, fades, tempo, recording))
        {
            const auto fromSeconds = tempo.secondsAt (f.from), toSeconds = tempo.secondsAt (f.to);
            const auto first = (int) std::llround ((fromSeconds - startSeconds) * rate);
            const auto last = juce::jmin (total, (int) std::llround ((toSeconds - startSeconds) * rate));
            for (int s = juce::jmax (0, first); s < last; ++s)
            {
                const auto when = startSeconds + s / rate;
                const auto into = when - clipStart, left = clipEnd - when;
                const auto source = (int) std::llround ((fromOffset + into) * rate);
                if (source < 0 || source >= heardSound.getNumSamples())
                    continue;
                auto gain = (double) p.gain();   // (0.47.0: its volume)
                if (p.fadeIn > 0.0 && into < p.fadeIn) gain *= juce::jmax (0.0, into) / p.fadeIn;
                if (p.fadeOut > 0.0 && left < p.fadeOut) gain *= juce::jmax (0.0, left) / p.fadeOut;
                if (f.fadeIn > 0.0 && when - fromSeconds < f.fadeIn) gain *= curve ((when - fromSeconds) / f.fadeIn);
                if (f.fadeOut > 0.0 && toSeconds - when < f.fadeOut) gain *= curve ((toSeconds - when) / f.fadeOut);
                for (int c = 0; c < sound.getNumChannels(); ++c)
                    sound.addSample (c, s, (float) (heardSound.getSample (juce::jmin (c, heardSound.getNumChannels() - 1), source) * gain));
            }
        }
    }

    juce::File Studio::writeLineForDrag (const LineIdea& idea, const std::vector<ChordSpan>& spans, int channel, const juce::String& name, juce::String& error)
    {
        if (spans.empty() || idea.notes.empty())
        {
            error = "Nothing to drag: the lane has no notes.";
            return {};
        }
        Track track;
        track.name = name;
        track.midiChannel = juce::jlimit (1, 16, channel);
        const auto part = partFromIdea (idea, spans.front().start, spans.back().end - spans.front().start, idea.style);
        const auto written = newDragOutFolder (composerDragFolder()).getChildFile (juce::File::createLegalFileName (name) + ".mid");
        const auto midi = makePartMidiFile (project, track, part);
        juce::FileOutputStream out (written);
        if (! out.openedOk() || ! midi.writeTo (out, 1))
        {
            error = "Couldn't write " + written.getFullPathName();
            return {};
        }
        SessionLog::note ("Composer drag out: " + written.getFileName());
        return written;
    }

    juce::File Studio::writePartForDrag (int partId, juce::String& error)
    {
        const auto where = findPart (partId);
        if (! where)
            return {};
        const auto* track = project.findTrack (where->first);
        if (track == nullptr)
            return {};
        const auto& part = track->parts[(size_t) where->second];
        const auto partName = part.name.isNotEmpty() ? part.name : (part.isAudio() ? juce::File (part.audioFile).getFileNameWithoutExtension() : juce::String ("Part"));
        const auto base = juce::File::createLegalFileName (track->name + " - " + partName);
        const auto folder = newDragOutFolder();
        if (! part.isAudio())
        {
            const auto written = folder.getChildFile (base + ".mid");
            const auto midi = makePartMidiFile (project, *track, part);
            juce::FileOutputStream out (written);
            if (! out.openedOk() || ! midi.writeTo (out, 1))
            {
                error = "Couldn't write " + written.getFullPathName();
                return {};
            }
            SessionLog::note ("Drag out: " + written.getFileName());
            return written;
        }
        const auto data = getHeardAudio (part.audioFile);
        if (data == nullptr || data->sampleRate <= 0.0)
        {
            error = "The event's recording couldn't be read.";
            return {};
        }
        const auto tempo = project.tempoMap();
        const auto rate = data->sampleRate;
        const auto startSeconds = tempo.secondsAt (part.start);
        const auto seconds = juce::jmin (dragOutMostSeconds, tempo.secondsAt (part.end()) - startSeconds);
        const auto samples = juce::jmax (1, (int) std::llround (seconds * rate));
        // The event alone on its track (no comping): what it plays.
        Track alone = *track;
        alone.soloLane = -1;
        alone.parts = { part };
        alone.parts.front().muted = false;
        juce::AudioBuffer<float> sound (juce::jlimit (1, 2, data->sound.getNumChannels()), samples);
        sound.clear();
        mixHeardClip (sound, alone, 0, startSeconds, rate);

        const auto bpm = tempo.bpmAt (part.start);
        const auto written = folder.getChildFile (base + " " + dragOutTempoText (bpm) + " BPM.wav");
        std::unordered_map<juce::String, juce::String> metadata;
        metadata[juce::WavAudioFormat::acidTempo] = juce::String (bpm, 3);
        metadata[juce::WavAudioFormat::acidBeats] = juce::String (juce::jmax (1, juce::roundToInt (tempo.beatAt (startSeconds + seconds) - part.start)));
        metadata[juce::WavAudioFormat::acidOneShot] = "0";
        const auto& signature = project.signatures().segmentAt (part.start).event;
        metadata[juce::WavAudioFormat::acidNumerator] = juce::String (signature.numerator);
        metadata[juce::WavAudioFormat::acidDenominator] = juce::String (signature.denominator);
        std::unique_ptr<juce::OutputStream> stream (written.createOutputStream());
        std::unique_ptr<juce::AudioFormatWriter> writer;
        if (stream != nullptr)
            writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate)
                                                                                   .withNumChannels (sound.getNumChannels())
                                                                                   .withBitsPerSample (32)
                                                                                   .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint)
                                                                                   .withMetadataValues (metadata));
        if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (sound, 0, sound.getNumSamples()))
        {
            error = "Couldn't write " + written.getFullPathName();
            return {};
        }
        writer.reset();
        SessionLog::note ("Drag out: " + written.getFileName());
        return written;
    }

    int Studio::bounceTakes (const std::vector<int>& partIds, juce::String& error)
    {
        Track* track = nullptr;
        for (const auto id : partIds)
            if (auto [t, i] = partById (project, id); t != nullptr)
            {
                if (track != nullptr && t != track)
                {
                    error = "Bounce works on the takes of one track.";
                    return 0;
                }
                track = t;
            }
        if (track == nullptr || track->frozen)
            return 0;
        const auto chosen = [&] (const Part& p) { return std::find (partIds.begin(), partIds.end(), p.id) != partIds.end(); };
        // What's heard among them alone (no lane soloed).
        Track among = *track;
        among.soloLane = -1;
        among.parts.erase (std::remove_if (among.parts.begin(), among.parts.end(), [&] (const Part& p) { return ! chosen (p); }), among.parts.end());
        if (among.parts.empty())
            return 0;
        auto from = among.parts.front().start, to = among.parts.front().end();
        auto lane = among.parts.front().lane;
        for (const auto& p : among.parts)
            from = juce::jmin (from, p.start), to = juce::jmax (to, p.end()), lane = juce::jmin (lane, p.lane);
        const auto tempo = project.tempoMap();
        Part bounced;
        bounced.start = from;
        bounced.length = to - from;
        bounced.name = among.parts.front().name;
        bounced.lane = lane;
        bounced.order = topOrder (*track) + 1;
        if (track->isAudio())
        {
            // Each heard stretch's samples, faded at its clip's own ends, into one sound.
            auto rate = 0.0;
            auto channels = 1;
            for (const auto& p : among.parts)
                if (const auto data = getAudio (p.audioFile))
                    rate = data->sampleRate, channels = juce::jmax (channels, data->sound.getNumChannels());
            if (rate <= 0.0)
            {
                error = "The takes' recordings couldn't be read.";
                return 0;
            }
            const auto startSeconds = tempo.secondsAt (from);
            const auto total = (int) std::ceil ((tempo.secondsAt (to) - startSeconds) * rate);
            juce::AudioBuffer<float> sound (juce::jmin (2, channels), juce::jmax (1, total));
            sound.clear();
            for (int i = 0; i < (int) among.parts.size(); ++i)
                mixHeardClip (sound, among, i, startSeconds, rate);
            auto folder = getAudioFolder();
            folder.createDirectory();
            juce::File bounceFile;
            int n = 1;
            do
                bounceFile = folder.getChildFile (juce::File::createLegalFileName (track->name) + " bounce " + juce::String (n++).paddedLeft ('0', 2) + ".wav");
            while (bounceFile.exists());
            std::unique_ptr<juce::OutputStream> stream (bounceFile.createOutputStream());
            std::unique_ptr<juce::AudioFormatWriter> writer;
            if (stream != nullptr)
                writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate)
                                                                                       .withNumChannels (sound.getNumChannels())
                                                                                       .withBitsPerSample (32)
                                                                                       .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
            if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (sound, 0, sound.getNumSamples()))
            {
                error = "The bounce couldn't be written to " + bounceFile.getFullPathName();
                return 0;
            }
            writer.reset();
            forgetAudio (bounceFile.getFileName());
            bounced.audioFile = bounceFile.getFileName();
            bounced.name = track->name + " bounce";
        }
        else
        {
            // The notes and controllers heard (unmuted takes, within each take), from the bounce's start.
            for (const auto& p : among.parts)
            {
                if (p.muted)
                    continue;
                const auto shift = p.start - from;
                for (auto note : p.notes)
                    if (note.start >= 0.0 && note.start < p.length)
                    {
                        note.length = juce::jmin (note.length, p.length - note.start);
                        note.start += shift;
                        bounced.notes.push_back (note);
                    }
                for (auto c : p.controllers)
                    if (c.start >= 0.0 && c.start < p.length)
                    {
                        c.start += shift;
                        bounced.controllers.push_back (c);
                    }
            }
            std::stable_sort (bounced.notes.begin(), bounced.notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
            ControllerKinds::sort (bounced.controllers);
        }
        track->parts.erase (std::remove_if (track->parts.begin(), track->parts.end(), chosen), track->parts.end());
        track->parts.push_back (std::move (bounced));
        changed();
        return track->parts.back().id;
    }

    void Studio::deleteOverlaps (const std::vector<int>& partIds)
    {
        Track* track = nullptr;
        for (const auto id : partIds)
            if (auto [t, i] = partById (project, id); t != nullptr && track == nullptr)
                track = t;
        if (track == nullptr || track->frozen)
            return;
        const auto chosen = [&] (const Part& p) { return std::find (partIds.begin(), partIds.end(), p.id) != partIds.end(); };
        Track among = *track;
        among.soloLane = -1;
        among.parts.erase (std::remove_if (among.parts.begin(), among.parts.end(), [&] (const Part& p) { return ! chosen (p); }), among.parts.end());
        auto lane = 255;
        for (const auto& p : among.parts)
            lane = juce::jmin (lane, p.lane);
        const auto tempo = project.tempoMap();
        std::vector<Part> kept;
        for (int i = 0; i < (int) among.parts.size(); ++i)
            for (const auto& [a, b] : heardStretches (among, i))
                if (auto piece = slice (among.parts[(size_t) i], a, b, tempo))
                {
                    piece->id = a <= among.parts[(size_t) i].start + edge ? among.parts[(size_t) i].id : 0;
                    piece->lane = lane;
                    kept.push_back (*piece);
                }
        track->parts.erase (std::remove_if (track->parts.begin(), track->parts.end(), chosen), track->parts.end());
        for (auto& p : kept)
            track->parts.push_back (std::move (p));
        changed();
    }

    int Studio::mergeMidiInLoop (const std::vector<int>& trackIds)
    {
        const auto from = project.loopStart, to = project.loopEnd;
        if (to <= from + edge)
            return 0;
        Part merged;
        merged.start = from;
        merged.length = to - from;
        int first = 0;
        for (const auto id : trackIds)
        {
            const auto* track = project.findTrack (id);
            if (track == nullptr || track->isAudio() || track->isChannel())
                continue;
            if (first == 0)
                first = id;
            for (int i = 0; i < (int) track->parts.size(); ++i)
            {
                const auto& p = track->parts[(size_t) i];
                if (heardStretches (*track, i).empty())
                    continue;
                for (auto note : p.notes)
                {
                    const auto at = p.start + note.start;
                    if (note.start < 0.0 || note.start >= p.length || at < from - edge || at >= to - edge)
                        continue;
                    note.length = juce::jmin (note.length, p.length - note.start, to - at);
                    note.start = at - from;
                    merged.notes.push_back (note);
                }
                for (auto c : p.controllers)
                {
                    const auto at = p.start + c.start;
                    if (c.start < 0.0 || c.start >= p.length || at < from - edge || at >= to - edge)
                        continue;
                    c.start = at - from;
                    merged.controllers.push_back (c);
                }
            }
        }
        if (first == 0 || (merged.notes.empty() && merged.controllers.empty()))
            return 0;
        std::stable_sort (merged.notes.begin(), merged.notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
        ControllerKinds::sort (merged.controllers);
        const auto id = duplicateTrack (first);
        if (id == 0)
            return 0;
        auto* copy = project.findTrack (id);
        copy->name = project.findTrack (first)->name + " merged";
        copy->showLanes = false;
        copy->soloLane = -1;
        merged.name = copy->name;
        copy->parts = { merged };
        changed();
        return id;
    }

    void Studio::startAudition (int trackId, int lane)
    {
        if (project.findTrack (trackId) == nullptr || exportJob != nullptr)
            return;
        auto heard = copyWithoutPluginStates (false);
        for (auto& t : heard.tracks)
            t.solo = t.id == trackId;
        heard.findTrack (trackId)->soloLane = lane;
        engine.update (heard);
        auditioning = true;
    }

    void Studio::stopAudition()
    {
        if (! auditioning)
            return;
        auditioning = false;
        if (exportJob == nullptr)
            engine.update (project);
    }

    void Studio::finishRecording()
    {
        const auto endBeat = engine.getPosition();
        const auto lapNow = engine.getCycleCount();   // (the lap being played when recording stopped)
        engine.stopRecording();
        wasRecording = false;
        const auto chordsToo = loggingChords.exchange (false);   // (0.44.0: the chord track was recording)
        if (audioWriter != nullptr)
        {
            finishAudioRecording();
            if (chordsToo)
                takeRecordedChords (recordStart, endBeat, {});
            return;
        }
        engine.takeRecorded (recorded);
        wasRecording = false;
        if (chordsToo)
        {
            // What was played (the last lap of a cycle): notes from their on and off.
            auto lastLap = 0;
            for (const auto& e : recorded)
                lastLap = juce::jmax (lastLap, e.pass);
            std::vector<HeardNote> played;
            std::map<int, size_t> open;
            for (const auto& e : recorded)
            {
                if (e.pass != lastLap)
                    continue;
                const auto kind = e.status & 0xf0;
                if (kind == 0x90 && e.data2 > 0)
                {
                    open[e.data1] = played.size();
                    played.push_back ({ e.beat, endBeat, e.data1, e.data2 / 127.0f });
                }
                else if (kind == 0x80 || (kind == 0x90 && e.data2 == 0))
                    if (const auto found = open.find (e.data1); found != open.end())
                    {
                        played[found->second].end = e.beat;
                        open.erase (found);
                    }
            }
            takeRecordedChords (recordStart, endBeat, played);
        }
        auto* track = project.findTrack (recordTrack);
        if (track == nullptr)
        {
            recorded.clear();
            return;
        }

        const auto bar = project.beatsPerBar();
        const auto cycled = project.loopOn && project.loopEnd > project.loopStart;
        const auto modes = project.recordModes;
        using Cycle = Project::RecordModes::MidiCycle;
        const auto named = [&] (Part part) { part.name = track->name; return part; };

        // The laps something was played in (a cycle; 0.29.0, the manual's MIDI Cycle Record Modes).
        std::vector<int> laps;
        for (const auto& e : recorded)
            if (std::find (laps.begin(), laps.end(), e.pass) == laps.end())
                laps.push_back (e.pass);
        std::sort (laps.begin(), laps.end());

        std::vector<Part> takes;
        if (cycled && (modes.midiCycle == Cycle::stacked || modes.midiCycle == Cycle::mixStacked) && laps.size() > 1)
        {
            // Stacked: each lap a part on a lane of its own; all but the last muted (Mix-Stacked: none).
            for (size_t l = 0; l < laps.size(); ++l)
            {
                Part part;
                part.start = project.loopStart;
                part.length = project.loopEnd - project.loopStart;
                buildRecordingNotes (part.notes, part.start, project.loopEnd, laps[l]);
                if (part.notes.empty())
                    continue;
                part = named (std::move (part));
                part.name << " take " << (int) (l + 1);
                part.muted = modes.midiCycle == Cycle::stacked && l + 1 < laps.size();
                takes.push_back (std::move (part));
            }
            // The controllers: with the last lap's notes (as each kind keeps its last pass).
            if (! takes.empty())
                buildRecordingControllers (takes.back().controllers, project.loopStart);
        }
        else
        {
            Part part;
            part.start = cycled ? project.loopStart : std::floor (recordStart / bar) * bar;
            if (cycled && modes.midiCycle == Cycle::keepLast && ! laps.empty())
            {
                // Keep Last: the last lap played through to its end (stopped during one: the one before).
                auto keep = laps.back();
                if (keep == lapNow && laps.size() > 1)
                    keep = laps[laps.size() - 2];
                buildRecordingNotes (part.notes, part.start, project.loopEnd, keep);
            }
            else if (cycled && modes.midiCycle == Cycle::overwrite && laps.size() > 1)
            {
                // Overwrite: from the first thing played in a lap, what earlier laps left from there on goes.
                for (const auto lap : laps)
                {
                    std::vector<Note> notes;
                    buildRecordingNotes (notes, part.start, project.loopEnd, lap);
                    if (notes.empty())
                        continue;
                    const auto from = notes.front().start;
                    part.notes.erase (std::remove_if (part.notes.begin(), part.notes.end(), [from] (const Note& n) { return n.start >= from - 1.0e-9; }),
                                      part.notes.end());
                    part.notes.insert (part.notes.end(), notes.begin(), notes.end());
                }
                std::sort (part.notes.begin(), part.notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
            }
            else
            {
                buildRecordingNotes (part.notes, part.start, endBeat);   // (Mix: every lap together)
            }
            buildRecordingControllers (part.controllers, part.start);
            if (part.notes.empty() && part.controllers.empty())
            {
                recorded.clear();
                return;   // nothing played: no part
            }
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
                for (const auto& c : part.controllers)
                    end = juce::jmax (end, part.start + c.start + 1.0e-6);
                part.length = juce::jmax (bar, std::ceil ((end - part.start) / bar - 1.0e-9) * bar);
            }
            takes.push_back (named (std::move (part)));
        }
        recorded.clear();
        if (takes.empty())
            return;

        // The MIDI Record Mode: New Parts (beside what's there), Merge (into the part already
        // there), Replace (what's there over the recording's stretch goes).
        const auto from = takes.front().start, to = takes.front().end();
        using Midi = Project::RecordModes::Midi;
        Part* under = nullptr;
        for (auto& p : track->parts)
            if (p.start < to - 1.0e-9 && p.end() > from + 1.0e-9 && ! p.muted && ! p.isAudio())
            {
                under = &p;
                break;
            }
        if (modes.midi == Midi::merge && under != nullptr && takes.size() == 1)
        {
            // Merge: the notes into the part there (it grows to hold them).
            const auto& take = takes.front();
            const auto newStart = juce::jmin (under->start, take.start), newEnd = juce::jmax (under->end(), take.end());
            const auto shiftOld = under->start - newStart, shiftNew = take.start - newStart;
            for (auto& n : under->notes) n.start += shiftOld;
            for (auto& c : under->controllers) c.start += shiftOld;
            for (auto n : take.notes) { n.start += shiftNew; under->notes.push_back (n); }
            for (auto c : take.controllers) { c.start += shiftNew; under->controllers.push_back (c); }
            std::stable_sort (under->notes.begin(), under->notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
            ControllerKinds::sort (under->controllers);
            under->start = newStart;
            under->length = newEnd - newStart;
            changed();
            return;
        }
        if (modes.midi == Midi::replace)
        {
            // Replace: the notes and controllers starting in the recording's stretch go (parts left
            // with nothing go too); the new part on the lane of the one it replaced in.
            const auto lane = under != nullptr ? under->lane : 0;
            for (auto& p : track->parts)
            {
                if (p.isAudio() || p.start >= to - 1.0e-9 || p.end() <= from + 1.0e-9)
                    continue;
                const auto in = [&] (double rel) { const auto at = p.start + rel; return at >= from - 1.0e-9 && at < to - 1.0e-9; };
                p.notes.erase (std::remove_if (p.notes.begin(), p.notes.end(), [&] (const Note& n) { return in (n.start); }), p.notes.end());
                p.controllers.erase (std::remove_if (p.controllers.begin(), p.controllers.end(), [&] (const Controller& c) { return in (c.start); }), p.controllers.end());
            }
            track->parts.erase (std::remove_if (track->parts.begin(), track->parts.end(), [&] (const Part& p)
                                {
                                    return ! p.isAudio() && p.notes.empty() && p.controllers.empty() && p.start >= from - 1.0e-9 && p.end() <= to + 1.0e-9;
                                }), track->parts.end());
            placeTakes (*track, std::move (takes), from, to, false, lane);
            changed();
            return;
        }
        placeTakes (*track, std::move (takes), from, to, false);
        changed();
    }

    std::vector<ChordEvent> Studio::chordsFromPlayed (const std::vector<HeardNote>& played) const
    {
        // Each group of notes starting together (within a 32nd): the chord sounding just after it,
        // if three or more different notes make one.
        std::vector<ChordEvent> found;
        auto notes = played;
        std::sort (notes.begin(), notes.end(), [] (const HeardNote& a, const HeardNote& b) { return a.start < b.start; });
        constexpr double together = 0.125;
        for (size_t i = 0; i < notes.size();)
        {
            const auto groupStart = notes[i].start;
            auto j = i;
            while (j < notes.size() && notes[j].start - groupStart <= together)
                ++j;
            const auto look = notes[j - 1].start + 0.01;
            std::vector<int> pitches;
            std::set<int> classes;
            for (const auto& n : notes)
                if (n.start <= look && n.end > look)
                {
                    pitches.push_back (n.pitch);
                    classes.insert (n.pitch % 12);
                }
            if (classes.size() >= 3)
                if (const auto chord = chordOfNotes (pitches))
                    if (found.empty() || ! (found.back().chord == *chord))
                        found.push_back ({ groupStart, *chord });
            i = j;
        }
        return found;
    }

    void Studio::takeRecordedChords (double from, double to, const std::vector<HeardNote>& played)
    {
        std::vector<ChordEvent> events;
        {
            const std::scoped_lock l (chordLogLock);
            events = chordLog;
            chordLog.clear();
        }
        if (events.empty())
            events = chordsFromPlayed (played);
        if (events.empty())
            return;
        // On the grid when snapping; two on one beat: the later; the same chord twice: once.
        const auto grid = project.snapOn ? project.quantizeBeats : 0.0;
        std::vector<ChordEvent> placed;
        for (auto e : events)
        {
            if (grid > 0.0)
                e.beat = std::round (e.beat / grid) * grid;
            e.beat = juce::jmax (0.0, e.beat);
            if (! placed.empty() && std::abs (placed.back().beat - e.beat) < 1.0e-6)
                placed.back() = e;
            else if (placed.empty() || ! (placed.back().chord == e.chord))
                placed.push_back (e);
        }
        from = juce::jmin (from, placed.front().beat);
        auto kept = project.chordTrack.events;
        kept.erase (std::remove_if (kept.begin(), kept.end(), [from, to] (const ChordEvent& e) { return e.beat >= from - 1.0e-9 && e.beat < to - 1.0e-9; }),
                    kept.end());
        kept.insert (kept.end(), placed.begin(), placed.end());
        std::sort (kept.begin(), kept.end(), [] (const ChordEvent& a, const ChordEvent& b) { return a.beat < b.beat; });
        project.chordTrack.events = std::move (kept);
        project.chordTrack.shown = true;
        chordTrackChanged();
    }

    void Studio::setChordTrackRecord (bool on)
    {
        if (project.chordTrack.record == on)
            return;
        project.chordTrack.record = on;
        if (on)
            project.chordTrack.shown = true;
        announce();
    }

    void Studio::applyPadModifier (ChordPads::Modifier m)
    {
        const auto pad = lastPlayedPad.load();
        if (! juce::isPositiveAndBelow (pad, (int) project.chordPads.pads.size()) || ! project.chordPads.pads[(size_t) pad].chord)
            return;
        using M = ChordPads::Modifier;
        switch (m)
        {
            case M::nextVoicing:     changePadVoicings ({ pad }, 1); break;
            case M::previousVoicing: changePadVoicings ({ pad }, -1); break;
            case M::moreTensions:    changePadTensions ({ pad }, 1); break;
            case M::fewerTensions:   changePadTensions ({ pad }, -1); break;
            case M::transposeUp:     transposePads ({ pad }, 1); break;
            case M::transposeDown:   transposePads ({ pad }, -1); break;
        }
        auditionPad (pad);   // (heard as it is now: the manual's "plays back ... and updates")
    }

    void Studio::startPadLearn (PadLearn what, int index)
    {
        padLearn = what;
        padLearnIndex = index;
        rangeFirst = -1;
        padPlayer.learning = what != PadLearn::none;
    }

    void Studio::cancelPadLearn()
    {
        startPadLearn (PadLearn::none);
    }

    void Studio::learnedKey (int note)
    {
        if (padLearn == PadLearn::none)
            return;
        auto pads = project.chordPads;
        const auto i = (size_t) juce::jlimit (0, 5, padLearnIndex);
        switch (padLearn)
        {
            case PadLearn::range:
                if (rangeFirst < 0)
                {
                    rangeFirst = note;
                    padPlayer.learning = true;   // (the end next)
                    return;
                }
                pads.triggerStart = juce::jmin (rangeFirst, note);
                pads.triggerEnd = juce::jmax (rangeFirst, note);
                break;
            case PadLearn::modifier:   pads.modifierKeys[juce::jmin (i, (size_t) 5)] = note; break;
            case PadLearn::section:    pads.sectionKeys[juce::jmin (i, (size_t) 4)] = note; break;
            case PadLearn::subsection: pads.subsectionKeys[juce::jmin (i, (size_t) 4)] = note; break;
            case PadLearn::none:       break;
        }
        padLearn = PadLearn::none;
        padPlayer.learning = false;
        setChordPads (pads);
        if (onLearned)
            onLearned();
    }

    juce::String Studio::describeSpike (const Engine::SpikeReport& r) const
    {
        // (0.57.3) One line in plain words: what happened, then where the time went.
        const auto ms = [] (double v) { return juce::String (v, v < 10.0 ? 2 : 1) + " ms"; };
        juce::String text;
        if (r.missedLock)
            text << "SPIKE: one block of silence (" << ms (r.budgetMs) << "): the engine was taking a change in (the lock)";
        else
            text << "SPIKE: a block took " << ms (r.blockMs) << " of its " << ms (r.budgetMs) << " ("
                 << juce::roundToInt (r.load * 100.0f) << "%)";
        if (r.lateCall)
            text << "; the driver called " << ms (r.sinceLastMs) << " after the last block (" << ms (r.budgetMs)
                 << " expected: the computer was late, not the studio)";
        if (! r.missedLock)
        {
            text << "; tracks " << ms (r.tracksMs) << " in all (" << r.numTracks << "), master " << ms (r.masterMs)
                 << ", " << (r.helpers > 0 ? juce::String (r.helpers) + " helper threads" : juce::String ("one core"));
            if (r.helpers > 0 && r.blockMs > r.tracksMs / (r.helpers + 1) + r.masterMs + 0.5 * r.budgetMs)
                text << " (more than the tracks' work spread over the cores: a thread waited)";
            for (int i = 0; i < r.numSlowest; ++i)
            {
                const auto& slow = r.slowest[(size_t) i];
                if (slow.ms < 0.05f)
                    break;
                juce::String what;
                if (const auto* t = project.findTrack (slow.trackId))
                    what << "\"" << t->name << "\"";
                else
                    what << "track " << slow.trackId;
                // (0.57.4) Each plug-in's own share, with its usual time; else the plug-ins by name.
                juce::StringArray plugins;
                for (int k = 0; k < slow.numPlugins; ++k)
                {
                    const auto& plugin = slow.plugins[(size_t) k];
                    auto* instance = plugin.slot < 0 ? engine.getInstrument (slow.trackId) : engine.getEffect (slow.trackId, plugin.slot);
                    const auto name = instance != nullptr ? instance->getName()
                                                          : (plugin.slot < 0 ? juce::String ("instrument") : "insert " + juce::String (plugin.slot + 1));
                    plugins.add (name + " " + ms (plugin.ms) + " (usually " + ms (plugin.usualMs) + ")");
                }
                what << " " << ms (slow.ms) << " (usually " << ms (slow.usualMs) << ")";
                if (! plugins.isEmpty())
                    what << " [" << plugins.joinIntoString (", ") << "]";
                text << (i == 0 ? "; slowest: " : ", ") << what;
            }
        }
        if (r.missedSince > 0)
            text << "; and " << r.missedSince << " more since the last report";
        return text;
    }

    void Studio::noteSpikes()
    {
        // At most a line a second (a run of spikes is counted in the next one).
        const auto now = juce::Time::getMillisecondCounter();
        if (now - lastSpikeNote < 1000 && lastSpikeNote != 0)
            return;
        if (const auto report = engine.takeSpikeReport())
        {
            lastSpikeNote = now;
            ++spikesNoted;
            SessionLog::note (describeSpike (*report));
        }
    }

    void Studio::timerCallback()
    {
        noteSpikes();   // (0.57.3: first, even while rendering)
        if (tapUnrecorded && ! isTapping (juce::Time::getMillisecondCounterHiRes() / 1000.0))
            endTapping();   // (0.45.0: the taps stopped; their undo step)
        if (mediaBay != nullptr)
            mediaBay->saveIfDue();   // (0.57.1: ratings and tags written once the edits stop)
        if (rendering || exportJob != nullptr)
            return;
        backupIfDue();   // (0.72.0: AUTO-SAVE)
        if (performer != nullptr)
        {
            // (0.39.0) The performance's tempo and bar, from the song at the playhead.
            performer->bpm = project.bpmAt (engine.getPosition());
            performer->beatsPerBar = project.beatsPerBar();
        }   // (a render runs: the screen's thread carries on meanwhile, nothing to do here)
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
            // (0.57.1) Every view redraws on a change: only when notes came in, else five times a
            // second (an audio recording still grows): it was 30 times a second, whatever came in.
            const auto before = recorded.size();
            engine.takeRecorded (recorded);
            if (recorded.size() != before || ++quietRecordingTicks >= 6)
            {
                quietRecordingTicks = 0;
                announce();
            }
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
        audioFolderOverride = juce::File();
        lastBackupHash = 0;
        lastBackupTime = juce::Time::getMillisecondCounter();
        endComposerAudition (false);   // (0.58.0)
        frozenParts.clear();
        builtSections.clear();
        composerRiff.reset();
        editorSoloTrack = 0;   // (0.78.0: another song's track ids)
        engine.setEditorSolo (0);
        punchOutAll();   // (writing automation ends with the song)
        if (onInstrumentGoing)
            onInstrumentGoing (0);
        removeAllEffects();
        forgetAllAudio();
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
        clearHistory();
        recordUndoState();
        modified = false;
        announce();
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

    juce::String Studio::trackTypeName (Track::Kind kind)
    {
        switch (kind)
        {
            case Track::Kind::instrument: return "INSTRUMENT";
            case Track::Kind::audio:      return "AUDIO";
            case Track::Kind::fx:         return "FX CHANNEL";
            case Track::Kind::group:      return "GROUP CHANNEL";
            case Track::Kind::folder:     return "FOLDER";
        }
        return {};
    }

    std::array<Studio::TrackTypeLook, Studio::numTrackTypes> Studio::readTrackTypeLooks() const
    {
        // "colour|list|amount" a type, ";" between them, in Track::Kind's order; "auto" for no colour.
        if (settings == nullptr)
            return trackTypeLooksHere;
        std::array<TrackTypeLook, numTrackTypes> looks;
        const auto parts = juce::StringArray::fromTokens (settings->getValue ("trackTypeLooks"), ";", "");
        for (int i = 0; i < numTrackTypes && i < parts.size(); ++i)
        {
            const auto fields = juce::StringArray::fromTokens (parts[i], "|", "");
            if (fields.size() < 3)
                continue;
            if (fields[0] != "auto")
                if (const auto c = juce::Colour::fromString (fields[0]); FamilyColours::isFamily (c))
                    looks[(size_t) i].colour = c;
            looks[(size_t) i].listColour = fields[1] == "1";
            looks[(size_t) i].listAmount = juce::jlimit (1, 100, fields[2].getIntValue());
        }
        return looks;
    }

    Studio::TrackTypeLook Studio::getTrackTypeLook (Track::Kind kind) const
    {
        const auto i = juce::jlimit (0, numTrackTypes - 1, (int) kind);
        return readTrackTypeLooks()[(size_t) i];
    }

    void Studio::setTrackTypeLook (Track::Kind kind, TrackTypeLook look)
    {
        if (look.colour && ! FamilyColours::isFamily (*look.colour))
            look.colour = FamilyColours::nearest (*look.colour);   // (only the family's: the owner's rule)
        look.listAmount = juce::jlimit (1, 100, look.listAmount);
        auto looks = readTrackTypeLooks();
        looks[(size_t) juce::jlimit (0, numTrackTypes - 1, (int) kind)] = look;
        trackTypeLooksHere = looks;
        if (settings != nullptr)
        {
            juce::StringArray parts;
            for (const auto& l : looks)
                parts.add ((l.colour ? l.colour->toString() : juce::String ("auto")) + "|" + (l.listColour ? "1" : "0") + "|" + juce::String (l.listAmount));
            settings->setValue ("trackTypeLooks", parts.joinIntoString (";"));
        }
    }

    void Studio::giveTypeLook (Track& track, int index) const
    {
        const auto look = getTrackTypeLook (track.kind);
        track.colour = look.colour.value_or (trackColour (index));
        track.listColourOn = look.listColour;
        track.listColourAmount = look.listAmount;
    }

    void Studio::applyTrackTypeLooks()
    {
        for (int i = 0; i < (int) project.tracks.size(); ++i)
            giveTypeLook (project.tracks[(size_t) i], i);
        changed();
    }

    int Studio::getAutoSaveMinutes() const
    {
        if (settings != nullptr)
            return juce::jlimit (0, 120, settings->getIntValue ("autoSaveMinutes", defaultAutoSaveMinutes));
        return autoSaveMinutesHere;
    }

    void Studio::setAutoSaveMinutes (int minutes)
    {
        autoSaveMinutesHere = juce::jlimit (0, 120, minutes);
        if (settings != nullptr)
            settings->setValue ("autoSaveMinutes", autoSaveMinutesHere);
        lastBackupTime = juce::Time::getMillisecondCounter();   // (counted from now)
    }

    int Studio::getAutoSaveKeep() const
    {
        if (settings != nullptr)
            return juce::jlimit (1, 200, settings->getIntValue ("autoSaveKeep", defaultAutoSaveKeep));
        return autoSaveKeepHere;
    }

    void Studio::setAutoSaveKeep (int copies)
    {
        autoSaveKeepHere = juce::jlimit (1, 200, copies);
        if (settings != nullptr)
            settings->setValue ("autoSaveKeep", autoSaveKeepHere);
    }

    juce::File Studio::backupFolder() const
    {
        if (file != juce::File() && audioFolderOverride == juce::File())
            return file.getParentDirectory().getChildFile ("Backups");
        if (file != juce::File() && file.getParentDirectory().getFileName() == "Backups")
            return file.getParentDirectory();   // (a backup opened: its own folder)
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Paragon PR7-S").getChildFile ("Backups");
    }

    void Studio::backupIfDue()
    {
        const auto minutes = getAutoSaveMinutes();
        if (minutes <= 0 || ! modified || engine.isRecording())
            return;
        const auto now = juce::Time::getMillisecondCounter();
        if (now - lastBackupTime < (juce::uint32) minutes * 60000u)
            return;
        lastBackupTime = now;
        writeBackup();
    }

    bool Studio::writeBackup()
    {
        // (0.85.2) Whether anything changed is told without writing the song out: the change
        // count, the levels (a fader drag's steps don't count as changes) and each plug-in's
        // settings, hashed. The XML text itself is made on the saver's thread from a copy
        // (it held the screen for the whole song's toXml every few minutes).
        collectPluginStates();
        const auto mix = [] (juce::uint64 h, juce::uint64 v) { return (h ^ v) * 1099511628211ull; };
        const auto stateHash = [] (const juce::MemoryBlock& m)
        {
            return (juce::uint64) std::hash<std::string_view>{} (std::string_view (static_cast<const char*> (m.getData()), m.getSize()));
        };
        juce::uint64 hash = mix (14695981039346656037ull, changeVersion);
        hash = mix (hash, (juce::uint64) std::bit_cast<juce::uint32> (project.masterVolumeDb));
        for (const auto& track : project.tracks)
        {
            hash = mix (hash, (juce::uint64) std::bit_cast<juce::uint32> (track.volumeDb));
            hash = mix (hash, (juce::uint64) std::bit_cast<juce::uint32> (track.pan));
            hash = mix (hash, stateHash (track.instrument.state));
            for (const auto& insert : track.inserts)
                hash = mix (hash, stateHash (insert.state));
        }
        for (const auto& insert : project.masterInserts)
            hash = mix (hash, stateHash (insert.state));
        if (hash == 0)
            hash = 1;
        if ((juce::int64) hash == lastBackupHash)
            return false;   // (nothing changed since the last copy)
        lastBackupHash = (juce::int64) hash;
        auto copy = std::make_shared<const Project> (project);
        const auto audioFolder = getAudioFolder().getFullPathName();
        auto name = file != juce::File() ? file.getFileNameWithoutExtension() : juce::String ("Untitled");
        if (const auto at = name.indexOf (" backup "); at > 0)
            name = name.substring (0, at);   // (a backup opened: its copies keep the song's name)
        const auto folder = backupFolder();
        const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
        name = juce::File::createLegalFileName (name);
        const auto destination = folder.getChildFile (name + " backup " + stamp + Project::fileExtension);
        lastBackup = destination;
        const auto keep = getAutoSaveKeep();
        SessionLog::note ("Auto-save: " + destination.getFullPathName());
        ++savesRunning;
        saver.addJob ([copy, audioFolder, destination, folder, name, keep, life = saveAlive, this]
        {
            auto xml = copy->toXml();
            xml->setAttribute ("backupAudioFolder", audioFolder);
            folder.createDirectory();
            const auto written = destination.replaceWithText (xml->toString());
            // The newest `keep` of this song's copies stay.
            auto copies = folder.findChildFiles (juce::File::findFiles, false, name + " backup *" + Project::fileExtension);
            std::sort (copies.begin(), copies.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName() > b.getFileName(); });
            for (int i = keep; i < copies.size(); ++i)
                copies.getReference (i).deleteFile();
            juce::MessageManager::callAsync ([life, this, written, destination]
            {
                if (! life->load())
                    return;
                --savesRunning;
                if (! written)
                    SessionLog::note ("Auto-save failed: " + destination.getFullPathName());
            });
        });
        return true;
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
            if (! midi.signatureChanges.empty())
            {
                project.signatureChanges = midi.signatureChanges;   // (0.45.0)
                project.signatureTrackShown = true;
                changed (false);
            }
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
            for (auto c : imported.controllers)
            {
                c.start += atBeat - part.start;
                if (c.start >= 0.0 && c.start < part.length)
                    part.controllers.push_back (c);
                else if (c.start < 0.0)
                    c.start = 0.0, part.controllers.push_back (c);   // (set before the first bar with notes: at the part's start)
            }
            ControllerKinds::sort (part.controllers);
            addPart (id, part);
            ++added;
        }
        // (0.49.0) The file's markers onto the marker track (the manual: a file from other software's
        // markers all go onto one track), from where it went in.
        if (! midi.markers.empty() && ! project.markerTrack.locked)
        {
            auto& track = project.markerTrack;
            for (const auto& fileMarker : midi.markers)
            {
                Marker m;
                m.id = track.nextId (false);
                m.start = m.end = juce::jmax (0.0, atBeat + fileMarker.beat);
                m.name = fileMarker.name;
                track.markers.push_back (m);
                track.sort();
            }
            track.shown = true;
            changed (! newSong);
        }
        if (newSong)
        {
            // The import is where the new song starts: nothing to undo before it.
            clearHistory();
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
        audioFolderOverride = juce::File();   // (its recordings were copied beside it)
        modified = false;
        library->addRecent (destination);
        announce();
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
        juce::String problems;
        startNewSong (problems);   // (0.78.2: the default template, if one is chosen)
        if (! save (destination))
        {
            error = "Couldn't save " + destination.getFullPathName();
            return false;
        }
        return true;
    }

    bool Studio::startNewSong (juce::String& problems)
    {
        const auto chosen = library->getDefaultTemplate();
        if (chosen.existsAsFile())
        {
            if (newFromTemplate (chosen, problems))
                return true;
            problems = "The default template couldn't be opened (" + chosen.getFullPathName() + "): an empty song instead.\n" + problems;
            newProject();
            return false;
        }
        newProject();
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
        forgetAllAudio();
        engine.update (project);
        modified = false;
        announce();
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
        const auto keptAudio = audioFolderOverride;
        const auto written = save (destination);
        library->removeRecent (destination);
        file = keptFile;
        audioFolderOverride = keptAudio;
        modified = keptModified;
        announce();
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
        audioFolderOverride = juce::File();   // (its recordings were copied beside it)
        modified = false;
        library->addRecent (destination);
        ++savesRunning;
        announce();
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
                announce();
                if (done)
                    done (written);
            });
        });
    }

    bool Studio::prepareSave (const juce::File& destination)
    {
        SessionLog::note ("Saving " + destination.getFullPathName());
        collectPluginStates();
        // The recordings go with the song: into "<song> Audio" beside it
        // (copied from where they were: the unsaved folder, or the song's old place).
        return copyAudioFiles (getAudioFolder(), destination.getSiblingFile (destination.getFileNameWithoutExtension() + " Audio"));
    }

    void Studio::collectPluginStates()
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
        stopMediaPreview();   // (the MediaBay's preview plays over the output: never into a render)
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
        engine.setEditorSolo (0);   // (0.78.0: the MIDI editor's SOLO never reaches a render)
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
                                                                                                        .withNumChannels (options.mono ? 1 : 2)
                                                                                                        .withBitsPerSample (bits)
                                                                                                        .withSampleFormat (bits == 32 ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint
                                                                                                                                      : juce::AudioFormatWriterOptions::SampleFormat::integral));
        }
        job->mono = options.mono;
        if (job->writer == nullptr)
        {
            error = "Couldn't write " + destination.getFullPathName();
            return false;
        }

        engine.setExportBus (options.channel);   // (0.51.0: a channel export)
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

    bool Studio::writeBlock (ExportJob& job, juce::AudioBuffer<float>& block, int start, int length)
    {
        // (0.78.0) A mono file: the two sides averaged into the first, that one written.
        if (! job.mono || block.getNumChannels() < 2)
            return job.writer->writeFromAudioSampleBuffer (block, start, length);
        auto* left = block.getWritePointer (0, start);
        juce::FloatVectorOperations::add (left, block.getReadPointer (1, start), length);
        juce::FloatVectorOperations::multiply (left, 0.5f, length);
        const float* one[] = { left, nullptr };
        return job.writer->writeFromFloatArrays (one, 1, length);
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
            SessionLog::jobBeat();   // (0.52.1: a render that stops moving is written down)
            if (! writeBlock (job, block, 0, length))
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
                SessionLog::jobBeat();
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
            if (lastSound >= 0 && ! writeBlock (job, tail, 0, (int) lastSound + 1))
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
        engine.setExportBus (0);
        job->writer.reset();   // the file finished
        if (! inBatch)   // (several renders in a row: back to realtime once, after the last)
            engine.setNonRealtime (false);
        if (! job->finished)
            job->file.deleteFile();
        // Everything as it was.
        if (! juce::approximatelyEqual (engine.getSampleRate(), job->previousRate))
            engine.prepare (job->previousRate, job->previousBlockSize);
        engine.setEditorSolo (editorSoloTrack);
        engine.update (project);
        engine.settleMix();   // (0.9.8: the song's own levels from the first sample, no glide from the export's copy)
        engine.setPosition (job->previousPosition);
        if (withDevices && ! inBatch)
            devices.addAudioCallback (&engine);
        announce();
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

    std::vector<int> Studio::getExportableChannels() const
    {
        std::vector<int> ids;
        for (const auto& t : project.tracks)
            if (! (t.isFolder() && ! t.folderGroup))
                ids.push_back (t.id);
        return ids;
    }

    bool Studio::exportBatch (const juce::File& folder, const juce::String& baseName, const ExportBatch& batch, juce::String& error,
                              std::vector<juce::File>* written)
    {
        struct Range { double start, end; juce::String label; };
        std::vector<Range> ranges;
        const auto& markers = project.markerTrack.markers;
        for (const auto i : batch.cycleMarkers)
            if (juce::isPositiveAndBelow (i, (int) markers.size()) && markers[(size_t) i].cycle)
            {
                const auto& m = markers[(size_t) i];
                ranges.push_back ({ m.start, m.end, m.name.isNotEmpty() ? m.name : "Cycle " + juce::String (m.id) });
            }
        if (ranges.empty())
            ranges.push_back ({ batch.options.start, batch.options.end, {} });
        auto channels = batch.channels;
        if (channels.empty())
            channels.push_back (0);
        if (! folder.isDirectory() && ! folder.createDirectory())
        {
            error = "Couldn't make the folder " + folder.getFullPathName();
            return false;
        }
        const auto total = (int) (ranges.size() * channels.size());
        const auto many = total > 1;
        if (many)
            beginBatch();
        auto ok = true;
        auto n = 0;
        for (const auto& range : ranges)
        {
            for (const auto channel : channels)
            {
                const auto* track = project.findTrack (channel);
                if (channel != 0 && track == nullptr)
                    continue;
                juce::StringArray parts { baseName };
                if (track != nullptr)
                    parts.add (track->name);
                if (range.label.isNotEmpty())
                    parts.add (range.label);
                const auto name = juce::File::createLegalFileName (parts.joinIntoString (" - "));
                auto file = folder.getChildFile (name + ".wav");
                for (int number = 2; file.exists(); ++number)
                    file = folder.getChildFile (name + " " + juce::String (number) + ".wav");   // (the manual's Create Unique File Name)
                auto options = batch.options;
                options.start = range.start;
                options.end = range.end;
                options.channel = channel;
                if (channel != 0)
                {
                    options.click = false;
                    if (! batch.includeMaster)
                    {
                        const auto before = options.prepareSong;
                        options.prepareSong = [before] (Project& song)
                        {
                            if (before)
                                before (song);
                            for (auto& insert : song.masterInserts)
                                insert.bypass = true;
                            song.masterVolumeDb = 0.0f;
                            song.masterRead = false;
                            song.masterEq.bypass = true;
                        };
                    }
                }
                setProgressStage ((double) n / total, 1.0 / total, "Exporting " + name);
                ++n;
                if (! exportMix (file, options, error))
                {
                    ok = false;
                    break;
                }
                if (written != nullptr)
                    written->push_back (file);
                if (wasRenderCancelled())
                {
                    ok = false;
                    break;
                }
            }
            if (! ok)
                break;
        }
        if (many)
            endBatch();
        return ok;
    }

    bool Studio::exportMix (const juce::File& destination, const ExportOptions& options, juce::String& error)
    {
        {
            const SessionLog::Step step ("Setting up the render of " + destination.getFileName() + " (plug-ins switched to offline)");
            if (! beginExport (destination, options, error))
                return false;
        }
        const SessionLog::Job job ("the render of " + destination.getFileName());
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
        const SessionLog::Step step ("Ending the render (plug-ins back to realtime, the audio device back on)");
        endExport();
        if (! rendered)
            error = renderCancelled ? juce::String ("Cancelled.") : "Couldn't write " + destination.getFullPathName();
        return rendered;
    }

    std::vector<int> Studio::renderInPlace (const std::vector<RenderSource>& sources, const RenderOptions& options, juce::String& error)
    {
        using P = RenderOptions::Processing;
        beginBatch();   // (the plugins offline once for all the stretches)
        auto batchEnded = false;
        const juce::ScopeGuard batch { [this, &batchEnded] { if (! batchEnded) endBatch(); } };
        std::vector<int> made, toDisable, toHide;
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
            // (0.78.0, the owner) A mono audio track renders a mono file onto a mono track; everything else stereo.
            const auto mono = track->isAudio() && ! track->stereoInput;
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
                render.mono = mono;   // (0.78.0: a mono track renders mono)
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
                forgetAudio (file.getFileName());
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
            const auto newId = addAudioTrack (! mono);
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
            // Disabled (0.31.0): muted first. A stretch of a track: whatever was chosen but keep, its parts
            // rendered there muted (disabling or removing the track would silence the rest of it).
            // (0.77.0, the owner: "render in place always means mute / disable / hide, however it was asked
            // for") Disable on a stretch too: its parts rendered there muted, and the whole track muted,
            // disabled and hidden once everything is rendered (a track disabled now would render the next
            // stretch of it silent).
            const auto muteSource = options.source == RenderOptions::Source::mute
                                 || options.source == RenderOptions::Source::disable
                                 || (! source.wholeTrack && options.source != RenderOptions::Source::keep);
            if (muteSource)
            {
                if (source.wholeTrack || options.source == RenderOptions::Source::disable)
                    project.findTrack (sourceId)->mute = true;
                if (! source.wholeTrack)
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
            if (options.source == RenderOptions::Source::disable && std::find (toDisable.begin(), toDisable.end(), sourceId) == toDisable.end())
                toDisable.push_back (sourceId);
            if ((source.wholeTrack || options.source == RenderOptions::Source::disable) && options.hideSource
                && options.source != RenderOptions::Source::remove && std::find (toHide.begin(), toHide.end(), sourceId) == toHide.end())
                toHide.push_back (sourceId);
            if (options.source == RenderOptions::Source::remove && source.wholeTrack)
                removeTrack (sourceId);
        }
        // After every stretch is rendered (the plugins back online first: the batch ends before they're unloaded).
        if (! toDisable.empty() || ! toHide.empty())
        {
            endBatch();
            batchEnded = true;
            if (! toDisable.empty())
                setTracksDisabled (toDisable, true);
            if (! toHide.empty())
                setTracksHidden (toHide, true);
        }
        error = problems.joinIntoString ("\n");
        if (! made.empty())
        {
            selectTrack (made.back());
            selectedTracks = made;
        }
        return made;
    }

    int Studio::removeMissing()
    {
        auto removed = 0;
        {
            // (Each change made here alone: not passed on to linked tracks, one engine update after.)
            const juce::ScopedValueSetter<bool> alone (mirroring, true);
            // Effects from the last slot back (emptying one can drop the empty slots after it).
            auto items = missingItems;
            std::stable_sort (items.begin(), items.end(), [] (const MissingItem& a, const MissingItem& b) { return a.slot > b.slot; });
            for (const auto& item : items)
            {
                if (item.kind == MissingItem::Kind::effect)
                {
                    const auto* inserts = insertsOf (item.trackId);
                    if (inserts == nullptr || ! juce::isPositiveAndBelow (item.slot, (int) inserts->size()) || (*inserts)[(size_t) item.slot].isEmpty())
                        continue;
                    removeInsert (item.trackId, item.slot);
                    ++removed;
                }
                else if (item.kind == MissingItem::Kind::recording)
                {
                    auto* track = project.findTrack (item.trackId);
                    if (track == nullptr || track->frozen)
                        continue;
                    const auto before = track->parts.size();
                    track->parts.erase (std::remove_if (track->parts.begin(), track->parts.end(),
                                                        [&] (const Part& p) { return p.isAudio() && p.audioFile == item.file; }),
                                        track->parts.end());
                    removed += (int) (before - track->parts.size());
                }
                else
                {
                    auto* track = project.findTrack (item.trackId);
                    if (track == nullptr || track->isAudio() || track->isChannel())
                        continue;
                    if (changeInstrument (item.trackId, Instrument {}))
                        ++removed;
                }
            }
            missingItems.clear();
        }
        changed();
        return removed;
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
        // (0.72.0) A backup: its song's recordings where they were when it was written (unless it
        // has a "<name> Audio" folder of its own: then it's been saved as a song since).
        audioFolderOverride = juce::File();
        if (const auto path = xml->getStringAttribute ("backupAudioFolder"); juce::File::isAbsolutePath (path))
            if (! source.getSiblingFile (source.getFileNameWithoutExtension() + " Audio").isDirectory() && juce::File (path).isDirectory())
                audioFolderOverride = juce::File (path);
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
        clearHistory();
        recordUndoState();
        modified = true;   // (a new song: not saved yet)
        if (problems.isNotEmpty())
            notes.add (problems);
        report = notes.joinIntoString ("\n");
        announce();
        return true;
    }

    void Studio::openProject (Project&& loadedProject, const juce::File& source, juce::String& problems)
    {
        lastBackupHash = 0;
        lastBackupTime = juce::Time::getMillisecondCounter();
        endComposerAudition (false);   // (0.58.0)
        builtSections.clear();
        composerRiff.reset();
        editorSoloTrack = 0;   // (0.78.0: another song's track ids)
        engine.setEditorSolo (0);
        std::optional<Project> loaded { std::move (loadedProject) };
        punchOutAll();

        if (onInstrumentGoing)
            onInstrumentGoing (0);
        removeAllEffects();
        engine.stop();
        for (const auto& track : project.tracks)
            engine.removeTrack (track.id);
        project = std::move (*loaded);
        for (const auto& track : project.tracks)   // (never a new track with an old one's number)
            project.nextTrackId = juce::jmax (project.nextTrackId, track.id + 1);
        padsChanged();

        file = source;
        forgetAllAudio();
        juce::StringArray missing;
        missingItems.clear();
        // (0.83.2) The recordings read on threads of their own, several at once, while the plug-ins load
        // here (they were read whole, one after another, before the first plug-in was started). Each
        // thread reads with its own reader into its own slot; they're taken into the cache once the
        // plug-ins are in, before anything plays.
        audioCacheRate = engine.getSampleRate();
        std::vector<juce::String> toRead;
        for (const auto& track : project.tracks)
        {
            if (track.isAudio())
                for (const auto& part : track.parts)
                    if (part.isAudio() && std::find (toRead.begin(), toRead.end(), part.audioFile) == toRead.end())
                        toRead.push_back (part.audioFile);
            if (track.frozen && track.freezeFile.isNotEmpty() && std::find (toRead.begin(), toRead.end(), track.freezeFile) == toRead.end())
                toRead.push_back (track.freezeFile);
        }
        std::vector<std::shared_ptr<const AudioData>> read (toRead.size());
        std::atomic<size_t> nextToRead { 0 };
        std::vector<std::thread> readers;
        {
            const auto folder = getAudioFolder();
            const auto count = juce::jmin (toRead.size(), (size_t) juce::jlimit (1, 4, (int) std::thread::hardware_concurrency() - 1));
            for (size_t r = 0; r < count; ++r)
                readers.emplace_back ([this, folder, &toRead, &read, &nextToRead]
                {
                    for (auto i = nextToRead++; i < toRead.size(); i = nextToRead++)
                        read[i] = readAudio (folder.getChildFile (toRead[i]));
                });
        }
        const auto takeRecordings = [&]
        {
            for (auto& reader : readers)
                if (reader.joinable())
                    reader.join();
            for (size_t i = 0; i < toRead.size(); ++i)
                if (read[i] != nullptr)
                    audioCache[toRead[i]] = read[i];
        };
        const juce::ScopeGuard readersJoined { [&] { for (auto& reader : readers) if (reader.joinable()) reader.join(); } };
        std::vector<std::pair<const Track*, juce::String>> recordingsToCheck;
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
                    if (part.isAudio())
                        recordingsToCheck.emplace_back (&track, part.audioFile);   // (checked once they're read)
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
            {
                missing.add (track.name + " (" + track.instrument.getName() + "): " + error);   // the track stays (its parts too), silent
                missingItems.push_back ({ MissingItem::Kind::instrument, track.id, -1, {} });
            }
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
                {
                    missing.add (where + ", insert " + juce::String (slot + 1) + " (" + inserts[(size_t) slot].plugin.name + "): " + error);
                    missingItems.push_back ({ MissingItem::Kind::effect, busId, slot, {} });
                }
                engine.insertEffect (busId, slot, std::move (instance));
            }
        };
        for (const auto& track : project.tracks)
            if (! track.disabled && ! track.frozen)
                loadEffects (track.id, track.name, track.inserts);
        loadEffects (Engine::masterBus, "Master", project.masterInserts);
        takeRecordings();
        for (const auto& [track, name] : recordingsToCheck)
            if (getAudio (name) == nullptr)
            {
                missing.add (track->name + ": the recording " + name + " isn't in " + getAudioFolder().getFullPathName());
                missingItems.push_back ({ MissingItem::Kind::recording, track->id, -1, name });
            }
        problems = missing.joinIntoString ("\n");
        numberParts();
        for (const auto& track : project.tracks)
            followDefaultDrumMap (track.id);
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
        applyAutoColour();   // (0.79.0)
        engine.update (project);
        clearHistory();
        recordUndoState();
        updateLiveTrack();
        modified = false;
        announce();
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
        announce();
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
        announce();
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
        announce();
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
        const auto bar = project.beatsPerBarAt (beat);
        Part part;
        part.start = juce::jmax (0.0, project.barStartAt (beat));
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
            case LineIdea::Part::pattern: return "Pattern";
            case LineIdea::Part::tune: return "Melody";
            case LineIdea::Part::drums:
            case LineIdea::Part::groove: return "Drums";
        }
        return "Idea";
    }

    void Studio::fitBends (std::vector<LineNote>& notes, int bendRange)
    {
        // (0.66.0) A bend bigger than the bend range reaches: written that much higher, bent only as far as
        // the range goes, so it still arrives on its note.
        const auto range = (float) juce::jmax (1, bendRange);
        for (auto& n : notes)
            if (n.effect == LineNote::Effect::bend && n.amount > range)
            {
                n.pitch += juce::roundToInt (n.amount - range);
                n.amount = range;
            }
    }

    std::vector<Controller> Studio::effectControllers (const std::vector<LineNote>& notes, int bendRange)
    {
        // Each group of notes with an effect starting together (a stab's two notes): its controllers.
        using E = LineNote::Effect;
        struct Group { double at, length; E effect; float amount; };
        std::vector<Group> groups;
        for (const auto& n : notes)
        {
            if (n.keyswitch || (n.effect != E::vibrato && n.effect != E::bend && n.effect != E::dive))
                continue;
            auto found = false;
            for (auto& g : groups)
                if (g.effect == n.effect && std::abs (g.at - n.beat) < HumanizeLimits::together)
                {
                    g.length = juce::jmax (g.length, n.length);
                    found = true;
                }
            if (! found)
                groups.push_back ({ n.beat, n.length, n.effect, n.amount });
        }
        std::vector<Controller> out;
        const auto range = (double) juce::jmax (1, bendRange);
        for (const auto& g : groups)
        {
            const auto end = g.at + juce::jmax (0.05, g.length);
            if (g.effect == E::bend)
            {
                // (0.66.0) Up by its amount (0: a whole step), as far as the bend range reaches.
                const auto semitones = g.amount > 0.0f ? (double) g.amount : 2.0;
                const auto bent = juce::jlimit (0, 16383, 8192 + juce::roundToInt (juce::jmin (1.0, semitones / range) * 8191.0));
                const auto up = g.at + juce::jmin (0.25, (end - g.at) * 0.5);
                out.push_back ({ Controller::Type::pitchbend, 0, g.at, 8192, false });
                out.push_back ({ Controller::Type::pitchbend, 0, up, bent, true });
                out.push_back ({ Controller::Type::pitchbend, 0, end, 8192, false });
            }
            else if (g.effect == E::dive)
            {
                // (0.66.0) The whammy bar: straight for a quarter of the note, then down by its amount (12: an
                // octave) over the rest, as far as the bend range reaches; back to none as the note ends.
                const auto semitones = g.amount > 0.0f ? (double) g.amount : 12.0;
                const auto down = juce::jlimit (0, 16383, 8192 - juce::roundToInt (juce::jmin (1.0, semitones / range) * 8192.0));
                out.push_back ({ Controller::Type::pitchbend, 0, g.at, 8192, false });
                out.push_back ({ Controller::Type::pitchbend, 0, g.at + (end - g.at) * 0.25, 8192, false });
                out.push_back ({ Controller::Type::pitchbend, 0, g.at + (end - g.at) * 0.9, down, true });
                out.push_back ({ Controller::Type::pitchbend, 0, end, 8192, false });
            }
            else
            {
                const auto peak = juce::jlimit (1, 127, juce::roundToInt (96.0f * (g.amount > 0.0f ? g.amount : 1.0f)));
                out.push_back ({ Controller::Type::cc, 1, g.at, 0, false });
                out.push_back ({ Controller::Type::cc, 1, g.at + (end - g.at) * 0.3, 0, false });
                out.push_back ({ Controller::Type::cc, 1, g.at + (end - g.at) * 0.8, peak, true });
                out.push_back ({ Controller::Type::cc, 1, end, 0, false });
            }
        }
        std::stable_sort (out.begin(), out.end(), [] (const Controller& a, const Controller& b) { return a.start < b.start; });
        return out;
    }

    Part Studio::partFromIdea (const LineIdea& idea, double start, double length, const juce::String& name) const
    {
        Part part;
        part.start = start;
        part.length = length;
        part.name = name;
        auto range = 2;
        const auto use = idea.part == LineIdea::Part::lead ? ProfileUse::lead
                       : idea.part == LineIdea::Part::rhythm ? ProfileUse::guitar : ProfileUse::bass;
        auto profile = profileFor (use);
        if (! profile.has_value() && use == ProfileUse::lead)
            profile = profileFor (ProfileUse::guitar);
        if (profile.has_value())
            range = profile->bendRange;
        auto notes = idea.notes;
        fitBends (notes, range);
        for (const auto& n : notes)
            part.notes.push_back ({ n.beat, n.length, n.pitch, n.velocity });
        part.controllers = effectControllers (notes, range);
        return part;
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
            // (0.54.0) Drums: a drum map (0.77.3: the default one), the MIDI editor's drum view.
            if (idea.part == LineIdea::Part::drums || idea.part == LineIdea::Part::groove)
                setTrackDrumMap (trackId, defaultDrumMapFor (trackId));
        }
        const auto part = partFromIdea (idea, spans.front().start, spans.back().end - spans.front().start, linePartName (idea.part) + ": " + idea.style);
        addPart (trackId, part);
        return trackId;
    }

    int Studio::findIdeaTrack (LineIdea::Part part) const
    {
        const auto name = linePartName (part);
        for (const auto& t : project.tracks)
            if (t.name == name && canTakeMidi (t.id))
                return t.id;
        return 0;
    }

    //==========================================================================
    // THE COMPOSER (0.58.0)
    ComposerContext Studio::getComposerContext (double atBeat) const
    {
        ComposerContext c;
        c.beatsPerBar = project.beatsPerBarAt (atBeat);
        c.tuning = GuitarTuning::named (getGuitarTuning());
        c.bassTuning = bassTuningNamed (getBassTuning());
        c.bassProfile = profileFor (ProfileUse::bass);
        c.guitarProfile = profileFor (ProfileUse::guitar);
        c.leadProfile = profileFor (ProfileUse::lead);
        c.velocities = getGuitarVelocities();
        c.style = getGuitarStyle();
        c.solo = getSoloStyle();
        c.riff = getRiffSettings();
        c.drums = getDrumSettings();
        c.melody = getMelodySettings();
        if (const auto name = getIdeasDrummer(); name.isNotEmpty())
            c.drummer = loadDrummer (name);
        c.humanize = getIdeasHumanize();
        c.feel = getIdeasFeel();
        c.bpm = project.bpmAt (atBeat);
        return c;
    }

    juce::String Studio::getComposerState() const
    {
        return settings != nullptr ? settings->getValue ("composer") : composerState;
    }

    void Studio::setComposerState (const juce::String& text)
    {
        composerState = text;
        if (settings != nullptr)
            settings->setValue ("composer", text);
    }

    std::optional<Project> Studio::composerAuditionCopy() const
    {
        if (! composerAudition.has_value())
            return std::nullopt;
        const auto start = composerAudition->start, end = composerAudition->end;
        // A copy of the song: each layer a part on its track, those tracks alone, the cycle round the layers.
        // (0.83.1: without the plug-ins' saved settings: this runs on every change while the composer
        // plays, a fader step included, and copied every Kontakt-sized state each time.)
        auto heard = copyWithoutPluginStates (false);
        std::set<int> playing;
        for (const auto& [trackId, idea] : composerAudition->layers)
        {
            if (! canTakeMidi (trackId) || idea.notes.empty())
                continue;
            auto* track = heard.findTrack (trackId);
            track->parts.push_back (partFromIdea (idea, start, end - start, idea.style));
            playing.insert (trackId);
        }
        if (playing.empty())
            return std::nullopt;
        // (0.76.0) Soloed in the mixer among the layers' tracks: only those; muted: not heard. The rest of the
        // song stays silent, as before.
        const auto anySoloed = std::any_of (heard.tracks.begin(), heard.tracks.end(),
                                            [&playing] (const Track& t) { return t.solo && playing.count (t.id) > 0; });
        for (auto& t : heard.tracks)
            t.solo = playing.count (t.id) > 0 && (! anySoloed || t.solo);
        heard.loopOn = true;
        heard.loopStart = start;
        heard.loopEnd = end;
        return heard;
    }

    bool Studio::auditionComposer (const std::vector<std::pair<int, LineIdea>>& layers, double start, double end)
    {
        if (exportJob != nullptr || end <= start || engine.isRecording())
            return false;
        const auto first = ! composerAudition.has_value();
        const auto before = composerAudition;
        if (first)
            composerAudition = ComposerAudition { start, end, engine.getPosition(), layers };
        else
        {
            composerAudition->start = start;
            composerAudition->end = end;
            composerAudition->layers = layers;
        }
        const auto heard = composerAuditionCopy();
        if (! heard.has_value())
        {
            composerAudition = before;
            return false;
        }
        if (first && engine.isPlaying())
        {
            // From the song's own playing: stopped first (where it was is where it goes back to).
            punchOutAll();
            engine.stop();
            composerAudition->wasAt = engine.getPosition();
        }
        engine.update (*heard);
        if (first || ! engine.isPlaying())
        {
            engine.setPosition (start);
            engine.play();
        }
        else if (const auto at = engine.getPosition(); at < start || at >= end)
            engine.setPosition (start);
        announce();
        return true;
    }

    void Studio::endComposerAudition (bool backToSong)
    {
        if (! composerAudition.has_value())
            return;
        const auto wasAt = composerAudition->wasAt;
        composerAudition.reset();
        engine.stop();
        engine.setPosition (wasAt);
        if (backToSong && exportJob == nullptr)
            engine.update (project);
    }

    void Studio::stopComposerAudition()
    {
        if (! composerAudition.has_value())
            return;
        endComposerAudition (true);
        announce();
    }

    std::vector<int> Studio::buildSection (const std::vector<LineIdea>& ideas, const std::vector<ChordSpan>& spans, const juce::String& sectionName,
                                           const std::vector<int>& onto)
    {
        std::vector<int> used;
        if (spans.empty())
            return used;
        const auto start = spans.front().start, end = spans.back().end;
        // The tracks first (adding a track is a step of its own), then every part in one change.
        std::vector<std::pair<int, const LineIdea*>> plan;
        for (size_t i = 0; i < ideas.size(); ++i)
        {
            const auto& idea = ideas[i];
            if (idea.notes.empty())
                continue;
            auto trackId = i < onto.size() && canTakeMidi (onto[i]) ? onto[i] : findIdeaTrack (idea.part);
            if (trackId == 0)
            {
                trackId = addTrack (Instrument {});
                if (trackId == 0)
                    continue;
                renameTrack (trackId, linePartName (idea.part));
                if (idea.part == LineIdea::Part::drums || idea.part == LineIdea::Part::groove)
                    setTrackDrumMap (trackId, defaultDrumMapFor (trackId));
            }
            plan.emplace_back (trackId, &idea);
            used.push_back (trackId);
        }
        for (const auto& [trackId, idea] : plan)
            if (auto* track = project.findTrack (trackId))
            {
                track->parts.erase (std::remove_if (track->parts.begin(), track->parts.end(), [&] (const Part& p)
                                    { return p.start >= start - 1.0e-9 && p.start + p.length <= end + 1.0e-9; }), track->parts.end());
                track->parts.push_back (partFromIdea (*idea, start, end - start, (sectionName.isNotEmpty() ? sectionName + ": " : juce::String()) + idea->style));
                track->parts.back().id = 0;
            }
        if (! plan.empty())
        {
            SessionLog::note ("Build section: " + sectionName + ", " + juce::String ((int) plan.size()) + " parts over beats "
                              + juce::String (start, 2) + " to " + juce::String (end, 2));
            changed();
        }
        return used;
    }

    void Studio::noteBuiltSection (const BuiltSection& section)
    {
        builtSections.erase (std::remove_if (builtSections.begin(), builtSections.end(), [&] (const BuiltSection& b)
                             { return b.start < section.end - 1.0e-6 && section.start < b.end - 1.0e-6; }), builtSections.end());
        builtSections.push_back (section);
        std::sort (builtSections.begin(), builtSections.end(), [] (const BuiltSection& a, const BuiltSection& b) { return a.start < b.start; });
    }

    std::vector<ChordSpan> Studio::chordsForNextSection (const std::vector<ChordSpan>& spans, double from)
    {
        if (spans.empty())
            return {};
        const auto length = spans.back().end - spans.front().start;
        const auto to = from + length;
        const auto& events = project.chordTrack.events;
        std::vector<int> which;
        auto startsInside = false;
        for (int i = 0; i < (int) events.size(); ++i)
        {
            const auto beat = events[(size_t) i].beat;
            const auto next = i + 1 < (int) events.size() ? events[(size_t) i + 1].beat : 1.0e12;
            if (beat < to - 1.0e-6 && next > from + 1.0e-6)
                which.push_back (i);
            startsInside = startsInside || (beat >= from - 1.0e-6 && beat < to - 1.0e-6);
        }
        if (startsInside)
        {
            auto found = getLineChords (which);
            std::vector<ChordSpan> out;
            for (auto span : found)
            {
                span.start = juce::jmax (span.start, from);
                span.end = juce::jmin (span.end, to);
                if (span.end > span.start + 1.0e-6)
                    out.push_back (span);
            }
            if (! out.empty())
            {
                out.back().end = to;   // (the last chord lasts to the section's end)
                return out;
            }
        }
        // None there: these chords again, onto the chord track too.
        const auto shift = from - spans.front().start;
        std::vector<ChordSpan> out;
        auto all = events;
        for (auto span : spans)
        {
            span.start += shift;
            span.end += shift;
            out.push_back (span);
            all.push_back ({ span.start, span.chord });
        }
        project.chordTrack.shown = true;
        setChordEvents (std::move (all));
        return out;
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

    GuitarStyle Studio::getGuitarStyle() const
    {
        return settings != nullptr && settings->containsKey ("guitarStyle") ? guitarStyleNamed (settings->getValue ("guitarStyle")) : guitarStyle;
    }

    void Studio::setGuitarStyle (GuitarStyle s)
    {
        guitarStyle = s;
        if (settings != nullptr)
            settings->setValue ("guitarStyle", guitarStyleName (s));
    }

    SoloStyle Studio::getSoloStyle() const
    {
        return settings != nullptr && settings->containsKey ("soloStyle") ? soloStyleNamed (settings->getValue ("soloStyle")) : soloStyle;
    }

    void Studio::setSoloStyle (SoloStyle s)
    {
        soloStyle = s;
        if (settings != nullptr)
            settings->setValue ("soloStyle", soloStyleName (s));
    }

    RiffSettings Studio::getRiffSettings() const
    {
        if (settings != nullptr && settings->containsKey ("riffSettings"))
            return RiffSettings::fromString (settings->getValue ("riffSettings"));
        return riffSettings;
    }

    void Studio::setRiffSettings (const RiffSettings& r)
    {
        riffSettings = r.limited();
        if (settings != nullptr)
            settings->setValue ("riffSettings", riffSettings.toString());
    }

    DrumSettings Studio::getDrumSettings() const
    {
        if (settings != nullptr && settings->containsKey ("drumSettings"))
            return DrumSettings::fromString (settings->getValue ("drumSettings"));
        return drumSettings;
    }

    void Studio::setDrumSettings (const DrumSettings& d)
    {
        drumSettings = d.limited();
        if (settings != nullptr)
            settings->setValue ("drumSettings", drumSettings.toString());
    }

    std::optional<DrumProfile> Studio::loadDrummer (const juce::String& name) const
    {
        if (name.isEmpty())
            return std::nullopt;
        const auto f = drummerFile (getDrummerFolder(), name);
        const auto modified = f.getLastModificationTime();
        const auto size = f.getSize();
        if (keptDrummer.file == f && keptDrummer.modified == modified && keptDrummer.size == size && f.existsAsFile())
            return keptDrummer.profile;
        auto profile = DrumProfile::load (f);
        if (profile.has_value() && profile->name.isEmpty())
            profile->name = name;
        keptDrummer = { f, modified, size, profile };
        return profile;
    }

    bool Studio::saveDrummer (const DrumProfile& profile, juce::String& error)
    {
        const auto file = drummerFile (getDrummerFolder(), profile.name);
        if (! profile.save (file))
        {
            error = "Couldn't write " + file.getFullPathName();
            return false;
        }
        SessionLog::note ("Drummer profile saved: " + file.getFullPathName() + " (" + juce::String (profile.barsLearned()) + " bars from "
                          + juce::String ((int) profile.songs.size()) + " songs)");
        return true;
    }

    bool Studio::deleteDrummer (const juce::String& name)
    {
        const auto file = drummerFile (getDrummerFolder(), name);
        if (! file.existsAsFile() || ! file.deleteFile())
            return false;
        if (getIdeasDrummer() == name)
            setIdeasDrummer ({});
        return true;
    }

    juce::String Studio::getIdeasDrummer() const
    {
        if (settings != nullptr && settings->containsKey ("ideasDrummer"))
            return settings->getValue ("ideasDrummer");
        return ideasDrummer;
    }

    void Studio::setIdeasDrummer (const juce::String& name)
    {
        ideasDrummer = name;
        if (settings != nullptr)
            settings->setValue ("ideasDrummer", name);
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

    MelodySettings Studio::getMelodySettings() const
    {
        if (settings != nullptr && settings->containsKey ("melodySettings"))
            return MelodySettings::fromString (settings->getValue ("melodySettings"));
        return melodySettings;
    }

    void Studio::setMelodySettings (const MelodySettings& m)
    {
        melodySettings = m.limited();
        if (settings != nullptr)
            settings->setValue ("melodySettings", melodySettings.toString());
    }

    juce::String Studio::getBassTuning() const
    {
        const auto name = settings != nullptr ? settings->getValue ("bassTuning") : bassTuning;
        return bassTuningNamed (name).name;
    }

    std::vector<InstrumentProfile> Studio::getInstrumentProfiles() const
    {
        const auto stored = settings != nullptr ? (settings->containsKey ("instrumentProfiles") ? std::optional<juce::String> (settings->getValue ("instrumentProfiles"))
                                                                                                 : std::nullopt)
                                                : instrumentProfiles;
        if (! stored.has_value())
            return { InstrumentProfile::djinnBass2() };   // (the first time: the owner's bass)
        auto profiles = profilesFromText (*stored);
        // (0.82.2, the owner: their bass is drop A, A E A D G) A Djinn Bass 2 saved with 0.62.0's A D A D G gets its
        // E string, once (the flag), so a tuning set by hand afterwards stays.
        if (settings != nullptr && ! settings->getBoolValue ("djinnDropA0822", false))
        {
            for (auto& p : profiles)
                if (p.name == "Djinn Bass 2" && p.strings == std::vector<int> { 33, 38, 45, 50, 55 })
                {
                    p.strings[1] = 40;
                    for (auto& k : p.keyswitches)
                        if (k.name == "Force E String (your D1)")
                            k.name = "Force E String";
                }
            settings->setValue ("instrumentProfiles", profilesToText (profiles));
            settings->setValue ("djinnDropA0822", true);
        }
        return profiles;
    }

    void Studio::setInstrumentProfiles (const std::vector<InstrumentProfile>& profiles)
    {
        const auto text = profilesToText (profiles);
        instrumentProfiles = text;
        if (settings != nullptr)
            settings->setValue ("instrumentProfiles", text);
    }

    std::optional<InstrumentProfile> Studio::profileFor (ProfileUse use) const
    {
        for (const auto& p : getInstrumentProfiles())
            if ((use == ProfileUse::bass && p.forBass) || (use == ProfileUse::guitar && p.forGuitar) || (use == ProfileUse::lead && p.forLead))
                return p;
        return {};
    }

    void Studio::setBassTuning (const juce::String& name)
    {
        bassTuning = bassTuningNamed (name).name;
        if (settings != nullptr)
            settings->setValue ("bassTuning", bassTuning);
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
        // (Bar by bar by the signature track, 0.45.0.)
        const auto map = project.signatures();
        const auto firstBar = map.barAt (juce::jmax (0.0, atBeat));
        const auto start = map.beatOfBar (firstBar);
        const auto end = map.beatOfBar (firstBar + (int) chords.size());
        // What was over those bars goes; then a chord a bar.
        std::vector<ChordEvent> events;
        for (const auto& e : project.chordTrack.events)
            if (e.beat < start - 1.0e-6 || e.beat >= end - 1.0e-6)
                events.push_back (e);
        for (size_t i = 0; i < chords.size(); ++i)
            events.push_back ({ map.beatOfBar (firstBar + (int) i), chords[i] });
        std::sort (events.begin(), events.end(), [] (const ChordEvent& a, const ChordEvent& b) { return a.beat < b.beat; });
        project.chordTrack.shown = true;   // (in the same undo step)
        project.chordTrack.events = std::move (events);
        chordTrackChanged();
    }

    void Studio::addProgressionToChordTrack (const std::vector<ProgressionStep>& steps, double atBeat)
    {
        if (steps.empty())
            return;
        const auto map = project.signatures();
        const auto firstBar = map.barAt (juce::jmax (0.0, atBeat));
        const auto start = map.beatOfBar (firstBar);
        // Each step its bars long, by the bar it starts in (half bars too).
        std::vector<double> at;
        auto end = start;
        for (const auto& s : steps)
        {
            at.push_back (end);
            end += s.bars * map.barLengthAt (end);
        }
        std::vector<ChordEvent> events;
        for (const auto& e : project.chordTrack.events)
            if (e.beat < start - 1.0e-6 || e.beat >= end - 1.0e-6)
                events.push_back (e);
        for (size_t i = 0; i < steps.size(); ++i)   // (0.85.3: each as long as its step, the last too)
            events.push_back ({ at[i], steps[i].chord, (i + 1 < steps.size() ? at[i + 1] : end) - at[i] });
        std::sort (events.begin(), events.end(), [] (const ChordEvent& a, const ChordEvent& b) { return a.beat < b.beat; });
        project.chordTrack.shown = true;
        project.chordTrack.events = std::move (events);
        chordTrackChanged();
    }

    std::vector<ChordSpan> Studio::progressionSpans (const std::vector<ProgressionStep>& steps, double startBeat, const Key& key) const
    {
        const auto bar = project.beatsPerBar();
        std::vector<ChordSpan> spans;
        auto beat = startBeat;
        for (const auto& s : steps)
        {
            spans.push_back ({ beat, beat + s.bars * bar, s.chord, key });
            beat += s.bars * bar;
        }
        return spans;
    }

    juce::StringArray Studio::getProgressionFavorites() const
    {
        if (settings == nullptr)
            return progressionFavorites;
        return juce::StringArray::fromLines (settings->getValue ("progressionFavorites"));
    }

    void Studio::setProgressionFavorite (const juce::String& name, bool on)
    {
        auto favorites = getProgressionFavorites();
        favorites.removeEmptyStrings();
        if (on)
            favorites.addIfNotAlreadyThere (name);
        else
            favorites.removeString (name);
        progressionFavorites = favorites;
        if (settings != nullptr)
            settings->setValue ("progressionFavorites", favorites.joinIntoString ("\n"));
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

    std::vector<int> Studio::placeChordEvents (const std::vector<ChordEvent>& placed, std::vector<ChordEvent> others)
    {
        auto result = placeChords (std::move (others), placed);
        project.chordTrack.shown = true;
        project.chordTrack.events = std::move (result.events);
        chordTrackChanged();   // (sorts: by beat already, so the places hold)
        return result.placed;
    }

    std::vector<int> Studio::moveChordEvents (const std::vector<int>& indices, double delta, bool copy)
    {
        const auto all = withLengths (project.chordTrack, project.beatsPerBar());
        std::vector<ChordEvent> moved, others;
        for (int i = 0; i < (int) all.size(); ++i)
        {
            const auto chosen = std::find (indices.begin(), indices.end(), i) != indices.end();
            if (chosen)
            {
                auto e = all[(size_t) i];
                e.beat = e.beat + delta;
                moved.push_back (e);
            }
            if (! chosen || copy)
                others.push_back (all[(size_t) i]);
        }
        if (moved.empty())
            return {};
        // (Not before the song's start: the earliest stops at 0.)
        auto earliest = moved.front().beat;
        for (const auto& m : moved)
            earliest = juce::jmin (earliest, m.beat);
        if (earliest < 0.0)
            for (auto& m : moved)
                m.beat -= earliest;
        return placeChordEvents (moved, std::move (others));
    }

    int Studio::resizeChordEvent (int index, double newStart, double newEnd)
    {
        if (! juce::isPositiveAndBelow (index, (int) project.chordTrack.events.size()))
            return -1;
        auto all = withLengths (project.chordTrack, project.beatsPerBar());
        auto chord = all[(size_t) index];
        newStart = juce::jmax (0.0, newStart);
        if (newEnd - newStart < shortestChord)
            return index;
        chord.beat = newStart;
        chord.length = newEnd - newStart;
        all.erase (all.begin() + index);
        const auto placed = placeChordEvents ({ chord }, std::move (all));
        return placed.empty() ? -1 : placed.front();
    }

    std::vector<int> Studio::splitChordEvents (double beat, const std::vector<int>& indices)
    {
        auto all = withLengths (project.chordTrack, project.beatsPerBar());
        std::vector<double> splitAt;
        auto any = false;
        std::vector<ChordEvent> out;
        for (int i = 0; i < (int) all.size(); ++i)
        {
            const auto& e = all[(size_t) i];
            const auto chosen = indices.empty() || std::find (indices.begin(), indices.end(), i) != indices.end();
            if (chosen && beat > e.beat + shortestChord - 1.0e-9 && beat < e.beat + e.length - shortestChord + 1.0e-9)
            {
                auto first = e, second = e;
                first.length = beat - e.beat;
                second.beat = beat;
                second.length = e.beat + e.length - beat;
                out.push_back (first);
                out.push_back (second);
                any = true;
            }
            else
                out.push_back (e);
        }
        if (! any)
            return {};
        project.chordTrack.events = std::move (out);
        chordTrackChanged();
        std::vector<int> halves;   // (the second halves: where each cut is)
        for (int i = 0; i < (int) project.chordTrack.events.size(); ++i)
            if (std::abs (project.chordTrack.events[(size_t) i].beat - beat) < 1.0e-6)
                halves.push_back (i);
        return halves;
    }

    int Studio::glueChordEvent (int index)
    {
        auto all = withLengths (project.chordTrack, project.beatsPerBar());
        if (! juce::isPositiveAndBelow (index, (int) all.size() - 1))
            return -1;
        auto& e = all[(size_t) index];
        const auto& next = all[(size_t) index + 1];
        e.length = next.beat + next.length - e.beat;
        all.erase (all.begin() + index + 1);
        project.chordTrack.events = std::move (all);
        chordTrackChanged();
        return index;
    }

    Studio::ChordClip Studio::copyChordEvents (const std::vector<int>& indices) const
    {
        const auto all = withLengths (project.chordTrack, project.beatsPerBar());
        ChordClip clip;
        auto first = 1.0e12, last = 0.0;
        for (const auto i : indices)
            if (juce::isPositiveAndBelow (i, (int) all.size()))
            {
                first = juce::jmin (first, all[(size_t) i].beat);
                last = juce::jmax (last, all[(size_t) i].beat + all[(size_t) i].length);
            }
        for (int i = 0; i < (int) all.size(); ++i)
            if (std::find (indices.begin(), indices.end(), i) != indices.end())
            {
                auto e = all[(size_t) i];
                e.beat -= first;
                clip.events.push_back (e);
            }
        clip.length = clip.events.empty() ? 0.0 : last - first;
        return clip;
    }

    std::vector<int> Studio::pasteChordEvents (const ChordClip& clip, double atBeat)
    {
        if (clip.events.empty())
            return {};
        auto placed = clip.events;
        for (auto& e : placed)
            e.beat += juce::jmax (0.0, atBeat);
        return placeChordEvents (placed, withLengths (project.chordTrack, project.beatsPerBar()));
    }

    std::vector<int> Studio::duplicateChordEvents (const std::vector<int>& indices)
    {
        const auto clip = copyChordEvents (indices);
        if (clip.events.empty())
            return {};
        auto first = 1.0e12;
        for (const auto i : indices)
            if (juce::isPositiveAndBelow (i, (int) project.chordTrack.events.size()))
                first = juce::jmin (first, project.chordTrack.events[(size_t) i].beat);
        return pasteChordEvents (clip, first + clip.length);
    }

    void Studio::removeChordEvents (std::vector<int> indices)
    {
        std::sort (indices.rbegin(), indices.rend());
        indices.erase (std::unique (indices.begin(), indices.end()), indices.end());
        // (0.85.3) As clips: the others keep their lengths, so a gap is left (the one before ran on).
        if (std::any_of (indices.begin(), indices.end(), [this] (int i) { return juce::isPositiveAndBelow (i, (int) project.chordTrack.events.size()); }))
            project.chordTrack.events = withLengths (project.chordTrack, project.beatsPerBar());
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

    void Studio::setChordTrackKey (double fromBeat, const Key& key)
    {
        fromBeat = juce::jmax (0.0, fromBeat);
        std::vector<ScaleEvent> scales;
        for (const auto& sc : project.chordTrack.scales)
            if (sc.beat < fromBeat - 1.0e-6)
                scales.push_back (sc);
        if (scales.empty() || ! (scales.back().key == key))
            scales.push_back ({ fromBeat, key });
        if (! project.chordTrack.autoScales && scales == project.chordTrack.scales)
            return;
        setScaleEvents (std::move (scales));
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

    std::vector<KeyIssue> Studio::checkSongKey (std::optional<Key> chosen) const
    {
        return checkKey (project, keyCheckKeys (project, chosen));
    }

    bool Studio::fixOutOfKeyNotes (const std::vector<KeyIssue>& issues)
    {
        auto any = false;
        for (const auto& issue : issues)
        {
            if (issue.kind != KeyIssue::Kind::note)
                continue;
            const auto where = findPart (issue.partId);
            if (! where.has_value())
                continue;
            auto* track = project.findTrack (where->first);
            if (track == nullptr || track->frozen || ! juce::isPositiveAndBelow (where->second, (int) track->parts.size()))
                continue;
            auto& part = track->parts[(size_t) where->second];
            if (! juce::isPositiveAndBelow (issue.noteIndex, (int) part.notes.size()))
                continue;
            auto& note = part.notes[(size_t) issue.noteIndex];
            if (part.heardPitch (note.pitch) != issue.pitch)
                continue;   // (changed since the check)
            note.pitch = juce::jlimit (0, 127, issue.suggested - part.transpose);
            any = true;
        }
        if (any)
            changed();
        return any;
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
        const auto first = track.events[(size_t) indices.front()].beat;
        const auto bar = project.beatsPerBarAt (first);
        const auto start = project.barStartAt (first);
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
                        notes.push_back ({ part.start + n.start, part.start + juce::jmin (n.start + n.length, part.length), part.heardPitch (n.pitch), n.velocity });
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

    std::vector<ChordEvent> Studio::detectChords (const std::vector<int>& partIds, const DetectSettings& settings)
    {
        const auto step = juce::jmax (0.25, settings.stepBeats);
        const auto tempo = project.tempoMap();
        std::vector<const Part*> parts;
        auto from = std::numeric_limits<double>::max(), to = 0.0;
        for (const auto id : partIds)
            if (const auto at = findPart (id))
            {
                const auto& part = project.findTrack (at->first)->parts[(size_t) at->second];
                parts.push_back (&part);
                from = juce::jmin (from, part.start);
                to = juce::jmax (to, part.end());
            }
        if (parts.empty() || to <= from)
            return {};
        // The grid from the step at or before the first start (bars' starts when the step is a bar).
        from = std::floor (from / step + 1.0e-9) * step;
        const auto count = (size_t) std::ceil ((to - from) / step - 1.0e-9);
        std::vector<ChromaStep> steps (count);
        const auto addIn = [&steps] (const std::vector<ChromaStep>& more)
        {
            // Each source's steps weighed alike: scaled so its strongest pitch class is 1.
            for (size_t i = 0; i < steps.size() && i < more.size(); ++i)
            {
                const auto top = *std::max_element (more[i].weight.begin(), more[i].weight.end());
                const auto bassTop = *std::max_element (more[i].bass.begin(), more[i].bass.end());
                for (size_t p = 0; p < 12; ++p)
                {
                    steps[i].weight[p] += top > 0.0 ? more[i].weight[p] / top : 0.0;
                    steps[i].bass[p] += bassTop > 0.0 ? more[i].bass[p] / bassTop : 0.0;
                }
            }
        };
        std::vector<HeardNote> notes;
        for (const auto* part : parts)
        {
            if (part->isAudio())
            {
                auto rate = 0.0;
                const auto start = tempo.secondsAt (part->start), end = tempo.secondsAt (part->end());
                const auto sound = clipMono (*part, start, end, rate);
                if (! sound.empty() && rate > 0.0)
                    addIn (chromaFromAudio (sound.data(), (int) sound.size(), rate, part->start, tempo, from, from + step * (double) count, step));
            }
            else
                for (const auto& n : part->notes)
                    if (n.start < part->length && ! part->muted)
                        notes.push_back ({ part->start + n.start, part->start + juce::jmin (n.start + n.length, part->length), part->heardPitch (n.pitch), n.velocity });
        }
        if (! notes.empty())
            addIn (chromaFromNotes (notes, from, from + step * (double) count, step));
        return chordsFromChroma (steps, from, step, settings);
    }

    int Studio::detectChordsToTrack (const std::vector<int>& partIds, const DetectSettings& settings)
    {
        setDetectSettings (settings);
        const auto found = detectChords (partIds, settings);
        if (found.empty())
            return 0;
        auto from = found.front().beat, to = 0.0;
        for (const auto id : partIds)
            if (const auto at = findPart (id))
                to = juce::jmax (to, project.findTrack (at->first)->parts[(size_t) at->second].end());
        auto events = project.chordTrack.events;
        events.erase (std::remove_if (events.begin(), events.end(), [from, to] (const ChordEvent& e) { return e.beat >= from - 1.0e-9 && e.beat < to - 1.0e-9; }),
                      events.end());
        events.insert (events.end(), found.begin(), found.end());
        std::sort (events.begin(), events.end(), [] (const ChordEvent& a, const ChordEvent& b) { return a.beat < b.beat; });
        project.chordTrack.events = std::move (events);
        project.chordTrack.shown = true;
        chordTrackChanged();
        return (int) found.size();
    }

    int Studio::detectChordsToPads (const std::vector<int>& partIds, const DetectSettings& settings)
    {
        setDetectSettings (settings);
        std::vector<Chord> chords;
        for (const auto& e : detectChords (partIds, settings))
            if (std::find (chords.begin(), chords.end(), e.chord) == chords.end())
                chords.push_back (e.chord);
        return chords.empty() ? 0 : assignProgressionToPads (chords, 0);
    }

    DetectSettings Studio::getDetectSettings() const
    {
        if (settings != nullptr && settings->containsKey ("chordDetect"))
            return DetectSettings::fromString (settings->getValue ("chordDetect"));
        return detectSettings;
    }

    void Studio::setDetectSettings (const DetectSettings& d)
    {
        detectSettings = d;
        if (settings != nullptr)
            settings->setValue ("chordDetect", d.toString());
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

    //==========================================================================
    // Follow Chord Track (0.36.0)
    namespace
    {
        bool canFollow (const Track* track) noexcept
        {
            return track != nullptr && track->kind == Track::Kind::instrument && ! track->frozen;
        }

        // The chords a track's (or a part's) own notes make, as a reference ("C" if none found).
        FollowReference analysedReference (const std::vector<std::pair<const Part*, double>>& parts, const Key& fallback)
        {
            std::vector<HeardNote> notes;
            auto from = std::numeric_limits<double>::max(), to = 0.0;
            for (const auto& [part, start] : parts)
            {
                from = juce::jmin (from, start);
                to = juce::jmax (to, start + part->length);
                for (const auto& n : part->notes)
                    if (n.start < part->length)
                        notes.push_back ({ start + n.start, start + juce::jmin (n.start + n.length, part->length), n.pitch, n.velocity });
            }
            FollowReference r;
            if (! notes.empty())
                r.chords = chordsFromNotes (notes, from, to, 1.0);
            if (r.chords.empty())
                return FollowReference::ofChord ({});   // (the manual: "C")
            r.scales = scalesFromChords (r.chords, fallback);
            return r;
        }
    }

    bool Studio::needsFollowSetup (int trackId) const
    {
        const auto* track = project.findTrack (trackId);
        return canFollow (track) && track->followReference.isEmpty();
    }

    bool Studio::startFollowing (int trackId, FollowMode mode, FollowStart start, const Chord& known)
    {
        auto* track = project.findTrack (trackId);
        if (! canFollow (track))
            return false;
        switch (start)
        {
            case FollowStart::directly:
                track->followReference = FollowReference::ofChordTrack (project.chordTrack);
                if (track->followReference.isEmpty())
                    track->followReference = FollowReference::ofChord ({});
                break;
            case FollowStart::analyze:
            {
                std::vector<std::pair<const Part*, double>> parts;
                for (const auto& part : track->parts)
                    if (! part.isAudio() && ! part.muted)
                        parts.push_back ({ &part, part.start });
                track->followReference = analysedReference (parts, project.chordPads.key());
                break;
            }
            case FollowStart::knownChord:
                track->followReference = FollowReference::ofChord (known);
                break;
        }
        track->follow = mode;
        SessionLog::note ("Follow Chord Track: " + track->name + ", " + followModeName (mode) + ", "
                          + juce::String (track->followReference.chords.size()) + " reference chord(s)");
        changed();
        return true;
    }

    void Studio::setFollowMode (int trackId, FollowMode mode)
    {
        auto* track = project.findTrack (trackId);
        if (! canFollow (track) || track->follow == mode)
            return;
        if (mode != FollowMode::off && track->followReference.isEmpty())
        {
            startFollowing (trackId, mode, FollowStart::directly);
            return;
        }
        track->follow = mode;
        changed();
    }

    void Studio::resetFollowing (int trackId)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || (track->follow == FollowMode::off && track->followReference.isEmpty()))
            return;
        track->follow = FollowMode::off;
        track->followReference = {};
        changed();
    }

    void Studio::setLiveFollow (int trackId, LiveFollow live)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || track->kind != Track::Kind::instrument || track->liveFollow == live)
            return;
        track->liveFollow = live;
        changed (false);   // (as the MIDI input: a setting, not an edit)
    }

    void Studio::setFollowVoice (int trackId, Voice voice)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || track->followVoice == voice)
            return;
        track->followVoice = voice;
        changed();
    }

    void Studio::setTrackVoicing (int trackId, std::optional<VoicingSettings> voicing)
    {
        auto* track = project.findTrack (trackId);
        if (track == nullptr || track->followVoicing == voicing)
            return;
        track->followVoicing = voicing;
        changed();
    }

    void Studio::setChordTrackVoicing (const VoicingSettings& voicing)
    {
        if (project.chordTrack.voicing == voicing)
            return;
        project.chordTrack.voicing = voicing;
        changed();
    }

    std::vector<std::vector<int>> Studio::getChordTrackVoicings (int trackId) const
    {
        const auto* track = project.findTrack (trackId);
        const auto& settings = track != nullptr && track->followVoicing ? *track->followVoicing : project.chordTrack.voicing;
        return voiceChords (project.chordTrack.events, settings);
    }

    void Studio::setPadPattern (int index)
    {
        if (! juce::isPositiveAndBelow (index, (int) patterns().size()))
            index = -1;
        performer->bpm = project.bpmAt (engine.getPosition());
        performer->beatsPerBar = project.beatsPerBar();
        performer->setPattern (index);
        if (settings != nullptr)
            settings->setValue ("padPattern", index < 0 ? juce::String() : patterns()[(size_t) index].name);
    }

    int Studio::getPadPattern() const
    {
        return performer->getPattern();
    }

    void Studio::setPerformPattern (int trackId, const juce::String& name)
    {
        auto* track = project.findTrack (trackId);
        const auto chosen = patternNamed (name) >= 0 ? patterns()[(size_t) patternNamed (name)].name : juce::String();
        if (track == nullptr || track->kind != Track::Kind::instrument || track->performPattern == chosen)
            return;
        track->performPattern = chosen;
        changed();
    }

    namespace
    {
        void keepUserPatterns (juce::PropertiesFile* settings)
        {
            if (settings == nullptr)
                return;
            juce::XmlElement xml ("UserPatterns");
            for (const auto& p : getUserPatterns())
                xml.addChildElement (patternToXml (p).release());
            settings->setValue ("userPatterns", xml.toString (juce::XmlElement::TextFormat().singleLine()));
        }
    }

    juce::String Studio::addPatternFromPart (int partId, juce::String name)
    {
        const auto at = findPart (partId);
        if (! at)
            return {};
        const auto& part = project.findTrack (at->first)->parts[(size_t) at->second];
        if (part.isAudio() || part.notes.empty())
            return {};
        std::vector<LineNote> notes;
        for (const auto& n : part.notes)
            if (n.start >= 0.0 && n.start < part.length)
                notes.push_back ({ n.start, juce::jmin (n.length, part.length - n.start), n.pitch, n.velocity });
        if (name.trim().isEmpty())
            name = part.name.isNotEmpty() ? part.name : "My pattern";
        // A name not taken (ours or another of yours).
        auto unique = name.trim();
        for (int i = 2; patternNamed (unique) >= 0; ++i)
            unique = name.trim() + " " + juce::String (i);
        auto mine = getUserPatterns();
        mine.push_back (patternFromNotes (unique, notes, part.length, project.beatsPerBar()));
        setUserPatterns (std::move (mine));
        keepUserPatterns (settings.get());
        return unique;
    }

    void Studio::removeUserPattern (const juce::String& name)
    {
        auto mine = getUserPatterns();
        const auto before = mine.size();
        mine.erase (std::remove_if (mine.begin(), mine.end(), [&name] (const Pattern& p) { return p.name == name; }), mine.end());
        if (mine.size() == before)
            return;
        const auto playing = performer->getPattern() >= 0 ? patterns()[(size_t) performer->getPattern()].name : juce::String();
        setUserPatterns (std::move (mine));
        keepUserPatterns (settings.get());
        performer->setPattern (patternNamed (playing));   // (its index may have moved; gone: none)
        changed();   // (a track performing it stops)
    }

    void Studio::setChordShift (int ticks)
    {
        ticks = juce::jlimit (-480, 480, ticks);
        if (project.chordTrack.shiftTicks == ticks)
            return;
        project.chordTrack.shiftTicks = ticks;
        changed();
    }

    std::vector<int> Studio::followedPitches (int partId) const
    {
        const auto at = findPart (partId);
        if (! at)
            return {};
        const auto& track = *project.findTrack (at->first);
        const auto& part = track.parts[(size_t) at->second];
        std::vector<int> pitches;
        if (track.follow == FollowMode::off || project.chordTrack.events.empty() || part.isAudio())
        {
            for (const auto& n : part.notes)
                pitches.push_back (n.pitch);
            return pitches;
        }
        for (const auto& f : followNotes (part.notes, part.start, track.follow, track.followReference, project.chordTrack,
                                          project.chordTrack.shiftTicks, project.chordPads.key(),
                                          track.followVoicing ? &*track.followVoicing : nullptr, track.followVoice))
            pitches.push_back (f.dropped ? -1 : f.pitch);
        return pitches;
    }

    int Studio::mapToChordTrack (const std::vector<int>& partIds, FollowMode mode)
    {
        if (project.chordTrack.events.empty() || mode == FollowMode::off)
            return -1;
        auto moved = 0;
        auto any = false;
        for (const auto id : partIds)
        {
            const auto at = findPart (id);
            if (! at)
                continue;
            auto& track = *project.findTrack (at->first);
            if (track.frozen || track.isAudio())
                continue;
            auto& part = track.parts[(size_t) at->second];
            if (part.isAudio() || part.notes.empty())
                continue;
            const auto reference = analysedReference ({ { &part, part.start } }, project.chordPads.key());
            const auto followed = followNotes (part.notes, part.start, mode, reference, project.chordTrack,
                                               project.chordTrack.shiftTicks, project.chordPads.key(),
                                               track.followVoicing ? &*track.followVoicing : nullptr, track.followVoice);
            std::vector<Note> notes;
            for (size_t n = 0; n < part.notes.size(); ++n)
            {
                if (followed[n].dropped)
                {
                    ++moved;
                    continue;
                }
                auto note = part.notes[n];
                if (note.pitch != followed[n].pitch)
                    ++moved;
                note.pitch = followed[n].pitch;
                notes.push_back (note);
            }
            part.notes = std::move (notes);
            any = true;
        }
        if (any)
            changed();
        return any ? moved : -1;
    }

    //==========================================================================
    // Audio Alignment (0.33.0)
    AlignSettings Studio::getAlignSettings() const
    {
        if (settings != nullptr && settings->containsKey ("audioAlignment"))
            return AlignSettings::fromString (settings->getValue ("audioAlignment"));
        return alignSettings;
    }

    void Studio::setAlignSettings (const AlignSettings& s)
    {
        alignSettings = s;
        if (settings != nullptr)
            settings->setValue ("audioAlignment", s.toString());
    }

    std::vector<float> Studio::heardMono (int trackId, double from, double to, double& rate, int skipPartId)
    {
        rate = 0.0;
        const auto* track = project.findTrack (trackId);
        if (track == nullptr || ! track->isAudio() || to <= from)
            return {};
        const auto tempo = project.tempoMap();
        for (const auto& p : track->parts)
            if (p.isAudio())
                if (const auto data = getHeardAudio (p.audioFile))
                {
                    rate = data->sampleRate;
                    break;
                }
        if (rate <= 0.0)
            return {};
        const auto total = (int) std::ceil ((to - from) * rate);
        std::vector<float> sound ((size_t) juce::jmax (1, total), 0.0f);
        for (int i = 0; i < (int) track->parts.size(); ++i)
        {
            const auto& p = track->parts[(size_t) i];
            if (! p.isAudio() || p.id == skipPartId)
                continue;
            const auto data = getHeardAudio (p.audioFile);
            if (data == nullptr)
                continue;
            const auto warped = p.isWarped() ? getWarpedSound (p, tempo) : nullptr;
            const auto& heard = warped != nullptr ? *warped : data->sound;
            const auto offset = warped != nullptr ? 0.0 : p.audioOffset;
            const auto clipStart = tempo.secondsAt (p.start);
            const auto channels = heard.getNumChannels();
            if (channels == 0)
                continue;
            for (const auto& [a, b] : heardStretches (*track, i))
            {
                const auto first = juce::jmax (0, (int) std::llround ((tempo.secondsAt (a) - from) * rate));
                const auto last = juce::jmin (total, (int) std::llround ((tempo.secondsAt (b) - from) * rate));
                for (int s = first; s < last; ++s)
                {
                    const auto source = (int) std::llround ((offset + from + s / rate - clipStart) * rate);
                    if (source < 0 || source >= heard.getNumSamples())
                        continue;
                    auto v = 0.0f;
                    for (int c = 0; c < channels; ++c)
                        v += heard.getSample (c, source);
                    sound[(size_t) s] = v / (float) channels;
                }
            }
        }
        return sound;
    }

    std::vector<float> Studio::clipMono (const Part& part, double from, double to, double& rate)
    {
        rate = 0.0;
        const auto data = part.isAudio() ? getHeardAudio (part.audioFile) : nullptr;
        if (data == nullptr || to <= from || data->sound.getNumChannels() == 0)
            return {};
        rate = data->sampleRate;
        const auto clipStart = project.tempoMap().secondsAt (part.start);
        const auto total = (int) std::ceil ((to - from) * rate);
        std::vector<float> sound ((size_t) juce::jmax (1, total), 0.0f);
        const auto channels = data->sound.getNumChannels();
        for (int s = 0; s < total; ++s)
        {
            const auto source = (int) std::llround ((part.audioOffset + from + s / rate - clipStart) * rate);
            if (source < 0 || source >= data->sound.getNumSamples())
                continue;
            auto v = 0.0f;
            for (int c = 0; c < channels; ++c)
                v += data->sound.getSample (c, source);
            sound[(size_t) s] = v / (float) channels;
        }
        return sound;
    }

    bool Studio::alignAudio (const AlignReference& reference, const std::vector<AlignTarget>& targets, const AlignSettings& options,
                             const std::vector<AlignSync>& syncs, const std::vector<juce::Range<double>>& protectedBeats, AlignReport& report)
    {
        report = {};
        const auto* refTrack = project.findTrack (reference.trackId);
        if (refTrack == nullptr || ! refTrack->isAudio() || reference.end <= reference.start)
        {
            report.notes.add ("Choose an audio event (or a range on an audio track) as the reference.");
            return false;
        }
        const auto tempo = project.tempoMap();
        constexpr double edgeRoom = 1.0 / 64.0;   // (no piece shorter than this is split off)
        constexpr double crossfade = 0.01;         // seconds, where a target is split
        auto apartBefore = 0.0, apartAfter = 0.0;
        report.alignedParts.assign (targets.size(), 0);
        for (size_t targetIndex = 0; targetIndex < targets.size(); ++targetIndex)
        {
            const auto& target = targets[targetIndex];
            auto [track, index] = partById (project, target.partId);
            if (track == nullptr)
                continue;
            const auto name = track->parts[(size_t) index].name.isNotEmpty() ? track->parts[(size_t) index].name : track->name;
            if (! track->isAudio() || ! track->parts[(size_t) index].isAudio())
            {
                report.notes.add (name + ": only audio events can be aligned.");
                continue;
            }
            if (track->id == reference.trackId)
            {
                report.notes.add (name + ": on the reference's own track (the target must be on another track).");
                continue;
            }
            if (track->frozen)
            {
                report.notes.add (name + ": its track is frozen.");
                continue;
            }
            const auto original = track->parts[(size_t) index];
            auto start = juce::jmax (original.start, reference.start);
            auto end = juce::jmin (original.end(), reference.end);
            if (target.start >= 0.0)
                start = juce::jmax (start, target.start);
            if (target.end >= 0.0)
                end = juce::jmin (end, target.end);
            if (tempo.secondsAt (end) - tempo.secondsAt (start) < 0.1)
            {
                report.notes.add (name + ": doesn't overlap the reference.");
                continue;
            }
            const auto savedParts = track->parts;   // (put back if it can't be aligned)
            // Its warp replaced (the manual: the audio to align mustn't be warped already).
            if (track->parts[(size_t) index].isWarped())
            {
                auto& p = track->parts[(size_t) index];
                p.audioOffset = p.warp.front().seconds;
                p.warp.clear();
                report.notes.add (name + ": its warp markers were replaced by the alignment.");
            }
            // Split at the overlap's edges; short crossfades there (fades at the cuts replaced).
            auto pieceIndex = index;
            if (start > track->parts[(size_t) index].start + edgeRoom)
            {
                auto second = cutPart (track->parts[(size_t) index], start - track->parts[(size_t) index].start, tempo);
                track->parts[(size_t) index].fadeOut = crossfade;
                second.fadeIn = crossfade;
                track->parts.push_back (second);
                pieceIndex = (int) track->parts.size() - 1;
            }
            if (end < track->parts[(size_t) pieceIndex].end() - edgeRoom)
            {
                auto& piece = track->parts[(size_t) pieceIndex];
                auto rest = cutPart (piece, end - piece.start, tempo);
                piece.fadeOut = crossfade;
                rest.fadeIn = crossfade;
                track->parts.push_back (rest);
            }
            numberParts();
            auto& piece = track->parts[(size_t) pieceIndex];
            const auto from = tempo.secondsAt (piece.start), to = tempo.secondsAt (piece.end());
            const auto margin = 0.05;
            const auto reach = juce::jlimit (AlignSettings::leastShift, AlignSettings::mostShift, options.maxShiftSeconds) + margin;
            AlignSound refSound, tgtSound;
            double refRate = 0.0, tgtRate = 0.0;
            refSound.samples = heardMono (reference.trackId, from - margin, to + margin, refRate);
            refSound.start = from - margin;
            tgtSound.samples = clipMono (piece, from - reach, to + reach, tgtRate);
            tgtSound.start = from - reach;
            if (refRate <= 0.0 || tgtRate <= 0.0 || std::abs (refRate - tgtRate) > 0.5)
            {
                track->parts = savedParts;
                report.notes.add (name + ": its recording or the reference's couldn't be read.");
                continue;
            }
            std::vector<AlignSyncPoint> points;
            std::vector<int> pointIndex;
            for (int s = 0; s < (int) syncs.size(); ++s)
                if (syncs[(size_t) s].partId == original.id || syncs[(size_t) s].partId == piece.id)
                {
                    points.push_back ({ tempo.secondsAt (syncs[(size_t) s].referenceBeat), tempo.secondsAt (syncs[(size_t) s].targetBeat) });
                    pointIndex.push_back (s);
                }
            std::vector<juce::Range<double>> held;
            for (const auto& r : protectedBeats)
                held.push_back ({ tempo.secondsAt (r.getStart()), tempo.secondsAt (r.getEnd()) });
            const auto result = alignSounds (refSound, tgtSound, from, to, refRate, options, points, held);
            for (const auto u : result.unmetSyncPoints)
                report.unmetSyncs.push_back (pointIndex[(size_t) u]);
            if (! result.ok)
            {
                track->parts = savedParts;
                report.notes.add (name + ": " + result.error);
                continue;
            }
            const auto pieceStart = from;
            if (result.shiftOnly)
            {
                if (std::abs (result.shift) >= 0.0002)
                {
                    auto offset = piece.audioOffset + result.shift;
                    if (offset < 0.0)
                    {
                        // (The recording starts later than the clip would: the clip starts later.)
                        const auto newStart = tempo.beatAt (pieceStart - offset);
                        piece.length -= newStart - piece.start;
                        piece.start = newStart;
                        offset = 0.0;
                    }
                    piece.audioOffset = offset;
                }
                ++report.moved;
            }
            else
            {
                piece.warp.clear();
                for (const auto& [t, g] : result.points)
                    piece.warp.push_back ({ tempo.beatAt (t) - piece.start, piece.audioOffset + g - pieceStart });
                tidyWarp (piece);
                ++report.stretched;
            }
            ++report.aligned;
            report.alignedParts[targetIndex] = piece.id;
            apartBefore += result.before;
            apartAfter += result.after;
        }
        if (report.aligned == 0)
            return false;
        report.before = apartBefore / report.aligned;
        report.after = apartAfter / report.aligned;
        changed();
        return true;
    }

    //==========================================================================
    // VariAudio (0.34.0)
    juce::String Studio::variSignature (const juce::String& file) const
    {
        const auto found = project.variAudio.find (file);
        if (found == project.variAudio.end() || found->second.bypass || ! found->second.isEdited())
            return {};
        if (const auto known = variSignatures.find (file); known != variSignatures.end() && known->second.first == found->second)
            return known->second.second;
        juce::String text;
        for (const auto& seg : found->second.segments)
            if (seg.isEdited())
                text << seg.start << ":" << seg.end << ":" << seg.shift << ":" << seg.straighten << ":" << seg.volumeDb << ":"
                     << seg.tiltStart << ":" << seg.tiltEnd << ":" << seg.anchor << ":" << seg.straightenFrom << ":" << seg.straightenTo
                     << ":" << seg.formant << "|";
        variSignatures.insert_or_assign (file, std::pair { found->second, text });
        return text;
    }

    std::shared_ptr<const PitchAnalysis> Studio::getPitchAnalysis (const juce::String& file)
    {
        const auto data = getAudio (file);
        if (data == nullptr)
            return nullptr;
        if (const auto found = pitchCache.find (file); found != pitchCache.end() && found->second.first == data)
            return found->second.second;
        auto analysis = std::make_shared<const PitchAnalysis> (analysePitch (data->sound, data->sampleRate));
        pitchCache[file] = { data, analysis };
        return analysis;
    }

    std::shared_ptr<const Studio::AudioData> Studio::getHeardAudio (const juce::String& file)
    {
        const auto signature = variSignature (file);
        if (signature.isEmpty())
            return getAudio (file);
        const auto data = getAudio (file);
        const auto analysis = getPitchAnalysis (file);
        if (data == nullptr || analysis == nullptr)
            return data;
        if (const auto found = heardCache.find (file); found != heardCache.end()
            && found->second.signature == signature && found->second.source == data)
            return found->second.heard;
        auto heard = std::make_shared<AudioData>();
        heard->sampleRate = data->sampleRate;
        heard->sound = renderVariAudio (data->sound, data->sampleRate, *analysis, project.variAudio[file].segments);
        heard->stereo = data->stereo;
        heard->makePeaks();
        heardCache[file] = { signature, data, heard };
        return heard;
    }

    const Project::VariAudio* Studio::getVariAudio (const juce::String& file) const
    {
        const auto found = project.variAudio.find (file);
        return found != project.variAudio.end() ? &found->second : nullptr;
    }

    bool Studio::editVariAudio (const juce::String& file)
    {
        if (project.variAudio.count (file) > 0)
            return true;
        const auto analysis = getPitchAnalysis (file);
        if (analysis == nullptr)
            return false;
        project.variAudio[file].segments = segmentPitch (*analysis);
        SessionLog::note ("VariAudio: " + file + " analysed, " + juce::String ((int) project.variAudio[file].segments.size()) + " segments");
        changed();
        return true;
    }

    void Studio::setVariSegments (const juce::String& file, std::vector<VariSegment> segments)
    {
        auto found = project.variAudio.find (file);
        if (found == project.variAudio.end())
            return;
        for (auto& seg : segments)
        {
            seg.shift = juce::jlimit (-24.0f, 24.0f, seg.shift);
            seg.straighten = juce::jlimit (0.0f, 1.0f, seg.straighten);
            seg.volumeDb = juce::jlimit (-96.0f, 12.0f, seg.volumeDb);
            seg.tiltStart = juce::jlimit (-24.0f, 24.0f, seg.tiltStart);
            seg.tiltEnd = juce::jlimit (-24.0f, 24.0f, seg.tiltEnd);
            seg.anchor = juce::jlimit (0.02f, 0.98f, seg.anchor);
            seg.straightenFrom = juce::jlimit (0.0f, 1.0f, seg.straightenFrom);
            seg.straightenTo = juce::jlimit (seg.straightenFrom, 1.0f, seg.straightenTo);
            seg.formant = juce::jlimit (-100.0f, 100.0f, seg.formant);
        }
        std::sort (segments.begin(), segments.end(), [] (const VariSegment& a, const VariSegment& b) { return a.start < b.start; });
        if (found->second.segments == segments)
            return;
        found->second.segments = std::move (segments);
        changed();
    }

    void Studio::editVariSegments (const juce::String& file, const std::vector<int>& indices, const std::function<void (VariSegment&)>& edit)
    {
        const auto* vari = getVariAudio (file);
        if (vari == nullptr)
            return;
        auto segments = vari->segments;
        for (const auto i : indices)
            if (juce::isPositiveAndBelow (i, (int) segments.size()))
                edit (segments[(size_t) i]);
        setVariSegments (file, std::move (segments));
    }

    void Studio::setVariAudioBypass (const juce::String& file, bool bypass)
    {
        auto found = project.variAudio.find (file);
        if (found == project.variAudio.end() || found->second.bypass == bypass)
            return;
        found->second.bypass = bypass;
        changed (false);   // (a comparison, not an edit: not an undo step)
    }

    void Studio::removeVariAudio (const juce::String& file)
    {
        if (project.variAudio.erase (file) > 0)
            changed();
    }

    void Studio::auditionRecording (const juce::String& file, double fromSeconds, double toSeconds, bool loop)
    {
        const auto data = getHeardAudio (file);
        if (data == nullptr)
            return;
        const auto from = juce::jlimit (0, data->sound.getNumSamples(), (int) (fromSeconds * data->sampleRate));
        const auto to = juce::jlimit (from, data->sound.getNumSamples(), (int) (toSeconds * data->sampleRate));
        if (to <= from)
            return;
        auto slice = std::make_shared<juce::AudioBuffer<float>> (data->sound.getNumChannels(), to - from);
        for (int c = 0; c < slice->getNumChannels(); ++c)
            slice->copyFrom (c, 0, data->sound, c, from, to - from);
        engine.setPreviewSync (false, 0.0, 0.0);
        engine.setPreview (slice, loop);
    }

    //==========================================================================
    // VariAudio, part two (0.35.0)
    bool Studio::splitVariSegment (const juce::String& file, int index, double seconds)
    {
        const auto* vari = getVariAudio (file);
        const auto analysis = getPitchAnalysis (file);
        if (vari == nullptr || analysis == nullptr)
            return false;
        auto segments = vari->segments;
        if (! splitSegment (segments, index, seconds, *analysis))
            return false;
        setVariSegments (file, std::move (segments));
        return true;
    }

    bool Studio::glueVariSegments (const juce::String& file, int first, int last)
    {
        const auto* vari = getVariAudio (file);
        const auto analysis = getPitchAnalysis (file);
        if (vari == nullptr || analysis == nullptr)
            return false;
        auto segments = vari->segments;
        if (! glueSegments (segments, first, last, *analysis))
            return false;
        setVariSegments (file, std::move (segments));
        return true;
    }

    bool Studio::deleteVariSegments (const juce::String& file, std::vector<int> indices)
    {
        const auto* vari = getVariAudio (file);
        if (vari == nullptr || indices.empty())
            return false;
        auto segments = vari->segments;
        std::sort (indices.rbegin(), indices.rend());
        indices.erase (std::unique (indices.begin(), indices.end()), indices.end());
        for (const auto i : indices)
            if (juce::isPositiveAndBelow (i, (int) segments.size()))
                segments.erase (segments.begin() + i);
        if (segments.size() == vari->segments.size())
            return false;
        setVariSegments (file, std::move (segments));
        return true;
    }

    bool Studio::setVariSegmentEdges (const juce::String& file, int index, double start, double end)
    {
        const auto* vari = getVariAudio (file);
        const auto analysis = getPitchAnalysis (file);
        if (vari == nullptr || analysis == nullptr)
            return false;
        auto segments = vari->segments;
        if (! setSegmentEdges (segments, index, start, end, *analysis))
            return false;
        setVariSegments (file, std::move (segments));
        return true;
    }

    void Studio::resetVariAudio (int partId, std::vector<int> indices, VariReset what)
    {
        const auto found = findPart (partId);
        if (! found)
            return;
        const auto& part = project.findTrack (found->first)->parts[(size_t) found->second];
        const auto file = part.audioFile;
        if (what == VariReset::warp || what == VariReset::all)
        {
            // (The clip's warp markers: those VariAudio's Warp Start/End made, and any others.)
            if (part.isWarped())
            {
                if (what == VariReset::warp)
                {
                    resetWarp ({ partId });
                    return;
                }
                auto& p = project.findTrack (found->first)->parts[(size_t) found->second];
                p.audioOffset = p.warp.front().seconds;
                p.warp.clear();
            }
            if (what == VariReset::warp)
                return;
        }
        const auto* vari = getVariAudio (file);
        if (vari == nullptr)
        {
            changed();
            return;
        }
        auto segments = vari->segments;
        if (indices.empty())
        {
            indices.resize (segments.size());
            std::iota (indices.begin(), indices.end(), 0);
        }
        for (const auto i : indices)
        {
            if (! juce::isPositiveAndBelow (i, (int) segments.size()))
                continue;
            auto& s = segments[(size_t) i];
            if (what == VariReset::pitch || what == VariReset::all)
                s.shift = 0.0f;
            if (what == VariReset::curve || what == VariReset::all)
            {
                s.straighten = s.tiltStart = s.tiltEnd = 0.0f;
                s.anchor = 0.5f;
                s.straightenFrom = 0.0f;
                s.straightenTo = 1.0f;
            }
            if (what == VariReset::formant || what == VariReset::all)
                s.formant = 0.0f;
            if (what == VariReset::volume || what == VariReset::all)
                s.volumeDb = 0.0f;
        }
        if (segments == vari->segments)
        {
            if (what == VariReset::all)
                changed();   // (the warp reset above)
            return;
        }
        setVariSegments (file, std::move (segments));
    }

    bool Studio::reanalyseVariAudio (const juce::String& file)
    {
        const auto analysis = getPitchAnalysis (file);
        if (analysis == nullptr)
            return false;
        auto& vari = project.variAudio[file];
        vari.segments = segmentPitch (*analysis);
        vari.bypass = false;
        changed();
        return true;
    }

    bool Studio::warpVariSegment (int partId, int index, bool atStart, double songBeat)
    {
        const auto found = findPart (partId);
        if (! found)
            return false;
        const auto& original = project.findTrack (found->first)->parts[(size_t) found->second];
        const auto* vari = getVariAudio (original.audioFile);
        if (vari == nullptr || ! juce::isPositiveAndBelow (index, (int) vari->segments.size()))
            return false;
        const auto& segs = vari->segments;
        const auto tempo = project.tempoMap();
        auto part = original;
        ensureWarp (part, tempo);
        if (part.warp.size() < 2)
            return false;
        // The edge moved and the two around it held: the segment and its neighbour (or the clip's
        // edge) stretch between them (the manual: "the segment and the adjacent segments").
        const auto& seg = segs[(size_t) index];
        const auto edge = atStart ? seg.start : seg.end;
        const auto first = part.warp.front().seconds, last = part.warp.back().seconds;
        const auto left = juce::jlimit (first, last, atStart ? (index > 0 ? segs[(size_t) index - 1].start : first) : seg.start);
        const auto right = juce::jlimit (first, last, atStart ? seg.end : (index + 1 < (int) segs.size() ? segs[(size_t) index + 1].end : last));
        if (edge <= left + 0.005 || edge >= right - 0.005)
            return false;
        const auto pin = [&] (double seconds)
        {
            for (size_t i = 0; i < part.warp.size(); ++i)
                if (std::abs (part.warp[i].seconds - seconds) < 0.005)
                    return;
            part.warp.push_back ({ warpBeatAt (part, seconds, tempo), seconds });
            std::sort (part.warp.begin(), part.warp.end(), [] (const WarpMarker& a, const WarpMarker& b) { return a.beat < b.beat; });
        };
        pin (left);
        pin (right);
        pin (edge);
        auto at = -1;
        for (size_t i = 0; i < part.warp.size(); ++i)
            if (std::abs (part.warp[i].seconds - edge) < 0.005)
                at = (int) i;
        if (at <= 0 || at + 1 >= (int) part.warp.size())
            return false;
        constexpr double gap = 1.0 / 64.0;
        const auto low = part.warp[(size_t) at - 1].beat + gap, high = part.warp[(size_t) at + 1].beat - gap;
        if (high <= low)
            return false;
        part.warp[(size_t) at].beat = juce::jlimit (low, high, songBeat - part.start);
        return editWarp (partId, [&] (Part& p) { p.warp = part.warp; p.audioOffset = part.audioOffset; return true; });
    }

    int Studio::extractMidi (int partId, const ExtractMidiOptions& options, ExtractDestination destination, juce::String& error)
    {
        const auto found = findPart (partId);
        if (! found)
            return 0;
        const auto clip = project.findTrack (found->first)->parts[(size_t) found->second];
        const auto clipTrackName = project.findTrack (found->first)->name;
        const auto* vari = getVariAudio (clip.audioFile);
        const auto analysis = getPitchAnalysis (clip.audioFile);
        if (vari == nullptr || analysis == nullptr || vari->segments.empty())
        {
            error = "The clip has no VariAudio segments yet: click EDIT VARIAUDIO first.";
            return 0;
        }
        const auto extracted = pr7s::extractMidi (vari->segments, *analysis, options);
        const auto tempo = project.tempoMap();
        const auto beatOf = [&] (double seconds) { return warpBeatAt (clip, seconds, tempo); };   // (from the clip's start)

        // Where it goes.
        auto trackId = 0;
        if (destination == ExtractDestination::selectedTrack)
        {
            for (const auto id : getSelectedTracks())
                if (canTakeMidi (id))
                {
                    trackId = id;
                    break;
                }
            if (trackId == 0)
            {
                error = "No instrument track is selected: select one first, or choose a new track.";
                return 0;
            }
        }
        else
        {
            trackId = addTrack ({});
            if (trackId == 0)
            {
                error = "A new instrument track couldn't be made.";
                return 0;
            }
            renameTrack (trackId, clipTrackName + " MIDI");
        }
        auto* track = project.findTrack (trackId);
        // An extraction from before, from the same clip's recording, on that track: replaced (the manual's).
        const auto name = "Extracted: " + (clip.name.isNotEmpty() ? clip.name : clip.audioFile);
        track->parts.erase (std::remove_if (track->parts.begin(), track->parts.end(), [&] (const Part& p) { return p.name == name && ! p.isAudio(); }),
                            track->parts.end());
        Part part;
        part.start = clip.start;
        part.length = clip.length;
        part.name = name;
        for (const auto& n : extracted.notes)
        {
            const auto a = beatOf (n.start), b = beatOf (n.end);
            if (b <= 0.0 || a >= clip.length)
                continue;   // (outside what the clip plays)
            Note note;
            note.start = juce::jmax (0.0, a);
            note.length = juce::jmax (1.0 / 128.0, juce::jmin (clip.length, b) - note.start);
            note.pitch = n.pitch;
            note.velocity = (float) n.velocity / 127.0f;
            part.notes.push_back (note);
        }
        auto lanes = track->controllerLanes;
        for (const auto& e : extracted.events)
        {
            const auto at = beatOf (e.seconds);
            if (at < 0.0 || at > clip.length)
                continue;
            Controller c;
            c.type = e.bend ? Controller::Type::pitchbend : Controller::Type::cc;
            c.number = e.bend ? 0 : e.number;
            c.start = at;
            c.value = e.value;
            part.controllers.push_back (c);
            const auto code = e.bend ? 128 : e.number;
            if (std::find (lanes.begin(), lanes.end(), code) == lanes.end())
                lanes.push_back (code);
        }
        std::stable_sort (part.controllers.begin(), part.controllers.end(), [] (const Controller& a, const Controller& b) { return a.start < b.start; });
        if (options.pitch != ExtractMidiOptions::Pitch::notes)
            track->pitchbendUp = track->pitchbendDown = juce::jlimit (1, 24, options.bendRange);
        track->controllerLanes = lanes;   // (the lanes of what was made, shown in the MIDI editor)
        track->parts.push_back (part);
        std::sort (track->parts.begin(), track->parts.end(), [] (const Part& a, const Part& b) { return a.start < b.start; });
        changed();
        for (const auto& p : project.findTrack (trackId)->parts)
            if (p.name == name)
                return p.id;
        return 0;
    }

    std::vector<int> Studio::generateHarmonyVoices (int partId, int voices, float vibratoReduction, juce::String& error)
    {
        std::vector<int> made;
        const auto found = findPart (partId);
        if (! found)
            return made;
        const auto sourceTrackId = project.findTrack (found->first)->id;
        const auto clip = project.findTrack (found->first)->parts[(size_t) found->second];
        if (! clip.isAudio() || ! editVariAudio (clip.audioFile))
        {
            error = "The clip's recording can't be read.";
            return made;
        }
        const auto analysis = getPitchAnalysis (clip.audioFile);
        const auto segments = getVariAudio (clip.audioFile)->segments;
        if (analysis == nullptr || segments.empty())
        {
            error = "No notes were found in the clip (a monophonic recording: a voice, a solo instrument).";
            return made;
        }
        const auto source = getAudioFolder().getChildFile (clip.audioFile);
        const auto tempo = project.tempoMap();
        auto after = sourceTrackId;
        for (int voice = 0; voice < juce::jlimit (1, 4, voices); ++voice)
        {
            // Its own copy of the recording (VariAudio's edits are kept by recording).
            const auto copyFile = getAudioFolder().getNonexistentChildFile (source.getFileNameWithoutExtension() + " " + harmonyVoiceName (voice),
                                                                            source.getFileExtension(), false);
            if (! source.copyFileTo (copyFile))
            {
                error = "The recording couldn't be copied for the " + juce::String (harmonyVoiceName (voice)) + " voice.";
                break;
            }
            const auto trackId = duplicateTrack (sourceTrackId);
            if (trackId == 0)
                break;
            auto* track = project.findTrack (trackId);
            track->name = harmonyVoiceName (voice);
            track->solo = track->mute = false;
            auto voicePart = clip;
            voicePart.id = 0;
            voicePart.lane = 0;
            voicePart.order = 0;
            voicePart.audioFile = copyFile.getFileName();
            track->parts = { voicePart };
            track->showLanes = false;
            // Its segments: the original's, each moved to the voice's note.
            auto voiced = segments;
            for (auto& seg : voiced)
            {
                const auto songBeat = clip.start + warpBeatAt (clip, (seg.start + seg.end) * 0.5, tempo);
                std::vector<int> chord;
                auto bass = -1;
                const auto& chords = project.chordTrack.events;
                if (const auto i = project.chordTrack.indexAt (songBeat); i >= 0 && ! chords.empty())
                {
                    for (const auto pc : chords[(size_t) i].chord.pitchClasses())
                        chord.push_back (pc);
                    bass = chords[(size_t) i].chord.bass >= 0 ? chords[(size_t) i].chord.bass : chords[(size_t) i].chord.root;
                }
                const auto target = harmonyPitch ((int) std::lround (seg.pitchNow()), chord, bass, voice);
                seg.shift = (float) target - seg.pitch;
                seg.straighten = juce::jmax (seg.straighten, juce::jlimit (0.0f, 1.0f, vibratoReduction));
            }
            project.variAudio[voicePart.audioFile].segments = voiced;
            // The same recording: the same pitch (not found again).
            if (const auto data = getAudio (voicePart.audioFile))
                pitchCache[voicePart.audioFile] = { data, analysis };
            // In order under the original: Soprano, Alto, Tenor, Bass.
            auto& tracks = project.tracks;
            const auto from = std::find_if (tracks.begin(), tracks.end(), [trackId] (const Track& t) { return t.id == trackId; });
            auto moved = std::move (*from);
            tracks.erase (from);
            const auto to = std::find_if (tracks.begin(), tracks.end(), [after] (const Track& t) { return t.id == after; });
            tracks.insert (to + 1, std::move (moved));
            after = trackId;
            made.push_back (trackId);
        }
        SessionLog::note ("Harmony voices: " + juce::String ((int) made.size()) + " made from " + clip.audioFile);
        changed();
        return made;
    }

    void Studio::setVariAudioReference (int trackId)
    {
        if (project.variAudioReference == trackId)
            return;
        project.variAudioReference = trackId;
        changed (false);
    }

    void Studio::setCatchingNotes (bool on)
    {
        engine.setCatchingNotes (on);
    }

    std::vector<int> Studio::takePlayedNotes()
    {
        return engine.takeCaughtNotes();
    }

    void Studio::forgetAudio (const juce::String& name)
    {
        // (0.34.1) A recording gone or written again: everything made from it goes with it.
        audioCache.erase (name);
        hitpointCache.erase (name);
        pitchCache.erase (name);
        heardCache.erase (name);
        warpedVariSignatures.erase (name);
        warpRenderer.clear();
    }

    void Studio::forgetAllAudio()
    {
        // (0.34.1) A new song or sample rate: every recording and what was made from them (they
        // stayed until the studio closed, the pitch and heard copies holding whole recordings).
        audioCache.clear();
        hitpointCache.clear();
        pitchCache.clear();
        heardCache.clear();
        variSignatures.clear();
        warpedVariSignatures.clear();
        warpRenderer.clear();
    }

    void Studio::mixOnly (bool onlyTheMix)
    {
        mixOnlyAsked = true;
        mixOnlyBroken = mixOnlyBroken || ! onlyTheMix;
    }

    void Studio::announce()
    {
        // (0.34.3) Every change counts, but a fader, pan or send level moved (nothing written, nothing
        // else changed with it): the views skip their full rebuild then, their timers following the mix.
        if (! mixOnlyAsked || mixOnlyBroken)
            ++changeVersion;
        mixOnlyAsked = mixOnlyBroken = false;
        sendChangeMessage();
    }
    //==========================================================================
    // The SECTIONS row (0.90.0)
    std::vector<SectionPlace> Studio::getSongSectionPlaces() const
    {
        return sectionPlaces (project.songSections, project.signatures());
    }

    std::optional<SectionPlace> Studio::getSongSectionPlace (int index) const
    {
        const auto places = getSongSectionPlaces();
        if (! juce::isPositiveAndBelow (index, (int) places.size()))
            return std::nullopt;
        return places[(size_t) index];
    }

    void Studio::addSongSectionTrack()
    {
        if (project.songSections.shown)
            return;
        project.songSections.shown = true;
        changed();
    }

    void Studio::removeSongSectionTrack()
    {
        if (! project.songSections.shown)
            return;
        project.songSections.shown = false;
        changed();
    }

    namespace
    {
        juce::String sectionNameOrVerse (const juce::String& name)
        {
            const auto base = sectionBaseOf (name);
            return base.isNotEmpty() ? base : juce::String ("Verse");
        }
    }

    int Studio::appendSongSection (const juce::String& name, int bars)
    {
        auto& sections = project.songSections;
        SongSection b;
        b.id = sections.nextId();
        b.name = sectionNameOrVerse (name);
        b.bars = juce::jlimit (leastSectionBars, mostSectionBars, bars);
        sections.blocks.push_back (b);
        sections.shown = true;
        changed();
        return (int) sections.blocks.size() - 1;
    }

    bool Studio::insertSongSection (int index, const juce::String& name, int bars)
    {
        const auto& blocks = project.songSections.blocks;
        index = juce::jlimit (0, (int) blocks.size(), index);
        std::vector<SectionMove> moves;
        for (int i = 0; i < (int) blocks.size(); ++i)
        {
            if (i == index)
            {
                SongSection b;
                b.id = project.songSections.nextId();
                b.name = sectionNameOrVerse (name);
                b.bars = juce::jlimit (leastSectionBars, mostSectionBars, bars);
                moves.push_back ({ -1, b });
            }
            moves.push_back ({ i, blocks[(size_t) i] });
        }
        if (index == (int) blocks.size())
        {
            SongSection b;
            b.id = project.songSections.nextId();
            b.name = sectionNameOrVerse (name);
            b.bars = juce::jlimit (leastSectionBars, mostSectionBars, bars);
            moves.push_back ({ -1, b });
        }
        return relayoutSections (moves);
    }

    bool Studio::moveSongSection (int from, int to)
    {
        const auto& blocks = project.songSections.blocks;
        if (! juce::isPositiveAndBelow (from, (int) blocks.size()))
            return false;
        to = juce::jlimit (0, (int) blocks.size() - 1, to);
        if (to == from)
            return true;
        std::vector<SectionMove> moves;
        for (int i = 0; i < (int) blocks.size(); ++i)
            moves.push_back ({ i, blocks[(size_t) i] });
        const auto moved = moves[(size_t) from];
        moves.erase (moves.begin() + from);
        moves.insert (moves.begin() + to, moved);
        return relayoutSections (moves);
    }

    bool Studio::setSongSectionBars (int index, int bars)
    {
        const auto& blocks = project.songSections.blocks;
        if (! juce::isPositiveAndBelow (index, (int) blocks.size()))
            return false;
        bars = juce::jlimit (leastSectionBars, mostSectionBars, bars);
        if (blocks[(size_t) index].bars == bars)
            return true;
        std::vector<SectionMove> moves;
        for (int i = 0; i < (int) blocks.size(); ++i)
            moves.push_back ({ i, blocks[(size_t) i] });
        moves[(size_t) index].block.bars = bars;
        return relayoutSections (moves);
    }

    bool Studio::renameSongSection (int index, const juce::String& name)
    {
        auto& blocks = project.songSections.blocks;
        if (! juce::isPositiveAndBelow (index, (int) blocks.size()) || sectionBaseOf (name).isEmpty())
            return false;
        if (blocks[(size_t) index].name == sectionBaseOf (name))
            return true;
        blocks[(size_t) index].name = sectionBaseOf (name);
        changed();
        return true;
    }

    bool Studio::duplicateSongSection (int index)
    {
        const auto& blocks = project.songSections.blocks;
        if (! juce::isPositiveAndBelow (index, (int) blocks.size()))
            return false;
        std::vector<SectionMove> moves;
        for (int i = 0; i < (int) blocks.size(); ++i)
        {
            moves.push_back ({ i, blocks[(size_t) i] });
            if (i == index)
            {
                auto copy = blocks[(size_t) i];
                copy.id = project.songSections.nextId();
                moves.push_back ({ i, copy });
            }
        }
        return relayoutSections (moves);
    }

    bool Studio::removeSongSection (int index)
    {
        const auto& blocks = project.songSections.blocks;
        if (! juce::isPositiveAndBelow (index, (int) blocks.size()))
            return false;
        std::vector<SectionMove> moves;
        for (int i = 0; i < (int) blocks.size(); ++i)
            if (i != index)
                moves.push_back ({ i, blocks[(size_t) i] });
        return relayoutSections (moves);
    }

    namespace
    {
        // A stretch of the old song carried to a new place: [from, to) moves by `delta`.
        struct Carry { double from = 0.0, to = 0.0, delta = 0.0; };

        // The points of a curve carried window by window: each window's points inside it moved, with a point at
        // its start and just before its end holding the values the curve had there (so a block keeps its own
        // levels wherever it lands); points in no window are dropped.
        std::vector<AutomationPoint> carryPoints (const std::vector<AutomationPoint>& points, const std::vector<Carry>& carries)
        {
            if (points.empty())
                return points;
            AutomationLane lane;
            lane.points = points;
            std::vector<AutomationPoint> out;
            constexpr double just = 1.0 / 960.0;
            for (const auto& c : carries)
            {
                if (c.to <= c.from)
                    continue;
                if (const auto v = lane.valueAt (c.from))
                    out.push_back ({ c.from + c.delta, *v, 0.0f, false });
                for (const auto& p : points)
                    if (p.beat > c.from + edge && p.beat < c.to - edge)
                    {
                        auto q = p;
                        q.beat += c.delta;
                        out.push_back (q);
                    }
                if (std::isfinite (c.to) && c.to - c.from > 2.0 * just)
                    if (const auto v = lane.valueAt (c.to - just))
                        out.push_back ({ c.to - just + c.delta, *v, 0.0f, false });
            }
            std::stable_sort (out.begin(), out.end(), [] (const AutomationPoint& a, const AutomationPoint& b) { return a.beat < b.beat; });
            return out;
        }
    }

    bool Studio::relayoutSections (const std::vector<SectionMove>& moves)
    {
        auto& sections = project.songSections;
        const auto oldBlocks = sections.blocks;
        const auto oldMap = project.signatures();
        const auto oldPlaces = sectionPlaces (sections, oldMap);
        const auto oldEndBar = sections.totalBars();
        const auto oldEnd = oldMap.beatOfBar (oldEndBar);
        for (const auto& m : moves)
            if (m.from >= (int) oldBlocks.size())
                return false;

        // 1. The signatures, bar by bar: each carried block keeps the signature it started with and the changes
        //    inside what's kept of it; what follows the row keeps its own (so every block keeps its beats).
        {
            std::vector<SignatureEvent> events;   // bar 0 included
            const auto sigAt = [&oldMap] (int bar) { return oldMap.segmentOfBar (bar).event; };
            const auto& oldChanges = project.signatureChanges;
            auto bar = 0;
            for (const auto& m : moves)
            {
                if (m.from >= 0)
                {
                    const auto& p = oldPlaces[(size_t) m.from];
                    const auto kept = juce::jmin (m.block.bars, p.bars);
                    auto start = sigAt (p.firstBar);
                    start.bar = bar;
                    events.push_back (start);
                    for (const auto& c : oldChanges)
                        if (c.bar > p.firstBar && c.bar < p.firstBar + kept)
                        {
                            auto e = c;
                            e.bar = c.bar - p.firstBar + bar;
                            events.push_back (e);
                        }
                }
                bar += m.block.bars;
            }
            auto after = sigAt (oldEndBar);
            after.bar = bar;
            events.push_back (after);
            for (const auto& c : oldChanges)
                if (c.bar > oldEndBar)
                {
                    auto e = c;
                    e.bar = c.bar - oldEndBar + bar;
                    events.push_back (e);
                }
            // Events before any block keep bar 0's signature (a row starting with a new block).
            if (std::none_of (events.begin(), events.end(), [] (const SignatureEvent& e) { return e.bar == 0; }))
                events.insert (events.begin(), sigAt (0));
            std::stable_sort (events.begin(), events.end(), [] (const SignatureEvent& a, const SignatureEvent& b) { return a.bar < b.bar; });
            std::vector<SignatureEvent> oneEach;   // (the last given for a bar wins)
            for (const auto& e : events)
                if (! oneEach.empty() && oneEach.back().bar == e.bar)
                    oneEach.back() = e;
                else
                    oneEach.push_back (e);
            project.numerator = oneEach.front().numerator;
            project.denominator = oneEach.front().denominator;
            std::vector<SignatureEvent> changes (oneEach.begin() + 1, oneEach.end());
            project.signatureChanges = tidySignatures (project.numerator, project.denominator, changes);
        }

        // 2. The new places, and what each window of the old song carries to.
        SongSections laid;
        laid.shown = true;
        for (const auto& m : moves)
            laid.blocks.push_back (m.block);
        const auto newMap = project.signatures();
        const auto newPlaces = sectionPlaces (laid, newMap);
        const auto newEnd = newMap.beatOfBar (laid.totalBars());
        std::vector<Carry> carries;
        for (size_t i = 0; i < moves.size(); ++i)
            if (const auto from = moves[i].from; from >= 0)
            {
                const auto& p = oldPlaces[(size_t) from];
                const auto kept = juce::jmin (moves[i].block.bars, p.bars);
                const auto keptEnd = oldMap.beatOfBar (p.firstBar + kept);
                carries.push_back ({ p.start, keptEnd, newPlaces[i].start - p.start });
            }
        carries.push_back ({ oldEnd, std::numeric_limits<double>::infinity(), newEnd - oldEnd });   // what follows the row

        // 3. The tempo: each window starts at the tempo it had; what's inside moves with it.
        if (! project.tempoEvents.empty())
        {
            const auto map = project.tempoMap();
            std::vector<TempoEvent> events;
            for (const auto& c : carries)
            {
                events.push_back ({ c.from + c.delta, map.bpmAt (c.from), false });
                for (const auto& e : project.tempoEvents)
                    if (e.beat > c.from + edge && e.beat < c.to - edge)
                        events.push_back ({ e.beat + c.delta, e.bpm, e.ramp });
            }
            std::stable_sort (events.begin(), events.end(), [] (const TempoEvent& a, const TempoEvent& b) { return a.beat < b.beat; });
            std::vector<TempoEvent> oneEach;
            for (const auto& e : events)
                if (! oneEach.empty() && std::abs (oneEach.back().beat - e.beat) < edge)
                    oneEach.back() = e;
                else
                    oneEach.push_back (e);
            if (oneEach.empty() || oneEach.front().beat > edge)
                oneEach.insert (oneEach.begin(), { 0.0, map.bpmAt (0.0), false });
            oneEach.front().beat = 0.0;
            project.tempoEvents = oneEach;
        }
        const auto tempo = project.tempoMap();

        // 4. Every track's parts: cut at the windows' edges, each piece carried (a block copied: twice), the rest
        //    dropped. Frozen tracks are left as they are (changed() puts their parts back).
        for (auto& track : project.tracks)
        {
            if (track.frozen || track.parts.empty())
                continue;
            for (const auto& c : carries)
            {
                splitTrackAt (track, c.from, tempo);
                if (std::isfinite (c.to))
                    splitTrackAt (track, c.to, tempo);
            }
            for (const auto& p : oldPlaces)
                splitTrackAt (track, p.end, tempo);
            std::vector<Part> placed;
            for (const auto& part : track.parts)
            {
                auto first = true;
                for (const auto& c : carries)
                    if (part.start >= c.from - edge && part.end() <= c.to + edge)
                    {
                        auto piece = part;
                        piece.start = juce::jmax (0.0, part.start + c.delta);
                        if (! first)
                            piece.id = 0;
                        first = false;
                        placed.push_back (piece);
                    }
            }
            track.parts = std::move (placed);
        }

        // 5. The chords (each its own length first) and the scales.
        {
            auto& ct = project.chordTrack;
            const auto chords = withLengths (ct, project.beatsPerBar());
            std::vector<ChordEvent> events;
            for (const auto& c : carries)
                for (const auto& e : chords)
                {
                    const auto from = juce::jmax (e.beat, c.from), to = juce::jmin (e.beat + e.length, c.to);
                    if (to - from >= shortestChord)
                        events.push_back ({ from + c.delta, e.chord, to - from });
                }
            if (! ct.autoScales && ! ct.scales.empty())
            {
                std::vector<ScaleEvent> scales;
                for (const auto& c : carries)
                {
                    scales.push_back ({ c.from + c.delta, ct.keyAt (c.from, project.chordPads.key()) });
                    for (const auto& e : ct.scales)
                        if (e.beat > c.from + edge && e.beat < c.to - edge)
                            scales.push_back ({ e.beat + c.delta, e.key });
                }
                std::stable_sort (scales.begin(), scales.end(), [] (const ScaleEvent& a, const ScaleEvent& b) { return a.beat < b.beat; });
                ct.scales = scales;
            }
            ct.events = events;
            ct.sort();
            if (ct.autoScales)
                ct.scales = ct.events.empty() ? std::vector<ScaleEvent> {} : scalesFromChords (ct.events, project.chordPads.key());
        }

        // 6. Markers: each with its start's window (a block copied: its markers copied, with new IDs).
        {
            std::vector<Marker> markers;
            auto& mt = project.markerTrack;
            for (const auto& m : mt.markers)
            {
                auto first = true;
                for (const auto& c : carries)
                    if (m.start >= c.from - edge && m.start < c.to - edge)
                    {
                        auto copy = m;
                        copy.start += c.delta;
                        copy.end = m.cycle ? m.end + c.delta : copy.start;
                        if (! first)
                            copy.id = 0;
                        first = false;
                        markers.push_back (copy);
                    }
            }
            for (auto& m : markers)
                if (m.id == 0)
                {
                    MarkerTrack t;
                    t.markers = markers;
                    m.id = t.nextId (m.cycle);
                }
            mt.markers = markers;
            mt.sort();
        }

        // 7. Automation, every lane (tracks' and the master's), points and trims.
        const auto carryLanes = [&carries] (std::vector<AutomationLane>& lanes)
        {
            for (auto& lane : lanes)
            {
                lane.points = carryPoints (lane.points, carries);
                lane.trim = carryPoints (lane.trim, carries);
            }
        };
        for (auto& track : project.tracks)
            if (! track.frozen)
                carryLanes (track.automation);
        carryLanes (project.masterAutomation);

        sections.blocks = laid.blocks;
        sections.shown = true;
        changed();
        return true;
    }
}
