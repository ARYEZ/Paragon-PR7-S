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

TEST_CASE ("Shreddage Jupiter and Rogue: 7-string drop A, 24 frets, the velocity layers, Sustain C-1, Power Chords C#-1, force keys (0.62.2)", "[instruments]")
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
    CHECK (p.isKeyswitch (*parseNoteText ("C#-1")));
    CHECK (p.keyFor (1) == *parseNoteText ("C-2"));    // (the manual: C-2 the lowest A string)
    CHECK (p.keyFor (7) == *parseNoteText ("F#-2"));
    CHECK (p.releaseKey() == *parseNoteText ("G-2"));
    CHECK (p.bendRange == 2);
    CHECK (profilesFromText (profilesToText ({ p })).front() == p);   // (the release key kept)
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

TEST_CASE ("ONLY STRING on Shreddage: the string's key from the start, G-2 after the last note (0.62.2)", "[instruments][composer]")
{
    ComposerContext c;
    c.humanize = 0.0f;
    c.guitarProfile = InstrumentProfile::shreddage ("Shreddage Jupiter");
    ComposerState s;
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    const auto total = spans.back().end - spans.front().start;
    s.lane (ComposerLayer::guitar).onlyString = 2;
    const auto layers = composeLayers (s, spans, c);
    const auto& notes = layers[(size_t) ComposerLayer::guitar].notes;
    const LineNote* force = nullptr;
    const LineNote* release = nullptr;
    auto lastStart = 0.0;
    for (const auto& n : notes)
    {
        if (n.keyswitch && n.pitch == *parseNoteText ("C#-2")) force = &n;
        else if (n.keyswitch && n.pitch == *parseNoteText ("G-2")) release = &n;
        else if (! n.keyswitch)
        {
            lastStart = juce::jmax (lastStart, n.beat);
            CHECK (n.pitch >= 40);
            CHECK (n.pitch <= 64);
        }
    }
    REQUIRE (force != nullptr);
    REQUIRE (release != nullptr);
    CHECK (force->beat == 0.0);
    CHECK (release->beat > lastStart);
    CHECK (release->beat + release->length <= total + 1.0e-9);
    // Djinn (no release key): none added.
    ComposerContext d;
    d.humanize = 0.0f;
    d.bassProfile = InstrumentProfile::djinnBass2();
    s.lane (ComposerLayer::bass).onlyString = 1;
    const auto bass = composeLayers (s, spans, d);
    auto keys = 0;
    for (const auto& n : bass[(size_t) ComposerLayer::bass].notes)
        keys += n.keyswitch ? 1 : 0;
    CHECK (keys == 1);
}

TEST_CASE ("Power chords by keyswitch: a root alone after C#-1, single notes after C-1, only where the kind changes (0.63.0)", "[instruments]")
{
    const auto power = 13, single = 12;
    LineIdea idea;
    const auto note = [] (double beat, int pitch, float velocity, double length = 0.5) { return LineNote { beat, length, pitch, velocity }; };
    idea.notes = {
        note (0.0, 33, 40.0f / 127.0f), note (0.0, 40, 44.0f / 127.0f, 0.6), note (0.0, 45, 40.0f / 127.0f),   // a palm-muted power chord (root, fifth, octave)
        note (0.5, 33, 40.0f / 127.0f), note (0.5, 40, 40.0f / 127.0f),                                    // another (root, fifth)
        note (1.0, 33, 96.0f / 127.0f),                                                                     // a single note
        note (1.5, 36, 96.0f / 127.0f),                                                                     // another
        note (2.0, 45, 96.0f / 127.0f), note (2.0, 49, 96.0f / 127.0f),                                     // a third (not a power chord)
        note (3.0, 33, 100.0f / 127.0f), note (3.02, 40, 100.0f / 127.0f), note (3.04, 45, 100.0f / 127.0f) // strummed: stays
    };
    usePowerChordKeys (idea, power, single);
    std::vector<std::pair<double, int>> keys;
    std::vector<LineNote> played;
    for (const auto& n : idea.notes)
        (n.keyswitch ? (void) keys.push_back ({ n.beat, n.pitch }) : played.push_back (n));
    CHECK (keys == std::vector<std::pair<double, int>> { { 0.0, power }, { 1.0, single } });
    REQUIRE (played.size() == 2 + 2 + 2 + 3);
    CHECK (played[0].pitch == 33);
    CHECK (played[0].beat == 0.0);
    CHECK (played[0].length == 0.6);                                   // (the longest)
    CHECK (juce::roundToInt (played[0].velocity * 127.0f) == 44);      // (the loudest: still a palm mute)
    CHECK (played[1].pitch == 33);
    CHECK (played[1].beat == 0.5);
    CHECK (played[2].pitch == 33);
    CHECK (played[3].pitch == 36);
    // The keys before their notes, in time order.
    CHECK (idea.notes.front().keyswitch);
    // Nothing to do without both keys.
    LineIdea other;
    other.notes = { note (0.0, 33, 0.5f), note (0.0, 40, 0.5f) };
    usePowerChordKeys (other, power, -1);
    CHECK (other.notes.size() == 2);
}

