#include "engine/strip/FactoryPresets.h"

#include <algorithm>
#include <cmath>

namespace pr7c
{

juce::String FactoryPreset::slot() const
{
    return bank < 0 ? juce::String() : FactoryBanks::letter (bank) + juce::String (number).paddedLeft ('0', 2);
}

namespace
{
    using Values = std::vector<std::pair<juce::String, float>>;

    // Choice indexes, in their saved order.
    constexpr float vca = 0, opto = 1, fet = 2, modern = 3;           // compCharacter
    constexpr float clean = 0, second = 1, secondAndThird = 2;        // compDistortion (MODERN only)
    constexpr float soft = 1, infinite = 30;                          // compRatio's ends
    constexpr float brick = 0, softLimit = 1, autoLimit = 2;          // limitMode
    constexpr float filtersSc = 2;                                    // filterPlacement: SC
    constexpr float fast = 0, medium = 1;                             // gateAttack

    Values operator+ (Values a, const Values& b)
    {
        a.insert (a.end(), b.begin(), b.end());
        return a;
    }

    // The pieces presets are built from.
    Values comp (float character, float threshold, float ratio, float attack, float release, float makeUp, bool autoRelease = false,
                 float distortion = clean)
    {
        Values v { { "compOn", 1 }, { "compCharacter", character }, { "compThreshold", threshold }, { "compRatio", ratio },
                   { "compAttack", attack }, { "compRelease", release }, { "compMakeUp", makeUp } };
        if (autoRelease)
            v.push_back ({ "compAuto", 1 });
        if (! juce::exactlyEqual (distortion, clean))
            v.push_back ({ "compDistortion", distortion });
        return v;
    }
    Values hpf (float hz) { return { { "hpfOn", 1 }, { "hpfFreq", hz } }; }
    Values lpf (float hz) { return { { "lpfOn", 1 }, { "lpfFreq", hz } }; }
    Values filtersOnSidechain (float hpfHz) { return { { "hpfOn", 1 }, { "hpfFreq", hpfHz }, { "filterPlacement", filtersSc } }; }
    // EQ bands. Bells on HF, LMF and LF switch their SHELF key off.
    Values band (const char* id, float hz, float gain) { return { { juce::String (id) + "Freq", hz }, { juce::String (id) + "Gain", gain } }; }
    Values bell (const char* id, float hz, float gain, float q)
    {
        auto v = band (id, hz, gain);
        if (juce::String (id) == "hmf" || juce::String (id) == "mf")
            v.push_back ({ juce::String (id) + "Q", q });
        else
            v.push_back ({ juce::String (id) + "Shelf", 0 });
        return v;
    }
    Values lowShelf (float hz, float gain) { return { { "lfFreq", hz }, { "lfGain", gain }, { "lfShelf", 1 } }; }
    Values highShelf (float hz, float gain) { return band ("hf", hz, gain); }
    Values gate (float threshold, float range, float attack, float hold, float release)
    {
        return { { "gateOn", 1 }, { "gateThreshold", threshold }, { "gateRange", range }, { "gateAttack", attack },
                 { "gateHold", hold }, { "gateRelease", release } };
    }
    Values expander (float threshold, float ratio, float range)
    {
        return { { "gateOn", 1 }, { "gateExpander", 1 }, { "gateThreshold", threshold }, { "gateRatio", ratio }, { "gateRange", range } };
    }
    Values sidechain (float hpfHz, bool edge = false)
    {
        Values v { { "scOn", 1 }, { "scHpf", hpfHz } };
        if (edge)
            v.push_back ({ "scEdge", 1 });
        return v;
    }
    Values limiter (float threshold, float makeUp, float mode, float mix = 100)
    {
        return { { "limitOn", 1 }, { "limitThreshold", threshold }, { "limitMakeUp", makeUp }, { "limitMode", mode }, { "limitMix", mix } };
    }
    Values saturation (float drive, float harmonics, float mix = 100, bool vcaClip = false)
    {
        Values v { { "satOn", 1 }, { "satDrive", drive }, { "satHarmonics", harmonics }, { "satMix", mix } };
        if (vcaClip)
            v.push_back ({ "satVcaClip", 1 });
        return v;
    }
    Values colour (float thd, float vGain = 0) { return { { "toneThd", thd }, { "toneVGain", vGain } }; }
    Values monoMaker (float hz) { return { { "monoOn", 1 }, { "monoFreq", hz } }; }
    Values width (float percent) { return { { "widthOn", 1 }, { "width", percent } }; }

