#include "engine/LaneWorlds.h"
#include "ui/ComposerPage.h"
#include "engine/SectionTypes.h"
#include "engine/ChordRoll.h"
#include "engine/GuitarTones.h"

#include "engine/MidiImport.h"
#include "ui/ChordUi.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    using pr7s::ComposerLayer;

    juce::Font interFont (float height, bool bold = false) { return juce::Font (ParagonLookAndFeel::textFont (height, bold ? juce::Font::bold : juce::Font::plain)); }   // (0.71.1: Bebas, the owner's rule; the old name kept)

    // As IDEAS' cards: bass and guitar orange, melody and lead pink, chords and drums red.
    juce::Colour colourOf (ComposerLayer l)
    {
        return l == ComposerLayer::bass || l == ComposerLayer::guitar || l == ComposerLayer::clean || l == ComposerLayer::acoustic ? pr7s::FamilyColours::orange
             : l == ComposerLayer::chords || l == ComposerLayer::drums ? pr7s::FamilyColours::red : pr7s::FamilyColours::pink;
    }

    void pedal (juce::Button& key, bool arrow = false)
    {
        key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        if (arrow)
            key.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
        key.setWantsKeyboardFocus (false);
    }

    const juce::StringArray& lengths()
    {
        static const juce::StringArray all { "4 BARS", "8 BARS", "16 BARS" };
        return all;
    }
}

//==============================================================================
ComposerPage::ComposerPage (pr7s::Studio& s) : studio (s)
{
    random.setSeedRandomly();
    // (0.206.0) The song's own state and riff.
    seenGeneration = studio.getSongGeneration();
    state = pr7s::ComposerState::fromString (studio.getComposerState());
    studio.setComposerRiffBlock (0);   // (0.226.0: no block chosen yet: the song's loose riff)
    useMyRiff = studio.getComposerRiff().has_value() && studio.isComposerRiffPlayed();
    fromRiff = studio.getComposerRiff().has_value() && studio.areComposerChordsFromRiff();
    fitRiff = studio.isComposerRiffFitted();
    seenRiff = studio.getComposerRiff();
    ++quietWrites;

    // Header keys.
    for (auto* key : { &hearKey, &useKey, &generateKey })
    {
        pedal (*key, key == &useKey);
        addAndMakeVisible (*key);
    }
    generateKey.setTooltip ("A new take of every layer that isn't locked, at once: the same styles and progression, new rhythms and notes");
    hearKey.setTooltip ("All the layers that are on, together and looped, through the song's own tracks (those tracks alone). The song isn't changed. Space stops it too.");
    useKey.setTooltip ("The layers that are on onto their tracks at the playhead's bar (one undo step); the chords onto the chord track too");
    generateKey.onClick = [this] { generate(); };
    hearKey.onClick = [this] { isHearing() ? stop() : (void) hear(); };
    useKey.onClick = [this] { showUseMenu(); };

    // The controls row.
    {
        int id = 1;
        for (const auto& k : pr7s::Key::all())
            keyBox.addItem (ChordUi::spokenKey (k).toUpperCase(), id++);
        keyBox.setTooltip ("The key: the chords are the library's in it (a progression written in the other mode plays on the relative key)");
        keyBox.onChange = [this]
        {
            const auto i = keyBox.getSelectedItemIndex();
            if (juce::isPositiveAndBelow (i, (int) pr7s::Key::all().size()))
                setKey (pr7s::Key::all()[(size_t) i]);
        };
        genreBox.addItem ("ANY GENRE", 1);
        id = 2;
        for (const auto& g : pr7s::progressionGenres())
            genreBox.addItem (g, id++);
        genreBox.setTooltip ("Which of the library's progressions GENERATE picks from");
        genreBox.onChange = [this] { setGenre (genreBox.getSelectedId() == 1 ? juce::String() : genreBox.getText()); };
        moodBox.addItem ("ANY MOOD", 1);
        id = 2;
        for (const auto& m : pr7s::progressionMoods())
            moodBox.addItem (m, id++);
        moodBox.setTooltip ("Which of the library's progressions GENERATE picks from");
        moodBox.onChange = [this] { setMood (moodBox.getSelectedId() == 1 ? juce::String() : moodBox.getText()); };
        id = 1;
        for (const auto& l : lengths())
            lengthBox.addItem (l, id++);
        lengthBox.setTooltip ("How long the idea is: the progression repeated to fill it");
        lengthBox.onChange = [this] { setBars (lengthBox.getText().getIntValue()); };
        id = 1;
        for (const auto st : pr7s::guitarStyles())
            styleBox.addItem (pr7s::guitarStyleName (st), id++);
        styleBox.setTooltip ("The guitar's, lead's and drums' style (the same as IDEAS')");
        styleBox.onChange = [this]
        {
            studio.setGuitarStyle (pr7s::guitarStyleNamed (styleBox.getText()));
            refresh();
        };
        for (auto* box : { &keyBox, &genreBox, &moodBox, &lengthBox, &styleBox })
            addAndMakeVisible (*box);
    }
    pedal (rollChordsKey);
    rollChordsKey.setTooltip ("New chords in the key, drawn chord to chord toward the likely ones (the section's feel weights them): "
                              "a SECTIONS block's chords onto the chord track (its lanes built again over them, their styles kept), "
                              "else the progression's chords swapped. Again: another roll");
    rollChordsKey.onClick = [this] { rollChords(); };
    addAndMakeVisible (rollChordsKey);
    pedal (progressionKey, true);
    progressionKey.setTooltip ("The progression: the library's that pass KEY, GENRE and MOOD (ROLL and GENERATE keep it: choose another here)");
    progressionKey.onClick = [this] { showProgressionMenu(); };
    addAndMakeVisible (progressionKey);
    for (auto* key : { &libraryKey, &chordTrackKey, &toChordTrackKey })
    {
        pedal (*key);
        addAndMakeVisible (*key);
    }
    libraryKey.setTooltip ("The chords from our progression library (KEY, GENRE, MOOD, PROGRESSION)");
    chordTrackKey.setTooltip ("The chord track's chords (those in the cycle when it's on, else all): the layers written under them");
    toChordTrackKey.setTooltip ("USE ALL also writes the library's chords onto the chord track at that bar");
    libraryKey.onClick = [this] { setFromRiff (false); setFromChordTrack (false); };
    chordTrackKey.onClick = [this] { setFromChordTrack (true); };
    // (0.75.0) Your own riff.
    pedal (myRiffKey);
    myRiffKey.setTooltip ("The chords read from your own riff (+ MY RIFF), in the song's key: those outside it stay as you played them, shown pink");
    myRiffKey.onClick = [this] { setFromRiff (true); };
    addAndMakeVisible (myRiffKey);
    pedal (pasteKey, true);
    pasteKey.setTooltip ("Paste in your own riff (a selected MIDI part, or a MIDI file; or drop the file here) and say which section it is: "
                         "its key is guessed, its chords read, the guitar plays it and the drums and bass are built around it");
    pasteKey.onClick = [this] { showPasteMenu(); };
    addAndMakeVisible (pasteKey);
    pedal (mixerKey);
    mixerKey.setTooltip ("The mixer in its own window over the Architect: faders, inserts, EQ while you hear the layers (they keep playing)");
    mixerKey.onClick = [this] { if (onOpenMixer) onOpenMixer(); };
    addAndMakeVisible (mixerKey);
    // (0.86.0) SYNC.
    pedal (typeKey, true);
    typeKey.setTooltip ("TYPE: what kind of section this is (a chorus DRIVING or DRONING...). It sets what every instrument does; "
                        "each lane's style menu then lists that type's approaches. STANDARD: the lanes' own styles");
    typeKey.onClick = [this] { showTypeMenu(); };
    addAndMakeVisible (typeKey);
    pedal (syncKey, true);
    syncKey.setTooltip ("SYNC: pick the lane that leads (the riff you like). Every other lane is written to its rhythm: the drums' kick and the bass "
                        "on its accents (DOUBLE THE SYNC TRACK in their style menus: every hit, the bass doubling the notes), chord stabs on its "
                        "accents, guitar chugs on its hits, the melody and lead leaning on its accents. It stays on: roll the leader and the rest follows");
    syncKey.onClick = [this] { showSyncMenu(); };
    addAndMakeVisible (syncKey);
    toChordTrackKey.onClick = [this] { setToChordTrack (! state.toChordTrack); };

    // (0.74.0) SECTION and BUILD.
    {
        int id = 1;
        for (const auto sec : pr7s::drumSections())
            sectionBox.addItem ("SECTION: " + pr7s::sectionWord (sec).toUpperCase(), id++);
        sectionBox.setTooltip ("The section you're writing: the drums play its beat (SOLO: the lead plays the whole solo). BUILD writes it after the last section built");
        sectionBox.onChange = [this]
        {
            const auto i = sectionBox.getSelectedItemIndex();
            if (juce::isPositiveAndBelow (i, (int) pr7s::drumSections().size()))
                setSection (pr7s::drumSections()[(size_t) i]);
        };
        addAndMakeVisible (sectionBox);
    }
    pedal (buildKey);
    buildKey.setTooltip ("The lanes that are on written as this section onto their tracks, after the last section built (the first at the playhead's bar), "
                         "the chords onto the chord track; one undo step. Then the next section is suggested and chosen: change it if you like");
    buildKey.onClick = [this] { buildSection(); };
    addAndMakeVisible (buildKey);

    // The lanes.
    for (const auto l : pr7s::composerLayers())
    {
        auto& lane = lanes[(size_t) l];
        for (auto* key : { &lane.on, &lane.lock, &lane.roll, &lane.bars, &lane.style, &lane.track, &lane.busy, &lane.low, &lane.octave })
        {
            pedal (*key, key == &lane.style || key == &lane.track || key == &lane.busy || key == &lane.low || key == &lane.octave);
            if ((key != &lane.low || pr7s::hasLowString (l)) && (key != &lane.octave || l != ComposerLayer::drums))
                addAndMakeVisible (*key);
        }
        if (l == ComposerLayer::acoustic)
        {
            pedal (lane.second, true);
            lane.second.setTooltip ("SECOND ACOUSTIC: what the second track plays when this lane is on two tracks (ARCHITECT TRACKS): a looser copy, "
                                    "or a part of its own (on one track both play there)");
            lane.second.onClick = [this] { showSecondMenu(); };
            addAndMakeVisible (lane.second);
        }
        lane.octave.setTooltip ("Moves this lane's notes up or down by whole octaves (its keyswitches stay where they are). Built sections keep it");
        lane.octave.onClick = [this, l] { showOctaveMenu (l); };
        lane.on.setTooltip ("Lit: heard and used. Off: left out (still made, shown dimmed)");
        lane.lock.setTooltip ("Lit: kept through GENERATE (and ROLL on the others). Locked with the chords, its notes stay exactly as they are");
        lane.roll.setTooltip ("A new take of this style: another rhythm and other notes, the same approach (choose the approach in the menu beside it)");
        lane.style.setTooltip ("Which of the layer's styles: the approach (ROLL keeps it)");
        lane.busy.setTooltip ("How busy its takes are: sparse, as the approach usually goes (50%), or busy. ROLL keeps it");
        lane.low.setTooltip ("Which string: any, the lowest as much as it can be, or the whole part on one string (with the INSTRUMENTS profile's keyswitch for it held). The menu also has the tuning");
        lane.track.setTooltip ("The track it plays on and is written to (automatic: the track named for it, else a new one when used)");
        lane.on.onClick = [this, l] { setOn (l, ! state.lane (l).on); };
        lane.lock.onClick = [this, l] { setLocked (l, ! state.lane (l).locked); };
        lane.roll.onClick = [this, l] { roll (l); };
        // (0.202.0) LOCK BARS.
        lane.bars.setTooltip ("Drag across bars in the lane's notes (Shift or Ctrl adds more), then LOCK BARS: those bars keep their notes exactly "
                              "while ROLL, GENERATE and every change rewrite the others. On the drums, ROLL then gives a new fill for the bars "
                              "not locked, and the options window lists the FILLS. Clicked with nothing chosen: every bar let go");
        lane.bars.onClick = [this, l]
        {
            if (barSelection[(size_t) l] != 0)
                lockSelectedBars (l);
            else if (state.lane (l).hasBarLocks())
                unlockAllBars (l);
            else
            {
                status = "Drag across the bars you like in " + pr7s::composerLayerName (l) + "'s notes first, then LOCK BARS";
                repaint();
            }
        };
        lane.style.onClick = [this, l] { showStyleMenu (l); };
        lane.track.onClick = [this, l] { showTrackMenu (l); };
        // (0.76.0) VOLUME: the fader of the track the lane is heard on (as the mixer's; hearing goes on).
        lane.volume.setRange (-60.0, 6.0, 0.5);
        lane.volume.setSkewFactorFromMidPoint (-12.0);
        lane.volume.setDoubleClickReturnValue (true, 0.0);
        lane.volume.setTextValueSuffix (" dB");
        lane.volume.setNumDecimalPlacesToDisplay (1);
        lane.volume.setTextBoxStyle (juce::Slider::TextBoxLeft, true, 70, 20);
        lane.volume.setTooltip ("This lane's Architect level: what HEAR plays it at (the song's faders stay as they are) and where a track "
                                "Architect makes for it, or first writes onto while it's empty, starts. The same as ARCHITECT TRACKS in the Hub. "
                                "Once the music is in the song, its mixer fader is its volume. Double-click: 0 dB");
        lane.volume.onDragStart = [this, l] { draggingVolume = (int) l; };
        lane.volume.onValueChange = [this, l]
        {
            setLaneVolume (l, (float) lanes[(size_t) l].volume.getValue(), draggingVolume != (int) l);
        };
        lane.volume.onDragEnd = [this, l]
        {
            draggingVolume = -1;
            setLaneVolume (l, (float) lanes[(size_t) l].volume.getValue(), true);   // (one undo step)
        };
        addAndMakeVisible (lane.volume);
        lane.busy.onClick = [this, l] { showBusyMenu (l); };
        lane.low.onClick = [this, l] { showLowMenu (l); };
    }

    setSize (1700, 940);
    refresh (false);
    --quietWrites;
    startTimerHz (30);
}

bool ComposerPage::startOverIfNewSong()
{
    if (studio.getSongGeneration() == seenGeneration)
        return false;
    startOverForSong();
    return true;
}

void ComposerPage::startOverForSong()
{
    seenGeneration = studio.getSongGeneration();
    ++quietWrites;
    // The options window and a trial: closed without putting another song's lane back.
    trial = {};
    if (browserWindow != nullptr)
        browserWindow->getBrowser().onClosed = nullptr;
    browserWindow.reset();
    stop();
    chooser.reset();
    // The new song's state and riff; everything worked out for the old one forgotten.
    state = pr7s::ComposerState::fromString (studio.getComposerState());
    studio.setComposerRiffBlock (0);
    useMyRiff = studio.getComposerRiff().has_value() && studio.isComposerRiffPlayed();
    fromRiff = studio.getComposerRiff().has_value() && studio.areComposerChordsFromRiff();
    fitRiff = studio.isComposerRiffFitted();
    seenRiff = studio.getComposerRiff();
    taken = {};
    chordOverrides.clear();
    lastSpans.clear();
    composedFrom = {};
    composed = {};
    ideas = {};
    spans.clear();
    chordNames.clear();
    fillsShown.clear();
    grooveOptionsKey = {};
    grooveOptionsKept.clear();
    grooveFilesKept.clear();
    grooveOptionsState = {};
    barSelection = {};
    selectingBars = false;
    trackChoice = {};
    recentRiffs.clear();
    nextShown.clear();
    targetBlock = 0;
    hearingStart = 0.0;
    shownHearBeat = -1.0;
    pressedLane = -1;
    draggedOut = false;
    draggingVolume = -1;
    status = {};
    refresh (false);
    aimAtSectionsRow();   // (its first block not built yet, as a fresh window)
    --quietWrites;
    if (onSongChanged)
        onSongChanged();
}

ComposerPage::~ComposerPage()
{
    if (browserWindow != nullptr)
    {
        browserWindow->getBrowser().onClosed = nullptr;   // (no trial put back into a page that's going)
        browserWindow.reset();
    }
    if (hearing)
        studio.stopComposerAudition();
}

//==============================================================================
juce::String ComposerPage::composeFingerprint() const
{
    // Everything `composeLayers` reads, as text: the state, the chords, and the context's every field.
    juce::String f;
    f << state.toString() << "|";
    for (const auto& s : spans)
        f << juce::String (s.start, 6) << "," << juce::String (s.end, 6) << "," << s.chord.toText() << "," << s.key.name() << ";";
    const auto& c = context;
    f << "|" << c.beatsPerBar << "|" << c.tuning.name;
    for (const auto n : c.tuning.strings) f << "," << n;
    f << "|" << c.bassTuning.name;
    for (const auto n : c.bassTuning.strings) f << "," << n;
    for (const auto* p : { &c.bassProfile, &c.guitarProfile, &c.leadProfile })
        f << "|" << (p->has_value() ? pr7s::profilesToText ({ **p }) : juce::String ("-"));
    f << "|" << c.velocities.toString() << "|" << (int) c.style << "|" << (int) c.solo << "|" << c.riff.toString()
      << "|" << c.drums.toString() << "|" << c.melody.toString() << "|" << c.humanize << "|" << (int) c.feel << "|" << juce::String (c.bpm, 6)
      << "|" << juce::String (c.lastBarBeats, 6) << "|" << (c.nextChord.has_value() ? c.nextChord->toText() : juce::String());
    f << "|F:" << c.favouriteFolders.joinIntoString (";");   // (0.178.0)
    f << "|G:" << (int) c.libraryGrooves.size();
    for (size_t i = 0; i < c.libraryGrooves.size() && i < 64; ++i)
        f << ";" << c.libraryGrooves[i].file.getFullPathName();
    if (c.drummer.has_value())
    {
        const auto file = pr7s::drummerFile (studio.getDrummerFolder(), c.drummer->name);
        f << "|D:" << c.drummer->name << "," << file.getLastModificationTime().toMilliseconds() << "," << file.getSize();
    }
    if (c.myRiff.has_value())
    {
        f << "|R:" << c.myRiff->notes.size();
        juce::uint64 h = 1469598103934665603ull;
        for (const auto& n : c.myRiff->notes)
            for (const auto v : { (juce::uint64) juce::roundToInt (n.beat * 960.0), (juce::uint64) juce::roundToInt (n.length * 960.0),
                                  (juce::uint64) n.pitch, (juce::uint64) juce::roundToInt (n.velocity * 1000.0f), (juce::uint64) n.keyswitch,
                                  (juce::uint64) n.effect, (juce::uint64) juce::roundToInt (n.amount * 1000.0f), (juce::uint64) n.technique })
                h = (h ^ v) * 1099511628211ull;
        f << "," << juce::String::toHexString ((juce::int64) h);
    }
    return f;
}

void ComposerPage::refresh (bool rehear)
{
    // The chords.
    followRiffInHand();   // (0.226.0: the chosen block's own riff)
    const auto at = hearing ? hearingStart : placeStart();
    context = studio.getComposerContext (at);
    const auto& riff = studio.getComposerRiff();
    if (fromRiff && ! riff.has_value())
        fromRiff = false;
    const auto target = getTargetPlace();
    followBlockChords();
    {   // (0.104.0) The block's transition (none outside the SECTIONS row).
        const auto& sections = studio.getSongSections();
        const auto index = sections.indexOf (targetBlock);
        state.transition = index >= 0 ? sections.blocks[(size_t) index].transition : juce::String();
        if (usesSectionsRow() && ! state.fromChordTrack && ! fromRiff)
            state.key = index >= 0 ? studio.getSongSectionKey (index) : studio.getSongKey();   // (0.114.0: the song's key)
    }
    if (target.has_value() && ! state.fromChordTrack && ! fromRiff)
        state.bars = target->bars;   // (0.90.0: the block's length; the progression repeated to fill it, cut at its end)
    spans = chordSpans();
    if (target.has_value())
    {
        const auto from = state.fromChordTrack ? target->start : 0.0;
        const auto to = from + (target->end - target->start);
        std::vector<pr7s::ChordSpan> inside;
        for (auto s : spans)
            if (s.end > from + 1.0e-6 && s.start < to - 1.0e-6)
            {
                s.start = juce::jmax (s.start, from);
                s.end = juce::jmin (s.end, to);
                inside.push_back (s);
            }
        spans = inside;
    }
    if (! state.fromChordTrack && ! fromRiff)
        if (const auto index = pr7s::composerProgressionIndex (state); index >= 0)
            state.progression = pr7s::progressionLibrary()[(size_t) index].name;
    for (auto it = chordOverrides.begin(); it != chordOverrides.end();)
    {
        if (juce::isPositiveAndBelow (it->first, (int) spans.size()))
        {
            spans[(size_t) it->first].chord = it->second;
            ++it;
        }
        else
            it = chordOverrides.erase (it);
    }
    chordNames.clear();
    const auto steps = state.fromChordTrack || fromRiff ? std::vector<pr7s::ProgressionStep> {} : pr7s::composerSteps (state);
    for (size_t i = 0; i < spans.size(); ++i)
        chordNames.add (i < steps.size() && chordOverrides.count ((int) i) == 0 ? steps[i].name (state.key) : spans[i].chord.name (spans[i].key));
    context.myRiff.reset();
    if (riff.has_value() && useMyRiff && ! spans.empty())
    {
        context.myRiff = pr7s::tileRiff (*riff, spans.back().end - spans.front().start, context.beatsPerBar);
        if (fitRiff && ! fromRiff)   // (0.226.0: its notes moved onto the chords the band plays)
            context.myRiff = pr7s::fitRiffToChords (*context.myRiff, *riff, spans, context.beatsPerBar);
    }
    // (0.74.0) Ideas taken from IDEAS stay in their lanes while the chords are the same.
    const auto sameChords = [] (const std::vector<pr7s::ChordSpan>& a, const std::vector<pr7s::ChordSpan>& b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::abs ((a[i].end - a[i].start) - (b[i].end - b[i].start)) > 1.0e-6 || ! (a[i].chord == b[i].chord))
                return false;
        return true;
    };
    const auto chordsChanged = ! sameChords (spans, lastSpans);
    {
        // (0.202.0) Bar locks past the section's last bar let go; a pitched lane's let go when the chords change under
        // them (its kept notes were written for the old ones: the drums' stay).
        const auto bars = barCount();
        if (bars > 0)
            pr7s::trimBarLocks (state, bars, context.beatsPerBar);
        for (auto& b : barSelection)
            b &= bars >= 64 ? ~(juce::uint64) 0 : (((juce::uint64) 1 << juce::jmax (0, bars)) - 1);
        if (chordsChanged && ! lastSpans.empty())
        {
            auto pitched = false;
            for (const auto l : pr7s::composerLayers())
                pitched = pitched || (l != ComposerLayer::drums && state.lane (l).hasBarLocks());
            if (pitched)
            {
                pr7s::dropPitchedBarLocks (state);
                status = "The chords changed: the locked bars of the pitched lanes were let go (the drums' kept)";
            }
        }
    }
    if (const auto fingerprint = composeFingerprint(); fingerprint != composedFrom)
    {
        composed = pr7s::composeLayers (state, spans, context);
        composedFrom = fingerprint;
    }
    ideas = composed;
    // (0.203.0) The fills' names, for the open bars' labels.
    fillsShown.clear();
    if (state.lane (ComposerLayer::drums).hasBarLocks() && state.lane (ComposerLayer::drums).fill.isNotEmpty())
        fillsShown = pr7s::composerFills (state, context, &spans);
    if (chordsChanged && std::any_of (taken.begin(), taken.end(), [] (const auto& t) { return t.has_value(); }))
    {
        for (auto& t : taken)
            t.reset();
        status = "The chords changed: the ideas taken from IDEAS went back to the lanes' own";
    }
    lastSpans = spans;
    for (const auto l : pr7s::composerLayers())
        if (taken[(size_t) l].has_value())
            ideas[(size_t) l] = *taken[(size_t) l];
    studio.setComposerState (state.toString(), quietWrites == 0);
    studio.setComposerRiffUse (useMyRiff, fromRiff, quietWrites == 0);   // (0.206.0: kept with the song's riff)
    studio.setComposerRiffFit (fitRiff, quietWrites == 0);

    // The keys.
    const auto& keys = pr7s::Key::all();
    keyBox.setSelectedItemIndex ((int) std::distance (keys.begin(), std::find (keys.begin(), keys.end(), state.key)), juce::dontSendNotification);
    genreBox.setText (state.genre.isEmpty() ? "ANY GENRE" : state.genre, juce::dontSendNotification);
    moodBox.setText (state.mood.isEmpty() ? "ANY MOOD" : state.mood, juce::dontSendNotification);
    lengthBox.setText (juce::String (state.bars) + " BARS", juce::dontSendNotification);
    styleBox.setText (pr7s::guitarStyleName (studio.getGuitarStyle()), juce::dontSendNotification);
    progressionKey.setButtonText (state.fromChordTrack ? juce::String ("PROGRESSION: THE CHORD TRACK'S")
                                  : fromRiff ? juce::String ("PROGRESSION: FROM MY RIFF")
                                  : state.progression.isEmpty() ? juce::String ("NO PROGRESSION FITS") : "PROGRESSION: " + state.progression.toUpperCase());
    progressionKey.setEnabled (! state.fromChordTrack && ! fromRiff);
    rollChordsKey.setEnabled (getTargetPlace().has_value() || ! state.fromChordTrack);   // (0.208.0)
    for (auto* box : { &genreBox, &moodBox })
        box->setEnabled (! state.fromChordTrack && ! fromRiff);
    lengthBox.setEnabled (! state.fromChordTrack && ! getTargetPlace().has_value());   // (0.90.0: a block's length is its own)
    // (0.75.0) The key locked for the song once a section is built.
    keyBox.setEnabled (! state.fromChordTrack && (usesSectionsRow() || ! isKeyLocked()));
    keyBox.setTooltip (isKeyLocked() ? "The key is locked for the whole song now that a section is built (right-click the SONG strip to start the song again)"
                                     : "The key: the chords are the library's in it (a progression written in the other mode plays on the relative key); "
                                       "with your riff, guessed from it");
    libraryKey.setToggleState (! state.fromChordTrack && ! fromRiff, juce::dontSendNotification);
    myRiffKey.setToggleState (fromRiff, juce::dontSendNotification);
    myRiffKey.setEnabled (riff.has_value());
    chordTrackKey.setToggleState (state.fromChordTrack, juce::dontSendNotification);
    toChordTrackKey.setToggleState (state.toChordTrack, juce::dontSendNotification);
    toChordTrackKey.setEnabled (! state.fromChordTrack);
    sectionBox.setSelectedItemIndex ((int) std::distance (pr7s::drumSections().begin(),
                                                          std::find (pr7s::drumSections().begin(), pr7s::drumSections().end(), getSection())),
                                     juce::dontSendNotification);
    if (const auto target = getTargetPlace())
        buildKey.setButtonText ("BUILD INTO " + pr7s::sectionLabel (studio.getSongSections().blocks, studio.getSongSections().indexOf (targetBlock)).toUpperCase());
    else
        buildKey.setButtonText ("BUILD " + pr7s::sectionWord (getSection()).toUpperCase());
    typeKey.setButtonText ("TYPE: " + (state.sectionType.isNotEmpty() ? state.sectionType : juce::String (pr7s::standardSectionType)));
    typeKey.setToggleState (state.sectionType.isNotEmpty(), juce::dontSendNotification);
    typeKey.setEnabled (! pr7s::sectionTypesFor (typeSectionName()).empty());
    hearKey.setButtonText (hearing ? "STOP" : "HEAR");
    hearKey.setToggleState (hearing, juce::dontSendNotification);
    {
        const auto sync = state.syncLayer();
        syncKey.setButtonText (sync.has_value() ? "SYNC: " + pr7s::composerLayerName (*sync) : juce::String ("SYNC: OFF"));
        syncKey.setToggleState (sync.has_value(), juce::dontSendNotification);
    }
    for (const auto l : pr7s::composerLayers())
    {
        auto& lane = lanes[(size_t) l];
        const auto& s = state.lane (l);
        lane.on.setToggleState (s.on, juce::dontSendNotification);
        lane.lock.setToggleState (s.locked, juce::dontSendNotification);
        const auto approach = s.approach >= 0 ? pr7s::approachName (state, l) : juce::String();
        lane.style.setButtonText (taken[(size_t) l].has_value() ? "FROM IDEAS: " + ideas[(size_t) l].style.toUpperCase()
                                  : approach.isNotEmpty() ? (s.world > 0 ? pr7s::laneWorldNames()[s.world] + ": " + approach
                                                             : state.sectionType.isNotEmpty() ? state.sectionType + ": " + approach
                                                             : approach)
                                  : ideas[(size_t) l].style.isEmpty() ? juce::String ("STYLE") : ideas[(size_t) l].style.toUpperCase());
        lane.track.setButtonText (trackText (l));
        lane.busy.setButtonText ("BUSY " + juce::String (juce::roundToInt (s.busy * 100.0f)) + "%");
        lane.octave.setButtonText ("OCTAVE " + (s.octave > 0 ? "+" + juce::String (s.octave) : juce::String (s.octave)));
        lane.octave.setToggleState (s.octave != 0, juce::dontSendNotification);
        lane.low.setToggleState (s.lowString || s.onlyString > 0, juce::dontSendNotification);
        lane.low.setButtonText (stringText (l));
        if (l == ComposerLayer::acoustic)
        {
            lane.second.setButtonText ("2ND: " + pr7s::secondAcousticParts()[juce::jlimit (0, pr7s::secondAcousticParts().size() - 1, s.second)]);
            lane.second.setToggleState (s.second > 0, juce::dontSendNotification);
        }
    }
    refreshBarKeys();
    seenChange = studio.getChangeVersion();
    seenContent = studio.getContentVersion();
    refreshVolumes();
    refreshSong();
    if (chordsChanged && onChordsChanged)
        onChordsChanged (placedSpans());

    if (rehear && hearing && ! hear())
        stop();
    repaint();
}

