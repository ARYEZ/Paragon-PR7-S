#include "DawProject.h"

#include <map>

namespace pr7s::DawProject
{
    namespace
    {
        // ---- .vstpreset ------------------------------------------------------
        // The VST3 SDK's preset file: "VST3", a version, the class id (32
        // characters), the chunk list's offset; the list ("List", a count, then
        // per chunk its id, offset and size); chunks "Comp" (the component's
        // state) and "Cont" (the controller's). Little-endian.
        struct PresetChunks { juce::MemoryBlock component, controller; };

        std::optional<PresetChunks> readPreset (const juce::MemoryBlock& data)
        {
            const auto* bytes = static_cast<const char*> (data.getData());
            const auto size = (juce::int64) data.getSize();
            constexpr juce::int64 headerSize = 4 + 4 + 32 + 8;
            if (size < headerSize || std::memcmp (bytes, "VST3", 4) != 0)
                return std::nullopt;
            const auto listAt = (juce::int64) juce::ByteOrder::littleEndianInt64 (bytes + 40);
            if (listAt < headerSize || listAt + 8 > size || std::memcmp (bytes + listAt, "List", 4) != 0)
                return std::nullopt;
            const auto count = (int) juce::ByteOrder::littleEndianInt (bytes + listAt + 4);
            PresetChunks chunks;
            for (int i = 0; i < count; ++i)
            {
                const auto entry = listAt + 8 + (juce::int64) i * 20;
                if (entry + 20 > size)
                    break;
                const auto offset = (juce::int64) juce::ByteOrder::littleEndianInt64 (bytes + entry + 4);
                const auto length = (juce::int64) juce::ByteOrder::littleEndianInt64 (bytes + entry + 12);
                if (offset < 0 || length < 0 || offset + length > size)
                    continue;
                if (std::memcmp (bytes + entry, "Comp", 4) == 0)
                    chunks.component = juce::MemoryBlock (bytes + offset, (size_t) length);
                else if (std::memcmp (bytes + entry, "Cont", 4) == 0)
                    chunks.controller = juce::MemoryBlock (bytes + offset, (size_t) length);
            }
            return chunks;
        }

        // ---- the song's XML -------------------------------------------------
        std::vector<juce::XmlElement*> children (const juce::XmlElement* e, const juce::String& name = {})
        {
            std::vector<juce::XmlElement*> list;
            if (e != nullptr)
                for (auto* child : e->getChildIterator())
                    if (name.isEmpty() || child->hasTagName (name))
                        list.push_back (child);
            return list;
        }

        double attr (const juce::XmlElement* e, const char* name, double fallback)
        {
            return e != nullptr && e->hasAttribute (name) ? e->getDoubleAttribute (name, fallback) : fallback;
        }

        // Volume in dB: "linear" is a gain (1 = 0 dB), "decibel" dB.
        float volumeDb (const juce::XmlElement* p)
        {
            if (p == nullptr)
                return 0.0f;
            const auto value = attr (p, "value", 1.0);
            const auto unit = p->getStringAttribute ("unit", "linear");
            const auto db = unit == "decibel" ? value : value > 0.0 ? 20.0 * std::log10 (value) : -60.0;
            return (float) juce::jlimit (-60.0, 6.0, db);
        }

        // Pan, -1 (left) to 1: "normalized" is 0 to 1; otherwise by its min and max.
        float panValue (const juce::XmlElement* p)
        {
            if (p == nullptr)
                return 0.0f;
            const auto value = attr (p, "value", 0.5);
            const auto min = attr (p, "min", p->getStringAttribute ("unit") == "normalized" ? 0.0 : -1.0);
            const auto max = attr (p, "max", 1.0);
            const auto travel = max > min ? (value - min) / (max - min) : 0.5;
            return (float) juce::jlimit (-1.0, 1.0, travel * 2.0 - 1.0);
        }

        juce::Colour colourOf (const juce::XmlElement* e, juce::Colour fallback)
        {
            const auto text = e->getStringAttribute ("color").trim().trimCharactersAtStart ("#");
            if (text.length() < 6)
                return fallback;
            return FamilyColours::nearest (juce::Colour ((juce::uint32) (0xff000000u | (juce::uint32) text.substring (0, 6).getHexValue32())));
        }

        // What an automation target (a Parameter element's id) moves.
        struct ParameterInfo
        {
            int busId = 0;
            AutomationLane::Target target = AutomationLane::Target::volume;
            int slot = 0;
            juce::String vst3Id;   // a plug-in's
            juce::String name;
            double min = 0.0, max = 1.0;
            juce::String unit;
        };

        struct Reader
        {
            const juce::File& source;
            juce::ZipFile& zip;
            const juce::File& audioFolder;
            const juce::Array<juce::PluginDescription>& known;
            Imported result;
            TempoMap tempo { 120.0 };   // the song's (its tempo changes too: 0.10.0), for times in seconds
            std::map<juce::String, juce::XmlElement*> byId;
            std::map<juce::String, int> trackIdOf;    // Track or Channel element id -> our bus id (0: the master)
            std::map<juce::String, ParameterInfo> parameters;
            std::map<juce::String, juce::String> extracted;   // path in the zip (or outside) -> file in the audio folder
            struct Pending { int trackId; juce::String destination; std::vector<juce::XmlElement*> sends; };
            std::vector<Pending> pending;
            int nextId = 1;
            juce::StringArray skippedDevices;

