#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// Freeze and track presets on screen (0.9.8), shared by the track list's
// right-click menu, the Inspector's FREEZE key and + ADD TRACK.
//
// Freeze (the manual's Freeze Channel Options): the tail size is chosen from
// the menu (none, 2, 5, 10 or 30 seconds); unfreeze keeps or deletes the
// freeze file (its Unfreeze Channel Options). The selected tracks, as the
// manual's "Freeze/Unfreeze Selected Tracks".
namespace TrackActions
{
    // Menu ids used: 70-79.
    void addFreezeItems (juce::PopupMenu&, pr7s::Studio&, int trackId);
    void addPresetItems (juce::PopupMenu&, pr7s::Studio&, int trackId);
    // True if `chosen` was one of the items above (and it was done).
    // `chooser` keeps a file chooser alive while it's open.
    bool handle (int chosen, pr7s::Studio&, int trackId, std::unique_ptr<juce::FileChooser>& chooser);

    static constexpr double tails[] { 0.0, 2.0, 5.0, 10.0, 30.0 };
    // Freezes or unfreezes the selected tracks (with this one among them):
    // false and the reasons in `error` if any couldn't be frozen.
    bool freezeSelected (pr7s::Studio&, double tailSeconds, juce::String& error);
    void unfreezeSelected (pr7s::Studio&, bool deleteFiles);

    void savePreset (pr7s::Studio&, int trackId, std::unique_ptr<juce::FileChooser>&);
    void loadPreset (pr7s::Studio&, int trackId, std::unique_ptr<juce::FileChooser>&);
    void addTrackFromPreset (pr7s::Studio&, std::unique_ptr<juce::FileChooser>&);
    // The work of the three above, without a chooser (for tests).
    bool savePresetTo (pr7s::Studio&, int trackId, const juce::File&);
    bool loadPresetFrom (pr7s::Studio&, int trackId, const juce::File&, juce::String& error);
    int addTrackFrom (pr7s::Studio&, const juce::File&, juce::String& error);
}
