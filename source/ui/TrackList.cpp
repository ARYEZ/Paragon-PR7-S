#include "TrackList.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    juce::FontOptions font (float height, int style = juce::Font::bold) { return ParagonLookAndFeel::textFont (height, style); }
}

//==============================================================================
class TrackList::Row final : public juce::Component
{
public:
    Row (TrackList& ownerList, int id) : owner (ownerList), trackId (id)
    {
        name.setFont (ParagonLookAndFeel::titleFont (13.0f));
        name.setColour (juce::Label::textColourId, ParagonColours::text);
        name.setEditable (false, true, false);   // double-click to rename
        name.setTooltip ("The track's name: double-click to rename");
        name.onTextChange = [this] { owner.studio.renameTrack (trackId, name.getText()); };
        name.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (name);

        for (auto* key : { &mute, &solo, &arm })
        {
            key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
            addAndMakeVisible (*key);
        }
        mute.setTooltip ("Mute this track");
        solo.setTooltip ("Solo: hear only the soloed tracks");
        arm.setTooltip ("Record-arm: recording and live playing go to this track");
        mute.onClick = [this] { if (auto* t = track()) owner.studio.setMute (trackId, ! t->mute); };
        solo.onClick = [this] { if (auto* t = track()) owner.studio.setSolo (trackId, ! t->solo); };
        arm.onClick = [this] { if (auto* t = track()) owner.studio.setArmed (t->armed ? 0 : trackId); };

        instrument.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        instrument.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        instrument.setTooltip ("The instrument: click to open its window, change it, or delete the track");
        instrument.onClick = [this] { owner.showInstrumentMenu (trackId); };
        addAndMakeVisible (instrument);

        volume.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        volume.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        volume.setRange (-60.0, 6.0, 0.1);
        volume.setSkewFactorFromMidPoint (-12.0);
        volume.setDoubleClickReturnValue (true, 0.0);
        volume.setPopupDisplayEnabled (true, true, nullptr);
        volume.textFromValueFunction = [] (double db) { return db <= -59.9 ? juce::String ("-inf dB") : juce::String (db, 1) + " dB"; };
        volume.setTooltip ("The track's volume (double-click for 0 dB)");
        volume.onDragEnd = [this] { owner.studio.setVolume (trackId, (float) volume.getValue()); };
        volume.onValueChange = [this] { if (! volume.isMouseButtonDown()) owner.studio.setVolume (trackId, (float) volume.getValue()); };
        addAndMakeVisible (volume);
    }

    const pr7s::Track* track() const { return owner.studio.getProject().findTrack (trackId); }

    void refresh()
    {
        const auto* t = track();
        if (t == nullptr)
            return;
        if (! name.isBeingEdited())
            name.setText (t->name, juce::dontSendNotification);
        mute.setToggleState (t->mute, juce::dontSendNotification);
        solo.setToggleState (t->solo, juce::dontSendNotification);
        arm.setToggleState (t->armed, juce::dontSendNotification);
        instrument.setButtonText (t->instrument.getName());
        if (! volume.isMouseButtonDown())
            volume.setValue (t->volumeDb, juce::dontSendNotification);
        repaint();
    }

