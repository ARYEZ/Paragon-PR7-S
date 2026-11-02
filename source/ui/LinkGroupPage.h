#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// The Link Group Settings dialog (0.17.0, the manual's): the group's name and
// which settings its channels share. LINK (a new group: `trackIds`, the
// selected channels) or OK (an existing group's settings: `groupId`).
// The manual's defaults: volume, sends, routing and selection.
class LinkGroupPage final : public juce::Component
{
public:
    LinkGroupPage (pr7s::Studio&, int groupId, std::vector<int> trackIds);
    void paint (juce::Graphics&) override;
    void resized() override;

    pr7s::LinkGroup getSettings() const;
    void setLinks (pr7s::LinkGroup::What, bool);   // (tests: a box ticked)
    void apply();                                   // LINK / OK: makes the group or changes it
    std::function<void()> onClose;

    // Shown as a box over `around` (new: the tracks given; else the group's settings).
    static void show (juce::Component& around, pr7s::Studio&, int groupId, std::vector<int> trackIds);
    // The mixer's LINK key and the menus' Link Selected Channels: the selected tracks
    // all in one group (selecting one selects them all) unlink it; two or more selected
    // otherwise: the dialog for a new group. False when there's nothing to do.
    static bool linkOrUnlinkSelected (juce::Component& around, pr7s::Studio&);

    juce::TextEditor name;
    juce::TextButton okKey, cancelKey { "CANCEL" };

private:
    pr7s::Studio& studio;
    const int groupId;
    const std::vector<int> trackIds;
    struct Option { pr7s::LinkGroup::What what; const char* label; };
    static const std::array<Option, 9>& options();
    juce::OwnedArray<juce::ToggleButton> boxes;
    juce::Label note;
};
