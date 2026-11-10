#include "Markers.h"

#include <algorithm>
#include <set>

namespace pr7s
{
    void MarkerTrack::sort()
    {
        for (auto& m : markers)
        {
            m.start = juce::jmax (0.0, m.start);
            m.end = m.cycle ? juce::jmax (m.start + 1.0e-6, m.end) : m.start;
        }
        std::stable_sort (markers.begin(), markers.end(), [] (const Marker& a, const Marker& b)
        {
            if (a.start != b.start)
                return a.start < b.start;
            return a.cycle && ! b.cycle;
        });
    }

    int MarkerTrack::nextId (bool cycle) const
    {
        std::set<int> used;
        for (const auto& m : markers)
            if (m.cycle == cycle)
                used.insert (m.id);
        int id = 1;
        while (used.count (id) > 0)
            ++id;
        return id;
    }

    const Marker* MarkerTrack::find (int id, bool cycle) const
    {
        const auto i = indexOf (id, cycle);
        return i >= 0 ? &markers[(size_t) i] : nullptr;
    }

    int MarkerTrack::indexOf (int id, bool cycle) const
    {
        for (size_t i = 0; i < markers.size(); ++i)
            if (markers[i].id == id && markers[i].cycle == cycle)
                return (int) i;
        return -1;
    }

    std::optional<double> MarkerTrack::nextStart (double beat, bool forward) const
    {
        constexpr auto hair = 1.0e-6;
        std::optional<double> best;
        for (const auto& m : markers)
        {
            if (forward ? m.start > beat + hair : m.start < beat - hair)
                if (! best || (forward ? m.start < *best : m.start > *best))
                    best = m.start;
        }
        return best;
    }

    int MarkerTrack::nearestBefore (double beat) const
    {
        int found = -1;
        for (size_t i = 0; i < markers.size(); ++i)
            if (markers[i].start <= beat + 1.0e-6)
                found = (int) i;   // (sorted: the last one at or before)
        return found;
    }

    void MarkerTrack::reassignIds (bool cycle)
    {
        sort();
        int id = 1;
        for (auto& m : markers)
            if (m.cycle == cycle)
                m.id = id++;
    }

    std::unique_ptr<juce::XmlElement> MarkerTrack::toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement> ("MarkerTrack");
        xml->setAttribute ("shown", shown);
        xml->setAttribute ("locked", locked);
        xml->setAttribute ("name", name);
        xml->setAttribute ("lines", showLines);
        xml->setAttribute ("ids", showIds);
        xml->setAttribute ("cycleFollows", cycleFollows);
        for (const auto& m : markers)
        {
            auto* e = xml->createNewChildElement ("Marker");
            e->setAttribute ("id", m.id);
            if (m.cycle)
                e->setAttribute ("cycle", true);
            e->setAttribute ("start", juce::String (m.start, 9));
            if (m.cycle)
                e->setAttribute ("end", juce::String (m.end, 9));
            if (m.name.isNotEmpty())
                e->setAttribute ("name", m.name);
        }
        return xml;
    }

    MarkerTrack MarkerTrack::fromXml (const juce::XmlElement& xml)
    {
        MarkerTrack track;
        track.shown = xml.getBoolAttribute ("shown");
        track.locked = xml.getBoolAttribute ("locked");
        track.name = xml.getStringAttribute ("name", "Markers");
        track.showLines = xml.getBoolAttribute ("lines", true);
        track.showIds = xml.getBoolAttribute ("ids", true);
        track.cycleFollows = xml.getBoolAttribute ("cycleFollows");
        for (const auto* e : xml.getChildWithTagNameIterator ("Marker"))
        {
            Marker m;
            m.cycle = e->getBoolAttribute ("cycle");
            m.id = juce::jmax (1, e->getIntAttribute ("id", 1));
            m.start = juce::jmax (0.0, e->getDoubleAttribute ("start"));
            m.end = m.cycle ? e->getDoubleAttribute ("end", m.start + 4.0) : m.start;
            if (m.cycle && m.end <= m.start)
                continue;   // (an empty range: nothing to recall)
            m.name = e->getStringAttribute ("name");
            // (Two of one kind with the same ID: the later one takes the next free ID.)
            if (track.indexOf (m.id, m.cycle) >= 0)
                m.id = track.nextId (m.cycle);
            track.markers.push_back (m);
        }
        track.sort();
        return track;
    }

    juce::String markerIdText (const Marker& m)
    {
        return m.cycle ? "(" + juce::String (m.id) + ")" : juce::String (m.id);
    }

    const juce::StringArray& songSectionNames()
    {
        static const juce::StringArray names { "Intro", "Verse", "Pre-Chorus", "Chorus", "Post-Chorus", "Hook", "Refrain",
                                               "Bridge", "Middle 8", "Breakdown", "Build-Up", "Drop", "Solo", "Instrumental",
                                               "Interlude", "Riff", "Break", "Tag", "Outro", "Ending", "Coda" };
        return names;
    }

    juce::String sectionBaseOf (const juce::String& name)
    {
        auto text = name.trim();
        const auto space = text.lastIndexOfChar (' ');
        if (space > 0 && text.substring (space + 1).containsOnly ("0123456789"))
            text = text.substring (0, space).trim();
        for (const auto& n : songSectionNames())
            if (n.equalsIgnoreCase (text))
                return n;
        return {};
    }

    void numberSections (std::vector<Marker>& markers)
    {
        std::vector<size_t> order (markers.size());
        for (size_t i = 0; i < order.size(); ++i)
            order[i] = i;
        std::stable_sort (order.begin(), order.end(), [&] (size_t a, size_t b) { return markers[a].start < markers[b].start; });
        for (const auto& base : songSectionNames())
        {
            std::vector<size_t> these;
            for (const auto i : order)
                if (sectionBaseOf (markers[i].name) == base)
                    these.push_back (i);
            for (size_t k = 0; k < these.size(); ++k)
                markers[these[k]].name = these.size() > 1 ? base + " " + juce::String ((int) k + 1) : base;
        }
    }
}
