#include "engine/RiffFinder.h"

#include <set>

#include "engine/ChordDetect.h"
#include "engine/KeyCheck.h"
#include "engine/MidiImport.h"

#include "engine/SectionPlan.h"

#include <algorithm>
#include <cstring>
#include <cmath>

namespace pr7s
{
    namespace
    {
        constexpr int guitarLowest = 33, guitarHighest = 88;   // a 7-string in drop A to the 24th fret of the top string

        // The path's words (letters and digits), lower case: the file's name and up to three folders over it.
        juce::StringArray wordsOf (const juce::File& file)
        {
            juce::String text = file.getFileNameWithoutExtension();
            auto folder = file.getParentDirectory();
            for (int up = 0; up < 3 && folder.getFileName().isNotEmpty(); ++up, folder = folder.getParentDirectory())
                text << " " << folder.getFileName();
            juce::StringArray words;
            juce::String word;
            for (auto c : text.toLowerCase() + " ")
            {
                if (juce::CharacterFunctions::isLetterOrDigit (c))
                    word += juce::String::charToString (c);
                else if (word.isNotEmpty())
                {
                    words.add (word);
                    word.clear();
                }
            }
            return words;
        }

        // A word that is one of `list` or starts with it and then has only digits ("verse2").
        bool says (const juce::StringArray& words, std::initializer_list<const char*> list)
        {
            for (const auto& w : words)
                for (const auto* l : list)
                    if (w == l || (w.startsWith (l) && w.substring ((int) std::strlen (l)).containsOnly ("0123456789")))
                        return true;
            return false;
        }

        const std::vector<std::vector<const char*>>& sectionWords()
        {
            // By DrumSection: intro, verse, chorus, bridge, breakdown, ending, solo.
            static const std::vector<std::vector<const char*>> words {
                { "intro" }, { "verse" }, { "chorus", "hook", "refrain" }, { "bridge", "middle8" },
                { "breakdown", "break", "bdown" }, { "outro", "ending", "end" }, { "solo", "lead" } };
            return words;
        }

        bool saysSection (const juce::StringArray& words, int section)
        {
            if (! juce::isPositiveAndBelow (section, (int) sectionWords().size()))
                return false;
            for (const auto* w : sectionWords()[(size_t) section])
                if (says (words, { w }))
                    return true;
            return false;
        }

        // 1 at the song's tempo, falling away by how far (in octaves); twice or half as fast counted at 0.75.
        double tempoFit (double tempo, double song)
        {
            if (tempo <= 0.0 || song <= 0.0)
                return 0.5;
            const auto near = [] (double ratio) { return std::exp (-std::abs (std::log2 (ratio)) * 6.0); };
            return juce::jmax (near (tempo / song), 0.75 * near (tempo / (song * 2.0)), 0.75 * near (tempo * 2.0 / song));
        }

        double wordFit (const juce::StringArray& words, int section)
        {
            double fit = 0.5;
            if (saysSection (words, section))
                fit = 1.0;
            else
                for (int s = 0; s < (int) sectionWords().size(); ++s)
                    if (s != section && saysSection (words, s))
                    {
                        fit = 0.1;
                        break;
                    }
            return fit;
        }

        bool riffWords (const juce::StringArray& words)
        {
            return says (words, { "riff", "riffs", "guitar", "guitars", "gtr", "chug", "chugs", "metal", "djent" });
        }

        // The order files are read in: what the name and tempo promise.
        double promise (const MediaItem& item, const RiffFindRequest& request)
        {
            const auto words = wordsOf (item.file);
            return wordFit (words, (int) request.section) + (riffWords (words) ? 0.5 : 0.0) + tempoFit (item.tempo(), request.bpm);
        }
    }

