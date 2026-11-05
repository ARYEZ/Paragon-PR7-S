#include "TempoEditor.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    const auto onyx = juce::Colour (0xff0c0a0d);
    const auto panelColour = juce::Colour (0xff120e13);
    constexpr float pointSize = 8.0f, pointGrab = 7.0f;
    juce::Font font (float size) { return ParagonLookAndFeel::textFont (size); }

    void styleKey (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1c1c20));
        b.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        b.setColour (juce::TextButton::textColourOnId, ParagonColours::accent);
        b.setWantsKeyboardFocus (false);
    }

    void styleField (juce::Label& l, bool editable)
    {
        l.setColour (juce::Label::textColourId, ParagonColours::text);
        l.setColour (juce::Label::backgroundColourId, juce::Colour (0xff050405));
        l.setColour (juce::Label::outlineColourId, ParagonColours::panelEdge);
        l.setFont (font (15.0f));
        l.setJustificationType (juce::Justification::centred);
        l.setEditable (false, editable, false);
    }

    juce::String bpmText (double bpm)
    {
        return juce::String (bpm, std::abs (bpm - std::round (bpm)) < 0.005 ? 0 : 2);
    }
}

//==============================================================================
// The ruler, the time signature display, the tempo scale and the curve, drawn
// and edited as one.
class TempoEditor::Canvas final : public juce::Component
{
public:
    explicit Canvas (TempoEditor& e) : editor (e) { setWantsKeyboardFocus (false); }

    TempoEditor& editor;
    enum class Drag { none, points, band, draw, position } drag = Drag::none;
    std::vector<pr7s::TempoEvent> working, original;
    juce::Point<float> downAt;
    juce::Rectangle<float> band;
    int anchor = -1;
    bool moved = false;
    std::vector<double> drawn;

    const std::vector<pr7s::TempoEvent>& events() const
    {
        return drag == Drag::points || drag == Drag::draw ? working : editor.studio.getProject().tempoEvents;
    }

    juce::Rectangle<float> curveArea() const
    {
        return getLocalBounds().toFloat().withTrimmedLeft ((float) scaleWidth).withTrimmedTop ((float) (rulerHeight + signatureHeight)).reduced (0.0f, 8.0f);
    }

    double bpmForY (float y) const
    {
        const auto& p = editor.studio.getProject();
        const auto area = curveArea();
        const auto proportion = juce::jlimit (0.0, 1.0, (double) (area.getBottom() - y) / juce::jmax (1.0f, area.getHeight()));
        return std::round (p.tempoViewLow + proportion * (p.tempoViewHigh - p.tempoViewLow));
    }

