#include "Sizing.h"

#include "Warp.h"

namespace pr7s
{
    const char* sizingName (Sizing s)
    {
        switch (s)
        {
            case Sizing::normal:        return "NORMAL SIZING";
            case Sizing::movesContents: return "SIZING MOVES CONTENTS";
            case Sizing::timeStretch:   return "SIZING APPLIES TIME STRETCH";
        }
        return "";
    }

    Sizing nextSizing (Sizing s)
    {
        return s == Sizing::normal ? Sizing::movesContents : s == Sizing::movesContents ? Sizing::timeStretch : Sizing::normal;
    }

    namespace StretchModes
    {
        const char* name (int mode)
        {
            switch (mode)
            {
                case voice: return "VOICE";
                case light: return "LIGHT";
                case tape:  return "TAPE";
                case rubberBand: return "RUBBER BAND";
                default:    return "CLEAN";
            }
        }

        const char* description (int mode)
        {
            switch (mode)
            {
                case voice: return "VOICE (pitch kept, and a voice's character: for vocals)";
                case light: return "LIGHT (pitch kept, quicker to work out, a little rougher)";
                case tape:  return "TAPE (no stretching: faster plays higher, slower plays lower)";
                case rubberBand: return "RUBBER BAND (pitch kept: the Rubber Band Library's finer engine, open source)";
                default:    return "CLEAN (pitch kept: the best for most things)";
            }
        }
    }

    void moveStartWithContents (Part& part, double newStart, const TempoMap& tempo)
    {
        const auto end = part.end();
        newStart = juce::jlimit (0.0, end - 1.0 / 64.0, newStart);
        if (part.isAudio() && part.isWarped())
        {
            // The markers go with the content: the old clip as if it had been moved to the new start, then
            // trimmed at its old end (what lies past its own end plays at its own speed).
            auto moved = part;
            moved.start = newStart;
            part.start = newStart;
            part.length = end - newStart;
            rebaseWarp (moved, part, tempo);
        }
        else
        {
            part.start = newStart;
            part.length = end - newStart;   // (notes, controllers and the offset all count from the start: unchanged)
        }
        if (part.isAudio())
        {
            const auto seconds = tempo.secondsBetween (part.start, part.end());
            part.fadeIn = juce::jlimit (0.0, seconds, part.fadeIn);
            part.fadeOut = juce::jlimit (0.0, seconds - part.fadeIn, part.fadeOut);
        }
    }

    void stretchPartTo (Part& part, double newStart, double newLength, const TempoMap& tempo)
    {
        newLength = juce::jmax (1.0 / 64.0, newLength);
        newStart = juce::jmax (0.0, newStart);
        const auto factor = part.length > 1.0e-9 ? newLength / part.length : 1.0;
        if (part.isAudio())
        {
            if (std::abs (factor - 1.0) > 1.0e-9)
            {
                ensureWarp (part, tempo);   // (its edges at its own speed, then scaled)
                for (auto& m : part.warp)
                    m.beat *= factor;
                if (! part.warp.empty())
                    part.warp.back().beat = newLength;
                part.fadeIn *= factor;
                part.fadeOut *= factor;
            }
        }
        else
        {
            for (auto& n : part.notes)
            {
                n.start *= factor;
                n.length *= factor;
            }
            for (auto& c : part.controllers)
                c.start *= factor;
        }
        part.start = newStart;
        part.length = newLength;
    }
}
