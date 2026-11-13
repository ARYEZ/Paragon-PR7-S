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
        // (0.95.0) The band's part under the type: STACCATO (the owner's Black, Stricken, Waffle: the bass doubles the
        // guitar's chugs, the kick plays every chug; their approaches the looser accents-only versions).
        // (0.96.0) SPLIT (the owner's Going Under, Papercut, Call Me When You're Sober): the bass doubles the guitar's
        // hits (or holds the root through the gaps), the kick catches the hits while the snare and hats keep time (or
        // a plain backbeat under the guitar).
        // (0.97.0) CLEAN (the owner's Masterpiece, Circle With Me, Clown, Crawling, In The End, The End of the Dream):
        // the heavy guitar steps back; the guitar's five approaches are clean parts of their own (not riff kinds), and
        // the bass, drums and keys each have options, chosen to suit the guitar's approach when left automatic.
        // (0.102.0) PRECHORUS (the owner's Faster, Left Behind, Call Me When You're Sober, The End of the Dream,
        // Crawling, Going Under, Waffle, Circle With Me, The Void, Sweet Sacrifice, At the End of August): a pre-chorus
        // type writes every lane it lists options for (sectionApproaches by the type's name).
        enum class Band { none, staccato, split, clean, preChorus, bridge };
        Band band = Band::none;
        // (0.100.0) AUTOMATIC draws from the first this many of them (0: all): a kind added later is a chosen option
        // only, so a section built on AUTOMATIC keeps its kind when it's rebuilt.
        int automaticFlavours = 0;
    };
    // True when the state's type has the staccato band and the guitar is on, generated (not WRITTEN RIFFS).
    bool staccatoBand (const ComposerState&);
    // The band's part under the state's type (none: no type, WRITTEN RIFFS, or the guitar off).
    SectionType::Band sectionBand (const ComposerState&);
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
    juce::StringArray preChorusApproaches (const juce::String& type, ComposerLayer);   // (0.102.0)
    juce::StringArray bridgeApproaches (const juce::String& type, ComposerLayer);      // (0.105.0)
    // The lane's approach as shown ("AUTOMATIC" when none is chosen; empty when the flavour has none).
    juce::String approachName (const ComposerState&, ComposerLayer);

    // Puts a type on the composer's settings: its name, the guitar's flavour (the generator, not WRITTEN RIFFS) and
    // every lane's approach back to automatic. STANDARD (empty or "STANDARD"): no type, the flavour ANY.
    void applySectionType (ComposerState&, const juce::String& sectionName, const juce::String& type);

    // (0.104.0, the owner's songs read for their shapes) How a block hands over to the next one: what its last bar (or
    // two) does. NONE first (no transition). A SHORT BAR's last bar is that many of the signature's beats long (the
    // song's signature changes for that bar and back after it; everything after the block moves up). The melody (the
    // singer's line) is never touched. Our names.
    struct SectionTransition
    {
        juce::String name, description;
        int shortBeats = 0;   // a SHORT BAR's beats (0: the bar stays whole)
    };
    const std::vector<SectionTransition>& sectionTransitions();
    std::optional<SectionTransition> sectionTransitionNamed (const juce::String&);   // NONE or unknown: none
    constexpr const char* noTransition = "NONE";
}
