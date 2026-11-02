#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/Chords.h"
#include "engine/Studio.h"

using namespace pr7s;

namespace
{
    std::set<int> classesOf (const std::vector<int>& notes)
    {
        std::set<int> s;
        for (const auto n : notes)
            s.insert (n % 12);
        return s;
    }

    struct Sent
    {
        std::vector<juce::MidiMessage> messages;
        std::vector<int> ons() const { std::vector<int> v; for (auto& m : messages) if (m.isNoteOn()) v.push_back (m.getNoteNumber()); return v; }
        std::vector<int> offs() const { std::vector<int> v; for (auto& m : messages) if (m.isNoteOff()) v.push_back (m.getNoteNumber()); return v; }
    };
}

TEST_CASE ("Chords: their notes and names (note names, roman numerals, Nashville numbers)", "[chords]")
{
    using T = Chord::Type;
    using X = Chord::Tension;
    CHECK ((Chord { 0, T::major, X::none, -1 }.intervals() == std::vector<int> { 0, 4, 7 }));
    CHECK ((Chord { 2, T::minor, X::seven, -1 }.pitchClasses() == std::vector<int> { 2, 5, 9, 0 }));
    CHECK (Chord { 0, T::major, X::none, -1 }.name() == "C");
    CHECK (Chord { 2, T::minor, X::seven, -1 }.name() == "Dm7");
    CHECK (Chord { 11, T::diminished, X::seven, -1 }.name() == "Bm7b5");
    CHECK (Chord { 7, T::sus4, X::seven, -1 }.name() == "G7sus4");
    CHECK (Chord { 0, T::major, X::none, 4 }.name() == "C/E");
    CHECK (Chord { 10, T::major, X::maj7, -1 }.name (true) == "Bbmaj7");
    CHECK (Chord { 10, T::major, X::maj7, -1 }.name (false) == "A#maj7");
    CHECK (Chord { 2, T::minor, X::seven, -1 }.romanName (0) == "ii7");
    CHECK (Chord { 7, T::major, X::seven, -1 }.romanName (0) == "V7");
    CHECK (Chord { 10, T::major, X::none, -1 }.romanName (0) == "bVII");
    CHECK (Chord { 9, T::minor, X::none, -1 }.nashvilleName (0) == "6m");
    CHECK (Chord { 7, T::major, X::none, 11 }.nashvilleName (0) == "5/7");
    // Saved and read back.
    const Chord c { 5, T::minor, X::nine, 0 };
    CHECK (Chord::fromText (c.toText()) == c);
    CHECK_FALSE (Chord::fromText ("13 0 0 -1").has_value());
    // More and fewer tensions along the type's ladder.
    CHECK (Chord { 9, T::minor, X::none, -1 }.withTensionStep (1).tension == X::seven);
    CHECK (Chord { 9, T::minor, X::seven, -1 }.withTensionStep (1).tension == X::nine);
    CHECK (Chord { 7, T::major, X::seven, -1 }.withTensionStep (1).tension == X::nine);
    CHECK (Chord { 0, T::major, X::none, -1 }.withTensionStep (-1).tension == X::none);
}

TEST_CASE ("Chords recognised from the notes played (Assign from MIDI Input)", "[chords]")
{
    CHECK (recogniseChord ({ 60, 64, 67 })->name() == "C");
    CHECK (recogniseChord ({ 52, 60, 67 })->name() == "C/E");
    CHECK (recogniseChord ({ 57, 60, 64, 67 })->name() == "Am7");
    CHECK (recogniseChord ({ 60, 64, 67, 69 })->name() == "C6");
    CHECK (recogniseChord ({ 55, 59, 62, 65 })->name() == "G7");
    CHECK (recogniseChord ({ 62, 65, 69, 72, 76 })->name() == "Dm9");
    CHECK_FALSE (recogniseChord ({ 60 }).has_value());
    CHECK_FALSE (recogniseChord ({ 60, 61, 62, 63 }).has_value());
}

TEST_CASE ("The scale presets fill the pads with the scale's chords, in the root key", "[chords]")
{
    ChordPads pads;
    pads.fillFromScale (ChordPads::Scale::major, 0);
    juce::StringArray names;
    for (const auto& p : pads.pads)
        names.add (p.chord ? p.chord->name() : "-");
    CHECK (names.joinIntoString (" ") == "C Cmaj7 Dm Dm7 Em F Fmaj7 G G7 Am Am7 Bdim");   // (white keys: the scale; black: its sevenths)
    pads.fillFromScale (ChordPads::Scale::naturalMinor, 9);
    CHECK (pads.pads[0].chord->name() == "Am");
    CHECK (pads.pads[4].chord->name() == "C");
    CHECK (pads.pads[2].chord->name() == "Bdim");
    CHECK (pads.rootKey == 9);
    CHECK (pads.displayName (*pads.pads[7].chord, ChordPads::Display::roman) == "v");
    // A grid: in order.
    pads.layout = ChordPads::Layout::grid;
    pads.fillFromScale (ChordPads::Scale::major, 0);
    CHECK (pads.pads[1].chord->name() == "Dm");
    CHECK (pads.pads[7].chord->name() == "Cmaj7");
    pads.layout = ChordPads::Layout::keyboard;
    pads.fillFromScale (ChordPads::Scale::dorian, 2);
    CHECK (pads.pads[5].chord->name() == "G");   // the Dorian's major IV
    // A new song's pads: the major scale in C.
    CHECK (Project().chordPads.pads[7].chord->name() == "G");
}

