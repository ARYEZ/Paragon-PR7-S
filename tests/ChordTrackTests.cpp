#include <catch2/catch_test_macros.hpp>

#include "engine/Studio.h"

using namespace pr7s;

namespace
{
    using T = Chord::Type;
    using X = Chord::Tension;
    juce::String names (const std::vector<ChordEvent>& events, const Key& key)
    {
        juce::StringArray n;
        for (const auto& e : events)
            n.add (juce::String (e.beat) + ":" + e.chord.name (key));
        return n.joinIntoString (" ");
    }
}

TEST_CASE ("The chord track: chords added, moved, removed; undone; saved; scales from the chords (0.16.0)", "[chordtrack]")
{
    Studio studio (false);
    CHECK_FALSE (studio.getChordTrack().shown);
    studio.addChordTrack();
    CHECK (studio.getChordTrack().shown);
    const Key c { 0, 0, false };
    studio.addChordEvent (0.0, { 0, T::major, X::none, -1 });
    studio.addChordEvent (8.0, { 5, T::major, X::none, -1 });
    studio.addChordEvent (4.0, { 9, T::minor, X::none, -1 });
    studio.addChordEvent (12.0, { 7, T::major, X::seven, -1 });
    CHECK (names (studio.getChordTrack().events, c) == "0:C 4:Am 8:F 12:G7");
    CHECK (studio.getChordTrack().indexAt (5.0) == 1);
    CHECK (studio.getChordTrack().endOf (1, 4.0) == 8.0);
    CHECK (studio.getChordTrack().endOf (3, 4.0) == 16.0);
    // The scale: C major, from the chords.
    REQUIRE (studio.getChordTrack().scales.size() == 1);
    CHECK (studio.getChordTrack().scales[0].key.name() == "C major");
    // The same beat: replaced.
    studio.addChordEvent (4.0, { 2, T::minor, X::seven, -1 });
    CHECK (names (studio.getChordTrack().events, c) == "0:C 4:Dm7 8:F 12:G7");
    studio.undo();
    CHECK (names (studio.getChordTrack().events, c) == "0:C 4:Am 8:F 12:G7");
    studio.removeChordEvents ({ 1 });
    CHECK (names (studio.getChordTrack().events, c) == "0:C 8:F 12:G7");
    studio.undo();
    // The next chord suggested: after G7, C.
    CHECK (studio.suggestChordAt (16.0).name (c) == "C");

    // A key change: Eb major chords from bar 5 on give a scale event there.
    studio.addChordEvent (16.0, { 3, T::major, X::none, -1 });
    studio.addChordEvent (20.0, { 8, T::major, X::none, -1 });
    studio.addChordEvent (24.0, { 10, T::major, X::seven, -1 });
    studio.addChordEvent (28.0, { 3, T::major, X::none, -1 });
    const auto& scales = studio.getChordTrack().scales;
    REQUIRE (scales.size() == 2);
    CHECK (scales[1].beat == 16.0);
    CHECK (scales[1].key.name() == "Eb major");
    CHECK (studio.chordTrackKeyAt (21.0).name() == "Eb major");
    // Set by hand: they stay (no longer following the chords).
    studio.setScaleEvents ({ { 0.0, Key { 5, 0, true } } });
    CHECK_FALSE (studio.getChordTrack().autoScales);
    studio.addChordEvent (32.0, { 0, T::major, X::none, -1 });
    CHECK (studio.getChordTrack().scales.size() == 1);
    studio.setAutoScales (true);
    CHECK (studio.getChordTrack().scales.size() >= 2);

    // Saved and opened again.
    const auto file = juce::File::createTempFile (Project::fileExtension);
    REQUIRE (studio.save (file));
    const auto saved = studio.getChordTrack();
    studio.newProject();
    CHECK (studio.getChordTrack().events.empty());
    juce::String problems;
    REQUIRE (studio.open (file, problems));
    CHECK (studio.getChordTrack() == saved);
    studio.getLibrary().removeRecent (file);
    file.deleteFile();
}

