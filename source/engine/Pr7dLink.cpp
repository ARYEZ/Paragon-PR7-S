#include "Pr7dLink.h"

#include <algorithm>
#include <cmath>

namespace pr7s
{
    namespace
    {
        // PR7-D's song parts (its grooves::Part order: beat, intro, verse, pre-chorus, chorus, bridge, fill, ending,
        // count-in), from the part's name (the composer names its parts "<Section>: <style>").
        int songPartOf (const juce::String& name)
        {
            const auto n = name.toLowerCase();
            if (n.startsWith ("intro")) return 1;
            if (n.startsWith ("pre-chorus") || n.startsWith ("pre chorus")) return 3;
            if (n.startsWith ("verse")) return 2;
            if (n.startsWith ("chorus") || n.startsWith ("refrain") || n.startsWith ("hook")) return 4;
            if (n.startsWith ("bridge") || n.startsWith ("middle") || n.startsWith ("interlude") || n.startsWith ("breakdown")
                || n.startsWith ("solo") || n.startsWith ("instrumental") || n.startsWith ("break")) return 5;
            if (n.startsWith ("ending") || n.startsWith ("outro") || n.startsWith ("coda")) return 7;
            return 0;
        }

        std::vector<const Part*> inOrder (const std::vector<Part>& copies)
        {
            std::vector<const Part*> out;
            for (const auto& p : copies)
                out.push_back (&p);
            std::stable_sort (out.begin(), out.end(), [] (const Part* a, const Part* b) { return a->start < b->start; });
            return out;
        }
    }

