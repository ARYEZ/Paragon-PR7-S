#include "ui/PadsRemotePage.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    using Learn = pr7s::Studio::PadLearn;

    juce::String keyName (int note) { return note < 0 ? juce::String ("NONE") : pr7s::noteName (note); }

    void style (juce::TextButton& key, const juce::String& tip = {})
    {
        key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key.setWantsKeyboardFocus (false);
        if (tip.isNotEmpty())
            key.setTooltip (tip);
    }
}

PadsRemotePage::PadsRemotePage (pr7s::Studio& s) : studio (s)
{
    style (rangeKey, "The keys that play the pads: click, then press the first and the last key on your MIDI keyboard");
    rangeKey.onClick = [this] { learn (Learn::range); };
    style (latchKey, "Latch Chords: a pad plays until it's played again");
    latchKey.onClick = [this] { auto p = studio.getChordPads(); p.latch = ! p.latch; studio.setChordPads (p); refresh(); };
    style (sectionsKey, "Sections mode: a pad's key picks its chord, the section keys play its notes from the bottom (1: the bass)");
    sectionsKey.onClick = [this]
    {
        auto p = studio.getChordPads();
        p.sectionsMode = ! p.sectionsMode;
        studio.getPadPlayer().releaseAll();
        studio.setChordPads (p);
        refresh();
    };
    style (modifiersKey, "Activate the chord modifier keys (they act on the last pad played)");
    modifiersKey.onClick = [this] { auto p = studio.getChordPads(); p.modifiersOn = ! p.modifiersOn; studio.setChordPads (p); refresh(); };
    style (cancelKey);
    cancelKey.onClick = [this] { studio.cancelPadLearn(); refresh(); };
    for (int i = 0; i < 5; ++i)
    {
        auto* section = sectionKeys.add (new juce::TextButton());
        style (*section, "Section " + juce::String (i + 1) + ": the held chord's " + (i == 0 ? juce::String ("lowest note (the bass)") : "note " + juce::String (i + 1) + " from the bottom")
                         + ". Click, then press its key");
        section->onClick = [this, i] { learn (Learn::section, i); };
        auto* sub = subsectionKeys.add (new juce::TextButton());
        style (*sub, "Subsection " + juce::String (i + 1) + ": section " + juce::String (i + 1) + " an octave up. Click, then press its key");
        sub->onClick = [this, i] { learn (Learn::subsection, i); };
    }
    for (int m = 0; m < pr7s::ChordPads::numModifiers; ++m)
    {
        auto* key = modifierKeys.add (new juce::TextButton());
        style (*key, pr7s::ChordPads::modifierName ((pr7s::ChordPads::Modifier) m) + " (on the last pad played). Click, then press its key");
        key->onClick = [this, m] { learn (Learn::modifier, m); };
    }
    for (auto* k : { &rangeKey, &latchKey, &sectionsKey, &modifiersKey, &cancelKey })
        addAndMakeVisible (*k);
    for (auto* list : { &sectionKeys, &subsectionKeys, &modifierKeys })
        for (auto* k : *list)
            addAndMakeVisible (k);
    studio.onLearned = [safe = juce::Component::SafePointer<PadsRemotePage> (this)] { if (safe != nullptr) safe->refresh(); };
    refresh();
    setSize (width, height);
    startTimerHz (10);
}

PadsRemotePage::~PadsRemotePage()
{
    studio.cancelPadLearn();
    studio.onLearned = nullptr;
}

void PadsRemotePage::learn (Learn what, int index)
{
    // A second click on the box being learned: stop.
    if (studio.getPadLearn() == what && what != Learn::none)
        studio.cancelPadLearn();
    else
        studio.startPadLearn (what, index);
    refresh();
}

