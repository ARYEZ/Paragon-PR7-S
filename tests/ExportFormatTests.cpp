// (0.200.0, the owner asked: "add FLAC and mp3 to the export options ... build LAME in") EXPORT's file
// types: FLAC (lossless, JUCE's encoder) and MP3 (LAME, built in), each with its own options, read back.

#include <string_view>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Mp3Writer.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/Windows.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    // A sine on each side, its phase from the song's own sample clock (the same sound every render).
    struct SineTone final : TestPlugin
    {
        SineTone (double hz, float leftLevel, float rightLevel)
            : TestPlugin (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)), frequency (hz), left (leftLevel), right (rightLevel)
        {
            instrument = true;
            name = "Sine Tone";
        }
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
        {
            juce::int64 at = counter;
            if (auto* head = getPlayHead())
                if (const auto position = head->getPosition())
                    if (const auto samples = position->getTimeInSamples())
                        at = *samples;
            const auto rate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const auto v = (float) std::sin (juce::MathConstants<double>::twoPi * frequency * (double) (at + i) / rate);
                buffer.setSample (0, i, left * v);
                buffer.setSample (1, i, right * v);
            }
            counter = at + buffer.getNumSamples();
        }
        double frequency;
        float left, right;
        juce::int64 counter = 0;
    };

    struct Sound
    {
        juce::AudioBuffer<float> audio;
        double rate = 0.0;
        int bits = 0;
    };

    Sound readBack (const juce::File& file)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        Sound sound;
        if (reader == nullptr)
            return sound;
        sound.rate = reader->sampleRate;
        sound.bits = (int) reader->bitsPerSample;
        sound.audio.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&sound.audio, 0, (int) reader->lengthInSamples, 0, true, true);
        return sound;
    }

    // Zero crossings going up, over [from, to) seconds.
    double frequencyOf (const Sound& sound, int channel, double from, double to)
    {
        const auto* data = sound.audio.getReadPointer (channel);
        const auto a = (int) (from * sound.rate), b = juce::jmin (sound.audio.getNumSamples(), (int) (to * sound.rate));
        int first = -1, last = -1, count = 0;
        for (int i = a + 1; i < b; ++i)
            if (data[i - 1] < 0.0f && data[i] >= 0.0f)
            {
                if (first < 0)
                    first = i;
                last = i;
                ++count;
            }
        return count > 1 ? (count - 1) * sound.rate / (double) (last - first) : 0.0;
    }

    float peakOf (const Sound& sound, int channel, double from, double to)
    {
        return sound.audio.getMagnitude (channel, (int) (from * sound.rate), (int) ((to - from) * sound.rate));
    }

    struct Song
    {
        Studio studio { false };
        juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-export-formats");
        double rate = 0.0;
        Song()
        {
            folder.deleteRecursively();
            folder.createDirectory();
            const auto id = studio.getProject().tracks[0].id;
            studio.getEngine().setInstrument (id, std::make_unique<SineTone> (440.0, 0.5f, 0.25f));
            rate = studio.getEngine().getSampleRate();
        }
        ~Song() { folder.deleteRecursively(); }
        Studio::ExportOptions eightBeats() const   // 4 seconds at the song's 120 BPM
        {
            Studio::ExportOptions o;
            o.start = 0.0;
            o.end = 8.0;
            o.tailSeconds = 0.0;
            return o;
        }
        juce::File write (const juce::String& name, const Studio::ExportOptions& o)
        {
            const auto file = folder.getChildFile (name);
            juce::String error;
            const auto ok = studio.exportMix (file, o, error);
            INFO (error);
            REQUIRE (ok);
            return file;
        }
    };
}

