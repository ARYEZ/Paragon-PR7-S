#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// The Auto Fades dialog (0.52.0, the manual's): fades made as the audio plays, at each
// event's ends (AUTO FADE IN / OUT) and where events meet (AUTO CROSSFADES: comped takes),
// LENGTH 1-500 ms, EQUAL POWER or EQUAL GAIN. For the song (`trackId` 0), or for one audio
// track: USE PROJECT SETTINGS off gives the track its own. OK applies, CANCEL leaves it.
// Not (yet): the curve type and shape buttons, As Default.
class AutoFadesPage final : public juce::Component
{
public:
    AutoFadesPage (pr7s::Studio&, int trackId);
    void paint (juce::Graphics&) override;
    void resized() override;

    pr7s::AutoFades getSettings() const;
    bool usesProjectSettings() const { return useProject.getToggleState(); }
    void setLength (double milliseconds);   // (tests)
    void apply();                           // OK
    std::function<void()> onClose;

    static void show (juce::Component& around, pr7s::Studio&, int trackId);

    juce::ToggleButton fadeIn { "AUTO FADE IN" }, fadeOut { "AUTO FADE OUT" }, crossfades { "AUTO CROSSFADES" },
                       useProject { "USE PROJECT SETTINGS" }, equalPower { "EQUAL POWER" }, equalGain { "EQUAL GAIN" };
    juce::Slider length;
    juce::TextButton okKey { "OK" }, cancelKey { "CANCEL" };

private:
    void refreshEnabled();
    pr7s::Studio& studio;
    const int trackId;
    juce::Label note;
};
