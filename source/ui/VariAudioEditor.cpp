#include "VariAudioEditor.h"

#include "engine/Warp.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    const juce::Colour pageColour { 0xff0d0a0e };
    const juce::Colour viewColour { 0xff141016 };
    const juce::Colour darkRow { 0xff100c11 };
    const juce::Colour pearl { 0xfff4eeea };
    using pr7s::FamilyColours::red;
    using pr7s::FamilyColours::pink;
    using pr7s::FamilyColours::orange;
    using pr7s::PitchSnap;
    using pr7s::VariSegment;

    const std::vector<VariSegment> noSegments;

    bool isBlackKey (int pitch) { const auto pc = (pitch % 12 + 12) % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

    juce::String centsText (float pitch)
    {
        const auto cents = juce::roundToInt ((pitch - std::round (pitch)) * 100.0f);
        return pr7s::noteName ((int) std::lround (pitch)) + (cents == 0 ? juce::String() : (cents > 0 ? " +" : " ") + juce::String (cents) + " CENTS");
    }
}

//==============================================================================
juce::TextButton& VariAudioEditor::makeKey (const juce::String& text, std::function<void()> onClick, const juce::String& tip)
{
    auto key = std::make_unique<juce::TextButton> (text);
    key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    key->setWantsKeyboardFocus (false);
    key->onClick = [this, action = std::move (onClick)] { action(); refresh(); };
    key->setTooltip (tip);
    panel.addAndMakeVisible (*key);
    keys.push_back (std::move (key));
    return *keys.back();
}

VariAudioEditor::VariAudioEditor (pr7s::Studio& s, int part) : studio (s), partId (part)
{
    setWantsKeyboardFocus (true);
    editorScale = pr7s::Key::fromPitchClass (0, false);
    if (auto* settings = studio.getSettings())
    {
        snap = (PitchSnap) juce::jlimit (0, 2, settings->getIntValue ("variAudioSnap", 0));
        const auto scale = juce::StringArray::fromTokens (settings->getValue ("variAudioScale", "0 0"), " ", "");
        editorScale = pr7s::Key::fromPitchClass (juce::jlimit (0, 11, scale[0].getIntValue()), scale[1] == "1");
        useChordTrack = settings->getBoolValue ("variAudioChordTrack", false);
        snapToScale = settings->getBoolValue ("variAudioSnapToScale", false);
        scaleGuides = settings->getBoolValue ("variAudioGuides", false);
    }

    analyseKey = &makeKey ("EDIT VARIAUDIO", [this] { analyse(); },
                           "Find the notes in the recording (monophonic: a voice, a solo instrument) and split it into segments");
    bypassKey = &makeKey ("BYPASS", [this]
    {
        if (const auto* vari = studio.getVariAudio (file()))
            studio.setVariAudioBypass (file(), ! vari->bypass);
    }, "Hear the recording as it was, to compare (VariAudio Changes Bypass)");
    resetKey = &makeKey ("RESET", [this]
    {
        selection.clear();
        studio.removeVariAudio (file());
    }, "Every segment and change gone (an undo step)");
    const char* snapNames[] = { "ABSOLUTE", "RELATIVE", "OFF" };
    const char* snapTips[] = { "Segments snap onto semitones", "Segments move by semitones, keeping how sharp or flat they were",
                               "Segments move freely" };
    for (int m = 0; m < 3; ++m)
        snapKeys[(size_t) m] = &makeKey (snapNames[m], [this, m] { setSnap ((PitchSnap) m); }, snapTips[m]);
    editorScaleKey = &makeKey ("EDITOR SCALE", [this] { setUseChordTrack (false); }, "Use the scale chosen here");
    chordTrackKey = &makeKey ("CHORD TRACK", [this] { setUseChordTrack (true); }, "Use the chord track's scale at each segment");
    suggestKey = &makeKey ("SUGGEST", [this]
    {
        juce::PopupMenu menu;
        const auto suggestions = getScaleSuggestions();
        menu.addSectionHeader (selection.empty() ? "SCALES FOR ALL THE SEGMENTS" : "SCALES FOR THE SELECTED SEGMENTS");
        for (int i = 0; i < (int) suggestions.size(); ++i)
            menu.addItem (i + 1, suggestions[(size_t) i].name());
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (suggestKey),
                            [safe = juce::Component::SafePointer<VariAudioEditor> (this), suggestions] (int chosen)
        {
            if (safe != nullptr && chosen > 0)
                safe->setEditorScale (suggestions[(size_t) chosen - 1]);
        });
    }, "The scales that hold these segments' notes best (the Scale Assistant)");
    suggestKey->getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    snapScaleKey = &makeKey ("SNAP TO SCALE", [this] { setSnapToScale (! snapToScale); }, "Moved segments land on the scale's notes");
    guidesKey = &makeKey ("SCALE GUIDES", [this] { setScaleGuides (! scaleGuides); }, "Notes outside the scale shown darker");
    quantizeKey = &makeKey ("QUANTIZE PITCHES", [this] { quantizeSelected(); },
                            "The selected segments (else all) onto the nearest note of the scale (the snap mode counts)");
    auditionKey = &makeKey ("AUDITION", [this] { auditionSelected(); }, "Hear the selected segments (as changed)");

    for (int pc = 0; pc < 12; ++pc)
        for (const auto minor : { false, true })
        {
            const auto key = pr7s::Key::fromPitchClass (pc, minor);
            scaleBox.addItem (key.name(), pc * 2 + (minor ? 1 : 0) + 1);
        }
    // (0.35.0) The Functions menu, the smart controls' set, MIDI Input, the reference track, the colours.
    functionsKey = &makeKey ("FUNCTIONS", [this] { showFunctionsMenu(); },
                             "Extract MIDI, Generate Harmony Voices, the resets (the selected segments, else all), Reanalyze Audio");
    functionsKey->getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    defaultControlsKey = &makeKey ("DEFAULT", [this] { setAllSmartControls (false); },
                                   "The smart controls used most: straighten, correct pitch, glue, split, warp start and end");
    allControlsKey = &makeKey ("ALL", [this] { setAllSmartControls (true); },
                               "Every smart control: also tilt (Alt: rotate), the tilt anchor, the straighten range, formant and volume");
    midiInputKey = &makeKey ("MIDI INPUT", [this] { setMidiInput (! midiInput); },
                             "Play a key: the selected segments go to that note (the first to it, the rest by as much)");
    stepKey = &makeKey ("STEP", [this] { setStepMode (! stepMode); }, "After each key played, the next segment is selected");
    referenceBox.setTextWhenNothingSelected ("NO REFERENCE");
    referenceBox.setTooltip ("A MIDI track's notes shown as lines, to tune and time against (saved with the song)");
    referenceBox.onChange = [this] { setReferenceTrack (juce::jmax (0, referenceBox.getSelectedId() - 1)); };
    panel.addAndMakeVisible (referenceBox);
    coloursBox.addItem ("AUTO", 1);
    coloursBox.addItem ("EVENT (THE TRACK'S)", 2);
    coloursBox.addItem ("PITCH", 3);
    coloursBox.addItem ("SCALE / CHORDS", 4);
    coloursBox.setTooltip ("The segments' colours: plain, the track's, by pitch, or whether they fit the scale (or the chord track)");
    coloursBox.onChange = [this] { setColours ((Colours) juce::jmax (0, coloursBox.getSelectedId() - 1)); };
    panel.addAndMakeVisible (coloursBox);
    scaleBox.onChange = [this]
    {
        const auto id = scaleBox.getSelectedId() - 1;
        if (id >= 0)
            setEditorScale (pr7s::Key::fromPitchClass (id / 2, id % 2 == 1));
    };
    panel.addAndMakeVisible (scaleBox);

    // The sliders: the selected segments, heard when let go (one undo step).
    const auto setUp = [this] (juce::Slider& slider, double low, double high, const juce::String& suffix,
                               std::function<void (VariSegment&, const VariSegment& before, double value)> edit)
    {
        slider.setSliderStyle (juce::Slider::LinearBar);
        slider.setRange (low, high, 1.0);
        slider.setTextValueSuffix (suffix);
        slider.setColour (juce::Slider::trackColourId, ParagonColours::accent.withAlpha (0.6f));
        slider.setColour (juce::Slider::textBoxTextColourId, ParagonColours::text);
        slider.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff151217));   // (pearl onyx: JUCE's own is blue-grey)
        panel.addAndMakeVisible (slider);
        slider.onDragStart = [this] { base = segments(); working = base; gesture = Gesture::slider; };
        slider.onValueChange = [this, &slider, edit]
        {
            const auto before = gesture == Gesture::slider ? base : segments();
            auto changed = before;
            for (const auto i : selectionOrAll())
                if (juce::isPositiveAndBelow (i, (int) changed.size()))
                    edit (changed[(size_t) i], before[(size_t) i], slider.getValue());
            if (gesture == Gesture::slider)
            {
                working = std::move (changed);
                repaint();
            }
            else
                commit (std::move (changed));
        };
        slider.onDragEnd = [this] { if (gesture == Gesture::slider) commit (working); };
    };
    setUp (correct, 0.0, 100.0, " %", [] (VariSegment& seg, const VariSegment& before, double v) { seg.shift = pr7s::correctedShift (before, (float) v / 100.0f); });
    setUp (straighten, 0.0, 100.0, " %", [] (VariSegment& seg, const VariSegment&, double v) { seg.straighten = (float) v / 100.0f; });
    setUp (volume, -24.0, 12.0, " dB", [] (VariSegment& seg, const VariSegment&, double v) { seg.volumeDb = (float) v; });
    setUp (formant, -100.0, 100.0, " %", [] (VariSegment& seg, const VariSegment&, double v) { seg.formant = (float) v; });
    formant.setTooltip ("Shift Formant: the voice's colour moved (darker below 0, brighter above), its pitch kept");
    formant.getProperties().set (ParagonLookAndFeel::barFromCentreProperty, true);   // (filled from 0, as the pan)
    correct.setTooltip ("Correct Pitch: the selected segments that far to the nearest semitone");
    straighten.setTooltip ("Straighten Curve: the wobble around each segment's pitch flattened (vibrato, drifts)");
    volume.setTooltip ("The selected segments' level");

    for (auto* label : { &title, &info, &correctLabel, &straightenLabel, &volumeLabel, &formantLabel })
    {
        label->setFont (ParagonLookAndFeel::textFont (14.0f));
        label->setColour (juce::Label::textColourId, ParagonColours::textDim);
        label->setInterceptsMouseClicks (false, false);
        panel.addAndMakeVisible (*label);
    }
    formantLabel.setText ("FORMANT", juce::dontSendNotification);
    if (auto* settings = studio.getSettings())
    {
        allSmartControls = settings->getBoolValue ("variAudioAllSmartControls", false);
        colours = (Colours) juce::jlimit (0, 3, settings->getIntValue ("variAudioColours", 0));
    }
    // The panel scrolls when the window is short.
    panelView.setViewedComponent (&panel, false);
    panelView.setScrollBarsShown (true, false);
    panelView.setScrollBarThickness (8);
    addAndMakeVisible (panelView);
    title.setFont (ParagonLookAndFeel::titleFont (20.0f));
    title.setColour (juce::Label::textColourId, ParagonColours::text);
    info.setColour (juce::Label::textColourId, ParagonColours::text);
    info.setJustificationType (juce::Justification::topLeft);
    correctLabel.setText ("CORRECT PITCH", juce::dontSendNotification);
    straightenLabel.setText ("STRAIGHTEN CURVE", juce::dontSendNotification);
    volumeLabel.setText ("VOLUME", juce::dontSendNotification);

    studio.addChangeListener (this);
    setSize (1400, 720);
    fitView();
    refresh();
}

