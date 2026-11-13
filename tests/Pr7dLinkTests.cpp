// (0.108.0, the owner: "full integration" with PR7-D) The studio link: PR7-D plays the composer's drums from its own song
// track. The messages, the song file both ways, the muted copies, PR7-D's edits read back, the composer's HEAR pausing it.

#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/Pr7dLink.h"
#include "engine/Studio.h"

#include <set>

using namespace pr7s;

namespace
{
    // The path inside a load message (the halves put together again), else empty.
    juce::String pathIn (const juce::MidiMessage& m)
    {
        if (! m.isSysEx())
            return {};
        const auto* d = m.getSysExData();
        const auto n = m.getSysExDataSize();
        if (n < 6 || d[0] != 0x7D || d[1] != 'P' || d[2] != 'R' || d[3] != '7' || d[4] != 'S' || d[5] != 1)
            return {};
        juce::MemoryBlock utf8;
        for (int i = 6; i + 1 < n; i += 2)
        {
            const auto byte = (juce::uint8) ((d[i] << 4) | d[i + 1]);
            utf8.append (&byte, 1);
        }
        return juce::String::fromUTF8 ((const char*) utf8.getData(), (int) utf8.getSize());
    }

    int commandOf (const juce::MidiMessage& m)
    {
        return m.isSysEx() && m.getSysExDataSize() >= 6 ? (int) m.getSysExData()[5] : -1;
    }

    LineIdea beat (double length)
    {
        LineIdea idea { LineIdea::Part::drums, "My library: Pocket", "", {} };
        for (auto b = 0.0; b < length - 1.0e-9; b += 0.5)
            idea.notes.push_back ({ b, 0.25, 42, 0.6f });
        for (auto b = 0.0; b < length - 1.0e-9; b += 2.0)
        {
            idea.notes.push_back ({ b, 0.25, 36, 0.9f });
            idea.notes.push_back ({ b + 1.0, 0.25, 38, 0.8f });
        }
        return idea;
    }

    std::set<int> pitchesOf (const Part& p)
    {
        std::set<int> out;
        for (const auto& n : p.notes)
            out.insert (n.pitch);
        return out;
    }
}

TEST_CASE ("0.108.0: the link's messages carry their command and the file's path, any letters", "[pr7d][link]")
{
    const auto path = juce::String (juce::CharPointer_UTF8 ("C:\\Users\\Ar\xc3\xa9z\\PR7-D Link\\song.xml"));
    const auto load = pr7dLinkMessage (Pr7dCommand::load, path);
    REQUIRE (load.isSysEx());
    CHECK (commandOf (load) == 1);
    CHECK (pathIn (load) == path);
    // Every byte between F0 and F7 a 7-bit byte (MIDI's rule for SysEx).
    for (int i = 0; i < load.getSysExDataSize(); ++i)
        CHECK (load.getSysExData()[i] < 0x80);
    CHECK (commandOf (pr7dLinkMessage (Pr7dCommand::pause)) == 2);
    CHECK (commandOf (pr7dLinkMessage (Pr7dCommand::resume)) == 3);
}

