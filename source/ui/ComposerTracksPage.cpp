#include "ComposerTracksPage.h"

#include "ParagonLookAndFeel.h"

using namespace pr7s;

juce::String ComposerTracksPage::laneName (int lane)
{
    static const char* names[] { "BASS", "GUITAR", "LEAD", "CHORDS", "MELODY", "DRUMS" };
    return names[juce::jlimit (0, Studio::numComposerTrackLanes - 1, lane)];
}

ComposerTracksPage::ComposerTracksPage (Studio& s) : studio (s)
{
    for (int i = 0; i < Studio::numComposerTrackLanes; ++i)
    {
        auto row = std::make_unique<Row>();
        auto* r = row.get();
        r->lane = i;
        r->name.setText (laneName (i), juce::dontSendNotification);
        r->name.setFont (ParagonLookAndFeel::titleFont (18.0f));
        r->name.setColour (juce::Label::textColourId, ParagonColours::text);
        addAndMakeVisible (r->name);
        r->names.setTooltip ("The names of the tracks the " + laneName (i).toLowerCase()
                             + " lane plays on, separated by commas (upper or lower case alike). The first is what a new one is called.");
        r->names.setFont (ParagonLookAndFeel::textFont (16.0f));
        r->names.onReturnKey = [this, r] { setNames (r->lane, r->names.getText()); };
        r->names.onFocusLost = [this, r] { setNames (r->lane, r->names.getText()); };
        addAndMakeVisible (r->names);
        r->every.setWantsKeyboardFocus (false);
        r->every.setTooltip ("On: the lane plays on every track with one of these names (a guitar left and a guitar right). "
                             "Off: on the first one found.");
        r->every.onClick = [this, r] { setEvery (r->lane, r->every.getToggleState()); };
        addAndMakeVisible (r->every);
        r->found.setFont (ParagonLookAndFeel::textFont (13.0f));
        r->found.setColour (juce::Label::textColourId, ParagonColours::textDim);
        addAndMakeVisible (r->found);
        rows[(size_t) i] = std::move (row);
    }
    looseKey.setWantsKeyboardFocus (false);
    looseKey.setTooltip ("A lane on two or more tracks: the copies after the first with their own small timing and velocity "
                         "differences (kept in their velocity layers), as a second take would have. Off: the same notes on each.");
    looseKey.onClick = [this] { setLooseCopies (looseKey.getToggleState()); };
    addAndMakeVisible (looseKey);
    resetKey.setWantsKeyboardFocus (false);
    resetKey.setTooltip ("Every lane back to its usual track name (Bass, Guitar, Lead Guitar, Piano, Melody, Drums), one track each");
    resetKey.onClick = [this] { resetAll(); };
    addAndMakeVisible (resetKey);
    refresh();
    startTimerHz (4);
}

ComposerTracksPage::~ComposerTracksPage()
{
    stopTimer();
}

juce::String ComposerTracksPage::getFoundText (int lane) const
{
    const auto part = Studio::composerTrackPart (lane);
    juce::StringArray names;
    for (const auto id : studio.findIdeaTracks (part))
        if (const auto* t = studio.getProject().findTrack (id))
            names.add (t->name.toUpperCase());
    if (names.isEmpty())
        return "IN THIS SONG: NONE YET (BUILD MAKES \"" + studio.newIdeaTrackName (part).toUpperCase() + "\")";
    return "IN THIS SONG: " + names.joinIntoString (" + ");
}

void ComposerTracksPage::refresh()
{
    for (auto& r : rows)
    {
        const auto tracks = studio.getComposerTracks (Studio::composerTrackPart (r->lane));
        if (! r->names.hasKeyboardFocus (true))
            r->names.setText (tracks.names.joinIntoString (", "), false);
        r->every.setToggleState (tracks.every, juce::dontSendNotification);
        r->found.setText (getFoundText (r->lane), juce::dontSendNotification);
    }
    looseKey.setToggleState (studio.getLooseCopies(), juce::dontSendNotification);
    seenChange = studio.getChangeVersion();
}

void ComposerTracksPage::timerCallback()
{
    // The open song's tracks change (added, renamed): what each lane finds follows.
    if (isShowing() && studio.getChangeVersion() != seenChange)
        refresh();
}

void ComposerTracksPage::setNames (int lane, const juce::String& text)
{
    auto tracks = studio.getComposerTracks (Studio::composerTrackPart (lane));
    tracks.names = juce::StringArray::fromTokens (text, ",", "");
    studio.setComposerTracks (Studio::composerTrackPart (lane), tracks);
    rows[(size_t) lane]->names.setText (studio.getComposerTracks (Studio::composerTrackPart (lane)).names.joinIntoString (", "), false);
    refresh();
}

void ComposerTracksPage::setEvery (int lane, bool on)
{
    auto tracks = studio.getComposerTracks (Studio::composerTrackPart (lane));
    tracks.every = on;
    studio.setComposerTracks (Studio::composerTrackPart (lane), tracks);
    refresh();
}

void ComposerTracksPage::setLooseCopies (bool on)
{
    studio.setLooseCopies (on);
    refresh();
}

void ComposerTracksPage::resetAll()
{
    studio.resetComposerTracks();
    for (auto& r : rows)
        r->names.setText (studio.getComposerTracks (Studio::composerTrackPart (r->lane)).names.joinIntoString (", "), false);
    refresh();
}

void ComposerTracksPage::resized()
{
    auto area = getLocalBounds();
    titleArea = area.removeFromTop (34);
    noteArea = area.removeFromTop (58);
    area.removeFromTop (8);
    headArea = area.removeFromTop (22);
    area.removeFromTop (4);
    for (auto& r : rows)
    {
        auto row = area.removeFromTop (58);
        auto top = row.removeFromTop (34).reduced (0, 2);
        r->name.setBounds (top.removeFromLeft (140));
        r->names.setBounds (top.removeFromLeft (juce::jmax (200, top.getWidth() - 260)));
        top.removeFromLeft (16);
        r->every.setBounds (top);
        r->found.setBounds (row.withTrimmedLeft (140).removeFromTop (20));
        area.removeFromTop (4);
    }
    area.removeFromTop (10);
    looseKey.setBounds (area.removeFromTop (30).withWidth (juce::jmin (520, area.getWidth())));
    area.removeFromTop (12);
    resetKey.setBounds (area.removeFromTop (34).withWidth (300));
}

void ComposerTracksPage::paint (juce::Graphics& g)
{
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (22.0f));
    g.drawText ("COMPOSER TRACKS", titleArea, juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::textFont (13.0f));
    g.drawFittedText ("The tracks each composer lane plays on when its TRACK menu is on automatic: an instrument track with one of these names "
                      "(several by commas; upper or lower case alike). The first name is what a new one is called. EVERY MATCHING TRACK plays "
                      "the lane on all of them, a guitar left and a guitar right.", noteArea, juce::Justification::topLeft, 3);
    g.setFont (ParagonLookAndFeel::textFont (12.0f));
    auto head = headArea;
    g.drawText ("LANE", head.removeFromLeft (140), juce::Justification::centredLeft, false);
    g.drawText ("TRACK NAMES", head, juce::Justification::centredLeft, false);
}
