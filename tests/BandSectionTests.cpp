// 0.82.3 (the owner asked): the band record's songs broken into their parts, named with the band's words.
#include <catch2/catch_test_macros.hpp>

#include "engine/BandRecord.h"
#include "engine/Markers.h"
#include "DrumSongs.h"

using namespace pr7s;

namespace
{
    juce::String namesOf (const std::vector<BandSection>& sections)
    {
        juce::StringArray names;
        for (const auto& s : sections)
            names.add (s.name + ":" + juce::String (s.firstBar) + "+" + juce::String (s.bars));
        return names.joinIntoString (", ");
    }

    // A one-bar riff `pitch` (eight eighths) over bars [from, to).
    void riffOver (std::vector<LineNote>& notes, int from, int to, int pitch, int every = 2)
    {
        for (auto b = from; b < to; ++b)
            for (auto s = 0; s < 16; s += every)
                notes.push_back ({ b * 4.0 + s * 0.25, 0.2, s % 8 == 0 ? pitch : pitch + 7, 0.8f });
    }
}

TEST_CASE ("0.82.3: song section names, numbered where they come back", "[band][sections]")
{
    CHECK (songSectionNames().contains ("Pre-Chorus"));
    CHECK (songSectionNames().contains ("Post-Chorus"));
    CHECK (songSectionNames().contains ("Solo"));
    CHECK (songSectionNames().contains ("Outro"));
    CHECK (sectionBaseOf ("Verse 3") == "Verse");
    CHECK (sectionBaseOf ("pre-chorus") == "Pre-Chorus");
    CHECK (sectionBaseOf ("Big riff thing").isEmpty());

    std::vector<Marker> markers;
    const char* names[] = { "Verse", "Chorus", "Verse 7", "Chorus", "Solo", "My riff", "Verse" };
    for (int i = 0; i < 7; ++i)
        markers.push_back ({ i + 1, true, i * 16.0, i * 16.0 + 16.0, names[i] });
    std::swap (markers[0], markers[6]);   // out of time order: numbered by time all the same
    numberSections (markers);
    std::sort (markers.begin(), markers.end(), [] (const Marker& a, const Marker& b) { return a.start < b.start; });
    CHECK (markers[0].name == "Verse 1");
    CHECK (markers[1].name == "Chorus 1");
    CHECK (markers[2].name == "Verse 2");
    CHECK (markers[3].name == "Chorus 2");
    CHECK (markers[4].name == "Solo");
    CHECK (markers[5].name == "My riff");
    CHECK (markers[6].name == "Verse 3");
}

TEST_CASE ("0.82.3: a riff change is heard at its bar line, a two-bar riff's halves aren't", "[band][sections]")
{
    std::vector<LineNote> riff;
    riffOver (riff, 0, 8, 33);
    for (auto b = 8; b < 16; ++b)   // a two-bar riff: D then F
        riffOver (riff, b, b + 1, b % 2 == 0 ? 38 : 41, 1);
    const auto changes = riffChanges (riff, SignatureMap(), 16);
    REQUIRE (changes.size() == 16);
    CHECK (changes[8] > 1.2f);
    CHECK (changes[4] < 0.2f);
    CHECK (changes[11] < 0.6f);
    CHECK (changes[12] < 0.6f);
}

TEST_CASE ("0.82.3: the song's sections from the drums, named and numbered", "[band][sections]")
{
    const auto drums = drumsongs::songOf ("I4 V8 C8 V8 C8 B8 C8 E4");
    const auto sections = findBandSections (drums, SignatureMap(), {}, {});
    INFO (namesOf (sections));
    REQUIRE (sections.size() >= 6);
    CHECK (sections.front().name == "Intro");
    CHECK (sections.front().firstBar == 0);
    CHECK (sections.back().name == "Outro");
    CHECK (sections[1].name == "Verse 1");
    CHECK (sections[2].name == "Chorus 1");
    CHECK (sections[3].name == "Verse 2");
    CHECK (sections[4].name == "Chorus 2");
    CHECK (sections[5].name == "Breakdown");
    CHECK (sections[6].name == "Chorus 3");
    auto bar = 0;
    for (const auto& s : sections)   // every bar covered, in order
    {
        CHECK (s.firstBar == bar);
        bar += s.bars;
    }
}

TEST_CASE ("0.82.3: a pre-chorus between verse and chorus, a solo where the lead gets busy", "[band][sections]")
{
    const auto drums = drumsongs::songOf ("I4 V8 R4 C8 V8 R4 C8 V8 C8 E4");
    // The lead: a few long notes over the song, then sixteenths over the third verse (bars 52-60).
    std::vector<LineNote> lead;
    for (auto b = 4; b < 52; b += 2)
        lead.push_back ({ b * 4.0, 2.0, 64, 0.8f });
    for (auto b = 52; b < 60; ++b)
        for (auto s = 0; s < 16; ++s)
            lead.push_back ({ b * 4.0 + s * 0.25, 0.2, 64 + s % 5, 0.8f });
    const auto sections = findBandSections (drums, SignatureMap(), {}, lead);
    INFO (namesOf (sections));
    juce::StringArray names;
    for (const auto& s : sections)
        names.add (s.name);
    CHECK (names.contains ("Pre-Chorus 1"));
    CHECK (names.contains ("Pre-Chorus 2"));
    CHECK (names.contains ("Solo"));
    CHECK (names.contains ("Verse 1"));
    CHECK (names.contains ("Verse 2"));
}
