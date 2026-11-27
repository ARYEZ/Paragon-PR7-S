#include "ui/StripEditor.h"

#include "ui/EqEditor.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    constexpr const char* moduleTitles[] = { "GATE / EXP", "SC FILTER", "COMPRESSOR", "SATURATION", "CONSOLE COLOUR", "MONO / WIDTH" };
    // Each module's width in knob columns (left to right: PR7-C's order; the channel's EQ sits between
    // the compressor and the saturation, in its own window).
    constexpr int moduleColumns[] = { 3, 2, 3, 2, 2, 2 };
    constexpr int titleHeight = 30, keyRowHeight = 26, knobSize = 56, knobNameHeight = 15, knobValueHeight = 16;

    juce::String db (double v) { return (v > 0.05 ? "+" : "") + juce::String (v, 1) + " DB"; }
    juce::String ms (double v) { return v < 10.0 ? juce::String (v, 1) + " MS" : juce::String (juce::roundToInt (v)) + " MS"; }
    juce::String hz (double v) { return v >= 1000.0 ? juce::String (v / 1000.0, v >= 10000.0 ? 1 : 2) + " KHZ" : juce::String (juce::roundToInt (v)) + " HZ"; }
    juce::String percent (double v) { return juce::String (juce::roundToInt (v)) + "%"; }
    juce::String ratioText (double v, bool soft)
    {
        if (soft && v < 1.05)
            return "SOFT";
        if (v >= 30.0)
            return juce::String::fromUTF8 ("\xe2\x88\x9e") + ":1";
        return juce::String (v, v < 10.0 ? 1 : 0) + ":1";
    }
    juce::String offOrPercent (double v) { return juce::roundToInt (v) == 0 ? juce::String ("OFF") : percent (v); }
}

