#include <catch2/catch_test_macros.hpp>

#include "engine/Studio.h"
#include "engine/Composer.h"
#include "engine/RiffPaste.h"
#include "engine/DrumIdeas.h"
#include "ui/ComposerPage.h"

// (0.114.0, the owner: "when I select a riff it needs to be imported without being changed at all and I need to
// select what composer lane it goes to. The other composer lanes need to then have their initial riffs based off
// of the riff I imported")

using namespace pr7s;
using L = ComposerLayer;

namespace
{
    std::vector<Note> bassRiff()
    {
        // A bar of uneven eighths and sixteenths with odd velocities and lengths: anything changed shows.
        std::vector<Note> notes;
        const double at[] { 0.0, 0.5, 0.75, 1.5, 2.0, 2.25, 3.0, 3.5 };
        const int pitch[] { 33, 33, 36, 33, 38, 40, 33, 43 };
        for (int i = 0; i < 8; ++i)
            notes.push_back ({ at[i], 0.17 + i * 0.03, pitch[i], (30.0f + i * 11.0f) / 127.0f });
        return notes;
    }

    std::vector<double> starts (const LineIdea& idea)
    {
        std::vector<double> out;
        for (const auto& n : idea.notes)
            if (! n.keyswitch && (out.empty() || std::abs (n.beat - out.back()) > 0.02))
                out.push_back (n.beat);
        return out;
    }
}

TEST_CASE ("MY RIFF on a lane you choose: played as you played it, the others following it (0.114.0)", "[composer][riff][myrifflane]")
{
    Studio studio (false);
    ComposerPage page (studio);
    page.setRandomSeed (3);
    const auto riff = riffFromNotes (bassRiff(), 0.0, 4.0, "My bass riff", DrumSection::verse);
    REQUIRE (page.pasteRiff (riff, (int) L::bass));
    const auto& s = page.getState();
    CHECK (page.getRiffLane() == L::bass);
    CHECK (s.syncLane == (int) L::bass);
    CHECK (s.lane (L::bass).locked);
    CHECK (s.lane (L::bass).on);
    // Unchanged: exactly the riff, repeated to the length (pitches, velocities, lengths, places), humanize or not.
    const auto total = page.getSpans().back().end - page.getSpans().front().start;
    CHECK (page.getIdea (L::bass).notes == tileRiff (*studio.getComposerRiff(), total, 4.0).notes);
    // The others follow its rhythm: the drums' kicks and the guitar's hits fall on the riff's notes.
    const auto hits = starts (page.getIdea (L::bass));
    const auto onRiff = [&hits] (double t) { return std::any_of (hits.begin(), hits.end(), [t] (double h) { return std::abs (h - t) < 0.02; }); };
    const auto kicks = kickHits (page.getIdea (L::drums));
    REQUIRE_FALSE (kicks.empty());
    for (const auto k : kicks)
        CHECK (onRiff (k));
    const auto guitar = starts (page.getIdea (L::guitar));
    REQUIRE_FALSE (guitar.empty());
    for (const auto g : guitar)
        CHECK (onRiff (g));
    // Saved with the state; moved to another lane, the bass writes its own part again.
    CHECK (ComposerState::fromString (s.toString()).riffLane == (int) L::bass);
    page.setRiffLane (L::lead);
    CHECK (page.getRiffLane() == L::lead);
    CHECK (page.getState().syncLane == (int) L::lead);
    CHECK_FALSE (page.getState().lane (L::bass).locked);
    CHECK (page.getIdea (L::lead).notes == tileRiff (*studio.getComposerRiff(), total, 4.0).notes);
    CHECK (page.getIdea (L::bass).notes != page.getIdea (L::lead).notes);
}

TEST_CASE ("MY RIFF built into a section: the riff's lane's track gets the riff's notes untouched (0.114.0)", "[composer][riff][myrifflane]")
{
    Studio studio (false);
    ComposerPage page (studio);
    page.setRandomSeed (9);
    const auto riff = riffFromNotes (bassRiff(), 0.0, 4.0, "My bass riff", DrumSection::chorus);
    REQUIRE (page.pasteRiff (riff, (int) L::bass));
    const auto tracks = page.buildSection();
    REQUIRE_FALSE (tracks.empty());
    const Track* bass = nullptr;
    for (const auto& t : studio.getProject().tracks)
        if (t.name == "Bass")
            bass = &t;
    REQUIRE (bass != nullptr);
    REQUIRE_FALSE (bass->parts.empty());
    const auto& notes = bass->parts.back().notes;
    const auto& written = studio.getComposerRiff()->notes;
    REQUIRE (notes.size() >= written.size());
    for (size_t i = 0; i < written.size(); ++i)
    {
        CHECK (notes[i].pitch == written[i].pitch);
        CHECK (std::abs (notes[i].start - written[i].beat) < 1.0e-6);
        CHECK (std::abs (notes[i].length - written[i].length) < 1.0e-6);
        CHECK (std::abs (notes[i].velocity - written[i].velocity) < 1.0e-6);
    }
}
