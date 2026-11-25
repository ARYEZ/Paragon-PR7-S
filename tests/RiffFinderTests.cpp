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

TEST_CASE ("0.227.1: drum grooves left out wherever they are: Toontrack's folders, drum notes on channel 1, the groove library", "[composer][riff][riffinder]")
{
    Folder folder;
    const auto aMinor = *keyFromText ("Am");

    // The owner's layout: G:\Paragon MIDI\Toontrack\000329@21st_Century_Metal\520@Straight_7#4\200-S125@Bridge\Variation_01.mid
    // (the "Toontrack" folder put four up from the file, past where the name check looked).
    auto deep = folder.dir.getChildFile ("Paragon MIDI").getChildFile ("Packs").getChildFile ("000329@21st_Century_Metal")
                          .getChildFile ("520@Straight_7#4").getChildFile ("200-S125@Bridge");
    deep.createDirectory();
    const auto coded = writeRiff (deep, "Variation_01", { 40, 43, 45 }, 2);
    auto toontrack = folder.dir.getChildFile ("Toontrack").getChildFile ("A").getChildFile ("B").getChildFile ("C");
    toontrack.createDirectory();
    const auto byName = writeRiff (toontrack, "Variation_02", { 40, 43, 45 }, 2);

    // A drum groove on channel 1 named like a riff: kick and hat together, snare on the backbeat.
    juce::File grooveLike;
    {
        juce::MidiMessageSequence seq;
        for (int i = 0; i < 16; ++i)
        {
            const auto t = i * 240.0;
            seq.addEvent (juce::MidiMessage::noteOn (1, 42, (juce::uint8) 90), t);
            seq.addEvent (juce::MidiMessage::noteOff (1, 42), t + 60);
            if (i % 4 == 0)
            {
                seq.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 110), t);
                seq.addEvent (juce::MidiMessage::noteOff (1, 36), t + 60);
            }
            if (i % 4 == 2)
            {
                seq.addEvent (juce::MidiMessage::noteOn (1, 38, (juce::uint8) 110), t);
                seq.addEvent (juce::MidiMessage::noteOff (1, 38), t + 60);
            }
        }
        seq.updateMatchedPairs();
        juce::MidiFile file;
        file.setTicksPerQuarterNote (480);
        file.addTrack (seq);
        grooveLike = folder.dir.getChildFile ("Heavy idea.mid");
        juce::FileOutputStream stream (grooveLike);
        file.writeTo (stream);
    }
    const auto inLibrary = writeRiff (folder.dir, "Library thing", { 40, 43, 45 }, 2);
    const auto riff = writeRiff (folder.dir, "Chug verse riff", { 40, 43, 45, 47, 50, 40 }, 2);
    // A drop-A tritone riff (A1, D#2, the C2 / D2 / F#2 around it) must not read as drums.
    const auto tritone = writeRiff (folder.dir, "Tritone riff", { 33, 39, 36, 42, 38, 33 }, 2);

    std::vector<MediaItem> items { itemOf (coded, "", 2), itemOf (byName, "", 2), itemOf (grooveLike, "", 4),
                                   itemOf (inLibrary, "", 2), itemOf (riff, "Em", 2), itemOf (tritone, "", 2) };
    CHECK (looksLikeDrums (items[0]));
    CHECK (looksLikeDrums (items[1]));
    CHECK_FALSE (looksLikeDrums (items[2]));   // its name says nothing: its notes do

    RiffFindRequest request;
    request.key = aMinor;
    request.drumFiles = { inLibrary.getFullPathName() };
    CHECK (isKnownDrumFile (inLibrary, request));
    const auto found = findRiffs (items, request);
    REQUIRE (found.size() == 2);
    CHECK (named (found, "Chug verse riff") != nullptr);
    CHECK (named (found, "Tritone riff") != nullptr);

    // A file inside one of PR7-D's linked groove folders.
    request.drumFiles.clear();
    request.drumFolders = { folder.dir };
    CHECK (findRiffs (items, request).empty());
}

