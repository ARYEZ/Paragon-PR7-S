#include "ChordPadsView.h"

#include "ui/ParagonLookAndFeel.h"

using pr7s::Chord;
using pr7s::ChordPads;

namespace
{
    juce::Font font (float height, int style = juce::Font::bold) { return juce::Font (ParagonLookAndFeel::textFont (height, style)); }
    // Chord symbols need small letters (Bebas has none: "Dm" would read "DM"): Inter.
    juce::Font chordFont (float height) { return juce::Font (juce::FontOptions (height, juce::Font::bold)); }

    const juce::Colour pearl { 0xfff4eeea };
    const juce::Colour padBody { 0xff1b171c }, padEmpty { 0xff121013 };

    void styleKey (juce::TextButton& key, bool menu = false)
    {
        key.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        key.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        key.setColour (juce::TextButton::textColourOnId, ParagonColours::accent);
        key.setWantsKeyboardFocus (false);
        if (menu)
            key.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    }

    bool isBlack (int note) { const auto p = ((note % 12) + 12) % 12; return p == 1 || p == 3 || p == 6 || p == 8 || p == 10; }

    // A MIDI note's name as the studio writes them (middle C = C3).
    juce::String keyName (int note, bool flats) { return Chord::noteName (note, flats) + juce::String (note / 12 - 2); }

    // A small padlock, for a locked pad.
    void drawLock (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour)
    {
        g.setColour (colour);
        const auto body = r.removeFromBottom (r.getHeight() * 0.58f);
        g.fillRoundedRectangle (body, 1.5f);
        juce::Path shackle;
        shackle.addCentredArc (body.getCentreX(), body.getY(), body.getWidth() * 0.32f, r.getHeight() * 0.85f, 0.0f,
                               -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        g.strokePath (shackle, juce::PathStrokeType (1.6f));
    }
}

//==============================================================================
// The chord editor (the manual's: a root note, a chord type, a tension, a bass note).
class ChordPadsView::ChordEditor final : public juce::Component
{
public:
    explicit ChordEditor (ChordPadsView& v) : view (v)
    {
        for (int i = 0; i < 12; ++i)
        {
            auto* r = roots.add (new juce::TextButton (Chord::noteName (i, false)));
            r->onClick = [this, i] { view.editRoot (i); };
            auto* b = basses.add (new juce::TextButton (Chord::noteName (i, false)));
            b->onClick = [this, i] { view.editBass (i); };
        }
        auto* none = basses.add (new juce::TextButton ("-"));
        none->onClick = [this] { view.editBass (-1); };
        for (const auto type : Chord::allTypes())
        {
            auto* t = types.add (new juce::TextButton (Chord::typeName (type)));
            t->onClick = [this, type] { view.editType (type); };
        }
        for (const auto tension : Chord::allTensions())
        {
            auto* t = tensions.add (new juce::TextButton (Chord::tensionName (tension, Chord::Type::major)));
            t->onClick = [this, tension] { view.editTension (tension); };
        }
        for (auto* list : { &roots, &types, &tensions, &basses })
            for (auto* key : *list)
            {
                styleKey (*key);
                addAndMakeVisible (key);
            }
    }

    void refresh (const std::optional<Chord>& chord, bool flats, bool enabled)
    {
        for (int i = 0; i < 12; ++i)
        {
            roots[i]->setButtonText (Chord::noteName (i, flats));
            basses[i]->setButtonText (Chord::noteName (i, flats));
            roots[i]->setToggleState (chord && chord->root == i, juce::dontSendNotification);
            basses[i]->setToggleState (chord && chord->bass == i, juce::dontSendNotification);
        }
        basses[12]->setToggleState (chord && chord->bass < 0, juce::dontSendNotification);
        for (int i = 0; i < types.size(); ++i)
            types[i]->setToggleState (chord && chord->type == Chord::allTypes()[(size_t) i], juce::dontSendNotification);
        for (int i = 0; i < tensions.size(); ++i)
            tensions[i]->setToggleState (chord && chord->tension == Chord::allTensions()[(size_t) i], juce::dontSendNotification);
        for (auto* list : { &roots, &types, &tensions, &basses })
            for (auto* key : *list)
                key->setEnabled (enabled);
        title = ! enabled ? "Select a pad" : chord ? chord->name (flats) : "A new chord: its root";
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff100c11));
        g.setColour (ParagonColours::panelEdge);
        g.fillRect (0, 0, 1, getHeight());
        auto area = getLocalBounds().reduced (10, 6);
        auto head = area.removeFromTop (18).toFloat();
        g.setColour (ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::titleFont (13.0f));
        g.drawText ("CHORD EDITOR", head.removeFromLeft (110.0f), juce::Justification::centredLeft, false);
        g.setColour (ParagonColours::text);
        g.setFont (chordFont (14.0f));
        g.drawText (title, head, juce::Justification::centredLeft, true);
        g.setColour (ParagonColours::textDim);
        g.setFont (font (10.0f));
        for (const auto& [label, y] : labels)
            g.drawText (label, juce::Rectangle<float> ((float) area.getX(), (float) y, 54.0f, rowHeight), juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10, 6);
        area.removeFromTop (22);
        labels.clear();
        const auto rows = 7.0f, gaps = 3.0f;
        rowHeight = juce::jlimit (14.0f, 26.0f, ((float) area.getHeight() - gaps * 6.0f) / rows);
        auto y = (float) area.getY();
        const auto x = area.getX() + 56;
        const auto width = (float) (area.getRight() - x);
        const auto row = [&] (juce::OwnedArray<juce::TextButton>& keys, int from, int count, int across)
        {
            const auto w = width / (float) across;
            for (int i = 0; i < count; ++i)
                keys[from + i]->setBounds (juce::Rectangle<float> ((float) x + w * (float) i, y, w - 2.0f, rowHeight).toNearestInt());
            y += rowHeight + gaps;
        };
        labels.push_back ({ "ROOT", (int) y });
        row (roots, 0, 6, 6);
        row (roots, 6, 6, 6);
        labels.push_back ({ "TYPE", (int) y });
        row (types, 0, 6, 6);
        labels.push_back ({ "TENSION", (int) y });
        row (tensions, 0, 7, 7);
        row (tensions, 7, 7, 7);
        labels.push_back ({ "BASS", (int) y });
        // "-" (none) first, then the twelve, over two rows of seven.
        const auto w = width / 7.0f;
        basses[12]->setBounds (juce::Rectangle<float> ((float) x, y, w - 2.0f, rowHeight).toNearestInt());
        for (int i = 0; i < 6; ++i)
            basses[i]->setBounds (juce::Rectangle<float> ((float) x + w * (float) (i + 1), y, w - 2.0f, rowHeight).toNearestInt());
        y += rowHeight + gaps;
        for (int i = 6; i < 12; ++i)
            basses[i]->setBounds (juce::Rectangle<float> ((float) x + w * (float) (i - 5), y, w - 2.0f, rowHeight).toNearestInt());
    }

