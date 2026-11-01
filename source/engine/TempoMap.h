#pragma once

#include <vector>

// The tempo track (0.10.0, the owner asked: "exactly like how Cubase does it").
//
// A tempo event: from its beat, the song's tempo. `ramp`: the tempo gets
// there gradually from the event before (the manual's Ramp), else it jumps
// there (Step). The first event is at beat 0 and can't be removed.
//
// A TempoMap turns beats into seconds and back with the events: between two
// events the tempo is constant (step) or changes evenly beat by beat (ramp),
// and the seconds over a ramp are exact (the time of a beat is 60 / tempo, so
// over an even change in tempo they add up to a logarithm), not a staircase.
// Before beat 0 and after the last event the tempo stays as it is there.
namespace pr7s
{
    struct TempoEvent
    {
        double beat = 0.0;
        double bpm = 120.0;
        bool ramp = false;   // gradually from the event before (the first one's is ignored)

        bool operator== (const TempoEvent& other) const noexcept
        {
            return beat == other.beat && bpm == other.bpm && ramp == other.ramp;   // NOLINT (exact: saved and undone as they are)
        }
    };

    class TempoMap
    {
    public:
        explicit TempoMap (double bpm = 120.0);
        // Sorted by beat; the first is moved to beat 0. Empty: 120.
        explicit TempoMap (std::vector<TempoEvent> events);

        double secondsAt (double beat) const noexcept;
        double beatAt (double seconds) const noexcept;
        double bpmAt (double beat) const noexcept;
        bool isConstant() const noexcept { return segments.size() == 1; }
        const std::vector<TempoEvent>& getEvents() const noexcept { return events; }

        // The seconds between two beats.
        double secondsBetween (double fromBeat, double toBeat) const noexcept { return secondsAt (toBeat) - secondsAt (fromBeat); }
        // The beat `seconds` after `fromBeat`.
        double beatAfter (double fromBeat, double seconds) const noexcept { return beatAt (secondsAt (fromBeat) + seconds); }

    private:
        struct Segment
        {
            double beat = 0.0, seconds = 0.0;   // where it starts
            double bpm = 120.0;                 // the tempo there
            double slope = 0.0;                 // BPM per beat (a ramp), else 0
        };
        std::vector<TempoEvent> events;
        std::vector<Segment> segments;
        const Segment& segmentForBeat (double beat) const noexcept;
        const Segment& segmentForSeconds (double seconds) const noexcept;
    };
}
