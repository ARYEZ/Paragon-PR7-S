#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/MainView.h"
#include "ui/Windows.h"

// 0.31.0 (the owner asked): Render in Place's new defaults (32-bit float; the original muted, disabled and
// hidden), only the cycle's or range's stretch, Alt+R; Q to the header's quantize value from anywhere; the
// MIDI and drum editors' INSTRUMENT key.

using Catch::Matchers::WithinAbs;
using namespace pr7s;

TEST_CASE ("Render in Place's defaults: 32-bit float, the original muted, disabled and hidden", "[render][031]")
{
    const Studio::RenderOptions defaults;
    CHECK (defaults.bitsPerSample == 32);
    CHECK (defaults.source == Studio::RenderOptions::Source::disable);
    CHECK (defaults.hideSource);
    // (0.77.0) Always: whatever the page was given, and settings kept from before can't change it.
    {
        Studio studio (false);
        Windows::RenderPage parts (studio, false), tracks (studio, true);
        for (auto* page : { &parts, &tracks })
        {
            CHECK (page->getOptions().source == Studio::RenderOptions::Source::disable);
            CHECK (page->getOptions().hideSource);
        }
    }

    // Settings kept from before (the old defaults) are moved to the new ones once; a choice after that stays.
    const auto file = juce::File::createTempFile (".settings");
    {
        juce::PropertiesFile::Options o;
        juce::PropertiesFile settings (file, o);
        settings.setValue ("renderBits", 24);
        settings.setValue ("renderSource", 0);
        settings.setValue ("renderHide", false);
        auto kept = Windows::RenderPage::keptOptions (&settings);
        CHECK (kept.bitsPerSample == 32);
        CHECK (kept.source == Studio::RenderOptions::Source::disable);
        CHECK (kept.hideSource);
        settings.setValue ("renderBits", 16);
        kept = Windows::RenderPage::keptOptions (&settings);
        CHECK (kept.bitsPerSample == 16);
    }
    file.deleteFile();

    // Rendered with them: a 32-bit file; the source muted, disabled and hidden.
    Studio studio (false);
    const auto synth = studio.getProject().tracks[0].id;
    studio.addPart (synth, { 0.0, 4.0, "Riff", { { 0.0, 1.0, 60, 1.0f } } });
    Studio::RenderOptions options;
    options.tail = Studio::RenderOptions::Tail::off;
    juce::String error;
    const auto made = studio.renderInPlace ({ { synth, 0.0, 4.0, true } }, options, error);
    REQUIRE (made.size() == 1);
    const auto* source = studio.getProject().findTrack (synth);
    CHECK (source->mute);
    CHECK (source->disabled);
    CHECK (source->hidden);
    const auto& clip = studio.getProject().findTrack (made[0])->parts.at (0);
    std::unique_ptr<juce::AudioFormatReader> reader (juce::WavAudioFormat().createReaderFor (
        studio.getAudioFolder().getChildFile (clip.audioFile).createInputStream().release(), true));
    REQUIRE (reader != nullptr);
    CHECK (reader->bitsPerSample == 32);
    CHECK (reader->usesFloatingPointData);

    // (0.77.0, the owner: "render in place always means mute / disable / hide") A stretch of a track (a range,
    // a part of a take): the parts rendered there muted, and the track muted, disabled and hidden too.
    Studio other (false);
    const auto keys = other.getProject().tracks[0].id;
    other.addPart (keys, { 0.0, 8.0, "Long", { { 0.0, 1.0, 60, 1.0f }, { 5.0, 1.0, 64, 1.0f } } });
    const auto section = other.renderInPlace ({ { keys, 4.0, 8.0, false } }, options, error);
    REQUIRE (section.size() == 1);
    const auto* kept = other.getProject().findTrack (keys);
    CHECK (kept->mute);
    CHECK (kept->disabled);
    CHECK (kept->hidden);
    for (const auto& p : kept->parts)
        CHECK (p.muted == (p.start >= 4.0 - 1.0e-9));
    // The render has sound (the track was disabled after it, not before).
    const auto* sectionTrack = other.getProject().findTrack (section[0]);
    REQUIRE (sectionTrack->parts.size() == 1);
    const auto sectionSound = other.getAudio (sectionTrack->parts[0].audioFile);
    REQUIRE (sectionSound != nullptr);
    CHECK (sectionSound->sound.getMagnitude (0, sectionSound->sound.getNumSamples()) > 0.01f);

    // Two stretches of one track at once (two parts selected): both rendered with sound, then the track disabled.
    Studio two (false);
    const auto lead = two.getProject().tracks[0].id;
    two.addPart (lead, { 0.0, 4.0, "A", { { 0.0, 1.0, 60, 1.0f } } });
    two.addPart (lead, { 8.0, 4.0, "B", { { 0.0, 1.0, 64, 1.0f } } });
    const auto both = two.renderInPlace ({ { lead, 0.0, 4.0, false }, { lead, 8.0, 12.0, false } }, options, error);
    REQUIRE (both.size() == 2);
    for (const auto id : both)
    {
        const auto* t = two.getProject().findTrack (id);
        REQUIRE (t->parts.size() == 1);
        const auto data = two.getAudio (t->parts[0].audioFile);
        REQUIRE (data != nullptr);
        CHECK (data->sound.getMagnitude (0, data->sound.getNumSamples()) > 0.01f);
    }
    CHECK (two.getProject().findTrack (lead)->disabled);
    CHECK (two.getProject().findTrack (lead)->hidden);
    two.getAudioFolder().deleteRecursively();
    studio.getAudioFolder().deleteRecursively();
    other.getAudioFolder().deleteRecursively();
}

