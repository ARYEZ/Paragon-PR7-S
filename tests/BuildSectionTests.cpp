#include <catch2/catch_test_macros.hpp>

#include "engine/Studio.h"
#include "engine/SectionPlan.h"
#include "ui/LinesPage.h"

// BUILD SECTION (0.57.0): a whole section onto tracks, then what comes next suggested from it.

using namespace pr7s;

namespace
{
    using T = Chord::Type;
    using X = Chord::Tension;
    using S = DrumSection;

    BuiltSection built (S kind, double start, int riff = 2, float mutes = 0.5f)
    {
        RiffSettings r;
        r.palmMutes = mutes;
        return { kind, start, start + 16.0, riff, r, 3 };
    }

    void fourChords (Studio& studio, double at)
    {
        studio.addChordEvent (at, { 9, T::minor, X::none, -1 });
        studio.addChordEvent (at + 4.0, { 5, T::major, X::none, -1 });
        studio.addChordEvent (at + 8.0, { 0, T::major, X::none, -1 });
        studio.addChordEvent (at + 12.0, { 7, T::major, X::none, -1 });
    }

    const Track* trackNamed (const Studio& studio, const juce::String& name)
    {
        for (const auto& t : studio.getProject().tracks)
            if (t.name == name)
                return &t;
        return nullptr;
    }
}

TEST_CASE ("What comes next: a chorus from the verse (opened up, or a new riff), then verse, bridge or breakdown, chorus, ending (0.57.0)", "[buildsection]")
{
    CHECK (suggestNextSections ({}, GuitarStyle::rock, 6).empty());
    // After a verse: the chorus, its riff the verse's opened up; a second with a new riff.
    const auto afterVerse = suggestNextSections ({ built (S::verse, 0.0, 2, 0.6f) }, GuitarStyle::metalcore, 6);
    REQUIRE (afterVerse.size() >= 2);
    CHECK (afterVerse[0].kind == S::chorus);
    CHECK (afterVerse[0].riff == 2);
    CHECK (afterVerse[0].riffSettings.palmMutes < 0.6f - 0.2f);
    CHECK (afterVerse[0].riffSettings.fills);
    CHECK (afterVerse[0].reason.contains ("opened up"));
    CHECK (afterVerse[0].variation == 3);   // (the same family of beats)
    CHECK (afterVerse[1].kind == S::chorus);
    CHECK (afterVerse[1].riff == 3);
    CHECK (std::is_sorted (afterVerse.begin(), afterVerse.end(), [] (const SectionSuggestion& a, const SectionSuggestion& b) { return a.score > b.score; }));
    // Verse, chorus: back to the verse, its riff exactly.
    const auto verse = built (S::verse, 0.0, 4, 0.7f);
    const auto afterChorus = suggestNextSections ({ verse, built (S::chorus, 16.0, 4, 0.3f) }, GuitarStyle::rock, 6);
    REQUIRE_FALSE (afterChorus.empty());
    CHECK (afterChorus[0].kind == S::verse);
    CHECK (afterChorus[0].riff == 4);
    CHECK (afterChorus[0].riffSettings == verse.riffSettings);
    // Two choruses in: metal styles a breakdown first (the chorus's riff in chugs), rock a bridge.
    const std::vector<BuiltSection> twice { built (S::verse, 0.0), built (S::chorus, 16.0), built (S::verse, 32.0), built (S::chorus, 48.0, 2, 0.3f) };
    const auto metal = suggestNextSections (twice, GuitarStyle::metalcore, 6);
    REQUIRE_FALSE (metal.empty());
    CHECK (metal[0].kind == S::breakdown);
    CHECK (metal[0].riffSettings.palmMutes > 0.6f);
    CHECK (metal[0].riffSettings.bars == 1);
    const auto rock = suggestNextSections (twice, GuitarStyle::rock, 6);
    REQUIRE_FALSE (rock.empty());
    CHECK (rock[0].kind == S::bridge);
    CHECK (std::none_of (rock.begin(), rock.end(), [] (const SectionSuggestion& s) { return s.kind == S::breakdown; }));
    // After the bridge: the chorus; then an ending is high; after the ending: nothing.
    auto song = twice;
    song.push_back (built (S::bridge, 64.0));
    CHECK (suggestNextSections (song, GuitarStyle::rock, 6)[0].kind == S::chorus);
    song.push_back (built (S::chorus, 80.0));
    const auto late = suggestNextSections (song, GuitarStyle::rock, 6);
    CHECK (late[0].kind == S::ending);
    song.push_back (built (S::ending, 96.0));
    CHECK (suggestNextSections (song, GuitarStyle::rock, 6).empty());
}

