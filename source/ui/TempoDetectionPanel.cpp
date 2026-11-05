#include "ui/TempoDetectionPanel.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    const juce::Colour pageColour { 0xff0d0a0e };
}

TempoDetectionPanel::TempoDetectionPanel (pr7s::Studio& s, std::function<std::vector<int>()> parts)
    : studio (s), selectedParts (std::move (parts))
{
    const auto style = [this] (juce::TextButton& key, const juce::String& tip, std::function<void()> action)
    {
        key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key.setWantsKeyboardFocus (false);
        key.setTooltip (tip);
        key.onClick = std::move (action);
        addAndMakeVisible (key);
    };
    style (analyseKey, "Find the beats of the selected audio event or MIDI part (7 s or more); the tempo track follows them", [this] { analyse(); });
    style (times2, "Multiply by 2: twice as many beats (the material is twice as fast as found)", [this] { correct (Correction::times2); });
    style (half, "Divide by 2: every second beat (the material is half as fast as found)", [this] { correct (Correction::half); });
    style (fourThirds, "Multiply by 4/3: four beats where three were found (dotted notes or triplets misled it)", [this] { correct (Correction::fourThirds); });
    style (threeQuarters, "Multiply by 3/4: three beats where four were found (with X 2: 2/4 found as 6/8, or the other way)", [this] { correct (Correction::threeQuarters); });
    style (offbeat, "Offbeat Correction: the beats half a beat on (the offbeat was taken for the beat)", [this] { correct (Correction::offbeat); });
    style (smooth, "Smooth Tempo: steadies the beats (for material with a steady tempo that came out uneven)", [this] { correct (Correction::smooth); });
    style (resetKey, "Reset: the tempo as it was before ANALYZE (everything moved back with it). Undo works too", [this] { reset(); });
    refreshKeys();
    setSize (preferredWidth, preferredHeight);
}

juce::TextButton* TempoDetectionPanel::keyNamed (const juce::String& name)
{
    for (auto* key : { &analyseKey, &times2, &half, &fourThirds, &threeQuarters, &offbeat, &smooth, &resetKey })
        if (key->getButtonText() == name)
            return key;
    return nullptr;
}

bool TempoDetectionPanel::analyse()
{
    const auto parts = selectedParts ? selectedParts() : std::vector<int> {};
    if (parts.empty())
    {
        status = "Select an audio event or a MIDI part first (7 seconds or more).";
        repaint();
        return false;
    }
    return analysePart (parts.front());
}

bool TempoDetectionPanel::analysePart (int id)
{
    pr7s::BeatAnalysis found;
    juce::String error;
    if (! studio.analyseTempo (id, found, error))
    {
        status = error;
        repaint();
        return false;
    }
    if (id != partId || ! haveOriginal)
    {
        // (What RESET goes back to: the tempo before the first analysis of this part.)
        originalOn = studio.getProject().tempoTrackOn;
        originalEvents = studio.getProject().tempoEvents;
        haveOriginal = true;
    }
    partId = id;
    partName = {};
    if (const auto where = studio.findPart (id))
        if (const auto* track = studio.getProject().findTrack (where->first))
        {
            const auto& part = track->parts[(size_t) where->second];
            partName = part.name.isNotEmpty() ? part.name : track->name;
        }
    beats = found.beats;
    confidence = found.confidence;
    return apply();
}

bool TempoDetectionPanel::correct (Correction c)
{
    if (beats.size() < 2 || partId == 0)
        return false;
    switch (c)
    {
        case Correction::times2:        beats = pr7s::rescaleBeats (beats, 2.0); break;
        case Correction::half:          beats = pr7s::rescaleBeats (beats, 0.5); break;
        case Correction::fourThirds:    beats = pr7s::rescaleBeats (beats, 4.0 / 3.0); break;
        case Correction::threeQuarters: beats = pr7s::rescaleBeats (beats, 0.75); break;
        case Correction::offbeat:       beats = pr7s::rescaleBeats (beats, 1.0, 0.5); break;
        case Correction::smooth:        beats = pr7s::smoothBeats (beats); break;
    }
    return apply();
}

bool TempoDetectionPanel::apply()
{
    juce::String report;
    const auto ok = studio.applyDetectedBeats (partId, beats, report);
    status = ok ? report : "Not applied: " + report;
    refreshKeys();
    repaint();
    return ok;
}

bool TempoDetectionPanel::reset()
{
    if (! haveOriginal)
        return false;
    studio.restoreTempo (originalOn, originalEvents);
    status = "The tempo is back as it was before the analysis.";
    beats.clear();
    haveOriginal = false;
    refreshKeys();
    repaint();
    return true;
}

void TempoDetectionPanel::refreshKeys()
{
    const auto have = beats.size() >= 2;
    for (auto* key : { &times2, &half, &fourThirds, &threeQuarters, &offbeat, &smooth })
        key->setEnabled (have);
    resetKey.setEnabled (haveOriginal);
}

