// Speed, round six (0.165.0): the EQ's stages in one pass, parts shared between copies of the song,
// the arrangement kept as a picture while Stationary Cursor scrolls it. Each checked against the old way.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>

#include "TestHelpers.h"
#include "engine/Eq.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/MainView.h"

using namespace pr7s;

namespace
{
    // The EQ as it was before 0.165.0: a stage at a time over the block, a float between stages.
    void oldCascade (const EqDesign& d, std::vector<float>& samples, std::vector<std::array<double, 2>>& state)
    {
        state.resize ((size_t) EqDesign::maxStages);
        for (int k = 0; k < d.numEq; ++k)
        {
            const auto& f = d.eq[(size_t) k];
            auto s1 = state[(size_t) k][0], s2 = state[(size_t) k][1];
            for (auto& sample : samples)
            {
                const double x = sample;
                const auto y = f.b0 * x + s1;
                s1 = f.b1 * x - f.a1 * y + s2;
                s2 = f.b2 * x - f.a2 * y;
                sample = (float) y;
            }
            state[(size_t) k] = { s1, s2 };
        }
    }

    int farPixels (const juce::Image& a, const juce::Image& b, int levels)
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

    void fillSong (Studio& studio, int tracks, int notesEach)
    {
        for (int t = 1; t < tracks; ++t)
            studio.addTrack ({});
        for (int t = 0; t < tracks; ++t)
        {
            Part part;
            part.start = (double) (t % 3) * 2.0;
            part.length = 120.0;
            part.name = "Part " + juce::String (t + 1);
            for (int i = 0; i < notesEach; ++i)
            {
                Note n;
                n.start = i * 120.0 / notesEach;
                n.length = 0.2;
                n.pitch = 36 + (i * 7 + t) % 48;
                n.velocity = 0.8f;
                part.notes.push_back (n);
            }
            studio.addPart (studio.getProject().tracks[(size_t) t].id, part);
        }
    }
}

TEST_CASE ("0.165.0: the EQ's stages run together give what they gave one by one, to a float's rounding", "[speed165][eq]")
{
    ChannelEq eq;
    for (auto& band : eq.bands)
    {
        band.on = true;
        band.gainDb = 6.0;
        band.q = 4.0;
    }
    eq.bands[0].type = EqBand::Type::lowCut;
    eq.bands[0].slope = 48;
    const auto design = designEq (eq, 48000.0);
    REQUIRE (design.numEq >= 6);   // (more than four stages: two groups)
    EqProcessor now;
    now.setDesign (design);
    std::vector<std::array<double, 2>> state;
    juce::Random random (3);
    auto worst = 0.0f;
    for (int block = 0; block < 8; ++block)
    {
        const auto n = block % 2 == 0 ? 2048 : 333;   // (longer than the 256-sample pieces, and not a multiple)
        std::vector<float> a ((size_t) n);
        for (auto& x : a)
            x = random.nextFloat() * 2.0f - 1.0f;
        auto b = a;
        float* one[] = { a.data() };
        now.processEq (one, 1, n);
        oldCascade (design, b, state);
        for (int i = 0; i < n; ++i)
            worst = juce::jmax (worst, std::abs (a[(size_t) i] - b[(size_t) i]));
    }
    CHECK (worst < 1.0e-4f);   // (-80 dB under full scale: the old float steps' rounding, nothing more)
}

