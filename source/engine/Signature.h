#pragma once

#include <vector>

// Time signature changes (0.45.0, the manual's Signature Track and Time Signature Events):
// a signature event sits at a bar, never between bars, and holds until the next. The first
// is always at bar 1 (index 0): the song's own numerator / denominator. Positions in the song
// stay in beats (quarter notes); a map turns them into bars and back.
namespace pr7s
{
    struct SignatureEvent
    {
        int bar = 0;                 // from 0 (bar 1 on screen)
        int numerator = 4, denominator = 4;
        double barLength() const noexcept { return numerator * 4.0 / denominator; }   // in beats
        double unit() const noexcept { return 4.0 / denominator; }   // the signature's beat (a quarter in 4/4, an eighth in 6/8)
        bool operator== (const SignatureEvent&) const = default;
    };

    class SignatureMap
    {
    public:
        // `changes`: events after the first (any order; at bar 0 or before: ignored, duplicates: the last).
        SignatureMap (int numerator, int denominator, const std::vector<SignatureEvent>& changes);
        SignatureMap() : SignatureMap (4, 4, {}) {}

        struct Segment
        {
            SignatureEvent event;
            double beat = 0.0;   // where its first bar starts
        };
        const std::vector<Segment>& segments() const noexcept { return list; }

        const Segment& segmentAt (double beat) const noexcept;    // the one holding the beat (before 0: the first)
        const Segment& segmentOfBar (int bar) const noexcept;
        double beatOfBar (int bar) const noexcept;                 // where bar `bar` (from 0) starts; negative bars by the first
        int barAt (double beat) const noexcept;                    // the bar holding the beat (floored; a hair before a bar line counts as on it)
        double barStart (double beat) const noexcept { return beatOfBar (barAt (beat)); }
        double nextBarStart (double beat) const noexcept { return beatOfBar (barAt (beat) + 1); }
        double barLengthAt (double beat) const noexcept { return segmentAt (beat).event.barLength(); }
        double unitAt (double beat) const noexcept { return segmentAt (beat).event.unit(); }
        bool isBarLine (double beat) const noexcept;
        // The bar with its beat in it: bar from 0, the signature's beat from 0, and what's left in beats.
        struct Position { int bar = 0; int beat = 0; double rest = 0.0; };
        Position positionOf (double beat) const noexcept;
        double beatOf (int bar, int beat, double rest) const noexcept;   // (back again)
        bool isSimple() const noexcept { return list.size() == 1; }

    private:
        std::vector<Segment> list;   // sorted by bar, the first at bar 0
    };

    // A signature as text ("7/8"), and back (false: not one; numerators 1-32, denominators 1, 2, 4, 8, 16, 32).
    bool parseSignature (const char* text, int& numerator, int& denominator);
    bool isValidSignature (int numerator, int denominator) noexcept;
}