            Reader (const juce::File& s, juce::ZipFile& z, const juce::File& a, const juce::Array<juce::PluginDescription>& k)
                : source (s), zip (z), audioFolder (a), known (k) {}

            void note (const juce::String& text) { result.notes.addIfNotAlreadyThere (text); }

            void index (juce::XmlElement* e)
            {
                if (e->hasAttribute ("id"))
                    byId[e->getStringAttribute ("id")] = e;
                for (auto* child : e->getChildIterator())
                    index (child);
            }

            std::optional<juce::MemoryBlock> fileData (const juce::XmlElement* fileRef)
            {
                if (fileRef == nullptr)
                    return std::nullopt;
                const auto path = fileRef->getStringAttribute ("path");
                juce::MemoryBlock data;
                if (fileRef->getBoolAttribute ("external", false))
                {
                    const auto file = juce::File::isAbsolutePath (path) ? juce::File (path) : source.getParentDirectory().getChildFile (path);
                    if (! file.loadFileAsData (data))
                        return std::nullopt;
                    return data;
                }
                if (const auto* entry = zip.getEntry (path))
                    if (std::unique_ptr<juce::InputStream> stream { zip.createStreamForEntry (*entry) })
                    {
                        stream->readIntoMemoryBlock (data);
                        return data;
                    }
                return std::nullopt;
            }

            // A time `value` after `originBeat`, in beats: seconds go through the
            // tempo map from there (so after a tempo change they still land right).
            double toBeats (double value, const juce::String& unit, double originBeat = 0.0) const
            {
                return unit == "seconds" ? tempo.beatAfter (originBeat, value) - originBeat : value;
            }
            double secondsPerBeatAt (double beat) const { return 60.0 / tempo.bpmAt (beat); }

            // ---- plug-ins --------------------------------------------------
            std::optional<juce::PluginDescription> findPlugin (const juce::String& name, const juce::String& vendor) const
            {
                for (const auto& d : known)
                    if (d.pluginFormatName == "VST3" && d.name.equalsIgnoreCase (name) && (vendor.isEmpty() || d.manufacturerName.equalsIgnoreCase (vendor)))
                        return d;
                for (const auto& d : known)
                    if (d.pluginFormatName == "VST3" && d.name.equalsIgnoreCase (name))
                        return d;
                return std::nullopt;
            }

            juce::PluginDescription describe (const juce::XmlElement* device, const juce::String& where, bool instrument)
            {
                const auto name = device->getStringAttribute ("deviceName", device->getStringAttribute ("name"));
                const auto vendor = device->getStringAttribute ("deviceVendor");
                if (auto found = findPlugin (name, vendor))
                    return *found;
                note (where + ": " + name + " isn't among the plug-ins found on this computer (PLUGINS > SCAN). Its settings are kept for when it is.");
                juce::PluginDescription d;
                d.name = name;
                d.descriptiveName = name;
                d.manufacturerName = vendor;
                d.pluginFormatName = "VST3";
                d.isInstrument = instrument;
                return d;
            }

            juce::MemoryBlock stateOf (const juce::XmlElement* device, const juce::String& where)
            {
                const auto* stateRef = device->getChildByName ("State");
                if (stateRef == nullptr)
                    return {};
                const auto data = fileData (stateRef);
                if (! data)
                {
                    note (where + ": its settings file is missing from the DAWproject (it starts with its own defaults).");
                    return {};
                }
                auto state = vstPresetToHostState (*data);
                if (state.isEmpty())
                    note (where + ": its settings aren't in the VST3 preset format (it starts with its own defaults).");
                return state;
            }

            // The parameters a device lists (those that are automated): matched by id later.
            void indexDeviceParameters (const juce::XmlElement* device, int busId, AutomationLane::Target target, int slot)
            {
                for (auto* p : children (device->getChildByName ("Parameters")))
                    if (p->hasAttribute ("id"))
                    {
                        ParameterInfo info;
                        info.busId = busId;
                        info.target = target;
                        info.slot = slot;
                        info.vst3Id = p->getStringAttribute ("parameterID");
                        info.name = p->getStringAttribute ("name");
                        info.min = attr (p, "min", 0.0);
                        info.max = attr (p, "max", 1.0);
                        info.unit = p->getStringAttribute ("unit", "normalized");
                        parameters[p->getStringAttribute ("id")] = info;
                    }
            }

