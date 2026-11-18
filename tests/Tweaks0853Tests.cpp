// 0.85.3, the owner's three: one take by default, the studio's X back to the Hub, chords edited like clips.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/ArrangeView.h"
#include "ui/Finish.h"
#include "ui/Hub.h"
#include "ui/MainView.h"

using namespace pr7s;
using Catch::Approx;

namespace
{
    Chord chordOf (int root, Chord::Type type = Chord::Type::major)
    {
        Chord c;
        c.root = root;
        c.type = type;
        return c;
    }

    // E minor's i, VI, III, VII (E G C D), a bar each (4/4).
    void fourChords (Studio& studio)
    {
        studio.addChordTrack();
        studio.setChordEvents ({ { 0.0, chordOf (4, Chord::Type::minor) }, { 4.0, chordOf (0) }, { 8.0, chordOf (7) }, { 12.0, chordOf (2) } });
    }

    std::vector<std::pair<double, double>> spans (const Studio& studio)   // (start, end) per chord
    {
        std::vector<std::pair<double, double>> out;
        const auto& t = studio.getChordTrack();
        for (int i = 0; i < (int) t.events.size(); ++i)
            out.push_back ({ t.events[(size_t) i].beat, t.endOf (i, 4.0) });
        return out;
    }
}

TEST_CASE ("0.85.3: recording replaces one take by default; takes on lanes when asked for", "[tweaks0853][record]")
{
    using Modes = Project::RecordModes;
    const Modes modes;
    CHECK (modes.audio == Modes::Audio::replace);
    CHECK (modes.midi == Modes::Midi::replace);
    CHECK (modes.midiCycle == Modes::MidiCycle::keepLast);
    CHECK_FALSE (modes.keepsTakesOnLanes());
    CHECK (Modes::onLanes().keepsTakesOnLanes());

    // A song saved with takes on lanes keeps them; one saved with the default stays on the default.
    Project p;
    p.recordModes = Modes::onLanes();
    auto again = Project::fromXml (*p.toXml());
    REQUIRE (again.has_value());
    CHECK (again->recordModes == Modes::onLanes());
    Project plain;
    again = Project::fromXml (*plain.toXml());
    REQUIRE (again.has_value());
    CHECK (again->recordModes == Modes {});

    // Recording MIDI twice over the same bars: one part, the second take's notes, no lanes.
    Studio studio (false);
    auto& engine = studio.getEngine();
    const auto trackId = studio.getProject().tracks[0].id;
    for (const auto pitch : { 60, 67 })
    {
        studio.setPosition (0.0);
        studio.record();
        renderSamples (engine, 24000);   // (to beat 1 at 120 BPM, 48 kHz)
        engine.getKeyboardState().noteOn (1, pitch, 0.8f);
        renderSamples (engine, 12000);
        engine.getKeyboardState().noteOff (1, pitch, 0.0f);
        renderSamples (engine, 12000);
        studio.stop();
    }
    const auto& track = *studio.getProject().findTrack (trackId);
    REQUIRE (track.parts.size() == 1);
    REQUIRE (track.parts[0].notes.size() == 1);
    CHECK (track.parts[0].notes[0].pitch == 67);
    CHECK (track.parts[0].lane == 0);
    CHECK_FALSE (track.showLanes);
}

TEST_CASE ("0.85.3: chords as clips: placed chords cut out what they cover", "[tweaks0853][chords]")
{
    // C from 0 to 8; G placed over 2-4: C 0-2, G 2-4, C 4-8.
    auto result = placeChords ({ { 0.0, chordOf (0), 8.0 } }, { { 2.0, chordOf (7), 2.0 } });
    REQUIRE (result.events.size() == 3);
    CHECK (result.events[0].beat == Approx (0.0));
    CHECK (result.events[0].length == Approx (2.0));
    CHECK (result.events[1].chord == chordOf (7));
    CHECK (result.placed[0] == 1);
    CHECK (result.events[2].beat == Approx (4.0));
    CHECK (result.events[2].length == Approx (4.0));
    CHECK (result.events[2].chord == chordOf (0));
    // Covering one whole: it goes.
    result = placeChords ({ { 0.0, chordOf (0), 4.0 }, { 4.0, chordOf (5), 4.0 } }, { { 3.0, chordOf (7), 6.0 } });
    REQUIRE (result.events.size() == 2);
    CHECK (result.events[0].length == Approx (3.0));
    CHECK (result.events[1].chord == chordOf (7));
}

