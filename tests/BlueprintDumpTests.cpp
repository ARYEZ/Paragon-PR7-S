#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>
#include "engine/SectionTypes.h"
#include "engine/Markers.h"

using namespace pr7s;

// Hidden: writes every section's types (name | band | description) and the transitions, for the blueprint reader.
TEST_CASE ("0.262.0: the section types and transitions listed", "[.typedump]")
{
    juce::String out;
    for (const auto& section : songSectionNames())
        for (const auto& t : sectionTypesFor (section))
            out << "T|" << section << "|" << t.name << "|" << (int) t.band << "|" << t.description << "\n";
    for (const auto& t : sectionTransitions())
        out << "X|" << t.name << "|" << t.shortBeats << "|" << t.description << "\n";
    juce::File (juce::SystemStats::getEnvironmentVariable ("PARAGON_TYPES_OUT", "/tmp/types.txt")).replaceWithText (out);
    CHECK (out.isNotEmpty());
}
