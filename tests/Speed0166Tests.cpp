// The MIDI editor kept as a picture while Stationary Cursor scrolls it, and its notes' glow (0.166.0).

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/KeyEditor.h"
#include "ui/MainView.h"

using namespace pr7s;

namespace
{
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

    struct Song
    {
        Studio studio { false };
        MainView view { studio };
        int lead = 0;
        explicit Song (int notes = 300)
        {
            Finish::set (Finish::Type::pearlWhite, false);
            view.setSize (1500, 900);
            view.setKeyEditorSize (100);
            lead = studio.getProject().tracks[0].id;
            Part part;
            part.length = 96.0;
            part.name = "Lead";
            for (int i = 0; i < notes; ++i)
                part.notes.push_back ({ i * 96.0 / notes, 0.25, 48 + (i * 5) % 30, 0.3f + 0.7f * (float) ((i * 37) % 100) / 100.0f });
            for (int i = 0; i < 200; ++i)
                part.controllers.push_back ({ Controller::Type::cc, 1, i * 0.48, (i * 9) % 128, true });
            studio.addPart (lead, part);
            Chord am { 9 };
            am.type = Chord::Type::minor;
            studio.setChordEvents ({ { 0.0, Chord { 0 }, 4.0 }, { 4.0, am, 4.0 }, { 8.0, Chord { 5 }, 8.0 } });
            studio.setControllerLanes (lead, { -1, 1 });
            studio.dispatchPendingMessages();
            view.getArrangeView().select (lead, 0);
            view.openKeyEditor();
            studio.dispatchPendingMessages();
        }
        KeyEditor& editor() { return view.getKeyEditor(); }
    };
}

TEST_CASE ("0.166.0: the MIDI editor kept as a picture while it scrolls draws what a full redraw draws", "[speed166][ui]")
{
    Song song;
    auto& editor = song.editor();
    REQUIRE (editor.getPart() != nullptr);
    REQUIRE (editor.getWidth() > 600);
    for (const auto scale : { 1.0f, 2.0f, 1.5f })
    {
        const auto width = juce::roundToInt ((float) editor.getWidth() * scale), height = juce::roundToInt ((float) editor.getHeight() * scale);
        const auto render = [&] (bool fresh)
        {
            juce::Image image (juce::Image::RGB, width, height, true, juce::SoftwareImageType());
            juce::Graphics g (image);
            g.addTransform (juce::AffineTransform::scale (scale));
            if (fresh)
                editor.paintFreshForTests (g);
            else
                editor.paint (g);
            return image;
        };
        editor.setScrollCacheForTests (true);
        editor.setFirstBeatForTests (0.0);
        render (false);
        CHECK (editor.getScrollCacheColumnsDrawn() > width / 2);   // (the first: all the part that scrolls)
        const auto step = editor.scrollStep();
        REQUIRE (step > 0.0);
        for (const auto steps : { 5, 1, 24, 90 })
        {
            editor.setFirstBeatForTests (editor.getFirstBeat() + steps * step);
            const auto kept = render (false);
            // Only the strip the scroll uncovered.
            CHECK (editor.getScrollCacheColumnsDrawn() == juce::roundToInt (steps * step * editor.getPixelsPerBeat() * scale));
            const auto whole = render (true);
            if (const auto folder = juce::SystemStats::getEnvironmentVariable ("PARAGON_CACHE_DIFF", {}); folder.isNotEmpty())
                for (const auto& [name, image] : { std::pair { "keptmidi", &kept }, std::pair { "wholemidi", &whole } })
                {
                    juce::FileOutputStream out (juce::File (folder).getChildFile (juce::String (name) + juce::String (scale) + "-" + juce::String (steps) + ".png"));
                    out.setPosition (0);
                    out.truncate();
                    juce::PNGImageFormat().writeImageToStream (*image, out);
                }
            CHECK (farPixels (kept, whole, 24) == 0);
            CHECK (farPixels (kept, whole, 2) < width * height / 2000);
        }
        // A redraw asked for over the notes draws those columns again; over the keys, none.
        editor.repaint (juce::Rectangle<int> (editor.getWidth() - 300, 0, 30, editor.getHeight()));
        render (false);
        CHECK (editor.getScrollCacheColumnsDrawn() > 0);
        CHECK (editor.getScrollCacheColumnsDrawn() < width / 4);
        editor.repaint (juce::Rectangle<int> (0, 0, 20, editor.getHeight()));
        render (false);
        CHECK (editor.getScrollCacheColumnsDrawn() == 0);
        // An edit redraws all of it, and shows it.
        auto notes = editor.getPart()->notes;
        notes[3].pitch += 2;
        song.studio.setNotes (song.lead, 0, notes);
        song.studio.dispatchPendingMessages();
        const auto edited = render (false);
        CHECK (editor.getScrollCacheColumnsDrawn() > width / 2);
        CHECK (farPixels (edited, render (true), 24) == 0);
        song.studio.undo();
        song.studio.dispatchPendingMessages();
    }
    // Stopped: drawn as always, the picture let go.
    editor.setScrollCacheForTests ({});
    juce::Image image (juce::Image::RGB, editor.getWidth(), editor.getHeight(), true, juce::SoftwareImageType());
    juce::Graphics g (image);
    editor.paint (g);
    CHECK (editor.getScrollCacheColumnsDrawn() == 0);
}

