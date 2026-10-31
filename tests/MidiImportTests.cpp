#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/MidiImport.h"
#include "engine/Studio.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    constexpr int ppq = 480;

    void addNote (juce::MidiMessageSequence& seq, int channel, int pitch, double beat, double length, juce::uint8 velocity)
    {
        seq.addEvent (juce::MidiMessage::noteOn (channel, pitch, velocity), beat * ppq);
        seq.addEvent (juce::MidiMessage::noteOff (channel, pitch), (beat + length) * ppq);
    }

    // A MIDI file as it'd be on disk, from these tracks, read back.
    std::optional<MidiImport> roundTrip (const juce::MidiFile& file, int type, juce::String& error)
    {
        juce::MemoryOutputStream out;
        REQUIRE (file.writeTo (out, type));
        juce::MemoryInputStream in (out.getData(), out.getDataSize(), false);
        return readMidiFile (in, error);
    }

    // A type-1 file: a tempo track (100 BPM, 3/4), a piano and a bass.
    juce::MidiFile song()
    {
        juce::MidiFile file;
        file.setTicksPerQuarterNote (ppq);
        juce::MidiMessageSequence conductor, piano, bass;
        conductor.addEvent (juce::MidiMessage::tempoMetaEvent (600000), 0.0);   // 100 BPM
        conductor.addEvent (juce::MidiMessage::timeSignatureMetaEvent (3, 4), 0.0);
        piano.addEvent (juce::MidiMessage::textMetaEvent (3, "Piano"), 0.0);
        addNote (piano, 1, 60, 0.0, 1.0, 100);
        addNote (piano, 1, 64, 2.5, 0.5, 64);
        bass.addEvent (juce::MidiMessage::textMetaEvent (3, "Bass"), 0.0);
        addNote (bass, 2, 36, 3.0, 2.0, 127);
        for (auto* seq : { &conductor, &piano, &bass })
        {
            seq->updateMatchedPairs();
            file.addTrack (*seq);
        }
        return file;
    }
}

TEST_CASE ("MIDI files: their tracks, names, notes (in beats), first tempo and time signature; a type-0 file split by channel", "[midi]")
{
    juce::String error;
    const auto read = roundTrip (song(), 1, error);
    REQUIRE (read.has_value());
    CHECK_THAT (read->bpm, WithinAbs (100.0, 1.0e-6));
    CHECK (read->numerator == 3);
    CHECK (read->denominator == 4);
    REQUIRE (read->tracks.size() == 2);   // the tempo track has no notes: no track for it
    const auto& piano = read->tracks[0];
    CHECK (piano.name == "Piano");
    REQUIRE (piano.notes.size() == 2);
    CHECK_THAT (piano.notes[1].start, WithinAbs (2.5, 1.0e-9));
    CHECK_THAT (piano.notes[1].length, WithinAbs (0.5, 1.0e-9));
    CHECK (piano.notes[1].pitch == 64);
    CHECK_THAT (piano.notes[1].velocity, WithinAbs (64.0 / 127.0, 1.0e-3));
    CHECK (read->tracks[1].name == "Bass");
    CHECK (read->tracks[1].channel == 2);

    // Type 0: everything in one track; a track per channel.
    juce::MidiFile single;
    single.setTicksPerQuarterNote (ppq);
    juce::MidiMessageSequence all;
    addNote (all, 1, 60, 0.0, 1.0, 100);
    addNote (all, 10, 36, 0.0, 0.25, 110);
    addNote (all, 10, 38, 1.0, 0.25, 110);
    all.updateMatchedPairs();
    single.addTrack (all);
    const auto dissolved = roundTrip (single, 0, error);
    REQUIRE (dissolved.has_value());
    REQUIRE (dissolved->tracks.size() == 2);
    CHECK (dissolved->tracks[0].name == "MIDI 01 (Ch. 1)");
    CHECK (dissolved->tracks[1].name == "MIDI 02 (Ch. 10)");
    CHECK (dissolved->tracks[1].notes.size() == 2);

    // Not a MIDI file: said so.
    juce::MemoryInputStream junk ("not a midi file", 15, false);
    CHECK_FALSE (readMidiFile (junk, error).has_value());
    CHECK (error.isNotEmpty());
}

