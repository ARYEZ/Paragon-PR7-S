#include "GuitarTones.h"

#include "engine/Markers.h"
#include "engine/SongSections.h"

namespace pr7s
{
    namespace
    {
        using Settings = std::vector<std::pair<juce::String, float>>;

        // Every tone starts from every effect block off (a tone is a known sound), then turns on what it uses.
        Settings base (bool acoustic)
        {
            Settings s { { "compOn", 0.0f }, { "modOn", 0.0f }, { "delayOn", 0.0f }, { "reverbOn", 0.0f },
                         { "mod2On", 0.0f }, { "fpOn", 0.0f }, { "lofiOn", 0.0f }, { "glitchOn", 0.0f } };
            if (acoustic)
            {
                // An acoustic shouldn't go through a guitar amp and cab.
                s.push_back ({ "ampOn", 0.0f });
                s.push_back ({ "cabOn", 0.0f });
            }
            return s;
        }

        void set (Settings& s, const juce::String& id, float value)
        {
            for (auto& [i, v] : s)
                if (i == id)
                {
                    v = value;
                    return;
                }
            s.push_back ({ id, value });
        }

        // The blocks (PR7-G's ids and ranges): the compressor (0 Classic, 1 Daybreak, 2 Bloom).
        void comp (Settings& s, int type, float threshold, float ratio, float level = 0.0f)
        {
            set (s, "compOn", 1.0f);
            set (s, "compType", (float) type);
            set (s, "compThreshold", threshold);
            set (s, "compRatio", ratio);
            set (s, "compLevel", level);
        }

        // Modulation (0 Vortex chorus ... 8 Harmonic Tremolo, 10 Doubler); free running (sync off).
        void mod (Settings& s, int type, float rate, float depth, float mix, bool stereo)
        {
            set (s, "modOn", 1.0f);
            set (s, "modType", (float) type);
            set (s, "modRate", rate);
            set (s, "modDepth", depth);
            set (s, "modMix", mix);
            set (s, "modStereo", stereo ? 1.0f : 0.0f);
            set (s, "modSync", 0.0f);
        }

        // The delay (0 Digital, 1 Analog, 2 Tape); `sync` PR7-G's divisions (0 off: `ms` then).
        void delay (Settings& s, int type, int sync, float ms, float feedback, float mix, bool pingPong, float duck, float tone = 6.0f)
        {
            set (s, "delayOn", 1.0f);
            set (s, "delayType", (float) type);
            set (s, "delaySync", (float) sync);
            set (s, "delayTime", ms);
            set (s, "delayFeedback", feedback);
            set (s, "delayTone", tone);
            set (s, "delayMix", mix);
            set (s, "delayPingPong", pingPong ? 1.0f : 0.0f);
            set (s, "delayDuck", duck);
        }

        // The reverb (1 Room, 2 Hall, 3 Cathedral, 4 Plate, 5 Spring).
        void reverb (Settings& s, int type, float size, float mix, float preDelay = 15.0f, float duck = 0.0f, float tone = 6.0f)
        {
            set (s, "reverbOn", 1.0f);
            set (s, "reverbType", (float) type);
            set (s, "reverbSize", size);
            set (s, "reverbTone", tone);
            set (s, "reverbMix", mix);
            set (s, "reverbPreDelay", preDelay);
            set (s, "reverbDuck", duck);
        }

        // PR7-G's delay divisions: 3 1/4, 4 1/4 dotted, 6 1/8, 7 1/8 dotted; 2 1/2.
        enum Sync { off = 0, half = 2, quarter = 3, dottedQuarter = 4, eighth = 6, dottedEighth = 7 };
        enum ModType { vortex = 0, harmonicTremolo = 8, doubler = 10 };
        enum DelayType { digital = 0, analog = 1, tape = 2 };
        enum ReverbType { room = 1, hall = 2, cathedral = 3, plate = 4, spring = 5 };
        enum CompType { classic = 0, bloom = 2 };

