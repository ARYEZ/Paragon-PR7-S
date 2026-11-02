#include <catch2/catch_test_macros.hpp>

#include <set>

#include "engine/Studio.h"

using namespace pr7s;

namespace
{
    using T = Chord::Type;
    using X = Chord::Tension;
    int pcOf (int n) { return ((n % 12) + 12) % 12; }

    // Chords a bar each in 4/4 from beat 0.
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

    const ChordSpan& spanAt (const std::vector<ChordSpan>& spans, double beat)
    {
        for (const auto& s : spans)
            if (beat >= s.start - 1.0e-9 && beat < s.end - 1.0e-9)
                return s;
        return spans.back();
    }

    bool isChordTone (const Chord& c, int pitch)
    {
        for (const auto p : c.pitchClasses())
            if (p == pcOf (pitch))
                return true;
        return false;
    }
}

TEST_CASE ("Bass ideas: each chord's root on its first beat, in the bass range, walking into the next root (0.18.0)", "[lines]")
{
    const Key c { 0, 0, false };
    const auto spans = bars ({ { 0, T::major, X::none, -1 }, { 7, T::major, X::none, -1 }, { 9, T::minor, X::none, -1 }, { 5, T::major, X::none, -1 } }, c);
    for (int variation = 0; variation < 4; ++variation)
    {
        const auto ideas = bassIdeas (spans, 4.0, variation);
        REQUIRE (ideas.size() == 6);
        for (const auto& idea : ideas)
        {
            INFO (idea.style << " (variation " << variation << ")");
            CHECK (idea.part == LineIdea::Part::bass);
            CHECK (idea.description.isNotEmpty());
            REQUIRE_FALSE (idea.notes.empty());
            for (const auto& s : spans)
            {
                // A note right on the chord's first beat, its root.
                auto found = false;
                for (const auto& n : idea.notes)
                    if (std::abs (n.beat - s.start) < 1.0e-9)
                    {
                        found = true;
                        CHECK (pcOf (n.pitch) == s.chord.root);
                    }
                CHECK (found);
            }
            for (const auto& n : idea.notes)
            {
                CHECK (n.pitch >= LineRanges::bassLow);
                CHECK (n.pitch <= LineRanges::bassHigh + 12);   // (octaves and fifths reach above the roots)
                CHECK (n.length > 0.0);
                CHECK (n.beat + n.length <= 16.0 + 1.0e-9);
            }
            CHECK (std::is_sorted (idea.notes.begin(), idea.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; }));
        }
        // Walking: the last beat of each chord but the last is a half step from the next chord's root.
        const auto& walking = ideas[4];
        REQUIRE (walking.style == "Walking");
        for (size_t s = 0; s + 1 < spans.size(); ++s)
            for (const auto& n : walking.notes)
                if (std::abs (n.beat - (spans[s].end - 1.0)) < 1.0e-9)
                {
                    const auto toNext = pcOf (spans[s + 1].chord.root - n.pitch);
                    CHECK ((toNext == 1 || toNext == 11));
                }
    }
    // The same chords and variation: the same notes; another variation: something different.
    CHECK (bassIdeas (spans, 4.0, 1)[1].notes == bassIdeas (spans, 4.0, 1)[1].notes);
    auto anyDifferent = false;
    for (size_t i = 0; i < 6; ++i)
        anyDifferent = anyDifferent || bassIdeas (spans, 4.0, 0)[i].notes != bassIdeas (spans, 4.0, 1)[i].notes;
    CHECK (anyDifferent);
    // A slash chord: its bass note is the bass's root.
    const auto slash = bars ({ { 0, T::major, X::none, 4 } }, c);   // C/E
    CHECK (pcOf (bassIdeas (slash, 4.0)[0].notes.front().pitch) == 4);
}

