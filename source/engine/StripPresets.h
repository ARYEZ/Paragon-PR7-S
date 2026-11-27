#pragma once

#include <optional>
#include <utility>
#include <vector>

#include "engine/ChannelStrip.h"

namespace pr7s
{
    // (0.249.0) STRIP PRESETS: a channel strip's settings by name (the owner's choice: presets of the strip's
    // own, PR7-C kept as a plug-in). Ours are PR7-C's factory presets (engine/strip/FactoryPresets, copied from
    // PR7-C) turned into strips: what the strip has carries over as PR7-C sets it (its compressor, gate / exp,
    // SC filter, saturation, console colour, mono maker and width, in the same units and ranges); what it
    // hasn't (PR7-C's five-band EQ, HPF / LPF on the sound, limiter) is left out and named in `leftOut`, so the
    // channel's own EQ and BOOSTER can be set by hand. PR7-C's HPF / LPF set to SC (on what the dynamics hear)
    // become the SC FILTER. A preset with nothing the strip has (an EQ only) isn't listed. The yours: one file
    // each (.pr7strip) in the presets folder. Applying never changes a strip's console channel or ANALOG /
    // DIGITAL (they're the channel's own, as on a desk).
    struct StripPreset
    {
        juce::String name;
        juce::String group;      // a bank's name ("Drums", "Mastering"...), or "My Presets"
        juce::String slot;       // PR7-C's slot ("A01"); empty for yours
        juce::String leftOut;    // "EQ, HPF" (what PR7-C's preset has that the strip doesn't); empty when nothing
        ChannelStrip strip;
        bool factory = true;
        juce::File file;         // yours
    };

    // PR7-C's parameter values (its ids, its units) as a strip; what wasn't taken into `leftOut` ("EQ", "HPF",
    // "LPF", "LIMITER").
    ChannelStrip stripFromPr7c (const std::vector<std::pair<juce::String, float>>& values, juce::StringArray* leftOut = nullptr);

    // Ours, in PR7-C's order (bank, then number).
    const std::vector<StripPreset>& factoryStripPresets();
    // The bank names, in order ("Drums" ... "Mastering").
    const juce::StringArray& stripPresetGroups();

    juce::File defaultStripPresetFolder();   // Documents/Paragon PR7-S/Strip Presets
    std::vector<StripPreset> userStripPresets (const juce::File& folder);   // A to Z
    std::optional<StripPreset> loadStripPreset (const juce::File&);
    // A file named after it (a legal name; " 2", " 3"... when taken unless `replace`); the file, or none.
    std::optional<juce::File> saveStripPreset (const juce::File& folder, const juce::String& name, const ChannelStrip&, bool replace = false);
    // A preset's strip onto one, keeping that strip's console channel and ANALOG / DIGITAL.
    ChannelStrip appliedStripPreset (const ChannelStrip& now, const ChannelStrip& preset);
}
