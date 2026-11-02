#include "ProjectLibrary.h"

#include "Project.h"

namespace pr7s
{
    ProjectLibrary::ProjectLibrary (juce::PropertiesFile* s) : settings (s) {}

    juce::String ProjectLibrary::get (const juce::String& key, const juce::String& fallback) const
    {
        if (settings != nullptr)
            return settings->getValue (key, fallback);
        const auto it = values.find (key);
        return it != values.end() ? it->second : fallback;
    }

    void ProjectLibrary::set (const juce::String& key, const juce::String& value)
    {
        if (settings != nullptr)
        {
            settings->setValue (key, value);
            settings->saveIfNeeded();
        }
        else
            values[key] = value;
    }

    //==========================================================================
    juce::Array<juce::File> ProjectLibrary::getRecent() const
    {
        const auto list = settings != nullptr ? juce::StringArray::fromLines (settings->getValue ("hubRecent")) : memory;
        juce::Array<juce::File> files;
        for (const auto& path : list)
            if (path.isNotEmpty() && juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile())
                files.add (juce::File (path));
        return files;
    }

    void ProjectLibrary::addRecent (const juce::File& file)
    {
        if (file == juce::File())
            return;
        auto list = settings != nullptr ? juce::StringArray::fromLines (settings->getValue ("hubRecent")) : memory;
        list.removeString (file.getFullPathName());
        list.removeEmptyStrings();
        list.insert (0, file.getFullPathName());
        while (list.size() > maxRecent)
            list.remove (list.size() - 1);
        if (settings != nullptr)
            set ("hubRecent", list.joinIntoString ("\n"));
        else
            memory = list;
    }

    void ProjectLibrary::removeRecent (const juce::File& file)
    {
        auto list = settings != nullptr ? juce::StringArray::fromLines (settings->getValue ("hubRecent")) : memory;
        list.removeString (file.getFullPathName());
        if (settings != nullptr)
            set ("hubRecent", list.joinIntoString ("\n"));
        else
            memory = list;
    }

    void ProjectLibrary::clearRecent()
    {
        if (settings != nullptr)
            set ("hubRecent", {});
        memory.clear();
    }

    //==========================================================================
    bool ProjectLibrary::usesDefaultLocation() const { return get ("hubUseDefaultLocation", "1") == "1"; }
    void ProjectLibrary::setUseDefaultLocation (bool on) { set ("hubUseDefaultLocation", on ? "1" : "0"); }

    juce::File ProjectLibrary::getDefaultLocation() const
    {
        const auto fallback = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Paragon PR7-S").getChildFile ("Projects");
        const auto path = get ("hubDefaultLocation", {});
        return path.isNotEmpty() && juce::File::isAbsolutePath (path) ? juce::File (path) : fallback;
    }

    void ProjectLibrary::setDefaultLocation (const juce::File& folder) { set ("hubDefaultLocation", folder.getFullPathName()); }

    juce::String ProjectLibrary::getFolderName() const
    {
        const auto name = get ("hubFolderName", "Untitled").trim();
        return name.isNotEmpty() ? name : juce::String ("Untitled");
    }

    void ProjectLibrary::setFolderName (const juce::String& name) { set ("hubFolderName", juce::File::createLegalFileName (name.trim())); }

    juce::File ProjectLibrary::newSongFile (const juce::String& wanted) const
    {
        auto name = juce::File::createLegalFileName (wanted.trim());
        if (name.isEmpty())
            name = getFolderName();
        const auto location = getDefaultLocation();
        auto folder = location.getChildFile (name);
        for (int i = 2; folder.exists(); ++i)
            folder = location.getChildFile (name + " " + juce::String (i));
        return folder.getChildFile (folder.getFileName() + Project::fileExtension);
    }

    //==========================================================================
    juce::Array<juce::File> ProjectLibrary::getUserLocations() const
    {
        juce::Array<juce::File> folders;
        for (const auto& path : juce::StringArray::fromLines (get ("hubUserLocations", {})))
            if (path.isNotEmpty() && juce::File::isAbsolutePath (path))
                folders.add (juce::File (path));
        return folders;
    }

    void ProjectLibrary::addUserLocation (const juce::File& folder)
    {
        auto list = juce::StringArray::fromLines (get ("hubUserLocations", {}));
        list.removeEmptyStrings();
        list.addIfNotAlreadyThere (folder.getFullPathName());
        set ("hubUserLocations", list.joinIntoString ("\n"));
    }

    void ProjectLibrary::removeUserLocation (const juce::File& folder)
    {
        auto list = juce::StringArray::fromLines (get ("hubUserLocations", {}));
        list.removeString (folder.getFullPathName());
        set ("hubUserLocations", list.joinIntoString ("\n"));
    }

    juce::Array<juce::File> ProjectLibrary::findSongs (const juce::File& folder)
    {
        juce::Array<juce::File> songs;
        if (! folder.isDirectory())
            return songs;
        const auto pattern = juce::String ("*") + Project::fileExtension;
        songs.addArray (folder.findChildFiles (juce::File::findFiles, false, pattern));
        for (const auto& sub : folder.findChildFiles (juce::File::findDirectories, false))
            songs.addArray (sub.findChildFiles (juce::File::findFiles, false, pattern));
        // Newest first.
        std::sort (songs.begin(), songs.end(), [] (const juce::File& a, const juce::File& b)
                   { return a.getLastModificationTime() > b.getLastModificationTime(); });
        return songs;
    }

    //==========================================================================
    juce::File ProjectLibrary::templateFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Paragon PR7-S").getChildFile ("Templates");
    }

    juce::Array<juce::File> ProjectLibrary::getTemplates() const
    {
        auto files = templateFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + Project::fileExtension);
        files.sort();
        return files;
    }

    //==========================================================================
    bool ProjectLibrary::showsPaths() const { return get ("hubShowPaths", "0") == "1"; }
    void ProjectLibrary::setShowsPaths (bool on) { set ("hubShowPaths", on ? "1" : "0"); }
    bool ProjectLibrary::showsLocation() const { return get ("hubShowLocation", "1") == "1"; }
    void ProjectLibrary::setShowsLocation (bool on) { set ("hubShowLocation", on ? "1" : "0"); }
    bool ProjectLibrary::opensAtStart() const { return get ("hubAtStart", "1") == "1"; }
    void ProjectLibrary::setOpensAtStart (bool on) { set ("hubAtStart", on ? "1" : "0"); }
}
