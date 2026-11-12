#pragma once

#include "engine/Composer.h"

#include <optional>
#include <vector>

// Section types (0.94.0, the owner's: "each section needs to get a type ... when we click on the chorus we get a drop
// down which lists the types of choruses ... DRIVING or DRONING. This would determine what each instrument does. Then
// each instrument has multiple options with that type"). A block on the SECTIONS row starts STANDARD (the composer as
// before: every lane's own styles). A type sets the guitar's riff flavour, and with it the band's part where we have
// one (DRONING's band: engine/Composer.cpp, from the owner's Crawling, Clown and Circle With Me examples), and lists the
// approaches each lane can take within it (sectionApproaches).
//
// The chorus's two are the owner's, and the verse's STACCATO CHUG (the owner: Black, My Last Serenade and Stricken's
// verses to flesh out; for now its riff kinds are our generator's: tritone chug, chugs, gallop, stop-start). The other
// types are our suggestions, seeded from the guitar flavours we already have (the owner keeps or removes them); a kind's types are offered to every name that plays as that kind
// (composerSectionFor: a Pre-Chorus as a verse, a Hook as a chorus...). Only DRONING has a band of its own so far: under
// every other type the bass, keys, lead and drums play their usual styles over the type's guitar.
namespace pr7s
{
    struct SectionType
    {
        juce::String name;           // "DRONING" (what the block shows)
        juce::String description;
        RiffFlavour flavour = RiffFlavour::any;
        // (0.94.0, the owner: "tritone chug is a great variation for rhythm guitars that would be part of an overall type
        // of verse ... STACCATO CHUG") A type made of several riff kinds: the rhythm guitar's approaches are these
        // flavours (AUTOMATIC: each take another of them). Empty: the guitar plays `flavour`.
        std::vector<RiffFlavour> guitarFlavours;
    };
    // A type by its name alone (the same name means the same type in every section that has it).
    std::optional<SectionType> sectionTypeCalled (const juce::String& type);
    // The guitar's flavour under the state's type and the guitar lane's approach (none: the state's own flavour).
    std::optional<RiffFlavour> typedGuitarFlavour (const ComposerState&);

    // The types a block of that name can have (STANDARD not among them: it's no type).
    std::vector<SectionType> sectionTypesFor (const juce::String& sectionName);
    std::optional<SectionType> sectionTypeNamed (const juce::String& sectionName, const juce::String& type);
    bool isSectionTypeOf (const juce::String& sectionName, const juce::String& type);   // empty: true (STANDARD)
    constexpr const char* standardSectionType = "STANDARD";

    // A lane's approaches under a guitar flavour (empty: the lane's usual styles). DRONING's: the guitar rung whole /
    // pushed a beat early / a moving top note / struck twice / sliding down; the bass held with the guitar / held and
    // pulsed at the loop's end; the keys a high pulse / a climbing motif / a held third; the lead octaves on the thirds
    // / on the roots / a muted pedal in threes; the drums open hats / half time / locked to the pushes. Our names.
    juce::StringArray sectionApproaches (RiffFlavour, ComposerLayer);
    juce::StringArray sectionApproaches (const ComposerState&, ComposerLayer);   // by the state's flavour
    // The lane's approach as shown ("AUTOMATIC" when none is chosen; empty when the flavour has none).
    juce::String approachName (const ComposerState&, ComposerLayer);

    // Puts a type on the composer's settings: its name, the guitar's flavour (the generator, not WRITTEN RIFFS) and
    // every lane's approach back to automatic. STANDARD (empty or "STANDARD"): no type, the flavour ANY.
    void applySectionType (ComposerState&, const juce::String& sectionName, const juce::String& type);
}
