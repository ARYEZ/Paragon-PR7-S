// (0.261.0) The duplicate audit the owner asked for: every option of every lane under every section type composed over
// the same chords, compared note for note. Hidden ([.dupaudit]); PARAGON_AUDIT_OUT=<file> writes the full report.
#include <catch2/catch_test_macros.hpp>
#include "TestHelpers.h"
#include <map>
#include <set>
#include "engine/Composer.h"
#include "engine/SectionTypes.h"

using namespace pr7s;

namespace
{
    std::vector<ChordSpan> auditSpans()
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

    // The notes as heard: pitch, start and length to a 96th of a beat, the second guitar marked.
    juce::String notesKey (const LineIdea& idea)
    {
        std::vector<juce::String> parts;
        for (const auto& n : idea.notes)
            if (! n.keyswitch)
                parts.push_back (juce::String (n.pitch) + "@" + juce::String (juce::roundToInt (n.beat * 96.0)) + "/"
                                 + juce::String (juce::roundToInt (n.length * 96.0)) + (n.harmony ? "h" : ""));
        std::sort (parts.begin(), parts.end());
        juce::String k;
        for (const auto& p : parts)
            k << p << ";";
        return k;
    }

    // The same rhythm (starts only) with any notes.
    juce::String rhythmKey (const LineIdea& idea)
    {
        std::set<int> at;
        for (const auto& n : idea.notes)
            if (! n.keyswitch && ! n.harmony)
                at.insert (juce::roundToInt (n.beat * 96.0));
        juce::String k;
        for (const auto a : at)
            k << a << ";";
        return k;
    }

    bool generic (const juce::String& n)
    {
        return n == "OUT" || n == "SILENT" || n == "ROOTS HELD" || n == "HELD PADS" || n == "ROOT EIGHTHS" || n == "DOUBLES THE GUITAR"
            || n == "LOCKED WITH THE KICK";
    }
}

TEST_CASE ("0.261.0: the duplicate audit (every type, lane and option)", "[dupaudit]")
{
    const ComposerContext c;
    const auto spans = auditSpans();
    const char* sections[] { "Intro", "Verse", "Pre-Chorus", "Chorus", "Post-Chorus", "Interlude", "Bridge", "Breakdown", "Outro", "Solo" };
    std::vector<std::pair<juce::String, juce::String>> types;   // (section, type), each type once
    std::set<juce::String> seenTypes;
    for (const auto* section : sections)
        for (const auto& t : sectionTypesFor (section))
            if (seenTypes.insert (t.name).second)
                types.push_back ({ section, t.name });

    // lane -> notes key -> the option names that wrote it (and under which type)
    std::map<int, std::map<juce::String, std::map<juce::String, juce::String>>> byNotes;
    std::map<int, std::map<juce::String, std::set<juce::String>>> byRhythm;
    std::map<juce::String, std::vector<juce::String>> typeByNotes;
    juce::StringArray withinType;
    std::map<int, std::set<juce::String>> extrasDone;
    int composed = 0;
    for (const auto& [section, type] : types)
    {
        ComposerState s;
        applySectionType (s, section, type);
        juce::String whole;
        {
            const auto layers = composeLayers (s, spans, c);
            for (const auto l : composerLayers())
                whole << (int) l << "{" << notesKey (layers[(size_t) l]) << "}";
        }
        typeByNotes[whole].push_back (juce::String (section) + ": " + type);
        for (const auto l : composerLayers())
        {
            const auto names = sectionApproaches (s, l);
            if (names.isEmpty())
                continue;
            const auto base = baseSectionApproaches (s, l).size();
            // (composerChoices lists styles where the type doesn't write the lane: each approach composed here.)
            std::vector<LineIdea> choices;
            for (int o = 0; o < names.size(); ++o)
            {
                auto each = s;
                each.lane (l).approach = o;
                choices.push_back (composeLayer (l, each, spans, c));
            }
            std::map<juce::String, juce::String> here;   // notes key -> first name under this type
            for (size_t o = 0; o < choices.size() && o < (size_t) names.size(); ++o)
            {
                const auto& name = names[(int) o];
                const auto own = (int) o < base;
                ++composed;
                const auto k = notesKey (choices[o]);
                if (k.isEmpty() || generic (name))
                    continue;
                // Within one list every name counts, shared options too (they sit in the same menu as the type's own).
                if (const auto it = here.find (k); it != here.end() && it->second != name)
                    withinType.add (composerLayerName (l) + " | " + type + " | " + it->second + "  ==  " + name);
                else
                    here[k] = name;
                // Across types a shared option is audited once (under the first type that lists it).
                if (! own && ! extrasDone[(int) l].insert (name).second)
                    continue;
                byNotes[(int) l][k].insert ({ name, type });
                if (l != ComposerLayer::drums)
                    byRhythm[(int) l][rhythmKey (choices[o])].insert (name);
            }
        }
    }

    juce::StringArray across, sameRhythm, sameTypes;
    for (const auto& [l, keys] : byNotes)
        for (const auto& [k, names] : keys)
            if (names.size() > 1)
            {
                juce::String line = composerLayerName ((ComposerLayer) l) + ": ";
                for (const auto& [name, type] : names)
                    line << name << " (" << type << ")  |  ";
                across.add (line.dropLastCharacters (5));
            }
    for (const auto& [l, keys] : byRhythm)
        for (const auto& [k, names] : keys)
            if (names.size() > 1)
            {
                juce::String line = composerLayerName ((ComposerLayer) l) + ": ";
                for (const auto& name : names)
                    line << name << "  |  ";
                sameRhythm.add (line.dropLastCharacters (5));
            }
    for (const auto& [k, list] : typeByNotes)
        if (list.size() > 1)
            sameTypes.add (juce::StringArray (list.data(), (int) list.size()).joinIntoString ("  ==  "));

    juce::String report;
    report << "Types audited: " << (int) types.size() << ", options composed: " << composed << "\n\n";
    report << "== Section types whose lanes all come out the same (" << sameTypes.size() << ")\n" << sameTypes.joinIntoString ("\n") << "\n\n";
    report << "== Two names, the same notes, under one type (" << withinType.size() << ")\n" << withinType.joinIntoString ("\n") << "\n\n";
    report << "== Two names, the same notes, anywhere (" << across.size() << ")\n" << across.joinIntoString ("\n") << "\n\n";
    report << "== Pitched lanes: the same rhythm under different names (" << sameRhythm.size() << ")\n" << sameRhythm.joinIntoString ("\n") << "\n";
    if (const auto out = juce::SystemStats::getEnvironmentVariable ("PARAGON_AUDIT_OUT", {}); out.isNotEmpty())
        juce::File (out).replaceWithText (report);
    INFO (report.substring (0, 4000));
    CHECK (composed > 0);
    // No menu offers two names for the same notes (shared options included: they sit in every type's menu).
    INFO (withinType.joinIntoString ("\n"));
    CHECK (withinType.isEmpty());
}
