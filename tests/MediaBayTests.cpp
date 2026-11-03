#include <catch2/catch_test_macros.hpp>

#include "engine/MediaBay.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/MainView.h"
#include "ui/MediaBayPage.h"
#include "ui/MediaRack.h"
#include "ui/TrackActions.h"

using namespace pr7s;

namespace
{
    // A folder of media to scan: audio (stereo, tempo and key in its name; a mono one in a
    // subfolder), a MIDI file, a track preset, a song and a file that isn't media.
    struct MediaFolder
    {
        juce::File root = juce::File::createTempFile ("pr7s-mediabay");

        MediaFolder()
        {
            root.createDirectory();
            writeWav (root.getChildFile ("Drums_120bpm_Am.wav"), 44100.0, 2, 22050);
            writeWav (root.getChildFile ("Kicks").getChildFile ("Kick 808.wav"), 48000.0, 1, 4800);
            writeMidi (root.getChildFile ("Bass Line 95bpm.mid"));
            root.getChildFile ("Lead Guitar.pr7track").replaceWithText ("<PR7S trackPreset=\"1\"/>");
            root.getChildFile ("My Song.pr7s").replaceWithText ("<PR7S/>");
            root.getChildFile ("notes.txt").replaceWithText ("not media");
        }
        ~MediaFolder() { root.deleteRecursively(); }

        static void writeWav (const juce::File& file, double rate, int channels, int samples)
        {
            file.getParentDirectory().createDirectory();
            juce::AudioBuffer<float> sound (channels, samples);
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < samples; ++i)
                    sound.setSample (c, i, 0.5f * std::sin ((float) i * 0.05f));
            juce::WavAudioFormat wav;
            auto stream = std::make_unique<juce::FileOutputStream> (file);
            stream->setPosition (0);
            stream->truncate();
            std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), rate, (unsigned int) channels, 24, {}, 0));
            REQUIRE (writer != nullptr);
            stream.release();
            writer->writeFromAudioSampleBuffer (sound, 0, samples);
        }

        static void writeMidi (const juce::File& file)
        {
            juce::MidiMessageSequence sequence;
            sequence.addEvent (juce::MidiMessage::tempoMetaEvent (60000000 / 95));
            for (int i = 0; i < 8; ++i)
            {
                sequence.addEvent (juce::MidiMessage::noteOn (1, 36 + i, (juce::uint8) 100), i * 480.0);
                sequence.addEvent (juce::MidiMessage::noteOff (1, 36 + i), i * 480.0 + 240.0);
            }
            sequence.updateMatchedPairs();
            juce::MidiFile midi;
            midi.setTicksPerQuarterNote (480);
            midi.addTrack (sequence);
            juce::FileOutputStream out (file);
            out.setPosition (0);
            out.truncate();
            midi.writeTo (out);
        }
    };

    std::vector<juce::String> names (const MediaDatabase& db, const std::vector<int>& found)
    {
        std::vector<juce::String> out;
        for (const auto i : found)
            out.push_back (db.getItems()[(size_t) i].name);
        return out;
    }

    bool has (const std::vector<juce::String>& list, const juce::String& name)
    {
        return std::find (list.begin(), list.end(), name) != list.end();
    }
}

TEST_CASE ("MediaBay text search: and, or, not, parentheses, phrases (0.21.0)", "[mediabay]")
{
    const juce::String drumloop = "drumloop 01 audio", snareDrum = "snare drum heavy audio", kick808 = "kick 808 audio", hihat = "hi-hat open audio";
    CHECK (TextSearch ("drum").matches (drumloop));
    CHECK (TextSearch ("DRUM").matches (snareDrum));                 // (any case)
    CHECK_FALSE (TextSearch ("drum").matches (kick808));
    CHECK (TextSearch ("snare drum").matches (snareDrum));            // both words
    CHECK (TextSearch ("snare + heavy").matches (snareDrum));
    CHECK (TextSearch ("snare and heavy").matches (snareDrum));
    CHECK_FALSE (TextSearch ("snare kick").matches (snareDrum));
    CHECK (TextSearch ("snare, kick").matches (kick808));             // either
    CHECK (TextSearch ("snare or kick").matches (snareDrum));
    CHECK_FALSE (TextSearch ("-808").matches (kick808));              // not
    CHECK (TextSearch ("not 808").matches (snareDrum));
    CHECK (TextSearch ("(kick or snare) -heavy").matches (kick808));
    CHECK_FALSE (TextSearch ("(kick or snare) -heavy").matches (snareDrum));
    CHECK (TextSearch ("\"snare drum\"").matches (snareDrum));       // a phrase
    CHECK_FALSE (TextSearch ("\"drum snare\"").matches (snareDrum));
    CHECK (TextSearch ("hi-hat").matches (hihat));                    // (a minus inside a word: the word)
    CHECK (TextSearch ("").isEmpty());
    CHECK (TextSearch ("").matches (kick808));
}

TEST_CASE ("MediaBay names: tempo and key read from a file's name (0.21.0)", "[mediabay]")
{
    CHECK (MediaDatabase::bpmFromName ("Drums_120bpm_Am") == 120.0);
    CHECK (MediaDatabase::bpmFromName ("Loop 95 BPM") == 95.0);
    CHECK (MediaDatabase::bpmFromName ("bpm_128 house") == 128.0);
    CHECK (MediaDatabase::bpmFromName ("Kick 808") == 0.0);
    CHECK (MediaDatabase::bpmFromName ("take 7bpm") == 0.0);   // (one digit: not a tempo)
    CHECK (MediaDatabase::keyFromName ("Drums_120bpm_Am") == "Am");
    CHECK (MediaDatabase::keyFromName ("Pad F#m 90") == "F#m");
    CHECK (MediaDatabase::keyFromName ("Bass_Bbmin") == "Bbm");
    CHECK (MediaDatabase::keyFromName ("Chords C maj") == "C");
    CHECK (MediaDatabase::keyFromName ("Bass Line 95bpm") == "");   // ("Bass" isn't B-flat)
    CHECK (MediaDatabase::keyFromName ("kick a 808") == "");        // (a lower-case "a" isn't a key)
    CHECK (MediaDatabase::keyFromName ("Drop A Chug Loop 140bpm") == "");   // (a guitar's tuning, not the key)
    CHECK (MediaDatabase::keyFromName ("Riff Drop C Em") == "Em");
}

