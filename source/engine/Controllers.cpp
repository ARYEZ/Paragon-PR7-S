#include "Controllers.h"

namespace pr7s::ControllerKinds
{
    bool isController (int code) noexcept
    {
        return juce::isPositiveAndBelow (code, 131) || (code >= polyBase && code < polyBase + 128);
    }

    bool isValid (int code) noexcept { return code == velocity || isController (code); }

    Controller::Type typeOf (int code) noexcept
    {
        if (code >= polyBase)
            return Controller::Type::polyPressure;
        switch (code)
        {
            case pitchbend:     return Controller::Type::pitchbend;
            case aftertouch:    return Controller::Type::aftertouch;
            case programChange: return Controller::Type::programChange;
            default:            return Controller::Type::cc;
        }
    }

    int numberOf (int code) noexcept
    {
        if (code >= polyBase)
            return code - polyBase;
        return juce::isPositiveAndBelow (code, 128) ? code : 0;
    }

    int codeOf (Controller::Type type, int number) noexcept
    {
        switch (type)
        {
            case Controller::Type::cc:            return juce::jlimit (0, 127, number);
            case Controller::Type::pitchbend:     return pitchbend;
            case Controller::Type::aftertouch:    return aftertouch;
            case Controller::Type::programChange: return programChange;
            case Controller::Type::polyPressure:  return polyBase + juce::jlimit (0, 127, number);
        }
        return 0;
    }

    int codeOf (const Controller& c) noexcept { return codeOf (c.type, c.number); }

    juce::String ccName (int number)
    {
        // The MIDI standard's names (the General MIDI ones where it gives them).
        switch (number)
        {
            case 0:   return "BANK SELECT MSB";
            case 1:   return "MODULATION";
            case 2:   return "BREATH";
            case 4:   return "FOOT";
            case 5:   return "PORTAMENTO TIME";
            case 6:   return "DATA ENTRY MSB";
            case 7:   return "VOLUME";
            case 8:   return "BALANCE";
            case 10:  return "PAN";
            case 11:  return "EXPRESSION";
            case 12:  return "EFFECT CONTROL 1";
            case 13:  return "EFFECT CONTROL 2";
            case 16:  return "GENERAL PURPOSE 1";
            case 17:  return "GENERAL PURPOSE 2";
            case 18:  return "GENERAL PURPOSE 3";
            case 19:  return "GENERAL PURPOSE 4";
            case 32:  return "BANK SELECT LSB";
            case 38:  return "DATA ENTRY LSB";
            case 64:  return "SUSTAIN";
            case 65:  return "PORTAMENTO";
            case 66:  return "SOSTENUTO";
            case 67:  return "SOFT PEDAL";
            case 68:  return "LEGATO";
            case 69:  return "HOLD 2";
            case 70:  return "SOUND VARIATION";
            case 71:  return "RESONANCE";
            case 72:  return "RELEASE";
            case 73:  return "ATTACK";
            case 74:  return "CUTOFF";
            case 75:  return "DECAY";
            case 76:  return "VIBRATO RATE";
            case 77:  return "VIBRATO DEPTH";
            case 78:  return "VIBRATO DELAY";
            case 80:  return "GENERAL PURPOSE 5";
            case 81:  return "GENERAL PURPOSE 6";
            case 82:  return "GENERAL PURPOSE 7";
            case 83:  return "GENERAL PURPOSE 8";
            case 84:  return "PORTAMENTO CONTROL";
            case 91:  return "REVERB";
            case 92:  return "TREMOLO";
            case 93:  return "CHORUS";
            case 94:  return "DETUNE";
            case 95:  return "PHASER";
            case 96:  return "DATA INCREMENT";
            case 97:  return "DATA DECREMENT";
            case 98:  return "NRPN LSB";
            case 99:  return "NRPN MSB";
            case 100: return "RPN LSB";
            case 101: return "RPN MSB";
            case 120: return "ALL SOUND OFF";
            case 121: return "RESET CONTROLLERS";
            case 122: return "LOCAL CONTROL";
            case 123: return "ALL NOTES OFF";
            case 124: return "OMNI OFF";
            case 125: return "OMNI ON";
            case 126: return "MONO ON";
            case 127: return "POLY ON";
            default:  break;
        }
        if (number >= 33 && number <= 63)
            return "CC " + juce::String (number - 32) + " LSB";
        return {};
    }

