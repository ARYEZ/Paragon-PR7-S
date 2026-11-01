#pragma once

#include <juce_data_structures/juce_data_structures.h>

// The songs the Hub shows (0.12.0, the owner asked: a start window as the
// other DAW's Hub, opening on a folder of saved songs). Kept in the settings
// file (in memory only when there's none, as in the tests):
// - the recent songs (newest first, 25 at most), added when a song is opened or saved;
// - the project location (the manual's Use Default Location, Default
//   Location, Project Folder Name): Create Empty makes the song's own folder
//   there, so its recordings live beside it from the start;
// - the user locations (more folders whose songs are listed);
// - templates: songs saved as starting points (Documents/Paragon PR7-S/Templates);
// - the Hub's own settings (paths shown, shown at start, location on the page).
namespace pr7s
{
    class ProjectLibrary
    {
    public:
        explicit ProjectLibrary (juce::PropertiesFile* settings = nullptr);

        // Recent songs.
        juce::Array<juce::File> getRecent() const;   // only those still there
        void addRecent (const juce::File&);
        void removeRecent (const juce::File&);
        void clearRecent();
        static constexpr int maxRecent = 25;

        // Where new songs go.
        bool usesDefaultLocation() const;
        void setUseDefaultLocation (bool);
        juce::File getDefaultLocation() const;          // Documents/Paragon PR7-S/Projects at first
        void setDefaultLocation (const juce::File&);
        juce::String getFolderName() const;             // "Untitled" at first (the manual's)
        void setFolderName (const juce::String&);
        // A new song's file: <location>/<name>/<name>.pr7s, a folder of its own
        // (a number added if it's taken). `name` empty: the folder name.
        juce::File newSongFile (const juce::String& name) const;

        // Other folders whose songs are listed.
        juce::Array<juce::File> getUserLocations() const;
        void addUserLocation (const juce::File&);
        void removeUserLocation (const juce::File&);
        // The songs in a folder and the folders in it (two deep: a song's own folder).
        static juce::Array<juce::File> findSongs (const juce::File& folder);

        // Templates.
        static juce::File templateFolder();
        juce::Array<juce::File> getTemplates() const;

        // The Hub's settings.
        bool showsPaths() const;           void setShowsPaths (bool);
        bool showsLocation() const;        void setShowsLocation (bool);
        bool opensAtStart() const;         void setOpensAtStart (bool);

    private:
        juce::PropertiesFile* settings;
        juce::StringArray memory;                       // the recent list without a settings file
        std::map<juce::String, juce::String> values;    // likewise the rest
        juce::String get (const juce::String& key, const juce::String& fallback) const;
        void set (const juce::String& key, const juce::String& value);
    };
}
