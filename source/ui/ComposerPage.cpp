#include "ui/ComposerPage.h"
#include "engine/SectionTypes.h"

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
        return l == ComposerLayer::bass || l == ComposerLayer::guitar ? pr7s::FamilyColours::orange
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
    state = pr7s::ComposerState::fromString (studio.getComposerState());

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
    mixerKey.setTooltip ("The mixer in its own window over the composer: faders, inserts, EQ while you hear the layers (they keep playing)");
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
        for (auto* key : { &lane.on, &lane.lock, &lane.roll, &lane.style, &lane.track, &lane.busy, &lane.low })
        {
            pedal (*key, key == &lane.style || key == &lane.track || key == &lane.busy || key == &lane.low);
            if (key != &lane.low || pr7s::hasLowString (l))
                addAndMakeVisible (*key);
        }
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
        lane.style.onClick = [this, l] { showStyleMenu (l); };
        lane.track.onClick = [this, l] { showTrackMenu (l); };
        // (0.76.0) VOLUME: the fader of the track the lane is heard on (as the mixer's; hearing goes on).
        lane.volume.setRange (-60.0, 6.0, 0.5);
        lane.volume.setSkewFactorFromMidPoint (-12.0);
        lane.volume.setDoubleClickReturnValue (true, 0.0);
        lane.volume.setTextValueSuffix (" dB");
        lane.volume.setNumDecimalPlacesToDisplay (1);
        lane.volume.setTextBoxStyle (juce::Slider::TextBoxLeft, true, 70, 20);
        lane.volume.setTooltip ("The volume of the track this lane plays on: the same as its fader in the mixer. Double-click: 0 dB");
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
    startTimerHz (30);
}

ComposerPage::~ComposerPage()
{
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
    const auto at = hearing ? hearingStart : placeStart();
    context = studio.getComposerContext (at);
    const auto& riff = studio.getComposerRiff();
    if (fromRiff && ! riff.has_value())
        fromRiff = false;
    const auto target = getTargetPlace();
    {   // (0.104.0) The block's transition (none outside the SECTIONS row).
        const auto& sections = studio.getSongSections();
        const auto index = sections.indexOf (targetBlock);
        state.transition = index >= 0 ? sections.blocks[(size_t) index].transition : juce::String();
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
        context.myRiff = pr7s::tileRiff (*riff, spans.back().end - spans.front().start, context.beatsPerBar);
    if (const auto fingerprint = composeFingerprint(); fingerprint != composedFrom)
    {
        composed = pr7s::composeLayers (state, spans, context);
        composedFrom = fingerprint;
    }
    ideas = composed;
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
    studio.setComposerState (state.toString());

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
    for (auto* box : { &genreBox, &moodBox })
        box->setEnabled (! state.fromChordTrack && ! fromRiff);
    lengthBox.setEnabled (! state.fromChordTrack && ! getTargetPlace().has_value());   // (0.90.0: a block's length is its own)
    // (0.75.0) The key locked for the song once a section is built.
    keyBox.setEnabled (! state.fromChordTrack && ! isKeyLocked());
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
                                  : approach.isNotEmpty() ? state.sectionType + ": " + approach
                                  : ideas[(size_t) l].style.isEmpty() ? juce::String ("STYLE") : ideas[(size_t) l].style.toUpperCase());
        lane.track.setButtonText (trackText (l));
        lane.busy.setButtonText ("BUSY " + juce::String (juce::roundToInt (s.busy * 100.0f)) + "%");
        lane.low.setToggleState (s.lowString || s.onlyString > 0, juce::dontSendNotification);
        lane.low.setButtonText (stringText (l));
    }
    seenChange = studio.getChangeVersion();
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
    if (l == ComposerLayer::guitar)
        useMyRiff = false;
    status = pr7s::composerLayerName (l) + ": " + (state.lane (l).approach >= 0 ? names[state.lane (l).approach] : juce::String ("AUTOMATIC"));
    refresh();
}

