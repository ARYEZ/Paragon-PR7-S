#pragma once

#include "engine/Studio.h"
#include "ui/ArrangeView.h"

// The Tempo Track Editor (0.13.0, the owner asked: as the other DAW has it;
// the manual's Tempo Track Editor, Ctrl+T), in a window of its own.
//
// Its toolbar (the manual's): ACTIVATE (Activate Tempo Track), AUTO-SCROLL,
// the tools SELECT / DRAW / ERASE / ZOOM, the Type of New Tempo Points (RAMP
// / STEP / AUTO), the Current Tempo (the fixed tempo, while the tempo track is
// off), SNAP, PROCESS TEMPO... and INFO (the info line). The info line: the
// selected event's Position, Value (BPM) and Type, typed in. Then the ruler
// (click: the playhead there), the time signature display, the tempo scale in
// BPM at the left, and the curve with its events, edited as on the tempo
// track: a click on the curve adds an event, events are selected (Shift,
// a rectangle) and dragged, drawn, erased, Delete removes them (never the
// first). Beside it, the tempo events as a list (the manual has it in the
// tempo track's Inspector; here it's with the curve: our choice).
//
// Process Tempo (the manual's dialog): the locator range made to last a new
// length or end at a new time, the tempo in it scaled (Studio::processTempo).
class TempoEditor final : public juce::Component,
                          private juce::ChangeListener,
                          private juce::Timer,
                          private juce::ScrollBar::Listener
{
public:
    explicit TempoEditor (pr7s::Studio&);
    ~TempoEditor() override;

    enum class Tool { select, draw, erase, zoom };
    void setTool (Tool);
    Tool getTool() const noexcept { return tool; }

    // The events selected (indices into the song's tempo events).
    const std::vector<int>& getSelection() const noexcept { return selection; }
    void selectEvents (std::vector<int>);
    void deleteSelected();
    // The info line's fields (as typed in): position ("5.1.1.0"), value (BPM), type.
    bool setSelectedPosition (const juce::String&);
    bool setSelectedValue (const juce::String&);
    void setSelectedRamp (bool);
    void setInfoShown (bool);
    bool isInfoShown() const noexcept { return infoShown; }
    void zoom (bool in);
    void openProcessTempo();

    // Where things are (tests): the curve's area, an event's point, a beat's x.
    juce::Rectangle<float> getCurveArea() const;
    juce::Point<float> getEventPosition (int index) const;
    float xForBeat (double beat) const;
    float yForBpm (double bpm) const;
    juce::Component& getCanvas();

    // "bar.beat.sixteenth.tick" (as the transport shows it) to beats; none if it isn't one.
    static std::optional<double> parsePosition (const juce::String&, const pr7s::Project&);

    std::function<bool (const juce::KeyPress&)> onOtherKey;   // keys it doesn't use (the studio's)

    bool keyPressed (const juce::KeyPress&) override;
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int toolbarHeight = 44, infoHeight = 36, listWidth = 250, scaleWidth = 56,
                         rulerHeight = 28, signatureHeight = 24;

private:
    class Canvas;
    class EventList;
    pr7s::Studio& studio;
    Timeline timeline;   // its own zoom and scroll
    Tool tool = Tool::select;
    std::vector<int> selection;
    bool infoShown = true;
    std::unique_ptr<Canvas> canvas;
    std::unique_ptr<EventList> list;
    juce::ScrollBar scrollBar { false };
    juce::TextButton activate { "ACTIVATE" }, autoScroll { "AUTO-SCROLL" }, selectTool { "SELECT" }, drawTool { "DRAW" },
                     eraseTool { "ERASE" }, zoomTool { "ZOOM" }, type { "RAMP" }, snap { "SNAP" }, process { "PROCESS TEMPO..." },
                     info { "INFO" };
    juce::Label currentTempo, positionField, valueField, positionLabel, valueLabel, typeLabel, noSelection;
    juce::ComboBox typeBox;
    double shownPlayhead = -1.0;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double) override;
    void refresh();
    void refreshInfo();
    void refreshScrollBar();
    friend class Canvas;
    friend class EventList;
};

// The Process Tempo dialog (the manual's): the bar range (the locators), the
// range in an alternative time format, the new end or length, the Equivalent
// Tempo Scaling, PROCESS.
class ProcessTempoPage final : public juce::Component
{
public:
    explicit ProcessTempoPage (pr7s::Studio&);
    enum class Format { seconds, minutesSeconds };
    void setFormat (Format);
    bool setNewLength (const juce::String&);    // in the format
    bool setNewEnd (const juce::String&);       // likewise
    double getScaling() const noexcept { return scaling; }
    bool apply();                                // PROCESS: false if it can't
    std::function<void()> onDone;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    pr7s::Studio& studio;
    Format format = Format::seconds;
    double start = 0.0, end = 0.0, startSeconds = 0.0, endSeconds = 0.0, newSeconds = 0.0, scaling = 1.0;
    juce::ComboBox formatBox;
    juce::TextEditor newEnd, newLength;
    juce::TextButton processKey { "PROCESS" }, cancel { "CANCEL" };
    juce::String asText (double seconds) const;
    std::optional<double> parse (const juce::String&) const;
    void show();
};

class TempoEditorWindow final : public juce::DocumentWindow
{
public:
    TempoEditorWindow (pr7s::Studio&, juce::PropertiesFile*);
    ~TempoEditorWindow() override;
    void closeButtonPressed() override;
    bool keyPressed (const juce::KeyPress&) override;
    std::function<void()> onClose;
    TempoEditor& getEditor() { return *editor; }

private:
    TempoEditor* editor = nullptr;
    juce::PropertiesFile* settings;
};
