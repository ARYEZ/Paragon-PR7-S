#include "TransportBar.h"

#include "ui/LedDisplay.h"
#include "ui/Logo.h"
#include "ui/Pearl.h"

namespace
{
    const auto orange = LedDisplay::ledOrange;
    constexpr float screenSize = 0.55f;

    juce::FontOptions font (float height, int style = juce::Font::bold) { return ParagonLookAndFeel::textFont (height, style); }

    void stamped (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height,
                  juce::Justification justification = juce::Justification::centred)
    {
        g.setFont (font (height));
        ParagonLookAndFeel::drawStampedText (g, text, area, justification, Finish::ink());
    }

    // The signatures offered (the common ones in pop, rock and metal).
    const std::pair<int, int> signatures[] = { { 4, 4 }, { 3, 4 }, { 2, 4 }, { 5, 4 }, { 6, 4 }, { 7, 4 }, { 6, 8 }, { 7, 8 }, { 9, 8 }, { 12, 8 } };
}

TransportBar::TransportBar (pr7s::Studio& studioToUse)
    : studio (studioToUse),
      leftMeter (studioToUse.getEngine().getOutputMeter (0)),
      rightMeter (studioToUse.getEngine().getOutputMeter (1))
{
    for (auto* key : { &toStart, &stopKey, &playKey, &recordKey, &cycleKey, &clickKey })
    {
        key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        addAndMakeVisible (*key);
    }
    toStart.setTooltip ("Back to the start (or the cycle's start). Home key");
    stopKey.setTooltip ("Stop. Press again to go back to where playing started. Space toggles play and stop");
    playKey.setTooltip ("Play. Space");
    recordKey.setTooltip ("Record into the armed track (red R in the track list). R key. Press again while recording to stop recording and keep playing");
    cycleKey.setTooltip ("CYCLE: play round the range marked on the ruler (drag across the ruler's top strip to set it). / key");
    clickKey.setTooltip ("CLICK: the metronome, on every beat (higher on the first of each bar). C key");

    toStart.onClick = [this] { studio.goToStart(); };
    stopKey.onClick = [this] { studio.stop(); };
    playKey.onClick = [this] { studio.play(); };
    recordKey.onClick = [this] { studio.record(); };
    cycleKey.onClick = [this] { studio.setCycleOn (! studio.getProject().loopOn); };
    clickKey.onClick = [this] { studio.setClick (! studio.getProject().click); };

    for (auto* meter : { &leftMeter, &rightMeter })
    {
        meter->setShowScale (false);
        addAndMakeVisible (*meter);
    }

    studio.addChangeListener (this);
    Finish::changes().addChangeListener (this);
    refreshKeys();
    startTimerHz (30);
}

TransportBar::~TransportBar()
{
    Finish::changes().removeChangeListener (this);
    studio.removeChangeListener (this);
}

void TransportBar::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &Finish::changes())
        resized();   // the grain differs on black
    refreshKeys();
    repaint();
}

void TransportBar::timerCallback()
{
    // The keys follow the engine (playing can stop by itself, e.g. on a new project).
    refreshKeys();
    repaint (positionScreen.expanded (4.0f).getSmallestIntegerContainer());
    followPerformance();
}

void TransportBar::followPerformance()
{
    // The average as it is; the peak held a moment, then falling (as the level meters' peaks do).
    const auto now = studio.getEngine().takePerformance();
    shownAverage = now.average;
    shownPeak = juce::jmax (now.peak, shownPeak * 0.92f);
    overloadLit = now.overloads > 0;
    repaint (cpuScreen.expanded (4.0f).getSmallestIntegerContainer());
}

juce::String TransportBar::getTooltip()
{
    if (! cpuScreen.contains (getMouseXYRelative().toFloat()))
        return {};
    const auto overloads = studio.getEngine().takePerformance().overloads;
    return "CPU: the audio processing load. Average " + juce::String (juce::roundToInt (shownAverage * 100.0f)) + "%, peak "
         + juce::String (juce::roundToInt (shownPeak * 100.0f)) + "% (of the time each block has). "
         + (overloads > 0 ? juce::String (overloads) + " overload" + (overloads == 1 ? "" : "s") + " (dropouts): click to reset. "
                                + "A bigger buffer in SETTINGS, or fewer effects and instruments, helps."
                          : juce::String ("No overloads."));
}

void TransportBar::setToolsWidth (int width)
{
    if (width == toolsWidth)
        return;
    toolsWidth = width;
    resized();
    repaint();
}

