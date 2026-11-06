#include "ui/ComposerPage.h"

#include "ui/ChordUi.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    using pr7s::ComposerLayer;

    juce::Font interFont (float height, bool bold = false) { return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain)); }

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
    libraryKey.onClick = [this] { setFromChordTrack (false); };
    chordTrackKey.onClick = [this] { setFromChordTrack (true); };
    toChordTrackKey.onClick = [this] { setToChordTrack (! state.toChordTrack); };

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
        lane.busy.onClick = [this, l] { showBusyMenu (l); };
        lane.low.onClick = [this, l] { showLowMenu (l); };
    }

    setSize (1500, 900);
    refresh (false);
    startTimerHz (30);
}

ComposerPage::~ComposerPage()
{
    if (hearing)
        studio.stopComposerAudition();
}

//==============================================================================
void ComposerPage::refresh (bool rehear)
{
    // The chords.
    const auto at = hearing ? hearingStart : placeStart();
    context = studio.getComposerContext (at);
    if (state.fromChordTrack)
        spans = studio.getLineChords();
    else
        spans = pr7s::composerSpans (state, 0.0, context.beatsPerBar);
    if (! state.fromChordTrack)
        if (const auto index = pr7s::composerProgressionIndex (state); index >= 0)
            state.progression = pr7s::progressionLibrary()[(size_t) index].name;
    chordNames.clear();
    const auto steps = state.fromChordTrack ? std::vector<pr7s::ProgressionStep> {} : pr7s::composerSteps (state);
    for (size_t i = 0; i < spans.size(); ++i)
        chordNames.add (i < steps.size() ? steps[i].name (state.key) : spans[i].chord.name (spans[i].key));
    ideas = pr7s::composeLayers (state, spans, context);
    studio.setComposerState (state.toString());

    // The keys.
    const auto& keys = pr7s::Key::all();
    keyBox.setSelectedItemIndex ((int) std::distance (keys.begin(), std::find (keys.begin(), keys.end(), state.key)), juce::dontSendNotification);
    genreBox.setText (state.genre.isEmpty() ? "ANY GENRE" : state.genre, juce::dontSendNotification);
    moodBox.setText (state.mood.isEmpty() ? "ANY MOOD" : state.mood, juce::dontSendNotification);
    lengthBox.setText (juce::String (state.bars) + " BARS", juce::dontSendNotification);
    styleBox.setText (pr7s::guitarStyleName (studio.getGuitarStyle()), juce::dontSendNotification);
    progressionKey.setButtonText (state.fromChordTrack ? juce::String ("PROGRESSION: THE CHORD TRACK'S")
                                  : state.progression.isEmpty() ? juce::String ("NO PROGRESSION FITS") : "PROGRESSION: " + state.progression.toUpperCase());
    progressionKey.setEnabled (! state.fromChordTrack);
    for (auto* box : { &keyBox, &genreBox, &moodBox, &lengthBox })
        box->setEnabled (! state.fromChordTrack);
    libraryKey.setToggleState (! state.fromChordTrack, juce::dontSendNotification);
    chordTrackKey.setToggleState (state.fromChordTrack, juce::dontSendNotification);
    toChordTrackKey.setToggleState (state.toChordTrack, juce::dontSendNotification);
    toChordTrackKey.setEnabled (! state.fromChordTrack);
    hearKey.setButtonText (hearing ? "STOP" : "HEAR");
    hearKey.setToggleState (hearing, juce::dontSendNotification);
    for (const auto l : pr7s::composerLayers())
    {
        auto& lane = lanes[(size_t) l];
        const auto& s = state.lane (l);
        lane.on.setToggleState (s.on, juce::dontSendNotification);
        lane.lock.setToggleState (s.locked, juce::dontSendNotification);
        lane.style.setButtonText (ideas[(size_t) l].style.isEmpty() ? juce::String ("STYLE") : ideas[(size_t) l].style.toUpperCase());
        lane.track.setButtonText (trackText (l));
        lane.busy.setButtonText ("BUSY " + juce::String (juce::roundToInt (s.busy * 100.0f)) + "%");
        lane.low.setToggleState (s.lowString || s.onlyString > 0, juce::dontSendNotification);
        lane.low.setButtonText (stringText (l));
    }
    seenChange = studio.getChangeVersion();

    if (rehear && hearing && ! hear())
        stop();
    repaint();
}

double ComposerPage::placeStart() const
{
    if (state.fromChordTrack)
    {
        const auto chords = studio.getLineChords();
        return chords.empty() ? 0.0 : chords.front().start;
    }
    auto& engine = const_cast<pr7s::Studio&> (studio).getEngine();
    return studio.getProject().barStartAt (juce::jmax (0.0, engine.getPosition()));
}

std::vector<pr7s::ChordSpan> ComposerPage::placedSpans() const
{
    if (state.fromChordTrack)
        return spans;
    auto placed = spans;
    const auto start = hearing ? hearingStart : placeStart();
    for (auto& s : placed)
    {
        s.start += start;
        s.end += start;
    }
    return placed;
}

//==============================================================================
void ComposerPage::generate()
{
    pr7s::generateComposer (state, random);
    status = "Generated: a new take of every layer not locked";
    refresh();
}

