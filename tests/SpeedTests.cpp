#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/LinesPage.h"
#include "ui/DrumLearnPage.h"
#include "ui/MainView.h"
#include "ui/Finish.h"
#include "ui/MixerView.h"
#include "DrumSongs.h"

// Timings for the speed pass after 0.57.0 (hidden: run with "[perf-heavy]", "[perf-studio]",
// "[perf-ideas]", "[perf-learn]"). They print what each thing costs; they check nothing but that it ran.

using namespace pr7s;

namespace
{
    constexpr double rate = 48000.0;

    double millisecondsOf (int times, const std::function<void (int)>& work)
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < times; ++i)
            work (i);
        return (juce::Time::getMillisecondCounterHiRes() - start) / times;
    }

    std::vector<ChordSpan> sixteenChords()
    {
        const Key am { 5, 0, true };
        const Chord chords[] = { { 9, Chord::Type::minor, Chord::Tension::none, -1 }, { 5, Chord::Type::major, Chord::Tension::none, -1 },
                                 { 0, Chord::Type::major, Chord::Tension::none, -1 }, { 7, Chord::Type::major, Chord::Tension::none, -1 } };
        std::vector<ChordSpan> spans;
        for (int i = 0; i < 16; ++i)
            spans.push_back ({ i * 4.0, i * 4.0 + 4.0, chords[i % 4], am });
        return spans;
    }
}

TEST_CASE ("Speed: a heavy song in the engine (EQ on every channel, Follow Chord Track, a performed pattern, automation)", "[.][perf-heavy]")
{
    Engine engine;
    engine.prepare (rate, 512);
    Project project;
    for (int i = 0; i < 32; ++i)
        project.chordTrack.events.push_back ({ i * 4.0, { (i * 5) % 12, i % 2 == 0 ? Chord::Type::major : Chord::Type::minor, Chord::Tension::none, -1 } });
    project.chordTrack.shown = true;
    for (int t = 1; t <= 32; ++t)
    {
        Track track;
        track.id = t;
        Part part;
        part.length = 128.0;
        for (int n = 0; n < 2000; ++n)
            part.notes.push_back ({ n * 0.0625, 0.05, 40 + (n * 7 + t) % 40, 0.8f });
        track.parts = { part };
        for (auto& band : track.eq.bands)
        {
            band.on = true;
            band.gainDb = 3.0;
        }
        track.eq.lowCutOn = true;
        if (t <= 8)
        {
            track.follow = FollowMode::chords;
            track.followReference = FollowReference::ofChordTrack (project.chordTrack);
        }
        if (t == 9 || t == 10)
            track.performPattern = patterns()[0].name;
        AutomationLane lane;
        lane.points = { { 0.0, 0.8f }, { 64.0, 0.2f }, { 128.0, 0.9f } };
        track.automation = { lane };
        track.automationRead = true;
        project.tracks.push_back (track);
        engine.setInstrument (t, std::make_unique<Probe>());
    }
    engine.update (project);
    engine.setPosition (0.0);
    engine.play();

    juce::AudioBuffer<float> buffer (2, 512);
    const auto perBlock = millisecondsOf (400, [&] (int) { engine.render (buffer, 512); });
    const auto fader = millisecondsOf (20, [&] (int i) { project.tracks[3].volumeDb = -1.0f * (float) (i % 10); engine.update (project); });
    const auto chordEdit = millisecondsOf (20, [&] (int i) { project.chordTrack.events[3].chord.root = i % 12; engine.update (project); });
    const auto noteEdit = millisecondsOf (20, [&] (int i) { project.tracks[20].parts[0].notes[0].pitch = 40 + i % 2; engine.update (project); });
    const auto eqEdit = millisecondsOf (20, [&] (int i) { project.tracks[12].eq.bands[1].gainDb = (double) (i % 6); engine.update (project); });
    WARN ("32 tracks x 2000 notes, EQ on all, 8 following, 2 performing, automation:\n"
          << "  render a 512-sample block (10.7 ms of sound): " << perBlock << " ms\n"
          << "  update after a fader move: " << fader << " ms\n"
          << "  update after a chord edit: " << chordEdit << " ms\n"
          << "  update after a note edit: " << noteEdit << " ms\n"
          << "  update after an EQ edit: " << eqEdit << " ms");
}

