#include "engine/RiffFinder.h"

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
        for (auto c : item.file.getFullPathName().toLowerCase() + " ")
        {
            if (juce::CharacterFunctions::isLetterOrDigit (c))
                word += juce::String::charToString (c);
            else if (word.isNotEmpty())
            {
                words.add (word);
                word.clear();
            }
        }
        return says (words, { "drum", "drums", "kit", "groove", "grooves", "beat", "beats", "perc", "percussion",
                              "fill", "fills", "kick", "snare", "hat", "hats", "cymbal", "toms", "toontrack", "ezx", "sdx",
                              "ezdrummer", "superior", "drummer", "bfd" });
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
        int octaves = 0;
        double best = -1.0;
        for (int o = -4; o <= 4; ++o)
        {
            const auto fit = inRange (o) - 0.001 * std::abs (o);   // ties: the fewest octaves
            if (fit > best)
            {
                best = fit;
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
        found.score = 0.45 * found.inKey + 0.2 * found.playable + 0.15 * tempo + 0.15 * section + (riffWords (words) ? 0.05 : 0.0);

        juce::StringArray why;
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
                if (auto riff = scoreRiff (*item, *midi, request); riff.has_value())
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