TEST_CASE ("Melody ideas: chord tones on each bar's first beat, in the key without rubs, ending on the chord (0.18.0)", "[lines]")
{
    const Key c { 0, 0, false }, am { 5, 0, true };
    for (const auto& [key, spans] : { std::pair { c, bars ({ { 0, T::major, X::none, -1 }, { 7, T::major, X::none, -1 }, { 9, T::minor, X::none, -1 }, { 5, T::major, X::none, -1 } }, c) },
                                      std::pair { am, bars ({ { 9, T::minor, X::none, -1 }, { 5, T::major, X::none, -1 }, { 7, T::major, X::none, -1 }, { 4, T::major, X::none, -1 },
                                                              { 9, T::minor, X::none, -1 } }, am) } })
        for (int variation = 0; variation < 4; ++variation)
        {
            const auto ideas = melodyIdeas (spans, 4.0, variation);
            REQUIRE (ideas.size() == 6);
            const auto total = spans.back().end;
            for (const auto& idea : ideas)
            {
                INFO (idea.style << " in " << key.name() << " (variation " << variation << ")");
                CHECK (idea.part == LineIdea::Part::melody);
                REQUIRE_FALSE (idea.notes.empty());
                for (const auto& n : idea.notes)
                {
                    CHECK (n.pitch >= LineRanges::melodyLow);
                    CHECK (n.pitch <= LineRanges::melodyHigh);
                    CHECK (n.length > 0.0);
                    CHECK (n.beat + n.length <= total + 1.0e-9);
                    // In the key (and no rub) over its chord, or (an anticipation) the next chord's tone.
                    const auto& s = spanAt (spans, n.beat);
                    const auto av = availableNotes (s.chord, s.key);
                    const auto fits = std::find (av.begin(), av.end(), pcOf (n.pitch)) != av.end()
                                   || isChordTone (spanAt (spans, juce::jmin (total - 0.01, s.end)).chord, n.pitch);
                    CHECK (fits);
                    // A bar's first beat: a chord tone (the syncopated line may rest there).
                    if (std::abs (std::fmod (n.beat, 4.0)) < 1.0e-9)
                    {
                        INFO ("beat " << n.beat << " pitch " << n.pitch << " chord " << s.chord.name());
                        CHECK (isChordTone (s.chord, n.pitch));
                    }
                }
                // The last note on the last chord, held to its end.
                const auto& last = idea.notes.back();
                CHECK (isChordTone (spans.back().chord, last.pitch));
                CHECK (last.beat + last.length >= total - 0.2);
            }
        }
    // In A minor over E major: G (the key's) rubs against G#, so it isn't offered; G# is.
    const auto av = availableNotes ({ 4, T::major, X::none, -1 }, am);
    CHECK (std::find (av.begin(), av.end(), 7) == av.end());
    CHECK (std::find (av.begin(), av.end(), 8) != av.end());
}

TEST_CASE ("Bass and melody ideas in the studio: the chord track's chords, a part on a new track, the preview (0.18.0)", "[lines]")
{
    Studio studio (false);
    CHECK (studio.getLineChords().empty());
    studio.addChordEvent (0.0, { 0, T::major, X::none, -1 });
    studio.addChordEvent (4.0, { 7, T::major, X::none, -1 });
    studio.addChordEvent (8.0, { 9, T::minor, X::none, -1 });
    studio.addChordEvent (12.0, { 5, T::major, X::none, -1 });
    // All of them (each until the next; the last a bar).
    auto spans = studio.getLineChords();
    REQUIRE (spans.size() == 4);
    CHECK (spans[3].start == 12.0);
    CHECK (spans[3].end == 16.0);
    CHECK (spans[0].key.name() == "C major");
    // The selection.
    spans = studio.getLineChords ({ 2, 1 });
    REQUIRE (spans.size() == 2);
    CHECK (spans[0].start == 4.0);
    CHECK (spans[1].end == 12.0);
    // The cycle's, cut at its end.
    studio.setCycle (4.0, 10.0);
    studio.setCycleOn (true);
    spans = studio.getLineChords();
    REQUIRE (spans.size() == 2);
    CHECK (spans[1].end == 10.0);
    studio.setCycleOn (false);

    // An idea onto a new track: a part under the chords, its notes where they were.
    spans = studio.getLineChords();
    const auto idea = bassIdeas (spans, 4.0)[4];
    const auto before = studio.getProject().tracks.size();
    const auto trackId = studio.addLinePart (idea, spans);
    REQUIRE (trackId != 0);
    CHECK (studio.getProject().tracks.size() == before + 1);
    const auto* track = studio.getProject().findTrack (trackId);
    CHECK (track->name == "Bass");
    REQUIRE (track->parts.size() == 1);
    CHECK (track->parts[0].start == 0.0);
    CHECK (track->parts[0].length == 16.0);
    CHECK (track->parts[0].name == "Bass: Walking");
    REQUIRE (track->parts[0].notes.size() == idea.notes.size());
    CHECK (track->parts[0].notes[3].pitch == idea.notes[3].pitch);
    // One undo step takes the part away.
    studio.undo();
    CHECK (studio.getProject().findTrack (trackId)->parts.empty());

    // The preview: running, then stopped.
    studio.previewLine (&idea, spans);
    CHECK (studio.isPreviewing());
    juce::Thread::sleep (30);
    CHECK (studio.getPreviewBeat() > 0.0);
    studio.stopPreview();
    CHECK_FALSE (studio.isPreviewing());
}