    bool looksLikeDrums (const MediaItem& item)
    {
        if (item.tags.category.containsIgnoreCase ("drum") || item.tags.category.containsIgnoreCase ("percussion")
            || item.fileTags.category.containsIgnoreCase ("drum"))
            return true;
        // Every folder of its path, not only the nearest three: drum packs nest deep (Toontrack/pack/style/part/file).
        juce::StringArray words;
        juce::String word;
        bool toontrackCode = false;
        for (auto folder = item.file.getParentDirectory(); folder.getFileName().isNotEmpty() && folder != folder.getParentDirectory();
             folder = folder.getParentDirectory())
        {
            const auto name = folder.getFileName();
            // Toontrack's folders are numbered "000329@21st_Century_Metal", "200-S125@Bridge".
            if (const auto at = name.indexOfChar ('@'); at > 0 && name.substring (0, at).containsOnly ("0123456789-SsTt"))
                toontrackCode = true;
        }
        if (toontrackCode)
            return true;
        const auto wordsIn = [] (const juce::String& text)
        {
            juce::StringArray found;
            juce::String w;
            for (auto c : text.toLowerCase() + " ")
            {
                if (juce::CharacterFunctions::isLetterOrDigit (c))
                    w += juce::String::charToString (c);
                else if (w.isNotEmpty())
                {
                    found.add (w);
                    w.clear();
                }
            }
            return found;
        };
        const auto own = wordsIn (item.file.getFileNameWithoutExtension());
        // Drum words in the file's own name, all of them.
        const auto drumNames = [] (const juce::StringArray& w)
        {
            return says (w, { "drum", "drums", "perc", "percussion", "kick", "snare", "hat", "hats", "hihat", "cymbal", "toms",
                              "toontrack", "ezx", "sdx", "ezdrummer", "superior", "drummer", "bfd" });
        };
        if (drumNames (own))   // "Bass Drum.mid": a drum, whatever else it says
            return true;
        // (0.227.2) A file whose own name says it's a melodic part (the owner's synth MIDI, played as guitar once it's
        // moved by octaves) is never left out by its folders: "Drum & Bass Kit/Bass Line.mid", "Synthwave Beats/Lead.mid".
        // Its notes are still checked as it's read.
        if (says (own, { "synth", "lead", "leads", "bass", "bassline", "sub", "chord", "chords", "arp", "arps", "pad", "pads",
                         "pluck", "plucks", "keys", "piano", "melody", "melodies", "riff", "riffs", "guitar", "gtr",
                         "line", "hook", "seq", "sequence", "stab", "stabs", "bell", "bells", "string", "strings", "brass" }))
            return false;
        if (says (own, { "kit", "groove", "grooves", "beat", "beats", "fill", "fills" }))
            return true;
        // In the folders only the words that name drums outright: sample packs call themselves "... Kit" or
        // "... Beats" and hold synth MIDI beside their drums.
        return drumNames (wordsIn (item.file.getParentDirectory().getFullPathName()));
    }

    bool notesLookLikeDrums (const std::vector<Note>& notes)
    {
        if (notes.size() < 8)
            return false;
        // The kit notes of General MIDI and Toontrack's layout: kicks, snares, hats, toms, cymbals.
        const auto kit = [] (int p) { return p >= 21 && p <= 59; };
        const auto isKick = [] (int p) { return p == 35 || p == 36; };   // (not 33/34: a drop-A guitar's low notes)
        const auto isSnare = [] (int p) { return p == 38 || p == 40 || p == 37 || p == 39; };
        const auto isTop = [] (int p) { return p == 42 || p == 44 || p == 46 || (p >= 21 && p <= 26) || p == 49 || p == 51
                                             || p == 52 || p == 53 || p == 55 || p == 57 || p == 59; };
        int onKit = 0, kicks = 0, snares = 0, tops = 0, kicksWithTop = 0;
        for (const auto& n : notes)
        {
            onKit += kit (n.pitch) ? 1 : 0;
            snares += isSnare (n.pitch) ? 1 : 0;
            tops += isTop (n.pitch) ? 1 : 0;
            if (isKick (n.pitch))
            {
                ++kicks;
                for (const auto& m : notes)
                    if (isTop (m.pitch) && std::abs (m.start - n.start) < 0.02)
                    {
                        ++kicksWithTop;
                        break;
                    }
            }
        }
        return kicks > 0 && snares > 0 && tops * 5 >= (int) notes.size() && onKit >= (int) notes.size() * 3 / 4 && kicksWithTop * 4 >= kicks;
    }

