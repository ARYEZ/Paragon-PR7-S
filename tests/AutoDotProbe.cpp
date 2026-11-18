#include <catch2/catch_test_macros.hpp>
#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/MainView.h"
using namespace pr7s;
TEST_CASE ("probe: automation dots", "[.dotprobe]")
{
    Finish::set (Finish::Type::pearlBlack, false);
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.setAutomationOpen (trackId, true);
    studio.appendAutomationLane (trackId, 0);
    std::vector<AutomationPoint> p;
    p.push_back ({ 2.0, 0.6f });
    p.push_back ({ 6.0, 0.6f }); p.push_back ({ 6.0, 0.6f }); p.push_back ({ 6.0, 0.6f });
    p.push_back ({ 10.0, 0.6f }); p.push_back ({ 10.01, 0.6f });
    p.push_back ({ 14.0, 0.6f }); p.push_back ({ 14.3, 0.6f });
    p.push_back ({ 18.0, 0.6f }); p.push_back ({ 18.0, 0.62f });
    studio.setAutomationPoints (trackId, 0, p);
    studio.setAutomationRead (trackId, true);
    MainView view (studio);
    view.setSize (1500, 900);
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    juce::Image image (juce::Image::RGB, arrange.getWidth() * 2, arrange.getHeight() * 2, true, juce::SoftwareImageType());
    { juce::Graphics g (image); g.addTransform (juce::AffineTransform::scale (2.0f)); arrange.paint (g); }
    juce::FileOutputStream out (juce::File ("/tmp/claude-0/-home-user/e34e6e16-74f2-5036-bf92-c4fe7f783f1b/scratchpad/dots.png"));
    out.setPosition (0); out.truncate();
    juce::PNGImageFormat().writeImageToStream (image, out);
}
