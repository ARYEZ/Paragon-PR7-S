#include "MediaBay.h"

#include "MidiImport.h"

#include <algorithm>
#include <random>

namespace pr7s
{
    juce::String mediaTypeName (MediaType type)
    {
        switch (type)
        {
            case MediaType::audio: return "AUDIO";
            case MediaType::midi: return "MIDI";
            case MediaType::trackPreset: return "TRACK PRESET";
            case MediaType::song: return "SONG";
        }
        return {};
    }

    std::optional<MediaType> mediaTypeOf (const juce::File& file)
    {
        const auto extension = file.getFileExtension().toLowerCase();
        if (extension == ".wav" || extension == ".aif" || extension == ".aiff" || extension == ".flac" || extension == ".ogg" || extension == ".mp3")
            return MediaType::audio;
        if (extension == ".mid" || extension == ".midi")
            return MediaType::midi;
        if (extension == ".pr7track")
            return MediaType::trackPreset;
        if (extension == ".pr7s")
            return MediaType::song;
        return std::nullopt;
    }

    juce::String MediaItem::searchText() const
    {
        juce::String text;
        text << name << ' ' << file.getFileName() << ' ' << file.getParentDirectory().getFileName() << ' ' << mediaTypeName (type);
        if (type == MediaType::trackPreset)
            text << " preset";
        if (bpm > 0.0)
            text << ' ' << juce::String (bpm, 0) << " bpm";
        if (key.isNotEmpty())
            text << ' ' << key;
        return text.toLowerCase();
    }

    //==========================================================================
    // Text search: a little grammar. expression := term (or term)*; term := factor (and? factor)*;
    // factor := not factor | ( expression ) | word.
    struct TextSearch::Node
    {
        enum class Kind { word, all, any, none } kind = Kind::word;
        juce::String word;
        std::vector<std::unique_ptr<Node>> children;

        bool matches (const juce::String& text) const
        {
            switch (kind)
            {
                case Kind::word: return text.contains (word);
                case Kind::all: for (const auto& c : children) if (! c->matches (text)) return false; return true;
                case Kind::any: for (const auto& c : children) if (c->matches (text)) return true; return false;
                case Kind::none: return children.empty() || ! children.front()->matches (text);
            }
            return true;
        }
    };

    namespace
    {
        struct Token { enum class Kind { word, andOp, orOp, notOp, open, close } kind; juce::String text; };

        std::vector<Token> tokenize (const juce::String& query)
        {
            std::vector<Token> tokens;
            auto p = query.getCharPointer();
            while (! p.isEmpty())
            {
                const auto c = *p;
                if (juce::CharacterFunctions::isWhitespace (c)) { ++p; continue; }
                if (c == '(') { tokens.push_back ({ Token::Kind::open, {} }); ++p; continue; }
                if (c == ')') { tokens.push_back ({ Token::Kind::close, {} }); ++p; continue; }
                if (c == '+') { tokens.push_back ({ Token::Kind::andOp, {} }); ++p; continue; }
                if (c == ',') { tokens.push_back ({ Token::Kind::orOp, {} }); ++p; continue; }
                if (c == '-') { tokens.push_back ({ Token::Kind::notOp, {} }); ++p; continue; }   // (only at a word's start: inside one it's read with the word)
                if (c == '"' || c == 0x201c || c == 0x201d)
                {
                    ++p;
                    juce::String phrase;
                    while (! p.isEmpty() && *p != '"' && *p != 0x201c && *p != 0x201d)
                        phrase += *p++;
                    if (! p.isEmpty())
                        ++p;
                    if (phrase.trim().isNotEmpty())
                        tokens.push_back ({ Token::Kind::word, phrase.trim().toLowerCase() });
                    continue;
                }
                juce::String word;
                while (! p.isEmpty() && ! juce::CharacterFunctions::isWhitespace (*p) && *p != '(' && *p != ')' && *p != ',' && *p != '+' && *p != '"')
                    word += *p++;
                const auto lower = word.toLowerCase();
                if (lower == "and") tokens.push_back ({ Token::Kind::andOp, {} });
                else if (lower == "or") tokens.push_back ({ Token::Kind::orOp, {} });
                else if (lower == "not") tokens.push_back ({ Token::Kind::notOp, {} });
                else tokens.push_back ({ Token::Kind::word, lower });
            }
            return tokens;
        }