            // A channel's devices: its instrument (if it takes one) and its inserts.
            void readDevices (const juce::XmlElement* channel, int busId, const juce::String& where, Instrument* instrument, std::vector<Insert>& inserts)
            {
                auto foundInstrument = false;
                for (auto* device : children (channel->getChildByName ("Devices")))
                {
                    const auto role = device->getStringAttribute ("deviceRole");
                    const auto name = device->getStringAttribute ("deviceName", device->getTagName());
                    if (! device->hasTagName ("Vst3Plugin"))
                    {
                        note (where + ": " + name + " (" + device->getTagName() + ") can't come across: only VST3 plug-ins do"
                              + (device->hasTagName ("Equalizer") || device->hasTagName ("Compressor") || device->hasTagName ("NoiseGate")
                                 || device->hasTagName ("Limiter") || device->hasTagName ("BuiltinDevice")
                                     ? " (a built-in device, such as the channel strip's)." : "."));
                        continue;
                    }
                    if (role == "instrument" && instrument != nullptr && ! foundInstrument)
                    {
                        instrument->kind = Instrument::Kind::plugin;
                        instrument->plugin = describe (device, where, true);
                        instrument->state = stateOf (device, where + " (" + name + ")");
                        indexDeviceParameters (device, busId, AutomationLane::Target::instrument, 0);
                        foundInstrument = true;
                        continue;
                    }
                    if (role == "noteFX")
                    {
                        note (where + ": " + name + " (a MIDI effect) can't come across.");
                        continue;
                    }
                    if ((int) inserts.size() >= Project::maxInserts)
                    {
                        note (where + ": more than " + juce::String (Project::maxInserts) + " effects; " + name + " and after left out.");
                        continue;
                    }
                    Insert insert;
                    insert.plugin = describe (device, where, false);
                    insert.state = stateOf (device, where + " (" + name + ")");
                    if (const auto* enabled = device->getChildByName ("Enabled"))
                        insert.bypass = ! enabled->getBoolAttribute ("value", true);
                    indexDeviceParameters (device, busId, AutomationLane::Target::insert, (int) inserts.size());
                    inserts.push_back (std::move (insert));
                }
            }

            void indexMixParameters (const juce::XmlElement* channel, int busId)
            {
                const auto add = [&] (const juce::XmlElement* p, AutomationLane::Target target, int slot)
                {
                    if (p == nullptr || ! p->hasAttribute ("id"))
                        return;
                    ParameterInfo info;
                    info.busId = busId;
                    info.target = target;
                    info.slot = slot;
                    info.min = attr (p, "min", 0.0);
                    info.max = attr (p, "max", 1.0);
                    info.unit = p->getStringAttribute ("unit");
                    parameters[p->getStringAttribute ("id")] = info;
                };
                add (channel->getChildByName ("Volume"), AutomationLane::Target::volume, 0);
                add (channel->getChildByName ("Pan"), AutomationLane::Target::pan, 0);
                add (channel->getChildByName ("Mute"), AutomationLane::Target::mute, 0);
            }

            // ---- the tracks ---------------------------------------------------
            void readTrack (juce::XmlElement* t, int index)
            {
                const auto* channel = t->getChildByName ("Channel");
                const auto role = channel != nullptr ? channel->getStringAttribute ("role", "regular") : juce::String();
                const auto content = juce::StringArray::fromTokens (t->getStringAttribute ("contentType"), " ", {});
                const auto name = t->getStringAttribute ("name", "Track");
                const auto nested = children (t, "Track");

                if (role == "master")
                {
                    trackIdOf[t->getStringAttribute ("id")] = 0;
                    trackIdOf[channel->getStringAttribute ("id")] = 0;
                    auto& p = result.project;
                    p.masterVolumeDb = volumeDb (channel->getChildByName ("Volume"));
                    readDevices (channel, 0, "Master", nullptr, p.masterInserts);
                    indexMixParameters (channel, 0);
                    if (std::abs (panValue (channel->getChildByName ("Pan"))) > 0.01f)
                        note ("The master's pan isn't kept (PR7-S's master has none).");
                    return;
                }

                // A folder: its tracks, flattened; with a group channel of its own, that's a group too.
                if (! nested.empty() && role != "submix")
                {
                    note ("The folder \"" + name + "\" is flattened: its tracks come across without it (PR7-S has no folders yet).");
                    for (auto* child : nested)
                        readTrack (child, index++);
                    return;
                }

                Track track;
                track.id = nextId++;
                track.name = name;
                track.colour = colourOf (t, trackColour (track.id - 1));
                if (role == "effect")
                    track.kind = Track::Kind::fx;
                else if (role == "submix")
                    track.kind = Track::Kind::group;
                else if (content.contains ("notes"))
                    track.kind = Track::Kind::instrument;
                else if (content.contains ("audio"))
                    track.kind = Track::Kind::audio;
                else if (content.contains ("markers"))
                {
                    note ("Markers (\"" + name + "\") aren't brought across yet.");
                    --nextId;
                    return;
                }
                else
                    track.kind = channel != nullptr ? Track::Kind::audio : Track::Kind::instrument;

                trackIdOf[t->getStringAttribute ("id")] = track.id;
                if (channel != nullptr)
                {
                    trackIdOf[channel->getStringAttribute ("id")] = track.id;
                    track.volumeDb = volumeDb (channel->getChildByName ("Volume"));
                    track.pan = panValue (channel->getChildByName ("Pan"));
                    if (const auto* mute = channel->getChildByName ("Mute"))
                        track.mute = mute->getBoolAttribute ("value", false);
                    track.solo = channel->getBoolAttribute ("solo", false);
                    track.stereoInput = channel->getIntAttribute ("audioChannels", 2) >= 2;
                    readDevices (channel, track.id, name, track.kind == Track::Kind::instrument ? &track.instrument : nullptr, track.inserts);
                    indexMixParameters (channel, track.id);
                    Pending later { track.id, channel->getStringAttribute ("destination"), {} };
                    for (auto* send : children (channel->getChildByName ("Sends"), "Send"))
                        later.sends.push_back (send);
                    pending.push_back (later);
                }
                if (track.kind == Track::Kind::instrument && track.instrument.kind == Instrument::Kind::paragonSynth)
                    note (name + ": no VST3 instrument came with it; it plays PR7-K, the studio's synth.");
                result.project.tracks.push_back (std::move (track));
                // A group with tracks inside: they follow it.
                for (auto* child : nested)
                    readTrack (child, index++);
            }

