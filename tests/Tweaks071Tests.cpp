// 0.71.0: the owner's tweaks (the info line's double-click, the faders' half-dB steps, the themed
// windows and menus, removing what's missing, the docked zone's height).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/InfoLine.h"
#include "ui/MainView.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    // A real click through the window's peer: JUCE's own mouse handling (focus, double-clicks), as a person's.
    void peerClick (juce::Component& window, juce::Point<float> at, int64_t& time)
    {
        auto* peer = window.getPeer();
        REQUIRE (peer != nullptr);
        const auto type = juce::Desktop::getInstance().getMainMouseSource().getType();
        const auto down = juce::ModifierKeys().withFlags (juce::ModifierKeys::leftButtonModifier);
        const auto pressure = juce::MouseInputSource::defaultPressure;
        peer->handleMouseEvent (type, at, {}, pressure, juce::MouseInputSource::defaultOrientation, time += 20);
        peer->handleMouseEvent (type, at, down, pressure, juce::MouseInputSource::defaultOrientation, time += 20);
        peer->handleMouseEvent (type, at, {}, pressure, juce::MouseInputSource::defaultOrientation, time += 20);
    }
}

TEST_CASE ("0.71.0: a double-click on the info line's VALUE opens it for typing (the owner: only a point's double-click did)", "[ui][infoline][tweaks071]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    view.setVisible (true);
    view.addToDesktop (0);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.setAutomationOpen (trackId, true);
    studio.setAutomationPoints (trackId, 0, { { 4.0, volumeToTravel (-3.0f), 0.0f }, { 5.0, volumeToTravel (-3.0f), 0.0f } });
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto& info = view.getInfoLine();
    arrange.selectPoints (trackId, 0, { 0, 1 });
    info.refresh();
    REQUIRE (info.getField ("VALUE") == "-3.0 dB");
    const auto field = info.getFieldArea ("VALUE");
    REQUIRE (! field.isEmpty());
    const auto at = view.getLocalPoint (&info, field.getCentre()).toFloat();
    int64_t time = juce::Time::currentTimeMillis();
    peerClick (view, at, time);
    peerClick (view, at, time);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
    info.refresh();
    REQUIRE (info.getEditor() != nullptr);
    CHECK (info.getEditor()->getText() == "-3.0 dB");
    CHECK (info.getEditor()->hasKeyboardFocus (true));
}