void ComposerPage::showTypeMenu()
{
    const auto name = typeSectionName();
    juce::PopupMenu menu;
    menu.addSectionHeader ("TYPE OF " + name.toUpperCase());
    menu.addItem (1, juce::String (pr7s::standardSectionType) + "  (every lane its own styles)", true, state.sectionType.isEmpty());
    const auto types = pr7s::sectionTypesFor (name);
    for (int i = 0; i < (int) types.size(); ++i)
        menu.addItem (2 + i, types[(size_t) i].name + "  (" + types[(size_t) i].description + ")", true, types[(size_t) i].name == state.sectionType);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (typeKey),
                        [safe = juce::Component::SafePointer<ComposerPage> (this), types] (int chosen)
                        {
                            if (safe == nullptr || chosen <= 0)
                                return;
                            safe->setSectionType (chosen == 1 ? juce::String() : types[(size_t) (chosen - 2)].name);
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
    status = "Generated: a new take of every layer not locked";
    refresh();
}

void ComposerPage::roll (ComposerLayer l)
{
    taken[(size_t) l].reset();
    pr7s::rollLane (state, l, random);
    if (l == ComposerLayer::guitar)
        takeNewRiff();
    status = pr7s::composerLayerName (l) + ": a new take";
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
    refresh();
}

void ComposerPage::setKey (const pr7s::Key& k)
{
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

juce::String ComposerPage::trackText (ComposerLayer l) const
{
    const auto writes = trackFor (l, false);
    if (writes != 0)
        if (const auto* t = studio.getProject().findTrack (writes))
            return "TRACK: " + t->name.toUpperCase();
    return "TRACK: NEW (" + pr7s::Studio::linePartName (pr7s::composerPartOf (l)).toUpperCase() + ")";
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
    for (const auto l : pr7s::composerLayers())
        if (state.lane (l).on)
            if (const auto t = trackFor (l, true))
                layers.emplace_back (t, ideas[(size_t) l]);
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
    hearKey.setButtonText ("STOP");
    hearKey.setToggleState (true, juce::dontSendNotification);
    repaint();
    return true;
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
    std::vector<pr7s::LineIdea> list;
    std::vector<int> onto;
    for (const auto l : pr7s::composerLayers())
        if (state.lane (l).on && ! ideas[(size_t) l].notes.empty())
        {
            list.push_back (ideas[(size_t) l]);
            onto.push_back (trackFor (l, false));
        }
    writeChordsAt (placed);
    const auto tracks = studio.buildSection (list, placed, "Composer", onto);
    juce::StringArray names;
    for (const auto id : tracks)
        if (const auto* t = studio.getProject().findTrack (id))
            names.add (t->name);
    const auto bar = studio.getProject().signatures().barAt (placed.front().start) + 1;
    status = tracks.empty() ? juce::String ("Nothing written: switch a layer on")
                            : "Written at bar " + juce::String (bar) + " onto " + names.joinIntoString (", ");
    refresh (false);
    return tracks;
}

int ComposerPage::useLayer (ComposerLayer l)
{
    stop();
    if (spans.empty() || ideas[(size_t) l].notes.empty())
        return 0;
    const auto tracks = studio.buildSection ({ ideas[(size_t) l] }, spansAt (placeStart()), "Composer", { trackFor (l, false) });
    refresh (false);
    return tracks.empty() ? 0 : tracks.front();
}

//==============================================================================
// (0.76.0) Each lane's volume: the fader of the track it's heard on.
void ComposerPage::setLaneVolume (ComposerLayer l, float db, bool undoable)
{
    const auto id = trackFor (l, true);
    if (id == 0)
        return;
    if (const auto* t = studio.getProject().findTrack (id); t != nullptr && std::abs (t->volumeDb - db) < 0.01f && ! undoable)
        return;
    studio.setVolume (id, db, undoable);
}

void ComposerPage::refreshVolumes()
{
    for (const auto l : pr7s::composerLayers())
    {
        auto& v = lanes[(size_t) l].volume;
        const auto id = trackFor (l, true);
        const auto* t = id != 0 ? studio.getProject().findTrack (id) : nullptr;
        v.setEnabled (t != nullptr);
        if (t != nullptr && std::abs (v.getValue() - (double) t->volumeDb) > 0.01)
            v.setValue (t->volumeDb, juce::dontSendNotification);
    }
}

//==============================================================================
// (0.75.0) The key locked, chords per section, your own riff.
bool ComposerPage::isKeyLocked() const
{
    return ! studio.getBuiltSections().empty();
}

std::vector<pr7s::ChordSpan> ComposerPage::chordSpans() const
{
    if (state.fromChordTrack)
        return studio.getLineChords();
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
        taken[(size_t) ComposerLayer::guitar].reset();
        state.lane (ComposerLayer::guitar).on = true;
    }
    refresh();
}

bool ComposerPage::pasteRiff (pr7s::ComposerRiff riff)
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
    studio.setComposerRiff (riff);
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
    fromRiff = true;
    state.fromChordTrack = false;
    useMyRiff = true;
    chordOverrides.clear();
    taken[(size_t) ComposerLayer::guitar].reset();
    state.lane (ComposerLayer::guitar).on = true;
    // The section's beat, and the riff's settings back to as written.
    studio.setRiffSettings ({});
    setSection (section);
    status = "Your riff (" + juce::String (bars) + (bars == 1 ? " bar" : " bars") + ") as the " + pr7s::sectionWord (section).toLowerCase() + "; " + keyText;
    repaint();
    return true;
}

