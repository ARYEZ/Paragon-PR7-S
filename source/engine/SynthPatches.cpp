#include "engine/SynthPatches.h"

#include "engine/LaneWorlds.h"

namespace pr7s
{
    namespace
    {
        using Values = std::map<juce::String, float>;

        juce::String osc (int n, const char* what) { return "osc" + juce::String (n) + what; }

        // PR7-K's indices (its Filter.h, SynthSettings.h, Delay.h, Reverb.h: saved by index, only ever appended).
        constexpr float saw = 0, pulse = 1, triangle = 2, sine = 3;
        constexpr float lp12 = 1, lp24 = 3;
        constexpr float sync1_4dotted = 4, sync1_8dotted = 7, sync1_8triplet = 8, sync1_16 = 9;
        constexpr float room = 1, hall = 2, cathedral = 3, plate = 4;

        // What every patch starts from: PR7-K's own defaults for the knobs a patch may touch (its Parameters.cpp and
        // SynthSettings.h), for the first three oscillators.
        Values base()
        {
            Values v;
            for (int n = 1; n <= 3; ++n)
            {
                v[osc (n, "On")] = 0; v[osc (n, "Wave")] = saw; v[osc (n, "Volume")] = 1; v[osc (n, "Pan")] = 0;
                v[osc (n, "Transpose")] = 0; v[osc (n, "Fine")] = 0; v[osc (n, "Random")] = 0; v[osc (n, "Gain")] = 1;
                v[osc (n, "Width")] = 0.5f; v[osc (n, "Noise")] = 0; v[osc (n, "NoiseColour")] = 0; v[osc (n, "Sub")] = 0;
                v[osc (n, "SubOct")] = 1; v[osc (n, "UniVoices")] = 7; v[osc (n, "UniMix")] = 0; v[osc (n, "UniDetune")] = 0.3f;
                v[osc (n, "UniSpread")] = 0.5f; v[osc (n, "UniOctaves")] = 0; v[osc (n, "VibAmount")] = 0; v[osc (n, "VibRate")] = 5;
                v[osc (n, "VibFade")] = 0; v[osc (n, "Skew")] = 0; v[osc (n, "Pinch")] = 0; v[osc (n, "Formant")] = 0;
                v[osc (n, "Crush")] = 0; v[osc (n, "SyncAmount")] = 0; v[osc (n, "FmAmount")] = 0; v[osc (n, "AmAmount")] = 0;
                v[osc (n, "Filter")] = 1; v[osc (n, "Filter2")] = 0; v[osc (n, "Arp")] = 0; v[osc (n, "Insert")] = 0;
                v[osc (n, "SendDelay")] = 1; v[osc (n, "SendReverb")] = 1;
            }
            v["flt1On"] = 0; v["delayOn"] = 0; v["reverbOn"] = 0;   // (written even where a state lacks them)
            v["flt1Type"] = lp24; v["flt1Cutoff"] = 2000; v["flt1Res"] = 0; v["flt1Drive"] = 0; v["flt1Key"] = 0; v["flt1Vel"] = 0;
            v["flt1ResVel"] = 0; v["flt1Env"] = 0; v["flt1EnvVel"] = 0; v["flt1LowCut"] = 20;
            v["flt1Attack"] = 0; v["flt1Hold"] = 0; v["flt1Decay"] = 300; v["flt1Sustain"] = 1; v["flt1Release"] = 200;
            v["amp1Volume"] = 1; v["amp1Vel"] = 1; v["amp1Pan"] = 0; v["amp1Spread"] = 0; v["amp1Spike"] = 0;
            v["amp1Attack"] = 1; v["amp1Hold"] = 0; v["amp1Decay"] = 300; v["amp1Sustain"] = 1; v["amp1Release"] = 200;
            v["amp1AttackCurve"] = 0; v["amp1DecayCurve"] = 0; v["amp1ReleaseCurve"] = 0;
            v["playMode"] = 0; v["legato"] = 1; v["glide"] = 0; v["voices"] = 32; v["patchLevel"] = 0;
            v["delayPingPong"] = 0; v["delayFeedback"] = 0.35f; v["delayTone"] = 7; v["delayMix"] = 0.25f; v["delaySync"] = 3;
            v["delayToReverb"] = 0;
            v["reverbType"] = 0; v["reverbSize"] = 0.5f; v["reverbTone"] = 6; v["reverbMix"] = 0.28f; v["reverbPreDelay"] = 20;
            return v;
        }

