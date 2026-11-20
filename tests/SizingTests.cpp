#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "engine/Sizing.h"
#include "engine/Stretch.h"
#include "engine/Warp.h"
#include "ui/Finish.h"
#include "ui/MainView.h"

// (0.187.0) The Object Selection tool's three sizings (Normal Sizing, Sizing Moves Contents, Sizing
// Applies Time Stretch): the 1 key cycling them, what each does to MIDI parts and audio clips, several
// parts at once, one undo step; a stretched clip's sound (its length, its pitch by algorithm); saved.

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    constexpr double rate = 48000.0;

    juce::MouseEvent mouseAt (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt, juce::ModifierKeys mods = {})
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                                 mods.withFlags (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, now, downAt, now, 1, at != downAt);
    }

    juce::AudioBuffer<float> tone (double hz, double seconds)
    {
        juce::AudioBuffer<float> sound (2, (int) (seconds * rate));
        for (int i = 0; i < sound.getNumSamples(); ++i)
            for (int c = 0; c < 2; ++c)
                sound.setSample (c, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / rate));
        return sound;
    }

    void writeWav (const juce::File& file, const juce::AudioBuffer<float>& sound)
    {
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2)
                                                                           .withBitsPerSample (32)
                                                                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (sound, 0, sound.getNumSamples());
    }

    double frequencyOf (const juce::AudioBuffer<float>& sound, double from, double to)
    {
        auto crossings = 0;
        for (int i = (int) (from * rate) + 1; i < (int) (to * rate) && i < sound.getNumSamples(); ++i)
            if (sound.getSample (0, i - 1) < 0.0f && sound.getSample (0, i) >= 0.0f)
                ++crossings;
        return crossings / (to - from);
    }

    Part midiPart (double start, double length)
    {
        Part part;
        part.start = start;
        part.length = length;
        part.name = "riff";
        part.notes.push_back ({ 1.0, 0.5, 60, 0.8f });
        part.notes.push_back ({ 2.0, 1.0, 64, 0.8f });
        Controller cc;
        cc.start = 3.0;
        cc.value = 90;
        part.controllers.push_back (cc);
        return part;
    }
}

TEST_CASE ("SIZING 0.187.0: 1 pressed again cycles SELECT's sizings; the key shows the one in use", "[sizing]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    auto& arrange = view.getArrangeView();
    arrange.setTool (ArrangeView::Tool::select);
    REQUIRE (arrange.getSelectSizing() == Sizing::normal);
    const auto press = [&] (char c) { return view.keyPressed (juce::KeyPress (c, {}, (juce::juce_wchar) c)); };
    CHECK (press ('1'));
    CHECK (arrange.getSelectSizing() == Sizing::movesContents);
    CHECK (press ('1'));
    CHECK (arrange.getSelectSizing() == Sizing::timeStretch);
    auto& key = static_cast<ToolKey&> (view.getTools().getButton (ArrangeView::Tool::select));
    CHECK (key.sizing == Sizing::timeStretch);
    CHECK (key.getTooltip().contains ("TIME STRETCH"));
    CHECK (press ('1'));
    CHECK (arrange.getSelectSizing() == Sizing::normal);

    // Another tool, then 1: back to SELECT in the sizing it had (not the next one).
    arrange.setSelectSizing (Sizing::timeStretch);
    CHECK (press ('3'));
    CHECK (arrange.getTool() == ArrangeView::Tool::split);
    CHECK (press ('1'));
    CHECK (arrange.getTool() == ArrangeView::Tool::select);
    CHECK (arrange.getSelectSizing() == Sizing::timeStretch);

    // The menu's choice (SELECT clicked again opens it: the three, the one in use ticked).
    arrange.setSelectSizing (Sizing::movesContents);
    CHECK (key.sizing == Sizing::movesContents);
    view.getTools().getButton (ArrangeView::Tool::select).onClick();   // (opens the menu: closed again at once)
    juce::PopupMenu::dismissAllActiveMenus();
    CHECK (arrange.getSelectSizing() == Sizing::movesContents);
    arrange.setSelectSizing (Sizing::normal);

    // Its key and pointer drawn in each sizing (nothing more to check than that they draw).
    for (const auto sizing : { Sizing::normal, Sizing::movesContents, Sizing::timeStretch })
    {
        arrange.setSelectSizing (sizing);
        CHECK (key.createComponentSnapshot (key.getLocalBounds()).isValid());
        CHECK (! juce::String (sizingName (sizing)).isEmpty());
    }
    CHECK (juce::String (sizingName (Sizing::timeStretch)) == "SIZING APPLIES TIME STRETCH");
    arrange.setSelectSizing (Sizing::normal);
}

