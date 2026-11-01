#pragma once

#include "engine/Studio.h"

// The chord pads zone (0.14.0, the owner asked: as the other DAW has it; the
// manual's "Using Chord Pads"), in the lower zone beside the MIDI editor and
// the mixer, or in a window of its own.
//
// The toolbar (the manual's): ROOT (the Root Key pane: the key, Transpose
// Pads, Enh. Shift), PRESETS (the scale presets and your own; Load Chords &
// Players / Chords Only / Players Only), < > (the previous / next preset),
// SAVE, PLAYER (the Basic player: its octave), PLAIN CHORDS (the overlaps:
// Hold First, Legato, Stop First), DISPLAY (chord symbols, the pad layout),
// REMOTE (the trigger keys, Latch Chords), EDITOR (the chord editor at the
// right). The extended toolbar: SELECT ALL, EDIT CHORD, FROM MIDI (Assign
// from MIDI Input), REMOVE, TRANSPOSE - +, TENSIONS - +, VOICING < >, LOCK,
// ADAPTIVE, REFERENCE. Then the pads (keyboard or grid layout) and the
// keyboard under them (the notes played, the trigger keys).
//
// A pad: click plays it (and selects it; Ctrl adds, Shift a run), the wheel
// changes its voicing (Shift: transposes, Alt: tensions), a double-click (or
// Return) edits its chord, a drag onto another pad swaps them (Alt: copies),
// onto a track in the arrangement makes a one-bar MIDI part.
class ChordPadsView final : public juce::Component,
                            public juce::DragAndDropContainer,
                            public juce::DragAndDropTarget,
                            public juce::SettableTooltipClient,
                            private juce::ChangeListener,
                            private juce::Timer
{
public:
    explicit ChordPadsView (pr7s::Studio&);
    ~ChordPadsView() override;

    std::function<void()> onClose, onWindowToggle;
    void setInWindow (bool);
    int getMinimumHeight() const noexcept { return toolbarHeight * 2 + 120 + keyboardHeight; }

    // Selection (pad indices, the first is the one the editor and modifiers start from).
    const std::vector<int>& getSelection() const noexcept { return selected; }
    void selectPads (std::vector<int>);
    void selectAll();
    void setEditorShown (bool);
    bool isEditorShown() const noexcept { return editorShown; }
    // Where a pad is drawn (tests).
    juce::Rectangle<float> getPadBounds (int pad) const;
    int padAt (juce::Point<float>) const;
    // The chord editor's choices applied to the selected pad (tests drive them).
    void editRoot (int pitchClass);
    void editType (pr7s::Chord::Type);
    void editTension (pr7s::Chord::Tension);
    void editBass (int pitchClass);   // -1: none
    void removeSelected (bool ask);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    static constexpr int toolbarHeight = 32, keyboardHeight = 42, editorWidth = 380;
    static constexpr const char* dragPrefix = "chordpad:";

private:
    class ChordEditor;
    pr7s::Studio& studio;
    std::vector<int> selected;
    bool editorShown = false, inWindow = false, transposeOnRoot = false;
    int pressedPad = -1, hoverPad = -1, dropPad = -1, presetIndex = -1;
    bool dragStarted = false;
    pr7s::Studio::PadsPresetLoad loadWhat = pr7s::Studio::PadsPresetLoad::chordsAndPlayers;
    std::vector<int> shownSounding;
    std::vector<bool> shownPressed;
    juce::Rectangle<int> padsArea, keysArea, bar, extendedBar;
    std::unique_ptr<ChordEditor> editor;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::TextButton rootKey, presetsKey { "PRESETS" }, previousPreset { "<" }, nextPreset { ">" }, saveKey { "SAVE" },
                     playerKey { "PLAYER: BASIC" }, modeKey { "PLAIN CHORDS" }, displayKey { "DISPLAY" }, remoteKey { "REMOTE" },
                     editorKey { "EDITOR" }, windowKey { "WINDOW" }, closeKey { "CLOSE" };
    juce::TextButton selectAllKey { "SELECT ALL" }, editKey { "EDIT CHORD" }, fromMidiKey { "FROM MIDI" }, removeKey { "REMOVE" },
                     transposeDown { "TRANSPOSE -" }, transposeUp { "+" }, tensionsDown { "TENSIONS -" }, tensionsUp { "+" },
                     voicingPrevious { "VOICING <" }, voicingNext { ">" }, lockKey { "LOCK" }, adaptiveKey { "ADAPTIVE" }, referenceKey { "REFERENCE" };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refresh();
    void audition (int pad);
    void applyToSelection (std::function<void (const std::vector<int>&)>, bool listen = true);
    void showRootMenu();
    void showPresetsMenu();
    void stepPreset (int direction);
    void savePreset();
    void showPlayerMenu();
    void showModeMenu();
    void showDisplayMenu();
    void showRemoteMenu();
    void showPadMenu (int pad);
    void setPadCount (int count);
    juce::Array<juce::File> userPresets() const;
    void paintPad (juce::Graphics&, int pad, juce::Rectangle<float>);
    void paintKeyboard (juce::Graphics&, juce::Rectangle<float>);
};