TEST_CASE ("Guitar tunings: the owner's 7-string drop A first, low to high (0.19.0)", "[lines]")
{
    const auto& all = GuitarTuning::all();
    REQUIRE (all.size() >= 6);
    CHECK (all.front().name == "7-string drop A");
    CHECK (all.front().strings == std::vector<int> { 33, 40, 45, 50, 55, 59, 64 });   // A0 E1 A1 D2 G2 B2 E3 (middle C = C3)
    CHECK (all.front().lowest() == 33);
    CHECK (all.front().highest() == 64 + 24);
    for (const auto& t : all)
    {
        INFO (t.name);
        CHECK (std::is_sorted (t.strings.begin(), t.strings.end()));
        CHECK (t.strings.size() >= 6);
    }
    CHECK (GuitarTuning::named ("6-string drop D").lowest() == 38);
    CHECK (GuitarTuning::named ("no such tuning").name == "7-string drop A");
}

namespace
{
    // Notes starting at `beat` (give or take a strum).
    std::vector<int> notesAt (const LineIdea& idea, double beat, double within = 1.0e-6)
    {
        std::vector<int> out;
        for (const auto& n : idea.notes)
            if (n.beat >= beat - 1.0e-6 && n.beat < beat + within)
                out.push_back (n.pitch);
        return out;
    }

    const LineIdea& ideaNamed (const std::vector<LineIdea>& ideas, const juce::String& style)
    {
        for (const auto& i : ideas)
            if (i.style == style)
                return i;
        FAIL ("no idea named " << style);
        return ideas.front();
    }
}

TEST_CASE ("Guitar rhythm ideas: power chords with no third, nothing under the lowest string, gallops, strums (0.19.0)", "[lines]")
{
    const Key am { 5, 0, true };
    // Am F C G, then B diminished (its power chord takes its own flat fifth) and E major.
    const auto spans = bars ({ { 9, T::minor, X::none, -1 }, { 5, T::major, X::none, -1 }, { 0, T::major, X::none, -1 }, { 7, T::major, X::none, -1 },
                               { 11, T::diminished, X::none, -1 }, { 4, T::major, X::none, -1 } }, am);
    for (const auto& tuning : GuitarTuning::all())
        for (int variation = 0; variation < 3; ++variation)
        {
            const auto ideas = guitarRhythmIdeas (spans, 4.0, variation, tuning);
            REQUIRE (ideas.size() == 6);
            for (const auto& idea : ideas)
            {
                INFO (tuning.name << ": " << idea.style << " (variation " << variation << ")");
                CHECK (idea.part == LineIdea::Part::rhythm);
                REQUIRE_FALSE (idea.notes.empty());
                CHECK (std::is_sorted (idea.notes.begin(), idea.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; }));
                for (const auto& n : idea.notes)
                {
                    // Never where a guitar library keeps its keyswitches, nor past the 24th fret.
                    CHECK (n.pitch >= tuning.lowest());
                    CHECK (n.pitch <= tuning.highest());
                    CHECK (n.beat + n.length <= 24.0 + 1.0e-6);
                    // Every note belongs to its chord (the pedal riff's pedal: the key's tonic).
                    const auto& s = spanAt (spans, n.beat);
                    if (idea.style != "Pedal-point riff")
                        CHECK (isChordTone (s.chord, n.pitch));
                    else
                        CHECK ((isChordTone (s.chord, n.pitch) || pcOf (n.pitch) == 9));
                }
            }
            // Power chords: only roots and fifths (no third), the root at the bottom on each chord's first beat, low.
            const auto& power = ideaNamed (ideas, "Power chords");
            for (const auto& s : spans)
            {
                auto at = notesAt (power, s.start);
                REQUIRE (at.size() == 3);
                std::sort (at.begin(), at.end());
                const auto root = s.chord.root;
                CHECK (pcOf (at[0]) == root);
                CHECK (at[2] == at[0] + 12);
                CHECK (at[0] < tuning.lowest() + 12);
                CHECK (pcOf (at[1] - at[0]) == (s.chord.type == T::diminished ? 6 : 7));
            }
            for (const auto& n : power.notes)
            {
                const auto& s = spanAt (spans, n.beat);
                const auto third = pcOf (s.chord.root + (s.chord.type == T::minor || s.chord.type == T::diminished ? 3 : 4));
                CHECK (pcOf (n.pitch) != third);
            }
            // The gallop: on every beat, an eighth and two sixteenths (0, 1/2, 3/4).
            const auto& gallop = ideaNamed (ideas, "Gallop");
            std::set<int> places;
            for (const auto& n : gallop.notes)
                places.insert ((int) std::round (std::fmod (n.beat, 1.0) * 4.0));
            CHECK (places == std::set<int> { 0, 2, 3 });
            // Strummed shapes: each stroke's notes within a tenth of a beat of the stroke, four or more down from the bar.
            const auto& strum = ideaNamed (ideas, "Open strummed shapes");
            for (const auto& n : strum.notes)
                CHECK (std::fmod (n.beat, 0.5) < 0.11);
            for (const auto& s : spans)
                CHECK (notesAt (strum, s.start, 0.11).size() >= 4);
        }
}

