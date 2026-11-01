#include <catch2/catch_test_macros.hpp>

#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/MainView.h"

using namespace pr7s;

TEST_CASE ("Progress of long jobs: Render in Place reports its stages to the end; the box shows it (0.11.1)", "[progress]")
{
    Studio studio (false);
    const auto synth = studio.getProject().tracks[0].id;
    studio.addPart (synth, { 0.0, 8.0, "Riff", { { 0.0, 1.0, 60, 1.0f }, { 4.0, 1.0, 64, 1.0f } } });
    const auto other = studio.addTrack ({});
    studio.addPart (other, { 0.0, 4.0, "Pad", { { 0.0, 2.0, 67, 1.0f } } });

    std::vector<std::pair<double, juce::String>> reports;
    studio.onProgress = [&] (double p, const juce::String& what) { reports.push_back ({ p, what }); };
    Studio::RenderOptions options;
    options.tail = Studio::RenderOptions::Tail::off;
    juce::String error;
    const auto made = studio.renderInPlace ({ { synth, 0.0, 8.0, true }, { other, 0.0, 4.0, true } }, options, error);
    CHECK (made.size() == 2);
    REQUIRE (reports.size() > 3);
    CHECK (reports.front().first == 0.0);
    CHECK (reports.front().second.contains ("(1 of 2)"));
    CHECK (std::any_of (reports.begin(), reports.end(), [] (auto& r) { return r.second.contains ("(2 of 2)") && r.first >= 0.5; }));
    CHECK (reports.back().first < 0.0);   // over: the box goes
    // Never backwards while it runs.
    for (size_t i = 1; i + 1 < reports.size(); ++i)
        CHECK (reports[i].first >= reports[i - 1].first - 1.0e-9);
    for (const auto& r : reports)
        CHECK (r.first <= 1.0 + 1.0e-9);
    studio.onProgress = nullptr;
}

TEST_CASE ("The progress box over the studio: what's being done and how far (0.11.1)", "[progress][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    REQUIRE (studio.onProgress != nullptr);
    studio.onProgress (0.4, "Rendering KEYS (1 of 2)");
    REQUIRE (view.getProgressBox() != nullptr);
    CHECK (view.getProgressBox()->getProgress() == 0.4);
    CHECK (view.getProgressBox()->getWhat() == "Rendering KEYS (1 of 2)");
    studio.onProgress (-1.0, {});
    CHECK (view.getProgressBox() == nullptr);
}

TEST_CASE ("SAVE lights up, reading SAVED, when the song has been saved (0.11.1)", "[ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    CHECK_FALSE (view.isSaveLit());
    const auto file = juce::File::createTempFile (Project::fileExtension);
    studio.save (file);
    view.flashSaved();   // (what the save's answer calls once it's on the disk)
    CHECK (view.isSaveLit());
    file.deleteFile();
}

namespace
{
    juce::MouseEvent mouseOn (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt)
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                                 juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, now, downAt, now, 1, at != downAt);
    }
}

TEST_CASE ("The select box scrolls the view past its edges, and takes in parts that were off the screen (0.11.1)", "[ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto track = studio.getProject().tracks[0].id;
    studio.addPart (track, { 2.0, 2.0, "Near", {} });
    studio.addPart (track, { 200.0, 4.0, "Far", {} });   // well past the right edge
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto& timeline = view.getTimeline();
    timeline.firstBeat = 0.0;
    arrange.setTool (ArrangeView::Tool::select);

    // From an empty spot on the track, dragged out past the right edge and held there.
    const auto bounds = arrange.getPartBounds (track, 0);
    const juce::Point<float> from (timeline.xForBeat (1.0), bounds.getY() - 2.0f);
    arrange.mouseDown (mouseOn (arrange, from, from));
    const juce::Point<float> past ((float) arrange.getWidth() + 80.0f, bounds.getBottom() + 4.0f);
    for (int i = 0; i < 400 && timeline.beatForX ((float) arrange.getWidth()) < 205.0; ++i)
        arrange.mouseDrag (mouseOn (arrange, past, from));
    CHECK (timeline.firstBeat > 100.0);   // it scrolled along the song
    arrange.mouseUp (mouseOn (arrange, past, from));
    // Both parts: the one where it began (now off the screen) and the far one.
    CHECK (arrange.getSelectedParts().size() == 2);
}

TEST_CASE ("The mixer's own window: the strips wider, the insert and send slots twice as tall (0.11.1)", "[ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    auto& mixer = view.getMixer();
    view.openMixer();
    view.setMixerInWindow (true);
    REQUIRE (view.getMixerWindow() != nullptr);
    view.getMixerWindow()->setSize (1600, 1000);
    auto& strip = mixer.getStrip (0);
    CHECK (strip.getWidth() == MixerView::windowStripWidth);
    CHECK (strip.getInsertSlot (0).getHeight() > 24);
    CHECK (strip.getSendSlot (0).getHeight() > 24);
    view.setMixerInWindow (false);
    CHECK (mixer.getStrip (0).getWidth() == MixerView::stripWidth);
}
