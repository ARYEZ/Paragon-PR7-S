#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>

#include "engine/Studio.h"
#include "engine/Composer.h"
#include "engine/SectionTypes.h"
#include "ui/ComposerTracksPage.h"

// (0.193.0, the owner's 70 song files) The ACOUSTIC GUITAR lane, SECOND ACOUSTIC and the shared acoustic-friendly options.

using namespace pr7s;

namespace
{
    Chord chordOf (int root, Chord::Type type)
    {
        Chord c;
        c.root = root;
        c.type = type;
        return c;
    }

    // A minor: Am F C G, a bar each, twice (8 bars).
    std::vector<ChordSpan> acousticSpans()
    {
        Key aMinor;
        aMinor.letter = 5;
        aMinor.minor = true;
        const Chord chords[] { chordOf (9, Chord::Type::minor), chordOf (5, Chord::Type::major), chordOf (0, Chord::Type::major), chordOf (7, Chord::Type::major) };
        std::vector<ChordSpan> spans;
        for (int b = 0; b < 8; ++b)
            spans.push_back ({ b * 4.0, b * 4.0 + 4.0, chords[b % 4], aMinor });
        return spans;
    }

    const ChordSpan& spanAt (const std::vector<ChordSpan>& spans, double t)
    {
        for (const auto& s : spans)
            if (t >= s.start - 1.0e-9 && t < s.end - 1.0e-9)
                return s;
        return spans.back();
    }

    LineIdea styleNamed (const juce::String& name, int second = 0, double bpm = 120.0)
    {
        AcousticRequest r;
        r.style = acousticGuitarStyles().indexOf (name, true);
        REQUIRE (r.style >= 0);
        r.second = second;
        r.bpm = bpm;
        return acousticGuitarPart (acousticSpans(), r);
    }

    // The strums: notes starting within 30 ms of the group's first, in time order.
    std::vector<std::vector<LineNote>> groups (const LineIdea& idea, bool harmony = false)
    {
        std::vector<std::vector<LineNote>> out;
        for (const auto& n : idea.notes)
        {
            if (n.harmony != harmony)
                continue;
            if (! out.empty() && n.beat - out.back().front().beat < 0.05)
                out.back().push_back (n);
            else
                out.push_back ({ n });
        }
        return out;
    }

    juce::String fingerprint (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 192.0) << (n.harmony ? "h" : "") << ";";
        return k;
    }
}

