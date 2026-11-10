#include "BandRecord.h"

#include "DrumMaps.h"
#include "DrumLearning.h"
#include "Markers.h"
#include "MidiImport.h"
#include "Studio.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include "VariAudio.h"

#include <map>

namespace pr7s
{
    namespace
    {
        constexpr double hop = 0.005;   // seconds between frames (the pitch analysis' own)

        std::vector<float> monoOf (const juce::AudioBuffer<float>& sound)
        {
            const auto length = sound.getNumSamples(), channels = sound.getNumChannels();
            std::vector<float> mono ((size_t) length, 0.0f);
            for (int c = 0; c < channels; ++c)
            {
                const auto* data = sound.getReadPointer (c);
                for (int i = 0; i < length; ++i)
                    mono[(size_t) i] += data[i] / (float) channels;
            }
            return mono;
        }

        // The level (dBFS, RMS) every 5 ms, over 25 ms round it (longer than a low string's cycle: a note's own
        // waves don't ripple it).
        std::vector<float> levelsOf (const std::vector<float>& mono, double rate)
        {
            const auto step = juce::jmax (1, (int) std::lround (rate * hop));
            const auto half = (long) std::lround (rate * 0.0125);
            std::vector<double> sums (mono.size() + 1, 0.0);
            for (size_t i = 0; i < mono.size(); ++i)
                sums[i + 1] = sums[i] + (double) mono[i] * mono[i];
            std::vector<float> levels (mono.size() / (size_t) step + 1, -120.0f);
            for (size_t f = 0; f < levels.size(); ++f)
            {
                const auto centre = (long) (f * (size_t) step);
                const auto from = (size_t) juce::jmax (0L, centre - half), to = (size_t) juce::jmin ((long) mono.size(), centre + half);
                if (to > from)
                    levels[f] = (float) (10.0 * std::log10 ((sums[to] - sums[from]) / (double) (to - from) + 1.0e-12));
            }
            return levels;
        }

        float percentile (std::vector<float> values, float p)
        {
            if (values.empty())
                return -120.0f;
            const auto at = values.begin() + (long) juce::jlimit<size_t> (0, values.size() - 1, (size_t) (p * (float) (values.size() - 1)));
            std::nth_element (values.begin(), at, values.end());
            return *at;
        }

        float medianOf (std::vector<float> values) { return percentile (std::move (values), 0.5f); }

        // How loud an attack was against the file's loud hits: 0.15 (24 dB under) to 1.
        float levelOf (float db, float loud) { return juce::jlimit (0.15f, 1.0f, 1.0f + (db - loud) / 24.0f * 0.85f); }

