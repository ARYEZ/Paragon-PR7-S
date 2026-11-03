#include "AutomationPanel.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    using Settings = pr7s::Studio::AutomationSettings;
    using Group = Settings::Group;
    const juce::Colour pageColour { 0xff0d0a0e };
    constexpr const char* groupNames[] = { "VOLUME", "PAN", "MUTE", "SENDS", "PLUG-INS" };
    constexpr const char* fillNames[] = { "TO PUNCH", "TO START", "TO END", "LOOP", "GAPS" };
}

juce::TextButton& AutomationPanel::makeKey (const juce::String& text, std::function<void()> onClick, const juce::String& tip)
{
    auto key = std::make_unique<juce::TextButton> (text);
    key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    key->setWantsKeyboardFocus (false);
    key->onClick = [this, action = std::move (onClick)] { action(); refresh(); };   // (the keys light at once, not when the studio's message comes)
    key->setTooltip (tip);
    addAndMakeVisible (*key);
    keys.push_back (std::move (key));
    return *keys.back();
}

AutomationPanel::AutomationPanel (pr7s::Studio& s) : studio (s)
{
    operationsTab = &makeKey ("OPERATIONS", [this] { setTab (Tab::operations); });
    settingsTab = &makeKey ("SETTINGS", [this] { setTab (Tab::settings); });

    readKey = &makeKey ("READ", [this] { studio.setAutomationReadAll (! studio.anyAutomationRead()); }, "Read automation on all tracks on or off");
    writeKey = &makeKey ("WRITE", [this] { studio.setAutomationWriteAll (! studio.anyAutomationWrite()); }, "Write automation on all tracks on or off");
    suspendKey = &makeKey ("SUSPEND", [this] { studio.setAllSuspended (! studio.isAllSuspended()); },
                           "Suspend all reading and writing of automation (every control is yours by hand)");
    const char* modeNames[] = { "TOUCH", "AUTO-LATCH", "CROSS-OVER" };
    const char* modeTips[] = { "Writes while you hold a control; let go and it returns to what was there (over the Return Time)",
                               "Writes while you hold a control; let go and its last value goes on being written until playback stops or W goes off",
                               "As Auto-Latch; touch it again and move it back across the old curve: it punches out where it crosses" };
    for (int m = 0; m < 3; ++m)
        modeKeys[(size_t) m] = &makeKey (modeNames[m], [this, m] { studio.setAutomationMode ((Settings::Mode) m); }, modeTips[m]);
    trimKey = &makeKey ("TRIM", [this] { studio.setTrim (! studio.getAutomationSettings().trim); },
                        "Trim: volume and send levels write (and edit) a trim curve over their automation, keeping it as it was");
    functionsKey = &makeKey ("FUNCTIONS", [this] { showFunctions(); });
    functionsKey->getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);

    for (int f = 0; f < 5; ++f)
        fillKeys[(size_t) f] = &makeKey (fillNames[f], [this, f] { studio.stepFill ((pr7s::Studio::FillOption) f); },
                                         "Click: once (the next pass); again: locked (every pass); again: off");
    for (int g = 0; g < 5; ++g)
    {
        readKeys[(size_t) g] = &makeKey (groupNames[g], [this, g]
        {
            studio.setSuspendRead ((Group) g, ! studio.getAutomationSettings().suspendRead[(size_t) g]);
        }, "Suspend reading: this kind of control isn't moved by its automation (yours by hand)");
        writeKeys[(size_t) g] = &makeKey (groupNames[g], [this, g]
        {
            studio.setSuspendWrite ((Group) g, ! studio.getAutomationSettings().suspendWrite[(size_t) g]);
        }, "Suspend writing: this kind of control isn't written (it punches out of a pass)");
        showKeys[(size_t) g] = &makeKey (groupNames[g], [this, g] { studio.showAutomationGroup ((Group) g, usedOnly); },
                                         "Show this kind of lane on every track (USED ONLY: those with automation)");
    }
    const auto anyOf = [] (const std::array<bool, Settings::numGroups>& a) { return std::any_of (a.begin(), a.end(), [] (bool b) { return b; }); };
    readKeys[5] = &makeKey ("SET ALL", [this, anyOf] { studio.setAllSuspended (true, ! anyOf (studio.getAutomationSettings().suspendRead)); });
    writeKeys[5] = &makeKey ("SET ALL", [this, anyOf] { studio.setAllSuspended (false, ! anyOf (studio.getAutomationSettings().suspendWrite)); });
    usedOnlyKey = &makeKey ("USED ONLY", [this] { usedOnly = ! usedOnly; refresh(); });
    showUsedKey = &makeKey ("SHOW USED", [this] { studio.showAllUsedAutomation(); }, "Every lane with automation, on every track");
    hideAllKey = &makeKey ("HIDE ALL", [this] { studio.hideAllAutomation(); });

    // SETTINGS
    const auto change = [this] (std::function<void (Settings&)> edit)
    {
        auto settings = studio.getAutomationSettings();
        edit (settings);
        studio.setAutomationSettings (settings);
    };
    for (auto* toggle : { &virgin, &continueOnJump, &reveal })
    {
        toggle->setColour (juce::ToggleButton::textColourId, ParagonColours::text);
        toggle->setColour (juce::ToggleButton::tickColourId, ParagonColours::accent);
        addAndMakeVisible (*toggle);
    }
    virgin.setTooltip ("Gaps: no curve before a lane's first point or after its last (the control stays as you set it); Delete makes a gap");
    virgin.onClick = [this, change] { change ([this] (Settings& a) { a.virginTerritory = virgin.getToggleState(); }); };
    continueOnJump.onClick = [this, change] { change ([this] (Settings& a) { a.continueOnJump = continueOnJump.getToggleState(); }); };
    reveal.onClick = [this, change] { change ([this] (Settings& a) { a.revealOnWrite = reveal.getToggleState(); }); };
    for (auto* slider : { &returnTime, &reduction })
    {
        slider->setSliderStyle (juce::Slider::LinearBar);
        slider->setColour (juce::Slider::trackColourId, ParagonColours::accent.withAlpha (0.6f));
        slider->setColour (juce::Slider::textBoxTextColourId, ParagonColours::text);
        addAndMakeVisible (*slider);
    }
    returnTime.setRange (0.0, 1000.0, 1.0);
    returnTime.setTextValueSuffix (" ms");
    returnTime.onValueChange = [this, change] { change ([this] (Settings& a) { a.returnSeconds = returnTime.getValue() / 1000.0; }); };
    reduction.setRange (0.0, 100.0, 1.0);
    reduction.setTextValueSuffix (" %");
    reduction.onValueChange = [this, change] { change ([this] (Settings& a) { a.reductionLevel = (int) reduction.getValue(); }); };
    for (auto* label : { &returnLabel, &reductionLabel, &freezeLabel })
    {
        label->setFont (ParagonLookAndFeel::textFont (13.0f));
        label->setColour (juce::Label::textColourId, ParagonColours::textDim);
        addAndMakeVisible (*label);
    }
    returnLabel.setText ("RETURN TIME", juce::dontSendNotification);
    reductionLabel.setText ("REDUCTION LEVEL", juce::dontSendNotification);
    freezeLabel.setText ("FREEZE TRIM", juce::dontSendNotification);
    freezeTrim.addItem ("MANUALLY", 1);
    freezeTrim.addItem ("ON PASS END", 2);
    freezeTrim.addItem ("ON LEAVING TRIM MODE", 3);
    freezeTrim.onChange = [this, change] { change ([this] (Settings& a) { a.freezeTrim = (Settings::FreezeTrim) (freezeTrim.getSelectedId() - 1); }); };
    addAndMakeVisible (freezeTrim);

    studio.addChangeListener (this);
    setSize (preferredWidth, preferredHeight);
    refresh();
}

