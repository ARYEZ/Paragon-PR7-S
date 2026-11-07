#include "ui/EqEditor.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    constexpr double displayRate = 48000.0;
    constexpr double lowestHz = 20.0, highestHz = 20000.0;

    juce::Font interFont (float height, bool bold = false) { return juce::Font (ParagonLookAndFeel::textFont (height, bold ? juce::Font::bold : juce::Font::plain)); }   // (0.71.1: Bebas, the owner's rule; the old name kept)

    // The bands' points in the family's colours (band 4 pearl white).
    juce::Colour bandColour (int band)
    {
        switch (band)
        {
            case 0: return pr7s::FamilyColours::orange;
            case 1: return pr7s::FamilyColours::red;
            case 2: return pr7s::FamilyColours::pink;
            default: return juce::Colour (0xfff4eeea);
        }
    }

    bool isCut (pr7s::EqBand::Type t) { return t == pr7s::EqBand::Type::lowCut || t == pr7s::EqBand::Type::highCut; }

    // A band's type as its key shows it: "PEAK", "LOW SHELF", "CUT 24".
    juce::String typeText (const pr7s::EqBand& b)
    {
        return b.isCut() ? "CUT " + juce::String (b.slope) : pr7s::bandTypeName (b.type).toUpperCase();
    }

    // "D6 +12": the note nearest a frequency and the cents off it (middle C = C3).
    juce::String noteOf (double hz)
    {
        const auto midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
        const auto nearest = juce::roundToInt (midi);
        const auto cents = juce::roundToInt ((midi - nearest) * 100.0);
        return pr7s::noteName (nearest) + (cents == 0 ? juce::String() : (cents > 0 ? " +" : " ") + juce::String (cents));
    }

    juce::String dbText (double db) { return (db >= 0.0 ? "+" : "") + juce::String (db, 1) + " dB"; }

    void styleBar (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 80, 20);
        s.setColour (juce::Slider::trackColourId, ParagonColours::accent.withAlpha (0.85f));
        s.setColour (juce::Slider::textBoxTextColourId, ParagonColours::text);
        s.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff1b171c));
        s.setColour (juce::Slider::textBoxOutlineColourId, ParagonColours::panelEdge);
    }

    void frequencyBar (juce::Slider& s, double low, double high)
    {
        styleBar (s);
        s.setRange (low, high, 0.0);
        s.setSkewFactorFromMidPoint (std::sqrt (low * high));
        s.textFromValueFunction = [] (double v) { return pr7s::frequencyText (v); };
        s.valueFromTextFunction = [&s] (const juce::String& t) { return pr7s::parseFrequency (t).value_or (s.getValue()); };
        s.updateText();   // (0.46.0: the low cut's box read "20.0000000" until moved)
    }

    void gainBar (juce::Slider& s)
    {
        styleBar (s);
        s.setRange (-pr7s::ChannelEq::maxGainDb, pr7s::ChannelEq::maxGainDb, 0.1);
        s.textFromValueFunction = [] (double v) { return dbText (v); };
        s.valueFromTextFunction = [] (const juce::String& t) { return t.retainCharacters ("+-0123456789.").getDoubleValue(); };
        s.setDoubleClickReturnValue (true, 0.0, juce::ModifierKeys::commandModifier);
        s.updateText();
    }

    void key (juce::TextButton& b, const juce::String& tip = {})
    {
        b.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        b.setWantsKeyboardFocus (false);
        if (tip.isNotEmpty())
            b.setTooltip (tip);
    }
}

//==============================================================================
EqCurve::EqCurve (pr7s::Studio& s, int busId, bool c) : studio (s), bus (busId), compact (c)
{
    shown = current();
    setTooltip (compact ? "The EQ's curve: click to add a band there, drag a point (Ctrl: gain only, Alt: frequency only, Shift: Q), double-click a point to switch it off"
                        : juce::String());
    startTimerHz (compact ? 15 : 30);
}

EqCurve::~EqCurve()
{
    listenTo (-1);
    if (tapping && studio.getEngine().getSpectrumBus() == bus)
        studio.getEngine().setSpectrumBus (-1);
}

void EqCurve::setBus (int busId)
{
    listenTo (-1);
    if (tapping && studio.getEngine().getSpectrumBus() == bus)
        studio.getEngine().setSpectrumBus (-1);
    tapping = false;
    bus = busId;
    dragging = -1;
    keyDragging = -1;
    shown = current();
    levels.clear();
    std::fill (heard.begin(), heard.end(), 0.0f);
    repaint();
}

void EqCurve::setKeysShown (bool on)
{
    showKeys = on;
    repaint();
}

void EqCurve::setSpectrumShown (bool on)
{
    showSpectrum = on;
    if (! on && tapping && studio.getEngine().getSpectrumBus() == bus)
        studio.getEngine().setSpectrumBus (-1), tapping = false;
    repaint();
}

double EqCurve::keyFrequency (int midi)
{
    return 440.0 * std::pow (2.0, (midi - 69) / 12.0);
}

void EqCurve::listenTo (int band)
{
    auto& engine = studio.getEngine();
    if (band < 0)
    {
        if (engine.getListenBus() == bus)
            engine.clearListen();
        return;
    }
    const auto eq = current();
    engine.setListen (bus, pr7s::listenDesign (eq.bands[(size_t) band], engine.getSampleRate() > 0.0 ? engine.getSampleRate() : 48000.0));
}

void EqCurve::updateSpectrum()
{
    // The channel's sound since last time, onto the end of the last fftSize samples; a Hann
    // window, its spectrum; each bin's level falls 1 dB a frame at most (a peak shows a moment).
    auto& engine = studio.getEngine();
    spectrumRate = engine.getSampleRate() > 0.0 ? engine.getSampleRate() : 48000.0;
    if (engine.readSpectrum (heard) <= 0 && ! levels.empty())
    {
        for (auto& l : levels)
            l = juce::jmax (-120.0f, l - 1.0f);
        return;
    }
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = heard[(size_t) i] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1)));
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);
    levels.resize ((size_t) fftSize / 2, -120.0f);
    for (int b = 0; b < fftSize / 2; ++b)
    {
        // (A full-scale sine reads 0 dB: the Hann window halves the sum, the transform counts both halves.)
        const auto db = juce::Decibels::gainToDecibels (fftData[(size_t) b] * 4.0f / (float) fftSize, -120.0f);
        levels[(size_t) b] = juce::jmax (db, levels[(size_t) b] - 1.0f);
    }
}

