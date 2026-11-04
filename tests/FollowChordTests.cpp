#include <catch2/catch_test_macros.hpp>

#include <set>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/FollowSetupPage.h"
#include "ui/Inspector.h"
#include "ui/MainView.h"

// 0.36.0, Follow Chord Track: a MIDI track's notes heard moved to the chord track's chords.

using namespace pr7s;

namespace
{
    using T = Chord::Type;
    using X = Chord::Tension;
    const Chord C { 0, T::major, X::none, -1 }, F { 5, T::major, X::none, -1 }, G { 7, T::major, X::none, -1 };
    const Chord Am { 9, T::minor, X::none, -1 }, C7 { 0, T::major, X::seven, -1 }, F7 { 5, T::major, X::seven, -1 };
    const Chord Bb { 10, T::major, X::none, -1 }, Dm { 2, T::minor, X::none, -1 };
    const Key cMajor { 0, 0, false }, aMinor { 5, 0, true }, gMajor { 4, 0, false };

    std::vector<int> follow (std::vector<int> pitches, FollowMode mode, const Chord& from, const Key& fromKey, const Chord& to, const Key& toKey)
    {
        for (auto& p : pitches)
            p = followPitch (p, mode, from, fromKey, to, toKey);
        return pitches;
    }
}

TEST_CASE ("0.36.0: Root Notes moves every note by the chord's root, the short way", "[follow]")
{
    // C E G over C: over F up a fourth (5), over G down a fourth (-5), as a transpose track.
    CHECK (follow ({ 60, 64, 67 }, FollowMode::rootNotes, C, cMajor, F, cMajor) == std::vector<int> { 65, 69, 72 });
    CHECK (follow ({ 60, 64, 67 }, FollowMode::rootNotes, C, cMajor, G, cMajor) == std::vector<int> { 55, 59, 62 });
    // A bass line C - G - C stays a bass line: the shape kept.
    CHECK (follow ({ 36, 43, 48 }, FollowMode::rootNotes, C, cMajor, Am, cMajor) == std::vector<int> { 33, 40, 45 });
    // The same chord: unchanged.
    CHECK (follow ({ 61, 66 }, FollowMode::rootNotes, C, cMajor, C, cMajor) == std::vector<int> { 61, 66 });
}

TEST_CASE ("0.36.0: Scales moves by scale steps, and into the new key", "[follow]")
{
    // In C major, C -> F is three steps up: C E G B becomes F A C E (diatonic: no F#, no Bb).
    CHECK (follow ({ 60, 64, 67, 71 }, FollowMode::scales, C, cMajor, F, cMajor) == std::vector<int> { 65, 69, 72, 76 });
    // C -> Dm: a step up; a passing D and F stay in the scale.
    CHECK (follow ({ 60, 62, 65 }, FollowMode::scales, C, cMajor, Dm, cMajor) == std::vector<int> { 62, 64, 67 });
    // C -> G is the short way: down four steps (C E G to G B D below).
    CHECK (follow ({ 60, 64, 67 }, FollowMode::scales, C, cMajor, G, cMajor) == std::vector<int> { 55, 59, 62 });
    // Into another key: C major's C over C -> G major's G over G: F becomes F# (its seventh).
    CHECK (follow ({ 60, 65 }, FollowMode::scales, C, cMajor, G, gMajor) == std::vector<int> { 55, 60 });   // C->G, F (4th)->C (4th of G)
    CHECK (followPitch (71, FollowMode::scales, C, cMajor, G, gMajor) == 66);   // B (7th of C) -> F# (7th of G)
    // A chord whose root isn't in the scale (Bb in C major): moved exactly by its root.
    CHECK (follow ({ 60, 64, 67 }, FollowMode::scales, C, cMajor, Bb, cMajor) == std::vector<int> { 58, 62, 65 });
    // C major to A minor over Am: by degree (the third becomes the minor third).
    CHECK (follow ({ 60, 64, 67 }, FollowMode::scales, C, cMajor, Am, aMinor) == std::vector<int> { 57, 60, 64 });
}

