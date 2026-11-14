#include <catch2/catch_test_macros.hpp>

#include <set>

#include "engine/Studio.h"
#include "ui/LinesPage.h"

using namespace pr7s;

namespace
{
    using T = Chord::Type;
    using X = Chord::Tension;
    int pcOf (int n) { return ((n % 12) + 12) % 12; }

    std::vector<ChordSpan> bars (std::initializer_list<Chord> chords, const Key& key, double barBeats = 4.0)
    {
        std::vector<ChordSpan> spans;
        auto at = 0.0;
        for (const auto& c : chords)
        {
            spans.push_back ({ at, at + barBeats, c, key });
            at += barBeats;
        }
        return spans;
    }

    const ChordSpan& spanAt (const std::vector<ChordSpan>& spans, double beat)
    {
        for (const auto& s : spans)
            if (beat >= s.start - 1.0e-9 && beat < s.end - 1.0e-9)
                return s;
        return spans.back();
    }

    bool isChordTone (const Chord& c, int pitch)
    {
        for (const auto p : c.pitchClasses())
            if (p == pcOf (pitch))
                return true;
        return false;
    }

    const LineIdea& named (const std::vector<LineIdea>& ideas, const juce::String& name)
    {
        for (const auto& i : ideas)
            if (i.style == name)
                return i;
        FAIL ("no idea named " << name);
        return ideas.front();
    }

    int midi (const LineNote& n) { return GuitarVelocity::midiOf (n.velocity); }
    bool muted (const LineNote& n) { return midi (n) >= 1 && midi (n) <= 59; }

    // A bar's notes as (place in the bar, pitch, velocity): two bars compared as played.
    std::vector<std::tuple<int, int, int>> barNotes (const LineIdea& idea, int bar, double barBeats = 4.0)
    {
        std::vector<std::tuple<int, int, int>> out;
        for (const auto& n : idea.notes)
            if (n.beat >= bar * barBeats - 1.0e-9 && n.beat < (bar + 1) * barBeats - 1.0e-9)
                out.emplace_back ((int) std::lround ((n.beat - bar * barBeats) * 96.0), n.pitch, midi (n));
        std::sort (out.begin(), out.end());
        return out;
    }

    int onsets (const LineIdea& idea)
    {
        std::set<long> at;
        for (const auto& n : idea.notes)
            at.insert (std::lround (n.beat * 96.0));
        return (int) at.size();
    }
}

TEST_CASE ("Guitar styles: six rhythm ideas and six leads each, on the chords, in the guitar's range, in the library's layers (0.53.0)", "[lines][riffs]")
{
    const Key am { 5, 0, true };
    const auto spans = bars ({ { 9, T::minor, X::none, -1 }, { 5, T::major, X::none, -1 }, { 0, T::major, X::none, -1 }, { 7, T::major, X::none, -1 },
                               { 11, T::diminished, X::none, -1 }, { 4, T::major, X::none, -1 }, { 9, T::minor, X::none, -1 }, { 4, T::major, X::none, -1 } }, am);
    CHECK (guitarStyles().size() == 6);   // (0.64.0: GROOVE)
    for (const auto style : guitarStyles())
    {
        CHECK (guitarStyleNamed (guitarStyleName (style)) == style);
        CHECK (guitarStyleDescription (style).isNotEmpty());
        for (const auto& tuning : { GuitarTuning::all()[0], GuitarTuning::named ("6-string standard (E)"), GuitarTuning::named ("8-string drop E") })
            for (int variation = 0; variation < 3; ++variation)
                for (const auto barsLong : { 1, 2, 4 })
                {
                    RiffSettings riff;
                    riff.bars = barsLong;
                    riff.fills = variation == 2;
                    const auto rhythm = guitarRhythmIdeas (spans, 4.0, variation, tuning, {}, style, riff);
                    const auto lead = guitarLeadIdeas (spans, 4.0, variation, tuning, {}, style);
                    REQUIRE (rhythm.size() == 6);
                    REQUIRE (lead.size() == 7);   // (0.65.0: the seventh, the whole solo)
                    std::set<juce::String> names;
                    for (const auto* list : { &rhythm, &lead })
                        for (const auto& idea : *list)
                        {
                            INFO (guitarStyleName (style) << ", " << tuning.name << ": " << idea.style << " (variation " << variation << ", " << barsLong << " bars)");
                            names.insert (idea.style);
                            CHECK (idea.description.isNotEmpty());
                            REQUIRE_FALSE (idea.notes.empty());
                            CHECK (std::is_sorted (idea.notes.begin(), idea.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; }));
                            const auto isLead = idea.part == LineIdea::Part::lead;
                            for (const auto& n : idea.notes)
                            {
                                CHECK (n.beat >= -1.0e-9);
                                CHECK (n.length > 0.0);
                                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
                                CHECK ((midi (n) >= 1 && midi (n) <= 127));
                                if (isLead)
                                {
                                    CHECK (n.pitch >= tuning.strings[1] + 5);
                                    CHECK (n.pitch <= tuning.strings.back() + 22);
                                }
                                else
                                {
                                    // Never where a library keeps its keyswitches, nor past the 24th fret.
                                    CHECK (n.pitch >= tuning.lowest());
                                    CHECK (n.pitch <= tuning.highest());
                                }
                            }
                            if (isLead && ! idea.style.startsWith ("Solo"))   // (0.116.0: a solo's pickups run into its chord tones)
                            {
                                // Each bar's first note is a chord tone (a twin's third or an octave may sound with it).
                                for (int bar = 0; bar < 8; ++bar)
                                {
                                    double first = 1.0e9;
                                    for (const auto& n : idea.notes)
                                        if (n.beat >= bar * 4.0 - 1.0e-9 && n.beat < bar * 4.0 + 4.0 - 1.0e-9)
                                            first = std::min (first, n.beat);
                                    if (first > 1.0e8)
                                        continue;
                                    bool tone = false;
                                    for (const auto& n : idea.notes)
                                        if (std::abs (n.beat - first) < 1.0e-6 && isChordTone (spanAt (spans, n.beat).chord, n.pitch + (n.effect == LineNote::Effect::bend ? 2 : 0)))   // (a bend sounds a whole step up)
                                            tone = true;
                                    CHECK (tone);
                                }
                            }
                            else if (style != GuitarStyle::rock)
                            {
                                // A bar's first hit: the chord's root (or its notes) or the key's low note (the pedal).
                                for (int bar = 0; bar < 8; ++bar)
                                    for (const auto& n : idea.notes)
                                        if (std::abs (n.beat - bar * 4.0) < 1.0e-6 && n.effect == LineNote::Effect::none)   // (0.64.0: GROOVE's stabs are dissonant on purpose)
                                        {
                                            const auto& s = spans[(size_t) bar];
                                            CHECK ((isChordTone (s.chord, n.pitch) || pcOf (n.pitch) == 9));
                                        }
                            }
                        }
                    CHECK (names.size() == 13);   // (every idea its own name)
                }
    }
}