    void paint (juce::Graphics& g) override
    {
        const auto& project = editor.studio.getProject();
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (panelColour);
        g.fillRect (bounds);
        const auto area = curveArea();
        const auto on = project.followsTempoTrack();

        // The ruler: bars (beats when there's room), the cycle faintly.
        const auto ruler = bounds.withTrimmedLeft ((float) scaleWidth).withHeight ((float) rulerHeight);
        g.setColour (juce::Colour (0xff151116));
        g.fillRect (ruler);
        const auto signatures = project.signatures();   // (bars by the signature events, 0.45.0)
        const auto lastBeat = editor.timeline.beatForX (bounds.getWidth());
        const auto barStep = std::ceil (60.0 / (signatures.barLengthAt (editor.timeline.firstBeat) * editor.timeline.pixelsPerBeat));   // bars between numbers
        g.setFont (font (12.0f));
        for (auto barIndex = signatures.barAt (editor.timeline.firstBeat); signatures.beatOfBar (barIndex) <= lastBeat; ++barIndex)
        {
            const auto x = editor.xForBeat (signatures.beatOfBar (barIndex));
            if (x < (float) scaleWidth)
                continue;
            const auto number = barIndex + 1;
            g.setColour (juce::Colour (0xff3a3238));
            g.fillRect (juce::Rectangle<float> (x, ruler.getBottom(), 1.0f, bounds.getHeight() - ruler.getBottom()));
            if ((number - 1) % juce::jmax (1, (int) barStep) == 0)
            {
                g.setColour (ParagonColours::textDim);
                g.fillRect (juce::Rectangle<float> (x, ruler.getY() + 12.0f, 1.0f, ruler.getHeight() - 12.0f));
                g.drawText (juce::String (number), juce::Rectangle<float> (x + 3.0f, ruler.getY(), 40.0f, 16.0f), juce::Justification::centredLeft, false);
            }
        }
        if (project.loopOn)
        {
            const auto x1 = editor.xForBeat (project.loopStart), x2 = editor.xForBeat (project.loopEnd);
            g.setColour (ParagonColours::accent.withAlpha (0.08f));
            g.fillRect (juce::Rectangle<float> (x1, ruler.getBottom(), x2 - x1, bounds.getHeight()).getIntersection (bounds.withTrimmedLeft ((float) scaleWidth)));
        }

        // The time signature display: each signature event at its bar (0.45.0).
        const auto signature = bounds.withTrimmedLeft ((float) scaleWidth).withY (ruler.getBottom()).withHeight ((float) signatureHeight);
        g.setColour (juce::Colour (0xff0f0b10));
        g.fillRect (signature);
        g.setColour (ParagonColours::panelEdge);
        g.fillRect (signature.withY (signature.getBottom() - 1.0f).withHeight (1.0f));
        g.setFont (ParagonLookAndFeel::titleFont (13.0f));
        for (const auto& segment : signatures.segments())
        {
            const auto x0 = editor.xForBeat (segment.beat);
            if (x0 < (float) scaleWidth - 1.0f || x0 > bounds.getRight())
                continue;
            g.setColour (segment.event.bar == 0 ? ParagonColours::accent : pr7s::FamilyColours::pink);
            g.fillRect (juce::Rectangle<float> (x0, signature.getY() + 2.0f, 1.5f, signature.getHeight() - 4.0f));
            g.drawText (juce::String (segment.event.numerator) + "/" + juce::String (segment.event.denominator),
                        juce::Rectangle<float> (x0 + 4.0f, signature.getY(), 60.0f, signature.getHeight()), juce::Justification::centredLeft, false);
        }

        // The tempo scale: BPM along the left, with level lines across.
        const auto scale = bounds.withWidth ((float) scaleWidth).withTrimmedTop ((float) (rulerHeight + signatureHeight));
        g.setColour (juce::Colour (0xff151116));
        g.fillRect (scale);
        const auto span = project.tempoViewHigh - project.tempoViewLow;
        const auto step = span > 160.0 ? 20.0 : span > 60.0 ? 10.0 : 5.0;
        g.setFont (font (11.0f));
        for (auto bpm = std::ceil (project.tempoViewLow / step) * step; bpm <= project.tempoViewHigh + 1.0e-6; bpm += step)
        {
            const auto y = editor.yForBpm (bpm);
            g.setColour (juce::Colour (0xff221c22));
            g.fillRect (juce::Rectangle<float> ((float) scaleWidth, y, bounds.getWidth() - (float) scaleWidth, 1.0f));
            g.setColour (ParagonColours::textDim);
            g.drawText (juce::String (juce::roundToInt (bpm)), juce::Rectangle<float> (0.0f, y - 7.0f, (float) scaleWidth - 8.0f, 14.0f),
                        juce::Justification::centredRight, false);
        }

        // The curve (greyed out with the tempo track off; the fixed tempo dotted across).
        g.saveState();
        g.reduceClipRegion (bounds.withTrimmedLeft ((float) scaleWidth).withTrimmedTop ((float) (rulerHeight + signatureHeight)).toNearestInt());
        const auto colour = on ? pr7s::FamilyColours::orange : ParagonColours::textDim.withAlpha (0.6f);
        if (! on)
        {
            const auto y = editor.yForBpm (project.bpm);
            g.setColour (ParagonColours::text.withAlpha (0.7f));
            for (auto x = (float) scaleWidth; x < bounds.getRight(); x += 6.0f)
                g.fillRect (x, y, 3.0f, 1.0f);
        }
        const auto& list = events();
        if (! list.empty())
        {
            juce::Path curve;
            curve.startNewSubPath ((float) scaleWidth, editor.yForBpm (list.front().bpm));
            for (size_t i = 1; i < list.size(); ++i)
            {
                const auto x = editor.xForBeat (list[i].beat);
                if (! list[i].ramp)
                    curve.lineTo (x, editor.yForBpm (list[i - 1].bpm));
                curve.lineTo (x, editor.yForBpm (list[i].bpm));
            }
            curve.lineTo (bounds.getRight(), editor.yForBpm (list.back().bpm));
            auto fill = curve;
            fill.lineTo (bounds.getRight(), area.getBottom());
            fill.lineTo ((float) scaleWidth, area.getBottom());
            fill.closeSubPath();
            g.setColour (colour.withAlpha (0.10f));
            g.fillPath (fill);
            g.setColour (colour);
            g.strokePath (curve, juce::PathStrokeType (2.0f));
            g.setFont (font (11.0f));
            for (int i = 0; i < (int) list.size(); ++i)
            {
                const auto at = juce::Point<float> (editor.xForBeat (list[(size_t) i].beat), editor.yForBpm (list[(size_t) i].bpm));
                const auto box = juce::Rectangle<float> (pointSize, pointSize).withCentre (at);
                const auto selected = std::find (editor.selection.begin(), editor.selection.end(), i) != editor.selection.end();
                g.setColour (selected ? juce::Colour (0xff050405) : colour);
                g.fillRect (box);
                g.setColour (selected ? colour : juce::Colour (0xff050405));
                g.drawRect (box, 1.2f);
                g.setColour (on ? ParagonColours::text : ParagonColours::textDim);
                g.drawText (bpmText (list[(size_t) i].bpm), juce::Rectangle<float> (at.x + 6.0f, at.y - 16.0f, 60.0f, 12.0f), juce::Justification::centredLeft, false);
            }
        }
        if (drag == Drag::band)
        {
            g.setColour (ParagonColours::accent.withAlpha (0.15f));
            g.fillRect (band);
            g.setColour (ParagonColours::accent);
            g.drawRect (band, 1.0f);
        }
        // The playhead: pearl white, a pink glow (as the arrangement's).
        const auto px = editor.xForBeat (editor.studio.getEngine().getPosition());
        g.setColour (pr7s::FamilyColours::pink.withAlpha (0.25f));
        g.fillRect (juce::Rectangle<float> (px - 3.0f, 0.0f, 7.0f, bounds.getHeight()));
        g.setColour (juce::Colour (0xfff4eeea));
        g.fillRect (juce::Rectangle<float> (px, 0.0f, 1.2f, bounds.getHeight()));
        g.restoreState();
    }

    int eventAt (juce::Point<float> p) const
    {
        const auto& list = editor.studio.getProject().tempoEvents;
        for (int i = (int) list.size() - 1; i >= 0; --i)
            if (editor.getEventPosition (i).getDistanceFrom (p) <= pointGrab)
                return i;
        return -1;
    }

