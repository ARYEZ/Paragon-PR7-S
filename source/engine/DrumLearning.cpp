#include "engine/DrumLearning.h"
#include "engine/MidiImport.h"

#include <map>
#include <numeric>

namespace pr7s
{
    namespace
    {
        using namespace GmDrum;
        constexpr int splash = 55;

        bool isHandNote (int n) { return n == closedHat || n == pedalHat || n == openHat || n == ride || n == rideBell || n == china; }
        bool isCrashNote (int n) { return n == crash || n == crash2 || n == splash || n == china; }
        bool isTomNote (int n) { return n == tomHigh || n == tomMid || n == tomLow || n == tomFloor; }
        bool isSnareNote (int n) { return n == snare || n == 39 || n == 40; }
        bool isGhost (const LearnedHit& h) { return h.note == sideStick || (isSnareNote (h.note) && h.velocity < 50); }
        int sixteenthOf (int step) { return juce::jlimit (0, 15, (int) std::lround (step / 3.0)); }

        // Is this stretch of the bar (steps) a fill: no time kept on a cymbal, the toms or snare busy.
        bool fillIn (const std::vector<LearnedHit>& hits, int from, int to)
        {
            auto hand = 0, toms = 0, snares = 0;
            for (const auto& h : hits)
                if (h.step >= from && h.step < to)
                {
                    if (isHandNote (h.note)) ++hand;
                    if (isTomNote (h.note)) ++toms;
                    if (isSnareNote (h.note) || h.note == sideStick) ++snares;
                }
            return hand == 0 && (toms >= 1 || snares >= 3) && toms + snares >= 2;
        }

        void analyse (DrumBar& bar)
        {
            bar.kick = {};
            bar.snare = {};
            bar.ghosts = bar.toms = bar.handHits = bar.notes = 0;
            bar.crashAtStart = bar.fill = bar.twoBeatFill = bar.halfTime = false;
            bar.cymbal = -1;
            std::map<int, int> cymbals;
            auto velocity = 0.0f;
            for (const auto& h : bar.hits)
            {
                ++bar.notes;
                velocity += (float) h.velocity / 127.0f;
                const auto s = sixteenthOf (h.step);
                if (h.note == kick)
                    bar.kick[(size_t) s] = true;
                else if (isGhost (h))
                    ++bar.ghosts;
                else if (isSnareNote (h.note))
                    bar.snare[(size_t) s] = true;
                else if (isTomNote (h.note))
                    ++bar.toms;
                if (isHandNote (h.note))
                {
                    ++bar.handHits;
                    ++cymbals[h.note == pedalHat ? closedHat : h.note];
                }
                if (isCrashNote (h.note))
                {
                    if (h.step <= 1)
                        bar.crashAtStart = true;
                    else if (h.note != china)
                        ++cymbals[h.note];   // (riding the crash)
                }
            }
            bar.velocity = bar.notes > 0 ? velocity / (float) bar.notes : 0.0f;
            auto most = 0;
            for (const auto& [note, count] : cymbals)
                if (count > most)
                {
                    most = count;
                    bar.cymbal = note;
                }
            bar.halfTime = bar.snare[8] && ! bar.snare[4] && ! bar.snare[12];
            bar.fill = fillIn (bar.hits, 36, 48);
            bar.twoBeatFill = bar.fill && fillIn (bar.hits, 24, 36);
        }

        // How differently two bars are played (0: alike). The last beat is left out when either is a fill.
        float barDistance (const DrumBar& a, const DrumBar& b)
        {
            if (a.silent() != b.silent())
                return 12.0f;
            if (a.silent())
                return 0.0f;
            const auto limit = a.fill || b.fill ? 12 : 16;
            auto d = 0.0f;
            for (auto s = 0; s < limit; ++s)
                d += (a.kick[(size_t) s] != b.kick[(size_t) s] ? 1.0f : 0.0f) + (a.snare[(size_t) s] != b.snare[(size_t) s] ? 1.0f : 0.0f);
            if (a.cymbal != b.cymbal)
                d += a.cymbal >= 0 && b.cymbal >= 0 ? 4.0f : 2.0f;
            d += juce::jmin (4.0f, (float) std::abs (a.handHits - b.handHits) / 3.0f);
            if (a.halfTime != b.halfTime)
                d += 3.0f;
            return d;
        }

        float meanDistance (const std::vector<DrumBar>& bars, int a0, int a1, int b0, int b1, bool within)
        {
            auto sum = 0.0f;
            auto n = 0;
            for (auto i = a0; i < a1; ++i)
                for (auto j = within ? i + 1 : b0; j < b1; ++j)
                {
                    sum += barDistance (bars[(size_t) i], bars[(size_t) j]);
                    ++n;
                }
            return n > 0 ? sum / (float) n : 0.0f;
        }

        // A section's playing, summed up for grouping and naming.
        struct Summary
        {
            std::array<float, 16> kick {}, snare {};
            int cymbal = -1;
            float hand = 0.0f, half = 0.0f, density = 0.0f, velocity = 0.0f, crash = 0.0f, energy = 0.0f;
            bool silent = true;
        };

        Summary summarise (const std::vector<DrumBar>& bars, const DrumSongSection& section)
        {
            Summary s;
            std::map<int, int> cymbals;
            auto played = 0;
            for (auto b = section.firstBar; b < section.firstBar + section.bars && b < (int) bars.size(); ++b)
            {
                const auto& bar = bars[(size_t) b];
                if (bar.silent())
                    continue;
                ++played;
                const auto limit = bar.fill ? 12 : 16;
                for (auto i = 0; i < limit; ++i)
                {
                    s.kick[(size_t) i] += bar.kick[(size_t) i] ? 1.0f : 0.0f;
                    s.snare[(size_t) i] += bar.snare[(size_t) i] ? 1.0f : 0.0f;
                }
                if (bar.cymbal >= 0)
                    ++cymbals[bar.cymbal];
                s.hand += (float) bar.handHits;
                s.half += bar.halfTime ? 1.0f : 0.0f;
                s.density += (float) bar.notes;
                s.velocity += bar.velocity;
                s.crash += bar.crashAtStart ? 1.0f : 0.0f;
            }
            if (played == 0)
                return s;
            s.silent = false;
            const auto n = (float) played;
            for (auto i = 0; i < 16; ++i)
            {
                s.kick[(size_t) i] /= n;
                s.snare[(size_t) i] /= n;
            }
            s.hand /= n;
            s.half /= n;
            s.density /= n;
            s.velocity /= n;
            s.crash /= n;
            auto most = 0;
            for (const auto& [note, count] : cymbals)
                if (count > most)
                {
                    most = count;
                    s.cymbal = note;
                }
            return s;
        }

