#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <complex>
#include <optional>

// The channel EQ (0.43.0, the owner asked; the manual's Equalizer and Pre sections): every
// audio-carrying channel (tracks, FX and group channels, the master) has
//   PRE   input gain (+-24 dB), phase invert, a low cut (20 Hz-20 kHz) and a high cut
//         (50 Hz-20 kHz), each 6, 12, 24, 36 or 48 dB per octave (12 at first);
//   EQ    four bands, each on/off, gain +-24 dB, frequency 20 Hz-20 kHz, Q; bands 1 and 4
//         parametric, shelf or cut, 2 and 3 always parametric; a bypass for all four.
// Where they play (the manual's channel): Pre first, then the inserts before the fader, then the
// EQ, then the fader (so pre-fader sends hear the EQ). The filters are the standard "audio EQ
// cookbook" biquads (R. Bristow-Johnson), the cuts Butterworth. The bands' starting frequencies
// and shapes are our choice.
//
// Since 0.46.0 (the owner asked: as the other DAW's Frequency 2, four bands of it) every band can
// be Low Shelf, Peak, High Shelf or Notch; band 1 also a low Cut and band 4 a high Cut, each 6,
// 12, 24, 48 or 96 dB per octave. Q is the width of a Peak or Notch, the dip and bump at a
// shelf's corner (above 0.71), and a Cut's resonance (none for Cut 6). Each band can invert its
// gain; the EQ has an Output level. The type numbers stayed (parametric is the Peak), so songs
// from 0.43.0 on sound as they did (their cuts were 12 dB per octave).
namespace pr7s
{
    struct EqBand
    {
        // (parametric = Peak; the numbers are what songs store)
        enum class Type { parametric, lowShelf, highShelf, lowCut, highCut, notch };
        bool on = false;
        Type type = Type::parametric;
        double gainDb = 0.0;      // +-24 (not for cuts)
        double frequency = 1000.0;
        double q = 1.0;
        int slope = 12;           // (0.46.0) a cut's: 6, 12, 24, 48 or 96 dB per octave
        bool invert = false;      // (0.46.0) Invert Gain: the gain plays the other way

        bool isCut() const noexcept { return type == Type::lowCut || type == Type::highCut; }
        double playedGainDb() const noexcept { return invert ? -gainDb : gainDb; }
        bool operator== (const EqBand& o) const noexcept
        {
            return on == o.on && type == o.type && gainDb == o.gainDb && frequency == o.frequency && q == o.q
                && slope == o.slope && invert == o.invert;
        }
    };

    struct ChannelEq
    {
        bool bypass = false;
        std::array<EqBand, 4> bands = defaultBands();
        bool lowCutOn = false, highCutOn = false;
        double lowCut = 20.0, highCut = 20000.0;   // Hz
        int lowCutSlope = 12, highCutSlope = 12;   // dB per octave: 6, 12, 24, 36, 48
        double preGainDb = 0.0;
        bool phaseInvert = false;
        double outputDb = 0.0;   // (0.46.0) the EQ's Output, after its bands (+-24)

        static constexpr double minFrequency = 20.0, maxFrequency = 20000.0, minHighCut = 50.0;
        static constexpr double maxGainDb = 24.0, minQ = 0.1, maxQ = 12.0;
        static const std::array<int, 5>& slopes();   // the Pre's cuts: 6 12 24 36 48
        static const std::array<int, 5>& bandSlopes();   // (0.46.0) a band's cut: 6 12 24 48 96
        static std::array<EqBand, 4> defaultBands(); // low shelf 100 Hz, 500 Hz, 2 kHz, high shelf 8 kHz (all off)
        static bool canBe (int band, EqBand::Type);  // cuts on bands 1 and 4 only
        // (0.46.0) The shapes a band offers: Low Shelf, Peak, High Shelf, Notch, and band 1 a low
        // Cut, band 4 a high Cut.
        static std::vector<EqBand::Type> typesFor (int band);

        ChannelEq limited() const;                   // every value in its range, types allowed
        bool isFlat() const;                         // nothing that changes the sound
        bool eqActive() const;                       // a band on and not bypassed
        bool preActive() const;
        void invert();                               // the manual's Invert EQ Settings: every band's gain negated
        std::unique_ptr<juce::XmlElement> toXml (const juce::String& tag = "Eq") const;
        static ChannelEq fromXml (const juce::XmlElement&);
        bool operator== (const ChannelEq&) const noexcept;
    };

    // One biquad's coefficients (a0 = 1).
    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        std::complex<double> response (double frequency, double rate) const;
    };

    // The stages a ChannelEq plays, at a rate: the Pre cuts and the EQ bands (bypassed or off:
    // left out), at most `maxStages` each.
    struct EqDesign
    {
        static constexpr int maxStages = 36;   // (0.46.0: four cuts of 96 dB are 32)
        std::array<Biquad, maxStages> pre {}, eq {};
        int numPre = 0, numEq = 0;
        float preGain = 1.0f;   // the input gain, with the phase's sign
        float eqGain = 1.0f;    // (0.46.0) the Output, after the bands (not when bypassed)
    };
    EqDesign designEq (const ChannelEq&, double rate);
    Biquad bandFilter (const EqBand&, double rate);   // one stage (a cut: its first; designEq makes them all)
    int addBand (std::array<Biquad, EqDesign::maxStages>&, int count, const EqBand&, double rate);   // (0.46.0) its stages
    // (0.46.0) Auto Listen: the stretch a band works on, alone (a band pass round a Peak or Notch,
    // a low pass under a Low Shelf or low Cut's frequency, a high pass over a High Shelf or high Cut's).
    EqDesign listenDesign (const EqBand&, double rate);

    // The level at a frequency (dB), as the design plays it: the EQ, and the Pre's cuts and gain when asked.
    double eqResponseDb (const EqDesign&, double frequency, double rate, bool withPre = true);

    // Plays a design on a stereo sound (audio thread: nothing allocated). The state is kept when
    // the design changes (the same number of stages), else cleared.
    class EqProcessor
    {
    public:
        void setDesign (const EqDesign&);   // (under the engine's lock)
        void reset();
        void processPre (float* const* channels, int numChannels, int numSamples);
        void processEq (float* const* channels, int numChannels, int numSamples);
        const EqDesign& getDesign() const noexcept { return design; }

    private:
        static void run (const std::array<Biquad, EqDesign::maxStages>&, int stages, std::array<std::array<double, 2>, EqDesign::maxStages>& state,
                         float* samples, int numSamples);
        EqDesign design;
        std::array<std::array<std::array<double, 2>, EqDesign::maxStages>, 2> preState {}, eqState {};
    };

    // Typed frequencies: "440", "1.2k", "1200 Hz", or a note with cents "A3", "A5 -23", "C4 +49"
    // (middle C = C3, A3 = 440 Hz, as the manual); none when it can't be read.
    std::optional<double> parseFrequency (const juce::String&);
    juce::String frequencyText (double hz);   // "440 Hz", "1.20 kHz"
    juce::String bandTypeName (EqBand::Type);   // "Peak", "Low Shelf", "Notch", "Cut"...
}