VariAudioEditor::~VariAudioEditor()
{
    if (midiInput)
        studio.setCatchingNotes (false);
    studio.removeChangeListener (this);
}

void VariAudioEditor::saveSettings() const
{
    if (auto* settings = studio.getSettings())
    {
        settings->setValue ("variAudioSnap", (int) snap);
        settings->setValue ("variAudioScale", juce::String (editorScale.pitchClass()) + (editorScale.minor ? " 1" : " 0"));
        settings->setValue ("variAudioChordTrack", useChordTrack);
        settings->setValue ("variAudioSnapToScale", snapToScale);
        settings->setValue ("variAudioGuides", scaleGuides);
        settings->setValue ("variAudioAllSmartControls", allSmartControls);
        settings->setValue ("variAudioColours", (int) colours);
    }
}

juce::TextButton* VariAudioEditor::keyNamed (const juce::String& text)
{
    for (auto& key : keys)
        if (key->getButtonText() == text)
            return key.get();
    return nullptr;
}

void VariAudioEditor::setPart (int id)
{
    if (id == partId)
        return;
    partId = id;
    selection.clear();
    fitView();
    refresh();
}

const pr7s::Part* VariAudioEditor::part() const
{
    if (const auto found = studio.findPart (partId))
        return &studio.getProject().findTrack (found->first)->parts[(size_t) found->second];
    return nullptr;
}

juce::String VariAudioEditor::file() const
{
    const auto* p = part();
    return p != nullptr ? p->audioFile : juce::String();
}

const std::vector<VariSegment>& VariAudioEditor::segments() const
{
    if (gesture != Gesture::none && gesture != Gesture::marquee && ! working.empty())
        return working;
    if (const auto* vari = studio.getVariAudio (file()))
        return vari->segments;
    return noSegments;
}

void VariAudioEditor::commit (std::vector<VariSegment> changed)
{
    gesture = Gesture::none;
    working.clear();
    base.clear();
    studio.setVariSegments (file(), std::move (changed));
    refresh();
}

std::vector<int> VariAudioEditor::selectionOrAll() const
{
    if (! selection.empty())
        return selection;
    std::vector<int> all ((size_t) segments().size());
    std::iota (all.begin(), all.end(), 0);
    return all;
}

void VariAudioEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (part() == nullptr)
    {
        selection.clear();
        repaint();
        return;
    }
    const auto count = (int) segments().size();
    selection.erase (std::remove_if (selection.begin(), selection.end(), [count] (int i) { return i >= count; }), selection.end());
    refresh();
}

//==============================================================================
bool VariAudioEditor::analyse()
{
    if (file().isEmpty())
        return false;
    const auto ok = studio.editVariAudio (file());
    fitView();
    refresh();
    return ok;
}

void VariAudioEditor::setSnap (PitchSnap mode)
{
    snap = mode;
    saveSettings();
    refresh();
}

void VariAudioEditor::selectSegments (std::vector<int> indices)
{
    std::sort (indices.begin(), indices.end());
    indices.erase (std::unique (indices.begin(), indices.end()), indices.end());
    selection = std::move (indices);
    refresh();
}

void VariAudioEditor::moveSelected (float semitones, PitchSnap mode)
{
    auto changed = segments();
    for (const auto i : selection)
        if (juce::isPositiveAndBelow (i, (int) changed.size()))
        {
            auto& s = changed[(size_t) i];
            s.shift = pr7s::movedShift (s, semitones, mode);
            if (snapToScale)
                s.shift = pr7s::quantizedShift (s, scaleAt (beatForSeconds (s.start)), mode);
        }
    commit (std::move (changed));
}

void VariAudioEditor::correctSelected (float amount)
{
    auto changed = segments();
    for (const auto i : selectionOrAll())
        changed[(size_t) i].shift = pr7s::correctedShift (changed[(size_t) i], amount);
    commit (std::move (changed));
}

void VariAudioEditor::straightenSelected (float amount)
{
    auto changed = segments();
    for (const auto i : selectionOrAll())
        changed[(size_t) i].straighten = juce::jlimit (0.0f, 1.0f, amount);
    commit (std::move (changed));
}

void VariAudioEditor::setVolumeOfSelected (float db)
{
    auto changed = segments();
    for (const auto i : selectionOrAll())
        changed[(size_t) i].volumeDb = db;
    commit (std::move (changed));
}

void VariAudioEditor::setEditorScale (const pr7s::Key& key)
{
    editorScale = key;
    useChordTrack = false;
    saveSettings();
    refresh();
}

void VariAudioEditor::setUseChordTrack (bool on)
{
    useChordTrack = on;
    saveSettings();
    refresh();
}

void VariAudioEditor::setSnapToScale (bool on)
{
    snapToScale = on;
    saveSettings();
    refresh();
}

void VariAudioEditor::setScaleGuides (bool on)
{
    scaleGuides = on;
    saveSettings();
    refresh();
}

pr7s::Key VariAudioEditor::scaleAt (double songBeat) const
{
    return useChordTrack ? studio.chordTrackKeyAt (songBeat) : editorScale;
}

void VariAudioEditor::quantizeSelected()
{
    auto changed = segments();
    for (const auto i : selectionOrAll())
    {
        auto& s = changed[(size_t) i];
        s.shift = pr7s::quantizedShift (s, scaleAt (beatForSeconds (s.start)), snap);
    }
    commit (std::move (changed));
}

std::vector<pr7s::Key> VariAudioEditor::getScaleSuggestions() const
{
    std::vector<VariSegment> chosen;
    for (const auto i : selectionOrAll())
        chosen.push_back (segments()[(size_t) i]);
    return pr7s::suggestScales (chosen);
}

void VariAudioEditor::auditionSelected()
{
    const auto indices = selectionOrAll();
    if (indices.empty())
        return;
    const auto& segs = segments();
    auto from = segs[(size_t) indices.front()].start, to = segs[(size_t) indices.front()].end;
    for (const auto i : indices)
        from = juce::jmin (from, segs[(size_t) i].start), to = juce::jmax (to, segs[(size_t) i].end);
    studio.auditionRecording (file(), from, to);
}

juce::String VariAudioEditor::getInfoText() const
{
    const auto& segs = segments();
    if (segs.empty())
        return studio.getVariAudio (file()) != nullptr ? "NO NOTES FOUND" : "CLICK EDIT VARIAUDIO TO FIND THE NOTES";
    if (selection.empty())
        return juce::String ((int) segs.size()) + " SEGMENTS";
    if (selection.size() > 1)
        return juce::String ((int) selection.size()) + " SEGMENTS SELECTED";
    const auto& s = segs[(size_t) selection.front()];
    juce::String text = centsText (s.pitchNow());
    if (std::abs (s.shift) > 1.0e-4f)
        text << "\nWAS " << centsText (s.pitch);
    if (std::abs (s.tiltStart) > 1.0e-3f || std::abs (s.tiltEnd) > 1.0e-3f)
        text << "\nTILTED " << juce::String (s.tiltStart, 2) << " / " << juce::String (s.tiltEnd, 2);
    if (std::abs (s.formant) > 1.0e-3f)
        text << "\nFORMANT " << (s.formant > 0.0f ? "+" : "") << juce::String (juce::roundToInt (s.formant)) << " %";
    return text;
}

//==============================================================================
// Part two (0.35.0)
void VariAudioEditor::setAllSmartControls (bool on)
{
    allSmartControls = on;
    saveSettings();
    refresh();
}

bool VariAudioEditor::splitAt (int index, double seconds)
{
    if (! studio.splitVariSegment (file(), index, seconds))
        return false;
    selectSegments ({ index, index + 1 });
    return true;
}

