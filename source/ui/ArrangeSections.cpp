// The arrangement's SECTIONS row (0.90.0, the owner's song engine): the song as blocks, each a section's name and
// its length in bars, end to end from bar 1. Clicked, dragged to another place in the order (the music in it goes
// with it: Studio::moveSongSection), double-clicked to open the composer on it, right-clicked for its menu.

#include "ArrangeView.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    const juce::Colour pearl { 0xfff4eeea };
    constexpr float dragStart = 6.0f;   // px before a press becomes a drag
    const int lengthChoices[] { 1, 2, 4, 6, 8, 12, 16, 24, 32 };

    // A block's colour by its kind (the family's tints only): verses orange, choruses pink, bridges and breakdowns
    // red, intros and endings pale, solos a deep pink.
    juce::Colour sectionColour (const juce::String& name)
    {
        switch (pr7s::composerSectionFor (name))
        {
            case 0: return pr7s::FamilyColours::paletteColour (4, 6);   // intro
            case 1: return pr7s::FamilyColours::orange;                 // verse (pre-chorus, riff)
            case 2: return pr7s::FamilyColours::pink;                   // chorus (and its kin)
            case 3: return pr7s::FamilyColours::red;                    // bridge
            case 4: return pr7s::FamilyColours::paletteColour (4, 1);   // breakdown
            case 5: return pr7s::FamilyColours::paletteColour (6, 6);   // ending
            case 6: return pr7s::FamilyColours::paletteColour (1, 3);   // solo
            default: return pr7s::FamilyColours::orange;
        }
    }
}

std::vector<int> ArrangeView::shownSectionOrder() const
{
    const auto count = (int) studio.getSongSections().blocks.size();
    std::vector<int> order;
    for (int i = 0; i < count; ++i)
        order.push_back (i);
    if (drag == Drag::sectionMove && juce::isPositiveAndBelow (sectionDragFrom, count) && juce::isPositiveAndBelow (sectionDragTo, count))
    {
        order.erase (order.begin() + sectionDragFrom);
        order.insert (order.begin() + sectionDragTo, sectionDragFrom);
    }
    return order;
}

juce::Rectangle<float> ArrangeView::getSectionArea() const
{
    std::vector<Timeline::Row> made;
    for (const auto& row : rowsNow (made))
        if (row.kind == Timeline::Row::Kind::sections)
            return juce::Rectangle<float> (0.0f, rowTop (row), (float) getWidth(), (float) row.height).reduced (0.0f, 3.0f);
    return {};
}

juce::Rectangle<float> ArrangeView::getSectionBounds (int index) const
{
    const auto area = getSectionArea();
    const auto& blocks = studio.getSongSections().blocks;
    if (area.isEmpty() || ! juce::isPositiveAndBelow (index, (int) blocks.size()))
        return {};
    const auto signatures = studio.getProject().signatures();
    auto bar = 0;
    for (const auto i : shownSectionOrder())
    {
        const auto& b = blocks[(size_t) i];
        if (i == index)
        {
            const auto x1 = timeline.xForBeat (signatures.beatOfBar (bar));
            const auto x2 = timeline.xForBeat (signatures.beatOfBar (bar + b.bars));
            return { x1 + 1.0f, area.getY(), juce::jmax (3.0f, x2 - x1 - 2.0f), area.getHeight() };
        }
        bar += b.bars;
    }
    return {};
}

int ArrangeView::sectionBlockAt (juce::Point<float> at) const
{
    for (int i = 0; i < (int) studio.getSongSections().blocks.size(); ++i)
        if (getSectionBounds (i).expanded (1.0f, 0.0f).contains (at))
            return i;
    return -1;
}

int ArrangeView::getSelectedSection() const noexcept
{
    return sectionSelectedId > 0 ? studio.getSongSections().indexOf (sectionSelectedId) : -1;
}