    struct Builder
    {
        std::vector<FactoryPreset> presets;
        void add (int bank, int number, const juce::String& name, const juce::String& tags, Values values, const juce::String& set = {})
        {
            presets.push_back ({ name, bank, number, set, juce::StringArray::fromTokens (tags, ",", {}), std::move (values) });
        }
    };

    using namespace FactoryBanks;

    // A: drums
    void addDrums (Builder& b)
    {
        b.add (drums, 1, "Kick Punch", "Kick", hpf (30) + gate (-30, 30, fast, 50, 150) + comp (fet, -14, 4, 15, 200, 2)
                                                 + bell ("lf", 60, 4, 0) + bell ("lmf", 350, -5, 0) + bell ("hmf", 3500, 3, 1.4f));
        b.add (drums, 2, "Kick Boom Control", "Kick", hpf (30) + comp (opto, -20, 3, 10, 300, 2) + lowShelf (80, -3)
                                                        + bell ("hmf", 4000, 2, 1.2f) + sidechain (20));
        b.add (drums, 3, "Snare Crack", "Snare", hpf (80) + gate (-35, 20, fast, 50, 200) + comp (vca, -16, 4, 5, 200, 2)
                                                   + bell ("lmf", 200, 2, 0) + bell ("mf", 800, -2, 1.5f) + bell ("hmf", 5000, 3, 1)
                                                   + highShelf (10000, 2) + saturation (8, 60));
        b.add (drums, 4, "Snare Fat", "Snare", hpf (70) + comp (modern, -18, 4, 8, 250, 3, false, second) + bell ("lmf", 180, 3, 0)
                                                 + bell ("hmf", 2500, -1.5f, 1) + highShelf (8000, 1.5f));
        b.add (drums, 5, "Toms Tight Gate", "Toms", hpf (60) + gate (-30, 40, medium, 100, 300) + bell ("lmf", 400, -4, 0)
                                                      + bell ("hmf", 3500, 2, 1));
        b.add (drums, 6, "Hi-Hat Tame", "Hi-hat,Cymbals", hpf (250) + comp (vca, -20, 3, 2, 200, 1) + sidechain (200, true)
                                                            + bell ("hmf", 4500, -1.5f, 1.5f) + highShelf (12000, 1));
        b.add (drums, 7, "Overheads Smooth", "Overheads,Cymbals", hpf (120) + comp (opto, -18, 2, 15, 400, 1) + sidechain (60, true)
                                                                    + bell ("lmf", 400, -2, 0) + highShelf (12000, 2) + width (115));
        b.add (drums, 8, "Room Mics Crush", "Room", hpf (150) + comp (fet, -34, infinite, 0.5f, 150, 10) + saturation (8, 50));
        b.add (drums, 9, "Drum Bus Glue", "Bus", filtersOnSidechain (60) + comp (vca, -16, 2, 30, 400, 1, true)
                                                   + saturation (6, 50, 70) + colour (30));
        b.add (drums, 10, "Drum Bus Smash", "Bus,Parallel", filtersOnSidechain (60) + comp (modern, -26, 10, 2, 150, 5, false, secondAndThird)
                                                              + limiter (-6, 3, brick, 45) + bell ("lf", 60, 2, 0) + highShelf (10000, 2));
    }

