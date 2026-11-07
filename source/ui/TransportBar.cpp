#include "TransportBar.h"

#include <map>

#include "ui/LedDisplay.h"
#include "ui/Logo.h"
#include "ui/Pearl.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    const auto orange = LedDisplay::ledOrange;
    constexpr float screenSize = 0.55f;

    juce::FontOptions font (float height, int style = juce::Font::bold) { return ParagonLookAndFeel::textFont (height, style); }

    void stamped (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height,
                  juce::Justification justification = juce::Justification::centred)
    {
        g.setFont (font (height));
        ParagonLookAndFeel::drawStampedText (g, text, area, justification, Finish::ink());
    }

    // The signatures offered (the common ones in pop, rock and metal).
    const std::pair<int, int> signatures[] = { { 4, 4 }, { 3, 4 }, { 2, 4 }, { 5, 4 }, { 6, 4 }, { 7, 4 }, { 6, 8 }, { 7, 8 }, { 9, 8 }, { 12, 8 } };
}

TransportBar::TransportBar (pr7s::Studio& studioToUse)
    : studio (studioToUse)
{
    for (auto* key : std::initializer_list<juce::TextButton*> { &toStart, &stopKey, &playKey, &recordKey, &cycleKey, &clickKey })
    {
        key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        addAndMakeVisible (*key);
    }
    toStart.setTooltip ("Back to the start (or the cycle's start). Home key");
    stopKey.setTooltip ("Stop. Press again to go back to where playing started. Space toggles play and stop");
    playKey.setTooltip ("Play. Space");
    recordKey.setTooltip ("Record into the armed track (red R in the track list). R key. Press again while recording to stop recording and keep playing. "
                          "Right-click: the record modes (what happens to what's there, and to a cycle's laps)");
    cycleKey.setTooltip ("CYCLE: play round the range marked on the ruler (drag across the ruler's top strip to set it). / key");
    clickKey.setTooltip ("CLICK: the metronome, on every beat (higher on the first of each bar). C key");

    toStart.onClick = [this] { studio.goToStart(); };
    stopKey.onClick = [this] { studio.stop(); };
    playKey.onClick = [this] { studio.play(); };
    recordKey.onClick = [this]
    {
        if (std::exchange (recordKey.popup, false))
            showRecordModes();
        else
            studio.record();
    };
    cycleKey.onClick = [this] { studio.setCycleOn (! studio.getProject().loopOn); };
    clickKey.onClick = [this] { studio.setClick (! studio.getProject().click); };

    studio.addChangeListener (this);
    Finish::changes().addChangeListener (this);
    refreshKeys();
    startTimerHz (30);
}

TransportBar::~TransportBar()
{
    Finish::changes().removeChangeListener (this);
    studio.removeChangeListener (this);
}

void TransportBar::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &Finish::changes())
        resized();   // the grain differs on black
    else if (studio.getChangeVersion() == seenChange)
        return;      // (0.34.3: only the mix moved; nothing here shows it)
    else
        seenChange = studio.getChangeVersion();
    refreshKeys();
    repaint();
}

void TransportBar::timerCallback()
{
    // The keys follow the engine (playing can stop by itself, e.g. on a new project).
    refreshKeys();
    if (const auto position = studio.getEngine().getPosition(); position != shownPosition)
    {
        shownPosition = position;
        repaint (positionScreen.getUnion (timeScreen).expanded (4.0f).getSmallestIntegerContainer());
    }
    // The signature where the playhead is (0.45.0).
    if (const auto now = studio.getSignatureNow(); now != shownSignature)
    {
        shownSignature = now;
        repaint (signatureScreen.expanded (4.0f).getSmallestIntegerContainer());
    }
    // The tempo track: the tempo where the playhead is (0.10.0); a tapped tempo while tapping (0.45.0).
    if (const auto text = getTempoText() + (isShowingTap() ? "t" : ""); text != shownTempo)
    {
        shownTempo = text;
        repaint (tempoScreen.expanded (4.0f).getSmallestIntegerContainer());
    }
    followPerformance();
    followOutput();
}

