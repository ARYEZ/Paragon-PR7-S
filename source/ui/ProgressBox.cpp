#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

#include "ProgressBox.h"

#include "engine/SessionLog.h"
#include "ui/LevelMeter.h"
#include "ui/Logo.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    // Windows calls a window "Not Responding" (and greys it) when its thread
    // hasn't looked at its messages for 5 seconds. Looking without taking any
    // (PM_NOREMOVE) is enough, and runs nothing else meanwhile.
    void stayResponsive()
    {
       #if defined (_WIN32)
        MSG message;
        PeekMessageW (&message, nullptr, 0, 0, PM_NOREMOVE);
       #endif
    }
}

ProgressBox::ProgressBox()
{
    setSize (460, 150);
    setOpaque (false);
}

void ProgressBox::set (double newProgress, const juce::String& newWhat, juce::Component* over)
{
    progress = juce::jlimit (0.0, 1.0, newProgress);
    what = newWhat;
    pr7s::SessionLog::beat();   // (busy, not frozen)
    setWantsKeyboardFocus (true);   // (modal while a render runs: the keys come here, and go nowhere)
    if (! isOnDesktop())
    {
        addToDesktop (juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowHasDropShadow);
        setAlwaysOnTop (true);
        if (auto* top = over != nullptr ? over->getTopLevelComponent() : nullptr)
            setCentrePosition (top->getScreenBounds().getCentre());
        else
            centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }
    // Drawn now, at most 30 times a second.
    const auto now = juce::Time::getMillisecondCounter();
    if (now - lastDrawn < 33 && progress < 1.0)
        return;
    lastDrawn = now;
    repaint();
    // (Only in the app: the tests' virtual screen can't be drawn to this way.)
    if (juce::JUCEApplicationBase::getInstance() != nullptr)
        if (auto* peer = getPeer())
            peer->performAnyPendingRepaintsNow();
    stayResponsive();
}

void ProgressBox::paintBar (juce::Graphics& g, juce::Rectangle<float> area, double value)
{
    LevelMeter::paintSegments (g, area, (float) juce::jlimit (0.0, 1.0, value), -1.0f, false, 4.0f);
}

void ProgressBox::paint (juce::Graphics& g)
{
    // The question boxes' look (0.9.10): black pearl, a red glow along the edge.
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff0c0a0d));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (ParagonColours::glow.withAlpha (0.35f));
    g.drawRoundedRectangle (bounds.reduced (1.5f), 7.0f, 3.0f);
    g.setColour (ParagonColours::glow);
    g.drawRoundedRectangle (bounds, 8.0f, 1.2f);

    auto area = bounds.reduced (22.0f, 18.0f);
    auto top = area.removeFromTop (34.0f);
    Logo::drawMark (g, top.removeFromLeft (34.0f).reduced (2.0f), juce::Justification::centredLeft, { juce::Colours::white, ParagonColours::glow, {}, {}, 1.0f, true });
    top.removeFromLeft (12.0f);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (20.0f));
    g.drawText (what, top, juce::Justification::centredLeft, true);

    area.removeFromTop (14.0f);
    auto barRow = area.removeFromTop (26.0f);
    auto percent = barRow.removeFromRight (64.0f);
    barRow.removeFromRight (10.0f);
    paintBar (g, barRow, progress);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (20.0f));
    g.drawText (juce::String (juce::roundToInt (progress * 100.0)) + "%", percent, juce::Justification::centredRight, false);
    area.removeFromTop (10.0f);
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::textFont (12.0f));
    g.drawText ("The studio carries on when it's done.", area, juce::Justification::centredLeft, true);
}
