#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include <optional>

namespace pr7s
{
    // (0.220.0, the owner: "add matching PR7-K patches for each cyberpunk part") A sound for each of the keys lane's
    // CYBERPUNK parts (LaneWorlds.h), written into the PR7-K playing on the track, as PR7-G's guitar tones are
    // (GuitarTones.h). Our own sounds, made from PR7-K's own building blocks (its saws, pulses, unison, filter,
    // envelopes, DELAY and REVERB), not copied from its factory presets or anyone else's; not tuned by ear here.
    //
    // PR7-K keeps its settings as `<ParagonX1>` with a PARAM (id, value: the knob's real value) for every knob, maybe
    // inside JUCE's `<VST3PluginState>` when hosted as a VST3 (read and written by GuitarTones' readPr7gSettings /
    // writePr7gSettings: the two plug-ins share the form). A patch starts from a clean slate on the instance's own
    // state: every switch ending "On" off, the oscillators, filter, amp and play knobs it uses back to PR7-K's
    // defaults, the loaded wavetables, SFZ instruments, drawings and spectra gone, the modulation matrix replaced;
    // then its own values. PR7-K's MIDI LEARN links are kept, with two of ours:
    //  - CC 74 (the part's filter moves) linked to FILTER CUTOFF (20 Hz-20 kHz across the controller);
    //  - CC 11 (the pumping pads' dips): a matrix route from EXPRESSION to AMP VOLUME, the knob at 0 and the route
    //    +0.5, so the level is unity until a CC 11 arrives (PR7-K reads EXPRESSION as full before then) and the
    //    part's dips (CC 11 120 -> 25) take it from about -0.5 dB to -14 dB.
    struct SynthPatch
    {
        juce::String name;                       // shown on PR7-K (its preset name) and in the status
        juce::String description;
        std::map<juce::String, float> settings;  // PR7-K parameter id -> real value
        bool pumping = false;                    // the EXPRESSION -> AMP VOLUME route
    };

    const std::vector<SynthPatch>& cyberpunkPatches();               // one per cyberpunkKeysOptions() entry, in order
    const SynthPatch* cyberpunkPatchFor (int option);
    std::optional<int> cyberpunkOptionOfStyle (const juce::String& style);   // "Cyberpunk: <part>" -> its option
    bool isCyberpunkPatchName (const juce::String& rigName);         // one of ours (written before)
    bool isPr7k (const juce::PluginDescription&);                     // (its name holds "PR7-K")

    // The patch onto PR7-K's settings (as readPr7gSettings gives them).
    void applySynthPatch (juce::XmlElement& settings, const SynthPatch&);
    // PR7-K's preset name in those settings ("" when none).
    juce::String synthPatchNameOf (const juce::XmlElement& settings);
    // Whether those settings are a fresh sound (no preset, or Init) or one of ours: a build may write over them.
    bool mayReplaceSynthPatch (const juce::XmlElement& settings);
}
