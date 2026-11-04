#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/LinesPage.h"

#include <set>

// 0.41.0, better melodies: a composer with density, range, steps vs leaps, phrases and motifs.

using namespace pr7s;

namespace
{
    using T = Chord::Type;
    const Chord C { 0, T::major, Chord::Tension::none, -1 }, Am { 9, T::minor, Chord::Tension::none, -1 },
                F { 5, T::major, Chord::Tension::none, -1 }, G { 7, T::major, Chord::Tension::none, -1 };
    const Key cMajor {};

    // Eight bars: C Am F G, twice.
    std::vector<ChordSpan> song()
    {
        std::vector<ChordSpan> spans;
        const Chord chords[] { C, Am, F, G };
        for (int i = 0; i < 8; ++i)
            spans.push_back ({ i * 4.0, i * 4.0 + 4.0, chords[i % 4], cMajor });
        return spans;
    }

    const ChordSpan& spanAt (const std::vector<ChordSpan>& spans, double beat)
    {
        for (const auto& s : spans)
            if (beat < s.end - 1.0e-9)
                return s;
        return spans.back();
    }

    bool contains (const std::vector<int>& list, int v) { return std::find (list.begin(), list.end(), v) != list.end(); }

    // The onsets of one phrase, from its start.
    std::vector<double> onsetsIn (const LineIdea& idea, double from, double to)
    {
        std::vector<double> out;
        for (const auto& n : idea.notes)
            if (n.beat >= from - 1.0e-9 && n.beat < to - 1.0e-9)
                out.push_back (std::round ((n.beat - from) * 1000.0) / 1000.0);
        return out;
    }

    double meanStep (const LineIdea& idea)
    {
        double total = 0.0;
        for (size_t i = 1; i < idea.notes.size(); ++i)
            total += std::abs (idea.notes[i].pitch - idea.notes[i - 1].pitch);
        return idea.notes.size() > 1 ? total / (double) (idea.notes.size() - 1) : 0.0;
    }
}

TEST_CASE ("0.41.0: composed melodies: in range, chord tones on the beats, the key between, inside the chords, home at the end, repeatable", "[melody]")
{
    const auto spans = song();
    for (const auto& settings : { MelodySettings {}, MelodySettings { 0.9f, 55, 72, 0.8f, 1, 0.3f, 0.6f, true }, MelodySettings { 0.1f, 67, 84, 0.0f, 4, 1.0f, 0.0f, false } })
    {
        const auto ideas = composedMelodies (spans, 4.0, settings, 0);
        REQUIRE (ideas.size() == 6);
        for (const auto& idea : ideas)
        {
            CHECK (idea.part == LineIdea::Part::tune);
            REQUIRE (idea.notes.size() >= 4);
            for (const auto& n : idea.notes)
            {
                CHECK (n.pitch >= settings.low);
                CHECK (n.pitch <= settings.high);
                CHECK (n.beat >= 0.0);
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
                const auto& span = spanAt (spans, n.beat);
                const auto tones = span.chord.pitchClasses();
                const auto onBeat = std::abs (n.beat - std::round (n.beat)) < 1.0e-6;
                if (onBeat)
                    CHECK (std::find (tones.begin(), tones.end(), n.pitch % 12) != tones.end());
                else
                    CHECK (contains (availableNotes (span.chord, span.key), n.pitch % 12));
                // (Not held into the next chord, but the last note.)
                if (&n != &idea.notes.back())
                    CHECK (n.beat + n.length <= span.end + 1.0e-6);
            }
            // Home: the last note on G's root (G), held to the end.
            CHECK (idea.notes.back().pitch % 12 == 7);
            CHECK_THAT (idea.notes.back().beat + idea.notes.back().length / 0.96, Catch::Matchers::WithinAbs (32.0, 0.05));
        }
        // The same seed, the same melody; MORE IDEAS (another variation), another.
        const auto again = composedMelodies (spans, 4.0, settings, 0);
        const auto other = composedMelodies (spans, 4.0, settings, 1);
        CHECK (again[2].notes == ideas[2].notes);
        CHECK_FALSE (other[2].notes == ideas[2].notes);
    }
}

