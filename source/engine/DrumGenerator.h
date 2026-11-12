#pragma once

#include "engine/DrumIdeas.h"

// The beat generator (0.55.0, the owner asked: beats made new each time, not only written ones;
// 0.56.0 learns drummer profiles from the owner's song MIDI and drives the same generator).
//
// Each beat is built bar by bar from rules, not read from a list: per STYLE a weight for a kick on
// each sixteenth, the snare's backbone (2 and 4, or 3 in half time), the hand's cymbal and rate,
// where ghost notes go, how much double kick; per SECTION (intro, verse, chorus, bridge, breakdown,
// ending) what changes (sparser, bigger, half time, the cymbal...). A phrase is four bars: the
// first bar's groove is drawn once and played again (a drummer repeats), the third slightly varied,
// the fourth with a fill. A seed makes it repeatable: the same seed and settings give the same beat
// (HEAR plays what USE IT adds); MORE IDEAS deals new seeds.
//
// DRUM SETTINGS: DENSITY (fewer or more kicks and hits), DOUBLE KICK, GHOST NOTES, FILLS (none, or
// every 2, 4 or 8 bars), FILL LENGTH (one or two beats), CYMBAL (the style's, or hats, ride, china
// for every section), LOCK KICK TO RIFF (the kick on the guitar riff's hits).
namespace pr7s
{
    // (0.65.0: SOLO, the owner asked: a guitar solo's section; added last, so the others keep their numbers.)
    enum class DrumSection { intro, verse, chorus, bridge, breakdown, ending, solo };
    const std::vector<DrumSection>& drumSections();
    juce::String drumSectionName (DrumSection);   // "INTRO", "VERSE"...

    struct DrumSettings
    {
        float density = 0.5f, doubleKick = 0.5f, ghostNotes = 0.5f;   // 0.5: the style's own
        int fillEvery = 4;             // bars (0: none; 2, 4 or 8)
        int fillBeats = 1;             // 1 or 2
        enum class Cymbal { automatic, hats, ride, china };
        Cymbal cymbal = Cymbal::automatic;
        bool lockToRiff = false;
        // (0.86.0) Locked in every section, the intro and ending too (the composer's SYNC); not saved.
        bool lockAlways = false;

        DrumSettings limited() const;
        juce::String toString() const;
        static DrumSettings fromString (const juce::String&);   // (unreadable: the defaults)
        bool operator== (const DrumSettings&) const = default;
    };

    // A beat for a section over the chords' length (bars from the first chord), as an idea (Part::drums).
    // `riffHits`: the guitar riff's hits (beats from the first chord), used when locking.
    LineIdea generateBeat (GuitarStyle, DrumSection, const std::vector<ChordSpan>&, double beatsPerBar,
                           const DrumSettings&, juce::uint32 seed, const std::vector<double>& riffHits = {});

    // One per section (the generated column of IDEAS' DRUMS tab).
    std::vector<LineIdea> generatedBeats (GuitarStyle, const std::vector<ChordSpan>&, double beatsPerBar,
                                          const DrumSettings&, int variation, const std::vector<double>& riffHits = {});
}