    bool isKnownDrumFile (const juce::File& file, const RiffFindRequest& request)
    {
        const auto path = file.getFullPathName();
        if (std::binary_search (request.drumFiles.begin(), request.drumFiles.end(), path))
            return true;
        for (const auto& folder : request.drumFolders)
            if (file.isAChildOf (folder))
                return true;
        return false;
    }

    std::optional<Key> keyFromText (const juce::String& text)
    {
        const auto t = text.trim();
        if (t.isEmpty())
            return std::nullopt;
        const auto letter = juce::String ("CDEFGAB").indexOfChar (juce::CharacterFunctions::toUpperCase (t[0]));
        if (letter < 0)
            return std::nullopt;
        int i = 1, accidental = 0;
        if (t[i] == '#')
        {
            accidental = 1;
            ++i;
        }
        else if (t[i] == 'b')
        {
            accidental = -1;
            ++i;
        }
        const auto rest = t.substring (i).trim().toLowerCase();
        const bool minor = rest.startsWith ("m") && ! rest.startsWith ("maj");
        const auto pc = ((Key::naturalOf (letter) + accidental) % 12 + 12) % 12;
        for (const auto& k : Key::all())
            if (k.minor == minor && k.letter == letter && k.accidental == accidental)
                return k;
        return Key::fromPitchClass (pc, minor, accidental < 0);
    }

    double riffQualityThreshold (int level)
    {
        return level <= 0 ? 0.0 : level == 1 ? 0.55 : 0.7;
    }