        // The size of a sine of `hz` in mono[from, to) (Goertzel, Hann-windowed), the window's own scale.
        double magnitudeAt (const std::vector<float>& mono, size_t from, size_t to, double hz, double rate)
        {
            const auto n = (double) (to - from);
            if (n < 8 || hz <= 0.0 || hz >= rate * 0.5)
                return 0.0;
            const auto w = juce::MathConstants<double>::twoPi * hz / rate;
            const auto coefficient = 2.0 * std::cos (w);
            double s1 = 0.0, s2 = 0.0;
            for (size_t i = from; i < to; ++i)
            {
                const auto hann = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) (i - from) / (n - 1.0));
                const auto s = mono[i] * hann + coefficient * s1 - s2;
                s2 = s1;
                s1 = s;
            }
            return std::sqrt (juce::jmax (0.0, s1 * s1 + s2 * s2 - coefficient * s1 * s2)) / n;
        }

        double hzOf (double midi) { return 440.0 * std::pow (2.0, (midi - 69.0) / 12.0); }

        // The pick attacks: where the level jumps at least `riseDb` within 10 ms over the quietest of the 20 ms
        // before, loud enough (within 40 dB of the loud hits), 40 ms apart. The level: the peak over the last
        // 40 ms, every millisecond (longer than a low A's cycle, so a note's own waves don't count as rises).
        // A pick always jumps (a re-picked note on a ringing string too, if it's played harder than it rings);
        // a note stopping never does.
        std::vector<double> attacksOf (const std::vector<float>& mono, double rate, float riseDb = 4.0f)
        {
            const auto step = juce::jmax (1, (int) std::lround (rate * 0.001));
            std::vector<float> peaks (mono.size() / (size_t) step + 1, 0.0f);
            for (size_t f = 0; f < peaks.size(); ++f)
                for (size_t i = f * (size_t) step; i < juce::jmin (mono.size(), (f + 1) * (size_t) step); ++i)
                    peaks[f] = juce::jmax (peaks[f], std::abs (mono[i]));
            std::vector<float> fine (peaks.size(), -120.0f);
            for (size_t f = 0; f < peaks.size(); ++f)
            {
                auto held = 0.0f;
                for (size_t k = f >= 39 ? f - 39 : 0; k <= f; ++k)
                    held = juce::jmax (held, peaks[k]);
                fine[f] = 20.0f * std::log10 (held + 1.0e-9f);
            }
            std::vector<float> audible;
            for (const auto l : fine)
                if (l > -80.0f)
                    audible.push_back (l);
            const auto loud = percentile (audible, 0.95f);
            std::vector<double> attacks;
            auto last = -1.0;
            for (int f = 0; f + 10 < (int) fine.size(); ++f)
            {
                // (Before the file's start: silence. A song whose first hit is on its first sample still has it.)
                auto before = f < 20 ? -120.0f : fine[(size_t) f];
                for (int k = juce::jmax (0, f - 20); k < f; ++k)
                    before = juce::jmin (before, fine[(size_t) k]);
                auto after = -120.0f;
                for (int k = f; k <= f + 10; ++k)
                    after = juce::jmax (after, fine[(size_t) k]);
                if (after - before >= riseDb && after > loud - 40.0f && fine[(size_t) f + 1] - (f > 0 ? fine[(size_t) f - 1] : -120.0f) > 1.0f)
                {
                    const auto t = f * 0.001;
                    if (last < 0.0 || t - last >= 0.04)
                    {
                        // At the steepest rise of the next 10 ms.
                        auto best = f;
                        for (int k = f; k < f + 10; ++k)
                            if (fine[(size_t) k + 1] - fine[(size_t) k] > fine[(size_t) best + 1] - fine[(size_t) best])
                                best = k;
                        attacks.push_back (best * 0.001);
                        last = best * 0.001;
                        f = best + 30;
                    }
                }
            }
            return attacks;
        }

        bool attackNear (const std::vector<double>& attacks, double seconds, double within)
        {
            const auto it = std::lower_bound (attacks.begin(), attacks.end(), seconds - within);
            return it != attacks.end() && *it <= seconds + within;
        }
    }

    const std::vector<BandRole>& bandRoles()
    {
        static const std::vector<BandRole> roles { BandRole::drums, BandRole::rhythm, BandRole::lead, BandRole::bass, BandRole::keys };
        return roles;
    }

    juce::String bandRoleName (BandRole role)
    {
        switch (role)
        {
            case BandRole::drums:   return "DRUMS";
            case BandRole::rhythm:  return "RHYTHM GUITAR";
            case BandRole::lead:    return "LEAD GUITAR";
            case BandRole::bass:    return "BASS";
            case BandRole::keys:    return "KEYS";
            case BandRole::unknown: break;
        }
        return "NOT USED";
    }

    namespace
    {
        juce::StringArray wordsOf (const juce::String& name)
        {
            juce::StringArray words;
            juce::String word;
            for (const auto c : name.toLowerCase())
            {
                if (juce::CharacterFunctions::isLetterOrDigit (c))
                    word << juce::String::charToString (c);
                else if (word.isNotEmpty())
                {
                    words.add (word);
                    word.clear();
                }
            }
            if (word.isNotEmpty())
                words.add (word);
            // "gtr1" -> "gtr", "1"
            juce::StringArray split;
            for (const auto& w : words)
            {
                auto letters = w.trimCharactersAtEnd ("0123456789");
                if (letters.isNotEmpty() && letters.length() < w.length() && ! letters.containsOnly ("0123456789"))
                {
                    split.add (letters);
                    split.add (w.substring (letters.length()));
                }
                else
                    split.add (w);
            }
            return split;
        }

        bool isMidi (const juce::File& f) { return f.hasFileExtension ("mid;midi"); }
        bool isAudio (const juce::File& f) { return f.hasFileExtension ("wav;wave;aif;aiff;flac"); }

        const juce::StringArray roleWords { "bass", "bs", "lead", "ld", "solo", "solos", "rhythm", "rhy", "rtm", "rhythmguitar",
                                            "gtr", "guitar", "guitars", "gt", "di", "dis", "drums", "drum", "dr", "kit", "midi", "mid",
                                            "keys", "key", "keyboard", "keyboards", "kb", "kbd", "kbds", "piano", "pno", "synth", "synths",
                                            "pad", "pads", "organ", "rhodes", "strings", "keytar" };
        const juce::StringArray keysWords { "keys", "key", "keyboard", "keyboards", "kb", "kbd", "kbds", "piano", "pno", "synth", "synths",
                                            "pad", "pads", "organ", "rhodes", "strings", "keytar", "ep" };

        // A file's song: its name without the role words (and the number after "gtr" / "guitar").
        juce::String songKeyOf (const juce::File& f)
        {
            const auto words = wordsOf (f.getFileNameWithoutExtension());
            juce::StringArray kept;
            for (int i = 0; i < words.size(); ++i)
            {
                if (roleWords.contains (words[i]))
                {
                    if ((words[i] == "gtr" || words[i] == "guitar" || words[i] == "gt") && i + 1 < words.size() && words[i + 1].containsOnly ("0123456789"))
                        ++i;
                    continue;
                }
                kept.add (words[i]);
            }
            return kept.joinIntoString (" ");
        }
    }

    BandRole guessBandRole (const juce::File& file)
    {
        const auto words = wordsOf (file.getFileNameWithoutExtension());
        if (isMidi (file))
        {
            for (const auto& w : words)
                if (keysWords.contains (w))
                    return BandRole::keys;
            return BandRole::drums;
        }
        const auto has = [&] (std::initializer_list<const char*> list)
        {
            for (const auto* w : list)
                if (words.contains (w))
                    return true;
            return false;
        };
        if (has ({ "bass", "bs" }))
            return BandRole::bass;
        if (has ({ "lead", "ld", "solo", "solos" }))
            return BandRole::lead;
        if (has ({ "rhythm", "rhy", "rtm", "rhythmguitar" }))
            return BandRole::rhythm;
        for (int i = 0; i + 1 < words.size(); ++i)
            if (words[i] == "gtr" || words[i] == "guitar" || words[i] == "gt")
            {
                if (words[i + 1] == "1")
                    return BandRole::rhythm;
                if (words[i + 1] == "2")
                    return BandRole::lead;
            }
        return BandRole::unknown;
    }

    juce::File BandSong::fileOf (BandRole role) const
    {
        if (role == BandRole::drums)
            return drums;
        for (const auto& [file, r] : dis)
            if (r == role)
                return file;
        return {};
    }

    std::vector<BandSong> scanBandFolder (const juce::File& folder)
    {
        std::vector<BandSong> songs;
        const auto addFiles = [&] (BandSong& song, const juce::Array<juce::File>& files)
        {
            for (const auto& f : files)
            {
                if (isMidi (f) && guessBandRole (f) == BandRole::keys)
                    song.dis.push_back ({ f, BandRole::keys });
                else if (isMidi (f) && song.drums == juce::File())
                    song.drums = f;
                else if (isMidi (f))
                    song.dis.push_back ({ f, BandRole::unknown });   // (another MIDI file: the page asks)
                else if (isAudio (f))
                    song.dis.push_back ({ f, guessBandRole (f) });
            }
        };
        const auto readTempo = [] (BandSong& song)
        {
            if (song.drums == juce::File())
                return;
            juce::String error;
            if (const auto midi = readMidiFile (song.drums, error))
            {
                song.bpm = midi->bpm;
                song.tempoFromFile = ! midi->tempoChanges.empty();
                song.numerator = midi->numerator;
                song.denominator = midi->denominator;
            }
        };
        const auto sorted = [] (juce::Array<juce::File> files)
        {
            std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
                       { return a.getFileName().compareNatural (b.getFileName()) < 0; });
            return files;
        };
        for (const auto& sub : sorted (folder.findChildFiles (juce::File::findDirectories, false)))
        {
            auto files = sorted (sub.findChildFiles (juce::File::findFiles, false));
            files.removeIf ([] (const juce::File& f) { return ! isMidi (f) && ! isAudio (f); });
            if (files.isEmpty())
                continue;
            BandSong song;
            song.name = sub.getFileName();
            addFiles (song, files);
            readTempo (song);
            songs.push_back (std::move (song));
        }
        // Files loose in the folder: grouped by their song's name.
        std::map<juce::String, juce::Array<juce::File>> groups;
        std::vector<juce::String> order;
        for (const auto& f : sorted (folder.findChildFiles (juce::File::findFiles, false)))
            if (isMidi (f) || isAudio (f))
            {
                const auto key = songKeyOf (f);
                if (groups.find (key) == groups.end())
                    order.push_back (key);
                groups[key].add (f);
            }
        for (const auto& key : order)
        {
            BandSong song;
            const auto& files = groups[key];
            // Its name as written in the first file (the role words taken out).
            song.name = key.isNotEmpty() ? key : files[0].getFileNameWithoutExtension();
            for (const auto& f : files)
                if (isMidi (f))
                {
                    auto words = wordsOf (f.getFileNameWithoutExtension());
                    song.name = f.getFileNameWithoutExtension();
                    break;
                }
            addFiles (song, files);
            readTempo (song);
            songs.push_back (std::move (song));
        }
        return songs;
    }


    //==========================================================================
    std::vector<float> riffChanges (const std::vector<LineNote>& riff, const SignatureMap& map, int bars)
    {
        std::vector<float> changes ((size_t) juce::jmax (0, bars), 0.0f);
        if (riff.empty() || bars < 3)
            return changes;
        // Each bar: the pitch classes it plays (by length) and where its notes start (16 places).
        struct Feel { std::array<float, 12> pcs {}; std::array<float, 16> onsets {}; bool empty = true; };
        std::vector<Feel> feel ((size_t) bars);
        for (const auto& n : riff)
        {
            if (n.keyswitch)
                continue;
            const auto b = map.barAt (n.beat);
            if (! juce::isPositiveAndBelow (b, bars))
                continue;
            auto& f = feel[(size_t) b];
            const auto start = map.beatOfBar (b), length = map.beatOfBar (b + 1) - start;
            f.pcs[(size_t) (((n.pitch % 12) + 12) % 12)] += (float) juce::jlimit (0.05, 1.0, n.length);
            f.onsets[(size_t) juce::jlimit (0, 15, (int) ((n.beat - start) / length * 16.0))] = 1.0f;
            f.empty = false;
        }
        for (auto& f : feel)
        {
            auto sum = 0.0f;
            for (const auto v : f.pcs) sum += v;
            if (sum > 0.0f)
                for (auto& v : f.pcs) v /= sum;
        }
        const auto distance = [] (const Feel& a, const Feel& b)
        {
            if (a.empty || b.empty)
                return a.empty == b.empty ? 0.0f : 2.0f;
            auto d = 0.0f, o = 0.0f;
            for (size_t i = 0; i < 12; ++i) d += std::abs (a.pcs[i] - b.pcs[i]);
            for (size_t i = 0; i < 16; ++i) o += std::abs (a.onsets[i] - b.onsets[i]);
            return d + o / 16.0f;
        };
        const auto mean = [&] (int from, int to)
        {
            Feel m;
            auto count = 0;
            for (auto i = from; i < to; ++i)
                if (! feel[(size_t) i].empty)
                {
                    for (size_t k = 0; k < 12; ++k) m.pcs[k] += feel[(size_t) i].pcs[k];
                    for (size_t k = 0; k < 16; ++k) m.onsets[k] += feel[(size_t) i].onsets[k];
                    ++count;
                }
            if (count > 0)
            {
                for (auto& v : m.pcs) v /= (float) count;
                for (auto& v : m.onsets) v /= (float) count;
                m.empty = false;
            }
            return m;
        };
        // A riff repeats every bar or two: the two bars before against the two after, less how much the bars
        // inside each side differ (a two-bar riff's halves aren't a change).
        for (auto b = 2; b + 2 <= bars; ++b)
        {
            const auto cross = distance (mean (b - 2, b), mean (b, b + 2));
            const auto within = 0.5f * (distance (feel[(size_t) b - 2], feel[(size_t) b - 1]) + distance (feel[(size_t) b], feel[(size_t) b + 1]));
            changes[(size_t) b] = juce::jlimit (0.0f, 1.5f, (cross - 0.5f * within) * 1.0f);
        }
        return changes;
    }

    std::vector<BandSection> findBandSections (const std::vector<Note>& drums, const SignatureMap& map,
                                               const std::vector<LineNote>& riff, const std::vector<LineNote>& lead)
    {
        auto song = drumSongFromNotes ("band", drums, 120.0, map);
        if (song.bars.empty())
            return {};
        const auto count = (int) song.bars.size();
        const auto changes = riffChanges (riff, map, count);
        auto found = findSections (song.bars, &changes);
        // Songs move in phrases: a cut within a bar of a four-bar line goes onto it, pieces under two bars join
        // the section before (the first: the one after).
        std::vector<int> cuts;
        for (size_t i = 1; i < found.size(); ++i)
        {
            auto c = found[i].firstBar;
            const auto phrase = juce::roundToInt (c / 4.0) * 4;
            if (std::abs (phrase - c) <= 1)
                c = phrase;
            if (c > 0 && c < count && (cuts.empty() || c > cuts.back()))
                cuts.push_back (c);
        }
        std::vector<DrumSongSection> sections;
        auto from = 0;
        for (const auto c : cuts)
        {
            if (c - from >= 2)
            {
                sections.push_back ({ from, c - from });
                from = c;
            }
        }
        if (count - from < 2 && ! sections.empty())
            sections.back().bars += count - from;
        else
            sections.push_back ({ from, count - from });
        nameSections (song.bars, sections);
        // Next to each other and playing alike (the same group, the same kind): one section, cut where it needn't be.
        for (size_t i = 1; i < sections.size();)
            if (sections[i].group >= 0 && sections[i].group == sections[i - 1].group && sections[i].kind == sections[i - 1].kind)
            {
                sections[i - 1].bars += sections[i].bars;
                sections.erase (sections.begin() + (long) i);
            }
            else
                ++i;
        const auto n = (int) sections.size();
        std::vector<juce::String> names;
        for (const auto& s : sections)
            switch (s.kind)
            {
                case DrumSection::intro:     names.push_back ("Intro"); break;
                case DrumSection::chorus:    names.push_back ("Chorus"); break;
                case DrumSection::bridge:    names.push_back ("Bridge"); break;
                case DrumSection::breakdown: names.push_back ("Breakdown"); break;
                case DrumSection::ending:    names.push_back ("Outro"); break;
                case DrumSection::solo:      names.push_back ("Solo"); break;
                case DrumSection::verse:
                default:                     names.push_back ("Verse"); break;
            }
        // The solo: where the lead plays far busier than it does elsewhere.
        if (! lead.empty())
        {
            std::vector<float> density;
            for (const auto& s : sections)
            {
                const auto from = map.beatOfBar (s.firstBar), to = map.beatOfBar (s.firstBar + s.bars);
                const auto notes = std::count_if (lead.begin(), lead.end(), [&] (const LineNote& l) { return ! l.keyswitch && l.beat >= from && l.beat < to; });
                density.push_back ((float) notes / (float) juce::jmax (1, s.bars));
            }
            auto sorted = density;
            std::sort (sorted.begin(), sorted.end());
            const auto median = sorted[sorted.size() / 2];
            for (auto i = 0; i < n; ++i)
                if (density[(size_t) i] >= 4.0f && density[(size_t) i] >= 2.0f * median + 1.0f && names[(size_t) i] != "Intro" && names[(size_t) i] != "Outro")
                    names[(size_t) i] = "Solo";
        }
        const auto introGroup = n > 0 && names[0] == "Intro" ? sections[0].group : -2;
        const auto plain = [&] (int i) { return (names[(size_t) i] == "Verse" || names[(size_t) i] == "Bridge") && sections[(size_t) i].bars <= 8; };
        // Plays like a verse found elsewhere in the song (then it is one).
        const auto verseGroup = [&] (int i)
        {
            for (auto j = 0; j < n; ++j)
                if (j != i && names[(size_t) j] == "Verse" && sections[(size_t) j].group == sections[(size_t) i].group)
                    return true;
            return false;
        };
        // A short part between a verse and a chorus, playing like neither: the pre-chorus; and every short part
        // that plays as one does.
        std::vector<int> preGroups;
        for (auto i = 1; i + 1 < n; ++i)
            if (plain (i) && names[(size_t) i + 1] == "Chorus" && names[(size_t) i - 1] == "Verse"
                && sections[(size_t) i].group != sections[(size_t) i + 1].group && sections[(size_t) i].group != sections[(size_t) i - 1].group
                && ! verseGroup (i))
                preGroups.push_back (sections[(size_t) i].group);
        for (auto i = 1; i + 1 < n; ++i)
            if (plain (i) && std::find (preGroups.begin(), preGroups.end(), sections[(size_t) i].group) != preGroups.end()
                && names[(size_t) i + 1] == "Chorus")
                names[(size_t) i] = "Pre-Chorus";
        // A short part after a chorus, playing like no verse, no pre-chorus and not what follows: a post-chorus (the
        // intro's groove come back: an interlude).
        for (auto i = 1; i + 1 < n; ++i)
        {
            const auto& sec = sections[(size_t) i];
            if (plain (i) && names[(size_t) i - 1] == "Chorus" && sec.group != sections[(size_t) i - 1].group
                && sec.group != sections[(size_t) i + 1].group && ! verseGroup (i)
                && std::find (preGroups.begin(), preGroups.end(), sec.group) == preGroups.end())
                names[(size_t) i] = sec.group == introGroup ? "Interlude" : "Post-Chorus";
        }
        // Next to each other under one name (two verse riffs back to back): one section.
        for (size_t i = 1; i < sections.size();)
            if (names[i] == names[i - 1])
            {
                sections[i - 1].bars += sections[i].bars;
                sections.erase (sections.begin() + (long) i);
                names.erase (names.begin() + (long) i);
            }
            else
                ++i;
        const auto kept = (int) sections.size();
        std::vector<Marker> markers;
        for (auto i = 0; i < kept; ++i)
        {
            Marker m;
            m.cycle = true;
            m.start = (double) sections[(size_t) i].firstBar;
            m.name = names[(size_t) i];
            markers.push_back (m);
        }
        numberSections (markers);
        std::vector<BandSection> named;
        for (auto i = 0; i < kept; ++i)
            named.push_back ({ sections[(size_t) i].firstBar, sections[(size_t) i].bars, markers[(size_t) i].name });
        return named;
    }

    //==========================================================================
    std::vector<PlayedNote> transcribeLine (const juce::AudioBuffer<float>& sound, double rate, BandRole role, int lowest, int highest)
    {
        std::vector<PlayedNote> notes;
        if (sound.getNumSamples() == 0 || rate <= 0.0)
            return notes;
        const auto bass = role == BandRole::bass;
        const auto lowHz = lowest > 0 ? (float) hzOf (lowest - 2.5) : (bass ? 25.0f : 50.0f);
        const auto highHz = highest > 0 ? (float) hzOf (highest + 1.5) : (bass ? 500.0f : 1600.0f);
        const auto analysis = analysePitch (sound, rate, lowHz, highHz);
        const auto& frames = analysis.frames;
        const auto count = (int) frames.size();
        const auto attacks = attacksOf (monoOf (sound), rate);
        std::vector<float> voicedLevels;
        for (const auto& f : frames)
            if (f.isVoiced())
                voicedLevels.push_back (f.levelDb);
        if (voicedLevels.empty())
            return notes;
        const auto loud = percentile (voicedLevels, 0.95f);
        std::vector<char> voiced ((size_t) count);
        for (int i = 0; i < count; ++i)
            voiced[(size_t) i] = frames[(size_t) i].isVoiced() && frames[(size_t) i].levelDb > loud - 40.0f;
        for (int i = 1; i + 1 < count; ++i)   // (a frame or two lost inside a note: bridged)
            if (! voiced[(size_t) i] && voiced[(size_t) i - 1])
            {
                auto j = i;
                while (j < count && ! voiced[(size_t) j] && j - i < 3)
                    ++j;
                if (j < count && voiced[(size_t) j] && j - i <= 2)
                    for (int k = i; k < j; ++k)
                        voiced[(size_t) k] = 1;
            }
        const auto pitch = [&] (int i)
        {
            if (frames[(size_t) i].isVoiced())
                return frames[(size_t) i].midi;
            for (int d = 1; d < 4; ++d)
            {
                if (i - d >= 0 && frames[(size_t) (i - d)].isVoiced()) return frames[(size_t) (i - d)].midi;
                if (i + d < count && frames[(size_t) (i + d)].isVoiced()) return frames[(size_t) (i + d)].midi;
            }
            return 0.0f;
        };
        const auto settled = [&] (int from, int to)   // the median pitch of frames [from, to)
        {
            std::vector<float> values;
            for (int i = juce::jmax (0, from); i < juce::jmin (count, to); ++i)
                if (voiced[(size_t) i])
                    values.push_back (pitch (i));
            return values.empty() ? 0.0f : medianOf (values);
        };

        // One note from frame `from` to `to` (exclusive), its curve read for bends and vibrato.
        const auto finish = [&] (int from, int to)
        {
            if ((to - from) * hop < 0.04)
                return;
            PlayedNote note;
            note.start = from * hop;
            note.end = to * hop;
            const auto skip = juce::jmin (2, (to - from) / 4);
            const auto base = settled (from + skip, from + skip + 8);
            if (base <= 0.0f)
                return;
            note.pitch = juce::roundToInt (base);
            auto peak = -120.0f;
            for (int i = from; i < juce::jmin (to, from + 8); ++i)
                peak = juce::jmax (peak, frames[(size_t) i].levelDb);
            note.level = levelOf (peak, loud);
            // Bent: how far above its own pitch the curve went and stayed for 30 ms.
            auto bend = 0.0f;
            for (int i = from; i + 6 <= to; ++i)
            {
                auto least = 100.0f;
                for (int k = i; k < i + 6; ++k)
                    least = juce::jmin (least, pitch (k) - (float) note.pitch);
                bend = juce::jmax (bend, least);
            }
            if (bend >= 0.6f)
                note.bend = std::round (bend * 2.0f) / 2.0f;
            // Vibrato: a wobble round its pitch (not a bend) at 3 to 9 Hz, at least a quarter of a second long.
            if (note.bend == 0.0f && (to - from) * hop >= 0.25)
            {
                std::vector<float> d;
                for (int i = from + skip; i < to; ++i)
                    d.push_back (pitch (i) - base);
                auto mean = 0.0f;
                for (const auto v : d) mean += v;
                mean /= (float) d.size();
                auto sq = 0.0f;
                int crossings = 0;
                for (size_t i = 0; i < d.size(); ++i)
                {
                    sq += (d[i] - mean) * (d[i] - mean);
                    if (i > 0 && ((d[i] - mean) > 0.0f) != ((d[i - 1] - mean) > 0.0f))
                        ++crossings;
                }
                const auto spread = std::sqrt (sq / (float) d.size());
                const auto hz = crossings / 2.0 / ((double) d.size() * hop);
                if (spread >= 0.12f && spread < 1.2f && hz >= 3.0 && hz <= 9.0)
                    note.vibrato = spread * 1.41f;
            }
            notes.push_back (note);
        };

        // Walk the voiced runs: a new note at an attack, or where the pitch jumps (a hammer-on or
        // pull-off: within 10 ms) or settles lower after a slide; a rise with no attack is a bend.
        for (int i = 0; i < count;)
        {
            if (! voiced[(size_t) i])
            {
                ++i;
                continue;
            }
            auto start = i;
            auto base = 0.0f;
            auto j = i + 1;
            for (; j < count && voiced[(size_t) j]; ++j)
            {
                if (j - start == 6)
                    base = settled (start + 1, start + 7);
                if (base <= 0.0f || j - start < 8)
                    continue;
                const auto now = pitch (j);
                const auto attack = attackNear (attacks, j * hop, 0.012) && j - start >= 8;
                const auto jump = std::abs (now - pitch (j - 2)) >= 0.8f && std::abs (settled (j, j + 6) - base) >= 0.8f;
                const auto slidDown = settled (j, j + 6) <= base - 0.8f && std::abs (pitch (j + 3) - pitch (j)) < 0.15f
                                      && std::abs (settled (j, j + 6) - std::round (settled (j, j + 6))) < 0.3f;
                if (attack || (jump && ! (now > base)) || (jump && now > base && std::abs (now - pitch (j - 1)) >= 0.7f) || slidDown)
                {
                    finish (start, j);
                    start = j;
                    base = 0.0f;
                }
            }
            finish (start, j);
            i = j;
        }
        // The starts onto the pick attacks: the pitch is only certain a window into a low note, so its first
        // voiced frame can be 20-40 ms off; the attack heard within 50 ms before (or 30 ms after) is when it was played.
        for (size_t n = 0; n < notes.size(); ++n)
        {
            auto& note = notes[n];
            const auto it = std::lower_bound (attacks.begin(), attacks.end(), note.start - 0.05);
            if (it != attacks.end() && *it <= note.start + 0.03 && *it < note.end - 0.02 && (n == 0 || *it > notes[n - 1].start + 0.02))
            {
                note.start = *it;
                if (n > 0)
                    notes[n - 1].end = juce::jmin (notes[n - 1].end, note.start);
            }
        }
        // Nothing under the low string: an octave slip goes up an octave, a half step or two under is the low
        // string itself (slack, or read flat); past the top fret an octave down.
        if (lowest > 0)
            for (auto& note : notes)
            {
                if (note.pitch < lowest && note.pitch >= lowest - 2)
                    note.pitch = lowest;
                while (note.pitch < lowest)
                    note.pitch += 12;
                while (highest > lowest && note.pitch > highest && note.pitch - 12 >= lowest)
                    note.pitch -= 12;
            }
        return notes;
    }

    //==========================================================================
    std::vector<PlayedNote> transcribeRhythm (const juce::AudioBuffer<float>& sound, double rate, int lowest, int highest)
    {
        std::vector<PlayedNote> notes;
        if (sound.getNumSamples() == 0 || rate <= 0.0)
            return notes;
        const auto mono = monoOf (sound);
        const auto levels = levelsOf (mono, rate);
        const auto attacks = attacksOf (mono, rate);
        if (attacks.empty())
            return notes;
        const auto analysis = analysePitch (sound, rate, 25.0f, 700.0f);
        std::vector<float> loudLevels;
        for (const auto l : levels)
            if (l > -70.0f)
                loudLevels.push_back (l);
        const auto loud = percentile (loudLevels, 0.95f);
        const auto frameOf = [&] (double seconds) { return juce::jlimit (0, (int) levels.size() - 1, (int) (seconds / hop)); };

        struct Hit { PlayedNote note; float decay = 0.0f, bright = 0.0f; };
        std::vector<Hit> hits;
        for (size_t a = 0; a < attacks.size(); ++a)
        {
            const auto start = attacks[a];
            const auto next = a + 1 < attacks.size() ? attacks[a + 1] : (double) mono.size() / rate;
            // The attack's peak (within 30 ms), then where it dies away (30 dB under) or the next hit.
            auto peakFrame = frameOf (start);
            for (int f = frameOf (start); f <= frameOf (start + 0.03); ++f)
                if (levels[(size_t) f] > levels[(size_t) peakFrame])
                    peakFrame = f;
            const auto peak = levels[(size_t) peakFrame];
            if (peak < loud - 36.0f)
                continue;   // (string noise, a scrape)
            auto end = next;
            for (int f = peakFrame; f < frameOf (next); ++f)
                if (levels[(size_t) f] < peak - 30.0f)
                {
                    end = f * hop;
                    break;
                }
            if (end - start < 0.03)
                continue;
            // How fast it dies (dB a millisecond over the next 60 ms, or what there is) and how bright it starts.
            const auto measureTo = juce::jmin (frameOf (end), peakFrame + 12);
            const auto decay = measureTo > peakFrame ? (peak - levels[(size_t) measureTo]) / (float) ((measureTo - peakFrame) * hop * 1000.0) : 0.0f;
            const auto from = (size_t) ((start + 0.005) * rate);
            const auto to = juce::jmin (mono.size(), (size_t) (juce::jmin (end, start + 0.25) * rate));
            const auto brightTo = juce::jmin (to, from + (size_t) (0.05 * rate));
            double whole = 0.0, edge = 0.0;
            for (size_t i = from + 1; i < brightTo; ++i)
            {
                whole += (double) mono[i] * mono[i];
                edge += (double) (mono[i] - mono[i - 1]) * (mono[i] - mono[i - 1]);
            }
            const auto bright = whole > 1.0e-12 ? (float) std::sqrt (edge / whole) : 0.0f;

            // The lowest note: candidates from the pitch found over the hit (a power chord's sounds as an octave
            // under its root), checked by its harmonics with its own fundamental required.
            std::vector<float> heard;
            for (int f = (int) ((start + 0.01) / hop); f < (int) (juce::jmin (end, start + 0.2) / hop); ++f)
                if (f >= 0 && f < (int) analysis.frames.size() && analysis.frames[(size_t) f].isVoiced())
                    heard.push_back (analysis.frames[(size_t) f].midi);
            std::vector<int> candidates;
            if (! heard.empty())
            {
                const auto y = juce::roundToInt (medianOf (heard));
                candidates = { y, y + 12, y + 7, y - 12, y + 24 };
            }
            else
                for (int m = juce::jmax (26, lowest); m <= 64; ++m)
                    candidates.push_back (m);
            auto best = -1;
            auto bestScore = 0.0;
            for (const auto m : candidates)
            {
                if (m < juce::jmax (21, lowest) || m > highest)
                    continue;   // (0.82.1: under the guitar's low string can't be played)
                const auto f0 = hzOf (m);
                const auto fundamental = magnitudeAt (mono, from, to, f0, rate);
                auto score = 0.0, strongest = 0.0;
                for (int h = 1; h <= 16 && h * f0 < 2500.0; ++h)
                {
                    const auto mag = magnitudeAt (mono, from, to, h * f0, rate);
                    strongest = juce::jmax (strongest, mag);
                    score += mag / std::sqrt ((double) h);
                }
                if (fundamental < 0.12 * strongest)
                    continue;   // (no fundamental: an octave under the real note)
                if (score > bestScore)
                {
                    bestScore = score;
                    best = m;
                }
            }
            if (best < 0)
                continue;
            Hit hit;
            hit.note.start = start;
            hit.note.end = end;
            hit.note.pitch = best;
            const auto root = magnitudeAt (mono, from, to, hzOf (best), rate);
            hit.note.powerChord = magnitudeAt (mono, from, to, hzOf (best + 7), rate) > 0.3 * root;
            hit.note.level = levelOf (peak, loud);
            hit.decay = decay;
            hit.bright = bright;
            hits.push_back (hit);
        }
        // Palm muted: dies away fast, or fairly fast and duller than this player's own hits.
        std::vector<float> brights;
        for (const auto& h : hits)
            brights.push_back (h.bright);
        const auto usualBright = medianOf (brights);
        for (auto& h : hits)
        {
            h.note.muted = h.decay >= 0.15f || (h.decay >= 0.08f && h.bright < 0.75f * usualBright);
            notes.push_back (h.note);
        }
        return notes;
    }

    //==========================================================================
    LineIdea playedIdea (const std::vector<PlayedNote>& played, double bpm, BandRole role, const GuitarVelocities& velocities)
    {
        LineIdea idea;
        idea.part = role == BandRole::bass ? LineIdea::Part::bass : role == BandRole::lead ? LineIdea::Part::lead : LineIdea::Part::rhythm;
        idea.style = "Played: " + bandRoleName (role).toLowerCase();
        const auto beatsPerSecond = bpm / 60.0;
        const auto guitar = role == BandRole::rhythm || role == BandRole::lead;
        const auto layered = [] (int low, int high, float level) { return juce::roundToInt ((float) low + ((float) high - (float) low) * level); };
        for (const auto& p : played)
        {
            LineNote note;
            note.beat = p.start * beatsPerSecond;
            note.length = juce::jmax (1.0 / 32.0, (p.end - p.start) * beatsPerSecond);
            note.pitch = p.pitch;
            if (guitar)
            {
                const auto midi = p.muted ? layered (velocities.muteTight, velocities.muteLoose, p.level)
                                          : layered (velocities.sustainSoft, velocities.sustainHard, p.level);
                note.velocity = (float) juce::jlimit (1, 127, midi) / 127.0f;
            }
            else
                note.velocity = p.level;
            if (p.bend > 0.0f)
            {
                note.effect = LineNote::Effect::bend;
                note.amount = p.bend;
            }
            else if (p.vibrato > 0.0f)
            {
                note.effect = LineNote::Effect::vibrato;
                note.amount = juce::jlimit (0.2f, 1.0f, p.vibrato);
            }
            idea.notes.push_back (note);
            if (p.powerChord)
            {
                auto fifth = note;
                fifth.pitch = p.pitch + 7;
                fifth.effect = LineNote::Effect::none;
                idea.notes.push_back (fifth);
            }
        }
        return idea;
    }

    //==========================================================================
    int matchedAt (const std::vector<double>& noteBeats, const std::vector<double>& onsets, double bpm, double shift, double stretch)
    {
        auto n = 0;
        const auto secondsPerBeat = 60.0 / juce::jmax (1.0, bpm);
        for (const auto b : noteBeats)
        {
            const auto t = (b + shift) * secondsPerBeat * stretch;
            const auto it = std::lower_bound (onsets.begin(), onsets.end(), t - 0.03);
            if (it != onsets.end() && *it <= t + 0.03)
                ++n;
        }
        return n;
    }

    std::pair<double, int> alignPartMidi (const std::vector<double>& noteBeats, std::vector<double> onsets, double bpm, double stretch, double barBeats)
    {
        std::sort (onsets.begin(), onsets.end());
        if (noteBeats.size() < 8 || onsets.size() < 8)
            return { 0.0, 0 };
        auto best = -1;
        auto shift = 0.0;
        for (int q = -256; q <= 256; ++q)
            if (const auto n = matchedAt (noteBeats, onsets, bpm, q * 0.25, stretch); n > best)
            {
                best = n;
                shift = q * 0.25;
            }
        if (barBeats > 0.0)
            for (const auto bars : { std::floor (shift / barBeats), std::ceil (shift / barBeats) })
                if (const auto n = matchedAt (noteBeats, onsets, bpm, bars * barBeats, stretch); n >= best * 0.95 && std::abs (bars * barBeats - shift) > 1.0e-9)
                {
                    shift = bars * barBeats;
                    best = n;
                    break;
                }
        return { shift, best };
    }

    BandAlignment alignBand (const std::vector<double>& kickBeats, std::vector<double> onsets, double bpm, double barBeats)
    {
        BandAlignment result;
        result.kicks = (int) kickBeats.size();
        std::sort (onsets.begin(), onsets.end());
        if (kickBeats.size() < 16 || onsets.size() < 16 || bpm <= 0.0)
            return result;
        const auto secondsPerBeat = 60.0 / bpm;
        const auto score = [&] (double shift, double stretch, int every)
        {
            auto n = 0;
            for (size_t k = 0; k < kickBeats.size(); k += (size_t) every)
            {
                const auto t = (kickBeats[k] + shift) * secondsPerBeat * stretch;
                const auto it = std::lower_bound (onsets.begin(), onsets.end(), t - 0.03);
                if (it != onsets.end() && *it <= t + 0.03)
                    ++n;
            }
            return n;
        };
        const auto every = kickBeats.size() > 400 ? 2 : 1;
        const auto bestShiftAt = [&] (double stretch, double& shift)
        {
            auto best = -1;
            for (int q = -256; q <= 256; ++q)
                if (const auto n = score (q * 0.25, stretch, every); n > best)
                {
                    best = n;
                    shift = q * 0.25;
                }
            return best;
        };
        // Coarse: every stretch in 0.01 % steps (0.01 % over five minutes is 30 ms: the window), every shift in quarter beats.
        auto shift = 0.0, stretch = 1.0;
        auto best = -1;
        for (int s = -100; s <= 100; ++s)
        {
            auto sh = 0.0;
            if (const auto n = bestShiftAt (1.0 + s * 0.0001, sh); n > best)
            {
                best = n;
                shift = sh;
                stretch = 1.0 + s * 0.0001;
            }
        }
        // Fine: round it, in 1/16 beat and 0.0005 % steps.
        auto fineShift = shift, fineStretch = stretch;
        best = score (shift, stretch, 1);
        for (int a = -4; a <= 4; ++a)
            for (int b = -20; b <= 20; ++b)
                if (const auto n = score (shift + a * 0.0625, stretch + b * 0.000005, 1); n > best)
                {
                    best = n;
                    fineShift = shift + a * 0.0625;
                    fineStretch = stretch + b * 0.000005;
                }
        // Whole quarter beats when it's that close (an exported MIDI starts on a beat).
        if (std::abs (fineShift * 4.0 - std::round (fineShift * 4.0)) < 0.25)
            fineShift = std::round (fineShift * 4.0) / 4.0;
        // A whole number of bars when that fits nearly as well: an exported MIDI starts on a bar line, and a
        // drummer's pattern repeats every beat, so shifts a beat apart score alike (the owner's first song:
        // 33 beats scored a little over the right 32, its breaks a beat off).
        if (barBeats > 0.0)
        {
            const auto here = score (fineShift, fineStretch, 1);
            for (const auto bars : { std::floor (fineShift / barBeats), std::ceil (fineShift / barBeats) })
            {
                const auto atBar = bars * barBeats;
                if (std::abs (atBar - fineShift) < 1.0e-9 || std::abs (atBar - fineShift) > barBeats * 0.5 + 1.0e-9)
                    continue;
                auto bestHere = 0;
                auto bestStretch = fineStretch;
                for (int b = -20; b <= 20; ++b)
                    if (const auto n = score (atBar, fineStretch + b * 0.000005, 1); n > bestHere)
                    {
                        bestHere = n;
                        bestStretch = fineStretch + b * 0.000005;
                    }
                if (bestHere >= here * 0.95)
                {
                    fineShift = atBar;
                    fineStretch = bestStretch;
                    break;
                }
            }
        }
        // Kept only when clearly better: the stretch against the best with none, the shift against none at that stretch.
        auto plainShift = 0.0;
        const auto plain = juce::jmax (1, [&] { bestShiftAt (1.0, plainShift); return score (plainShift, 1.0, 1); }());
        const auto withStretch = score (fineShift, fineStretch, 1);
        if (std::abs (fineStretch - 1.0) < 0.0003 || withStretch < plain * 1.15)
        {
            fineStretch = 1.0;
            fineShift = plainShift;
        }
        const auto unshifted = juce::jmax (1, score (0.0, fineStretch, 1));
        const auto fitted = score (fineShift, fineStretch, 1);
        if (fineShift != 0.0 && (fitted < unshifted * 1.15 || fitted < (int) kickBeats.size() * 3 / 10))
            fineShift = 0.0;
        result.drumShiftBeats = fineShift;
        result.diStretch = fineStretch;
        result.matched = score (fineShift, fineStretch, 1);
        return result;
    }

    juce::File defaultBandReviewFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Paragon PR7-S").getChildFile ("Band Record");
    }

    bool makeBandReviewSong (const BandSong& song, const juce::File& into, const BandReviewOptions& options, juce::File& made,
                             juce::String& error, const std::function<bool (double)>& progress, juce::String* report)
    {
        const auto keepGoing = [&] (double done) { return ! progress || progress (done); };
        const auto name = juce::File::createLegalFileName (song.name.isNotEmpty() ? song.name : "Song");
        const auto folder = into.getChildFile (name);
        const auto audioFolder = folder.getChildFile (name + " Audio");
        if (! audioFolder.createDirectory())
        {
            error = "Couldn't make " + audioFolder.getFullPathName();
            return false;
        }
        const auto bpm = juce::jlimit (20.0, 400.0, song.bpm);

        // Every DI read and heard first (their notes place the drums).
        struct Heard { juce::File file; BandRole role; std::vector<PlayedNote> played; double seconds = 0.0; bool stereo = false; };
        std::vector<Heard> heard;
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        auto done = 0;
        const auto total = juce::jmax (1, (int) song.dis.size());
        for (const auto& [file, role] : song.dis)
        {
            if (! keepGoing (0.05 + 0.85 * done++ / total))
            {
                error = "Stopped";
                return false;
            }
            if (role == BandRole::unknown || role == BandRole::drums || role == BandRole::keys || isMidi (file))
                continue;
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
            if (reader == nullptr || reader->lengthInSamples <= 0)
            {
                error << "Couldn't read " << file.getFileName() << "\n";
                continue;
            }
            const auto length = (int) juce::jmin ((juce::int64) std::numeric_limits<int>::max() / 2, reader->lengthInSamples);
            juce::AudioBuffer<float> sound ((int) juce::jmax (1u, reader->numChannels), length);
            reader->read (&sound, 0, length, 0, true, true);
            Heard h;
            h.file = file;
            h.role = role;
            h.seconds = length / reader->sampleRate;
            h.stereo = reader->numChannels > 1;
            const auto& strings = role == BandRole::bass ? options.bassStrings : options.guitarStrings;
            const auto low = strings.empty() ? -1 : strings.front();
            const auto high = strings.empty() ? -1 : strings.back() + options.frets;
            h.played = role == BandRole::rhythm ? transcribeRhythm (sound, reader->sampleRate, low > 0 ? low : 21, low > 0 ? juce::jmin (76, high) : 76)
                                                : transcribeLine (sound, reader->sampleRate, role, low, high);
            heard.push_back (std::move (h));
        }

        // The drummer's MIDI, and where it sits against the DIs.
        std::optional<MidiImport> drums;
        if (song.drums.existsAsFile())
        {
            juce::String midiError;
            drums = readMidiFile (song.drums, midiError);
        }
        BandAlignment fit;
        if (drums.has_value() && ! heard.empty())
        {
            std::vector<double> kicks, onsets;
            for (const auto& t : drums->tracks)
                for (const auto& n : t.notes)
                    if (n.pitch == 35 || n.pitch == 36)
                        kicks.push_back (n.start);
            std::sort (kicks.begin(), kicks.end());
            for (const auto& h : heard)
                if (h.role == BandRole::bass || h.role == BandRole::rhythm)
                    for (const auto& p : h.played)
                        onsets.push_back (p.start);
            fit = alignBand (kicks, onsets, bpm, song.numerator * 4.0 / song.denominator);
        }

        Project project;
        project.bpm = bpm;
        project.numerator = song.numerator;
        project.denominator = song.denominator;
        project.tracks.clear();
        const auto bar = project.beatsPerBar();
        const auto wholeBars = [bar] (double beats) { return juce::jmax (bar, std::ceil (beats / bar - 1.0e-9) * bar); };
        // A DI's seconds as the song's beats: the click's tempo, the files' own speed taken out.
        const auto beatsOf = [&] (double seconds) { return seconds / fit.diStretch * bpm / 60.0; };
        const auto addTrack = [&] (Track track) -> Track&
        {
            track.id = project.nextTrackId++;
            track.colour = trackColour ((int) project.tracks.size());
            project.tracks.push_back (std::move (track));
            return project.tracks.back();
        };
        juce::StringArray said;
        if (fit.drumShiftBeats != 0.0)
        {
            const auto bars = std::abs (fit.drumShiftBeats) / bar;
            const auto amount = std::abs (bars - std::round (bars)) < 1.0e-6 ? juce::String (juce::roundToInt (bars)) + (juce::roundToInt (bars) == 1 ? " bar" : " bars")
                                                                              : juce::String (std::abs (fit.drumShiftBeats), 2) + " beats";
            said.add ("The drum MIDI starts " + amount + (fit.drumShiftBeats < 0.0 ? " before" : " after") + " the DIs: moved into place.");
        }
        if (fit.diStretch != 1.0)
            said.add ("The DIs run " + juce::String (std::abs (fit.diStretch - 1.0) * 100.0, 2) + " % " + (fit.diStretch > 1.0 ? "slow" : "fast")
                      + " against " + juce::String (bpm, 2).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " BPM (they play as "
                      + juce::String (bpm / fit.diStretch, 2) + " BPM): fitted to the click.");
        if (fit.kicks > 0)
            said.add (juce::String (fit.matched) + " of the drummer's " + juce::String (fit.kicks) + " kicks have a bass or guitar note within 30 ms.");

        if (drums.has_value())
        {
            Part part;
            part.name = "Drums: " + song.name;
            for (const auto& t : drums->tracks)
                for (auto n : t.notes)
                {
                    n.start += fit.drumShiftBeats;
                    if (n.start >= 0.0)
                        part.notes.push_back (n);
                }
            std::sort (part.notes.begin(), part.notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
            auto last = 0.0;
            for (const auto& n : part.notes)
                last = juce::jmax (last, n.start + n.length);
            part.length = wholeBars (last);
            const auto gm = DrumMap::gm();
            project.drumMaps.push_back (gm);
            Track drumTrack;
            drumTrack.kind = Track::Kind::instrument;
            drumTrack.name = "Drums (MIDI)";
            drumTrack.drumMap = gm.name;
            drumTrack.drumEditor = true;
            drumTrack.parts.push_back (part);
            addTrack (drumTrack);
        }

        // (0.82.4) The keyboards: the players' own MIDI, exact, read as it is. Placed with the drums' shift when that
        // lines its notes up with the DIs about as well as its own best fit (the same session's exports), else by its own.
        std::vector<double> diOnsets;
        for (const auto& h : heard)
            for (const auto& p : h.played)
                diOnsets.push_back (p.start);
        std::sort (diOnsets.begin(), diOnsets.end());
        for (const auto& [file, role] : song.dis)
        {
            if (role != BandRole::keys)
                continue;
            if (! isMidi (file))
            {
                said.add (file.getFileName() + ": keyboards are read from their MIDI; audio of them isn't transcribed.");
                continue;
            }
            juce::String midiError;
            const auto keys = readMidiFile (file, midiError);
            if (! keys.has_value())
            {
                error << "Couldn't read " << file.getFileName() << ": " << midiError << "\n";
                continue;
            }
            std::vector<double> starts;
            for (const auto& t : keys->tracks)
                for (const auto& n : t.notes)
                    starts.push_back (n.start);
            auto shift = fit.drumShiftBeats;
            if (! diOnsets.empty())
            {
                const auto withDrums = matchedAt (starts, diOnsets, bpm, fit.drumShiftBeats, fit.diStretch);
                const auto own = alignPartMidi (starts, diOnsets, bpm, fit.diStretch, bar);
                if (own.second > withDrums * 1.15 + 4)
                    shift = own.first;
                said.add ("Keys (" + file.getFileName() + "): " + juce::String (keys->tracks.size()) + (keys->tracks.size() == 1 ? " track, " : " tracks, ")
                          + juce::String ((int) starts.size()) + " notes, "
                          + (shift == fit.drumShiftBeats ? juce::String ("placed with the drums.") : "placed by its own fit (" + juce::String (shift, 2) + " beats)."));
            }
            for (const auto& t : keys->tracks)
            {
                Part part;
                part.name = (t.name.isNotEmpty() ? t.name : file.getFileNameWithoutExtension());
                auto last = 0.0;
                for (auto n : t.notes)
                {
                    n.start += shift;
                    if (n.start >= 0.0)
                    {
                        part.notes.push_back (n);
                        last = juce::jmax (last, n.start + n.length);
                    }
                }
                for (auto c : t.controllers)
                {
                    c.start += shift;
                    if (c.start >= 0.0)
                        part.controllers.push_back (c);
                }
                if (part.notes.empty())
                    continue;
                part.length = wholeBars (last);
                Track keysTrack;
                keysTrack.kind = Track::Kind::instrument;
                keysTrack.name = "KEYS: " + part.name;
                keysTrack.midiChannel = juce::jlimit (1, 16, t.channel);
                keysTrack.parts.push_back (part);
                addTrack (keysTrack);
            }
        }

        std::vector<LineNote> riffNotes, leadNotes;
        for (const auto& h : heard)
        {
            // The DI itself, copied beside the song (stretched to the click when the files run off it).
            const auto copy = audioFolder.getChildFile (h.file.getFileName());
            if (! copy.existsAsFile() && ! h.file.copyFileTo (copy))
            {
                error << "Couldn't copy " << h.file.getFileName() << "\n";
                continue;
            }
            Track diTrack;
            diTrack.kind = Track::Kind::audio;
            diTrack.stereoInput = h.stereo;
            diTrack.name = bandRoleName (h.role) + " DI";
            Part clip;
            clip.name = h.file.getFileNameWithoutExtension();
            clip.audioFile = copy.getFileName();
            clip.length = beatsOf (h.seconds);
            if (fit.diStretch != 1.0)
                clip.warp = { { 0.0, 0.0 }, { clip.length, h.seconds } };
            diTrack.parts.push_back (clip);
            addTrack (diTrack);

            // What was heard on it.
            auto idea = playedIdea (h.played, bpm / fit.diStretch, h.role, options.velocities);
            const auto range = h.role == BandRole::lead ? options.leadBendRange : h.role == BandRole::bass ? options.bassBendRange : options.guitarBendRange;
            Studio::fitBends (idea.notes, range);
            Part heardPart;
            heardPart.name = bandRoleName (h.role) + " (heard)";
            heardPart.length = wholeBars (beatsOf (h.seconds));
            for (const auto& n : idea.notes)
                heardPart.notes.push_back ({ n.beat, n.length, n.pitch, n.velocity });
            heardPart.controllers = Studio::effectControllers (idea.notes, range);
            auto& gathered = h.role == BandRole::lead ? leadNotes : riffNotes;
            gathered.insert (gathered.end(), idea.notes.begin(), idea.notes.end());
            Track heardTrack;
            heardTrack.kind = Track::Kind::instrument;
            heardTrack.name = bandRoleName (h.role) + " (heard)";
            heardTrack.parts.push_back (heardPart);
            addTrack (heardTrack);
        }
        // The song's sections, as cycle markers to rename and move.
        if (drums.has_value())
        {
            std::vector<Note> drumNotes;
            for (const auto& t : project.tracks)
                if (t.name == "Drums (MIDI)" && ! t.parts.empty())
                    drumNotes = t.parts[0].notes;
            const auto map = project.signatures();
            const auto sections = findBandSections (drumNotes, map, riffNotes, leadNotes);
            if (sections.size() > 1)
            {
                juce::StringArray named;
                for (const auto& sec : sections)
                {
                    Marker m;
                    m.cycle = true;
                    m.id = project.markerTrack.nextId (true);
                    m.start = map.beatOfBar (sec.firstBar);
                    m.end = map.beatOfBar (sec.firstBar + sec.bars);
                    m.name = sec.name;
                    project.markerTrack.markers.push_back (m);
                    named.add (sec.name);
                }
                project.markerTrack.shown = true;
                project.markerTrack.name = "Sections";
                project.markerTrack.sort();
                said.add ("Sections (a guess from the drums and the riffs: rename or move the markers): " + named.joinIntoString (", ") + ".");
            }
        }
        if (project.tracks.empty())
        {
            error << "Nothing to read in " << song.name;
            return false;
        }
        made = folder.getChildFile (name + Project::fileExtension);
        const auto xml = project.toXml();
        if (xml == nullptr || ! xml->writeTo (made))
        {
            error << "Couldn't write " << made.getFullPathName();
            return false;
        }
        if (report != nullptr)
            *report = said.joinIntoString ("\n");
        keepGoing (1.0);
        return true;
    }
}
