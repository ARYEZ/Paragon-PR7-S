#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/KeyEditor.h"
#include "ui/MainView.h"

// 0.42.0, chord detection from MIDI and audio.

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    using T = Chord::Type;
    using X = Chord::Tension;
    constexpr double rate = 44100.0;

    juce::String names (const std::vector<ChordEvent>& events)
    {
        juce::StringArray out;
        for (const auto& e : events)
            out.add (e.chord.name (Key {}));
        return out.joinIntoString (" ");
    }

    // A chord as notes: bass an octave under the root (octave 2), the triad around middle C, a bar long.
    void chordNotes (std::vector<HeardNote>& notes, double at, double length, int root, bool minor, float velocity = 0.8f)
    {
        notes.push_back ({ at, at + length, 36 + root, velocity });
        for (const auto i : { 0, minor ? 3 : 4, 7 })
            notes.push_back ({ at, at + length, 60 + root + i, velocity });
    }

    // A played tone: a fundamental and five overtones fading (1/k), with a short attack and release.
    void addTone (std::vector<float>& sound, double startSeconds, double seconds, int note, float level)
    {
        const auto f = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        const auto first = (int) (startSeconds * rate), count = (int) (seconds * rate);
        for (int i = 0; i < count && first + i < (int) sound.size(); ++i)
        {
            const auto t = i / rate;
            const auto envelope = juce::jmin (1.0, t / 0.01) * juce::jmin (1.0, (seconds - t) / 0.05);
            auto v = 0.0;
            for (int k = 1; k <= 6; ++k)
                v += std::sin (juce::MathConstants<double>::twoPi * f * k * t) / k;
            sound[(size_t) (first + i)] += (float) (v * level * envelope);
        }
    }

    // C Am F G a bar each (2 s at 120), as a guitar or piano might: bass, then the triad.
    std::vector<float> progression (double barSeconds = 2.0)
    {
        std::vector<float> sound ((size_t) (barSeconds * 4.0 * rate), 0.0f);
        const std::pair<int, bool> chords[] { { 0, false }, { 9, true }, { 5, false }, { 7, false } };
        for (int b = 0; b < 4; ++b)
        {
            const auto [root, minor] = chords[b];
            addTone (sound, b * barSeconds, barSeconds, 36 + root, 0.12f);
            for (const auto i : { 0, minor ? 3 : 4, 7 })
                addTone (sound, b * barSeconds, barSeconds, 60 + ((root + i) % 12), 0.08f);
        }
        return sound;
    }

    void writeWav (const juce::File& file, const std::vector<float>& sound)
    {
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::AudioBuffer<float> buffer (1, (int) sound.size());
        for (int i = 0; i < (int) sound.size(); ++i)
            buffer.setSample (0, i, sound[(size_t) i]);
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (1)
                                                                           .withBitsPerSample (32)
                                                                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }
}

