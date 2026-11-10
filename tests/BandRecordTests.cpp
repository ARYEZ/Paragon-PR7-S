// 0.82.0 (the owner asked): reading the band's record. DIs synthesised here as plucked strings (a fundamental
// and its harmonics dying away; palm mutes short and dull; bends gliding), so what's heard can be checked.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/BandRecord.h"
#include "ui/BandRecordPage.h"
#include "engine/Studio.h"

#include <iostream>
#include <map>

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    constexpr double rate = 44100.0;

    struct Pluck
    {
        double start, length;
        double midi;                  // its pitch (fractional allowed)
        bool muted = false;
        bool fifth = false;           // a power chord: the fifth with it
        double bendTo = 0.0;          // semitones it glides up to over its first third
        float level = 0.6f;
    };

    double hzOf (double midi) { return 440.0 * std::pow (2.0, (midi - 69.0) / 12.0); }

    // A DI: each pluck a string's tone (eight harmonics, the higher dying faster), muted: dying in ~60 ms and dull.
    juce::AudioBuffer<float> di (const std::vector<Pluck>& plucks, double seconds)
    {
        juce::AudioBuffer<float> sound (1, (int) (seconds * rate));
        sound.clear();
        auto* out = sound.getWritePointer (0);
        for (const auto& p : plucks)
        {
            const auto from = (int) (p.start * rate), count = (int) (p.length * rate);
            for (const auto offset : p.fifth ? std::vector<double> { 0.0, 7.0 } : std::vector<double> { 0.0 })
            {
                std::vector<double> phase (8, 0.0);
                for (int i = 0; i < count && from + i < sound.getNumSamples(); ++i)
                {
                    const auto t = i / rate;
                    const auto glide = p.bendTo > 0.0 ? p.bendTo * juce::jmin (1.0, t / (p.length / 3.0)) : 0.0;
                    const auto f = hzOf (p.midi + offset + glide);
                    const auto decay = p.muted ? std::exp (-t / 0.035) : std::exp (-t / 0.9);
                    auto sample = 0.0;
                    for (int h = 1; h <= 8; ++h)
                    {
                        phase[(size_t) h - 1] += juce::MathConstants<double>::twoPi * f * h / rate;
                        const auto dull = p.muted ? std::exp (-(h - 1) * 0.9) : 1.0 / h;
                        sample += std::sin (phase[(size_t) h - 1]) * dull * std::exp (-t * (h - 1) * (p.muted ? 3.0 : 0.8));
                    }
                    const auto attack = juce::jmin (1.0, t / 0.002);
                    out[from + i] += (float) (sample * decay * attack * p.level * 0.4);
                }
            }
        }
        return sound;
    }
}

TEST_CASE ("0.82.0: a bass DI: its notes, low A included, repeated picks on one note split, levels by how hard", "[band][082]")
{
    // A0 (Djinn's low string, 27.5 Hz), then D1 picked twice, then A1 loud.
    const auto sound = di ({ { 0.1, 0.5, 21.0 }, { 0.7, 0.3, 26.0 }, { 1.0, 0.3, 26.0 }, { 1.4, 0.5, 33.0, false, false, 0.0, 1.0f } }, 2.2);
    const auto notes = transcribeLine (sound, rate, BandRole::bass);
    REQUIRE (notes.size() == 4);
    CHECK (notes[0].pitch == 21);
    CHECK (notes[1].pitch == 26);
    CHECK (notes[2].pitch == 26);
    CHECK (notes[3].pitch == 33);
    CHECK_THAT (notes[1].start, WithinAbs (0.7, 0.03));
    CHECK_THAT (notes[2].start, WithinAbs (1.0, 0.03));
    CHECK (notes[3].level > notes[0].level);
}

