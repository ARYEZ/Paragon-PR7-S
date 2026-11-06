#include <catch2/catch_test_macros.hpp>

#include <set>

#include "engine/Studio.h"
#include "engine/Composer.h"
#include "engine/DrumIdeas.h"
#include "ui/ComposerPage.h"
#include "ui/Finish.h"
#include "ui/ParagonLookAndFeel.h"

// THE COMPOSER (0.58.0): every layer stacked, each locked or rolled, one GENERATE; heard through the
// song's tracks without changing the song; USE ALL onto tracks.

using namespace pr7s;

namespace
{
    using L = ComposerLayer;

    const Track* trackNamed (const Studio& studio, const juce::String& name)
    {
        for (const auto& t : studio.getProject().tracks)
            if (t.name == name)
                return &t;
        return nullptr;
    }

    ComposerContext plainContext()
    {
        ComposerContext c;
        c.humanize = 0.0f;
        return c;
    }

    float peakOf (const juce::AudioBuffer<float>& b)
    {
        return juce::jmax (b.getMagnitude (0, 0, b.getNumSamples()), b.getMagnitude (1, 0, b.getNumSamples()));
    }
}

TEST_CASE ("The composer's state: kept as text, the lead off and the bass on the kick to start with (0.58.0)", "[composer]")
{
    ComposerState s;
    CHECK_FALSE (s.lane (L::lead).on);
    CHECK (s.lane (L::chords).on);
    CHECK (s.lane (L::bass).style == composerStyleCount (L::bass) - 1);
    CHECK (composerStyleCount (L::bass) == 7);
    CHECK (composerStyleCount (L::drums) == 7);   // (0.65.0: SOLO)

    s.key = Key::all()[20];
    s.genre = progressionGenres()[0];
    s.mood = "";
    s.bars = 16;
    s.progression = progressionLibrary()[3].name;
    s.fromChordTrack = true;
    s.toChordTrack = false;
    s.lane (L::guitar) = { false, true, 4, 1234 };
    const auto back = ComposerState::fromString (s.toString());
    CHECK (back == s);
    // Unreadable: the defaults.
    CHECK (ComposerState::fromString ("nonsense") == ComposerState());
    // A style out of range is held to the layer's.
    auto odd = s;
    odd.lane (L::drums).style = 5;
    auto text = odd.toString().replace ("1,0,5,", "1,0,99,");
    CHECK (ComposerState::fromString (text).lane (L::drums).style <= 6);   // (0.65.0: seven sections)
}

TEST_CASE ("The composer's chords: the library's progression in the key, repeated to the length (0.58.0)", "[composer]")
{
    ComposerState s;
    s.key = Key {};   // C major
    s.bars = 8;
    const auto found = composerProgressions (s);
    REQUIRE_FALSE (found.empty());
    // The first that passes when none is named; a named one when it passes.
    CHECK (composerProgressionIndex (s) == found.front());
    s.progression = progressionLibrary()[(size_t) found[2]].name;
    CHECK (composerProgressionIndex (s) == found[2]);

    const auto& p = progressionLibrary()[(size_t) found[2]];
    const auto spans = composerSpans (s, 0.0, 4.0);
    REQUIRE_FALSE (spans.empty());
    const auto bars = (spans.back().end - spans.front().start) / 4.0;
    CHECK (bars >= 8.0 - 1.0e-9);
    CHECK (std::fmod (bars, p.totalBars()) < 1.0e-9);   // (whole repeats)
    CHECK (spans.front().chord == p.in (s.key).front().chord);
    for (size_t i = 1; i < spans.size(); ++i)
        CHECK (spans[i].start == spans[i - 1].end);
    // From a beat: shifted.
    const auto later = composerSpans (s, 32.0, 4.0);
    CHECK (later.front().start == 32.0);

    // A filter nothing passes: no chords.
    s.genre = "NO SUCH GENRE";
    CHECK (composerSpans (s, 0.0, 4.0).empty());
    CHECK (composerProgressionIndex (s) == -1);
}