    juce::String name (int code)
    {
        if (code == velocity)      return "VELOCITY";
        if (code == pitchbend)     return "PITCHBEND";
        if (code == aftertouch)    return "AFTERTOUCH";
        if (code == programChange) return "PROGRAM CHANGE";
        if (code >= polyBase)      return "POLY PRESSURE " + juce::MidiMessage::getMidiNoteName (code - polyBase, true, true, 3);
        const auto n = ccName (code);
        return "CC " + juce::String (code) + (n.isNotEmpty() ? " " + n : juce::String());
    }

    juce::String shortName (int code)
    {
        if (code == velocity)      return "VEL";
        if (code == pitchbend)     return "PB";
        if (code == aftertouch)    return "AT";
        if (code == programChange) return "PROG";
        if (code >= polyBase)      return "PP " + juce::MidiMessage::getMidiNoteName (code - polyBase, true, true, 3);
        switch (code)
        {
            case 1:  return "MOD";
            case 2:  return "BREATH";
            case 7:  return "VOL";
            case 10: return "PAN";
            case 11: return "EXPR";
            case 64: return "SUS";
            case 74: return "CUTOFF";
            default: return "CC " + juce::String (code);
        }
    }

    int maxValue (int code) noexcept { return code == pitchbend ? 16383 : 127; }

    int defaultValue (int code) noexcept
    {
        switch (code)
        {
            case pitchbend: return 8192;
            case 7:         return 100;
            case 8:
            case 10:        return 64;
            case 11:        return 127;
            default:        return 0;
        }
    }

    juce::String valueText (int code, int value)
    {
        if (code == pitchbend)
        {
            const auto bend = value - 8192;
            return bend > 0 ? "+" + juce::String (bend) : juce::String (bend);
        }
        if (code == 10 || code == 8)
            return value == 64 ? juce::String ("C") : value < 64 ? "L" + juce::String (64 - value) : "R" + juce::String (value - 64);
        if (code == 64 || code == 65 || code == 66 || code == 67 || code == 68 || code == 69)
            return juce::String (value) + (value >= 64 ? " ON" : " OFF");
        return juce::String (value);
    }

    bool alwaysStep (int code) noexcept
    {
        switch (code)
        {
            case 0: case 32: case 64: case 98: case 99: case 100: case 101:
            case 120: case 121: case 122: case 123: case 124: case 125: case 126: case 127:
            case programChange:
                return true;
            default:
                return false;
        }
    }

    juce::String toText (int code)
    {
        if (code == velocity)      return "velocity";
        if (code == pitchbend)     return "pitchbend";
        if (code == aftertouch)    return "aftertouch";
        if (code == programChange) return "program";
        if (code >= polyBase)      return "poly" + juce::String (code - polyBase);
        return "cc" + juce::String (code);
    }

    int fromText (const juce::String& text)
    {
        const auto t = text.trim().toLowerCase();
        if (t == "velocity")   return velocity;
        if (t == "pitchbend")  return pitchbend;
        if (t == "aftertouch") return aftertouch;
        if (t == "program")    return programChange;
        const auto number = [&] (int skip) -> int
        {
            const auto digits = t.substring (skip);
            return digits.isNotEmpty() && digits.containsOnly ("0123456789") ? digits.getIntValue() : -1;
        };
        if (t.startsWith ("poly"))
        {
            const auto n = number (4);
            return juce::isPositiveAndBelow (n, 128) ? polyBase + n : -2;
        }
        if (t.startsWith ("cc"))
        {
            const auto n = number (2);
            return juce::isPositiveAndBelow (n, 128) ? n : -2;
        }
        return -2;
    }