TEST_CASE ("0.82.1: the band's tunings: nothing under the low string (a hair under it is the low string itself)", "[band][082]")
{
    // A 5-string bass in drop A: the low A (21) is the lowest it plays. A note read a whole step under it is the low A
    // played slack; with no tuning given it stays as read.
    const auto sound = di ({ { 0.1, 0.5, 19.0 }, { 0.7, 0.4, 26.0 } }, 1.4);
    const auto free = transcribeLine (sound, rate, BandRole::bass);
    REQUIRE (free.size() == 2);
    CHECK (free[0].pitch == 19);
    const auto tuned = transcribeLine (sound, rate, BandRole::bass, 21, 67);
    REQUIRE (tuned.size() == 2);
    CHECK (tuned[0].pitch == 21);
    CHECK (tuned[1].pitch == 26);
    // A 7-string in drop A: no rhythm root under its low A (33).
    std::vector<Pluck> plucks { { 0.1, 0.5, 33.0, false, true }, { 0.7, 0.5, 34.0, false, true }, { 1.3, 0.5, 38.0, false, true } };
    const auto hits = transcribeRhythm (di (plucks, 2.0), rate, 33, 76);
    REQUIRE (hits.size() == 3);
    for (const auto& h : hits)
        CHECK (h.pitch >= 33);
    CHECK (hits[0].pitch == 33);
    CHECK (hits[1].pitch == 34);
    CHECK (hits[2].pitch == 38);
}

TEST_CASE ("0.82.0: a lead DI: notes, a bend (no new attack) read as a bend of its size, not two notes", "[band][082]")
{
    const auto sound = di ({ { 0.1, 0.3, 64.0 }, { 0.45, 0.3, 67.0 }, { 0.8, 0.9, 69.0, false, false, 2.0 } }, 2.0);
    const auto notes = transcribeLine (sound, rate, BandRole::lead);
    REQUIRE (notes.size() == 3);
    CHECK (notes[0].pitch == 64);
    CHECK (notes[1].pitch == 67);
    CHECK (notes[2].pitch == 69);
    CHECK_THAT (notes[2].bend, WithinAbs (2.0, 0.51));
    CHECK (notes[0].bend == 0.0f);
}

TEST_CASE ("0.82.0: a rhythm guitar DI: hits, roots (power chords not read an octave under), palm mutes against open hits", "[band][082]")
{
    // Low A (33) chugs palm muted, then an open A5 power chord, a muted E (40) single note, an open D5 (38).
    std::vector<Pluck> plucks;
    for (int i = 0; i < 4; ++i)
        plucks.push_back ({ 0.1 + i * 0.15, 0.14, 33.0, true });
    plucks.push_back ({ 0.75, 0.6, 33.0, false, true });
    plucks.push_back ({ 1.45, 0.14, 40.0, true });
    plucks.push_back ({ 1.7, 0.6, 38.0, false, true });
    const auto notes = transcribeRhythm (di (plucks, 2.6), rate);
    REQUIRE (notes.size() == 7);
    for (int i = 0; i < 4; ++i)
    {
        CHECK (notes[(size_t) i].pitch == 33);
        CHECK (notes[(size_t) i].muted);
        CHECK_THAT (notes[(size_t) i].start, WithinAbs (0.1 + i * 0.15, 0.03));
    }
    CHECK (notes[4].pitch == 33);
    CHECK (notes[4].powerChord);
    CHECK_FALSE (notes[4].muted);
    CHECK (notes[5].pitch == 40);
    CHECK (notes[5].muted);
    CHECK_FALSE (notes[5].powerChord);
    CHECK (notes[6].pitch == 38);
    CHECK (notes[6].powerChord);
    CHECK_FALSE (notes[6].muted);
}

TEST_CASE ("0.82.0: the notes as an idea: beats at the song's tempo, guitars in the owner's velocity layers, bends as effects", "[band][082]")
{
    GuitarVelocities v;
    std::vector<PlayedNote> played { { 0.5, 0.75, 33, true, 1.0f, false }, { 1.0, 1.25, 40, false, 0.8f, true } };
    const auto rhythm = playedIdea (played, 120.0, BandRole::rhythm, v);
    REQUIRE (rhythm.notes.size() == 3);   // the power chord's fifth added
    CHECK_THAT (rhythm.notes[0].beat, WithinAbs (1.0, 1.0e-9));
    CHECK_THAT (rhythm.notes[0].length, WithinAbs (0.5, 1.0e-9));
    CHECK (rhythm.notes[1].pitch == 40);
    const auto open = juce::roundToInt (rhythm.notes[0].velocity * 127.0f), mute = juce::roundToInt (rhythm.notes[2].velocity * 127.0f);
    CHECK (open >= 60);
    CHECK (open <= 119);
    CHECK (mute <= 59);
    CHECK (rhythm.notes[2].pitch == 40);

    PlayedNote bent { 0.0, 1.0, 69 };
    bent.bend = 2.0f;
    const auto lead = playedIdea ({ bent }, 90.0, BandRole::lead, v);
    REQUIRE (lead.notes.size() == 1);
    CHECK (lead.notes[0].effect == LineNote::Effect::bend);
    CHECK (lead.notes[0].amount == 2.0f);
    CHECK_THAT (lead.notes[0].length, WithinAbs (1.5, 1.0e-9));
}