void TransportBar::followOutput()
{
    // (0.73.2, the owner asked) OUT holds the loudest peak since it was last clicked, as a mixer's
    // max readout does. The output's peaks are read every tick (30 a second, so none is missed);
    // the number only ever rises, so the screen is redrawn only when a louder peak comes.
    auto& engine = studio.getEngine();
    outPeak = juce::jmax (outPeak, engine.getOutputMeter (0).takePeak(), engine.getOutputMeter (1).takePeak());
    const auto db = juce::Decibels::gainToDecibels (outPeak, LevelMeter::minDb);
    if (std::abs (db - shownOutDb) >= 0.05f)
    {
        shownOutDb = db;
        repaint (outScreen.expanded (4.0f).getSmallestIntegerContainer());
    }
}

void TransportBar::resetOutPeak()
{
    outPeak = 0.0f;
    shownOutDb = LevelMeter::minDb;
    repaint (outScreen.expanded (4.0f).getSmallestIntegerContainer());
}

void TransportBar::followPerformance()
{
    // The average as it is; the peak held a moment, then falling (as the level meters' peaks do).
    const auto now = studio.getEngine().takePerformance();
    shownAverage = now.average;
    shownPeak = juce::jmax (now.peak, shownPeak * 0.92f);
    overloadLit = now.overloads > 0;
    const auto shown = juce::roundToInt (shownAverage * 100.0f) * 1000 + juce::roundToInt (shownPeak * 100.0f) * 2 + (overloadLit ? 1 : 0);
    if (shown != shownCpu)
    {
        shownCpu = shown;
        repaint (cpuScreen.expanded (4.0f).getSmallestIntegerContainer());
    }
}

juce::String TransportBar::getTooltip()
{
    if (tempoScreen.contains (getMouseXYRelative().toFloat()))
        return studio.getProject().followsTempoTrack()
                 ? "The tempo track's tempo where the playhead is (change it on the tempo track). Right-click: Activate Tempo Track"
                 : "The song's tempo: drag up or down, or double-click to type it. Right-click: Activate Tempo Track, or add the tempo track";
    if (outScreen.contains (getMouseXYRelative().toFloat()))
        return "OUT: the loudest peak of the output (left or right) since you last clicked here, in dB. "
               "Red over 0 dB (clipping). Click to start again";
    if (! cpuScreen.contains (getMouseXYRelative().toFloat()))
        return {};
    const auto overloads = studio.getEngine().getOverloads();   // (not takePerformance: that takes the peak the meter shows)
    return "CPU: the audio processing load. Average " + juce::String (juce::roundToInt (shownAverage * 100.0f)) + "%, peak "
         + juce::String (juce::roundToInt (shownPeak * 100.0f)) + "% (of the time each block has). "
         + (overloads > 0 ? juce::String (overloads) + " overload" + (overloads == 1 ? "" : "s") + " (dropouts): click to reset. "
                                + "A bigger buffer in SETTINGS, or fewer effects and instruments, helps."
                          : juce::String ("No overloads."));
}

void TransportBar::setToolsWidth (int width)
{
    if (width == toolsWidth)
        return;
    toolsWidth = width;
    resized();
    repaint();
}

void TransportBar::refreshKeys()
{
    auto& engine = studio.getEngine();
    const auto& project = studio.getProject();
    playKey.setToggleState (engine.isPlaying(), juce::dontSendNotification);
    stopKey.setToggleState (! engine.isPlaying(), juce::dontSendNotification);
    recordKey.setToggleState (engine.isRecording(), juce::dontSendNotification);
    cycleKey.setToggleState (project.loopOn, juce::dontSendNotification);
    clickKey.setToggleState (project.click, juce::dontSendNotification);
}

