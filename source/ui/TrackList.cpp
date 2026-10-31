#include "TrackList.h"

#include "ui/AutomationKeys.h"
#include "ui/LevelMeter.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    // An audio track's input as it's shown: "IN 1", or "IN 1+2" for a pair.
    juce::String inputName (const pr7s::Track& track)
    {
        return "IN " + juce::String (track.input + 1) + (track.stereoInput ? "+" + juce::String (track.input + 2) : juce::String());
    }

    juce::FontOptions font (float height, int style = juce::Font::bold) { return ParagonLookAndFeel::textFont (height, style); }
}

//==============================================================================
// Show/Hide Automation: a small arrow at a row's lower left (pointing right
// while the lanes are hidden, down while they're shown).
class TrackList::Arrow final : public juce::Component,
                               public juce::SettableTooltipClient
{
public:
    std::function<void()> onClick;
    bool open = false;

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (3.0f);
        juce::Path p;
        if (open)
            p.addTriangle (area.getX(), area.getY() + 2.0f, area.getRight(), area.getY() + 2.0f, area.getCentreX(), area.getBottom() - 1.0f);
        else
            p.addTriangle (area.getX() + 2.0f, area.getY(), area.getX() + 2.0f, area.getBottom(), area.getRight() - 1.0f, area.getCentreY());
        g.setColour (isMouseOver() || open ? ParagonColours::accent : ParagonColours::textDim);
        g.fillPath (p);
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains (e.getPosition()) && onClick)
            onClick();
    }
};

//==============================================================================
class TrackList::Row final : public juce::Component
{
public:
    Row (TrackList& ownerList, int id) : owner (ownerList), trackId (id)
    {
        AutomationKeys::setUp (read, write, owner.studio, trackId);
        addAndMakeVisible (read);
        addAndMakeVisible (write);
        arrow.setTooltip ("Show/Hide Automation: this track's automation lanes");
        arrow.onClick = [this] { owner.studio.setAutomationOpen (trackId, ! owner.studio.isAutomationOpen (trackId)); };
        addAndMakeVisible (arrow);
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
        monitor.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        monitor.setTooltip ("MON: hear what comes in from the interface (your guitar), through the track's effects");
        monitor.onClick = [this] { if (auto* t = track()) owner.studio.setMonitor (trackId, ! t->monitor); };
        addChildComponent (monitor);
        mute.setTooltip ("Mute this track");
        solo.setTooltip ("Solo: hear only the soloed tracks");
        arm.setTooltip ("REC: record-arm: recording and live playing go to this track");
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
        // Grabbed and let go, for writing automation (W on, while playing).
        volume.onDragStart = [this] { owner.studio.grabControl (trackId, pr7s::Studio::control (pr7s::AutomationLane::Target::volume)); };
        volume.onDragEnd = [this]
        {
            owner.studio.setVolume (trackId, (float) volume.getValue());
            owner.studio.releaseControl (trackId, pr7s::Studio::control (pr7s::AutomationLane::Target::volume));
        };
        volume.onValueChange = [this] { owner.studio.setVolume (trackId, (float) volume.getValue(), ! volume.isMouseButtonDown()); };
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
        if (t->isChannel())
        {
            // An FX or group channel: what it is (an FX channel: its first effect).
            const auto& inserts = t->inserts;
            instrument.setButtonText (t->isGroup() ? juce::String ("GROUP CHANNEL")
                                      : ! inserts.empty() && ! inserts.front().isEmpty() ? "FX: " + inserts.front().plugin.name : juce::String ("FX CHANNEL"));
            instrument.setTooltip (t->isGroup() ? "A group channel: the tracks routed to it (their Output), through its effects and fader. Click to delete it"
                                                : "An FX channel: what the tracks' sends give it, through its effects. Click to open its effect's window, or to delete it");
        }
        else
        {
            instrument.setButtonText (t->isAudio() ? inputName (*t) : t->instrument.getName());
            instrument.setTooltip (t->isAudio() ? "The input it records from and where it goes (Output): click to choose, or to delete the track"
                                                : "The instrument and where it goes (Output): click to open its window, change it, or delete the track");
        }
        arm.setVisible (! t->isChannel());   // an FX or group channel records nothing
        monitor.setVisible (t->isAudio());
        monitor.setToggleState (t->monitor, juce::dontSendNotification);
        AutomationKeys::refresh (read, write, owner.studio, trackId);
        arrow.open = t->automationOpen;
        arrow.repaint();
        followMix();
        repaint();
    }

