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

// (0.52.1, the owner asked: the export window twice the size) A page shown bigger, owning it,
// and following its size: the page sets its own size (as its lists grow) and this frame takes
// that size times the scale, so a dialog around it grows too.
class ScaledPage final : public juce::Component,
                         private juce::ComponentListener
{
public:
    ScaledPage (std::unique_ptr<juce::Component> page, float scaleToUse)
        : content (std::move (page)), scale (juce::jlimit (0.5f, 4.0f, scaleToUse))
    {
        addAndMakeVisible (*content);
        content->addComponentListener (this);
        follow();
    }
    ~ScaledPage() override { content->removeComponentListener (this); }

    juce::Component& getPage() noexcept { return *content; }
    float getScale() const noexcept { return scale; }

    // Twice the size, unless the screen holding `around` can't take `tallest` (the page's
    // largest height) and `widest` that big: then as big as fits (never under 1).
    static float scaleFor (const juce::Component& around, int widest, int tallest)
    {
        const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (around.getScreenBounds());
        if (display == nullptr)
            return 2.0f;
        const auto area = display->userArea;
        const auto fits = juce::jmin ((float) (area.getHeight() - 60) / (float) juce::jmax (1, tallest),
                                      (float) (area.getWidth() - 40) / (float) juce::jmax (1, widest));
        return juce::jlimit (1.0f, 2.0f, fits);
    }

    void resized() override
    {
        const juce::ScopedValueSetter<bool> laying (layingOut, true);
        content->setTransform (juce::AffineTransform::scale (scale));
        content->setBounds (0, 0, juce::roundToInt ((float) getWidth() / scale), juce::roundToInt ((float) getHeight() / scale));
    }

private:
    void componentMovedOrResized (juce::Component&, bool, bool wasResized) override
    {
        if (wasResized && ! layingOut)
            follow();
    }
    void follow()
    {
        setSize (juce::roundToInt ((float) content->getWidth() * scale), juce::roundToInt ((float) content->getHeight() * scale));
    }

    std::unique_ptr<juce::Component> content;
    float scale = 1.0f;
    bool layingOut = false;
};