            void resolveRouting()
            {
                for (const auto& p : pending)
                {
                    auto* track = result.project.findTrack (p.trackId);
                    if (track == nullptr)
                        continue;
                    if (p.destination.isNotEmpty())
                    {
                        const auto to = trackIdOf.find (p.destination);
                        const auto* target = to != trackIdOf.end() ? result.project.findTrack (to->second) : nullptr;
                        if (target != nullptr && target->isGroup() && ! track->isChannel())
                            track->output = target->id;
                        else if (target != nullptr && to->second != 0)
                            note (track->name + ": its output (" + target->name + ") isn't a group PR7-S can route it to; it goes to the master.");
                    }
                    for (auto* s : p.sends)
                    {
                        const auto to = trackIdOf.find (s->getStringAttribute ("destination"));
                        const auto* target = to != trackIdOf.end() ? result.project.findTrack (to->second) : nullptr;
                        if (target == nullptr || ! target->isFx())
                        {
                            note (track->name + ": a send that doesn't go to an FX channel is left out.");
                            continue;
                        }
                        if ((int) track->sends.size() >= Project::maxSends || track->isFx())
                        {
                            note (track->name + ": more sends than PR7-S has (" + juce::String (Project::maxSends) + "); the rest are left out.");
                            continue;
                        }
                        Send send;
                        send.target = target->id;
                        send.levelDb = volumeDb (s->getChildByName ("Volume"));
                        send.preFader = s->getStringAttribute ("type", "post") == "pre";
                        if (const auto* enable = s->getChildByName ("Enable"))
                            send.on = enable->getBoolAttribute ("value", true);
                        if (const auto* v = s->getChildByName ("Volume"); v != nullptr && v->hasAttribute ("id"))
                        {
                            ParameterInfo info;
                            info.busId = track->id;
                            info.target = AutomationLane::Target::send;
                            info.slot = (int) track->sends.size();
                            info.unit = v->getStringAttribute ("unit");
                            parameters[v->getStringAttribute ("id")] = info;
                        }
                        track->sends.push_back (send);
                    }
                }
            }

            // ---- the arrangement ------------------------------------------------
            // Where content is placed: its beats plus `shift`, heard from `from` to `to` (song beats).
            struct Window { double shift = 0.0, from = 0.0, to = 1.0e12; };

            struct Sink
            {
                Track* track = nullptr;
                Part* part = nullptr;   // notes go here (MIDI)
                juce::String clipName;
                double fadeIn = 0.0, fadeOut = 0.0;
                bool muted = false;
            };

            juce::String audioFileFor (const juce::XmlElement* audio)
            {
                const auto* fileRef = audio->getChildByName ("File");
                if (fileRef == nullptr)
                    return {};
                const auto key = fileRef->getStringAttribute ("path") + (fileRef->getBoolAttribute ("external", false) ? "!" : "");
                if (const auto done = extracted.find (key); done != extracted.end())
                    return done->second;
                const auto data = fileData (fileRef);
                if (! data)
                {
                    note ("The audio file " + fileRef->getStringAttribute ("path") + " isn't in the DAWproject; its clips stay, silent.");
                    return extracted[key] = juce::File (fileRef->getStringAttribute ("path")).getFileName();
                }
                audioFolder.createDirectory();
                auto target = audioFolder.getNonexistentChildFile (juce::File (fileRef->getStringAttribute ("path").replaceCharacter ('\\', '/')).getFileNameWithoutExtension(),
                                                                    juce::File (fileRef->getStringAttribute ("path")).getFileExtension(), false);
                target.replaceWithData (data->getData(), data->getSize());
                return extracted[key] = target.getFileName();
            }