TEST_CASE ("BUILD SECTION in the studio: a track per part (reused), parts over the chords, rebuilding replaces, one undo step (0.57.0)", "[buildsection]")
{
    Studio studio (false);
    const auto before = (int) studio.getProject().tracks.size();
    std::vector<ChordSpan> spans { { 0.0, 8.0, { 9, T::minor, X::none, -1 }, Key { 5, 0, true } }, { 8.0, 16.0, { 5, T::major, X::none, -1 }, Key { 5, 0, true } } };
    LineIdea drums { LineIdea::Part::drums, "Verse", "", { { 0.0, 0.25, 36, 0.9f }, { 1.0, 0.25, 38, 0.9f } } };
    LineIdea bass { LineIdea::Part::bass, "Locked to the kick", "", { { 0.0, 1.0, 33, 0.8f } } };
    const auto tracks = studio.buildSection ({ drums, bass }, spans, "Verse");
    REQUIRE (tracks.size() == 2);
    CHECK ((int) studio.getProject().tracks.size() == before + 2);
    const auto* d = trackNamed (studio, "Drums");
    const auto* b = trackNamed (studio, "Bass");
    REQUIRE (d != nullptr);
    REQUIRE (b != nullptr);
    CHECK (d->drumMap == DrumMap::gm().name);
    REQUIRE (d->parts.size() == 1);
    CHECK (d->parts[0].name == "Verse: Verse");
    CHECK (d->parts[0].start == 0.0);
    CHECK (d->parts[0].length == 16.0);
    CHECK (d->parts[0].notes.size() == 2);
    CHECK (b->parts[0].name == "Verse: Locked to the kick");

    // Again over the same chords: replaced. Further on: the same tracks.
    studio.buildSection ({ drums, bass }, spans, "Verse");
    CHECK (trackNamed (studio, "Drums")->parts.size() == 1);
    auto later = spans;
    for (auto& s : later)
    {
        s.start += 16.0;
        s.end += 16.0;
    }
    studio.buildSection ({ drums, bass }, later, "Chorus");
    CHECK ((int) studio.getProject().tracks.size() == before + 2);
    REQUIRE (trackNamed (studio, "Drums")->parts.size() == 2);
    CHECK (trackNamed (studio, "Bass")->parts[1].name == "Chorus: Locked to the kick");
    studio.undo();
    CHECK (trackNamed (studio, "Drums")->parts.size() == 1);
    CHECK (trackNamed (studio, "Bass")->parts.size() == 1);

    // What was built is remembered (one per stretch), and forgotten with the song.
    studio.noteBuiltSection ({ S::verse, 0.0, 16.0, 0, {}, 0 });
    studio.noteBuiltSection ({ S::chorus, 16.0, 32.0, 0, {}, 0 });
    studio.noteBuiltSection ({ S::bridge, 0.0, 16.0, 0, {}, 0 });   // (over the verse: it replaces it)
    REQUIRE (studio.getBuiltSections().size() == 2);
    CHECK (studio.getBuiltSections()[0].kind == S::bridge);
    studio.newProject();
    CHECK (studio.getBuiltSections().empty());
}

TEST_CASE ("The next section's chords: the chord track's there, else these again (written onto the chord track) (0.57.0)", "[buildsection]")
{
    Studio studio (false);
    fourChords (studio, 0.0);
    const auto spans = studio.getLineChords();
    REQUIRE (spans.size() == 4);
    // Nothing after bar 4: the same four again from beat 16, onto the chord track.
    const auto next = studio.chordsForNextSection (spans, 16.0);
    REQUIRE (next.size() == 4);
    CHECK (next[0].start == 16.0);
    CHECK (next.back().end == 32.0);
    CHECK (next[2].chord == spans[2].chord);
    CHECK (studio.getChordTrack().events.size() == 8);
    // Chords already there: those (a different progression).
    studio.addChordEvent (32.0, { 2, T::minor, X::none, -1 });
    studio.addChordEvent (40.0, { 7, T::major, X::none, -1 });
    const auto theirs = studio.chordsForNextSection (spans, 32.0);
    REQUIRE (theirs.size() == 2);
    CHECK (theirs[0].chord.root == 2);
    CHECK (theirs[0].start == 32.0);
    CHECK (theirs.back().end == 48.0);
    CHECK (studio.getChordTrack().events.size() == 10);
}