AutomationPanel::~AutomationPanel()
{
    studio.removeChangeListener (this);
}

juce::TextButton* AutomationPanel::keyNamed (const juce::String& text)
{
    for (auto& key : keys)
        if (key->getButtonText() == text && key->isVisible())
            return key.get();
    return nullptr;
}

void AutomationPanel::setTab (Tab t)
{
    tab = t;
    resized();
    refresh();
    repaint();
}

void AutomationPanel::refresh()
{
    const auto& a = studio.getAutomationSettings();
    operationsTab->setToggleState (tab == Tab::operations, juce::dontSendNotification);
    settingsTab->setToggleState (tab == Tab::settings, juce::dontSendNotification);
    readKey->setToggleState (studio.anyAutomationRead(), juce::dontSendNotification);
    writeKey->setToggleState (studio.anyAutomationWrite(), juce::dontSendNotification);
    suspendKey->setToggleState (studio.isAllSuspended(), juce::dontSendNotification);
    for (int m = 0; m < 3; ++m)
        modeKeys[(size_t) m]->setToggleState ((int) a.mode == m, juce::dontSendNotification);
    trimKey->setToggleState (a.trim, juce::dontSendNotification);
    for (int f = 0; f < 5; ++f)
    {
        const auto state = studio.getFill ((pr7s::Studio::FillOption) f);
        fillKeys[(size_t) f]->setToggleState (state != Settings::Fill::off, juce::dontSendNotification);
        fillKeys[(size_t) f]->setButtonText (juce::String (fillNames[f]) + (state == Settings::Fill::locked ? " (LOCKED)" : ""));
    }
    fillKeys[4]->setEnabled (a.virginTerritory);   // (gaps are virgin territory's)
    for (int g = 0; g < 5; ++g)
    {
        readKeys[(size_t) g]->setToggleState (a.suspendRead[(size_t) g], juce::dontSendNotification);
        writeKeys[(size_t) g]->setToggleState (a.suspendWrite[(size_t) g], juce::dontSendNotification);
    }
    usedOnlyKey->setToggleState (usedOnly, juce::dontSendNotification);
    virgin.setToggleState (a.virginTerritory, juce::dontSendNotification);
    continueOnJump.setToggleState (a.continueOnJump, juce::dontSendNotification);
    reveal.setToggleState (a.revealOnWrite, juce::dontSendNotification);
    returnTime.setValue (a.returnSeconds * 1000.0, juce::dontSendNotification);
    reduction.setValue (a.reductionLevel, juce::dontSendNotification);
    freezeTrim.setSelectedId ((int) a.freezeTrim + 1, juce::dontSendNotification);
}

