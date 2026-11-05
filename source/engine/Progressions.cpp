#include "engine/Progressions.h"

namespace pr7s
{
    namespace
    {
        int wrap (int note) { return ((note % 12) + 12) % 12; }

        // The mode's degrees from its tonic.
        int degreeOffset (int degree, bool minorMode)
        {
            static const int major[7] { 0, 2, 4, 5, 7, 9, 11 };
            static const int minor[7] { 0, 2, 3, 5, 7, 8, 10 };
            return (minorMode ? minor : major)[juce::jlimit (0, 6, degree)];
        }

        // "vii" -> 6, "IV" -> 3 (case ignored); the characters read; -1: none.
        int readRoman (const juce::String& text, int& at)
        {
            static const char* const romans[] { "vii", "iii", "vi", "iv", "ii", "v", "i" };
            static const int degrees[] { 6, 2, 5, 3, 1, 4, 0 };
            const auto lower = text.substring (at).toLowerCase();
            for (int i = 0; i < 7; ++i)
                if (lower.startsWith (romans[i]))
                {
                    at += (int) std::strlen (romans[i]);
                    return degrees[i];
                }
            return -1;
        }

        // A leading b or # (several allowed); the half steps.
        int readAccidentals (const juce::String& text, int& at)
        {
            auto shift = 0;
            while (at < text.length() && (text[at] == 'b' || text[at] == '#'))
                shift += text[at++] == 'b' ? -1 : 1;
            return shift;
        }

        struct Entry { const char* name; const char* genre; const char* moods; bool minor; const char* numerals; };
        constexpr bool M = false, m = true;

