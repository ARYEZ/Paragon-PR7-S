#include "engine/Blueprints.h"

// Our blueprints (0.152.0): the owner chose ten songs; each song's form was read from the owner's MIDI files (and one
// from the tabs they sent) for its layout only: where the sections start and end, which repeat, the chords as steps
// of the key, the tempo. No notes are taken. Named in our own words (the owner's choice: no songs' names in the app).
// Section names are songSectionNames()'; types are ours (SectionTypes.h), picked for each section's character;
// everything is in 4/4 (the owner: no meter changes).
namespace pr7s
{
    namespace
    {
        struct Row { const char* name; int bars, link; const char* type; const char* transition; const char* chords; };

        Blueprint make (const char* name, const char* description, double bpm, bool minor, const char* key,
                        std::initializer_list<Row> rows)
        {
            Blueprint bp;
            bp.name = name;
            bp.description = description;
            bp.bpm = bpm;
            bp.minor = minor;
            bp.factory = true;
            const auto& all = Key::all();
            for (size_t i = 0; i < all.size(); ++i)
                if (all[i].name() == juce::String (key))
                    bp.keyIndex = (int) i;
            for (const auto& r : rows)
                bp.blocks.push_back (blueprintBlock (r.name, r.bars, r.link, r.type, r.transition, r.chords, minor));
            return bp;
        }
    }