void TransportBar::refreshKeys()
{
    auto& engine = studio.getEngine();
    const auto& project = studio.getProject();
    playKey.setToggleState (engine.isPlaying(), juce::dontSendNotification);
    stopKey.setToggleState (! engine.isPlaying(), juce::dontSendNotification);
    recordKey.setToggleState (engine.isRecording(), juce::dontSendNotification);
    cycleKey.setToggleState (project.loopOn, juce::dontSendNotification);
    clickKey.setToggleState (project.click, juce::dontSendNotification);
}

juce::String TransportBar::getPositionText() const
{
    return pr7s::formatBarsBeats (studio.getEngine().getPosition(), studio.getProject());
}

juce::String TransportBar::getTempoText() const
{
    return juce::String (studio.getProject().bpm, 2);
}

void TransportBar::resized()
{
    satin = Finish::makeSatinGrain (juce::jmax (1, getWidth() * 2), juce::jmax (1, getHeight() * 2), 7071);

    // Left to right: the badge; AUTO-SCROLL (its key and menu arrow: the
    // main window puts them in `autoScrollArea`); the keys; the CPU screen;
    // the position screen; the tempo and the signature; on a wide window the
    // tools (`toolsArea`); the output meters at the right end.
    const auto wide = toolsWidth > 0;
    const auto keyPitch = wide ? 66.0f : 70.0f;
    const auto autoWidth = keyPitch + 3.0f + 20.0f;
    const auto cpuWidth = wide ? 92.0f : 96.0f;
    const auto tempoWidth = wide ? 136.0f : 150.0f;
    const auto signatureWidth = wide ? 88.0f : 96.0f;

    auto area = getLocalBounds().toFloat().reduced (14.0f, 12.0f);
    badge = area.removeFromLeft (! wide && getWidth() < 1700 ? 150.0f : 190.0f);
    area.removeFromLeft (16.0f);
    // The output meters: two LED columns, L and R, with OUT stamped under them.
    meterArea = area.removeFromRight (36.0f);
    const auto columns = meterArea.withTrimmedTop (10.0f).withTrimmedBottom (14.0f);
    leftMeter.setBounds (columns.withWidth (10.0f).translated (6.0f, 0.0f).toNearestInt());
    rightMeter.setBounds (columns.withWidth (10.0f).translated (20.0f, 0.0f).toNearestInt());
    area.removeFromRight (16.0f);

    // The group from AUTO-SCROLL to the signature (and the tools): on one
    // screen it starts after the badge and the position screen takes what's
    // left; wide, the position screen has its own width and the group sits
    // in the middle (as near as the badge and the meters allow), so on three
    // screens it's all on the middle one.
    const auto leftWidth = autoWidth + 12.0f + 6.0f * keyPitch + 12.0f + cpuWidth + 12.0f;
    const auto rightWidth = 12.0f + tempoWidth + 12.0f + signatureWidth + (wide ? 20.0f + (float) toolsWidth : 0.0f);
    auto x = area.getX();
    auto positionWidth = area.getWidth() - leftWidth - rightWidth;
    if (wide)
    {
        positionWidth = (float) widePositionWidth;
        const auto groupWidth = leftWidth + positionWidth + rightWidth;
        x = juce::jlimit (area.getX(), juce::jmax (area.getX(), area.getRight() - groupWidth), ((float) getWidth() - groupWidth) * 0.5f);
    }
    juce::Rectangle<float> row (x, area.getY(), leftWidth + positionWidth + rightWidth, area.getHeight());

    autoScrollArea = row.removeFromLeft (autoWidth).withSizeKeepingCentre (autoWidth, 40.0f).translated (0.0f, -6.0f).toNearestInt();
    autoScrollStamp = juce::Rectangle<float> ((float) autoScrollArea.getX(), area.getBottom() - 14.0f, keyPitch - 8.0f, 14.0f);
    row.removeFromLeft (12.0f);
    keysArea = row.removeFromLeft (6.0f * keyPitch);
    auto keys = keysArea;
    for (auto* key : { &toStart, &stopKey, &playKey, &recordKey, &cycleKey, &clickKey })
        key->setBounds (keys.removeFromLeft (keyPitch).withSizeKeepingCentre (keyPitch - 8.0f, 40.0f).translated (0.0f, -6.0f).toNearestInt());
    row.removeFromLeft (12.0f);
    cpuScreen = row.removeFromLeft (cpuWidth);
    row.removeFromLeft (12.0f);
    positionScreen = row.removeFromLeft (positionWidth);
    row.removeFromLeft (12.0f);
    tempoScreen = row.removeFromLeft (tempoWidth);
    row.removeFromLeft (12.0f);
    signatureScreen = row.removeFromLeft (signatureWidth);
    toolsArea = {};
    if (wide)
    {
        row.removeFromLeft (20.0f);
        toolsArea = row.removeFromLeft ((float) toolsWidth).withSizeKeepingCentre ((float) toolsWidth, 40.0f)
                       .withY ((float) toStart.getBounds().getY()).toNearestInt();
    }
    keyPitchNow = keyPitch;
}

