#include "Project.h"
#include "Controllers.h"

namespace pr7s
{
    namespace
    {
        // Bump when the file layout changes in a way older versions can't read.
        constexpr int fileVersion = 1;
        const juce::Identifier projectTag { "PR7SProject" }, trackTag { "Track" }, partTag { "Part" }, noteTag { "Note" },
                               stateTag { "InstrumentState" }, insertTag { "Insert" }, insertStateTag { "InsertState" },
                               masterTag { "Master" }, laneTag { "Automation" }, pointTag { "Point" }, sendTag { "Send" };

        // The fader's law, as the mixer's slider has it (-60 to +6 dB, -12 dB half way).
        const juce::NormalisableRange<float> faderRange = []
        {
            juce::NormalisableRange<float> range (-60.0f, 6.0f);
            range.setSkewForCentre (-12.0f);
            return range;
        }();

        const char* targetNames[] = { "volume", "pan", "mute", "instrument", "insert", "send" };

        void lanesToXml (const std::vector<AutomationLane>& lanes, juce::XmlElement& parent)
        {
            for (const auto& lane : lanes)
            {
                auto* e = parent.createNewChildElement (laneTag);
                e->setAttribute ("target", targetNames[(int) lane.target]);
                if (lane.target == AutomationLane::Target::insert || lane.target == AutomationLane::Target::send)
                    e->setAttribute ("slot", lane.slot);
                if (lane.target == AutomationLane::Target::instrument || lane.target == AutomationLane::Target::insert)
                {
                    e->setAttribute ("parameter", lane.parameter);
                    e->setAttribute ("parameterName", lane.parameterName);
                }
                e->setAttribute ("shown", lane.shown);
                e->setAttribute ("muted", lane.muted);
                for (const auto& point : lane.points)
                {
                    auto* p = e->createNewChildElement (pointTag);
                    p->setAttribute ("beat", point.beat);
                    p->setAttribute ("value", point.value);
                    if (std::abs (point.curve) > 1.0e-6f)
                        p->setAttribute ("curve", point.curve);
                    if (point.terminator)
                        p->setAttribute ("terminator", true);
                }
                for (const auto& point : lane.trim)
                {
                    auto* p = e->createNewChildElement ("Trim");
                    p->setAttribute ("beat", point.beat);
                    p->setAttribute ("value", point.value);
                    if (std::abs (point.curve) > 1.0e-6f)
                        p->setAttribute ("curve", point.curve);
                }
            }
        }

        std::vector<AutomationLane> lanesFromXml (const juce::XmlElement& parent)
        {
            std::vector<AutomationLane> lanes;
            for (const auto* e : parent.getChildWithTagNameIterator (laneTag))
            {
                AutomationLane lane;
                const auto target = e->getStringAttribute ("target");
                const auto known = std::find_if (std::begin (targetNames), std::end (targetNames), [&] (const char* n) { return target == n; });
                if (known == std::end (targetNames))
                    continue;   // from a newer version: left out
                lane.target = (AutomationLane::Target) std::distance (std::begin (targetNames), known);
                lane.slot = juce::jlimit (0, Project::maxInserts - 1, e->getIntAttribute ("slot"));
                lane.parameter = juce::jmax (0, e->getIntAttribute ("parameter"));
                lane.parameterName = e->getStringAttribute ("parameterName");
                lane.shown = e->getBoolAttribute ("shown", true);
                lane.muted = e->getBoolAttribute ("muted");
                for (const auto* p : e->getChildWithTagNameIterator (pointTag))
                    lane.points.push_back ({ juce::jmax (0.0, p->getDoubleAttribute ("beat")),
                                             (float) juce::jlimit (0.0, 1.0, p->getDoubleAttribute ("value")),
                                             (float) juce::jlimit (-1.0, 1.0, p->getDoubleAttribute ("curve")),
                                             p->getBoolAttribute ("terminator") });
                for (const auto* p : e->getChildWithTagNameIterator ("Trim"))
                    lane.trim.push_back ({ juce::jmax (0.0, p->getDoubleAttribute ("beat")),
                                           (float) juce::jlimit (0.0, 1.0, p->getDoubleAttribute ("value")),
                                           (float) juce::jlimit (-1.0, 1.0, p->getDoubleAttribute ("curve")), false });
                std::stable_sort (lane.trim.begin(), lane.trim.end(), [] (const auto& a, const auto& b) { return a.beat < b.beat; });
                std::stable_sort (lane.points.begin(), lane.points.end(), [] (const auto& a, const auto& b) { return a.beat < b.beat; });
                lanes.push_back (std::move (lane));
            }
            return lanes;
        }

        void insertsToXml (const std::vector<Insert>& inserts, juce::XmlElement& parent)
        {
            // By slot (an empty one without a plugin); none after the last used.
            auto used = (int) inserts.size();
            while (used > 0 && inserts[(size_t) used - 1].isEmpty())
                --used;
            for (int slot = 0; slot < used; ++slot)
            {
                const auto& insert = inserts[(size_t) slot];
                auto* e = parent.createNewChildElement (insertTag);
                if (insert.isEmpty())
                    continue;
                e->setAttribute ("bypass", insert.bypass);
                e->addChildElement (insert.plugin.createXml().release());
                if (insert.state.getSize() > 0)
                    e->createNewChildElement (insertStateTag)->addTextElement (insert.state.toBase64Encoding());
            }
        }