TEST_CASE ("ACOUSTIC GUITAR: a lane after CLEAN GUITAR, off to start with, its styles, kept with the others (0.193.0)", "[composer][acoustic]")
{
    using L = ComposerLayer;
    const auto& order = composerLayers();
    const auto at = [&order] (L l) { return std::find (order.begin(), order.end(), l) - order.begin(); };
    CHECK (at (L::acoustic) == at (L::clean) + 1);
    CHECK (composerLayerName (L::acoustic) == "ACOUSTIC GUITAR");
    CHECK (composerPartOf (L::acoustic) == LineIdea::Part::acoustic);
    CHECK (composerStyleCount (L::acoustic) == acousticGuitarStyles().size());
    CHECK (acousticGuitarStyles().size() == 35 + 10);   // (0.217.0: ten more)
    CHECK (secondAcousticParts().size() == 7);
    CHECK (secondAcousticParts()[0] == "LOOSER COPY");

    ComposerState s;
    CHECK_FALSE (s.lane (L::acoustic).on);
    CHECK (s.lane (L::acoustic).second == 0);

    // A 0.192.0 state (seven lanes, twelve fields each) still reads: the acoustic lane at its defaults, the others as they were.
    auto old = s;
    old.lane (L::drums).style = 2;
    old.lane (L::clean).on = true;
    old.lane (L::guitar).approach = 40;
    const auto parts = juce::StringArray::fromTokens (old.toString(), "|", "");
    auto lanes = juce::StringArray::fromTokens (parts[7], ";", "");
    lanes.remove ((int) L::acoustic);
    for (auto& lane : lanes)
    {
        auto fields = juce::StringArray::fromTokens (lane, ",", "");
        fields.remove (12);
        lane = fields.joinIntoString (",");
    }
    auto oldParts = parts;
    oldParts.set (7, lanes.joinIntoString (";"));
    const auto read = ComposerState::fromString (oldParts.joinIntoString ("|"));
    CHECK (read.lane (L::drums).style == 2);
    CHECK (read.lane (L::clean).on);
    CHECK (read.lane (L::guitar).approach == 40);   // (0.193.0: approaches past 15 no longer cut on reading)
    CHECK_FALSE (read.lane (L::acoustic).on);
    CHECK (read.lane (L::acoustic).second == 0);

    s.lane (L::acoustic).on = true;
    s.lane (L::acoustic).style = 12;
    s.lane (L::acoustic).second = 3;
    CHECK (ComposerState::fromString (s.toString()) == s);

    // Every style its own part, through ARCHITECT's choices.
    ComposerContext c;
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    REQUIRE_FALSE (spans.empty());
    const auto choices = composerChoices (L::acoustic, s, spans, c);
    REQUIRE ((int) choices.size() == acousticGuitarStyles().size());
    std::set<juce::String> different;
    for (size_t i = 0; i < choices.size(); ++i)
    {
        INFO (acousticGuitarStyles()[(int) i]);
        CHECK_FALSE (choices[i].notes.empty());
        CHECK (choices[i].part == LineIdea::Part::acoustic);
        CHECK (choices[i].description.isNotEmpty());
        different.insert (fingerprint (choices[i]));
    }
    CHECK (different.size() == choices.size());
    // Not written by a section type (it plays its own styles under any).
    for (const auto& [section, type] : std::vector<std::pair<juce::String, juce::String>> { { "Chorus", "DRIVING" }, { "Verse", "CLEAN VERSE" },
                                                                                             { "Bridge", "HALF-TIME MELODIC" } })
    {
        auto t = s;
        applySectionType (t, section, type);
        CHECK_FALSE (sectionTypeWritesLane (L::acoustic, t, c));
        CHECK (sectionApproaches (t, L::acoustic).isEmpty());
    }
}

TEST_CASE ("ACOUSTIC GUITAR: every style's shape: range, the grid, the chords (0.193.0)", "[composer][acoustic]")
{
    const auto spans = acousticSpans();
    for (int i = 0; i < acousticGuitarStyles().size(); ++i)
    {
        const auto name = acousticGuitarStyles()[i];
        INFO (name);
        AcousticRequest r;
        r.style = i;
        const auto idea = acousticGuitarPart (spans, r);
        REQUIRE_FALSE (idea.notes.empty());
        for (const auto& n : idea.notes)
        {
            CHECK (n.pitch >= AcousticVoicing::lowest);
            CHECK (n.pitch <= AcousticVoicing::highest);
            CHECK (n.beat >= -1.0e-9);
            CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
            CHECK_FALSE (n.harmony);   // (LOOSER COPY: none)
        }
        // Each strum or picked note starts on the sixteenth grid (the hammer-ons a thirty-second after it).
        for (const auto& g : groups (idea))
        {
            const auto steps = g.front().beat / 0.25;
            const auto off = std::abs (steps - std::round (steps));
            CHECK ((off < 1.0e-6 || std::abs (off - 0.5) < 1.0e-6));
        }
    }
    // The chords followed: every note of the power eighths is its chord's.
    {
        const auto idea = styleNamed ("Acoustic power eighths");
        for (const auto& n : idea.notes)
        {
            const auto pcs = spanAt (spans, n.beat).chord.pitchClasses();
            CHECK (std::find (pcs.begin(), pcs.end(), n.pitch % 12) != pcs.end());
        }
        CHECK (groups (idea).size() == 64);   // (every eighth)
    }
    // The add9 arpeggio starts again on each chord's root (the root in 40-51).
    {
        const auto idea = styleNamed ("Add9 arpeggio");
        for (const auto& span : spans)
        {
            const auto first = std::find_if (idea.notes.begin(), idea.notes.end(), [&] (const LineNote& n) { return std::abs (n.beat - span.start) < 1.0e-6; });
            REQUIRE (first != idea.notes.end());
            CHECK (first->pitch % 12 == span.chord.root);
            CHECK (first->pitch >= 40);
            CHECK (first->pitch <= 51);
        }
    }
    // The open voicings: from the root's string, root-fifth-octave-third-fifth, an open string's ninth on top where it fits.
    {
        const auto c = AcousticVoicing::open (chordOf (0, Chord::Type::major));
        CHECK (c == std::vector<int> { 48, 55, 60, 64, 67, 74 });
        const auto am = AcousticVoicing::open (chordOf (9, Chord::Type::minor), false);
        CHECK (am == std::vector<int> { 45, 52, 57, 60, 64, 69 });
        CHECK (AcousticVoicing::power (chordOf (7, Chord::Type::major)) == std::vector<int> { 43, 50, 55 });
        const auto high = AcousticVoicing::high (chordOf (5, Chord::Type::major));
        CHECK (high.size() == 5);
        CHECK (high.back() >= 69);
        CHECK (high.back() <= 76);
    }
}