TEST_CASE ("SIZING 0.187.0: Normal Sizing unchanged; Sizing Moves Contents carries a part's content with its left edge", "[sizing]")
{
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addPart (trackId, midiPart (4.0, 4.0));
    const auto part = [&]() -> const Part& { return studio.getProject().findTrack (trackId)->parts.at (0); };
    const auto id = part().id;

    // Normal: the arrangement trims as it always did (sizeParts leaves it to it).
    CHECK_FALSE (studio.sizeParts ({ id }, id, 2.0, 6.0, Sizing::normal));
    CHECK (part().start == 4.0);

    // Moves Contents, left edge to beat 2: the notes keep their places from the part's start (they move with it).
    REQUIRE (studio.sizeParts ({ id }, id, 2.0, 6.0, Sizing::movesContents));
    CHECK_THAT (part().start, WithinAbs (2.0, 1.0e-9));
    CHECK_THAT (part().length, WithinAbs (6.0, 1.0e-9));
    CHECK_THAT (part().end(), WithinAbs (8.0, 1.0e-9));
    CHECK_THAT (part().notes[0].start, WithinAbs (1.0, 1.0e-9));   // (song beat 3 now: it was 5)
    CHECK_THAT (part().controllers[0].start, WithinAbs (3.0, 1.0e-9));
    studio.undo();
    CHECK (part().start == 4.0);

    // The right edge with Moves Contents: as Normal Sizing (the content stays with the start).
    REQUIRE (studio.sizeParts ({ id }, id, 4.0, 6.0, Sizing::movesContents));
    CHECK_THAT (part().length, WithinAbs (6.0, 1.0e-9));
    CHECK_THAT (part().notes[1].start, WithinAbs (2.0, 1.0e-9));
    studio.undo();

    // An audio clip: its offset kept, so its sound starts at its new start.
    const auto audioId = studio.addAudioTrack (true);
    writeWav (studio.getAudioFolder().getChildFile ("tone.wav"), tone (440.0, 6.0));
    Part clip;
    clip.start = 4.0;
    clip.length = 4.0;
    clip.audioFile = "tone.wav";
    clip.audioOffset = 0.5;
    studio.addPart (audioId, clip);
    const auto clipId = studio.getProject().findTrack (audioId)->parts.at (0).id;
    REQUIRE (studio.sizeParts ({ clipId }, clipId, 3.0, 5.0, Sizing::movesContents));
    const auto& moved = studio.getProject().findTrack (audioId)->parts.at (0);
    CHECK_THAT (moved.start, WithinAbs (3.0, 1.0e-9));
    CHECK_THAT (moved.length, WithinAbs (5.0, 1.0e-9));
    CHECK_THAT (moved.audioOffset, WithinAbs (0.5, 1.0e-9));
}

TEST_CASE ("SIZING 0.187.0: on screen, a left edge dragged in each sizing", "[sizing]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addPart (trackId, midiPart (4.0, 4.0));
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto& timeline = arrange.getTimeline();
    timeline.pixelsPerBeat = 60.0;
    timeline.firstBeat = 0.0;
    const auto part = [&]() -> const Part& { return studio.getProject().findTrack (trackId)->parts.at (0); };
    const auto drag = [&] (float fromX, float toX)
    {
        const auto y = arrange.getPartBounds (trackId, 0).getCentreY();
        const juce::Point<float> from (fromX, y), to (toX, y);
        arrange.mouseDown (mouseAt (arrange, from, from));
        for (int step = 1; step <= 4; ++step)
            arrange.mouseDrag (mouseAt (arrange, from + (to - from) * ((float) step / 4.0f), from));
        arrange.mouseUp (mouseAt (arrange, to, from));
    };
    arrange.setTool (ArrangeView::Tool::select);
    const auto leftEdge = [&] { return arrange.getPartBounds (trackId, 0).getX() + 2.0f; };
    const auto rightEdge = [&] { return arrange.getPartBounds (trackId, 0).getRight() - 2.0f; };

    // Normal: the left edge to beat 2: the notes stay where they are in the song.
    arrange.setSelectSizing (Sizing::normal);
    drag (leftEdge(), leftEdge() - 120.0f);
    CHECK_THAT (part().start, WithinAbs (2.0, 1.0e-9));
    CHECK_THAT (part().notes[0].start, WithinAbs (3.0, 1.0e-9));
    studio.undo();
    REQUIRE (part().start == 4.0);

    // Moves Contents: the notes go with the edge.
    arrange.setSelectSizing (Sizing::movesContents);
    drag (leftEdge(), leftEdge() - 120.0f);
    CHECK_THAT (part().start, WithinAbs (2.0, 1.0e-9));
    CHECK_THAT (part().notes[0].start, WithinAbs (1.0, 1.0e-9));
    studio.undo();
    REQUIRE (part().start == 4.0);

    // Time Stretch, the right edge to beat 12: twice as long, every note twice as far in and as long.
    arrange.setSelectSizing (Sizing::timeStretch);
    const auto stepsBefore = studio.getHistoryPosition();
    drag (rightEdge(), rightEdge() + 240.0f);
    CHECK_THAT (part().length, WithinAbs (8.0, 1.0e-9));
    CHECK_THAT (part().notes[1].start, WithinAbs (4.0, 1.0e-9));
    CHECK_THAT (part().notes[1].length, WithinAbs (2.0, 1.0e-9));
    CHECK_THAT (part().controllers[0].start, WithinAbs (6.0, 1.0e-9));
    CHECK (studio.getHistoryPosition() == stepsBefore + 1);   // (one undo step for the drag)
    arrange.setSelectSizing (Sizing::normal);
}

