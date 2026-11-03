#include "ui/MediaUi.h"

namespace MediaUi
{
    AttributeFilterColumn::AttributeFilterColumn (AttributeFilterHost& h, int i) : host (h), index (i)
    {
        styleKey (title);
        title.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
        title.setTooltip ("Which attribute this column filters by");
        title.onClick = [this] { showAttributeMenu(); };
        addAndMakeVisible (title);
        list.setModel (this);
        list.setRowHeight (22);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff100d11));
        list.setOutlineThickness (0);
        addAndMakeVisible (list);
    }

    AttributeFilterColumn::~AttributeFilterColumn() { list.setModel (nullptr); }

    void AttributeFilterColumn::refresh()
    {
        values = host.getFilterValues (index);
        title.setButtonText (host.getFilterColumn (index).name());
        list.updateContent();
        list.repaint();
    }

    void AttributeFilterColumn::resized()
    {
        auto area = getLocalBounds();
        title.setBounds (area.removeFromTop (26));
        area.removeFromTop (4);
        list.setBounds (area);
    }

    int AttributeFilterColumn::getNumRows() { return (int) values.size(); }

    void AttributeFilterColumn::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool)
    {
        if (! juce::isPositiveAndBelow (row, (int) values.size()))
            return;
        const auto& [value, count] = values[(size_t) row];
        const auto chosen = host.getFilterSelection (index).contains (value);
        if (chosen)
        {
            g.setColour (ParagonColours::accent.withAlpha (0.22f));
            g.fillRect (0, 0, width, height);
            g.setColour (ParagonColours::accent);
            g.fillRect (0, 0, 3, height);
        }
        auto area = juce::Rectangle<int> (0, 0, width, height).reduced (8, 0);
        g.setFont (interFont (12.0f));
        g.setColour (ParagonColours::textDim);
        g.drawText (juce::String (count), area.removeFromRight (40), juce::Justification::centredRight, false);
        const auto attribute = host.getFilterColumn (index);
        g.setColour (chosen ? ParagonColours::text : ParagonColours::text.withAlpha (0.85f));
        if (attribute.kind == pr7s::MediaAttribute::rating)
            g.setColour (pr7s::FamilyColours::orange);
        g.drawText (shownValue (host.getFilterDatabase(), attribute, value), area, juce::Justification::centredLeft, true);
    }

    void AttributeFilterColumn::listBoxItemClicked (int row, const juce::MouseEvent& event)
    {
        if (juce::isPositiveAndBelow (row, (int) values.size()))
            host.clickFilterValue (index, values[(size_t) row].first, event.mods.isCommandDown());
        list.deselectAllRows();
    }

    void AttributeFilterColumn::showAttributeMenu()
    {
        juce::PopupMenu menu;
        const auto all = filterableAttributes (host.getFilterDatabase());
        const auto current = host.getFilterColumn (index);
        for (int i = 0; i < (int) all.size(); ++i)
        {
            if (all[(size_t) i].kind == pr7s::MediaAttribute::user && (i == 0 || all[(size_t) i - 1].kind != pr7s::MediaAttribute::user))
                menu.addSectionHeader ("YOUR ATTRIBUTES");
            menu.addItem (i + 1, all[(size_t) i].name(), true, all[(size_t) i] == current);
        }
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&title),
                            [safe = juce::Component::SafePointer<AttributeFilterColumn> (this), all] (int chosen)
                            {
                                if (safe != nullptr && chosen > 0)
                                    safe->host.setFilterColumn (safe->index, all[(size_t) chosen - 1]);
                            });
    }
}