            // An audio segment: from `w.from` to `w.to`, the file from `offsetSeconds`.
            void addAudio (Sink& sink, const juce::XmlElement* audio, const Window& w, double offsetSeconds)
            {
                if (sink.track == nullptr || ! sink.track->isAudio())
                {
                    note ("Audio on a track that isn't an audio track is left out.");
                    return;
                }
                const auto file = audioFileFor (audio);
                if (file.isEmpty())
                    return;
                const auto fileSeconds = attr (audio, "duration", 1.0e9);
                const auto end = juce::jmin (w.to, tempo.beatAfter (w.from, juce::jmax (0.0, fileSeconds - offsetSeconds)));
                if (end <= w.from + 1.0e-6 || w.from >= 1.0e11)
                    return;
                Part part;
                part.start = w.from;
                part.length = end - w.from;
                part.name = sink.clipName;
                part.audioFile = file;
                part.audioOffset = juce::jmax (0.0, offsetSeconds);
                part.fadeIn = sink.fadeIn;
                part.fadeOut = sink.fadeOut;
                part.muted = sink.muted;
                sink.track->parts.push_back (part);
            }

            void walk (const juce::XmlElement* e, const juce::String& unitIn, const Window& w, Sink& outerSink)
            {
                const auto unit = e->getStringAttribute ("timeUnit", unitIn);
                // Any timeline can say whose track it's on (Lanes, Clips, Points...): from here down, that track.
                Sink scoped;
                if (e->hasAttribute ("track"))
                    if (const auto found = trackIdOf.find (e->getStringAttribute ("track")); found != trackIdOf.end())
                        scoped.track = found->second == 0 ? nullptr : result.project.findTrack (found->second);
                Sink& sink = e->hasAttribute ("track") ? scoped : outerSink;
                if (e->hasTagName ("Notes"))
                {
                    if (sink.part == nullptr)
                    {
                        if (sink.track != nullptr && ! sink.track->isAudio())
                            note ("Notes outside a part are left out.");
                        return;
                    }
                    for (auto* n : children (e, "Note"))
                    {
                        const auto start = toBeats (attr (n, "time", 0.0), unit, w.shift) + w.shift;
                        auto end = start + toBeats (attr (n, "duration", 0.25), unit, start);
                        if (start < w.from - 1.0e-9 || start >= w.to - 1.0e-9)
                            continue;
                        end = juce::jmin (end, w.to);
                        Note note;
                        note.start = start - sink.part->start;
                        note.length = juce::jmax (1.0 / 128.0, end - start);
                        note.pitch = juce::jlimit (0, 127, n->getIntAttribute ("key", 60));
                        note.velocity = (float) juce::jlimit (1.0 / 127.0, 1.0, attr (n, "vel", 0.8));
                        sink.part->notes.push_back (note);
                    }
                    const auto list = children (e, "Note");
                    if (std::any_of (list.begin(), list.end(), [] (auto* n) { return n->getNumChildElements() > 0; }))
                        note ("Note expressions aren't brought across (the notes are).");
                    return;
                }
                if (e->hasTagName ("Audio"))
                {
                    // As the content itself: its time is seconds into the file.
                    addAudio (sink, e, w, tempo.secondsBetween (w.shift, w.from));
                    return;
                }
                if (e->hasTagName ("Warps"))
                {
                    const auto* audio = e->getChildByName ("Audio");
                    const auto warps = children (e, "Warp");
                    if (audio == nullptr || warps.empty())
                        return;
                    // Clip time (its unit) to seconds in the file, by the first two warps.
                    const auto t0 = attr (warps[0], "time", 0.0), c0 = attr (warps[0], "contentTime", 0.0);
                    auto slope = 1.0;
                    if (warps.size() >= 2 && attr (warps[1], "time", 0.0) > t0)
                        slope = (attr (warps[1], "contentTime", 0.0) - c0) / (attr (warps[1], "time", 0.0) - t0);
                    const auto contentUnit = e->getStringAttribute ("contentTimeUnit", "seconds");
                    // Seconds per unit of the clip's time, as played without stretching.
                    const auto spb = secondsPerBeatAt (w.from);
                    const auto natural = unit == "beats" ? spb : 1.0;
                    const auto slopeSeconds = contentUnit == "beats" ? slope * spb : slope;
                    if (std::abs (slopeSeconds - natural) > natural * 0.01)
                        note ("Time-stretched audio is played at its own speed (PR7-S doesn't stretch yet), so it may not line up.");
                    const auto clipTime = unit == "seconds" ? tempo.secondsBetween (w.shift, w.from) : w.from - w.shift;   // in the warps' unit
                    const auto contentAt = c0 + (clipTime - t0) * slope;
                    addAudio (sink, audio, w, contentUnit == "beats" ? contentAt * spb : contentAt);
                    return;
                }
                if (e->hasTagName ("Clips"))
                {
                    for (auto* clip : children (e, "Clip"))
                        walkClip (clip, unit, w, sink);
                    return;
                }
                if (e->hasTagName ("Points"))
                {
                    readPoints (e, unit, w);
                    return;
                }
                if (e->hasTagName ("Markers"))
                {
                    note ("Markers aren't brought across yet.");
                    return;
                }
                if (e->hasTagName ("Lanes"))
                {
                    for (auto* child : e->getChildIterator())
                        walk (child, unit, w, sink);
                    return;
                }
                if (e->hasTagName ("Video"))
                    note ("Video isn't brought across.");
            }

