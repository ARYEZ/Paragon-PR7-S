#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Finish.h"
#include "ui/ParagonLookAndFeel.h"

// The PR7 family's pearl unit surface, for the studio's units (the
// transport, the keyboard strip): the finish's pearl (white or black) with
// its satin grain, a light catch along the top edge and the family's red
// glow round the base.
namespace Pearl
{
    inline void paintUnit (juce::Graphics& g, juce::Rectangle<float> area, const juce::Image& satin, float radius = 10.0f)
    {
        // The glow on the dark round it, then the unit.
        for (int ring = 3; ring >= 1; --ring)
        {
            g.setColour (ParagonColours::glow.withAlpha (0.05f * (float) (4 - ring)));
            g.fillRoundedRectangle (area.expanded ((float) ring * 2.0f), radius + (float) ring * 2.0f);
        }
        g.setGradientFill (juce::ColourGradient (Finish::pick (0xfff4f0f2, 0xff2d282c), 0.0f, area.getY(),
                                                 Finish::pick (0xffd3cdd0, 0xff131114), 0.0f, area.getBottom(), false));
        g.fillRoundedRectangle (area, radius);
        if (satin.isValid())
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path shape;
            shape.addRoundedRectangle (area, radius);
            g.reduceClipRegion (shape);
            g.drawImage (satin, area);
        }
        g.setColour (juce::Colours::white.withAlpha (Finish::isBlack() ? 0.12f : 0.7f));
        g.drawLine (area.getX() + radius, area.getY() + 1.0f, area.getRight() - radius, area.getY() + 1.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (area.reduced (0.5f), radius, 1.0f);
    }

    // A dark recessed panel, for the studio's work areas (the tracks, the arrangement).
    inline void paintWell (juce::Graphics& g, juce::Rectangle<float> area, float radius = 8.0f)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0e0b0f), 0.0f, area.getY(),
                                                 juce::Colour (0xff09070a), 0.0f, area.getBottom(), false));
        g.fillRoundedRectangle (area, radius);
        g.setColour (ParagonColours::panelEdge);
        g.drawRoundedRectangle (area.reduced (0.5f), radius, 1.0f);
    }
}
