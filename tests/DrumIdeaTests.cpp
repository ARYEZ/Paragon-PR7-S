#include <catch2/catch_test_macros.hpp>

#include <set>

#include "engine/Studio.h"
#include "engine/DrumIdeas.h"
#include "ui/LinesPage.h"

// IDEAS' DRUMS tab (0.54.0): generated beats, the library's grooves, the bass locked to them.

using namespace pr7s;

namespace
{
    using T = Chord::Type;
    using X = Chord::Tension;
    int pcOf (int n) { return ((n % 12) + 12) % 12; }

    std::vector<ChordSpan> bars (std::initializer_list<Chord> chords, const Key& key)
    {
        std::vector<ChordSpan> spans;
        auto at = 0.0;
        for (const auto& c : chords)
        {
            spans.push_back ({ at, at + 4.0, c, key });
            at += 4.0;
        }
        return spans;
    }

    const Key am { 5, 0, true };
    const Chord a { 9, T::minor, X::none, -1 }, f { 5, T::major, X::none, -1 }, c { 0, T::major, X::none, -1 }, g { 7, T::major, X::none, -1 };

    std::vector<int> pitchesAt (const LineIdea& idea, double beat)
    {
        std::vector<int> out;
        for (const auto& n : idea.notes)
            if (std::abs (n.beat - beat) < 1.0e-6)
                out.push_back (n.pitch);
        return out;
    }

    bool has (const std::vector<int>& list, int v) { return std::find (list.begin(), list.end(), v) != list.end(); }

    void writeGroove (const juce::File& file, int bars)
    {
        juce::MidiMessageSequence seq;
        for (int b = 0; b < bars; ++b)
            for (int beat = 0; beat < 4; ++beat)
            {
                const auto tick = (b * 4 + beat) * 480.0;
                seq.addEvent (juce::MidiMessage::noteOn (10, beat % 2 == 0 ? 36 : 38, (juce::uint8) 100), tick);
                seq.addEvent (juce::MidiMessage::noteOff (10, beat % 2 == 0 ? 36 : 38), tick + 120.0);
                seq.addEvent (juce::MidiMessage::noteOn (10, 42, (juce::uint8) 80), tick);
                seq.addEvent (juce::MidiMessage::noteOff (10, 42), tick + 120.0);
            }
        seq.updateMatchedPairs();
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (480);
        midi.addTrack (seq);
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::FileOutputStream out (file);
        REQUIRE (midi.writeTo (out));
    }
}

TEST_CASE ("Drum ideas: six beats in every style, GM drums, a crash on each phrase, a fill on its last beat, one locked to the riff (0.54.0)", "[drumideas]")
{
    const auto spans = bars ({ a, f, c, g, a, f, c, g }, am);
    const auto& tuning = GuitarTuning::all()[0];
    for (const auto style : guitarStyles())
        for (int variation = 0; variation < 2; ++variation)
        {
            const auto riffs = guitarRhythmIdeas (spans, 4.0, variation, tuning, {}, style, RiffSettings {});
            const auto& riff = riffs[2];
            const auto ideas = drumIdeas (style, spans, 4.0, variation, &riff);
            REQUIRE (ideas.size() == 6);
            std::set<juce::String> names;
            for (const auto& idea : ideas)
            {
                INFO (guitarStyleName (style) << ": " << idea.style << " (variation " << variation << ")");
                names.insert (idea.style);
                CHECK (idea.part == LineIdea::Part::drums);
                CHECK (idea.description.isNotEmpty());
                REQUIRE_FALSE (idea.notes.empty());
                CHECK (std::is_sorted (idea.notes.begin(), idea.notes.end(), [] (const LineNote& x, const LineNote& y) { return x.beat < y.beat; }));
                for (const auto& n : idea.notes)
                {
                    CHECK ((n.pitch >= 35 && n.pitch <= 59));   // (General MIDI's drums)
                    CHECK (n.beat >= -1.0e-9);
                    CHECK (n.beat < 32.0);
                    CHECK ((n.velocity > 0.0f && n.velocity <= 1.0f));
                }
                // A kick and a crash on the song's first beat and on bar 5's (each phrase's first).
                for (const auto at : { 0.0, 16.0 })
                {
                    const auto there = pitchesAt (idea, at);
                    CHECK (has (there, GmDrum::kick));
                    CHECK ((has (there, GmDrum::crash) || has (there, GmDrum::crash2) || has (there, GmDrum::china)));
                }
                // The fill: bar 4's last beat holds only drums and toms of a fill (no hats or ride).
                for (const auto& n : idea.notes)
                    if (n.beat >= 15.0 - 1.0e-9 && n.beat < 16.0 - 1.0e-9)
                        CHECK ((n.pitch == GmDrum::kick || n.pitch == GmDrum::snare || n.pitch == GmDrum::tomHigh || n.pitch == GmDrum::tomMid
                                || n.pitch == GmDrum::tomLow || n.pitch == GmDrum::tomFloor));
            }
            CHECK (names.size() == 6);
            // Locked to the riff: a kick on every hit of the riff (outside the fills).
            const auto& locked = ideas.back();
            CHECK (locked.style == "Locked to the riff");
            CHECK (locked.description.contains (riff.style));
            const auto kicks = kickHits (locked);
            for (const auto h : riffHits (riff))
                if (std::fmod (h, 16.0) < 15.0 - 1.0e-9)
                    CHECK (std::find_if (kicks.begin(), kicks.end(), [h] (double k) { return std::abs (k - h) < 1.0e-6; }) != kicks.end());
        }
    // No riff: kicks on one and three.
    const auto plain = drumIdeas (GuitarStyle::rock, spans, 4.0, 0, nullptr).back();
    CHECK (has (pitchesAt (plain, 2.0), GmDrum::kick));
}