TEST_CASE ("Guitar velocities in the owner's library's layout: 1-59 palm muted, 60-119 open, 120-126 rakes, 127 pinch harmonics (0.19.1, 0.19.2)", "[lines]")
{
    namespace V = GuitarVelocity;
    // The engine sends round (velocity * 127): each value comes back exactly.
    for (const auto v : { V::muteTight, V::muteMid, V::muteLoose, V::sustainSoft, V::sustain, V::sustainHard, V::rake, V::pinch })
        CHECK (V::midiOf (V::of (v)) == v);
    CHECK (V::muteLoose <= V::mutedMost);
    CHECK (V::sustainHard <= V::sustainMost);
    CHECK ((V::rake >= 120 && V::rake <= 126));
    CHECK (V::pinch == 127);

    const Key am { 5, 0, true };
    const auto spans = bars ({ { 9, T::minor, X::none, -1 }, { 5, T::major, X::none, -1 }, { 0, T::major, X::none, -1 }, { 4, T::major, X::none, -1 } }, am);
    const auto& tuning = GuitarTuning::all()[0];
    const auto muted = [] (const LineNote& n) { return V::midiOf (n.velocity) >= 1 && V::midiOf (n.velocity) <= 59; };
    const auto open = [] (const LineNote& n) { return V::midiOf (n.velocity) >= 60 && V::midiOf (n.velocity) <= 119; };
    const auto rake = [] (const LineNote& n) { return V::midiOf (n.velocity) >= 120 && V::midiOf (n.velocity) <= 126; };
    const auto pinch = [] (const LineNote& n) { return V::midiOf (n.velocity) == 127; };
    for (int variation = 0; variation < 4; ++variation)
    {
        INFO ("variation " << variation);
        const auto rhythm = guitarRhythmIdeas (spans, 4.0, variation, tuning);
        const auto count = [] (const LineIdea& idea, auto&& is) { return (int) std::count_if (idea.notes.begin(), idea.notes.end(), is); };
        // No rakes in the rhythm parts (a rake is into a single note).
        for (const auto& idea : rhythm)
            CHECK (count (idea, rake) == 0);
        // Chugs: muted between open accents, never a pinch or rake.
        const auto& chugs = ideaNamed (rhythm, "Palm-muted chugs");
        CHECK (count (chugs, pinch) == 0);
        CHECK (count (chugs, muted) > count (chugs, open));
        for (const auto& n : chugs.notes)
            CHECK ((muted (n) || open (n)));
        for (const auto& s : spans)   // the bar's first hit open
            for (const auto& n : chugs.notes)
                if (std::abs (n.beat - s.start) < 1.0e-6)
                    CHECK (open (n));
        // Gallop: muted (the odd variations open each bar's first hit), never a pinch.
        const auto& gallop = ideaNamed (rhythm, "Gallop");
        CHECK (count (gallop, pinch) == 0);
        for (const auto& n : gallop.notes)
        {
            const auto barStart = std::fmod (n.beat, 4.0) < 1.0e-6;
            CHECK ((barStart && variation % 2 == 1 ? open (n) : muted (n)));
        }
        // Pedal-point: the pedal muted, stabs open, a pinch harmonic (one note) every second bar.
        const auto& pedal = ideaNamed (rhythm, "Pedal-point riff");
        CHECK (count (pedal, pinch) == 2);
        for (const auto& n : pedal.notes)
            if (pcOf (n.pitch) == 9 && n.pitch < tuning.lowest() + 12 && n.length < 0.45)
                CHECK (muted (n));
        // Open rhythm parts: all open.
        for (const auto* name : { "Power chords", "Octave chords", "Open strummed shapes" })
            for (const auto& n : ideaNamed (rhythm, name).notes)
                CHECK (open (n));
        // Leads: open, but rakes into phrase starts (every second bar's first note of the pentatonic
        // box and the blues run, the triplet lick's first note) and the pentatonic box's last note,
        // a pinch-harmonic squeal.
        for (const auto& idea : guitarLeadIdeas (spans, 4.0, variation, tuning))
        {
            INFO (idea.style);
            for (size_t i = 0; i < idea.notes.size(); ++i)
            {
                const auto& n = idea.notes[i];
                const auto bar = (int) std::floor (n.beat / 4.0 + 1.0e-9);
                const auto barStart = std::abs (n.beat - bar * 4.0) < 1.0e-6;
                const auto wantsRake = ((idea.style == "Pentatonic box" || idea.style == "Blues scale run") && barStart && bar % 2 == 0)
                                    || (idea.style == "Repeating triplet lick" && i == 0);
                if (idea.style == "Pentatonic box" && i + 1 == idea.notes.size())
                    CHECK (pinch (n));
                else if (wantsRake)
                    CHECK (rake (n));
                else
                    CHECK (open (n));
            }
        }
    }
}