TEST_CASE ("SIZING 0.187.0: Sizing Applies Time Stretch scales MIDI exactly, several parts by the same amount, one undo step", "[sizing]")
{
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addPart (trackId, midiPart (0.0, 4.0));
    studio.addPart (trackId, midiPart (8.0, 6.0));
    const auto& parts = [&]() -> const PartList& { return studio.getProject().findTrack (trackId)->parts; };
    const auto a = parts()[0].id, b = parts()[1].id;

    // The right edge of the first to beat 6 (x 1.5): the second x 1.5 too, from its start.
    REQUIRE (studio.sizeParts ({ a, b }, a, 0.0, 6.0, Sizing::timeStretch));
    CHECK_THAT (parts()[0].length, WithinAbs (6.0, 1.0e-9));
    CHECK_THAT (parts()[0].notes[0].start, WithinAbs (1.5, 1.0e-9));
    CHECK_THAT (parts()[0].notes[0].length, WithinAbs (0.75, 1.0e-9));
    CHECK_THAT (parts()[0].controllers[0].start, WithinAbs (4.5, 1.0e-9));
    CHECK_THAT (parts()[1].start, WithinAbs (8.0, 1.0e-9));
    CHECK_THAT (parts()[1].length, WithinAbs (9.0, 1.0e-9));
    CHECK_THAT (parts()[1].notes[1].start, WithinAbs (3.0, 1.0e-9));
    CHECK_THAT (parts()[1].notes[1].length, WithinAbs (1.5, 1.0e-9));

    // One undo step brings both back.
    studio.undo();
    CHECK_THAT (parts()[0].length, WithinAbs (4.0, 1.0e-9));
    CHECK_THAT (parts()[1].length, WithinAbs (6.0, 1.0e-9));
    CHECK_THAT (parts()[1].notes[1].start, WithinAbs (2.0, 1.0e-9));

    // The left edge: the end stays, everything scaled about it.
    REQUIRE (studio.sizeParts ({ b }, b, 11.0, 3.0, Sizing::timeStretch));
    CHECK_THAT (parts()[1].start, WithinAbs (11.0, 1.0e-9));
    CHECK_THAT (parts()[1].end(), WithinAbs (14.0, 1.0e-9));
    CHECK_THAT (parts()[1].notes[0].start, WithinAbs (0.5, 1.0e-9));
    CHECK_THAT (parts()[1].notes[0].length, WithinAbs (0.25, 1.0e-9));
}