void TransportBar::drawFixedPitch (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height)
{
    // Bebas's figures differ in width: each in a cell as wide as the widest,
    // the separators and spaces in narrower ones, the whole fitted and centred.
    juce::Font f (font (height));
    // (0.57.3) The widest figure's width, measured once per font and height (measuring asked the
    // system's font lookup for each figure: a sixth of the screens' time, 30 times a second).
    static std::map<std::pair<juce::String, float>, float> widths;   // (message thread only)
    const auto key = std::pair { f.getTypefaceName() + "/" + f.getTypefaceStyle(), height };
    auto known = widths.find (key);
    if (known == widths.end())
    {
        if (widths.size() > 64)
            widths.clear();
        known = widths.emplace (key, juce::jmax (juce::GlyphArrangement::getStringWidth (f, "0"), juce::GlyphArrangement::getStringWidth (f, "8"),
                                                 juce::GlyphArrangement::getStringWidth (f, "4")) * 1.05f).first;
    }
    const auto digit = known->second;
    // (A figure space, U+2007, is a blank as wide as a figure: padding that keeps the value's width.)
    const auto cellFor = [digit] (juce::juce_wchar c) { return juce::CharacterFunctions::isDigit (c) || c == 0x2007 ? digit : digit * 0.5f; };
    auto total = 0.0f;
    for (auto c : text)
        total += cellFor (c);
    if (total > area.getWidth() && total > 0.0f)
    {
        const auto scale = area.getWidth() / total;
        f = juce::Font (font (height * scale));
        return drawFixedPitch (g, text, area, height * scale);
    }
    g.setFont (f);
    auto x = area.getCentreX() - total * 0.5f;
    for (auto c : text)
    {
        const auto w = cellFor (c);
        if (c != 0x2007)
            g.drawText (juce::String::charToString (c), juce::Rectangle<float> (x, area.getY(), w, area.getHeight()), juce::Justification::centred, false);
        x += w;
    }
}

juce::String TransportBar::screenPosition (const juce::String& text, int numerator)
{
    // Each field padded to a fixed number of figure-wide blanks (bars 2, the
    // beat as many as the signature's top, the sixteenth 1, ticks 3), so the
    // value is always as wide and stays still as it counts (0.13.1, the owner
    // saw it pulse: "1. 1. 1.  0" grew to "1. 1. 2. 34", the whole re-centred).
    const juce::juce_wchar blank = 0x2007;
    auto fields = juce::StringArray::fromTokens (text, ".", {});
    const int widths[] = { 2, juce::String (juce::jmax (1, numerator)).length(), 1, 3 };
    if (fields.size() != 4)
        return text;
    juce::StringArray padded;
    for (int i = 0; i < 4; ++i)
    {
        const auto field = fields[i].trim();
        padded.add (juce::String::repeatedString (juce::String::charToString (blank), juce::jmax (0, widths[i] - field.length())) + field);
    }
    return padded.joinIntoString (". ");
}

juce::String TransportBar::getPositionText() const
{
    return pr7s::formatBarsBeats (studio.getEngine().getPosition(), studio.getProject());
}

juce::String TransportBar::getTempoText() const
{
    if (isShowingTap())
        return studio.getLastTap().bpm > 0.0 ? juce::String (studio.getLastTap().bpm, 2) : juce::String ("TAP");
    return juce::String (studio.getTempoNow(), 2);   // (the tempo track's where the playhead is, else the fixed tempo)
}

bool TransportBar::isShowingTap() const
{
    return studio.isTapping (juce::Time::getMillisecondCounterHiRes() / 1000.0);
}

void TransportBar::tap()
{
    studio.tapTempo (juce::Time::getMillisecondCounterHiRes() / 1000.0, getSelectionStart ? getSelectionStart() : std::nullopt);
    shownTempo = {};
    repaint (tempoScreen.expanded (4.0f).getSmallestIntegerContainer());
}

