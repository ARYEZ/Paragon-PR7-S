// (0.112.1, the owner: "so much lag with the composer now it is basically unusable") The composer timed with a
// groove library the size of the owner's (PR7-D's index, 100,000 grooves) and a song laid out in sections.
// Hidden timings ("[.perf-composer]") and checks that the library's size no longer weighs on every action.

#include <catch2/catch_test_macros.hpp>

#include "engine/Composer.h"
#include "engine/DrumIdeas.h"
#include "engine/Pr7dGrooves.h"
#include "engine/Studio.h"
#include "ui/ComposerPage.h"

#include <chrono>
#include <iostream>

using namespace pr7s;

namespace
{
    using L = ComposerLayer;

    // PR7-D's presets folder with `count` grooves listed (their files aren't written: only the index is read
    // until one is played).
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

    template <typename F>
    double timeMs (F&& f, int times = 1)
    {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < times; ++i)
            f();
        return std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - start).count() / times;
    }

    void songWithSections (Studio& studio)
    {
        for (const auto* name : { "Intro", "Verse", "Chorus", "Verse", "Chorus", "Bridge", "Chorus", "Outro" })
            studio.appendSongSection (name, 8);
    }
}

TEST_CASE ("0.112.1: the composer's timings with a big groove library", "[.perf-composer]")
{
    const auto count = juce::jmax (100, juce::SystemStats::getEnvironmentVariable ("PARAGON_PERF_GROOVES", "100000").getIntValue());
    BigLibrary library (count);
    Studio studio (false);
    studio.setPr7dFolderForTests (library.presets);
    songWithSections (studio);
    std::cout << "grooves " << count << "\n";
    std::cout << "first library read " << timeMs ([&] { studio.getComposerContext (0.0); }) << " ms\n";
    std::cout << "getComposerContext " << timeMs ([&] { studio.getComposerContext (0.0); }, 5) << " ms\n";
    std::unique_ptr<ComposerPage> page;
    std::cout << "open the composer " << timeMs ([&] { page = std::make_unique<ComposerPage> (studio); }) << " ms\n";
    page->setRandomSeed (3);
    std::cout << "refresh (nothing changed) " << timeMs ([&] { page->refreshForTests(); }, 5) << " ms\n";
    std::cout << "roll drums " << timeMs ([&] { page->roll (L::drums); }, 3) << " ms\n";
    std::cout << "roll guitar " << timeMs ([&] { page->roll (L::guitar); }, 3) << " ms\n";
    std::cout << "generate " << timeMs ([&] { page->generate(); }, 3) << " ms\n";
    std::cout << "a song edit (rename) then refresh " << timeMs ([&] { studio.renameTrack (studio.getProject().tracks.front().id, "X" + juce::String (juce::Random().nextInt())); page->refreshForTests(); }, 3) << " ms\n";
    std::cout << "drums choices (style menu) " << timeMs ([&] { pr7s::composerChoices (L::drums, page->getState(), page->getSpans(), studio.getComposerContext (0.0)); }) << " ms\n";
    std::cout << "paint " << timeMs ([&] { page->createComponentSnapshot (page->getLocalBounds(), true, 1.0f); }, 3) << " ms\n";
    std::cout << "timer tick " << timeMs ([&] { page->runTimerForTests(); }, 30) << " ms\n";
}