TEST_CASE ("Speed: the studio's cost per change in a big song (the undo step, the engine update)", "[.][perf-studio]")
{
    Studio studio (false);
    std::vector<int> ids;
    for (int t = 0; t < 30; ++t)
    {
        const auto id = studio.addTrack (Instrument {});
        Part part;
        part.length = 128.0;
        for (int n = 0; n < 2000; ++n)
            part.notes.push_back ({ n * 0.0625, 0.05, 40 + (n * 7 + t) % 40, 0.8f });
        studio.addPart (id, part);
        ids.push_back (id);
    }
    auto notes = studio.getProject().findTrack (ids[5])->parts[0].notes;
    const auto fader = millisecondsOf (20, [&] (int i) { studio.setVolume (ids[2], -1.0f * (float) (i % 10)); });
    const auto dragging = millisecondsOf (20, [&] (int i) { studio.setVolume (ids[2], -1.0f * (float) (i % 10), false); });
    const auto noteEdit = millisecondsOf (20, [&] (int i) { notes[0].pitch = 40 + i % 2; studio.setNotes (ids[5], 0, notes); });
    const auto undo = millisecondsOf (10, [&] (int) { studio.undo(); });
    WARN ("30 tracks x 2000 notes:\n"
          << "  a fader set (an undo step): " << fader << " ms\n"
          << "  a fader dragged (no undo step): " << dragging << " ms\n"
          << "  a note edit: " << noteEdit << " ms\n"
          << "  an undo: " << undo << " ms");
}

TEST_CASE ("Speed: IDEAS dealing ideas (opening, MORE IDEAS, a STYLE change, a tab change)", "[.][perf-ideas]")
{
    Studio studio (false);
    const auto spans = sixteenChords();
    double opening = 0.0;
    std::unique_ptr<LinesPage> page;
    opening = millisecondsOf (1, [&] (int) { page = std::make_unique<LinesPage> (studio, spans); });
    const auto more = millisecondsOf (5, [&] (int) { page->moreIdeas(); });
    const auto style = millisecondsOf (5, [&] (int i) { page->setGuitarStyle (guitarStyles()[(size_t) (i % 5)]); });
    const auto tab = millisecondsOf (6, [&] (int i) { page->setTab (i % 2 == 0 ? LinesPage::Tab::drums : LinesPage::Tab::guitar); });
    // As before 0.57.1: every kind dealt on every change (each asked for after MORE IDEAS).
    const auto every = millisecondsOf (5, [&] (int)
    {
        page->moreIdeas();
        for (int p = 0; p < 9; ++p)
            page->getIdeas ((LineIdea::Part) p);
    });
    WARN ("IDEAS over 16 bars of chords:\n"
          << "  opening: " << opening << " ms\n"
          << "  MORE IDEAS: " << more << " ms\n"
          << "  a STYLE change: " << style << " ms\n"
          << "  a tab change (drums <-> guitar): " << tab << " ms\n"
          << "  every kind dealt (as before 0.57.1): " << every << " ms");
}

TEST_CASE ("Speed: reading a folder of drum songs and learning from it", "[.][perf-learn]")
{
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-speed-songs");
    folder.deleteRecursively();
    const char* layouts[] = { "I4 V8 C8 V8 C8 B8 C8 E4", "V8 C8 V8 C8 R8 C8 E4", "I8 V8 V8 C8 V8 C8 B8 B8 C8 E4", "I4 C8 V8 C8 R8 C8 C8 E4" };
    for (int s = 0; s < 40; ++s)
        drumsongs::writeSong (folder.getChildFile ("Song " + juce::String (s) + ".mid"), drumsongs::songOf (layouts[s % 4]));
    Studio studio (false);
    DrumLearnPage page (studio);
    const auto reading = millisecondsOf (1, [&] (int) { page.loadFolder (folder); });
    std::vector<DrumSong> songs (page.getSongs().begin(), page.getSongs().end());
    DrumProfile profile;
    const auto learning = millisecondsOf (3, [&] (int) { profile = learnProfile ("Speed", songs); });
    const auto beats = millisecondsOf (5, [&] (int i) { generatedBeats (profile, sixteenChords(), 4.0, {}, i); });
    WARN ("40 songs (" << page.getSongs().size() << " read):\n"
          << "  reading the folder (sections found): " << reading << " ms\n"
          << "  learning a profile: " << learning << " ms\n"
          << "  six beats from it: " << beats << " ms");
    folder.deleteRecursively();
}