bool VariAudioEditor::glue (int index, bool withNext)
{
    // A selection of neighbours holding it: all of them as one; else it and the one beside it.
    auto first = withNext ? index : index - 1, last = withNext ? index + 1 : index;
    if (selection.size() > 1 && std::find (selection.begin(), selection.end(), index) != selection.end()
        && selection.back() - selection.front() == (int) selection.size() - 1)
        first = selection.front(), last = selection.back();
    if (! studio.glueVariSegments (file(), first, last))
        return false;
    selectSegments ({ first });
    return true;
}

bool VariAudioEditor::deleteSelected()
{
    if (selection.empty())
        return false;
    const auto gone = selection;
    selection.clear();
    return studio.deleteVariSegments (file(), gone);
}

void VariAudioEditor::setFormantOfSelected (float percent)
{
    auto changed = segments();
    for (const auto i : selectionOrAll())
        changed[(size_t) i].formant = juce::jlimit (-100.0f, 100.0f, percent);
    commit (std::move (changed));
}

void VariAudioEditor::resetSelected (pr7s::Studio::VariReset what)
{
    studio.resetVariAudio (partId, selection, what);
    refresh();
}

bool VariAudioEditor::reanalyse()
{
    selection.clear();
    const auto ok = studio.reanalyseVariAudio (file());
    fitView();
    refresh();
    return ok;
}

int VariAudioEditor::extractMidi (const pr7s::ExtractMidiOptions& options, pr7s::Studio::ExtractDestination destination, juce::String& error)
{
    return studio.extractMidi (partId, options, destination, error);
}

void VariAudioEditor::setMidiInput (bool on)
{
    midiInput = on;
    studio.setCatchingNotes (on);
    if (on)
        startTimerHz (30);
    else
        stopTimer();
    refresh();
}

void VariAudioEditor::setStepMode (bool on)
{
    stepMode = on;
    refresh();
}

void VariAudioEditor::timerCallback()
{
    if (! midiInput)
        return;
    for (const auto note : studio.takePlayedNotes())
        notePlayed (note);
}

void VariAudioEditor::notePlayed (int note)
{
    // The first selected segment to the note (the manual's E0 to C5), the others moved as far.
    const auto& segs = segments();
    if (selection.empty() || ! juce::isPositiveAndBelow (selection.front(), (int) segs.size()))
        return;
    note = juce::jlimit (28, 84, note);
    const auto delta = (float) note - segs[(size_t) selection.front()].pitchNow();
    auto changed = segs;
    for (const auto i : selection)
        changed[(size_t) i].shift += delta;
    const auto last = selection.back();
    commit (std::move (changed));
    if (stepMode && last + 1 < (int) segments().size())
        selectSegments ({ last + 1 });
}

void VariAudioEditor::setReferenceTrack (int trackId)
{
    studio.setVariAudioReference (trackId);
    refresh();
}

void VariAudioEditor::setColours (Colours scheme)
{
    colours = scheme;
    saveSettings();
    refresh();
}

void VariAudioEditor::showFunctionsMenu()
{
    using Reset = pr7s::Studio::VariReset;
    const auto forSelection = selection.empty() ? juce::String() : juce::String (" for Selection");
    juce::PopupMenu menu;
    menu.addItem (1, "Extract MIDI...");
    menu.addItem (2, "Generate Harmony Voices...");
    menu.addSeparator();
    menu.addItem (10, "Reset Pitch Changes" + forSelection);
    menu.addItem (11, "Reset Pitch Curve Changes" + forSelection);
    menu.addItem (12, "Reset Formant Shift Changes" + forSelection);
    menu.addItem (13, "Reset Volume Changes" + forSelection);
    menu.addItem (14, "Reset Warp Changes");
    menu.addItem (15, "Reset All Changes" + forSelection);
    menu.addSeparator();
    menu.addItem (20, "Reanalyze Audio (new segments: every edit goes)");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (functionsKey),
                        [safe = juce::Component::SafePointer<VariAudioEditor> (this)] (int chosen)
    {
        if (safe == nullptr)
            return;
        switch (chosen)
        {
            case 1: safe->showExtractMidi(); break;
            case 2:
                HarmonyVoicesPage::show (safe->studio, safe->partId, *safe->functionsKey, [safe] (int firstPart, juce::String error)
                {
                    if (error.isNotEmpty())
                        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Generate Harmony Voices", error);
                    else if (safe != nullptr && firstPart > 0 && safe->onOpenPart)
                        safe->onOpenPart (firstPart);
                });
                break;
            case 10: safe->resetSelected (Reset::pitch); break;
            case 11: safe->resetSelected (Reset::curve); break;
            case 12: safe->resetSelected (Reset::formant); break;
            case 13: safe->resetSelected (Reset::volume); break;
            case 14: safe->resetSelected (Reset::warp); break;
            case 15: safe->resetSelected (Reset::all); break;
            case 20: safe->reanalyse(); break;
            default: break;
        }
    });
}

