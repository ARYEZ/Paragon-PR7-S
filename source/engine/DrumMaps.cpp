#include "DrumMaps.h"

namespace pr7s
{
    juce::String DrumMap::gmName (int pitch)
    {
        // General MIDI Level 1's percussion key map (35-81) and Level 2's additions.
        static const char* const names[] = {
            "High Q", "Slap", "Scratch Push", "Scratch Pull", "Sticks", "Square Click", "Metronome Click", "Metronome Bell",   // 27-34
            "Acoustic Bass Drum", "Bass Drum 1", "Side Stick", "Acoustic Snare", "Hand Clap", "Electric Snare",               // 35-40
            "Low Floor Tom", "Closed Hi-Hat", "High Floor Tom", "Pedal Hi-Hat", "Low Tom", "Open Hi-Hat", "Low-Mid Tom",       // 41-47
            "Hi-Mid Tom", "Crash Cymbal 1", "High Tom", "Ride Cymbal 1", "Chinese Cymbal", "Ride Bell", "Tambourine",          // 48-54
            "Splash Cymbal", "Cowbell", "Crash Cymbal 2", "Vibraslap", "Ride Cymbal 2", "Hi Bongo", "Low Bongo",               // 55-61
            "Mute Hi Conga", "Open Hi Conga", "Low Conga", "High Timbale", "Low Timbale", "High Agogo", "Low Agogo",           // 62-68
            "Cabasa", "Maracas", "Short Whistle", "Long Whistle", "Short Guiro", "Long Guiro", "Claves", "Hi Wood Block",      // 69-76
            "Low Wood Block", "Mute Cuica", "Open Cuica", "Mute Triangle", "Open Triangle",                                    // 77-81
            "Shaker", "Jingle Bell", "Belltree", "Castanets", "Mute Surdo", "Open Surdo" };                                   // 82-87
        return pitch >= 27 && pitch <= 87 ? juce::String (names[pitch - 27]) : juce::String();
    }

    DrumMap DrumMap::gm()
    {
        DrumMap map;
        map.name = "GM Map";
        for (int pitch = 0; pitch < 128; ++pitch)
        {
            map.sounds[(size_t) pitch] = { gmName (pitch), pitch, pitch, -1, false };
            map.order.push_back (pitch);
        }
        return map;
    }

    DrumMap DrumMap::empty (const juce::String& name)
    {
        DrumMap map;
        map.name = name;
        for (int pitch = 0; pitch < 128; ++pitch)
        {
            map.sounds[(size_t) pitch] = { "Sound " + juce::String (pitch + 1), pitch, pitch, -1, false };
            map.order.push_back (pitch);
        }
        return map;
    }

    bool DrumMap::isIdentity() const noexcept
    {
        for (int pitch = 0; pitch < 128; ++pitch)
        {
            const auto& s = sounds[(size_t) pitch];
            if (s.outNote != pitch || s.channel >= 0 || s.mute)
                return false;
        }
        return true;
    }

    int DrumMap::pitchForInput (int inNote) const noexcept
    {
        // The sound played from that key: one moved onto it (its I-note set to it) first, as that's
        // what setting an I-note means; else the sound at that pitch, if its I-note is still its own.
        for (int pitch = 0; pitch < 128; ++pitch)
            if (pitch != inNote && sounds[(size_t) pitch].inNote == inNote)
                return pitch;
        if (juce::isPositiveAndBelow (inNote, 128) && sounds[(size_t) inNote].inNote == inNote)
            return inNote;
        return juce::jlimit (0, 127, inNote);
    }