    double snapped (double beat) const
    {
        return juce::jmax (0.0, pr7s::snap (beat, editor.timeline.snapGrid (editor.studio.getProject())));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto& studio = editor.studio;
        const auto& project = studio.getProject();
        moved = false;
        downAt = e.position;
        if (e.position.y < (float) rulerHeight)
        {
            drag = Drag::position;   // the ruler: the playhead there
            studio.setPosition (juce::jmax (0.0, editor.timeline.beatForX (e.position.x - (float) scaleWidth)));
            return;
        }
        if (e.position.x >= (float) scaleWidth && e.position.y < (float) (rulerHeight + signatureHeight) && ! e.mods.isPopupMenu())
        {
            // The signature display (0.45.0): DRAW adds one at the nearest bar line, ERASE removes the one there.
            const auto map = project.signatures();
            const auto beat = editor.timeline.beatForX (e.position.x - (float) scaleWidth);
            const auto bar = signatureAt (e.position);
            if (editor.tool == Tool::erase && bar > 0)
                studio.removeSignatures ({ bar });
            else if (editor.tool == Tool::draw && bar < 0 && editor.onEditSignature)
                editor.onEditSignature (map.barAt (beat + 0.5 * map.barLengthAt (beat)), true);
            repaint();
            return;
        }
        if (e.position.x < (float) scaleWidth || e.position.y < (float) (rulerHeight + signatureHeight))
            return;
        if (e.mods.isPopupMenu())
            return;
        original = working = project.tempoEvents;
        anchor = eventAt (e.position);
        if (editor.tool == Tool::zoom)
        {
            const auto beat = editor.timeline.beatForX (e.position.x - (float) scaleWidth);
            editor.timeline.pixelsPerBeat = juce::jlimit (2.0, 400.0, editor.timeline.pixelsPerBeat * (e.mods.isAltDown() ? 0.67 : 1.5));
            editor.timeline.firstBeat = juce::jmax (0.0, beat - (e.position.x - (float) scaleWidth) / editor.timeline.pixelsPerBeat);
            editor.refreshScrollBar();
            repaint();
            return;
        }
        if (editor.tool == Tool::erase)
        {
            if (anchor > 0)
                studio.removeTempoEvents ({ anchor });
            editor.selection.clear();
            editor.refresh();
            return;
        }
        if (project.tempoLocked)
        {
            if (anchor >= 0)
                editor.selectEvents ({ anchor });
            return;
        }
        if (editor.tool == Tool::draw)
        {
            drag = Drag::draw;
            drawn.clear();
            editor.selection.clear();
            mouseDrag (e);
            return;
        }
        if (anchor >= 0)
        {
            auto& sel = editor.selection;
            const auto already = std::find (sel.begin(), sel.end(), anchor) != sel.end();
            if (e.mods.isShiftDown())
            {
                if (already)
                    sel.erase (std::find (sel.begin(), sel.end(), anchor));
                else
                    sel.push_back (anchor);
                std::sort (sel.begin(), sel.end());
                editor.refreshInfo();
                repaint();
                return;
            }
            if (! already)
                sel = { anchor };
            drag = Drag::points;
            editor.refreshInfo();
            repaint();
            return;
        }
        if (! e.mods.isShiftDown())
            editor.selection.clear();
        band = { e.position, e.position };
        drag = Drag::band;
        editor.refreshInfo();
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto& studio = editor.studio;
        if (e.getDistanceFromDragStart() > 2)
            moved = true;
        if (drag == Drag::position)
        {
            studio.setPosition (juce::jmax (0.0, editor.timeline.beatForX (e.position.x - (float) scaleWidth)));
            return;
        }
        if (drag == Drag::band)
        {
            band = juce::Rectangle<float> (downAt, e.position);
            editor.selection.clear();
            for (int i = 0; i < (int) studio.getProject().tempoEvents.size(); ++i)
                if (band.contains (editor.getEventPosition (i)))
                    editor.selection.push_back (i);
            editor.refreshInfo();
        }
        else if (drag == Drag::points && moved && anchor >= 0)
        {
            // The grabbed event to the mouse (snapped), the selected ones with it; Ctrl keeps to one direction.
            const auto& grabbed = original[(size_t) anchor];
            const auto offset = e.position - downAt;
            const auto ctrl = e.mods.isCommandDown();
            const auto alongOnly = ctrl && std::abs (offset.x) >= std::abs (offset.y);
            const auto upDownOnly = ctrl && ! alongOnly;
            const auto beat = snapped (grabbed.beat + offset.x / editor.timeline.pixelsPerBeat);
            const auto beatShift = upDownOnly ? 0.0 : beat - grabbed.beat;
            const auto bpmShift = alongOnly ? 0.0 : bpmForY (editor.yForBpm (grabbed.bpm) + offset.y) - grabbed.bpm;
            working = original;
            for (const auto i : editor.selection)
                if (juce::isPositiveAndBelow (i, (int) working.size()))
                {
                    if (i > 0)
                        working[(size_t) i].beat = juce::jmax (1.0e-3, original[(size_t) i].beat + beatShift);
                    working[(size_t) i].bpm = juce::jlimit (pr7s::Project::minBpm, pr7s::Project::maxBpm, original[(size_t) i].bpm + bpmShift);
                }
        }
        else if (drag == Drag::draw)
        {
            const auto& project = studio.getProject();
            const auto beat = snapped (editor.timeline.beatForX (e.position.x - (float) scaleWidth));
            const auto bpm = bpmForY (e.position.y);
            const auto startBeat = juce::jmax (0.0, editor.timeline.beatForX (downAt.x - (float) scaleWidth));
            const auto from = juce::jmin (startBeat, beat), to = juce::jmax (startBeat, beat);
            std::vector<pr7s::TempoEvent> kept;
            for (const auto& ev : working)
            {
                const auto wasDrawn = std::any_of (drawn.begin(), drawn.end(), [&ev] (double b) { return std::abs (b - ev.beat) < 1.0e-9; });
                if ((ev.beat <= 0.0 || ev.beat < from - 1.0e-9 || ev.beat > to + 1.0e-9 || wasDrawn) && ! (std::abs (ev.beat - beat) < 1.0e-9 && ev.beat > 0.0))
                    kept.push_back (ev);
            }
            auto ramp = project.newTempoType == pr7s::Project::NewTempoType::ramp;
            if (project.newTempoType == pr7s::Project::NewTempoType::automatic)
                for (const auto& ev : kept)
                    if (ev.beat < beat)
                        ramp = ev.ramp;
            if (beat <= 0.0 && ! kept.empty())
                kept.front().bpm = bpm;
            else
            {
                kept.push_back ({ beat, bpm, ramp });
                drawn.push_back (beat);
            }
            std::stable_sort (kept.begin(), kept.end(), [] (const auto& a, const auto& b) { return a.beat < b.beat; });
            working = std::move (kept);
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        auto& studio = editor.studio;
        const auto finished = drag;
        drag = Drag::none;
        if (finished == Drag::band && ! moved)
        {
            // A click on the curve: an event there (snapped), at the tempo clicked.
            const auto index = studio.addTempoEvent (snapped (editor.timeline.beatForX (downAt.x - (float) scaleWidth)), bpmForY (downAt.y));
            editor.selection.clear();
            if (index >= 0)
                editor.selection.push_back (index);
        }
        else if ((finished == Drag::points && moved) || finished == Drag::draw)
        {
            std::vector<pr7s::TempoEvent> movedOnes;
            for (const auto i : editor.selection)
                if (juce::isPositiveAndBelow (i, (int) working.size()))
                    movedOnes.push_back (working[(size_t) i]);
            studio.setTempoEvents (working);
            editor.selection.clear();
            const auto& list = studio.getProject().tempoEvents;
            for (int i = 0; i < (int) list.size(); ++i)
                if (std::find (movedOnes.begin(), movedOnes.end(), list[(size_t) i]) != movedOnes.end())
                    editor.selection.push_back (i);
        }
        editor.refresh();
    }

    // The signature event whose label is under the point (its bar), -1: none.
    int signatureAt (juce::Point<float> at) const
    {
        if (at.y < (float) rulerHeight || at.y >= (float) (rulerHeight + signatureHeight))
            return -1;
        const auto map = editor.studio.getProject().signatures();
        for (auto it = map.segments().rbegin(); it != map.segments().rend(); ++it)
        {
            const auto x = editor.xForBeat (it->beat);
            if (at.x >= x - 2.0f && at.x <= x + 44.0f)
                return it->event.bar;
        }
        return -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.position.y >= (float) rulerHeight && e.position.y < (float) (rulerHeight + signatureHeight) && e.position.x >= (float) scaleWidth)
        {
            if (editor.onEditSignature)
            {
                const auto map = editor.studio.getProject().signatures();
                const auto beat = editor.timeline.beatForX (e.position.x - (float) scaleWidth);
                const auto bar = signatureAt (e.position);
                editor.onEditSignature (bar >= 0 ? bar : map.barAt (beat + 0.5 * map.barLengthAt (beat)), bar < 0);
            }
            return;
        }
        // An event: its value typed in the info line.
        if (const auto i = eventAt (e.position); i >= 0)
        {
            editor.selectEvents ({ i });
            editor.setInfoShown (true);
            editor.valueField.showEditor();
        }
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        auto& t = editor.timeline;
        if (e.mods.isCommandDown() || e.mods.isCtrlDown())
        {
            const auto beat = t.beatForX (e.position.x - (float) scaleWidth);
            t.pixelsPerBeat = juce::jlimit (2.0, 400.0, t.pixelsPerBeat * (wheel.deltaY > 0.0f ? 1.25 : 0.8));
            t.firstBeat = juce::jmax (0.0, beat - (e.position.x - (float) scaleWidth) / t.pixelsPerBeat);
        }
        else
        {
            const auto delta = std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? wheel.deltaX : wheel.deltaY;
            t.firstBeat = juce::jmax (0.0, t.firstBeat - delta * 400.0 / t.pixelsPerBeat);
        }
        editor.refreshScrollBar();
        repaint();
    }
};

