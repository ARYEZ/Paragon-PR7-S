#include "engine/SectionPlan.h"

namespace pr7s
{
    juce::String sectionWord (DrumSection s)
    {
        const auto name = drumSectionName (s);
        return name.substring (0, 1) + name.substring (1).toLowerCase();
    }

    std::vector<SectionSuggestion> suggestNextSections (const std::vector<BuiltSection>& song, GuitarStyle style, int riffCount)
    {
        using S = DrumSection;
        std::vector<SectionSuggestion> out;
        if (song.empty() || song.back().kind == S::ending)
            return out;
        const auto& last = song.back();
        const auto count = [&] (S kind) { return (int) std::count_if (song.begin(), song.end(), [kind] (const BuiltSection& b) { return b.kind == kind; }); };
        const auto lastOf = [&] (S kind) -> const BuiltSection*
        {
            for (auto it = song.rbegin(); it != song.rend(); ++it)
                if (it->kind == kind)
                    return &*it;
            return nullptr;
        };
        const auto heavy = style != GuitarStyle::rock;
        const auto verses = count (S::verse), choruses = count (S::chorus);
        const auto bridged = count (S::bridge) > 0, brokenDown = count (S::breakdown) > 0, soloed = count (S::solo) > 0;
        const auto riffs = juce::jmax (1, riffCount);
        const auto newRiff = [&] (int from, int step) { return (from + step) % riffs; };
        const auto clamp = [] (float v) { return juce::jlimit (0.0f, 1.0f, v); };

        // Each kind: how it follows from the song so far.
        const auto add = [&] (S kind, float score, const juce::String& reason, int riff, RiffSettings settings)
        {
            if (score <= 0.0f)
                return;
            out.push_back ({ kind, juce::jlimit (0.0f, 1.0f, score), reason, riff, settings.limited(), last.variation });
        };
        const auto opened = [&] (const BuiltSection& from)
        {
            auto r = from.riffSettings;
            r.palmMutes = clamp (r.palmMutes - 0.3f);
            r.tightness = clamp (r.tightness - 0.15f);
            r.fills = true;
            return r;
        };
        const auto chugs = [&] (const BuiltSection& from)
        {
            auto r = from.riffSettings;
            r.palmMutes = clamp (r.palmMutes + 0.35f);
            r.tightness = clamp (r.tightness + 0.3f);
            r.busyness = clamp (r.busyness + 0.25f);
            r.bars = 1;
            return r;
        };

        // Where the chorus's riff and settings come from: the chorus before, else the verse opened up.
        const auto* chorusBefore = lastOf (S::chorus);
        const auto* verseBefore = lastOf (S::verse);
        const auto chorusFromVerse = [&] (float score)
        {
            const auto& from = verseBefore != nullptr ? *verseBefore : last;
            add (S::chorus, score, "Bigger: the verse's riff opened up (fewer palm mutes, ringing, a fill into it), crashes and the ride or china",
                 from.riff, opened (from));
            add (S::chorus, score - 0.12f, "Bigger, and a new riff for contrast with the verse", newRiff (from.riff, 1), opened (from));
        };
        const auto chorusAgain = [&] (float score)
        {
            if (chorusBefore != nullptr)
                add (S::chorus, score, "The chorus again, as before (its riff and settings)", chorusBefore->riff, chorusBefore->riffSettings);
            else
                chorusFromVerse (score);
        };
        const auto verseAgain = [&] (float score)
        {
            if (verseBefore != nullptr)
                add (S::verse, score, "Back to the verse: its riff and settings as before", verseBefore->riff, verseBefore->riffSettings);
            else
                add (S::verse, score, "The verse: the intro's riff, tighter", last.riff, chugs (last));
        };
        const auto breakdown = [&] (float score)
        {
            const auto& from = chorusBefore != nullptr ? *chorusBefore : last;
            add (S::breakdown, score, "Heavier: the same riff tightened into chugs, half time and the china", from.riff, chugs (from));
        };
        const auto bridge = [&] (float score)
        {
            auto r = last.riffSettings;
            r.palmMutes = clamp (r.palmMutes - 0.2f);
            r.busyness = clamp (r.busyness - 0.2f);
            add (S::bridge, score, "Somewhere new: a new riff, looser, half time on the ride bell", newRiff (last.riff, 2), r);
        };
        // (0.65.0) A guitar solo: over the chorus's riff (the studio thins the rhythm out under it).
        const auto solo = [&] (float score)
        {
            if (soloed)
                return;
            const auto& from = chorusBefore != nullptr ? *chorusBefore : last;
            add (S::solo, score, "A guitar solo: the lead builds from melodic to its fastest, the riff thinned out under it, the drums on the ride",
                 from.riff, from.riffSettings);
        };
        const auto ending = [&] (float score)
        {
            const auto& from = chorusBefore != nullptr ? *chorusBefore : last;
            add (S::ending, score, "The end: the last chorus's riff, a fill and the last hit", from.riff, from.riffSettings);
        };

        switch (last.kind)
        {
            case S::intro:
                verseAgain (0.9f);
                chorusFromVerse (0.45f);
                break;
            case S::verse:
                chorusFromVerse (0.9f);
                if (choruses == 0 && verses == 1)
                    verseAgain (0.45f);
                if (heavy)
                    breakdown (choruses > 0 ? 0.55f : 0.3f);
                if (choruses > 0)
                    bridge (0.3f);
                break;
            case S::chorus:
                if (choruses < 2)
                    verseAgain (0.85f);
                if (choruses >= 2 && ! brokenDown && heavy)
                    breakdown (0.8f);
                if (choruses >= 2 && ! bridged)
                    bridge (heavy ? 0.65f : 0.8f);
                if (bridged || brokenDown)
                {
                    chorusAgain (0.55f);
                    ending (0.8f);
                }
                else if (choruses >= 3)
                    ending (0.7f);
                if (choruses < 2)
                    breakdown (heavy ? 0.35f : 0.0f);
                if (choruses >= 2)
                    solo (bridged || brokenDown ? 0.5f : 0.75f);
                break;
            case S::bridge:
                chorusAgain (0.9f);
                solo (0.6f);
                if (heavy && ! brokenDown)
                    breakdown (0.55f);
                ending (0.3f);
                break;
            case S::breakdown:
                chorusAgain (0.9f);
                ending (0.45f);
                if (! bridged)
                    bridge (0.3f);
                break;
            case S::solo:
                chorusAgain (0.9f);
                if (heavy && ! brokenDown)
                    breakdown (0.5f);
                ending (0.4f);
                break;
            case S::ending:
                break;
        }
        std::stable_sort (out.begin(), out.end(), [] (const SectionSuggestion& a, const SectionSuggestion& b) { return a.score > b.score; });
        return out;
    }
}