        // A patch being written: the clean slate, then its own values.
        struct Patch
        {
            SynthPatch p;
            Patch (const char* name, const char* description) { p.name = name; p.description = description; p.settings = base(); }
            Patch& set (const juce::String& id, float value) { p.settings[id] = value; return *this; }
            Patch& o (int n, const char* what, float value) { return set (osc (n, what), value); }
            Patch& voice (int n, float wave, float volume = 1)
            {
                return o (n, "On", 1).o (n, "Wave", wave).o (n, "Volume", volume);
            }
            Patch& unison (int n, float voices, float mix, float detune, float spread)
            {
                return o (n, "UniVoices", voices).o (n, "UniMix", mix).o (n, "UniDetune", detune).o (n, "UniSpread", spread);
            }
            Patch& filter (float type, float hz, float res, float env, float key = 0.3f)
            {
                return set ("flt1On", 1).set ("flt1Type", type).set ("flt1Cutoff", hz).set ("flt1Res", res).set ("flt1Env", env)
                      .set ("flt1Key", key);
            }
            Patch& filterEnv (float a, float d, float s, float r)
            {
                return set ("flt1Attack", a).set ("flt1Decay", d).set ("flt1Sustain", s).set ("flt1Release", r);
            }
            Patch& amp (float a, float d, float s, float r)
            {
                return set ("amp1Attack", a).set ("amp1Decay", d).set ("amp1Sustain", s).set ("amp1Release", r);
            }
            Patch& delay (float sync, float feedback, float mix, bool pingPong = true)
            {
                return set ("delayOn", 1).set ("delaySync", sync).set ("delayFeedback", feedback).set ("delayMix", mix)
                      .set ("delayPingPong", pingPong ? 1.0f : 0.0f).set ("delayTone", 6);
            }
            Patch& reverb (float type, float size, float mix)
            {
                return set ("reverbOn", 1).set ("reverbType", type).set ("reverbSize", size).set ("reverbMix", mix);
            }
            Patch& level (float db) { return set ("patchLevel", db); }
            Patch& pump()
            {
                p.pumping = true;
                return set ("amp1Volume", 0);   // the EXPRESSION route brings it to unity
            }
        };