    RiffQuality riffQuality (const std::vector<LineNote>& all, int bars, double beatsPerBar)
    {
        RiffQuality q;
        std::vector<LineNote> notes;
        for (const auto& n : all)
            if (! n.keyswitch)
                notes.push_back (n);
        if (notes.empty())
        {
            q.problems.add ("no notes");
            return q;
        }
        std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        const auto bpb = juce::jmax (0.25, beatsPerBar);
        bars = juce::jmax (1, bars);

        // Notes starting together are one hit.
        struct Hit { double beat, longest; int low, high, count; };
        std::vector<Hit> hits;
        for (const auto& n : notes)
        {
            if (! hits.empty() && n.beat - hits.back().beat < 0.04)
            {
                auto& h = hits.back();
                h.longest = juce::jmax (h.longest, n.length);
                h.low = juce::jmin (h.low, n.pitch);
                h.high = juce::jmax (h.high, n.pitch);
                ++h.count;
            }
            else
                hits.push_back ({ n.beat, n.length, n.pitch, n.pitch, 1 });
        }
        const auto clamp01 = [] (double v) { return juce::jlimit (0.0, 1.0, v); };
        const auto count = (double) hits.size();

        q.hitsPerBar = count / bars;
        q.hits = q.hitsPerBar <= 16.0 ? clamp01 ((q.hitsPerBar - 1.0) / 2.0) : clamp01 (1.0 - (q.hitsPerBar - 16.0) / 16.0);

        // Places in the bar, on a sixteenth grid; and each bar's own set for the repeats.
        std::set<int> places;
        std::vector<std::set<int>> barPlaces ((size_t) bars);
        for (const auto& h : hits)
        {
            const auto bar = juce::jlimit (0, bars - 1, (int) std::floor (h.beat / bpb + 1e-6));
            const auto place = juce::roundToInt ((h.beat - bar * bpb) / 0.25);
            places.insert (place);
            barPlaces[(size_t) bar].insert (place);
        }
        q.rhythm = clamp01 (((double) places.size() - 1.0) / 4.0);

        int held = 0, stacked = 0;
        for (const auto& h : hits)
        {
            held += h.longest >= 2.0 ? 1 : 0;
            stacked += (h.count >= 4 || h.high - h.low > 19) ? 1 : 0;
        }
        q.held = 1.0 - clamp01 ((held / count - 0.25) / 0.5);
        q.stacks = 1.0 - clamp01 (1.5 * stacked / count);

        int quick = 0, jumps = 0;
        for (size_t i = 1; i < hits.size(); ++i)
            if (hits[i].beat - hits[i - 1].beat < 0.5)
            {
                ++quick;
                jumps += std::abs (hits[i].low - hits[i - 1].low) > 12 ? 1 : 0;
            }
        q.leaps = quick > 0 ? 1.0 - clamp01 (2.0 * jumps / quick) : 1.0;

        int filledBars = 0;
        for (const auto& b : barPlaces)
            filledBars += b.empty() ? 0 : 1;
        const auto filledShare = (double) filledBars / bars;
        q.filled = filledShare * filledShare;

        if (bars == 1)
            q.repeats = 1.0;   // a one-bar riff repeats itself
        else
        {
            double sum = 0.0;
            int counted = 0;
            for (size_t a = 0; a < barPlaces.size(); ++a)
            {
                if (barPlaces[a].empty())
                    continue;
                double best = 0.0;
                for (size_t b = 0; b < barPlaces.size(); ++b)
                {
                    if (a == b || barPlaces[b].empty())
                        continue;
                    int both = 0;
                    for (const auto p : barPlaces[a])
                        both += barPlaces[b].count (p) > 0 ? 1 : 0;
                    const auto either = (int) barPlaces[a].size() + (int) barPlaces[b].size() - both;
                    best = juce::jmax (best, either > 0 ? (double) both / either : 0.0);
                }
                sum += best;
                ++counted;
            }
            q.repeats = counted > 0 ? clamp01 (1.4 * sum / counted) : 0.0;
        }

        int lowHits = 0;
        for (const auto& h : hits)
            lowHits += h.low < 64 ? 1 : 0;
        q.low = clamp01 (1.5 * lowHits / count);

        // The weighted mix, pulled down by the worst of the ones a guitar can't get round (too few hits, held
        // pads, big stacks, leaps round the keyboard): one of those alone is enough to make it no riff.
        const auto mix = 0.15 * q.hits + 0.1 * q.rhythm + 0.15 * q.held + 0.15 * q.stacks + 0.15 * q.leaps
                       + 0.15 * q.repeats + 0.1 * q.filled + 0.05 * q.low;
        const auto worst = juce::jmin (juce::jmin (q.hits, q.held), juce::jmin (q.stacks, q.leaps));
        q.score = mix * (0.6 + 0.4 * worst);

        if (q.hits < 0.5)
            q.problems.add (q.hitsPerBar > 16.0 ? juce::String ("too many notes to pick")
                                                 : "only " + juce::String (q.hitsPerBar, 1) + " hits a bar");
        if (q.rhythm < 0.5)
            q.problems.add ("little rhythm");
        if (q.held < 0.5)
            q.problems.add ("mostly held notes");
        if (q.stacks < 0.5)
            q.problems.add ("big stacked chords");
        if (q.leaps < 0.5)
            q.problems.add ("jumps round the keyboard");
        if (q.repeats < 0.4)
            q.problems.add ("no repeating shape");
        if (q.filled < 0.7)
            q.problems.add ("empty bars");
        if (q.low < 0.4)
            q.problems.add ("high up the neck");
        return q;
    }

    bool looksLikePad (const std::vector<LineNote>& all, double beatsPerBar)
    {
        std::vector<LineNote> notes;
        for (const auto& n : all)
            if (! n.keyswitch)
                notes.push_back (n);
        if (notes.size() < 3)
            return false;
        std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        struct Hit { double beat, longest; int count; };
        std::vector<Hit> hits;
        for (const auto& n : notes)
            if (! hits.empty() && n.beat - hits.back().beat < 0.04)
            {
                hits.back().longest = juce::jmax (hits.back().longest, n.length);
                ++hits.back().count;
            }
            else
                hits.push_back ({ n.beat, n.length, 1 });
        double padTime = 0.0, allTime = 0.0;
        for (const auto& h : hits)
        {
            allTime += h.longest;
            if (h.count >= 3 && h.longest >= 1.0)
                padTime += h.longest;
        }
        const auto bpb = juce::jmax (0.25, beatsPerBar);
        const auto span = juce::jmax (bpb, notes.back().beat + notes.back().length - std::floor (notes.front().beat / bpb) * bpb);
        const auto perBar = hits.size() / juce::jmax (1.0, span / bpb);
        return allTime > 0.0 && padTime / allTime >= 0.6 && perBar <= 4.0;
    }

