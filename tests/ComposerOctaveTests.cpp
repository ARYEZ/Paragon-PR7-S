// The composer's lanes (0.120.0, the owner's two asks): their keys always in two readable rows, and OCTAVE, a lane's
// notes moved up or down by whole octaves.

#include <catch2/catch_test_macros.hpp>
#include <set>

#include "TestHelpers.h"
#include "engine/Composer.h"
#include "engine/Studio.h"
#include "ui/ComposerPage.h"

using namespace pr7s;
using L = ComposerLayer;

TEST_CASE ("0.120.0: OCTAVE moves a lane's notes by whole octaves, its keyswitches stay, never the drums", "[composer][octave]")
{
    LineIdea idea;
    idea.notes = { { 0.0, 1.0, 40, 0.8f }, { 1.0, 1.0, 47, 0.8f } };
    LineNote key { 0.0, 4.0, 13, 0.5f };
    key.keyswitch = true;
    idea.notes.push_back (key);
    auto up = idea;
    moveByOctaves (up, L::guitar, 1);
    CHECK (up.notes[0].pitch == 52);
    CHECK (up.notes[1].pitch == 59);
    CHECK (up.notes[2].pitch == 13);   // the keyswitch where it was
    auto down = idea;
    moveByOctaves (down, L::bass, -2);
    CHECK (down.notes[0].pitch == 16);
    auto drums = idea;
    moveByOctaves (drums, L::drums, 1);
    CHECK (drums.notes[0].pitch == 40);

    // Kept with the composer's state (and so with a built section's recipe).
    ComposerState s;
    s.lane (L::bass).octave = -1;
    s.lane (L::lead).octave = 2;
    const auto back = ComposerState::fromString (s.toString());
    CHECK (back.lane (L::bass).octave == -1);
    CHECK (back.lane (L::lead).octave == 2);
    CHECK (back.lane (L::guitar).octave == 0);
    // Older text (nine fields a lane): as written.
    auto old = s.toString().replace (",-1;", ";").replace (",2;", ";").replace (",0;", ";");
    CHECK (ComposerState::fromString (old).lane (L::guitar).octave == 0);
}

TEST_CASE ("0.120.0: the composer page's OCTAVE key and its lanes' keys in two rows", "[composer][octave][ui]")
{
    Studio studio (false);
    ComposerPage page (studio);
    page.setRandomSeed (5);
    const auto before = page.getIdea (L::bass).notes;
    REQUIRE_FALSE (before.empty());
    page.setOctave (L::bass, -1);
    CHECK (page.getOctave (L::bass) == -1);
    CHECK (page.getOctaveKey (L::bass).getButtonText() == "OCTAVE -1");
    const auto& after = page.getIdea (L::bass).notes;
    REQUIRE (after.size() == before.size());
    for (size_t i = 0; i < after.size(); ++i)
        if (! before[i].keyswitch)
            CHECK (after[i].pitch == before[i].pitch - 12);
    page.setOctave (L::bass, 7);
    CHECK (page.getOctave (L::bass) == 2);   // (two at most)
    page.setOctave (L::drums, 1);
    CHECK (page.getOctave (L::drums) == 0);
    CHECK_FALSE (page.getOctaveKey (L::drums).isVisible());

    // Every lane's keys inside its header, in two rows, none on another, none squashed.
    for (const auto l : composerLayers())
    {
        const auto header = page.getLaneHeader (l);
        std::vector<juce::Component*> keys;
        for (auto* child : page.getChildren())
            if (child->isVisible() && dynamic_cast<juce::TextButton*> (child) != nullptr && header.contains (child->getBounds()))
                keys.push_back (child);
        REQUIRE (keys.size() >= 6);
        std::set<int> rows;
        for (auto* k : keys)
        {
            rows.insert (k->getY());
            CHECK (k->getWidth() >= 44);
            CHECK (k->getHeight() >= 16);
            for (auto* other : keys)
                if (other != k)
                    CHECK_FALSE (k->getBounds().intersects (other->getBounds()));
        }
        CHECK (rows.size() == 2);
    }
}
