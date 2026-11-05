#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// The Tempo Detection panel (0.45.0, the manual's): for the selected audio event or MIDI part (at
// least 7 s), ANALYZE finds its beats and the tempo track follows them (Studio::applyDetectedBeats:
// everything keeps its place in time). The corrections work on the beats found and apply again:
// x2, /2, x4/3, x3/4, OFFBEAT (half a beat on), SMOOTH TEMPO; RESET puts the tempo back as it was
// before the first ANALYZE (everything moved back with it). Each apply is one undo step.
class TempoDetectionPanel final : public juce::Component
{
public:
    TempoDetectionPanel (pr7s::Studio&, std::function<std::vector<int>()> selectedParts);
    void paint (juce::Graphics&) override;
    void resized() override;

    // (Tests drive these as the keys do.)
    bool analyse();                  // the first selected part
    bool analysePart (int partId);
    enum class Correction { times2, half, fourThirds, threeQuarters, offbeat, smooth };
    bool correct (Correction);
    bool reset();
    const std::vector<double>& getBeats() const noexcept { return beats; }
    juce::String getStatus() const { return status; }
    juce::TextButton* keyNamed (const juce::String&);
    static constexpr int preferredWidth = 760, preferredHeight = 330;

private:
    bool apply();
    void refreshKeys();
    pr7s::Studio& studio;
    std::function<std::vector<int>()> selectedParts;
    int partId = 0;
    juce::String partName;
    std::vector<double> beats;
    double confidence = 0.0;
    bool haveOriginal = false, originalOn = false;
    std::vector<pr7s::TempoEvent> originalEvents;
    juce::String status { "Select an audio event or a MIDI part of 7 seconds or more, then ANALYZE." };
    juce::TextButton analyseKey { "ANALYZE" }, times2 { "X 2" }, half { "/ 2" }, fourThirds { "X 4/3" }, threeQuarters { "X 3/4" },
                     offbeat { "OFFBEAT" }, smooth { "SMOOTH TEMPO" }, resetKey { "RESET" };
    juce::Rectangle<float> graph;
};

class TempoDetectionWindow final : public juce::DocumentWindow
{
public:
    TempoDetectionWindow (pr7s::Studio&, std::function<std::vector<int>()> selectedParts);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    TempoDetectionPanel& getPanel() { return *panel; }

private:
    TempoDetectionPanel* panel = nullptr;
};
