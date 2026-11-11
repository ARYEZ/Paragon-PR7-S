#pragma once

#include <deque>

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"
#include "ui/LinesPage.h"
#include "ui/PartDropTarget.h"

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
    void setLowString (pr7s::ComposerLayer, bool);       // (0.61.0) bass and guitar
    void setOnlyString (pr7s::ComposerLayer, int);       // (0.62.0) 1.. the string (1 the lowest); 0 off
    juce::String stringText (pr7s::ComposerLayer) const; // the key's text: ANY STRING, LOW STRING, STRING 2 ONLY
    void setTuning (pr7s::ComposerLayer, const juce::String& name);   // the bass's or the guitar's
    int trackFor (pr7s::ComposerLayer, bool hearing) const;   // (0: a new track when used / not heard)
    bool hear();                             // false: nothing to play them on
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
    bool pasteRiff (pr7s::ComposerRiff);                         // the riff in: its key guessed (not once locked), its chords, the guitar plays it
    bool pasteRiffFromPart (int partId, pr7s::DrumSection);
    bool pasteRiffFromFile (const juce::File&, pr7s::DrumSection);
    // (0.83.4, the owner: drag a MIDI part or file in and it builds that part) Your riff taken, then the section
    // built around it (the lanes on, the guitar playing the riff): the tracks written, none if nothing was.
    std::vector<int> buildFromPart (int partId, pr7s::DrumSection);
    std::vector<int> buildFromFile (const juce::File&, pr7s::DrumSection);
    bool canTakePart (int partId) const;                         // a MIDI part with notes
    void dropPart (int partId);                                  // asks THIS RIFF IS MY..., then buildFromPart
    void askSection (std::function<void (pr7s::DrumSection)> then, juce::Component* target);
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
    const pr7s::LineIdea& getIdea (pr7s::ComposerLayer l) const noexcept { return ideas[(size_t) l]; }
    const std::vector<pr7s::ChordSpan>& getSpans() const noexcept { return spans; }
    const juce::StringArray& getChordNames() const noexcept { return chordNames; }
    juce::String getStatus() const { return status; }
    juce::Button* keyNamed (const juce::String&);   // header keys by their text, lanes' as "<LAYER> ON/LOCK/ROLL"
    void setRandomSeed (juce::int64 seed) { random.setSeed (seed); }

    static constexpr int headerHeight = 64, controlsHeight = 44, songHeight = 54, rulerHeight = 30, laneHeaderWidth = 420, gap = 12;

private:
    struct Lane
    {
        juce::TextButton on { "ON" }, lock { "LOCK" }, roll { "ROLL" }, style, track, busy, low { "ANY STRING" };
        juce::Slider volume { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };   // (0.76.0) the track it's heard on
        juce::Rectangle<int> header, notes;
    };

    void timerCallback() override;
    void refresh (bool rehear = true);   // spans, ideas, keys, the state saved; heard again when hearing
    void showStyleMenu (pr7s::ComposerLayer);
    static constexpr int soloItems = 5000;
    static constexpr int flavourItems = 6000, writtenRiffsItem = 6100;   // (0.85.0) the guitar's style menu
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
    void showLowMenu (pr7s::ComposerLayer);
    std::vector<pr7s::ChordSpan> placedSpans() const;   // at the hearing start, else buildStart()
    void refreshSong();                                  // the SONG strip's NEXT keys
    void paintSong (juce::Graphics&) const;
    juce::String trackText (pr7s::ComposerLayer) const;
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
    int pressedLane = -1;          // the lane the mouse went down on (a click hears, a drag takes it out)
    bool draggedOut = false;

    juce::ComboBox keyBox, genreBox, moodBox, lengthBox, styleBox;
    juce::TextButton progressionKey, libraryKey { "LIBRARY" }, chordTrackKey { "CHORD TRACK" }, toChordTrackKey { "+ CHORD TRACK" };
    juce::TextButton hearKey { "HEAR" }, useKey { "USE ALL" }, generateKey { "GENERATE" };
    juce::Rectangle<int> ruler, controlsLabels;
    // (0.74.0)
    juce::ComboBox sectionBox;
    juce::TextButton buildKey { "BUILD" };
    juce::OwnedArray<juce::TextButton> nextKeys;
    std::vector<pr7s::SectionSuggestion> nextShown;
    juce::Rectangle<int> songArea;
    std::array<std::optional<pr7s::LineIdea>, pr7s::composerLayerCount> taken;
    std::vector<pr7s::ChordSpan> lastSpans;   // (taken ideas forgotten when the chords change)
    bool fromRiff = false, useMyRiff = false;
    std::map<int, pr7s::Chord> chordOverrides;
    juce::TextButton myRiffKey { "MY RIFF" }, pasteKey { "+ MY RIFF" }, mixerKey { "MIXER" };
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
