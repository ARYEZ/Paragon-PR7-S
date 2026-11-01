#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>
#include <vector>

// Key commands and macros (0.11.0, the owner asked: more shortcuts, and an
// Elgato Stream Deck driving the studio as it drives the owner's other DAW).
// The manual's Key Commands dialog: every action is a command in a category,
// with keys (several allowed: the manual's Assign Additional Key); keys can
// be removed, reset, cleared or set back to the defaults; presets saved and
// loaded as XML; the assignments exported as HTML. Macros: named lists of
// commands, run one after another, each a command of its own (the Macro
// category) that can have keys too.
//
// The Stream Deck: its Hotkey action presses a key combination; give that
// combination to a command or a macro here and the button runs it (in the
// plug-in windows too: they pass their keys to the studio).
//
// The defaults are the studio's keys as they were, and they still work
// exactly as before (with what they do depending on where the last click
// was: MainView's built-in handling). A key given to a command runs that
// command first; a default key taken away from its command does nothing.
namespace KeyCommands
{
    struct Command
    {
        juce::String id;          // stable, saved ("Transport.StartStop")
        juce::String category;    // the manual's folders: Transport, File, Edit...
        juce::String name;        // as shown
        juce::Array<juce::KeyPress> defaults;
        std::function<void()> action;
    };

    struct Macro
    {
        juce::String name;
        juce::StringArray commands;   // command ids, in order
    };

    // "Ctrl+Alt+F13", "Shift+H", "Space"; and back.
    juce::String describe (const juce::KeyPress&);
    juce::KeyPress parse (const juce::String&);
    // The key without mouse buttons (as it's stored and compared).
    juce::KeyPress normalise (const juce::KeyPress&);

    class Map
    {
    public:
        static constexpr const char* macroPrefix = "Macro.";

        void add (Command);
        const std::vector<Command>& getCommands() const noexcept { return commands; }
        const Command* find (const juce::String& id) const;
        juce::String nameOf (const juce::String& id) const;   // a command's or a macro's

        // Assignments.
        juce::Array<juce::KeyPress> keysFor (const juce::String& id) const;   // a macro's too
        juce::Array<juce::KeyPress> defaultsFor (const juce::String& id) const;
        bool isCustomized (const juce::String& id) const;
        // Gives the key to the command (taking it from whatever had it: a key
        // does one thing); `additional` keeps its other keys, else they go.
        // Returns what had the key before (empty if nothing).
        juce::String assign (const juce::String& id, const juce::KeyPress&, bool additional = true);
        void removeKey (const juce::String& id, const juce::KeyPress&);
        void reset (const juce::String& id);
        void resetAll();   // the manual's Defaults (macros kept)
        void clearAll();   // the manual's Clear All: no keys at all
        juce::String commandFor (const juce::KeyPress&) const;   // what the key runs (a command or a macro id), or empty

        // The key press, for MainView: run a command given this key by the
        // owner (true); `swallowed` set if it's a default key taken away from
        // its command (it then does nothing). False otherwise: the built-in keys.
        bool handle (const juce::KeyPress&, bool& swallowed);

        // Macros.
        std::vector<Macro>& getMacros() noexcept { return macros; }
        const std::vector<Macro>& getMacros() const noexcept { return macros; }
        int addMacro (const juce::String& name);            // its index; a unique name
        void renameMacro (int index, const juce::String& name);
        int duplicateMacro (int index);
        void deleteMacro (int index);
        void addToMacro (int index, const juce::String& commandId, int after = -1);
        void removeFromMacro (int index, int position);
        void moveInMacro (int index, int position, int by);
        static juce::String macroId (const juce::String& name) { return juce::String (macroPrefix) + name; }

        // Runs a command, or a macro's commands one after another. False if there's no such thing.
        bool run (const juce::String& id);

        // Saving: the settings file, presets and the HTML list.
        std::unique_ptr<juce::XmlElement> toXml() const;
        void fromXml (const juce::XmlElement&);   // the assignments and macros (unknown commands ignored)
        juce::String toHtml() const;
        std::function<void()> onChange;          // something changed (MainView saves it)

        static juce::File presetFolder();        // Documents/Paragon PR7-S/Key Commands
        static constexpr const char* presetExtension = ".pr7keys";

    private:
        std::vector<Command> commands;
        std::map<juce::String, juce::Array<juce::KeyPress>> custom;   // only the commands changed from their defaults
        std::vector<Macro> macros;
        int running = 0;   // (a macro inside a macro: no deeper than a few)
        int stepsLeft = 0; // (and no more than a thousand steps from one key)
        void changed();
        juce::String uniqueMacroName (juce::String) const;
    };
}