TEST_CASE ("MediaBay database: a scanned folder's files, their attributes, filters, ratings, kept (0.21.0)", "[mediabay]")
{
    MediaFolder folder;
    const auto store = folder.root.getSiblingFile (folder.root.getFileName() + "-store.xml");
    {
        MediaDatabase db (store);
        CHECK_FALSE (db.hadStore());
        CHECK (db.addLocation (folder.root));
        CHECK_FALSE (db.addLocation (folder.root));   // (in already)
        db.scanNow();
        REQUIRE (db.getItems().size() == 5);           // (notes.txt isn't media)

        const auto& items = db.getItems();
        const auto itemNamed = [&] (const juce::String& name) -> const MediaItem&
        {
            for (const auto& item : items)
                if (item.name == name)
                    return item;
            FAIL ("no " << name);
            return items.front();
        };
        const auto& drums = itemNamed ("Drums_120bpm_Am");
        CHECK (drums.type == MediaType::audio);
        CHECK (std::abs (drums.seconds - 0.5) < 0.001);
        CHECK (drums.sampleRate == 44100.0);
        CHECK (drums.channels == 2);
        CHECK (drums.bpm == 120.0);
        CHECK (drums.key == "Am");
        const auto& kick = itemNamed ("Kick 808");
        CHECK (kick.channels == 1);
        CHECK (std::abs (kick.seconds - 0.1) < 0.001);
        const auto& bass = itemNamed ("Bass Line 95bpm");
        CHECK (bass.type == MediaType::midi);
        CHECK (bass.notes == 8);
        CHECK (std::abs (bass.beats - 7.5) < 0.01);
        CHECK (std::abs (bass.bpm - 95.0) < 0.5);
        CHECK (itemNamed ("Lead Guitar").type == MediaType::trackPreset);
        CHECK (itemNamed ("My Song").type == MediaType::song);

        // Filters: types, text (name, folder, type, tempo, key), folder and its subfolders.
        MediaQuery q;
        q.types = { MediaType::audio };
        CHECK (db.find (q).size() == 2);
        q.types = { MediaType::midi, MediaType::trackPreset };
        CHECK (db.find (q).size() == 2);
        q = {};
        q.text = "kick";
        CHECK (names (db, db.find (q)) == std::vector<juce::String> { "Kick 808" });
        q.text = "kicks";                                          // (its folder's name)
        CHECK (db.find (q).size() == 1);
        q.text = "120 bpm";
        CHECK (names (db, db.find (q)) == std::vector<juce::String> { "Drums_120bpm_Am" });
        q.text = "midi, preset";
        CHECK (db.find (q).size() == 2);
        q.text = "audio -808";
        CHECK (names (db, db.find (q)) == std::vector<juce::String> { "Drums_120bpm_Am" });
        q = {};
        q.folder = folder.root;
        q.subfolders = false;
        CHECK (db.find (q).size() == 4);                          // (not Kicks/Kick 808)
        q.subfolders = true;
        CHECK (db.find (q).size() == 5);

        // Ratings: filter, kept by path.
        db.setRating (drums.file, 4);
        db.setRating (kick.file, 2);
        q = {};
        q.minimumRating = 3;
        CHECK (names (db, db.find (q)) == std::vector<juce::String> { "Drums_120bpm_Am" });
        db.setRating (kick.file, 9);
        CHECK (db.getRating (kick.file) == 5);                    // (at most five)

        // Sorting and shuffling.
        auto all = db.find ({});
        db.sort (all, MediaColumn::length, false);
        CHECK (db.getItems()[(size_t) all.front()].name == "Bass Line 95bpm");   // 7.5 beats at 95 bpm: the longest
        db.sort (all, MediaColumn::rating, false);
        CHECK (db.getItems()[(size_t) all.front()].name == "Kick 808");
        auto a = db.find ({}), b = db.find ({});
        MediaDatabase::shuffle (a, 7);
        MediaDatabase::shuffle (b, 7);
        CHECK (a == b);                                            // (repeatable by seed)
    }

    // Kept: a new database reads the locations, the files and the ratings back.
    {
        MediaDatabase db (store);
        CHECK (db.hadStore());
        CHECK (db.getLocations().size() == 1);
        REQUIRE (db.getItems().size() == 5);
        CHECK (db.getRating (folder.root.getChildFile ("Drums_120bpm_Am.wav")) == 4);

        // A file changed and one removed: the next scan sees both.
        MediaFolder::writeWav (folder.root.getChildFile ("Drums_120bpm_Am.wav"), 44100.0, 2, 44100);
        folder.root.getChildFile ("My Song.pr7s").deleteFile();
        juce::Thread::sleep (20);
        db.rescan();   // (in the background)
        for (int i = 0; i < 500 && ! db.takeScanResults(); ++i)
            juce::Thread::sleep (10);
        REQUIRE (db.getItems().size() == 4);
        for (const auto& item : db.getItems())
            if (item.name == "Drums_120bpm_Am")
            {
                CHECK (std::abs (item.seconds - 1.0) < 0.001);    // (read again: it changed)
                CHECK (item.rating == 4);                          // (its rating kept)
            }

        // A location removed: its files leave the results.
        db.removeLocation (folder.root);
        CHECK (db.getItems().empty());
        CHECK (db.getRating (folder.root.getChildFile ("Drums_120bpm_Am.wav")) == 4);   // (still known)
    }
    store.deleteFile();
}

