#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/GuitarTones.h"
#include "engine/Studio.h"
#include "ui/ComposerTracksPage.h"
#include "TestHelpers.h"

// (0.205.0, step 2 of the owner's PR7-G idea) THE TONES CHANGE WITH THE SECTIONS: PR7-G's mod mix, delay mix and
// feedback, reverb mix and size written as automation block by block over the SECTIONS row.

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    // A knob as a hosted plug-in's, named as PR7-G names it.
    struct NamedKnob final : juce::AudioPluginInstance::HostedParameter
    {
        explicit NamedKnob (juce::String n) : knobName (std::move (n)) {}
        float getValue() const override { return value.load(); }
        void setValue (float v) override { value = v; }
        float getDefaultValue() const override { return 0.5f; }
        juce::String getName (int) const override { return knobName; }
        juce::String getLabel() const override { return {}; }
        float getValueForText (const juce::String& text) const override { return text.getFloatValue(); }
        juce::String getParameterID() const override { return knobName.removeCharacters (" "); }
        juce::String knobName;
        std::atomic<float> value { 0.5f };
    };

    std::unique_ptr<juce::XmlElement> myRig()
    {
        auto xml = std::make_unique<juce::XmlElement> ("ParagonX1");
        xml->setAttribute ("rigName", "MY RIG");
        const std::pair<const char*, double> params[] { { "modMix", 0.5 }, { "delayMix", 0.25 }, { "delayFeedback", 0.4 },
                                                        { "reverbMix", 0.3 }, { "reverbSize", 0.6 }, { "delayOn", 1.0 } };
        for (const auto& [id, value] : params)
        {
            auto* p = xml->createNewChildElement ("PARAM");
            p->setAttribute ("id", id);
            p->setAttribute ("value", value);
        }
        return xml;
    }

    // A stand-in for PR7-G with its five knobs (and one more first, so the indices aren't 0..4).
    struct FakePr7g final : TestPlugin
    {
        explicit FakePr7g (bool withKnobs = true)
            : TestPlugin (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
        {
            name = "Paragon PR7-G";
            juce::AudioProcessor::copyXmlToBinary (*myRig(), bytes);
            if (withKnobs)
                for (const auto* n : { "Input Gain", "Mod Mix", "Delay Mix", "Delay Feedback", "Reverb Mix", "Reverb Size" })
                {
                    auto k = std::make_unique<NamedKnob> (n);
                    knobs.push_back (k.get());
                    addHostedParameter (std::move (k));
                }
        }
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        void getStateInformation (juce::MemoryBlock& block) override { block = bytes; }
        void setStateInformation (const void* data, int size) override { bytes.replaceAll (data, (size_t) size); }
        juce::MemoryBlock bytes;
        std::vector<NamedKnob*> knobs;
    };

    struct Maker
    {
        std::vector<FakePr7g*> made;
        bool withKnobs = true;
        void attach (Studio& studio)
        {
            studio.makePr7gForTests = [this]
            {
                auto p = std::make_unique<FakePr7g> (withKnobs);
                made.push_back (p.get());
                return std::unique_ptr<juce::AudioPluginInstance> (std::move (p));
            };
        }
    };

    LineIdea leadIdea()
    {
        LineIdea idea;
        idea.part = LineIdea::Part::lead;
        idea.style = "x";
        for (int i = 0; i < 8; ++i)
            idea.notes.push_back ({ (double) i, 0.5, 64 + i % 3, 0.8f });
        return idea;
    }

    void addRow (Studio& studio)
    {
        studio.appendSongSection ("Verse", 4);
        studio.appendSongSection ("Chorus", 4);
        studio.appendSongSection ("Bridge", 4);
    }

    const AutomationLane* laneOf (const Studio& studio, int trackId, int parameter)
    {
        for (const auto& l : *studio.getAutomation (trackId))
            if (l.target == AutomationLane::Target::insert && l.parameter == parameter)
                return &l;
        return nullptr;
    }

    // A lead track with PR7-G on it (its own rig, MY RIG).
    int leadWithPr7g (Studio& studio, Maker& maker)
    {
        maker.attach (studio);
        const auto id = studio.addTrack (Instrument {});
        Insert insert;
        auto instance = studio.makePr7gForTests();
        insert.plugin = instance->getPluginDescription();
        REQUIRE (studio.addInsert (id, insert, std::move (instance)));
        return id;
    }
}