        float summaryDistance (const Summary& a, const Summary& b)
        {
            auto d = 0.0f;
            for (auto i = 0; i < 16; ++i)
                d += std::abs (a.kick[(size_t) i] - b.kick[(size_t) i]) + std::abs (a.snare[(size_t) i] - b.snare[(size_t) i]);
            if (a.cymbal != b.cymbal)
                d += 4.0f;
            // The cymbal's rate: twice or half as fast is a change of feel (quarters to eighths).
            const auto fast = juce::jmax (a.hand, b.hand), slow = juce::jmin (a.hand, b.hand);
            d += slow > 0.5f && fast / slow >= 1.6f ? 3.0f : juce::jmin (4.0f, (fast - slow) / 4.0f);
            d += 3.0f * std::abs (a.half - b.half);
            return d;
        }

        juce::String kindId (DrumSection s) { return drumSectionName (s).toLowerCase(); }

        juce::String hitsToText (const std::vector<LearnedHit>& hits)
        {
            juce::StringArray parts;
            for (const auto& h : hits)
                parts.add (juce::String (h.step) + ":" + juce::String (h.note) + ":" + juce::String (h.velocity));
            return parts.joinIntoString (" ");
        }

        std::vector<LearnedHit> hitsFromText (const juce::String& text)
        {
            std::vector<LearnedHit> hits;
            for (const auto& token : juce::StringArray::fromTokens (text, " ", ""))
            {
                const auto p = juce::StringArray::fromTokens (token, ":", "");
                if (p.size() != 3)
                    continue;
                const auto step = p[0].getIntValue(), note = p[1].getIntValue(), velocity = p[2].getIntValue();
                if (step >= 0 && step < learnSteps && note >= 35 && note <= 59)
                    hits.push_back ({ step, note, juce::jlimit (1, 127, velocity) });
            }
            return hits;
        }

        template <size_t N, typename T>
        juce::String arrayToText (const std::array<T, N>& a)
        {
            juce::StringArray parts;
            for (const auto v : a)
                parts.add (juce::String ((double) v, 4));
            return parts.joinIntoString (" ");
        }

        template <size_t N, typename T>
        std::array<T, N> arrayFromText (const juce::String& text)
        {
            std::array<T, N> a {};
            const auto parts = juce::StringArray::fromTokens (text, " ", "");
            for (size_t i = 0; i < N && (int) i < parts.size(); ++i)
                a[i] = (T) parts[(int) i].getDoubleValue();
            return a;
        }

        const SectionHabits* habitsFor (const DrumProfile& profile, DrumSection section)
        {
            using S = DrumSection;
            std::vector<S> order { section };
            switch (section)
            {
                case S::verse: order.insert (order.end(), { S::chorus, S::bridge, S::intro, S::breakdown, S::ending }); break;
                case S::chorus: order.insert (order.end(), { S::verse, S::bridge, S::breakdown, S::intro, S::ending }); break;
                case S::bridge: order.insert (order.end(), { S::verse, S::chorus, S::breakdown, S::intro, S::ending }); break;
                case S::breakdown: order.insert (order.end(), { S::chorus, S::bridge, S::verse, S::intro, S::ending }); break;
                case S::intro: order.insert (order.end(), { S::verse, S::chorus, S::bridge, S::breakdown, S::ending }); break;
                case S::ending: order.insert (order.end(), { S::verse, S::chorus, S::intro, S::bridge, S::breakdown }); break;
                case S::solo: order.insert (order.end(), { S::chorus, S::verse, S::bridge, S::breakdown, S::intro, S::ending }); break;
            }
            for (const auto s : order)
                if (profile.has (s) && ! profile.of (s).grooves.empty())
                    return &profile.of (s);
            return nullptr;
        }

        // One of the most played (the first four), more often the more it was played.
        const SectionHabits::Pattern* pick (const std::vector<SectionHabits::Pattern>& list, juce::Random& rng, const SectionHabits::Pattern* not_ = nullptr)
        {
            std::vector<const SectionHabits::Pattern*> choices;
            for (const auto& p : list)
                if (&p != not_ && choices.size() < 4)
                    choices.push_back (&p);
            if (choices.empty())
                return nullptr;
            auto total = 0;
            for (const auto* p : choices)
                total += juce::jmax (1, p->count);
            auto r = rng.nextInt (juce::jmax (1, total));
            for (const auto* p : choices)
            {
                r -= juce::jmax (1, p->count);
                if (r < 0)
                    return p;
            }
            return choices.back();
        }

        using Hits = std::vector<LearnedHit>;
        bool has (const Hits& hits, int step, int note)
        {
            return std::any_of (hits.begin(), hits.end(), [=] (const LearnedHit& h) { return h.step == step && h.note == note; });
        }

        // DRUM SETTINGS on a learned bar (0.5: as played).
        Hits shape (Hits bar, const SectionHabits& h, const DrumSettings& s, juce::Random& rng)
        {
            // Density: kicks dropped where the drummer rarely put one, or added where they often did.
            if (s.density < 0.5f)
            {
                const auto p = (0.5f - s.density) * 2.0f;
                bar.erase (std::remove_if (bar.begin(), bar.end(), [&] (const LearnedHit& x)
                           { return x.note == kick && x.step != 0 && rng.nextFloat() < p * (1.0f - 0.5f * h.kick[(size_t) sixteenthOf (x.step)]); }), bar.end());
            }
            else if (s.density > 0.5f)
            {
                const auto p = (s.density - 0.5f) * 2.0f;
                for (auto step = 0; step < learnSteps; step += 3)
                    if (! has (bar, step, kick) && rng.nextFloat() < p * h.kick[(size_t) sixteenthOf (step)])
                        bar.push_back ({ step, kick, step % 12 == 0 ? 110 : 94 });
            }
            // Double kick: the feet filling in next to the kicks (or the fills taken out).
            if (s.doubleKick > 0.9f)
            {
                for (auto step = 0; step < learnSteps; step += 3)
                    if (! has (bar, step, kick))
                        bar.push_back ({ step, kick, 92 });
            }
            else if (s.doubleKick > 0.5f)
            {
                const auto p = (s.doubleKick - 0.5f) * 1.4f;
                const auto before = bar;
                for (auto step = 3; step < learnSteps; step += 3)
                    if (! has (before, step, kick) && (has (before, step - 3, kick) || has (before, step + 3, kick)) && rng.nextFloat() < p)
                        bar.push_back ({ step, kick, 90 });
            }
            else if (s.doubleKick < 0.5f)
            {
                const auto p = (0.5f - s.doubleKick) * 2.0f;
                const auto before = bar;
                bar.erase (std::remove_if (bar.begin(), bar.end(), [&] (const LearnedHit& x)
                           { return x.note == kick && x.step % 12 != 0 && (has (before, x.step - 3, kick) || has (before, x.step - 1, kick)) && rng.nextFloat() < p; }), bar.end());
            }
            // Ghost notes.
            if (s.ghostNotes < 0.5f)
            {
                const auto p = (0.5f - s.ghostNotes) * 2.0f;
                bar.erase (std::remove_if (bar.begin(), bar.end(), [&] (const LearnedHit& x) { return isGhost (x) && rng.nextFloat() < p; }), bar.end());
            }
            else if (s.ghostNotes > 0.5f)
            {
                const auto p = (s.ghostNotes - 0.5f) * 2.0f;
                for (auto step = 3; step < learnSteps; step += 3)
                    if (step % 12 != 0 && std::none_of (bar.begin(), bar.end(), [=] (const LearnedHit& x) { return x.step == step && isSnareNote (x.note); })
                        && rng.nextFloat() < p * juce::jmax (0.12f, h.ghost[(size_t) sixteenthOf (step)]) * 0.6f)
                        bar.push_back ({ step, snare, 34 + rng.nextInt (12) });
            }
            // The cymbal for every section.
            if (s.cymbal != DrumSettings::Cymbal::automatic)
            {
                const auto to = s.cymbal == DrumSettings::Cymbal::hats ? closedHat : s.cymbal == DrumSettings::Cymbal::ride ? ride : china;
                for (auto& x : bar)
                    if (isHandNote (x.note) || ((x.note == crash || x.note == crash2) && x.step > 1))
                        x.note = to;
            }
            std::sort (bar.begin(), bar.end(), [] (const LearnedHit& a, const LearnedHit& b) { return a.step != b.step ? a.step < b.step : a.note < b.note; });
            bar.erase (std::unique (bar.begin(), bar.end(), [] (const LearnedHit& a, const LearnedHit& b) { return a.step == b.step && a.note == b.note; }), bar.end());
            return bar;
        }

