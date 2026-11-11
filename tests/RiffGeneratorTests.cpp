#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <iostream>
#include <set>

#include "engine/Composer.h"
#include "engine/RiffGenerator.h"

using namespace pr7s;

namespace
{
    std::vector<ChordSpan> fourBars()
    {
        Key key;   // C major: A minor's chords
        key.minor = false;
        std::vector<ChordSpan> spans;
        const Chord chords[] = { Chord { 9, Chord::Type::minor }, Chord { 5, Chord::Type::major },
                                 Chord { 0, Chord::Type::major }, Chord { 7, Chord::Type::major } };
        for (int i = 0; i < 4; ++i)
            spans.push_back ({ i * 4.0, i * 4.0 + 4.0, chords[i], key });
        return spans;
    }

    int count (const std::vector<std::string>& bars, const std::string& of)
    {
        int n = 0;
        for (const auto& b : bars)
            for (const auto ch : b)
                n += of.find (ch) != std::string::npos ? 1 : 0;
        return n;
    }

    const std::string riffCharacters = "-=xXcCdDfFeEgGkKtTzZ*#obBhprvwuqyjnaAilms123456789";
}

TEST_CASE ("The riff generator: a thousand seeds, a thousand riffs (0.85.0)", "[riffgen]")
{
    for (const auto style : guitarStyles())
        for (const auto flavour : { RiffFlavour::any, RiffFlavour::tritone, RiffFlavour::chugs })
        {
            INFO (guitarStyleName (style) << " " << riffFlavourName (flavour));
            std::set<juce::String> seen;
            for (juce::uint32 seed = 1; seed <= 1000; ++seed)
            {
                RiffRecipe r;
                r.style = style;
                r.flavour = flavour;
                r.seed = seed;
                seen.insert (riffFingerprint (generateRiffBars (r)));
            }
            CHECK (seen.size() >= 950);
        }
}

TEST_CASE ("The riff generator: bars as long as the bar, every bar starting on a hit, only riff characters (0.85.0)", "[riffgen]")
{
    for (const auto steps : { 16, 12, 14 })
        for (const auto bars : { 1, 2, 4 })
            for (const auto section : drumSections())
                for (juce::uint32 seed = 1; seed <= 40; ++seed)
                {
                    RiffRecipe r;
                    r.style = guitarStyles()[seed % guitarStyles().size()];
                    r.flavour = riffFlavours()[seed % riffFlavours().size()];
                    r.section = section;
                    r.bars = bars;
                    r.seed = seed;
                    const auto out = generateRiffBars (r, steps);
                    REQUIRE ((int) out.size() == bars);
                    for (const auto& b : out)
                    {
                        CHECK ((int) b.size() == steps);
                        CHECK (b[0] != '-');
                        CHECK (b[0] != '=');
                        for (const auto ch : b)
                            CHECK (riffCharacters.find (ch) != std::string::npos);
                    }
                }
}

TEST_CASE ("The riff generator: the same recipe the same riff; the flavour heard; the section heard (0.85.0)", "[riffgen]")
{
    RiffRecipe r;
    r.style = GuitarStyle::groove;
    r.seed = 77;
    CHECK (generateRiffBars (r) == generateRiffBars (r));

    // TRITONE: flat fifths in (almost) every one; far more than CHUGS'.
    int tritoneWith = 0, tritones = 0, chugTritones = 0, chugMutes = 0, tritoneMutes = 0;
    int verseHeld = 0, chorusHeld = 0, verseRests = 0, breakdownRests = 0;
    for (juce::uint32 seed = 1; seed <= 300; ++seed)
    {
        RiffRecipe t;
        t.style = GuitarStyle::numetal;
        t.flavour = RiffFlavour::tritone;
        t.seed = seed;
        const auto tri = generateRiffBars (t);
        const auto n = count (tri, "eEbBvqyjnaAi");
        tritoneWith += n > 0 ? 1 : 0;
        tritones += n;
        tritoneMutes += count (tri, "xz*");   // (the djent kind chugs on a: not counted)
        auto c = t;
        c.flavour = RiffFlavour::chugs;
        const auto chug = generateRiffBars (c);
        chugTritones += count (chug, "eEbBvqyjnaAi");
        chugMutes += count (chug, "xz*");

        RiffRecipe s;
        s.style = GuitarStyle::metalcore;
        s.seed = seed;
        s.section = DrumSection::verse;
        const auto verse = generateRiffBars (s);
        s.section = DrumSection::chorus;
        const auto chorus = generateRiffBars (s);
        s.section = DrumSection::breakdown;
        const auto breakdown = generateRiffBars (s);
        verseHeld += count (verse, "=");
        chorusHeld += count (chorus, "=");
        verseRests += count (verse, "-");
        breakdownRests += count (breakdown, "-");
    }
    CHECK (tritoneWith >= 285);
    CHECK (tritones > 3 * chugTritones);
    CHECK (chugMutes > tritoneMutes);
    CHECK (chorusHeld > verseHeld);          // a chorus rings
    CHECK (breakdownRests > verseRests);     // a breakdown leaves space
}