        std::vector<SynthPatch> makePatches()
        {
            std::vector<SynthPatch> out;
            const auto add = [&out] (Patch& p) { out.push_back (p.p); };

            Patch arp ("CYBER PLUCK ARP", "Bright detuned saws with a pulse an octave up, a resonant filter snapping shut on every note; ping-pong echoes in dotted eighths and a hall");
            arp.voice (1, saw, 0.8f).unison (1, 3, 0.5f, 0.2f, 0.7f).voice (2, pulse, 0.4f).o (2, "Transpose", 12).o (2, "Width", 0.35f)
               .filter (lp24, 900, 0.45f, 0.45f, 0.5f).filterEnv (0, 220, 0.1f, 180).amp (1, 350, 0.2f, 250)
               .delay (sync1_8dotted, 0.35f, 0.25f).reverb (hall, 0.55f, 0.2f).level (-2);
            add (arp);

            Patch stab ("CYBER GATED STAB", "Five wide saws over a saw an octave down, short and bright: chord stabs that stop dead, a plate around them");
            stab.voice (1, saw, 0.6f).unison (1, 5, 0.7f, 0.35f, 0.8f).voice (2, saw, 0.5f).o (2, "Transpose", -12)
                .filter (lp24, 1800, 0.3f, 0.3f).filterEnv (0, 150, 0, 120).amp (0.5f, 180, 0, 90)
                .reverb (plate, 0.4f, 0.18f).level (-3);
            add (stab);

            Patch bass ("CYBER PULSE BASS", "A saw and a pulse an octave down with a sine sub, a driven resonant filter that closes after every note: the sequenced bass");
            bass.voice (1, saw, 0.8f).o (1, "Sub", 0.5f).voice (2, pulse, 0.6f).o (2, "Transpose", -12)
                .filter (lp24, 600, 0.35f, 0.4f).set ("flt1Drive", 0.3f).filterEnv (0, 180, 0.15f, 80).amp (1, 200, 0.7f, 60).level (-2);
            add (bass);

            Patch pad ("CYBER PUMP PAD", "Seven very wide saws with five more an octave up, soft and slow: a pad whose level ducks on the part's CC 11 (the kick's pump)");
            pad.voice (1, saw, 0.5f).unison (1, 7, 0.8f, 0.4f, 1).voice (2, saw, 0.3f).o (2, "Transpose", 12).unison (2, 5, 0.6f, 0.3f, 1)
               .filter (lp12, 2600, 0.1f, 0).amp (120, 800, 0.9f, 700).set ("amp1ReleaseCurve", -0.4f)
               .reverb (hall, 0.7f, 0.3f).pump().level (-4);
            add (pad);

            Patch lead ("CYBER OCTAVE LEAD", "A narrow pulse with three copies over a saw an octave down, vibrato growing in: the hook high over the band, echoed");
            lead.voice (1, pulse, 0.7f).o (1, "Width", 0.42f).unison (1, 3, 0.4f, 0.15f, 0.5f).o (1, "VibAmount", 10).o (1, "VibRate", 5.5f)
                .o (1, "VibFade", 300).voice (2, saw, 0.5f).o (2, "Transpose", -12)
                .filter (lp24, 3200, 0.25f, 0.2f, 0.5f).filterEnv (5, 500, 0.5f, 250).amp (3, 300, 0.85f, 250)
                .delay (sync1_8dotted, 0.3f, 0.22f).reverb (plate, 0.5f, 0.2f).level (-3);
            add (lead);

            Patch riser ("CYBER RISER", "Seven detuned saws with white noise through a resonant filter for the part's sweep, sixteenth echoes feeding a hall");
            riser.voice (1, saw, 0.6f).unison (1, 7, 0.7f, 0.45f, 0.9f).o (1, "Noise", 0.25f)
                 .filter (lp24, 400, 0.55f, 0, 0.2f).amp (1, 200, 0.8f, 400)
                 .delay (sync1_16, 0.45f, 0.25f).set ("delayToReverb", 0.3f).reverb (hall, 0.65f, 0.28f).level (-4);
            add (riser);

            Patch glitch ("CYBER GLITCH", "A thin crushed pulse and a saw an octave up, a resonant filter that snaps: short digital blips, sixteenth echoes");
            glitch.voice (1, pulse, 0.8f).o (1, "Width", 0.2f).o (1, "Crush", 0.3f).voice (2, saw, 0.4f).o (2, "Transpose", 12)
                  .filter (lp24, 2400, 0.5f, 0.3f).filterEnv (0, 90, 0, 60).amp (0, 120, 0, 40)
                  .delay (sync1_16, 0.2f, 0.18f).reverb (room, 0.3f, 0.12f).level (-3);
            add (glitch);

            Patch dark ("CYBER DARK PAD", "Wide saws over a triangle an octave down and a quiet pulse an octave up, the filter creeping open: a dark, slow cathedral pad");
            dark.voice (1, saw, 0.5f).unison (1, 7, 0.7f, 0.25f, 0.9f).voice (2, triangle, 0.5f).o (2, "Transpose", -12)
                .voice (3, pulse, 0.2f).o (3, "Transpose", 12).o (3, "Width", 0.3f).o (3, "Fine", 7)
                .filter (lp24, 900, 0.2f, 0.25f).filterEnv (1500, 2500, 0.6f, 2000).amp (900, 1500, 0.85f, 2500)
                .set ("amp1ReleaseCurve", -0.4f).delay (sync1_4dotted, 0.4f, 0.15f).reverb (cathedral, 0.8f, 0.35f).level (-4);
            add (dark);

            Patch drop ("CYBER SUB DROP", "A sine with a sine sub for the drops, and three saws an octave up for the stabs, through a driven filter that closes");
            drop.voice (1, sine, 0.9f).o (1, "Sub", 0.6f).voice (2, saw, 0.45f).o (2, "Transpose", 12).unison (2, 3, 0.5f, 0.2f, 0.6f)
                .filter (lp24, 1200, 0.3f, 0.35f).set ("flt1Drive", 0.4f).filterEnv (0, 250, 0, 150).amp (1, 600, 0.6f, 200)
                .reverb (room, 0.3f, 0.1f).level (-2);
            add (drop);

            Patch seq ("CYBER SEQUENCE", "A saw and a detuned narrow pulse through a resonant filter for the part's slow wave, closing after every step; echoes in dotted eighths");
            seq.voice (1, saw, 0.8f).voice (2, pulse, 0.6f).o (2, "Width", 0.3f).o (2, "Fine", 8)
               .filter (lp24, 1100, 0.5f, 0.35f).filterEnv (0, 160, 0.1f, 100).amp (1, 200, 0.4f, 120)
               .delay (sync1_8dotted, 0.3f, 0.2f).reverb (plate, 0.4f, 0.15f).level (-2);
            add (seq);

            Patch kick ("CYBER KICK CHORD", "Five wide saws with a pulse an octave up, the filter snapping open on each hit and the chord dying fast");
            kick.voice (1, saw, 0.6f).unison (1, 5, 0.6f, 0.3f, 0.8f).voice (2, pulse, 0.3f).o (2, "Transpose", 12)
                .filter (lp24, 2200, 0.25f, 0.4f).filterEnv (0, 200, 0, 150).amp (0.5f, 260, 0, 160)
                .reverb (plate, 0.45f, 0.2f).level (-3);
            add (kick);

            Patch bell ("CYBER BELL", "A sine with a sine an octave up and a quiet triangle two octaves up, ringing away: a glassy bell (the echoes are in the part)");
            bell.voice (1, sine, 0.8f).voice (2, sine, 0.4f).o (2, "Transpose", 12).voice (3, triangle, 0.35f).o (3, "Transpose", 24)
                .o (3, "Fine", 3).amp (0, 1200, 0, 900).set ("amp1DecayCurve", -0.5f).reverb (hall, 0.6f, 0.3f).level (-2);
            add (bell);

            Patch fade ("CYBER FADE ARP", "Soft detuned saws and a triangle an octave up, the filter closing as the part fades; long ping-pong echoes and a hall");
            fade.voice (1, saw, 0.7f).unison (1, 3, 0.5f, 0.2f, 0.7f).voice (2, triangle, 0.4f).o (2, "Transpose", 12)
                .filter (lp24, 1400, 0.35f, 0.3f).filterEnv (0, 300, 0.2f, 300).amp (1, 400, 0.2f, 400)
                .delay (sync1_8dotted, 0.45f, 0.3f).reverb (hall, 0.7f, 0.3f).level (-3);
            add (fade);

            jassert ((int) out.size() == cyberpunkKeysOptions().size());
            return out;
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
    }