TEST_CASE ("0.205.0: the sections' scale table", "[tones205]")
{
    CHECK (sectionToneScale ("Chorus") == SectionToneScale {});
    CHECK (sectionToneScale ("Chorus 2") == SectionToneScale {});
    CHECK (sectionToneScale ("Hook") == SectionToneScale {});
    const auto verse = sectionToneScale ("Verse 1"), bridge = sectionToneScale ("Bridge"), breakdown = sectionToneScale ("Breakdown");
    CHECK (verse.delay < 1.0f);
    CHECK (verse.reverb < 1.0f);
    CHECK (bridge.reverb > 1.0f);
    CHECK (bridge.delay > 1.0f);
    CHECK (breakdown.delay < verse.delay);
    CHECK (sectionToneScale ("Pre-Chorus").delay == 0.85f);
    CHECK (sectionToneScale ("Post-Chorus").reverb == 1.2f);
    CHECK (sectionToneScale ("Interlude") == sectionToneScale ("Instrumental"));
    CHECK (sectionToneScale ("Intro").reverb == 1.4f);
    CHECK (sectionToneScale ("Outro").reverb == 1.5f);
    CHECK (sectionToneScale ("Coda") == sectionToneScale ("Outro"));
    CHECK (sectionToneScale ("Solo").delay == 1.15f);
    CHECK (sectionToneScale ("Not A Section") == SectionToneScale {});

    // The values: the baseline times the scale, inside each knob's range; a knob the baseline lacks left out.
    const std::map<juce::String, float> baseline { { "modMix", 0.5f }, { "delayMix", 0.9f }, { "delayFeedback", 0.9f }, { "reverbMix", 0.5f } };
    const auto chorus = sectionToneValues (baseline, sectionToneScale ("Chorus"));
    CHECK (chorus.at ("modMix") == 0.5f);
    CHECK (chorus.count ("reverbSize") == 0);
    const auto intro = sectionToneValues (baseline, sectionToneScale ("Intro"));
    CHECK (intro.at ("delayMix") == 1.0f);         // 0.9 x 1.3 kept at the top
    CHECK (intro.at ("delayFeedback") == 0.95f);   // 0.9 x 1.1 kept at PR7-G's 0.95
    CHECK_THAT (intro.at ("reverbMix"), WithinAbs (0.7, 1.0e-6));
    CHECK (sectionToneParameters().size() == 5);

    // The baseline: one of our tones by the rig's name, else the instance's own knobs.
    juce::XmlElement ours ("ParagonX1");
    ours.setAttribute ("rigName", "SOARING LEAD");
    const auto soaring = sectionToneBaseline (ours);
    CHECK (soaring.count ("modMix") == 0);   // (it doesn't use the modulation)
    CHECK_THAT (soaring.at ("reverbMix"), WithinAbs (0.2, 1.0e-6));
    CHECK_THAT (sectionToneBaseline (*myRig()).at ("delayFeedback"), WithinAbs (0.4, 1.0e-6));
}

TEST_CASE ("0.205.0: PR7-G's knobs written block by block over the SECTIONS row", "[tones205]")
{
    Studio studio (false);
    Maker maker;
    const auto id = leadWithPr7g (studio, maker);
    juce::String error;
    CHECK_FALSE (studio.writeSectionTones (id, &error));   // no blocks yet
    CHECK (error.contains ("SECTIONS"));
    addRow (studio);

    const auto before = studio.getProject().findTrack (id)->automation.size();
    REQUIRE (studio.writeSectionTones (id, &error));
    CHECK (studio.getSectionToneBlocks() == 3);
    const auto* track = studio.getProject().findTrack (id);
    CHECK (track->automationRead);
    CHECK (track->automation.size() == before + 5);
    const auto written = track->automation;

    // Input Gain 0, Mod Mix 1, Delay Mix 2, Delay Feedback 3, Reverb Mix 4, Reverb Size 5 (with their real tops);
    // the verse 0..16, chorus 16..32, bridge 32..48.
    const struct { int index; float base, maximum; } knobs[] { { 1, 0.5f, 1.0f }, { 2, 0.25f, 1.0f }, { 3, 0.4f, 0.95f },
                                                                { 4, 0.3f, 1.0f }, { 5, 0.6f, 1.0f } };
    const float verse[] { 0.8f, 0.6f, 0.85f, 0.7f, 0.9f }, bridge[] { 1.2f, 1.25f, 1.1f, 1.3f, 1.15f };
    for (int k = 0; k < 5; ++k)
    {
        const auto* lane = laneOf (studio, id, knobs[k].index);
        REQUIRE (lane != nullptr);
        CHECK (lane->slot == 0);
        CHECK_FALSE (lane->shown);   // made hidden
        const auto normal = [&] (float scale) { return juce::jmin (knobs[k].maximum, knobs[k].base * scale) / knobs[k].maximum; };
        const auto& p = lane->points;
        REQUIRE (p.size() == 6);
        CHECK (p[0].beat == 0.0);
        CHECK_THAT (p[1].beat, WithinAbs (16.0 - 1.0 / 960.0, 1.0e-9));
        CHECK (p[2].beat == 16.0);
        CHECK (p[4].beat == 32.0);
        CHECK_THAT (p[5].beat, WithinAbs (48.0 - 1.0 / 960.0, 1.0e-9));
        CHECK_THAT (p[0].value, WithinAbs (normal (verse[k]), 1.0e-5));
        CHECK_THAT (p[1].value, WithinAbs (normal (verse[k]), 1.0e-5));
        CHECK_THAT (p[2].value, WithinAbs (normal (1.0f), 1.0e-5));
        CHECK_THAT (p[4].value, WithinAbs (normal (bridge[k]), 1.0e-5));
    }
    CHECK (laneOf (studio, id, 0) == nullptr);   // Input Gain left alone

    // One undo step: undone, no lanes; redone, back.
    studio.undo();
    CHECK (studio.getProject().findTrack (id)->automation.size() == before);
    studio.redo();
    CHECK (studio.getProject().findTrack (id)->automation == written);

    // Written again: the same points, none doubled; the owner's point inside the row replaced, the one after it kept.
    const auto* size = laneOf (studio, id, 5);
    const auto sizeIndex = (int) (size - studio.getAutomation (id)->data());
    auto points = size->points;
    points.push_back ({ 6.0, 0.95f });
    points.push_back ({ 60.0, 0.1f });
    studio.setAutomationPoints (id, sizeIndex, points);
    REQUIRE (studio.writeSectionTones (id));
    const auto& again = laneOf (studio, id, 5)->points;
    REQUIRE (again.size() == 7);
    CHECK (again[6].beat == 60.0);
    CHECK (again[6].value == 0.1f);
    for (int k = 0; k < 5; ++k)
        if (knobs[k].index != 5)
            CHECK (laneOf (studio, id, knobs[k].index)->points == written[(size_t) (laneOf (studio, id, knobs[k].index) - studio.getAutomation (id)->data())].points);
}