private:
    ChordPadsView& view;
    juce::OwnedArray<juce::TextButton> roots, types, tensions, basses;
    std::vector<std::pair<juce::String, int>> labels;
    float rowHeight = 20.0f;
    juce::String title;
};

//==============================================================================
ChordPadsView::ChordPadsView (pr7s::Studio& s) : studio (s)
{
    setWantsKeyboardFocus (true);
    editor = std::make_unique<ChordEditor> (*this);
    addChildComponent (*editor);
    for (auto* key : { &rootKey, &presetsKey, &playerKey, &modeKey, &displayKey, &remoteKey })
        styleKey (*key, true);
    for (auto* key : { &previousPreset, &nextPreset, &saveKey, &editorKey, &windowKey, &closeKey, &selectAllKey, &editKey, &fromMidiKey,
                       &removeKey, &transposeDown, &transposeUp, &tensionsDown, &tensionsUp, &voicingPrevious, &voicingNext, &lockKey,
                       &adaptiveKey, &referenceKey })
        styleKey (*key);
    for (auto* key : { &rootKey, &presetsKey, &previousPreset, &nextPreset, &saveKey, &playerKey, &modeKey, &displayKey, &remoteKey, &editorKey,
                       &windowKey, &closeKey, &selectAllKey, &editKey, &fromMidiKey, &removeKey, &transposeDown, &transposeUp, &tensionsDown,
                       &tensionsUp, &voicingPrevious, &voicingNext, &lockKey, &adaptiveKey, &referenceKey })
        addAndMakeVisible (*key);

    rootKey.setTooltip ("The root key: the key the chords' functions are named in. Transpose Pads moves the chords to it; Flats spells them with flats");
    rootKey.onClick = [this] { showRootMenu(); };
    presetsKey.setTooltip ("Chord pads presets: the scales (in the root key) and your own; what to load (chords, the player, or both)");
    presetsKey.onClick = [this] { showPresetsMenu(); };
    previousPreset.setTooltip ("The previous preset");
    previousPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.setTooltip ("The next preset");
    nextPreset.onClick = [this] { stepPreset (1); };
    saveKey.setTooltip ("Save the pads as a preset (their chords and the player)");
    saveKey.onClick = [this] { savePreset(); };
    playerKey.setTooltip ("The player: how chords are voiced (the Basic player: close chords, its octave)");
    playerKey.onClick = [this] { showPlayerMenu(); };
    modeKey.setTooltip ("The player mode: plain chords, and what happens to a chord held when the next is played (Hold First, Legato, Stop First)");
    modeKey.onClick = [this] { showModeMenu(); };
    displayKey.setTooltip ("How the pads show their chords (note names, roman numerals, Nashville numbers) and their layout (keyboard or grid)");
    displayKey.onClick = [this] { showDisplayMenu(); };
    remoteKey.setTooltip ("The keys on your MIDI keyboard that play the pads (C1 to B1 at first), and Latch Chords");
    remoteKey.onClick = [this] { showRemoteMenu(); };
    editorKey.setTooltip ("The chord editor beside the pads: the selected pad's root, type, tension and bass");
    editorKey.onClick = [this] { setEditorShown (! editorShown); };
    windowKey.onClick = [this] { if (onWindowToggle) onWindowToggle(); };
    closeKey.onClick = [this] { if (onClose) onClose(); };

    selectAllKey.setTooltip ("Select all the pads (Ctrl+A)");
    selectAllKey.onClick = [this] { selectAll(); };
    editKey.setTooltip ("Edit the selected pad's chord (double-click it, or Return)");
    editKey.onClick = [this] { setEditorShown (true); };
    fromMidiKey.setTooltip ("Assign from MIDI Input: play a chord on your MIDI keyboard, and the selected pad takes it, voicing and all (then the next selected pad)");
    fromMidiKey.onClick = [this]
    {
        studio.setAssignFromInput (studio.getAssignFromInput().empty() ? selected : std::vector<int> {});
    };
    removeKey.setTooltip ("Remove the selected pads' chords (Delete). This can't be undone");
    removeKey.onClick = [this] { removeSelected (true); };
    transposeDown.setTooltip ("Transpose the selected pads down a semitone (Shift + the wheel over a pad)");
    transposeDown.onClick = [this] { applyToSelection ([this] (auto& pads) { studio.transposePads (pads, -1); }); };
    transposeUp.setTooltip ("Transpose the selected pads up a semitone");
    transposeUp.onClick = [this] { applyToSelection ([this] (auto& pads) { studio.transposePads (pads, 1); }); };
    tensionsDown.setTooltip ("Fewer tensions on the selected pads (Alt + the wheel over a pad)");
    tensionsDown.onClick = [this] { applyToSelection ([this] (auto& pads) { studio.changePadTensions (pads, -1); }); };
    tensionsUp.setTooltip ("More tensions on the selected pads");
    tensionsUp.onClick = [this] { applyToSelection ([this] (auto& pads) { studio.changePadTensions (pads, 1); }); };
    voicingPrevious.setTooltip ("The previous voicing (the wheel over a pad). A voicing chosen by hand turns the pad's adaptive voicing off");
    voicingPrevious.onClick = [this] { applyToSelection ([this] (auto& pads) { studio.changePadVoicings (pads, -1); }); };
    voicingNext.setTooltip ("The next voicing");
    voicingNext.onClick = [this] { applyToSelection ([this] (auto& pads) { studio.changePadVoicings (pads, 1); }); };
    lockKey.setTooltip ("Lock the selected pads: no editing, no changes from the keys or the wheel (Ctrl+Shift+L)");
    lockKey.onClick = [this]
    {
        if (selected.empty())
            return;
        studio.setPadsLocked (selected, ! studio.getChordPads().pads[(size_t) selected.front()].locked);
    };
    adaptiveKey.setTooltip ("Adaptive voicing: the pad's voicing chosen as it's played, so the voices move as little as they can from the chord before");
    adaptiveKey.onClick = [this]
    {
        if (selected.empty())
            return;
        studio.setPadsAdaptive (selected, ! studio.getChordPads().pads[(size_t) selected.front()].adaptive);
    };
    referenceKey.setTooltip ("Adaptive voicing reference: the other pads' voicings stay near this pad's (one pad at a time)");
    referenceKey.onClick = [this]
    {
        if (selected.empty())
            return;
        studio.setAdaptiveReference (studio.getChordPads().adaptiveReference == selected.front() ? -1 : selected.front());
    };

    studio.addChangeListener (this);
    refresh();
    startTimerHz (30);
}

