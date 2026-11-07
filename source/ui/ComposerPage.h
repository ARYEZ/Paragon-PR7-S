#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

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
class ComposerPage final : public juce::Component,
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
    double placeStart() const;               // where HEAR and USE put the layers (a bar: the playhead's)

    const pr7s::ComposerState& getState() const noexcept { return state; }
    const pr7s::LineIdea& getIdea (pr7s::ComposerLayer l) const noexcept { return ideas[(size_t) l]; }
    const std::vector<pr7s::ChordSpan>& getSpans() const noexcept { return spans; }
    const juce::StringArray& getChordNames() const noexcept { return chordNames; }
    juce::String getStatus() const { return status; }
    juce::Button* keyNamed (const juce::String&);   // header keys by their text, lanes' as "<LAYER> ON/LOCK/ROLL"
    void setRandomSeed (juce::int64 seed) { random.setSeed (seed); }

    static constexpr int headerHeight = 64, controlsHeight = 44, rulerHeight = 30, laneHeaderWidth = 420, gap = 12;

private:
    struct Lane
    {
        juce::TextButton on { "ON" }, lock { "LOCK" }, roll { "ROLL" }, style, track, busy, low { "ANY STRING" };
        juce::Rectangle<int> header, notes;
    };

    void timerCallback() override;
    void refresh (bool rehear = true);   // spans, ideas, keys, the state saved; heard again when hearing
    void showStyleMenu (pr7s::ComposerLayer);
    static constexpr int soloItems = 5000;   // (0.70.0) the lead's style menu: Solo Style items
    void showTrackMenu (pr7s::ComposerLayer);
    void showProgressionMenu();
    void showUseMenu();
    void showLaneMenu (pr7s::ComposerLayer);
    void showBusyMenu (pr7s::ComposerLayer);
    void showLowMenu (pr7s::ComposerLayer);
    std::vector<pr7s::ChordSpan> placedSpans() const;
    juce::String trackText (pr7s::ComposerLayer) const;
    juce::String summary() const;
    void paintLane (juce::Graphics&, pr7s::ComposerLayer, const Lane&) const;

    pr7s::Studio& studio;
    pr7s::ComposerState state;
    pr7s::ComposerContext context;
    std::vector<pr7s::ChordSpan> spans;      // from 0 (the library's), or the chord track's as they are
    juce::StringArray chordNames;            // each span's chord as written (the library's spelling: "bV" in A minor is Eb)
    std::array<pr7s::LineIdea, pr7s::composerLayerCount> ideas;
    std::array<int, pr7s::composerLayerCount> trackChoice {};
    std::array<Lane, pr7s::composerLayerCount> lanes;
    juce::Random random;
    bool hearing = false;
    double hearingStart = 0.0;
    juce::String status;
    juce::uint64 seenChange = 0;
    int pressedLane = -1;          // the lane the mouse went down on (a click hears, a drag takes it out)
    bool draggedOut = false;

    juce::ComboBox keyBox, genreBox, moodBox, lengthBox, styleBox;
    juce::TextButton progressionKey, libraryKey { "LIBRARY" }, chordTrackKey { "CHORD TRACK" }, toChordTrackKey { "+ CHORD TRACK" };
    juce::TextButton hearKey { "HEAR" }, useKey { "USE ALL" }, generateKey { "GENERATE" };
    juce::Rectangle<int> ruler, controlsLabels;
};

// Its window: kept open beside the studio while you listen.
class ComposerWindow final : public juce::DocumentWindow
{
public:
    explicit ComposerWindow (pr7s::Studio&);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    ComposerPage& getPage() { return *page; }

private:
    ComposerPage* page = nullptr;
};
