#include "TrackList.h"
#include "ui/ChordUi.h"

#include "ui/AutomationKeys.h"
#include "ui/AutoFadesPage.h"
#include "ui/LinkGroupPage.h"
#include "ui/LevelMeter.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/TrackActions.h"
#include "ui/ValueSlider.h"

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
        setOpaque (true);   // (0.83.2: it fills itself: the list behind isn't painted under it again)
        // (0.9.9: M, S, REC, R and W aren't on the row, as the owner asked: they're in the Inspector and the mixer.
        // The row says which are on, under the name.)
        AutomationKeys::setUp (read, write, owner.studio, trackId);
        arrow.setTooltip ("Show/Hide Automation: this track's automation lanes");
        arrow.onClick = [this] { owner.studio.setAutomationOpen (trackId, ! owner.studio.isAutomationOpen (trackId)); };
        addAndMakeVisible (arrow);
        // (0.50.0) A folder's Expand/Collapse, beside its name.
        folderArrow.setTooltip ("Expand / Collapse Folder: its tracks shown under it, or folded away (they still play)");
        folderArrow.onClick = [this] { if (auto* t = track()) owner.studio.setFolderOpen (trackId, ! t->folderOpen); };
        addChildComponent (folderArrow);
        name.setFont (ParagonLookAndFeel::titleFont (22.0f));   // (0.9.2: 1.7 times the old 13, the owner asked for 1.5 to 2)
        name.setMinimumHorizontalScale (0.6f);
        name.setColour (juce::Label::textColourId, ParagonColours::text);
        name.setEditable (false, true, false);   // double-click to rename
        name.setTooltip ("The track's name: double-click to rename");
        name.onTextChange = [this] { owner.studio.renameTrack (trackId, name.getText()); };
        name.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (name);
        // (0.73.0, the owner asked) The slot's number: 1 at the top. It belongs to the place, not the
        // track (a track moved takes its new place's number). Double-click it and type a number: the
        // track moves to that slot and the others make room.
        number.setFont (ParagonLookAndFeel::titleFont (18.0f));
        number.setJustificationType (juce::Justification::centred);
        number.setColour (juce::Label::textColourId, ParagonColours::textDim);
        number.setEditable (false, true, false);
        number.setTooltip ("The track's place in the list: double-click and type a number to move it there");
        number.setInterceptsMouseClicks (false, false);
        number.onTextChange = [this]
        {
            const auto to = number.getText().trim().getIntValue();
            number.setText (juce::String (slot), juce::dontSendNotification);
            if (to < 1 || to == slot)
                return;
            // (Later: moving the track makes the rows again, this one too.)
            juce::Component::SafePointer<TrackList> list (&owner);
            const auto id = trackId;
            juce::MessageManager::callAsync ([list, id, to] { if (list != nullptr) list->moveTrackToSlot (id, to); });
        };
        addAndMakeVisible (number);

        for (auto* key : { &mute, &solo, &arm })
            key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        monitor.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        lanes.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        lanes.setTooltip ("LANES (Show Lanes): the track's takes, each lane under it; the track's row shows what's heard. "
                          "Comp with the COMP tool; right-click a part for To Front");
        lanes.onClick = [this] { if (auto* t = track()) owner.studio.setShowLanes (trackId, ! t->showLanes); };
        addChildComponent (lanes);
        monitor.setTooltip ("MON: hear what comes in from the interface (your guitar), through the track's effects");
        monitor.onClick = [this] { if (auto* t = track()) owner.studio.setMonitor (trackId, ! t->monitor); };
        addChildComponent (monitor);
        mute.setTooltip ("Mute this track");
        solo.setTooltip ("Solo: hear only the soloed tracks");
        arm.setTooltip ("REC: record-arm: recording and live playing go to this track");
        mute.onClick = [this] { if (auto* t = track()) owner.studio.setMute (trackId, ! t->mute); };
        solo.onClick = [this] { if (auto* t = track()) owner.studio.setSolo (trackId, ! soloed (*t)); };
        addChildComponent (mute);   // (0.50.0: shown on a folder's row: its M and S act on its tracks)
        addChildComponent (solo);
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
        volume.setDoubleClickReturnValue (true, 0.0, juce::ModifierKeys::commandModifier);
        volume.parse = [] (const juce::String& text) { return ValueEntry::parseDb (text); };
        volume.setPopupDisplayEnabled (true, true, nullptr);
        volume.textFromValueFunction = [] (double db) { return db <= -59.9 ? juce::String ("-inf dB") : juce::String (db, 1) + " dB"; };
        volume.setTooltip ("The track's volume (double-click to type a value; Ctrl-click for 0 dB)");
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
    bool soloed (const pr7s::Track& t) const
    {
        return t.isFolder() && ! t.folderGroup ? owner.studio.isFolderSoloed (trackId) : t.solo;
    }
    int indent() const { return 14 * owner.studio.getProject().folderDepth (trackId); }   // (0.50.0) in folders: indented
    static constexpr int numberWidth = 30;   // (0.73.0) the slot's number, left of everything indented

    void setSlot (int newSlot)
    {
        if (slot == newSlot)
            return;
        slot = newSlot;
        if (! number.isBeingEdited())
            number.setText (juce::String (slot), juce::dontSendNotification);
    }

    void refresh()
    {
        const auto* t = track();
        if (t == nullptr)
            return;
        if (! name.isBeingEdited())
            name.setText (t->name, juce::dontSendNotification);
        name.setColour (juce::Label::textColourId, t->disabled ? ParagonColours::textDim : ParagonColours::text);
        mute.setToggleState (t->mute, juce::dontSendNotification);
        solo.setToggleState (soloed (*t), juce::dontSendNotification);
        folderArrow.setVisible (t->isFolder());
        folderArrow.open = t->folderOpen;
        folderArrow.repaint();
        mute.setVisible (t->isFolder());
        solo.setVisible (t->isFolder());
        arm.setToggleState (t->armed, juce::dontSendNotification);
        if (t->isChannel())
        {
            // An FX or group channel: what it is (an FX channel: its first effect).
            const auto& inserts = t->inserts;
            instrument.setButtonText (t->isFolder() ? juce::String ("FOLDER") : t->isGroup() ? juce::String ("GROUP CHANNEL")
                                      : ! inserts.empty() && ! inserts.front().isEmpty() ? "FX: " + inserts.front().plugin.name : juce::String ("FX CHANNEL"));
            instrument.setTooltip (t->isGroup() ? "A group channel: the tracks routed to it (their Output), through its effects and fader. Click to delete it"
                                                : "An FX channel: what the tracks' sends give it, through its effects. Click to open its effect's window, or to delete it");
        }
        else
        {
            instrument.setButtonText (t->disabled ? juce::String ("DISABLED") : t->isAudio() ? inputName (*t) : t->instrument.getName());
            instrument.setTooltip (t->isAudio() ? "The input it records from and where it goes (Output): click to choose, or to delete the track"
                                                : "The instrument and where it goes (Output): click to open its window, change it, or delete the track");
            if (t->frozen)
                instrument.setTooltip ("Frozen: played from its freeze file, its instrument and effects unloaded. Right-click for Unfreeze Track");
            if (t->disabled)
                instrument.setTooltip ("Disabled: silent, its instrument and effects unloaded (their settings kept). Click for Enable Track");
        }
        monitor.setVisible (false);   // (0.35.0: in the Inspector)
        monitor.setToggleState (t->monitor, juce::dontSendNotification);
        lanes.setToggleState (t->showLanes, juce::dontSendNotification);
        lanes.setVisible (false);
        AutomationKeys::refresh (read, write, owner.studio, trackId);
        arrow.open = t->automationOpen;
        arrow.repaint();
        followMix();
        resized();   // (0.50.0: a folder's keys follow its group channel)
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
        if (reading.push (peak) && ! levelArea.isEmpty())
            if (const auto now = reading.shown (levelArea.getWidth()); now != shownLevel)   // (0.83.1: only when a segment changes)
            {
                shownLevel = now;
                repaint (levelArea.toNearestInt().expanded (2));
            }
    }

    void paint (juce::Graphics& g) override
    {
        const auto* t = track();
        if (t == nullptr)
        {
            g.fillAll (juce::Colour (0xff100c11));   // (opaque: it must cover itself)
            return;
        }
        const auto bounds = getLocalBounds().toFloat();
        const auto selected = owner.studio.isTrackSelected (trackId);
        g.setColour (selected ? juce::Colour (0xff1f1720) : juce::Colour (0xff100c11));
        g.fillRect (bounds);
        g.setColour (juce::Colour (0xff050405));
        g.fillRect (bounds.withY (bounds.getBottom() - 1.0f).withHeight (1.0f));
        // (0.73.0, the owner asked) The track's colour over its row, as much as chosen for it.
        if (t->listColourOn)
        {
            g.setColour ((t->disabled ? juce::Colour (0xff4a454c) : t->colour).withAlpha ((float) juce::jlimit (1, 100, t->listColourAmount) / 100.0f));
            g.fillRect (bounds.withTrimmedLeft (6.0f).withTrimmedBottom (1.0f));
        }
        g.setColour (t->disabled ? juce::Colour (0xff4a454c) : t->colour);   // disabled: its colour greyed (as the manual has it)
        g.fillRect (bounds.withWidth (6.0f).reduced (0.0f, 1.0f));
        // (0.50.0) In folders: a faint line for each folder round it; a folder: its colour's tab.
        for (int level = 1; level * 14 <= indent(); ++level)
        {
            g.setColour (ParagonColours::textDim.withAlpha (0.25f));
            g.fillRect (juce::Rectangle<float> ((float) numberWidth + 6.0f + (float) level * 14.0f - 4.0f, 0.0f, 1.0f, bounds.getHeight()));
        }
        if (t->isFolder())
        {
            g.setColour (t->colour.withAlpha (0.12f));
            g.fillRect (bounds.withTrimmedLeft (6.0f + (float) (numberWidth + indent())));
        }
        if (selected)
        {
            g.setColour (ParagonColours::accent.withAlpha (0.6f));
            g.drawRect (bounds.withTrimmedLeft (6.0f), 1.0f);
        }

        // What's on, under its name (0.9.9: the keys are in the Inspector and the mixer): SOLO, MUTED, REC, READ, WRITE, FROZEN.
        // (0.83.2: not worked out for a redraw that doesn't reach it: the meter's, 30 times a second.)
        if (getHeight() >= 66 && g.getClipBounds().intersects (name.getBounds().withTrimmedLeft (4).removeFromBottom (16)))
        {
            juce::StringArray states;
            if (t->isFolder())
            {
                const auto count = (int) owner.studio.getProject().folderMembers (trackId, false).size();
                states.add (juce::String (count) + (count == 1 ? " TRACK" : " TRACKS") + (t->folderOpen ? "" : " (FOLDED)"));
                if (t->folderGroup) states.add ("GROUP CHANNEL");
            }
            if (soloed (*t)) states.add ("SOLO");
            if (t->mute) states.add ("MUTED");
            if (t->armed) states.add ("REC");
            if (t->automationWrite) states.add ("WRITE");
            else if (t->automationRead) states.add ("READ");
            if (t->frozen) states.add ("FROZEN");
            if (t->follow != pr7s::FollowMode::off) states.add ("FOLLOWS " + pr7s::followModeName (t->follow));   // (0.36.0)
            if (t->liveFollow != pr7s::LiveFollow::off) states.add ("LIVE: " + pr7s::liveFollowName (t->liveFollow));
            if (const auto* link = owner.studio.getLinkGroupOf (t->id)) states.add ("LINKED: " + link->name.toUpperCase());
            if (! states.isEmpty())
            {
                g.setColour (t->solo || t->armed ? ParagonColours::accent : juce::Colour (0xfff4eeea).withAlpha (0.75f));
                g.setFont (font (13.0f));
                g.drawText (states.joinIntoString ("  "), name.getBounds().withTrimmedLeft (4).removeFromBottom (16), juce::Justification::bottomLeft, false);
            }
        }

        // Its level: PR7-C's meter, lying down (orange at the left to pink-red at the right).
        if (! levelArea.isEmpty())   // (none in a short row)
            LevelMeter::paintSegments (g, levelArea, LevelMeter::proportionForDb (reading.db),
                                   reading.heldDb > LevelMeter::minDb ? LevelMeter::proportionForDb (reading.heldDb) : -1.0f, false);
    }

    void resized() override
    {
        // The name large on the left (0.9.2: the owner's choice; the
        // instrument is in the Inspector and the right-click menu), what's on
        // under it; on the right MON (audio), the level and the volume (0.9.9:
        // no M S REC R W, the owner asked). Shorter than two lines (zoomed out): the volume only.
        const auto compact = getHeight() < 66;
        instrument.setVisible (false);
        levelArea = {};
        number.setBounds (6, 0, numberWidth, compact ? getHeight() : juce::jmin (getHeight(), 40));
        auto area = getLocalBounds().withTrimmedLeft (12 + numberWidth + indent()).reduced (6, compact ? juce::jmax (2, (getHeight() - 24) / 2) : 7);
        const auto* t = track();
        if (t != nullptr && t->isFolder())
        {
            // (0.50.0) A folder: its arrow, its name, M and S (and with its group channel, the volume).
            folderArrow.setBounds (area.removeFromLeft (18).withSizeKeepingCentre (18, 18).translated (0, compact ? 0 : -8));
            area.removeFromLeft (2);
            auto right = area.removeFromRight (t->folderGroup ? 92 : 58).withSizeKeepingCentre (t->folderGroup ? 92 : 58, 24);
            area.removeFromRight (6);
            volume.setVisible (t->folderGroup);
            if (t->folderGroup)
            {
                volume.setBounds (right.removeFromRight (28));
                right.removeFromRight (6);
            }
            solo.setBounds (right.removeFromRight (26));
            right.removeFromRight (4);
            mute.setBounds (right.removeFromRight (26));
            monitor.setBounds ({}), lanes.setBounds ({});
            name.setBounds (area);
            arrow.setVisible (t->folderGroup);
            arrow.setBounds (numberWidth + indent(), getHeight() - 18, 16, 16);
            return;
        }
        arrow.setVisible (true);
        // On the right, level with the name: MON (an audio track's), the level, the volume.
        // (0.36.1, the owner asked: the name runs up to the meter. The strip is only as wide as what's
        // in it: the level 44 + 4 + the volume 28; it was 180, kept from when MON and LANES sat there.)
        const auto rightWidth = compact ? 32 : 76;
        auto right = area.removeFromRight (rightWidth).withSizeKeepingCentre (rightWidth, 24);
        area.removeFromRight (6);
        volume.setVisible (true);
        volume.setBounds (right.removeFromRight (28));
        right.removeFromRight (4);
        if (! compact)
        {
            levelArea = right.removeFromRight (44).toFloat().withSizeKeepingCentre (44.0f, 8.0f);
        }
        // (0.35.0, the owner asked) LANES and MON are in the Inspector, under RENDER and PRESET.
        monitor.setBounds ({}), lanes.setBounds ({});
        monitor.setVisible (false);
        lanes.setVisible (false);
        name.setBounds (area);
        arrow.setBounds (numberWidth + indent(), getHeight() - 18, 16, 16);
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
        if (number.getBounds().contains (event.getPosition()))
            number.showEditor();
        else if (name.getBounds().contains (event.getPosition()))
            name.showEditor();
    }

    TrackList& owner;
    const int trackId;
    bool dragged = false, plainClickOnSelected = false;
    juce::Label name, number;
    int slot = 0;
    juce::TextButton mute { "M" }, solo { "S" }, arm { "REC" }, monitor { "MON" }, instrument, read { "R" }, write { "W" };
    juce::TextButton lanes { "LANES" };   // Show Lanes (0.29.0)
    ValueSlider volume;   // (double-click: type a value, 0.9.9)
    Arrow arrow, folderArrow;
    juce::Rectangle<float> levelArea;
    LevelMeter::Ballistics reading;
    int shownLevel = -1;   // (0.83.1) what the meter last drew
};