void TransportBar::showTapModes (juce::Component* target)
{
    using Mode = pr7s::Studio::TapMode;
    juce::PopupMenu menu;
    menu.addSectionHeader ("TAP TEMPO MODE");
    for (auto m : { Mode::automatic, Mode::selectionStart, Mode::playbackStart, Mode::cursor, Mode::projectTempo, Mode::displayOnly })
        menu.addItem ((int) m + 1, pr7s::Studio::tapModeName (m), true, studio.getTapMode() == m);
    menu.showMenuAsync (target != nullptr ? juce::PopupMenu::Options().withTargetComponent (target) : juce::PopupMenu::Options(), [this] (int chosen)
    {
        if (chosen > 0)
            studio.setTapMode ((Mode) (chosen - 1));
    });
}

void TransportBar::resized()
{
    satin = Finish::makeSatinGrain (juce::jmax (1, getWidth() * 2), juce::jmax (1, getHeight() * 2), 7071);

    // Left to right: the badge; AUTO-SCROLL (its key and menu arrow: the
    // main window puts them in `autoScrollArea`); the keys; the CPU screen;
    // the position screen; the tempo and the signature; on a wide window the
    // tools (`toolsArea`); the output meters at the right end.
    const auto wide = toolsWidth > 0;
    const auto keyPitch = wide ? 66.0f : 70.0f;
    const auto autoWidth = keyPitch + 3.0f + 20.0f;
    const auto cpuWidth = wide ? 92.0f : 96.0f;
    const auto tempoWidth = wide ? 116.0f : 128.0f;
    const auto outWidth = cpuWidth;   // (0.73.0: OUT a screen beside CPU; TAP moved to the header)
    const auto signatureWidth = wide ? 88.0f : 96.0f;

    auto area = getLocalBounds().toFloat().reduced (14.0f, 12.0f);
    badge = area.removeFromLeft (! wide && getWidth() < 1700 ? 150.0f : 190.0f);
    area.removeFromLeft (16.0f);

    // The group from AUTO-SCROLL to the signature (and the tools): on one
    // screen it starts after the badge and the position screen takes what's
    // left; wide, the position screen has its own width and the group sits
    // in the middle (as near as the badge and the meters allow), so on three
    // screens it's all on the middle one.
    const auto leftWidth = autoWidth + 12.0f + 6.0f * keyPitch + 12.0f + cpuWidth + 12.0f + outWidth + 12.0f;
    const auto rightWidth = 12.0f + tempoWidth + 12.0f + signatureWidth + (wide ? 20.0f + (float) toolsWidth : 0.0f);
    auto x = area.getX();
    auto positionWidth = area.getWidth() - leftWidth - rightWidth;
    if (wide)
    {
        positionWidth = (float) widePositionWidth;
        const auto groupWidth = leftWidth + positionWidth + rightWidth;
        x = juce::jlimit (area.getX(), juce::jmax (area.getX(), area.getRight() - groupWidth), ((float) getWidth() - groupWidth) * 0.5f);
    }
    juce::Rectangle<float> row (x, area.getY(), leftWidth + positionWidth + rightWidth, area.getHeight());

    autoScrollArea = row.removeFromLeft (autoWidth).withSizeKeepingCentre (autoWidth, 40.0f).translated (0.0f, -6.0f).toNearestInt();
    autoScrollStamp = juce::Rectangle<float> ((float) autoScrollArea.getX(), area.getBottom() - 14.0f, keyPitch - 8.0f, 14.0f);
    row.removeFromLeft (12.0f);
    keysArea = row.removeFromLeft (6.0f * keyPitch);
    auto keys = keysArea;
    for (auto* key : std::initializer_list<juce::TextButton*> { &toStart, &stopKey, &playKey, &recordKey, &cycleKey, &clickKey })
        key->setBounds (keys.removeFromLeft (keyPitch).withSizeKeepingCentre (keyPitch - 8.0f, 40.0f).translated (0.0f, -6.0f).toNearestInt());
    row.removeFromLeft (12.0f);
    cpuScreen = row.removeFromLeft (cpuWidth);
    row.removeFromLeft (12.0f);
    outScreen = row.removeFromLeft (outWidth);
    row.removeFromLeft (12.0f);
    // Two screens alike, POSITION and TIME, in the position's room.
    timeScreen = row.removeFromLeft (positionWidth);
    positionScreen = timeScreen.removeFromLeft ((positionWidth - 12.0f) * 0.5f);
    timeScreen.removeFromLeft (12.0f);
    row.removeFromLeft (12.0f);
    tempoScreen = row.removeFromLeft (tempoWidth);
    row.removeFromLeft (12.0f);
    signatureScreen = row.removeFromLeft (signatureWidth);
    toolsArea = {};
    if (wide)
    {
        row.removeFromLeft (20.0f);
        toolsArea = row.removeFromLeft ((float) toolsWidth).withSizeKeepingCentre ((float) toolsWidth, 40.0f)
                       .withY ((float) toStart.getBounds().getY()).toNearestInt();
    }
    keyPitchNow = keyPitch;
}

