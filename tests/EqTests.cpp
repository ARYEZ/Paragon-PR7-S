#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"

// 0.43.0, the channel EQ and Pre section.

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double rate = 48000.0;
    using T = EqBand::Type;

    ChannelEq oneBand (int index, T type, double gain, double frequency, double q = 1.0)
    {
        ChannelEq e;
        e.bands[(size_t) index] = { true, type, gain, frequency, q };
        return e;
    }

    double response (const ChannelEq& e, double f) { return eqResponseDb (designEq (e, rate), f, rate); }

    // A sine through a processor's EQ: its level afterwards (dB, the second half: settled).
    double sineThrough (const ChannelEq& e, double frequency)
    {
        EqProcessor p;
        p.setDesign (designEq (e, rate));
        const int n = 48000;
        std::vector<float> left ((size_t) n), right ((size_t) n);
        for (int i = 0; i < n; ++i)
            left[(size_t) i] = right[(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * frequency * i / rate) * 0.25f;
        float* channels[] { left.data(), right.data() };
        for (int at = 0; at < n; at += 512)
        {
            float* block[] { channels[0] + at, channels[1] + at };
            p.processPre (block, 2, juce::jmin (512, n - at));
            p.processEq (block, 2, juce::jmin (512, n - at));
        }
        auto peak = 0.0f;
        for (int i = n / 2; i < n; ++i)
            peak = juce::jmax (peak, std::abs (left[(size_t) i]));
        return juce::Decibels::gainToDecibels (peak / 0.25f);
    }
}

TEST_CASE ("0.43.0: the bands' curves: parametric, shelves, cuts; the Pre's cuts by slope", "[eq]")
{
    // A parametric +6 dB at 1 kHz: +6 there, nothing far away.
    const auto bell = oneBand (1, T::parametric, 6.0, 1000.0, 1.0);
    CHECK_THAT (response (bell, 1000.0), WithinAbs (6.0, 0.01));
    CHECK (std::abs (response (bell, 50.0)) < 0.1);
    CHECK (std::abs (response (bell, 18000.0)) < 0.2);
    // A higher Q is narrower: less at an octave away.
    CHECK (response (oneBand (1, T::parametric, 6.0, 1000.0, 4.0), 2000.0) < response (bell, 2000.0) - 1.0);
    // Shelves: +6 all the way out, nothing on the far side.
    const auto low = oneBand (0, T::lowShelf, 6.0, 200.0, 0.71), high = oneBand (3, T::highShelf, -6.0, 5000.0, 0.71);
    CHECK_THAT (response (low, 20.0), WithinAbs (6.0, 0.1));
    CHECK (std::abs (response (low, 10000.0)) < 0.1);
    CHECK_THAT (response (high, 20000.0), WithinAbs (-6.0, 0.2));
    // Band 1 as a cut: -3 dB at its frequency (Butterworth Q), falling 12 dB an octave.
    const auto bandCut = oneBand (0, T::lowCut, 0.0, 200.0, 0.7071);
    CHECK_THAT (response (bandCut, 200.0), WithinAbs (-3.0, 0.05));
    CHECK_THAT (response (bandCut, 25.0), WithinAbs (-36.0, 0.6));   // (three octaves down)

    // The Pre's low cut at 100 Hz: -3 dB there; two octaves down, about 2 x the slope.
    for (const auto slope : ChannelEq::slopes())
    {
        ChannelEq e;
        e.lowCutOn = true;
        e.lowCut = 100.0;
        e.lowCutSlope = slope;
        CHECK_THAT (response (e, 100.0), WithinAbs (-3.0, 0.1));
        CHECK_THAT (response (e, 25.0), WithinAbs (-2.0 * slope, slope <= 6 ? 0.6 : 0.4));
        CHECK (std::abs (response (e, 5000.0)) < 0.05);
    }
    // The high cut: 50 Hz at its least.
    ChannelEq hc;
    hc.highCutOn = true;
    hc.highCut = 10.0;
    CHECK (hc.limited().highCut == ChannelEq::minHighCut);
    hc.highCut = 1000.0;
    hc.highCutSlope = 24;
    CHECK_THAT (response (hc, 1000.0), WithinAbs (-3.0, 0.1));
    CHECK_THAT (response (hc, 4000.0), WithinAbs (-48.0, 1.0));   // (two octaves up: 2 x 24)
    CHECK (response (hc, 16000.0) < -90.0);   // (near the top a digital filter cuts more than the analog figure)

    // Bypass: the bands out; the Pre still in.
    auto both = bell;
    both.preGainDb = -6.0;
    both.bypass = true;
    CHECK_THAT (response (both, 1000.0), WithinAbs (-6.0, 0.01));
    both.bypass = false;
    CHECK_THAT (response (both, 1000.0), WithinAbs (0.0, 0.02));
    CHECK_THAT (eqResponseDb (designEq (both, rate), 1000.0, rate, false), WithinAbs (6.0, 0.02));   // (the EQ alone)
}