//==============================================================================
// A lane of takes (0.29.0, the manual's Lanes): LANE n, how many takes are on it, and S (the
// manual's lane Solo: only this lane heard; the track's own S too, to hear it alone).
class TrackList::TakeRow final : public juce::Component
{
public:
    TakeRow (TrackList& ownerList, int track, int laneNumber) : owner (ownerList), trackId (track), lane (laneNumber)
    {
        solo.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        solo.setTooltip ("Solo this lane: only its takes are heard (solo the track too, to hear it alone)");
        solo.onClick = [this] { owner.studio.setLaneSolo (trackId, lane); };
        addAndMakeVisible (solo);
        refresh();
    }

    void refresh()
    {
        const auto* t = owner.studio.getProject().findTrack (trackId);
        solo.setToggleState (t != nullptr && t->soloLane == lane, juce::dontSendNotification);
        solo.setVisible (t != nullptr && lane < t->numLanes());
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto* t = owner.studio.getProject().findTrack (trackId);
        if (t == nullptr)
            return;
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0d0a0e));
        g.fillRect (bounds);
        g.setColour (juce::Colour (0xff050405));
        g.fillRect (bounds.withY (bounds.getBottom() - 1.0f).withHeight (1.0f));
        g.setColour (t->colour.withAlpha (0.6f));
        g.fillRect (bounds.withWidth (6.0f).reduced (0.0f, 1.0f));
        auto takes = 0;
        for (const auto& p : t->parts)
            takes += p.lane == lane ? 1 : 0;
        g.setColour (lane < t->numLanes() ? ParagonColours::text : ParagonColours::textDim);
        g.setFont (font (14.0f));
        const auto text = lane < t->numLanes() ? "LANE " + juce::String (lane + 1) + (takes > 0 ? "   " + juce::String (takes) + (takes == 1 ? " TAKE" : " TAKES") : juce::String())
                                               : juce::String ("NEW LANE (DRAG A TAKE HERE)");
        g.drawText (text, bounds.withTrimmedLeft (24.0f).withTrimmedRight (50.0f), juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        solo.setBounds (getLocalBounds().removeFromRight (40).withSizeKeepingCentre (30, juce::jmin (24, getHeight() - 8)));
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        owner.studio.selectTrack (trackId);
    }

    TrackList& owner;
    const int trackId, lane;
    juce::TextButton solo { "S" };
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
        // (0.72.0, the owner: removing one lane was only in its menu) X: Remove Parameter, this lane only.
        remove.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        remove.setTooltip ("Remove this automation lane (its points too; Undo puts it back). The others stay");
        remove.onClick = [this]
        {
            // (Later: the rows are made again as the lane goes, this one with them.)
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<TrackList> (&owner), bus = busId, index = laneIndex]
            {
                if (safe != nullptr)
                    safe->studio.removeAutomationParameter (bus, index);
            });
        };
        addAndMakeVisible (remove);
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
        remove.setBounds (top.removeFromRight (26));
        top.removeFromRight (4);
        mute.setBounds (top.removeFromRight (30));
        top.removeFromRight (6);
        valueArea = top.removeFromRight (60).toFloat();
        top.removeFromRight (6);
        control.setBounds (top);
        append.setBounds (2, getHeight() - 20, 18, 18);
    }

    TrackList& owner;
    const int busId, laneIndex;
    juce::TextButton control, mute { "M" }, append { "+" }, remove { "X" };
    juce::Rectangle<float> valueArea;
    juce::String shownValue;   // (0.83.1) the value the row last drew
};

