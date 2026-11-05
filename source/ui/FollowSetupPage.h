#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// Follow Chord Track's dialog (0.36.0, the manual's "Follow Chord Track Dialog"): shown the
// first time a track is set to follow the chord track. How its notes relate to the chord track:
// Follow Directly (they were played over these chords), Analyze Chords (their own chords are
// found first; MIDI only), or Apply a Known Chord (no chord changes in them: which chord).
class FollowSetupPage final : public juce::Component
{
public:
    FollowSetupPage();
    void paint (juce::Graphics&) override;
    void resized() override;

    pr7s::FollowStart getStart() const noexcept;
    void setStart (pr7s::FollowStart);
    pr7s::Chord getKnownChord() const;
    void setKnownChord (const pr7s::Chord&);

    std::function<void()> onOk, onCancel;
    juce::TextButton& getOk() noexcept { return ok; }

    // The dialog in a call-out over `target`; on OK the track follows in `mode` (`done` after).
    static void show (pr7s::Studio&, int trackId, pr7s::FollowMode, juce::Component& target, std::function<void()> done = {});

private:
    void refreshEnabled();
    juce::ToggleButton directly { "Follow Directly" }, analyze { "Analyze Chords" }, known { "Apply a Known Chord" };
    juce::Label directlyNote, syncLabel, analyzeNote, knownNote, rootLabel, typeLabel;
    juce::ComboBox root, type;
    juce::TextButton ok { "OK" }, cancel { "CANCEL" };
};
