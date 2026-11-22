#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "engine/Lines.h"

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
}
