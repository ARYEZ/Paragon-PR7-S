#include "ui/AutoFadesPage.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    juce::FontOptions font (float height, int style = juce::Font::bold) { return ParagonLookAndFeel::textFont (height, style); }
    constexpr int crossfadeGroup = 5201;
}

AutoFadesPage::AutoFadesPage (pr7s::Studio& s, int track) : studio (s), trackId (track)
{
    const auto* t = studio.getProject().findTrack (trackId);
    const auto start = t != nullptr ? studio.getProject().autoFadesOf (*t) : studio.getProject().autoFades;
    fadeIn.setToggleState (start.fadeIn, juce::dontSendNotification);
    fadeOut.setToggleState (start.fadeOut, juce::dontSendNotification);
    crossfades.setToggleState (start.crossfades, juce::dontSendNotification);
    useProject.setToggleState (t == nullptr || ! t->autoFades, juce::dontSendNotification);
    useProject.setVisible (t != nullptr);
    equalPower.setRadioGroupId (crossfadeGroup);
    equalGain.setRadioGroupId (crossfadeGroup);
    equalPower.setToggleState (start.equalPower, juce::dontSendNotification);
    equalGain.setToggleState (! start.equalPower, juce::dontSendNotification);
    for (auto* box : { &fadeIn, &fadeOut, &crossfades, &useProject, &equalPower, &equalGain })
    {
        box->setColour (juce::ToggleButton::textColourId, ParagonColours::text);
        box->setColour (juce::ToggleButton::tickColourId, ParagonColours::accent);
        box->setWantsKeyboardFocus (false);
        addChildComponent (*box);
        if (box != &useProject)
            box->setVisible (true);
    }
    useProject.onClick = [this]
    {
        // Back to the song's: the boxes show what the song has.
        if (useProject.getToggleState())
        {
            const auto& song = studio.getProject().autoFades;
            fadeIn.setToggleState (song.fadeIn, juce::dontSendNotification);
            fadeOut.setToggleState (song.fadeOut, juce::dontSendNotification);
            crossfades.setToggleState (song.crossfades, juce::dontSendNotification);
            equalPower.setToggleState (song.equalPower, juce::dontSendNotification);
            equalGain.setToggleState (! song.equalPower, juce::dontSendNotification);
            length.setValue (song.milliseconds, juce::dontSendNotification);
        }
        refreshEnabled();
    };
    length.setSliderStyle (juce::Slider::LinearBar);
    length.setRange (1.0, 500.0, 1.0);
    length.setSkewFactorFromMidPoint (50.0);
    length.setTextValueSuffix (" MS");
    length.setValue (start.milliseconds, juce::dontSendNotification);
    length.setColour (juce::Slider::trackColourId, ParagonColours::accent.withAlpha (0.55f));
    length.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff151217));
    length.setColour (juce::Slider::textBoxTextColourId, ParagonColours::text);
    length.setColour (juce::Slider::textBoxOutlineColourId, ParagonColours::panelEdge);
    addAndMakeVisible (length);
    note.setText (t != nullptr ? "For " + t->name + ". Fades made as the audio plays (not drawn on the events)."
                               : "For the whole song (tracks with their own settings keep theirs). Fades made as the audio plays.",
                  juce::dontSendNotification);
    note.setFont (font (12.5f, juce::Font::plain));
    note.setColour (juce::Label::textColourId, ParagonColours::textDim);
    addAndMakeVisible (note);
    for (auto* key : { &okKey, &cancelKey })
    {
        key->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        key->setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        addAndMakeVisible (*key);
    }
    okKey.setColour (juce::TextButton::textColourOffId, ParagonColours::accent);
    okKey.onClick = [this] { apply(); if (onClose) onClose(); };
    cancelKey.onClick = [this] { if (onClose) onClose(); };
    refreshEnabled();
    setSize (480, t != nullptr ? 420 : 386);
}

