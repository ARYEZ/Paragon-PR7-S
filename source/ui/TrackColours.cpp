#include "ui/TrackColours.h"

#include "ui/ParagonLookAndFeel.h"

TrackColourMenu::TrackColourMenu (pr7s::Studio& s, std::vector<int> ids) : studio (s), trackIds (std::move (ids))
{
    using namespace pr7s;
    defaultKey.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
    defaultKey.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
    defaultKey.setWantsKeyboardFocus (false);
    defaultKey.setTooltip ("Their automatic colour back: red, pink and orange in turn, by their place in the list");
    defaultKey.onClick = [this] { studio.setTracksColour (trackIds, std::nullopt); repaint(); };
    addAndMakeVisible (defaultKey);
    // (0.73.0) The colour in the track list: per track, 1-100 %.
    const auto* first = [this]() -> const Track* { for (const auto id : trackIds) if (const auto* t = studio.getProject().findTrack (id)) return t; return nullptr; }();
    listKey.setToggleState (first != nullptr && first->listColourOn, juce::dontSendNotification);
    listKey.setWantsKeyboardFocus (false);
    listKey.setTooltip ("Shows the track's colour over its row in the track list (only the tracks chosen), as much as the bar says");
    listKey.onClick = [this] { setListColour (listKey.getToggleState(), (int) listAmount.getValue()); };
    addAndMakeVisible (listKey);
    listAmount.setRange (1.0, 100.0, 1.0);
    listAmount.setValue (first != nullptr ? first->listColourAmount : Track::defaultListColourAmount, juce::dontSendNotification);
    listAmount.setTextValueSuffix (" %");
    listAmount.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 22);
    listAmount.setTooltip ("How much of the track's colour the row shows: 1 % a faint tint, 100 % the whole colour");
    listAmount.onValueChange = [this] { setListColour (true, (int) listAmount.getValue()); };
    addAndMakeVisible (listAmount);
    const auto width = margin * 2 + FamilyColours::paletteHues * (cell + gap) - gap;
    const auto height = titleHeight + familyHeight + gap + FamilyColours::paletteShades * (cell + gap) + footerHeight + listHeight;
    setSize (width, height);
}

juce::Colour TrackColourMenu::current() const
{
    for (const auto id : trackIds)
        if (const auto* t = studio.getProject().findTrack (id))
            return t->colour;
    return {};
}

void TrackColourMenu::resized()
{
    using namespace pr7s;
    swatches.clear();
    auto area = getLocalBounds().reduced (margin, 0).toFloat();
    area.removeFromTop ((float) titleHeight);
    // The gradient's three, wide.
    auto family = area.removeFromTop ((float) familyHeight - 6.0f);
    const auto third = (family.getWidth() - 2.0f * gap) / 3.0f;
    for (const auto colour : { FamilyColours::pink, FamilyColours::red, FamilyColours::orange })
    {
        swatches.push_back ({ family.removeFromLeft (third), colour });
        family.removeFromLeft ((float) gap);
    }
    area.removeFromTop (6.0f + gap);
    // The palette: a column per hue, deep at the top to pale at the bottom.
    for (int shade = 0; shade < FamilyColours::paletteShades; ++shade)
        for (int hue = 0; hue < FamilyColours::paletteHues; ++hue)
            swatches.push_back ({ juce::Rectangle<float> (area.getX() + (float) (hue * (cell + gap)), area.getY() + (float) (shade * (cell + gap)),
                                                          (float) cell, (float) cell),
                                  FamilyColours::paletteColour (hue, shade) });
    auto bottom = getLocalBounds();
    auto list = bottom.removeFromBottom (listHeight).reduced (margin, 6);
    defaultKey.setBounds (bottom.removeFromBottom (footerHeight).reduced (margin, 8));
    listKey.setBounds (list.removeFromTop (28));
    list.removeFromTop (4);
    listAmount.setBounds (list.removeFromTop (24));
}

void TrackColourMenu::setListColour (bool on, int amount)
{
    listKey.setToggleState (on, juce::dontSendNotification);
    listAmount.setValue (amount, juce::dontSendNotification);
    studio.setTracksListColour (trackIds, on, amount);
}

void TrackColourMenu::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0a0d));
    g.setColour (ParagonColours::accent);
    g.fillRect (0, 0, getWidth(), 3);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (13.0f));
    g.drawText (trackIds.size() > 1 ? "COLOUR THE SELECTED TRACKS" : "COLOUR THE TRACK", getLocalBounds().reduced (margin, 0).withHeight (titleHeight).withTrimmedTop (4),
                juce::Justification::centredLeft, false);
    const auto now = current();
    for (const auto& [r, colour] : swatches)
    {
        g.setColour (colour);
        g.fillRoundedRectangle (r, 3.0f);
        if (colour == now)
        {
            g.setColour (juce::Colour (0xfff4eeea));
            g.drawRoundedRectangle (r.reduced (1.0f), 3.0f, 2.0f);
        }
        else if (hover && *hover == colour)
        {
            g.setColour (juce::Colour (0xfff4eeea).withAlpha (0.6f));
            g.drawRoundedRectangle (r.reduced (1.0f), 3.0f, 1.0f);
        }
    }
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (margin, getHeight() - footerHeight - listHeight, getWidth() - 2 * margin, 1);
    g.fillRect (margin, getHeight() - listHeight, getWidth() - 2 * margin, 1);
}

std::optional<juce::Colour> TrackColourMenu::colourAt (juce::Point<float> p) const
{
    for (const auto& [r, colour] : swatches)
        if (r.contains (p))
            return colour;
    return std::nullopt;
}

juce::Rectangle<float> TrackColourMenu::swatchFor (juce::Colour colour) const
{
    for (const auto& [r, c] : swatches)
        if (c == colour)
            return r;
    return {};
}

void TrackColourMenu::mouseDown (const juce::MouseEvent& e)
{
    if (const auto colour = colourAt (e.position))
    {
        studio.setTracksColour (trackIds, *colour);
        repaint();
    }
}

void TrackColourMenu::mouseMove (const juce::MouseEvent& e)
{
    const auto now = colourAt (e.position);
    if (now != hover)
    {
        hover = now;
        repaint();
    }
}

void TrackColourMenu::mouseExit (const juce::MouseEvent&)
{
    hover.reset();
    repaint();
}

void TrackColourMenu::show (juce::Component& around, pr7s::Studio& studio, std::vector<int> trackIds)
{
    if (trackIds.empty())
        return;
    auto menu = std::make_unique<TrackColourMenu> (studio, std::move (trackIds));
    juce::CallOutBox::launchAsynchronously (std::move (menu), around.getScreenBounds(), nullptr);
}

void ColourKey::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (juce::Colour (over ? 0xff221b23 : 0xff151217));
    g.fillRoundedRectangle (r, 3.0f);
    // A little palette: the gradient's three over their pale tints.
    auto inner = r.reduced (4.0f, 4.0f);
    const auto w = inner.getWidth() / 3.0f;
    int i = 0;
    for (const auto colour : { pr7s::FamilyColours::pink, pr7s::FamilyColours::red, pr7s::FamilyColours::orange })
    {
        auto column = juce::Rectangle<float> (inner.getX() + w * (float) i++, inner.getY(), w, inner.getHeight()).reduced (0.5f, 0.0f);
        g.setColour (colour);
        g.fillRect (column.removeFromTop (column.getHeight() * 0.6f));
        g.setColour (colour.withSaturation (0.4f).withBrightness (1.0f));
        g.fillRect (column);
    }
    g.setColour (down ? ParagonColours::accent : ParagonColours::panelEdge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}