    // The volume knob as it is now: the automation's, while it's read.
    void followMix()
    {
        const auto* t = track();
        if (t == nullptr || volume.isMouseButtonDown())
            return;
        const auto db = t->automationRead ? owner.studio.getEngine().getMixNow (trackId).volumeDb : t->volumeDb;
        if (std::abs (volume.getValue() - db) > 0.01)
            volume.setValue (db, juce::dontSendNotification);
    }

    void setLevel (float peak)
    {
        if (reading.push (peak))
            repaint (levelArea.toNearestInt().expanded (2));
    }

    void paint (juce::Graphics& g) override
    {
        const auto* t = track();
        if (t == nullptr)
            return;
        const auto bounds = getLocalBounds().toFloat();
        const auto selected = owner.studio.isTrackSelected (trackId);
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

        // Its level: PR7-C's meter, lying down (orange at the left to pink-red at the right).
        if (! levelArea.isEmpty())   // (none in a short row)
            LevelMeter::paintSegments (g, levelArea, LevelMeter::proportionForDb (reading.db),
                                   reading.heldDb > LevelMeter::minDb ? LevelMeter::proportionForDb (reading.heldDb) : -1.0f, false);
    }

    void resized() override
    {
        // Shorter than its two lines (the rows zoomed out): only the first, in the middle.
        const auto compact = getHeight() < 66;
        for (auto* control : std::initializer_list<juce::Component*> { &instrument, &read, &write, &volume })
            control->setVisible (! compact);
        levelArea = {};
        auto area = getLocalBounds().withTrimmedLeft (12).reduced (6, compact ? juce::jmax (2, (getHeight() - 24) / 2) : 7);
        auto top = area.removeFromTop (juce::jmin (24, area.getHeight()));
        if (arm.isVisible())
        {
            arm.setBounds (top.removeFromRight (36));
            top.removeFromRight (3);
        }
        solo.setBounds (top.removeFromRight (30));
        top.removeFromRight (3);
        mute.setBounds (top.removeFromRight (30));
        top.removeFromRight (3);
        monitor.setBounds (top.removeFromRight (40));   // audio tracks only (hidden otherwise)
        if (! monitor.isVisible())
            top.setRight (mute.getX() - 3);
        top.removeFromRight (6);
        name.setBounds (top);
        arrow.setBounds (0, getHeight() - 18, 16, 16);
        if (compact)
            return;
        area.removeFromTop (6);
        auto bottom = area.removeFromTop (24);
        volume.setBounds (bottom.removeFromRight (28));
        bottom.removeFromRight (4);
        levelArea = bottom.removeFromRight (44).toFloat().withSizeKeepingCentre (44.0f, 8.0f);
        bottom.removeFromRight (6);
        write.setBounds (bottom.removeFromRight (26));
        bottom.removeFromRight (2);
        read.setBounds (bottom.removeFromRight (26));
        bottom.removeFromRight (6);
        instrument.setBounds (bottom);
        // (Show/Hide Automation: the arrow at the lower left corner, as the manual has it.)
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        auto& studio = owner.studio;
        dragged = false;
        if (event.mods.isPopupMenu())
        {
            if (! studio.isTrackSelected (trackId))
                studio.selectTrack (trackId);
            owner.showInstrumentMenu (trackId, true);
            return;
        }
        // Ctrl-click adds (or removes) it, Shift-click a run (the manual's);
        // a click on one of several selected keeps them, to drag them together.
        if (event.mods.isCommandDown())
            studio.toggleTrackSelected (trackId);
        else if (event.mods.isShiftDown())
            studio.selectTrackRun (trackId);
        else if (! studio.isTrackSelected (trackId))
            studio.selectTrack (trackId);
        plainClickOnSelected = ! event.mods.isCommandDown() && ! event.mods.isShiftDown() && studio.getSelectedTracks().size() > 1;
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu() || event.getDistanceFromDragStart() < 5)
            return;
        dragged = true;
        owner.dragTrack (trackId, event.getEventRelativeTo (&owner).getPosition());
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragged)
            owner.dropTracks (trackId);
        else if (plainClickOnSelected)
            owner.studio.selectTrack (trackId);   // a click (no drag) on one of several: it alone
        plainClickOnSelected = false;
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (name.getBounds().contains (event.getPosition()))
            name.showEditor();
    }

    TrackList& owner;
    const int trackId;
    bool dragged = false, plainClickOnSelected = false;
    juce::Label name;
    juce::TextButton mute { "M" }, solo { "S" }, arm { "REC" }, monitor { "MON" }, instrument, read { "R" }, write { "W" };
    juce::Slider volume;
    Arrow arrow;
    juce::Rectangle<float> levelArea;
    LevelMeter::Ballistics reading;
};