TEST_CASE ("0.165.0: parts are shared between copies until one is changed", "[speed165][parts]")
{
    Part one, two;
    one.start = 1.0;
    two.start = 2.0;
    PartList a { one, two };
    PartList b = a;
    CHECK (a.sharesWith (b));
    CHECK (a == b);
    // Reading never copies.
    const auto& readOnly = b;
    CHECK (readOnly[1].start == 2.0);
    CHECK (a.sharesWith (b));
    // Writing gives the writer its own: the other copy keeps what it had.
    b[0].start = 5.0;
    CHECK (! a.sharesWith (b));
    CHECK (a[0].start == 1.0);
    CHECK (b[0].start == 5.0);
    // An iterator taken from the shared contents still finds its place after the copy.
    PartList c = a;
    const auto& shared = c;
    c.erase (shared.begin() + 1);
    CHECK (c.size() == 1);
    CHECK (a.size() == 2);
    // A whole song copied (an undo step) shares every track's parts; the song's next edit copies only that track's.
    Studio studio (false);
    fillSong (studio, 3, 50);
    const auto& song = studio.getProject();
    const Project kept = song;
    for (size_t t = 0; t < song.tracks.size(); ++t)
        CHECK (kept.tracks[t].parts.sharesWith (song.tracks[t].parts));
    auto notes = song.tracks[1].parts[0].notes;
    notes[0].pitch = 100;
    studio.setNotes (song.tracks[1].id, 0, notes);
    CHECK (kept.tracks[0].parts.sharesWith (song.tracks[0].parts));
    CHECK (! kept.tracks[1].parts.sharesWith (song.tracks[1].parts));
    CHECK (kept.tracks[1].parts[0].notes[0].pitch != 100);
    // And undo brings it back exactly.
    studio.undo();
    CHECK (studio.getProject().tracks[1].parts[0].notes[0].pitch == kept.tracks[1].parts[0].notes[0].pitch);
}

TEST_CASE ("0.165.0: the arrangement kept as a picture while it scrolls draws what a full redraw draws", "[speed165][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    fillSong (studio, 4, 200);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.setAutomationOpen (trackId, true);
    studio.appendAutomationLane (trackId, 0);
    std::vector<AutomationPoint> points;
    for (int i = 0; i < 60; ++i)
        points.push_back ({ i * 2.1, 0.5f + 0.4f * (float) std::sin (i * 0.9) });
    studio.setAutomationPoints (trackId, 0, points);
    studio.addTempoTrack();
    studio.setTempoTrackOn (false);   // (its dashed line, and the BPM labels pinned at the right)
    MainView view (studio);
    view.setSize (1500, 900);
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto& timeline = view.getTimeline();

    for (const auto scale : { 1.0f, 2.0f, 1.5f })
    {
        const auto width = juce::roundToInt ((float) arrange.getWidth() * scale), height = juce::roundToInt ((float) arrange.getHeight() * scale);
        const auto render = [&] (bool fresh)
        {
            juce::Image image (juce::Image::RGB, width, height, true, juce::SoftwareImageType());
            juce::Graphics g (image);
            g.addTransform (juce::AffineTransform::scale (scale));
            if (fresh)
                arrange.paintFreshForTests (g);
            else
                arrange.paint (g);
            return image;
        };
        arrange.setScrollCacheForTests (true);
        timeline.firstBeat = 0.0;
        render (false);
        const auto ringColumns = (int) std::floor ((float) arrange.getWidth() * scale + 1.0e-3f);   // (whole screen pixels)
        CHECK (arrange.getScrollCacheColumnsDrawn() == ringColumns);   // (the first: all of it)
        const auto step = arrange.scrollStep();
        REQUIRE (step > 0.0);
        for (const auto steps : { 7, 1, 30, 113 })
        {
            timeline.firstBeat += steps * step;
            const auto kept = render (false);
            // Only the strip the scroll uncovered was drawn.
            CHECK (arrange.getScrollCacheColumnsDrawn() == juce::roundToInt (steps * step * timeline.pixelsPerBeat * scale));
            const auto whole = render (true);
            if (const auto folder = juce::SystemStats::getEnvironmentVariable ("PARAGON_CACHE_DIFF", {}); folder.isNotEmpty())
                for (const auto& [name, image] : { std::pair { "kept", &kept }, std::pair { "whole", &whole } })
                {
                    juce::FileOutputStream out (juce::File (folder).getChildFile (juce::String (name) + juce::String (scale) + "-" + juce::String (steps) + ".png"));
                    out.setPosition (0);
                    out.truncate();
                    juce::PNGImageFormat().writeImageToStream (*image, out);
                }
            CHECK (farPixels (kept, whole, 24) == 0);
            CHECK (farPixels (kept, whole, 2) < width * height / 2000);   // (a cut edge's smoothing, at most a shade)
        }
        // A redraw asked for in a stretch draws those columns again (here: the selection lit).
        arrange.repaint (juce::Rectangle<int> (200, 0, 30, arrange.getHeight()));
        render (false);
        CHECK (arrange.getScrollCacheColumnsDrawn() > 0);
        CHECK (arrange.getScrollCacheColumnsDrawn() < width / 4);
        // An edit redraws all of it, and shows it.
        auto moved = studio.getProject().tracks[2].parts[0];
        moved.start += 4.0;
        studio.updatePart (studio.getProject().tracks[2].id, 0, moved);
        studio.dispatchPendingMessages();
        const auto edited = render (false);
        CHECK (arrange.getScrollCacheColumnsDrawn() == ringColumns);
        CHECK (farPixels (edited, render (true), 24) == 0);
        studio.undo();
        studio.dispatchPendingMessages();
    }
    // Not scrolling under the playhead (stopped): drawn as always, the picture let go.
    arrange.setScrollCacheForTests ({});
    juce::Image image (juce::Image::RGB, arrange.getWidth(), arrange.getHeight(), true, juce::SoftwareImageType());
    juce::Graphics g (image);
    arrange.paint (g);
    CHECK (arrange.getScrollCacheColumnsDrawn() == 0);
}