float EqCurve::spectrumLevelAt (double hz) const
{
    if (levels.empty())
        return -120.0f;
    const auto bin = juce::jlimit (0, (int) levels.size() - 1, juce::roundToInt (hz / spectrumRate * fftSize));
    return levels[(size_t) bin];
}

pr7s::ChannelEq EqCurve::current() const
{
    const auto* eq = studio.getChannelEq (bus);
    return eq != nullptr ? *eq : pr7s::ChannelEq {};
}

void EqCurve::apply (const pr7s::ChannelEq& e, bool undoable)
{
    studio.setChannelEq (bus, e, undoable);
    shown = current();
    repaint();
}

void EqCurve::timerCallback()
{
    if (! isShowing())
    {
        if (tapping && studio.getEngine().getSpectrumBus() == bus)
            studio.getEngine().setSpectrumBus (-1);
        tapping = false;
        return;
    }
    if (! compact && showSpectrum)
    {
        if (studio.getEngine().getSpectrumBus() != bus)
            studio.getEngine().setSpectrumBus (bus);   // (one window shows a spectrum: this one)
        tapping = true;
        updateSpectrum();
        repaint (plotArea().getSmallestIntegerContainer());
    }
    if (const auto now = current(); ! (now == shown))
    {
        shown = now;
        repaint();
    }
}

juce::Rectangle<float> EqCurve::plotArea() const
{
    auto r = getLocalBounds().toFloat();
    if (compact)
        return r.reduced (2.0f, 4.0f);
    r.removeFromTop (20.0f);
    r.removeFromBottom (16.0f + (showKeys ? (float) keysHeight + 4.0f : 0.0f));
    r.removeFromLeft (34.0f);
    return r.reduced (2.0f, 6.0f);
}

juce::Rectangle<float> EqCurve::keysArea() const
{
    if (compact || ! showKeys)
        return {};
    const auto a = plotArea();
    return { a.getX(), (float) getHeight() - (float) keysHeight - 2.0f, a.getWidth(), (float) keysHeight };
}

juce::Rectangle<float> EqCurve::keyMark (int band) const
{
    const auto k = keysArea();
    if (k.isEmpty())
        return {};
    const auto x = xForFrequency (current().bands[(size_t) band].frequency);
    return { x - 5.0f, k.getY() - 1.0f, 10.0f, 9.0f };
}

float EqCurve::xForFrequency (double hz) const
{
    const auto a = plotArea();
    const auto along = std::log (juce::jlimit (lowestHz, highestHz, hz) / lowestHz) / std::log (highestHz / lowestHz);
    return a.getX() + (float) along * a.getWidth();
}

double EqCurve::frequencyForX (float x) const
{
    const auto a = plotArea();
    const auto along = juce::jlimit (0.0, 1.0, (double) (x - a.getX()) / juce::jmax (1.0f, a.getWidth()));
    return lowestHz * std::pow (highestHz / lowestHz, along);
}

float EqCurve::yForDb (double db) const
{
    const auto a = plotArea();
    return a.getCentreY() - (float) (juce::jlimit (-rangeDb, rangeDb, db) / rangeDb) * a.getHeight() * 0.5f;
}

double EqCurve::dbForY (float y) const
{
    const auto a = plotArea();
    return juce::jlimit (-rangeDb, rangeDb, (double) (a.getCentreY() - y) / (a.getHeight() * 0.5) * rangeDb);
}

juce::Point<float> EqCurve::bandPoint (int band) const
{
    const auto eq = current();   // (the studio's now: an undo may have moved it since the last repaint)
    const auto& b = eq.bands[(size_t) band];
    return { xForFrequency (b.frequency), yForDb (b.isCut() ? 0.0 : b.playedGainDb()) };
}

int EqCurve::bandAt (juce::Point<float> p) const
{
    auto best = -1;
    auto bestDistance = compact ? 9.0f : 11.0f;
    const auto eq = current();
    for (int i = 0; i < 4; ++i)
        if (eq.bands[(size_t) i].on)
            if (const auto d = bandPoint (i).getDistanceFrom (p); d < bestDistance)
            {
                best = i;
                bestDistance = d;
            }
    return best;
}

