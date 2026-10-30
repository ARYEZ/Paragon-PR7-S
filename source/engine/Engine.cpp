#include "Engine.h"

namespace pr7s
{
    namespace
    {
        constexpr double clickSeconds = 0.03;   // each tick of the click
        constexpr float clickVolume = 0.3f;
        constexpr double gainRampSeconds = 0.02;

        // Volume and pan into a gain for each side: the pan is a balance
        // control, as on a stereo channel (the centre is the full level on both).
        float levelOf (float db)
        {
            return db <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain (db);
        }

        std::pair<float, float> sideGains (float volumeDb, float pan, bool audible)
        {
            if (! audible)
                return { 0.0f, 0.0f };
            const auto gain = levelOf (volumeDb);
            return { gain * juce::jmin (1.0f, 1.0f - pan), gain * juce::jmin (1.0f, 1.0f + pan) };
        }

        std::pair<float, float> sideGains (const Track& track, bool audible)
        {
            return sideGains (track.volumeDb, track.pan, audible);
        }

        constexpr int automationStep = 32;   // samples between volume, pan and mute automation values

        // The lanes that are played back: R on, the lane not muted, with
        // points, and its control not being held (written).
        template <typename LaneStates>
        void readLanes (const std::vector<AutomationLane>& lanes, bool read, LaneStates& into, bool& mixAutomated,
                        int busId, const std::vector<std::pair<int, AutomationLane>>& held)
        {
            mixAutomated = false;
            const auto isHeld = [&] (const AutomationLane& lane)
            {
                return std::any_of (held.begin(), held.end(), [&] (const auto& h) { return h.first == busId && h.second.sameControl (lane); });
            };
            for (const auto& lane : lanes)
                if (read && ! lane.muted && ! lane.points.empty() && ! isHeld (lane))
                {
                    into.push_back ({ lane, -1.0f });
                    using T = AutomationLane::Target;
                    mixAutomated = mixAutomated || lane.target == T::volume || lane.target == T::pan || lane.target == T::mute;
                }
        }
    }

    Engine::Engine()
    {
        masterInserts.reserve ((size_t) Project::maxInserts);
        masterBuffer.setSize (2, maxBlockSize);
        masterGain.setCurrentAndTargetValue (1.0f);
    }

    Engine::~Engine()
    {
        if (onPluginGoing)
        {
            for (auto& track : tracks)
            {
                onPluginGoing (track->instrument.get());
                for (auto& slot : track->inserts)
                    onPluginGoing (slot.plugin.get());
            }
            for (auto& slot : masterInserts)
                onPluginGoing (slot.plugin.get());
        }
        const juce::ScopedLock sl (lock);
        tracks.clear();
        masterInserts.clear();
    }

    void Engine::prepare (double newSampleRate, int newMaxBlockSize)
    {
        const juce::ScopedLock sl (lock);
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        maxBlockSize = juce::jmax (1, newMaxBlockSize);
        midiInput.reset (sampleRate);
        live.ensureSize (2048);
        deviceBuffer.setSize (2, maxBlockSize);
        silence.assign ((size_t) maxBlockSize, 0.0f);
        effectMidi.ensureSize (16);
        auto masterChannels = 2;
        for (auto& slot : masterInserts)
            if (slot.plugin != nullptr)
            {
                prepareEffect (*slot.plugin);
                masterChannels = juce::jmax (masterChannels, channelsFor (*slot.plugin));
            }
        masterBuffer.setSize (masterChannels, maxBlockSize);
        masterGain.reset (sampleRate, gainRampSeconds);
        masterGain.setCurrentAndTargetValue (settings.masterGain);
        for (auto& track : tracks)
        {
            for (auto& slot : track->inserts)
                if (slot.plugin != nullptr)
                    prepareEffect (*slot.plugin);
            prepareTrack (*track);
        }
    }

    void Engine::setNonRealtime (bool offline)
    {
        const juce::ScopedLock sl (lock);
        auto set = [offline] (juce::AudioPluginInstance* plugin) { if (plugin != nullptr) plugin->setNonRealtime (offline); };
        for (auto& slot : masterInserts)
            set (slot.plugin.get());
        for (auto& track : tracks)
        {
            set (track->instrument.get());
            for (auto& slot : track->inserts)
                set (slot.plugin.get());
        }
    }

    int Engine::channelsFor (const juce::AudioPluginInstance& plugin)
    {
        return juce::jmax (1, plugin.getTotalNumInputChannels(), plugin.getTotalNumOutputChannels());
    }

    int Engine::channelsNeeded (const TrackState& track) const
    {
        auto channels = track.instrumentChannels;
        for (const auto& slot : track.inserts)
            if (slot.plugin != nullptr)
                channels = juce::jmax (channels, channelsFor (*slot.plugin));
        return channels;
    }

    void Engine::prepareEffect (juce::AudioPluginInstance& effect)
    {
        // Stereo in and out if it can; otherwise as it comes (a mono effect
        // gets the two sides mixed, and its one output goes to both).
        juce::AudioProcessor::BusesLayout stereo;
        stereo.inputBuses.add (juce::AudioChannelSet::stereo());
        stereo.outputBuses.add (juce::AudioChannelSet::stereo());
        if (effect.getBusCount (true) > 0 && effect.getBusCount (false) > 0 && effect.checkBusesLayoutSupported (stereo))
            effect.setBusesLayout (stereo);
        effect.setPlayHead (&head);
        effect.setRateAndBufferSizeDetails (sampleRate, maxBlockSize);
        effect.prepareToPlay (sampleRate, maxBlockSize);
    }