//==============================================================================
StripEditor::StripEditor (pr7s::Studio& s, int busId) : studio (s), bus (busId)
{
    using S = pr7s::ChannelStrip;
    // GATE / EXP (PR7-C's: FAST / MED / SLOW the attack, HOLD, RELEASE exponential or LINEAR).
    addKey (0, 0, "gate.on", "GATE", "The gate / expander on: it turns the sound down when it's quiet",
            [] (const S& x) { return x.gate.on; }, [] (S& x) { x.gate.on = ! x.gate.on; });
    addKey (0, 0, "gate.exp", "EXP", "An expander instead of a gate: below the threshold, (RATIO - 1) dB more per dB, up to RANGE",
            [] (const S& x) { return x.gate.expander; }, [] (S& x) { x.gate.expander = ! x.gate.expander; });
    addKey (0, 0, "gate.minus30", "-30", "The threshold 30 dB lower (-90 to -30 dBFS)",
            [] (const S& x) { return x.gate.minus30; }, [] (S& x) { x.gate.minus30 = ! x.gate.minus30; });
    for (int a = 0; a < 3; ++a)
        addKey (0, 1, "gate." + pr7s::ChannelStrip::attackNames()[a].toLowerCase(), pr7s::ChannelStrip::attackNames()[a],
                "How quickly it opens: 0.1, 1 or 10 ms",
                [a] (const S& x) { return x.gate.attack == a; }, [a] (S& x) { x.gate.attack = a; });
    addKey (0, 2, "gate.linear", "LINEAR", "The release at a steady rate (all of RANGE in RELEASE), not easing off",
            [] (const S& x) { return x.gate.linear; }, [] (S& x) { x.gate.linear = ! x.gate.linear; });
    addKey (0, 2, "link", "LINK", "The gate and compressor work on both sides together, from the louder",
            [] (const S& x) { return x.link; }, [] (S& x) { x.link = ! x.link; });
    addKnob (0, "gate.threshold", "THRESHOLD", -60.0, 0.0, 0.0, [] (const S& x) { return x.gate.thresholdDb; },
             [] (S& x, double v) { x.gate.thresholdDb = (float) v; }, db, 0.1);
    addKnob (0, "gate.range", "RANGE", 0.0, 80.0, 0.0, [] (const S& x) { return x.gate.rangeDb; },
             [] (S& x, double v) { x.gate.rangeDb = (float) v; }, [] (double v) { return juce::String (v, 1) + " DB"; }, 0.1);
    addKnob (0, "gate.ratio", "RATIO", 1.0, 20.0, 3.0, [] (const S& x) { return x.gate.ratio; },
             [] (S& x, double v) { x.gate.ratio = (float) v; }, [] (double v) { return ratioText (v, false); });
    addKnob (0, "gate.hold", "HOLD", 0.0, 500.0, 50.0, [] (const S& x) { return x.gate.holdMs; },
             [] (S& x, double v) { x.gate.holdMs = (float) v; }, ms);
    addKnob (0, "gate.release", "RELEASE", 5.0, 3000.0, 200.0, [] (const S& x) { return x.gate.releaseMs; },
             [] (S& x, double v) { x.gate.releaseMs = (float) v; }, ms);

    // SC FILTER: what the gate and compressor hear (never the sound itself).
    addKey (1, 0, "sc.on", "SC FILTER", "Filters what the gate and compressor listen to: the sound itself isn't touched",
            [] (const S& x) { return x.sc.on; }, [] (S& x) { x.sc.on = ! x.sc.on; });
    addKey (1, 1, "sc.highq", "HIGH Q", "The filters resonant (Q 2.5)",
            [] (const S& x) { return x.sc.highQ; }, [] (S& x) { x.sc.highQ = ! x.sc.highQ; });
    addKey (1, 1, "sc.edge", "EDGE", "The detector hears the harsh upper mids (6 kHz) 8 dB louder, so the dynamics clamp down on them",
            [] (const S& x) { return x.sc.edge; }, [] (S& x) { x.sc.edge = ! x.sc.edge; });
    addKey (1, 2, "sc.listen", "LISTEN", "Hear what the gate and compressor hear, instead of the sound",
            [] (const S& x) { return x.sc.listen; }, [] (S& x) { x.sc.listen = ! x.sc.listen; });
    addKnob (1, "sc.hpf", "HPF", 20.0, 200.0, 60.0, [] (const S& x) { return x.sc.hpf; },
             [] (S& x, double v) { x.sc.hpf = (float) v; }, hz);
    addKnob (1, "sc.lpf", "LPF", 1000.0, 20000.0, 5000.0, [] (const S& x) { return x.sc.lpf; },
             [] (S& x, double v) { x.sc.lpf = (float) v; }, hz);

    // COMPRESSOR: VCA / OPTO / FET / MODERN (MODERN's CLEAN / 2ND / 2ND + 3RD).
    addKey (2, 0, "comp.on", "COMP", "The compressor on",
            [] (const S& x) { return x.comp.on; }, [] (S& x) { x.comp.on = ! x.comp.on; });
    addKey (2, 0, "comp.auto", "AUTO", "A release that stretches: quick after a short peak, slower after long compression",
            [] (const S& x) { return x.comp.autoRelease; }, [] (S& x) { x.comp.autoRelease = ! x.comp.autoRelease; });
    const juce::StringArray characterTips { "Clean and precise: follows the loudness, a near-hard knee",
                                            "Smooth: slower, a wide soft knee, a release that stretches",
                                            "Fast and aggressive: catches every transient, a touch of grit as it works",
                                            "A modern studio compressor's feel: knees that change with the ratio, its own distortion" };
    for (int c = 0; c < 4; ++c)
        addKey (2, 1, "comp." + pr7s::ChannelStrip::characterNames()[c].toLowerCase(), pr7s::ChannelStrip::characterNames()[c], characterTips[c],
                [c] (const S& x) { return x.comp.character == c; }, [c] (S& x) { x.comp.character = c; });
    for (int d = 0; d < 3; ++d)
        addKey (2, 2, "comp.distortion" + juce::String (d), pr7s::ChannelStrip::distortionNames()[d],
                "MODERN's distortion as it compresses: none, mostly 2nd harmonic, or 2nd and 3rd",
                [d] (const S& x) { return x.comp.distortion == d; }, [d] (S& x) { x.comp.distortion = d; });
    addKnob (2, "comp.threshold", "THRESHOLD", -50.0, 0.0, 0.0, [] (const S& x) { return x.comp.thresholdDb; },
             [] (S& x, double v) { x.comp.thresholdDb = (float) v; }, db, 0.1);
    addKnob (2, "comp.ratio", "RATIO", 1.0, 30.0, 4.0, [] (const S& x) { return x.comp.ratio; },
             [] (S& x, double v) { x.comp.ratio = (float) v; }, [] (double v) { return ratioText (v, true); });
    addKnob (2, "comp.attack", "ATTACK", 0.1, 30.0, 3.0, [] (const S& x) { return x.comp.attackMs; },
             [] (S& x, double v) { x.comp.attackMs = (float) v; }, ms);
    addKnob (2, "comp.release", "RELEASE", 50.0, 3000.0, 300.0, [] (const S& x) { return x.comp.releaseMs; },
             [] (S& x, double v) { x.comp.releaseMs = (float) v; }, ms);
    addKnob (2, "comp.makeup", "MAKE UP", 0.0, 20.0, 0.0, [] (const S& x) { return x.comp.makeUpDb; },
             [] (S& x, double v) { x.comp.makeUpDb = (float) v; }, db, 0.1);

    // SATURATION.
    addKey (3, 0, "sat.on", "SATURATION", "The saturation on: harmonics, the level kept as DRIVE goes up",
            [] (const S& x) { return x.sat.on; }, [] (S& x) { x.sat.on = ! x.sat.on; });
    addKey (3, 1, "sat.clip", "VCA CLIP", "A much harder clip on the peaks first, for crunch",
            [] (const S& x) { return x.sat.vcaClip; }, [] (S& x) { x.sat.vcaClip = ! x.sat.vcaClip; });
    addKnob (3, "sat.drive", "DRIVE", 0.0, 24.0, 0.0, [] (const S& x) { return x.sat.driveDb; },
             [] (S& x, double v) { x.sat.driveDb = (float) v; }, db, 0.1);
    addKnob (3, "sat.harmonics", "HARMONICS", 0.0, 100.0, 0.0, [] (const S& x) { return x.sat.harmonics; },
             [] (S& x, double v) { x.sat.harmonics = (float) v; },
             [] (double v) { return v < 50.0 ? "2ND " + percent (100.0 - v) : "3RD " + percent (v); }, 1.0);
    addKnob (3, "sat.mix", "MIX", 0.0, 100.0, 0.0, [] (const S& x) { return x.sat.mix; },
             [] (S& x, double v) { x.sat.mix = (float) v; }, percent, 1.0);

    // CONSOLE COLOUR: THD, V GAIN, the console channel (ANALOG: the right side the next one).
    addKey (4, 0, "colour.on", "COLOUR", "The console colour on: THD, the noise and this console channel's own small differences",
            [] (const S& x) { return x.colour.on; }, [] (S& x) { x.colour.on = ! x.colour.on; });
    addKey (4, 1, "colour.analog", "ANALOG", "The right side its own console channel (the next one), as a real console pair",
            [] (const S& x) { return ! x.colour.digital; }, [] (S& x) { x.colour.digital = false; });
    addKey (4, 1, "colour.digital", "DIGITAL", "Both sides the same console channel",
            [] (const S& x) { return x.colour.digital; }, [] (S& x) { x.colour.digital = true; });
    addKey (4, 2, "colour.random", "RANDOM CH.", "This channel strip to a console channel no other strip in the song uses",
            [] (const S&) { return false; }, [] (S&) {});
    addKnob (4, "colour.thd", "THD", 0.0, 100.0, 0.0, [] (const S& x) { return x.colour.thd; },
             [] (S& x, double v) { x.colour.thd = (float) v; }, offOrPercent, 1.0);
    addKnob (4, "colour.vgain", "V GAIN", 0.0, 100.0, 0.0, [] (const S& x) { return x.colour.vGain; },
             [] (S& x, double v) { x.colour.vGain = (float) v; }, offOrPercent, 1.0);
    addKnob (4, "colour.channel", "CHANNEL", 1.0, 72.0, 0.0, [] (const S& x) { return x.colour.channel; },
             [] (S& x, double v) { x.colour.channel = juce::roundToInt (v); },
             [] (double v) { return "CH " + juce::String (juce::roundToInt (v)); }, 1.0);

    // MONO MAKER / STEREO WIDTH.
    addKey (5, 0, "stereo.mono", "MONO MAKER", "Below its frequency, mono (the sides kept in phase above it)",
            [] (const S& x) { return x.stereo.monoOn; }, [] (S& x) { x.stereo.monoOn = ! x.stereo.monoOn; });
    addKey (5, 1, "stereo.width", "WIDTH", "The sides turned up or down (the middle, and so the mono sum, never touched)",
            [] (const S& x) { return x.stereo.widthOn; }, [] (S& x) { x.stereo.widthOn = ! x.stereo.widthOn; });
    addKnob (5, "stereo.monofreq", "MONO BELOW", 20.0, 500.0, 100.0, [] (const S& x) { return x.stereo.monoHz; },
             [] (S& x, double v) { x.stereo.monoHz = (float) v; }, hz);
    addKnob (5, "stereo.amount", "WIDTH", 0.0, 200.0, 0.0, [] (const S& x) { return x.stereo.width; },
             [] (S& x, double v) { x.stereo.width = (float) v; }, percent, 1.0);

    // RANDOM CH. is an action, not a setting.
    if (auto* random = dynamic_cast<juce::TextButton*> (findControl ("colour.random")))
        random->onClick = [this] { studio.randomiseConsoleChannels (bus); refresh(); };

    menuKey.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    menuKey.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    menuKey.setWantsKeyboardFocus (false);
    menuKey.setTooltip ("Every module off; the console channels at random");
    menuKey.onClick = [this] { showMenu (studio, bus, &menuKey); };
    addAndMakeVisible (menuKey);

    shown = current();
    refresh();
    setSize (preferredWidth, preferredHeight);
    startTimerHz (15);
}