void EqCurve::paint (juce::Graphics& g)
{
    const auto a = plotArea();
    g.setColour (juce::Colour (0xff0d0a0e));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

    // The grid: decades and their halves; every 6 dB.
    g.setFont (interFont (compact ? 8.5f : 10.0f));
    for (const auto hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const auto x = xForFrequency (hz);
        g.setColour (ParagonColours::panelEdge.withAlpha (hz == 100.0 || hz == 1000.0 || hz == 10000.0 ? 0.9f : 0.5f));
        g.fillRect (x, a.getY(), 1.0f, a.getHeight());
        if (! compact)
        {
            g.setColour (ParagonColours::textDim);
            g.drawText (hz >= 1000.0 ? juce::String (juce::roundToInt (hz / 1000.0)) + "k" : juce::String (juce::roundToInt (hz)),
                        juce::Rectangle<float> (x - 20.0f, a.getBottom() + 4.0f, 40.0f, 14.0f), juce::Justification::centred, false);
        }
    }
    for (auto db = -18; db <= 18; db += 6)
    {
        const auto y = yForDb (db);
        g.setColour (ParagonColours::panelEdge.withAlpha (db == 0 ? 1.0f : 0.45f));
        g.fillRect (a.getX(), y, a.getWidth(), db == 0 ? 1.5f : 1.0f);
        if (! compact && db != 0)
        {
            g.setColour (ParagonColours::textDim);
            g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (0.0f, y - 7.0f, 32.0f, 14.0f), juce::Justification::centredRight, false);
        }
    }

    // (0.46.0) The channel's sound after the EQ, behind the curve: -90 to 0 dB over the height.
    if (! compact && showSpectrum && ! levels.empty())
    {
        juce::Path spectrum;
        const auto yOfLevel = [&] (float db) { return a.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f) * a.getHeight(); };
        spectrum.startNewSubPath (a.getX(), a.getBottom());
        for (auto x = a.getX(); x <= a.getRight() + 0.5f; x += 2.0f)
        {
            // The loudest bin between this column and the next (the high end has many per column).
            const auto exact = frequencyForX (x) / spectrumRate * fftSize;
            const auto lowBin = juce::jlimit (1, (int) levels.size() - 2, (int) exact);
            const auto highBin = juce::jlimit (lowBin, (int) levels.size() - 1, (int) (frequencyForX (x + 2.0f) / spectrumRate * fftSize));
            auto loudest = -120.0f;
            if (highBin == lowBin)
            {
                // (The low end: a bin spans many pixels; between two bins, by how far along.)
                const auto along = (float) juce::jlimit (0.0, 1.0, exact - lowBin);
                loudest = levels[(size_t) lowBin] + along * (levels[(size_t) lowBin + 1] - levels[(size_t) lowBin]);
            }
            for (int b = lowBin; b <= highBin && highBin > lowBin; ++b)
                loudest = juce::jmax (loudest, levels[(size_t) b]);
            spectrum.lineTo (x, yOfLevel (loudest));
        }
        spectrum.lineTo (a.getRight(), a.getBottom());
        spectrum.closeSubPath();
        g.setColour (juce::Colour (0xfff4eeea).withAlpha (0.07f));
        g.fillPath (spectrum);
        g.setColour (juce::Colour (0xfff4eeea).withAlpha (0.22f));
        g.strokePath (spectrum, juce::PathStrokeType (1.0f));
    }

    // The curve as it plays (EQ and Pre); the EQ alone in grey when bypassed.
    const auto design = pr7s::designEq (shown, displayRate);
    const auto pathOf = [&] (const pr7s::EqDesign& d, bool withPre)
    {
        juce::Path p;
        for (auto x = a.getX(); x <= a.getRight() + 0.5f; x += 2.0f)
        {
            const auto y = yForDb (pr7s::eqResponseDb (d, frequencyForX (x), displayRate, withPre));
            if (x == a.getX())
                p.startNewSubPath (x, y);
            else
                p.lineTo (x, y);
        }
        return p;
    };
    if (shown.bypass && std::any_of (shown.bands.begin(), shown.bands.end(), [] (const pr7s::EqBand& b) { return b.on; }))
    {
        auto unbypassed = shown;
        unbypassed.bypass = false;
        juce::Path ghost;
        juce::PathStrokeType (1.2f).createDashedStroke (ghost, pathOf (pr7s::designEq (unbypassed, displayRate), false), std::array<float, 2> { 4.0f, 4.0f }.data(), 2);
        g.setColour (ParagonColours::textDim.withAlpha (0.7f));
        g.fillPath (ghost);
    }
    const auto curve = pathOf (design, true);
    {
        auto fill = curve;
        const auto zero = yForDb (0.0);
        fill.lineTo (a.getRight(), zero);
        fill.lineTo (a.getX(), zero);
        fill.closeSubPath();
        g.setColour (pr7s::FamilyColours::pink.withAlpha (0.16f));
        g.fillPath (fill);
    }
    g.setColour (shown.isFlat() ? ParagonColours::textDim : ParagonColours::accent);
    g.strokePath (curve, juce::PathStrokeType (compact ? 1.6f : 2.2f));

    // The bands' points.
    for (int i = 0; i < 4; ++i)
    {
        if (! shown.bands[(size_t) i].on)
            continue;
        const auto p = bandPoint (i);
        const auto r = compact ? 6.0f : 8.5f;
        g.setColour (bandColour (i).withAlpha (shown.bypass ? 0.4f : 1.0f));
        g.fillEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colour (0xff120d10));
        g.setFont (interFont (compact ? 9.0f : 11.0f, true));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (p.x - r, p.y - r, r * 2.0f, r * 2.0f), juce::Justification::centred, false);
        if (i == dragging)
        {
            g.setColour (juce::Colour (0xfff4eeea));
            g.drawEllipse (p.x - r - 2.0f, p.y - r - 2.0f, r * 2.0f + 4.0f, r * 2.0f + 4.0f, 1.2f);
        }
    }

    // (0.46.0) The keyboard under it (C-1 to C9 as the scale has room), each band's mark on it.
    if (const auto k = keysArea(); ! k.isEmpty())
    {
        g.setColour (juce::Colour (0xfff4eeea).withAlpha (0.85f));
        g.fillRect (k);
        for (int midi = 12; midi <= 136; ++midi)
        {
            const auto pc = midi % 12;
            const auto black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            const auto x1 = xForFrequency (keyFrequency (midi) / std::pow (2.0, 1.0 / 24.0)), x2 = xForFrequency (keyFrequency (midi) * std::pow (2.0, 1.0 / 24.0));
            if (x2 < k.getX() || x1 > k.getRight())
                continue;
            if (black)
            {
                g.setColour (juce::Colour (0xff120d10));
                g.fillRect (juce::Rectangle<float> (x1, k.getY(), juce::jmax (1.0f, x2 - x1), k.getHeight() * 0.6f));
            }
            else if (pc == 0 || pc == 5)
            {
                g.setColour (juce::Colour (0xff120d10).withAlpha (0.35f));
                g.fillRect (juce::Rectangle<float> (x1, k.getY(), 1.0f, k.getHeight()));
                if (pc == 0 && x2 - x1 > 1.0f)
                {
                    g.setFont (interFont (8.0f));
                    g.drawText ("C" + juce::String (midi / 12 - 2), juce::Rectangle<float> (x1 + 1.0f, k.getBottom() - 10.0f, 22.0f, 10.0f), juce::Justification::centredLeft, false);
                }
            }
        }
        for (int i = 0; i < 4; ++i)
            if (shown.bands[(size_t) i].on)
            {
                const auto m = keyMark (i);
                juce::Path mark;
                mark.addTriangle (m.getX(), m.getY(), m.getRight(), m.getY(), m.getCentreX(), m.getBottom());
                g.setColour (bandColour (i));
                g.fillPath (mark);
                g.fillRect (juce::Rectangle<float> (m.getCentreX() - 0.75f, m.getBottom(), 1.5f, k.getBottom() - m.getBottom()));
            }
    }

    // Where the mouse is: frequency, note, level (the manual's cross-hair readout).
    if (! compact && hover.x >= a.getX() && a.contains (hover))
    {
        const auto hz = frequencyForX (hover.x);
        // (0.71.0, the owner's: the ordinary pointer, no cross-hair lines; the readout stays.)
        g.setColour (ParagonColours::text);
        g.setFont (interFont (12.0f));
        g.drawText (pr7s::frequencyText (hz) + "   " + noteOf (hz) + "   " + dbText (dbForY (hover.y)),
                    juce::Rectangle<float> (a.getX(), 0.0f, a.getWidth(), 18.0f), juce::Justification::centredRight, false);
    }
}

