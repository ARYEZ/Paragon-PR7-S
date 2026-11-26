#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/ParagonLookAndFeel.h"

// (0.209.0, the owner asked) A key that rolls something: a die beside its name, both in the
// accent (the family gradient), so ROLL CHORDS stands out from the plain keys round it.
// The key's background is the look-and-feel's own, so it sits in any row as the others do.
class DiceKey final : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;

    // A die's face (five pips) in `area`, tilted a little, filled with `colour` through setFill.
    static void drawDie (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
    {
        const auto side = juce::jmin (area.getWidth(), area.getHeight());
        const auto box = area.withSizeKeepingCentre (side, side);
        const auto corner = side * 0.22f;
        const auto pip = side * 0.17f;
        const auto c = box.getCentre();
        const auto spread = side * 0.25f;
        juce::Path face;
        face.addRoundedRectangle (box.reduced (side * 0.06f), corner);
        juce::Path pips;
        for (const auto& [dx, dy] : { std::pair { 0.0f, 0.0f }, { -1.0f, -1.0f }, { 1.0f, -1.0f }, { -1.0f, 1.0f }, { 1.0f, 1.0f } })
            pips.addEllipse (c.x + dx * spread - pip * 0.5f, c.y + dy * spread - pip * 0.5f, pip, pip);
        const auto tilt = juce::AffineTransform::rotation (-0.18f, c.x, c.y);
        face.applyTransform (tilt);
        pips.applyTransform (tilt);
        ParagonColours::setFill (g, colour, box);
        g.strokePath (face, juce::PathStrokeType (juce::jmax (1.2f, side * 0.09f)));
        g.fillPath (pips);
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto& lf = getLookAndFeel();
        lf.drawButtonBackground (g, *this, findColour (buttonColourId), highlighted, down);
        const auto colour = ParagonColours::accent.withMultipliedAlpha (isEnabled() ? 1.0f : 0.35f);
        auto area = getLocalBounds().toFloat().reduced (6.0f, 3.0f).translated (0.0f, down ? 0.5f : 0.0f);
        const auto font = lf.getTextButtonFont (*this, getHeight());
        g.setFont (font);
        const auto text = getButtonText();
        const auto iconSide = juce::jmin (area.getHeight() * 0.8f, font.getHeight() * 1.05f);
        const auto gap = iconSide * 0.4f;
        const auto textWidth = (float) juce::GlyphArrangement::getStringWidthInt (font, text);
        const auto groupWidth = juce::jmin (area.getWidth(), iconSide + gap + textWidth);
        auto group = area.withSizeKeepingCentre (groupWidth, area.getHeight());
        drawDie (g, group.removeFromLeft (iconSide), colour);
        group.removeFromLeft (gap);
        const auto textBox = group.toNearestInt();
        ParagonColours::setTextFill (g, colour, text, textBox, juce::Justification::centredLeft);
        g.drawFittedText (text, textBox, juce::Justification::centredLeft, 1, 0.7f);
    }
};