//==============================================================================
// The tempo events as a list (position, tempo, type): a click selects one.
class TempoEditor::EventList final : public juce::Component,
                                     private juce::ListBoxModel
{
public:
    explicit EventList (TempoEditor& e) : editor (e)
    {
        list.setModel (this);
        list.setRowHeight (26);
        list.setColour (juce::ListBox::backgroundColourId, panelColour);
        list.setMultipleSelectionEnabled (true);
        list.setWantsKeyboardFocus (false);
        addAndMakeVisible (list);
    }
    int getNumRows() override { return (int) editor.studio.getProject().tempoEvents.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        const auto& events = editor.studio.getProject().tempoEvents;
        if (! juce::isPositiveAndBelow (row, (int) events.size()))
            return;
        const auto& e = events[(size_t) row];
        if (selected)
        {
            g.setColour (ParagonColours::accent.withAlpha (0.22f));
            g.fillAll();
        }
        g.setColour (ParagonColours::text);
        g.setFont (font (13.0f));
        g.drawText (pr7s::formatBarsBeats (e.beat, editor.studio.getProject()), 8, 0, 110, height, juce::Justification::centredLeft, false);
        g.drawText (bpmText (e.bpm), 110, 0, 60, height, juce::Justification::centredRight, false);
        g.setColour (row == 0 ? ParagonColours::textDim : ParagonColours::accent);
        g.drawText (row == 0 ? juce::String ("START") : e.ramp ? juce::String ("RAMP") : juce::String ("STEP"), width - 62, 0, 56, height,
                    juce::Justification::centredRight, false);
    }
    void selectedRowsChanged (int) override
    {
        if (updating)
            return;
        std::vector<int> rows;
        const auto chosen = list.getSelectedRows();
        for (int i = 0; i < chosen.size(); ++i)
            rows.push_back (chosen[i]);
        editor.selection = rows;
        editor.refreshInfo();
        editor.canvas->repaint();
    }
    void refresh()
    {
        updating = true;
        list.updateContent();
        juce::SparseSet<int> rows;
        for (const auto i : editor.selection)
            rows.addRange ({ i, i + 1 });
        list.setSelectedRows (rows, juce::dontSendNotification);
        list.repaint();
        updating = false;
    }
    void resized() override { list.setBounds (getLocalBounds().withTrimmedTop (26)); }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff151116));
        g.setColour (ParagonColours::textDim);
        g.setFont (font (11.0f));
        g.drawText ("POSITION", 8, 0, 100, 26, juce::Justification::centredLeft, false);
        g.drawText ("BPM", 110, 0, 60, 26, juce::Justification::centredRight, false);
        g.drawText ("TYPE", getWidth() - 62, 0, 56, 26, juce::Justification::centredRight, false);
    }
    TempoEditor& editor;
    juce::ListBox list;
    bool updating = false;
};