bool ComposerPage::pasteRiffFromPart (int partId, pr7s::DrumSection section)
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
    return pasteRiff (pr7s::riffFromNotes (notes, part.start, bpb, track->name + (part.name.isNotEmpty() ? " - " + part.name : juce::String()), section));
}

bool ComposerPage::pasteRiffFromFile (const juce::File& file, pr7s::DrumSection section)
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
    return pasteRiff (pr7s::riffFromNotes (notes, 0.0, context.beatsPerBar, file.getFileNameWithoutExtension(), section));
}

std::vector<int> ComposerPage::buildFromPart (int partId, pr7s::DrumSection section)
{
    if (! pasteRiffFromPart (partId, section))
        return {};
    const auto riffStatus = status;
    auto tracks = buildSection();
    if (! tracks.empty())
        status = riffStatus + ".  " + status;
    repaint();
    return tracks;
}

std::vector<int> ComposerPage::buildFromFile (const juce::File& file, pr7s::DrumSection section)
{
    if (! pasteRiffFromFile (file, section))
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
    askSection ([safe = juce::Component::SafePointer<ComposerPage> (this), partId] (pr7s::DrumSection sec)
                { if (safe != nullptr) safe->buildFromPart (partId, sec); }, &pasteKey);
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
    menu.addItem (1, partId != 0 ? "From the Selected Part (" + partName + ")" : juce::String ("From the Selected Part (select a MIDI part first)"), partId != 0);
    menu.addItem (2, "From a MIDI File...");
    if (riff.has_value())
    {
        menu.addSeparator();
        menu.addItem (3, "The Guitar Plays My Riff", true, useMyRiff);
        menu.addItem (4, "The Chords from My Riff", true, fromRiff);
        menu.addItem (5, "Forget My Riff (" + riff->name + ")");
    }
    menu.addSeparator();
    menu.addItem (6, "Learn My Band: Read Our Record...", onLearnMyBand != nullptr);   // (0.82.0)
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (pasteKey),
                        [safe = juce::Component::SafePointer<ComposerPage> (this), partId] (int chosen)
                        {
                            if (safe == nullptr || chosen == 0)
                                return;
                            if (chosen == 6)
                            {
                                if (safe->onLearnMyBand)
                                    safe->onLearnMyBand();
                                return;
                            }
                            if (chosen == 1)
                                safe->askSection ([safe, partId] (pr7s::DrumSection sec) { if (safe != nullptr) safe->pasteRiffFromPart (partId, sec); }, &safe->pasteKey);
                            else if (chosen == 2)
                            {
                                safe->chooser = std::make_unique<juce::FileChooser> ("Your riff (a MIDI file)", juce::File(), "*.mid;*.midi");
                                safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                                            [safe] (const juce::FileChooser& fc)
                                                            {
                                                                const auto file = fc.getResult();
                                                                if (safe == nullptr || ! file.existsAsFile())
                                                                    return;
                                                                juce::MessageManager::callAsync ([safe, file]
                                                                {
                                                                    if (safe != nullptr)
                                                                        safe->askSection ([safe, file] (pr7s::DrumSection sec) { if (safe != nullptr) safe->pasteRiffFromFile (file, sec); }, &safe->pasteKey);
                                                                });
                                                            });
                            }
                            else if (chosen == 3)
                                safe->setPlayMyRiff (! safe->useMyRiff);
                            else if (chosen == 4)
                                safe->setFromRiff (! safe->fromRiff);
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
            askSection ([safe = juce::Component::SafePointer<ComposerPage> (this), file] (pr7s::DrumSection sec)
                        { if (safe != nullptr) safe->buildFromFile (file, sec); }, nullptr);
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
    if (! guitar.locked && ! taken[(size_t) ComposerLayer::guitar].has_value())
    {
        // (0.75.0) Your riff: reshaped by the suggestion's riff settings, or a new riff for contrast.
        useMyRiff = studio.getComposerRiff().has_value() && next.riff == myRiffIndex;
        if (! useMyRiff)
            guitar.style = juce::jlimit (0, pr7s::composerStyleCount (ComposerLayer::guitar) - 1, next.riff);
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
    std::vector<pr7s::LineIdea> list;
    std::vector<int> onto;
    std::vector<int> layersBuilt;   // (0.92.0) which lane each of `list` is
    for (const auto l : pr7s::composerLayers())
        if (state.lane (l).on && ! ideas[(size_t) l].notes.empty())
        {
            list.push_back (ideas[(size_t) l]);
            onto.push_back (trackFor (l, false));
            layersBuilt.push_back ((int) l);
        }
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
    for (const auto l : pr7s::composerLayers())
    {
        auto& lane = lanes[(size_t) l];
        const auto prefix = pr7s::composerLayerName (l) + " ";
        if (name == prefix + "ON") return &lane.on;
        if (name == prefix + "LOCK") return &lane.lock;
        if (name == prefix + "ROLL") return &lane.roll;
        if (name == prefix + "STYLE") return &lane.style;
        if (name == prefix + "TRACK") return &lane.track;
        if (name == prefix + "BUSY") return &lane.busy;
        if (name == prefix + "LOW") return &lane.low;
    }
    return nullptr;
}

//==============================================================================
void ComposerPage::showStyleMenu (ComposerLayer l)
{
    const auto choices = pr7s::composerChoices (l, state, spans, context);
    juce::PopupMenu menu;
    // (0.97.1) A lane the section type writes: its options are the type's approaches, each a different part.
    const auto typeWrites = pr7s::sectionTypeWritesLane (l, state, context);
    for (int i = 0; i < (int) choices.size(); ++i)
        menu.addItem (i + 1, choices[(size_t) i].style + (choices[(size_t) i].description.isNotEmpty() ? "  (" + choices[(size_t) i].description + ")" : juce::String()),
                      true, i == state.lane (l).style);
    // (0.94.0) The section type's approaches for this lane, first.
    if (const auto approaches = pr7s::sectionApproaches (state, l); typeWrites && ! approaches.isEmpty())
    {
        juce::PopupMenu typed;
        typed.addSectionHeader (state.sectionType.isNotEmpty() ? state.sectionType : juce::String ("DRONING"));
        typed.addItem (approachItems, "Automatic (the take chooses)", true, state.lane (l).approach < 0);
        for (int i = 0; i < approaches.size(); ++i)
        {
            const auto& idea = i < (int) choices.size() ? choices[(size_t) i] : pr7s::LineIdea {};
            typed.addItem (approachItems + 1 + i, approaches[i] + (idea.description.isNotEmpty() ? "  (" + idea.description + ")" : juce::String()),
                           true, state.lane (l).approach == i);
        }
        menu = typed;
    }
    else if (const auto approaches = pr7s::sectionApproaches (state, l); ! approaches.isEmpty())
    {
        juce::PopupMenu typed;
        typed.addSectionHeader (state.sectionType.isNotEmpty() ? state.sectionType : juce::String ("DRONING"));
        typed.addItem (approachItems, "Automatic (the style and the take choose)", true, state.lane (l).approach < 0);
        for (int i = 0; i < approaches.size(); ++i)
            typed.addItem (approachItems + 1 + i, approaches[i], true, state.lane (l).approach == i);
        juce::PopupMenu whole;
        whole.addSubMenu ("Approach: " + pr7s::approachName (state, l), typed);
        whole.addSeparator();
        whole.addSectionHeader ("STYLES");
        for (int i = 0; i < (int) choices.size(); ++i)
            whole.addItem (i + 1, choices[(size_t) i].style + (choices[(size_t) i].description.isNotEmpty() ? "  (" + choices[(size_t) i].description + ")" : juce::String()),
                           true, i == state.lane (l).style);
        menu = whole;
    }
    // (0.70.0, the owner asked) The lead's last choice is the whole solo: its SOLO STYLE chosen here too (the
    // same setting as IDEAS' SOLO box).
    if (l == ComposerLayer::lead)
    {
        juce::PopupMenu solos;
        const auto now = studio.getSoloStyle();
        for (int i = 0; i < (int) pr7s::soloStyles().size(); ++i)
        {
            const auto st = pr7s::soloStyles()[(size_t) i];
            solos.addItem (soloItems + i, pr7s::soloStyleName (st) + "  (" + pr7s::soloStyleDescription (st) + ")", true, st == now);
        }
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
            flavours.addItem (flavourItems + i, pr7s::riffFlavourName (f) + "  (" + pr7s::riffFlavourDescription (f) + ")", true,
                              ! state.writtenRiffs && i == state.riffFlavour);
        }
        menu.addSeparator();
        if (pr7s::sectionBand (state) == pr7s::SectionType::Band::clean || pr7s::sectionBand (state) == pr7s::SectionType::Band::preChorus
            || pr7s::sectionBand (state) == pr7s::SectionType::Band::bridge
            || pr7s::sectionBand (state) == pr7s::SectionType::Band::breakdown)   // (0.97.0: the clean parts aren't riffs; 0.102.0: nor the pre-chorus's)
            menu.addSubMenu ("Riff Flavour: (the type writes its own guitar part)", flavours, false);
        else if (const auto typed = pr7s::typedGuitarFlavour (state))   // (0.94.0: the type's approach picks the riff kind)
            menu.addSubMenu ("Riff Flavour: " + pr7s::riffFlavourName (*typed) + " (the type's approach)", flavours, false);
        else
            menu.addSubMenu ("Riff Flavour: " + (state.writtenRiffs ? juce::String ("(written riffs)")
                                                                    : pr7s::riffFlavourName (pr7s::riffFlavours()[(size_t) state.riffFlavour])), flavours);
        menu.addItem (writtenRiffsItem, "Written Riffs (the six of each style, not generated)", true, state.writtenRiffs);
    }
    if (l == ComposerLayer::guitar && studio.getComposerRiff().has_value())
    {
        menu.addSeparator();
        menu.addItem (myRiffItem, "My Riff (" + studio.getComposerRiff()->name + ")", true, useMyRiff);
    }
    // (0.106.0, the owner) The drums from the groove library: on by default; the grooves for this section to choose from.
    if (l == ComposerLayer::drums)
    {
        menu.addSeparator();
        menu.addItem (libraryItems, "Grooves from My Library (PR7-D's groove library, else the MediaBay's drum MIDI)", true, state.libraryDrums);
        // (0.108.0, the studio link) PR7-D plays the drums from its own song track (the track keeps a muted copy).
        menu.addItem (libraryItems + 90, "PR7-D Plays the Drums from Its Song Track (on a track playing PR7-D)", true, studio.getPr7dPlaysDrums());
        const auto grooves = pr7s::composerLibraryGrooves (state, context);
        juce::PopupMenu list;
        list.addItem (libraryItems + 1, "Automatic (the section chooses, ROLL gives another)", state.libraryDrums, state.libraryGroove.isEmpty());
        for (int i = 0; i < (int) grooves.size(); ++i)
            list.addItem (libraryItems + 2 + i, grooves[(size_t) i].name + "  (" + grooves[(size_t) i].folder + ")", state.libraryDrums,
                          state.libraryGroove == grooves[(size_t) i].file.getFullPathName());
        const auto playing = pr7s::composerLibraryGroove (state, context);
        menu.addSubMenu ("Library Groove: " + (grooves.empty() ? juce::String ("(none found: scan your grooves in the MediaBay)")
                                                               : playing.has_value() ? playing->name : juce::String ("(off)")),
                         list, ! grooves.empty());
    }
    // (0.86.0) The bass and the drums under SYNC: every hit of the sync lane.
    if (pr7s::hasDoubleSync (l))
    {
        const auto sync = state.syncLayer();
        menu.addSeparator();
        menu.addItem (doubleSyncItem, sync.has_value() && *sync != l ? "Double the Sync Track (the " + pr7s::composerLayerName (*sync).toLowerCase() + ", every hit)"
                                                                      : juce::String ("Double the Sync Track (with SYNC on)"),
                      sync.has_value() && *sync != l, state.lane (l).doubleSync);
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (lanes[(size_t) l].style),
                        [safe = juce::Component::SafePointer<ComposerPage> (this), l] (int chosen)
                        {
                            if (safe == nullptr || chosen <= 0)
                                return;
                            if (chosen >= libraryItems && chosen < libraryItems + 100)
                            {
                                if (chosen == libraryItems)
                                    safe->setLibraryDrums (! safe->isUsingLibraryDrums());
                                else if (chosen == libraryItems + 90)
                                    safe->studio.setPr7dPlaysDrums (! safe->studio.getPr7dPlaysDrums());
                                else if (chosen == libraryItems + 1)
                                    safe->setLibraryGroove ({});
                                else
                                {
                                    const auto grooves = pr7s::composerLibraryGrooves (safe->state, safe->context);
                                    const auto i = chosen - libraryItems - 2;
                                    if (juce::isPositiveAndBelow (i, (int) grooves.size()))
                                        safe->setLibraryGroove (grooves[(size_t) i].file.getFullPathName());
                                }
                            }
                            else if (chosen == doubleSyncItem)
                                safe->setDoubleSync (l, ! safe->isDoublingSync (l));
                            else if (chosen == myRiffItem)
                                safe->setPlayMyRiff (true);
                            else if (chosen == writtenRiffsItem)
                                safe->setWrittenRiffs (! safe->isUsingWrittenRiffs());
                            else if (chosen >= approachItems && chosen < approachItems + 50)
                                safe->setApproach (l, chosen - approachItems - 1);
                            else if (chosen >= flavourItems && chosen < writtenRiffsItem)
                                safe->setRiffFlavour (chosen - flavourItems);
                            else if (chosen >= soloItems)
                                safe->setSoloStyle (pr7s::soloStyles()[(size_t) juce::jlimit (0, (int) pr7s::soloStyles().size() - 1, chosen - soloItems)]);
                            else
                                safe->setStyle (l, chosen - 1);
                        });
}

