#include "engine/Signature.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace pr7s
{
    namespace
    {
        constexpr double hair = 1.0e-6;
    }

    bool isValidSignature (int numerator, int denominator) noexcept
    {
        return numerator >= 1 && numerator <= 32
            && (denominator == 1 || denominator == 2 || denominator == 4 || denominator == 8 || denominator == 16 || denominator == 32);
    }

    bool parseSignature (const char* text, int& numerator, int& denominator)
    {
        if (text == nullptr)
            return false;
        char* end = nullptr;
        const auto n = std::strtol (text, &end, 10);
        while (end != nullptr && (*end == ' ' || *end == '/' || *end == '\t'))
            ++end;
        if (end == nullptr || *end == 0)
            return false;
        char* tail = nullptr;
        const auto d = std::strtol (end, &tail, 10);
        if (tail == end || ! isValidSignature ((int) n, (int) d))
            return false;
        numerator = (int) n;
        denominator = (int) d;
        return true;
    }

    SignatureMap::SignatureMap (int numerator, int denominator, const std::vector<SignatureEvent>& changes)
    {
        if (! isValidSignature (numerator, denominator))
            numerator = 4, denominator = 4;
        std::vector<SignatureEvent> sorted;
        for (const auto& e : changes)
            if (e.bar > 0 && isValidSignature (e.numerator, e.denominator))
            {
                auto same = std::find_if (sorted.begin(), sorted.end(), [&] (const SignatureEvent& s) { return s.bar == e.bar; });
                if (same != sorted.end())
                    *same = e;
                else
                    sorted.push_back (e);
            }
        std::sort (sorted.begin(), sorted.end(), [] (const SignatureEvent& a, const SignatureEvent& b) { return a.bar < b.bar; });
        list.push_back ({ { 0, numerator, denominator }, 0.0 });
        for (const auto& e : sorted)
        {
            const auto& last = list.back();
            list.push_back ({ e, last.beat + (e.bar - last.event.bar) * last.event.barLength() });
        }
    }

    const SignatureMap::Segment& SignatureMap::segmentAt (double beat) const noexcept
    {
        // The last segment starting at or before the beat (a hair before counts as on it).
        auto it = std::upper_bound (list.begin(), list.end(), beat + hair, [] (double b, const Segment& s) { return b < s.beat; });
        return it == list.begin() ? list.front() : *(it - 1);
    }

    const SignatureMap::Segment& SignatureMap::segmentOfBar (int bar) const noexcept
    {
        auto it = std::upper_bound (list.begin(), list.end(), bar, [] (int b, const Segment& s) { return b < s.event.bar; });
        return it == list.begin() ? list.front() : *(it - 1);
    }

    double SignatureMap::beatOfBar (int bar) const noexcept
    {
        const auto& s = segmentOfBar (bar);
        return s.beat + (bar - s.event.bar) * s.event.barLength();
    }

    int SignatureMap::barAt (double beat) const noexcept
    {
        const auto& s = segmentAt (beat);
        return s.event.bar + (int) std::floor ((beat - s.beat) / s.event.barLength() + hair);
    }

    bool SignatureMap::isBarLine (double beat) const noexcept
    {
        return std::abs (beatOfBar (barAt (beat)) - beat) < hair * 4.0;
    }

    SignatureMap::Position SignatureMap::positionOf (double beat) const noexcept
    {
        Position p;
        p.bar = barAt (beat);
        const auto& s = segmentOfBar (p.bar);
        auto within = std::max (0.0, beat - beatOfBar (p.bar));
        p.beat = (int) std::floor (within / s.event.unit() + hair);
        p.beat = std::clamp (p.beat, 0, s.event.numerator - 1);
        p.rest = std::max (0.0, within - p.beat * s.event.unit());
        return p;
    }

    double SignatureMap::beatOf (int bar, int beat, double rest) const noexcept
    {
        return beatOfBar (bar) + beat * segmentOfBar (bar).event.unit() + rest;
    }
}
