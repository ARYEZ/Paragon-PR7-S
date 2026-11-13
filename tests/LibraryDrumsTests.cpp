// (0.106.0, the owner: "I want the composer to reach for my library grooves by default. Those are much better grooves
// than what we are currently generating") The composer's drums lane from the MediaBay's drum MIDI.

#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/Composer.h"
#include "engine/DrumIdeas.h"
#include "engine/SectionTypes.h"

#include <set>

using namespace pr7s;

namespace
{
    // A one-bar (or one-beat) drum file: each hit (beat, pitch) a sixteenth long, 480 ticks a quarter.
    juce::File writeGroove (const juce::File& file, const std::vector<std::pair<double, int>>& hits)
    {
        juce::MidiMessageSequence track;
        for (const auto& [beat, pitch] : hits)
        {
            track.addEvent (juce::MidiMessage::noteOn (10, pitch, (juce::uint8) 100), beat * 480.0);
            track.addEvent (juce::MidiMessage::noteOff (10, pitch), beat * 480.0 + 120.0);
        }
        track.updateMatchedPairs();
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (480);
        midi.addTrack (track);
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::FileOutputStream out (file);
        midi.writeTo (out);
        return file;
    }

    struct Library
    {
        juce::TemporaryFile folder;
        juce::File root;
        std::vector<LibraryGroove> grooves;
        Library()
        {
            root = folder.getFile();
            root.createDirectory();
            // A verse groove (kick, snare, hats), a chorus groove (ride 51, crash 57 on 1), a fill (toms on beat 4).
            std::vector<std::pair<double, int>> verse, chorus;
            for (int i = 0; i < 8; ++i)
                verse.push_back ({ i * 0.5, 42 });
            verse.push_back ({ 0.0, 36 });
            verse.push_back ({ 1.0, 38 });
            verse.push_back ({ 2.0, 36 });
            verse.push_back ({ 3.0, 38 });
            for (int i = 0; i < 4; ++i)
                chorus.push_back ({ (double) i, 51 });
            chorus.push_back ({ 0.0, 57 });
            chorus.push_back ({ 0.0, 36 });
            chorus.push_back ({ 2.0, 38 });
            const auto v = writeGroove (root.getChildFile ("Metal/Verse/Verse Groove 01.mid"), verse);
            const auto v2 = writeGroove (root.getChildFile ("Metal/Verse/Verse Groove 02.mid"), { { 0.0, 36 }, { 1.0, 38 } });
            const auto ch = writeGroove (root.getChildFile ("Metal/Chorus/Chorus Groove 01.mid"), chorus);
            const auto f = writeGroove (root.getChildFile ("Metal/Fills/Fill 01.mid"), { { 3.0, 48 }, { 3.25, 47 }, { 3.5, 45 }, { 3.75, 43 } });
            // (as findLibraryGrooves leaves them: a fill 3 down)
            grooves = { { v, "Verse Groove 01", "Verse", 120.0, 4 }, { v2, "Verse Groove 02", "Verse", 120.0, 4 },
                        { ch, "Chorus Groove 01", "Chorus", 120.0, 4 }, { f, "Fill 01", "Fills", 120.0, 1 } };
        }
    };

    std::vector<ChordSpan> eightBars()
    {
        std::vector<ChordSpan> spans;
        for (int bar = 0; bar < 8; ++bar)
            spans.push_back ({ bar * 4.0, bar * 4.0 + 4.0, Chord { bar % 2 == 0 ? 9 : 5, bar % 2 == 0 ? Chord::Type::minor : Chord::Type::major }, Key {} });
        return spans;
    }

    std::set<int> pitchesIn (const LineIdea& idea, double from, double to)
    {
        std::set<int> out;
        for (const auto& n : idea.notes)
            if (n.beat >= from - 1.0e-6 && n.beat < to - 1.0e-6)
                out.insert (n.pitch);
        return out;
    }
}

