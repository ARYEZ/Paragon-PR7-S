#include "FollowSetupPage.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    const juce::Colour pageColour { 0xff0d0a0e };
    constexpr int radioGroup = 3601;

    void styleNote (juce::Label& label, const juce::String& text, bool heading = false)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (ParagonLookAndFeel::textFont (heading ? 14.0f : 13.0f));
        label.setColour (juce::Label::textColourId, heading ? ParagonColours::text : ParagonColours::textDim);
        label.setJustificationType (juce::Justification::topLeft);
        label.setMinimumHorizontalScale (1.0f);
    }

    void styleToggle (juce::ToggleButton& t)
    {
        t.setColour (juce::ToggleButton::textColourId, ParagonColours::text);
        t.setColour (juce::ToggleButton::tickColourId, ParagonColours::accent);
        t.setColour (juce::ToggleButton::tickDisabledColourId, ParagonColours::textDim);
        t.setRadioGroupId (radioGroup, juce::dontSendNotification);
        t.setWantsKeyboardFocus (false);
    }

    const std::vector<pr7s::Chord::Type>& types()
    {
        return pr7s::Chord::allTypes();
    }
}

FollowSetupPage::FollowSetupPage()
{
    for (auto* t : { &directly, &analyze, &known })
    {
        styleToggle (*t);
        t->onClick = [this] { refreshEnabled(); };
        addAndMakeVisible (*t);
    }
    directly.setToggleState (true, juce::dontSendNotification);
    styleNote (directlyNote, "The notes already go with the chord track's chords (made from them, or played over them). "
                             "When the chords change, the notes change with them.");
    styleNote (syncLabel, "SYNCHRONIZE THE TRACK WITH THE CHORD TRACK FIRST", true);
    styleNote (analyzeNote, "The notes have nothing to do with the chord track: their own chords are found in them first "
                            "(as Create Chord Events), then they're moved onto the chord track's.");
    styleNote (knownNote, "The notes stay on one chord all the way (no chord changes in them): say which.");
    styleNote (rootLabel, "ROOT NOTE", true);
    styleNote (typeLabel, "CHORD TYPE", true);
    for (int p = 0; p < 12; ++p)
        root.addItem (pr7s::Chord::noteName (p, p == 3 || p == 8 || p == 10), p + 1);
    for (size_t i = 0; i < types().size(); ++i)
        type.addItem (pr7s::Chord::typeName (types()[i]).toUpperCase(), (int) i + 1);
    root.setSelectedId (1, juce::dontSendNotification);
    type.setSelectedId (1, juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> { &directlyNote, &syncLabel, &analyzeNote, &knownNote, &rootLabel, &typeLabel,
                                                             &root, &type, &ok, &cancel })
        addAndMakeVisible (*c);
    ok.onClick = [this] { if (onOk) onOk(); };
    cancel.onClick = [this] { if (onCancel) onCancel(); };
    refreshEnabled();
    setSize (460, 360);
}

void FollowSetupPage::refreshEnabled()
{
    root.setEnabled (known.getToggleState());
    type.setEnabled (known.getToggleState());
}

pr7s::FollowStart FollowSetupPage::getStart() const noexcept
{
    return analyze.getToggleState() ? pr7s::FollowStart::analyze
         : known.getToggleState()   ? pr7s::FollowStart::knownChord
                                    : pr7s::FollowStart::directly;
}

void FollowSetupPage::setStart (pr7s::FollowStart start)
{
    (start == pr7s::FollowStart::analyze ? analyze : start == pr7s::FollowStart::knownChord ? known : directly)
        .setToggleState (true, juce::dontSendNotification);
    refreshEnabled();
}

pr7s::Chord FollowSetupPage::getKnownChord() const
{
    pr7s::Chord chord;
    chord.root = juce::jlimit (0, 11, root.getSelectedId() - 1);
    chord.type = types()[(size_t) juce::jlimit (0, (int) types().size() - 1, type.getSelectedId() - 1)];
    return chord;
}

void FollowSetupPage::setKnownChord (const pr7s::Chord& chord)
{
    root.setSelectedId (chord.root + 1, juce::dontSendNotification);
    for (size_t i = 0; i < types().size(); ++i)
        if (types()[i] == chord.type)
            type.setSelectedId ((int) i + 1, juce::dontSendNotification);
}

void FollowSetupPage::paint (juce::Graphics& g)
{
    g.fillAll (pageColour);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (18.0f));
    g.drawText ("FOLLOW CHORD TRACK", getLocalBounds().reduced (16, 10).removeFromTop (26), juce::Justification::centredLeft);
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (16, syncLabel.getY() - 6, getWidth() - 32, 1);
}

void FollowSetupPage::resized()
{
    auto area = getLocalBounds().reduced (16, 10);
    area.removeFromTop (32);
    directly.setBounds (area.removeFromTop (24));
    directlyNote.setBounds (area.removeFromTop (38).withTrimmedLeft (26));
    area.removeFromTop (12);
    syncLabel.setBounds (area.removeFromTop (20));
    area.removeFromTop (2);
    analyze.setBounds (area.removeFromTop (24));
    analyzeNote.setBounds (area.removeFromTop (38).withTrimmedLeft (26));
    area.removeFromTop (4);
    known.setBounds (area.removeFromTop (24));
    knownNote.setBounds (area.removeFromTop (22).withTrimmedLeft (26));
    area.removeFromTop (4);
    auto line = area.removeFromTop (26).withTrimmedLeft (26);
    rootLabel.setBounds (line.removeFromLeft (80));
    root.setBounds (line.removeFromLeft (80));
    line.removeFromLeft (14);
    typeLabel.setBounds (line.removeFromLeft (84));
    type.setBounds (line);
    auto buttons = getLocalBounds().reduced (16, 12).removeFromBottom (30);
    cancel.setBounds (buttons.removeFromRight (110));
    buttons.removeFromRight (8);
    ok.setBounds (buttons.removeFromRight (110));
}

void FollowSetupPage::show (pr7s::Studio& studio, int trackId, pr7s::FollowMode mode, juce::Component& target, std::function<void()> done)
{
    auto page = std::make_unique<FollowSetupPage>();
    auto* raw = page.get();
    auto& box = juce::CallOutBox::launchAsynchronously (std::move (page), target.getScreenBounds(), nullptr);
    juce::Component::SafePointer<juce::CallOutBox> boxSafe (&box);
    raw->onCancel = [boxSafe] { if (boxSafe != nullptr) boxSafe->dismiss(); };
    raw->onOk = [&studio, trackId, mode, raw, boxSafe, done]
    {
        studio.startFollowing (trackId, mode, raw->getStart(), raw->getKnownChord());
        if (boxSafe != nullptr)
            boxSafe->dismiss();
        if (done)
            done();
    };
}
