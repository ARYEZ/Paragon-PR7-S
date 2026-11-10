#include "ui/TrackTypeColours.h"

#include "ui/ParagonLookAndFeel.h"

using namespace pr7s;

//==============================================================================
FamilyColourPicker::FamilyColourPicker (std::optional<juce::Colour> c) : current (c)
{
    automaticKey.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
    automaticKey.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
    automaticKey.setWantsKeyboardFocus (false);
    automaticKey.setTooltip ("No colour of its own: each new track of this type takes red, pink or orange in turn, by its place");
    automaticKey.onClick = [this]
    {
        current.reset();
        repaint();
        if (onPick)
            onPick (std::nullopt);
    };
    addAndMakeVisible (automaticKey);
    const auto width = margin * 2 + FamilyColours::paletteHues * (cell + gap) - gap;
    setSize (width, 40 + 40 + gap + FamilyColours::paletteShades * (cell + gap) + margin);
}

void FamilyColourPicker::resized()
{
    swatches.clear();
    auto area = getLocalBounds().reduced (margin, 0).toFloat();
    automaticKey.setBounds (area.removeFromTop (40.0f).reduced (0.0f, 6.0f).toNearestInt());
    auto family = area.removeFromTop (34.0f);
    const auto third = (family.getWidth() - 2.0f * gap) / 3.0f;
    for (const auto colour : { FamilyColours::pink, FamilyColours::red, FamilyColours::orange })
    {
        swatches.push_back ({ family.removeFromLeft (third), colour });
        family.removeFromLeft ((float) gap);
    }
    area.removeFromTop (6.0f + gap);
    for (int shade = 0; shade < FamilyColours::paletteShades; ++shade)
        for (int hue = 0; hue < FamilyColours::paletteHues; ++hue)
            swatches.push_back ({ juce::Rectangle<float> (area.getX() + (float) (hue * (cell + gap)), area.getY() + (float) (shade * (cell + gap)),
                                                          (float) cell, (float) cell),
                                  FamilyColours::paletteColour (hue, shade) });
}

void FamilyColourPicker::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0a0d));
    g.setColour (ParagonColours::accent);
    g.fillRect (0, 0, getWidth(), 3);
    for (const auto& [r, colour] : swatches)
    {
        g.setColour (colour);
        g.fillRoundedRectangle (r, 3.0f);
        if (current && *current == colour)
        {
            g.setColour (juce::Colour (0xfff4eeea));
            g.drawRoundedRectangle (r.reduced (1.0f), 3.0f, 2.0f);
        }
    }
    if (! current)
    {
        g.setColour (juce::Colour (0xfff4eeea));
        g.drawRoundedRectangle (automaticKey.getBounds().toFloat().reduced (0.5f), 4.0f, 1.5f);
    }
}

std::optional<juce::Colour> FamilyColourPicker::colourAt (juce::Point<float> p) const
{
    for (const auto& [r, colour] : swatches)
        if (r.contains (p))
            return colour;
    return std::nullopt;
}

void FamilyColourPicker::mouseDown (const juce::MouseEvent& e)
{
    if (const auto colour = colourAt (e.position))
    {
        current = *colour;
        repaint();
        if (onPick)
            onPick (*colour);
    }
}

void FamilyColourPicker::show (juce::Component& around, std::optional<juce::Colour> current, std::function<void (std::optional<juce::Colour>)> onPick)
{
    auto picker = std::make_unique<FamilyColourPicker> (current);
    picker->onPick = std::move (onPick);
    juce::CallOutBox::launchAsynchronously (std::move (picker), around.getScreenBounds(), nullptr);
}

//==============================================================================
void TrackTypeColoursPage::SwatchKey::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    if (colour)
    {
        g.setColour (*colour);
        g.fillRoundedRectangle (r, 4.0f);
    }
    else
    {
        // AUTOMATIC: the three it takes in turn, and the word.
        const auto w = r.getWidth() / 3.0f;
        int i = 0;
        for (const auto c : { FamilyColours::red, FamilyColours::pink, FamilyColours::orange })
        {
            g.setColour (c);
            g.fillRect (juce::Rectangle<float> (r.getX() + w * (float) i++, r.getY(), w, r.getHeight()).reduced (0.0f, 0.0f));
        }
        g.setColour (juce::Colour (0xfff4eeea));
        g.setFont (ParagonLookAndFeel::titleFont (15.0f));
        g.drawText ("AUTOMATIC", r, juce::Justification::centred);
    }
    g.setColour (down ? ParagonColours::accent : over ? juce::Colour (0xfff4eeea) : ParagonColours::panelEdge);
    g.drawRoundedRectangle (r, 4.0f, over || down ? 1.5f : 1.0f);
}