ChordPadsView::~ChordPadsView()
{
    studio.removeChangeListener (this);
    if (pressedPad >= 0)
        studio.getPadPlayer().release (pressedPad);
}

void ChordPadsView::setInWindow (bool shouldBeInWindow)
{
    inWindow = shouldBeInWindow;
    windowKey.setButtonText (inWindow ? "DOCK" : "WINDOW");
    windowKey.setTooltip (inWindow ? "Back into the studio's lower zone" : "The chord pads in a window of their own");
}

void ChordPadsView::selectPads (std::vector<int> pads)
{
    const auto count = (int) studio.getChordPads().pads.size();
    pads.erase (std::remove_if (pads.begin(), pads.end(), [count] (int p) { return ! juce::isPositiveAndBelow (p, count); }), pads.end());
    selected = std::move (pads);
    refresh();
}

void ChordPadsView::selectAll()
{
    std::vector<int> all;
    for (int i = 0; i < (int) studio.getChordPads().pads.size(); ++i)
        all.push_back (i);
    selectPads (all);
}

void ChordPadsView::setEditorShown (bool shown)
{
    editorShown = shown;
    editor->setVisible (shown);
    resized();
    refresh();
}

void ChordPadsView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    selectPads (selected);   // (pads may have gone: fewer of them)
}

void ChordPadsView::refresh()
{
    const auto& pads = studio.getChordPads();
    rootKey.setButtonText ("ROOT: " + Chord::noteName (pads.rootKey, pads.flats));
    playerKey.setButtonText (pads.octaveOffset == 0 ? juce::String ("PLAYER: BASIC")
                                                    : "PLAYER: BASIC " + juce::String (pads.octaveOffset > 0 ? "+" : "") + juce::String (pads.octaveOffset));
    modeKey.setButtonText (pads.overlap == ChordPads::Overlap::holdFirst ? "PLAIN: HOLD FIRST"
                         : pads.overlap == ChordPads::Overlap::stopFirst ? "PLAIN: STOP FIRST" : "PLAIN: LEGATO");
    editorKey.setToggleState (editorShown, juce::dontSendNotification);
    fromMidiKey.setToggleState (! studio.getAssignFromInput().empty(), juce::dontSendNotification);
    const auto* first = selected.empty() ? nullptr : &pads.pads[(size_t) selected.front()];
    lockKey.setToggleState (first != nullptr && first->locked, juce::dontSendNotification);
    adaptiveKey.setToggleState (first != nullptr && first->adaptive, juce::dontSendNotification);
    referenceKey.setToggleState (first != nullptr && pads.adaptiveReference == selected.front(), juce::dontSendNotification);
    for (auto* key : { &editKey, &fromMidiKey, &removeKey, &transposeDown, &transposeUp, &tensionsDown, &tensionsUp, &voicingPrevious,
                       &voicingNext, &lockKey, &adaptiveKey, &referenceKey })
        key->setEnabled (! selected.empty() || key == &fromMidiKey);
    if (editor != nullptr)
        editor->refresh (first != nullptr ? first->chord : std::nullopt, pads.flats, first != nullptr && ! first->locked);
    repaint();
}

void ChordPadsView::audition (int pad)
{
    if (juce::isPositiveAndBelow (pad, (int) studio.getChordPads().pads.size()) && studio.getChordPads().pads[(size_t) pad].chord)
        studio.auditionPad (pad);
}

void ChordPadsView::applyToSelection (std::function<void (const std::vector<int>&)> action, bool listen)
{
    if (selected.empty())
        return;
    action (selected);
    if (listen)
        audition (selected.front());   // (the manual: the first selected pad is played)
}

//==============================================================================
void ChordPadsView::editRoot (int pitchClass)
{
    if (selected.empty())
        return;
    const auto& pad = studio.getChordPads().pads[(size_t) selected.front()];
    auto chord = pad.chord.value_or (Chord {});
    chord.root = pitchClass;
    studio.assignChord (selected.front(), chord);
    audition (selected.front());
}

void ChordPadsView::editType (Chord::Type type)
{
    if (selected.empty())
        return;
    auto chord = studio.getChordPads().pads[(size_t) selected.front()].chord.value_or (Chord {});
    chord.type = type;
    studio.assignChord (selected.front(), chord);
    audition (selected.front());
}

void ChordPadsView::editTension (Chord::Tension tension)
{
    if (selected.empty())
        return;
    auto chord = studio.getChordPads().pads[(size_t) selected.front()].chord.value_or (Chord {});
    chord.tension = tension;
    studio.assignChord (selected.front(), chord);
    audition (selected.front());
}

void ChordPadsView::editBass (int pitchClass)
{
    if (selected.empty())
        return;
    auto chord = studio.getChordPads().pads[(size_t) selected.front()].chord.value_or (Chord {});
    chord.bass = pitchClass;
    studio.assignChord (selected.front(), chord);
    audition (selected.front());
}

void ChordPadsView::removeSelected (bool ask)
{
    if (selected.empty())
        return;
    if (! ask)
    {
        studio.removeChords (selected);
        return;
    }
    // (The manual: this can't be undone, so it asks.)
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Remove chords",
                                        "Remove the chords from the selected pads? This can't be undone.", "Remove", "Cancel", nullptr,
                                        juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<ChordPadsView> (this)] (int result)
                                        {
                                            if (safe != nullptr && result == 1)
                                                safe->studio.removeChords (safe->selected);
                                        }));
}