StripEditor::~StripEditor() = default;

void StripEditor::addKnob (int module, const juce::String& id, const juce::String& name, double low, double high, double centre,
                           std::function<double (const pr7s::ChannelStrip&)> get, std::function<void (pr7s::ChannelStrip&, double)> set,
                           std::function<juce::String (double)> text, double step)
{
    auto knob = std::make_unique<Knob>();
    knob->id = id;
    knob->name = name;
    knob->get = std::move (get);
    knob->set = std::move (set);
    knob->text = std::move (text);
    knob->module = module;
    knob->slider = std::make_unique<ValueSlider>();
    auto& slider = *knob->slider;
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (low, high, step);
    if (centre > low && centre < high)
        slider.setSkewFactorFromMidPoint (centre);
    slider.setWantsKeyboardFocus (false);
    slider.setTooltip (name + ". Double-click to type a value.");
    slider.parse = [low, high] (const juce::String& t) -> std::optional<double>
    {
        const auto digits = t.retainCharacters ("0123456789.-");
        if (digits.isEmpty())
            return std::nullopt;
        auto v = digits.getDoubleValue();
        if (t.containsIgnoreCase ("k") && v < 100.0)
            v *= 1000.0;
        return juce::jlimit (low, high, v);
    };
    auto* raw = knob.get();
    slider.onDragStart = [this] { dragging = true; };
    slider.onDragEnd = [this, raw]
    {
        dragging = false;
        change ([raw] (pr7s::ChannelStrip& x) { raw->set (x, raw->slider->getValue()); }, true);
    };
    slider.onValueChange = [this, raw]
    {
        change ([raw] (pr7s::ChannelStrip& x) { raw->set (x, raw->slider->getValue()); }, ! dragging);
        repaint();
    };
    addAndMakeVisible (slider);
    knobs.push_back (std::move (knob));
}

