// Speed, the screen's half (0.191.0): the MIDI editor's part kept, kept shadows, the track list's rows
// repainted only when they change, the tempo and signature maps kept, and the paints' own scans.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstdio>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/KeyEditor.h"
#include "ui/MainView.h"
#include "ui/ParagonLookAndFeel.h"

using namespace pr7s;

namespace
{
    using perfClock = std::chrono::steady_clock;

    template <typename F> double msPer (int times, F&& f)
    {
        f();
        const auto t0 = perfClock::now();
        for (int i = 0; i < times; ++i)
            f();
        return std::chrono::duration<double, std::milli> (perfClock::now() - t0).count() / times;
    }

    // A big song: 30 tracks of 20 parts (600), the lead's first part 2,000 notes, a chord track.
    struct BigSong
    {
        Studio studio { false };
        MainView view { studio };
        int lead = 0;
        BigSong()
        {
            Finish::set (Finish::Type::pearlWhite, false);
            view.setSize (1500, 900);
            view.setKeyEditorSize (100);
            for (int t = 1; t < 30; ++t)
                studio.addTrack ({});
            // (the lead's part found last: the last track's last part)
            for (int t = 0; t < 30; ++t)
                for (int p = 0; p < (t == 29 ? 19 : 20); ++p)
                {
                    Part small { 128.0 + p * 8.0, 8.0, "P", {} };
                    for (int n = 0; n < 16; ++n)
                        small.notes.push_back ({ n * 0.5, 0.4, 50 + n, 0.7f });
                    studio.addPart (studio.getProject().tracks[(size_t) t].id, small);
                }
            lead = studio.getProject().tracks.back().id;
            Part part;
            part.length = 128.0;
            part.name = "Lead";
            for (int i = 0; i < 2000; ++i)
                part.notes.push_back ({ i * 128.0 / 2000, 0.25, 40 + (i * 5) % 40, 0.3f + 0.7f * (float) ((i * 37) % 100) / 100.0f });
            studio.addPart (lead, part);
            studio.addTempoTrack();
            for (int e = 1; e < 8; ++e)
                studio.addTempoEvent (e * 16.0, 100.0 + e * 5.0);
            Chord am { 9 };
            am.type = Chord::Type::minor;
            std::vector<ChordEvent> chords;
            for (int bar = 0; bar < 32; ++bar)
                chords.push_back ({ bar * 4.0, bar % 2 == 0 ? Chord { 0 } : am, 4.0 });
            studio.setChordEvents (chords);
            studio.setCycle (16.0, 48.0);
            studio.setCycleOn (true);
            studio.dispatchPendingMessages();
            view.getArrangeView().select (lead, (int) studio.getProject().findTrack (lead)->parts.size() - 1);
            view.openKeyEditor();
            studio.dispatchPendingMessages();
        }
        KeyEditor& editor() { return view.getKeyEditor(); }
    };
}

TEST_CASE ("0.191.0: timings", "[.perf-191]")
{
    {
        BigSong song;
        auto& editor = song.editor();
        editor.fitToPart();
        juce::Image image (juce::Image::RGB, editor.getWidth(), editor.getHeight(), true, juce::SoftwareImageType());
        std::printf ("The MIDI editor's whole paint, 2,000 notes in a 600-part song, CHORD GUIDES on: %.2f ms\n",
                     msPer (20, [&] { juce::Graphics g (image); editor.paintFreshForTests (g); }));
        const auto area = editor.getNotesArea();
        std::printf ("The MIDI editor's mouse moves (a note looked for under the mouse): %.3f ms\n",
                     msPer (200, [&]
                     {
                         for (int i = 0; i < 10; ++i)
                         {
                             const juce::Point<float> at { area.getX() + area.getWidth() * (float) i / 10.0f, area.getCentreY() };
                             editor.mouseMove (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                                 &editor, &editor, juce::Time::getCurrentTime(), at, juce::Time::getCurrentTime(), 1, false));
                         }
                     }) / 10.0);
    }
    {
        // A panel of 120 pedal keys.
        Finish::set (Finish::Type::pearlWhite, false);
        ParagonLookAndFeel lookAndFeel;
        juce::Component panel;
        panel.setLookAndFeel (&lookAndFeel);
        panel.setSize (1200, 480);
        juce::OwnedArray<juce::TextButton> keys;
        for (int i = 0; i < 120; ++i)
        {
            auto* key = keys.add (new juce::TextButton ("KEY " + juce::String (i)));
            key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
            key->setBounds ((i % 12) * 100 + 4, (i / 12) * 48 + 4, 92, 40);
            panel.addAndMakeVisible (key);
        }
        juce::Image image (juce::Image::RGB, panel.getWidth(), panel.getHeight(), true, juce::SoftwareImageType());
        std::printf ("A panel of 120 pedal keys: %.2f ms\n", msPer (20, [&] { juce::Graphics g (image); panel.paintEntireComponent (g, false); }));
        panel.setLookAndFeel (nullptr);
    }
    {
        BigSong song;
        song.view.setSize (5760, 1000);
        song.studio.dispatchPendingMessages();
        juce::Image image (juce::Image::RGB, 5760, 1000, true, juce::SoftwareImageType());
        std::printf ("The whole window at 5760 wide: %.2f ms\n", msPer (5, [&] { juce::Graphics g (image); song.view.paintEntireComponent (g, false); }));
        std::printf ("The studio's tempo and signature now (a timer's look): %.4f ms\n",
                     msPer (2000, [&] { (void) song.studio.getTempoNow(); (void) song.studio.getSignatureNow(); }));
    }
}

