#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Colours taken from the mockup: near-black panels with an
// orange accent.
namespace ParagonColours
{
    inline const juce::Colour background { 0xff060407 };   // measured off the mockup: near black, faintly purple
    inline const juce::Colour panel      { 0xff0c0a0d };
    inline const juce::Colour panelEdge  { 0xff27222a };
    inline const juce::Colour accent     { 0xffff7a1a };
    inline const juce::Colour accentDim  { 0xff5a2a0a };
    inline const juce::Colour meterRed   { 0xffff3b2f };
    // The meters' colour from the bottom (0) to the top (1): orange to
    // pink-red, as on PR7-C's meters.
    inline juce::Colour meterColour (float t)
    {
        return juce::Colour (0xffff7a1a).interpolatedWith (juce::Colour (0xffff1f5a), juce::jlimit (0.0f, 1.0f, t));
    }
    inline const juce::Colour text       { 0xffe8e8ea };
    inline const juce::Colour textDim    { 0xff8a8a92 };

    // The silver pedal unit, and dark "ink" for text printed on it.
    inline const juce::Colour metalLight { 0xfff1edef };
    inline const juce::Colour metalMid   { 0xffd5cfd2 };
    inline const juce::Colour metalDark  { 0xffa59ea2 };
    inline const juce::Colour ink        { 0xff26252a };
    inline const juce::Colour inkDim     { 0xff6b6569 };
    inline const juce::Colour glow       { 0xffff4a3a };   // the red-orange edge light

    // (PR7-G's signal-chain block colours aren't here: PR7-S keeps to the
    // family's red, pink and orange, pearl white and pearl onyx.)
}

class ParagonLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ParagonLookAndFeel();

    // Set this property on a slider to draw it as one of the big black
    // knobs on the pedal face (RIG LEVEL, PRESENCE) instead of the
    // small ringed knob.
    static constexpr const char* pedalKnobProperty = "pedalKnob";

    // Set this on a TextButton to draw it as one of the pedal's small black
    // buttons with an LED stripe (SAVE, BYPASS...). The stripe lights when
    // the button's toggle state is on.
    static constexpr const char* pedalButtonProperty = "pedalButton";
    // On a pedal key: the colour its lettering lights in (an ARGB int), instead of the orange.
    static constexpr const char* litColourProperty = "litColour";

    // Set this on a TextButton to draw it as the mockup's light outlined
    // buttons printed on the metal (VOLUME, WAH / FILTER, MORPH / FX). Its
    // outline and text turn red when its toggle state is on.
    static constexpr const char* outlinedButtonProperty = "outlinedButton";

    // Set this (to a font size) on a TextButton to draw just its text, no
    // box: the header's rig name and tempo, which sit in framed groups.
    static constexpr const char* plainTextProperty = "plainText";

    // Set this on a vertical slider to draw it as a graphic-EQ fader: a thin
    // groove lit from its centre (0 dB) to the cap, and a small black cap.
    static constexpr const char* eqFaderProperty = "eqFader";
    // With eqFader: lit from the bottom to the cap (a volume fader), not from the centre.
    static constexpr const char* faderFromBottomProperty = "faderFromBottom";
    static constexpr int eqFaderThumbRadius = 7;

    // Every font in the plugin is Inter, built in: semibold for bold text,
    // and the heavy display italic for bold italic (the PARAGON badge).
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    // Which text is in Bebas Neue (capitals only, tall and narrow): none
    // (all Inter, as the other PR7s), the titles and buttons, or everything
    // the studio draws (not the LED screens or tooltips). The owner chose
    // everything (0.3.1); the others stay for comparing renders.
    enum class DisplayFont { inter, bebasTitles, bebasEverywhere };
    static void setDisplayFont (DisplayFont);
    static DisplayFont getDisplayFont();
    // The fonts to draw with: `textFont` for ordinary text, `titleFont` for
    // titles and buttons. Bebas is drawn larger (bebasScale) so its narrow
    // capitals look the size Inter's text does.
    static juce::FontOptions textFont (float height, int style = juce::Font::bold);
    static juce::FontOptions titleFont (float height);
    static constexpr float bebasScale = 1.3f;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    // Toggle buttons are drawn as the mockup's red/orange status LEDs.
    // EQ faders: travel inset by half the cap's height, so the cap's centre
    // reaches exactly the top and bottom of the scale.
    int getSliderThumbRadius (juce::Slider&) override;
    // Drop-down lists: a long choice ("Multi-Tap", "1/8 triplet") squeezes to
    // fit a narrow box rather than being cut off with "...".
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    // The two-thumb MIN / MAX slider: a thin track with round glowing thumbs.
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& background,
                               bool highlighted, bool down) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

private:
    juce::Typeface::Ptr regular, semiBold, blackItalic, bebas;

    static void drawPedalKnob (juce::Graphics&, juce::Rectangle<float> bounds, float angle);

public:
    // The mockup's piano-black keys (footswitches, slot blocks, small
    // buttons): a glossy black top set in chamfered edges, the top bevel
    // catching the light, the side bevels reflecting the red glow, and a
    // soft shadow on the metal. `bevel` is the chamfer's width. `neon`
    // draws it pink to red to orange (the pearl black unit's keys).
    static void drawPianoKey (juce::Graphics&, juce::Rectangle<float> area, float radius, float bevel,
                              bool pressed, bool highlighted, bool neon = false);

    // The mockup's LED bars: when lit, a neon tube (white-hot core inside a
    // red-orange outline, with a soft glow); when off, a dimmer solid red.
    static void drawNeonBar (juce::Graphics&, juce::Rectangle<float> bar, bool lit);
    // A raised chrome bezel on the metal with a dark well inside it, the way a
    // real unit's footswitches and big keys are mounted. Returns the well,
    // for the key to be drawn in.
    static juce::Rectangle<float> drawChromeBezel (juce::Graphics&, juce::Rectangle<float> bezel, float radius);
    // Text stamped into the metal, with a fine edge underneath (a light
    // catch on pearl white, a shadow on pearl black: Finish::stampEdge).
    static void drawStampedText (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                                 juce::Justification, juce::Colour colour);
};

// JUCE finds fonts through the *default* look-and-feel, so while any plugin
// window is open ours is made the default. Shared by every open window, and
// the default is restored when the last one closes. (The default is per
// plugin binary, so other plugins in the DAW aren't affected.)
struct SharedParagonLookAndFeel
{
    SharedParagonLookAndFeel();
    ~SharedParagonLookAndFeel();

    ParagonLookAndFeel lookAndFeel;
};
