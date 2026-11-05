#include "ui/ExportPages.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    const juce::Colour windowColour { 0xff0c0a0d };
    constexpr int resolutions[] = { 48, 96, 120, 192, 240, 384, 480, 960 };
}

MidiExportPage::MidiExportPage (pr7s::Studio& s) : studio (s)
{
    auto* settings = studio.getSettings();
    const auto kept = [settings] (const char* key, bool fallback) { return settings != nullptr ? settings->getBoolValue (key, fallback) : fallback; };
    for (auto* box : { &markers, &type0, &locators, &volumePan })
    {
        box->setColour (juce::ToggleButton::textColourId, ParagonColours::text);
        box->setColour (juce::ToggleButton::tickColourId, ParagonColours::accent);
        addAndMakeVisible (*box);
    }
    markers.setToggleState (kept ("midiExportMarkers", true), juce::dontSendNotification);
    type0.setToggleState (kept ("midiExportType0", false), juce::dontSendNotification);
    locators.setToggleState (kept ("midiExportLocators", false), juce::dontSendNotification);
    volumePan.setToggleState (kept ("midiExportVolumePan", false), juce::dontSendNotification);
    resolutionLabel.setText ("RESOLUTION", juce::dontSendNotification);
    resolutionLabel.setFont (ParagonLookAndFeel::textFont (13.0f));
    resolutionLabel.setColour (juce::Label::textColourId, ParagonColours::textDim);
    addAndMakeVisible (resolutionLabel);
    for (const auto r : resolutions)
        resolution.addItem (juce::String (r) + " ticks per quarter note", r);
    const auto keptResolution = settings != nullptr ? settings->getIntValue ("midiExportResolution", 480) : 480;
    resolution.setSelectedId (std::find (std::begin (resolutions), std::end (resolutions), keptResolution) != std::end (resolutions) ? keptResolution : 480,
                              juce::dontSendNotification);
    addAndMakeVisible (resolution);
    note.setText ("Every instrument track's parts as a standard MIDI file, with the tempo, the time signatures and the markers. "
                  "Notes as you hear them (a part's Transpose and Velocity applied); muted parts left out.", juce::dontSendNotification);
    note.setFont (ParagonLookAndFeel::textFont (13.0f, juce::Font::plain));
    note.setColour (juce::Label::textColourId, ParagonColours::textDim);
    addAndMakeVisible (note);
    for (auto* button : { &exportButton, &cancelButton })
    {
        button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        button->setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        addAndMakeVisible (*button);
    }
    exportButton.setColour (juce::TextButton::textColourOffId, ParagonColours::accent);
    exportButton.onClick = [this]
    {
        if (auto* s = studio.getSettings())
        {
            s->setValue ("midiExportMarkers", markers.getToggleState());
            s->setValue ("midiExportType0", type0.getToggleState());
            s->setValue ("midiExportLocators", locators.getToggleState());
            s->setValue ("midiExportVolumePan", volumePan.getToggleState());
            s->setValue ("midiExportResolution", resolution.getSelectedId());
            s->saveIfNeeded();
        }
        if (onExport)
            onExport (getOptions());
    };
    cancelButton.onClick = [this] { if (onCancel) onCancel(); };
    setSize (560, 300);
}

pr7s::MidiExportOptions MidiExportPage::getOptions() const
{
    pr7s::MidiExportOptions options;
    options.markers = markers.getToggleState();
    options.type0 = type0.getToggleState();
    options.locatorsOnly = locators.getToggleState();
    options.volumePan = volumePan.getToggleState();
    options.resolution = resolution.getSelectedId() > 0 ? resolution.getSelectedId() : 480;
    return options;
}

void MidiExportPage::paint (juce::Graphics& g)
{
    g.fillAll (windowColour);
}

void MidiExportPage::resized()
{
    auto area = getLocalBounds().reduced (16);
    auto row = [&area] (int height) { auto r = area.removeFromTop (height); area.removeFromTop (8); return r; };
    for (auto* box : { &markers, &type0, &locators, &volumePan })
        box->setBounds (row (24));
    auto r = row (28);
    resolutionLabel.setBounds (r.removeFromLeft (110));
    resolution.setBounds (r);
    note.setBounds (row (44));
    auto buttons = area.removeFromBottom (32);
    cancelButton.setBounds (buttons.removeFromRight (96));
    buttons.removeFromRight (8);
    exportButton.setBounds (buttons.removeFromRight (110));
}