    // B: bass
    void addBass (Builder& b)
    {
        b.add (bass, 1, "Bass Solid", "Bass", hpf (35) + comp (opto, -20, 4, 10, 300, 3) + lowShelf (80, 2) + bell ("lmf", 250, -3, 0)
                                                + bell ("hmf", 1200, 2, 1) + saturation (10, 20) + monoMaker (150));
        b.add (bass, 2, "Bass DI Even", "Bass,DI", hpf (35) + comp (vca, -18, 3, 15, 250, 3) + bell ("lmf", 250, -2.5f, 0)
                                                     + bell ("hmf", 800, 1.5f, 1));
        b.add (bass, 3, "Bass Growl", "Bass,Drive", hpf (40) + comp (modern, -18, 4, 10, 350, 3, false, secondAndThird)
                                                      + bell ("hmf", 1000, 3, 1.2f) + saturation (14, 40));
        b.add (bass, 4, "Slap Bass", "Bass,Slap", hpf (40) + comp (fet, -16, 4, 2, 200, 2) + bell ("lmf", 300, -3, 0)
                                                    + bell ("hmf", 2500, 2, 1) + highShelf (8000, 2));
        b.add (bass, 5, "808 Control", "Bass,808", hpf (25) + comp (opto, -18, 3, 20, 400, 1) + bell ("hmf", 1200, 2, 1)
                                                     + lpf (8000) + saturation (8, 30) + monoMaker (200));
        b.add (bass, 6, "Synth Bass Tight", "Bass,Synth", hpf (30) + comp (vca, -18, 3, 5, 200, 2) + bell ("lmf", 200, -2, 0)
                                                            + monoMaker (180));
        b.add (bass, 7, "Upright Bass", "Bass,Acoustic", hpf (40) + comp (opto, -20, 3, 15, 400, 2) + bell ("lmf", 160, -2, 0)
                                                           + bell ("hmf", 2000, 1.5f, 1) + colour (20));
        b.add (bass, 8, "Bass Amp Warm", "Bass,Amp", hpf (40) + comp (modern, -18, 3, 10, 300, 2, false, second) + lowShelf (100, 1.5f)
                                                       + lpf (6000) + colour (40));
    }

    // C: guitars
    void addGuitars (Builder& b)
    {
        b.add (guitars, 1, "Acoustic Sparkle", "Acoustic", hpf (100) + comp (vca, -18, 2, 10, 250, 1) + bell ("lmf", 250, -3, 0)
                                                             + bell ("hmf", 4500, 2, 0.8f) + highShelf (12000, 3) + width (120));
        b.add (guitars, 2, "Electric Guitar Tame", "Electric", hpf (90) + lpf (9000) + bell ("hmf", 2500, -3, 2) + saturation (6, 40, 50));
        b.add (guitars, 3, "Rhythm Guitar Tight", "Electric,Rhythm", hpf (110) + lpf (10000) + comp (vca, -18, 2.5f, 10, 200, 1)
                                                                       + bell ("mf", 600, -2, 1) + bell ("hmf", 2500, 1, 1));
        b.add (guitars, 4, "Lead Guitar Sustain", "Electric,Lead", hpf (90) + comp (opto, -22, 4, 10, 500, 4) + bell ("hmf", 1500, 2, 1)
                                                                     + lpf (11000));
        b.add (guitars, 5, "Clean Guitar Shine", "Electric,Clean", hpf (90) + comp (vca, -18, 3, 8, 250, 2) + bell ("lmf", 300, -1.5f, 0)
                                                                     + highShelf (10000, 2.5f));
        b.add (guitars, 6, "High Gain Fizz Control", "Electric,High gain", hpf (100) + lpf (8000) + bell ("hmf", 3500, -2.5f, 2)
                                                                             + comp (vca, -18, 2, 10, 200, 1) + sidechain (60, true));
        b.add (guitars, 7, "Acoustic Strum Even", "Acoustic", hpf (90) + comp (opto, -20, 3, 15, 350, 3) + bell ("lmf", 200, -2, 0)
                                                                + highShelf (10000, 2));
        b.add (guitars, 8, "Guitar Bus", "Bus", hpf (80) + comp (vca, -16, 2, 20, 300, 1, true) + bell ("mf", 500, -1.5f, 0.8f)
                                                  + colour (25));
    }

    // D: synths and keys
    void addSynths (Builder& b)
    {
        b.add (synths, 1, "Piano Bright", "Piano", hpf (40) + comp (vca, -18, soft, 20, 400, 1, true) + bell ("lmf", 300, -2, 0)
                                                     + highShelf (8000, 2) + width (115));
        b.add (synths, 2, "Piano Warm", "Piano", hpf (40) + comp (opto, -20, 3, 20, 500, 1) + highShelf (9000, -1.5f) + lowShelf (120, 1)
                                                   + colour (30));
        b.add (synths, 3, "Electric Piano", "Keys", hpf (60) + comp (modern, -18, 3, 10, 300, 2, false, second) + bell ("hmf", 1500, 1.5f, 1));
        b.add (synths, 4, "Synth Pad Wide", "Synth,Pad", hpf (120) + comp (opto, -22, 2, 30, 600, 1) + width (150) + monoMaker (150));
        b.add (synths, 5, "Synth Lead Bite", "Synth,Lead", hpf (150) + comp (fet, -16, 4, 3, 200, 2) + bell ("hmf", 3000, 2.5f, 1)
                                                             + saturation (8, 70));
        b.add (synths, 6, "Organ", "Keys,Organ", hpf (50) + comp (opto, -20, 3, 15, 400, 1) + bell ("mf", 800, -1.5f, 1) + saturation (10, 20));
        b.add (synths, 7, "Pluck Tight", "Synth,Pluck", hpf (100) + gate (-40, 20, fast, 30, 120) + comp (vca, -16, 3, 5, 150, 1));
        b.add (synths, 8, "Strings Smooth", "Strings", hpf (80) + comp (opto, -22, 2, 30, 600, 1) + bell ("hmf", 3000, -1.5f, 1)
                                                         + highShelf (12000, 1.5f) + width (120));
    }