//==============================================================================
void ChordPadsView::showRootMenu()
{
    const auto& pads = studio.getChordPads();
    juce::PopupMenu menu;
    menu.addSectionHeader ("ROOT KEY");
    for (int i = 0; i < 12; ++i)
        menu.addItem (1 + i, Chord::noteName (i, pads.flats), true, pads.rootKey == i);
    menu.addSeparator();
    menu.addItem (20, "Transpose Pads (the chords move to the key)", true, transposeOnRoot);
    menu.addItem (21, "Flats (Enh. Shift: Bb, not A#)", true, pads.flats);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&rootKey),
                        [safe = juce::Component::SafePointer<ChordPadsView> (this)] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        if (chosen == 20)
            safe->transposeOnRoot = ! safe->transposeOnRoot;
        else if (chosen == 21)
        {
            auto p = safe->studio.getChordPads();
            p.flats = ! p.flats;
            safe->studio.setChordPads (p);
        }
        else
            safe->studio.setPadsRootKey (chosen - 1, safe->transposeOnRoot);
        safe->refresh();
    });
}

juce::Array<juce::File> ChordPadsView::userPresets() const
{
    auto files = pr7s::Studio::chordPadsPresetFolder().findChildFiles (juce::File::findFiles, false, "*.pr7pads");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    return files;
}

void ChordPadsView::showPresetsMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("SCALES, IN THE ROOT KEY");
    const auto scales = ChordPads::scaleNames();
    for (int i = 0; i < scales.size(); ++i)
        menu.addItem (1 + i, scales[i], true, presetIndex == i);
    const auto mine = userPresets();
    if (! mine.isEmpty())
    {
        menu.addSeparator();
        menu.addSectionHeader ("YOUR PRESETS");
        for (int i = 0; i < mine.size(); ++i)
            menu.addItem (100 + i, mine[i].getFileNameWithoutExtension(), true, presetIndex == scales.size() + i);
    }
    menu.addSeparator();
    menu.addItem (201, "Load Chords & Players", true, loadWhat == pr7s::Studio::PadsPresetLoad::chordsAndPlayers);
    menu.addItem (202, "Load Chords Only", true, loadWhat == pr7s::Studio::PadsPresetLoad::chordsOnly);
    menu.addItem (203, "Load Players Only", true, loadWhat == pr7s::Studio::PadsPresetLoad::playersOnly);
    menu.addSeparator();
    menu.addItem (210, "Save Preset...");
    menu.addItem (211, "Show the Presets Folder");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetsKey),
                        [safe = juce::Component::SafePointer<ChordPadsView> (this), count = scales.size()] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        using L = pr7s::Studio::PadsPresetLoad;
        if (chosen >= 201 && chosen <= 203)
            safe->loadWhat = chosen == 201 ? L::chordsAndPlayers : chosen == 202 ? L::chordsOnly : L::playersOnly;
        else if (chosen == 210)
            safe->savePreset();
        else if (chosen == 211)
        {
            pr7s::Studio::chordPadsPresetFolder().createDirectory();
            pr7s::Studio::chordPadsPresetFolder().revealToUser();
        }
        else
        {
            safe->presetIndex = chosen >= 100 ? count + chosen - 100 : chosen - 1;
            safe->stepPreset (0);
        }
    });
}

void ChordPadsView::stepPreset (int direction)
{
    // The scales, then your presets, round and round.
    const auto scales = ChordPads::scaleNames().size();
    const auto mine = userPresets();
    const auto total = scales + mine.size();
    presetIndex = ((presetIndex + direction) % total + total) % total;
    if (presetIndex < scales)
        studio.loadScalePreset ((ChordPads::Scale) presetIndex, studio.getChordPads().rootKey, loadWhat);
    else
        studio.loadChordPadsPreset (mine[presetIndex - scales], loadWhat);
    presetsKey.setButtonText (presetIndex < scales ? ChordPads::scaleNames()[presetIndex].toUpperCase()
                                                   : mine[presetIndex - scales].getFileNameWithoutExtension().toUpperCase());
}

void ChordPadsView::savePreset()
{
    auto* box = new juce::AlertWindow ("Save chord pads preset", "Its name:", juce::MessageBoxIconType::NoIcon);
    box->addTextEditor ("name", "My chords");
    box->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    box->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    box->enterModalState (true, juce::ModalCallbackFunction::create ([box, safe = juce::Component::SafePointer<ChordPadsView> (this)] (int result)
    {
        if (safe == nullptr || result != 1)
            return;
        const auto name = juce::File::createLegalFileName (box->getTextEditorContents ("name").trim());
        if (name.isEmpty())
            return;
        safe->studio.saveChordPadsPreset (pr7s::Studio::chordPadsPresetFolder().getChildFile (name + ".pr7pads"));
    }), true);
}

void ChordPadsView::showPlayerMenu()
{
    const auto& pads = studio.getChordPads();
    juce::PopupMenu menu;
    menu.addSectionHeader ("BASIC PLAYER: CLOSE CHORDS");
    menu.addItem (99, "(Piano and guitar players: a later release)", false, false);
    menu.addSeparator();
    menu.addSectionHeader ("OCTAVE");
    for (int o = -2; o <= 2; ++o)
        menu.addItem (10 + o, o == 0 ? juce::String ("0") : (o > 0 ? "+" : "") + juce::String (o), true, pads.octaveOffset == o);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&playerKey),
                        [safe = juce::Component::SafePointer<ChordPadsView> (this)] (int chosen)
    {
        if (safe == nullptr || chosen < 8 || chosen > 12)
            return;
        auto p = safe->studio.getChordPads();
        p.octaveOffset = chosen - 10;
        safe->studio.setChordPads (p);
    });
}

void ChordPadsView::showModeMenu()
{
    const auto& pads = studio.getChordPads();
    juce::PopupMenu menu;
    menu.addSectionHeader ("PLAIN CHORDS: A CHORD PLAYED WHILE ANOTHER IS HELD");
    menu.addItem (1, "Hold First (the first goes on; common notes aren't played again)", true, pads.overlap == ChordPads::Overlap::holdFirst);
    menu.addItem (2, "Legato (the first ends, but its common notes go on)", true, pads.overlap == ChordPads::Overlap::legato);
    menu.addItem (3, "Stop First (the first ends, common notes too)", true, pads.overlap == ChordPads::Overlap::stopFirst);
    menu.addSeparator();
    menu.addItem (99, "(Pattern and Sections modes: a later release)", false, false);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&modeKey),
                        [safe = juce::Component::SafePointer<ChordPadsView> (this)] (int chosen)
    {
        if (safe == nullptr || chosen < 1 || chosen > 3)
            return;
        auto p = safe->studio.getChordPads();
        p.overlap = chosen == 1 ? ChordPads::Overlap::holdFirst : chosen == 2 ? ChordPads::Overlap::legato : ChordPads::Overlap::stopFirst;
        safe->studio.setChordPads (p);
    });
}

