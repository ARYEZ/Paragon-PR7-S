#include <catch2/catch_test_macros.hpp>

#include "engine/Studio.h"
#include "engine/Composer.h"
#include "engine/Instruments.h"
#include "ui/ComposerPage.h"
#include "ui/Hub.h"
#include "ui/InstrumentsPage.h"

// INSTRUMENTS (0.62.0): each instrument's range, tuning and keyswitches (Djinn Bass 2 built in, the owner's
// tuning A0 D1 A1 D2 G2), and the composer's ONLY STRING holding the library's keyswitch for it.

using namespace pr7s;

namespace
{
    using L = ComposerLayer;

    ComposerContext withDjinn()
    {
        ComposerContext c;
        c.humanize = 0.0f;
        c.bassProfile = InstrumentProfile::djinnBass2();
        return c;
    }
}

TEST_CASE ("Note names typed as the studio and the libraries name them: middle C = C3 (0.62.0)", "[instruments]")
{
    CHECK (parseNoteText ("A0") == 33);
    CHECK (parseNoteText ("C3") == 60);
    CHECK (parseNoteText ("C-2") == 0);
    CHECK (parseNoteText ("C#-1") == 13);
    CHECK (parseNoteText ("G8") == 127);
    CHECK (parseNoteText ("Eb3") == 63);
    CHECK (parseNoteText ("bb1") == 46);
    CHECK (parseNoteText ("33") == 33);
    CHECK_FALSE (parseNoteText ("H2").has_value());
    CHECK_FALSE (parseNoteText ("").has_value());
    CHECK_FALSE (parseNoteText ("A9").has_value());
    // The same names the MIDI editor shows, both ways.
    for (int n = 0; n < 128; ++n)
        CHECK (parseNoteText (noteName (n)) == n);
}

TEST_CASE ("Djinn Bass 2: the owner's tuning, its range clear of every keyswitch, a key per string (0.62.0)", "[instruments]")
{
    const auto p = InstrumentProfile::djinnBass2();
    CHECK (p.strings == std::vector<int> { 33, 38, 45, 50, 55 });   // A0 D1 A1 D2 G2
    CHECK (p.rangeLow() == 33);
    CHECK (p.rangeHigh() == 79);
    CHECK (p.keyFor (1) == 127);   // "Force B String": Djinn's lowest string, the owner's A0
    CHECK (p.keyFor (2) == 126);
    CHECK (p.keyFor (5) == 123);
    CHECK (p.keyFor (6) == -1);
    for (const auto& k : p.keyswitches)
    {
        INFO (k.name);
        CHECK ((k.note < p.rangeLow() || k.note > p.rangeHigh()));
    }
    CHECK (p.bendRange == 4);
    CHECK (p.forBass);

    // Kept as text and read back.
    auto guitar = InstrumentProfile::blank (InstrumentProfile::Kind::guitar);
    guitar.name = "Jupiter";
    guitar.forGuitar = true;
    guitar.lowest = 33;
    guitar.velocities->pinch = 125;
    const auto back = profilesFromText (profilesToText ({ p, guitar }));
    REQUIRE (back.size() == 2);
    CHECK (back[0] == p);
    CHECK (back[1] == guitar);
}

TEST_CASE ("The studio's instruments: Djinn Bass 2 for the bass the first time, one per lane, kept (0.62.0)", "[instruments][studio]")
{
    Studio studio (false);
    const auto first = studio.getInstrumentProfiles();
    REQUIRE (first.size() == 1);
    CHECK (first.front().name == "Djinn Bass 2");
    REQUIRE (studio.profileFor (Studio::ProfileUse::bass).has_value());
    CHECK_FALSE (studio.profileFor (Studio::ProfileUse::guitar).has_value());
    CHECK (studio.getComposerContext (0.0).bassProfile.has_value());

    auto guitar = InstrumentProfile::blank (InstrumentProfile::Kind::guitar);
    guitar.forGuitar = true;
    studio.setInstrumentProfiles ({ first.front(), guitar });
    CHECK (studio.getInstrumentProfiles().size() == 2);
    CHECK (studio.profileFor (Studio::ProfileUse::guitar)->name == "New Guitar");
    // None at all: none for the bass (the composer's own tunings then).
    studio.setInstrumentProfiles ({});
    CHECK (studio.getInstrumentProfiles().empty());
    CHECK_FALSE (studio.profileFor (Studio::ProfileUse::bass).has_value());
}