        struct Parser
        {
            const std::vector<Token>& tokens;
            size_t at = 0;
            using NodePtr = std::unique_ptr<TextSearch::Node>;
            using Kind = TextSearch::Node::Kind;

            bool is (Token::Kind k) const { return at < tokens.size() && tokens[at].kind == k; }

            NodePtr expression()
            {
                auto first = term();
                if (! is (Token::Kind::orOp))
                    return first;
                auto any = std::make_unique<TextSearch::Node>();
                any->kind = Kind::any;
                if (first) any->children.push_back (std::move (first));
                while (is (Token::Kind::orOp))
                {
                    ++at;
                    if (auto next = term())
                        any->children.push_back (std::move (next));
                }
                return any;
            }

            NodePtr term()
            {
                auto all = std::make_unique<TextSearch::Node>();
                all->kind = Kind::all;
                while (at < tokens.size() && ! is (Token::Kind::orOp) && ! is (Token::Kind::close))
                {
                    if (is (Token::Kind::andOp)) { ++at; continue; }
                    if (auto f = factor())
                        all->children.push_back (std::move (f));
                }
                if (all->children.empty())
                    return nullptr;
                if (all->children.size() == 1)
                    return std::move (all->children.front());
                return all;
            }

            NodePtr factor()
            {
                if (is (Token::Kind::notOp))
                {
                    ++at;
                    auto none = std::make_unique<TextSearch::Node>();
                    none->kind = Kind::none;
                    if (auto inner = factor())
                        none->children.push_back (std::move (inner));
                    return none;
                }
                if (is (Token::Kind::open))
                {
                    ++at;
                    auto inner = expression();
                    if (is (Token::Kind::close))
                        ++at;
                    return inner;
                }
                if (is (Token::Kind::word))
                {
                    auto word = std::make_unique<TextSearch::Node>();
                    word->word = tokens[at++].text;
                    return word;
                }
                ++at;   // (a stray ")": skipped)
                return nullptr;
            }
        };
    }

    TextSearch::TextSearch (const juce::String& query)
    {
        const auto tokens = tokenize (query);
        Parser parser { tokens };
        root = parser.expression();
        while (parser.at < tokens.size())   // (text after an unmatched ")": and-ed on)
        {
            ++parser.at;
            if (auto more = parser.expression())
            {
                auto all = std::make_unique<Node>();
                all->kind = Node::Kind::all;
                if (root) all->children.push_back (std::move (root));
                all->children.push_back (std::move (more));
                root = std::move (all);
            }
        }
    }

    TextSearch::~TextSearch() = default;

    bool TextSearch::matches (const juce::String& text) const
    {
        return root == nullptr || root->matches (text);
    }

    //==========================================================================
    double MediaDatabase::bpmFromName (const juce::String& name)
    {
        // A number of 2 or 3 digits next to "bpm" ("120bpm", "95 BPM", "bpm_128").
        const auto lower = name.toLowerCase();
        for (auto at = lower.indexOf ("bpm"); at >= 0; at = lower.indexOf (at + 1, "bpm"))
        {
            const auto digitsBefore = [&]
            {
                auto end = at;
                while (end > 0 && (lower[end - 1] == ' ' || lower[end - 1] == '_' || lower[end - 1] == '-'))
                    --end;
                auto start = end;
                while (start > 0 && juce::CharacterFunctions::isDigit (lower[start - 1]))
                    --start;
                return lower.substring (start, end);
            }();
            const auto digitsAfter = [&]
            {
                auto start = at + 3;
                while (start < lower.length() && (lower[start] == ' ' || lower[start] == '_' || lower[start] == '-'))
                    ++start;
                auto end = start;
                while (end < lower.length() && juce::CharacterFunctions::isDigit (lower[end]))
                    ++end;
                return lower.substring (start, end);
            }();
            for (const auto& digits : { digitsBefore, digitsAfter })
                if (digits.length() >= 2 && digits.length() <= 3)
                {
                    const auto bpm = digits.getDoubleValue();
                    if (bpm >= 40.0 && bpm <= 300.0)
                        return bpm;
                }
        }
        return 0.0;
    }