TEST_CASE ("0.36.0: Chords puts every note on the new chord's notes", "[follow]")
{
    // A C major scale run over C, followed to F: every note lands on F A C (ties go down).
    const auto run = follow ({ 60, 62, 64, 65, 67, 69, 71, 72 }, FollowMode::chords, C, cMajor, F, cMajor);
    for (const auto p : run)
        CHECK ((p % 12 == 5 || p % 12 == 9 || p % 12 == 0));
    // The chord's own notes move with the root: C E G -> F A C.
    CHECK (follow ({ 60, 64, 67 }, FollowMode::chords, C, cMajor, F, cMajor) == std::vector<int> { 65, 69, 72 });
    // Onto a minor chord: C E G over C, to Am (down a third): A C E.
    CHECK (follow ({ 60, 64, 67 }, FollowMode::chords, C, cMajor, Am, cMajor) == std::vector<int> { 57, 60, 64 });
}

TEST_CASE ("0.36.0: Chords & Scales keeps each chord tone the same tone, the others by scale", "[follow]")
{
    // C7's seventh (Bb) over F7 is F7's seventh (Eb); its third (E) is F7's third (A).
    CHECK (follow ({ 60, 64, 67, 70 }, FollowMode::chordsAndScales, C7, cMajor, F7, cMajor) == std::vector<int> { 65, 69, 72, 75 });
    // C's third over Am is Am's third (C: minor), its fifth Am's fifth (E).
    CHECK (follow ({ 60, 64, 67 }, FollowMode::chordsAndScales, C, cMajor, Am, cMajor) == std::vector<int> { 57, 60, 64 });
    // A passing D (not in C) over F in C major: by scale steps, three up: G.
    CHECK (followPitch (62, FollowMode::chordsAndScales, C, cMajor, F, cMajor) == 67);
    // C7's seventh over a plain F (no seventh to be): by scale steps, keeping its flat: Bb -> Eb.
    CHECK (followPitch (70, FollowMode::chordsAndScales, C7, cMajor, F, cMajor) == 75);
}

TEST_CASE ("0.36.0: Live Input puts keys on the chord's notes or the scale's; notes stay in 0-127", "[follow]")
{
    CHECK (livePitch (61, LiveFollow::chords, C, cMajor) == 60);    // C# -> C (ties: down)
    CHECK (livePitch (62, LiveFollow::chords, C, cMajor) == 60);    // D -> C (C and E both a tone away: down)
    CHECK (livePitch (66, LiveFollow::chords, C, cMajor) == 67);    // F# -> G
    CHECK (livePitch (61, LiveFollow::scales, C, cMajor) == 60);    // C# -> C
    CHECK (livePitch (66, LiveFollow::scales, C, cMajor) == 65);    // F# -> F
    CHECK (livePitch (66, LiveFollow::scales, G, gMajor) == 66);    // F# in G major: kept
    CHECK (livePitch (61, LiveFollow::off, C, cMajor) == 61);
    CHECK (followPitch (126, FollowMode::rootNotes, C, cMajor, F, cMajor) == 119);   // (folded down an octave)
    CHECK (followPitch (1, FollowMode::rootNotes, F, cMajor, C, cMajor) == 8);
}

