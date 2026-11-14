#include <catch2/catch_test_macros.hpp>

#include <set>

#include "engine/Studio.h"
#include "engine/Composer.h"
#include "ui/ComposerTracksPage.h"

// (0.114.0, the owner: "a composed lane after lead guitar called clean guitar")

using namespace pr7s;

TEST_CASE ("CLEAN GUITAR: a composer lane after LEAD, off to start with, its own styles, kept with the others (0.114.0)", "[composer][clean]")
{
    using L = ComposerLayer;
    const auto& order = composerLayers();
    const auto at = [&order] (L l) { return std::find (order.begin(), order.end(), l) - order.begin(); };
    CHECK (at (L::clean) == at (L::lead) + 1);
    CHECK (composerLayerName (L::clean) == "CLEAN GUITAR");
    CHECK (composerPartOf (L::clean) == LineIdea::Part::clean);

    ComposerState s;
    CHECK_FALSE (s.lane (L::clean).on);
    // A 0.113.0 state (six lanes) still reads: the clean lane at its defaults, the others where they were.
    auto old = s;
    old.lane (L::drums).style = 2;
    auto text = old.toString();
    const auto parts = juce::StringArray::fromTokens (text, "|", "");
    auto lanes = juce::StringArray::fromTokens (parts[7], ";", "");
    lanes.remove (6);
    auto oldParts = parts;
    oldParts.set (7, lanes.joinIntoString (";"));
    const auto read = ComposerState::fromString (oldParts.joinIntoString ("|"));
    CHECK (read.lane (L::drums).style == 2);
    CHECK_FALSE (read.lane (L::clean).on);
    // Saved and read back with the clean lane on.
    s.lane (L::clean).on = true;
    s.lane (L::clean).style = 4;
    CHECK (ComposerState::fromString (s.toString()) == s);

    ComposerContext c;
    c.humanize = 0.0f;
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    REQUIRE_FALSE (spans.empty());
    const auto choices = composerChoices (L::clean, s, spans, c);
    REQUIRE ((int) choices.size() == composerStyleCount (L::clean));
    REQUIRE (composerStyleCount (L::clean) == cleanGuitarStyles().size());
    std::set<std::vector<int>> different;
    for (size_t i = 0; i < choices.size(); ++i)
    {
        INFO (cleanGuitarStyles()[(int) i]);
        CHECK_FALSE (choices[i].notes.empty());
        CHECK (choices[i].part == LineIdea::Part::clean);
        std::vector<int> fingerprint;
        for (const auto& n : choices[i].notes)
            fingerprint.push_back (n.pitch * 1000 + juce::roundToInt (n.beat * 48.0));
        different.insert (fingerprint);
    }
    CHECK (different.size() == choices.size());   // (every style its own part)
    const auto layers = composeLayers (s, spans, c);
    CHECK (layers[(size_t) L::clean].notes == choices[4].notes);
}

TEST_CASE ("CLEAN GUITAR: built onto a \"Clean Guitar\" track, its own row in COMPOSER TRACKS (0.114.0)", "[composer][clean]")
{
    Studio studio (false);
    CHECK (Studio::linePartName (LineIdea::Part::clean) == "Clean Guitar");
    CHECK (Studio::composerTrackLane (LineIdea::Part::clean) == 6);
    CHECK (Studio::composerTrackPart (6) == LineIdea::Part::clean);
    CHECK (ComposerTracksPage::laneName (6) == "CLEAN GUITAR");
    CHECK (studio.getComposerTracks (LineIdea::Part::clean).names == juce::StringArray ("Clean Guitar"));
    studio.setComposerTracks (LineIdea::Part::clean, { juce::StringArray ("Clean L", "Clean R"), true });
    CHECK (studio.getComposerTracks (LineIdea::Part::clean).names == juce::StringArray ("Clean L", "Clean R"));
    CHECK (studio.getComposerTracks (LineIdea::Part::lead).names == juce::StringArray ("Lead Guitar"));

    ComposerState s;
    s.lane (ComposerLayer::clean).on = true;
    ComposerContext c;
    c.humanize = 0.0f;
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    const auto idea = composeLayers (s, spans, c)[(size_t) ComposerLayer::clean];
    const auto track = studio.addLinePart (idea, spans, 0);
    REQUIRE (track != 0);
    CHECK (studio.getProject().findTrack (track)->name == "Clean L");
}
