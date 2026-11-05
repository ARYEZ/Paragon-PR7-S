#include "engine/DrumIdeas.h"
#include "engine/MidiImport.h"

#include <cstring>

namespace pr7s
{
    namespace
    {
        int pcOf (int note) { return ((note % 12) + 12) % 12; }
        int bassOf (const Chord& chord) { return chord.bass >= 0 ? chord.bass : chord.root; }

        int nearestPitch (int pitchClass, double around, int low, int high)
        {
            auto best = -1;
            for (auto n = low; n <= high; ++n)
                if (pcOf (n) == pcOf (pitchClass) && (best < 0 || std::abs (n - around) < std::abs (best - around)))
                    best = n;
            return best < 0 ? low : best;
        }

        const ChordSpan& spanAt (const std::vector<ChordSpan>& spans, double songBeat)
        {
            for (const auto& s : spans)
                if (songBeat < s.end - 1.0e-9)
                    return s;
            return spans.back();
        }

        float of (int midi) { return (float) juce::jlimit (1, 127, midi) / 127.0f; }

        void add (std::vector<LineNote>& notes, double at, double length, int pitch, int velocity)
        {
            notes.push_back ({ at, juce::jmax (0.05, length), pitch, of (velocity) });
        }

        // A drum's row: the note and its bars, "A|B|C" (a character per step).
        struct Row { int note; const char* bars; };
        struct BeatDef
        {
            const char* name;
            const char* description;
            std::vector<Row> rows;
        };

        enum class Fill { snareToToms, snareRoll, kickSnare, tomSextuplets, tomsDown };

        Fill fillOf (GuitarStyle style)
        {
            switch (style)
            {
                case GuitarStyle::rock: return Fill::snareToToms;
                case GuitarStyle::thrash: return Fill::snareRoll;
                case GuitarStyle::numetal: return Fill::kickSnare;
                case GuitarStyle::metalcore: return Fill::tomSextuplets;
                case GuitarStyle::shred: return Fill::tomsDown;
            }
            return Fill::snareToToms;
        }