TEST_CASE ("0.200.0: FLAC is the 24-bit WAV, sample for sample, and smaller", "[export][formats]")
{
    Song song;
    REQUIRE (song.rate > 0.0);
    auto o = song.eightBeats();
    o.bitsPerSample = 24;
    const auto wav = song.write ("mix.wav", o);
    o.fileType = Studio::ExportOptions::FileType::flac;
    o.flacCompression = 8;
    const auto flac = song.write ("mix.flac", o);
    const auto a = readBack (wav), b = readBack (flac);
    REQUIRE (b.audio.getNumChannels() == 2);
    CHECK (b.bits == 24);
    CHECK (b.rate == song.rate);
    REQUIRE (a.audio.getNumSamples() == b.audio.getNumSamples());
    CHECK (b.audio.getNumSamples() == (int) std::llround (4.0 * song.rate));
    auto differ = 0;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < a.audio.getNumSamples(); ++i)
            if (a.audio.getSample (c, i) != b.audio.getSample (c, i))
                ++differ;
    CHECK (differ == 0);
    CHECK (flac.getSize() < wav.getSize() * 3 / 4);
    CHECK_THAT (peakOf (b, 0, 1.0, 3.0), WithinAbs (0.5, 0.01));
    CHECK_THAT (peakOf (b, 1, 1.0, 3.0), WithinAbs (0.25, 0.01));

    // 32-bit float asked for: FLAC holds whole numbers, so 24-bit; 16-bit as asked; mono one channel.
    o.bitsPerSample = 32;
    CHECK (readBack (song.write ("float.flac", o)).bits == 24);
    o.bitsPerSample = 16;
    o.mono = true;
    const auto mono = readBack (song.write ("mono.flac", o));
    CHECK (mono.bits == 16);
    CHECK (mono.audio.getNumChannels() == 1);
    CHECK_THAT (peakOf (mono, 0, 1.0, 3.0), WithinAbs (0.375, 0.01));
}

TEST_CASE ("0.200.0: MP3 through LAME: the tone, its level, the length, mono, bit rates, the ID3 tag", "[export][formats][mp3]")
{
    Song song;
    auto o = song.eightBeats();
    o.fileType = Studio::ExportOptions::FileType::mp3;
    o.mp3.bitRate = 320;
    o.mp3.highQuality = false;
    o.mp3.sampleRate = 0;
    o.mp3.id3 = true;
    o.mp3.title = "Ten Ton Test";
    o.mp3.artist = "Paragon";
    o.mp3.genre = "Metal";
    o.mp3.year = "2026";
    o.mp3.track = "3";
    const auto file = song.write ("mix.mp3", o);
    const auto sound = readBack (file);
    REQUIRE (sound.audio.getNumChannels() == 2);
    CHECK (sound.rate == song.rate);
    // The length: 4 seconds, give or take the encoder's start delay and its last frame's padding.
    const auto expected = (int) std::llround (4.0 * song.rate);
    INFO ("MP3 samples " << sound.audio.getNumSamples() << ", expected " << expected);
    CHECK (std::abs (sound.audio.getNumSamples() - expected) <= 3 * 1152 + 1200);
    CHECK_THAT (frequencyOf (sound, 0, 1.0, 3.0), WithinAbs (440.0, 1.0));
    CHECK_THAT (frequencyOf (sound, 1, 1.0, 3.0), WithinAbs (440.0, 1.0));
    CHECK_THAT (peakOf (sound, 0, 1.0, 3.0), WithinAbs (0.5, 0.03));
    CHECK_THAT (peakOf (sound, 1, 1.0, 3.0), WithinAbs (0.25, 0.03));
    // 320 kbps for 4 seconds: about 160 kB (plus the tags).
    CHECK (std::abs ((double) file.getSize() - 320000.0 / 8.0 * 4.0) < 12000.0);

    // The tags: ID3 v2 at the start, v1 ("TAG") at the end; the Info tag (LAME's) in the first frame.
    juce::MemoryBlock bytes;
    REQUIRE (file.loadFileAsData (bytes));
    CHECK (std::memcmp (bytes.getData(), "ID3", 3) == 0);
    CHECK (std::memcmp ((const char*) bytes.getData() + bytes.getSize() - 128, "TAG", 3) == 0);
    CHECK (std::string ((const char*) bytes.getData() + bytes.getSize() - 125, 12) == "Ten Ton Test");
    const auto* begin = (const char*) bytes.getData();
    const auto* end = begin + bytes.getSize();
    // (Each text searched for kept in one string: "Info", "Info" + 4 can be two different copies of the
    // literal on MSVC, and the search then ran over neither. Windows failed on it.)
    const auto contains = [begin, end] (std::string_view text)
    {
        return std::search (begin, end, text.begin(), text.end()) != end;
    };
    CHECK (contains ("Info"));
    CHECK (contains ("Paragon"));

    // Bit rates: the files' sizes in proportion.
    o.mp3.id3 = false;
    o.mp3.bitRate = 128;
    const auto small = song.write ("128.mp3", o);
    o.mp3.bitRate = 64;
    const auto smaller = song.write ("64.mp3", o);
    o.mp3.bitRate = 256;
    const auto bigger = song.write ("256.mp3", o);
    CHECK (std::abs ((double) small.getSize() - 128000.0 / 8.0 * 4.0) < 6000.0);
    CHECK (std::abs ((double) smaller.getSize() - 64000.0 / 8.0 * 4.0) < 4000.0);
    CHECK (std::abs ((double) bigger.getSize() - 256000.0 / 8.0 * 4.0) < 9000.0);
    CHECK_THAT (frequencyOf (readBack (smaller), 0, 1.0, 3.0), WithinAbs (440.0, 1.0));

    // Mono: one channel, the two sides averaged.
    o.mp3.bitRate = 192;
    o.mono = true;
    const auto mono = readBack (song.write ("mono.mp3", o));
    REQUIRE (mono.audio.getNumChannels() == 1);
    CHECK_THAT (peakOf (mono, 0, 1.0, 3.0), WithinAbs (0.375, 0.03));
    CHECK_THAT (frequencyOf (mono, 0, 1.0, 3.0), WithinAbs (440.0, 1.0));

    // A sample rate of its own: 22.05 kHz (LAME resamples); high quality: LAME picks one for the bit rate.
    o.mono = false;
    o.mp3.sampleRate = 22050;
    const auto low = readBack (song.write ("22k.mp3", o));
    CHECK (low.rate == 22050.0);
    CHECK_THAT (frequencyOf (low, 0, 1.0, 3.0), WithinAbs (440.0, 1.0));
    o.mp3.highQuality = true;
    o.mp3.bitRate = 32;
    const auto chosen = readBack (song.write ("hq32.mp3", o));
    CHECK (chosen.rate > 0.0);
    CHECK (chosen.rate <= 24000.0);   // 32 kbps: LAME goes down to an MPEG-2 rate
}

