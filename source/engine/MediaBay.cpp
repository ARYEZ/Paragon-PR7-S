#include "MediaBay.h"

#include "MidiImport.h"

#include <algorithm>
#include <cstring>
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
            case MediaType::pluginPreset: return "VST PRESET";
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
        if (extension == ".vstpreset")
            return MediaType::pluginPreset;
        return std::nullopt;
    }

    juce::String MediaItem::searchText() const
    {
        juce::String text;
        text << name << ' ' << file.getFileName() << ' ' << file.getParentDirectory().getFileName() << ' ' << mediaTypeName (type);
        if (type == MediaType::trackPreset || type == MediaType::pluginPreset)
            text << " preset";
        if (presetPlugin.isNotEmpty())
            text << ' ' << presetPlugin;
        if (tempo() > 0.0)
            text << ' ' << juce::String (tempo(), 0) << " bpm";
        if (keyName().isNotEmpty())
            text << ' ' << keyName();
        if (info.isNotEmpty())
            text << ' ' << info;
        for (const auto* t : { &tags.category, &tags.subCategory, &tags.style, &tags.subStyle, &tags.comment })
            if (t->isNotEmpty())
                text << ' ' << *t;
        for (const auto& c : tags.character)
            text << ' ' << c;
        for (const auto& [name, value] : tags.user)
            if (value.isNotEmpty() && value != "0" && value != "1")
                text << ' ' << value;
        return text.toLowerCase();
    }

    //==========================================================================
    juce::String AttributeId::name() const
    {
        switch (kind)
        {
            case MediaAttribute::category: return "CATEGORY";
            case MediaAttribute::subCategory: return "SUB CATEGORY";
            case MediaAttribute::style: return "STYLE";
            case MediaAttribute::subStyle: return "SUB STYLE";
            case MediaAttribute::character: return "CHARACTER";
            case MediaAttribute::tempo: return "TEMPO";
            case MediaAttribute::key: return "KEY";
            case MediaAttribute::rating: return "RATING";
            case MediaAttribute::comment: return "COMMENT";
            case MediaAttribute::user: return user.toUpperCase();
        }
        return {};
    }

    bool MediaTags::isEmpty() const
    {
        return category.isEmpty() && subCategory.isEmpty() && style.isEmpty() && subStyle.isEmpty() && comment.isEmpty()
            && key.isEmpty() && bpm <= 0.0 && character.isEmpty() && user.empty();
    }

    const std::vector<AttributeList>& defaultCategories()
    {
        static const std::vector<AttributeList> list {
            { "Drums", { "Kick", "Snare", "Hi-Hat", "Cymbal", "Tom", "Percussion", "Kit", "Fill" } },
            { "Guitar", { "Rhythm", "Lead", "Riff", "Chug", "Clean", "Acoustic", "Noise" } },
            { "Bass", { "Electric", "Synth", "Sub", "Upright" } },
            { "Keys", { "Piano", "Electric Piano", "Organ", "Clav" } },
            { "Synth", { "Lead", "Pad", "Arp", "Pluck", "Sequence" } },
            { "Strings", { "Ensemble", "Solo", "Pizzicato" } },
            { "Brass and Winds", { "Brass", "Woodwinds", "Ensemble" } },
            { "Vocals", { "Lead", "Backing", "Choir", "Chop", "Spoken" } },
            { "Sound FX", { "Riser", "Impact", "Sweep", "Reverse", "Glitch" } },
            { "Ambience", { "Drone", "Texture", "Field Recording" } },
        };
        return list;
    }

    const std::vector<AttributeList>& defaultStyles()
    {
        static const std::vector<AttributeList> list {
            { "Metal", { "Djent", "Metalcore", "Thrash", "Death", "Progressive", "Nu Metal", "Doom" } },
            { "Rock", { "Hard Rock", "Alternative", "Punk", "Classic Rock", "Post-Rock" } },
            { "Pop", { "Dance Pop", "Synth Pop", "Ballad" } },
            { "Electronic", { "Synthwave", "Dubstep", "House", "Techno", "Drum and Bass", "Ambient" } },
            { "Hip-Hop", { "Trap", "Boom Bap", "Lo-Fi" } },
            { "Cinematic", { "Trailer", "Score", "Hybrid" } },
            { "Blues", { "Delta", "Electric Blues", "Blues Rock" } },
            { "Jazz", { "Swing", "Fusion", "Bebop" } },
            { "Funk", { "Classic Funk", "Disco" } },
        };
        return list;
    }

    const std::vector<CharacterPair>& characterPairs()
    {
        static const std::vector<CharacterPair> pairs {
            { "Clean", "Distorted" }, { "Dry", "Processed" }, { "Dark", "Bright" }, { "Soft", "Hard" },
            { "Acoustic", "Electric" }, { "Short", "Long" }, { "Simple", "Complex" }, { "Tight", "Loose" },
            { "Calm", "Aggressive" },
        };
        return pairs;
    }

    const juce::StringArray& mediaKeyNames()
    {
        static const juce::StringArray names { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B",
                                               "Cm", "C#m", "Dm", "Ebm", "Em", "Fm", "F#m", "Gm", "G#m", "Am", "Bbm", "Bm" };
        return names;
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
        juce::String bare;
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
            // A bare letter ("Ghosthack-A_Kit") is the weakest sign: kept only if no part says
            // more ("Am", "F#", "C maj"); a later part that does wins (0.225.0).
            if (lower.isEmpty())
            {
                if (accidental.isNotEmpty())
                    return juce::String::charToString (letter) + accidental;
                if (bare.isEmpty())
                    bare = juce::String::charToString (letter);
                continue;
            }
            if (lower == "maj" || lower == "major")
                return juce::String::charToString (letter) + accidental;
            if (lower == "m" || lower == "min" || lower == "minor")
                return juce::String::charToString (letter) + accidental + "m";
        }
        return bare;
    }

    juce::String MediaDatabase::keyFromRootNote (int midiNote, int mode)
    {
        static const char* const major[] { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        static const char* const minor[] { "Cm", "C#m", "Dm", "Ebm", "Em", "Fm", "F#m", "Gm", "G#m", "Am", "Bbm", "Bm" };
        const auto pc = ((midiNote % 12) + 12) % 12;
        return mode == 1 ? minor[pc] : major[pc];
    }

    juce::String MediaDatabase::readIXml (const juce::File& wav)
    {
        // Walk the RIFF chunks for "iXML" (JUCE keeps only its ASWG part).
        juce::FileInputStream in (wav);
        if (! in.openedOk())
            return {};
        char riff[4], wave[4];
        if (in.read (riff, 4) != 4 || std::memcmp (riff, "RIFF", 4) != 0)
            return {};
        in.readInt();
        if (in.read (wave, 4) != 4 || std::memcmp (wave, "WAVE", 4) != 0)
            return {};
        for (int chunks = 0; chunks < 200 && ! in.isExhausted(); ++chunks)
        {
            char id[4];
            if (in.read (id, 4) != 4)
                return {};
            const auto size = (juce::int64) (juce::uint32) in.readInt();
            const auto next = in.getPosition() + size + (size & 1);
            if (std::memcmp (id, "iXML", 4) == 0)
            {
                if (size <= 0 || size > 4 * 1024 * 1024)
                    return {};
                juce::MemoryBlock block;
                in.readIntoMemoryBlock (block, (juce::pointer_sized_int) size);   // (not ssize_t: Windows has none)
                return block.toString();
            }
            if (! in.setPosition (next))
                return {};
        }
        return {};
    }

    namespace
    {
        // "Drum&Perc | Kick" -> "Drum&Perc", "Kick" (no bar: all of it, no sub).
        void splitPair (const juce::String& value, juce::String& first, juce::String& second)
        {
            first = value.upToFirstOccurrenceOf ("|", false, false).trim();
            second = value.contains ("|") ? value.fromFirstOccurrenceOf ("|", false, false).trim() : juce::String();
        }

        void addCharacter (MediaTags& t, const juce::String& value)
        {
            for (auto word : juce::StringArray::fromTokens (value, ",|;/ ", ""))
                for (const auto& pair : characterPairs())
                    for (const auto& side : { pair.left, pair.right })
                        if (word.trim().equalsIgnoreCase (side))
                            t.character.addIfNotAlreadyThere (side);
        }
    }

    MediaTags MediaDatabase::tagsFromIXml (const juce::String& ixml, double& bpm, juce::String& key, juce::StringArray* searchable)
    {
        // <BWFXML><STEINBERG><ATTR_LIST><ATTR><NAME/><TYPE/><VALUE/></ATTR>... The names aren't
        // published by its maker: the ones read first are those sound libraries and tag editors
        // write (MusicalInstrument "Category|Sub", MusicalStyle "Style|Sub", ...); anything else
        // is matched by what its name holds.
        MediaTags t;
        const auto xml = juce::parseXML (ixml);
        const auto* steinberg = xml != nullptr ? (xml->hasTagName ("STEINBERG") ? xml.get() : xml->getChildByName ("STEINBERG")) : nullptr;
        const auto* list = steinberg != nullptr ? steinberg->getChildByName ("ATTR_LIST") : nullptr;
        if (list == nullptr)
            return t;
        juce::String genre, postCategory;
        for (const auto* attr : list->getChildWithTagNameIterator ("ATTR"))
        {
            const auto exact = attr->getChildElementAllSubText ("NAME", {}).trim();
            const auto name = exact.toLowerCase();
            const auto value = attr->getChildElementAllSubText ("VALUE", {}).trim();
            if (value.isEmpty())
                continue;
            const auto sub = name.contains ("sub");
            juce::String first, second;
            splitPair (value, first, second);
            if (name == "musicalinstrument" || name == "musicalcategory" || (name.contains ("category") && ! name.contains ("post") && ! name.contains ("template")))
            {
                if (sub)
                    t.subCategory = second.isNotEmpty() ? second : first;
                else
                {
                    t.category = first;
                    if (second.isNotEmpty())
                        t.subCategory = second;
                }
            }
            else if (name == "mediacategorypost")
                postCategory = value;
            else if (name.contains ("style"))
            {
                if (sub)
                    t.subStyle = second.isNotEmpty() ? second : first;
                else
                {
                    t.style = first;
                    if (second.isNotEmpty())
                        t.subStyle = second;
                }
            }
            else if (name.contains ("genre"))
                genre = value;
            else if (name.contains ("character"))
                addCharacter (t, value);
            else if (name.contains ("comment"))
                t.comment = value;
            else if (name.contains ("tempo") || name.contains ("bpm"))
            {
                const auto v = value.getDoubleValue();
                if (v >= 20.0 && v <= 400.0)
                    bpm = v;
            }
            else if (name.contains ("key") && ! name.contains ("keyword"))
            {
                // "Am", "A minor", "F# maj"...
                auto spoken = value.replace (" minor", "m").replace (" Minor", "m").replace (" min", "m").replace (" major", "").replace (" Major", "").replace (" maj", "");
                if (const auto k = keyFromName (spoken.removeCharacters (" ")); k.isNotEmpty())
                    key = k;
            }
            else if (searchable != nullptr
                     && (name.contains ("library") || name.contains ("album") || name.contains ("artist")
                         || name.contains ("title") || name.contains ("company") || name.contains ("keyword")
                         || name.contains ("cdname") || name.contains ("description")))
                searchable->addIfNotAlreadyThere (value);
        }
        if (t.category.isEmpty() && postCategory.isNotEmpty())
            splitPair (postCategory, t.category, t.subCategory);
        else if (searchable != nullptr && postCategory.isNotEmpty())
            searchable->addIfNotAlreadyThere (postCategory);
        if (t.style.isEmpty() && genre.isNotEmpty())
            splitPair (genre, t.style, t.subStyle);
        return t;
    }

    MediaTags MediaDatabase::tagsFromAppleLoop (const juce::String& appleTags, juce::StringArray* searchable)
    {
        static const juce::StringArray genres { "Rock/Blues", "Electronic/Dance", "Jazz", "Urban", "World/Ethnic",
                                                "Cinematic/New Age", "Orchestral", "Country/Folk", "Experimental", "Other Genre" };
        // Apple's descriptors: those that are one of our character sides become one; the rest
        // (Single, Ensemble, Part, Fill, Grooving, Arrhythmic, Melodic, Dissonant, Cheerful,
        // Relaxed, Intense...) are searched.
        static const juce::StringArray descriptors { "Single", "Ensemble", "Part", "Fill", "Acoustic", "Electric",
                                                     "Dry", "Processed", "Clean", "Distorted", "Cheerful", "Dark",
                                                     "Relaxed", "Intense", "Grooving", "Arrhythmic", "Melodic", "Dissonant" };
        MediaTags t;
        for (auto tag : juce::StringArray::fromTokens (appleTags, ";", ""))
        {
            tag = tag.trim();
            if (tag.isEmpty())
                continue;
            if (genres.contains (tag))
            {
                if (t.style.isEmpty())
                    t.style = tag;
            }
            else if (descriptors.contains (tag))
            {
                const auto before = t.character.size();
                addCharacter (t, tag);
                if (t.character.size() == before && searchable != nullptr)
                    searchable->addIfNotAlreadyThere (tag);
            }
            else if (t.category.isEmpty())
                t.category = tag;
            else if (t.subCategory.isEmpty())
                t.subCategory = tag;
            else if (searchable != nullptr)
                searchable->addIfNotAlreadyThere (tag);
        }
        return t;
    }

    bool MediaDatabase::readVstPresetInfo (const juce::File& file, juce::String& classId, juce::StringPairArray& info)
    {
        // "VST3", a version (4), the class id (32 ASCII), the chunk list's offset (8); the list:
        // "List", a count, then per chunk its id (4), offset (8), size (8). Little-endian.
        juce::FileInputStream in (file);
        if (! in.openedOk() || in.getTotalLength() < 48)
            return false;
        char header[48];
        if (in.read (header, 48) != 48 || std::memcmp (header, "VST3", 4) != 0)
            return false;
        classId = juce::String (juce::CharPointer_ASCII (header + 8), 32).trim();
        const auto listAt = (juce::int64) juce::ByteOrder::littleEndianInt64 (header + 40);
        const auto total = in.getTotalLength();
        if (listAt < 48 || listAt + 8 > total || ! in.setPosition (listAt))
            return true;
        char list[8];
        if (in.read (list, 8) != 8 || std::memcmp (list, "List", 4) != 0)
            return true;
        const auto count = juce::jlimit (0, 64, (int) juce::ByteOrder::littleEndianInt (list + 4));
        for (int i = 0; i < count; ++i)
        {
            char entry[20];
            if (! in.setPosition (listAt + 8 + (juce::int64) i * 20) || in.read (entry, 20) != 20)
                break;
            if (std::memcmp (entry, "Info", 4) != 0)
                continue;
            const auto offset = (juce::int64) juce::ByteOrder::littleEndianInt64 (entry + 4);
            const auto size = (juce::int64) juce::ByteOrder::littleEndianInt64 (entry + 12);
            if (offset < 0 || size <= 0 || size > 1024 * 1024 || offset > total || size > total - offset || ! in.setPosition (offset))
                break;
            juce::MemoryBlock block;
            in.readIntoMemoryBlock (block, (juce::pointer_sized_int) size);
            // <MetaInfo><Attr id="PlugInName" value="..." type="string"/>...</MetaInfo>
            if (const auto xml = juce::parseXML (block.toString()))
                for (const auto* attr : xml->getChildIterator())
                    if (const auto id = attr->getStringAttribute ("id"); id.isNotEmpty())
                        info.set (id, attr->getStringAttribute ("value"));
            break;
        }
        return true;
    }

    int MediaDatabase::vst3UniqueIdFor (const juce::String& classId)
    {
        // As JUCE does for a VST3: the class id's four 32-bit numbers, hashed (value * 31 + each).
        const auto hex = classId.trim();
        if (hex.length() != 32 || ! hex.containsOnly ("0123456789abcdefABCDEF"))
            return 0;
        juce::uint32 value = 0;
        for (int i = 0; i < 4; ++i)
            value = value * 31 + (juce::uint32) hex.substring (i * 8, i * 8 + 8).getHexValue64();
        return (int) value;
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

                // What the file says about itself (0.22.2): a loop's tempo, beats and root note
                // (WAV: ACID; AIFF: Apple loop), its description, genre, comments, keywords.
                const auto& m = reader->metadataValues;
                const auto number = [&m] (const char* k) { return m.getValue (k, {}).getDoubleValue(); };
                double tempo = 0.0, beats = 0.0;
                int root = -1, mode = -1;
                bool oneShot = false;
                if (m.containsKey (juce::WavAudioFormat::acidTempo) || m.containsKey (juce::WavAudioFormat::acidBeats))
                {
                    tempo = number (juce::WavAudioFormat::acidTempo);
                    beats = number (juce::WavAudioFormat::acidBeats);
                    oneShot = m.getValue (juce::WavAudioFormat::acidOneShot, "0") == "1";
                    if (m.getValue (juce::WavAudioFormat::acidRootSet, "0") == "1")
                        root = m.getValue (juce::WavAudioFormat::acidRootNote, "-1").getIntValue();
                }
                if (m.containsKey (juce::AiffAudioFormat::appleBeats))
                {
                    beats = number (juce::AiffAudioFormat::appleBeats);
                    oneShot = m.getValue (juce::AiffAudioFormat::appleOneShot, "0") == "1";
                    if (m.getValue (juce::AiffAudioFormat::appleRootSet, "0") == "1")
                        root = m.getValue (juce::AiffAudioFormat::appleRootNote, "-1").getIntValue();
                    const auto scale = m.getValue (juce::AiffAudioFormat::appleKey, {});
                    mode = scale == "minor" ? 1 : scale == "major" ? 0 : -1;
                }
                if (tempo <= 0.0 && beats > 0.0 && item.seconds > 0.0 && ! oneShot)
                    tempo = beats * 60.0 / item.seconds;
                if (tempo >= 20.0 && tempo <= 400.0 && ! oneShot)
                {
                    item.bpm = std::round (tempo * 100.0) / 100.0;
                    item.beats = beats;
                }
                if (item.key.isEmpty() && root >= 0 && root < 128)
                    item.key = keyFromRootNote (root, mode);

                juce::StringArray info;
                for (const auto* k : { juce::WavAudioFormat::bwavDescription, juce::WavAudioFormat::riffInfoGenre, juce::WavAudioFormat::riffInfoComment,
                                       juce::WavAudioFormat::riffInfoComments, juce::WavAudioFormat::riffInfoKeywords, juce::WavAudioFormat::riffInfoSubject })
                    if (const auto v = m.getValue (k, {}).trim(); v.isNotEmpty())
                        info.addIfNotAlreadyThere (v);
                // Apple Loops' tags (0.224.0): genre, instrument, descriptors.
                if (const auto apple = m.getValue (juce::AiffAudioFormat::appleTag, {}); apple.isNotEmpty())
                    item.fileTags = tagsFromAppleLoop (apple, &info);
                // A RIFF INFO genre is the style when nothing else gives one (0.224.0).
                if (const auto genre = m.getValue (juce::WavAudioFormat::riffInfoGenre, {}).trim(); genre.isNotEmpty() && item.fileTags.style.isEmpty())
                    item.fileTags.style = genre;
                item.info = info.joinIntoString (" ").substring (0, 400);
            }
            // The other DAW's MediaBay tags, in the WAV's iXML.
            if (file.hasFileExtension ("wav;wave;bwf"))
                if (const auto ixml = readIXml (file); ixml.containsIgnoreCase ("STEINBERG"))
                {
                    double bpm = 0.0;
                    juce::String key;
                    juce::StringArray more;
                    auto fromIXml = tagsFromIXml (ixml, bpm, key, &more);
                    // (the iXML's word wins; what it doesn't say is kept from INFO / ACID)
                    if (fromIXml.style.isEmpty())
                    {
                        fromIXml.style = item.fileTags.style;
                        fromIXml.subStyle = item.fileTags.subStyle;
                    }
                    item.fileTags = fromIXml;
                    if (bpm > 0.0 && item.bpm <= 0.0)
                        item.bpm = bpm;
                    if (key.isNotEmpty() && item.key.isEmpty())
                        item.key = key;
                    if (! more.isEmpty())
                        item.info = (item.info + " " + more.joinIntoString (" ")).trim().substring (0, 600);
                }
        }
        else if (item.type == MediaType::pluginPreset)
        {
            // A VST preset (0.24.0): its plug-in, and the tags its Info gives (as the iXML's: they
            // become its tags when it has none of yours).
            juce::StringPairArray presetInfo;
            if (readVstPresetInfo (file, item.presetClass, presetInfo))
            {
                item.presetPlugin = presetInfo.getValue ("PlugInName", {});
                if (item.presetPlugin.isEmpty())   // (the VST3 preset folders: .../<Vendor>/<Plug-in>/<preset>.vstpreset)
                    item.presetPlugin = file.getParentDirectory().getFileName();
                item.presetInstrument = presetInfo.getValue ("PlugInCategory", {}).startsWithIgnoreCase ("Instrument");
                const auto split = [] (const juce::String& v, juce::String& a, juce::String& b)
                {
                    a = v.upToFirstOccurrenceOf ("|", false, false).trim();
                    b = v.fromFirstOccurrenceOf ("|", false, false).trim();
                };
                split (presetInfo.getValue ("MusicalInstrument", {}), item.fileTags.category, item.fileTags.subCategory);
                split (presetInfo.getValue ("MusicalStyle", {}), item.fileTags.style, item.fileTags.subStyle);
                for (auto word : juce::StringArray::fromTokens (presetInfo.getValue ("MusicalCharacter", {}), ",|;", ""))
                    for (const auto& pair : characterPairs())
                        for (const auto& side : { pair.left, pair.right })
                            if (word.trim().equalsIgnoreCase (side))
                                item.fileTags.character.addIfNotAlreadyThere (side);
                if (const auto name = presetInfo.getValue ("Name", {}); name.isNotEmpty())
                    item.name = name;
                item.info = presetInfo.getValue ("PlugInCategory", {}).replaceCharacter ('|', ' ');
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
        // What the file doesn't say, guessed from its name and folders (0.225.0).
        if (item.type == MediaType::audio || item.type == MediaType::midi)
        {
            const auto guessed = tagsFromWords (file);
            auto& t = item.fileTags;
            if (t.category.isEmpty())
            {
                t.category = guessed.category;
                t.subCategory = guessed.subCategory;
            }
            if (t.style.isEmpty())
            {
                t.style = guessed.style;
                t.subStyle = guessed.subStyle;
            }
            if (t.character.isEmpty())
                t.character = guessed.character;
        }
        return item;
    }

    MediaTags MediaDatabase::tagsFromWords (const juce::File& file)
    {
        // The words of a name, lower case: letters and digits only ("Hi-Hat" -> "hi", "hat").
        const auto wordsOf = [] (const juce::String& text)
        {
            juce::StringArray words;
            juce::String word;
            for (auto c : text)
            {
                if (juce::CharacterFunctions::isLetterOrDigit (c))
                    word += juce::CharacterFunctions::toLowerCase (c);
                else if (word.isNotEmpty())
                {
                    words.add (word);
                    word.clear();
                }
            }
            if (word.isNotEmpty())
                words.add (word);
            return words;
        };
        struct Rule { const char* words; const char* category; const char* sub; };
        // In order: the first rule whose words are all there wins ("bass guitar" before "guitar",
        // "bass drum" before "bass", instruments before roles such as "lead" or "pad").
        static const Rule rules[] {
            { "bass drum", "Drums", "Kick" }, { "kick", "Drums", "Kick" }, { "kicks", "Drums", "Kick" },
            { "bass guitar", "Bass", "Electric" }, { "upright bass", "Bass", "Upright" }, { "double bass", "Bass", "Upright" },
            { "synth bass", "Bass", "Synth" }, { "sub bass", "Bass", "Sub" }, { "808", "Bass", "Sub" },
            { "snare", "Drums", "Snare" }, { "snares", "Drums", "Snare" }, { "rimshot", "Drums", "Snare" },
            { "hihat", "Drums", "Hi-Hat" }, { "hihats", "Drums", "Hi-Hat" }, { "hi hat", "Drums", "Hi-Hat" }, { "hat", "Drums", "Hi-Hat" }, { "hats", "Drums", "Hi-Hat" },
            { "cymbal", "Drums", "Cymbal" }, { "cymbals", "Drums", "Cymbal" }, { "crash", "Drums", "Cymbal" }, { "ride", "Drums", "Cymbal" }, { "china", "Drums", "Cymbal" },
            { "tom", "Drums", "Tom" }, { "toms", "Drums", "Tom" },
            { "fill", "Drums", "Fill" }, { "fills", "Drums", "Fill" },
            { "clap", "Drums", "Percussion" }, { "claps", "Drums", "Percussion" }, { "perc", "Drums", "Percussion" }, { "percs", "Drums", "Percussion" },
            { "percussion", "Drums", "Percussion" }, { "percussions", "Drums", "Percussion" }, { "shaker", "Drums", "Percussion" },
            { "tambourine", "Drums", "Percussion" }, { "conga", "Drums", "Percussion" }, { "bongo", "Drums", "Percussion" }, { "snap", "Drums", "Percussion" },
            { "guitar", "Guitar", nullptr }, { "guitars", "Guitar", nullptr }, { "gtr", "Guitar", nullptr },
            { "bass", "Bass", nullptr }, { "bassline", "Bass", nullptr }, { "sub", "Bass", "Sub" },
            { "electric piano", "Keys", "Electric Piano" }, { "rhodes", "Keys", "Electric Piano" }, { "epiano", "Keys", "Electric Piano" }, { "wurli", "Keys", "Electric Piano" },
            { "piano", "Keys", "Piano" }, { "organ", "Keys", "Organ" }, { "clav", "Keys", "Clav" }, { "keys", "Keys", nullptr },
            { "cello", "Strings", "Solo" }, { "violin", "Strings", "Solo" }, { "viola", "Strings", "Solo" }, { "pizzicato", "Strings", "Pizzicato" }, { "pizz", "Strings", "Pizzicato" },
            { "strings", "Strings", "Ensemble" }, { "string", "Strings", nullptr },
            { "trumpet", "Brass and Winds", "Brass" }, { "trombone", "Brass and Winds", "Brass" }, { "horn", "Brass and Winds", "Brass" }, { "horns", "Brass and Winds", "Brass" }, { "brass", "Brass and Winds", "Brass" },
            { "flute", "Brass and Winds", "Woodwinds" }, { "sax", "Brass and Winds", "Woodwinds" }, { "saxophone", "Brass and Winds", "Woodwinds" }, { "clarinet", "Brass and Winds", "Woodwinds" },
            { "choir", "Vocals", "Choir" }, { "chop", "Vocals", "Chop" }, { "vocal", "Vocals", nullptr }, { "vocals", "Vocals", nullptr }, { "vox", "Vocals", nullptr }, { "voice", "Vocals", nullptr }, { "chant", "Vocals", nullptr },
            { "drone", "Ambience", "Drone" }, { "drones", "Ambience", "Drone" }, { "texture", "Ambience", "Texture" }, { "textures", "Ambience", "Texture" },
            { "soundscape", "Ambience", "Texture" }, { "soundscapes", "Ambience", "Texture" }, { "atmosphere", "Ambience", "Texture" }, { "atmos", "Ambience", "Texture" },
            { "riser", "Sound FX", "Riser" }, { "uplifter", "Sound FX", "Riser" }, { "impact", "Sound FX", "Impact" }, { "sweep", "Sound FX", "Sweep" },
            { "downlifter", "Sound FX", "Sweep" }, { "whoosh", "Sound FX", "Sweep" }, { "reverse", "Sound FX", "Reverse" }, { "glitch", "Sound FX", "Glitch" },
            { "fx", "Sound FX", nullptr }, { "sfx", "Sound FX", nullptr },
            { "pad", "Synth", "Pad" }, { "pads", "Synth", "Pad" }, { "arp", "Synth", "Arp" }, { "arpeggio", "Synth", "Arp" }, { "pluck", "Synth", "Pluck" }, { "plucks", "Synth", "Pluck" },
            { "sequence", "Synth", "Sequence" }, { "seq", "Synth", "Sequence" }, { "lead", "Synth", "Lead" }, { "leads", "Synth", "Lead" }, { "synth", "Synth", nullptr },
            { "drum", "Drums", nullptr }, { "drums", "Drums", nullptr }, { "beat", "Drums", nullptr }, { "beats", "Drums", nullptr }, { "break", "Drums", nullptr }, { "breaks", "Drums", nullptr },
        };
        // A guitar's or a synth's part ("Lead Guitar" is GUITAR / LEAD).
        static const std::pair<const char*, const char*> guitarSubs[] {
            { "lead", "Lead" }, { "rhythm", "Rhythm" }, { "riff", "Riff" }, { "chug", "Chug" }, { "clean", "Clean" }, { "acoustic", "Acoustic" }, { "noise", "Noise" } };
        static const std::pair<const char*, const char*> styles[] {
            { "djent", "Metal|Djent" }, { "metalcore", "Metal|Metalcore" }, { "thrash", "Metal|Thrash" }, { "deathcore", "Metal|Death" }, { "death metal", "Metal|Death" },
            { "doom", "Metal|Doom" }, { "nu metal", "Metal|Nu Metal" }, { "numetal", "Metal|Nu Metal" }, { "metal", "Metal|" },
            { "punk", "Rock|Punk" }, { "post rock", "Rock|Post-Rock" }, { "hard rock", "Rock|Hard Rock" }, { "rock", "Rock|" },
            { "synthwave", "Electronic|Synthwave" }, { "dubstep", "Electronic|Dubstep" }, { "riddim", "Electronic|Dubstep" }, { "techno", "Electronic|Techno" },
            { "house", "Electronic|House" }, { "dnb", "Electronic|Drum and Bass" }, { "drum and bass", "Electronic|Drum and Bass" }, { "edm", "Electronic|" },
            { "trap", "Hip-Hop|Trap" }, { "drill", "Hip-Hop|Trap" }, { "boom bap", "Hip-Hop|Boom Bap" }, { "boombap", "Hip-Hop|Boom Bap" },
            { "lofi", "Hip-Hop|Lo-Fi" }, { "lo fi", "Hip-Hop|Lo-Fi" }, { "hip hop", "Hip-Hop|" }, { "hiphop", "Hip-Hop|" },
            { "trailer", "Cinematic|Trailer" }, { "cinematic", "Cinematic|" }, { "blues", "Blues|" }, { "jazz", "Jazz|" }, { "disco", "Funk|Disco" }, { "funk", "Funk|" },
            { "pop", "Pop|" },
        };
        static const std::pair<const char*, const char*> characters[] {
            { "wet", "Processed" }, { "processed", "Processed" }, { "dry", "Dry" }, { "clean", "Clean" }, { "distorted", "Distorted" }, { "dist", "Distorted" },
            { "gritty", "Distorted" }, { "dirty", "Distorted" }, { "overdrive", "Distorted" }, { "dark", "Dark" }, { "bright", "Bright" }, { "soft", "Soft" },
            { "hard", "Hard" }, { "acoustic", "Acoustic" }, { "electric", "Electric" }, { "aggressive", "Aggressive" }, { "calm", "Calm" }, { "tight", "Tight" },
        };

        const auto has = [] (const juce::StringArray& words, const char* phrase)
        {
            const auto want = juce::StringArray::fromTokens (phrase, " ", "");
            for (int i = 0; i + want.size() <= words.size(); ++i)
            {
                bool all = true;
                for (int k = 0; k < want.size() && all; ++k)
                    all = words[i + k] == want[k];
                if (all)
                    return true;
            }
            return false;
        };

        // The name first, then up to three folders up (not the drive). A folder's "fx" / "sfx" /
        // "kit" / "loops" says nothing about the file (libraries keep everything under "SFX").
        std::vector<juce::StringArray> places;
        places.push_back (wordsOf (file.getFileNameWithoutExtension()));
        auto folder = file.getParentDirectory();
        for (int up = 0; up < 3 && folder.getParentDirectory() != folder; ++up, folder = folder.getParentDirectory())
        {
            auto words = wordsOf (folder.getFileName());
            for (auto skip : { "fx", "sfx", "kit", "kits", "loops", "loop", "samples", "one", "shots", "oneshots" })
                words.removeString (skip);
            places.push_back (words);
        }

        MediaTags t;
        for (const auto& words : places)
        {
            for (const auto& r : rules)
                if (has (words, r.words))
                {
                    t.category = r.category;
                    t.subCategory = r.sub != nullptr ? r.sub : "";
                    if (t.category == "Guitar")
                        for (const auto& [w, sub] : guitarSubs)
                            if (has (words, w))
                            {
                                t.subCategory = sub;
                                break;
                            }
                    break;
                }
            if (t.category.isNotEmpty())
                break;
        }
        for (const auto& words : places)
        {
            for (const auto& [w, both] : styles)
                if (has (words, w))
                {
                    const juce::String value (both);
                    t.style = value.upToFirstOccurrenceOf ("|", false, false);
                    t.subStyle = value.fromFirstOccurrenceOf ("|", false, false);
                    break;
                }
            if (t.style.isNotEmpty())
                break;
        }
        // Character from the name only (a folder's "Dark" is rarely every file's).
        for (const auto& [w, side] : characters)
            if (has (places.front(), w))
            {
                // One side of a pair at most.
                bool taken = false;
                for (const auto& pair : characterPairs())
                    if ((pair.left == side || pair.right == side) && (t.character.contains (pair.left) || t.character.contains (pair.right)))
                        taken = true;
                if (! taken)
                    t.character.add (side);
            }
        return t;
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
            auto index = prepareScan (found);   // (0.190.0: sorted and indexed here, not on the screen's thread)
            const juce::ScopedLock sl (database.pendingLock);
            database.pending = std::move (found);
            database.pendingIndex = std::move (index);
        }

    private:
        MediaDatabase& database;
    };

    MediaDatabase::MediaDatabase (juce::File s) : store (std::move (s))
    {
        load();
        rebuildIndex();
    }

    MediaDatabase::~MediaDatabase()
    {
        scanner.reset();
        waitForSave();
        if (unsaved)
            save();
    }

    void MediaDatabase::saveSoon()
    {
        ++generation;
        unsaved = true;
        lastEdit = juce::Time::getMillisecondCounterHiRes();
    }

    void MediaDatabase::saveIfDue (double quietMs)
    {
        if (unsaved && juce::Time::getMillisecondCounterHiRes() - lastEdit >= quietMs)
            saveInBackground();
    }

    void MediaDatabase::rebuildIndex()
    {
        ++generation;
        byPath.clear();
        byPath.reserve (items.size() * 2);
        parents.assign (items.size(), {});
        for (int i = 0; i < (int) items.size(); ++i)
        {
            byPath[items[(size_t) i].file.getFullPathName()] = i;
            parents[(size_t) i] = items[(size_t) i].file.getParentDirectory().getFullPathName();
        }
        searchTexts.assign (items.size(), {});
        searchMade.assign (items.size(), 0);
        nameRank.clear();
        folderRank.clear();
    }

    void MediaDatabase::invalidateSearch (int index) const
    {
        if (juce::isPositiveAndBelow (index, (int) searchMade.size()))
            searchMade[(size_t) index] = 0;
    }

    int MediaDatabase::indexOf (const juce::File& file) const
    {
        const auto found = byPath.find (file.getFullPathName());
        return found != byPath.end() ? found->second : -1;
    }

    const std::vector<int>& MediaDatabase::nameRanks() const
    {
        if (nameRank.size() != items.size())
        {
            // The items are kept in name order (a scan's results are sorted so, and saved so): checked in one walk,
            // sorted only if they aren't.
            nameRank.resize (items.size());
            bool inOrder = true;
            for (size_t i = 1; i < items.size() && inOrder; ++i)
                inOrder = items[i - 1].name.compareNatural (items[i].name) <= 0;
            std::vector<int> order ((size_t) items.size());
            for (int i = 0; i < (int) order.size(); ++i)
                order[(size_t) i] = i;
            if (! inOrder)
                std::stable_sort (order.begin(), order.end(), [this] (int a, int b)
                                  { return items[(size_t) a].name.compareNatural (items[(size_t) b].name) < 0; });
            for (int r = 0; r < (int) order.size(); ++r)
                nameRank[(size_t) order[(size_t) r]] = r;
        }
        return nameRank;
    }

    const std::vector<int>& MediaDatabase::folderRanks() const
    {
        if (folderRank.size() != items.size())
        {
            // A few thousand folders among 120,000 files: the folders sorted once, each file given its folder's place.
            std::vector<juce::String> folders (parents.begin(), parents.end());
            std::sort (folders.begin(), folders.end());
            folders.erase (std::unique (folders.begin(), folders.end()), folders.end());
            std::stable_sort (folders.begin(), folders.end(), [] (const juce::String& a, const juce::String& b) { return a.compareNatural (b) < 0; });
            std::unordered_map<juce::String, int> rankOf;
            for (int r = 0; r < (int) folders.size(); ++r)
                rankOf[folders[(size_t) r]] = r;
            folderRank.resize (items.size());
            for (size_t i = 0; i < items.size(); ++i)
                folderRank[i] = rankOf[parents[i]];
        }
        return folderRank;
    }

    bool MediaDatabase::renameFile (const juce::File& file, const juce::String& newName, juce::String& error)
    {
        const auto name = juce::File::createLegalFileName (newName.trim());
        if (name.isEmpty())
        {
            error = "Type a name for it.";
            return false;
        }
        if (! file.existsAsFile())
        {
            error = "The file isn't there any more.";
            return false;
        }
        const auto target = file.getSiblingFile (name + file.getFileExtension());
        if (target == file)
            return true;
        if (target.exists())
        {
            error = "There's already a file called \"" + target.getFileName() + "\" in that folder.";
            return false;
        }
        if (! file.moveFileTo (target))
        {
            error = "The file couldn't be renamed (is it open in another program, or read-only?).";
            return false;
        }
        {
            const juce::ScopedLock sl (pendingLock);   // (a scan in another thread reads `items`)
            const auto oldPath = file.getFullPathName(), newPath = target.getFullPathName();
            if (auto r = ratings.find (oldPath); r != ratings.end())
            {
                ratings[newPath] = r->second;
                ratings.erase (oldPath);
            }
            if (auto t = tags.find (oldPath); t != tags.end())
            {
                tags[newPath] = t->second;
                tags.erase (oldPath);
            }
            if (const auto i = indexOf (file); i >= 0)
            {
                auto& item = items[(size_t) i];
                item.file = target;
                item.name = target.getFileNameWithoutExtension();
            }
            rebuildIndex();
        }
        save();
        return true;
    }

    bool MediaDatabase::addLocation (const juce::File& folder)
    {
        if (! folder.isDirectory() || locations.contains (folder.getFullPathName()))
            return false;
        {
            const juce::ScopedLock sl (pendingLock);
            locations.add (folder.getFullPathName());
            exclusions.removeString (folder.getFullPathName());
        }
        save();
        return true;
    }

    void MediaDatabase::removeLocation (const juce::File& folder)
    {
        {
            const juce::ScopedLock sl (pendingLock);   // (a scan in another thread reads `items` and `locations`)
            locations.removeString (folder.getFullPathName());
        }
        dropUnincluded();
        save();
    }

    void MediaDatabase::dropUnincluded()
    {
        const juce::ScopedLock sl (pendingLock);
        items.erase (std::remove_if (items.begin(), items.end(), [this] (const MediaItem& item)
                                     { return ! includedBy (item.file, locations, exclusions); }),
                     items.end());
        rebuildIndex();
    }

    bool MediaDatabase::includedBy (const juce::File& file, const juce::StringArray& scanned, const juce::StringArray& leftOut)
    {
        // The deepest of the scanned and left-out folders holding it (or it) decides.
        int deepest = -1;
        bool in = false;
        const auto consider = [&] (const juce::StringArray& list, bool scannedList)
        {
            for (const auto& path : list)
            {
                const juce::File folder (path);
                if ((file == folder || file.isAChildOf (folder)) && path.length() > deepest)
                {
                    deepest = path.length();
                    in = scannedList;
                }
            }
        };
        consider (scanned, true);
        consider (leftOut, false);
        return in;
    }

    bool MediaDatabase::isIncluded (const juce::File& file) const
    {
        return includedBy (file, locations, exclusions);
    }

    MediaDatabase::ScanState MediaDatabase::scanState (const juce::File& folder) const
    {
        const auto in = isIncluded (folder);
        // Anything deeper that decides otherwise makes it a square.
        for (const auto& path : in ? exclusions : locations)
            if (juce::File (path).isAChildOf (folder))
                return ScanState::some;
        return in ? ScanState::all : ScanState::none;
    }

    void MediaDatabase::setIncluded (const juce::File& folder, bool scanned)
    {
        if (folder == juce::File())
            return;
        {
            const juce::ScopedLock sl (pendingLock);
            // What was decided inside it goes: the whole folder is as asked now.
            const auto inside = [&folder] (const juce::String& path) { return juce::File (path).isAChildOf (folder); };
            for (auto* list : { &locations, &exclusions })
                for (int i = list->size(); --i >= 0;)
                    if (inside ((*list)[i]) || juce::File ((*list)[i]) == folder)
                        list->remove (i);
            if (includedBy (folder, locations, exclusions) != scanned)
                (scanned ? locations : exclusions).add (folder.getFullPathName());
        }
        dropUnincluded();
        save();
    }

    std::vector<MediaItem> MediaDatabase::scanAll (std::function<bool()> stop)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        // What was read before, by path, to skip unchanged files. (A copy: the message thread owns `items`.)
        std::map<juce::String, MediaItem> known;
        juce::StringArray where, leftOut;
        {
            const juce::ScopedLock sl (pendingLock);
            for (const auto& item : items)
                known[item.file.getFullPathName()] = item;
            where = locations;
            leftOut = exclusions;
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
                if (! leftOut.isEmpty() && ! includedBy (file, where, leftOut))
                    continue;
                ++filesSeen;
                if (auto old = known.find (file.getFullPathName());
                    old != known.end() && old->second.size == entry.getFileSize() && old->second.modified == entry.getModificationTime()
                    && old->second.readVersion >= MediaItem::currentReadVersion)
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
        auto index = prepareScan (found);
        {
            const juce::ScopedLock sl (pendingLock);
            pending = std::move (found);
            pendingIndex = std::move (index);
        }
        takeScanResults();
    }

    MediaDatabase::PreparedIndex MediaDatabase::prepareScan (std::vector<MediaItem>& found)
    {
        // (0.190.0) What takeScanResults did with the whole list on the message thread, done by the scan's own
        // thread: sorted by name (the same stable order), each file's index by path and its folder's path.
        std::stable_sort (found.begin(), found.end(), [] (const MediaItem& a, const MediaItem& b) { return a.name.compareNatural (b.name) < 0; });
        PreparedIndex index;
        index.count = found.size();
        index.byPath.reserve (found.size() * 2);
        index.parents.assign (found.size(), {});
        for (int i = 0; i < (int) found.size(); ++i)
        {
            index.byPath[found[(size_t) i].file.getFullPathName()] = i;
            index.parents[(size_t) i] = found[(size_t) i].file.getParentDirectory().getFullPathName();
        }
        return index;
    }

    bool MediaDatabase::takeScanResults()
    {
        std::optional<std::vector<MediaItem>> result;
        std::optional<PreparedIndex> index;
        {
            const juce::ScopedLock sl (pendingLock);
            std::swap (result, pending);
            std::swap (index, pendingIndex);
        }
        if (! result)
            return false;
        ++scansTaken;
        scanning = false;
        {
            const juce::ScopedLock sl (pendingLock);   // (a scan in another thread reads `items`)
            items = std::move (*result);
            for (auto& item : items)
            {
                // A file read for the first time with tags of its own (0.22.2): they become its
                // tags here, to change as you like (yours always win: only when it has none).
                if (! item.fileTags.isEmpty() && tags.count (item.file.getFullPathName()) == 0)
                {
                    tags[item.file.getFullPathName()] = item.fileTags;
                    item.fileTags = {};
                }
                item.rating = getRating (item.file);
                item.tags = getTags (item.file);
            }
            const auto byName = [] (const MediaItem& a, const MediaItem& b) { return a.name.compareNatural (b.name) < 0; };
            auto sortedAlready = std::is_sorted (items.begin(), items.end(), byName);
            if (! sortedAlready)
                std::stable_sort (items.begin(), items.end(), byName);
            if (sortedAlready && index.has_value() && index->count == items.size())
            {
                // (0.190.0) The index the scan made (the order is the one it was made for): rebuildIndex's work without it.
                ++generation;
                byPath = std::move (index->byPath);
                parents = std::move (index->parents);
                searchTexts.assign (items.size(), {});
                searchMade.assign (items.size(), 0);
                nameRank.clear();
                folderRank.clear();
            }
            else
                rebuildIndex();
            nameRank.resize (items.size());
            for (int i = 0; i < (int) items.size(); ++i)
                nameRank[(size_t) i] = i;   // (just sorted by name)
        }
        saveInBackground();
        return true;
    }

    void MediaDatabase::setItemsForTests (std::vector<MediaItem> list)
    {
        {
            const juce::ScopedLock sl (pendingLock);
            pending = std::move (list);
        }
        takeScanResults();
    }

    std::vector<int> MediaDatabase::find (const MediaQuery& query) const
    {
        const TextSearch search (query.text);
        const auto folderPath = query.folder == juce::File() ? juce::String() : query.folder.getFullPathName();
        std::vector<int> found;
        for (int i = 0; i < (int) items.size(); ++i)
            if (passes (i, query, search, folderPath))
                found.push_back (i);
        return found;
    }

    namespace
    {
        // (0.163.0) A file's folder against the folder chosen, as text (File::isAChildOf walked up each file's path).
        bool inFolder (const juce::String& parent, const juce::String& folder, bool subfolders)
        {
            if (parent == folder)
                return true;
            if (! subfolders || parent.length() <= folder.length() || ! parent.startsWith (folder))
                return false;
            const auto sep = juce::File::getSeparatorChar();
            return folder.getLastCharacter() == sep || parent[folder.length()] == sep;
        }
    }

    bool MediaDatabase::passes (int index, const MediaQuery& query, const TextSearch& search, const juce::String& folderPath) const
    {
        const auto& item = items[(size_t) index];
        if (! query.types.empty() && query.types.count (item.type) == 0)
            return false;
        if (item.rating < query.minimumRating)
            return false;
        if (folderPath.isNotEmpty() && ! inFolder (parents[(size_t) index], folderPath, query.subfolders))
            return false;
        for (const auto& filter : query.attributes)
            if (! passesFilter (index, filter))
                return false;
        return search.isEmpty() || search.matches (searchTextOf (item));
    }

    bool MediaDatabase::passesBase (int index, const MediaQuery& query, const TextSearch& search, const juce::String& folderPath) const
    {
        const auto& item = items[(size_t) index];
        if (! query.types.empty() && query.types.count (item.type) == 0)
            return false;
        if (item.rating < query.minimumRating)
            return false;
        if (folderPath.isNotEmpty() && ! inFolder (parents[(size_t) index], folderPath, query.subfolders))
            return false;
        return search.isEmpty() || search.matches (searchTextOf (item));
    }

    const MediaDatabase::AttributeIds& MediaDatabase::attributeIdsOf (const AttributeId& attribute) const
    {
        // (0.190.0) Each item's values of an attribute as numbers into one list of the values found, made once
        // per change of the items, their ratings or tags (every count and filter made a list of strings for
        // every item for every column, on every keystroke).
        const auto key = juce::String ((int) attribute.kind) + ":" + (attribute.kind == MediaAttribute::user ? attribute.user.toLowerCase() : juce::String());
        auto& ids = attributeIds[key];
        if (ids.made && ids.generation == generation && ids.count == items.size())
            return ids;
        ids = {};
        ids.made = true;
        ids.generation = generation;
        ids.count = items.size();
        ids.offsets.reserve (items.size() + 1);
        std::unordered_map<juce::String, int> place;
        for (const auto& item : items)
        {
            ids.offsets.push_back ((int) ids.ids.size());
            for (const auto& v : attributeValues (item, attribute))
            {
                auto found = place.find (v);
                if (found == place.end())
                {
                    found = place.emplace (v, ids.values.size()).first;
                    ids.values.add (v);
                }
                ids.ids.push_back (found->second);
            }
        }
        ids.offsets.push_back ((int) ids.ids.size());
        return ids;
    }

    bool MediaDatabase::passesFilter (int index, const AttributeFilter& filter) const
    {
        if (filter.values.isEmpty())
            return true;
        const auto& ids = attributeIdsOf (filter.attribute);
        const auto from = ids.offsets[(size_t) index], to = ids.offsets[(size_t) index + 1];
        const auto has = [&] (const juce::String& v)
        {
            for (auto k = from; k < to; ++k)
                if (ids.values[ids.ids[(size_t) k]] == v)
                    return true;
            return false;
        };
        if (filter.attribute.kind == MediaAttribute::character)
        {
            for (const auto& v : filter.values)
                if (! has (v))
                    return false;
            return true;
        }
        for (const auto& v : filter.values)
            if (has (v))
                return true;
        return false;
    }

    const juce::String& MediaDatabase::searchTextOf (const MediaItem& item) const
    {
        // (0.57.2) Made once per item (it was made for every item on every keystroke). (0.163.0) Kept beside the
        // items by place (it was a map by path: a path made and looked up for every item on every keystroke), and
        // forgotten only for an item whose tags change.
        const auto index = (int) (&item - items.data());
        if (! juce::isPositiveAndBelow (index, (int) items.size()) || index >= (int) searchMade.size())
        {
            static thread_local juce::String text;
            text = item.searchText();
            return text;
        }
        if (! searchMade[(size_t) index])
        {
            searchTexts[(size_t) index] = item.searchText();
            searchMade[(size_t) index] = 1;
        }
        return searchTexts[(size_t) index];
    }

    std::vector<std::pair<juce::String, int>> MediaDatabase::valueCounts (const MediaQuery& query, const AttributeId& attribute) const
    {
        return valueCountsAll (query, { attribute }).front();
    }

    std::vector<std::vector<std::pair<juce::String, int>>> MediaDatabase::valueCountsAll (const MediaQuery& query,
                                                                                         const std::vector<AttributeId>& columns) const
    {
        // (0.190.0) Every column's counts in one walk over the items: an item that passes every filter counts in
        // every column; one that fails only filters of a column's own attribute counts in that column alone (as
        // the query without that column's filters finds it); the rest in none. The same counts as a walk per column.
        std::vector<std::vector<int>> counts;
        std::vector<const AttributeIds*> columnIds;
        for (const auto& attribute : columns)
        {
            columnIds.push_back (&attributeIdsOf (attribute));
            counts.emplace_back (columnIds.back()->values.size(), 0);
        }
        const TextSearch search (query.text);
        const auto folderPath = query.folder == juce::File() ? juce::String() : query.folder.getFullPathName();
        std::vector<int> failing;
        for (int i = 0; i < (int) items.size(); ++i)
        {
            if (! passesBase (i, query, search, folderPath))
                continue;
            failing.clear();
            for (int f = 0; f < (int) query.attributes.size(); ++f)
                if (! passesFilter (i, query.attributes[(size_t) f]))
                    failing.push_back (f);
            for (size_t col = 0; col < columns.size(); ++col)
            {
                auto counted = true;
                for (const auto f : failing)
                    counted = counted && query.attributes[(size_t) f].attribute == columns[col];
                if (! counted)
                    continue;
                const auto& ids = *columnIds[col];
                for (auto k = ids.offsets[(size_t) i]; k < ids.offsets[(size_t) i + 1]; ++k)
                    ++counts[col][(size_t) ids.ids[(size_t) k]];
            }
        }
        std::vector<std::vector<std::pair<juce::String, int>>> all;
        for (size_t col = 0; col < columns.size(); ++col)
        {
            std::vector<std::pair<juce::String, int>> list;
            for (size_t v = 0; v < counts[col].size(); ++v)
                if (counts[col][v] > 0)
                    list.push_back ({ columnIds[col]->values[(int) v], counts[col][v] });
            std::sort (list.begin(), list.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });   // (as the map gave them)
            const auto numeric = columns[col].kind == MediaAttribute::tempo || columns[col].kind == MediaAttribute::rating;
            std::sort (list.begin(), list.end(), [numeric] (const auto& a, const auto& b)
            {
                if (numeric)
                    return a.first.getDoubleValue() < b.first.getDoubleValue();
                return a.first.compareNatural (b.first) < 0;
            });
            all.push_back (std::move (list));
        }
        return all;
    }

    void MediaDatabase::sort (std::vector<int>& indices, MediaColumn column, bool ascending) const
    {
        // (0.163.0) Names and folders by their places (worked out once), categories and styles by their values'
        // places (a few dozen values, not 120,000 natural-order text comparisons a sort).
        const auto byRank = [&] (const std::vector<int>& rank)
        {
            std::stable_sort (indices.begin(), indices.end(), [&] (int a, int b)
            {
                return ascending ? rank[(size_t) a] < rank[(size_t) b] : rank[(size_t) a] > rank[(size_t) b];
            });
        };
        if (column == MediaColumn::name)
            return byRank (nameRanks());
        if (column == MediaColumn::folder)
            return byRank (folderRanks());
        if (column == MediaColumn::category || column == MediaColumn::style)
        {
            const auto pairOf = [column] (const MediaItem& m)
            {
                return column == MediaColumn::category ? std::pair { m.tags.category, m.tags.subCategory } : std::pair { m.tags.style, m.tags.subStyle };
            };
            std::vector<std::pair<juce::String, juce::String>> values;
            for (const auto i : indices)
                values.push_back (pairOf (items[(size_t) i]));
            std::sort (values.begin(), values.end());
            values.erase (std::unique (values.begin(), values.end()), values.end());
            std::stable_sort (values.begin(), values.end(), [] (const auto& a, const auto& b)
            {
                const auto c = a.first.compareNatural (b.first);
                return c != 0 ? c < 0 : a.second.compareNatural (b.second) < 0;
            });
            std::map<std::pair<juce::String, juce::String>, int> rankOf;
            for (int r = 0; r < (int) values.size(); ++r)
                rankOf[values[(size_t) r]] = r;
            std::vector<int> rank (items.size(), 0);
            for (const auto i : indices)
                rank[(size_t) i] = rankOf[pairOf (items[(size_t) i])];
            return byRank (rank);
        }
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
                case MediaColumn::tempo: return a.tempo() < b.tempo() ? -1 : a.tempo() > b.tempo() ? 1 : 0;
                case MediaColumn::key: return a.keyName().compare (b.keyName());
                case MediaColumn::category:
                {
                    const auto c = a.tags.category.compareNatural (b.tags.category);
                    return c != 0 ? c : a.tags.subCategory.compareNatural (b.tags.subCategory);
                }
                case MediaColumn::style:
                {
                    const auto c = a.tags.style.compareNatural (b.tags.style);
                    return c != 0 ? c : a.tags.subStyle.compareNatural (b.tags.subStyle);
                }
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
        setRatings ({ file }, stars);
    }

    void MediaDatabase::setRatings (const juce::Array<juce::File>& files, int stars)
    {
        stars = juce::jlimit (0, 5, stars);
        {
            const juce::ScopedLock sl (pendingLock);
            for (const auto& file : files)
            {
                if (stars == 0)
                    ratings.erase (file.getFullPathName());
                else
                    ratings[file.getFullPathName()] = stars;
                if (const auto i = indexOf (file); i >= 0)
                    items[(size_t) i].rating = stars;
            }
        }
        saveSoon();   // (0.57.1: a rating or tag: written once things go quiet)
    }

    int MediaDatabase::getRating (const juce::File& file) const
    {
        const auto found = ratings.find (file.getFullPathName());
        return found != ratings.end() ? found->second : 0;
    }

    //==========================================================================
    MediaTags MediaDatabase::getTags (const juce::File& file) const
    {
        const auto found = tags.find (file.getFullPathName());
        return found != tags.end() ? found->second : MediaTags {};
    }

    void MediaDatabase::editTags (const juce::Array<juce::File>& files, const std::function<void (MediaTags&)>& edit)
    {
        if (files.isEmpty())
            return;
        {
            const juce::ScopedLock sl (pendingLock);   // (a scan in another thread reads `items`)
            for (const auto& file : files)
            {
                auto t = getTags (file);
                edit (t);
                if (t.isEmpty())
                    tags.erase (file.getFullPathName());
                else
                    tags[file.getFullPathName()] = t;
                if (const auto i = indexOf (file); i >= 0)
                {
                    items[(size_t) i].tags = t;
                    invalidateSearch (i);
                }
            }
        }
        saveSoon();   // (0.57.1: a rating or tag: written once things go quiet)
    }

    namespace
    {
        // The sub categories (or styles) a list gives a name, from ours and yours.
        juce::StringArray subsIn (const std::vector<AttributeList>& lists, const juce::String& name)
        {
            juce::StringArray subs;
            for (const auto& list : lists)
                if (list.name.equalsIgnoreCase (name))
                    subs.addArray (list.subs);
            subs.removeDuplicates (true);
            return subs;
        }

        std::vector<AttributeList> merged (const std::vector<AttributeList>& ours, const std::vector<AttributeList>& yours)
        {
            auto all = ours;
            for (const auto& list : yours)
            {
                auto found = std::find_if (all.begin(), all.end(), [&] (const AttributeList& l) { return l.name.equalsIgnoreCase (list.name); });
                if (found == all.end())
                    all.push_back (list);
                else
                    for (const auto& sub : list.subs)
                        found->subs.addIfNotAlreadyThere (sub, true);
            }
            return all;
        }

        bool addTo (std::vector<AttributeList>& yours, const std::vector<AttributeList>& all, const juce::String& name, const juce::String& sub)
        {
            const auto n = name.trim(), s = sub.trim();
            if (n.isEmpty())
                return false;
            const auto known = std::find_if (all.begin(), all.end(), [&] (const AttributeList& l) { return l.name.equalsIgnoreCase (n); });
            if (known != all.end() && (s.isEmpty() || known->subs.contains (s, true)))
                return false;
            auto mine = std::find_if (yours.begin(), yours.end(), [&] (const AttributeList& l) { return l.name.equalsIgnoreCase (n); });
            if (mine == yours.end())
            {
                yours.push_back ({ known != all.end() ? known->name : n, {} });
                mine = yours.end() - 1;
            }
            if (s.isNotEmpty())
                mine->subs.addIfNotAlreadyThere (s, true);
            return true;
        }

        juce::String tempoText (double bpm)
        {
            return bpm > 0.0 ? juce::String (bpm, bpm == std::round (bpm) ? 0 : 1) : juce::String();
        }
    }

    std::vector<AttributeList> MediaDatabase::getCategories() const { return merged (defaultCategories(), userCategories); }
    std::vector<AttributeList> MediaDatabase::getStyles() const { return merged (defaultStyles(), userStyles); }

    juce::StringArray MediaDatabase::subsOf (const AttributeId& list, const juce::String& name) const
    {
        return subsIn (list.kind == MediaAttribute::style || list.kind == MediaAttribute::subStyle ? getStyles() : getCategories(), name);
    }

    bool MediaDatabase::addCategory (const juce::String& name, const juce::String& sub)
    {
        if (! addTo (userCategories, getCategories(), name, sub))
            return false;
        save();
        return true;
    }

    bool MediaDatabase::addStyle (const juce::String& name, const juce::String& sub)
    {
        if (! addTo (userStyles, getStyles(), name, sub))
            return false;
        save();
        return true;
    }

    const UserAttribute* MediaDatabase::findUserAttribute (const juce::String& name) const
    {
        for (const auto& a : userAttributes)
            if (a.name.equalsIgnoreCase (name))
                return &a;
        return nullptr;
    }

    bool MediaDatabase::addUserAttribute (const UserAttribute& attribute)
    {
        auto a = attribute;
        a.name = a.name.trim();
        a.precision = juce::jlimit (0, 6, a.precision);
        if (a.name.isEmpty() || findUserAttribute (a.name) != nullptr)
            return false;
        // (Not one of ours by another name: the filter's and inspector's titles must tell them apart.)
        for (const auto kind : { MediaAttribute::category, MediaAttribute::subCategory, MediaAttribute::style, MediaAttribute::subStyle,
                                 MediaAttribute::character, MediaAttribute::tempo, MediaAttribute::key, MediaAttribute::rating, MediaAttribute::comment })
            if (AttributeId { kind, {} }.name().equalsIgnoreCase (a.name))
                return false;
        userAttributes.push_back (a);
        save();
        return true;
    }

    void MediaDatabase::removeUserAttribute (const juce::String& name)
    {
        const auto* found = findUserAttribute (name);
        if (found == nullptr)
            return;
        const auto exact = found->name;
        userAttributes.erase (std::remove_if (userAttributes.begin(), userAttributes.end(), [&] (const UserAttribute& a) { return a.name == exact; }),
                              userAttributes.end());
        juce::Array<juce::File> files;
        for (const auto& [path, t] : tags)
            if (t.user.count (exact) > 0)
                files.add (juce::File (path));
        editTags (files, [&] (MediaTags& t) { t.user.erase (exact); });
        save();
    }

    void MediaDatabase::setAttribute (const juce::Array<juce::File>& files, const AttributeId& attribute, const juce::String& value)
    {
        const auto v = value.trim();
        if (v.isEmpty())
            return removeAttribute (files, attribute);
        if (attribute.kind == MediaAttribute::rating)
            return setRatings (files, v.getIntValue());
        if (attribute.kind == MediaAttribute::character)
        {
            for (int pair = 0; pair < (int) characterPairs().size(); ++pair)
            {
                const auto& p = characterPairs()[(size_t) pair];
                if (v.equalsIgnoreCase (p.left)) return setCharacter (files, pair, -1);
                if (v.equalsIgnoreCase (p.right)) return setCharacter (files, pair, 1);
            }
            return;
        }
        if (attribute.kind == MediaAttribute::user)
        {
            const auto* user = findUserAttribute (attribute.user);
            if (user == nullptr)
                return;
            auto stored = v;
            if (user->type == UserAttributeType::number)
                stored = user->precision > 0 ? juce::String (v.getDoubleValue(), user->precision)   // (0 decimals in JUCE means "all of them")
                                             : juce::String (juce::roundToInt (v.getDoubleValue()));
            else if (user->type == UserAttributeType::yesNo)
                stored = (v == "1" || v.equalsIgnoreCase ("yes") || v.equalsIgnoreCase ("on")) ? "1" : "0";
            const auto name = user->name;
            return editTags (files, [&] (MediaTags& t) { t.user[name] = stored; });
        }
        const auto categories = getCategories();
        const auto styles = getStyles();
        editTags (files, [&] (MediaTags& t)
        {
            switch (attribute.kind)
            {
                case MediaAttribute::category:
                    if (! t.category.equalsIgnoreCase (v) && ! subsIn (categories, v).contains (t.subCategory, true))
                        t.subCategory = {};
                    t.category = v;
                    break;
                case MediaAttribute::subCategory: t.subCategory = v; break;
                case MediaAttribute::style:
                    if (! t.style.equalsIgnoreCase (v) && ! subsIn (styles, v).contains (t.subStyle, true))
                        t.subStyle = {};
                    t.style = v;
                    break;
                case MediaAttribute::subStyle: t.subStyle = v; break;
                case MediaAttribute::tempo:
                {
                    const auto bpm = v.getDoubleValue();
                    if (bpm >= 20.0 && bpm <= 400.0)
                        t.bpm = bpm;
                    break;
                }
                case MediaAttribute::key: t.key = v; break;
                case MediaAttribute::comment: t.comment = v; break;
                default: break;
            }
        });
    }

    void MediaDatabase::removeAttribute (const juce::Array<juce::File>& files, const AttributeId& attribute)
    {
        if (attribute.kind == MediaAttribute::rating)
            return setRatings (files, 0);
        editTags (files, [&] (MediaTags& t)
        {
            switch (attribute.kind)
            {
                case MediaAttribute::category: t.category = {}; t.subCategory = {}; break;
                case MediaAttribute::subCategory: t.subCategory = {}; break;
                case MediaAttribute::style: t.style = {}; t.subStyle = {}; break;
                case MediaAttribute::subStyle: t.subStyle = {}; break;
                case MediaAttribute::character: t.character.clear(); break;
                case MediaAttribute::tempo: t.bpm = 0.0; break;
                case MediaAttribute::key: t.key = {}; break;
                case MediaAttribute::comment: t.comment = {}; break;
                case MediaAttribute::user:
                    for (auto it = t.user.begin(); it != t.user.end();)
                        it = it->first.equalsIgnoreCase (attribute.user) ? t.user.erase (it) : std::next (it);
                    break;
                default: break;
            }
        });
    }

    void MediaDatabase::setCharacter (const juce::Array<juce::File>& files, int pair, int side)
    {
        if (! juce::isPositiveAndBelow (pair, (int) characterPairs().size()))
            return;
        const auto& p = characterPairs()[(size_t) pair];
        editTags (files, [&] (MediaTags& t)
        {
            t.character.removeString (p.left);
            t.character.removeString (p.right);
            if (side != 0)
                t.character.add (side < 0 ? p.left : p.right);
            // (In the pairs' order, so every file's reads the same.)
            juce::StringArray ordered;
            for (const auto& q : characterPairs())
                for (const auto& word : { q.left, q.right })
                    if (t.character.contains (word))
                        ordered.add (word);
            t.character = ordered;
        });
    }

    juce::StringArray MediaDatabase::attributeValues (const MediaItem& item, const AttributeId& attribute)
    {
        const auto one = [] (const juce::String& s) { return s.isEmpty() ? juce::StringArray() : juce::StringArray (s); };
        const auto& t = item.tags;
        switch (attribute.kind)
        {
            case MediaAttribute::category: return one (t.category);
            case MediaAttribute::subCategory: return one (t.subCategory);
            case MediaAttribute::style: return one (t.style);
            case MediaAttribute::subStyle: return one (t.subStyle);
            case MediaAttribute::character: return t.character;
            case MediaAttribute::tempo: return one (tempoText (item.tempo()));
            case MediaAttribute::key: return one (item.keyName());
            case MediaAttribute::rating: return item.rating > 0 ? juce::StringArray (juce::String (item.rating)) : juce::StringArray();
            case MediaAttribute::comment: return one (t.comment);
            case MediaAttribute::user:
                for (const auto& [name, value] : t.user)
                    if (name.equalsIgnoreCase (attribute.user))
                        return one (value);
                return {};
        }
        return {};
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
        for (const auto* e : xml->getChildWithTagNameIterator ("Exclude"))
            exclusions.addIfNotAlreadyThere (e->getStringAttribute ("path"));
        for (const auto* e : xml->getChildWithTagNameIterator ("Rating"))
            ratings[e->getStringAttribute ("path")] = juce::jlimit (0, 5, e->getIntAttribute ("stars"));
        for (const auto* e : xml->getChildWithTagNameIterator ("Tags"))
        {
            MediaTags t;
            t.category = e->getStringAttribute ("category");
            t.subCategory = e->getStringAttribute ("subCategory");
            t.style = e->getStringAttribute ("style");
            t.subStyle = e->getStringAttribute ("subStyle");
            t.comment = e->getStringAttribute ("comment");
            t.key = e->getStringAttribute ("key");
            t.bpm = e->getDoubleAttribute ("bpm");
            t.character = juce::StringArray::fromTokens (e->getStringAttribute ("character"), ",", "");
            t.character.removeEmptyStrings();
            for (const auto* u : e->getChildWithTagNameIterator ("User"))
                t.user[u->getStringAttribute ("name")] = u->getStringAttribute ("value");
            if (! t.isEmpty())
                tags[e->getStringAttribute ("path")] = t;
        }
        const auto readLists = [&] (const char* tag, std::vector<AttributeList>& into)
        {
            for (const auto* e : xml->getChildWithTagNameIterator (tag))
            {
                AttributeList list { e->getStringAttribute ("name"), {} };
                for (const auto* sub : e->getChildWithTagNameIterator ("Sub"))
                    list.subs.add (sub->getStringAttribute ("name"));
                if (list.name.isNotEmpty())
                    into.push_back (list);
            }
        };
        readLists ("UserCategory", userCategories);
        readLists ("UserStyle", userStyles);
        for (const auto* e : xml->getChildWithTagNameIterator ("UserAttribute"))
        {
            UserAttribute a;
            a.name = e->getStringAttribute ("name");
            const auto type = e->getStringAttribute ("type");
            a.type = type == "number" ? UserAttributeType::number : type == "yesNo" ? UserAttributeType::yesNo : UserAttributeType::text;
            a.precision = juce::jlimit (0, 6, e->getIntAttribute ("precision"));
            if (a.name.isNotEmpty() && findUserAttribute (a.name) == nullptr)
                userAttributes.push_back (a);
        }
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
            item.info = e->getStringAttribute ("info");
            item.presetClass = e->getStringAttribute ("class");
            item.presetPlugin = e->getStringAttribute ("plugin");
            item.presetInstrument = e->getBoolAttribute ("instrument");
            if (item.type == MediaType::pluginPreset)
                item.name = e->getStringAttribute ("name", item.name);
            item.readVersion = e->getIntAttribute ("read", 1);
            item.rating = getRating (item.file);
            item.tags = getTags (item.file);
            items.push_back (item);
        }
    }

    namespace
    {
        // (0.163.0) What a save writes: the database's own lists (a save now), or copies of them (a save on its thread).
        template <typename Source>
        std::unique_ptr<juce::XmlElement> buildMediaXml (const Source& src)
    {
            // (0.163.0) The children gathered, then put in back to front: JUCE's createNewChildElement walks to the end
            // of the list for every child, so 120,000 items took four minutes to save (and a save came after every
            // scan and every rating).
            auto xml = std::make_unique<juce::XmlElement> ("MediaBay");
            std::vector<juce::XmlElement*> children;
            children.reserve (src.items.size() + src.ratings.size() + src.tags.size() + src.locations.size() + 64);
            struct Adder
            {
                std::vector<juce::XmlElement*>& to;
                juce::XmlElement* createNewChildElement (const char* tag) { to.push_back (new juce::XmlElement (tag)); return to.back(); }
            } adder { children };
            auto* to = &adder;
            for (const auto& location : src.locations)
                to->createNewChildElement ("Location")->setAttribute ("path", location);
            for (const auto& folder : src.exclusions)
                to->createNewChildElement ("Exclude")->setAttribute ("path", folder);
            for (const auto& [path, stars] : src.ratings)
            {
                auto* e = to->createNewChildElement ("Rating");
                e->setAttribute ("path", path);
                e->setAttribute ("stars", stars);
            }
            for (const auto& [path, t] : src.tags)
            {
                auto* e = to->createNewChildElement ("Tags");
                e->setAttribute ("path", path);
                const auto put = [e] (const char* name, const juce::String& v) { if (v.isNotEmpty()) e->setAttribute (name, v); };
                put ("category", t.category);
                put ("subCategory", t.subCategory);
                put ("style", t.style);
                put ("subStyle", t.subStyle);
                put ("comment", t.comment);
                put ("key", t.key);
                if (t.bpm > 0.0) e->setAttribute ("bpm", t.bpm);
                put ("character", t.character.joinIntoString (","));
                for (const auto& [name, value] : t.user)
                {
                    auto* u = e->createNewChildElement ("User");
                    u->setAttribute ("name", name);
                    u->setAttribute ("value", value);
                }
            }
            const auto writeLists = [&] (const char* tag, const std::vector<AttributeList>& lists)
            {
                for (const auto& list : lists)
                {
                    auto* e = to->createNewChildElement (tag);
                    e->setAttribute ("name", list.name);
                    for (const auto& sub : list.subs)
                        e->createNewChildElement ("Sub")->setAttribute ("name", sub);
                }
            };
            writeLists ("UserCategory", src.userCategories);
            writeLists ("UserStyle", src.userStyles);
            for (const auto& a : src.userAttributes)
            {
                auto* e = to->createNewChildElement ("UserAttribute");
                e->setAttribute ("name", a.name);
                e->setAttribute ("type", a.type == UserAttributeType::number ? "number" : a.type == UserAttributeType::yesNo ? "yesNo" : "text");
                if (a.precision > 0) e->setAttribute ("precision", a.precision);
            }
            for (const auto& item : src.items)
            {
                auto* e = to->createNewChildElement ("Item");
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
                if (item.info.isNotEmpty()) e->setAttribute ("info", item.info);
                if (item.presetClass.isNotEmpty()) e->setAttribute ("class", item.presetClass);
                if (item.presetPlugin.isNotEmpty()) e->setAttribute ("plugin", item.presetPlugin);
                if (item.presetInstrument) e->setAttribute ("instrument", true);
                if (item.type == MediaType::pluginPreset && item.name != item.file.getFileNameWithoutExtension()) e->setAttribute ("name", item.name);
                e->setAttribute ("read", item.readVersion);
            }
            for (auto i = children.rbegin(); i != children.rend(); ++i)
                xml->prependChildElement (*i);
            return xml;
        }


        struct MediaSaveCopy
        {
            juce::StringArray locations, exclusions;
            std::vector<MediaItem> items;
            std::map<juce::String, int> ratings;
            std::map<juce::String, MediaTags> tags;
            std::vector<AttributeList> userCategories, userStyles;
            std::vector<UserAttribute> userAttributes;
        };
    }

    std::unique_ptr<juce::XmlElement> MediaDatabase::makeXml() const
    {
        // (The private lists through references: a template outside the class can't read them.)
        struct Lists
        {
            const juce::StringArray& locations; const juce::StringArray& exclusions;
            const std::vector<MediaItem>& items; const std::map<juce::String, int>& ratings;
            const std::map<juce::String, MediaTags>& tags;
            const std::vector<AttributeList>& userCategories; const std::vector<AttributeList>& userStyles;
            const std::vector<UserAttribute>& userAttributes;
        };
        return buildMediaXml (Lists { locations, exclusions, items, ratings, tags, userCategories, userStyles, userAttributes });
    }

    void MediaDatabase::save() const
    {
        waitForSave();
        unsaved = false;
        if (store == juce::File())
            return;
        const auto xml = makeXml();
        store.getParentDirectory().createDirectory();
        xml->writeTo (store);
    }

    void MediaDatabase::saveInBackground() const
    {
        // (0.163.0) Made here (the items can change after), written on a thread of its own.
        waitForSave();
        unsaved = false;
        if (store == juce::File())
            return;
        // (Copied here: building the XML of 120,000 files took half a second, now on the save's thread.)
        auto copy = std::make_shared<MediaSaveCopy> (MediaSaveCopy { locations, exclusions, items, ratings, tags,
                                                                     userCategories, userStyles, userAttributes });
        const auto file = store;
        saving = std::async (std::launch::async, [copy, file]
        {
            const auto xml = buildMediaXml (*copy);
            file.getParentDirectory().createDirectory();
            xml->writeTo (file);
        });
    }

    void MediaDatabase::waitForSave() const
    {
        if (saving.valid())
            saving.wait();
    }
}
