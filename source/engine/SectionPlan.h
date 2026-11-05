#pragma once

#include "engine/DrumGenerator.h"
#include "engine/Riffs.h"

// BUILD SECTION's song so far and what comes next (0.57.0, the owner asked: as the chord
// suggestions do, build the verse and have it suggest the chorus's beat and riff from it).
//
// Each built section is remembered (its kind, where it lies, the riff it used and the riff
// settings, the beats' variation). `suggestNextSections` scores what could come next from the
// song so far (our table, a pop/rock/metal song's usual order: intro, verse, chorus, verse,
// chorus, then a bridge or breakdown, a last chorus, an ending), and for each says how it
// relates to what's built:
//   - a chorus keeps the verse's riff opened up (fewer palm mutes, looser), or takes a new riff
//     for contrast (a second suggestion);
//   - a verse after a chorus brings back the last verse's riff and settings exactly;
//   - a breakdown tightens the same riff into chugs (more palm mutes, tighter, busier);
//   - a bridge takes a new riff, looser;
//   - an ending the last chorus's riff.
// The beats keep the same variation (the same drummer's family of grooves for every section).
namespace pr7s
{
    struct BuiltSection
    {
        DrumSection kind = DrumSection::verse;
        double start = 0.0, end = 0.0;   // beats in the song
        int riff = 0;                    // the guitar rhythm card used
        RiffSettings riffSettings;
        int variation = 0;               // the IDEAS deal the beats came from
        bool operator== (const BuiltSection&) const = default;
    };

    struct SectionSuggestion
    {
        DrumSection kind = DrumSection::chorus;
        float score = 0.0f;              // 0..1, best first
        juce::String reason;             // "Bigger: the verse's riff opened up..."
        int riff = 0;                    // the rhythm card to use
        RiffSettings riffSettings;
        int variation = 0;
    };

    // `riffCount`: how many rhythm cards there are (a new riff is the next one round).
    std::vector<SectionSuggestion> suggestNextSections (const std::vector<BuiltSection>& song, GuitarStyle, int riffCount);

    juce::String sectionWord (DrumSection);   // "Verse"
}
