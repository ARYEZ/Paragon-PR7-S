#include "InfoLine.h"

#include "ui/ArrangeView.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    const juce::Colour stripColour { 0xff0d0a0e };
    constexpr int fieldWidth = 150, typeWidth = 220;
}

InfoLine::InfoLine (pr7s::Studio& s, ArrangeView& a) : studio (s), arrange (a)
{
    setTooltip ("The info line: the selected automation points (the first one's values). Double-click START or VALUE to type: "
                "all of them move by what the first one moves. Hold Ctrl as you press Enter to set them all to it. Right-click to hide");
    refresh();
    startTimerHz (10);
}

void InfoLine::timerCallback()
{
    refresh();
}

void InfoLine::refresh()
{
    fields.clear();
    count = 0;
    const auto& selection = arrange.getPointSelection();
    const auto* lane = arrange.getSelectedPointsLane();
    if (lane != nullptr && ! selection.points.empty())
    {
        const auto& points = arrange.editPointsOf (*lane);
        const auto first = selection.points.front();
        if (juce::isPositiveAndBelow (first, (int) points.size()))
        {
            const auto& p = points[(size_t) first];
            count = (int) selection.points.size();
            fields.push_back ({ "TYPE", studio.getLaneName (selection.busId, *lane) + (arrange.isTrimming (*lane) ? " (TRIM)" : ""), {}, false });
            fields.push_back ({ "START", pr7s::formatBarsBeats (p.beat, studio.getProject()).replace ("  ", " "), {}, true });
            fields.push_back ({ "VALUE", arrange.formatPointValue (selection.busId, *lane, p.value).value_or ("?"), {}, true });
            fields.push_back ({ "TERMINATOR", p.terminator ? "YES" : "NO", {}, ! arrange.isTrimming (*lane) });
        }
    }
    juce::String key (count);
    for (const auto& f : fields)
        key << "|" << f.value;
    resized();
    if (key != shownKey)
    {
        shownKey = key;
        repaint();
    }
}

void InfoLine::resized()
{
    auto x = 12;
    for (auto& f : fields)
    {
        const auto w = f.name == "TYPE" ? typeWidth : fieldWidth;
        f.area = { x, 0, w, getHeight() };
        x += w;
    }
    if (editor != nullptr)
        for (const auto& f : fields)
            if (f.name == editing)
                editor->setBounds (f.area.withTrimmedTop (getHeight() / 2 - 2).reduced (6, 2));
}

juce::String InfoLine::getField (const juce::String& name) const
{
    for (const auto& f : fields)
        if (f.name == name)
            return f.value;
    return {};
}

void InfoLine::paint (juce::Graphics& g)
{
    g.fillAll (stripColour);
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
    if (fields.empty())
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::textFont (13.0f));
        g.drawText ("INFO LINE: SELECT AUTOMATION POINTS (CLICK THEM, OR DRAG A BOX ROUND THEM) TO SEE AND CHANGE THEM HERE",
                    getLocalBounds().reduced (12, 0), juce::Justification::centredLeft, true);
        return;
    }
    // Several: in pink (the manual: "shown in another color"), the first one's values.
    const auto valueColour = count > 1 ? pr7s::FamilyColours::pink.brighter (0.25f) : ParagonColours::accent;
    for (const auto& f : fields)
    {
        auto area = f.area;
        g.setColour (ParagonColours::panelEdge);
        g.fillRect (area.getRight() - 1, 6, 1, getHeight() - 12);
        g.setColour (ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::textFont (12.0f));
        g.drawText (f.name, area.removeFromTop (getHeight() / 2).reduced (8, 0), juce::Justification::bottomLeft, false);
        if (f.name == editing && editor != nullptr)
            continue;
        g.setColour (f.editable || f.name == "TYPE" ? valueColour : ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::textFont (15.0f));
        g.drawFittedText (f.value.toUpperCase(), area.reduced (8, 0), juce::Justification::topLeft, 1, 0.8f);
    }
    if (count > 1)
    {
        g.setColour (pr7s::FamilyColours::pink.brighter (0.25f));
        g.setFont (ParagonLookAndFeel::textFont (13.0f));
        const auto x = fields.back().area.getRight() + 12;
        g.drawText (juce::String (count) + " POINTS SELECTED: CHANGES MOVE THEM ALL BY THE SAME AMOUNT (CTRL+ENTER: ALL TO THE VALUE)",
                    juce::Rectangle<int> (x, 0, juce::jmax (0, getWidth() - x - 12), getHeight()), juce::Justification::centredLeft, true);
    }
}

void InfoLine::mouseDoubleClick (const juce::MouseEvent& e)
{
    for (const auto& f : fields)
        if (f.area.contains (e.getPosition()) && f.editable && f.name != "TERMINATOR")
            beginEdit (f.name);
}

void InfoLine::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.addItem ("Hide Info Line", [safe = juce::Component::SafePointer<InfoLine> (this)]
        {
            if (safe != nullptr && safe->onHide)
                safe->onHide();
        });
        menu.showMenuAsync (juce::PopupMenu::Options());
        return;
    }
    for (const auto& f : fields)
        if (f.area.contains (e.getPosition()) && f.name == "TERMINATOR" && f.editable && e.getNumberOfClicks() == 1)
            toggleTerminator();
}

void InfoLine::toggleTerminator()
{
    arrange.setSelectedPointsTerminator (getField ("TERMINATOR") != "YES");
    refresh();
}

void InfoLine::beginEdit (const juce::String& field)
{
    editing = field;
    editor = std::make_unique<juce::TextEditor>();
    editor->setFont (ParagonLookAndFeel::textFont (14.0f));
    editor->setText (getField (field), false);
    editor->setSelectAllWhenFocused (true);
    auto finish = [this] (bool apply)
    {
        if (editor == nullptr)
            return;
        const auto text = editor->getText();
        const auto field = editing;
        // (The manual's modifier: Ctrl held as the value goes in sets them all to it.)
        const auto absolute = juce::ModifierKeys::getCurrentModifiersRealtime().isCommandDown();
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<InfoLine> (this)] { if (safe != nullptr) { safe->editor.reset(); safe->editing = {}; safe->refresh(); safe->repaint(); } });
        if (apply)
            enter (field, text, absolute);
    };
    editor->onReturnKey = [finish] { finish (true); };
    editor->onEscapeKey = [finish] { finish (false); };
    editor->onFocusLost = [finish] { finish (false); };
    addAndMakeVisible (*editor);
    resized();
    editor->grabKeyboardFocus();
    repaint();
}

bool InfoLine::enter (const juce::String& field, const juce::String& text, bool absolute)
{
    auto done = false;
    if (field == "VALUE")
        done = arrange.setSelectedPointsValue (text, absolute);
    else if (field == "START")
        done = arrange.setSelectedPointsStart (text, absolute);
    refresh();
    return done;
}
