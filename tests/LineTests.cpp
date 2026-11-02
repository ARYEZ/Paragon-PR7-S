#include <catch2/catch_test_macros.hpp>

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

#include "ui/MainView.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/Finish.h"

TEST_CASE ("The BASS & MELODY window: twelve ideas, heard, used, more of them (0.18.0)", "[lines][ui]")
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
}
