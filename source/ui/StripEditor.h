#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"
#include "ui/ValueSlider.h"

// (0.248.0) The CHANNEL STRIP: PR7-C's modules on a channel (or the master), in PR7-C's fixed order, left to
// right: GATE / EXP and its SC FILTER, COMPRESSOR, [the channel's EQ, in its own window], SATURATION, CONSOLE
// COLOUR, MONO MAKER / STEREO WIDTH. Every control is the studio's setting (Studio::setChannelStrip): a knob's
// drag is heard as it moves and is one undo step when let go. Our look (the family's keys and knobs), not
// PR7-C's panel.
class StripEditor final : public juce::Component,
                          private juce::Timer
{
public:
    StripEditor (pr7s::Studio&, int busId);
    ~StripEditor() override;

    void setBus (int busId);
    int getBus() const noexcept { return bus; }

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int preferredWidth = 1240, preferredHeight = 400;

    // (Tests) A knob by its id ("comp.threshold"...), set as a release would (one undo step); a key by its id
    // ("comp.on", "comp.fet"...) clicked; what a knob shows.
    void setKnob (const juce::String& id, double value);
    double getKnob (const juce::String& id) const;
    void clickKey (const juce::String& id);
    bool isKeyLit (const juce::String& id) const;
    juce::Component* findControl (const juce::String& id) const;
    // The module columns' places, for a snapshot's eye and tests.
    juce::Rectangle<int> getModuleArea (int index) const;
    static constexpr int numModules = 6;   // GATE, SC FILTER, COMP, SATURATION, COLOUR, STEREO

    // The strip's menu (also the Inspector's header): every module off, the console channel at random.
    static void showMenu (pr7s::Studio&, int busId, juce::Component* target);

private:
    struct Knob
    {
        juce::String id, name;
        std::unique_ptr<ValueSlider> slider;
        std::function<double (const pr7s::ChannelStrip&)> get;
        std::function<void (pr7s::ChannelStrip&, double)> set;
        std::function<juce::String (double)> text;
        int module = 0;
    };
    struct Key
    {
        juce::String id;
        std::unique_ptr<juce::TextButton> button;
        std::function<bool (const pr7s::ChannelStrip&)> lit;
        std::function<void (pr7s::ChannelStrip&)> click;
        int module = 0;
        int row = 0;
    };

    void addKnob (int module, const juce::String& id, const juce::String& name, double low, double high, double centre,
                  std::function<double (const pr7s::ChannelStrip&)> get, std::function<void (pr7s::ChannelStrip&, double)> set,
                  std::function<juce::String (double)> text, double step = 0.0);
    void addKey (int module, int row, const juce::String& id, const juce::String& name, const juce::String& tip,
                 std::function<bool (const pr7s::ChannelStrip&)> lit, std::function<void (pr7s::ChannelStrip&)> click);
    pr7s::ChannelStrip current() const;
    void change (const std::function<void (pr7s::ChannelStrip&)>&, bool undoable);
    void refresh();
    void timerCallback() override;

    pr7s::Studio& studio;
    int bus = 0;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Key>> keys;
    juce::TextButton menuKey { "STRIP" };
    pr7s::ChannelStrip shown;
    float gateReduction = 0.0f, compReduction = 0.0f;
    bool dragging = false;
};

class StripWindow final : public juce::DocumentWindow
{
public:
    StripWindow (pr7s::Studio&, int busId);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    StripEditor& getEditor() { return *editor; }

private:
    StripEditor* editor = nullptr;
};
