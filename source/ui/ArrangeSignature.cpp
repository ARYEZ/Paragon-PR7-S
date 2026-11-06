// The arrangement's signature track (0.45.0, the manual's Signature Track): drawn, clicked,
// dragged, typed in. The events live in the song (Studio's setSignatureAt and the rest).

#include "ArrangeView.h"

#include "ui/ParagonLookAndFeel.h"

#include <cmath>

namespace
{
    juce::String signatureText (const pr7s::SignatureEvent& e) { return juce::String (e.numerator) + "/" + juce::String (e.denominator); }
}

juce::Rectangle<float> ArrangeView::getSignatureArea() const
{
    std::vector<Timeline::Row> made;
    for (const auto& row : rowsNow (made))
        if (row.kind == Timeline::Row::Kind::signature)
            return juce::Rectangle<float> (0.0f, rowTop (row), (float) getWidth(), (float) row.height).reduced (0.0f, 2.0f);
    return {};
}

juce::Rectangle<float> ArrangeView::getSignatureBounds (int bar) const
{
    const auto area = getSignatureArea();
    if (area.isEmpty())
        return {};
    const auto& project = studio.getProject();
    const auto map = project.signatures();
    for (const auto& segment : map.segments())
        if (segment.event.bar == bar)
        {
            const auto x = timeline.xForBeat (segment.beat);
            const auto width = juce::jmax (34.0f, juce::GlyphArrangement::getStringWidth (juce::Font (ParagonLookAndFeel::titleFont (juce::jmin (17.0f, area.getHeight() * 0.62f))),
                                                                                    signatureText (segment.event)) + 14.0f);
            return juce::Rectangle<float> (x, area.getY() + 2.0f, width, area.getHeight() - 4.0f);
        }
    return {};
}

int ArrangeView::signatureAt (juce::Point<float> at) const
{
    // The latest first (a label can reach over the next bar's line when zoomed out).
    const auto map = studio.getProject().signatures();
    for (auto it = map.segments().rbegin(); it != map.segments().rend(); ++it)
        if (getSignatureBounds (it->event.bar).contains (at))
            return it->event.bar;
    return -1;
}

void ArrangeView::paintSignature (juce::Graphics& g, const Timeline::Row& row)
{
    const auto area = juce::Rectangle<float> (0.0f, rowTop (row), (float) getWidth(), (float) row.height).reduced (0.0f, 2.0f);
    const auto& project = studio.getProject();
    const auto map = project.signatures();
    // The background shows the bars (the manual's: always bars).
    const auto first = map.barAt (timeline.firstBeat), last = map.barAt (timeline.beatForX ((float) getWidth())) + 1;
    auto every = 1;
    while (every < 1024 && map.barLengthAt (timeline.firstBeat) * every * timeline.pixelsPerBeat < 8.0)
        every *= 2;
    g.setColour (ParagonColours::textDim.withAlpha (0.18f));
    for (auto bar = first - first % every; bar <= last; bar += every)
        g.fillRect (juce::Rectangle<float> (timeline.xForBeat (map.beatOfBar (bar)), area.getY() + area.getHeight() * 0.55f, 1.0f, area.getHeight() * 0.45f));

    // Each signature: a label from its bar line, the stretch it holds faintly in its tint.
    const auto& segments = map.segments();
    const auto font = ParagonLookAndFeel::titleFont (juce::jmin (17.0f, area.getHeight() * 0.62f));
    for (size_t i = 0; i < segments.size(); ++i)
    {
        const auto& segment = segments[i];
        const auto x = timeline.xForBeat (segment.beat);
        const auto end = i + 1 < segments.size() ? timeline.xForBeat (segments[i + 1].beat) : (float) getWidth() + 10.0f;
        if (end < 0.0f || x > (float) getWidth())
            continue;
        const auto bar = signatureDragFrom == segment.event.bar && signatureDragTo >= 0 ? signatureDragTo : segment.event.bar;
        const auto drawnX = timeline.xForBeat (map.beatOfBar (bar));
        const auto selected = std::find (signatureSelection.begin(), signatureSelection.end(), segment.event.bar) != signatureSelection.end();
        const auto tint = i == 0 ? pr7s::FamilyColours::orange : pr7s::FamilyColours::pink;
        auto label = getSignatureBounds (segment.event.bar).withX (drawnX);
        g.setColour (tint.withAlpha (0.06f));
        g.fillRect (juce::Rectangle<float> (x, area.getY(), juce::jmax (0.0f, end - x), area.getHeight()));
        g.setColour (tint.withAlpha (selected ? 0.95f : 0.55f));
        g.fillRoundedRectangle (label, 3.0f);
        g.fillRect (juce::Rectangle<float> (drawnX, area.getY(), 1.5f, area.getHeight()));
        g.setColour (selected ? juce::Colours::black : ParagonColours::text);
        g.setFont (font);
        g.drawText (signatureText (segment.event), label.reduced (6.0f, 0.0f), juce::Justification::centredLeft, false);
    }
    if (project.signatureLocked)
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::titleFont (11.0f));
        g.drawText ("LOCKED", area.reduced (8.0f, 0.0f), juce::Justification::centredRight, false);
    }
}