void ArrangeView::selectSection (int index)
{
    const auto& blocks = studio.getSongSections().blocks;
    sectionSelectedId = juce::isPositiveAndBelow (index, (int) blocks.size()) ? blocks[(size_t) index].id : 0;
    repaint();
}

void ArrangeView::paintSections (juce::Graphics& g, const Timeline::Row& row)
{
    const auto area = juce::Rectangle<float> (0.0f, rowTop (row), (float) getWidth(), (float) row.height).reduced (0.0f, 3.0f);
    const auto& blocks = studio.getSongSections().blocks;
    const auto clip = g.getClipBounds().toFloat();
    const auto selected = getSelectedSection();
    if (blocks.empty())
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::titleFont (14.0f));
        g.drawText ("DOUBLE-CLICK HERE OR + SECTION TO ADD THE SONG'S FIRST PART", area.reduced (12.0f, 0.0f), juce::Justification::centredLeft, false);
        return;
    }
    for (int i = 0; i < (int) blocks.size(); ++i)
    {
        const auto bounds = getSectionBounds (i);
        if (bounds.getRight() < clip.getX() || bounds.getX() > clip.getRight())
            continue;
        const auto& b = blocks[(size_t) i];
        const auto tint = sectionColour (b.name);
        const auto dragged = drag == Drag::sectionMove && i == sectionDragFrom;
        const auto isSelected = i == selected;
        g.setColour (tint.withAlpha (isSelected || dragged ? 0.62f : 0.32f));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (dragged ? pearl : tint.withAlpha (isSelected ? 1.0f : 0.85f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, dragged || isSelected ? 2.0f : 1.0f);
        auto text = bounds.reduced (8.0f, 2.0f);
        if (text.getWidth() < 14.0f)
            continue;
        // (0.91.0) A linked copy: two links of a chain at its top right.
        if (b.link > 0 && text.getWidth() > 60.0f)
        {
            const auto h = juce::jmin (10.0f, bounds.getHeight() * 0.28f);
            const auto at = juce::Point<float> (bounds.getRight() - 8.0f - h * 1.6f, bounds.getY() + 4.0f);
            g.setColour (pearl.withAlpha (0.9f));
            g.drawRoundedRectangle ({ at.x, at.y, h * 1.0f, h * 0.62f }, h * 0.3f, 1.4f);
            g.drawRoundedRectangle ({ at.x + h * 0.6f, at.y + h * 0.3f, h * 1.0f, h * 0.62f }, h * 0.3f, 1.4f);
            text.removeFromRight (h * 1.8f);
        }
        g.setColour (isSelected || dragged ? juce::Colours::white : ParagonColours::text);
        g.setFont (ParagonLookAndFeel::titleFont (juce::jmin (18.0f, bounds.getHeight() * 0.55f)));
        const auto label = pr7s::sectionLabel (blocks, i).toUpperCase();
        const auto bars = juce::String (b.bars) + (b.bars == 1 ? " BAR" : " BARS");
        const auto barsWidth = juce::GlyphArrangement::getStringWidth (juce::Font (ParagonLookAndFeel::titleFont (12.0f)), bars) + 6.0f;
        if (text.getWidth() > barsWidth * 2.5f)
        {
            g.drawText (label, text.withTrimmedRight (barsWidth), juce::Justification::centredLeft, true);
            g.setColour (ParagonColours::textDim);
            g.setFont (ParagonLookAndFeel::titleFont (12.0f));
            g.drawText (bars, text, juce::Justification::centredRight, false);
        }
        else
            g.drawText (label, text, juce::Justification::centredLeft, true);
    }
}