    const std::vector<SynthPatch>& cyberpunkPatches()
    {
        static const auto patches = makePatches();
        return patches;
    }

    const SynthPatch* cyberpunkPatchFor (int option)
    {
        const auto& all = cyberpunkPatches();
        return option >= 0 && option < (int) all.size() ? &all[(size_t) option] : nullptr;
    }

    std::optional<int> cyberpunkOptionOfStyle (const juce::String& style)
    {
        const auto text = style.trim();
        if (! text.startsWithIgnoreCase ("Cyberpunk:"))
            return std::nullopt;
        const auto name = text.fromFirstOccurrenceOf (":", false, false).trim();
        const auto& options = cyberpunkKeysOptions();
        for (int i = 0; i < options.size(); ++i)
            if (options[i].equalsIgnoreCase (name))
                return i;
        return std::nullopt;
    }

    bool isCyberpunkPatchName (const juce::String& rigName)
    {
        for (const auto& p : cyberpunkPatches())
            if (p.name.equalsIgnoreCase (rigName.trim()))
                return true;
        return false;
    }

    bool isPr7k (const juce::PluginDescription& d)
    {
        return d.name.containsIgnoreCase ("PR7-K");
    }

    juce::String synthPatchNameOf (const juce::XmlElement& settings)
    {
        return settings.getStringAttribute ("rigName");
    }

    bool mayReplaceSynthPatch (const juce::XmlElement& settings)
    {
        const auto name = synthPatchNameOf (settings).trim();
        return name.isEmpty() || name.equalsIgnoreCase ("Init") || isCyberpunkPatchName (name);
    }

    void applySynthPatch (juce::XmlElement& settings, const SynthPatch& patch)
    {
        // The clean slate: every switch off (the oscillators, effects, arps, sequencers, signature effects...).
        for (auto* p : settings.getChildWithTagNameIterator ("PARAM"))
            if (p->getStringAttribute ("id").endsWith ("On"))
                p->setAttribute ("value", 0.0);
        // What a sound loads beside its knobs, and its old modulation.
        for (const auto* tag : { "Wavetables", "SfzInstruments", "Spectra", "Drawings", "Modulation" })
            while (auto* child = settings.getChildByName (tag))
                settings.removeChildElement (child, true);
        for (const auto& [id, value] : patch.settings)
            setParam (settings, id, value);
        if (patch.pumping)
        {
            auto* matrix = settings.createNewChildElement ("Modulation");
            auto* route = matrix->createNewChildElement ("Route");
            route->setAttribute ("on", 1);
            route->setAttribute ("source", "expression");
            route->setAttribute ("destination", "amp1Volume");
            route->setAttribute ("amount", 0.5);
            route->setAttribute ("percent", 0);
            route->setAttribute ("lag", 0.0);
            route->setAttribute ("curve", 0.0);
        }
        // MIDI LEARN: the instance's links kept (an empty list isn't saved), CC 74 onto the filter's cutoff.
        auto* links = settings.getChildByName ("MidiLinks");
        if (links == nullptr)
            links = settings.createNewChildElement ("MidiLinks");
        for (auto* l = links->getFirstChildElement(); l != nullptr;)
        {
            auto* next = l->getNextElement();
            if (l->getIntAttribute ("cc", -1) == 74 || l->getStringAttribute ("parameter") == "flt1Cutoff")
                links->removeChildElement (l, true);
            l = next;
        }
        auto* link = links->createNewChildElement ("Link");
        link->setAttribute ("cc", 74);
        link->setAttribute ("parameter", "flt1Cutoff");
        settings.setAttribute ("rigName", patch.name);
        settings.setAttribute ("rigFile", juce::String());
        settings.setAttribute ("rigFactory", juce::String());
        settings.setAttribute ("rigModified", 0);
    }
}
