#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// A frame that shows a component bigger (0.9.3: the key editor at double
// size, as the owner asked: everything in it, text, keys, notes, buttons).
// The component is laid out at the holder's size divided by the scale and
// drawn scaled up, so it works unchanged (its mouse positions are its own).
class ScaledHolder final : public juce::Component
{
public:
    explicit ScaledHolder (juce::Component& toShow) : content (toShow) { addAndMakeVisible (content); }

    void setScale (float newScale)
    {
        scale = juce::jlimit (0.5f, 4.0f, newScale);
        resized();
        repaint();
    }
    float getScale() const noexcept { return scale; }

    void resized() override
    {
        content.setTransform (juce::AffineTransform::scale (scale));
        content.setBounds (0, 0, juce::roundToInt ((float) getWidth() / scale), juce::roundToInt ((float) getHeight() / scale));
    }

private:
    juce::Component& content;
    float scale = 1.0f;
};
