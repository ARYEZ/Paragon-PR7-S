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
class VariAudioEditor final : public juce::Component, private juce::ChangeListener
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

    enum class Gesture { none, move, straighten, correct, marquee, slider };
    Gesture gesture = Gesture::none;
    juce::Point<float> downAt, dragAt;
    std::vector<pr7s::VariSegment> base, working;   // as they were when the gesture began; as it goes
    bool dragged = false;
    int hover = -1;

    std::vector<std::unique_ptr<juce::TextButton>> keys;
    juce::TextButton* analyseKey = nullptr, *bypassKey = nullptr, *resetKey = nullptr, *auditionKey = nullptr;
    std::array<juce::TextButton*, 3> snapKeys {};
    juce::TextButton* editorScaleKey = nullptr, *chordTrackKey = nullptr, *suggestKey = nullptr, *snapScaleKey = nullptr,
                    * guidesKey = nullptr, *quantizeKey = nullptr;
    juce::ComboBox scaleBox;
    juce::Slider correct, straighten, volume;
    juce::Label title, info, correctLabel, straightenLabel, volumeLabel;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> headings;
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
