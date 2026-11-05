#include "engine/DrumGenerator.h"

namespace pr7s
{
    namespace
    {
        using namespace GmDrum;

        // A style's habits, at a verse's level.
        struct Rules
        {
            float kick[16];      // the chance of a kick on each sixteenth
            float ghost;         // how much the snare ghosts between the backbeats
            float doubleKick;    // how much the feet fill in between kicks
            int verseCymbal, chorusCymbal;
            int handStep;        // sixteenths between the hand's hits (2: eighths, 1: sixteenths)
            bool skankVerse;     // thrash: the verse's snare on every off-beat eighth
            bool openHats;       // nu-metal: the hats open on the and of two and four
            const char* kickWords;
        };

        const Rules& rulesOf (GuitarStyle style)
        {
            static const Rules rock { { 1, 0, .1f, 0, .05f, 0, .35f, .1f, .75f, 0, .45f, .1f, .05f, 0, .3f, .15f },
                                      .3f, .05f, closedHat, ride, 2, false, false, "kick on one and three with pushes" };
            static const Rules thrash { { 1, 0, .3f, 0, .85f, 0, .3f, 0, .9f, 0, .3f, 0, .85f, 0, .3f, .1f },
                                        .1f, .55f, closedHat, ride, 2, true, false, "kick driving the beats" };
            static const Rules numetal { { 1, .05f, .1f, .45f, .05f, .05f, .5f, .2f, .15f, .1f, .55f, .3f, .05f, .1f, .25f, .15f },
                                         .35f, .1f, closedHat, crash, 2, false, true, "syncopated, bouncing kick" };
            static const Rules metalcore { { 1, .1f, .5f, .1f, .3f, .1f, .5f, .1f, .8f, .1f, .5f, .1f, .3f, .1f, .5f, .2f },
                                           .15f, .45f, ride, china, 2, false, false, "busy kick under the riff" };
            static const Rules shred { { 1, .4f, .6f, .4f, .6f, .4f, .6f, .4f, .9f, .4f, .6f, .4f, .6f, .4f, .6f, .4f },
                                       .05f, .85f, ride, crash, 2, false, false, "double kick rolling under it" };
            switch (style)
            {
                case GuitarStyle::rock: return rock;
                case GuitarStyle::thrash: return thrash;
                case GuitarStyle::numetal: return numetal;
                case GuitarStyle::metalcore: return metalcore;
                case GuitarStyle::shred: return shred;
            }
            return rock;
        }

        struct Hit { int step; int note; int velocity; };
        using Bar = std::vector<Hit>;

        bool halfTimeIn (GuitarStyle style, DrumSection section)
        {
            return section == DrumSection::breakdown || section == DrumSection::bridge
                || (section == DrumSection::chorus && style == GuitarStyle::metalcore);
        }

        int handOf (const Rules& rules, DrumSection section, const DrumSettings& s, GuitarStyle style)
        {
            switch (s.cymbal)
            {
                case DrumSettings::Cymbal::hats: return closedHat;
                case DrumSettings::Cymbal::ride: return ride;
                case DrumSettings::Cymbal::china: return china;
                case DrumSettings::Cymbal::automatic: break;
            }
            switch (section)
            {
                case DrumSection::chorus: return rules.chorusCymbal;
                case DrumSection::breakdown: return china;
                case DrumSection::bridge: return ride;
                case DrumSection::intro: return style == GuitarStyle::rock || style == GuitarStyle::numetal ? closedHat : ride;
                case DrumSection::verse:
                case DrumSection::ending: break;
            }
            return rules.verseCymbal;
        }

