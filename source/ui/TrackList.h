#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"
#include "ui/ArrangeView.h"

// The track list, left of the arrangement and lined up with its lanes
// (inspired by Cubase's track list, in our look). Each track: its colour, its name
// (double-click to rename), M (mute), S (solo) and REC (record-arm) keys, its
// instrument (click for the instrument's own window, a different
// instrument, or to delete the track), R and W (read and write automation),
// its level and volume; the arrow at its lower left shows and hides its
// automation lanes (right-click: Show/Hide Automation, Show All Used
// Automation). Each automation lane: its control (click to choose another,
// or Remove Parameter), M (Mute Automation), its value now, and + (Append
// Automation Track) at its lower left. MASTER at the end: the master's R, W
// and lanes. + ADD TRACK at the bottom offers the Paragon Synth and every
// VST3 instrument found.
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

    void showLaneMenu (int busId, int laneIndex, juce::Component& target);

private:
    class Row;
    class LaneRow;
    class MasterRow;
    class Arrow;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void rebuild();
    void showInstrumentMenu (int trackId, bool withAutomation = false);
    void placeRows();

    pr7s::Studio& studio;
    Timeline& timeline;
    std::vector<std::unique_ptr<Row>> rows;
    std::vector<std::unique_ptr<LaneRow>> laneRows;
    std::unique_ptr<MasterRow> masterRow;
    std::vector<Timeline::Row> lastLayout;
    juce::TextButton addTrack { "+ ADD TRACK" };
};