TEST_CASE ("0.82.0: a record folder: songs by subfolder or by name, roles guessed from file names, the tempo from the drum MIDI", "[band][082]")
{
    const auto folder = juce::File::createTempFile ("").getSiblingFile ("pr7s-band-082");
    folder.deleteRecursively();
    const juce::AudioBuffer<float> silence (1, 100);
    // A subfolder song.
    const auto one = folder.getChildFile ("01 Opener");
    one.createDirectory();
    for (const auto* name : { "Opener Bass DI.wav", "Opener Rhythm Gtr.wav", "Opener Lead.wav" })
        REQUIRE (writeWav (one.getChildFile (name), silence, rate));
    {
        juce::MidiMessageSequence track;
        track.addEvent (juce::MidiMessage::tempoMetaEvent (500000 * 120 / 140), 0.0);   // 140 BPM
        track.addEvent (juce::MidiMessage::timeSignatureMetaEvent (7, 8), 0.0);
        track.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 100), 0.0);
        track.addEvent (juce::MidiMessage::noteOff (10, 36), 120.0);
        juce::MidiFile file;
        file.setTicksPerQuarterNote (480);
        file.addTrack (track);
        juce::FileOutputStream out (one.getChildFile ("Opener drums.mid"));
        REQUIRE (out.openedOk());
        file.writeTo (out);
    }
    // Loose files: a second song.
    for (const auto* name : { "Song Two - bass.wav", "Song Two - gtr 1.wav", "Song Two - gtr 2.wav" })
        REQUIRE (writeWav (folder.getChildFile (name), silence, rate));

    const auto songs = scanBandFolder (folder);
    REQUIRE (songs.size() == 2);
    CHECK (songs[0].name == "01 Opener");
    CHECK (songs[0].drums.getFileName() == "Opener drums.mid");
    CHECK (songs[0].fileOf (BandRole::bass).getFileName() == "Opener Bass DI.wav");
    CHECK (songs[0].fileOf (BandRole::rhythm).getFileName() == "Opener Rhythm Gtr.wav");
    CHECK (songs[0].fileOf (BandRole::lead).getFileName() == "Opener Lead.wav");
    CHECK (songs[0].tempoFromFile);
    CHECK_THAT (songs[0].bpm, WithinAbs (140.0, 0.01));
    CHECK (songs[0].numerator == 7);
    CHECK (songs[0].denominator == 8);
    CHECK (songs[1].name == "song two");
    CHECK (songs[1].fileOf (BandRole::bass).getFileName() == "Song Two - bass.wav");
    CHECK (songs[1].fileOf (BandRole::rhythm).getFileName() == "Song Two - gtr 1.wav");
    CHECK (songs[1].fileOf (BandRole::lead).getFileName() == "Song Two - gtr 2.wav");
    CHECK_FALSE (songs[1].tempoFromFile);
    folder.deleteRecursively();
}

TEST_CASE ("0.82.0 debug: what's heard", "[.banddebug]")
{
    const auto print = [] (const char* what, const std::vector<PlayedNote>& notes)
    {
        std::cout << what << "\n";
        for (const auto& n : notes)
            std::cout << "  " << n.start << "-" << n.end << " p" << n.pitch << (n.powerChord ? " 5" : "") << (n.muted ? " M" : "") << " lvl " << n.level << " bend " << n.bend << "\n";
    };
    print ("bass", transcribeLine (di ({ { 0.1, 0.5, 21.0 }, { 0.7, 0.3, 26.0 }, { 1.0, 0.3, 26.0 }, { 1.4, 0.5, 33.0, false, false, 0.0, 1.0f } }, 2.2), rate, BandRole::bass));
    std::vector<Pluck> plucks;
    for (int i = 0; i < 4; ++i)
        plucks.push_back ({ 0.1 + i * 0.15, 0.14, 33.0, true });
    plucks.push_back ({ 0.75, 0.6, 33.0, false, true });
    plucks.push_back ({ 1.45, 0.14, 40.0, true });
    plucks.push_back ({ 1.7, 0.6, 38.0, false, true });
    const auto sound = di (plucks, 2.6);
    print ("rhythm", transcribeRhythm (sound, rate));
}

