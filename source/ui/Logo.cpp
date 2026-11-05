#include "Logo.h"

#include "ui/Finish.h"
#include "ui/ParagonLookAndFeel.h"

namespace Logo
{
    namespace
    {
        // The letters are designed 1 unit tall, upright, then slanted. Stems
        // are 0.24 wide and bars 0.2 tall: chunky, for a bold silhouette.
        constexpr float slant = 0.3f;     // how far the top leans right, per unit of height
        constexpr float spacing = 0.1f;
        constexpr float letterWidths[] = { 0.8f, 0.84f, 0.8f, 0.4f, 0.8f };   // P, R, 7, dash, S
        constexpr float totalWidth = 0.8f + 0.84f + 0.8f + 0.4f + 0.8f + 4.0f * spacing + slant;

        // The two speed cuts, as fractions of the height from the top.
        constexpr std::pair<float, float> cuts[] = { { 0.63f, 0.68f }, { 0.79f, 0.83f } };

        void addPolygon (juce::Path& path, std::initializer_list<juce::Point<float>> points, float x)
        {
            auto first = true;
            for (auto point : points)
            {
                point.x += x;
                if (first)
                    path.startNewSubPath (point);
                else
                    path.lineTo (point);
                first = false;
            }
            path.closeSubPath();
        }

        // P's body (R shares it): bevelled corners on the bowl, and its counter.
        void addBowl (juce::Path& path, float x)
        {
            addPolygon (path, { { 0.0f, 0.0f }, { 0.64f, 0.0f }, { 0.8f, 0.15f }, { 0.8f, 0.45f }, { 0.64f, 0.6f },
                                { 0.24f, 0.6f }, { 0.24f, 1.0f }, { 0.0f, 1.0f } }, x);
            addPolygon (path, { { 0.24f, 0.2f }, { 0.56f, 0.2f }, { 0.56f, 0.4f }, { 0.24f, 0.4f } }, x);
        }

        // The letters, 1 unit tall, upright: [0] is P, R and "-S", [1] is the 7.
        std::pair<juce::Path, juce::Path> letterPaths()
        {
            juce::Path pr, seven;
            pr.setUsingNonZeroWinding (false);   // so the bowls' counters are holes

            auto x = 0.0f;
            addBowl (pr, x);
            x += letterWidths[0] + spacing;

            addBowl (pr, x);
            addPolygon (pr, { { 0.4f, 0.6f }, { 0.64f, 0.6f }, { 0.84f, 1.0f }, { 0.6f, 1.0f } }, x);   // R's leg
            x += letterWidths[1] + spacing;

            addPolygon (seven, { { 0.0f, 0.0f }, { 0.8f, 0.0f }, { 0.8f, 0.2f }, { 0.44f, 1.0f }, { 0.18f, 1.0f },
                                 { 0.5f, 0.2f }, { 0.0f, 0.2f } }, x);
            x += letterWidths[2] + spacing;

            // "-S" (the studio): a dash at mid height, clear of the speed
            // cuts, and an S with the same bevelled corners as the P and R.
            addPolygon (pr, { { 0.0f, 0.42f }, { 0.4f, 0.42f }, { 0.4f, 0.6f }, { 0.0f, 0.6f } }, x);
            x += letterWidths[3] + spacing;

            addPolygon (pr, { { 0.16f, 0.0f }, { 0.8f, 0.0f }, { 0.8f, 0.2f }, { 0.24f, 0.2f }, { 0.24f, 0.4f },
                              { 0.64f, 0.4f }, { 0.8f, 0.55f }, { 0.8f, 0.85f }, { 0.64f, 1.0f }, { 0.0f, 1.0f },
                              { 0.0f, 0.8f }, { 0.56f, 0.8f }, { 0.56f, 0.6f }, { 0.16f, 0.6f }, { 0.0f, 0.45f }, { 0.0f, 0.15f } }, x);

            // Lean them forward: the top moves right, the baseline stays put.
            const auto lean = juce::AffineTransform (1.0f, -slant, slant, 0.0f, 1.0f, 0.0f);
            pr.applyTransform (lean);
            seven.applyTransform (lean);
            return { pr, seven };
        }
    }

    float widthFor (float height) noexcept
    {
        return height * totalWidth;
    }

    juce::Rectangle<float> drawMark (juce::Graphics& g, juce::Rectangle<float> area, juce::Justification justification, const Style& style)
    {
        const auto height = juce::jmin (area.getHeight(), area.getWidth() / totalWidth);
        const auto bounds = justification.appliedToRectangle (juce::Rectangle<float> (widthFor (height), height), area);

        static const auto letters = letterPaths();
        const auto place = juce::AffineTransform::scale (height).translated (bounds.getX(), bounds.getY());
        auto pr = letters.first, seven = letters.second;
        pr.applyTransform (place);
        seven.applyTransform (place);

        if (style.glow)
        {
            // A soft neon halo behind the 7, wider and fainter outwards.
            for (int ring = 3; ring >= 1; --ring)
            {
                g.setColour (style.seven.withAlpha (0.1f * (float) (4 - ring)));
                g.strokePath (seven, juce::PathStrokeType (height * 0.07f * (float) ring, juce::PathStrokeType::curved));
            }
        }

        juce::Graphics::ScopedSaveState saved (g);
        for (const auto& [top, bottom] : cuts)
            g.excludeClipRegion (juce::Rectangle<float> (bounds.getX() - height, bounds.getY() + height * top,
                                                         bounds.getWidth() + 2.0f * height, height * (bottom - top))
                                     .getSmallestIntegerContainer());

        if (! style.highlight.isTransparent())
        {
            const auto down = juce::AffineTransform::translation (0.0f, juce::jmax (1.0f, height * 0.04f));
            g.setColour (style.highlight);
            g.fillPath (pr, down);
            g.fillPath (seven, down);
        }

        g.setColour (style.letters);
        g.fillPath (pr);
        g.setColour (style.seven);
        g.fillPath (seven);

        if (! style.outline.isTransparent())
        {
            g.setColour (style.outline);
            const juce::PathStrokeType stroke (style.outlineWidth, juce::PathStrokeType::mitered);
            g.strokePath (pr, stroke);
            g.strokePath (seven, stroke);
        }
        return bounds;
    }