TEST_CASE ("0.43.0: sound through the EQ: a sine boosted and cut as the curve says; a turned knob keeps the filter's memory", "[eq]")
{
    CHECK_THAT (sineThrough (oneBand (1, T::parametric, 6.0, 1000.0), 1000.0), WithinAbs (6.0, 0.1));
    CHECK_THAT (sineThrough (oneBand (2, T::parametric, -12.0, 3000.0), 3000.0), WithinAbs (-12.0, 0.15));
    ChannelEq cutter;
    cutter.lowCutOn = true;
    cutter.lowCut = 400.0;
    cutter.lowCutSlope = 48;
    CHECK (sineThrough (cutter, 100.0) < -90.0);
    CHECK_THAT (sineThrough (cutter, 5000.0), WithinAbs (0.0, 0.1));
    ChannelEq flipped;
    flipped.phaseInvert = true;
    flipped.preGainDb = 6.0;
    CHECK_THAT (sineThrough (flipped, 440.0), WithinAbs (6.0, 0.05));
    CHECK (flipped.preActive());
    CHECK ((ChannelEq {}).isFlat());
}

TEST_CASE ("0.43.0: in the engine: Pre, then the inserts before the fader, then the EQ, then the fader; the master too", "[eq][engine]")
{
    Engine engine;
    engine.prepare (48000.0, 512);
    Project project;
    Track track;
    track.id = 1;
    project.tracks.push_back (track);
    engine.setInstrument (1, std::make_unique<SteadyTone> (0.25f, 0.25f));
    engine.insertEffect (1, 0, plusEffect (0.1f));
    project.tracks[0].inserts.resize (1);
    // Pre +6 dB (x2), the insert +0.1, a low shelf +6 dB (x2 at DC): ((0.25 x 2) + 0.1) x 2 = 1.2.
    auto& eq = project.tracks[0].eq;
    eq.preGainDb = juce::Decibels::gainToDecibels (2.0);
    eq.bands[0] = { true, T::lowShelf, juce::Decibels::gainToDecibels (2.0), 1000.0, 0.71 };
    engine.update (project);
    const auto level = [&engine] { return renderSamples (engine, 9600, 512).back(); };
    CHECK_THAT (level(), WithinAbs (1.2, 1.0e-3));
    // Phase inverted: the sign of what's before the insert flips: (-0.5 + 0.1) x 2 = -0.8.
    eq.phaseInvert = true;
    engine.update (project);
    CHECK_THAT (level(), WithinAbs (-0.8, 1.0e-3));
    // A low cut takes the steady level (DC) away before the insert: 0.1 x 2 = 0.2.
    eq.phaseInvert = false;
    eq.lowCutOn = true;
    eq.lowCut = 40.0;
    engine.update (project);
    CHECK_THAT (level(), WithinAbs (0.2, 2.0e-3));
    // The master: Pre -6 dB (x0.5) on the sum.
    eq = {};
    project.masterEq.preGainDb = juce::Decibels::gainToDecibels (0.5);
    engine.update (project);
    CHECK_THAT (level(), WithinAbs (0.175, 1.0e-3));
}

