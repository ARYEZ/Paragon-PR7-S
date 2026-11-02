#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"
#include "ui/ArrangeView.h"
#include "ui/TrackColours.h"

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
// and lanes. Add Track (right-click a track, at the foot of its menu, or
// right-click under the tracks; 0.9.11, no + ADD TRACK key) offers PR7-K and every
// VST3 instrument found, an FX channel (empty, or with one of your
// effects) and a group channel. An FX or group channel's row has no REC or
// instrument: its button says what it is (click for its effect's window, or
// to delete it). A track's button also chooses its Output (the master or a
// group channel).
class TrackList final : public juce::Component,
                        private juce::ChangeListener,
                        private juce::Timer
{
public:
    TrackList (pr7s::Studio&, Timeline&);
    ~TrackList() override;

    // Called with a track's id to open its instrument's window; and to open the plugin manager.
    std::function<void (int)> onOpenInstrument;
    std::function<void (int busId, int slot)> onOpenEffect;   // an effect's window (an FX channel's)
    std::function<void()> onFindPlugins;
    std::function<void()> onTempoMenu;   // the tempo row's right-click (the arrangement's tempo menu)
    // Render in Place (the track's menu): the selected tracks whole; ask how (else as last time).
    std::function<void (std::vector<pr7s::Studio::RenderSource>, bool ask)> onRenderInPlace;
    juce::TextButton& getAddTrackButton() noexcept { return addTrack; }
    juce::TextButton& getMuteAllKey() noexcept { return muteAll; }
    juce::TextButton& getSoloAllKey() noexcept { return soloAll; }
    juce::Button& getColourKey() noexcept { return colourKey; }   // (in the header, at the top)

    void showAddTrackMenu();
    // The Add Track menu (its item ids from `offset`), and doing what was chosen in it (the id less the offset).
    juce::PopupMenu makeAddTrackMenu (int offset = 0);
    bool addTrackChosen (int chosen);
    static constexpr int addTrackOffset = 1000000;
    static constexpr int instrumentItems = 100000;   // (the track menu's instruments: clear of every other item, however many)
    // The track menu's Output items (30000 + a group's id), not the instruments above them
    // (0.17.3: an instrument chosen was taken for an output, and nothing changed).
    static constexpr bool isOutputItem (int id) noexcept { return id > 30000 && id < instrumentItems; }
    static juce::PopupMenu alphabetical (const juce::PopupMenu&);   // the track menu's order (0.9.12)
    // A track's menu (its instrument key's): its window, change it, Output, delete... (also the Inspector's)
    void showInstrumentMenu (int trackId, bool withAutomation = false);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int addButtonHeight = Timeline::bottomRoom;

    void showLaneMenu (int busId, int laneIndex, juce::Component& target);

    // Moving tracks (as the manual has it): drag a track's row (by its name
    // or an empty spot; the selected ones together) up or down, a line
    // showing where they'll go; or its menu's Move. Ctrl-click / Shift-click
    // select several; Up / Down step through them (MainView).
    enum class MoveTo { first, previous, next, last };
    void moveSelectedTracks (MoveTo);
    void dragTrack (int trackId, juce::Point<int> at);   // (a row, being dragged: where the mouse is, in the list)
    void dropTracks (int trackId);
    int getDropIndex() const noexcept { return dropIndex; }
    // Shift+Delete or the menu: the selected tracks (asked first if any has something on it).
    void removeSelectedTracks();
    // A double-click under the tracks: a new track like the selected one.
    void addTrackLikeSelected();
    void mouseDown (const juce::MouseEvent&) override;   // right-click under the tracks: Add Track
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void paintOverChildren (juce::Graphics&) override;

private:
    class Row;
    class LaneRow;
    class MasterRow;
    class TempoRow;
    class ChordRow;
    class Arrow;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void rebuild();
    void placeRows();

    pr7s::Studio& studio;
    Timeline& timeline;
    std::vector<std::unique_ptr<Row>> rows;
    std::vector<std::unique_ptr<LaneRow>> laneRows;
    std::unique_ptr<MasterRow> masterRow;
    std::unique_ptr<TempoRow> tempoRow;   // the tempo track's (0.10.0), while it's added
    std::unique_ptr<ChordRow> chordRow;   // the chord track's (0.16.0), while it's shown
    std::vector<Timeline::Row> lastLayout;
    int dropIndex = -1, dropY = -1;   // a drag of tracks: where they'd go
    std::vector<int> tracksToMove (int trackId) const;
    juce::TextButton addTrack { "+ ADD TRACK" };
    juce::TextButton muteAll { "M" }, soloAll { "S" };   // Deactivate All Mute / Solo States (0.9.9)
    ColourKey colourKey;   // the colour menu (0.17.3)
    std::unique_ptr<juce::FileChooser> chooser;   // a preset's (0.9.8)
};