    void drawAppIcon (juce::Graphics& g, juce::Rectangle<float> area, bool withName)
    {
        const auto size = juce::jmin (area.getWidth(), area.getHeight());
        const auto tile = area.withSizeKeepingCentre (size, size).reduced (size * 0.02f);
        const auto corner = size * 0.2f;

        // The tile: pearl onyx, a touch lighter at the top, the family's red glow round its edge.
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d171c), tile.getCentreX(), tile.getY(),
                                                 juce::Colour (0xff070508), tile.getCentreX(), tile.getBottom(), false));
        g.fillRoundedRectangle (tile, corner);
        if (size >= 24.0f)
        {
            g.setColour (ParagonColours::glow.withAlpha (0.28f));
            g.drawRoundedRectangle (tile.reduced (size * 0.025f), corner * 0.9f, size * 0.035f);
        }
        g.setColour (ParagonColours::glow);
        g.drawRoundedRectangle (tile.reduced (size * 0.008f), corner, juce::jmax (1.0f, size * 0.012f));

        // The monogram: the mark's 7 and S, side by side, 1 unit tall, slanted as the mark.
        juce::Path seven, ess;
        ess.setUsingNonZeroWinding (false);
        addPolygon (seven, { { 0.0f, 0.0f }, { 0.8f, 0.0f }, { 0.8f, 0.2f }, { 0.44f, 1.0f }, { 0.18f, 1.0f },
                             { 0.5f, 0.2f }, { 0.0f, 0.2f } }, 0.0f);
        addPolygon (ess, { { 0.16f, 0.0f }, { 0.8f, 0.0f }, { 0.8f, 0.2f }, { 0.24f, 0.2f }, { 0.24f, 0.4f },
                           { 0.64f, 0.4f }, { 0.8f, 0.55f }, { 0.8f, 0.85f }, { 0.64f, 1.0f }, { 0.0f, 1.0f },
                           { 0.0f, 0.8f }, { 0.56f, 0.8f }, { 0.56f, 0.6f }, { 0.16f, 0.6f }, { 0.0f, 0.45f }, { 0.0f, 0.15f } },
                    0.8f + spacing * 1.5f);
        const auto lean = juce::AffineTransform (1.0f, -slant, slant, 0.0f, 1.0f, 0.0f);
        seven.applyTransform (lean);
        ess.applyTransform (lean);
        auto inner = tile.reduced (size * 0.13f);
        auto nameArea = withName ? inner.removeFromBottom (inner.getHeight() * 0.24f) : juce::Rectangle<float>();

        // Fitted by the letters' own outline (the slant included), centred in what's left.
        juce::Path both (seven);
        both.addPath (ess);
        const auto shape = both.getBounds();
        const auto fit = juce::jmin (inner.getWidth() * 0.9f / shape.getWidth(), inner.getHeight() * (withName ? 0.86f : 0.78f) / shape.getHeight());
        const auto place = juce::AffineTransform::translation (-shape.getCentreX(), -shape.getCentreY())
                               .scaled (fit).translated (inner.getCentreX(), inner.getCentreY());
        seven.applyTransform (place);
        ess.applyTransform (place);
        const auto box = seven.getBounds().getUnion (ess.getBounds());
        const auto height = box.getHeight();

        // The letters (crisp: no halo), the speed cuts through them when big enough to see. The 7 in the
        // family's colours, pink at the top to orange at its foot; the S pearl white.
        {
            juce::Graphics::ScopedSaveState saved (g);
            if (size >= 48.0f)
                for (const auto& [top, bottom] : cuts)
                    g.excludeClipRegion (juce::Rectangle<float> (tile.getX(), box.getY() + height * top, tile.getWidth(), height * (bottom - top))
                                             .getSmallestIntegerContainer());
            const auto sevenBox = seven.getBounds();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffde0639), sevenBox.getCentreX(), sevenBox.getY(),
                                                     juce::Colour (0xffcd6619), sevenBox.getCentreX(), sevenBox.getBottom(), false));
            g.fillPath (seven);
            g.setColour (juce::Colour (0xfff4eeea));   // pearl white
            g.fillPath (ess);
        }

        // The whole mark under it, and the family's neon stripe between.
        if (withName)
        {
            const auto stripe = nameArea.removeFromTop (nameArea.getHeight() * 0.22f).withSizeKeepingCentre (inner.getWidth() * 0.82f, juce::jmax (1.0f, size * 0.012f));
            g.setGradientFill (Finish::neonGradient (stripe.getX(), stripe.getRight()));
            g.fillRect (stripe);
            drawMark (g, nameArea.reduced (0.0f, nameArea.getHeight() * 0.08f), juce::Justification::centred,
                      { juce::Colour (0xfff4eeea), juce::Colour (0xffe0103c), {}, {}, 1.0f, false });
        }
    }

    juce::Image makeAppIcon (int size, bool withName)
    {
        juce::Image image (juce::Image::ARGB, size, size, true, juce::SoftwareImageType());
        juce::Graphics g (image);
        drawAppIcon (g, image.getBounds().toFloat(), withName);
        return image;
    }
}
