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

    // A stretch of a track (the cycle, a range): the parts rendered there muted, the track itself left playing.
    Studio other (false);
    const auto keys = other.getProject().tracks[0].id;
    other.addPart (keys, { 0.0, 8.0, "Long", { { 0.0, 1.0, 60, 1.0f }, { 5.0, 1.0, 64, 1.0f } } });
    const auto section = other.renderInPlace ({ { keys, 4.0, 8.0, false } }, options, error);
    REQUIRE (section.size() == 1);
    const auto* kept = other.getProject().findTrack (keys);
    CHECK_FALSE (kept->mute);
    CHECK_FALSE (kept->disabled);
    CHECK_FALSE (kept->hidden);
    for (const auto& p : kept->parts)
        CHECK (p.muted == (p.start >= 4.0 - 1.0e-9));
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
    // CYCLE on: its stretch only.
    studio.setCycle (4.0, 12.0);
    studio.setCycleOn (true);
    narrowed = view.narrowedSources (whole);
    REQUIRE (narrowed.size() == 1);
    CHECK_FALSE (narrowed[0].wholeTrack);
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
    CHECK_FALSE (studio.getProject().findTrack (synth)->disabled);   // (a stretch: the track plays on)
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