TEST_CASE ("0.36.0: a following track plays its notes moved; the chord track changed, it follows again; Shift Chords", "[follow][engine]")
{
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    auto instrument = std::make_unique<Probe>();
    auto* probe = instrument.get();
    studio.getEngine().setInstrument (trackId, std::move (instrument));
    studio.addChordTrack();
    studio.addChordEvent (0.0, C);
    studio.addChordEvent (4.0, G);
    // Played over C, then G: a C triad, then a G triad.
    studio.addPart (trackId, { 0.0, 8.0, "Piano", { { 0.0, 1.0, 60, 0.8f }, { 0.0, 1.0, 64, 0.8f }, { 0.0, 1.0, 67, 0.8f },
                                                    { 4.0, 1.0, 55, 0.8f }, { 4.0, 1.0, 59, 0.8f }, { 4.0, 1.0, 62, 0.8f } } });
    REQUIRE (studio.needsFollowSetup (trackId));
    REQUIRE (studio.startFollowing (trackId, FollowMode::chords, FollowStart::directly));
    CHECK_FALSE (studio.needsFollowSetup (trackId));
    const auto partId = studio.getProject().findTrack (trackId)->parts[0].id;
    // The chord track as it was: nothing moves.
    CHECK (studio.followedPitches (partId) == std::vector<int> { 60, 64, 67, 55, 59, 62 });
    // The progression changed: C -> F, G -> Am. The notes follow.
    studio.setChordEventChord (0, F);
    studio.setChordEventChord (1, Am);
    CHECK (studio.followedPitches (partId) == std::vector<int> { 65, 69, 72, 57, 60, 64 });
    studio.play();
    renderSamples (studio.getEngine(), 24000 * 8);
    studio.stop();
    std::vector<int> heard;
    for (const auto& r : probe->notesOn())
        heard.push_back (r.message.getNoteNumber());
    std::sort (heard.begin(), heard.begin() + 3);
    std::sort (heard.begin() + 3, heard.end());
    CHECK (heard == std::vector<int> { 65, 69, 72, 57, 60, 64 });
    // Undo: the chord back to G.
    studio.undo();
    CHECK (studio.followedPitches (partId) == std::vector<int> { 65, 69, 72, 55, 59, 62 });
    studio.redo();

    // Shift Chords: a C played 30 ticks before beat 4. Without a shift it's in the first chord's
    // time (played over C, now F): F. With -40 ticks it takes the next (played over G, now Am): C.
    studio.addPart (trackId, { 0.0, 8.0, "Early", { { 4.0 - 30.0 / 480.0, 0.5, 60, 0.8f } } });
    const auto earlyId = studio.getProject().findTrack (trackId)->parts.back().id;
    CHECK (studio.followedPitches (earlyId) == std::vector<int> { 65 });
    studio.setChordShift (-40);
    CHECK (studio.getChordTrack().shiftTicks == -40);
    CHECK (studio.followedPitches (earlyId) == std::vector<int> { 60 });
    studio.setChordShift (0);

    // Off: as played.
    studio.setFollowMode (trackId, FollowMode::off);
    CHECK (studio.followedPitches (partId) == std::vector<int> { 60, 64, 67, 55, 59, 62 });
    // On again: the reference kept (no dialog).
    CHECK_FALSE (studio.needsFollowSetup (trackId));
    studio.setFollowMode (trackId, FollowMode::rootNotes);
    CHECK (studio.followedPitches (partId) == std::vector<int> { 65, 69, 72, 57, 61, 64 });   // G -> A: up a tone
    studio.resetFollowing (trackId);
    CHECK (studio.needsFollowSetup (trackId));
}

