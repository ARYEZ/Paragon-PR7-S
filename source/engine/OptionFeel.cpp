#include "OptionFeel.h"
#include "SoloEngine.h"

#include <algorithm>
#include <array>
#include <set>
#include <cmath>

namespace pr7s
{
    juce::String optionFeelName (OptionFeel f)
    {
        switch (f)
        {
            case OptionFeel::straight: return "STRAIGHT";
            case OptionFeel::half: return "HALF TIME";
            case OptionFeel::doubleTime: return "DOUBLE TIME";
            case OptionFeel::broken: return "BROKEN";
            case OptionFeel::unknown: break;
        }
        return {};
    }

    juce::String energyName (int energy)
    {
        return energy == 1 ? "CALM" : energy == 2 ? "MEDIUM" : energy == 3 ? "HEAVY" : juce::String();
    }

    namespace
    {
        struct Onset
        {
            double beat = 0.0;
            float velocity = 0.0f;
        };

        // Notes starting within 1/48 beat of each other count as one hit (the loudest).
        std::vector<Onset> onsetsOf (std::vector<Onset> notes)
        {
            std::sort (notes.begin(), notes.end(), [] (const Onset& a, const Onset& b) { return a.beat < b.beat; });
            std::vector<Onset> out;
            for (const auto& n : notes)
            {
                if (! out.empty() && n.beat - out.back().beat < 1.0 / 48.0)
                    out.back().velocity = std::max (out.back().velocity, n.velocity);
                else
                    out.push_back (n);
            }
            return out;
        }

        bool onGrid (double beat, double step)
        {
            const auto x = beat / step;
            return std::abs (x - std::round (x)) < 0.08;
        }

        double spanOf (const LineIdea& idea)
        {
            auto end = 0.0;
            for (const auto& n : idea.notes)
                if (! n.keyswitch)
                    end = std::max (end, n.beat + std::max (n.length, 0.0));
            return end;
        }

        double meanVelocity (const std::vector<Onset>& onsets)
        {
            if (onsets.empty())
                return 0.0;
            auto sum = 0.0;
            for (const auto& o : onsets)
                sum += o.velocity;
            return sum / (double) onsets.size();
        }
    }