TEST_CASE ("Guitar lead ideas: in the key's scales, a chord tone on every bar, in a lead's range, the last note on the chord (0.19.0)", "[lines]")
{
    for (const auto& key : { Key { 5, 0, true }, Key { 2, 0, true }, Key { 0, 0, false }, Key { 4, 0, false } })
    {
        const auto t = key.pitchClass();
        const auto chord = [&] (int offset, T type) { return Chord { pcOf (t + offset), type, X::none, -1 }; };
        const auto spans = key.minor ? bars ({ chord (0, T::minor), chord (8, T::major), chord (3, T::major), chord (7, T::major) }, key)
                                     : bars ({ chord (0, T::major), chord (7, T::major), chord (9, T::minor), chord (5, T::major) }, key);
        const auto scale = key.scale();
        for (const auto& tuning : { GuitarTuning::all()[0], GuitarTuning::named ("6-string standard (E)") })
            for (int variation = 0; variation < 3; ++variation)
            {
                const auto ideas = guitarLeadIdeas (spans, 4.0, variation, tuning);
                REQUIRE (ideas.size() == 6);
                for (const auto& idea : ideas)
                {
                    INFO (key.name() << ", " << tuning.name << ": " << idea.style << " (variation " << variation << ")");
                    CHECK (idea.part == LineIdea::Part::lead);
                    REQUIRE_FALSE (idea.notes.empty());
                    for (const auto& n : idea.notes)
                    {
                        CHECK (n.pitch >= tuning.strings[1] + 5);
                        CHECK (n.pitch <= tuning.strings.back() + 22);
                        // The key's notes, the chord's, or the blues note (the flat fifth; a major key's minor third).
                        const auto pc = pcOf (n.pitch);
                        const auto inKey = std::find (scale.begin(), scale.end(), pc) != scale.end();
                        const auto blue = pc == pcOf (t + 6) || (! key.minor && pc == pcOf (t + 3));
                        // The neoclassical run's raised seventh (harmonic minor), never over a chord holding the natural one.
                        const auto& chordThere = spanAt (spans, n.beat).chord;
                        const auto raised = key.minor && idea.style == "Neoclassical run" && pc == pcOf (t + 11) && ! isChordTone (chordThere, t + 10);
                        CHECK ((inKey || blue || raised || isChordTone (chordThere, n.pitch)));
                    }
                    // Every bar starts on a chord tone.
                    for (const auto& s : spans)
                    {
                        const auto at = notesAt (idea, s.start - spans.front().start);
                        REQUIRE (at.size() == 1);
                        CHECK (isChordTone (s.chord, at[0]));
                    }
                    CHECK (isChordTone (spans.back().chord, idea.notes.back().pitch));
                }
                // The neoclassical run in a minor key goes through the raised seventh (over V).
                if (key.minor)
                {
                    auto raised = false;
                    for (const auto& n : ideaNamed (ideas, "Neoclassical run").notes)
                        raised = raised || pcOf (n.pitch) == pcOf (t + 11);
                    CHECK (raised);
                }
            }
    }
    // The same chords and variation: the same notes.
    const Key am { 5, 0, true };
    const auto spans = bars ({ { 9, T::minor, X::none, -1 }, { 4, T::major, X::none, -1 } }, am);
    CHECK (guitarLeadIdeas (spans, 4.0, 2, GuitarTuning::all()[0])[5].notes == guitarLeadIdeas (spans, 4.0, 2, GuitarTuning::all()[0])[5].notes);
}