TEST_CASE ("The composer's layers: every one made, the same state the same notes, a variation others (0.58.0)", "[composer]")
{
    ComposerState s;
    for (auto& lane : s.lanes)
        lane.on = true;
    const auto c = plainContext();
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    REQUIRE_FALSE (spans.empty());
    const auto a = composeLayers (s, spans, c);
    for (const auto l : composerLayers())
    {
        INFO (composerLayerName (l));
        CHECK_FALSE (a[(size_t) l].notes.empty());
        CHECK (a[(size_t) l].part == composerPartOf (l));
        CHECK_FALSE (a[(size_t) l].style.isEmpty());
    }
    const auto b = composeLayers (s, spans, c);
    for (const auto l : composerLayers())
        CHECK (a[(size_t) l].notes == b[(size_t) l].notes);
    auto other = s;
    other.lane (L::melody).variation = 7;
    CHECK (composeLayers (other, spans, c)[(size_t) L::melody].notes != a[(size_t) L::melody].notes);
    // Each style is one of the layer's choices (the style menu).
    for (const auto l : composerLayers())
    {
        const auto choices = composerChoices (l, s, spans, c);
        REQUIRE ((int) choices.size() == composerStyleCount (l));
        CHECK (choices[(size_t) s.lane (l).style].notes == a[(size_t) l].notes);
    }
    // Humanized: the same rules as IDEAS' (still repeatable).
    auto loose = c;
    loose.humanize = 0.6f;
    const auto h1 = composeLayers (s, spans, loose), h2 = composeLayers (s, spans, loose);
    CHECK (h1[(size_t) L::bass].notes == h2[(size_t) L::bass].notes);
    CHECK (h1[(size_t) L::melody].notes != a[(size_t) L::melody].notes);
}

TEST_CASE ("The composer's bass locked to the kick: a note on every kick of the drums lane (0.58.0)", "[composer]")
{
    ComposerState s;
    s.lane (L::bass).style = 6;
    const auto c = plainContext();
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    const auto layers = composeLayers (s, spans, c);
    const auto kicks = kickHits (layers[(size_t) L::drums]);
    REQUIRE_FALSE (kicks.empty());
    const auto& bass = layers[(size_t) L::bass].notes;
    REQUIRE_FALSE (bass.empty());
    for (const auto& n : bass)
        CHECK (std::any_of (kicks.begin(), kicks.end(), [&n] (double k) { return std::abs (k - n.beat) < 1.0e-6; }));
    // Another drums style: the bass follows it.
    auto other = s;
    other.lane (L::drums).style = 4;   // (the breakdown)
    const auto layers2 = composeLayers (other, spans, c);
    CHECK (layers2[(size_t) L::bass].notes != bass);
}

TEST_CASE ("ROLL and GENERATE: a new take of the same style, the progression kept; locked layers kept (0.61.0)", "[composer]")
{
    juce::Random random (42);
    ComposerState s;
    const auto before = s;
    rollLane (s, L::guitar, random);
    CHECK (s.lane (L::guitar).style == before.lane (L::guitar).style);       // (the owner: the approach only from the menu)
    CHECK (s.lane (L::guitar).variation != before.lane (L::guitar).variation);
    const auto progression = composerProgressionIndex (s);
    rollLane (s, L::chords, random);
    CHECK (composerProgressionIndex (s) == progression);

    // Locked: kept through GENERATE.
    s.lane (L::bass).locked = true;
    s.lane (L::chords).locked = true;
    const auto lockedBass = s.lane (L::bass);
    const auto keptProgression = s.progression;
    const auto c = plainContext();
    const auto bassBefore = composeLayers (s, composerSpans (s, 0.0, 4.0), c)[(size_t) L::bass].notes;
    const auto drumsBefore = s.lane (L::drums);
    CHECK (generateComposer (s, random) == composerLayerCount - 2);
    CHECK (s.lane (L::bass) == lockedBass);
    CHECK (s.progression == keptProgression);
    CHECK_FALSE (s.lane (L::drums) == drumsBefore);
    CHECK (s.lane (L::drums).style == drumsBefore.style);
    // The chords locked too: the bass's notes exactly as they were... unless it's on the kick and the drums changed.
    if (lockedBass.style < 6)
        CHECK (composeLayers (s, composerSpans (s, 0.0, 4.0), c)[(size_t) L::bass].notes == bassBefore);
    // Chords from the chord track: rolling them changes the piano part only, not the (absent) progression.
    auto fromTrack = s;
    fromTrack.fromChordTrack = true;
    fromTrack.lane (L::chords).locked = false;
    const auto name = fromTrack.progression;
    rollLane (fromTrack, L::chords, random);
    CHECK (fromTrack.progression == name);
}

