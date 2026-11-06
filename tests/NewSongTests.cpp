#include <catch2/catch_test_macros.hpp>

#include "engine/Studio.h"
#include "ui/MainView.h"
#include "ui/Finish.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/Windows.h"

// (0.60.0) The owner's report: NEW froze the studio every time (the screen's thread stuck in the graphics driver
// while drawing, just after the new song was set up). A new song drawn whole, at the owner's three screens' width.

using namespace pr7s;

namespace
{
    void listHuge (juce::Component& c, juce::StringArray& out, int depth = 0)
    {
        if (c.getWidth() > 16384 || c.getHeight() > 16384 || c.getWidth() < 0 || c.getHeight() < 0)
            out.add (juce::String::repeatedString ("  ", depth) + typeid (c).name() + " " + c.getBounds().toString());
        for (auto* child : c.getChildren())
            listHuge (*child, out, depth + 1);
    }
}

TEST_CASE ("NEW: a new song is drawn as quickly as any other, nothing in it drawn oversized (0.60.0)", "[ui][newsong]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        Studio studio (false);
        MainView view (studio);
        view.setSize (5760, 1000);
        // A song with parts, the MIDI editor open on one, as the owner's was.
        const auto track = studio.getProject().tracks.front().id;
        Part part;
        part.start = 0.0;
        part.length = 16.0;
        for (int i = 0; i < 32; ++i)
            part.notes.push_back ({ i * 0.5, 0.4, 60 + i % 12, 0.8f });
        studio.addPart (track, part);
        studio.dispatchPendingMessages();
        const auto before = juce::Time::getMillisecondCounterHiRes();
        view.createComponentSnapshot (view.getLocalBounds(), true, 1.0f);
        const auto songMs = juce::Time::getMillisecondCounterHiRes() - before;

        studio.newProject();
        studio.dispatchPendingMessages();
        juce::StringArray huge;
        listHuge (view, huge);
        INFO (huge.joinIntoString ("\n"));
        CHECK (huge.isEmpty());
        const auto start = juce::Time::getMillisecondCounterHiRes();
        view.createComponentSnapshot (view.getLocalBounds(), true, 1.0f);
        const auto newMs = juce::Time::getMillisecondCounterHiRes() - start;
        INFO ("song " << songMs << " ms, new " << newMs << " ms");
        CHECK (newMs < juce::jmax (2000.0, songMs * 4.0));
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

TEST_CASE ("SETTINGS' DRAW WITHOUT THE GRAPHICS CARD: kept, and every window follows it (0.60.0)", "[ui][newsong]")
{
    Studio studio (false);
    CHECK_FALSE (studio.getSoftwareDrawing());
    studio.setSoftwareDrawing (true);
    CHECK (studio.getSoftwareDrawing());
    // A window on screen: switched where the system offers both ways (Windows), left alone where there's one.
    juce::Component window;
    window.setSize (200, 100);
    window.addToDesktop (0);
    window.setVisible (true);
    auto* peer = window.getPeer();
    REQUIRE (peer != nullptr);
    const auto engines = peer->getAvailableRenderingEngines();
    const auto switched = Windows::applyDrawing (true);
    if (engines.size() < 2)
        CHECK (switched == 0);
    else
        CHECK (engines[peer->getCurrentRenderingEngine()] == "Software Renderer");
    CHECK (Windows::applyDrawing (true) == 0);   // (already so: nothing to do)
    window.removeFromDesktop();
    studio.setSoftwareDrawing (false);
}
