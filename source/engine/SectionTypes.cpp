#include "engine/SectionTypes.h"

#include "engine/SongSections.h"

namespace pr7s
{
    std::vector<SectionType> sectionTypesFor (const juce::String& sectionName)
    {
        // composerSectionFor: intro 0, verse 1, chorus 2, bridge 3, breakdown 4, ending 5, solo 6.
        const SectionType driving { "DRIVING", "Eighth notes driving the chorus on", RiffFlavour::drivingEighths };
        // (0.99.0, the owner's Another Life chorus, with Faster's syncopated bar) the guitar's eighths, the bass and the kick
        // locked in a syncopated figure under them.
        const SectionType syncopated { "SYNCOPATED DRIVE", "Driving eighths over a syncopated bass and kick: stab openings, pushed changes, a half-time snare",
                                       RiffFlavour::syncopatedDrive };
        const SectionType droning { "DRONING", "Chords rung across the bar, the band carrying the movement", RiffFlavour::droning };
        const SectionType staccato { "STACCATO CHUG", "Short palm-muted chugs and stops, the bass and the kick locked to them; the rhythm guitar picks the kind",
                                     RiffFlavour::pedalStabs,
                                     { RiffFlavour::pedalStabs, RiffFlavour::chugThrees,
                                       RiffFlavour::tritone, RiffFlavour::chugs, RiffFlavour::gallop, RiffFlavour::stopStart,
                                       RiffFlavour::gallopSqueal },   // (0.100.0: This Fire Burns' verse)
                                     SectionType::Band::staccato, 6 };
        // (0.96.0, the owner's) The bar split into a played part and a part left open; Waffle's BURST & SILENCE moved
        // here from STACCATO CHUG (the owner: it fits better).
        const SectionType split { "SPLIT CHUG", "The bar split: chugs or stabs in one part, the rest left open for the vocal", RiffFlavour::secondHalfStabs,
                                  { RiffFlavour::secondHalfStabs, RiffFlavour::firstHalfBurst, RiffFlavour::hitPush, RiffFlavour::burstSilence },
                                  SectionType::Band::split };
        // (0.97.0, the owner's: Masterpiece, Circle With Me, Clown, Crawling, In The End, The End of the Dream)
        const SectionType clean { "CLEAN VERSE", "The heavy guitar steps back: a clean guitar picked or ringing, or the keys carry the verse",
                                  RiffFlavour::openChords, {}, SectionType::Band::clean };
        // (0.100.0, the owner's Faster verse) palm-muted eighths with a plain backbeat under them.
        const SectionType muted { "MUTED DRIVE", "Palm-muted eighths, single notes then chords, a plain backbeat; the verse holds back for the chorus",
                                  RiffFlavour::mutedDrive };
        const SectionType sparse { "SPARSE", "Space for the voice: stabs, octaves, chords let ring", RiffFlavour::stabs,
                                   { RiffFlavour::stabs, RiffFlavour::octaves, RiffFlavour::openChords } };
        const SectionType heavy { "HEAVY CHUG", "The low string pounded: chugs, tritone grinds, stops; the bass and the kick locked to them", RiffFlavour::chugs,
                                  { RiffFlavour::chugs, RiffFlavour::tritone, RiffFlavour::stopStart, RiffFlavour::stabs },
                                  SectionType::Band::staccato };
        const SectionType creep { "CREEP", "A line creeping a half step at a time", RiffFlavour::creep };
        const SectionType open { "OPEN CHORDS", "Open chords let ring", RiffFlavour::openChords };
        const SectionType stabs { "STABS", "Short chord stabs with space between", RiffFlavour::stabs };
        // (0.102.0) A pre-chorus's own types first (the owner's songs, shapes only; our names), then the verse's
        // (a pre-chorus plays as a verse otherwise, so a block given one of those keeps it).
        if (sectionBaseOf (sectionName) == "Pre-Chorus")
        {
            const auto pre = SectionType::Band::preChorus;
            return { { "BUILD & STOP", "The verse's drive pushed toward the chorus, then everything stops for a beat before it lands", RiffFlavour::openChords, {}, pre },
                     { "GUITARS OUT, BASS CLIMBS", "The guitars drop out; the bass climbs in long pushed notes, keys and a high motif carry it", RiffFlavour::openChords, {}, pre },
                     { "CYCLE IN QUARTERS", "A still clean shape in eighths with a three-note cycle in quarters over it, ending on a two-bar stop", RiffFlavour::openChords, {}, pre },
                     { "CHUGS UNDER THE SHAPE", "Low muted chugs with the bass and kick on them, the verse's still shape played high over them", RiffFlavour::openChords, {}, pre },
                     { "STABS & GAPS", "The whole band on short stabs together, then silence for the voice, a pickup into the next bar", RiffFlavour::openChords, {}, pre },
                     { "RISING BASS", "Chords held or tremolo picked while the bass climbs a step a bar underneath", RiffFlavour::openChords, {}, pre },
                     staccato, split, clean, muted, droning, sparse };
        }
        // (0.110.0) A post-chorus's, an interlude's or an instrumental's own types first (the owner's songs, shapes only;
        // our names), then what it had: a post-chorus the chorus's, the others the bridge's.
        if (const auto base = sectionBaseOf (sectionName); base == "Post-Chorus" || base == "Interlude" || base == "Instrumental")
        {
            const auto i = SectionType::Band::interlude;
            const auto r = RiffFlavour::openChords;
            std::vector<SectionType> types {
                { "HOOK RIFF", "Pushed power chords under a two-voice lead hook doubled an octave down, every change an eighth early", r, {}, i },
                { "CHORUS CARRIES ON", "The chorus's drive keeps going under a tag (moving or frozen on the tonic), ending on two bars of stabs", r, {}, i },
                { "PEDAL IN THREES", "Sixteenths with the low root on every third, moving notes between, the band doubling it", r, {}, i },
                { "HIGH TRILL HOOK", "A high half-step trill over a chord tone, or chugs answered by a fast trill, over toms or the ride", r, {}, i },
                { "PEDAL & HIGH MELODY", "A muted low pedal or gallops carrying a high melody stepping down", r, {}, i },
                { "BURST & ANSWER", "A high burst answered by a walk down to the root, or chugs answered by a pinched slide", r, {}, i },
                { "STAB & ANSWER", "The band stabs and stops, the lead answers in the gap, the next chord comes in early", r, {}, i },
                { "TREMOLO OVER CHUGS", "Root chugs in threes under double kick, long tremolo-picked octaves falling over them", r, {}, i },
                { "CLEAN BREATH", "A clean arpeggio or a falling top note over toms, the band climbing back in", r, {}, i },
                { "TRITONE AIR & BUILD", "A slide up and a fall to the tritone, the tritone held, the guitar out, scratches building back", r, {}, i },
                { "CHROMATIC WOBBLE", "Muted low sixteenths wobbling a half step in odd groups across the bar line", r, {}, i } };
            const auto before = sectionTypesFor (base == "Post-Chorus" ? juce::String ("Chorus") : juce::String ("Bridge"));
            types.insert (types.end(), before.begin(), before.end());
            return types;
        }
        switch (composerSectionFor (sectionName))
        {
            case 0:
            {
                // (0.111.0) An intro's own types first (the owner's songs, shapes only; our names), then the old two.
                const auto b = SectionType::Band::bookend;
                const auto r = RiffFlavour::openChords;
                return { { "RIFF ALONE, THEN THE BAND", "One guitar alone (a muted line or the riff), the band landing together halfway", r, {}, b },
                         { "FULL BAND FROM THE DOWNBEAT", "The riff at full force from the first beat", r, {}, b },
                         { "ARPEGGIO BUILD", "A clean arpeggio alone, the bass coming in by stages, the heavy band as a wall at the end", r, {}, b },
                         { "KEYS HOOK OVER A LOOP", "A keys hook or ostinato over a light loop, a guitar pickup into the next section", r, {}, b },
                         { "SWELL AND DRONE", "A swelling keys drone, one guitar chord swelled in, the band landing on the next section", r, {}, b },
                         { "STAB CALLS", "Bursts and stabs with the toms and silence between, then a riff", r, {}, b },
                         { "TREMOLO SWELL", "Tremolo-picked chords over the ride alone, the groove coming in halfway", r, {}, b },
                         { "HOLD AND ANSWER", "A low chord held, answered by two pickup hits or a high clash", r, {}, b },
                         { "DOWNBEAT HITS UNDER A LEAD", "A lead line over the band hitting only each bar's first beat", r, {}, b },
                         { "RIFF WITH DRUMS BUILDING", "The riff steady while the drums build: hits, a groove, a double-kick carpet", r, {}, b },
                         droning, open };
            }
            case 1: return { staccato, split, clean, muted, droning, sparse };
            case 2:
            {
                // (0.115.0) Four more from the owner's 22 song files (shapes only; our names), after the first three.
                const auto c = SectionType::Band::chorus;
                const auto r = RiffFlavour::openChords;
                return { driving, syncopated, droning,
                         { "PUSHED CHORDS UNDER A FIXED HOOK", "Power chords struck rarely and held, each change pushed a beat early, under a hook that keeps its notes over every chord", r, {}, c },
                         { "TWIN HARMONY, HALF TIME", "A lead line on a 3+3+2 figure doubled a third under, power chords rung the bar, half-time drums", r, {}, c },
                         { "SIXTEENTH WALL, HALF-TIME KIT", "Muted power chords on every sixteenth over a half-time backbeat, a three-chord turnaround every fourth bar", r, {}, c },
                         { "GROUPED STREAM", "One guitar an unbroken wall of eighths, the bass and the kick in groups of fives with their rests off the beat", r, {}, c },
                         // (0.116.0) The other four.
                         { "TOM GROOVE RIFF", "The toms carry the time, the guitar on the off-beats, a lead ostinato in eighths, the bass bursting then leaving space", r, {}, c },
                         { "RING, THEN ANSWER", "A power chord rung a whole bar, answered next bar by hits after the beat; half time, a falling held lead", r, {}, c },
                         { "HALF-TIME CHORDS WITH AN OCTAVE PEDAL", "Chords held in half time while one note doubled at the octave is picked in eighths over every chord", r, {}, c },
                         { "CHUG BAR, CHORD BAR", "A bar of chugs on the low root answered by a bar of struck chords, a high squeal at the end of every other bar", r, {}, c } };
            }
            case 3:
            {
                // (0.105.0) A bridge's own types first (the owner's songs, shapes only; our names), then the old three.
                const auto b = SectionType::Band::bridge;
                const auto r = RiffFlavour::openChords;
                return { { "HALF-TIME MELODIC", "Half time: a ringing chord with a melody on it, stabs in 3+3+2, a ghost-note groove into a triplet fill", r, {}, b },
                         { "HEAVY BUILD", "Scratches, dotted climbs, siren chugs or falling eighths into a harmonic, the kick with the guitar", r, {}, b },
                         { "LATE-CHANGE CLIMB", "Power chords climbing in sixteenths, each change a sixteenth late, the kick on every one", r, {}, b },
                         { "CLEAN BRIDGE", "Clean and bare: a pair of notes with the top stepping down, held octaves or a pedal, the band coming in late", r, {}, b },
                         { "SLIDING CHORDS", "A four-bar figure: chord and eighths, a lick, a line in quarters, a chord sliding down a half step", r, {}, b },
                         { "GUITARS OUT, THEN BACK", "The guitars drop out and come back for the end, lifting into the next chorus", r, {}, b },
                         { "SLIDE OVER A HELD ROOT", "A palm-muted root rung with a sliding triplet figure on top, in turn with syncopated stabs", r, {}, b },
                         creep, open, droning };
            }
            case 4:
            {
                // (0.109.0) A breakdown's own types first (the owner's songs, shapes only; our names), then the old two.
                const auto d = SectionType::Band::breakdown;
                const auto r = RiffFlavour::chugs;
                return { { "CHROMATIC CRAWL", "Low power chords in quarters creeping inside a minor third, harmonics ringing in the gaps, the snare once in two bars", r, {}, d },
                         { "HIT & HOLD, THEN CARPET", "A stab and a held chord over half time, then muted eighths over a double-kick carpet", r, {}, d },
                         { "CHUG & SQUEAL", "Half-time chugs answered by a high clashing chord held over them", r, {}, d },
                         { "DOTTED TRITONE PUSH", "Dead-string scratches building, then power chords in a dotted push climbing to the tritone", r, {}, d },
                         { "TRIPLET CLUSTER", "A low clashing cluster in triplet groups with gaps, widening at the end as if slowing down", r, {}, d },
                         { "GAPPED SIXTEENTHS", "One muted chord in sixteenths carved by its gaps, or a chug stream stabbed with dark chords", r, {}, d },
                         { "THREES & TWOS", "Stabs and chugs grouped in threes and twos, the kick and china locked to them", r, {}, d },
                         { "CALL & CHUG", "A bar of bent single notes or high octaves, answered by a bar of chugs", r, {}, d },
                         { "HALF-TIME OPEN CHORDS", "Wide open chords on a sparse half-time rhythm, a melody over them, a build at the end", r, {}, d },
                         { "OFF-BEAT GALLOPS", "Single-note gallops coming in after the downbeat in bursts, a pinch squeal in a gap", r, {}, d },
                         heavy, staccato };
            }
            case 5:
            {
                // (0.111.0) An outro's own types first (the owner's songs, shapes only; our names), then the old two.
                const auto b = SectionType::Band::bookend;
                const auto r = RiffFlavour::openChords;
                return { { "INTRO RETURNS, LAST CHORD RINGS", "The opening riff back, then the last chord rung out", r, {}, b },
                         { "WINDING DOWN", "The hits thinning out (beats, halves, whole notes) to the last chord held", r, {}, b },
                         { "DEAD STOP", "The riff to the end, cut off on beat 3 of the last bar with nothing left ringing", r, {}, b },
                         { "STABS, THEN RING", "Two stabs a bar (or pushed chords), then the last chord rung out", r, {}, b },
                         { "FILL AND FINAL HIT", "The riff, a fill down the toms, one big final hit", r, {}, b },
                         { "HOOK ALONE TO THE END", "The band stops on a hit, a clean arpeggio or keys hook alone to one held note", r, {}, b },
                         { "SPEEDING HITS", "One chord hit in half notes, quarters, then eighths, then a stop", r, {}, b },
                         { "SPACED STABS", "Four chugs into a hit on 1, then three beats of silence, over and over", r, {}, b },
                         { "TRIPLE STAB STOP", "The drive to the last bar: three sixteenths on the key's chord, then silence", r, {}, b },
                         droning, stabs };
            }
            case 6: return { driving, droning };
            default: break;
        }
        return {};
    }