juce::Button* TrackList::getLaneRemoveKey (int busId, int laneIndex)
{
    for (auto& row : laneRows)
        if (row->busId == busId && row->laneIndex == laneIndex)
            return &row->remove;
    return nullptr;
}

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
        repaint();   // (chosen or not)
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto selected = owner.studio.isMasterSelected();
        g.setColour (selected ? juce::Colour (0xff1f1720) : juce::Colour (0xff140f14));
        g.fillRect (bounds);
        g.setColour (juce::Colour (0xff050405));
        g.fillRect (bounds.withY (bounds.getBottom() - 1.0f).withHeight (1.0f));
        g.setColour (pr7s::FamilyColours::red);
        g.fillRect (bounds.withWidth (6.0f).reduced (0.0f, 1.0f));
        if (selected)
        {
            g.setColour (ParagonColours::accent.withAlpha (0.6f));
            g.drawRect (bounds.withTrimmedLeft (6.0f), 1.0f);
        }
        g.setColour (ParagonColours::text);
        g.setFont (ParagonLookAndFeel::titleFont (13.0f));
        g.drawText ("MASTER", bounds.withTrimmedLeft (24.0f).withTrimmedRight (80.0f), juce::Justification::centredLeft, false);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        owner.studio.selectMaster();   // its Inspector (0.9.12)
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
// The tempo track's row (0.10.0, the manual's Tempo Track Controls): ON
// (Activate Tempo Track), LOCK, the Type of New Tempo Points, and the tempo
// at the cursor (double-click to type one: the event in effect there changes).
class TrackList::TempoRow final : public juce::Component,
                                  public juce::SettableTooltipClient
{
public:
    explicit TempoRow (TrackList& ownerList) : owner (ownerList)
    {
        for (auto* key : { &on, &lock, &type })
        {
            key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
            key->setWantsKeyboardFocus (false);
            addAndMakeVisible (*key);
        }
        on.setClickingTogglesState (false);
        lock.setClickingTogglesState (false);
        on.setTooltip ("Activate Tempo Track: the song follows the tempo track's curve; off, the fixed tempo (the transport's)");
        lock.setTooltip ("Lock: the tempo events can't be changed");
        type.setTooltip ("Type of New Tempo Points: Ramp (gradually from the point before), Step (a jump), Auto (as the point before)");
        on.onClick = [this] { owner.studio.setTempoTrackOn (! owner.studio.getProject().tempoTrackOn); };
        lock.onClick = [this] { owner.studio.setTempoLocked (! owner.studio.getProject().tempoLocked); };
        type.onClick = [this]
        {
            using T = pr7s::Project::NewTempoType;
            const auto now = owner.studio.getProject().newTempoType;
            owner.studio.setNewTempoType (now == T::ramp ? T::step : now == T::step ? T::automatic : T::ramp);
        };
        setTooltip ("The tempo track: right-click for its menu; double-click the tempo to type one");
        refresh();
    }

    void refresh()
    {
        const auto& project = owner.studio.getProject();
        on.setToggleState (project.tempoTrackOn, juce::dontSendNotification);
        lock.setToggleState (project.tempoLocked, juce::dontSendNotification);
        using T = pr7s::Project::NewTempoType;
        type.setButtonText (project.newTempoType == T::ramp ? "RAMP" : project.newTempoType == T::step ? "STEP" : "AUTO");
        followTempo();
        repaint();
    }

    void followTempo()
    {
        const auto now = owner.studio.getTempoNow();
        if (std::abs (now - shownTempo) > 0.004)
        {
            shownTempo = now;
            repaint (tempoArea.toNearestInt().expanded (2));
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff140f14));
        g.fillRect (bounds);
        g.setColour (juce::Colour (0xff050405));
        g.fillRect (bounds.withY (bounds.getBottom() - 1.0f).withHeight (1.0f));
        g.setColour (pr7s::FamilyColours::orange);
        g.fillRect (bounds.withWidth (6.0f).reduced (0.0f, 1.0f));
        g.setColour (ParagonColours::text);
        g.setFont (ParagonLookAndFeel::titleFont (13.0f));
        g.drawText ("TEMPO", nameArea, juce::Justification::centredLeft, false);
        g.setColour (owner.studio.getProject().tempoTrackOn ? ParagonColours::text : ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::titleFont (15.0f));
        g.drawText (juce::String (shownTempo, 2) + " BPM", tempoArea, juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        auto area = getLocalBounds().withTrimmedLeft (24).reduced (0, 4);
        auto top = area.removeFromTop (juce::jmin (24, area.getHeight() / 2));
        nameArea = top.removeFromLeft (70).toFloat();
        top.removeFromRight (6);
        type.setBounds (top.removeFromRight (46));
        top.removeFromRight (3);
        lock.setBounds (top.removeFromRight (44));
        top.removeFromRight (3);
        on.setBounds (top.removeFromRight (34));
        tempoArea = area.removeFromTop (juce::jmin (24, area.getHeight())).toFloat();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && owner.onTempoMenu)
            owner.onTempoMenu();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (! tempoArea.contains (e.position))
            return;
        auto* box = new FamilyAlertWindow ("Tempo", owner.studio.getProject().followsTempoTrack()
                                                        ? "The tempo where the cursor is (the tempo event in effect there changes)."
                                                        : "The song's tempo (the tempo track is off).",
                                           juce::MessageBoxIconType::NoIcon);
        box->addTextEditor ("bpm", juce::String (owner.studio.getTempoNow(), 2), "BPM");
        box->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        box->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        box->enterModalState (true, juce::ModalCallbackFunction::create ([box, safe = juce::Component::SafePointer<TempoRow> (this)] (int result)
        {
            if (safe != nullptr && result == 1)
                if (const auto bpm = box->getTextEditorContents ("bpm").replaceCharacter (',', '.').getDoubleValue(); bpm > 0.0)
                    safe->owner.studio.setTempoAtCursor (bpm);
        }), true);
    }

    TrackList& owner;
    juce::TextButton on { "ON" }, lock { "LOCK" }, type { "RAMP" };
    juce::Rectangle<float> nameArea, tempoArea;
    double shownTempo = -1.0;
};

