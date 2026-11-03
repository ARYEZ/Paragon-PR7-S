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
    addAndMakeVisible (*key);
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
    scaleBox.onChange = [this]
    {
        const auto id = scaleBox.getSelectedId() - 1;
        if (id >= 0)
            setEditorScale (pr7s::Key::fromPitchClass (id / 2, id % 2 == 1));
    };
    addAndMakeVisible (scaleBox);

    // The sliders: the selected segments, heard when let go (one undo step).
    const auto setUp = [this] (juce::Slider& slider, double low, double high, const juce::String& suffix,
                               std::function<void (VariSegment&, const VariSegment& before, double value)> edit)
    {
        slider.setSliderStyle (juce::Slider::LinearBar);
        slider.setRange (low, high, 1.0);
        slider.setTextValueSuffix (suffix);
        slider.setColour (juce::Slider::trackColourId, ParagonColours::accent.withAlpha (0.6f));
        slider.setColour (juce::Slider::textBoxTextColourId, ParagonColours::text);
        addAndMakeVisible (slider);
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
    correct.setTooltip ("Correct Pitch: the selected segments that far to the nearest semitone");
    straighten.setTooltip ("Straighten Curve: the wobble around each segment's pitch flattened (vibrato, drifts)");
    volume.setTooltip ("The selected segments' level");

    for (auto* label : { &title, &info, &correctLabel, &straightenLabel, &volumeLabel })
    {
        label->setFont (ParagonLookAndFeel::textFont (14.0f));
        label->setColour (juce::Label::textColourId, ParagonColours::textDim);
        label->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (*label);
    }
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
    return text;
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
    const auto r = getSegmentBounds (index);
    return juce::Rectangle<float> (12.0f, 7.0f).withCentre ({ r.getCentreX(), r.getY() - 5.0f });
}

juce::Rectangle<float> VariAudioEditor::getCorrectHandle (int index) const
{
    const auto r = getSegmentBounds (index);
    return juce::Rectangle<float> (12.0f, 7.0f).withCentre ({ r.getCentreX(), r.getBottom() + 5.0f });
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
    info.setText (getInfoText(), juce::dontSendNotification);
    repaint();
}

void VariAudioEditor::resized()
{
    headings.clear();
    auto panel = getLocalBounds().removeFromLeft (panelWidth).reduced (12, 10);
    title.setBounds (panel.removeFromTop (28));
    panel.removeFromTop (6);
    const auto row = [&panel] (int h) { auto line = panel.removeFromTop (h); panel.removeFromTop (6); return line; };
    const auto heading = [&] (const juce::String& text) { headings.push_back ({ text, row (18) }); };
    analyseKey->setBounds (row (32).reduced (1));
    auto line = row (28);
    bypassKey->setBounds (line.removeFromLeft (line.getWidth() / 2).reduced (1));
    resetKey->setBounds (line.reduced (1));
    heading ("PITCH SNAP");
    line = row (28);
    const auto third = line.getWidth() / 3;
    for (auto* key : snapKeys)
        key->setBounds (line.removeFromLeft (third).reduced (1));
    heading ("SELECTED SEGMENTS");
    for (auto [label, slider] : { std::pair { &correctLabel, &correct }, std::pair { &straightenLabel, &straighten }, std::pair { &volumeLabel, &volume } })
    {
        label->setBounds (row (16));
        slider->setBounds (row (24));
    }
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
    panel.removeFromTop (6);
    auditionKey->setBounds (row (30).reduced (1));
    info.setBounds (panel);
    if (getWidth() > 0 && segments().empty())
        fitView();
}

void VariAudioEditor::paint (juce::Graphics& g)
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
    g.setFont (ParagonLookAndFeel::textFont (13.0f));
    for (int i = 0; i < (int) segs.size(); ++i)
    {
        const auto r = getSegmentBounds (i);
        if (r.getRight() < (float) area.getX() || r.getX() > (float) area.getRight())
            continue;
        const auto& s = segs[(size_t) i];
        const auto colour = isSelected (i) ? pink : orange;
        g.setColour (colour.withAlpha (isSelected (i) ? 0.75f : 0.45f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (colour.brighter (0.3f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
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
        if (i == hover && r.getWidth() >= 30.0f)
        {
            g.setColour (pearl);
            g.fillRect (getStraightenHandle (i));
            g.setColour (red);
            g.fillRect (getCorrectHandle (i));
        }
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
    if (hover >= 0 && getSegmentBounds (hover).getWidth() >= 30.0f)
    {
        if (getStraightenHandle (hover).expanded (3.0f).contains (e.position) || getCorrectHandle (hover).expanded (3.0f).contains (e.position))
        {
            if (! inSelection (hover))
                selection = { hover };
            gesture = getStraightenHandle (hover).expanded (3.0f).contains (e.position) ? Gesture::straighten : Gesture::correct;
            base = segs;
            working = base;
            return;
        }
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
    if ((was == Gesture::move || was == Gesture::straighten || was == Gesture::correct) && dragged)
    {
        commit (working);
        return;
    }
    gesture = Gesture::none;
    working.clear();
    base.clear();
    if (was == Gesture::move && ! dragged)
        selectSegments ({ segmentAt (e.position) });
    repaint();
}

void VariAudioEditor::mouseMove (const juce::MouseEvent& e)
{
    // The segment under the mouse, or whose smart controls it's over.
    auto now = segmentAt (e.position);
    if (now < 0 && hover >= 0
        && (getStraightenHandle (hover).expanded (3.0f).contains (e.position) || getCorrectHandle (hover).expanded (3.0f).contains (e.position)))
        now = hover;
    if (now != hover)
    {
        hover = now;
        repaint();
    }
    const auto onHandle = hover >= 0 && (getStraightenHandle (hover).expanded (3.0f).contains (e.position) || getCorrectHandle (hover).expanded (3.0f).contains (e.position));
    setMouseCursor (onHandle ? juce::MouseCursor::UpDownResizeCursor : hover >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
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
