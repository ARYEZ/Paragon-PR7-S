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

    MediaTags MediaDatabase::tagsFromIXml (const juce::String& ixml, double& bpm, juce::String& key)
    {
        // <BWFXML><STEINBERG><ATTR_LIST><ATTR><TYPE/><NAME/><VALUE/></ATTR>... The names aren't
        // published: they're matched by what they hold (our best reading of them).
        MediaTags t;
        const auto xml = juce::parseXML (ixml);
        const auto* steinberg = xml != nullptr ? xml->getChildByName ("STEINBERG") : nullptr;
        const auto* list = steinberg != nullptr ? steinberg->getChildByName ("ATTR_LIST") : nullptr;
        if (list == nullptr)
            return t;
        // ("Drum&Perc | Kick": the part after the bar.)
        const auto lastPart = [] (const juce::String& v) { return v.fromLastOccurrenceOf ("|", false, false).trim(); };
        for (const auto* attr : list->getChildWithTagNameIterator ("ATTR"))
        {
            const auto name = attr->getChildElementAllSubText ("NAME", {}).toLowerCase();
            const auto value = attr->getChildElementAllSubText ("VALUE", {}).trim();
            if (value.isEmpty())
                continue;
            const auto sub = name.contains ("sub");
            if (name.contains ("category"))
                (sub ? t.subCategory : t.category) = lastPart (value);
            else if (name.contains ("style"))
                (sub ? t.subStyle : t.style) = lastPart (value);
            else if (name.contains ("character"))
            {
                for (auto word : juce::StringArray::fromTokens (value, ",|;", ""))
                    for (const auto& pair : characterPairs())
                        for (const auto& side : { pair.left, pair.right })
                            if (word.trim().equalsIgnoreCase (side))
                                t.character.addIfNotAlreadyThere (side);
            }
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
                item.info = info.joinIntoString (" ").substring (0, 400);
            }
            // The other DAW's MediaBay tags, in the WAV's iXML.
            if (file.hasFileExtension ("wav;wave;bwf"))
                if (const auto ixml = readIXml (file); ixml.containsIgnoreCase ("STEINBERG"))
                {
                    double bpm = 0.0;
                    juce::String key;
                    item.fileTags = tagsFromIXml (ixml, bpm, key);
                    if (bpm > 0.0 && item.bpm <= 0.0)
                        item.bpm = bpm;
                    if (key.isNotEmpty() && item.key.isEmpty())
                        item.key = key;
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
        for (int i = 0; i < (int) items.size(); ++i)
            if (passes (items[(size_t) i], query, search))
                found.push_back (i);
        return found;
    }

    bool MediaDatabase::passes (const MediaItem& item, const MediaQuery& query, const TextSearch& search) const
    {
        if (! query.types.empty() && query.types.count (item.type) == 0)
            return false;
        if (item.rating < query.minimumRating)
            return false;
        if (const auto& folder = query.folder; folder != juce::File())
        {
            const auto parent = item.file.getParentDirectory();
            if (query.subfolders ? ! (parent == folder || item.file.isAChildOf (folder)) : parent != folder)
                return false;
        }
        for (const auto& filter : query.attributes)
        {
            if (filter.values.isEmpty())
                continue;
            const auto has = attributeValues (item, filter.attribute);
            if (filter.attribute.kind == MediaAttribute::character)
            {
                for (const auto& v : filter.values)
                    if (! has.contains (v))
                        return false;
            }
            else
            {
                bool any = false;
                for (const auto& v : filter.values)
                    any = any || has.contains (v);
                if (! any)
                    return false;
            }
        }
        return search.isEmpty() || search.matches (item.searchText());
    }

    std::vector<std::pair<juce::String, int>> MediaDatabase::valueCounts (const MediaQuery& query, const AttributeId& attribute) const
    {
        auto others = query;
        others.attributes.erase (std::remove_if (others.attributes.begin(), others.attributes.end(),
                                                 [&] (const AttributeFilter& f) { return f.attribute == attribute; }),
                                 others.attributes.end());
        const TextSearch search (others.text);
        std::map<juce::String, int> counts;
        for (const auto& item : items)
            if (passes (item, others, search))
                for (const auto& v : attributeValues (item, attribute))
                    ++counts[v];
        std::vector<std::pair<juce::String, int>> list (counts.begin(), counts.end());
        const auto numeric = attribute.kind == MediaAttribute::tempo || attribute.kind == MediaAttribute::rating;
        std::sort (list.begin(), list.end(), [numeric] (const auto& a, const auto& b)
        {
            if (numeric)
                return a.first.getDoubleValue() < b.first.getDoubleValue();
            return a.first.compareNatural (b.first) < 0;
        });
        return list;
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
                for (auto& item : items)
                    if (item.file == file)
                        item.rating = stars;
            }
        }
        save();
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
                for (auto& item : items)
                    if (item.file == file)
                        item.tags = t;
            }
        }
        save();
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

    void MediaDatabase::save() const
    {
        if (store == juce::File())
            return;
        juce::XmlElement xml ("MediaBay");
        for (const auto& location : locations)
            xml.createNewChildElement ("Location")->setAttribute ("path", location);
        for (const auto& folder : exclusions)
            xml.createNewChildElement ("Exclude")->setAttribute ("path", folder);
        for (const auto& [path, stars] : ratings)
        {
            auto* e = xml.createNewChildElement ("Rating");
            e->setAttribute ("path", path);
            e->setAttribute ("stars", stars);
        }
        for (const auto& [path, t] : tags)
        {
            auto* e = xml.createNewChildElement ("Tags");
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
                auto* e = xml.createNewChildElement (tag);
                e->setAttribute ("name", list.name);
                for (const auto& sub : list.subs)
                    e->createNewChildElement ("Sub")->setAttribute ("name", sub);
            }
        };
        writeLists ("UserCategory", userCategories);
        writeLists ("UserStyle", userStyles);
        for (const auto& a : userAttributes)
        {
            auto* e = xml.createNewChildElement ("UserAttribute");
            e->setAttribute ("name", a.name);
            e->setAttribute ("type", a.type == UserAttributeType::number ? "number" : a.type == UserAttributeType::yesNo ? "yesNo" : "text");
            if (a.precision > 0) e->setAttribute ("precision", a.precision);
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
            if (item.info.isNotEmpty()) e->setAttribute ("info", item.info);
            if (item.presetClass.isNotEmpty()) e->setAttribute ("class", item.presetClass);
            if (item.presetPlugin.isNotEmpty()) e->setAttribute ("plugin", item.presetPlugin);
            if (item.presetInstrument) e->setAttribute ("instrument", true);
            if (item.type == MediaType::pluginPreset && item.name != item.file.getFileNameWithoutExtension()) e->setAttribute ("name", item.name);
            e->setAttribute ("read", item.readVersion);
        }
        store.getParentDirectory().createDirectory();
        xml.writeTo (store);
    }
}