TrackTypeColoursPage::TrackTypeColoursPage (Studio& s) : studio (s)
{
    for (int i = 0; i < Studio::numTrackTypes; ++i)
    {
        auto row = std::make_unique<Row>();
        row->kind = (Track::Kind) i;
        auto* r = row.get();
        r->name.setText (Studio::trackTypeName (r->kind), juce::dontSendNotification);
        r->name.setFont (ParagonLookAndFeel::titleFont (18.0f));
        r->name.setColour (juce::Label::textColourId, ParagonColours::text);
        addAndMakeVisible (r->name);
        r->swatch.setTooltip ("The colour a new " + Studio::trackTypeName (r->kind).toLowerCase() + " track starts with (click to choose)");
        r->swatch.onClick = [this, r]
        {
            FamilyColourPicker::show (r->swatch, studio.getTrackTypeLook (r->kind).colour,
                                      [safe = juce::Component::SafePointer<TrackTypeColoursPage> (this), kind = r->kind] (std::optional<juce::Colour> c)
                                      {
                                          if (safe != nullptr)
                                              safe->setColour (kind, c);
                                      });
        };
        addAndMakeVisible (r->swatch);
        r->listKey.setWantsKeyboardFocus (false);
        r->listKey.setTooltip ("A new track of this type shows its colour over its row in the track list, as much as the bar says");
        r->listKey.onClick = [this, r] { setListColour (r->kind, r->listKey.getToggleState(), (int) r->amount.getValue()); };
        addAndMakeVisible (r->listKey);
        r->amount.setRange (1.0, 100.0, 1.0);
        r->amount.setTextValueSuffix (" %");
        r->amount.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 22);
        r->amount.setTooltip ("How much of the colour the row shows: 1 % a faint tint, 100 % the whole colour");
        r->amount.onValueChange = [this, r] { setListColour (r->kind, true, (int) r->amount.getValue()); };
        addAndMakeVisible (r->amount);
        rows[(size_t) i] = std::move (row);
    }
    applyKey.setTooltip ("Gives the open song's tracks their type's colour and track-list colour now (Undo takes it back)");
    applyKey.onClick = [this] { applyToSong(); };
    resetKey.setTooltip ("Every type back to AUTOMATIC (red, pink, orange in turn) with no colour in the track list");
    resetKey.onClick = [this] { resetAll(); };
    for (auto* b : { &applyKey, &resetKey })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (*b);
    }
    refresh();
}

void TrackTypeColoursPage::refresh()
{
    for (auto& r : rows)
    {
        const auto look = studio.getTrackTypeLook (r->kind);
        r->swatch.colour = look.colour;
        r->swatch.repaint();
        r->listKey.setToggleState (look.listColour, juce::dontSendNotification);
        r->amount.setValue (look.listAmount, juce::dontSendNotification);
        r->amount.setEnabled (look.listColour);
    }
}

void TrackTypeColoursPage::setColour (Track::Kind kind, std::optional<juce::Colour> colour)
{
    auto look = studio.getTrackTypeLook (kind);
    look.colour = colour;
    studio.setTrackTypeLook (kind, look);
    refresh();
}

void TrackTypeColoursPage::setListColour (Track::Kind kind, bool on, int amount)
{
    auto look = studio.getTrackTypeLook (kind);
    look.listColour = on;
    look.listAmount = amount;
    studio.setTrackTypeLook (kind, look);
    refresh();
}

void TrackTypeColoursPage::applyToSong()
{
    studio.applyTrackTypeLooks();
}

void TrackTypeColoursPage::resetAll()
{
    for (int i = 0; i < Studio::numTrackTypes; ++i)
        studio.setTrackTypeLook ((Track::Kind) i, {});
    refresh();
}

void TrackTypeColoursPage::resized()
{
    auto area = getLocalBounds();
    titleArea = area.removeFromTop (34);
    noteArea = area.removeFromTop (58);
    area.removeFromTop (10);
    headArea = area.removeFromTop (22);
    area.removeFromTop (6);
    for (auto& r : rows)
    {
        auto row = area.removeFromTop (44).reduced (0, 5);
        r->name.setBounds (row.removeFromLeft (200));
        r->swatch.setBounds (row.removeFromLeft (200));
        row.removeFromLeft (28);
        r->listKey.setBounds (row.removeFromLeft (250));
        row.removeFromLeft (12);
        r->amount.setBounds (row.withWidth (juce::jmin (260, row.getWidth())));
        area.removeFromTop (4);
    }
    area.removeFromTop (18);
    auto foot = area.removeFromTop (34);
    applyKey.setBounds (foot.removeFromLeft (300));
    foot.removeFromLeft (12);
    resetKey.setBounds (foot.removeFromLeft (180));
}

void TrackTypeColoursPage::paint (juce::Graphics& g)
{
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (22.0f));
    g.drawText ("TRACK TYPE DEFAULT COLOURS", titleArea, juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::textFont (13.0f));
    g.drawFittedText ("What a new track of each type starts with: its colour (AUTOMATIC: red, pink and orange in turn, by its place) and its colour "
                      "over its row in the track list. You can still colour any track yourself afterwards; COLOUR IN ORDER (the colour menu) wins "
                      "over these while it's on.", noteArea, juce::Justification::topLeft, 3);
    g.setFont (ParagonLookAndFeel::textFont (12.0f));
    auto head = headArea;
    g.drawText ("TRACK TYPE", head.removeFromLeft (200), juce::Justification::centredLeft, false);
    g.drawText ("COLOUR", head.removeFromLeft (200), juce::Justification::centredLeft, false);
    head.removeFromLeft (28);
    g.drawText ("TRACK LIST", head.removeFromLeft (250), juce::Justification::centredLeft, false);
    head.removeFromLeft (12);
    g.drawText ("HOW MUCH", head.removeFromLeft (260), juce::Justification::centredLeft, false);
}