TEST_CASE ("Voicings: close inversions in range; adaptive voicing moves the voices least, near the reference", "[chords]")
{
    const Chord c { 0, Chord::Type::major, Chord::Tension::none, -1 };
    const auto voicings = voicingsFor (c, 48);
    REQUIRE (voicings.size() >= 3);
    for (const auto& named : namedVoicings (c, 48))
    {
        const auto& v = named.notes;
        CHECK (classesOf (v) == std::set<int> { 0, 4, 7 });
        CHECK (std::is_sorted (v.begin(), v.end()));
        if (named.close)   // (open and spread voicings reach lower)
        {
            CHECK (v.front() >= 48);
            CHECK (v.front() < 66);
        }
    }
    // A slash chord's bass under the voicing.
    const auto slash = voicingsFor (Chord { 0, Chord::Type::major, Chord::Tension::none, 4 }, 48);
    for (const auto& v : slash)
    {
        CHECK (v.front() % 12 == 4);
        CHECK (v.front() < v[1]);
    }
    // After C E G (60 64 67), F goes to C F A (60 65 69): the least movement.
    const Chord f { 5, Chord::Type::major, Chord::Tension::none, -1 };
    const auto fv = voicingsFor (f, 48);
    const auto chosen = fv[(size_t) closestVoicing (fv, { 60, 64, 67 }, {})];
    CHECK ((chosen == std::vector<int> { 60, 65, 69 }));
    // Nothing before: the middle one.
    CHECK (closestVoicing (fv, {}, {}) == (int) fv.size() / 2);
}

TEST_CASE ("The pad player: a pad's chord on and off; Hold First, Legato and Stop First", "[chords]")
{
    ChordPads pads;
    pads.fillFromScale (ChordPads::Scale::major, 0);
    for (auto& p : pads.pads)
        p.adaptive = false;
    pads.pads[0].notes = { 60, 64, 67 };   // C
    pads.pads[5].notes = { 60, 64, 69 };   // Am (C and E in common)
    PadPlayer player;
    Sent sent;
    player.send = [&] (const juce::MidiMessage& m) { sent.messages.push_back (m); };

    pads.overlap = ChordPads::Overlap::legato;
    player.setPads (pads);
    player.press (0, 100);
    CHECK ((sent.ons() == std::vector<int> { 60, 64, 67 }));
    CHECK (player.isPressed (0));
    sent.messages.clear();
    player.press (5, 100);   // C still held: G goes, A comes, C and E carry on
    CHECK ((sent.offs() == std::vector<int> { 67 }));
    CHECK ((sent.ons() == std::vector<int> { 69 }));
    sent.messages.clear();
    player.release (0);
    CHECK (sent.messages.empty());   // (its notes now Am's)
    player.release (5);
    CHECK (classesOf (sent.offs()) == std::set<int> { 0, 4, 9 });
    CHECK (player.getSounding().empty());

    sent.messages.clear();
    pads.overlap = ChordPads::Overlap::stopFirst;
    player.setPads (pads);
    player.press (0, 100);
    sent.messages.clear();
    player.press (5, 100);   // all of C stops, all of Am starts
    CHECK (sent.offs().size() == 3);
    CHECK (sent.ons().size() == 3);
    player.releaseAll();

    sent.messages.clear();
    pads.overlap = ChordPads::Overlap::holdFirst;
    player.setPads (pads);
    player.press (0, 100);
    sent.messages.clear();
    player.press (5, 100);   // C held on; only A is new
    CHECK (sent.offs().empty());
    CHECK ((sent.ons() == std::vector<int> { 69 }));
    sent.messages.clear();
    player.release (0);      // G ends; C and E are Am's too
    CHECK ((sent.offs() == std::vector<int> { 67 }));
    player.releaseAll();
    CHECK (player.getSounding().empty());
}

TEST_CASE ("The trigger keys (C1 to B1) play the pads; other keys pass on; latch", "[chords]")
{
    ChordPads pads;
    pads.fillFromScale (ChordPads::Scale::major, 0);
    PadPlayer player;
    Sent sent;
    player.send = [&] (const juce::MidiMessage& m) { sent.messages.push_back (m); };
    std::vector<int> others;
    player.onOtherKey = [&] (int note, bool on) { if (on) others.push_back (note); };
    player.setPads (pads);
    CHECK (player.handle (juce::MidiMessage::noteOn (1, 36 + 4, (juce::uint8) 90)));   // E1: Em
    CHECK (classesOf (sent.ons()) == std::set<int> { 4, 7, 11 });
    CHECK (sent.messages.front().getVelocity() == 90);
    CHECK (player.handle (juce::MidiMessage::noteOff (1, 40)));
    CHECK (player.getSounding().empty());
    CHECK_FALSE (player.handle (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 90)));   // a key to play: not taken
    CHECK ((others == std::vector<int> { 60 }));

    // Latch: plays until pressed again.
    pads.latch = true;
    player.setPads (pads);
    sent.messages.clear();
    player.press (0, 100);
    player.release (0);
    CHECK (sent.offs().empty());
    CHECK (player.isPressed (0));
    player.press (0, 100);
    CHECK (sent.offs().size() == 3);
    CHECK_FALSE (player.isPressed (0));
}