void EqCurve::mouseMove (const juce::MouseEvent& e)
{
    hover = e.position;
    if (! compact)
        repaint();
    setMouseCursor (bandAt (e.position) >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void EqCurve::mouseExit (const juce::MouseEvent&)
{
    hover = { -1.0f, -1.0f };
    repaint();
}

void EqCurve::mouseDown (const juce::MouseEvent& e)
{
    shown = current();
    if (e.mods.isPopupMenu())
    {
        if (onMenu)
            onMenu (e);
        else
            EqEditor::showPresetMenu (studio, bus, this);
        return;
    }
    // (0.46.0) The keyboard: a band's mark dragged (the one nearest the click within 8 px).
    if (const auto k = keysArea(); ! k.isEmpty() && e.position.y >= k.getY() - 2.0f)
    {
        auto best = -1;
        auto bestDistance = 8.0f;
        for (int i = 0; i < 4; ++i)
            if (shown.bands[(size_t) i].on)
                if (const auto d = std::abs (keyMark (i).getCentreX() - e.position.x); d < bestDistance)
                    best = i, bestDistance = d;
        keyDragging = best;
        atDown = shown;
        if (best >= 0 && autoListen)
            listenTo (best);
        return;
    }
    auto band = bandAt (e.position);
    auto eq = shown;
    if (band < 0)
    {
        // The band that's off nearest the click (in octaves) comes on there; all on: the nearest.
        const auto hz = frequencyForX (e.position.x);
        auto best = -1;
        auto bestDistance = 1.0e9;
        for (const auto wantOff : { true, false })
        {
            for (int i = 0; i < 4; ++i)
                if (eq.bands[(size_t) i].on != wantOff)
                    if (const auto d = std::abs (std::log2 (hz / eq.bands[(size_t) i].frequency)); d < bestDistance)
                    {
                        best = i;
                        bestDistance = d;
                    }
            if (best >= 0)
                break;
        }
        band = best;
        auto& b = eq.bands[(size_t) band];
        b.on = true;
        b.frequency = hz;
        if (! b.isCut())
            b.gainDb = std::round ((b.invert ? -1.0 : 1.0) * dbForY (e.position.y) * 10.0) / 10.0;
        apply (eq, false);
    }
    dragging = band;
    atDown = current();
    if (autoListen && band >= 0)
        listenTo (band);
    repaint();
}

void EqCurve::mouseDrag (const juce::MouseEvent& e)
{
    if (keyDragging >= 0)
    {
        auto eq = atDown;
        eq.bands[(size_t) keyDragging].frequency = frequencyForX (e.position.x);
        apply (eq, false);
        if (autoListen)
            listenTo (keyDragging);
        return;
    }
    if (dragging < 0)
        return;
    auto eq = atDown;
    auto& b = eq.bands[(size_t) dragging];
    if (e.mods.isShiftDown())
        b.q = juce::jlimit (pr7s::ChannelEq::minQ, pr7s::ChannelEq::maxQ, atDown.bands[(size_t) dragging].q * std::pow (2.0, -e.getDistanceFromDragStartY() / 40.0));
    else
    {
        if (! e.mods.isCommandDown())
            b.frequency = frequencyForX (e.position.x);
        if (! e.mods.isAltDown() && ! b.isCut())
            b.gainDb = std::round ((b.invert ? -1.0 : 1.0) * dbForY (e.position.y) * 10.0) / 10.0;   // (inverted: the point stays under the mouse)
    }
    apply (eq, false);
    if (autoListen)
        listenTo (dragging);
}

void EqCurve::mouseUp (const juce::MouseEvent&)
{
    if (keyDragging >= 0)
    {
        // Let go on the keyboard: exactly the nearest key's frequency (the manual's).
        auto eq = current();
        auto& b = eq.bands[(size_t) keyDragging];
        b.frequency = juce::jlimit (pr7s::ChannelEq::minFrequency, pr7s::ChannelEq::maxFrequency,
                                    keyFrequency (juce::roundToInt (69.0 + 12.0 * std::log2 (b.frequency / 440.0))));
        apply (eq, true);
        keyDragging = -1;
        listenTo (-1);
        return;
    }
    if (dragging >= 0)
        apply (current(), true);   // (the drag: one undo step)
    dragging = -1;
    listenTo (-1);
    repaint();
}

void EqCurve::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // (0.46.0, the manual's) The wheel over a point: its Q (up narrower). One undo step a turn.
    const auto band = bandAt (e.position);
    if (band < 0 || wheel.deltaY == 0.0f)
        return;
    auto eq = current();
    auto& b = eq.bands[(size_t) band];
    if (b.isCut() && b.slope == 6)
        return;
    b.q = juce::jlimit (pr7s::ChannelEq::minQ, pr7s::ChannelEq::maxQ, b.q * std::pow (2.0, wheel.deltaY * (wheel.isReversed ? -1.0 : 1.0) * 0.5));
    apply (eq, true);
}

void EqCurve::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (const auto band = bandAt (e.position); band >= 0)
    {
        auto eq = current();
        eq.bands[(size_t) band].on = false;
        dragging = -1;
        apply (eq, true);
    }
}