TEST_CASE ("0.41.0: the controls: density adds notes, leaps widen the steps, the phrase repeats as the form says", "[melody]")
{
    const auto spans = song();
    const auto& form = melodyForms()[0];   // Song form (AABA)
    auto count = [&] (float density)
    {
        MelodySettings m;
        m.density = density;
        m.rests = 0.0f;
        size_t total = 0;
        for (juce::uint32 seed = 1; seed <= 10; ++seed)
            total += composeMelody (spans, 4.0, m, form, seed).notes.size();
        return total;
    };
    CHECK (count (0.9f) > count (0.5f) * 3 / 2);
    CHECK (count (0.5f) > count (0.1f) * 3 / 2);

    auto steps = [&] (float leaps)
    {
        MelodySettings m;
        m.leaps = leaps;
        double total = 0.0;
        for (juce::uint32 seed = 1; seed <= 10; ++seed)
            total += meanStep (composeMelody (spans, 4.0, m, form, seed));
        return total / 10.0;
    };
    CHECK (steps (1.0f) > steps (0.0f) + 0.8);

    // REPEAT 100%: the A phrases (bars 1-2, 3-4, 7-8) have one rhythm; B (5-6) its own.
    MelodySettings exact;
    exact.repetition = 1.0f;
    exact.rests = 0.0f;
    for (juce::uint32 seed = 1; seed <= 5; ++seed)
    {
        const auto idea = composeMelody (spans, 4.0, exact, form, seed);
        const auto a1 = onsetsIn (idea, 0.0, 8.0), a2 = onsetsIn (idea, 8.0, 16.0), a3 = onsetsIn (idea, 24.0, 32.0);
        CHECK (a1 == a2);
        CHECK (a1 == a3);
    }
    // One-bar phrases in a hook (AAAB): bars 1, 2 and 3 alike.
    exact.phraseBars = 1;
    const auto hook = composeMelody (spans, 4.0, exact, melodyForms()[2], 3);
    CHECK (onsetsIn (hook, 0.0, 4.0) == onsetsIn (hook, 4.0, 8.0));
    CHECK (onsetsIn (hook, 0.0, 4.0) == onsetsIn (hook, 8.0, 12.0));

    // The climbing motif over one chord: each return starts higher.
    std::vector<ChordSpan> oneChord { { 0.0, 32.0, C, cMajor } };
    MelodySettings wide;
    wide.low = 48;
    wide.high = 96;
    const auto climb = composeMelody (oneChord, 4.0, wide, melodyForms()[4], 2);
    auto firstAt = [&] (double beat) { for (const auto& n : climb.notes) if (std::abs (n.beat - beat) < 1.0e-6) return n.pitch; return -1; };
    CHECK (firstAt (8.0) > firstAt (0.0));
    CHECK (firstAt (16.0) > firstAt (8.0));
}

TEST_CASE ("0.41.0: the settings: kept in bounds, as text and back, the studio's", "[melody]")
{
    MelodySettings m { 2.0f, 90, 50, -1.0f, 3, 0.5f, 0.2f, true };
    const auto l = m.limited();
    CHECK (l.density == 1.0f);
    CHECK (l.leaps == 0.0f);
    CHECK (l.phraseBars == 2);
    CHECK (l.high - l.low >= MelodySettings::leastSpan);
    CHECK (MelodySettings::fromString (l.toString()) == l);
    CHECK (MelodySettings::fromString ("nonsense") == MelodySettings {});
    Studio studio (false);
    CHECK (studio.getMelodySettings() == MelodySettings {});
    MelodySettings mine;
    mine.density = 0.8f;
    mine.phraseBars = 4;
    studio.setMelodySettings (mine);
    CHECK (studio.getMelodySettings() == mine);
}

TEST_CASE ("0.41.0: IDEAS' MELODY tab: six forms, MELODY SETTINGS deals them again, heard with the chords, used onto a track", "[melody][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    LinesPage page (studio, song());
    page.setSize (1240, 760);
    page.setTab (LinesPage::Tab::melody);
    CHECK (page.getNumCardsShown() == 6);
    const auto& ideas = page.getIdeas (LineIdea::Part::tune);
    REQUIRE (ideas.size() == 6);
    CHECK (ideas[0].style == "Song form");
    const auto before = ideas[1].notes.size();
    MelodySettings busy;
    busy.density = 1.0f;
    page.setMelodySettings (busy);
    CHECK (studio.getMelodySettings() == busy);
    CHECK (page.getIdeas (LineIdea::Part::tune)[1].notes.size() > before);
    page.play (LineIdea::Part::tune, 0);
    CHECK (studio.isPreviewing());
    page.stop();
    const auto trackId = page.use (LineIdea::Part::tune, 0);
    const auto* track = studio.getProject().findTrack (trackId);
    REQUIRE (track != nullptr);
    CHECK (track->name.startsWith ("Melody"));
    // The panel: a change reaches its owner.
    MelodyPanel panel (busy);
    MelodySettings seen;
    panel.onChange = [&seen] (const MelodySettings& s) { seen = s; };
    auto m = busy;
    m.phraseBars = 1;
    panel.setSettings (m);
    CHECK (seen.phraseBars == 1);
    CHECK (MelodyPanel::summary (m) == "MELODY: C3-G4, 1 BAR");
    CHECK (page.createComponentSnapshot (page.getLocalBounds()).isValid());
}
