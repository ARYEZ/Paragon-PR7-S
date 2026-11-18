// 0.167.0 (the owner: some automation dots looked bigger and brighter than others): points on the
// same spot on screen are drawn as one dot, not stacked (their see-through halos added up).

#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/MainView.h"

using namespace pr7s;

TEST_CASE ("0.167.0: points stacked on one spot draw the same dot as a single point", "[autodots]")
{
    Finish::set (Finish::Type::pearlBlack, false);
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.setAutomationOpen (trackId, true);
    studio.appendAutomationLane (trackId, 0);
    std::vector<AutomationPoint> p;
    p.push_back ({ 2.0, 0.6f });                                                               // alone
    p.push_back ({ 6.0, 0.6f }); p.push_back ({ 6.0, 0.6f }); p.push_back ({ 6.0, 0.6f });     // three on one spot
    p.push_back ({ 10.0, 0.6f }); p.push_back ({ 10.01, 0.6f });                               // a hair apart
    p.push_back ({ 14.0, 0.6f }); p.push_back ({ 16.0, 0.6f });                                // apart: two dots
    studio.setAutomationPoints (trackId, 0, p);
    studio.setAutomationRead (trackId, true);
    MainView view (studio);
    view.setSize (1500, 900);
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto& timeline = view.getTimeline();
    timeline.firstBeat = 0.0;
    for (const auto scale : { 1.0f, 2.0f })
    {
        juce::Image image (juce::Image::RGB, juce::roundToInt (arrange.getWidth() * scale), juce::roundToInt (arrange.getHeight() * scale), true, juce::SoftwareImageType());
        {
            juce::Graphics g (image);
            g.addTransform (juce::AffineTransform::scale (scale));
            arrange.paint (g);
        }
        // The light in a box round each dot (the line through it is the same everywhere): before 0.167.0
        // three points on one spot gave about 6 % more than one.
        const auto lightAround = [&] (double beat)
        {
            const auto x = juce::roundToInt (timeline.xForBeat (beat) * scale);
            auto centre = 0, brightest = -1;
            for (int y = 0; y < image.getHeight(); ++y)
            {
                const auto c = image.getPixelAt (x, y);
                if (const auto b = c.getRed() + c.getGreen() + c.getBlue(); b > brightest)
                    brightest = b, centre = y;
            }
            const auto half = juce::roundToInt (8.0f * scale), tall = juce::roundToInt (11.0f * scale);
            juce::int64 sum = 0;
            for (int y = centre - tall; y <= centre + tall; ++y)
                for (int dx = -half; dx <= half; ++dx)
                {
                    const auto c = image.getPixelAt (x + dx, y);
                    sum += c.getRed() + c.getGreen() + c.getBlue();
                }
            return (double) sum;
        };
        const auto single = lightAround (2.0);
        CHECK (std::abs (lightAround (6.0) - single) < single * 0.02);
        CHECK (std::abs (lightAround (10.0) - single) < single * 0.02);
        CHECK (std::abs (lightAround (16.0) - single) < single * 0.02);   // (the second of two apart still drawn)
    }
}
