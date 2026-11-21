#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/ArrangeView.h"
#include "ui/ParagonLookAndFeel.h"

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
            case T::split:    // (0.81.0, the owner asked) a straight razor: the blade solid, the handle outlined, tilted
            {
                // Drawn level first: the blade on the left (its edge along the bottom, a squared point),
                // the shank and pivot, the handle on the right; then turned so the blade points down-left.
                juce::Path blade;
                blade.startNewSubPath (1.5f, 8.0f);                       // the point, top
                blade.lineTo (14.0f, 8.0f);                               // the spine
                blade.lineTo (15.5f, 10.0f);                              // into the shank
                blade.lineTo (15.5f, 12.0f);
                blade.lineTo (14.0f, 14.5f);
                blade.quadraticTo (8.0f, 13.6f, 2.5f, 15.0f);             // the edge, a touch hollow
                blade.lineTo (1.5f, 13.0f);
                blade.closeSubPath();
                const auto turn = juce::AffineTransform::rotation (-0.6f, 12.0f, 12.0f);
                blade.applyTransform (turn);
                f = blade;
                juce::Path handle;
                handle.addRoundedRectangle (17.0f, 9.3f, 6.0f, 3.4f, 1.6f);
                handle.addEllipse (15.2f, 9.8f, 2.4f, 2.4f);              // the pivot
                handle.applyTransform (turn);
                s = handle;
                break;
            }
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
            case T::line:     // a line between two points (0.27.0)
                s.startNewSubPath (4.0f, 18.0f);
                s.lineTo (20.0f, 6.0f);
                f.addEllipse (2.0f, 16.0f, 4.5f, 4.5f);
                f.addEllipse (18.0f, 4.0f, 4.5f, 4.5f);
                break;
            case T::warp:     // a clock with arrows (the manual's Free Warp pointer, drawn our way)
                s.addEllipse (5.0f, 5.0f, 14.0f, 14.0f);
                s.startNewSubPath (12.0f, 12.0f);
                s.lineTo (12.0f, 8.0f);
                s.startNewSubPath (12.0f, 12.0f);
                s.lineTo (15.0f, 13.5f);
                f.addTriangle (1.0f, 12.0f, 4.0f, 9.0f, 4.0f, 15.0f);
                f.addTriangle (23.0f, 12.0f, 20.0f, 9.0f, 20.0f, 15.0f);
                break;
            case T::comp:     // takes stacked on lanes, a stretch of the middle one brought forward (0.29.0)
                s.startNewSubPath (3.0f, 5.0f);
                s.lineTo (21.0f, 5.0f);
                s.startNewSubPath (3.0f, 19.0f);
                s.lineTo (21.0f, 19.0f);
                s.startNewSubPath (3.0f, 12.0f);
                s.lineTo (7.0f, 12.0f);
                s.startNewSubPath (17.0f, 12.0f);
                s.lineTo (21.0f, 12.0f);
                f.addRoundedRectangle (7.5f, 8.5f, 9.0f, 7.0f, 1.5f);
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
        ParagonColours::setFill (g, colour, box);   // (0.189.0: a lit tool in the family gradient)
        g.fillPath (icon.fill, transform);
        g.strokePath (icon.stroke, juce::PathStrokeType (juce::jmax (1.2f, size / 12.0f), juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded), transform);
    }

    // (0.187.0) SELECT's sizing as a mark in the corner of its 24 x 24 square: two chevrons (the content
    // goes with the edge) for Sizing Moves Contents, a little waveform (the manual's icon for a stretched
    // part) for Sizing Applies Time Stretch; nothing for Normal Sizing.
    inline juce::Path sizingMark (pr7s::Sizing sizing)
    {
        juce::Path s;
        if (sizing == pr7s::Sizing::movesContents)
            for (const auto x : { 14.5f, 18.5f })
            {
                s.startNewSubPath (x, 14.5f);
                s.lineTo (x + 4.0f, 18.5f);
                s.lineTo (x, 22.5f);
            }
        else if (sizing == pr7s::Sizing::timeStretch)
        {
            s.startNewSubPath (13.5f, 19.0f);
            s.lineTo (15.3f, 15.5f);
            s.lineTo (17.1f, 22.5f);
            s.lineTo (18.9f, 14.5f);
            s.lineTo (20.7f, 21.5f);
            s.lineTo (22.5f, 17.5f);
            s.lineTo (23.5f, 19.0f);
        }
        return s;
    }

    // SELECT's icon with its sizing's mark (the pointer drawn smaller, up in the corner, to make room).
    inline void draw (juce::Graphics& g, ArrangeView::Tool tool, juce::Rectangle<float> area, juce::Colour colour, pr7s::Sizing sizing);

    // The tool as the mouse pointer: white with a dark edge, 24 pixels, its
    // hot spot where it acts (the pointer's tip, the pencil's, the lens'
    // middle, the razor's edge...). SELECT: the usual pointer.
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
                           : tool == T::split ? juce::Point<int> (9, 16)   // (the middle of the razor's edge)
                           : tool == T::range ? juce::Point<int> (4, 12)
                                              : juce::Point<int> (12, 12);
        return juce::MouseCursor (image, hotSpot.x, hotSpot.y);   // (the arrangement keeps the ones it makes)
    }

    inline void draw (juce::Graphics& g, ArrangeView::Tool tool, juce::Rectangle<float> area, juce::Colour colour, pr7s::Sizing sizing)
    {
        if (tool != ArrangeView::Tool::select || sizing == pr7s::Sizing::normal)
        {
            draw (g, tool, area, colour);
            return;
        }
        const auto size = juce::jmin (area.getWidth(), area.getHeight());
        const auto box = area.withSizeKeepingCentre (size, size);
        const auto transform = juce::AffineTransform::scale (size / 24.0f).translated (box.getX(), box.getY());
        const auto icon = make (tool);
        ParagonColours::setFill (g, colour, box);   // (0.189.0: a lit tool in the family gradient)
        g.fillPath (icon.fill, juce::AffineTransform::scale (0.78f).followedBy (transform));
        g.strokePath (sizingMark (sizing), juce::PathStrokeType (juce::jmax (1.2f, size / 13.0f), juce::PathStrokeType::curved,
                                                                 juce::PathStrokeType::rounded), transform);
    }

    // (0.187.0) SELECT's pointer while it sizes another way: the arrow (or, over a part's edge, a left-right
    // arrow) with the sizing's mark, white with a dark edge as the other tools' pointers.
    inline juce::MouseCursor sizingCursor (pr7s::Sizing sizing, bool onEdge)
    {
        if (sizing == pr7s::Sizing::normal)
            return onEdge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor;
        juce::Image image (juce::Image::ARGB, 24, 24, true);
        {
            juce::Graphics g (image);
            const auto paint = [&] (juce::Point<float> offset, juce::Colour colour)
            {
                const auto shift = juce::AffineTransform::translation (offset.x, offset.y);
                g.setColour (colour);
                if (onEdge)
                {
                    juce::Path arrow;
                    arrow.startNewSubPath (2.0f, 7.0f);
                    arrow.lineTo (16.0f, 7.0f);
                    arrow.startNewSubPath (5.5f, 3.5f);
                    arrow.lineTo (2.0f, 7.0f);
                    arrow.lineTo (5.5f, 10.5f);
                    arrow.startNewSubPath (12.5f, 3.5f);
                    arrow.lineTo (16.0f, 7.0f);
                    arrow.lineTo (12.5f, 10.5f);
                    g.strokePath (arrow, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), shift);
                }
                else
                    g.fillPath (make (ArrangeView::Tool::select).fill, juce::AffineTransform::translation (-4.0f, -1.5f)
                                                                        .scaled (0.78f).followedBy (shift));
                g.strokePath (sizingMark (sizing), juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), shift);
            };
            for (const auto& offset : { juce::Point<float> (-1.0f, 0.0f), { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } })
                paint (offset, juce::Colour (0xff120c10));
            paint ({}, juce::Colours::white);
        }
        return onEdge ? juce::MouseCursor (image, 9, 7) : juce::MouseCursor (image, 0, 0);
    }
}
