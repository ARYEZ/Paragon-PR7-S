#pragma once

#include <optional>

#include <juce_gui_basics/juce_gui_basics.h>

// Typing a value in (0.9.9, the owner asked): double-click a fader or a pan
// knob (or the number under it) and type "-6", "+3 dB", "-inf"; for pan "L30",
// "R 15", "C" or -30 (left) / 30 (right). Enter sets it (one undo step, as a
// drag); Esc or a click elsewhere leaves it as it was. Ctrl-click still resets
// (0 dB, the centre).
namespace ValueEntry
{
    // dB: a number, with or without "dB"; "-inf", "inf", "off" or "-oo" for silence (`lowest`).
    inline std::optional<double> parseDb (juce::String text, double lowest = -60.0, double highest = 6.0)
    {
        text = text.trim().toLowerCase().replace ("db", "").trim();
        if (text == "-inf" || text == "inf" || text == "off" || text == "-oo" || text == "oo")
            return lowest;
        if (text.isEmpty() || ! text.containsAnyOf ("0123456789"))
            return std::nullopt;
        if (! text.containsOnly ("+-.,0123456789"))
            return std::nullopt;
        return juce::jlimit (lowest, highest, text.replaceCharacter (',', '.').getDoubleValue());
    }

    // Pan, -1 (left) to 1 (right): "C"; "L30" / "30L" / "L 30"; "R15"; or a number, - for left.
    inline std::optional<double> parsePan (juce::String text)
    {
        text = text.trim().toUpperCase().removeCharacters (" %");
        if (text == "C" || text == "CENTRE" || text == "CENTER" || text == "0")
            return 0.0;
        auto sign = 1.0;
        if (text.containsChar ('L')) { sign = -1.0; text = text.removeCharacters ("L"); }
        else if (text.containsChar ('R')) text = text.removeCharacters ("R");
        if (text.isEmpty() || ! text.containsOnly ("+-.,0123456789") || ! text.containsAnyOf ("0123456789"))
            return std::nullopt;
        return juce::jlimit (-1.0, 1.0, sign * text.replaceCharacter (',', '.').getDoubleValue() / 100.0);
    }
}

// A slider whose double-click opens a box to type its value in (instead of
// JUCE's jump to a value). `parse` turns the text into a value (none: refused).
class ValueSlider : public juce::Slider
{
public:
    std::function<std::optional<double> (const juce::String&)> parse;

    // (0.71.0, the owner's) What a drag moves it by, snapped to it (0: the range's interval). Typed
    // values and automation aren't snapped: only the mouse's drag (JUCE asks `snapValue` for those).
    double dragStep = 0.0;

    double snapValue (double attempted, juce::Slider::DragMode mode) override
    {
        if (dragStep <= 0.0 || mode == juce::Slider::notDragging)
            return attempted;
        return juce::jlimit (getMinimum(), getMaximum(), std::round (attempted / dragStep) * dragStep);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (parse == nullptr)
            juce::Slider::mouseDoubleClick (e);
        else
            showEntry();
    }

    // The value typed in: set as one change (one undo step: no mouse button is down).
    bool enter (const juce::String& text)
    {
        const auto value = parse != nullptr ? parse (text) : std::nullopt;
        if (! value)
            return false;
        setValue (*value, juce::sendNotificationSync);
        return true;
    }

    void showEntry()
    {
        struct Entry final : juce::Component
        {
            explicit Entry (ValueSlider& s) : slider (&s)
            {
                editor.setText (s.getTextFromValue (s.getValue()), false);
                editor.setJustification (juce::Justification::centred);
                editor.setSelectAllWhenFocused (true);
                editor.onReturnKey = [this]
                {
                    if (slider != nullptr && ! slider->enter (editor.getText()))
                    {
                        editor.setText ("?", false);   // not a value it takes: try again
                        editor.selectAll();
                        return;
                    }
                    close();
                };
                editor.onEscapeKey = [this] { close(); };
                addAndMakeVisible (editor);
                setSize (110, 30);
            }
            void resized() override { editor.setBounds (getLocalBounds()); }
            void parentHierarchyChanged() override
            {
                juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<juce::TextEditor> (&editor)]
                {
                    if (safe != nullptr && safe->isShowing())
                        safe->grabKeyboardFocus();
                });
            }
            void close()
            {
                // Hidden now (dismiss() waits for a message); JUCE deletes it after, outside this key press.
                if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                {
                    box->exitModalState (0);
                    box->setVisible (false);
                }
            }
            juce::Component::SafePointer<ValueSlider> slider;
            juce::TextEditor editor;
        };
        juce::CallOutBox::launchAsynchronously (std::make_unique<Entry> (*this), getScreenBounds(), nullptr);
    }
};