        // Our library. Genres and moods are ours; a progression may sit in two moods.
        const Entry entries[] {
            // POP
            { "Four-chord anthem",      "POP", "UPLIFTING|HAPPY",   M, "I V vi IV" },
            { "From the sixth",         "POP", "SAD|NOSTALGIC",     M, "vi IV I V" },
            { "Doo-wop",                "POP", "NOSTALGIC|ROMANTIC",M, "I vi IV V" },
            { "Bright lift",            "POP", "HAPPY",             M, "I IV vi V" },
            { "Summer drive",           "POP", "HAPPY|CHILL",       M, "I iii IV V" },
            { "Heartbreak ballad",      "POP", "SAD|ROMANTIC",      M, "I V/7 vi IV" },
            { "Falling bass",           "POP", "NOSTALGIC|ROMANTIC",M, "I V/7 vi I/5 IV I/3 ii V" },
            { "Two-chord sway",         "POP", "CHILL",             M, "I:2 IV:2" },
            { "Open sus",               "POP", "DREAMY",            M, "Isus2 V vi IV" },
            { "Minor pop",              "POP", "SAD|DARK",          m, "i VI III VII" },
            { "Minor swing back",       "POP", "SAD",               m, "i VII VI VII" },
            { "Dance-floor minor",      "POP", "TENSE",             m, "i iv VI V" },
            { "Climbing road",          "POP", "NOSTALGIC|UPLIFTING",M,"IV V iii vi" },
            { "Chorus lift",            "POP", "UPLIFTING",         M, "ii IV I V" },
            { "Bridge build",           "POP", "TENSE",             M, "vi V IV V" },
            { "Borrowed sadness",       "POP", "NOSTALGIC|SAD",     M, "I IV iv I" },
            { "Bittersweet",            "POP", "NOSTALGIC",         M, "I iii vi IV" },
            { "Pre-chorus climb",       "POP", "UPLIFTING|TENSE",   M, "IV:0.5 V:0.5 vi:0.5 V:0.5 IV:2" },
            { "Minor hook",             "POP", "DARK|ROMANTIC",     m, "i III VII iv" },
            { "Happy-sad",              "POP", "NOSTALGIC|HAPPY",   M, "I III vi IV" },

            // ROCK
            { "Three-chord rock",       "ROCK", "HAPPY",            M, "I IV V IV" },
            { "Mixolydian anthem",      "ROCK", "EPIC|HAPPY",       M, "I bVII IV I" },
            { "Classic turn",           "ROCK", "HAPPY",            M, "I bIII IV I" },
            { "Power ballad",           "ROCK", "ROMANTIC|EPIC",    M, "I iii IV V" },
            { "Grunge",                 "ROCK", "DARK",             m, "i III VI iv" },
            { "Minor rock loop",        "ROCK", "DARK|TENSE",       m, "i VI VII i" },
            { "Wide open",              "ROCK", "EPIC",             M, "I bVII bVI bVII" },
            { "Indie shimmer",          "ROCK", "DREAMY",           M, "Imaj7 IVmaj7 vi V" },
            { "Punk push",              "ROCK", "HAPPY|TENSE",      M, "I IV I V" },
            { "Garage",                 "ROCK", "HAPPY",            M, "I bIII bVII IV" },
            { "Stadium",                "ROCK", "EPIC|UPLIFTING",   M, "vi IV I V" },
            { "Riff in minor",          "ROCK", "TENSE",            m, "i VII VI VII i:2" },
            { "Alt-rock sigh",          "ROCK", "SAD|DREAMY",       M, "I iii IV iv" },
            { "Southern",               "ROCK", "HAPPY|CHILL",      M, "I bVII IV IV" },

            // METAL (7-string friendly: minor, Phrygian and harmonic minor)
            { "Epic metal",             "METAL", "EPIC|DARK",       m, "i VI VII i" },
            { "Phrygian chug",          "METAL", "DARK|TENSE",      m, "i bII i bII" },
            { "Phrygian fall",          "METAL", "DARK",            m, "i bII VII i" },
            { "Gallop climb",           "METAL", "EPIC",            m, "i VI III VII" },
            { "Dirge",                  "METAL", "DARK|SAD",        m, "i:2 iv:2" },
            { "Tritone dread",          "METAL", "DARK|TENSE",      m, "i bII bV i" },
            { "Harmonic minor riff",    "METAL", "TENSE",           m, "i VI iv V" },
            { "Rising to the tonic",    "METAL", "EPIC|SAD",        m, "VI VII i:2" },
            { "Djent",                  "METAL", "DARK",            m, "i:2 bII VII" },
            { "Doom",                   "METAL", "DARK|SAD",        m, "i:2 VI:2 iv:2 V:2" },
            { "Neoclassical cycle",     "METAL", "EPIC|TENSE",      m, "i iv VII III VI iio V i" },
            { "Power metal",            "METAL", "UPLIFTING|EPIC",  M, "I V vi iii IV I IV V" },
            { "Diminished pull",        "METAL", "TENSE",           m, "i #ivo V i" },
            { "Breakdown",              "METAL", "DARK",            m, "i:2 bII:2" },
            { "Melodic death",          "METAL", "SAD|EPIC",        m, "i VII VI V" },
            { "Black metal tremolo",    "METAL", "DARK",            m, "i VI iv i" },
            { "Thrash",                 "METAL", "TENSE",           m, "i bII VII bII" },
            { "Metalcore chorus",       "METAL", "EPIC|UPLIFTING",  m, "VI III VII i" },

            // BLUES
            { "Twelve-bar blues",       "BLUES", "HAPPY|CHILL",     M, "I7 I7 I7 I7 IV7 IV7 I7 I7 V7 IV7 I7 V7" },
            { "Quick-change blues",     "BLUES", "HAPPY",           M, "I7 IV7 I7 I7 IV7 IV7 I7 I7 V7 IV7 I7 V7" },
            { "Minor blues",            "BLUES", "SAD|DARK",        m, "i7 i7 i7 i7 iv7 iv7 i7 i7 VI7 V7 i7 V7" },
            { "Eight-bar blues",        "BLUES", "NOSTALGIC",       M, "I7 V7 IV7 IV7 I7 V7 I7 V7" },
            { "Slow blues turnaround",  "BLUES", "SAD",             M, "I7 IV7 I7 V7" },
            { "Shuffle vamp",           "BLUES", "CHILL",           M, "I7:2 IV7:2" },
            { "Minor blues vamp",       "BLUES", "DARK|CHILL",      m, "i7:2 iv7:2" },

            // JAZZ
            { "Two-five-one",           "JAZZ", "CHILL|ROMANTIC",   M, "ii7 V7 Imaj7:2" },
            { "Turnaround",             "JAZZ", "CHILL",            M, "Imaj7 vi7 ii7 V7" },
            { "Rhythm turnaround",      "JAZZ", "HAPPY",            M, "Imaj7:0.5 VI7:0.5 ii7:0.5 V7:0.5 iii7:0.5 VI7:0.5 ii7:0.5 V7:0.5" },
            { "Minor two-five-one",     "JAZZ", "DARK|ROMANTIC",    m, "iih7 V7 i7:2" },
            { "Falling leaves cycle",   "JAZZ", "NOSTALGIC|ROMANTIC",M,"ii7 V7 Imaj7 IVmaj7 viih7 III7 vi7:2" },
            { "Backdoor",               "JAZZ", "DREAMY",           M, "iv7 bVII7 Imaj7:2" },
            { "Tritone substitute",     "JAZZ", "CHILL",            M, "ii7 bII7 Imaj7:2" },
            { "Modal vamp",             "JAZZ", "DREAMY|CHILL",     m, "i7:2 IV7:2" },
            { "Jazz ballad",            "JAZZ", "ROMANTIC",         M, "Imaj7 iii7 vi7 ii7 V7 Imaj7" },
            { "Three-six-two-five",     "JAZZ", "HAPPY|CHILL",      M, "iii7 VI7 ii7 V7" },
            { "Lady bird turn",         "JAZZ", "DREAMY",           M, "Imaj7 bIIImaj7 bVImaj7 bIImaj7" },
            { "Minor line cliché",      "JAZZ", "SAD|ROMANTIC",     m, "i i/7 i6 VI" },

            // R&B & SOUL
            { "Neo-soul glide",         "R&B & SOUL", "CHILL|ROMANTIC",   M, "IVmaj7 iii7 ii7 Imaj7" },
            { "Slow jam",               "R&B & SOUL", "ROMANTIC",         M, "Imaj7 iii7 IVmaj7 V9" },
            { "Minor groove",           "R&B & SOUL", "DARK|CHILL",       m, "i9:2 iv7:2" },
            { "Nineties R&B",           "R&B & SOUL", "ROMANTIC|NOSTALGIC",M,"IVmaj7 V7 iii7 vi7" },
            { "Soul passing chord",     "R&B & SOUL", "UPLIFTING",        M, "I I/3 IV #ivo7 I/5 V7 I:2" },
            { "Funk vamp",              "R&B & SOUL", "HAPPY",            m, "i7:2 IV9:2" },
            { "Smooth nines",           "R&B & SOUL", "CHILL",            M, "Imaj9 vi9 ii9 V9" },
            { "Late night",             "R&B & SOUL", "DREAMY|ROMANTIC",  m, "VImaj7 v7 i7:2" },
            { "Soul ballad",            "R&B & SOUL", "SAD|ROMANTIC",     M, "Imaj7 vi7 IVmaj7 iv6" },
            { "Motown bounce",          "R&B & SOUL", "HAPPY",            M, "I vi ii V7" },

            // GOSPEL & WORSHIP
            { "Open heaven",            "GOSPEL & WORSHIP", "UPLIFTING",        M, "IV I V vi" },
            { "Worship ballad",         "GOSPEL & WORSHIP", "ROMANTIC|UPLIFTING",M,"I/3 IV I/5 V" },
            { "Gospel walk-up",         "GOSPEL & WORSHIP", "UPLIFTING|HAPPY",  M, "I iii7 IV #ivo7 I/5 VI7 ii7 V7" },
            { "Amen",                   "GOSPEL & WORSHIP", "UPLIFTING",        M, "IV:2 I:2" },
            { "Church turnaround",      "GOSPEL & WORSHIP", "HAPPY",            M, "iii7 VI7 ii7 V7 I:2" },
            { "Minor praise",           "GOSPEL & WORSHIP", "SAD|UPLIFTING",    m, "iv7 VII7 IIImaj7 VImaj7" },
            { "Worship build",          "GOSPEL & WORSHIP", "UPLIFTING|EPIC",   M, "I V/7 vi V IV I/3 ii V" },
            { "Sunday morning",         "GOSPEL & WORSHIP", "HAPPY",            M, "I I7 IV iv I V7 I:2" },
            { "Altar call",             "GOSPEL & WORSHIP", "ROMANTIC|CHILL",   M, "Imaj7 IV/1 Imaj7 IV/1" },

            // EDM
            { "Festival",               "EDM", "EPIC|UPLIFTING",    m, "i VI III VII" },
            { "Trance lift",            "EDM", "EPIC|UPLIFTING",    m, "VI III VII i" },
            { "Progressive house",      "EDM", "UPLIFTING",         M, "IV V vi vi" },
            { "Deep house",             "EDM", "CHILL|DREAMY",      m, "i7 iv7 i7 VImaj7" },
            { "Future bass",            "EDM", "HAPPY|DREAMY",      M, "IVmaj7 V7 iii7 vi7" },
            { "Dubstep",                "EDM", "DARK",              m, "i:2 VI VII" },
            { "Synthwave",              "EDM", "NOSTALGIC|DREAMY",  m, "VI VII i iv" },
            { "Tropical",               "EDM", "HAPPY|CHILL",       M, "IV vi V I" },
            { "Big room",               "EDM", "TENSE",             m, "i:2 i VII" },
            { "Uplifting trance",       "EDM", "EPIC",              m, "i VI III VII i VI iv V" },
            { "Melodic techno",         "EDM", "DARK|DREAMY",       m, "i:2 VII:2 VI:2 VII:2" },

            // HIP-HOP & TRAP
            { "Dark trap",              "HIP-HOP & TRAP", "DARK|TENSE",  m, "i:2 VI:2" },
            { "Phrygian trap",          "HIP-HOP & TRAP", "DARK",        m, "i i bII VII" },
            { "Boom bap",               "HIP-HOP & TRAP", "CHILL|NOSTALGIC",m,"i9 VImaj7 iv7 v7" },
            { "Soul sample",            "HIP-HOP & TRAP", "NOSTALGIC",   M, "ii7 iii7 IVmaj7 iii7" },
            { "Drill",                  "HIP-HOP & TRAP", "DARK|TENSE",  m, "i VI i V" },
            { "Melodic rap",            "HIP-HOP & TRAP", "SAD",         m, "i VI III VII" },
            { "West coast",             "HIP-HOP & TRAP", "CHILL",       M, "Imaj9:2 IVmaj9:2" },
            { "Emo rap",                "HIP-HOP & TRAP", "SAD|DREAMY",  m, "VI III VII v" },

            // LO-FI
            { "Study session",          "LO-FI", "CHILL|DREAMY",    M, "IVmaj7 iii7 vi7 Imaj7" },
            { "Rainy window",           "LO-FI", "SAD|CHILL",       m, "i9 iv7 VImaj7 V7" },
            { "Bedroom",                "LO-FI", "DREAMY",          M, "Imaj7:2 IVmaj7:2" },
            { "Jazzy lo-fi",            "LO-FI", "CHILL",           M, "ii9 V7 Imaj9 vi7" },
            { "Old tape",               "LO-FI", "NOSTALGIC",       M, "IVmaj7 iv6 Imaj7 vi7" },
            { "Night drive",            "LO-FI", "DREAMY|CHILL",    m, "i7 VImaj7 iv7 VII" },

            // FILM & GAME
            { "Heroic rise",            "FILM & GAME", "EPIC|UPLIFTING",  M, "I bVI bVII I" },
            { "Epic minor",             "FILM & GAME", "EPIC|DARK",       m, "VI VII i:2" },
            { "Chromatic mediants",     "FILM & GAME", "EPIC|DREAMY",     M, "I bVI I bIII" },
            { "Villain",                "FILM & GAME", "DARK|TENSE",      m, "i:2 #iv:2" },
            { "Wonder",                 "FILM & GAME", "DREAMY|UPLIFTING",M, "I III IV iv" },
            { "Adventure",              "FILM & GAME", "HAPPY|EPIC",      M, "I II IV I" },
            { "Sorrow",                 "FILM & GAME", "SAD",             m, "i v VI III iv i iv V" },
            { "Floating in space",      "FILM & GAME", "DREAMY",          M, "I:2 II:2" },
            { "Battle",                 "FILM & GAME", "TENSE|EPIC",      m, "i VI iv V" },
            { "Victory",                "FILM & GAME", "UPLIFTING|HAPPY", M, "I V/7 vi I/5 IV V I:2" },
            { "Hopeful",                "FILM & GAME", "UPLIFTING|ROMANTIC",M,"IV V iii vi ii V I:2" },
            { "Dark fantasy",           "FILM & GAME", "DARK|EPIC",       m, "i III VII iv" },
            { "Lament",                 "FILM & GAME", "SAD|NOSTALGIC",   m, "i v/7 iv/6 V" },
            { "Suspense",               "FILM & GAME", "TENSE",           m, "i:2 bII:2 i:2 V:2" },
            { "Mystery",                "FILM & GAME", "TENSE|DREAMY",    M, "I bIII I bVI" },

            // FOLK & COUNTRY
            { "Country",                "FOLK & COUNTRY", "HAPPY",            M, "I IV I V" },
            { "Country ballad",         "FOLK & COUNTRY", "SAD|NOSTALGIC",    M, "I I7 IV iv" },
            { "Folk",                   "FOLK & COUNTRY", "NOSTALGIC",        M, "vi IV V I" },
            { "Celtic",                 "FOLK & COUNTRY", "EPIC|NOSTALGIC",   m, "i VII IV i" },
            { "Bluegrass",              "FOLK & COUNTRY", "HAPPY",            M, "I:2 IV I V:2 I" },
            { "Americana",              "FOLK & COUNTRY", "NOSTALGIC|CHILL",  M, "I iii IV I" },
            { "Campfire",               "FOLK & COUNTRY", "HAPPY|CHILL",      M, "I V IV I" },
            { "Sea shanty",             "FOLK & COUNTRY", "HAPPY",            m, "i iv i V" },
            { "Train song",             "FOLK & COUNTRY", "HAPPY",            M, "I I IV IV V V I I" },

            // LATIN
            { "Andalusian cadence",     "LATIN", "TENSE|ROMANTIC",  m, "i VII VI V" },
            { "Reggaeton",              "LATIN", "ROMANTIC|DARK",   m, "i iv VI V" },
            { "Bossa nova",             "LATIN", "ROMANTIC|CHILL",  M, "Imaj7:2 II7:2 ii7 bII7 Imaj7:2" },
            { "Montuno",                "LATIN", "HAPPY",           M, "I IV V IV" },
            { "Tango",                  "LATIN", "TENSE|ROMANTIC",  m, "i V7 i iv V7 i" },
            { "Bolero",                 "LATIN", "ROMANTIC|SAD",    m, "i7 iv7 V7 i" },
            { "Cumbia",                 "LATIN", "HAPPY",           M, "I:2 V:2" },
            { "Bachata",                "LATIN", "ROMANTIC|SAD",    m, "i iv V i" },

            // CLASSICAL
            { "The Canon",              "CLASSICAL", "ROMANTIC|NOSTALGIC",M, "I V vi iii IV I IV V" },
            { "Circle of fifths",       "CLASSICAL", "ROMANTIC",          M, "I IV viio iii vi ii V I" },
            { "Perfect cadence",        "CLASSICAL", "UPLIFTING",         M, "I IV V I" },
            { "Lament bass",            "CLASSICAL", "SAD",               m, "i v/7 iv/6 V" },
            { "Romantic minor",         "CLASSICAL", "ROMANTIC|SAD",      m, "i VI iv V7 i" },
            { "Picardy ending",         "CLASSICAL", "UPLIFTING",         m, "i iv V I" },
            { "Neapolitan",             "CLASSICAL", "TENSE|ROMANTIC",    m, "i bII V7 i" },
            { "Deceptive",              "CLASSICAL", "ROMANTIC",          M, "I IV V vi" },
            { "Chorale",                "CLASSICAL", "UPLIFTING|CHILL",   M, "I vi ii V I" },
            { "Minor cycle",            "CLASSICAL", "SAD|EPIC",          m, "i iv VII III VI iio V i" },
        };
    }