    // E: vocals and voice
    void addVocals (Builder& b)
    {
        b.add (vocals, 1, "Clear Lead Vocal", "Vocal,Lead", hpf (90) + comp (vca, -18, 3, 5, 250, 3, true) + bell ("lmf", 300, -2, 0)
                                                              + bell ("hmf", 3000, 2, 1) + highShelf (10000, 3) + saturation (4, 20) + colour (20));
        b.add (vocals, 2, "Warm Opto Vocal", "Vocal,Lead", hpf (80) + comp (opto, -22, 4, 10, 400, 4) + bell ("mf", 500, -1.5f, 1)
                                                             + highShelf (12000, 2) + saturation (8, 0, 60) + colour (30));
        b.add (vocals, 3, "Soft Backing Vocal", "Vocal,Backing", hpf (120) + lpf (14000) + comp (vca, -20, soft, 10, 300, 2, true)
                                                                   + bell ("hmf", 2500, -2, 0.8f) + width (120));
        b.add (vocals, 4, "Rap Vocal Tight", "Vocal,Rap", hpf (100) + comp (fet, -16, 4, 3, 200, 3) + bell ("lmf", 220, -2.5f, 0)
                                                            + bell ("hmf", 4000, 3, 1) + highShelf (12000, 2));
        b.add (vocals, 5, "Scream Control", "Vocal,Scream", hpf (120) + comp (fet, -18, 6, 1, 300, 3) + sidechain (100, true)
                                                              + bell ("hmf", 3000, -2, 1.5f) + saturation (6, 70));
        b.add (vocals, 6, "Vocal De-Harsh", "Vocal,Harsh", hpf (90) + comp (vca, -20, 3, 2, 200, 1) + sidechain (100, true)
                                                             + bell ("hmf", 3500, -1.5f, 2));
        b.add (vocals, 7, "Podcast Voice", "Voice,Podcast", hpf (80) + expander (-50, 2, 12) + comp (opto, -20, 4, 10, 400, 4)
                                                              + bell ("lmf", 300, -2, 0) + bell ("hmf", 2500, 2, 1));
        b.add (vocals, 8, "Vintage Vocal Glow", "Vocal,Vintage", hpf (80) + comp (modern, -20, 4, 10, 300, 3, false, second)
                                                                   + highShelf (9000, 1.5f) + colour (40));
        b.add (vocals, 9, "Telephone Vocal", "Vocal,Lo-fi", hpf (400) + lpf (3000) + bell ("mf", 1200, 6, 1.5f)
                                                              + saturation (18, 70, 100, true) + width (0));
    }

    // F: the mix
    void addMix (Builder& b)
    {
        b.add (mix, 1, "Mix Bus Console Glue", "Mix,Glue", filtersOnSidechain (25) + comp (vca, -14, 1.5f, 30, 400, 1, true)
                                                             + saturation (3, 30, 50) + colour (40) + monoMaker (100) + limiter (-1, 0, softLimit));
        b.add (mix, 2, "Mix Bus Modern Glue", "Mix,Glue", filtersOnSidechain (60) + comp (modern, -16, 2, 10, 300, 1.5f, false, second));
        b.add (mix, 3, "Analog Character", "Mix,Colour", colour (50, 20) + saturation (6, 0, 40));
        b.add (mix, 4, "Mix Bus Wide", "Mix,Stereo", monoMaker (120) + width (125) + highShelf (12000, 1));
        b.add (mix, 5, "Mix Bus Punch", "Mix,Punch", filtersOnSidechain (80) + comp (fet, -16, 3, 30, 300, 1.5f, true));
        b.add (mix, 6, "Low End Mono", "Mix,Stereo,Low end", monoMaker (150));
        b.add (mix, 7, "Tape Warmth", "Mix,Colour", comp (modern, -12, 2, 5, 200, 0.5f, false, secondAndThird) + highShelf (14000, -1)
                                                      + colour (30));
        b.add (mix, 8, "Lo-Fi Telephone", "Mix,Lo-fi", hpf (400) + lpf (3000) + bell ("mf", 1200, 6, 1.5f)
                                                         + saturation (18, 70, 100, true) + width (0));
    }