void ChordPadsView::setPadCount (int count)
{
    auto p = studio.getChordPads();
    count = juce::jlimit (1, 64, count);
    p.pads.resize ((size_t) count);
    p.triggerEnd = juce::jmin (127, p.triggerStart + count - 1);
    if (p.adaptiveReference >= count)
        p.adaptiveReference = -1;
    studio.setChordPads (p);
}

void ChordPadsView::showDisplayMenu()
{
    const auto& pads = studio.getChordPads();
    using D = ChordPads::Display;
    juce::PopupMenu primary, secondary, layout, start, grid;
    const std::pair<D, const char*> names[] = { { D::noteName, "Note Name" }, { D::roman, "Roman Numerals" }, { D::nashville, "Nashville Number System" } };
    for (const auto& [d, name] : names)
    {
        primary.addItem (10 + (int) d, name, true, pads.primary == d);
        secondary.addItem (20 + (int) d, name, true, pads.secondary == d);
    }
    secondary.addItem (20 + (int) D::none, "None", true, pads.secondary == D::none);
    layout.addItem (30, "Keyboard: 12 pads", true, pads.layout == ChordPads::Layout::keyboard && pads.pads.size() == 12);
    layout.addItem (31, "Keyboard: 24 pads", true, pads.layout == ChordPads::Layout::keyboard && pads.pads.size() == 24);
    const std::pair<int, int> grids[] = { { 2, 4 }, { 2, 6 }, { 2, 8 }, { 3, 4 }, { 4, 4 } };
    for (int i = 0; i < 5; ++i)
        layout.addItem (40 + i, "Grid: " + juce::String (grids[i].first) + " x " + juce::String (grids[i].second), true,
                        pads.layout == ChordPads::Layout::grid && pads.rows == grids[i].first && pads.columns == grids[i].second);
    for (int i = 0; i < 12; ++i)
        start.addItem (50 + i, Chord::noteName (i, pads.flats), true, pads.startNote == i);
    juce::PopupMenu menu;
    menu.addSectionHeader ("CHORD SYMBOLS");
    menu.addSubMenu ("Primary Type", primary);
    menu.addSubMenu ("Secondary Type", secondary);
    menu.addSeparator();
    menu.addSectionHeader ("PAD LAYOUT");
    menu.addSubMenu ("Type and Number of Pads", layout);
    menu.addSubMenu ("Start Note (keyboard)", start, pads.layout == ChordPads::Layout::keyboard);
    menu.addSeparator();
    menu.addItem (60, "Show Voicing Indicators", true, pads.showVoicing);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&displayKey),
                        [safe = juce::Component::SafePointer<ChordPadsView> (this)] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        auto p = safe->studio.getChordPads();
        const std::pair<int, int> gridSizes[] = { { 2, 4 }, { 2, 6 }, { 2, 8 }, { 3, 4 }, { 4, 4 } };
        if (chosen >= 10 && chosen < 20)      p.primary = (D) (chosen - 10);
        else if (chosen >= 20 && chosen < 30) p.secondary = (D) (chosen - 20);
        else if (chosen == 30 || chosen == 31)
        {
            p.layout = ChordPads::Layout::keyboard;
            safe->studio.setChordPads (p);
            safe->setPadCount (chosen == 30 ? 12 : 24);
            return;
        }
        else if (chosen >= 40 && chosen < 45)
        {
            p.layout = ChordPads::Layout::grid;
            p.rows = gridSizes[chosen - 40].first;
            p.columns = gridSizes[chosen - 40].second;
            safe->studio.setChordPads (p);
            safe->setPadCount (p.rows * p.columns);
            return;
        }
        else if (chosen >= 50 && chosen < 62) p.startNote = chosen - 50;
        else if (chosen == 60)                p.showVoicing = ! p.showVoicing;
        safe->studio.setChordPads (p);
    });
}

void ChordPadsView::showRemoteMenu()
{
    const auto& pads = studio.getChordPads();
    juce::PopupMenu menu;
    menu.addSectionHeader ("THE KEYS THAT PLAY THE PADS");
    for (int octave = -1; octave <= 4; ++octave)
    {
        const auto first = (octave + 2) * 12;
        menu.addItem (10 + octave + 1, keyName (first, pads.flats) + " to " + keyName (first + (int) pads.pads.size() - 1, pads.flats), true,
                      pads.triggerStart == first);
    }
    menu.addSeparator();
    menu.addItem (30, "Latch Chords (a pad plays until it's played again)", true, pads.latch);
    menu.addItem (99, "(They work while the chord pads are open. MIDI Learn and the modifiers: a later release)", false, false);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&remoteKey),
                        [safe = juce::Component::SafePointer<ChordPadsView> (this)] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        auto p = safe->studio.getChordPads();
        if (chosen == 30)
            p.latch = ! p.latch;
        else if (chosen >= 10 && chosen <= 15)
        {
            p.triggerStart = (chosen - 11 + 2) * 12;
            p.triggerEnd = juce::jmin (127, p.triggerStart + (int) p.pads.size() - 1);
        }
        safe->studio.setChordPads (p);
    });
}