TEST_CASE ("Chord pads in the studio: kept with the song; edited (transpose, tensions, voicings, lock, swap, copy, root key)", "[chords][studio]")
{
    Studio studio (false);
    auto all = std::vector<int> {};
    for (int i = 0; i < 12; ++i)
        all.push_back (i);

    studio.transposePads ({ 0 }, 2);
    CHECK (studio.getChordPads().pads[0].chord->name() == "D");
    studio.changePadTensions ({ 0 }, 1);
    CHECK (studio.getChordPads().pads[0].chord->name() == "Dadd9");
    studio.changePadTensions ({ 0 }, -1);

    // A voicing chosen by hand: adaptive voicing off (the manual).
    const auto before = studio.getPadVoicing (1);
    studio.changePadVoicings ({ 1 }, 1);
    CHECK_FALSE (studio.getChordPads().pads[1].adaptive);
    CHECK (studio.getPadVoicing (1) == juce::jmin (before + 1, studio.getPadVoicingCount (1) - 1));

    // Locked: nothing changes it.
    studio.setPadsLocked ({ 2 }, true);
    studio.transposePads ({ 2 }, 1);
    studio.removeChords ({ 2 });
    CHECK (studio.getChordPads().pads[2].chord->name() == "Dm");

    studio.swapPads (3, 4);
    CHECK (studio.getChordPads().pads[3].chord->name() == "Em");
    CHECK (studio.getChordPads().pads[4].chord->name() == "Dm7");
    studio.copyPad (3, 6);
    CHECK (studio.getChordPads().pads[6].chord->name() == "Em");

    // The root key: without transposing, the names' functions change; with, the chords move.
    studio.setPadsRootKey (7, false);
    CHECK (studio.getChordPads().displayName (*studio.getChordPads().pads[3].chord, ChordPads::Display::roman) == "vi");
    studio.setPadsRootKey (9, true);   // G to A: everything up a tone (the locked pad stays)
    CHECK (studio.getChordPads().pads[3].chord->name() == "F#m");
    CHECK (studio.getChordPads().pads[2].chord->name() == "Dm");

    studio.removeChords ({ 5 });
    CHECK_FALSE (studio.getChordPads().pads[5].chord.has_value());
    CHECK (studio.isModified());

    // Saved with the song, and opened again.
    const auto file = juce::File::createTempFile (Project::fileExtension);
    REQUIRE (studio.save (file));
    const auto saved = studio.getChordPads();
    studio.newProject();
    CHECK (studio.getChordPads().pads[3].chord->name() == "Dm7");
    juce::String problems;
    REQUIRE (studio.open (file, problems));
    CHECK (studio.getChordPads() == saved);
    studio.getLibrary().removeRecent (file);
    file.deleteFile();
}

TEST_CASE ("Chord pads: assigned from notes played in; a pad dragged onto a track makes a one-bar part; presets", "[chords][studio]")
{
    Studio studio (false);
    studio.setAssignFromInput ({ 0, 1 });
    for (const auto n : { 57, 60, 64 })
        studio.padKeyPlayed (n, true);
    for (const auto n : { 57, 60, 64 })
        studio.padKeyPlayed (n, false);
    CHECK (studio.getChordPads().pads[0].chord->name() == "Am");
    CHECK ((studio.getPadNotes (0) == std::vector<int> { 57, 60, 64 }));   // the voicing as played
    CHECK ((studio.getAssignFromInput() == std::vector<int> { 1 }));       // the next selected pad is next
    studio.setAssignFromInput ({});

    const auto track = studio.getProject().tracks[0].id;
    const auto id = studio.addPartFromPad (7, track, 5.0);   // G, dropped in bar 2
    REQUIRE (id != 0);
    const auto at = studio.findPart (id);
    REQUIRE (at.has_value());
    const auto& part = studio.getProject().findTrack (at->first)->parts[(size_t) at->second];
    CHECK (part.start == 4.0);
    CHECK (part.length == 4.0);
    CHECK (part.name == "G");
    CHECK (classesOf ([&] { std::vector<int> v; for (auto& n : part.notes) v.push_back (n.pitch); return v; }()) == std::set<int> { 7, 11, 2 });
    CHECK (studio.addPartFromPad (7, studio.addAudioTrack (false), 0.0) == 0);   // not onto an audio track

    // A preset saved, then loaded: chords only keeps the player; players only keeps the chords.
    const auto preset = juce::File::createTempFile (".pr7pads");
    auto pads = studio.getChordPads();
    pads.octaveOffset = 1;
    pads.overlap = ChordPads::Overlap::holdFirst;
    studio.setChordPads (pads);
    REQUIRE (studio.saveChordPadsPreset (preset));
    studio.loadScalePreset (ChordPads::Scale::phrygian, 4);
    CHECK (studio.getChordPads().pads[2].chord->name() == "F");
    auto reset = studio.getChordPads();
    reset.octaveOffset = 0;
    studio.setChordPads (reset);
    REQUIRE (studio.loadChordPadsPreset (preset, Studio::PadsPresetLoad::playersOnly));
    CHECK (studio.getChordPads().octaveOffset == 1);
    CHECK (studio.getChordPads().pads[2].chord->name() == "F");   // the chords kept
    REQUIRE (studio.loadChordPadsPreset (preset, Studio::PadsPresetLoad::chordsOnly));
    CHECK (studio.getChordPads().pads[0].chord->name() == "Am");
    preset.deleteFile();
}

