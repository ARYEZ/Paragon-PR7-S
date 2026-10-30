#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"
#include "ui/ParagonLookAndFeel.h"

// A track's (or the master's) R and W keys, wherever they're shown (the
// track list, the mixer): R reads its automation, W writes it (and turns R
// on). Lit as the manual has W: red when it's on, orange while it's writing
// (R lights pink, so the two are told apart: the family's colours only).
namespace AutomationKeys
{
    inline const juce::Colour readColour { 0xffff2f7a }, writeColour { 0xffe0103c }, writingColour { 0xffff7a1a };

    inline void setUp (juce::TextButton& read, juce::TextButton& write, pr7s::Studio& studio, int busId)
    {
        for (auto* key : { &read, &write })
        {
            key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
            key->setWantsKeyboardFocus (false);
        }
        read.getProperties().set (ParagonLookAndFeel::litColourProperty, (juce::int64) readColour.getARGB());
        read.setTooltip ("R: read automation (play back what's been written or drawn)");
        write.setTooltip ("W: write automation (while playing, what you do to its controls is recorded; turns R on too)");
        read.onClick = [&studio, busId] { studio.setAutomationRead (busId, ! studio.getAutomationRead (busId)); };
        write.onClick = [&studio, busId] { studio.setAutomationWrite (busId, ! studio.getAutomationWrite (busId)); };
    }

    inline void refresh (juce::TextButton& read, juce::TextButton& write, const pr7s::Studio& studio, int busId)
    {
        read.setToggleState (studio.getAutomationRead (busId), juce::dontSendNotification);
        write.setToggleState (studio.getAutomationWrite (busId), juce::dontSendNotification);
        const auto writing = studio.isWritingAutomation (busId);
        write.getProperties().set (ParagonLookAndFeel::litColourProperty, (juce::int64) (writing ? writingColour : writeColour).getARGB());
        write.repaint();
    }
}
