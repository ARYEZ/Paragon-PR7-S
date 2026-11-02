#include "ui/LinkGroupPage.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    juce::FontOptions font (float height, int style = juce::Font::bold) { return ParagonLookAndFeel::textFont (height, style); }
}

const std::array<LinkGroupPage::Option, 9>& LinkGroupPage::options()
{
    using W = pr7s::LinkGroup::What;
    static const std::array<Option, 9> list { {
        { W::volume, "Volume (by the same amount; ABS: the same value)" },
        { W::pan, "Pan" },
        { W::sends, "Sends (the same slot: its FX channel, level, on, pre/post)" },
        { W::inserts, "Inserts (slot by slot: effects, bypass, their knobs)" },
        { W::muteSolo, "Mute / Solo" },
        { W::automation, "Automation Read / Write" },
        { W::routing, "Routing (the output)" },
        { W::selection, "Selection (one clicked selects them all)" },
        { W::recordMonitor, "Monitor (audio tracks; one track records at a time)" },
    } };
    return list;
}

LinkGroupPage::LinkGroupPage (pr7s::Studio& s, int group, std::vector<int> ids) : studio (s), groupId (group), trackIds (std::move (ids))
{
    const auto* existing = studio.getProject().findLinkGroup (groupId);
    const auto start = existing != nullptr ? *existing : pr7s::LinkGroup {};
    auto next = 1;
    for (const auto& g : studio.getProject().linkGroups)
        next = juce::jmax (next, g.id + 1);
    name.setText (existing != nullptr ? existing->name : "Link " + juce::String (next), false);
    name.setFont (font (16.0f, juce::Font::plain));
    name.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff151217));
    name.setColour (juce::TextEditor::textColourId, ParagonColours::text);
    name.setColour (juce::TextEditor::outlineColourId, ParagonColours::panelEdge);
    name.setColour (juce::TextEditor::focusedOutlineColourId, ParagonColours::accent);
    addAndMakeVisible (name);
    for (const auto& option : options())
    {
        auto* box = boxes.add (new juce::ToggleButton (option.label));
        box->setToggleState (start.links (option.what), juce::dontSendNotification);
        box->setColour (juce::ToggleButton::textColourId, ParagonColours::text);
        box->setColour (juce::ToggleButton::tickColourId, ParagonColours::accent);
        box->setWantsKeyboardFocus (false);
        addAndMakeVisible (box);
    }
    note.setText (existing != nullptr ? juce::String (studio.getProject().linkGroupMembers (groupId).size()) + " channels. Hold Alt (or SUS in the mixer) "
                                        "to change one on its own."
                                      : juce::String ((int) trackIds.size()) + " channels. Hold Alt (or SUS in the mixer) to change one on its own.",
                  juce::dontSendNotification);
    note.setFont (font (12.5f, juce::Font::plain));
    note.setColour (juce::Label::textColourId, ParagonColours::textDim);
    addAndMakeVisible (note);
    okKey.setButtonText (existing != nullptr ? "OK" : "LINK");
    for (auto* key : { &okKey, &cancelKey })
    {
        key->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        key->setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        addAndMakeVisible (*key);
    }
    okKey.setColour (juce::TextButton::textColourOffId, ParagonColours::accent);
    okKey.onClick = [this] { apply(); if (onClose) onClose(); };
    cancelKey.onClick = [this] { if (onClose) onClose(); };
    setSize (460, 120 + (int) options().size() * 28 + 80);
}

pr7s::LinkGroup LinkGroupPage::getSettings() const
{
    pr7s::LinkGroup settings;
    settings.name = name.getText().trim();
    for (int i = 0; i < (int) options().size(); ++i)
        settings.setLinks (options()[(size_t) i].what, boxes[i]->getToggleState());
    return settings;
}

void LinkGroupPage::setLinks (pr7s::LinkGroup::What what, bool on)
{
    for (int i = 0; i < (int) options().size(); ++i)
        if (options()[(size_t) i].what == what)
            boxes[i]->setToggleState (on, juce::dontSendNotification);
}

void LinkGroupPage::apply()
{
    if (studio.getProject().findLinkGroup (groupId) != nullptr)
        studio.setLinkGroupSettings (groupId, getSettings());
    else
        studio.linkTracks (trackIds, getSettings());
}

void LinkGroupPage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0a0d));
    g.setColour (ParagonColours::accent);
    g.fillRect (0, 0, getWidth(), 3);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (15.0f));
    g.drawText (studio.getProject().findLinkGroup (groupId) != nullptr ? "LINK GROUP SETTINGS" : "LINK SELECTED CHANNELS",
                getLocalBounds().reduced (18, 0).withTrimmedTop (12).withHeight (24), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (font (12.0f));
    g.drawText ("NAME", getLocalBounds().reduced (18, 0).withTrimmedTop (42).withHeight (16), juce::Justification::centredLeft, false);
    g.drawText ("LINKED", getLocalBounds().reduced (18, 0).withTrimmedTop (100).withHeight (16), juce::Justification::centredLeft, false);
}

void LinkGroupPage::resized()
{
    auto area = getLocalBounds().reduced (18, 0);
    area.removeFromTop (60);
    name.setBounds (area.removeFromTop (30));
    area.removeFromTop (30);
    for (auto* box : boxes)
        box->setBounds (area.removeFromTop (28));
    area.removeFromTop (6);
    auto keys = area.removeFromBottom (48).withSizeKeepingCentre (area.getWidth(), 32);
    note.setBounds (area);
    cancelKey.setBounds (keys.removeFromRight (90));
    keys.removeFromRight (8);
    okKey.setBounds (keys.removeFromRight (90));
}

void LinkGroupPage::show (juce::Component& around, pr7s::Studio& studio, int groupId, std::vector<int> trackIds)
{
    auto* page = new LinkGroupPage (studio, groupId, std::move (trackIds));
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (page);
    options.dialogTitle = "Paragon PR7-S: Link Group Settings";
    options.dialogBackgroundColour = juce::Colour (0xff0c0a0d);
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.componentToCentreAround = &around;
    options.launchAsync();
    page->onClose = [page]
    {
        if (auto* dialog = page->findParentComponentOfClass<juce::DialogWindow>())
            dialog->exitModalState (0);   // deleted once its click is over
    };
}

bool LinkGroupPage::linkOrUnlinkSelected (juce::Component& around, pr7s::Studio& studio)
{
    const auto selected = studio.getSelectedTracks();
    if (selected.empty())
        return false;
    // All the selected in one group: unlink it (the manual: select a linked channel, click Link).
    const auto* group = studio.getLinkGroupOf (selected.front());
    if (group != nullptr && std::all_of (selected.begin(), selected.end(), [&] (int id) { return studio.getProject().findTrack (id)->linkGroup == group->id; }))
    {
        studio.removeLinkGroup (group->id);
        return true;
    }
    if (selected.size() < 2)
        return false;
    show (around, studio, 0, selected);
    return true;
}