void StripEditor::addKey (int module, int row, const juce::String& id, const juce::String& name, const juce::String& tip,
                          std::function<bool (const pr7s::ChannelStrip&)> lit, std::function<void (pr7s::ChannelStrip&)> click)
{
    auto key = std::make_unique<Key>();
    key->id = id;
    key->module = module;
    key->row = row;
    key->lit = std::move (lit);
    key->click = std::move (click);
    key->button = std::make_unique<juce::TextButton> (name);
    auto& button = *key->button;
    button.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    button.setWantsKeyboardFocus (false);
    button.setTooltip (tip);
    auto* raw = key.get();
    button.onClick = [this, raw] { change (raw->click, true); };
    addAndMakeVisible (button);
    keys.push_back (std::move (key));
}

pr7s::ChannelStrip StripEditor::current() const
{
    const auto* strip = studio.getChannelStrip (bus);
    return strip != nullptr ? *strip : pr7s::ChannelStrip {};
}

void StripEditor::change (const std::function<void (pr7s::ChannelStrip&)>& edit, bool undoable)
{
    if (studio.getChannelStrip (bus) == nullptr)
        return;
    auto s = current();
    edit (s);
    studio.setChannelStrip (bus, s, undoable);
    shown = current();
    if (dragging)
        for (auto& key : keys)
            key->button->setToggleState (key->lit (shown), juce::dontSendNotification);
    else
        refresh();   // (keys lit, MODERN's distortion keys enabled or not, knobs as set)
}

