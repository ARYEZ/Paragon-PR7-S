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
    // (0.79.0) COLOUR IN ORDER.
    orderKey.setWantsKeyboardFocus (false);
    orderKey.setTooltip ("Colours tracks by their number: track 1 takes colour 1 (the numbers on the swatches), track 2 colour 2 and so on, "
                         "kept so as tracks move, come and go. While it's on it wins over a colour picked by hand.");
    orderKey.onClick = [this] { orderChanged(); };
    addAndMakeVisible (orderKey);
    for (auto* bar : { &orderFrom, &orderTo })
    {
        bar->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 130, 22);
        bar->onValueChange = [this] { orderChanged(); };
        addAndMakeVisible (*bar);
    }
    orderFrom.textFromValueFunction = [] (double v) { return "FROM TRACK " + juce::String ((int) v); };
    orderTo.textFromValueFunction = [this] (double v) { return (int) v >= lastNumber() ? juce::String ("TO THE LAST") : "TO TRACK " + juce::String ((int) v); };
    orderFrom.valueFromTextFunction = orderTo.valueFromTextFunction = [] (const juce::String& t) { return (double) t.retainCharacters ("0123456789").getIntValue(); };
    orderFrom.setTooltip ("The first track number the colours run over (it takes that number's colour)");
    orderTo.setTooltip ("The last track number they run over; all the way up: to the last track, however many come");
    for (auto& key : kindKeys)
    {
        key.setWantsKeyboardFocus (false);
        key.setTooltip ("Ticked: tracks of this kind are coloured in order. Unticked: skipped (they keep their colour and aren't counted)");
        key.onClick = [this] { orderChanged(); };
        addAndMakeVisible (key);
    }
    refreshOrder();
    const auto width = margin * 2 + FamilyColours::paletteHues * (cell + gap) - gap;
    const auto height = titleHeight + familyHeight + gap + FamilyColours::paletteShades * (cell + gap) + footerHeight + listHeight + orderHeight;
    setSize (width, height);
}

juce::Colour TrackColourMenu::current() const
{
    for (const auto id : trackIds)
        if (const auto* t = studio.getProject().findTrack (id))
            return t->colour;
    return {};
}

int TrackColourMenu::lastNumber() const
{
    return juce::jmax (1, (int) studio.getShownTracks().size());
}

void TrackColourMenu::refreshOrder()
{
    const juce::ScopedValueSetter<bool> busy (refreshing, true);
    const auto& rule = studio.getProject().autoColour;
    const auto last = lastNumber();
    orderKey.setToggleState (rule.on, juce::dontSendNotification);
    orderFrom.setRange (1.0, (double) juce::jmax (last, rule.from), 1.0);
    orderTo.setRange (1.0, (double) last, 1.0);
    orderFrom.setValue (rule.from, juce::dontSendNotification);
    orderTo.setValue (rule.to <= 0 ? last : juce::jmin (rule.to, last), juce::dontSendNotification);
    orderFrom.updateText();
    orderTo.updateText();
    const bool kinds[] { rule.instrument, rule.audio, rule.group, rule.fx, rule.folder };
    for (int i = 0; i < 5; ++i)
        kindKeys[i].setToggleState (kinds[i], juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> { &orderFrom, &orderTo, &kindKeys[0], &kindKeys[1], &kindKeys[2], &kindKeys[3], &kindKeys[4] })
        c->setAlpha (rule.on ? 1.0f : 0.5f);
}

void TrackColourMenu::orderChanged()
{
    if (refreshing)
        return;
    pr7s::AutoColour rule;
    rule.on = orderKey.getToggleState();
    rule.from = (int) orderFrom.getValue();
    const auto to = (int) orderTo.getValue();
    rule.to = to >= lastNumber() ? 0 : juce::jmax (to, rule.from);
    rule.instrument = kindKeys[0].getToggleState();
    rule.audio = kindKeys[1].getToggleState();
    rule.group = kindKeys[2].getToggleState();
    rule.fx = kindKeys[3].getToggleState();
    rule.folder = kindKeys[4].getToggleState();
    setAutoColour (rule);
}

void TrackColourMenu::setAutoColour (pr7s::AutoColour rule)
{
    studio.setAutoColour (rule);
    refreshOrder();
    repaint();
}

juce::String TrackColourMenu::getOrderText() const
{
    const auto& rule = studio.getProject().autoColour;
    if (! rule.on)
        return "OFF: EACH TRACK KEEPS THE COLOUR YOU GIVE IT";
    const auto count = (int) studio.autoColourNumbers().size();
    return juce::String (count) + (count == 1 ? " TRACK" : " TRACKS") + " IN ORDER. A COLOUR PICKED FOR THEM DOESN'T STICK WHILE THIS IS ON";
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
    auto order = bottom.removeFromBottom (orderHeight).reduced (margin, 6);
    orderKey.setBounds (order.removeFromTop (28));
    order.removeFromTop (4);
    orderFrom.setBounds (order.removeFromTop (24));
    order.removeFromTop (4);
    orderTo.setBounds (order.removeFromTop (24));
    order.removeFromTop (6);
    // Two rows (one is too narrow for five words): INSTRUMENT AUDIO GROUP, then FX FOLDER.
    auto kinds = order.removeFromTop (26);
    const auto kindWidth = kinds.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
        kindKeys[i].setBounds (kinds.removeFromLeft (kindWidth));
    order.removeFromTop (4);
    kinds = order.removeFromTop (26);
    for (int i = 3; i < 5; ++i)
        kindKeys[i].setBounds (kinds.removeFromLeft (kindWidth));
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
    const auto numbersShown = studio.getProject().autoColour.on;
    for (const auto& [r, colour] : swatches)
    {
        g.setColour (colour);
        g.fillRoundedRectangle (r, 3.0f);
        if (numbersShown)   // (0.79.0) each colour's number, for COLOUR IN ORDER
        {
            g.setColour (colour.getPerceivedBrightness() > 0.6f ? juce::Colour (0xff0c0a0d) : juce::Colour (0xfff4eeea));
            g.setFont (ParagonLookAndFeel::textFont (12.0f));
            g.drawText (juce::String (pr7s::FamilyColours::numberOf (colour)), r.reduced (3.0f, 1.0f), juce::Justification::topLeft, false);
        }
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
    g.fillRect (margin, getHeight() - footerHeight - listHeight - orderHeight, getWidth() - 2 * margin, 1);
    g.fillRect (margin, getHeight() - listHeight - orderHeight, getWidth() - 2 * margin, 1);
    g.fillRect (margin, getHeight() - orderHeight, getWidth() - 2 * margin, 1);
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::textFont (12.0f));
    g.drawFittedText (getOrderText(), getLocalBounds().reduced (margin, 0).removeFromBottom (32).withTrimmedBottom (6),
                      juce::Justification::centredLeft, 2);
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
