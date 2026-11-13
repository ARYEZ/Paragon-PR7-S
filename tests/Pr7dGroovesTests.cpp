// (0.107.0, the owner: "you can't leverage the groove library that is scanned into the PR7-D? which is my drum plugin")
// PR7-D's groove library read from its own two files, its grooves' notes turned into the drum track's, and the
// composer's drums taking them; the studio's drums (PR7-D) on new drum tracks.

#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/Composer.h"
#include "engine/DrumIdeas.h"
#include "engine/Pr7dGrooves.h"
#include "engine/Studio.h"

#include <set>

using namespace pr7s;

namespace
{
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

    // PR7-D's presets folder as PR7-D writes it: Grooves.xml (the linked folders, each with its layout) and
    // Groove Index.xml (what it read).
    struct Pr7dFolder
    {
        juce::TemporaryFile temp;
        juce::File presets, packs, gmPack, hidden;
        juce::File verse, chorus, fill, waltz, outside, gmVerse;

        Pr7dFolder()
        {
            const auto root = temp.getFile();
            root.createDirectory();
            presets = root.getChildFile ("Rigs");
            presets.createDirectory();
            packs = root.getChildFile ("Packs");     // GROOVE LIBRARY layout (PR7-D's default)
            gmPack = root.getChildFile ("GM Pack");  // GENERAL MIDI
            hidden = root.getChildFile ("Hidden");   // excluded in PR7-D

            // GROOVE LIBRARY notes: kick 36, snare 38, the hats' closed tip 61, the open kick 34; cymbal 1 on 28.
            std::vector<std::pair<double, int>> v;
            for (int i = 0; i < 8; ++i)
                v.push_back ({ i * 0.5, 61 });
            v.push_back ({ 0.0, 36 });
            v.push_back ({ 1.0, 38 });
            v.push_back ({ 2.0, 34 });
            v.push_back ({ 3.0, 38 });
            verse = writeGroove (packs.getChildFile ("Metal/Verse/Pocket 01.mid"), v);
            chorus = writeGroove (packs.getChildFile ("Metal/Chorus/Big 01.mid"), { { 0.0, 28 }, { 0.0, 36 }, { 1.0, 51 }, { 2.0, 38 } });
            // A fill known only by the pack's label (no "fill" in its name), racktoms 1-3 and floortom 1.
            fill = writeGroove (packs.getChildFile ("Metal/Extras/Turnaround A.mid"), { { 3.0, 48 }, { 3.25, 47 }, { 3.5, 45 }, { 3.75, 43 } });
            waltz = writeGroove (packs.getChildFile ("Metal/Verse/Waltz 01.mid"), { { 0.0, 36 } });
            gmVerse = writeGroove (gmPack.getChildFile ("Verse/GM Verse.mid"), { { 0.0, 36 }, { 1.0, 38 }, { 0.0, 49 } });
            outside = writeGroove (hidden.getChildFile ("Verse/Hidden Verse.mid"), { { 0.0, 36 } });

            juce::XmlElement settings ("Grooves");
            auto folder = [&settings] (const juce::File& f, int map, bool excluded)
            {
                auto* e = settings.createNewChildElement ("Folder");
                e->setAttribute ("path", f.getFullPathName());
                e->setAttribute ("map", map);
                if (excluded)
                    e->setAttribute ("excluded", true);
            };
            folder (packs, 1, false);
            folder (gmPack, 2, false);
            folder (hidden, 1, true);
            settings.writeTo (presets.getChildFile ("Grooves.xml"));

            juce::XmlElement index ("GrooveIndex");
            auto groove = [&index] (const juce::File& f, double bpm, int num, int den, int bars, const juce::String& tags, const juce::String& stamp = "1|2|1")
            {
                auto* g = index.createNewChildElement ("G");
                g->setAttribute ("path", f.getFullPathName());
                g->setAttribute ("stamp", stamp);
                g->setAttribute ("bpm", bpm);
                g->setAttribute ("bpmKnown", 1);
                g->setAttribute ("num", num);
                g->setAttribute ("den", den);
                g->setAttribute ("bars", bars);
                if (tags.isNotEmpty())
                    g->setAttribute ("tags", tags);
            };
            groove (verse, 120.0, 4, 4, 1, "Genre:Metal|Type:Beat");
            groove (chorus, 120.0, 4, 4, 1, "Genre:Metal|Type:Beat");
            groove (fill, 120.0, 4, 4, 1, "Genre:Metal|Type:Fill");
            groove (waltz, 120.0, 3, 4, 1, {});
            groove (gmVerse, 120.0, 4, 4, 1, {});
            groove (outside, 120.0, 4, 4, 1, {});
            groove (packs.getChildFile ("midiDB/Pack/Verse/Inside.mid"), 120.0, 4, 4, 1, {}, "db4:1|2|1");   // (a groove database's)
            index.writeTo (presets.getChildFile ("Groove Index.xml"));
        }
    };

