#pragma once

#include "Lines.h"

#include <juce_core/juce_core.h>

#include <map>

namespace pr7s
{
    // (0.156.0, the owner chose mock-up D with C's feel filters) Each option a lane offers, measured from the part it
    // writes, so the option browser can narrow a long list by how it feels: our own measures, made from the notes.
    //
    //   FEEL     straight: hits on the beats and their halves; half time: few hits a bar (a drum part: the snare on 3
    //            only, not 2 and 4); double time: a hit on most sixteenths, or the snare on every beat or eighth;
    //            broken: many hits off the eighths' grid or pushed against the beat.
    //   ENERGY   calm / medium / heavy by hits a beat, weighted by how hard they're played.
    enum class OptionFeel { unknown, straight, half, doubleTime, broken };
    juce::String optionFeelName (OptionFeel);   // "STRAIGHT", "HALF TIME", "DOUBLE TIME", "BROKEN"

    struct OptionMeasure
    {
        OptionFeel feel = OptionFeel::unknown;
        int energy = 0;   // 1 calm, 2 medium, 3 heavy; 0 not measured
        double hitsPerBeat = 0.0;
        double offGrid = 0.0;   // the share of hits off the eighths
    };

    // `drums`: the notes are General MIDI drums (a part of the drums lane).
    OptionMeasure measureOption (const LineIdea&, double beatsPerBar, bool drums);
    juce::String energyName (int energy);   // "CALM", "MEDIUM", "HEAVY"

    // The favourites and the last few used, per list ("lane:drums", "type:chorus", ...), by name. Kept by Studio in the
    // settings file as lines "list|name".
    struct OptionMemory
    {
        static constexpr int recentKept = 4;
        std::map<juce::String, juce::StringArray> favourites, recent;

        bool isFavourite (const juce::String& list, const juce::String& name) const;
        void setFavourite (const juce::String& list, const juce::String& name, bool on);
        void noteUsed (const juce::String& list, const juce::String& name);   // to the front, at most recentKept
        juce::StringArray favouritesOf (const juce::String& list) const;
        juce::StringArray recentOf (const juce::String& list) const;

        static juce::String toText (const std::map<juce::String, juce::StringArray>&);
        static std::map<juce::String, juce::StringArray> fromText (const juce::String&);
    };
}
