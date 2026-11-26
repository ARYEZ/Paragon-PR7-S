#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/GuitarTones.h"
#include "engine/LaneWorlds.h"
#include "engine/Studio.h"
#include "engine/SynthPatches.h"
#include "TestHelpers.h"

// (0.220.0, the owner asked) A PR7-K patch for each of the keys lane's CYBERPUNK parts, written into the PR7-K
// playing the track.

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    // PR7-K's settings as its getStateInformation writes them: a few of its knobs, a loaded wavetable, an SFZ, a matrix
    // and a MIDI LEARN link of the owner's.
    std::unique_ptr<juce::XmlElement> pr7kTree (const juce::String& rigName = "MY OWN PAD")
    {
        auto xml = std::make_unique<juce::XmlElement> ("ParagonX1");
        xml->setAttribute ("parameterVersion", 71);
        xml->setAttribute ("rigName", rigName);
        xml->setAttribute ("rigFactory", "My Own Pad");
        xml->setAttribute ("rigModified", 1);
        const std::pair<const char*, double> params[] { { "osc1On", 1.0 }, { "osc1Wave", 4.0 }, { "osc4On", 1.0 }, { "osc5UniMix", 0.9 },
                                                        { "flt1On", 0.0 }, { "talkOn", 1.0 }, { "arp1On", 1.0 }, { "outputGain", -6.0 },
                                                        { "flt1Cutoff", 500.0 }, { "amp1Volume", 1.2 } };
        for (const auto& [id, value] : params)
        {
            auto* p = xml->createNewChildElement ("PARAM");
            p->setAttribute ("id", id);
            p->setAttribute ("value", value);
        }
        xml->createNewChildElement ("Wavetables")->createNewChildElement ("WavetableRef")->setAttribute ("file", "C:/Tables/Mine.wav");
        xml->createNewChildElement ("SfzInstruments")->createNewChildElement ("Sfz")->setAttribute ("file", "C:/Sfz/Mine.sfz");
        auto* matrix = xml->createNewChildElement ("Modulation");
        auto* route = matrix->createNewChildElement ("Route");
        route->setAttribute ("source", "lfo1");
        route->setAttribute ("destination", "flt1Cutoff");
        auto* links = xml->createNewChildElement ("MidiLinks");
        auto* mine = links->createNewChildElement ("Link");
        mine->setAttribute ("cc", 1);
        mine->setAttribute ("parameter", "macro1");
        auto* taken = links->createNewChildElement ("Link");
        taken->setAttribute ("cc", 74);
        taken->setAttribute ("parameter", "macro2");
        return xml;
    }

    juce::MemoryBlock rawState (const juce::XmlElement& xml)
    {
        juce::MemoryBlock block;
        juce::AudioProcessor::copyXmlToBinary (xml, block);
        return block;
    }

    const char* privateTail = "\0\0\0\0\0\0\0\0BYPASSDATA\x0a\0\0\0\0\0\0\0JUCEPrivateData";
    constexpr size_t privateTailSize = 8 + 10 + 8 + 15;

    // As JUCE's VST3 host keeps it.
    juce::MemoryBlock wrappedState (const juce::XmlElement& xml)
    {
        auto component = rawState (xml);
        component.append (privateTail, privateTailSize);
        juce::XmlElement wrapper ("VST3PluginState");
        wrapper.createNewChildElement ("IComponent")->addTextElement (component.toBase64Encoding());
        juce::MemoryBlock out;
        juce::AudioProcessor::copyXmlToBinary (wrapper, out);
        return out;
    }

    double paramOf (const juce::XmlElement& xml, const juce::String& id, double otherwise = -999.0)
    {
        for (const auto* p : xml.getChildWithTagNameIterator ("PARAM"))
            if (p->getStringAttribute ("id") == id)
                return p->getDoubleAttribute ("value");
        return otherwise;
    }

    // A stand-in for PR7-K: an instrument named as it, keeping its settings as PR7-K's state.
    struct FakePr7k final : TestPlugin
    {
        explicit FakePr7k (juce::MemoryBlock state, const juce::String& n = "Paragon PR7-K")
            : TestPlugin (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
        {
            name = n;
            instrument = true;
            bytes = std::move (state);
        }
        void processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) override { b.clear(); }
        void getStateInformation (juce::MemoryBlock& block) override { block = bytes; }
        void setStateInformation (const void* data, int size) override { bytes.replaceAll (data, (size_t) size); ++setCount; }
        std::unique_ptr<juce::XmlElement> settings() const { return readPr7gSettings (bytes); }
        juce::MemoryBlock bytes;
        int setCount = 0;
    };

    std::vector<ChordSpan> twoBars()
    {
        std::vector<ChordSpan> spans (2);
        spans[0].start = 0.0; spans[0].end = 4.0; spans[0].chord = Chord { 9, Chord::Type::minor }; spans[0].key = Key { 5, 0, true };
        spans[1].start = 4.0; spans[1].end = 8.0; spans[1].chord = Chord { 5, Chord::Type::major }; spans[1].key = Key { 5, 0, true };
        return spans;
    }

    LineIdea partFor (int option)
    {
        CyberpunkRequest request;
        request.kicks = { 0.0, 2.0, 4.0, 6.0 };
        return cyberpunkKeys (option, twoBars(), request);
    }

    // A keys track playing a stand-in PR7-K with these settings.
    std::pair<int, FakePr7k*> keysTrack (Studio& studio, juce::MemoryBlock state, const juce::String& name = "Paragon PR7-K")
    {
        const auto id = studio.addTrack (Instrument {});
        auto fake = std::make_unique<FakePr7k> (std::move (state), name);
        auto* raw = fake.get();
        studio.getEngine().setInstrument (id, std::move (fake));
        return { id, raw };
    }
}