TEST_CASE ("Importing a MIDI file: into a new song (its tempo and time signature) or this one at a bar (this song's tempo)", "[midi][studio]")
{
    juce::String error;
    const auto read = roundTrip (song(), 1, error);
    REQUIRE (read.has_value());

    // A new song: the file's tempo and signature, a Paragon Synth track for each of its tracks.
    Studio studio (false);
    CHECK (studio.importMidi (*read, true, 0.0) == 2);
    const auto& project = studio.getProject();
    CHECK (project.bpm == 100.0);
    CHECK (project.numerator == 3);
    REQUIRE (project.tracks.size() == 2);   // the new song's empty track made way
    CHECK (project.tracks[0].name == "Piano");
    CHECK (project.tracks[0].instrument.kind == Instrument::Kind::paragonSynth);
    REQUIRE (project.tracks[0].parts.size() == 1);
    CHECK (project.tracks[0].parts[0].start == 0.0);
    CHECK (project.tracks[0].parts[0].length == 3.0);   // a 3/4 bar
    const auto& bass = project.tracks[1].parts[0];
    CHECK (bass.start == 3.0);                          // the bar its note starts in
    CHECK (bass.length == 3.0);
    CHECK (bass.notes[0].start == 0.0);
    CHECK_FALSE (studio.canUndo());                     // the import is where the new song starts

    // Into a song at 120 BPM, 4/4, at bar 3: its tempo kept, the notes from bar 3.
    Studio other (false);
    other.setTempo (120.0);
    CHECK (other.importMidi (*read, false, 8.0) == 2);
    CHECK (other.getProject().bpm == 120.0);
    CHECK (other.getProject().numerator == 4);
    REQUIRE (other.getProject().tracks.size() == 3);    // its own track, and the two
    const auto& piano = other.getProject().tracks[1].parts[0];
    CHECK (piano.start == 8.0);
    CHECK_THAT (piano.notes[1].start, WithinAbs (2.5, 1.0e-9));
}

#include "ui/Finish.h"
#include "ui/MainView.h"

TEST_CASE ("Importing a MIDI file on screen: IMPORT in the header; a .mid dropped on the arrangement goes in at that bar", "[midi][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    std::vector<juce::TextButton*> buttons;
    std::function<void (juce::Component&)> find = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (auto* b = dynamic_cast<juce::TextButton*> (child))
                buttons.push_back (b);
            find (*child);
        }
    };
    find (view);
    CHECK (std::any_of (buttons.begin(), buttons.end(), [] (auto* b) { return b->getButtonText() == "IMPORT" && b->isVisible(); }));

    // The file on disk, dropped over bar 3 of the arrangement (4/4: beat 8).
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-import-test.mid");
    {
        file.deleteFile();
        juce::FileOutputStream out (file);
        REQUIRE (out.openedOk());
        REQUIRE (song().writeTo (out, 1));
    }
    CHECK (view.isInterestedInFileDrag ({ file.getFullPathName() }));
    CHECK_FALSE (view.isInterestedInFileDrag ({ "song.wav" }));
    auto& arrange = view.getArrangeView();
    const auto dropAt = view.getLocalPoint (&arrange, juce::Point<int> ((int) (9.0 * Timeline().pixelsPerBeat), 60));
    view.filesDropped ({ file.getFullPathName() }, dropAt.x, dropAt.y);
    const auto& tracks = studio.getProject().tracks;
    REQUIRE (tracks.size() == 3);   // the song's own, and the file's two
    CHECK (tracks[1].name == "Piano");
    CHECK (tracks[1].parts[0].start == 8.0);   // the bar it was dropped on
    CHECK (studio.getProject().bpm == 120.0);  // this song's tempo kept
    file.deleteFile();
}