TEST_CASE ("The riff generator: the riff played over the chords, in the guitar's range, a hit on every bar (0.85.0)", "[riffgen]")
{
    const auto spans = fourBars();
    const auto& tuning = GuitarTuning::all().front();
    for (juce::uint32 seed = 1; seed <= 50; ++seed)
    {
        RiffRecipe r;
        r.style = guitarStyles()[seed % guitarStyles().size()];
        r.flavour = riffFlavours()[seed % riffFlavours().size()];
        r.seed = seed;
        const auto idea = generateRiff (r, spans, 4.0, tuning, GuitarVelocities {});
        INFO (idea.style << ": " << idea.description);
        REQUIRE_FALSE (idea.notes.empty());
        CHECK (idea.style.startsWith ("Generated"));
        CHECK (idea.part == LineIdea::Part::rhythm);
        for (const auto& n : idea.notes)
        {
            CHECK (n.pitch >= tuning.lowest());
            CHECK (n.pitch <= tuning.highest());
            CHECK (n.beat >= 0.0);
            CHECK (n.beat < 16.0);
        }
        for (int bar = 0; bar < 4; ++bar)
        {
            bool hit = false;
            for (const auto& n : idea.notes)
                hit = hit || std::abs (n.beat - bar * 4.0) < 1.0e-6;
            CHECK (hit);
        }
    }
}

TEST_CASE ("The composer's guitar: generated riffs by default, the flavour saved, ROLL a new riff, the written riffs kept (0.85.0)", "[riffgen][composer]")
{
    ComposerState s;
    for (auto& lane : s.lanes)
        lane.on = true;
    ComposerContext c;
    c.humanize = 0.0f;
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    REQUIRE_FALSE (spans.empty());

    const auto generated = composeLayers (s, spans, c)[(size_t) ComposerLayer::guitar];
    CHECK (generated.style.startsWith ("Generated"));
    CHECK_FALSE (guitarRiffFingerprint (s, c).isEmpty());

    // The flavour goes into the state's text and back.
    s.riffFlavour = 3;
    auto back = ComposerState::fromString (s.toString());
    CHECK (back.riffFlavour == 3);
    CHECK_FALSE (back.writtenRiffs);
    s.writtenRiffs = true;
    back = ComposerState::fromString (s.toString());
    CHECK (back.writtenRiffs);
    // The written riffs: the style's own names, and no fingerprint.
    const auto written = composeLayers (s, spans, c)[(size_t) ComposerLayer::guitar];
    CHECK_FALSE (written.style.startsWith ("Generated"));
    CHECK (guitarRiffFingerprint (s, c).isEmpty());
    s.writtenRiffs = false;

    // ROLL: a new riff (its fingerprint) nearly every time.
    juce::Random random (5);
    std::set<juce::String> seen;
    for (int i = 0; i < 200; ++i)
    {
        rollLane (s, ComposerLayer::guitar, random);
        seen.insert (guitarRiffFingerprint (s, c));
    }
    CHECK (seen.size() >= 190);

    // A song text from before 0.85.0 (no flavour field): generated, ANY.
    auto old = ComposerState::fromString (ComposerState().toString().upToLastOccurrenceOf ("|", false, false));
    CHECK (old.riffFlavour == 0);
    CHECK_FALSE (old.writtenRiffs);
}

TEST_CASE ("The riff generator: BUSY thins or thickens the same riff, never another (0.85.0)", "[riffgen]")
{
    const auto hits = [] (const std::vector<std::string>& bars) {
        int n = 0;
        for (const auto& b : bars)
            for (const auto ch : b)
                n += ch != '-' && ch != '=' ? 1 : 0;
        return n;
    };
    for (juce::uint32 seed = 1; seed <= 300; ++seed)
    {
        RiffRecipe r;
        r.style = guitarStyles()[seed % guitarStyles().size()];
        r.flavour = riffFlavours()[seed % riffFlavours().size()];
        r.seed = seed;
        auto last = -1;
        for (const auto busy : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            r.busy = busy;
            const auto n = hits (generateRiffBars (r));
            CHECK (n >= last);
            last = n;
        }
        // The riff's hits on the beats are its own at any BUSY (a held note's tail may be cut by a chug).
        r.busy = 0.5f;
        const auto usual = generateRiffBars (r);
        r.busy = 1.0f;
        const auto busy = generateRiffBars (r);
        for (size_t b = 0; b < usual.size(); ++b)
            for (int i = 0; i < 16; i += 4)
                if (usual[b][(size_t) i] != '-' && usual[b][(size_t) i] != '=')
                    CHECK (usual[b][(size_t) i] == busy[b][(size_t) i]);
    }
}

// Prints riffs for reading (hidden): PARAGON_RIFF_STYLE (a style name), PARAGON_RIFF_FLAVOUR, PARAGON_RIFF_COUNT.
TEST_CASE ("The riff generator: print riffs (0.85.0)", "[.riffdump]")
{
    const auto env = [] (const char* name, const char* otherwise) {
        const auto* v = std::getenv (name);
        return juce::String (v != nullptr ? v : otherwise);
    };
    RiffRecipe r;
    r.style = guitarStyleNamed (env ("PARAGON_RIFF_STYLE", "GROOVE"));
    r.flavour = riffFlavourNamed (env ("PARAGON_RIFF_FLAVOUR", "TRITONE"));
    r.bars = env ("PARAGON_RIFF_BARS", "2").getIntValue();
    for (const auto section : { DrumSection::verse, DrumSection::chorus, DrumSection::breakdown })
    {
        r.section = section;
        std::cout << "== " << guitarStyleName (r.style) << " " << riffFlavourName (r.flavour) << " " << drumSectionName (section) << "\n";
        for (juce::uint32 seed = 1; seed <= (juce::uint32) env ("PARAGON_RIFF_COUNT", "8").getIntValue(); ++seed)
        {
            r.seed = seed;
            std::string line;
            for (const auto& b : generateRiffBars (r))
                line += "|" + b;
            std::cout << line << "|\n";
        }
    }
}