void AutoFadesPage::refreshEnabled()
{
    // A track using the song's settings: shown, not changed here.
    const auto own = ! useProject.isVisible() || ! useProject.getToggleState();
    for (juce::Component* c : { (juce::Component*) &fadeIn, (juce::Component*) &fadeOut, (juce::Component*) &crossfades,
                                (juce::Component*) &equalPower, (juce::Component*) &equalGain, (juce::Component*) &length })
        c->setEnabled (own);
}

pr7s::AutoFades AutoFadesPage::getSettings() const
{
    pr7s::AutoFades f;
    f.fadeIn = fadeIn.getToggleState();
    f.fadeOut = fadeOut.getToggleState();
    f.crossfades = crossfades.getToggleState();
    f.milliseconds = length.getValue();
    f.equalPower = equalPower.getToggleState();
    return f;
}

void AutoFadesPage::setLength (double milliseconds)
{
    length.setValue (milliseconds, juce::dontSendNotification);
}

void AutoFadesPage::apply()
{
    if (studio.getProject().findTrack (trackId) == nullptr)
        studio.setAutoFades (getSettings());
    else if (useProject.getToggleState())
        studio.setTrackAutoFades (trackId, std::nullopt);
    else
        studio.setTrackAutoFades (trackId, getSettings());
}

void AutoFadesPage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0a0d));
    g.setColour (ParagonColours::accent);
    g.fillRect (0, 0, getWidth(), 3);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (15.0f));
    g.drawText (useProject.isVisible() ? "AUTO FADES: TRACK" : "AUTO FADES: SONG",
                getLocalBounds().reduced (18, 0).withTrimmedTop (12).withHeight (24), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (font (12.0f));
    const auto top = useProject.isVisible() ? 34 : 0;
    g.drawText ("FADES", getLocalBounds().reduced (18, 0).withTrimmedTop (50 + top).withHeight (16), juce::Justification::centredLeft, false);
    g.drawText ("LENGTH", getLocalBounds().reduced (18, 0).withTrimmedTop (156 + top).withHeight (16), juce::Justification::centredLeft, false);
    g.drawText ("CROSSFADES", getLocalBounds().reduced (18, 0).withTrimmedTop (216 + top).withHeight (16), juce::Justification::centredLeft, false);
}

void AutoFadesPage::resized()
{
    auto area = getLocalBounds().reduced (18, 0);
    area.removeFromTop (42);
    if (useProject.isVisible())
        useProject.setBounds (area.removeFromTop (30)), area.removeFromTop (4);
    area.removeFromTop (26);
    fadeIn.setBounds (area.removeFromTop (28));
    fadeOut.setBounds (area.removeFromTop (28));
    crossfades.setBounds (area.removeFromTop (28));
    area.removeFromTop (24);
    length.setBounds (area.removeFromTop (28));
    area.removeFromTop (32);
    auto shapes = area.removeFromTop (28);
    equalPower.setBounds (shapes.removeFromLeft (shapes.getWidth() / 2));
    equalGain.setBounds (shapes);
    area.removeFromTop (8);
    auto keys = area.removeFromBottom (48).withSizeKeepingCentre (area.getWidth(), 32);
    note.setBounds (area);
    cancelKey.setBounds (keys.removeFromRight (90));
    keys.removeFromRight (8);
    okKey.setBounds (keys.removeFromRight (90));
}

void AutoFadesPage::show (juce::Component& around, pr7s::Studio& studio, int trackId)
{
    auto* page = new AutoFadesPage (studio, trackId);
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (page);
    options.dialogTitle = "Paragon PR7-S: Auto Fades";
    options.dialogBackgroundColour = juce::Colour (0xff0c0a0d);
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.componentToCentreAround = &around;
    options.launchAsync();
    page->onClose = [page]
    {
        if (auto* dialog = page->findParentComponentOfClass<juce::DialogWindow>())
            dialog->exitModalState (0);   // deleted once its click is over
    };
}