TEST_CASE ("A pad pressed plays its chord on the live track's instrument (and is recorded as played)", "[chords][studio][engine]")
{
    Studio studio (false);
    const auto track = studio.getProject().tracks[0].id;
    auto probe = std::make_unique<Probe>();
    auto* heard = probe.get();
    studio.getEngine().setInstrument (track, std::move (probe));
    studio.selectTrack (track);
    studio.getPadPlayer().press (7, 100);   // G
    juce::AudioBuffer<float> out (2, 512);
    studio.getEngine().render (out, 512);
    std::set<int> notes;
    for (const auto& r : heard->notesOn())
        notes.insert (r.message.getNoteNumber() % 12);
    CHECK (notes == std::set<int> { 7, 11, 2 });
    studio.getPadPlayer().release (7);
    studio.getEngine().render (out, 512);
    CHECK (heard->notesOff().size() == 3);

    // Its trigger keys from a device play the pads only while they're shown.
    auto& input = studio.getEngine().getLiveInput();
    input.handleIncomingMidiMessage (nullptr, juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100).withTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001));
    studio.getEngine().render (out, 512);
    CHECK (heard->notesOn().back().message.getNoteNumber() == 36);   // pads put away: C1 is just C1
    studio.setPadsActive (true);
    input.handleIncomingMidiMessage (nullptr, juce::MidiMessage::noteOn (1, 38, (juce::uint8) 100).withTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001));
    studio.getEngine().render (out, 512);
    CHECK (studio.getPadPlayer().isPressed (2));
    studio.setPadsActive (false);
    CHECK_FALSE (studio.getPadPlayer().isPressed (2));
}

#include "ui/Finish.h"
#include "ui/MainView.h"

namespace
{
    juce::MouseEvent padMouse (juce::Component& target, juce::Point<float> at, juce::ModifierKeys mods = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier))
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, mods, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, now, at, now, 1, false);
    }
}

TEST_CASE ("The chord pads zone: F4 opens it (the trigger keys live), one zone docked at a time; pads played, selected, edited, dropped", "[chords][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    CHECK_FALSE (view.isChordPadsOpen());
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F4Key)));
    CHECK (view.isChordPadsOpen());
    CHECK (studio.arePadsActive());
    view.openMixer();   // docked too: the pads go away (one docked at a time)
    CHECK_FALSE (view.isChordPadsOpen());
    CHECK_FALSE (studio.arePadsActive());
    view.openChordPads();
    CHECK_FALSE (view.isMixerOpen());

    auto& pads = view.getChordPads();
    REQUIRE (pads.getWidth() > 0);
    // A click on G plays it (while held) and selects it.
    const auto g = pads.getPadBounds (7).getCentre();
    CHECK (pads.padAt (g) == 7);
    pads.mouseDown (padMouse (pads, g));
    CHECK (studio.getPadPlayer().isPressed (7));
    CHECK ((pads.getSelection() == std::vector<int> { 7 }));
    pads.mouseUp (padMouse (pads, g));
    CHECK_FALSE (studio.getPadPlayer().isPressed (7));

    // The wheel over it: the next voicing (by hand: adaptive off).
    const auto before = studio.getPadVoicing (7);
    juce::MouseWheelDetails wheel { 0.0f, -0.2f, false, false, false };
    pads.mouseWheelMove (padMouse (pads, g, {}), wheel);
    CHECK_FALSE (studio.getChordPads().pads[7].adaptive);
    CHECK (studio.getPadVoicing (7) == juce::jmin (before + 1, studio.getPadVoicingCount (7) - 1));
    // Shift + the wheel up: transposed.
    juce::MouseWheelDetails up { 0.0f, 0.2f, false, false, false };
    pads.mouseWheelMove (padMouse (pads, g, juce::ModifierKeys (juce::ModifierKeys::shiftModifier)), up);
    CHECK (studio.getChordPads().pads[7].chord->name() == "G#");

    // The chord editor: a root, a type, a tension, a bass.
    pads.setEditorShown (true);
    pads.editRoot (2);
    pads.editType (Chord::Type::minor);
    pads.editTension (Chord::Tension::seven);
    pads.editBass (0);
    CHECK (studio.getChordPads().pads[7].chord->name() == "Dm7/C");

    // Dropped on another pad: swapped (Alt: copied).
    juce::DragAndDropTarget::SourceDetails swap { juce::var ("chordpad:7"), &pads, pads.getPadBounds (0).getCentre().toInt() };
    pads.itemDropped (swap);
    CHECK (studio.getChordPads().pads[0].chord->name() == "Dm7/C");
    CHECK (studio.getChordPads().pads[7].chord->name() == "C");
    juce::DragAndDropTarget::SourceDetails copy { juce::var ("chordpad:0:copy"), &pads, pads.getPadBounds (9).getCentre().toInt() };
    pads.itemDropped (copy);
    CHECK (studio.getChordPads().pads[9].chord->name() == "Dm7/C");
    CHECK (studio.getChordPads().pads[0].chord->name() == "Dm7/C");

    // Dropped on a track in the arrangement: a one-bar part at the bar.
    auto& arrange = view.getArrangeView();
    const auto track = studio.getProject().tracks[0].id;
    auto rowY = -1;
    for (int y = 0; y < arrange.getHeight() && rowY < 0; ++y)
        if (arrange.trackAtY ((float) y) == track)
            rowY = y + 10;
    REQUIRE (rowY > 0);
    juce::DragAndDropTarget::SourceDetails drop { juce::var ("chordpad:7"), &pads,
                                                  juce::Point<int> ((int) view.getTimeline().xForBeat (5.0), rowY) };
    CHECK (arrange.isInterestedInDragSource (drop));
    arrange.itemDropped (drop);
    REQUIRE (studio.getProject().findTrack (track)->parts.size() == 1);
    CHECK (studio.getProject().findTrack (track)->parts[0].start == 4.0);
    CHECK (studio.getProject().findTrack (track)->parts[0].name == "C");

    // Delete: the selected pads' chords removed (here without the question).
    pads.selectPads ({ 9 });
    pads.removeSelected (false);
    CHECK_FALSE (studio.getChordPads().pads[9].chord.has_value());

    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F4Key)));
    CHECK_FALSE (view.isChordPadsOpen());
    CHECK_FALSE (studio.arePadsActive());
}