//==============================================================================
struct EqEditor::BandColumn final : public juce::Component
{
    BandColumn (EqEditor& e, int i) : editor (e), index (i)
    {
        key (on, "This band on or off");
        on.setButtonText ("BAND " + juce::String (i + 1));
        on.onClick = [this] { editor.setBandOn (index, ! editor.shown.bands[(size_t) index].on); };
        addAndMakeVisible (on);
        key (type, index == 0 ? "Its shape: Low Shelf, Peak, High Shelf, Notch, or a low Cut (6 to 96 dB an octave)"
                   : index == 3 ? "Its shape: Low Shelf, Peak, High Shelf, Notch, or a high Cut (6 to 96 dB an octave)"
                                : "Its shape: Low Shelf, Peak, High Shelf or Notch");
        type.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
        key (invertKey, "Invert Gain: a boost becomes the same cut, and the other way round");
        invertKey.onClick = [this] { editor.setBandInvert (index, ! editor.shown.bands[(size_t) index].invert); };
        addAndMakeVisible (invertKey);
        type.onClick = [this] { showTypeMenu(); };
        addAndMakeVisible (type);
        gainBar (gain);
        gain.setTooltip ("Cut or boost, +-24 dB (Ctrl-click: 0)");
        frequencyBar (frequency, pr7s::ChannelEq::minFrequency, pr7s::ChannelEq::maxFrequency);
        frequency.setTooltip ("The frequency (20 Hz to 20 kHz): drag, or double-click and type Hz, kHz or a note (A3 = 440 Hz; \"A5 -23\" with cents)");
        styleBar (q);
        q.setRange (pr7s::ChannelEq::minQ, pr7s::ChannelEq::maxQ, 0.01);
        q.setSkewFactorFromMidPoint (1.0);
        q.textFromValueFunction = [] (double v) { return "Q " + juce::String (v, 2); };
        q.valueFromTextFunction = [] (const juce::String& t) { return t.retainCharacters ("0123456789.").getDoubleValue(); };
        q.setTooltip ("Peak and Notch: how wide (higher is narrower). Shelves: over 0.71, a dip and a bump at the corner. "
                      "Cuts: a resonance at the frequency (none for Cut 6). The wheel over a point on the curve sets it too");
        for (auto* s : { &gain, &frequency, &q })
        {
            s->onValueChange = [this, s] { sent (*s, ! s->isMouseButtonDown()); };
            s->onDragEnd = [this, s] { sent (*s, true); };
            addAndMakeVisible (*s);
        }
    }

    void sent (juce::Slider& s, bool undoable)
    {
        auto& band = editor.shown.bands[(size_t) index];
        if (&s == &gain) editor.change ([this, v = s.getValue()] (pr7s::ChannelEq& e) { e.bands[(size_t) index].gainDb = v; e.bands[(size_t) index].on = true; }, undoable);
        if (&s == &frequency) editor.change ([this, v = s.getValue()] (pr7s::ChannelEq& e) { e.bands[(size_t) index].frequency = v; e.bands[(size_t) index].on = true; }, undoable);
        if (&s == &q) editor.change ([this, v = s.getValue()] (pr7s::ChannelEq& e) { e.bands[(size_t) index].q = v; e.bands[(size_t) index].on = true; }, undoable);
        juce::ignoreUnused (band);
    }

    void showTypeMenu()
    {
        // (0.46.0) Low Shelf, Peak, High Shelf, Notch; band 1 and 4 also their Cut and its slope.
        juce::PopupMenu menu;
        const auto& now = editor.shown.bands[(size_t) index];
        for (const auto t : pr7s::ChannelEq::typesFor (index))
            if (t == pr7s::EqBand::Type::lowCut || t == pr7s::EqBand::Type::highCut)
                for (const auto slope : pr7s::ChannelEq::bandSlopes())
                    menu.addItem (1000 + slope, "Cut " + juce::String (slope) + " dB/oct", true, now.isCut() && now.slope == slope);
            else
                menu.addItem ((int) t + 1, pr7s::bandTypeName (t), true, t == now.type);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&type),
                            [safe = juce::Component::SafePointer<BandColumn> (this)] (int chosen)
                            {
                                if (safe == nullptr || chosen <= 0)
                                    return;
                                if (chosen > 1000)
                                    safe->editor.setBandCut (safe->index, chosen - 1000);
                                else
                                    safe->editor.setBandType (safe->index, (pr7s::EqBand::Type) (chosen - 1));
                            });
    }

    void refresh (const pr7s::EqBand& b)
    {
        on.setToggleState (b.on, juce::dontSendNotification);
        type.setButtonText (typeText (b));
        gain.setValue (b.gainDb, juce::dontSendNotification);
        frequency.setValue (b.frequency, juce::dontSendNotification);
        q.setValue (b.q, juce::dontSendNotification);
        gain.setEnabled (! b.isCut());
        q.setEnabled (! (b.isCut() && b.slope == 6));
        invertKey.setToggleState (b.invert, juce::dontSendNotification);
        invertKey.setEnabled (! b.isCut());
        for (auto* s : { &gain, &frequency, &q })
            s->setAlpha (b.on ? 1.0f : 0.5f);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (bandColour (index));
        g.fillRect (0, 0, getWidth(), 3);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 6);
        r.removeFromTop (2);
        const auto row = [&r] { auto line = r.removeFromTop (28); r.removeFromTop (6); return line; };
        on.setBounds (row());
        type.setBounds (row());
        auto gainRow = row();
        invertKey.setBounds (gainRow.removeFromRight (64));
        gainRow.removeFromRight (6);
        gain.setBounds (gainRow);
        frequency.setBounds (row());
        q.setBounds (row());
    }

    EqEditor& editor;
    int index;
    juce::TextButton on, type, invertKey { "INVERT" };
    juce::Slider gain, frequency, q;
};