TEST_CASE ("0.227.2: synth MIDI in packs called \"... Kit\" or \"... Beats\" is found, moved down into a guitar's range", "[composer][riff][riffinder]")
{
    Folder folder;
    auto kit = folder.dir.getChildFile ("Cyberpunk Kit").getChildFile ("MIDI");
    kit.createDirectory();
    auto dnb = folder.dir.getChildFile ("Drum & Bass Beats");
    dnb.createDirectory();
    auto drums = folder.dir.getChildFile ("Cyberpunk Kit").getChildFile ("Drums");
    drums.createDirectory();

    // A synth line two octaves above a guitar's middle, in a sample pack's MIDI folder.
    const auto synth = writeRiff (kit, "Synth Lead 01 Am", { 81, 84, 88, 86, 84, 81 }, 2);
    const auto loose = writeRiff (kit, "Idea 3", { 69, 72, 76, 74 }, 2);
    const auto bassLine = writeRiff (dnb, "Reese Bass Line", { 45, 48, 52 }, 2);
    const auto drumOne = writeRiff (drums, "Loop 1", { 45, 48, 52 }, 2);
    const auto bassDrum = writeRiff (kit, "Bass Drum", { 45, 48, 52 }, 2);
    const auto beat = writeRiff (kit, "Beat 2", { 45, 48, 52 }, 2);

    std::vector<MediaItem> items { itemOf (synth, "Am", 2), itemOf (loose, "", 2), itemOf (bassLine, "", 2),
                                   itemOf (drumOne, "", 2), itemOf (bassDrum, "", 2), itemOf (beat, "", 2) };
    CHECK_FALSE (looksLikeDrums (items[0]));   // "Kit" in a folder isn't drums any more
    CHECK_FALSE (looksLikeDrums (items[1]));
    CHECK_FALSE (looksLikeDrums (items[2]));   // its own name says bass: "Drum & Bass" in the folder doesn't count
    CHECK (looksLikeDrums (items[3]));         // a "Drums" folder still does
    CHECK (looksLikeDrums (items[4]));         // "Bass Drum" is a drum
    CHECK (looksLikeDrums (items[5]));         // a file called Beat

    RiffFindRequest request;
    request.key = *keyFromText ("Am");
    const auto found = findRiffs (items, request);
    REQUIRE (found.size() == 3);
    const auto* lead = named (found, "Synth Lead 01 Am");
    REQUIRE (lead != nullptr);
    CHECK (lead->playable == 1.0);
    int highest = 0;
    for (const auto& n : lead->riff.notes)
        highest = std::max (highest, n.pitch);
    CHECK (highest <= 88);   // brought down by octaves into a guitar's range
    CHECK (named (found, "Idea 3") != nullptr);
    CHECK (named (found, "Reese Bass Line") != nullptr);
}