    bool parseNumeral (const juce::String& raw, bool minorMode, LibraryProgression::Step& step)
    {
        using T = Chord::Type;
        using X = Chord::Tension;
        auto text = raw.trim();
        step = {};
        if (const auto colon = text.indexOfChar (':'); colon >= 0)
        {
            step.bars = text.substring (colon + 1).getDoubleValue();
            text = text.substring (0, colon);
            if (step.bars <= 0.0)
                return false;
        }
        juce::String bass;
        if (const auto slash = text.indexOfChar ('/'); slash >= 0)
        {
            bass = text.substring (slash + 1);
            text = text.substring (0, slash);
        }
        auto at = 0;
        const auto shift = readAccidentals (text, at);
        const auto upper = at < text.length() && juce::CharacterFunctions::isUpperCase (text[at]);
        const auto degree = readRoman (text, at);
        if (degree < 0)
            return false;
        step.offset = wrap (degreeOffset (degree, minorMode) + shift);
        step.accidental = shift < 0 ? -1 : shift > 0 ? 1 : 0;
        step.numeral = text;
        step.type = upper ? T::major : T::minor;
        auto rest = text.substring (at);
        const auto take = [&rest] (const char* s) { if (! rest.startsWith (s)) return false; rest = rest.substring ((int) std::strlen (s)); return true; };
        if (take ("h7"))          { step.type = T::diminished; step.tension = X::seven; }
        else if (take ("o7"))     { step.type = T::diminished; step.tension = X::dim7; }
        else if (take ("o"))        step.type = T::diminished;
        else if (take ("+"))        step.type = T::augmented;
        else if (take ("sus2"))     step.type = T::sus2;
        else if (take ("sus4"))     step.type = T::sus4;
        if (rest.isNotEmpty() && step.tension == X::none)
        {
            if (take ("maj7"))       step.tension = X::maj7;
            else if (take ("maj9"))  step.tension = X::maj9;
            else if (take ("add9"))  step.tension = X::add9;
            else if (take ("69"))    step.tension = X::sixNine;
            else if (take ("6"))     step.tension = X::six;
            else if (take ("7"))     step.tension = X::seven;
            else if (take ("9"))     step.tension = X::nine;
            else if (take ("11"))    step.tension = X::eleven;
            else if (take ("13"))    step.tension = X::thirteen;
        }
        if (rest.isNotEmpty())
            return false;
        if (bass.isNotEmpty())
        {
            auto b = 0;
            const auto bassShift = readAccidentals (bass, b);
            const auto number = bass.substring (b).getIntValue();
            if (number < 1 || number > 7 || bass.substring (b) != juce::String (number))
                return false;
            step.bassOffset = wrap (degreeOffset (number - 1, minorMode) + bassShift);
            if (step.bassOffset == step.offset)
                step.bassOffset = -1;   // (the root: a plain chord)
        }
        return true;
    }

