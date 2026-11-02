#include "TempoMap.h"

#include <algorithm>
#include <cmath>

namespace pr7s
{
    namespace
    {
        constexpr double flat = 1.0e-9;   // a slope smaller than this: a constant tempo

        // Seconds from a segment's start to `beats` into it: 60 / tempo summed;
        // with the tempo changing evenly (T + k b), 60 / k * ln ((T + k b) / T).
        double secondsInto (double bpm, double slope, double beats) noexcept
        {
            if (std::abs (slope) < flat)
                return beats * 60.0 / bpm;
            return 60.0 / slope * std::log ((bpm + slope * beats) / bpm);
        }

        // Its inverse: the beats `seconds` into it.
        double beatsInto (double bpm, double slope, double seconds) noexcept
        {
            if (std::abs (slope) < flat)
                return seconds * bpm / 60.0;
            return bpm / slope * (std::exp (slope * seconds / 60.0) - 1.0);
        }
    }

    TempoMap::TempoMap (double bpm) : TempoMap (std::vector<TempoEvent> { TempoEvent { 0.0, bpm, false } }) {}

    TempoMap::TempoMap (std::vector<TempoEvent> list) : events (std::move (list))
    {
        if (events.empty())
            events.push_back ({});
        std::stable_sort (events.begin(), events.end(), [] (const TempoEvent& a, const TempoEvent& b) { return a.beat < b.beat; });
        events.front().beat = 0.0;
        events.front().ramp = false;
        for (auto& e : events)
            e.bpm = std::max (1.0, e.bpm);
        // Two at the same beat: the later one counts (a jump there).
        segments.reserve (events.size());
        auto seconds = 0.0;
        for (size_t i = 0; i < events.size(); ++i)
        {
            Segment segment;
            segment.beat = events[i].beat;
            segment.seconds = seconds;
            segment.bpm = events[i].bpm;
            if (i + 1 < events.size())
            {
                const auto length = events[i + 1].beat - events[i].beat;
                if (events[i + 1].ramp && length > flat)
                    segment.slope = (events[i + 1].bpm - events[i].bpm) / length;
                seconds += secondsInto (segment.bpm, segment.slope, length);
            }
            if (! segments.empty() && events[i].beat - segments.back().beat < flat)
                segments.back() = segment;   // (the same beat: the later one)
            else
                segments.push_back (segment);
        }
    }

    const TempoMap::Segment& TempoMap::segmentForBeat (double beat) const noexcept
    {
        // The last segment starting at or before the beat (the first for beats before 0).
        auto it = std::upper_bound (segments.begin(), segments.end(), beat, [] (double b, const Segment& s) { return b < s.beat; });
        return it == segments.begin() ? segments.front() : *(it - 1);
    }

    const TempoMap::Segment& TempoMap::segmentForSeconds (double seconds) const noexcept
    {
        auto it = std::upper_bound (segments.begin(), segments.end(), seconds, [] (double t, const Segment& s) { return t < s.seconds; });
        return it == segments.begin() ? segments.front() : *(it - 1);
    }

    double TempoMap::secondsAt (double beat) const noexcept
    {
        const auto& s = segmentForBeat (beat);
        if (beat < s.beat)   // before the song: its first tempo, going back
            return s.seconds + (beat - s.beat) * 60.0 / s.bpm;
        return s.seconds + secondsInto (s.bpm, s.slope, beat - s.beat);
    }

    double TempoMap::beatAt (double seconds) const noexcept
    {
        const auto& s = segmentForSeconds (seconds);
        if (seconds < s.seconds)
            return s.beat + (seconds - s.seconds) * s.bpm / 60.0;
        return s.beat + beatsInto (s.bpm, s.slope, seconds - s.seconds);
    }

    double TempoMap::bpmAt (double beat) const noexcept
    {
        const auto& s = segmentForBeat (beat);
        return beat < s.beat ? s.bpm : s.bpm + s.slope * (beat - s.beat);
    }
}