TEST_CASE ("Piano chord ideas: the chord's notes, the left hand low, the right hand moving little (0.19.0)", "[lines]")
{
    const Key c { 0, 0, false };
    const auto spans = bars ({ { 0, T::major, X::none, -1 }, { 9, T::minor, X::none, -1 }, { 5, T::major, X::none, -1 }, { 7, T::major, X::seven, -1 },
                               { 11, T::diminished, X::none, -1 } }, c);
    for (int variation = 0; variation < 3; ++variation)
    {
        const auto ideas = pianoChordIdeas (spans, 4.0, variation);
        REQUIRE (ideas.size() == 6);
        for (const auto& idea : ideas)
        {
            INFO (idea.style << " (variation " << variation << ")");
            CHECK (idea.part == LineIdea::Part::chords);
            REQUIRE_FALSE (idea.notes.empty());
            for (const auto& n : idea.notes)
            {
                CHECK (isChordTone (spanAt (spans, n.beat).chord, n.pitch));
                CHECK (n.pitch >= 33);
                CHECK (n.pitch <= 84);
            }
            // Something on every chord's first beat, its root lowest.
            for (const auto& s : spans)
            {
                auto at = notesAt (idea, s.start);
                REQUIRE_FALSE (at.empty());
                CHECK (pcOf (*std::min_element (at.begin(), at.end())) == s.chord.root);
            }
        }
        // Block chords: the right hand's top note moves at most a fourth from chord to chord.
        const auto& block = ideaNamed (ideas, "Block chords");
        auto previousTop = -1;
        for (const auto& s : spans)
        {
            const auto at = notesAt (block, s.start);
            const auto top = *std::max_element (at.begin(), at.end());
            if (previousTop >= 0)
                CHECK (std::abs (top - previousTop) <= 5);
            previousTop = top;
        }
    }
}

TEST_CASE ("Ideas onto the selected track, or a new one named for the part; the tuning remembered (0.19.0)", "[lines]")
{
    Studio studio (false);
    studio.addChordEvent (0.0, { 9, T::minor, X::none, -1 });
    studio.addChordEvent (4.0, { 7, T::major, X::none, -1 });
    const auto spans = studio.getLineChords();
    const auto guitarTrack = studio.getProject().tracks[0].id;   // (the owner's guitar VST on it)
    const auto tracks = studio.getProject().tracks.size();
    const auto rhythm = guitarRhythmIdeas (spans, 4.0, 0, GuitarTuning::all()[0])[3];
    CHECK (studio.addLinePart (rhythm, spans, guitarTrack) == guitarTrack);
    CHECK (studio.getProject().tracks.size() == tracks);
    REQUIRE (studio.getProject().findTrack (guitarTrack)->parts.size() == 1);
    CHECK (studio.getProject().findTrack (guitarTrack)->parts[0].name == "Guitar: Gallop");
    CHECK (studio.getProject().findTrack (guitarTrack)->parts[0].length == 8.0);
    // A new track for each part, named for it.
    const auto lead = studio.addLinePart (guitarLeadIdeas (spans, 4.0, 0, GuitarTuning::all()[0])[0], spans);
    CHECK (studio.getProject().findTrack (lead)->name == "Lead Guitar");
    CHECK (studio.getProject().findTrack (studio.addLinePart (pianoChordIdeas (spans, 4.0)[0], spans))->name == "Piano");
    // Not onto a track that can't take MIDI (frozen, disabled, audio): a new one instead.
    studio.setTracksDisabled ({ guitarTrack }, true);
    const auto other = studio.addLinePart (rhythm, spans, guitarTrack);
    CHECK (other != guitarTrack);
    CHECK (studio.getProject().findTrack (other)->name == "Guitar");

    CHECK (studio.getGuitarTuning() == "7-string drop A");
    studio.setGuitarTuning ("6-string drop C");
    CHECK (studio.getGuitarTuning() == "6-string drop C");
    studio.setGuitarTuning ("nonsense");
    CHECK (studio.getGuitarTuning() == "7-string drop A");
}