//==============================================================================
TempoEditor::TempoEditor (pr7s::Studio& s) : studio (s)
{
    timeline.pixelsPerBeat = 24.0;
    canvas = std::make_unique<Canvas> (*this);
    list = std::make_unique<EventList> (*this);
    addAndMakeVisible (*canvas);
    addAndMakeVisible (*list);
    scrollBar.setAutoHide (false);
    scrollBar.addListener (this);
    addAndMakeVisible (scrollBar);

    for (auto* b : { &activate, &autoScroll, &selectTool, &drawTool, &eraseTool, &zoomTool, &type, &snap, &process, &info })
    {
        styleKey (*b);
        addAndMakeVisible (*b);
    }
    activate.setTooltip ("Activate Tempo Track: the song follows the curve; off, its fixed tempo (Current Tempo)");
    activate.onClick = [this] { studio.setTempoTrackOn (! studio.getProject().followsTempoTrack()); };
    autoScroll.setTooltip ("Auto-Scroll: the view follows the playhead");
    autoScroll.setClickingTogglesState (true);
    autoScroll.setToggleState (true, juce::dontSendNotification);
    selectTool.setTooltip ("Object Selection (1): select events, drag them; a click on the curve adds one");
    drawTool.setTooltip ("Draw (8): draw events");
    eraseTool.setTooltip ("Erase (5): click an event to remove it");
    zoomTool.setTooltip ("Zoom (6): click to zoom in, Alt-click to zoom out");
    selectTool.onClick = [this] { setTool (Tool::select); };
    drawTool.onClick = [this] { setTool (Tool::draw); };
    eraseTool.onClick = [this] { setTool (Tool::erase); };
    zoomTool.onClick = [this] { setTool (Tool::zoom); };
    type.setTooltip ("Type of New Tempo Points: Ramp (gradually from the point before), Step (a jump), Auto (as the point before)");
    type.onClick = [this]
    {
        using T = pr7s::Project::NewTempoType;
        const auto now = studio.getProject().newTempoType;
        studio.setNewTempoType (now == T::ramp ? T::step : now == T::step ? T::automatic : T::ramp);
    };
    snap.setTooltip ("Snap On/Off (J): events snap to the quantize value");
    snap.onClick = [this] { studio.setSnap (! studio.getProject().snapOn); };
    process.setTooltip ("Process Tempo: the locator range made to last a new length, its tempo scaled");
    process.onClick = [this] { openProcessTempo(); };
    info.setTooltip ("Show/Hide Info: the selected event's position, value and type");
    info.onClick = [this] { setInfoShown (! infoShown); };

    styleField (currentTempo, true);
    currentTempo.setTooltip ("Current Tempo: the song's fixed tempo (with the tempo track off); double-click to type it");
    currentTempo.onTextChange = [this]
    {
        if (const auto bpm = currentTempo.getText().replaceCharacter (',', '.').getDoubleValue(); bpm > 0.0)
            studio.setTempo (bpm);
        refresh();
    };
    addAndMakeVisible (currentTempo);

    styleField (positionField, true);
    styleField (valueField, true);
    positionField.onTextChange = [this] { setSelectedPosition (positionField.getText()); };
    valueField.onTextChange = [this] { setSelectedValue (valueField.getText()); };
    for (auto* l : { &positionLabel, &valueLabel, &typeLabel, &noSelection })
    {
        l->setColour (juce::Label::textColourId, ParagonColours::textDim);
        l->setFont (font (12.0f));
        addAndMakeVisible (*l);
    }
    positionLabel.setText ("POSITION", juce::dontSendNotification);
    valueLabel.setText ("VALUE (BPM)", juce::dontSendNotification);
    typeLabel.setText ("TYPE", juce::dontSendNotification);
    noSelection.setText ("NO TEMPO EVENT SELECTED: CLICK ONE ON THE CURVE OR IN THE LIST", juce::dontSendNotification);
    typeBox.addItem ("Step", 1);
    typeBox.addItem ("Ramp", 2);
    typeBox.onChange = [this] { setSelectedRamp (typeBox.getSelectedId() == 2); };
    typeBox.setWantsKeyboardFocus (false);
    addAndMakeVisible (positionField);
    addAndMakeVisible (valueField);
    addAndMakeVisible (typeBox);

    setWantsKeyboardFocus (true);
    studio.addChangeListener (this);
    setSize (1200, 640);
    refresh();
    startTimerHz (30);
}

TempoEditor::~TempoEditor()
{
    studio.removeChangeListener (this);
}

juce::Component& TempoEditor::getCanvas() { return *canvas; }

void TempoEditor::setTool (Tool t)
{
    tool = t;
    selectTool.setToggleState (t == Tool::select, juce::dontSendNotification);
    drawTool.setToggleState (t == Tool::draw, juce::dontSendNotification);
    eraseTool.setToggleState (t == Tool::erase, juce::dontSendNotification);
    zoomTool.setToggleState (t == Tool::zoom, juce::dontSendNotification);
}

float TempoEditor::xForBeat (double beat) const
{
    return (float) scaleWidth + timeline.xForBeat (beat);
}

float TempoEditor::yForBpm (double bpm) const
{
    const auto& p = studio.getProject();
    const auto area = canvas->curveArea();
    const auto proportion = (bpm - p.tempoViewLow) / juce::jmax (1.0, p.tempoViewHigh - p.tempoViewLow);
    return area.getBottom() - (float) juce::jlimit (0.0, 1.0, proportion) * area.getHeight();
}

juce::Rectangle<float> TempoEditor::getCurveArea() const { return canvas->curveArea(); }

juce::Point<float> TempoEditor::getEventPosition (int index) const
{
    const auto& list2 = canvas->events();
    if (! juce::isPositiveAndBelow (index, (int) list2.size()))
        return {};
    return { xForBeat (list2[(size_t) index].beat), yForBpm (list2[(size_t) index].bpm) };
}

void TempoEditor::selectEvents (std::vector<int> indices)
{
    selection = std::move (indices);
    refresh();
}

void TempoEditor::deleteSelected()
{
    studio.removeTempoEvents (selection);
    selection.clear();
    refresh();
}

std::optional<double> TempoEditor::parsePosition (const juce::String& text, const pr7s::Project& project)
{
    auto parts = juce::StringArray::fromTokens (text.trim(), ".:, ", {});
    parts.removeEmptyStrings();
    if (parts.isEmpty() || parts.size() > 4)
        return std::nullopt;
    for (const auto& p : parts)
        if (! p.containsOnly ("0123456789"))
            return std::nullopt;
    const auto bar = parts[0].getIntValue(), beat = parts.size() > 1 ? parts[1].getIntValue() : 1,
               sixteenth = parts.size() > 2 ? parts[2].getIntValue() : 1, tick = parts.size() > 3 ? parts[3].getIntValue() : 0;
    if (bar < 1 || beat < 1 || sixteenth < 1 || tick < 0)
        return std::nullopt;
    // (Bars by the signature track, 0.45.0: the beat in the signature of its bar.)
    const auto map = project.signatures();
    return map.beatOf (bar - 1, beat - 1, (sixteenth - 1) * 0.25 + tick / 480.0);
}

