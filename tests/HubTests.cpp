#include <catch2/catch_test_macros.hpp>

#include "engine/ProjectLibrary.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/Hub.h"
#include "ui/MainView.h"

using namespace pr7s;

namespace
{
    juce::File song (const juce::File& folder, const juce::String& name)
    {
        folder.createDirectory();
        auto f = folder.getChildFile (name + Project::fileExtension);
        f.replaceWithText (Project().toXml()->toString());
        return f;
    }
}

TEST_CASE ("The project library: recent songs, the project location, a new song's own folder, the songs in a folder (0.12.0)", "[hub]")
{
    const auto root = juce::File::createTempFile ("pr7s-hub");
    root.createDirectory();
    ProjectLibrary library;

    // Recent: newest first, no twice, only those still there, 25 at most.
    const auto a = song (root, "A"), b = song (root, "B");
    library.addRecent (a);
    library.addRecent (b);
    library.addRecent (a);
    CHECK (library.getRecent() == juce::Array<juce::File> { a, b });
    b.deleteFile();
    CHECK (library.getRecent() == juce::Array<juce::File> { a });
    for (int i = 0; i < 30; ++i)
        library.addRecent (song (root, "Song " + juce::String (i)));
    CHECK (library.getRecent().size() == ProjectLibrary::maxRecent);
    library.removeRecent (root.getChildFile ("Song 29" + juce::String (Project::fileExtension)));
    CHECK (library.getRecent().getFirst().getFileNameWithoutExtension() == "Song 28");
    library.clearRecent();
    CHECK (library.getRecent().isEmpty());

    // The location: a new song in a folder of its own; taken names get a number.
    CHECK (library.usesDefaultLocation());
    CHECK (library.getFolderName() == "Untitled");
    library.setDefaultLocation (root.getChildFile ("Projects"));
    const auto first = library.newSongFile ("My Song");
    CHECK (first == root.getChildFile ("Projects").getChildFile ("My Song").getChildFile (juce::String ("My Song") + Project::fileExtension));
    first.getParentDirectory().createDirectory();
    CHECK (library.newSongFile ("My Song").getParentDirectory().getFileName() == "My Song 2");
    CHECK (library.newSongFile ({}).getParentDirectory().getFileName() == "Untitled");

    // The songs in a folder: there and one folder down (a song's own folder), newest first.
    song (root.getChildFile ("Projects").getChildFile ("Two"), "Two");
    song (root.getChildFile ("Projects"), "Loose");
    song (root.getChildFile ("Projects").getChildFile ("Deep").getChildFile ("Deeper"), "Too deep");
    const auto found = ProjectLibrary::findSongs (root.getChildFile ("Projects"));
    CHECK (found.size() == 2);

    // User locations.
    library.addUserLocation (root);
    library.addUserLocation (root);
    CHECK (library.getUserLocations().size() == 1);
    library.removeUserLocation (root);
    CHECK (library.getUserLocations().isEmpty());
    root.deleteRecursively();
}

TEST_CASE ("The studio and the Hub: Create Empty saves the song in its own folder; songs opened or saved become recent; templates (0.12.0)", "[hub][studio]")
{
    const auto root = juce::File::createTempFile ("pr7s-hub");
    Studio studio (false);
    auto& library = studio.getLibrary();
    library.setDefaultLocation (root);

    juce::String error;
    REQUIRE (studio.createSong ("Big Song", error));
    const auto file = root.getChildFile ("Big Song").getChildFile (juce::String ("Big Song") + Project::fileExtension);
    CHECK (studio.getFile() == file);
    CHECK (file.existsAsFile());
    CHECK_FALSE (studio.isModified());
    CHECK (library.getRecent().getFirst() == file);
    CHECK (studio.getAudioFolder().getParentDirectory() == file.getParentDirectory());   // its recordings beside it

    // Saved somewhere else, opened again: recent.
    const auto other = root.getChildFile ("Other" + juce::String (Project::fileExtension));
    studio.addTrack ({});
    REQUIRE (studio.save (other));
    CHECK (library.getRecent().getFirst() == other);
    juce::String problems;
    REQUIRE (studio.open (file, problems));
    CHECK (library.getRecent().getFirst() == file);

    // A template from this song (the song stays as it was); a new song from it: not saved anywhere yet.
    studio.addTrack ({});
    const auto tracks = studio.getProject().tracks.size();
    REQUIRE (studio.saveAsTemplate ("PR7 test template", error));
    const auto templateFile = ProjectLibrary::templateFolder().getChildFile (juce::String ("PR7 test template") + Project::fileExtension);
    CHECK (templateFile.existsAsFile());
    CHECK (studio.getFile() == file);
    CHECK (studio.isModified());
    CHECK (library.getTemplates().contains (templateFile));
    CHECK_FALSE (library.getRecent().contains (templateFile));
    studio.newProject();
    REQUIRE (studio.newFromTemplate (templateFile, problems));
    CHECK (studio.getProject().tracks.size() == tracks);
    CHECK (studio.getFile() == juce::File());
    templateFile.deleteFile();
    templateFile.getSiblingFile ("PR7 test template Audio").deleteRecursively();
    root.deleteRecursively();
}

TEST_CASE ("The Hub: its sections (recent, templates, the projects folder), the search, folding, opening (0.12.0)", "[hub][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    const auto root = juce::File::createTempFile ("pr7s-hub");
    Studio studio (false);
    auto& library = studio.getLibrary();
    library.setDefaultLocation (root);
    const auto a = song (root.getChildFile ("Forever"), "Forever"), b = song (root.getChildFile ("Signs"), "Signs of life");
    library.addRecent (a);

    Hub hub (studio);
    const auto& items = hub.getItems();
    REQUIRE (! items.empty());
    CHECK (items[0].header);
    CHECK (items[0].section == "RECENT");
    CHECK (items[1].file == a);
    const auto inFolder = std::count_if (items.begin(), items.end(), [&] (const Hub::Item& i) { return ! i.header && i.section.startsWith ("IN "); });
    CHECK (inFolder == 2);

    hub.setSearch ("signs");
    for (const auto& item : items)
        if (! item.header)
            CHECK (item.file.getFullPathName() == b.getFullPathName());
    hub.setSearch ({});

    // Double-click: the song opens (the main window's part); a template: a new song from it.
    juce::File opened;
    hub.onOpen = [&] (const juce::File& f) { opened = f; };
    hub.openItem (1);
    CHECK (opened == a);

    hub.showSettingsPage (true);
    CHECK (hub.isShowingSettings());
    root.deleteRecursively();
}

TEST_CASE ("The Hub from the main window: HUB opens it; a song chosen in it opens and the Hub closes (0.12.0)", "[hub][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    const auto root = juce::File::createTempFile ("pr7s-hub");
    Studio studio (false);
    studio.getLibrary().setDefaultLocation (root);
    const auto a = song (root.getChildFile ("Forever"), "Forever");
    MainView view (studio);
    view.setSize (1500, 900);
    view.openHub();
    REQUIRE (view.getHubWindow() != nullptr);
    auto& hub = dynamic_cast<HubWindow&> (*view.getHubWindow()).getHub();
    REQUIRE (hub.onOpen != nullptr);
    hub.onOpen (a);   // (the song isn't changed: no question)
    CHECK (studio.getFile() == a);
    CHECK (view.getKeyMap().find ("File.Hub") != nullptr);
    root.deleteRecursively();
}
