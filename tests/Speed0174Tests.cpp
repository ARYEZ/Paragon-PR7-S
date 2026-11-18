// Speed, round seven (0.174.0): the owner's two look changes and the redraws that were left.

#include <catch2/catch_test_macros.hpp>

#include "engine/Studio.h"
#include "engine/MediaBay.h"
#include "ui/ArrangeView.h"
#include "ui/EqEditor.h"
#include "ui/Finish.h"
#include "ui/KeyEditor.h"
#include "ui/MainView.h"
#include "ui/TransportBar.h"

using namespace pr7s;

namespace
{
    struct Song
    {
        Studio studio { false };
        MainView view { studio };
        int first = 0, second = 0, third = 0;
        Song()
        {
            Finish::set (Finish::Type::pearlWhite, false);
            view.setSize (1500, 900);
            view.setKeyEditorSize (100);
            first = studio.getProject().tracks[0].id;
            studio.addTrack ({});
            second = studio.getProject().tracks.back().id;
            studio.addTrack ({});
            third = studio.getProject().tracks.back().id;
            for (const auto id : { first, second, third })
            {
                Part part;
                part.start = 4.0;
                part.length = 16.0;
                part.name = "Part";
                for (int i = 0; i < 32; ++i)
                    part.notes.push_back ({ i * 0.5, 0.25, 48 + (i * 7) % 24, 0.8f });
                studio.addPart (id, part);
            }
            studio.dispatchPendingMessages();
        }
        void settle() { studio.dispatchPendingMessages(); }
        float rowTopOf (int trackId)
        {
            auto& arrange = view.getArrangeView();
            for (const auto& row : view.getTimeline().layout (studio))
                if (row.kind == Timeline::Row::Kind::track && row.busId == trackId)
                    return arrange.rowTop (row);
            return -1.0f;
        }
    };

    int farPixels (const juce::Image& a, const juce::Image& b, int levels)
    {
        auto far = 0;
        for (int y = 0; y < a.getHeight(); ++y)
            for (int x = 0; x < a.getWidth(); ++x)
            {
                const auto p = a.getPixelAt (x, y), q = b.getPixelAt (x, y);
                far += juce::jmax (std::abs (p.getRed() - q.getRed()), std::abs (p.getGreen() - q.getGreen()), std::abs (p.getBlue() - q.getBlue())) > levels ? 1 : 0;
            }
        return far;
    }
}

TEST_CASE ("0.174.0: an edit of a part's notes changes the content, not the layout", "[speed174]")
{
    Song song;
    auto& studio = song.studio;
    const auto content = studio.getContentVersion(), layout = studio.getLayoutVersion();
    auto notes = studio.getProject().findTrack (song.second)->parts[0].notes;
    notes[0].pitch += 2;
    studio.setNotes (song.second, 0, notes);
    CHECK (studio.getContentVersion() > content);
    CHECK (studio.getLayoutVersion() == layout);
    // Muted, resized, a field changed: parts alone too.
    studio.setPartMuted (song.second, 0, true);
    studio.resizePart (song.second, 0, 12.0);
    CHECK (studio.getLayoutVersion() == layout);
    // Anything else: the layout too (and the flag never left behind for the next change).
    studio.renameTrack (song.third, "Renamed");
    CHECK (studio.getLayoutVersion() > layout);
    const auto after = studio.getLayoutVersion();
    studio.setPartsMuted ({ studio.getProject().findTrack (song.first)->parts[0].id }, true);
    CHECK (studio.getLayoutVersion() == after);
    studio.undo();
    CHECK (studio.getLayoutVersion() > after);
}

TEST_CASE ("0.174.0: muting a part copies only its own track's parts", "[speed174]")
{
    Song song;
    auto& studio = song.studio;
    const auto before = studio.getProject();   // (sharing every track's parts with the song)
    studio.setPartsMuted ({ studio.getProject().findTrack (song.second)->parts[0].id }, true);
    CHECK (studio.getProject().findTrack (song.first)->parts.sharesWith (before.findTrack (song.first)->parts));
    CHECK (studio.getProject().findTrack (song.third)->parts.sharesWith (before.findTrack (song.third)->parts));
    CHECK_FALSE (studio.getProject().findTrack (song.second)->parts.sharesWith (before.findTrack (song.second)->parts));
}

TEST_CASE ("0.174.0: the arrangement redraws only the rows whose parts changed, and draws them right", "[speed174][ui]")
{
    Song song;
    auto& studio = song.studio;
    auto& arrange = song.view.getArrangeView();
    song.settle();
    auto notes = studio.getProject().findTrack (song.second)->parts[0].notes;
    notes[3].pitch += 5;
    studio.setNotes (song.second, 0, notes);
    song.settle();
    const auto& areas = arrange.getPartRepaints();
    REQUIRE (areas.size() == 1);
    const auto top = song.rowTopOf (song.second);
    CHECK (areas[0].getY() <= (int) top);
    CHECK (areas[0].getBottom() >= (int) top + song.view.getTimeline().trackRowHeight());
    CHECK (areas[0].getHeight() < arrange.getHeight() / 2);
    // What's drawn after the row alone is what a whole redraw draws.
    juce::Image kept (juce::Image::RGB, arrange.getWidth(), arrange.getHeight(), true, juce::SoftwareImageType());
    {
        juce::Graphics g (kept);
        arrange.paintFreshForTests (g);
    }
    // (a row's redraw paints the same as everything: the paint is clipped, not different)
    juce::Image strip (juce::Image::RGB, arrange.getWidth(), arrange.getHeight(), true, juce::SoftwareImageType());
    {
        juce::Graphics g (strip);
        g.drawImageAt (kept, 0, 0);
        g.reduceClipRegion (areas[0]);
        g.fillAll (juce::Colours::black);
        arrange.paint (g);
    }
    CHECK (farPixels (kept, strip, 24) == 0);
    // A change of something else: everything again.
    studio.renameTrack (song.first, "Other");
    song.settle();
    CHECK (arrange.getPartRepaints().empty());
}