    std::optional<SectionType> sectionTypeNamed (const juce::String& sectionName, const juce::String& type)
    {
        for (const auto& t : sectionTypesFor (sectionName))
            if (t.name.equalsIgnoreCase (type.trim()))
                return t;
        return std::nullopt;
    }

    std::optional<SectionType> sectionTypeCalled (const juce::String& type)
    {
        if (type.isEmpty())
            return std::nullopt;
        for (const auto* name : { "Intro", "Verse", "Chorus", "Bridge", "Breakdown", "Ending", "Solo", "Pre-Chorus", "Interlude" })
            if (auto t = sectionTypeNamed (name, type))
                return t;
        return std::nullopt;
    }

    std::optional<RiffFlavour> typedGuitarFlavour (const ComposerState& s)
    {
        if (s.writtenRiffs)
            return std::nullopt;
        const auto t = sectionTypeCalled (s.sectionType);
        if (! t.has_value() || t->guitarFlavours.empty())
            return std::nullopt;
        const auto& lane = s.lane (ComposerLayer::guitar);
        const auto n = (int) t->guitarFlavours.size();
        const auto automatic = t->automaticFlavours > 0 ? juce::jmin (n, t->automaticFlavours) : n;
        return t->guitarFlavours[(size_t) (lane.approach >= 0 ? lane.approach % n : lane.variation % automatic)];
    }

