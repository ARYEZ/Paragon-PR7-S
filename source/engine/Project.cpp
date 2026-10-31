#include "Project.h"

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
                                             (float) juce::jlimit (0.0, 1.0, p->getDoubleAttribute ("value")) });
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
        return before.value + (next->value - before.value) * along;
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
        return kind == Kind::paragonSynth ? juce::String ("Paragon Synth") : plugin.name;
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
        xml->setAttribute ("nextTrackId", nextTrackId);

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

        int highestId = 0;
        for (const auto* t : xml.getChildWithTagNameIterator (trackTag))
        {
            Track track;
            track.id = t->getIntAttribute ("id");
            track.name = t->getStringAttribute ("name", "Track");
            track.colour = juce::Colour::fromString (t->getStringAttribute ("colour", trackColour (0).toString()));
            // Songs from before 0.5.1 could have colours outside the family's
            // (blue, lime, violet, aqua, amber): each becomes its new one.
            const juce::uint32 retired[] = { 0xff45b4ff, 0xffb8f040, 0xffb86dff, 0xff4fe3dd, 0xffffb22e };
            for (size_t i = 0; i < std::size (retired); ++i)
                if (track.colour.getARGB() == retired[i])
                    track.colour = trackColour (3 + (int) i);
            track.volumeDb = (float) juce::jlimit (-60.0, 6.0, t->getDoubleAttribute ("volumeDb", 0.0));
            track.pan = (float) juce::jlimit (-1.0, 1.0, t->getDoubleAttribute ("pan", 0.0));
            track.mute = t->getBoolAttribute ("mute");
            track.solo = t->getBoolAttribute ("solo");
            track.armed = t->getBoolAttribute ("armed");
            track.midiInput = t->getStringAttribute ("midiInput");
            track.midiChannel = juce::jlimit (1, 16, t->getIntAttribute ("midiChannel", 1));
            track.disabled = t->getBoolAttribute ("disabled");
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
                part.length = juce::jmax (0.0625, p->getDoubleAttribute ("length", 4.0));
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
        return project;
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
        // Only the family's colours: shades along its red, pink and orange
        // (the owner's rule), each unlike the one before it.
        static const juce::Colour colours[] = {
            juce::Colour (0xffe0103c),   // the chain's red
            juce::Colour (0xffff2f7a),   // hot pink
            juce::Colour (0xffff7a1a),   // orange
            juce::Colour (0xffff7aa8),   // rose (a light pink)
            juce::Colour (0xffff4a2a),   // red-orange
            juce::Colour (0xffc2185b),   // deep pink
            juce::Colour (0xffffa04a),   // peach (a light orange)
            juce::Colour (0xffa3122f),   // crimson
        };
        return colours[(size_t) (index % (int) std::size (colours) + (int) std::size (colours)) % std::size (colours)];
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
