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
    for (const auto& v : voicings)
    {
        CHECK (classesOf (v) == std::set<int> { 0, 4, 7 });
        CHECK (v.front() >= 48);
        CHECK (v.front() < 66);
        CHECK (std::is_sorted (v.begin(), v.end()));
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