//==============================================================================
// The signature track's row (0.45.0, the manual's Signature Track Controls): LOCK and the
// signature at the playhead (double-click: insert one there).
class TrackList::SignatureRow final : public juce::Component,
                                      public juce::SettableTooltipClient
{
public:
    explicit SignatureRow (TrackList& ownerList) : owner (ownerList)
    {
        lock.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        lock.setWantsKeyboardFocus (false);
        lock.setClickingTogglesState (false);
        lock.setTooltip ("Lock: the signature events can't be changed");
        lock.onClick = [this] { owner.studio.setSignatureLocked (! owner.studio.getProject().signatureLocked); };
        addAndMakeVisible (lock);
        setTooltip ("The signature track: right-click for its menu; double-click to insert a signature at the cursor's bar");
        refresh();
    }

    void refresh()
    {
        lock.setToggleState (owner.studio.getProject().signatureLocked, juce::dontSendNotification);
        followSignature();
        repaint();
    }

    void followSignature()
    {
        const auto now = owner.studio.getSignatureNow();
        if (now != shown)
        {
            shown = now;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff140f14));
        g.fillRect (bounds);
        g.setColour (juce::Colour (0xff050405));
        g.fillRect (bounds.withY (bounds.getBottom() - 1.0f).withHeight (1.0f));
        g.setColour (pr7s::FamilyColours::pink);
        g.fillRect (bounds.withWidth (6.0f).reduced (0.0f, 1.0f));
        g.setColour (ParagonColours::text);
        g.setFont (ParagonLookAndFeel::titleFont (13.0f));
        g.drawText ("SIGNATURE", nameArea, juce::Justification::centredLeft, false);
        g.setFont (ParagonLookAndFeel::titleFont (15.0f));
        g.drawText (juce::String (shown.numerator) + "/" + juce::String (shown.denominator), valueArea, juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        auto area = getLocalBounds().withTrimmedLeft (24).reduced (0, 4);
        nameArea = area.removeFromLeft (84).toFloat();
        area.removeFromRight (6);
        lock.setBounds (area.removeFromRight (44).withSizeKeepingCentre (44, juce::jmin (24, area.getHeight())));
        valueArea = area.toFloat();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && owner.onSignatureMenu)
            owner.onSignatureMenu();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (owner.onInsertSignature)
            owner.onInsertSignature();
    }

    TrackList& owner;
    juce::TextButton lock { "LOCK" };
    juce::Rectangle<float> nameArea, valueArea;
    pr7s::SignatureEvent shown { -1, 0, 0 };
};

//==============================================================================
// The marker track's row (0.49.0, the manual's Marker Track controls): its name (double-click:
// rename), + POS (Add Position Marker at the cursor), + CYC (Add Cycle Marker at the locators;
// Shift-click: and name it), LOCK, and GO TO (the manual's Locate, Cycle and Zoom pop-up menus).
// The SECTIONS row (0.90.0): its name, the song's length in bars, and + SECTION (a block after the last).
class TrackList::SectionsRow final : public juce::Component,
                                     public juce::SettableTooltipClient
{
public:
    explicit SectionsRow (TrackList& ownerList) : owner (ownerList)
    {
        add.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        add.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
        add.setWantsKeyboardFocus (false);
        add.setTooltip ("Add a section after the last: intro, verse, pre-chorus, chorus, bridge, breakdown, solo...");
        add.onClick = [this] { if (owner.onAddSection) owner.onAddSection(); };
        addAndMakeVisible (add);
        // (0.114.0, the owner: "in the sections lane I need to be able to set the key of the song")
        key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
        key.setWantsKeyboardFocus (false);
        key.setTooltip ("The song's key: every part moves into it (chords, notes, what the composer built). "
                        "ALL SECTIONS SYNCED TO ONE KEY (on to start with): untick it for key changes, then each block's own key in its editor (right-click a block)");
        key.onClick = [this] { showKeyMenu(); };
        addAndMakeVisible (key);
        setTooltip ("The song's sections: drag a block to rearrange (its music goes with it), double-click one to open the composer on it, right-click for its menu");
        refresh();
    }

    void refresh()
    {
        const auto text = "KEY: " + ChordUi::spokenKey (owner.studio.getSongKey()).toUpperCase() + (owner.studio.isOneKey() ? "" : " +");
        if (key.getButtonText() != text)
            key.setButtonText (text);
        repaint();
    }

    void showKeyMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader ("THE SONG'S KEY (EVERY PART MOVES INTO IT)");
        const auto& keys = pr7s::Key::all();
        const auto now = owner.studio.getSongKey();
        juce::PopupMenu majors, minors;
        for (size_t i = 0; i < keys.size(); ++i)
            (keys[i].minor ? minors : majors).addItem (1 + (int) i, ChordUi::spokenKey (keys[i]).toUpperCase(), true, keys[i] == now);
        menu.addSubMenu ("Major Keys", majors, true, nullptr, ! now.minor);
        menu.addSubMenu ("Minor Keys", minors, true, nullptr, now.minor);
        menu.addSeparator();
        menu.addItem (1000, "All Sections Synced to One Key", true, owner.studio.isOneKey());
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (key),
                            [safe = juce::Component::SafePointer<SectionsRow> (this)] (int chosen)
                            {
                                if (safe == nullptr || chosen == 0)
                                    return;
                                auto& studio = safe->owner.studio;
                                if (chosen == 1000)
                                    studio.setOneKey (! studio.isOneKey());
                                else if (juce::isPositiveAndNotGreaterThan (chosen, (int) pr7s::Key::all().size()))
                                    studio.setSongKey (pr7s::Key::all()[(size_t) chosen - 1]);
                            });
    }

    juce::TextButton& getKeyButton() noexcept { return key; }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff140f14));
        g.fillRect (bounds);
        g.setColour (juce::Colour (0xff050405));
        g.fillRect (bounds.withY (bounds.getBottom() - 1.0f).withHeight (1.0f));
        g.setColour (pr7s::FamilyColours::pink);
        g.fillRect (bounds.withWidth (6.0f).reduced (0.0f, 1.0f));
        const auto& sections = owner.studio.getSongSections();
        g.setColour (ParagonColours::text);
        g.setFont (ParagonLookAndFeel::titleFont (14.0f));
        const auto bars = sections.totalBars();
        g.drawFittedText ("SECTIONS" + (bars > 0 ? "  " + juce::String (bars) + " BARS" : juce::String()), nameArea.toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.7f);
    }

    void resized() override
    {
        auto area = getLocalBounds().withTrimmedLeft (16).reduced (0, 4);
        area.removeFromRight (6);
        add.setBounds (area.removeFromRight (92).withSizeKeepingCentre (92, juce::jmin (24, area.getHeight())));
        area.removeFromRight (4);
        const auto keyWidth = juce::jmin (190, juce::jmax (0, area.getWidth() - 80));
        key.setBounds (area.removeFromRight (keyWidth).withSizeKeepingCentre (keyWidth, juce::jmin (24, area.getHeight())));
        area.removeFromRight (4);
        nameArea = area.toFloat();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && owner.onSectionsMenu)
            owner.onSectionsMenu();
    }

private:
    TrackList& owner;
    juce::TextButton add { "+ SECTION" }, key;
    juce::Rectangle<float> nameArea;
};

juce::Button* TrackList::getSongKeyButton() { return sectionsRow != nullptr ? &sectionsRow->getKeyButton() : nullptr; }
void TrackList::showSongKeyMenu() { if (sectionsRow != nullptr) sectionsRow->showKeyMenu(); }

class TrackList::MarkerRow final : public juce::Component,
                                   public juce::SettableTooltipClient
{
public:
    explicit MarkerRow (TrackList& ownerList) : owner (ownerList)
    {
        for (auto* key : { &addPosition, &addCycle, &lock, &goTo })
        {
            key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
            key->setWantsKeyboardFocus (false);
            addAndMakeVisible (*key);
        }
        goTo.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
        addPosition.setTooltip ("Add Position Marker: a marker at the playhead (Insert)");
        addCycle.setTooltip ("Add Cycle Marker: one over the locators' range (Shift-click: and name it)");
        lock.setTooltip ("Lock: the markers can't be changed (you can still go to them)");
        goTo.setTooltip ("Locate a marker, set the locators to a cycle marker, or zoom to one");
        addPosition.onClick = [this] { owner.studio.addPositionMarkerAtCursor(); };
        addCycle.onClick = [this]
        {
            const auto index = owner.studio.addCycleMarkerAtLocators();
            if (index >= 0 && juce::ModifierKeys::currentModifiers.isShiftDown())
                rename (index);
        };
        lock.onClick = [this] { owner.studio.setMarkerTrackLocked (! owner.studio.getMarkerTrack().locked); };
        goTo.onClick = [this] { showGoToMenu(); };
        setTooltip ("The marker track: right-click for its menu; double-click its name to rename it");
        refresh();
    }

    void refresh()
    {
        const auto& track = owner.studio.getMarkerTrack();
        lock.setToggleState (track.locked, juce::dontSendNotification);
        addPosition.setEnabled (! track.locked);
        addCycle.setEnabled (! track.locked);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff140f14));
        g.fillRect (bounds);
        g.setColour (juce::Colour (0xff050405));
        g.fillRect (bounds.withY (bounds.getBottom() - 1.0f).withHeight (1.0f));
        g.setColour (pr7s::FamilyColours::orange);
        g.fillRect (bounds.withWidth (6.0f).reduced (0.0f, 1.0f));
        g.setColour (ParagonColours::text);
        g.setFont (ParagonLookAndFeel::titleFont (13.0f));
        g.drawFittedText (owner.studio.getMarkerTrack().name.toUpperCase(), nameArea.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    }

    void resized() override
    {
        auto area = getLocalBounds().withTrimmedLeft (16).reduced (0, 4);
        area.removeFromRight (6);
        const auto keyHeight = juce::jmin (24, area.getHeight());
        const auto place = [&] (juce::Button& key, int width)
        {
            key.setBounds (area.removeFromRight (width).withSizeKeepingCentre (width, keyHeight));
            area.removeFromRight (4);
        };
        place (goTo, 62);
        place (lock, 42);
        place (addCycle, 46);
        place (addPosition, 46);
        nameArea = area.toFloat();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && owner.onMarkerMenu)
            owner.onMarkerMenu();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (nameArea.contains (e.position))
            renameTrack();
    }