TEST_CASE ("0.85.3: chords on the chord track move, lengthen, split, glue, copy and paste like clips", "[tweaks0853][chords]")
{
    Studio studio (false);
    fourChords (studio);
    REQUIRE (studio.getChordTrack().events.size() == 4);

    SECTION ("moved: a gap where it was, what it lands on cut")
    {
        // The second chord (C, 4-8) moved a bar later: 4-8 empty, it over 8-12 in D's... G's place.
        const auto placed = studio.moveChordEvents ({ 1 }, 4.0, false);
        REQUIRE (placed.size() == 1);
        const auto s = spans (studio);
        REQUIRE (s.size() == 3);
        CHECK (s[0] == std::pair<double, double> { 0.0, 4.0 });
        CHECK (s[1] == std::pair<double, double> { 8.0, 12.0 });
        CHECK (studio.getChordTrack().events[(size_t) placed[0]].chord == chordOf (0));
        CHECK (s[2] == std::pair<double, double> { 12.0, 16.0 });
        // In the gap: no chord sounds after the first (it ends at 4).
        CHECK (studio.getChordTrack().endOf (0, 4.0) == Approx (4.0));
        // One undo step.
        studio.undo();
        CHECK (studio.getChordTrack().events.size() == 4);
    }
    SECTION ("moved with a copy (Alt): the original stays")
    {
        studio.moveChordEvents ({ 0 }, 16.0, true);
        REQUIRE (studio.getChordTrack().events.size() == 5);
        CHECK (studio.getChordTrack().events.back().chord == chordOf (4, Chord::Type::minor));
        CHECK (studio.getChordTrack().events.back().beat == Approx (16.0));
    }
    SECTION ("lengthened over the next: the next gives way; shortened: a gap")
    {
        const auto index = studio.resizeChordEvent (0, 0.0, 6.0);
        CHECK (index == 0);
        auto s = spans (studio);
        CHECK (s[0] == std::pair<double, double> { 0.0, 6.0 });
        CHECK (s[1] == std::pair<double, double> { 6.0, 8.0 });   // (C, what's left of it)
        studio.resizeChordEvent (0, 0.0, 2.0);
        s = spans (studio);
        CHECK (s[0] == std::pair<double, double> { 0.0, 2.0 });
        CHECK (s[1].first == Approx (6.0));
        // The last one lengthened: it lasts as long as asked (not a bar).
        const auto last = (int) studio.getChordTrack().events.size() - 1;
        studio.resizeChordEvent (last, 12.0, 20.0);
        CHECK (studio.getChordTrack().endOf (last, 4.0) == Approx (20.0));
    }
    SECTION ("split, then glued back")
    {
        const auto halves = studio.splitChordEvents (2.0, { 0 });
        REQUIRE (halves.size() == 1);
        CHECK (studio.getChordTrack().events.size() == 5);
        CHECK (studio.getChordTrack().events[1].beat == Approx (2.0));
        CHECK (studio.getChordTrack().events[1].chord == chordOf (4, Chord::Type::minor));
        CHECK (studio.glueChordEvent (0) == 0);
        CHECK (studio.getChordTrack().events.size() == 4);
        CHECK (studio.getChordTrack().endOf (0, 4.0) == Approx (4.0));
    }
    SECTION ("copied and pasted at a beat; duplicated after themselves")
    {
        const auto clip = studio.copyChordEvents ({ 0, 1 });
        CHECK (clip.length == Approx (8.0));
        const auto pasted = studio.pasteChordEvents (clip, 16.0);
        REQUIRE (pasted.size() == 2);
        CHECK (studio.getChordTrack().events.size() == 6);
        CHECK (studio.getChordTrack().events[(size_t) pasted[1]].beat == Approx (20.0));
        // Pasted over chords: they're cut out.
        studio.pasteChordEvents (clip, 2.0);
        const auto s = spans (studio);
        CHECK (s[0] == std::pair<double, double> { 0.0, 2.0 });
        CHECK (s[1] == std::pair<double, double> { 2.0, 6.0 });
        const auto dup = studio.duplicateChordEvents ({ 0 });
        REQUIRE (dup.size() == 1);
        CHECK (studio.getChordTrack().events[(size_t) dup[0]].beat == Approx (2.0));
    }
    SECTION ("deleted: a gap left, the one before doesn't run on")
    {
        studio.removeChordEvents ({ 1 });
        REQUIRE (studio.getChordTrack().events.size() == 3);
        CHECK (studio.getChordTrack().endOf (0, 4.0) == Approx (4.0));
        CHECK (studio.getChordTrack().events[1].beat == Approx (8.0));
    }
    SECTION ("lengths saved with the song")
    {
        studio.resizeChordEvent (3, 12.0, 22.0);
        const auto xml = studio.getProject().toXml();
        const auto again = Project::fromXml (*xml);
        REQUIRE (again.has_value());
        CHECK (again->chordTrack.events == studio.getChordTrack().events);
        CHECK (again->chordTrack.endOf (3, 4.0) == Approx (22.0));
    }
}