        // The phrase's third bar when the drummer has no second groove: a kick moved where they'd put one.
        Hits varyKick (Hits bar, const SectionHabits& h, juce::Random& rng)
        {
            std::vector<size_t> kicks;
            for (size_t i = 0; i < bar.size(); ++i)
                if (bar[i].note == kick && bar[i].step != 0)
                    kicks.push_back (i);
            if (! kicks.empty() && rng.nextBool())
                bar.erase (bar.begin() + (std::ptrdiff_t) kicks[(size_t) rng.nextInt ((int) kicks.size())]);
            auto total = 0.0f;
            for (auto s = 1; s < 16; ++s)
                total += has (bar, s * 3, kick) ? 0.0f : h.kick[(size_t) s];
            if (total > 0.0f)
            {
                auto r = rng.nextFloat() * total;
                for (auto s = 1; s < 16; ++s)
                {
                    if (has (bar, s * 3, kick))
                        continue;
                    r -= h.kick[(size_t) s];
                    if (r <= 0.0f)
                    {
                        bar.push_back ({ s * 3, kick, 94 });
                        break;
                    }
                }
            }
            std::sort (bar.begin(), bar.end(), [] (const LearnedHit& a, const LearnedHit& b) { return a.step != b.step ? a.step < b.step : a.note < b.note; });
            return bar;
        }

        int pushRole (const LearnedHit& h)
        {
            if (h.note == kick) return 0;
            if (isSnareNote (h.note) && h.velocity >= 50) return 1;
            if (isHandNote (h.note)) return 2;
            return -1;
        }

        juce::String cymbalWord (int note)
        {
            return note == ride ? "ride" : note == china ? "china" : note == crash || note == crash2 ? "crashes" : note == rideBell ? "ride bell"
                 : note == openHat ? "open hats" : note < 0 ? "no cymbal" : "hats";
        }
    }

    //==========================================================================
    DrumRole drumRoleOf (int pitch, float velocity)
    {
        const auto n = gmNoteOf (pitch);
        if (n == kick) return DrumRole::kick;
        if (n == sideStick) return DrumRole::ghost;
        if (isSnareNote (n)) return velocity * 127.0f < 50.0f ? DrumRole::ghost : DrumRole::snare;
        if (n == closedHat || n == pedalHat) return DrumRole::hats;
        if (n == openHat) return DrumRole::openHat;
        if (n == ride) return DrumRole::ride;
        if (n == rideBell) return DrumRole::bell;
        if (n == china) return DrumRole::china;
        if (n == crash || n == crash2 || n == splash) return DrumRole::crash;
        if (isTomNote (n)) return DrumRole::tom;
        return DrumRole::other;
    }

    int gmNoteOf (int pitch)
    {
        switch (pitch)
        {
            case 35: case 36: return kick;
            case 37: return sideStick;
            case 38: case 40: return snare;
            case 39: return 39;
            case 42: case 22: return closedHat;
            case 44: case 21: return pedalHat;
            case 46: case 23: case 24: case 25: case 26: return openHat;
            case 49: return crash;
            case 57: return crash2;
            case 55: return splash;
            case 52: return china;
            case 51: case 59: return ride;
            case 53: return rideBell;
            case 50: case 48: return tomHigh;
            case 47: case 45: return tomMid;
            case 43: return tomLow;
            case 41: return tomFloor;
            default: break;
        }
        return pitch >= 35 && pitch <= 59 ? pitch : -1;
    }

    DrumSong drumSongFromNotes (const juce::String& name, const std::vector<Note>& notes, double bpm, const SignatureMap& map)
    {
        DrumSong song;
        song.name = name;
        song.bpm = bpm;
        auto last = -1.0;
        for (const auto& n : notes)
            if (gmNoteOf (n.pitch) >= 0)
                last = juce::jmax (last, n.start);
        if (last < 0.0)
            return song;
        const auto count = juce::jmax (1, map.barAt (last) + 1);
        song.bars.resize ((size_t) count);
        for (auto i = 0; i < count; ++i)
        {
            song.bars[(size_t) i].start = map.beatOfBar (i);
            song.bars[(size_t) i].length = map.beatOfBar (i + 1) - song.bars[(size_t) i].start;
        }
        for (const auto& n : notes)
        {
            const auto note = gmNoteOf (n.pitch);
            if (note < 0 || n.start < -1.0e-9)
                continue;
            auto bar = juce::jlimit (0, count - 1, map.barAt (n.start));
            auto* b = &song.bars[(size_t) bar];
            auto step = (int) std::lround ((n.start - b->start) / b->length * learnSteps);
            const auto offset = n.start - (b->start + step * b->length / learnSteps);
            if (step >= learnSteps)
            {
                if (bar + 1 < count)
                {
                    b = &song.bars[(size_t) ++bar];
                    step = 0;
                }
                else
                    step = learnSteps - 1;
            }
            const LearnedHit hit { juce::jlimit (0, learnSteps - 1, step), note, juce::jlimit (1, 127, (int) std::lround (n.velocity * 127.0f)) };
            auto twin = std::find_if (b->hits.begin(), b->hits.end(), [&] (const LearnedHit& h) { return h.step == hit.step && h.note == hit.note; });
            if (twin != b->hits.end())
            {
                twin->velocity = juce::jmax (twin->velocity, hit.velocity);
                continue;
            }
            b->hits.push_back (hit);
            if (const auto role = pushRole (hit); role >= 0)
            {
                song.pushSum[(size_t) role] += offset;
                ++song.pushCount[(size_t) role];
            }
        }
        for (auto& b : song.bars)
        {
            std::sort (b.hits.begin(), b.hits.end(), [] (const LearnedHit& x, const LearnedHit& y) { return x.step != y.step ? x.step < y.step : x.note < y.note; });
            analyse (b);
        }
        song.sections = findSections (song.bars);
        nameSections (song.bars, song.sections);
        return song;
    }

