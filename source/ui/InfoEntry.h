#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The text box the info lines type values in (the automation's and the MIDI editor's). JUCE's
// TextEditor only answers Enter with no modifier held: Ctrl+Enter (the manual's "set them all
// to this value") went past it and nothing was applied. This one takes Enter with any modifier
// and says whether Ctrl (Cmd on a Mac) was held.
class InfoEntry final : public juce::TextEditor
{
public:
    std::function<void (bool absolute)> onEnter;

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key.getKeyCode() == juce::KeyPress::returnKey && onEnter)
        {
            onEnter (key.getModifiers().isCommandDown());
            return true;
        }
        return juce::TextEditor::keyPressed (key);
    }
};
