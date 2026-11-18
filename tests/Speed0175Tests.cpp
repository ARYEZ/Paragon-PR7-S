// Speed, round eight (0.175.0): the changes give exactly what they gave before.

#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/Composer.h"
#include "engine/SectionTypes.h"
#include "engine/Studio.h"
#include "ui/ArrangeView.h"
#include "ui/ComposerPage.h"
#include "ui/Finish.h"
#include "ui/KeyEditor.h"
#include "ui/MainView.h"

using namespace pr7s;

namespace
{
    bool sameNotes (const LineIdea& a, const LineIdea& b)
    {
        if (a.notes.size() != b.notes.size() || a.style != b.style)
            return false;
        for (size_t i = 0; i < a.notes.size(); ++i)
            if (a.notes[i].beat != b.notes[i].beat || a.notes[i].length != b.notes[i].length || a.notes[i].pitch != b.notes[i].pitch
                || a.notes[i].velocity != b.notes[i].velocity || a.notes[i].keyswitch != b.notes[i].keyswitch)
                return false;
        return true;
    }

    std::vector<ChordSpan> eightBars()
    {
        Key aMinor;
        for (const auto& k : Key::all())
            if (k.minor && k.pitchClass() == 9)
                aMinor = k;
        std::vector<ChordSpan> spans;
        for (int bar = 0; bar < 8; ++bar)
            spans.push_back ({ bar * 4.0, bar * 4.0 + 4.0, Chord { bar % 2 == 0 ? 9 : 5, bar % 2 == 0 ? Chord::Type::minor : Chord::Type::major }, aMinor });
        return spans;
    }

    // What a strip's redraw paints is what a whole redraw paints there (the paint is culled, not different).
    int stripDifferences (juce::Component& c, juce::Rectangle<int> strip)
    {
        juce::Image whole (juce::Image::RGB, c.getWidth(), c.getHeight(), true, juce::SoftwareImageType());
        {
            juce::Graphics g (whole);
            c.paint (g);
        }
        juce::Image part (juce::Image::RGB, c.getWidth(), c.getHeight(), true, juce::SoftwareImageType());
        {
            juce::Graphics g (part);
            g.drawImageAt (whole, 0, 0);
            g.reduceClipRegion (strip);
            g.fillAll (juce::Colours::black);
            c.paint (g);
        }
        auto far = 0;
        for (int y = 0; y < whole.getHeight(); ++y)
            for (int x = 0; x < whole.getWidth(); ++x)
            {
                const auto p = whole.getPixelAt (x, y), q = part.getPixelAt (x, y);
                far += juce::jmax (std::abs (p.getRed() - q.getRed()), std::abs (p.getGreen() - q.getGreen()), std::abs (p.getBlue() - q.getBlue())) > 24 ? 1 : 0;
            }
        return far;
    }
}

TEST_CASE ("0.175.0: chasing walks back from where playing starts and finds each controller's last value", "[speed175][controllers]")
{
    Engine engine;
    engine.prepare (48000.0, 512);
    Project project;
    Track track;
    track.id = 1;
    track.midiChannel = 3;
    Part part;
    part.length = 400.0;
    // CC 7 only at the very start (the walk back must reach it), CC 1 every eighth all the way, CC 64 a few
    // times, a bend in the middle, CC 11 only after where playing starts.
    part.controllers.push_back ({ Controller::Type::cc, 7, 0.25, 77, false });
    for (int i = 0; i < 3200; ++i)
        part.controllers.push_back ({ Controller::Type::cc, 1, i * 0.125, (i * 37) % 128, false });
    part.controllers.push_back ({ Controller::Type::cc, 64, 10.0, 127, false });
    part.controllers.push_back ({ Controller::Type::cc, 64, 150.0, 0, false });
    part.controllers.push_back ({ Controller::Type::pitchbend, 0, 120.0, 3000, false });
    part.controllers.push_back ({ Controller::Type::cc, 11, 390.0, 50, false });
    std::sort (part.controllers.begin(), part.controllers.end(), [] (const Controller& a, const Controller& b) { return a.start < b.start; });
    track.parts = { part };
    project.tracks.push_back (track);
    auto instrument = std::make_unique<Probe>();
    auto* probe = instrument.get();
    engine.setInstrument (1, std::move (instrument));
    engine.update (project);

    for (const auto start : { 0.1, 0.3, 77.06, 200.0, 299.99 })
    {
        INFO ("from beat " << start);
        probe->received.clear();
        engine.stop();
        renderSamples (engine, 512);
        probe->received.clear();
        engine.setPosition (start);
        engine.play();
        renderSamples (engine, 64);
        std::map<int, int> cc;
        auto bendValue = -1;
        for (const auto& r : probe->received)
        {
            if (r.sample != 0)
                continue;
            CHECK (r.message.getChannel() == 3);
            if (r.message.isController())
                cc[r.message.getControllerNumber()] = r.message.getControllerValue();
            if (r.message.isPitchWheel())
                bendValue = r.message.getPitchWheelValue();
        }
        // The last of each kind before the start, worked out the slow way.
        std::map<int, int> expected;
        auto expectedBend = -1;
        for (const auto& c : part.controllers)
            if (c.start < start - 1.0e-9)
            {
                if (c.type == Controller::Type::cc)
                    expected[c.number] = c.value;
                else if (c.type == Controller::Type::pitchbend)
                    expectedBend = c.value;
            }
        CHECK (cc == expected);
        CHECK (bendValue == expectedBend);
    }
}

