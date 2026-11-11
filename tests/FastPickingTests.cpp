// 0.83.3 (the owner: the rhythm guitar's fast picking lost notes). DIs made the way a picked string sounds: each
// pick starts the string again (its harmonics back at full, the high ones dying fast), a short bright pick noise,
// and on a ringing string hardly any drop in level between picks: so a pick can't be found by loudness alone.

#include <catch2/catch_test_macros.hpp>

#include "engine/BandRecord.h"

#include <iostream>
#include <random>

using namespace pr7s;

namespace
{
    constexpr double rate = 44100.0;
    double hzOf (double midi) { return 440.0 * std::pow (2.0, (midi - 69.0) / 12.0); }

    struct Picking
    {
        double from, step;     // the first pick, seconds between picks
        int count;
        double midi;
        bool fifth = false;    // a power chord
        bool muted = false;    // palm muted (dies in ~40 ms)
        bool contactDip = false;   // the pick stopping the string for a moment before it sounds
        float accentEvery = 0; // every n-th pick harder (0: all alike)
    };

    // One run of picks on a string: the string's phase carries on, its harmonics restart at each pick.
    juce::AudioBuffer<float> picked (const std::vector<Picking>& runs, double seconds, unsigned seed = 7)
    {
        juce::AudioBuffer<float> sound (1, (int) (seconds * rate));
        sound.clear();
        auto* out = sound.getWritePointer (0);
        std::mt19937 random (seed);
        std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
        for (const auto& run : runs)
            for (const auto offset : run.fifth ? std::vector<double> { 0.0, 7.0 } : std::vector<double> { 0.0 })
            {
                const auto f = hzOf (run.midi + offset);
                std::vector<double> phase (16, 0.0);
                const auto end = (int) ((run.from + run.step * run.count + (run.muted ? 0.05 : 0.6)) * rate);
                for (int i = (int) (run.from * rate); i < juce::jmin (end, sound.getNumSamples()); ++i)
                {
                    const auto t = i / rate - run.from;
                    const auto k = juce::jmin (run.count - 1, (int) std::floor (t / run.step));
                    const auto since = t - k * run.step;
                    const auto hard = run.accentEvery > 0 && k % (int) run.accentEvery == 0 ? 1.0 : 0.8;
                    const auto body = run.muted ? std::exp (-since / 0.04) : std::exp (-since / 1.2);
                    auto sample = 0.0;
                    for (int h = 1; h <= 16; ++h)
                    {
                        phase[(size_t) h - 1] += juce::MathConstants<double>::twoPi * f * h / rate;
                        const auto shape = run.muted ? std::exp (-(h - 1) * 0.7) : 1.0 / h;
                        sample += std::sin (phase[(size_t) h - 1]) * shape * std::exp (-since * (h - 1) * (run.muted ? 30.0 : 14.0));
                    }
                    auto gain = body * hard;
                    if (run.contactDip && k > 0 && since < 0.003)
                        gain *= 0.3;
                    out[i] += (float) (sample * gain * 0.25);
                    if (offset == 0.0 && since < 0.0025)
                        out[i] += noise (random) * 0.05f * (float) (1.0 - since / 0.0025);
                }
            }
        return sound;
    }

    struct Score { int expected = 0, found = 0, matched = 0; double worstMs = 0; };

    Score score (const std::vector<PlayedNote>& notes, const std::vector<double>& starts)
    {
        Score s;
        s.expected = (int) starts.size();
        s.found = (int) notes.size();
        for (const auto t : starts)
        {
            auto best = 1.0e9;
            for (const auto& n : notes)
                best = juce::jmin (best, std::abs (n.start - t));
            if (best < 0.02)
            {
                ++s.matched;
                s.worstMs = juce::jmax (s.worstMs, best * 1000.0);
            }
        }
        return s;
    }

    std::vector<double> startsOf (const Picking& run)
    {
        std::vector<double> starts;
        for (int k = 0; k < run.count; ++k)
            starts.push_back (run.from + k * run.step);
        return starts;
    }

    void report (const char* what, const Score& s)
    {
        std::cout << what << ": " << s.matched << " of " << s.expected << " picks found (" << s.found << " notes, worst "
                  << s.worstMs << " ms off)\n";
    }
}

TEST_CASE ("0.83.3: fast picking on a rhythm guitar DI: every pick a note", "[band][picking]")
{
    // 16ths at 180 BPM (83 ms) open single notes, the string ringing: no drop in level between picks.
    const Picking open16 { 0.2, 60.0 / 180.0 / 4.0, 24, 33.0 };
    // Tremolo: 32nds at 140 BPM (54 ms) on an open A5 power chord.
    const Picking tremolo { 0.2, 60.0 / 140.0 / 8.0, 36, 33.0, true };
    // Palm-muted 16th chugs at 200 BPM (75 ms) with an accent every four.
    const Picking chugs { 0.2, 60.0 / 200.0 / 4.0, 32, 33.0, false, true, false, 4 };
    // 16ths at 160 BPM (94 ms), an open D5 power chord, the pick touching the string first.
    const Picking dipped { 0.2, 60.0 / 160.0 / 4.0, 20, 38.0, true, false, true };
    for (const auto& [name, run] : { std::pair { "open 16ths at 180", open16 }, { "tremolo 32nds at 140", tremolo },
                                     { "muted 16ths at 200", chugs }, { "power chord 16ths at 160", dipped } })
    {
        const auto seconds = run.from + run.step * run.count + 0.8;
        const auto notes = transcribeRhythm (picked ({ run }, seconds), rate, 33, 76);
        const auto s = score (notes, startsOf (run));
        report (name, s);
        CHECK (s.matched >= s.expected - 1);
        CHECK (s.found <= s.expected + 1);
        CHECK (s.worstMs <= 12.0);
        for (const auto& n : notes)
            CHECK ((n.pitch == (int) run.midi || n.pitch == (int) run.midi + 12));
        auto wrong = 0;   // (muted read as open, or open as muted)
        for (const auto& n : notes)
            wrong += n.muted != run.muted ? 1 : 0;
        std::cout << "  " << wrong << " read " << (run.muted ? "open" : "muted") << "\n";
        CHECK (wrong <= 1);
    }
}

TEST_CASE ("0.83.3: a held note isn't split, and a slow riff keeps its hits", "[band][picking]")
{
    // A power chord let ring for 1.5 s: one note.
    const Picking held { 0.2, 1.5, 1, 33.0, true };
    auto notes = transcribeRhythm (picked ({ held }, 2.5), rate, 33, 76);
    CHECK (notes.size() == 1);
    // Quarter notes at 100 BPM, muted: four hits.
    const Picking slow { 0.2, 0.6, 4, 40.0, false, true };
    notes = transcribeRhythm (picked ({ slow }, 3.0), rate, 33, 76);
    CHECK (score (notes, startsOf (slow)).matched == 4);
    CHECK (notes.size() == 4);
}
