#include <catch2/catch_test_macros.hpp>

#include "engine/Studio.h"
#include "engine/Composer.h"
#include "engine/KeyCheck.h"
#include "engine/RiffFinder.h"
#include "ui/ComposerPage.h"
#include "ui/Finish.h"

// (0.227.0) FIND A RIFF IN THE MEDIABAY: its MIDI files, not drums, 1-16 bars, moved into the song's key.

using namespace pr7s;

namespace
{
    struct Folder
    {
        juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("PR7S-riffs", "");
        Folder() { dir.createDirectory(); }
        ~Folder() { dir.deleteRecursively(); }
    };

    // A MIDI file of `bars` 4/4 bars of eighths walking `pitches` round, on `channel`.
    juce::File writeRiff (const juce::File& dir, const juce::String& name, std::vector<int> pitches, int bars, int channel = 1)
    {
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);
        for (int i = 0; i < bars * 8; ++i)
        {
            const auto p = pitches[(size_t) i % pitches.size()];
            seq.addEvent (juce::MidiMessage::noteOn (channel, p, (juce::uint8) 100), i * 240.0);
            seq.addEvent (juce::MidiMessage::noteOff (channel, p), i * 240.0 + 200.0);
        }
        seq.updateMatchedPairs();
        juce::MidiFile file;
        file.setTicksPerQuarterNote (480);
        file.addTrack (seq);
        auto out = dir.getChildFile (name + ".mid");
        juce::FileOutputStream stream (out);
        file.writeTo (stream);
        return out;
    }

    MediaItem itemOf (const juce::File& file, const juce::String& key, int bars)
    {
        MediaItem item;
        item.file = file;
        item.type = MediaType::midi;
        item.name = file.getFileNameWithoutExtension();
        item.bpm = 120.0;
        item.key = key;
        item.beats = bars * 4.0;
        item.notes = bars * 8;
        return item;
    }

    const RiffFind* named (const std::vector<RiffFind>& found, const juce::String& name)
    {
        for (const auto& f : found)
            if (f.name == name)
                return &f;
        return nullptr;
    }

    double inKeyShare (const ComposerRiff& riff, const Key& key)
    {
        int in = 0;
        for (const auto& n : riff.notes)
            in += isPitchClassInKey (n.pitch % 12, key) ? 1 : 0;
        return riff.notes.empty() ? 0.0 : in / (double) riff.notes.size();
    }
}