        // One bar's groove, drawn from the rules.
        Bar drawGroove (GuitarStyle style, DrumSection section, const DrumSettings& s, juce::Random& rng,
                        const std::vector<int>& lockedKicks)
        {
            const auto& rules = rulesOf (style);
            Bar bar;
            const auto halfTime = halfTimeIn (style, section);
            const auto skank = rules.skankVerse && (section == DrumSection::verse || section == DrumSection::intro);
            // The snare's backbone.
            std::vector<int> backbeat = halfTime ? std::vector<int> { 8 } : skank ? std::vector<int> { 2, 6, 10, 14 } : std::vector<int> { 4, 12 };
            for (const auto b : backbeat)
                bar.push_back ({ b, snare, skank ? 100 : 118 });
            const auto isBackbeat = [&] (int step) { return std::find (backbeat.begin(), backbeat.end(), step) != backbeat.end(); };

            // The kick.
            auto density = 0.4f + 1.2f * s.density;
            if (section == DrumSection::intro) density *= 0.6f;
            if (section == DrumSection::chorus) density *= 1.15f;
            if (section == DrumSection::bridge) density *= 0.8f;
            std::array<bool, 16> kick {};
            if (! lockedKicks.empty())
                for (const auto k : lockedKicks)
                    kick[(size_t) juce::jlimit (0, 15, k)] = true;
            else if (section == DrumSection::breakdown)
            {
                // Chugs: on each beat, maybe a cluster of one to three sixteenths.
                for (auto beat = 0; beat < 4; ++beat)
                    if (beat == 0 || rng.nextFloat() < 0.35f + 0.5f * s.density)
                    {
                        const auto length = 1 + rng.nextInt (3);
                        for (auto k = 0; k < length; ++k)
                            kick[(size_t) (beat * 4 + k)] = true;
                    }
            }
            else
            {
                for (auto step = 0; step < 16; ++step)
                {
                    auto p = rules.kick[step] * density;
                    if (halfTime && step % 4 != 0)
                        p *= 0.6f;
                    if (isBackbeat (step))
                        p *= 0.25f;
                    kick[(size_t) step] = step == 0 || rng.nextFloat() < juce::jlimit (0.0f, 1.0f, p);
                }
                // Double kick: the feet fill in next to the kicks (more when DOUBLE KICK is up).
                const auto dk = juce::jlimit (0.0f, 1.0f, rules.doubleKick * 2.0f * s.doubleKick);
                if (section != DrumSection::intro && dk > 0.0f)
                {
                    const auto before = kick;
                    for (auto step = 1; step < 16; ++step)
                        if (! before[(size_t) step] && (before[(size_t) step - 1] || (step < 15 && before[(size_t) step + 1]))
                            && rng.nextFloat() < dk * 0.7f)
                            kick[(size_t) step] = true;
                    if (dk > 0.8f)   // a lot: constant sixteenths
                        for (auto& k : kick)
                            k = true;
                }
            }
            for (auto step = 0; step < 16; ++step)
                if (kick[(size_t) step])
                    bar.push_back ({ step, GmDrum::kick, step % 4 == 0 ? 112 : 96 });

            // The hand.
            const auto hand = handOf (rules, section, s, style);
            const auto handStep = section == DrumSection::breakdown || hand == crash || hand == china ? 4 : rules.handStep;
            for (auto step = 0; step < 16; step += handStep)
            {
                auto note = hand;
                if (rules.openHats && hand == closedHat && (step == 6 || step == 14))
                    note = openHat;
                bar.push_back ({ step, note, step % 4 == 0 ? 100 : 78 });
            }
            if (section == DrumSection::bridge && s.cymbal == DrumSettings::Cymbal::automatic)
                for (auto step = 0; step < 16; step += 4)
                    bar.push_back ({ step, rideBell, 104 });

            // Ghost notes.
            const auto g = juce::jlimit (0.0f, 1.0f, rules.ghost * 2.0f * s.ghostNotes);
            if (section != DrumSection::breakdown && ! skank)
                for (auto step = 1; step < 16; ++step)
                    if (! isBackbeat (step) && step % 4 != 0 && rng.nextFloat() < g * 0.35f)
                        bar.push_back ({ step, snare, 34 + rng.nextInt (12) });
            return bar;
        }

        // A small change for the phrase's third bar: a kick moved or added.
        Bar varied (Bar bar, juce::Random& rng)
        {
            std::vector<size_t> kicks;
            for (size_t i = 0; i < bar.size(); ++i)
                if (bar[i].note == kick && bar[i].step != 0)
                    kicks.push_back (i);
            if (! kicks.empty() && rng.nextBool())
                bar.erase (bar.begin() + (std::ptrdiff_t) kicks[(size_t) rng.nextInt ((int) kicks.size())]);
            else
            {
                const auto step = 2 * (1 + rng.nextInt (7)) + (rng.nextBool() ? 1 : 0);
                if (step < 16)
                    bar.push_back ({ step, kick, 92 });
            }
            return bar;
        }