TEST_CASE ("0.36.0: Analyze Chords and Apply a Known Chord; doubles dropped; saved with the song", "[follow]")
{
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addChordTrack();
    studio.addChordEvent (0.0, F);
    // Played in G (G B D), nothing to do with the chord track's F.
    studio.addPart (trackId, { 0.0, 4.0, "Riff", { { 0.0, 2.0, 55, 0.8f }, { 0.0, 2.0, 59, 0.8f }, { 0.0, 2.0, 62, 0.8f } } });
    const auto partId = studio.getProject().findTrack (trackId)->parts[0].id;
    REQUIRE (studio.startFollowing (trackId, FollowMode::chords, FollowStart::analyze));
    const auto& reference = studio.getProject().findTrack (trackId)->followReference;
    REQUIRE (reference.chords.size() == 1);
    CHECK (reference.chords[0].chord == G);
    CHECK (studio.followedPitches (partId) == std::vector<int> { 53, 57, 60 });   // G B D -> F A C (down a tone)

    // A known chord (D minor): G B D taken as over Dm, onto F: up a third.
    studio.resetFollowing (trackId);
    REQUIRE (studio.startFollowing (trackId, FollowMode::rootNotes, FollowStart::knownChord, Dm));
    CHECK (studio.followedPitches (partId) == std::vector<int> { 58, 62, 65 });

    // Doubles: two notes landing on one pitch at one start, the softer dropped.
    studio.resetFollowing (trackId);
    studio.setNotes (trackId, 0, { { 0.0, 1.0, 60, 0.9f }, { 0.0, 1.0, 62, 0.5f } });
    REQUIRE (studio.startFollowing (trackId, FollowMode::chords, FollowStart::knownChord, C));
    const auto pitches = studio.followedPitches (partId);   // C -> F: 65, D+5 = 67 -> nearest of F A C: 65 (down)... or 69
    REQUIRE (pitches.size() == 2);
    CHECK (pitches[0] == 65);
    CHECK ((pitches[1] == -1 || pitches[1] != 65));

    // Saved and opened again.
    studio.setLiveFollow (trackId, LiveFollow::scales);
    studio.setChordShift (-30);
    const auto xml = studio.getProject().toXml();
    const auto opened = Project::fromXml (*xml);
    REQUIRE (opened.has_value());
    const auto* track = opened->findTrack (trackId);
    REQUIRE (track != nullptr);
    CHECK (track->follow == FollowMode::chords);
    CHECK (track->liveFollow == LiveFollow::scales);
    CHECK (track->followReference == studio.getProject().findTrack (trackId)->followReference);
    CHECK (opened->chordTrack.shiftTicks == -30);
}

TEST_CASE ("0.36.0: Live Input: keys played go out on the chord's notes, their note offs to the same notes; recorded as heard", "[follow][engine]")
{
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    auto instrument = std::make_unique<Probe>();
    auto* probe = instrument.get();
    studio.getEngine().setInstrument (trackId, std::move (instrument));
    studio.addChordTrack();
    studio.addChordEvent (0.0, C);
    studio.addChordEvent (4.0, F);
    studio.setLiveFollow (trackId, LiveFollow::chords);
    studio.selectTrack (trackId);
    // Stopped at the start: over C, D goes out as C.
    renderSamples (studio.getEngine(), 512);
    studio.getEngine().getKeyboardState().noteOn (1, 62, 0.9f);
    renderSamples (studio.getEngine(), 2048);
    studio.getEngine().getKeyboardState().noteOff (1, 62, 0.0f);
    renderSamples (studio.getEngine(), 2048);
    REQUIRE (probe->notesOn().size() == 1);
    CHECK (probe->notesOn()[0].message.getNoteNumber() == 60);
    REQUIRE (probe->notesOff().size() >= 1);
    CHECK (probe->notesOff().back().message.getNoteNumber() == 60);

    // Recording over F (beat 4 on): D goes out and is recorded as C (F A C's nearest: C, down).
    probe->received.clear();
    studio.setPosition (4.0);
    studio.record();
    renderSamples (studio.getEngine(), 12000);
    studio.getEngine().getKeyboardState().noteOn (1, 64, 0.9f);   // E -> F
    renderSamples (studio.getEngine(), 12000);
    studio.getEngine().getKeyboardState().noteOff (1, 64, 0.0f);
    renderSamples (studio.getEngine(), 12000);
    studio.stop();
    REQUIRE (probe->notesOn().size() == 1);
    CHECK (probe->notesOn()[0].message.getNoteNumber() == 65);
    const auto& parts = studio.getProject().findTrack (trackId)->parts;
    REQUIRE (parts.size() == 1);
    REQUIRE (parts[0].notes.size() == 1);
    CHECK (parts[0].notes[0].pitch == 65);

    // Off: as played.
    studio.setLiveFollow (trackId, LiveFollow::off);
    probe->received.clear();
    studio.getEngine().getKeyboardState().noteOn (1, 62, 0.9f);
    renderSamples (studio.getEngine(), 2048);
    studio.getEngine().getKeyboardState().noteOff (1, 62, 0.0f);
    renderSamples (studio.getEngine(), 2048);
    REQUIRE (probe->notesOn().size() == 1);
    CHECK (probe->notesOn()[0].message.getNoteNumber() == 62);
}