bool TempoEditor::setSelectedPosition (const juce::String& text)
{
    auto events = studio.getProject().tempoEvents;
    if (selection.size() != 1 || ! juce::isPositiveAndBelow (selection.front(), (int) events.size()) || selection.front() == 0)
    {
        refreshInfo();
        return false;   // (the first event stays at the start)
    }
    const auto beat = parsePosition (text, studio.getProject());
    if (! beat || *beat <= 0.0)
    {
        refreshInfo();
        return false;
    }
    const auto moved = events[(size_t) selection.front()];
    events[(size_t) selection.front()].beat = *beat;
    studio.setTempoEvents (events);
    selection.clear();
    const auto& now = studio.getProject().tempoEvents;
    for (int i = 0; i < (int) now.size(); ++i)
        if (std::abs (now[(size_t) i].beat - *beat) < 1.0e-6 && std::abs (now[(size_t) i].bpm - moved.bpm) < 1.0e-9)
            selection.push_back (i);
    refresh();
    return true;
}

bool TempoEditor::setSelectedValue (const juce::String& text)
{
    const auto bpm = text.retainCharacters ("0123456789.,").replaceCharacter (',', '.').getDoubleValue();
    auto events = studio.getProject().tempoEvents;
    if (selection.empty() || bpm <= 0.0)
    {
        refreshInfo();
        return false;
    }
    for (const auto i : selection)
        if (juce::isPositiveAndBelow (i, (int) events.size()))
            events[(size_t) i].bpm = juce::jlimit (pr7s::Project::minBpm, pr7s::Project::maxBpm, bpm);
    studio.setTempoEvents (events);
    refresh();
    return true;
}

void TempoEditor::setSelectedRamp (bool ramp)
{
    auto events = studio.getProject().tempoEvents;
    for (const auto i : selection)
        if (i > 0 && i < (int) events.size())
            events[(size_t) i].ramp = ramp;
    studio.setTempoEvents (events);
    refresh();
}

void TempoEditor::setInfoShown (bool shown)
{
    infoShown = shown;
    resized();
    refreshInfo();
}

void TempoEditor::zoom (bool in)
{
    const auto centre = timeline.beatForX ((float) (canvas->getWidth() - scaleWidth) * 0.5f);
    timeline.pixelsPerBeat = juce::jlimit (2.0, 400.0, timeline.pixelsPerBeat * (in ? 1.5 : 1.0 / 1.5));
    timeline.firstBeat = juce::jmax (0.0, centre - (double) (canvas->getWidth() - scaleWidth) * 0.5 / timeline.pixelsPerBeat);
    refreshScrollBar();
    canvas->repaint();
}

void TempoEditor::openProcessTempo()
{
    // The manual's Process Tempo dialog, over this window (it closes itself when done).
    auto* page = new ProcessTempoPage (studio);
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (page);
    options.dialogTitle = "Process Tempo";
    options.dialogBackgroundColour = onyx;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.componentToCentreAround = this;
    options.launchAsync();
    page->onDone = [page]
    {
        if (auto* dialog = page->findParentComponentOfClass<juce::DialogWindow>())
            dialog->exitModalState (0);
    };
}

bool TempoEditor::keyPressed (const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();
    const auto mods = key.getModifiers();
    if ((code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) && ! mods.isAnyModifierKeyDown())
    {
        deleteSelected();
        return true;
    }
    if (! mods.isAnyModifierKeyDown())
    {
        const auto c = key.getTextCharacter();
        if (c == '1') { setTool (Tool::select); return true; }
        if (c == '8') { setTool (Tool::draw); return true; }
        if (c == '5') { setTool (Tool::erase); return true; }
        if (c == '6') { setTool (Tool::zoom); return true; }
        if (c == 'h' || c == 'H') { zoom (true); return true; }
        if (c == 'g' || c == 'G') { zoom (false); return true; }
    }
    return onOtherKey != nullptr && onOtherKey (key);
}

void TempoEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // The selection follows its events. The same number of events: an edit of
    // them (a drag, a typed value): the same indices. Events added or removed
    // (here, on the tempo track, by undo): found again by where they are and
    // their tempo; the ones gone leave the selection.
    const auto& events = studio.getProject().tempoEvents;
    const auto count = (int) events.size();
    if (count != (int) knownEvents.size())
    {
        std::vector<int> kept;
        for (const auto i : selection)
            if (i >= 0 && i < (int) knownEvents.size())
                for (int j = 0; j < count; ++j)
                    if (std::abs (events[(size_t) j].beat - knownEvents[(size_t) i].beat) < 1.0e-9
                        && std::abs (events[(size_t) j].bpm - knownEvents[(size_t) i].bpm) < 1.0e-9)
                    {
                        kept.push_back (j);
                        break;
                    }
        selection = kept;
    }
    selection.erase (std::remove_if (selection.begin(), selection.end(), [count] (int i) { return i < 0 || i >= count; }), selection.end());
    knownEvents = events;
    refresh();
}

void TempoEditor::timerCallback()
{
    // The playhead (its strip only), followed while playing (Auto-Scroll).
    const auto position = studio.getEngine().getPosition();
    if (std::abs (position - shownPlayhead) < 1.0e-9)
        return;
    const auto oldX = xForBeat (shownPlayhead), newX = xForBeat (position);
    shownPlayhead = position;
    if (autoScroll.getToggleState() && studio.getEngine().isPlaying())
    {
        const auto width = (float) (canvas->getWidth() - scaleWidth);
        if (newX > (float) scaleWidth + width * 0.9f || newX < (float) scaleWidth)
        {
            timeline.firstBeat = juce::jmax (0.0, position - 0.05 * width / timeline.pixelsPerBeat);
            refreshScrollBar();
            canvas->repaint();
            return;
        }
    }
    canvas->repaint (juce::Rectangle<int> (juce::roundToInt (oldX) - 5, 0, 12, canvas->getHeight()));
    canvas->repaint (juce::Rectangle<int> (juce::roundToInt (newX) - 5, 0, 12, canvas->getHeight()));
}

void TempoEditor::scrollBarMoved (juce::ScrollBar*, double start)
{
    timeline.firstBeat = juce::jmax (0.0, start);
    canvas->repaint();
}

void TempoEditor::refreshScrollBar()
{
    const auto length = Timeline::scrollLength (studio);
    const auto visible = (double) juce::jmax (1, canvas->getWidth() - scaleWidth) / timeline.pixelsPerBeat;
    scrollBar.setRangeLimits (0.0, juce::jmax (length, timeline.firstBeat + visible), juce::dontSendNotification);
    scrollBar.setCurrentRange (timeline.firstBeat, visible, juce::dontSendNotification);
}