TEST_CASE ("ACOUSTIC GUITAR: strums spread down low to high, up high to low, and ring to the next (0.193.0)", "[composer][acoustic]")
{
    const auto idea = styleNamed ("Strum with flicks: quarters", 0, 120.0);
    const auto strums = groups (idea);
    auto downs = 0, ups = 0;
    for (const auto& g : strums)
    {
        if (g.size() < 2)
            continue;
        const auto spread = g.back().beat - g.front().beat;
        CHECK (spread >= 0.005 * 2.0 - 1.0e-9);    // (5 ms at 120 BPM: 0.01 beat)
        CHECK (spread <= 0.015 * 2.0 + 1.0e-9);    // (15 ms)
        const auto step = std::fmod (g.front().beat, 1.0);
        const auto rising = g.back().pitch > g.front().pitch;
        if (std::abs (step) < 1.0e-6)
        {
            CHECK (rising);   // (on the beat: down, low to high)
            ++downs;
        }
        if (std::abs (step - 0.75) < 1.0e-6)
        {
            CHECK_FALSE (rising);   // (the last sixteenth: up, high to low)
            ++ups;
        }
    }
    CHECK (downs == 32);
    CHECK (ups == 16);
    // Every note rings until the next strum starts (and not past it).
    for (size_t i = 0; i + 1 < strums.size(); ++i)
        for (const auto& n : strums[i])
            CHECK (n.beat + n.length <= strums[i + 1].front().beat + 1.0e-6);
    for (size_t i = 0; i + 1 < strums.size(); ++i)
        CHECK (strums[i].front().beat + strums[i].front().length > strums[i + 1].front().beat - 0.01);
    // The velocities: downs on the beat louder than the sixteenth ups.
    CHECK (strums[0].front().velocity > strums[3].front().velocity);
}