    bool isSectionTypeOf (const juce::String& sectionName, const juce::String& type)
    {
        return type.isEmpty() || type.equalsIgnoreCase (standardSectionType) || sectionTypeNamed (sectionName, type).has_value();
    }

    juce::StringArray sectionApproaches (RiffFlavour flavour, ComposerLayer layer)
    {
        if (flavour == RiffFlavour::drivingEighths)
            switch (layer)
            {
                // (0.98.0, the owner's This Fire Burns and Faster choruses; our names)
                case ComposerLayer::guitar: return { "STRAIGHT EIGHTHS", "SYNCOPATED THIRD BAR", "OPEN AND MUTED IN TURN",
                                                      "SLIDES INTO THE CHANGES", "STRUMMED FULL CHORDS", "MOVING POWER CHORDS" };
                case ComposerLayer::bass: return { "ROOT EIGHTHS", "DOUBLES THE GUITAR", "WALKING INTO THE CHANGES", "OCTAVE EIGHTHS",
                                                    "LOCKED WITH THE KICK" };
                case ComposerLayer::chords: return { "HELD PADS", "ANSWERS IN BARS 3 AND 4", "HIGH EIGHTH PULSE", "SILENT" };
                case ComposerLayer::lead: return { "RINGING ARPEGGIO", "HELD NOTES & A SQUEAL", "DOUBLES THE RHYTHM AN OCTAVE UP" };
                case ComposerLayer::drums: return { "DRIVING BACKBEAT", "HALF TIME, KICK ON THE EIGHTHS", "CYMBALS ON THE BEATS, KICKS BETWEEN",
                                                     "KICK WITH THE GUITAR", "DOUBLE KICK",
                                                     "CRASH ON THE BACKBEAT", "TRIBAL CALL & ANSWER" };   // (0.116.0)
                case ComposerLayer::melody: break;
            }
        if (flavour == RiffFlavour::mutedDrive)
            switch (layer)
            {
                // (0.100.0, the owner's Faster verse; our names)
                case ComposerLayer::guitar: return { "SINGLE NOTES, THEN CHORDS", "MUTED POWER CHORDS", "ROCKING CHORDS" };
                case ComposerLayer::bass: return { "ROOT EIGHTHS", "DOUBLES THE GUITAR", "ROOTS ON THE BEATS", "LOCKED WITH THE KICK" };
                case ComposerLayer::chords: return { "SILENT", "HELD PADS" };
                case ComposerLayer::lead: return { "DOUBLES THE RHYTHM AN OCTAVE UP", "SILENT" };
                case ComposerLayer::drums: return { "PLAIN BACKBEAT", "DRIVING KICK", "HALF TIME" };
                case ComposerLayer::melody: break;
            }
        if (flavour == RiffFlavour::syncopatedDrive)
            switch (layer)
            {
                // (0.99.0, the owner's Another Life chorus and Faster's syncopated bar; our names)
                case ComposerLayer::guitar: return { "STABS, THEN EIGHTHS", "SYNCOPATED THIRD BAR", "PUSHED CHANGES", "STABS, THEN PUSHES" };
                case ComposerLayer::bass: return { "LOCKED SYNCOPATION", "ROOT EIGHTHS", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" };
                case ComposerLayer::chords: return { "HELD PADS", "STABS WITH THE BASS", "SILENT" };
                case ComposerLayer::lead: return { "THREE AGAINST FOUR", "HELD NOTES & A SQUEAL", "RINGING ARPEGGIO" };
                case ComposerLayer::drums: return { "SYNCOPATED KICK, HALF-TIME SNARE", "SYNCOPATED KICK, BACKBEAT", "KICK WITH THE GUITAR",
                                                     "CYMBALS ON THE BEATS, KICKS BETWEEN" };
                case ComposerLayer::melody: break;
            }
        if (flavour != RiffFlavour::droning)
            return {};
        switch (layer)
        {
            case ComposerLayer::guitar: return { "RUNG WHOLE", "PUSHED A BEAT EARLY", "MOVING TOP NOTE", "STRUCK TWICE", "SLIDING DOWN" };
            case ComposerLayer::bass: return { "HELD WITH THE GUITAR", "HELD, PULSED AT THE LOOP'S END", "EIGHTHS UNDER THE DRONE",
                                                "OCTAVE PULSE", "WALKING INTO THE CHANGES", "WHOLE NOTES", "LOCKED WITH THE KICK" };
            case ComposerLayer::chords: return { "HIGH PULSE", "CLIMBING MOTIF", "HELD THIRD", "HELD PADS", "ARPEGGIO", "OCTAVE BELLS", "SILENT" };
            case ComposerLayer::lead: return { "OCTAVES ON THE THIRDS", "OCTAVES ON THE ROOTS", "MUTED PEDAL IN THREES" };
            case ComposerLayer::drums: return { "OPEN HATS", "HALF TIME", "LOCKED TO THE PUSHES", "RIDE GROOVE", "TOM PULSE", "DOUBLE KICK", "CYMBALS ONLY",
                                                "THE GROOVE CARRIES ON" };   // (0.116.0)
            case ComposerLayer::melody: break;
        }
        return {};
    }

    // (0.105.0) The bridge types' options, lane by lane (the bass's last always LOCKED WITH THE KICK; an empty lead
    // list: the lead plays its own styles: in these bridges it carries the hook).
    juce::StringArray bridgeApproaches (const juce::String& type, ComposerLayer layer)
    {
        using L = ComposerLayer;
        const auto t = type.trim().toUpperCase();
        if (t == "HALF-TIME MELODIC")
            switch (layer)
            {
                case L::guitar: return { "RINGING CHORD & MELODY", "STABS IN 3+3+2", "RINGING COLOUR CHORDS" };
                case L::bass: return { "ROOTS HELD", "ROOT EIGHTHS", "LOCKED WITH THE KICK" };
                case L::chords: return { "ROOT OCTAVES", "HELD PADS", "SILENT" };
                case L::drums: return { "GHOST GROOVE, TRIPLET FILL", "HALF TIME" };
                case L::lead: case L::melody: break;
            }
        if (t == "HEAVY BUILD")
            switch (layer)
            {
                case L::guitar: return { "SCRATCH BUILD", "DOTTED CLIMB", "SIREN CHUG", "FALLING EIGHTHS & HARMONIC" };
                case L::bass: return { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" };
                case L::chords: return { "SILENT", "HELD PADS" };
                case L::lead: return { "SIREN BENDS", "SILENT" };
                case L::drums: return { "KICK WITH THE GUITAR", "HALF TIME", "BUILDING SNARE" };
                case L::melody: break;
            }
        if (t == "LATE-CHANGE CLIMB")
            switch (layer)
            {
                case L::guitar: return { "SIXTEENTHS, CHANGING LATE", "EIGHTHS, CHANGING LATE" };
                case L::bass: return { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" };
                case L::chords: return { "SUS4 PAD", "SILENT" };
                case L::drums: return { "KICK ON EVERY SIXTEENTH", "HALF TIME" };
                case L::lead: case L::melody: break;
            }
        if (t == "CLEAN BRIDGE")
            switch (layer)
            {
                case L::guitar: return { "FIXED PAIR, TOP STEPPING DOWN", "HELD OCTAVES", "PEDAL IN 3+3+2 & MELODY" };
                case L::bass: return { "OUT, THEN IN HALFWAY", "SILENT", "ROOTS HELD", "LOCKED WITH THE KICK" };
                case L::chords: return { "FIFTHS PULSE", "HELD PADS", "SILENT" };
                case L::drums: return { "SNARE MARCH HALFWAY", "NO DRUMS", "RIDE ONLY" };
                case L::lead: case L::melody: break;
            }
        if (t == "SLIDING CHORDS")
            switch (layer)
            {
                case L::guitar: return { "FOUR-BAR FIGURE", "HELD AND SLIDING" };
                case L::bass: return { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" };
                case L::chords: return { "HELD PADS", "SILENT" };
                case L::drums: return { "DRIVING BEAT", "HALF TIME" };
                case L::lead: case L::melody: break;
            }
        if (t == "GUITARS OUT, THEN BACK")
            switch (layer)
            {
                case L::guitar: return { "BACK HALFWAY", "BACK FOR THE LAST QUARTER" };
                case L::bass: return { "ROOTS HELD", "ROOT EIGHTHS", "LOCKED WITH THE KICK" };
                case L::chords: return { "HELD PADS", "ARPEGGIO", "SILENT" };
                case L::drums: return { "HALF TIME, THEN DRIVING", "TOMS, THEN DRIVING" };
                case L::lead: case L::melody: break;
            }
        if (t == "SLIDE OVER A HELD ROOT")
            switch (layer)
            {
                case L::guitar: return { "HELD ROOT & STABS IN TURN", "HELD ROOT ONLY" };
                case L::bass: return { "ROOTS HELD", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" };
                case L::chords: return { "SILENT", "HELD PADS" };
                case L::drums: return { "KICK WITH THE STABS", "HALF TIME" };
                case L::lead: case L::melody: break;
            }
        return {};
    }

    // (0.111.0) The intro and outro types' options, lane by lane (the bass's last always LOCKED WITH THE KICK; only
    // DOWNBEAT HITS UNDER A LEAD has a lead list).
    juce::StringArray bookendApproaches (const juce::String& type, ComposerLayer layer)
    {
        using L = ComposerLayer;
        const auto t = type.trim().toUpperCase();
        struct Lists { const char* name; juce::StringArray guitar, bass, keys, drums, lead; };
        const juce::StringArray doubles { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" };
        static const Lists lists[] {
            { "RIFF ALONE, THEN THE BAND", { "MUTED LINE, THEN CHORDS", "RIFF ALONE, THEN DOUBLED" }, { "OUT, THEN DOUBLES", "ROOTS HELD", "LOCKED WITH THE KICK" }, { "PADS WHEN THE BAND COMES", "SILENT" }, { "OUT, THEN THE GROOVE", "HITS, THEN THE GROOVE" }, {} },
            { "FULL BAND FROM THE DOWNBEAT", { "SIXTEENTH CHUG RIFF", "THREE AGAINST FOUR" }, {}, { "SILENT", "HELD PADS" }, { "KICK WITH THE RIFF", "DOTTED KICKS, CRASH QUARTERS" }, {} },
            { "ARPEGGIO BUILD", { "ARPEGGIO, THEN THE WALL", "ARPEGGIO ALONE" }, { "IN BY STAGES", "ROOTS HELD", "LOCKED WITH THE KICK" }, { "CLIMBING HELD NOTES", "SILENT" }, { "WALL AT THE END", "CYMBAL SWELLS ONLY" }, {} },
            { "KEYS HOOK OVER A LOOP", { "PICKUP AT THE END", "OUT" }, { "HELD FROM HALFWAY", "ROOTS HELD", "LOCKED WITH THE KICK" }, { "QUARTER-NOTE HOOK", "SIXTEENTH OSTINATO" }, { "LIGHT LOOP", "HATS ONLY" }, {} },
            { "SWELL AND DRONE", { "ONE SWELLED CHORD", "OUT" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" }, { "SWELLING DRONE", "HELD PADS" }, { "OUT, A HIT TO END", "CYMBAL SWELLS" }, {} },
            { "STAB CALLS", { "STABS, THEN THE RIFF", "BURST & SILENCE CALLS" }, {}, { "SILENT", "STABS WITH THE GUITAR" }, { "TOMS WITH THE STABS", "HITS ONLY" }, {} },
            { "TREMOLO SWELL", { "TREMOLO, RIFF REVEALED", "TREMOLO ONLY" }, {}, { "SILENT", "HELD PADS" }, { "RIDE, THEN THE GROOVE", "RIDE ONLY" }, {} },
            { "HOLD AND ANSWER", { "HELD CHORD, PICKUP HITS", "HELD CHORD, HIGH CLASH" }, {}, { "SILENT", "HELD PADS" }, { "CRASH ON THE HOLDS", "HALF TIME" }, {} },
            { "DOWNBEAT HITS UNDER A LEAD", { "HITS ON ONE", "HITS, THEN THE GROOVE" }, {}, { "SILENT", "HELD PADS" }, { "HITS AND A ROLL", "HITS, THEN BLAST" }, { "PEDAL MELODY", "SILENT" } },
            { "RIFF WITH DRUMS BUILDING", { "GALLOP RIFF", "CHUG & STAB RIFF" }, {}, { "SILENT", "LOW DRONE" }, { "HITS, GROOVE, CARPET", "HITS, THEN CARPET" }, {} },
            { "INTRO RETURNS, LAST CHORD RINGS", { "RIFF, THEN RING", "MUTED LINE, THEN RING" }, {}, { "HELD PADS", "SILENT" }, { "GROOVE, CRASH ON THE CHORD", "HALF TIME, CRASH ON THE CHORD" }, {} },
            { "WINDING DOWN", { "HITS THINNING OUT", "EIGHTHS THINNING OUT" }, {}, { "HELD PADS", "SILENT" }, { "THINNING WITH THE GUITAR", "RIDE, THEN CRASH" }, {} },
            { "DEAD STOP", { "RIFF, CUT AT THE BAR LINE", "HIGH CLASH, CUT" }, {}, { "SILENT", "LOW DRONE" }, { "KICK WITH THE RIFF, FLAM ON THE LAST", "HALF TIME" }, {} },
            { "STABS, THEN RING", { "TWO STABS A BAR", "PUSHED, THEN RING" }, {}, { "HELD PADS", "SILENT" }, { "KICK AND CRASH WITH THE STABS", "STRAIGHT, CRASH ON THE CHORD" }, {} },
            { "FILL AND FINAL HIT", { "RIFF, WALK, FINAL HIT", "DRIVE, FINAL HIT" }, {}, { "HELD PADS", "SILENT" }, { "FILL, THEN THE HIT", "DOUBLE KICK, FILL, HIT" }, {} },
            { "HOOK ALONE TO THE END", { "HIT, THEN CLEAN ARPEGGIO", "HIT, THEN OUT" }, { "HIT, THEN OUT", "ROOTS HELD", "LOCKED WITH THE KICK" }, { "HOOK IN QUARTERS", "SILENT" }, { "HIT, THEN WIND DOWN", "HIT, THEN OUT" }, {} },
            { "SPEEDING HITS", { "HALVES, QUARTERS, EIGHTHS", "AFTER A PICKUP BAR" }, {}, { "STABS WITH THE GUITAR", "SILENT" }, { "CRASH ON EVERY HIT", "KICK ON EVERY HIT" }, {} },
            { "SPACED STABS", { "WALK, THEN SPACED STABS", "SPACED STABS ONLY" }, {}, { "HIGH PULSE IN THE GAPS", "SILENT" }, { "TOMS WITH THE CHUGS", "KICK AND CRASH ON THE HITS" }, {} },
            { "TRIPLE STAB STOP", { "DRIVE, THEN THREE STABS", "CHUGS, THEN THREE STABS" }, {}, { "HELD PADS", "SILENT" }, { "DRIVING, STABS WITH THE BAND", "DOUBLE KICK, STABS" }, {} },
        };
        for (const auto& list : lists)
            if (t == list.name)
                switch (layer)
                {
                    case L::guitar: return list.guitar;
                    case L::bass: return list.bass.isEmpty() ? doubles : list.bass;
                    case L::chords: return list.keys;
                    case L::drums: return list.drums;
                    case L::lead: return list.lead;
                    case L::melody: return {};
                }
        return {};
    }

    // (0.110.0) The interlude / post-chorus types' options, lane by lane (the bass's last always LOCKED WITH THE KICK; an
    // empty lead list: the lead plays its own styles).
    juce::StringArray interludeApproaches (const juce::String& type, ComposerLayer layer)
    {
        using L = ComposerLayer;
        const auto t = type.trim().toUpperCase();
        struct Lists { const char* name; juce::StringArray guitar, keys, drums, lead; };
        static const Lists lists[] {
            { "HOOK RIFF", { "PUSHED POWER CHORDS", "MUTED PEDAL TEASER", "HIGH DESCANT" }, { "PUSHED TRIADS", "SILENT" }, { "CRASH QUARTERS", "RIDE BELL EIGHTHS" }, { "TURNING HOOK", "HOOK, OCTAVE DOWN", "SILENT" } },
            { "CHORUS CARRIES ON", { "DRIVING EIGHTHS", "FROZEN ON THE TONIC" }, { "HELD PADS", "FALLING HIGH NOTES", "SILENT" }, { "SYNCOPATED KICK, RIDE", "STRAIGHT BACKBEAT" }, { "THREE AGAINST FOUR", "SILENT" } },
            { "PEDAL IN THREES", { "PEDAL IN THREES", "PEDAL, THEN RUNG CHORDS" }, { "SILENT", "LOW DRONE" }, { "DOUBLE KICK", "KICK ON THE PEDAL" }, {} },
            { "HIGH TRILL HOOK", { "HIGH TRILL", "CHUG & TRILL" }, { "SILENT", "HELD PADS" }, { "TOM GROOVE", "RIDE GROOVE" }, {} },
            { "PEDAL & HIGH MELODY", { "PEDAL & MELODY", "GALLOPS & DYADS" }, { "SLOW LINE DOWN", "SILENT" }, { "HALF TIME, PUSHED KICKS", "DRIVING" }, {} },
            { "BURST & ANSWER", { "BURST, WALK DOWN", "CHUGS, SLIDE DOWN" }, { "SILENT", "HELD PADS" }, { "KICK WITH THE GUITAR", "STRAIGHT BACKBEAT" }, {} },
            { "STAB & ANSWER", { "STAB, EARLY CHANGE", "UNISON BURST, HIGH CHORD" }, { "SILENT", "STABS WITH THE GUITAR" }, { "HITS WITH THE STABS", "TOMS WITH THE BURST" }, { "ANSWER IN THE GAP", "SILENT" } },
            { "TREMOLO OVER CHUGS", { "CHUGS IN THREES", "CHUGS WALKING DOWN" }, { "SILENT", "LOW DRONE" }, { "DOUBLE KICK, HALF TIME", "KICK ON THE CHUGS" }, { "TREMOLO OCTAVES", "SILENT" } },
            { "CLEAN BREATH", { "ARCH ARPEGGIO", "FALLING TOP NOTE" }, { "HELD PADS", "SILENT" }, { "ROLLING TOMS", "RIDE ONLY" }, {} },
            { "TRITONE AIR & BUILD", { "AIR, THEN SCRATCHES", "TRITONE HELD" }, { "SILENT", "LOW DRONE" }, { "CARRY THE GAP, BUILD", "HALF TIME" }, {} },
            { "CHROMATIC WOBBLE", { "WOBBLE IN ODD GROUPS", "WOBBLE & ACCENTS" }, { "SILENT", "LOW DRONE" }, { "KICK ON EVERY CHUG", "HALF TIME" }, {} },
        };
        for (const auto& list : lists)
            if (t == list.name)
                switch (layer)
                {
                    case L::guitar: return list.guitar;
                    case L::bass: return t == "CLEAN BREATH" ? juce::StringArray { "OUT, THEN IN HALFWAY", "ROOTS HELD", "LOCKED WITH THE KICK" }
                                                             : juce::StringArray { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" };
                    case L::chords: return list.keys;
                    case L::drums: return list.drums;
                    case L::lead: return list.lead;
                    case L::melody: return {};
                }
        return {};
    }

    // (0.115.0) The four chorus types' options, lane by lane (the bass's last always LOCKED WITH THE KICK; an empty lead
    // list: the lead plays its own styles).
    juce::StringArray chorusApproaches (const juce::String& type, ComposerLayer layer)
    {
        using L = ComposerLayer;
        const auto t = type.trim().toUpperCase();
        struct Lists { const char* name; juce::StringArray guitar, bass, keys, drums, lead; };
        static const Lists lists[] {
            { "PUSHED CHORDS UNDER A FIXED HOOK", { "PUSHED & HELD", "PUSHED, GALLOP ENDING" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "HIGH PEDAL & STEPS", "SILENT" }, { "HALF-TIME TWO-BAR", "DOUBLE KICK, GAP AFTER THE SNARE" }, { "FIXED HOOK", "HIGH TWO-NOTE HOOK" } },
            { "TWIN HARMONY, HALF TIME", { "RUNG ON THE ONE", "LOCKS TO THE LINE AT THE END", "LOCKED TO THE LINE" }, { "WHOLE-NOTE ROOTS", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
              { "HELD PADS", "SILENT" }, { "HALF TIME, CRASHES ON 1 AND 3", "HALF TIME, KICKS ON THE ANDS", "KICK WITH THE LINE" }, { "TWIN LINE A THIRD UNDER", "DOTTED ARPEGGIO" } },
            { "SIXTEENTH WALL, HALF-TIME KIT", { "SIXTEENTH WALL", "WALL, OPEN ON THE BEATS" }, { "SIXTEENTHS ON THE ROOTS", "EIGHTHS ON THE ROOTS", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "HALF TIME, CRASH EIGHTHS", "HALF TIME, RIDE EIGHTHS" }, {} },
            { "GROUPED STREAM", { "UNBROKEN EIGHTHS", "GROUPED FIVES" }, { "GROUPED FIVES", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
              { "HELD TOP NOTE", "SILENT" }, { "KICK IN GROUPS", "STRAIGHT BACKBEAT" }, {} },
            { "TOM GROOVE RIFF", { "OFF-BEAT EIGHTHS", "OFF-BEAT SINGLE NOTES" }, { "BURST, THEN SPACE", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "HELD PAD NOTE", "SILENT" }, { "TOMS FOR HATS", "MARCH: KICK AND FLOOR TOM", "TOMS ONLY" }, { "EIGHTH OSTINATO", "SILENT" } },
            { "RING, THEN ANSWER", { "RING, THEN HITS", "RING, THEN EIGHTHS" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "HELD PADS", "SILENT" }, { "HALF TIME, OPEN HATS", "HALF TIME, RIDE" }, { "FALLING HELD NOTES", "SILENT" } },
            { "HALF-TIME CHORDS WITH AN OCTAVE PEDAL", { "HELD, THEN TWO HALVES", "HELD WHOLE" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "HALF-TIME 3-3-2 KICK", "SNARE ON 1 AND 3" }, { "OCTAVE PEDAL", "SILENT" } },
            { "CHUG BAR, CHORD BAR", { "CHUG BAR, CHORD BAR", "CHUG BARS ONLY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "KICK WITH THE CHUGS", "PLAIN BACKBEAT" }, { "FALLING SQUEAL", "SILENT" } },
        };
        for (const auto& list : lists)
            if (t == list.name)
                switch (layer)
                {
                    case L::guitar: return list.guitar;
                    case L::bass: return list.bass;
                    case L::chords: return list.keys;
                    case L::drums: return list.drums;
                    case L::lead: return list.lead;
                    default: return {};
                }
        return {};
    }

    // (0.109.0) The breakdown types' options, lane by lane (the bass's always DOUBLES THE GUITAR, ROOTS HELD, LOCKED WITH
    // THE KICK; only HALF-TIME OPEN CHORDS has a lead list: the others' lead plays its own styles).
    juce::StringArray breakdownApproaches (const juce::String& type, ComposerLayer layer)
    {
        using L = ComposerLayer;
        const auto t = type.trim().toUpperCase();
        struct Lists { const char* name; juce::StringArray guitar, keys, drums, lead; };
        static const Lists lists[] {
            { "CHROMATIC CRAWL", { "CRAWL & HARMONICS", "CRAWL & RAKES", "PLAIN QUARTERS" }, { "HIGH HELD NOTES", "SILENT" }, { "SNARE EVERY TWO BARS", "HALF TIME" }, {} },
            { "HIT & HOLD, THEN CARPET", { "HOLD, THEN EIGHTHS", "HIT & HOLD ONLY", "MUTED EIGHTHS ONLY" }, { "SILENT", "LOW PAD" }, { "HALF TIME, THEN CARPET", "HALF TIME", "CARPET" }, {} },
            { "CHUG & SQUEAL", { "CHUGS & HELD SQUEAL", "CHUG PAIRS & STAB", "SQUEAL ON 2 AND 4" }, { "SILENT", "HELD PADS" }, { "KICK WITH THE CHUGS, RIDE", "KICK WITH THE CHUGS, OPEN HATS" }, {} },
            { "DOTTED TRITONE PUSH", { "SCRATCHES, THEN THE PUSH", "THE PUSH ONLY", "ROOT & TRITONE, SCRATCHES" }, { "SILENT", "HELD PADS" }, { "KICK ON THE PUSHES", "HALF TIME" }, {} },
            { "TRIPLET CLUSTER", { "TRIPLETS, THEN WIDER", "TRIPLETS ONLY" }, { "SILENT", "LOW DRONE" }, { "KICK WITH THE TRIPLETS", "HALF TIME" }, {} },
            { "GAPPED SIXTEENTHS", { "GAPS IN THE SIXTEENTHS", "CHUG STREAM & DARK STABS" }, { "SILENT", "LOW DRONE" }, { "KICK WITH THE CHUGS", "DOUBLE-KICK CARPET" }, {} },
            { "THREES & TWOS", { "3+3+2 STABS & CHUGS", "3+3+3+3+2+2" }, { "SILENT", "STABS ON THE GROUPS" }, { "KICK & CHINA ON THE GROUPS", "GROUPS, THEN CARPET IN TURN" }, {} },
            { "CALL & CHUG", { "BENT CALL, CHUG ANSWER", "OCTAVE STABS, WALK DOWN" }, { "SILENT", "HELD PADS" }, { "KICK WITH THE ANSWER", "HALF TIME" }, {} },
            { "HALF-TIME OPEN CHORDS", { "OPEN CHORDS, SPARSE", "OPEN CHORDS, THEN BUILD" }, { "HELD PADS", "SILENT" }, { "HALF TIME, RIDE", "HALF TIME, THEN BUILD" }, { "MELODY ON TOP", "SILENT" } },
            { "OFF-BEAT GALLOPS", { "GALLOP BURSTS & SQUEAL", "GALLOP BURSTS" }, { "SILENT", "LOW DRONE" }, { "DOUBLE KICK ON THE GALLOPS", "HALF TIME" }, {} },
        };
        for (const auto& list : lists)
            if (t == list.name)
                switch (layer)
                {
                    case L::guitar: return list.guitar;
                    case L::bass: return { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" };
                    case L::chords: return list.keys;
                    case L::drums: return list.drums;
                    case L::lead: return list.lead;
                    case L::melody: return {};
                }
        return {};
    }

    // (0.102.0) The pre-chorus types' options, lane by lane (the bass's last always LOCKED WITH THE KICK; none: the
    // lane plays its own styles).
    juce::StringArray preChorusApproaches (const juce::String& type, ComposerLayer layer)
    {
        using L = ComposerLayer;
        if (type == "BUILD & STOP")
            switch (layer)
            {
                case L::guitar: return { "MUTED EIGHTHS, THEN THE STOP", "ROOTS, DYADS, CHORDS", "STRIKE, CHUG, RING" };
                case L::bass: return { "ROOT EIGHTHS", "DOUBLES THE GUITAR", "PULSES", "LOCKED WITH THE KICK" };
                case L::chords: return { "DOMINANT PEDAL", "HELD PADS", "SILENT" };
                case L::lead: return { "SILENT", "RISING LINE" };
                case L::drums: return { "BEAT INTO A FILL", "RIDE AND A ROLL", "BUILDING SNARE" };
                case L::melody: break;
            }
        if (type == "GUITARS OUT, BASS CLIMBS")
            switch (layer)
            {
                case L::guitar: return { "GUITARS OUT", "HIGH THREE-NOTE CELL" };
                case L::bass: return { "PUSHED ROOTS", "OUT", "LOCKED WITH THE KICK" };
                case L::chords: return { "OCTAVE PAD", "PIANO SIXTEENTHS", "HELD STRINGS" };
                case L::lead: return { "HIGH MOTIF", "SILENT" };
                case L::drums: return { "BROKEN BEAT", "GHOST GROOVE", "HALF TIME" };
                case L::melody: break;
            }
        if (type == "CYCLE IN QUARTERS")
            switch (layer)
            {
                case L::guitar: return { "STILL SHAPE IN THREES", "STILL CLUSTER", "STILL SHAPE, SIXTEENTHS", "GUITARS OUT" };
                case L::bass: return { "ROOT EIGHTHS", "WHOLE NOTES", "LOCKED WITH THE KICK" };
                case L::chords: return { "SILENT", "HELD PADS" };
                case L::lead: return { "QUARTER CYCLE", "SILENT" };
                case L::drums: return { "GHOST GROOVE", "HALF-TIME HATS", "TOM GROOVE" };
                case L::melody: break;
            }
        if (type == "CHUGS UNDER THE SHAPE")
            switch (layer)
            {
                case L::guitar: return { "ROOT CHUGS", "CHUGS ON EVERY EIGHTH" };
                case L::bass: return { "DOUBLES THE GUITAR", "ROOT EIGHTHS", "LOCKED WITH THE KICK" };
                case L::chords: return { "SILENT", "HELD PADS" };
                case L::lead: return { "STILL CLUSTER, AN OCTAVE UP", "STILL SHAPE, AN OCTAVE UP", "SILENT" };
                case L::drums: return { "KICK WITH THE CHUGS", "HALF TIME" };
                case L::melody: break;
            }
        if (type == "STABS & GAPS")
            switch (layer)
            {
                case L::guitar: return { "STABS & GAPS", "STABS EVERY BAR" };
                case L::bass: return { "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" };
                case L::chords: return { "SILENT", "HELD PADS" };
                case L::drums: return { "TOMS WITH THE HITS", "KICK WITH THE HITS" };
                case L::lead:
                case L::melody: break;
            }
        if (type == "RISING BASS")
            switch (layer)
            {
                case L::guitar: return { "TREMOLO CHORDS", "HELD CHORDS" };
                case L::bass: return { "CLIMBS IN SIXTEENTHS", "CLIMBS IN EIGHTHS", "LOCKED WITH THE KICK" };
                case L::chords: return { "SILENT", "HELD PADS" };
                case L::drums: return { "BUILDING KICK", "BEAT INTO A FILL" };
                case L::lead:
                case L::melody: break;
            }
        return {};
    }

    SectionType::Band sectionBand (const ComposerState& s)
    {
        const auto t = sectionTypeCalled (s.sectionType);
        if (! t.has_value() || s.writtenRiffs || ! s.lane (ComposerLayer::guitar).on)
            return SectionType::Band::none;
        return t->band;
    }

    bool staccatoBand (const ComposerState& s)
    {
        return sectionBand (s) == SectionType::Band::staccato;
    }

    juce::StringArray sectionApproaches (const ComposerState& s, ComposerLayer layer)
    {
        if (s.writtenRiffs)
            return {};
        if (const auto t = sectionTypeCalled (s.sectionType); t.has_value() && t->band == SectionType::Band::staccato)
        {
            if (layer == ComposerLayer::bass)
                return { "DOUBLES THE GUITAR", "ROOTS ON THE ACCENTS", "LOCKED WITH THE KICK" };
            if (layer == ComposerLayer::drums)
                return { "KICK ON EVERY CHUG", "KICK ON THE ACCENTS", "DOUBLE-KICK CARPET" };   // (0.100.0: This Fire Burns' verse)
        }
        if (const auto t = sectionTypeCalled (s.sectionType); t.has_value() && t->band == SectionType::Band::split)
        {
            if (layer == ComposerLayer::bass)
                return { "DOUBLES THE HITS", "HOLDS THROUGH THE GAPS", "LOCKED WITH THE KICK" };
            if (layer == ComposerLayer::drums)
                return { "KICK WITH THE HITS", "PLAIN BACKBEAT" };
        }
        if (const auto t = sectionTypeCalled (s.sectionType); t.has_value() && t->band == SectionType::Band::clean)
            switch (layer)
            {
                // (0.100.0, the owner's Another Life verse: PEDAL HOOK, MUTED DYAD WALK, the keys' OCTAVE DRONE, and the band
                // coming in halfway, as its first verse)
                case ComposerLayer::guitar: return { "RINGING ARPEGGIO", "PICKED CHORD", "SPARSE CLEAN NOTES", "GUITARS OUT", "HELD CHORD & PULSE",
                                                     "PEDAL HOOK", "MUTED DYAD WALK", "STILL SHAPE IN THREES", "STILL SHAPE, SIXTEENTHS",
                                                     "STILL CLUSTER" };
                case ComposerLayer::bass: return { "SYNCOPATED ROOTS", "EIGHTHS, CHANGING LATE", "WHOLE NOTES", "OCTAVE EIGHTHS",
                                                   "OUT, THEN IN HALFWAY", "LOCKED WITH THE KICK" };
                case ComposerLayer::drums: return { "LIGHT GROOVE", "HALF TIME, GHOST SNARES", "NO DRUMS", "OUT, THEN IN HALFWAY",
                                                    "HALF-TIME HATS", "TOM GROOVE" };
                case ComposerLayer::chords: return { "HELD PADS", "SIXTEENTH PULSE", "PIANO LINE", "SILENT", "OCTAVE DRONE" };
                // (0.101.0, Circle With Me's first clean guitar) the lead lane's own options under a clean verse; its
                // AUTOMATIC is the lead's style as before (the lane keeps its styles beside these).
                case ComposerLayer::lead: return { "SWELL MELODY", "SWELL, THEN WARBLE" };
                case ComposerLayer::melody: return {};
            }
        if (const auto t = sectionTypeCalled (s.sectionType); t.has_value() && t->band == SectionType::Band::preChorus)
            return preChorusApproaches (t->name, layer);
        if (const auto t = sectionTypeCalled (s.sectionType); t.has_value() && t->band == SectionType::Band::bridge)
            return bridgeApproaches (t->name, layer);
        if (const auto t = sectionTypeCalled (s.sectionType); t.has_value() && t->band == SectionType::Band::breakdown)
            return breakdownApproaches (t->name, layer);
        if (const auto t = sectionTypeCalled (s.sectionType); t.has_value() && t->band == SectionType::Band::interlude)
            return interludeApproaches (t->name, layer);
        if (const auto t = sectionTypeCalled (s.sectionType); t.has_value() && t->band == SectionType::Band::bookend)
            return bookendApproaches (t->name, layer);
        if (const auto t = sectionTypeCalled (s.sectionType); t.has_value() && t->band == SectionType::Band::chorus)
            return chorusApproaches (t->name, layer);
        if (layer == ComposerLayer::guitar)
            if (const auto t = sectionTypeCalled (s.sectionType); t.has_value() && ! t->guitarFlavours.empty())
            {
                juce::StringArray names;   // (the type's riff kinds)
                for (const auto f : t->guitarFlavours)
                    names.add (riffFlavourName (f));
                return names;
            }
        const auto& flavours = riffFlavours();
        return sectionApproaches (flavours[(size_t) juce::jlimit (0, (int) flavours.size() - 1, s.riffFlavour)], layer);
    }

    juce::String approachName (const ComposerState& s, ComposerLayer layer)
    {
        const auto names = sectionApproaches (s, layer);
        if (names.isEmpty())
            return {};
        const auto a = s.lane (layer).approach;
        return a >= 0 ? names[a % names.size()] : juce::String ("AUTOMATIC");
    }

    void applySectionType (ComposerState& s, const juce::String& sectionName, const juce::String& type)
    {
        const auto& flavours = riffFlavours();
        const auto indexOf = [&flavours] (RiffFlavour f)
        { return (int) std::distance (flavours.begin(), std::find (flavours.begin(), flavours.end(), f)); };
        for (auto& l : s.lanes)
            l.approach = -1;
        if (const auto t = sectionTypeNamed (sectionName, type))
        {
            s.sectionType = t->name;
            s.riffFlavour = indexOf (t->flavour);
            s.writtenRiffs = false;
        }
        else
        {
            s.sectionType = {};
            s.riffFlavour = indexOf (RiffFlavour::any);
        }
    }

    const std::vector<SectionTransition>& sectionTransitions()
    {
        static const std::vector<SectionTransition> list {
            { noTransition, "Straight into the next block" },
            { "STOP", "Everyone hits the last bar's first beat together, then silence" },
            { "BREATH", "Everyone stops for the last beat" },
            { "SNARE ROLL", "Sixteenth snares from the and of three, getting louder" },
            { "DRUMS BUILD", "The last bar the drums alone, building eighths into sixteenths" },
            { "BASS ALONE", "The last bar the bass alone" },
            { "SILENT BAR", "The last bar silent (the singer alone)" },
            { "WALK DOWN", "The bass and guitar walk down a half step at a time into the next chord" },
            { "WALK UP", "The bass and guitar walk up a half step at a time into the next chord" },
            { "PINCH SLIDE", "The guitar's pinch harmonic sliding down over the last two beats" },
            { "MUTED LINK", "The last two bars palm-muted eighths, the keys and lead out" },
            { "SHORT BAR: 1 BEAT", "The last bar one beat long, the band hitting it together", 1 },
            { "SHORT BAR: 2 BEATS", "The last bar two beats long, the band hitting each beat together", 2 },
            { "SHORT BAR: 3 BEATS", "The last bar three beats long, the band hitting each beat together", 3 },
        };
        return list;
    }

    std::optional<SectionTransition> sectionTransitionNamed (const juce::String& name)
    {
        if (name.isEmpty() || name.equalsIgnoreCase (noTransition))
            return std::nullopt;
        for (const auto& t : sectionTransitions())
            if (t.name.equalsIgnoreCase (name))
                return t;
        return std::nullopt;
    }
}
