#pragma once

#include <deque>

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"
#include "ui/LinesPage.h"
#include "ui/PartDropTarget.h"
#include "ui/OptionBrowser.h"

// THE COMPOSER (0.58.0, the owner asked: "a combined page, all layers stacked, each with a lock and a
// re-roll, and one generate button"). engine/Composer.h says how the layers are chosen.
//
//   header      KEY, GENRE, MOOD, LENGTH, PROGRESSION (the library's that pass), STYLE (the guitar
//               and drums' style), CHORDS FROM: LIBRARY | CHORD TRACK, + CHORD TRACK; HEAR, USE ALL,
//               GENERATE (every unlocked layer rolled).
//   lanes       CHORDS, MELODY, BASS, GUITAR, LEAD, DRUMS stacked over the same bars: each with ON (heard
//               and used), LOCK, ROLL (0.61.0: a new take of the same style), its style (a menu of the six:
//               the only way the approach changes), the track it plays on, BUSY, LOW STRING (bass, guitar:
//               with the tuning) and its notes.
//   HEAR        every lane that's on, together and looped, through the song's own tracks (each lane's
//               track: chosen in its TRACK menu, else the one named for it, Bass, Drums..., else the
//               selected track; the drums only on a track of their own), those tracks alone, from the
//               playhead's bar (or under the chord track's chords). The song isn't changed: the engine
//               plays a copy. Rolling, locking, switching lanes while it plays carries on from where it is.
//   USE ALL     the lanes that are on onto their tracks at that bar (the track named for the lane is
//               reused, else a new one), one undo step; the library's chords onto the chord track too.
//
// 0.74.0 (the owner asked: "merge IDEAS into the COMPOSER ... my one stop shop songwriting suite. Set my
// chords, style, section and then build that part ... building a song part by part"):
//   SECTION     which section is being written (intro, verse, chorus, bridge, breakdown, ending, solo):
//               the drums play that section's beat (the drums lane's style is the section), SOLO also
//               switches the lead on to the whole solo.
//   BUILD       the lanes that are on written as that section onto their tracks after the last section
//               built (the first at the playhead's bar), the library's chords onto the chord track;
//               the section remembered (Studio::noteBuiltSection). Then the best next section is chosen
//               (engine/SectionPlan.h: its riff settings, the guitar's riff) and the others offered.
//   SONG        the sections built so far, in order, and NEXT: what could come next (click: chosen).
//   IDEAS       every IDEAS card and setting is the window's second tab (ComposerSuite); a card's USE IT
//               menu also puts it into its composer lane (`takeIdea`: kept, locked, until rolled).
class ComposerPage final : public juce::Component,
                           public juce::FileDragAndDropTarget,
                           private juce::Timer
{
public:
    explicit ComposerPage (pr7s::Studio&);
    ~ComposerPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    // (0.59.0) A lane's notes dragged out as a MIDI file (the system's file drag: onto a track in the
    // arrangement, the desktop, another program or a plug-in's window). `onExternalDrag` stands in for the
    // system's drag (tests). Returns the file written.
    juce::File dragLaneOut (pr7s::ComposerLayer);
    std::function<void (const juce::StringArray&)> onExternalDrag;

    // What the keys do (tests drive them).
    void generate();
    void roll (pr7s::ComposerLayer);
    void setLocked (pr7s::ComposerLayer, bool);
    void setOn (pr7s::ComposerLayer, bool);
    void setSoloStyle (pr7s::SoloStyle);   // (0.70.0) the lead lane's Solo Style (Studio::setSoloStyle), the lane on the solo
    void setStyle (pr7s::ComposerLayer, int);
    // (0.85.0) The guitar's riff generator: FLAVOUR (riffFlavours()' index) and WRITTEN RIFFS instead.
    void setRiffFlavour (int);
    void setWrittenRiffs (bool);
    // (0.86.0, the owner) SYNC: one lane leads (none: off), the others follow its rhythm; the leader is locked when
    // chosen. DOUBLE THE SYNC TRACK on the bass and the drums: every hit, the bass doubling the notes.
    void setSync (std::optional<pr7s::ComposerLayer>);
    std::optional<pr7s::ComposerLayer> getSync() const noexcept { return state.syncLayer(); }
    void setDoubleSync (pr7s::ComposerLayer, bool);
    bool isDoublingSync (pr7s::ComposerLayer l) const noexcept { return state.lane (l).doubleSync; }
    void showSyncMenu();
    int getRiffFlavour() const noexcept { return state.riffFlavour; }
    bool isUsingWrittenRiffs() const noexcept { return state.writtenRiffs; }
    void setKey (const pr7s::Key&);
    void setGenre (const juce::String&);     // (empty: any)
    void setMood (const juce::String&);
    void setBars (int);                      // 4, 8 or 16
    void setProgression (const juce::String& name);
    void setFromChordTrack (bool);
    void setToChordTrack (bool);
    void setTrack (pr7s::ComposerLayer, int trackId);   // 0: automatic (the one named for it, else new)
    void setBusy (pr7s::ComposerLayer, float);           // (0.61.0) 0 sparse .. 0.5 as usual .. 1 busy
    void setOctave (pr7s::ComposerLayer, int);           // (0.120.0) -2..+2 (not the drums)
    int getOctave (pr7s::ComposerLayer l) const { return state.lane (l).octave; }
    juce::TextButton& getOctaveKey (pr7s::ComposerLayer l) { return lanes[(size_t) l].octave; }
    juce::Rectangle<int> getLaneHeader (pr7s::ComposerLayer l) const { return lanes[(size_t) l].header; }
    void setLowString (pr7s::ComposerLayer, bool);       // (0.61.0) bass and guitar
    // (0.193.0) SECOND ACOUSTIC: what the ACOUSTIC GUITAR lane's second track plays (secondAcousticParts()).
    void setSecondAcoustic (int part);
    int getSecondAcoustic() const { return state.lane (pr7s::ComposerLayer::acoustic).second; }
    juce::TextButton& getSecondAcousticKey() { return lanes[(size_t) pr7s::ComposerLayer::acoustic].second; }
    void setOnlyString (pr7s::ComposerLayer, int);       // (0.62.0) 1.. the string (1 the lowest); 0 off
    juce::String stringText (pr7s::ComposerLayer) const; // the key's text: ANY STRING, LOW STRING, STRING 2 ONLY
    void setTuning (pr7s::ComposerLayer, const juce::String& name);   // the bass's or the guitar's
    int trackFor (pr7s::ComposerLayer, bool hearing) const;
    // (0.112.0) Every track the lane plays on (COMPOSER TRACKS' EVERY: guitar left and right), the first as trackFor's.
    std::vector<int> tracksFor (pr7s::ComposerLayer, bool hearing) const;
    juce::String trackText (pr7s::ComposerLayer) const;   // "TRACK: BASS", "TRACKS: GUITAR L + GUITAR R"
    // Each lane's idea once per track it plays on (the copies after the first a little looser), with those tracks and lanes.
    struct Placed { std::vector<pr7s::LineIdea> ideas; std::vector<int> onto, layers; };
    Placed placeLanes (double originBeat, bool hearing) const;   // (0: a new track when used / not heard)
    bool hear();                             // false: nothing to play them on
    void closeOptionBrowser();   // (0.176.0) the OPTIONS browser window, if one is open
    void stop();
    bool isHearing() const;
    std::vector<int> useAll();               // the tracks written
    int useLayer (pr7s::ComposerLayer);      // the track written (0: none)
    double placeStart() const;               // where USE puts the layers (a bar: the playhead's)
    double buildStart() const;               // (0.74.0) where BUILD and HEAR put them: after the last section built

    // (0.74.0) Sections, the song, ideas from IDEAS.
    void setSection (pr7s::DrumSection);
    pr7s::DrumSection getSection() const;
    std::vector<int> buildSection();          // the tracks written (none: nothing on, no chords)
    std::vector<pr7s::SectionSuggestion> getNextSuggestions() const;
    void chooseNext (const pr7s::SectionSuggestion&);   // its section, riff and riff settings (nothing built)
    void forgetSong();                        // the sections built are forgotten (the song's parts stay)
    // (0.90.0) A block of the SECTIONS row (its id; a double-click on it): the composer works there. HEAR, USE and
    // BUILD go to its start, LENGTH is its bars (the chords cut at its end), SECTION its kind's. Kept until another
    // block is chosen, `clearTargetSection`, or the block is gone.
    void targetSection (int blockId);
    // (0.94.0, the owner's section types) TYPE: the section's type (engine/SectionTypes.h; "STANDARD": none). On a
    // block of the SECTIONS row it's the block's (built: built again at once, or after asking when HUB SETTINGS says
    // so); else the composer's own. `setApproach`: a lane's approach within the type (-1 automatic).
    void setSectionType (const juce::String& type);
    void setLibraryDrums (bool on);                        // (0.106.0) the drums lane from the groove library
    bool isUsingLibraryDrums() const { return state.libraryDrums; }
    void setLibraryGroove (const juce::String& path);   // (empty: by the section and the take)
    void setFitKicks (bool on);   // (0.137.0)
    juce::String getSectionType() const { return state.sectionType; }
    juce::String typeSectionName() const;   // the name the types are for: the block's, else the SECTION's
    void setApproach (pr7s::ComposerLayer, int approach);
    int getApproach (pr7s::ComposerLayer l) const { return state.lane (l).approach; }
    int getStyle (pr7s::ComposerLayer l) const { return state.lane (l).style; }
    // (0.156.0, the owner chose D with C's feel filters) A lane's style key: a short menu (Automatic, the section
    // type's own options, favourites and recent, MORE OPTIONS...) and the option browser behind MORE OPTIONS with
    // everything. Options' ids: a style's number, or `approachId` + an approach's number.
    static constexpr int approachId = 100000;
    static constexpr int shortListMost = 12;   // a group longer than this is left to the browser (favourites and recent still shown)
    // (0.179.0) withParts false: the library grooves' names only (pending, made by fillGrooveOption), for the short
    // menu and the browser; the other options are always whole.
    std::vector<OptionBrowser::Option> laneOptions (pr7s::ComposerLayer, bool withParts = true) const;
    // (0.202.0, the owner: "lock bars 1-3 and 5-7, hit roll and get better fills for the end of each four bars") BAR
    // LOCKS: bars chosen on a lane (dragged across in its notes; Shift or Ctrl adds), then LOCK BARS keeps their notes
    // exactly while ROLL, GENERATE and every change rewrite the others. On the drums with bars locked, ROLL gives
    // another fill (composerFills) and the options window lists the FILLS first (ids `fillId` + the fill's place).
    void selectBars (pr7s::ComposerLayer, int firstBar, int lastBar, bool add = false);   // 0 = bar 1
    void clearBarSelection (pr7s::ComposerLayer);
    juce::uint64 getBarSelection (pr7s::ComposerLayer l) const noexcept { return barSelection[(size_t) l]; }
    juce::uint64 getLockedBars (pr7s::ComposerLayer l) const noexcept { return state.lane (l).lockedBars; }
    void lockBars (pr7s::ComposerLayer, juce::uint64 bars);
    void unlockBars (pr7s::ComposerLayer, juce::uint64 bars);
    void lockSelectedBars (pr7s::ComposerLayer);      // (LOCK BARS: every chosen bar locked, or let go if all were)
    void unlockAllBars (pr7s::ComposerLayer);
    void rollGroove (pr7s::ComposerLayer);            // ROLL as without bar locks (the drums: a new groove, no fill)
    int barCount() const;
    bool openBarsChosen() const;                      // (0.203.0) a drums bar chosen that isn't locked
    juce::String fillOfBar (int bar) const;           // (0.203.0) the fill's name in that drums bar (empty: none)
    int barAtX (pr7s::ComposerLayer, int x) const;
    juce::Rectangle<int> getBarBounds (pr7s::ComposerLayer, int bar) const;
    juce::TextButton& getBarsKey (pr7s::ComposerLayer l) { return lanes[(size_t) l].bars; }
    static constexpr int fillId = 300000;
    static inline const juce::String fillsGroup = "FILLS";
    std::vector<OptionBrowser::Option> fillOptions() const;   // (none without bar locks on the drums)
    juce::String getFill() const { return state.lane (pr7s::ComposerLayer::drums).fill; }
    juce::String optionListKey (pr7s::ComposerLayer l) const { return "lane:" + pr7s::composerLayerName (l).toLowerCase(); }
    // (0.177.0, the owner: "if it is selecting 100 riffs from my library why wouldn't I literally hear 100 different
    // options") While the drums play grooves from the library, the drums lane's options ARE the section's grooves
    // (the type's drum options sit under every one of them, so they'd all sound the same): ids `grooveId` + the
    // groove's place in the section's list (composerGroovePool), grouped Toontrack / your other packs; favourites and
    // recent kept apart from the type's options (`optionsKeyFor`).
    static constexpr int grooveId = 200000;
    static constexpr int libraryItems = 7000;     // (0.106.0) the drums' style menu: 7000 on/off, 7001 automatic, 7002 + i a groove
    static inline const juce::String toontrackGroup = "TOONTRACK", otherPacksGroup = "YOUR OTHER PACKS",
                                     favouriteFoldersGroup = "FAVOURITE FOLDERS";   // (0.178.0)
    bool showsLibraryGrooves (pr7s::ComposerLayer) const;
    std::vector<pr7s::LibraryGroove> sectionGrooves() const;   // the section's list (the one chosen by hand left out of the key)
    std::vector<OptionBrowser::Option> libraryGrooveOptions (bool withParts = true) const;
    bool fillGrooveOption (OptionBrowser::Option&) const;   // (0.179.0) a pending groove's part made (and kept)
    // (0.178.0) A groove option's folder; the favourite folder holding it (none: not in one); marked or unmarked
    // (the browser's list made again when given).
    juce::File grooveFolderOf (int grooveOptionId) const;
    juce::File favouriteFolderHolding (int grooveOptionId) const;
    void markGrooveFolder (int grooveOptionId, bool favourite, OptionBrowser* browser = nullptr);
    juce::String optionsKeyFor (pr7s::ComposerLayer l) const { return optionListKey (l) + (showsLibraryGrooves (l) ? ":library" : ""); }
    juce::PopupMenu makeStyleMenu (pr7s::ComposerLayer);
    void styleMenuChosen (pr7s::ComposerLayer, int item);
    std::unique_ptr<OptionBrowser> makeOptionBrowser (pr7s::ComposerLayer);   // (tests: shown by showOptionBrowser)
    void showOptionBrowser (pr7s::ComposerLayer);
    // (0.160.0) The browsers open in a window of their own, centred and maximized (one at a time: a new one replaces it).
    void openBrowserWindow (std::unique_ptr<OptionBrowser>);
    OptionBrowserWindow* getBrowserWindow() { return browserWindow.get(); }
    void useOption (pr7s::ComposerLayer, int id);     // an option's id chosen (noted as used)
    void tryOption (pr7s::ComposerLayer, int id);     // HEAR: chosen for now and heard; put back unless used
    void endTrial (bool keep);
    // (0.157.0) TYPE and the solo style: a short menu and the browser (types measured by the drums each writes, the
    // library's grooves left out; solos by the solo each writes). Ids: a type's place in sectionTypesFor + 1 (0:
    // STANDARD); a solo style's number.
    std::vector<OptionBrowser::Option> typeOptions() const;
    std::vector<OptionBrowser::Option> soloOptions() const;
    juce::PopupMenu makeTypeMenu();
    void typeMenuChosen (int item);
    std::unique_ptr<OptionBrowser> makeTypeBrowser();
    std::unique_ptr<OptionBrowser> makeSoloBrowser();
    bool canTryType() const;   // HEAR can try a type (not on a block already built: that would rebuild the song)
    void useType (const juce::String& name);
    void useSoloStyle (pr7s::SoloStyle);
    static inline const juce::String typeListKey = "type", soloListKey = "solo";
    static constexpr int moreTypesItem = 4801, moreSolosItem = 4802;
    static constexpr int moreOptionsItem = 4800;
    static constexpr int approachItems = 6300;    // (0.94.0) the type's approaches: 6300 automatic, 6301 + i (0.156.0: up to 6399)
    static inline const juce::String typeOwnGroup = "THIS TYPE'S OWN", songsGroup = "FROM YOUR SONGS", stylesGroup = "STYLES";
    // (0.126.0) OWN STYLES: a lane the section type writes plays its own styles instead (and back).
    void setOwnStyles (pr7s::ComposerLayer, bool own);
    // (0.126.0) SYNC TO: this lane alone follows another lane's rhythm (none: not on its own).
    void setSyncTo (pr7s::ComposerLayer, std::optional<pr7s::ComposerLayer> leader);
    std::optional<pr7s::ComposerLayer> getSyncTo (pr7s::ComposerLayer l) const
    { return state.followsOwnLeader (l) ? std::optional<pr7s::ComposerLayer> ((pr7s::ComposerLayer) state.lane (l).syncTo) : std::nullopt; }
    bool isPlayingOwnStyles (pr7s::ComposerLayer l) const { return state.lane (l).ownStyles; }
    bool typeWouldWrite (pr7s::ComposerLayer) const;   // (the type writes it when it doesn't play its own styles)
    std::function<void (const juce::String& question, std::function<void()> yes)> askToRebuild;   // (tests answer it)
    void clearTargetSection();
    int getTargetSection() const noexcept { return targetBlock; }
    // (0.93.0) The SECTIONS row in charge: with blocks on it the SONG strip shows them (a click composes one), NEXT
    // adds the suggested block at the end and composes it, BUILD moves on to the next block not built yet.
    bool usesSectionsRow() const;
    void aimAtSectionsRow();                          // the first block not built yet (else the first), if none chosen
    juce::Rectangle<int> getSongChipBounds (int blockIndex) const;   // a block's chip on the SONG strip (empty: not shown)
    void addNextSection (const pr7s::SectionSuggestion&);
    std::optional<pr7s::SectionPlace> getTargetPlace() const;   // none: no block (or it's gone)
    void takeIdea (const pr7s::LineIdea&);    // into its lane (by its part), locked, until rolled or restyled
    bool hasTakenIdea (pr7s::ComposerLayer l) const noexcept { return taken[(size_t) l].has_value(); }
    static std::optional<pr7s::ComposerLayer> layerOf (pr7s::LineIdea::Part);
    std::vector<pr7s::ChordSpan> spansAt (double start) const;   // the chords placed from that beat
    std::function<void (const std::vector<pr7s::ChordSpan>&)> onChordsChanged;   // (the suite: IDEAS follows)

    // (0.75.0) The key locked once a section is built (the song's own tempo is never changed here); chords per
    // section, only the key's own offered; your own riff pasted in.
    bool isKeyLocked() const;
    void setChord (int spanIndex, std::optional<pr7s::Chord>);   // one chord swapped (none: back to the source's)
    std::vector<pr7s::Chord> getChordChoices() const;           // the key's own chords (the swap menu)
    // (0.114.0) `lane`: the lane your riff plays on, unchanged (-1: the one it was on, the guitar to start with);
    // the others follow it (SYNC on that lane).
    bool pasteRiff (pr7s::ComposerRiff, int lane = -1);          // the riff in: its key guessed (not once locked), its chords
    bool pasteRiffFromPart (int partId, pr7s::DrumSection, int lane = -1);
    bool pasteRiffFromFile (const juce::File&, pr7s::DrumSection, int lane = -1);
    // (0.83.4, the owner: drag a MIDI part or file in and it builds that part) Your riff taken, then the section
    // built around it (the lanes on, the guitar playing the riff): the tracks written, none if nothing was.
    std::vector<int> buildFromPart (int partId, pr7s::DrumSection, int lane = -1);
    std::vector<int> buildFromFile (const juce::File&, pr7s::DrumSection, int lane = -1);
    bool canTakePart (int partId) const;                         // a MIDI part with notes
    void dropPart (int partId);                                  // asks THIS RIFF IS MY..., then buildFromPart
    void askSection (std::function<void (pr7s::DrumSection)> then, juce::Component* target);
    void askSectionAndLane (std::function<void (pr7s::DrumSection, int lane)> then, juce::Component* target);   // (0.114.0)
    void setRiffLane (pr7s::ComposerLayer);                      // (0.114.0) your riff onto another lane
    pr7s::ComposerLayer getRiffLane() const noexcept { return state.riffLayer(); }
    void forgetRiff();
    void setFromRiff (bool);                                     // CHORDS FROM: MY RIFF
    bool isFromRiff() const noexcept { return fromRiff; }
    bool isPlayingMyRiff() const noexcept { return useMyRiff && studio.getComposerRiff().has_value(); }
    void setPlayMyRiff (bool);
    std::function<std::vector<int>()> getSelectedParts;          // (MainView: the arrangement's selection)
    // (0.76.0, the owner: instruments too loud while composing) Each lane's VOLUME is the fader of the track it's
    // heard on; MIXER opens the mixer in its own window over the composer. Hearing goes on through both.
    std::function<void()> onOpenMixer;
    std::function<void()> onLearnMyBand;   // (0.82.0) LEARN MY BAND: read the band's record (+ MY RIFF's menu)
    void setLaneVolume (pr7s::ComposerLayer, float db, bool undoable = true);
    juce::Slider& getLaneVolume (pr7s::ComposerLayer l) noexcept { return lanes[(size_t) l].volume; }
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int x, int y) override;
    static constexpr int myRiffIndex = 6;                        // (the guitar's riffs 0-5; a section built on your riff: 6)
    int getNumNextKeys() const noexcept { return nextKeys.size(); }
    juce::TextButton* getNextKey (int i) const { return nextKeys[i]; }

    const pr7s::ComposerState& getState() const noexcept { return state; }
    const pr7s::ComposerContext& getContext() const noexcept { return context; }
    const pr7s::LineIdea& getIdea (pr7s::ComposerLayer l) const noexcept { return ideas[(size_t) l]; }
    const std::vector<pr7s::ChordSpan>& getSpans() const noexcept { return spans; }
    void runTimerForTests() { timerCallback(); }
    void refreshForTests() { refresh (false); }
    const juce::StringArray& getChordNames() const noexcept { return chordNames; }
    juce::String getStatus() const { return status; }
    juce::Button* keyNamed (const juce::String&);   // header keys by their text, lanes' as "<LAYER> ON/LOCK/ROLL"
    void setRandomSeed (juce::int64 seed) { random.setSeed (seed); }

    static constexpr int headerHeight = 64, controlsHeight = 44, songHeight = 54, rulerHeight = 30, laneHeaderWidth = 480, gap = 12;