void TempoDetectionPanel::paint (juce::Graphics& g)
{
    g.fillAll (pageColour);
    auto area = getLocalBounds().reduced (16, 12).toFloat();
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (18.0f));
    g.drawText ("TEMPO DETECTION", area.removeFromTop (24.0f), juce::Justification::centredLeft, false);
    g.setFont (ParagonLookAndFeel::textFont (13.0f));
    g.setColour (ParagonColours::textDim);
    g.drawText ("NAME: " + (partName.isNotEmpty() ? partName : juce::String ("(nothing analysed yet)")), area.removeFromTop (20.0f),
                juce::Justification::centredLeft, true);

    // The tempo of each beat found, along the part (bars of the family's gradient), and the beats as ticks.
    g.setColour (juce::Colour (0xff151016));
    g.fillRoundedRectangle (graph, 4.0f);
    if (beats.size() >= 2)
    {
        const auto from = beats.front(), to = beats.back();
        auto low = 1.0e9, high = 0.0;
        std::vector<double> bpm;
        for (size_t i = 1; i < beats.size(); ++i)
        {
            bpm.push_back (60.0 / juce::jmax (1.0e-4, beats[i] - beats[i - 1]));
            low = juce::jmin (low, bpm.back());
            high = juce::jmax (high, bpm.back());
        }
        if (high - low < 10.0)
            low -= 5.0, high += 5.0;
        const auto inner = graph.reduced (8.0f, 10.0f).withTrimmedTop (12.0f);   // (under the readouts)
        const auto xOf = [&] (double t) { return inner.getX() + (float) ((t - from) / juce::jmax (1.0e-6, to - from)) * inner.getWidth(); };
        const auto yOf = [&] (double v) { return inner.getBottom() - (float) ((v - low) / (high - low)) * inner.getHeight(); };
        juce::Path curve;
        for (size_t i = 0; i < bpm.size(); ++i)
        {
            const auto x1 = xOf (beats[i]), x2 = xOf (beats[i + 1]), y = yOf (bpm[i]);
            if (i == 0)
                curve.startNewSubPath (x1, y);
            else
                curve.lineTo (x1, y);
            curve.lineTo (x2, y);
        }
        g.setGradientFill (juce::ColourGradient::horizontal (pr7s::FamilyColours::orange, inner.getX(), pr7s::FamilyColours::pink, inner.getRight()));
        g.strokePath (curve, juce::PathStrokeType (2.0f));
        g.setColour (ParagonColours::textDim.withAlpha (0.5f));
        for (const auto t : beats)
            g.fillRect (juce::Rectangle<float> (xOf (t), graph.getBottom() - 6.0f, 1.0f, 5.0f));
        g.setColour (ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::textFont (11.0f));
        g.drawText (juce::String (high, 1) + " BPM", graph.reduced (8.0f, 2.0f), juce::Justification::topLeft, false);
        g.drawText (juce::String (low, 1) + " BPM", graph.reduced (8.0f, 2.0f).withTrimmedBottom (6.0f), juce::Justification::bottomLeft, false);
        g.drawText (juce::String ((int) beats.size()) + " BEATS, " + juce::String (pr7s::medianBpm (beats), 1) + " BPM, CLARITY "
                        + juce::String (juce::roundToInt (confidence * 100.0)) + "%",
                    graph.reduced (8.0f, 2.0f), juce::Justification::topRight, false);
    }
    else
    {
        g.setColour (ParagonColours::textDim.withAlpha (0.6f));
        g.setFont (ParagonLookAndFeel::textFont (12.0f));
        g.drawText ("The tempo of each beat found shows here.", graph, juce::Justification::centred, false);
    }
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::textFont (13.0f));
    g.drawFittedText (status, juce::Rectangle<int> (16, getHeight() - 52, getWidth() - 32, 40), juce::Justification::centredLeft, 2, 0.9f);
}

void TempoDetectionPanel::resized()
{
    auto area = getLocalBounds().reduced (16, 12);
    area.removeFromTop (48);
    auto keys = area.removeFromTop (30);
    analyseKey.setBounds (keys.removeFromLeft (110));
    keys.removeFromLeft (14);
    for (auto* key : { &times2, &half, &fourThirds, &threeQuarters, &offbeat, &smooth })
    {
        key->setBounds (keys.removeFromLeft (key == &smooth ? 124 : key == &offbeat ? 84 : 66));
        keys.removeFromLeft (6);
    }
    resetKey.setBounds (keys.removeFromRight (80));
    area.removeFromTop (12);
    graph = area.withTrimmedBottom (56).toFloat();
}

TempoDetectionWindow::TempoDetectionWindow (pr7s::Studio& s, std::function<std::vector<int>()> parts)
    : DocumentWindow ("Paragon PR7-S: tempo detection", pageColour, DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    panel = new TempoDetectionPanel (s, std::move (parts));
    setContentOwned (panel, true);
    setResizable (true, false);
    setResizeLimits (TempoDetectionPanel::preferredWidth, 280, 3000, 1400);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
}

void TempoDetectionWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}
