#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// The VariAudio editor (0.34.0, the manual's VariAudio Inspector section and its event display:
// our window for an audio clip, opened by double-clicking it or from its menu).
//
//   The panel   EDIT VARIAUDIO (the analysis: the clip's recording split into segments), BYPASS
//               (the manual's VariAudio Changes Bypass), RESET; PITCH SNAP: ABSOLUTE / RELATIVE /
//               OFF; CORRECT PITCH, STRAIGHTEN CURVE, VOLUME (the selected segments); the Scale
//               Assistant: EDITOR SCALE or CHORD TRACK, the scale, SUGGEST, SNAP TO SCALE (the
//               manual's Snap Pitch Editing), SCALE GUIDES, QUANTIZE PITCHES; AUDITION.
//   The display A piano keyboard at the left (Ctrl-click a key: every segment on it); each segment
//               a block at its pitch with its pitch curve, the waveform faint behind. Click to
//               select (Shift/Ctrl: add), drag a box; drag a segment up or down (Shift: free, Ctrl:
//               absolute, Alt: relative); its smart controls: straighten (top middle) and correct
//               pitch (bottom middle). Up/Down move (Shift: a cent), Left/Right go through the
//               segments, Ctrl+A, H/G zoom, the wheel scrolls (Shift: in time; Ctrl: zoom).
// (0.35.0, part two) Smart controls in two sets (the manual's Show Default / Show All Smart
// Controls): by default straighten (above the middle), correct pitch (below it), glue (below
// the corners: to the one before / after), split (the bottom edge: click; or Alt-click the
// segment), warp start / end (the lower half of its sides: drag in time; Alt: move the segment's
// edge instead). All adds tilt (above the corners; Alt: rotate round the anchor), the anchor
// (on the top edge), the straighten range (on the top edge's ends), formant and volume (above, a
// quarter in). The panel: FUNCTIONS (Extract MIDI, Generate Harmony Voices, the resets,
// Reanalyze), FORMANT, SMART CONTROLS DEFAULT / ALL, MIDI INPUT / STEP, the MIDI REFERENCE track,
// the segment COLOURS. Delete removes the selected segments.

// The Extract MIDI dialog (the manual's): pitch (notes; static or continuous pitchbend and its
// range), volume (fixed or dynamic velocity, or a controller curve), where it goes.
class ExtractMidiPage final : public juce::Component
{
public:
    ExtractMidiPage();
    void resized() override;
    void paint (juce::Graphics&) override;
    pr7s::ExtractMidiOptions getOptions() const;
    pr7s::Studio::ExtractDestination getDestination() const;
    std::function<void()> onOk, onCancel;

private:
    juce::ComboBox pitch, volume, destination;
    juce::Slider range, velocity, controller;
    juce::Label pitchLabel, rangeLabel, volumeLabel, velocityLabel, controllerLabel, destinationLabel;
    juce::TextButton ok { "EXTRACT MIDI" }, cancel { "CANCEL" };
};

// The Generate Harmony Voices dialog (the manual's): how many voices (1-4), the vibrato
// reduction, the editor opened on them after.
class HarmonyVoicesPage final : public juce::Component
{
public:
    HarmonyVoicesPage();
    void resized() override;
    void paint (juce::Graphics&) override;
    int getVoices() const { return voices.getSelectedId(); }
    float getVibratoReduction() const { return (float) vibrato.getValue() / 100.0f; }
    bool getOpenAfter() const { return openAfter.getToggleState(); }
    std::function<void()> onOk, onCancel;
    // The dialog in a call-out over `target`; `done` gets the new tracks' first clip (0: none) and any error.
    static void show (pr7s::Studio&, int partId, juce::Component& target, std::function<void (int firstPart, juce::String error)> done);

private:
    juce::ComboBox voices;
    juce::Slider vibrato;
    juce::ToggleButton openAfter { "Open the VariAudio editor on them after" };
    juce::Label voicesLabel, vibratoLabel, note;
    juce::TextButton ok { "GENERATE" }, cancel { "CANCEL" };
};

