// 0.127.0 (the owner: the rhythm guitar lane was built for Shreddage, whose power chord articulation (C#-1) sounds
// better than power chords written out note by note): every power chord the composer writes for the guitar is its
// root alone after C#-1, single notes back on C-1.

#include <catch2/catch_test_macros.hpp>

#include "engine/Composer.h"
#include "engine/Instruments.h"
#include "engine/RiffGenerator.h"
#include "engine/SectionTypes.h"
#include "engine/Studio.h"
#include "ui/InstrumentsPage.h"

using namespace pr7s;

namespace
{
    using L = ComposerLayer;

    int pcOf (int interval) { return ((interval % 12) + 12) % 12; }

    // Groups of notes starting together that are a power chord written out (a root, its fifth, octaves).
    int writtenPowerChords (const LineIdea& idea)
    {
        std::vector<LineNote> played;
        for (const auto& n : idea.notes)
            if (! n.keyswitch)
                played.push_back (n);
        std::stable_sort (played.begin(), played.end(), [] (const LineNote& a, const LineNote& b)
                          { return a.beat < b.beat || (a.beat == b.beat && a.pitch < b.pitch); });
        auto count = 0;
        for (size_t i = 0; i < played.size();)
        {
            auto j = i + 1;
            while (j < played.size() && std::abs (played[j].beat - played[i].beat) < 0.03)
                ++j;
            auto fifth = false, power = j - i >= 2;
            for (auto k = i + 1; k < j; ++k)
            {
                const auto p = pcOf (played[k].pitch - played[i].pitch);
                fifth = fifth || p == 7;
                power = power && (p == 0 || p == 7);
            }
            count += power && fifth ? 1 : 0;
            i = j;
        }
        return count;
    }

    int keyPresses (const LineIdea& idea, int key)
    {
        return (int) std::count_if (idea.notes.begin(), idea.notes.end(), [key] (const LineNote& n) { return n.keyswitch && n.pitch == key; });
    }

    ComposerContext jupiterContext (float humanize)
    {
        ComposerContext c;
        c.humanize = humanize;
        auto jupiter = InstrumentProfile::shreddage ("Shreddage Jupiter", GuitarVelocities {});
        jupiter.forGuitar = jupiter.forLead = true;
        c.guitarProfile = jupiter;
        c.leadProfile = jupiter;
        return c;
    }
}

TEST_CASE ("0.127.0: every guitar power chord the composer writes is played by Shreddage's power chord key", "[composer][powerchords]")
{
    for (const auto humanize : { 0.0f, 0.6f })
    {
        const auto c = jupiterContext (humanize);
        auto checked = 0, powerKeys = 0;
        // Every riff kind under STANDARD, a few takes each.
        for (int f = 0; f < (int) riffFlavours().size(); ++f)
            for (int v = 0; v < 4; ++v)
            {
                ComposerState s;
                for (auto& lane : s.lanes)
                    lane.on = true;
                s.riffFlavour = f;
                s.lane (L::guitar).variation = v;
                const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
                REQUIRE_FALSE (spans.empty());
                const auto layers = composeLayers (s, spans, c);
                const auto& guitar = layers[(size_t) L::guitar];
                INFO (riffFlavourName (riffFlavours()[(size_t) f]) << " take " << v << " humanize " << humanize);
                CHECK (writtenPowerChords (guitar) == 0);
                powerKeys += keyPresses (guitar, 13);
                ++checked;
            }
        // Every section type of every kind of section, each guitar option.
        for (const auto* name : { "Intro", "Verse", "Pre-Chorus", "Chorus", "Post-Chorus", "Bridge", "Breakdown", "Outro", "Solo" })
            for (const auto& type : sectionTypesFor (name))
            {
                ComposerState s;
                for (auto& lane : s.lanes)
                    lane.on = true;
                applySectionType (s, name, type.name);
                const auto options = juce::jmax (1, sectionApproaches (s, L::guitar).size());
                for (int a = 0; a < options; ++a)
                {
                    auto each = s;
                    each.lane (L::guitar).approach = sectionApproaches (s, L::guitar).isEmpty() ? -1 : a;
                    const auto spans = composerSpans (each, 0.0, c.beatsPerBar);
                    const auto layers = composeLayers (each, spans, c);
                    const auto& guitar = layers[(size_t) L::guitar];
                    INFO (name << " " << type.name << " option " << a << " humanize " << humanize);
                    CHECK (writtenPowerChords (guitar) == 0);
                    powerKeys += keyPresses (guitar, 13);
                    ++checked;
                }
            }
        CHECK (checked > 100);
        CHECK (powerKeys > 50);   // (power chords are played, through the key)
    }
}

TEST_CASE ("0.127.0: Shreddage goes onto an empty guitar lane (and lead), once; one set by hand is kept", "[composer][powerchords][instruments]")
{
    const GuitarVelocities v;
    // Only the bass: Shreddage Jupiter added on the guitar and lead lanes.
    std::vector<InstrumentProfile> profiles { InstrumentProfile::djinnBass2() };
    CHECK (giveGuitarLaneShreddage (profiles, v));
    REQUIRE (profiles.size() == 2);
    CHECK (profiles[1].name == "Shreddage Jupiter");
    CHECK (profiles[1].forGuitar);
    CHECK (profiles[1].forLead);
    CHECK (profiles[1].keyUsedFor (Keyswitch::Use::powerChords) == 13);   // C#-1
    CHECK_FALSE (giveGuitarLaneShreddage (profiles, v));   // (a lane already has one)
    // A Shreddage made by hand but not ticked: it's ticked, nothing added; a lead lane already taken kept.
    auto rogue = InstrumentProfile::shreddage ("Shreddage Rogue", v);
    auto lead = InstrumentProfile::blank (InstrumentProfile::Kind::guitar);
    lead.forLead = true;
    std::vector<InstrumentProfile> mine { InstrumentProfile::djinnBass2(), lead, rogue };
    CHECK (giveGuitarLaneShreddage (mine, v));
    REQUIRE (mine.size() == 3);
    CHECK (mine[2].forGuitar);
    CHECK_FALSE (mine[2].forLead);
    CHECK (mine[1].forLead);
}

TEST_CASE ("0.127.0: + NEW Shreddage takes the free guitar and lead lanes", "[composer][powerchords][instruments][ui]")
{
    Studio studio (false);
    InstrumentsPage page (studio);
    page.addProfile (InstrumentsPage::NewKind::jupiter);
    REQUIRE (studio.profileFor (Studio::ProfileUse::guitar).has_value());
    CHECK (studio.profileFor (Studio::ProfileUse::guitar)->name == "Shreddage Jupiter");
    CHECK (studio.profileFor (Studio::ProfileUse::lead)->name == "Shreddage Jupiter");
    page.addProfile (InstrumentsPage::NewKind::rogue);   // (the lanes taken: the second one waits)
    CHECK (studio.profileFor (Studio::ProfileUse::guitar)->name == "Shreddage Jupiter");
}