void ChordPadsView::showPadMenu (int pad)
{
    const auto& p = studio.getChordPads().pads[(size_t) pad];
    juce::PopupMenu menu;
    menu.addItem (1, "Edit Chord...", ! p.locked);
    menu.addItem (2, "Assign from MIDI Input", ! p.locked);
    menu.addItem (3, "Remove Chord", p.chord.has_value() && ! p.locked);
    menu.addSeparator();
    menu.addItem (4, "Lock", true, p.locked);
    menu.addItem (5, "Adaptive Voicing", ! p.locked, p.adaptive);
    menu.addItem (6, "Adaptive Voicing Reference", p.chord.has_value(), studio.getChordPads().adaptiveReference == pad);
    menu.addSeparator();
    menu.addItem (7, "Transpose Up", p.chord.has_value() && ! p.locked);
    menu.addItem (8, "Transpose Down", p.chord.has_value() && ! p.locked);
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<ChordPadsView> (this), pad] (int chosen)
    {
        if (safe == nullptr)
            return;
        auto& s = safe->studio;
        switch (chosen)
        {
            case 1: safe->setEditorShown (true); break;
            case 2: s.setAssignFromInput ({ pad }); break;
            case 3: safe->removeSelected (true); break;
            case 4: s.setPadsLocked ({ pad }, ! s.getChordPads().pads[(size_t) pad].locked); break;
            case 5: s.setPadsAdaptive ({ pad }, ! s.getChordPads().pads[(size_t) pad].adaptive); break;
            case 6: s.setAdaptiveReference (s.getChordPads().adaptiveReference == pad ? -1 : pad); break;
            case 7: s.transposePads ({ pad }, 1); safe->audition (pad); break;
            case 8: s.transposePads ({ pad }, -1); safe->audition (pad); break;
            default: break;
        }
    });
}

//==============================================================================
juce::Rectangle<float> ChordPadsView::getPadBounds (int pad) const
{
    const auto& pads = studio.getChordPads();
    const auto count = (int) pads.pads.size();
    if (! juce::isPositiveAndBelow (pad, count) || padsArea.isEmpty())
        return {};
    const auto area = padsArea.toFloat();
    if (pads.layout == ChordPads::Layout::grid)
    {
        // As a pad controller's: the first at the bottom left.
        const auto cols = juce::jmax (1, pads.columns), rows = juce::jmax (1, (count + cols - 1) / cols);
        const auto w = area.getWidth() / (float) cols, h = area.getHeight() / (float) rows;
        const auto row = rows - 1 - pad / cols, col = pad % cols;
        return { area.getX() + w * (float) col, area.getY() + h * (float) row, w, h };
    }
    // Keyboard: white keys' pads along the bottom, black keys' above, between them.
    auto whites = 0;
    for (int i = 0; i < count; ++i)
        whites += isBlack (pads.startNote + i) ? 0 : 1;
    const auto w = area.getWidth() / (float) juce::jmax (1, whites), h = area.getHeight() * 0.5f;
    auto white = 0;
    for (int i = 0; i < pad; ++i)
        white += isBlack (pads.startNote + i) ? 0 : 1;
    if (isBlack (pads.startNote + pad))
        return { juce::jlimit (area.getX(), area.getRight() - w, area.getX() + w * (float) white - w * 0.5f), area.getY(), w, h };
    return { area.getX() + w * (float) white, area.getY() + h, w, h };
}

int ChordPadsView::padAt (juce::Point<float> at) const
{
    for (int i = 0; i < (int) studio.getChordPads().pads.size(); ++i)
        if (getPadBounds (i).reduced (2.0f).contains (at))
            return i;
    return -1;
}

void ChordPadsView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0a0e));
    for (const auto& strip : { bar, extendedBar })
    {
        g.setColour (juce::Colour (0xff100c11));
        g.fillRect (strip);
        g.setColour (ParagonColours::panelEdge);
        g.fillRect (strip.withY (strip.getBottom() - 1).withHeight (1));
    }
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (9.5f));
    g.drawText ("CHORD PADS", bar.withTrimmedLeft (14).toFloat(), juce::Justification::centredLeft, false);

    const auto count = (int) studio.getChordPads().pads.size();
    for (int i = 0; i < count; ++i)
        paintPad (g, i, getPadBounds (i));
    paintKeyboard (g, keysArea.toFloat());
}

void ChordPadsView::paintPad (juce::Graphics& g, int index, juce::Rectangle<float> r)
{
    const auto& pads = studio.getChordPads();
    const auto& pad = pads.pads[(size_t) index];
    r = r.reduced (3.0f);
    const auto pressed = index < (int) shownPressed.size() && shownPressed[(size_t) index];
    if (pressed)
        g.setGradientFill (juce::ColourGradient (ParagonColours::meterColour (0.0f), r.getX(), r.getBottom(),
                                                 ParagonColours::meterColour (1.0f), r.getX(), r.getY(), false));
    else
        g.setColour (pad.chord ? padBody : padEmpty);
    g.fillRoundedRectangle (r, 5.0f);

    const auto isSelected = std::find (selected.begin(), selected.end(), index) != selected.end();
    const auto& targets = studio.getAssignFromInput();
    const auto isInputTarget = ! targets.empty() && targets.front() == index;
    if (index == dropPad)                      { g.setColour (ParagonColours::accent); g.drawRoundedRectangle (r, 5.0f, 2.5f); }
    else if (isInputTarget)                    { g.setColour (juce::Colour (0xffd42e2a)); g.drawRoundedRectangle (r, 5.0f, 2.5f); }
    else if (isSelected)                       { g.setColour (pearl); g.drawRoundedRectangle (r, 5.0f, 2.0f); }
    else if (pads.adaptiveReference == index)  { g.setColour (ParagonColours::accent); g.drawRoundedRectangle (r, 5.0f, 2.0f); }
    else                                       { g.setColour (ParagonColours::panelEdge); g.drawRoundedRectangle (r, 5.0f, 1.0f); }

    const auto ink = pressed ? juce::Colour (0xff120d10) : ParagonColours::text;
    const auto dim = pressed ? juce::Colour (0xff3a2a26) : ParagonColours::textDim;
    auto inside = r.reduced (6.0f, 4.0f);
    auto top = inside.removeFromTop (14.0f);
    auto bottom = inside.removeFromBottom (14.0f);
    g.setFont (font (10.0f));
    // The top corners: the reference, and adaptive voicing (left); the lock (right).
    if (pads.adaptiveReference == index)
    {
        g.setColour (pressed ? ink : ParagonColours::accent);
        g.drawText ("REF", top, juce::Justification::centredLeft, false);
    }
    if (pad.locked)
        drawLock (g, top.removeFromRight (10.0f).withSizeKeepingCentre (9.0f, 12.0f), dim);
    else if (pad.chord && pad.adaptive)
    {
        g.setColour (dim);
        g.drawText ("A", top, juce::Justification::centredRight, false);
    }
    // The trigger key at the bottom right; the voicing at the bottom left.
    g.setColour (dim);
    const auto key = pads.triggerStart + index;
    if (key <= pads.triggerEnd)
        g.drawText (keyName (key, pads.flats), bottom, juce::Justification::centredRight, false);
    if (pad.chord && pads.showVoicing && r.getHeight() > 56.0f)
    {
        const auto at = studio.getPadVoicing (index), of = studio.getPadVoicingCount (index);
        g.drawText (at >= 0 ? "V " + juce::String (at + 1) + "/" + juce::String (of) : juce::String ("V PLAYED"), bottom, juce::Justification::centredLeft, false);
    }
    // The chord: large, and the second way of naming it under it.
    if (! pad.chord)
    {
        g.setColour (dim.withAlpha (0.6f));
        g.setFont (ParagonLookAndFeel::titleFont (18.0f));
        g.drawText ("+", inside, juce::Justification::centred, false);
        return;
    }
    const auto primary = pads.displayName (*pad.chord, pads.primary);
    const auto secondary = pads.secondary == pads.primary ? juce::String() : pads.displayName (*pad.chord, pads.secondary);
    const auto big = juce::jlimit (12.0f, 30.0f, inside.getHeight() * 0.42f);
    auto nameArea = secondary.isNotEmpty() && inside.getHeight() > 40.0f ? inside.removeFromTop (inside.getHeight() * 0.66f) : inside;
    g.setColour (ink);
    g.setFont (chordFont (big * 0.85f));
    g.drawFittedText (primary, nameArea.toNearestInt(), juce::Justification::centredBottom, 1, 0.6f);
    if (secondary.isNotEmpty() && inside.getHeight() > 8.0f && nameArea != inside)
    {
        g.setColour (dim);
        g.setFont (chordFont (juce::jmin (12.0f, big * 0.45f)));
        g.drawFittedText (secondary, inside.toNearestInt(), juce::Justification::centredTop, 1, 0.6f);
    }
}