        // (Each style's sixth beat, "Locked to the riff", is built from the riff: not listed here.)
        const std::vector<BeatDef>& beatsOf (GuitarStyle style)
        {
            using namespace GmDrum;
            static const std::vector<BeatDef> rock {
                { "Straight rock", "Kick on one and the and of three, snare on two and four, eighth-note hats (the rock beat)",
                  { { kick, "X-----x-x-------" }, { snare, "----X-------X---" }, { closedHat, "x-x-x-x-x-x-x-x-" } } },
                { "Four on the floor", "Kick on every beat, open hats on the off-beats (driving rock, dance rock)",
                  { { kick, "X---x---x---x---" }, { snare, "----X-------X---" }, { closedHat, "x-------x-------|x---x---x---x---" }, { openHat, "--x---x---x---x-|--x---x---x---x-" } } },
                { "Half-time", "The snare on three only: big and slow under the same tempo (ballads, heavy choruses)",
                  { { kick, "X-----x---x-----" }, { snare, "--------X-------" }, { closedHat, "x-x-x-x-x-x-x-x-" } } },
                { "Ride groove", "The ride cymbal on the eighths, the bell on the beats (a lifted chorus or solo section)",
                  { { kick, "X-----x-x-------|X-----x-x---x---" }, { snare, "----X-------X---" }, { ride, "x-x-x-x-x-x-x-x-" }, { rideBell, "X---X---X---X---" } } },
                { "Ghost-note groove", "Sixteenth hats and soft snare ghost notes between the backbeats (funk rock, hard rock grooves)",
                  { { kick, "X-----x---x-----|X-----x---x--x--" }, { snare, "----X--g-g--X--g" }, { closedHat, "Xxxxxxxxxxxxxxxx" } } } };
            static const std::vector<BeatDef> thrash {
                { "Skank beat", "Kick on the beats, snare on every off-beat, hats driving: the thrash beat",
                  { { kick, "X---x---x---x---" }, { snare, "--X---X---X---X-" }, { closedHat, "x-x-x-x-x-x-x-x-" } } },
                { "Double-kick gallop", "The double kick galloping with the guitars, the snare on two and four",
                  { { kick, "X-xxx-xxx-xxx-xx" }, { snare, "----X-------X---" }, { ride, "x---x---x---x---" } } },
                { "Sixteenth double kick", "Constant sixteenth-note double kick under a backbeat (speed metal)",
                  { { kick, "Xxxxxxxxxxxxxxxx" }, { snare, "----X-------X---" }, { ride, "x-x-x-x-x-x-x-x-" } } },
                { "Blast beat", "Kick and cymbal together, the snare between every one: the blast beat",
                  { { kick, "X-x-x-x-x-x-x-x-" }, { snare, "-x-x-x-x-x-x-x-x" }, { ride, "x-x-x-x-x-x-x-x-" } } },
                { "Half-time breakdown", "Half time: the snare on three, the kick with the riff, the china on the beats",
                  { { kick, "X--x--x---x-x---" }, { snare, "--------X-------" }, { china, "x---x---x---x---" } } } };
            static const std::vector<BeatDef> numetal {
                { "Bounce groove", "Syncopated kicks with space between them, a heavy backbeat (the nu-metal bounce)",
                  { { kick, "X--x--x---x-----" }, { snare, "----X-------X---" }, { closedHat, "x-x-x-x-x-x-x-x-" } } },
                { "Syncopated stomp", "Kicks off the beat, the hats opening on the and of two and four",
                  { { kick, "X-x---x-x--x----" }, { snare, "----X-------X---" }, { closedHat, "x-x---x-x-x---x-" }, { openHat, "----x-------x---" } } },
                { "Hip-hop swing", "A swung, laid-back groove: the hip-hop side of nu-metal",
                  { { kick, "X-----x-x---" }, { snare, "---X-----X--" }, { closedHat, "x-xx-xx-xx-x" } } },
                { "Stop-start", "Hits together with the riff, then silence (the band stops)",
                  { { kick, "X-x---------x-x-" }, { snare, "----X-------X---" }, { closedHat, "x-x-x-x---------" } } },
                { "Tom groove", "The floor tom in place of the hats: a tribal, rolling verse",
                  { { kick, "X-----x-x-------" }, { snare, "----X-------X---" }, { tomLow, "x-x-x-x-x-x-x-x-" } } } };
            static const std::vector<BeatDef> metalcore {
                { "Breakdown", "Kicks with the breakdown's chugs, the snare on three, the china on the beats",
                  { { kick, "X--x--x-----x-x-" }, { snare, "--------X-------" }, { china, "x---x---x---x---" } } },
                { "Double-time verse", "Eighth-note kicks, the snare on two and four, the ride driving (the fast verse)",
                  { { kick, "X-x-x-x-x-x-x-x-" }, { snare, "----X-------X---" }, { ride, "x-x-x-x-x-x-x-x-" } } },
                { "Gallop double kick", "Galloping double kick, crashes on the beats",
                  { { kick, "X-xxx-xxx-xxx-xx" }, { snare, "----X-------X---" }, { crash, "x---x---x---x---" } } },
                { "Half-time chorus", "Half time with the ride: the big, open chorus",
                  { { kick, "X-----x-x-------" }, { snare, "--------X-------" }, { ride, "x-x-x-x-x-x-x-x-" } } },
                { "Blast into breakdown", "A bar of blast beat, then a bar of breakdown",
                  { { kick, "X-x-x-x-x-x-x-x-|X--x--x-----x-x-" }, { snare, "-x-x-x-x-x-x-x-x|--------X-------" },
                    { ride, "x-x-x-x-x-x-x-x-|----------------" }, { china, "----------------|x---x---x---x---" } } } };
            static const std::vector<BeatDef> shred {
                { "Power metal double kick", "Sixteenth-note double kick, snare on two and four, crashes: power metal",
                  { { kick, "Xxxxxxxxxxxxxxxx" }, { snare, "----X-------X---" }, { ride, "x-x-x-x-x-x-x-x-" } } },
                { "Galloping double kick", "The double kick galloping under the solo, crashes on the beats",
                  { { kick, "X-xxx-xxx-xxx-xx" }, { snare, "----X-------X---" }, { crash, "x---x---x---x---" } } },
                { "Driving rock", "Kick on the beats, eighth-note hats: out of the guitar's way",
                  { { kick, "X---x---x---x---" }, { snare, "----X-------X---" }, { closedHat, "x-x-x-x-x-x-x-x-" } } },
                { "Neoclassical march", "A marching kick pattern with a ghosted snare (neoclassical metal)",
                  { { kick, "X--x--x-x--x--x-" }, { snare, "----X------gX--g" }, { ride, "x-x-x-x-x-x-x-x-" } } },
                { "Half-time ballad", "Half time with the ride: for the slow, melodic solo",
                  { { kick, "X-------x-------" }, { snare, "--------X-------" }, { ride, "x-x-x-x-x-x-x-x-" } } } };
            switch (style)
            {
                case GuitarStyle::rock: return rock;
                case GuitarStyle::thrash: return thrash;
                case GuitarStyle::numetal: return numetal;
                case GuitarStyle::metalcore: return metalcore;
                case GuitarStyle::shred: return shred;
            }
            return rock;
        }