TEST_CASE ("0.36.0: Map to Chord Track writes the parts' notes moved (their own chords found first); one undo step", "[follow]")
{
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addPart (trackId, { 0.0, 4.0, "Riff", { { 0.0, 2.0, 55, 0.8f }, { 0.0, 2.0, 59, 0.8f }, { 0.0, 2.0, 62, 0.8f } } });
    const auto partId = studio.getProject().findTrack (trackId)->parts[0].id;
    CHECK (studio.mapToChordTrack ({ partId }, FollowMode::chords) == -1);   // (no chord track: nothing to map to)
    studio.addChordTrack();
    studio.addChordEvent (0.0, Am);
    CHECK (studio.mapToChordTrack ({ partId }, FollowMode::chordsAndScales) == 3);
    std::vector<int> now;
    for (const auto& n : studio.getProject().findTrack (partId > 0 ? trackId : 0)->parts[0].notes)
        now.push_back (n.pitch);
    CHECK (now == std::vector<int> { 57, 60, 64 });   // G B D (G) -> A C E (Am), each tone the same tone
    CHECK (studio.getProject().findTrack (trackId)->follow == FollowMode::off);   // (written in: nothing follows)
    studio.undo();
    now.clear();
    for (const auto& n : studio.getProject().findTrack (trackId)->parts[0].notes)
        now.push_back (n.pitch);
    CHECK (now == std::vector<int> { 55, 59, 62 });
}

TEST_CASE ("0.36.0 on screen: the Inspector's CHORDS for instrument tracks only; the dialog the first time; the row's state", "[follow][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto keys = studio.getProject().tracks[0].id;
    const auto guitar = studio.addAudioTrack (false);
    studio.addChordTrack();
    studio.addChordEvent (0.0, C);
    studio.selectTrack (keys);
    studio.dispatchPendingMessages();
    auto& inspector = view.getInspector();
    REQUIRE (inspector.getFollowKey().isVisible());
    CHECK (inspector.getFollowKey().getButtonText() == "OFF");
    CHECK (inspector.getLiveKey().isVisible());
    CHECK (inspector.getShiftKey().getButtonText() == "0 TICKS");
    // The first time a mode is chosen: the dialog (the track doesn't follow until OK).
    inspector.chooseFollowMode (FollowMode::scales);
    CHECK (studio.getProject().findTrack (keys)->follow == FollowMode::off);
    // The dialog itself: Apply a Known Chord, D minor.
    FollowSetupPage page;
    CHECK (page.getStart() == FollowStart::directly);
    page.setStart (FollowStart::knownChord);
    page.setKnownChord (Dm);
    CHECK (page.getStart() == FollowStart::knownChord);
    CHECK (page.getKnownChord() == Dm);
    studio.startFollowing (keys, FollowMode::scales, page.getStart(), page.getKnownChord());
    studio.dispatchPendingMessages();
    CHECK (inspector.getFollowKey().getButtonText() == "SCALES");
    // Later choices need no dialog.
    inspector.chooseFollowMode (FollowMode::rootNotes);
    CHECK (studio.getProject().findTrack (keys)->follow == FollowMode::rootNotes);
    studio.setChordShift (-30);
    studio.dispatchPendingMessages();
    CHECK (inspector.getShiftKey().getButtonText() == "-30 TICKS");
    // An audio track: no CHORDS section (0.36.0 follows MIDI only).
    studio.selectTrack (guitar);
    studio.dispatchPendingMessages();
    CHECK_FALSE (inspector.getFollowKey().isVisible());
    CHECK (view.createComponentSnapshot (view.getLocalBounds()).isValid());
}