TEST_CASE ("0.82.0: a review song: the drum MIDI, each DI on an audio track with what was heard under it, at the song's tempo", "[band][082]")
{
    const auto folder = juce::File::createTempFile ("").getSiblingFile ("pr7s-band-review-082");
    folder.deleteRecursively();
    const auto source = folder.getChildFile ("Record").getChildFile ("Opener");
    source.createDirectory();
    REQUIRE (writeWav (source.getChildFile ("Opener bass.wav"), di ({ { 0.0, 0.4, 33.0 }, { 0.5, 0.4, 38.0 } }, 1.0), rate));
    std::vector<Pluck> chugs;
    for (int i = 0; i < 4; ++i)
        chugs.push_back ({ i * 0.25, 0.14, 33.0, true });
    REQUIRE (writeWav (source.getChildFile ("Opener rhythm.wav"), di (chugs, 1.0), rate));
    {
        juce::MidiMessageSequence track;
        track.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);   // 120 BPM
        for (int i = 0; i < 4; ++i)
        {
            track.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 100), i * 240.0);
            track.addEvent (juce::MidiMessage::noteOff (10, 36), i * 240.0 + 60.0);
        }
        juce::MidiFile file;
        file.setTicksPerQuarterNote (480);
        file.addTrack (track);
        juce::FileOutputStream out (source.getChildFile ("Opener drums.mid"));
        REQUIRE (out.openedOk());
        file.writeTo (out);
    }
    const auto songs = scanBandFolder (folder.getChildFile ("Record"));
    REQUIRE (songs.size() == 1);
    juce::File made;
    juce::String error;
    REQUIRE (makeBandReviewSong (songs[0], folder.getChildFile ("Review"), {}, made, error));
    CHECK (made.existsAsFile());
    CHECK (made.getSiblingFile ("Opener Audio").getChildFile ("Opener bass.wav").existsAsFile());

    Studio studio (false);
    juce::String problems;
    REQUIRE (studio.open (made, problems));
    const auto& tracks = studio.getProject().tracks;
    REQUIRE (tracks.size() == 5);
    CHECK (tracks[0].name == "Drums (MIDI)");
    CHECK (tracks[0].drumEditor);
    CHECK (tracks[0].parts[0].notes.size() == 4);
    CHECK_THAT (tracks[0].parts[0].notes[1].start, WithinAbs (0.5, 1.0e-6));
    // The DIs in name order: bass, then rhythm.
    CHECK (tracks[1].name == "BASS DI");
    CHECK (tracks[1].isAudio());
    CHECK (studio.getAudio (tracks[1].parts[0].audioFile) != nullptr);
    CHECK (tracks[2].name == "BASS (heard)");
    REQUIRE (tracks[2].parts[0].notes.size() == 2);
    CHECK (tracks[2].parts[0].notes[0].pitch == 33);
    CHECK (tracks[2].parts[0].notes[1].pitch == 38);
    CHECK_THAT (tracks[2].parts[0].notes[1].start, WithinAbs (1.0, 0.06));   // 0.5 s at 120 BPM
    CHECK (tracks[4].name == "RHYTHM GUITAR (heard)");
    REQUIRE (tracks[4].parts[0].notes.size() == 4);
    for (const auto& n : tracks[4].parts[0].notes)
        CHECK (juce::roundToInt (n.velocity * 127.0f) <= 59);   // palm muted: the mute layer
    folder.deleteRecursively();
}