//==============================================================================
// An automation lane's header: its control (click for the others, or to
// remove it), M (Mute Automation), its value at the playhead, and + (Append
// Automation Track) at its lower left.
class TrackList::LaneRow final : public juce::Component
{
public:
    LaneRow (TrackList& ownerList, int bus, int index) : owner (ownerList), busId (bus), laneIndex (index)
    {
        control.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        control.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        control.setTooltip ("The control this lane automates: click to choose another, or to remove it");
        control.onClick = [this] { owner.showLaneMenu (busId, laneIndex, control); };
        addAndMakeVisible (control);
        mute.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        mute.setTooltip ("Mute Automation: this lane isn't played back");
        mute.onClick = [this] { if (const auto* l = lane()) owner.studio.setLaneMuted (busId, laneIndex, ! l->muted); };
        addAndMakeVisible (mute);
        append.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        append.setColour (juce::TextButton::textColourOffId, ParagonColours::textDim);
        append.setTooltip ("Append Automation Track: a lane for the next control");
        append.onClick = [this] { owner.studio.appendAutomationLane (busId, laneIndex); };
        addAndMakeVisible (append);
        refresh();
    }

    const pr7s::AutomationLane* lane() const
    {
        const auto* lanes = owner.studio.getAutomation (busId);
        return lanes != nullptr && juce::isPositiveAndBelow (laneIndex, (int) lanes->size()) ? &(*lanes)[(size_t) laneIndex] : nullptr;
    }

    void refresh()
    {
        if (const auto* l = lane())
        {
            control.setButtonText (owner.studio.getLaneName (busId, *l));
            mute.setToggleState (l->muted, juce::dontSendNotification);
        }
        repaint();
    }

    // Its value now (at the playhead, as the lane has it; else the control's own).
    juce::String valueText() const
    {
        const auto* l = lane();
        if (l == nullptr)
            return {};
        const auto& studio = owner.studio;
        const auto value = l->points.empty() || ! studio.getAutomationRead (busId) || l->muted
                             ? studio.getControlValue (busId, *l)
                             : *l->valueAt (const_cast<pr7s::Studio&> (studio).getEngine().getPosition());
        using T = pr7s::AutomationLane::Target;
        switch (l->target)
        {
            case T::send:
            case T::volume: { const auto db = pr7s::travelToVolume (value); return db <= -59.9f ? juce::String ("-inf") : juce::String (db, 1) + " dB"; }
            case T::pan:    { const auto p = juce::roundToInt (pr7s::travelToPan (value) * 100.0f); return p == 0 ? juce::String ("C") : (p < 0 ? "L " : "R ") + juce::String (std::abs (p)); }
            case T::mute:   return value >= 0.5f ? "ON" : "OFF";
            case T::instrument:
            case T::insert: break;
        }
        return juce::String (juce::roundToInt (value * 100.0f)) + "%";
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0b090c));
        g.fillRect (bounds);
        g.setColour (juce::Colour (0xff050405));
        g.fillRect (bounds.withY (bounds.getBottom() - 1.0f).withHeight (1.0f));
        const auto* track = owner.studio.getProject().findTrack (busId);
        g.setColour ((track != nullptr ? track->colour : ParagonColours::accent).withAlpha (0.45f));
        g.fillRect (bounds.withWidth (6.0f).reduced (0.0f, 1.0f));
        g.setColour (ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::textFont (12.0f));
        g.drawText (valueText(), valueArea, juce::Justification::centredRight, false);
    }

    void resized() override
    {
        auto area = getLocalBounds().withTrimmedLeft (22).reduced (6, 6);
        auto top = area.removeFromTop (22);
        mute.setBounds (top.removeFromRight (30));
        top.removeFromRight (6);
        valueArea = top.removeFromRight (60).toFloat();
        top.removeFromRight (6);
        control.setBounds (top);
        append.setBounds (2, getHeight() - 20, 18, 18);
    }

    TrackList& owner;
    const int busId, laneIndex;
    juce::TextButton control, mute { "M" }, append { "+" };
    juce::Rectangle<float> valueArea;
};