        std::vector<Insert> insertsFromXml (const juce::XmlElement& parent)
        {
            std::vector<Insert> inserts;
            for (const auto* e : parent.getChildWithTagNameIterator (insertTag))
            {
                if ((int) inserts.size() >= Project::maxInserts)
                    break;
                Insert insert;
                insert.bypass = e->getBoolAttribute ("bypass");
                if (const auto* description = e->getChildByName ("PLUGIN"))
                    insert.plugin.loadFromXml (*description);
                if (const auto* state = e->getChildByName (insertStateTag))
                    insert.state.fromBase64Encoding (state->getAllSubText());
                inserts.push_back (std::move (insert));
            }
            return inserts;
        }
    }

    std::optional<float> AutomationLane::valueAt (double beat) const noexcept
    {
        if (points.empty())
            return std::nullopt;
        // The first point after the beat (the last of several at one beat wins before it).
        const auto next = std::upper_bound (points.begin(), points.end(), beat, [] (double b, const AutomationPoint& p) { return b < p.beat; });
        if (next == points.begin())
            return points.front().value;
        const auto& before = *std::prev (next);
        if (next == points.end() || isStep())
            return before.value;
        const auto span = next->beat - before.beat;
        const auto along = span > 0.0 ? (float) ((beat - before.beat) / span) : 1.0f;
        return before.value + (next->value - before.value) * curveShape (along, next->curve);
    }

    std::optional<float> AutomationLane::valueAt (double beat, bool virginTerritory) const noexcept
    {
        if (points.empty())
            return std::nullopt;
        if (! virginTerritory)
            return valueAt (beat);
        if (beat < points.front().beat)
            return std::nullopt;   // virgin territory before the first point
        // In a gap: after a terminator, before the next point.
        const auto next = std::upper_bound (points.begin(), points.end(), beat, [] (double b, const AutomationPoint& p) { return b < p.beat; });
        if (next != points.begin())
        {
            const auto& before = *std::prev (next);
            if (before.terminator && beat > before.beat)
                return std::nullopt;
            if (next == points.end() && beat > before.beat)
                return std::nullopt;   // virgin territory after the last point (the manual's)
        }
        return valueAt (beat);
    }

    float AutomationLane::trimDb (const std::vector<AutomationPoint>& trim, double beat) noexcept
    {
        if (trim.empty())
            return 0.0f;
        AutomationLane t;
        t.points = trim;
        return (*t.valueAt (beat) - 0.5f) * 2.0f * trimRange;
    }

    std::optional<float> AutomationLane::playedAt (double beat, bool virginTerritory) const noexcept
    {
        const auto value = valueAt (beat, virginTerritory);
        if (! value || trim.empty() || ! canTrim())
            return value;
        return volumeToTravel (juce::jlimit (-60.0f, 6.0f, travelToVolume (*value) + trimDb (trim, beat)));
    }

    Project::AutomationSettings::Group Project::AutomationSettings::groupOf (const AutomationLane& lane) noexcept
    {
        using T = AutomationLane::Target;
        switch (lane.target)
        {
            case T::volume: return Group::volume;
            case T::pan:    return Group::pan;
            case T::mute:   return Group::mute;
            case T::send:   return Group::sends;
            case T::instrument:
            case T::insert: return Group::plugins;
        }
        return Group::plugins;
    }

    float AutomationLane::curveShape (float along, float curve) noexcept
    {
        along = juce::jlimit (0.0f, 1.0f, along);
        if (std::abs (curve) < 1.0e-4f)
            return along;
        return std::pow (along, std::pow (4.0f, -juce::jlimit (-1.0f, 1.0f, curve)));
    }

    float volumeToTravel (float db) noexcept
    {
        return faderRange.convertTo0to1 (juce::jlimit (-60.0f, 6.0f, db));
    }

