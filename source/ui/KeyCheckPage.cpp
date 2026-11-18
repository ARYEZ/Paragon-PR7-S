#include "ui/KeyCheckPage.h"
#include "ui/ChordUi.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    const juce::Colour pageColour { 0xff0c0a0d };
    juce::Font font (float h) { return ParagonLookAndFeel::textFont (h); }
    juce::String keyName (const pr7s::Key& key) { return ChordUi::spokenKey (key).toUpperCase(); }
}

KeyCheckPage::KeyCheckPage (pr7s::Studio& s) : studio (s)
{
    list.setModel (this);
    list.setRowHeight (rowHeight);
    list.setColour (juce::ListBox::backgroundColourId, pageColour);
    addAndMakeVisible (list);
    keyBox.addItem ("KEY: AUTOMATIC", 1);
    const auto& keys = pr7s::Key::all();
    for (int i = 0; i < (int) keys.size(); ++i)
        keyBox.addItem ("KEY: " + keyName (keys[(size_t) i]), i + 2);
    keyBox.setSelectedId (1, juce::dontSendNotification);
    keyBox.onChange = [this]
    {
        const auto id = keyBox.getSelectedId();
        setKey (id >= 2 ? std::optional<pr7s::Key> (pr7s::Key::all()[(size_t) id - 2]) : std::nullopt);
    };
    addAndMakeVisible (keyBox);
    fixAllKey.onClick = [this] { fixAll(); };
    fixAllKey.setTooltip ("Every listed note to the nearest note of the key (one undo step)");
    addAndMakeVisible (fixAllKey);
    checkKey.onClick = [this] { check(); };
    addAndMakeVisible (checkKey);
    check();
    startTimerHz (4);
}

KeyCheckPage::~KeyCheckPage()
{
    list.setModel (nullptr);
}

void KeyCheckPage::setKey (std::optional<pr7s::Key> key)
{
    chosen = key;
    auto id = 1;
    if (key.has_value())
    {
        const auto& keys = pr7s::Key::all();
        for (int i = 0; i < (int) keys.size(); ++i)
            if (keys[(size_t) i] == *key)
                id = i + 2;
    }
    keyBox.setSelectedId (id, juce::dontSendNotification);
    check();
    if (onKeyChange)
        onKeyChange();
}

void KeyCheckPage::check()
{
    const auto& project = studio.getProject();
    issues = studio.checkSongKey (chosen);
    seenVersion = studio.getChangeVersion();
    if (chosen.has_value())
        keyText = keyName (*chosen);
    else if (! project.chordTrack.scales.empty())
        keyText = "THE CHORD TRACK'S KEYS";
    else if (const auto guessed = pr7s::guessSongKey (project))
        keyText = keyName (*guessed) + " (GUESSED FROM THE NOTES)";
    else
        keyText = "NO NOTES YET";
    auto notes = 0;
    for (const auto& i : issues)
        notes += i.kind == pr7s::KeyIssue::Kind::note ? 1 : 0;
    fixAllKey.setEnabled (notes > 0);
    list.updateContent();
    list.repaint();
    repaint();
}

juce::String KeyCheckPage::getSummary() const
{
    auto notes = 0, chords = 0;
    for (const auto& i : issues)
        (i.kind == pr7s::KeyIssue::Kind::note ? notes : chords)++;
    if (notes + chords == 0)
        return "EVERYTHING IS IN THE KEY: " + keyText;
    return juce::String (notes) + (notes == 1 ? " NOTE" : " NOTES") + " AND " + juce::String (chords) + (chords == 1 ? " CHORD" : " CHORDS")
         + " OUTSIDE THE KEY: " + keyText;
}

bool KeyCheckPage::fix (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) issues.size()) || issues[(size_t) row].kind != pr7s::KeyIssue::Kind::note)
        return false;
    const auto done = studio.fixOutOfKeyNotes ({ issues[(size_t) row] });
    check();
    return done;
}

bool KeyCheckPage::fixAll()
{
    const auto done = studio.fixOutOfKeyNotes (issues);
    check();
    return done;
}

juce::String KeyCheckPage::getRowText (int row) const
{
    if (! juce::isPositiveAndBelow (row, (int) issues.size()))
        return {};
    const auto& i = issues[(size_t) row];
    const auto& project = studio.getProject();
    auto text = "BAR " + pr7s::formatBarsBeats (i.beat, project).removeCharacters (" ") + "  ";
    if (i.kind == pr7s::KeyIssue::Kind::chord)
        return text + "CHORD TRACK  " + i.chord.name (i.key) + "  (" + keyName (i.key) + ")";
    const auto* track = project.findTrack (i.trackId);
    return text + (track != nullptr ? track->name.toUpperCase() : juce::String()) + "  " + pr7s::noteName (i.pitch) + " -> "
         + pr7s::noteName (i.suggested) + "  (" + keyName (i.key) + ")";
}