void VariAudioEditor::showExtractMidi()
{
    auto page = std::make_unique<ExtractMidiPage>();
    auto* raw = page.get();
    auto& box = juce::CallOutBox::launchAsynchronously (std::move (page), functionsKey->getScreenBounds(), nullptr);
    juce::Component::SafePointer<juce::CallOutBox> boxSafe (&box);
    raw->onCancel = [boxSafe] { if (boxSafe != nullptr) boxSafe->dismiss(); };
    raw->onOk = [this, raw, boxSafe, safe = juce::Component::SafePointer<VariAudioEditor> (this)]
    {
        if (safe == nullptr)
            return;
        juce::String error;
        const auto made = extractMidi (raw->getOptions(), raw->getDestination(), error);
        if (boxSafe != nullptr)
            boxSafe->dismiss();
        if (made == 0 && error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Extract MIDI", error);
    };
}

//==============================================================================
juce::Rectangle<int> VariAudioEditor::getKeyboardArea() const
{
    return getLocalBounds().withTrimmedLeft (panelWidth).withWidth (keyboardWidth).withTrimmedTop (rulerHeight);
}

juce::Rectangle<int> VariAudioEditor::getDisplayArea() const
{
    return getLocalBounds().withTrimmedLeft (panelWidth + keyboardWidth).withTrimmedTop (rulerHeight);
}

double VariAudioEditor::beatForSeconds (double seconds) const
{
    const auto* p = part();
    if (p == nullptr)
        return 0.0;
    return p->start + pr7s::warpBeatAt (*p, seconds, studio.getProject().tempoMap());
}

float VariAudioEditor::xForSeconds (double seconds) const
{
    return (float) (getDisplayArea().getX() + (beatForSeconds (seconds) - firstBeat) * pixelsPerBeat);
}

float VariAudioEditor::yForPitch (float pitch) const
{
    return (float) getDisplayArea().getY() + (topPitch - pitch) * rowHeight;
}

float VariAudioEditor::pitchForY (float y) const
{
    return topPitch - (y - (float) getDisplayArea().getY()) / rowHeight;
}

juce::Rectangle<float> VariAudioEditor::getSegmentBounds (int index) const
{
    const auto& segs = segments();
    if (! juce::isPositiveAndBelow (index, (int) segs.size()))
        return {};
    const auto& s = segs[(size_t) index];
    const auto x0 = xForSeconds (s.start), x1 = xForSeconds (s.end);
    const auto top = yForPitch (s.pitchNow() + 0.5f);
    return { x0, top, juce::jmax (2.0f, x1 - x0), rowHeight };
}

juce::Rectangle<float> VariAudioEditor::getStraightenHandle (int index) const
{
    return getSmartControl (index, SmartControl::straighten);
}

juce::Rectangle<float> VariAudioEditor::getCorrectHandle (int index) const
{
    return getSmartControl (index, SmartControl::correct);
}

juce::Rectangle<float> VariAudioEditor::getSmartControl (int index, SmartControl which) const
{
    const auto r = getSegmentBounds (index);
    if (r.isEmpty())
        return {};
    const auto& seg = segments()[(size_t) index];
    const auto w = r.getWidth(), h = r.getHeight();
    const auto room = w >= 30.0f, cornerRoom = w >= 44.0f, allRoom = allSmartControls && w >= 70.0f;
    const auto handle = [] (float x, float y, float width = 12.0f) { return juce::Rectangle<float> (width, 7.0f).withCentre ({ x, y }); };
    switch (which)
    {
        // Above the segment: straighten in the middle; with all of them, tilt at the corners, formant
        // and volume a quarter in.
        case SmartControl::straighten: return room ? handle (r.getCentreX(), r.getY() - 5.0f) : juce::Rectangle<float>();
        case SmartControl::tiltLeft:   return allRoom ? handle (r.getX() + 6.0f, r.getY() - 5.0f, 10.0f) : juce::Rectangle<float>();
        case SmartControl::tiltRight:  return allRoom ? handle (r.getRight() - 6.0f, r.getY() - 5.0f, 10.0f) : juce::Rectangle<float>();
        case SmartControl::formant:    return allRoom ? handle (r.getX() + w * 0.25f, r.getY() - 5.0f, 10.0f) : juce::Rectangle<float>();
        case SmartControl::volume:     return allRoom ? handle (r.getX() + w * 0.75f, r.getY() - 5.0f, 10.0f) : juce::Rectangle<float>();
        // Below it: correct pitch in the middle, glue at the corners.
        case SmartControl::correct:    return room ? handle (r.getCentreX(), r.getBottom() + 5.0f) : juce::Rectangle<float>();
        case SmartControl::glueLeft:   return cornerRoom ? handle (r.getX() + 6.0f, r.getBottom() + 5.0f, 10.0f) : juce::Rectangle<float>();
        case SmartControl::glueRight:  return cornerRoom ? handle (r.getRight() - 6.0f, r.getBottom() + 5.0f, 10.0f) : juce::Rectangle<float>();
        // On it: split along the bottom edge, warp at the lower half of the sides; with all of them,
        // the anchor and the straighten range on the top edge.
        case SmartControl::split:      return room ? juce::Rectangle<float> (r.getX() + 12.0f, r.getBottom() - 4.0f, w - 24.0f, 4.0f) : juce::Rectangle<float>();
        case SmartControl::warpStart:  return room ? juce::Rectangle<float> (r.getX(), r.getY() + h * 0.5f, 5.0f, h * 0.5f - 4.0f) : juce::Rectangle<float>();
        case SmartControl::warpEnd:    return room ? juce::Rectangle<float> (r.getRight() - 5.0f, r.getY() + h * 0.5f, 5.0f, h * 0.5f - 4.0f) : juce::Rectangle<float>();
        case SmartControl::anchor:     return allRoom ? juce::Rectangle<float> (8.0f, 5.0f).withCentre ({ r.getX() + w * seg.anchor, r.getY() + 2.5f }) : juce::Rectangle<float>();
        case SmartControl::rangeFrom:  return allRoom ? juce::Rectangle<float> (r.getX() + w * seg.straightenFrom, r.getY(), 4.0f, h * 0.5f) : juce::Rectangle<float>();
        case SmartControl::rangeTo:    return allRoom ? juce::Rectangle<float> (r.getX() + w * seg.straightenTo - 4.0f, r.getY(), 4.0f, h * 0.5f) : juce::Rectangle<float>();
        case SmartControl::none:       break;
    }
    return {};
}

VariAudioEditor::SmartControl VariAudioEditor::smartControlAt (int index, juce::Point<float> at) const
{
    if (index < 0)
        return SmartControl::none;
    // The small ones first (they sit over the bigger ones).
    for (const auto which : { SmartControl::straighten, SmartControl::correct, SmartControl::glueLeft, SmartControl::glueRight,
                              SmartControl::tiltLeft, SmartControl::tiltRight, SmartControl::formant, SmartControl::volume,
                              SmartControl::anchor, SmartControl::rangeFrom, SmartControl::rangeTo,
                              SmartControl::warpStart, SmartControl::warpEnd, SmartControl::split })
    {
        const auto box = getSmartControl (index, which);
        if (! box.isEmpty() && box.expanded (which == SmartControl::split ? 0.0f : 2.0f, 2.0f).contains (at))
            return which;
    }
    return SmartControl::none;
}

float VariAudioEditor::xForBeat (double songBeat) const
{
    return (float) (getDisplayArea().getX() + (songBeat - firstBeat) * pixelsPerBeat);
}

double VariAudioEditor::secondsForX (float x) const
{
    const auto* p = part();
    if (p == nullptr)
        return 0.0;
    const auto beat = firstBeat + (x - getDisplayArea().getX()) / pixelsPerBeat - p->start;
    return pr7s::warpSecondsAt (*p, beat, studio.getProject().tempoMap());
}

juce::Colour VariAudioEditor::colourOf (int index) const
{
    const auto& segs = segments();
    if (! juce::isPositiveAndBelow (index, (int) segs.size()))
        return orange;
    const auto& s = segs[(size_t) index];
    switch (colours)
    {
        case Colours::event:
            if (const auto found = studio.findPart (partId))
                return studio.getProject().findTrack (found->first)->colour;
            return orange;
        case Colours::pitch:
        {
            // A colour of the family's palette for each note (its hues across the twelve).
            const auto pc = ((int) std::lround (s.pitchNow()) % 12 + 12) % 12;
            return pr7s::FamilyColours::paletteColour (pc * 8 / 11, pc % 2 == 0 ? 3 : 5);
        }
        case Colours::scale:
        {
            const auto scale = scaleAt (beatForSeconds (s.start)).scale();
            const auto pc = ((int) std::lround (s.pitchNow()) % 12 + 12) % 12;
            return std::find (scale.begin(), scale.end(), pc) != scale.end() ? orange : pink;
        }
        case Colours::automatic:
            break;
    }
    return orange;
}

int VariAudioEditor::segmentAt (juce::Point<float> at) const
{
    const auto& segs = segments();
    for (int i = (int) segs.size() - 1; i >= 0; --i)
        if (getSegmentBounds (i).expanded (0.0f, 2.0f).contains (at))
            return i;
    return -1;
}

void VariAudioEditor::fitView()
{
    const auto* p = part();
    const auto display = getDisplayArea();
    if (p == nullptr || display.getWidth() <= 0)
        return;
    firstBeat = p->start;
    pixelsPerBeat = juce::jmax (4.0, display.getWidth() / juce::jmax (0.25, p->length));
    const auto& segs = segments();
    if (segs.empty())
    {
        topPitch = 84.0f;
        rowHeight = juce::jlimit (8.0f, 24.0f, display.getHeight() / 36.0f);
        return;
    }
    auto low = 127.0f, high = 0.0f;
    for (const auto& s : segs)
        low = juce::jmin (low, s.pitchNow()), high = juce::jmax (high, s.pitchNow());
    const auto span = juce::jmax (12.0f, high - low + 8.0f);
    rowHeight = juce::jlimit (8.0f, 28.0f, display.getHeight() / span);
    topPitch = (low + high) * 0.5f + display.getHeight() / rowHeight * 0.5f;
}

PitchSnap VariAudioEditor::snapFor (const juce::ModifierKeys& mods) const
{
    if (mods.isShiftDown()) return PitchSnap::off;          // (the manual's: Shift off, Ctrl absolute, Alt relative)
    if (mods.isCommandDown()) return PitchSnap::absolute;
    if (mods.isAltDown()) return PitchSnap::relative;
    return snap;
}

//==============================================================================
void VariAudioEditor::refresh()
{
    const auto* vari = studio.getVariAudio (file());
    const auto* p = part();
    title.setText (p != nullptr ? (p->name.isNotEmpty() ? p->name : file()) : juce::String ("NO CLIP"), juce::dontSendNotification);
    analyseKey->setToggleState (vari != nullptr, juce::dontSendNotification);
    analyseKey->setEnabled (p != nullptr);
    bypassKey->setToggleState (vari != nullptr && vari->bypass, juce::dontSendNotification);
    bypassKey->setEnabled (vari != nullptr);
    resetKey->setEnabled (vari != nullptr);
    for (int m = 0; m < 3; ++m)
        snapKeys[(size_t) m]->setToggleState ((int) snap == m, juce::dontSendNotification);
    editorScaleKey->setToggleState (! useChordTrack, juce::dontSendNotification);
    chordTrackKey->setToggleState (useChordTrack, juce::dontSendNotification);
    snapScaleKey->setToggleState (snapToScale, juce::dontSendNotification);
    guidesKey->setToggleState (scaleGuides, juce::dontSendNotification);
    scaleBox.setSelectedId (editorScale.pitchClass() * 2 + (editorScale.minor ? 1 : 0) + 1, juce::dontSendNotification);
    scaleBox.setEnabled (! useChordTrack);
    const auto any = vari != nullptr && ! vari->segments.empty();
    for (auto* c : std::initializer_list<juce::Component*> { &correct, &straighten, &volume, suggestKey, quantizeKey, auditionKey })
        c->setEnabled (any);
    if (gesture != Gesture::slider)
    {
        correct.setValue (0.0, juce::dontSendNotification);
        const auto& segs = segments();
        const auto first = ! selection.empty() && selection.front() < (int) segs.size() ? &segs[(size_t) selection.front()] : nullptr;
        straighten.setValue (first != nullptr ? first->straighten * 100.0 : 0.0, juce::dontSendNotification);
        volume.setValue (first != nullptr ? first->volumeDb : 0.0, juce::dontSendNotification);
    }
    formant.setEnabled (any);
    functionsKey->setEnabled (p != nullptr);
    defaultControlsKey->setToggleState (! allSmartControls, juce::dontSendNotification);
    allControlsKey->setToggleState (allSmartControls, juce::dontSendNotification);
    midiInputKey->setToggleState (midiInput, juce::dontSendNotification);
    stepKey->setToggleState (stepMode, juce::dontSendNotification);
    midiInputKey->setEnabled (any);
    if (gesture != Gesture::slider)
    {
        const auto& segs = segments();
        const auto first = ! selection.empty() && selection.front() < (int) segs.size() ? &segs[(size_t) selection.front()] : nullptr;
        formant.setValue (first != nullptr ? first->formant : 0.0, juce::dontSendNotification);
    }
    // The reference track: the instrument tracks (rebuilt as they change).
    {
        juce::StringArray names;
        std::vector<int> ids;
        for (const auto& track : studio.getProject().tracks)
            if (track.kind == pr7s::Track::Kind::instrument)
                names.add (track.name), ids.push_back (track.id);
        referenceBox.clear (juce::dontSendNotification);
        referenceBox.addItem ("NO REFERENCE", 1);
        for (size_t i = 0; i < ids.size(); ++i)
            referenceBox.addItem (names[(int) i], ids[i] + 1);
        const auto reference = studio.getProject().variAudioReference;
        referenceBox.setSelectedId (reference > 0 && std::find (ids.begin(), ids.end(), reference) != ids.end() ? reference + 1 : 1, juce::dontSendNotification);
    }
    coloursBox.setSelectedId ((int) colours + 1, juce::dontSendNotification);
    info.setText (getInfoText(), juce::dontSendNotification);
    repaint();
}

void VariAudioEditor::resized()
{
    panelView.setBounds (getLocalBounds().removeFromLeft (panelWidth - 1));
    layoutPanel();
    if (getWidth() > 0 && segments().empty())
        fitView();
}

void VariAudioEditor::layoutPanel()
{
    // One column, as tall as it needs (the view scrolls it when the window is shorter).
    headings.clear();
    const auto width = panelWidth - 1 - panelView.getScrollBarThickness();
    juce::Rectangle<int> panelArea (0, 0, width, 4000);
    auto area = panelArea.reduced (12, 10);
    const auto top = area.getY();
    title.setBounds (area.removeFromTop (28));
    area.removeFromTop (6);
    const auto row = [&area] (int h) { auto line = area.removeFromTop (h); area.removeFromTop (6); return line; };
    const auto heading = [&] (const juce::String& text) { headings.push_back ({ text, row (18) }); };
    analyseKey->setBounds (row (32).reduced (1));
    auto line = row (28);
    bypassKey->setBounds (line.removeFromLeft (line.getWidth() / 2).reduced (1));
    resetKey->setBounds (line.reduced (1));
    functionsKey->setBounds (row (28).reduced (1));
    heading ("PITCH SNAP");
    line = row (28);
    const auto third = line.getWidth() / 3;
    for (auto* key : snapKeys)
        key->setBounds (line.removeFromLeft (third).reduced (1));
    heading ("SELECTED SEGMENTS");
    for (auto [label, slider] : { std::pair { &correctLabel, &correct }, std::pair { &straightenLabel, &straighten },
                                  std::pair { &formantLabel, &formant }, std::pair { &volumeLabel, &volume } })
    {
        label->setBounds (row (16));
        slider->setBounds (row (24));
    }
    heading ("SMART CONTROLS");
    line = row (28);
    defaultControlsKey->setBounds (line.removeFromLeft (line.getWidth() / 2).reduced (1));
    allControlsKey->setBounds (line.reduced (1));
    heading ("MIDI INPUT");
    line = row (28);
    midiInputKey->setBounds (line.removeFromLeft (line.getWidth() * 2 / 3).reduced (1));
    stepKey->setBounds (line.reduced (1));
    heading ("SCALE");
    line = row (28);
    editorScaleKey->setBounds (line.removeFromLeft (line.getWidth() / 2).reduced (1));
    chordTrackKey->setBounds (line.reduced (1));
    line = row (28);
    suggestKey->setBounds (line.removeFromRight (90).reduced (1));
    scaleBox.setBounds (line.reduced (1));
    line = row (28);
    snapScaleKey->setBounds (line.removeFromLeft (line.getWidth() / 2).reduced (1));
    guidesKey->setBounds (line.reduced (1));
    quantizeKey->setBounds (row (30).reduced (1));
    heading ("DISPLAY");
    referenceBox.setBounds (row (26).reduced (1));
    coloursBox.setBounds (row (26).reduced (1));
    area.removeFromTop (6);
    auditionKey->setBounds (row (30).reduced (1));
    info.setBounds (area.removeFromTop (60));
    panel.setSize (width, juce::jmax (area.getY() - top + 20, panelView.getHeight()));
}

void VariAudioEditor::paintPanel (juce::Graphics& g) const
{
    g.fillAll (pageColour);
    g.setFont (ParagonLookAndFeel::titleFont (13.0f));
    for (const auto& [text, area] : headings)
    {
        g.setColour (ParagonColours::accent);
        g.drawText (text, area, juce::Justification::centredLeft, false);
        g.setColour (ParagonColours::panelEdge);
        g.fillRect (area.withY (area.getBottom() - 1).withHeight (1));
    }
}

void VariAudioEditor::paint (juce::Graphics& g)
{
    g.fillAll (pageColour);
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (panelWidth - 1, 0, 1, getHeight());
    paintKeyboard (g, getKeyboardArea());
    paintDisplay (g, getDisplayArea());
    // The ruler: bars.
    const auto ruler = getLocalBounds().withTrimmedLeft (panelWidth + keyboardWidth).withHeight (rulerHeight);
    g.setColour (pageColour.brighter (0.06f));
    g.fillRect (ruler);
    const auto perBar = juce::jmax (1.0, studio.getProject().beatsPerBar());
    g.setFont (ParagonLookAndFeel::textFont (13.0f));
    for (auto bar = std::floor (firstBeat / perBar) * perBar; ; bar += perBar)
    {
        const auto x = (float) (ruler.getX() + (bar - firstBeat) * pixelsPerBeat);
        if (x > (float) ruler.getRight())
            break;
        if (x < (float) ruler.getX())
            continue;
        g.setColour (ParagonColours::textDim);
        g.drawVerticalLine ((int) x, (float) ruler.getY() + 12.0f, (float) ruler.getBottom());
        g.drawText (juce::String ((int) std::lround (bar / perBar) + 1), juce::Rectangle<float> (x + 3.0f, (float) ruler.getY(), 40.0f, 14.0f),
                    juce::Justification::centredLeft, false);
    }
}

void VariAudioEditor::paintKeyboard (juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.saveState();
    g.reduceClipRegion (area);
    g.setColour (pearl);
    g.fillRect (area);
    std::set<int> lit;
    for (const auto i : selection)
        if (i < (int) segments().size())
            lit.insert ((int) std::lround (segments()[(size_t) i].pitchNow()));
    g.setFont (ParagonLookAndFeel::textFont (juce::jmin (13.0f, rowHeight)));
    for (auto pitch = (int) std::floor (pitchForY ((float) area.getBottom())); pitch <= (int) std::ceil (topPitch); ++pitch)
    {
        const auto y = yForPitch ((float) pitch + 0.5f);
        const juce::Rectangle<float> key ((float) area.getX(), y, (float) area.getWidth(), rowHeight);
        if (lit.count (pitch) > 0)
            g.setColour (pink);
        else
            g.setColour (isBlackKey (pitch) ? ParagonColours::ink : pearl);
        g.fillRect (isBlackKey (pitch) ? key.withWidth (key.getWidth() * 0.62f) : key);
        g.setColour (ParagonColours::metalDark);
        g.drawHorizontalLine ((int) key.getBottom(), key.getX(), key.getRight());
        if (pitch % 12 == 0 || rowHeight >= 14.0f)
        {
            g.setColour (isBlackKey (pitch) && lit.count (pitch) == 0 ? ParagonColours::inkDim : ParagonColours::ink);
            g.drawText (pr7s::noteName (pitch), key.withTrimmedRight (4.0f), juce::Justification::centredRight, false);
        }
    }
    g.restoreState();
}

void VariAudioEditor::paintDisplay (juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.saveState();
    g.reduceClipRegion (area);
    g.setColour (viewColour);
    g.fillRect (area);
    const auto* p = part();
    // Rows: black keys and (with the guides) notes outside the scale darker; C lines.
    const auto viewScale = scaleAt (firstBeat + area.getWidth() * 0.5 / pixelsPerBeat).scale();
    for (auto pitch = (int) std::floor (pitchForY ((float) area.getBottom())); pitch <= (int) std::ceil (topPitch); ++pitch)
    {
        const auto y = yForPitch ((float) pitch + 0.5f);
        const auto outside = scaleGuides && std::find (viewScale.begin(), viewScale.end(), (pitch % 12 + 12) % 12) == viewScale.end();
        if (outside || isBlackKey (pitch))
        {
            g.setColour (outside ? darkRow.darker (0.5f) : darkRow);
            g.fillRect (juce::Rectangle<float> ((float) area.getX(), y, (float) area.getWidth(), rowHeight));
        }
        if (pitch % 12 == 0)
        {
            g.setColour (ParagonColours::panelEdge);
            g.drawHorizontalLine ((int) (y + rowHeight), (float) area.getX(), (float) area.getRight());
        }
    }
    // Bars.
    const auto perBar = juce::jmax (1.0, studio.getProject().beatsPerBar());
    g.setColour (ParagonColours::panelEdge);
    for (auto bar = std::floor (firstBeat / perBar) * perBar; ; bar += perBar)
    {
        const auto x = (float) (area.getX() + (bar - firstBeat) * pixelsPerBeat);
        if (x > (float) area.getRight())
            break;
        g.drawVerticalLine ((int) x, (float) area.getY(), (float) area.getBottom());
    }
    if (p == nullptr)
    {
        g.restoreState();
        return;
    }
    // The clip's edges.
    const auto clipX0 = (float) (area.getX() + (p->start - firstBeat) * pixelsPerBeat), clipX1 = (float) (area.getX() + (p->end() - firstBeat) * pixelsPerBeat);
    g.setColour (orange.withAlpha (0.05f));
    g.fillRect (juce::Rectangle<float> (clipX0, (float) area.getY(), clipX1 - clipX0, (float) area.getHeight()));
    // The waveform, faint behind (as heard now).
    if (const auto heard = studio.getHeardAudio (p->audioFile))
    {
        g.setColour (pearl.withAlpha (0.05f));
        const auto mid = (float) area.getCentreY(), half = area.getHeight() * 0.4f;
        const auto tempo = studio.getProject().tempoMap();
        for (auto x = juce::jmax (area.getX(), (int) clipX0); x < juce::jmin (area.getRight(), (int) clipX1); ++x)
        {
            const auto beatA = firstBeat + (x - area.getX()) / pixelsPerBeat - p->start;
            const auto a = pr7s::warpSecondsAt (*p, beatA, tempo), b = pr7s::warpSecondsAt (*p, beatA + 1.0 / pixelsPerBeat, tempo);
            const auto i0 = juce::jmax (0, (int) (a * heard->sampleRate) / pr7s::Studio::AudioData::peakStep);
            const auto i1 = juce::jmin ((int) heard->peaks.size(), juce::jmax (i0 + 1, (int) (b * heard->sampleRate) / pr7s::Studio::AudioData::peakStep));
            auto peak = 0.0f;
            for (int i = i0; i < i1; ++i)
                peak = juce::jmax (peak, heard->peaks[(size_t) i]);
            if (peak > 0.002f)
                g.drawVerticalLine (x, mid - peak * half, mid + peak * half);
        }
    }
    const auto& segs = segments();
    if (segs.empty())
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::textFont (16.0f));
        g.drawText (getInfoText(), area, juce::Justification::centred, true);
        g.restoreState();
        return;
    }
    const auto analysis = studio.getPitchAnalysis (p->audioFile);
    const auto isSelected = [this] (int i) { return std::find (selection.begin(), selection.end(), i) != selection.end(); };
    // (0.35.0) The MIDI reference track's notes, as lines (to tune and time against).
    if (const auto* reference = studio.getProject().findTrack (studio.getProject().variAudioReference))
    {
        g.setColour (pearl.withAlpha (0.55f));
        for (const auto& refPart : reference->parts)
            for (const auto& note : refPart.notes)
            {
                const auto x0 = xForBeat (refPart.start + note.start), x1 = xForBeat (refPart.start + note.start + note.length);
                if (x1 < (float) area.getX() || x0 > (float) area.getRight())
                    continue;
                const auto y = yForPitch ((float) note.pitch);
                g.fillRect (juce::Rectangle<float> (x0, y - 1.0f, juce::jmax (2.0f, x1 - x0), 2.0f));
                g.fillRect (juce::Rectangle<float> (x0, y - 4.0f, 1.5f, 8.0f));
            }
    }
    g.setFont (ParagonLookAndFeel::textFont (13.0f));
    for (int i = 0; i < (int) segs.size(); ++i)
    {
        const auto r = getSegmentBounds (i);
        if (r.getRight() < (float) area.getX() || r.getX() > (float) area.getRight())
            continue;
        const auto& s = segs[(size_t) i];
        // (0.35.0) Its colour by the scheme chosen; selected: pink (Auto), else its own lit, edged in pearl.
        const auto plain = colours == Colours::automatic;
        const auto colour = isSelected (i) ? (plain ? pink : colourOf (i).brighter (0.25f)) : colourOf (i);
        g.setColour (colour.withAlpha (isSelected (i) ? 0.75f : 0.45f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (isSelected (i) && ! plain ? pearl : colour.brighter (0.3f));
        g.drawRoundedRectangle (r, 3.0f, isSelected (i) && ! plain ? 2.0f : 1.0f);
        // The pitch curve (as it will be heard), and as it was (faint) when changed.
        if (analysis != nullptr && r.getWidth() > 6.0f)
        {
            juce::Path now, was;
            auto started = false;
            for (auto t = s.start; t <= s.end; t += analysis->hopSeconds)
            {
                const auto x = xForSeconds (t);
                const auto heard = analysis->pitchAt (t);
                const auto y = yForPitch (pr7s::editedPitchAt (s, *analysis, t));
                const auto y0 = yForPitch (heard > 0.0f && std::abs (heard - s.pitch) <= 3.0f ? heard : s.pitch);
                if (! started)
                    now.startNewSubPath (x, y), was.startNewSubPath (x, y0), started = true;
                else
                    now.lineTo (x, y), was.lineTo (x, y0);
            }
            if (s.isEdited())
            {
                g.setColour (pearl.withAlpha (0.25f));
                g.strokePath (was, juce::PathStrokeType (1.0f));
            }
            g.setColour (pearl.withAlpha (0.9f));
            g.strokePath (now, juce::PathStrokeType (1.5f));
        }
        if (i == hover)
        {
            g.setColour (pearl);
            g.drawText (centsText (s.pitchNow()), r.translated (0.0f, -rowHeight - 6.0f).withWidth (juce::jmax (r.getWidth(), 140.0f)),
                        juce::Justification::centredLeft, false);
        }
        // The smart controls (on the segment under the mouse, when there's room).
        if (i == hover)
            paintSmartControls (g, i);
    }
    // (0.35.0) A segment's start or end being warped: where it goes.
    if (gesture == Gesture::warpEdge && dragged)
    {
        g.setColour (orange);
        g.fillRect (juce::Rectangle<float> (warpToX - 1.0f, (float) area.getY(), 2.0f, (float) area.getHeight()));
        g.setFont (ParagonLookAndFeel::textFont (13.0f));
        g.drawText (control == SmartControl::warpStart ? "WARP START" : "WARP END",
                    juce::Rectangle<float> (warpToX + 4.0f, (float) area.getY() + 4.0f, 120.0f, 16.0f), juce::Justification::centredLeft, false);
    }
    if (gesture == Gesture::marquee)
    {
        const auto box = juce::Rectangle<float> (downAt, dragAt);
        g.setColour (pearl.withAlpha (0.12f));
        g.fillRect (box);
        g.setColour (pearl.withAlpha (0.6f));
        g.drawRect (box, 1.0f);
    }
    g.restoreState();
}

void VariAudioEditor::paintSmartControls (juce::Graphics& g, int index) const
{
    // Each in its colour (pearl for the curve, red correct, orange time and glue, pink split, formant
    // and the anchor), formant and volume marked F and V.
    g.setFont (ParagonLookAndFeel::textFont (9.0f));
    const auto draw = [&] (SmartControl which, juce::Colour colour, const char* letter = nullptr)
    {
        const auto box = getSmartControl (index, which);
        if (box.isEmpty())
            return;
        g.setColour (colour);
        g.fillRect (box);
        if (letter != nullptr)
        {
            g.setColour (ParagonColours::ink);
            g.drawText (letter, box, juce::Justification::centred, false);
        }
    };
    draw (SmartControl::straighten, pearl);
    draw (SmartControl::correct, red);
    draw (SmartControl::glueLeft, orange);
    draw (SmartControl::glueRight, orange);
    draw (SmartControl::split, pink.withAlpha (0.7f));
    draw (SmartControl::warpStart, orange.withAlpha (0.8f));
    draw (SmartControl::warpEnd, orange.withAlpha (0.8f));
    draw (SmartControl::formant, pink, "F");
    draw (SmartControl::volume, orange, "V");
    draw (SmartControl::rangeFrom, pearl.withAlpha (0.8f));
    draw (SmartControl::rangeTo, pearl.withAlpha (0.8f));
    for (const auto which : { SmartControl::tiltLeft, SmartControl::tiltRight })
        if (const auto box = getSmartControl (index, which); ! box.isEmpty())
        {
            juce::Path triangle;
            triangle.addTriangle (box.getX(), box.getBottom(), box.getRight(), box.getBottom(), box.getCentreX(), box.getY());
            g.setColour (pearl);
            g.fillPath (triangle);
        }
    if (const auto box = getSmartControl (index, SmartControl::anchor); ! box.isEmpty())
    {
        juce::Path triangle;
        triangle.addTriangle (box.getX(), box.getY(), box.getRight(), box.getY(), box.getCentreX(), box.getBottom());
        g.setColour (pink);
        g.fillPath (triangle);
    }
}

//==============================================================================
void VariAudioEditor::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    gesture = Gesture::none;
    dragged = false;
    downAt = dragAt = e.position;
    const auto& segs = segments();
    if (getKeyboardArea().toFloat().contains (e.position))
    {
        // Ctrl-click a key: every segment on that note (a little off it too).
        if (e.mods.isCommandDown())
        {
            const auto pitch = (float) std::lround (pitchForY (e.position.y));
            std::vector<int> found;
            for (int i = 0; i < (int) segs.size(); ++i)
                if (std::abs (segs[(size_t) i].pitchNow() - pitch) < 0.5f)
                    found.push_back (i);
            selectSegments (found);
        }
        return;
    }
    if (! getDisplayArea().toFloat().contains (e.position.withY (juce::jmax (e.position.y, (float) getDisplayArea().getY()))) || segs.empty())
        return;
    const auto inSelection = [this] (int i) { return std::find (selection.begin(), selection.end(), i) != selection.end(); };
    // The smart controls first.
    altAtDown = e.mods.isAltDown();
    if (const auto which = smartControlAt (hover, e.position); which != SmartControl::none)
    {
        const auto index = hover;
        if (! inSelection (index))
            selection = { index };
        control = which;
        controlIndex = index;
        warpToX = e.position.x;
        switch (which)
        {
            // Clicks.
            case SmartControl::glueLeft:
            case SmartControl::glueRight:
                glue (index, which == SmartControl::glueRight);
                return;
            case SmartControl::split:
                splitAt (index, secondsForX (e.position.x));
                return;
            case SmartControl::anchor:
                if (altAtDown)
                {
                    // Alt-click: the anchor back to the middle.
                    auto changed = segs;
                    changed[(size_t) index].anchor = 0.5f;
                    commit (std::move (changed));
                    return;
                }
                gesture = Gesture::anchor;
                break;
            // Drags.
            case SmartControl::straighten: gesture = Gesture::straighten; break;
            case SmartControl::correct:    gesture = Gesture::correct; break;
            case SmartControl::warpStart:
            case SmartControl::warpEnd:    gesture = altAtDown ? Gesture::resizeEdge : Gesture::warpEdge; break;
            case SmartControl::tiltLeft:
            case SmartControl::tiltRight:  gesture = Gesture::tilt; break;
            case SmartControl::rangeFrom:
            case SmartControl::rangeTo:    gesture = Gesture::range; break;
            case SmartControl::formant:    gesture = Gesture::formant; break;
            case SmartControl::volume:     gesture = Gesture::volume; break;
            case SmartControl::none:       break;
        }
        base = segs;
        working = base;
        return;
    }
    const auto hit = segmentAt (e.position);
    if (hit < 0)
    {
        if (! e.mods.isShiftDown() && ! e.mods.isCommandDown())
            selection.clear();
        gesture = Gesture::marquee;
        refresh();
        return;
    }
    if (e.mods.isShiftDown() || e.mods.isCommandDown())
    {
        auto changed = selection;
        if (inSelection (hit))
            changed.erase (std::find (changed.begin(), changed.end(), hit));
        else
            changed.push_back (hit);
        selectSegments (changed);
        return;
    }
    if (! inSelection (hit))
        selectSegments ({ hit });
    gesture = Gesture::move;
    base = segs;
    working = base;
}