// (0.82.0) The owner's own files, when they're given: PARAGON_BAND_DIR (a record folder), PARAGON_BAND_OUT (where
// the review songs and a picture go). Prints what was heard; not run otherwise.
TEST_CASE ("0.82.0: the owner's record, read", "[.bandreal]")
{
    const juce::File in (juce::SystemStats::getEnvironmentVariable ("PARAGON_BAND_DIR", {}));
    const juce::File out (juce::SystemStats::getEnvironmentVariable ("PARAGON_BAND_OUT", {}));
    REQUIRE (in.isDirectory());
    const auto songs = scanBandFolder (in);
    for (const auto& song : songs)
    {
        std::cout << "SONG " << song.name << ": " << song.bpm << " BPM" << (song.tempoFromFile ? " (from the MIDI)" : " (assumed)")
                  << ", " << song.numerator << "/" << song.denominator << ", drums " << song.drums.getFileName() << "\n";
        for (const auto& [f, r] : song.dis)
            std::cout << "  " << f.getFileName() << " -> " << bandRoleName (r) << "\n";
        const auto started = juce::Time::getMillisecondCounterHiRes();
        juce::File made;
        juce::String error, report;
        REQUIRE (makeBandReviewSong (song, out, {}, made, error, {}, &report));
        std::cout << report << "\n";
        std::cout << "  made in " << (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0 << " s: " << made.getFullPathName() << "\n" << error << "\n";
        Studio studio (false);
        juce::String problems;
        REQUIRE (studio.open (made, problems));
        const auto& project = studio.getProject();
        for (const auto& m : project.markerTrack.markers)
            std::cout << "  SECTION " << m.name << " from bar " << (project.signatures().barAt (m.start) + 1) << ", " << juce::roundToInt ((m.end - m.start) / project.beatsPerBar()) << " bars\n";
        for (const auto& t : project.tracks)
        {
            if (t.parts.empty() || t.isAudio())
                continue;
            const auto& notes = t.parts[0].notes;
            std::map<int, int> pitches;
            int muted = 0, chords = 0;
            for (size_t i = 0; i < notes.size(); ++i)
            {
                ++pitches[notes[i].pitch];
                if (juce::roundToInt (notes[i].velocity * 127.0f) < 60)
                    ++muted;
                if (i > 0 && std::abs (notes[i].start - notes[i - 1].start) < 1.0e-9)
                    ++chords;
            }
            std::cout << "  " << t.name << ": " << notes.size() << " notes, " << muted << " in the mute layer, " << chords << " fifths\n    pitches:";
            std::vector<std::pair<int, int>> sorted (pitches.begin(), pitches.end());
            std::sort (sorted.begin(), sorted.end(), [] (auto& a, auto& b) { return a.second > b.second; });
            for (size_t i = 0; i < juce::jmin ((size_t) 10, sorted.size()); ++i)
                std::cout << " " << pr7s::noteName (sorted[i].first) << "x" << sorted[i].second;
            std::cout << "\n    first:";
            for (size_t i = 0; i < juce::jmin ((size_t) 24, notes.size()); ++i)
                std::cout << " " << juce::String (notes[i].start, 2) << ":" << pr7s::noteName (notes[i].pitch) << "/" << juce::roundToInt (notes[i].velocity * 127.0f);
            std::cout << "\n";
        }
    }
}

TEST_CASE ("0.82.0: LEARN MY BAND: a folder's songs, a DI's player put right, the songs read into review songs", "[band][082][ui]")
{
    const auto folder = juce::File::createTempFile ("").getSiblingFile ("pr7s-band-page-082");
    folder.deleteRecursively();
    const auto song = folder.getChildFile ("Record").getChildFile ("Opener");
    song.createDirectory();
    REQUIRE (writeWav (song.getChildFile ("Opener bass.wav"), di ({ { 0.0, 0.4, 33.0 }, { 0.5, 0.4, 38.0 } }, 1.0), rate));
    REQUIRE (writeWav (song.getChildFile ("Opener take 3.wav"), di ({ { 0.0, 0.4, 57.0 } }, 1.0), rate));
    Studio studio (false);
    BandRecordPage page (studio);
    page.setSize (1200, 600);
    REQUIRE (page.loadFolder (folder.getChildFile ("Record")) == 1);
    const auto& songs = page.getSongs();
    REQUIRE (songs[0].dis.size() == 2);
    CHECK (songs[0].dis[1].second == BandRole::unknown);   // "take 3": who played it is asked
    page.setRole (0, 1, BandRole::lead);
    CHECK (page.getSongs()[0].dis[1].second == BandRole::lead);
    page.setTempo (0, 100.0);
    CHECK (page.getSongs()[0].bpm == 100.0);
    CHECK (page.getStatusText().contains ("1 SONG"));
    page.readSongsNow();
    const auto made = page.getReviewSong (0);
    REQUIRE (made.existsAsFile());
    juce::String problems;
    REQUIRE (studio.open (made, problems));
    CHECK (studio.getProject().bpm == 100.0);
    CHECK (studio.getProject().findTrack (studio.getProject().tracks.back().id)->name == "LEAD GUITAR (heard)");
    page.getReviewFolder().deleteRecursively();
    folder.deleteRecursively();
}
