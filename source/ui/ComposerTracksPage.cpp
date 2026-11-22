#include "ComposerTracksPage.h"

#include "ParagonLookAndFeel.h"

using namespace pr7s;

juce::String ComposerTracksPage::laneName (int lane)
{
    static const char* names[] { "BASS", "GUITAR", "LEAD", "CHORDS", "MELODY", "DRUMS", "CLEAN GUITAR", "ACOUSTIC GUITAR" };
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
        content.addAndMakeVisible (r->name);
        r->names.setTooltip ("The names of the tracks the " + laneName (i).toLowerCase()
                             + " lane plays on, separated by commas (upper or lower case alike). A name's words found in a track's name count too: "
                             "RHYTHM finds RHYTHM GUITAR LEFT and RHYTHM GUITAR RIGHT. The first is what a new one is called.");
        r->names.setFont (ParagonLookAndFeel::textFont (16.0f));
        r->names.onReturnKey = [this, r] { setNames (r->lane, r->names.getText()); };
        r->names.onFocusLost = [this, r] { setNames (r->lane, r->names.getText()); };
        content.addAndMakeVisible (r->names);
        r->every.setWantsKeyboardFocus (false);
        r->every.setTooltip ("On: the lane plays on every track with one of these names (a guitar left and a guitar right). "
                             "Off: on the first one found.");
        r->every.onClick = [this, r] { setEvery (r->lane, r->every.getToggleState()); };
        content.addAndMakeVisible (r->every);
        r->found.setFont (ParagonLookAndFeel::textFont (13.0f));
        r->found.setColour (juce::Label::textColourId, ParagonColours::textDim);
        content.addAndMakeVisible (r->found);
        // (0.201.0, the owner's levels) LEVEL: where a new track's fader starts.
        r->level.setSliderStyle (juce::Slider::LinearBar);
        r->level.setRange (-60.0, 6.0, 0.1);
        r->level.dragStep = 0.5;
        r->level.setTextBoxIsEditable (false);   // (a double-click opens ValueSlider's own entry)
        r->level.setColour (juce::Slider::trackColourId, ParagonColours::accent.withAlpha (0.55f));
        r->level.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff151217));
        r->level.setColour (juce::Slider::textBoxTextColourId, ParagonColours::text);
        r->level.setColour (juce::Slider::textBoxOutlineColourId, ParagonColours::panelEdge);
        r->level.textFromValueFunction = [] (double v) { return v <= -60.0 ? juce::String ("-INF") : juce::String (v, 1) + " DB"; };
        r->level.parse = [] (const juce::String& text) { return ValueEntry::parseDb (text); };
        r->level.updateText();   // (a value equal to the slider's start isn't a change: its text was made before the function)
        r->level.setDoubleClickReturnValue (true, Studio::defaultLaneLevel (i), juce::ModifierKeys::commandModifier);
        r->level.setWantsKeyboardFocus (false);
        r->level.setTooltip ("LEVEL: the fader a track Architect makes for the " + laneName (i).toLowerCase()
                             + " lane starts at. Tracks it finds by name keep their own fader. Double-click to type a value; "
                               "Ctrl-click for the usual one.");
        r->level.onValueChange = [this, r] { studio.setLaneLevel (r->lane, (float) r->level.getValue()); };
        content.addAndMakeVisible (r->level);
        rows[(size_t) i] = std::move (row);
    }
    looseKey.setWantsKeyboardFocus (false);
    looseKey.setTooltip ("A lane on two or more tracks: the copies after the first with their own small timing and velocity "
                         "differences (kept in their velocity layers), as a second take would have. Off: the same notes on each.");
    looseKey.onClick = [this] { setLooseCopies (looseKey.getToggleState()); };
    content.addAndMakeVisible (looseKey);
    resetKey.setWantsKeyboardFocus (false);
    resetKey.setTooltip ("Every lane back to its usual track name (Bass, Guitar, Lead Guitar, Piano, Melody, Drums...), one track each, "
                         "and its usual level (chords -11.5 dB, melody -9.5, bass -5.5, the rest 0)");
    resetKey.onClick = [this] { resetAll(); };
    content.addAndMakeVisible (resetKey);
    // (0.202.0) The rows in a view of their own: at the Hub's usual height they ran past the panel (the lower lanes and
    // the reset key hidden).
    view.setViewedComponent (&content, false);
    view.setScrollBarsShown (true, false);
    view.setScrollBarThickness (10);
    addAndMakeVisible (view);
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
        if (! r->level.isMouseButtonDown())
            r->level.setValue (studio.getLaneLevels()[(size_t) r->lane], juce::dontSendNotification);
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

void ComposerTracksPage::setLevel (int lane, float db)
{
    if (lane < 0 || lane >= Studio::numComposerTrackLanes)
        return;
    studio.setLaneLevel (lane, db);
    refresh();
}

void ComposerTracksPage::resetAll()
{
    studio.resetComposerTracks();
    studio.resetLaneLevels();   // (0.201.0) and the owner's levels
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
    view.setBounds (area);
    // (0.202.0) Tighter rows (the found tracks beside the name's foot), in the view: they scroll when the panel is short.
    const auto width = juce::jmax (400, area.getWidth() - 14);
    constexpr int rowHeight = 50;
    content.setSize (width, Studio::numComposerTrackLanes * (rowHeight + 4) + 10 + 30 + 12 + 34 + 6);
    if (content.getHeight() <= area.getHeight())
        content.setSize (area.getWidth(), content.getHeight());
    auto inside = content.getLocalBounds();
    for (auto& r : rows)
    {
        auto row = inside.removeFromTop (rowHeight);
        auto top = row.removeFromTop (30).reduced (0, 1);
        r->name.setBounds (top.removeFromLeft (140));
        r->level.setBounds (top.removeFromRight (180).reduced (0, 3));
        top.removeFromRight (16);
        r->names.setBounds (top.removeFromLeft (juce::jmax (200, top.getWidth() - 260)));
        top.removeFromLeft (16);
        r->every.setBounds (top);
        r->found.setBounds (row.withTrimmedLeft (140).removeFromTop (18));
        inside.removeFromTop (4);
    }
    inside.removeFromTop (10);
    looseKey.setBounds (inside.removeFromTop (30).withWidth (juce::jmin (520, inside.getWidth())));
    inside.removeFromTop (12);
    resetKey.setBounds (inside.removeFromTop (34).withWidth (380));
}

void ComposerTracksPage::paint (juce::Graphics& g)
{
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (22.0f));
    g.drawText ("ARCHITECT TRACKS", titleArea, juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::textFont (13.0f));
    g.drawFittedText ("The tracks each Architect lane plays on when its TRACK menu is on automatic: an instrument track with one of these names "
                      "(several by commas; upper or lower case alike). The first name is what a new one is called. EVERY MATCHING TRACK plays "
                      "the lane on all of them, a guitar left and a guitar right. LEVEL is where the fader of a track Architect makes starts; "
                      "tracks it finds by name keep their own.", noteArea, juce::Justification::topLeft, 3);
    g.setFont (ParagonLookAndFeel::textFont (12.0f));
    auto head = headArea;
    g.drawText ("LANE", head.removeFromLeft (140), juce::Justification::centredLeft, false);
    g.drawText ("LEVEL (A NEW TRACK'S FADER)", head.removeFromRight (180), juce::Justification::centredLeft, false);
    g.drawText ("TRACK NAMES", head, juce::Justification::centredLeft, false);
}