TEST_CASE ("ACOUSTIC GUITAR: the style rules (0.193.0)", "[composer][acoustic]")
{
    const auto spans = acousticSpans();
    const auto onsetsIn = [] (const LineIdea& idea, int bar)
    {
        std::set<int> at;
        for (const auto& g : groups (idea))
            if (g.front().beat >= bar * 4.0 - 1.0e-6 && g.front().beat < bar * 4.0 + 4.0 - 1.0e-6)
                at.insert (juce::roundToInt ((g.front().beat - bar * 4.0) / 0.25));
        return at;
    };
    // BIG STRUMS: X---o-o-X---o-o-, the o's on the top three strings only.
    {
        const auto idea = styleNamed ("Big strums on one and three, light tops between");
        CHECK (onsetsIn (idea, 0) == std::set<int> { 0, 4, 6, 8, 12, 14 });
        CHECK (onsetsIn (idea, 3) == std::set<int> { 0 });   // (every fourth bar one chord, held)
        for (const auto& g : groups (idea))
            CHECK ((g.size() == 3 || g.size() >= 5));
    }
    // STRUM OFF THE ONE: nothing on any downbeat.
    {
        const auto idea = styleNamed ("Strum off the one");
        for (int b = 0; b < 8; ++b)
            CHECK (onsetsIn (idea, b).count (0) == 0);
    }
    // ONE-BAR FINGERPICK, ONE BAR OF AIR: the odd bars empty.
    {
        const auto idea = styleNamed ("One-bar fingerpick, one bar of air");
        CHECK (onsetsIn (idea, 1).empty());
        CHECK (onsetsIn (idea, 0).size() == 8);
    }
    // SKIPPING STRUM: X--X-XX--X-XX-XX.
    CHECK (onsetsIn (styleNamed ("Skipping strum"), 2) == std::set<int> { 0, 3, 5, 6, 9, 11, 12, 14, 15 });
    // ROOT CHORD AND HIGH CLUSTER IN TURN: 0, 6, 12; the cluster bars high.
    {
        const auto idea = styleNamed ("Root chord and high cluster in turn");
        CHECK (onsetsIn (idea, 0) == std::set<int> { 0, 6, 12 });
        for (const auto& n : idea.notes)
            if (n.beat >= 4.0 && n.beat < 8.0)
                CHECK (n.pitch >= 58);
    }
    // FIXED-TOP FINGERPICKING: the top notes are the key's own whatever the chord.
    {
        const auto idea = styleNamed ("Fixed-top fingerpicking");
        std::set<int> tops;
        for (const auto& n : idea.notes)
            if (n.pitch >= 62)
                tops.insert (n.pitch);
        CHECK (tops.size() <= 3);
    }
    // STRUMMED TRIADS IN SIXES: three-note triads, 6+6+4.
    {
        const auto idea = styleNamed ("Strummed triads in sixes");
        CHECK (onsetsIn (idea, 0) == std::set<int> { 0, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 14, 15 });
        for (const auto& g : groups (idea))
            CHECK (g.size() == 3);
    }
}

TEST_CASE ("ACOUSTIC GUITAR: SECOND ACOUSTIC onto the lane's second track (0.193.0)", "[composer][acoustic]")
{
    Studio studio (false);
    std::set<juce::String> different;
    for (int part = 1; part < secondAcousticParts().size(); ++part)
    {
        INFO (secondAcousticParts()[part]);
        const auto idea = styleNamed ("Add9 arpeggio", part);
        auto harmony = 0;
        for (const auto& n : idea.notes)
            if (n.harmony)
            {
                ++harmony;
                CHECK (n.pitch >= AcousticVoicing::lowest);
                CHECK (n.pitch <= AcousticVoicing::highest);
            }
        CHECK (harmony > 0);
        const auto first = studio.doubledCopy (idea, 0, 0.0, 2), secondTrack = studio.doubledCopy (idea, 1, 0.0, 2);
        CHECK (std::none_of (first.notes.begin(), first.notes.end(), [] (const LineNote& n) { return n.harmony; }));
        CHECK (std::all_of (secondTrack.notes.begin(), secondTrack.notes.end(), [] (const LineNote& n) { return n.harmony; }));
        CHECK ((int) secondTrack.notes.size() == harmony);
        // On one track both play there.
        CHECK (studio.doubledCopy (idea, 0, 0.0, 1).notes.size() == idea.notes.size());
        LineIdea only;
        for (const auto& n : idea.notes)
            if (n.harmony)
                only.notes.push_back (n);
        different.insert (fingerprint (only));
    }
    CHECK ((int) different.size() == secondAcousticParts().size() - 1);
    // LOOSER COPY writes none (the second track a looser copy of the first).
    const auto plain = styleNamed ("Add9 arpeggio", 0);
    CHECK (std::none_of (plain.notes.begin(), plain.notes.end(), [] (const LineNote& n) { return n.harmony; }));
    // AS THE BASS: low root-fifth dyads on 1, 2 and 2&.
    {
        const auto idea = styleNamed ("Add9 arpeggio", secondAcousticParts().indexOf ("AS THE BASS"));
        for (const auto& n : idea.notes)
            if (n.harmony)
            {
                CHECK (n.pitch <= 58);
                const auto k = juce::roundToInt (std::fmod (n.beat, 4.0) / 0.25);
                CHECK ((k == 0 || k == 4 || k == 6));
            }
    }
    // Through ARCHITECT: the lane's own field.
    ComposerState s;
    s.lane (ComposerLayer::acoustic).on = true;
    s.lane (ComposerLayer::acoustic).second = secondAcousticParts().indexOf ("HIGH THIRDS");
    const ComposerContext c;
    const auto idea = composeLayers (s, composerSpans (s, 0.0, c.beatsPerBar), c)[(size_t) ComposerLayer::acoustic];
    CHECK (std::any_of (idea.notes.begin(), idea.notes.end(), [] (const LineNote& n) { return n.harmony; }));
}