void TempoEditor::refresh()
{
    const auto& project = studio.getProject();
    activate.setToggleState (project.followsTempoTrack(), juce::dontSendNotification);
    snap.setToggleState (project.snapOn, juce::dontSendNotification);
    using T = pr7s::Project::NewTempoType;
    type.setButtonText (project.newTempoType == T::ramp ? "RAMP" : project.newTempoType == T::step ? "STEP" : "AUTO");
    // Current Tempo: the fixed tempo, changed here only with the tempo track off (the manual's).
    currentTempo.setText (bpmText (project.bpm) + " BPM", juce::dontSendNotification);
    currentTempo.setEnabled (! project.followsTempoTrack());
    currentTempo.setColour (juce::Label::textColourId, project.followsTempoTrack() ? ParagonColours::textDim : ParagonColours::text);
    setTool (tool);
    list->refresh();
    refreshInfo();
    refreshScrollBar();
    canvas->repaint();
}

void TempoEditor::refreshInfo()
{
    const auto& events = studio.getProject().tempoEvents;
    knownEvents = events;   // (the selection, as it stands, is of these)
    const auto one = ! selection.empty() && juce::isPositiveAndBelow (selection.front(), (int) events.size());
    for (auto* c : std::initializer_list<juce::Component*> { &positionField, &valueField, &typeBox, &positionLabel, &valueLabel, &typeLabel })
        c->setVisible (infoShown && one);
    noSelection.setVisible (infoShown && ! one);
    info.setToggleState (infoShown, juce::dontSendNotification);
    if (! one)
        return;
    const auto& e = events[(size_t) selection.front()];
    positionField.setText (selection.size() > 1 ? juce::String ("(" + juce::String ((int) selection.size()) + " EVENTS)") : pr7s::formatBarsBeats (e.beat, studio.getProject()),
                           juce::dontSendNotification);
    positionField.setEditable (false, selection.size() == 1 && selection.front() > 0, false);
    valueField.setText (bpmText (e.bpm), juce::dontSendNotification);
    typeBox.setSelectedId (e.ramp ? 2 : 1, juce::dontSendNotification);
    typeBox.setEnabled (selection.front() > 0 || selection.size() > 1);
}

void TempoEditor::paint (juce::Graphics& g)
{
    g.fillAll (onyx);
    auto bar = getLocalBounds().removeFromTop (toolbarHeight).toFloat();
    g.setColour (juce::Colour (0xff100c11));
    g.fillRect (bar);
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (bar.withY (bar.getBottom() - 1.0f).withHeight (1.0f));
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (13.0f));
    g.drawText ("TEMPO TRACK", bar.withTrimmedLeft (14.0f).withWidth (110.0f), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (font (11.0f));
    g.drawText ("CURRENT TEMPO", currentTempo.getBounds().translated (0, -12).withHeight (12).toFloat(), juce::Justification::centredLeft, false);
}

void TempoEditor::resized()
{
    auto area = getLocalBounds();
    auto bar = area.removeFromTop (toolbarHeight).reduced (8, 7).withTrimmedLeft (110);
    const auto key = [&bar] (juce::Component& c, int width) { c.setBounds (bar.removeFromLeft (width)); bar.removeFromLeft (4); };
    key (activate, 86);
    key (autoScroll, 100);
    bar.removeFromLeft (10);
    key (selectTool, 70);
    key (drawTool, 60);
    key (eraseTool, 64);
    key (zoomTool, 60);
    bar.removeFromLeft (10);
    key (type, 64);
    bar.removeFromLeft (10);
    currentTempo.setBounds (bar.removeFromLeft (110).withTrimmedTop (10));
    bar.removeFromLeft (14);
    key (snap, 60);
    bar.removeFromLeft (10);
    info.setBounds (bar.removeFromRight (60));
    bar.removeFromRight (6);
    process.setBounds (bar.removeFromRight (140));

    if (infoShown)
    {
        auto line = area.removeFromTop (infoHeight).reduced (14, 5);
        noSelection.setBounds (line);
        positionLabel.setBounds (line.removeFromLeft (70));
        positionField.setBounds (line.removeFromLeft (120));
        line.removeFromLeft (20);
        valueLabel.setBounds (line.removeFromLeft (86));
        valueField.setBounds (line.removeFromLeft (80));
        line.removeFromLeft (20);
        typeLabel.setBounds (line.removeFromLeft (44));
        typeBox.setBounds (line.removeFromLeft (110));
    }
    list->setBounds (area.removeFromLeft (listWidth));
    area.removeFromLeft (2);
    scrollBar.setBounds (area.removeFromBottom (14).withTrimmedLeft (scaleWidth));
    canvas->setBounds (area);
    refreshScrollBar();
}

//==============================================================================
ProcessTempoPage::ProcessTempoPage (pr7s::Studio& s) : studio (s)
{
    const auto& project = studio.getProject();
    start = project.loopStart;
    end = project.loopEnd;
    const auto map = project.tempoMap();
    startSeconds = map.secondsAt (start);
    endSeconds = map.secondsAt (end);
    newSeconds = endSeconds - startSeconds;
    formatBox.addItem ("Seconds", 1);
    formatBox.addItem ("Minutes:Seconds", 2);
    formatBox.setSelectedId (1, juce::dontSendNotification);
    formatBox.onChange = [this] { setFormat (formatBox.getSelectedId() == 2 ? Format::minutesSeconds : Format::seconds); };
    addAndMakeVisible (formatBox);
    for (auto* t : { &newEnd, &newLength })
    {
        t->setFont (font (16.0f));
        t->setJustification (juce::Justification::centred);
        addAndMakeVisible (*t);
    }
    newEnd.onReturnKey = [this] { setNewEnd (newEnd.getText()); };
    newEnd.onFocusLost = [this] { setNewEnd (newEnd.getText()); };
    newLength.onReturnKey = [this] { setNewLength (newLength.getText()); };
    newLength.onFocusLost = [this] { setNewLength (newLength.getText()); };
    for (auto* b : { &processKey, &cancel })
    {
        styleKey (*b);
        addAndMakeVisible (*b);
    }
    processKey.onClick = [this]
    {
        // (PROCESS doesn't take the focus from the field: what's typed there and
        // not yet taken in, taken in now.)
        if (newLength.getText() != asText (newSeconds))
        {
            if (! setNewLength (newLength.getText()))
                return;
        }
        else if (newEnd.getText() != asText (startSeconds + newSeconds) && ! setNewEnd (newEnd.getText()))
            return;
        if (apply() && onDone)
            onDone();
    };
    cancel.onClick = [this] { if (onDone) onDone(); };
    setSize (520, 380);
    show();
}

