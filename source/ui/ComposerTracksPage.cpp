#include "ComposerTracksPage.h"

#include "ParagonLookAndFeel.h"
#include "engine/GuitarTones.h"

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
        // (0.204.0) TONE: PR7-G on the guitar lanes' tracks.
        if (const auto part = Studio::composerTrackPart (i); hasGuitarTones (part))
        {
            r->tone = std::make_unique<juce::ComboBox>();
            r->tone->setWantsKeyboardFocus (false);
            r->tone->setTooltip ("TONE: when BUILD writes the " + laneName (i).toLowerCase() + " lane onto a track, PR7-G goes on its "
                                 "inserts with this tone (AUTOMATIC: chosen from the part's style). One of your own PR7-G rigs "
                                 "works too. A PR7-G already on the track is left as it is. OFF: nothing is added.");
            r->tone->onChange = [this, r]
            {
                const auto id = r->tone->getSelectedId();
                Studio::ToneChoice c;
                if (id == 2)
                    c.kind = Studio::ToneChoice::Kind::off;
                else if (id >= rigItems && id - rigItems < r->rigs.size())
                    c = { Studio::ToneChoice::Kind::rig, r->rigs[id - rigItems].getFullPathName() };
                else if (id >= toneItems && id - toneItems < r->toneNames.size())
                    c = { Studio::ToneChoice::Kind::tone, r->toneNames[id - toneItems] };
                else if (id == missingRigItem)
                    return;
                setTone (r->lane, c);
            };
            content.addAndMakeVisible (*r->tone);
            r->toneLabel.setText ("TONE", juce::dontSendNotification);
            r->toneLabel.setFont (ParagonLookAndFeel::textFont (13.0f));
            r->toneLabel.setColour (juce::Label::textColourId, ParagonColours::textDim);
            r->toneLabel.setJustificationType (juce::Justification::centredRight);
            content.addAndMakeVisible (r->toneLabel);
        }
        rows[(size_t) i] = std::move (row);
    }
    looseKey.setWantsKeyboardFocus (false);
    looseKey.setTooltip ("A lane on two or more tracks: the copies after the first with their own small timing and velocity "
                         "differences (kept in their velocity layers), as a second take would have. Off: the same notes on each.");
    looseKey.onClick = [this] { setLooseCopies (looseKey.getToggleState()); };
    content.addAndMakeVisible (looseKey);
    // (0.205.0) PR7-G's mix, delay and reverb follow the SECTIONS row.
    sectionTonesKey.setWantsKeyboardFocus (false);
    sectionTonesKey.setTooltip ("On: when BUILD puts a tone on a lead, clean or acoustic guitar track, PR7-G's modulation, delay and "
                                "reverb change block by block of the SECTIONS row (drier verses, wetter bridges, intros and outros), "
                                "written as automation on hidden lanes (the track's automation shows them)");
    sectionTonesKey.onClick = [this] { setSectionTones (sectionTonesKey.getToggleState()); };
    content.addAndMakeVisible (sectionTonesKey);
    resetKey.setWantsKeyboardFocus (false);
    resetKey.setTooltip ("Every lane back to its usual track name (Bass, Guitar, Lead Guitar, Piano, Melody, Drums...), one track each, "
                         "and its usual level (chords -11.5 dB, melody -9.5, bass -5.5, the rest 0)");
    resetKey.onClick = [this] { resetAll(); };
    content.addAndMakeVisible (resetKey);
    // (0.202.0) The rows in a view of their own: at the Hub's usual height they ran past the panel (the lower lanes and
    // the reset key hidden).
    refreshTones();
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
    sectionTonesKey.setToggleState (studio.getSectionTones(), juce::dontSendNotification);
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

void ComposerTracksPage::refreshTones()
{
    const auto rigs = pr7gRigFiles();
    for (auto& r : rows)
    {
        if (r->tone == nullptr)
            continue;
        r->tone->clear (juce::dontSendNotification);
        r->toneNames.clear();
        r->rigs = rigs;
        r->tone->addItem ("AUTOMATIC (BY STYLE)", 1);
        r->tone->addItem ("OFF", 2);
        r->tone->addSectionHeading ("OUR TONES");
        for (const auto* t : guitarTonesFor (Studio::composerTrackPart (r->lane)))
        {
            r->tone->addItem (t->name, toneItems + r->toneNames.size());
            r->toneNames.add (t->name);
        }
        if (! rigs.isEmpty())
        {
            r->tone->addSectionHeading ("MY RIGS");
            for (int k = 0; k < rigs.size(); ++k)
                r->tone->addItem (rigs[k].getFileNameWithoutExtension().toUpperCase(), rigItems + k);
        }
        showToneChoice (*r);
    }
}

void ComposerTracksPage::showToneChoice (Row& r)
{
    if (r.tone == nullptr)
        return;
    const auto choice = studio.getToneChoice (Studio::composerTrackPart (r.lane));
    auto id = 1;
    if (choice.kind == Studio::ToneChoice::Kind::off)
        id = 2;
    else if (choice.kind == Studio::ToneChoice::Kind::tone)
        id = r.toneNames.contains (choice.value) ? toneItems + r.toneNames.indexOf (choice.value) : 1;
    else if (choice.kind == Studio::ToneChoice::Kind::rig)
    {
        id = missingRigItem;
        for (int k = 0; k < r.rigs.size(); ++k)
            if (r.rigs[k].getFullPathName() == choice.value)
                id = rigItems + k;
        if (id == missingRigItem && r.tone->indexOfItemId (missingRigItem) < 0)
            r.tone->addItem (juce::File (choice.value).getFileNameWithoutExtension().toUpperCase() + " (NOT FOUND)", missingRigItem);
    }
    r.tone->setSelectedId (id, juce::dontSendNotification);
}

void ComposerTracksPage::setTone (int lane, const Studio::ToneChoice& choice)
{
    if (lane < 0 || lane >= Studio::numComposerTrackLanes)
        return;
    studio.setToneChoice (Studio::composerTrackPart (lane), choice);
    showToneChoice (*rows[(size_t) lane]);
}

juce::ComboBox* ComposerTracksPage::getToneBox (int lane)
{
    return lane >= 0 && lane < Studio::numComposerTrackLanes ? rows[(size_t) lane]->tone.get() : nullptr;
}

void ComposerTracksPage::visibilityChanged()
{
    if (isVisible())
        refreshTones();   // (rigs saved in PR7-G since)
}

void ComposerTracksPage::setSectionTones (bool on)
{
    studio.setSectionTones (on);
    sectionTonesKey.setToggleState (on, juce::dontSendNotification);
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
    constexpr int rowHeight = 56;   // (0.204.0: room for the guitar lanes' TONE box)
    content.setSize (width, Studio::numComposerTrackLanes * (rowHeight + 4) + 10 + 30 + 4 + 30 + 12 + 34 + 6);
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
        auto foot = row.withTrimmedLeft (140).removeFromTop (22);
        if (r->tone != nullptr)
        {
            auto box = foot.removeFromRight (juce::jmin (360, foot.getWidth() / 2));
            r->toneLabel.setBounds (box.removeFromLeft (52));
            r->tone->setBounds (box.reduced (0, 1));
            foot.removeFromRight (12);
        }
        r->found.setBounds (foot);
        inside.removeFromTop (4);
    }
    inside.removeFromTop (10);
    looseKey.setBounds (inside.removeFromTop (30).withWidth (juce::jmin (520, inside.getWidth())));
    inside.removeFromTop (4);
    sectionTonesKey.setBounds (inside.removeFromTop (30).withWidth (juce::jmin (520, inside.getWidth())));
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