std::optional<pr7s::SectionPlace> ComposerPage::getTargetPlace() const
{
    if (targetBlock <= 0)
        return std::nullopt;
    return studio.getSongSectionPlace (studio.getSongSections().indexOf (targetBlock));
}

void ComposerPage::targetSection (int blockId)
{
    const auto index = studio.getSongSections().indexOf (blockId);
    if (index < 0)
        return;
    targetBlock = blockId;
    const auto& blocks = studio.getSongSections().blocks;
    const auto place = getTargetPlace();
    setSection (pr7s::drumSections()[(size_t) pr7s::composerSectionFor (blocks[(size_t) index].name)]);   // (refreshes)
    applyBlockType();
    if (place.has_value())
    {
        status = "Composing " + pr7s::sectionLabel (blocks, index) + " (bars " + juce::String (place->firstBar + 1) + " to "
               + juce::String (place->firstBar + place->bars) + "): HEAR plays it there, BUILD writes into it";
        if (const auto riff = studio.getSongSectionRiff (index); riff.has_value())   // (0.226.0)
            status += ". " + blockRiffText();
    }
    repaint();
}

juce::String ComposerPage::typeSectionName() const
{
    const auto& sections = studio.getSongSections();
    if (const auto index = sections.indexOf (targetBlock); index >= 0)
        return sections.blocks[(size_t) index].name;
    const auto word = pr7s::sectionWord (getSection());
    return word.substring (0, 1).toUpperCase() + word.substring (1).toLowerCase();
}

void ComposerPage::applyBlockType()
{
    const auto& sections = studio.getSongSections();
    const auto index = sections.indexOf (targetBlock);
    if (index < 0)
        return;
    const auto& block = sections.blocks[(size_t) index];
    if (block.type == state.sectionType)
        return;
    pr7s::applySectionType (state, block.name, block.type);
    taken[(size_t) ComposerLayer::guitar].reset();
    refresh();
}

void ComposerPage::setSectionType (const juce::String& type)
{
    const auto name = typeSectionName();
    if (! pr7s::isSectionTypeOf (name, type))
        return;
    const auto index = studio.getSongSections().indexOf (targetBlock);
    const auto shown = type.isEmpty() ? juce::String (pr7s::standardSectionType) : type.toUpperCase();
    if (index < 0)
    {
        pr7s::applySectionType (state, name, type);
        for (auto& t : taken)
            t.reset();
        status = "Type: " + shown;
        refresh();
        return;
    }
    const auto apply = [safe = juce::Component::SafePointer<ComposerPage> (this), index, type, shown]
    {
        if (safe == nullptr)
            return;
        const auto built = safe->studio.isSongSectionBuilt (index);
        if (! safe->studio.setSongSectionType (index, type))
            return;
        for (auto& t : safe->taken)
            t.reset();
        safe->applyBlockType();
        const auto label = pr7s::sectionLabel (safe->studio.getSongSections().blocks, index);
        safe->status = label + ": " + shown + (built ? " (built again; your own edits kept)" : "");
        safe->refresh();
    };
    if (studio.isSongSectionBuilt (index) && studio.getAskBeforeTypeChange())
    {
        const auto question = "Build " + pr7s::sectionLabel (studio.getSongSections().blocks, index) + " again as " + shown
                            + "? The lanes you haven't changed by hand are written again.";
        if (askToRebuild)
            askToRebuild (question, apply);
        else
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "CHANGE THE TYPE", question, "REBUILD", "CANCEL", this,
                                                juce::ModalCallbackFunction::create ([apply] (int ok) { if (ok == 1) apply(); }));
        return;
    }
    apply();
}

void ComposerPage::setApproach (ComposerLayer l, int approach)
{
    const auto names = pr7s::sectionApproaches (state, l);
    state.lane (l).approach = names.isEmpty() || approach < 0 ? -1 : juce::jlimit (0, names.size() - 1, approach);
    taken[(size_t) l].reset();
    if (l == ComposerLayer::drums)
        state.lane (l).fill = {};   // (0.202.0)
    if (l == ComposerLayer::guitar)
        useMyRiff = false;
    status = pr7s::composerLayerName (l) + ": " + (state.lane (l).approach >= 0 ? names[state.lane (l).approach] : juce::String ("AUTOMATIC"));
    refresh();
}

void ComposerPage::setOwnStyles (ComposerLayer l, bool own)
{
    state.lane (l).ownStyles = own;
    taken[(size_t) l].reset();
    status = pr7s::composerLayerName (l) + (own ? ": ITS OWN STYLES" : ": FOLLOWS THE SECTION TYPE");
    refresh();
}

void ComposerPage::setWorld (ComposerLayer l, int world)
{
    if (! hasWorlds (l))
        return;
    auto& lane = state.lane (l);
    world = juce::jlimit (0, pr7s::laneWorldNames().size() - 1, world);
    if (lane.world == world)
        return;
    lane.world = world;
    lane.approach = -1;      // (the other world's list: AUTOMATIC picks for the section)
    lane.ownStyles = false;
    lane.on = true;
    taken[(size_t) l].reset();
    status = pr7s::composerLayerName (l) + ": " + pr7s::laneWorldNames()[world] + " WORLD";
    refresh();
}

bool ComposerPage::typeWouldWrite (ComposerLayer l) const
{
    auto with = state;
    with.lane (l).ownStyles = false;
    return pr7s::sectionTypeWritesLane (l, with, context);
}

void ComposerPage::showTypeMenu()
{
    // (0.157.0) The short list; MORE TYPES... the browser.
    makeTypeMenu().showMenuAsync (juce::PopupMenu::Options().withTargetComponent (typeKey),
                                  [safe = juce::Component::SafePointer<ComposerPage> (this)] (int chosen)
                                  {
                                      if (safe != nullptr && chosen > 0)
                                          safe->typeMenuChosen (chosen);
                                  });
}

void ComposerPage::clearTargetSection()
{
    if (targetBlock == 0)
        return;
    targetBlock = 0;
    refresh();
}

double ComposerPage::placeStart() const
{
    if (const auto target = getTargetPlace())
        return target->start;   // (0.90.0: the block composed)
    if (state.fromChordTrack)
    {
        const auto chords = studio.getLineChords();
        return chords.empty() ? 0.0 : chords.front().start;
    }
    auto& engine = const_cast<pr7s::Studio&> (studio).getEngine();
    return studio.getProject().barStartAt (juce::jmax (0.0, engine.getPosition()));
}

double ComposerPage::buildStart() const
{
    // (0.74.0) After the last section built (the song grows a section at a time); the first at the playhead's bar.
    if (state.fromChordTrack || getTargetPlace().has_value())
        return placeStart();
    const auto& built = studio.getBuiltSections();
    return built.empty() ? placeStart() : built.back().end;
}

std::vector<pr7s::ChordSpan> ComposerPage::spansAt (double start) const
{
    if (state.fromChordTrack)
        return spans;
    auto placed = spans;
    for (auto& s : placed)
    {
        s.start += start;
        s.end += start;
    }
    return placed;
}

std::vector<pr7s::ChordSpan> ComposerPage::placedSpans() const
{
    return spansAt (hearing ? hearingStart : buildStart());
}

//==============================================================================
void ComposerPage::generate()
{
    for (const auto l : pr7s::composerLayers())
        if (! state.lane (l).locked)
            taken[(size_t) l].reset();
    pr7s::generateComposer (state, random);
    if (! state.lane (ComposerLayer::guitar).locked)
        takeNewRiff();
    if (! state.lane (ComposerLayer::drums).locked)
    {
        if (state.lane (ComposerLayer::drums).hasBarLocks())
            pr7s::rollFill (state, context, random, barCount(), &spans);   // (0.203.0: a new fill in each bar not locked)
        else
            pr7s::rollLibraryGroove (state, placedSpans(), context, random);   // (0.137.0: the next of the section's 100)
    }
    status = "Generated: a new take of every layer not locked";
    refresh();
}

void ComposerPage::roll (ComposerLayer l)
{
    // (0.202.0, the owner: "select just those bars and hit roll to get a bunch of 1 bar fill choices") The drums with
    // bars locked: another fill over the bars not locked.
    if (l == ComposerLayer::drums && state.lane (l).hasBarLocks())
    {
        taken[(size_t) l].reset();
        if (pr7s::rollFill (state, context, random, barCount(), &spans))
        {
            // (0.203.0, the owner: "make each open bar get its own fill") Each open bar a fill of its own.
            const auto fills = pr7s::composerFills (state, context, &spans);
            juce::StringArray names;
            for (const auto& [bar, id] : pr7s::barFills (state.lane (l), barCount()))
                for (int i = 0; i < (int) fills.size(); ++i)
                    if (fills[(size_t) i].id == id)
                        names.add ("bar " + juce::String (bar + 1) + ": " + fills[(size_t) i].name);
            status = "Drums: a new fill in each bar not locked (" + names.joinIntoString (", ") + ")";
            refresh();
            return;
        }
    }
    rollGroove (l);
}

void ComposerPage::rollGroove (ComposerLayer l)
{
    if (l == ComposerLayer::drums)
        state.lane (l).fill = {};
    taken[(size_t) l].reset();
    pr7s::rollLane (state, l, random);
    if (l == ComposerLayer::guitar)
        takeNewRiff();
    status = pr7s::composerLayerName (l) + ": a new take";
    // (0.137.0, the owner: "it scrolls through all 100 ... in random order") The drums with library grooves: the next
    // one of the section's pool none heard again until all have been.
    if (l == ComposerLayer::drums)
        if (pr7s::rollLibraryGroove (state, placedSpans(), context, random))
            if (const auto now = pr7s::composerLibraryGroove (state, placedSpans(), context))
            {
                auto heard = juce::StringArray::fromTokens (state.groovesHeard, ",", "");
                heard.removeEmptyStrings();
                const auto pool = pr7s::composerGroovePool (state, placedSpans(), context);
                auto count = 0;
                for (const auto& g : pool)
                    count += heard.contains (juce::String::toHexString ((juce::int64) g.file.getFullPathName().hashCode64())) ? 1 : 0;
                status = "Drums: " + now->name + " (" + juce::String (count) + " of " + juce::String ((int) pool.size()) + " heard)";
            }
    refresh();
}

void ComposerPage::takeNewRiff()
{
    // (0.85.0) A new riff every time: a take whose riff was given lately is drawn again (a few tries).
    auto fingerprint = pr7s::guitarRiffFingerprint (state, context);
    if (fingerprint.isEmpty())
        return;
    for (int tries = 0; tries < 40 && std::find (recentRiffs.begin(), recentRiffs.end(), fingerprint) != recentRiffs.end(); ++tries)
    {
        pr7s::rollLane (state, ComposerLayer::guitar, random);
        fingerprint = pr7s::guitarRiffFingerprint (state, context);
    }
    recentRiffs.push_back (fingerprint);
    while ((int) recentRiffs.size() > recentRiffsKept)
        recentRiffs.pop_front();
}

void ComposerPage::setRiffFlavour (int flavour)
{
    state.riffFlavour = juce::jlimit (0, (int) pr7s::riffFlavours().size() - 1, flavour);
    state.writtenRiffs = false;
    taken[(size_t) ComposerLayer::guitar].reset();
    status = "Guitar: " + pr7s::riffFlavourName (pr7s::riffFlavours()[(size_t) state.riffFlavour]) + " riffs";
    refresh();
}

void ComposerPage::setWrittenRiffs (bool written)
{
    state.writtenRiffs = written;
    taken[(size_t) ComposerLayer::guitar].reset();
    status = written ? "Guitar: the written riffs" : "Guitar: generated riffs";
    refresh();
}

void ComposerPage::setSync (std::optional<ComposerLayer> leader)
{
    state.syncLane = leader.has_value() ? (int) *leader : -1;
    if (leader.has_value())
    {
        state.lane (*leader).locked = true;   // (the riff you found stays; unlock it to roll it, the rest follows)
        for (const auto l : pr7s::composerLayers())
            if (l != *leader)
                taken[(size_t) l].reset();    // (an idea taken from IDEAS would ignore the leader)
        status = "SYNC: every lane follows the " + pr7s::composerLayerName (*leader).toLowerCase() + "'s rhythm (its lane locked)";
    }
    else
        status = "SYNC off: every lane its own";
    refresh();
}

void ComposerPage::setSyncTo (ComposerLayer l, std::optional<ComposerLayer> leader)
{
    state.lane (l).syncTo = leader.has_value() && *leader != l ? (int) *leader : -1;
    taken[(size_t) l].reset();   // (an idea taken from IDEAS would ignore the leader)
    status = pr7s::composerLayerName (l) + (state.followsOwnLeader (l) ? ": follows the " + pr7s::composerLayerName (*leader).toLowerCase() + "'s rhythm"
                                                                       : juce::String (": not synced on its own"));
    refresh();
}

void ComposerPage::setDoubleSync (ComposerLayer l, bool on)
{
    if (! pr7s::hasDoubleSync (l))
        return;
    state.lane (l).doubleSync = on;
    status = pr7s::composerLayerName (l) + (on ? ": doubling the sync track, every hit" : ": the sync track's accents");
    refresh();
}

void ComposerPage::showSyncMenu()
{
    const auto now = state.syncLayer();
    juce::PopupMenu menu;
    menu.addSectionHeader ("THE LANE THE OTHERS FOLLOW");
    menu.addItem (1, "Off (every lane its own rhythm)", true, ! now.has_value());
    menu.addSeparator();
    for (const auto l : pr7s::composerLayers())
        menu.addItem (10 + (int) l, "Sync to the " + pr7s::composerLayerName (l), true, now == l);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (syncKey),
                        [safe = juce::Component::SafePointer<ComposerPage> (this)] (int chosen)
                        {
                            if (safe == nullptr || chosen <= 0)
                                return;
                            safe->setSync (chosen == 1 ? std::optional<ComposerLayer> {} : std::optional<ComposerLayer> ((ComposerLayer) (chosen - 10)));
                        });
}

void ComposerPage::setBusy (ComposerLayer l, float busy)
{
    state.lane (l).busy = juce::jlimit (0.0f, 1.0f, busy);
    refresh();
}

void ComposerPage::setOctave (ComposerLayer l, int octave)
{
    if (l == ComposerLayer::drums)
        return;
    state.lane (l).octave = juce::jlimit (-2, 2, octave);
    refresh();
}

void ComposerPage::showOctaveMenu (ComposerLayer l)
{
    const auto current = state.lane (l).octave;
    juce::PopupMenu menu;
    menu.addItem (100, "Up an Octave", current < 2);
    menu.addItem (101, "Down an Octave", current > -2);
    menu.addSeparator();
    for (int o = 2; o >= -2; --o)
        menu.addItem (10 + o, o == 0 ? juce::String ("As Written") : (o > 0 ? "+" : "") + juce::String (o) + (std::abs (o) == 1 ? " Octave" : " Octaves"),
                      true, o == current);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (lanes[(size_t) l].octave),
                        [safe = juce::Component::SafePointer<ComposerPage> (this), l, current] (int chosen)
                        {
                            if (safe == nullptr || chosen <= 0)
                                return;
                            safe->setOctave (l, chosen == 100 ? current + 1 : chosen == 101 ? current - 1 : chosen - 10);
                        });
}

void ComposerPage::setSecondAcoustic (int part)
{
    state.lane (ComposerLayer::acoustic).second = juce::jlimit (0, pr7s::secondAcousticParts().size() - 1, part);
    refresh();
}

void ComposerPage::showSecondMenu()
{
    const auto current = state.lane (ComposerLayer::acoustic).second;
    juce::PopupMenu menu;
    menu.addSectionHeader ("THE SECOND ACOUSTIC (THE LANE'S SECOND TRACK)");
    const auto& parts = pr7s::secondAcousticParts();
    for (int i = 0; i < parts.size(); ++i)
        menu.addItem (i + 1, parts[i], true, i == current);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (lanes[(size_t) ComposerLayer::acoustic].second),
                        [safe = juce::Component::SafePointer<ComposerPage> (this)] (int chosen)
                        {
                            if (safe != nullptr && chosen > 0)
                                safe->setSecondAcoustic (chosen - 1);
                        });
}

void ComposerPage::setLowString (ComposerLayer l, bool on)
{
    if (! pr7s::hasLowString (l))
        return;
    state.lane (l).lowString = on;
    if (on)
        state.lane (l).onlyString = 0;
    refresh();
}

void ComposerPage::setOnlyString (ComposerLayer l, int string)
{
    if (! pr7s::hasLowString (l))
        return;
    const auto strings = (int) pr7s::composerStrings (l, context).strings.size();
    state.lane (l).onlyString = juce::jlimit (0, strings, string);
    if (string > 0)
        state.lane (l).lowString = false;
    refresh();
}

juce::String ComposerPage::stringText (ComposerLayer l) const
{
    const auto& s = state.lane (l);
    return s.onlyString > 0 ? "STRING " + juce::String (s.onlyString) + " ONLY" : s.lowString ? juce::String ("LOW STRING") : juce::String ("ANY STRING");
}

void ComposerPage::setTuning (ComposerLayer l, const juce::String& name)
{
    if (l == ComposerLayer::bass)
        studio.setBassTuning (name);
    else if (l == ComposerLayer::guitar)
        studio.setGuitarTuning (name);
    refresh();
}

void ComposerPage::showBusyMenu (ComposerLayer l)
{
    juce::PopupMenu menu;
    const auto current = juce::roundToInt (state.lane (l).busy * 100.0f);
    const std::pair<int, const char*> choices[] = { { 0, "0%: sparse" }, { 15, "15%" }, { 30, "30%" }, { 50, "50%: as the approach usually goes" },
                                                     { 65, "65%" }, { 80, "80%" }, { 100, "100%: busy" } };
    for (const auto& c : choices)
        menu.addItem (c.first + 1, c.second, true, c.first == current);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (lanes[(size_t) l].busy),
                        [safe = juce::Component::SafePointer<ComposerPage> (this), l] (int chosen)
                        {
                            if (safe != nullptr && chosen > 0)
                                safe->setBusy (l, (float) (chosen - 1) / 100.0f);
                        });
}

void ComposerPage::showLowMenu (ComposerLayer l)
{
    const auto& s = state.lane (l);
    const auto on = pr7s::composerStrings (l, context);
    juce::PopupMenu menu;
    menu.addItem (1, "Any string", true, ! s.lowString && s.onlyString == 0);
    menu.addItem (2, "The lowest string as much as it can (open to the 12th fret)", true, s.lowString);
    menu.addSectionHeader ("ONLY ONE STRING (THE WHOLE PART ON IT)");
    for (int i = 0; i < (int) on.strings.size(); ++i)
    {
        const auto key = on.profile != nullptr ? on.profile->keyFor (i + 1) : -1;
        menu.addItem (10 + i + 1, "String " + juce::String (i + 1) + " (" + pr7s::noteName (on.strings[(size_t) i]) + ")"
                                  + (key >= 0 ? ", keyswitch " + pr7s::noteName (key) + " held" : juce::String (on.profile != nullptr ? ", no keyswitch for it" : "")),
                      true, s.onlyString == i + 1);
    }
    menu.addSeparator();
    if (on.profile != nullptr)
    {
        menu.addSectionHeader ("TUNING AND RANGE");
        menu.addItem (-1, "From the Hub's INSTRUMENTS: " + on.profile->name + " (" + pr7s::noteName (on.profile->rangeLow()) + " to "
                          + pr7s::noteName (on.profile->rangeHigh()) + ")", false, false);
    }
    else
    {
        const auto& tunings = l == ComposerLayer::bass ? pr7s::bassTunings() : pr7s::GuitarTuning::all();
        const auto current = l == ComposerLayer::bass ? studio.getBassTuning() : studio.getGuitarTuning();
        menu.addSectionHeader (l == ComposerLayer::bass ? "BASS TUNING" : "GUITAR TUNING");
        for (size_t i = 0; i < tunings.size(); ++i)
            menu.addItem (100 + (int) i, tunings[i].name, true, tunings[i].name == current);
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (lanes[(size_t) l].low),
                        [safe = juce::Component::SafePointer<ComposerPage> (this), l] (int chosen)
                        {
                            if (safe == nullptr || chosen == 0)
                                return;
                            if (chosen == 1)
                            {
                                safe->state.lane (l).lowString = false;
                                safe->setOnlyString (l, 0);
                            }
                            else if (chosen == 2)
                                safe->setLowString (l, true);
                            else if (chosen > 10 && chosen < 100)
                                safe->setOnlyString (l, chosen - 10);
                            else
                            {
                                const auto& list = l == ComposerLayer::bass ? pr7s::bassTunings() : pr7s::GuitarTuning::all();
                                if (juce::isPositiveAndBelow (chosen - 100, (int) list.size()))
                                    safe->setTuning (l, list[(size_t) chosen - 100].name);
                            }
                        });
}

void ComposerPage::setLocked (ComposerLayer l, bool on)
{
    state.lane (l).locked = on;
    refresh (false);
}

void ComposerPage::setOn (ComposerLayer l, bool on)
{
    state.lane (l).on = on;
    refresh();
}

