// Speed, round three (0.83.1): each change checked to give the same result as before.

#include <catch2/catch_test_macros.hpp>

#include <random>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/LevelMeter.h"
#include "ui/Pearl.h"

using namespace pr7s;

namespace
{
    struct Desk
    {
        Engine engine;
        Project project;
        Desk() { engine.prepare (48000.0, 512); }
        void addTrack (int id, float left, float right)
        {
            Track track;
            track.id = id;
            project.tracks.push_back (track);
            engine.setInstrument (id, std::make_unique<SteadyTone> (left, right));
            engine.update (project);
        }
    };
}

TEST_CASE ("0.83.1: a mix-only update sounds exactly as a full one", "[speed083][engine]")
{
    Desk light, full;
    for (auto* desk : { &light, &full })
    {
        desk->addTrack (1, 0.3f, 0.2f);
        desk->addTrack (2, 0.1f, 0.4f);
        renderSamples (desk->engine, 2048);
    }
    // A fader, a pan, an EQ band, a BOOSTER and the master's fader moved.
    for (auto* desk : { &light, &full })
    {
        auto& p = desk->project;
        p.tracks[0].volumeDb = -7.5f;
        p.tracks[1].pan = 0.4f;
        p.tracks[1].eq.bands[1].on = true;
        p.tracks[1].eq.bands[1].gainDb = 6.0f;
        p.tracks[0].boost.trimDb = 3.0f;
        p.masterVolumeDb = -2.0f;
    }
    light.engine.updateMix (light.project);
    full.engine.update (full.project);
    std::vector<float> lightRight, fullRight;
    const auto a = renderSamples (light.engine, 8192, 512, &lightRight);
    const auto b = renderSamples (full.engine, 8192, 512, &fullRight);
    CHECK (a == b);
    CHECK (lightRight == fullRight);
}

TEST_CASE ("0.83.1: a fader dragged in the studio moves the sound at once (the light update)", "[speed083][studio]")
{
    Studio studio (false);
    const auto id = studio.getProject().tracks.front().id;
    studio.getEngine().prepare (48000.0, 512);
    studio.setVolume (id, -12.0f, false);
    renderSamples (studio.getEngine(), 512);
    CHECK (studio.getEngine().getMixNow (id).volumeDb == -12.0f);
    studio.setVolume (id, -3.0f, true);   // the release: a full update and an undo step
    renderSamples (studio.getEngine(), 512);
    CHECK (studio.getEngine().getMixNow (id).volumeDb == -3.0f);
    CHECK (studio.canUndo());
}

TEST_CASE ("0.83.1: solo by routing gives the same tracks as walking each one", "[speed083][solo]")
{
    // The old rule, walked as it was (feedsInto / reaches per pair), against the new one-walk version.
    std::mt19937 random (12345);
    for (int round = 0; round < 200; ++round)
    {
        Project p;
        const auto count = 3 + (int) (random() % 10);
        for (int i = 0; i < count; ++i)
        {
            Track t;
            t.id = i + 1;
            const auto kind = random() % 4;
            t.kind = kind == 0 ? Track::Kind::group : kind == 1 ? Track::Kind::fx : Track::Kind::instrument;
            p.tracks.push_back (t);
        }
        for (auto& t : p.tracks)
        {
            t.output = random() % 3 == 0 ? 0 : (int) (random() % (unsigned) count) + 1;
            if (random() % 3 == 0)
                t.sends.push_back ({ (int) (random() % (unsigned) count) + 1, 0.0f, true, false });
            t.solo = random() % 4 == 0;
            t.disabled = random() % 9 == 0;
        }
        std::vector<int> soloed;
        for (const auto& t : p.tracks)
            if (t.solo && ! t.disabled)
                soloed.push_back (t.id);
        std::vector<int> expected;
        if (! soloed.empty())
            for (const auto& t : p.tracks)
            {
                auto heard = false;
                for (const auto s : soloed)
                {
                    const auto* st = p.findTrack (s);
                    heard = t.id == s || p.feedsInto (t.id, s) || (st->isFx() && p.reaches (t.id, s)) || p.reaches (s, t.id);
                    if (heard)
                        break;
                }
                if (! heard)
                    expected.push_back (t.id);
            }
        REQUIRE (p.soloedOutTracks() == expected);
    }
}

TEST_CASE ("0.83.1: meters redraw only when a segment changes", "[speed083][ui]")
{
    LevelMeter::Ballistics reading;
    reading.push (0.5f);
    const auto first = reading.shown (90.0f);
    // A falling level shows the same segments for a few frames (a track-list meter: segments of about
    // 2 dB; the fall is 0.8 dB a frame. A tall mixer meter has 1 dB segments: fewer frames are spared).
    auto same = 0, before = first;
    for (int i = 0; i < 10; ++i)
    {
        reading.push (0.0f);
        const auto now = reading.shown (90.0f);
        same += now == before ? 1 : 0;   // (a frame that needn't be drawn again)
        before = now;
    }
    CHECK (same > 0);
    reading.push (1.0f);
    CHECK (reading.shown (90.0f) != first);
}

TEST_CASE ("0.83.1: the pearl unit drawn from its kept picture looks as drawn afresh", "[speed083][ui]")
{
    const auto satin = Finish::makeSatinGrain (800, 200, 7071);
    for (const auto scale : { 1.0f, 2.0f })
    {
        const auto w = (int) (400 * scale), h = (int) (100 * scale);
        juce::Image direct (juce::Image::ARGB, w, h, true, juce::SoftwareImageType()), kept (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
        const auto area = juce::Rectangle<float> (4.0f, 4.0f, 392.0f, 92.0f);
        {
            juce::Graphics g (direct);
            g.addTransform (juce::AffineTransform::scale (scale));
            Pearl::paintUnit (g, area, satin);
        }
        Pearl::UnitCache cache;
        for (int twice = 0; twice < 2; ++twice)
        {
            kept.clear (kept.getBounds());
            juce::Graphics g (kept);
            g.addTransform (juce::AffineTransform::scale (scale));
            Pearl::paintUnit (g, area, satin, cache);
        }
        auto worst = 0;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const auto a = direct.getPixelAt (x, y), b = kept.getPixelAt (x, y);
                worst = juce::jmax (worst, std::abs (a.getRed() - b.getRed()), std::abs (a.getGreen() - b.getGreen()),
                                    juce::jmax (std::abs (a.getBlue() - b.getBlue()), std::abs (a.getAlpha() - b.getAlpha())));
            }
        CHECK (worst <= 3);
    }
}