    void Engine::prepareTrack (TrackState& track)
    {
        if (track.instrument != nullptr)
        {
            track.instrument->setPlayHead (&head);
            track.instrument->setRateAndBufferSizeDetails (sampleRate, maxBlockSize);
            track.instrument->prepareToPlay (sampleRate, maxBlockSize);
            track.instrumentChannels = juce::jmax (2, track.instrument->getTotalNumInputChannels(), track.instrument->getTotalNumOutputChannels());
        }
        track.buffer.setSize (channelsNeeded (track), maxBlockSize);
        track.midi.ensureSize (4096);
        track.left.reset (sampleRate, gainRampSeconds);
        track.right.reset (sampleRate, gainRampSeconds);
        track.left.setCurrentAndTargetValue (track.gainLeft);
        track.right.setCurrentAndTargetValue (track.gainRight);
    }

    Engine::TrackState* Engine::findTrack (int id) const
    {
        for (const auto& track : tracks)
            if (track->id == id)
                return track.get();
        return nullptr;
    }

    //==========================================================================
    void Engine::setInstrument (int trackId, std::unique_ptr<juce::AudioPluginInstance> instrument)
    {
        // Prepared here, out of the audio's way; then swapped in.
        auto incoming = std::make_unique<TrackState>();
        incoming->id = trackId;
        incoming->instrument = std::move (instrument);
        incoming->inserts.reserve ((size_t) Project::maxInserts);
        prepareTrack (*incoming);
        // A track that has inserts already: room for them in the new buffer too.
        if (const auto* existing = findTrack (trackId))
        {
            auto channels = incoming->instrumentChannels;
            for (const auto& slot : existing->inserts)
                if (slot.plugin != nullptr)
                    channels = juce::jmax (channels, channelsFor (*slot.plugin));
            if (channels > incoming->buffer.getNumChannels())
                incoming->buffer.setSize (channels, maxBlockSize);
        }

        std::unique_ptr<juce::AudioPluginInstance> old;
        {
            const juce::ScopedLock sl (lock);
            if (auto* existing = findTrack (trackId))
            {
                // Keep the notes and mix; send the old instrument away.
                old = std::move (existing->instrument);
                existing->instrument = std::move (incoming->instrument);
                existing->instrumentChannels = incoming->instrumentChannels;
                std::swap (existing->buffer, incoming->buffer);
                existing->held.reset();
            }
            else
            {
                tracks.push_back (std::move (incoming));
            }
        }
        if (old != nullptr && onPluginGoing)
            onPluginGoing (old.get());
        // `old` (and `incoming` if it was emptied) are freed here, outside the lock.
    }

    void Engine::removeTrack (int trackId)
    {
        std::unique_ptr<TrackState> removed;
        {
            const juce::ScopedLock sl (lock);
            for (auto it = tracks.begin(); it != tracks.end(); ++it)
                if ((*it)->id == trackId)
                {
                    removed = std::move (*it);
                    tracks.erase (it);
                    break;
                }
        }
        if (removed != nullptr && onPluginGoing)
        {
            onPluginGoing (removed->instrument.get());
            for (auto& slot : removed->inserts)
                onPluginGoing (slot.plugin.get());
        }
    }

    juce::AudioPluginInstance* Engine::getInstrument (int trackId) const
    {
        const juce::ScopedLock sl (lock);
        auto* track = findTrack (trackId);
        return track != nullptr ? track->instrument.get() : nullptr;
    }

    void Engine::addAudioTrack (int trackId)
    {
        auto incoming = std::make_unique<TrackState>();
        incoming->id = trackId;
        incoming->audio = true;
        incoming->inserts.reserve ((size_t) Project::maxInserts);
        prepareTrack (*incoming);
        const juce::ScopedLock sl (lock);
        if (findTrack (trackId) == nullptr)
            tracks.push_back (std::move (incoming));
    }

    void Engine::setRecorder (int trackId, juce::AudioFormatWriter::ThreadedWriter* writer)
    {
        const juce::ScopedLock sl (lock);
        if (auto* track = findTrack (trackId))
        {
            track->recorder = writer;
        }
        if (writer != nullptr)
            audioRecordStart = -1;
    }

    const float* Engine::inputChannel (int channel, int offset) const noexcept
    {
        if (blockInputs != nullptr && juce::isPositiveAndBelow (channel, blockNumInputs) && blockInputs[channel] != nullptr)
            return blockInputs[channel] + blockInputOffset + offset;
        return silence.data();
    }