        // A row's bar for bar `k` of a four-bar phrase: A, A B A B, or A B A C.
        juce::String barOf (const char* bars, int k)
        {
            const auto parts = juce::StringArray::fromTokens (bars, "|", "");
            if (parts.isEmpty())
                return {};
            if (parts.size() == 1)
                return parts[0];
            if (parts.size() == 2)
                return parts[k % 2];
            return k % 4 == 1 ? parts[1] : k % 4 == 3 ? parts[2] : parts[0];
        }

        int velocityOf (juce::juce_wchar c)
        {
            return c == 'X' ? 118 : c == 'x' ? 96 : c == 'g' ? 40 : 0;
        }

        // Each four-bar phrase: a crash with its first kick, a fill on its last beat.
        void phrase (LineIdea& idea, GuitarStyle style, double beatsPerBar, double total, int variation)
        {
            using namespace GmDrum;
            auto& notes = idea.notes;
            for (auto bar = 0; bar * beatsPerBar < total - 1.0e-9; bar += 4)
            {
                const auto at = bar * beatsPerBar;
                auto hasCrash = false;
                for (const auto& n : notes)
                    if (std::abs (n.beat - at) < 1.0e-6 && (n.pitch == crash || n.pitch == china || n.pitch == crash2))
                        hasCrash = true;
                if (! hasCrash)
                    add (notes, at, 1.0, bar % 8 == 0 ? crash : crash2, 112);
            }
            for (auto bar = 3; (bar + 1) * beatsPerBar <= total + 1.0e-9; bar += 4)
            {
                const auto end = (bar + 1) * beatsPerBar, from = end - juce::jmin (1.0, beatsPerBar);
                notes.erase (std::remove_if (notes.begin(), notes.end(), [&] (const LineNote& n) { return n.beat >= from - 1.0e-9 && n.beat < end - 1.0e-9; }),
                             notes.end());
                const auto fill = fillOf (style);
                const auto odd = variation % 2 == 1;
                switch (fill)
                {
                    case Fill::snareToToms:
                    {
                        const int seq[4] = { snare, snare, odd ? tomMid : tomHigh, tomLow };
                        for (auto k = 0; k < 4; ++k)
                            add (notes, from + k * 0.25, 0.25, seq[k], 84 + k * 10);
                        break;
                    }
                    case Fill::snareRoll:
                        for (auto k = 0; k < (odd ? 6 : 4); ++k)
                        {
                            const auto step = 1.0 / (odd ? 6 : 4);
                            add (notes, from + k * step, step, snare, 80 + k * 7);
                            add (notes, from + k * step, step, kick, 90);
                        }
                        break;
                    case Fill::kickSnare:
                        add (notes, from, 0.25, kick, 110);
                        add (notes, from + 0.25, 0.25, snare, 90);
                        add (notes, from + 0.5, 0.25, odd ? tomLow : snare, 104);
                        add (notes, from + 0.75, 0.25, kick, 112);
                        break;
                    case Fill::tomSextuplets:
                    {
                        const int seq[6] = { tomHigh, tomHigh, tomMid, tomMid, tomLow, tomLow };
                        for (auto k = 0; k < 6; ++k)
                        {
                            add (notes, from + k / 6.0, 1.0 / 6.0, odd ? seq[5 - k] : seq[k], 88 + k * 6);
                            add (notes, from + k / 6.0, 1.0 / 6.0, kick, 92);
                        }
                        break;
                    }
                    case Fill::tomsDown:
                    {
                        const int seq[4] = { tomHigh, tomMid, tomLow, tomFloor };
                        for (auto k = 0; k < 4; ++k)
                            add (notes, from + k * 0.25, 0.25, seq[k], 90 + k * 8);
                        break;
                    }
                }
            }
        }
    }

    //==========================================================================
    std::vector<double> riffHits (const LineIdea& riff)
    {
        std::vector<double> hits;
        for (const auto& n : riff.notes)
            if (hits.empty() || n.beat - hits.back() >= HumanizeLimits::together)
                hits.push_back (n.beat);
        return hits;
    }

    std::vector<double> kickHits (const LineIdea& drums)
    {
        std::vector<double> hits;
        for (const auto& n : drums.notes)
            if (n.pitch == GmDrum::kick || n.pitch == 35)
                hits.push_back (n.beat);
        std::sort (hits.begin(), hits.end());
        hits.erase (std::unique (hits.begin(), hits.end(), [] (double a, double b) { return std::abs (a - b) < 1.0e-6; }), hits.end());
        return hits;
    }