TEST_CASE ("MediaBay scan waits while the song plays (the manual's MediaBay doesn't scan then) (0.21.0)", "[mediabay]")
{
    MediaFolder folder;
    MediaDatabase db;
    db.addLocation (folder.root);
    std::atomic<bool> playing { true };
    db.shouldPause = [&] { return playing.load(); };
    db.rescan();
    juce::Thread::sleep (300);
    CHECK_FALSE (db.takeScanResults());   // (still waiting)
    CHECK (db.isScanning());
    playing = false;
    auto done = false;
    for (int i = 0; i < 500 && ! done; ++i)
    {
        done = db.takeScanResults();
        juce::Thread::sleep (10);
    }
    CHECK (done);
    CHECK (db.getItems().size() == 5);
}

TEST_CASE ("MediaBay preview: audio over the output, at its level, looping or not; MIDI through the live track (0.21.0)", "[mediabay][studio]")
{
    MediaFolder folder;
    Studio studio (false);
    const auto file = folder.root.getChildFile ("Drums_120bpm_Am.wav");
    REQUIRE (studio.previewMedia (file, false));
    CHECK (studio.isMediaPreviewing());
    REQUIRE (studio.getMediaPreviewSound() != nullptr);
    const auto rate = studio.getEngine().getSampleRate();
    CHECK (std::abs (studio.getMediaPreviewSound()->sound.getNumSamples() - (int) (0.5 * rate)) < 4);   // (at the engine's rate)

    // Heard while stopped, at the preview level; to its end, then silent.
    studio.setMediaPreviewLevel (0.5f);
    juce::AudioBuffer<float> out (2, 512);
    studio.getEngine().render (out, 512);
    CHECK (out.getMagnitude (0, 0, 512) > 0.1f);
    CHECK (out.getMagnitude (0, 0, 512) < 0.26f);   // (0.5 at 0.5)
    CHECK (studio.getMediaPreviewProgress() > 0.0);
    juce::AudioBuffer<float> rest (2, (int) rate);
    studio.getEngine().render (rest, (int) rate);
    CHECK_FALSE (studio.isMediaPreviewing());
    studio.getEngine().render (out, 512);
    CHECK (out.getMagnitude (0, 0, 512) == 0.0f);

    // Looping: round and round.
    REQUIRE (studio.previewMedia (file, true));
    studio.getEngine().render (rest, (int) rate);
    CHECK (studio.isMediaPreviewing());
    studio.stopMediaPreview();
    CHECK_FALSE (studio.isMediaPreviewing());

    // MIDI: through the live track (the IDEAS previews' way).
    REQUIRE (studio.previewMedia (folder.root.getChildFile ("Bass Line 95bpm.mid"), false));
    CHECK (studio.isMediaPreviewing());
    studio.stopMediaPreview();
    CHECK_FALSE (studio.isMediaPreviewing());
    // Not for presets and songs.
    CHECK_FALSE (studio.previewMedia (folder.root.getChildFile ("Lead Guitar.pr7track"), false));
}

TEST_CASE ("The MediaBay window: F5, filters, ratings, the previewer, double-click and drag into the song (0.21.0)", "[mediabay][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    MediaFolder folder;
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    studio.getMediaBay().addLocation (folder.root);
    studio.getMediaBay().scanNow();

    // F5 opens it, F5 again closes it.
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F5Key)));
    REQUIRE (view.getMediaBayWindow() != nullptr);
    auto& page = view.getMediaBayWindow()->getPage();
    page.setAutoPlay (false);
    page.resetFilters();
    CHECK (page.getNumResults() == 5);
    CHECK (page.getStatusText().startsWith ("5 FILES"));

    // The type keys, the search, the rating filter, a location without its subfolders.
    page.setTypeShown (MediaType::audio, true);
    CHECK (page.getNumResults() == 2);
    page.setSearch ("808");
    CHECK (page.getNumResults() == 1);
    CHECK (page.getStatusText().startsWith ("1 FILE OF 5"));
    page.resetFilters();
    CHECK (page.getNumResults() == 5);
    page.sortBy (MediaColumn::name, true);
    CHECK (page.getResult (0)->name == "Bass Line 95bpm");
    page.setRating (0, 3);
    CHECK (studio.getMediaBay().getRating (page.getResult (0)->file) == 3);
    page.setMinimumRating (3);
    CHECK (page.getNumResults() == 1);
    page.setMinimumRating (0);
    page.selectLocation (0);
    page.setSubfolders (false);
    CHECK (page.getNumResults() == 4);
    page.setSubfolders (true);
    page.selectLocation (-1);

    // The previewer: the selected file played; auto play plays on selection.
    page.selectRow (1);   // (Drums_120bpm_Am, sorted by name)
    REQUIRE (page.getResult (1)->name == "Drums_120bpm_Am");
    CHECK (page.play());
    CHECK (studio.isMediaPreviewing());
    page.stop();
    CHECK_FALSE (studio.isMediaPreviewing());
    page.setAutoPlay (true);
    page.selectRow (2);   // (Kick 808)
    CHECK (studio.isMediaPreviewing());
    page.stop();
    page.setAutoPlay (false);

    // Double-click (USE IT): audio on a new audio track at the playhead; MIDI on a new
    // instrument track; a track preset as a new track.
    const auto tracksBefore = studio.getProject().tracks.size();
    studio.setPosition (4.0);
    page.use (1);
    REQUIRE (studio.getProject().tracks.size() == tracksBefore + 1);
    const auto& audioTrack = studio.getProject().tracks.back();
    CHECK (audioTrack.isAudio());
    REQUIRE (audioTrack.parts.size() == 1);
    CHECK (audioTrack.parts[0].start == 4.0);
    page.use (0);   // (Bass Line 95bpm.mid)
    REQUIRE (studio.getProject().tracks.size() == tracksBefore + 2);
    CHECK (! studio.getProject().tracks.back().isAudio());
    CHECK (studio.getProject().tracks.back().parts.size() == 1);

    // Dragged: the description carries the paths; dropped on the first (instrument) track there.
    const auto description = MediaBayPage::dragDescriptionFor ({ folder.root.getChildFile ("Bass Line 95bpm.mid").getFullPathName() });
    CHECK (MediaBayPage::filesFromDragDescription (description).size() == 1);
    CHECK (MediaBayPage::filesFromDragDescription ("chordpad:3").isEmpty());
    const auto synth = studio.getProject().tracks[0].id;
    auto& arrange = view.getArrangeView();
    const auto at = view.getLocalPoint (&arrange, juce::Point<float> (view.getTimeline().xForBeat (8.0), arrange.trackTop (synth) + 10.0f).toInt());
    const auto partsBefore = studio.getProject().findTrack (synth)->parts.size();
    // (Its 95 bpm differs from the song's: asked, as a dragged-in file is; "keep the song's".)
    auto asked = 0;
    view.askMidiTempo = [&] (const juce::File&, double, std::function<void (int)> answer) { ++asked; answer (2); };   // (2: keep the song's tempo; 0 would cancel)
    view.mediaDropped (MediaBayPage::filesFromDragDescription (description), at.x, at.y);
    CHECK (studio.getProject().findTrack (synth)->parts.size() == partsBefore + 1);
    CHECK (asked == 1);
    CHECK (studio.getProject().tracks.size() == tracksBefore + 2);   // (on that track: no new one)

    // F5 closes it.
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F5Key)));
    CHECK (view.getMediaBayWindow() == nullptr);
}