    juce::String MediaDatabase::keyFromName (const juce::String& name)
    {
        // A part of the name (between _ - space . or brackets) that is a key: a letter A-G, then #
        // or b, then m / min / minor / maj / major ("Am", "F#m", "Bbmin", "C maj" as "C").
        juce::StringArray parts;
        parts.addTokens (name, " _-.()[]", "");
        parts.removeEmptyStrings();
        for (int i = 0; i < parts.size(); ++i)
        {
            const auto& part = parts[i];
            if (part.isEmpty())
                continue;
            // "Drop A", "Drop C#": a guitar's tuning, not the key (the owner's samples say it).
            if (i > 0 && (parts[i - 1].equalsIgnoreCase ("drop") || parts[i - 1].equalsIgnoreCase ("tuning") || parts[i - 1].equalsIgnoreCase ("standard")))
                continue;
            const auto letter = juce::CharacterFunctions::toUpperCase (part[0]);
            if (letter < 'A' || letter > 'G' || ! juce::CharacterFunctions::isUpperCase (part[0]))
                continue;
            auto rest = part.substring (1);
            juce::String accidental;
            if (rest.startsWith ("#") || rest.startsWith ("b"))
            {
                // ("b" alone after the letter is a flat; "bass" isn't.)
                if (rest.startsWith ("#") || rest.length() == 1 || rest.substring (1).startsWithIgnoreCase ("m"))
                {
                    accidental = rest.substring (0, 1);
                    rest = rest.substring (1);
                }
            }
            const auto lower = rest.toLowerCase();
            if (lower.isEmpty() || lower == "maj" || lower == "major")
                return juce::String::charToString (letter) + accidental;
            if (lower == "m" || lower == "min" || lower == "minor")
                return juce::String::charToString (letter) + accidental + "m";
        }
        return {};
    }