TEST_CASE ("Render in Place of whole tracks takes only the range, or the cycle; Alt+R renders as last time", "[render][031][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto synth = studio.getProject().tracks[0].id;
    studio.addPart (synth, { 0.0, 16.0, "Song", { { 0.0, 1.0, 60, 1.0f }, { 8.0, 1.0, 64, 1.0f } } });
    studio.dispatchPendingMessages();
    const std::vector<Studio::RenderSource> whole { { synth, 0.0, 16.0, true } };

    // No cycle, no range: the whole track.
    studio.setCycleOn (false);
    auto narrowed = view.narrowedSources (whole);
    REQUIRE (narrowed.size() == 1);
    CHECK (narrowed[0].wholeTrack);
    // CYCLE on: its stretch only, still a whole track (0.52.1: its original can be disabled and hidden),
    // with the warning that parts outside the cycle go silent with it.
    studio.setCycle (4.0, 12.0);
    studio.setCycleOn (true);
    narrowed = view.narrowedSources (whole);
    REQUIRE (narrowed.size() == 1);
    CHECK (narrowed[0].wholeTrack);
    CHECK (view.renderWarning (narrowed).contains ("CYCLE is on: only bars 2 to 3 will be rendered"));   // (beats 4-12 in 4/4)
    {
        Windows::RenderPage page (studio, narrowed[0].wholeTrack);
        page.setWarning (view.renderWarning (narrowed));
        CHECK (page.getOptions().source == Studio::RenderOptions::Source::disable);
        CHECK (page.getOptions().hideSource);
        CHECK (page.getWarning().isNotEmpty());
    }
    // The cycle over all of it: no warning.
    studio.setCycle (0.0, 16.0);
    CHECK (view.renderWarning (view.narrowedSources (whole)).isEmpty());
    studio.setCycle (4.0, 12.0);
    CHECK_THAT (narrowed[0].start, WithinAbs (4.0, 1.0e-9));
    CHECK_THAT (narrowed[0].end, WithinAbs (12.0, 1.0e-9));
    // A range set: the range, over the cycle.
    view.getArrangeView().setRange (Studio::Range { 2.0, 6.0, { synth } });
    narrowed = view.narrowedSources (whole);
    REQUIRE (narrowed.size() == 1);
    CHECK_THAT (narrowed[0].start, WithinAbs (2.0, 1.0e-9));
    CHECK_THAT (narrowed[0].end, WithinAbs (6.0, 1.0e-9));
    // Parts asked for stay as they are.
    narrowed = view.narrowedSources ({ { synth, 0.0, 16.0, false } });
    CHECK_THAT (narrowed[0].start, WithinAbs (0.0, 1.0e-9));
    view.getArrangeView().setRange (std::nullopt);

    // Alt+R: the selected track over the cycle, as last time (no page): a new track; the source's parts there muted.
    studio.selectTrack (synth);
    view.getArrangeView().selectParts ({});
    const auto before = studio.getProject().tracks.size();
    CHECK (view.keyPressed (juce::KeyPress ('r', juce::ModifierKeys::altModifier, 'r')));
    REQUIRE (studio.getProject().tracks.size() == before + 1);
    const auto& rendered = studio.getProject().tracks.back().isAudio() ? studio.getProject().tracks.back() : studio.getProject().tracks[1];
    REQUIRE (rendered.isAudio());
    REQUIRE_FALSE (rendered.parts.empty());
    CHECK_THAT (rendered.parts.front().start, WithinAbs (4.0, 1.0e-6));
    // (0.52.1) A whole track over the cycle: the defaults, disabled and hidden.
    CHECK (studio.getProject().findTrack (synth)->disabled);
    CHECK (studio.getProject().findTrack (synth)->hidden);
    CHECK_FALSE (studio.getEngine().isRecording());                  // (not R)
    studio.getAudioFolder().deleteRecursively();
}

