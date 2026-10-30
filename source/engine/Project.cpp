#include "Project.h"

namespace pr7s
{
    namespace
    {
        // Bump when the file layout changes in a way older versions can't read.
        constexpr int fileVersion = 1;
        const juce::Identifier projectTag { "PR7SProject" }, trackTag { "Track" }, partTag { "Part" }, noteTag { "Note" },
                               stateTag { "InstrumentState" }, insertTag { "Insert" }, insertStateTag { "InsertState" },
                               masterTag { "Master" };

        void insertsToXml (const std::vector<Insert>& inserts, juce::XmlElement& parent)
        {
            for (const auto& insert : inserts)
            {
                auto* e = parent.createNewChildElement (insertTag);
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
            t->setAttribute ("instrument", track.instrument.kind == Instrument::Kind::plugin ? "plugin" : "paragonSynth");
            if (track.instrument.kind == Instrument::Kind::plugin)
                t->addChildElement (track.instrument.plugin.createXml().release());
            if (track.instrument.state.getSize() > 0)
                t->createNewChildElement (stateTag)->addTextElement (track.instrument.state.toBase64Encoding());
            insertsToXml (track.inserts, *t);

            for (const auto& part : track.parts)
            {
                auto* p = t->createNewChildElement (partTag);
                p->setAttribute ("start", part.start);
                p->setAttribute ("length", part.length);
                p->setAttribute ("name", part.name);
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
            track.volumeDb = (float) juce::jlimit (-60.0, 6.0, t->getDoubleAttribute ("volumeDb", 0.0));
            track.pan = (float) juce::jlimit (-1.0, 1.0, t->getDoubleAttribute ("pan", 0.0));
            track.mute = t->getBoolAttribute ("mute");
            track.solo = t->getBoolAttribute ("solo");
            track.armed = t->getBoolAttribute ("armed");
            if (t->getStringAttribute ("instrument") == "plugin")
            {
                track.instrument.kind = Instrument::Kind::plugin;
                if (const auto* description = t->getChildByName ("PLUGIN"))
                    track.instrument.plugin.loadFromXml (*description);
            }
            if (const auto* state = t->getChildByName (stateTag))
                track.instrument.state.fromBase64Encoding (state->getAllSubText());
            track.inserts = insertsFromXml (*t);

            for (const auto* p : t->getChildWithTagNameIterator (partTag))
            {
                Part part;
                part.start = juce::jmax (0.0, p->getDoubleAttribute ("start"));
                part.length = juce::jmax (0.0625, p->getDoubleAttribute ("length", 4.0));
                part.name = p->getStringAttribute ("name");
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
        if (const auto* master = xml.getChildByName (masterTag))   // songs from before 0.3.0 have none: 0 dB, no effects
        {
            project.masterVolumeDb = (float) juce::jlimit (-60.0, 6.0, master->getDoubleAttribute ("volumeDb", 0.0));
            project.masterInserts = insertsFromXml (*master);
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
        static const juce::Colour colours[] = {
            juce::Colour (0xffe0103c),   // the chain's red
            juce::Colour (0xffff2f7a),   // hot pink
            juce::Colour (0xffff7a1a),   // orange
            juce::Colour (0xff45b4ff),   // blue
            juce::Colour (0xffb8f040),   // lime
            juce::Colour (0xffb86dff),   // violet
            juce::Colour (0xff4fe3dd),   // aqua
            juce::Colour (0xffffb22e),   // amber
        };
        return colours[(size_t) (index % (int) std::size (colours) + (int) std::size (colours)) % std::size (colours)];
    }

    double snap (double beats, double grid) noexcept
    {
        return grid > 0.0 ? std::round (beats / grid) * grid : beats;
    }

    bool quantize (Part& part, double grid, const std::vector<int>& which)
    {
        if (grid <= 0.0)
            return true;
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

        // Same pitch, same start: keep the longer one.
        std::vector<Note> kept;
        for (const auto& note : part.notes)
        {
            const auto twin = std::find_if (kept.begin(), kept.end(), [&note] (const Note& other)
            {
                return other.pitch == note.pitch && std::abs (other.start - note.start) < 1.0e-9;
            });
            if (twin == kept.end())
                kept.push_back (note);
            else if (note.length > twin->length)
                *twin = note;
        }
        const auto merged = kept.size() != part.notes.size();
        part.notes = std::move (kept);
        return ! merged;
    }

    juce::String noteName (int pitch)
    {
        return juce::MidiMessage::getMidiNoteName (juce::jlimit (0, 127, pitch), true, true, 3);
    }
}
