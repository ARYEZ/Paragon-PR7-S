#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/ProgressionsPage.h"

#include <set>

// 0.40.0, the progression library: progressions by genre and mood, in any key.

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    Key keyNamed (const juce::String& name)
    {
        for (const auto& k : Key::all())
            if (k.name() == name)
                return k;
        FAIL ("no key " << name);
        return {};
    }
    juce::StringArray namesOf (const std::vector<ProgressionStep>& steps, const Key& key)
    {
        juce::StringArray names;
        for (const auto& s : steps)
            names.add (s.chord.name (key));
        return names;
    }
}

TEST_CASE ("0.40.0: numerals read: case, accidentals, chord kinds, tensions, slash basses, lengths", "[progressions]")
{
    using T = Chord::Type;
    using X = Chord::Tension;
    LibraryProgression::Step s;
    REQUIRE (parseNumeral ("vi", false, s));
    CHECK (s.offset == 9);
    CHECK (s.type == T::minor);
    REQUIRE (parseNumeral ("bVII7", false, s));
    CHECK (s.offset == 10);
    CHECK (s.type == T::major);
    CHECK (s.tension == X::seven);
    REQUIRE (parseNumeral ("VI", true, s));        // (minor's own sixth: a minor sixth up)
    CHECK (s.offset == 8);
    REQUIRE (parseNumeral ("bII", true, s));
    CHECK (s.offset == 1);
    REQUIRE (parseNumeral ("#ivo7", false, s));
    CHECK (s.offset == 6);
    CHECK (s.type == T::diminished);
    CHECK (s.tension == X::dim7);
    REQUIRE (parseNumeral ("iih7", true, s));
    CHECK (s.type == T::diminished);
    CHECK (s.tension == X::seven);
    REQUIRE (parseNumeral ("Imaj7:2", false, s));
    CHECK (s.tension == X::maj7);
    CHECK_THAT (s.bars, WithinAbs (2.0, 1.0e-9));
    REQUIRE (parseNumeral ("V/7", false, s));      // G/B in C
    CHECK (s.offset == 7);
    CHECK (s.bassOffset == 11);
    REQUIRE (parseNumeral ("I/1", false, s));      // (the root in the bass: a plain chord)
    CHECK (s.bassOffset == -1);
    REQUIRE (parseNumeral ("Isus2", false, s));
    CHECK (s.type == T::sus2);
    CHECK_FALSE (parseNumeral ("X", false, s));
    CHECK_FALSE (parseNumeral ("I7x", false, s));
    CHECK_FALSE (parseNumeral ("I/9", false, s));
    CHECK_FALSE (parseNumeral ("I:0", false, s));
}

TEST_CASE ("0.40.0: the library: every entry read, names unique, every genre and mood used, many of them", "[progressions]")
{
    const auto& library = progressionLibrary();
    // Every line of the table was read (none dropped for a typo).
    size_t written = 0;
    for (const auto& p : library)
        written += (size_t) juce::StringArray::fromTokens (p.numerals, " ", {}).size() == p.steps.size() ? 1 : 0;
    CHECK (written == library.size());
    CHECK (library.size() >= 150);
    std::set<juce::String> names;
    for (const auto& p : library)
    {
        names.insert (p.name);
        CHECK (progressionGenres().contains (p.genre));
        REQUIRE_FALSE (p.moods.isEmpty());
        for (const auto& m : p.moods)
            CHECK (progressionMoods().contains (m));
        CHECK (p.totalBars() >= 2.0);
    }
    CHECK (names.size() == library.size());
    for (const auto& g : progressionGenres())
    {
        ProgressionFilter f;
        f.genre = g;
        CHECK (findProgressions (Key {}, f).size() >= 6);
    }
    for (const auto& m : progressionMoods())
    {
        ProgressionFilter f;
        f.mood = m;
        CHECK (findProgressions (Key {}, f).size() >= 6);
    }
}