private:
    std::unique_ptr<OptionBrowserWindow> browserWindow;
    int targetBlock = 0;   // (0.90.0) the SECTIONS row's block composed (its id; 0: none)
    struct Lane
    {
        juce::TextButton on { "ON" }, lock { "LOCK" }, roll { "ROLL" }, bars { "LOCK BARS" }, style, track, busy, low { "ANY STRING" }, octave { "OCTAVE 0" },
                         second { "2ND: LOOSER COPY" };   // (0.193.0: the acoustic lane only)
        juce::Slider volume { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };   // (0.76.0) the track it's heard on
        juce::Rectangle<int> header, notes;
        juce::Rectangle<int> note;   // (0.120.0) under the keys: where it's heard (empty: no room)
    };

    std::vector<OptionBrowser::Option> laneOptionsOwn (pr7s::ComposerLayer, bool withParts) const;
    std::vector<pr7s::ComposerFill> fillsShown;   // (0.203.0) the fills, for the open bars' labels
    std::array<juce::uint64, pr7s::composerLayerCount> barSelection {};   // (0.202.0) the bars chosen on each lane
    bool selectingBars = false;
    int barAnchor = 0;
    juce::uint64 selectionBefore = 0;
    void refreshBarKeys();
    void timerCallback() override;
    void refresh (bool rehear = true);   // spans, ideas, keys, the state saved; heard again when hearing
    void showStyleMenu (pr7s::ComposerLayer);
    static constexpr int soloItems = 5000;
    static constexpr int flavourItems = 6000, writtenRiffsItem = 6100;   // (0.85.0) the guitar's style menu
    static constexpr int doubleSyncItem = 6200;   // (0.86.0) the bass's and drums' style menus
    static constexpr int ownStylesItem = 6290;
    static constexpr int syncToItems = 6260;      // (0.126.0) SYNC TO: 6260 none, 6261 + layer    // (0.126.0) the type's lanes: Play Its Own Styles / Follow Again
    // (0.157.0) HEAR in a browser: what to put back (the list it was tried from) unless USE IT.
    struct Trial { bool active = false; juce::String list; std::function<void()> restore; bool wasHearing = false; };
    void beginTrial (const juce::String& list, std::function<void()> restore);
    Trial trial;
    void showTypeMenu();
    void applyBlockType();   // the target block's type onto the state (when it differs)
    static constexpr int myRiffItem = 4900;   // (0.75.0) the guitar's style menu: My Riff   // (0.70.0) the lead's style menu: Solo Style items
    void showTrackMenu (pr7s::ComposerLayer);
    void showProgressionMenu();
    void showPasteMenu();
    void showChordMenu (int spanIndex);
    int chordAt (juce::Point<int>) const;
    std::vector<pr7s::ChordSpan> chordSpans() const;   // the source's chords from 0, before the swaps
    void writeChordsAt (const std::vector<pr7s::ChordSpan>& placed);
    void showUseMenu();
    void showLaneMenu (pr7s::ComposerLayer);
    void showBusyMenu (pr7s::ComposerLayer);
    void showOctaveMenu (pr7s::ComposerLayer);
    void showLowMenu (pr7s::ComposerLayer);
    void showSecondMenu();
    std::vector<pr7s::ChordSpan> placedSpans() const;   // at the hearing start, else buildStart()
    void refreshSong();                                  // the SONG strip's NEXT keys
    std::vector<std::pair<juce::Rectangle<int>, int>> songChips() const;   // (0.93.0) the blocks' chips and their indices
    void paintSong (juce::Graphics&) const;
    juce::String summary() const;
    void paintLane (juce::Graphics&, pr7s::ComposerLayer, const Lane&) const;

    pr7s::Studio& studio;
    pr7s::ComposerState state;
    pr7s::ComposerContext context;
    std::vector<pr7s::ChordSpan> spans;      // from 0 (the library's), or the chord track's as they are
    juce::StringArray chordNames;            // each span's chord as written (the library's spelling: "bV" in A minor is Eb)
    std::array<pr7s::LineIdea, pr7s::composerLayerCount> ideas;
    // (0.85.2) What the lanes were composed from (`composedFrom`): the same inputs, the same ideas, so a song
    // edit that changes none of them (a note moved elsewhere, a part renamed) doesn't compose all six again.
    juce::String composedFrom;
    std::array<pr7s::LineIdea, pr7s::composerLayerCount> composed;   // (before the ideas taken from IDEAS)
    juce::String composeFingerprint() const;
    // (0.177.0) The library grooves' options made once for the chords and state they were made from (100 drum parts).
    mutable juce::String grooveOptionsKey;
    mutable std::vector<OptionBrowser::Option> grooveOptionsKept;
    mutable std::vector<juce::String> grooveFilesKept;   // (0.179.0) each kept option's groove
    mutable pr7s::ComposerState grooveOptionsState;      // (0.179.0) what the kept parts are made from
    std::array<int, pr7s::composerLayerCount> trackChoice {};
    std::array<Lane, pr7s::composerLayerCount> lanes;
    juce::Random random;
    // (0.85.0) The generated riffs given lately (fingerprints): a ROLL or GENERATE never gives one of them again.
    std::deque<juce::String> recentRiffs;
    static constexpr int recentRiffsKept = 200;
    void takeNewRiff();   // after a roll: draws again until the guitar's riff isn't a recent one
    bool hearing = false;
    double shownHearBeat = -1.0;   // (0.83.2) where the playhead was last drawn (its strips repainted only)
    double hearingStart = 0.0;
    juce::String status;
    juce::uint64 seenChange = 0;
    juce::uint64 seenContent = 0;   // (0.190.0)
    int pressedLane = -1;          // the lane the mouse went down on (a click hears, a drag takes it out)
    bool draggedOut = false;

    juce::ComboBox keyBox, genreBox, moodBox, lengthBox, styleBox;
    juce::TextButton progressionKey, libraryKey { "LIBRARY" }, chordTrackKey { "CHORD TRACK" }, toChordTrackKey { "+ CHORD TRACK" };
    juce::TextButton hearKey { "HEAR" }, useKey { "USE ALL" }, generateKey { "GENERATE" };
    juce::Rectangle<int> ruler, controlsLabels;
    // (0.74.0)
    juce::ComboBox sectionBox;
    juce::TextButton typeKey { "TYPE: STANDARD" };
    juce::TextButton buildKey { "BUILD" };
    juce::OwnedArray<juce::TextButton> nextKeys;
    std::vector<pr7s::SectionSuggestion> nextShown;
    juce::Rectangle<int> songArea;
    std::array<std::optional<pr7s::LineIdea>, pr7s::composerLayerCount> taken;
    std::vector<pr7s::ChordSpan> lastSpans;   // (taken ideas forgotten when the chords change)
    bool fromRiff = false, useMyRiff = false;
    std::map<int, pr7s::Chord> chordOverrides;
    juce::TextButton myRiffKey { "MY RIFF" }, pasteKey { "+ MY RIFF" }, mixerKey { "MIXER" }, syncKey { "SYNC: OFF" };
    void refreshVolumes();
    int draggingVolume = -1;
    std::unique_ptr<juce::FileChooser> chooser;
};