void VariAudioEditor::mouseDrag (const juce::MouseEvent& e)
{
    dragAt = e.position;
    if (e.getDistanceFromDragStart() > 3)
        dragged = true;
    const auto dy = downAt.y - e.position.y;
    switch (gesture)
    {
        case Gesture::move:
        {
            if (! dragged)
                return;
            const auto mode = snapFor (e.mods);
            working = base;
            for (const auto i : selection)
            {
                auto& s = working[(size_t) i];
                s.shift = pr7s::movedShift (base[(size_t) i], dy / rowHeight, mode);
                if (snapToScale)
                    s.shift = pr7s::quantizedShift (s, scaleAt (beatForSeconds (s.start)), mode);
            }
            break;
        }
        case Gesture::straighten:
            working = base;
            for (const auto i : selection)
                working[(size_t) i].straighten = juce::jlimit (0.0f, 1.0f, base[(size_t) i].straighten + dy / 120.0f);
            break;
        case Gesture::correct:
            working = base;
            for (const auto i : selection)
                working[(size_t) i].shift = pr7s::correctedShift (base[(size_t) i], juce::jlimit (0.0f, 1.0f, std::abs (dy) / 120.0f));
            break;
        case Gesture::warpEdge:
            warpToX = e.position.x;   // (applied on release: the warp markers, one undo step)
            break;
        case Gesture::resizeEdge:
            if (const auto analysis = studio.getPitchAnalysis (file()))
            {
                working = base;
                const auto& seg = base[(size_t) controlIndex];
                const auto at = secondsForX (e.position.x);
                pr7s::setSegmentEdges (working, controlIndex, control == SmartControl::warpStart ? at : seg.start,
                                       control == SmartControl::warpStart ? seg.end : at, *analysis);
            }
            break;
        case Gesture::tilt:
            working = base;
            for (const auto i : selection)
            {
                auto& seg = working[(size_t) i];
                if (altAtDown)
                    pr7s::rotateSegment (seg, dy / rowHeight, control == SmartControl::tiltLeft);   // (Alt: rotated round the anchor)
                else if (control == SmartControl::tiltLeft)
                    seg.tiltStart = base[(size_t) i].tiltStart + dy / rowHeight;
                else
                    seg.tiltEnd = base[(size_t) i].tiltEnd + dy / rowHeight;
            }
            break;
        case Gesture::anchor:
        {
            working = base;
            const auto r = getSegmentBounds (controlIndex);
            working[(size_t) controlIndex].anchor = juce::jlimit (0.02f, 0.98f, (e.position.x - r.getX()) / juce::jmax (1.0f, r.getWidth()));
            break;
        }
        case Gesture::range:
        {
            // The same range for all the selected (the manual's), from where it lands on this one.
            working = base;
            const auto r = getSegmentBounds (controlIndex);
            const auto along = juce::jlimit (0.0f, 1.0f, (e.position.x - r.getX()) / juce::jmax (1.0f, r.getWidth()));
            for (const auto i : selection)
            {
                auto& seg = working[(size_t) i];
                if (control == SmartControl::rangeFrom)
                    seg.straightenFrom = juce::jmin (along, seg.straightenTo - 0.05f);
                else
                    seg.straightenTo = juce::jmax (along, seg.straightenFrom + 0.05f);
            }
            break;
        }
        case Gesture::formant:
            working = base;
            for (const auto i : selection)
                working[(size_t) i].formant = juce::jlimit (-100.0f, 100.0f, base[(size_t) i].formant + dy * 0.5f);
            break;
        case Gesture::volume:
            working = base;
            for (const auto i : selection)
                working[(size_t) i].volumeDb = juce::jlimit (-48.0f, 12.0f, base[(size_t) i].volumeDb + dy / 6.0f);
            break;
        case Gesture::marquee:
        case Gesture::none:
        case Gesture::slider:
            break;
    }
    repaint();
}