#include "ui/MainView.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/Finish.h"

TEST_CASE ("The IDEAS window: ideas heard, used, more of them (0.18.0); piano, guitar and bass tabs, the tuning (0.19.0)", "[lines][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    studio.addChordEvent (0.0, { 9, T::minor, X::none, -1 });
    studio.addChordEvent (4.0, { 5, T::major, X::none, -1 });
    studio.addChordEvent (8.0, { 0, T::major, X::none, -1 });
    studio.addChordEvent (12.0, { 7, T::major, X::none, -1 });
    view.openLineIdeas();
    REQUIRE (view.getLinesWindow() != nullptr);
    auto& page = view.getLinesWindow()->getPage();
    CHECK (page.getIdeas (LineIdea::Part::bass).size() == 6);
    CHECK (page.getIdeas (LineIdea::Part::melody).size() == 6);

    // Heard: that one playing (with the chords), then stopped.
    page.play (LineIdea::Part::melody, 2);
    REQUIRE (page.getPlaying().has_value());
    CHECK (page.getPlaying()->second == 2);
    CHECK (studio.isPreviewing());
    page.stop();
    CHECK_FALSE (studio.isPreviewing());
    CHECK_FALSE (page.getPlaying().has_value());

    // Used: a new track with a part under the four chords.
    const auto trackId = page.use (LineIdea::Part::bass, 0);
    REQUIRE (trackId != 0);
    REQUIRE (studio.getProject().findTrack (trackId)->parts.size() == 1);
    CHECK (studio.getProject().findTrack (trackId)->parts[0].length == 16.0);

    // More ideas: the next variation, other notes.
    const auto before = page.getIdeas (LineIdea::Part::melody);
    page.moreIdeas();
    CHECK (page.getVariation() == 1);
    auto changed = false;
    for (size_t i = 0; i < before.size(); ++i)
        changed = changed || before[i].notes != page.getIdeas (LineIdea::Part::melody)[i].notes;
    CHECK (changed);

    // Opened again on other chords (the selection): the same window, those chords.
    view.openLineIdeas ({ 0, 1 });
    CHECK (&view.getLinesWindow()->getPage() == &page);
    CHECK (page.getIdeas (LineIdea::Part::bass)[0].notes.back().beat < 8.0);

    // 0.19.0: tabs. Piano: chords and melody (twelve cards); guitar: rhythm and lead; bass alone.
    page.setTab (LinesPage::Tab::piano);
    CHECK (page.getNumCardsShown() == 12);
    CHECK (page.getIdeas (LineIdea::Part::chords).size() == 6);
    page.setTab (LinesPage::Tab::guitar);
    CHECK (page.getNumCardsShown() == 12);
    CHECK (page.getIdeas (LineIdea::Part::rhythm).size() == 6);
    CHECK (page.getIdeas (LineIdea::Part::lead).size() == 6);
    page.setTab (LinesPage::Tab::bass);
    CHECK (page.getNumCardsShown() == 6);
    // The tuning: the guitar ideas dealt again for it, nothing under its lowest string.
    page.setTuning ("6-string standard (E)");
    CHECK (studio.getGuitarTuning() == "6-string standard (E)");
    CHECK (page.getTuning().lowest() == 40);
    for (const auto& idea : page.getIdeas (LineIdea::Part::rhythm))
        for (const auto& n : idea.notes)
            CHECK (n.pitch >= 40);
    // USE IT onto the selected track (an instrument track).
    const auto selected = studio.getSelectedTrack();
    REQUIRE (studio.canTakeMidi (selected));
    const auto parts = studio.getProject().findTrack (selected)->parts.size();
    CHECK (page.use (LineIdea::Part::rhythm, 0, selected) == selected);
    CHECK (studio.getProject().findTrack (selected)->parts.size() == parts + 1);
}