TEST_CASE ("Riff length: one bar repeats, two bars go A B, four bars A B A and a turnaround (0.53.0)", "[lines][riffs]")
{
    const Key em { 2, 0, true };
    const Chord e { 4, T::minor, X::none, -1 };
    const auto spans = bars ({ e, e, e, e, e, e, e, e }, em);
    const auto& tuning = GuitarTuning::all()[0];
    const auto riff = [] (int length) { RiffSettings r; r.bars = length; return r; };
    const auto gallop = [&] (int length) { return named (guitarRhythmIdeas (spans, 4.0, 0, tuning, {}, GuitarStyle::thrash, riff (length)), "Downpicked gallop"); };
    const auto one = gallop (1), two = gallop (2), four = gallop (4);
    for (int bar = 1; bar < 8; ++bar)
        CHECK (barNotes (one, bar) == barNotes (one, 0));
    CHECK (barNotes (two, 1) != barNotes (two, 0));
    CHECK (barNotes (two, 2) == barNotes (two, 0));
    CHECK (barNotes (two, 3) == barNotes (two, 1));
    CHECK (barNotes (four, 2) == barNotes (four, 0));
    CHECK (barNotes (four, 1) == barNotes (two, 1));
    CHECK (barNotes (four, 3) != barNotes (four, 1));   // the turnaround
    CHECK (barNotes (four, 7) == barNotes (four, 3));
    // The pedal: E minor's low E on the 7-string drop A (A1 33: E2 is 40); the dyads E and B.
    for (const auto& n : one.notes)
        if (muted (n))
            CHECK ((n.pitch == 40 || n.pitch == 47));
}