void VariAudioEditor::mouseUp (const juce::MouseEvent& e)
{
    const auto was = gesture;
    if (was == Gesture::marquee)
    {
        gesture = Gesture::none;
        const auto box = juce::Rectangle<float> (downAt, e.position);
        auto chosen = selection;
        if (dragged)
            for (int i = 0; i < (int) segments().size(); ++i)
                if (box.intersects (getSegmentBounds (i)))
                    chosen.push_back (i);
        selectSegments (chosen);
        return;
    }
    if (was == Gesture::warpEdge)
    {
        gesture = Gesture::none;
        working.clear();
        base.clear();
        if (dragged)
            studio.warpVariSegment (partId, controlIndex, control == SmartControl::warpStart,
                                    firstBeat + (warpToX - getDisplayArea().getX()) / pixelsPerBeat);
        refresh();
        return;
    }
    if ((was == Gesture::move || was == Gesture::straighten || was == Gesture::correct || was == Gesture::resizeEdge || was == Gesture::tilt
         || was == Gesture::anchor || was == Gesture::range || was == Gesture::formant || was == Gesture::volume) && dragged)
    {
        commit (working);
        return;
    }
    gesture = Gesture::none;
    working.clear();
    base.clear();
    if (was == Gesture::move && ! dragged)
    {
        // Alt-click: split there (the manual's: anywhere above the Split control); else select it.
        if (altAtDown && segmentAt (e.position) >= 0)
        {
            splitAt (segmentAt (e.position), secondsForX (e.position.x));
            return;
        }
        selectSegments ({ segmentAt (e.position) });
    }
    repaint();
}