        std::vector<GuitarTone> makeTones()
        {
            using P = LineIdea::Part;
            std::vector<GuitarTone> tones;
            const auto add = [&tones] (const char* name, const char* description, P part, Settings s, std::vector<juce::String> words)
            {
                tones.push_back ({ name, description, part, std::move (s), std::move (words) });
            };

            // LEAD (SOARING LEAD the default).
            {
                auto s = base (false);
                delay (s, tape, dottedQuarter, 375.0f, 0.38f, 0.22f, false, 0.45f, 5.5f);
                reverb (s, hall, 0.62f, 0.2f, 30.0f, 0.3f);
                add ("SOARING LEAD", "A tape echo a dotted quarter behind and a hall: the echo ducks while you play and blooms in the gaps",
                     P::lead, s, { "soar", "bend", "scream", "wail", "anthem" });
            }
            {
                auto s = base (false);
                delay (s, digital, eighth, 250.0f, 0.18f, 0.12f, false, 0.6f, 7.0f);
                reverb (s, plate, 0.4f, 0.12f, 10.0f);
                add ("SHRED LEAD", "A short, tight digital echo on the eighths and a small plate: fast runs stay clear",
                     P::lead, s, { "shred", "fire", "run", "sextuplet", "sweep", "tap", "gypsy", "thrash", "prog", "flash", "stream",
                                   "gear", "burst", "cell", "climb", "storm" });
            }
            {
                auto s = base (false);
                delay (s, analog, off, 110.0f, 0.1f, 0.18f, false, 0.0f, 5.0f);
                reverb (s, spring, 0.35f, 0.14f, 5.0f);
                add ("BLUES LEAD", "A single slapback echo and a spring: close and dry, as a small amp in a room",
                     P::lead, s, { "blues", "sleaze", "swagger", "hammer", "shuffle", "talk", "riff", "stab", "stutter" });
            }
            {
                auto s = base (false);
                comp (s, bloom, -20.0f, 3.0f, 2.0f);
                mod (s, vortex, 0.5f, 0.3f, 0.25f, true);
                delay (s, tape, dottedQuarter, 375.0f, 0.5f, 0.3f, false, 0.3f, 5.0f);
                reverb (s, cathedral, 0.8f, 0.32f, 40.0f, 0.15f);
                add ("AMBIENT LEAD", "Sustain, a slow chorus, long tape echoes and a big space: for melodies that float",
                     P::lead, s, { "swell", "float", "ballad", "hymn", "melody", "chant", "cry", "warble", "singalong", "theme", "slow",
                                   "sing", "held" });
            }
            {
                auto s = base (false);
                delay (s, digital, dottedEighth, 280.0f, 0.25f, 0.15f, true, 0.4f, 6.5f);
                reverb (s, hall, 0.5f, 0.18f, 20.0f, 0.2f);
                add ("TWIN LEAD", "An echo bouncing left and right and a medium hall: room for a second guitar a third above",
                     P::lead, s, { "twin", "harmony", "third" });
            }

            // CLEAN GUITAR (CLEAN SHIMMER the default).
            {
                auto s = base (false);
                comp (s, classic, -20.0f, 3.0f, 2.0f);
                mod (s, vortex, 0.7f, 0.4f, 0.45f, true);
                delay (s, digital, dottedQuarter, 375.0f, 0.35f, 0.22f, true, 0.2f, 6.0f);
                reverb (s, hall, 0.6f, 0.25f, 25.0f);
                add ("CLEAN SHIMMER", "Even, wide and moving: a stereo chorus, a dotted echo bouncing side to side and a hall",
                     P::clean, s, { "arpeggio", "ringing", "picked", "still", "cluster", "sixteenth", "chord" });
            }
            {
                auto s = base (false);
                mod (s, harmonicTremolo, 4.0f, 0.5f, 1.0f, false);
                delay (s, analog, eighth, 250.0f, 0.2f, 0.12f, false, 0.0f, 5.0f);
                reverb (s, spring, 0.4f, 0.2f, 5.0f);
                add ("CLEAN PULSE", "A throbbing tremolo, a short echo and a spring: an old amp's clean channel",
                     P::clean, s, { "pulse", "pedal", "hook", "dyad", "walk", "sparse", "muted" });
            }
            {
                auto s = base (false);
                comp (s, bloom, -22.0f, 3.0f, 2.0f);
                delay (s, tape, half, 500.0f, 0.55f, 0.3f, false, 0.2f, 5.0f);
                reverb (s, cathedral, 0.85f, 0.38f, 40.0f);
                add ("CLEAN SWELL", "Long tape echoes and a huge space behind swells: the notes fade in and hang in the air",
                     P::clean, s, { "swell", "warble", "float" });
            }

            // ACOUSTIC GUITAR (ACOUSTIC ROOM the default; no amp, no cab).
            {
                auto s = base (true);
                comp (s, classic, -16.0f, 2.5f, 1.0f);
                reverb (s, room, 0.35f, 0.14f, 8.0f);
                add ("ACOUSTIC ROOM", "Gently evened out in a small room: as it'd sound miked up close",
                     P::acoustic, s, {});
            }
            {
                auto s = base (true);
                comp (s, classic, -18.0f, 3.0f, 1.5f);
                mod (s, doubler, 0.5f, 0.4f, 0.5f, true);
                reverb (s, plate, 0.45f, 0.16f, 12.0f);
                add ("ACOUSTIC WIDE", "Doubled out to both sides with a plate: strummed chords as a wall",
                     P::acoustic, s, { "strum", "big", "wall", "power", "eighth" });
            }
            {
                auto s = base (true);
                delay (s, digital, dottedQuarter, 375.0f, 0.2f, 0.1f, false, 0.3f, 6.0f);
                reverb (s, hall, 0.55f, 0.22f, 20.0f);
                add ("ACOUSTIC AIR", "A hall and a soft echo around picked notes: open and airy",
                     P::acoustic, s, { "pick", "arpeggio", "finger", "harmonic", "ring", "high", "nylon" });
            }
            return tones;
        }

