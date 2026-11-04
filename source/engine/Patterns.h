#pragma once

#include "engine/Lines.h"

#include <atomic>
#include <optional>

// Performances (0.39.0, the owner asked: as the chord tool they use does, "performances"):
// rhythm and arpeggio patterns that play any chords. Each pattern is steps over a bar of
// 4/4 (scaled to the song's bar): when a step plays, how long, how loud, and which note of
// the chord: a voice counted from the bottom of the chord's voicing (past the top: the same
// notes an octave up), the bass (the chord's root, or its slash bass, under the voicing), the
// bass's fifth, the top voice, or the whole chord (`strum`: spread from the lowest note up, or
// down from the top for an up-stroke). Our own patterns and names, in four groups.
//
// Used two ways: written as ideas under the chord track's chords (IDEAS' PATTERNS tab), and
// played live from the chord pads (PERFORM on the pads' toolbar): a pad held plays its chord
// through the pattern at the song's tempo (with the song when it's playing, from the press
// when it's stopped).
namespace pr7s
{
    struct PatternStep
    {
        static constexpr int bass = -1, all = -2, top = -3, bassFifth = -4;
        double beat = 0.0, length = 0.5;   // in a 4/4 bar's quarter beats (the pattern's own)
        int voice = 0;                     // >= 0 from the bottom; or one of the above
        int octave = 0;                    // octaves added
        float velocity = 0.72f;
        int strum = 0;                     // `all` only: 1 spread up (down-stroke), -1 spread down (up-stroke)
    };

    struct Pattern
    {
        juce::String name, group, description;
        double bars = 1.0;                 // how long before it repeats (in bars)
        std::vector<PatternStep> steps;
    };

    const std::vector<Pattern>& patterns();          // every pattern, by group (ours, then the user's: message thread)
    const juce::StringArray& patternGroups();        // "ARPEGGIOS", "STRUMS & PULSES", "BALLAD & BROKEN", "RHYTHMIC RIFFS" (+ "MY PATTERNS")

    // (0.44.0) The user's patterns (the manual's Pattern player mode: "Import from MIDI Loop"),
    // after ours, in the group MY PATTERNS. Message thread only (the performer keeps a copy).
    inline const juce::String userPatternGroup { "MY PATTERNS" };
    void setUserPatterns (std::vector<Pattern>);
    std::vector<Pattern> getUserPatterns();
    // A MIDI part's notes as a pattern: each distinct pitch a voice, counted from the lowest; the
    // lowest as the bass when it's a fifth or more under the next (a bass line under chords);
    // the part's bars (whole) as the pattern's length. Notes in beats from the part's start.
    Pattern patternFromNotes (const juce::String& name, const std::vector<LineNote>&, double lengthBeats, double beatsPerBar);
    std::unique_ptr<juce::XmlElement> patternToXml (const Pattern&);
    std::optional<Pattern> patternFromXml (const juce::XmlElement&);

    // (0.44.0) A pattern over the chord track's chords (a track performing it): song beats, each chord
    // voiced as IDEAS' patterns are (close round middle C, least movement, the root in the bass).
    std::vector<LineNote> performChordTrack (const Pattern&, const std::vector<ChordSpan>&, double beatsPerBar);
    std::vector<int> patternsInGroup (const juce::String& group);   // indices into patterns()
    int patternNamed (const juce::String& name);     // -1: none

    // A pattern's notes for one chord: `voicing` (its notes, any order) and `bassNote` (-1: an
    // octave under the voicing's lowest), from `from` to `to` (beats, in the song), the pattern
    // laid bar by bar from `barOrigin` (where its bars start), notes cut at `to`.
    std::vector<LineNote> patternNotes (const Pattern&, std::vector<int> voicing, int bassNote,
                                        double from, double to, double barOrigin, double beatsPerBar);
    // The patterns of a group as ideas under these chords (Part::pattern): each chord voiced
    // close round middle C with the least movement (as the pads' adaptive voicing), its root in
    // the bass (C1-B1), the pattern counted from the first chord's bar.
    std::vector<LineIdea> patternIdeas (const std::vector<ChordSpan>&, double beatsPerBar, const juce::String& group);

    // Plays a pattern live (the pads): `play` starts it on a chord (or moves it to a new one,
    // in time), `stop` ends it. A high-resolution timer (2 ms) sends the notes through `send`.
    // The beat: the song's while `songBeat` gives one (playing), else counted from the first
    // press at `bpm` (set from the message thread; read here).
    class PatternPerformer final : private juce::HighResolutionTimer
    {
    public:
        explicit PatternPerformer (std::function<void (const juce::MidiMessage&)> send);
        ~PatternPerformer() override;
        std::function<double()> songBeat;              // the song's beat while playing, else a negative number
        std::atomic<double> bpm { 120.0 }, beatsPerBar { 4.0 };
        void setPattern (int index);                   // -1: none (the pads play chords as before)
        int getPattern() const noexcept { return pattern.load(); }
        void play (std::vector<int> voicing, juce::uint8 velocity, int channel);
        void stop();
        bool isPlaying() const;
        std::vector<int> getSounding() const;
        void process();                                // what each timer tick does (tests call it)

    private:
        void hiResTimerCallback() override { process(); }
        double now() const;                            // the pattern's clock (beats)
        void endDue (double beat, bool all);
        std::function<void (const juce::MidiMessage&)> sendTo;
        std::atomic<int> pattern { -1 };
        Pattern current;   // (0.44.0) a copy of it: the list may change on the message thread meanwhile
        mutable juce::CriticalSection lock;
        std::vector<int> chord;
        juce::uint8 velocity = 100;
        int channel = 1;
        bool active = false, freeClock = true;
        double startedMs = 0.0, lastBeat = 0.0;
        struct Sounding { int note; double endBeat; };
        std::vector<Sounding> sounding;
    };
}
