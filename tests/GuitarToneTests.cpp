#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/GuitarTones.h"
#include "engine/Studio.h"
#include "ui/ComposerTracksPage.h"
#include "TestHelpers.h"

#include <map>

// (0.204.0, the owner asked) GUITAR TONES: PR7-G on the lead, clean and acoustic lanes' tracks with a tone chosen
// from the part's style; the owner's amp model and cab IR kept.

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    // PR7-G's settings as its getStateInformation writes them (a few of its parameters, an amp and a cab).
    std::unique_ptr<juce::XmlElement> pr7gTree()
    {
        auto xml = std::make_unique<juce::XmlElement> ("ParagonX1");
        xml->setAttribute ("ampModelPath", "C:/Amps/My Amp.nam");
        xml->setAttribute ("cabIrPath", "C:/Cabs/My Cab.wav");
        xml->setAttribute ("rigName", "MY RIG");
        xml->setAttribute ("rigModified", 1);
        const std::pair<const char*, double> params[] { { "inputGain", 3.0 }, { "outputGain", -2.0 }, { "driveOn", 1.0 }, { "ampOn", 1.0 },
                                                        { "cabOn", 1.0 }, { "delayOn", 0.0 }, { "delayMix", 0.5 }, { "modOn", 1.0 },
                                                        { "lofiOn", 1.0 } };
        for (const auto& [id, value] : params)
        {
            auto* p = xml->createNewChildElement ("PARAM");
            p->setAttribute ("id", id);
            p->setAttribute ("value", value);
        }
        return xml;
    }

    juce::MemoryBlock rawState (const juce::XmlElement& xml)
    {
        juce::MemoryBlock block;
        juce::AudioProcessor::copyXmlToBinary (xml, block);
        return block;
    }

    // As JUCE's VST3 host keeps it: the plug-in's bytes (with JUCE's private data after them) in IComponent.
    const char* privateTail = "\0\0\0\0\0\0\0\0BYPASSDATA\x0a\0\0\0\0\0\0\0JUCEPrivateData";
    constexpr size_t privateTailSize = 8 + 10 + 8 + 15;

    juce::MemoryBlock wrappedState (const juce::XmlElement& xml)
    {
        auto component = rawState (xml);
        component.append (privateTail, privateTailSize);
        juce::XmlElement wrapper ("VST3PluginState");
        wrapper.createNewChildElement ("IComponent")->addTextElement (component.toBase64Encoding());
        juce::MemoryBlock controller ("CONTROLLER", 10);
        wrapper.createNewChildElement ("IEditController")->addTextElement (controller.toBase64Encoding());
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

    // PR7-G's ranges (its Parameters.cpp) for every id a tone may set.
    const std::map<juce::String, std::pair<float, float>>& ranges()
    {
        static const std::map<juce::String, std::pair<float, float>> r {
            { "compOn", { 0, 1 } }, { "compType", { 0, 2 } }, { "compThreshold", { -40, 0 } }, { "compRatio", { 1, 20 } }, { "compLevel", { -12, 24 } },
            { "modOn", { 0, 1 } }, { "modType", { 0, 11 } }, { "modRate", { 0.05f, 10 } }, { "modDepth", { 0, 1 } }, { "modMix", { 0, 1 } },
            { "modStereo", { 0, 1 } }, { "modSync", { 0, 15 } },
            { "delayOn", { 0, 1 } }, { "delayType", { 0, 9 } }, { "delaySync", { 0, 15 } }, { "delayTime", { 1, 2500 } },
            { "delayFeedback", { 0, 0.95f } }, { "delayTone", { 0, 10 } }, { "delayMix", { 0, 1 } }, { "delayPingPong", { 0, 1 } },
            { "delayDuck", { 0, 1 } },
            { "reverbOn", { 0, 1 } }, { "reverbType", { 0, 7 } }, { "reverbSize", { 0, 1 } }, { "reverbTone", { 0, 10 } }, { "reverbMix", { 0, 1 } },
            { "reverbPreDelay", { 0, 250 } }, { "reverbDuck", { 0, 1 } },
            { "ampOn", { 0, 1 } }, { "cabOn", { 0, 1 } }, { "mod2On", { 0, 1 } }, { "fpOn", { 0, 1 } }, { "lofiOn", { 0, 1 } }, { "glitchOn", { 0, 1 } } };
        return r;
    }

    // A stand-in for PR7-G: an effect named as it, keeping its settings as PR7-G's state.
    struct FakePr7g final : TestPlugin
    {
        FakePr7g()
            : TestPlugin (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
        {
            name = "Paragon PR7-G";
            bytes = rawState (*pr7gTree());
        }
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        void getStateInformation (juce::MemoryBlock& block) override { block = bytes; }
        void setStateInformation (const void* data, int size) override { bytes.replaceAll (data, (size_t) size); ++setCount; }
        std::unique_ptr<juce::XmlElement> settings() const { return readPr7gSettings (bytes); }
        juce::MemoryBlock bytes;
        int setCount = 0;
    };

    LineIdea ideaOf (LineIdea::Part part, const juce::String& style)
    {
        LineIdea idea;
        idea.part = part;
        idea.style = style;
        for (int i = 0; i < 8; ++i)
            idea.notes.push_back ({ (double) i, 0.5, 64 + i % 3, 0.8f });
        return idea;
    }

    std::vector<ChordSpan> oneBar()
    {
        std::vector<ChordSpan> spans (1);
        spans[0].start = 0.0;
        spans[0].end = 4.0;
        return spans;
    }

    // The PR7-G instances the studio made, in order.
    struct Maker
    {
        std::vector<FakePr7g*> made;
        void attach (Studio& studio)
        {
            studio.makePr7gForTests = [this]
            {
                auto p = std::make_unique<FakePr7g>();
                made.push_back (p.get());
                return std::unique_ptr<juce::AudioPluginInstance> (std::move (p));
            };
        }
    };

    int pr7gCount (const Studio& studio, int trackId)
    {
        auto count = 0;
        if (const auto* inserts = studio.getInserts (trackId))
            for (const auto& i : *inserts)
                count += isPr7g (i.plugin) ? 1 : 0;
        return count;
    }

    const Track* trackNamed (const Studio& studio, const juce::String& name)
    {
        for (const auto& t : studio.getProject().tracks)
            if (t.name == name)
                return &t;
        return nullptr;
    }
}

TEST_CASE ("0.204.0: PR7-G's settings read and written back in either form", "[tones204]")
{
    const auto tree = pr7gTree();
    SECTION ("the plug-in's own bytes")
    {
        const auto before = rawState (*tree);
        auto read = readPr7gSettings (before);
        REQUIRE (read != nullptr);
        CHECK (read->isEquivalentTo (tree.get(), false));
        read->setAttribute ("rigName", "CHANGED");
        const auto after = writePr7gSettings (*read, before);
        auto again = juce::AudioProcessor::getXmlFromBinary (after.getData(), (int) after.getSize());
        REQUIRE (again != nullptr);
        CHECK (again->hasTagName ("ParagonX1"));
        CHECK (again->getStringAttribute ("rigName") == "CHANGED");
    }
    SECTION ("JUCE's VST3 wrapper with its private data")
    {
        const auto before = wrappedState (*tree);
        auto read = readPr7gSettings (before);
        REQUIRE (read != nullptr);
        CHECK (read->isEquivalentTo (tree.get(), false));
        read->setAttribute ("rigName", "CHANGED");
        const auto after = writePr7gSettings (*read, before);
        auto wrapper = juce::AudioProcessor::getXmlFromBinary (after.getData(), (int) after.getSize());
        REQUIRE (wrapper != nullptr);
        CHECK (wrapper->hasTagName ("VST3PluginState"));
        juce::MemoryBlock component, controller;
        REQUIRE (component.fromBase64Encoding (wrapper->getChildByName ("IComponent")->getAllSubText()));
        REQUIRE (controller.fromBase64Encoding (wrapper->getChildByName ("IEditController")->getAllSubText()));
        CHECK (controller == juce::MemoryBlock ("CONTROLLER", 10));
        // The plug-in's bytes, then JUCE's private data exactly as it was.
        REQUIRE (component.getSize() > privateTailSize);
        CHECK (std::memcmp ((const char*) component.getData() + component.getSize() - privateTailSize, privateTail, privateTailSize) == 0);
        auto inside = juce::AudioProcessor::getXmlFromBinary (component.getData(), (int) component.getSize());
        REQUIRE (inside != nullptr);
        CHECK (inside->getStringAttribute ("rigName") == "CHANGED");
        CHECK (readPr7gSettings (after) != nullptr);
    }
    SECTION ("not PR7-G's")
    {
        juce::XmlElement other ("SomethingElse");
        CHECK (readPr7gSettings (rawState (other)) == nullptr);
        CHECK (readPr7gSettings (juce::MemoryBlock ("hello", 5)) == nullptr);
    }
}

TEST_CASE ("0.204.0: a tone changes only its own settings, the amp and cab kept", "[tones204]")
{
    const auto* tone = toneNamed ("soaring lead");
    REQUIRE (tone != nullptr);
    auto settings = pr7gTree();
    applyTone (*settings, *tone);
    for (const auto& [id, value] : tone->settings)
        CHECK_THAT (paramOf (*settings, id), WithinAbs (value, 1.0e-4));
    CHECK (paramOf (*settings, "inputGain") == 3.0);
    CHECK (paramOf (*settings, "outputGain") == -2.0);
    CHECK (paramOf (*settings, "driveOn") == 1.0);
    CHECK (paramOf (*settings, "ampOn") == 1.0);   // (only the acoustic tones touch the amp and cab)
    CHECK (paramOf (*settings, "lofiOn") == 0.0);  // (the blocks a tone doesn't use go off)
    CHECK (paramOf (*settings, "delayOn") == 1.0);
    CHECK (settings->getStringAttribute ("ampModelPath") == "C:/Amps/My Amp.nam");
    CHECK (settings->getStringAttribute ("cabIrPath") == "C:/Cabs/My Cab.wav");
    CHECK (settings->getStringAttribute ("rigName") == "SOARING LEAD");
    CHECK (settings->getIntAttribute ("rigModified") == 0);

    auto acoustic = pr7gTree();
    applyTone (*acoustic, *toneNamed ("ACOUSTIC ROOM"));
    CHECK (paramOf (*acoustic, "ampOn") == 0.0);
    CHECK (paramOf (*acoustic, "cabOn") == 0.0);
}

TEST_CASE ("0.204.0: one of the owner's PR7-G rigs", "[tones204]")
{
    const auto folder = juce::File::createTempFile ("tones204");
    folder.createDirectory();
    const auto write = [&] (const juce::String& fileName, bool keep)
    {
        juce::XmlElement rig ("ParagonRig");
        rig.setAttribute ("name", "My Lead");
        rig.setAttribute ("ampModel", "D:/Other Amp.nam");
        rig.setAttribute ("cabIr", "D:/Other Cab.wav");
        if (keep)
            rig.setAttribute ("keepAmpAndCab", true);
        const std::pair<const char*, double> params[] { { "delayOn", 1.0 }, { "delayMix", 0.33 }, { "inputGain", 6.0 } };
        for (const auto& [id, value] : params)
        {
            auto* p = rig.createNewChildElement ("Param");
            p->setAttribute ("id", id);
            p->setAttribute ("value", value);
        }
        const auto file = folder.getChildFile (fileName);
        REQUIRE (rig.writeTo (file));
        return file;
    };
    const auto rig = write ("My Lead.pr7rig", false);
    auto settings = pr7gTree();
    REQUIRE (applyRigFile (*settings, rig));
    CHECK_THAT (paramOf (*settings, "delayMix"), WithinAbs (0.33, 1.0e-6));
    CHECK (paramOf (*settings, "inputGain") == 6.0);
    CHECK (paramOf (*settings, "outputGain") == -2.0);
    CHECK (settings->getStringAttribute ("ampModelPath") == "D:/Other Amp.nam");
    CHECK (settings->getStringAttribute ("cabIrPath") == "D:/Other Cab.wav");
    CHECK (settings->getStringAttribute ("rigName") == "My Lead");
    CHECK (settings->getStringAttribute ("rigFile") == rig.getFullPathName());

    auto kept = pr7gTree();
    REQUIRE (applyRigFile (*kept, write ("Keep.pr7rig", true)));
    CHECK (kept->getStringAttribute ("ampModelPath") == "C:/Amps/My Amp.nam");
    CHECK (kept->getStringAttribute ("cabIrPath") == "C:/Cabs/My Cab.wav");

    auto notRig = pr7gTree();
    CHECK_FALSE (applyRigFile (*notRig, folder.getChildFile ("missing.pr7rig")));
    folder.deleteRecursively();
}

TEST_CASE ("0.204.0: the tone suggested by the part's style", "[tones204]")
{
    using P = LineIdea::Part;
    CHECK (suggestedTone (P::lead, "Solo: Shred Runs").name == "SHRED LEAD");
    CHECK (suggestedTone (P::lead, "Solo: Soaring Bends").name == "SOARING LEAD");
    CHECK (suggestedTone (P::lead, "Pentatonic box").name == "SOARING LEAD");   // (nothing matched: the default)
    CHECK (suggestedTone (P::lead, "Solo: Blues Hammer").name == "BLUES LEAD");
    CHECK (suggestedTone (P::lead, "Solo: Twin Lead Harmony").name == "TWIN LEAD");
    CHECK (suggestedTone (P::clean, "RINGING ARPEGGIO").name == "CLEAN SHIMMER");
    CHECK (suggestedTone (P::clean, "PEDAL HOOK").name == "CLEAN PULSE");
    CHECK (suggestedTone (P::clean, "").name == "CLEAN SHIMMER");
    CHECK (suggestedTone (P::acoustic, "").name == "ACOUSTIC ROOM");
    CHECK (suggestedTone (P::acoustic, "BIG STRUMS").name == "ACOUSTIC WIDE");
    CHECK_FALSE (hasGuitarTones (P::rhythm));
    CHECK_FALSE (hasGuitarTones (P::bass));
    CHECK (hasGuitarTones (P::lead));
    CHECK (Studio::suggestedToneName (P::rhythm, "x").isEmpty());
    CHECK (guitarTonesFor (P::lead).size() == 5);
    CHECK (guitarTonesFor (P::clean).size() == 3);
    CHECK (guitarTonesFor (P::acoustic).size() == 3);
}

TEST_CASE ("0.204.0: every tone's settings are PR7-G's, inside its ranges; our own names", "[tones204]")
{
    const juce::StringArray banned { "cubase", "steinberg", "boss", "mxr", "fender", "roland", "marshall", "vox", "eventide", "strymon",
                                     "line 6", "gilmour", "slash", "hendrix", "vai", "satriani", "edge", "pink floyd", "u2", "metallica" };
    juce::StringArray names;
    for (const auto& t : guitarTones())
    {
        INFO (t.name);
        CHECK (hasGuitarTones (t.part));
        CHECK (t.name == t.name.toUpperCase());
        CHECK_FALSE (names.contains (t.name));
        names.add (t.name);
        CHECK (t.description.isNotEmpty());
        for (const auto& b : banned)
        {
            CHECK_FALSE (t.name.toLowerCase().contains (b));
            CHECK_FALSE (t.description.toLowerCase().contains (b));
        }
        for (const auto& [id, value] : t.settings)
        {
            INFO (id);
            const auto found = ranges().find (id);
            REQUIRE (found != ranges().end());
            CHECK (value >= found->second.first);
            CHECK (value <= found->second.second);
        }
        for (const char* off : { "mod2On", "fpOn", "lofiOn", "glitchOn" })
        {
            auto seen = false;
            for (const auto& [id, value] : t.settings)
                seen = seen || (id == off && value == 0.0f);
            CHECK (seen);
        }
    }
}

TEST_CASE ("0.204.0: BUILD puts PR7-G with the tone on a lead's track", "[tones204]")
{
    Studio studio (false);
    Maker maker;
    maker.attach (studio);
    const auto spans = oneBar();
    studio.buildSection ({ ideaOf (LineIdea::Part::lead, "Solo: Shred Runs") }, spans, "Verse");
    const auto* lead = trackNamed (studio, studio.newIdeaTrackName (LineIdea::Part::lead));
    REQUIRE (lead != nullptr);
    REQUIRE (maker.made.size() == 1);
    CHECK (pr7gCount (studio, lead->id) == 1);
    auto settings = maker.made[0]->settings();
    REQUIRE (settings != nullptr);
    for (const auto& [id, value] : toneNamed ("SHRED LEAD")->settings)
        CHECK_THAT (paramOf (*settings, id), WithinAbs (value, 1.0e-4));
    CHECK (settings->getStringAttribute ("ampModelPath") == "C:/Amps/My Amp.nam");
    CHECK (settings->getStringAttribute ("rigName") == "SHRED LEAD");
    CHECK (studio.getLastTones().joinIntoString (",") == "LEAD: SHRED LEAD ON PR7-G");
    // The insert keeps the settings too (saved with the song).
    CHECK (readPr7gSettings ((*studio.getInserts (lead->id)).back().state) != nullptr);

    // Again: the PR7-G there is the owner's now: left alone.
    maker.made[0]->bytes = rawState (*pr7gTree());
    const auto sets = maker.made[0]->setCount;
    studio.buildSection ({ ideaOf (LineIdea::Part::lead, "Solo: Shred Runs") }, spans, "Verse");
    CHECK (maker.made.size() == 1);
    CHECK (pr7gCount (studio, lead->id) == 1);
    CHECK (maker.made[0]->setCount == sets);
    CHECK (studio.getLastTones().isEmpty());

    // Asked for (the lane's menu): replaced.
    studio.setToneChoice (LineIdea::Part::lead, { Studio::ToneChoice::Kind::tone, "AMBIENT LEAD" });
    juce::String error;
    REQUIRE (studio.toneTrack (lead->id, ideaOf (LineIdea::Part::lead, "Solo: Shred Runs"), true, &error));
    CHECK (pr7gCount (studio, lead->id) == 1);
    settings = maker.made[0]->settings();
    CHECK (settings->getStringAttribute ("rigName") == "AMBIENT LEAD");
    CHECK (paramOf (*settings, "reverbType") == 3.0);
    CHECK (studio.getLastTone() == "AMBIENT LEAD");
}

TEST_CASE ("0.204.0: OFF adds nothing; a rhythm lane gets no tone", "[tones204]")
{
    Studio studio (false);
    Maker maker;
    maker.attach (studio);
    const auto spans = oneBar();
    studio.setToneChoice (LineIdea::Part::clean, { Studio::ToneChoice::Kind::off, {} });
    CHECK (studio.getToneChoice (LineIdea::Part::clean).kind == Studio::ToneChoice::Kind::off);
    studio.buildSection ({ ideaOf (LineIdea::Part::clean, "RINGING ARPEGGIO"), ideaOf (LineIdea::Part::rhythm, "Power chords") }, spans, "Verse");
    CHECK (maker.made.empty());
    for (const auto& t : studio.getProject().tracks)
        CHECK (pr7gCount (studio, t.id) == 0);

    // The acoustic on automatic: no amp, no cab.
    studio.buildSection ({ ideaOf (LineIdea::Part::acoustic, "") }, spans, "Chorus");
    REQUIRE (maker.made.size() == 1);
    auto settings = maker.made[0]->settings();
    CHECK (settings->getStringAttribute ("rigName") == "ACOUSTIC ROOM");
    CHECK (paramOf (*settings, "ampOn") == 0.0);
}

TEST_CASE ("0.204.0: without PR7-G nothing is added", "[tones204]")
{
    Studio studio (false);   // (no hook, PR7-G not looked for, none known)
    studio.buildSection ({ ideaOf (LineIdea::Part::lead, "x") }, oneBar(), "Verse");
    const auto* lead = trackNamed (studio, studio.newIdeaTrackName (LineIdea::Part::lead));
    REQUIRE (lead != nullptr);
    CHECK (pr7gCount (studio, lead->id) == 0);
    juce::String error;
    CHECK_FALSE (studio.toneTrack (lead->id, ideaOf (LineIdea::Part::lead, "x"), true, &error));
    CHECK (error.contains ("wasn't found"));
}

TEST_CASE ("0.204.0: the Hub's TONE boxes", "[tones204][ui]")
{
    Studio studio (false);
    ComposerTracksPage page (studio);
    page.setSize (1100, 700);
    CHECK (page.getToneBox (0) == nullptr);   // (bass)
    CHECK (page.getToneBox (1) == nullptr);   // (guitar)
    auto* lead = page.getToneBox (2);
    REQUIRE (lead != nullptr);
    REQUIRE (page.getToneBox (6) != nullptr);
    REQUIRE (page.getToneBox (7) != nullptr);
    CHECK (lead->getText() == "AUTOMATIC (BY STYLE)");
    page.setTone (2, { Studio::ToneChoice::Kind::tone, "BLUES LEAD" });
    CHECK (studio.getToneChoice (LineIdea::Part::lead).value == "BLUES LEAD");
    CHECK (lead->getText() == "BLUES LEAD");
    // The box itself: OFF.
    lead->setSelectedId (2, juce::sendNotificationSync);
    CHECK (studio.getToneChoice (LineIdea::Part::lead).kind == Studio::ToneChoice::Kind::off);
    // A clean tone chosen in its box.
    auto* clean = page.getToneBox (6);
    for (int i = 0; i < clean->getNumItems(); ++i)
        if (clean->getItemText (i) == "CLEAN SWELL")
            clean->setSelectedItemIndex (i, juce::sendNotificationSync);
    CHECK (studio.getToneChoice (LineIdea::Part::clean).value == "CLEAN SWELL");
    // A rig that isn't there any more: named, (NOT FOUND).
    page.setTone (7, { Studio::ToneChoice::Kind::rig, "/nowhere/Gone.pr7rig" });
    CHECK (page.getToneBox (7)->getText().contains ("NOT FOUND"));
    // A tone of another part isn't taken.
    studio.setToneChoice (LineIdea::Part::acoustic, { Studio::ToneChoice::Kind::tone, "NO SUCH TONE" });
    CHECK (studio.getToneChoice (LineIdea::Part::acoustic).kind == Studio::ToneChoice::Kind::automatic);
}