TEST_CASE ("Hearing the composer: its layers through the song's tracks, looped, the song untouched (0.58.0)", "[composer][studio]")
{
    Studio studio (false);
    const auto piano = studio.getProject().tracks.front().id;
    // The song: a part elsewhere on the track, and a second track that isn't in the composer.
    Part p;
    p.start = 64.0;
    p.length = 4.0;
    p.notes.push_back ({ 0.0, 1.0, 60, 0.8f });
    studio.addPart (piano, p);
    const auto other = studio.addTrack (Instrument {});
    const auto songBefore = studio.getProject().toXml()->toString();
    studio.setPosition (40.0);

    ComposerState s;
    const auto c = plainContext();
    const auto spans = composerSpans (s, 8.0, 4.0);
    const auto layers = composeLayers (s, spans, c);
    REQUIRE (studio.auditionComposer ({ { piano, layers[(size_t) L::chords] } }, spans.front().start, spans.back().end));
    CHECK (studio.isComposerAuditioning());
    CHECK (studio.getEngine().isPlaying());
    CHECK (studio.getEngine().getPosition() == 8.0);
    CHECK (studio.getProject().toXml()->toString() == songBefore);   // (the engine plays a copy)

    // It sounds (the chords on the track's synth).
    juce::AudioBuffer<float> out (2, 48000);
    studio.getEngine().render (out, 48000, nullptr);
    CHECK (peakOf (out) > 0.001f);

    // Again while playing: carries on from where it is.
    const auto at = studio.getEngine().getPosition();
    REQUIRE (studio.auditionComposer ({ { piano, layers[(size_t) L::melody] } }, spans.front().start, spans.back().end));
    CHECK (studio.getEngine().getPosition() == at);

    // The transport's stop: back to the song where it was.
    studio.stop();
    CHECK_FALSE (studio.isComposerAuditioning());
    CHECK_FALSE (studio.getEngine().isPlaying());
    CHECK (studio.getEngine().getPosition() == 40.0);
    CHECK (studio.getProject().toXml()->toString() == songBefore);

    // A change to the song ends it too.
    REQUIRE (studio.auditionComposer ({ { piano, layers[(size_t) L::chords] } }, spans.front().start, spans.back().end));
    studio.renameTrack (other, "Something");
    CHECK_FALSE (studio.isComposerAuditioning());
    CHECK_FALSE (studio.getEngine().isPlaying());

    // Nothing to play on: false.
    CHECK_FALSE (studio.auditionComposer ({ { 9999, layers[(size_t) L::chords] } }, 0.0, 16.0));
    CHECK_FALSE (studio.isComposerAuditioning());
}

TEST_CASE ("BUILD SECTION onto chosen tracks (0.58.0)", "[composer][studio]")
{
    Studio studio (false);
    studio.addChordEvent (0.0, { 0, Chord::Type::major, Chord::Tension::none, -1 });
    const auto spans = studio.getLineChords();
    const auto mine = studio.addTrack (Instrument {});
    studio.renameTrack (mine, "My Kontakt Bass");
    const auto bass = bassIdeas (spans, 4.0).front();
    const auto melody = melodyIdeas (spans, 4.0).front();
    const auto used = studio.buildSection ({ bass, melody }, spans, "Composer", { mine, 0 });
    REQUIRE (used.size() == 2);
    CHECK (used[0] == mine);
    CHECK (studio.getProject().findTrack (mine)->parts.size() == 1);
    CHECK (studio.getProject().findTrack (used[1])->name == "Melody");
}

