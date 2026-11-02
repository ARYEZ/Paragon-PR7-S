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

TEST_CASE ("Guitar velocities set by the owner: the ideas use them, in-between values stay in the sustain range, kept in the settings (0.19.3)", "[lines]")
{
    GuitarVelocities v;
    CHECK (v.toString() == "34,44,54,80,96,110,123,127");
    CHECK (GuitarVelocities::fromString (v.toString()) == v);
    CHECK (GuitarVelocities::fromString ("nonsense") == GuitarVelocities {});
    CHECK (GuitarVelocities::fromString ("1,2,3") == GuitarVelocities {});
    v.set (6, 200);
    CHECK (v.rake == 127);   // (clamped to MIDI)
    CHECK (GuitarVelocities::nameOf (7) == "PINCH HARMONIC");
    CHECK (GuitarVelocities::layerOf (6) == "120-126");

    // Another layout: every idea follows it.
    GuitarVelocities mine;
    mine.muteTight = 20; mine.muteMid = 30; mine.muteLoose = 40;
    mine.sustainSoft = 70; mine.sustain = 85; mine.sustainHard = 100;
    mine.rake = 121; mine.pinch = 127;
    const Key am { 5, 0, true };
    const auto spans = bars ({ { 9, T::minor, X::none, -1 }, { 5, T::major, X::none, -1 }, { 0, T::major, X::none, -1 }, { 4, T::major, X::none, -1 } }, am);
    const auto& tuning = GuitarTuning::all()[0];
    const auto rhythm = guitarRhythmIdeas (spans, 4.0, 0, tuning, mine);
    const auto lead = guitarLeadIdeas (spans, 4.0, 0, tuning, mine);
    std::set<int> used;
    for (const auto* ideas : { &rhythm, &lead })
        for (const auto& idea : *ideas)
            for (const auto& n : idea.notes)
                used.insert (GuitarVelocity::midiOf (n.velocity));
    for (const auto u : used)
    {
        INFO (u);
        // Each a value set, or an in-between one inside [sustainSoft, sustainHard].
        const auto set = u == 20 || u == 30 || u == 40 || u == 121 || u == 127;
        CHECK ((set || (u >= 70 && u <= 100)));
    }
    CHECK (used.count (20) == 1);
    CHECK (used.count (121) == 1);
    CHECK (used.count (127) == 1);
    CHECK (used.count (100) == 1);
    // A sustain range squeezed small: in-betweens stay inside it (never into the mute layers).
    GuitarVelocities narrow;
    narrow.sustainSoft = 62; narrow.sustain = 63; narrow.sustainHard = 64;
    for (const auto& idea : guitarLeadIdeas (spans, 4.0, 1, tuning, narrow))
        for (const auto& n : idea.notes)
        {
            const auto m = GuitarVelocity::midiOf (n.velocity);
            CHECK ((m >= 62 || m == narrow.rake || m == narrow.pinch));
        }

    // The studio keeps them.
    Studio studio (false);
    CHECK (studio.getGuitarVelocities() == GuitarVelocities {});
    studio.setGuitarVelocities (mine);
    CHECK (studio.getGuitarVelocities() == mine);
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

TEST_CASE ("Humanize: timing in milliseconds, the bar's first beat steadiest, chords together, velocities in their layers, repeatable (0.20.0)", "[lines][humanize]")
{
    // Sixteen bars of sixteenths at 120 BPM (a beat is 500 ms), velocities in each guitar layer.
    std::vector<LineNote> notes;
    const int layered[] = { 44, 96, 123, 127 };
    for (int i = 0; i < 256; ++i)
        notes.push_back ({ i * 0.25, 0.2, 40 + i % 12, (float) layered[i % 4] / 127.0f });
    HumanizeOptions o;
    o.bpm = 120.0;
    o.layers = guitarLayers();
    o.seed = 7;

    // Amount 0: nothing changes.
    CHECK (humanize (notes, o) == notes);

    o.amount = 1.0f;
    const auto loose = humanize (notes, o);
    REQUIRE (loose.size() == notes.size());
    CHECK (loose == humanize (notes, o));   // repeatable
    o.seed = 8;
    CHECK (loose != humanize (notes, o));   // another seed, another performance
    o.seed = 7;

    // Each note within 12 ms (0.024 beats at 120), the bar's first beat within 0.3 of that; some moved.
    const auto msToBeats = 120.0 / 60000.0;
    auto moved = 0, velocities = 0;
    for (size_t i = 0; i < notes.size(); ++i)
    {
        // (Single notes 0.25 apart keep their order: the shift is under half the gap.)
        const auto& a = notes[i];
        const auto& b = loose[i];
        CHECK (b.pitch == a.pitch);
        const auto ms = std::abs (b.beat - a.beat) / msToBeats;
        const auto downbeat = std::fmod (a.beat, 4.0) < 1.0e-9;
        CHECK (ms <= (downbeat ? 12.0 * 0.3 : 12.0) + 1.0e-6);
        moved += ms > 0.5 ? 1 : 0;
        // Velocity: within 10, and inside its layer (a pinch stays 127, a rake stays a rake).
        const auto va = GuitarVelocity::midiOf (a.velocity), vb = GuitarVelocity::midiOf (b.velocity);
        CHECK (std::abs (vb - va) <= 10);
        if (va == 127) CHECK (vb == 127);
        if (va == 123) CHECK ((vb >= 120 && vb <= 126));
        if (va == 96)  CHECK ((vb >= 60 && vb <= 119));
        if (va == 44)  CHECK ((vb >= 1 && vb <= 59));
        velocities += vb != va ? 1 : 0;
        // Length: within 12%.
        CHECK (std::abs (b.length - a.length) <= a.length * 0.12 + 1.0e-9);
    }
    CHECK (moved > 100);
    CHECK (velocities > 100);

    // A strummed chord (six notes 0.02 beats apart) moves as one: order kept, spread within 25%.
    std::vector<LineNote> strum;
    for (int bar = 0; bar < 8; ++bar)
        for (int k = 0; k < 6; ++k)
            strum.push_back ({ bar * 4.0 + 1.0 + k * 0.02, 1.0, 40 + k * 5, 0.75f });
    const auto played = humanize (strum, o);
    for (int bar = 0; bar < 8; ++bar)
    {
        const auto first = played[(size_t) bar * 6].beat, last = played[(size_t) bar * 6 + 5].beat;
        CHECK (last - first >= 0.1 * 0.75 - 1.0e-9);
        CHECK (last - first <= 0.1 * 1.25 + 1.0e-9);
        for (int k = 0; k < 6; ++k)
            CHECK (played[(size_t) bar * 6 + k].pitch == 40 + k * 5);   // still low to high
    }
    // A block chord stays exactly together.
    std::vector<LineNote> block { { 1.0, 1.0, 48, 0.7f }, { 1.0, 1.0, 52, 0.7f }, { 1.0, 1.0, 55, 0.7f } };
    const auto blockPlayed = humanize (block, o);
    CHECK (blockPlayed[0].beat == blockPlayed[1].beat);
    CHECK (blockPlayed[1].beat == blockPlayed[2].beat);

    // FEEL: a lean added to every note (here 5 ms behind, with no randomness).
    HumanizeOptions lean;
    lean.feelMs = 5.0;
    lean.bpm = 120.0;
    const auto behind = humanize (notes, lean);
    for (size_t i = 1; i < notes.size(); ++i)
        CHECK (std::abs ((behind[i].beat - notes[i].beat) - 5.0 * msToBeats) < 1.0e-9);
    // Nothing before the part's start.
    lean.feelMs = -5.0;
    CHECK (humanize (notes, lean).front().beat == 0.0);
}

TEST_CASE ("Humanizing whole parts: MIDI parts loosened, audio parts left alone, one undo step (0.20.1)", "[lines][humanize]")
{
    Studio studio (false);
    const auto midiTrack = studio.getProject().tracks[0].id;
    Part riff { 0.0, 4.0, "Riff", {} };
    for (int i = 0; i < 16; ++i)
        riff.notes.push_back ({ i * 0.25, 0.2, 40 + i % 5, 96.0f / 127.0f });
    studio.addPart (midiTrack, riff);
    const auto partId = studio.getProject().tracks[0].parts[0].id;
    CHECK_FALSE (studio.humanizeParts ({}));
    CHECK_FALSE (studio.humanizeParts ({ partId }, 0.0f));
    const auto before = studio.getProject().tracks[0].parts[0].notes;
    CHECK (studio.humanizeParts ({ partId }, 1.0f));
    const auto& after = studio.getProject().tracks[0].parts[0].notes;
    REQUIRE (after.size() == before.size());
    CHECK (after != before);
    for (const auto& n : after)
    {
        const auto v = juce::roundToInt (n.velocity * 127.0f);
        CHECK ((v >= 60 && v <= 119));
    }
    studio.undo();
    CHECK (studio.getProject().tracks[0].parts[0].notes == before);
}

TEST_CASE ("Humanize's settings are kept (0.20.0)", "[lines][humanize]")
{
    Studio studio (false);
    CHECK (studio.getIdeasHumanize() == 0.0f);
    CHECK (studio.getIdeasFeel());
    CHECK (studio.getHumanizeAmount() == 0.5f);
    CHECK (studio.getHumanizeKeepsLayers());
    studio.setIdeasHumanize (0.4f);
    studio.setIdeasFeel (false);
    studio.setHumanizeAmount (2.0f);
    studio.setHumanizeKeepsLayers (false);
    CHECK (studio.getIdeasHumanize() == 0.4f);
    CHECK_FALSE (studio.getIdeasFeel());
    CHECK (studio.getHumanizeAmount() == 1.0f);
    CHECK_FALSE (studio.getHumanizeKeepsLayers());
}

#include "ui/MainView.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/Finish.h"

TEST_CASE ("The chord pads open IDEAS: their IDEAS key, for the chord track's chords (0.20.2)", "[lines][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    auto& pads = view.getChordPads();
    REQUIRE (pads.getIdeasKey().onClick != nullptr);
    studio.addChordEvent (0.0, { 9, T::minor, X::none, -1 });
    studio.addChordEvent (4.0, { 5, T::major, X::none, -1 });
    pads.getIdeasKey().onClick();
    REQUIRE (view.getLinesWindow() != nullptr);
    CHECK (view.getLinesWindow()->getPage().getIdeas (LineIdea::Part::rhythm).size() == 6);
}

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
    // VELOCITIES (0.19.3): the panel's changes reach the studio and deal the guitar ideas again; RESET.
    page.setTab (LinesPage::Tab::guitar);
    GuitarVelocityPanel panel (studio.getGuitarVelocities());
    auto changes = 0;
    panel.onChange = [&] (const GuitarVelocities& v) { ++changes; page.setGuitarVelocities (v); };
    panel.setValue (1, 40);   // PALM MUTE
    CHECK (changes == 1);
    CHECK (studio.getGuitarVelocities().muteMid == 40);
    auto found40 = false;
    for (const auto& idea : page.getIdeas (LineIdea::Part::rhythm))
        for (const auto& n : idea.notes)
            found40 = found40 || GuitarVelocity::midiOf (n.velocity) == 40;
    CHECK (found40);
    panel.setValue (1, 40);   // (the same: nothing to do)
    CHECK (changes == 1);
    panel.reset();
    CHECK (changes == 2);
    CHECK (studio.getGuitarVelocities() == GuitarVelocities {});
    CHECK (panel.getValues() == GuitarVelocities {});

    // HUMANIZE (0.20.0): the ideas loosened, the same each time (HEAR = USE IT); OFF puts them on the grid.
    const auto onGrid = page.getIdeas (LineIdea::Part::lead);
    HumanizePanel humanizePanel (studio.getIdeasHumanize(), studio.getIdeasFeel());
    humanizePanel.onChange = [&] (float amount, bool feel) { page.setHumanize (amount, feel); };
    humanizePanel.setAmount (0.6f);
    CHECK (studio.getIdeasHumanize() == 0.6f);
    CHECK (LinesPage::humanizeText (0.6f) == "HUMANIZE 60%");
    CHECK (LinesPage::humanizeText (0.0f) == "HUMANIZE OFF");
    const auto loosened = page.getIdeas (LineIdea::Part::lead);
    auto differs = false;
    for (size_t i = 0; i < onGrid.size(); ++i)
        differs = differs || loosened[i].notes != onGrid[i].notes;
    CHECK (differs);
    page.setGuitarVelocities (studio.getGuitarVelocities());   // (no change: not dealt again)
    page.moreIdeas();
    const auto again = page.getIdeas (LineIdea::Part::lead);
    page.setHumanize (0.6f, ! studio.getIdeasFeel());
    page.setHumanize (0.6f, ! studio.getIdeasFeel());
    CHECK (page.getIdeas (LineIdea::Part::lead)[0].notes == again[0].notes);   // the same performance each time
    humanizePanel.setAmount (0.0f);
    CHECK (studio.getIdeasHumanize() == 0.0f);

    // USE IT onto the selected track (an instrument track).
    const auto selected = studio.getSelectedTrack();
    REQUIRE (studio.canTakeMidi (selected));
    const auto parts = studio.getProject().findTrack (selected)->parts.size();
    CHECK (page.use (LineIdea::Part::rhythm, 0, selected) == selected);
    CHECK (studio.getProject().findTrack (selected)->parts.size() == parts + 1);
}