void KeyCheckPage::paint (juce::Graphics& g)
{
    g.fillAll (pageColour);
    g.setColour (ParagonColours::accent);
    g.fillRect (0, 0, getWidth(), 3);
    auto head = getLocalBounds().removeFromTop (headerHeight).reduced (gap, 8);
    auto title = head.removeFromTop (34);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (22.0f));
    g.drawText ("KEY CHECK", title.removeFromLeft (160), juce::Justification::centredLeft, false);
    g.setColour (issues.empty() ? ParagonColours::text : ParagonColours::accent);
    g.setFont (font (16.0f));
    g.drawText (getSummary(), head.removeFromTop (24), juce::Justification::centredLeft, true);
    g.setColour (ParagonColours::textDim);
    g.setFont (font (13.0f));
    g.drawText ("THE CHORD TRACK'S RULE (THE SCALE, IN MINOR ALSO THE RAISED 7TH). DRUMS SKIPPED. CLICK A ROW TO SEE THE NOTE. "
                "NOTES OUTSIDE THE KEY CAN BE MEANT: FIX ONLY THE MISTAKES.",
                head, juce::Justification::centredLeft, true);
}

void KeyCheckPage::resized()
{
    auto area = getLocalBounds().reduced (gap, 0);
    auto head = area.removeFromTop (headerHeight).reduced (0, 8);
    auto top = head.removeFromTop (34);
    fixAllKey.setBounds (top.removeFromRight (150).reduced (0, 2));
    top.removeFromRight (8);
    checkKey.setBounds (top.removeFromRight (130).reduced (0, 2));
    top.removeFromRight (8);
    keyBox.setBounds (top.removeFromRight (240).reduced (0, 2));
    list.setBounds (area.withTrimmedBottom (gap));
}

void KeyCheckPage::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) issues.size()))
        return;
    const auto& i = issues[(size_t) row];
    const auto& project = studio.getProject();
    g.setColour (selected ? ParagonColours::accentDim : row % 2 == 0 ? juce::Colour (0xff131015) : pageColour);
    g.fillRect (0, 0, width, height);
    auto r = juce::Rectangle<int> (0, 0, width, height).reduced (10, 0);
    g.setColour (ParagonColours::textDim);
    g.setFont (font (16.0f));
    g.drawText ("BAR " + pr7s::formatBarsBeats (i.beat, project).removeCharacters (" "), r.removeFromLeft (130), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::text);
    if (i.kind == pr7s::KeyIssue::Kind::chord)
    {
        g.drawText ("CHORD TRACK", r.removeFromLeft (220), juce::Justification::centredLeft, true);
        g.setColour (juce::Colour (0xffde0639));
        g.setFont (ChordUi::chordFont (16.0f));
        g.drawText (i.chord.name (i.key), r.removeFromLeft (170), juce::Justification::centredLeft, false);
        g.setColour (ParagonColours::textDim);
        g.setFont (font (15.0f));
        g.drawText ("IN " + keyName (i.key) + ": SWAP IT ON THE CHORD TRACK", r.withTrimmedRight (10), juce::Justification::centredLeft, true);
        return;
    }
    const auto* track = project.findTrack (i.trackId);
    g.drawText (track != nullptr ? track->name.toUpperCase() : juce::String(), r.removeFromLeft (220), juce::Justification::centredLeft, true);
    g.setColour (juce::Colour (0xffde0639));
    g.drawText (pr7s::noteName (i.pitch), r.removeFromLeft (60), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.drawText ("TO", r.removeFromLeft (30), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::text);
    g.drawText (pr7s::noteName (i.suggested), r.removeFromLeft (80), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.drawText ("IN " + keyName (i.key), r.withTrimmedRight (100), juce::Justification::centredLeft, true);
    const auto key = fixArea (width).toFloat();
    g.setColour (ParagonColours::accentDim);
    g.fillRoundedRectangle (key, 4.0f);
    g.setColour (ParagonColours::accent);
    g.drawRoundedRectangle (key.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (ParagonColours::text);
    g.setFont (font (15.0f));
    g.drawText ("FIX", key, juce::Justification::centred, false);
}

void KeyCheckPage::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (! juce::isPositiveAndBelow (row, (int) issues.size()))
        return;
    if (issues[(size_t) row].kind == pr7s::KeyIssue::Kind::note && fixArea (list.getVisibleRowWidth()).contains (e.getPosition().withY (5)))
    {
        fix (row);
        return;
    }
    if (onShowIssue)
        onShowIssue (issues[(size_t) row]);
}

void KeyCheckPage::timerCallback()
{
    if (studio.getChangeVersion() != seenVersion)
        check();
}

KeyCheckWindow::KeyCheckWindow (pr7s::Studio& studio)
    : juce::DocumentWindow ("Paragon PR7-S: Key Check", pageColour, juce::DocumentWindow::closeButton)
{
    page = new KeyCheckPage (studio);
    page->setSize (KeyCheckPage::preferredWidth, KeyCheckPage::preferredHeight);
    setContentOwned (page, true);
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (720, 400, 3840, 2160);
}

void KeyCheckWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}