void ComposerPage::setLibraryDrums (bool on)
{
    state.libraryDrums = on;
    taken[(size_t) ComposerLayer::drums].reset();
    status = on ? "Drums: grooves from your library" : "Drums: the type's or the generator's";
    refresh();
}

void ComposerPage::setLibraryGroove (const juce::String& path)
{
    state.libraryGroove = path;
    state.libraryDrums = true;
    taken[(size_t) ComposerLayer::drums].reset();
    refresh();
}

void ComposerPage::showTrackMenu (ComposerLayer l)
{
    juce::PopupMenu menu;
    const auto name = pr7s::Studio::linePartName (pr7s::composerPartOf (l));
    const auto current = trackFor (l, false);
    menu.addItem (1, "Automatic: the \"" + name + "\" track, else a new one when used", true, trackChoice[(size_t) l] == 0);
    if (studio.findIdeaTrack (pr7s::composerPartOf (l)) == 0)
        menu.addItem (2, "Make the \"" + name + "\" track now (to hear it on its own instrument)");
    menu.addSeparator();
    for (const auto& t : studio.getProject().tracks)
        if (studio.canTakeMidi (t.id))
            menu.addItem (1000 + t.id, t.name, true, trackChoice[(size_t) l] != 0 && t.id == current);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (lanes[(size_t) l].track),
                        [safe = juce::Component::SafePointer<ComposerPage> (this), l, name] (int chosen)
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
                                const auto id = l == ComposerLayer::drums ? studio.addDrumsTrack (name) : studio.addTrack (pr7s::Instrument {});
                                if (id != 0 && l != ComposerLayer::drums)
                                    studio.renameTrack (id, name);
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
                            if (safe != nullptr && chosen > 0)
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

void ComposerPage::showLaneMenu (ComposerLayer l)
{
    const auto& s = state.lane (l);
    juce::PopupMenu menu;
    menu.addItem (1, "Roll (a New Take)");
    menu.addItem (2, s.locked ? "Unlock" : "Lock");
    menu.addItem (3, s.on ? "Switch Off" : "Switch On");
    menu.addSeparator();
    menu.addItem (4, "Use Only This Layer", ! ideas[(size_t) l].notes.empty());
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
    for (const auto l : pr7s::composerLayers())
        if (lanes[(size_t) l].notes.contains (e.getPosition()))
        {
            if (e.mods.isPopupMenu())
                showLaneMenu (l);
            else
                pressedLane = (int) l;   // (a click hears on release; a drag takes the lane out)
            return;
        }
}

void ComposerPage::mouseDrag (const juce::MouseEvent& e)
{
    if (pressedLane < 0 || draggedOut || e.getDistanceFromDragStart() < 6)
        return;
    draggedOut = true;
    dragLaneOut ((ComposerLayer) pressedLane);
}

void ComposerPage::mouseUp (const juce::MouseEvent&)
{
    if (pressedLane >= 0 && ! draggedOut)
        isHearing() ? stop() : (void) hear();
    pressedLane = -1;
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
    const auto name = juce::String ("Composer ") + pr7s::composerLayerName (l).toLowerCase() + " - " + idea.style;   // ("Composer bass - Walking")
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
    // The song changed (tracks added or renamed, the chord track): the lanes' tracks and chords again.
    if (! hearing && studio.getChangeVersion() != seenChange)
        refresh (false);
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
    g.setColour (ParagonColours::accent);
    g.fillRect (0, 0, getWidth(), 3);
    auto head = getLocalBounds().removeFromTop (headerHeight).reduced (gap, 8);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (20.0f));
    g.drawText ("COMPOSER", head.removeFromTop (26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (interFont (12.5f));
    g.drawText (status.isNotEmpty() ? summary() + "     " + status : summary(), head.withTrimmedRight (1100), juce::Justification::centredLeft, true);

    // The labels in the controls row.
    g.setFont (ParagonLookAndFeel::titleFont (12.0f));
    g.setColour (ParagonColours::textDim);
    g.drawText ("CHORDS FROM", controlsLabels, juce::Justification::centredRight, false);

    paintSong (g);

    // The ruler: bars, and the chords' names where each starts.
    if (! spans.empty() && ! ruler.isEmpty())
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
                juce::Rectangle<int> (gap, ruler.getY(), laneHeaderWidth, ruler.getHeight()), juce::Justification::centredLeft, true);
    for (const auto l : pr7s::composerLayers())
        paintLane (g, l, lanes[(size_t) l]);
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
    {
        const auto r = lane.header.toFloat();
        g.setColour (juce::Colour (0xff141015));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (s.locked ? ParagonColours::accent : ParagonColours::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, s.locked ? 1.5f : 1.0f);
        g.setColour (colour.withAlpha (s.on ? 1.0f : 0.35f));
        g.fillRoundedRectangle (r.withWidth (4.0f).reduced (0.0f, 8.0f).withX (r.getX() + 6.0f), 1.5f);
        g.setColour (s.on ? ParagonColours::text : ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::titleFont (20.0f));
        g.drawText (pr7s::composerLayerName (l), lane.header.reduced (18, 6).removeFromTop (24), juce::Justification::centredLeft, false);
        const auto sync = state.syncLayer();
        if (s.locked || sync.has_value())
        {
            // (0.86.0) SYNC: the leader says so; a follower says whom it follows.
            const auto word = sync == l ? juce::String ("SYNC LEADER") + (s.locked ? " - LOCKED" : "")
                            : sync.has_value() ? (s.doubleSync ? "DOUBLES THE " : "FOLLOWS THE ") + pr7s::composerLayerName (*sync) + (s.locked ? " - LOCKED" : "")
                                               : juce::String ("LOCKED");
            g.setColour (sync == l ? pr7s::FamilyColours::pink : ParagonColours::accent);
            g.setFont (ParagonLookAndFeel::titleFont (11.0f));
            g.drawText (word, lane.header.reduced (18, 6).removeFromTop (24).withTrimmedRight (200), juce::Justification::centredRight, false);
        }
        // Where it's heard, when that's not where it's written.
        const auto writes = trackFor (l, false), heard = trackFor (l, true);
        if (writes != heard || heard == 0)
        {
            g.setColour (ParagonColours::textDim);
            g.setFont (interFont (10.5f));
            const auto* t = studio.getProject().findTrack (heard);
            g.drawText (heard == 0 ? juce::String ("not heard: no track of its own yet (TRACK > make it)")
                                   : "heard on " + (t != nullptr ? t->name : juce::String()) + " until it has its own track",
                        lane.header.reduced (18, 4).removeFromBottom (14), juce::Justification::centredLeft, true);
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
    for (const auto& n : notes)
    {
        if (n.keyswitch)
            continue;   // (a keyswitch held: named on the lane instead)
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
    ruler = area.removeFromTop (rulerHeight).withTrimmedLeft (laneHeaderWidth + gap).withTrimmedBottom (2);
    const auto laneHeight = area.getHeight() / pr7s::composerLayerCount;
    for (const auto l : pr7s::composerLayers())
    {
        auto& lane = lanes[(size_t) l];
        auto row = area.removeFromTop (laneHeight).withTrimmedBottom (6);
        lane.header = row.removeFromLeft (laneHeaderWidth);
        row.removeFromLeft (gap);
        lane.notes = row;
        auto keys = lane.header.reduced (18, 8);
        lane.volume.setBounds (keys.withHeight (22).removeFromRight (190).translated (0, 1));
        keys.removeFromTop (28);
        const auto two = keys.getHeight() >= 60;
        auto first = keys.removeFromTop (juce::jmin (26, two ? keys.getHeight() / 2 - 3 : keys.getHeight()));
        lane.on.setBounds (first.removeFromLeft (52));
        first.removeFromLeft (6);
        lane.lock.setBounds (first.removeFromLeft (64));
        first.removeFromLeft (6);
        lane.roll.setBounds (first.removeFromLeft (64));
        first.removeFromLeft (6);
        const auto low = pr7s::hasLowString (l);
        if (two)
        {
            lane.style.setBounds (first);
            keys.removeFromTop (6);
            auto second = keys.removeFromTop (26);
            if (low)
            {
                lane.low.setBounds (second.removeFromRight (124));
                second.removeFromRight (6);
            }
            lane.busy.setBounds (second.removeFromRight (96));
            second.removeFromRight (6);
            lane.track.setBounds (second);
        }
        else
        {
            auto rest = first;
            const auto each = (rest.getWidth() - (low ? 18 : 12)) / (low ? 4 : 3);
            lane.style.setBounds (rest.removeFromLeft (each));
            rest.removeFromLeft (6);
            lane.track.setBounds (rest.removeFromLeft (each));
            rest.removeFromLeft (6);
            lane.busy.setBounds (rest.removeFromLeft (each));
            if (low)
            {
                rest.removeFromLeft (6);
                lane.low.setBounds (rest);
            }
        }
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
    composeTab.setTooltip ("The composer: every layer stacked over the chords, SECTION and BUILD, the song so far");
    ideasTab.setTooltip ("Every IDEAS card and setting (piano, guitar, bass, patterns, melody, drums) for the composer's chords. "
                         "A card's USE IT menu puts it into its composer lane");
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
    : juce::DocumentWindow ("Paragon PR7-S: Composer", juce::Colour (0xff0c0a0d), juce::DocumentWindow::closeButton)
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
    suite->getIdeas().stop();
    if (onClose)
        onClose();
}