TEST_CASE ("ACOUSTIC GUITAR: built onto an \"Acoustic Guitar\" track, its own ARCHITECT TRACKS row (0.193.0)", "[composer][acoustic]")
{
    Studio studio (false);
    CHECK (Studio::linePartName (LineIdea::Part::acoustic) == "Acoustic Guitar");
    CHECK (Studio::composerTrackLane (LineIdea::Part::acoustic) == 7);
    CHECK (Studio::composerTrackPart (7) == LineIdea::Part::acoustic);
    CHECK (Studio::composerTrackLane (LineIdea::Part::clean) == 6);   // (the others keep their rows)
    CHECK (ComposerTracksPage::laneName (7) == "ACOUSTIC GUITAR");
    CHECK (studio.getComposerTracks (LineIdea::Part::acoustic).names == juce::StringArray ("Acoustic Guitar"));
    studio.setComposerTracks (LineIdea::Part::acoustic, { juce::StringArray ("Acoustic L", "Acoustic R"), true });
    CHECK (studio.getComposerTracks (LineIdea::Part::acoustic).names == juce::StringArray ("Acoustic L", "Acoustic R"));
    CHECK (studio.getComposerTracks (LineIdea::Part::clean).names == juce::StringArray ("Clean Guitar"));

    ComposerState s;
    s.lane (ComposerLayer::acoustic).on = true;
    const ComposerContext c;
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    const auto idea = composeLayers (s, spans, c)[(size_t) ComposerLayer::acoustic];
    const auto track = studio.addLinePart (idea, spans, 0);
    REQUIRE (track != 0);
    CHECK (studio.getProject().findTrack (track)->name == "Acoustic L");
}