TEST_CASE ("MediaBay tags: set on several files, linked subs, characters, tempo and key over the name's, kept by path (0.21.1)", "[mediabay]")
{
    using A = MediaAttribute;
    MediaFolder folder;
    const auto store = juce::File::createTempFile ("pr7s-mediabay-tags.xml");
    const auto drums = folder.root.getChildFile ("Drums_120bpm_Am.wav");
    const auto kick = folder.root.getChildFile ("Kicks").getChildFile ("Kick 808.wav");
    const auto bass = folder.root.getChildFile ("Bass Line 95bpm.mid");
    {
        MediaDatabase database (store);
        database.addLocation (folder.root);
        database.scanNow();
        const auto item = [&] (const juce::File& f) -> const MediaItem&
        {
            for (const auto& i : database.getItems())
                if (i.file == f)
                    return i;
            FAIL ("not found");
            return database.getItems().front();
        };

        // Two files at once (the manual's Editing Attributes of Multiple Files).
        database.setAttribute ({ drums, kick }, { A::category, {} }, "Drums");
        database.setAttribute ({ kick }, { A::subCategory, {} }, "Kick");
        CHECK (item (drums).tags.category == "Drums");
        CHECK (item (kick).tags.subCategory == "Kick");
        // A new category drops a sub category that isn't one of its own.
        database.setAttribute ({ kick }, { A::category, {} }, "Bass");
        CHECK (item (kick).tags.subCategory.isEmpty());
        database.setAttribute ({ kick }, { A::category, {} }, "Drums");
        database.setAttribute ({ kick }, { A::subCategory, {} }, "Kick");
        // Removing a category takes its sub with it.
        database.removeAttribute ({ drums }, { A::category, {} });
        CHECK (item (drums).tags.isEmpty());

        // Character: one side of a pair; the other side goes; in the pairs' order.
        database.setCharacter ({ drums }, 2, 1);                              // Bright
        database.setAttribute ({ drums }, { A::character, {} }, "Distorted");
        CHECK (item (drums).tags.character.joinIntoString (",") == "Distorted,Bright");
        database.setCharacter ({ drums }, 2, -1);                             // Dark instead
        CHECK (item (drums).tags.character.joinIntoString (",") == "Distorted,Dark");
        database.setCharacter ({ drums }, 0, 0);                              // neither Clean nor Distorted
        CHECK (item (drums).tags.character.joinIntoString (",") == "Dark");

        // Your tempo and key win over the name's; removed, the name's is back.
        CHECK (item (drums).tempo() == 120.0);
        database.setAttribute ({ drums }, { A::tempo, {} }, "140");
        database.setAttribute ({ drums }, { A::key, {} }, "F#m");
        CHECK (item (drums).tempo() == 140.0);
        CHECK (item (drums).keyName() == "F#m");
        CHECK (MediaDatabase::attributeValues (item (drums), { A::tempo, {} }) == juce::StringArray ("140"));
        database.setAttribute ({ drums }, { A::tempo, {} }, "5");             // (not a tempo: left)
        CHECK (item (drums).tempo() == 140.0);
        database.removeAttribute ({ drums }, { A::key, {} });
        CHECK (item (drums).keyName() == "Am");

        // Rating and comment through the same calls; the search finds tags too.
        database.setAttribute ({ drums, bass }, { A::rating, {} }, "4");
        CHECK (item (bass).rating == 4);
        database.setAttribute ({ bass }, { A::comment, {} }, "verse groove");
        database.setAttribute ({ bass }, { A::style, {} }, "Metal");
        database.setAttribute ({ bass }, { A::subStyle, {} }, "Djent");
        MediaQuery query;
        query.text = "djent groove";
        CHECK (database.find (query).size() == 1);
        query.text = "kick";
        CHECK (database.find (query).size() == 1);   // (the name "Kick 808" and its sub category: one file)

        // Your own values (the manual's Adding User Attribute Values).
        CHECK (database.addCategory ("Guitar", "Seven String"));
        CHECK_FALSE (database.addCategory ("guitar", "Seven String"));   // (already there)
        CHECK (database.addStyle ("Gothic", ""));
        CHECK (database.subsOf ({ A::category, {} }, "Guitar").contains ("Seven String"));
        CHECK (database.subsOf ({ A::category, {} }, "Guitar").contains ("Riff"));

        // User attributes (the manual's Defining User Attributes).
        CHECK (database.addUserAttribute ({ "Session", UserAttributeType::text, 0 }));
        CHECK (database.addUserAttribute ({ "Strings", UserAttributeType::number, 0 }));
        CHECK (database.addUserAttribute ({ "Used", UserAttributeType::yesNo, 0 }));
        CHECK_FALSE (database.addUserAttribute ({ "session", UserAttributeType::text, 0 }));
        CHECK_FALSE (database.addUserAttribute ({ "Category", UserAttributeType::text, 0 }));
        database.setAttribute ({ kick }, { A::user, "Strings" }, "7.4");
        database.setAttribute ({ kick }, { A::user, "Used" }, "yes");
        database.setAttribute ({ kick }, { A::user, "Session" }, "Album demos");
        CHECK (item (kick).tags.user.at ("Strings") == "7");
        CHECK (item (kick).tags.user.at ("Used") == "1");
    }

    // Kept: reopened from the store (and kept for a file the scan no longer sees).
    {
        MediaDatabase database (store);
        const auto tags = database.getTags (kick);
        CHECK (tags.category == "Drums");
        CHECK (tags.subCategory == "Kick");
        CHECK (tags.user.at ("Session") == "Album demos");
        CHECK (database.getTags (drums).bpm == 140.0);
        CHECK (database.getTags (drums).character.joinIntoString (",") == "Dark");
        CHECK (database.getTags (bass).subStyle == "Djent");
        CHECK (database.getUserAttributes().size() == 3);
        CHECK (database.findUserAttribute ("used")->type == UserAttributeType::yesNo);
        CHECK (database.subsOf ({ A::category, {} }, "Guitar").contains ("Seven String"));
        bool gothic = false;
        for (const auto& style : database.getStyles())
            gothic = gothic || style.name == "Gothic";
        CHECK (gothic);
        // A user attribute deleted: its values go from every file.
        database.removeUserAttribute ("Session");
        CHECK (database.getTags (kick).user.count ("Session") == 0);
        CHECK (database.getTags (kick).user.count ("Strings") == 1);
    }
    store.deleteFile();
}

