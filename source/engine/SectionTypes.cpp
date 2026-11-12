#include "engine/SectionTypes.h"

#include "engine/SongSections.h"

namespace pr7s
{
    std::vector<SectionType> sectionTypesFor (const juce::String& sectionName)
    {
        // composerSectionFor: intro 0, verse 1, chorus 2, bridge 3, breakdown 4, ending 5, solo 6.
        const SectionType driving { "DRIVING", "Eighth notes driving the chorus on", RiffFlavour::drivingEighths };
        const SectionType droning { "DRONING", "Chords rung across the bar, the band carrying the movement", RiffFlavour::droning };
        const SectionType staccato { "STACCATO CHUG", "Short palm-muted chugs and stops, the bass and the kick locked to them; the rhythm guitar picks the kind",
                                     RiffFlavour::pedalStabs,
                                     { RiffFlavour::pedalStabs, RiffFlavour::chugThrees, RiffFlavour::burstSilence,
                                       RiffFlavour::tritone, RiffFlavour::chugs, RiffFlavour::gallop, RiffFlavour::stopStart },
                                     SectionType::Band::staccato };
        const SectionType sparse { "SPARSE", "Space for the voice: stabs, octaves, chords let ring", RiffFlavour::stabs,
                                   { RiffFlavour::stabs, RiffFlavour::octaves, RiffFlavour::openChords } };
        const SectionType heavy { "HEAVY CHUG", "The low string pounded: chugs, tritone grinds, stops; the bass and the kick locked to them", RiffFlavour::chugs,
                                  { RiffFlavour::chugs, RiffFlavour::tritone, RiffFlavour::stopStart, RiffFlavour::stabs },
                                  SectionType::Band::staccato };
        const SectionType creep { "CREEP", "A line creeping a half step at a time", RiffFlavour::creep };
        const SectionType open { "OPEN CHORDS", "Open chords let ring", RiffFlavour::openChords };
        const SectionType stabs { "STABS", "Short chord stabs with space between", RiffFlavour::stabs };
        switch (composerSectionFor (sectionName))
        {
            case 0: return { droning, open };
            case 1: return { staccato, droning, sparse };
            case 2: return { driving, droning };
            case 3: return { creep, open, droning };
            case 4: return { heavy, staccato };
            case 5: return { droning, stabs };
            case 6: return { driving, droning };
            default: break;
        }
        return {};
    }

    std::optional<SectionType> sectionTypeNamed (const juce::String& sectionName, const juce::String& type)
    {
        for (const auto& t : sectionTypesFor (sectionName))
            if (t.name.equalsIgnoreCase (type.trim()))
                return t;
        return std::nullopt;
    }

    std::optional<SectionType> sectionTypeCalled (const juce::String& type)
    {
        if (type.isEmpty())
            return std::nullopt;
        for (const auto* name : { "Intro", "Verse", "Chorus", "Bridge", "Breakdown", "Ending", "Solo" })
            if (auto t = sectionTypeNamed (name, type))
                return t;
        return std::nullopt;
    }

    std::optional<RiffFlavour> typedGuitarFlavour (const ComposerState& s)
    {
        if (s.writtenRiffs)
            return std::nullopt;
        const auto t = sectionTypeCalled (s.sectionType);
        if (! t.has_value() || t->guitarFlavours.empty())
            return std::nullopt;
        const auto& lane = s.lane (ComposerLayer::guitar);
        const auto n = (int) t->guitarFlavours.size();
        return t->guitarFlavours[(size_t) (lane.approach >= 0 ? lane.approach % n : lane.variation % n)];
    }

    bool isSectionTypeOf (const juce::String& sectionName, const juce::String& type)
    {
        return type.isEmpty() || type.equalsIgnoreCase (standardSectionType) || sectionTypeNamed (sectionName, type).has_value();
    }

    juce::StringArray sectionApproaches (RiffFlavour flavour, ComposerLayer layer)
    {
        if (flavour != RiffFlavour::droning)
            return {};
        switch (layer)
        {
            case ComposerLayer::guitar: return { "RUNG WHOLE", "PUSHED A BEAT EARLY", "MOVING TOP NOTE", "STRUCK TWICE", "SLIDING DOWN" };
            case ComposerLayer::bass: return { "HELD WITH THE GUITAR", "HELD, PULSED AT THE LOOP'S END" };
            case ComposerLayer::chords: return { "HIGH PULSE", "CLIMBING MOTIF", "HELD THIRD" };
            case ComposerLayer::lead: return { "OCTAVES ON THE THIRDS", "OCTAVES ON THE ROOTS", "MUTED PEDAL IN THREES" };
            case ComposerLayer::drums: return { "OPEN HATS", "HALF TIME", "LOCKED TO THE PUSHES" };
            case ComposerLayer::melody: break;
        }
        return {};
    }

    bool staccatoBand (const ComposerState& s)
    {
        const auto t = sectionTypeCalled (s.sectionType);
        return t.has_value() && t->band == SectionType::Band::staccato && ! s.writtenRiffs && s.lane (ComposerLayer::guitar).on;
    }

    juce::StringArray sectionApproaches (const ComposerState& s, ComposerLayer layer)
    {
        if (s.writtenRiffs)
            return {};
        if (const auto t = sectionTypeCalled (s.sectionType); t.has_value() && t->band == SectionType::Band::staccato)
        {
            if (layer == ComposerLayer::bass)
                return { "DOUBLES THE GUITAR", "ROOTS ON THE ACCENTS" };
            if (layer == ComposerLayer::drums)
                return { "KICK ON EVERY CHUG", "KICK ON THE ACCENTS" };
        }
        if (layer == ComposerLayer::guitar)
            if (const auto t = sectionTypeCalled (s.sectionType); t.has_value() && ! t->guitarFlavours.empty())
            {
                juce::StringArray names;   // (the type's riff kinds)
                for (const auto f : t->guitarFlavours)
                    names.add (riffFlavourName (f));
                return names;
            }
        const auto& flavours = riffFlavours();
        return sectionApproaches (flavours[(size_t) juce::jlimit (0, (int) flavours.size() - 1, s.riffFlavour)], layer);
    }

    juce::String approachName (const ComposerState& s, ComposerLayer layer)
    {
        const auto names = sectionApproaches (s, layer);
        if (names.isEmpty())
            return {};
        const auto a = s.lane (layer).approach;
        return a >= 0 ? names[a % names.size()] : juce::String ("AUTOMATIC");
    }

    void applySectionType (ComposerState& s, const juce::String& sectionName, const juce::String& type)
    {
        const auto& flavours = riffFlavours();
        const auto indexOf = [&flavours] (RiffFlavour f)
        { return (int) std::distance (flavours.begin(), std::find (flavours.begin(), flavours.end(), f)); };
        for (auto& l : s.lanes)
            l.approach = -1;
        if (const auto t = sectionTypeNamed (sectionName, type))
        {
            s.sectionType = t->name;
            s.riffFlavour = indexOf (t->flavour);
            s.writtenRiffs = false;
        }
        else
        {
            s.sectionType = {};
            s.riffFlavour = indexOf (RiffFlavour::any);
        }
    }
}
