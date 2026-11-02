#include "ChordEventEditor.h"

using pr7s::Chord;
using ChordUi::chordFont;

ChordEventEditor::ChordEventEditor (const Chord& c, const pr7s::Key& k, std::optional<Chord> b)
    : chord (c), key (k), before (b)
{
    for (int i = 0; i < 12; ++i)
    {
        auto* r = roots.add (new ChordUi::InterKey (key.spell (i).text()));
        r->onClick = [this, i] { auto x = chord; x.root = i; choose (x); };
        auto* bs = basses.add (new ChordUi::InterKey (key.spell (i).text()));
        bs->onClick = [this, i] { auto x = chord; x.bass = i; choose (x); };
    }
    auto* none = basses.add (new ChordUi::InterKey ("-"));
    none->onClick = [this] { auto x = chord; x.bass = -1; choose (x); };
    for (const auto type : Chord::allTypes())
    {
        auto* t = types.add (new juce::TextButton (Chord::typeName (type)));
        t->onClick = [this, type] { auto x = chord; x.type = type; choose (x); };
    }
    for (const auto tension : Chord::allTensions())
    {
        auto* t = tensions.add (new ChordUi::InterKey (Chord::tensionName (tension, Chord::Type::major)));
        t->onClick = [this, tension] { auto x = chord; x.tension = tension; choose (x); };
    }
    // What the chord before suggests (the six best).
    if (before)
    {
        suggestions = pr7s::suggestNext (*before, key);
        if (suggestions.size() > 6)
            suggestions.resize (6);
        for (const auto& s : suggestions)
        {
            auto* n = next.add (new ChordUi::InterKey (s.chord.name (key) + "  " + juce::String (s.score)));
            n->setTooltip (s.chord.romanName (key) + ": " + s.reason);
            n->onClick = [this, sc = s.chord] { choose (sc); };
        }
    }
    for (auto* list : { &roots, &types, &tensions, &basses, &next })
        for (auto* key : *list)
        {
            ChordUi::styleKey (*key);
            addAndMakeVisible (key);
        }
    ChordUi::styleKey (deleteKey);
    deleteKey.onClick = [this] { if (onDelete) onDelete(); };
    addAndMakeVisible (deleteKey);
    setSize (440, before ? 310 : 260);
    refresh();
}

void ChordEventEditor::choose (const Chord& c)
{
    chord = c;
    refresh();
    if (onChange)
        onChange (chord);
}

void ChordEventEditor::refresh()
{
    for (int i = 0; i < 12; ++i)
    {
        roots[i]->setToggleState (chord.root == i, juce::dontSendNotification);
        basses[i]->setToggleState (chord.bass == i, juce::dontSendNotification);
    }
    basses[12]->setToggleState (chord.bass < 0, juce::dontSendNotification);
    for (int i = 0; i < types.size(); ++i)
        types[i]->setToggleState (chord.type == Chord::allTypes()[(size_t) i], juce::dontSendNotification);
    for (int i = 0; i < tensions.size(); ++i)
        tensions[i]->setToggleState (chord.tension == Chord::allTensions()[(size_t) i], juce::dontSendNotification);
    for (int i = 0; i < next.size(); ++i)
        next[i]->setToggleState (suggestions[(size_t) i].chord == chord, juce::dontSendNotification);
    repaint();
}

void ChordEventEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff100c11));
    auto area = getLocalBounds().reduced (10, 8);
    auto head = area.removeFromTop (22).toFloat();
    g.setColour (ParagonColours::text);
    g.setFont (chordFont (17.0f));
    g.drawText (chord.name (key), head.removeFromLeft (120.0f), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (chordFont (12.0f));
    g.drawText (chord.romanName (key) + " in " + ChordUi::spokenKey (key) + ":  " + chord.noteNames (key).joinIntoString (" "), head,
                juce::Justification::centredLeft, true);
    g.setFont (juce::Font (ParagonLookAndFeel::textFont (10.0f)));
    for (const auto& [label, y] : std::initializer_list<std::pair<const char*, int>> {
             { "ROOT", roots[0]->getY() }, { "TYPE", types[0]->getY() }, { "TENSION", tensions[0]->getY() }, { "BASS", basses[12]->getY() } })
        g.drawText (label, juce::Rectangle<int> (10, y, 54, roots[0]->getHeight()), juce::Justification::centredLeft, false);
    if (! next.isEmpty())
        g.drawText (before ? "AFTER " + before->name (key).toUpperCase() : juce::String(), juce::Rectangle<int> (10, next[0]->getY(), 54, next[0]->getHeight()),
                    juce::Justification::centredLeft, true);
}

void ChordEventEditor::resized()
{
    auto area = getLocalBounds().reduced (10, 8);
    area.removeFromTop (28);
    const auto x = area.getX() + 56;
    const auto width = (float) (area.getRight() - x);
    const auto h = 22.0f, gap = 3.0f;
    auto y = (float) area.getY();
    const auto row = [&] (juce::OwnedArray<juce::TextButton>& keys, int from, int count, int across, int offset = 0)
    {
        const auto w = width / (float) across;
        for (int i = 0; i < count; ++i)
            keys[from + i]->setBounds (juce::Rectangle<float> ((float) x + w * (float) (i + offset), y, w - 2.0f, h).toNearestInt());
        y += h + gap;
    };
    row (roots, 0, 6, 6);
    row (roots, 6, 6, 6);
    row (types, 0, 6, 6);
    row (tensions, 0, 7, 7);
    row (tensions, 7, 7, 7);
    basses[12]->setBounds (juce::Rectangle<float> ((float) x, y, width / 7.0f - 2.0f, h).toNearestInt());
    row (basses, 0, 6, 7, 1);
    row (basses, 6, 6, 7, 1);
    if (! next.isEmpty())
    {
        y += 4.0f;
        row (next, 0, next.size(), 6);
    }
    deleteKey.setBounds (getWidth() - 10 - 80, 8, 80, 22);
}