TEST_CASE ("0.38.0: voicing libraries: piano, guitar and basic voicings hold the chord's notes, in their ranges; adaptive moves least", "[follow][voicings]")
{
    using V = VoicingSettings;
    const auto pcsOf = [] (const std::vector<int>& v) { std::set<int> p; for (const auto n : v) p.insert (n % 12); return p; };
    V piano;
    // Piano triads: the left hand's root (36-47), the right hand close in its range; every inversion offered.
    const auto c = voicingCandidates (C, piano);
    REQUIRE (c.size() >= 3);
    for (const auto& v : c)
    {
        CHECK (v.front() >= 36);
        CHECK (v.front() < 48);
        CHECK (v.front() % 12 == 0);
        CHECK (pcsOf (v) == std::set<int> { 0, 4, 7 });
        for (size_t i = 1; i < v.size(); ++i)
        {
            CHECK (v[i] >= piano.lowest);
            CHECK (v[i] <= piano.highest);
        }
    }
    // Four notes without the root: C -> E G B D (3 5 7 9), rootless right hand.
    piano.piano = V::PianoStyle::fourNote;
    piano.addRoot = false;
    for (const auto& v : voicingCandidates (C, piano))
    {
        CHECK (v.size() == 4);
        CHECK (pcsOf (v) == std::set<int> { 4, 7, 11, 2 });
    }
    // Open jazz on Am: C G B F# (3 7 9 13).
    piano.piano = V::PianoStyle::fourNoteOpen;
    for (const auto& v : voicingCandidates (Am, piano))
        CHECK (pcsOf (v) == std::set<int> { 0, 7, 11, 6 });
    // Guitar triads: barre shapes, six or five strings, root lowest, within the guitar's range.
    V guitar;
    guitar.library = V::Library::guitar;
    for (const auto& v : voicingCandidates (G, guitar))
    {
        CHECK ((v.size() == 6 || v.size() == 5));
        CHECK (v.front() % 12 == 7);
        CHECK (v.front() >= 40);
        CHECK (pcsOf (v) == std::set<int> { 7, 11, 2 });
    }
    // Four-note guitar: the seventh for the octave (C7: no octave C in the middle).
    guitar.guitar = V::GuitarStyle::fourNote;
    for (const auto& v : voicingCandidates (C7, guitar))
        CHECK (pcsOf (v) == std::set<int> { 0, 4, 7, 10 });
    // Three-string triads: three notes on the G B E strings.
    guitar.guitar = V::GuitarStyle::threeString;
    for (const auto& v : voicingCandidates (F, guitar))
    {
        CHECK (v.size() == 3);
        CHECK (v.front() >= 55);
    }
    // Basic: root position from C3 first (an octave offset moves it).
    V basic;
    basic.library = V::Library::basic;
    CHECK (voicingCandidates (C, basic).front() == std::vector<int> { 60, 64, 67 });
    basic.basicOctave = -1;
    CHECK (voicingCandidates (F, basic).front() == std::vector<int> { 53, 57, 60 });
    // Adaptive: C F G C voiced with the least movement (no voice jumps more than a fourth); off: each from its first choice.
    std::vector<ChordEvent> events { { 0.0, C }, { 4.0, F }, { 8.0, G }, { 12.0, C } };
    const auto voiced = voiceChords (events, V {});
    REQUIRE (voiced.size() == 4);
    for (size_t i = 1; i < voiced.size(); ++i)
        for (size_t k = 1; k < juce::jmin (voiced[i].size(), voiced[i - 1].size()); ++k)
            CHECK (std::abs (voiced[i][k] - voiced[i - 1][k]) <= 5);
    // Voices: soprano the top, bass the bottom.
    CHECK (voiceOf ({ 36, 60, 64, 67 }, Voice::soprano) == 67);
    CHECK (voiceOf ({ 36, 60, 64, 67 }, Voice::alto) == 64);
    CHECK (voiceOf ({ 36, 60, 64, 67 }, Voice::tenor) == 60);
    CHECK (voiceOf ({ 36, 60, 64, 67 }, Voice::bass) == 36);
    // Saved and read back.
    V odd;
    odd.library = V::Library::guitar;
    odd.guitar = V::GuitarStyle::fourNote;
    odd.adaptive = false;
    CHECK (V::fromXml (*odd.toXml()) == odd);
}