TEST_CASE ("Chords to MIDI: a part of them, voiced with the least movement, each until the next (0.16.0)", "[chordtrack]")
{
    Studio studio (false);
    studio.addChordEvent (4.0, { 0, T::major, X::none, -1 });
    studio.addChordEvent (8.0, { 5, T::major, X::none, -1 });
    studio.addChordEvent (12.0, { 7, T::major, X::none, -1 });
    const auto track = studio.getProject().tracks[0].id;
    const auto id = studio.chordsToMidi (track);
    REQUIRE (id != 0);
    const auto at = studio.findPart (id);
    REQUIRE (at.has_value());
    REQUIRE (at->first == track);
    const auto& part = studio.getProject().findTrack (track)->parts[(size_t) at->second];
    CHECK (part.start == 4.0);
    CHECK (part.length == 12.0);
    CHECK (part.notes.size() == 9);
    // Each chord's notes exactly, at its place, until the next.
    std::map<double, std::set<int>> byStart;
    for (const auto& n : part.notes)
    {
        byStart[n.start].insert (n.pitch % 12);
        CHECK (n.length == 4.0);
    }
    CHECK ((byStart[0.0] == std::set<int> { 0, 4, 7 }));
    CHECK ((byStart[4.0] == std::set<int> { 5, 9, 0 }));
    CHECK ((byStart[8.0] == std::set<int> { 7, 11, 2 }));
    // The voices move little: no note leaps more than a fourth to the next chord's nearest.
    // Onto an audio track: a new instrument track instead.
    const auto audio = studio.addAudioTrack (false);
    const auto count = studio.getProject().tracks.size();
    CHECK (studio.chordsToMidi (audio, { 0 }) != 0);
    CHECK (studio.getProject().tracks.size() == count + 1);
}

TEST_CASE ("MIDI to chords: the chords heard in a part, beat by beat; inversions; arpeggios (0.16.0)", "[chordtrack]")
{
    Studio studio (false);
    const auto track = studio.getProject().tracks[0].id;
    const Key c { 0, 0, false };
    // C | Am | F | G7, one bar each, with a passing note (a quiet D over C).
    Part part { 0.0, 16.0, "Piano", {} };
    const auto chord = [&part] (double at, std::initializer_list<int> pitches)
    {
        for (const auto p : pitches)
            part.notes.push_back ({ at, 4.0, p, 0.8f });
    };
    chord (0.0, { 48, 52, 55, 60 });
    part.notes.push_back ({ 1.0, 0.5, 62, 0.3f });
    chord (4.0, { 45, 52, 57, 60 });
    chord (8.0, { 41, 53, 57, 60 });
    chord (12.0, { 43, 53, 59, 62 });   // G7 (G F B D)
    studio.addPart (track, part);
    const auto id = studio.getProject().findTrack (track)->parts[0].id;
    CHECK (studio.chordsFromParts ({ id }) == 4);
    CHECK (names (studio.getChordTrack().events, c) == "0:C 4:Am 8:F 12:G7");
    CHECK (studio.getChordTrack().shown);

    // An inversion: C with E in the bass.
    Studio second (false);
    const auto t2 = second.getProject().tracks[0].id;
    second.addPart (t2, { 0.0, 4.0, "Inv", { { 0.0, 4.0, 52, 0.8f }, { 0.0, 4.0, 55, 0.8f }, { 0.0, 4.0, 60, 0.8f } } });
    second.chordsFromParts ({ second.getProject().findTrack (t2)->parts[0].id });
    REQUIRE (second.getChordTrack().events.size() == 1);
    CHECK (second.getChordTrack().events[0].chord.name (c) == "C/E");

    // An arpeggio: Am's notes one after another over the bar.
    Studio third (false);
    const auto t3 = third.getProject().tracks[0].id;
    Part arp { 0.0, 4.0, "Arp", {} };
    int pitches[] = { 57, 60, 64, 69, 64, 60, 57, 60 };
    for (int i = 0; i < 8; ++i)
        arp.notes.push_back ({ i * 0.5, 0.5, pitches[i], 0.8f });
    third.addPart (t3, arp);
    third.chordsFromParts ({ third.getProject().findTrack (t3)->parts[0].id }, 4.0);
    REQUIRE (third.getChordTrack().events.size() == 1);
    CHECK (third.getChordTrack().events[0].chord.name (c) == "Am");
}

TEST_CASE ("Assign from Chord Track: its chords, each once, in order onto the pads (0.16.0)", "[chordtrack]")
{
    Studio studio (false);
    studio.addChordEvent (0.0, { 9, T::minor, X::none, -1 });
    studio.addChordEvent (4.0, { 5, T::major, X::none, -1 });
    studio.addChordEvent (8.0, { 9, T::minor, X::none, -1 });
    studio.addChordEvent (12.0, { 4, T::major, X::none, -1 });
    studio.setPadsLocked ({ 1 }, true);
    studio.assignPadsFromChordTrack();
    const auto& pads = studio.getChordPads();
    const auto key = pads.key();
    CHECK (pads.pads[0].chord->name (key) == "Am");
    CHECK (pads.pads[1].locked);                         // (kept)
    CHECK (pads.pads[2].chord->name (key) == "F");
    CHECK (pads.pads[3].chord->name (key) == "E");
    CHECK_FALSE (pads.pads[4].chord.has_value());
    CHECK (key.name() == "A minor");                     // the chord track's scale
}

