#pragma once

#include "Project.h"

#include <functional>

// MIDI controller lanes (0.25.0, the manual's Controller Display): what each lane shows,
// by a code (an int, saved as text):
//
//   -1          velocity (the notes' own: a bar per note)
//   0 - 127     a continuous controller (CC) by its number
//   128         pitchbend (14 bits: 0 - 16383, 8192 = no bend)
//   129         aftertouch (channel pressure)
//   130         program change
//   256 + note  poly pressure for that note (key)
//
// Each part keeps its controller events (Part::controllers, by start); a value holds until
// the next event of its kind, or ramps to it (Controller::ramp). The engine sends them on
// the track's channel, a ramp as a run of values (`expandRamps`), and on play, a jump or
// the cycle coming round it first sends the value each controller has there ("chasing").
namespace pr7s::ControllerKinds
{
    constexpr int velocity = -1, pitchbend = 128, aftertouch = 129, programChange = 130, polyBase = 256;

    bool isValid (int code) noexcept;                 // velocity or a controller
    bool isController (int code) noexcept;            // not velocity
    Controller::Type typeOf (int code) noexcept;
    int numberOf (int code) noexcept;                 // a CC's number, poly pressure's note, else 0
    int codeOf (const Controller&) noexcept;
    int codeOf (Controller::Type, int number) noexcept;

    juce::String ccName (int number);                 // "MODULATION", "EXPRESSION"... or "CC 3"
    juce::String name (int code);                     // "VELOCITY", "PITCHBEND", "CC 1 MODULATION", "POLY PRESSURE C3"...
    juce::String shortName (int code);                // for a lane's narrow header: "VEL", "PB", "MOD", "CC 74"...
    int maxValue (int code) noexcept;                 // 127, or pitchbend's 16383
    int defaultValue (int code) noexcept;             // pitchbend 8192, volume 100, pan 64, else 0
    juce::String valueText (int code, int value);     // pitchbend centred: "+4096" (0: "0"); pan "L12"/"C"/"R12"; else the number
    bool alwaysStep (int code) noexcept;              // the manual's list: bank select, sustain, NRPN/RPN, the mode messages (recorded as steps)

    juce::String toText (int code);                   // "velocity", "cc1", "pitchbend", "aftertouch", "program", "poly60"
    int fromText (const juce::String&);               // -2: not one
    juce::String lanesToText (const std::vector<int>&);
    std::vector<int> lanesFromText (const juce::String&);

    // The menu's order: the most used controllers first, then the rest by number.
    const std::vector<int>& commonCodes();

    void sort (std::vector<Controller>&);             // by start; at the same start, the order kept
    std::vector<int> usedCodes (const Part&);         // the kinds the part has events of (in code order)

    // The value at a beat (part beats) from the part's events of that kind (ramps followed);
    // nothing before its first event.
    std::optional<int> valueAt (const std::vector<Controller>&, int code, double beat);

    // The MIDI message for a value on a channel (1-16).
    juce::MidiMessage toMidi (int code, int value, int channel);

    // Every value the part's events send (beats from the part's start, in order): each event,
    // and a ramp as a value every `resolution` beats where it changes (the engine's).
    void expandRamps (const Part&, double resolution, const std::function<void (double beat, int code, int value)>&);
}
