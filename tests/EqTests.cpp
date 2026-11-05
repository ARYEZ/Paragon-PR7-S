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
    // Cuts only on bands 1 and 4 (0.46.0: shelves and the notch on all four).
    auto wrong = eq;
    wrong.bands[2].type = T::lowCut;
    CHECK (wrong.limited().bands[2].type == T::parametric);
    wrong.bands[2].type = T::lowShelf;
    CHECK (wrong.limited().bands[2].type == T::lowShelf);

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

//==============================================================================
// 0.46.0: the EQ as the other DAW's Frequency 2, four bands of it.

TEST_CASE ("0.46.0: band shapes: cuts 6 to 96 dB an octave, Q as a cut's resonance and a shelf's corner, Notch, Invert Gain, Output", "[eq]")
{
    // Cuts on band 1: an octave under the frequency, about the slope down (Butterworth: -3 dB at it).
    for (const auto slope : ChannelEq::bandSlopes())
    {
        auto e = oneBand (0, T::lowCut, 0.0, 400.0, 0.70710678);
        e.bands[0].slope = slope;
        INFO ("slope " << slope);
        CHECK_THAT (response (e, 400.0), WithinAbs (-3.01, 0.05));
        CHECK_THAT (response (e, 200.0), WithinAbs (-(double) slope, slope <= 12 ? 1.1 : 0.5));   // (Butterworth: 10 log10 (1 + 4^n))
        CHECK (std::abs (response (e, 8000.0)) < 0.05);
    }
    // A high cut on band 4, 48 dB.
    auto high = oneBand (3, T::highCut, 0.0, 1000.0, 0.70710678);   // (low enough not to be squeezed near half the rate)
    high.bands[3].slope = 48;
    CHECK_THAT (response (high, 2000.0), WithinAbs (-48.0, 0.5));
    // Q over 0.71 on a cut: a resonance at its frequency (Q 2 on Cut 12: about +9 dB).
    auto resonant = oneBand (0, T::lowCut, 0.0, 400.0, 2.0);
    CHECK_THAT (response (resonant, 400.0), WithinAbs (juce::Decibels::gainToDecibels (2.0), 0.05));
    // A song's cut from 0.43.0 (12 dB, its Q) sounds as it did: one biquad, the same curve.
    const auto old = oneBand (0, T::lowCut, 0.0, 150.0, 1.3);
    CHECK_THAT (response (old, 150.0), WithinAbs (juce::Decibels::gainToDecibels (1.3), 0.02));
    // A shelf with Q over 0.71: a dip past its corner (a low shelf boost dips above it).
    const auto plain = oneBand (0, T::lowShelf, 9.0, 300.0, 0.71), shaped = oneBand (0, T::lowShelf, 9.0, 300.0, 3.0);
    auto lowest = [&] (const ChannelEq& e) { auto m = 100.0; for (auto f = 300.0; f < 3000.0; f *= 1.05) m = juce::jmin (m, response (e, f)); return m; };
    CHECK (lowest (plain) > -0.1);
    CHECK (lowest (shaped) < -1.0);
    // Notch: the same gain, much narrower than a Peak.
    const auto peak = oneBand (1, T::parametric, -12.0, 1000.0, 1.0), notch = oneBand (1, T::notch, -12.0, 1000.0, 1.0);
    CHECK_THAT (response (notch, 1000.0), WithinAbs (-12.0, 0.01));
    CHECK (response (notch, 1200.0) > response (peak, 1200.0) + 6.0);
    // Invert Gain: +6 plays as -6; the point is drawn there too.
    auto inverted = oneBand (1, T::parametric, 6.0, 1000.0, 1.0);
    inverted.bands[1].invert = true;
    CHECK_THAT (response (inverted, 1000.0), WithinAbs (-6.0, 0.01));
    // Output: everything up or down; nothing when bypassed.
    auto loud = oneBand (1, T::parametric, 6.0, 1000.0, 1.0);
    loud.outputDb = -4.0;
    CHECK_THAT (response (loud, 1000.0), WithinAbs (2.0, 0.01));
    CHECK_THAT (response (loud, 50.0), WithinAbs (-4.0, 0.05));
    CHECK_THAT (sineThrough (loud, 1000.0), WithinAbs (2.0, 0.1));
    loud.bypass = true;
    CHECK (std::abs (response (loud, 1000.0)) < 0.01);
    // Types each band offers: cuts only on the outer two.
    CHECK (ChannelEq::typesFor (0).back() == T::lowCut);
    CHECK (ChannelEq::typesFor (3).back() == T::highCut);
    CHECK (ChannelEq::typesFor (1).size() == 4);
    // Saved and read back; a 0.43.0 song (no slope, invert, output) reads as 12 dB, not inverted, 0 dB.
    auto saved = oneBand (0, T::lowCut, 0.0, 80.0, 0.71);
    saved.bands[0].slope = 96;
    saved.bands[1] = { true, T::notch, -9.0, 3000.0, 2.0 };
    saved.bands[1].invert = true;
    saved.outputDb = 3.5;
    CHECK (ChannelEq::fromXml (*saved.toXml()) == saved);
    juce::XmlElement older ("Eq");
    auto* band = older.createNewChildElement ("Band");
    band->setAttribute ("on", true);
    band->setAttribute ("type", 3);
    band->setAttribute ("frequency", 120.0);
    band->setAttribute ("q", 0.9);
    const auto read = ChannelEq::fromXml (older);
    CHECK (read.bands[0].slope == 12);
    CHECK_FALSE (read.bands[0].invert);
    CHECK (read.outputDb == 0.0);
}

