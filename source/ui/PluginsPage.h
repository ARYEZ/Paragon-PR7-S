#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "engine/Studio.h"

// The PLUGINS window's page (0.52.2, the owner asked: as the Hub's look). The Hub's pearl header
// with the mark, then the plug-ins found in the Hub's red-edged panel: names in Bebas, maker,
// kind and version dimmed, the chosen row lit as the Hub's. JUCE's PluginListComponent does the
// work underneath (SCAN / OPTIONS, the scan, Delete, the right-click menu, sorting by a column);
// only its rows are ours (`Model`).
class PluginsPage final : public juce::Component
{
public:
    explicit PluginsPage (pr7s::Studio&);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::PluginListComponent& getList() noexcept { return *list; }

    class Model;   // (the rows: tests read their text)
    static juce::String cellText (const juce::KnownPluginList&, int row, int column);

private:
    pr7s::Studio& studio;
    std::unique_ptr<juce::PluginListComponent> list;
    juce::Image satin;
};