TEST_CASE ("MediaBay attribute filter: values with counts, either within a column, Character all, linked subs (0.21.1)", "[mediabay]")
{
    using A = MediaAttribute;
    MediaFolder folder;
    MediaDatabase database;
    database.addLocation (folder.root);
    database.scanNow();
    const auto drums = folder.root.getChildFile ("Drums_120bpm_Am.wav");
    const auto kick = folder.root.getChildFile ("Kicks").getChildFile ("Kick 808.wav");
    const auto bass = folder.root.getChildFile ("Bass Line 95bpm.mid");
    database.setAttribute ({ drums, kick }, { A::category, {} }, "Drums");
    database.setAttribute ({ drums }, { A::subCategory, {} }, "Kit");
    database.setAttribute ({ kick }, { A::subCategory, {} }, "Kick");
    database.setAttribute ({ bass }, { A::category, {} }, "Bass");
    database.setAttribute ({ bass }, { A::subCategory, {} }, "Synth");
    database.setCharacter ({ drums, kick }, 0, 1);    // Distorted
    database.setCharacter ({ drums }, 2, -1);          // Dark

    MediaQuery query;
    query.attributes = { { { A::category, {} }, {} }, { { A::subCategory, {} }, {} }, { { A::character, {} }, {} } };
    using Counts = std::vector<std::pair<juce::String, int>>;
    CHECK (database.valueCounts (query, { A::category, {} }) == Counts { { "Bass", 1 }, { "Drums", 2 } });
    CHECK (database.valueCounts (query, { A::subCategory, {} }) == Counts { { "Kick", 1 }, { "Kit", 1 }, { "Synth", 1 } });

    // A category chosen: only its files, and the sub category column shows only its subs
    // (the manual: "some attributes are linked"); its own column still shows every category.
    query.attributes[0].values = { "Drums" };
    CHECK (database.find (query).size() == 2);
    CHECK (database.valueCounts (query, { A::subCategory, {} }) == Counts { { "Kick", 1 }, { "Kit", 1 } });
    CHECK (database.valueCounts (query, { A::category, {} }).size() == 2);
    // Either of two values (Ctrl-click in the manual).
    query.attributes[0].values = { "Drums", "Bass" };
    CHECK (database.find (query).size() == 3);
    // Character values always all (the manual's AND).
    query.attributes[0].values = {};
    query.attributes[2].values = { "Distorted" };
    CHECK (database.find (query).size() == 2);
    query.attributes[2].values = { "Distorted", "Dark" };
    CHECK (database.find (query).size() == 1);
    // Tempo and key: the name's or yours.
    query.attributes[2].values = {};
    CHECK (database.valueCounts (query, { A::tempo, {} }) == Counts { { "95", 1 }, { "120", 1 } });
    CHECK (database.valueCounts (query, { A::key, {} }) == Counts { { "Am", 1 } });
    // Sorting by category, then sub category.
    auto all = database.find (MediaQuery {});
    database.sort (all, MediaColumn::category, true);
    CHECK (database.getItems()[(size_t) all[all.size() - 1]].file == drums);   // (Drums / Kit after Drums / Kick)
    CHECK (database.getItems()[(size_t) all[all.size() - 2]].file == kick);
}