TEST_CASE ("The composer page: GENERATE, LOCK, the tracks it plays on, USE ALL (0.58.0)", "[composer][ui]")
{
    Studio studio (false);
    ComposerPage page (studio);
    page.setRandomSeed (7);
    page.setOn (L::lead, false);

    // Generate: the unlocked change, the locked stay.
    page.setLocked (L::melody, true);
    const auto melody = page.getIdea (L::melody).notes;
    const auto progression = page.getState().progression;
    page.setLocked (L::chords, true);
    const auto guitar = page.getState().lane (L::guitar);
    page.generate();
    CHECK (page.getIdea (L::melody).notes == melody);
    CHECK (page.getState().progression == progression);
    CHECK_FALSE (page.getState().lane (L::guitar) == guitar);
    // Kept in the settings for next time.
    CHECK (ComposerState::fromString (studio.getComposerState()) == page.getState());

    // The tracks: none named for the layers yet. Heard on the selected track, except the drums; written to new ones.
    const auto first = studio.getProject().tracks.front().id;
    CHECK (page.trackFor (L::bass, true) == first);
    CHECK (page.trackFor (L::drums, true) == 0);
    CHECK (page.trackFor (L::bass, false) == 0);
    // A track named for a layer: that one, for both.
    const auto drums = studio.addTrack (Instrument {});
    studio.renameTrack (drums, "Drums");
    CHECK (page.trackFor (L::drums, true) == drums);
    CHECK (page.trackFor (L::drums, false) == drums);
    // Chosen: that one.
    page.setTrack (L::bass, first);
    CHECK (page.trackFor (L::bass, false) == first);
    page.setTrack (L::bass, 0);

    // HEAR: the song's tracks, then STOP.
    REQUIRE (page.hear());
    CHECK (page.isHearing());
    page.roll (L::guitar);   // (heard again from where it is)
    CHECK (page.isHearing());
    page.stop();
    CHECK_FALSE (studio.isComposerAuditioning());

    // USE ALL at the playhead's bar: the layers onto their tracks, the chords onto the chord track.
    studio.setPosition (17.0);   // (bar 5 starts at beat 16)
    const auto tracks = page.useAll();
    const auto on = std::count_if (composerLayers().begin(), composerLayers().end(), [&page] (L l) { return page.getState().lane (l).on; });
    CHECK ((long) tracks.size() == on);
    REQUIRE (trackNamed (studio, "Bass") != nullptr);
    CHECK (trackNamed (studio, "Bass")->parts.front().start == 16.0);
    CHECK (studio.getProject().findTrack (drums)->parts.size() == 1);
    CHECK (trackNamed (studio, "Lead Guitar") == nullptr);   // (off: not written)
    const auto& events = studio.getProject().chordTrack.events;
    REQUIRE_FALSE (events.empty());
    CHECK (events.front().beat == 16.0);
    CHECK (events.front().chord == page.getSpans().front().chord);
    CHECK (page.getStatus().contains ("bar 5"));

    // The chords named as written: "bV" in G-sharp minor is D, not C double sharp.
    page.setFromChordTrack (false);
    page.setKey (Key::fromPitchClass (8, true));
    page.setProgression ("Tritone dread");
    REQUIRE (page.getState().progression == "Tritone dread");
    CHECK (page.getChordNames()[0] == "G#m");
    CHECK (page.getChordNames()[1] == "A");
    CHECK (page.getChordNames()[2] == "D");
    for (const auto& name : page.getChordNames())
        CHECK_FALSE (name.contains ("##"));

    // From the chord track: the layers under its chords.
    page.setFromChordTrack (true);
    CHECK (page.getSpans().front().start == 16.0);
    CHECK (page.placeStart() == 16.0);
}