void ComposerPage::setSoloStyle (pr7s::SoloStyle st)
{
    // The setting IDEAS' SOLO box uses; the lead lane switched to the solo (its last style) so it's heard.
    studio.setSoloStyle (st);
    setStyle (ComposerLayer::lead, pr7s::composerStyleCount (ComposerLayer::lead) - 1);
}

void ComposerPage::setStyle (ComposerLayer l, int style)
{
    taken[(size_t) l].reset();
    if (l == ComposerLayer::guitar)
        useMyRiff = false;
    state.lane (l).style = juce::jlimit (0, pr7s::composerStyleCount (l) - 1, style);
    if (l == ComposerLayer::drums)
        state.lane (l).fill = {};   // (0.202.0: the style's own notes in the bars not locked)
    refresh();
}

void ComposerPage::setKey (const pr7s::Key& k)
{
    // (0.114.0) With the SECTIONS row, the key is the song's (or, with key changes allowed, the block's): set there,
    // everything already in the song moves into it.
    if (usesSectionsRow())
    {
        const auto index = studio.getSongSections().indexOf (targetBlock);
        if (index >= 0)
            studio.setSongSectionKey (index, k);
        else
            studio.setSongKey (k);
        state.key = k;
        chordOverrides.clear();
        status = (studio.isOneKey() ? "The song's key: " : "This section's key: ") + ChordUi::spokenKey (k)
               + " (everything in it moved into it)";
        refresh();
        return;
    }
    if (isKeyLocked() && ! (k == state.key))
    {
        status = "The key is locked for the song (a section is built): right-click the SONG strip to start again";
        refresh (false);
        return;
    }
    state.key = k;
    chordOverrides.clear();
    refresh();
}

void ComposerPage::setGenre (const juce::String& g)
{
    state.genre = g;
    refresh();
}

void ComposerPage::setMood (const juce::String& m)
{
    state.mood = m;
    refresh();
}

void ComposerPage::setBars (int bars)
{
    chordOverrides.clear();
    state.bars = bars == 4 || bars == 16 ? bars : 8;
    refresh();
}

void ComposerPage::setProgression (const juce::String& name)
{
    chordOverrides.clear();
    state.progression = name;
    refresh();
}

std::vector<pr7s::Progression> ComposerPage::getPopularInKey() const
{
    std::vector<pr7s::Progression> out;
    for (const auto& p : pr7s::popularProgressions (state.key))
        if (std::all_of (p.chords.begin(), p.chords.end(), [this] (const pr7s::Chord& c) { return c.isInKey (state.key); }))
            out.push_back (p);
    return out;
}

void ComposerPage::usePopularProgression (int index)
{
    const auto popular = getPopularInKey();
    if (! juce::isPositiveAndBelow (index, (int) popular.size()))
        return;
    const auto& chosen = popular[(size_t) index];
    if (hearing)
        stop();
    state.fromChordTrack = false;
    fromRiff = false;
    chordOverrides.clear();
    // The library's progression with these chords, when there is one (its rhythm, its name).
    for (const auto& p : pr7s::progressionLibrary())
    {
        const auto steps = p.in (state.key);
        if (steps.size() != chosen.chords.size())
            continue;
        auto same = true;
        for (size_t i = 0; i < steps.size() && same; ++i)
            same = steps[i].chord == chosen.chords[i] && std::abs (steps[i].bars - 1.0) < 1.0e-9;
        if (! same)
            continue;
        state.genre = {};
        state.mood = {};
        state.progression = p.name;
        status = "Popular in " + ChordUi::spokenKey (state.key) + ": " + chosen.name + " (the library's " + p.name + ")";
        refresh();
        return;
    }
    // Else its chords over the progression's, in turn.
    spans = chordSpans();
    for (int i = 0; i < (int) spans.size(); ++i)
        chordOverrides[i] = chosen.chords[(size_t) i % chosen.chords.size()];
    status = "Popular in " + ChordUi::spokenKey (state.key) + ": " + chosen.name;
    refresh();
}

bool ComposerPage::rollChords()
{
    // The block's name (or the composer's SECTION's) gives the roll its feel.
    static const juce::StringArray sectionNames { "Intro", "Verse", "Chorus", "Bridge", "Breakdown", "Ending", "Solo" };
    const auto seed = (juce::uint32) random.nextInt();
    if (getTargetPlace().has_value())
    {
        const auto& sections = studio.getSongSections();
        const auto index = sections.indexOf (targetBlock);
        if (! studio.rollSectionChords (index, seed))
        {
            status = "Nothing to roll the chords of";
            repaint();
            return false;
        }
        state.fromChordTrack = true;
        fromRiff = false;
        chordOverrides.clear();
        juce::StringArray names;
        for (const auto& c : studio.getSectionChords (index))
            names.add (c.chord.name (studio.getSongSectionKey (index)));
        status = "ROLL CHORDS: " + pr7s::sectionLabel (sections.blocks, index) + " now " + names.joinIntoString (" ")
               + (studio.isSongSectionBuilt (index) ? " (its lanes built again over them)" : "");
        refresh();
        return true;
    }
    if (state.fromChordTrack)
    {
        status = "The chord track's chords: aim at a SECTIONS block to roll them";
        repaint();
        return false;
    }
    const auto source = chordSpans();
    if (source.empty())
    {
        status = "No chords to roll";
        repaint();
        return false;
    }
    pr7s::ChordRollRequest request;
    request.key = state.key;
    const auto section = (int) std::distance (pr7s::drumSections().begin(),
                                              std::find (pr7s::drumSections().begin(), pr7s::drumSections().end(), getSection()));
    request.section = sectionNames[juce::jlimit (0, sectionNames.size() - 1, section)];
    request.beatsPerBar = context.beatsPerBar;
    const auto total = source.back().end - source.front().start;
    request.bars = juce::jmax (1, juce::roundToInt (total / juce::jmax (0.25, context.beatsPerBar)));
    for (const auto& s : source)
        request.layout.push_back (s.end - s.start);
    request.avoid = chordRollsLately;
    {
        std::vector<pr7s::RolledChord> now;
        for (int i = 0; i < (int) source.size(); ++i)
            now.push_back ({ chordOverrides.count (i) > 0 ? chordOverrides[i] : source[(size_t) i].chord, source[(size_t) i].end - source[(size_t) i].start });
        request.avoid.push_back (pr7s::rolledFingerprint (now));
    }
    const auto rolled = pr7s::rollChords (request, seed);
    if (rolled.empty())
        return false;
    chordRollsLately.push_back (pr7s::rolledFingerprint (rolled));
    if (chordRollsLately.size() > 24)
        chordRollsLately.erase (chordRollsLately.begin());
    // Each of the source's chords: the rolled chord sounding at its start.
    chordOverrides.clear();
    for (int i = 0; i < (int) source.size(); ++i)
    {
        const auto offset = source[(size_t) i].start - source.front().start;
        auto t = 0.0;
        auto chord = rolled.front().chord;
        for (const auto& r : rolled)
        {
            if (t > offset + 1.0e-6)
                break;
            chord = r.chord;
            t += r.beats;
        }
        chordOverrides[i] = chord;
    }
    juce::StringArray names;
    for (const auto& r : rolled)
        names.add (r.chord.name (state.key));
    status = "ROLL CHORDS: " + names.joinIntoString (" ");
    refresh();
    return true;
}

void ComposerPage::setFromChordTrack (bool on)
{
    if (on && studio.getLineChords().empty())
    {
        status = "The chord track has no chords yet";
        on = false;
    }
    if (hearing)
        stop();
    state.fromChordTrack = on;
    if (on)
        fromRiff = false;
    chordOverrides.clear();
    refresh();
}

void ComposerPage::setToChordTrack (bool on)
{
    state.toChordTrack = on;
    refresh (false);
}

void ComposerPage::setTrack (ComposerLayer l, int trackId)
{
    trackChoice[(size_t) l] = trackId;
    refresh();
}

int ComposerPage::trackFor (ComposerLayer l, bool forHearing) const
{
    if (const auto chosen = trackChoice[(size_t) l]; studio.canTakeMidi (chosen))
        return chosen;
    if (const auto named = studio.findIdeaTrack (pr7s::composerPartOf (l)))
        return named;
    if (! forHearing || l == ComposerLayer::drums)
        return 0;   // (drums on a piano or a guitar would be noise: heard only on a track of their own)
    if (const auto selected = studio.getSelectedTrack(); studio.canTakeMidi (selected))
        return selected;
    for (const auto& t : studio.getProject().tracks)
        if (studio.canTakeMidi (t.id))
            return t.id;
    return 0;
}

std::vector<int> ComposerPage::tracksFor (ComposerLayer l, bool forHearing) const
{
    if (const auto chosen = trackChoice[(size_t) l]; studio.canTakeMidi (chosen))
        return { chosen };
    if (auto found = studio.findIdeaTracks (pr7s::composerPartOf (l)); ! found.empty())
        return found;
    return { trackFor (l, forHearing) };
}

ComposerPage::Placed ComposerPage::placeLanes (double originBeat, bool forHearing) const
{
    Placed p;
    for (const auto l : pr7s::composerLayers())
        if (state.lane (l).on && ! ideas[(size_t) l].notes.empty())
        {
            const auto tracks = tracksFor (l, forHearing);
            for (size_t i = 0; i < tracks.size(); ++i)
            {
                if (forHearing && tracks[i] == 0)
                    continue;
                p.ideas.push_back (studio.doubledCopy (ideas[(size_t) l], (int) i, originBeat, (int) tracks.size()));
                p.onto.push_back (tracks[i]);
                p.layers.push_back ((int) l);
            }
        }
    return p;
}

juce::String ComposerPage::trackText (ComposerLayer l) const
{
    juce::StringArray names;
    for (const auto id : tracksFor (l, false))
        if (const auto* t = studio.getProject().findTrack (id))
            names.add (t->name.toUpperCase());
    if (names.size() > 1)
        return "TRACKS: " + names.joinIntoString (" + ");
    if (names.size() == 1)
        return "TRACK: " + names[0];
    return "TRACK: NEW (" + studio.newIdeaTrackName (pr7s::composerPartOf (l)).toUpperCase() + ")";
}

//==============================================================================
bool ComposerPage::hear()
{
    if (spans.empty())
    {
        status = state.fromChordTrack ? "The chord track has no chords" : "No progression fits the key, genre and mood";
        return false;
    }
    if (! hearing)
        hearingStart = buildStart();
    const auto placed = placedSpans();
    std::vector<std::pair<int, pr7s::LineIdea>> layers;
    {
        auto lanesPlaced = placeLanes (placed.front().start, true);
        for (size_t i = 0; i < lanesPlaced.ideas.size(); ++i)
            layers.emplace_back (lanesPlaced.onto[i], std::move (lanesPlaced.ideas[i]));
    }
    if (! studio.auditionComposer (layers, placed.front().start, placed.back().end))
    {
        status = "Nothing to hear them on: add an instrument track (or switch a layer on)";
        hearing = false;
        hearKey.setButtonText ("HEAR");
        hearKey.setToggleState (false, juce::dontSendNotification);
        repaint();
        return false;
    }
    hearing = true;
    seenChange = studio.getChangeVersion();
    seenContent = studio.getContentVersion();
    hearKey.setButtonText ("STOP");
    hearKey.setToggleState (true, juce::dontSendNotification);
    repaint();
    return true;
}

void ComposerPage::closeOptionBrowser()
{
    browserWindow.reset();   // (as its X: a trial not used puts the lane back)
}

void ComposerPage::stop()
{
    if (hearing)
        studio.stopComposerAudition();
    hearing = false;
    hearKey.setButtonText ("HEAR");
    hearKey.setToggleState (false, juce::dontSendNotification);
    repaint();
}

bool ComposerPage::isHearing() const
{
    return hearing && studio.isComposerAuditioning();
}

std::vector<int> ComposerPage::useAll()
{
    stop();
    if (spans.empty())
        return {};
    const auto placed = spansAt (placeStart());
    const auto lanesPlaced = placeLanes (placed.front().start, false);
    writeChordsAt (placed);
    const auto tracks = studio.buildSection (lanesPlaced.ideas, placed, "Architect", lanesPlaced.onto);
    juce::StringArray names;
    for (const auto id : tracks)
        if (const auto* t = studio.getProject().findTrack (id))
            names.add (t->name);
    const auto bar = studio.getProject().signatures().barAt (placed.front().start) + 1;
    status = tracks.empty() ? juce::String ("Nothing written: switch a layer on")
                            : "Written at bar " + juce::String (bar) + " onto " + names.joinIntoString (", ");
    if (const auto tones = studio.getLastTones(); ! tones.isEmpty() && ! tracks.empty())   // (0.204.0)
        status += "  " + tones.joinIntoString (", ");
    refresh (false);
    return tracks;
}

int ComposerPage::useLayer (ComposerLayer l)
{
    stop();
    if (spans.empty() || ideas[(size_t) l].notes.empty())
        return 0;
    const auto placed = spansAt (placeStart());
    std::vector<pr7s::LineIdea> list;
    const auto onto = tracksFor (l, false);
    for (size_t i = 0; i < onto.size(); ++i)
        list.push_back (studio.doubledCopy (ideas[(size_t) l], (int) i, placed.front().start, (int) onto.size()));
    const auto tracks = studio.buildSection (list, placed, "Architect", onto);
    refresh (false);
    return tracks.empty() ? 0 : tracks.front();
}

//==============================================================================
// (0.76.0) Each lane's volume. (0.206.0, the owner: "the whole reason to set ARCHITECT volume is for hearing the riffs
// play in ARCHITECT") The lane's ARCHITECT level (the Hub's ARCHITECT TRACKS shows the same): what HEAR plays its tracks
// at and where a new or empty track of it starts. It never reads or moves a track's fader (it read the hearing's
// stand-in, the selected or first instrument track: the owner's CHORDS lane showed BASS's 0 dB and dragged BASS).
void ComposerPage::setLaneVolume (ComposerLayer l, float db, bool undoable)
{
    juce::ignoreUnused (undoable);   // (a setting, not a song edit: no undo steps)
    const auto part = pr7s::composerPartOf (l);
    if (const auto lane = pr7s::Studio::composerTrackLane (part); lane >= 0 && std::abs (studio.getLaneLevel (part) - db) >= 0.01f)
        studio.setLaneLevel (lane, db);   // (heard at once while HEAR plays)
}

void ComposerPage::refreshVolumes()
{
    for (const auto l : pr7s::composerLayers())
    {
        auto& v = lanes[(size_t) l].volume;
        const auto db = (double) studio.getLaneLevel (pr7s::composerPartOf (l));
        if (std::abs (v.getValue() - db) > 0.01)
            v.setValue (db, juce::dontSendNotification);
    }
}

//==============================================================================
// (0.221.0, the owner: "i changed the chord progression for the solo ... when i open architect it is still using the
// original chords") A block whose chords on the chord track are new to us (another block aimed at, or its chords
// changed in the block editor or on the chord track) is composed from them: CHORDS FROM: CHORD TRACK. Choosing
// LIBRARY afterwards still works: nothing changes it back until those chords change again.
void ComposerPage::followBlockChords()
{
    const auto index = studio.getSongSections().indexOf (targetBlock);
    if (index < 0)
    {
        seenChordsBlock = 0;
        seenBlockChords.clear();
        return;
    }
    auto now = studio.getSectionChords (index);
    if (targetBlock == seenChordsBlock && now == seenBlockChords)
        return;
    seenChordsBlock = targetBlock;
    seenBlockChords = std::move (now);
    if (seenBlockChords.empty() || state.fromChordTrack)
        return;
    state.fromChordTrack = true;
    fromRiff = false;
    chordOverrides.clear();
}

// (0.75.0) The key locked, chords per section, your own riff.
bool ComposerPage::isKeyLocked() const
{
    return ! studio.getBuiltSections().empty();
}

std::vector<pr7s::ChordSpan> ComposerPage::chordSpans() const
{
    if (state.fromChordTrack)
        // (0.212.0, the owner: "architect is now saying it has no chords") A SECTIONS block reads every chord on the
        // chord track: with CYCLE on, only the cycle's chords were read, then cut to a block outside the cycle.
        return studio.getLineChords ({}, ! getTargetPlace().has_value());
    if (fromRiff)
        if (const auto& riff = studio.getComposerRiff())
        {
            const auto bpb = context.beatsPerBar;
            return pr7s::riffSpans (pr7s::riffChords (*riff, state.key, bpb), state.key, 0.0, state.bars * bpb, riff->bars * bpb);
        }
    return pr7s::composerSpans (state, 0.0, context.beatsPerBar);
}

std::vector<pr7s::Chord> ComposerPage::getChordChoices() const
{
    std::vector<pr7s::Chord> out;
    for (const auto& c : pr7s::keyChords (state.key))
        if (std::find (out.begin(), out.end(), c) == out.end())
            out.push_back (c);
    return out;
}

void ComposerPage::setChord (int spanIndex, std::optional<pr7s::Chord> chord)
{
    if (state.fromChordTrack || ! juce::isPositiveAndBelow (spanIndex, (int) spans.size()))
        return;
    if (chord.has_value())
    {
        if (! chord->isInKey (state.key))
        {
            status = "Only the key's own chords (" + ChordUi::spokenKey (state.key) + ")";
            repaint();
            return;
        }
        chordOverrides[spanIndex] = *chord;
    }
    else
        chordOverrides.erase (spanIndex);
    status = "Chord " + juce::String (spanIndex + 1) + (chord.has_value() ? ": " + chord->name (state.key) : juce::String (": back to the source's"));
    refresh();
}

void ComposerPage::writeChordsAt (const std::vector<pr7s::ChordSpan>& placed)
{
    if (state.fromChordTrack || ! state.toChordTrack || placed.empty())
        return;
    // (0.85.3) The key chosen here goes onto the chord track's key strip with the chords: moving or
    // changing a chord there then shows when it leaves that key.
    const auto keyOnTrack = [this, start = placed.front().start] { studio.setChordTrackKey (start, state.key); };
    if (! fromRiff && chordOverrides.empty())
    {
        studio.addProgressionToChordTrack (pr7s::composerSteps (state), placed.front().start);
        keyOnTrack();
        return;
    }
    std::vector<pr7s::ProgressionStep> steps;
    for (const auto& span : placed)
    {
        pr7s::ProgressionStep step;
        step.chord = span.chord;
        step.bars = (span.end - span.start) / juce::jmax (0.25, context.beatsPerBar);
        steps.push_back (step);
    }
    studio.addProgressionToChordTrack (steps, placed.front().start);
    keyOnTrack();
}

void ComposerPage::setFromRiff (bool on)
{
    if (on && ! studio.getComposerRiff().has_value())
    {
        status = "No riff yet: + MY RIFF";
        on = false;
    }
    if (hearing)
        stop();
    fromRiff = on;
    if (on)
        state.fromChordTrack = false;
    chordOverrides.clear();
    refresh();
}

void ComposerPage::setPlayMyRiff (bool on)
{
    useMyRiff = on && studio.getComposerRiff().has_value();
    if (useMyRiff)
    {
        const auto l = state.riffLayer();
        taken[(size_t) l].reset();
        state.lane (l).on = true;
    }
    refresh();
}

void ComposerPage::setRiffLane (ComposerLayer l)
{
    // (0.114.0) Your riff onto that lane, as you played it; the others follow it (SYNC on it). The lane it left
    // goes back to writing its own part.
    const auto before = state.riffLayer();
    state.riffLane = (int) l;
    if (state.syncLane == (int) before)
        state.syncLane = (int) l;
    if (before != l)
        state.lane (before).locked = false;
    useMyRiff = studio.getComposerRiff().has_value();
    taken[(size_t) l].reset();
    state.lane (l).on = true;
    state.lane (l).locked = true;
    status = "Your riff plays on the " + pr7s::composerLayerName (l).toLowerCase() + " lane, as you played it; the others follow it";
    refresh();
}

bool ComposerPage::pasteRiff (pr7s::ComposerRiff riff, int lane)
{
    if (riff.notes.empty())
    {
        status = "That has no notes to make a riff of";
        repaint();
        return false;
    }
    stop();
    const auto bars = riff.bars;
    const auto section = riff.section;
    // (0.226.0) With the SECTIONS row and no block chosen: the riff goes on the blocks of its section (by name)
    // that have none yet, the first of them chosen. A block chosen: onto it (and its linked copies).
    std::vector<int> alsoOn;
    if (usesSectionsRow() && ! getTargetPlace().has_value())
    {
        const auto& blocks = studio.getSongSections().blocks;
        for (int i = 0; i < (int) blocks.size(); ++i)
            if (blocks[(size_t) i].riff == 0 && pr7s::composerSectionFor (blocks[(size_t) i].name) == (int) section)
                alsoOn.push_back (i);
        if (! alsoOn.empty())
        {
            targetBlock = blocks[(size_t) alsoOn.front()].id;
            studio.setComposerRiffBlock (targetBlock);
        }
    }
    studio.setComposerRiff (riff);
    if (alsoOn.size() > 1)
        if (const auto index = studio.getSongSections().indexOf (targetBlock); index >= 0)
            studio.setSongSectionRiff (alsoOn, studio.getSongSections().blocks[(size_t) index].riff);
    seenRiff = studio.getComposerRiff();
    juce::String keyText;
    if (! isKeyLocked())
    {
        state.key = pr7s::guessKey (riff.notes);
        keyText = "key guessed: " + ChordUi::spokenKey (state.key) + " (change KEY if it's wrong)";
    }
    else
        keyText = "in the song's key, " + ChordUi::spokenKey (state.key) + " (locked)";
    if (bars > state.bars)
        state.bars = bars <= 8 ? 8 : 16;
    // (0.226.0, the owner: a progression set for the verse, then a riff of their own used on it) Chords already
    // on the chord track where the riff goes win: the band follows them and BUILD leaves them; none there: the
    // riff's own chords, written onto the chord track as before.
    const auto start = buildStart();
    const auto end = getTargetPlace().has_value() ? getTargetPlace()->end : start + state.bars * juce::jmax (0.25, context.beatsPerBar);
    const auto keepChords = chordTrackHasChordsAt (start, end);
    fromRiff = ! keepChords;
    state.fromChordTrack = keepChords;
    useMyRiff = true;
    chordOverrides.clear();
    // (0.114.0, the owner: "imported without being changed at all ... select what composer lane it goes to. The
    // other composer lanes then have their initial riffs based off of it") Played unchanged on its lane, locked;
    // every other lane follows its rhythm (SYNC on it), the bass doubling it.
    const auto onto = juce::isPositiveAndBelow (lane, pr7s::composerLayerCount) ? (ComposerLayer) lane : state.riffLayer();
    riffLanesFor (onto);
    // The section's beat, and the riff's settings back to as written.
    studio.setRiffSettings ({});
    setSection (section);
    status = "Your riff (" + juce::String (bars) + (bars == 1 ? " bar" : " bars") + ") as the " + pr7s::sectionWord (section).toLowerCase()
           + " on the " + pr7s::composerLayerName (onto).toLowerCase() + " lane, the others following it; " + keyText;
    if (const auto index = studio.getSongSections().indexOf (targetBlock); index >= 0 && getTargetPlace().has_value())
    {
        // Which blocks have it now.
        const auto& blocks = studio.getSongSections().blocks;
        const auto id = blocks[(size_t) index].riff;
        juce::StringArray on;
        for (int i = 0; i < (int) blocks.size(); ++i)
            if (id > 0 && blocks[(size_t) i].riff == id)
                on.add (pr7s::sectionLabel (blocks, i));
        status += ".  On " + on.joinIntoString (", ");
    }
    if (keepChords)
    {
        refresh();
        const auto clashes = riffClashText();
        status += clashes.isNotEmpty() ? ".  " + clashes : juce::String (".  The chord track's chords kept; the riff agrees with them");
    }
    repaint();
    return true;
}

bool ComposerPage::chordTrackHasChordsAt (double start, double end) const
{
    for (const auto& span : const_cast<pr7s::Studio&> (studio).getLineChords ({}, false))
        if (span.end > start + 1.0e-6 && span.start < end - 1.0e-6)
            return true;
    return false;
}

void ComposerPage::setFitRiff (bool on)
{
    fitRiff = on && studio.getComposerRiff().has_value();
    taken[(size_t) state.riffLayer()].reset();
    status = fitRiff ? juce::String ("My riff fitted to the chords: its notes moved onto the chord under them (rhythm and shape kept)")
                     : juce::String ("My riff played exactly as you played it");
    refresh();
}

juce::String ComposerPage::riffClashText() const
{
    const auto& riff = studio.getComposerRiff();
    if (! riff.has_value() || fromRiff || spans.empty())
        return {};
    const auto clashes = pr7s::riffClashes (*riff, spans, context.beatsPerBar);
    if (clashes.empty())
        return {};
    const auto& sig = studio.getProject().signatures();
    juce::StringArray parts;
    for (size_t i = 0; i < clashes.size() && i < 3; ++i)
    {
        const auto& c = clashes[i];
        const auto key = spans.front().key;
        parts.add ("bar " + juce::String (sig.barAt (spans.front().start + c.beat) + 1) + ": the riff says " + c.riff.name (key)
                   + ", the track " + c.track.name (key));
    }
    if (clashes.size() > 3)
        parts.add ("and " + juce::String ((int) clashes.size() - 3) + " more");
    return "The riff's chords and the chord track's disagree (" + parts.joinIntoString ("; ") + ")"
         + (fitRiff ? juce::String ("; fitted to the track's") : juce::String ("; FIT MY RIFF TO THE CHORDS in + MY RIFF moves its notes onto them"));
}