//==============================================================================
EqEditor::EqEditor (pr7s::Studio& s, int busId) : studio (s), bus (busId), curve (s, busId, false)
{
    curve.onMenu = [this] (const juce::MouseEvent&) { showMenu(); };
    addAndMakeVisible (curve);
    key (bypassKey, "Bypass all four bands (the Pre section stays in)");
    bypassKey.onClick = [this] { setBypass (! shown.bypass); };
    key (presetsKey, "Save or load an EQ preset; invert or reset the EQ");
    presetsKey.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    presetsKey.onClick = [this] { showMenu(); };
    key (lowCutKey, "A low cut before everything (the Pre section)");
    lowCutKey.onClick = [this] { setLowCut (! shown.lowCutOn, shown.lowCut, shown.lowCutSlope); };
    key (highCutKey, "A high cut before everything (the Pre section)");
    highCutKey.onClick = [this] { setHighCut (! shown.highCutOn, shown.highCut, shown.highCutSlope); };
    key (phaseKey, "The phase turned over (the Pre section)");
    phaseKey.onClick = [this] { setPhase (! shown.phaseInvert); };
    for (auto* slope : { &lowSlopeKey, &highSlopeKey })
    {
        key (*slope, "How steep: 6, 12, 24, 36 or 48 dB an octave");
        slope->getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
        slope->onClick = [this, slope]
        {
            const auto low = slope == &lowSlopeKey;
            juce::PopupMenu menu;
            for (const auto v : pr7s::ChannelEq::slopes())
                menu.addItem (v, juce::String (v) + " dB", true, v == (low ? shown.lowCutSlope : shown.highCutSlope));
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (slope),
                                [safe = juce::Component::SafePointer<EqEditor> (this), low] (int chosen)
                                {
                                    if (safe == nullptr || chosen <= 0)
                                        return;
                                    if (low)
                                        safe->setLowCut (true, safe->shown.lowCut, chosen);
                                    else
                                        safe->setHighCut (true, safe->shown.highCut, chosen);
                                });
        };
    }
    // (0.46.0) As the other DAW's Frequency 2: Auto Listen, the keyboard, the spectrum, Output.
    key (listenKey, "Auto Listen: while you drag a band's point, the channel plays only what that band works on");
    listenKey.onClick = [this] { setAutoListen (! listenKey.getToggleState()); };
    key (keysKey, "The keyboard under the curve: each band's mark on it (drag it; let go and it's exactly that key)");
    keysKey.onClick = [this] { setKeysShown (! keysKey.getToggleState()); };
    key (spectrumKey, "The channel's sound after the EQ, behind the curve");
    spectrumKey.onClick = [this] { setSpectrumShown (! spectrumKey.getToggleState()); };
    gainBar (output);
    output.setTooltip ("Output: the EQ's level after its bands (+-24 dB; Ctrl-click: 0)");
    output.textFromValueFunction = [] (double v) { return "OUTPUT " + dbText (v); };
    output.updateText();
    output.onValueChange = [this] { change ([v = output.getValue()] (pr7s::ChannelEq& e) { e.outputDb = v; }, ! output.isMouseButtonDown()); };
    output.onDragEnd = [this] { change ([] (pr7s::ChannelEq&) {}, true); };
    addAndMakeVisible (output);
    if (auto* settings = studio.getSettings())
    {
        listenKey.setToggleState (settings->getBoolValue ("eqAutoListen", false), juce::dontSendNotification);
        keysKey.setToggleState (settings->getBoolValue ("eqKeys", true), juce::dontSendNotification);
        spectrumKey.setToggleState (settings->getBoolValue ("eqSpectrum", true), juce::dontSendNotification);
    }
    else
    {
        keysKey.setToggleState (true, juce::dontSendNotification);
        spectrumKey.setToggleState (true, juce::dontSendNotification);
    }
    curve.setAutoListen (listenKey.getToggleState());
    curve.setKeysShown (keysKey.getToggleState());
    curve.setSpectrumShown (spectrumKey.getToggleState());
    for (auto* k : { &bypassKey, &presetsKey, &lowCutKey, &highCutKey, &phaseKey, &lowSlopeKey, &highSlopeKey, &listenKey, &keysKey, &spectrumKey })
        addAndMakeVisible (*k);
    frequencyBar (lowCutFreq, pr7s::ChannelEq::minFrequency, pr7s::ChannelEq::maxFrequency);
    frequencyBar (highCutFreq, pr7s::ChannelEq::minHighCut, pr7s::ChannelEq::maxFrequency);
    gainBar (preGain);
    preGain.setTooltip ("The input gain, before everything (+-24 dB; Ctrl-click: 0)");
    preGain.textFromValueFunction = [] (double v) { return "GAIN " + dbText (v); };
    preGain.updateText();   // (the text set before, by gainBar, is shown until told)
    lowCutFreq.onValueChange = [this] { change ([v = lowCutFreq.getValue()] (pr7s::ChannelEq& e) { e.lowCut = v; e.lowCutOn = true; }, ! lowCutFreq.isMouseButtonDown()); };
    highCutFreq.onValueChange = [this] { change ([v = highCutFreq.getValue()] (pr7s::ChannelEq& e) { e.highCut = v; e.highCutOn = true; }, ! highCutFreq.isMouseButtonDown()); };
    preGain.onValueChange = [this] { change ([v = preGain.getValue()] (pr7s::ChannelEq& e) { e.preGainDb = v; }, ! preGain.isMouseButtonDown()); };
    for (auto* s : { &lowCutFreq, &highCutFreq, &preGain })
    {
        s->onDragEnd = [this] { change ([] (pr7s::ChannelEq&) {}, true); };
        addAndMakeVisible (*s);
    }
    for (int i = 0; i < 4; ++i)
        addAndMakeVisible (bands.add (new BandColumn (*this, i)));
    refresh();
    setSize (width, height);
    startTimerHz (10);
}

EqEditor::~EqEditor() = default;