bool ArrangeView::sectionDown (const juce::MouseEvent& event)
{
    const auto hit = sectionBlockAt (event.position);
    if (event.mods.isPopupMenu())
    {
        if (hit >= 0)
            selectSection (hit);
        showSectionMenu (hit);
        return true;
    }
    if (hit < 0)
    {
        selectSection (-1);
        return true;
    }
    selectSection (hit);
    if (const auto place = studio.getSongSectionPlace (hit))
        studio.setPosition (place->start);   // (a click goes to the part's start, as a marker's does)
    drag = Drag::sectionMove;
    dragMoved = false;
    sectionDragFrom = sectionDragTo = hit;
    sectionDownX = event.position.x;
    return true;
}

void ArrangeView::sectionDrag (const juce::MouseEvent& event)
{
    if (! dragMoved && std::abs (event.position.x - sectionDownX) < dragStart)
        return;
    dragMoved = true;
    // Its place: after every other block whose middle is left of the mouse.
    const auto places = studio.getSongSectionPlaces();
    auto target = 0;
    for (int i = 0; i < (int) places.size(); ++i)
        if (i != sectionDragFrom)
        {
            const auto middle = timeline.xForBeat ((places[(size_t) i].start + places[(size_t) i].end) * 0.5);
            if (middle < event.position.x)
                ++target;
        }
    if (target != sectionDragTo)
    {
        sectionDragTo = target;
        repaint();
    }
}

void ArrangeView::sectionUp()
{
    const auto from = sectionDragFrom, to = sectionDragTo;
    const auto moved = dragMoved;
    drag = Drag::none;
    dragMoved = false;
    sectionDragFrom = sectionDragTo = -1;
    if (moved && from >= 0 && to >= 0 && from != to)
        studio.moveSongSection (from, to);
    repaint();
}

bool ArrangeView::sectionDoubleClick (const juce::MouseEvent& event)
{
    const auto row = rowAt (event.position.y);
    if (! row || row->kind != Timeline::Row::Kind::sections)
        return false;
    const auto hit = sectionBlockAt (event.position);
    if (hit >= 0)
    {
        if (onOpenSection)
            onOpenSection (hit);
        return true;
    }
    showAddSectionMenu (-1, event.getScreenPosition());
    return true;
}

void ArrangeView::showAddSectionMenu (int insertAt, juce::Point<int> where)
{
    juce::PopupMenu menu;
    menu.addSectionHeader (insertAt < 0 ? "ADD A SECTION AT THE END" : "INSERT A SECTION");
    const auto& names = pr7s::songSectionNames();
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (1 + i, names[i]);
    auto options = juce::PopupMenu::Options();
    if (where != juce::Point<int>())
        options = options.withTargetScreenArea ({ where.x, where.y, 1, 1 });
    menu.showMenuAsync (options, [safe = juce::Component::SafePointer<ArrangeView> (this), insertAt] (int chosen)
    {
        if (safe == nullptr || chosen <= 0 || chosen > pr7s::songSectionNames().size())
            return;
        const auto name = pr7s::songSectionNames()[chosen - 1];
        auto& studio = safe->studio;
        if (insertAt < 0)
            safe->selectSection (studio.appendSongSection (name, 8));
        else if (studio.insertSongSection (insertAt, name, 8))
            safe->selectSection (insertAt);
        safe->repaint();
    });
}