    void setLevel (float peak)
    {
        const auto shown = juce::jmax (peak, level * 0.85f);
        if (std::abs (shown - level) > 0.001f)
        {
            level = shown;
            repaint (levelArea.toNearestInt().expanded (2));
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto* t = track();
        if (t == nullptr)
            return;
        const auto bounds = getLocalBounds().toFloat();
        const auto selected = owner.studio.getSelectedTrack() == trackId;
        g.setColour (selected ? juce::Colour (0xff1f1720) : juce::Colour (0xff100c11));
        g.fillRect (bounds);
        g.setColour (juce::Colour (0xff050405));
        g.fillRect (bounds.withY (bounds.getBottom() - 1.0f).withHeight (1.0f));
        g.setColour (t->colour);
        g.fillRect (bounds.withWidth (6.0f).reduced (0.0f, 1.0f));
        if (selected)
        {
            g.setColour (ParagonColours::accent.withAlpha (0.6f));
            g.drawRect (bounds.withTrimmedLeft (6.0f), 1.0f);
        }

        // Its level, as a row of LEDs.
        constexpr int count = 12;
        const auto db = juce::Decibels::gainToDecibels (level, -60.0f);
        const auto lit = juce::roundToInt (juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 54.0f) * (float) count);
        const auto width = levelArea.getWidth() / (float) count;
        for (int i = 0; i < count; ++i)
        {
            const auto led = juce::Rectangle<float> (levelArea.getX() + width * (float) i, levelArea.getY(), width - 1.5f, levelArea.getHeight());
            const auto colour = i >= count - 1 ? ParagonColours::meterRed : ParagonColours::accent;
            g.setColour (i < lit ? colour : colour.withAlpha (0.12f));
            g.fillRoundedRectangle (led, 1.0f);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().withTrimmedLeft (12).reduced (6, 7);
        auto top = area.removeFromTop (24);
        arm.setBounds (top.removeFromRight (30));
        top.removeFromRight (3);
        solo.setBounds (top.removeFromRight (30));
        top.removeFromRight (3);
        mute.setBounds (top.removeFromRight (30));
        top.removeFromRight (6);
        name.setBounds (top);
        area.removeFromTop (6);
        auto bottom = area.removeFromTop (24);
        volume.setBounds (bottom.removeFromRight (28));
        bottom.removeFromRight (6);
        levelArea = bottom.removeFromRight (64).toFloat().withSizeKeepingCentre (64.0f, 6.0f);
        bottom.removeFromRight (8);
        instrument.setBounds (bottom);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        owner.studio.selectTrack (trackId);
        if (event.mods.isPopupMenu())
            owner.showInstrumentMenu (trackId);
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (name.getBounds().contains (event.getPosition()))
            name.showEditor();
    }

    TrackList& owner;
    const int trackId;
    juce::Label name;
    juce::TextButton mute { "M" }, solo { "S" }, arm { "R" }, instrument;
    juce::Slider volume;
    juce::Rectangle<float> levelArea;
    float level = 0.0f;
};

//==============================================================================
TrackList::TrackList (pr7s::Studio& studioToUse, Timeline& timelineToUse)
    : studio (studioToUse), timeline (timelineToUse)
{
    addTrack.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    addTrack.setTooltip ("Add a track, with the Paragon Synth or one of your VST3 instruments");
    addTrack.onClick = [this] { showAddTrackMenu(); };
    addAndMakeVisible (addTrack);
    studio.addChangeListener (this);
    rebuild();
    startTimerHz (30);
}

TrackList::~TrackList()
{
    studio.removeChangeListener (this);
}

void TrackList::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuild();
}

void TrackList::timerCallback()
{
    for (auto& row : rows)
        row->setLevel (studio.getEngine().takeTrackPeak (row->trackId));
}

void TrackList::rebuild()
{
    // The rows follow the project's tracks (kept where they are, so a name being edited isn't lost).
    const auto& tracks = studio.getProject().tracks;
    auto same = rows.size() == tracks.size();
    for (size_t i = 0; same && i < tracks.size(); ++i)
        same = rows[i]->trackId == tracks[i].id;
    if (! same)
    {
        rows.clear();
        for (const auto& track : tracks)
        {
            rows.push_back (std::make_unique<Row> (*this, track.id));
            addAndMakeVisible (*rows.back());
        }
    }
    for (auto& row : rows)
        row->refresh();
    resized();
    repaint();
}

void TrackList::resized()
{
    for (size_t i = 0; i < rows.size(); ++i)
        rows[i]->setBounds (0, Timeline::rulerHeight + (int) i * Timeline::trackHeight - timeline.scrollY, getWidth(), Timeline::trackHeight);
    addTrack.setBounds (getLocalBounds().removeFromBottom (addButtonHeight).reduced (10, 5));
    addTrack.toFront (false);
}