void VariAudioEditor::mouseMove (const juce::MouseEvent& e)
{
    // The segment under the mouse, or whose smart controls it's over.
    auto now = segmentAt (e.position);
    if (hover >= 0 && smartControlAt (hover, e.position) != SmartControl::none)
        now = hover;   // (over its controls, some outside it: still that one)
    if (now != hover)
    {
        hover = now;
        repaint();
    }
    switch (smartControlAt (hover, e.position))
    {
        case SmartControl::straighten: case SmartControl::correct: case SmartControl::tiltLeft: case SmartControl::tiltRight:
        case SmartControl::formant: case SmartControl::volume:
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor); break;
        case SmartControl::warpStart: case SmartControl::warpEnd: case SmartControl::anchor: case SmartControl::rangeFrom: case SmartControl::rangeTo:
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); break;
        case SmartControl::split:
            setMouseCursor (juce::MouseCursor::IBeamCursor); break;
        case SmartControl::glueLeft: case SmartControl::glueRight:
            setMouseCursor (juce::MouseCursor::PointingHandCursor); break;
        case SmartControl::none:
            setMouseCursor (hover >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor); break;
    }
}

void VariAudioEditor::mouseExit (const juce::MouseEvent&)
{
    if (hover >= 0)
    {
        hover = -1;
        repaint();
    }
}

void VariAudioEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Shift + double-click: this segment and every one after it on the same note.
    const auto hit = segmentAt (e.position);
    if (hit < 0 || ! e.mods.isShiftDown())
        return;
    const auto& segs = segments();
    const auto pitch = std::lround (segs[(size_t) hit].pitchNow());
    std::vector<int> chosen;
    for (int i = hit; i < (int) segs.size(); ++i)
        if (std::lround (segs[(size_t) i].pitchNow()) == pitch)
            chosen.push_back (i);
    selectSegments (chosen);
}

void VariAudioEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto amount = std::abs (wheel.deltaY) > 0.0f ? wheel.deltaY : wheel.deltaX;
    if (e.mods.isCommandDown())
    {
        const auto beatAt = firstBeat + (e.position.x - getDisplayArea().getX()) / pixelsPerBeat;
        pixelsPerBeat = juce::jlimit (4.0, 4000.0, pixelsPerBeat * (amount > 0 ? 1.25 : 0.8));
        firstBeat = beatAt - (e.position.x - getDisplayArea().getX()) / pixelsPerBeat;
    }
    else if (e.mods.isShiftDown())
        firstBeat -= amount * 200.0 / pixelsPerBeat;
    else
        topPitch = juce::jlimit (12.0f, 127.0f, topPitch + amount * 6.0f);
    repaint();
}

bool VariAudioEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const auto code = key.getKeyCode();
    const auto& segs = segments();
    if ((code == juce::KeyPress::upKey || code == juce::KeyPress::downKey) && ! selection.empty())
    {
        const auto up = code == juce::KeyPress::upKey;
        if (mods.isShiftDown())
            moveSelected (up ? 0.01f : -0.01f, PitchSnap::off);   // (a cent, the snap ignored)
        else
            moveSelected (up ? 1.0f : -1.0f, snap);
        return true;
    }
    if ((code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey) && ! segs.empty())
    {
        const auto right = code == juce::KeyPress::rightKey;
        auto at = selection.empty() ? (right ? -1 : (int) segs.size()) : (right ? selection.back() : selection.front());
        at = juce::jlimit (0, (int) segs.size() - 1, at + (right ? 1 : -1));
        selectSegments ({ at });
        return true;
    }
    if (mods.isCommandDown() && (code == 'A' || code == 'a'))
    {
        std::vector<int> all ((size_t) segs.size());
        std::iota (all.begin(), all.end(), 0);
        selectSegments (all);
        return true;
    }
    if (! mods.isAnyModifierKeyDown() && (code == 'H' || code == 'h' || code == 'G' || code == 'g'))
    {
        const auto in = code == 'H' || code == 'h';
        const auto middle = firstBeat + getDisplayArea().getWidth() * 0.5 / pixelsPerBeat;
        pixelsPerBeat = juce::jlimit (4.0, 4000.0, pixelsPerBeat * (in ? 1.5 : 1.0 / 1.5));
        firstBeat = middle - getDisplayArea().getWidth() * 0.5 / pixelsPerBeat;
        repaint();
        return true;
    }
    if ((code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) && ! selection.empty())
    {
        deleteSelected();   // (the manual's Backspace; Delete too)
        return true;
    }
    if (code == juce::KeyPress::escapeKey && ! selection.empty())
    {
        selectSegments ({});
        return true;
    }
    return onOtherKey && onOtherKey (key);
}