        // A fill over the bar's last `beats` beats, from the style's words.
        void addFill (Bar& bar, GuitarStyle style, int beats, juce::Random& rng)
        {
            const auto from = 16 - 4 * beats;
            bar.erase (std::remove_if (bar.begin(), bar.end(), [from] (const Hit& h) { return h.step >= from; }), bar.end());
            const auto metal = style != GuitarStyle::rock;
            const int toms[] = { tomHigh, tomMid, tomLow, tomFloor };
            const auto kind = rng.nextInt (style == GuitarStyle::numetal ? 3 : 4);
            for (auto step = from; step < 16; ++step)
            {
                const auto along = (float) (step - from) / (float) juce::jmax (1, 16 - from);
                const auto velocity = 84 + (int) (34.0f * along);
                int note;
                switch (kind)
                {
                    case 0: note = snare; break;                                                   // a snare roll
                    case 1: note = toms[juce::jmin (3, (int) (along * 4.0f))]; break;              // round the toms, high to low
                    case 2: note = (step - from) % 2 == 0 ? snare : toms[(step - from) / 2 % 4]; break;   // snare and toms in turn
                    default: note = (step - from) < (16 - from) / 2 ? snare : toms[(step - from) % 4]; break;   // snare, then the toms
                }
                if (style == GuitarStyle::numetal && kind == 0 && step % 2 == 1)
                    continue;   // (a stop-start fill: eighths)
                bar.push_back ({ step, note, velocity });
                if (metal && step % 2 == 0)
                    bar.push_back ({ step, kick, 100 });
            }
        }

        juce::String cymbalWord (int note)
        {
            return note == ride ? "ride" : note == china ? "china" : note == crash ? "crashes" : note == rideBell ? "ride bell" : "hats";
        }
    }

    //==========================================================================
    const std::vector<DrumSection>& drumSections()
    {
        static const std::vector<DrumSection> all { DrumSection::intro, DrumSection::verse, DrumSection::chorus,
                                                    DrumSection::bridge, DrumSection::breakdown, DrumSection::ending };
        return all;
    }

    juce::String drumSectionName (DrumSection section)
    {
        switch (section)
        {
            case DrumSection::intro: return "INTRO";
            case DrumSection::verse: return "VERSE";
            case DrumSection::chorus: return "CHORUS";
            case DrumSection::bridge: return "BRIDGE";
            case DrumSection::breakdown: return "BREAKDOWN";
            case DrumSection::ending: return "ENDING";
        }
        return {};
    }

    DrumSettings DrumSettings::limited() const
    {
        auto s = *this;
        s.density = juce::jlimit (0.0f, 1.0f, density);
        s.doubleKick = juce::jlimit (0.0f, 1.0f, doubleKick);
        s.ghostNotes = juce::jlimit (0.0f, 1.0f, ghostNotes);
        s.fillEvery = fillEvery <= 0 ? 0 : fillEvery <= 2 ? 2 : fillEvery <= 4 ? 4 : 8;
        s.fillBeats = fillBeats >= 2 ? 2 : 1;
        return s;
    }

    juce::String DrumSettings::toString() const
    {
        return juce::String (density, 2) + "," + juce::String (doubleKick, 2) + "," + juce::String (ghostNotes, 2) + ","
             + juce::String (fillEvery) + "," + juce::String (fillBeats) + "," + juce::String ((int) cymbal) + "," + (lockToRiff ? "1" : "0");
    }

    DrumSettings DrumSettings::fromString (const juce::String& text)
    {
        const auto parts = juce::StringArray::fromTokens (text, ",", "");
        if (parts.size() != 7)
            return {};
        for (const auto& p : parts)
            if (p.trim().isEmpty() || ! p.trim().containsOnly ("0123456789."))
                return {};
        DrumSettings s;
        s.density = parts[0].getFloatValue();
        s.doubleKick = parts[1].getFloatValue();
        s.ghostNotes = parts[2].getFloatValue();
        s.fillEvery = parts[3].getIntValue();
        s.fillBeats = parts[4].getIntValue();
        s.cymbal = (Cymbal) juce::jlimit (0, 3, parts[5].getIntValue());
        s.lockToRiff = parts[6].trim() == "1";
        return s.limited();
    }