TEST_CASE ("0.165.0: timings", "[.perf-165]")
{
    using clock = std::chrono::steady_clock;
    const auto ms = [] (clock::time_point a, clock::time_point b) { return std::chrono::duration<double, std::milli> (b - a).count(); };
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    fillSong (studio, 30, 2000);
    MainView view (studio);
    view.setSize (5760, 1000);
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto& timeline = view.getTimeline();
    juce::Image image (juce::Image::RGB, arrange.getWidth(), arrange.getHeight(), true, juce::SoftwareImageType());
    for (const auto kept : { false, true })
    {
        arrange.setScrollCacheForTests (kept);
        timeline.firstBeat = 0.0;
        { juce::Graphics g (image); arrange.paint (g); }
        const auto t0 = clock::now();
        for (int frame = 0; frame < 60; ++frame)
        {
            timeline.firstBeat += 3.0 / timeline.pixelsPerBeat;
            juce::Graphics g (image);
            arrange.paint (g);
        }
        std::printf ("A frame of Stationary Cursor at 5760 wide, %s: %.2f ms\n", kept ? "kept as a picture" : "drawn whole", ms (t0, clock::now()) / 60.0);
    }
    // Undo steps: a note edited on one track of 30 (each step copies the song). Without the tests' guard (it copies
    // the last steps' notes to check them).
    const juce::ScopedValueSetter<bool> unguarded (Studio::guardUndoSharing, false);
    const auto id = studio.getProject().tracks[3].id;
    const auto t0 = clock::now();
    for (int i = 0; i < 50; ++i)
    {
        auto notes = studio.getProject().tracks[3].parts[0].notes;
        notes[(size_t) i].pitch = 60;
        studio.setNotes (id, 0, notes);
    }
    std::printf ("A note edit (an undo step) on 30 x 2000 notes: %.3f ms\n", ms (t0, clock::now()) / 50.0);
    const auto t1 = clock::now();
    for (int i = 0; i < 50; ++i)
        studio.setVolume (id, -6.0f - (float) i * 0.1f, true);
    std::printf ("A fader set (an undo step) on 30 x 2000 notes: %.3f ms\n", ms (t1, clock::now()) / 50.0);
}
