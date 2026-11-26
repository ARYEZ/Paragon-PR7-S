// (0.219.0, the owner: "a metal song with the normal options for the sections but for the keyboards ... a cyberpunk
// feel, keep the other instruments in the rock and metal world") A lane's WORLD and the keys lane's CYBERPUNK parts.

#include <catch2/catch_test_macros.hpp>
#include <set>

#include "TestHelpers.h"
#include "engine/Composer.h"
#include "engine/KeyCheck.h"
#include "engine/LaneWorlds.h"
#include "engine/SectionTypes.h"
#include "engine/Studio.h"
#include "ui/ComposerPage.h"

using namespace pr7s;
using L = ComposerLayer;

namespace
{
    // A minor: Am F C G, a bar each, twice (8 bars).
    std::vector<ChordSpan> worldSpans (double start = 0.0)
    {
        Key aMinor;
        for (const auto& k : Key::all())
            if (k.minor && k.pitchClass() == 9)
                aMinor = k;
        const std::array<Chord, 4> chords { Chord { 9, Chord::Type::minor }, Chord { 5, Chord::Type::major },
                                            Chord { 0, Chord::Type::major }, Chord { 7, Chord::Type::major } };
        std::vector<ChordSpan> spans;
        for (int bar = 0; bar < 8; ++bar)
            spans.push_back ({ start + bar * 4.0, start + bar * 4.0 + 4.0, chords[(size_t) (bar % 4)], aMinor });
        return spans;
    }

    juce::String keyOf (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << ";";
        for (const auto& c : idea.controls)
            k << "c" << c.number << "=" << c.value << "@" << juce::roundToInt (c.beat * 96.0) << ";";
        return k;
    }
}

TEST_CASE ("0.219.0: every cyberpunk keys part plays, in key, on the grid, inside the section, and differs", "[composer][worlds]")
{
    const auto spans = worldSpans();
    const auto& options = cyberpunkKeysOptions();
    REQUIRE (options.size() == 13);
    CyberpunkRequest r;
    for (int bar = 0; bar < 8; ++bar)
        for (const auto k : { 0.0, 1.5, 2.0 })
            r.kicks.push_back (bar * 4.0 + k);
    r.guitarHits = { 0.0, 0.75, 1.5, 4.0, 4.75, 8.0 };
    std::set<juce::String> seen;
    for (int i = 0; i < options.size(); ++i)
    {
        INFO (options[i]);
        const auto idea = cyberpunkKeys (i, spans, r);
        CHECK (idea.part == LineIdea::Part::chords);
        CHECK (idea.style.startsWith ("Cyberpunk: "));
        CHECK (idea.description.isNotEmpty());
        CHECK (cyberpunkKeysDescription (i) == idea.description);
        REQUIRE_FALSE (idea.notes.empty());
        for (const auto& n : idea.notes)
        {
            CHECK (n.beat >= 0.0);
            CHECK (n.length > 0.0);
            CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
            CHECK (n.pitch >= 24);
            CHECK (n.pitch <= 108);
            CHECK (isPitchClassInKey (n.pitch % 12, spans.front().key));   // (diatonic chords: every note in A minor)
            const auto steps = n.beat / 0.25;
            CHECK (std::abs (steps - std::round (steps)) < 1.0e-6);       // (the sixteenth grid)
        }
        for (const auto& c : idea.controls)
        {
            CHECK ((c.number == 74 || c.number == 11));
            CHECK (c.beat >= 0.0);
            CHECK (c.beat <= 32.0 + 1.0e-6);
            CHECK (c.value >= 0);
            CHECK (c.value <= 127);
        }
        CHECK (seen.insert (keyOf (idea)).second);
    }
    // The filter moves where they're promised: the arpeggio opens, the riser sweeps up, the fade closes.
    const auto first = [] (const LineIdea& idea, int number) { for (const auto& c : idea.controls) if (c.number == number) return c.value; return -1; };
    const auto last = [] (const LineIdea& idea, int number) { int v = -1; for (const auto& c : idea.controls) if (c.number == number) v = c.value; return v; };
    const auto arp = cyberpunkKeys (0, spans, r);
    CHECK (first (arp, 74) < last (arp, 74));
    const auto riser = cyberpunkKeys (5, spans, r);
    CHECK (first (riser, 74) <= 10);
    CHECK (last (riser, 74) == 127);
    const auto fade = cyberpunkKeys (12, spans, r);
    CHECK (first (fade, 74) > last (fade, 74));
    // The riser climbs: its last bar sits higher than its first.
    double lowFirst = 0, lowLast = 0;
    int nf = 0, nl = 0;
    for (const auto& n : riser.notes)
    {
        if (n.beat < 4.0) { lowFirst += n.pitch; ++nf; }
        if (n.beat >= 28.0) { lowLast += n.pitch; ++nl; }
    }
    CHECK (lowLast / nl > lowFirst / nf + 12.0);
}

