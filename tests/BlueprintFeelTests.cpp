// (0.262.0) Hidden: every section type and every option of its lanes composed once over the same chords and measured
// (where the kick and snare fall, how busy each pitched lane is, how much it holds, which lanes play), so the
// blueprints made from the owner's songs can pick the types and options that play most like each song's section.
// PARAGON_FEEL_OUT=<file>: a JSON line per (section, type, lane, option).
#include <catch2/catch_test_macros.hpp>
#include "TestHelpers.h"
#include <set>
#include "engine/Composer.h"
#include "engine/SectionTypes.h"

using namespace pr7s;

namespace
{
    std::vector<ChordSpan> feelSpans()
    {
        Key aMinor;
        for (const auto& k : Key::all())
            if (k.minor && k.pitchClass() == 9)
                aMinor = k;
        const std::array<Chord, 4> chords { Chord { 9, Chord::Type::minor }, Chord { 5, Chord::Type::major },
                                            Chord { 0, Chord::Type::major }, Chord { 7, Chord::Type::major } };
        std::vector<ChordSpan> spans;
        for (int bar = 0; bar < 8; ++bar)
            spans.push_back ({ bar * 4.0, bar * 4.0 + 4.0, chords[(size_t) (bar % 4)], aMinor });
        return spans;
    }

    // kick / snare: the share of bars with a hit on each sixteenth; onsets: distinct sixteenths a bar with a start;
    // held: notes of two beats or more a bar; low: the average pitch.
    juce::String measure (const LineIdea& idea, bool drums)
    {
        std::array<double, 16> kick {}, snare {};
        std::set<int> starts;
        int held = 0, count = 0;
        double pitch = 0.0;
        for (const auto& n : idea.notes)
        {
            if (n.keyswitch)
                continue;
            const auto slot = juce::roundToInt (n.beat * 4.0);
            if (drums)
            {
                const auto s = (size_t) (((slot % 16) + 16) % 16);
                if (n.pitch == 35 || n.pitch == 36) kick[s] += 1.0 / 8.0;
                if (n.pitch == 37 || n.pitch == 38 || n.pitch == 40) snare[s] += 1.0 / 8.0;
            }
            else
            {
                starts.insert (slot);
                if (n.length >= 2.0) ++held;
                pitch += n.pitch;
                ++count;
            }
        }
        juce::String s;
        if (drums)
        {
            s << "\"kick\":[";
            for (size_t i = 0; i < 16; ++i) s << (i ? "," : "") << juce::String (juce::jmin (1.0, kick[i]), 2);
            s << "],\"snare\":[";
            for (size_t i = 0; i < 16; ++i) s << (i ? "," : "") << juce::String (juce::jmin (1.0, snare[i]), 2);
            s << "],\"hits\":" << juce::String ((double) idea.notes.size() / 8.0, 2);
        }
        else
            s << "\"onsets\":" << juce::String ((double) starts.size() / 8.0, 2) << ",\"held\":" << juce::String (held / 8.0, 2)
              << ",\"pitch\":" << juce::String (count > 0 ? pitch / count : 0.0, 1) << ",\"notes\":" << count;
        return s;
    }
}

TEST_CASE ("0.262.0: every type and option measured", "[.feeldump]")
{
    const ComposerContext c;
    const auto spans = feelSpans();
    const juce::File file (juce::SystemStats::getEnvironmentVariable ("PARAGON_FEEL_OUT", "/tmp/feel.jsonl"));
    file.deleteFile();
    juce::FileOutputStream stream (file);
    auto lines = 0;
    for (const auto* section : { "Intro", "Verse", "Pre-Chorus", "Chorus", "Post-Chorus", "Bridge", "Breakdown", "Interlude", "Outro", "Solo" })
        for (const auto& t : sectionTypesFor (section))
        {
            ComposerState s;
            applySectionType (s, section, t.name);
            for (const auto l : composerLayers())
            {
                juce::String out;   // (a line or a few at a time: one string for everything was copied on every append)
                const auto drums = l == ComposerLayer::drums;
                const auto names = sectionApproaches (s, l);
                const auto plain = composeLayer (l, s, spans, c);
                out << "{\"section\":\"" << section << "\",\"type\":\"" << t.name << "\",\"lane\":\"" << composerLayerName (l)
                    << "\",\"on\":" << (s.lane (l).on ? 1 : 0) << ",\"option\":-1," << measure (plain, drums) << "}\n";
                if (! s.lane (l).on)
                {
                    stream << out;
                    continue;
                }
                for (int o = 0; o < names.size(); ++o)
                {
                    auto each = s;
                    each.lane (l).approach = o;
                    out << "{\"section\":\"" << section << "\",\"type\":\"" << t.name << "\",\"lane\":\"" << composerLayerName (l)
                        << "\",\"on\":1,\"option\":" << o << ",\"name\":\"" << names[o].replace ("\"", "'") << "\","
                        << measure (composeLayer (l, each, spans, c), drums) << "}\n";
                }
                stream << out;
                ++lines;
            }
        }
    stream.flush();
    CHECK (lines > 0);
}