    void Engine::playClips (TrackState& track, juce::int64 clock, int offset, int length, double beatsPerSample)
    {
        // Each clip's samples in this stretch of the song, faded at its ends,
        // added to the track's sound.
        auto* left = track.buffer.getWritePointer (0);
        auto* right = track.buffer.getWritePointer (1);
        for (const auto& clip : track.clips)
        {
            const auto startSample = (juce::int64) std::llround (clip.start / beatsPerSample);
            const auto endSample = (juce::int64) std::llround (clip.end / beatsPerSample);
            const auto from = juce::jmax (clock, startSample), to = juce::jmin (clock + length, endSample);
            if (from >= to || clip.sound == nullptr)
                continue;
            const auto& sound = *clip.sound;
            const auto* sourceLeft = sound.getReadPointer (0);
            const auto* sourceRight = sound.getNumChannels() > 1 ? sound.getReadPointer (1) : sourceLeft;
            const auto clipLength = (double) (endSample - startSample);
            for (auto s = from; s < to; ++s)
            {
                const auto into = (double) (s - startSample);
                const auto at = clip.offset + (s - startSample);
                if (at < 0 || at >= sound.getNumSamples())
                    continue;
                auto gain = 1.0;
                if (clip.fadeIn > 0.0 && into < clip.fadeIn)
                    gain *= into / clip.fadeIn;
                if (clip.fadeOut > 0.0 && clipLength - into < clip.fadeOut)
                    gain *= (clipLength - into) / clip.fadeOut;
                const auto i = offset + (int) (s - clock);
                left[i] += (float) (sourceLeft[at] * gain);
                right[i] += (float) (sourceRight[at] * gain);
            }
        }
    }

    void Engine::insertEffect (int busId, int slot, std::unique_ptr<juce::AudioPluginInstance> effect)
    {
        if (effect != nullptr)
            prepareEffect (*effect);

        // A track not here yet (its instrument couldn't be loaded): kept anyway, silent.
        if (busId != masterBus && findTrack (busId) == nullptr)
        {
            auto silent = std::make_unique<TrackState>();
            silent->id = busId;
            silent->inserts.reserve ((size_t) Project::maxInserts);
            prepareTrack (*silent);
            const juce::ScopedLock sl (lock);
            tracks.push_back (std::move (silent));
        }

        // A bigger buffer, if this effect has more channels, made here; swapped in below.
        auto& inserts = busId == masterBus ? masterInserts : findTrack (busId)->inserts;
        auto& buffer = busId == masterBus ? masterBuffer : findTrack (busId)->buffer;
        juce::AudioBuffer<float> bigger;
        if (effect != nullptr && channelsFor (*effect) > buffer.getNumChannels())
            bigger.setSize (channelsFor (*effect), maxBlockSize);

        const juce::ScopedLock sl (lock);
        Slot added;
        added.plugin = std::move (effect);
        inserts.insert (inserts.begin() + juce::jlimit (0, (int) inserts.size(), slot), std::move (added));
        if (bigger.getNumChannels() > 0)
            std::swap (buffer, bigger);
    }

    void Engine::removeEffect (int busId, int slot)
    {
        std::unique_ptr<juce::AudioPluginInstance> removed;
        {
            const juce::ScopedLock sl (lock);
            auto* track = busId == masterBus ? nullptr : findTrack (busId);
            auto* inserts = busId == masterBus ? &masterInserts : track != nullptr ? &track->inserts : nullptr;
            if (inserts == nullptr || ! juce::isPositiveAndBelow (slot, (int) inserts->size()))
                return;
            removed = std::move ((*inserts)[(size_t) slot].plugin);
            inserts->erase (inserts->begin() + slot);
        }
        if (removed != nullptr && onPluginGoing)
            onPluginGoing (removed.get());
        // `removed` is deleted here, outside the lock.
    }

    juce::AudioPluginInstance* Engine::getEffect (int busId, int slot) const
    {
        const juce::ScopedLock sl (lock);
        const auto* track = busId == masterBus ? nullptr : findTrack (busId);
        const auto* inserts = busId == masterBus ? &masterInserts : track != nullptr ? &track->inserts : nullptr;
        return inserts != nullptr && juce::isPositiveAndBelow (slot, (int) inserts->size()) ? (*inserts)[(size_t) slot].plugin.get() : nullptr;
    }

    int Engine::getNumEffects (int busId) const
    {
        const juce::ScopedLock sl (lock);
        const auto* track = busId == masterBus ? nullptr : findTrack (busId);
        return busId == masterBus ? (int) masterInserts.size() : track != nullptr ? (int) track->inserts.size() : 0;
    }

    void Engine::update (const Project& project)
    {
        // Each track's notes as note on/off events by beat, built here
        // (allocating) and swapped in under the lock.
        auto anySolo = false;
        for (const auto& track : project.tracks)
            anySolo = anySolo || track.solo;

        struct Prepared { int id; std::vector<Event> events; std::pair<float, float> gains; std::vector<bool> bypass; std::vector<Clip> clips;
                          std::vector<LaneState> automation; bool mixAutomated = false; };
        std::vector<Prepared> prepared;
        for (const auto& track : project.tracks)
        {
            Prepared p { track.id, {}, sideGains (track, ! track.mute && (! anySolo || track.solo)), {}, {}, {}, false };
            for (const auto& insert : track.inserts)
                p.bypass.push_back (insert.bypass);
            readLanes (track.automation, track.automationRead, p.automation, p.mixAutomated, track.id, held);
            if (track.isAudio())
            {
                for (const auto& part : track.parts)
                    if (part.isAudio() && audioSource)
                        p.clips.push_back ({ part.start, part.end(), audioSource (part.audioFile),
                                             (juce::int64) std::llround (part.audioOffset * sampleRate),
                                             part.fadeIn * sampleRate, part.fadeOut * sampleRate });
                prepared.push_back (std::move (p));
                continue;
            }
            for (const auto& part : track.parts)
                for (const auto& note : part.notes)
                {
                    const auto on = part.start + note.start;
                    if (note.start < 0.0 || note.start >= part.length)
                        continue;   // outside the part: not heard (as in a DAW)
                    const auto off = part.start + juce::jmin (note.start + note.length, part.length);
                    const auto velocity = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (note.velocity * 127.0f));
                    p.events.push_back ({ on, 0x90, (juce::uint8) note.pitch, velocity });
                    p.events.push_back ({ off, 0x80, (juce::uint8) note.pitch, 0 });
                }
            // By beat; at the same beat a note off comes first, so a repeated note restarts.
            std::sort (p.events.begin(), p.events.end(), [] (const Event& a, const Event& b)
            {
                if (a.beat < b.beat || b.beat < a.beat)
                    return a.beat < b.beat;
                return (a.status == 0x80) > (b.status == 0x80);
            });
            prepared.push_back (std::move (p));
        }