void TrackList::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b090c));
    // The header over the rows, level with the ruler.
    auto header = getLocalBounds().removeFromTop (Timeline::rulerHeight).toFloat();
    g.setColour (juce::Colour (0xff151116));
    g.fillRect (header);
    g.setColour (ParagonColours::textDim);
    g.setFont (font (11.0f));
    g.drawText ("TRACKS", header.reduced (14.0f, 0.0f), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (header.withY (header.getBottom() - 1.0f).withHeight (1.0f));
    g.setColour (juce::Colour (0xff0b090c));
    g.fillRect (getLocalBounds().removeFromBottom (addButtonHeight));
}

void TrackList::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const auto content = (int) studio.getProject().tracks.size() * Timeline::trackHeight;
    const auto room = juce::jmax (0, content - (getHeight() - Timeline::rulerHeight - addButtonHeight));
    timeline.scrollY = juce::jlimit (0, room, timeline.scrollY - juce::roundToInt (wheel.deltaY * 300.0f));
    if (timeline.onScrollOrZoom)
        timeline.onScrollOrZoom();
}

void TrackList::showAddTrackMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("ADD AN INSTRUMENT TRACK");
    menu.addItem (1, "Paragon Synth (built in)");
    const auto instruments = studio.getPluginHost().getInstruments();
    if (! instruments.isEmpty())
    {
        menu.addSeparator();
        for (int i = 0; i < instruments.size(); ++i)
            menu.addItem (100 + i, instruments[i].manufacturerName + ": " + instruments[i].name);
    }
    menu.addSeparator();
    menu.addItem (2, instruments.isEmpty() ? "Find my VST3 instruments..." : "Find more VST3 instruments...");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addTrack), [this, instruments] (int chosen)
    {
        if (chosen == 1)
        {
            studio.addTrack ({});
        }
        else if (chosen == 2)
        {
            if (onFindPlugins)
                onFindPlugins();
        }
        else if (chosen >= 100)
        {
            pr7s::Instrument instrument;
            instrument.kind = pr7s::Instrument::Kind::plugin;
            instrument.plugin = instruments[chosen - 100];
            juce::String error;
            if (studio.addTrack (instrument, &error) == 0)
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't add the track", error);
        }
    });
}

void TrackList::showInstrumentMenu (int trackId)
{
    const auto* track = studio.getProject().findTrack (trackId);
    if (track == nullptr)
        return;
    auto* instance = studio.getEngine().getInstrument (trackId);
    juce::PopupMenu menu, change;
    menu.addItem (1, "Open " + track->instrument.getName() + "'s window", instance != nullptr && instance->hasEditor());
    change.addItem (10, "Paragon Synth (built in)");
    const auto instruments = studio.getPluginHost().getInstruments();
    for (int i = 0; i < instruments.size(); ++i)
        change.addItem (100 + i, instruments[i].manufacturerName + ": " + instruments[i].name);
    menu.addSubMenu ("Change instrument", change);
    menu.addSeparator();
    menu.addItem (2, "Delete track");

    menu.showMenuAsync (juce::PopupMenu::Options(), [this, trackId, instruments] (int chosen)
    {
        if (chosen == 1 && onOpenInstrument)
        {
            onOpenInstrument (trackId);
        }
        else if (chosen == 2)
        {
            const auto* doomed = studio.getProject().findTrack (trackId);
            if (doomed == nullptr)
                return;
            // Deleting a track isn't undoable yet: ask first.
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Delete track",
                                                "Delete \"" + doomed->name + "\" and its parts? This can't be undone.",
                                                "Delete", "Cancel", nullptr,
                                                juce::ModalCallbackFunction::create ([this, trackId] (int result)
                                                {
                                                    if (result == 1)
                                                        studio.removeTrack (trackId);
                                                }));
        }
        else if (chosen == 10 || chosen >= 100)
        {
            pr7s::Instrument instrument;
            if (chosen >= 100)
            {
                instrument.kind = pr7s::Instrument::Kind::plugin;
                instrument.plugin = instruments[chosen - 100];
            }
            juce::String error;
            if (! studio.changeInstrument (trackId, instrument, &error))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't change the instrument", error);
        }
    });
}