    int LibraryProgression::tonicIn (const Key& key) const
    {
        const auto tonic = key.pitchClass();
        if (minor == key.minor)
            return tonic;
        return wrap (key.minor ? tonic + 3 : tonic + 9);   // (the relative key)
    }

    std::vector<ProgressionStep> LibraryProgression::in (const Key& key) const
    {
        const auto tonic = tonicIn (key);
        std::vector<ProgressionStep> out;
        for (const auto& s : steps)
            out.push_back ({ Chord { wrap (tonic + s.offset), s.type, s.tension, s.bassOffset < 0 ? -1 : wrap (tonic + s.bassOffset) }, s.bars,
                             s.accidental, minor == key.minor ? s.numeral : juce::String() });
        return out;
    }

    juce::String ProgressionStep::name (const Key& key) const
    {
        return spelling != 0 ? chord.name (spelling < 0) : chord.name (key);
    }

    juce::String ProgressionStep::numeralIn (const Key& key) const
    {
        if (spelling == 0 || numeral.isEmpty())
            return chord.romanName (key);
        // The written one, its tension marks as the key's numeral writes them.
        auto own = chord.romanName (key);
        auto at = 0;
        readAccidentals (numeral, at);
        const auto roman = numeral.substring (0, at) + numeral.substring (at).retainCharacters ("IVXivx");
        auto keyAt = 0;
        readAccidentals (own, keyAt);
        while (keyAt < own.length() && juce::String ("IVXivx").containsChar (own[keyAt]))
            ++keyAt;
        return roman + own.substring (keyAt);
    }