TEST_CASE ("0.46.0: the engine's spectrum tap and Auto Listen", "[eq][engine]")
{
    Engine engine;
    engine.prepare (48000.0, 512);
    Project project;
    Track track;
    track.id = 1;
    project.tracks.push_back (track);
    engine.setInstrument (1, std::make_unique<SteadyTone> (0.25f, 0.25f));
    engine.update (project);
    // The track's sound after its EQ, into the ring for the screen.
    engine.setSpectrumBus (1);
    renderSamples (engine, 4096, 512);
    std::vector<float> heard (8192, -1.0f);
    CHECK (engine.readSpectrum (heard) == 4096);
    CHECK_THAT (heard.back(), WithinAbs (0.25, 1.0e-4));
    CHECK_THAT (heard[0], WithinAbs (-1.0, 1.0e-6));   // (what was there before, moved along)
    CHECK (engine.readSpectrum (heard) == 0);           // (nothing new)
    engine.setSpectrumBus (-1);
    renderSamples (engine, 1024, 512);
    CHECK (engine.readSpectrum (heard) == 0);
    // Auto Listen on a Peak at 1 kHz: a band pass, so the steady level (DC) goes; back when cleared.
    EqBand peak { true, T::parametric, 6.0, 1000.0, 1.0 };
    engine.setListen (1, listenDesign (peak, 48000.0));
    CHECK (std::abs (renderSamples (engine, 9600, 512).back()) < 1.0e-3);
    engine.clearListen();
    CHECK_THAT (renderSamples (engine, 2048, 512).back(), WithinAbs (0.25, 1.0e-4));
    // A low shelf's listen: a low pass under it (DC through).
    EqBand shelf { true, T::lowShelf, 6.0, 200.0, 0.71 };
    engine.setListen (1, listenDesign (shelf, 48000.0));
    CHECK_THAT (renderSamples (engine, 9600, 512).back(), WithinAbs (0.25, 2.0e-3));
    const auto listen = listenDesign (shelf, 48000.0);
    CHECK (eqResponseDb (listen, 4000.0, 48000.0, false) < -40.0);
}

