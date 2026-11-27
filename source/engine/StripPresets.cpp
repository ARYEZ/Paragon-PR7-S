#include "engine/StripPresets.h"

#include "engine/strip/FactoryPresets.h"

namespace pr7s
{
    ChannelStrip stripFromPr7c (const std::vector<std::pair<juce::String, float>>& values, juce::StringArray* leftOut)
    {
        // PR7-C's defaults are the strip's (ChannelStrip's are PR7-C's Parameters.cpp), so a preset's values go
        // over a fresh strip as PR7-C puts them over its defaults.
        ChannelStrip s;
        bool hpfOn = false, lpfOn = false, onSidechain = false, eq = false, limiter = false, colourKnob = false;
        float hpf = 80.0f, lpf = 20000.0f;
        for (const auto& [id, v] : values)
        {
            const auto on = v > 0.5f;
            if (id == "compOn") s.comp.on = on;
            else if (id == "compThreshold") s.comp.thresholdDb = v;
            else if (id == "compRatio") s.comp.ratio = v;
            else if (id == "compAttack") s.comp.attackMs = v;
            else if (id == "compRelease") s.comp.releaseMs = v;
            else if (id == "compAuto") s.comp.autoRelease = on;
            else if (id == "compMakeUp") s.comp.makeUpDb = v;
            else if (id == "compCharacter") s.comp.character = juce::roundToInt (v);
            else if (id == "compDistortion") s.comp.distortion = juce::roundToInt (v);
            else if (id == "dynamicsLink") s.link = on;
            else if (id == "gateOn") s.gate.on = on;
            else if (id == "gateThreshold") s.gate.thresholdDb = v;
            else if (id == "gateMinus30") s.gate.minus30 = on;
            else if (id == "gateRange") s.gate.rangeDb = v;
            else if (id == "gateExpander") s.gate.expander = on;
            else if (id == "gateRatio") s.gate.ratio = v;
            else if (id == "gateAttack") s.gate.attack = juce::roundToInt (v);
            else if (id == "gateHold") s.gate.holdMs = v;
            else if (id == "gateRelease") s.gate.releaseMs = v;
            else if (id == "gateLinear") s.gate.linear = on;
            else if (id == "scOn") s.sc.on = on;
            else if (id == "scHpf") s.sc.hpf = v;
            else if (id == "scLpf") s.sc.lpf = v;
            else if (id == "scHighQ") s.sc.highQ = on;
            else if (id == "scEdge") s.sc.edge = on;
            else if (id == "satOn") s.sat.on = on;
            else if (id == "satDrive") s.sat.driveDb = v;
            else if (id == "satHarmonics") s.sat.harmonics = v;
            else if (id == "satMix") s.sat.mix = v;
            else if (id == "satVcaClip") s.sat.vcaClip = on;
            else if (id == "toneThd") { s.colour.thd = v; colourKnob = true; }
            else if (id == "toneVGain") { s.colour.vGain = v; colourKnob = true; }
            else if (id == "monoOn") s.stereo.monoOn = on;
            else if (id == "monoFreq") s.stereo.monoHz = v;
            else if (id == "widthOn") s.stereo.widthOn = on;
            else if (id == "width") s.stereo.width = v;
            else if (id == "hpfOn") hpfOn = on;
            else if (id == "hpfFreq") hpf = v;
            else if (id == "lpfOn") lpfOn = on;
            else if (id == "lpfFreq") lpf = v;
            else if (id == "filterPlacement") onSidechain = juce::roundToInt (v) == 2;
            else if (id.startsWith ("limit")) limiter = limiter || (id == "limitOn" && on);
            else if (id.startsWith ("hf") || id.startsWith ("hmf") || id.startsWith ("mf") || id.startsWith ("lmf") || id.startsWith ("lf"))
                eq = true;
        }
        // PR7-C's console colour is on by default (its lamp); the strip's only when something is set (THD or
        // V GAIN above 0), so a preset without colour costs nothing.
        s.colour.on = colourKnob && (s.colour.thd > 0.0f || s.colour.vGain > 0.0f);
        // HPF / LPF on SC filter only what the dynamics hear: the SC FILTER.
        if (onSidechain && (hpfOn || lpfOn))
        {
            s.sc.on = true;
            if (hpfOn) s.sc.hpf = hpf;
            if (lpfOn) s.sc.lpf = lpf;
        }
        if (leftOut != nullptr)
        {
            leftOut->clear();
            if (eq) leftOut->add ("EQ");
            if (hpfOn && ! onSidechain) leftOut->add ("HPF");
            if (lpfOn && ! onSidechain) leftOut->add ("LPF");
            if (limiter) leftOut->add ("LIMITER");
        }
        return s.limited();
    }

    const juce::StringArray& stripPresetGroups()
    {
        return pr7c::FactoryBanks::names;
    }

    const std::vector<StripPreset>& factoryStripPresets()
    {
        static const auto presets = []
        {
            std::vector<StripPreset> list;
            for (const auto& p : pr7c::factoryPresets())
            {
                if (p.bank < 0)
                    continue;
                juce::StringArray left;
                auto strip = stripFromPr7c (p.values, &left);
                if (! strip.inUse())
                    continue;
                list.push_back ({ p.name, pr7c::FactoryBanks::names[p.bank], p.slot(), left.joinIntoString (", "), strip, true, {} });
            }
            return list;
        }();
        return presets;
    }

    juce::File defaultStripPresetFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Paragon PR7-S").getChildFile ("Strip Presets");
    }

    std::optional<StripPreset> loadStripPreset (const juce::File& file)
    {
        const auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName ("StripPreset"))
            return std::nullopt;
        const auto* strip = xml->getChildByName ("Strip");
        if (strip == nullptr)
            return std::nullopt;
        StripPreset p;
        p.name = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
        p.group = "My Presets";
        p.strip = ChannelStrip::fromXml (*strip).limited();
        p.factory = false;
        p.file = file;
        return p;
    }

    std::vector<StripPreset> userStripPresets (const juce::File& folder)
    {
        std::vector<StripPreset> list;
        if (! folder.isDirectory())
            return list;
        for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.pr7strip"))
            if (auto p = loadStripPreset (f))
                list.push_back (std::move (*p));
        std::sort (list.begin(), list.end(), [] (const StripPreset& a, const StripPreset& b)
                   { return a.name.compareNatural (b.name) < 0; });
        return list;
    }

    std::optional<juce::File> saveStripPreset (const juce::File& folder, const juce::String& name, const ChannelStrip& strip, bool replace)
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        if (clean.isEmpty() || ! folder.createDirectory())
            return std::nullopt;
        auto file = folder.getChildFile (clean + ".pr7strip");
        for (int n = 2; ! replace && file.exists(); ++n)
            file = folder.getChildFile (clean + " " + juce::String (n) + ".pr7strip");
        juce::XmlElement xml ("StripPreset");
        xml.setAttribute ("name", name.trim());
        xml.addChildElement (strip.limited().toXml().release());
        if (! xml.writeTo (file))
            return std::nullopt;
        return file;
    }

    ChannelStrip appliedStripPreset (const ChannelStrip& now, const ChannelStrip& preset)
    {
        auto s = preset.limited();
        s.colour.channel = now.colour.channel;
        s.colour.digital = now.colour.digital;
        return s;
    }
}