    OptionMeasure measureOption (const LineIdea& idea, double beatsPerBar, bool drums)
    {
        OptionMeasure m;
        const auto bpb = beatsPerBar > 0.0 ? beatsPerBar : 4.0;
        std::vector<Onset> all;
        for (const auto& n : idea.notes)
            if (! n.keyswitch)
                all.push_back ({ n.beat, n.velocity });
        const auto onsets = onsetsOf (all);
        if (onsets.empty())
            return m;
        const auto length = std::max ({ spanOf (idea), onsets.back().beat + 0.25, bpb });
        const auto beats = std::ceil (length / bpb - 1e-6) * bpb;
        const auto bars = beats / bpb;
        m.hitsPerBeat = (double) onsets.size() / beats;

        if (! drums)
        {
            // (0.158.0) Off the sixteenths (triplets, sextuplets, pushed) counts as broken; a line of sixteenths is busy,
            // not broken. Measured over the bars that play (a solo's opening rest doesn't make it calm).
            auto off = 0;
            for (const auto& o : onsets)
                off += onGrid (o.beat, 0.25) ? 0 : 1;
            m.offGrid = (double) off / (double) onsets.size();
            {
                std::set<int> playing;
                for (const auto& o : onsets)
                    playing.insert ((int) std::floor (o.beat / bpb));
                m.hitsPerBeat = (double) onsets.size() / (double (playing.size()) * bpb);
            }
            if (m.hitsPerBeat < 0.6)
                m.feel = OptionFeel::half;
            else if (m.hitsPerBeat > 2.6)
                m.feel = OptionFeel::doubleTime;   // (sixteenths are off the eighths half the time: not broken)
            else if (m.offGrid > 0.35)
                m.feel = OptionFeel::broken;
            else
                m.feel = OptionFeel::straight;
            const auto score = m.hitsPerBeat * meanVelocity (onsets);
            m.energy = score < 0.9 ? 1 : score < 2.2 ? 2 : 3;
            return m;
        }

        // (Measured against the composer's own drum options, the owner's songs' among them.) The bar in sixteenths of
        // a four-beat bar; its last beat left out (fills live there). The loud snares (and claps) say the feel; ghost
        // notes only when there are no loud ones.
        const auto sixteenth = bpb / 16.0;
        std::vector<Onset> kicks, loud, soft;
        for (const auto& n : idea.notes)
        {
            if (n.keyswitch)
                continue;
            if (n.pitch == 35 || n.pitch == 36)
                kicks.push_back ({ n.beat, n.velocity });
            else if (n.pitch == 38 || n.pitch == 39 || n.pitch == 40)
                (n.velocity >= 0.6f ? loud : soft).push_back ({ n.beat, n.velocity });
        }
        const auto snaresSoft = loud.empty();
        const auto snareHits = onsetsOf (snaresSoft ? soft : loud);
        const auto kickHits = onsetsOf (kicks);
        const auto barsCount = juce::jmax (1, (int) std::round (bars));
        std::vector<std::array<bool, 12>> inBar ((size_t) barsCount, std::array<bool, 12> {});
        // Every fourth bar left out too when there are four or more (a phrase's last bar holds its fill).
        const auto fillBar = [barsCount] (int bar) { return barsCount >= 4 && bar % 4 == 3; };
        auto early = 0, odd = 0;
        for (const auto& o : snareHits)
        {
            const auto pos = o.beat / sixteenth;
            const auto step = (int) std::round (pos);
            if (fillBar ((int) std::floor (pos / 16.0 + 1e-6)))
                continue;
            if (std::abs (pos - step) > 0.2)
            {
                ++early;
                ++odd;   // (off the sixteenths: pushed)
                continue;
            }
            const auto inThe = step % 16;
            if (inThe >= 12)
                continue;
            ++early;
            odd += inThe % 2;
            inBar[(size_t) juce::jlimit (0, barsCount - 1, step / 16)][(size_t) inThe] = true;
        }
        auto kickOdd = 0;
        auto kicksCounted = 0;
        for (const auto& o : kickHits)
        {
            const auto pos = o.beat / sixteenth;
            if (fillBar ((int) std::floor (pos / 16.0 + 1e-6)) || ((int) std::round (pos)) % 16 >= 12)
                continue;
            ++kicksCounted;
            kickOdd += (std::abs (pos - std::round (pos)) > 0.2 || ((int) std::round (pos)) % 2 == 1) ? 1 : 0;
        }
        const auto barsWith = [&] (std::initializer_list<int> steps)
        {
            auto n = 0;
            for (int b = 0; b < (int) inBar.size(); ++b)
            {
                if (fillBar (b))
                    continue;
                const auto& bar = inBar[(size_t) b];
                auto all = true;
                for (const auto st : steps)
                    all = all && bar[(size_t) st];
                n += all ? 1 : 0;
            }
            return n;
        };
        auto counted = 0;
        for (int b = 0; b < barsCount; ++b)
            counted += fillBar (b) ? 0 : 1;
        const auto half = counted / 2.0;
        m.offGrid = kicksCounted == 0 ? 0.0 : (double) kickOdd / (double) kicksCounted;

        if (! snaresSoft && (barsWith ({ 0, 4, 8 }) >= half || barsWith ({ 2, 6, 10 }) >= half))
            m.feel = OptionFeel::doubleTime;
        else if ((early > 0 && odd * 4 >= early) || (kicksCounted >= 4 && kickOdd * 4 >= kicksCounted))
            m.feel = OptionFeel::broken;
        else if (barsWith ({ 8 }) >= half && barsWith ({ 4 }) * 4 < counted)
            m.feel = OptionFeel::half;
        else if (snareHits.empty() && (double) kickHits.size() / beats <= 0.5)
            m.feel = OptionFeel::half;
        else
            m.feel = OptionFeel::straight;

        // Energy: the kick and snare count fully, everything else less; times how hard.
        const auto backbone = (double) (kickHits.size() + snareHits.size()) / beats;
        const auto others = juce::jmax (0.0, m.hitsPerBeat - backbone);
        const auto score = (backbone + 0.4 * others) * meanVelocity (onsets);
        m.energy = score < 1.0 ? 1 : score < 1.9 ? 2 : 3;
        return m;
    }

    //==============================================================================
    juce::StringArray typeGroupOrder()
    {
        return { "CHORUS TYPES", "VERSE TYPES", "PRE-CHORUS TYPES", "BRIDGE TYPES", "BREAKDOWN TYPES", "INTERLUDE TYPES",
                 "INTRO & OUTRO TYPES", "RIFF TYPES" };
    }

    juce::String typeGroupOf (const SectionType& t)
    {
        using B = SectionType::Band;
        switch (t.band)
        {
            case B::chorus: return "CHORUS TYPES";
            case B::verse: case B::staccato: case B::split: case B::clean: return "VERSE TYPES";
            case B::preChorus: return "PRE-CHORUS TYPES";
            case B::bridge: return "BRIDGE TYPES";
            case B::breakdown: return "BREAKDOWN TYPES";
            case B::interlude: return "INTERLUDE TYPES";
            case B::bookend: return "INTRO & OUTRO TYPES";
            case B::none: break;
        }
        return "RIFF TYPES";
    }

    namespace
    {
        // Whole words from their start ("RUN" finds RUNS, not GRUNGE; "SHORT BAR" a phrase).
        bool hasWord (const juce::String& text, std::initializer_list<const char*> words)
        {
            juce::String norm (" ");
            for (auto c : text.toUpperCase())
                norm += juce::CharacterFunctions::isLetterOrDigit (c) ? juce::String::charToString (c) : juce::String (" ");
            for (const auto* w : words)
                if (norm.contains (" " + juce::String (w).trim().toUpperCase()))
                    return true;
            return false;
        }
    }

