#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"
#include "ui/ArrangeView.h"

// The track list, left of the arrangement and lined up with its lanes
// (inspired by Cubase's track list, in our look). Each track: its colour, its name
// (double-click to rename), M (mute), S (solo) and R (record-arm) keys, its
// level, and its instrument (click for the instrument's own window, a
// different instrument, or to delete the track). + ADD TRACK at the bottom
// offers the Paragon Synth and every VST3 instrument found.
class TrackList final : public juce::Component,
                        private juce::ChangeListener,
                        private juce::Timer
{
public:
    TrackList (pr7s::Studio&, Timeline&);
    ~TrackList() override;

    // Called with a track's id to open its instrument's window; and to open the plugin manager.
    std::function<void (int)> onOpenInstrument;
    std::function<void()> onFindPlugins;

    void showAddTrackMenu();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int addButtonHeight = Timeline::bottomRoom;

private:
    class Row;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void rebuild();
    void showInstrumentMenu (int trackId);

    pr7s::Studio& studio;
    Timeline& timeline;
    std::vector<std::unique_ptr<Row>> rows;
    juce::TextButton addTrack { "+ ADD TRACK" };
};