    MediaItem MediaDatabase::read (const juce::File& file, juce::AudioFormatManager& formats)
    {
        MediaItem item;
        item.file = file;
        item.type = mediaTypeOf (file).value_or (MediaType::audio);
        item.name = file.getFileNameWithoutExtension();
        item.size = file.getSize();
        item.modified = file.getLastModificationTime();
        item.bpm = bpmFromName (item.name);
        item.key = keyFromName (item.name);
        if (item.type == MediaType::audio)
        {
            if (std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file)); reader != nullptr)
            {
                item.sampleRate = reader->sampleRate;
                item.channels = (int) reader->numChannels;
                item.seconds = reader->sampleRate > 0.0 ? (double) reader->lengthInSamples / reader->sampleRate : 0.0;
            }
        }
        else if (item.type == MediaType::midi)
        {
            juce::String error;
            if (const auto midi = readMidiFile (file, error))
            {
                if (item.bpm <= 0.0)
                    item.bpm = midi->bpm;
                for (const auto& track : midi->tracks)
                    for (const auto& note : track.notes)
                    {
                        item.beats = juce::jmax (item.beats, note.start + note.length);
                        ++item.notes;
                    }
                item.seconds = item.beats * 60.0 / juce::jmax (1.0, midi->bpm);
            }
        }
        return item;
    }

    //==========================================================================
    class MediaDatabase::Scanner final : public juce::Thread
    {
    public:
        explicit Scanner (MediaDatabase& d) : juce::Thread ("MediaBay scan"), database (d) {}
        ~Scanner() override { stopThread (10000); }

        void run() override
        {
            auto found = database.scanAll ([this] { return threadShouldExit(); });
            if (threadShouldExit())
                return;
            const juce::ScopedLock sl (database.pendingLock);
            database.pending = std::move (found);
        }

    private:
        MediaDatabase& database;
    };

    MediaDatabase::MediaDatabase (juce::File s) : store (std::move (s))
    {
        load();
    }

    MediaDatabase::~MediaDatabase()
    {
        scanner.reset();
    }

    bool MediaDatabase::addLocation (const juce::File& folder)
    {
        if (! folder.isDirectory() || locations.contains (folder.getFullPathName()))
            return false;
        {
            const juce::ScopedLock sl (pendingLock);
            locations.add (folder.getFullPathName());
        }
        save();
        return true;
    }

    void MediaDatabase::removeLocation (const juce::File& folder)
    {
        const juce::ScopedLock sl (pendingLock);   // (a scan in another thread reads `items` and `locations`)
        locations.removeString (folder.getFullPathName());
        items.erase (std::remove_if (items.begin(), items.end(), [this] (const MediaItem& item)
        {
            for (const auto& location : locations)
                if (item.file.isAChildOf (juce::File (location)))
                    return false;
            return true;
        }), items.end());
        save();
    }

    std::vector<MediaItem> MediaDatabase::scanAll (std::function<bool()> stop)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        // What was read before, by path, to skip unchanged files. (A copy: the message thread owns `items`.)
        std::map<juce::String, MediaItem> known;
        juce::StringArray where;
        {
            const juce::ScopedLock sl (pendingLock);
            for (const auto& item : items)
                known[item.file.getFullPathName()] = item;
            where = locations;
        }
        std::vector<MediaItem> found;
        std::set<juce::String> seen;
        filesSeen = 0;
        for (const auto& location : where)
        {
            for (const auto& entry : juce::RangedDirectoryIterator (juce::File (location), true, "*", juce::File::findFiles))
            {
                if (stop())
                    return {};
                while (shouldPause && shouldPause() && ! stop())
                    juce::Thread::sleep (100);
                const auto& file = entry.getFile();
                if (! mediaTypeOf (file) || ! seen.insert (file.getFullPathName()).second)
                    continue;
                ++filesSeen;
                if (auto old = known.find (file.getFullPathName());
                    old != known.end() && old->second.size == entry.getFileSize() && old->second.modified == entry.getModificationTime())
                {
                    found.push_back (old->second);
                    continue;
                }
                found.push_back (read (file, formats));
            }
        }
        return found;
    }

    void MediaDatabase::rescan()
    {
        scanner.reset();
        {
            const juce::ScopedLock sl (pendingLock);
            pending.reset();
        }
        scanning = true;
        scanner = std::make_unique<Scanner> (*this);
        scanner->startThread();
    }

    void MediaDatabase::scanNow()
    {
        scanner.reset();
        scanning = true;
        auto found = scanAll ([] { return false; });
        {
            const juce::ScopedLock sl (pendingLock);
            pending = std::move (found);
        }
        takeScanResults();
    }

    bool MediaDatabase::takeScanResults()
    {
        std::optional<std::vector<MediaItem>> result;
        {
            const juce::ScopedLock sl (pendingLock);
            std::swap (result, pending);
        }
        if (! result)
            return false;
        scanning = false;
        {
            const juce::ScopedLock sl (pendingLock);   // (a scan in another thread reads `items`)
            items = std::move (*result);
            for (auto& item : items)
                item.rating = getRating (item.file);
            std::stable_sort (items.begin(), items.end(), [] (const MediaItem& a, const MediaItem& b)
                              { return a.name.compareNatural (b.name) < 0; });
        }
        save();
        return true;
    }

    std::vector<int> MediaDatabase::find (const MediaQuery& query) const
    {
        const TextSearch search (query.text);
        std::vector<int> found;
        const auto folder = query.folder;
        for (int i = 0; i < (int) items.size(); ++i)
        {
            const auto& item = items[(size_t) i];
            if (! query.types.empty() && query.types.count (item.type) == 0)
                continue;
            if (item.rating < query.minimumRating)
                continue;
            if (folder != juce::File())
            {
                const auto parent = item.file.getParentDirectory();
                if (query.subfolders ? ! (parent == folder || item.file.isAChildOf (folder)) : parent != folder)
                    continue;
            }
            if (! search.isEmpty() && ! search.matches (item.searchText()))
                continue;
            found.push_back (i);
        }
        return found;
    }

    void MediaDatabase::sort (std::vector<int>& indices, MediaColumn column, bool ascending) const
    {
        const auto compare = [this, column] (int ia, int ib)
        {
            const auto& a = items[(size_t) ia];
            const auto& b = items[(size_t) ib];
            switch (column)
            {
                case MediaColumn::rating: return a.rating - b.rating;
                case MediaColumn::name: return a.name.compareNatural (b.name);
                case MediaColumn::type: return (int) a.type - (int) b.type;
                case MediaColumn::length: return a.seconds < b.seconds ? -1 : a.seconds > b.seconds ? 1 : 0;
                case MediaColumn::tempo: return a.bpm < b.bpm ? -1 : a.bpm > b.bpm ? 1 : 0;
                case MediaColumn::key: return a.key.compare (b.key);
                case MediaColumn::folder: return a.file.getParentDirectory().getFullPathName().compareNatural (b.file.getParentDirectory().getFullPathName());
                case MediaColumn::size: return a.size < b.size ? -1 : a.size > b.size ? 1 : 0;
                case MediaColumn::date: return a.modified < b.modified ? -1 : a.modified > b.modified ? 1 : 0;
            }
            return 0;
        };
        std::stable_sort (indices.begin(), indices.end(), [&] (int a, int b)
        {
            const auto c = compare (a, b);
            return ascending ? c < 0 : c > 0;
        });
    }

    void MediaDatabase::shuffle (std::vector<int>& indices, juce::uint32 seed)
    {
        std::mt19937 random (seed);
        std::shuffle (indices.begin(), indices.end(), random);
    }

    void MediaDatabase::setRating (const juce::File& file, int stars)
    {
        stars = juce::jlimit (0, 5, stars);
        if (stars == 0)
            ratings.erase (file.getFullPathName());
        else
            ratings[file.getFullPathName()] = stars;
        {
            const juce::ScopedLock sl (pendingLock);
            for (auto& item : items)
                if (item.file == file)
                    item.rating = stars;
        }
        save();
    }

    int MediaDatabase::getRating (const juce::File& file) const
    {
        const auto found = ratings.find (file.getFullPathName());
        return found != ratings.end() ? found->second : 0;
    }

    //==========================================================================
    void MediaDatabase::load()
    {
        if (store == juce::File() || ! store.existsAsFile())
            return;
        const auto xml = juce::XmlDocument::parse (store);
        if (xml == nullptr || ! xml->hasTagName ("MediaBay"))
            return;
        storeExisted = true;
        for (const auto* e : xml->getChildWithTagNameIterator ("Location"))
            locations.addIfNotAlreadyThere (e->getStringAttribute ("path"));
        for (const auto* e : xml->getChildWithTagNameIterator ("Rating"))
            ratings[e->getStringAttribute ("path")] = juce::jlimit (0, 5, e->getIntAttribute ("stars"));
        for (const auto* e : xml->getChildWithTagNameIterator ("Item"))
        {
            MediaItem item;
            item.file = juce::File (e->getStringAttribute ("path"));
            const auto type = mediaTypeOf (item.file);
            if (! type)
                continue;
            item.type = *type;
            item.name = item.file.getFileNameWithoutExtension();
            item.size = e->getStringAttribute ("size").getLargeIntValue();
            item.modified = juce::Time (e->getStringAttribute ("modified").getLargeIntValue());
            item.seconds = e->getDoubleAttribute ("seconds");
            item.sampleRate = e->getDoubleAttribute ("rate");
            item.channels = e->getIntAttribute ("channels");
            item.bpm = e->getDoubleAttribute ("bpm");
            item.key = e->getStringAttribute ("key");
            item.beats = e->getDoubleAttribute ("beats");
            item.notes = e->getIntAttribute ("notes");
            item.rating = getRating (item.file);
            items.push_back (item);
        }
    }

    void MediaDatabase::save() const
    {
        if (store == juce::File())
            return;
        juce::XmlElement xml ("MediaBay");
        for (const auto& location : locations)
            xml.createNewChildElement ("Location")->setAttribute ("path", location);
        for (const auto& [path, stars] : ratings)
        {
            auto* e = xml.createNewChildElement ("Rating");
            e->setAttribute ("path", path);
            e->setAttribute ("stars", stars);
        }
        for (const auto& item : items)
        {
            auto* e = xml.createNewChildElement ("Item");
            e->setAttribute ("path", item.file.getFullPathName());
            e->setAttribute ("size", juce::String (item.size));
            e->setAttribute ("modified", juce::String (item.modified.toMilliseconds()));
            if (item.seconds > 0.0) e->setAttribute ("seconds", item.seconds);
            if (item.sampleRate > 0.0) e->setAttribute ("rate", item.sampleRate);
            if (item.channels > 0) e->setAttribute ("channels", item.channels);
            if (item.bpm > 0.0) e->setAttribute ("bpm", item.bpm);
            if (item.key.isNotEmpty()) e->setAttribute ("key", item.key);
            if (item.beats > 0.0) e->setAttribute ("beats", item.beats);
            if (item.notes > 0) e->setAttribute ("notes", item.notes);
        }
        store.getParentDirectory().createDirectory();
        xml.writeTo (store);
    }
}