TEST_CASE ("The MediaBay window's attribute inspector and filter: several files at once, the filter's clicks (0.21.1)", "[mediabay][ui]")
{
    using A = MediaAttribute;
    Finish::set (Finish::Type::pearlWhite, false);
    MediaFolder folder;
    Studio studio (false);
    studio.getMediaBay().addLocation (folder.root);
    studio.getMediaBay().scanNow();
    MediaBayPage page (studio);
    page.setAutoPlay (false);
    page.resetFilters();
    page.sortBy (MediaColumn::name, true);
    REQUIRE (page.getNumResults() == 5);
    REQUIRE (page.getResult (1)->name == "Drums_120bpm_Am");
    REQUIRE (page.getResult (2)->name == "Kick 808");
    CHECK (page.isInspectorShown());
    CHECK (page.areFiltersShown());

    // Two files selected: the inspector sets both; their differing values read (MIXED).
    page.selectRows ({ 1, 2 });
    CHECK (page.getSelectedFiles().size() == 2);
    page.setAttribute ({ A::category, {} }, "Drums");
    CHECK (page.getAttributeText ({ A::category, {} }) == "Drums");
    CHECK (page.getAttributeText ({ A::tempo, {} }) == "(MIXED)");   // (120 from a name, none)
    page.setCharacter (0, 1);
    CHECK (page.getAttributeText ({ A::character, {} }) == "Distorted");
    // The selection survives an edit (and AUTO PLAY doesn't start again: it's off here anyway).
    CHECK (page.getSelectedFiles().size() == 2);

    page.selectRows ({ 2 });
    page.setAttribute ({ A::subCategory, {} }, "Kick");
    page.addListValue (true, "metal", "Djent");   // (the list's spelling: Metal)
    CHECK (page.getAttributeText ({ A::style, {} }) == "Metal");
    CHECK (page.getAttributeText ({ A::subStyle, {} }) == "Djent");
    page.removeAttribute ({ A::subStyle, {} });
    CHECK (page.getAttributeText ({ A::subStyle, {} }).isEmpty());

    // DYNAMIC: only the attributes it has; DEFINED: all of them.
    page.setInspectorDynamic (true);
    auto rows = page.getInspectorRows();
    CHECK (rows.contains ("CATEGORY"));
    CHECK (rows.contains ("SUB CATEGORY"));
    CHECK_FALSE (rows.contains ("COMMENT"));
    page.setInspectorDynamic (false);
    CHECK (page.getInspectorRows().contains ("COMMENT"));
    CHECK (page.addUserAttribute ({ "Session", UserAttributeType::text, 0 }));
    CHECK (page.getInspectorRows().contains ("SESSION"));

    // The filter: CATEGORY, SUB CATEGORY, STYLE, CHARACTER at first; a click chooses one value,
    // a second click none; Ctrl-click another.
    CHECK (page.getFilterColumn (0) == AttributeId { A::category, {} });
    CHECK (page.getFilterColumn (3) == AttributeId { A::character, {} });
    using Counts = std::vector<std::pair<juce::String, int>>;
    CHECK (page.getFilterValues (0) == Counts { { "Drums", 2 } });
    page.clickFilterValue (0, "Drums", false);
    CHECK (page.getNumResults() == 2);
    page.clickFilterValue (1, "Kick", false);
    CHECK (page.getNumResults() == 1);
    page.clickFilterValue (1, "Kick", false);
    CHECK (page.getNumResults() == 2);
    page.clickFilterValue (0, "Drums", false);
    CHECK (page.getNumResults() == 5);
    // A column on another attribute (a user attribute too).
    page.setFilterColumn (2, { A::user, "Session" });
    page.selectRows ({ 0 });
    page.setAttribute ({ A::user, "Session" }, "Demos");
    CHECK (page.getFilterValues (2) == Counts { { "Demos", 1 } });
    page.clickFilterValue (2, "Demos", false);
    CHECK (page.getNumResults() == 1);
    // Deleting the attribute puts the column back on CATEGORY, its filter gone.
    page.deleteUserAttribute ("Session");
    CHECK (page.getFilterColumn (2) == AttributeId { A::category, {} });
    CHECK (page.getNumResults() == 5);
    // RESET clears the filter's choices.
    page.clickFilterValue (3, "Distorted", false);
    CHECK (page.getNumResults() == 2);
    page.resetFilters();
    CHECK (page.getNumResults() == 5);

    // FILTERS and INSPECTOR hide them (the results get the room).
    page.setSize (1600, 900);
    const auto before = page.getWidth();
    page.setInspectorShown (false);
    page.setFiltersShown (false);
    CHECK_FALSE (page.isInspectorShown());
    CHECK (page.getWidth() == before);
}