namespace
{
    juce::String chordsOf (const Key& key)
    {
        juce::StringArray names;
        const auto chords = keyChords (key);
        for (size_t i = 0; i < (key.minor ? 9u : 7u); ++i)
            names.add (chords[i].name (key));
        return names.joinIntoString (" ");
    }
    juce::String numeralsOf (const Key& key)
    {
        juce::StringArray names;
        const auto chords = keyChords (key);
        for (size_t i = 0; i < (key.minor ? 9u : 7u); ++i)
            names.add (chords[i].romanName (key));
        return names.joinIntoString (" ");
    }
    Key keyNamed (const juce::String& name)
    {
        for (const auto& k : Key::all())
            if (k.name() == name)
                return k;
        FAIL ("no key " << name);
        return {};
    }
}

TEST_CASE ("Keys: all 30, spelled as their key signatures spell them; every chord of every key as a textbook writes it (0.15.0)", "[chords][keys30]")
{
    REQUIRE (Key::all().size() == 30);
    // The key signatures.
    CHECK (keyNamed ("C major").signature() == 0);
    CHECK (keyNamed ("F# major").signature() == 6);
    CHECK (keyNamed ("Cb major").signature() == -7);
    CHECK (keyNamed ("Eb minor").signature() == -6);
    CHECK (keyNamed ("G# minor").signature() == 5);

    // Every major key's seven chords (I ii iii IV V vi vii°).
    const std::pair<const char*, const char*> majors[] = {
        { "C major",  "C Dm Em F G Am Bdim" },
        { "G major",  "G Am Bm C D Em F#dim" },
        { "D major",  "D Em F#m G A Bm C#dim" },
        { "A major",  "A Bm C#m D E F#m G#dim" },
        { "E major",  "E F#m G#m A B C#m D#dim" },
        { "B major",  "B C#m D#m E F# G#m A#dim" },
        { "F# major", "F# G#m A#m B C# D#m E#dim" },
        { "C# major", "C# D#m E#m F# G# A#m B#dim" },
        { "F major",  "F Gm Am Bb C Dm Edim" },
        { "Bb major", "Bb Cm Dm Eb F Gm Adim" },
        { "Eb major", "Eb Fm Gm Ab Bb Cm Ddim" },
        { "Ab major", "Ab Bbm Cm Db Eb Fm Gdim" },
        { "Db major", "Db Ebm Fm Gb Ab Bbm Cdim" },
        { "Gb major", "Gb Abm Bbm Cb Db Ebm Fdim" },
        { "Cb major", "Cb Dbm Ebm Fb Gb Abm Bbdim" } };
    for (const auto& [key, chords] : majors)
    {
        INFO (key);
        CHECK (chordsOf (keyNamed (key)) == chords);
        CHECK (numeralsOf (keyNamed (key)) == juce::String (juce::CharPointer_UTF8 ("I ii iii IV V vi vii\xc2\xb0")));
    }
    // Every minor key's: i ii° III iv v VI VII, then the harmonic minor's V and vii°.
    const std::pair<const char*, const char*> minors[] = {
        { "A minor",  "Am Bdim C Dm Em F G E G#dim" },
        { "E minor",  "Em F#dim G Am Bm C D B D#dim" },
        { "B minor",  "Bm C#dim D Em F#m G A F# A#dim" },
        { "F# minor", "F#m G#dim A Bm C#m D E C# E#dim" },
        { "C# minor", "C#m D#dim E F#m G#m A B G# B#dim" },
        { "G# minor", "G#m A#dim B C#m D#m E F# D# F##dim" },
        { "D# minor", "D#m E#dim F# G#m A#m B C# A# C##dim" },
        { "A# minor", "A#m B#dim C# D#m E#m F# G# E# G##dim" },
        { "D minor",  "Dm Edim F Gm Am Bb C A C#dim" },
        { "G minor",  "Gm Adim Bb Cm Dm Eb F D F#dim" },
        { "C minor",  "Cm Ddim Eb Fm Gm Ab Bb G Bdim" },
        { "F minor",  "Fm Gdim Ab Bbm Cm Db Eb C Edim" },
        { "Bb minor", "Bbm Cdim Db Ebm Fm Gb Ab F Adim" },
        { "Eb minor", "Ebm Fdim Gb Abm Bbm Cb Db Bb Ddim" },
        { "Ab minor", "Abm Bbdim Cb Dbm Ebm Fb Gb Eb Gdim" } };
    for (const auto& [key, chords] : minors)
    {
        INFO (key);
        CHECK (chordsOf (keyNamed (key)) == chords);
        CHECK (numeralsOf (keyNamed (key)) == juce::String (juce::CharPointer_UTF8 ("i ii\xc2\xb0 III iv v VI VII V vii\xc2\xb0")));
    }
    // The notes inside: spelled from the chord's root, letter by letter.
    const auto ebm = keyNamed ("Eb minor");
    CHECK (Chord { 11, Chord::Type::major, Chord::Tension::none, -1 }.noteNames (ebm).joinIntoString (" ") == "Cb Eb Gb");   // VI
    CHECK (Chord { 10, Chord::Type::major, Chord::Tension::seven, -1 }.noteNames (ebm).joinIntoString (" ") == "Bb D F Ab"); // V7
    CHECK (Chord { 2, Chord::Type::diminished, Chord::Tension::dim7, -1 }.noteNames (ebm).joinIntoString (" ") == "D F Ab Cb"); // vii°7
    CHECK (Chord { 6, Chord::Type::major, Chord::Tension::none, -1 }.noteNames (keyNamed ("F# major")).joinIntoString (" ") == "F# A# C#");
    CHECK (Chord { 5, Chord::Type::diminished, Chord::Tension::none, -1 }.noteNames (keyNamed ("F# major")).joinIntoString (" ") == "E# G# B");
    CHECK (Chord { 3, Chord::Type::minor, Chord::Tension::none, 6 }.name (ebm) == "Ebm/Gb");
    // Outside the key: flats in flat keys, raised degrees where a key raises them.
    const auto c = keyNamed ("C major");
    CHECK (Chord { 10, Chord::Type::major, Chord::Tension::none, -1 }.romanName (c) == "bVII");
    CHECK (Chord { 8, Chord::Type::major, Chord::Tension::none, -1 }.name (c) == "Ab");
    CHECK (Chord { 6, Chord::Type::diminished, Chord::Tension::none, -1 }.romanName (c) == juce::String (juce::CharPointer_UTF8 ("#iv\xc2\xb0")));
    CHECK (Chord { 2, Chord::Type::major, Chord::Tension::none, -1 }.romanName (c) == "II");
    // Nashville: from the tonic's major scale.
    CHECK (Chord { 6, Chord::Type::major, Chord::Tension::none, -1 }.nashvilleName (ebm) == "b3");
    CHECK (Chord { 3, Chord::Type::minor, Chord::Tension::none, -1 }.nashvilleName (ebm) == "1m");
}