TEST_CASE ("0.200.0: stems as FLAC and MP3 take the type's extension", "[export][formats][stems]")
{
    Song song;
    const auto second = song.studio.addTrack ({});
    song.studio.getEngine().setInstrument (second, std::make_unique<SineTone> (660.0, 0.2f, 0.2f));
    song.studio.renameTrack (second, "Lead");
    Studio::ExportBatch batch;
    batch.options = song.eightBeats();
    batch.options.fileType = Studio::ExportOptions::FileType::mp3;
    batch.channels = { song.studio.getProject().tracks[0].id, second };
    juce::String error;
    std::vector<juce::File> written;
    REQUIRE (song.studio.exportBatch (song.folder, "Song", batch, error, &written));
    REQUIRE (written.size() == 2);
    CHECK (written[1].getFileName() == "Song - Lead.mp3");
    CHECK_THAT (frequencyOf (readBack (written[1]), 0, 1.0, 3.0), WithinAbs (660.0, 1.5));
    batch.options.fileType = Studio::ExportOptions::FileType::flac;
    written.clear();
    REQUIRE (song.studio.exportBatch (song.folder, "Song", batch, error, &written));
    CHECK (written[1].getFileName() == "Song - Lead.flac");
    CHECK_THAT (peakOf (readBack (written[1]), 0, 1.0, 3.0), WithinAbs (0.2, 0.01));
}