void EqEditor::setBus (int busId)
{
    bus = busId;
    curve.setBus (busId);
    refresh();
    repaint();
}

juce::String EqEditor::channelName (pr7s::Studio& studio, int busId)
{
    if (busId == pr7s::Engine::masterBus)
        return "MASTER";
    const auto* t = studio.getProject().findTrack (busId);
    return t != nullptr ? t->name : juce::String();
}

void EqEditor::timerCallback()
{
    if (const auto* eq = studio.getChannelEq (bus); eq != nullptr && ! (*eq == shown))
        refresh();
}

void EqEditor::refresh()
{
    const auto* eq = studio.getChannelEq (bus);
    shown = eq != nullptr ? *eq : pr7s::ChannelEq {};
    bypassKey.setToggleState (shown.bypass, juce::dontSendNotification);
    lowCutKey.setToggleState (shown.lowCutOn, juce::dontSendNotification);
    highCutKey.setToggleState (shown.highCutOn, juce::dontSendNotification);
    phaseKey.setToggleState (shown.phaseInvert, juce::dontSendNotification);
    lowSlopeKey.setButtonText (juce::String (shown.lowCutSlope) + " DB");
    highSlopeKey.setButtonText (juce::String (shown.highCutSlope) + " DB");
    if (! lowCutFreq.isMouseButtonDown())  lowCutFreq.setValue (shown.lowCut, juce::dontSendNotification);
    if (! highCutFreq.isMouseButtonDown()) highCutFreq.setValue (shown.highCut, juce::dontSendNotification);
    if (! preGain.isMouseButtonDown())     preGain.setValue (shown.preGainDb, juce::dontSendNotification);
    if (! output.isMouseButtonDown())      output.setValue (shown.outputDb, juce::dontSendNotification);
    lowCutFreq.setAlpha (shown.lowCutOn ? 1.0f : 0.5f);
    highCutFreq.setAlpha (shown.highCutOn ? 1.0f : 0.5f);
    for (int i = 0; i < bands.size(); ++i)
        bands[i]->refresh (shown.bands[(size_t) i]);
    repaint (0, 0, getWidth(), 36);
}

void EqEditor::change (const std::function<void (pr7s::ChannelEq&)>& edit, bool undoable)
{
    // (0.46.0) From the song's EQ as it is now, not what was last shown: an Undo just before a
    // click came back with the click (the shown copy catches up only on the timer).
    const auto* now = studio.getChannelEq (bus);
    auto eq = now != nullptr ? *now : shown;
    edit (eq);
    studio.setChannelEq (bus, eq, undoable);
    refresh();
}

void EqEditor::setBandOn (int band, bool on)                 { change ([band, on] (pr7s::ChannelEq& e) { e.bands[(size_t) band].on = on; }); }
void EqEditor::setBandType (int band, pr7s::EqBand::Type t) { change ([band, t] (pr7s::ChannelEq& e) { e.bands[(size_t) band].type = t; e.bands[(size_t) band].on = true; }); }
void EqEditor::setBandGain (int band, double db)            { change ([band, db] (pr7s::ChannelEq& e) { e.bands[(size_t) band].gainDb = db; e.bands[(size_t) band].on = true; }); }
void EqEditor::setBandFrequency (int band, double hz)       { change ([band, hz] (pr7s::ChannelEq& e) { e.bands[(size_t) band].frequency = hz; e.bands[(size_t) band].on = true; }); }
void EqEditor::setBandQ (int band, double q)                { change ([band, q] (pr7s::ChannelEq& e) { e.bands[(size_t) band].q = q; e.bands[(size_t) band].on = true; }); }
void EqEditor::setBypass (bool on)                          { change ([on] (pr7s::ChannelEq& e) { e.bypass = on; }); }
void EqEditor::setBandInvert (int band, bool on)            { change ([band, on] (pr7s::ChannelEq& e) { e.bands[(size_t) band].invert = on; e.bands[(size_t) band].on = true; }); }
void EqEditor::setOutput (double db)                        { change ([db] (pr7s::ChannelEq& e) { e.outputDb = db; }); }

void EqEditor::setBandCut (int band, int slope)
{
    if (band != 0 && band != 3)
        return;
    change ([band, slope] (pr7s::ChannelEq& e)
    {
        auto& b = e.bands[(size_t) band];
        b.type = band == 0 ? pr7s::EqBand::Type::lowCut : pr7s::EqBand::Type::highCut;
        b.slope = slope;
        b.on = true;
    });
}

void EqEditor::setAutoListen (bool on)
{
    listenKey.setToggleState (on, juce::dontSendNotification);
    curve.setAutoListen (on);
    if (auto* settings = studio.getSettings())
        settings->setValue ("eqAutoListen", on);
}

void EqEditor::setKeysShown (bool on)
{
    keysKey.setToggleState (on, juce::dontSendNotification);
    curve.setKeysShown (on);
    if (auto* settings = studio.getSettings())
        settings->setValue ("eqKeys", on);
}

void EqEditor::setSpectrumShown (bool on)
{
    spectrumKey.setToggleState (on, juce::dontSendNotification);
    curve.setSpectrumShown (on);
    if (auto* settings = studio.getSettings())
        settings->setValue ("eqSpectrum", on);
}
void EqEditor::setLowCut (bool on, double hz, int slope)    { change ([=] (pr7s::ChannelEq& e) { e.lowCutOn = on; e.lowCut = hz; e.lowCutSlope = slope; }); }
void EqEditor::setHighCut (bool on, double hz, int slope)   { change ([=] (pr7s::ChannelEq& e) { e.highCutOn = on; e.highCut = hz; e.highCutSlope = slope; }); }
void EqEditor::setPreGain (double db)                       { change ([db] (pr7s::ChannelEq& e) { e.preGainDb = db; }); }
void EqEditor::setPhase (bool inverted)                     { change ([inverted] (pr7s::ChannelEq& e) { e.phaseInvert = inverted; }); }
void EqEditor::invert()                                     { change ([] (pr7s::ChannelEq& e) { e.invert(); }); }
void EqEditor::reset()                                      { change ([] (pr7s::ChannelEq& e) { e = {}; }); }