void TransportBar::paint (juce::Graphics& g)
{
    Pearl::paintUnit (g, getLocalBounds().toFloat().reduced (4.0f), satin);
    glasses.clear();
    // (0.34.4) The screens' redraws are of their own strips (the position 30 times a second), but
    // every screen was drawn again each time, its shadow's blur and all, for JUCE to clip away.
    // A screen well outside what's being redrawn isn't drawn; its glass is still noted (the
    // reflections over the glasses are drawn after the keys, over whatever is redrawn).
    const auto redrawn = g.getClipBounds().toFloat();
    const auto outside = [&] (juce::Rectangle<float> screen)
    {
        if (redrawn.intersects (screen.expanded (LedDisplay::screenReach (screenSize))))
            return false;
        glasses.push_back (LedDisplay::glassOf (screen, screenSize));
        return true;
    };

    // The badge: the PR7-S mark and what it is, printed in red.
    {
        auto b = badge;
        Logo::drawMark (g, b.removeFromTop (b.getHeight() * 0.62f).reduced (0.0f, 6.0f), juce::Justification::centredLeft,
                        { Finish::ink(), juce::Colour (0xffe0103c), Finish::stampEdge(), {}, 1.0f, false });
        g.setColour (Finish::printedRed());
        g.setFont (font (10.0f));
        g.drawText ("RECORDING STUDIO", b, juce::Justification::topLeft, false);
    }

    // The keys' shortcuts.
    {
        auto keys = keysArea;
        // Under each key, its shortcut on the computer's keyboard (AUTO-SCROLL's: F).
        for (const auto* name : { "HOME", "SPACE", "SPACE", "R", "/", "C" })
            stamped (g, name, keys.removeFromLeft (keyPitchNow).removeFromBottom (14.0f), 9.0f);
        stamped (g, "F", autoScrollStamp, 9.0f);
    }

    // POSITION (bars, beats, sixteenths and ticks) and TIME (minutes and
    // seconds): two screens alike (0.9.10, the owner asked), the main values,
    // large, in Bebas; each character in a cell of its own width so the
    // figures stay still as they change.
    {
        const auto beats = studio.getEngine().getPosition();
        const auto drawScreen = [&] (juce::Rectangle<float> screen, const char* label, const juce::String& value)
        {
            if (outside (screen))
                return;
            const auto glass = LedDisplay::paintScreen (g, screen, screenSize);
            glasses.push_back (glass);
            auto inside = glass.reduced (12.0f, 6.0f);
            g.setColour (orange.withAlpha (0.6f));
            g.setFont (font (9.0f));
            g.drawText (label, inside.removeFromTop (12.0f), juce::Justification::topLeft, false);
            g.setColour (orange);
            drawFixedPitch (g, value, inside, juce::jmin (44.0f, inside.getHeight() * 0.95f));
        };
        drawScreen (positionScreen, "POSITION", screenPosition (getPositionText(), [this] { auto most = studio.getProject().numerator; for (const auto& e : studio.getProject().signatureChanges) most = juce::jmax (most, e.numerator); return most; }()));
        drawScreen (timeScreen, "TIME", pr7s::formatTime (studio.getProject().tempoMap().secondsAt (beats)));
    }

    // CPU: the audio processing load, as much of each block's time as it
    // takes (the average, big; the peak, small); OVER lit after an overload.
    if (! outside (cpuScreen))
    {
        const auto glass = LedDisplay::paintScreen (g, cpuScreen, screenSize);
        glasses.push_back (glass);
        auto inside = glass.reduced (9.0f, 6.0f);
        auto top = inside.removeFromTop (12.0f);
        g.setFont (font (9.0f));
        g.setColour (orange.withAlpha (0.6f));
        g.drawText ("CPU", top, juce::Justification::topLeft, false);
        g.setColour (overloadLit ? ParagonColours::meterRed : orange.withAlpha (0.18f));
        g.drawText ("OVER", top, juce::Justification::topRight, false);
        const auto percent = [] (float load) { return juce::String (juce::roundToInt (juce::jlimit (0.0f, 9.99f, load) * 100.0f)); };
        auto bottom = inside.removeFromBottom (12.0f);
        g.setColour (orange.withAlpha (0.75f));
        g.drawText ("PEAK " + percent (shownPeak) + "%", bottom, juce::Justification::bottomRight, false);
        g.setColour (shownAverage >= 1.0f ? ParagonColours::meterRed : orange);
        g.setFont (font (juce::jmin (26.0f, inside.getHeight() * 0.95f)));
        g.drawFittedText (percent (shownAverage) + "%", inside.toNearestInt(), juce::Justification::centredRight, 1, 0.7f);
    }

    // TEMPO and SIGNATURE.
    if (! outside (tempoScreen))
    {
        const auto glass = LedDisplay::paintScreen (g, tempoScreen, screenSize);
        glasses.push_back (glass);
        auto inside = glass.reduced (10.0f, 6.0f);
        g.setColour (orange.withAlpha (0.6f));
        g.setFont (font (9.0f));
        g.drawText (isShowingTap() ? "TAP TEMPO" : studio.getProject().followsTempoTrack() ? "TEMPO TRACK" : "TEMPO", inside.removeFromTop (12.0f), juce::Justification::topLeft, false);
        g.setColour (orange);
        g.setFont (font (juce::jmin (28.0f, inside.getHeight() * 0.62f)));
        g.drawFittedText (getTempoText(), inside.withTrimmedRight (30.0f).toNearestInt(), juce::Justification::centredRight, 1, 0.7f);
        g.setFont (font (10.0f));
        g.drawText ("BPM", inside.removeFromRight (28.0f), juce::Justification::centredRight, false);
    }
    if (! outside (signatureScreen))
    {
        const auto glass = LedDisplay::paintScreen (g, signatureScreen, screenSize);
        glasses.push_back (glass);
        auto inside = glass.reduced (10.0f, 6.0f);
        g.setColour (orange.withAlpha (0.6f));
        g.setFont (font (9.0f));
        // The signature where the playhead is (0.45.0: it can change along the song).
        const auto now = studio.getSignatureNow();
        g.drawText (now.bar > 0 ? "SIGN. BAR " + juce::String (now.bar + 1) : juce::String ("SIGNATURE"), inside.removeFromTop (12.0f), juce::Justification::topLeft, false);
        g.setColour (orange);
        g.setFont (font (juce::jmin (28.0f, inside.getHeight() * 0.62f)));
        g.drawFittedText (juce::String (now.numerator) + "/" + juce::String (now.denominator), inside.toNearestInt(),
                          juce::Justification::centred, 1, 0.7f);
    }
    // OUT (0.73.0, the owner's: the output's level as a screen beside CPU; 0.73.1: the number only;
    // 0.73.2: the loudest peak since OUT was last clicked, in dB; red over 0 dB).
    if (! outside (outScreen))
    {
        const auto glass = LedDisplay::paintScreen (g, outScreen, screenSize);
        glasses.push_back (glass);
        auto inside = glass.reduced (9.0f, 6.0f);
        auto top = inside.removeFromTop (12.0f);
        g.setFont (font (9.0f));
        g.setColour (orange.withAlpha (0.6f));
        g.drawText ("OUT", top, juce::Justification::topLeft, false);
        auto bottom = inside.removeFromBottom (12.0f);
        g.setColour (orange.withAlpha (0.75f));
        g.drawText ("DB MAX", bottom, juce::Justification::bottomRight, false);
        const auto silent = shownOutDb <= LevelMeter::minDb + 0.1f;
        g.setColour (shownOutDb > 0.0f ? ParagonColours::meterRed : orange);
        g.setFont (font (juce::jmin (26.0f, inside.getHeight() * 0.95f)));
        g.drawFittedText (silent ? juce::String ("-INF") : (shownOutDb > 0.0f ? "+" : "") + juce::String (shownOutDb, 1),
                          inside.toNearestInt(), juce::Justification::centredRight, 1, 0.7f);
    }
}

