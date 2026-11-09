#include "ui/BoosterView.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    constexpr float marks[] = { 10.0f, 5.0f, 0.0f, -5.0f, -10.0f, -20.0f, -30.0f, -40.0f, -60.0f };
    constexpr int numMarks = (int) std::size (marks);
    juce::FontOptions font (float h) { return ParagonLookAndFeel::textFont (h); }
}

float BoosterView::faderProportion (float db)
{
    db = juce::jlimit (marks[numMarks - 1], marks[0], db);
    for (int i = numMarks - 1; i > 0; --i)
    {
        const auto low = marks[i], high = marks[i - 1];
        if (db <= high)
            return ((float) (numMarks - 1 - i) + (db - low) / (high - low)) / (float) (numMarks - 1);
    }
    return 1.0f;
}

float BoosterView::faderDb (float proportion)
{
    const auto position = juce::jlimit (0.0f, 1.0f, proportion) * (float) (numMarks - 1);
    const auto below = juce::jmin ((int) position, numMarks - 2);
    const auto low = marks[numMarks - 1 - below], high = marks[numMarks - 2 - below];
    return std::round ((low + (high - low) * (position - (float) below)) * 10.0f) / 10.0f;
}

juce::String BoosterView::dbText (float db, bool isTrim)
{
    if (! isTrim && db <= pr7s::Booster::leastLevel)
        return "-INF DB";
    return (db > 0.0f ? "+" : "") + juce::String (db, 1) + " DB";
}

BoosterView::BoosterView (pr7s::Studio& s) : studio (s)
{
    trim.setRange (-pr7s::Booster::trimRange, pr7s::Booster::trimRange, 0.1);
    trim.setDoubleClickReturnValue (true, 0.0);
    trim.setTooltip ("TRIM: fine level, -12 to +12 dB (as PR7-C's OUTPUT TRIM). Double-click for 0 dB.");
    fader.setRange (0.0, 1.0);
    fader.getProperties().set (ParagonLookAndFeel::channelFaderProperty, true);
    fader.setDoubleClickReturnValue (true, faderProportion (0.0f));
    fader.setVelocityModeParameters (0.25, 1, 0.0, true, juce::ModifierKeys::shiftModifier);   // Shift-drag: fine
    fader.setTooltip ("The booster's level: +10 dB down to silence at the bottom, after the EQ and before the sends and fader. "
                      "Double-click for 0 dB; Shift-drag for fine moves.");
    for (auto* slider : { &trim, &fader })
    {
        slider->setWantsKeyboardFocus (false);
        slider->onDragStart = [this] { dragging = true; };
        slider->onValueChange = [this] { send (! dragging); repaint(); };
        slider->onDragEnd = [this] { dragging = false; send (true); };
        addAndMakeVisible (*slider);
    }
    refresh();
    startTimerHz (10);
}

void BoosterView::setBus (int busId)
{
    bus = busId;
    refresh();
}

pr7s::Booster BoosterView::current() const
{
    return { (float) trim.getValue(), faderDb ((float) fader.getValue()) };
}

void BoosterView::send (bool undoable)
{
    if (studio.getBooster (bus) != nullptr)
        studio.setBooster (bus, current(), undoable);
}

void BoosterView::refresh()
{
    if (dragging)
        return;
    const auto* boost = studio.getBooster (bus);
    const auto shown = boost != nullptr ? *boost : pr7s::Booster {};
    if (! (current() == shown))
    {
        trim.setValue (shown.trimDb, juce::dontSendNotification);
        fader.setValue (faderProportion (shown.levelDb), juce::dontSendNotification);
        repaint();
    }
    setEnabled (boost != nullptr);
}

int BoosterView::getPreferredHeight (int width) const
{
    juce::ignoreUnused (width);
    return 210;
}