TEST_CASE ("0.219.0: the pumping pads duck on every kick; the stabs land on the kicks and the guitar's chugs", "[composer][worlds]")
{
    const auto spans = worldSpans();
    CyberpunkRequest r;
    for (int bar = 0; bar < 8; ++bar)
        for (const auto k : { 0.0, 1.5, 2.0 })
            r.kicks.push_back (bar * 4.0 + k);
    const auto pads = cyberpunkKeys (3, spans, r);
    int dips = 0;
    for (const auto& c : pads.controls)
        if (c.number == 11 && c.value <= 30)
        {
            ++dips;
            CHECK (std::find_if (r.kicks.begin(), r.kicks.end(), [&c] (double k) { return std::abs (k - c.beat) < 1.0e-9; }) != r.kicks.end());
        }
    CHECK (dips == (int) r.kicks.size());
    // No kicks: on the beats.
    CyberpunkRequest none;
    int beats = 0;
    for (const auto& c : cyberpunkKeys (3, spans, none).controls)
        if (c.number == 11 && c.value <= 30)
            ++beats;
    CHECK (beats == 32);
    // CHORD ON EVERY KICK: a chord starts on each kick and nowhere else.
    std::set<int> starts;
    for (const auto& n : cyberpunkKeys (10, spans, r).notes)
        starts.insert (juce::roundToInt (n.beat * 96.0));
    std::set<int> kicks;
    for (const auto k : r.kicks)
        kicks.insert (juce::roundToInt (k * 96.0));
    CHECK (starts == kicks);
    // SUB DROPS: a sub on every bar; stabs on the guitar's hits when given.
    r.guitarHits = { 0.75, 4.75, 9.5 };
    const auto subs = cyberpunkKeys (8, spans, r);
    int subNotes = 0;
    std::set<int> stabs;
    for (const auto& n : subs.notes)
    {
        if (n.pitch < 36)
        {
            ++subNotes;
            CHECK (std::fmod (n.beat, 4.0) < 1.0e-9);
        }
        else
            stabs.insert (juce::roundToInt (n.beat * 96.0));
    }
    CHECK (subNotes == 8);
    CHECK (stabs == std::set<int> { 72, 456, 912 });
}