TEST_CASE ("0.108.0: the copies as PR7-D's song and back: blocks per part, hits on PR7-D's drums", "[pr7d][link]")
{
    Part verse;
    verse.start = 0.0;
    verse.length = 8.0;
    verse.name = "Verse: My library: Pocket";
    for (const auto& n : beat (8.0).notes)
        verse.notes.push_back ({ n.beat, n.length, n.pitch, n.velocity });
    Part chorus = verse;
    chorus.start = 6.0;   // (overlapping the verse's last two beats: the verse's block is cut there)
    chorus.name = "Chorus: My library: Big";
    chorus.notes.push_back ({ 0.0, 0.25, 49, 1.0f });
    verse.linkCopy = chorus.linkCopy = true;

    // On the PR7-D Map's notes.
    const auto xml = pr7dSongXml ({ chorus, verse }, true, 4, 4, {}, "/tmp/link.xml");
    REQUIRE (xml->hasTagName ("Song"));
    CHECK (xml->getStringAttribute ("studioLink") == "/tmp/link.xml");
    const auto blocks = xml->getChildWithTagNameIterator ("Block");
    std::vector<const juce::XmlElement*> list (blocks.begin(), blocks.end());
    REQUIRE (list.size() == 2);
    CHECK (list[0]->getStringAttribute ("name") == verse.name);   // (in time order)
    CHECK (list[0]->getDoubleAttribute ("length") == 6.0);        // (cut where the chorus starts)
    CHECK (list[0]->getIntAttribute ("part") == 2);               // (PR7-D's VERSE)
    CHECK (list[1]->getIntAttribute ("part") == 4);               // (CHORUS)
    // The kick, snare and closed hat on PR7-D's BD (0), SD (2), CH (6).
    std::set<int> slots;
    for (const auto& token : juce::StringArray::fromTokens (list[0]->getStringAttribute ("hits"), " ", {}))
        slots.insert (juce::StringArray::fromTokens (token, ":", {})[1].getIntValue());
    CHECK (slots == std::set<int> { 0, 2, 6 });

    // Read back: the same drums on the same notes, the copies muted.
    const auto back = pr7dPartsFromSong (*xml, true);
    REQUIRE (back.size() == 2);
    CHECK (back[0].muted);
    CHECK (back[0].linkCopy);
    CHECK (back[0].length == 6.0);
    CHECK (pitchesOf (back[0]) == std::set<int> { 36, 38, 42 });
    // General MIDI's crash (49) is the PR7-D Map's cymbal 1 (28): on a PR7-D Map track the copy holds 28 for it.
    const auto gmXml = pr7dSongXml ({ chorus }, false, 4, 4, {}, "x");
    CHECK (pitchesOf (pr7dPartsFromSong (*gmXml, false)[0]).count (49) == 1);
    CHECK (pitchesOf (pr7dPartsFromSong (*gmXml, true)[0]).count (28) == 1);

    // A block's loop played round to its length, and its opening hit.
    juce::XmlElement song ("Song");
    auto* b = song.createNewChildElement ("Block");
    b->setAttribute ("start", 4.0);
    b->setAttribute ("length", 8.0);
    b->setAttribute ("loop", 2.0);
    b->setAttribute ("hits", "0:0:0.9 1:2:0.8");
    b->setAttribute ("opening", true);
    b->setAttribute ("openingSlot", 11);
    const auto looped = pr7dPartsFromSong (song, false);
    REQUIRE (looped.size() == 1);
    CHECK (looped[0].start == 4.0);
    CHECK (looped[0].notes.size() == 9);   // (8 hits round the loop + the crash)
}

