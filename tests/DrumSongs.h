#pragma once

#include "engine/DrumLearning.h"

// Songs of drums for the learning tests and the LEARN A DRUMMER snapshot (0.56.0).
namespace drumsongs
{
    using namespace pr7s::GmDrum;
    using pr7s::Note;

    // A song written bar by bar: hits as (sixteenth, note, velocity 1-127).
    struct Writer
    {
        std::vector<Note> notes;
        int bar = 0;
        void hit (int sixteenth, int note, int velocity) { notes.push_back ({ bar * 4.0 + sixteenth * 0.25, 0.25, note, (float) velocity / 127.0f }); }
        void hitAt (double beatInBar, int note, int velocity) { notes.push_back ({ bar * 4.0 + beatInBar, 0.25, note, (float) velocity / 127.0f }); }
        void next() { ++bar; }
        void fill (int fromBeat)   // toms down in sixteenths from a beat to the bar's end
        {
            const int toms[] = { tomHigh, tomMid, tomLow, tomFloor };
            for (auto s = fromBeat * 4; s < 16; ++s)
                hit (s, toms[(s - fromBeat * 4) * 4 / juce::jmax (1, 16 - fromBeat * 4)], 90 + s);
        }
    };

    // The verse: hats in eighths, kick 1, the and of 2, 3; snare 2 and 4, ghosts.
    inline void verseBar (Writer& w, bool fillAtEnd, bool withCrash, int hat = closedHat)
    {
        for (auto s = 0; s < (fillAtEnd ? 12 : 16); s += 2)
            w.hit (s, hat, s % 4 == 0 ? 96 : 72);
        w.hit (0, kick, 110);
        w.hit (6, kick, 96);
        w.hit (8, kick, 104);
        w.hit (4, snare, 116);
        if (! fillAtEnd)
        {
            w.hit (12, snare, 116);
            w.hit (15, snare, 38);
        }
        w.hit (7, snare, 40);
        if (withCrash)
            w.hit (0, crash, 112);
        if (fillAtEnd)
            w.fill (3);
        w.next();
    }

    // The chorus: the ride in eighths, louder, kick 1, a and 3's and; snare 2 and 4.
    inline void chorusBar (Writer& w, bool fillAtEnd, bool withCrash)
    {
        for (auto s = 0; s < (fillAtEnd ? 12 : 16); s += 2)
            w.hit (s, ride, s % 4 == 0 ? 112 : 96);
        w.hit (0, kick, 120);
        w.hit (3, kick, 108);
        w.hit (8, kick, 116);
        w.hit (10, kick, 108);
        w.hit (4, snare, 124);
        if (! fillAtEnd)
            w.hit (12, snare, 124);
        if (withCrash)
            w.hit (0, crash2, 120);
        if (fillAtEnd)
            w.fill (3);
        w.next();
    }

    // The breakdown: half time, the china on the beats, kicks in chugs.
    inline void breakdownBar (Writer& w, bool fillAtEnd)
    {
        for (auto s = 0; s < (fillAtEnd ? 12 : 16); s += 4)
            w.hit (s, china, 116);
        for (const auto k : { 0, 1, 2, 6, 10, 11 })
            if (! fillAtEnd || k < 12)
                w.hit (k, kick, 118);
        w.hit (8, snare, 124);
        if (fillAtEnd)
            w.fill (3);
        w.next();
    }

    // A bridge: half time on the ride bell, quieter.
    inline void bridgeBar (Writer& w, bool fillAtEnd)
    {
        for (auto s = 0; s < (fillAtEnd ? 12 : 16); s += 4)
            w.hit (s, rideBell, 84);
        w.hit (0, kick, 88);
        w.hit (10, kick, 80);
        w.hit (8, snare, 92);
        if (fillAtEnd)
            w.fill (3);
        w.next();
    }