    double LibraryProgression::totalBars() const
    {
        auto total = 0.0;
        for (const auto& s : steps)
            total += s.bars;
        return total;
    }

    const std::vector<LibraryProgression>& progressionLibrary()
    {
        static const auto library = []
        {
            std::vector<LibraryProgression> all;
            for (const auto& e : entries)
            {
                LibraryProgression p;
                p.name = juce::String::fromUTF8 (e.name);
                p.genre = e.genre;
                p.moods = juce::StringArray::fromTokens (e.moods, "|", {});
                p.minor = e.minor;
                p.numerals = e.numerals;
                auto readable = true;
                for (const auto& word : juce::StringArray::fromTokens (p.numerals, " ", {}))
                {
                    LibraryProgression::Step step;
                    readable = readable && parseNumeral (word, p.minor, step);
                    p.steps.push_back (step);
                }
                jassert (readable);   // (the tests read every one)
                if (readable && ! p.steps.empty())
                    all.push_back (std::move (p));
            }
            return all;
        }();
        return library;
    }

    const juce::StringArray& progressionGenres()
    {
        static const juce::StringArray genres { "POP", "ROCK", "METAL", "BLUES", "JAZZ", "R&B & SOUL", "GOSPEL & WORSHIP",
                                                "EDM", "HIP-HOP & TRAP", "LO-FI", "FILM & GAME", "FOLK & COUNTRY", "LATIN", "CLASSICAL" };
        return genres;
    }