    std::vector<ChordSpan> eightBars()
    {
        std::vector<ChordSpan> spans;
        for (int bar = 0; bar < 8; ++bar)
            spans.push_back ({ bar * 4.0, bar * 4.0 + 4.0, Chord { 9, Chord::Type::minor }, Key {} });
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

TEST_CASE ("0.107.0: PR7-D's groove library is read from its own files", "[pr7d][library]")
{
    Pr7dFolder folder;
    const auto library = readPr7dLibrary (folder.presets);
    REQUIRE (library.found);
    // The database's groove and the excluded folder's are left out.
    REQUIRE (library.grooves.size() == 5);
    std::set<juce::String> names;
    for (const auto& g : library.grooves)
        names.insert (g.name);
    CHECK (names == std::set<juce::String> { "Pocket 01", "Big 01", "Turnaround A", "Waltz 01", "GM Verse" });
    for (const auto& g : library.grooves)
    {
        if (g.name == "GM Verse")
            CHECK (g.layout == 2);
        else
            CHECK (g.layout == 1);
        if (g.name == "Turnaround A")
            CHECK (g.tags.contains ("Type:Fill"));
        if (g.name == "Waltz 01")
            CHECK (g.barBeats() == 3.0);
    }
    // Nothing there: not found, no grooves.
    juce::TemporaryFile none;
    CHECK_FALSE (readPr7dLibrary (none.getFile()).found);
}

TEST_CASE ("0.107.0: a groove's notes are turned into the drum track's (General MIDI or the PR7-D Map)", "[pr7d][library]")
{
    // GROOVE LIBRARY -> General MIDI: the open kick, the hats' pedal-following and closed notes, the floor toms,
    // cymbal 1, a pad (nothing there).
    CHECK (convertDrumNote (34, 1, DrumNotes::generalMidi) == 36);
    CHECK (convertDrumNote (7, 1, DrumNotes::generalMidi) == 42);
    CHECK (convertDrumNote (61, 1, DrumNotes::generalMidi) == 42);
    CHECK (convertDrumNote (43, 1, DrumNotes::generalMidi) == 41);
    CHECK (convertDrumNote (28, 1, DrumNotes::generalMidi) == 49);
    CHECK (convertDrumNote (48, 1, DrumNotes::generalMidi) == 48);
    CHECK (convertDrumNote (0, 1, DrumNotes::generalMidi) == -1);
    // General MIDI -> the PR7-D Map: the crash onto cymbal 1, the kick and snare as they are.
    CHECK (convertDrumNote (49, 2, DrumNotes::pr7d) == 28);
    CHECK (convertDrumNote (36, 2, DrumNotes::pr7d) == 36);
    CHECK (convertDrumNote (38, 2, DrumNotes::pr7d) == 38);
    CHECK (convertDrumNote (56, 2, DrumNotes::pr7d) == -1);   // (the cowbell: PR7-D has none)
    // Same layout, CUSTOM, or as written: unchanged. ROOT from C1.
    CHECK (convertDrumNote (61, 1, DrumNotes::pr7d) == 61);
    CHECK (convertDrumNote (61, 3, DrumNotes::generalMidi) == 61);
    CHECK (convertDrumNote (61, 1, DrumNotes::asWritten) == 61);
    CHECK (convertDrumNote (38, 0, DrumNotes::generalMidi) == 38);   // (root + 2: the snare)
    CHECK (convertDrumNote (36, 0, DrumNotes::pr7d) == 36);
}

TEST_CASE ("0.107.0: the composer's drums play PR7-D's grooves, labelled fills at the phrase ends", "[pr7d][library][composer]")
{
    Pr7dFolder folder;
    Studio studio (false);
    studio.setPr7dFolderForTests (folder.presets);
    auto c = studio.getComposerContext (0.0);
    // PR7-D's library wins: its 4/4 grooves (not the waltz), fills by their label.
    REQUIRE (c.libraryGrooves.size() == 4);
    for (const auto& g : c.libraryGrooves)
    {
        CHECK (g.name != "Waltz 01");
        CHECK (g.notes == DrumNotes::generalMidi);   // (no PR7-D in the tests: the drums track would be General MIDI)
    }
    REQUIRE (libraryGroovesFor (c.libraryGrooves, 1, true).size() == 1);
    CHECK (libraryGroovesFor (c.libraryGrooves, 1, true).front().name == "Turnaround A");
    CHECK (libraryGroovesFor (c.libraryGrooves, 2, false).front().name == "Big 01");

    c.drums.fillEvery = 4;
    ComposerState s;
    s.lane (ComposerLayer::drums).style = 1;   // (a verse)
    s.lane (ComposerLayer::drums).variation = 0;
    const auto spans = eightBars();
    const auto all = composeLayers (s, spans, c);
    const auto& drums = all[(size_t) ComposerLayer::drums];
    CHECK (drums.style.startsWith ("My library:"));
    // The groove library's notes turned into General MIDI: the closed tip (61) a closed hat, the open kick (34) a kick.
    const auto verse = pitchesIn (drums, 0.0, 12.0);
    CHECK (verse.count (61) == 0);
    CHECK (verse.count (34) == 0);
    if (drums.style == "My library: Pocket 01")
        CHECK (verse == std::set<int> { 36, 38, 42 });
    // The labelled fill on bar 4's last beat: the racktoms and floortom 1 as General MIDI's toms.
    CHECK (pitchesIn (drums, 15.0, 16.0) == std::set<int> { 41, 43, 45, 48 });

    // With the PR7-D Map on the drums track, the grooves stay on PR7-D's notes.
    const auto id = studio.addDrumsTrack();
    studio.setTrackDrumMap (id, DrumMap::pr7d().name);
    CHECK (studio.composerDrumNotes() == DrumNotes::pr7d);
    c = studio.getComposerContext (0.0);
    for (const auto& g : c.libraryGrooves)
        CHECK (g.notes == DrumNotes::pr7d);
    s.lane (ComposerLayer::drums).style = 1;
    const auto onPr7d = composeLayers (s, spans, c)[(size_t) ComposerLayer::drums];
    if (onPr7d.style == "My library: Pocket 01")
        CHECK (pitchesIn (onPr7d, 0.0, 4.0).count (61) == 1);
    // ... and the bass's LOCKED WITH THE KICK still finds the open kick (34) as a kick.
    LineIdea open { LineIdea::Part::drums, "", "", { { 0.0, 0.25, 34, 0.8f }, { 2.0, 0.25, 36, 0.8f } } };
    CHECK (kickHits (open).size() == 2);
}

TEST_CASE ("0.107.0: a new drums track: the studio's drums when found, else the studio's synth, on the default map", "[pr7d][backline]")
{
    Studio studio (false);
    // (The tests don't look for PR7-D: the studio's synth stands in.)
    CHECK (studio.drumsInstrument().kind == Instrument::Kind::paragonSynth);
    const auto id = studio.addDrumsTrack();
    REQUIRE (id != 0);
    const auto* track = studio.getProject().findTrack (id);
    REQUIRE (track != nullptr);
    CHECK (track->name == "Drums");
    CHECK (track->drumMap.isNotEmpty());
    // No PR7-D library set: none read.
    CHECK_FALSE (studio.getPr7dLibrary().found);
}