TEST_CASE ("0.220.0: a patch for every cyberpunk part, found by the part's style", "[patches220]")
{
    const auto& patches = cyberpunkPatches();
    REQUIRE ((int) patches.size() == cyberpunkKeysOptions().size());
    juce::StringArray names;
    for (int i = 0; i < (int) patches.size(); ++i)
    {
        const auto& p = patches[(size_t) i];
        CHECK (p.name.startsWith ("CYBER "));
        CHECK (p.description.isNotEmpty());
        CHECK (! names.contains (p.name));
        names.add (p.name);
        CHECK (isCyberpunkPatchName (p.name.toLowerCase()));
        // The part the lane writes names its option, and so its patch.
        const auto option = cyberpunkOptionOfStyle (partFor (i).style);
        REQUIRE (option.has_value());
        CHECK (*option == i);
        CHECK (cyberpunkPatchFor (i) == &p);
        // Every patch makes a sound: an oscillator on, at a level, the amp letting it through (or the pump route).
        CHECK (p.settings.at ("osc1On") == 1.0f);
        CHECK (p.settings.at ("osc1Volume") > 0.0f);
        CHECK ((p.settings.at ("amp1Volume") > 0.0f || p.pumping));
        CHECK (p.settings.at ("patchLevel") <= 0.0f);
        for (const auto& [id, value] : p.settings)
            CHECK (std::isfinite (value));
    }
    CHECK (cyberpunkPatches()[3].pumping);
    for (int i = 0; i < (int) patches.size(); ++i)
        if (i != 3)
            CHECK (! patches[(size_t) i].pumping);
    CHECK (! cyberpunkOptionOfStyle ("Pumping pads on the kick").has_value());
    CHECK (! cyberpunkOptionOfStyle ("Cyberpunk: no such part").has_value());
    CHECK (cyberpunkPatchFor (-1) == nullptr);
    CHECK (cyberpunkPatchFor ((int) patches.size()) == nullptr);
}