            void walkClip (const juce::XmlElement* clip, const juce::String& unit, const Window& w, Sink& outer)
            {
                const auto start = toBeats (attr (clip, "time", 0.0), unit, w.shift) + w.shift;
                const auto contentUnit = clip->getStringAttribute ("contentTimeUnit", unit);
                const auto playStart = toBeats (attr (clip, "playStart", 0.0), contentUnit, start);
                const auto duration = clip->hasAttribute ("duration") ? toBeats (attr (clip, "duration", 0.0), unit, start)
                                    : clip->hasAttribute ("playStop") ? toBeats (attr (clip, "playStop", 0.0), contentUnit, start) - playStart
                                                                      : 4.0;
                const auto from = juce::jmax (start, w.from), to = juce::jmin (start + duration, w.to);
                if (to <= from + 1.0e-9)
                    return;
                if (clip->hasAttribute ("enable") && ! clip->getBoolAttribute ("enable", true))
                    if (outer.part == nullptr)
                        outer.muted = true;

                // The top clip on a track: a part (MIDI), or the name and fades its audio takes.
                Sink sink = outer;
                std::unique_ptr<Part> made;
                if (outer.part == nullptr && outer.track != nullptr)
                {
                    sink.clipName = clip->getStringAttribute ("name");
                    const auto fadeUnit = clip->getStringAttribute ("fadeTimeUnit", unit);
                    const auto fadeSeconds = [&] (const char* name)
                    {
                        const auto value = juce::jmax (0.0, attr (clip, name, 0.0));
                        return fadeUnit == "seconds" ? value : tempo.secondsBetween (start, start + value);
                    };
                    sink.fadeIn = fadeSeconds ("fadeInTime");
                    sink.fadeOut = fadeSeconds ("fadeOutTime");
                    sink.muted = clip->hasAttribute ("enable") && ! clip->getBoolAttribute ("enable", true);
                    if (! outer.track->isAudio())
                    {
                        made = std::make_unique<Part>();
                        made->start = from;
                        made->length = to - from;
                        made->name = sink.clipName;
                        made->muted = sink.muted;
                        sink.part = made.get();
                    }
                }

                // The content, once, or looped: from playStart to the loop's end, then the loop again and again.
                const juce::XmlElement* content = clip->getFirstChildElement();   // (a clip's one child is its content)
                if (clip->hasAttribute ("reference"))
                    if (const auto found = byId.find (clip->getStringAttribute ("reference")); found != byId.end())
                        content = found->second;
                if (content != nullptr)
                {
                    const auto loopStart = clip->hasAttribute ("loopStart") ? toBeats (attr (clip, "loopStart", 0.0), contentUnit, start) : 0.0;
                    const auto loopEnd = clip->hasAttribute ("loopEnd") ? toBeats (attr (clip, "loopEnd", 0.0), contentUnit, start) : 0.0;
                    const auto loopLength = loopEnd - loopStart;
                    if (loopLength > 1.0e-6 && playStart < loopEnd && start + (loopEnd - playStart) < to - 1.0e-6)
                    {
                        // The first pass, then the loop.
                        walk (content, contentUnit, { start - playStart, from, juce::jmin (to, start + loopEnd - playStart) }, sink);
                        auto passStart = start + (loopEnd - playStart);
                        for (int pass = 0; passStart < to - 1.0e-6 && pass < 4096; ++pass, passStart += loopLength)
                            walk (content, contentUnit, { passStart - loopStart, juce::jmax (from, passStart), juce::jmin (to, passStart + loopLength) }, sink);
                    }
                    else
                        walk (content, contentUnit, { start - playStart, from, to }, sink);
                }
                if (made != nullptr)
                {
                    std::sort (made->notes.begin(), made->notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
                    outer.track->parts.push_back (*made);
                }
            }

            // ---- automation -----------------------------------------------------
            void readPoints (const juce::XmlElement* e, const juce::String& unit, const Window& w)
            {
                const auto* target = e->getChildByName ("Target");
                if (target == nullptr)
                    return;
                const auto parameterId = target->getStringAttribute ("parameter");
                if (parameterId.isEmpty())
                {
                    if (target->hasAttribute ("expression"))
                        note ("MIDI controller and expression automation isn't brought across yet.");
                    return;
                }
                const auto found = parameters.find (parameterId);
                if (found == parameters.end())
                {
                    const auto* p = byId.count (parameterId) > 0 ? byId[parameterId] : nullptr;
                    if (p != nullptr && p->hasTagName ("Tempo"))
                        return;   // (the tempo's changes: read into the tempo track first, readTempo)
                    if (p != nullptr && p->hasTagName ("TimeSignature"))
                        note ("Time signature changes aren't brought across yet: the song keeps its first signature.");
                    else
                        note ("Automation of a control PR7-S doesn't have is left out.");
                    return;
                }
                const auto& info = found->second;
                AutomationLane lane;
                lane.target = info.target;
                lane.slot = info.slot;
                lane.parameterName = info.name;
                const auto unitOfValues = e->getStringAttribute ("unit", info.unit);
                for (auto* point : e->getChildIterator())
                {
                    if (! point->hasAttribute ("time") || ! point->hasAttribute ("value"))
                        continue;
                    const auto beat = toBeats (attr (point, "time", 0.0), unit, w.shift) + w.shift;
                    float travel = 0.0f;
                    if (point->hasTagName ("BoolPoint"))
                        travel = point->getBoolAttribute ("value") ? 1.0f : 0.0f;
                    else
                    {
                        const auto v = attr (point, "value", 0.0);
                        switch (info.target)
                        {
                            case AutomationLane::Target::volume:
                            case AutomationLane::Target::send:
                            {
                                const auto db = unitOfValues == "decibel" ? v : v > 0.0 ? 20.0 * std::log10 (v) : -60.0;
                                travel = volumeToTravel ((float) juce::jlimit (-60.0, 6.0, db));
                                break;
                            }
                            case AutomationLane::Target::mute:
                                travel = v >= 0.5 ? 1.0f : 0.0f;
                                break;
                            default:
                                travel = (float) (unitOfValues == "normalized" || info.max <= info.min ? v : (v - info.min) / (info.max - info.min));
                                break;
                        }
                    }
                    lane.points.push_back ({ juce::jmax (0.0, beat), juce::jlimit (0.0f, 1.0f, travel) });
                }
                if (lane.points.empty())
                    return;
                std::sort (lane.points.begin(), lane.points.end(), [] (const auto& a, const auto& b) { return a.beat < b.beat; });
                if (info.target == AutomationLane::Target::instrument || info.target == AutomationLane::Target::insert)
                {
                    result.pluginLanes.push_back ({ info.busId, lane, info.vst3Id });
                    return;
                }
                addLane (info.busId, std::move (lane));
            }

            void addLane (int busId, AutomationLane lane)
            {
                if (busId == 0)
                {
                    if (lane.target != AutomationLane::Target::volume)
                        return;
                    result.project.masterAutomation.push_back (std::move (lane));
                    result.project.masterRead = true;
                    return;
                }
                if (auto* track = result.project.findTrack (busId))
                {
                    track->automation.push_back (std::move (lane));
                    track->automationRead = true;
                }
            }
        };
    }

