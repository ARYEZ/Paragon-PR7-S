#include <catch2/catch_test_macros.hpp>

#include "engine/Studio.h"
#include "ui/DrumLearnPage.h"
#include "ui/LinesPage.h"
#include "DrumSongs.h"

// LEARN A DRUMMER (0.56.0): the page reads a folder, its edits, SAVE PROFILE; IDEAS' DRUMMER key.

using namespace pr7s;

TEST_CASE ("LEARN A DRUMMER: a folder of songs read, sections put right, the profile saved; DRUMMER on the DRUMS tab (0.56.0)", "[drumlearn][ui]")
{
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-learn-ui");
    root.deleteRecursively();
    const auto songs = root.getChildFile ("Songs");
    REQUIRE (drumsongs::writeSong (songs.getChildFile ("One.mid"), drumsongs::songOf ("I4 V8 C8 V8 C8 B8 C8 E4")));
    REQUIRE (drumsongs::writeSong (songs.getChildFile ("Two").getChildFile ("Two.mid"), drumsongs::songOf ("V8 C8 V8 C8 R8 C8 E4"), 140.0));
    songs.getChildFile ("Broken.mid").replaceWithText ("not a MIDI file");

    Studio studio (false);
    studio.setDrummerFolderForTests (root.getChildFile ("Profiles"));
    DrumLearnPage page (studio);
    page.setSize (1500, 860);
    page.setVisible (true);   // (getComponentAt finds only visible components)
    CHECK (page.loadFolder (songs) == 2);
    CHECK (page.getUnread().size() == 1);
    CHECK (page.getName() == "Songs");
    CHECK (page.getStatusText().startsWith ("2 songs"));
    REQUIRE (page.getSongs()[0].sections.size() == 8);
    CHECK (std::abs (page.getSongs()[1].bpm - 140.0) < 0.01);

    // Put right: the breakdown called a bridge, the first verse split, then joined back; a song left out.
    page.setKind (0, 5, DrumSection::bridge);
    CHECK (page.getSongs()[0].sections[5].kind == DrumSection::bridge);
    CHECK (page.split (0, 1, 8));
    CHECK (page.getSongs()[0].sections.size() == 9);
    CHECK (page.join (0, 1));
    CHECK (page.moveStart (0, 2, 13));
    CHECK (page.getSongs()[0].sections[2].firstBar == 13);
    CHECK (page.moveStart (0, 2, 12));
    page.setUsed (0, 0, false);
    CHECK_FALSE (page.getSongs()[0].sections[0].used);

    // The mouse: the tick, and dragging a section's left edge.
    {
        const auto box = page.getUseBox (1);
        const auto at = box.getCentre().toFloat();
        const juce::MouseEvent e (juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys(), juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f,
                                  &page, &page, juce::Time::getCurrentTime(), at, juce::Time::getCurrentTime(), 1, false);
        auto* rows = page.getComponentAt (box.getCentre());
        REQUIRE (rows != nullptr);
        rows->mouseDown (e.getEventRelativeTo (rows));
        CHECK_FALSE (page.getSongs()[1].use);
        rows->mouseDown (e.getEventRelativeTo (rows));
        CHECK (page.getSongs()[1].use);
    }
    {
        const auto edge = page.getSectionBounds (0, 2);
        auto* rows = page.getComponentAt (edge.getCentre());
        REQUIRE (rows != nullptr);
        const auto from = rows->getLocalPoint (&page, juce::Point<int> (edge.getX(), edge.getCentreY())).toFloat();
        const auto barWidth = (float) page.getSectionBounds (0, 2).getWidth() / 8.0f;
        const auto to = from.translated (barWidth * 2.0f, 0.0f);
        const auto now = juce::Time::getCurrentTime();
        const juce::MouseEvent down (juce::Desktop::getInstance().getMainMouseSource(), from, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, rows, rows, now, from, now, 1, false);
        const juce::MouseEvent drag (juce::Desktop::getInstance().getMainMouseSource(), to, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, rows, rows, now, from, now, 1, true);
        rows->mouseDown (down);
        rows->mouseDrag (drag);
        rows->mouseUp (drag);
        CHECK (page.getSongs()[0].sections[2].firstBar == 14);
        page.moveStart (0, 2, 12);
    }

    // Heard: the section through the preview.
    page.hear (0, 1);
    CHECK (page.getHearing() == std::make_pair (0, 1));
    CHECK (studio.isPreviewing());
    page.stop();
    CHECK (page.getHearing() == std::make_pair (-1, -1));

    // Saved: a profile in the folder, without the left-out intro and with a bridge.
    juce::String error;
    page.setName ({});
    CHECK_FALSE (page.save (error));
    page.setName ("Session drummer");
    juce::String saved;
    page.onSaved = [&] (const juce::String& n) { saved = n; };
    REQUIRE (page.save (error));
    CHECK (saved == "Session drummer");
    CHECK (studio.getDrummerNames() == std::vector<juce::String> { "Session drummer" });
    const auto profile = studio.loadDrummer ("Session drummer");
    REQUIRE (profile.has_value());
    CHECK (profile->has (DrumSection::bridge));
    CHECK (profile->of (DrumSection::bridge).sections == 2);   // (the breakdown renamed, and song two's)
    CHECK (profile->of (DrumSection::intro).bars == 0);
    REQUIRE (profile->songs.size() == 2);

    // (0.57.1) Read in the background: the same songs, the status line counting meanwhile.
    {
        DrumLearnPage background (studio);
        background.loadFolderInBackground (songs);
        CHECK (background.getStatusText().startsWith ("Reading the songs") == background.isReading());
        background.waitForReading();
        CHECK_FALSE (background.isReading());
        REQUIRE (background.getSongs().size() == 2);
        CHECK (background.getSongs()[0].sections.size() == 8);
        CHECK (background.getUnread().size() == 1);
        background.loadFolderInBackground (songs);   // (closed while reading: stopped cleanly)
    }

    // Opened again: the sections as saved.
    DrumLearnPage again (studio);
    again.openProfileInBackground (*profile);
    again.waitForReading();
    REQUIRE (again.getSongs().size() == 2);
    CHECK (again.getSongs()[0].sections[5].kind == DrumSection::bridge);
    CHECK_FALSE (again.getSongs()[0].sections[0].used);
    CHECK (again.getName() == "Session drummer");

    // IDEAS: DRUMMER plays the generated column from the profile.
    const Key am { 5, 0, true };
    std::vector<ChordSpan> spans;
    for (int i = 0; i < 8; ++i)
        spans.push_back ({ i * 4.0, i * 4.0 + 4.0, Chord { 9, Chord::Type::minor, Chord::Tension::none, -1 }, am });
    {
        LinesPage ideas (studio, spans);
        ideas.setTab (LinesPage::Tab::drums);
        CHECK (ideas.getDrummer().isEmpty());
        CHECK (ideas.getIdeas (LineIdea::Part::drums)[1].description.startsWith ("Generated: "));
        ideas.setDrummer ("Session drummer");
        CHECK (ideas.getDrummer() == "Session drummer");
        CHECK (studio.getIdeasDrummer() == "Session drummer");
        const auto& beats = ideas.getIdeas (LineIdea::Part::drums);
        REQUIRE (beats.size() == 6);
        CHECK (beats[1].description.startsWith ("Session drummer: "));
        CHECK (beats[1].style == "Verse");
        CHECK_FALSE (beats[1].notes.empty());
        ideas.setDrummer ({});
        CHECK (ideas.getIdeas (LineIdea::Part::drums)[1].description.startsWith ("Generated: "));
        ideas.setDrummer ("Session drummer");
    }
    {
        LinesPage reopened (studio, spans);   // (remembered)
        CHECK (reopened.getDrummer() == "Session drummer");
    }
    CHECK (studio.deleteDrummer ("Session drummer"));
    CHECK (studio.getDrummerNames().empty());
    CHECK (studio.getIdeasDrummer().isEmpty());
    root.deleteRecursively();
}
