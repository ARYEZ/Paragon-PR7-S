#include "engine/SongSections.h"

#include "engine/Markers.h"
#include "engine/SectionTypes.h"

namespace pr7s
{
    int SongSections::nextId() const noexcept
    {
        auto most = 0;
        for (const auto& b : blocks)
            most = juce::jmax (most, b.id);
        return most + 1;
    }

    int SongSections::totalBars() const noexcept
    {
        auto bars = 0;
        for (const auto& b : blocks)
            bars += b.bars;
        return bars;
    }

    int SongSections::indexOf (int id) const noexcept
    {
        for (int i = 0; i < (int) blocks.size(); ++i)
            if (blocks[(size_t) i].id == id)
                return i;
        return -1;
    }

    int SongSections::nextLink() const noexcept
    {
        auto most = 0;
        for (const auto& b : blocks)
            most = juce::jmax (most, b.link);
        return most + 1;
    }

    std::vector<int> SongSections::linkedWith (int index) const
    {
        std::vector<int> out;
        if (! juce::isPositiveAndBelow (index, (int) blocks.size()))
            return out;
        const auto link = blocks[(size_t) index].link;
        for (int i = 0; i < (int) blocks.size(); ++i)
            if (i == index || (link > 0 && blocks[(size_t) i].link == link))
                out.push_back (i);
        return out;
    }

    std::unique_ptr<juce::XmlElement> SongSections::toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement> ("SongSections");
        xml->setAttribute ("shown", shown);
        for (const auto& b : blocks)
        {
            auto* e = xml->createNewChildElement ("Section");
            e->setAttribute ("id", b.id);
            e->setAttribute ("name", b.name);
            e->setAttribute ("bars", b.bars);
            if (b.link > 0)
                e->setAttribute ("link", b.link);
            if (b.type.isNotEmpty())
                e->setAttribute ("type", b.type);
            if (b.transition.isNotEmpty())
                e->setAttribute ("transition", b.transition);
            if (b.recipe.isNotEmpty())
            {
                e->setAttribute ("recipe", b.recipe);
                for (const auto& l : b.lanes)
                {
                    auto* lane = e->createNewChildElement ("Lane");
                    lane->setAttribute ("layer", l.layer);
                    lane->setAttribute ("track", l.track);
                    lane->setAttribute ("mark", juce::String (l.mark));
                }
            }
        }
        return xml;
    }

    SongSections SongSections::fromXml (const juce::XmlElement& xml)
    {
        SongSections s;
        s.shown = xml.getBoolAttribute ("shown", true);
        for (auto* e : xml.getChildWithTagNameIterator ("Section"))
        {
            SongSection b;
            b.id = e->getIntAttribute ("id", 0);
            const auto name = sectionBaseOf (e->getStringAttribute ("name"));
            b.name = name.isNotEmpty() ? name : juce::String ("Verse");
            b.bars = juce::jlimit (leastSectionBars, mostSectionBars, e->getIntAttribute ("bars", 8));
            b.link = juce::jmax (0, e->getIntAttribute ("link", 0));
            b.recipe = e->getStringAttribute ("recipe");
            b.type = e->getStringAttribute ("type");
            b.transition = e->getStringAttribute ("transition");
            for (auto* lane : e->getChildWithTagNameIterator ("Lane"))
                b.lanes.push_back ({ lane->getIntAttribute ("layer"), lane->getIntAttribute ("track"),
                                     lane->getStringAttribute ("mark").getLargeIntValue() });
            s.blocks.push_back (b);
        }
        // Every id unique and from 1.
        std::vector<int> seen;
        for (auto& b : s.blocks)
            if (b.id <= 0 || std::find (seen.begin(), seen.end(), b.id) != seen.end())
                b.id = 0;
            else
                seen.push_back (b.id);
        for (auto& b : s.blocks)
            if (b.id == 0)
                seen.push_back (b.id = s.nextId());
        // A link with one block left is no link.
        for (auto& b : s.blocks)
            if (b.link > 0 && std::count_if (s.blocks.begin(), s.blocks.end(), [&b] (const SongSection& o) { return o.link == b.link; }) < 2)
                b.link = 0;
        return s;
    }

    std::vector<SectionPlace> sectionPlaces (const SongSections& sections, const SignatureMap& map)
    {
        std::vector<SectionPlace> out;
        auto bar = 0;
        for (const auto& b : sections.blocks)
        {
            SectionPlace p;
            p.firstBar = bar;
            p.bars = b.bars;
            p.start = map.beatOfBar (bar);
            p.end = map.beatOfBar (bar + b.bars);
            out.push_back (p);
            bar += b.bars;
        }
        return out;
    }

    int sectionAt (const std::vector<SectionPlace>& places, double beat) noexcept
    {
        for (int i = 0; i < (int) places.size(); ++i)
            if (places[(size_t) i].contains (beat))
                return i;
        return -1;
    }

    juce::String sectionLabel (const std::vector<SongSection>& blocks, int index)
    {
        if (! juce::isPositiveAndBelow (index, (int) blocks.size()))
            return {};
        const auto& name = blocks[(size_t) index].name;
        auto count = 0, place = 0;
        for (int i = 0; i < (int) blocks.size(); ++i)
            if (blocks[(size_t) i].name == name)
            {
                ++count;
                if (i <= index)
                    ++place;
            }
        return count > 1 ? name + " " + juce::String (place) : name;
    }

    juce::String sectionLabelWithType (const std::vector<SongSection>& blocks, int index)
    {
        const auto label = sectionLabel (blocks, index);
        if (label.isEmpty() || blocks[(size_t) index].type.isEmpty())
            return label;
        return label + " - " + blocks[(size_t) index].type;
    }

    int composerSectionFor (const juce::String& name)
    {
        // drumSections(): intro 0, verse 1, chorus 2, bridge 3, breakdown 4, ending 5, solo 6.
        const auto base = sectionBaseOf (name);
        if (base == "Intro") return 0;
        if (base == "Verse" || base == "Pre-Chorus" || base == "Riff") return 1;
        if (base == "Chorus" || base == "Post-Chorus" || base == "Hook" || base == "Refrain" || base == "Drop") return 2;
        if (base == "Bridge" || base == "Middle 8" || base == "Interlude" || base == "Instrumental" || base == "Break") return 3;
        if (base == "Breakdown" || base == "Build-Up") return 4;
        if (base == "Outro" || base == "Ending" || base == "Tag" || base == "Coda") return 5;
        if (base == "Solo") return 6;
        return 1;
    }

    int SongSection::shortBeats() const
    {
        const auto t = sectionTransitionNamed (transition);
        return t.has_value() ? t->shortBeats : 0;
    }
}