TEST_CASE ("0.166.0: the MIDI editor's notes glow in their own colour", "[speed166][ui]")
{
    Song song (1);
    auto& editor = song.editor();
    editor.setChordGuides (false);
    auto notes = editor.getPart()->notes;
    notes[0] = { 2.0, 1.0, 64, 1.0f };   // (HIGH: red)
    song.studio.setNotes (song.lead, 0, notes);
    song.studio.dispatchPendingMessages();
    juce::Image image (juce::Image::RGB, editor.getWidth(), editor.getHeight(), true, juce::SoftwareImageType());
    {
        juce::Graphics g (image);
        editor.paintFreshForTests (g);
    }
    // Just outside the note's edge (above it, in the middle): lit by its red, not the plain row.
    const auto bounds = editor.getNoteBounds (0);
    REQUIRE (! bounds.isEmpty());
    const auto glow = image.getPixelAt ((int) bounds.getCentreX(), (int) bounds.getY() - 2);
    const auto row = image.getPixelAt ((int) bounds.getCentreX(), (int) bounds.getY() - 12);
    CHECK (glow.getRed() > row.getRed() + 15);
    CHECK (glow.getRed() > glow.getBlue() + 10);   // (red, not grey or blue: the family's colours)
}

TEST_CASE ("0.166.0: timings", "[.perf-166]")
{
    using clock = std::chrono::steady_clock;
    Song song (2000);
    song.view.setSize (5760, 1000);
    song.studio.dispatchPendingMessages();
    auto& editor = song.editor();
    juce::Image image (juce::Image::RGB, editor.getWidth() * 2, editor.getHeight() * 2, true, juce::SoftwareImageType());
    for (const auto kept : { false, true })
    {
        editor.setScrollCacheForTests (kept);
        editor.setFirstBeatForTests (0.0);
        const auto paintOnce = [&] { juce::Graphics g (image); g.addTransform (juce::AffineTransform::scale (2.0f)); editor.paint (g); };
        paintOnce();
        const auto t0 = clock::now();
        for (int frame = 0; frame < 60; ++frame)
        {
            editor.setFirstBeatForTests (editor.getFirstBeat() + 2.0 / editor.getPixelsPerBeat());
            paintOnce();
        }
        std::printf ("A MIDI editor frame of Stationary Cursor at 5760 wide (200 %%), %s: %.2f ms\n", kept ? "kept as a picture" : "drawn whole",
                     std::chrono::duration<double, std::milli> (clock::now() - t0).count() / 60.0);
    }
}