    // G: mastering
    void addMastering (Builder& b)
    {
        b.add (master, 1, "Master Loud", "Mastering,Loud", comp (vca, -16, soft, 30, 400, 1.5f, true) + limiter (-5, 4, autoLimit)
                                                             + monoMaker (120) + width (110) + colour (20));
        b.add (master, 2, "Master Gentle Polish", "Mastering", comp (vca, -12, 1.5f, 30, 400, 0.5f, true) + highShelf (14000, 1)
                                                                 + limiter (-2, 1, softLimit));
        b.add (master, 3, "Master Transparent Limit", "Mastering,Limiter", limiter (-4, 3, autoLimit));
        b.add (master, 4, "Master Warm", "Mastering,Colour", comp (opto, -14, 2, 30, 600, 0.5f) + highShelf (12000, -1) + lowShelf (80, 1)
                                                               + colour (35) + limiter (-3, 2, softLimit));
        b.add (master, 5, "Master Wide", "Mastering,Stereo", monoMaker (120) + width (120) + limiter (-3, 2, autoLimit));
        b.add (master, 6, "Master Low End Control", "Mastering,Low end", filtersOnSidechain (30) + monoMaker (100) + lowShelf (60, -1)
                                                                           + comp (opto, -14, 2, 30, 400, 0.5f) + limiter (-3, 2, autoLimit));
        b.add (master, 7, "Master Streaming Safe", "Mastering,Streaming", comp (vca, -14, soft, 30, 400, 0.5f, true) + limiter (-3.5f, 2, brick));
        b.add (master, 8, "Master Modern Punch", "Mastering,Punch", comp (modern, -14, 2, 20, 300, 1, false, second)
                                                                      + limiter (-4, 3, autoLimit) + width (110));
    }

    // A style, in a few traits (0 to 1 unless noted), from which each part
    // of its set is built: our reading of how each style is usually mixed.
    struct Style
    {
        float aggression, brightness, weight, scoop, grit;
        float harmonics;        // 0 (warm 2nd) to 100 (gritty 3rd)
        float loudness, widthPercent;
        float vocalCharacter, drumCharacter;
    };

    float lerp (float from, float to, float t) { return from + (to - from) * t; }
    float round1 (float v) { return std::round (v * 10.0f) / 10.0f; }

    Values styleVocal (const Style& s)
    {
        return hpf (round1 (lerp (80, 120, s.aggression)))
             + comp (s.vocalCharacter, round1 (lerp (-12, -20, s.aggression)), round1 (lerp (3, 4.5f, s.aggression)), round1 (lerp (10, 3, s.aggression)),
                     350, round1 (lerp (2, 4, s.aggression)), false, juce::exactlyEqual (s.vocalCharacter, modern) ? (s.harmonics > 50 ? secondAndThird : second) : clean)
             + bell ("lmf", 300, -2, 0) + bell ("hmf", 3000, round1 (lerp (0.5f, 3, s.brightness)), 1)
             + highShelf (10000, round1 (lerp (1, 3.5f, s.brightness)))
             + saturation (round1 (lerp (2, 10, s.grit)), s.harmonics, 60)
             + (s.aggression > 0.6f ? sidechain (100, true) : Values {});
    }

    Values styleDrums (const Style& s)
    {
        return filtersOnSidechain (60)
             + comp (s.drumCharacter, round1 (lerp (-12, -18, s.aggression)), round1 (lerp (2, 3, s.aggression)), round1 (lerp (30, 15, s.aggression)),
                     300, round1 (lerp (1, 2, s.aggression)), true, juce::exactlyEqual (s.drumCharacter, modern) ? second : clean)
             + bell ("lf", 60, round1 (lerp (1, 4, s.weight)), 0) + bell ("lmf", 400, round1 (lerp (-1, -4, s.scoop)), 0)
             + bell ("hmf", 4500, round1 (lerp (0.5f, 3, s.brightness)), 1) + highShelf (12000, round1 (lerp (1, 3, s.brightness)))
             + saturation (round1 (lerp (3, 12, s.grit)), s.harmonics, 60);
    }