void ComposerPage::riffLanesFor (ComposerLayer onto)
{
    // (0.114.0, the owner: "imported without being changed at all ... select what composer lane it goes to. The
    // other composer lanes then have their initial riffs based off of it") Played unchanged on its lane, locked;
    // every other lane follows its rhythm (SYNC on it), the bass doubling it.
    if (state.riffLayer() != onto)
        state.lane (state.riffLayer()).locked = false;
    state.riffLane = (int) onto;
    for (const auto l : pr7s::composerLayers())
        taken[(size_t) l].reset();
    state.lane (onto).on = true;
    state.lane (onto).locked = true;
    state.syncLane = (int) onto;
    if (onto != ComposerLayer::bass)
        state.lane (ComposerLayer::bass).doubleSync = true;
}

void ComposerPage::followRiffInHand()
{
    // (0.226.0) The riff in hand is the chosen block's (none: none), or the song's loose one with no block chosen.
    // When it changes (another block, its riff changed or taken off), the riff's lanes are set for it: on it, the
    // riff's lane locked with the others following; off it, that lane writes its own again.
    studio.setComposerRiffBlock (getTargetPlace().has_value() ? targetBlock : 0);
    const auto& riff = studio.getComposerRiff();
    if (riff == seenRiff)
        return;
    const auto had = seenRiff.has_value();
    seenRiff = riff;
    useMyRiff = riff.has_value() && studio.isComposerRiffPlayed();
    fromRiff = riff.has_value() && studio.areComposerChordsFromRiff();
    fitRiff = riff.has_value() && studio.isComposerRiffFitted();
    chordOverrides.clear();
    if (useMyRiff)
        riffLanesFor (state.riffLayer());
    else if (had && ! riff.has_value())
    {
        state.lane (state.riffLayer()).locked = false;
        if (state.syncLane == state.riffLane)
            state.syncLane = -1;
        taken[(size_t) state.riffLayer()].reset();
    }
}

bool ComposerPage::chooseBlockRiff (int riffId)
{
    const auto index = studio.getSongSections().indexOf (targetBlock);
    if (index < 0 || ! getTargetPlace().has_value())
        return false;
    stop();
    if (! studio.setSongSectionRiff (index, riffId))
        return false;
    if (riffId >= 0)
        studio.setComposerRiffUse (true, studio.areComposerChordsFromRiff());   // (a riff chosen for it: played)
    // Its lanes set for the riff now in hand (none: the riff's lane let go, as followRiffInHand does).
    if (studio.getComposerRiff().has_value())
        seenRiff.reset();
    followRiffInHand();
    status = pr7s::sectionLabel (studio.getSongSections().blocks, index) + ": " + blockRiffText();
    refresh();
    return true;
}

juce::String ComposerPage::blockRiffText() const
{
    const auto index = studio.getSongSections().indexOf (targetBlock);
    if (index < 0 || ! getTargetPlace().has_value())
        return {};
    if (const auto riff = studio.getSongSectionRiff (index))
        return "RIFF: " + riff->name.toUpperCase();
    return "NO RIFF (ITS LANES WRITE THEIR OWN)";
}

bool ComposerPage::pasteRiffFromPart (int partId, pr7s::DrumSection section, int lane)
{
    const auto where = studio.findPart (partId);
    if (! where.has_value())
        return false;
    const auto* track = studio.getProject().findTrack (where->first);
    if (track == nullptr || track->kind != pr7s::Track::Kind::instrument || ! juce::isPositiveAndBelow (where->second, (int) track->parts.size()))
    {
        status = "Choose a MIDI part (an instrument track's) for your riff";
        repaint();
        return false;
    }
    const auto& part = track->parts[(size_t) where->second];
    auto notes = part.notes;
    for (auto& n : notes)
    {
        n.pitch = part.heardPitch (n.pitch);
        n.velocity = part.heardVelocity (n.velocity);
    }
    notes.erase (std::remove_if (notes.begin(), notes.end(), [&part] (const pr7s::Note& n) { return n.start >= part.length; }), notes.end());
    const auto bpb = studio.getProject().beatsPerBarAt (part.start);
    return pasteRiff (pr7s::riffFromNotes (notes, part.start, bpb, track->name + (part.name.isNotEmpty() ? " - " + part.name : juce::String()), section), lane);
}

bool ComposerPage::pasteRiffFromFile (const juce::File& file, pr7s::DrumSection section, int lane)
{
    juce::String error;
    const auto midi = pr7s::readMidiFile (file, error);
    if (! midi.has_value())
    {
        status = error.isNotEmpty() ? error : "Not a MIDI file";
        repaint();
        return false;
    }
    // Every track's notes but the drums' (channel 10), together.
    std::vector<pr7s::Note> notes;
    for (const auto& t : midi->tracks)
        if (t.channel != 10)
            notes.insert (notes.end(), t.notes.begin(), t.notes.end());
    std::sort (notes.begin(), notes.end(), [] (const pr7s::Note& a, const pr7s::Note& b) { return a.start < b.start; });
    return pasteRiff (pr7s::riffFromNotes (notes, 0.0, context.beatsPerBar, file.getFileNameWithoutExtension(), section), lane);
}

std::vector<int> ComposerPage::buildFromPart (int partId, pr7s::DrumSection section, int lane)
{
    if (! pasteRiffFromPart (partId, section, lane))
        return {};
    const auto riffStatus = status;
    auto tracks = buildSection();
    if (! tracks.empty())
        status = riffStatus + ".  " + status;
    repaint();
    return tracks;
}

std::vector<int> ComposerPage::buildFromFile (const juce::File& file, pr7s::DrumSection section, int lane)
{
    if (! pasteRiffFromFile (file, section, lane))
        return {};
    const auto riffStatus = status;
    auto tracks = buildSection();
    if (! tracks.empty())
        status = riffStatus + ".  " + status;
    repaint();
    return tracks;
}

bool ComposerPage::canTakePart (int partId) const
{
    const auto where = studio.findPart (partId);
    if (! where.has_value())
        return false;
    const auto* track = studio.getProject().findTrack (where->first);
    return track != nullptr && track->kind == pr7s::Track::Kind::instrument && juce::isPositiveAndBelow (where->second, (int) track->parts.size())
        && ! track->parts[(size_t) where->second].notes.empty();
}

void ComposerPage::dropPart (int partId)
{
    askSectionAndLane ([safe = juce::Component::SafePointer<ComposerPage> (this), partId] (pr7s::DrumSection sec, int lane)
                       { if (safe != nullptr) safe->buildFromPart (partId, sec, lane); }, &pasteKey);
}

void ComposerPage::forgetRiff()
{
    studio.setComposerRiff (std::nullopt);
    fromRiff = false;
    useMyRiff = false;
    chordOverrides.clear();
    status = "Your riff forgotten";
    refresh();
}

void ComposerPage::askSection (std::function<void (pr7s::DrumSection)> then, juce::Component* target)
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("THIS RIFF IS MY...");
    int id = 1;
    for (const auto sec : pr7s::drumSections())
        menu.addItem (id++, pr7s::sectionWord (sec) + " Riff");
    auto options = juce::PopupMenu::Options();
    if (target != nullptr)
        options = options.withTargetComponent (target);
    menu.showMenuAsync (options, [then] (int chosen)
                        {
                            if (juce::isPositiveAndNotGreaterThan (chosen, (int) pr7s::drumSections().size()))
                                then (pr7s::drumSections()[(size_t) chosen - 1]);
                        });
}

void ComposerPage::askSectionAndLane (std::function<void (pr7s::DrumSection, int)> then, juce::Component* target)
{
    const auto askLane = [safe = juce::Component::SafePointer<ComposerPage> (this), then, target] (pr7s::DrumSection sec)
    {
        if (safe == nullptr)
            return;
        juce::PopupMenu menu;
        if (const auto index = safe->studio.getSongSections().indexOf (safe->targetBlock); index >= 0 && safe->getTargetPlace().has_value())
            menu.addSectionHeader ("FOR " + pr7s::sectionLabel (safe->studio.getSongSections().blocks, index).toUpperCase() + ": IT PLAYS ON THE...");
        else
            menu.addSectionHeader ("IT PLAYS ON THE... (AS YOU PLAYED IT)");
        for (const auto l : pr7s::composerLayers())
            menu.addItem (100 + (int) l, pr7s::composerLayerName (l) + " lane", true, l == safe->state.riffLayer());
        auto options = juce::PopupMenu::Options();
        if (target != nullptr)
            options = options.withTargetComponent (target);
        menu.showMenuAsync (options, [then, sec] (int chosen)
                            {
                                if (chosen >= 100 && chosen < 100 + pr7s::composerLayerCount)
                                    then (sec, chosen - 100);
                            });
    };
    // (0.226.0) A block chosen: the riff is that block's (its section), so only the lane is asked.
    if (const auto index = studio.getSongSections().indexOf (targetBlock); index >= 0 && getTargetPlace().has_value())
    {
        askLane (pr7s::drumSections()[(size_t) pr7s::composerSectionFor (studio.getSongSections().blocks[(size_t) index].name)]);
        return;
    }
    askSection (askLane, target);
}

void ComposerPage::showPasteMenu()
{
    const auto parts = getSelectedParts != nullptr ? getSelectedParts() : std::vector<int> {};
    int partId = 0;
    juce::String partName;
    for (const auto id : parts)
        if (const auto where = studio.findPart (id))
            if (const auto* t = studio.getProject().findTrack (where->first); t != nullptr && t->kind == pr7s::Track::Kind::instrument)
            {
                partId = id;
                partName = t->name;
                break;
            }
    const auto& riff = studio.getComposerRiff();
    juce::PopupMenu menu;
    menu.addSectionHeader ("BUILD A SECTION FROM MY RIFF");
    menu.addItem (7, partId != 0 ? "From the Selected Part (" + partName + ")" : juce::String ("From the Selected Part (select a MIDI part first)"), partId != 0);
    menu.addItem (8, "From a MIDI File...");
    menu.addSectionHeader ("ONLY LOAD MY RIFF (BUILD LATER)");
    menu.addItem (1, partId != 0 ? "From the Selected Part (" + partName + ")" : juce::String ("From the Selected Part (select a MIDI part first)"), partId != 0);
    menu.addItem (2, "From a MIDI File...");
    // (0.226.0) The chosen block's riff: one of the song's, or none.
    const auto songRiffs = studio.getSongRiffs();
    const auto blockIndex = getTargetPlace().has_value() ? studio.getSongSections().indexOf (targetBlock) : -1;
    if (blockIndex >= 0)
    {
        const auto& blocks = studio.getSongSections().blocks;
        const auto mine = blocks[(size_t) blockIndex].riff;
        menu.addSectionHeader (pr7s::sectionLabel (blocks, blockIndex).toUpperCase() + "'S RIFF");
        for (size_t i = 0; i < songRiffs.size() && i < 90; ++i)
        {
            const auto& r = songRiffs[i];
            const auto where = r.id == 0 ? juce::String ("the riff with no block") : r.blocks.joinIntoString (", ");
            menu.addItem (300 + (int) i, r.name + "  (" + juce::String (r.bars) + (r.bars == 1 ? " bar" : " bars") + ", " + where + ")",
                          true, r.id > 0 && r.id == mine);
        }
        menu.addItem (299, "No Riff: Its Lanes Write Their Own", true, mine == 0);
    }
    if (riff.has_value())
    {
        menu.addSeparator();
        menu.addItem (3, "My Riff Plays (" + pr7s::composerLayerName (state.riffLayer()) + " lane)", true, useMyRiff);
        juce::PopupMenu lanes;
        for (const auto l : pr7s::composerLayers())
            lanes.addItem (100 + (int) l, pr7s::composerLayerName (l), true, l == state.riffLayer());
        menu.addSubMenu ("My Riff Plays on the...", lanes);
        menu.addItem (4, "The Chords from My Riff", true, fromRiff);
        menu.addItem (9, "Fit My Riff to the Chords (its notes moved onto them)", ! fromRiff, fitRiff && ! fromRiff);   // (0.226.0)
        menu.addItem (5, blockIndex >= 0 ? "Take " + riff->name + " off " + pr7s::sectionLabel (studio.getSongSections().blocks, blockIndex)
                                         : "Forget My Riff (" + riff->name + ")");
    }
    menu.addSeparator();
    menu.addItem (6, "Learn My Band: Read Our Record...", onLearnMyBand != nullptr);   // (0.82.0)
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (pasteKey),
                        [safe = juce::Component::SafePointer<ComposerPage> (this), partId, songRiffs] (int chosen)
                        {
                            if (safe == nullptr || chosen == 0)
                                return;
                            if (chosen == 299)
                            {
                                safe->chooseBlockRiff (-1);
                                return;
                            }
                            if (chosen >= 300 && chosen < 300 + (int) songRiffs.size())
                            {
                                safe->chooseBlockRiff (songRiffs[(size_t) chosen - 300].id);
                                return;
                            }
                            if (chosen == 6)
                            {
                                if (safe->onLearnMyBand)
                                    safe->onLearnMyBand();
                                return;
                            }
                            if (chosen >= 100 && chosen < 100 + pr7s::composerLayerCount)
                                safe->setRiffLane ((ComposerLayer) (chosen - 100));
                            else if (chosen == 1 || chosen == 7)
                            {
                                const auto build = chosen == 7;
                                safe->askSectionAndLane ([safe, partId, build] (pr7s::DrumSection sec, int lane)
                                {
                                    if (safe == nullptr)
                                        return;
                                    if (build)
                                        safe->buildFromPart (partId, sec, lane);
                                    else
                                        safe->pasteRiffFromPart (partId, sec, lane);
                                }, &safe->pasteKey);
                            }
                            else if (chosen == 2 || chosen == 8)
                            {
                                const auto build = chosen == 8;
                                safe->chooser = std::make_unique<juce::FileChooser> ("Your riff (a MIDI file)", juce::File(), "*.mid;*.midi");
                                safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                                            [safe, build] (const juce::FileChooser& fc)
                                                            {
                                                                const auto file = fc.getResult();
                                                                if (safe == nullptr || ! file.existsAsFile())
                                                                    return;
                                                                juce::MessageManager::callAsync ([safe, file, build]
                                                                {
                                                                    if (safe != nullptr)
                                                                        safe->askSectionAndLane ([safe, file, build] (pr7s::DrumSection sec, int lane)
                                                                        {
                                                                            if (safe == nullptr)
                                                                                return;
                                                                            if (build)
                                                                                safe->buildFromFile (file, sec, lane);
                                                                            else
                                                                                safe->pasteRiffFromFile (file, sec, lane);
                                                                        }, &safe->pasteKey);
                                                                });
                                                            });
                            }
                            else if (chosen == 3)
                                safe->setPlayMyRiff (! safe->useMyRiff);
                            else if (chosen == 4)
                                safe->setFromRiff (! safe->fromRiff);
                            else if (chosen == 9)
                                safe->setFitRiff (! safe->fitRiff);
                            else if (chosen == 5)
                                safe->forgetRiff();
                        });
}

bool ComposerPage::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (juce::File (f).hasFileExtension ("mid;midi"))
            return true;
    return false;
}

void ComposerPage::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
        if (juce::File (f).hasFileExtension ("mid;midi"))
        {
            const juce::File file (f);
            // (0.83.4) Dropped in: taken and the section built around it.
            askSectionAndLane ([safe = juce::Component::SafePointer<ComposerPage> (this), file] (pr7s::DrumSection sec, int lane)
                               { if (safe != nullptr) safe->buildFromFile (file, sec, lane); }, nullptr);
            return;
        }
}

int ComposerPage::chordAt (juce::Point<int> p) const
{
    if (spans.empty() || ! ruler.contains (p))
        return -1;
    const auto first = spans.front().start, total = spans.back().end - first;
    const auto beat = first + (double) (p.x - ruler.getX()) / juce::jmax (1, ruler.getWidth()) * total;
    for (int i = 0; i < (int) spans.size(); ++i)
        if (beat >= spans[(size_t) i].start && beat < spans[(size_t) i].end)
            return i;
    return -1;
}

void ComposerPage::showChordMenu (int spanIndex)
{
    if (state.fromChordTrack)
    {
        status = "The chord track's chords are changed on the chord track";
        repaint();
        return;
    }
    const auto choices = getChordChoices();
    const auto now = spans[(size_t) spanIndex].chord;
    juce::PopupMenu menu;
    menu.addSectionHeader ("CHORD " + juce::String (spanIndex + 1) + ": ONLY " + ChordUi::spokenKey (state.key).toUpperCase() + "'S CHORDS");
    for (int i = 0; i < (int) choices.size(); ++i)
        menu.addItem (i + 1, choices[(size_t) i].name (state.key) + "   " + choices[(size_t) i].romanName (state.key), true, choices[(size_t) i] == now);
    if (chordOverrides.count (spanIndex) > 0)
    {
        menu.addSeparator();
        menu.addItem (1000, fromRiff ? "Back to My Riff's Chord" : "Back to the Progression's Chord");
    }
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<ComposerPage> (this), spanIndex, choices] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        if (chosen == 1000)
            safe->setChord (spanIndex, std::nullopt);
        else if (juce::isPositiveAndNotGreaterThan (chosen, (int) choices.size()))
            safe->setChord (spanIndex, choices[(size_t) chosen - 1]);
    });
}

//==============================================================================
// (0.74.0) Sections and the song.
pr7s::DrumSection ComposerPage::getSection() const
{
    const auto& all = pr7s::drumSections();
    return all[(size_t) juce::jlimit (0, (int) all.size() - 1, state.lane (ComposerLayer::drums).style)];
}

void ComposerPage::setSection (pr7s::DrumSection section)
{
    const auto& all = pr7s::drumSections();
    const auto index = (int) std::distance (all.begin(), std::find (all.begin(), all.end(), section));
    taken[(size_t) ComposerLayer::drums].reset();
    state.lane (ComposerLayer::drums).style = juce::jlimit (0, (int) all.size() - 1, index);
    // A solo: the lead plays the whole solo (its last style).
    if (section == pr7s::DrumSection::solo && ! state.lane (ComposerLayer::lead).locked)
    {
        taken[(size_t) ComposerLayer::lead].reset();
        state.lane (ComposerLayer::lead).on = true;
        state.lane (ComposerLayer::lead).style = pr7s::composerStyleCount (ComposerLayer::lead) - 1;
    }
    // (0.94.0) A type this section doesn't have: STANDARD (a block's own type is put on by targetSection after).
    if (state.sectionType.isNotEmpty() && ! pr7s::isSectionTypeOf (typeSectionName(), state.sectionType))
        pr7s::applySectionType (state, typeSectionName(), {});
    refresh();
}

bool ComposerPage::usesSectionsRow() const
{
    return ! studio.getSongSections().blocks.empty();
}

void ComposerPage::aimAtSectionsRow()
{
    if (! usesSectionsRow() || getTargetPlace().has_value())
        return;
    const auto& blocks = studio.getSongSections().blocks;
    for (const auto& b : blocks)
        if (b.recipe.isEmpty())
        {
            targetSection (b.id);
            return;
        }
    targetSection (blocks.front().id);
}

void ComposerPage::addNextSection (const pr7s::SectionSuggestion& next)
{
    // A block of that kind after the last (a name already there: a linked copy of it), then composed.
    const auto index = studio.appendSongSection (pr7s::sectionWord (next.kind), 8);
    if (index < 0)
        return;
    chooseNext (next);
    targetSection (studio.getSongSections().blocks[(size_t) index].id);
}

std::vector<pr7s::SectionSuggestion> ComposerPage::getNextSuggestions() const
{
    if (usesSectionsRow())
    {
        // (0.93.0) What the SECTIONS row holds so far, as the sections of the song.
        std::vector<pr7s::BuiltSection> song;
        const auto places = studio.getSongSectionPlaces();
        const auto& blocks = studio.getSongSections().blocks;
        for (size_t i = 0; i < blocks.size(); ++i)
        {
            pr7s::BuiltSection b;
            b.kind = pr7s::drumSections()[(size_t) pr7s::composerSectionFor (blocks[i].name)];
            b.start = places[i].start;
            b.end = places[i].end;
            b.riff = state.lane (ComposerLayer::guitar).style;
            b.riffSettings = studio.getRiffSettings();
            b.variation = state.lane (ComposerLayer::drums).variation;
            song.push_back (b);
        }
        return pr7s::suggestNextSections (song, studio.getGuitarStyle(), pr7s::composerStyleCount (ComposerLayer::guitar));
    }
    // (0.75.0) With your riff, it's one of the riffs: the same one comes back, a new one is the next round.
    return pr7s::suggestNextSections (studio.getBuiltSections(), studio.getGuitarStyle(),
                                      pr7s::composerStyleCount (ComposerLayer::guitar) + (studio.getComposerRiff().has_value() ? 1 : 0));
}

void ComposerPage::chooseNext (const pr7s::SectionSuggestion& next)
{
    // What it relates to (engine/SectionPlan.h): the riff settings, the guitar's riff, the same family of beats.
    studio.setRiffSettings (next.riffSettings);
    auto& guitar = state.lane (ComposerLayer::guitar);
    // (0.114.0: a lane holding your riff is locked on it; a suggestion can still move off it or back)
    const auto riffOnGuitar = useMyRiff && state.riffLayer() == ComposerLayer::guitar;
    if ((! guitar.locked || riffOnGuitar) && ! taken[(size_t) ComposerLayer::guitar].has_value())
    {
        // (0.75.0) Your riff, or a new riff for contrast.
        useMyRiff = studio.getComposerRiff().has_value() && next.riff == myRiffIndex;
        if (! useMyRiff)
        {
            guitar.style = juce::jlimit (0, pr7s::composerStyleCount (ComposerLayer::guitar) - 1, next.riff);
            if (riffOnGuitar)
                guitar.locked = false;
        }
    }
    state.lane (ComposerLayer::drums).variation = next.variation;
    status = "Next: " + pr7s::sectionWord (next.kind).toLowerCase() + ". " + next.reason;
    setSection (next.kind);
}

std::vector<int> ComposerPage::buildSection()
{
    stop();
    if (spans.empty())
    {
        status = state.fromChordTrack ? "The chord track has no chords" : "No progression fits the key, genre and mood";
        repaint();
        return {};
    }
    const auto section = getSection();
    const auto placed = spansAt (buildStart());
    auto lanesPlaced = placeLanes (placed.front().start, false);   // (0.112.0: a lane on each of its tracks)
    const auto& list = lanesPlaced.ideas;
    const auto& onto = lanesPlaced.onto;
    const auto& layersBuilt = lanesPlaced.layers;   // (0.92.0) which lane each of `list` is
    if (list.empty())
    {
        status = "Nothing to build: switch a layer on";
        repaint();
        return {};
    }
    writeChordsAt (placed);
    const auto tracks = studio.buildSection (list, placed, pr7s::sectionWord (section), onto);
    if (tracks.empty())
    {
        status = "Nothing written";
        repaint();
        return {};
    }
    // (0.92.0) Into a block of the SECTIONS row: the block keeps how it was built (its chords changing later rebuilds it).
    if (getTargetPlace().has_value() && tracks.size() == layersBuilt.size())
    {
        std::vector<std::pair<int, int>> layerTracks;
        for (size_t i = 0; i < tracks.size(); ++i)
            layerTracks.emplace_back (layersBuilt[i], tracks[i]);
        studio.noteSectionBuilt (targetBlock, state.toString(), layerTracks);
    }
    studio.noteBuiltSection ({ section, placed.front().start, placed.back().end, isPlayingMyRiff() ? myRiffIndex : state.lane (ComposerLayer::guitar).style,
                               studio.getRiffSettings(), state.lane (ComposerLayer::drums).variation });
    const auto& sig = studio.getProject().signatures();
    const auto bar = sig.barAt (placed.front().start) + 1, bars = sig.barAt (placed.back().end) + 1 - bar;   // (barAt: a hair before a line is on it)
    const auto built = pr7s::sectionWord (section) + " built at bar " + juce::String (bar) + " (" + juce::String (bars) + " bars)";
    // The best next section chosen for you (change it with SECTION or the NEXT keys).
    if (const auto target = getTargetPlace())
    {
        const auto& blocks = studio.getSongSections().blocks;
        const auto index = studio.getSongSections().indexOf (targetBlock);
        const auto done = pr7s::sectionLabel (blocks, index) + " built";
        // (0.93.0) On to the next block not built yet.
        auto next = -1;
        for (int i = index + 1; i < (int) blocks.size() && next < 0; ++i)
            if (blocks[(size_t) i].recipe.isEmpty())
                next = i;
        if (next >= 0)
        {
            targetSection (blocks[(size_t) next].id);
            status = done + ". Next: " + pr7s::sectionLabel (blocks, next);
        }
        else
            status = done + ". The blocks after it are built: + ADD for more";
    }
    else if (const auto next = getNextSuggestions(); ! next.empty())
    {
        chooseNext (next.front());
        status = built + ".  Next: " + pr7s::sectionWord (next.front().kind).toLowerCase() + " (" + next.front().reason + ")";
    }
    else
        status = built + ". The song has its ending.";
    if (const auto tones = studio.getLastTones(); ! tones.isEmpty())   // (0.204.0)
        status += "  " + tones.joinIntoString (", ");
    refresh (false);
    return tracks;
}

void ComposerPage::forgetSong()
{
    studio.clearBuiltSections();
    status = "The song's sections forgotten (its parts stay): the next BUILD goes at the playhead's bar";
    refresh (false);
}