TEST_CASE ("Q quantizes to the header's quantize value: the editor's notes, or the selected parts", "[quantize][031][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addPart (trackId, { 0.0, 8.0, "Loose", { { 0.9, 0.5, 60, 0.8f }, { 2.2, 0.5, 64, 0.8f } } });
    studio.dispatchPendingMessages();
    const auto notes = [&] { return studio.getProject().findTrack (trackId)->parts.at (0).notes; };

    // The selected part, from the arrangement, to 1/4 (a beat).
    studio.setQuantizeBeats (1.0);
    view.getArrangeView().select (trackId, 0);
    CHECK (view.keyPressed (juce::KeyPress ('q', {}, 'q')));
    CHECK_THAT (notes()[0].start, WithinAbs (1.0, 1.0e-9));
    CHECK_THAT (notes()[1].start, WithinAbs (2.0, 1.0e-9));
    CHECK_THAT (notes()[0].length, WithinAbs (0.5, 1.0e-9));   // (lengths kept)
    studio.undo();

    // The MIDI editor open on it, nothing selected in the arrangement: the editor's notes, to 1/2 (two beats).
    view.getArrangeView().select (trackId, 0);   // (the editor follows the arrangement's part)
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    studio.dispatchPendingMessages();
    view.getArrangeView().selectParts ({});
    studio.setQuantizeBeats (2.0);
    studio.dispatchPendingMessages();
    CHECK (view.getKeyEditor().getQuantizeKey().getButtonText() == "QUANTIZE 1/2");
    CHECK (view.keyPressed (juce::KeyPress ('q', {}, 'q')));
    CHECK_THAT (notes()[0].start, WithinAbs (0.0, 1.0e-9));
    CHECK_THAT (notes()[1].start, WithinAbs (2.0, 1.0e-9));

    // 1/64 (a sixteenth of a beat), from the editor's QUANTIZE key.
    studio.setNotes (trackId, 0, { { 0.05, 0.5, 60, 0.8f } });   // (nearest sixteenth of a beat: 0.0625)
    studio.setQuantizeBeats (1.0 / 16.0);
    view.getKeyEditor().getQuantizeKey().onClick();
    CHECK_THAT (notes()[0].start, WithinAbs (0.0625, 1.0e-9));
}