TEST_CASE ("0.205.0: the engine plays the block's value", "[tones205]")
{
    Studio studio (false);
    Maker maker;
    const auto id = leadWithPr7g (studio, maker);
    addRow (studio);
    REQUIRE (studio.writeSectionTones (id));
    auto* fake = maker.made.front();
    studio.setPosition (14.0);
    studio.play();
    renderSamples (studio.getEngine(), 2048);
    CHECK_THAT (fake->knobs[4]->getValue(), WithinAbs (0.3 * 0.7, 1.0e-4));   // the verse's Reverb Mix
    studio.stop();
    studio.setPosition (17.0);
    studio.play();
    renderSamples (studio.getEngine(), 2048);
    CHECK_THAT (fake->knobs[4]->getValue(), WithinAbs (0.3, 1.0e-4));         // the chorus's
    studio.stop();
}

TEST_CASE ("0.205.0: BUILD writes them; the switch off writes none; no PR7-G: false", "[tones205]")
{
    {
        Studio studio (false);
        Maker maker;
        maker.attach (studio);
        addRow (studio);
        std::vector<ChordSpan> spans (1);
        spans[0].end = 4.0;
        CHECK (studio.getSectionTones());
        studio.buildSection ({ leadIdea() }, spans, "Verse");
        REQUIRE (maker.made.size() == 1);
        const Track* lead = nullptr;
        for (const auto& t : studio.getProject().tracks)
            if (t.name == studio.newIdeaTrackName (LineIdea::Part::lead))
                lead = &t;
        REQUIRE (lead != nullptr);
        // The tone BUILD chose (one of ours) is the baseline: its reverb in the chorus, drier in the verse.
        const auto* tone = toneNamed (readPr7gSettings (maker.made[0]->bytes)->getStringAttribute ("rigName"));
        REQUIRE (tone != nullptr);
        const auto* reverb = laneOf (studio, lead->id, 4);
        REQUIRE (reverb != nullptr);
        REQUIRE (reverb->points.size() == 6);
        CHECK (reverb->points[2].value > reverb->points[0].value);
        CHECK (studio.getProject().findTrack (lead->id)->automationRead);
    }
    {
        Studio studio (false);
        Maker maker;
        maker.attach (studio);
        addRow (studio);
        studio.setSectionTones (false);
        CHECK_FALSE (studio.getSectionTones());
        std::vector<ChordSpan> spans (1);
        spans[0].end = 4.0;
        studio.buildSection ({ leadIdea() }, spans, "Verse");
        REQUIRE (maker.made.size() == 1);
        for (const auto& t : studio.getProject().tracks)
            for (const auto& l : t.automation)
                CHECK (l.target != AutomationLane::Target::insert);
    }
    {
        Studio studio (false);
        addRow (studio);
        const auto id = studio.addTrack (Instrument {});
        juce::String error;
        CHECK_FALSE (studio.writeSectionTones (id, &error));
        CHECK (error.contains ("PR7-G"));
        // PR7-G without the knobs (an older one): nothing written.
        Maker maker;
        maker.withKnobs = false;
        const auto other = leadWithPr7g (studio, maker);
        CHECK_FALSE (studio.writeSectionTones (other, &error));
        CHECK (error.contains ("knobs"));
    }
}

TEST_CASE ("0.205.0: the Hub's TONES CHANGE WITH THE SECTIONS", "[tones205][ui]")
{
    Studio studio (false);
    ComposerTracksPage page (studio);
    page.setSize (1200, 800);
    CHECK (page.getSectionTonesKey().getToggleState());
    page.setSectionTones (false);
    CHECK_FALSE (studio.getSectionTones());
    CHECK_FALSE (page.getSectionTonesKey().getToggleState());
    page.getSectionTonesKey().setToggleState (true, juce::dontSendNotification);
    page.getSectionTonesKey().onClick();
    CHECK (studio.getSectionTones());
    CHECK (page.getSectionTonesKey().isVisible());
}