TEST_CASE ("0.43.0: the studio: set, undone, saved with the song, in track presets, as presets of its own; typed frequencies", "[eq][studio]")
{
    Studio studio (false);
    const auto id = studio.getProject().tracks[0].id;
    auto eq = *studio.getChannelEq (id);
    eq.bands[1] = { true, T::parametric, 4.5, 750.0, 2.0 };
    eq.lowCutOn = true;
    eq.lowCutSlope = 24;
    // A drag: steps not undoable, then the release.
    auto during = eq;
    during.bands[1].gainDb = 2.0;
    studio.setChannelEq (id, during, false);
    studio.setChannelEq (id, eq, true);
    CHECK (*studio.getChannelEq (id) == eq);
    CHECK_THAT (studio.getEqResponseDb (id, 750.0, false), WithinAbs (4.5, 0.05));
    studio.undo();
    CHECK (*studio.getChannelEq (id) == ChannelEq {});
    studio.redo();
    CHECK (*studio.getChannelEq (id) == eq);
    // The master's.
    auto master = *studio.getChannelEq (0);
    master.highCutOn = true;
    studio.setChannelEq (0, master);
    CHECK (studio.getProject().masterEq.highCutOn);

    // Saved and opened.
    const auto xml = studio.getProject().toXml();
    const auto back = Project::fromXml (*xml);
    REQUIRE (back);
    CHECK (back->findTrack (id)->eq == eq);
    CHECK (back->masterEq == master);

    // Track presets carry it.
    const auto preset = studio.makeTrackPreset (id);
    REQUIRE (preset != nullptr);
    const auto other = studio.addTrack ({});
    juce::String error;
    REQUIRE (studio.applyTrackPreset (other, *preset, error));
    CHECK (*studio.getChannelEq (other) == eq);

    // EQ presets.
    const auto file = juce::File::createTempFile (".pr7eq");
    REQUIRE (studio.saveEqPreset (id, file));
    REQUIRE (studio.loadEqPreset (0, file));
    CHECK (studio.getChannelEq (0)->bands == eq.bands);
    file.deleteFile();

    // Inverted: every gain negated.
    auto inverted = eq;
    inverted.invert();
    CHECK (inverted.bands[1].gainDb == -4.5);
    // Bands 2 and 3 only parametric.
    auto wrong = eq;
    wrong.bands[2].type = T::lowShelf;
    CHECK (wrong.limited().bands[2].type == T::parametric);

    // Frequencies typed as the manual: notes (A3 = 440 Hz), cents after a space, Hz and kHz.
    CHECK_THAT (*parseFrequency ("A3"), WithinAbs (440.0, 1.0e-9));
    CHECK_THAT (*parseFrequency ("A5 -23"), WithinAbs (1760.0 * std::pow (2.0, -0.23 / 12.0), 1.0e-6));
    CHECK_THAT (*parseFrequency ("C4 +49"), WithinAbs (523.2511 * std::pow (2.0, 0.49 / 12.0), 0.01));
    CHECK_THAT (*parseFrequency ("1.2k"), WithinAbs (1200.0, 1.0e-9));
    CHECK_THAT (*parseFrequency ("250 Hz"), WithinAbs (250.0, 1.0e-9));
    CHECK_THAT (*parseFrequency ("2.5 kHz"), WithinAbs (2500.0, 1.0e-9));
    CHECK_FALSE (parseFrequency ("loud").has_value());
    CHECK (frequencyText (440.0) == "440 Hz");
    CHECK (frequencyText (1200.0) == "1.20 kHz");
}

#include "ui/EqEditor.h"
#include "ui/Finish.h"

namespace
{
    juce::MouseEvent mouseOn (juce::Component& c, juce::Point<float> at, juce::Point<float> downAt, juce::ModifierKeys mods = {}, int clicks = 1)
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, mods.withFlags (juce::ModifierKeys::leftButtonModifier),
                                 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, downAt, now, clicks, at != downAt);
    }
}