    LineIdea generateBeat (GuitarStyle style, DrumSection section, const std::vector<ChordSpan>& spans, double beatsPerBar,
                           const DrumSettings& settings, juce::uint32 seed, const std::vector<double>& riffHits)
    {
        LineIdea idea { LineIdea::Part::drums, drumSectionName (section).substring (0, 1) + drumSectionName (section).substring (1).toLowerCase(), {}, {} };
        if (spans.empty() || beatsPerBar <= 0.0)
            return idea;
        const auto s = settings.limited();
        const auto total = spans.back().end - spans.front().start;
        const auto bars = juce::jmax (1, (int) std::ceil (total / beatsPerBar - 1.0e-9));
        const auto step = beatsPerBar / 16.0;
        juce::Random rng ((juce::int64) seed * 2654435761LL + (int) section * 97 + (int) style * 13 + 5);
        const auto locked = s.lockToRiff && ! riffHits.empty() && section != DrumSection::intro && section != DrumSection::ending;
        const auto lockedKicksOf = [&] (int bar)
        {
            std::vector<int> steps;
            if (locked)
                for (const auto h : riffHits)
                    if (h >= bar * beatsPerBar - 1.0e-9 && h < (bar + 1) * beatsPerBar - 1.0e-9)
                        steps.push_back ((int) std::round ((h - bar * beatsPerBar) / step));
            return steps;
        };
        // The section's groove, drawn once (a drummer keeps it), and its variation.
        const auto groove = drawGroove (style, section, s, rng, {});
        const auto variation = varied (groove, rng);
        for (auto bar = 0; bar < bars; ++bar)
        {
            const auto inPhrase = bar % 4;
            Bar hits;
            if (locked)
            {
                juce::Random lockRng ((juce::int64) seed + bar);
                hits = drawGroove (style, section, s, lockRng, lockedKicksOf (bar));
            }
            else
                hits = inPhrase == 2 ? variation : groove;
            const auto last = bar == bars - 1;
            if (section == DrumSection::ending && last)
                hits = { { 0, kick, 120 }, { 0, crash, 120 }, { 0, crash2, 112 }, { 0, snare, 110 } };   // the last hit
            else if ((s.fillEvery > 0 && (bar + 1) % s.fillEvery == 0 && ! last) || (section == DrumSection::ending && bar == bars - 2))
                addFill (hits, style, section == DrumSection::ending ? 2 : s.fillBeats, rng);
            // A crash where a phrase begins (and after every fill), with the kick.
            const auto afterFill = bar > 0 && s.fillEvery > 0 && bar % s.fillEvery == 0;
            if ((bar % 4 == 0 && ! (section == DrumSection::intro && bar == 0)) || afterFill)
                if (std::none_of (hits.begin(), hits.end(), [] (const Hit& h) { return h.step == 0 && (h.note == crash || h.note == crash2 || h.note == china); }))
                {
                    hits.push_back ({ 0, bar % 8 == 0 ? crash : crash2, 112 });
                    if (std::none_of (hits.begin(), hits.end(), [] (const Hit& h) { return h.step == 0 && h.note == kick; }))
                        hits.push_back ({ 0, kick, 112 });
                }
            for (const auto& h : hits)
            {
                const auto at = bar * beatsPerBar + h.step * step;
                if (at < total - 1.0e-9)
                    idea.notes.push_back ({ at, juce::jmin (step * 2.0, total - at), h.note, (float) juce::jlimit (1, 127, h.velocity) / 127.0f });
            }
        }
        std::stable_sort (idea.notes.begin(), idea.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });

        // What it is, in words.
        const auto& rules = rulesOf (style);
        juce::StringArray words;
        words.add (halfTimeIn (style, section) ? "half time" : rules.skankVerse && section == DrumSection::verse ? "skank beat" : "backbeat on two and four");
        words.add (locked ? "the kick on the riff's hits" : section == DrumSection::breakdown ? "kicks in chug bursts" : juce::String (rules.kickWords));
        words.add (cymbalWord (handOf (rules, section, s, style)));
        if (s.ghostNotes > 0.6f && rules.ghost > 0.1f && section != DrumSection::breakdown)
            words.add ("ghost notes");
        if (section == DrumSection::ending)
            words.add ("a fill and the last hit");
        else if (s.fillEvery > 0)
            words.add ("a fill every " + juce::String (s.fillEvery) + " bars");
        idea.description = "Generated: " + words.joinIntoString (", ");
        return idea;
    }

    std::vector<LineIdea> generatedBeats (GuitarStyle style, const std::vector<ChordSpan>& spans, double beatsPerBar,
                                          const DrumSettings& settings, int variation, const std::vector<double>& riffHits)
    {
        std::vector<LineIdea> out;
        for (const auto section : drumSections())
            out.push_back (generateBeat (style, section, spans, beatsPerBar, settings, (juce::uint32) (variation * 7919 + 1), riffHits));
        return out;
    }
}