TEST_CASE ("Riff settings: palm mutes, tightness, busyness and fills (0.53.0)", "[lines][riffs]")
{
    const Key am { 5, 0, true };
    const Chord a { 9, T::minor, X::none, -1 }, f { 5, T::major, X::none, -1 }, c { 0, T::major, X::none, -1 }, g { 7, T::major, X::none, -1 };
    const auto spans = bars ({ a, f, c, g, a, f, c, g }, am);
    const auto& tuning = GuitarTuning::all()[0];
    const auto rhythm = [&] (GuitarStyle style, const char* name, RiffSettings r, int variation = 0)
    {
        return named (guitarRhythmIdeas (spans, 4.0, variation, tuning, {}, style, r), name);
    };
    const auto count = [] (const LineIdea& idea, auto&& is) { return (int) std::count_if (idea.notes.begin(), idea.notes.end(), is); };

    // As written at 50%: rock's ideas exactly as 0.19.0's.
    {
        const auto before = guitarRhythmIdeas (spans, 4.0, 1, tuning);
        const auto now = guitarRhythmIdeas (spans, 4.0, 1, tuning, {}, GuitarStyle::rock, RiffSettings {});
        REQUIRE (before.size() == now.size());
        for (size_t i = 0; i < before.size(); ++i)
            CHECK (before[i].notes == now[i].notes);
        const auto leadBefore = guitarLeadIdeas (spans, 4.0, 1, tuning);
        const auto leadNow = guitarLeadIdeas (spans, 4.0, 1, tuning, {}, GuitarStyle::rock);
        for (size_t i = 0; i < leadBefore.size(); ++i)
            CHECK (leadBefore[i].notes == leadNow[i].notes);
    }

    // PALM MUTES: more muted notes above 50%, fewer below; a bar's first hit never changed by it.
    {
        RiffSettings more, fewer;
        more.palmMutes = 1.0f;
        fewer.palmMutes = 0.0f;
        const auto written = rhythm (GuitarStyle::rock, "Octave chords", {});
        const auto muchMuted = rhythm (GuitarStyle::rock, "Octave chords", more);
        CHECK (count (written, muted) == 0);
        CHECK (count (muchMuted, muted) > written.notes.size() / 3);
        for (const auto& n : muchMuted.notes)
            if (std::fmod (n.beat, 4.0) < 1.0e-6)
                CHECK_FALSE (muted (n));
        const auto chugs = rhythm (GuitarStyle::thrash, "Tremolo pedal", {});
        const auto rung = rhythm (GuitarStyle::thrash, "Tremolo pedal", fewer);
        CHECK (count (rung, muted) < count (chugs, muted) / 2);
    }

    // TIGHTNESS: the mutes' velocities lower (and shorter) when tight, higher when loose, all still palm mutes.
    {
        RiffSettings tight, loose;
        tight.tightness = 1.0f;
        loose.tightness = 0.0f;
        const auto mean = [] (const LineIdea& idea)
        {
            double sum = 0.0;
            int n = 0;
            for (const auto& note : idea.notes)
                if (muted (note))
                {
                    sum += midi (note);
                    ++n;
                }
            return n > 0 ? sum / n : 0.0;
        };
        const auto t = rhythm (GuitarStyle::metalcore, "Breakdown", tight);
        const auto l = rhythm (GuitarStyle::metalcore, "Breakdown", loose);
        const auto w = rhythm (GuitarStyle::metalcore, "Breakdown", {});
        CHECK (mean (t) < mean (w));
        CHECK (mean (l) > mean (w));
        CHECK (count (t, muted) == count (w, muted));
        CHECK (count (l, muted) == count (w, muted));
        for (const auto& n : t.notes)
            if (muted (n))
                CHECK (midi (n) == GuitarVelocity::muteTight);
    }

    // BUSYNESS: fewer hits below 50%, chugs added above; a bar's first hit always kept.
    {
        RiffSettings sparse, busy;
        sparse.busyness = 0.0f;
        busy.busyness = 1.0f;
        const auto w = rhythm (GuitarStyle::numetal, "Bounce riff", {});
        const auto s = rhythm (GuitarStyle::numetal, "Bounce riff", sparse);
        const auto b = rhythm (GuitarStyle::numetal, "Bounce riff", busy);
        CHECK (onsets (s) < onsets (w));
        CHECK (onsets (b) > onsets (w));
        for (int bar = 0; bar < 8; ++bar)
            CHECK (! barNotes (s, bar).empty());
        for (const auto& n : b.notes)   // the added chugs: palm muted on the chord's root (or the pedal)
            CHECK ((isChordTone (spanAt (spans, n.beat).chord, n.pitch) || pcOf (n.pitch) == 9 || pcOf (n.pitch) == 10 || pcOf (n.pitch) == 3));   // (the pedal, its half step and flat fifth)
        // A strum isn't chugged.
        CHECK (rhythm (GuitarStyle::rock, "Open strummed shapes", busy).notes.size() == rhythm (GuitarStyle::rock, "Open strummed shapes", {}).notes.size());
    }

    // FILLS: the last beat of each phrase (every 4 bars for a one-bar riff, else the riff's length).
    {
        RiffSettings filled;
        filled.fills = true;
        filled.bars = 1;
        const auto plain = rhythm (GuitarStyle::thrash, "Tremolo pedal", RiffSettings { 1 });
        const auto fill = rhythm (GuitarStyle::thrash, "Tremolo pedal", filled);   // variation 0: a chromatic run
        for (int bar = 0; bar < 8; ++bar)
            if (bar == 3 || bar == 7)
                CHECK (barNotes (fill, bar) != barNotes (plain, bar));
            else
                CHECK (barNotes (fill, bar) == barNotes (plain, bar));
        // The run: four sixteenths on the last beat, down by half steps into the next chord's root (A after G).
        std::vector<int> run;
        for (const auto& n : fill.notes)
            if (n.beat >= 15.0 - 1.0e-9 && n.beat < 16.0 - 1.0e-9)
                run.push_back (n.pitch);
        REQUIRE (run.size() == 4);
        for (size_t i = 1; i < run.size(); ++i)
            CHECK (run[i] == run[i - 1] - 1);
        CHECK (pcOf (run.back() - 1) == 9);
        // Nothing of the riff left sounding into the fill.
        for (const auto& n : fill.notes)
            if (n.beat < 15.0 - 1.0e-9)
                CHECK (n.beat + n.length <= 15.0 + 1.0e-6);
        // Two-bar riffs: a fill every second bar; the odd variations a burst (thrash) of six triplet chugs.
        filled.bars = 2;
        const auto burst = rhythm (GuitarStyle::thrash, "Tremolo pedal", filled, 1);
        for (const auto bar : { 1, 3, 5, 7 })
        {
            int inFill = 0;
            for (const auto& n : burst.notes)
                if (n.beat >= bar * 4.0 + 3.0 - 1.0e-9 && n.beat < bar * 4.0 + 4.0 - 1.0e-9)
                    ++inFill;
            CHECK (inFill == 6);
        }
    }
}

