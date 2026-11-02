#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

// The MediaBay (0.21.0, the owner asked: as the manual's MediaBay window). The studio's media
// files (audio, MIDI files, track presets, songs) in the folders you choose to scan, with their
// attributes and your star ratings, searched and filtered. Our design where the manual leaves
// room: the locations are whole folders (with their subfolders), not a tree of ticked subfolders;
// tempo and key come from the file's name ("Drums_120bpm_Am.wav") or, for MIDI files, the file.
namespace pr7s
{
    enum class MediaType { audio, midi, trackPreset, song };
    juce::String mediaTypeName (MediaType);                       // "AUDIO", "MIDI", "TRACK PRESET", "SONG"
    std::optional<MediaType> mediaTypeOf (const juce::File&);   // by extension; none: not a media file

    struct MediaItem
    {
        juce::File file;
        MediaType type = MediaType::audio;
        juce::String name;            // the file's name without its extension
        juce::int64 size = 0;
        juce::Time modified;
        double seconds = 0.0;         // audio: its length
        double sampleRate = 0.0;
        int channels = 0;
        double bpm = 0.0;             // 0: not known
        juce::String key;             // "Am", "F#", "": not known
        double beats = 0.0;           // MIDI files: to the last note's end
        int notes = 0;                // MIDI files
        int rating = 0;               // 0 to 5 stars (yours)
        juce::String searchText() const;   // what a text search looks in (lower case)
    };

    // Text search (the manual's Boolean Text Search): words must all be found (and, +, or nothing
    // between them); "or" or a comma for either; "not" or a leading minus for none; parentheses;
    // quotation marks for a phrase. Matching ignores case and finds words inside words ("drum" in
    // "Drumloop"). Our choice: a minus inside a word (hi-hat) is part of the word.
    class TextSearch
    {
    public:
        explicit TextSearch (const juce::String& query);
        ~TextSearch();
        bool matches (const juce::String& lowerCaseText) const;
        bool isEmpty() const noexcept { return root == nullptr; }
        struct Node;

    private:
        std::unique_ptr<Node> root;
    };

    struct MediaQuery
    {
        juce::String text;
        std::set<MediaType> types;     // empty: all
        int minimumRating = 0;
        juce::File folder;             // none: every location
        bool subfolders = true;        // the folder's subfolders too (the manual's Include Folders and Subfolders)
    };

    enum class MediaColumn { rating, name, type, length, tempo, key, folder, size, date };

    class MediaDatabase
    {
    public:
        // `store`: where it's kept (none: in memory only, the tests). Locations, attributes and
        // ratings are read from it now and written back after each change.
        explicit MediaDatabase (juce::File store = {});
        ~MediaDatabase();

        // The folders scanned (each with its subfolders).
        const juce::StringArray& getLocations() const noexcept { return locations; }
        bool addLocation (const juce::File&);        // false if it isn't a folder or is in already
        void removeLocation (const juce::File&);     // its files leave the results (their ratings are kept)
        bool hadStore() const noexcept { return storeExisted; }

        // Scanning: in the background (`rescan`), or at once (`scanNow`, the tests). A file read
        // before and unchanged (size and date) isn't read again. `shouldPause`: while it says so,
        // the scan waits (the studio's: while playing or recording, as the manual's MediaBay).
        void rescan();
        void scanNow();
        bool isScanning() const noexcept { return scanning.load(); }
        int getFilesSeen() const noexcept { return filesSeen.load(); }
        std::function<bool()> shouldPause;
        bool takeScanResults();                      // (message thread) a finished scan's items in; true if any

        const std::vector<MediaItem>& getItems() const noexcept { return items; }
        std::vector<int> find (const MediaQuery&) const;   // indices into getItems, in the items' order
        void sort (std::vector<int>& indices, MediaColumn, bool ascending) const;
        static void shuffle (std::vector<int>& indices, juce::uint32 seed);

        void setRating (const juce::File&, int stars);     // 0-5; kept even when the file isn't scanned
        int getRating (const juce::File&) const;

        // A file's attributes, read now (any thread).
        static MediaItem read (const juce::File&, juce::AudioFormatManager&);
        static double bpmFromName (const juce::String&);          // "Beat 120bpm", "loop_95_bpm": 0 if none
        static juce::String keyFromName (const juce::String&);    // "Bass_Am_120": "Am"; "" if none

        void save() const;

    private:
        class Scanner;
        void load();
        std::vector<MediaItem> scanAll (std::function<bool()> stop);
        juce::File store;
        bool storeExisted = false;
        juce::StringArray locations;
        std::vector<MediaItem> items;
        std::map<juce::String, int> ratings;   // full path: stars
        std::unique_ptr<Scanner> scanner;
        std::atomic<bool> scanning { false };
        std::atomic<int> filesSeen { 0 };
        juce::CriticalSection pendingLock;
        std::optional<std::vector<MediaItem>> pending;
    };
}