void TransportBar::paintOverChildren (juce::Graphics& g)
{
    for (const auto& glass : glasses)
        LedDisplay::paintGlassOverlay (g, glass, screenSize);
}

void TransportBar::showRecordModes()
{
    using Modes = pr7s::Project::RecordModes;
    const auto modes = studio.getProject().recordModes;
    juce::PopupMenu menu;
    menu.addSectionHeader ("AUDIO RECORD MODE");
    menu.addItem (1, "Keep History (what's there stays; each take on a lane)", true, modes.audio == Modes::Audio::keepHistory);
    menu.addItem (2, "Cycle History + Replace (replaces what's there; a cycle's laps kept)", true, modes.audio == Modes::Audio::cycleHistoryReplace);
    menu.addItem (3, "Replace (only the last take, in place of what's there)", true, modes.audio == Modes::Audio::replace);
    menu.addSectionHeader ("MIDI RECORD MODE");
    menu.addItem (11, "New Parts (beside what's there)", true, modes.midi == Modes::Midi::newParts);
    menu.addItem (12, "Merge (into the part that's there)", true, modes.midi == Modes::Midi::merge);
    menu.addItem (13, "Replace (what's there goes)", true, modes.midi == Modes::Midi::replace);
    menu.addSectionHeader ("MIDI CYCLE RECORD MODE");
    menu.addItem (21, "Mix (each lap added to the last)", true, modes.midiCycle == Modes::MidiCycle::mix);
    menu.addItem (22, "Overwrite (playing in a lap replaces what was there from then on)", true, modes.midiCycle == Modes::MidiCycle::overwrite);
    menu.addItem (23, "Keep Last (the last whole lap played in)", true, modes.midiCycle == Modes::MidiCycle::keepLast);
    menu.addItem (24, "Stacked (each lap a take on a lane, all but the last muted)", true, modes.midiCycle == Modes::MidiCycle::stacked);
    menu.addItem (25, "Mix-Stacked (each lap a take on a lane, none muted)", true, modes.midiCycle == Modes::MidiCycle::mixStacked);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&recordKey), [this] (int chosen)
    {
        auto m = studio.getProject().recordModes;
        if (chosen >= 1 && chosen <= 3)
            m.audio = (Modes::Audio) (chosen - 1);
        else if (chosen >= 11 && chosen <= 13)
            m.midi = (Modes::Midi) (chosen - 11);
        else if (chosen >= 21 && chosen <= 25)
            m.midiCycle = (Modes::MidiCycle) (chosen - 21);
        else
            return;
        studio.setRecordModes (m);
    });
}