TEST_CASE ("Speed: audio tracks in the engine (takes on lanes, auto crossfades)", "[.][perf-audio]")
{
    Engine engine;
    engine.prepare (rate, 512);
    auto sound = std::make_shared<juce::AudioBuffer<float>> (2, (int) (rate * 300.0));
    sound->clear();
    engine.setAudioSource ([sound] (const juce::String&) -> std::shared_ptr<const juce::AudioBuffer<float>> { return sound; });
    Project project;
    project.autoFades.crossfades = true;
    for (int t = 1; t <= 24; ++t)
    {
        Track track;
        track.id = t;
        track.kind = Track::Kind::audio;
        for (int lane = 0; lane < 3; ++lane)
            for (int c = 0; c < 40; ++c)
            {
                Part p;
                p.start = c * 4.0 + lane * 0.5;
                p.length = 3.5;
                p.audioFile = "take" + juce::String (lane) + ".wav";
                p.audioOffset = c * 2.0;
                p.lane = lane;
                p.order = (c * 7 + lane) % 3;
                track.parts.push_back (p);
            }
        project.tracks.push_back (track);
        engine.addAudioTrack (t);
    }
    engine.update (project);
    engine.setPosition (0.0);
    engine.play();
    juce::AudioBuffer<float> buffer (2, 512);
    const auto perBlock = millisecondsOf (400, [&] (int) { engine.render (buffer, 512); });
    const auto fader = millisecondsOf (20, [&] (int i) { project.tracks[3].volumeDb = -1.0f * (float) (i % 10); engine.update (project); });
    const auto clipEdit = millisecondsOf (20, [&] (int i) { project.tracks[5].parts[7].fadeIn = 0.01 * (i % 3); engine.update (project); });
    WARN ("24 audio tracks x 120 clips on 3 lanes, auto crossfades:\n"
          << "  render a 512-sample block: " << perBlock << " ms\n"
          << "  update after a fader move: " << fader << " ms\n"
          << "  update after a clip edit: " << clipEdit << " ms");
}

TEST_CASE ("Speed: the screen's redraws while playing (the transport's screens, a mixer meter)", "[.][perf-screen]")
{
    Studio studio (false);
    MainView view (studio);
    view.setSize (1920, 1000);
    for (int t = 1; t < 16; ++t)
        studio.addTrack ({});
    studio.dispatchPendingMessages();
    view.openMixer();
    studio.dispatchPendingMessages();
    auto image = juce::Image (juce::Image::ARGB, 1920, 1000, true);
    const auto clipped = [&] (juce::Component& c, juce::Rectangle<int> area)
    {
        return millisecondsOf (40, [&] (int)
        {
            juce::Graphics g (image);
            g.reduceClipRegion (area);
            c.paintEntireComponent (g, false);
        });
    };
    auto& transport = view.getTransport();
    const auto screens = transport.getPositionScreen().expanded (4.0f).getSmallestIntegerContainer()
                             .getUnion (transport.getTimeScreen().expanded (4.0f).getSmallestIntegerContainer());
    auto& strip = view.getMixer().getStrip (3);
    WARN ("A frame's redraws:\n"
          << "  the transport's POSITION and TIME screens: " << clipped (transport, screens) << " ms\n"
          << "  the whole transport: " << clipped (transport, transport.getLocalBounds()) << " ms\n"
          << "  a mixer strip's meter: " << clipped (strip, strip.getMeterArea()) << " ms (x 17 strips)\n"
          << "  the whole strip: " << clipped (strip, strip.getLocalBounds()) << " ms");
}

TEST_CASE ("Snapshot: the transport whole and redrawn in strips (to compare two builds pixel by pixel)", "[.][snapshot-transport]")
{
    // PARAGON_TRANSPORT_PNG=<folder>: transport-whole.png, and transport-strips.png (each screen redrawn
    // over the whole as it is while playing). Every finish.
    const auto folder = juce::File (juce::SystemStats::getEnvironmentVariable ("PARAGON_TRANSPORT_PNG", {}));
    REQUIRE (folder != juce::File());
    folder.createDirectory();
    for (const auto finish : { Finish::Type::pearlWhite, Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon })
        {
            Finish::set (finish, false);
            Studio studio (false);
            MainView view (studio);
            view.setSize (1920, 1000);
            studio.setPosition (37.25);
            studio.dispatchPendingMessages();
            auto& transport = view.getTransport();
            const auto name = juce::String (finish == Finish::Type::pearlWhite ? "white" : finish == Finish::Type::pearlBlack ? "black" : "neon");
            for (const auto scale : { 1.0f, 1.5f })
            {
                const auto w = juce::roundToInt (transport.getWidth() * scale), h = juce::roundToInt (transport.getHeight() * scale);
                juce::Image whole (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
                {
                    juce::Graphics g (whole);
                    g.addTransform (juce::AffineTransform::scale (scale));
                    transport.paintEntireComponent (g, false);
                }
                auto strips = whole.createCopy();
                for (const auto screen : { transport.getPositionScreen(), transport.getTimeScreen() })
                {
                    juce::Graphics g (strips);
                    g.addTransform (juce::AffineTransform::scale (scale));
                    g.reduceClipRegion (screen.expanded (4.0f).getSmallestIntegerContainer());
                    transport.paintEntireComponent (g, false);
                }
                const auto suffix = name + "-" + juce::String (scale, 1) + ".png";
                for (const auto& [image, file] : { std::pair { whole, "transport-whole-" + suffix }, std::pair { strips, "transport-strips-" + suffix } })
                {
                    juce::FileOutputStream out (folder.getChildFile (file));
                    out.setPosition (0);
                    out.truncate();
                    juce::PNGImageFormat().writeImageToStream (image, out);
                }
            }
        }
    Finish::set (Finish::Type::pearlWhite, false);
}