void StripEditor::setBus (int busId)
{
    bus = busId;
    shown = current();
    refresh();
    repaint();
}

void StripEditor::refresh()
{
    shown = current();
    for (auto& knob : knobs)
        if (! knob->slider->isMouseButtonDown())
            knob->slider->setValue (knob->get (shown), juce::dontSendNotification);
    for (auto& key : keys)
    {
        key->button->setToggleState (key->lit (shown), juce::dontSendNotification);
        if (key->id.startsWith ("comp.distortion"))
            key->button->setEnabled (shown.comp.character == 3);
    }
    const auto enabled = studio.getChannelStrip (bus) != nullptr;
    for (auto& knob : knobs)
        knob->slider->setEnabled (enabled);
    repaint();
}

void StripEditor::timerCallback()
{
    if (! isShowing())
        return;
    if (! (current() == shown) && ! dragging)
        refresh();
    const auto gate = studio.getStripReductionDb (bus, false), comp = studio.getStripReductionDb (bus, true);
    if (std::abs (gate - gateReduction) > 0.1f || std::abs (comp - compReduction) > 0.1f)
    {
        gateReduction = gate;
        compReduction = comp;
        repaint (getModuleArea (0));
        repaint (getModuleArea (2));
    }
}

juce::Rectangle<int> StripEditor::getModuleArea (int index) const
{
    auto area = getLocalBounds().reduced (14, 0).withTrimmedTop (44).withTrimmedBottom (14);
    const auto total = [] { int t = 0; for (auto c : moduleColumns) t += c; return t; }();
    const auto gap = 10;
    const auto unit = (area.getWidth() - gap * (numModules - 1)) / (double) total;
    auto x = (double) area.getX();
    for (int m = 0; m < numModules; ++m)
    {
        const auto w = unit * moduleColumns[m];
        if (m == index)
            return juce::Rectangle<int> (juce::roundToInt (x), area.getY(), juce::roundToInt (w), area.getHeight());
        x += w + gap;
    }
    return {};
}