    // A song from a layout: "I4 V8 C8 B8 R8 E4" (intro, verse, chorus, breakdown, bridge, ending; bars).
    inline std::vector<Note> songOf (const juce::String& layout, double swing = 0.0)
    {
        Writer w;
        for (const auto& token : juce::StringArray::fromTokens (layout, " ", ""))
        {
            const auto kind = token[0];
            const auto bars = token.substring (1).getIntValue();
            for (auto b = 0; b < bars; ++b)
            {
                const auto last = b == bars - 1;
                switch (kind)
                {
                    case 'I':
                        for (auto s = 0; s < (last ? 12 : 16); s += 4)
                            w.hit (s, closedHat, 80);
                        w.hit (0, kick, 90);
                        w.hit (4, snare, 90);
                        if (! last) w.hit (12, snare, 90);
                        if (last) w.fill (3);
                        w.next();
                        break;
                    case 'V': verseBar (w, last || b % 4 == 3, b % 4 == 0); break;
                    case 'C': chorusBar (w, last, b % 4 == 0); break;
                    case 'B': breakdownBar (w, last); break;
                    case 'R': bridgeBar (w, last); break;
                    case 'E':
                        if (last)
                        {
                            w.hit (0, kick, 127);
                            w.hit (0, crash, 127);
                            w.hit (0, snare, 120);
                            w.next();
                        }
                        else
                        {
                            for (auto s = 0; s < 16; s += 4)
                            {
                                w.hit (s, crash, 120);
                                w.hit (s, kick, 120);
                            }
                            w.hit (4, snare, 120);
                            w.hit (12, snare, 120);
                            w.next();
                        }
                        break;
                    default: w.next(); break;
                }
            }
        }
        if (swing != 0.0)
            for (auto& n : w.notes)
                if (std::abs (std::fmod (n.start, 0.5) - 0.25) < 1.0e-9)
                    n.start += swing;
        return w.notes;
    }

    // Written to a MIDI file (drums on channel 10).
    inline bool writeSong (const juce::File& file, const std::vector<Note>& notes, double bpm = 120.0)
    {
        juce::MidiMessageSequence drums, conductor;
        for (const auto& n : notes)
        {
            drums.addEvent (juce::MidiMessage::noteOn (10, n.pitch, n.velocity), n.start * 480.0);
            drums.addEvent (juce::MidiMessage::noteOff (10, n.pitch), (n.start + n.length) * 480.0);
        }
        drums.updateMatchedPairs();
        conductor.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / bpm)), 0.0);
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (480);
        midi.addTrack (conductor);
        midi.addTrack (drums);
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::FileOutputStream out (file);
        return out.openedOk() && midi.writeTo (out);
    }

    inline std::vector<Note> song()   // "I4 V8 C8 V8 C8 B8 C8 E4", written out by hand
    {
        Writer w;
        // Intro: 4 bars, hats in quarters, the kick on one, a fill to end it.
        for (auto b = 0; b < 4; ++b)
        {
            for (auto s = 0; s < (b == 3 ? 12 : 16); s += 4)
                w.hit (s, closedHat, 80);
            w.hit (0, kick, 90);
            w.hit (4, snare, 90);
            if (b != 3)
                w.hit (12, snare, 90);
            if (b == 3)
                w.fill (3);
            w.next();
        }
        const auto verse = [&]
        {
            for (auto b = 0; b < 8; ++b)
                verseBar (w, b == 3 || b == 7, b == 0 || b == 4);
        };
        const auto chorus = [&]
        {
            for (auto b = 0; b < 8; ++b)
                chorusBar (w, b == 7, b == 0 || b == 4);
        };
        verse();
        chorus();
        verse();
        chorus();
        for (auto b = 0; b < 8; ++b)
            breakdownBar (w, b == 7);
        chorus();
        // The ending: three bars of hits on the beats with crashes, then the last hit.
        for (auto b = 0; b < 3; ++b)
        {
            for (auto s = 0; s < 16; s += 4)
            {
                w.hit (s, crash, 120);
                w.hit (s, kick, 120);
            }
            w.hit (4, snare, 120);
            w.hit (12, snare, 120);
            w.next();
        }
        w.hit (0, kick, 127);
        w.hit (0, crash, 127);
        w.hit (0, snare, 120);
        return w.notes;
    }

}