TEST_CASE ("The bass locked to the kick, or following the riff: the chords' roots on the hits (0.54.0)", "[drumideas]")
{
    const auto spans = bars ({ a, f, c, g }, am);
    const std::vector<double> hits { 0.0, 0.75, 1.5, 4.0, 6.5, 8.0, 12.0, 14.0 };
    const auto bass = bassOnHits (spans, hits, "Locked to the kick", "test");
    CHECK (bass.part == LineIdea::Part::bass);
    REQUIRE (bass.notes.size() == hits.size());
    for (size_t i = 0; i < hits.size(); ++i)
    {
        const auto& n = bass.notes[i];
        CHECK (std::abs (n.beat - hits[i]) < 1.0e-9);
        CHECK ((n.pitch >= LineRanges::bassLow && n.pitch <= LineRanges::bassHigh));
        const auto& chord = spans[(size_t) (hits[i] / 4.0)].chord;
        CHECK (pcOf (n.pitch) == chord.root);
        // Held to the next hit, never past its chord.
        const auto next = i + 1 < hits.size() ? hits[i + 1] : 16.0;
        CHECK (n.beat + n.length <= std::min (next, std::floor (hits[i] / 4.0) * 4.0 + 4.0) + 1.0e-9);
    }
}

TEST_CASE ("Grooves from the library: drum-looking MIDI files, the style's first, near the tempo, fills last; laid over the chords (0.54.0)", "[drumideas]")
{
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-library-test");
    root.deleteRecursively();
    const auto item = [&] (const juce::String& path, double bpm)
    {
        MediaItem m;
        m.file = root.getChildFile (path);
        m.type = MediaType::midi;
        m.name = m.file.getFileNameWithoutExtension();
        m.bpm = bpm;
        return m;
    };
    std::vector<MediaItem> items { item ("GGD Drums/Metalcore/Breakdown 140.mid", 140.0), item ("GGD Drums/Metalcore/Fill 140.mid", 140.0),
                                   item ("GGD Drums/Rock/Verse 120.mid", 120.0), item ("Melodies/Lead line.mid", 140.0),
                                   item ("GGD Drums/Metalcore/Breakdown slow.mid", 90.0) };
    MediaItem audio = item ("GGD Drums/Loop.wav", 140.0);
    audio.type = MediaType::audio;
    items.push_back (audio);
    const auto found = findLibraryGrooves (items, GuitarStyle::metalcore, 140.0);
    REQUIRE (found.size() == 4);   // (not the melody, not the audio)
    CHECK (found[0].name == "Breakdown 140");
    CHECK (found[1].name == "Breakdown slow");
    CHECK (found.back().name == "Verse 120");
    for (const auto& gr : found)
        CHECK (gr.name != "Lead line");

    // A two-bar groove over four bars of chords: twice, from the first bar.
    writeGroove (found[0].file, 2);
    juce::String error;
    const auto spans = bars ({ a, f, c, g }, am);
    const auto idea = grooveIdea (found[0], spans, 4.0, error);
    INFO (error);
    REQUIRE (idea.has_value());
    CHECK (idea->part == LineIdea::Part::groove);
    CHECK (idea->notes.size() == 4 * 4 * 2);
    CHECK (idea->description.contains ("2 bars"));
    CHECK (has (pitchesAt (*idea, 8.0), 36));
    CHECK (has (pitchesAt (*idea, 9.0), 38));
    CHECK_FALSE (grooveIdea (found[2], spans, 4.0, error).has_value());   // (no such file)
    root.deleteRecursively();
}

TEST_CASE ("IDEAS' DRUMS tab: the generated beats and the library's grooves; the bass tab's two locked ideas (0.54.0)", "[drumideas][ui]")
{
    Studio studio (false);
    const auto spans = bars ({ a, f, c, g }, am);
    LinesPage page (studio, spans);
    page.setGuitarStyle (GuitarStyle::numetal);
    page.setWrittenBeats (true);   // (0.55.0: the written ones behind WRITTEN BEATS)
    page.setTab (LinesPage::Tab::drums);
    CHECK (page.getIdeas (LineIdea::Part::drums).size() == 6);
    CHECK (page.getIdeas (LineIdea::Part::drums).front().style == "Bounce groove");
    CHECK (page.getNumCardsShown() == 6 + (int) page.getIdeas (LineIdea::Part::groove).size());
    // The riff last played on the guitar tab is the one the locked beat follows.
    page.setTab (LinesPage::Tab::guitar);
    page.play (LineIdea::Part::rhythm, 3);
    page.stop();
    CHECK (page.getLockedRiff() == 3);
    page.setTab (LinesPage::Tab::drums);
    CHECK (page.getIdeas (LineIdea::Part::drums).back().description.contains (page.getIdeas (LineIdea::Part::rhythm)[3].style));
    // The bass: eight, the last two locked.
    page.setTab (LinesPage::Tab::bass);
    const auto& bass = page.getIdeas (LineIdea::Part::bass);
    REQUIRE (bass.size() == 8);
    CHECK (bass[6].style == "Locked to the kick");
    CHECK (bass[7].style == "Following the riff");
    CHECK (page.getNumCardsShown() == 8);
    // USE IT: a drums track with the GM map.
    const auto trackId = page.use (LineIdea::Part::drums, 0);
    const auto* track = studio.getProject().findTrack (trackId);
    REQUIRE (track != nullptr);
    CHECK (track->name == "Drums");
    CHECK (track->drumMap == DrumMap::gm().name);
}