void StripEditor::resized()
{
    menuKey.setBounds (getWidth() - 14 - 110, 8, 110, 28);
    for (int m = 0; m < numModules; ++m)
    {
        auto area = getModuleArea (m).reduced (8, 0);
        area.removeFromTop (titleHeight);
        // Its keys, a row at a time, shared across.
        for (int row = 0; row < 3; ++row)
        {
            std::vector<Key*> inRow;
            for (auto& key : keys)
                if (key->module == m && key->row == row)
                    inRow.push_back (key.get());
            auto line = area.removeFromTop (keyRowHeight);
            area.removeFromTop (4);
            if (inRow.empty())
                continue;
            const auto w = (line.getWidth() - 4 * ((int) inRow.size() - 1)) / (int) inRow.size();
            for (auto* key : inRow)
            {
                key->button->setBounds (line.removeFromLeft (w));
                line.removeFromLeft (4);
            }
        }
        area.removeFromTop (6);
        // Its knobs, in its columns.
        std::vector<Knob*> mine;
        for (auto& knob : knobs)
            if (knob->module == m)
                mine.push_back (knob.get());
        const auto columns = moduleColumns[m];
        const auto cell = area.getWidth() / columns;
        const auto rowHeight = knobNameHeight + knobSize + knobValueHeight + 6;
        for (size_t i = 0; i < mine.size(); ++i)
        {
            const auto col = (int) i % columns, row = (int) i / columns;
            const auto cellArea = juce::Rectangle<int> (area.getX() + col * cell, area.getY() + row * rowHeight, cell, rowHeight);
            mine[i]->slider->setBounds (cellArea.withTrimmedTop (knobNameHeight).withHeight (knobSize).withSizeKeepingCentre (knobSize, knobSize));
        }
    }
}

void StripEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0a0d));
    ParagonColours::setFill (g, ParagonColours::accent, juce::Rectangle<float> (0.0f, 0.0f, (float) getWidth(), 3.0f));
    g.fillRect (0, 0, getWidth(), 3);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (18.0f));
    const auto title = "CHANNEL STRIP: " + EqEditor::channelName (studio, bus).toUpperCase();
    g.drawText (title, juce::Rectangle<int> (14, 8, getWidth() - 160, 28), juce::Justification::centredLeft, true);
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::textFont (12.0f));
    g.drawText (shown.inUse() ? "IN USE: " + juce::String (pr7s::ChannelStrip::latencySamples) + " SAMPLES' DELAY, LINED UP BY THE DELAY COMPENSATION"
                              : juce::String ("ALL OFF: THE SOUND PASSES UNTOUCHED"),
                juce::Rectangle<int> (getWidth() / 2 - 120, 8, getWidth() / 2 - 20, 28), juce::Justification::centredLeft, true);

    for (int m = 0; m < numModules; ++m)
    {
        const auto area = getModuleArea (m);
        if (! g.getClipBounds().intersects (area))
            continue;
        g.setColour (juce::Colour (0xff17141a));
        g.fillRoundedRectangle (area.toFloat(), 6.0f);
        g.setColour (juce::Colour (0xff2a252e));
        g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 6.0f, 1.0f);
        auto head = area.reduced (10, 0).removeFromTop (titleHeight);
        g.setFont (ParagonLookAndFeel::titleFont (15.0f));
        ParagonColours::setTextFill (g, ParagonColours::text, moduleTitles[m], head, juce::Justification::centredLeft);
        g.drawText (moduleTitles[m], head, juce::Justification::centredLeft, false);
        // The gate's and compressor's gain reduction, as a bar along their title.
        if (m == 0 || m == 2)
        {
            const auto reduction = m == 0 ? gateReduction : compReduction;
            auto meter = head.removeFromRight (head.getWidth() / 2).withSizeKeepingCentre (head.getWidth() / 2, 8);
            g.setColour (juce::Colour (0xff221e25));
            g.fillRect (meter);
            const auto amount = juce::jlimit (0.0f, 1.0f, reduction / 24.0f);
            if (amount > 0.0f)
            {
                const auto lit = meter.withWidth (juce::roundToInt ((float) meter.getWidth() * amount)).toFloat();
                ParagonColours::setFill (g, ParagonColours::accent, lit);
                g.fillRect (lit);
            }
            g.setColour (ParagonColours::textDim);
            g.setFont (ParagonLookAndFeel::textFont (11.0f));
            g.drawText ("GR " + juce::String (reduction, 1), meter.translated (0, -12).withHeight (12), juce::Justification::centredRight, false);
        }
        // Each knob's name over it and value under it.
        for (auto& knob : knobs)
            if (knob->module == m)
            {
                const auto b = knob->slider->getBounds();
                g.setColour (ParagonColours::textDim);
                g.setFont (ParagonLookAndFeel::textFont (12.0f));
                g.drawText (knob->name, b.withY (b.getY() - knobNameHeight).withHeight (knobNameHeight).expanded (20, 0), juce::Justification::centred, false);
                g.setFont (ParagonLookAndFeel::textFont (13.0f));
                const auto value = knob->text (knob->slider->getValue());
                const auto valueArea = b.withY (b.getBottom()).withHeight (knobValueHeight).expanded (20, 0);
                ParagonColours::setTextFill (g, ParagonColours::text, value, valueArea, juce::Justification::centred);
                g.drawText (value, valueArea, juce::Justification::centred, false);
            }
    }
}

