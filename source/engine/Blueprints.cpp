#include "engine/Blueprints.h"

#include "engine/Progressions.h"

namespace pr7s
{
    namespace
    {
        int wrapPc (int n) { return ((n % 12) + 12) % 12; }
    }

    int Blueprint::totalBars() const noexcept
    {
        auto total = 0;
        for (const auto& b : blocks)
            total += b.bars;
        return total;
    }

    bool Blueprint::hasChords() const noexcept
    {
        for (const auto& b : blocks)
            if (! b.chords.empty())
                return true;
        return false;
    }

    int Blueprint::tonicIn (const Key& key) const noexcept
    {
        const auto tonic = key.pitchClass();
        if (minor == key.minor)
            return tonic;
        return wrapPc (key.minor ? tonic + 3 : tonic + 9);
    }

    std::vector<std::pair<Chord, double>> Blueprint::chordsIn (const Key& key, int block) const
    {
        std::vector<std::pair<Chord, double>> out;
        if (block < 0 || block >= (int) blocks.size())
            return out;
        const auto tonic = tonicIn (key);
        for (const auto& c : blocks[(size_t) block].chords)
            out.push_back ({ c.chord.transposed (tonic), c.bars });
        return out;
    }

    std::unique_ptr<juce::XmlElement> Blueprint::toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement> ("Blueprint");
        xml->setAttribute ("name", name);
        if (description.isNotEmpty())
            xml->setAttribute ("description", description);
        if (bpm > 0.0)
            xml->setAttribute ("bpm", bpm);
        xml->setAttribute ("minor", minor ? 1 : 0);
        if (keyIndex >= 0)
            xml->setAttribute ("key", keyIndex);
        for (const auto& b : blocks)
        {
            auto* e = xml->createNewChildElement ("Block");
            e->setAttribute ("name", b.name);
            e->setAttribute ("bars", b.bars);
            if (b.link > 0)
                e->setAttribute ("link", b.link);
            if (b.type.isNotEmpty())
                e->setAttribute ("type", b.type);
            if (b.transition.isNotEmpty())
                e->setAttribute ("transition", b.transition);
            for (const auto& c : b.chords)
            {
                auto* ce = e->createNewChildElement ("Chord");
                ce->setAttribute ("chord", c.chord.toText());
                ce->setAttribute ("bars", c.bars);
            }
        }
        return xml;
    }

    std::optional<Blueprint> Blueprint::fromXml (const juce::XmlElement& xml)
    {
        if (! xml.hasTagName ("Blueprint"))
            return std::nullopt;
        Blueprint bp;
        bp.name = xml.getStringAttribute ("name").trim();
        bp.description = xml.getStringAttribute ("description");
        bp.bpm = juce::jlimit (0.0, 400.0, xml.getDoubleAttribute ("bpm", 0.0));
        bp.minor = xml.getIntAttribute ("minor", 1) != 0;
        bp.keyIndex = xml.getIntAttribute ("key", -1);
        if (bp.keyIndex >= (int) Key::all().size())
            bp.keyIndex = -1;
        for (auto* e : xml.getChildWithTagNameIterator ("Block"))
        {
            BlueprintBlock b;
            b.name = e->getStringAttribute ("name", "Verse");
            b.bars = juce::jlimit (1, 64, e->getIntAttribute ("bars", 8));
            b.link = juce::jmax (0, e->getIntAttribute ("link", 0));
            b.type = e->getStringAttribute ("type");
            b.transition = e->getStringAttribute ("transition");
            for (auto* ce : e->getChildWithTagNameIterator ("Chord"))
                if (const auto chord = Chord::fromText (ce->getStringAttribute ("chord")))
                {
                    const auto bars = ce->getDoubleAttribute ("bars", 1.0);
                    if (bars > 0.0)
                        b.chords.push_back ({ *chord, juce::jmin (64.0, bars) });
                }
            bp.blocks.push_back (std::move (b));
            if (bp.blocks.size() >= 256)
                break;
        }
        if (bp.name.isEmpty() || bp.blocks.empty())
            return std::nullopt;
        return bp;
    }

    bool Blueprint::operator== (const Blueprint& o) const
    {
        return name == o.name && description == o.description && bpm == o.bpm && minor == o.minor && keyIndex == o.keyIndex
            && blocks == o.blocks;
    }

    BlueprintBlock blueprintBlock (const juce::String& name, int bars, int link, const juce::String& type,
                                   const juce::String& transition, const juce::String& numerals, bool minor)
    {
        BlueprintBlock b;
        b.name = name;
        b.bars = bars;
        b.link = link;
        b.type = type;
        b.transition = transition;
        for (const auto& word : juce::StringArray::fromTokens (numerals, " ", ""))
        {
            if (word.trim().isEmpty())
                continue;
            LibraryProgression::Step step;
            if (! parseNumeral (word, minor, step))
            {
                jassertfalse;   // (a typo in a table)
                continue;
            }
            b.chords.push_back ({ Chord { step.offset, step.type, step.tension, step.bassOffset }, step.bars });
        }
        return b;
    }

    juce::File defaultBlueprintFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Paragon PR7-S").getChildFile ("Blueprints");
    }

    std::optional<Blueprint> loadBlueprint (const juce::File& file)
    {
        if (! file.existsAsFile() || file.getSize() > 4 * 1024 * 1024)
            return std::nullopt;
        const auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr)
            return std::nullopt;
        auto bp = Blueprint::fromXml (*xml);
        if (bp.has_value())
        {
            bp->file = file;
            bp->factory = false;
        }
        return bp;
    }

    bool saveBlueprint (const Blueprint& bp, const juce::File& file)
    {
        if (! file.getParentDirectory().createDirectory())
            return false;
        return bp.toXml()->writeTo (file);
    }

    std::vector<Blueprint> userBlueprints (const juce::File& folder)
    {
        std::vector<Blueprint> out;
        if (! folder.isDirectory())
            return out;
        for (const auto& entry : juce::RangedDirectoryIterator (folder, false, juce::String ("*") + blueprintExtension))
            if (auto bp = loadBlueprint (entry.getFile()))
                out.push_back (std::move (*bp));
        std::sort (out.begin(), out.end(), [] (const Blueprint& a, const Blueprint& b) { return a.name.compareNatural (b.name) < 0; });
        return out;
    }
}