namespace
{
    int differing (const juce::Image& a, const juce::Image& b, int levels)
    {
        auto far = 0;
        for (int y = 0; y < a.getHeight(); ++y)
            for (int x = 0; x < a.getWidth(); ++x)
            {
                const auto p = a.getPixelAt (x, y), q = b.getPixelAt (x, y);
                const auto d = juce::jmax (std::abs (p.getRed() - q.getRed()), std::abs (p.getGreen() - q.getGreen()), std::abs (p.getBlue() - q.getBlue()));
                far += d > levels ? 1 : 0;
            }
        return far;
    }
}

TEST_CASE ("0.191.0: the MIDI editor keeps its part's place and finds it again", "[speed191]")
{
    BigSong song;
    auto& editor = song.editor();
    auto& studio = song.studio;
    const auto* track = studio.getProject().findTrack (song.lead);
    const auto id = track->parts.back().id;
    REQUIRE (editor.getPart() != nullptr);
    CHECK (editor.getPart()->id == id);
    CHECK (editor.getTrackId() == song.lead);

    // An edit (an undo step: a copy of the song) and a part removed before it: the same part, found again.
    auto notes = editor.getPart()->notes;
    notes.front().pitch += 1;
    studio.setNotes (song.lead, (int) studio.getProject().findTrack (song.lead)->parts.size() - 1, notes);
    REQUIRE (editor.getPart() != nullptr);
    CHECK (editor.getPart()->id == id);
    CHECK (editor.getPart()->notes.front().pitch == notes.front().pitch);
    studio.deleteParts ({ studio.getProject().findTrack (song.lead)->parts.front().id });
    REQUIRE (editor.getPart() != nullptr);
    CHECK (editor.getPart()->id == id);
    CHECK (editor.getPart() == &studio.getProject().findTrack (song.lead)->parts.back());
    studio.undo();
    REQUIRE (editor.getPart() != nullptr);
    CHECK (editor.getPart()->id == id);
    CHECK (editor.getPart() == &studio.getProject().findTrack (song.lead)->parts.back());
    studio.undo();
    REQUIRE (editor.getPart() != nullptr);
    CHECK (editor.getPart()->notes.front().pitch == notes.front().pitch - 1);
    // The part itself removed: none.
    studio.deleteParts ({ id });
    CHECK (editor.getPart() == nullptr);
    CHECK (editor.getTrackId() == 0);
    studio.undo();
    CHECK (editor.getPart() != nullptr);
}

TEST_CASE ("0.191.0: a chord's guide levels are the per-pitch rule's", "[speed191]")
{
    BigSong song;
    auto& editor = song.editor();
    for (int pitch = 30; pitch < 100; ++pitch)
        for (const auto beat : { 0.5, 5.0, 33.0, 127.9, 200.0 })
        {
            const auto& track = song.studio.getProject().chordTrack;
            const auto index = track.indexAt (beat);
            if (index < 0 || beat >= track.endOf (index, 4.0) - 1.0e-9)
                CHECK (editor.chordGuideLevel (pitch, beat) == 0);
            else
                CHECK (editor.chordGuideLevel (pitch, beat) == editor.chordGuideLevels (index)[(size_t) (pitch % 12)]);
        }
}

