// SYNC (0.86.0, the owner): one composer lane leads, every other lane is written to its rhythm; the bass and the
// drums can double it (every hit, the bass on the riff's notes).

#include <catch2/catch_test_macros.hpp>

#include "engine/Composer.h"
#include "engine/DrumIdeas.h"
#include "engine/Studio.h"
#include "ui/ComposerPage.h"
#include "ui/Finish.h"

using namespace pr7s;

namespace
{
    using L = ComposerLayer;

    ComposerContext plainContext()
    {
        ComposerContext c;
        c.humanize = 0.0f;
        c.drums.fillEvery = 0;   // (no fills: a fill takes the kicks of its beat)
        return c;
    }

    ComposerState tritoneState()
    {
        ComposerState s;
        for (auto& lane : s.lanes)
            lane.on = true;
        for (int i = 0; i < (int) riffFlavours().size(); ++i)
            if (riffFlavours()[(size_t) i] == RiffFlavour::tritone)
                s.riffFlavour = i;
        return s;
    }

    bool near (const std::vector<double>& beats, double beat, double within = 0.02)
    {
        return std::any_of (beats.begin(), beats.end(), [&] (double b) { return std::abs (b - beat) <= within; });
    }

    std::vector<double> startsOf (const LineIdea& idea)
    {
        std::vector<double> out;
        for (const auto& n : idea.notes)
            if (! n.keyswitch)
                out.push_back (n.beat);
        return out;
    }
}

TEST_CASE ("0.86.0: SYNC and DOUBLE THE SYNC TRACK are kept with the composer's state", "[sync][composer]")
{
    ComposerState s;
    CHECK_FALSE (s.syncLayer().has_value());
    s.syncLane = (int) L::guitar;
    s.lane (L::bass).doubleSync = true;
    const auto back = ComposerState::fromString (s.toString());
    REQUIRE (back.syncLayer().has_value());
    CHECK (*back.syncLayer() == L::guitar);
    CHECK (back.lane (L::bass).doubleSync);
    CHECK_FALSE (back.lane (L::drums).doubleSync);
    CHECK (back == s);
    // Older text (before 0.86.0): SYNC off.
    auto old = s.toString();
    old = old.upToLastOccurrenceOf (",", false, false);
    CHECK_FALSE (ComposerState::fromString (old).syncLayer().has_value());
}

TEST_CASE ("0.86.0: a tritone chug riff leading: the drums double it (a kick on every hit), the bass doubles its notes", "[sync][composer]")
{
    auto s = tritoneState();
    const auto c = plainContext();
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    REQUIRE_FALSE (spans.empty());
    const auto before = composeLayers (s, spans, c);
    const auto riff = before[(size_t) L::guitar];
    REQUIRE_FALSE (riff.notes.empty());

    s.syncLane = (int) L::guitar;
    s.lane (L::drums).doubleSync = true;
    s.lane (L::bass).doubleSync = true;
    const auto layers = composeLayers (s, spans, c);
    // The leader as it was.
    CHECK (layers[(size_t) L::guitar].notes == riff.notes);
    const auto rhythm = syncRhythmOf (L::guitar, riff, c.beatsPerBar, spans.front().start);
    REQUIRE_FALSE (rhythm.all.empty());
    REQUIRE (rhythm.accents.size() <= rhythm.all.size());

    // Drums: a kick on every hit of the riff (on the sixteenth grid the drums keep).
    const auto kicks = kickHits (layers[(size_t) L::drums]);
    for (const auto h : rhythm.all)
    {
        INFO ("riff hit at " << h);
        CHECK (near (kicks, h));
    }
    CHECK (layers[(size_t) L::drums].style.startsWith ("Doubling"));

    // Bass: a note on every hit, nothing else; its pitch the riff's lowest there (in the bass's octave), or the
    // chord's root under a high stab.
    const auto& bass = layers[(size_t) L::bass].notes;
    REQUIRE (bass.size() == rhythm.all.size());
    const auto lowestOfRiff = [&riff] (double beat)
    {
        auto low = 128;
        for (const auto& n : riff.notes)
            if (! n.keyswitch && std::abs (n.beat - beat) < 0.11)
                low = juce::jmin (low, n.pitch);
        return low;
    };
    for (size_t i = 0; i < bass.size(); ++i)
    {
        INFO ("bass note " << i << " at " << bass[i].beat);
        CHECK (std::abs (bass[i].beat - rhythm.all[i]) < 1.0e-9);
        CHECK (bass[i].pitch <= bassTop (c.bassTuning));
        CHECK (bass[i].pitch >= c.bassTuning.lowest());
        const auto riffPc = lowestOfRiff (bass[i].beat) % 12;
        const ChordSpan* span = nullptr;
        for (const auto& sp : spans)
            if (bass[i].beat >= sp.start - 1.0e-9 && bass[i].beat < sp.end - 1.0e-9)
                span = &sp;
        REQUIRE (span != nullptr);
        const auto rootPc = span->chord.bass >= 0 ? span->chord.bass : span->chord.root;
        CHECK ((bass[i].pitch % 12 == riffPc || bass[i].pitch % 12 == rootPc));
    }
}