TEST_CASE ("0.220.0: a patch starts from a clean slate and keeps the owner's MIDI links", "[patches220]")
{
    auto settings = pr7kTree();
    applySynthPatch (*settings, *cyberpunkPatchFor (3));
    // Every switch off but those the patch turns on.
    CHECK (paramOf (*settings, "osc4On") == 0.0);
    CHECK (paramOf (*settings, "talkOn") == 0.0);
    CHECK (paramOf (*settings, "arp1On") == 0.0);
    CHECK (paramOf (*settings, "osc1On") == 1.0);
    CHECK (paramOf (*settings, "osc2On") == 1.0);
    CHECK (paramOf (*settings, "osc3On") == 0.0);
    CHECK (paramOf (*settings, "flt1On") == 1.0);
    CHECK (paramOf (*settings, "reverbOn") == 1.0);
    CHECK (paramOf (*settings, "delayOn") == 0.0);
    // The patch's knobs, the others left (OUTPUT is the owner's, oscillators past the third switched off).
    CHECK (paramOf (*settings, "osc1Wave") == 0.0);
    CHECK_THAT (paramOf (*settings, "flt1Cutoff"), WithinAbs (2600.0, 1.0e-3));
    CHECK (paramOf (*settings, "outputGain") == -6.0);
    CHECK_THAT (paramOf (*settings, "osc5UniMix"), WithinAbs (0.9, 1.0e-6));
    // The loaded sounds gone; the matrix ours: EXPRESSION -> AMP VOLUME, the knob at 0.
    CHECK (settings->getChildByName ("Wavetables") == nullptr);
    CHECK (settings->getChildByName ("SfzInstruments") == nullptr);
    auto* matrix = settings->getChildByName ("Modulation");
    REQUIRE (matrix != nullptr);
    REQUIRE (matrix->getNumChildElements() == 1);
    auto* route = matrix->getFirstChildElement();
    CHECK (route->getStringAttribute ("source") == "expression");
    CHECK (route->getStringAttribute ("destination") == "amp1Volume");
    CHECK_THAT (route->getDoubleAttribute ("amount"), WithinAbs (0.5, 1.0e-6));
    CHECK (! route->getBoolAttribute ("percent"));
    CHECK (paramOf (*settings, "amp1Volume") == 0.0);
    // MIDI LEARN: the owner's CC 1 kept; CC 74 now the filter's cutoff (the old CC 74 link let go).
    auto* links = settings->getChildByName ("MidiLinks");
    REQUIRE (links != nullptr);
    std::map<int, juce::String> byCc;
    for (const auto* l : links->getChildWithTagNameIterator ("Link"))
        byCc[l->getIntAttribute ("cc")] = l->getStringAttribute ("parameter");
    CHECK (byCc.size() == 2);
    CHECK (byCc[1] == "macro1");
    CHECK (byCc[74] == "flt1Cutoff");
    // Named as ours.
    CHECK (settings->getStringAttribute ("rigName") == "CYBER PUMP PAD");
    CHECK (settings->getStringAttribute ("rigFactory").isEmpty());
    CHECK (settings->getIntAttribute ("rigModified") == 0);
    CHECK (settings->getIntAttribute ("parameterVersion") == 71);

    // A patch without the pump: no matrix of its own (the old one gone), AMP VOLUME back to unity.
    applySynthPatch (*settings, *cyberpunkPatchFor (0));
    CHECK (settings->getChildByName ("Modulation") == nullptr);
    CHECK (paramOf (*settings, "amp1Volume") == 1.0);
    CHECK (paramOf (*settings, "delayOn") == 1.0);
    CHECK (paramOf (*settings, "osc2Transpose") == 12.0);
    // Written twice, still one CC 74 link.
    auto count74 = 0;
    for (const auto* l : settings->getChildByName ("MidiLinks")->getChildWithTagNameIterator ("Link"))
        count74 += l->getIntAttribute ("cc") == 74 ? 1 : 0;
    CHECK (count74 == 1);
}

