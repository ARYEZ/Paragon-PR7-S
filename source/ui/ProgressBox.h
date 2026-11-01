#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The progress of a long job (0.11.1, the owner asked: Render in Place "did
// nothing until it was finished"): a box in the family's look over the
// studio, what's being done, and a bar drawn as one of the studio's
// horizontal meters (lit orange to pink-red as it goes) with its percentage.
//
// The jobs it shows (Render in Place, Freeze, opening a song's plug-ins) run
// on the screen's own thread, which the box can't wait for: `set` draws it at
// once (not when the thread is next free) and keeps Windows from greying the
// studio out as "Not Responding" meanwhile. The freeze watch is told too.
class ProgressBox final : public juce::Component
{
public:
    ProgressBox();
    // Shown over `over` (its window) the first time; `progress` 0 to 1.
    void set (double progress, const juce::String& what, juce::Component* over);
    double getProgress() const noexcept { return progress; }
    // A CANCEL key while a render runs (none while songs' plug-ins load: those can't stop).
    void setOnCancel (std::function<void()>);
    juce::Button& getCancelKey() { return cancelKey; }
    void resized() override;
    juce::String getWhat() const { return what; }

    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;   // (nothing happens while it works; Escape cancels)
    void inputAttemptWhenModal() override {}

    // The bar on its own (the export window's bar is drawn the same way).
    static void paintBar (juce::Graphics&, juce::Rectangle<float>, double progress);

private:
    double progress = 0.0;
    juce::String what;
    juce::uint32 lastDrawn = 0;
    juce::TextButton cancelKey { "CANCEL" };
    std::function<void()> onCancel;
};