    std::vector<LineNote> padAsGuitar (const std::vector<LineNote>& all, double beatsPerBar, double totalBeats, double* readShare)
    {
        std::vector<LineNote> notes;
        for (const auto& n : all)
            if (! n.keyswitch)
                notes.push_back (n);
        std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        const auto bpb = juce::jmax (0.25, beatsPerBar);

        // Each change: the notes starting together, and those still sounding from before.
        struct Change { double beat; int root; bool diminished; float velocity; bool read; };
        std::vector<Change> changes;
        for (size_t i = 0; i < notes.size();)
        {
            auto j = i;
            std::vector<int> pitches;
            float velocity = 0.0f;
            while (j < notes.size() && notes[j].beat - notes[i].beat < 0.04)
            {
                pitches.push_back (notes[j].pitch);
                velocity = juce::jmax (velocity, notes[j].velocity);
                ++j;
            }
            for (size_t k = 0; k < i; ++k)
                if (notes[k].beat + notes[k].length > notes[i].beat + 0.1)
                    pitches.push_back (notes[k].pitch);
            const auto chord = chordOfNotes (pitches, true);
            const auto lowest = *std::min_element (pitches.begin(), pitches.end());
            Change c { notes[i].beat, chord.has_value() ? chord->root : ((lowest % 12) + 12) % 12,
                       chord.has_value() && chord->type == Chord::Type::diminished, velocity, chord.has_value() };
            if (changes.empty() || changes.back().root != c.root || changes.back().diminished != c.diminished)
                changes.push_back (c);
            i = j;
        }
        int read = 0;
        for (const auto& c : changes)
            read += c.read ? 1 : 0;
        if (readShare != nullptr)
            *readShare = changes.empty() ? 0.0 : (double) read / (double) changes.size();

        std::vector<LineNote> out;
        const auto end = juce::jmax (totalBeats, notes.empty() ? 0.0 : notes.back().beat + notes.back().length);
        for (size_t c = 0; c < changes.size(); ++c)
        {
            const auto from = changes[c].beat;
            const auto to = c + 1 < changes.size() ? changes[c + 1].beat : end;
            const auto root = 33 + ((changes[c].root - 9) % 12 + 12) % 12;   // A1 (33) up to G#2
            const auto velocity = juce::jlimit (0.6f, 0.86f, 0.55f + 0.35f * changes[c].velocity);
            // Struck at the change, then on every half bar line while it holds.
            std::vector<double> strikes { from };
            const auto half = bpb / 2.0;
            for (auto b = (std::floor (from / half + 1e-6) + 1.0) * half; b < to - 0.25; b += half)
                strikes.push_back (b);
            for (size_t s = 0; s < strikes.size(); ++s)
            {
                const auto next = s + 1 < strikes.size() ? strikes[s + 1] : to;
                const auto length = juce::jmax (0.1, (next - strikes[s]) * 0.96);
                for (const auto offset : { 0, changes[c].diminished ? 6 : 7, 12 })
                {
                    LineNote n;
                    n.beat = strikes[s];
                    n.length = length;
                    n.pitch = root + offset;
                    n.velocity = s == 0 ? velocity : velocity * 0.92f;
                    out.push_back (n);
                }
            }
        }
        return out;
    }

