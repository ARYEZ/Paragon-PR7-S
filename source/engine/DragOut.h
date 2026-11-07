#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "Project.h"

// Parts dragged out of the arrangement as files (0.53.1, the owner asked from the PR7-D chat: a
// part dropped on a plug-in's window, such as PR7-D's GROOVES page, or on the desktop, as a file).
// A plug-in has its own copy of JUCE, so the studio's own drags never reach it: only the
// operating system's file drag does. The files are written here; Studio::writePartForDrag
// makes them, ArrangeView starts the drag.
namespace pr7s
{
    // A MIDI part as a standard MIDI file of its own (type 1): the part's start is the file's
    // beat 0; the tempo there (and the tempo track's changes inside the part), the signature
    // there (and changes inside it); the notes as heard (its Transpose and Velocity), its
    // controllers, on the track's MIDI channel; `resolution` ticks a quarter note.
    juce::MidiFile makePartMidiFile (const Project&, const Track&, const Part&, int resolution = 480);

    // The files' folder: "Paragon PR7-S Drag Out" in the temporary folder, emptied when the
    // studio starts (never right after a drop: the target reads the file a moment later, often
    // on a thread of its own). Each drag gets a folder of its own inside it, so a file's name
    // stays clean and a file still being read is never overwritten.
    juce::File dragOutFolder();
    void clearDragOutFolder();   // (this one and the composer's)
    juce::File newDragOutFolder (const juce::File& root = dragOutFolder());
    // (0.59.0) The composer's lanes dragged out: a folder of their own, so a lane dropped back on the
    // studio's arrangement lands on the track under it (a part dragged back is refused: nothing to do).
    juce::File composerDragFolder();

    // The tempo in a file's name: "140", "122.5".
    juce::String dragOutTempoText (double bpm);

    // What PR7-D reads at most (and so the cap on an audio event's file): 300 seconds.
    constexpr double dragOutMostSeconds = 300.0;
}
