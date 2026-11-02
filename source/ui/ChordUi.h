#pragma once

#include "engine/Chords.h"
#include "ui/ParagonLookAndFeel.h"

// What the chord pads and the chord track share (0.16.0): chord names drawn in
// Inter (Bebas has no small letters: "Dm" would read "DM", "Eb" "EB"), keys
// written out in words for the toolbars and menus.
namespace ChordUi
{
    inline juce::Font chordFont (float height) { return juce::Font (juce::FontOptions (height, juce::Font::bold)); }

    // A key whose label is a note or chord name: drawn in Inter.
    class InterKey : public juce::TextButton
    {
    public:
        using juce::TextButton::TextButton;
        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            getLookAndFeel().drawButtonBackground (g, *this, findColour (getToggleState() ? buttonOnColourId : buttonColourId), over, down);
            g.setColour (findColour (getToggleState() ? textColourOnId : textColourOffId).withMultipliedAlpha (isEnabled() ? 1.0f : 0.4f));
            g.setFont (chordFont (juce::jmin (14.0f, (float) getHeight() * 0.6f)));
            g.drawFittedText (getButtonText(), getLocalBounds().reduced (2), juce::Justification::centred, 1, 0.7f);
        }
    };

    // "E-flat minor".
    inline juce::String spokenKey (const pr7s::Key& key)
    {
        juce::String t = juce::String::charToString ((juce::juce_wchar) "CDEFGAB"[key.letter]);
        if (key.accidental == -1) t << "-flat";
        if (key.accidental == 1)  t << "-sharp";
        return t + (key.minor ? " minor" : " major");
    }

    inline juce::String signatureText (const pr7s::Key& key)
    {
        const auto n = key.signature();
        if (n == 0)
            return "no sharps or flats";
        return juce::String (std::abs (n)) + (n > 0 ? (std::abs (n) == 1 ? " sharp" : " sharps") : (std::abs (n) == 1 ? " flat" : " flats"));
    }

    inline void styleKey (juce::TextButton& key)
    {
        key.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        key.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        key.setColour (juce::TextButton::textColourOnId, ParagonColours::accent);
        key.setWantsKeyboardFocus (false);
    }
}