std::optional<ComposerLayer> ComposerPage::layerOf (pr7s::LineIdea::Part part)
{
    using P = pr7s::LineIdea::Part;
    switch (part)
    {
        case P::chords: case P::pattern: return ComposerLayer::chords;
        case P::melody: case P::tune: return ComposerLayer::melody;
        case P::bass: return ComposerLayer::bass;
        case P::rhythm: return ComposerLayer::guitar;
        case P::lead: return ComposerLayer::lead;
        case P::clean: return ComposerLayer::clean;
        case P::acoustic: return ComposerLayer::acoustic;
        case P::drums: case P::groove: return ComposerLayer::drums;
        default: return std::nullopt;
    }
}

void ComposerPage::takeIdea (const pr7s::LineIdea& idea)
{
    const auto l = layerOf (idea.part);
    if (! l.has_value())
        return;
    taken[(size_t) *l] = idea;
    state.lane (*l).on = true;
    state.lane (*l).locked = true;
    status = pr7s::composerLayerName (*l) + ": \"" + idea.style + "\" from IDEAS (locked; ROLL or the style menu gives the lane its own again)";
    refresh();
}

void ComposerPage::refreshSong()
{
    nextShown = getNextSuggestions();
    if (nextShown.size() > 3)
        nextShown.resize (3);
    nextKeys.clear();
    for (size_t i = 0; i < nextShown.size(); ++i)
    {
        // Which riff: yours, the last section's, or a new one (two of the same section are told apart).
        const auto& built = studio.getBuiltSections();
        const auto riff = nextShown[i].riff;
        const auto which = studio.getComposerRiff().has_value() && riff == myRiffIndex ? juce::String (" (MY RIFF)")
                         : ! built.empty() && riff == built.back().riff ? juce::String (" (SAME RIFF)")
                         : ! built.empty() ? juce::String (" (NEW RIFF)") : juce::String();
        const auto row = usesSectionsRow();
        auto* key = nextKeys.add (new juce::TextButton ((row ? "+ ADD: " : "NEXT: ") + pr7s::sectionWord (nextShown[i].kind).toUpperCase() + (row ? juce::String() : which)));
        pedal (*key);
        key->setTooltip (row ? "Add a " + pr7s::sectionWord (nextShown[i].kind).toLowerCase() + " after the last block and compose it: " + nextShown[i].reason
                             : nextShown[i].reason);
        key->setToggleState (! row && nextShown[i].kind == getSection() && i == 0, juce::dontSendNotification);
        if (row)
            key->onClick = [this, next = nextShown[i]] { addNextSection (next); };
        else
            key->onClick = [this, next = nextShown[i]] { chooseNext (next); };
        addAndMakeVisible (key);
    }
    resized();
}

juce::Button* ComposerPage::keyNamed (const juce::String& name)
{
    for (auto* key : { &hearKey, &useKey, &generateKey, &libraryKey, &chordTrackKey, &toChordTrackKey, &buildKey, &myRiffKey, &pasteKey, &mixerKey })
        if (key->getButtonText() == name)
            return key;
    if (name == "SYNC")
        return &syncKey;
    if (name == "ROLL CHORDS")
        return &rollChordsKey;
    for (const auto l : pr7s::composerLayers())
    {
        auto& lane = lanes[(size_t) l];
        const auto prefix = pr7s::composerLayerName (l) + " ";
        if (name == prefix + "ON") return &lane.on;
        if (name == prefix + "LOCK") return &lane.lock;
        if (name == prefix + "ROLL") return &lane.roll;
        if (name == prefix + "BARS") return &lane.bars;
        if (name == prefix + "STYLE") return &lane.style;
        if (name == prefix + "TRACK") return &lane.track;
        if (name == prefix + "BUSY") return &lane.busy;
        if (name == prefix + "LOW") return &lane.low;
    }
    return nullptr;
}

//==============================================================================
juce::PopupMenu ComposerPage::makeStyleMenu (ComposerLayer l)
{
    // (0.173.0: the lane's choices were worked out here and never used: laneOptions makes them)
    juce::PopupMenu menu;
    // (0.97.1) A lane the section type writes: its options are the type's approaches, each a different part.
    const auto typeWrites = pr7s::sectionTypeWritesLane (l, state, context);
    // (0.219.0) The lane's WORLD first: the section's own, or another genre's parts.
    const auto inWorld = hasWorlds (l) && state.lane (l).world != 0;
    if (hasWorlds (l))
    {
        menu.addSectionHeader ("WORLD");
        for (int w = 0; w < pr7s::laneWorldNames().size(); ++w)
            menu.addItem (worldItems + w, pr7s::laneWorldNames()[w], true, state.lane (l).world == w);
        menu.addSeparator();
    }
    // (0.156.0, the owner's mock-up D) The short list: Automatic, the type's own options, favourites and recent;
    // everything else behind MORE OPTIONS... (the option browser, with the feel filters).
    const auto options = laneOptions (l, false);   // (0.179.0: the grooves' names: none of their parts is shown here)
    const auto memory = studio.getOptionMemory();
    const auto key = optionsKeyFor (l);
    const auto showingGrooves = showsLibraryGrooves (l);
    std::vector<pr7s::LibraryGroove> pool;
    if (showingGrooves)
        pool = sectionGrooves();
    const auto itemOf = [] (int id) { return id >= grooveId ? libraryItems + 200 + (id - grooveId)
                                           : id >= approachId ? approachItems + 1 + (id - approachId) : id + 1; };
    const auto isOn = [this, l, &pool] (int id)
    {
        if (id >= grooveId)
            return juce::isPositiveAndBelow (id - grooveId, (int) pool.size())
                && state.libraryGroove == pool[(size_t) (id - grooveId)].file.getFullPathName();
        return id >= approachId ? state.lane (l).approach == id - approachId
                                : state.lane (l).approach < 0 && id == state.lane (l).style;
    };
    // (0.160.0, the owner: a long description made the menu as wide as the screen and its submenus open off to the side) names only: what
    // each does is in the browser (MORE OPTIONS...)
    const auto nameOf = [] (const OptionBrowser::Option& o) { return o.name; };
    std::map<juce::String, int> groupSizes;
    for (const auto& o : options)
        ++groupSizes[o.group];
    juce::StringArray shownNames;
    const auto addGroup = [&] (const juce::String& group, const juce::String& heading)
    {
        if (groupSizes[group] == 0 || groupSizes[group] > shortListMost)
            return;
        menu.addSectionHeader (heading);
        for (const auto& o : options)
            if (o.group == group)
            {
                menu.addItem (itemOf (o.id), nameOf (o), true, isOn (o.id));
                shownNames.add (o.name);
            }
    };
    const auto approaches = pr7s::sectionApproaches (state, l);
    if (showingGrooves)
    {
        // (0.177.0) The library's grooves are the drums' options: the section's list behind MORE OPTIONS.
        menu.addSectionHeader ("LIBRARY GROOVES FOR THIS SECTION");
        menu.addItem (libraryItems + 1, "Automatic (ROLL goes through all " + juce::String ((int) pool.size()) + ")", true, state.libraryGroove.isEmpty());
    }
    else if (! approaches.isEmpty())
    {
        menu.addSectionHeader (inWorld ? pr7s::laneWorldNames()[state.lane (l).world]
                                       : state.sectionType.isNotEmpty() ? state.sectionType : juce::String ("DRONING"));
        menu.addItem (approachItems, typeWrites ? "Automatic (the take chooses)" : "Automatic (the style and the take choose)", true, state.lane (l).approach < 0);
    }
    addGroup (typeOwnGroup, inWorld ? juce::String ("ITS PARTS") : juce::String ("THIS TYPE'S OWN"));
    addGroup (stylesGroup, "STYLES");
    // Favourites, then the recent, of what isn't shown already.
    {
        bool headed = false;
        const auto addNamed = [&] (const juce::StringArray& names, bool star)
        {
            for (const auto& n : names)
                for (const auto& o : options)
                    if (o.name == n && ! shownNames.contains (n))
                    {
                        if (! headed)
                            menu.addSectionHeader (juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 FAVOURITES & RECENT")));
                        headed = true;
                        menu.addItem (itemOf (o.id), (star ? juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 ")) : juce::String()) + o.name
                                                         + (star ? juce::String() : juce::String ("  (recent)")), true, isOn (o.id));
                        shownNames.add (n);
                        break;
                    }
        };
        addNamed (memory.favouritesOf (key), true);
        addNamed (memory.recentOf (key), false);
    }
    if (! options.empty())
    {
        menu.addSeparator();
        menu.addItem (moreOptionsItem, (showingGrooves ? "MORE OPTIONS... (all " + juce::String ((int) options.size()) + " grooves, each to hear, with search and feel filters)"
                                                : "MORE OPTIONS... (all " + juce::String ((int) options.size()) + ", with search and feel filters)"));
    }
    // (0.126.0) A lane the section type writes can play its own styles instead, and come back.
    if (typeWrites && ! inWorld)
    {
        menu.addSeparator();
        menu.addItem (ownStylesItem, "Play Its Own Styles (not the section type's part)");
    }
    else if (state.lane (l).ownStyles && typeWouldWrite (l))
    {
        menu.addSeparator();
        menu.addItem (ownStylesItem, "Follow the Section Type Again (" + state.sectionType + ")");
    }
    // (0.70.0, the owner asked) The lead's last choice is the whole solo: its SOLO STYLE chosen here too (the
    // same setting as IDEAS' SOLO box).
    if (l == ComposerLayer::lead)
    {
        // (0.157.0) The short list: the one playing, favourites and recent; the rest in the browser.
        juce::PopupMenu solos;
        const auto now = studio.getSoloStyle();
        const auto memory = studio.getOptionMemory();
        juce::StringArray named { pr7s::soloStyleName (now) };
        for (const auto& n : memory.favouritesOf (soloListKey))
            named.addIfNotAlreadyThere (n);
        for (const auto& n : memory.recentOf (soloListKey))
            named.addIfNotAlreadyThere (n);
        for (const auto& n : named)
            for (int i = 0; i < (int) pr7s::soloStyles().size(); ++i)
            {
                const auto st = pr7s::soloStyles()[(size_t) i];
                if (pr7s::soloStyleName (st) == n)
                    solos.addItem (soloItems + i, (memory.isFavourite (soloListKey, n) ? juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 ")) : juce::String()) + n, true, st == now);
            }
        solos.addSeparator();
        solos.addItem (moreSolosItem, "MORE SOLO STYLES... (all " + juce::String ((int) pr7s::soloStyles().size()) + ", with search and feel filters)");
        menu.addSeparator();
        menu.addSubMenu ("Solo Style: " + pr7s::soloStyleName (now), solos);
    }
    if (l == ComposerLayer::guitar)
    {
        // (0.85.0) The riff generator's flavour: every ROLL / GENERATE another riff of it.
        juce::PopupMenu flavours;
        for (int i = 0; i < (int) pr7s::riffFlavours().size(); ++i)
        {
            const auto f = pr7s::riffFlavours()[(size_t) i];
            flavours.addItem (flavourItems + i, pr7s::riffFlavourName (f), true,
                              ! state.writtenRiffs && i == state.riffFlavour);
        }
        menu.addSeparator();
        if (pr7s::sectionBand (state) == pr7s::SectionType::Band::clean || pr7s::sectionBand (state) == pr7s::SectionType::Band::preChorus
            || pr7s::sectionBand (state) == pr7s::SectionType::Band::bridge
            || pr7s::sectionBand (state) == pr7s::SectionType::Band::breakdown
            || pr7s::sectionBand (state) == pr7s::SectionType::Band::interlude
            || pr7s::sectionBand (state) == pr7s::SectionType::Band::bookend
            || pr7s::sectionBand (state) == pr7s::SectionType::Band::chorus
            || pr7s::sectionBand (state) == pr7s::SectionType::Band::verse)   // (0.97.0: the clean parts aren't riffs; 0.102.0: nor the pre-chorus's)
            menu.addSubMenu ("Riff Flavour: (the type writes its own guitar part)", flavours, false);
        else if (const auto typed = pr7s::typedGuitarFlavour (state))   // (0.94.0: the type's approach picks the riff kind)
            menu.addSubMenu ("Riff Flavour: " + pr7s::riffFlavourName (*typed) + " (the type's approach)", flavours, false);
        else
            menu.addSubMenu ("Riff Flavour: " + (state.writtenRiffs ? juce::String ("(written riffs)")
                                                                    : pr7s::riffFlavourName (pr7s::riffFlavours()[(size_t) state.riffFlavour])), flavours);
        menu.addItem (writtenRiffsItem, "Written Riffs (the six of each style, not generated)", true, state.writtenRiffs);
    }
    if (studio.getComposerRiff().has_value())   // (0.114.0: on any lane: your riff moves to it)
    {
        menu.addSeparator();
        menu.addItem (myRiffItem, "My Riff (" + studio.getComposerRiff()->name + ")", true, useMyRiff && l == state.riffLayer());
    }
    // (0.106.0, the owner) The drums from the groove library: on by default; the grooves for this section to choose from.
    if (l == ComposerLayer::drums)
    {
        menu.addSeparator();
        menu.addItem (libraryItems, "Grooves from My Library (PR7-D's, else the MediaBay's)", true, state.libraryDrums);
        // (0.108.0, the studio link) PR7-D plays the drums from its own song track (the track keeps a muted copy).
        menu.addItem (libraryItems + 90, "PR7-D Plays the Drums from Its Song Track (on a track playing PR7-D)", true, studio.getPr7dPlaysDrums());
        // (0.137.0, the owner's rules) The section's 100: two thirds from the Toontrack folder, 4 to 16 bars, naming
        // the section, best fit to the guitar first; ROLL goes through them all in a random order.
        menu.addItem (libraryItems + 91, "Fit the Kick to the Riff (kicks nudged onto its hits)", state.libraryDrums, state.fitKicks);
        const auto grooves = sectionGrooves();   // (0.177.0: the same list as the options)
        auto fromToontrack = 0, fromFavourites = 0;
        for (const auto& g : grooves)
        {
            fromFavourites += g.favourite ? 1 : 0;
            fromToontrack += g.toontrack && ! g.favourite ? 1 : 0;
        }
        juce::PopupMenu list;
        list.addItem (libraryItems + 1, "Automatic (ROLL goes through all " + juce::String ((int) grooves.size()) + " in a random order)", state.libraryDrums,
                      state.libraryGroove.isEmpty());
        if (! grooves.empty())
            list.addSectionHeader (juce::String ((int) grooves.size()) + " for this section: " + juce::String (fromFavourites) + " from your favourite folders, "
                                   + juce::String (fromToontrack) + " Toontrack, " + juce::String ((int) grooves.size() - fromFavourites - fromToontrack)
                                   + " other packs (best fit to the riff first)");
        for (int i = 0; i < (int) grooves.size(); ++i)
            list.addItem (libraryItems + 200 + i, grooves[(size_t) i].name + "  (" + grooves[(size_t) i].folder + ", " + juce::String (grooves[(size_t) i].bars)
                                                      + " bars)", state.libraryDrums, state.libraryGroove == grooves[(size_t) i].file.getFullPathName());
        const auto playing = pr7s::composerLibraryGroove (state, placedSpans(), context);
        menu.addSubMenu ("Library Groove: " + (grooves.empty() ? juce::String ("(none found: scan your grooves in PR7-D or the MediaBay)")
                                                               : playing.has_value() ? playing->name : juce::String ("(off)")),
                         list, ! grooves.empty());
    }
    // (0.126.0, the owner) SYNC TO: this lane alone follows another lane's rhythm.
    {
        juce::PopupMenu to;
        const auto own = state.lane (l).syncTo;
        to.addItem (syncToItems, "Not Synced on Its Own" + juce::String (state.syncLayer().has_value() ? " (the SYNC key's lane leads)" : ""),
                    true, own < 0 || own == (int) l);
        for (const auto other : pr7s::composerLayers())
            if (other != l)
                to.addItem (syncToItems + 1 + (int) other, "Sync to the " + pr7s::composerLayerName (other), true, own == (int) other);
        menu.addSeparator();
        menu.addSubMenu ("Sync To: " + (state.followsOwnLeader (l) ? pr7s::composerLayerName ((ComposerLayer) own) : juce::String ("(not on its own)")), to);
    }
    // (0.86.0) The bass and the drums under SYNC: every hit of the sync lane.
    if (pr7s::hasDoubleSync (l))
    {
        const auto sync = state.leaderOf (l);
        menu.addSeparator();
        menu.addItem (doubleSyncItem, sync.has_value() && *sync != l ? "Double the Sync Track (the " + pr7s::composerLayerName (*sync).toLowerCase() + ", every hit)"
                                                                      : juce::String ("Double the Sync Track (with SYNC on)"),
                      sync.has_value() && *sync != l, state.lane (l).doubleSync);
    }
    return menu;
}

void ComposerPage::showStyleMenu (ComposerLayer l)
{
    makeStyleMenu (l).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (lanes[(size_t) l].style),
                                     [safe = juce::Component::SafePointer<ComposerPage> (this), l] (int chosen)
                                     {
                                         if (safe != nullptr && chosen > 0)
                                             safe->styleMenuChosen (l, chosen);
                                     });
}

void ComposerPage::styleMenuChosen (ComposerLayer l, int chosen)
{
    if (chosen <= 0)
        return;
    if (chosen == moreOptionsItem)
    {
        showOptionBrowser (l);
        return;
    }
    if (chosen == moreSolosItem)
    {
        auto browser = makeSoloBrowser();
        if (getPeer() != nullptr || getTopLevelComponent()->getPeer() != nullptr)
            openBrowserWindow (std::move (browser));
        return;
    }
    if (chosen >= libraryItems && chosen < libraryItems + 400)
    {
        if (chosen == libraryItems)
            setLibraryDrums (! isUsingLibraryDrums());
        else if (chosen == libraryItems + 90)
            studio.setPr7dPlaysDrums (! studio.getPr7dPlaysDrums());
        else if (chosen == libraryItems + 91)
            setFitKicks (! state.fitKicks);
        else if (chosen == libraryItems + 1)
            setLibraryGroove ({});
        else if (chosen >= libraryItems + 200)
            useOption (l, grooveId + chosen - libraryItems - 200);
    }
    else if (chosen >= syncToItems && chosen < syncToItems + 20)
        setSyncTo (l, chosen == syncToItems ? std::optional<ComposerLayer> {}
                                                  : std::optional<ComposerLayer> ((ComposerLayer) (chosen - syncToItems - 1)));
    else if (chosen == ownStylesItem)
        setOwnStyles (l, ! isPlayingOwnStyles (l));
    else if (chosen >= worldItems && chosen < worldItems + 10)
        setWorld (l, chosen - worldItems);
    else if (chosen == doubleSyncItem)
        setDoubleSync (l, ! isDoublingSync (l));
    else if (chosen == myRiffItem)
        setRiffLane (l);
    else if (chosen == writtenRiffsItem)
        setWrittenRiffs (! isUsingWrittenRiffs());
    else if (chosen == approachItems)
        setApproach (l, -1);
    else if (chosen > approachItems && chosen < approachItems + 100)
        useOption (l, approachId + chosen - approachItems - 1);
    else if (chosen >= flavourItems && chosen < writtenRiffsItem)
        setRiffFlavour (chosen - flavourItems);
    else if (chosen >= soloItems)
        useSoloStyle (pr7s::soloStyles()[(size_t) juce::jlimit (0, (int) pr7s::soloStyles().size() - 1, chosen - soloItems)]);
    else
        useOption (l, chosen - 1);
}

bool ComposerPage::showsLibraryGrooves (ComposerLayer l) const
{
    // (when SYNC makes another lane lead, the groove's kicks follow that lane, but it's still the groove that plays)
    return l == ComposerLayer::drums && state.libraryDrums && ! sectionGrooves().empty();
}

std::vector<pr7s::LibraryGroove> ComposerPage::sectionGrooves() const
{
    auto s = state;
    s.libraryGroove = {};   // (the list is the section's, whichever one is chosen)
    return pr7s::composerGroovePool (s, placedSpans(), context);
}

std::vector<OptionBrowser::Option> ComposerPage::libraryGrooveOptions (bool withParts) const
{
    const auto grooves = sectionGrooves();
    auto s = state;
    s.libraryGroove = {};
    s.libraryPick = {};
    s.groovesHeard = {};
    juce::String key = s.toString() + "|" + juce::String (context.beatsPerBar) + "|" + juce::String (context.bpm);
    for (const auto& sp : spans)
        key << juce::String (sp.start, 6) << "," << juce::String (sp.end, 6) << "," << sp.chord.toText() << ";";
    for (const auto& g : grooves)
        key << g.file.getFullPathName().hashCode64() << (g.favourite ? "f," : ",");
    if (key != grooveOptionsKey)
    {
        // (0.179.0) The names first (instant); each part made when it's asked for, then kept with the list.
        grooveOptionsKey = key;
        grooveOptionsKept.clear();
        grooveFilesKept.clear();
        grooveOptionsState = s;
        for (int i = 0; i < (int) grooves.size(); ++i)
        {
            const auto& g = grooves[(size_t) i];
            OptionBrowser::Option o;
            o.id = grooveId + i;
            // (names repeat across packs ("Verse 1"): the folder makes each one its own, for favourites too)
            o.name = g.name + " (" + g.folder + ")";
            o.description = g.folder + ", " + juce::String (g.bars) + (g.bars == 1 ? " bar" : " bars")
                          + (g.bpm > 0.0 ? ", " + juce::String (juce::roundToInt (g.bpm)) + " BPM" : juce::String())
                          + "  -  " + g.file.getFullPathName();
            o.group = g.favourite ? favouriteFoldersGroup : g.toontrack ? toontrackGroup : otherPacksGroup;
            o.drums = true;
            o.pending = true;
            grooveFilesKept.push_back (g.file.getFullPathName());
            grooveOptionsKept.push_back (std::move (o));
        }
    }
    if (withParts)
        for (auto& o : grooveOptionsKept)
            fillGrooveOption (o);
    return grooveOptionsKept;
}

bool ComposerPage::fillGrooveOption (OptionBrowser::Option& o) const
{
    const auto i = o.id - grooveId;
    if (! juce::isPositiveAndBelow (i, (int) grooveOptionsKept.size()))
        return false;
    auto& kept = grooveOptionsKept[(size_t) i];
    if (kept.pending)
    {
        auto each = grooveOptionsState;
        each.libraryGroove = grooveFilesKept[(size_t) i];
        kept.idea = pr7s::composeLayer (ComposerLayer::drums, each, spans, context);
        kept.measure = pr7s::measureOption (kept.idea, context.beatsPerBar, true);
        kept.pending = false;
    }
    if (&o != &kept)
    {
        o.idea = kept.idea;
        o.measure = kept.measure;
        o.pending = false;
    }
    return true;
}

std::vector<OptionBrowser::Option> ComposerPage::laneOptions (ComposerLayer l, bool withParts) const
{
    auto out = laneOptionsOwn (l, withParts);
    if (l == ComposerLayer::drums && state.lane (l).hasBarLocks())
    {
        auto fills = fillOptions();
        out.insert (out.begin(), std::make_move_iterator (fills.begin()), std::make_move_iterator (fills.end()));
    }
    return out;
}

std::vector<OptionBrowser::Option> ComposerPage::fillOptions() const
{
    std::vector<OptionBrowser::Option> out;
    if (! state.lane (ComposerLayer::drums).hasBarLocks() || spans.empty())
        return out;
    const auto fills = pr7s::composerFills (state, context, &spans);
    for (int i = 0; i < (int) fills.size(); ++i)
    {
        const auto& f = fills[(size_t) i];
        OptionBrowser::Option o;
        o.id = fillId + i;
        o.name = f.name;
        o.description = f.description + " (the bars not locked filled with it; a crash and a kick where it lands)";
        o.group = f.group == pr7s::grooveFillsGroup ? pr7s::grooveFillsGroup : fillsGroup;   // (0.223.0)
        o.drums = true;
        auto each = state;
        each.lane (ComposerLayer::drums).fill = f.id;
        o.idea = pr7s::composeLayer (ComposerLayer::drums, each, spans, context);
        o.measure = pr7s::measureOption (o.idea, context.beatsPerBar, true);
        out.push_back (std::move (o));
    }
    return out;
}

std::vector<OptionBrowser::Option> ComposerPage::laneOptionsOwn (ComposerLayer l, bool withParts) const
{
    if (showsLibraryGrooves (l))
        return libraryGrooveOptions (withParts);
    std::vector<OptionBrowser::Option> out;
    const auto choices = pr7s::composerChoices (l, state, spans, context);
    const auto drums = l == ComposerLayer::drums;
    const auto bpb = context.beatsPerBar;
    const auto approaches = pr7s::sectionApproaches (state, l);
    const auto ownNames = pr7s::baseSectionApproaches (state, l);   // (0.160.0: by name: the bass's shared options sit in front of its own last)
    if (pr7s::sectionTypeWritesLane (l, state, context))
    {
        // The type's options, each a different part (composerChoices made one each, in this order).
        for (int i = 0; i < approaches.size(); ++i)
        {
            OptionBrowser::Option o;
            o.id = approachId + i;
            o.name = approaches[i];
            o.group = ownNames.contains (approaches[i]) ? typeOwnGroup : songsGroup;
            o.drums = drums;
            if (i < (int) choices.size())
            {
                o.idea = choices[(size_t) i];
                o.description = o.idea.description;
                o.measure = pr7s::measureOption (o.idea, bpb, drums);
            }
            out.push_back (std::move (o));
        }
        return out;
    }
    // A type with options that only steer the lane's own styles (the guitar's riff kinds...): those, then the styles.
    for (int i = 0; i < approaches.size(); ++i)
    {
        OptionBrowser::Option o;
        o.id = approachId + i;
        o.name = approaches[i];
        o.group = ownNames.contains (approaches[i]) ? typeOwnGroup : songsGroup;
        o.drums = drums;
        out.push_back (std::move (o));
    }
    for (int i = 0; i < (int) choices.size(); ++i)
    {
        OptionBrowser::Option o;
        o.id = i;
        o.idea = choices[(size_t) i];
        o.name = o.idea.style;
        o.description = o.idea.description;
        o.group = stylesGroup;
        o.drums = drums;
        o.measure = pr7s::measureOption (o.idea, bpb, drums);
        out.push_back (std::move (o));
    }
    return out;
}

std::unique_ptr<OptionBrowser> ComposerPage::makeOptionBrowser (ComposerLayer l)
{
    const auto& lane = state.lane (l);
    auto chosen = lane.approach >= 0 ? approachId + lane.approach : lane.style;
    auto title = pr7s::composerLayerName (l) + (state.sectionType.isNotEmpty() ? juce::String (": ") + state.sectionType : juce::String());
    const auto grooves = showsLibraryGrooves (l);
    if (grooves)
    {
        // (0.177.0) The section's grooves: the one playing chosen.
        const auto pool = sectionGrooves();
        chosen = -1;
        if (const auto playing = pr7s::composerLibraryGroove (state, placedSpans(), context))
            for (int i = 0; i < (int) pool.size(); ++i)
                if (pool[(size_t) i].file == playing->file)
                    chosen = grooveId + i;
        title = "DRUMS: " + juce::String ((int) pool.size()) + " LIBRARY GROOVES FOR THIS SECTION";
    }
    // (0.202.0) The drums with bars locked: the FILLS first, the one chosen chosen.
    const auto withFills = l == ComposerLayer::drums && lane.hasBarLocks();
    std::vector<pr7s::ComposerFill> fills;
    if (withFills)
    {
        fills = pr7s::composerFills (state, context, &spans);
        // (0.203.0) The fill of the first open bar chosen (else the first open bar).
        const auto now = pr7s::barFills (lane, barCount());
        juce::String shown;
        for (const auto& [bar, id] : now)
            if (shown.isEmpty() || ((barSelection[(size_t) l] >> juce::jmin (bar, 63)) & 1u) != 0)
            {
                const auto pick = ((barSelection[(size_t) l] >> juce::jmin (bar, 63)) & 1u) != 0;
                shown = id;
                if (pick)
                    break;
            }
        for (int i = 0; i < (int) fills.size(); ++i)
            if (fills[(size_t) i].id == shown)
                chosen = fillId + i;
        title << (openBarsChosen() ? " - FILLS FOR THE OPEN BARS CHOSEN" : " - FILLS FOR THE BARS NOT LOCKED (EACH ITS OWN: ROLL)");
    }
    auto browser = std::make_unique<OptionBrowser> (title, laneOptions (l, false), chosen, studio.getOptionMemory(), optionsKeyFor (l));
    browser->setAutoHear (true);   // (0.180.0, the owner: a highlighted option plays at once, in time with the song)
    if (withFills && ! fills.empty())
    {
        browser->setGroupOrder ({ pr7s::grooveFillsGroup, fillsGroup, favouriteFoldersGroup, toontrackGroup, otherPacksGroup });
        browser->setGroup (std::any_of (fills.begin(), fills.end(), [] (const pr7s::ComposerFill& f) { return f.group == pr7s::grooveFillsGroup; })
                               ? pr7s::grooveFillsGroup : fillsGroup);
    }
    if (grooves)
    {
        if (! withFills)
            browser->setGroupOrder ({ favouriteFoldersGroup, toontrackGroup, otherPacksGroup });
        // (0.179.0) The list at once; the grooves' parts made a few a frame (the chosen first, then those in view).
        juce::Component::SafePointer<ComposerPage> filler (this);
        browser->onFill = [filler] (OptionBrowser::Option& o) { return filler != nullptr && filler->fillGrooveOption (o); };
        // (0.178.0) Right-click a groove: its folder marked (or unmarked) as a favourite, the list made again.
        juce::Component::SafePointer<ComposerPage> page (this);
        auto* shown = browser.get();
        browser->onRightClick = [page, shown] (int id, juce::Point<int> at)
        {
            if (page == nullptr)
                return;
            juce::PopupMenu menu;
            const auto folder = page->grooveFolderOf (id);
            const auto marked = page->favouriteFolderHolding (id);
            if (marked != juce::File())
                menu.addItem (2, "Unmark the Favourite Folder \"" + marked.getFileName() + "\"");
            else if (folder != juce::File())
                menu.addItem (1, "Mark Its Folder as a Favourite (\"" + folder.getFileName() + "\")");
            if (menu.getNumItems() == 0)
                return;
            juce::Component::SafePointer<OptionBrowser> browserAlive (shown);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ at.x, at.y, 1, 1 }),
                                [page, browserAlive, id] (int chosen)
                                {
                                    if (page == nullptr || chosen == 0)
                                        return;
                                    page->markGrooveFolder (id, chosen == 1, browserAlive.getComponent());
                                });
        };
    }
    juce::Component::SafePointer<ComposerPage> safe (this);
    browser->onHear = [safe, l] (int id) { if (safe != nullptr) safe->tryOption (l, id); };
    browser->onUse = [safe, l] (int id)
    {
        if (safe == nullptr)
            return;
        safe->endTrial (true);
        safe->useOption (l, id);
    };
    browser->onFavourite = [safe, l] (const juce::String& name, bool on)
    {
        if (safe != nullptr)
            safe->studio.setOptionFavourite (safe->optionsKeyFor (l), name, on);
    };
    browser->onClosed = [safe] { if (safe != nullptr) safe->endTrial (false); };
    return browser;
}