TEST_CASE ("0.42.0: MIDI: the chords, steady through passing melody notes; an inversion; plain or with sevenths", "[chorddetect]")
{
    // C Am F G a bar each, with a melody running eighths through the key over them (half of them
    // not in the chord): the old reading, beat by beat, flickered; this one changes only at the bars.
    std::vector<HeardNote> notes;
    const std::pair<int, bool> chords[] { { 0, false }, { 9, true }, { 5, false }, { 7, false } };
    for (int b = 0; b < 4; ++b)
        chordNotes (notes, b * 4.0, 4.0, chords[b].first, chords[b].second, 0.6f);
    const int run[] { 72, 74, 76, 77, 79, 77, 76, 74 };
    for (int e = 0; e < 32; ++e)
        notes.push_back ({ e * 0.5, e * 0.5 + 0.45, run[e % 8], 0.9f });
    DetectSettings d;
    d.stepBeats = 1.0;   // (asked every beat: the smoothing keeps it to the bars)
    const auto found = detectChordsInNotes (notes, 0.0, 16.0, d);
    CHECK (names (found) == "C Am F G");
    REQUIRE (found.size() == 4);
    for (int b = 0; b < 4; ++b)
        CHECK_THAT (found[(size_t) b].beat, WithinAbs (b * 4.0, 1.0e-9));

    // C over E: an inversion.
    std::vector<HeardNote> inverted { { 0, 4, 52, 0.8f }, { 0, 4, 60, 0.8f }, { 0, 4, 64, 0.8f }, { 0, 4, 67, 0.8f } };
    const auto c = detectChordsInNotes (inverted, 0.0, 4.0, d);
    REQUIRE (c.size() == 1);
    CHECK (c[0].chord.name (Key {}) == "C/E");

    // Cmaj7: plain only reads C; all chords reads Cmaj7.
    std::vector<HeardNote> seventh;
    for (const auto p : { 48, 60, 64, 67, 71 })
        seventh.push_back ({ 0, 4, p, 0.8f });
    CHECK (names (detectChordsInNotes (seventh, 0.0, 4.0, d)) == "C");
    d.plainOnly = false;
    CHECK (names (detectChordsInNotes (seventh, 0.0, 4.0, d)) == "Cmaj7");

    // Silence carries the chord on; nothing at all: none.
    CHECK (detectChordsInNotes ({}, 0.0, 8.0, d).empty());
    std::vector<HeardNote> gap;
    chordNotes (gap, 0.0, 2.0, 0, false);
    chordNotes (gap, 6.0, 2.0, 7, false);
    CHECK (names (detectChordsInNotes (gap, 0.0, 8.0, d)) == "C G");

    // The settings as text.
    CHECK (DetectSettings::fromString (d.toString()) == d);
    CHECK (DetectSettings::fromString ("bad") == DetectSettings {});
}

TEST_CASE ("0.42.0: audio: a chord progression played (tones with overtones) read back, in time with the tempo", "[chorddetect]")
{
    DetectSettings d;
    d.stepBeats = 2.0;
    {
        const auto sound = progression();
        TempoMap tempo (120.0);
        const auto found = detectChordsInAudio (sound.data(), (int) sound.size(), rate, 0.0, tempo, 0.0, 16.0, d);
        CHECK (names (found) == "C Am F G");
        REQUIRE (found.size() == 4);
        for (int b = 0; b < 4; ++b)
            CHECK_THAT (found[(size_t) b].beat, WithinAbs (b * 4.0, 1.0e-9));
    }
    {
        // At 90 BPM (a bar is 2.67 s), the sound starting at beat 8: the chords at beats 8, 12, 16, 20.
        const auto bar = 4.0 * 60.0 / 90.0;
        const auto sound = progression (bar);
        TempoMap tempo (90.0);
        const auto found = detectChordsInAudio (sound.data(), (int) sound.size(), rate, 8.0, tempo, 8.0, 24.0, d);
        CHECK (names (found) == "C Am F G");
        REQUIRE (found.size() == 4);
        CHECK_THAT (found[1].beat, WithinAbs (12.0, 1.0e-9));
    }
    {
        // A minor chord's own overtones don't turn it major: A minor held alone.
        std::vector<float> sound ((size_t) (4.0 * rate), 0.0f);
        addTone (sound, 0.0, 4.0, 45, 0.12f);
        for (const auto n : { 57, 60, 64 })
            addTone (sound, 0.0, 4.0, n, 0.08f);
        TempoMap tempo (120.0);
        CHECK (names (detectChordsInAudio (sound.data(), (int) sound.size(), rate, 0.0, tempo, 0.0, 8.0, d)) == "Am");
    }
    // Silence: nothing.
    std::vector<float> quiet ((size_t) rate, 0.0f);
    CHECK (detectChordsInAudio (quiet.data(), (int) quiet.size(), rate, 0.0, TempoMap (120.0), 0.0, 2.0, d).empty());
}

