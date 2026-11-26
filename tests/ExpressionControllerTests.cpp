#include <catch2/catch_test_macros.hpp>
#include "engine/Studio.h"
#include "engine/Instruments.h"
#include "ui/InstrumentsPage.h"

// 0.180.0 (the owner's report: a clean guitar riff after one with swells or vibrato played on with the instrument's
// sound turned most of the way down). ARCHITECT's controllers: the swell and vibrato numbers from the instrument
// profile (0: none), and every part ending with what its notes moved back at rest.

using namespace pr7s;

namespace
{
    using E = LineNote::Effect;
    using T = Chord::Type;
    using X = Chord::Tension;

    std::vector<ChordSpan> amBars (int count)
    {
        const Key am { 5, 0, true };
        std::vector<ChordSpan> spans;
        for (int i = 0; i < count; ++i)
            spans.push_back ({ i * 4.0, i * 4.0 + 4.0, { 9, T::minor, X::none, -1 }, am });
        return spans;
    }

    std::vector<LineNote> swellBendVibrato()
    {
        std::vector<LineNote> notes;
        notes.push_back ({ 0.0, 2.0, 64, 0.8f });
        notes.back().effect = E::swell;
        notes.push_back ({ 2.0, 0.5, 66, 0.8f });
        notes.back().effect = E::bend;
        notes.push_back ({ 3.0, 1.0, 64, 0.8f });
        notes.back().effect = E::vibrato;
        return notes;
    }

    int countCc (const std::vector<Controller>& c, int number)
    {
        return (int) std::count_if (c.begin(), c.end(), [number] (const Controller& k) { return k.type == Controller::Type::cc && k.number == number; });
    }
}

TEST_CASE ("0.180.0: swells and vibrato on the profile's controllers, or none", "[instruments][expression]")
{
    const auto notes = swellBendVibrato();
    const auto usual = Studio::effectControllers (notes, 2, 120.0);
    CHECK (countCc (usual, 11) == 4);   // the swell (from 0, up) and the two other notes at 110
    CHECK (countCc (usual, 1) == 4);    // the vibrato

    const auto moved = Studio::effectControllers (notes, 2, 120.0, 7, 74);
    CHECK (countCc (moved, 11) == 0);
    CHECK (countCc (moved, 1) == 0);
    CHECK (countCc (moved, 7) == 4);
    CHECK (countCc (moved, 74) == 4);

    const auto none = Studio::effectControllers (notes, 2, 120.0, 0, 0);
    for (const auto& c : none)
        CHECK (c.type == Controller::Type::pitchbend);   // only the bend is left
    CHECK (! none.empty());
}

TEST_CASE ("0.180.0: a part ends with its controllers back at rest", "[instruments][expression]")
{
    auto c = Studio::effectControllers (swellBendVibrato(), 2, 120.0);
    Studio::restControllers (c, 8.0);
    REQUIRE (c.size() >= 3);
    CHECK (std::is_sorted (c.begin(), c.end(), [] (const Controller& a, const Controller& b) { return a.start < b.start; }));
    // The last value of each kind, as it leaves the part.
    std::optional<int> wheel, expression, modWheel;
    for (const auto& k : c)
    {
        CHECK (k.start < 8.0);
        if (k.type == Controller::Type::pitchbend) wheel = k.value;
        if (k.type == Controller::Type::cc && k.number == 11) expression = k.value;
        if (k.type == Controller::Type::cc && k.number == 1) modWheel = k.value;
    }
    CHECK (wheel == 8192);
    CHECK (expression == 127);
    CHECK (modWheel == 0);
    CHECK (c.back().start > 8.0 - 0.01);

    // Nothing moved: nothing added.
    std::vector<Controller> empty;
    Studio::restControllers (empty, 8.0);
    CHECK (empty.empty());

    // Only what the part moved is put back: a bend alone gets the wheel centred, no controllers.
    std::vector<Controller> bendOnly { { Controller::Type::pitchbend, 0, 1.0, 12000, false } };
    Studio::restControllers (bendOnly, 4.0);
    REQUIRE (bendOnly.size() == 2);
    CHECK (bendOnly[1].type == Controller::Type::pitchbend);
    CHECK (bendOnly[1].value == 8192);

    // The profile's numbers.
    auto own = Studio::effectControllers (swellBendVibrato(), 2, 120.0, 7, 74);
    Studio::restControllers (own, 8.0, 7, 74);
    CHECK (own.back().start < 8.0);
    CHECK (std::any_of (own.begin(), own.end(), [] (const Controller& k) { return k.number == 7 && k.value == 127 && k.start > 7.9; }));
    CHECK (std::any_of (own.begin(), own.end(), [] (const Controller& k) { return k.number == 74 && k.value == 0 && k.start > 7.9; }));
}

TEST_CASE ("0.180.0: an ARCHITECT part on a track uses the profile's controllers and ends at rest", "[instruments][expression][studio]")
{
    Studio studio (false);
    auto guitar = InstrumentProfile::shreddage ("Shreddage Jupiter");
    guitar.forGuitar = true;
    guitar.swellCc = 2;
    guitar.vibratoCc = 0;
    studio.setInstrumentProfiles ({ guitar });

    LineIdea idea { LineIdea::Part::clean, "Swells", "", swellBendVibrato() };
    const auto trackId = studio.addLinePart (idea, amBars (2));
    REQUIRE (trackId != 0);
    const auto& part = studio.getProject().findTrack (trackId)->parts[0];
    CHECK (countCc (part.controllers, 11) == 0);
    CHECK (countCc (part.controllers, 1) == 0);
    REQUIRE (countCc (part.controllers, 2) > 0);
    const auto last = std::find_if (part.controllers.rbegin(), part.controllers.rend(),
                                    [] (const Controller& k) { return k.type == Controller::Type::cc && k.number == 2; });
    CHECK (last->value == 127);
    CHECK (last->start > part.length - 0.01);
    CHECK (last->start < part.length);
}

TEST_CASE ("0.180.0: the profile keeps its controllers; profiles saved before it get CC 11 and CC 1", "[instruments][expression]")
{
    auto p = InstrumentProfile::djinnBass2();
    p.swellCc = 7;
    p.vibratoCc = 0;
    const auto back = InstrumentProfile::fromXml (*p.toXml());
    REQUIRE (back.has_value());
    CHECK (back->swellCc == 7);
    CHECK (back->vibratoCc == 0);

    auto old = p.toXml();
    old->removeAttribute ("swellCc");
    old->removeAttribute ("vibratoCc");
    const auto older = InstrumentProfile::fromXml (*old);
    REQUIRE (older.has_value());
    CHECK (older->swellCc == 11);
    CHECK (older->vibratoCc == 1);
}

TEST_CASE ("0.180.0: the INSTRUMENTS page's SWELL CC and VIBRATO CC", "[instruments][expression][ui]")
{
    Studio studio (false);
    InstrumentsPage page (studio);
    page.selectProfile (0);
    CHECK (page.setSwellCc ("74"));
    CHECK (page.setVibratoCc ("OFF"));
    CHECK (page.getProfiles()[0].swellCc == 74);
    CHECK (page.getProfiles()[0].vibratoCc == 0);
    CHECK (studio.getInstrumentProfiles()[0].swellCc == 74);
    CHECK (page.setVibratoCc ("cc 21"));
    CHECK (page.getProfiles()[0].vibratoCc == 21);
    CHECK_FALSE (page.setSwellCc ("128"));
    CHECK_FALSE (page.setSwellCc ("loud"));
    CHECK (page.getProfiles()[0].swellCc == 74);   // unchanged by what it refused
}