    Values styleGuitars (const Style& s)
    {
        return hpf (round1 (lerp (80, 120, s.aggression))) + lpf (round1 (lerp (9000, 13000, s.brightness)))
             + comp (vca, -18, round1 (lerp (1.5f, 2.5f, s.aggression)), 15, 250, 1)
             + bell ("mf", 600, round1 (lerp (0, -3, s.scoop)), 1) + bell ("hmf", 2500, round1 (lerp (-1, 1.5f, s.brightness)), 1);
    }

    Values styleBass (const Style& s)
    {
        const auto character = s.aggression > 0.7f ? modern : opto;
        return hpf (35)
             + comp (character, round1 (lerp (-16, -22, s.aggression)), 3, 10, 300, round1 (lerp (1.5f, 3.5f, s.aggression)), false,
                     juce::exactlyEqual (character, modern) ? (s.grit > 0.6f ? secondAndThird : second) : clean)
             + bell ("lf", 80, round1 (lerp (0.5f, 3, s.weight)), 0) + bell ("lmf", 250, -2.5f, 0)
             + bell ("hmf", 1200, round1 (lerp (1, 4, s.aggression)), 1)
             + saturation (round1 (lerp (4, 16, s.grit)), s.harmonics, 70) + monoMaker (150);
    }

    // Synths and keys: in these styles mostly a layer behind the guitars and
    // vocal (pads, leads, piano, strings), so they're kept out of the low end
    // and the low mids, held even by a gentle opto, and made the widest part.
    Values styleSynths (const Style& s)
    {
        return hpf (round1 (lerp (90, 180, s.aggression)))
             + comp (opto, round1 (lerp (-16, -22, s.aggression)), round1 (lerp (2, 3, s.aggression)), 20, 400, round1 (lerp (1, 2, s.aggression)))
             + bell ("lmf", 350, round1 (lerp (-1, -3, s.weight)), 0) + bell ("hmf", 2500, round1 (lerp (-1, 2, s.brightness)), 1)
             + highShelf (10000, round1 (lerp (0, 2.5f, s.brightness)))
             + saturation (round1 (lerp (2, 10, s.grit)), s.harmonics, 50)
             + width (round1 (lerp (110, 160, juce::jlimit (0.0f, 1.0f, (s.widthPercent - 100) / 30)))) + monoMaker (150);
    }

    Values styleMix (const Style& s)
    {
        return filtersOnSidechain (60)
             + comp (modern, round1 (lerp (-12, -16, s.aggression)), round1 (lerp (1.5f, 2, s.aggression)), 20, 300,
                     round1 (lerp (0.5f, 1.5f, s.aggression)), false, s.grit > 0.6f ? second : clean)
             + saturation (round1 (lerp (2, 6, s.grit)), s.harmonics, 40) + colour (round1 (lerp (15, 45, s.grit)))
             + monoMaker (100) + width (round1 (lerp (100, s.widthPercent, 0.5f)));
    }

    Values styleMaster (const Style& s)
    {
        return comp (vca, -14, soft, 30, 400, 0.5f, true)
             + highShelf (14000, round1 (lerp (0, 1.5f, s.brightness))) + lowShelf (60, round1 (lerp (0, 1.5f, s.weight)))
             + limiter (round1 (-1 - lerp (1.5f, 4, s.loudness)), round1 (lerp (1.5f, 4, s.loudness)), autoLimit) + monoMaker (120) + width (s.widthPercent);
    }

    // One preset per part of a song, at the same number in each bank.
    void addSet (Builder& b, const juce::String& group, const juce::String& name, int number, const Style& s)
    {
        const auto set = group + "/" + name;
        const auto tags = group == "Genres" ? juce::String ("Genre,") + name : juce::String ("Signature,") + name;
        b.add (vocals, number, name + " Vocal", tags + ",Vocal", styleVocal (s), set);
        b.add (drums, number, name + " Drums", tags + ",Drums", styleDrums (s), set);
        b.add (guitars, number, name + " Guitars", tags + ",Guitar", styleGuitars (s), set);
        b.add (bass, number, name + " Bass", tags + ",Bass", styleBass (s), set);
        b.add (synths, number, name + " Synths", tags + ",Synth,Keys", styleSynths (s), set);
        b.add (mix, number, name + " Mix", tags + ",Mix", styleMix (s), set);
        b.add (master, number, name + " Master", tags + ",Mastering", styleMaster (s), set);
    }

