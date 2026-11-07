#include "ui/PluginsPage.h"
#include "ui/HubFrame.h"

#include "engine/PluginHost.h"
#include "ui/Finish.h"
#include "ui/Logo.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/Pearl.h"

namespace
{
    const auto onyx = juce::Colour (0xff0c0a0d);          // (the Hub's)
    const auto panelColour = juce::Colour (0xff120e13);
    enum Column { name = 1, format, category, maker, version };
}

// The rows as the Hub's songs: the name large, the rest dimmed; the chosen row lit with a bar at its left.
class PluginsPage::Model final : public juce::TableListBoxModel
{
public:
    Model (juce::PluginListComponent& o, juce::KnownPluginList& l) : owner (o), list (l) {}

    int getNumRows() override { return list.getNumTypes() + list.getBlacklistedFiles().size(); }

    void paintRowBackground (juce::Graphics& g, int, int width, int height, bool selected) override
    {
        g.fillAll (panelColour);
        if (selected)
        {
            g.setColour (ParagonColours::accent.withAlpha (0.22f));
            g.fillRect (0, 0, width, height);
        }
    }

    void paintCell (juce::Graphics& g, int row, int column, int width, int height, bool selected) override
    {
        if (selected && column == Column::name)
        {
            g.setColour (ParagonColours::accent.withAlpha (0.7f));
            g.fillRect (0, 0, 3, height);
        }
        const auto text = cellText (list, row, column);
        if (text.isEmpty())
            return;
        const auto deactivated = row >= list.getNumTypes();
        if (column == Column::name)
        {
            g.setColour (deactivated ? ParagonColours::accent : ParagonColours::text);
            g.setFont (ParagonLookAndFeel::titleFont (17.0f));
        }
        else
        {
            g.setColour (deactivated ? ParagonColours::accent : ParagonColours::textDim);
            g.setFont (ParagonLookAndFeel::textFont (13.0f));
        }
        g.drawFittedText (text, column == Column::name ? 14 : 8, 0, width - (column == Column::name ? 20 : 14), height,
                          column == Column::version ? juce::Justification::centredRight : juce::Justification::centredLeft, 1, 0.9f);
    }

    void cellClicked (int row, int column, const juce::MouseEvent& e) override
    {
        juce::TableListBoxModel::cellClicked (row, column, e);
        if (row >= 0 && row < getNumRows() && e.mods.isPopupMenu())
            owner.createMenuForRow (row).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (e.originalComponent)
                                                                                  .withMousePosition()
                                                                                  .withDeletionCheck (owner));
    }

    void deleteKeyPressed (int) override { owner.removeSelectedPlugins(); }

    void sortOrderChanged (int column, bool forwards) override
    {
        switch (column)
        {
            case Column::name:     list.sort (juce::KnownPluginList::sortAlphabetically, forwards); break;
            case Column::format:   list.sort (juce::KnownPluginList::sortByFormat, forwards); break;
            case Column::category: list.sort (juce::KnownPluginList::sortByCategory, forwards); break;
            case Column::maker:    list.sort (juce::KnownPluginList::sortByManufacturer, forwards); break;
            default: break;
        }
    }

private:
    juce::PluginListComponent& owner;
    juce::KnownPluginList& list;
};

juce::String PluginsPage::cellText (const juce::KnownPluginList& list, int row, int column)
{
    if (row >= list.getNumTypes())
    {
        const auto files = list.getBlacklistedFiles();
        if (column == Column::name)
            return files[row - list.getNumTypes()];
        return column == Column::category ? juce::String ("DEACTIVATED: IT FAILED WHEN LOADED") : juce::String();
    }
    const auto types = list.getTypes();
    if (! juce::isPositiveAndBelow (row, (int) types.size()))
        return {};
    const auto& d = types[(size_t) row];
    switch (column)
    {
        case Column::name:     return d.name;
        case Column::format:   return d.pluginFormatName;
        case Column::category: return d.category.isNotEmpty() ? d.category.replace ("|", "  -  ").toUpperCase() : juce::String ("-");
        case Column::maker:    return d.manufacturerName;
        case Column::version:  return d.version.isNotEmpty() ? "V" + d.version : juce::String();
        default:               return {};
    }
}

PluginsPage::PluginsPage (pr7s::Studio& s) : studio (s)
{
    auto& host = studio.getPluginHost();
    list = std::make_unique<juce::PluginListComponent> (host.getFormats(), host.getKnownPlugins(), pr7s::PluginHost::deadMansPedalFile(),
                                                        studio.getSettings());
    list->setOptionsButtonText ("SCAN / OPTIONS");
    list->setTableModel (new Model (*list, host.getKnownPlugins()));
    auto& table = list->getTableListBox();
    table.setRowHeight (40);
    table.setHeaderHeight (30);
    table.setColour (juce::ListBox::backgroundColourId, panelColour);
    table.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    list->setColour (juce::ListBox::backgroundColourId, panelColour);
    // The Hub's columns: the name widest; the rest as wide as they need.
    auto& columns = table.getHeader();
    columns.setColumnName (Column::name, "Name");
    columns.setColumnName (Column::category, "Kind");
    columns.setColumnName (Column::maker, "Maker");
    columns.setColumnName (Column::version, "Version");
    columns.setColumnWidth (Column::name, 260);
    columns.setColumnWidth (Column::format, 70);
    columns.setColumnWidth (Column::category, 190);
    columns.setColumnWidth (Column::maker, 230);
    columns.setColumnWidth (Column::version, 110);
    addAndMakeVisible (*list);
    setSize (960, 720);
}

void PluginsPage::paint (juce::Graphics& g)
{
    const auto found = studio.getPluginHost().getKnownPlugins().getNumTypes();
    HubFrame::paint (g, getLocalBounds(), satin, "PLUG-INS  -  VST3 INSTRUMENTS AND EFFECTS  -  " + juce::String (found) + " FOUND");
}

void PluginsPage::resized()
{
    satin = HubFrame::makeSatin (getWidth());
    list->setBounds (HubFrame::contentOf (getLocalBounds()));
}
