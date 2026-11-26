#pragma once

#include "engine/Chords.h"

#include <optional>

// CHORD ROLL (0.208.0, the owner: "a ROLL that cycles through random progressions in the key, weighted toward the
// likely / popular ones", one way to add chords in ARCHITECT and the rest of the studio). Our design.
//
// The chords are drawn one after another (a Markov chain), only from the key's own chords (`keyChords`' seven triads;
// a minor key's major V and the raised seventh's diminished chord too: every one `isInKey`):
//   - the first chord by its START weight: how often our progressions (the progression library and the popular
//     progressions in a key) open on it, plus a little for every chord of the key, so any of them can open; the tonic
//     the most likely, but held under 57 % of the opening weight (then the section's feel, and again under 65 %);
//   - each next chord by the chord before it: how often the library's and the popular progressions go from that chord
//     to this one (by scale degree in the key's mode, the chord's quality kept: a minor key's v and V apart; the last
//     chord back to the first counted too, as they're played round), plus a little for every move between the key's
//     chords so every chord can follow every other (diminished chords less). Progressions written in the other mode
//     (played on the relative key, as the library plays them) count half. Never the same chord twice in a row.
//   - the last chord of a round weighted toward chords that lead back to its first (the round repeats), and to the next
//     section's first chord when it's known;
//   - the section's feel (by its name: intro, verse, pre-chorus, chorus, post-chorus, bridge, breakdown, outro / ending,
//     solo) multiplies the weights by scale degree (our choices):
//        intro       opens on the tonic, IV and VI a little up;
//        verse       stays near home: i, iv and VI up, V a little down;
//        pre-chorus  builds: opens away from the tonic on IV / ii / VI, leans to V at the end of a round;
//        chorus      lifts: VI, III, VII and IV up, opening on VI more often;
//        post-chorus as the chorus, a little less;
//        bridge      goes elsewhere: rarely opens on the tonic, IV / VI / V up, the tonic down;
//        breakdown   heavy and still: the tonic, ii, VI and VII up, two-bar chords more often;
//        outro       comes home: opens on the tonic, ends on it;
//        solo        a loop: the last chord pulled hard toward the first, VI and VII up.
//
// The rhythm: one chord a bar in rounds of four bars repeated to fill the section (as songs do; a section under four
// bars: a chord a bar); in sections of eight bars or more, now and then four chords of two bars (breakdowns more
// often); bridges and solos sometimes draw each round of four anew. A `layout` (the block's chords' lengths, when it
// has two or more filling it) is kept as it is: one chord drawn for each.
namespace pr7s
{
    struct RolledChord
    {
        Chord chord;
        double beats = 0.0;
        bool operator== (const RolledChord&) const = default;
    };

    // The chain for a key and a section (empty name: no feel): the chords, their scale degrees (0 the tonic's), the
    // start weights, and next[a][b], the weight of b after a (0 on the diagonal). Tests read it.
    struct ChordRollWeights
    {
        std::vector<Chord> chords;
        std::vector<int> degrees;
        std::vector<double> start;
        std::vector<std::vector<double>> next;
        double loopPull = 1.5;      // how hard a round's last chord leans toward one that leads to its first
        double endOnTonic = 1.0;    // the section's last chord's tonic weight times this (outro: more)
        double toFifthAtEnd = 1.0;  // a round's last chord's V weight times this (pre-chorus: more)
        double twoBarChance = 0.25; // (sections of 8 bars or more)
        double redrawChance = 0.0;  // each round of four drawn anew (bridge, solo)
        int indexOf (const Chord&) const;   // -1: not one of them
    };
    ChordRollWeights chordRollWeights (const Key&, const juce::String& sectionName);

    // How often each move (degree a to degree b, by the chord's place in keyChords' order) is in the progressions
    // counted, for a mode (the raw counts, before smoothing; tests read them).
    const std::vector<std::vector<double>>& chordRollCounts (bool minor);

    struct ChordRollRequest
    {
        Key key;
        juce::String section;                  // the block's name ("Chorus 2"): its feel; empty: none
        int bars = 4;
        double beatsPerBar = 4.0;
        std::vector<double> layout;            // the chords' lengths to keep (beats); empty or one: ours
        std::optional<Chord> next;             // the next section's first chord, when known
        std::vector<juce::String> avoid;       // progressions given lately (rolledFingerprint): drawn again
    };

    // The section's chords, end to end, filling bars x beatsPerBar exactly (each drawn up to 16 times while it's one to
    // avoid). The same seed, the same chords.
    std::vector<RolledChord> rollChords (const ChordRollRequest&, juce::uint32 seed);
    std::vector<RolledChord> rollChords (const Key&, const juce::String& sectionName, int bars, double beatsPerBar,
                                         juce::uint32 seed, const std::vector<juce::String>& avoid = {});
    juce::String rolledFingerprint (const std::vector<RolledChord>&);
}