TEST_CASE ("0.43.0: the curve by mouse: a click adds the nearest band there, a drag moves it (one undo step), keys limit it, double-click turns it off", "[eq][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    const auto id = studio.getProject().tracks[0].id;
    EqCurve curve (studio, id, false);
    curve.setSize (800, 300);
    const auto at = [&curve] (double hz, double db) { return juce::Point<float> (curve.xForFrequency (hz), curve.yForDb (db)); };

    // A click near 450 Hz at +6 dB: band 2 (500 Hz) is nearest, comes on there.
    const auto down = at (450.0, 6.0);
    curve.mouseDown (mouseOn (curve, down, down));
    curve.mouseUp (mouseOn (curve, down, down));
    auto eq = *studio.getChannelEq (id);
    REQUIRE (eq.bands[1].on);
    CHECK_THAT (eq.bands[1].frequency, WithinAbs (450.0, 5.0));
    CHECK_THAT (eq.bands[1].gainDb, WithinAbs (6.0, 0.15));
    CHECK_FALSE (eq.bands[0].on);
    // Dragged to 900 Hz, -3 dB: one undo step back to where the click left it.
    const auto point = curve.bandPoint (1);
    CHECK (curve.bandAt (point) == 1);
    curve.mouseDown (mouseOn (curve, point, point));
    curve.mouseDrag (mouseOn (curve, at (900.0, -3.0), point));
    curve.mouseUp (mouseOn (curve, at (900.0, -3.0), point));
    eq = *studio.getChannelEq (id);
    CHECK_THAT (eq.bands[1].frequency, WithinAbs (900.0, 10.0));
    CHECK_THAT (eq.bands[1].gainDb, WithinAbs (-3.0, 0.15));
    studio.undo();
    CHECK_THAT (studio.getChannelEq (id)->bands[1].frequency, WithinAbs (450.0, 5.0));
    // Ctrl: gain only; Alt: frequency only; Shift: Q (up: narrower).
    {
        const auto p = curve.bandPoint (1);
        curve.mouseDown (mouseOn (curve, p, p));
        curve.mouseDrag (mouseOn (curve, at (2000.0, 9.0), p, juce::ModifierKeys::commandModifier));
        curve.mouseUp (mouseOn (curve, at (2000.0, 9.0), p, juce::ModifierKeys::commandModifier));
        eq = *studio.getChannelEq (id);
        CHECK_THAT (eq.bands[1].frequency, WithinAbs (450.0, 5.0));
        CHECK_THAT (eq.bands[1].gainDb, WithinAbs (9.0, 0.15));
    }
    {
        const auto p = curve.bandPoint (1);
        curve.mouseDown (mouseOn (curve, p, p));
        curve.mouseDrag (mouseOn (curve, at (2000.0, -9.0), p, juce::ModifierKeys::altModifier));
        curve.mouseUp (mouseOn (curve, at (2000.0, -9.0), p, juce::ModifierKeys::altModifier));
        eq = *studio.getChannelEq (id);
        CHECK_THAT (eq.bands[1].frequency, WithinAbs (2000.0, 25.0));
        CHECK_THAT (eq.bands[1].gainDb, WithinAbs (9.0, 0.15));
    }
    {
        const auto p = curve.bandPoint (1);
        const auto q = studio.getChannelEq (id)->bands[1].q;
        curve.mouseDown (mouseOn (curve, p, p));
        curve.mouseDrag (mouseOn (curve, p.translated (0.0f, -40.0f), p, juce::ModifierKeys::shiftModifier));
        curve.mouseUp (mouseOn (curve, p.translated (0.0f, -40.0f), p, juce::ModifierKeys::shiftModifier));
        CHECK_THAT (studio.getChannelEq (id)->bands[1].q, WithinAbs (q * 2.0, 0.01));
    }
    // Double-click: off.
    const auto p = curve.bandPoint (1);
    curve.mouseDoubleClick (mouseOn (curve, p, p, {}, 2));
    CHECK_FALSE (studio.getChannelEq (id)->bands[1].on);
    CHECK (curve.createComponentSnapshot (curve.getLocalBounds()).isValid());
}

TEST_CASE ("0.43.0: the EQ editor's controls, each an undo step; the window follows another channel", "[eq][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    const auto id = studio.getProject().tracks[0].id;
    EqEditor editor (studio, id);
    editor.setBandGain (2, 5.0);
    editor.setBandFrequency (2, 3000.0);
    editor.setBandQ (2, 2.0);
    editor.setBandType (0, EqBand::Type::lowCut);
    editor.setLowCut (true, 60.0, 36);
    editor.setHighCut (true, 12000.0, 6);
    editor.setPreGain (-2.0);
    editor.setPhase (true);
    auto eq = *studio.getChannelEq (id);
    CHECK (eq.bands[2].on);
    CHECK (eq.bands[2].gainDb == 5.0);
    CHECK (eq.bands[2].frequency == 3000.0);
    CHECK (eq.bands[0].type == EqBand::Type::lowCut);
    CHECK (eq.lowCutSlope == 36);
    CHECK (eq.highCutSlope == 6);
    CHECK (eq.phaseInvert);
    editor.invert();
    CHECK (studio.getChannelEq (id)->bands[2].gainDb == -5.0);
    studio.undo();
    CHECK (studio.getChannelEq (id)->bands[2].gainDb == 5.0);
    editor.setBypass (true);
    CHECK (studio.getChannelEq (id)->bypass);
    editor.reset();
    CHECK (*studio.getChannelEq (id) == ChannelEq {});
    editor.setBus (0);
    editor.setBandOn (3, true);
    CHECK (studio.getProject().masterEq.bands[3].on);
    CHECK (EqEditor::channelName (studio, 0) == "MASTER");
    editor.setSize (EqEditor::width, EqEditor::height);
    CHECK (editor.createComponentSnapshot (editor.getLocalBounds()).isValid());
}
