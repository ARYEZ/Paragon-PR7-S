#include "engine/Eq.h"

namespace pr7s
{
    namespace
    {
        constexpr double pi = juce::MathConstants<double>::pi;

        Biquad normalised (double b0, double b1, double b2, double a0, double a1, double a2)
        {
            return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
        }

        double safeFrequency (double f, double rate) { return juce::jlimit (1.0, rate * 0.45, f); }

        Biquad cut (bool high, double frequency, double q, double rate)
        {
            // high: a high cut (low-pass); else a low cut (high-pass).
            const auto w = 2.0 * pi * safeFrequency (frequency, rate) / rate;
            const auto c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
            if (high)
                return normalised ((1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
            return normalised ((1.0 + c) / 2.0, -(1.0 + c), (1.0 + c) / 2.0, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
        }

        Biquad onePole (bool high, double frequency, double rate)
        {
            const auto k = std::tan (pi * safeFrequency (frequency, rate) / rate);
            if (high)
                return { k / (1.0 + k), k / (1.0 + k), 0.0, (k - 1.0) / (k + 1.0), 0.0 };
            return { 1.0 / (1.0 + k), -1.0 / (1.0 + k), 0.0, (k - 1.0) / (k + 1.0), 0.0 };
        }

        // A Butterworth cut of a slope (dB per octave) as stages: order = slope / 6, its pole
        // pairs' Qs 1 / (2 cos ((2k - 1) pi / 2n)) (6 dB: one pole; the others even). `resonance`
        // (0.46.0, a band's Q over 0.71) raises the sharpest pair's Q by that much.
        int addCut (std::array<Biquad, EqDesign::maxStages>& stages, int count, bool high, double frequency, int slope, double rate,
                    double resonance = 1.0)
        {
            const auto order = juce::jlimit (1, 16, slope / 6);
            if (order % 2 == 1 && count < EqDesign::maxStages)
                stages[(size_t) count++] = onePole (high, frequency, rate);
            for (int k = 1; k <= order / 2; ++k)
            {
                auto q = 1.0 / (2.0 * std::cos ((2.0 * k - 1.0) * pi / (2.0 * order)));
                if (k == order / 2)
                    q *= resonance;   // (the last pair has the highest Q)
                if (count < EqDesign::maxStages)
                    stages[(size_t) count++] = cut (high, frequency, q, rate);
            }
            return count;
        }

        int pcOfLetter (juce::juce_wchar c)
        {
            switch (juce::CharacterFunctions::toUpperCase (c))
            {
                case 'C': return 0; case 'D': return 2; case 'E': return 4; case 'F': return 5;
                case 'G': return 7; case 'A': return 9; case 'B': return 11; default: return -1;
            }
        }
    }

    const std::array<int, 5>& ChannelEq::bandSlopes()
    {
        static const std::array<int, 5> s { 6, 12, 24, 48, 96 };
        return s;
    }

    std::vector<EqBand::Type> ChannelEq::typesFor (int band)
    {
        using T = EqBand::Type;
        std::vector<T> types { T::lowShelf, T::parametric, T::highShelf, T::notch };
        if (band == 0)
            types.push_back (T::lowCut);
        if (band == 3)
            types.push_back (T::highCut);
        return types;
    }

    const std::array<int, 5>& ChannelEq::slopes()
    {
        static const std::array<int, 5> s { 6, 12, 24, 36, 48 };
        return s;
    }

    std::array<EqBand, 4> ChannelEq::defaultBands()
    {
        using T = EqBand::Type;
        return { EqBand { false, T::lowShelf, 0.0, 100.0, 0.71 }, EqBand { false, T::parametric, 0.0, 500.0, 1.0 },
                 EqBand { false, T::parametric, 0.0, 2000.0, 1.0 }, EqBand { false, T::highShelf, 0.0, 8000.0, 0.71 } };
    }

    bool ChannelEq::canBe (int band, EqBand::Type type)
    {
        // (0.46.0: shelves and the notch on every band; cuts on the outer two. A song's low cut on
        // band 4 or high cut on band 1 from before stays, so it sounds as it did.)
        return band == 0 || band == 3 || ! (type == EqBand::Type::lowCut || type == EqBand::Type::highCut);
    }

    ChannelEq ChannelEq::limited() const
    {
        auto e = *this;
        for (int i = 0; i < 4; ++i)
        {
            auto& b = e.bands[(size_t) i];
            if (! canBe (i, b.type))
                b.type = EqBand::Type::parametric;
            b.gainDb = juce::jlimit (-maxGainDb, maxGainDb, b.gainDb);
            b.frequency = juce::jlimit (minFrequency, maxFrequency, b.frequency);
            b.q = juce::jlimit (minQ, maxQ, b.q);
            auto best = 12;
            for (const auto v : bandSlopes())
                if (std::abs (v - b.slope) < std::abs (best - b.slope))
                    best = v;
            b.slope = best;
        }
        e.outputDb = juce::jlimit (-maxGainDb, maxGainDb, e.outputDb);
        e.lowCut = juce::jlimit (minFrequency, maxFrequency, e.lowCut);
        e.highCut = juce::jlimit (minHighCut, maxFrequency, e.highCut);
        const auto nearestSlope = [] (int s)
        {
            auto best = 12;
            for (const auto v : slopes())
                if (std::abs (v - s) < std::abs (best - s))
                    best = v;
            return best;
        };
        e.lowCutSlope = nearestSlope (e.lowCutSlope);
        e.highCutSlope = nearestSlope (e.highCutSlope);
        e.preGainDb = juce::jlimit (-maxGainDb, maxGainDb, e.preGainDb);
        return e;
    }

    bool ChannelEq::eqActive() const
    {
        return ! bypass && (outputDb != 0.0 || std::any_of (bands.begin(), bands.end(), [] (const EqBand& b) { return b.on; }));
    }

    bool ChannelEq::preActive() const
    {
        return lowCutOn || highCutOn || preGainDb != 0.0 || phaseInvert;
    }

    bool ChannelEq::isFlat() const
    {
        return ! eqActive() && ! preActive();
    }

    void ChannelEq::invert()
    {
        for (auto& b : bands)
            b.gainDb = -b.gainDb;
    }

    bool ChannelEq::operator== (const ChannelEq& o) const noexcept
    {
        return bypass == o.bypass && bands == o.bands && lowCutOn == o.lowCutOn && highCutOn == o.highCutOn && lowCut == o.lowCut
            && highCut == o.highCut && lowCutSlope == o.lowCutSlope && highCutSlope == o.highCutSlope && preGainDb == o.preGainDb
            && phaseInvert == o.phaseInvert && outputDb == o.outputDb;
    }

    std::unique_ptr<juce::XmlElement> ChannelEq::toXml (const juce::String& tag) const
    {
        auto xml = std::make_unique<juce::XmlElement> (tag);
        xml->setAttribute ("bypass", bypass);
        xml->setAttribute ("lowCutOn", lowCutOn);
        xml->setAttribute ("lowCut", lowCut);
        xml->setAttribute ("lowCutSlope", lowCutSlope);
        xml->setAttribute ("highCutOn", highCutOn);
        xml->setAttribute ("highCut", highCut);
        xml->setAttribute ("highCutSlope", highCutSlope);
        xml->setAttribute ("preGain", preGainDb);
        xml->setAttribute ("phase", phaseInvert);
        if (outputDb != 0.0)
            xml->setAttribute ("output", outputDb);
        for (const auto& b : bands)
        {
            auto* band = xml->createNewChildElement ("Band");
            band->setAttribute ("on", b.on);
            band->setAttribute ("type", (int) b.type);
            band->setAttribute ("gain", b.gainDb);
            band->setAttribute ("frequency", b.frequency);
            band->setAttribute ("q", b.q);
            if (b.slope != 12)
                band->setAttribute ("slope", b.slope);
            if (b.invert)
                band->setAttribute ("invert", true);
        }
        return xml;
    }

    ChannelEq ChannelEq::fromXml (const juce::XmlElement& xml)
    {
        ChannelEq e;
        e.bypass = xml.getBoolAttribute ("bypass");
        e.lowCutOn = xml.getBoolAttribute ("lowCutOn");
        e.lowCut = xml.getDoubleAttribute ("lowCut", e.lowCut);
        e.lowCutSlope = xml.getIntAttribute ("lowCutSlope", e.lowCutSlope);
        e.highCutOn = xml.getBoolAttribute ("highCutOn");
        e.highCut = xml.getDoubleAttribute ("highCut", e.highCut);
        e.highCutSlope = xml.getIntAttribute ("highCutSlope", e.highCutSlope);
        e.preGainDb = xml.getDoubleAttribute ("preGain");
        e.phaseInvert = xml.getBoolAttribute ("phase");
        e.outputDb = xml.getDoubleAttribute ("output", 0.0);
        auto i = 0;
        for (auto* band : xml.getChildWithTagNameIterator ("Band"))
        {
            if (i >= 4)
                break;
            auto& b = e.bands[(size_t) i++];
            b.on = band->getBoolAttribute ("on");
            b.type = (EqBand::Type) juce::jlimit (0, 5, band->getIntAttribute ("type", (int) b.type));
            b.slope = band->getIntAttribute ("slope", 12);
            b.invert = band->getBoolAttribute ("invert");
            b.gainDb = band->getDoubleAttribute ("gain", b.gainDb);
            b.frequency = band->getDoubleAttribute ("frequency", b.frequency);
            b.q = band->getDoubleAttribute ("q", b.q);
        }
        return e.limited();
    }

    std::complex<double> Biquad::response (double frequency, double rate) const
    {
        const auto w = 2.0 * pi * frequency / rate;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }

    Biquad bandFilter (const EqBand& b, double rate)
    {
        const auto w = 2.0 * pi * safeFrequency (b.frequency, rate) / rate;
        const auto c = std::cos (w), s = std::sin (w);
        // (0.46.0: a Notch is a Peak eight times narrower.)
        const auto q = juce::jmax (ChannelEq::minQ, b.q) * (b.type == EqBand::Type::notch ? 8.0 : 1.0);
        const auto alpha = s / (2.0 * q);
        const auto a = std::pow (10.0, b.playedGainDb() / 40.0);
        const auto root = 2.0 * std::sqrt (a) * alpha;
        switch (b.type)
        {
            case EqBand::Type::lowShelf:
                return normalised (a * ((a + 1) - (a - 1) * c + root), 2 * a * ((a - 1) - (a + 1) * c), a * ((a + 1) - (a - 1) * c - root),
                                   (a + 1) + (a - 1) * c + root, -2 * ((a - 1) + (a + 1) * c), (a + 1) + (a - 1) * c - root);
            case EqBand::Type::highShelf:
                return normalised (a * ((a + 1) + (a - 1) * c + root), -2 * a * ((a - 1) + (a + 1) * c), a * ((a + 1) + (a - 1) * c - root),
                                   (a + 1) - (a - 1) * c + root, 2 * ((a - 1) - (a + 1) * c), (a + 1) - (a - 1) * c - root);
            case EqBand::Type::lowCut:   return cut (false, b.frequency, q, rate);
            case EqBand::Type::highCut:  return cut (true, b.frequency, q, rate);
            case EqBand::Type::notch:
            case EqBand::Type::parametric:
            default:
                return normalised (1 + alpha * a, -2 * c, 1 - alpha * a, 1 + alpha / a, -2 * c, 1 - alpha / a);
        }
    }

    EqDesign designEq (const ChannelEq& source, double rate)
    {
        const auto e = source.limited();
        EqDesign d;
        rate = juce::jmax (8000.0, rate);
        d.preGain = (float) (juce::Decibels::decibelsToGain (e.preGainDb) * (e.phaseInvert ? -1.0 : 1.0));
        if (e.lowCutOn)
            d.numPre = addCut (d.pre, d.numPre, false, e.lowCut, e.lowCutSlope, rate);
        if (e.highCutOn)
            d.numPre = addCut (d.pre, d.numPre, true, e.highCut, e.highCutSlope, rate);
        if (! e.bypass)
        {
            for (const auto& b : e.bands)
                if (b.on)
                    d.numEq = addBand (d.eq, d.numEq, b, rate);
            d.eqGain = (float) juce::Decibels::decibelsToGain (e.outputDb);
        }
        return d;
    }

    int addBand (std::array<Biquad, EqDesign::maxStages>& stages, int count, const EqBand& b, double rate)
    {
        if (b.isCut())
            return addCut (stages, count, b.type == EqBand::Type::highCut, b.frequency, b.slope, rate,
                           juce::jmax (0.1, b.q / 0.70710678));
        if (count < EqDesign::maxStages)
            stages[(size_t) count++] = bandFilter (b, rate);
        return count;
    }

    EqDesign listenDesign (const EqBand& b, double rate)
    {
        EqDesign d;
        rate = juce::jmax (8000.0, rate);
        using T = EqBand::Type;
        if (b.type == T::lowShelf || b.type == T::lowCut)
            d.numEq = addCut (d.eq, 0, true, b.frequency, 24, rate);    // under it
        else if (b.type == T::highShelf || b.type == T::highCut)
            d.numEq = addCut (d.eq, 0, false, b.frequency, 24, rate);   // over it
        else
        {
            // A band pass (0 dB at the centre) as wide as the band.
            const auto w = 2.0 * pi * safeFrequency (b.frequency, rate) / rate;
            const auto q = juce::jlimit (0.5, 40.0, b.q * (b.type == T::notch ? 8.0 : 1.0));
            const auto alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
            d.eq[0] = normalised (alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
            d.numEq = 1;
        }
        return d;
    }

    double eqResponseDb (const EqDesign& d, double frequency, double rate, bool withPre)
    {
        std::complex<double> h { 1.0, 0.0 };
        for (int i = 0; i < d.numEq; ++i)
            h *= d.eq[(size_t) i].response (frequency, rate);
        auto gain = std::abs (h) * d.eqGain;
        if (withPre)
        {
            std::complex<double> p { 1.0, 0.0 };
            for (int i = 0; i < d.numPre; ++i)
                p *= d.pre[(size_t) i].response (frequency, rate);
            gain *= std::abs (p) * std::abs (d.preGain);
        }
        return juce::Decibels::gainToDecibels (gain, -120.0);
    }

    void EqProcessor::setDesign (const EqDesign& d)
    {
        // The same stages: their memory kept (no click on a turned knob); else cleared.
        if (d.numPre != design.numPre)
            for (auto& channel : preState)
                for (auto& s : channel)
                    s = {};
        if (d.numEq != design.numEq)
            for (auto& channel : eqState)
                for (auto& s : channel)
                    s = {};
        design = d;
    }

    void EqProcessor::reset()
    {
        preState = {};
        eqState = {};
    }

    void EqProcessor::run (const std::array<Biquad, EqDesign::maxStages>& stages, int count,
                           std::array<std::array<double, 2>, EqDesign::maxStages>& state, float* samples, int numSamples)
    {
        // Transposed direct form II, in doubles.
        for (int k = 0; k < count; ++k)
        {
            const auto& f = stages[(size_t) k];
            auto s1 = state[(size_t) k][0], s2 = state[(size_t) k][1];
            for (int i = 0; i < numSamples; ++i)
            {
                const double x = samples[i];
                const auto y = f.b0 * x + s1;
                s1 = f.b1 * x - f.a1 * y + s2;
                s2 = f.b2 * x - f.a2 * y;
                samples[i] = (float) y;
            }
            // (Values too small to hear flushed: no slow denormals in a long silence.)
            state[(size_t) k][0] = std::abs (s1) < 1.0e-25 ? 0.0 : s1;
            state[(size_t) k][1] = std::abs (s2) < 1.0e-25 ? 0.0 : s2;
        }
    }

    void EqProcessor::runStereo (const std::array<Biquad, EqDesign::maxStages>& stages, int count,
                                 std::array<std::array<std::array<double, 2>, EqDesign::maxStages>, 2>& state,
                                 float* left, float* right, int numSamples)
    {
        for (int k = 0; k < count; ++k)
        {
            const auto& f = stages[(size_t) k];
            auto l1 = state[0][(size_t) k][0], l2 = state[0][(size_t) k][1];
            auto r1 = state[1][(size_t) k][0], r2 = state[1][(size_t) k][1];
            for (int i = 0; i < numSamples; ++i)
            {
                const double xl = left[i], xr = right[i];
                const auto yl = f.b0 * xl + l1;
                const auto yr = f.b0 * xr + r1;
                l1 = f.b1 * xl - f.a1 * yl + l2;
                r1 = f.b1 * xr - f.a1 * yr + r2;
                l2 = f.b2 * xl - f.a2 * yl;
                r2 = f.b2 * xr - f.a2 * yr;
                left[i] = (float) yl;
                right[i] = (float) yr;
            }
            state[0][(size_t) k][0] = std::abs (l1) < 1.0e-25 ? 0.0 : l1;
            state[0][(size_t) k][1] = std::abs (l2) < 1.0e-25 ? 0.0 : l2;
            state[1][(size_t) k][0] = std::abs (r1) < 1.0e-25 ? 0.0 : r1;
            state[1][(size_t) k][1] = std::abs (r2) < 1.0e-25 ? 0.0 : r2;
        }
    }

    void EqProcessor::processPre (float* const* channels, int numChannels, int numSamples)
    {
        if (numChannels >= 2 && channels[0] != channels[1])
        {
            if (design.preGain != 1.0f)
                for (int c = 0; c < 2; ++c)
                    juce::FloatVectorOperations::multiply (channels[c], design.preGain, numSamples);
            runStereo (design.pre, design.numPre, preState, channels[0], channels[1], numSamples);
            return;
        }
        for (int c = 0; c < juce::jmin (2, numChannels); ++c)
        {
            if (design.preGain != 1.0f)
                juce::FloatVectorOperations::multiply (channels[c], design.preGain, numSamples);
            run (design.pre, design.numPre, preState[(size_t) c], channels[c], numSamples);
        }
    }

    void EqProcessor::processEq (float* const* channels, int numChannels, int numSamples)
    {
        if (design.numEq == 0 && design.eqGain == 1.0f)
            return;
        if (numChannels >= 2 && channels[0] != channels[1])
        {
            runStereo (design.eq, design.numEq, eqState, channels[0], channels[1], numSamples);
            if (design.eqGain != 1.0f)
                for (int c = 0; c < 2; ++c)
                    juce::FloatVectorOperations::multiply (channels[c], design.eqGain, numSamples);
            return;
        }
        for (int c = 0; c < juce::jmin (2, numChannels); ++c)
        {
            run (design.eq, design.numEq, eqState[(size_t) c], channels[c], numSamples);
            if (design.eqGain != 1.0f)
                juce::FloatVectorOperations::multiply (channels[c], design.eqGain, numSamples);
        }
    }

    std::optional<double> parseFrequency (const juce::String& raw)
    {
        auto text = raw.trim();
        if (text.isEmpty())
            return std::nullopt;
        // A note: letter, # or b, octave, then maybe " +23" / " -23" cents.
        if (const auto pc = pcOfLetter (text[0]); pc >= 0 && ! text.startsWithIgnoreCase ("e+"))
        {
            auto at = 1;
            auto shift = 0;
            if (at < text.length() && (text[at] == '#' || text[at] == 'b'))
                shift = text[at++] == '#' ? 1 : -1;
            const auto rest = text.substring (at).trim();
            const auto octaveText = rest.upToFirstOccurrenceOf (" ", false, false);
            if (octaveText.isEmpty() || ! octaveText.containsOnly ("-0123456789"))
                return std::nullopt;
            const auto cents = rest.containsChar (' ') ? rest.fromFirstOccurrenceOf (" ", false, false).trim() : juce::String();
            if (cents.isNotEmpty() && ! cents.containsOnly ("+-0123456789"))
                return std::nullopt;
            const auto midi = 12 * (octaveText.getIntValue() + 2) + pc + shift;
            const auto f = 440.0 * std::pow (2.0, (midi - 69 + cents.getIntValue() / 100.0) / 12.0);
            if (f < 1.0 || f > 40000.0)
                return std::nullopt;
            return f;
        }
        // A number, maybe with k, Hz or kHz.
        auto lower = text.toLowerCase().removeCharacters (" ");
        auto factor = 1.0;
        if (lower.endsWith ("khz")) { factor = 1000.0; lower = lower.dropLastCharacters (3); }
        else if (lower.endsWith ("hz")) lower = lower.dropLastCharacters (2);
        else if (lower.endsWith ("k")) { factor = 1000.0; lower = lower.dropLastCharacters (1); }
        if (lower.isEmpty() || ! lower.containsOnly ("0123456789."))
            return std::nullopt;
        const auto f = lower.getDoubleValue() * factor;
        if (f <= 0.0)
            return std::nullopt;
        return f;
    }

    juce::String frequencyText (double hz)
    {
        if (hz >= 1000.0)
            return juce::String (hz / 1000.0, hz >= 10000.0 ? 1 : 2) + " kHz";
        return juce::String (juce::roundToInt (hz)) + " Hz";
    }

    juce::String bandTypeName (EqBand::Type t)
    {
        switch (t)
        {
            case EqBand::Type::parametric: return "Peak";
            case EqBand::Type::lowShelf:   return "Low Shelf";
            case EqBand::Type::highShelf:  return "High Shelf";
            case EqBand::Type::lowCut:     return "Cut";
            case EqBand::Type::highCut:    return "Cut";
            case EqBand::Type::notch:      return "Notch";
        }
        return {};
    }

    const std::vector<EqStartingPoint>& eqStartingPoints()
    {
        using T = EqBand::Type;
        // A band: on, its type, gain, frequency, Q.
        const auto band = [] (T type, double gain, double hz, double q) { return EqBand { true, type, gain, hz, q }; };
        const auto off = [] (int i) { auto b = ChannelEq::defaultBands()[(size_t) i]; b.on = false; return b; };
        const auto make = [&] (double lowCut, std::array<EqBand, 4> bands, double highCut = 0.0)
        {
            ChannelEq eq;
            eq.bands = bands;
            if (lowCut > 0.0) { eq.lowCutOn = true; eq.lowCut = lowCut; eq.lowCutSlope = 24; }
            if (highCut > 0.0) { eq.highCutOn = true; eq.highCut = highCut; eq.highCutSlope = 12; }
            return eq.limited();
        };
        static const std::vector<EqStartingPoint> points {
            { "VOCALS", "Lead Vocal: Presence", make (90.0, { off (0), band (T::parametric, -2.5, 300.0, 1.2), band (T::parametric, 2.5, 3500.0, 1.0), band (T::highShelf, 2.5, 10000.0, 0.71) }) },
            { "VOCALS", "Lead Vocal: Warm", make (80.0, { band (T::lowShelf, 1.5, 200.0, 0.71), band (T::parametric, -2.0, 450.0, 1.4), band (T::parametric, 1.5, 2500.0, 1.0), band (T::highShelf, 1.5, 9000.0, 0.71) }) },
            { "VOCALS", "Backing Vocals: Tucked In", make (150.0, { off (0), band (T::parametric, -3.0, 400.0, 1.0), band (T::parametric, -2.0, 3000.0, 1.2), band (T::highShelf, 2.0, 11000.0, 0.71) }) },
            { "VOCALS", "Scream: Tame the Harshness", make (100.0, { off (0), band (T::parametric, -2.0, 500.0, 1.0), band (T::parametric, -3.0, 2800.0, 3.0), band (T::highShelf, 1.5, 9000.0, 0.71) }) },
            { "VOCALS", "Telephone Voice", make (400.0, { off (0), band (T::parametric, 6.0, 1500.0, 1.0), off (2), off (3) }, 3500.0) },
            { "DRUMS", "Kick: Punch and Click", make (35.0, { band (T::parametric, 3.0, 60.0, 1.4), band (T::parametric, -4.0, 350.0, 1.5), band (T::parametric, 3.0, 4000.0, 1.4), off (3) }) },
            { "DRUMS", "Snare: Body and Crack", make (80.0, { band (T::parametric, 2.0, 200.0, 1.4), band (T::parametric, -2.5, 800.0, 2.0), band (T::parametric, 2.0, 5000.0, 1.0), band (T::highShelf, 1.5, 10000.0, 0.71) }) },
            { "DRUMS", "Toms: Less Ring", make (60.0, { band (T::parametric, 2.0, 110.0, 1.4), band (T::parametric, -4.0, 500.0, 2.0), band (T::parametric, 2.0, 3500.0, 1.4), off (3) }) },
            { "DRUMS", "Overheads: Cymbals Only", make (250.0, { off (0), band (T::parametric, -2.0, 1000.0, 1.0), off (2), band (T::highShelf, 2.0, 10000.0, 0.71) }) },
            { "DRUMS", "Drum Bus: Weight and Snap", make (30.0, { band (T::lowShelf, 2.0, 80.0, 0.71), band (T::parametric, -2.0, 400.0, 1.0), band (T::parametric, 1.5, 5000.0, 1.0), off (3) }) },
            { "BASS", "Bass: Tight Low End", make (35.0, { band (T::parametric, 2.0, 80.0, 1.2), band (T::parametric, -3.0, 250.0, 1.4), band (T::parametric, 2.5, 900.0, 1.4), off (3) }, 6000.0) },
            { "BASS", "Bass: Growl", make (40.0, { band (T::lowShelf, 1.5, 100.0, 0.71), band (T::parametric, -2.0, 300.0, 1.4), band (T::parametric, 3.5, 1500.0, 1.2), band (T::parametric, 2.0, 3500.0, 1.4) }) },
            { "GUITARS", "Rhythm Guitar: Clear the Mud", make (80.0, { off (0), band (T::parametric, -3.0, 250.0, 1.4), band (T::parametric, -1.5, 3500.0, 2.0), off (3) }, 12000.0) },
            { "GUITARS", "Rhythm Guitar: Scooped Metal", make (90.0, { band (T::parametric, 1.5, 120.0, 1.0), band (T::parametric, -4.0, 700.0, 1.0), band (T::parametric, 2.0, 3000.0, 1.2), off (3) }, 11000.0) },
            { "GUITARS", "Lead Guitar: Cut Through", make (100.0, { off (0), band (T::parametric, -2.0, 300.0, 1.2), band (T::parametric, 3.0, 1800.0, 1.2), off (3) }, 10000.0) },
            { "GUITARS", "Acoustic Guitar: Sparkle", make (80.0, { off (0), band (T::parametric, -3.0, 200.0, 1.4), band (T::parametric, 1.5, 4000.0, 1.0), band (T::highShelf, 2.5, 10000.0, 0.71) }) },
            { "KEYS AND SYNTHS", "Piano: Clear", make (60.0, { off (0), band (T::parametric, -2.0, 300.0, 1.2), band (T::parametric, 1.5, 3000.0, 1.0), band (T::highShelf, 1.5, 9000.0, 0.71) }) },
            { "KEYS AND SYNTHS", "Pad: Out of the Way", make (200.0, { off (0), band (T::parametric, -2.5, 600.0, 1.0), band (T::parametric, -2.5, 3000.0, 1.0), off (3) }, 10000.0) },
            { "MIX", "Master: Gentle Smile", make (25.0, { band (T::lowShelf, 1.0, 80.0, 0.71), band (T::parametric, -1.0, 400.0, 0.7), off (2), band (T::highShelf, 1.0, 12000.0, 0.71) }) },
            { "MIX", "Clean Up Lows (Low Cut 100 Hz)", make (100.0, { off (0), off (1), off (2), off (3) }) },
        };
        return points;
    }
}