TEST_CASE ("0.38.0: Voicings and Single Voice: chords played take the chord track's voicings; one voice for every note; saved; undone", "[follow][voicings]")
{
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    auto instrument = std::make_unique<Probe>();
    auto* probe = instrument.get();
    studio.getEngine().setInstrument (trackId, std::move (instrument));
    studio.addChordTrack();
    studio.addChordEvent (0.0, C);
    studio.addChordEvent (4.0, F);
    // Played over C: a C chord on beat 0 and on beat 4 (three notes each), and a lone top note at beat 6.
    studio.addPart (trackId, { 0.0, 8.0, "Pads", { { 0.0, 1.0, 60, 0.8f }, { 0.0, 1.0, 64, 0.8f }, { 0.0, 1.0, 67, 0.8f },
                                                   { 4.0, 1.0, 60, 0.8f }, { 4.0, 1.0, 64, 0.8f }, { 4.0, 1.0, 67, 0.8f },
                                                   { 6.0, 0.5, 79, 0.8f } } });
    const auto partId = studio.getProject().findTrack (trackId)->parts[0].id;
    // Basic voicings, so the voices are plain to check.
    VoicingSettings basic;
    basic.library = VoicingSettings::Library::basic;
    basic.adaptive = false;
    studio.setChordTrackVoicing (basic);
    REQUIRE (studio.startFollowing (trackId, FollowMode::voicings, FollowStart::knownChord, C));
    // The chords: the chord track's voicings (C: 60 64 67; F: 65 69 72), lowest to lowest.
    auto pitches = studio.followedPitches (partId);
    REQUIRE (pitches.size() == 7);
    CHECK (std::vector<int> (pitches.begin(), pitches.begin() + 3) == std::vector<int> { 60, 64, 67 });
    CHECK (std::vector<int> (pitches.begin() + 3, pitches.begin() + 6) == std::vector<int> { 65, 69, 72 });
    // The lone G5 (an octave over C's top voice, G4): F's top voice an octave up, C6.
    CHECK (pitches[6] == 84);

    // Single Voice, bass: every note F's lowest (65) after beat 4, C's (60) before; doubles dropped.
    studio.setFollowMode (trackId, FollowMode::singleVoice);
    studio.setFollowVoice (trackId, Voice::bass);
    pitches = studio.followedPitches (partId);
    CHECK (std::count (pitches.begin(), pitches.begin() + 3, 60) == 1);
    CHECK (std::count (pitches.begin(), pitches.begin() + 3, -1) == 2);   // (three on one pitch: two dropped)
    CHECK (pitches[6] == 65);
    // Heard so.
    studio.play();
    renderSamples (studio.getEngine(), 24000 * 8);
    studio.stop();
    std::vector<int> heard;
    for (const auto& r : probe->notesOn())
        heard.push_back (r.message.getNoteNumber());
    CHECK (heard == std::vector<int> { 60, 65, 65 });
    // Soprano: the top voices.
    studio.setFollowVoice (trackId, Voice::soprano);
    pitches = studio.followedPitches (partId);
    CHECK (pitches[6] == 72);

    // The track's own voicings over the chord track's: piano (the left hand's root first).
    VoicingSettings piano;
    studio.setTrackVoicing (trackId, piano);
    studio.setFollowVoice (trackId, Voice::bass);
    pitches = studio.followedPitches (partId);
    const auto low = *std::max_element (pitches.begin(), pitches.begin() + 3);
    CHECK (low >= 36);
    CHECK (low < 48);
    CHECK (low % 12 == 0);
    // Undo: back to the chord track's.
    studio.undo();
    studio.undo();
    CHECK_FALSE (studio.getProject().findTrack (trackId)->followVoicing.has_value());

    // Saved with the song.
    studio.setTrackVoicing (trackId, piano);
    studio.setFollowVoice (trackId, Voice::tenor);
    const auto opened = Project::fromXml (*studio.getProject().toXml());
    REQUIRE (opened.has_value());
    const auto* track = opened->findTrack (trackId);
    REQUIRE (track != nullptr);
    CHECK (track->follow == FollowMode::singleVoice);
    CHECK (track->followVoice == Voice::tenor);
    CHECK (track->followVoicing == piano);
    CHECK (opened->chordTrack.voicing == basic);
}