        juce::XmlElement* paramOf (juce::XmlElement& settings, const juce::String& id)
        {
            for (auto* p : settings.getChildWithTagNameIterator ("PARAM"))
                if (p->getStringAttribute ("id") == id)
                    return p;
            return nullptr;
        }

        void setParam (juce::XmlElement& settings, const juce::String& id, double value)
        {
            auto* p = paramOf (settings, id);
            if (p == nullptr)
            {
                p = settings.createNewChildElement ("PARAM");
                p->setAttribute ("id", id);
            }
            p->setAttribute ("value", value);
        }

        bool isPr7gTree (const juce::XmlElement* xml) { return xml != nullptr && xml->hasTagName ("ParagonX1"); }

        // The plug-in's own bytes: copyXmlToBinary's block (magic, length, text, a zero), maybe JUCE's private data after it.
        size_t xmlBlockSize (const juce::MemoryBlock& data)
        {
            if (data.getSize() <= 9)
                return 0;
            const auto length = (size_t) juce::ByteOrder::littleEndianInt (static_cast<const char*> (data.getData()) + 4);
            return juce::jmin (data.getSize(), length + 9);
        }

        std::unique_ptr<juce::XmlElement> wrapperOf (const juce::MemoryBlock& state)
        {
            auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
            if (xml != nullptr && xml->hasTagName ("VST3PluginState"))
                return xml;
            return nullptr;
        }

        juce::MemoryBlock componentOf (const juce::XmlElement& wrapper)
        {
            juce::MemoryBlock block;
            if (const auto* component = wrapper.getChildByName ("IComponent"))
                block.fromBase64Encoding (component->getAllSubText().trim());
            return block;
        }
    }

    const std::vector<GuitarTone>& guitarTones()
    {
        static const auto tones = makeTones();
        return tones;
    }