TEST_CASE ("The Media rack: home tiles, pages, search, types, filter, favorites, the file browser (0.22.0)", "[mediabay][ui]")
{
    using A = MediaAttribute;
    using Page = MediaRack::Page;
    Finish::set (Finish::Type::pearlWhite, false);
    MediaFolder folder;
    Studio studio (false);
    studio.getMediaBay().addLocation (folder.root);
    studio.getMediaBay().scanNow();
    MediaRack rack (studio);
    rack.setSize (360, 900);
    rack.setAutoPlay (false);

    // HOME: the manual's tiles (ours: no User Presets, one preset folder).
    CHECK (rack.getPage() == Page::home);
    CHECK (rack.getTileNames() == juce::StringArray { "VST INSTRUMENTS", "VST EFFECTS", "LOOPS & SAMPLES", "PRESETS", "FAVORITES", "FILE BROWSER" });
    CHECK (rack.getAddress() == "HOME");

    // VST INSTRUMENTS: the studio's synth first; a double-click makes an instrument track with it.
    rack.clickTile (0);
    CHECK (rack.getPage() == Page::instruments);
    CHECK (rack.getAddress() == "HOME  /  VST INSTRUMENTS");
    const auto synth = rack.findRow ("Paragon Synth");
    REQUIRE (synth >= 0);
    CHECK (rack.getRow (synth)->studioSynth);
    CHECK (rack.getDragDescription (synth) == "mediarack:instrument:studio");
    CHECK (MediaRack::instrumentFor (studio, "mediarack:instrument:studio").has_value());
    const auto tracks = studio.getProject().tracks.size();
    rack.use (synth);
    CHECK (studio.getProject().tracks.size() == tracks + 1);
    CHECK (studio.getProject().tracks.back().kind == Track::Kind::instrument);
    rack.setSearch ("nothing like it");
    CHECK (rack.findRow ("Paragon Synth") < 0);
    rack.setSearch ({});

    // BACK and HOME.
    rack.back();
    CHECK (rack.getPage() == Page::home);
    rack.openPage (Page::loops);

    // LOOPS & SAMPLES: the audio and MIDI files; the type keys narrow them; search; shuffle.
    CHECK (rack.getNumRows() == 3);
    CHECK (rack.getDragDescription (0).startsWith ("mediabay:"));
    rack.setTypeShown (MediaType::audio, true);
    CHECK (rack.getNumRows() == 2);
    rack.setTypeShown (MediaType::audio, false);
    rack.setSearch ("808");
    CHECK (rack.getNumRows() == 1);
    rack.setSearch ({});
    rack.shuffle (7);
    CHECK (rack.getNumRows() == 3);
    // The attribute filter (two columns: CATEGORY, STYLE at first); CLEAR lights while one's set.
    studio.getMediaBay().setAttribute ({ folder.root.getChildFile ("Drums_120bpm_Am.wav") }, { A::category, {} }, "Drums");
    rack.refresh();
    rack.setFiltersShown (true);
    CHECK (rack.getFilterColumn (0) == AttributeId { A::category, {} });
    using Counts = std::vector<std::pair<juce::String, int>>;
    CHECK (rack.getFilterValues (0) == Counts { { "Drums", 1 } });
    rack.clickFilterValue (0, "Drums", false);
    CHECK (rack.isAttributeFilterSet());
    CHECK (rack.getNumRows() == 1);
    rack.resetAttributeFilter();
    CHECK (rack.getNumRows() == 3);

    // PRESETS: the track presets; ALL: every file.
    rack.openPage (Page::presets);
    CHECK (rack.getNumRows() == 1);
    rack.openPage (Page::results);
    CHECK (rack.getNumRows() == 5);
    // A search on HOME looks through every file.
    rack.home();
    rack.setSearch ("bass");
    CHECK (rack.getPage() == Page::results);
    CHECK (rack.getNumRows() == 1);
    rack.setSearch ({});

    // FAVORITES: a folder (already in a location: not added again), its files as results.
    rack.openPage (Page::favorites);
    CHECK (rack.getTileNames().isEmpty());
    CHECK (rack.addFavorite (folder.root.getChildFile ("Kicks")));
    CHECK_FALSE (rack.addFavorite (folder.root.getChildFile ("Kicks")));
    CHECK (studio.getMediaBay().getLocations().size() == 1);
    CHECK (rack.getTileNames() == juce::StringArray { "KICKS" });
    rack.clickTile (0);
    CHECK (rack.getPage() == Page::favorites);
    CHECK (rack.getAddress().endsWith ("KICKS"));
    CHECK (rack.getNumRows() == 1);
    CHECK (rack.getRow (0)->text == "Kick 808");

    // FILE BROWSER: its roots (the favorites first), then a folder's folders and media files.
    rack.openFolder ({});
    CHECK (rack.getPage() == Page::fileBrowser);
    CHECK (rack.findRow ("FAVORITES") == 0);
    CHECK (rack.findRow ("Kicks") == 1);
    rack.openFolder (folder.root);
    REQUIRE (rack.getRow (0)->kind == MediaRack::Row::Kind::up);
    CHECK (rack.findRow ("Kicks") == 1);
    CHECK (rack.findRow ("Drums_120bpm_Am") > 1);
    CHECK (rack.findRow ("Lead Guitar") > 1);
    CHECK (rack.findRow ("notes") < 0);   // (not media)
    rack.use (rack.findRow ("Kicks"));
    CHECK (rack.getFolder() == folder.root.getChildFile ("Kicks"));
    CHECK (rack.findRow ("Kick 808") >= 0);
    rack.use (0);   // (up)
    CHECK (rack.getFolder() == folder.root);
    rack.back();
    CHECK (rack.getFolder() == folder.root.getChildFile ("Kicks"));
    rack.openPage (Page::favorites);
    rack.removeFavorite (0);
    CHECK (rack.getFavorites().isEmpty());
}

TEST_CASE ("The Media rack in the right zone: Ctrl+Alt+R, drops of instruments and track presets (0.22.0)", "[mediabay][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    MediaFolder folder;
    Studio studio (false);
    MainView view (studio);
    view.setSize (1600, 900);
    studio.getMediaBay().addLocation (folder.root);
    studio.getMediaBay().scanNow();

    CHECK_FALSE (view.isRightZoneShown());
    CHECK (view.keyPressed (juce::KeyPress ('r', juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier, 0)));
    CHECK (view.isRightZoneShown());
    REQUIRE (view.getMediaRack() != nullptr);
    CHECK (view.getMediaRack()->isVisible());
    CHECK (view.getMediaRack()->getWidth() == MediaRack::defaultWidth);
    CHECK (view.getMediaRack()->getRight() <= view.getWidth());
    view.setRightZoneWidth (5000);
    CHECK (view.getRightZoneWidth() == MediaRack::maximumWidth);
    view.setRightZoneWidth (MediaRack::defaultWidth);

    // An instrument dropped: a new instrument track.
    const auto tracks = studio.getProject().tracks.size();
    CHECK (view.rackDropped ("mediarack:instrument:studio", view.getWidth() / 2, 400));
    CHECK (studio.getProject().tracks.size() == tracks + 1);
    CHECK_FALSE (view.rackDropped ("mediabay:/x.wav", 0, 0));

    // A track preset dropped on an instrument track is applied to it (no new track); dropped
    // where there's no track, a new track.
    const auto target = studio.getProject().tracks.front().id;
    studio.setVolume (target, -6.0f);
    const auto preset = folder.root.getChildFile ("Quiet.pr7track");
    REQUIRE (TrackActions::savePresetTo (studio, target, preset));
    studio.setVolume (target, 0.0f);
    const auto x = view.getWidth() / 2;
    int y = -1;
    for (int yy = 0; yy < view.getHeight() && y < 0; ++yy)
        if (view.trackUnder (x, yy) == target)
            y = yy;
    REQUIRE (y >= 0);
    const auto before = studio.getProject().tracks.size();
    view.mediaDropped ({ preset.getFullPathName() }, x, y);
    CHECK (studio.getProject().tracks.size() == before);
    CHECK (std::abs (studio.getProject().findTrack (target)->volumeDb - (-6.0f)) < 0.01f);
    view.mediaDropped ({ preset.getFullPathName() }, x, view.getHeight() - 5);
    CHECK (studio.getProject().tracks.size() == before + 1);

    // Ctrl+Alt+R again hides it.
    CHECK (view.keyPressed (juce::KeyPress ('r', juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier, 0)));
    CHECK_FALSE (view.isRightZoneShown());
}

