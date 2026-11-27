#pragma once

#include <juce_core/juce_core.h>

#include <utility>
#include <vector>

// (PR7-S 0.249.0) Copied unchanged from PR7-C 0.5.0 (accce42) but for the namespace pr7c and the include path:
// the strip presets (engine/StripPresets) are converted from these. Copy it again the same way when PR7-C changes.
namespace pr7c
{

// PR7-C's factory presets, filed the way PR7-X files its rigs (the owner's
// ask): seven banks, A drums, B bass, C guitars, D synths and keys, E vocals
// and voice, F the mix, G mastering. In each bank, 01 to 10 are single
// presets; 11 to 19 are the nine genre sets and 31 to 38 the eight
// signature-sound sets, one preset per part of a song (vocal, drums,
// guitars, bass, synths and keys, mix, master) at the same number in each
// bank, so a whole song can be set up from one style. "Default" comes first, outside the
// banks.
//
// They're our own reading of each sound and style, not anyone's settings,
// and never tuned by ear: starting points to adjust.
struct FactoryPreset
{
    juce::String name;
    int bank = -1;          // 0 to 6 (A to G); -1 for Default
    int number = 0;         // 1 to 38
    juce::String set;       // "Genres/<genre>" or "Signature Sounds/<sound>" for a set's parts, else empty
    juce::StringArray tags;
    // Only what differs from the defaults, in each parameter's own units.
    std::vector<std::pair<juce::String, float>> values;

    // "A01"; empty for Default.
    juce::String slot() const;
};

namespace FactoryBanks
{
    enum Bank { drums, bass, guitars, synths, vocals, mix, master };
    inline const juce::StringArray names { "Drums", "Bass", "Guitars", "Synths & Keys", "Vocals & Voice", "Mix", "Mastering" };
    inline juce::String letter (int bank) { return juce::String::charToString ((juce::juce_wchar) ('A' + juce::jlimit (0, 6, bank))); }

    // The sets, in their slots' order (11 to 19, then 31 to 38).
    inline const juce::StringArray genres { "Modern Metal", "Metalcore", "Djent", "Nu Metal", "Argent Metal", "Hard Rock",
                                            "Grunge", "Symphonic Metal", "Industrial" };
    inline const juce::StringArray signatureSounds { "Ethereal Djent", "Dark Synth Metal", "Raw 90s Nu Metal", "Clean Prog Djent",
                                                     "Cinematic Post-Hardcore", "Swedish Melodic Death", "Early 2000s Metalcore",
                                                     "Polished Nu Metal" };
}

// Default first, then by bank and number: the order ◀ ▶ step through.
const std::vector<FactoryPreset>& factoryPresets();
} // namespace pr7c