juce::String ProcessTempoPage::asText (double seconds) const
{
    if (format == Format::seconds)
        return juce::String (seconds, 3);
    const auto minutes = (int) (seconds / 60.0);
    return juce::String (minutes) + ":" + juce::String (seconds - minutes * 60.0, 3).paddedLeft ('0', 6);
}

std::optional<double> ProcessTempoPage::parse (const juce::String& text) const
{
    const auto t = text.trim().replaceCharacter (',', '.');
    if (t.isEmpty() || ! t.containsOnly ("0123456789.:"))
        return std::nullopt;
    if (t.containsChar (':'))
        return t.upToFirstOccurrenceOf (":", false, false).getIntValue() * 60.0 + t.fromFirstOccurrenceOf (":", false, false).getDoubleValue();
    return t.getDoubleValue();
}

void ProcessTempoPage::setFormat (Format f)
{
    format = f;
    formatBox.setSelectedId (f == Format::minutesSeconds ? 2 : 1, juce::dontSendNotification);
    show();
}

bool ProcessTempoPage::setNewLength (const juce::String& text)
{
    const auto value = parse (text);
    if (! value || *value <= 0.0)
    {
        show();
        return false;
    }
    newSeconds = *value;
    show();
    return true;
}

bool ProcessTempoPage::setNewEnd (const juce::String& text)
{
    const auto value = parse (text);
    if (! value || *value <= startSeconds)
    {
        show();
        return false;
    }
    newSeconds = *value - startSeconds;
    show();
    return true;
}

void ProcessTempoPage::show()
{
    scaling = pr7s::Studio::tempoScaling (studio.getProject().tempoMap(), start, end, newSeconds);
    newEnd.setText (asText (startSeconds + newSeconds), false);
    newLength.setText (asText (newSeconds), false);
    processKey.setEnabled (end > start);
    repaint();
}

bool ProcessTempoPage::apply()
{
    return studio.processTempo (start, end, newSeconds);
}

void ProcessTempoPage::paint (juce::Graphics& g)
{
    g.fillAll (onyx);
    const auto& project = studio.getProject();
    auto area = getLocalBounds().reduced (22, 18).toFloat();
    const auto heading = [&] (const juce::String& text)
    {
        g.setColour (ParagonColours::accent);
        g.setFont (ParagonLookAndFeel::titleFont (14.0f));
        g.drawText (text, area.removeFromTop (22.0f), juce::Justification::centredLeft, false);
    };
    const auto row = [&] (const juce::String& label, const juce::String& value)
    {
        auto r = area.removeFromTop (24.0f);
        g.setColour (ParagonColours::textDim);
        g.setFont (font (13.0f));
        g.drawText (label, r.removeFromLeft (200.0f), juce::Justification::centredLeft, false);
        g.setColour (ParagonColours::text);
        g.drawText (value, r, juce::Justification::centredLeft, false);
    };
    heading ("BAR RANGE (THE LOCATORS)");
    row ("Start at Bar", pr7s::formatBarsBeats (start, project));
    row ("Finish at Bar", pr7s::formatBarsBeats (end, project));
    area.removeFromTop (8.0f);
    heading ("ALTERNATIVE TIME FORMAT");
    {
        auto r = area.removeFromTop (28.0f);   // (the format box beside it)
        g.setColour (ParagonColours::textDim);
        g.setFont (font (13.0f));
        g.drawText ("Time Format", r.removeFromLeft (200.0f), juce::Justification::centredLeft, false);
    }
    row ("Start at Position", asText (startSeconds));
    row ("Finish at Position", asText (endSeconds));
    area.removeFromTop (8.0f);
    heading ("NEW RANGE");
    g.setColour (ParagonColours::textDim);
    g.setFont (font (13.0f));
    g.drawText ("New End Position", newEnd.getBounds().withX (22).withWidth (200).toFloat(), juce::Justification::centredLeft, false);
    g.drawText ("New Length", newLength.getBounds().withX (22).withWidth (200).toFloat(), juce::Justification::centredLeft, false);
    auto scale = newLength.getBounds().translated (0, 34).withX (22).withWidth (getWidth() - 44).toFloat();
    g.drawText ("Equivalent Tempo Scaling", scale.removeFromLeft (200.0f), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::text);
    g.drawText (juce::String (scaling * 100.0, 2) + " %", scale, juce::Justification::centredLeft, false);
}

void ProcessTempoPage::resized()
{
    formatBox.setBounds (222, 18 + 22 + 48 + 8 + 22, 180, 24);
    newEnd.setBounds (222, 18 + 22 + 48 + 8 + 22 + 28 + 48 + 8 + 22, 180, 26);
    newLength.setBounds (222, newEnd.getBottom() + 6, 180, 26);
    auto foot = getLocalBounds().reduced (22, 16).removeFromBottom (32);
    cancel.setBounds (foot.removeFromRight (100));
    foot.removeFromRight (8);
    processKey.setBounds (foot.removeFromRight (120));
}

//==============================================================================
TempoEditorWindow::TempoEditorWindow (pr7s::Studio& studio, juce::PropertiesFile* s)
    : DocumentWindow ("Paragon PR7-S: Tempo Track Editor", onyx, DocumentWindow::allButtons), settings (s)
{
    setUsingNativeTitleBar (true);
    editor = new TempoEditor (studio);
    setContentOwned (editor, true);
    setResizable (true, false);
    setResizeLimits (900, 420, 32768, 8000);
    const auto saved = settings != nullptr ? settings->getValue ("tempoEditorWindow") : juce::String();
    if (saved.isEmpty() || ! restoreWindowStateFromString (saved))
        centreWithSize (getWidth(), getHeight());
    setVisible (true);
    editor->grabKeyboardFocus();
}

TempoEditorWindow::~TempoEditorWindow()
{
    if (settings != nullptr)
        settings->setValue ("tempoEditorWindow", getWindowStateAsString());
}

void TempoEditorWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}

bool TempoEditorWindow::keyPressed (const juce::KeyPress& key)
{
    return editor->keyPressed (key);
}