void ComposerPage::showOptionBrowser (ComposerLayer l)
{
    auto browser = makeOptionBrowser (l);
    if (getPeer() == nullptr && getTopLevelComponent()->getPeer() == nullptr)
        return;
    openBrowserWindow (std::move (browser));
}

void ComposerPage::openBrowserWindow (std::unique_ptr<OptionBrowser> browser)
{
    browserWindow.reset();   // (the old one's trial ends as it goes)
    browserWindow = std::make_unique<OptionBrowserWindow> (std::move (browser), this);
    browserWindow->onClose = [safe = juce::Component::SafePointer<ComposerPage> (this)]
    {
        juce::MessageManager::callAsync ([safe]
        {
            if (safe != nullptr)
                safe->browserWindow.reset();
        });
    };
}

juce::File ComposerPage::grooveFolderOf (int id) const
{
    const auto pool = sectionGrooves();
    const auto i = id - grooveId;
    return juce::isPositiveAndBelow (i, (int) pool.size()) ? pool[(size_t) i].file.getParentDirectory() : juce::File();
}

juce::File ComposerPage::favouriteFolderHolding (int id) const
{
    const auto pool = sectionGrooves();
    const auto i = id - grooveId;
    if (! juce::isPositiveAndBelow (i, (int) pool.size()))
        return {};
    for (const auto& f : studio.getGrooveFavouriteFolders())
        if (pr7s::isInFavouriteFolder (pool[(size_t) i].file, { f }))
            return juce::File (f);
    return {};
}

void ComposerPage::markGrooveFolder (int id, bool favourite, OptionBrowser* browser)
{
    const auto pool = sectionGrooves();
    const auto i = id - grooveId;
    if (! juce::isPositiveAndBelow (i, (int) pool.size()))
        return;
    const auto file = pool[(size_t) i].file;
    if (favourite)
        studio.setGrooveFolderFavourite (file.getParentDirectory(), true);
    else
        for (const auto& f : studio.getGrooveFavouriteFolders())
            if (pr7s::isInFavouriteFolder (file, { f }))
                studio.setGrooveFolderFavourite (juce::File (f), false);
    refresh();
    status = favourite ? "Favourite folder: " + file.getParentDirectory().getFileName() : juce::String ("Favourite folder unmarked");
    if (browser != nullptr)
    {
        // The list again (the groups changed), the same groove still chosen.
        browser->setOptions (laneOptions (ComposerLayer::drums, false));
        const auto again = sectionGrooves();
        for (int k = 0; k < (int) again.size(); ++k)
            if (again[(size_t) k].file == file)
                browser->choose (grooveId + k);
    }
}

void ComposerPage::useOption (ComposerLayer l, int id)
{
    if (id >= fillId)
    {
        // (0.202.0) A fill for the bars not locked: kept on the drums.
        const auto fills = pr7s::composerFills (state, context, &spans);
        const auto i = id - fillId;
        if (! juce::isPositiveAndBelow (i, (int) fills.size()))
            return;
        studio.noteOptionUsed (optionsKeyFor (l), fills[(size_t) i].name);
        // (0.203.0) Onto the open bars chosen on the lane, else onto every open bar.
        pr7s::setFillOnBars (state.lane (ComposerLayer::drums), fills[(size_t) i].id, barSelection[(size_t) ComposerLayer::drums], barCount());
        taken[(size_t) ComposerLayer::drums].reset();
        status = "Drums: " + fills[(size_t) i].name + (openBarsChosen() ? " in the open bars chosen" : " in every bar not locked");
        refresh();
        return;
    }
    if (id >= grooveId)
    {
        // (0.177.0) A library groove: kept on this section's drums.
        const auto pool = sectionGrooves();
        const auto i = id - grooveId;
        if (! juce::isPositiveAndBelow (i, (int) pool.size()))
            return;
        const auto& g = pool[(size_t) i];
        studio.noteOptionUsed (optionsKeyFor (l), g.name + " (" + g.folder + ")");
        setLibraryGroove (g.file.getFullPathName());
        status = "Drums: " + g.name + " (" + g.folder + ")";
        return;
    }
    const auto options = laneOptions (l);
    for (const auto& o : options)
        if (o.id == id)
            studio.noteOptionUsed (optionsKeyFor (l), o.name);
    if (id >= approachId)
    {
        setApproach (l, id - approachId);
        return;
    }
    // A style: the type's option let go, so the style plays.
    if (state.lane (l).approach >= 0 && ! pr7s::sectionTypeWritesLane (l, state, context))
        state.lane (l).approach = -1;
    setStyle (l, id);
}

void ComposerPage::beginTrial (const juce::String& list, std::function<void()> restore)
{
    if (trial.active && trial.list == list)
        return;   // (the first thing tried is what goes back)
    endTrial (false);
    trial = { true, list, std::move (restore), hearing };
}

void ComposerPage::tryOption (ComposerLayer l, int id)
{
    if (id >= fillId)
    {
        const auto fills = pr7s::composerFills (state, context, &spans);
        const auto i = id - fillId;
        if (! juce::isPositiveAndBelow (i, (int) fills.size()))
            return;
        const auto before = state.lane (ComposerLayer::drums).fill;
        beginTrial (optionsKeyFor (l), [this, before]
        {
            state.lane (ComposerLayer::drums).fill = before;
            refresh();
        });
        pr7s::setFillOnBars (state.lane (ComposerLayer::drums), fills[(size_t) i].id, barSelection[(size_t) ComposerLayer::drums], barCount());
        taken[(size_t) ComposerLayer::drums].reset();
        refresh();
        if (! hearing)
            hear();
        return;
    }
    if (id >= grooveId)
    {
        // (0.177.0) HEAR a library groove: the one playing before goes back unless USE IT.
        const auto pool = sectionGrooves();
        const auto i = id - grooveId;
        if (! juce::isPositiveAndBelow (i, (int) pool.size()))
            return;
        const auto groove = state.libraryGroove, pick = state.libraryPick;
        const auto fill = state.lane (ComposerLayer::drums).fill;
        beginTrial (optionsKeyFor (l), [this, groove, pick, fill]
        {
            state.libraryPick = pick;
            state.lane (ComposerLayer::drums).fill = fill;
            state.libraryGroove = groove;
            state.libraryDrums = true;
            taken[(size_t) ComposerLayer::drums].reset();
            refresh();
        });
        setLibraryGroove (pool[(size_t) i].file.getFullPathName());
        if (! hearing)
            hear();
        return;
    }
    const auto approach = state.lane (l).approach, style = state.lane (l).style;
    const auto fill = state.lane (l).fill;
    beginTrial (optionListKey (l), [this, l, approach, style, fill]
    {
        state.lane (l).approach = approach;
        taken[(size_t) l].reset();
        setStyle (l, style);
        if (state.lane (l).fill != fill)
        {
            state.lane (l).fill = fill;
            refresh();
        }
    });
    if (id >= approachId)
        setApproach (l, id - approachId);
    else
    {
        if (! pr7s::sectionTypeWritesLane (l, state, context))
            state.lane (l).approach = -1;
        setStyle (l, id);
    }
    if (! hearing)
        hear();
}

void ComposerPage::endTrial (bool keep)
{
    if (! trial.active)
        return;
    const auto was = trial;
    trial = {};
    if (keep)
        return;
    if (was.restore)
        was.restore();
    if (! was.wasHearing)
        stop();
}

//==============================================================================
std::vector<OptionBrowser::Option> ComposerPage::typeOptions() const
{
    std::vector<OptionBrowser::Option> out;
    const auto name = typeSectionName();
    const auto types = pr7s::sectionTypesFor (name);
    // Each type measured by the drums it writes (the library's grooves off: they'd be the same under every type).
    const auto measure = [this, &name] (const juce::String& type, OptionBrowser::Option& o)
    {
        if (spans.empty())
            return;
        auto each = state;
        each.libraryDrums = false;
        each.lane (ComposerLayer::drums).on = true;
        pr7s::applySectionType (each, name, type);
        o.idea = pr7s::composeLayer (ComposerLayer::drums, each, spans, context);   // (0.175.0: the drums alone)
        o.drums = true;
        o.measure = pr7s::measureOption (o.idea, context.beatsPerBar, true);
    };
    {
        OptionBrowser::Option o;
        o.id = 0;
        o.name = pr7s::standardSectionType;
        o.description = "Every lane plays its own styles";
        o.group = "STANDARD";
        measure ({}, o);
        out.push_back (std::move (o));
    }
    for (int i = 0; i < (int) types.size(); ++i)
    {
        OptionBrowser::Option o;
        o.id = i + 1;
        o.name = types[(size_t) i].name;
        o.description = types[(size_t) i].description;
        o.group = pr7s::typeGroupOf (types[(size_t) i]);
        measure (o.name, o);
        out.push_back (std::move (o));
    }
    return out;
}

std::vector<OptionBrowser::Option> ComposerPage::soloOptions() const
{
    std::vector<OptionBrowser::Option> out;
    const auto& profile = context.leadProfile.has_value() ? context.leadProfile : context.guitarProfile;
    const auto tuning = profile.has_value() ? profile->tuning() : context.tuning;
    const auto velocities = profile.has_value() && profile->velocities.has_value() ? *profile->velocities : context.velocities;
    const auto shortest = profile.has_value() ? profile->shortestNote : 0.0;
    for (const auto st : pr7s::soloStyles())
    {
        OptionBrowser::Option o;
        o.id = (int) st;
        o.name = pr7s::soloStyleName (st);
        o.description = pr7s::soloStyleDescription (st);
        o.group = st == pr7s::SoloStyle::automatic ? juce::String ("BY STYLE") : pr7s::soloGroupOf (st);
        if (! spans.empty())
        {
            const auto chosen = st == pr7s::SoloStyle::automatic ? pr7s::soloStyleForGuitarStyle ((int) context.style, 0) : st;
            o.idea = pr7s::composeSolo (chosen, spans, context.beatsPerBar, tuning, velocities, 0, shortest, context.bpm);
            o.measure = pr7s::measureOption (o.idea, context.beatsPerBar, false);
        }
        out.push_back (std::move (o));
    }
    return out;
}

bool ComposerPage::canTryType() const
{
    const auto index = studio.getSongSections().indexOf (targetBlock);
    return index < 0 || ! studio.isSongSectionBuilt (index);
}

void ComposerPage::useType (const juce::String& name)
{
    studio.noteOptionUsed (typeListKey, name.isEmpty() ? juce::String (pr7s::standardSectionType) : name);
    setSectionType (name);
}

void ComposerPage::useSoloStyle (pr7s::SoloStyle st)
{
    studio.noteOptionUsed (soloListKey, pr7s::soloStyleName (st));
    setSoloStyle (st);
}

juce::PopupMenu ComposerPage::makeTypeMenu()
{
    const auto name = typeSectionName();
    const auto types = pr7s::sectionTypesFor (name);
    const auto memory = studio.getOptionMemory();
    juce::PopupMenu menu;
    menu.addSectionHeader ("TYPE OF " + name.toUpperCase());
    menu.addItem (1, juce::String (pr7s::standardSectionType) + "  (every lane its own styles)", true, state.sectionType.isEmpty());
    juce::StringArray shown;
    const auto add = [&] (const juce::String& n, const juce::String& prefix)
    {
        for (int i = 0; i < (int) types.size(); ++i)
            if (types[(size_t) i].name == n && ! shown.contains (n))
            {
                menu.addItem (2 + i, prefix + n, true, n == state.sectionType);
                shown.add (n);
            }
    };
    if (state.sectionType.isNotEmpty())
        add (state.sectionType, {});
    if ((int) types.size() <= shortListMost)
        for (const auto& t : types)
            add (t.name, {});
    else
    {
        bool headed = false;
        for (const auto& n : memory.favouritesOf (typeListKey))
        {
            if (! headed && ! shown.contains (n))
            {
                menu.addSectionHeader (juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 FAVOURITES & RECENT")));
                headed = true;
            }
            add (n, juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 ")));
        }
        for (const auto& n : memory.recentOf (typeListKey))
        {
            if (! headed && ! shown.contains (n))
            {
                menu.addSectionHeader (juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 FAVOURITES & RECENT")));
                headed = true;
            }
            add (n, {});
        }
    }
    menu.addSeparator();
    menu.addItem (moreTypesItem, "MORE TYPES... (all " + juce::String ((int) types.size()) + ", with search and feel filters)");
    return menu;
}

void ComposerPage::typeMenuChosen (int item)
{
    if (item <= 0)
        return;
    if (item == moreTypesItem)
    {
        auto browser = makeTypeBrowser();
        if (getPeer() != nullptr || getTopLevelComponent()->getPeer() != nullptr)
            openBrowserWindow (std::move (browser));
        return;
    }
    const auto types = pr7s::sectionTypesFor (typeSectionName());
    if (item == 1)
        useType ({});
    else if (juce::isPositiveAndBelow (item - 2, (int) types.size()))
        useType (types[(size_t) (item - 2)].name);
}

std::unique_ptr<OptionBrowser> ComposerPage::makeTypeBrowser()
{
    const auto types = pr7s::sectionTypesFor (typeSectionName());
    auto chosen = 0;
    for (int i = 0; i < (int) types.size(); ++i)
        if (types[(size_t) i].name == state.sectionType)
            chosen = i + 1;
    auto browser = std::make_unique<OptionBrowser> ("TYPE OF " + typeSectionName().toUpperCase(), typeOptions(), chosen,
                                                    studio.getOptionMemory(), typeListKey);
    juce::Component::SafePointer<ComposerPage> safe (this);
    const auto nameOf = [types] (int id) { return id <= 0 || id > (int) types.size() ? juce::String() : types[(size_t) id - 1].name; };
    browser->setHearable (canTryType());
    {
        juce::StringArray order { "STANDARD" };
        order.addArray (pr7s::typeGroupOrder());
        browser->setGroupOrder (order);
    }
    browser->onHear = [safe, nameOf] (int id)
    {
        if (safe == nullptr || ! safe->canTryType())
            return;
        const auto was = safe->state.sectionType;
        safe->beginTrial (typeListKey, [safe, was] { if (safe != nullptr) safe->setSectionType (was); });
        safe->setSectionType (nameOf (id));
        if (! safe->hearing)
            safe->hear();
    };
    browser->onUse = [safe, nameOf] (int id)
    {
        if (safe == nullptr)
            return;
        safe->endTrial (true);
        safe->useType (nameOf (id));
    };
    browser->onFavourite = [safe] (const juce::String& name, bool on) { if (safe != nullptr) safe->studio.setOptionFavourite (typeListKey, name, on); };
    browser->onClosed = [safe] { if (safe != nullptr) safe->endTrial (false); };
    return browser;
}

std::unique_ptr<OptionBrowser> ComposerPage::makeSoloBrowser()
{
    auto browser = std::make_unique<OptionBrowser> ("SOLO STYLE", soloOptions(), (int) studio.getSoloStyle(), studio.getOptionMemory(), soloListKey);
    browser->setAutoHear (true);   // (0.180.0)
    {
        juce::StringArray order { "BY STYLE" };
        order.addArray (pr7s::soloGroupOrder());
        browser->setGroupOrder (order);   // (0.158.0)
    }
    juce::Component::SafePointer<ComposerPage> safe (this);
    browser->onHear = [safe] (int id)
    {
        if (safe == nullptr)
            return;
        const auto was = safe->studio.getSoloStyle();
        const auto lead = safe->state.lane (ComposerLayer::lead);
        safe->beginTrial (soloListKey, [safe, was, lead]
        {
            if (safe == nullptr)
                return;
            safe->studio.setSoloStyle (was);
            safe->state.lane (ComposerLayer::lead) = lead;
            safe->taken[(size_t) ComposerLayer::lead].reset();
            safe->refresh();
        });
        if (! safe->state.lane (ComposerLayer::lead).on)
            safe->state.lane (ComposerLayer::lead).on = true;
        safe->setSoloStyle ((pr7s::SoloStyle) id);
        if (! safe->hearing)
            safe->hear();
    };
    browser->onUse = [safe] (int id)
    {
        if (safe == nullptr)
            return;
        safe->endTrial (true);
        safe->useSoloStyle ((pr7s::SoloStyle) id);
    };
    browser->onFavourite = [safe] (const juce::String& name, bool on) { if (safe != nullptr) safe->studio.setOptionFavourite (soloListKey, name, on); };
    browser->onClosed = [safe] { if (safe != nullptr) safe->endTrial (false); };
    return browser;
}

void ComposerPage::setLibraryDrums (bool on)
{
    state.libraryDrums = on;
    taken[(size_t) ComposerLayer::drums].reset();
    status = on ? "Drums: grooves from your library" : "Drums: the type's or the generator's";
    refresh();
}

void ComposerPage::setFitKicks (bool on)
{
    state.fitKicks = on;
    taken[(size_t) ComposerLayer::drums].reset();
    status = on ? "Drums: the kick fitted to the riff" : "Drums: the groove's kick as written";
    refresh();
}

void ComposerPage::setLibraryGroove (const juce::String& path)
{
    state.libraryGroove = path;
    state.libraryDrums = true;
    state.lane (ComposerLayer::drums).fill = {};   // (0.202.0: the groove's own notes in the bars not locked)
    taken[(size_t) ComposerLayer::drums].reset();
    refresh();
}

void ComposerPage::showTrackMenu (ComposerLayer l)
{
    juce::PopupMenu menu;
    const auto name = studio.newIdeaTrackName (pr7s::composerPartOf (l));
    const auto current = trackFor (l, false);
    // (0.112.0) The names HUB SETTINGS > COMPOSER TRACKS gives this lane.
    const auto tracks = studio.getComposerTracks (pr7s::composerPartOf (l));
    const auto names = "\"" + tracks.names.joinIntoString ("\", \"") + "\"";
    menu.addItem (1, juce::String ("Automatic: ") + (tracks.every ? "every track called " : "the track called ") + names
                         + ", else a new \"" + name + "\" one when used (Hub > ARCHITECT TRACKS)", true, trackChoice[(size_t) l] == 0);
    if (studio.findIdeaTrack (pr7s::composerPartOf (l)) == 0)
        menu.addItem (2, "Make the \"" + name + "\" track now (to hear it on its own instrument)");
    menu.addSeparator();
    for (const auto& t : studio.getProject().tracks)
        if (studio.canTakeMidi (t.id))
            menu.addItem (1000 + t.id, t.name, true, trackChoice[(size_t) l] != 0 && t.id == current);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (lanes[(size_t) l].track),
                        [safe = juce::Component::SafePointer<ComposerPage> (this), l] (int chosen)
                        {
                            if (safe == nullptr || chosen == 0)
                                return;
                            if (chosen == 1)
                                safe->setTrack (l, 0);
                            else if (chosen == 2)
                            {
                                auto& studio = safe->studio;
                                // (0.107.0) A drums track plays the studio's drums (PR7-D) when found, on the
                                // default map (0.77.3).
                                // (0.201.0) At the lane's level (Hub > ARCHITECT TRACKS).
                                studio.addIdeaTrack (pr7s::composerPartOf (l));
                                safe->setTrack (l, 0);
                            }
                            else
                                safe->setTrack (l, chosen - 1000);
                        });
}

void ComposerPage::showProgressionMenu()
{
    const auto found = pr7s::composerProgressions (state);
    const auto& library = pr7s::progressionLibrary();
    juce::PopupMenu menu;
    // (0.208.0) The popular progressions in the key first: the same list and names the chord pads show.
    {
        const auto popular = getPopularInKey();
        menu.addSectionHeader ("POPULAR IN " + ChordUi::spokenKey (state.key).toUpperCase());
        for (int i = 0; i < (int) popular.size(); ++i)
        {
            juce::StringArray names;
            for (const auto& c : popular[(size_t) i].chords)
                names.add (c.name (state.key));
            menu.addItem (popularItems + i, popular[(size_t) i].name + "   " + names.joinIntoString (" "));
        }
        menu.addSeparator();
        menu.addSectionHeader ("THE LIBRARY");
    }
    // By genre (each a submenu), in the order found (the key's own mode first within each).
    std::map<juce::String, juce::PopupMenu> byGenre;
    juce::StringArray order;
    for (const auto i : found)
    {
        const auto& p = library[(size_t) i];
        if (! order.contains (p.genre))
            order.add (p.genre);
        byGenre[p.genre].addItem (i + 1, p.name + "   " + p.numerals, true, p.name == state.progression);
    }
    for (const auto& g : order)
        menu.addSubMenu (g, byGenre[g]);
    if (found.empty())
        menu.addItem (-1, "None pass: choose ANY GENRE or ANY MOOD", false, false);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (progressionKey),
                        [safe = juce::Component::SafePointer<ComposerPage> (this)] (int chosen)
                        {
                            if (safe == nullptr || chosen <= 0)
                                return;
                            if (chosen >= popularItems)
                                safe->usePopularProgression (chosen - popularItems);
                            else
                                safe->setProgression (pr7s::progressionLibrary()[(size_t) chosen - 1].name);
                        });
}

void ComposerPage::showUseMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "All the layers that are on");
    menu.addSeparator();
    for (const auto l : pr7s::composerLayers())
        menu.addItem (10 + (int) l, "Only " + pr7s::composerLayerName (l).toLowerCase() + " (" + trackText (l).fromFirstOccurrenceOf ("TRACK: ", false, false).toLowerCase() + ")",
                      ! ideas[(size_t) l].notes.empty());
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (useKey),
                        [safe = juce::Component::SafePointer<ComposerPage> (this)] (int chosen)
                        {
                            if (safe == nullptr || chosen == 0)
                                return;
                            if (chosen == 1)
                                safe->useAll();
                            else
                                safe->useLayer ((ComposerLayer) (chosen - 10));
                        });
}