    juce::MemoryBlock vstPresetToHostState (const juce::MemoryBlock& data)
    {
        const auto chunks = readPreset (data);
        if (! chunks || (chunks->component.isEmpty() && chunks->controller.isEmpty()))
            return {};
        // JUCE's VST3 host: <VST3PluginState> with the component's and the controller's states in base64.
        juce::XmlElement state ("VST3PluginState");
        if (! chunks->component.isEmpty())
            state.createNewChildElement ("IComponent")->addTextElement (chunks->component.toBase64Encoding());
        if (! chunks->controller.isEmpty())
            state.createNewChildElement ("IEditController")->addTextElement (chunks->controller.toBase64Encoding());
        juce::MemoryBlock out;
        juce::AudioProcessor::copyXmlToBinary (state, out);
        return out;
    }

    std::optional<Imported> read (const juce::File& file, const juce::File& audioFolder,
                                  const juce::Array<juce::PluginDescription>& knownPlugins, juce::String& error)
    {
        juce::ZipFile zip (file);
        const auto* entry = zip.getEntry ("project.xml");
        if (entry == nullptr)
        {
            error = file.getFileName() + " isn't a DAWproject (it has no project.xml inside).";
            return std::nullopt;
        }
        std::unique_ptr<juce::InputStream> stream { zip.createStreamForEntry (*entry) };
        const auto xml = stream != nullptr ? juce::XmlDocument::parse (stream->readEntireStreamAsString()) : nullptr;
        if (xml == nullptr || ! xml->hasTagName ("Project"))
        {
            error = "The song inside " + file.getFileName() + " couldn't be read.";
            return std::nullopt;
        }

        Reader reader (file, zip, audioFolder, knownPlugins);
        reader.index (xml.get());
        auto& project = reader.result.project;
        project.tracks.clear();
        if (const auto* app = xml->getChildByName ("Application"))
            reader.result.application = (app->getStringAttribute ("name") + " " + app->getStringAttribute ("version")).trim();

        // The tempo and the time signature (the first; changes are noted with the automation).
        if (const auto* transport = xml->getChildByName ("Transport"))
        {
            if (const auto* tempo = transport->getChildByName ("Tempo"))
                project.bpm = juce::jlimit (20.0, 400.0, attr (tempo, "value", 120.0));
            if (const auto* signature = transport->getChildByName ("TimeSignature"))
            {
                project.numerator = juce::jlimit (1, 32, signature->getIntAttribute ("numerator", 4));
                project.denominator = juce::jlimit (1, 32, signature->getIntAttribute ("denominator", 4));
            }
        }
        reader.tempo = TempoMap (project.bpm);
        // Its tempo changes (0.10.0): onto the tempo track, and the times in seconds by them.
        // (The tempo's points: the arrangement's TempoAutomation, or any Points
        // aimed at the transport's Tempo; the official library writes either.)
        const auto* tempoParameter = xml->getChildByName ("Transport") != nullptr ? xml->getChildByName ("Transport")->getChildByName ("Tempo") : nullptr;
        const juce::XmlElement* tempoPoints = nullptr;
        juce::String tempoUnitFromLanes = "beats";
        if (const auto* arrangement = xml->getChildByName ("Arrangement"))
        {
            tempoPoints = arrangement->getChildByName ("TempoAutomation");
            const auto tempoId = tempoParameter != nullptr ? tempoParameter->getStringAttribute ("id") : juce::String();
            std::function<void (const juce::XmlElement*, const juce::String&)> find = [&] (const juce::XmlElement* e, const juce::String& unit)
            {
                for (auto* child : e->getChildIterator())
                {
                    if (tempoPoints != nullptr)
                        return;
                    const auto childUnit = child->getStringAttribute ("timeUnit", unit);
                    if (child->hasTagName ("Points") && tempoId.isNotEmpty())
                        if (const auto* target = child->getChildByName ("Target"); target != nullptr && target->getStringAttribute ("parameter") == tempoId)
                        {
                            tempoPoints = child;
                            tempoUnitFromLanes = childUnit;
                            return;
                        }
                    find (child, childUnit);
                }
            };
            if (tempoPoints == nullptr)
                find (arrangement, arrangement->getStringAttribute ("timeUnit", "beats"));
        }
        {
            if (const auto* points = tempoPoints)
            {
                const auto low = attr (tempoParameter, "min", 20.0), high = attr (tempoParameter, "max", 999.0);
                const auto timeUnit = points->getStringAttribute ("timeUnit", tempoUnitFromLanes);
                const auto normalized = points->getStringAttribute ("unit", "bpm") == "normalized";
                std::vector<const juce::XmlElement*> list;
                for (auto* p : points->getChildIterator())
                    if (p->hasTagName ("RealPoint"))
                        list.push_back (p);
                std::stable_sort (list.begin(), list.end(), [] (auto* a, auto* b) { return attr (a, "time", 0.0) < attr (b, "time", 0.0); });
                std::vector<TempoEvent> events;
                auto lastTime = 0.0, lastBeat = 0.0, lastBpm = project.bpm;
                auto lastLinear = false;
                for (const auto* p : list)
                {
                    const auto time = juce::jmax (0.0, attr (p, "time", 0.0));
                    auto bpm = attr (p, "value", project.bpm);
                    if (normalized)
                        bpm = low + bpm * (high - low);
                    bpm = juce::jlimit (Project::minBpm, Project::maxBpm, bpm);
                    // Seconds to beats as it goes (a ramp in seconds: the average tempo over it).
                    const auto beat = timeUnit == "seconds" ? lastBeat + (time - lastTime) * (lastLinear ? (lastBpm + bpm) * 0.5 : lastBpm) / 60.0 : time;
                    if (events.empty() && beat > 1.0e-9)
                        events.push_back ({ 0.0, project.bpm, false });
                    events.push_back ({ beat, bpm, lastLinear && ! events.empty() });
                    lastLinear = p->getStringAttribute ("interpolation", "hold") == "linear";
                    lastTime = time;
                    lastBeat = beat;
                    lastBpm = bpm;
                }
                const auto changes = std::any_of (events.begin(), events.end(), [&] (const TempoEvent& e) { return std::abs (e.bpm - events.front().bpm) > 1.0e-6; });
                if (changes)
                {
                    project.tempoEvents = events;
                    project.tempoTrackShown = true;
                    project.tempoTrackOn = true;
                    project.bpm = events.front().bpm;
                    reader.tempo = TempoMap (events);
                }
            }
        }

        auto index = 0;
        for (auto* track : children (xml->getChildByName ("Structure"), "Track"))
            reader.readTrack (track, index++);
        for (auto* channel : children (xml->getChildByName ("Structure"), "Channel"))   // (a master channel on its own)
            if (channel->getStringAttribute ("role") == "master")
            {
                reader.trackIdOf[channel->getStringAttribute ("id")] = 0;
                project.masterVolumeDb = volumeDb (channel->getChildByName ("Volume"));
                reader.readDevices (channel, 0, "Master", nullptr, project.masterInserts);
                reader.indexMixParameters (channel, 0);
            }
        reader.resolveRouting();

        if (const auto* arrangement = xml->getChildByName ("Arrangement"))
        {
            Reader::Sink none;
            for (auto* child : arrangement->getChildIterator())
                reader.walk (child, "beats", {}, none);
            if (arrangement->getChildByName ("TimeSignatureAutomation") != nullptr)
                reader.note ("Time signature changes aren't brought across yet: the song keeps its first signature.");
        }
        if (const auto* scenes = xml->getChildByName ("Scenes"); scenes != nullptr && scenes->getNumChildElements() > 0)
            reader.note ("The clip launcher's scenes aren't brought across.");

        // Parts in order on each track.
        for (auto& track : project.tracks)
            std::sort (track.parts.begin(), track.parts.end(), [] (const Part& a, const Part& b) { return a.start < b.start; });
        // The cycle round the song.
        auto end = 0.0;
        for (const auto& track : project.tracks)
            for (const auto& part : track.parts)
                end = juce::jmax (end, part.end());
        project.loopStart = 0.0;
        project.loopEnd = end > 0.0 ? std::ceil (end / project.beatsPerBar()) * project.beatsPerBar() : 16.0;
        return std::move (reader.result);
    }
}