class VariAudioEditor final : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    VariAudioEditor (pr7s::Studio&, int partId);
    ~VariAudioEditor() override;

    void setPart (int partId);
    int getPartId() const noexcept { return partId; }

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

    std::function<bool (const juce::KeyPress&)> onOtherKey;   // (undo, transport...: the studio's keys)

    // What the panel's keys do (the tests drive these).
    bool analyse();
    void setSnap (pr7s::PitchSnap);
    pr7s::PitchSnap getSnap() const noexcept { return snap; }
    void selectSegments (std::vector<int>);
    const std::vector<int>& getSelection() const noexcept { return selection; }
    void moveSelected (float semitones, pr7s::PitchSnap);
    void correctSelected (float amount);
    void straightenSelected (float amount);
    void setVolumeOfSelected (float db);
    void setEditorScale (const pr7s::Key&);
    void setUseChordTrack (bool);
    void setSnapToScale (bool);
    void setScaleGuides (bool);
    void quantizeSelected();
    std::vector<pr7s::Key> getScaleSuggestions() const;
    pr7s::Key scaleAt (double songBeat) const;
    void auditionSelected();
    juce::String getInfoText() const;
    juce::TextButton* keyNamed (const juce::String&);

    // Part two (0.35.0).
    enum class SmartControl { none, straighten, correct, warpStart, warpEnd, glueLeft, glueRight, split,
                              tiltLeft, tiltRight, anchor, rangeFrom, rangeTo, formant, volume };
    enum class Colours { automatic, event, pitch, scale };   // the manual's Auto, Event, Pitch, Scale/Chords
    void setAllSmartControls (bool);
    bool getAllSmartControls() const noexcept { return allSmartControls; }
    juce::Rectangle<float> getSmartControl (int index, SmartControl) const;   // empty: not shown
    SmartControl smartControlAt (int index, juce::Point<float>) const;
    bool splitAt (int index, double seconds);
    bool glue (int index, bool withNext);          // the selection when it holds `index` and others; else with the neighbour
    bool deleteSelected();
    void setFormantOfSelected (float percent);
    void resetSelected (pr7s::Studio::VariReset);
    bool reanalyse();
    int extractMidi (const pr7s::ExtractMidiOptions&, pr7s::Studio::ExtractDestination, juce::String& error);
    void setMidiInput (bool);
    bool getMidiInput() const noexcept { return midiInput; }
    void setStepMode (bool);
    bool getStepMode() const noexcept { return stepMode; }
    void notePlayed (int note);                    // MIDI Input: the selected segments to this note (Step: then the next)
    void setReferenceTrack (int trackId);
    void setColours (Colours);
    Colours getColours() const noexcept { return colours; }
    juce::Colour colourOf (int index) const;
    double secondsForX (float x) const;            // the recording's moment under a place in the display
    float xForBeat (double songBeat) const;
    std::function<void (int partId)> onOpenPart;   // (after harmony voices: the editor on the first)

    // The display's mapping (for the mouse and the tests).
    juce::Rectangle<float> getSegmentBounds (int index) const;
    juce::Rectangle<float> getStraightenHandle (int index) const;
    juce::Rectangle<float> getCorrectHandle (int index) const;
    juce::Rectangle<int> getDisplayArea() const;
    juce::Rectangle<int> getKeyboardArea() const;
    float yForPitch (float pitch) const;
    float pitchForY (float y) const;
    float xForSeconds (double seconds) const;
    void fitView();

    static constexpr int panelWidth = 250, keyboardWidth = 52, rulerHeight = 24;

private:
    struct Panel final : juce::Component
    {
        explicit Panel (VariAudioEditor& o) : owner (o) {}
        void paint (juce::Graphics& g) override { owner.paintPanel (g); }
        VariAudioEditor& owner;
    };
    void paintPanel (juce::Graphics&) const;
    void timerCallback() override;
    void showFunctionsMenu();
    void showExtractMidi();
    void layoutPanel();
    void paintSmartControls (juce::Graphics&, int index) const;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refresh();
    juce::TextButton& makeKey (const juce::String& text, std::function<void()> onClick, const juce::String& tip = {});
    const pr7s::Part* part() const;
    juce::String file() const;
    const std::vector<pr7s::VariSegment>& segments() const;   // the working copy while dragging, else the song's
    void commit (std::vector<pr7s::VariSegment>);
    int segmentAt (juce::Point<float>) const;
    double beatForSeconds (double seconds) const;
    pr7s::PitchSnap snapFor (const juce::ModifierKeys&) const;
    std::vector<int> selectionOrAll() const;
    void paintKeyboard (juce::Graphics&, juce::Rectangle<int>) const;
    void paintDisplay (juce::Graphics&, juce::Rectangle<int>) const;
    void saveSettings() const;

    pr7s::Studio& studio;
    int partId = 0;
    std::vector<int> selection;
    pr7s::PitchSnap snap = pr7s::PitchSnap::absolute;
    pr7s::Key editorScale;
    bool useChordTrack = false, snapToScale = false, scaleGuides = false;
    // The view: the beat at the display's left, pixels per beat; the pitch at its top, pixels per semitone.
    double firstBeat = 0.0, pixelsPerBeat = 120.0;
    float topPitch = 84.0f, rowHeight = 16.0f;

    enum class Gesture { none, move, straighten, correct, marquee, slider,
                         warpEdge, resizeEdge, tilt, anchor, range, formant, volume };
    Gesture gesture = Gesture::none;
    juce::Point<float> downAt, dragAt;
    std::vector<pr7s::VariSegment> base, working;   // as they were when the gesture began; as it goes
    bool dragged = false;
    int hover = -1;
    // (0.35.0) What the gesture works on: the smart control, the segment it was on, Alt held at the start.
    SmartControl control = SmartControl::none;
    int controlIndex = -1;
    bool altAtDown = false;
    float warpToX = 0.0f;
    bool allSmartControls = false, midiInput = false, stepMode = false;
    Colours colours = Colours::automatic;

    std::vector<std::unique_ptr<juce::TextButton>> keys;
    juce::TextButton* analyseKey = nullptr, *bypassKey = nullptr, *resetKey = nullptr, *auditionKey = nullptr;
    std::array<juce::TextButton*, 3> snapKeys {};
    juce::TextButton* editorScaleKey = nullptr, *chordTrackKey = nullptr, *suggestKey = nullptr, *snapScaleKey = nullptr,
                    * guidesKey = nullptr, *quantizeKey = nullptr;
    juce::ComboBox scaleBox;
    juce::Slider correct, straighten, volume, formant;
    juce::Label title, info, correctLabel, straightenLabel, volumeLabel, formantLabel;
    juce::TextButton* functionsKey = nullptr, *defaultControlsKey = nullptr, *allControlsKey = nullptr,
                    * midiInputKey = nullptr, *stepKey = nullptr;
    juce::ComboBox referenceBox, coloursBox;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> headings;   // (in the panel)
    Panel panel { *this };
    juce::Viewport panelView;
};

class VariAudioWindow final : public juce::DocumentWindow
{
public:
    VariAudioWindow (pr7s::Studio&, int partId);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    VariAudioEditor& getEditor() { return *editor; }

private:
    VariAudioEditor* editor = nullptr;
};