private:
    void showGoToMenu()
    {
        const auto& markers = owner.studio.getMarkerTrack().markers;
        juce::PopupMenu menu, cycles, zooms;
        menu.addSectionHeader ("LOCATE");
        const auto label = [] (const pr7s::Marker& m)
        {
            return pr7s::markerIdText (m) + (m.name.isNotEmpty() ? "  " + m.name : juce::String());
        };
        for (int i = 0; i < (int) markers.size(); ++i)
        {
            menu.addItem (1 + i, label (markers[(size_t) i]));
            if (markers[(size_t) i].cycle)
            {
                cycles.addItem (10001 + i, label (markers[(size_t) i]));
                zooms.addItem (20001 + i, label (markers[(size_t) i]));
            }
        }
        if (markers.empty())
            menu.addItem (-1, "(no markers yet: + POS or + CYC)", false);
        menu.addSeparator();
        menu.addSubMenu ("Cycle (the locators to a cycle marker)", cycles, cycles.getNumItems() > 0);
        menu.addSubMenu ("Zoom (to a cycle marker)", zooms, zooms.getNumItems() > 0);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&goTo),
                            [safe = juce::Component::SafePointer<MarkerRow> (this)] (int chosen)
        {
            if (safe == nullptr || chosen <= 0)
                return;
            auto& studio = safe->owner.studio;
            if (chosen > 20000)
            {
                if (safe->owner.onMarkerZoom)
                    safe->owner.onMarkerZoom (chosen - 20001);
            }
            else if (chosen > 10000)
                studio.cycleToMarker (chosen - 10001);
            else
                studio.locateMarker (chosen - 1);
        });
    }

    void rename (int index)
    {
        const auto& markers = owner.studio.getMarkerTrack().markers;
        if (! juce::isPositiveAndBelow (index, (int) markers.size()))
            return;
        const auto marker = markers[(size_t) index];
        auto* box = new FamilyAlertWindow ("Cycle Marker " + pr7s::markerIdText (marker), "Its description.", juce::MessageBoxIconType::NoIcon);
        box->addTextEditor ("name", marker.name, "DESCRIPTION");
        box->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        box->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        box->enterModalState (true, juce::ModalCallbackFunction::create ([box, marker, safe = juce::Component::SafePointer<MarkerRow> (this)] (int result)
        {
            if (safe == nullptr || result != 1)
                return;
            auto& studio = safe->owner.studio;
            auto all = studio.getMarkerTrack().markers;
            if (const auto i = studio.getMarkerTrack().indexOf (marker.id, marker.cycle); i >= 0)
            {
                all[(size_t) i].name = box->getTextEditorContents ("name").trim();
                studio.setMarkers (std::move (all));
            }
        }), true);
    }

    void renameTrack()
    {
        auto* box = new FamilyAlertWindow ("Marker Track", "Its name.", juce::MessageBoxIconType::NoIcon);
        box->addTextEditor ("name", owner.studio.getMarkerTrack().name, "NAME");
        box->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        box->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        box->enterModalState (true, juce::ModalCallbackFunction::create ([box, safe = juce::Component::SafePointer<MarkerRow> (this)] (int result)
        {
            if (safe != nullptr && result == 1)
                safe->owner.studio.renameMarkerTrack (box->getTextEditorContents ("name"));
        }), true);
    }

    TrackList& owner;
    juce::TextButton addPosition { "+ POS" }, addCycle { "+ CYC" }, lock { "LOCK" }, goTo { "GO TO" };
    juce::Rectangle<float> nameArea;
};

//==============================================================================
// The chord track's row (0.16.0): its name, the key at the playhead, TO PADS
// (the manual's Assign from Chord Track), TO MIDI (its chords as a part on the
// selected track), AUTO SCALES (the scales follow the chords).
class TrackList::ChordRow final : public juce::Component,
                                  public juce::SettableTooltipClient,
                                  private juce::Timer
{
public:
    explicit ChordRow (TrackList& ownerList) : owner (ownerList)
    {
        for (auto* key : { &toPads, &toMidi, &autoScales, &lines, &record })
        {
            key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
            key->setWantsKeyboardFocus (false);
            addAndMakeVisible (*key);
        }
        autoScales.setClickingTogglesState (false);
        toPads.setTooltip ("Assign from Chord Track: its chords, each once, onto the chord pads in order (they replace the pads' chords)");
        toMidi.setTooltip ("Chords to MIDI: all its chords as a MIDI part on the selected instrument track (or a new one), voiced to move least");
        autoScales.setTooltip ("The scales (the strip under the chords) follow the chords; off once one is set by hand (right-click the strip)");
        toPads.onClick = [this]
        {
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Assign from Chord Track",
                                                "The chord track's chords onto the pads, each once, in order. The pads' chords are replaced (locked ones kept).",
                                                "Assign", "Cancel", nullptr,
                                                juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<ChordRow> (this)] (int result)
                                                {
                                                    if (safe != nullptr && result == 1)
                                                        safe->owner.studio.assignPadsFromChordTrack();
                                                }));
        };
        toMidi.onClick = [this] { owner.studio.chordsToMidi (owner.studio.getSelectedTrack()); };
        autoScales.onClick = [this] { owner.studio.setAutoScales (! owner.studio.getChordTrack().autoScales); };
        lines.setTooltip ("Piano, guitar and bass ideas for the chords (those in the cycle when it's on, else all): hear them, put one on a track");
        lines.onClick = [this] { if (owner.onLineIdeas) owner.onLineIdeas ({}); };
        // (0.44.0) Record Enable: while recording, the chords played (pads or keyboard) land here.
        record.setTooltip ("Record Enable: while you record, the chords you play (the pads, or three or more keys together) become chord events here");
        record.onClick = [this] { owner.studio.setChordTrackRecord (! owner.studio.isChordTrackRecording()); refresh(); };
        setTooltip ("The chord track: double-click its lane to add a chord, right-click for its menu");
        refresh();
        startTimerHz (10);
    }

    void refresh()
    {
        autoScales.setToggleState (owner.studio.getChordTrack().autoScales, juce::dontSendNotification);
        record.setToggleState (owner.studio.isChordTrackRecording(), juce::dontSendNotification);
        toPads.setEnabled (! owner.studio.getChordTrack().events.empty());
        toMidi.setEnabled (! owner.studio.getChordTrack().events.empty());
        lines.setEnabled (! owner.studio.getChordTrack().events.empty());
        timerCallback();   // (the chord and key at the playhead, at once)
        repaint();
    }

    void timerCallback() override
    {
        // The chord and key at the playhead, redrawn only when they change.
        const auto beat = owner.studio.getEngine().getPosition();
        const auto& track = owner.studio.getChordTrack();
        const auto index = track.indexAt (beat);
        const auto key = owner.studio.chordTrackKeyAt (beat);
        const auto text = (index >= 0 ? track.events[(size_t) index].chord.name (key) + "   " : juce::String()) + ChordUi::spokenKey (key);
        if (text != shown)
        {
            shown = text;
            repaint (nowArea.toNearestInt().expanded (2));
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff140f14));
        g.fillRect (bounds);
        g.setColour (juce::Colour (0xff050405));
        g.fillRect (bounds.withY (bounds.getBottom() - 1.0f).withHeight (1.0f));
        g.setColour (pr7s::FamilyColours::pink);
        g.fillRect (bounds.withWidth (6.0f).reduced (0.0f, 1.0f));
        g.setColour (ParagonColours::text);
        g.setFont (ParagonLookAndFeel::titleFont (13.0f));
        g.drawText ("CHORDS", nameArea, juce::Justification::centredLeft, false);
        g.setColour (ParagonColours::textDim);
        g.setFont (ChordUi::chordFont (12.0f));
        g.drawText (shown, nowArea, juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        auto area = getLocalBounds().withTrimmedLeft (24).reduced (0, 4);
        auto top = area.removeFromTop (juce::jmin (24, area.getHeight() / 2));
        nameArea = top.removeFromLeft (70).toFloat();
        top.removeFromRight (6);
        autoScales.setBounds (top.removeFromRight (78));
        top.removeFromRight (3);
        toMidi.setBounds (top.removeFromRight (60));
        top.removeFromRight (3);
        toPads.setBounds (top.removeFromRight (60));
        auto second = area.removeFromTop (juce::jmin (22, area.getHeight()));
        second.removeFromRight (6);
        lines.setBounds (second.removeFromRight (110).reduced (0, 1));
        second.removeFromRight (3);
        record.setBounds (second.removeFromRight (44).reduced (0, 1));
        nowArea = second.toFloat();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu())
            return;
        juce::PopupMenu menu;
        menu.addItem (1, "Chords to MIDI (onto the selected track)", ! owner.studio.getChordTrack().events.empty());
        menu.addItem (2, "Assign the Chords to the Pads", ! owner.studio.getChordTrack().events.empty());
        menu.addItem (3, "Scales Follow the Chords", true, owner.studio.getChordTrack().autoScales);
        menu.addItem (5, "Piano, Guitar and Bass Ideas...", ! owner.studio.getChordTrack().events.empty());
        menu.addSeparator();
        menu.addItem (4, "Remove the Chord Track (its chords kept)");
        menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<ChordRow> (this)] (int chosen)
        {
            if (safe == nullptr)
                return;
            auto& studio = safe->owner.studio;
            if (chosen == 1) studio.chordsToMidi (studio.getSelectedTrack());
            if (chosen == 2) studio.assignPadsFromChordTrack();
            if (chosen == 3) studio.setAutoScales (! studio.getChordTrack().autoScales);
            if (chosen == 4) studio.removeChordTrack();
            if (chosen == 5 && safe->owner.onLineIdeas) safe->owner.onLineIdeas ({});
        });
    }

    TrackList& owner;
    juce::TextButton toPads { "TO PADS" }, toMidi { "TO MIDI" }, autoScales { "AUTO SCALES" }, lines { "IDEAS" }, record { "REC" };
    juce::Rectangle<float> nameArea, nowArea;
    juce::String shown;
};