void EqEditor::showMenu()
{
    showPresetMenu (studio, bus, &presetsKey);
}

void EqEditor::showPresetMenu (pr7s::Studio& studio, int busId, juce::Component* target)
{
    juce::PopupMenu menu, load;
    const auto folder = pr7s::Studio::eqPresetFolder();
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.pr7eq");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    for (int i = 0; i < files.size(); ++i)
        load.addItem (100 + i, files[i].getFileNameWithoutExtension());
    menu.addSectionHeader ("EQ: " + channelName (studio, busId).toUpperCase());
    menu.addItem (1, "Save Preset...");
    menu.addSubMenu ("Load Preset", load, files.size() > 0);
    // (0.71.0) Our starting points, by kind of source.
    {
        juce::PopupMenu starting;
        juce::String group;
        juce::PopupMenu sub;
        const auto& points = pr7s::eqStartingPoints();
        for (int i = 0; i <= (int) points.size(); ++i)
        {
            if (i == (int) points.size() || points[(size_t) i].group != group)
            {
                if (group.isNotEmpty())
                    starting.addSubMenu (group, sub);
                if (i == (int) points.size())
                    break;
                group = points[(size_t) i].group;
                sub = {};
            }
            sub.addItem (startingPointItems + i, points[(size_t) i].name);
        }
        menu.addSubMenu ("Starting Points", starting);
    }
    menu.addSeparator();
    menu.addItem (2, "Invert EQ Settings");
    menu.addItem (3, "Reset EQ and Pre");
    const auto* eq = studio.getChannelEq (busId);
    menu.addItem (4, "Bypass the EQ", true, eq != nullptr && eq->bypass);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target),
                        [&studio, busId, files, folder] (int chosen)
                        {
                            const auto* now = studio.getChannelEq (busId);
                            if (now == nullptr)
                                return;
                            auto eq = *now;
                            if (chosen == 2) { eq.invert(); studio.setChannelEq (busId, eq); }
                            if (chosen == 3) studio.setChannelEq (busId, {});
                            if (chosen == 4) { eq.bypass = ! eq.bypass; studio.setChannelEq (busId, eq); }
                            if (const auto& points = pr7s::eqStartingPoints();
                                chosen >= startingPointItems && chosen - startingPointItems < (int) points.size())
                            {
                                // (The starting point's bands and Pre; this channel's bypass kept.)
                                auto chosenEq = points[(size_t) (chosen - startingPointItems)].eq;
                                chosenEq.bypass = eq.bypass;
                                studio.setChannelEq (busId, chosenEq);
                                return;
                            }
                            if (chosen >= 100 && chosen - 100 < files.size())
                                studio.loadEqPreset (busId, files[chosen - 100]);
                            if (chosen == 1)
                            {
                                folder.createDirectory();
                                auto chooser = std::make_shared<juce::FileChooser> ("Save the EQ preset", folder.getChildFile ("My EQ.pr7eq"), "*.pr7eq");
                                juce::MessageManager::callAsync ([&studio, busId, chooser]
                                {
                                    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                                                          [&studio, busId, chooser] (const juce::FileChooser& c)
                                                          {
                                                              if (const auto file = c.getResult(); file != juce::File())
                                                                  studio.saveEqPreset (busId, file.withFileExtension (".pr7eq"));
                                                          });
                                });
                            }
                        });
}

void EqEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0a0d));
    g.setColour (ParagonColours::accent);
    g.fillRect (0, 0, getWidth(), 3);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (18.0f));
    g.drawText ("EQ: " + channelName (studio, bus).toUpperCase(), juce::Rectangle<int> (14, 6, juce::jmax (80, listenKey.getX() - 24), 30), juce::Justification::centredLeft, true);
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::titleFont (13.0f));
    g.drawText ("PRE", juce::Rectangle<int> (14, getHeight() - 44, 40, 36), juce::Justification::centredLeft, false);
}

void EqEditor::resized()
{
    auto area = getLocalBounds().reduced (14, 6);
    auto top = area.removeFromTop (30);
    presetsKey.setBounds (top.removeFromRight (110));
    top.removeFromRight (8);
    bypassKey.setBounds (top.removeFromRight (90));
    top.removeFromRight (16);
    output.setBounds (top.removeFromRight (170));
    top.removeFromRight (16);
    spectrumKey.setBounds (top.removeFromRight (96));
    top.removeFromRight (6);
    keysKey.setBounds (top.removeFromRight (60));
    top.removeFromRight (6);
    listenKey.setBounds (top.removeFromRight (110));
    area.removeFromTop (6);
    auto pre = area.removeFromBottom (36).reduced (0, 4);
    pre.removeFromLeft (44);
    const auto preWidth = pre.getWidth();
    const auto place = [&pre, preWidth] (juce::Component& c, int share) { c.setBounds (pre.removeFromLeft (preWidth * share / 100)); pre.removeFromLeft (6); };
    place (lowCutKey, 10);
    place (lowCutFreq, 14);
    place (lowSlopeKey, 8);
    place (highCutKey, 10);
    place (highCutFreq, 14);
    place (highSlopeKey, 8);
    place (preGain, 16);
    phaseKey.setBounds (pre);
    area.removeFromBottom (8);
    auto columns = area.removeFromBottom (5 * 34 + 10);
    const auto columnWidth = (columns.getWidth() - 3 * 12) / 4;
    for (auto* b : bands)
    {
        b->setBounds (columns.removeFromLeft (columnWidth));
        columns.removeFromLeft (12);
    }
    area.removeFromBottom (8);
    curve.setBounds (area);
}

//==============================================================================
EqWindow::EqWindow (pr7s::Studio& studio, int busId)
    : juce::DocumentWindow ("Paragon PR7-S: EQ", juce::Colour (0xff0c0a0d), juce::DocumentWindow::closeButton)
{
    editor = new EqEditor (studio, busId);
    setContentOwned (editor, true);
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (640, 460, 3840, 2160);
}

void EqWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}
