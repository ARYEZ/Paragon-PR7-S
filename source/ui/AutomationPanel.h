#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// The Automation panel (0.28.0, the manual's Automation Panel; F6): two tabs.
//
//   OPERATIONS  READ / WRITE (all tracks), SUSPEND (all reading and writing); the mode: TOUCH,
//               AUTO-LATCH, CROSS-OVER; TRIM; FUNCTIONS (delete, fill gaps, freeze trim, copy
//               and paste automation).
//               FILL: TO PUNCH, TO START, TO END, LOOP, GAPS (a click: once, for the next pass;
//               again: locked, every pass; again: off).
//               SUSPEND READ / SUSPEND WRITE: VOLUME, PAN, MUTE, SENDS, PLUG-INS, SET ALL.
//               SHOW: VOLUME, PAN, MUTE, SENDS, PLUG-INS (every track's lanes of that kind;
//               USED ONLY: those with points), SHOW USED, HIDE ALL.
//   SETTINGS    USE VIRGIN TERRITORY, CONTINUE WRITING ON TRANSPORT JUMP, REVEAL PARAMETER ON
//               WRITE, RETURN TIME, REDUCTION LEVEL, FREEZE TRIM (manually, on pass end, on
//               leaving trim mode).
//
// All of it is the song's (saved with it); it follows the studio (keys lit as things are).
class AutomationPanel final : public juce::Component, private juce::ChangeListener
{
public:
    explicit AutomationPanel (pr7s::Studio&);
    ~AutomationPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    enum class Tab { operations, settings };
    void setTab (Tab);
    Tab getTab() const noexcept { return tab; }
    void refresh();   // the keys as the studio's settings are
    void showFunctions();

    // For the tests: a key by its text (in this panel).
    juce::TextButton* keyNamed (const juce::String&);
    juce::Slider& getReturnTime() noexcept { return returnTime; }
    juce::Slider& getReductionLevel() noexcept { return reduction; }
    juce::ComboBox& getFreezeTrim() noexcept { return freezeTrim; }

    static constexpr int preferredWidth = 860, preferredHeight = 430;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    juce::TextButton& makeKey (const juce::String& text, std::function<void()> onClick, const juce::String& tip = {});

    pr7s::Studio& studio;
    Tab tab = Tab::operations;
    bool usedOnly = false;
    std::vector<std::unique_ptr<juce::TextButton>> keys;
    juce::TextButton* operationsTab = nullptr, *settingsTab = nullptr;
    juce::TextButton* readKey = nullptr, *writeKey = nullptr, *suspendKey = nullptr, *trimKey = nullptr, *functionsKey = nullptr;
    std::array<juce::TextButton*, 3> modeKeys {};
    std::array<juce::TextButton*, 5> fillKeys {};
    std::array<juce::TextButton*, 6> readKeys {}, writeKeys {};   // (the last: SET ALL)
    std::array<juce::TextButton*, 5> showKeys {};
    juce::TextButton* usedOnlyKey = nullptr, *showUsedKey = nullptr, *hideAllKey = nullptr;
    juce::ToggleButton virgin { "USE VIRGIN TERRITORY" }, continueOnJump { "CONTINUE WRITING ON TRANSPORT JUMP" }, reveal { "REVEAL PARAMETER ON WRITE" };
    juce::Slider returnTime, reduction;
    juce::Label returnLabel, reductionLabel, freezeLabel;
    juce::ComboBox freezeTrim;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> headings;
};

class AutomationPanelWindow final : public juce::DocumentWindow
{
public:
    explicit AutomationPanelWindow (pr7s::Studio&);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    AutomationPanel& getPanel() { return *panel; }

private:
    AutomationPanel* panel = nullptr;
};