juce::String PadsRemotePage::getKeyText (Learn what, int index) const
{
    const auto& p = studio.getChordPads();
    switch (what)
    {
        case Learn::range:      return "PADS: " + keyName (p.triggerStart) + " TO " + keyName (p.triggerEnd);
        case Learn::section:    return juce::String (index + 1) + ": " + keyName (p.sectionKeys[(size_t) index]);
        case Learn::subsection: return juce::String (index + 1) + "+8VA: " + keyName (p.subsectionKeys[(size_t) index]);
        case Learn::modifier:   return pr7s::ChordPads::modifierName ((pr7s::ChordPads::Modifier) index).toUpperCase() + ": "
                                       + keyName (p.modifierKeys[(size_t) index]);
        case Learn::none:       break;
    }
    return {};
}

void PadsRemotePage::timerCallback()
{
    refresh();
}

void PadsRemotePage::refresh()
{
    const auto& p = studio.getChordPads();
    const auto learning = studio.getPadLearn();
    const auto set = [&] (juce::TextButton& key, Learn what, int index)
    {
        const auto lit = learning == what && (what == Learn::range || studio.getPadLearnIndex() == index);
        key.setButtonText (lit ? juce::String ("PRESS A KEY...") : getKeyText (what, index));
        key.setToggleState (lit, juce::dontSendNotification);
    };
    set (rangeKey, Learn::range, 0);
    for (int i = 0; i < 5; ++i)
    {
        set (*sectionKeys[i], Learn::section, i);
        set (*subsectionKeys[i], Learn::subsection, i);
        sectionKeys[i]->setEnabled (p.sectionsMode);
        subsectionKeys[i]->setEnabled (p.sectionsMode);
    }
    for (int m = 0; m < modifierKeys.size(); ++m)
    {
        set (*modifierKeys[m], Learn::modifier, m);
        modifierKeys[m]->setEnabled (p.modifiersOn);
    }
    latchKey.setToggleState (p.latch, juce::dontSendNotification);
    sectionsKey.setToggleState (p.sectionsMode, juce::dontSendNotification);
    modifiersKey.setToggleState (p.modifiersOn, juce::dontSendNotification);
    cancelKey.setEnabled (learning != Learn::none);
    repaint();
}

void PadsRemotePage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0a0d));
    auto area = getLocalBounds().reduced (14, 10);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (16.0f));
    g.drawText ("CHORD PADS REMOTE", area.removeFromTop (22), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::Font (ParagonLookAndFeel::textFont (11.5f)));
    g.drawFittedText (studio.getPadLearn() != Learn::none
                          ? juce::String ("Press the key on your MIDI keyboard now (for the pads' range: the first key, then the last).")
                          : juce::String ("Click a box, then press the key on your MIDI keyboard (MIDI Learn). The keys work while the chord pads are open."),
                      area.removeFromTop (30), juce::Justification::topLeft, 2, 0.9f);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (13.0f));
    const auto heading = [&g] (juce::Rectangle<int> r, const char* text) { g.drawText (text, r, juce::Justification::centredLeft, false); };
    heading ({ 14, 70, 300, 20 }, "PADS");
    heading ({ 14, 140, 300, 20 }, "SECTIONS");
    heading ({ 14, 262, 300, 20 }, "CHORD MODIFIERS");
}

void PadsRemotePage::resized()
{
    const auto w = getWidth() - 28;
    rangeKey.setBounds (14, 92, 250, 28);
    latchKey.setBounds (272, 92, 150, 28);
    cancelKey.setBounds (getWidth() - 14 - 130, 92, 130, 28);
    sectionsKey.setBounds (getWidth() - 14 - 150, 136, 150, 26);
    const auto cell = (w - 4 * 6) / 5;
    for (int i = 0; i < 5; ++i)
    {
        sectionKeys[i]->setBounds (14 + i * (cell + 6), 166, cell, 28);
        subsectionKeys[i]->setBounds (14 + i * (cell + 6), 200, cell, 28);
    }
    modifiersKey.setBounds (getWidth() - 14 - 150, 258, 150, 26);
    const auto half = (w - 6) / 2;
    for (int m = 0; m < modifierKeys.size(); ++m)
        modifierKeys[m]->setBounds (14 + (m % 2) * (half + 6), 290 + (m / 2) * 34, half, 28);
}