TEST_CASE ("0.106.0: the composer's drums from the groove library: by the section, a fill at the phrase ends, on by default", "[composer][library]")
{
    Library library;
    ComposerContext c;
    c.libraryGrooves = library.grooves;
    c.drums.fillEvery = 4;
    const auto spans = eightBars();
    ComposerState s;
    CHECK (s.libraryDrums);   // (on by default)

    // The verse's grooves first for a verse, the chorus's for a chorus; fills apart.
    CHECK (libraryGroovesFor (library.grooves, 1, false).front().name.startsWith ("Verse"));
    CHECK (libraryGroovesFor (library.grooves, 2, false).front().name == "Chorus Groove 01");
    REQUIRE (libraryGroovesFor (library.grooves, 1, true).size() == 1);
    CHECK (libraryGroovesFor (library.grooves, 1, false).size() == 3);

    s.lane (ComposerLayer::drums).style = 1;   // (verse)
    s.lane (ComposerLayer::drums).variation = 0;
    {
        const auto all = composeLayers (s, spans, c);
        const auto& drums = all[(size_t) ComposerLayer::drums];
        CHECK (drums.style == "My library: Verse Groove 01");
        CHECK (pitchesIn (drums, 0.0, 12.0) == std::set<int> { 36, 38, 42 });
        // A fill on bar 4's last beat and on the last bar's; none elsewhere.
        CHECK (pitchesIn (drums, 15.0, 16.0) == std::set<int> { 43, 45, 47, 48 });
        CHECK (pitchesIn (drums, 31.0, 32.0) == std::set<int> { 43, 45, 47, 48 });
        CHECK (pitchesIn (drums, 7.0, 8.0) == std::set<int> { 38, 42 });
    }
    // ROLL picks another of the section's best grooves.
    s.lane (ComposerLayer::drums).variation = 1;
    CHECK (composeLayers (s, spans, c)[(size_t) ComposerLayer::drums].style == "My library: Verse Groove 02");
    // A chorus: the chorus's groove.
    s.lane (ComposerLayer::drums).style = 2;
    s.lane (ComposerLayer::drums).variation = 0;
    {
        const auto all = composeLayers (s, spans, c);
        CHECK (all[(size_t) ComposerLayer::drums].style == "My library: Chorus Groove 01");
        CHECK (pitchesIn (all[(size_t) ComposerLayer::drums], 0.0, 4.0).count (51) == 1);
    }
    // A groove chosen by hand wins over the section.
    s.libraryGroove = library.grooves[0].file.getFullPathName();
    CHECK (composeLayers (s, spans, c)[(size_t) ComposerLayer::drums].style == "My library: Verse Groove 01");
    CHECK (ComposerState::fromString (s.toString()) == s);
    // Off: the generator's (or the type's) drums again.
    s.libraryDrums = false;
    CHECK_FALSE (composeLayers (s, spans, c)[(size_t) ComposerLayer::drums].style.startsWith ("My library"));
    CHECK (ComposerState::fromString (s.toString()).libraryDrums == false);
    s.libraryDrums = true;
    s.libraryGroove = {};
    // No library: as before.
    ComposerContext none;
    CHECK_FALSE (composeLayers (s, spans, none)[(size_t) ComposerLayer::drums].style.startsWith ("My library"));
    // Older settings text (no library part): on.
    auto older = s.toString();
    older = older.upToLastOccurrenceOf ("|", false, false).upToLastOccurrenceOf ("|", false, false);
    CHECK (ComposerState::fromString (older).libraryDrums);
}

TEST_CASE ("0.106.0: library grooves over a section type's drums; SYNC still makes the drums follow; the bass locks to the library's kick", "[composer][library]")
{
    Library library;
    ComposerContext c;
    c.libraryGrooves = library.grooves;
    const auto spans = eightBars();
    ComposerState s;
    applySectionType (s, "Chorus", "DRIVING");
    s.lane (ComposerLayer::drums).style = 2;
    {
        const auto all = composeLayers (s, spans, c);
        CHECK (all[(size_t) ComposerLayer::drums].style == "My library: Chorus Groove 01");
        // LOCKED WITH THE KICK: the bass on the library groove's kicks (beat 0 of every bar).
        auto locked = s;
        locked.lane (ComposerLayer::bass).approach = sectionApproaches (s, ComposerLayer::bass).size() - 1;
        const auto bass = composeLayers (locked, spans, c)[(size_t) ComposerLayer::bass];
        REQUIRE_FALSE (bass.notes.empty());
        for (const auto& n : bass.notes)
            CHECK (std::abs (std::fmod (n.beat, 4.0)) < 1.0e-6);
    }
    // SYNC on the guitar: the drums follow it (generated, locked to its hits), not the library.
    s.syncLane = (int) ComposerLayer::guitar;
    CHECK_FALSE (composeLayers (s, spans, c)[(size_t) ComposerLayer::drums].style.startsWith ("My library"));
    // The drums leading: the library groove.
    s.syncLane = (int) ComposerLayer::drums;
    CHECK (composeLayers (s, spans, c)[(size_t) ComposerLayer::drums].style.startsWith ("My library"));
}