    const std::vector<Blueprint>& factoryBlueprints()
    {
        static const std::vector<Blueprint> all {
            make ("Stadium Build",
                  "Bass and drums carry a long first verse, a guitar riff alone before each chorus, a half-time bridge "
                  "over one pedal, and a double-length last chorus; the intro riff comes back as the post-chorus and the outro.",
                  110.0, true, "F minor", {
                { "Intro", 8, 0, "FULL BAND FROM THE DOWNBEAT", "DRUMS BUILD", "i:2 bII III:0.5 IV:0.5 i:2 bII VII:0.5 III:0.25 VI:0.25" },
                { "Verse", 16, 0, "SPARSE", "SILENT BAR", "i:2 bII:2 VII:2 VI:2 i:2 bII:2 VII:2 VI:2" },
                { "Pre-Chorus", 8, 1, "SNARE CARPET BUILD", "SNARE ROLL", "I:7 III:0.5 bII:0.5" },
                { "Riff", 4, 2, "", "DRUMS BUILD", "i III VII VI" },
                { "Chorus", 8, 0, "DRIVING", "DRUMS BUILD", "i III V VII i III v iv:0.5 V:0.5" },
                { "Post-Chorus", 8, 0, "HOOK RIFF", "BREATH", "i:2 bII III:0.5 IV:0.5 i:2 bII VII:0.5 III:0.25 VI:0.25" },
                { "Verse", 8, 0, "SPARSE", "", "i:8" },
                { "Pre-Chorus", 8, 1, "SNARE CARPET BUILD", "SNARE ROLL", "I:7 III:0.5 bII:0.5" },
                { "Chorus", 16, 3, "DRIVING", "DRUMS BUILD", "i III V VII i III v iv:0.5 V:0.5 i III v VII i III iv VI:0.5 V:0.5" },
                { "Bridge", 6, 0, "HALF-TIME MELODIC", "STOP", "V:6" },
                { "Riff", 4, 2, "", "DRUMS BUILD", "i III VII VI" },
                { "Build-Up", 4, 0, "", "DRUMS BUILD", "i III VII VI:0.5 V:0.5" },
                { "Chorus", 16, 3, "DRIVING", "DRUMS BUILD", "i III V VII i III v iv:0.5 V:0.5 i III v VII i III iv VI:0.5 V:0.5" },
                { "Outro", 8, 0, "INTRO RETURNS", "", "i:2 bII III:0.5 IV:0.5 i:2 bII VII:0.5 III:0.25 VI:0.25" } }),

            make ("Seven-Bar Cycle",
                  "A picked riff over one pedal for the verses, against a chorus that's one seven-bar chord cycle played "
                  "twice; a bridge strips it to the bass, then one last double cycle and a single note.",
                  100.0, true, "A minor", {
                { "Intro", 4, 0, "RIFF ALONE, THEN THE BAND", "", "i:4" },
                { "Verse", 8, 1, "PICKED OSTINATO", "DRUMS BUILD", "i:8" },
                { "Chorus", 14, 2, "DRONING", "DRUMS BUILD", "i III v IV III II V i III v IV III II V" },
                { "Break", 2, 0, "", "", "i:2" },
                { "Verse", 8, 1, "PICKED OSTINATO", "DRUMS BUILD", "i:8" },
                { "Chorus", 14, 2, "DRONING", "DRUMS BUILD", "i III v IV III II V i III v IV III II V" },
                { "Bridge", 7, 0, "CLEAN BRIDGE", "", "i III v IV III II V" },
                { "Chorus", 14, 2, "DRONING", "DRUMS BUILD", "i III v IV III II V i III v IV III II V" },
                { "Ending", 1, 0, "LAST CHORD RINGS", "", "i" } }),

            make ("Rap Verse, Sung Hook",
                  "Short, slow and heavy: chugged verses on one chord under a rap, a four-bar sung chorus then a double one, "
                  "a stop-start build, and two bridges that end the song on one hit.",
                  75.0, true, "C# minor", {
                { "Intro", 3, 0, "SWELL AND DRONE", "", "i:3" },
                { "Riff", 4, 0, "", "DRUMS BUILD", "iv:4" },
                { "Verse", 8, 1, "SPLIT CHUG", "BREATH", "i:8" },
                { "Chorus", 4, 0, "DRONING", "", "iv:4" },
                { "Verse", 8, 1, "SPLIT CHUG", "BREATH", "i:8" },
                { "Chorus", 8, 0, "DRONING", "DRUMS BUILD", "iv:8" },
                { "Build-Up", 4, 0, "HIT & HOLD, THEN CARPET", "SNARE ROLL", "i:4" },
                { "Bridge", 8, 0, "HALF-TIME MELODIC", "", "VI iv VI iv VI iv VI iv" },
                { "Bridge", 10, 0, "HALF-TIME MELODIC", "DRUMS BUILD", "VI iv VI iv VI iv VI:3 iv" },
                { "Ending", 1, 0, "FILL AND FINAL HIT", "", "i" } }),

            make ("Quiet Verse, Loud Return",
                  "Clean piano verses, a four-bar lift, choruses that hold the drums back, guitars arriving in the "
                  "post-chorus; a one-bar break, a bare bridge and a build into a last chorus that opens on a stop.",
                  91.0, true, "D minor", {
                { "Intro", 2, 0, "SWELL AND DRONE", "", "i:2" },
                { "Verse", 8, 1, "CLEAN VERSE", "", "i IV i VII i IV i VII" },
                { "Pre-Chorus", 4, 2, "CYCLE IN QUARTERS", "", "VI III VII:2" },
                { "Chorus", 8, 3, "DRONING", "DRUMS BUILD", "I iv VI:0.5 III:0.5 VII I iv VI:0.5 III:0.5 VII" },
                { "Post-Chorus", 4, 0, "CHORUS CARRIES ON", "", "I iv VI:0.5 III:0.5 VII" },
                { "Verse", 8, 1, "CLEAN VERSE", "", "i IV i VII i IV i VII" },
                { "Pre-Chorus", 4, 2, "CYCLE IN QUARTERS", "", "VI III VII:2" },
                { "Chorus", 8, 3, "DRONING", "DRUMS BUILD", "I iv VI:0.5 III:0.5 VII I iv VI:0.5 III:0.5 VII" },
                { "Break", 1, 0, "", "", "i" },
                { "Bridge", 4, 0, "CLEAN BRIDGE", "", "i iv VI:0.5 III:0.5 VII" },
                { "Build-Up", 8, 0, "", "DRUMS BUILD", "I iv VI:0.5 III:0.5 VII I iv VI:0.5 III:0.5 VII" },
                { "Chorus", 8, 3, "DRONING", "DRUMS BUILD", "I iv VI:0.5 III:0.5 VII I iv VI:0.5 III:0.5 VII" },
                { "Outro", 4, 0, "WINDING DOWN", "", "I iv VI:0.5 III:0.5 VII" },
                { "Ending", 3, 0, "LAST CHORD RINGS", "", "i:3" } }),

            make ("Riff Bookends",
                  "A heavy intro riff that comes back as the outro, verses of a single-note riff locked to the kick, "
                  "a ringing chorus, a bridge, a double-kick breakdown and two choruses in a row to finish.",
                  82.0, true, "C# minor", {
                { "Intro", 8, 0, "FULL BAND FROM THE DOWNBEAT", "DRUMS BUILD", "i:8" },
                { "Verse", 8, 0, "LINE OVER HITS", "", "i:8" },
                { "Chorus", 8, 1, "DRONING", "DRUMS BUILD", "VI i III i VI i iv V" },
                { "Verse", 12, 0, "LINE OVER HITS", "STOP", "i:12" },
                { "Chorus", 8, 1, "DRONING", "DRUMS BUILD", "VI i III i VI i iv V" },
                { "Bridge", 8, 0, "HALF-TIME MELODIC", "", "VI i VI i VI i iv II" },
                { "Breakdown", 8, 0, "HEAVY CHUG", "DRUMS BUILD", "i:8" },
                { "Chorus", 8, 1, "DRONING", "DRUMS BUILD", "VI i III i VI i iv V" },
                { "Chorus", 8, 1, "DRONING", "DRUMS BUILD", "VI i III i VI i iv V" },
                { "Outro", 8, 0, "INTRO RETURNS", "", "i:8" } }),

            make ("Piano Hook First",
                  "Opens with the chorus sung over piano alone, then the band's riff, driving verses, a two-bar lift into "
                  "nine-bar choruses, a sparse bridge, a heavy middle eight and a last chorus that ends on one chord.",
                  93.0, true, "E minor", {
                { "Intro", 10, 0, "KEYS ALONE, THEN THE RIFF", "", "i VI i bI III bI III vi III:2" },
                { "Riff", 4, 0, "", "", "i:0.75 VII:0.25 i:0.75 VII:0.25 i:0.75 VII:0.25 i:0.75 VII:0.25" },
                { "Verse", 8, 1, "MUTED DRIVE", "DRUMS BUILD", "i:0.75 VII:0.25 i:0.75 VII:0.25 i:0.75 VII:0.25 i:0.75 VII:0.25 "
                                                              "i:0.75 VII:0.25 i:0.75 VII:0.25 i:0.75 VII:0.25 i:0.75 VII:0.25" },
                { "Pre-Chorus", 2, 2, "BUILD & STOP", "", "VI V" },
                { "Chorus", 9, 3, "DRIVING", "", "i VI i bI III bI III vi III" },
                { "Riff", 2, 0, "", "DRUMS BUILD", "i:0.75 VII:0.25 i:0.75 VII:0.25" },
                { "Verse", 8, 1, "MUTED DRIVE", "DRUMS BUILD", "i:0.75 VII:0.25 i:0.75 VII:0.25 i:0.75 VII:0.25 i:0.75 VII:0.25 "
                                                              "i:0.75 VII:0.25 i:0.75 VII:0.25 i:0.75 VII:0.25 i:0.75 VII:0.25" },
                { "Pre-Chorus", 2, 2, "BUILD & STOP", "", "VI V" },
                { "Chorus", 9, 3, "DRIVING", "", "i VI i bI III bI III vi III" },
                { "Break", 1, 0, "", "", "III" },
                { "Bridge", 10, 0, "CLEAN BRIDGE", "", "VI vi III:2 VI vi III bV III bV" },
                { "Middle 8", 8, 0, "HEAVY BUILD", "", "iv:0.5 VII:0.5 i VI:0.5 V:0.5 i iv:0.5 VII:0.5 i VI V" },
                { "Chorus", 8, 0, "DRIVING", "", "i VI i bI III bI VI III" },
                { "Ending", 1, 0, "LAST CHORD RINGS", "", "III" } }),

            make ("Clean Intro, Long Climb",
                  "A clean figure alone, a slow distorted build and a snare roll into the main riff; chugged verses, a "
                  "half-time lift into short driving choruses, a long solo, a quiet spoken break and a double last chorus "
                  "before the riff and the intro figure together to the end.",
                  120.0, true, "E minor", {
                { "Intro", 12, 0, "ARPEGGIO BUILD", "", "i:12" },
                { "Build-Up", 16, 0, "", "SNARE ROLL", "i:16" },
                { "Riff", 8, 1, "", "", "i:3 III:0.5 i:0.25 II:0.25 i:3 III:0.5 i:0.25 II:0.25" },
                { "Verse", 8, 2, "STACCATO CHUG", "", "i:3 III:0.5 i:0.5 i:3 III:0.5 i:0.5" },
                { "Pre-Chorus", 4, 3, "BUILD & STOP", "SNARE ROLL", "II:2.5 V:0.5 II:0.5 i:0.5" },
                { "Chorus", 7, 0, "DRIVING", "", "II:3 i II i III:0.5 i:0.25 II:0.25" },
                { "Riff", 8, 1, "", "", "i:3 III:0.5 i:0.25 II:0.25 i:3 III:0.5 i:0.25 II:0.25" },
                { "Verse", 8, 2, "STACCATO CHUG", "", "i:3 III:0.5 i:0.5 i:3 III:0.5 i:0.5" },
                { "Pre-Chorus", 4, 3, "BUILD & STOP", "SNARE ROLL", "II:2.5 V:0.5 II:0.5 i:0.5" },
                { "Chorus", 8, 0, "DRIVING", "STOP", "II:3 i II i III:0.5 i:0.5 i" },
                { "Solo", 19, 0, "DRIVING", "STOP", "i:3 III:0.5 i:0.25 II:0.25 i:3 III:0.5 i:0.25 II:0.25 II:2.5 V:0.5 II:0.5 i:0.5 II:3 i II i i" },
                { "Break", 12, 0, "CLEAN BRIDGE", "", "i:12" },
                { "Build-Up", 8, 0, "", "SNARE ROLL", "II:8" },
                { "Chorus", 14, 0, "DRIVING", "STOP", "II:3 i II i II:3 i II i III:0.5 i:0.5 i" },
                { "Riff", 8, 1, "", "", "i:3 III:0.5 i:0.25 II:0.25 i:3 III:0.5 i:0.25 II:0.25" },
                { "Outro", 25, 0, "INTRO RETURNS", "", "i:25" } }),

            make ("Clean Verse, Heavy Chorus",
                  "A clean bass-and-drums groove for the verses and a short clean interlude, against heavy riffed "
                  "choruses; a refrain on one pedal after the second and third, a solo that builds, and two choruses "
                  "back to the clean groove at the end.",
                  115.0, true, "G# minor", {
                { "Intro", 5, 0, "RIFF ALONE, THEN THE BAND", "", "i III:0.5 II:0.25 V:0.25 i III:0.5 II:0.25 V:0.25 i" },
                { "Verse", 8, 1, "CLEAN VERSE", "WALK UP", "i III:0.5 II:0.25 V:0.25 i III:0.5 II:0.25 V:0.25 i III:0.5 II:0.25 V:0.25 i III:0.5 II:0.25 V:0.25" },
                { "Chorus", 8, 2, "DRIVING", "DRUMS BUILD", "i VII:0.5 VI:0.5 i VII:0.5 III:0.5 i VII:0.5 VI:0.5 i VII:0.5 II:0.5" },
                { "Interlude", 4, 3, "HOOK RIFF", "", "i III:0.5 II:0.25 V:0.25 i III:0.5 II:0.25 V:0.25" },
                { "Verse", 8, 1, "CLEAN VERSE", "WALK UP", "i III:0.5 II:0.25 V:0.25 i III:0.5 II:0.25 V:0.25 i III:0.5 II:0.25 V:0.25 i III:0.5 II:0.25 V:0.25" },
                { "Chorus", 8, 2, "DRIVING", "DRUMS BUILD", "i VII:0.5 VI:0.5 i VII:0.5 III:0.5 i VII:0.5 VI:0.5 i VII:0.5 II:0.5" },
                { "Refrain", 12, 4, "DRONING", "STOP", "V:12" },
                { "Interlude", 4, 3, "HOOK RIFF", "", "i III:0.5 II:0.25 V:0.25 i III:0.5 II:0.25 V:0.25" },
                { "Verse", 8, 1, "CLEAN VERSE", "WALK UP", "i III:0.5 II:0.25 V:0.25 i III:0.5 II:0.25 V:0.25 i III:0.5 II:0.25 V:0.25 i III:0.5 II:0.25 V:0.25" },
                { "Chorus", 8, 2, "DRIVING", "DRUMS BUILD", "i VII:0.5 VI:0.5 i VII:0.5 III:0.5 i VII:0.5 VI:0.5 i VII:0.5 II:0.5" },
                { "Refrain", 12, 4, "DRONING", "STOP", "V:12" },
                { "Solo", 16, 0, "DRONING", "SNARE ROLL", "i III:0.5 II:0.25 V:0.25 i III:0.5 II:0.25 V:0.25 i III:0.5 II:0.25 V:0.25 i III:0.5 II:0.25 V:0.25 i:6 III i" },
                { "Chorus", 8, 2, "DRIVING", "DRUMS BUILD", "i VII:0.5 VI:0.5 i VII:0.5 III:0.5 i VII:0.5 VI:0.5 i VII:0.5 II:0.5" },
                { "Chorus", 8, 2, "DRIVING", "DRUMS BUILD", "i VII:0.5 VI:0.5 i VII:0.5 III:0.5 i VII:0.5 VI:0.5 i VII:0.5 II:0.5" },
                { "Interlude", 4, 3, "HOOK RIFF", "", "i III:0.5 II:0.25 V:0.25 i III:0.5 II:0.25 V:0.25" },
                { "Outro", 5, 0, "LAST CHORD RINGS", "", "i:5" } }),

            make ("Riff-Led Lift",
                  "Two guitars alone, then the band; the verse riff alone before every verse, a climbing pre-chorus that "
                  "stops dead before each driving chorus, a long interlude from sparse stabs to a heavy pedal, a drums-only "
                  "break and the verse riff as the outro.",
                  99.0, true, "A minor", {
                { "Intro", 12, 0, "RIFF WITH DRUMS BUILDING", "", "v:9 v:0.5 iv:0.5 III III:0.5 II:0.5" },
                { "Riff", 2, 0, "", "", "iv III" },
                { "Verse", 8, 1, "MUTED DRIVE", "", "iv:0.75 III:0.25 iv:0.75 III:0.25 iv:0.75 III:0.25 iv:0.75 III:0.25 iv:0.75 III:0.25 iv:0.75 III:0.25 iv:0.75 III:0.25 v" },
                { "Pre-Chorus", 4, 2, "RISING BASS", "STOP", "v:4" },
                { "Chorus", 8, 3, "DRIVING", "", "i VII VI V i II III iv:0.5 v:0.5" },
                { "Riff", 1, 0, "", "", "iv:0.5 III:0.5" },
                { "Verse", 8, 1, "MUTED DRIVE", "", "iv:0.75 III:0.25 iv:0.75 III:0.25 iv:0.75 III:0.25 iv:0.75 III:0.25 iv:0.75 III:0.25 iv:0.75 III:0.25 iv:0.75 III:0.25 v" },
                { "Pre-Chorus", 4, 2, "RISING BASS", "STOP", "v:4" },
                { "Chorus", 8, 3, "DRIVING", "", "i VII VI V i II III iv:0.5 v:0.5" },
                { "Interlude", 16, 0, "TRITONE AIR & BUILD", "STOP", "v:0.5 i:0.5 v:0.5 i:0.5 v:0.5 i:0.5 v:0.5 i:0.5 v:0.5 i:0.5 v:0.5 i:0.5 v:0.5 i:0.5 v:0.5 i:0.5 i:8" },
                { "Chorus", 8, 3, "DRIVING", "", "i VII VI V i II III iv:0.5 v:0.5" },
                { "Break", 4, 0, "", "", "" },
                { "Outro", 8, 0, "WINDING DOWN", "", "iv:0.75 III:0.25 iv:0.75 III:0.25 iv:0.75 III:0.25 iv:0.75 III:0.25 iv:0.75 III:0.25 iv:0.75 III:0.25 iv:0.75 III:0.25 v" } }),

            make ("Still Verse, Huge Chorus",
                  "Half-time throughout: a heavy pedal riff before the verses, verses of one clean still shape, a "
                  "pre-chorus that builds and stops, huge pushed chorus chords, then a break, a breakdown, a reordered last "
                  "chorus and a riffed post-chorus, ending on the breakdown.",
                  103.0, true, "Ab minor", {
                { "Intro", 4, 0, "SWELL AND DRONE", "", "i:4" },
                { "Riff", 9, 0, "", "STOP", "VII:9" },
                { "Verse", 8, 1, "CLEAN VERSE", "", "i:8" },
                { "Pre-Chorus", 7, 2, "BUILD & STOP", "STOP", "VII iv VI III VII:3" },
                { "Chorus", 8, 3, "DRONING", "STOP", "i VII iv VI i VII iv VI" },
                { "Riff", 4, 0, "", "", "VII:4" },
                { "Verse", 8, 1, "CLEAN VERSE", "", "i:8" },
                { "Pre-Chorus", 7, 2, "BUILD & STOP", "STOP", "VII iv VI III VII:3" },
                { "Chorus", 8, 3, "DRONING", "STOP", "i VII iv VI i VII iv VI" },
                { "Break", 8, 0, "GUITARS OUT, THEN BACK", "", "bII:0.5 i:0.5 bII:0.5 i:0.5 bII:0.5 i:0.5 bII:0.5 i:0.5 bII:0.5 i:0.5 bII:0.5 i:0.5 bII:0.5 i:0.5 bII:0.5 i:0.5" },
                { "Breakdown", 9, 0, "HEAVY CHUG", "SILENT BAR", "bII:0.5 i:0.5 bII:0.5 i:0.5 bII:0.5 i:0.5 bII:0.5 i:0.5 bII:0.5 i:0.5 bII:0.5 i:0.5 bII:0.5 i:0.5 bII:0.5 i:0.5 bII:0.5 i:0.5" },
                { "Chorus", 8, 0, "DRONING", "STOP", "VI iv i III VI:0.75 III:0.75 iv i:0.75 VII:1.25" },
                { "Post-Chorus", 8, 0, "HOOK RIFF", "", "i:8" },
                { "Breakdown", 4, 0, "HEAVY CHUG", "", "bII:0.5 i:0.5 bII:0.5 i:0.5 bII:0.5 i:0.5 bII:0.5 i:0.5" } }),
        };
        return all;
    }
}