TEST_CASE ("The composer's bass on Djinn Bass 2: every note in its range, none on a keyswitch (0.62.0)", "[instruments][composer]")
{
    ComposerState s;
    const auto c = withDjinn();
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    const auto& djinn = *c.bassProfile;
    for (int style = 0; style < composerStyleCount (L::bass); ++style)
        for (const auto low : { false, true })
        {
            INFO ("style " << style << " low " << low);
            auto t = s;
            t.lane (L::bass).style = style;
            t.lane (L::bass).lowString = low;
            t.lane (L::bass).variation = 17 + style;
            const auto layers = composeLayers (t, spans, c);
            const auto& notes = layers[(size_t) L::bass].notes;
            REQUIRE_FALSE (notes.empty());
            for (const auto& n : notes)
            {
                CHECK_FALSE (n.keyswitch);
                CHECK (n.pitch >= djinn.rangeLow());
                CHECK (n.pitch <= djinn.rangeHigh());
                CHECK_FALSE (djinn.isKeyswitch (n.pitch));
                if (low)
                    CHECK (n.pitch <= djinn.rangeLow() + (style == 3 ? 24 : 14));
            }
        }
}

TEST_CASE ("ONLY STRING: the whole part on one string, its keyswitch held from the start (0.62.0)", "[instruments][composer]")
{
    ComposerState s;
    const auto c = withDjinn();
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    const auto total = spans.back().end - spans.front().start;
    for (int string = 1; string <= 5; ++string)
    {
        INFO ("string " << string);
        auto t = s;
        t.lane (L::bass).style = 2;   // (driving eighths)
        t.lane (L::bass).onlyString = string;
        const auto layers = composeLayers (t, spans, c);
        const auto& notes = layers[(size_t) L::bass].notes;
        const auto open = c.bassProfile->strings[(size_t) string - 1];
        auto keys = 0;
        std::vector<double> starts;
        for (const auto& n : notes)
        {
            if (n.keyswitch)
            {
                ++keys;
                CHECK (n.pitch == c.bassProfile->keyFor (string));
                CHECK (n.beat == 0.0);
                CHECK (n.length == total);
                continue;
            }
            CHECK (n.pitch >= open);
            CHECK (n.pitch <= open + 24);
            starts.push_back (n.beat);
        }
        CHECK (keys == 1);
        CHECK (starts.size() > 8);
        CHECK (std::adjacent_find (starts.begin(), starts.end()) == starts.end());   // (one note at a time)
    }
    // Without a profile: on the string, no keyswitch.
    auto plain = withDjinn();
    plain.bassProfile.reset();
    auto t = s;
    t.lane (L::bass).onlyString = 1;
    const auto layers = composeLayers (t, spans, plain);
    for (const auto& n : layers[(size_t) L::bass].notes)
    {
        CHECK_FALSE (n.keyswitch);
        CHECK (n.pitch >= plain.bassTuning.lowest());
        CHECK (n.pitch <= plain.bassTuning.lowest() + 24);
    }
    // Saved with the state.
    CHECK (ComposerState::fromString (t.toString()).lane (L::bass).onlyString == 1);
}

TEST_CASE ("The composer page's string menu: ONLY STRING written onto the track with its keyswitch (0.62.0)", "[instruments][composer][ui]")
{
    Studio studio (false);
    ComposerPage page (studio);
    page.setOnlyString (L::bass, 2);
    CHECK (page.getState().lane (L::bass).onlyString == 2);
    CHECK (page.keyNamed ("BASS LOW")->getButtonText() == "STRING 2 ONLY");
    CHECK (std::any_of (page.getIdea (L::bass).notes.begin(), page.getIdea (L::bass).notes.end(),
                        [] (const LineNote& n) { return n.keyswitch && n.pitch == 126; }));
    page.setLowString (L::bass, true);
    CHECK (page.getState().lane (L::bass).onlyString == 0);
    CHECK (page.keyNamed ("BASS LOW")->getButtonText() == "LOW STRING");
    page.setOnlyString (L::bass, 1);
    CHECK_FALSE (page.getState().lane (L::bass).lowString);
    page.setOnlyString (L::bass, 9);   // (Djinn has five strings)
    CHECK (page.getState().lane (L::bass).onlyString == 5);
    page.setOnlyString (L::bass, 1);
    const auto track = page.useLayer (L::bass);
    REQUIRE (track != 0);
    auto keyswitchWritten = false;
    for (const auto& part : studio.getProject().findTrack (track)->parts)
        for (const auto& n : part.notes)
            keyswitchWritten = keyswitchWritten || n.pitch == 127;
    CHECK (keyswitchWritten);
}

