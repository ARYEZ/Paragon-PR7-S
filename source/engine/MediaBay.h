#pragma once

#include <unordered_map>

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
    // (0.24.0: pluginPreset, a .vstpreset: a VST3 plug-in's settings, the plug-in named inside.)
    enum class MediaType { audio, midi, trackPreset, song, pluginPreset };
    juce::String mediaTypeName (MediaType);                       // "AUDIO", "MIDI", "TRACK PRESET", "SONG", "VST PRESET"
    std::optional<MediaType> mediaTypeOf (const juce::File&);   // by extension; none: not a media file

    // Tags (0.21.1, the manual's Attribute Inspector): what you say about a file, kept in the
    // MediaBay's own store by the file's path. Our choice: never written into your files (the
    // manual writes them unless a file is write-protected; your samples stay as they are).
    enum class MediaAttribute { category, subCategory, style, subStyle, character, tempo, key, rating, comment, user };

    struct AttributeId
    {
        MediaAttribute kind = MediaAttribute::category;
        juce::String user;            // a user attribute's name
        juce::String name() const;    // "CATEGORY", "SUB CATEGORY", ..., the user attribute's name
        bool operator== (const AttributeId& o) const { return kind == o.kind && user == o.user; }
        bool operator!= (const AttributeId& o) const { return ! (*this == o); }
    };

    struct MediaTags
    {
        juce::String category, subCategory, style, subStyle, comment, key;
        double bpm = 0.0;                              // 0: none (the name's or the file's then)
        juce::StringArray character;                   // the sides chosen ("Distorted", "Dark")
        std::map<juce::String, juce::String> user;     // user attribute: value ("1"/"0" for yes/no)
        bool isEmpty() const;
    };

    // The lists offered (our own words; you add your own values to the first two).
    struct AttributeList { juce::String name; juce::StringArray subs; };
    const std::vector<AttributeList>& defaultCategories();
    const std::vector<AttributeList>& defaultStyles();
    struct CharacterPair { juce::String left, right; };
    const std::vector<CharacterPair>& characterPairs();   // opposites: one side or neither
    const juce::StringArray& mediaKeyNames();             // "C" ... "B", "Cm" ... "Bm"

    enum class UserAttributeType { text, number, yesNo };
    struct UserAttribute { juce::String name; UserAttributeType type = UserAttributeType::text; int precision = 0; };

    // The attribute filter: values of one attribute; a file passes when it has any of them
    // (Character: all of them, as the manual's). Filters on different attributes all apply.
    struct AttributeFilter { AttributeId attribute; juce::StringArray values; };

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
        MediaTags tags;               // (yours, 0.21.1)
        // From inside the file (0.22.2): its tags as another program wrote them (the other DAW's
        // MediaBay tags in a WAV's iXML), put into `tags` when the file is first read; its
        // description, genre, comments and keywords (searched).
        MediaTags fileTags;
        juce::String info;
        // A VST preset's (0.24.0): its plug-in's class id (32 hex digits), name, and whether it's an
        // instrument's (from the preset's Info: its category starts "Instrument").
        juce::String presetClass, presetPlugin;
        bool presetInstrument = false;
        int readVersion = currentReadVersion;
        static constexpr int currentReadVersion = 3;   // (older items are read again: 0.22.2 reads more)
        double tempo() const noexcept { return tags.bpm > 0.0 ? tags.bpm : bpm; }   // your tempo, else the name's or file's
        juce::String keyName() const { return tags.key.isNotEmpty() ? tags.key : key; }
        juce::String searchText() const;   // what a text search looks in (lower case; the tags too)
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
        std::vector<AttributeFilter> attributes;   // (empty value lists don't filter)
    };

    enum class MediaColumn { rating, name, type, length, tempo, key, folder, size, date, category, style };

    class MediaDatabase
    {
    public:
        // `store`: where it's kept (none: in memory only, the tests). Locations, attributes and
        // ratings are read from it now and written back after each change.
        explicit MediaDatabase (juce::File store = {});
        ~MediaDatabase();
        // (0.57.1) Ratings and tags mark it to be saved; written when edits stop for a while (the
        // studio's timer calls saveIfDue) or when it closes. Each star click wrote the whole database.
        void saveSoon();
        void saveIfDue (double quietMs = 2000.0);
        bool hasUnsavedEdits() const noexcept { return unsaved; }

        // The folders scanned (each with its subfolders), and folders left out inside them.
        const juce::StringArray& getLocations() const noexcept { return locations; }
        const juce::StringArray& getExclusions() const noexcept { return exclusions; }
        bool addLocation (const juce::File&);        // false if it isn't a folder or is in already
        void removeLocation (const juce::File&);     // its files leave the results (their ratings are kept)

        // The file browser's tick boxes (0.22.1, the manual's Scanning Folders): a folder (and
        // everything in it) scanned or not. A file is scanned when the nearest of the scanned
        // and left-out folders above it is a scanned one. Not scanned any more: its files leave
        // the results at once (ratings and tags kept). Scanned: call rescan() for its files.
        void setIncluded (const juce::File& folder, bool scanned);
        bool isIncluded (const juce::File&) const;
        enum class ScanState { none, some, all };    // empty box, a square, a tick
        ScanState scanState (const juce::File& folder) const;
        static bool includedBy (const juce::File&, const juce::StringArray& locations, const juce::StringArray& exclusions);
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
        void setRatings (const juce::Array<juce::File>&, int stars);   // (saved once)

        // Tags (0.21.1). Each edit applies to all the files given, is kept by path (even when a
        // file isn't scanned), and is saved at once.
        MediaTags getTags (const juce::File&) const;
        void editTags (const juce::Array<juce::File>&, const std::function<void (MediaTags&)>&);
        // A value as text: a category's or style's name, a tempo ("120"), a key, stars ("3"),
        // a comment, a user attribute's value. Character: a side (the other side goes). Empty:
        // as removeAttribute. Category: a sub category not of it goes; Style likewise.
        void setAttribute (const juce::Array<juce::File>&, const AttributeId&, const juce::String& value);
        void removeAttribute (const juce::Array<juce::File>&, const AttributeId&);
        void setCharacter (const juce::Array<juce::File>&, int pair, int side);   // side -1 left, 0 neither, 1 right
        static juce::StringArray attributeValues (const MediaItem&, const AttributeId&);   // as shown; empty: none

        // The lists: ours with yours added (the manual's Adding User Attribute Values).
        std::vector<AttributeList> getCategories() const;
        std::vector<AttributeList> getStyles() const;
        juce::StringArray subsOf (const AttributeId& listAttribute, const juce::String& name) const;   // a category's or style's
        bool addCategory (const juce::String& name, const juce::String& sub);   // false: nothing new
        bool addStyle (const juce::String& name, const juce::String& sub);

        // User attributes (the manual's Defining User Attributes): a name not already used.
        const std::vector<UserAttribute>& getUserAttributes() const noexcept { return userAttributes; }
        bool addUserAttribute (const UserAttribute&);
        void removeUserAttribute (const juce::String& name);   // its values on every file go too
        const UserAttribute* findUserAttribute (const juce::String& name) const;

        // The attribute filter's column: the values found, with how many files have each, among
        // the files the query finds without that column's own filter (so a sub category column
        // shows the chosen categories' subs).
        std::vector<std::pair<juce::String, int>> valueCounts (const MediaQuery&, const AttributeId&) const;

        // A file's attributes, read now (any thread).
        static MediaItem read (const juce::File&, juce::AudioFormatManager&);
        static double bpmFromName (const juce::String&);          // "Beat 120bpm", "loop_95_bpm": 0 if none
        static juce::String keyFromRootNote (int midiNote, int mode);   // mode 1 minor, 0 major, -1 unknown (a plain letter)
        static MediaTags tagsFromIXml (const juce::String& ixml, double& bpm, juce::String& key);   // the other DAW's iXML attributes
        static juce::String readIXml (const juce::File& wav);     // a WAV's iXML chunk ("" if none)
        // A .vstpreset's header and Info (0.24.0): the class id, and the Info's attributes
        // (the VST3 SDK's preset keys: PlugInName, PlugInCategory, MusicalInstrument, MusicalStyle,
        // MusicalCharacter, Name...). False if it isn't one.
        static bool readVstPresetInfo (const juce::File&, juce::String& classId, juce::StringPairArray& info);
        // JUCE's id for a VST3 plug-in from its class id (as PluginDescription::uniqueId); 0 if not 32 hex digits.
        static int vst3UniqueIdFor (const juce::String& classId);
        static juce::String keyFromName (const juce::String&);    // "Bass_Am_120": "Am"; "" if none

        void save() const;
        mutable bool unsaved = false;
        mutable std::unordered_map<juce::String, juce::String> searchCache;   // (0.57.2) by path
        const juce::String& searchTextOf (const MediaItem&) const;
        double lastEdit = 0.0;

    private:
        class Scanner;
        void load();
        std::vector<MediaItem> scanAll (std::function<bool()> stop);
        juce::File store;
        bool storeExisted = false;
        juce::StringArray locations, exclusions;
        std::vector<MediaItem> items;
        std::map<juce::String, int> ratings;   // full path: stars
        std::map<juce::String, MediaTags> tags;   // full path: tags
        std::vector<AttributeList> userCategories, userStyles;
        std::vector<UserAttribute> userAttributes;
        bool passes (const MediaItem&, const MediaQuery&, const TextSearch&) const;
        void dropUnincluded();
        std::unique_ptr<Scanner> scanner;
        std::atomic<bool> scanning { false };
        std::atomic<int> filesSeen { 0 };
        juce::CriticalSection pendingLock;
        std::optional<std::vector<MediaItem>> pending;
    };
}