TEST_CASE ("0.227.0: riffs found in the MediaBay: drums and songs left out, moved into the song's key, the best first", "[composer][riff][riffinder]")
{
    Folder folder;
    const auto aMinor = *keyFromText ("Am");
    CHECK (aMinor.name() == "A minor");
    CHECK (keyFromText ("F#")->name() == "F# major");
    CHECK (keyFromText ("Bbm")->name() == "Bb minor");
    CHECK_FALSE (keyFromText ("").has_value());

    // E minor (E G A B D), C major, and A minor already: the C major one's notes stay in the key moved to A minor.
    const auto eMinor = writeRiff (folder.dir, "Chug verse riff", { 40, 43, 45, 47, 50, 40 }, 2);
    const auto aMinorRiff = writeRiff (folder.dir, "Low riff", { 33, 36, 38, 40, 43 }, 1);
    const auto cMajor = writeRiff (folder.dir, "Bright chorus", { 48, 52, 55, 57, 60 }, 4);
    const auto drums = writeRiff (folder.dir, "Metal drums 01", { 36, 38, 42 }, 2);
    const auto onTen = writeRiff (folder.dir, "Something", { 36, 38, 42 }, 2, 10);
    const auto song = writeRiff (folder.dir, "Whole song idea", { 40, 43, 45 }, 40);

    std::vector<MediaItem> items { itemOf (eMinor, "Em", 2), itemOf (aMinorRiff, "", 1), itemOf (cMajor, "C", 4),
                                   itemOf (drums, "", 2), itemOf (onTen, "", 2), itemOf (song, "", 2) };
    items.back().beats = 0.0;   // not known: read, then left out for its length
    MediaItem audio = itemOf (eMinor, "", 2);
    audio.type = MediaType::audio;
    items.push_back (audio);

    CHECK (looksLikeDrums (items[3]));
    CHECK_FALSE (looksLikeDrums (items[0]));

    RiffFindRequest request;
    request.key = aMinor;
    request.section = DrumSection::verse;
    const auto found = findRiffs (items, request);
    REQUIRE (found.size() == 3);
    CHECK (named (found, "Metal drums 01") == nullptr);
    CHECK (named (found, "Something") == nullptr);
    CHECK (named (found, "Whole song idea") == nullptr);

    const auto* e = named (found, "Chug verse riff");
    REQUIRE (e != nullptr);
    CHECK (e->keyKnown);
    CHECK (e->from.name() == "E minor");
    CHECK (e->shift == 5);
    CHECK (e->riff.bars == 2);
    CHECK (inKeyShare (e->riff, aMinor) == 1.0);
    CHECK (e->riff.notes.front().pitch % 12 == 9);   // E moved up to A
    for (const auto& n : e->riff.notes)
        CHECK ((n.pitch >= 33 && n.pitch <= 88));
    CHECK (e->reason.contains ("MOVED +5"));
    CHECK (e->reason.contains ("says verse"));

    const auto* a = named (found, "Low riff");
    REQUIRE (a != nullptr);
    CHECK_FALSE (a->keyKnown);
    CHECK (a->shift == 0);
    CHECK (a->riff.notes.front().pitch == 33);

    const auto* c = named (found, "Bright chorus");
    REQUIRE (c != nullptr);
    CHECK (c->shift == 0);   // C major is A minor's relative major: the same notes, not moved
    CHECK (inKeyShare (c->riff, aMinor) == 1.0);
    CHECK (c->riff.bars == 4);

    // The verse riff first: the section's word, in key.
    CHECK (found.front().name == "Chug verse riff");
    for (size_t i = 1; i < found.size(); ++i)
        CHECK (found[i - 1].score >= found[i].score);

    // Asked for the chorus: the chorus's file says it, the verse's counts against.
    request.section = DrumSection::chorus;
    const auto forChorus = findRiffs (items, request);
    REQUIRE (! forChorus.empty());
    CHECK (forChorus.front().name == "Bright chorus");

    // Stopped at once: nothing read.
    std::atomic<int> read { 0 };
    CHECK (findRiffs (items, request, [] { return true; }, &read).empty());
    CHECK (read == 0);
}

TEST_CASE ("0.227.0: ARCHITECT finds riffs, hears one on the riff's lane and keeps it as MY RIFF", "[composer][riff][riffinder]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Folder folder;
    Studio studio (false);
    const auto eMinor = writeRiff (folder.dir, "Chug verse riff", { 40, 43, 45, 47, 50, 40 }, 2);
    const auto cMajor = writeRiff (folder.dir, "Bright chorus", { 48, 52, 55, 57, 60 }, 4);
    studio.getMediaBay().setItemsForTests ({ itemOf (eMinor, "Em", 2), itemOf (cMajor, "C", 4) });

    ComposerPage page (studio);
    page.setSize (1700, 940);
    page.setRandomSeed (5);
    const auto aMinor = *keyFromText ("Am");
    page.setKey (aMinor);
    page.setSection (DrumSection::verse);

    const auto found = page.findRiffsNow();
    REQUIRE (found.size() == 2);
    auto browser = page.makeRiffBrowser (found);
    CHECK (browser->getShown().size() == 2);
    CHECK (browser->isAutoHear());

    // HEAR: the riff on the guitar lane, nothing kept.
    REQUIRE (page.hearFoundRiff (found.front()));
    CHECK (page.getState().lane (ComposerLayer::guitar).on);
    CHECK_FALSE (studio.getComposerRiff().has_value());
    page.stop();

    // USE IT: MY RIFF, in A minor, the key not changed by the guess.
    REQUIRE (page.useFoundRiff (found.front()));
    REQUIRE (studio.getComposerRiff().has_value());
    CHECK (inKeyShare (*studio.getComposerRiff(), aMinor) == 1.0);
    CHECK (page.getState().key == aMinor);
    CHECK (page.isPlayingMyRiff());

    // On its own thread too.
    page.findRiffsInMediaBay();
    CHECK (page.isFindingRiffs());
    page.waitForRiffsForTests();
    CHECK_FALSE (page.isFindingRiffs());
    browser.reset();
}