void ComposerPage::roll (ComposerLayer l)
{
    pr7s::rollLane (state, l, random);
    status = pr7s::composerLayerName (l) + ": a new take";
    refresh();
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

void ComposerPage::setStyle (ComposerLayer l, int style)
{
    state.lane (l).style = juce::jlimit (0, pr7s::composerStyleCount (l) - 1, style);
    refresh();
}

void ComposerPage::setKey (const pr7s::Key& k)
{
    state.key = k;
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
    state.bars = bars == 4 || bars == 16 ? bars : 8;
    refresh();
}

void ComposerPage::setProgression (const juce::String& name)
{
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
        hearingStart = placeStart();
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
    const auto placed = placedSpans();
    std::vector<pr7s::LineIdea> list;
    std::vector<int> onto;
    for (const auto l : pr7s::composerLayers())
        if (state.lane (l).on && ! ideas[(size_t) l].notes.empty())
        {
            list.push_back (ideas[(size_t) l]);
            onto.push_back (trackFor (l, false));
        }
    if (! state.fromChordTrack && state.toChordTrack)
        studio.addProgressionToChordTrack (pr7s::composerSteps (state), placed.front().start);
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
    const auto tracks = studio.buildSection ({ ideas[(size_t) l] }, placedSpans(), "Composer", { trackFor (l, false) });
    refresh (false);
    return tracks.empty() ? 0 : tracks.front();
}

juce::Button* ComposerPage::keyNamed (const juce::String& name)
{
    for (auto* key : { &hearKey, &useKey, &generateKey, &libraryKey, &chordTrackKey, &toChordTrackKey })
        if (key->getButtonText() == name)
            return key;
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
    for (int i = 0; i < (int) choices.size(); ++i)
        menu.addItem (i + 1, choices[(size_t) i].style + (choices[(size_t) i].description.isNotEmpty() ? "  (" + choices[(size_t) i].description + ")" : juce::String()),
                      true, i == state.lane (l).style);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (lanes[(size_t) l].style),
                        [safe = juce::Component::SafePointer<ComposerPage> (this), l] (int chosen)
                        {
                            if (safe != nullptr && chosen > 0)
                                safe->setStyle (l, chosen - 1);
                        });
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
                                const auto id = studio.addTrack (pr7s::Instrument {});
                                if (id != 0)
                                {
                                    studio.renameTrack (id, name);
                                    if (l == ComposerLayer::drums)
                                        studio.setTrackDrumMap (id, pr7s::DrumMap::gm().name);
                                }
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
    if (hearing)
    {
        // The playhead across the lanes.
        auto area = ruler;
        for (const auto& lane : lanes)
            area = area.getUnion (lane.notes);
        repaint (area);
    }
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
    return (state.fromChordTrack ? juce::String ("The chord track's chords") : ChordUi::spokenKey (state.key) + ", " + state.progression)
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
    g.drawText (status.isNotEmpty() ? summary() + "     " + status : summary(), head.withTrimmedRight (560), juce::Justification::centredLeft, true);

    // The labels in the controls row.
    g.setFont (ParagonLookAndFeel::titleFont (12.0f));
    g.setColour (ParagonColours::textDim);
    g.drawText ("CHORDS FROM", controlsLabels, juce::Justification::centredRight, false);

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
            g.setColour (ParagonColours::text);
            g.drawText (chordNames[(int) i], juce::Rectangle<float> (x + 3.0f, (float) ruler.getY() + 12.0f, juce::jmax (20.0f, xFor (s.end) - x - 4.0f), 17.0f),
                        juce::Justification::centredLeft, true);
        }
    }
    // (0.59.0) The hint, where the lanes' headers meet the ruler.
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::titleFont (11.0f));
    g.drawText ("CLICK THE NOTES TO HEAR, DRAG THEM OUT AS A MIDI FILE",
                juce::Rectangle<int> (gap, ruler.getY(), laneHeaderWidth, ruler.getHeight()), juce::Justification::centredLeft, true);
    for (const auto l : pr7s::composerLayers())
        paintLane (g, l, lanes[(size_t) l]);
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
        if (s.locked)
        {
            g.setColour (ParagonColours::accent);
            g.setFont (ParagonLookAndFeel::titleFont (11.0f));
            g.drawText ("LOCKED", lane.header.reduced (18, 6).removeFromTop (24), juce::Justification::centredRight, false);
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
    generateKey.setBounds (head.removeFromRight (180));
    head.removeFromRight (8);
    useKey.setBounds (head.removeFromRight (140));
    head.removeFromRight (8);
    hearKey.setBounds (head.removeFromRight (110));

    auto controls = area.removeFromTop (controlsHeight).withTrimmedBottom (8);
    for (auto* box : { &keyBox, &genreBox, &moodBox, &lengthBox, &styleBox })
    {
        box->setBounds (controls.removeFromLeft (box == &keyBox ? 200 : 150));
        controls.removeFromLeft (8);
    }
    toChordTrackKey.setBounds (controls.removeFromRight (150));
    controls.removeFromRight (8);
    chordTrackKey.setBounds (controls.removeFromRight (130));
    controls.removeFromRight (6);
    libraryKey.setBounds (controls.removeFromRight (100));
    controls.removeFromRight (6);
    controlsLabels = controls.removeFromRight (100);
    controls.removeFromRight (8);
    progressionKey.setBounds (controls);

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
ComposerWindow::ComposerWindow (pr7s::Studio& studio)
    : juce::DocumentWindow ("Paragon PR7-S: Composer", juce::Colour (0xff0c0a0d), juce::DocumentWindow::closeButton)
{
    page = new ComposerPage (studio);
    setContentOwned (page, true);
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (1100, 700, 3840, 2160);
}

void ComposerWindow::closeButtonPressed()
{
    page->stop();
    if (onClose)
        onClose();
}
