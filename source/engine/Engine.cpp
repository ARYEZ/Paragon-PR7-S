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
        std::pair<float, float> sideGains (const Track& track, bool audible)
        {
            if (! audible)
                return { 0.0f, 0.0f };
            const auto gain = track.volumeDb <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain (track.volumeDb);
            return { gain * juce::jmin (1.0f, 1.0f - track.pan), gain * juce::jmin (1.0f, 1.0f + track.pan) };
        }
    }

    Engine::Engine() = default;

    Engine::~Engine()
    {
        const juce::ScopedLock sl (lock);
        tracks.clear();
    }

    void Engine::prepare (double newSampleRate, int newMaxBlockSize)
    {
        const juce::ScopedLock sl (lock);
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        maxBlockSize = juce::jmax (1, newMaxBlockSize);
        midiInput.reset (sampleRate);
        live.ensureSize (2048);
        deviceBuffer.setSize (2, maxBlockSize);
        for (auto& track : tracks)
            prepareTrack (*track);
    }

    void Engine::prepareTrack (TrackState& track)
    {
        if (track.instrument != nullptr)
        {
            track.instrument->setPlayHead (&head);
            track.instrument->setRateAndBufferSizeDetails (sampleRate, maxBlockSize);
            track.instrument->prepareToPlay (sampleRate, maxBlockSize);
            const auto channels = juce::jmax (2, track.instrument->getTotalNumInputChannels(), track.instrument->getTotalNumOutputChannels());
            track.buffer.setSize (channels, maxBlockSize);
        }
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
        prepareTrack (*incoming);

        std::unique_ptr<juce::AudioPluginInstance> old;
        {
            const juce::ScopedLock sl (lock);
            if (auto* existing = findTrack (trackId))
            {
                // Keep the notes and mix; send the old instrument away.
                old = std::move (existing->instrument);
                existing->instrument = std::move (incoming->instrument);
                existing->buffer = std::move (incoming->buffer);
                existing->held.reset();
            }
            else
            {
                tracks.push_back (std::move (incoming));
            }
        }
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
    }

    juce::AudioPluginInstance* Engine::getInstrument (int trackId) const
    {
        const juce::ScopedLock sl (lock);
        auto* track = findTrack (trackId);
        return track != nullptr ? track->instrument.get() : nullptr;
    }

    void Engine::update (const Project& project)
    {
        // Each track's notes as note on/off events by beat, built here
        // (allocating) and swapped in under the lock.
        auto anySolo = false;
        for (const auto& track : project.tracks)
            anySolo = anySolo || track.solo;

        struct Prepared { int id; std::vector<Event> events; std::pair<float, float> gains; };
        std::vector<Prepared> prepared;
        for (const auto& track : project.tracks)
        {
            Prepared p { track.id, {}, sideGains (track, ! track.mute && (! anySolo || track.solo)) };
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

        const juce::ScopedLock sl (lock);
        settings = { project.bpm, project.numerator, project.denominator, project.loopOn,
                     project.loopStart, juce::jmax (project.loopStart + 0.0625, project.loopEnd), project.click };
        for (auto& p : prepared)
            if (auto* track = findTrack (p.id))
            {
                std::swap (track->events, p.events);   // the old list goes out with `prepared`
                track->gainLeft = p.gains.first;
                track->gainRight = p.gains.second;
            }
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
            jumpTo = beats;   // the audio thread jumps, stopping the notes sounding
        else
            position = beats;
    }

    void Engine::takeRecorded (std::vector<RecordedEvent>& into)
    {
        const auto scope = recordedFifo.read (recordedFifo.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i)
            into.push_back (recordedEvents[(size_t) (scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i)
            into.push_back (recordedEvents[(size_t) (scope.startIndex2 + i)]);
    }

    float Engine::takeTrackPeak (int trackId)
    {
        const juce::ScopedLock sl (lock);
        auto* track = findTrack (trackId);
        return track != nullptr ? track->meter.takePeak() : 0.0f;
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

    void Engine::render (juce::AudioBuffer<float>& output, int numSamples)
    {
        // Larger than the device promised: in pieces.
        for (int done = 0; done < numSamples;)
        {
            const auto length = juce::jmin (maxBlockSize, numSamples - done);
            juce::AudioBuffer<float> piece (output.getArrayOfWritePointers(), output.getNumChannels(), done, length);
            renderBlock (piece, length);
            done += length;
        }
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
        if (! nowPlaying || ! wasPlaying || std::abs (clockBeatsPerSample - beatsPerSample) > 1.0e-15)
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

                for (auto& track : tracks)
                    schedule (*track, clock, end, done, beatsPerSample);

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

        // Each instrument plays its notes and what's played live into it.
        const auto liveId = liveTrack.load();
        for (auto& track : tracks)
        {
            if (track->id == liveId)
                track->midi.addEvents (live, 0, numSamples, 0);
            if (track->instrument == nullptr)
                continue;

            juce::AudioBuffer<float> sound (track->buffer.getArrayOfWritePointers(), track->buffer.getNumChannels(), numSamples);
            sound.clear();
            track->instrument->processBlock (sound, track->midi);

            track->left.setTargetValue (track->gainLeft);
            track->right.setTargetValue (track->gainRight);
            const auto* left = sound.getReadPointer (0);
            const auto* right = track->instrument->getTotalNumOutputChannels() > 1 ? sound.getReadPointer (1) : left;
            auto* outLeft = output.getWritePointer (0);
            auto* outRight = output.getNumChannels() > 1 ? output.getWritePointer (1) : nullptr;
            float peak = 0.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                const auto l = left[i] * track->left.getNextValue();
                const auto r = right[i] * track->right.getNextValue();
                outLeft[i] += l;
                if (outRight != nullptr)
                    outRight[i] += r;
                peak = juce::jmax (peak, std::abs (l), std::abs (r));
            }
            track->meter.pushSamples (&peak, 1);
        }

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
            outputMeters[(size_t) channel].pushSamples (output.getReadPointer (channel), numSamples);
    }

    //==========================================================================
    void Engine::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* outputChannelData, int numOutputChannels,
                                                   int numSamples, const juce::AudioIODeviceCallbackContext&)
    {
        // The studio renders stereo; the device's first two outputs get it.
        for (int done = 0; done < numSamples;)
        {
            const auto length = juce::jmin (deviceBuffer.getNumSamples(), numSamples - done);
            juce::AudioBuffer<float> stereo (deviceBuffer.getArrayOfWritePointers(), 2, length);
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
            if (track->instrument != nullptr)
                track->instrument->releaseResources();
    }
}