TEST_CASE ("0.108.0: a composer drum part on a PR7-D track: a muted copy, PR7-D told to load its song", "[pr7d][link]")
{
    juce::TemporaryFile folder;
    folder.getFile().createDirectory();
    Studio studio (false);
    studio.setPr7dLinkFolderForTests (folder.getFile());
    const auto id = studio.addTrack (Instrument {});
    REQUIRE (id != 0);
    studio.treatAsPr7dForTests = [id] (int track) { return track == id; };
    studio.setTrackDrumMap (id, DrumMap::pr7d().name);
    std::vector<std::pair<int, juce::MidiMessage>> sent;
    studio.onPr7dMessage = [&sent] (int track, const juce::MidiMessage& m) { sent.emplace_back (track, m); };

    const std::vector<ChordSpan> spans { { 0.0, 8.0, Chord { 9, Chord::Type::minor }, Key {} } };
    REQUIRE (studio.addLinePart (beat (8.0), spans, id) == id);
    const auto* track = studio.getProject().findTrack (id);
    REQUIRE (track != nullptr);
    REQUIRE (track->parts.size() == 1);
    CHECK (track->parts[0].muted);        // (PR7-D plays it: the track's notes would play every hit twice)
    CHECK (track->parts[0].linkCopy);
    CHECK (track->drumLink.isNotEmpty());
    REQUIRE (sent.size() == 1);
    CHECK (sent[0].first == id);
    const auto file = studio.pr7dLinkFile (id);
    CHECK (pathIn (sent[0].second) == file.getFullPathName());
    REQUIRE (file.existsAsFile());
    const auto xml = juce::XmlDocument::parse (file);
    REQUIRE (xml != nullptr);
    CHECK (xml->getNumChildElements() == 1);

    // Nothing changed in the copies (another edit): nothing sent again. The copy moved (a SECTIONS move, say): sent again.
    studio.renameTrack (id, "Drums 2");
    CHECK (sent.size() == 1);
    studio.setPartsField ({ track->parts[0].id }, Studio::PartField::start, 4.0, true);
    REQUIRE (sent.size() == 2);
    CHECK (juce::XmlDocument::parse (file)->getChildByName ("Block")->getDoubleAttribute ("start") == 4.0);

    // PR7-D's edits written to the file: read back into the copy (and not sent back).
    {
        auto edited = juce::XmlDocument::parse (file);
        auto* block = edited->getChildByName ("Block");
        block->setAttribute ("hits", "0:0:1 1:2:1");   // (only a kick and a snare left)
        edited->writeTo (file);
        file.setLastModificationTime (juce::Time::getCurrentTime() + juce::RelativeTime::seconds (5));
    }
    CHECK (studio.readPr7dEdits() == 1);
    track = studio.getProject().findTrack (id);
    REQUIRE (track->parts.size() == 1);
    CHECK (track->parts[0].linkCopy);
    CHECK (pitchesOf (track->parts[0]) == std::set<int> { 36, 38 });
    CHECK (sent.size() == 2);
    CHECK (studio.readPr7dEdits() == 0);   // (read once)

    // The composer heard: PR7-D's song paused, then on again.
    REQUIRE (studio.auditionComposer ({ { id, beat (8.0) } }, 0.0, 8.0));
    REQUIRE (sent.size() == 3);
    CHECK (commandOf (sent[2].second) == 2);
    studio.stopComposerAudition();
    REQUIRE (sent.size() == 4);
    CHECK (commandOf (sent[3].second) == 3);

    // Turned off: the composer's drums on the track as notes, as before.
    studio.setPr7dPlaysDrums (false);
    studio.addLinePart (beat (4.0), { { 16.0, 20.0, Chord {}, Key {} } }, id);
    track = studio.getProject().findTrack (id);
    CHECK_FALSE (track->parts.back().muted);
    CHECK_FALSE (track->parts.back().linkCopy);
}

TEST_CASE ("0.108.0: a message for a track's instrument reaches it at the next block", "[pr7d][link][engine]")
{
    Engine engine;
    engine.prepare (44100.0, 512);
    Project project;
    Track track;
    track.id = 1;
    project.tracks.push_back (track);
    auto instrument = std::make_unique<Probe>();
    auto* probe = instrument.get();
    engine.setInstrument (1, std::move (instrument));
    engine.update (project);
    CHECK_FALSE (engine.sendToInstrument (2, pr7dLinkMessage (Pr7dCommand::pause)));   // (no such track)
    REQUIRE (engine.sendToInstrument (1, pr7dLinkMessage (Pr7dCommand::load, "/tmp/a.xml")));
    juce::AudioBuffer<float> out (2, 512);
    engine.render (out, 512);   // (stopped: still delivered)
    auto found = 0;
    for (const auto& r : probe->received)
        if (pathIn (r.message) == "/tmp/a.xml")
            ++found;
    CHECK (found == 1);
    engine.render (out, 512);
    found = 0;
    for (const auto& r : probe->received)
        if (r.message.isSysEx())
            ++found;
    CHECK (found == 1);   // (once)
}
