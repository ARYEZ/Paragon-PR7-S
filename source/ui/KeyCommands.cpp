#include "KeyCommands.h"

namespace KeyCommands
{
    namespace
    {
        struct Named { int code; const char* name; };
        const Named namedKeys[] = {
            { juce::KeyPress::spaceKey, "Space" }, { juce::KeyPress::escapeKey, "Esc" }, { juce::KeyPress::returnKey, "Return" },
            { juce::KeyPress::tabKey, "Tab" }, { juce::KeyPress::deleteKey, "Delete" }, { juce::KeyPress::backspaceKey, "Backspace" },
            { juce::KeyPress::insertKey, "Insert" }, { juce::KeyPress::upKey, "Up" }, { juce::KeyPress::downKey, "Down" },
            { juce::KeyPress::leftKey, "Left" }, { juce::KeyPress::rightKey, "Right" }, { juce::KeyPress::pageUpKey, "Page Up" },
            { juce::KeyPress::pageDownKey, "Page Down" }, { juce::KeyPress::homeKey, "Home" }, { juce::KeyPress::endKey, "End" },
            { juce::KeyPress::F1Key, "F1" }, { juce::KeyPress::F2Key, "F2" }, { juce::KeyPress::F3Key, "F3" }, { juce::KeyPress::F4Key, "F4" },
            { juce::KeyPress::F5Key, "F5" }, { juce::KeyPress::F6Key, "F6" }, { juce::KeyPress::F7Key, "F7" }, { juce::KeyPress::F8Key, "F8" },
            { juce::KeyPress::F9Key, "F9" }, { juce::KeyPress::F10Key, "F10" }, { juce::KeyPress::F11Key, "F11" }, { juce::KeyPress::F12Key, "F12" },
            { juce::KeyPress::F13Key, "F13" }, { juce::KeyPress::F14Key, "F14" }, { juce::KeyPress::F15Key, "F15" }, { juce::KeyPress::F16Key, "F16" },
            { juce::KeyPress::F17Key, "F17" }, { juce::KeyPress::F18Key, "F18" }, { juce::KeyPress::F19Key, "F19" }, { juce::KeyPress::F20Key, "F20" },
            { juce::KeyPress::F21Key, "F21" }, { juce::KeyPress::F22Key, "F22" }, { juce::KeyPress::F23Key, "F23" }, { juce::KeyPress::F24Key, "F24" },
            { juce::KeyPress::numberPad0, "Num 0" }, { juce::KeyPress::numberPad1, "Num 1" }, { juce::KeyPress::numberPad2, "Num 2" },
            { juce::KeyPress::numberPad3, "Num 3" }, { juce::KeyPress::numberPad4, "Num 4" }, { juce::KeyPress::numberPad5, "Num 5" },
            { juce::KeyPress::numberPad6, "Num 6" }, { juce::KeyPress::numberPad7, "Num 7" }, { juce::KeyPress::numberPad8, "Num 8" },
            { juce::KeyPress::numberPad9, "Num 9" }, { juce::KeyPress::numberPadAdd, "Num +" }, { juce::KeyPress::numberPadSubtract, "Num -" },
            { juce::KeyPress::numberPadMultiply, "Num *" }, { juce::KeyPress::numberPadDivide, "Num /" },
            { juce::KeyPress::numberPadDecimalPoint, "Num ." },
        };