TEST_CASE ("Riff settings and the guitar style are kept: text, limits, the studio (0.53.0)", "[lines][riffs]")
{
    RiffSettings r;
    CHECK (r.toString() == "2,0.50,0.50,0.50,0");
    r.bars = 3;
    r.palmMutes = 2.0f;
    r.fills = true;
    const auto l = r.limited();
    CHECK (l.bars == 4);
    CHECK (l.palmMutes == 1.0f);
    CHECK (RiffSettings::fromString (l.toString()) == l);
    CHECK (RiffSettings::fromString ("nonsense") == RiffSettings {});
    CHECK (RiffSettings::fromString ("1,2,3") == RiffSettings {});
    CHECK (guitarStyleNamed ("nu-metal") == GuitarStyle::numetal);
    CHECK (guitarStyleNamed ("polka") == GuitarStyle::rock);

    Studio studio (false);
    CHECK (studio.getGuitarStyle() == GuitarStyle::rock);
    CHECK (studio.getRiffSettings() == RiffSettings {});
    studio.setGuitarStyle (GuitarStyle::metalcore);
    studio.setRiffSettings (l);
    CHECK (studio.getGuitarStyle() == GuitarStyle::metalcore);
    CHECK (studio.getRiffSettings() == l);
}

TEST_CASE ("IDEAS' guitar tab: STYLE deals the style's twelve, RIFF SETTINGS deals again (0.53.0)", "[lines][riffs][ui]")
{
    Studio studio (false);
    const Key am { 5, 0, true };
    const auto spans = bars ({ { 9, T::minor, X::none, -1 }, { 5, T::major, X::none, -1 }, { 0, T::major, X::none, -1 }, { 4, T::major, X::none, -1 } }, am);
    LinesPage page (studio, spans);
    page.setTab (LinesPage::Tab::guitar);
    CHECK (page.getNumCardsShown() == 13);
    CHECK (page.getIdeas (LineIdea::Part::rhythm).front().style == "Power chords");
    page.setGuitarStyle (GuitarStyle::thrash);
    CHECK (studio.getGuitarStyle() == GuitarStyle::thrash);
    CHECK (page.getIdeas (LineIdea::Part::rhythm).front().style == "Downpicked gallop");
    CHECK (page.getIdeas (LineIdea::Part::lead).front().style == "Chromatic tremolo");
    CHECK (page.getNumCardsShown() == 13);
    page.setGuitarStyle (GuitarStyle::shred);
    CHECK (page.getIdeas (LineIdea::Part::lead).front().style == "Sweep arpeggio");
    const auto before = page.getIdeas (LineIdea::Part::rhythm).front().notes;
    RiffSettings r;
    r.bars = 4;
    r.fills = true;
    page.setRiffSettings (r);
    CHECK (studio.getRiffSettings() == r);
    CHECK (page.getIdeas (LineIdea::Part::rhythm).front().notes != before);
    CHECK (RiffPanel::summary (r) == "RIFF: 4 BARS, FILLS");

    RiffPanel panel (r);
    RiffSettings seen;
    panel.onChange = [&] (const RiffSettings& s) { seen = s; };
    auto quieter = r;
    quieter.busyness = 0.2f;
    panel.setSettings (quieter);
    CHECK (seen == quieter.limited());
    panel.setSettings ({});
    CHECK (seen == RiffSettings {});
}