void AutomationPanel::showFunctions()
{
    using Scope = pr7s::Studio::AutomationScope;
    juce::PopupMenu menu;
    auto& s = studio;
    menu.addItem ("Delete All Automation in Project", [&s] { s.deleteAllAutomation(); });
    menu.addItem ("Delete Automation of Selected Tracks", [&s] { s.deleteAutomationOfSelectedTracks(); });
    menu.addItem ("Delete Automation in Range", [&s] { s.deleteAutomationInRange(); });
    menu.addSeparator();
    const auto virginOn = studio.getAutomationSettings().virginTerritory;
    menu.addItem ("Fill Gaps on Selected Tracks", virginOn, false, [&s] { s.fillGaps (false); });
    menu.addItem ("Fill Gaps with Current Value (Selected Tracks)", virginOn, false, [&s] { s.fillGaps (true); });
    menu.addSeparator();
    menu.addItem ("Freeze All Trim Automation in Project", [&s] { s.freezeTrim (false); });
    menu.addItem ("Freeze Trim Automation of Selected Tracks", [&s] { s.freezeTrim (true); });
    menu.addSeparator();
    menu.addItem ("Copy Automation of All Tracks", [&s] { s.copyAutomation (Scope::allTracks); });
    menu.addItem ("Copy Automation of Selected Tracks", [&s] { s.copyAutomation (Scope::selectedTracks); });
    menu.addItem ("Paste Automation of All Tracks", studio.hasAutomationClip(), false, [&s] { s.pasteAutomation (Scope::allTracks); });
    menu.addItem ("Paste Automation of Selected Tracks", studio.hasAutomationClip(), false, [&s] { s.pasteAutomation (Scope::selectedTracks); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (functionsKey));
}

void AutomationPanel::paint (juce::Graphics& g)
{
    g.fillAll (pageColour);
    g.setFont (ParagonLookAndFeel::titleFont (13.0f));
    for (const auto& [text, area] : headings)
    {
        g.setColour (ParagonColours::accent);
        g.drawText (text, area, juce::Justification::centredLeft, false);
        g.setColour (ParagonColours::panelEdge);
        g.fillRect (area.withY (area.getBottom() - 1).withHeight (1));
    }
}

void AutomationPanel::resized()
{
    headings.clear();
    auto r = getLocalBounds().reduced (14, 12);
    auto tabs = r.removeFromTop (30);
    operationsTab->setBounds (tabs.removeFromLeft (130).reduced (2));
    settingsTab->setBounds (tabs.removeFromLeft (110).reduced (2));
    r.removeFromTop (10);
    const auto operations = tab == Tab::operations;
    for (auto& key : keys)
        if (key.get() != operationsTab && key.get() != settingsTab)
            key->setVisible (operations);
    for (auto* c : std::initializer_list<juce::Component*> { &virgin, &continueOnJump, &reveal, &returnTime, &reduction, &returnLabel, &reductionLabel, &freezeLabel, &freezeTrim })
        c->setVisible (! operations);
    if (! operations)
    {
        const auto row = [&r] (int h) { auto line = r.removeFromTop (h); r.removeFromTop (8); return line; };
        virgin.setBounds (row (28));
        continueOnJump.setBounds (row (28));
        reveal.setBounds (row (28));
        auto line = row (28);
        returnLabel.setBounds (line.removeFromLeft (170));
        returnTime.setBounds (line.removeFromLeft (300));
        line = row (28);
        reductionLabel.setBounds (line.removeFromLeft (170));
        reduction.setBounds (line.removeFromLeft (300));
        line = row (28);
        freezeLabel.setBounds (line.removeFromLeft (170));
        freezeTrim.setBounds (line.removeFromLeft (300));
        return;
    }
    // The top row: READ WRITE SUSPEND | TOUCH AUTO-LATCH CROSS-OVER | TRIM | FUNCTIONS.
    auto top = r.removeFromTop (34);
    for (auto* key : { readKey, writeKey, suspendKey })
        key->setBounds (top.removeFromLeft (84).reduced (2));
    top.removeFromLeft (14);
    for (auto* key : modeKeys)
        key->setBounds (top.removeFromLeft (104).reduced (2));
    top.removeFromLeft (14);
    trimKey->setBounds (top.removeFromLeft (70).reduced (2));
    functionsKey->setBounds (top.removeFromRight (120).reduced (2));
    r.removeFromTop (12);
    const auto section = [&] (const juce::String& title, const auto& sectionKeys, int keyWidth)
    {
        headings.push_back ({ title, r.removeFromTop (20) });
        r.removeFromTop (4);
        auto line = r.removeFromTop (32);
        for (auto* key : sectionKeys)
            if (key != nullptr)
                key->setBounds (line.removeFromLeft (keyWidth).reduced (2));
        r.removeFromTop (10);
        return line;
    };
    section ("FILL", fillKeys, 140);
    section ("SUSPEND READ", readKeys, 112);
    section ("SUSPEND WRITE", writeKeys, 112);
    auto showLine = section ("SHOW", showKeys, 100);
    usedOnlyKey->setBounds (showLine.removeFromLeft (100).reduced (2));
    showUsedKey->setBounds (showLine.removeFromLeft (110).reduced (2));
    hideAllKey->setBounds (showLine.removeFromLeft (100).reduced (2));
}

//==============================================================================
AutomationPanelWindow::AutomationPanelWindow (pr7s::Studio& s)
    : DocumentWindow ("Paragon PR7-S: automation", pageColour, DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    panel = new AutomationPanel (s);
    setContentOwned (panel, true);
    setResizable (true, false);
    setResizeLimits (AutomationPanel::preferredWidth, 360, 3000, 2000);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
}

void AutomationPanelWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}