    const juce::StringArray& progressionMoods()
    {
        static const juce::StringArray moods { "HAPPY", "SAD", "UPLIFTING", "EPIC", "DARK", "TENSE", "DREAMY", "CHILL", "ROMANTIC", "NOSTALGIC" };
        return moods;
    }

    std::vector<int> findProgressions (const Key& key, const ProgressionFilter& filter)
    {
        const auto words = juce::StringArray::fromTokens (filter.text.toLowerCase(), " ,", "\"");
        std::vector<int> own, other;
        const auto& library = progressionLibrary();
        for (int i = 0; i < (int) library.size(); ++i)
        {
            const auto& p = library[(size_t) i];
            if (filter.genre.isNotEmpty() && p.genre != filter.genre)
                continue;
            if (filter.mood.isNotEmpty() && ! p.moods.contains (filter.mood))
                continue;
            if (filter.ownModeOnly && p.minor != key.minor)
                continue;
            if (filter.favoritesOnly && ! filter.favorites.contains (p.name))
                continue;
            if (! words.isEmpty())
            {
                auto text = (p.name + " " + p.genre + " " + p.moods.joinIntoString (" ") + " " + p.numerals + " "
                             + (p.minor ? "minor" : "major")).toLowerCase();
                for (const auto& s : p.in (key))
                    text << " " << s.name (key).toLowerCase();
                if (! std::all_of (words.begin(), words.end(), [&text] (const juce::String& w) { return text.contains (w); }))
                    continue;
            }
            (p.minor == key.minor ? own : other).push_back (i);
        }
        own.insert (own.end(), other.begin(), other.end());
        return own;
    }

    int progressionNamed (const juce::String& name)
    {
        const auto& library = progressionLibrary();
        for (int i = 0; i < (int) library.size(); ++i)
            if (library[(size_t) i].name == name)
                return i;
        return -1;
    }

    juce::String progressionToText (const std::vector<ProgressionStep>& steps)
    {
        juce::StringArray parts;
        for (const auto& s : steps)
            parts.add (s.chord.toText() + "*" + juce::String (s.bars));
        return parts.joinIntoString (";");
    }

    std::vector<ProgressionStep> progressionFromText (const juce::String& text)
    {
        std::vector<ProgressionStep> steps;
        for (const auto& part : juce::StringArray::fromTokens (text, ";", {}))
        {
            const auto chord = Chord::fromText (part.upToFirstOccurrenceOf ("*", false, false));
            const auto bars = part.containsChar ('*') ? part.fromFirstOccurrenceOf ("*", false, false).getDoubleValue() : 1.0;
            if (! chord || bars <= 0.0)
                return {};
            steps.push_back ({ *chord, bars, 0, {} });
        }
        return steps;
    }
}