//==============================================================================
// The master's row: its R and W, and its automation lanes' arrow.
class TrackList::MasterRow final : public juce::Component
{
public:
    explicit MasterRow (TrackList& ownerList) : owner (ownerList)
    {
        AutomationKeys::setUp (read, write, owner.studio, pr7s::Engine::masterBus);
        addAndMakeVisible (read);
        addAndMakeVisible (write);
        arrow.setTooltip ("Show/Hide Automation: the master's automation lanes (its volume, its effects)");
        arrow.onClick = [this] { owner.studio.setAutomationOpen (pr7s::Engine::masterBus, ! owner.studio.isAutomationOpen (pr7s::Engine::masterBus)); };
        addAndMakeVisible (arrow);
        refresh();
    }

    void refresh()
    {
        AutomationKeys::refresh (read, write, owner.studio, pr7s::Engine::masterBus);
        arrow.open = owner.studio.isAutomationOpen (pr7s::Engine::masterBus);
        arrow.repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff140f14));
        g.fillRect (bounds);
        g.setColour (juce::Colour (0xff050405));
        g.fillRect (bounds.withY (bounds.getBottom() - 1.0f).withHeight (1.0f));
        g.setColour (ParagonColours::accent);
        g.fillRect (bounds.withWidth (6.0f).reduced (0.0f, 1.0f));
        g.setColour (ParagonColours::text);
        g.setFont (ParagonLookAndFeel::titleFont (13.0f));
        g.drawText ("MASTER", bounds.withTrimmedLeft (24.0f).withTrimmedRight (80.0f), juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (6, 0).withSizeKeepingCentre (getWidth() - 12, juce::jmin (24, getHeight() - 10));
        write.setBounds (area.removeFromRight (26));
        area.removeFromRight (2);
        read.setBounds (area.removeFromRight (26));
        arrow.setBounds (2, getHeight() - 18, 16, 16);
    }

    TrackList& owner;
    juce::TextButton read { "R" }, write { "W" };
    Arrow arrow;
};

