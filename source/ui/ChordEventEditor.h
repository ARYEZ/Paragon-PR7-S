#pragma once

#include "ui/ChordUi.h"

// A chord on the chord track, edited (0.16.0, the manual's chord editor for chord
// events): its root, type, tension and bass (each choice heard), and what the
// chord before suggests in its place, scored (one click takes it).
class ChordEventEditor final : public juce::Component
{
public:
    ChordEventEditor (const pr7s::Chord& chord, const pr7s::Key& key, std::optional<pr7s::Chord> before);
    std::function<void (const pr7s::Chord&)> onChange;
    std::function<void()> onDelete;
    const pr7s::Chord& getChord() const noexcept { return chord; }
    void choose (const pr7s::Chord&);   // (tests: as a key does)

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    pr7s::Chord chord;
    pr7s::Key key;
    std::optional<pr7s::Chord> before;
    std::vector<pr7s::Suggestion> suggestions;
    juce::OwnedArray<juce::TextButton> roots, types, tensions, basses, next;
    juce::TextButton deleteKey { "DELETE" };
    void refresh();
};
