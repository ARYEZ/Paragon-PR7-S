#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Finish.h"
#include "ui/Logo.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/Pearl.h"

// The Hub's look for a window's page (0.52.2's PLUGINS; since 0.71.0 SETTINGS and KEYS too, the owner
// asked): the pearl header with the mark and a caption, then the page in the Hub's red-edged panel.
namespace HubFrame
{
    inline const juce::Colour onyx { 0xff0c0a0d };    // (the Hub's window)
    inline const juce::Colour panel { 0xff120e13 };   // (its panel)
    constexpr int headerHeight = 96;
    constexpr int pageMarginX = 18, pageMarginY = 14;  // the panel inside the window below the header
    constexpr int inset = 14;                          // what's in the panel, inside its edge

    // Where the header and the panel go in a page of this size.
    inline juce::Rectangle<int> headerOf (juce::Rectangle<int> bounds) { return bounds.removeFromTop (headerHeight); }
    inline juce::Rectangle<int> panelOf (juce::Rectangle<int> bounds)
    {
        bounds.removeFromTop (headerHeight);
        return bounds.reduced (pageMarginX, pageMarginY);
    }
    // The room inside the panel for the page's own controls.
    inline juce::Rectangle<int> contentOf (juce::Rectangle<int> bounds) { return panelOf (bounds).reduced (inset); }
    // The extra size the frame adds round a page's controls.
    constexpr int extraWidth = 2 * (pageMarginX + inset), extraHeight = headerHeight + 2 * (pageMarginY + inset);

    // The satin grain for the header (made again when the width changes).
    inline juce::Image makeSatin (int width) { return Finish::makeSatinGrain (juce::jmax (1, width * 2), 220, 4242); }

    inline void paint (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::Image& satin, const juce::String& caption)
    {
        g.fillAll (onyx);
        const auto header = headerOf (bounds);
        Pearl::paintUnit (g, header.toFloat().reduced (4.0f), satin);
        auto badge = header.toFloat().reduced (24.0f, 14.0f).withWidth (juce::jmin (520.0f, (float) header.getWidth() - 48.0f));
        Logo::drawMark (g, badge.removeFromTop (badge.getHeight() * 0.66f), juce::Justification::centredLeft,
                        { Finish::ink(), juce::Colour (0xffe0103c), Finish::stampEdge(), {}, 1.0f, false });
        g.setColour (Finish::printedRed());
        g.setFont (ParagonLookAndFeel::textFont (12.0f));
        g.drawText (caption, badge, juce::Justification::topLeft, false);

        // The page: the Hub's panel with the question boxes' red edge.
        const auto pageArea = panelOf (bounds).toFloat();
        g.setColour (panel);
        g.fillRoundedRectangle (pageArea, 6.0f);
        g.setColour (ParagonColours::glow.withAlpha (0.3f));
        g.drawRoundedRectangle (pageArea.reduced (1.5f), 5.0f, 3.0f);
        g.setColour (ParagonColours::glow);
        g.drawRoundedRectangle (pageArea, 6.0f, 1.2f);
    }
}