TEST_CASE ("SIZING 0.187.0: an audio clip stretched through its warp markers: its length and pitch by algorithm", "[sizing][stretch]")
{
    Studio studio (false);
    const auto trackId = studio.addAudioTrack (true);
    writeWav (studio.getAudioFolder().getChildFile ("tone.wav"), tone (440.0, 2.0));
    Part clip;
    clip.length = 4.0;   // (120 BPM: 2 s, the whole recording)
    clip.audioFile = "tone.wav";
    studio.addPart (trackId, clip);
    const auto part = [&]() -> const Part& { return studio.getProject().findTrack (trackId)->parts.at (0); };
    const auto id = part().id;
    const auto tempo = studio.getProject().tempoMap();

    // The right edge to beat 6: markers map the whole recording (0 to 2 s) to 6 beats (3 s).
    REQUIRE (studio.sizeParts ({ id }, id, 0.0, 6.0, Sizing::timeStretch));
    REQUIRE (part().isWarped());
    REQUIRE (part().warp.size() == 2);
    CHECK_THAT (part().warp.front().beat, WithinAbs (0.0, 1.0e-9));
    CHECK_THAT (part().warp.front().seconds, WithinAbs (0.0, 1.0e-9));
    CHECK_THAT (part().warp.back().beat, WithinAbs (6.0, 1.0e-9));
    CHECK_THAT (part().warp.back().seconds, WithinAbs (2.0, 1.0e-6));
    CHECK (part().stretchMode == StretchModes::clean);
    CHECK_THAT (warpFactors (part(), tempo).back(), WithinAbs (1.5, 1.0e-6));

    // CLEAN: three seconds of sound, the pitch kept.
    auto sound = studio.getWarpedSound (part(), tempo);
    REQUIRE (sound != nullptr);
    CHECK (std::abs (sound->getNumSamples() - (int) (3.0 * rate)) <= 1);
    CHECK_THAT (frequencyOf (*sound, 0.5, 2.5), WithinAbs (440.0, 6.0));

    // TAPE: the pitch follows the speed (x 1.5 slower: 440 / 1.5).
    REQUIRE (studio.setStretchMode ({ id }, StretchModes::tape));
    sound = studio.getWarpedSound (part(), tempo);
    REQUIRE (sound != nullptr);
    CHECK (std::abs (sound->getNumSamples() - (int) (3.0 * rate)) <= 1);
    CHECK_THAT (frequencyOf (*sound, 0.5, 2.5), WithinAbs (440.0 / 1.5, 4.0));

    // VOICE and LIGHT keep the pitch too.
    for (const auto mode : { StretchModes::voice, StretchModes::light })
    {
        REQUIRE (studio.setStretchMode ({ id }, mode));
        sound = studio.getWarpedSound (part(), tempo);
        REQUIRE (sound != nullptr);
        CHECK_THAT (frequencyOf (*sound, 0.5, 2.5), WithinAbs (440.0, 8.0));
    }
    CHECK_FALSE (studio.setStretchMode ({ id }, StretchModes::light));   // (already so: no step)

    // A clip already warped: its markers scaled (squeezed back to 4 beats: its own speed again).
    REQUIRE (studio.sizeParts ({ id }, id, 0.0, 4.0, Sizing::timeStretch));
    CHECK_THAT (part().warp.back().beat, WithinAbs (4.0, 1.0e-9));
    CHECK_THAT (warpFactors (part(), tempo).back(), WithinAbs (1.0, 1.0e-6));

    // A new stretch takes the default algorithm.
    studio.resetWarp ({ id });
    studio.setStretchMode ({ id }, StretchModes::clean);
    studio.setDefaultStretchMode (StretchModes::tape);
    REQUIRE (studio.sizeParts ({ id }, id, 0.0, 2.0, Sizing::timeStretch));
    CHECK (part().stretchMode == StretchModes::tape);
    studio.setDefaultStretchMode (StretchModes::clean);

    // Flatten writes it as it sounds and it plays at its own speed again.
    juce::String error;
    CHECK (studio.flattenWarp ({ id }, error) == 1);
    CHECK_FALSE (part().isWarped());
    CHECK (part().stretchMode == StretchModes::clean);
}

TEST_CASE ("SIZING 0.187.0: the algorithm saved and read back; TAPE resampling and the names", "[sizing][stretch]")
{
    Project project;
    Track track;
    track.id = 1;
    track.kind = Track::Kind::audio;
    Part clip;
    clip.audioFile = "a.wav";
    clip.stretchMode = StretchModes::voice;
    clip.warp = { { 0.0, 0.0 }, { 4.0, 1.0 } };
    track.parts = std::vector<Part> { clip };
    project.tracks.push_back (track);
    const auto xml = project.toXml();
    const auto back = Project::fromXml (*xml);
    REQUIRE (back.has_value());
    const auto& found = back->findTrack (1);
    REQUIRE (found != nullptr);
    REQUIRE (found->parts.size() == 1);
    CHECK (found->parts[0].stretchMode == StretchModes::voice);
    CHECK (found->parts[0].warp.size() == 2);

    // TAPE: a 1 kHz tone read at half speed is a 500 Hz one twice as long.
    const auto input = tone (1000.0, 1.0);
    const auto slow = stretchAudio (input, rate, (int) (2.0 * rate), 0.0, StretchModes::tape);
    CHECK (slow.getNumSamples() == (int) (2.0 * rate));
    CHECK_THAT (frequencyOf (slow, 0.2, 1.8), WithinAbs (500.0, 2.0));

    for (int m = 0; m < StretchModes::count; ++m)
    {
        CHECK (juce::String (StretchModes::name (m)).isNotEmpty());
        CHECK (juce::String (StretchModes::description (m)).startsWith (StretchModes::name (m)));
    }
}
