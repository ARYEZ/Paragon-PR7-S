#include <catch2/catch_test_macros.hpp>

#include "engine/MediaBay.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/MainView.h"
#include "ui/MediaBayPage.h"

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