TEST_CASE ("0.220.0: a fresh PR7-K or one of ours may be written over, the owner's own sound only by hand", "[patches220]")
{
    CHECK (mayReplaceSynthPatch (*pr7kTree ("")));
    CHECK (mayReplaceSynthPatch (*pr7kTree ("Init")));
    CHECK (mayReplaceSynthPatch (*pr7kTree ("CYBER DARK PAD")));
    CHECK (! mayReplaceSynthPatch (*pr7kTree ("MY OWN PAD")));

    Studio studio (false);
    const auto idea = partFor (7);
    {
        auto [id, fake] = keysTrack (studio, wrappedState (*pr7kTree ("MY OWN PAD")));
        juce::String error;
        CHECK (! studio.patchTrack (id, idea, false, &error));
        CHECK (error.contains ("MY OWN PAD"));
        CHECK (fake->setCount == 0);
        // By hand (the lane's menu): written, JUCE's private data after PR7-K's bytes kept.
        CHECK (studio.patchTrack (id, idea, true, &error));
        CHECK (fake->setCount == 1);
        auto settings = fake->settings();
        REQUIRE (settings != nullptr);
        CHECK (settings->getStringAttribute ("rigName") == "CYBER DARK PAD");
        CHECK (studio.getLastPatch() == "CYBER DARK PAD");
        for (const auto& [pid, value] : cyberpunkPatchFor (7)->settings)
            CHECK_THAT (paramOf (*settings, pid), WithinAbs (value, 1.0e-4));
        juce::MemoryBlock component;
        auto wrapper = juce::AudioProcessor::getXmlFromBinary (fake->bytes.getData(), (int) fake->bytes.getSize());
        REQUIRE (wrapper != nullptr);
        component.fromBase64Encoding (wrapper->getChildByName ("IComponent")->getAllSubText().trim());
        CHECK (juce::String::fromUTF8 ((const char*) component.getData(), (int) component.getSize()).length() > 0);
        CHECK (std::memcmp ((const char*) component.getData() + component.getSize() - privateTailSize, privateTail, privateTailSize) == 0);
    }
    {
        auto [id, fake] = keysTrack (studio, rawState (*pr7kTree ("Init")));
        CHECK (studio.patchTrack (id, idea, false));
        CHECK (fake->settings()->getStringAttribute ("rigName") == "CYBER DARK PAD");
    }
    {
        // Not a cyberpunk part, or not PR7-K: nothing written.
        auto [id, fake] = keysTrack (studio, rawState (*pr7kTree ("Init")));
        LineIdea plain;
        plain.part = LineIdea::Part::chords;
        plain.style = "Block chords";
        juce::String error;
        CHECK (! studio.patchTrack (id, plain, true, &error));
        CHECK (error.contains ("CYBERPUNK"));
        auto [other, otherFake] = keysTrack (studio, rawState (*pr7kTree ("Init")), "Some Other Synth");
        CHECK (! studio.patchTrack (other, idea, true, &error));
        CHECK (error.contains ("PR7-K"));
        CHECK (fake->setCount == 0);
        CHECK (otherFake->setCount == 0);
    }
}

TEST_CASE ("0.220.0: BUILD puts the part's patch on the keys track's PR7-K", "[patches220]")
{
    Studio studio (false);
    auto [id, fake] = keysTrack (studio, wrappedState (*pr7kTree ("Init")));
    const auto spans = twoBars();
    const auto idea = partFor (3);
    studio.buildSection ({ idea }, spans, "Chorus", { id });
    CHECK (fake->setCount == 1);
    auto settings = fake->settings();
    REQUIRE (settings != nullptr);
    CHECK (settings->getStringAttribute ("rigName") == "CYBER PUMP PAD");
    CHECK (studio.getLastTones().contains ("KEYS: CYBER PUMP PAD ON PR7-K"));
    // Building again with another part changes it (it's ours now)...
    studio.buildSection ({ partFor (8) }, spans, "Breakdown", { id });
    CHECK (fake->settings()->getStringAttribute ("rigName") == "CYBER SUB DROP");
    // ...but a sound of the owner's stays.
    auto [mine, mineFake] = keysTrack (studio, wrappedState (*pr7kTree ("MY OWN PAD")));
    studio.buildSection ({ idea }, spans, "Chorus", { mine });
    CHECK (mineFake->setCount == 0);
    CHECK (mineFake->settings()->getStringAttribute ("rigName") == "MY OWN PAD");
    // A plain keys part builds without a patch.
    auto [plainTrack, plainFake] = keysTrack (studio, wrappedState (*pr7kTree ("Init")));
    LineIdea plain = idea;
    plain.style = "Block chords";
    studio.buildSection ({ plain }, spans, "Verse", { plainTrack });
    CHECK (plainFake->setCount == 0);
}
