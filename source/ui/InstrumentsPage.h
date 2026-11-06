#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"

// The Hub's INSTRUMENTS page (0.62.0, the owner asked: rules for each instrument they use, such as
// Djinn Bass 2's range, tuning and keyswitches). engine/Instruments.h says what a profile holds.
//
//   left    the profiles (+ NEW: Djinn Bass 2, a bass, a guitar, a copy of the chosen one; DELETE)
//   right   the chosen one: NAME; BASS / GUITAR; USED FOR the composer's BASS, GUITAR or LEAD lane
//           (one profile per lane: marking one unmarks the others); the STRINGS low to high, typed
//           as notes ("A0": middle C = C3, as the libraries and the MIDI editor name them), + / -;
//           FRETS; the LOWEST and HIGHEST note it may play (empty: from the strings); BEND RANGE;
//           VELOCITY LAYERS (guitars: IDEAS' VELOCITIES panel, per instrument); the KEYSWITCHES:
//           a name, its note, and what it does (an articulation, or forces string n).
// Every change is kept at once (Studio::setInstrumentProfiles).
class InstrumentsPage final : public juce::Component
{
public:
    explicit InstrumentsPage (pr7s::Studio&);
    ~InstrumentsPage() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    // What the keys and fields do (tests drive them).
    const std::vector<pr7s::InstrumentProfile>& getProfiles() const noexcept { return profiles; }
    int getSelected() const noexcept { return selected; }
    void selectProfile (int);
    enum class NewKind { djinnBass2, bass, guitar, copy };
    void addProfile (NewKind);
    void deleteProfile();
    void setName (const juce::String&);
    void setKind (pr7s::InstrumentProfile::Kind);
    void setUse (pr7s::Studio::ProfileUse, bool);
    bool setString (int index, const juce::String& note);    // false: not a note
    void addString();
    void removeString();
    bool setFrets (const juce::String&);
    bool setRange (const juce::String& lowest, const juce::String& highest);   // (empty: from the strings)
    bool setBendRange (const juce::String&);
    void addKeyswitch();
    bool setKeyswitch (int index, const juce::String& name, const juce::String& note, int forcesString);
    void removeKeyswitch (int index);
    void setVelocities (const pr7s::GuitarVelocities&);
    juce::Button* keyNamed (const juce::String&);

    static constexpr int listWidth = 240, rowHeight = 30;

private:
    class List;
    class Editor;
    pr7s::Studio& studio;
    std::vector<pr7s::InstrumentProfile> profiles;
    int selected = -1;
    std::unique_ptr<List> list;
    std::unique_ptr<Editor> editor;
    juce::Viewport editorView;
    juce::TextButton newKey { "+ NEW" }, deleteKey { "DELETE" };

    pr7s::InstrumentProfile* current();
    void store();      // the profiles into the studio, the list repainted
    void rebuild();    // the editor's fields made again (after a choice or a change of shape)
    void showNewMenu();
};
