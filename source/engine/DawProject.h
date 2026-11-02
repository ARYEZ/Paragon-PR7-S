#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <optional>

#include "Project.h"

// DAWproject import (0.9.13, the owner asked: songs from Cubase, which exports
// DAWproject since 14: File > Export > DAWproject). The open exchange format
// by Bitwig and PreSonus (github.com/bitwig/dawproject, version 1.0): a zip
// holding project.xml (the song), the plug-ins' states (.vstpreset for
// VST3) and the audio.
//
// What comes across: the tempo and time signature; instrument, audio, FX and
// group tracks (folders flattened; a folder with a group channel becomes the
// group); names and colours (the nearest of the family's three); volume,
// pan, mute, solo; outputs to groups; sends to FX channels (pre or post,
// on/off, level); VST3 instruments and insert effects with their settings
// (bypassed if disabled); MIDI parts and their notes (clips inside clips
// and loops flattened); audio clips (offset, fades); the master's volume
// and effects; automation of volume, pan, mute, sends and plug-in knobs.
// What doesn't (listed in `notes`): tempo and signature changes (the first
// is used), markers, built-in devices (Cubase's channel strip EQ and
// dynamics), VST2 / CLAP / AU plug-ins, time-stretched audio (played at its
// own speed), note expressions, the clip launcher.
namespace pr7s::DawProject
{
    // Automation of a plug-in's knob, matched to the plug-in's parameter once it's loaded.
    struct PluginLane
    {
        int busId = 0;               // track id, or 0: the master
        AutomationLane lane;         // target instrument or insert (and slot); `parameter` still to find
        juce::String parameterId;    // the VST3 parameter's id
    };

    struct Imported
    {
        Project project;
        std::vector<PluginLane> pluginLanes;
        juce::StringArray notes;      // what didn't come across, or came across changed
        juce::String application;     // who wrote it ("Cubase 15.0")
    };

    // Reads a .dawproject: the song, its audio copied into `audioFolder`.
    // `knownPlugins`: the scanned plug-ins (matched by name and maker).
    // None, with `error` set, if it isn't one.
    std::optional<Imported> read (const juce::File&, const juce::File& audioFolder,
                                  const juce::Array<juce::PluginDescription>& knownPlugins, juce::String& error);

    // A .vstpreset's states (its component's and controller's) as JUCE's
    // VST3 host keeps them (what setStateInformation takes); empty if the
    // data isn't a .vstpreset.
    juce::MemoryBlock vstPresetToHostState (const juce::MemoryBlock&);
}