    float travelToVolume (float travel) noexcept
    {
        return faderRange.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, travel));
    }

    juce::String Instrument::getName() const
    {
        return kind == Kind::paragonSynth ? juce::String ("PR7-K") : plugin.name;   // (the studio's synth since 0.9.11)
    }

    Track* Project::findTrack (int id)
    {
        for (auto& track : tracks)
            if (track.id == id)
                return &track;
        return nullptr;
    }

    const Track* Project::findTrack (int id) const
    {
        return const_cast<Project*> (this)->findTrack (id);
    }

    const DrumMap* Project::findDrumMap (const juce::String& mapName) const
    {
        for (const auto& map : drumMaps)
            if (map.name == mapName)
                return &map;
        return nullptr;
    }

    const DrumMap* Project::drumMapOf (const Track& track) const
    {
        return track.drumMap.isNotEmpty() ? findDrumMap (track.drumMap) : nullptr;
    }

    Track* Project::armedTrack()
    {
        for (auto& track : tracks)
            if (track.armed)
                return &track;
        return nullptr;
    }

    const Track* Project::armedTrack() const
    {
        return const_cast<Project*> (this)->armedTrack();
    }

    double Project::lastBeat() const
    {
        double last = 0.0;
        for (const auto& track : tracks)
            for (const auto& part : track.parts)
                last = juce::jmax (last, part.end());
        return last;
    }

    TempoMap Project::tempoMap() const
    {
        return followsTempoTrack() ? TempoMap (tempoEvents) : TempoMap (bpm);
    }

    double Project::bpmAt (double beat) const
    {
        return followsTempoTrack() ? TempoMap (tempoEvents).bpmAt (beat) : bpm;
    }

    std::unique_ptr<juce::XmlElement> Project::toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement> (projectTag);
        xml->setAttribute ("version", fileVersion);
        xml->setAttribute ("bpm", bpm);
        xml->setAttribute ("numerator", numerator);
        xml->setAttribute ("denominator", denominator);
        xml->setAttribute ("loopOn", loopOn);
        xml->setAttribute ("loopStart", loopStart);
        xml->setAttribute ("loopEnd", loopEnd);
        xml->setAttribute ("click", click);
        xml->setAttribute ("snap", snapOn);
        xml->setAttribute ("quantize", quantizeBeats);
        xml->setAttribute ("nextTrackId", nextTrackId);
        xml->addChildElement (chordPads.toXml().release());
        if (chordTrack.shown || ! chordTrack.events.empty())
            xml->addChildElement (chordTrack.toXml().release());
        for (const auto& map : drumMaps)
            xml->addChildElement (map.toXml().release());
        for (const auto& group : linkGroups)
        {
            auto* g = xml->createNewChildElement ("LinkGroup");
            g->setAttribute ("id", group.id);
            g->setAttribute ("name", group.name);
            g->setAttribute ("volume", group.volume);
            g->setAttribute ("pan", group.pan);
            g->setAttribute ("sends", group.sends);
            g->setAttribute ("inserts", group.inserts);
            g->setAttribute ("muteSolo", group.muteSolo);
            g->setAttribute ("automation", group.automation);
            g->setAttribute ("routing", group.routing);
            g->setAttribute ("selection", group.selection);
            g->setAttribute ("recordMonitor", group.recordMonitor);
        }
        if (! tempoEvents.empty() || tempoTrackShown)
        {
            auto* tempo = xml->createNewChildElement ("Tempo");
            tempo->setAttribute ("on", tempoTrackOn);
            tempo->setAttribute ("shown", tempoTrackShown);
            tempo->setAttribute ("locked", tempoLocked);
            tempo->setAttribute ("newType", newTempoType == NewTempoType::step ? "step" : newTempoType == NewTempoType::automatic ? "automatic" : "ramp");
            tempo->setAttribute ("low", tempoViewLow);
            tempo->setAttribute ("high", tempoViewHigh);
            for (const auto& e : tempoEvents)
            {
                auto* event = tempo->createNewChildElement ("Event");
                event->setAttribute ("beat", e.beat);
                event->setAttribute ("bpm", e.bpm);
                if (e.ramp)
                    event->setAttribute ("ramp", true);
            }
        }

        for (const auto& track : tracks)
        {
            auto* t = xml->createNewChildElement (trackTag);
            t->setAttribute ("id", track.id);
            t->setAttribute ("name", track.name);
            t->setAttribute ("colour", track.colour.toString());
            t->setAttribute ("volumeDb", track.volumeDb);
            t->setAttribute ("pan", track.pan);
            t->setAttribute ("mute", track.mute);
            t->setAttribute ("solo", track.solo);
            t->setAttribute ("armed", track.armed);
            if (track.isChannel())
                t->setAttribute ("kind", track.isFx() ? "fx" : "group");
            if (track.output != 0)
                t->setAttribute ("output", track.output);
            if (track.sendsBypass)
                t->setAttribute ("sendsBypass", true);
            if (track.linkGroup != 0)
                t->setAttribute ("linkGroup", track.linkGroup);
            for (const auto& send : track.sends)
            {
                auto* e = t->createNewChildElement (sendTag);
                e->setAttribute ("target", send.target);
                e->setAttribute ("levelDb", send.levelDb);
                e->setAttribute ("on", send.on);
                e->setAttribute ("preFader", send.preFader);
            }
            if (track.isAudio())
            {
                t->setAttribute ("kind", "audio");
                t->setAttribute ("input", track.input);
                t->setAttribute ("stereoInput", track.stereoInput);
                t->setAttribute ("monitor", track.monitor);
            }
            if (track.midiInput.isNotEmpty())
                t->setAttribute ("midiInput", track.midiInput);
            if (track.midiChannel != 1)
                t->setAttribute ("midiChannel", track.midiChannel);
            if (track.disabled)
                t->setAttribute ("disabled", true);
            if (track.controllerLanes != std::vector<int> { ControllerKinds::velocity })
                t->setAttribute ("controllerLanes", ControllerKinds::lanesToText (track.controllerLanes));
            if (track.drumMap.isNotEmpty())
                t->setAttribute ("drumMap", track.drumMap);
            if (track.drumEditor)
                t->setAttribute ("drumEditor", true);
            if (track.pitchbendUp != 2 || track.pitchbendDown != 2 || track.pitchbendSnap || track.pitchbendGrid)
            {
                t->setAttribute ("pitchbendUp", track.pitchbendUp);
                t->setAttribute ("pitchbendDown", track.pitchbendDown);
                t->setAttribute ("pitchbendSnap", track.pitchbendSnap);
                t->setAttribute ("pitchbendGrid", track.pitchbendGrid);
            }
            if (track.frozen)
            {
                t->setAttribute ("frozen", true);
                t->setAttribute ("freezeFile", track.freezeFile);
            }
            for (int q = 0; q < Track::numQuickControls; ++q)
                if (const auto& qc = track.quickControls[(size_t) q])
                {
                    auto* e = t->createNewChildElement ("QuickControl");
                    e->setAttribute ("index", q);
                    e->setAttribute ("target", targetNames[(int) qc->target]);
                    e->setAttribute ("slot", qc->slot);
                    e->setAttribute ("parameter", qc->parameter);
                    e->setAttribute ("parameterName", qc->parameterName);
                }
            if (track.hidden)
                t->setAttribute ("hidden", true);
            t->setAttribute ("instrument", track.instrument.kind == Instrument::Kind::plugin ? "plugin" : "paragonSynth");
            if (track.instrument.kind == Instrument::Kind::plugin)
                t->addChildElement (track.instrument.plugin.createXml().release());
            if (track.instrument.state.getSize() > 0)
                t->createNewChildElement (stateTag)->addTextElement (track.instrument.state.toBase64Encoding());
            insertsToXml (track.inserts, *t);
            t->setAttribute ("preFaderSlots", track.preFaderSlots);
            t->setAttribute ("automationOpen", track.automationOpen);
            t->setAttribute ("automationRead", track.automationRead);
            t->setAttribute ("automationWrite", track.automationWrite);
            lanesToXml (track.automation, *t);

            for (const auto& part : track.parts)
            {
                auto* p = t->createNewChildElement (partTag);
                p->setAttribute ("start", part.start);
                p->setAttribute ("length", part.length);
                p->setAttribute ("name", part.name);
                if (part.muted)
                    p->setAttribute ("muted", true);
                if (part.isAudio())
                {
                    p->setAttribute ("audioFile", part.audioFile);
                    p->setAttribute ("audioOffset", part.audioOffset);
                    p->setAttribute ("fadeIn", part.fadeIn);
                    p->setAttribute ("fadeOut", part.fadeOut);
                }
                for (const auto& note : part.notes)
                {
                    auto* n = p->createNewChildElement (noteTag);
                    n->setAttribute ("start", note.start);
                    n->setAttribute ("length", note.length);
                    n->setAttribute ("pitch", note.pitch);
                    n->setAttribute ("velocity", note.velocity);
                }
                for (const auto& c : part.controllers)
                {
                    auto* e = p->createNewChildElement ("Controller");
                    e->setAttribute ("kind", ControllerKinds::toText (ControllerKinds::codeOf (c)));
                    e->setAttribute ("start", c.start);
                    e->setAttribute ("value", c.value);
                    if (c.ramp)
                        e->setAttribute ("ramp", true);
                }
            }
        }
        auto* master = xml->createNewChildElement (masterTag);
        master->setAttribute ("volumeDb", masterVolumeDb);
        insertsToXml (masterInserts, *master);
        master->setAttribute ("preFaderSlots", masterPreFaderSlots);
        lanesToXml (masterAutomation, *master);
        master->setAttribute ("automationOpen", masterAutomationOpen);
        master->setAttribute ("automationRead", masterRead);
        master->setAttribute ("automationWrite", masterWrite);
        {
            // (0.28.0) The Automation panel's settings.
            const auto& a = automationSettings;
            auto* e = xml->createNewChildElement ("AutomationSettings");
            e->setAttribute ("mode", (int) a.mode);
            e->setAttribute ("toPunch", (int) a.toPunch);
            e->setAttribute ("toStart", (int) a.toStart);
            e->setAttribute ("toEnd", (int) a.toEnd);
            e->setAttribute ("loop", (int) a.loop);
            e->setAttribute ("gaps", (int) a.gaps);
            juce::String read, write;
            for (int g = 0; g < AutomationSettings::numGroups; ++g)
                read << (a.suspendRead[(size_t) g] ? "1" : "0"), write << (a.suspendWrite[(size_t) g] ? "1" : "0");
            e->setAttribute ("suspendRead", read);
            e->setAttribute ("suspendWrite", write);
            e->setAttribute ("trim", a.trim);
            e->setAttribute ("freezeTrim", (int) a.freezeTrim);
            e->setAttribute ("virginTerritory", a.virginTerritory);
            e->setAttribute ("continueOnJump", a.continueOnJump);
            e->setAttribute ("revealOnWrite", a.revealOnWrite);
            e->setAttribute ("returnSeconds", a.returnSeconds);
            e->setAttribute ("reductionLevel", a.reductionLevel);
        }
        return xml;
    }

    std::optional<Project> Project::fromXml (const juce::XmlElement& xml)
    {
        if (! xml.hasTagName (projectTag) || xml.getIntAttribute ("version", 0) > fileVersion)
            return std::nullopt;

        Project project;
        project.bpm = juce::jlimit (minBpm, maxBpm, xml.getDoubleAttribute ("bpm", 120.0));
        project.numerator = juce::jlimit (1, 32, xml.getIntAttribute ("numerator", 4));
        project.denominator = juce::jlimit (1, 32, xml.getIntAttribute ("denominator", 4));
        project.loopOn = xml.getBoolAttribute ("loopOn");
        project.loopStart = juce::jmax (0.0, xml.getDoubleAttribute ("loopStart", 0.0));
        project.loopEnd = juce::jmax (project.loopStart + 0.25, xml.getDoubleAttribute ("loopEnd", 16.0));
        project.click = xml.getBoolAttribute ("click");
        project.snapOn = xml.getBoolAttribute ("snap", true);
        project.quantizeBeats = juce::jlimit (1.0 / 16.0, 4.0, xml.getDoubleAttribute ("quantize", 0.25));
        if (const auto* pads = xml.getChildByName ("ChordPads"))
            project.chordPads = ChordPads::fromXml (*pads);
        if (const auto* chords = xml.getChildByName ("ChordTrack"))
            project.chordTrack = ChordTrack::fromXml (*chords);
        if (const auto* tempo = xml.getChildByName ("Tempo"))
        {
            project.tempoTrackShown = tempo->getBoolAttribute ("shown");
            project.tempoLocked = tempo->getBoolAttribute ("locked");
            const auto type = tempo->getStringAttribute ("newType", "ramp");
            project.newTempoType = type == "step" ? NewTempoType::step : type == "automatic" ? NewTempoType::automatic : NewTempoType::ramp;
            project.tempoViewLow = juce::jlimit (minBpm, maxBpm - 10.0, tempo->getDoubleAttribute ("low", 60.0));
            project.tempoViewHigh = juce::jlimit (project.tempoViewLow + 10.0, maxBpm, tempo->getDoubleAttribute ("high", 180.0));
            for (const auto* e : tempo->getChildWithTagNameIterator ("Event"))
                project.tempoEvents.push_back ({ juce::jmax (0.0, e->getDoubleAttribute ("beat")),
                                                 juce::jlimit (minBpm, maxBpm, e->getDoubleAttribute ("bpm", project.bpm)),
                                                 e->getBoolAttribute ("ramp") });
            std::stable_sort (project.tempoEvents.begin(), project.tempoEvents.end(), [] (const TempoEvent& a, const TempoEvent& b) { return a.beat < b.beat; });
            if (! project.tempoEvents.empty())
                project.tempoEvents.front().beat = 0.0;
            project.tempoTrackOn = tempo->getBoolAttribute ("on") && ! project.tempoEvents.empty();
        }

        int highestId = 0;
        for (const auto* t : xml.getChildWithTagNameIterator (trackTag))
        {
            Track track;
            track.id = t->getIntAttribute ("id");
            track.name = t->getStringAttribute ("name", "Track");
            track.colour = juce::Colour::fromString (t->getStringAttribute ("colour", trackColour (0).toString()));
            // Songs from before 0.5.1 could have colours outside the family's
            // (blue, lime, violet, aqua, amber): each becomes its new one.
            // Others (0.5.1 to 0.9.11's shades): the nearest of the gradient's three (0.9.12).
            const juce::uint32 retired[] = { 0xff45b4ff, 0xffb8f040, 0xffb86dff, 0xff4fe3dd, 0xffffb22e };
            auto replaced = false;
            for (size_t i = 0; i < std::size (retired); ++i)
                if (track.colour.getARGB() == retired[i])
                    track.colour = trackColour (3 + (int) i), replaced = true;
            if (! replaced && ! FamilyColours::isFamily (track.colour))   // (0.17.3: the colour menu's tints kept)
                track.colour = FamilyColours::nearest (track.colour);
            track.volumeDb = (float) juce::jlimit (-60.0, 6.0, t->getDoubleAttribute ("volumeDb", 0.0));
            track.pan = (float) juce::jlimit (-1.0, 1.0, t->getDoubleAttribute ("pan", 0.0));
            track.mute = t->getBoolAttribute ("mute");
            track.solo = t->getBoolAttribute ("solo");
            track.armed = t->getBoolAttribute ("armed");
            track.midiInput = t->getStringAttribute ("midiInput");
            track.midiChannel = juce::jlimit (1, 16, t->getIntAttribute ("midiChannel", 1));
            track.disabled = t->getBoolAttribute ("disabled");
            if (t->hasAttribute ("controllerLanes"))
                track.controllerLanes = ControllerKinds::lanesFromText (t->getStringAttribute ("controllerLanes"));
            track.drumMap = t->getStringAttribute ("drumMap");
            track.drumEditor = t->getBoolAttribute ("drumEditor");
            track.pitchbendUp = juce::jlimit (1, 96, t->getIntAttribute ("pitchbendUp", 2));
            track.pitchbendDown = juce::jlimit (1, 96, t->getIntAttribute ("pitchbendDown", 2));
            track.pitchbendSnap = t->getBoolAttribute ("pitchbendSnap");
            track.pitchbendGrid = t->getBoolAttribute ("pitchbendGrid");
            track.freezeFile = t->getStringAttribute ("freezeFile");
            track.frozen = t->getBoolAttribute ("frozen") && track.freezeFile.isNotEmpty();
            for (const auto* e : t->getChildWithTagNameIterator ("QuickControl"))
            {
                const auto q = e->getIntAttribute ("index", -1);
                const auto target = e->getStringAttribute ("target");
                const auto known = std::find_if (std::begin (targetNames), std::end (targetNames), [&] (const char* n) { return target == n; });
                if (! juce::isPositiveAndBelow (q, Track::numQuickControls) || known == std::end (targetNames))
                    continue;
                AutomationLane qc;
                qc.target = (AutomationLane::Target) std::distance (std::begin (targetNames), known);
                qc.slot = juce::jlimit (0, Project::maxInserts - 1, e->getIntAttribute ("slot"));
                qc.parameter = juce::jmax (0, e->getIntAttribute ("parameter"));
                qc.parameterName = e->getStringAttribute ("parameterName");
                track.quickControls[(size_t) q] = qc;
            }
            track.hidden = t->getBoolAttribute ("hidden");
            if (t->getStringAttribute ("kind") == "audio")
            {
                track.kind = Track::Kind::audio;
                track.input = juce::jlimit (0, 63, t->getIntAttribute ("input"));
                track.stereoInput = t->getBoolAttribute ("stereoInput");
                track.monitor = t->getBoolAttribute ("monitor");
            }
            if (t->getStringAttribute ("kind") == "fx")
                track.kind = Track::Kind::fx;
            if (t->getStringAttribute ("kind") == "group")
                track.kind = Track::Kind::group;
            track.output = juce::jmax (0, t->getIntAttribute ("output"));
            track.sendsBypass = t->getBoolAttribute ("sendsBypass");
            track.linkGroup = juce::jmax (0, t->getIntAttribute ("linkGroup"));
            for (const auto* e : t->getChildWithTagNameIterator (sendTag))
            {
                if ((int) track.sends.size() >= maxSends)
                    break;
                track.sends.push_back ({ juce::jmax (0, e->getIntAttribute ("target")),
                                         (float) juce::jlimit (-60.0, 6.0, e->getDoubleAttribute ("levelDb")),
                                         e->getBoolAttribute ("on", true), e->getBoolAttribute ("preFader") });
            }
            if (t->getStringAttribute ("instrument") == "plugin")
            {
                track.instrument.kind = Instrument::Kind::plugin;
                if (const auto* description = t->getChildByName ("PLUGIN"))
                    track.instrument.plugin.loadFromXml (*description);
            }
            if (const auto* state = t->getChildByName (stateTag))
                track.instrument.state.fromBase64Encoding (state->getAllSubText());
            track.inserts = insertsFromXml (*t);
            track.preFaderSlots = juce::jlimit (0, maxInserts, t->getIntAttribute ("preFaderSlots", maxInserts));
            track.automation = lanesFromXml (*t);
            track.automationOpen = t->getBoolAttribute ("automationOpen");
            track.automationRead = t->getBoolAttribute ("automationRead");
            track.automationWrite = t->getBoolAttribute ("automationWrite");

            for (const auto* p : t->getChildWithTagNameIterator (partTag))
            {
                Part part;
                part.start = juce::jmax (0.0, p->getDoubleAttribute ("start"));
                part.length = juce::jmax (1.0e-6, p->getDoubleAttribute ("length", 4.0));
                part.name = p->getStringAttribute ("name");
                part.muted = p->getBoolAttribute ("muted");
                part.audioFile = p->getStringAttribute ("audioFile");
                part.audioOffset = juce::jmax (0.0, p->getDoubleAttribute ("audioOffset"));
                part.fadeIn = juce::jmax (0.0, p->getDoubleAttribute ("fadeIn"));
                part.fadeOut = juce::jmax (0.0, p->getDoubleAttribute ("fadeOut"));
                for (const auto* n : p->getChildWithTagNameIterator (noteTag))
                    part.notes.push_back ({ n->getDoubleAttribute ("start"), juce::jmax (0.001, n->getDoubleAttribute ("length", 0.25)),
                                            juce::jlimit (0, 127, n->getIntAttribute ("pitch", 60)),
                                            (float) juce::jlimit (0.0, 1.0, n->getDoubleAttribute ("velocity", 0.8)) });
                for (const auto* e : p->getChildWithTagNameIterator ("Controller"))
                {
                    const auto code = ControllerKinds::fromText (e->getStringAttribute ("kind"));
                    if (! ControllerKinds::isController (code))
                        continue;
                    part.controllers.push_back ({ ControllerKinds::typeOf (code), ControllerKinds::numberOf (code),
                                                  juce::jmax (0.0, e->getDoubleAttribute ("start")),
                                                  juce::jlimit (0, ControllerKinds::maxValue (code), e->getIntAttribute ("value")),
                                                  e->getBoolAttribute ("ramp") });
                }
                ControllerKinds::sort (part.controllers);
                track.parts.push_back (std::move (part));
            }
            highestId = juce::jmax (highestId, track.id);
            project.tracks.push_back (std::move (track));
        }
        project.nextTrackId = juce::jmax (highestId + 1, xml.getIntAttribute ("nextTrackId", 1));
        // Routing that leads nowhere it can go: back to the master, or no send.
        for (auto& track : project.tracks)
        {
            const auto* out = project.findTrack (track.output);
            if (track.isChannel() || out == nullptr || ! out->isGroup())
                track.output = 0;
            if (track.isFx())
                track.sends.clear();
            for (auto& send : track.sends)
                if (const auto* to = project.findTrack (send.target); to == nullptr || ! to->isFx())
                    send.target = 0;
        }
        // Link groups (0.17.0): those with two members or more; a track in one that isn't there, in none.
        for (const auto* g : xml.getChildWithTagNameIterator ("LinkGroup"))
        {
            LinkGroup group;
            group.id = g->getIntAttribute ("id");
            group.name = g->getStringAttribute ("name");
            group.volume = g->getBoolAttribute ("volume", true);
            group.pan = g->getBoolAttribute ("pan");
            group.sends = g->getBoolAttribute ("sends", true);
            group.inserts = g->getBoolAttribute ("inserts");
            group.muteSolo = g->getBoolAttribute ("muteSolo");
            group.automation = g->getBoolAttribute ("automation");
            group.routing = g->getBoolAttribute ("routing", true);
            group.selection = g->getBoolAttribute ("selection", true);
            group.recordMonitor = g->getBoolAttribute ("recordMonitor");
            if (group.id > 0 && project.findLinkGroup (group.id) == nullptr)
                project.linkGroups.push_back (group);
        }
        for (const auto* m : xml.getChildWithTagNameIterator ("DrumMap"))
            if (auto map = DrumMap::fromXml (*m); map && project.findDrumMap (map->name) == nullptr)
                project.drumMaps.push_back (std::move (*map));
        for (auto& track : project.tracks)
            if (track.drumMap.isNotEmpty() && project.findDrumMap (track.drumMap) == nullptr)
                track.drumMap = {}, track.drumEditor = false;   // (a map that isn't in the song: none)
        for (auto& track : project.tracks)
            if (project.findLinkGroup (track.linkGroup) == nullptr)
                track.linkGroup = 0;
        project.linkGroups.erase (std::remove_if (project.linkGroups.begin(), project.linkGroups.end(),
                                                  [&project] (const LinkGroup& g) { return project.linkGroupMembers (g.id).size() < 2; }),
                                  project.linkGroups.end());
        for (auto& track : project.tracks)
            if (project.findLinkGroup (track.linkGroup) == nullptr)
                track.linkGroup = 0;
        if (const auto* master = xml.getChildByName (masterTag))   // songs from before 0.3.0 have none: 0 dB, no effects
        {
            project.masterVolumeDb = (float) juce::jlimit (-60.0, 6.0, master->getDoubleAttribute ("volumeDb", 0.0));
            project.masterInserts = insertsFromXml (*master);
            project.masterPreFaderSlots = juce::jlimit (0, maxInserts, master->getIntAttribute ("preFaderSlots", maxInserts));
            project.masterAutomation = lanesFromXml (*master);
            project.masterAutomationOpen = master->getBoolAttribute ("automationOpen");
            project.masterRead = master->getBoolAttribute ("automationRead");
            project.masterWrite = master->getBoolAttribute ("automationWrite");
        }
        if (const auto* e = xml.getChildByName ("AutomationSettings"))
        {
            auto& a = project.automationSettings;
            using S = AutomationSettings;
            a.mode = (S::Mode) juce::jlimit (0, 2, e->getIntAttribute ("mode"));
            a.toPunch = (S::Fill) juce::jlimit (0, 2, e->getIntAttribute ("toPunch"));
            a.toStart = (S::Fill) juce::jlimit (0, 2, e->getIntAttribute ("toStart"));
            a.toEnd = (S::Fill) juce::jlimit (0, 2, e->getIntAttribute ("toEnd"));
            a.loop = (S::Fill) juce::jlimit (0, 2, e->getIntAttribute ("loop"));
            a.gaps = (S::Fill) juce::jlimit (0, 2, e->getIntAttribute ("gaps"));
            const auto read = e->getStringAttribute ("suspendRead"), write = e->getStringAttribute ("suspendWrite");
            for (int g = 0; g < S::numGroups; ++g)
            {
                a.suspendRead[(size_t) g] = read[g] == '1';
                a.suspendWrite[(size_t) g] = write[g] == '1';
            }
            a.trim = e->getBoolAttribute ("trim");
            a.freezeTrim = (S::FreezeTrim) juce::jlimit (0, 2, e->getIntAttribute ("freezeTrim"));
            a.virginTerritory = e->getBoolAttribute ("virginTerritory");
            a.continueOnJump = e->getBoolAttribute ("continueOnJump");
            a.revealOnWrite = e->getBoolAttribute ("revealOnWrite", true);
            a.returnSeconds = juce::jlimit (0.0, 5.0, e->getDoubleAttribute ("returnSeconds", 0.1));
            a.reductionLevel = juce::jlimit (0, 100, e->getIntAttribute ("reductionLevel", 50));
        }
        return project;
    }

    bool LinkGroup::links (What what) const noexcept
    {
        switch (what)
        {
            case What::volume:        return volume;
            case What::pan:           return pan;
            case What::sends:         return sends;
            case What::inserts:       return inserts;
            case What::muteSolo:      return muteSolo;
            case What::automation:    return automation;
            case What::routing:       return routing;
            case What::selection:     return selection;
            case What::recordMonitor: return recordMonitor;
        }
        return false;
    }

    void LinkGroup::setLinks (What what, bool on) noexcept
    {
        switch (what)
        {
            case What::volume:        volume = on; break;
            case What::pan:           pan = on; break;
            case What::sends:         sends = on; break;
            case What::inserts:       inserts = on; break;
            case What::muteSolo:      muteSolo = on; break;
            case What::automation:    automation = on; break;
            case What::routing:       routing = on; break;
            case What::selection:     selection = on; break;
            case What::recordMonitor: recordMonitor = on; break;
        }
    }

    LinkGroup* Project::findLinkGroup (int id)
    {
        for (auto& g : linkGroups)
            if (id != 0 && g.id == id)
                return &g;
        return nullptr;
    }

    const LinkGroup* Project::findLinkGroup (int id) const
    {
        return const_cast<Project*> (this)->findLinkGroup (id);
    }

    std::vector<int> Project::linkGroupMembers (int groupId) const
    {
        std::vector<int> ids;
        for (const auto& t : tracks)
            if (groupId != 0 && t.linkGroup == groupId)
                ids.push_back (t.id);
        return ids;
    }

    juce::String formatBarsBeats (double beats, const Project& project)
    {
        beats = juce::jmax (0.0, beats);
        // Whole ticks first, so a position never shows "480" ticks.
        constexpr int ticksPerBeat = 480, ticksPerSixteenth = 120;
        const auto totalTicks = (juce::int64) std::floor (beats * ticksPerBeat + 1.0e-6);
        const auto ticksPerBar = (juce::int64) std::llround (project.beatsPerBar() * ticksPerBeat);
        const auto ticksPerUnit = (juce::int64) std::llround (4.0 / project.denominator * ticksPerBeat);   // the signature's beat (a quarter in 4/4, an eighth in 6/8)
        const auto bar = totalTicks / ticksPerBar + 1;
        auto rest = totalTicks % ticksPerBar;
        const auto beat = rest / ticksPerUnit + 1;
        rest %= ticksPerUnit;
        const auto sixteenth = rest / ticksPerSixteenth + 1;
        const auto tick = rest % ticksPerSixteenth;
        return juce::String (bar) + ". " + juce::String (beat) + ". " + juce::String (sixteenth) + ". " + juce::String (tick).paddedLeft (' ', 3);
    }

    juce::String formatTime (double seconds)
    {
        seconds = juce::jmax (0.0, seconds);
        const auto millis = (juce::int64) std::floor (seconds * 1000.0 + 1.0e-6);
        return juce::String (millis / 60000) + ":" + juce::String ((millis / 1000) % 60).paddedLeft ('0', 2) + "."
             + juce::String (millis % 1000).paddedLeft ('0', 3);
    }

    juce::Colour trackColour (int index)
    {
        // Only the gradient's three colours (0.9.12, the owner's rule), in turn.
        static const juce::Colour colours[] = { FamilyColours::red, FamilyColours::pink, FamilyColours::orange };
        return colours[(size_t) (index % 3 + 3) % 3];
    }

    juce::Colour FamilyColours::nearest (juce::Colour colour)
    {
        // By hue mostly (a light pink is still pink), then by lightness.
        const auto distance = [colour] (juce::Colour other)
        {
            auto hue = std::abs (colour.getHue() - other.getHue());
            hue = juce::jmin (hue, 1.0f - hue);
            return hue * 4.0f + std::abs (colour.getBrightness() - other.getBrightness()) * 0.5f;
        };
        auto best = pink;
        for (const auto candidate : { red, orange })
            if (distance (candidate) < distance (best))
                best = candidate;
        return best;
    }

    juce::Colour FamilyColours::paletteColour (int hue, int shade)
    {
        // Hues 5 degrees apart: 346 (the pink) to 1 (the red) to 26 (the orange).
        const auto degrees = std::fmod (346.0f + 5.0f * (float) juce::jlimit (0, paletteHues - 1, hue), 360.0f);
        // Deep to pale: brightness up, saturation down.
        static const float saturation[] = { 1.0f, 0.96f, 0.92f, 0.88f, 0.76f, 0.58f, 0.42f, 0.26f };
        static const float brightness[] = { 0.42f, 0.56f, 0.70f, 0.84f, 0.95f, 1.0f, 1.0f, 1.0f };
        const auto s = (size_t) juce::jlimit (0, paletteShades - 1, shade);
        return juce::Colour::fromHSV (degrees / 360.0f, saturation[s], brightness[s], 1.0f);
    }

    bool FamilyColours::isFamily (juce::Colour colour)
    {
        if (colour == red || colour == pink || colour == orange)
            return true;
        for (int h = 0; h < paletteHues; ++h)
            for (int s = 0; s < paletteShades; ++s)
                if (paletteColour (h, s) == colour)
                    return true;
        return false;
    }

    double snap (double beats, double grid) noexcept
    {
        return grid > 0.0 ? std::round (beats / grid) * grid : beats;
    }

    void quantize (Part& part, double grid, const std::vector<int>& which)
    {
        if (grid <= 0.0)
            return;
        std::vector<bool> chosen (part.notes.size(), which.empty());
        for (const auto index : which)
            if (juce::isPositiveAndBelow (index, (int) part.notes.size()))
                chosen[(size_t) index] = true;

        for (size_t i = 0; i < part.notes.size(); ++i)
            if (chosen[i])
            {
                auto& note = part.notes[i];
                note.start = juce::jmax (0.0, snap (part.start + note.start, grid) - part.start);
            }
    }

    juce::String noteName (int pitch)
    {
        return juce::MidiMessage::getMidiNoteName (juce::jlimit (0, 127, pitch), true, true, 3);
    }
}