void TransportBar::mouseDown (const juce::MouseEvent& event)
{
    if (outScreen.contains (event.position))
    {
        resetOutPeak();   // (0.73.2) the loudest from now on
        return;
    }
    if (cpuScreen.contains (event.position))
    {
        studio.getEngine().resetOverloads();   // (as the manual has it: a click resets the overload light)
        followPerformance();
        return;
    }
    if (tempoScreen.contains (event.position) && event.mods.isPopupMenu())
    {
        // The manual's Activate Tempo Track (on its transport), and the tempo track itself.
        const auto& project = studio.getProject();
        juce::PopupMenu menu;
        menu.addItem (1, "Activate Tempo Track", true, project.followsTempoTrack());
        menu.addItem (2, project.tempoTrackShown ? "The tempo track is in the track list" : "Add the Tempo Track", ! project.tempoTrackShown);
        menu.addItem (3, "Open Tempo Track Editor (Ctrl+T)");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (tempoScreen.toNearestInt())),
                            [this] (int chosen)
                            {
                                if (chosen == 1)
                                    studio.setTempoTrackOn (! studio.getProject().followsTempoTrack());
                                else if (chosen == 2)
                                    studio.addTempoTrack();
                                else if (chosen == 3 && onOpenTempoEditor)
                                    onOpenTempoEditor();
                            });
        return;
    }
    // (With the tempo track on, the tempo isn't changed here: the manual's. Its row's tempo or its curve.)
    if (tempoScreen.contains (event.position) && ! studio.getProject().followsTempoTrack())
    {
        draggingTempo = true;
        dragStartBpm = studio.getProject().bpm;
        return;
    }
    if (signatureScreen.contains (event.position))
    {
        juce::PopupMenu menu;
        const auto& project = studio.getProject();
        menu.addSectionHeader (project.signatureChanges.empty() ? "THE SONG'S SIGNATURE" : "THE FIRST SIGNATURE (BAR 1)");
        for (size_t i = 0; i < std::size (signatures); ++i)
        {
            const auto [numerator, denominator] = signatures[i];
            menu.addItem ((int) i + 1, juce::String (numerator) + "/" + juce::String (denominator), true,
                          project.numerator == numerator && project.denominator == denominator);
        }
        // (0.45.0) Changes along the song.
        menu.addSeparator();
        menu.addItem (100, "Insert Signature at the Cursor's Bar...", onInsertSignature != nullptr && ! project.signatureLocked);
        menu.addItem (101, project.signatureTrackShown ? "The signature track is in the track list" : "Add the Signature Track", ! project.signatureTrackShown);
        menu.addItem (102, "Open Tempo Track Editor (Ctrl+T)", onOpenTempoEditor != nullptr);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (
                                localAreaToGlobal (signatureScreen.toNearestInt())),
                            [this] (int chosen)
                            {
                                if (chosen == 100 && onInsertSignature)
                                    onInsertSignature();
                                else if (chosen == 101)
                                    studio.addSignatureTrack();
                                else if (chosen == 102 && onOpenTempoEditor)
                                    onOpenTempoEditor();
                                else if (chosen > 0 && chosen <= (int) std::size (signatures))
                                    studio.setSignature (signatures[(size_t) chosen - 1].first, signatures[(size_t) chosen - 1].second);
                            });
    }
}

void TransportBar::mouseDrag (const juce::MouseEvent& event)
{
    if (! draggingTempo)
        return;
    // Up for faster: 1 BPM every 3 pixels (a tenth of that with shift held).
    const auto step = event.mods.isShiftDown() ? 0.1 : 1.0;
    studio.setTempo (std::round ((dragStartBpm - event.getDistanceFromDragStartY() / 3.0 * step) / step) * step);
}

void TransportBar::mouseUp (const juce::MouseEvent&)
{
    draggingTempo = false;
}

void TransportBar::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (! tempoScreen.contains (event.position) || studio.getProject().followsTempoTrack())
        return;
    auto* window = new FamilyAlertWindow ("Tempo", "The song's tempo, in beats per minute (20 to 300):", juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("bpm", juce::String (studio.getProject().bpm, 2));
    window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    window->enterModalState (true, juce::ModalCallbackFunction::create ([this, window] (int result)
    {
        if (result == 1)
            if (const auto bpm = window->getTextEditorContents ("bpm").getDoubleValue(); bpm > 0.0)
                studio.setTempo (bpm);
    }), true);
}