namespace
{
    // A MIDI file of the given notes (beat, length, pitch), on channel 1.
    juce::File writeNotes (const juce::File& dir, const juce::String& name, const std::vector<std::array<double, 3>>& notes)
    {
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);
        for (const auto& n : notes)
        {
            seq.addEvent (juce::MidiMessage::noteOn (1, (int) n[2], (juce::uint8) 100), n[0] * 480.0);
            seq.addEvent (juce::MidiMessage::noteOff (1, (int) n[2]), (n[0] + n[1]) * 480.0 - 1.0);
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

    std::vector<LineNote> lineNotes (const std::vector<std::array<double, 3>>& notes)
    {
        std::vector<LineNote> out;
        for (const auto& n : notes)
        {
            LineNote l;
            l.beat = n[0];
            l.length = n[1];
            l.pitch = (int) n[2];
            out.push_back (l);
        }
        return out;
    }
}

TEST_CASE ("0.228.0: riff quality: pads, single held notes and noodling left out, riffs kept; synth lines brought down", "[composer][riff][riffinder]")
{
    Folder folder;
    const auto aMinor = *keyFromText ("Am");

    // A chug riff: palm-muted eighths with an accent pattern, two bars that repeat.
    std::vector<std::array<double, 3>> chug;
    for (int bar = 0; bar < 2; ++bar)
        for (const auto [b, p] : std::vector<std::pair<double, int>> { { 0, 33 }, { 0.5, 33 }, { 0.75, 33 }, { 1.5, 36 }, { 2, 33 },
                                                                       { 2.5, 33 }, { 3, 39 }, { 3.5, 38 } })
            chug.push_back ({ bar * 4 + b, 0.2, (double) p });
    // A pad: five-note chords held a bar each.
    std::vector<std::array<double, 3>> pad;
    for (int bar = 0; bar < 4; ++bar)
        for (const auto p : { 45, 52, 57, 60, 64 })
            pad.push_back ({ bar * 4.0, 4.0, (double) (p + (bar % 2) * 3) });
    // One long note a bar.
    std::vector<std::array<double, 3>> held;
    for (int bar = 0; bar < 4; ++bar)
        held.push_back ({ bar * 4.0, 3.5, 57.0 + (bar % 2) * 2 });
    // Noodling: random-feeling notes jumping round the keyboard, no bar like another.
    std::vector<std::array<double, 3>> noodle;
    juce::Random random (17);
    for (int i = 0; i < 24; ++i)
        noodle.push_back ({ i * 0.333 + random.nextFloat() * 0.1, 0.2, (double) (40 + random.nextInt (40)) });
    // A synth arpeggio line up the keyboard: sixteenths, the same shape each bar.
    std::vector<std::array<double, 3>> synth;
    for (int bar = 0; bar < 2; ++bar)
        for (int s = 0; s < 16; ++s)
            synth.push_back ({ bar * 4 + s * 0.25, 0.2, (double) std::vector<int> { 81, 84, 88, 84, 81, 79, 76, 79 }[(size_t) s % 8] });

    // The measures alone.
    const auto chugQ = riffQuality (lineNotes (chug), 2, 4.0);
    const auto padQ = riffQuality (lineNotes (pad), 4, 4.0);
    const auto heldQ = riffQuality (lineNotes (held), 4, 4.0);
    const auto noodleQ = riffQuality (lineNotes (noodle), 2, 4.0);
    CHECK (chugQ.score > 0.85);
    CHECK (chugQ.problems.isEmpty());
    CHECK (padQ.score < 0.4);
    CHECK (padQ.problems.contains ("big stacked chords"));
    CHECK (padQ.problems.contains ("mostly held notes"));
    CHECK (heldQ.score < 0.5);
    CHECK (heldQ.problems.contains ("mostly held notes"));
    CHECK (noodleQ.score < 0.7);
    CHECK (noodleQ.problems.contains ("jumps round the keyboard"));
    CHECK (riffQualityThreshold (0) == 0.0);
    CHECK (riffQualityThreshold (1) < riffQualityThreshold (2));

    std::vector<MediaItem> items { itemOf (writeNotes (folder.dir, "Chug", chug), "Am", 2),
                                   itemOf (writeNotes (folder.dir, "Pad chords", pad), "Am", 4),
                                   itemOf (writeNotes (folder.dir, "Held", held), "Am", 4),
                                   itemOf (writeNotes (folder.dir, "Noodle", noodle), "Am", 2),
                                   itemOf (writeNotes (folder.dir, "Synth arp", synth), "Am", 2) };
    for (auto& item : items)
        item.notes = 8;

    RiffFindRequest request;
    request.key = aMinor;
    const auto normal = findRiffs (items, request);
    CHECK (named (normal, "Chug") != nullptr);
    // (0.230.0) A pad is no longer left out: it's found as chords for a guitar.
    REQUIRE (named (normal, "Pad chords") != nullptr);
    CHECK (named (normal, "Pad chords")->pad);
    CHECK (named (normal, "Held") == nullptr);
    const auto* arp = named (normal, "Synth arp");
    REQUIRE (arp != nullptr);
    // Brought down from A5 to the riffing strings (its middle note under C3).
    std::vector<int> pitches;
    for (const auto& n : arp->riff.notes)
        pitches.push_back (n.pitch);
    std::sort (pitches.begin(), pitches.end());
    CHECK (pitches[pitches.size() / 2] <= 60);
    CHECK (pitches.front() >= 33);
    CHECK (named (normal, "Chug")->reason.startsWith ("riff "));

    // EVERYTHING lists them all; STRICT keeps the chug.
    request.leastQuality = riffQualityThreshold (0);
    CHECK (findRiffs (items, request).size() == 5);
    request.leastQuality = riffQualityThreshold (2);
    CHECK (named (findRiffs (items, request), "Chug") != nullptr);

    // A hidden file isn't read.
    request.leastQuality = 0.0;
    request.hidden = { items[0].file.getFullPathName() };
    CHECK (named (findRiffs (items, request), "Chug") == nullptr);

    // The studio keeps the choice and the hidden files.
    Studio studio (false);
    CHECK (studio.getRiffQualityLevel() == 1);
    studio.setRiffQualityLevel (2);
    CHECK (studio.getRiffQualityLevel() == 2);
    studio.hideRiff (items[1].file);
    studio.hideRiff (items[1].file);
    CHECK (studio.getHiddenRiffs().size() == 1);
    studio.clearHiddenRiffs();
    CHECK (studio.getHiddenRiffs().empty());
}

TEST_CASE ("0.230.0: a pad's held chords found and played as a guitar's ringing power chords", "[composer][riff][riffinder][pads]")
{
    Folder folder;
    const auto aMinor = *keyFromText ("Am");
    // Am, F, C, G held a bar each, voiced up the keyboard (five notes).
    const std::vector<std::vector<int>> chords { { 57, 64, 69, 72, 76 }, { 53, 60, 65, 69, 72 }, { 48, 55, 60, 64, 67 }, { 55, 62, 67, 71, 74 } };
    std::vector<std::array<double, 3>> pad;
    for (int bar = 0; bar < 4; ++bar)
        for (const auto p : chords[(size_t) bar])
            pad.push_back ({ bar * 4.0, 4.0, (double) p });

    CHECK (looksLikePad (lineNotes (pad), 4.0));
    std::vector<std::array<double, 3>> chug;
    for (int i = 0; i < 16; ++i)
        chug.push_back ({ i * 0.5, 0.2, 33.0 });
    CHECK_FALSE (looksLikePad (lineNotes (chug), 4.0));

    double read = 0.0;
    const auto guitar = padAsGuitar (lineNotes (pad), 4.0, 16.0, &read);
    CHECK (read == 1.0);
    // Two strikes a bar (the change and the half bar), three notes each: root, fifth, octave on the low strings.
    REQUIRE (guitar.size() == 4 * 2 * 3);
    const int roots[] { 9, 5, 0, 7 };
    for (size_t i = 0; i < guitar.size(); i += 3)
    {
        const auto bar = (int) (guitar[i].beat / 4.0);
        CHECK (guitar[i].pitch % 12 == roots[bar]);
        CHECK (guitar[i].pitch >= 33);
        CHECK (guitar[i].pitch <= 44);
        CHECK (guitar[i + 1].pitch == guitar[i].pitch + 7);
        CHECK (guitar[i + 2].pitch == guitar[i].pitch + 12);
        CHECK (guitar[i].length > 1.8);   // rung to the next strike
        CHECK (guitar[i].length < 2.0);
    }

    // Found in the MediaBay as a pad, and passes NORMAL and STRICT.
    std::vector<MediaItem> items { itemOf (writeNotes (folder.dir, "Soft pad", pad), "Am", 4) };
    items[0].notes = 20;
    RiffFindRequest request;
    request.key = aMinor;
    request.leastQuality = riffQualityThreshold (2);
    const auto found = findRiffs (items, request);
    const auto* f = named (found, "Soft pad");
    REQUIRE (f != nullptr);
    CHECK (f->pad);
    CHECK (f->reason.contains ("PAD: played as chords"));
    CHECK (f->riff.notes.size() == guitar.size());
}

TEST_CASE ("0.231.0: a pad strummed as full chord shapes: downs and ups on the eighths, a down at every change", "[composer][riff][riffinder][pads]")
{
    Folder folder;
    const auto aMinor = *keyFromText ("Am");
    const std::vector<std::vector<int>> chords { { 57, 64, 69, 72, 76 }, { 53, 60, 65, 69, 72 }, { 48, 55, 60, 64, 67 }, { 55, 62, 67, 71, 74 } };
    std::vector<std::array<double, 3>> pad;
    for (int bar = 0; bar < 4; ++bar)
        for (const auto p : chords[(size_t) bar])
            pad.push_back ({ bar * 4.0, 4.0, (double) p });

    double read = 0.0;
    const auto strum = padAsGuitar (lineNotes (pad), 4.0, 16.0, &read, PadStyle::strummed);
    CHECK (read == 1.0);
    const int roots[] { 9, 5, 0, 7 };
    // Strokes: notes starting within 0.03 beat of one another.
    struct Stroke { double at; std::vector<int> pitches; };
    std::vector<Stroke> strokes;
    for (const auto& n : strum)
    {
        CHECK (n.pitch >= 40);
        CHECK (n.pitch <= 76);
        if (strokes.empty() || n.beat - strokes.back().at > 0.03)
            strokes.push_back ({ n.beat, {} });
        strokes.back().pitches.push_back (n.pitch);
    }
    // D . D U . U D U a bar: six strokes, four bars.
    REQUIRE (strokes.size() == 24);
    const double places[] { 0.0, 1.0, 1.5, 2.5, 3.0, 3.5 };
    for (size_t s = 0; s < strokes.size(); ++s)
    {
        const auto bar = (int) (s / 6);
        CHECK (std::abs (strokes[s].at - (bar * 4.0 + places[s % 6])) < 1e-9);
        const auto down = s % 6 == 0 || s % 6 == 1 || s % 6 == 4;
        if (down)
        {
            CHECK (strokes[s].pitches.size() >= 5);                     // the whole shape, low to high
            CHECK (std::is_sorted (strokes[s].pitches.begin(), strokes[s].pitches.end()));
            CHECK (strokes[s].pitches.front() % 12 == roots[bar]);       // the root on the bottom
        }
        else
        {
            CHECK (strokes[s].pitches.size() == 4);                     // the top four, high to low
            CHECK (std::is_sorted (strokes[s].pitches.rbegin(), strokes[s].pitches.rend()));
        }
        // Every note of the chord (A minor, F, C, G) is one of its chord's.
        Chord shape;
        shape.root = roots[bar];
        shape.type = bar == 0 ? Chord::Type::minor : Chord::Type::major;
        const auto pcs = shape.pitchClasses();
        for (const auto p : strokes[s].pitches)
            CHECK (std::find (pcs.begin(), pcs.end(), p % 12) != pcs.end());
    }

    // Through FIND A RIFF with the strummed choice.
    std::vector<MediaItem> items { itemOf (writeNotes (folder.dir, "Soft pad", pad), "Am", 4) };
    items[0].notes = 20;
    RiffFindRequest request;
    request.key = aMinor;
    request.padStyle = PadStyle::strummed;
    const auto found = findRiffs (items, request);
    const auto* f = named (found, "Soft pad");
    REQUIRE (f != nullptr);
    CHECK (f->pad);
    CHECK (f->reason.contains ("PAD: strummed as chords"));
    CHECK (f->riff.notes.size() == strum.size());

    // The choice is kept.
    Studio studio (false);
    CHECK (studio.getRiffPadStyle() == 0);
    studio.setRiffPadStyle (1);
    CHECK (studio.getRiffPadStyle() == 1);
}