    std::vector<const GuitarTone*> guitarTonesFor (LineIdea::Part part)
    {
        std::vector<const GuitarTone*> out;
        for (const auto& t : guitarTones())
            if (t.part == part)
                out.push_back (&t);
        return out;
    }

    const GuitarTone* toneNamed (const juce::String& name)
    {
        for (const auto& t : guitarTones())
            if (t.name.equalsIgnoreCase (name.trim()))
                return &t;
        return nullptr;
    }

    bool hasGuitarTones (LineIdea::Part part)
    {
        return part == LineIdea::Part::lead || part == LineIdea::Part::clean || part == LineIdea::Part::acoustic;
    }

    const GuitarTone& suggestedTone (LineIdea::Part part, const juce::String& style)
    {
        const auto tones = guitarTonesFor (hasGuitarTones (part) ? part : LineIdea::Part::lead);
        const auto text = style.toLowerCase();
        const GuitarTone* best = tones.front();
        auto bestCount = 0;
        for (const auto* t : tones)
        {
            auto count = 0;
            for (const auto& w : t->words)
                if (text.contains (w))
                    ++count;
            if (count > bestCount)
            {
                best = t;
                bestCount = count;
            }
        }
        return *best;
    }

    std::unique_ptr<juce::XmlElement> readPr7gSettings (const juce::MemoryBlock& state)
    {
        if (auto wrapper = wrapperOf (state))
        {
            const auto component = componentOf (*wrapper);
            auto xml = juce::AudioProcessor::getXmlFromBinary (component.getData(), (int) component.getSize());
            return isPr7gTree (xml.get()) ? std::move (xml) : nullptr;
        }
        auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
        return isPr7gTree (xml.get()) ? std::move (xml) : nullptr;
    }

    juce::MemoryBlock writePr7gSettings (const juce::XmlElement& settings, const juce::MemoryBlock& stateBefore)
    {
        juce::MemoryBlock own;
        juce::AudioProcessor::copyXmlToBinary (settings, own);
        auto wrapper = wrapperOf (stateBefore);
        if (wrapper == nullptr)
            return own;
        // The bytes after the plug-in's own (JUCE's private data: its bypass) kept.
        const auto before = componentOf (*wrapper);
        if (const auto mine = xmlBlockSize (before); mine > 0 && mine < before.getSize())
            own.append (static_cast<const char*> (before.getData()) + mine, before.getSize() - mine);
        auto* component = wrapper->getChildByName ("IComponent");
        if (component == nullptr)
            component = wrapper->createNewChildElement ("IComponent");
        component->deleteAllTextElements();
        component->addTextElement (own.toBase64Encoding());
        juce::MemoryBlock out;
        juce::AudioProcessor::copyXmlToBinary (*wrapper, out);
        return out;
    }

    void applyTone (juce::XmlElement& settings, const GuitarTone& tone)
    {
        for (const auto& [id, value] : tone.settings)
            setParam (settings, id, value);
        settings.setAttribute ("rigName", tone.name);
        settings.setAttribute ("rigFile", juce::String());
        settings.setAttribute ("rigFactory", juce::String());
        settings.setAttribute ("rigModified", 0);
    }

    bool applyRigFile (juce::XmlElement& settings, const juce::File& file)
    {
        const auto rig = juce::XmlDocument::parse (file);
        if (rig == nullptr || ! rig->hasTagName ("ParagonRig"))
            return false;
        for (const auto* p : rig->getChildWithTagNameIterator ("Param"))
            if (const auto id = p->getStringAttribute ("id"); id.isNotEmpty())
                setParam (settings, id, p->getDoubleAttribute ("value"));
        if (! rig->getBoolAttribute ("keepAmpAndCab"))
        {
            if (const auto amp = rig->getStringAttribute ("ampModel"); amp.isNotEmpty())
                settings.setAttribute ("ampModelPath", amp);
            if (const auto cab = rig->getStringAttribute ("cabIr"); cab.isNotEmpty())
                settings.setAttribute ("cabIrPath", cab);
        }
        settings.setAttribute ("rigName", rig->getStringAttribute ("name", file.getFileNameWithoutExtension()));
        settings.setAttribute ("rigFile", file.getFullPathName());
        settings.setAttribute ("rigFactory", juce::String());
        settings.setAttribute ("rigModified", 0);
        return true;
    }

