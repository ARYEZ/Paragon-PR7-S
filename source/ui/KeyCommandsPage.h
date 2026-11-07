#pragma once

#include "ui/KeyCommands.h"

// The Key Commands window (0.11.0, the manual's Key Commands dialog, KEYS in
// the header). Left: the commands by category, a search (by name, or PRESS
// KEY: press a key to see what it's on), and All / Assigned / Customized /
// Unassigned. Right: the chosen command's keys (REMOVE each), CLICK HERE AND
// PRESS A KEY (another key for it: the manual's Assign Additional Key; a key
// another command had moves here, and it says so), RESET; then the macros:
// NEW, DUPLICATE, DELETE, RENAME, and the chosen macro's commands (ADD the
// command chosen on the left after the one chosen here, REMOVE, UP, DOWN).
// A macro chosen is the command whose keys are shown (give it a key too).
// Foot: DEFAULTS, CLEAR ALL, PRESETS (save, load), EXPORT LIST (HTML).
class KeyCommandsPage final : public juce::Component
{
public:
    explicit KeyCommandsPage (KeyCommands::Map&);
    ~KeyCommandsPage() override;

    enum class Filter { all, assigned, customized, unassigned };
    void setSearch (const juce::String&);
    void setFilter (Filter);
    void searchKey (const juce::KeyPress&);           // PRESS KEY: only the command with this key
    std::vector<juce::String> getShownCommands() const; // the command ids listed (tests)
    void selectCommand (const juce::String& id);
    juce::String getSelectedCommand() const noexcept { return selected; }
    // The key catcher's key (as pressed after CLICK HERE): given to the selected command.
    void assignKey (const juce::KeyPress&);
    juce::String getStatus() const;
    void selectMacro (int index);
    int getSelectedMacro() const noexcept { return macroIndex; }
    void savePresetAs (const juce::File&);
    bool loadPreset (const juce::File&);
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int preferredWidth = 1060 + 36, preferredHeight = 680 + 124;   // (0.71.0: the Hub's header and panel round it)

private:
    class CommandList;
    class MacroList;
    class StepList;
    class KeyCatcher;
    KeyCommands::Map& map;
    std::unique_ptr<CommandList> commandList;
    std::unique_ptr<MacroList> macroList;
    std::unique_ptr<StepList> stepList;
    std::unique_ptr<KeyCatcher> catcher;
    juce::TextEditor search;
    juce::TextButton pressKey { "PRESS KEY" }, defaults { "DEFAULTS" }, clearAll { "CLEAR ALL" }, presets { "PRESETS" }, exportList { "EXPORT LIST" },
                     reset { "RESET" }, newMacro { "NEW" }, duplicateMacro { "DUPLICATE" }, deleteMacro { "DELETE" }, renameMacro { "RENAME" },
                     addStep { "ADD COMMAND" }, removeStep { "REMOVE" }, stepUp { "UP" }, stepDown { "DOWN" };
    juce::ComboBox filterBox;
    std::vector<std::unique_ptr<juce::TextButton>> keyRows;   // the chosen command's keys, each a REMOVE
    juce::Label status;
    juce::String selected;
    int macroIndex = -1, stepIndex = -1;
    Filter filter = Filter::all;
    juce::KeyPress searchedKey;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> commandArea, keysArea, macrosArea, keysTitle, macrosTitle;
    juce::Image satin;   // (0.71.0: the Hub's header)
    void rebuildKeys();
    void showPresetMenu();
    friend class CommandList;
};

// Its window (KEYS): kept open beside the studio while it's worked in.
class KeyCommandsWindow final : public juce::DocumentWindow
{
public:
    explicit KeyCommandsWindow (KeyCommands::Map&);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    KeyCommandsPage& getPage() { return *page; }

private:
    KeyCommandsPage* page = nullptr;
};