    std::unique_ptr<juce::XmlElement> DrumMap::toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement> ("DrumMap");
        xml->setAttribute ("name", name);
        for (int pitch = 0; pitch < 128; ++pitch)
        {
            const auto& s = sounds[(size_t) pitch];
            auto* e = xml->createNewChildElement ("Sound");
            e->setAttribute ("pitch", pitch);
            e->setAttribute ("name", s.name);
            if (s.inNote != pitch)
                e->setAttribute ("in", s.inNote);
            if (s.outNote != pitch)
                e->setAttribute ("out", s.outNote);
            if (s.channel >= 0)
                e->setAttribute ("channel", s.channel);
            if (s.mute)
                e->setAttribute ("mute", true);
        }
        juce::StringArray pitches;
        for (const auto p : order)
            pitches.add (juce::String (p));
        xml->setAttribute ("order", pitches.joinIntoString (","));
        return xml;
    }

    namespace
    {
        // Every pitch once: what's given first (valid, not repeated), then the rest in order.
        std::vector<int> completeOrder (const std::vector<int>& given)
        {
            std::vector<int> order;
            std::array<bool, 128> seen {};
            for (const auto p : given)
                if (juce::isPositiveAndBelow (p, 128) && ! seen[(size_t) p])
                    order.push_back (p), seen[(size_t) p] = true;
            for (int p = 0; p < 128; ++p)
                if (! seen[(size_t) p])
                    order.push_back (p);
            return order;
        }
    }

    std::optional<DrumMap> DrumMap::fromXml (const juce::XmlElement& xml)
    {
        if (! xml.hasTagName ("DrumMap"))
            return std::nullopt;
        auto map = gm();
        map.name = xml.getStringAttribute ("name", "Drum Map");
        for (const auto* e : xml.getChildWithTagNameIterator ("Sound"))
        {
            const auto pitch = e->getIntAttribute ("pitch", -1);
            if (! juce::isPositiveAndBelow (pitch, 128))
                continue;
            auto& s = map.sounds[(size_t) pitch];
            s.name = e->getStringAttribute ("name");
            s.inNote = juce::jlimit (0, 127, e->getIntAttribute ("in", pitch));
            s.outNote = juce::jlimit (0, 127, e->getIntAttribute ("out", pitch));
            s.channel = juce::jlimit (-1, 15, e->getIntAttribute ("channel", -1));
            s.mute = e->getBoolAttribute ("mute");
        }
        std::vector<int> given;
        for (const auto& t : juce::StringArray::fromTokens (xml.getStringAttribute ("order"), ",", {}))
            given.push_back (t.getIntValue());
        map.order = completeOrder (given);
        return map;
    }

    //==========================================================================
    namespace
    {
        // In a .drm: <int name="INote" value="36"/>, <string name="Name" value="Kick"/>...
        const juce::XmlElement* named (const juce::XmlElement& parent, const juce::String& name)
        {
            for (const auto* child : parent.getChildIterator())
                if (child->getStringAttribute ("name") == name)
                    return child;
            return nullptr;
        }
    }

    std::optional<DrumMap> DrumMap::readDrm (const juce::String& text, juce::String& error)
    {
        const auto xml = juce::XmlDocument::parse (text);
        if (xml == nullptr || ! xml->hasTagName ("DrumMap"))
        {
            error = "It isn't a drum map file (.drm).";
            return std::nullopt;
        }
        auto map = gm();
        if (const auto* n = named (*xml, "Name"))
            map.name = n->getStringAttribute ("value").trim();
        if (map.name.isEmpty())
            map.name = "Drum Map";
        const auto* list = named (*xml, "Map");
        if (list == nullptr)
        {
            error = "The file has no drum sounds in it.";
            return std::nullopt;
        }
        // The items are the 128 pitches in order.
        auto pitch = 0;
        for (const auto* item : list->getChildWithTagNameIterator ("item"))
        {
            if (pitch >= 128)
                break;
            auto& s = map.sounds[(size_t) pitch];
            const auto intOf = [&] (const char* key, int fallback)
            {
                const auto* e = named (*item, key);
                return e != nullptr ? e->getIntAttribute ("value", fallback) : fallback;
            };
            s.inNote = juce::jlimit (0, 127, intOf ("INote", pitch));
            s.outNote = juce::jlimit (0, 127, intOf ("ONote", pitch));
            s.channel = juce::jlimit (-1, 15, intOf ("Channel", -1));
            s.mute = intOf ("Mute", 0) != 0;
            if (const auto* e = named (*item, "Name"))
                s.name = e->getStringAttribute ("value");
            ++pitch;
        }
        if (pitch == 0)
        {
            error = "The file has no drum sounds in it.";
            return std::nullopt;
        }
        std::vector<int> given;
        if (const auto* order = named (*xml, "Order"))
            for (const auto* item : order->getChildWithTagNameIterator ("item"))
                given.push_back (item->getIntAttribute ("value", -1));
        map.order = completeOrder (given);
        return map;
    }

    std::optional<DrumMap> DrumMap::readDrm (const juce::File& file, juce::String& error)
    {
        if (! file.existsAsFile())
        {
            error = "The file isn't there.";
            return std::nullopt;
        }
        auto map = readDrm (file.loadFileAsString(), error);
        if (map && (map->name.isEmpty() || map->name == "Drum Map"))
            map->name = file.getFileNameWithoutExtension();
        return map;
    }

    juce::String DrumMap::toDrm() const
    {
        // The same layout as the files we read (so the other DAW reads ours too, as far as we know its format).
        juce::XmlElement xml ("DrumMap");
        const auto add = [] (juce::XmlElement& parent, const char* tag, const char* key, const juce::String& value, bool wide = false)
        {
            auto* e = parent.createNewChildElement (tag);
            e->setAttribute ("name", key);
            e->setAttribute ("value", value);
            if (wide)
                e->setAttribute ("wide", "true");
        };
        add (xml, "string", "Name", name, true);
        auto* quantize = xml.createNewChildElement ("list");
        quantize->setAttribute ("name", "Quantize");
        quantize->setAttribute ("type", "list");
        auto* q = quantize->createNewChildElement ("item");
        add (*q, "int", "Grid", "4");
        add (*q, "int", "Type", "0");
        add (*q, "float", "Swing", "0");
        add (*q, "int", "Legato", "50");
        auto* list = xml.createNewChildElement ("list");
        list->setAttribute ("name", "Map");
        list->setAttribute ("type", "list");
        for (int pitch = 0; pitch < 128; ++pitch)
        {
            const auto& s = sounds[(size_t) pitch];
            auto* item = list->createNewChildElement ("item");
            add (*item, "int", "INote", juce::String (s.inNote));
            add (*item, "int", "ONote", juce::String (s.outNote));
            add (*item, "int", "Channel", juce::String (s.channel));
            add (*item, "float", "Length", "200");
            add (*item, "int", "Mute", s.mute ? "1" : "0");
            add (*item, "int", "DisplayNote", juce::String (pitch));
            add (*item, "int", "HeadSymbol", "0");
            add (*item, "int", "Voice", "0");
            add (*item, "int", "PortIndex", "0");
            add (*item, "string", "Name", s.name, true);
            add (*item, "int", "QuantizeIndex", "0");
        }
        auto* orderList = xml.createNewChildElement ("list");
        orderList->setAttribute ("name", "Order");
        orderList->setAttribute ("type", "int");
        for (const auto p : order)
            orderList->createNewChildElement ("item")->setAttribute ("value", p);
        add (xml, "int", "Flags", "0");
        return xml.toString();
    }

    bool DrumMap::writeDrm (const juce::File& file) const
    {
        return file.replaceWithText (toDrm());
    }
}