    juce::StringArray transitionGroupOrder()
    {
        return { "STOPS & SILENCES", "SHORT BARS", "ROLLS & BUILDS", "WALKS & SLIDES", "DROP-OUTS", "HELD CHORDS", "HITS TOGETHER" };
    }

    juce::String transitionGroupOf (const juce::String& n)
    {
        if (hasWord (n, { "SHORT BAR" }))
            return "SHORT BARS";
        if (hasWord (n, { "STOP", "SILEN", "BREATH", "CUT", "BAND IN", "EMPTY" }))
            return "STOPS & SILENCES";
        if (hasWord (n, { "ROLL", "BUILD", "FILL", "PICKUP", "SWELL", "RISER", "CLIMB", "RUMBLE", "RAMP", "CASCADE", "GROWING",
                             "SNARE EIGHTHS", "SNARE SIXTEENTHS", "TOM SIXTEENTHS" }))
            return "ROLLS & BUILDS";
        if (hasWord (n, { "WALK", "SLIDE", "LINK", "BASS FIGURE", "PINCH", "RUN UP", "SCALE", "FALL" }))
            return "WALKS & SLIDES";
        if (hasWord (n, { "DROP", "ALONE", "BREAK", "OUT", "ONE GUITAR" }))
            return "DROP-OUTS";
        if (hasWord (n, { "RING", "RUNG", "HELD", "HOLD", "CHORD ON", "SUSTAIN", "TIED" }))
            return "HELD CHORDS";
        return "HITS TOGETHER";
    }

    juce::StringArray soloGroupOrder()
    {
        return { "BENDS", "MELODIC", "FAST RUNS & CELLS", "TAPPING & LEGATO", "TREMOLO & TRILLS", "WHAMMY & SQUEALS", "OTHER" };
    }

    juce::String soloGroupOf (SoloStyle style)
    {
        const auto n = soloStyleName (style) + " " + soloStyleDescription (style);
        const auto name = soloStyleName (style);
        if (hasWord (name, { "TAP", "LEGATO", "PULL-OFF", "PULL OFF", "THROW" }))
            return "TAPPING & LEGATO";
        if (hasWord (name, { "TREMOLO", "TRILL" }))
            return "TREMOLO & TRILLS";
        if (hasWord (name, { "DIVE", "WHAMMY", "SQUEAL", "SCREAM", "WAIL" }))
            return "WHAMMY & SQUEALS";
        if (hasWord (name, { "SEXTUPLET", "RUN", "SHRED", "SWEEP", "FIRE", "STREAM", "CELL", "SEQUENCE", "BURST", "ARPEGGIO", "FLASH", "CLIMB", "STAIR" }))
            return "FAST RUNS & CELLS";
        if (hasWord (name, { "BEND" }))
            return "BENDS";
        if (hasWord (n, { "MELOD", "HOOK", "BALLAD", "THEME", "SING", "HYMN", "CHANT", "HEAD", "LINE" }))
            return "MELODIC";
        if (hasWord (n, { "BEND" }))
            return "BENDS";
        return "OTHER";
    }

    //==============================================================================
    bool OptionMemory::isFavourite (const juce::String& list, const juce::String& name) const
    {
        const auto found = favourites.find (list);
        return found != favourites.end() && found->second.contains (name);
    }

    void OptionMemory::setFavourite (const juce::String& list, const juce::String& name, bool on)
    {
        auto& names = favourites[list];
        if (on)
            names.addIfNotAlreadyThere (name);
        else
            names.removeString (name);
        if (names.isEmpty())
            favourites.erase (list);
    }

    void OptionMemory::noteUsed (const juce::String& list, const juce::String& name)
    {
        if (name.isEmpty())
            return;
        auto& names = recent[list];
        names.removeString (name);
        names.insert (0, name);
        while (names.size() > recentKept)
            names.remove (names.size() - 1);
    }

    juce::StringArray OptionMemory::favouritesOf (const juce::String& list) const
    {
        const auto found = favourites.find (list);
        return found == favourites.end() ? juce::StringArray() : found->second;
    }

    juce::StringArray OptionMemory::recentOf (const juce::String& list) const
    {
        const auto found = recent.find (list);
        return found == recent.end() ? juce::StringArray() : found->second;
    }

    juce::String OptionMemory::toText (const std::map<juce::String, juce::StringArray>& lists)
    {
        juce::StringArray lines;
        for (const auto& [list, names] : lists)
            for (const auto& n : names)
                lines.add (list + "|" + n);
        return lines.joinIntoString ("\n");
    }

    std::map<juce::String, juce::StringArray> OptionMemory::fromText (const juce::String& text)
    {
        std::map<juce::String, juce::StringArray> out;
        for (const auto& line : juce::StringArray::fromLines (text))
        {
            const auto list = line.upToFirstOccurrenceOf ("|", false, false).trim();
            const auto name = line.fromFirstOccurrenceOf ("|", false, false).trim();
            if (list.isNotEmpty() && name.isNotEmpty())
                out[list].addIfNotAlreadyThere (name);
        }
        return out;
    }
}
