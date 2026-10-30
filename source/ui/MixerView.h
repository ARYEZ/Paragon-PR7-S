#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"
#include "ui/LevelMeter.h"

// The mixer, inspired by Cubase's MixConsole, in our look: a channel strip
// for each track, then the master's, on the right.
//
//   a strip        the track's colour and name; its 8 inserts (effects, top
//                  to bottom, the order the sound goes through them); pan;
//                  M (mute) and S (solo); the volume fader (double-click: 0
//                  dB) and the level; click it to select the track
//   an insert      empty: click to add an effect (one of your VST3 effects);
//                  an effect: click to open its window, bypass it, replace
//                  it or remove it
//   MASTER         every track goes through it: its inserts, then its fader
//
// It sits under the arrangement (MIXER, Alt+F3) or in a window of its own
// (F3; or WINDOW / DOCK), as the key editor does.
class MixerView final : public juce::Component,
                        private juce::ChangeListener,
                        private juce::Timer
{
public:
    explicit MixerView (pr7s::Studio&);
    ~MixerView() override;

    std::function<void (int busId, int slot)> onOpenEffect;   // its window
    std::function<void()> onFindPlugins, onClose, onWindowToggle;
    void setInWindow (bool);   // the WINDOW / DOCK button's name follows
    bool isInWindow() const noexcept { return inWindow; }

    // An insert's menu (what a click on it shows).
    void showInsertMenu (int busId, int slot);

    // For tests: the strips, in order (the master's last).
    class Strip;
    int getNumStrips() const noexcept { return (int) strips.size() + 1; }
    Strip& getStrip (int index);   // index getNumStrips() - 1: the master's

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int barHeight = 32, stripWidth = 100, masterWidth = 124;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void rebuild();

    pr7s::Studio& studio;
    std::vector<std::unique_ptr<Strip>> strips;
    std::unique_ptr<Strip> master;
    juce::Viewport viewport;
    juce::Component holder;   // the track strips, side by side, in the viewport
    juce::TextButton windowButton { "WINDOW" }, closeButton { "CLOSE" };
    bool inWindow = false;
};

// One channel strip (a track's, or the master's with busId Engine::masterBus).
class MixerView::Strip final : public juce::Component
{
public:
    Strip (MixerView&, int busId);

    void refresh();
    void setLevels (float left, float right);
    int getBusId() const noexcept { return busId; }

    juce::Slider& getVolume() noexcept { return volume; }
    juce::Slider& getPan() noexcept { return pan; }
    juce::TextButton& getMute() noexcept { return mute; }
    juce::TextButton& getSolo() noexcept { return solo; }
    juce::TextButton& getInsertSlot (int slot) { return slots[(size_t) slot]; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    bool isMaster() const noexcept { return busId == pr7s::Engine::masterBus; }

    MixerView& owner;
    const int busId;
    std::array<juce::TextButton, pr7s::Project::maxInserts> slots;
    juce::Slider volume, pan;
    juce::TextButton mute { "M" }, solo { "S" };
    juce::Rectangle<float> meterArea, nameArea, readoutArea, panReadoutArea;
    LevelMeter::Ballistics left, right;
};