TEST_CASE ("The Hub's INSTRUMENTS page: new, strings, range, one lane each, keyswitches, all kept (0.62.0)", "[instruments][ui]")
{
    Studio studio (false);
    Hub hub (studio);
    hub.setSize (Hub::preferredWidth, Hub::preferredHeight);
    hub.showInstrumentsPage();
    REQUIRE (hub.isShowingInstruments());
    auto& page = *hub.getInstrumentsPage();
    CHECK (page.isVisible());
    REQUIRE (page.getProfiles().size() == 1);
    CHECK (page.getSelected() == 0);

    page.addProfile (InstrumentsPage::NewKind::guitar);
    REQUIRE (page.getProfiles().size() == 2);
    CHECK (page.getSelected() == 1);
    page.setName ("Shreddage Jupiter");
    CHECK (page.setString (0, "A0"));
    CHECK_FALSE (page.setString (0, "nonsense"));
    CHECK (page.getProfiles()[1].strings.front() == 33);
    CHECK (page.setRange ("A0", "E5"));
    CHECK (page.getProfiles()[1].rangeHigh() == parseNoteText ("E5"));
    CHECK_FALSE (page.setRange ("E5", "A0"));
    CHECK (page.setRange ("", ""));   // (from the strings again)
    CHECK (page.getProfiles()[1].lowest == -1);

    // One profile per lane.
    page.setUse (Studio::ProfileUse::bass, true);
    CHECK (page.getProfiles()[1].forBass);
    CHECK_FALSE (page.getProfiles()[0].forBass);
    page.setUse (Studio::ProfileUse::guitar, true);
    CHECK (studio.profileFor (Studio::ProfileUse::guitar)->name == "Shreddage Jupiter");

    // Keyswitches: one key per string.
    page.addKeyswitch();
    page.addKeyswitch();
    CHECK (page.setKeyswitch (0, "Force low string", "C-1", 1));
    CHECK (page.setKeyswitch (1, "Force low string again", "D-1", 1));
    CHECK (page.getProfiles()[1].keyFor (1) == parseNoteText ("D-1"));
    CHECK (page.getProfiles()[1].keyswitches[0].forcesString == 0);
    CHECK_FALSE (page.setKeyswitch (0, "x", "not a note", 0));
    page.removeKeyswitch (0);
    CHECK (page.getProfiles()[1].keyswitches.size() == 1);
    page.addString();
    CHECK (page.getProfiles()[1].strings.size() == 8);
    page.removeString();
    CHECK (page.getProfiles()[1].strings.size() == 7);

    // All of it in the studio.
    CHECK (studio.getInstrumentProfiles() == page.getProfiles());
    page.selectProfile (0);
    page.deleteProfile();
    CHECK (studio.getInstrumentProfiles().size() == 1);
    CHECK (studio.getInstrumentProfiles().front().name == "Shreddage Jupiter");
    hub.showSettingsPage (false);
    CHECK_FALSE (page.isVisible());
}

TEST_CASE ("Shreddage Jupiter and Rogue: 7-string drop A, 24 frets, the velocity layers, Sustain C-1, Power Chord Sustain C-2 (0.62.1)", "[instruments]")
{
    const auto p = InstrumentProfile::shreddage ("Shreddage Jupiter");
    CHECK (p.kind == InstrumentProfile::Kind::guitar);
    CHECK (p.strings == std::vector<int> { *parseNoteText ("A0"), *parseNoteText ("E1"), *parseNoteText ("A1"), *parseNoteText ("D2"),
                                           *parseNoteText ("G2"), *parseNoteText ("B2"), *parseNoteText ("E3") });
    CHECK (p.frets == 24);
    CHECK (p.rangeLow() == 33);
    CHECK (p.rangeHigh() == 64 + 24);
    REQUIRE (p.velocities.has_value());
    CHECK (*p.velocities == GuitarVelocities {});
    CHECK (p.isKeyswitch (*parseNoteText ("C-1")));
    CHECK (p.isKeyswitch (*parseNoteText ("C-2")));
    for (const auto& k : p.keyswitches)
        CHECK (k.note < p.rangeLow());
    CHECK (p.tuning().strings == GuitarTuning::all().front().strings);   // (the same as IDEAS' 7-string drop A)

    // From the INSTRUMENTS page's + NEW, with the studio's own VELOCITIES.
    Studio studio (false);
    auto v = GuitarVelocities {};
    v.pinch = 127;
    v.muteTight = 30;
    studio.setGuitarVelocities (v);
    InstrumentsPage page (studio);
    page.addProfile (InstrumentsPage::NewKind::jupiter);
    page.addProfile (InstrumentsPage::NewKind::rogue);
    const auto& profiles = page.getProfiles();
    REQUIRE (profiles.size() == 3);
    CHECK (profiles[1].name == "Shreddage Jupiter");
    CHECK (profiles[2].name == "Shreddage Rogue");
    CHECK (profiles[1].velocities == v);
    // The guitar lane on Jupiter: every note in its range, none on its keyswitches.
    page.selectProfile (1);
    page.setUse (Studio::ProfileUse::guitar, true);
    const auto c = studio.getComposerContext (0.0);
    REQUIRE (c.guitarProfile.has_value());
    ComposerState s;
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    for (int style = 0; style < 6; ++style)
    {
        s.lane (ComposerLayer::guitar).style = style;
        const auto layers = composeLayers (s, spans, c);
        for (const auto& n : layers[(size_t) ComposerLayer::guitar].notes)
        {
            CHECK (n.pitch >= 33);
            CHECK (n.pitch <= 88);
        }
    }
}