TEST_CASE ("0.40.0: in a key: spelled in it; the other mode on the relative key, so plain chords stay in the key's notes", "[progressions]")
{
    const auto& anthem = progressionLibrary()[(size_t) progressionNamed ("Four-chord anthem")];
    CHECK (namesOf (anthem.in (keyNamed ("G major")), keyNamed ("G major")) == juce::StringArray { "G", "D", "Em", "C" });
    CHECK (namesOf (anthem.in (keyNamed ("Eb major")), keyNamed ("Eb major")) == juce::StringArray { "Eb", "Bb", "Cm", "Ab" });
    // A major progression in A minor: on C major.
    CHECK (namesOf (anthem.in (keyNamed ("A minor")), keyNamed ("A minor")) == juce::StringArray { "C", "G", "Am", "F" });
    // A minor one in D major: on B minor.
    const auto& epic = progressionLibrary()[(size_t) progressionNamed ("Festival")];
    CHECK (namesOf (epic.in (keyNamed ("D major")), keyNamed ("D major")) == juce::StringArray { "Bm", "G", "D", "A" });
    // A chord written with a flat is named with one: bV in A minor is Eb, not D#.
    const auto& dread = progressionLibrary()[(size_t) progressionNamed ("Tritone dread")];
    const auto dreadSteps = dread.in (keyNamed ("A minor"));
    CHECK (dreadSteps[1].name (keyNamed ("A minor")) == "Bb");
    CHECK (dreadSteps[2].name (keyNamed ("A minor")) == "Eb");
    CHECK (dreadSteps[2].numeralIn (keyNamed ("A minor")) == "bV");
    CHECK (dreadSteps[0].numeralIn (keyNamed ("A minor")) == "i");
    // The slash bass and lengths come through.
    const auto& heartbreak = progressionLibrary()[(size_t) progressionNamed ("Heartbreak ballad")];
    const auto steps = heartbreak.in (Key {});
    CHECK (steps[1].chord.bass == 11);   // G/B
    // In all 30 keys, every chord written without an accidental has its root on the key's scale
    // (the other mode's on the relative key: the same notes).
    for (const auto& key : Key::all())
    {
        const auto scale = key.scale();
        for (const auto& p : progressionLibrary())
        {
            const auto words = juce::StringArray::fromTokens (p.numerals, " ", {});
            const auto steps = p.in (key);
            for (int i = 0; i < words.size(); ++i)
                if (words[i][0] != 'b' && words[i][0] != '#')
                    CHECK (std::find (scale.begin(), scale.end(), steps[(size_t) i].chord.root) != scale.end());
        }
    }
}

TEST_CASE ("0.40.0: finding: genre, mood, words (chords and numerals too), favourites, the key's own mode first", "[progressions]")
{
    ProgressionFilter f;
    const auto all = findProgressions (Key {}, f);
    CHECK (all.size() == progressionLibrary().size());
    // C major: major ones first.
    auto seenMinor = false;
    for (const auto i : all)
    {
        if (progressionLibrary()[(size_t) i].minor)
            seenMinor = true;
        else
            CHECK_FALSE (seenMinor);
    }
    f.ownModeOnly = true;
    for (const auto i : findProgressions (keyNamed ("E minor"), f))
        CHECK (progressionLibrary()[(size_t) i].minor);
    f = {};
    f.genre = "METAL";
    f.mood = "DARK";
    for (const auto i : findProgressions (keyNamed ("A minor"), f))
    {
        CHECK (progressionLibrary()[(size_t) i].genre == "METAL");
        CHECK (progressionLibrary()[(size_t) i].moods.contains ("DARK"));
    }
    f = {};
    f.text = "phrygian";
    CHECK (findProgressions (Key {}, f).size() >= 2);
    f.text = "bvii";   // (a numeral)
    CHECK_FALSE (findProgressions (Key {}, f).empty());
    f.text = "bb";     // (a chord in F major: Bb)
    const auto withBb = findProgressions (keyNamed ("F major"), f);
    CHECK (std::find (withBb.begin(), withBb.end(), progressionNamed ("Four-chord anthem")) != withBb.end());
    f = {};
    f.favoritesOnly = true;
    f.favorites = { "Doom", "Bossa nova" };
    CHECK (findProgressions (Key {}, f).size() == 2);
    // The drag's text comes back as it went.
    const auto steps = progressionLibrary()[(size_t) progressionNamed ("Bossa nova")].in (keyNamed ("F major"));
    const auto back = progressionFromText (progressionToText (steps));
    REQUIRE (back.size() == steps.size());
    for (size_t i = 0; i < steps.size(); ++i)
    {
        CHECK (back[i].chord == steps[i].chord);
        CHECK_THAT (back[i].bars, WithinAbs (steps[i].bars, 1.0e-9));
    }
    CHECK (progressionFromText ("nonsense").empty());
}