TEST_CASE ("A key on the pads: its chords, spelled; a minor key's v and major V side by side; saved (0.15.0)", "[chords]")
{
    ChordPads pads;
    pads.fillFromKey (Key { 2, -1, true });   // Eb minor
    juce::StringArray names;
    for (const auto& p : pads.pads)
        names.add (p.chord ? pads.displayName (*p.chord, ChordPads::Display::noteName) : "-");
    // White keys: i ii° III iv v VI VII; black: i7, IIImaj7, iv7, the major V (beside v), V7.
    CHECK (names.joinIntoString (" ") == "Ebm Ebm7 Fdim Gbmaj7 Gb Abm Abm7 Bbm Bb Cb Bb7 Db");
    CHECK (pads.displayName (*pads.pads[7].chord, ChordPads::Display::roman) == "v");
    CHECK (pads.displayName (*pads.pads[8].chord, ChordPads::Display::roman) == "V");
    // Saved and read back: the key as spelled (Eb, not D#).
    const auto back = ChordPads::fromXml (*pads.toXml());
    CHECK (back.key().name() == "Eb minor");
    CHECK (back == pads);
    // A song from 0.14 (no key letter): its root and flats give the key.
    ChordPads old;
    old.rootKey = 3;
    old.flats = true;
    CHECK (old.key().name() == "Eb major");
}

TEST_CASE ("Voicings by name: inversions, open, spread; each exactly the chord's notes (0.15.0)", "[chords]")
{
    for (const auto& key : Key::all())
        for (const auto& chord : keyChords (key))
        {
            const auto want = chord.pitchClasses();
            const auto voicings = namedVoicings (chord, 48);
            REQUIRE (voicings.size() >= 3);
            for (const auto& v : voicings)
            {
                std::set<int> classes;
                for (const auto n : v.notes)
                    classes.insert (((n % 12) + 12) % 12);
                INFO (chord.name (key) << " " << v.kind);
                CHECK (classes == std::set<int> (want.begin(), want.end()));   // nothing added, nothing missing
                CHECK (std::is_sorted (v.notes.begin(), v.notes.end()));
            }
        }
    const auto c = namedVoicings ({ 0, Chord::Type::major, Chord::Tension::none, -1 }, 48);
    juce::StringArray kinds;
    for (const auto& v : c)
        kinds.addIfNotAlreadyThere (v.kind);
    CHECK (kinds.contains ("Root position"));
    CHECK (kinds.contains ("1st inversion"));
    CHECK (kinds.contains ("2nd inversion"));
    CHECK (kinds.contains ("Open (drop 2)"));
    CHECK (kinds.contains ("Spread (root, then a tenth)"));
    // Root position: C E G, the root lowest.
    for (const auto& v : c)
        if (v.kind == "Root position")
            CHECK (v.notes.front() % 12 == 0);
}

TEST_CASE ("Suggestions: scored by function, the circle and common notes; V to I strongest (0.15.0)", "[chords]")
{
    const auto c = Key { 0, 0, false };
    const auto g = Chord { 7, Chord::Type::major, Chord::Tension::none, -1 };
    const auto next = suggestNext (g, c);
    REQUIRE (! next.empty());
    CHECK (next.front().chord.name (c) == "C");
    CHECK (next.front().score >= 90);
    CHECK (next.front().reason.contains ("dominant to tonic"));
    // Deceptive V-vi next best among the key's; borrowed chords lower, marked.
    const auto find = [&next, &c] (const juce::String& name) { for (const auto& s : next) if (s.chord.name (c) == name) return s; return Suggestion {}; };
    CHECK (find ("Am").score > find ("Dm").score);
    CHECK (find ("Bb").inKey == false);
    CHECK (find ("Bb").score < find ("C").score);
    // ii goes to V; in a minor key the major V beats the minor v.
    const auto fromII = suggestNext ({ 2, Chord::Type::minor, Chord::Tension::none, -1 }, c);
    CHECK (fromII.front().chord.root == 7);
    const auto am = Key { 5, 0, true };
    const auto fromIv = suggestNext ({ 2, Chord::Type::minor, Chord::Tension::none, -1 }, am);
    int majorV = 0, minorV = 0;
    for (const auto& s : fromIv)
    {
        if (s.chord.root == 4 && s.chord.type == Chord::Type::major && s.chord.tension == Chord::Tension::none) majorV = s.score;
        if (s.chord.root == 4 && s.chord.type == Chord::Type::minor) minorV = s.score;
    }
    CHECK (majorV > minorV);
    CHECK (std::is_sorted (next.begin(), next.end(), [] (const Suggestion& a, const Suggestion& b) { return a.score > b.score; }));
}