TEST_CASE ("0.85.3: a progression onto the chord track gives each chord its own length", "[tweaks0853][chords]")
{
    Studio studio (false);
    ProgressionStep a, b;
    a.chord = chordOf (4, Chord::Type::minor);
    a.bars = 1.0;
    b.chord = chordOf (0);
    b.bars = 2.0;
    studio.addProgressionToChordTrack (std::vector<ProgressionStep> { a, b }, 0.0);
    REQUIRE (studio.getChordTrack().events.size() == 2);
    CHECK (studio.getChordTrack().endOf (1, 4.0) == Approx (12.0));   // (the last, two bars: not one)
}

TEST_CASE ("0.85.3: the key chosen goes on the key strip; an edit out of it is shown", "[tweaks0853][chords][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    fourChords (studio);
    const Key eMinor { 2, 0, true };   // (E)
    studio.setChordTrackKey (0.0, eMinor);
    CHECK_FALSE (studio.getChordTrack().autoScales);
    REQUIRE (studio.getChordTrack().scales.size() == 1);
    CHECK (studio.getChordTrack().scales[0].key == eMinor);
    CHECK (studio.chordTrackKeyAt (10.0) == eMinor);

    MainView view (studio);
    view.setSize (1600, 900);
    auto& arrange = view.getArrangeView();
    const auto click = [&arrange] (juce::Point<float> at)
    {
        const auto now = juce::Time::getCurrentTime();
        const auto event = [&] (juce::ModifierKeys mods)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, mods, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                     &arrange, &arrange, now, at, now, 1, false);
        };
        arrange.mouseDown (event (juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier)));
        arrange.mouseUp (event ({}));
    };
    CHECK (arrange.getChordWarning().isEmpty());
    // The second chord made C sharp major (outside E minor), picked, copied and pasted at bar 5:
    // the arrangement says the pasted one is out of the key.
    studio.setChordEventChord (1, chordOf (1));
    REQUIRE (arrange.getChordEventBounds (1).getWidth() > 0.0f);
    click (arrange.getChordEventBounds (1).getCentre());
    REQUIRE (arrange.getChordSelection() == std::vector<int> { 1 });
    arrange.copySelectedChords (false);
    studio.setPosition (16.0);
    REQUIRE (arrange.pasteChords());
    CHECK (arrange.getChordWarning().startsWith ("OUT OF KEY: C#"));
    CHECK (arrange.getChordWarning().contains ("BAR 5"));
    CHECK (arrange.getChordWarning().contains ("E MINOR"));
    // An edit that stays in the key says nothing new.
    REQUIRE (arrange.getChordEventBounds (0).getWidth() > 0.0f);
    click (arrange.getChordEventBounds (0).getCentre());
    arrange.duplicateSelectedChords();
    CHECK (arrange.getChordWarning().startsWith ("OUT OF KEY: C#"));   // (still the last one's, for its few seconds)
}

TEST_CASE ("0.85.3: the studio's X goes back to the Hub; the Hub's X then quits", "[tweaks0853][hub][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    const auto root = juce::File::createTempFile ("pr7s-hub0853");
    root.createDirectory();
    Studio studio (false);
    studio.getLibrary().setDefaultLocation (root);
    const auto file = root.getChildFile ("Song").getChildFile ("Song.pr7s");
    file.getParentDirectory().createDirectory();
    REQUIRE (studio.save (file));

    auto view = std::make_unique<MainView> (studio);
    juce::DocumentWindow window ("test", juce::Colours::black, 0);
    window.setContentNonOwned (view.get(), false);
    window.setSize (1400, 800);
    window.setVisible (true);
    auto quits = 0;
    view->onQuit = [&quits] { ++quits; };

    view->closeToHub();   // (nothing unsaved: no question)
    CHECK (view->isAtHub());
    CHECK_FALSE (window.isVisible());
    CHECK (studio.getFile() == juce::File());   // (the song closed)
    REQUIRE (view->getHubWindow() != nullptr);

    // A song chosen in the Hub: the studio comes back with it.
    auto& hub = dynamic_cast<HubWindow&> (*view->getHubWindow()).getHub();
    hub.onOpen (file);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    CHECK (studio.getFile() == file);
    CHECK_FALSE (view->isAtHub());
    CHECK (window.isVisible());
    CHECK (view->getHubWindow() == nullptr);
    CHECK (quits == 0);

    // The HUB key over a song: its X only closes it.
    view->openHub();
    REQUIRE (view->getHubWindow() != nullptr);
    view->getHubWindow()->closeButtonPressed();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    CHECK (quits == 0);
    CHECK (window.isVisible());

    // Back at the Hub alone: its X quits.
    view->closeToHub();
    REQUIRE (view->getHubWindow() != nullptr);
    view->getHubWindow()->closeButtonPressed();
    CHECK (quits == 1);
    window.clearContentComponent();
    view.reset();
    root.deleteRecursively();
}