void ChordPadsView::paintKeyboard (juce::Graphics& g, juce::Rectangle<float> area)
{
    // From C0 to C6: what's sounding lit, the pads' trigger keys tinted, the key
    // of the pad under the mouse outlined (the manual's chord pads zone keyboard).
    const auto& pads = studio.getChordPads();
    constexpr int low = 24, high = 96;
    g.setColour (juce::Colour (0xff0a080b));
    g.fillRect (area);
    area = area.reduced (4.0f, 3.0f);
    auto whites = 0;
    for (int n = low; n <= high; ++n)
        whites += isBlack (n) ? 0 : 1;
    const auto w = area.getWidth() / (float) whites;
    const auto sounding = [this] (int n) { return std::find (shownSounding.begin(), shownSounding.end(), n) != shownSounding.end(); };
    const auto trigger = [&pads] (int n) { return n >= pads.triggerStart && n <= pads.triggerEnd && n - pads.triggerStart < (int) pads.pads.size(); };
    const auto hoverKey = hoverPad >= 0 ? pads.triggerStart + hoverPad : -1;
    std::vector<std::pair<int, juce::Rectangle<float>>> blacks;
    auto x = area.getX();
    for (int n = low; n <= high; ++n)
    {
        if (isBlack (n))
        {
            blacks.push_back ({ n, { x - w * 0.32f, area.getY(), w * 0.64f, area.getHeight() * 0.6f } });
            continue;
        }
        const juce::Rectangle<float> key (x, area.getY(), w - 1.0f, area.getHeight());
        g.setColour (sounding (n) ? ParagonColours::accent : trigger (n) ? juce::Colour (0xffe8b0b8) : juce::Colour (0xffd8d2d4));
        g.fillRect (key);
        if (n == hoverKey)
        {
            g.setColour (juce::Colour (0xffde0639));
            g.drawRect (key, 2.0f);
        }
        if (n % 12 == 0)
        {
            g.setColour (ParagonColours::ink);
            g.setFont (font (8.0f));
            g.drawText (keyName (n, false), key.withTop (key.getBottom() - 11.0f), juce::Justification::centred, false);
        }
        x += w;
    }
    for (const auto& [n, key] : blacks)
    {
        g.setColour (sounding (n) ? ParagonColours::accent : trigger (n) ? juce::Colour (0xff8a2a44) : juce::Colour (0xff1a1619));
        g.fillRect (key);
        if (n == hoverKey)
        {
            g.setColour (juce::Colour (0xffde0639));
            g.drawRect (key, 2.0f);
        }
    }
}

void ChordPadsView::resized()
{
    auto area = getLocalBounds();
    bar = area.removeFromTop (toolbarHeight);
    extendedBar = area.removeFromTop (toolbarHeight);
    keysArea = area.removeFromBottom (keyboardHeight);
    if (editorShown)
        editor->setBounds (area.removeFromRight (juce::jmin (editorWidth, area.getWidth() / 2)));
    padsArea = area.reduced (6, 4);

    auto b = bar.reduced (6, 4);
    b.removeFromLeft (90);
    closeKey.setBounds (b.removeFromRight (64));
    b.removeFromRight (6);
    windowKey.setBounds (b.removeFromRight (74));
    b.removeFromRight (12);
    const auto place = [&b] (juce::TextButton& key, int width, int gap = 4)
    {
        key.setBounds (b.removeFromLeft (width));
        b.removeFromLeft (gap);
    };
    place (rootKey, 90, 10);
    place (presetsKey, 170);
    place (previousPreset, 26, 2);
    place (nextPreset, 26);
    place (saveKey, 56, 10);
    place (playerKey, 140);
    place (modeKey, 150, 10);
    place (displayKey, 90);
    place (remoteKey, 86, 10);
    place (editorKey, 74);

    auto e = extendedBar.reduced (6, 4);
    const auto placeE = [&e] (juce::TextButton& key, int width, int gap = 4)
    {
        key.setBounds (e.removeFromLeft (width));
        e.removeFromLeft (gap);
    };
    placeE (selectAllKey, 84, 10);
    placeE (editKey, 86);
    placeE (fromMidiKey, 84);
    placeE (removeKey, 74, 10);
    placeE (transposeDown, 96, 2);
    placeE (transposeUp, 28, 8);
    placeE (tensionsDown, 90, 2);
    placeE (tensionsUp, 28, 8);
    placeE (voicingPrevious, 84, 2);
    placeE (voicingNext, 28, 10);
    placeE (lockKey, 56);
    placeE (adaptiveKey, 84);
    placeE (referenceKey, 90);
}