TEST_CASE ("IDEAS' BUILD SECTION: the parts it writes, the bass on the beat's kick, NEXT builds the chorus after the verse (0.57.0)", "[buildsection][ui]")
{
    Studio studio (false);
    fourChords (studio, 0.0);
    LinesPage page (studio, studio.getLineChords());
    page.setGuitarStyle (GuitarStyle::metalcore);
    BuildChoice c;
    c.section = S::verse;
    page.setBuildChoice (c);
    CHECK (BuildChoice::fromString (c.toString()) == c);
    CHECK (BuildChoice::fromString ("nonsense") == BuildChoice {});

    const auto ideas = page.sectionIdeas (c);
    REQUIRE (ideas.size() == 5);   // drums, bass, guitar, lead, piano (melody off)
    CHECK (ideas[0].part == LineIdea::Part::drums);
    CHECK (ideas[0].style == "Verse");
    CHECK (ideas[1].part == LineIdea::Part::bass);
    // The bass on the drums' kick.
    const auto kicks = kickHits (ideas[0]);
    for (const auto& n : ideas[1].notes)
        CHECK (std::any_of (kicks.begin(), kicks.end(), [&n] (double k) { return std::abs (k - n.beat) < 1.0e-6; }));
    CHECK (ideas[2].part == LineIdea::Part::rhythm);
    CHECK (page.describeBuildPart (BuildChoice::drums).contains ("verse beat"));
    CHECK (page.describeBuildPart (BuildChoice::guitar).startsWith ("Riff: "));
    // The card last played on a tab is the one used.
    page.setTab (LinesPage::Tab::guitar);
    page.play (LineIdea::Part::lead, 4);
    page.stop();
    CHECK (page.sectionIdeas (c)[3].style == page.getIdeas (LineIdea::Part::lead)[4].style);

    const auto tracks = page.buildSection();
    REQUIRE (tracks.size() == 5);
    for (const auto* name : { "Drums", "Bass", "Guitar", "Lead Guitar", "Piano" })
    {
        INFO (name);
        REQUIRE (trackNamed (studio, name) != nullptr);
        CHECK (trackNamed (studio, name)->parts.size() == 1);
    }
    REQUIRE (studio.getBuiltSections().size() == 1);
    CHECK (studio.getBuiltSections()[0].kind == S::verse);

    // NEXT: the chorus, after the verse, from the verse's riff opened up.
    const auto mutes = studio.getRiffSettings().palmMutes;
    const auto next = page.getNextSuggestions();
    REQUIRE_FALSE (next.empty());
    CHECK (next[0].kind == S::chorus);
    page.buildNext (next[0]);
    CHECK (studio.getRiffSettings().palmMutes < mutes);
    CHECK (page.getBuildChoice().section == S::chorus);
    REQUIRE (studio.getBuiltSections().size() == 2);
    CHECK (studio.getBuiltSections()[1].start == 16.0);
    const auto* guitar = trackNamed (studio, "Guitar");
    REQUIRE (guitar->parts.size() == 2);
    CHECK (guitar->parts[1].start == 16.0);
    CHECK (guitar->parts[1].name.startsWith ("Chorus: "));
    CHECK (trackNamed (studio, "Drums")->parts[1].name == "Chorus: Chorus");
    CHECK (page.getChords().front().start == 16.0);
    CHECK (studio.getChordTrack().events.size() == 8);

    // The panel: sections, parts, BUILD, the NEXT keys.
    BuildSectionPanel panel (c, [] (BuildChoice::Part) { return juce::String ("x"); }, page.getNextSuggestions());
    CHECK (panel.getNumNextKeys() >= 1);
    BuildChoice seen;
    panel.onChange = [&] (const BuildChoice& x) { seen = x; };
    auto off = c;
    off.parts[BuildChoice::piano] = false;
    panel.setChoice (off);
    CHECK (seen == off);
    auto built = false;
    panel.onBuild = [&] { built = true; };
    panel.getBuildKey().onClick();
    CHECK (built);
    CHECK (panel.getBuildKey().getButtonText() == "BUILD VERSE");
}
