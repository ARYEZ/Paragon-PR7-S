#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "Lines.h"

#include <optional>

// (0.82.0, the owner asked) Reading the owner's band's record so the idea engine can learn from what
// the band actually played: per song a drum MIDI file and a DI (the dry, unamplified signal) per player,
// all tracked to a click at one tempo and starting on the same beat. This part turns each DI into notes
// (when, which pitch, how hard, palm muted or open, bent, with vibrato); the studio puts them on tracks
// beside the DIs in a review song, where the owner corrects them in the MIDI editor. What's learned from
// the corrected songs comes later (0.83.0). Ours throughout: plain functions on sounds, no screen.
namespace pr7s
{
    // Who played a file.
    enum class BandRole { unknown, drums, rhythm, lead, bass };
    const std::vector<BandRole>& bandRoles();     // drums, rhythm, lead, bass
    juce::String bandRoleName (BandRole);         // "DRUMS", "RHYTHM GUITAR", "LEAD GUITAR", "BASS"
    // From a file's name: a MIDI file is the drums; a WAV naming bass, lead/solo or rhythm (rhy, rtm) is that
    // player's; "gtr 1" / "guitar 1" rhythm, "gtr 2" lead; else unknown (the page asks).
    BandRole guessBandRole (const juce::File&);

    // A note heard on a DI. Seconds from the file's start (= the song's first beat).
    struct PlayedNote
    {
        double start = 0.0, end = 0.0;
        int pitch = 40;                 // the note picked (a power chord: its root)
        bool powerChord = false;        // the fifth (and octave) with it
        float level = 0.7f;             // 0-1, how hard: the attack against the file's loudest
        bool muted = false;             // palm muted (rhythm guitar)
        float bend = 0.0f;              // semitones it was bent up to (0: none)
        float vibrato = 0.0f;           // its vibrato's depth in semitones (0: none)
        bool operator== (const PlayedNote&) const = default;
    };

    // Single-note lines (bass, lead): the pitch every 5 ms (bass from 25 Hz), cut into notes where the
    // pitch moves to another note or a new pick attack is heard (repeated notes on one pitch are split);
    // a glide with no new attack is a bend (or a slide down: kept as two notes), not a new note.
    std::vector<PlayedNote> transcribeLine (const juce::AudioBuffer<float>&, double sampleRate, BandRole);
    // Rhythm guitar: a hit at each pick attack, its lowest note found from the harmonics (its own
    // fundamental must be there, so an octave under isn't taken), a fifth above it = a power chord,
    // muted when it dies away fast and dull (against the file's own hits), held until the next hit or
    // it dies away.
    std::vector<PlayedNote> transcribeRhythm (const juce::AudioBuffer<float>&, double sampleRate);

    // The notes as an idea (beats from the song's start at `bpm`): guitars in the owner's velocity
    // layers (muted in the mute layer, open in the sustain layer, by how hard), bends and vibrato as
    // the effects the studio writes as controllers; bass and anything else by level alone.
    LineIdea playedIdea (const std::vector<PlayedNote>&, double bpm, BandRole, const GuitarVelocities&);

    // A song of the record: its drum MIDI, its DIs and who played each, its tempo and signature (from
    // the drum MIDI when it has them; `tempoFromFile` false: 120 was assumed and should be typed).
    struct BandSong
    {
        juce::String name;
        juce::File drums;
        std::vector<std::pair<juce::File, BandRole>> dis;
        double bpm = 120.0;
        bool tempoFromFile = false;
        int numerator = 4, denominator = 4;
        juce::File fileOf (BandRole) const;   // the first DI of that role (none: an empty file)
    };
    // A folder of the record: each subfolder with audio or MIDI in it a song; files loose in the folder
    // grouped into songs by the name before " - ", "_" or a role word ("Song 3 bass.wav", "Song 3.mid").
    std::vector<BandSong> scanBandFolder (const juce::File&);

    // The review song of a song of the record, written as <into>/<song>/<song>.pr7s with the DIs copied into
    // its "<song> Audio" folder: the drummer's MIDI on a drum track (the GM map), each DI on an audio track
    // and under it an instrument track with what was heard on it ("<player> (heard)"), at the song's tempo
    // and signature. Opened like any song; corrected in the MIDI editor; saved. The studio's own song is
    // untouched (it can run on any thread). `progress` (0-1) false: stop. Bend ranges: the instruments the
    // heard parts will play (Studio::profileFor's, else 2).
    struct BandReviewOptions
    {
        GuitarVelocities velocities;
        int guitarBendRange = 2, leadBendRange = 2, bassBendRange = 2;
    };
    bool makeBandReviewSong (const BandSong&, const juce::File& into, const BandReviewOptions&, juce::File& made,
                             juce::String& error, const std::function<bool (double)>& progress = {}, juce::String* report = nullptr);

    // (0.82.0, the owner's first song: its drum MIDI started 8 bars before the DIs, and the DIs ran 0.23 % slow
    // against the click: 120 BPM played back as 119.72, a sample rate a hair off somewhere) Where the drummer's kicks
    // land among the guitar and bass notes: onset (seconds) = (kick beat + `drumShiftBeats`) x 60 / bpm x `diStretch`.
    // The best fit of shift (+-64 beats) and stretch (+-1 %), kept only when clearly better than none (else 0 / 1);
    // a whole number of bars (`barBeats` long) preferred when it fits within 5 % as well.
    struct BandAlignment
    {
        double drumShiftBeats = 0.0, diStretch = 1.0;
        int matched = 0, kicks = 0;   // kicks with a note within 30 ms at the fit
    };
    BandAlignment alignBand (const std::vector<double>& kickBeats, std::vector<double> onsetSeconds, double bpm, double barBeats = 4.0);
    // Documents/Paragon PR7-S/Band Record (where the review songs go).
    juce::File defaultBandReviewFolder();
}