TEST_CASE ("ACOUSTIC GUITAR: the riff styles follow the guitar lane (0.193.0)", "[composer][acoustic]")
{
    ComposerState s;
    s.lane (ComposerLayer::acoustic).on = true;
    s.lane (ComposerLayer::acoustic).style = acousticGuitarStyles().indexOf ("Strummed tonic on the riff's accents");
    const ComposerContext c;
    const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
    const auto layers = composeLayers (s, spans, c);
    const auto rhythm = syncRhythmOf (ComposerLayer::guitar, layers[(size_t) ComposerLayer::guitar], c.beatsPerBar, 0.0);
    REQUIRE_FALSE (rhythm.accents.empty());
    const auto acoustic = layers[(size_t) ComposerLayer::acoustic];
    auto matched = 0;
    for (const auto& g : groups (acoustic))
    {
        const auto t = g.front().beat;
        const auto onAccent = std::any_of (rhythm.accents.begin(), rhythm.accents.end(), [t] (double a) { return std::abs (a - t) < 1.0e-6; });
        const auto flick = std::any_of (rhythm.accents.begin(), rhythm.accents.end(), [t] (double a) { return t - a > 0.2 && t - a < 0.55; });
        CHECK ((t < 1.0e-6 || onAccent || flick));
        matched += onAccent ? 1 : 0;
        // The tonic as a major chord (no minor third over the key's tonic).
        if (onAccent)
            for (const auto& n : g)
                CHECK (n.pitch % 12 != (s.key.pitchClass() + 3) % 12);
    }
    CHECK (matched > 4);
    // With SYNC on the acoustic the guitar follows it, and nothing goes round.
    auto synced = s;
    synced.syncLane = (int) ComposerLayer::acoustic;
    const auto followed = composeLayers (synced, spans, c);
    CHECK_FALSE (followed[(size_t) ComposerLayer::guitar].notes.empty());
}

