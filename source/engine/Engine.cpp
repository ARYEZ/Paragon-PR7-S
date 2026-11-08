#include "Engine.h"
#include "Controllers.h"
#include "SessionLog.h"
#include "Patterns.h"
#include <map>

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
        // (0.57.1) A knob's last value set carried over to the new lanes (the same control): an update
        // no longer has every automated plug-in knob set again on the next block when nothing moved.
        template <typename LaneStates>
        void keepApplied (LaneStates& into, const LaneStates& from)
        {
            for (auto& state : into)
                for (const auto& old : from)
                    if (old.lane.sameControl (state.lane))
                    {
                        state.applied = old.applied;
                        break;
                    }
        }

        template <typename LaneStates>
        void readLanes (const std::vector<AutomationLane>& lanes, bool read, LaneStates& into, bool& mixAutomated,
                        int busId, const std::vector<std::pair<int, AutomationLane>>& held, const Project::AutomationSettings& automation)
        {
            mixAutomated = false;
            const auto isHeld = [&] (const AutomationLane& lane)
            {
                return std::any_of (held.begin(), held.end(), [&] (const auto& h) { return h.first == busId && h.second.sameControl (lane); });
            };
            for (const auto& lane : lanes)
                if (read && ! lane.muted && ! lane.points.empty() && ! isHeld (lane)
                    && ! automation.suspendRead[(size_t) Project::AutomationSettings::groupOf (lane)])   // (0.28.0: Suspend Read)
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
        // (0.52.1) Out from under the lock, then each plug-in unloaded as a step of its own: a hang
        // while closing names the plug-in.
        std::vector<std::unique_ptr<TrackState>> going;
        std::vector<Slot> masterGoing;
        {
            const juce::ScopedLock sl (lock);
            std::swap (going, tracks);
            std::swap (masterGoing, masterInserts);
        }
        const auto unload = [] (std::unique_ptr<juce::AudioPluginInstance>& plugin)
        {
            if (plugin == nullptr)
                return;
            const SessionLog::Step step ("Unloading " + plugin->getName());
            plugin.reset();
        };
        for (auto& track : going)
        {
            for (auto& slot : track->inserts)
                unload (slot.plugin);
            unload (track->instrument);
        }
        for (auto& slot : masterGoing)
            unload (slot.plugin);
    }

    void Engine::prepare (double newSampleRate, int newMaxBlockSize)
    {
        const juce::ScopedLock sl (lock);
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        maxBlockSize = juce::jmax (1, newMaxBlockSize);
        midiInput.reset (sampleRate);
        live.ensureSize (2048);
        liveMapped.ensureSize (2048);
        deviceBuffer.setSize (2, maxBlockSize);
        silence.assign ((size_t) maxBlockSize, 0.0f);
        effectMidi.ensureSize (4096);   // (an effect may send MIDI out: room for it, not grown on the audio thread)
        auto masterChannels = 2;
        for (auto& slot : masterInserts)
            if (slot.plugin != nullptr)
            {
                const SessionLog::Step step ("Preparing " + slot.plugin->getName() + " (the master's) at " + juce::String (sampleRate, 0) + " Hz");
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
                {
                    const SessionLog::Step step ("Preparing " + slot.plugin->getName() + " at " + juce::String (sampleRate, 0) + " Hz");
                    prepareEffect (*slot.plugin);
                }
            std::unique_ptr<SessionLog::Step> step;
            if (track->instrument != nullptr)
                step = std::make_unique<SessionLog::Step> ("Preparing " + track->instrument->getName() + " at " + juce::String (sampleRate, 0) + " Hz");
            prepareTrack (*track);
            step.reset();
            track->eq.setDesign (designEq (track->eqSettings, sampleRate));   // (0.43.0: its filters at the new rate)
            track->eq.reset();
        }
        masterEq.setDesign (designEq (masterEqSettings, sampleRate));
        masterEq.reset();
    }

    void Engine::setLiveInputFilter (const juce::String& deviceName)
    {
        const juce::SpinLock::ScopedLockType sl (liveFilterLock);
        liveFilter = deviceName;
    }

    bool Engine::acceptsLiveFrom (const juce::String& deviceName) const
    {
        const juce::SpinLock::ScopedLockType sl (liveFilterLock);
        return liveFilter.isEmpty() || liveFilter == deviceName;
    }

    void Engine::setNonRealtime (bool offline, const std::function<bool (int trackId)>& sounding)
    {
        // A plugin reads realtime or offline when it's set up (VST3's
        // setupProcessing, in prepareToPlay), and JUCE skips that set-up if the
        // rate and block size haven't changed. So each one switching is
        // released first and prepared again: Kontakt and Superior Drummer,
        // marked offline without it, rendered silence (0.8.6).
        // Only the plugins that will sound (`sounding`: the tracks in the
        // render, the master's), and only those prepared again: switching and
        // preparing every plugin in the song made a render hang before it
        // started (0.9.9). Back to realtime: all that are offline.
        const juce::ScopedLock sl (lock);
        const auto set = [this, offline] (juce::AudioPluginInstance* plugin, bool instrument)
        {
            if (plugin == nullptr || plugin->isNonRealtime() == offline)
                return;
            const SessionLog::Step step ("Switching " + plugin->getName() + (offline ? " to offline (for the render)" : " back to realtime"));
            plugin->releaseResources();
            plugin->setNonRealtime (offline);
            if (instrument)
            {
                plugin->setRateAndBufferSizeDetails (sampleRate, maxBlockSize);
                plugin->prepareToPlay (sampleRate, maxBlockSize);
            }
            else
                prepareEffect (*plugin);
        };
        for (auto& slot : masterInserts)
            set (slot.plugin.get(), false);
        for (auto& track : tracks)
        {
            if (offline && sounding && ! sounding (track->id))
                continue;
            set (track->instrument.get(), true);
            for (auto& slot : track->inserts)
                set (slot.plugin.get(), false);
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
        track.bus.setSize (2, maxBlockSize);
        track.midi.ensureSize (4096);
        track.effectMidi.ensureSize (4096);   // (0.34.5: each track's own: effects may run on several cores at once)
        track.preFaderCopy.setSize (2, maxBlockSize);
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
        {
            std::unique_ptr<SessionLog::Step> step;
            if (incoming->instrument != nullptr)
                step = std::make_unique<SessionLog::Step> ("Preparing the instrument " + incoming->instrument->getName());
            prepareTrack (*incoming);
        }
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
                for (auto& state : existing->automation)   // (0.57.1: the new one is sent every automated value)
                    state.applied = -1.0f;
                existing->instrument = std::move (incoming->instrument);
                existing->instrumentChannels = incoming->instrumentChannels;
                std::swap (existing->buffer, incoming->buffer);
                existing->held.reset();
            }
            else
            {
                tracks.push_back (std::move (incoming));
                wave.reserve (tracks.size() + 16);   // (0.34.5: room for a wave of them, made here, not on the audio thread)
            }
        }
        if (old != nullptr && onPluginGoing)
            onPluginGoing (old.get());
        // `old` (and `incoming` if it was emptied) are freed here, outside the lock.
        if (old != nullptr)
        {
            const SessionLog::Step step ("Unloading the instrument " + old->getName());
            old.reset();
        }
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
        if (removed != nullptr)
        {
            for (auto& slot : removed->inserts)
                if (slot.plugin != nullptr)
                {
                    const SessionLog::Step step ("Unloading " + slot.plugin->getName() + " (a track removed)");
                    slot.plugin.reset();
                }
            if (removed->instrument != nullptr)
            {
                const SessionLog::Step step ("Unloading the instrument " + removed->instrument->getName() + " (a track removed)");
                removed->instrument.reset();
            }
        }
    }

    juce::AudioPluginInstance* Engine::getInstrument (int trackId) const
    {
        // (no lock: see getMixNow)
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
        wave.reserve (tracks.size() + 16);   // (0.34.5: room for a wave of them, made here, not on the audio thread)
    }

    void Engine::addChannel (int trackId, bool group)
    {
        auto incoming = std::make_unique<TrackState>();
        incoming->id = trackId;
        incoming->role = group ? TrackState::Role::group : TrackState::Role::fx;
        incoming->inserts.reserve ((size_t) Project::maxInserts);
        prepareTrack (*incoming);
        const juce::ScopedLock sl (lock);
        if (findTrack (trackId) == nullptr)
            tracks.push_back (std::move (incoming));
        wave.reserve (tracks.size() + 16);   // (0.34.5: room for a wave of them, made here, not on the audio thread)
    }

    float Engine::getSendNowDb (int trackId, int slot) const
    {
        // (no lock: see getMixNow)
        if (const auto* track = findTrack (trackId); track != nullptr && juce::isPositiveAndBelow (slot, Project::maxSends))
            return track->nowSendDb[(size_t) slot].load();
        return -60.0f;
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

    void Engine::placeClip (Clip& clip, const Timing& timing)
    {
        clip.startSample = timing.sampleAt (clip.start);
        clip.endSample = timing.sampleAt (clip.end);
        // ((0.52.0) A crossfade's stretch can reach past the clip's own edges, into its recording.)
        // Only with an auto fade there to cover it; else within the clip, as before.
        const auto from = clip.autoIn > 0.0 ? clip.from : juce::jmax (clip.from, clip.start);
        const auto to = clip.autoOut > 0.0 ? clip.to : juce::jmin (clip.to, clip.end);
        clip.fromSample = from != clip.start ? timing.sampleAt (from) : clip.startSample;
        clip.toSample = to != clip.end ? timing.sampleAt (to) : clip.endSample;
    }

    void Engine::playClips (TrackState& track, juce::int64 clock, int offset, int length, const Timing&)
    {
        // Each clip's samples in this stretch of the song, faded at its ends,
        // added to the track's sound.
        auto* left = track.buffer.getWritePointer (0);
        auto* right = track.buffer.getWritePointer (1);
        for (const auto& clip : track.clips)
        {
            const auto startSample = clip.startSample;
            const auto endSample = clip.endSample;
            const auto from = juce::jmax (clock, clip.fromSample);
            const auto to = juce::jmin (clock + length, clip.toSample);
            if (from >= to || clip.sound == nullptr)
                continue;
            const auto& sound = *clip.sound;
            const auto* sourceLeft = sound.getReadPointer (0);
            const auto* sourceRight = sound.getNumChannels() > 1 ? sound.getReadPointer (1) : sourceLeft;
            const auto clipLength = (double) (endSample - startSample);
            // One sample as it always was: inside the recording, faded at the clip's ends.
            const auto playOne = [&] (juce::int64 s)
            {
                const auto into = (double) (s - startSample);
                const auto at = clip.offset + (s - startSample);
                if (at < 0 || at >= sound.getNumSamples())
                    return;
                auto gain = (double) clip.gain;
                if (clip.fadeIn > 0.0 && into < clip.fadeIn)
                    gain *= juce::jmax (0.0, into) / clip.fadeIn;
                if (clip.fadeOut > 0.0 && clipLength - into < clip.fadeOut)
                    gain *= juce::jmax (0.0, clipLength - into) / clip.fadeOut;
                // (0.52.0) Auto fades at the stretch's ends.
                const auto curve = [&clip] (double x)
                {
                    x = juce::jlimit (0.0, 1.0, x);
                    return clip.equalPower ? std::sin (x * juce::MathConstants<double>::halfPi) : x;
                };
                if (clip.autoIn > 0.0 && (double) (s - clip.fromSample) < clip.autoIn)
                    gain *= curve (((double) (s - clip.fromSample) + 0.5) / clip.autoIn);
                if (clip.autoOut > 0.0 && (double) (clip.toSample - s) <= clip.autoOut)
                    gain *= curve (((double) (clip.toSample - s) - 0.5) / clip.autoOut);
                const auto i = offset + (int) (s - clock);
                left[i] += (float) (sourceLeft[at] * gain);
                right[i] += (float) (sourceRight[at] * gain);
            };
            // (0.34.1) The middle (inside the recording, no fade: the gain is exactly 1, so each sample
            // added as it is, the same as the line above) in one go; the edges one at a time.
            const auto inFadeIn = [&] (juce::int64 s) { return (clip.fadeIn > 0.0 && (double) (s - startSample) < clip.fadeIn)
                                                               || s < startSample || (clip.autoIn > 0.0 && (double) (s - clip.fromSample) < clip.autoIn); };
            const auto inFadeOut = [&] (juce::int64 s) { return (clip.fadeOut > 0.0 && clipLength - (double) (s - startSample) < clip.fadeOut)
                                                                || s >= endSample || (clip.autoOut > 0.0 && (double) (clip.toSample - s) <= clip.autoOut); };
            auto middleFrom = juce::jmax (from, startSample - clip.offset);                         // (the recording's first sample)
            auto middleTo = juce::jmin (to, startSample - clip.offset + sound.getNumSamples());    // (past its last)
            while (middleFrom < middleTo && inFadeIn (middleFrom))
                ++middleFrom;   // (at most the fade's length, once per block)
            while (middleTo > middleFrom && inFadeOut (middleTo - 1))
                --middleTo;
            if (middleFrom >= middleTo)
            {
                for (auto s = from; s < to; ++s)
                    playOne (s);
                continue;
            }
            for (auto s = from; s < middleFrom; ++s)
                playOne (s);
            const auto n = (int) (middleTo - middleFrom);
            const auto i = offset + (int) (middleFrom - clock);
            const auto at = clip.offset + (middleFrom - startSample);
            if (clip.gain == 1.0f)
            {
                juce::FloatVectorOperations::add (left + i, sourceLeft + at, n);
                juce::FloatVectorOperations::add (right + i, sourceRight + at, n);
            }
            else if (clip.gain > 0.0f)
            {
                // (0.47.0) The event's volume: one multiply-add (the same as playOne's, to a rounding).
                juce::FloatVectorOperations::addWithMultiply (left + i, sourceLeft + at, clip.gain, n);
                juce::FloatVectorOperations::addWithMultiply (right + i, sourceRight + at, clip.gain, n);
            }
            for (auto s = middleTo; s < to; ++s)
                playOne (s);
        }
    }

    void Engine::insertEffect (int busId, int slot, std::unique_ptr<juce::AudioPluginInstance> effect)
    {
        if (effect != nullptr)
        {
            const SessionLog::Step step ("Preparing the effect " + effect->getName());
            prepareEffect (*effect);
        }

        // A track not here yet (its instrument couldn't be loaded): kept anyway, silent.
        if (busId != masterBus && findTrack (busId) == nullptr)
        {
            auto silent = std::make_unique<TrackState>();
            silent->id = busId;
            silent->inserts.reserve ((size_t) Project::maxInserts);
            prepareTrack (*silent);
            const juce::ScopedLock sl (lock);
            tracks.push_back (std::move (silent));
            wave.reserve (tracks.size() + 16);   // (0.34.5: room for a wave of them, made here, not on the audio thread)
        }

        // A bigger buffer, if this effect has more channels, made here; swapped in below.
        auto& inserts = busId == masterBus ? masterInserts : findTrack (busId)->inserts;
        auto& buffer = busId == masterBus ? masterBuffer : findTrack (busId)->buffer;
        juce::AudioBuffer<float> bigger;
        if (effect != nullptr && channelsFor (*effect) > buffer.getNumChannels())
            bigger.setSize (channelsFor (*effect), maxBlockSize);

        const juce::ScopedLock sl (lock);
        for (auto& state : busId == masterBus ? masterAutomation : findTrack (busId)->automation)   // (0.57.1: the slots move: sent again)
            state.applied = -1.0f;
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
            for (auto& state : track != nullptr ? track->automation : masterAutomation)   // (0.57.1: the slots move: sent again)
                state.applied = -1.0f;
        }
        if (removed != nullptr && onPluginGoing)
            onPluginGoing (removed.get());
        // `removed` is deleted here, outside the lock.
        if (removed != nullptr)
        {
            const SessionLog::Step step ("Unloading the effect " + removed->getName());
            removed.reset();
        }
    }

    juce::AudioPluginInstance* Engine::getEffect (int busId, int slot) const
    {
        // (no lock: see getMixNow)
        const auto* track = busId == masterBus ? nullptr : findTrack (busId);
        const auto* inserts = busId == masterBus ? &masterInserts : track != nullptr ? &track->inserts : nullptr;
        return inserts != nullptr && juce::isPositiveAndBelow (slot, (int) inserts->size()) ? (*inserts)[(size_t) slot].plugin.get() : nullptr;
    }

    int Engine::getNumEffects (int busId) const
    {
        // (no lock: see getMixNow)
        const auto* track = busId == masterBus ? nullptr : findTrack (busId);
        return busId == masterBus ? (int) masterInserts.size() : track != nullptr ? (int) track->inserts.size() : 0;
    }

    void Engine::update (const Project& project)
    {
        // Each track's notes as note on/off events by beat, built here
        // (allocating) and swapped in under the lock.
        // Solo (0.70.0, the owner's rule: Project::soloedOutTracks): only the soloed tracks, what is routed into
        // them and the channels they go on into make sound. (A disabled track's solo doesn't count.)
        const auto soloedOutIds = project.soloedOutTracks();
        const auto soloedOut = [&soloedOutIds] (const Track& t)
        { return std::find (soloedOutIds.begin(), soloedOutIds.end(), t.id) != soloedOutIds.end(); };

        // The tempo: fixed, or the tempo track's (built here; moved in under the lock).
        auto tempo = project.tempoMap();
        const auto tempoVersion = settings.tempoVersion + (tempo.getEvents() == settings.tempo.getEvents() ? 0 : 1);

        struct Prepared { int id; std::vector<Event> events; std::pair<float, float> gains; std::vector<bool> bypass; std::vector<Clip> clips;
                          std::vector<LaneState> automation; bool mixAutomated = false; bool eventsKept = false; bool clipsKept = false;
                          std::array<juce::int16, 128> drumOut {}; std::array<juce::int8, 128> drumChannel {}; std::array<juce::uint8, 128> drumIn {}; };
        std::vector<Prepared> prepared;
        for (const auto& track : project.tracks)
        {
            Prepared p { track.id, {}, sideGains (track, ! track.mute && ! track.disabled && ! soloedOut (track)), {}, {}, {}, false, false, false, {}, {}, {} };
            auto* engineTrack = findTrack (track.id);   // (message thread: its builtFrom is ours alone)
            if (engineTrack != nullptr && (track.disabled || track.frozen))
                engineTrack->builtFrom.valid = false;   // (no events or clips of its own: built afresh next time)
            for (int n = 0; n < 128; ++n)
                p.drumOut[(size_t) n] = (juce::int16) n, p.drumChannel[(size_t) n] = -1, p.drumIn[(size_t) n] = (juce::uint8) n;
            for (const auto& insert : track.inserts)
                p.bypass.push_back (insert.bypass);
            readLanes (track.automation, track.automationRead, p.automation, p.mixAutomated, track.id, held, project.automationSettings);
            if (track.disabled)
            {
                // Disabled: nothing played, nothing read from disk (its plugins are unloaded too).
                p.automation.clear();
                p.mixAutomated = false;
                prepared.push_back (std::move (p));
                continue;
            }
            if (track.frozen)
            {
                // Frozen: the freeze file from the song's start (its fader, pan and sends still act on it).
                if (audioSource)
                    if (auto sound = audioSource (track.freezeFile))
                        p.clips.push_back ({ 0.0, tempo.beatAt ((double) sound->getNumSamples() / sampleRate), sound, 0, 0.0, 0.0 });
                for (auto& clip : p.clips)
                    placeClip (clip, { &tempo, sampleRate });
                prepared.push_back (std::move (p));
                continue;
            }
            if (track.isAudio())
            {
                // Only what's in front plays where clips overlap (takes, 0.29.0): each clip as
                // many times as it has stretches heard, its fades still at its own ends.
                // (0.57.1) Each part's heard stretches once for the track, each recording asked for once.
                std::map<juce::String, decltype (audioSource (juce::String()))> sounds;
                const auto soundOf = [&] (const juce::String& file)
                {
                    auto found = sounds.find (file);
                    if (found == sounds.end())
                        found = sounds.emplace (file, audioSource (file)).first;
                    return found->second;
                };
                const auto soundFor = [&] (const Part& part)
                {
                    return part.isWarped() && warpSource ? warpSource (part, tempo) : soundOf (part.audioFile);
                };
                // (0.57.2) Built from the same parts, solo lane, auto fades, tempo and rate, the same parts
                // are heard; if they still play the very same sounds, the same clips would come out: kept
                // (a fader moved, a send, a knob).
                const auto& fadesNow = project.autoFadesOf (track);
                if (engineTrack != nullptr)
                {
                    const auto& built = engineTrack->builtFrom;
                    auto same = built.valid && built.parts == track.parts && built.soloLane == track.soloLane
                                && built.fades == fadesNow && built.tempo == tempo.getEvents() && built.rate == sampleRate
                                && built.sounds.size() == track.parts.size();
                    for (size_t i = 0; same && i < track.parts.size(); ++i)
                        if (built.sounds[i] != nullptr)
                            same = soundFor (track.parts[i]).get() == built.sounds[i];
                    if (same)
                    {
                        p.clipsKept = true;
                        prepared.push_back (std::move (p));
                        continue;
                    }
                }
                const auto heardOf = heardStretchesOf (track);
                std::vector<decltype (audioSource (juce::String()))> partSounds (track.parts.size());
                std::vector<const void*> soundIds (track.parts.size(), nullptr);
                for (int i = 0; i < (int) track.parts.size(); ++i)
                {
                    const auto& part = track.parts[(size_t) i];
                    if (! part.isAudio() || ! audioSource || heardOf[(size_t) i].empty())
                        continue;
                    partSounds[(size_t) i] = soundFor (part);
                    soundIds[(size_t) i] = partSounds[(size_t) i].get();
                }
                if (engineTrack != nullptr)
                {
                    auto& built = engineTrack->builtFrom;
                    built.valid = true;
                    built.parts = track.parts;
                    built.soloLane = track.soloLane;
                    built.sounds = soundIds;
                    built.fades = fadesNow;
                    built.tempo = tempo.getEvents();
                    built.rate = sampleRate;
                }
                ++clipBuilds;
                for (int i = 0; i < (int) track.parts.size(); ++i)
                {
                    const auto& part = track.parts[(size_t) i];
                    if (! part.isAudio() || ! audioSource)
                        continue;
                    const auto& stretches = heardOf[(size_t) i];
                    if (stretches.empty())
                        continue;
                    // Warped (0.32.0): its stretched sound, from the clip's start.
                    const auto warped = part.isWarped() && warpSource;
                    auto sound = partSounds[(size_t) i];
                    const auto offset = warped ? (juce::int64) 0 : (juce::int64) std::llround (part.audioOffset * sampleRate);
                    // (0.52.0) With the auto fades: a crossfade's stretch reaches past the cut.
                    const auto& fades = project.autoFadesOf (track);
                    const auto recording = [&] (const Part& other)
                    {
                        const auto s = soundOf (other.audioFile);
                        return s != nullptr ? (double) s->getNumSamples() / sampleRate : 0.0;
                    };
                    for (const auto& f : fadedStretches (track, i, fades, tempo, recording, &heardOf))
                    {
                        Clip clip { part.start, part.end(), sound, offset, part.fadeIn * sampleRate, part.fadeOut * sampleRate, part.gain(), 0.0, 0.0, true,
                                    f.from, f.to };
                        clip.autoIn = f.fadeIn * sampleRate;
                        clip.autoOut = f.fadeOut * sampleRate;
                        clip.equalPower = fades.equalPower;
                        p.clips.push_back (clip);
                    }
                }
                for (auto& clip : p.clips)
                    placeClip (clip, { &tempo, sampleRate });
                prepared.push_back (std::move (p));
                continue;
            }
            // On the track's MIDI channel (the instrument may listen to one: a Kontakt multi).
            const auto channelBits = juce::jlimit (1, 16, track.midiChannel) - 1;
            // Through its drum map (0.26.0): each sound's O-note, its own channel if it has one, muted sounds silent.
            const auto* map = project.drumMapOf (track);
            for (int n = 0; n < 128; ++n)
            {
                const auto* sound = map != nullptr ? &map->sounds[(size_t) n] : nullptr;
                p.drumOut[(size_t) n] = (juce::int16) (sound == nullptr ? n : sound->mute ? -1 : sound->outNote);
                p.drumChannel[(size_t) n] = (juce::int8) (sound != nullptr && sound->channel >= 0 ? sound->channel : -1);
                p.drumIn[(size_t) n] = (juce::uint8) (map != nullptr ? map->pitchForInput (n) : n);
            }
            // (0.34.1) Built from exactly what it was last time: kept (the same events would come out).
            const auto mapNow = map != nullptr ? std::optional<DrumMap> (*map) : std::nullopt;
            // (0.36.0) Following the chord track: the chords it follows count as what it's built from.
            const auto follows = track.follow != FollowMode::off && ! project.chordTrack.events.empty();
            // (0.44.0) Performing a pattern over the chord track.
            const auto performIndex = track.kind == Track::Kind::instrument && ! project.chordTrack.events.empty()
                                          ? patternNamed (track.performPattern) : -1;
            const auto patternNow = performIndex >= 0 ? track.performPattern : juce::String();
            const auto followNow = follows ? track.follow : FollowMode::off;
            const auto referenceNow = follows ? track.followReference : FollowReference {};
            const auto chordsNow = follows || performIndex >= 0 ? project.chordTrack : ChordTrack {};
            const auto fallbackNow = follows ? project.chordPads.key() : Key {};
            const auto voiceNow = follows ? track.followVoice : Voice::soprano;
            const auto voicingNow = follows ? track.followVoicing : std::nullopt;
            if (engineTrack != nullptr)
            {
                auto& built = engineTrack->builtFrom;
                if (built.valid && built.midiChannel == track.midiChannel && built.soloLane == track.soloLane
                    && built.map == mapNow && built.parts == track.parts && built.follow == followNow
                    && built.reference == referenceNow && built.chords == chordsNow && built.fallback == fallbackNow
                    && built.voice == voiceNow && built.voicing == voicingNow && built.pattern == patternNow)
                {
                    p.eventsKept = true;
                    prepared.push_back (std::move (p));
                    continue;
                }
                built = { true, track.parts, track.midiChannel, track.soloLane, mapNow, followNow, referenceNow, chordsNow, fallbackNow,
                          voiceNow, voicingNow, patternNow };
            }
            ++eventBuilds;
            for (const auto& part : track.parts)
            {
                if (part.muted || (track.soloLane >= 0 && part.lane != track.soloLane))
                    continue;   // a muted part isn't played (nor, with a lane soloed, another lane's: 0.29.0)
                // (0.36.0) Each note's pitch as the chord track has it now (dropped: doubled by another).
                const auto followed = follows ? followNotes (part.notes, part.start, followNow, referenceNow, chordsNow,
                                                             chordsNow.shiftTicks, fallbackNow, voicingNow ? &*voicingNow : nullptr, voiceNow)
                                              : std::vector<FollowedNote> {};
                for (size_t n = 0; n < part.notes.size(); ++n)
                {
                    const auto& note = part.notes[n];
                    if (follows && followed[n].dropped)
                        continue;
                    if (note.muted)
                        continue;   // (0.77.1) muted in the MIDI editor
                    // (0.47.0) The part's Transpose and Velocity from the info line, for playback only.
                    const auto pitch = part.heardPitch (juce::jlimit (0, 127, follows ? followed[n].pitch : note.pitch));
                    const auto on = part.start + note.start;
                    if (note.start < 0.0 || note.start >= part.length)
                        continue;   // outside the part: not heard (as in a DAW)
                    const auto out = p.drumOut[(size_t) pitch];
                    if (out < 0)
                        continue;   // a muted drum sound
                    const auto bits = p.drumChannel[(size_t) pitch] >= 0 ? p.drumChannel[(size_t) pitch] : channelBits;
                    const auto off = part.start + juce::jmin (note.start + note.length, part.length);
                    const auto velocity = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (part.heardVelocity (note.velocity) * 127.0f));
                    p.events.push_back ({ on, (juce::uint8) (0x90 | bits), (juce::uint8) out, velocity });
                    p.events.push_back ({ off, (juce::uint8) (0x80 | bits), (juce::uint8) out, 0 });
                }
            }
            if (performIndex >= 0)
            {
                // (0.44.0) The pattern over each chord (until the next; the last a bar).
                std::vector<ChordSpan> spans;
                const auto& events = project.chordTrack.events;
                for (size_t e = 0; e < events.size(); ++e)
                    spans.push_back ({ events[e].beat, project.chordTrack.endOf ((int) e, project.beatsPerBar()), events[e].chord, Key {} });
                for (const auto& note : performChordTrack (patterns()[(size_t) performIndex], spans, project.beatsPerBar()))
                {
                    const auto pitch = juce::jlimit (0, 127, note.pitch);
                    const auto velocity = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (note.velocity * 127.0f));
                    p.events.push_back ({ note.beat, (juce::uint8) (0x90 | channelBits), (juce::uint8) pitch, velocity });
                    p.events.push_back ({ note.beat + note.length, (juce::uint8) (0x80 | channelBits), (juce::uint8) pitch, 0 });
                }
            }
            // Controllers (0.25.0): each event, and a ramp as a value every 1/128 beat where it changes.
            for (const auto& part : track.parts)
            {
                if (part.muted || (track.soloLane >= 0 && part.lane != track.soloLane))
                    continue;
                ControllerKinds::expandRamps (part, 1.0 / 128.0, [&] (double beat, int code, int value)
                {
                    if (beat < 0.0 || beat >= part.length)
                        return;
                    const auto message = ControllerKinds::toMidi (code, value, channelBits + 1);
                    const auto* raw = message.getRawData();
                    p.events.push_back ({ part.start + beat, raw[0], raw[1], message.getRawDataSize() > 2 ? raw[2] : (juce::uint8) 0 });
                });
            }
            // By beat; at the same beat a note off comes first, so a repeated note restarts, then
            // controllers (a note starts with the bend and the controllers it's meant to have).
            std::stable_sort (p.events.begin(), p.events.end(), [] (const Event& a, const Event& b)
            {
                if (a.beat < b.beat || b.beat < a.beat)
                    return a.beat < b.beat;
                const auto rank = [] (const Event& e) { const auto kind = e.status & 0xf0; return kind == 0x80 ? 0 : kind == 0x90 ? 2 : 1; };
                return rank (a) < rank (b);
            });
            prepared.push_back (std::move (p));
        }

        // Live Input (0.36.0): the chord track as bits by beat, for the live track's keys.
        std::vector<LiveChord> chordBits;
        auto liveMode = LiveFollow::off;
        auto liveModeTrack = 0;
        for (const auto& track : project.tracks)
            if (track.liveFollow != LiveFollow::off && ! track.isAudio() && ! track.isChannel())
                if (track.id == liveTrack.load() || liveModeTrack == 0)
                    liveMode = track.liveFollow, liveModeTrack = track.id;
        if (liveMode != LiveFollow::off)
        {
            const auto fallback = project.chordPads.key();
            for (const auto& e : project.chordTrack.events)
            {
                const auto key = project.chordTrack.keyAt (e.beat, project.chordTrack.scales.empty() ? fallback : project.chordTrack.scales.front().key);
                chordBits.push_back ({ e.beat, pitchClassMask (e.chord.pitchClasses()), pitchClassMask (key.scale()) });
            }
        }

        const auto masterLevel = levelOf (project.masterVolumeDb);
        std::vector<LaneState> masterLanes;
        bool masterLanesMix = false;
        readLanes (project.masterAutomation, project.masterRead, masterLanes, masterLanesMix, masterBus, held, project.automationSettings);

        const juce::ScopedLock sl (lock);
        settings = { std::move (tempo), tempoVersion, project.signatures(), project.loopOn,
                     project.loopStart, juce::jmax (project.loopStart + 0.0625, project.loopEnd), project.click, masterLevel,
                     project.masterVolumeDb, project.automationSettings.virginTerritory };
        keepApplied (masterLanes, masterAutomation);
        std::swap (masterAutomation, masterLanes);
        std::swap (liveChords, chordBits);   // (the old list goes out with `chordBits`)
        liveFollowMode = liveMode;
        liveFollowTrack = liveModeTrack;
        masterPreFader = project.masterPreFaderSlots;
        if (! (masterEqSettings == project.masterEq))
        {
            masterEqSettings = project.masterEq;
            masterEq.setDesign (designEq (masterEqSettings, sampleRate));
        }
        masterAutomated = masterLanesMix;
        for (auto& p : prepared)
            if (auto* track = findTrack (p.id))
            {
                if (! p.eventsKept)
                {
                    std::swap (track->events, p.events);   // the old list goes out with `prepared`
                    track->checkHeld = true;
                }
                if (! p.clipsKept)
                    std::swap (track->clips, p.clips);  // likewise the old clips
                if (const auto* t = project.findTrack (p.id))
                {
                    // (0.50.0) A channel's role follows the song: a folder with its group channel on is a group.
                    if (t->isChannel())
                        track->role = t->isGroup() ? TrackState::Role::group : TrackState::Role::fx;
                    track->input = t->input;
                    track->frozen = t->frozen;
                    track->midiChannel = juce::jlimit (1, 16, t->midiChannel);
                    track->stereoInput = t->stereoInput;
                    track->monitor = t->monitor && ! t->disabled;
                    track->volumeDb = t->volumeDb;
                    track->pan = t->pan;
                    track->mute = t->mute || t->disabled;
                    track->soloedOut = soloedOut (*t);
                    track->preFader = t->preFaderSlots;
                    if (! (track->eqSettings == t->eq))   // (0.43.0: designed only when it changed)
                    {
                        track->eqSettings = t->eq;
                        track->eq.setDesign (designEq (t->eq, sampleRate));
                    }
                    track->output = t->output;
                    for (int s = 0; s < Project::maxSends; ++s)
                    {
                        auto& send = track->sends[(size_t) s];
                        const auto* from = juce::isPositiveAndBelow (s, (int) t->sends.size()) ? &t->sends[(size_t) s] : nullptr;
                        send.target = from != nullptr ? from->target : 0;
                        send.levelDb = from != nullptr ? from->levelDb : -60.0f;
                        send.on = from != nullptr && from->on && from->target != 0 && ! t->sendsBypass;
                        send.preFader = from != nullptr && from->preFader;
                    }
                }
                keepApplied (p.automation, track->automation);
                std::swap (track->automation, p.automation);
                track->drumOut = p.drumOut;
                track->drumChannel = p.drumChannel;
                track->drumIn = p.drumIn;
                track->mixAutomated = p.mixAutomated;
                track->gainLeft = p.gains.first;
                track->gainRight = p.gains.second;
                for (size_t i = 0; i < juce::jmin (p.bypass.size(), track->inserts.size()); ++i)
                    track->inserts[i].bypass = p.bypass[i];
            }
        for (size_t i = 0; i < juce::jmin (project.masterInserts.size(), masterInserts.size()); ++i)
            masterInserts[i].bypass = project.masterInserts[i].bypass;
        // (0.60.0) The order channels are mixed in: a channel only once everything feeding it is in. Its height:
        // the longest chain of channels its output and sends lead through to the master (one that feeds the
        // master alone: 0). The studio never makes a loop; the passes stop after one per track anyway.
        for (auto& track : tracks)
            track->height = 0;
        deepestChannel = 0;
        for (size_t pass = 0; pass <= tracks.size(); ++pass)
        {
            auto grew = false;
            for (auto& track : tracks)
            {
                if (track->role == TrackState::Role::source)
                    continue;
                const auto feed = [&] (int id)
                {
                    if (const auto* to = id != 0 ? findTrack (id) : nullptr; to != nullptr && to != track.get() && to->role != TrackState::Role::source
                                                                             && track->height < to->height + 1)
                    {
                        track->height = to->height + 1;
                        grew = true;
                    }
                };
                feed (track->output);
                for (const auto& send : track->sends)
                    feed (send.target);
                deepestChannel = juce::jmax (deepestChannel, track->height);
            }
            if (! grew)
                break;
        }
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
        // (no lock: see getMixNow)
        auto* track = findTrack (trackId);
        return track != nullptr ? track->meters[(size_t) juce::jlimit (0, 2, reader)].takePeak() : 0.0f;
    }

    void Engine::runInserts (std::vector<Slot>& slots, juce::AudioBuffer<float>& buffer, int numSamples, int fromSlot, int toSlot, juce::MidiBuffer& effectsMidi)
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
            effectsMidi.clear();
            const auto started = juce::Time::getHighResolutionTicks();   // (0.57.4: each plug-in's own time)
            effect.processBlock (view, effectsMidi);
            slot.ownSeconds += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - started);
            if (outs == 1 && buffer.getNumChannels() > 1)   // mono out: to both sides
                juce::FloatVectorOperations::copy (buffer.getWritePointer (1), buffer.getReadPointer (0), numSamples);
        }
    }

    //==========================================================================
    void Engine::schedule (TrackState& track, juce::int64 fromSample, juce::int64 toSample, int sampleOffset, const Timing& timing)
    {
        // Every event whose sample (its beat by the tempo, rounded to the
        // nearest sample) is from `fromSample` up to (not including) `toSample`.
        if (std::exchange (track.chase, false))
            chaseControllers (track, timing.beatAt ((double) fromSample - 0.5), sampleOffset);
        const auto first = std::lower_bound (track.events.begin(), track.events.end(), timing.beatAt ((double) fromSample - 0.5),
                                             [] (const Event& e, double beat) { return e.beat < beat; });
        for (auto it = first; it != track.events.end(); ++it)
        {
            const auto at = timing.sampleAt (it->beat);
            if (at < fromSample)
                continue;
            if (at >= toSample)
                break;
            const auto kind = it->status & 0xf0;
            if (kind == 0x90)
            {
                const auto channel = (it->status & 0x0f) + 1;
                if (track.held.any() && channel != track.heldChannel)
                    stopHeldNotes (track, sampleOffset + (int) (at - fromSample));   // (the channel changed: the old notes end on theirs)
                track.heldChannel = channel;
                track.held.set (it->data1);
            }
            else if (kind == 0x80)
                track.held.reset (it->data1);
            else if (kind == 0xe0)
                track.bent = true;
            else if (kind == 0xb0 && it->data1 == 64)
                track.sustained = it->data2 >= 64;
            const juce::uint8 bytes[] = { it->status, it->data1, it->data2 };
            track.midi.addEvent (bytes, 3, sampleOffset + (int) (at - fromSample));
        }
    }

    void Engine::chaseControllers (TrackState& track, double beat, int sampleOffset)
    {
        // (0.25.0, the manual's chasing) Each controller's value where playing starts: the
        // last event of its kind before it, so a sound starts with the bend, the pedal and the
        // controllers it has there. (Audio thread: fixed arrays, no allocation.)
        std::array<juce::int16, 128> cc;
        cc.fill (-1);
        int bend = -1, pressure = -1, program = -1;
        auto channelBits = 0;
        for (const auto& e : track.events)
        {
            if (e.beat >= beat)
                break;
            switch (e.status & 0xf0)
            {
                case 0xb0: cc[(size_t) (e.data1 & 0x7f)] = e.data2; channelBits = e.status & 0x0f; break;
                case 0xe0: bend = e.data1 | (e.data2 << 7); channelBits = e.status & 0x0f; break;
                case 0xd0: pressure = e.data1; channelBits = e.status & 0x0f; break;
                case 0xc0: program = e.data1; channelBits = e.status & 0x0f; break;
                default: break;
            }
        }
        const auto channel = channelBits + 1;
        if (program >= 0)
            track.midi.addEvent (juce::MidiMessage::programChange (channel, program), sampleOffset);
        for (int n = 0; n < 128; ++n)
            if (cc[(size_t) n] >= 0)
                track.midi.addEvent (juce::MidiMessage::controllerEvent (channel, n, cc[(size_t) n]), sampleOffset);
        if (bend >= 0)
        {
            track.midi.addEvent (juce::MidiMessage::pitchWheel (channel, bend), sampleOffset);
            track.bent = bend != 8192;
        }
        if (pressure >= 0)
            track.midi.addEvent (juce::MidiMessage::channelPressureChange (channel, pressure), sampleOffset);
        if (cc[64] >= 0)
            track.sustained = cc[64] >= 64;
    }

    void Engine::resetControllers (TrackState& track, int sampleOffset)
    {
        // On stop (as the manual's Reset on Stop): a bend back to the middle and the sustain
        // pedal up, so nothing rings or stays out of tune after the song stops.
        if (track.bent)
            track.midi.addEvent (juce::MidiMessage::pitchWheel (track.midiChannel, 8192), sampleOffset);
        if (track.sustained)
            track.midi.addEvent (juce::MidiMessage::controllerEvent (track.midiChannel, 64, 0), sampleOffset);
        track.bent = track.sustained = false;
    }

    void Engine::stopHeldNotes (TrackState& track, int sampleOffset)
    {
        for (int note = 0; note < 128; ++note)
            if (track.held.test ((size_t) note))
                track.midi.addEvent (juce::MidiMessage::noteOff (track.heldChannel, note), sampleOffset);
        track.held.reset();
    }

    void Engine::endNotesNoLongerEnded (TrackState& track, double beatNow, int sampleOffset)
    {
        // A note sounding goes on only if the new list still ends it ahead, on
        // its channel, before the same key starts again (else it would ring
        // until stop: the part was muted, the note deleted, moved or transposed).
        const auto channelBits = (juce::uint8) (track.heldChannel - 1);
        const auto from = std::lower_bound (track.events.begin(), track.events.end(), beatNow - 1.0e-9,
                                            [] (const Event& e, double beat) { return e.beat < beat; });
        // (0.34.1) One walk for every held note, stopping once each has met its next on or off (it
        // walked the list once per note, to its end for a note never ended: a spike after an edit).
        auto unsettled = track.held;
        std::bitset<128> ends;
        for (auto it = from; it != track.events.end() && unsettled.any(); ++it)
        {
            const auto kind = it->status & 0xf0;
            if ((kind != 0x80 && kind != 0x90) || (it->status & 0x0f) != channelBits || ! unsettled.test (it->data1 & 0x7f))
                continue;
            ends.set (it->data1 & 0x7f, kind == 0x80);
            unsettled.reset (it->data1 & 0x7f);
        }
        for (int note = 0; note < 128; ++note)   // (in this order, as before)
            if (track.held.test ((size_t) note) && ! ends.test ((size_t) note))
            {
                track.midi.addEvent (juce::MidiMessage::noteOff (track.heldChannel, note), sampleOffset);
                track.held.reset ((size_t) note);
            }
    }

    void Engine::render (juce::AudioBuffer<float>& output, int numSamples, const juce::AudioBuffer<float>* input)
    {
        // (0.34.1) Numbers too small to hear (under about -760 dB) count as 0: a decaying tail or a
        // filter going quiet otherwise costs the processor many times over. Only this thread, only now.
        const juce::ScopedNoDenormals noDenormals;
        // Larger than the device promised: in pieces.
        blockInputs = input != nullptr ? input->getArrayOfReadPointers() : nullptr;
        blockNumInputs = input != nullptr ? input->getNumChannels() : 0;
        newCall = true;
        lockMissedThisCall = false;
        masterSecondsThisCall = 0.0;
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

    void Engine::processTrack (TrackState& state, const BlockContext& block)
    {
        // (0.34.5) On any core: only this track's own things are touched here.
        auto* track = &state;
        track->playedThisBlock = false;
        const auto numSamples = block.numSamples;
        const auto& timing = *block.timing;
        const auto liveId = block.liveId;
        const auto* spans = block.spans;
        const auto numSpans = block.numSpans;
        const auto spanBeat = [&block] (const Segment& span, int sample) { return block.spanBeat (span, sample); };
        if (track->id == liveId)
            for (const auto metadata : live)   // played live: on the track's channel, as its notes
            {
                auto message = metadata.getMessage();
                if (message.getChannel() > 0)
                    message.setChannel (track->midiChannel);
                if (message.isNoteOnOrOff())
                {
                    // Through its drum map: the key played is a sound's I-note, sent as its O-note.
                    const auto pitch = track->drumIn[(size_t) message.getNoteNumber()];
                    const auto out = track->drumOut[pitch];
                    if (out < 0)
                        continue;   // a muted sound
                    message.setNoteNumber (out);
                    if (track->drumChannel[pitch] >= 0)
                        message.setChannel (track->drumChannel[pitch] + 1);
                }
                track->midi.addEvent (message, metadata.samplePosition);
            }
        if (track->role != TrackState::Role::source)
        {
            // A group or FX channel: what was routed or sent to it.
            for (int channel = 0; channel < track->buffer.getNumChannels(); ++channel)
            {
                if (channel < 2)
                    juce::FloatVectorOperations::copy (track->buffer.getWritePointer (channel), track->bus.getReadPointer (channel), numSamples);
                else
                    juce::FloatVectorOperations::clear (track->buffer.getWritePointer (channel), numSamples);
            }
        }
        else if (track->audio || track->frozen)
        {
            // An audio track: its clips, and what comes in if it's monitored (a frozen track: its freeze file).
            for (int channel = 0; channel < track->buffer.getNumChannels(); ++channel)
                juce::FloatVectorOperations::clear (track->buffer.getWritePointer (channel), numSamples);
            for (int i = 0; i < numSegments; ++i)
                playClips (*track, segments[(size_t) i].clock, segments[(size_t) i].offset, segments[(size_t) i].length, timing);
            if (track->monitor && track->audio && ! track->frozen)
            {
                juce::FloatVectorOperations::add (track->buffer.getWritePointer (0), inputChannel (track->input, 0), numSamples);
                juce::FloatVectorOperations::add (track->buffer.getWritePointer (1),
                                                  inputChannel (track->stereoInput ? track->input + 1 : track->input, 0), numSamples);
            }
        }
        else if (track->instrument == nullptr)
        {
            return;   // (no instrument: nothing to play, send or meter)
        }
        else
        {
            // (Cleared and copied sample by sample: JUCE buffers remember being
            // cleared, and a view cleared here wouldn't see the instrument's
            // sound written through another view, then skip mixing it.)
            for (int channel = 0; channel < track->buffer.getNumChannels(); ++channel)
                juce::FloatVectorOperations::clear (track->buffer.getWritePointer (channel), numSamples);
            juce::AudioBuffer<float> played (track->buffer.getArrayOfWritePointers(), track->instrumentChannels, numSamples);
            const auto started = juce::Time::getHighResolutionTicks();   // (0.57.4: the instrument's own time)
            track->instrument->processBlock (played, track->midi);
            track->instrumentSeconds += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - started);
            if (track->instrument->getTotalNumOutputChannels() == 1)   // a mono instrument: on both sides
                juce::FloatVectorOperations::copy (track->buffer.getWritePointer (1), track->buffer.getReadPointer (0), numSamples);
        }
        juce::AudioBuffer<float> sound (track->buffer.getArrayOfWritePointers(), track->buffer.getNumChannels(), numSamples);
        // (0.43.0) The Pre section (gain, phase, cuts), the inserts before the fader, then the EQ.
        // A frozen track's file has them already.
        if (! track->frozen)
            track->eq.processPre (sound.getArrayOfWritePointers(), sound.getNumChannels(), numSamples);
        runInserts (track->inserts, sound, numSamples, 0, track->preFader, track->effectMidi);
        if (! track->frozen)
            track->eq.processEq (sound.getArrayOfWritePointers(), sound.getNumChannels(), numSamples);
        afterEq (track->id, sound.getArrayOfWritePointers(), sound.getNumChannels(), numSamples);
        // The sends before the fader take the sound as it is here: kept for them (they're added
        // into the FX channels afterwards, on the audio thread, in the tracks' order).
        const auto preSends = std::any_of (track->sends.begin(), track->sends.end(), [] (const auto& send) { return send.preFader && send.target != 0; });
        if (preSends)
            for (int channel = 0; channel < 2; ++channel)
                juce::FloatVectorOperations::copy (track->preFaderCopy.getWritePointer (channel), sound.getReadPointer (channel), numSamples);

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
                const auto mix = mixAt (track->automation, set, at, settings.virginTerritory);
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
            if (! track->left.isSmoothing() && ! track->right.isSmoothing())
            {
                // (0.34.1) Not moving: each side times its gain in one go (getNextValue would
                // return exactly that gain for every sample).
                juce::FloatVectorOperations::multiply (left, track->left.getTargetValue(), numSamples);
                juce::FloatVectorOperations::multiply (right, track->right.getTargetValue(), numSamples);
            }
            else
                for (int i = 0; i < numSamples; ++i)
                {
                    left[i] *= track->left.getNextValue();
                    right[i] *= track->right.getNextValue();
                }
        }
        runInserts (track->inserts, sound, numSamples, track->preFader, Project::maxInserts, track->effectMidi);
        float peak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            peak = juce::jmax (peak, std::abs (left[i]), std::abs (right[i]));
        track->blockPeak = peak;
        track->playedThisBlock = true;
    }

    void Engine::mixTrackOut (TrackState& state, const BlockContext& block, juce::AudioBuffer<float>& master)
    {
        // (0.34.5) On the audio thread, in the tracks' order: its sends, then its sound to its group or the master.
        auto* track = &state;
        if (! track->playedThisBlock)
            return;
        const auto numSamples = block.numSamples;
        runSends (*track, true, numSamples, block.blockBeat, track->preFaderCopy);   // the sends before the fader
        runSends (*track, false, numSamples, block.blockBeat, track->buffer);       // the sends after it
        const auto* left = track->buffer.getReadPointer (0);
        const auto* right = track->buffer.getReadPointer (1);

        // To its group (a track routed to one), else the master.
        auto* out = &master;
        juce::AudioBuffer<float> toGroup;
        const auto only = exportBus.load();   // (0.51.0) a channel export: only that channel to the output
        if (track->output != 0 && track->id != only)   // (0.60.0: any track or channel into any group or FX channel)
            if (auto* group = findTrack (track->output); group != nullptr && group->role != TrackState::Role::source && group != track)
            {
                toGroup.setDataToReferTo (group->bus.getArrayOfWritePointers(), 2, numSamples);
                out = &toGroup;
            }
        if (only == 0 || out != &master || track->id == only)
        {
            juce::FloatVectorOperations::add (out->getWritePointer (0), left, numSamples);
            juce::FloatVectorOperations::add (out->getWritePointer (1), right, numSamples);
        }
        for (auto& meter : track->meters)
            meter.pushSamples (&track->blockPeak, 1);
    }

    void Engine::mapLive (double beat) noexcept
    {
        // The chord in force at the block's start (before the first: the first; none: as played).
        const LiveChord* now = nullptr;
        for (const auto& c : liveChords)
            if (c.beat <= beat + 1.0e-9 || now == nullptr)
                now = &c;
            else
                break;
        liveMapped.clear();
        for (const auto metadata : live)
        {
            auto message = metadata.getMessage();
            if (message.isNoteOnOrOff())
            {
                const auto channel = juce::jlimit (1, 16, message.getChannel()) - 1;
                const auto key = message.getNoteNumber();
                auto& sent = liveSent[(size_t) channel][(size_t) key];
                if (message.isNoteOn())
                {
                    const auto mask = now == nullptr ? (juce::uint16) 0 : liveFollowMode == LiveFollow::chords ? now->chord : now->scale;
                    const auto to = nearestInMask (key, mask);
                    sent = (juce::int16) (to + 1);
                    message.setNoteNumber (to);
                }
                else if (sent > 0)
                {
                    message.setNoteNumber (sent - 1);   // (its note on's note)
                    sent = 0;
                }
            }
            liveMapped.addEvent (message, metadata.samplePosition);
        }
        live.swapWith (liveMapped);
    }

    void Engine::renderBlock (juce::AudioBuffer<float>& output, int numSamples)
    {
        output.clear();
        const juce::ScopedTryLock sl (lock);
        if (! sl.isLocked())
        {
            lockMissedThisCall = true;   // (0.57.3: the spike report says so)
            return;   // the screen is swapping something in: one silent block
        }
        if (std::exchange (newCall, false))
        {
            for (auto& slot : masterInserts)
                slot.ownSeconds = 0.0;
            for (auto& track : tracks)
            {
                track->ownSeconds = track->instrumentSeconds = 0.0;
                for (auto& slot : track->inserts)
                    slot.ownSeconds = 0.0;
            }
        }

        // What's being played live this block: devices and the on-screen keyboard.
        live.clear();
        midiInput.removeNextBlockOfMessages (live, numSamples);
        keyboardState.processNextMidiBuffer (live, 0, numSamples, true);

        for (auto& track : tracks)
            track->midi.clear();

        const auto nowPlaying = playing.load();
        // The song's samples from its start, by the tempo (fixed, or the tempo track's).
        const Timing timing { &settings.tempo, sampleRate };
        const auto toSamples = [&timing] (double beats) { return timing.sampleAt (beats); };

        // The clock: re-based from the beat position when stopped, on
        // starting, and when the tempo changes; on a jump, from the new place
        // (the notes sounding stop). Stopping stops them too.
        if (! nowPlaying || ! wasPlaying || rebase.exchange (false) || clockTempoVersion != settings.tempoVersion)
            sampleClock = toSamples (position.load());
        clockTempoVersion = settings.tempoVersion;
        if (const auto jump = jumpTo.exchange (-1.0); jump >= 0.0 && nowPlaying)
        {
            sampleClock = toSamples (jump);
            for (auto& track : tracks)
                stopHeldNotes (*track, 0), track->chase = true;
        }
        if (nowPlaying && ! wasPlaying)
            for (auto& track : tracks)
                track->chase = true;
        if ((wasPlaying && ! nowPlaying) || releaseAll.exchange (false))
            for (auto& track : tracks)
                stopHeldNotes (*track, 0), resetControllers (*track, 0);
        wasPlaying = nowPlaying;
        if (nowPlaying)
        {
            const auto beatNow = timing.beatAt ((double) sampleClock);
            for (auto& track : tracks)
                if (std::exchange (track->checkHeld, false) && track->held.any())
                    endNotesNoLongerEnded (*track, beatNow, 0);
        }

        auto clock = sampleClock;
        const auto beat = timing.beatAt ((double) clock);
        if (liveFollowMode != LiveFollow::off && liveFollowTrack == liveTrack.load())
            mapLive (beat);   // (0.36.0) keys onto the chord track's chord or scale, before they're played or recorded
        head.info.setBpm (settings.tempo.bpmAt (beat));
        {
            const auto& signature = settings.signatures.segmentAt (beat).event;
            head.info.setTimeSignature (juce::AudioPlayHead::TimeSignature { signature.numerator, signature.denominator });
            head.info.setPpqPositionOfLastBarStart (settings.signatures.barStart (beat));
            head.info.setBarCount (settings.signatures.barAt (beat));
        }
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
            const auto* recordingTrack = findTrack (liveTrack.load());
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
                    schedule (*track, clock, end, done, timing);

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

                // The click: the signature's beats (a quarter in 4/4, an eighth in 6/8), each bar's first
                // accented, by the signature in force there (0.45.0).
                if (settings.click)
                {
                    const auto& map = settings.signatures;
                    auto pos = map.positionOf (timing.beatAt ((double) clock));
                    for (auto guard = 0; numTicks < (int) ticks.size() && guard < 4096; ++guard)
                    {
                        const auto at = toSamples (map.beatOf (pos.bar, pos.beat, 0.0));
                        if (at >= end)
                            break;
                        if (at >= clock)
                            ticks[(size_t) numTicks++] = { done + (int) (at - clock), pos.beat == 0 };
                        if (++pos.beat >= map.segmentOfBar (pos.bar).event.numerator)
                            pos.beat = 0, ++pos.bar;
                    }
                }

                // Recording: each note played, at its beat in the song.
                if (isRecording)
                    for (const auto metadata : live)
                        if (metadata.samplePosition >= done && metadata.samplePosition < done + length)
                        {
                            const auto message = metadata.getMessage();
                            if (! message.isNoteOnOrOff() && ! message.isController() && ! message.isPitchWheel()
                                && ! message.isChannelPressure() && ! message.isAftertouch() && ! message.isProgramChange())
                                continue;   // (0.25.0: controllers recorded too)
                            const auto scope = recordedFifo.write (1);
                            if (scope.blockSize1 + scope.blockSize2 == 0)
                                continue;   // full: the message thread hasn't kept up
                            const auto at = scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2;
                            const auto* raw = message.getRawData();
                            // (A drum map's I-note recorded as its sound's pitch, as the manual's.)
                            const auto data1 = message.isNoteOnOrOff() && recordingTrack != nullptr ? recordingTrack->drumIn[(size_t) (raw[1] & 0x7f)] : raw[1];
                            recordedEvents[(size_t) at] = { timing.beatAt ((double) (clock + metadata.samplePosition - done)), raw[0],
                                                            data1, message.getRawDataSize() > 2 ? raw[2] : (juce::uint8) 0,
                                                            cycleCount.load() };
                        }

                done += length;
                if (wraps)
                {
                    ++cycleCount;
                    for (auto& track : tracks)
                        stopHeldNotes (*track, juce::jmin (done, numSamples - 1)), track->chase = true;
                    clock = loopStart;
                }
                else
                {
                    clock = end;
                }
            }
            sampleClock = clock;
            position = timing.beatAt ((double) clock);
        }

        // Automation: where in the song this block is (the playhead's place
        // when stopped, so the controls follow it there too), and the
        // stretches it plays (one when stopped, standing still).
        const auto blockBeat = numSegments > 0 ? timing.beatAt ((double) segments[0].clock) : position.load();
        std::array<Segment, 16> spans;
        auto numSpans = numSegments;
        std::copy (segments.begin(), segments.begin() + numSegments, spans.begin());
        if (numSpans == 0)
            spans[0] = { toSamples (blockBeat), 0, numSamples }, numSpans = 1;
        const auto spanBeat = [&] (const Segment& span, int sample)   // the beat at a sample of the block in this span
        {
            return nowPlaying ? timing.beatAt ((double) (span.clock + (sample - span.offset))) : blockBeat;
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
        // What groups and FX channels take in, this block: nothing yet.
        for (auto& track : tracks)
            if (track->role != TrackState::Role::source)
                for (int channel = 0; channel < 2; ++channel)
                    juce::FloatVectorOperations::clear (track->bus.getWritePointer (channel), numSamples);
        // The tracks, then the group and FX channels, deepest first (0.60.0: any channel may feed any other; a
        // channel is mixed once everything routed or sent to it is in: its `height`, worked out in update).
        // (0.34.5) Each pass's own work on the cores side by side, then their sums in order.
        const BlockContext block { numSamples, spans.data(), numSpans, &timing, nowPlaying, blockBeat, liveId };
        for (int pass = 0; pass < deepestChannel + 2; ++pass)
        {
            const auto height = deepestChannel - (pass - 1);
            const auto inPass = [pass, height] (const TrackState& t)
            {
                return pass == 0 ? t.role == TrackState::Role::source : t.role != TrackState::Role::source && t.height == height;
            };
            const auto oneByOne = [&]
            {
                for (auto& track : tracks)
                    if (inPass (*track))
                    {
                        timedTrack (*track, block);
                        mixTrackOut (*track, block, master);
                    }
            };
            if (workers.getHelpers() == 0 || wave.capacity() < tracks.size())
            {
                oneByOne();
                continue;
            }
            wave.clear();   // (keeps its room: nothing allocated)
            for (auto& track : tracks)
                if (inPass (*track))
                    wave.push_back (track.get());   // (none of a pass feeds another of it: their heights would differ)
            if (wave.size() < 2)
            {
                oneByOne();
                continue;
            }
            auto task = [this, &block] (int i) { timedTrack (*wave[(size_t) i], block); };
            workers.run ((int) wave.size(), task);
            for (auto* track : wave)
                mixTrackOut (*track, block, master);
        }

        // The master: its inserts before the fader, its volume, the inserts after it, to the output.
        const auto masterStarted = juce::Time::getHighResolutionTicks();
        masterEq.processPre (master.getArrayOfWritePointers(), master.getNumChannels(), numSamples);
        runInserts (masterInserts, master, numSamples, 0, masterPreFader, effectMidi);
        masterEq.processEq (master.getArrayOfWritePointers(), master.getNumChannels(), numSamples);
        afterEq (masterBus, master.getArrayOfWritePointers(), master.getNumChannels(), numSamples);
        const auto outputs = juce::jmin (2, output.getNumChannels());
        if (masterAutomated)
        {
            const MixNow set { settings.masterVolumeDb, 0.0f, false };
            const auto gainAt = [&] (double at) { return levelOf (mixAt (masterAutomation, set, at, settings.virginTerritory).volumeDb); };
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
            if (! masterGain.isSmoothing())
            {
                juce::FloatVectorOperations::multiply (masterLeft, masterGain.getTargetValue(), numSamples);   // (0.34.1: as the tracks')
                juce::FloatVectorOperations::multiply (masterRight, masterGain.getTargetValue(), numSamples);
            }
            else
                for (int i = 0; i < numSamples; ++i)
                {
                    const auto gain = masterGain.getNextValue();
                    masterLeft[i] *= gain;
                    masterRight[i] *= gain;
                }
        }
        runInserts (masterInserts, master, numSamples, masterPreFader, Project::maxInserts, effectMidi);
        masterSecondsThisCall += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - masterStarted);
        for (int channel = 0; channel < outputs; ++channel)
            juce::FloatVectorOperations::copy (output.getWritePointer (channel), master.getReadPointer (channel), numSamples);

        // For the screen: each mix as it is at the block's end.
        const auto endBeat = numSegments > 0 ? position.load() : blockBeat;
        for (auto& track : tracks)
        {
            const MixNow set { track->volumeDb, track->pan, track->mute };
            const auto now = track->mixAutomated ? mixAt (track->automation, set, endBeat, settings.virginTerritory) : set;   // (0.34.1)
            track->nowVolumeDb = now.volumeDb;
            track->nowPan = now.pan;
            track->nowMute = now.mute;
        }
        nowMasterVolumeDb = mixAt (masterAutomation, { settings.masterVolumeDb, 0.0f, false }, endBeat, settings.virginTerritory).volumeDb;

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

        // The MediaBay's preview (0.21.0), over everything.
        if (previewPlaying.load() && preview != nullptr)
        {
            const auto length = preview->getNumSamples();
            const auto gain = previewGain.load();
            const auto addSample = [&] (int i, int at)
            {
                for (int channel = 0; channel < juce::jmin (2, output.getNumChannels()); ++channel)
                    output.getWritePointer (channel)[i] += gain * preview->getReadPointer (juce::jmin (channel, preview->getNumChannels() - 1))[at];
            };
            const auto wait = previewWait.load();
            const auto lockBeats = previewLockBeats.load();
            if (wait && ! nowPlaying)
            {
                previewPosition = 0;   // (waiting for the song: from the start when it plays)
            }
            else if (wait && lockBeats > 0.0 && numSegments > 0 && length > 0)
            {
                // Locked to the beat (0.23.0): each stretch of the block (the cycle can come round
                // inside it) starts where the song's beat says, from the left locator.
                auto at = 0;
                for (int k = 0; k < numSegments; ++k)
                {
                    const auto& segment = segments[(size_t) k];
                    auto phase = std::fmod ((timing.beatAt ((double) segment.clock) - previewAnchor.load()) / lockBeats, 1.0);
                    if (phase < 0.0)
                        phase += 1.0;
                    at = juce::jlimit (0, length - 1, (int) (phase * (double) length));
                    for (int i = segment.offset; i < segment.offset + segment.length; ++i)
                    {
                        addSample (i, at);
                        if (++at >= length)
                            at = 0;   // (a locked loop always goes round)
                    }
                }
                previewPosition = at;
            }
            else
            {
                auto at = previewPosition.load();
                for (int i = 0; i < numSamples && length > 0; ++i)
                {
                    if (at >= length)
                    {
                        if (! previewLoop.load())
                        {
                            previewPlaying = false;
                            break;
                        }
                        at = 0;
                    }
                    addSample (i, at);
                    ++at;
                }
                previewPosition = at;
            }
        }

        for (int channel = 0; channel < juce::jmin (2, output.getNumChannels()); ++channel)
            for (auto& reader : outputMeters)
                reader[(size_t) channel].pushSamples (output.getReadPointer (channel), numSamples);
    }

    void Engine::setPreview (std::shared_ptr<const juce::AudioBuffer<float>> sound, bool loop)
    {
        // The old sound leaves here, on the message thread, after the swap.
        auto old = sound;
        {
            const juce::ScopedLock sl (lock);
            std::swap (old, preview);
            previewPosition = 0;
            previewLength = preview != nullptr ? preview->getNumSamples() : 0;
            previewLoop = loop;
            previewPlaying = preview != nullptr && previewLength.load() > 0;
        }
    }

    void Engine::stopPreview()
    {
        previewPlaying = false;
    }

    double Engine::getPreviewProgress() const noexcept
    {
        const auto length = previewLength.load();
        return length > 0 ? juce::jlimit (0.0, 1.0, (double) previewPosition.load() / (double) length) : 0.0;
    }

    //==========================================================================
    Engine::MixNow Engine::mixAt (const std::vector<LaneState>& lanes, MixNow mix, double beat, bool virginTerritory)
    {
        for (const auto& state : lanes)
        {
            const auto target = state.lane.target;
            if (target != AutomationLane::Target::volume && target != AutomationLane::Target::pan && target != AutomationLane::Target::mute)
                continue;   // (0.34.1: knob and send lanes aren't read here at all)
            const auto value = state.lane.playedAt (beat, virginTerritory);   // (0.28.0: with its trim; none in a gap)
            if (! value)
                continue;
            switch (target)
            {
                case AutomationLane::Target::volume: mix.volumeDb = travelToVolume (*value); break;
                case AutomationLane::Target::pan:    mix.pan = travelToPan (*value); break;
                case AutomationLane::Target::mute:   mix.mute = *value >= 0.5f; break;
                case AutomationLane::Target::instrument:
                case AutomationLane::Target::insert:
                case AutomationLane::Target::send: break;
            }
        }
        return mix;
    }

    void Engine::runSends (TrackState& track, bool preFader, int numSamples, double beat, const juce::AudioBuffer<float>& source)
    {
        // Each send that's on, to its FX channel: its level (or its lane's, while
        // R is on) ramped across the block from where it was, so moves don't click.
        for (int s = 0; s < Project::maxSends; ++s)
        {
            auto& send = track.sends[(size_t) s];
            if (send.preFader != preFader)
                continue;
            auto db = send.levelDb;
            for (const auto& state : track.automation)
                if (state.lane.target == AutomationLane::Target::send && state.lane.slot == s)
                    if (const auto value = state.lane.playedAt (beat, settings.virginTerritory))
                        db = travelToVolume (*value);
            track.nowSendDb[(size_t) s] = db;
            const auto gain = send.on ? levelOf (db) : 0.0f;
            auto* fx = send.target != 0 ? findTrack (send.target) : nullptr;
            if (fx == nullptr || fx->role != TrackState::Role::fx || (gain <= 0.0f && send.lastGain <= 0.0f))
            {
                send.lastGain = gain;
                continue;
            }
            for (int channel = 0; channel < 2; ++channel)
            {
                const auto* from = source.getReadPointer (channel);
                auto* to = fx->bus.getWritePointer (channel);
                if (juce::exactlyEqual (gain, send.lastGain))
                    for (int i = 0; i < numSamples; ++i)   // (0.34.1: not moving, one gain: the ramp below gives exactly it)
                        to[i] += from[i] * gain;
                else
                    for (int i = 0; i < numSamples; ++i)
                        to[i] += from[i] * (send.lastGain + (gain - send.lastGain) * (float) i / (float) numSamples);
            }
            send.lastGain = gain;
        }
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
            const auto found = lane.valueAt (beat, settings.virginTerritory);
            if (! found)
                continue;   // (a gap: the knob stays where it's set)
            const auto value = *found;
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
        // No lock (0.9.9): the screen's questions (this, the meters, which
        // plugin is where) read atomics, and the lists of tracks and slots that
        // only the message thread changes. Taking the lock here waited for the
        // audio thread's whole block each time: with a 2048-sample buffer and
        // heavy plugins, the mixer's and Inspector's hundreds of questions
        // after a click froze the screen for about a second. Message thread only.
        if (const auto* track = findTrack (busId))
            return { track->nowVolumeDb.load(), track->nowPan.load(), track->nowMute.load() };
        return {};
    }

    //==========================================================================
    void Engine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels, float* const* outputChannelData,
                                                   int numOutputChannels, int numSamples, const juce::AudioIODeviceCallbackContext&)
    {
        SessionLog::markAudioThread();   // (a crash here says so in the session log)
        const juce::ScopedNoDenormals noDenormals;   // (0.34.1: as in render)
        const auto started = juce::Time::getHighResolutionTicks();
        // (0.57.3) For the spike report: how long since the driver's last call; each track's time anew.
        const auto lastCall = lastCallTicks.exchange (started);
        const auto sinceLast = lastCall != 0 ? juce::Time::highResolutionTicksToSeconds (started - lastCall) : 0.0;
        lockMissedThisCall = false;
        masterSecondsThisCall = 0.0;
        newCall = true;
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
        const auto taken = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - started);
        notePerformance (taken, numSamples);
        noteSpike (taken, numSamples, sinceLast);
    }

    void Engine::setHelperThreads (int count)
    {
        // Under the lock: no block is being played while helpers stop and start.
        const juce::ScopedLock sl (lock);
        workers.setHelpers (count);
        wave.reserve (tracks.size() + 16);
    }

    void Engine::settleMix()
    {
        const juce::ScopedLock sl (lock);
        for (auto& track : tracks)
        {
            track->left.setCurrentAndTargetValue (track->gainLeft);
            track->right.setCurrentAndTargetValue (track->gainRight);
            for (auto& send : track->sends)
                send.lastGain = send.on ? levelOf (send.levelDb) : 0.0f;
        }
        masterGain.setCurrentAndTargetValue (settings.masterGain);
    }

    void Engine::timedTrack (TrackState& track, const BlockContext& block)
    {
        // (0.57.3) Its own time (on whichever core runs it; a thread Windows paused meanwhile counts too).
        const auto started = juce::Time::getHighResolutionTicks();
        processTrack (track, block);
        track.ownSeconds += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - started);
    }

    void Engine::noteSpike (double secondsTaken, int numSamples, double secondsSinceLast) noexcept
    {
        const auto rate = getSampleRate();
        if (rate <= 0.0 || numSamples <= 0)
            return;
        const auto budget = (double) numSamples / rate;
        const auto load = (float) (secondsTaken / budget);
        // Later than one and a half blocks after the last call (not a pause of a second or more: the
        // device stopped and started, or the computer slept).
        const auto late = secondsSinceLast > budget * 1.5 && secondsSinceLast < 1.0;
        const auto spiked = load >= 0.9f || lockMissedThisCall || late;
        const auto fill = spiked && ! spikeReady.load (std::memory_order_acquire);
        if (spiked && ! fill)
            ++spikesMissed;
        if (fill)
        {
            spike = {};
            spike.load = load;
            spike.blockMs = secondsTaken * 1000.0;
            spike.budgetMs = budget * 1000.0;
            spike.sinceLastMs = secondsSinceLast * 1000.0;
            spike.missedLock = lockMissedThisCall;
            spike.lateCall = late;
            spike.masterMs = masterSecondsThisCall * 1000.0;
            spike.helpers = workers.getHelpers();
        }
        // Each track's time against its usual (the track list only read under the lock; busy: skipped).
        const juce::ScopedTryLock sl (lock);
        if (sl.isLocked())
            for (auto& track : tracks)
            {
                if (fill && ! lockMissedThisCall)
                {
                    const auto ms = (float) (track->ownSeconds * 1000.0);
                    spike.tracksMs += ms;
                    ++spike.numTracks;
                    // Kept slowest first among the few.
                    auto at = spike.numSlowest;
                    while (at > 0 && spike.slowest[(size_t) at - 1].ms < ms)
                        --at;
                    if (at < SpikeReport::mostTracks)
                    {
                        for (auto k = juce::jmin (spike.numSlowest, SpikeReport::mostTracks - 1); k > at; --k)
                            spike.slowest[(size_t) k] = spike.slowest[(size_t) k - 1];
                        auto& slow = spike.slowest[(size_t) at];
                        slow = { track->id, ms, track->usualSeconds * 1000.0f, {}, 0 };
                        spike.numSlowest = juce::jmin (spike.numSlowest + 1, SpikeReport::mostTracks);
                        // Its plug-ins, slowest first among the few: the instrument, then each insert.
                        const auto addPlugin = [&slow] (int slotIndex, double seconds, float usual)
                        {
                            const auto pms = (float) (seconds * 1000.0);
                            auto place = slow.numPlugins;
                            while (place > 0 && slow.plugins[(size_t) place - 1].ms < pms)
                                --place;
                            if (place >= SpikeReport::mostPlugins)
                                return;
                            for (auto k = juce::jmin (slow.numPlugins, SpikeReport::mostPlugins - 1); k > place; --k)
                                slow.plugins[(size_t) k] = slow.plugins[(size_t) k - 1];
                            slow.plugins[(size_t) place] = { slotIndex, pms, usual * 1000.0f };
                            slow.numPlugins = juce::jmin (slow.numPlugins + 1, SpikeReport::mostPlugins);
                        };
                        if (track->instrument != nullptr)
                            addPlugin (-1, track->instrumentSeconds, track->instrumentUsual);
                        for (int i = 0; i < (int) track->inserts.size(); ++i)
                            if (const auto& slot = track->inserts[(size_t) i]; slot.plugin != nullptr && ! slot.bypass)
                                addPlugin (i, slot.ownSeconds, slot.usualSeconds);
                    }
                }
                if (! lockMissedThisCall)
                {
                    track->usualSeconds = track->usualSeconds * 0.95f + (float) track->ownSeconds * 0.05f;
                    track->instrumentUsual = track->instrumentUsual * 0.95f + (float) track->instrumentSeconds * 0.05f;
                    for (auto& slot : track->inserts)
                        slot.usualSeconds = slot.usualSeconds * 0.95f + (float) slot.ownSeconds * 0.05f;
                }
            }
        if (fill)
        {
            spike.missedSince = spikesMissed.exchange (0);
            spikeReady.store (true, std::memory_order_release);
        }
    }

    std::optional<Engine::SpikeReport> Engine::takeSpikeReport()
    {
        if (! spikeReady.load (std::memory_order_acquire))
            return std::nullopt;
        auto report = spike;
        spikeReady.store (false, std::memory_order_release);
        return report;
    }

    void Engine::notePerformance (double secondsTaken, int numSamples) noexcept
    {
        const auto rate = getSampleRate();
        if (rate <= 0.0 || numSamples <= 0)
            return;
        const auto load = (float) (secondsTaken / ((double) numSamples / rate));
        // (Written by the audio thread only, so plain loads and stores are enough.)
        averageLoad.store (averageLoad.load() * 0.9f + load * 0.1f);
        if (load > peakLoad.load())
            peakLoad.store (load);
        if (load >= 1.0f)
            ++overloads;
    }

    Engine::Performance Engine::takePerformance()
    {
        return { averageLoad.load(), peakLoad.exchange (0.0f), overloads.load() };
    }

    void Engine::audioDeviceAboutToStart (juce::AudioIODevice* device)
    {
        lastCallTicks = 0;   // (0.57.3: the first call isn't late)
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

    //==========================================================================
    // (0.46.0) The EQ window's spectrum and Auto Listen.
    void Engine::afterEq (int busId, float* const* channels, int numChannels, int numSamples) noexcept
    {
        if (numChannels <= 0 || numSamples <= 0)
            return;
        if (listenBus.load() == busId)
            listenEq.processEq (channels, numChannels, numSamples);
        if (spectrumBus.load() != busId)
            return;
        // Mono into the ring (a full ring: the oldest of this block left out; the screen reads often).
        const auto scope = spectrumFifo.write (juce::jmin (numSamples, spectrumFifo.getFreeSpace()));
        const auto mono = [&] (int i)
        {
            auto v = channels[0][i];
            if (numChannels > 1)
                v = 0.5f * (v + channels[1][i]);
            return v;
        };
        const auto skip = numSamples - (scope.blockSize1 + scope.blockSize2);
        for (int i = 0; i < scope.blockSize1; ++i)
            spectrumRing[(size_t) (scope.startIndex1 + i)] = mono (skip + i);
        for (int i = 0; i < scope.blockSize2; ++i)
            spectrumRing[(size_t) (scope.startIndex2 + i)] = mono (skip + scope.blockSize1 + i);
    }

    int Engine::readSpectrum (std::vector<float>& into)
    {
        const auto ready = spectrumFifo.getNumReady();
        if (ready <= 0 || into.empty())
            return 0;
        const auto take = juce::jmin (ready, (int) into.size());
        // (More than fits: the oldest dropped.)
        if (ready > take)
        {
            const auto drop = spectrumFifo.read (ready - take);
            juce::ignoreUnused (drop);
        }
        const auto scope = spectrumFifo.read (take);
        // Shift what's kept to the left; the new samples at the end.
        std::move (into.begin() + take, into.end(), into.begin());
        auto at = (int) into.size() - take;
        for (int i = 0; i < scope.blockSize1; ++i)
            into[(size_t) at++] = spectrumRing[(size_t) (scope.startIndex1 + i)];
        for (int i = 0; i < scope.blockSize2; ++i)
            into[(size_t) at++] = spectrumRing[(size_t) (scope.startIndex2 + i)];
        return take;
    }

    void Engine::setListen (int busId, const EqDesign& design)
    {
        const juce::ScopedLock sl (lock);
        if (listenBus.load() != busId)
            listenEq.reset();
        listenEq.setDesign (design);
        listenBus = busId;
    }

    void Engine::clearListen()
    {
        listenBus = -1;
    }
}