TEST_CASE ("0.42.0: the studio: MIDI and audio parts onto the chord track (replacing what was there, one undo step) or the pads", "[chorddetect]")
{
    Studio studio (false);
    studio.setTempo (120.0);
    // An audio clip of C Am F G from bar 3 (beat 8).
    writeWav (studio.getAudioFolder().getChildFile ("chords.wav"), progression());
    const auto audioTrack = studio.addAudioTrack (false);
    Part clip;
    clip.start = 8.0;
    clip.length = 16.0;
    clip.audioFile = "chords.wav";
    studio.addPart (audioTrack, clip);
    const auto clipId = studio.getProject().findTrack (audioTrack)->parts[0].id;
    studio.addChordEvent (12.0, Chord { 2, T::minor, X::none, -1 });   // (replaced)
    studio.addChordEvent (40.0, Chord { 4, T::minor, X::none, -1 });   // (kept: after the clip)
    DetectSettings d;
    d.stepBeats = 4.0;
    CHECK (studio.detectChordsToTrack ({ clipId }, d) == 4);
    const auto& events = studio.getChordTrack().events;
    REQUIRE (events.size() == 5);
    CHECK (names ({ events.begin(), events.begin() + 4 }) == "C Am F G");
    CHECK_THAT (events[0].beat, WithinAbs (8.0, 1.0e-9));
    CHECK (events[4].chord.root == 4);
    CHECK (studio.getDetectSettings() == d);
    studio.undo();
    CHECK (studio.getChordTrack().events.size() == 2);

    // A MIDI part: G D Em C, onto the pads.
    const auto midiTrack = studio.addTrack ({});
    Part part;
    part.start = 0.0;
    part.length = 16.0;
    std::vector<HeardNote> heard;
    const std::pair<int, bool> chords[] { { 7, false }, { 2, false }, { 4, true }, { 0, false } };
    for (int b = 0; b < 4; ++b)
        chordNotes (heard, b * 4.0, 4.0, chords[b].first, chords[b].second);
    for (const auto& h : heard)
        part.notes.push_back ({ h.start, h.end - h.start, h.pitch, h.velocity });
    studio.addPart (midiTrack, part);
    const auto partId = studio.getProject().findTrack (midiTrack)->parts[0].id;
    CHECK (studio.detectChordsToPads ({ partId }, d) == 4);
    const auto& pads = studio.getChordPads().pads;
    REQUIRE (pads[0].chord.has_value());
    CHECK (pads[0].chord->root == 7);
    CHECK (pads[2].chord->type == T::minor);
    // MIDI and audio together: the two weighed alike (the MIDI's G D Em C over the clip's C Am F G
    // from beat 8: what both share wins where they differ is open; just: something found for each bar).
    CHECK (studio.detectChords ({ partId, clipId }, d).size() >= 2);
}

TEST_CASE ("0.42.0: the chord of some notes; the MIDI editor's info line names the selection's chord", "[chorddetect][ui]")
{
    CHECK (chordOfNotes ({ 60, 64, 67 })->name (Key {}) == "C");
    CHECK (chordOfNotes ({ 57, 60, 64, 67 })->name (Key {}) == "Am7");
    CHECK (chordOfNotes ({ 57, 60, 64, 67 }, true)->name (Key {}) == "Am");
    CHECK (chordOfNotes ({ 64, 67, 72 })->name (Key {}) == "C/E");
    CHECK_FALSE (chordOfNotes ({ 60, 72 }).has_value());   // (one pitch class)
    CHECK (chordOfNotes ({ 60, 62, 64, 67 }).has_value());  // (a best reading even when no exact chord)

    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addPart (trackId, { 0.0, 4.0, "Chord", { { 0.0, 1.0, 62, 0.8f }, { 0.0, 1.0, 65, 0.8f }, { 0.0, 1.0, 69, 0.8f } } });
    studio.dispatchPendingMessages();
    view.getArrangeView().select (trackId, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    view.setKeyEditorSize (100);
    auto& editor = view.getKeyEditor();
    editor.setSize (1300, 700);
    REQUIRE (editor.getPart() != nullptr);
    CHECK (editor.getInfoField ("CHORD").isEmpty());
    editor.selectAll();
    CHECK (editor.getInfoField ("CHORD") == "Dm");
    CHECK (editor.createComponentSnapshot (editor.getLocalBounds()).isValid());
}