TEST_CASE ("0.200.0: the export page's file types, their options, kept for next time", "[export][formats][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    const auto settingsFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-export-formats.settings");
    settingsFile.deleteFile();
    Studio studio (false);
    studio.setSettingsForTests (std::make_unique<juce::PropertiesFile> (settingsFile, juce::PropertiesFile::Options {}));
    {
        Windows::ExportPage page (studio);
        CHECK (page.getFileType() == Windows::ExportPage::FileType::wave);
        CHECK (page.getOptions().fileType == Studio::ExportOptions::FileType::wave);
        const auto waveHeight = page.getHeight();
        page.setFileType (Windows::ExportPage::FileType::flac);
        CHECK (page.flacLevel.isVisible());
        CHECK_FALSE (page.bits.isVisible());
        CHECK (page.getHeight() > waveHeight);
        page.flacLevel.setSelectedId (9, juce::dontSendNotification);   // level 8
        page.flacBits.setSelectedId (16, juce::dontSendNotification);
        auto o = page.getOptions();
        CHECK (o.fileType == Studio::ExportOptions::FileType::flac);
        CHECK (o.flacCompression == 8);
        CHECK (o.bitsPerSample == 16);
        CHECK (o.extension() == ".flac");

        page.setFileType (Windows::ExportPage::FileType::mp3);
        CHECK (page.mp3BitRate.isVisible());
        CHECK_FALSE (page.title.isVisible());
        const auto mp3Height = page.getHeight();
        Mp3Settings m;
        m.bitRate = 192;
        m.sampleRate = 44100;
        m.highQuality = false;
        m.id3 = true;
        m.title = "Song";
        m.artist = "Band";
        m.album = "Record";
        m.year = "2026";
        m.genre = "Rock";
        m.track = "2";
        m.comment = "A note";
        page.setMp3Settings (m);
        CHECK (page.title.isVisible());
        CHECK (page.getHeight() > mp3Height);
        o = page.getOptions();
        CHECK (o.fileType == Studio::ExportOptions::FileType::mp3);
        CHECK (o.extension() == ".mp3");
        CHECK (o.mp3 == m);
        CHECK (o.sampleRate == 0.0);
        CHECK (page.getBatch().options.mp3 == m);
        CHECK (page.mp3Rate.isEnabled());
        page.highQuality.setToggleState (true, juce::sendNotificationSync);
        CHECK_FALSE (page.mp3Rate.isEnabled());
        page.highQuality.setToggleState (false, juce::sendNotificationSync);
        page.exportButton.onClick();   // kept
        CHECK (page.createComponentSnapshot (page.getLocalBounds()).isValid());
    }
    {
        // Next time: MP3 again, its settings; the title the song's own (none: empty), the track number not kept.
        Windows::ExportPage page (studio);
        CHECK (page.getFileType() == Windows::ExportPage::FileType::mp3);
        const auto o = page.getOptions();
        CHECK (o.mp3.bitRate == 192);
        CHECK (o.mp3.sampleRate == 44100);
        CHECK_FALSE (o.mp3.highQuality);
        CHECK (o.mp3.id3);
        CHECK (o.mp3.artist == "Band");
        CHECK (o.mp3.genre == "Rock");
        CHECK (o.mp3.comment == "A note");
        CHECK (o.mp3.title.isEmpty());
        CHECK (o.mp3.track.isEmpty());
        page.setFileType (Windows::ExportPage::FileType::flac);
        CHECK (page.getOptions().flacCompression == 8);
    }
    studio.setSettingsForTests (nullptr);
    settingsFile.deleteFile();
}

TEST_CASE ("0.200.0: the MP3 settings' text and the lists", "[export][formats][mp3]")
{
    Mp3Settings m;
    CHECK (Mp3Settings::fromString (m.toString()) == m);
    m.bitRate = 96;
    m.sampleRate = 32000;
    m.id3 = true;
    m.title = "A | B";
    const auto back = Mp3Settings::fromString (m.toString());
    CHECK (back.bitRate == 96);
    CHECK (back.sampleRate == 32000);
    CHECK (back.title == "A / B");
    CHECK (Mp3Settings::fromString ("999|12345|1|0").bitRate == 320);
    CHECK (mp3BitRates().front() == 8);
    CHECK (mp3BitRates().back() == 320);
    CHECK (mp3Genres().contains ("Metal"));
    CHECK (mp3Genres().size() > 100);
}