//==============================================================================
TrackList::TrackList (pr7s::Studio& studioToUse, Timeline& timelineToUse)
    : studio (studioToUse), timeline (timelineToUse)
{
    addTrack.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    addTrack.setTooltip ("Add a track, with PR7-K or one of your VST3 instruments");
    addTrack.onClick = [this] { showAddTrackMenu(); };   // (0.9.11: not shown; the right-click menus have Add Track)
    addTrack.setWantsKeyboardFocus (false);
    addChildComponent (addTrack);   // (0.9.11: hidden, the owner asked; right-click for Add Track)
    // The manual's Deactivate All Mute States / Solo States (0.9.9): lit while
    // any track is muted / soloed (hidden ones too); click to turn them all off.
    for (auto* key : { &muteAll, &soloAll })
    {
        key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key->setWantsKeyboardFocus (false);
        addChildComponent (*key);   // (0.73.0: in the header's row now, the owner asked; kept here for the state)
    }
    muteAll.setTooltip ("Deactivate All Mute States: lit while any track is muted; click to unmute them all");
    soloAll.setTooltip ("Deactivate All Solo States: lit while any track is soloed (a soloed track silences the others); click to turn every solo off");
    muteAll.onClick = [this] { studio.deactivateAllMute(); };
    soloAll.onClick = [this] { studio.deactivateAllSolo(); };
    // The colour menu (0.17.3): the selected tracks.
    colourKey.setWantsKeyboardFocus (false);
    colourKey.onClick = [this] { TrackColourMenu::show (colourKey, studio, studio.getSelectedTracks()); };
    addAndMakeVisible (colourKey);
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
    if (studio.getChangeVersion() == seenChange)
        return;   // (0.34.3: only the mix moved; the rows follow it on their timer)
    seenChange = studio.getChangeVersion();
    muteAll.setToggleState (studio.anyMute(), juce::dontSendNotification);
    soloAll.setToggleState (studio.anySolo(), juce::dontSendNotification);
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
        if (auto text = lane->valueText(); text != lane->shownValue)   // (0.83.1: only when the value shown changes)
        {
            lane->shownValue = std::move (text);
            lane->repaint (lane->valueArea.toNearestInt());
        }
    if (tempoRow != nullptr)
        tempoRow->followTempo();
    if (signatureRow != nullptr)
        signatureRow->followSignature();
}