void BoosterView::resized()
{
    auto r = getLocalBounds();
    auto left = r.removeFromLeft (r.getWidth() / 2);
    // The knob: its scale over it, TRIM and the value under it.
    auto knobSide = juce::jmin (left.getWidth() - 8, 76);
    trimArea = juce::Rectangle<int> (knobSide, knobSide).withCentre ({ left.getCentreX(), left.getY() + 18 + knobSide / 2 });
    trim.setBounds (trimArea);
    trimReadout = juce::Rectangle<int> (left.getX(), trimArea.getBottom() + 20, left.getWidth(), 18);
    // The fader: its scale left of the slot, the value under it.
    faderReadout = r.removeFromBottom (18);
    r.removeFromBottom (4);
    scaleArea = r.removeFromLeft (28);
    faderArea = r.reduced (0, 12);
    const auto radius = ParagonLookAndFeel::channelFaderThumbRadius;
    fader.setBounds (faderArea.getX(), faderArea.getY() - radius, faderArea.getWidth(), faderArea.getHeight() + radius * 2);
}

void BoosterView::paint (juce::Graphics& g)
{
    const auto dim = isEnabled() ? 1.0f : 0.4f;
    // The knob's -12 0 +12 and its name.
    g.setFont (font (11.0f));
    g.setColour (ParagonColours::textDim.withMultipliedAlpha (dim));
    const auto c = trimArea.getCentre().toFloat();
    const auto rr = (float) trimArea.getWidth() * 0.5f;
    g.drawText ("-12", juce::Rectangle<float> (30.0f, 12.0f).withCentre ({ c.x - rr * 0.78f, c.y + rr * 0.92f }), juce::Justification::centred);
    g.drawText ("+12", juce::Rectangle<float> (30.0f, 12.0f).withCentre ({ c.x + rr * 0.78f, c.y + rr * 0.92f }), juce::Justification::centred);
    g.drawText ("0", juce::Rectangle<float> (20.0f, 12.0f).withCentre ({ c.x, (float) trimArea.getY() - 8.0f }), juce::Justification::centred);
    g.setColour (ParagonColours::text.withMultipliedAlpha (dim));
    g.setFont (font (13.0f));
    g.drawText ("TRIM", trimReadout.translated (0, -16), juce::Justification::centred);
    g.setColour (ParagonColours::accent.withMultipliedAlpha (dim));
    g.drawText (dbText ((float) trim.getValue(), true), trimReadout, juce::Justification::centred);
    g.drawText (dbText (faderDb ((float) fader.getValue()), false), faderReadout, juce::Justification::centred);

    // PR7-C's fader scale: a long line at each mark, short ones between.
    const auto top = (float) faderArea.getY(), bottom = (float) faderArea.getBottom();
    const auto lineLeft = (float) scaleArea.getRight() - 2.0f, lineWidth = (float) faderArea.getWidth() + 4.0f;
    for (int i = 0; i < numMarks; ++i)
    {
        const auto y = bottom - faderProportion (marks[i]) * (bottom - top);
        g.setColour (ParagonColours::text.withAlpha (0.55f * dim));
        g.fillRect (juce::Rectangle<float> (lineLeft, y - 0.6f, lineWidth, 1.2f));
        const auto label = i + 1 == numMarks ? juce::String (juce::CharPointer_UTF8 ("-\xe2\x88\x9e")) : juce::String (juce::roundToInt (marks[i]));
        g.setColour (ParagonColours::textDim.withMultipliedAlpha (dim));
        g.setFont (i + 1 == numMarks ? ParagonLookAndFeel::lowercaseFont (12.0f) : font (11.0f));
        g.drawText (label, juce::Rectangle<float> ((float) scaleArea.getX(), y - 7.0f, (float) scaleArea.getWidth() - 4.0f, 14.0f), juce::Justification::centredRight);
        if (i + 1 < numMarks)
        {
            const auto nextY = bottom - faderProportion (marks[i + 1]) * (bottom - top);
            g.setColour (ParagonColours::text.withAlpha (0.22f * dim));
            for (int step = 1; step < 5; ++step)
                g.fillRect (juce::Rectangle<float> (lineLeft + 6.0f, y + (nextY - y) * (float) step / 5.0f - 0.4f, lineWidth - 12.0f, 0.8f));
        }
    }
}
