#include <catch2/catch_test_macros.hpp>

#include "engine/Studio.h"
#include "ui/MainView.h"
#include "ui/Finish.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/ComposerPage.h"

// (0.113.0) The owner's reports: a click on a track in the track list stuck the screen about a second;
// the studio hung closing or opening a song. Timings (hidden: "[.perf-click]").

using namespace pr7s;

namespace
{
    double msOf (const std::function<void()>& work)
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        work();
        return juce::Time::getMillisecondCounterHiRes() - start;
    }

    struct BigLibrary
    {
        juce::TemporaryFile temp;
        juce::File presets, packs;

        explicit BigLibrary (int count)
        {
            const auto root = temp.getFile();
            root.createDirectory();
            presets = root.getChildFile ("Rigs");
            presets.createDirectory();
            packs = root.getChildFile ("Packs");
            juce::XmlElement settings ("Grooves");
            auto* f = settings.createNewChildElement ("Folder");
            f->setAttribute ("path", packs.getFullPathName());
            f->setAttribute ("map", 1);
            settings.writeTo (presets.getChildFile ("Grooves.xml"));
            const char* sections[] { "Intro", "Verse", "Chorus", "Bridge", "Breakdown", "Outro", "Fill", "Groove" };
            const char* genres[] { "Metal", "Rock", "Prog", "Djent", "Funk" };
            juce::XmlElement index ("GrooveIndex");
            for (int i = 0; i < count; ++i)
            {
                auto* g = index.createNewChildElement ("G");
                const auto path = packs.getChildFile (juce::String ("Pack ") + juce::String (i / 500) + "/" + genres[i % 5] + "/"
                                                      + sections[(i / 7) % 8] + "/" + sections[(i / 7) % 8] + " " + juce::String (i) + ".mid");
                g->setAttribute ("path", path.getFullPathName());
                g->setAttribute ("stamp", "1|2|1");
                g->setAttribute ("bpm", 90 + i % 90);
                g->setAttribute ("bpmKnown", 1);
                g->setAttribute ("num", 4);
                g->setAttribute ("den", 4);
                g->setAttribute ("bars", 1 + i % 4);
                g->setAttribute ("tags", juce::String ("Genre:") + genres[i % 5] + "|Type:" + ((i % 9) == 0 ? "Fill" : "Beat"));
            }
            index.writeTo (presets.getChildFile ("Groove Index.xml"));
        }
    };

    std::vector<int> bigSong (Studio& studio, int tracks = 30)
    {
        std::vector<int> ids;
        for (int t = 0; t < tracks; ++t)
        {
            const auto id = studio.addTrack (Instrument {});
            Part part;
            part.length = 128.0;
            for (int n = 0; n < 2000; ++n)
                part.notes.push_back ({ n * 0.0625, 0.05, 40 + (n * 7 + t) % 40, 0.8f });
            studio.addPart (id, part);
            ids.push_back (id);
        }
        for (const auto* name : { "Intro", "Verse", "Chorus", "Verse", "Chorus", "Bridge", "Chorus", "Outro" })
            studio.appendSongSection (name, 8);
        return ids;
    }
}

TEST_CASE ("Speed 0.113.0: a track clicked, a song closed and opened (the owner's lag)", "[.][perf-click]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        Studio studio (false);
        MainView view (studio);
        view.setSize (5760, 1000);
        const auto ids = bigSong (studio);
        studio.dispatchPendingMessages();
        view.createComponentSnapshot (view.getLocalBounds(), true, 1.0f);

        BigLibrary library (100000);
        studio.setPr7dFolderForTests (library.presets);
        for (int pass = 0; pass < 2; ++pass)
        {
        if (pass == 1)
        {
            view.openComposer();
            studio.dispatchPendingMessages();
            if (auto* w = view.getComposerWindow())
                w->getPage().runTimerForTests();
            WARN ("the composer open");
        }
        for (int i = 0; i < 4; ++i)
        {
            const auto select = msOf ([&] { studio.selectTrack (ids[(size_t) (i * 7 % 30)]); });
            const auto messages = msOf ([&] { studio.dispatchPendingMessages(); });
            const auto paint = msOf ([&] { view.createComponentSnapshot (view.getLocalBounds(), true, 1.0f); });
            const auto timer = msOf ([&] { if (auto* w = view.getComposerWindow()) w->getPage().runTimerForTests(); });
            WARN ("click " << i << ": select " << select << " ms, messages " << messages << " ms, paint " << paint << " ms, composer timer " << timer << " ms");
        }
        }

        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-perf-click").getChildFile ("song.pr7s");
        file.getParentDirectory().createDirectory();
        juce::String error;
        const auto save = msOf ([&] { studio.save (file); });
        const auto close = msOf ([&] { studio.newProject(); studio.dispatchPendingMessages(); });
        const auto open = msOf ([&] { studio.open (file, error); studio.dispatchPendingMessages(); });
        const auto paint = msOf ([&] { view.createComponentSnapshot (view.getLocalBounds(), true, 1.0f); });
        WARN ("save " << save << " ms, close (new) " << close << " ms, open " << open << " ms, paint after " << paint << " ms");
        file.getParentDirectory().deleteRecursively();
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

TEST_CASE ("0.113.0: closing a song unloads its plug-ins as steps of the progress box; closing to the Hub loads nothing", "[close]")
{
    Studio studio (false);
    for (int t = 0; t < 3; ++t)
        studio.addTrack (Instrument {});
    juce::StringArray steps;
    auto ended = 0;
    studio.onProgress = [&] (double progress, const juce::String& what)
    {
        if (progress < 0.0)
            ++ended;
        else
            steps.add (juce::String (progress, 2) + " " + what);
    };
    studio.newProject (false);
    studio.onProgress = nullptr;
    INFO (steps.joinIntoString ("\n"));
    CHECK (steps.size() == 4);   // (the starter track and the three)
    CHECK (steps[0].startsWith ("0.00 Closing the song: unloading"));
    CHECK (steps[3].contains ("(4 of 4)"));
    CHECK (ended == 1);
    CHECK (studio.getProject().tracks.empty());
    CHECK_FALSE (studio.isModified());
    // A new song as before: its one track.
    studio.newProject();
    CHECK (studio.getProject().tracks.size() == 1);
}