    juce::String lanesToText (const std::vector<int>& lanes)
    {
        juce::StringArray parts;
        for (const auto code : lanes)
            parts.add (toText (code));
        return parts.joinIntoString (",");
    }

    std::vector<int> lanesFromText (const juce::String& text)
    {
        std::vector<int> lanes;
        for (const auto& part : juce::StringArray::fromTokens (text, ",", {}))
            if (const auto code = fromText (part); isValid (code))
                lanes.push_back (code);
        return lanes;
    }

    const std::vector<int>& commonCodes()
    {
        static const std::vector<int> codes { velocity, pitchbend, 1, 11, 7, 64, 10, 74, 71, 2, 21, aftertouch, programChange };
        return codes;
    }

    void sort (std::vector<Controller>& events)
    {
        std::stable_sort (events.begin(), events.end(), [] (const Controller& a, const Controller& b) { return a.start < b.start; });
    }

    std::vector<int> usedCodes (const Part& part)
    {
        std::vector<int> codes;
        for (const auto& c : part.controllers)
            codes.push_back (codeOf (c));
        std::sort (codes.begin(), codes.end());
        codes.erase (std::unique (codes.begin(), codes.end()), codes.end());
        return codes;
    }

    std::optional<int> valueAt (const std::vector<Controller>& events, int code, double beat)
    {
        const Controller* before = nullptr;
        for (const auto& c : events)
        {
            if (codeOf (c) != code)
                continue;
            if (c.start > beat)
            {
                if (c.ramp && before != nullptr && c.start > before->start)
                {
                    const auto t = (beat - before->start) / (c.start - before->start);
                    return juce::roundToInt (before->value + t * (c.value - before->value));
                }
                break;
            }
            before = &c;
        }
        if (before == nullptr)
            return std::nullopt;
        return before->value;
    }

    juce::MidiMessage toMidi (int code, int value, int channel)
    {
        channel = juce::jlimit (1, 16, channel);
        const auto v = juce::jlimit (0, maxValue (code), value);
        switch (typeOf (code))
        {
            case Controller::Type::cc:            return juce::MidiMessage::controllerEvent (channel, numberOf (code), v);
            case Controller::Type::pitchbend:     return juce::MidiMessage::pitchWheel (channel, v);
            case Controller::Type::aftertouch:    return juce::MidiMessage::channelPressureChange (channel, v);
            case Controller::Type::programChange: return juce::MidiMessage::programChange (channel, v);
            case Controller::Type::polyPressure:  return juce::MidiMessage::aftertouchChange (channel, numberOf (code), v);
        }
        return {};
    }

    void expandRamps (const Part& part, double resolution, const std::function<void (double, int, int)>& emit)
    {
        // Each kind's events in order; the values between a ramp's ends a step of `resolution` apart.
        struct Pending { double beat; int code, value; size_t order; };
        std::vector<Pending> out;
        std::vector<std::pair<int, const Controller*>> last;   // code -> the event before
        const auto previous = [&] (int code) -> const Controller*&
        {
            for (auto& l : last)
                if (l.first == code)
                    return l.second;
            last.push_back ({ code, nullptr });
            return last.back().second;
        };
        resolution = juce::jmax (1.0 / 960.0, resolution);
        for (const auto& c : part.controllers)
        {
            const auto code = codeOf (c);
            auto*& before = previous (code);
            if (c.ramp && before != nullptr && c.start > before->start && c.value != before->value)
            {
                auto sent = before->value;
                for (auto beat = before->start + resolution; beat < c.start - 1.0e-9; beat += resolution)
                {
                    const auto t = (beat - before->start) / (c.start - before->start);
                    const auto v = juce::roundToInt (before->value + t * (c.value - before->value));
                    if (v != sent)
                        out.push_back ({ beat, code, v, out.size() }), sent = v;
                }
            }
            out.push_back ({ c.start, code, c.value, out.size() });
            before = &c;
        }
        std::stable_sort (out.begin(), out.end(), [] (const Pending& a, const Pending& b) { return a.beat < b.beat; });
        for (const auto& p : out)
            emit (p.beat, p.code, p.value);
    }
}