//==============================================================================
TrackList::TrackList (pr7s::Studio& studioToUse, Timeline& timelineToUse)
    : studio (studioToUse), timeline (timelineToUse)
{
    addTrack.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    addTrack.setTooltip ("Add a track, with the Paragon Synth or one of your VST3 instruments");
    addTrack.onClick = [this] { showAddTrackMenu(); };
    autoScrollButton.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    autoScrollButton.setWantsKeyboardFocus (false);
    autoScrollButton.onClick = [this] { if (onAutoScrollToggle) onAutoScrollToggle(); };
    addAndMakeVisible (autoScrollButton);
    autoScrollMenu.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    autoScrollMenu.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    autoScrollMenu.setWantsKeyboardFocus (false);
    autoScrollMenu.setTooltip ("Auto-Scroll's settings: Page Scroll or Stationary Cursor; Suspend Auto-Scroll when Editing");
    autoScrollMenu.onClick = [this]
    {
        timeline.autoScroll.showMenu (autoScrollMenu, [this] { if (timeline.onAutoScrollChange) timeline.onAutoScrollChange(); });
    };
    addAndMakeVisible (autoScrollMenu);
    refreshAutoScroll();
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
    {
        row->setLevel (studio.getEngine().takeTrackPeak (row->trackId));
        row->followMix();
    }
    for (auto& lane : laneRows)
        lane->repaint (lane->valueArea.toNearestInt());
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

    // The automation lanes' rows (made again when they change), and the master's.
    const auto layout = timeline.layout (studio);
    if (layout != lastLayout)
    {
        laneRows.clear();
        for (const auto& row : layout)
            if (row.kind == Timeline::Row::Kind::lane)
            {
                laneRows.push_back (std::make_unique<LaneRow> (*this, row.busId, row.laneIndex));
                addAndMakeVisible (*laneRows.back());
            }
        lastLayout = layout;
    }
    for (auto& lane : laneRows)
        lane->refresh();
    if (masterRow == nullptr)
    {
        masterRow = std::make_unique<MasterRow> (*this);
        addAndMakeVisible (*masterRow);
    }
    masterRow->refresh();
    resized();
    repaint();
}

void TrackList::resized()
{
    placeRows();
    addTrack.setBounds (getLocalBounds().removeFromBottom (addButtonHeight).reduced (10, 5));
    addTrack.toFront (false);
    auto header = getLocalBounds().removeFromTop (Timeline::rulerHeight).reduced (8, 6);
    autoScrollMenu.setBounds (header.removeFromRight (22));
    header.removeFromRight (3);
    autoScrollButton.setBounds (header.removeFromRight (96));
    autoScrollButton.toFront (false);
    autoScrollMenu.toFront (false);
}

void TrackList::refreshAutoScroll()
{
    timeline.autoScroll.refreshKey (autoScrollButton);
}

void TrackList::placeRows()
{
    // Each where the layout has it (so they line up with the arrangement's rows).
    auto laneRow = laneRows.begin();
    for (const auto& row : timeline.layout (studio))
    {
        const juce::Rectangle<int> bounds (0, Timeline::rulerHeight + row.top - timeline.scrollY, getWidth(), row.height);
        if (row.kind == Timeline::Row::Kind::track)
        {
            for (auto& r : rows)
                if (r->trackId == row.busId)
                    r->setBounds (bounds);
        }
        else if (row.kind == Timeline::Row::Kind::lane)
        {
            if (laneRow != laneRows.end())
                (*laneRow++)->setBounds (bounds);
        }
        else if (masterRow != nullptr)
        {
            masterRow->setBounds (bounds);
        }
    }
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
    const auto room = juce::jmax (0, timeline.contentHeight (studio) - (getHeight() - Timeline::rulerHeight - addButtonHeight));
    timeline.scrollY = juce::jlimit (0, room, timeline.scrollY - juce::roundToInt (wheel.deltaY * 300.0f));
    if (timeline.onScrollOrZoom)
        timeline.onScrollOrZoom();
}