void TransportBar::paint (juce::Graphics& g)
{
    Pearl::paintUnit (g, getLocalBounds().toFloat().reduced (4.0f), satin);
    glasses.clear();

    // The badge: the PR7-S mark and what it is, printed in red.
    {
        auto b = badge;
        Logo::drawMark (g, b.removeFromTop (b.getHeight() * 0.62f).reduced (0.0f, 6.0f), juce::Justification::centredLeft,
                        { Finish::ink(), juce::Colour (0xffe0103c), Finish::stampEdge(), {}, 1.0f, false });
        g.setColour (Finish::printedRed());
        g.setFont (font (10.0f));
        g.drawText ("RECORDING STUDIO", b, juce::Justification::topLeft, false);
    }

    // The keys' shortcuts.
    {
        auto keys = keysArea;
        // Under each key, its shortcut on the computer's keyboard (AUTO-SCROLL's: F).
        for (const auto* name : { "HOME", "SPACE", "SPACE", "R", "/", "C" })
            stamped (g, name, keys.removeFromLeft (keyPitchNow).removeFromBottom (14.0f), 9.0f);
        stamped (g, "F", autoScrollStamp, 9.0f);
    }

    // POSITION: bars, beats, sixteenths and ticks, then minutes and seconds.
    {
        const auto glass = LedDisplay::paintScreen (g, positionScreen, screenSize);
        glasses.push_back (glass);
        auto inside = glass.reduced (12.0f, 6.0f);
        g.setColour (orange.withAlpha (0.6f));
        g.setFont (font (9.0f));
        g.drawText ("POSITION", inside.removeFromTop (12.0f), juce::Justification::topLeft, false);
        const auto beats = studio.getEngine().getPosition();
        auto row = inside;
        g.setColour (orange);
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), juce::jmin (30.0f, row.getHeight() * 0.62f), juce::Font::bold));
        g.drawFittedText (getPositionText(), row.removeFromLeft (row.getWidth() * 0.62f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        g.setColour (orange.withAlpha (0.75f));
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), juce::jmin (18.0f, row.getHeight() * 0.42f), juce::Font::plain));
        g.drawFittedText (pr7s::formatTime (beats * studio.getProject().secondsPerBeat()), row.toNearestInt(), juce::Justification::centredRight, 1, 0.7f);
    }

    // CPU: the audio processing load, as much of each block's time as it
    // takes (the average, big; the peak, small); OVER lit after an overload.
    {
        const auto glass = LedDisplay::paintScreen (g, cpuScreen, screenSize);
        glasses.push_back (glass);
        auto inside = glass.reduced (9.0f, 6.0f);
        auto top = inside.removeFromTop (12.0f);
        g.setFont (font (9.0f));
        g.setColour (orange.withAlpha (0.6f));
        g.drawText ("CPU", top, juce::Justification::topLeft, false);
        g.setColour (overloadLit ? ParagonColours::meterRed : orange.withAlpha (0.18f));
        g.drawText ("OVER", top, juce::Justification::topRight, false);
        const auto percent = [] (float load) { return juce::String (juce::roundToInt (juce::jlimit (0.0f, 9.99f, load) * 100.0f)); };
        auto bottom = inside.removeFromBottom (12.0f);
        g.setColour (orange.withAlpha (0.75f));
        g.drawText ("PEAK " + percent (shownPeak) + "%", bottom, juce::Justification::bottomRight, false);
        g.setColour (shownAverage >= 1.0f ? ParagonColours::meterRed : orange);
        g.setFont (font (juce::jmin (26.0f, inside.getHeight() * 0.95f)));
        g.drawFittedText (percent (shownAverage) + "%", inside.toNearestInt(), juce::Justification::centredRight, 1, 0.7f);
    }

    // TEMPO and SIGNATURE.
    {
        const auto glass = LedDisplay::paintScreen (g, tempoScreen, screenSize);
        glasses.push_back (glass);
        auto inside = glass.reduced (10.0f, 6.0f);
        g.setColour (orange.withAlpha (0.6f));
        g.setFont (font (9.0f));
        g.drawText ("TEMPO", inside.removeFromTop (12.0f), juce::Justification::topLeft, false);
        g.setColour (orange);
        g.setFont (font (juce::jmin (28.0f, inside.getHeight() * 0.62f)));
        g.drawFittedText (getTempoText(), inside.withTrimmedRight (30.0f).toNearestInt(), juce::Justification::centredRight, 1, 0.7f);
        g.setFont (font (10.0f));
        g.drawText ("BPM", inside.removeFromRight (28.0f), juce::Justification::centredRight, false);
    }
    {
        const auto glass = LedDisplay::paintScreen (g, signatureScreen, screenSize);
        glasses.push_back (glass);
        auto inside = glass.reduced (10.0f, 6.0f);
        g.setColour (orange.withAlpha (0.6f));
        g.setFont (font (9.0f));
        g.drawText ("SIGNATURE", inside.removeFromTop (12.0f), juce::Justification::topLeft, false);
        g.setColour (orange);
        g.setFont (font (juce::jmin (28.0f, inside.getHeight() * 0.62f)));
        const auto& project = studio.getProject();
        g.drawFittedText (juce::String (project.numerator) + "/" + juce::String (project.denominator), inside.toNearestInt(),
                          juce::Justification::centred, 1, 0.7f);
    }
    stamped (g, "L", juce::Rectangle<float> (meterArea.getX() + 4.0f, meterArea.getY(), 14.0f, 10.0f), 8.0f);
    stamped (g, "R", juce::Rectangle<float> (meterArea.getX() + 18.0f, meterArea.getY(), 14.0f, 10.0f), 8.0f);
    stamped (g, "OUT", meterArea.withTrimmedTop (meterArea.getHeight() - 12.0f), 8.0f);

}

