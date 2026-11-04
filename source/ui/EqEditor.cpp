#include "ui/EqEditor.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    constexpr double displayRate = 48000.0;
    constexpr double lowestHz = 20.0, highestHz = 20000.0;

    juce::Font interFont (float height, bool bold = false) { return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain)); }

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

    // "D6 +12": the note nearest a frequency and the cents off it (middle C = C3).
    juce::String noteOf (double hz)
    {
        const auto midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
        const auto nearest = juce::roundToInt (midi);
        const auto cents = juce::roundToInt ((midi - nearest) * 100.0);
        return pr7s::noteName (nearest) + (cents == 0 ? juce::String() : (cents > 0 ? " +" : " ") + juce::String (cents));
    }

    juce::String dbText (double db) { return (db > 0.0 ? "+" : "") + juce::String (db, 1) + " dB"; }

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
    }

    void gainBar (juce::Slider& s)
    {
        styleBar (s);
        s.setRange (-pr7s::ChannelEq::maxGainDb, pr7s::ChannelEq::maxGainDb, 0.1);
        s.textFromValueFunction = [] (double v) { return dbText (v); };
        s.valueFromTextFunction = [] (const juce::String& t) { return t.retainCharacters ("+-0123456789.").getDoubleValue(); };
        s.setDoubleClickReturnValue (true, 0.0, juce::ModifierKeys::commandModifier);
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
    startTimerHz (15);
}

void EqCurve::setBus (int busId)
{
    bus = busId;
    dragging = -1;
    shown = current();
    repaint();
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
        return;
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
    r.removeFromBottom (16.0f);
    r.removeFromLeft (34.0f);
    return r.reduced (2.0f, 6.0f);
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
    const auto& b = shown.bands[(size_t) band];
    return { xForFrequency (b.frequency), yForDb (isCut (b.type) ? 0.0 : b.gainDb) };
}

int EqCurve::bandAt (juce::Point<float> p) const
{
    auto best = -1;
    auto bestDistance = compact ? 9.0f : 11.0f;
    for (int i = 0; i < 4; ++i)
        if (shown.bands[(size_t) i].on)
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

    // Where the mouse is: frequency, note, level (the manual's cross-hair readout).
    if (! compact && hover.x >= a.getX() && a.contains (hover))
    {
        const auto hz = frequencyForX (hover.x);
        g.setColour (juce::Colour (0xfff4eeea).withAlpha (0.35f));
        g.fillRect (hover.x, a.getY(), 1.0f, a.getHeight());
        g.fillRect (a.getX(), hover.y, a.getWidth(), 1.0f);
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
    setMouseCursor (bandAt (e.position) >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CrosshairCursor);
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
        if (! isCut (b.type))
            b.gainDb = std::round (dbForY (e.position.y) * 10.0) / 10.0;
        apply (eq, false);
    }
    dragging = band;
    atDown = current();
    repaint();
}

void EqCurve::mouseDrag (const juce::MouseEvent& e)
{
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
        if (! e.mods.isAltDown() && ! isCut (b.type))
            b.gainDb = std::round (dbForY (e.position.y) * 10.0) / 10.0;
    }
    apply (eq, false);
}

void EqCurve::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        apply (current(), true);   // (the drag: one undo step)
    dragging = -1;
    repaint();
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
        key (type, pr7s::ChannelEq::canBe (index, pr7s::EqBand::Type::lowShelf) ? "Its shape: parametric, shelf or cut"
                                                                                : "Bands 2 and 3 are always parametric");
        type.getProperties().set (ParagonLookAndFeel::menuArrowProperty, pr7s::ChannelEq::canBe (index, pr7s::EqBand::Type::lowShelf));
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
        q.setTooltip ("How wide: higher is narrower");
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
        if (! pr7s::ChannelEq::canBe (index, pr7s::EqBand::Type::lowShelf))
            return;
        using T = pr7s::EqBand::Type;
        juce::PopupMenu menu;
        const auto now = editor.shown.bands[(size_t) index].type;
        const auto shelf = index == 0 ? T::lowShelf : T::highShelf;
        const auto cutType = index == 0 ? T::lowCut : T::highCut;
        for (const auto t : { T::parametric, shelf, cutType })
            menu.addItem ((int) t + 1, pr7s::bandTypeName (t), true, t == now);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&type),
                            [safe = juce::Component::SafePointer<BandColumn> (this)] (int chosen)
                            {
                                if (safe != nullptr && chosen > 0)
                                    safe->editor.setBandType (safe->index, (pr7s::EqBand::Type) (chosen - 1));
                            });
    }

    void refresh (const pr7s::EqBand& b)
    {
        on.setToggleState (b.on, juce::dontSendNotification);
        type.setButtonText (pr7s::bandTypeName (b.type).toUpperCase());
        gain.setValue (b.gainDb, juce::dontSendNotification);
        frequency.setValue (b.frequency, juce::dontSendNotification);
        q.setValue (b.q, juce::dontSendNotification);
        gain.setEnabled (! isCut (b.type));
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
        gain.setBounds (row());
        frequency.setBounds (row());
        q.setBounds (row());
    }

    EqEditor& editor;
    int index;
    juce::TextButton on, type;
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
    for (auto* k : { &bypassKey, &presetsKey, &lowCutKey, &highCutKey, &phaseKey, &lowSlopeKey, &highSlopeKey })
        addAndMakeVisible (*k);
    frequencyBar (lowCutFreq, pr7s::ChannelEq::minFrequency, pr7s::ChannelEq::maxFrequency);
    frequencyBar (highCutFreq, pr7s::ChannelEq::minHighCut, pr7s::ChannelEq::maxFrequency);
    gainBar (preGain);
    preGain.setTooltip ("The input gain, before everything (+-24 dB; Ctrl-click: 0)");
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
    lowCutFreq.setAlpha (shown.lowCutOn ? 1.0f : 0.5f);
    highCutFreq.setAlpha (shown.highCutOn ? 1.0f : 0.5f);
    for (int i = 0; i < bands.size(); ++i)
        bands[i]->refresh (shown.bands[(size_t) i]);
    repaint (0, 0, getWidth(), 36);
}

void EqEditor::change (const std::function<void (pr7s::ChannelEq&)>& edit, bool undoable)
{
    auto eq = shown;
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
    g.drawText ("EQ: " + channelName (studio, bus).toUpperCase(), juce::Rectangle<int> (14, 6, getWidth() - 300, 30), juce::Justification::centredLeft, true);
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