TEST_CASE ("MediaBay folders scanned or left out: a tick, a square, an empty box (0.22.1)", "[mediabay]")
{
    using State = MediaDatabase::ScanState;
    MediaFolder folder;
    const auto kicks = folder.root.getChildFile ("Kicks");
    const auto store = juce::File::createTempFile ("pr7s-mediabay-scan.xml");
    {
        MediaDatabase db (store);
        db.setIncluded (folder.root, true);
        db.scanNow();
        CHECK (db.getItems().size() == 5);
        CHECK (db.scanState (folder.root) == State::all);
        CHECK (db.scanState (kicks) == State::all);

        // A subfolder left out: its files go at once, the folder above gets a square.
        db.setIncluded (kicks, false);
        CHECK (db.getItems().size() == 4);
        CHECK (db.scanState (folder.root) == State::some);
        CHECK (db.scanState (kicks) == State::none);
        CHECK_FALSE (db.isIncluded (kicks.getChildFile ("Kick 808.wav")));
        CHECK (db.isIncluded (folder.root.getChildFile ("Drums_120bpm_Am.wav")));
        db.scanNow();
        CHECK (db.getItems().size() == 4);   // (a scan skips it)
    }
    {
        // Kept: the left-out folder too.
        MediaDatabase db (store);
        CHECK (db.getExclusions().size() == 1);
        CHECK (db.scanState (folder.root) == State::some);
        // Ticked again: whole again, nothing left out.
        db.setIncluded (kicks, true);
        CHECK (db.getExclusions().isEmpty());
        CHECK (db.getLocations().size() == 1);
        CHECK (db.scanState (folder.root) == State::all);
        db.scanNow();
        CHECK (db.getItems().size() == 5);
        // The whole folder left out: what was decided inside it goes too.
        db.setIncluded (kicks, false);
        db.setIncluded (folder.root, false);
        CHECK (db.getLocations().isEmpty());
        CHECK (db.getExclusions().isEmpty());
        CHECK (db.getItems().empty());
    }
    {
        // Only a subfolder scanned: the folder above has a square, its own files aren't scanned.
        MediaDatabase db;
        db.setIncluded (kicks, true);
        db.scanNow();
        CHECK (db.getItems().size() == 1);
        CHECK (db.scanState (folder.root) == State::some);
        CHECK (db.scanState (kicks) == State::all);
        CHECK_FALSE (db.isIncluded (folder.root.getChildFile ("Drums_120bpm_Am.wav")));
    }
    store.deleteFile();
}

TEST_CASE ("The MediaBay window's file browser: the tree, its boxes, a folder's files, favorites (0.22.1)", "[mediabay][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    MediaFolder folder;
    const auto kicks = folder.root.getChildFile ("Kicks");
    Studio studio (false);
    studio.getMediaBay().addLocation (folder.root);
    studio.getMediaBay().scanNow();
    MediaBayPage page (studio);
    page.setAutoPlay (false);
    page.resetFilters();
    REQUIRE (page.getNumResults() == 5);

    // The tree: the scanned folder (under SCANNED FOLDERS), opened: its subfolder.
    CHECK (page.revealFolder (folder.root));
    CHECK (page.getFolderChildren (folder.root) == juce::StringArray { "Kicks" });
    CHECK (page.revealFolder (kicks));
    CHECK_FALSE (page.revealFolder (juce::File ("/no/such/folder/here")));

    // A click on a folder: its files (with SUBFOLDERS, its subfolders' too).
    page.selectFolder (kicks);
    CHECK (page.getNumResults() == 1);
    page.selectFolder (folder.root);
    CHECK (page.getNumResults() == 5);
    page.setSubfolders (false);
    CHECK (page.getNumResults() == 4);
    page.setSubfolders (true);

    // Its box: left out, its files go; the status says so when it's the one chosen.
    page.setFolderScanned (kicks, false);
    CHECK (page.getNumResults() == 4);
    page.selectFolder (kicks);
    CHECK (page.getNumResults() == 0);
    CHECK (page.getStatusText().contains ("ISN'T SCANNED"));
    page.setFolderScanned (kicks, true);
    studio.getMediaBay().scanNow();
    page.refresh();
    CHECK (page.getNumResults() == 1);
    page.selectFolder ({});
    CHECK (page.getNumResults() == 5);

    // A favorite (the Media rack's list too), scanned if it wasn't.
    CHECK (page.addFavorite (kicks));
    CHECK_FALSE (page.addFavorite (kicks));
    CHECK (page.getFavorites() == juce::StringArray { kicks.getFullPathName() });

    // ADD FOLDER... (as the chooser's answer): a folder of its own, chosen.
    page.removeSelectedLocation();   // (none chosen: nothing)
    CHECK (studio.getMediaBay().getLocations().size() == 1);
}