TEST_CASE ("The composer's guitar on Shreddage: no power chord left as three notes, the keys where the kind changes (0.63.0)", "[instruments][composer]")
{
    ComposerContext c;
    c.humanize = 0.0f;
    c.guitarProfile = InstrumentProfile::shreddage ("Shreddage Jupiter");
    REQUIRE (c.guitarProfile->keyUsedFor (Keyswitch::Use::powerChords) == *parseNoteText ("C#-1"));
    REQUIRE (c.guitarProfile->keyUsedFor (Keyswitch::Use::singleNotes) == *parseNoteText ("C-1"));
    for (const auto style : guitarStyles())
    {
        c.style = style;
        ComposerState s;
        const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
        auto keysSeen = 0;
        for (int card = 0; card < 6; ++card)
        {
            INFO (guitarStyleName (style) << " card " << card);
            s.lane (ComposerLayer::guitar).style = card;
            const auto layers = composeLayers (s, spans, c);
            const auto& notes = layers[(size_t) ComposerLayer::guitar].notes;
            REQUIRE_FALSE (notes.empty());
            CHECK (notes.front().keyswitch);   // (the instrument set before the first note)
            int lastKey = -1;
            for (size_t i = 0; i < notes.size(); ++i)
            {
                const auto& n = notes[i];
                if (n.keyswitch)
                {
                    CHECK ((n.pitch == 12 || n.pitch == 13));
                    CHECK (n.pitch != lastKey);   // (only where the kind changes)
                    lastKey = n.pitch;
                    ++keysSeen;
                    continue;
                }
                // No root-and-fifth group left.
                for (size_t j = i + 1; j < notes.size() && std::abs (notes[j].beat - n.beat) < 1.0e-4; ++j)
                    if (! notes[j].keyswitch)
                        CHECK ((notes[j].pitch - n.pitch) % 12 != 7);
            }
        }
        CHECK (keysSeen > 0);
    }
    // Without a profile: as before (no keys).
    ComposerContext plain;
    plain.humanize = 0.0f;
    ComposerState s;
    const auto layers = composeLayers (s, composerSpans (s, 0.0, plain.beatsPerBar), plain);
    for (const auto& n : layers[(size_t) ComposerLayer::guitar].notes)
        CHECK_FALSE (n.keyswitch);
}

TEST_CASE ("Playable: nothing faster than a sixteenth, nothing shorter, two palm-muted sixteenths in a row at most (0.63.1)", "[instruments]")
{
    const auto note = [] (double beat, int pitch, int velocity, double length = 0.1) { return LineNote { beat, length, pitch, (float) velocity / 127.0f }; };
    LineIdea idea;
    idea.notes = { note (0.0, 33, 40), note (0.125, 33, 40),                                  // a 32nd: left out
                   note (0.25, 33, 40), note (0.5, 33, 40), note (0.75, 33, 40),              // the third 16th chug in a row: out
                   note (1.0, 33, 40), note (1.25, 33, 40),
                   note (2.0, 45, 96, 0.05), note (2.0, 52, 96, 0.05),                        // a chord: one hit, both lengthened
                   note (2.25, 45, 96), note (2.5, 45, 96), note (2.75, 45, 96) };             // open 16ths: kept
    keepPlayable (idea, 0.25, 2, 4.0);
    std::vector<double> starts;
    for (const auto& n : idea.notes)
    {
        starts.push_back (n.beat);
        CHECK (n.length >= 0.25 - 1.0e-9);
    }
    CHECK (starts == std::vector<double> { 0.0, 0.25, 0.75, 1.0, 2.0, 2.0, 2.25, 2.5, 2.75 });
    // Humanize's few milliseconds early still count as a sixteenth.
    LineIdea loose;
    loose.notes = { note (0.0, 33, 96, 0.25), note (0.235, 33, 96, 0.25) };
    keepPlayable (loose, 0.25, 2, 1.0);
    CHECK (loose.notes.size() == 2);
}

TEST_CASE ("The composer on Djinn and Shreddage: every note a sixteenth or longer, a sixteenth apart or more (0.63.1)", "[instruments][composer]")
{
    ComposerContext c;
    c.humanize = 0.0f;
    c.bassProfile = InstrumentProfile::djinnBass2();
    c.guitarProfile = InstrumentProfile::shreddage ("Shreddage Jupiter");
    c.leadProfile = InstrumentProfile::shreddage ("Shreddage Rogue");
    for (const auto style : guitarStyles())
    {
        c.style = style;
        ComposerState s;
        s.lane (ComposerLayer::lead).on = true;
        const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
        for (const auto layer : { ComposerLayer::bass, ComposerLayer::guitar, ComposerLayer::lead })
            for (int card = 0; card < 6; ++card)
                for (const auto busy : { 0.5f, 1.0f })
                {
                    INFO (guitarStyleName (style) << " " << composerLayerName (layer) << " card " << card << " busy " << busy);
                    auto t = s;
                    t.lane (layer).style = card;
                    t.lane (layer).busy = busy;
                    const auto layers = composeLayers (t, spans, c);
                    std::vector<LineNote> hits;
                    for (const auto& n : layers[(size_t) layer].notes)
                        if (! n.keyswitch)
                        {
                            CHECK (n.length >= 0.25 - 1.0e-6);
                            if (hits.empty() || n.beat - hits.back().beat > 1.0e-4)
                                hits.push_back (n);
                        }
                    auto mutedRun = 0;
                    for (size_t i = 1; i < hits.size(); ++i)
                    {
                        const auto gap = hits[i].beat - hits[i - 1].beat;
                        CHECK (gap >= 0.25 - 0.04);
                        const auto muted = hits[i].velocity * 127.0f < 59.5f && hits[i - 1].velocity * 127.0f < 59.5f;
                        mutedRun = muted && gap < 0.29 ? mutedRun + 1 : 0;
                        if (layer != ComposerLayer::bass)
                            CHECK (mutedRun < 2);   // (at most two in a row: one sixteenth step between them)
                    }
                }
    }
}