TEST_CASE ("0.40.0: onto the chord track: from the bar, each chord its bars, what was there replaced, one undo step", "[progressions]")
{
    Studio studio (false);
    studio.addChordEvent (0.0, Chord { 2, Chord::Type::minor, Chord::Tension::none, -1 });
    studio.addChordEvent (6.0, Chord { 9, Chord::Type::minor, Chord::Tension::none, -1 });
    const auto steps = progressionLibrary()[(size_t) progressionNamed ("Two-five-one")].in (Key {});   // Dm7 G7 Cmaj7:2
    studio.addProgressionToChordTrack (steps, 5.0);   // (the bar at 4)
    const auto& events = studio.getChordTrack().events;
    REQUIRE (events.size() == 4);
    CHECK (events[0].beat == 0.0);
    CHECK_THAT (events[1].beat, WithinAbs (4.0, 1.0e-9));
    CHECK_THAT (events[2].beat, WithinAbs (8.0, 1.0e-9));
    CHECK_THAT (events[3].beat, WithinAbs (12.0, 1.0e-9));
    CHECK (events[3].chord.tension == Chord::Tension::maj7);
    CHECK (studio.getChordTrack().shown);
    studio.undo();
    CHECK (studio.getChordTrack().events.size() == 2);
    // The spans heard: each as long as its bars.
    const auto spans = studio.progressionSpans (steps, 0.0, Key {});
    REQUIRE (spans.size() == 3);
    CHECK_THAT (spans[2].start, WithinAbs (8.0, 1.0e-9));
    CHECK_THAT (spans[2].end, WithinAbs (16.0, 1.0e-9));
    // Favourites (no settings file: kept for the run).
    studio.setProgressionFavorite ("Doom", true);
    studio.setProgressionFavorite ("Doom", true);
    CHECK (studio.getProgressionFavorites() == juce::StringArray { "Doom" });
    studio.setProgressionFavorite ("Doom", false);
    CHECK (studio.getProgressionFavorites().isEmpty());
}

TEST_CASE ("0.40.0: the PROGRESSIONS window: filters with counts, hear, to the track, to the pads, to IDEAS", "[progressions][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    ProgressionsPage page (studio);
    page.setSize (1500, 860);
    CHECK (page.getShown().size() == progressionLibrary().size());
    CHECK (page.countFor ("METAL", "") == (int) std::count_if (progressionLibrary().begin(), progressionLibrary().end(),
                                                              [] (const LibraryProgression& p) { return p.genre == "METAL"; }));
    page.setGenre ("JAZZ");
    for (const auto i : page.getShown())
        CHECK (progressionLibrary()[(size_t) i].genre == "JAZZ");
    page.setMood ("DARK");
    CHECK (page.getShown().size() == (size_t) page.countFor ("JAZZ", "DARK"));
    page.setGenre ("");
    page.setMood ("");
    page.setSearch ("andalusian");
    REQUIRE (page.getShown().size() == 1);
    const auto andalusian = page.getShown()[0];
    page.setKey (keyNamed ("E minor"));
    CHECK (page.getShown().size() == 1);
    CHECK (namesOf (page.stepsOf (andalusian), keyNamed ("E minor")) == juce::StringArray { "Em", "D", "C", "B" });
    page.hear (andalusian);
    CHECK (page.getHearing() == andalusian);
    CHECK (studio.isPreviewing());
    page.hear (andalusian);   // (again: stop)
    CHECK (page.getHearing() == -1);
    CHECK_FALSE (studio.isPreviewing());
    page.toChordTrack (andalusian);
    CHECK (studio.getChordTrack().events.size() == 4);
    CHECK (page.toPads (andalusian) == 4);
    REQUIRE (studio.getChordPads().pads[3].chord.has_value());
    CHECK (studio.getChordPads().pads[3].chord->root == 11);
    std::vector<int> ideasFor;
    page.onIdeas = [&ideasFor] (std::vector<int> events) { ideasFor = std::move (events); };
    page.toIdeas (andalusian);
    CHECK (ideasFor == std::vector<int> { 0, 1, 2, 3 });
    page.setSearch ("");
    page.toggleFavorite (andalusian);
    page.setFavoritesOnly (true);
    CHECK (page.getShown() == std::vector<int> { andalusian });
    CHECK (page.createComponentSnapshot (page.getLocalBounds()).isValid());
}