TEST_CASE ("0.175.0: one lane made alone is that lane of all of them", "[speed175][composer]")
{
    const auto spans = eightBars();
    ComposerContext c;
    c.humanize = 0.4f;
    ComposerState s;
    for (const auto l : composerLayers())
        s.lane (l).on = true;
    for (const auto variation : { 0, 5 })
    {
        for (const auto l : composerLayers())
            s.lane (l).variation = variation;
        const auto all = composeLayers (s, spans, c);
        for (const auto l : composerLayers())
        {
            INFO ("lane " << composerLayerName (l) << " variation " << variation);
            CHECK (sameNotes (composeLayer (l, s, spans, c), all[(size_t) l]));
        }
    }
}

TEST_CASE ("0.175.0: progressions found again are the ones found the first time", "[speed175][composer]")
{
    ComposerState s;
    const auto first = composerProgressions (s);
    CHECK (composerProgressions (s) == first);
    auto minor = s;
    for (const auto& k : Key::all())
        if (k.minor && k.pitchClass() == 9)
            minor.key = k;
    const auto inMinor = composerProgressions (minor);
    CHECK (composerProgressions (minor) == inMinor);
    CHECK (inMinor != first);
    CHECK (composerProgressions (s) == first);
}

TEST_CASE ("0.175.0: the drums lane's options under a type are each what they'd be made alone", "[speed175][composer]")
{
    const auto spans = eightBars();
    ComposerContext c;
    ComposerState s;
    s.lane (ComposerLayer::drums).on = true;
    s.lane (ComposerLayer::guitar).on = true;
    const auto types = sectionTypesFor ("Chorus");
    REQUIRE_FALSE (types.empty());
    applySectionType (s, "Chorus", types.front().name);
    REQUIRE (sectionTypeWritesLane (ComposerLayer::drums, s, c));
    const auto choices = composerChoices (ComposerLayer::drums, s, spans, c);
    REQUIRE (choices.size() > 3);
    for (size_t i = 0; i < choices.size(); ++i)
    {
        auto each = s;
        each.lane (ComposerLayer::drums).approach = (int) i;
        INFO ("option " << i);
        CHECK (sameNotes (choices[i], composeLayer (ComposerLayer::drums, each, spans, c)));
    }
}

TEST_CASE ("0.175.0: changing a block's chords copies only the tracks whose notes move", "[speed175][sections]")
{
    Studio studio (false);
    const auto first = studio.getProject().tracks[0].id;
    studio.addTrack ({});
    const auto second = studio.getProject().tracks.back().id;
    REQUIRE (studio.appendSongSection ("Verse", 4) == 0);
    REQUIRE (studio.appendSongSection ("Chorus", 4) == 1);
    REQUIRE (studio.setSectionChords (0, { { Chord { 9, Chord::Type::minor }, 8.0 }, { Chord { 5 }, 8.0 } }));
    REQUIRE (studio.setSectionChords (1, { { Chord { 0 }, 16.0 } }));
    Part inVerse;   // (an A over A minor: the verse's chords move it)
    inVerse.start = 0.0;
    inVerse.length = 16.0;
    inVerse.notes = { { 0.0, 1.0, 69, 0.8f }, { 8.0, 1.0, 65, 0.8f } };
    studio.addPart (first, inVerse);
    Part inChorus;   // (only in the chorus: nothing of it moves)
    inChorus.start = 16.0;
    inChorus.length = 16.0;
    inChorus.notes = { { 0.0, 1.0, 72, 0.8f } };
    studio.addPart (second, inChorus);

    const auto before = studio.getProject();
    REQUIRE (studio.setSectionChords (0, { { Chord { 0 }, 8.0 }, { Chord { 7 }, 8.0 } }));
    CHECK (studio.getProject().findTrack (second)->parts.sharesWith (before.findTrack (second)->parts));
    CHECK_FALSE (studio.getProject().findTrack (first)->parts.sharesWith (before.findTrack (first)->parts));
    CHECK (studio.getProject().findTrack (first)->parts[0].notes[0].pitch != 69);
}

TEST_CASE ("0.175.0: the MIDI editor's and the arrangement's playhead strips draw what a whole redraw draws", "[speed175][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    view.setKeyEditorSize (100);
    const auto id = studio.getProject().tracks[0].id;
    for (int t = 0; t < 3; ++t)
    {
        Part part;
        part.start = t * 4.0;
        part.length = 12.0;
        for (int i = 0; i < 40; ++i)
            part.notes.push_back ({ i * 0.25, 0.2, 48 + (i * 5) % 24, 0.8f });
        studio.addPart (id, part);
    }
    studio.dispatchPendingMessages();
    view.getArrangeView().select (id, 1);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    auto& editor = view.getKeyEditor();
    REQUIRE (editor.getPart() != nullptr);
    editor.selectAll();
    auto& arrange = view.getArrangeView();
    for (const auto x : { 300, 650, 1100 })
    {
        INFO ("strip at " << x);
        CHECK (stripDifferences (editor, { x - 7, 0, 15, editor.getHeight() }) == 0);
        CHECK (stripDifferences (arrange, { x - 7, 0, 15, arrange.getHeight() }) == 0);
    }
}

TEST_CASE ("0.175.0: ARCHITECT's playhead strips draw what a whole redraw draws", "[speed175][ui][composer]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    ComposerPage page (studio);
    page.setSize (1700, 940);
    page.setRandomSeed (3);
    page.generate();
    for (const auto x : { 700, 1200, 1600 })
    {
        INFO ("strip at " << x);
        CHECK (stripDifferences (page, { x - 3, 0, 8, page.getHeight() }) == 0);
        CHECK (stripDifferences (page, { x - 3, 400, 8, 40 }) == 0);
    }
}