#include "ui/ChordEventEditor.h"
#include "ui/Finish.h"
#include "ui/MainView.h"

namespace
{
    juce::MouseEvent mouse (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt, juce::ModifierKeys mods = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), int clicks = 1)
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, mods, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, now, downAt, now, clicks, at != downAt);
    }
}

TEST_CASE ("The chord track on screen: its row, a double-click adds the chord suggested, pads dropped on it, moved by a drag, Delete (0.16.0)", "[chordtrack][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    studio.addChordTrack();
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto& timeline = view.getTimeline();
    timeline.firstBeat = 0.0;
    const auto area = arrange.getChordArea();
    REQUIRE (! area.isEmpty());

    // A double-click on the empty lane: the key's I (nothing before it); then, after it, what it suggests.
    const juce::Point<float> at1 (timeline.xForBeat (0.0) + 1.0f, area.getCentreY());
    arrange.mouseDoubleClick (mouse (arrange, at1, at1, {}, 2));
    REQUIRE (studio.getChordTrack().events.size() == 1);
    const Key c { 0, 0, false };
    CHECK (studio.getChordTrack().events[0].chord.name (c) == "C");
    const juce::Point<float> at2 (timeline.xForBeat (4.0) + 2.0f, area.getCentreY());
    arrange.mouseDoubleClick (mouse (arrange, at2, at2, {}, 2));
    REQUIRE (studio.getChordTrack().events.size() == 2);
    CHECK (studio.getChordTrack().events[1].chord.name (c) == "F");   // (after C: I to IV, down a fifth, a note in common)

    // A pad (G, on the white key G) dropped on the lane at bar 3.
    juce::DragAndDropTarget::SourceDetails pad { juce::var ("chordpad:7"), &arrange,
                                                 juce::Point<int> ((int) timeline.xForBeat (9.0), (int) area.getCentreY()) };
    CHECK (arrange.isInterestedInDragSource (pad));
    arrange.itemDropped (pad);
    REQUIRE (studio.getChordTrack().events.size() == 3);
    CHECK (studio.getChordTrack().events[2].beat == 8.0);
    CHECK (studio.getChordTrack().events[2].chord.name (c) == "G");
    // A chord from the circle dropped at bar 4.
    juce::DragAndDropTarget::SourceDetails circle { juce::var (juce::String ("chord:") + Chord { 9, T::minor, X::none, -1 }.toText()), &arrange,
                                                    juce::Point<int> ((int) timeline.xForBeat (12.5), (int) area.getCentreY()) };
    arrange.itemDropped (circle);
    REQUIRE (studio.getChordTrack().events.size() == 4);
    CHECK (studio.getChordTrack().events[3].chord.name (c) == "Am");

    // Dragged: Am from bar 4 to bar 5 (snapped).
    const auto from = arrange.getChordEventBounds (3).getCentre();
    arrange.mouseDown (mouse (arrange, from, from));
    CHECK ((arrange.getChordSelection() == std::vector<int> { 3 }));
    const auto to = from.translated (timeline.xForBeat (16.0) - timeline.xForBeat (12.0), 0.0f);
    arrange.mouseDrag (mouse (arrange, to, from));
    arrange.mouseUp (mouse (arrange, to, from));
    CHECK (studio.getChordTrack().events[3].beat == 16.0);
    studio.undo();
    CHECK (studio.getChordTrack().events[3].beat == 12.0);

    // Delete: the selected chord goes.
    const auto g = arrange.getChordEventBounds (2).getCentre();
    arrange.mouseDown (mouse (arrange, g, g));
    arrange.mouseUp (mouse (arrange, g, g));
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    CHECK (studio.getChordTrack().events.size() == 3);
}

TEST_CASE ("The chord editor box: a root, a type, a tension, a bass; a suggestion after the chord before (0.16.0)", "[chordtrack][ui]")
{
    const Key c { 0, 0, false };
    ChordEventEditor editor ({ 0, T::major, X::none, -1 }, c, Chord { 7, T::major, X::seven, -1 });
    std::vector<Chord> changes;
    editor.onChange = [&changes] (const Chord& chord) { changes.push_back (chord); };
    editor.choose ({ 2, T::minor, X::seven, 0 });
    REQUIRE (changes.size() == 1);
    CHECK (changes[0].name (c) == "Dm7/C");
    CHECK (editor.getChord().name (c) == "Dm7/C");
}