    std::optional<RiffFind> scoreRiff (const MediaItem& item, const MidiImport& midi, const RiffFindRequest& request)
    {
        std::vector<Note> notes;
        for (const auto& t : midi.tracks)
            if (t.channel != 10)
                notes.insert (notes.end(), t.notes.begin(), t.notes.end());
        notes.erase (std::remove_if (notes.begin(), notes.end(), [] (const Note& n) { return n.muted; }), notes.end());
        if (notes.size() < 3)
            return std::nullopt;
        std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
        if (notesLookLikeDrums (notes))
            return std::nullopt;   // a drum groove written on channel 1 (most packs' are)
        const auto bpb = juce::jmax (0.25, request.beatsPerBar);
        const auto firstBar = std::floor (notes.front().start / bpb + 1e-6) * bpb;
        double last = 0.0;
        for (const auto& n : notes)
            last = juce::jmax (last, n.start + n.length);
        if (last - firstBar > 16.0 * bpb + 0.25)
            return std::nullopt;   // a song, not a riff

        RiffFind found;
        found.file = item.file;
        found.name = item.file.getFileNameWithoutExtension();
        found.folder = item.file.getParentDirectory().getFileName();
        found.riff = riffFromNotes (notes, 0.0, bpb, found.name, request.section);
        if (found.riff.notes.empty())
            return std::nullopt;
        found.pad = looksLikePad (found.riff.notes, bpb);

        if (const auto known = keyFromText (item.keyName()); known.has_value())
        {
            found.from = *known;
            found.keyKnown = true;
        }
        else
            found.from = guessKey (found.riff.notes);
        // Into the song's key in the riff's own mode: a major riff going into a minor key goes to that key's
        // relative major (the same notes), so it keeps its sound (C major into A minor stays as it is).
        auto target = request.key;
        if (found.from.minor != request.key.minor)
            target = Key::fromPitchClass ((request.key.pitchClass() + (request.key.minor ? 3 : 9)) % 12, found.from.minor, request.key.signature() < 0);
        found.shift = keyShiftOf (found.from, target);

        // Moved into the key, then by whole octaves to sit best in a guitar's range.
        for (auto& n : found.riff.notes)
            if (! n.keyswitch)
                n.pitch = movePitchToKey (n.pitch, found.from, target);
        // (0.230.0) A pad: its chords (read in the song's key) as a guitar's ringing power chords.
        double padRead = 0.0;
        if (found.pad)
            found.riff.notes = padAsGuitar (found.riff.notes, bpb, found.riff.bars * bpb, &padRead);
        const auto inRange = [&] (int octaves)
        {
            double in = 0.0, all = 0.0;
            for (const auto& n : found.riff.notes)
            {
                const auto w = 0.25 + n.length;
                all += w;
                const auto p = n.pitch + 12 * octaves;
                if (p >= guitarLowest && p <= guitarHighest)
                    in += w;
            }
            return all > 0.0 ? in / all : 0.0;
        };
        // (0.228.0) Of the octaves that fit about as well, one whose middle note sits in a riff's register (C1 to C3,
        // the low strings), so a synth line from up the keyboard comes down to where a guitar riffs; then the fewest.
        std::vector<int> pitches;
        for (const auto& n : found.riff.notes)
            if (! n.keyswitch)
                pitches.push_back (n.pitch);
        std::sort (pitches.begin(), pitches.end());
        const auto middle = pitches.empty() ? 50 : pitches[pitches.size() / 2];
        double bestFit = -1.0;
        for (int o = -4; o <= 4; ++o)
            bestFit = juce::jmax (bestFit, inRange (o));
        int octaves = 0;
        int nearest = 1000;
        for (int o = -4; o <= 4; ++o)
        {
            if (inRange (o) < bestFit - 0.03)
                continue;
            const auto m = middle + 12 * o;
            const auto distance = (m > 60 ? m - 60 : m < 36 ? 36 - m : 0) * 10 + std::abs (o);   // ties: the fewest octaves
            if (distance < nearest)
            {
                nearest = distance;
                octaves = o;
            }
        }
        for (auto& n : found.riff.notes)
            if (! n.keyswitch)
                n.pitch = juce::jlimit (0, 127, n.pitch + 12 * octaves);
        found.playable = inRange (0);

        double in = 0.0, all = 0.0;
        for (const auto& n : found.riff.notes)
        {
            const auto w = (0.25 + n.length) * (0.5 + n.velocity);
            all += w;
            if (isPitchClassInKey (((n.pitch % 12) + 12) % 12, request.key))
                in += w;
        }
        found.inKey = all > 0.0 ? in / all : 0.0;

        const auto words = wordsOf (item.file);
        found.tempo = item.tempo();
        const auto tempo = tempoFit (found.tempo, request.bpm);
        const auto section = wordFit (words, (int) request.section);
        auto quality = riffQuality (found.riff.notes, found.riff.bars, bpb);
        if (found.pad)
        {
            // Held chords would never pass as a riff: judged by how well its chords could be read instead.
            quality.score = 0.55 + 0.3 * padRead;
            quality.problems.clear();
            quality.problems.add ("PAD: played as chords");
            if (padRead < 0.7)
                quality.problems.add ("some chords a best guess");
        }
        found.quality = quality.score;
        found.score = 0.35 * quality.score + 0.3 * found.inKey + 0.1 * found.playable + 0.1 * tempo + 0.1 * section
                    + (riffWords (words) ? 0.05 : 0.0);

        juce::StringArray why;
        why.add ("riff " + juce::String (juce::roundToInt (quality.score * 100.0)) + "%");
        why.add ("in key " + juce::String (juce::roundToInt (found.inKey * 100.0)) + "%");
        why.add (found.shift == 0 ? juce::String ("already in ") + request.key.name()
                                  : "MOVED " + juce::String (found.shift > 0 ? "+" : "") + juce::String (found.shift)
                                        + " (from " + found.from.name() + (found.keyKnown ? "" : ", guessed") + ")");
        why.add (juce::String (found.riff.bars) + (found.riff.bars == 1 ? " bar" : " bars"));
        if (found.tempo > 0.0)
            why.add (juce::String (juce::roundToInt (found.tempo)) + " BPM");
        if (saysSection (words, (int) request.section))
            why.add ("says " + sectionWord (request.section).toLowerCase());
        if (found.playable < 0.9)
            why.add (juce::String (juce::roundToInt ((1.0 - found.playable) * 100.0)) + "% out of a guitar's range");
        for (const auto& p : quality.problems)
            why.add (p);
        found.reason = why.joinIntoString (", ");
        return found;
    }