    void addSets (Builder& b)
    {
        // aggression, brightness, weight, scoop, grit, harmonics, loudness, width, vocal, drums
        const Style genreStyles[] = {
            { 0.9f, 0.6f, 0.8f, 0.5f, 0.7f, 70, 0.9f, 110, fet, fet },          // Modern Metal: tight, bright, loud
            { 0.85f, 0.6f, 0.7f, 0.5f, 0.6f, 60, 0.9f, 110, modern, fet },      // Metalcore
            { 0.8f, 0.7f, 0.9f, 0.6f, 0.5f, 80, 0.85f, 105, vca, modern },      // Djent: precise, a heavy low end
            { 0.75f, 0.4f, 0.9f, 0.7f, 0.6f, 50, 0.8f, 110, fet, vca },         // Nu Metal: scooped, a big bottom
            { 0.8f, 0.5f, 0.7f, 0.4f, 0.7f, 40, 0.85f, 115, opto, fet },        // Argent Metal
            { 0.6f, 0.4f, 0.6f, 0.3f, 0.5f, 30, 0.7f, 105, modern, vca },       // Hard Rock: warmer, more open
            { 0.5f, 0.1f, 0.6f, 0.2f, 0.7f, 20, 0.6f, 100, fet, opto },         // Grunge: dark, raw, less loud
            { 0.6f, 0.5f, 0.6f, 0.3f, 0.3f, 50, 0.7f, 125, opto, vca },         // Symphonic Metal: wide, cleaner
            { 0.9f, 0.5f, 0.8f, 0.4f, 0.9f, 90, 0.95f, 110, modern, fet },      // Industrial: gritty, loud
        };
        const Style signatureStyles[] = {
            { 0.6f, 0.8f, 0.8f, 0.5f, 0.3f, 70, 0.75f, 130, opto, modern },     // Ethereal Djent
            { 0.8f, 0.3f, 0.9f, 0.4f, 0.8f, 85, 0.9f, 120, modern, fet },       // Dark Synth Metal
            { 0.7f, 0.2f, 0.8f, 0.7f, 0.7f, 40, 0.65f, 100, fet, vca },         // Raw 90s Nu Metal
            { 0.5f, 0.6f, 0.7f, 0.4f, 0.2f, 60, 0.7f, 115, vca, modern },       // Clean Prog Djent
            { 0.7f, 0.5f, 0.7f, 0.3f, 0.5f, 50, 0.8f, 125, modern, vca },       // Cinematic Post-Hardcore
            { 0.85f, 0.6f, 0.6f, 0.5f, 0.6f, 70, 0.85f, 110, fet, fet },        // Swedish Melodic Death
            { 0.8f, 0.5f, 0.7f, 0.6f, 0.5f, 60, 0.8f, 105, modern, fet },       // Early 2000s Metalcore
            { 0.7f, 0.5f, 0.8f, 0.5f, 0.4f, 50, 0.85f, 115, vca, modern },      // Polished Nu Metal
        };
        static_assert (std::size (genreStyles) == 9 && std::size (signatureStyles) == 8);
        for (int i = 0; i < genres.size(); ++i)
            addSet (b, "Genres", genres[i], 11 + i, genreStyles[(size_t) i]);
        for (int i = 0; i < signatureSounds.size(); ++i)
            addSet (b, "Signature Sounds", signatureSounds[i], 31 + i, signatureStyles[(size_t) i]);
    }
}

const std::vector<FactoryPreset>& factoryPresets()
{
    static const auto presets = []
    {
        Builder b;
        b.presets.push_back ({ "Default", -1, 0, {}, {}, {} });
        addDrums (b);
        addBass (b);
        addGuitars (b);
        addSynths (b);
        addVocals (b);
        addMix (b);
        addMastering (b);
        addSets (b);
        std::stable_sort (b.presets.begin() + 1, b.presets.end(), [] (const FactoryPreset& x, const FactoryPreset& y)
        {
            return x.bank != y.bank ? x.bank < y.bank : x.number < y.number;
        });
        return b.presets;
    }();
    return presets;
}
} // namespace pr7c