bool ComposerPage::putToneOnLane (ComposerLayer l)
{
    const auto& idea = ideas[(size_t) l];
    std::vector<int> tracks;
    for (const auto id : tracksFor (l, false))
        if (studio.canTakeMidi (id))
            tracks.push_back (id);
    if (tracks.empty() || idea.notes.empty())
    {
        status = "Build the " + pr7s::composerLayerName (l).toLowerCase() + " lane onto a track first";
        repaint();
        return false;
    }
    auto any = false;
    juce::String problem;
    for (const auto id : tracks)
    {
        juce::String error;
        if (studio.toneTrack (id, idea, true, &error))
            any = true;
        else
            problem = error;
    }
    status = any ? pr7s::composerLayerName (l).toUpperCase() + ": " + studio.getLastTone() + " ON PR7-G" : problem;
    repaint();
    return any;
}

bool ComposerPage::putPatchOnLane (ComposerLayer l)
{
    const auto& idea = ideas[(size_t) l];
    std::vector<int> tracks;
    for (const auto id : tracksFor (l, false))
        if (studio.canTakeMidi (id))
            tracks.push_back (id);
    if (tracks.empty() || idea.notes.empty())
    {
        status = "Build the " + pr7s::composerLayerName (l).toLowerCase() + " lane onto a track first";
        repaint();
        return false;
    }
    auto any = false;
    juce::String problem;
    for (const auto id : tracks)
    {
        juce::String error;
        if (studio.patchTrack (id, idea, true, &error))
            any = true;
        else
            problem = error;
    }
    status = any ? pr7s::composerLayerName (l).toUpperCase() + ": " + studio.getLastPatch() + " ON PR7-K" : problem;
    repaint();
    return any;
}

bool ComposerPage::writeSectionTonesOnLane (ComposerLayer l)
{
    auto written = 0;
    juce::String problem = "No track plays the " + pr7s::composerLayerName (l).toLowerCase() + " lane yet";
    for (const auto id : tracksFor (l, false))
    {
        juce::String error;
        if (studio.writeSectionTones (id, &error))
            ++written;
        else
            problem = error;
    }
    status = written > 0 ? pr7s::composerLayerName (l).toUpperCase() + ": PR7-G FOLLOWS " + juce::String (studio.getSectionToneBlocks())
                               + " SECTIONS" + (written > 1 ? " ON " + juce::String (written) + " TRACKS" : juce::String())
                         : problem;
    repaint();
    return written > 0;
}

void ComposerPage::showLaneMenu (ComposerLayer l)
{
    const auto& s = state.lane (l);
    juce::PopupMenu menu;
    menu.addItem (1, "Roll (a New Take)");
    menu.addItem (2, s.locked ? "Unlock" : "Lock");
    menu.addItem (3, s.on ? "Switch Off" : "Switch On");
    menu.addSeparator();
    menu.addItem (4, "Use Only This Layer", ! ideas[(size_t) l].notes.empty());
    // (0.202.0) BAR LOCKS.
    menu.addSeparator();
    const auto chosen = barSelection[(size_t) l];
    menu.addItem (10, "Lock the Chosen Bars", chosen != 0 && (chosen & ~s.lockedBars) != 0);
    menu.addItem (11, "Unlock the Chosen Bars", (chosen & s.lockedBars) != 0);
    menu.addItem (12, "Unlock All Bars", s.hasBarLocks());
    menu.addItem (15, "Clear the Bars Chosen", chosen != 0);
    if (l == ComposerLayer::drums && s.hasBarLocks())
    {
        menu.addItem (13, "Choose a Fill for the Bars Not Locked...");
        menu.addItem (14, "Roll the Groove (No Fill)");
    }
    // (0.204.0) GUITAR TONES: PR7-G on the lane's tracks.
    if (pr7s::hasGuitarTones (pr7s::composerPartOf (l)))
    {
        menu.addSeparator();
        menu.addItem (20, "Put the Suggested Tone on Its Track (PR7-G)", ! ideas[(size_t) l].notes.empty());
        menu.addItem (21, "Write the Tone's Section Changes (PR7-G)", ! studio.getSongSections().blocks.empty());
    }
    // (0.220.0) The keys lane's CYBERPUNK parts: their PR7-K patch.
    if (l == ComposerLayer::chords && state.lane (l).world == (int) pr7s::LaneWorld::cyberpunk)
    {
        menu.addSeparator();
        menu.addItem (22, "Put the Matching Patch on Its Track (PR7-K)", ! ideas[(size_t) l].notes.empty());
    }
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<ComposerPage> (this), l] (int chosen)
    {
        if (safe == nullptr)
            return;
        const auto& lane = safe->state.lane (l);
        switch (chosen)
        {
            case 1: safe->roll (l); break;
            case 2: safe->setLocked (l, ! lane.locked); break;
            case 3: safe->setOn (l, ! lane.on); break;
            case 4: safe->useLayer (l); break;
            case 10: safe->lockBars (l, safe->barSelection[(size_t) l]); safe->clearBarSelection (l); break;
            case 11: safe->unlockBars (l, safe->barSelection[(size_t) l]); safe->clearBarSelection (l); break;
            case 12: safe->unlockAllBars (l); break;
            case 13: safe->showOptionBrowser (l); break;
            case 14: safe->rollGroove (l); break;
            case 15: safe->clearBarSelection (l); break;
            case 20: safe->putToneOnLane (l); break;
            case 21: safe->writeSectionTonesOnLane (l); break;
            case 22: safe->putPatchOnLane (l); break;
            default: break;
        }
    });
}

void ComposerPage::mouseDown (const juce::MouseEvent& e)
{
    pressedLane = -1;
    draggedOut = false;
    if (const auto chord = chordAt (e.getPosition()); chord >= 0)
    {
        showChordMenu (chord);
        return;
    }
    if (songArea.contains (e.getPosition()) && ! e.mods.isPopupMenu() && usesSectionsRow())
    {
        for (const auto& [r, index] : songChips())
            if (r.contains (e.getPosition()))
            {
                targetSection (studio.getSongSections().blocks[(size_t) index].id);   // (0.93.0: compose that block)
                return;
            }
    }
    if (songArea.contains (e.getPosition()) && e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Forget the Sections Built (the song's parts stay)", ! studio.getBuiltSections().empty());
        menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<ComposerPage> (this)] (int chosen)
                            {
                                if (safe != nullptr && chosen == 1)
                                    safe->forgetSong();
                            });
        return;
    }
    selectingBars = false;
    for (const auto l : pr7s::composerLayers())
        if (lanes[(size_t) l].notes.contains (e.getPosition()))
        {
            if (e.mods.isPopupMenu())
            {
                // (0.202.0) Nothing chosen: the bar under the mouse, so the menu's Lock the Chosen Bars has one.
                if (barSelection[(size_t) l] == 0)
                    if (const auto bar = barAtX (l, e.x); bar >= 0)
                        selectBars (l, bar, bar);
                showLaneMenu (l);
            }
            else
                pressedLane = (int) l;   // (a click hears on release; a drag sideways chooses bars, up or down takes the lane out)
            return;
        }
}

void ComposerPage::mouseDrag (const juce::MouseEvent& e)
{
    if (pressedLane < 0 || draggedOut)
        return;
    const auto l = (ComposerLayer) pressedLane;
    if (selectingBars)
    {
        // (0.202.0) The bars from where it went down to where it is.
        const auto bar = barAtX (l, e.x);
        if (bar >= 0)
        {
            barSelection[(size_t) pressedLane] = selectionBefore;
            selectBars (l, barAnchor, bar, true);
        }
        return;
    }
    if (e.getDistanceFromDragStart() < 6)
        return;
    const auto offset = e.getOffsetFromDragStart();
    if (std::abs (offset.x) >= std::abs (offset.y) && barAtX (l, e.getMouseDownX()) >= 0)
    {
        selectingBars = true;
        barAnchor = barAtX (l, e.getMouseDownX());
        selectionBefore = e.mods.isShiftDown() || e.mods.isCommandDown() ? barSelection[(size_t) pressedLane] : 0;
        mouseDrag (e);
        return;
    }
    draggedOut = true;
    dragLaneOut (l);
}

void ComposerPage::mouseUp (const juce::MouseEvent& e)
{
    if (pressedLane >= 0 && selectingBars)
    {
        const auto l = (ComposerLayer) pressedLane;
        const auto chosen = barSelection[(size_t) pressedLane];
        auto first = -1, last = -1, count = 0;
        for (int b = 0; b < 64; ++b)
            if (((chosen >> b) & 1u) != 0)
            {
                first = first < 0 ? b : first;
                last = b;
                ++count;
            }
        if (count > 0)
            status = pr7s::composerLayerName (l) + ": " + (count == 1 ? "bar " + juce::String (first + 1)
                                                                      : juce::String (count) + " bars (" + juce::String (first + 1) + "-" + juce::String (last + 1) + ")")
                   + " chosen: LOCK BARS keeps them";
        refreshBarKeys();
        repaint();
    }
    else if (pressedLane >= 0 && ! draggedOut)
    {
        const auto l = (ComposerLayer) pressedLane;
        if (e.mods.isShiftDown() || e.mods.isCommandDown())
        {
            // (0.202.0) Shift / Ctrl-click: that bar added to the bars chosen (or taken out).
            if (const auto bar = barAtX (l, e.x); bar >= 0)
            {
                barSelection[(size_t) pressedLane] ^= (juce::uint64) 1 << bar;
                refreshBarKeys();
                repaint();
            }
        }
        else if (barSelection[(size_t) pressedLane] != 0)
            clearBarSelection (l);   // (a click lets the bars chosen go)
        else
            isHearing() ? stop() : (void) hear();
    }
    selectingBars = false;
    pressedLane = -1;
}

//==============================================================================
// (0.202.0) BAR LOCKS.
bool ComposerPage::openBarsChosen() const
{
    const auto& lane = state.lane (ComposerLayer::drums);
    const auto chosen = barSelection[(size_t) ComposerLayer::drums] & ~lane.lockedBars;
    const auto bars = juce::jmin (64, barCount());
    return bars > 0 && (chosen & (bars >= 64 ? ~(juce::uint64) 0 : (((juce::uint64) 1 << bars) - 1))) != 0;
}

juce::String ComposerPage::fillOfBar (int bar) const
{
    const auto now = pr7s::barFills (state.lane (ComposerLayer::drums), barCount());
    const auto found = now.find (bar);
    if (found == now.end())
        return {};
    for (const auto& f : fillsShown)
        if (f.id == found->second)
            return f.name;
    return {};
}

int ComposerPage::barCount() const
{
    return pr7s::composerBarCount (spans, context.beatsPerBar);
}

int ComposerPage::barAtX (ComposerLayer l, int x) const
{
    const auto& r = lanes[(size_t) l].notes;
    const auto bars = barCount();
    if (bars <= 0 || spans.empty() || r.getWidth() <= 0)
        return -1;
    const auto total = spans.back().end - spans.front().start;
    const auto beat = (double) (x - r.getX()) / (double) r.getWidth() * total;
    return juce::jlimit (0, juce::jmin (bars, 64) - 1, (int) std::floor (beat / context.beatsPerBar));
}

juce::Rectangle<int> ComposerPage::getBarBounds (ComposerLayer l, int bar) const
{
    const auto& r = lanes[(size_t) l].notes;
    if (spans.empty() || bar < 0 || bar >= barCount())
        return {};
    const auto total = spans.back().end - spans.front().start;
    const auto x0 = r.getX() + (int) std::round ((bar * context.beatsPerBar) / total * r.getWidth());
    const auto x1 = r.getX() + (int) std::round (juce::jmin (total, (bar + 1) * context.beatsPerBar) / total * r.getWidth());
    return { x0, r.getY(), juce::jmax (1, x1 - x0), r.getHeight() };
}

void ComposerPage::selectBars (ComposerLayer l, int firstBar, int lastBar, bool add)
{
    const auto bars = juce::jmin (barCount(), 64);
    if (bars <= 0)
        return;
    if (firstBar > lastBar)
        std::swap (firstBar, lastBar);
    firstBar = juce::jlimit (0, bars - 1, firstBar);
    lastBar = juce::jlimit (0, bars - 1, lastBar);
    auto& chosen = barSelection[(size_t) l];
    if (! add)
        chosen = 0;
    for (auto b = firstBar; b <= lastBar; ++b)
        chosen |= (juce::uint64) 1 << b;
    refreshBarKeys();
    repaint();
}

void ComposerPage::clearBarSelection (ComposerLayer l)
{
    barSelection[(size_t) l] = 0;
    refreshBarKeys();
    repaint();
}

void ComposerPage::lockBars (ComposerLayer l, juce::uint64 bars)
{
    if (bars == 0 || spans.empty())
        return;
    // The notes as the lane shows them now (what you hear) kept in those bars.
    pr7s::lockLaneBars (state.lane (l), ideas[(size_t) l], bars, context.beatsPerBar, barCount());
    juce::StringArray numbers;
    for (int b = 0; b < 64; ++b)
        if (((state.lane (l).lockedBars >> b) & 1u) != 0)
            numbers.add (juce::String (b + 1));
    status = pr7s::composerLayerName (l) + ": bars " + numbers.joinIntoString (", ") + " locked"
           + (l == ComposerLayer::drums ? juce::String (" (ROLL: a new fill for the others)") : juce::String (" (ROLL rewrites the others)"));
    refresh();
}

void ComposerPage::unlockBars (ComposerLayer l, juce::uint64 bars)
{
    if (bars == 0)
        return;
    pr7s::unlockLaneBars (state.lane (l), bars, context.beatsPerBar);
    status = pr7s::composerLayerName (l) + (state.lane (l).hasBarLocks() ? ": bars let go" : ": every bar let go");
    refresh();
}

void ComposerPage::lockSelectedBars (ComposerLayer l)
{
    const auto chosen = barSelection[(size_t) l];
    if (chosen == 0)
        return;
    if ((chosen & ~state.lane (l).lockedBars) == 0)
        unlockBars (l, chosen);   // (every one already locked: LOCK BARS lets them go)
    else
        lockBars (l, chosen);
    clearBarSelection (l);
}

void ComposerPage::unlockAllBars (ComposerLayer l)
{
    if (! state.lane (l).hasBarLocks())
        return;
    pr7s::dropBarLocks (state.lane (l));
    status = pr7s::composerLayerName (l) + ": every bar let go";
    refresh();
}

void ComposerPage::refreshBarKeys()
{
    for (const auto l : pr7s::composerLayers())
    {
        auto& key = lanes[(size_t) l].bars;
        const auto chosen = barSelection[(size_t) l];
        const auto locked = state.lane (l).lockedBars;
        key.setButtonText (chosen != 0 ? ((chosen & ~locked) == 0 ? "UNLOCK BARS" : "LOCK BARS")
                                       : locked != 0 ? "UNLOCK ALL" : "LOCK BARS");
        key.setToggleState (locked != 0, juce::dontSendNotification);
    }
}

void ComposerPage::mouseMove (const juce::MouseEvent& e)
{
    const auto overNotes = std::any_of (lanes.begin(), lanes.end(), [&e] (const Lane& lane) { return lane.notes.contains (e.getPosition()); });
    setMouseCursor (overNotes ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

juce::File ComposerPage::dragLaneOut (ComposerLayer l)
{
    const auto& idea = ideas[(size_t) l];
    if (spans.empty() || idea.notes.empty())
        return {};
    // Where it would be placed (the tempo there goes in the file), on its track's channel; the drums on 10.
    const auto* track = studio.getProject().findTrack (trackFor (l, false));
    const auto channel = l == ComposerLayer::drums ? 10 : track != nullptr ? track->midiChannel : 1;
    const auto name = juce::String ("Architect ") + pr7s::composerLayerName (l).toLowerCase() + " - " + idea.style;   // ("Architect bass - Walking")
    juce::String error;
    const auto file = studio.writeLineForDrag (idea, placedSpans(), channel, name, error);
    if (file == juce::File())
    {
        status = error;
        repaint();
        return {};
    }
    status = "Dragged out: " + file.getFileName();
    repaint();
    if (onExternalDrag)
        onExternalDrag ({ file.getFullPathName() });
    else
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);   // (a copy, never a move)
    return file;
}

void ComposerPage::timerCallback()
{
    // Heard no longer (Space, the song changed): the keys say so.
    if (hearing && ! studio.isComposerAuditioning())
    {
        hearing = false;
        hearKey.setButtonText ("HEAR");
        hearKey.setToggleState (false, juce::dontSendNotification);
        repaint();
    }
    if (! isShowing())
        return;   // (0.164.0: behind the IDEAS tab or a closed window it rebuilt on every change; it catches up when shown)
    if (startOverIfNewSong())   // (0.206.0: another song: its own ARCHITECT)
        return;
    // The song changed (tracks added or renamed, the chord track): the lanes' tracks and chords again.
    // (0.190.0) A track clicked (a selection alone) changes nothing the lanes are made from: only
    // the "heard on" lines (painted from the selection) are drawn again, not the whole composition.
    if (! hearing && studio.getChangeVersion() != seenChange)
    {
        if (studio.getContentVersion() != seenContent)
            refresh (false);
        else
        {
            seenChange = studio.getChangeVersion();
            repaint();
        }
    }
    // (0.76.0) The faders moved in the mixer (mix-only changes don't move the change version): the lanes follow.
    if (draggingVolume < 0)
        refreshVolumes();
    if (hearing && ! spans.empty())
    {
        // The playhead across the lanes: (0.83.2) only its old and new strips in each lane (the whole of
        // every lane's notes was redrawn 30 times a second, the notes and all).
        const auto beat = studio.isComposerAuditioning() ? const_cast<pr7s::Studio&> (studio).getEngine().getPosition() - studio.getComposerAuditionStart() : -1.0;
        if (! juce::approximatelyEqual (beat, shownHearBeat))
        {
            const auto total = spans.back().end - spans.front().start;
            for (const auto& lane : lanes)
                for (const auto at : { shownHearBeat, beat })
                    if (at >= 0.0 && total > 0.0)
                    {
                        const auto x = (float) lane.notes.getX() + (float) (at / total) * (float) lane.notes.getWidth();
                        repaint (juce::Rectangle<int> (juce::roundToInt (x) - 3, lane.notes.getY(), 8, lane.notes.getHeight()));
                    }
            shownHearBeat = beat;
        }
    }
    else
        shownHearBeat = -1.0;
}

//==============================================================================
juce::String ComposerPage::summary() const
{
    if (spans.empty())
        return state.fromChordTrack ? "The chord track has no chords" : "No progression in the library passes this key, genre and mood";
    const auto& names = chordNames;
    // (The progression's once through: it repeats to fill the length.)
    const auto steps = state.fromChordTrack ? (int) names.size() : (int) pr7s::progressionLibrary()[(size_t) juce::jmax (0, pr7s::composerProgressionIndex (state))].in (state.key).size();
    juce::StringArray once;
    for (int i = 0; i < juce::jmin (steps, names.size()); ++i)
        once.add (names[i]);
    const auto bars = (spans.back().end - spans.front().start) / juce::jmax (1.0, context.beatsPerBar);
    const auto keyPart = ChordUi::spokenKey (state.key) + (isKeyLocked() ? " (locked)" : juce::String())
                       + ", " + juce::String (juce::roundToInt (context.bpm)) + " BPM";
    return (state.fromChordTrack ? juce::String ("The chord track's chords") : fromRiff ? keyPart + ", chords from your riff" : keyPart + ", " + state.progression)
         + ", " + juce::String (juce::roundToInt (bars)) + " bars:  " + once.joinIntoString ("  ")
         + (names.size() > once.size() ? "  (repeated)" : juce::String());
}

void ComposerPage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0a0d));
    // (0.175.0) Only what the redraw reaches: while it plays, the playhead's 8 px strips in every lane
    // came here 30 times a second and laid out the header, the song strip and every lane's header.
    const auto clip = g.getClipBounds();
    if (clip.getY() < headerHeight)
    {
    ParagonColours::setFill (g, ParagonColours::accent, juce::Rectangle<float> ((float) (0), (float) (0), (float) (getWidth()), (float) (3)));
    g.fillRect (0, 0, getWidth(), 3);
    auto head = getLocalBounds().removeFromTop (headerHeight).reduced (gap, 8);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (20.0f));
    g.drawText ("ARCHITECT", head.removeFromTop (26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (interFont (12.5f));
    g.drawText (status.isNotEmpty() ? summary() + "     " + status : summary(), head.withTrimmedRight (1100), juce::Justification::centredLeft, true);
    }

    // The labels in the controls row.
    g.setFont (ParagonLookAndFeel::titleFont (12.0f));
    g.setColour (ParagonColours::textDim);
    g.drawText ("CHORDS FROM", controlsLabels, juce::Justification::centredRight, false);

    if (clip.intersects (songArea))
        paintSong (g);

    // The ruler: bars, and the chords' names where each starts.
    if (! spans.empty() && ! ruler.isEmpty() && clip.intersects (ruler))
    {
        const auto first = spans.front().start, total = spans.back().end - first;
        const auto xFor = [&] (double beat) { return (float) ruler.getX() + (float) ((beat - first) / total) * (float) ruler.getWidth(); };
        g.setColour (juce::Colour (0xff141015));
        g.fillRect (ruler);
        const auto barLength = juce::jmax (0.25, context.beatsPerBar);
        g.setFont (ParagonLookAndFeel::titleFont (11.0f));
        for (double b = 0.0; b < total - 1.0e-6; b += barLength)
        {
            const auto x = xFor (first + b);
            g.setColour (ParagonColours::panelEdge);
            g.fillRect (x, (float) ruler.getY(), 1.0f, (float) ruler.getHeight() / 3.0f);
            g.setColour (ParagonColours::textDim);
            g.drawText (juce::String ((int) std::round (b / barLength) + 1), juce::Rectangle<float> (x + 3.0f, (float) ruler.getY(), 30.0f, 12.0f),
                        juce::Justification::centredLeft, false);
        }
        g.setFont (ChordUi::chordFont (13.0f));
        for (size_t i = 0; i < spans.size(); ++i)
        {
            const auto& s = spans[i];
            const auto x = xFor (s.start);
            g.setColour (s.chord.isInKey (s.key) ? ParagonColours::text : juce::Colour (pr7s::FamilyColours::pink));
            if (chordOverrides.count ((int) i) > 0)
                g.fillRect (x + 3.0f, (float) ruler.getBottom() - 2.0f, juce::jmax (12.0f, xFor (s.end) - x - 8.0f), 1.5f);   // (swapped: underlined)
            g.drawText (chordNames[(int) i], juce::Rectangle<float> (x + 3.0f, (float) ruler.getY() + 12.0f, juce::jmax (20.0f, xFor (s.end) - x - 4.0f), 17.0f),
                        juce::Justification::centredLeft, true);
        }
    }
    // (0.59.0) The hint, where the lanes' headers meet the ruler.
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::titleFont (11.0f));
    g.drawText ("CHORDS: CLICK TO SWAP. NOTES: CLICK TO HEAR, DRAG OUT AS MIDI",
                juce::Rectangle<int> (gap, ruler.getY(), laneHeaderWidth - 150, ruler.getHeight()), juce::Justification::centredLeft, true);
    for (const auto l : pr7s::composerLayers())
        if (const auto& lane = lanes[(size_t) l]; clip.intersects (lane.header.getUnion (lane.notes).getUnion (lane.note)))
            paintLane (g, l, lane);
}