        const auto masterLevel = levelOf (project.masterVolumeDb);
        std::vector<LaneState> masterLanes;
        bool masterLanesMix = false;
        readLanes (project.masterAutomation, project.masterRead, masterLanes, masterLanesMix, masterBus, held);

        const juce::ScopedLock sl (lock);
        settings = { project.bpm, project.numerator, project.denominator, project.loopOn,
                     project.loopStart, juce::jmax (project.loopStart + 0.0625, project.loopEnd), project.click, masterLevel,
                     project.masterVolumeDb };
        std::swap (masterAutomation, masterLanes);
        masterPreFader = project.masterPreFaderSlots;
        masterAutomated = masterLanesMix;
        for (auto& p : prepared)
            if (auto* track = findTrack (p.id))
            {
                std::swap (track->events, p.events);   // the old list goes out with `prepared`
                std::swap (track->clips, p.clips);      // likewise the old clips
                if (const auto* t = project.findTrack (p.id))
                {
                    track->input = t->input;
                    track->stereoInput = t->stereoInput;
                    track->monitor = t->monitor;
                    track->volumeDb = t->volumeDb;
                    track->pan = t->pan;
                    track->mute = t->mute;
                    track->soloedOut = anySolo && ! t->solo;
                    track->preFader = t->preFaderSlots;
                }
                std::swap (track->automation, p.automation);
                track->mixAutomated = p.mixAutomated;
                track->gainLeft = p.gains.first;
                track->gainRight = p.gains.second;
                for (size_t i = 0; i < juce::jmin (p.bypass.size(), track->inserts.size()); ++i)
                    track->inserts[i].bypass = p.bypass[i];
            }
        for (size_t i = 0; i < juce::jmin (project.masterInserts.size(), masterInserts.size()); ++i)
            masterInserts[i].bypass = project.masterInserts[i].bypass;
    }

    void Engine::setHeld (int busId, const AutomationLane& control, bool isHeld)
    {
        held.erase (std::remove_if (held.begin(), held.end(), [&] (const auto& h) { return h.first == busId && h.second.sameControl (control); }),
                    held.end());
        if (isHeld)
            held.push_back ({ busId, control });
    }

    //==========================================================================
    void Engine::play()
    {
        playing = true;
    }

    void Engine::stop()
    {
        recording = false;
        playing = false;
    }

    void Engine::record()
    {
        recording = true;
        playing = true;
    }

    void Engine::setPosition (double beats)
    {
        beats = juce::jmax (0.0, beats);
        if (playing.load())
        {
            jumpTo = beats;   // the audio thread jumps, stopping the notes sounding
        }
        else
        {
            position = beats;
            rebase = true;    // from here, even if play is pressed before the audio thread has seen the stop
        }
    }

    void Engine::takeRecorded (std::vector<RecordedEvent>& into)
    {
        const auto scope = recordedFifo.read (recordedFifo.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i)
            into.push_back (recordedEvents[(size_t) (scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i)
            into.push_back (recordedEvents[(size_t) (scope.startIndex2 + i)]);
    }

    float Engine::takeTrackPeak (int trackId, int reader)
    {
        const juce::ScopedLock sl (lock);
        auto* track = findTrack (trackId);
        return track != nullptr ? track->meters[(size_t) juce::jlimit (0, 1, reader)].takePeak() : 0.0f;
    }

    void Engine::runInserts (std::vector<Slot>& slots, juce::AudioBuffer<float>& buffer, int numSamples, int fromSlot, int toSlot)
    {
        // In order, each on the sound in place. (Audio thread: no allocation.)
        for (int i = juce::jmax (0, fromSlot); i < juce::jmin (toSlot, (int) slots.size()); ++i)
        {
            auto& slot = slots[(size_t) i];
            if (slot.plugin == nullptr || slot.bypass)
                continue;
            auto& effect = *slot.plugin;
            const auto ins = effect.getTotalNumInputChannels(), outs = effect.getTotalNumOutputChannels();
            const auto channels = juce::jmin (buffer.getNumChannels(), channelsFor (effect));
            // (On the samples themselves, not through the buffer's own
            // methods, which skip work on a buffer they think is silent.)
            if (ins == 1 && buffer.getNumChannels() > 1)
            {
                // Mono in: both sides, mixed.
                auto* left = buffer.getWritePointer (0);
                juce::FloatVectorOperations::multiply (left, 0.5f, numSamples);
                juce::FloatVectorOperations::addWithMultiply (left, buffer.getReadPointer (1), 0.5f, numSamples);
            }
            for (int channel = ins; channel < channels; ++channel)
                juce::FloatVectorOperations::clear (buffer.getWritePointer (channel), numSamples);
            juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), channels, numSamples);
            effectMidi.clear();
            effect.processBlock (view, effectMidi);
            if (outs == 1 && buffer.getNumChannels() > 1)   // mono out: to both sides
                juce::FloatVectorOperations::copy (buffer.getWritePointer (1), buffer.getReadPointer (0), numSamples);
        }
    }

    //==========================================================================
    void Engine::schedule (TrackState& track, juce::int64 fromSample, juce::int64 toSample, int sampleOffset, double beatsPerSample)
    {
        // Every event whose sample (its beat at this tempo, rounded to the
        // nearest sample) is from `fromSample` up to (not including) `toSample`.
        const auto first = std::lower_bound (track.events.begin(), track.events.end(), ((double) fromSample - 0.5) * beatsPerSample,
                                             [] (const Event& e, double beat) { return e.beat < beat; });
        for (auto it = first; it != track.events.end(); ++it)
        {
            const auto at = (juce::int64) std::llround (it->beat / beatsPerSample);
            if (at < fromSample)
                continue;
            if (at >= toSample)
                break;
            if (it->status == 0x90)
                track.held.set (it->data1);
            else
                track.held.reset (it->data1);
            const juce::uint8 bytes[] = { it->status, it->data1, it->data2 };
            track.midi.addEvent (bytes, 3, sampleOffset + (int) (at - fromSample));
        }
    }

    void Engine::stopHeldNotes (TrackState& track, int sampleOffset)
    {
        for (int note = 0; note < 128; ++note)
            if (track.held.test ((size_t) note))
                track.midi.addEvent (juce::MidiMessage::noteOff (1, note), sampleOffset);
        track.held.reset();
    }

    void Engine::render (juce::AudioBuffer<float>& output, int numSamples, const juce::AudioBuffer<float>* input)
    {
        // Larger than the device promised: in pieces.
        blockInputs = input != nullptr ? input->getArrayOfReadPointers() : nullptr;
        blockNumInputs = input != nullptr ? input->getNumChannels() : 0;
        for (int done = 0; done < numSamples;)
        {
            const auto length = juce::jmin (maxBlockSize, numSamples - done);
            juce::AudioBuffer<float> piece (output.getArrayOfWritePointers(), output.getNumChannels(), done, length);
            blockInputOffset = done;
            renderBlock (piece, length);
            done += length;
        }
        blockInputs = nullptr;
        blockNumInputs = 0;
    }

    void Engine::renderBlock (juce::AudioBuffer<float>& output, int numSamples)
    {
        output.clear();
        const juce::ScopedTryLock sl (lock);
        if (! sl.isLocked())
            return;   // the screen is swapping something in: one silent block

        // What's being played live this block: devices and the on-screen keyboard.
        live.clear();
        midiInput.removeNextBlockOfMessages (live, numSamples);
        keyboardState.processNextMidiBuffer (live, 0, numSamples, true);

        for (auto& track : tracks)
            track->midi.clear();

        const auto nowPlaying = playing.load();
        const auto beatsPerSample = settings.bpm / 60.0 / sampleRate;
        const auto toSamples = [beatsPerSample] (double beats) { return (juce::int64) std::llround (beats / beatsPerSample); };

        // The clock: re-based from the beat position when stopped, on
        // starting, and when the tempo changes; on a jump, from the new place
        // (the notes sounding stop). Stopping stops them too.
        if (! nowPlaying || ! wasPlaying || rebase.exchange (false) || std::abs (clockBeatsPerSample - beatsPerSample) > 1.0e-15)
            sampleClock = toSamples (position.load());
        clockBeatsPerSample = beatsPerSample;
        if (const auto jump = jumpTo.exchange (-1.0); jump >= 0.0 && nowPlaying)
        {
            sampleClock = toSamples (jump);
            for (auto& track : tracks)
                stopHeldNotes (*track, 0);
        }
        if (wasPlaying && ! nowPlaying)
            for (auto& track : tracks)
                stopHeldNotes (*track, 0);
        wasPlaying = nowPlaying;

        auto clock = sampleClock;
        const auto beat = (double) clock * beatsPerSample;
        head.info.setBpm (settings.bpm);
        head.info.setTimeSignature (juce::AudioPlayHead::TimeSignature { settings.numerator, settings.denominator });
        head.info.setPpqPosition (beat);
        head.info.setIsPlaying (nowPlaying);
        head.info.setIsRecording (recording.load());
        head.info.setIsLooping (settings.loopOn);
        head.info.setLoopPoints (juce::AudioPlayHead::LoopPoints { settings.loopStart, settings.loopEnd });
        head.info.setTimeInSamples (clock);
        head.info.setTimeInSeconds ((double) clock / sampleRate);

        numTicks = 0;
        numSegments = 0;
        if (nowPlaying)
        {
            // The block in segments: up to the cycle's end, then back to its start.
            const auto isRecording = recording.load();
            const auto beatUnit = 4.0 / settings.denominator;   // the click's beat (a quarter in 4/4, an eighth in 6/8)
            const auto unitsPerBar = settings.numerator;
            const auto loopStart = toSamples (settings.loopStart), loopEnd = toSamples (settings.loopEnd);
            for (int done = 0; done < numSamples;)
            {
                // Exactly at the cycle's end (a block ended there): back to its start.
                if (settings.loopOn && clock == loopEnd)
                    clock = loopStart;
                auto length = numSamples - done;
                auto wraps = false;
                if (settings.loopOn && clock < loopEnd && clock + length >= loopEnd)
                {
                    length = (int) (loopEnd - clock);
                    wraps = true;
                }
                const auto end = clock + length;
                if (numSegments < (int) segments.size())
                    segments[(size_t) numSegments++] = { clock, done, length };

                for (auto& track : tracks)
                    schedule (*track, clock, end, done, beatsPerSample);

                // Recording audio: the armed audio track's input, to its file
                // (every pass of a cycle, one after another; the studio keeps the last).
                if (isRecording)
                    for (auto& track : tracks)
                        if (track->recorder != nullptr)
                        {
                            const float* channels[2] = { inputChannel (track->input, done),
                                                         inputChannel (track->stereoInput ? track->input + 1 : track->input, done) };
                            if (audioRecordStart.load() < 0)
                                audioRecordStart = clock;
                            track->recorder->write (channels, length);
                        }

                if (settings.click)
                    for (auto unit = (juce::int64) std::floor ((double) clock * beatsPerSample / beatUnit); numTicks < (int) ticks.size(); ++unit)
                    {
                        const auto at = toSamples ((double) unit * beatUnit);
                        if (at < clock)
                            continue;
                        if (at >= end)
                            break;
                        ticks[(size_t) numTicks++] = { done + (int) (at - clock), unit % unitsPerBar == 0 };
                    }

                // Recording: each note played, at its beat in the song.
                if (isRecording)
                    for (const auto metadata : live)
                        if (metadata.samplePosition >= done && metadata.samplePosition < done + length)
                        {
                            const auto message = metadata.getMessage();
                            if (! message.isNoteOnOrOff())
                                continue;
                            const auto scope = recordedFifo.write (1);
                            if (scope.blockSize1 + scope.blockSize2 == 0)
                                continue;   // full: the message thread hasn't kept up
                            const auto at = scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2;
                            const auto* raw = message.getRawData();
                            recordedEvents[(size_t) at] = { (double) (clock + metadata.samplePosition - done) * beatsPerSample, raw[0],
                                                            raw[1], message.getRawDataSize() > 2 ? raw[2] : (juce::uint8) 0 };
                        }

                done += length;
                if (wraps)
                {
                    ++cycleCount;
                    for (auto& track : tracks)
                        stopHeldNotes (*track, juce::jmin (done, numSamples - 1));
                    clock = loopStart;
                }
                else
                {
                    clock = end;
                }
            }
            sampleClock = clock;
            position = (double) clock * beatsPerSample;
        }

        // Automation: where in the song this block is (the playhead's place
        // when stopped, so the controls follow it there too), and the
        // stretches it plays (one when stopped, standing still).
        const auto blockBeat = numSegments > 0 ? (double) segments[0].clock * beatsPerSample : position.load();
        std::array<Segment, 16> spans;
        auto numSpans = numSegments;
        std::copy (segments.begin(), segments.begin() + numSegments, spans.begin());
        if (numSpans == 0)
            spans[0] = { toSamples (blockBeat), 0, numSamples }, numSpans = 1;
        const auto spanBeat = [&] (const Segment& span, int sample)   // the beat at a sample of the block in this span
        {
            return nowPlaying ? (double) (span.clock + (sample - span.offset)) * beatsPerSample : blockBeat;
        };

        // Plugin knobs first, so the block plays with them where the song has them.
        applyKnobs (masterAutomation, nullptr, masterInserts, blockBeat);
        for (auto& track : tracks)
            applyKnobs (track->automation, track->instrument.get(), track->inserts, blockBeat);

        // Each instrument plays its notes and what's played live into it;
        // then its inserts; then its volume and pan, into the master bus.
        for (int channel = 0; channel < masterBuffer.getNumChannels(); ++channel)
            juce::FloatVectorOperations::clear (masterBuffer.getWritePointer (channel), numSamples);
        juce::AudioBuffer<float> master (masterBuffer.getArrayOfWritePointers(), masterBuffer.getNumChannels(), numSamples);
        const auto liveId = liveTrack.load();
        for (auto& track : tracks)
        {
            if (track->id == liveId)
                track->midi.addEvents (live, 0, numSamples, 0);
            if (track->audio)
            {
                // An audio track: its clips, and what comes in if it's monitored.
                for (int channel = 0; channel < track->buffer.getNumChannels(); ++channel)
                    juce::FloatVectorOperations::clear (track->buffer.getWritePointer (channel), numSamples);
                for (int i = 0; i < numSegments; ++i)
                    playClips (*track, segments[(size_t) i].clock, segments[(size_t) i].offset, segments[(size_t) i].length, beatsPerSample);
                if (track->monitor)
                {
                    juce::FloatVectorOperations::add (track->buffer.getWritePointer (0), inputChannel (track->input, 0), numSamples);
                    juce::FloatVectorOperations::add (track->buffer.getWritePointer (1),
                                                      inputChannel (track->stereoInput ? track->input + 1 : track->input, 0), numSamples);
                }
            }
            else if (track->instrument == nullptr)
            {
                continue;
            }
            else
            {
                // (Cleared and copied sample by sample: JUCE buffers remember being
                // cleared, and a view cleared here wouldn't see the instrument's
                // sound written through another view, then skip mixing it.)
                for (int channel = 0; channel < track->buffer.getNumChannels(); ++channel)
                    juce::FloatVectorOperations::clear (track->buffer.getWritePointer (channel), numSamples);
                juce::AudioBuffer<float> played (track->buffer.getArrayOfWritePointers(), track->instrumentChannels, numSamples);
                track->instrument->processBlock (played, track->midi);
                if (track->instrument->getTotalNumOutputChannels() == 1)   // a mono instrument: on both sides
                    juce::FloatVectorOperations::copy (track->buffer.getWritePointer (1), track->buffer.getReadPointer (0), numSamples);
            }
            juce::AudioBuffer<float> sound (track->buffer.getArrayOfWritePointers(), track->buffer.getNumChannels(), numSamples);
            runInserts (track->inserts, sound, numSamples, 0, track->preFader);

            // Its fader and pan, on its own sound; then the inserts after the
            // fader; then into the master.
            auto* left = sound.getWritePointer (0);
            auto* right = sound.getWritePointer (1);
            const MixNow set { track->volumeDb, track->pan, track->mute };
            if (track->mixAutomated)
            {
                // Volume, pan and mute as the lanes have them, every 32
                // samples, in a straight line between (no steps to hear).
                const auto gainsAt = [&] (double at)
                {
                    const auto mix = mixAt (track->automation, set, at);
                    return sideGains (mix.volumeDb, mix.pan, ! mix.mute && ! track->soloedOut);
                };
                std::pair<float, float> gains;
                for (int s = 0; s < numSpans; ++s)
                {
                    const auto& span = spans[(size_t) s];
                    gains = gainsAt (spanBeat (span, span.offset));
                    for (int from = span.offset; from < span.offset + span.length; from += automationStep)
                    {
                        const auto n = juce::jmin (automationStep, span.offset + span.length - from);
                        const auto next = gainsAt (spanBeat (span, from + n));
                        for (int i = 0; i < n; ++i)
                        {
                            const auto along = (float) i / (float) n;
                            left[from + i] *= gains.first + (next.first - gains.first) * along;
                            right[from + i] *= gains.second + (next.second - gains.second) * along;
                        }
                        gains = next;
                    }
                }
                // Turned off, the mix moves on from here.
                track->left.setCurrentAndTargetValue (gains.first);
                track->right.setCurrentAndTargetValue (gains.second);
            }
            else
            {
                track->left.setTargetValue (track->gainLeft);
                track->right.setTargetValue (track->gainRight);
                for (int i = 0; i < numSamples; ++i)
                {
                    left[i] *= track->left.getNextValue();
                    right[i] *= track->right.getNextValue();
                }
            }
            runInserts (track->inserts, sound, numSamples, track->preFader, Project::maxInserts);

            auto* outLeft = master.getWritePointer (0);
            auto* outRight = master.getWritePointer (1);
            juce::FloatVectorOperations::add (outLeft, left, numSamples);
            juce::FloatVectorOperations::add (outRight, right, numSamples);
            float peak = 0.0f;
            for (int i = 0; i < numSamples; ++i)
                peak = juce::jmax (peak, std::abs (left[i]), std::abs (right[i]));
            for (auto& meter : track->meters)
                meter.pushSamples (&peak, 1);
        }

        // The master: its inserts before the fader, its volume, the inserts after it, to the output.
        runInserts (masterInserts, master, numSamples, 0, masterPreFader);
        const auto outputs = juce::jmin (2, output.getNumChannels());
        if (masterAutomated)
        {
            const MixNow set { settings.masterVolumeDb, 0.0f, false };
            const auto gainAt = [&] (double at) { return levelOf (mixAt (masterAutomation, set, at).volumeDb); };
            float gain = 0.0f;
            for (int s = 0; s < numSpans; ++s)
            {
                const auto& span = spans[(size_t) s];
                gain = gainAt (spanBeat (span, span.offset));
                for (int from = span.offset; from < span.offset + span.length; from += automationStep)
                {
                    const auto n = juce::jmin (automationStep, span.offset + span.length - from);
                    const auto next = gainAt (spanBeat (span, from + n));
                    for (int channel = 0; channel < 2; ++channel)
                    {
                        auto* sound = master.getWritePointer (channel);
                        for (int i = 0; i < n; ++i)
                            sound[from + i] *= gain + (next - gain) * (float) i / (float) n;
                    }
                    gain = next;
                }
            }
            masterGain.setCurrentAndTargetValue (gain);
        }
        else
        {
            masterGain.setTargetValue (settings.masterGain);
            auto* masterLeft = master.getWritePointer (0);
            auto* masterRight = master.getWritePointer (1);
            for (int i = 0; i < numSamples; ++i)
            {
                const auto gain = masterGain.getNextValue();
                masterLeft[i] *= gain;
                masterRight[i] *= gain;
            }
        }
        runInserts (masterInserts, master, numSamples, masterPreFader, Project::maxInserts);
        for (int channel = 0; channel < outputs; ++channel)
            juce::FloatVectorOperations::copy (output.getWritePointer (channel), master.getReadPointer (channel), numSamples);

        // For the screen: each mix as it is at the block's end.
        const auto endBeat = numSegments > 0 ? position.load() : blockBeat;
        for (auto& track : tracks)
        {
            const auto now = mixAt (track->automation, { track->volumeDb, track->pan, track->mute }, endBeat);
            track->nowVolumeDb = now.volumeDb;
            track->nowPan = now.pan;
            track->nowMute = now.mute;
        }
        nowMasterVolumeDb = mixAt (masterAutomation, { settings.masterVolumeDb, 0.0f, false }, endBeat).volumeDb;

        // The click: a short tone at each beat, higher on the bar's first.
        for (int i = 0, next = 0; i < numSamples; ++i)
        {
            if (next < numTicks && ticks[(size_t) next].sample == i)
            {
                clickRemaining = (int) (clickSeconds * sampleRate);
                clickIncrement = (ticks[(size_t) next].accent ? 1760.0 : 1320.0) / sampleRate;
                clickPhase = 0.0;
                clickLevel = clickVolume;
                ++next;
            }
            if (clickRemaining > 0)
            {
                const auto fade = (float) clickRemaining / (float) (clickSeconds * sampleRate);
                const auto sample = clickLevel * fade * (float) std::sin (juce::MathConstants<double>::twoPi * clickPhase);
                clickPhase += clickIncrement;
                --clickRemaining;
                for (int channel = 0; channel < output.getNumChannels(); ++channel)
                    output.addSample (channel, i, sample);
            }
        }

        for (int channel = 0; channel < juce::jmin (2, output.getNumChannels()); ++channel)
            for (auto& reader : outputMeters)
                reader[(size_t) channel].pushSamples (output.getReadPointer (channel), numSamples);
    }

    //==========================================================================
    Engine::MixNow Engine::mixAt (const std::vector<LaneState>& lanes, MixNow mix, double beat)
    {
        for (const auto& state : lanes)
        {
            const auto value = state.lane.valueAt (beat);
            switch (state.lane.target)
            {
                case AutomationLane::Target::volume: mix.volumeDb = travelToVolume (*value); break;
                case AutomationLane::Target::pan:    mix.pan = travelToPan (*value); break;
                case AutomationLane::Target::mute:   mix.mute = *value >= 0.5f; break;
                case AutomationLane::Target::instrument:
                case AutomationLane::Target::insert: break;
            }
        }
        return mix;
    }

    void Engine::applyKnobs (std::vector<LaneState>& lanes, juce::AudioPluginInstance* instrument, std::vector<Slot>& inserts, double beat)
    {
        for (auto& state : lanes)
        {
            const auto& lane = state.lane;
            juce::AudioPluginInstance* plugin = nullptr;
            if (lane.target == AutomationLane::Target::instrument)
                plugin = instrument;
            else if (lane.target == AutomationLane::Target::insert && lane.slot < (int) inserts.size())
                plugin = inserts[(size_t) lane.slot].plugin.get();
            if (plugin == nullptr)
                continue;
            const auto& parameters = plugin->getParameters();
            if (lane.parameter >= parameters.size())
                continue;
            const auto value = *lane.valueAt (beat);
            if (std::abs (value - state.applied) < 1.0e-6f)
                continue;
            state.applied = value;
            auto* parameter = parameters[lane.parameter];
            parameter->setValue (value);   // as a host does: the plugin hears it before this block (its window follows)
        }
    }

    Engine::MixNow Engine::getMixNow (int busId) const
    {
        if (busId == masterBus)
            return { nowMasterVolumeDb.load(), 0.0f, false };
        const juce::ScopedLock sl (lock);
        if (const auto* track = findTrack (busId))
            return { track->nowVolumeDb.load(), track->nowPan.load(), track->nowMute.load() };
        return {};
    }

    //==========================================================================
    void Engine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels, float* const* outputChannelData,
                                                   int numOutputChannels, int numSamples, const juce::AudioIODeviceCallbackContext&)
    {
        // The studio renders stereo; the device's first two outputs get it.
        blockInputs = inputChannelData;
        blockNumInputs = numInputChannels;
        for (int done = 0; done < numSamples;)
        {
            const auto length = juce::jmin (deviceBuffer.getNumSamples(), numSamples - done);
            juce::AudioBuffer<float> stereo (deviceBuffer.getArrayOfWritePointers(), 2, length);
            blockInputOffset = done;
            renderBlock (stereo, length);
            for (int channel = 0; channel < numOutputChannels; ++channel)
                if (outputChannelData[channel] != nullptr)
                {
                    if (channel < 2)
                        juce::FloatVectorOperations::copy (outputChannelData[channel] + done, stereo.getReadPointer (channel), length);
                    else
                        juce::FloatVectorOperations::clear (outputChannelData[channel] + done, length);
                }
            done += length;
        }
    }

    void Engine::audioDeviceAboutToStart (juce::AudioIODevice* device)
    {
        prepare (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
    }

    void Engine::audioDeviceStopped()
    {
        const juce::ScopedLock sl (lock);
        for (auto& track : tracks)
        {
            if (track->instrument != nullptr)
                track->instrument->releaseResources();
            for (auto& slot : track->inserts)
                if (slot.plugin != nullptr)
                    slot.plugin->releaseResources();
        }
        for (auto& slot : masterInserts)
            if (slot.plugin != nullptr)
                slot.plugin->releaseResources();
    }
}