TEST_CASE ("The MIDI and drum editors' INSTRUMENT key opens the track's instrument window", "[editor][031][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addPart (trackId, { 0.0, 4.0, "Riff", { { 0.0, 1.0, 60, 0.8f } } });
    view.getArrangeView().select (trackId, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    studio.dispatchPendingMessages();
    auto& key = view.getKeyEditor().getInstrumentKey();
    CHECK (key.isEnabled());
    CHECK (key.isVisible());
    // (The test synth has no window of its own, PR7-K does: what's checked is that MainView is asked for this track's.)
    auto asked = 0;
    auto opens = view.getKeyEditor().onOpenInstrument;
    REQUIRE (opens != nullptr);
    view.getKeyEditor().onOpenInstrument = [&] (int id) { asked = id; opens (id); };
    key.onClick();
    CHECK (asked == trackId);
    CHECK_FALSE (view.isInstrumentWindowOpen (trackId));   // (no editor to show: nothing opened, nothing broken)
    // In the drum view too (the same panel).
    studio.setTrackDrumEditor (trackId, true);
    studio.dispatchPendingMessages();
    CHECK (key.isEnabled());
    asked = 0;
    key.onClick();
    CHECK (asked == trackId);
}

#include "ui/ScaledHolder.h"

TEST_CASE ("0.52.1: the export page shown bigger follows its own size; plug-in windows open on the right-hand screen", "[render][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    auto* page = new Windows::ExportPage (studio);
    ScaledPage scaled (std::unique_ptr<juce::Component> (page), 2.0f);
    CHECK (scaled.getWidth() == page->getWidth() * 2);
    CHECK (scaled.getHeight() == page->getHeight() * 2);
    // The channels' list: the page grows, the frame with it (and the page keeps its own size).
    const auto before = page->getHeight();
    page->setChannelsMode (true);
    CHECK (page->getHeight() > before);
    CHECK (scaled.getHeight() == page->getHeight() * 2);
    CHECK (page->getWidth() * 2 == scaled.getWidth());
    // Never more than twice, never less than once.
    CHECK (ScaledPage::scaleFor (scaled, 620, 610) <= 2.0f);
    CHECK (ScaledPage::scaleFor (scaled, 620, 610) >= 1.0f);

    // The right-hand screen: one of the screens there are, the furthest right.
    const auto area = Windows::rightmostScreenArea();
    CHECK_FALSE (area.isEmpty());
    for (const auto& d : juce::Desktop::getInstance().getDisplays().displays)
        CHECK (d.userArea.getCentreX() <= area.getCentreX());
}

#include "engine/ParagonSynth.h"

TEST_CASE ("0.52.2: a plug-in's window opens where it was left; off every screen now: the right-hand screen", "[ui][pluginwindow]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-window-places.settings");
    file.deleteFile();
    juce::PropertiesFile::Options options;
    juce::PropertiesFile settings (file, options);
    ParagonSynth synth;
    const auto key = Windows::Instrument::placeKey (synth.getName());

    // The first time: centred on the right-hand screen.
    juce::Point<int> first;
    {
        Windows::Instrument window ("Lead", synth, &settings);
        CHECK (Windows::rightmostScreenArea().contains (window.getBounds().getCentre()));
        first = window.getPosition();
        // Moved by the owner: kept.
        window.setTopLeftPosition (first.x - 30, first.y + 20);
        CHECK (settings.getValue (key) == juce::String (first.x - 30) + " " + juce::String (first.y + 20));
    }
    // Opened again (on any track): where it was left.
    {
        Windows::Instrument window ("Another track", synth, &settings);
        INFO ("kept: " << settings.getValue (key) << ", now at " << window.getX() << " " << window.getY() << ", first " << first.x << " " << first.y);
        CHECK (window.getPosition() == juce::Point<int> (first.x - 30, first.y + 20));
    }
    // A place no screen shows any more (a monitor unplugged): the right-hand screen again.
    settings.setValue (key, "-50000 -50000");
    {
        Windows::Instrument window ("Lead", synth, &settings);
        CHECK (Windows::rightmostScreenArea().contains (window.getBounds().getCentre()));
    }
    // Without settings: always the right-hand screen.
    {
        Windows::Instrument window ("Lead", synth);
        CHECK (Windows::rightmostScreenArea().contains (window.getBounds().getCentre()));
    }
    file.deleteFile();
}

#include "ui/PluginsPage.h"

TEST_CASE ("0.52.2: the PLUGINS page lists what was found, in the Hub's look", "[ui][pluginwindow]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    auto& known = studio.getPluginHost().getKnownPlugins();
    known.clear();
    juce::PluginDescription d;
    d.name = "Kontakt 8";
    d.pluginFormatName = "VST3";
    d.category = "Instrument|Sampler";
    d.manufacturerName = "Native Instruments";
    d.version = "8.9.0";
    d.fileOrIdentifier = "C:/VST3/Kontakt 8.vst3";
    known.addType (d);
    PluginsPage page (studio);
    CHECK (page.getList().getTableListBox().getNumRows() == 1);
    CHECK (PluginsPage::cellText (known, 0, 1) == "Kontakt 8");
    CHECK (PluginsPage::cellText (known, 0, 3) == "INSTRUMENT  -  SAMPLER");
    CHECK (PluginsPage::cellText (known, 0, 5) == "V8.9.0");
    CHECK (page.createComponentSnapshot (page.getLocalBounds()).isValid());
    known.clear();
}