TEST_CASE ("0.86.0: without DOUBLE the drums and bass take the accents; chords stab them; melody and lead lean on them", "[sync][composer]")
{
    auto s = tritoneState();
    const auto c = plainContext();
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    const auto plain = composeLayers (s, spans, c);
    s.syncLane = (int) L::guitar;
    const auto layers = composeLayers (s, spans, c);
    const auto rhythm = syncRhythmOf (L::guitar, layers[(size_t) L::guitar], c.beatsPerBar, spans.front().start);

    const auto kicks = kickHits (layers[(size_t) L::drums]);
    for (const auto a : rhythm.accents)
        CHECK (near (kicks, a));
    for (const auto b : startsOf (layers[(size_t) L::bass]))
        CHECK (near (rhythm.accents, b, 1.0e-6));
    for (const auto b : startsOf (layers[(size_t) L::chords]))
        CHECK (near (rhythm.accents, b, 1.0e-6));
    // The melody: its own line, each note either where it was or moved onto an accent (a sixteenth at most).
    const auto& melody = layers[(size_t) L::melody].notes;
    const auto& was = plain[(size_t) L::melody].notes;
    REQUIRE (melody.size() == was.size());
    auto moved = 0;
    for (const auto& n : melody)
    {
        const auto onAccent = near (rhythm.accents, n.beat, 1.0e-6);
        CHECK ((onAccent || near (startsOf (plain[(size_t) L::melody]), n.beat, 1.0e-6)));
        moved += onAccent ? 1 : 0;
    }
    CHECK (moved > 0);
    // Turned off: as before.
    s.syncLane = -1;
    const auto off = composeLayers (s, spans, c);
    for (const auto l : composerLayers())
        CHECK (off[(size_t) l].notes == plain[(size_t) l].notes);
}

TEST_CASE ("0.86.0: any lane leads: the drums leading, the guitar chugs on the kicks and the bass doubles the kicks", "[sync][composer]")
{
    auto s = tritoneState();
    s.lane (L::drums).style = 4;   // (the breakdown: chug clusters on the kick)
    s.syncLane = (int) L::drums;
    s.lane (L::bass).doubleSync = true;
    const auto c = plainContext();
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    const auto layers = composeLayers (s, spans, c);
    const auto kicks = kickHits (layers[(size_t) L::drums]);
    REQUIRE_FALSE (kicks.empty());
    for (const auto g : startsOf (layers[(size_t) L::guitar]))
        CHECK (near (kicks, g, 1.0e-6));
    const auto bassStarts = startsOf (layers[(size_t) L::bass]);
    REQUIRE_FALSE (bassStarts.empty());
    for (const auto b : bassStarts)
        CHECK (near (kicks, b, 1.0e-6));
    // Sync stays on: another breakdown take moves the others with it.
    auto other = s;
    other.lane (L::drums).variation = 77;
    const auto layers2 = composeLayers (other, spans, c);
    if (kickHits (layers2[(size_t) L::drums]) != kicks)
        CHECK (layers2[(size_t) L::guitar].notes != layers[(size_t) L::guitar].notes);
}

TEST_CASE ("0.86.0: the composer's SYNC key: the leader chosen and locked, DOUBLE on the bass and drums", "[sync][composer][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    ComposerPage page (studio);
    page.setSize (1700, 940);
    auto* key = page.keyNamed ("SYNC");
    REQUIRE (key != nullptr);
    CHECK (key->getButtonText() == "SYNC: OFF");
    page.setSync (L::guitar);
    CHECK (key->getButtonText() == "SYNC: GUITAR");
    REQUIRE (page.getSync().has_value());
    CHECK (*page.getSync() == L::guitar);
    CHECK (page.keyNamed ("GUITAR LOCK")->getToggleState());
    page.setDoubleSync (L::bass, true);
    CHECK (page.isDoublingSync (L::bass));
    page.setDoubleSync (L::chords, true);   // (only the bass and the drums double)
    CHECK_FALSE (page.isDoublingSync (L::chords));
    // GENERATE keeps the leader (locked); the rest follow its rhythm.
    page.generate();
    CHECK (*page.getSync() == L::guitar);
    page.setSync (std::nullopt);
    CHECK (key->getButtonText() == "SYNC: OFF");
}

// Hidden: prints a synced riff, its doubled bass and the kicks, two bars (PARAGON_SYNC_VARIATION).
TEST_CASE ("0.86.0: print a synced section", "[.syncdump]")
{
    auto s = tritoneState();
    s.lane (L::guitar).variation = juce::SystemStats::getEnvironmentVariable ("PARAGON_SYNC_VARIATION", "3").getIntValue();
    s.syncLane = (int) L::guitar;
    s.lane (L::bass).doubleSync = true;
    s.lane (L::drums).doubleSync = juce::SystemStats::getEnvironmentVariable ("PARAGON_SYNC_DRUMS_DOUBLE", "1") == "1";
    const auto c = plainContext();
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    const auto layers = composeLayers (s, spans, c);
    const auto line = [] (const char* name, const LineIdea& idea, bool kicksOnly)
    {
        juce::String text;
        for (auto step = 0; step < 32; ++step)
        {
            juce::StringArray here;
            for (const auto& n : idea.notes)
                if (! n.keyswitch && std::abs (n.beat - step * 0.25) < 0.01 && (! kicksOnly || n.pitch == 36 || n.pitch == 35))
                    here.add (kicksOnly ? "K" : pr7s::noteName (n.pitch) + (GuitarVelocity::midiOf (n.velocity) < 60 ? "m" : ""));
            text << (step % 4 == 0 ? "|" : " ") << (here.isEmpty() ? juce::String ("-") : here.joinIntoString ("+"));
        }
        std::printf ("%-7s %s\n", name, text.toRawUTF8());
    };
    std::printf ("%s / %s\n", layers[(size_t) L::guitar].style.toRawUTF8(), layers[(size_t) L::guitar].description.toRawUTF8());
    line ("GUITAR", layers[(size_t) L::guitar], false);
    line ("BASS", layers[(size_t) L::bass], false);
    line ("KICK", layers[(size_t) L::drums], true);
    line ("CHORDS", layers[(size_t) L::chords], false);
}