TEST_CASE ("0.46.0: the EQ window: types with cut slopes, INVERT, OUTPUT, the keyboard (a mark let go on a key), the wheel's Q, the spectrum", "[eq][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    const auto id = studio.getProject().tracks[0].id;
    EqEditor editor (studio, id);
    editor.setBounds (0, 0, EqEditor::width, EqEditor::height);
    editor.setKeysShown (true);
    editor.setSpectrumShown (true);
    editor.setBandCut (0, 48);
    auto eq = *studio.getChannelEq (id);
    CHECK (eq.bands[0].type == T::lowCut);
    CHECK (eq.bands[0].slope == 48);
    editor.setBandCut (1, 24);   // (not on band 2)
    CHECK_FALSE (studio.getChannelEq (id)->bands[1].isCut());
    editor.setBandType (2, T::notch);
    editor.setBandInvert (2, true);
    editor.setOutput (-2.5);
    eq = *studio.getChannelEq (id);
    CHECK (eq.bands[2].type == T::notch);
    CHECK (eq.bands[2].invert);
    CHECK (eq.outputDb == -2.5);
    studio.undo();
    CHECK (studio.getChannelEq (id)->outputDb == 0.0);

    // The keyboard: band 2's mark dragged near 445 Hz, let go: exactly A3 (440 Hz).
    auto& curve = editor.getCurve();
    editor.setBandType (1, T::parametric);
    CHECK (studio.getChannelEq (id)->outputDb == 0.0);   // (an edit right after Undo doesn't bring the undone back)
    editor.setBandFrequency (1, 300.0);
    const auto keys = curve.keysArea();
    REQUIRE_FALSE (keys.isEmpty());
    const auto mark = curve.keyMark (1).getCentre();
    const juce::Point<float> to (curve.xForFrequency (445.0), mark.y);
    curve.mouseDown (mouseOn (curve, mark, mark));
    curve.mouseDrag (mouseOn (curve, to, mark));
    curve.mouseUp (mouseOn (curve, to, mark));
    CHECK_THAT (studio.getChannelEq (id)->bands[1].frequency, WithinAbs (440.0, 1.0e-6));
    CHECK_THAT (EqCurve::keyFrequency (60), WithinAbs (261.6256, 1.0e-3));   // middle C = C3

    // The wheel over a point: its Q (up narrower).
    const auto q = studio.getChannelEq (id)->bands[1].q;
    juce::MouseWheelDetails wheel {};
    wheel.deltaY = 1.0f;
    const auto point = curve.bandPoint (1);
    curve.mouseWheelMove (mouseOn (curve, point, point), wheel);
    CHECK (studio.getChannelEq (id)->bands[1].q > q * 1.3);

    // Auto Listen while a point is dragged: the engine listens to that band, and stops on release.
    editor.setAutoListen (true);
    const auto p = curve.bandPoint (1);
    curve.mouseDown (mouseOn (curve, p, p));
    CHECK (studio.getEngine().getListenBus() == id);
    curve.mouseUp (mouseOn (curve, p, p));
    CHECK (studio.getEngine().getListenBus() == -1);

    // The spectrum from what the engine sends: a 1 kHz sine at half scale reads about -6 dB there,
    // nothing much far from it.
    struct Sine final : TestPlugin
    {
        Sine() : TestPlugin (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)) { instrument = true; name = "Sine"; }
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
        {
            for (int i = 0; i < buffer.getNumSamples(); ++i, ++n)
                for (int c = 0; c < buffer.getNumChannels(); ++c)
                    buffer.setSample (c, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (double) n / 48000.0));
        }
        juce::int64 n = 0;
    };
    editor.setBandOn (1, false);
    editor.setBandOn (0, false);
    editor.setBandOn (2, false);
    auto& engine = studio.getEngine();
    engine.prepare (48000.0, 512);
    engine.setInstrument (id, std::make_unique<Sine>());
    studio.setChannelEq (id, *studio.getChannelEq (id));   // (the engine brought up to date)
    engine.setSpectrumBus (id);
    editor.setOutput (0.0);
    const auto out = renderSamples (engine, 8192, 512);
    CHECK_THAT (*std::max_element (out.begin() + 4096, out.end()), WithinAbs (0.5, 0.01));
    curve.updateSpectrum();
    // (-6 dB, less up to 1.4 where 1 kHz falls between two of the analysis's frequencies.)
    CHECK (curve.spectrumLevelAt (1000.0) <= -5.5f);
    CHECK (curve.spectrumLevelAt (1000.0) >= -7.6f);
    CHECK (curve.spectrumLevelAt (6000.0) < -50.0);
    CHECK (editor.createComponentSnapshot (editor.getLocalBounds()).isValid());
}