TEST_CASE ("Snapshot: the composer (0.58.0)", "[.][snapshot-composer]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "composer.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        Studio studio (false);
        const auto bass = studio.addTrack (Instrument {});
        studio.renameTrack (bass, "Bass");
        studio.setGuitarStyle (GuitarStyle::metalcore);
        ComposerPage page (studio);
        page.setRandomSeed (11);
        page.setKey (Key::fromPitchClass (4, true));   // (E minor)
        page.generate();
        page.setLocked (L::bass, true);
        page.setOn (L::lead, false);
        page.setSize (1500, 900);
        const auto image = page.createComponentSnapshot (page.getLocalBounds(), true, 1.0f);
        juce::File (path).deleteFile();
        juce::FileOutputStream out { juce::File (path) };
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

TEST_CASE ("Takes: every ROLL of a bass or piano approach a different part, still that approach (0.61.0)", "[composer][takes]")
{
    ComposerState s;
    s.bars = 8;
    const auto c = plainContext();
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    REQUIRE_FALSE (spans.empty());
    const auto origin = spans.front().start;
    const auto spanAt = [&] (double beat) -> const ChordSpan&
    {
        for (const auto& sp : spans)
            if (beat + origin < sp.end - 1.0e-9)
                return sp;
        return spans.back();
    };
    for (const auto layer : { L::bass, L::chords })
        for (int style = 0; style < 6; ++style)
        {
            INFO (composerLayerName (layer) << " style " << style);
            std::vector<std::vector<LineNote>> seen;
            auto t = s;
            t.lane (layer).style = style;
            juce::Random random (7);
            for (int roll = 0; roll < 10; ++roll)
            {
                rollLane (t, layer, random);
                const auto idea = composeLayers (t, spans, c)[(size_t) layer];
                REQUIRE_FALSE (idea.notes.empty());
                CHECK (idea.style == composerChoices (layer, s, spans, c)[(size_t) style].style);   // (the approach's name)
                if (std::find (seen.begin(), seen.end(), idea.notes) == seen.end())
                    seen.push_back (idea.notes);
                // Every chord's first beat: the bass on its root (or slash bass).
                if (layer == L::bass)
                    for (const auto& n : idea.notes)
                        for (const auto& sp : spans)
                            if (std::abs (n.beat - (sp.start - origin)) < 1.0e-6)
                                CHECK (n.pitch % 12 == (sp.chord.bass >= 0 ? sp.chord.bass : sp.chord.root) % 12);
                // Root and fifth: only the root, fifth and octave (and a note leading in at a chord's end).
                if (layer == L::bass && style == 1)
                    for (const auto& n : idea.notes)
                    {
                        const auto& sp = spanAt (n.beat);
                        const auto interval = ((n.pitch - sp.chord.root) % 12 + 12) % 12;
                        const auto endsChord = n.beat + n.length >= sp.end - origin - 1.1;
                        CHECK ((interval == 0 || interval == 7 || interval == 6 || interval == 8 || endsChord));
                    }
            }
            CHECK (seen.size() >= 7);   // (ten rolls: almost all different)
        }
}

TEST_CASE ("BUSY: sparse takes have fewer notes, busy ones more; ROLL keeps it (0.61.0)", "[composer][takes]")
{
    ComposerState s;
    const auto c = plainContext();
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    for (const auto layer : { L::bass, L::chords, L::guitar, L::melody, L::drums })
        for (int style = 0; style < 6; ++style)
        {
            INFO (composerLayerName (layer) << " style " << style);
            if (layer == L::drums && style != 1)
                break;
            auto count = [&] (float busy)
            {
                size_t total = 0;
                for (int v = 0; v < 6; ++v)
                {
                    auto t = s;
                    t.lane (layer).style = style;
                    t.lane (layer).variation = v * 37 + 3;
                    t.lane (layer).busy = busy;
                    std::set<double> onsets;
                    const auto layers = composeLayers (t, spans, c);
                    for (const auto& n : layers[(size_t) layer].notes)
                        if (layer != L::drums || n.pitch == 35 || n.pitch == 36)   // (the drums: BUSY is the kicks' density)
                            onsets.insert (std::round (n.beat * 960.0));
                    total += onsets.size();
                }
                return total;
            };
            const auto sparse = count (0.0f), usual = count (0.5f), busy = count (1.0f);
            CHECK (sparse <= usual);
            CHECK (usual <= busy);
            CHECK (sparse < busy);
        }
    juce::Random random (3);
    auto t = s;
    t.lane (L::bass).busy = 0.8f;
    rollLane (t, L::bass, random);
    CHECK (t.lane (L::bass).busy == 0.8f);
    // Kept in the saved state.
    t.lane (L::guitar).lowString = true;
    const auto back = ComposerState::fromString (t.toString());
    CHECK (back.lane (L::bass).busy == 0.8f);
    CHECK (back.lane (L::guitar).lowString);
    CHECK (back == t);
}

TEST_CASE ("LOW STRING: the bass's roots on its lowest string, within its tuning's range (0.61.0)", "[composer][takes]")
{
    ComposerState s;
    auto c = plainContext();
    c.bassTuning = bassTuningNamed ("5-string drop A");
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    const auto lowest = c.bassTuning.lowest();
    for (int style = 0; style < 6; ++style)
    {
        INFO ("style " << style);
        auto t = s;
        t.lane (L::bass).style = style;
        t.lane (L::bass).lowString = true;
        t.lane (L::bass).variation = 11 + style;
        const auto idea = composeLayers (t, spans, c)[(size_t) L::bass];
        REQUIRE_FALSE (idea.notes.empty());
        auto onLow = 0;
        for (const auto& n : idea.notes)
        {
            CHECK (n.pitch >= lowest);
            CHECK (n.pitch <= bassTop (c.bassTuning));
            if (n.pitch <= lowest + 12)
                ++onLow;
        }
        CHECK (onLow * 10 >= (int) idea.notes.size() * (style == 3 ? 4 : 8));   // (octaves: the high notes leave it)
        // Every chord's first note on the low string.
        for (const auto& sp : spans)
            for (const auto& n : idea.notes)
                if (std::abs (n.beat - (sp.start - spans.front().start)) < 1.0e-6)
                    CHECK (n.pitch <= lowest + 12);
    }
    // Off: never below the tuning's lowest either.
    auto t = s;
    for (int style = 0; style < 7; ++style)
    {
        t.lane (L::bass).style = style;
        const auto layers = composeLayers (t, spans, c);
        for (const auto& n : layers[(size_t) L::bass].notes)
        {
            INFO ("style " << style << " beat " << n.beat << " length " << n.length << " velocity " << n.velocity);
            CHECK (n.pitch >= lowest);
        }
    }
    // The guitar: its riff folded onto the low string.
    auto g = s;
    g.lane (L::guitar).lowString = true;
    const auto guitarLayers = composeLayers (g, spans, c);
    for (const auto& n : guitarLayers[(size_t) L::guitar].notes)
        if (n.velocity * 127.0f < 119.5f)
        {
            INFO ("beat " << n.beat << " length " << n.length << " velocity " << n.velocity);
            CHECK (n.pitch >= c.tuning.lowest());
        }
}

TEST_CASE ("The composer page: ROLL keeps the style, BUSY and LOW STRING keys (0.61.0)", "[composer][ui]")
{
    Studio studio (false);
    ComposerPage page (studio);
    page.setRandomSeed (5);
    page.setStyle (L::bass, 2);
    const auto before = page.getIdea (L::bass).notes;
    page.roll (L::bass);
    CHECK (page.getState().lane (L::bass).style == 2);
    CHECK (page.getIdea (L::bass).style == "Driving eighths");
    CHECK (page.getIdea (L::bass).notes != before);
    page.setBusy (L::bass, 1.0f);
    REQUIRE (page.keyNamed ("BASS BUSY") != nullptr);
    CHECK (page.keyNamed ("BASS BUSY")->getButtonText() == "BUSY 100%");
    page.setLowString (L::bass, true);
    CHECK (page.keyNamed ("BASS LOW")->getToggleState());
    CHECK (page.keyNamed ("BASS LOW")->isVisible());
    CHECK_FALSE (page.keyNamed ("DRUMS LOW")->isVisible());
    page.setLowString (L::drums, true);
    CHECK_FALSE (page.getState().lane (L::drums).lowString);
    page.setTuning (L::bass, "5-string drop A");
    CHECK (studio.getBassTuning().startsWith ("5-string drop A"));
    for (const auto& n : page.getIdea (L::bass).notes)
        CHECK (n.pitch >= 21);
}