void TransportBar::paintOverChildren (juce::Graphics& g)
{
    for (const auto& glass : glasses)
        LedDisplay::paintGlassOverlay (g, glass, screenSize);
}

void TransportBar::mouseDown (const juce::MouseEvent& event)
{
    if (cpuScreen.contains (event.position))
    {
        studio.getEngine().resetOverloads();   // (as the manual has it: a click resets the overload light)
        followPerformance();
        return;
    }
    if (tempoScreen.contains (event.position))
    {
        draggingTempo = true;
        dragStartBpm = studio.getProject().bpm;
        return;
    }
    if (signatureScreen.contains (event.position))
    {
        juce::PopupMenu menu;
        const auto& project = studio.getProject();
        for (size_t i = 0; i < std::size (signatures); ++i)
        {
            const auto [numerator, denominator] = signatures[i];
            menu.addItem ((int) i + 1, juce::String (numerator) + "/" + juce::String (denominator), true,
                          project.numerator == numerator && project.denominator == denominator);
        }
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (
                                localAreaToGlobal (signatureScreen.toNearestInt())),
                            [this] (int chosen)
                            {
                                if (chosen > 0)
                                    studio.setSignature (signatures[(size_t) chosen - 1].first, signatures[(size_t) chosen - 1].second);
                            });
    }
}

void TransportBar::mouseDrag (const juce::MouseEvent& event)
{
    if (! draggingTempo)
        return;
    // Up for faster: 1 BPM every 3 pixels (a tenth of that with shift held).
    const auto step = event.mods.isShiftDown() ? 0.1 : 1.0;
    studio.setTempo (std::round ((dragStartBpm - event.getDistanceFromDragStartY() / 3.0 * step) / step) * step);
}

void TransportBar::mouseUp (const juce::MouseEvent&)
{
    draggingTempo = false;
}

void TransportBar::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (! tempoScreen.contains (event.position))
        return;
    auto* window = new juce::AlertWindow ("Tempo", "The song's tempo, in beats per minute (20 to 300):", juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("bpm", juce::String (studio.getProject().bpm, 2));
    window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    window->enterModalState (true, juce::ModalCallbackFunction::create ([this, window] (int result)
    {
        if (result == 1)
            if (const auto bpm = window->getTextEditorContents ("bpm").getDoubleValue(); bpm > 0.0)
                studio.setTempo (bpm);
    }), true);
}