    std::vector<RiffFind> findRiffs (const std::vector<MediaItem>& items, const RiffFindRequest& request,
                                     const std::function<bool()>& stop, std::atomic<int>* read)
    {
        const auto bpb = juce::jmax (0.25, request.beatsPerBar);
        std::vector<std::pair<double, const MediaItem*>> likely;
        for (const auto& item : items)
        {
            if (item.type != MediaType::midi || item.notes < 3 || looksLikeDrums (item) || isKnownDrumFile (item.file, request))
                continue;
            if (std::binary_search (request.hidden.begin(), request.hidden.end(), item.file.getFullPathName()))
                continue;   // (0.228.0) one you've hidden
            if (item.beats > 0.0 && item.beats > 17.0 * bpb + 1.0)
                continue;   // longer than 16 bars (and the bar the first note may start in)
            likely.push_back ({ promise (item, request), &item });
        }
        std::stable_sort (likely.begin(), likely.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });
        if ((int) likely.size() > request.mostRead)
            likely.resize ((size_t) juce::jmax (0, request.mostRead));

        std::vector<RiffFind> found;
        for (const auto& [_, item] : likely)
        {
            if (stop && stop())
                break;
            juce::String error;
            if (const auto midi = readMidiFile (item->file, error); midi.has_value())
                if (auto riff = scoreRiff (*item, *midi, request); riff.has_value() && riff->quality >= request.leastQuality)
                    found.push_back (std::move (*riff));
            if (read != nullptr)
                ++*read;
        }
        std::stable_sort (found.begin(), found.end(), [] (const RiffFind& a, const RiffFind& b) { return a.score > b.score; });
        if ((int) found.size() > request.most)
            found.resize ((size_t) juce::jmax (0, request.most));
        return found;
    }
}