    juce::File pr7dLinkFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
              #if JUCE_MAC
               .getChildFile ("Application Support")
              #endif
               .getChildFile ("Paragon PR7-S").getChildFile ("PR7-D Link");
    }

    juce::MidiMessage pr7dLinkMessage (Pr7dCommand command, const juce::String& path)
    {
        juce::MemoryBlock data;
        const juce::uint8 head[] { 0x7D, 'P', 'R', '7', 'S', (juce::uint8) command };
        data.append (head, sizeof (head));
        if (command == Pr7dCommand::load)
        {
            const auto* utf8 = path.toRawUTF8();
            for (size_t i = 0; utf8[i] != 0; ++i)
            {
                const auto byte = (juce::uint8) utf8[i];
                const juce::uint8 halves[2] { (juce::uint8) (byte >> 4), (juce::uint8) (byte & 0x0F) };
                data.append (halves, 2);
            }
        }
        return juce::MidiMessage::createSysExMessage (data.getData(), (int) data.getSize());
    }

    std::unique_ptr<juce::XmlElement> pr7dSongXml (const std::vector<Part>& copies, bool pr7dMap, int numerator, int denominator,
                                                   const std::vector<SignatureEvent>& changes, const juce::String& linkPath)
    {
        auto song = std::make_unique<juce::XmlElement> ("Song");
        song->setAttribute ("num", numerator);
        song->setAttribute ("den", denominator);
        if (! changes.empty())
        {
            const SignatureMap map (numerator, denominator, changes);
            juce::StringArray meters;
            for (const auto& c : changes)
                meters.add (juce::String (map.beatOfBar (c.bar), 6) + ":" + juce::String (c.numerator) + ":" + juce::String (c.denominator));
            song->setAttribute ("meters", meters.joinIntoString (" "));
        }
        song->setAttribute ("studioLink", linkPath);
        const auto layout = pr7dMap ? 1 : 2;
        const auto parts = inOrder (copies);
        for (size_t i = 0; i < parts.size(); ++i)
        {
            const auto& part = *parts[i];
            auto length = part.length;
            if (i + 1 < parts.size())
                length = juce::jmin (length, parts[i + 1]->start - part.start);
            if (length < 0.25)
                continue;   // (PR7-D's shortest block)
            auto* block = song->createNewChildElement ("Block");
            block->setAttribute ("name", part.name);
            block->setAttribute ("source", "");
            block->setAttribute ("part", songPartOf (part.name));
            block->setAttribute ("start", part.start);
            block->setAttribute ("length", length);
            block->setAttribute ("loop", length);
            block->setAttribute ("muted", false);
            std::vector<const Note*> notes;
            for (const auto& n : part.notes)
                if (! n.muted && n.start >= 0.0 && n.start < length)
                    notes.push_back (&n);
            std::stable_sort (notes.begin(), notes.end(), [] (const Note* a, const Note* b) { return a->start < b->start; });
            juce::StringArray hits;
            for (const auto* n : notes)
                if (const auto slot = drumSlotOf (part.heardPitch (n->pitch), layout); slot >= 0)
                    hits.add (juce::String (n->start, 6) + ":" + juce::String (slot) + ":"
                              + juce::String (juce::jlimit (0.0f, 1.0f, part.heardVelocity (n->velocity)), 4));   // (no length: as PR7-D's own grooves)
            block->setAttribute ("hits", hits.joinIntoString (" "));
        }
        return song;
    }

    std::vector<Part> pr7dPartsFromSong (const juce::XmlElement& song, bool pr7dMap)
    {
        const auto target = pr7dMap ? DrumNotes::pr7d : DrumNotes::generalMidi;
        std::vector<Part> out;
        for (const auto* e : song.getChildWithTagNameIterator ("Block"))
        {
            Part part;
            part.name = e->getStringAttribute ("name");
            part.start = juce::jmax (0.0, e->getDoubleAttribute ("start"));
            part.length = juce::jmax (0.25, e->getDoubleAttribute ("length", 4.0));
            part.muted = true;
            part.linkCopy = true;
            const auto loop = juce::jmax (0.25, e->getDoubleAttribute ("loop", part.length));
            const auto blockMuted = e->getBoolAttribute ("muted");
            struct Hit { double beat, length; int slot; float velocity; };
            std::vector<Hit> hits;
            for (const auto& token : juce::StringArray::fromTokens (e->getStringAttribute ("hits"), " ", {}))
            {
                const auto f = juce::StringArray::fromTokens (token, ":", {});
                if (f.size() == 3 || f.size() == 4)
                    hits.push_back ({ juce::jmax (0.0, f[0].getDoubleValue()), f.size() == 4 ? juce::jmax (0.0, f[3].getDoubleValue()) : 0.0,
                                      f[1].getIntValue(), juce::jlimit (0.0f, 1.0f, f[2].getFloatValue()) });
            }
            // Its loop played round to its length.
            for (auto from = 0.0; from < part.length - 1.0e-9; from += loop)
                for (const auto& h : hits)
                {
                    const auto at = from + h.beat;
                    if (h.beat >= loop - 1.0e-9 || at >= part.length - 1.0e-9)
                        continue;
                    if (const auto note = drumNoteOf (h.slot, target); note >= 0)
                        part.notes.push_back ({ at, h.length > 0.0 ? h.length : 0.125, note, h.velocity, blockMuted });
                }
            // EDIT PLAY STYLE's opening hit: once, at the start.
            if (e->getBoolAttribute ("opening", false))
                if (const auto note = drumNoteOf (e->getIntAttribute ("openingSlot", 11), target); note >= 0)
                    part.notes.push_back ({ 0.0, 0.125, note, (float) juce::jlimit (0.0, 1.0, e->getDoubleAttribute ("openingVelocity", 0.9)), blockMuted });
            std::stable_sort (part.notes.begin(), part.notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
            out.push_back (std::move (part));
        }
        std::stable_sort (out.begin(), out.end(), [] (const Part& a, const Part& b) { return a.start < b.start; });
        return out;
    }

    juce::String pr7dCopiesFingerprint (const std::vector<Part>& copies)
    {
        juce::String text;
        for (const auto* p : inOrder (copies))
        {
            text << p->start << "/" << p->length << "/" << p->name << "/" << p->transpose << "/" << p->velocityOffset << "|";
            for (const auto& n : p->notes)
                text << n.start << ":" << n.pitch << ":" << n.velocity << ":" << n.length << (n.muted ? "m" : "") << ",";
            text << ";";
        }
        return juce::String::toHexString (text.hashCode64()) + "-" + juce::String (text.length());
    }
}