void ComposerPage::paintSong (juce::Graphics& g) const
{
    if (songArea.isEmpty())
        return;
    auto area = songArea;
    g.setColour (juce::Colour (0xff141015));
    g.fillRoundedRectangle (area.toFloat(), 5.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 5.0f, 1.0f);
    area = area.reduced (12, 6);
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::titleFont (14.0f));
    g.drawText ("SONG", area.removeFromLeft (52), juce::Justification::centredLeft, false);
    if (! nextKeys.isEmpty())
        area.setRight (nextKeys.getFirst()->getX() - 12);
    if (usesSectionsRow())
    {
        // (0.93.0) The SECTIONS row's blocks: the one composed lit, the built ones with a mark.
        const auto& blocks = studio.getSongSections().blocks;
        const auto places = studio.getSongSectionPlaces();
        for (const auto& [r, index] : songChips())
        {
            const auto& b = blocks[(size_t) index];
            const auto kind = pr7s::drumSections()[(size_t) pr7s::composerSectionFor (b.name)];
            const auto colour = kind == pr7s::DrumSection::chorus ? pr7s::FamilyColours::pink
                              : kind == pr7s::DrumSection::verse || kind == pr7s::DrumSection::bridge ? pr7s::FamilyColours::orange
                              : pr7s::FamilyColours::red;
            const auto lit = b.id == targetBlock;
            const auto rf = r.toFloat();
            g.setColour (colour.withAlpha (lit ? 0.6f : 0.22f));
            g.fillRoundedRectangle (rf, 4.0f);
            g.setColour (lit ? juce::Colour (0xfff4eeea) : colour);
            g.drawRoundedRectangle (rf.reduced (0.5f), 4.0f, lit ? 2.0f : 1.2f);
            g.setColour (lit ? juce::Colours::white : ParagonColours::text);
            g.setFont (ParagonLookAndFeel::titleFont (15.0f));
            g.drawText (pr7s::sectionLabel (blocks, index).toUpperCase(), rf.reduced (8.0f, 2.0f).removeFromTop (rf.getHeight() * 0.55f),
                        juce::Justification::bottomLeft, false);
            g.setColour (ParagonColours::textDim);
            g.setFont (ParagonLookAndFeel::titleFont (12.0f));
            g.drawText ("BAR " + juce::String (places[(size_t) index].firstBar + 1) + ", " + juce::String (b.bars) + " BARS"
                            + (b.recipe.isNotEmpty() ? "  BUILT" : ""),
                        rf.reduced (8.0f, 2.0f).removeFromBottom (rf.getHeight() * 0.45f), juce::Justification::topLeft, false);
        }
        return;
    }
    const auto& built = studio.getBuiltSections();
    if (built.empty())
    {
        g.setFont (interFont (13.0f));
        g.drawText ("Nothing built yet: choose the chords, STYLE and SECTION, then BUILD. Each section goes after the last.",
                    area, juce::Justification::centredLeft, true);
        return;
    }
    const auto& sig = studio.getProject().signatures();
    const auto chip = 118;
    const auto fits = juce::jmax (1, (area.getWidth() + 6) / (chip + 6));
    const auto first = (int) built.size() > fits ? (int) built.size() - fits + 1 : 0;   // (the latest, with "..." before)
    if (first > 0)
    {
        g.setFont (ParagonLookAndFeel::titleFont (14.0f));
        g.drawText (juce::String (first) + " MORE...", area.removeFromLeft (chip), juce::Justification::centredLeft, false);
        area.removeFromLeft (6);
    }
    for (int i = first; i < (int) built.size(); ++i)
    {
        const auto& b = built[(size_t) i];
        const auto r = area.removeFromLeft (chip).toFloat();
        area.removeFromLeft (6);
        const auto colour = b.kind == pr7s::DrumSection::chorus ? pr7s::FamilyColours::pink
                          : b.kind == pr7s::DrumSection::verse || b.kind == pr7s::DrumSection::bridge ? pr7s::FamilyColours::orange
                          : pr7s::FamilyColours::red;
        g.setColour (colour.withAlpha (0.28f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (colour);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.2f);
        const auto bar = sig.barAt (b.start) + 1, bars = sig.barAt (b.end) + 1 - bar;
        g.setColour (ParagonColours::text);
        g.setFont (ParagonLookAndFeel::titleFont (15.0f));
        g.drawText (pr7s::sectionWord (b.kind).toUpperCase(), r.reduced (8.0f, 2.0f).removeFromTop (r.getHeight() * 0.55f), juce::Justification::bottomLeft, false);
        g.setColour (ParagonColours::textDim);
        g.setFont (interFont (11.0f));
        g.drawText ("BAR " + juce::String (bar) + ", " + juce::String (bars) + " BARS", r.reduced (8.0f, 2.0f).removeFromBottom (r.getHeight() * 0.45f),
                    juce::Justification::topLeft, false);
    }
}

std::vector<std::pair<juce::Rectangle<int>, int>> ComposerPage::songChips() const
{
    std::vector<std::pair<juce::Rectangle<int>, int>> out;
    if (songArea.isEmpty() || ! usesSectionsRow())
        return out;
    auto area = songArea.reduced (12, 6);
    area.removeFromLeft (52);
    if (! nextKeys.isEmpty())
        area.setRight (nextKeys.getFirst()->getX() - 12);
    const auto count = (int) studio.getSongSections().blocks.size();
    const auto chip = 118;
    const auto fits = juce::jmax (1, (area.getWidth() + 6) / (chip + 6));
    // The one composed kept in view: from the start, or so that it's the last shown.
    const auto target = juce::jmax (0, studio.getSongSections().indexOf (targetBlock));
    const auto first = juce::jlimit (0, juce::jmax (0, count - fits), target - fits + 1);
    for (int i = first; i < count && i < first + fits; ++i)
    {
        out.push_back ({ area.removeFromLeft (chip), i });
        area.removeFromLeft (6);
    }
    return out;
}

juce::Rectangle<int> ComposerPage::getSongChipBounds (int blockIndex) const
{
    for (const auto& [r, index] : songChips())
        if (index == blockIndex)
            return r;
    return {};
}

void ComposerPage::paintLane (juce::Graphics& g, ComposerLayer l, const Lane& lane) const
{
    const auto& s = state.lane (l);
    const auto colour = colourOf (l);
    // The header: the layer's name over its keys.
    if (g.clipRegionIntersects (lane.header.getUnion (lane.note)))
    {
        const auto r = lane.header.toFloat();
        g.setColour (juce::Colour (0xff141015));
        g.fillRoundedRectangle (r, 5.0f);
        ParagonColours::setFill (g, s.locked ? ParagonColours::accent : ParagonColours::panelEdge, r.reduced (0.5f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, s.locked ? 1.5f : 1.0f);
        g.setColour (colour.withAlpha (s.on ? 1.0f : 0.35f));
        g.fillRoundedRectangle (r.withWidth (4.0f).reduced (0.0f, 8.0f).withX (r.getX() + 6.0f), 1.5f);
        g.setColour (s.on ? ParagonColours::text : ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::titleFont (20.0f));
        g.drawText (pr7s::composerLayerName (l), lane.header.reduced (18, 6).removeFromTop (24), juce::Justification::centredLeft, false);
        auto sync = state.leaderOf (l);
        if (! sync.has_value())   // (0.126.0: a lane another lane follows by its own SYNC TO leads too)
            for (const auto other : pr7s::composerLayers())
                if (other != l && state.followsOwnLeader (other) && state.lane (other).syncTo == (int) l)
                    sync = l;
        if (! s.locked && hasWorlds (l) && s.world != 0)
        {
            // (0.219.0) The lane's own WORLD.
            g.setFont (ParagonLookAndFeel::titleFont (11.0f));
            const auto words = pr7s::laneWorldNames()[s.world] + " WORLD";
            const auto wordsArea = lane.header.reduced (18, 6).removeFromTop (24).withTrimmedRight (200);
            ParagonColours::setTextFill (g, ParagonColours::accent, words, wordsArea, juce::Justification::centredRight);
            g.drawText (words, wordsArea, juce::Justification::centredRight, false);
        }
        else if (! s.locked && ! sync.has_value() && (s.ownStyles ? typeWouldWrite (l) : pr7s::sectionTypeWritesLane (l, state, context)))
        {
            // (0.126.0) Whether the section type writes this lane, or it plays its own styles.
            g.setFont (ParagonLookAndFeel::titleFont (11.0f));
            const auto words = juce::String (s.ownStyles ? "OWN STYLES" : "FOLLOWS THE SECTION TYPE");
            const auto wordsArea = lane.header.reduced (18, 6).removeFromTop (24).withTrimmedRight (200);
            ParagonColours::setTextFill (g, s.ownStyles ? ParagonColours::textDim : ParagonColours::accent, words, wordsArea, juce::Justification::centredRight);
            g.drawText (words, wordsArea, juce::Justification::centredRight, false);
        }
        else if (s.locked || sync.has_value())
        {
            // (0.86.0) SYNC: the leader says so; a follower says whom it follows.
            const auto word = sync == l ? juce::String ("SYNC LEADER") + (s.locked ? " - LOCKED" : "")
                            : sync.has_value() ? (s.doubleSync ? "DOUBLES THE " : "FOLLOWS THE ") + pr7s::composerLayerName (*sync) + (s.locked ? " - LOCKED" : "")
                                               : juce::String ("LOCKED");
            g.setFont (ParagonLookAndFeel::titleFont (11.0f));
            const auto wordArea = lane.header.reduced (18, 6).removeFromTop (24).withTrimmedRight (200);
            ParagonColours::setTextFill (g, sync == l ? pr7s::FamilyColours::pink : ParagonColours::accent, word, wordArea, juce::Justification::centredRight);
            g.drawText (word, wordArea, juce::Justification::centredRight, false);
        }
        // Where it's heard, when that's not where it's written.
        const auto writes = trackFor (l, false), heard = trackFor (l, true);
        if ((writes != heard || heard == 0) && ! lane.note.isEmpty())
        {
            g.setColour (ParagonColours::textDim);
            g.setFont (interFont (10.5f));
            const auto* t = studio.getProject().findTrack (heard);
            g.drawText (heard == 0 ? juce::String ("not heard: no track of its own yet (TRACK > make it)")
                                   : "heard on " + (t != nullptr ? t->name : juce::String()) + " until it has its own track",
                        lane.note, juce::Justification::centredLeft, true);
        }
    }
    // The notes.
    const auto r = lane.notes.toFloat();
    g.setColour (juce::Colour (0xff0b090c));
    g.fillRoundedRectangle (r, 3.0f);
    if (spans.empty())
        return;
    const auto first = spans.front().start, total = spans.back().end - first;
    const auto xFor = [&] (double beat) { return r.getX() + (float) (beat / total) * r.getWidth(); };
    {
        // (0.202.0) The locked bars shaded (a red line over them), the bars chosen lit.
        const auto bpb = context.beatsPerBar;
        const auto bars = juce::jmin (64, pr7s::composerBarCount (spans, bpb));
        for (int b = 0; b < bars; ++b)
        {
            const auto x0 = xFor (b * bpb), x1 = xFor (juce::jmin (total, (b + 1) * bpb));
            const auto cell = juce::Rectangle<float> (x0, r.getY(), x1 - x0, r.getHeight());
            if (((s.lockedBars >> b) & 1u) != 0)
            {
                g.setColour (juce::Colour (0xfff4eeea).withAlpha (0.07f));
                g.fillRect (cell);
                g.setColour (juce::Colour (pr7s::FamilyColours::red).withAlpha (0.9f));
                g.fillRect (cell.withHeight (3.0f));
                if (x1 - x0 > 40.0f)
                {
                    g.setColour (ParagonColours::textDim);
                    g.setFont (ParagonLookAndFeel::titleFont (11.0f));
                    g.drawText ("LOCKED", cell.reduced (5.0f, 4.0f).withHeight (14.0f).translated (0.0f, 2.0f), juce::Justification::topLeft, false);
                }
            }
            else if (l == ComposerLayer::drums && x1 - x0 > 40.0f)
                if (const auto name = fillOfBar (b); name.isNotEmpty())
                {
                    // (0.203.0) The open bar's own fill named at its foot.
                    g.setColour (ParagonColours::textDim);
                    g.setFont (ParagonLookAndFeel::titleFont (10.0f));
                    g.drawText ("FILL: " + name.toUpperCase(), cell.reduced (5.0f, 3.0f).removeFromBottom (12.0f), juce::Justification::bottomLeft, true);
                }
            if (((barSelection[(size_t) l] >> b) & 1u) != 0)
            {
                ParagonColours::setFill (g, ParagonColours::accent.withAlpha (0.22f), cell);
                g.fillRect (cell);
                ParagonColours::setFill (g, ParagonColours::accent, cell);
                g.drawRect (cell, 1.5f);
            }
        }
    }
    for (const auto& span : spans)
    {
        g.setColour (ParagonColours::panelEdge.withAlpha (0.6f));
        g.fillRect (xFor (span.start - first), r.getY(), 1.0f, r.getHeight());
    }
    const auto& notes = ideas[(size_t) l].notes;
    auto low = 127, high = 0;
    for (const auto& n : notes)
        if (! n.keyswitch)
        {
            low = juce::jmin (low, n.pitch);
            high = juce::jmax (high, n.pitch);
        }
    low -= 2;
    high += 2;
    const auto body = r.reduced (0.0f, 4.0f);
    const auto row = body.getHeight() / (float) juce::jmax (1, high - low + 1);
    const auto clipX = (float) g.getClipBounds().getX(), clipRight = (float) g.getClipBounds().getRight();
    for (const auto& n : notes)
    {
        if (n.keyswitch)
            continue;   // (a keyswitch held: named on the lane instead)
        if (xFor (n.beat + n.length) < clipX - 2.0f || xFor (n.beat) > clipRight + 2.0f)
            continue;   // (0.175.0: outside the redrawn strip)
        const auto y = body.getBottom() - (float) (n.pitch - low + 1) * row;
        g.setColour (colour.withAlpha ((s.on ? 0.45f + 0.55f * n.velocity : 0.18f)));
        g.fillRoundedRectangle (juce::Rectangle<float> (xFor (n.beat), y, juce::jmax (2.0f, xFor (n.beat + n.length) - xFor (n.beat) - 1.0f),
                                                        juce::jmax (2.0f, row - 1.0f)), 1.5f);
    }
    for (const auto& n : notes)
        if (n.keyswitch)
        {
            g.setColour (ParagonColours::textDim);
            g.setFont (ParagonLookAndFeel::titleFont (12.0f));
            g.drawText ("KEYSWITCH " + pr7s::noteName (n.pitch) + " HELD (STRING " + juce::String (s.onlyString) + ")",
                        lane.notes.reduced (10, 6), juce::Justification::bottomRight, false);
            break;
        }
    if (! s.on)
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::titleFont (13.0f));
        g.drawText ("OFF", lane.notes.reduced (10, 6), juce::Justification::topRight, false);
    }
    if (hearing && studio.isComposerAuditioning())
    {
        const auto beat = const_cast<pr7s::Studio&> (studio).getEngine().getPosition() - studio.getComposerAuditionStart();
        if (beat >= 0.0 && beat <= total)
        {
            g.setColour (juce::Colour (0xfff4eeea));
            g.fillRect (xFor (beat), r.getY(), 1.4f, r.getHeight());
        }
    }
}

void ComposerPage::resized()
{
    auto area = getLocalBounds().reduced (gap, 0);
    auto head = area.removeFromTop (headerHeight).reduced (0, 14);
    buildKey.setBounds (head.removeFromRight (190));
    head.removeFromRight (6);
    sectionBox.setBounds (head.removeFromRight (170));
    head.removeFromRight (6);
    typeKey.setBounds (head.removeFromRight (180));
    head.removeFromRight (6);
    pasteKey.setBounds (head.removeFromRight (120));
    head.removeFromRight (6);
    mixerKey.setBounds (head.removeFromRight (80));
    head.removeFromRight (6);
    syncKey.setBounds (head.removeFromRight (140));
    head.removeFromRight (12);
    generateKey.setBounds (head.removeFromRight (130));
    head.removeFromRight (6);
    useKey.setBounds (head.removeFromRight (120));
    head.removeFromRight (6);
    hearKey.setBounds (head.removeFromRight (90));

    auto controls = area.removeFromTop (controlsHeight).withTrimmedBottom (8);
    for (auto* box : { &keyBox, &genreBox, &moodBox, &lengthBox, &styleBox })
    {
        box->setBounds (controls.removeFromLeft (box == &keyBox ? 200 : 150));
        controls.removeFromLeft (8);
    }
    toChordTrackKey.setBounds (controls.removeFromRight (150));
    controls.removeFromRight (8);
    myRiffKey.setBounds (controls.removeFromRight (100));
    controls.removeFromRight (6);
    chordTrackKey.setBounds (controls.removeFromRight (130));
    controls.removeFromRight (6);
    libraryKey.setBounds (controls.removeFromRight (100));
    controls.removeFromRight (6);
    controlsLabels = controls.removeFromRight (100);
    controls.removeFromRight (8);
    progressionKey.setBounds (controls);

    songArea = area.removeFromTop (songHeight).withTrimmedBottom (8);
    {
        auto keys = songArea.reduced (10, 9);
        for (int i = nextKeys.size(); --i >= 0;)
        {
            nextKeys[i]->setBounds (keys.removeFromRight (220));
            keys.removeFromRight (8);
        }
    }
    area.removeFromBottom (gap);
    {
        rollChordsKey.setBounds (juce::Rectangle<int> (area.getX() + laneHeaderWidth - 140, area.getY() + 3, 140, rulerHeight - 7));   // (0.208.0)
    }
    ruler = area.removeFromTop (rulerHeight).withTrimmedLeft (laneHeaderWidth + gap).withTrimmedBottom (2);
    const auto laneHeight = area.getHeight() / pr7s::composerLayerCount;
    for (const auto l : pr7s::composerLayers())
    {
        auto& lane = lanes[(size_t) l];
        auto row = area.removeFromTop (laneHeight).withTrimmedBottom (6);
        lane.header = row.removeFromLeft (laneHeaderWidth);
        row.removeFromLeft (gap);
        lane.notes = row;
        // (0.120.0) Always two rows of keys under the name (they were crammed into one in a short lane).
        auto keys = lane.header.reduced (16, 6);
        lane.volume.setBounds (keys.withHeight (22).removeFromRight (190).translated (0, 1));
        keys.removeFromTop (26);
        const auto rowHeight = juce::jlimit (16, 26, (keys.getHeight() - 4) / 2);
        auto first = keys.removeFromTop (rowHeight);
        keys.removeFromTop (4);
        auto second = keys.removeFromTop (rowHeight);
        lane.note = keys.getHeight() >= 14 ? keys.removeFromBottom (14) : juce::Rectangle<int>();
        lane.on.setBounds (first.removeFromLeft (46));
        first.removeFromLeft (6);
        lane.lock.setBounds (first.removeFromLeft (56));
        first.removeFromLeft (6);
        lane.roll.setBounds (first.removeFromLeft (56));
        first.removeFromLeft (6);
        lane.bars.setBounds (first.removeFromLeft (96));   // (0.202.0)
        first.removeFromLeft (6);
        lane.style.setBounds (first);
        if (l != ComposerLayer::drums)
        {
            lane.octave.setBounds (second.removeFromRight (94));
            second.removeFromRight (6);
        }
        if (pr7s::hasLowString (l))
        {
            lane.low.setBounds (second.removeFromRight (118));
            second.removeFromRight (6);
        }
        if (l == ComposerLayer::acoustic)
        {
            lane.second.setBounds (second.removeFromRight (170));
            second.removeFromRight (6);
        }
        lane.busy.setBounds (second.removeFromRight (88));
        second.removeFromRight (6);
        lane.track.setBounds (second);
    }
}

//==============================================================================
ComposerSuite::ComposerSuite (pr7s::Studio& s) : studio (s), composer (s)
{
    addAndMakeVisible (composer);
    ideas = std::make_unique<LinesPage> (studio, composer.spansAt (composer.buildStart()));
    addChildComponent (*ideas);
    ideas->onComposer = [this] { showTab (Tab::compose); };
    ideas->onToComposer = [this] (const pr7s::LineIdea& idea)
    {
        composer.takeIdea (idea);
        showTab (Tab::compose);
    };
    ideas->composerLaneOf = [] (pr7s::LineIdea::Part part)
    {
        const auto l = ComposerPage::layerOf (part);
        return l.has_value() ? pr7s::composerLayerName (*l) : juce::String();
    };
    composer.onSongChanged = [this]   // (0.206.0) another song: IDEAS on its chords, nothing heard
    {
        ideasOnOtherChords = false;
        if (ideas != nullptr)
        {
            ideas->stop();
            ideas->setChords (composer.spansAt (composer.buildStart()));
        }
    };
    composer.onChordsChanged = [this] (const std::vector<pr7s::ChordSpan>& spans)
    {
        ideasOnOtherChords = false;
        if (ideas != nullptr)
            ideas->setChords (spans);
    };
    for (auto* key : { &composeTab, &ideasTab })
    {
        key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key->setWantsKeyboardFocus (false);
        addAndMakeVisible (*key);
    }
    composeTab.setTooltip ("The Architect: every layer stacked over the chords, SECTION and BUILD, the song so far");
    ideasTab.setTooltip ("Every IDEAS card and setting (piano, guitar, bass, patterns, melody, drums) for the Architect's chords. "
                         "A card's USE IT menu puts it into its Architect lane");
    composeTab.onClick = [this] { showTab (Tab::compose); };
    ideasTab.onClick = [this] { showTab (Tab::ideas); };
    if (auto* settings = studio.getSettings())
        tab = settings->getValue ("composerTab") == "ideas" ? Tab::ideas : Tab::compose;
    setSize (composer.getWidth(), composer.getHeight() + tabsHeight);
    showTab (tab);
}

void ComposerSuite::showTab (Tab t)
{
    tab = t;
    if (t == Tab::ideas && ! ideasOnOtherChords)
    {
        // IDEAS on the composer's chords where the next BUILD goes (USE IT onto a track lands there).
        const auto placed = composer.spansAt (composer.buildStart());
        const auto& now = ideas->getChords();
        if (placed.empty() != now.empty() || (! placed.empty() && std::abs (placed.front().start - now.front().start) > 1.0e-6))
            ideas->setChords (placed);
    }
    if (t == Tab::ideas)
        composer.stop();
    else
        ideas->stop();
    composer.setVisible (t == Tab::compose);
    ideas->setVisible (t == Tab::ideas);
    composeTab.setToggleState (t == Tab::compose, juce::dontSendNotification);
    ideasTab.setToggleState (t == Tab::ideas, juce::dontSendNotification);
    if (auto* settings = studio.getSettings())
        settings->setValue ("composerTab", t == Tab::ideas ? "ideas" : "compose");
}

void ComposerSuite::showIdeas (std::vector<pr7s::ChordSpan> spans)
{
    if (! spans.empty())
    {
        ideasOnOtherChords = true;
        ideas->setChords (std::move (spans));
    }
    else
        ideasOnOtherChords = false;
    showTab (Tab::ideas);
}

void ComposerSuite::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0a0d));
    auto strip = getLocalBounds().removeFromTop (tabsHeight);
    g.setColour (juce::Colour (0xff141015));
    g.fillRect (strip);
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (strip.removeFromBottom (1));
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::titleFont (13.0f));
    g.drawText ("SONGWRITING: SET THE CHORDS, STYLE AND SECTION, BUILD IT, THEN THE NEXT", strip.withTrimmedLeft (300).reduced (12, 0),
                juce::Justification::centredLeft, true);
}

bool ComposerSuite::isInterestedInPart (int partId)
{
    return composer.canTakePart (partId);
}

void ComposerSuite::partDragHover (bool over)
{
    if (std::exchange (partHovering, over) != over)
        repaint();
}

void ComposerSuite::partDropped (int partId)
{
    partDragHover (false);
    showTab (Tab::compose);
    if (auto* window = getTopLevelComponent())
        window->toFront (true);
    composer.dropPart (partId);
}

void ComposerSuite::paintOverChildren (juce::Graphics& g)
{
    if (! partHovering)
        return;
    // Lit in the family's gradient while a part is held over it, and what letting go does.
    const auto area = getLocalBounds().toFloat().reduced (3.0f);
    g.setGradientFill (ParagonLookAndFeel::familyGradient (area.getTopLeft(), area.getTopRight()));
    g.drawRoundedRectangle (area, 8.0f, 4.0f);
    auto note = getLocalBounds().withSizeKeepingCentre (620, 64);
    g.setColour (juce::Colour (0xe0100c11));
    g.fillRoundedRectangle (note.toFloat(), 10.0f);
    g.setColour (juce::Colours::white);
    g.setFont (ParagonLookAndFeel::titleFont (26.0f));
    g.drawText ("LET GO TO BUILD A SECTION AROUND THIS RIFF", note, juce::Justification::centred, false);
}

void ComposerSuite::resized()
{
    auto area = getLocalBounds();
    auto strip = area.removeFromTop (tabsHeight).reduced (12, 6);
    composeTab.setBounds (strip.removeFromLeft (130));
    strip.removeFromLeft (8);
    ideasTab.setBounds (strip.removeFromLeft (130));
    composer.setBounds (area);
    ideas->setBounds (area);
}

//==============================================================================
ComposerWindow::ComposerWindow (pr7s::Studio& studio)
    : juce::DocumentWindow ("Paragon PR7-S: Architect", juce::Colour (0xff0c0a0d), juce::DocumentWindow::closeButton)
{
    suite = new ComposerSuite (studio);
    setContentOwned (suite, true);
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (1100, 700, 3840, 2160);
}

void ComposerWindow::closeButtonPressed()
{
    suite->getComposer().stop();
    suite->getComposer().closeOptionBrowser();   // (0.176.0: the window is kept hidden, not taken down: its browser goes)
    suite->getIdeas().stop();
    if (onClose)
        onClose();
}
