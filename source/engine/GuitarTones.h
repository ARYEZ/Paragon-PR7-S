#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "engine/Lines.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

// (0.204.0, the owner asked: "we would never just have a plain clean guitar, there'd always be some modulation,
// delays, reverb, same with leads") THE GUITAR TONES: when ARCHITECT writes a LEAD, CLEAN GUITAR or ACOUSTIC
// GUITAR lane onto a track, Paragon PR7-G (the family's guitar processor, a VST3 effect that ships beside the
// studio) goes on the track's inserts with a tone suggested by the part's style: its compressor, modulation,
// delay and reverb set, the other blocks off. Our own tone designs, by ear on paper: they can't be listened to
// here.
//
// PR7-G's settings (read from its code): its getStateInformation is copyXmlToBinary of its parameter tree, a
// <ParagonX1> element with <PARAM id value/> children (real values: switches 0/1, choices by index, numbers in
// their units) and properties for the amp model, cab IR and rig (ampModelPath, cabIrPath, rigName, rigFile,
// rigFactory, rigModified). Its setStateInformation replaces the whole tree and loads ampModelPath / cabIrPath
// (empty: cleared), so a tone always starts from the instance's own settings and changes only its PARAMs (and
// the rig's name): the owner's amp model and cab IR stay. Hosted by JUCE as a VST3 the state is JUCE's
// <VST3PluginState> with <IComponent> (base64 of the plug-in's bytes, maybe followed by JUCE's private data)
// and maybe <IEditController>; both forms are read and written back as they came.
namespace pr7s
{
    struct GuitarTone
    {
        juce::String name, description;
        LineIdea::Part part = LineIdea::Part::lead;
        std::vector<std::pair<juce::String, float>> settings;   // PR7-G's parameter ids and real values
        std::vector<juce::String> words;                          // style words it's suggested for (lower case)
    };

    // Every tone, the lead's, the clean guitar's and the acoustic's, each part's default first.
    const std::vector<GuitarTone>& guitarTones();
    std::vector<const GuitarTone*> guitarTonesFor (LineIdea::Part);
    const GuitarTone* toneNamed (const juce::String&);   // (any case; none: nullptr)
    bool hasGuitarTones (LineIdea::Part);                // lead, clean and acoustic only
    // The tone whose words the style holds most of (the style lower case; "Solo: Shred Runs" holds "shred" and
    // "run"); none: the part's default (SOARING LEAD, CLEAN SHIMMER, ACOUSTIC ROOM). A part without tones: the lead's.
    const GuitarTone& suggestedTone (LineIdea::Part, const juce::String& style);

    // PR7-G's own settings element (<ParagonX1>) from an instance's state, either form; nullptr when it isn't PR7-G's.
    std::unique_ptr<juce::XmlElement> readPr7gSettings (const juce::MemoryBlock& state);
    // The settings written back in the form `stateBefore` had (the VST3 wrapper: its IEditController and JUCE's
    // private data after the plug-in's own bytes kept as they were).
    juce::MemoryBlock writePr7gSettings (const juce::XmlElement& settings, const juce::MemoryBlock& stateBefore);
    // The tone's PARAMs set (added when missing); the rig named after the tone (no file, not modified).
    void applyTone (juce::XmlElement& settings, const GuitarTone&);
    // One of PR7-G's own rigs (.pr7rig: <ParagonRig name ampModel cabIr keepAmpAndCab> of <Param id value/>):
    // every Param it lists, its amp model and cab IR unless it keeps the current ones (or names none). False:
    // not a rig file.
    bool applyRigFile (juce::XmlElement& settings, const juce::File& rig);
    juce::File pr7gRigFolder();             // PR7-G's own rig folder (its banks are subfolders)
    juce::Array<juce::File> pr7gRigFiles(); // every .pr7rig in it, sorted by name
    bool isPr7g (const juce::PluginDescription&);   // (its name holds "PR7-G")

    // (0.205.0, the owner's "PR7-G amp sim effects", step 2) THE TONES CHANGE WITH THE SECTIONS: PR7-G's modulation
    // mix, delay mix, delay feedback, reverb mix and reverb size written as automation, block by block of the
    // SECTIONS row, scaled from the tone as chosen (the chorus's). Our own table, by ear on paper (not listened to
    // here): a verse drier, a bridge, an intro and an outro wetter, a breakdown dry and tight.
    struct SectionToneScale
    {
        float mod = 1.0f, delay = 1.0f, feedback = 1.0f, reverb = 1.0f, size = 1.0f;
        bool operator== (const SectionToneScale&) const = default;
    };
    // By the block's name (sectionBaseOf; Pre-Chorus, Post-Chorus / Interlude / Instrumental their own, the rest by
    // composerSectionFor): Chorus (Hook, Refrain, Drop) 1; Verse (Riff) mod 0.8 delay 0.6 feedback 0.85 reverb 0.7
    // size 0.9; Pre-Chorus 0.9 0.85 0.95 0.9 1; Bridge (Middle 8, Break) 1.2 1.25 1.1 1.3 1.15; Breakdown (Build-Up)
    // 0.6 0.4 0.7 0.5 0.8; Solo 1 1.15 1.05 0.9 1; Intro 1 1.3 1.1 1.4 1.2; Interlude / Post-Chorus / Instrumental
    // 1.1 1.2 1.05 1.2 1.1; Outro / Ending / Tag / Coda 1 1.3 1.2 1.5 1.25; a name not in the list: 1.
    SectionToneScale sectionToneScale (const juce::String& sectionName);
    // The five knobs: PR7-G's id, its name as the plug-in shows it (the automation lane is found by it), its top.
    struct SectionToneParameter { const char* id; const char* name; float maximum; };
    const std::vector<SectionToneParameter>& sectionToneParameters();   // modMix, delayMix, delayFeedback, reverbMix, reverbSize
    // The five real values (by id) for a block: the baseline's times the scale, kept inside 0..maximum; a knob
    // missing from the baseline is left out.
    std::map<juce::String, float> sectionToneValues (const std::map<juce::String, float>& baseline, const SectionToneScale&);
    // The tone as chosen, from PR7-G's settings: one of ours when its rigName names one (that tone's settings),
    // else the instance's own PARAM values.
    std::map<juce::String, float> sectionToneBaseline (const juce::XmlElement& settings);
}
