#include "LevelMeter.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include "ParagonLookAndFeel.h"

namespace
{
    constexpr int refreshHz = 30;
    constexpr float fallDbPerSecond = 24.0f;   // how fast the meter drops after a peak
    constexpr int holdFrames = refreshHz * 3 / 2;   // a peak stays lit for 1.5 s
}

LevelMeter::LevelMeter (LevelMeterSource& sourceToUse) : source (sourceToUse)
{
    startTimerHz (refreshHz);
}

float LevelMeter::proportionForDb (float db) noexcept
{
    return juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
}

bool LevelMeter::Ballistics::push (float peakGain)
{
    const auto peakDb = juce::Decibels::gainToDecibels (peakGain, minDb);
    const auto newDb = juce::jmax (peakDb, db - fallDbPerSecond / (float) refreshHz, minDb);
    auto newHeld = heldDb;
    if (peakDb >= heldDb)
    {
        newHeld = peakDb;
        heldFor = holdFrames;
    }
    else if (heldFor > 0)
    {
        --heldFor;
    }
    else
    {
        newHeld = newDb;   // the hold's over: it falls with the meter
    }
    const auto changed = ! juce::approximatelyEqual (newDb, db) || ! juce::approximatelyEqual (newHeld, heldDb);
    db = newDb;
    heldDb = newHeld;
    return changed;
}

void LevelMeter::timerCallback()
{
    if (reading.push (source.takePeak()))
        repaint();
}

void LevelMeter::paintSegments (juce::Graphics& g, juce::Rectangle<float> area, float lit, float held, bool vertical, float pitch)
{
    g.setColour (juce::Colour (0xff050405));   // the glass
    g.fillRect (area);
    const auto length = vertical ? area.getHeight() : area.getWidth();
    const auto count = juce::jmax (1, (int) (length / pitch));
    const auto step = length / (float) count;
    const auto heldSegment = held > 0.0f ? juce::jlimit (0, count - 1, (int) (held * (float) count)) : -1;
    for (int i = 0; i < count; ++i)
    {
        const auto proportion = ((float) i + 0.5f) / (float) count;
        const auto colour = ParagonColours::meterColour (proportion);
        const auto on = proportion <= lit || i == heldSegment;
        const auto segment = vertical ? juce::Rectangle<float> (area.getX(), area.getBottom() - (float) (i + 1) * step + 0.6f, area.getWidth(), step - 1.2f)
                                      : juce::Rectangle<float> (area.getX() + (float) i * step + 0.6f, area.getY(), step - 1.2f, area.getHeight());
        if (on)
        {
            g.setColour (colour.withAlpha (0.25f));
            g.fillRect (vertical ? segment.expanded (1.0f, 0.6f) : segment.expanded (0.6f, 1.0f));
        }
        g.setColour (on ? colour : colour.withAlpha (0.1f));
        g.fillRect (segment);
    }
}

void LevelMeter::paint (juce::Graphics& g)
{
    const auto held = reading.heldDb > minDb ? proportionForDb (reading.heldDb) : -1.0f;
    paintSegments (g, getLocalBounds().toFloat().reduced (0.0f, 2.0f), proportionForDb (reading.db), held);
}