        juce::String keyName (int code)
        {
            for (const auto& k : namedKeys)
                if (k.code == code)
                    return k.name;
            if (code > 32 && code < 127)
                return juce::String::charToString ((juce::juce_wchar) juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) code));
            return "Key " + juce::String (code);
        }

        bool contains (const juce::Array<juce::KeyPress>& keys, const juce::KeyPress& key)
        {
            for (const auto& k : keys)
                if (k == key)
                    return true;
            return false;
        }
    }

    juce::KeyPress normalise (const juce::KeyPress& key)
    {
        auto code = key.getKeyCode();
        if (code >= 'a' && code <= 'z')
            code = (int) juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) code);
        const auto mods = key.getModifiers().withoutMouseButtons();
        return juce::KeyPress (code, juce::ModifierKeys (mods.getRawFlags() & (juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::commandModifier
                                                                               | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier)), 0);
    }

    juce::String describe (const juce::KeyPress& press)
    {
        if (! press.isValid())
            return {};
        const auto key = normalise (press);
        const auto mods = key.getModifiers();
        juce::String text;
        if (mods.isCtrlDown() || mods.isCommandDown())
            text << "Ctrl+";
        if (mods.isAltDown())
            text << "Alt+";
        if (mods.isShiftDown())
            text << "Shift+";
        return text + keyName (key.getKeyCode());
    }

    juce::KeyPress parse (const juce::String& text)
    {
        auto rest = text.trim();
        int flags = 0;
        for (;;)
        {
            if (rest.startsWithIgnoreCase ("Ctrl+"))       { flags |= juce::ModifierKeys::commandModifier; rest = rest.substring (5); }
            else if (rest.startsWithIgnoreCase ("Cmd+"))   { flags |= juce::ModifierKeys::commandModifier; rest = rest.substring (4); }
            else if (rest.startsWithIgnoreCase ("Alt+"))   { flags |= juce::ModifierKeys::altModifier; rest = rest.substring (4); }
            else if (rest.startsWithIgnoreCase ("Shift+")) { flags |= juce::ModifierKeys::shiftModifier; rest = rest.substring (6); }
            else break;
        }
        if (rest.isEmpty())
            return {};
        int code = 0;
        for (const auto& k : namedKeys)
            if (rest.equalsIgnoreCase (k.name))
                code = k.code;
        if (code == 0 && rest.length() == 1)
            code = (int) juce::CharacterFunctions::toUpperCase (rest[0]);
        if (code == 0 && rest.startsWith ("Key "))
            code = rest.substring (4).getIntValue();
        return code != 0 ? normalise (juce::KeyPress (code, juce::ModifierKeys (flags), 0)) : juce::KeyPress();
    }

    //==========================================================================
    void Map::add (Command command)
    {
        for (auto& key : command.defaults)
            key = normalise (key);
        commands.push_back (std::move (command));
    }

    const Command* Map::find (const juce::String& id) const
    {
        for (const auto& c : commands)
            if (c.id == id)
                return &c;
        return nullptr;
    }

    juce::String Map::nameOf (const juce::String& id) const
    {
        if (id.startsWith (macroPrefix))
            return "Macro: " + id.fromFirstOccurrenceOf (macroPrefix, false, false);
        if (const auto* c = find (id))
            return c->category + " > " + c->name;
        return id;
    }

    juce::Array<juce::KeyPress> Map::defaultsFor (const juce::String& id) const
    {
        if (const auto* c = find (id))
            return c->defaults;
        return {};
    }

    juce::Array<juce::KeyPress> Map::keysFor (const juce::String& id) const
    {
        if (const auto it = custom.find (id); it != custom.end())
            return it->second;
        return defaultsFor (id);
    }

    bool Map::isCustomized (const juce::String& id) const
    {
        return custom.count (id) > 0;
    }

    juce::String Map::commandFor (const juce::KeyPress& press) const
    {
        const auto key = normalise (press);
        // A key given by hand first (it may be another command's default, taken from it).
        for (const auto& [id, keys] : custom)
            if (contains (keys, key))
                return id;
        for (const auto& c : commands)
            if (! isCustomized (c.id) && contains (c.defaults, key))
                return c.id;
        return {};
    }

    juce::String Map::assign (const juce::String& id, const juce::KeyPress& press, bool additional)
    {
        const auto key = normalise (press);
        if (! key.isValid())
            return {};
        const auto before = commandFor (key);
        if (before == id && additional)
            return {};
        if (before.isNotEmpty() && before != id)
            removeKey (before, key);
        auto keys = additional ? keysFor (id) : juce::Array<juce::KeyPress>();
        if (! contains (keys, key))
            keys.add (key);
        custom[id] = keys;
        changed();
        return before == id ? juce::String() : before;
    }

    void Map::removeKey (const juce::String& id, const juce::KeyPress& press)
    {
        const auto key = normalise (press);
        auto keys = keysFor (id);
        for (int i = keys.size(); --i >= 0;)
            if (keys.getReference (i) == key)
                keys.remove (i);
        custom[id] = keys;
        changed();
    }

    void Map::reset (const juce::String& id)
    {
        custom.erase (id);
        // Its defaults again: taken from whatever was given them since.
        for (const auto& key : defaultsFor (id))
            for (auto& [other, keys] : custom)
                if (other != id)
                    for (int i = keys.size(); --i >= 0;)
                        if (keys.getReference (i) == key)
                            keys.remove (i);
        changed();
    }

    void Map::resetAll()
    {
        // (Macros keep their keys unless one is a command's default.)
        std::map<juce::String, juce::Array<juce::KeyPress>> kept;
        for (const auto& [id, keys] : custom)
            if (id.startsWith (macroPrefix))
            {
                juce::Array<juce::KeyPress> left;
                for (const auto& key : keys)
                {
                    auto isDefault = false;
                    for (const auto& c : commands)
                        isDefault = isDefault || contains (c.defaults, key);
                    if (! isDefault)
                        left.add (key);
                }
                kept[id] = left;
            }
        custom = std::move (kept);
        changed();
    }

    void Map::clearAll()
    {
        custom.clear();
        for (const auto& c : commands)
            custom[c.id] = {};
        for (const auto& m : macros)
            custom[macroId (m.name)] = {};
        changed();
    }

    bool Map::handle (const juce::KeyPress& press, bool& swallowed)
    {
        swallowed = false;
        const auto key = normalise (press);
        const auto id = commandFor (key);
        if (id.isNotEmpty())
        {
            // Its own default: the built-in handling does it (where the last click was counts).
            if (! id.startsWith (macroPrefix) && ! isCustomized (id))
                return false;
            if (const auto* c = find (id); c != nullptr && contains (c->defaults, key))
                return false;
            return run (id);
        }
        // A default key nothing has now: it was taken away on purpose.
        for (const auto& c : commands)
            if (contains (c.defaults, key))
            {
                swallowed = true;
                return false;
            }
        return false;
    }

    bool Map::run (const juce::String& id)
    {
        if (running > 4)
            return false;   // (a macro running itself)
        if (id.startsWith (macroPrefix))
        {
            const auto name = id.fromFirstOccurrenceOf (macroPrefix, false, false);
            for (const auto& m : macros)
                if (m.name == name)
                {
                    const auto list = m.commands;   // (a command may change the macros)
                    ++running;
                    for (const auto& step : list)
                        run (step);
                    --running;
                    return true;
                }
            return false;
        }
        if (const auto* c = find (id); c != nullptr && c->action != nullptr)
        {
            ++running;
            c->action();
            --running;
            return true;
        }
        return false;
    }

    //==========================================================================
    juce::String Map::uniqueMacroName (juce::String name) const
    {
        name = name.trim().replaceCharacter ('.', ' ');
        if (name.isEmpty())
            name = "Macro";
        const auto taken = [this] (const juce::String& n)
        {
            return std::any_of (macros.begin(), macros.end(), [&n] (const Macro& m) { return m.name == n; });
        };
        auto candidate = name;
        for (int i = 2; taken (candidate); ++i)
            candidate = name + " " + juce::String (i);
        return candidate;
    }

    int Map::addMacro (const juce::String& name)
    {
        macros.push_back ({ uniqueMacroName (name), {} });
        changed();
        return (int) macros.size() - 1;
    }

    void Map::renameMacro (int index, const juce::String& name)
    {
        if (! juce::isPositiveAndBelow (index, (int) macros.size()) || name.trim() == macros[(size_t) index].name)
            return;
        const auto oldId = macroId (macros[(size_t) index].name);
        macros[(size_t) index].name = {};
        const auto newName = uniqueMacroName (name);
        macros[(size_t) index].name = newName;
        // Its keys, and macros that run it, follow the new name.
        if (const auto it = custom.find (oldId); it != custom.end())
        {
            custom[macroId (newName)] = it->second;
            custom.erase (oldId);
        }
        for (auto& m : macros)
            for (auto& step : m.commands)
                if (step == oldId)
                    step = macroId (newName);
        changed();
    }

    int Map::duplicateMacro (int index)
    {
        if (! juce::isPositiveAndBelow (index, (int) macros.size()))
            return -1;
        auto copy = macros[(size_t) index];
        copy.name = uniqueMacroName (copy.name + " copy");
        macros.insert (macros.begin() + index + 1, copy);
        changed();
        return index + 1;
    }

    void Map::deleteMacro (int index)
    {
        if (! juce::isPositiveAndBelow (index, (int) macros.size()))
            return;
        const auto id = macroId (macros[(size_t) index].name);
        custom.erase (id);
        macros.erase (macros.begin() + index);
        for (auto& m : macros)
            m.commands.removeString (id);
        changed();
    }

    void Map::addToMacro (int index, const juce::String& commandId, int after)
    {
        if (! juce::isPositiveAndBelow (index, (int) macros.size()) || commandId.isEmpty())
            return;
        auto& list = macros[(size_t) index].commands;
        if (juce::isPositiveAndBelow (after, list.size()))
            list.insert (after + 1, commandId);   // (after the one selected, as the manual has it)
        else
            list.add (commandId);
        changed();
    }

    void Map::removeFromMacro (int index, int position)
    {
        if (! juce::isPositiveAndBelow (index, (int) macros.size()))
            return;
        macros[(size_t) index].commands.remove (position);
        changed();
    }

    void Map::moveInMacro (int index, int position, int by)
    {
        if (! juce::isPositiveAndBelow (index, (int) macros.size()))
            return;
        auto& list = macros[(size_t) index].commands;
        const auto to = position + by;
        if (! juce::isPositiveAndBelow (position, list.size()) || ! juce::isPositiveAndBelow (to, list.size()))
            return;
        list.move (position, to);
        changed();
    }

    void Map::changed()
    {
        if (onChange)
            onChange();
    }

    //==========================================================================
    std::unique_ptr<juce::XmlElement> Map::toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement> ("KeyCommands");
        xml->setAttribute ("version", 1);
        for (const auto& [id, keys] : custom)
        {
            auto* e = xml->createNewChildElement ("Command");
            e->setAttribute ("id", id);
            juce::StringArray texts;
            for (const auto& key : keys)
                texts.add (describe (key));
            e->setAttribute ("keys", texts.joinIntoString (", "));
        }
        for (const auto& m : macros)
        {
            auto* e = xml->createNewChildElement ("Macro");
            e->setAttribute ("name", m.name);
            for (const auto& step : m.commands)
                e->createNewChildElement ("Step")->setAttribute ("command", step);
        }
        return xml;
    }

    void Map::fromXml (const juce::XmlElement& xml)
    {
        if (! xml.hasTagName ("KeyCommands"))
            return;
        custom.clear();
        macros.clear();
        for (const auto* m : xml.getChildWithTagNameIterator ("Macro"))
        {
            Macro macro { m->getStringAttribute ("name"), {} };
            for (const auto* step : m->getChildWithTagNameIterator ("Step"))
                macro.commands.add (step->getStringAttribute ("command"));
            if (macro.name.isNotEmpty())
                macros.push_back (macro);
        }
        for (const auto* e : xml.getChildWithTagNameIterator ("Command"))
        {
            const auto id = e->getStringAttribute ("id");
            const auto isMacro = id.startsWith (macroPrefix)
                && std::any_of (macros.begin(), macros.end(), [&] (const Macro& m) { return macroId (m.name) == id; });
            if (find (id) == nullptr && ! isMacro)
                continue;   // (a command this version doesn't have)
            juce::Array<juce::KeyPress> keys;
            for (const auto& text : juce::StringArray::fromTokens (e->getStringAttribute ("keys"), ",", {}))
                if (const auto key = parse (text); key.isValid())
                    keys.add (key);
            custom[id] = keys;
        }
        changed();
    }

    juce::String Map::toHtml() const
    {
        // The manual's Export Key Command Assignments: a page listing them all.
        juce::String html;
        html << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>Paragon PR7-S key commands</title>"
             << "<style>body{font-family:sans-serif;background:#141014;color:#f4eeea}h2{color:#cd6619}"
             << "td{padding:2px 14px 2px 0}td.k{color:#de0639;font-weight:bold}</style></head><body>"
             << "<h1>Paragon PR7-S key commands</h1>";
        juce::String category;
        for (const auto& c : commands)
        {
            if (c.category != category)
            {
                if (category.isNotEmpty())
                    html << "</table>";
                category = c.category;
                html << "<h2>" << category << "</h2><table>";
            }
            juce::StringArray texts;
            for (const auto& key : keysFor (c.id))
                texts.add (describe (key));
            html << "<tr><td>" << c.name << "</td><td class=\"k\">" << texts.joinIntoString (", ") << "</td></tr>";
        }
        if (category.isNotEmpty())
            html << "</table>";
        if (! macros.empty())
        {
            html << "<h2>Macro</h2><table>";
            for (const auto& m : macros)
            {
                juce::StringArray texts, steps;
                for (const auto& key : keysFor (macroId (m.name)))
                    texts.add (describe (key));
                for (const auto& step : m.commands)
                    steps.add (nameOf (step));
                html << "<tr><td>" << m.name << "</td><td class=\"k\">" << texts.joinIntoString (", ") << "</td><td>"
                     << steps.joinIntoString (", then ") << "</td></tr>";
            }
            html << "</table>";
        }
        html << "</body></html>";
        return html;
    }

    juce::File Map::presetFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Paragon PR7-S").getChildFile ("Key Commands");
    }
}