//==============================================================================
VariAudioWindow::VariAudioWindow (pr7s::Studio& s, int partId)
    : DocumentWindow ("Paragon PR7-S: VariAudio", pageColour, DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    editor = new VariAudioEditor (s, partId);
    setContentOwned (editor, true);
    setResizable (true, false);
    setResizeLimits (900, 520, 8000, 3000);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
}

void VariAudioWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}

//==============================================================================
namespace
{
    void styleLabel (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (ParagonLookAndFeel::textFont (14.0f));
        label.setColour (juce::Label::textColourId, ParagonColours::textDim);
    }

    void styleBar (juce::Slider& slider, double low, double high, double value, const juce::String& suffix)
    {
        slider.setSliderStyle (juce::Slider::LinearBar);
        slider.setRange (low, high, 1.0);
        slider.setValue (value, juce::dontSendNotification);
        slider.setTextValueSuffix (suffix);
        slider.setColour (juce::Slider::trackColourId, ParagonColours::accent.withAlpha (0.6f));
        slider.setColour (juce::Slider::textBoxTextColourId, ParagonColours::text);
    }
}

ExtractMidiPage::ExtractMidiPage()
{
    pitch.addItem ("Just notes, no pitchbend", 1);
    pitch.addItem ("Notes and static pitchbend (one per segment)", 2);
    pitch.addItem ("Notes and continuous pitchbend (the curve)", 3);
    pitch.setSelectedId (1, juce::dontSendNotification);
    volume.addItem ("Fixed velocity", 1);
    volume.addItem ("Dynamic velocity (each segment's level)", 2);
    volume.addItem ("Volume controller curve", 3);
    volume.setSelectedId (1, juce::dontSendNotification);
    destination.addItem ("The first selected instrument track", 1);
    destination.addItem ("A new instrument track", 2);
    destination.setSelectedId (2, juce::dontSendNotification);
    styleBar (range, 1.0, 24.0, 2.0, " semitones");
    styleBar (velocity, 1.0, 127.0, 100.0, {});
    styleBar (controller, 0.0, 127.0, 7.0, {});
    range.setTooltip ("Pitchbend Range: set your instrument to the same");
    controller.setTooltip ("The controller the volume curve is written to (7: volume, 11: expression)");
    styleLabel (pitchLabel, "PITCH");
    styleLabel (rangeLabel, "PITCHBEND RANGE");
    styleLabel (volumeLabel, "VOLUME");
    styleLabel (velocityLabel, "VELOCITY");
    styleLabel (controllerLabel, "MIDI CONTROLLER");
    styleLabel (destinationLabel, "PUT IT ON");
    for (auto* c : std::initializer_list<juce::Component*> { &pitch, &volume, &destination, &range, &velocity, &controller, &pitchLabel, &rangeLabel,
                                                              &volumeLabel, &velocityLabel, &controllerLabel, &destinationLabel, &ok, &cancel })
        addAndMakeVisible (*c);
    const auto enable = [this]
    {
        range.setEnabled (pitch.getSelectedId() != 1);
        velocity.setEnabled (volume.getSelectedId() != 2);
        controller.setEnabled (volume.getSelectedId() == 3);
    };
    pitch.onChange = enable;
    volume.onChange = enable;
    enable();
    ok.onClick = [this] { if (onOk) onOk(); };
    cancel.onClick = [this] { if (onCancel) onCancel(); };
    setSize (440, 330);
}

void ExtractMidiPage::paint (juce::Graphics& g)
{
    g.fillAll (pageColour);
}

void ExtractMidiPage::resized()
{
    auto area = getLocalBounds().reduced (14, 12);
    const auto pair = [&] (juce::Label& label, juce::Component& control)
    {
        auto line = area.removeFromTop (26);
        label.setBounds (line.removeFromLeft (140));
        control.setBounds (line);
        area.removeFromTop (8);
    };
    pair (pitchLabel, pitch);
    pair (rangeLabel, range);
    pair (volumeLabel, volume);
    pair (velocityLabel, velocity);
    pair (controllerLabel, controller);
    pair (destinationLabel, destination);
    auto buttons = area.removeFromBottom (30);
    cancel.setBounds (buttons.removeFromRight (110));
    buttons.removeFromRight (8);
    ok.setBounds (buttons.removeFromRight (140));
}

pr7s::ExtractMidiOptions ExtractMidiPage::getOptions() const
{
    pr7s::ExtractMidiOptions options;
    options.pitch = (pr7s::ExtractMidiOptions::Pitch) (pitch.getSelectedId() - 1);
    options.bendRange = (int) range.getValue();
    options.volume = (pr7s::ExtractMidiOptions::Volume) (volume.getSelectedId() - 1);
    options.velocity = (int) velocity.getValue();
    options.controller = (int) controller.getValue();
    return options;
}

pr7s::Studio::ExtractDestination ExtractMidiPage::getDestination() const
{
    return destination.getSelectedId() == 1 ? pr7s::Studio::ExtractDestination::selectedTrack : pr7s::Studio::ExtractDestination::newTrack;
}

//==============================================================================
HarmonyVoicesPage::HarmonyVoicesPage()
{
    for (int n = 1; n <= 4; ++n)
        voices.addItem (juce::String (n) + (n == 1 ? " voice (Soprano)" : n == 2 ? " voices (Soprano, Alto)" : n == 3 ? " voices (Soprano, Alto, Tenor)"
                                                                                                                 : " voices (Soprano, Alto, Tenor, Bass)"), n);
    voices.setSelectedId (4, juce::dontSendNotification);
    styleBar (vibrato, 0.0, 100.0, 50.0, " %");
    vibrato.setTooltip ("Reduce Vibrato Strength: each voice's curve straightened this much");
    styleLabel (voicesLabel, "NUMBER OF VOICES");
    styleLabel (vibratoLabel, "REDUCE VIBRATO");
    styleLabel (note, "Each voice is a new track with its own copy of the clip and its recording. With chords on the chord "
                      "track they follow the chord there; without, they're 3 semitones up, and 3, 6 and 9 down.");
    note.setJustificationType (juce::Justification::topLeft);
    openAfter.setColour (juce::ToggleButton::textColourId, ParagonColours::text);
    openAfter.setColour (juce::ToggleButton::tickColourId, ParagonColours::accent);
    for (auto* c : std::initializer_list<juce::Component*> { &voices, &vibrato, &openAfter, &voicesLabel, &vibratoLabel, &note, &ok, &cancel })
        addAndMakeVisible (*c);
    ok.onClick = [this] { if (onOk) onOk(); };
    cancel.onClick = [this] { if (onCancel) onCancel(); };
    setSize (440, 250);
}

void HarmonyVoicesPage::paint (juce::Graphics& g)
{
    g.fillAll (pageColour);
}

void HarmonyVoicesPage::resized()
{
    auto area = getLocalBounds().reduced (14, 12);
    auto line = area.removeFromTop (26);
    voicesLabel.setBounds (line.removeFromLeft (150));
    voices.setBounds (line);
    area.removeFromTop (8);
    line = area.removeFromTop (26);
    vibratoLabel.setBounds (line.removeFromLeft (150));
    vibrato.setBounds (line);
    area.removeFromTop (8);
    openAfter.setBounds (area.removeFromTop (24));
    area.removeFromTop (6);
    auto buttons = area.removeFromBottom (30);
    cancel.setBounds (buttons.removeFromRight (110));
    buttons.removeFromRight (8);
    ok.setBounds (buttons.removeFromRight (140));
    note.setBounds (area);
}

void HarmonyVoicesPage::show (pr7s::Studio& studio, int partId, juce::Component& target, std::function<void (int, juce::String)> done)
{
    auto page = std::make_unique<HarmonyVoicesPage>();
    auto* raw = page.get();
    auto& box = juce::CallOutBox::launchAsynchronously (std::move (page), target.getScreenBounds(), nullptr);
    juce::Component::SafePointer<juce::CallOutBox> boxSafe (&box);
    raw->onCancel = [boxSafe] { if (boxSafe != nullptr) boxSafe->dismiss(); };
    raw->onOk = [&studio, partId, raw, boxSafe, done]
    {
        juce::String error;
        const auto tracks = studio.generateHarmonyVoices (partId, raw->getVoices(), raw->getVibratoReduction(), error);
        auto first = 0;
        if (raw->getOpenAfter() && ! tracks.empty())
            if (const auto* track = studio.getProject().findTrack (tracks.front()); track != nullptr && ! track->parts.empty())
                first = track->parts.front().id;
        if (boxSafe != nullptr)
            boxSafe->dismiss();
        if (done)
            done (first, tracks.empty() && error.isEmpty() ? juce::String ("No voices were made.") : error);
    };
}
