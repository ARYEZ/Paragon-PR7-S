#pragma once

#include "Project.h"

// The Edit History (0.83.0, the owner asked: the manual's Edit History Dialog: "lists all your
// edits ... Action, Time, State, Details ... Move the separator upwards to undo your actions. To
// redo an action, move the separator down").
//
// The studio's undo keeps a copy of the song after each step (Studio::recordUndoState), so a step
// in the list is a copy, and going back to any of them is putting that copy back. Each step's
// name is worked out by comparing it with the copy before (`describeEdit`): our own wording, a
// best guess at what was done ("Move Part", "Volume", "Edit Notes"...), with the track and the
// part in the details. Several kinds of change at once read as the first one found and "(and more)".
namespace pr7s
{
    struct HistoryStep
    {
        juce::String action;    // "Move Part", "Volume" ...
        juce::String details;   // "Bass: Verse riff" (and whatever the owner types)
        juce::Time time;
    };

    HistoryStep describeEdit (const Project& before, const Project& after);
}
