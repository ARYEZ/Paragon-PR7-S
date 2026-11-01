#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/ValueSlider.h"

#include "engine/Studio.h"
#include "ui/LevelMeter.h"

// The mixer, inspired by Cubase's MixConsole, in our look: a channel strip
// for each track, then the master's, on the right.
//
//   a strip        the track's colour and name; its inserts (16 slots, at
//                  least 8 shown, more as they fill: effects, top to bottom,
//                  the order the sound goes through them; those below the
//                  line are after the fader); pan; M (mute) and S (solo);
//                  the volume fader (Ctrl-click or double-click: 0 dB;
//                  Shift-drag: fine) and the level; R and W (read and write
//                  automation: grabbing the fader, pan or M while W is on and
//                  playing writes them); click it to select the track
//   an insert      empty: click to add an effect (one of your VST3 effects);
//                  an effect: click to open its window, bypass it, replace
//                  it or remove it, or make it the last before the fader
//   a send         (SENDS) some of the channel's sound to an FX channel:
//                  its name and level (a bar); click the dot to turn it on
//                  or off, drag across it for the level (Shift: fine),
//                  Ctrl-click for 0 dB; right-click (or click an empty one)
//                  for its FX channel, Move to Pre-/Post-Fader, Remove, Add
//                  FX Channel to Send. PRE marks one before the fader
//   INSERTS, SENDS which rack the strips show; click the lit one to hide it
//                  (only the faders: the docked mixer then takes less room)
//   OUT            where the track's sound goes: the master, or a group channel
//   MASTER         every track goes through it: its inserts, then its fader
//
// It sits under the arrangement (MIXER, Alt+F3) or in a window of its own
// (F3; or WINDOW / DOCK), as the key editor does.
class MixerView final : public juce::Component,
                        private juce::ChangeListener,
                        private juce::Timer
{
public:
    // `singleStrip`: one track's strip only (setSingleTrack), no bar, no master,
    // no rack: the Inspector's Fader section (0.9.0), with its own meter reader.
    explicit MixerView (pr7s::Studio&, bool singleStrip = false);
    void setSingleTrack (int trackId, bool isMaster = false);   // single-strip mode: whose strip (or the master's)
    int getSingleTrack() const noexcept { return singleTrack; }
    ~MixerView() override;

    std::function<void (int busId, int slot)> onOpenEffect;   // its window
    std::function<void()> onFindPlugins, onClose, onWindowToggle;
    void setInWindow (bool);   // the WINDOW / DOCK button's name follows
    bool isInWindow() const noexcept { return inWindow; }

    // An insert's menu (what a click on it shows); a send's; a track's output.
    void showInsertMenu (int busId, int slot);
    void showSendMenu (int busId, int slot);
    void showOutputMenu (int busId);

    // The strips' racks: their inserts, or their sends (INSERTS / SENDS in the
    // mixer's bar: our design, so the strips keep their faders' room), or
    // neither (click the lit one again): then only the faders, pan and keys
    // show, and the mixer can be docked much shorter. Remembered for next time.
    void setShowSends (bool);
    bool isShowingSends() const noexcept { return showSends; }
    void setRackShown (bool);
    bool isRackShown() const noexcept { return rackShown; }
    // What the strips show now (0.9.14, the owner asked): in the mixer's own
    // window both racks, the inserts above the sends, always (there's room);
    // docked, the choice above (INSERTS, SENDS or neither; neither at first).
    bool showsInserts() const noexcept { return inWindow || (rackShown && ! showSends); }
    bool showsSends() const noexcept { return inWindow || (rackShown && showSends); }
    // The least height it can be docked at (more with the rack shown: room for its slots).
    int getMinimumHeight() const;
    // What it would like at first (before its divider is dragged): its rack at full size and a full fader.
    int getPreferredHeight() const;
    std::function<void()> onMinimumHeightChange;   // the main window lays the lower zone out again
    class SendSlot;

    // For tests: the strips, in order (the master's last).
    class Strip;
    int getNumStrips() const noexcept { return (int) strips.size() + 1; }
    Strip& getStrip (int index);   // index getNumStrips() - 1: the master's

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int barHeight = 32, stripWidth = 100, masterWidth = 124;
    static constexpr int windowStripWidth = 140, windowMasterWidth = 160;   // in its own window: wider, for the bigger racks (0.11.1)

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void rebuild();

    pr7s::Studio& studio;
    const bool single;
    int singleTrack = 0;
    bool singleIsMaster = false;
    std::vector<std::unique_ptr<Strip>> strips;
    std::unique_ptr<Strip> master;
    juce::Viewport viewport;
    juce::Component holder;   // the track strips, side by side, in the viewport
    juce::TextButton windowButton { "WINDOW" }, closeButton { "CLOSE" }, insertsButton { "INSERTS" }, sendsButton { "SENDS" };
    bool inWindow = false, showSends = false, rackShown = true;
    void rackChanged();
    int rackRows (int insertRows) const;   // the rack's rows (slots) for the heights
};

// A send's slot on a strip (the SENDS rack).
class MixerView::SendSlot final : public juce::Component,
                                  public juce::SettableTooltipClient
{
public:
    SendSlot (MixerView&, int busId, int slot);
    void refresh();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    const pr7s::Send* send() const;
    MixerView& owner;
    const int busId, slot;
    float startTravel = 0.0f;
    bool dragging = false;
};

// One channel strip (a track's, or the master's with busId Engine::masterBus).
class MixerView::Strip final : public juce::Component
{
public:
    Strip (MixerView&, int busId);

    void refresh();
    void setLevels (float left, float right);
    int getBusId() const noexcept { return busId; }

    ValueSlider& getVolume() noexcept { return volume; }
    ValueSlider& getPan() noexcept { return pan; }
    juce::TextButton& getMute() noexcept { return mute; }
    juce::TextButton& getRead() noexcept { return read; }
    juce::TextButton& getWrite() noexcept { return write; }
    void followMix();   // the fader and pan as the automation has them now (while it's read)
    juce::TextButton& getSolo() noexcept { return solo; }
    juce::TextButton& getInsertSlot (int slot) { return slots[(size_t) slot]; }
    SendSlot& getSendSlot (int slot) { return *sendSlots[(size_t) slot]; }
    juce::TextButton& getOutput() noexcept { return output; }
    int getShownSlots() const noexcept { return shownSlots; }
    // Its height without the rack, and with the shortest fader (the rack adds its slots).
    static constexpr int fixedHeight = 8 + 18 + 14 + 6 + 50 + 22 + 6 + 16 + 6, shortestFader = 70, shortestSlot = 11, dividerHeight = 14;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;   // the number under the fader or pan: type a value (0.9.9)
    juce::Rectangle<float> getReadoutArea() const noexcept { return readoutArea; }
    juce::Rectangle<float> getPanReadoutArea() const noexcept { return panReadoutArea; }

private:
    bool isMaster() const noexcept { return busId == pr7s::Engine::masterBus; }

    MixerView& owner;
    const int busId;
    std::array<juce::TextButton, pr7s::Project::maxInserts> slots;
    std::array<std::unique_ptr<SendSlot>, pr7s::Project::maxSends> sendSlots;
    juce::TextButton output;   // OUT: where its sound goes
    int shownSlots = 8;   // at least 8, and one more than the last used (up to 16)
    ValueSlider volume, pan;   // (double-click: type a value, 0.9.9)
    juce::TextButton mute { "M" }, solo { "S" }, read { "R" }, write { "W" };
    juce::Rectangle<float> meterArea, nameArea, readoutArea, panReadoutArea, rackArea, sendsDivider;
    LevelMeter::Ballistics left, right;
};