TEST_CASE ("0.191.0: a kept shadow is drawForPath's, pixel for pixel", "[speed191]")
{
    const auto shapes = { juce::Rectangle<float> (20.0f, 14.0f, 92.0f, 40.0f), juce::Rectangle<float> (20.5f, 14.25f, 92.0f, 40.0f),
                          juce::Rectangle<float> (140.0f, 60.0f, 92.0f, 40.0f), juce::Rectangle<float> (140.5f, 60.25f, 92.0f, 40.0f),
                          juce::Rectangle<float> (33.0f, 90.0f, 30.0f, 44.0f) };
    for (const auto scale : { 1.0f, 1.5f, 2.0f })
        for (const auto& shape : shapes)
        {
            juce::Image a (juce::Image::RGB, 600, 400, true, juce::SoftwareImageType()), b (juce::Image::RGB, 600, 400, true, juce::SoftwareImageType());
            {
                juce::Graphics g (a);
                g.fillAll (juce::Colour (0xffe9e4e7));
                g.addTransform (juce::AffineTransform::scale (scale));
                juce::Path outline;
                outline.addRoundedRectangle (shape, 6.0f);
                juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 9, { 0, 4 }).drawForPath (g, outline);
            }
            for (int twice = 0; twice < 2; ++twice)
            {
                juce::Graphics g (b);
                g.fillAll (juce::Colour (0xffe9e4e7));
                g.addTransform (juce::AffineTransform::scale (scale));
                const auto made = ParagonLookAndFeel::getShadowsMade();
                ParagonLookAndFeel::drawRoundedShadow (g, shape, 6.0f, juce::Colours::black.withAlpha (0.6f), 9, { 0, 4 });
                if (twice == 1)
                    CHECK (ParagonLookAndFeel::getShadowsMade() == made);   // (the second time: the kept one)
                CHECK (differing (a, b, 0) == 0);
            }
        }
    // Moved by whole pixels: the same image, not made again.
    juce::Image c (juce::Image::RGB, 600, 400, true, juce::SoftwareImageType());
    juce::Graphics g (c);
    ParagonLookAndFeel::drawRoundedShadow (g, { 300.5f, 200.25f, 92.0f, 40.0f }, 6.0f, juce::Colours::black, 9, { 0, 4 });
    const auto made = ParagonLookAndFeel::getShadowsMade();
    ParagonLookAndFeel::drawRoundedShadow (g, { 310.5f, 230.25f, 92.0f, 40.0f }, 6.0f, juce::Colours::black, 9, { 0, 4 });
    CHECK (ParagonLookAndFeel::getShadowsMade() == made);
}

TEST_CASE ("0.191.0: the track list's rows aren't drawn again for a change they don't show", "[speed191]")
{
    BigSong song;
    auto& studio = song.studio;
    auto& list = song.view.getTrackList();
    studio.dispatchPendingMessages();
    const auto before = list.getRowsRedrawn();
    // A part muted: nothing a row shows.
    studio.setPartMuted (song.lead, 0, true);
    studio.dispatchPendingMessages();
    CHECK (list.getRowsRedrawn() == before);
    // A track renamed: that row alone.
    studio.renameTrack (song.lead, "Renamed");
    studio.dispatchPendingMessages();
    CHECK (list.getRowsRedrawn() == before + 1);
}

TEST_CASE ("0.191.0: the kept tempo and signature maps are the song's", "[speed191]")
{
    Studio studio (false);
    const auto same = [&studio]
    {
        const auto fresh = studio.getProject().tempoMap();
        const auto& kept = studio.getTempoMap();
        for (double beat = 0.0; beat < 200.0; beat += 1.7)
        {
            CHECK (kept.bpmAt (beat) == fresh.bpmAt (beat));
            CHECK (kept.secondsAt (beat) == fresh.secondsAt (beat));
        }
        const auto freshSignatures = studio.getProject().signatures();
        for (double beat = 0.0; beat < 200.0; beat += 1.3)
            CHECK (studio.getSignatureMap().barAt (beat) == freshSignatures.barAt (beat));
        CHECK (studio.getTempoNow() == studio.getProject().bpmAt (studio.getEngine().getPosition()));
    };
    same();
    studio.setTempo (97.0);
    same();
    studio.addTempoTrack();
    studio.addTempoEvent (16.0, 140.0);
    same();
    studio.addTempoEvent (32.0, 80.0);
    studio.setPosition (40.0);
    same();
    studio.setSignatureAt (3, 7, 8);
    same();
    studio.undo();
    same();
    studio.undo();
    same();
    studio.setTempoTrackOn (false);
    same();
}

TEST_CASE ("0.191.0: the kept layout is the song's as it is now", "[speed191]")
{
    BigSong song;
    auto& studio = song.studio;
    auto& timeline = song.view.getArrangeView().getTimeline();
    const auto fresh = [&]
    {
        Timeline other;
        other.rowZoom = timeline.rowZoom;
        return other.layout (studio);
    };
    CHECK (timeline.layout (studio) == fresh());
    const auto made = timeline.getLayoutsMade();
    (void) timeline.layout (studio);
    CHECK (timeline.getLayoutsMade() == made);   // (nothing changed: kept)
    studio.setShowLanes (song.lead, true);
    CHECK (timeline.layout (studio) == fresh());
    const auto folder = studio.moveTracksToNewFolder ({ studio.getProject().tracks[2].id, studio.getProject().tracks[3].id }, false);
    CHECK (timeline.layout (studio) == fresh());
    studio.setFolderOpen (folder, false);
    CHECK (timeline.layout (studio) == fresh());
    timeline.rowZoom = 1.5;
    CHECK (timeline.layout (studio) == fresh());
    studio.undo();
    CHECK (timeline.layout (studio) == fresh());
}