void TrackList::showAddTrackMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("ADD AN AUDIO TRACK");
    menu.addItem (3, "Audio track (mono: a guitar, a voice)");
    menu.addItem (4, "Audio track (stereo: a pair of inputs)");
    menu.addSectionHeader ("ADD AN INSTRUMENT TRACK");
    menu.addItem (1, "Paragon Synth (built in)");
    const auto instruments = studio.getPluginHost().getInstruments();
    if (! instruments.isEmpty())
    {
        menu.addSeparator();
        for (int i = 0; i < instruments.size(); ++i)
            menu.addItem (100 + i, instruments[i].manufacturerName + ": " + instruments[i].name);
    }
    // FX and group channels (as the manual's Add Track dialog has them).
    const auto effects = studio.getPluginHost().getEffects();
    juce::PopupMenu withEffect;
    for (int i = 0; i < effects.size(); ++i)
        withEffect.addItem (1000 + i, effects[i].manufacturerName + ": " + effects[i].name);
    menu.addSectionHeader ("ADD AN FX CHANNEL OR A GROUP");
    menu.addItem (5, "FX channel (empty: add its effects in the mixer)");
    if (! effects.isEmpty())
        menu.addSubMenu ("FX channel with", withEffect);
    menu.addItem (6, "Group channel (a submix: route tracks to it)");
    menu.addSeparator();
    menu.addItem (2, instruments.isEmpty() ? "Find my VST3 instruments..." : "Find more VST3 instruments...");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addTrack), [this, instruments, effects] (int chosen)
    {
        if (chosen == 5)
        {
            studio.addFxChannel (nullptr);
        }
        else if (chosen == 6)
        {
            studio.addGroupChannel();
        }
        else if (chosen >= 1000 && chosen - 1000 < effects.size())
        {
            juce::String error;
            if (studio.addFxChannel (&effects.getReference (chosen - 1000), &error) == 0)
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load the effect", error);
        }
        else if (chosen == 1)
        {
            studio.addTrack ({});
        }
        else if (chosen == 3 || chosen == 4)
        {
            studio.addAudioTrack (chosen == 4);
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

void TrackList::showLaneMenu (int busId, int laneIndex, juce::Component& target)
{
    const auto* lanes = studio.getAutomation (busId);
    if (lanes == nullptr || ! juce::isPositiveAndBelow (laneIndex, (int) lanes->size()))
        return;
    const auto& current = (*lanes)[(size_t) laneIndex];
    const auto controls = studio.getAutomationControls (busId);
    // Each control; an asterisk where a hidden lane holds points (as the manual has it).
    juce::PopupMenu menu;
    juce::String group;
    juce::PopupMenu sub;
    const auto flush = [&]
    {
        if (group.isNotEmpty() && sub.getNumItems() > 0)
            menu.addSubMenu (group, sub);
        sub = {};
    };
    for (int i = 0; i < (int) controls.size(); ++i)
    {
        const auto& c = controls[(size_t) i];
        const auto lane = c.asLane();
        const auto existing = std::find_if (lanes->begin(), lanes->end(), [&] (const pr7s::AutomationLane& l) { return l.sameControl (lane); });
        const auto hiddenData = existing != lanes->end() && ! existing->shown && ! existing->points.empty();
        const auto name = c.name + (hiddenData ? " *" : "");
        if (c.group != group)
        {
            flush();
            group = c.group;
        }
        (group.isEmpty() ? menu : sub).addItem (100 + i, name, true, current.sameControl (lane));
    }
    flush();
    menu.addSeparator();
    menu.addItem (1, "Remove Parameter");
    menu.addItem (2, "Remove Unused Parameters");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target),
                        [safe = juce::Component::SafePointer<TrackList> (this), busId, laneIndex, controls] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        auto& s = safe->studio;
        if (chosen == 1)
            s.removeAutomationParameter (busId, laneIndex);
        else if (chosen == 2)
            s.removeUnusedAutomation (busId);
        else if (chosen >= 100 && chosen - 100 < (int) controls.size())
            s.setLaneControl (busId, laneIndex, controls[(size_t) (chosen - 100)]);
    });
}