//==============================================================================
void ChordPadsView::mouseDown (const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    const auto pad = padAt (event.position);
    dragStarted = false;
    if (pad < 0)
    {
        if (! event.mods.isCommandDown() && ! event.mods.isShiftDown() && padsArea.contains (event.getPosition()))
            selectPads ({});
        return;
    }
    if (event.mods.isPopupMenu())
    {
        if (std::find (selected.begin(), selected.end(), pad) == selected.end())
            selectPads ({ pad });
        showPadMenu (pad);
        return;
    }
    if (event.mods.isCommandDown())
    {
        auto now = selected;
        if (const auto it = std::find (now.begin(), now.end(), pad); it != now.end())
            now.erase (it);
        else
            now.insert (now.begin(), pad);   // (the newest first: the modifiers start from it)
        selectPads (now);
    }
    else if (event.mods.isShiftDown() && ! selected.empty())
    {
        std::vector<int> run;
        const auto from = selected.front();
        for (int i = juce::jmin (from, pad); i <= juce::jmax (from, pad); ++i)
            run.push_back (i);
        std::stable_partition (run.begin(), run.end(), [from] (int i) { return i == from; });
        selectPads (run);
    }
    else
        selectPads ({ pad });
    // Clicked: played, as long as it's held.
    pressedPad = pad;
    studio.getPadPlayer().press (pad, 100);
}

void ChordPadsView::mouseDrag (const juce::MouseEvent& event)
{
    if (pressedPad < 0 || dragStarted || event.getDistanceFromDragStart() < 6
        || ! studio.getChordPads().pads[(size_t) pressedPad].chord)
        return;
    // Dragged: onto another pad (swap; Alt: copy) or onto a track (a MIDI part).
    dragStarted = true;
    studio.getPadPlayer().release (pressedPad);
    const auto r = getPadBounds (pressedPad).toNearestInt();
    juce::ScaledImage image (createComponentSnapshot (r, true).convertedToFormat (juce::Image::ARGB));
    image.getImage().multiplyAllAlphas (0.75f);
    startDragging (juce::String (dragPrefix) + juce::String (pressedPad) + (event.mods.isAltDown() ? ":copy" : ""), this, image, true);
}

void ChordPadsView::mouseUp (const juce::MouseEvent&)
{
    if (pressedPad >= 0 && ! dragStarted)
        studio.getPadPlayer().release (pressedPad);
    pressedPad = -1;
    dragStarted = false;
}

void ChordPadsView::mouseMove (const juce::MouseEvent& event)
{
    const auto pad = padAt (event.position);
    if (pad != hoverPad)
    {
        hoverPad = pad;
        repaint (keysArea);
    }
    if (pad >= 0)
        if (const auto& chord = studio.getChordPads().pads[(size_t) pad].chord)
            setTooltip (chord->name (studio.getChordPads().flats));
}

void ChordPadsView::mouseExit (const juce::MouseEvent&)
{
    hoverPad = -1;
    repaint (keysArea);
}

void ChordPadsView::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (const auto pad = padAt (event.position); pad >= 0)
    {
        selectPads ({ pad });
        setEditorShown (true);
    }
}

void ChordPadsView::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto pad = padAt (event.position);
    if (pad < 0 || ! studio.getChordPads().pads[(size_t) pad].chord)
        return;
    const auto delta = std::abs (wheel.deltaY) > 0.0f ? wheel.deltaY : wheel.deltaX;
    if (std::abs (delta) < 1.0e-4f)
        return;
    const auto up = delta > 0.0f;
    // (The manual: the wheel over a pad: Shift transposes, Alt changes tensions, alone: up is the previous voicing.)
    if (event.mods.isShiftDown())
        studio.transposePads ({ pad }, up ? 1 : -1);
    else if (event.mods.isAltDown())
        studio.changePadTensions ({ pad }, up ? 1 : -1);
    else
        studio.changePadVoicings ({ pad }, up ? -1 : 1);
    audition (pad);
}

bool ChordPadsView::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        removeSelected (true);
        return true;
    }
    if (mods.isCommandDown() && ! mods.isShiftDown() && (key.getKeyCode() == 'A' || key.getKeyCode() == 'a'))
    {
        selectAll();
        return true;
    }
    if (key == juce::KeyPress::returnKey)
    {
        setEditorShown (true);
        return true;
    }
    if (mods.isCommandDown() && mods.isShiftDown() && (key.getKeyCode() == 'L' || key.getKeyCode() == 'l'))
    {
        lockKey.onClick();
        return true;
    }
    return false;   // (the studio's keys: Space and the rest)
}

//==============================================================================
bool ChordPadsView::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description.toString().startsWith (dragPrefix);
}

void ChordPadsView::itemDragEnter (const SourceDetails& details) { itemDragMove (details); }

void ChordPadsView::itemDragMove (const SourceDetails& details)
{
    const auto pad = padAt (details.localPosition.toFloat());
    if (pad != dropPad)
    {
        dropPad = pad;
        repaint();
    }
}

void ChordPadsView::itemDragExit (const SourceDetails&)
{
    dropPad = -1;
    repaint();
}

void ChordPadsView::itemDropped (const SourceDetails& details)
{
    const auto text = details.description.toString().fromFirstOccurrenceOf (dragPrefix, false, false);
    const auto from = text.getIntValue();
    const auto copy = text.endsWith (":copy");
    const auto to = padAt (details.localPosition.toFloat());
    dropPad = -1;
    if (to >= 0 && to != from)
    {
        copy ? studio.copyPad (from, to) : studio.swapPads (from, to);
        selectPads ({ to });
    }
    repaint();
}

//==============================================================================
void ChordPadsView::timerCallback()
{
    // What's sounding and which pads are down: drawn only when it changes.
    const auto sounding = studio.getPadPlayer().getSounding();
    std::vector<bool> pressed (studio.getChordPads().pads.size());
    for (size_t i = 0; i < pressed.size(); ++i)
        pressed[i] = studio.getPadPlayer().isPressed ((int) i);
    if (sounding != shownSounding)
    {
        shownSounding = sounding;
        repaint (keysArea);
    }
    if (pressed != shownPressed)
    {
        shownPressed = pressed;
        repaint (padsArea.expanded (4));
    }
}