// (0.74.0) The suite: COMPOSE and IDEAS as tabs of one window. IDEAS follows the composer's chords
// (placed where BUILD would put them) unless it was opened on other chords; its cards go into the
// composer's lanes from their USE IT menu.
class ComposerSuite final : public juce::Component,
                            public PartDropTarget
{
public:
    explicit ComposerSuite (pr7s::Studio&);
    enum class Tab { compose, ideas };
    void showTab (Tab);
    Tab getTab() const noexcept { return tab; }
    void showIdeas (std::vector<pr7s::ChordSpan>);   // the IDEAS tab on these chords
    ComposerPage& getComposer() noexcept { return composer; }
    LinesPage& getIdeas() noexcept { return *ideas; }
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::TextButton& getTabKey (Tab t) noexcept { return t == Tab::compose ? composeTab : ideasTab; }
    // (0.83.4) A MIDI part dragged here from the arrangement: lit while held over, COMPOSE shown and the section
    // built around it on release (the composer asks which section first).
    bool isInterestedInPart (int partId) override;
    void partDragHover (bool over) override;
    void partDropped (int partId) override;
    void paintOverChildren (juce::Graphics&) override;
    bool isPartHovering() const noexcept { return partHovering; }
    static constexpr int tabsHeight = 40;

private:
    pr7s::Studio& studio;
    ComposerPage composer;
    std::unique_ptr<LinesPage> ideas;
    juce::TextButton composeTab { "COMPOSE" }, ideasTab { "IDEAS" };
    Tab tab = Tab::compose;
    bool ideasOnOtherChords = false;   // (opened on the chord track's chosen chords: kept until the composer's change)
    bool partHovering = false;
};

// Its window: kept open beside the studio while you listen.
class ComposerWindow final : public juce::DocumentWindow
{
public:
    explicit ComposerWindow (pr7s::Studio&);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    ComposerPage& getPage() { return suite->getComposer(); }
    LinesPage& getIdeasPage() { return suite->getIdeas(); }
    ComposerSuite& getSuite() { return *suite; }

private:
    ComposerSuite* suite = nullptr;
};
