#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// (0.174.0) A part of a component's drawing kept as a picture at the context's pixel scale and drawn back
// where it was made (as Pearl's unit cache): for drawings that stay the same while what's around them moves.
// The picture is transparent where nothing was drawn, so it can sit over or under other drawing (drawing it
// over something gives what drawing its parts there would, but for rounding).
struct PictureCache
{
    juce::Image image;
    juce::Rectangle<float> area;
    float scale = 0.0f;
    int made = 0;   // (how many times it was drawn afresh: tests)

    void invalidate() noexcept { image = {}; }

    template <typename Draw>
    void draw (juce::Graphics& g, juce::Rectangle<float> wanted, bool stale, Draw&& paint)
    {
        const auto s = g.getInternalContext().getPhysicalPixelScaleFactor();
        wanted = wanted.getSmallestIntegerContainer().toFloat();
        if (stale || ! image.isValid() || area != wanted || scale != s)
        {
            const auto w = juce::jmax (1, juce::roundToInt (wanted.getWidth() * s)), h = juce::jmax (1, juce::roundToInt (wanted.getHeight() * s));
            image = juce::Image (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
            juce::Graphics into (image);
            into.addTransform (juce::AffineTransform::translation (-wanted.getX(), -wanted.getY()).scaled (s));
            paint (into);
            area = wanted;
            scale = s;
            ++made;
        }
        g.drawImageTransformed (image, juce::AffineTransform::scale (1.0f / scale).translated (area.getX(), area.getY()));
    }
};
