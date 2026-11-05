#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/MidiExport.h"
#include "engine/Studio.h"

// The MIDI file export's options (0.51.0, the manual's Export Options Dialog for MIDI Files): markers,
// type 0, the resolution, the locator range only, the tracks' volume and pan. EXPORT... asks where.
class MidiExportPage final : public juce::Component
{
public:
    explicit MidiExportPage (pr7s::Studio&);
    void paint (juce::Graphics&) override;
    void resized() override;
    pr7s::MidiExportOptions getOptions() const;
    std::function<void (const pr7s::MidiExportOptions&)> onExport;
    std::function<void()> onCancel;

    juce::ToggleButton markers { "Export Markers" }, type0 { "Export as Type 0 (all on one track)" },
                       locators { "Export Locator Range (only between the locators)" },
                       volumePan { "Export the tracks' Volume/Pan (as CC 7 and 10 at the start)" };
    juce::ComboBox resolution;
    juce::TextButton exportButton { "EXPORT..." }, cancelButton { "CANCEL" };

private:
    pr7s::Studio& studio;
    juce::Label resolutionLabel, note;
};