TEST_CASE ("0.193.0: round seven's shared options (drums, bass, keys, guitar)", "[sections][types][acoustic]")
{
    const ComposerContext c;
    const auto spans = acousticSpans();
    const std::vector<std::pair<ComposerLayer, juce::StringArray>> added {
        { ComposerLayer::drums, { "TAMBOURINE ON TWO AND FOUR ONLY", "SHAKER AND HATS, KICK EVERY FOURTH BAR", "HALF TIME WITH TAMBOURINE AND SHAKER",
                                  "SHAKER SIXTEENTHS, CABASA, BACKBEAT", "RIDE EIGHTHS, PEDAL HAT, SHAKER", "FLOOR TOM IN 3+3+2, NO KICK",
                                  "PEDAL HAT, DOTTED KICK, LATE SNARE", "MARACAS AND CONGAS IN THREES", "SIDESTICK AND SHAKER",
                                  "BONGOS ON THE GUITAR'S RHYTHM", "TAMBOURINE EIGHTHS OVER THE BACKBEAT" } },
        { ComposerLayer::bass, { "ROOT ON ONE HELD, REPEAT ON FOUR", "ROOT-FIFTH TRESILLO WITH THE TOM", "LATE PUSHES UNDER HELD CHORDS",
                                 "WALK-UPS EVERY SECOND BAR" } },
        { ComposerLayer::chords, { "STRINGS HALF NOTES WITH THE STRUM", "OCTAVE COUNTERLINE", "CELLO DOUBLES THE RIFF", "STRINGS PULSE ON THE ROOT",
                                   "BELL DOUBLES THE GUITAR" } },
        { ComposerLayer::guitar, { "POWER-CHORD QUARTERS OVER THE STRUM", "THE VERSE'S STRUM, DISTORTED" } } };
    for (const auto& [section, type] : std::vector<std::pair<juce::String, juce::String>> { { "Chorus", "DRIVING" }, { "Verse", "CLEAN VERSE" } })
    {
        ComposerState s;
        applySectionType (s, section, type);
        for (const auto& [l, names] : added)
        {
            const auto extras = extraApproaches (l);
            const auto all = sectionApproaches (s, l);
            if (l == ComposerLayer::bass)
                CHECK (all[all.size() - 1] == "LOCKED WITH THE KICK");
            std::set<juce::String> seen;
            for (int i = 0; i < all.size(); ++i)
            {
                auto t = s;
                t.lane (l).approach = i;
                const auto idea = composeLayers (t, spans, c)[(size_t) l];
                const auto k = fingerprint (idea);
                if (names.contains (all[i]))
                {
                    INFO (type << " " << all[i]);
                    CHECK (extras.contains (all[i]));
                    CHECK (! idea.notes.empty());
                    CHECK (idea.description.isNotEmpty());
                    for (const auto& n : idea.notes)
                        CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
                    CHECK (seen.count (k) == 0);
                }
                seen.insert (k);
            }
            for (const auto& name : names)
                CHECK (all.contains (name));
        }
    }
    // Every new name only once in the lists (and not one of the types' own).
    for (const auto& [l, names] : added)
        for (const auto& name : names)
        {
            INFO (name);
            const auto list = extraApproaches (l);
            CHECK (std::count (list.begin(), list.end(), name) == 1);
        }

    ComposerState s;
    applySectionType (s, "Chorus", "DRIVING");
    const auto play = [&] (ComposerLayer l, const juce::String& name)
    {
        auto t = s;
        t.lane (l).approach = sectionApproaches (t, l).indexOf (name);
        REQUIRE (t.lane (l).approach >= 0);
        return composeLayers (t, spans, c)[(size_t) l];
    };
    const auto pitches = [] (const LineIdea& idea)
    {
        std::set<int> out;
        for (const auto& n : idea.notes)
            out.insert (n.pitch);
        return out;
    };
    const auto stepsOf = [] (const LineIdea& idea, int pitch)
    {
        std::set<int> out;
        for (const auto& n : idea.notes)
            if (n.pitch == pitch)
                out.insert (juce::roundToInt (std::fmod (n.beat, 4.0) / 0.25));
        return out;
    };
    {
        const auto tamb = play (ComposerLayer::drums, "TAMBOURINE ON TWO AND FOUR ONLY");
        CHECK (pitches (tamb) == std::set<int> { 49, 54 });
        CHECK (stepsOf (tamb, 54) == std::set<int> { 4, 12 });
        CHECK (std::count_if (tamb.notes.begin(), tamb.notes.end(), [] (const LineNote& n) { return n.pitch == 49; }) == 1);
    }
    {
        const auto tom = play (ComposerLayer::drums, "FLOOR TOM IN 3+3+2, NO KICK");
        CHECK (pitches (tom).count (36) == 0);
        CHECK (stepsOf (tom, 41) == std::set<int> { 0, 6, 12 });
        CHECK (stepsOf (tom, 42) == std::set<int> { 0, 4, 8, 12 });
    }
    {
        const auto hand = play (ComposerLayer::drums, "MARACAS AND CONGAS IN THREES");
        CHECK (stepsOf (hand, 64) == std::set<int> { 0, 12 });
        CHECK (stepsOf (hand, 38) == std::set<int> { 3 });
        CHECK (pitches (hand).count (70) == 1);
        CHECK (pitches (hand).count (36) == 0);
    }
    {
        const auto side = play (ComposerLayer::drums, "SIDESTICK AND SHAKER");
        CHECK (stepsOf (side, 37) == std::set<int> { 4, 12 });
        CHECK (stepsOf (side, 36) == std::set<int> { 0 });
        CHECK (stepsOf (side, 82).size() == 8);
    }
    {
        const auto bongos = play (ComposerLayer::drums, "BONGOS ON THE GUITAR'S RHYTHM");
        for (const auto p : pitches (bongos))
            CHECK ((p == 60 || p == 61));
    }
    {
        const auto late = play (ComposerLayer::bass, "LATE PUSHES UNDER HELD CHORDS");
        for (const auto& n : late.notes)
            CHECK (std::fmod (n.beat, 4.0) >= 3.0 - 1.0e-6);
    }
    {
        const auto cello = play (ComposerLayer::chords, "CELLO DOUBLES THE RIFF");
        for (const auto& n : cello.notes)
        {
            CHECK (n.pitch >= 36);
            CHECK (n.pitch <= 55);
        }
    }
    {
        const auto quarters = play (ComposerLayer::guitar, "POWER-CHORD QUARTERS OVER THE STRUM");
        std::set<int> at;
        for (const auto& n : quarters.notes)
            at.insert (juce::roundToInt (std::fmod (n.beat, 4.0) / 0.25));
        CHECK (at == std::set<int> { 0, 4, 8, 12 });
    }
}