TEST_CASE ("The KEY menu's choice fills the pads; the circle suggests what comes next; chords from it onto pads; the pads on one screen (0.15.0)", "[chords][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    view.openChordPads();
    auto& pads = view.getChordPads();

    // Eb minor: its chords, spelled, on the pads.
    pads.chooseKey (Key { 2, -1, true });
    CHECK (studio.getChordPads().key().name() == "Eb minor");
    CHECK (studio.getChordPads().displayName (*studio.getChordPads().pads[9].chord, ChordPads::Display::noteName) == "Cb");
    // Transposing or renaming instead (the menu's choice): A major then takes the pads up, or only names them.
    studio.setPadsKey (Key { 5, 0, false }, Studio::PadsKeyChange::renameOnly);
    CHECK (studio.getChordPads().pads[0].chord->root == 3);   // (Ebm still there)
    CHECK (studio.getChordPads().key().name() == "A major");

    // The circle: a chord clicked plays and leads the suggestions.
    pads.chooseKey (Key { 0, 0, false });
    pads.setCircleShown (true);
    CHECK (pads.isCircleShown());
    CHECK_FALSE (pads.isEditorShown());
    pads.circleChordClicked ({ 7, Chord::Type::major, Chord::Tension::none, -1 });
    REQUIRE (pads.getSuggestionsFrom().has_value());
    CHECK (pads.getSuggestionsFrom()->root == 7);
    REQUIRE (! pads.getSuggestions().empty());
    CHECK (pads.getSuggestions().front().chord.root == 0);   // G: then C

    // Double-click a suggestion: onto the selected pad (else the first one without a chord).
    pads.selectPads ({ 3 });
    CHECK (pads.assignToNextPad ({ 9, Chord::Type::minor, Chord::Tension::seven, -1 }) == 3);
    CHECK (studio.getChordPads().pads[3].chord->name() == "Am7");
    studio.removeChords ({ 1 });
    pads.selectPads ({});
    CHECK (pads.assignToNextPad ({ 2, Chord::Type::minor, Chord::Tension::none, -1 }) == 1);
    // Dragged from the circle onto a pad.
    juce::DragAndDropTarget::SourceDetails drop { juce::var (juce::String ("chord:") + Chord { 5, Chord::Type::major, Chord::Tension::maj7, -1 }.toText()),
                                                  &pads, pads.getPadBounds (6).getCentre().toInt() };
    CHECK (pads.isInterestedInDragSource (drop));
    pads.itemDropped (drop);
    CHECK (studio.getChordPads().pads[6].chord->name() == "Fmaj7");

    // A voicing chosen by name: that one, adaptive off.
    const auto named = namedVoicings (*studio.getChordPads().pads[0].chord, 48);
    for (int i = 0; i < (int) named.size(); ++i)
        if (named[(size_t) i].kind == "Spread (root, then a tenth)")
        {
            studio.setPadVoicing (0, i);
            CHECK (studio.getPadNotes (0) == named[(size_t) i].notes);
            CHECK_FALSE (studio.getChordPads().pads[0].adaptive);
        }

    // Wider than a screen (spanning three): the pads' controls on one screen's width, centred.
    pads.setSize (5760, 320);
    CHECK (pads.getContentArea().getWidth() <= 1920);
    CHECK (std::abs (pads.getContentArea().getCentreX() - 2880) <= 1);
    CHECK (pads.getPadBounds (0).getX() >= (float) pads.getContentArea().getX());

    // 0.17.0: the screens either side used: every key on the left, the circle always on the right.
    pads.setCircleShown (false);
    pads.setEditorShown (true);
    REQUIRE (pads.isKeyListShown());
    CHECK (pads.isCircleOnSide());
    CHECK (pads.getCircle()->isVisible());
    CHECK (pads.getKeyList()->getRight() <= pads.getContentArea().getX());
    CHECK (pads.getCircle()->getX() >= pads.getContentArea().getRight());
    CHECK (pads.isEditorShown());   // (the editor in the middle meanwhile)
    // A key clicked there: chosen (filling the pads with its chords, as the KEY menu's choice).
    const auto ab = Key { 5, -1, false };
    const auto cell = pads.keyListCell (ab);
    CHECK (cell.getHeight() >= 30.0f);   // (large: easy to read)
    REQUIRE (pads.keyListKeyAt (cell.getCentre()).has_value());
    CHECK (pads.keyListKeyAt (cell.getCentre())->name() == "Ab major");
    auto* list = pads.getKeyList();
    const auto at = cell.getCentre();
    list->mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                       list, list, juce::Time::getCurrentTime(), at, juce::Time::getCurrentTime(), 1, false));
    CHECK (studio.getChordPads().key().name() == "Ab major");
    CHECK (studio.getChordPads().pads[0].chord->name (studio.getChordPads().key()) == "Ab");
    // Its relative minor sits on the same row (the same notes).
    CHECK (pads.keyListKeyAt (pads.keyListCell (Key { 3, 0, true }).getCentre())->name() == "F minor");
    CHECK (std::abs (pads.keyListCell (Key { 3, 0, true }).getY() - cell.getY()) < 0.5f);
    // The key's popular progressions beside the suggestions (0.17.2): there's room on the right screen.
    CHECK (pads.isPopularShown());
    REQUIRE (pads.getProgressions().size() == 10);
    CHECK (pads.getProgressions()[0].chords.front().name (studio.getChordPads().key()) == "Ab");   // (Ab major's I-V-vi-IV)
    pads.playProgression (0);   // (a chord at once; the timer the rest)
    CHECK (pads.getPlayingProgression() == 0);
    // One screen only: neither side.
    pads.setSize (1500, 320);
    CHECK_FALSE (pads.isKeyListShown());
    CHECK_FALSE (pads.isCircleOnSide());
}