TEST_CASE ("0.219.0: the keys lane's WORLD: its list, AUTOMATIC by section, saved, the rest of the band left as it was", "[composer][worlds]")
{
    ComposerState s;
    applySectionType (s, "Chorus", "DRIVING");
    s.lane (L::drums).style = 2;   // (chorus)
    const ComposerContext c;
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    const auto before = composeLayers (s, spans, c);

    auto w = s;
    w.lane (L::chords).world = (int) LaneWorld::cyberpunk;
    CHECK (sectionApproaches (w, L::chords) == cyberpunkKeysOptions());
    CHECK (baseSectionApproaches (w, L::chords) == cyberpunkKeysOptions());
    CHECK (sectionTypeWritesLane (L::chords, w, c));
    CHECK (sectionApproaches (w, L::bass) == sectionApproaches (s, L::bass));   // (the bass's list as it was)
    const auto after = composeLayers (w, spans, c);
    const auto& keys = after[(size_t) L::chords];
    CHECK (keys.style.startsWith ("Cyberpunk: "));
    // AUTOMATIC in a chorus: one of the chorus's own.
    const auto automatic = cyberpunkKeysAutomatic (WorldSection::chorus, w.lane (L::chords).variation);
    CHECK ((automatic == 3 || automatic == 4 || automatic == 10));
    CHECK (keys.style == "Cyberpunk: " + cyberpunkKeysOptions()[automatic].toLowerCase());
    for (const auto l : composerLayers())
        if (l != L::chords)
        {
            INFO (composerLayerName (l));
            CHECK (after[(size_t) l].notes.size() == before[(size_t) l].notes.size());
            CHECK (after[(size_t) l].style == before[(size_t) l].style);
        }
    // A chosen part.
    w.lane (L::chords).approach = 6;
    CHECK (composeLayers (w, spans, c)[(size_t) L::chords].style == "Cyberpunk: glitch stutter");
    // Each option its own part through the lane's choices (the style menu and the browser).
    const auto choices = composerChoices (L::chords, w, spans, c);
    REQUIRE ((int) choices.size() == cyberpunkKeysOptions().size());
    for (int i = 0; i < (int) choices.size(); ++i)
        CHECK (choices[(size_t) i].style == "Cyberpunk: " + cyberpunkKeysOptions()[i].toLowerCase());
    // Automatic by section: the verse's and the pre-chorus's.
    CHECK (cyberpunkKeysAutomatic (WorldSection::preChorus, 3) == 5);
    for (int v = 0; v < 6; ++v)
    {
        const auto verse = cyberpunkKeysAutomatic (WorldSection::verse, v);
        CHECK ((verse == 1 || verse == 2 || verse == 9));
    }
    // Saved with the state (and so with the song and a built section's recipe); older text: the section's world.
    const auto back = ComposerState::fromString (w.toString());
    CHECK (back.lane (L::chords).world == (int) LaneWorld::cyberpunk);
    CHECK (back.lane (L::bass).world == 0);
    CHECK (back.lane (L::chords).approach == 6);
    CHECK (ComposerState::fromString (s.toString()).lane (L::chords).world == 0);
}

TEST_CASE ("0.219.0: a cyberpunk part's filter and level moves go into the part as controllers", "[composer][worlds]")
{
    Studio studio (false);
    const auto spans = worldSpans();
    CyberpunkRequest r;
    const auto idea = cyberpunkKeys (5, spans, r);   // (the riser: CC 74 from 5 to 127)
    const auto track = studio.addLinePart (idea, spans, 0);
    REQUIRE (track != 0);
    const auto* t = studio.getProject().findTrack (track);
    REQUIRE (t != nullptr);
    REQUIRE (t->parts.size() == 1);
    const auto& part = t->parts[0];
    CHECK (part.notes.size() == idea.notes.size());
    std::vector<int> values;
    for (const auto& k : part.controllers)
        if (k.type == Controller::Type::cc && k.number == 74)
            values.push_back (k.value);
    REQUIRE (values.size() == 2);
    CHECK (values.front() == 5);
    CHECK (values.back() == 127);
    for (size_t i = 1; i < part.controllers.size(); ++i)
        CHECK (part.controllers[i - 1].start <= part.controllers[i].start);
}

TEST_CASE ("0.219.0: ARCHITECT's keys lane: the WORLD menu, the lane's parts, back to the section's", "[composer][worlds][ui]")
{
    Studio studio (false);
    ComposerPage page (studio);
    page.setRandomSeed (5);
    CHECK (ComposerPage::hasWorlds (L::chords));
    CHECK_FALSE (ComposerPage::hasWorlds (L::guitar));
    CHECK (page.getWorld (L::chords) == 0);
    page.styleMenuChosen (L::chords, ComposerPage::worldItems + (int) LaneWorld::cyberpunk);
    CHECK (page.getWorld (L::chords) == (int) LaneWorld::cyberpunk);
    CHECK (page.getIdea (L::chords).style.startsWith ("Cyberpunk: "));
    CHECK (page.getStatus().contains ("CYBERPUNK"));
    const auto options = page.laneOptions (L::chords);
    REQUIRE ((int) options.size() == cyberpunkKeysOptions().size());
    CHECK (options.front().name == cyberpunkKeysOptions()[0]);
    // Back to the section's world: the keys play as before.
    page.styleMenuChosen (L::chords, ComposerPage::worldItems);
    CHECK (page.getWorld (L::chords) == 0);
    CHECK_FALSE (page.getIdea (L::chords).style.startsWith ("Cyberpunk: "));
    // The guitar has no world: the item does nothing there.
    page.setWorld (L::guitar, 1);
    CHECK (page.getWorld (L::guitar) == 0);
}