void StripEditor::setKnob (const juce::String& id, double value)
{
    for (auto& knob : knobs)
        if (knob->id == id)
        {
            auto* raw = knob.get();
            raw->slider->setValue (value, juce::dontSendNotification);
            change ([raw] (pr7s::ChannelStrip& x) { raw->set (x, raw->slider->getValue()); }, true);
            refresh();
        }
}

double StripEditor::getKnob (const juce::String& id) const
{
    for (auto& knob : knobs)
        if (knob->id == id)
            return knob->slider->getValue();
    return 0.0;
}

void StripEditor::clickKey (const juce::String& id)
{
    for (auto& key : keys)
        if (key->id == id && key->button->onClick)
            key->button->onClick();
}

bool StripEditor::isKeyLit (const juce::String& id) const
{
    for (auto& key : keys)
        if (key->id == id)
            return key->button->getToggleState();
    return false;
}

juce::Component* StripEditor::findControl (const juce::String& id) const
{
    for (auto& key : keys)
        if (key->id == id)
            return key->button.get();
    for (auto& knob : knobs)
        if (knob->id == id)
            return knob->slider.get();
    return nullptr;
}

void StripEditor::showMenu (pr7s::Studio& studio, int busId, juce::Component* target)
{
    juce::PopupMenu menu;
    menu.addItem (1, "Every Module Off");
    menu.addItem (2, "Back to How It Starts");
    menu.addSeparator();
    menu.addItem (3, "Random Console Channel (This Strip)");
    menu.addItem (4, "Random Console Channels (Every Strip With Its Colour On)");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [&studio, busId] (int chosen)
    {
        const auto* strip = studio.getChannelStrip (busId);
        if (strip == nullptr || chosen <= 0)
            return;
        auto s = *strip;
        if (chosen == 1)
        {
            s.gate.on = s.comp.on = s.sat.on = s.colour.on = s.stereo.monoOn = s.stereo.widthOn = s.sc.listen = false;
            studio.setChannelStrip (busId, s);
        }
        else if (chosen == 2)
            studio.setChannelStrip (busId, {});
        else if (chosen == 3)
            studio.randomiseConsoleChannels (busId);
        else if (chosen == 4)
            studio.randomiseConsoleChannels (std::nullopt);
    });
}

//==============================================================================
StripWindow::StripWindow (pr7s::Studio& studio, int busId)
    : juce::DocumentWindow ("Paragon PR7-S: Channel Strip", juce::Colour (0xff0c0a0d), juce::DocumentWindow::closeButton)
{
    editor = new StripEditor (studio, busId);
    setContentOwned (editor, true);
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (1000, 400, 3840, 2160);
}

void StripWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}