    std::vector<LineIdea> drumIdeas (GuitarStyle style, const std::vector<ChordSpan>& spans, double beatsPerBar, int variation, const LineIdea* riff)
    {
        using namespace GmDrum;
        std::vector<LineIdea> ideas;
        if (spans.empty() || beatsPerBar <= 0.0)
            return ideas;
        const auto total = spans.back().end - spans.front().start;
        for (const auto& def : beatsOf (style))
        {
            LineIdea idea { LineIdea::Part::drums, def.name, def.description, {} };
            for (auto bar = 0; bar * beatsPerBar < total - 1.0e-9; ++bar)
            {
                // (MORE IDEAS: the odd variations swap the phrase's second and fourth bars.)
                const auto k = variation % 2 == 1 && bar % 2 == 1 ? bar + 2 : bar;
                for (const auto& row : def.rows)
                {
                    const auto steps = barOf (row.bars, k);
                    if (steps.isEmpty())
                        continue;
                    const auto step = beatsPerBar / steps.length();
                    for (auto i = 0; i < steps.length(); ++i)
                        if (const auto v = velocityOf (steps[i]); v > 0)
                        {
                            const auto at = bar * beatsPerBar + i * step;
                            if (at < total - 1.0e-9)
                                add (idea.notes, at, step, row.note, v);
                        }
                }
            }
            phrase (idea, style, beatsPerBar, total, variation);
            std::stable_sort (idea.notes.begin(), idea.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
            ideas.push_back (std::move (idea));
        }
        // Locked to the riff: the kick on its hits.
        {
            const auto halfTime = style == GuitarStyle::numetal || style == GuitarStyle::metalcore;
            LineIdea idea { LineIdea::Part::drums, "Locked to the riff",
                            riff != nullptr ? "The kick on every hit of the guitar riff \"" + riff->style + "\", " + (halfTime ? "the snare on three" : "the snare on two and four")
                                            : juce::String ("The kick on one and three (no guitar riff to follow yet)"),
                            {} };
            std::vector<double> hits;
            if (riff != nullptr)
                hits = riffHits (*riff);
            else
                for (auto b = 0.0; b < total - 1.0e-9; b += 2.0)
                    hits.push_back (b);
            for (const auto h : hits)
                if (h < total - 1.0e-9)
                    add (idea.notes, h, 0.25, kick, std::abs (std::fmod (h, beatsPerBar)) < 1.0e-6 ? 118 : 100);
            for (auto bar = 0; bar * beatsPerBar < total - 1.0e-9; ++bar)
                for (auto beat = 0; beat < (int) std::round (beatsPerBar); ++beat)
                {
                    const auto at = bar * beatsPerBar + beat;
                    if (at >= total - 1.0e-9)
                        break;
                    if (halfTime ? beat == 2 : beat % 2 == 1)
                        add (idea.notes, at, 0.5, snare, 116);
                    add (idea.notes, at, 0.5, style == GuitarStyle::metalcore ? china : style == GuitarStyle::thrash || style == GuitarStyle::shred ? ride : closedHat, 92);
                }
            phrase (idea, style, beatsPerBar, total, variation);
            std::stable_sort (idea.notes.begin(), idea.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
            ideas.push_back (std::move (idea));
        }
        return ideas;
    }

    LineIdea bassOnHits (const std::vector<ChordSpan>& spans, const std::vector<double>& hits, const juce::String& name, const juce::String& description)
    {
        LineIdea idea { LineIdea::Part::bass, name, description, {} };
        if (spans.empty())
            return idea;
        const auto origin = spans.front().start, total = spans.back().end - origin;
        auto sorted = hits;
        std::sort (sorted.begin(), sorted.end());
        double prev = 38.0;
        for (size_t i = 0; i < sorted.size(); ++i)
        {
            const auto at = sorted[i];
            if (at < -1.0e-9 || at >= total - 1.0e-9)
                continue;
            const auto& span = spanAt (spans, at + origin);
            const auto next = i + 1 < sorted.size() ? sorted[i + 1] : total;
            const auto length = juce::jmin (next, span.end - origin) - at;
            if (length <= 1.0e-6)
                continue;
            const auto pitch = nearestPitch (bassOf (span.chord), prev, LineRanges::bassLow, LineRanges::bassHigh);
            prev = pitch;
            idea.notes.push_back ({ at, length * 0.92, pitch, std::abs (std::fmod (at, 1.0)) < 1.0e-6 ? 0.86f : 0.74f });
        }
        return idea;
    }

    //==========================================================================
    std::vector<LibraryGroove> findLibraryGrooves (const std::vector<MediaItem>& items, GuitarStyle style, double songBpm)
    {
        static const juce::StringArray drumWords { "drum", "groove", "beat", "kick", "snare", "ggd", "toontrack", "ezd", "ezdrummer", "sd3",
                                                   "superior", "getgood", "addictive", "bfd", "mtpdk", "perfect drums" };
        juce::StringArray styleWords;
        switch (style)
        {
            case GuitarStyle::rock: styleWords = { "rock", "pop", "punk", "hard" }; break;
            case GuitarStyle::thrash: styleWords = { "thrash", "speed", "skank", "blast", "death" }; break;
            case GuitarStyle::numetal: styleWords = { "nu metal", "nu-metal", "numetal", "nu_metal", "bounce", "hip hop", "hiphop" }; break;
            case GuitarStyle::metalcore: styleWords = { "core", "breakdown", "djent", "chug" }; break;
            case GuitarStyle::shred: styleWords = { "power", "prog", "double", "shred", "speed" }; break;
        }
        std::vector<LibraryGroove> out;
        for (const auto& item : items)
        {
            if (item.type != MediaType::midi)
                continue;
            const auto text = (item.file.getFullPathName() + " " + item.tags.category + " " + item.tags.subCategory + " " + item.tags.style).toLowerCase();
            auto drummy = item.tags.category.containsIgnoreCase ("drum");
            for (const auto& w : drumWords)
                drummy = drummy || text.contains (w);
            if (! drummy)
                continue;
            auto score = 0;
            for (const auto& w : styleWords)
                if (text.contains (w))
                    score += 4;
            if (style != GuitarStyle::rock && text.contains ("metal"))
                score += 1;
            if (text.contains ("fill"))
                score -= 3;
            const auto bpm = item.tempo();
            if (bpm > 0.0 && songBpm > 0.0)
            {
                auto ratio = songBpm / bpm;
                for (const auto k : { 0.5, 2.0 })
                    if (std::abs (std::log2 (ratio * k)) < std::abs (std::log2 (ratio)))
                        ratio *= k;
                const auto off = std::abs (std::log2 (ratio));
                score += off < 0.05 ? 4 : off < 0.12 ? 2 : off > 0.3 ? -2 : 0;
            }
            out.push_back ({ item.file, item.name, item.file.getParentDirectory().getFileName(), bpm, score });
        }
        std::stable_sort (out.begin(), out.end(), [] (const LibraryGroove& a, const LibraryGroove& b)
        {
            return a.score != b.score ? a.score > b.score : a.name.compareNatural (b.name) < 0;
        });
        return out;
    }

    std::optional<LineIdea> grooveIdea (const LibraryGroove& groove, const std::vector<ChordSpan>& spans, double beatsPerBar, juce::String& error)
    {
        if (spans.empty() || beatsPerBar <= 0.0)
            return std::nullopt;
        const auto file = readMidiFile (groove.file, error);
        if (! file)
            return std::nullopt;
        std::vector<Note> notes;
        auto end = 0.0;
        for (const auto& track : file->tracks)
            for (const auto& n : track.notes)
            {
                notes.push_back (n);
                end = juce::jmax (end, n.start + n.length);
            }
        if (notes.empty())
        {
            error = "No notes in " + groove.file.getFileName();
            return std::nullopt;
        }
        // Whole bars, the groove repeated over the chords from the first bar.
        auto first = notes.front().start;
        for (const auto& n : notes)
            first = juce::jmin (first, n.start);
        const auto startBar = std::floor (first / beatsPerBar + 1.0e-9) * beatsPerBar;
        const auto length = juce::jmax (beatsPerBar, std::ceil ((end - startBar) / beatsPerBar - 1.0e-6) * beatsPerBar);
        const auto total = spans.back().end - spans.front().start;
        const auto bars = (int) std::llround (length / beatsPerBar);
        LineIdea idea { LineIdea::Part::groove, groove.name,
                        "From " + groove.folder + (groove.bpm > 0.0 ? ", " + juce::String (juce::roundToInt (groove.bpm)) + " BPM" : juce::String())
                            + ", " + juce::String (bars) + (bars == 1 ? " bar" : " bars") + ", repeated",
                        {} };
        for (auto from = 0.0; from < total - 1.0e-9; from += length)
            for (const auto& n : notes)
            {
                const auto at = from + n.start - startBar;
                if (at < total - 1.0e-9)
                    idea.notes.push_back ({ at, juce::jmin (n.length, total - at), n.pitch, n.velocity });
            }
        std::stable_sort (idea.notes.begin(), idea.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        return idea;
    }
}