TEST_CASE ("Suggestions after every chord in every key, and from outside it (0.16.1: 0.16.0 crashed on Windows)", "[chords]")
{
    // A secondary dominant's check read past its scale's end (two lists, begin of
    // one and end of another): every key, every root and type, plain and seventh.
    for (const auto& key : Key::all())
        for (int root = 0; root < 12; ++root)
            for (const auto type : { Chord::Type::major, Chord::Type::minor, Chord::Type::diminished, Chord::Type::augmented,
                                     Chord::Type::sus2, Chord::Type::sus4 })
                for (const auto tension : { Chord::Tension::none, Chord::Tension::seven })
                {
                    const auto next = suggestNext ({ root, type, tension, -1 }, key);
                    REQUIRE_FALSE (next.empty());
                    for (const auto& s : next)
                    {
                        REQUIRE (s.score >= 1);
                        REQUIRE (s.score <= 99);
                        REQUIRE (s.reason.isNotEmpty());
                    }
                }
    // The secondary dominant's reason: in C, E (V of vi) pulls to vi.
    const auto fromC = suggestNext ({ 0, Chord::Type::major, Chord::Tension::none, -1 }, Key { 0, 0, false });
    auto found = false;
    for (const auto& s : fromC)
        if (s.chord.root == 4 && s.chord.type == Chord::Type::major)
        {
            found = true;
            INFO (s.reason.toStdString());
            CHECK (s.reason.contains ("secondary dominant: pulls to vi"));
        }
    CHECK (found);
}

TEST_CASE ("The ten most popular progressions in a key; onto the chord track and the pads (0.17.2)", "[chords]")
{
    const auto spelled = [] (const Progression& p, const Key& key)
    {
        juce::StringArray n;
        for (const auto& c : p.chords)
            n.add (c.name (key));
        return n.joinIntoString (" ");
    };
    const Key c { 0, 0, false }, am { 5, 0, true }, ebm { 2, -1, true };
    const auto major = popularProgressions (c);
    REQUIRE (major.size() == 10);
    CHECK (spelled (major[0], c) == "C G Am F");          // I-V-vi-IV
    CHECK (spelled (major[1], c) == "C Am F G");          // I-vi-IV-V
    CHECK (spelled (major[4], c) == "Dm G7 C");           // ii-V7-I
    CHECK (spelled (major[6], c) == "C G Am Em F C F G"); // the Canon
    CHECK (spelled (major[7], c) == "C Bb F C");          // I-bVII-IV-I
    const auto minor = popularProgressions (am);
    REQUIRE (minor.size() == 10);
    CHECK (spelled (minor[0], am) == "Am F C G");         // i-VI-III-VII
    CHECK (spelled (minor[3], am) == "Am Dm E Am");       // i-iv-V-i
    CHECK (spelled (minor[4], am) == "Am G F E");         // the Andalusian
    CHECK (spelled (minor[6], am) == "Bdim E7 Am");       // ii°-V7-i
    // Spelled in the key: E-flat minor's.
    CHECK (spelled (popularProgressions (ebm)[0], ebm) == "Ebm Cb Gb Db");
    for (const auto& key : Key::all())
        for (const auto& p : popularProgressions (key))
        {
            REQUIRE (p.chords.size() >= 2);
            CHECK (p.name.isNotEmpty());
            // Every one has the key's own tonic chord in it (I in major, i in minor).
            CHECK (std::any_of (p.chords.begin(), p.chords.end(), [&key] (const Chord& ch)
                   { return ch.root == key.pitchClass() && (ch.type == Chord::Type::minor) == key.minor; }));
        }

    // Onto the chord track: a chord a bar from the bar the beat is in, what was there replaced; one undo step.
    Studio studio (false);
    studio.addChordEvent (4.0, { 2, Chord::Type::minor, Chord::Tension::none, -1 });
    studio.addChordEvent (20.0, { 9, Chord::Type::minor, Chord::Tension::none, -1 });
    studio.removeChordTrack();
    studio.addProgressionToChordTrack (major[0].chords, 5.5);   // (bar 2)
    const auto& track = studio.getChordTrack();
    CHECK (track.shown);
    REQUIRE (track.events.size() == 5);
    CHECK (track.events[0].beat == 4.0);
    CHECK (track.events[0].chord.name (c) == "C");
    CHECK (track.events[3].beat == 16.0);
    CHECK (track.events[3].chord.name (c) == "F");
    CHECK (track.events[4].beat == 20.0);   // (after the four bars: kept)
    studio.undo();
    CHECK (studio.getChordTrack().events.size() == 2);
    CHECK (studio.getChordTrack().events[0].chord.name (c) == "Dm");

    // Onto the pads from a pad on; a locked pad skipped.
    studio.setPadsLocked ({ 2 }, true);
    CHECK (studio.assignProgressionToPads (minor[4].chords, 1) == 4);
    const auto& pads = studio.getChordPads().pads;
    CHECK (pads[1].chord->name (am) == "Am");
    CHECK (pads[3].chord->name (am) == "G");
    CHECK (pads[5].chord->name (am) == "E");
}