void TrackList::showInstrumentMenu (int trackId, bool withAutomation)
{
    const auto* track = studio.getProject().findTrack (trackId);
    if (track == nullptr)
        return;
    auto* instance = studio.getEngine().getInstrument (trackId);
    const auto instruments = studio.getPluginHost().getInstruments();
    juce::PopupMenu menu, change;
    if (withAutomation)   // right-click on the track (as the manual has it)
    {
        menu.addItem (20, track->automationOpen ? "Hide Automation" : "Show Automation");
        menu.addItem (21, "Show All Used Automation");
        menu.addSeparator();
    }
    if (track->isChannel())
    {
        if (track->isFx())
        {
            const auto hasEffect = ! track->inserts.empty() && ! track->inserts.front().isEmpty() && studio.getEngine().getEffect (trackId, 0) != nullptr;
            menu.addItem (40, hasEffect ? "Open " + track->inserts.front().plugin.name + "'s window" : juce::String ("No effect yet (add one in the mixer)"), hasEffect);
            menu.addSeparator();
        }
        menu.addItem (2, "Delete track");
    }
    else
    {
        // Output: where its sound goes (the master, or a group channel).
        juce::PopupMenu output;
        output.addItem (30, "Master", true, track->output == 0);
        for (const auto& other : studio.getProject().tracks)
            if (other.isGroup())
                output.addItem (30000 + other.id, other.name, true, track->output == other.id);
        menu.addSubMenu ("Output", output);
        menu.addSeparator();
    }
    if (track->isAudio())
    {
        // The inputs switched on in SETTINGS (two, with none to ask).
        auto inputs = 2;
        if (auto* device = studio.getDeviceManager().getCurrentAudioDevice())
            inputs = juce::jmax (1, device->getActiveInputChannels().countNumberOfSetBits());
        menu.addSectionHeader ("RECORD FROM");
        for (int i = 0; i < inputs; ++i)
            menu.addItem (200 + i, "IN " + juce::String (i + 1) + " (mono)", true, ! track->stereoInput && track->input == i);
        for (int i = 0; i + 1 < inputs; i += 2)
            menu.addItem (300 + i, "IN " + juce::String (i + 1) + "+" + juce::String (i + 2) + " (stereo)", true, track->stereoInput && track->input == i);
        menu.addSeparator();
        menu.addItem (2, "Delete track");
    }
    else if (! track->isChannel())
    {
        menu.addItem (1, "Open " + track->instrument.getName() + "'s window", instance != nullptr && instance->hasEditor());
        change.addItem (10, "Paragon Synth (built in)");
        for (int i = 0; i < instruments.size(); ++i)
            change.addItem (100 + i, instruments[i].manufacturerName + ": " + instruments[i].name);
        menu.addSubMenu ("Change instrument", change);
        menu.addSeparator();
        menu.addItem (2, "Delete track");
    }

    // The selected tracks (this one among them): duplicate, move, delete; the empty ones.
    const auto several = studio.getSelectedTracks().size() > 1;
    menu.addSeparator();
    menu.addItem (50, several ? "Duplicate the selected tracks" : "Duplicate track");
    juce::PopupMenu moveTo;
    moveTo.addItem (51, "To the top");
    moveTo.addItem (52, "Up one");
    moveTo.addItem (53, "Down one");
    moveTo.addItem (54, "To the bottom");
    menu.addSubMenu (several ? "Move the selected tracks" : "Move track", moveTo);
    if (several)
        menu.addItem (56, "Delete the selected tracks (Shift+Delete)");
    menu.addItem (55, "Remove empty tracks", ! studio.getEmptyTracks().empty());
    menu.showMenuAsync (juce::PopupMenu::Options(), [this, trackId, instruments] (int chosen)
    {
        if (chosen == 50)
            studio.duplicateTracks (studio.getSelectedTracks());
        else if (chosen >= 51 && chosen <= 54)
            moveSelectedTracks ((MoveTo) (chosen - 51));
        else if (chosen == 55)
            studio.removeTracks (studio.getEmptyTracks());
        else if (chosen == 56)
            removeSelectedTracks();
        else if (chosen == 20)
            studio.setAutomationOpen (trackId, ! studio.isAutomationOpen (trackId));
        else if (chosen == 21)
            studio.showAllUsedAutomation();
        else if (chosen == 30)
            studio.setOutput (trackId, 0);
        else if (chosen > 30000)
            studio.setOutput (trackId, chosen - 30000);
        else if (chosen == 40 && onOpenEffect)
            onOpenEffect (trackId, 0);
        else if (chosen == 1 && onOpenInstrument)
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
        else if (chosen >= 300)
        {
            studio.setTrackInput (trackId, chosen - 300, true);
        }
        else if (chosen >= 200)
        {
            studio.setTrackInput (trackId, chosen - 200, false);
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

//==============================================================================
std::vector<int> TrackList::tracksToMove (int trackId) const
{
    return studio.isTrackSelected (trackId) ? studio.getSelectedTracks() : std::vector<int> { trackId };
}

void TrackList::dragTrack (int, juce::Point<int> at)
{
    // The drop place: before the first track whose middle is below the mouse (else the end).
    const auto& tracks = studio.getProject().tracks;
    dropIndex = (int) tracks.size();
    dropY = -1;
    for (int i = 0; i < (int) tracks.size(); ++i)
        for (const auto& row : rows)
            if (row->trackId == tracks[(size_t) i].id && dropY < 0 && at.y < row->getBounds().getCentreY())
            {
                dropIndex = i;
                dropY = row->getY();
            }
    if (dropY < 0)
        for (const auto& row : rows)
            if (row->trackId == tracks.back().id)
                dropY = row->getBottom();
    repaint();
}

void TrackList::dropTracks (int trackId)
{
    if (dropIndex >= 0)
        studio.moveTracks (tracksToMove (trackId), dropIndex);
    dropIndex = -1;
    dropY = -1;
    repaint();
}

void TrackList::paintOverChildren (juce::Graphics& g)
{
    // Where the dragged tracks would go: a line between the rows.
    if (dropIndex >= 0 && dropY >= 0)
    {
        g.setColour (ParagonColours::accent);
        g.fillRect (juce::Rectangle<float> (0.0f, (float) dropY - 1.5f, (float) getWidth(), 3.0f));
    }
}

void TrackList::moveSelectedTracks (MoveTo where)
{
    const auto ids = studio.getSelectedTracks();
    if (ids.empty())
        return;
    const auto& tracks = studio.getProject().tracks;
    int first = (int) tracks.size(), last = -1;
    for (int i = 0; i < (int) tracks.size(); ++i)
        if (std::find (ids.begin(), ids.end(), tracks[(size_t) i].id) != ids.end())
        {
            first = juce::jmin (first, i);
            last = juce::jmax (last, i);
        }
    switch (where)
    {
        case MoveTo::first:    studio.moveTracks (ids, 0); break;
        case MoveTo::previous: studio.moveTracks (ids, juce::jmax (0, first - 1)); break;
        case MoveTo::next:     studio.moveTracks (ids, juce::jmin ((int) tracks.size(), last + 2)); break;
        case MoveTo::last:     studio.moveTracks (ids, (int) tracks.size()); break;
    }
}

void TrackList::removeSelectedTracks()
{
    const auto ids = studio.getSelectedTracks();
    if (ids.empty())
        return;
    // Empty ones go straight away; any with something on: asked first (as the manual has it; it can't be undone yet).
    auto withSomething = 0;
    for (const auto id : ids)
        withSomething += studio.isTrackEmpty (id) ? 0 : 1;
    if (withSomething == 0)
    {
        studio.removeTracks (ids);
        return;
    }
    const auto* first = studio.getProject().findTrack (ids.front());
    const auto what = ids.size() == 1 ? "\"" + first->name + "\" and its parts" : juce::String ((int) ids.size()) + " tracks and their parts";
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, ids.size() == 1 ? "Delete track" : "Delete tracks",
                                        "Delete " + what + "? This can't be undone.", "Delete", "Cancel", nullptr,
                                        juce::ModalCallbackFunction::create ([this, ids] (int result)
                                        {
                                            if (result == 1)
                                                studio.removeTracks (ids);
                                        }));
}

void TrackList::addTrackLikeSelected()
{
    // Double-click under the tracks: a track like the selected one (an audio
    // track mono or stereo as it is, a group or an FX channel), else one on the Paragon Synth.
    const auto* selected = studio.getProject().findTrack (studio.getSelectedTrack());
    if (selected != nullptr && selected->isAudio())
        studio.addAudioTrack (selected->stereoInput);
    else if (selected != nullptr && selected->isGroup())
        studio.addGroupChannel();
    else if (selected != nullptr && selected->isFx())
        studio.addFxChannel (nullptr);
    else
        studio.addTrack ({});
}

void TrackList::mouseDoubleClick (const juce::MouseEvent& event)
{
    // The empty space under the tracks (not the header, not + ADD TRACK).
    if (event.y > Timeline::rulerHeight && event.y < getHeight() - addButtonHeight)
        addTrackLikeSelected();
}