void ArrangeView::showSectionMenu (int index)
{
    const auto& blocks = studio.getSongSections().blocks;
    const auto valid = juce::isPositiveAndBelow (index, (int) blocks.size());
    const auto& names = pr7s::songSectionNames();
    juce::PopupMenu menu;
    if (valid)
    {
        const auto& b = blocks[(size_t) index];
        menu.addSectionHeader (pr7s::sectionLabel (blocks, index).toUpperCase() + ": " + juce::String (b.bars) + " BARS");
        menu.addItem (1, "Open in the Composer (double-click)", onOpenSection != nullptr);
        juce::PopupMenu rename, length, before, after;
        for (int i = 0; i < names.size(); ++i)
        {
            rename.addItem (100 + i, names[i], true, names[i] == b.name);
            before.addItem (1000 + i, names[i]);
            after.addItem (2000 + i, names[i]);
        }
        for (const auto bars : lengthChoices)
            length.addItem (500 + bars, juce::String (bars) + (bars == 1 ? " bar" : " bars"), true, bars == b.bars);
        if (std::find (std::begin (lengthChoices), std::end (lengthChoices), b.bars) == std::end (lengthChoices))
            length.addItem (500 + b.bars, juce::String (b.bars) + " bars", true, true);
        length.addItem (4, "Other...");
        menu.addSubMenu ("Section", rename);
        menu.addSubMenu ("Length (what follows moves)", length);
        menu.addSubMenu ("Insert a Section Before", before);
        menu.addSubMenu ("Insert a Section After", after);
        menu.addItem (2, "Duplicate (a linked copy after it: edit one, both change)");
        menu.addItem (7, "Duplicate as Its Own Copy (not linked)");
        menu.addItem (8, "Unlink (its own from now on; its music stays)", b.link > 0);
        menu.addItem (3, "Set the Locators to It");
        menu.addItem (5, "Delete (and its music; what follows moves up)");
        menu.addSeparator();
    }
    juce::PopupMenu atEnd;
    for (int i = 0; i < names.size(); ++i)
        atEnd.addItem (3000 + i, names[i]);
    menu.addSubMenu ("Add a Section at the End", atEnd);
    menu.addItem (6, "Hide the Sections Row (its blocks stay)");
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<ArrangeView> (this), index] (int chosen)
    {
        if (safe == nullptr || chosen <= 0)
            return;
        auto& studio = safe->studio;
        const auto& names = pr7s::songSectionNames();
        if (chosen == 1 && safe->onOpenSection)
            safe->onOpenSection (index);
        else if (chosen == 2)
        {
            if (studio.duplicateSongSection (index))
                safe->selectSection (index + 1);
        }
        else if (chosen == 7)
        {
            if (studio.duplicateSongSection (index, false))
                safe->selectSection (index + 1);
        }
        else if (chosen == 8)
            studio.unlinkSongSection (index);
        else if (chosen == 3)
        {
            if (const auto place = studio.getSongSectionPlace (index))
                studio.setCycle (place->start, place->end);
        }
        else if (chosen == 4)
        {
            auto* box = new FamilyAlertWindow ("Section Length", "Its length in bars (1 to 64). What follows it moves.", juce::MessageBoxIconType::NoIcon);
            box->addTextEditor ("bars", juce::String (studio.getSongSections().blocks[(size_t) index].bars), "BARS");
            box->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
            box->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            box->enterModalState (true, juce::ModalCallbackFunction::create ([box, index, safe] (int result)
            {
                if (safe != nullptr && result == 1)
                    if (const auto bars = box->getTextEditorContents ("bars").getIntValue(); bars > 0)
                        safe->studio.setSongSectionBars (index, bars);
            }), true);
        }
        else if (chosen == 5)
        {
            studio.removeSongSection (index);
            safe->selectSection (-1);
        }
        else if (chosen == 6)
            studio.removeSongSectionTrack();
        else if (chosen >= 100 && chosen < 100 + names.size())
            studio.renameSongSection (index, names[chosen - 100]);
        else if (chosen > 500 && chosen <= 500 + pr7s::mostSectionBars)
            studio.setSongSectionBars (index, chosen - 500);
        else if (chosen >= 1000 && chosen < 1000 + names.size())
        {
            if (studio.insertSongSection (index, names[chosen - 1000], 8))
                safe->selectSection (index);
        }
        else if (chosen >= 2000 && chosen < 2000 + names.size())
        {
            if (studio.insertSongSection (index + 1, names[chosen - 2000], 8))
                safe->selectSection (index + 1);
        }
        else if (chosen >= 3000 && chosen < 3000 + names.size())
            safe->selectSection (studio.appendSongSection (names[chosen - 3000], 8));
        safe->repaint();
    });
}