void TrackList::rebuild()
{
    // The rows follow the project's tracks (kept where they are, so a name being edited isn't lost).
    // (A row for each track shown: hidden ones are in the left zone's VISIBILITY.)
    const auto shown = studio.getShownTracks();
    auto same = rows.size() == shown.size();
    for (size_t i = 0; same && i < shown.size(); ++i)
        same = rows[i]->trackId == shown[i];
    if (! same)
    {
        rows.clear();
        for (const auto id : shown)
        {
            rows.push_back (std::make_unique<Row> (*this, id));
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
        takeRows.clear();
        for (const auto& row : layout)
            if (row.kind == Timeline::Row::Kind::lane)
            {
                laneRows.push_back (std::make_unique<LaneRow> (*this, row.busId, row.laneIndex));
                addAndMakeVisible (*laneRows.back());
            }
            else if (row.kind == Timeline::Row::Kind::take)
            {
                takeRows.push_back (std::make_unique<TakeRow> (*this, row.busId, row.laneIndex));
                addAndMakeVisible (*takeRows.back());
            }
        lastLayout = layout;
    }
    for (auto& lane : laneRows)
        lane->refresh();
    for (auto& take : takeRows)
        take->refresh();
    if (masterRow == nullptr)
    {
        masterRow = std::make_unique<MasterRow> (*this);
        addAndMakeVisible (*masterRow);
    }
    masterRow->refresh();
    if (studio.getProject().tempoTrackShown && tempoRow == nullptr)
    {
        tempoRow = std::make_unique<TempoRow> (*this);
        addAndMakeVisible (*tempoRow);
    }
    else if (! studio.getProject().tempoTrackShown)
        tempoRow = nullptr;
    if (tempoRow != nullptr)
        tempoRow->refresh();
    if (studio.getProject().signatureTrackShown && signatureRow == nullptr)
    {
        signatureRow = std::make_unique<SignatureRow> (*this);
        addAndMakeVisible (*signatureRow);
    }
    else if (! studio.getProject().signatureTrackShown)
        signatureRow = nullptr;
    if (signatureRow != nullptr)
        signatureRow->refresh();
    if (studio.getSongSections().shown && sectionsRow == nullptr)
    {
        sectionsRow = std::make_unique<SectionsRow> (*this);
        addAndMakeVisible (*sectionsRow);
    }
    else if (! studio.getSongSections().shown)
        sectionsRow = nullptr;
    if (sectionsRow != nullptr)
        sectionsRow->refresh();
    if (studio.getMarkerTrack().shown && markerRow == nullptr)
    {
        markerRow = std::make_unique<MarkerRow> (*this);
        addAndMakeVisible (*markerRow);
    }
    else if (! studio.getMarkerTrack().shown)
        markerRow = nullptr;
    if (markerRow != nullptr)
        markerRow->refresh();
    if (studio.getChordTrack().shown && chordRow == nullptr)
    {
        chordRow = std::make_unique<ChordRow> (*this);
        addAndMakeVisible (*chordRow);
    }
    else if (! studio.getChordTrack().shown)
        chordRow = nullptr;
    if (chordRow != nullptr)
        chordRow->refresh();
    resized();
    repaint();
}

void TrackList::resized()
{
    placeRows();
    // + ADD TRACK at the top, in the header (level with the ruler).
    auto header = getLocalBounds().removeFromTop (Timeline::rulerHeight).reduced (8, 6);
    addTrack.setBounds ({});
    colourKey.setBounds (header.removeFromRight (34));
}

bool TrackList::moveTrackToSlot (int trackId, int slot)
{
    return studio.moveTrackToSlot (trackId, slot);
}

int TrackList::getTrackNumber (int trackId) const
{
    for (const auto& r : rows)
        if (r->trackId == trackId)
            return r->slot;
    return 0;
}

juce::Label* TrackList::getNumberLabel (int trackId)
{
    for (auto& r : rows)
        if (r->trackId == trackId)
            return &r->number;
    return nullptr;
}

void TrackList::placeRows()
{
    // Each where the layout has it (so they line up with the arrangement's rows).
    auto laneRow = laneRows.begin();
    auto takeRow = takeRows.begin();
    int slot = 0;   // (0.73.0) the tracks' numbers, 1 at the top, by place
    for (const auto& row : timeline.layout (studio))
    {
        const juce::Rectangle<int> bounds (0, Timeline::rulerHeight + row.top - timeline.scrollY, getWidth(), row.height);
        if (row.kind == Timeline::Row::Kind::track)
        {
            ++slot;
            for (auto& r : rows)
                if (r->trackId == row.busId)
                {
                    r->setSlot (slot);
                    r->setBounds (bounds);
                }
        }
        else if (row.kind == Timeline::Row::Kind::lane)
        {
            if (laneRow != laneRows.end())
                (*laneRow++)->setBounds (bounds);
        }
        else if (row.kind == Timeline::Row::Kind::take)
        {
            if (takeRow != takeRows.end())
                (*takeRow++)->setBounds (bounds);
        }
        else if (row.kind == Timeline::Row::Kind::tempo)
        {
            if (tempoRow != nullptr)
                tempoRow->setBounds (bounds);
        }
        else if (row.kind == Timeline::Row::Kind::signature)
        {
            if (signatureRow != nullptr)
                signatureRow->setBounds (bounds);
        }
        else if (row.kind == Timeline::Row::Kind::markers)
        {
            if (markerRow != nullptr)
                markerRow->setBounds (bounds);
        }
        else if (row.kind == Timeline::Row::Kind::sections)
        {
            if (sectionsRow != nullptr)
                sectionsRow->setBounds (bounds);
        }
        else if (row.kind == Timeline::Row::Kind::chords)
        {
            if (chordRow != nullptr)
                chordRow->setBounds (bounds);
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
}

void TrackList::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const auto room = juce::jmax (0, timeline.contentHeight (studio) - (getHeight() - Timeline::rulerHeight - addButtonHeight));
    timeline.scrollY = juce::jlimit (0, room, timeline.scrollY - juce::roundToInt (wheel.deltaY * 300.0f));
    if (timeline.onScrollOrZoom)
        timeline.onScrollOrZoom();
}

juce::PopupMenu TrackList::makeAddTrackMenu (int offset)
{
    // (0.9.11: no + ADD TRACK key; this menu is at the foot of the track
    // list's right-click menu, and a right-click under the tracks.)
    juce::PopupMenu menu;
    menu.addSectionHeader ("ADD AN AUDIO TRACK");
    menu.addItem (offset + 3, "Audio track (mono: a guitar, a voice)");
    menu.addItem (offset + 4, "Audio track (stereo: a pair of inputs)");
    menu.addSectionHeader ("ADD AN INSTRUMENT TRACK");
    menu.addItem (offset + 1, studio.getPluginHost().isStudioSynthFound() ? juce::String ("PR7-K (the studio's synth)")
                                                                  : juce::String ("PR7-K not found: the old Paragon Synth"));
    // (0.107.0, the Backline) The studio's drums.
    if (studio.getPluginHost().isStudioDrumsFound())
        menu.addItem (offset + 15, "PR7-D (the studio's drums)");
    const auto instruments = studio.getPluginHost().getInstruments();
    if (! instruments.isEmpty())
    {
        menu.addSeparator();
        for (int i = 0; i < instruments.size(); ++i)
            menu.addItem (offset + 100 + i, instruments[i].manufacturerName + ": " + instruments[i].name);
    }
    // FX and group channels (as the manual's Add Track dialog has them).
    const auto effects = studio.getPluginHost().getEffects();
    juce::PopupMenu withEffect;
    for (int i = 0; i < effects.size(); ++i)
        withEffect.addItem (offset + 1000 + i, effects[i].manufacturerName + ": " + effects[i].name);
    menu.addSectionHeader ("ADD AN FX CHANNEL OR A GROUP");
    menu.addItem (offset + 5, "FX channel (empty: add its effects in the mixer)");
    if (! effects.isEmpty())
        menu.addSubMenu ("FX channel with", withEffect);
    menu.addItem (offset + 6, "Group channel (a submix: route tracks to it)");
    menu.addSectionHeader ("ADD THE TEMPO TRACK");
    menu.addItem (offset + 8, "Tempo track (tempo changes: steps and ramps)", ! studio.getProject().tempoTrackShown);
    menu.addItem (offset + 10, "Signature track (time signature changes)", ! studio.getProject().signatureTrackShown);
    menu.addItem (offset + 11, "Marker track (position and cycle markers)", ! studio.getMarkerTrack().shown);
    menu.addItem (offset + 12, "Folder track (holds tracks: fold them away, mute or solo them together)");
    menu.addItem (offset + 13, "Folder track with group channel (its tracks summed)");
    menu.addItem (offset + 9, "Chord track (the song's chords and scales)", ! studio.getChordTrack().shown);
    menu.addItem (offset + 14, "Sections row (the song's parts as blocks: verse, chorus...)", ! studio.getSongSections().shown);
    menu.addSeparator();
    menu.addItem (offset + 7, "Track from a preset...");
    menu.addSeparator();
    menu.addItem (offset + 2, instruments.isEmpty() ? "Find my VST3 instruments..." : "Find more VST3 instruments...");

    return menu;
}

bool TrackList::addTrackChosen (int chosen)
{
    const auto instruments = studio.getPluginHost().getInstruments();
    const auto effects = studio.getPluginHost().getEffects();
    if (chosen <= 0)
        return false;
        if (chosen == 8)
        {
            studio.addTempoTrack();
        }
        else if (chosen == 9)
        {
            studio.addChordTrack();
        }
        else if (chosen == 10)
        {
            studio.addSignatureTrack();
        }
        else if (chosen == 11)
        {
            studio.addMarkerTrack();
        }
        else if (chosen == 14)
        {
            studio.addSongSectionTrack();
        }
        else if (chosen == 12 || chosen == 13)
        {
            studio.addFolderTrack ({}, chosen == 13);
        }
        else if (chosen == 7)
        {
            TrackActions::addTrackFromPreset (studio, chooser);
        }
        else if (chosen == 5)
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
        else if (chosen == 15)
        {
            studio.addDrumsTrack();
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
    return true;
}

void TrackList::showAddTrackMenu()
{
    makeAddTrackMenu().showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<TrackList> (this)] (int chosen)
    {
        if (safe != nullptr)
            safe->addTrackChosen (chosen);
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

juce::PopupMenu TrackList::alphabetical (const juce::PopupMenu& menu)
{
    // Its items (submenus with them) sorted by name, A to Z, without the
    // separators and section titles that grouped them before.
    std::vector<juce::PopupMenu::Item> items;
    for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
    {
        const auto& item = it.getItem();
        if (item.isSeparator || item.isSectionHeader || item.text.isEmpty())
            continue;
        items.push_back (item);
    }
    std::stable_sort (items.begin(), items.end(), [] (const auto& a, const auto& b) { return a.text.compareIgnoreCase (b.text) < 0; });
    juce::PopupMenu sorted;
    for (auto& item : items)
        sorted.addItem (std::move (item));
    return sorted;
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
        if (studio.getProject().hasOutput (*track))
        {
            // (0.59.0, 0.60.0) A group's or FX channel's output: the master, or any group or FX channel, never round in a loop.
            juce::PopupMenu output;
            output.addItem (30, "Master", true, track->output == 0);
            for (const auto& other : studio.getProject().tracks)
                if ((other.isGroup() || other.isFx()) && studio.getProject().canRouteTo (trackId, other.id))
                    output.addItem (30000 + other.id, other.name + (other.isFx() ? "  (FX)" : ""), true, track->output == other.id);
            menu.addSubMenu ("Output", output);
            menu.addSeparator();
        }
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
            if ((other.isGroup() || other.isFx()) && studio.getProject().canRouteTo (trackId, other.id))
                output.addItem (30000 + other.id, other.name + (other.isFx() ? "  (FX)" : ""), true, track->output == other.id);
        menu.addSubMenu ("Output", output);
        menu.addSeparator();
    }
    if (track->isAudio())
    {
        // The inputs switched on in SETTINGS (two, with none to ask).
        auto inputs = 2;
        if (auto* device = studio.getDeviceManager().getCurrentAudioDevice())
            inputs = juce::jmax (1, device->getActiveInputChannels().countNumberOfSetBits());
        juce::PopupMenu recordFrom;   // (a submenu since 0.9.12: the menu is in alphabetical order)
        for (int i = 0; i < inputs; ++i)
            recordFrom.addItem (200 + i, "IN " + juce::String (i + 1) + " (mono)", true, ! track->stereoInput && track->input == i);
        for (int i = 0; i + 1 < inputs; i += 2)
            recordFrom.addItem (300 + i, "IN " + juce::String (i + 1) + "+" + juce::String (i + 2) + " (stereo)", true, track->stereoInput && track->input == i);
        menu.addSubMenu ("Record From", recordFrom);
        menu.addSeparator();
        menu.addItem (2, "Delete track");
    }
    else if (! track->isChannel())
    {
        menu.addItem (1, "Open " + track->instrument.getName() + "'s window", studio.hasEditor (instance));
        change.addItem (10, studio.getPluginHost().isStudioSynthFound() ? juce::String ("PR7-K (the studio's synth)")
                                                                         : juce::String ("PR7-K not found: the old Paragon Synth"));
        for (int i = 0; i < instruments.size(); ++i)
            change.addItem (instrumentItems + i, instruments[i].manufacturerName + ": " + instruments[i].name);
        menu.addSubMenu (track->frozen ? "Change instrument (unfreeze first)" : "Change instrument", change, ! track->frozen && ! track->disabled);
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
    if (! track->isChannel())
    {
        menu.addSeparator();
        menu.addItem (57, several ? "Render the selected tracks in Place..." : "Render in Place...", ! track->disabled && ! track->frozen);
        menu.addItem (58, "Render in Place (as last time)", ! track->disabled && ! track->frozen);
        TrackActions::addFreezeItems (menu, studio, trackId);
    }
    menu.addSeparator();
    TrackActions::addPresetItems (menu, studio, trackId);
    // Disable (as the manual has it: silent, its plugins unloaded) and hide.
    menu.addSeparator();
    if (! track->isChannel())
        menu.addItem (59, track->disabled ? (several ? "Enable Selected Tracks" : "Enable Track")
                                          : (several ? "Disable Selected Tracks" : "Disable Track"));
    menu.addItem (60, several ? "Hide Selected Tracks" : "Hide Track");
    menu.addItem (64, several ? "Colour the Selected Tracks..." : "Colour...");   // (0.17.3)
    if (! track->isChannel())
    {
        // Takes (0.29.0).
        menu.addSeparator();
        menu.addItem (65, track->showLanes ? "Hide Lanes" : "Show Lanes");
        menu.addItem (66, "Clean Up Lanes", track->parts.size() > 1 && ! track->frozen);
        menu.addItem (67, "Create Tracks from Lanes", track->numLanes() > 1 && ! track->frozen);
        // Auto fades (0.52.0, the manual's Auto Fades dialog).
        if (track->isAudio())
            menu.addItem (88, "Auto Fades Settings...");
        menu.addItem (89, "Song's Auto Fades Settings...");
    }
    // Link groups (0.17.0, the manual's Linking Channels).
    menu.addSeparator();
    {
        const auto* link = studio.getLinkGroupOf (trackId);
        menu.addItem (61, "Link Selected Channels...", several);
        if (link != nullptr)
        {
            menu.addItem (62, "Link Group Settings (" + link->name + ")...");
            menu.addItem (63, "Unlink " + link->name);
        }
        juce::PopupMenu included;
        included.addItem (20000, "None", true, link == nullptr);
        for (const auto& g : studio.getProject().linkGroups)
            included.addItem (20000 + g.id, juce::String (g.id) + "  " + g.name, true, link != nullptr && link->id == g.id);
        menu.addSubMenu ("Included in Link Group: " + (link != nullptr ? link->name : juce::String ("None")), included);
    }
    // Folders (0.50.0, the manual's Track Folding).
    {
        juce::PopupMenu folding, into;
        folding.addItem (80, several ? "Move Selected Tracks to New Folder" : "Move Track to New Folder");
        folding.addItem (81, several ? "Move Selected Tracks to New Folder with Group Channel" : "Move Track to New Folder with Group Channel");
        for (const auto& other : studio.getProject().tracks)
            if (other.isFolder() && other.id != trackId && ! studio.getProject().isInFolder (other.id, trackId))
                into.addItem (500000 + other.id, other.name, true, track->folder == other.id);
        folding.addSubMenu ("Move into Folder", into, into.getNumItems() > 0);
        folding.addItem (82, "Move out of Its Folder", track->folder != 0);
        if (track->isFolder())
        {
            folding.addSeparator();
            folding.addItem (84, track->folderOpen ? "Collapse Folder" : "Expand Folder");
            folding.addItem (83, track->folderGroup ? "Disable Group Channel" : "Enable Group Channel");
            juce::PopupMenu data;
            data.addItem (85, "Always Show Data", true, track->folderData == pr7s::Track::FolderData::always);
            data.addItem (86, "Never Show Data", true, track->folderData == pr7s::Track::FolderData::never);
            data.addItem (87, "Hide Data When Expanded", true, track->folderData == pr7s::Track::FolderData::hideWhenExpanded);
            folding.addSubMenu ("Show Data on Folder Tracks", data);
        }
        menu.addSeparator();
        menu.addSubMenu ("Track Folding", folding);
    }
    // Add Track (0.9.11: the + ADD TRACK key's menu, here at the foot).
    menu.addSeparator();
    menu.addSubMenu ("Add Track", makeAddTrackMenu (addTrackOffset));
    menu = alphabetical (menu);   // (0.9.12, the owner asked)
    menu.showMenuAsync (juce::PopupMenu::Options(), [this, trackId, instruments] (int chosen)
    {
        if (chosen >= addTrackOffset)
        {
            addTrackChosen (chosen - addTrackOffset);
            return;
        }
        if (TrackActions::handle (chosen, studio, trackId, chooser))
            return;
        // Folders (0.50.0).
        if (chosen == 80 || chosen == 81)
        {
            auto ids = studio.getSelectedTracks();
            if (std::find (ids.begin(), ids.end(), trackId) == ids.end())
                ids = { trackId };
            studio.moveTracksToNewFolder (ids, chosen == 81);
            return;
        }
        if (chosen >= 500000 && chosen < instrumentItems * 10)
        {
            auto ids = studio.getSelectedTracks();
            if (std::find (ids.begin(), ids.end(), trackId) == ids.end())
                ids = { trackId };
            studio.setTracksFolder (ids, chosen - 500000);
            return;
        }
        if (chosen == 82)
        {
            if (const auto* t = studio.getProject().findTrack (trackId))
            {
                const auto* parent = studio.getProject().findTrack (t->folder);
                studio.setTracksFolder ({ trackId }, parent != nullptr ? parent->folder : 0);
            }
            return;
        }
        if (chosen >= 83 && chosen <= 87)
        {
            if (const auto* t = studio.getProject().findTrack (trackId))
            {
                if (chosen == 83) studio.setFolderGroup (trackId, ! t->folderGroup);
                if (chosen == 84) studio.setFolderOpen (trackId, ! t->folderOpen);
                if (chosen == 85) studio.setFolderData (trackId, pr7s::Track::FolderData::always);
                if (chosen == 86) studio.setFolderData (trackId, pr7s::Track::FolderData::never);
                if (chosen == 87) studio.setFolderData (trackId, pr7s::Track::FolderData::hideWhenExpanded);
            }
            return;
        }
        if (chosen == 88 || chosen == 89)
        {
            AutoFadesPage::show (*this, studio, chosen == 88 ? trackId : 0);
            return;
        }
        if (chosen == 65 || chosen == 66 || chosen == 67)
        {
            if (const auto* t = studio.getProject().findTrack (trackId))
            {
                if (chosen == 65)
                    studio.setShowLanes (trackId, ! t->showLanes);
                else if (chosen == 66)
                    studio.cleanUpLanes (trackId);
                else
                    studio.createTracksFromLanes (trackId);
            }
            return;
        }
        if (chosen == 64)
        {
            TrackColourMenu::show (*this, studio, studio.isTrackSelected (trackId) ? studio.getSelectedTracks() : std::vector<int> { trackId });
            return;
        }
        if (chosen == 61)
        {
            LinkGroupPage::show (*this, studio, 0, studio.getSelectedTracks());
            return;
        }
        if (chosen == 62 || chosen == 63)
        {
            if (const auto* link = studio.getLinkGroupOf (trackId))
                chosen == 62 ? LinkGroupPage::show (*this, studio, link->id, {}) : studio.removeLinkGroup (link->id);
            return;
        }
        if (chosen >= 20000 && chosen < 30000)
        {
            studio.setTrackLinkGroup (trackId, chosen - 20000);
            return;
        }
        if ((chosen == 57 || chosen == 58) && onRenderInPlace)
        {
            // The selected tracks, whole (from the song's start to their last part's end).
            std::vector<pr7s::Studio::RenderSource> sources;
            for (const auto id : studio.getSelectedTracks())
                if (const auto* t = studio.getProject().findTrack (id); t != nullptr && ! t->isChannel() && ! t->parts.empty())
                {
                    auto end = 0.0;
                    for (const auto& part : t->parts)
                        end = juce::jmax (end, part.end());
                    sources.push_back ({ id, 0.0, end, true });
                }
            if (! sources.empty())
                onRenderInPlace (sources, chosen == 57);
            return;
        }
        if (chosen == 59)
        {
            const auto* t = studio.getProject().findTrack (trackId);
            studio.setTracksDisabled (studio.getSelectedTracks(), t != nullptr && ! t->disabled);
        }
        else if (chosen == 60)
            studio.setTracksHidden (studio.getSelectedTracks(), true);
        else if (chosen == 50)
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
        else if (isOutputItem (chosen))   // (0.17.3: not the instruments, numbered above the outputs)
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
        else if (chosen == 10 || (chosen >= instrumentItems && chosen - instrumentItems < instruments.size()))
        {
            pr7s::Instrument instrument;
            if (chosen >= instrumentItems)
            {
                instrument.kind = pr7s::Instrument::Kind::plugin;
                instrument.plugin = instruments[chosen - instrumentItems];
            }
            juce::String error;
            if (! studio.changeInstrument (trackId, instrument, &error))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't change the instrument", error);
        }
        else if (chosen >= 300 && chosen < 400)
        {
            studio.setTrackInput (trackId, chosen - 300, true);
        }
        else if (chosen >= 200 && chosen < 300)
        {
            studio.setTrackInput (trackId, chosen - 200, false);
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
    // track mono or stereo as it is, a group or an FX channel), else one on the studio's synth (PR7-K).
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

void TrackList::mouseDown (const juce::MouseEvent& event)
{
    // A right-click under the tracks (or with no tracks at all): the Add Track menu (0.9.11).
    if (event.mods.isPopupMenu() && event.y > Timeline::rulerHeight)
        showAddTrackMenu();
}

void TrackList::mouseDoubleClick (const juce::MouseEvent& event)
{
    // The empty space under the tracks (not the header).
    if (event.y > Timeline::rulerHeight)
        addTrackLikeSelected();
}