    std::optional<DrumSong> readDrumSong (const juce::File& file, juce::String& error)
    {
        const auto midi = readMidiFile (file, error);
        if (! midi.has_value())
            return std::nullopt;
        const auto drumChannel = std::any_of (midi->tracks.begin(), midi->tracks.end(), [] (const MidiImport::Track& t) { return t.channel == 10; });
        std::vector<Note> notes;
        for (const auto& t : midi->tracks)
            if (! drumChannel || t.channel == 10)
                notes.insert (notes.end(), t.notes.begin(), t.notes.end());
        const SignatureMap map (midi->numerator, midi->denominator, midi->signatureChanges);
        auto song = drumSongFromNotes (file.getFileNameWithoutExtension(), notes, midi->bpm, map);
        song.file = file;
        if (song.bars.empty())
        {
            error = "No drum notes in " + file.getFileName();
            return std::nullopt;
        }
        return song;
    }

    std::vector<juce::File> findDrumSongs (const juce::File& folder)
    {
        std::vector<juce::File> files;
        if (! folder.isDirectory())
            return files;
        for (const auto& entry : juce::RangedDirectoryIterator (folder, true, "*.mid;*.midi;*.MID;*.MIDI", juce::File::findFiles))
            files.push_back (entry.getFile());
        std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
                   { return a.getFullPathName().compareNatural (b.getFullPathName()) < 0; });
        files.erase (std::unique (files.begin(), files.end()), files.end());
        return files;
    }

    std::vector<DrumSongSection> findSections (const std::vector<DrumBar>& bars, const std::vector<float>* extra)
    {
        const auto n = (int) bars.size();
        if (n == 0)
            return {};
        std::vector<std::pair<float, int>> candidates;
        for (auto b = 1; b < n; ++b)
        {
            const auto b0 = juce::jmax (0, b - 4), a1 = juce::jmin (n, b + 4);
            const auto cross = meanDistance (bars, b0, b, b, a1, false);
            const auto within = 0.5f * (meanDistance (bars, b0, b, 0, b, true) + meanDistance (bars, b, a1, 0, a1, true));
            auto score = juce::jmin (4.0f, juce::jmax (0.0f, cross - within) * 0.5f);
            auto riding = 0, around = 0;
            for (auto i = b0; i < a1; ++i, ++around)
                riding += bars[(size_t) i].crashAtStart ? 1 : 0;
            const auto& now = bars[(size_t) b];
            const auto& before = bars[(size_t) b - 1];
            if (now.crashAtStart && riding * 10 < around * 6)
                score += 1.0f;
            if (before.fill)
                score += 1.0f;
            if (now.cymbal >= 0 && before.cymbal >= 0 && now.cymbal != before.cymbal)
                score += 2.0f;
            if (now.silent() != before.silent())
                score += 4.0f;
            if (extra != nullptr && b < (int) extra->size())
                score += (*extra)[(size_t) b];
            if (score >= 2.5f)
                candidates.emplace_back (score, b);
        }
        std::sort (candidates.begin(), candidates.end(), [] (const auto& x, const auto& y) { return x.first > y.first || (! (y.first > x.first) && x.second < y.second); });
        std::vector<int> cuts;
        for (const auto& [score, b] : candidates)
        {
            const auto gap = score >= 6.0f ? 1 : 3;
            if (b < (score >= 6.0f ? 1 : 2))
                continue;
            if (std::all_of (cuts.begin(), cuts.end(), [&] (int c) { return std::abs (c - b) >= gap; }))
                cuts.push_back (b);
        }
        std::sort (cuts.begin(), cuts.end());
        std::vector<DrumSongSection> sections;
        auto from = 0;
        for (const auto c : cuts)
        {
            sections.push_back ({ from, c - from });
            from = c;
        }
        sections.push_back ({ from, n - from });
        // A lone bar at either end belongs with its neighbour: the song's last hit to the ending,
        // a pickup to what it leads into.
        if (sections.size() > 1 && sections.back().bars == 1)
        {
            sections[sections.size() - 2].bars += 1;
            sections.pop_back();
        }
        if (sections.size() > 1 && sections.front().bars == 1)
        {
            sections[1].firstBar = 0;
            sections[1].bars += 1;
            sections.erase (sections.begin());
        }
        return sections;
    }

    void nameSections (const std::vector<DrumBar>& bars, std::vector<DrumSongSection>& sections)
    {
        using S = DrumSection;
        const auto count = (int) sections.size();
        if (count == 0)
            return;
        std::vector<Summary> sums;
        for (const auto& s : sections)
            sums.push_back (summarise (bars, s));

        // Groups: sections that play alike.
        std::vector<int> leaders;
        for (auto i = 0; i < count; ++i)
        {
            sections[(size_t) i].group = -1;
            if (sums[(size_t) i].silent)
                continue;
            for (size_t g = 0; g < leaders.size(); ++g)
                if (summaryDistance (sums[(size_t) i], sums[(size_t) leaders[g]]) < 4.5f)
                {
                    sections[(size_t) i].group = (int) g;
                    break;
                }
            if (sections[(size_t) i].group < 0)
            {
                sections[(size_t) i].group = (int) leaders.size();
                leaders.push_back (i);
            }
        }
        // Energy: how big each plays, against the song's own.
        auto density = 0.0f, velocity = 0.0f;
        auto played = 0;
        for (const auto& s : sums)
            if (! s.silent)
            {
                density += s.density;
                velocity += s.velocity;
                ++played;
            }
        if (played == 0)
        {
            for (auto& s : sections)
            {
                s.kind = S::intro;
                s.used = false;
            }
            return;
        }
        density /= (float) played;
        velocity /= (float) played;
        std::vector<float> energies;
        for (auto& s : sums)
            if (! s.silent)
            {
                const auto bigCymbal = s.cymbal == ride || s.cymbal == china || s.cymbal == crash || s.cymbal == crash2 || s.cymbal == splash;
                s.energy = s.density / juce::jmax (0.01f, density) + s.velocity / juce::jmax (0.01f, velocity) + (bigCymbal ? 0.5f : 0.0f) + 0.5f * s.crash;
                energies.push_back (s.energy);
            }
        std::sort (energies.begin(), energies.end());
        const auto median = energies[energies.size() / 2];

        std::vector<bool> named ((size_t) count, false);
        const auto name = [&] (int i, S kind) { sections[(size_t) i].kind = kind; named[(size_t) i] = true; };
        for (auto i = 0; i < count; ++i)
        {
            sections[(size_t) i].used = ! sums[(size_t) i].silent;
            if (sums[(size_t) i].silent)
                name (i, i == 0 ? S::intro : i == count - 1 ? S::ending : S::bridge);
        }
        const auto groupSize = [&] (int g) { return (int) std::count_if (sections.begin(), sections.end(), [g] (const DrumSongSection& s) { return s.group == g; }); };
        // Half time: a breakdown (a quiet one, a bridge).
        for (auto i = 0; i < count; ++i)
            if (! named[(size_t) i] && sums[(size_t) i].half >= 0.6f)
            {
                const auto& s = sums[(size_t) i];
                name (i, s.cymbal == china || s.cymbal == crash || s.energy >= median ? S::breakdown : S::bridge);
            }
        // The first and last.
        auto first = 0;
        while (first < count && sums[(size_t) first].silent)
            ++first;
        if (first < count && ! named[(size_t) first] && count > 1 && sections[(size_t) first].bars <= 8
            && groupSize (sections[(size_t) first].group) == 1)   // (a groove played again later is a verse or chorus, not an intro)
            name (first, S::intro);
        auto last = count - 1;
        while (last > 0 && sums[(size_t) last].silent)
            --last;
        if (last > first && ! named[(size_t) last]
            && (sections[(size_t) last].bars <= 4 || (groupSize (sections[(size_t) last].group) == 1 && sections[(size_t) last].bars <= 8)))
            name (last, S::ending);

        // Verse and chorus from the groups played more than once (the bigger: the chorus).
        std::map<int, std::vector<int>> groups;
        for (auto i = 0; i < count; ++i)
            if (! named[(size_t) i])
                groups[sections[(size_t) i].group].push_back (i);
        struct G { int group; int size; float energy; };
        std::vector<G> repeated;
        for (const auto& [g, members] : groups)
            if (members.size() >= 2)
            {
                auto e = 0.0f;
                for (const auto i : members)
                    e += sums[(size_t) i].energy;
                repeated.push_back ({ g, (int) members.size(), e / (float) members.size() });
            }
        auto chorusEnergy = -1.0f, verseEnergy = -1.0f;
        if (repeated.size() >= 2)
        {
            std::sort (repeated.begin(), repeated.end(), [] (const G& a, const G& b) { return a.energy > b.energy; });
            chorusEnergy = repeated[0].energy;
            auto verse = std::max_element (repeated.begin() + 1, repeated.end(), [] (const G& a, const G& b)
                                           { return a.size != b.size ? a.size < b.size : a.energy > b.energy; });
            verseEnergy = verse->energy;
        }
        else if (repeated.size() == 1)
        {
            auto all = 0.0f;
            auto n = 0;
            for (auto i = 0; i < count; ++i)
                if (! named[(size_t) i])
                {
                    all += sums[(size_t) i].energy;
                    ++n;
                }
            if (repeated[0].energy > all / (float) juce::jmax (1, n) + 0.05f)
                chorusEnergy = repeated[0].energy;
            else
                verseEnergy = repeated[0].energy;
        }
        for (const auto& g : repeated)
        {
            const auto toChorus = verseEnergy < 0.0f || (chorusEnergy >= 0.0f && std::abs (g.energy - chorusEnergy) < std::abs (g.energy - verseEnergy));
            for (const auto i : groups[g.group])
                name (i, toChorus ? S::chorus : S::verse);
        }
        // Played once: a bridge after the first chorus, else by its size.
        auto firstChorus = -1;
        for (auto i = 0; i < count; ++i)
            if (named[(size_t) i] && sections[(size_t) i].kind == S::chorus && sections[(size_t) i].used)
            {
                firstChorus = i;
                break;
            }
        for (auto i = 0; i < count; ++i)
            if (! named[(size_t) i])
            {
                const auto e = sums[(size_t) i].energy;
                if (firstChorus >= 0 && i > firstChorus)
                    name (i, S::bridge);
                else if (chorusEnergy >= 0.0f && verseEnergy >= 0.0f)
                    name (i, std::abs (e - chorusEnergy) < std::abs (e - verseEnergy) ? S::chorus : S::verse);
                else
                    name (i, e > median ? S::chorus : S::verse);
            }
    }

    //==========================================================================
    void setSectionKind (DrumSong& song, int index, DrumSection kind)
    {
        if (juce::isPositiveAndBelow (index, (int) song.sections.size()))
            song.sections[(size_t) index].kind = kind;
    }

    void setSectionUsed (DrumSong& song, int index, bool used)
    {
        if (juce::isPositiveAndBelow (index, (int) song.sections.size()))
            song.sections[(size_t) index].used = used;
    }

    bool splitSection (DrumSong& song, int index, int atBar)
    {
        if (! juce::isPositiveAndBelow (index, (int) song.sections.size()))
            return false;
        auto& s = song.sections[(size_t) index];
        if (atBar <= s.firstBar || atBar >= s.firstBar + s.bars)
            return false;
        DrumSongSection after = s;
        after.firstBar = atBar;
        after.bars = s.firstBar + s.bars - atBar;
        s.bars = atBar - s.firstBar;
        song.sections.insert (song.sections.begin() + index + 1, after);
        return true;
    }

    bool joinSections (DrumSong& song, int index)
    {
        if (index < 0 || index + 1 >= (int) song.sections.size())
            return false;
        song.sections[(size_t) index].bars += song.sections[(size_t) index + 1].bars;
        song.sections.erase (song.sections.begin() + index + 1);
        return true;
    }

    bool moveSectionStart (DrumSong& song, int index, int firstBar)
    {
        if (index <= 0 || index >= (int) song.sections.size())
            return false;
        auto& before = song.sections[(size_t) index - 1];
        auto& s = song.sections[(size_t) index];
        const auto end = s.firstBar + s.bars;
        if (firstBar <= before.firstBar || firstBar >= end)
            return false;
        before.bars = firstBar - before.firstBar;
        s.firstBar = firstBar;
        s.bars = end - firstBar;
        return true;
    }

    juce::String sectionsToText (const std::vector<DrumSongSection>& sections)
    {
        juce::StringArray parts;
        for (const auto& s : sections)
            parts.add (juce::String (s.firstBar) + ":" + kindId (s.kind) + ":" + juce::String (s.bars) + ":" + (s.used ? "1" : "0"));
        return parts.joinIntoString (";");
    }

    std::optional<DrumSection> drumSectionNamed (const juce::String& text)
    {
        for (const auto s : drumSections())
            if (drumSectionName (s).equalsIgnoreCase (text.trim()))
                return s;
        return std::nullopt;
    }

    std::optional<std::vector<DrumSongSection>> sectionsFromText (const juce::String& text, int totalBars)
    {
        std::vector<DrumSongSection> out;
        auto next = 0;
        for (const auto& token : juce::StringArray::fromTokens (text, ";", ""))
        {
            const auto p = juce::StringArray::fromTokens (token, ":", "");
            if (p.size() != 4)
                return std::nullopt;
            const auto kind = drumSectionNamed (p[1]);
            DrumSongSection s { p[0].getIntValue(), p[2].getIntValue(), kind.value_or (DrumSection::verse), p[3].trim() == "1", 0 };
            if (! kind.has_value() || s.firstBar != next || s.bars < 1)
                return std::nullopt;
            next += s.bars;
            out.push_back (s);
        }
        if (out.empty() || next != totalBars)
            return std::nullopt;
        return out;
    }

    LineIdea sectionIdea (const DrumSong& song, int index)
    {
        LineIdea idea { LineIdea::Part::drums, {}, {}, {} };
        if (! juce::isPositiveAndBelow (index, (int) song.sections.size()))
            return idea;
        const auto& s = song.sections[(size_t) index];
        idea.style = drumSectionName (s.kind);
        idea.description = song.name;
        const auto origin = song.bars[(size_t) s.firstBar].start;
        for (auto b = s.firstBar; b < s.firstBar + s.bars && b < (int) song.bars.size(); ++b)
        {
            const auto& bar = song.bars[(size_t) b];
            for (const auto& h : bar.hits)
                idea.notes.push_back ({ bar.start - origin + h.step * bar.length / learnSteps, bar.length / 24.0, h.note, (float) h.velocity / 127.0f });
        }
        return idea;
    }

    //==========================================================================
    int DrumProfile::barsLearned() const
    {
        auto n = 0;
        for (const auto& s : sections)
            n += s.bars;
        return n;
    }

    std::unique_ptr<juce::XmlElement> DrumProfile::toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement> ("DrummerProfile");
        xml->setAttribute ("name", name);
        xml->setAttribute ("version", 1);
        for (const auto& song : songs)
        {
            auto* e = xml->createNewChildElement ("Song");
            e->setAttribute ("file", song.path);
            e->setAttribute ("sections", song.sections);
            e->setAttribute ("use", song.use);
        }
        for (const auto kind : drumSections())
        {
            const auto& h = of (kind);
            if (h.bars <= 0)
                continue;
            auto* e = xml->createNewChildElement ("Section");
            e->setAttribute ("kind", kindId (kind));
            e->setAttribute ("bars", h.bars);
            e->setAttribute ("sections", h.sections);
            e->setAttribute ("fillChance", h.fillChance);
            e->setAttribute ("crashChance", h.crashChance);
            e->setAttribute ("halfTime", h.halfTime);
            e->setAttribute ("cymbal", h.cymbal);
            e->setAttribute ("kick", arrayToText (h.kick));
            e->setAttribute ("snare", arrayToText (h.snare));
            e->setAttribute ("ghost", arrayToText (h.ghost));
            e->setAttribute ("push", arrayToText (h.push));
            for (const auto& g : h.grooves)
            {
                auto* p = e->createNewChildElement ("Groove");
                p->setAttribute ("count", g.count);
                p->setAttribute ("hits", hitsToText (g.hits));
            }
            for (const auto& f : h.fills)
            {
                auto* p = e->createNewChildElement ("Fill");
                p->setAttribute ("count", f.count);
                p->setAttribute ("beats", f.beats);
                p->setAttribute ("hits", hitsToText (f.hits));
            }
        }
        return xml;
    }

    std::optional<DrumProfile> DrumProfile::fromXml (const juce::XmlElement& xml)
    {
        if (! xml.hasTagName ("DrummerProfile"))
            return std::nullopt;
        DrumProfile p;
        p.name = xml.getStringAttribute ("name");
        for (const auto* e : xml.getChildWithTagNameIterator ("Song"))
            p.songs.push_back ({ e->getStringAttribute ("file"), e->getStringAttribute ("sections"), e->getBoolAttribute ("use", true) });
        for (const auto* e : xml.getChildWithTagNameIterator ("Section"))
        {
            const auto kind = drumSectionNamed (e->getStringAttribute ("kind"));
            if (! kind.has_value())
                continue;
            auto& h = p.sections[(size_t) *kind];
            h.bars = juce::jmax (0, e->getIntAttribute ("bars"));
            h.sections = e->getIntAttribute ("sections");
            h.fillChance = (float) e->getDoubleAttribute ("fillChance");
            h.crashChance = (float) e->getDoubleAttribute ("crashChance");
            h.halfTime = (float) e->getDoubleAttribute ("halfTime");
            h.cymbal = e->getIntAttribute ("cymbal", -1);
            h.kick = arrayFromText<16, float> (e->getStringAttribute ("kick"));
            h.snare = arrayFromText<16, float> (e->getStringAttribute ("snare"));
            h.ghost = arrayFromText<16, float> (e->getStringAttribute ("ghost"));
            h.push = arrayFromText<3, double> (e->getStringAttribute ("push"));
            for (const auto* g : e->getChildWithTagNameIterator ("Groove"))
                h.grooves.push_back ({ hitsFromText (g->getStringAttribute ("hits")), g->getIntAttribute ("count"), 0 });
            for (const auto* f : e->getChildWithTagNameIterator ("Fill"))
                h.fills.push_back ({ hitsFromText (f->getStringAttribute ("hits")), f->getIntAttribute ("count"), juce::jlimit (1, 2, f->getIntAttribute ("beats", 1)) });
        }
        return p;
    }

    bool DrumProfile::save (const juce::File& file) const
    {
        file.getParentDirectory().createDirectory();
        return toXml()->writeTo (file);
    }

    std::optional<DrumProfile> DrumProfile::load (const juce::File& file)
    {
        if (const auto xml = juce::XmlDocument::parse (file))
            return fromXml (*xml);
        return std::nullopt;
    }

    DrumProfile learnProfile (const juce::String& name, const std::vector<DrumSong>& songs)
    {
        struct Counted { std::vector<LearnedHit> hits; std::vector<int> velocitySums; int count = 0; int beats = 0; };
        struct Acc
        {
            std::map<juce::String, Counted> grooves, fills;
            std::array<int, 16> kick {}, snare {}, ghost {};
            int bars = 0, grooveBars = 0, sections = 0, phrases = 0, crashes = 0, fillBars = 0, half = 0;
            std::map<int, int> cymbals;
            std::array<double, 3> push {};
            std::array<int, 3> pushBars {};
        };
        std::array<Acc, DrumProfile::sectionCount> acc;
        const auto add = [] (std::map<juce::String, Counted>& into, const std::vector<LearnedHit>& hits, int beats)
        {
            juce::StringArray key;
            for (const auto& h : hits)
                key.add (juce::String (h.step) + ":" + juce::String (h.note));
            auto& c = into[juce::String (beats) + "|" + key.joinIntoString (" ")];
            if (c.count == 0)
            {
                c.hits = hits;
                c.velocitySums.assign (hits.size(), 0);
                c.beats = beats;
            }
            for (size_t i = 0; i < hits.size(); ++i)
                c.velocitySums[i] += hits[i].velocity;
            ++c.count;
        };

        DrumProfile profile;
        profile.name = name;
        for (const auto& song : songs)
        {
            profile.songs.push_back ({ song.file.getFullPathName(), sectionsToText (song.sections), song.use });
            if (! song.use)
                continue;
            for (const auto& section : song.sections)
            {
                if (! section.used)
                    continue;
                auto& a = acc[(size_t) section.kind];
                ++a.sections;
                for (auto b = section.firstBar; b < section.firstBar + section.bars && b < (int) song.bars.size(); ++b)
                {
                    const auto& bar = song.bars[(size_t) b];
                    if (bar.silent())
                        continue;
                    ++a.bars;
                    if ((b - section.firstBar) % 4 == 0)
                    {
                        ++a.phrases;
                        a.crashes += bar.crashAtStart ? 1 : 0;
                    }
                    a.half += bar.halfTime ? 1 : 0;
                    if (bar.cymbal >= 0)
                        ++a.cymbals[bar.cymbal];
                    const auto fillFrom = bar.twoBeatFill ? 24 : bar.fill ? 36 : learnSteps;
                    for (const auto& h : bar.hits)
                        if (h.step < fillFrom)
                        {
                            const auto s = (size_t) sixteenthOf (h.step);
                            if (h.note == kick) ++a.kick[s];
                            else if (isGhost (h)) ++a.ghost[s];
                            else if (isSnareNote (h.note)) ++a.snare[s];
                        }
                    if (bar.fill)
                    {
                        ++a.fillBars;
                        std::vector<LearnedHit> fill;
                        for (const auto& h : bar.hits)
                            if (h.step >= fillFrom)
                                fill.push_back ({ h.step - fillFrom, h.note, h.velocity });
                        add (a.fills, fill, bar.twoBeatFill ? 2 : 1);
                    }
                    else
                    {
                        ++a.grooveBars;
                        // The groove without the crash that marks a phrase (unless the crash is what's ridden).
                        std::vector<LearnedHit> groove;
                        for (const auto& h : bar.hits)
                            if (! (h.step <= 1 && isCrashNote (h.note) && h.note != bar.cymbal))
                                groove.push_back (h);
                        if (! groove.empty())
                            add (a.grooves, groove, 0);
                    }
                }
                const auto barsHere = (double) section.bars;
                for (size_t r = 0; r < 3; ++r)
                    if (song.pushCount[r] > 0)
                    {
                        a.push[r] += song.pushSum[r] / song.pushCount[r] * barsHere;
                        a.pushBars[r] += section.bars;
                    }
            }
        }
        const auto finish = [] (const std::map<juce::String, Counted>& from)
        {
            std::vector<SectionHabits::Pattern> out;
            for (const auto& [key, c] : from)
            {
                SectionHabits::Pattern p { c.hits, c.count, c.beats };
                for (size_t i = 0; i < p.hits.size(); ++i)
                    p.hits[i].velocity = juce::jlimit (1, 127, (int) std::lround ((double) c.velocitySums[i] / c.count));
                out.push_back (std::move (p));
            }
            std::stable_sort (out.begin(), out.end(), [] (const SectionHabits::Pattern& a, const SectionHabits::Pattern& b) { return a.count > b.count; });
            if (out.size() > 12)
                out.resize (12);
            return out;
        };
        for (const auto kind : drumSections())
        {
            const auto& a = acc[(size_t) kind];
            auto& h = profile.sections[(size_t) kind];
            if (a.bars == 0)
                continue;
            h.bars = a.bars;
            h.sections = a.sections;
            h.grooves = finish (a.grooves);
            h.fills = finish (a.fills);
            const auto per = (float) juce::jmax (1, a.bars);
            for (size_t s = 0; s < 16; ++s)
            {
                h.kick[s] = (float) a.kick[s] / per;
                h.snare[s] = (float) a.snare[s] / per;
                h.ghost[s] = (float) a.ghost[s] / per;
            }
            h.fillChance = (float) a.fillBars / per;
            h.crashChance = a.phrases > 0 ? (float) a.crashes / (float) a.phrases : 0.0f;
            h.halfTime = (float) a.half / per;
            auto most = 0;
            for (const auto& [note, count] : a.cymbals)
                if (count > most)
                {
                    most = count;
                    h.cymbal = note;
                }
            for (size_t r = 0; r < 3; ++r)
                h.push[r] = a.pushBars[r] > 0 ? a.push[r] / a.pushBars[r] : 0.0;
        }
        return profile;
    }

    juce::File defaultDrummerFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Paragon PR7-S").getChildFile ("Drummer Profiles");
    }

    std::vector<juce::String> drummerNames (const juce::File& folder)
    {
        std::vector<juce::String> names;
        for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.pr7drummer"))
            names.push_back (f.getFileNameWithoutExtension());
        std::sort (names.begin(), names.end(), [] (const juce::String& a, const juce::String& b) { return a.compareNatural (b) < 0; });
        return names;
    }

    juce::File drummerFile (const juce::File& folder, const juce::String& name)
    {
        return folder.getChildFile (juce::File::createLegalFileName (name.trim().isEmpty() ? juce::String ("Drummer") : name.trim()) + ".pr7drummer");
    }

    LineIdea generateBeat (const DrumProfile& profile, DrumSection section, const std::vector<ChordSpan>& spans, double beatsPerBar,
                           const DrumSettings& settings, juce::uint32 seed, const std::vector<double>& riffHits)
    {
        LineIdea idea { LineIdea::Part::drums, drumSectionName (section).substring (0, 1) + drumSectionName (section).substring (1).toLowerCase(), {}, {} };
        const auto* h = habitsFor (profile, section);
        if (h == nullptr || spans.empty() || beatsPerBar <= 0.0)
        {
            idea.description = profile.name + ": nothing learned yet";
            return idea;
        }
        const auto own = h == &profile.of (section);
        const auto s = settings.limited();
        const auto total = spans.back().end - spans.front().start;
        const auto bars = juce::jmax (1, (int) std::ceil (total / beatsPerBar - 1.0e-9));
        const auto stepBeats = beatsPerBar / learnSteps;
        juce::Random rng ((juce::int64) seed * 2654435761LL + (int) section * 97 + 11);

        // Their groove for the section, kept; the third bar of a phrase another of theirs (or a kick moved).
        const auto* first = pick (h->grooves, rng);
        const auto groove = shape (first->hits, *h, s, rng);
        const auto* second = h->grooves.size() > 1 && rng.nextBool() ? pick (h->grooves, rng, first) : nullptr;
        const auto variation = second != nullptr ? shape (second->hits, *h, s, rng) : varyKick (groove, *h, rng);
        const auto locked = s.lockToRiff && ! riffHits.empty() && (s.lockAlways || (section != DrumSection::intro && section != DrumSection::ending));

        const auto fillOf = [&] (int beats) -> const SectionHabits::Pattern*
        {
            std::vector<SectionHabits::Pattern> wanted;
            for (const auto& f : h->fills)
                if (f.beats == beats)
                    wanted.push_back (f);
            const auto& from = wanted.empty() ? h->fills : wanted;
            if (from.empty())
                return nullptr;
            const auto* chosen = pick (from, rng);
            for (const auto& f : h->fills)
                if (f.hits == chosen->hits && f.beats == chosen->beats)
                    return &f;
            return nullptr;
        };

        for (auto bar = 0; bar < bars; ++bar)
        {
            auto hits = bar % 4 == 2 ? variation : groove;
            if (locked)
            {
                hits.erase (std::remove_if (hits.begin(), hits.end(), [] (const LearnedHit& x) { return x.note == kick; }), hits.end());
                for (const auto r : riffHits)
                    if (r >= bar * beatsPerBar - 1.0e-9 && r < (bar + 1) * beatsPerBar - 1.0e-9)
                        hits.push_back ({ juce::jlimit (0, learnSteps - 1, (int) std::lround ((r - bar * beatsPerBar) / stepBeats)), kick, 104 });
            }
            const auto last = bar == bars - 1;
            if (section == DrumSection::ending && last)
                hits = { { 0, kick, 120 }, { 0, crash, 120 }, { 0, crash2, 112 }, { 0, snare, 110 } };
            else if ((s.fillEvery > 0 && (bar + 1) % s.fillEvery == 0 && section != DrumSection::ending) || (section == DrumSection::ending && bar == bars - 2))
            {
                const auto beats = section == DrumSection::ending ? 2 : s.fillBeats;
                const auto from = learnSteps - beats * 12;
                hits.erase (std::remove_if (hits.begin(), hits.end(), [from] (const LearnedHit& x) { return x.step >= from; }), hits.end());
                if (const auto* fill = fillOf (beats))
                {
                    const auto shift = from + (fill->beats < beats ? 12 * (beats - fill->beats) : 0);
                    for (const auto& x : fill->hits)
                        if (shift + x.step < learnSteps)
                            hits.push_back ({ shift + x.step, x.note, x.velocity });
                }
                else
                    for (auto step = from; step < learnSteps; step += 3)
                        hits.push_back ({ step, snare, 84 + 34 * (step - from) / juce::jmax (1, learnSteps - from) });
            }
            // A crash where a phrase begins, as often as the drummer did (always after a fill).
            const auto afterFill = bar > 0 && s.fillEvery > 0 && bar % s.fillEvery == 0;
            const auto phraseStart = bar % 4 == 0 && ! (section == DrumSection::intro && bar == 0);
            if ((phraseStart && rng.nextFloat() < juce::jmax (0.25f, h->crashChance)) || afterFill)
                if (std::none_of (hits.begin(), hits.end(), [] (const LearnedHit& x) { return x.step <= 1 && isCrashNote (x.note); }))
                {
                    hits.push_back ({ 0, bar % 8 == 0 ? crash : crash2, 112 });
                    if (! has (hits, 0, kick))
                        hits.push_back ({ 0, kick, 112 });
                }
            for (const auto& x : hits)
            {
                const auto role = pushRole (x);
                const auto at = juce::jmax (0.0, bar * beatsPerBar + x.step * stepBeats + (role >= 0 ? h->push[(size_t) role] : 0.0));
                if (at < total - 1.0e-9)
                    idea.notes.push_back ({ at, juce::jmin (stepBeats * 6.0, total - at), x.note, (float) juce::jlimit (1, 127, x.velocity) / 127.0f });
            }
        }
        std::stable_sort (idea.notes.begin(), idea.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });

        juce::StringArray words;
        const auto learnedAs = std::find_if (profile.sections.begin(), profile.sections.end(), [h] (const SectionHabits& x) { return &x == h; }) - profile.sections.begin();
        words.add (own ? juce::String (h->bars) + " " + drumSectionName (section).toLowerCase() + " bars learned"
                       : "from their " + drumSectionName ((DrumSection) learnedAs).toLowerCase() + " (no " + drumSectionName (section).toLowerCase() + " learned)");
        words.add ("their groove played " + juce::String (first->count) + (first->count == 1 ? " time" : " times"));
        if (locked)
            words.add ("the kick on the riff's hits");
        words.add (cymbalWord (s.cymbal == DrumSettings::Cymbal::automatic ? h->cymbal : s.cymbal == DrumSettings::Cymbal::hats ? closedHat
                                                                                       : s.cymbal == DrumSettings::Cymbal::ride ? ride : china));
        if (h->halfTime > 0.5f)
            words.add ("half time");
        if (! h->fills.empty() && (s.fillEvery > 0 || section == DrumSection::ending))
            words.add ("their fills");
        idea.description = profile.name + ": " + words.joinIntoString (", ");
        return idea;
    }

    std::vector<LineIdea> generatedBeats (const DrumProfile& profile, const std::vector<ChordSpan>& spans, double beatsPerBar,
                                          const DrumSettings& settings, int variation, const std::vector<double>& riffHits)
    {
        std::vector<LineIdea> out;
        for (const auto section : drumSections())
            out.push_back (generateBeat (profile, section, spans, beatsPerBar, settings, (juce::uint32) (variation * 7919 + 1), riffHits));
        return out;
    }
}