bool ArrangeView::signatureDown (const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
    {
        if (const auto bar = signatureAt (event.position); bar >= 0 && std::find (signatureSelection.begin(), signatureSelection.end(), bar) == signatureSelection.end())
            signatureSelection = { bar };
        showSignatureMenu();
        return true;
    }
    const auto& project = studio.getProject();
    const auto map = project.signatures();
    const auto hit = signatureAt (event.position);
    const auto beat = timeline.beatForX (event.position.x);
    if (tool == Tool::erase)
    {
        if (hit > 0)
            studio.removeSignatures ({ hit });
        signatureSelection.clear();
        repaint();
        return true;
    }
    if (tool == Tool::draw && hit < 0)
    {
        // DRAW: one at the nearest bar line, with the signature in force there, then its box (the manual's: edit it).
        const auto bar = map.barAt (beat + 0.5 * map.barLengthAt (beat));
        editSignature (bar, true);
        return true;
    }
    if (hit < 0)
    {
        if (! event.mods.isShiftDown())
            signatureSelection.clear();
        repaint();
        return true;
    }
    if (event.mods.isShiftDown())
    {
        const auto it = std::find (signatureSelection.begin(), signatureSelection.end(), hit);
        if (it != signatureSelection.end())
            signatureSelection.erase (it);
        else
            signatureSelection.push_back (hit);
    }
    else
        signatureSelection = { hit };
    // Dragged: along the bars (not the first: it's always at bar 1).
    if (hit > 0 && ! project.signatureLocked)
    {
        drag = Drag::signatureMove;
        signatureDragFrom = hit;
        signatureDragTo = hit;
    }
    repaint();
    return true;
}

void ArrangeView::signatureDrag (const juce::MouseEvent& event)
{
    const auto map = studio.getProject().signatures();
    const auto beat = timeline.beatForX (event.position.x);
    const auto bar = juce::jmax (1, map.barAt (beat + 0.5 * map.barLengthAt (beat)));
    if (bar != signatureDragTo)
    {
        signatureDragTo = bar;
        dragMoved = true;
        repaint();
    }
}

void ArrangeView::signatureUp()
{
    drag = Drag::none;
    const auto from = std::exchange (signatureDragFrom, -1), to = std::exchange (signatureDragTo, -1);
    if (from > 0 && to > 0 && to != from && studio.moveSignature (from, to))
        signatureSelection = { to };
    repaint();
}

void ArrangeView::deleteSelectedSignatures()
{
    if (signatureSelection.empty())
        return;
    studio.removeSignatures (signatureSelection);   // (bar 1's is never removed)
    signatureSelection.clear();
    repaint();
}

void ArrangeView::editSignature (int bar, bool adding)
{
    const auto& project = studio.getProject();
    if (project.signatureLocked && bar > 0)
        return;
    const auto map = project.signatures();
    const auto now = map.segmentOfBar (bar).event;
    auto* box = new FamilyAlertWindow (adding ? "Insert Time Signature" : "Time Signature",
                                       bar == 0 ? juce::String ("The song's first signature (bar 1: it can't be moved).")
                                                : juce::String ("A signature holds from its bar to the next one. Type it as 7/8, and the bar it starts at."),
                                       juce::MessageBoxIconType::NoIcon);
    box->addTextEditor ("signature", juce::String (now.numerator) + "/" + juce::String (now.denominator), "SIGNATURE");
    if (bar > 0)
        box->addTextEditor ("bar", juce::String (bar + 1), "BAR");
    box->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    box->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    box->enterModalState (true, juce::ModalCallbackFunction::create ([box, bar, adding, safe = juce::Component::SafePointer<ArrangeView> (this)] (int result)
    {
        if (safe == nullptr || result != 1)
            return;
        int numerator = 0, denominator = 0;
        if (! pr7s::parseSignature (box->getTextEditorContents ("signature").toRawUTF8(), numerator, denominator))
            return;
        auto at = bar;
        if (bar > 0)
            at = juce::jmax (1, box->getTextEditorContents ("bar").getIntValue() - 1);
        auto& studio = safe->studio;
        if (! adding && at != bar)
            studio.moveSignature (bar, at);
        studio.setSignatureAt (at, numerator, denominator);
        safe->signatureSelection = { at };
        safe->repaint();
    }), true);
}

void ArrangeView::showSignatureMenu()
{
    const auto& project = studio.getProject();
    juce::PopupMenu menu;
    menu.addSectionHeader ("SIGNATURE TRACK");
    const auto selected = signatureSelection.size() == 1 ? signatureSelection.front() : -1;
    menu.addItem (1, "Edit This Signature...", selected >= 0 && (selected == 0 || ! project.signatureLocked));
    menu.addItem (2, "Insert Signature at the Cursor's Bar...", ! project.signatureLocked);
    menu.addItem (3, signatureSelection.size() > 1 ? "Delete Them" : "Delete",
                  ! project.signatureLocked && std::any_of (signatureSelection.begin(), signatureSelection.end(), [] (int b) { return b > 0; }));
    menu.addSeparator();
    menu.addItem (4, "Lock", true, project.signatureLocked);
    menu.addItem (5, "Open Tempo Track Editor (Ctrl+T)", onOpenTempoEditor != nullptr);
    menu.addSeparator();
    menu.addItem (6, "Remove the Signature Track (its signatures stay)");
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<ArrangeView> (this), selected] (int chosen)
    {
        if (safe == nullptr)
            return;
        auto& studio = safe->studio;
        switch (chosen)
        {
            case 1: safe->editSignature (selected, false); return;
            case 2: safe->editSignature (studio.getProject().signatures().barAt (studio.getEngine().getPosition()), true); return;
            case 3: safe->deleteSelectedSignatures(); return;
            case 4: studio.setSignatureLocked (! studio.getProject().signatureLocked); safe->repaint(); return;
            case 5: if (safe->onOpenTempoEditor) safe->onOpenTempoEditor(); return;
            case 6: studio.removeSignatureTrack(); return;
            default: return;
        }
    });
}
