#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/ArrangeView.h"

// The arrangement's tools' icons (as the manual's toolbar has a picture for
// each; ours drawn in the family's look): on their keys, and as the mouse
// pointer over the tracks, so the tool in use shows at a glance.
namespace ToolIcons
{
    // Each drawn in a 24 x 24 square: `fill` for solid shapes, `stroke` for lines.
    struct Icon { juce::Path fill, stroke; };

    inline Icon make (ArrangeView::Tool tool)
    {
        using T = ArrangeView::Tool;
        Icon icon;
        auto& f = icon.fill;
        auto& s = icon.stroke;
        switch (tool)
        {
            case T::select:   // an arrow pointer
                f.startNewSubPath (5.0f, 2.0f);
                f.lineTo (5.0f, 19.0f);
                f.lineTo (9.2f, 15.2f);
                f.lineTo (12.2f, 21.5f);
                f.lineTo (15.0f, 20.2f);
                f.lineTo (12.0f, 14.0f);
                f.lineTo (17.5f, 13.6f);
                f.closeSubPath();
                break;
            case T::range:    // a span between two brackets
                for (const auto x : { 4.0f, 20.0f })
                {
                    s.startNewSubPath (x, 4.0f);
                    s.lineTo (x, 20.0f);
                    s.startNewSubPath (x + (x < 12.0f ? 3.5f : -3.5f), 4.0f);
                    s.lineTo (x, 4.0f);
                    s.startNewSubPath (x + (x < 12.0f ? 3.5f : -3.5f), 20.0f);
                    s.lineTo (x, 20.0f);
                }
                f.addRectangle (7.0f, 9.0f, 10.0f, 6.0f);
                break;
            case T::split:    // scissors
                s.addEllipse (3.0f, 14.0f, 6.0f, 6.0f);
                s.addEllipse (15.0f, 14.0f, 6.0f, 6.0f);
                s.startNewSubPath (8.0f, 14.5f);
                s.lineTo (17.5f, 2.5f);
                s.startNewSubPath (16.0f, 14.5f);
                s.lineTo (6.5f, 2.5f);
                break;
            case T::glue:     // a tube of glue, a drop from its nozzle
                f.addRoundedRectangle (3.0f, 10.0f, 13.0f, 8.0f, 2.0f);
                f.addTriangle (16.0f, 11.0f, 16.0f, 17.0f, 20.0f, 14.0f);
                f.addEllipse (19.5f, 18.0f, 3.0f, 4.0f);
                s.startNewSubPath (3.0f, 9.0f);
                s.lineTo (3.0f, 19.0f);
                break;
            case T::erase:    // an eraser, tilted
            {
                juce::Path body;
                body.addRoundedRectangle (4.0f, 8.0f, 16.0f, 8.0f, 1.5f);
                body.applyTransform (juce::AffineTransform::rotation (-0.6f, 12.0f, 12.0f));
                s = body;
                juce::Path tip;
                tip.addRoundedRectangle (4.0f, 8.0f, 6.0f, 8.0f, 1.5f);
                tip.applyTransform (juce::AffineTransform::rotation (-0.6f, 12.0f, 12.0f));
                f = tip;
                s.startNewSubPath (3.0f, 21.0f);
                s.lineTo (13.0f, 21.0f);
                break;
            }
            case T::zoom:     // a magnifier
                s.addEllipse (3.0f, 3.0f, 12.0f, 12.0f);
                f.startNewSubPath (13.5f, 15.5f);
                f.lineTo (15.5f, 13.5f);
                f.lineTo (21.5f, 19.5f);
                f.lineTo (19.5f, 21.5f);
                f.closeSubPath();
                s.startNewSubPath (6.5f, 9.0f);
                s.lineTo (11.5f, 9.0f);
                s.startNewSubPath (9.0f, 6.5f);
                s.lineTo (9.0f, 11.5f);
                break;
            case T::mute:     // an X
                s.startNewSubPath (5.0f, 5.0f);
                s.lineTo (19.0f, 19.0f);
                s.startNewSubPath (19.0f, 5.0f);
                s.lineTo (5.0f, 19.0f);
                break;
            case T::draw:     // a pencil (its tip at the lower left)
                f.startNewSubPath (3.0f, 21.0f);
                f.lineTo (4.5f, 15.5f);
                f.lineTo (16.5f, 3.5f);
                f.lineTo (20.5f, 7.5f);
                f.lineTo (8.5f, 19.5f);
                f.closeSubPath();
                break;
            case T::play:     // a speaker, a wave from it
                f.startNewSubPath (3.0f, 9.0f);
                f.lineTo (7.0f, 9.0f);
                f.lineTo (12.0f, 4.5f);
                f.lineTo (12.0f, 19.5f);
                f.lineTo (7.0f, 15.0f);
                f.lineTo (3.0f, 15.0f);
                f.closeSubPath();
                s.addCentredArc (12.0f, 12.0f, 5.5f, 5.5f, 0.0f, 0.6f, 2.55f, true);
                s.addCentredArc (12.0f, 12.0f, 9.0f, 9.0f, 0.0f, 0.6f, 2.55f, true);
                break;
        }
        return icon;
    }

    // An icon drawn in `area` (kept square, centred) in a colour.
    inline void draw (juce::Graphics& g, ArrangeView::Tool tool, juce::Rectangle<float> area, juce::Colour colour)
    {
        const auto size = juce::jmin (area.getWidth(), area.getHeight());
        const auto box = area.withSizeKeepingCentre (size, size);
        const auto transform = juce::AffineTransform::scale (size / 24.0f).translated (box.getX(), box.getY());
        const auto icon = make (tool);
        g.setColour (colour);
        g.fillPath (icon.fill, transform);
        g.strokePath (icon.stroke, juce::PathStrokeType (juce::jmax (1.2f, size / 12.0f), juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded), transform);
    }

    // The tool as the mouse pointer: white with a dark edge, 24 pixels, its
    // hot spot where it acts (the pointer's tip, the pencil's, the lens'
    // middle, the scissors' crossing...). SELECT: the usual pointer.
    inline juce::MouseCursor cursor (ArrangeView::Tool tool)
    {
        using T = ArrangeView::Tool;
        if (tool == T::select)
            return juce::MouseCursor::NormalCursor;
        juce::Image image (juce::Image::ARGB, 24, 24, true);
        {
            juce::Graphics g (image);
            for (const auto& offset : { juce::Point<float> (-1.0f, 0.0f), { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } })
                draw (g, tool, juce::Rectangle<float> (0.0f, 0.0f, 24.0f, 24.0f).translated (offset.x, offset.y), juce::Colour (0xff120c10));
            draw (g, tool, { 0.0f, 0.0f, 24.0f, 24.0f }, juce::Colours::white);
        }
        const auto hotSpot = tool == T::draw ? juce::Point<int> (3, 21)
                           : tool == T::zoom ? juce::Point<int> (9, 9)
                           : tool == T::split ? juce::Point<int> (12, 9)
                           : tool == T::range ? juce::Point<int> (4, 12)
                                              : juce::Point<int> (12, 12);
        return juce::MouseCursor (image, hotSpot.x, hotSpot.y);   // (the arrangement keeps the ones it makes)
    }
}