    juce::File pr7gRigFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
              #if JUCE_MAC
               .getChildFile ("Application Support")
              #endif
               .getChildFile ("Paragon").getChildFile ("Paragon PR7-G").getChildFile ("Rigs");
    }

    juce::Array<juce::File> pr7gRigFiles()
    {
        auto files = pr7gRigFolder().findChildFiles (juce::File::findFiles, true, "*.pr7rig");
        std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
                   { return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0; });
        return files;
    }

    bool isPr7g (const juce::PluginDescription& d)
    {
        return d.name.containsIgnoreCase ("PR7-G");
    }

    //==========================================================================
    // (0.205.0) The tones change with the sections.
    SectionToneScale sectionToneScale (const juce::String& sectionName)
    {
        const auto base = sectionBaseOf (sectionName);
        if (base.isEmpty())
            return {};
        if (base == "Pre-Chorus")
            return { 0.9f, 0.85f, 0.95f, 0.9f, 1.0f };
        if (base == "Post-Chorus" || base == "Interlude" || base == "Instrumental")
            return { 1.1f, 1.2f, 1.05f, 1.2f, 1.1f };
        switch (composerSectionFor (base))
        {
            case 0:  return { 1.0f, 1.3f, 1.1f, 1.4f, 1.2f };     // intro: wide open
            case 1:  return { 0.8f, 0.6f, 0.85f, 0.7f, 0.9f };    // verse: drier, room for the vocal
            case 2:  return {};                                   // chorus: the tone as chosen
            case 3:  return { 1.2f, 1.25f, 1.1f, 1.3f, 1.15f };   // bridge: wetter, somewhere else
            case 4:  return { 0.6f, 0.4f, 0.7f, 0.5f, 0.8f };     // breakdown: dry and tight
            case 5:  return { 1.0f, 1.3f, 1.2f, 1.5f, 1.25f };    // outro: the tails ring out
            case 6:  return { 1.0f, 1.15f, 1.05f, 0.9f, 1.0f };   // solo: more echo, a closer room
            default: return {};
        }
    }

    const std::vector<SectionToneParameter>& sectionToneParameters()
    {
        // PR7-G's Parameters.cpp: all linear from 0.
        static const std::vector<SectionToneParameter> list {
            { "modMix", "Mod Mix", 1.0f }, { "delayMix", "Delay Mix", 1.0f }, { "delayFeedback", "Delay Feedback", 0.95f },
            { "reverbMix", "Reverb Mix", 1.0f }, { "reverbSize", "Reverb Size", 1.0f } };
        return list;
    }

    std::map<juce::String, float> sectionToneValues (const std::map<juce::String, float>& baseline, const SectionToneScale& scale)
    {
        const float factors[] { scale.mod, scale.delay, scale.feedback, scale.reverb, scale.size };
        std::map<juce::String, float> values;
        const auto& params = sectionToneParameters();
        for (size_t i = 0; i < params.size(); ++i)
            if (const auto found = baseline.find (params[i].id); found != baseline.end())
                values[params[i].id] = juce::jlimit (0.0f, params[i].maximum, found->second * factors[i]);
        return values;
    }

    std::map<juce::String, float> sectionToneBaseline (const juce::XmlElement& settings)
    {
        std::map<juce::String, float> baseline;
        if (const auto* tone = toneNamed (settings.getStringAttribute ("rigName")))
        {
            for (const auto& [id, value] : tone->settings)
                baseline[id] = value;
            return baseline;
        }
        for (const auto* p : settings.getChildWithTagNameIterator ("PARAM"))
            if (const auto id = p->getStringAttribute ("id"); id.isNotEmpty())
                baseline[id] = (float) p->getDoubleAttribute ("value");
        return baseline;
    }
}
