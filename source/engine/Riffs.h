#pragma once

#include "engine/Lines.h"

// Guitar styles and riff settings (0.53.0, the owner asked for better guitar ideas: thrash,
// nu-metal, metalcore / breakdowns and shred leads, and a RIFF SETTINGS panel with riff length,
// palm-mute amount, busyness and ending fills). Our own riffs and rules, not taken from any
// product or record.
//
// ROCK is 0.19.0's twelve ideas, unchanged. The other styles' riffs are written as bars of
// steps (a character per sixteenth, or per triplet when a bar has 12 or 24 of them), played on
// the chords: the riff starts again at each chord, its notes taken from that chord (power
// chords, octaves) or from the key's low note on the lowest string (the pedal). A riff has up
// to three bars, A, B and C: one bar long it plays A; two bars, A B; four bars, A B A C (C the
// turnaround). The leads are a rhythm per bar and the moves a line makes through a scale.
namespace pr7s
{
    // (0.64.0) GROOVE: low palm-muted chugs answered by a high, dissonant two-note stab (a tritone with
    // vibrato, a whole step bent up, a ringing unison): the owner's examples were groove metal and
    // nu-metal records. Added last, so the others keep their numbers.
    enum class GuitarStyle { rock, thrash, numetal, metalcore, shred, groove };
    const std::vector<GuitarStyle>& guitarStyles();
    juce::String guitarStyleName (GuitarStyle);        // "ROCK", "THRASH", "NU-METAL", "METALCORE", "SHRED", "GROOVE"
    GuitarStyle guitarStyleNamed (const juce::String&);   // (else rock)
    juce::String guitarStyleDescription (GuitarStyle);

    // RIFF SETTINGS (the rhythm ideas, every style).
    //   bars        1, 2 or 4: how long a riff is before it repeats (written styles; ROCK's are a bar)
    //   palmMutes   0-1, 0.5: as written; above, open notes off the beat palm muted; below, mutes let ring
    //   tightness   0-1, 0.5: as written; above, tighter (lower velocity, shorter); below, looser
    //   busyness    0-1, 0.5: as written; below, weak hits left out; above, chugs added in the gaps
    //   fills       the last beat of each phrase (4 bars for a one-bar riff, else the riff's length)
    //               a fill in the style: a chromatic run into the next chord, a burst of chugs,
    //               a pinch-harmonic squeal, a stop
    struct RiffSettings
    {
        int bars = 2;
        float palmMutes = 0.5f, tightness = 0.5f, busyness = 0.5f;
        bool fills = false;

        RiffSettings limited() const;
        juce::String toString() const;                  // "2,0.50,0.50,0.50,0"
        static RiffSettings fromString (const juce::String&);   // (unreadable: the defaults)
        bool operator== (const RiffSettings&) const = default;
    };

    // The written styles' six rhythm ideas and six leads (not rock: Lines.cpp's own).
    std::vector<LineIdea> styleRhythmIdeas (GuitarStyle, const std::vector<ChordSpan>&, double beatsPerBar, int variation,
                                            const GuitarTuning&, const GuitarVelocities&, const RiffSettings&);
    std::vector<LineIdea> styleLeadIdeas (GuitarStyle, const std::vector<ChordSpan>&, double beatsPerBar, int variation,
                                          const GuitarTuning&, const GuitarVelocities&);

    // (0.65.0) A WHOLE GUITAR SOLO over the chords (the owner asked for a Solo section): made from the
    // style's own six leads, ordered from the calmest (fewest notes a beat) to the busiest, a lead per
    // two-bar phrase so the solo starts melodic and climbs to its fastest phrase at the end. Each phrase's
    // longest single note (held at least three quarters of a beat) is bent up a whole step into its note
    // (written a whole step under, `LineNote::Effect::bend`), the other long single notes get vibrato, and
    // the last note is held to the end with vibrato. Pinch harmonics and rakes are left as they are.
    LineIdea guitarSolo (const std::vector<LineIdea>& leads, const std::vector<ChordSpan>&, double beatsPerBar, const GuitarTuning&);

    // PALM MUTES, TIGHTNESS, BUSYNESS and FILLS on a rhythm idea's notes (beats from the first
    // chord's start): every style's, rock's too. `strummed`: no chugs added (a strum isn't chugged).
    void shapeRiff (LineIdea&, const std::vector<ChordSpan>&, double beatsPerBar, const RiffSettings&, const GuitarVelocities&,
                    GuitarStyle, int variation, const GuitarTuning&, bool strummed = false);
}