TEST_CASE ("0.174.0: the MIDI editor isn't redrawn for another track's parts", "[speed174][ui]")
{
    Song song;
    auto& studio = song.studio;
    song.view.getArrangeView().select (song.first, 0);
    song.view.openKeyEditor();
    song.settle();
    auto& editor = song.view.getKeyEditor();
    REQUIRE (editor.getPart() != nullptr);
    const auto skipped = editor.getRefreshesSkipped();
    auto notes = studio.getProject().findTrack (song.second)->parts[0].notes;
    notes[0].pitch += 1;
    studio.setNotes (song.second, 0, notes);
    song.settle();
    CHECK (editor.getRefreshesSkipped() == skipped + 1);
    // Its own part edited: shown.
    auto own = studio.getProject().findTrack (song.first)->parts[0].notes;
    own[0].pitch += 1;
    studio.setNotes (song.first, 0, own);
    song.settle();
    CHECK (editor.getRefreshesSkipped() == skipped + 1);
    REQUIRE (editor.getPart() != nullptr);
    CHECK (editor.getPart()->notes[0].pitch == own[0].pitch);
}

TEST_CASE ("0.174.0: the cycle's tint is solid colour in the rows", "[speed174][ui]")
{
    Song song;
    auto& studio = song.studio;
    studio.setCycle (8.0, 12.0);
    studio.setCycleOn (true);
    song.settle();
    auto& arrange = song.view.getArrangeView();
    auto& timeline = song.view.getTimeline();
    juce::Image image (juce::Image::RGB, arrange.getWidth(), arrange.getHeight(), true, juce::SoftwareImageType());
    {
        juce::Graphics g (image);
        arrange.paintFreshForTests (g);
    }
    // Under the last row, inside the cycle and outside it.
    const auto y = arrange.getHeight() - 20;
    const auto inside = image.getPixelAt (juce::roundToInt (timeline.xForBeat (10.0)), y);
    const auto outside = image.getPixelAt (juce::roundToInt (timeline.xForBeat (20.0)), y);
    const auto tint = ParagonColours::accent.withAlpha (0.07f);
    const auto expected = outside.overlaidWith (tint);
    if (const auto file = juce::SystemStats::getEnvironmentVariable ("PARAGON_TINT_PNG", {}); file.isNotEmpty())
    {
        juce::FileOutputStream out { juce::File (file) };
        out.setPosition (0); out.truncate();
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::printf ("inside %s outside %s expected %s at y %d x %d/%d\n", inside.toString().toRawUTF8(), outside.toString().toRawUTF8(),
                     expected.toString().toRawUTF8(), y, juce::roundToInt (timeline.xForBeat (10.0)), juce::roundToInt (timeline.xForBeat (20.0)));
    }
    CHECK (std::abs (inside.getRed() - expected.getRed()) <= 2);
    CHECK (std::abs (inside.getGreen() - expected.getGreen()) <= 2);
    CHECK (std::abs (inside.getBlue() - expected.getBlue()) <= 2);
    CHECK (inside != outside);
}

TEST_CASE ("0.174.0: the transport's screens are drawn from kept pictures", "[speed174][ui]")
{
    Song song;
    auto& transport = song.view.getTransport();
    juce::Image first (juce::Image::RGB, transport.getWidth(), transport.getHeight(), true, juce::SoftwareImageType());
    {
        juce::Graphics g (first);
        transport.paintEntireComponent (g, false);
    }
    const auto made = transport.getScreenPicturesMade();
    CHECK (made > 0);
    juce::Image second (juce::Image::RGB, transport.getWidth(), transport.getHeight(), true, juce::SoftwareImageType());
    {
        juce::Graphics g (second);
        transport.paintEntireComponent (g, false);
    }
    CHECK (transport.getScreenPicturesMade() == made);   // (nothing drawn afresh)
    CHECK (farPixels (first, second, 0) == 0);
}

TEST_CASE ("0.174.0: the EQ window's grid and curve are drawn from kept pictures", "[speed174][ui]")
{
    Song song;
    EqCurve curve (song.studio, song.first, false);
    curve.setSize (700, 320);
    juce::Image image (juce::Image::ARGB, 700, 320, true, juce::SoftwareImageType());
    const auto draw = [&] { juce::Graphics g (image); curve.paintEntireComponent (g, false); };
    draw();
    const auto made = curve.getPicturesMade();
    CHECK (made == 2);
    draw();
    draw();
    CHECK (curve.getPicturesMade() == made);
    // The EQ changed: the curve drawn again (the grid isn't).
    auto eq = *song.studio.getChannelEq (song.first);
    eq.bands[1].on = true;
    eq.bands[1].gainDb = 6.0;
    song.studio.setChannelEq (song.first, eq, true);
    curve.refreshForTests();
    draw();
    CHECK (curve.getPicturesMade() == made + 1);
}

TEST_CASE ("0.174.0: a scan is seen by every view, not only the first to ask", "[speed174]")
{
    juce::TemporaryFile store (".xml");
    MediaDatabase database (store.getFile());
    const auto scans = database.getScansTaken();
    CHECK_FALSE (database.takeScanResults());
    CHECK (database.getScansTaken() == scans);
}
