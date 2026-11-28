#include "engine/SectionTypes.h"

#include "engine/SongSections.h"
#include "engine/LaneWorlds.h"
#include <map>
#include <mutex>
#include <tuple>

namespace pr7s
{
    static std::vector<SectionType> sectionTypesUncached (const juce::String& sectionName)
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
            std::vector<SectionType> types { { "BUILD & STOP", "The verse's drive pushed toward the chorus, then everything stops for a beat before it lands", RiffFlavour::openChords, {}, pre },
                     { "GUITARS OUT, BASS CLIMBS", "The guitars drop out; the bass climbs in long pushed notes, keys and a high motif carry it", RiffFlavour::openChords, {}, pre },
                     { "CYCLE IN QUARTERS", "A still clean shape in eighths with a three-note cycle in quarters over it, ending on a two-bar stop", RiffFlavour::openChords, {}, pre },
                     { "CHUGS UNDER THE SHAPE", "Low muted chugs with the bass and kick on them, the verse's still shape played high over them", RiffFlavour::openChords, {}, pre },
                     { "STABS & GAPS", "The whole band on short stabs together, then silence for the voice, a pickup into the next bar", RiffFlavour::openChords, {}, pre },
                     { "RISING BASS", "Chords held or tremolo picked while the bass climbs a step a bar underneath", RiffFlavour::openChords, {}, pre },
                     // (0.123.0) From the owner's rock songs (shapes only; our names).
                     { "SHIFTING BURSTS, KICK JOINS", "Muted bursts of four sixteenths that move a beat later each bar, half time at first, then the kick joins every burst", RiffFlavour::openChords, {}, pre },
                     { "SNARE CARPET BUILD", "No kick: the snare and a crash on every sixteenth for a few bars over the riff, then the groove comes back with a shaker", RiffFlavour::openChords, {}, pre },
                     { "PEDAL WITH OPEN-HAT BEATS", "The low root on every sixteenth but the downbeat, climbing into the next bar, with open hats on the beats", RiffFlavour::openChords, {}, pre },
                     { "CHROMATIC CHUG, TREMOLO TRIAD", "A chug rocking between the root and the half step above, a high triad tremolo picked over it, into a snare roll", RiffFlavour::openChords, {}, pre },
                     { "OFF-BEAT SNARE, LOW BURSTS", "The snare and an open hat on every off eighth, no backbeat, under low muted bursts", RiffFlavour::openChords, {}, pre },
                     { "THREES ACROSS THE BAR", "A waltz feel kept in 4/4: three-beat cells that run across the bar lines, straight eighths from halfway", RiffFlavour::openChords, {}, pre  },
                     // (0.141.0) From the owner's 61 song files (shapes only; our names).
                     { "KICK OUT, LOW TREMOLO", "The kick out: low dyads tremolo-picked rocking a half step, hats and the snare only", RiffFlavour::openChords, {}, pre },
                     { "UNISON ROCKING RIFF", "The guitars and the bass in unison on a riff rocking between the root and the step under it", RiffFlavour::openChords, {}, pre },
                     { "RING & STAB UNDER CYMBAL EIGHTHS", "A chord rung each half bar and a high stab, a cymbal on every eighth, a high bent note over it", RiffFlavour::openChords, {}, pre },
                     { "UNISON EIGHTHS BUILD", "Everyone on every eighth, the kick, snare and floor tom together, then hits and a roll into the chorus", RiffFlavour::openChords, {}, pre },
                     { "TREMOLO PEDAL, THEN OCTAVE PUMP", "The root tremolo-picked under held chords, then the root and its octave pumped in eighths", RiffFlavour::openChords, {}, pre },
                     { "PAIRS IN 3+3+2, RING BAR", "Pairs of sixteenths on 3+3+2, every fourth bar a chord rung", RiffFlavour::openChords, {}, pre },
                     { "OCTAVES ROCKING TO THE TRITONE", "Low octaves rocking between the root and its tritone, the bass with them", RiffFlavour::openChords, {}, pre },
                     // (0.149.0) Round four (shapes only, our names).
                     { "STOP BARS IN TURN WITH THE RIFF", "Two bars of three dotted stabs, two bars of the riff, again, then a build of pushes", RiffFlavour::openChords, {}, pre },
                     { "MARCHING SNARE OVER HELD CHORDS", "No kick or cymbals: a soft marching snare under whole-note chords, stopping after beat 2", RiffFlavour::openChords, {}, pre },
                     { "DRUMS OUT, KEYS PULSE AND PUSHES", "Piano chords in halves, guitar pushes in each bar's second half, the drums back halfway", RiffFlavour::openChords, {}, pre },
                     { "SIXTEENTH ROOTS STEPPING DOWN", "A root stream one root a bar falling, then bursts of six and five, then high clashing stabs", RiffFlavour::openChords, {}, pre },
                     { "SIDESTICK DROP", "Only a kick on the 3+3+2 anchors and a side stick, root octaves on the anchors, a stop after the and of 2", RiffFlavour::openChords, {}, pre },
                     { "HIGH PEDAL IN 3+3+2 OVER A MOVING BASS", "High picked eighths on 3+3+2 over bass sixteenths on each new root", RiffFlavour::openChords, {}, pre },
                     // (0.183.0) Round six (the owner's 25 songs; shapes only, our names).
                     { "PIANO EIGHTHS, CLAPS ON THE BEATS, GUITARS RING", "No kick or snare: claps on every beat, piano chords in eighths, a guitar chord rung each bar with a pickup pair at its end", RiffFlavour::openChords, {}, pre },
                     { "QUARTER CHORDS OVER KICK EIGHTHS", "A full chord on every quarter, the last pushed, over the kick on every eighth and soft quarter claps that grow louder", RiffFlavour::openChords, {}, pre },
                     { "KICK EIGHTHS, THEN SNARE EIGHTHS", "The kick on the front half's eighths and the snare on the back half's, over a held chord and root eighths", RiffFlavour::openChords, {}, pre },
                     { "TRITONE CHORDS BETWEEN ROOT SINGLES, SNARE CLIMB", "Power chords a tritone and a fifth over the root between low root singles, the snare climbing in eighths", RiffFlavour::openChords, {}, pre },
                     { "DOTTED KICK, SNARE PAIRS, MUTED ROOT STABS", "The kick on a dotted 3+3+2 twice, snare pairs on 2 and 4, muted root stabs off the beat", RiffFlavour::openChords, {}, pre },
                     { "SIXTEENTHS UP A HALF STEP AFTER SIX", "Sixteenths with the bass: six on the root, ten a half step up, every bar, over ride sixteenths and snare eighths", RiffFlavour::openChords, {}, pre },
                     // (0.196.0) Round seven (the owner's 70 songs; shapes only, our names): three led by the ACOUSTIC lane
                     // (applying one switches it on), then two electric.
                     { "STRUMS AND STRINGS, DRUMS OUT", "An acoustic's quarter accents with light ups, two chords a bar climbing to the chorus, strings on 1 and 3, the drums out", RiffFlavour::openChords, {}, pre },
                     { "ACOUSTIC EIGHTHS, THEN A STUTTER", "An acoustic's full chords in eighths, every fourth bar a stutter: two chords pushed together, low strums, a sixteenth run; the kick and open hats on every eighth", RiffFlavour::openChords, {}, pre },
                     { "FULL ACOUSTIC EIGHTHS, 3+3+2 HIT, STOP", "An acoustic strumming every eighth, a bar of three held hits on 3+3+2, the last chord struck once and held; the band on the hits", RiffFlavour::openChords, {}, pre },
                     { "OFF-THE-ONE EIGHTHS, BASS ON THE EDGES", "A bar of quarter chords, then a bar of eighths coming in after the one; the bass only on the entry and the pickup", RiffFlavour::openChords, {}, pre },
                     { "FOUR-STAGE BUILD", "Root eighths alone over quarter kicks, then a stab cell joins, then everyone on eighths over a backbeat, then the snare on every quarter", RiffFlavour::openChords, {}, pre },
                     // (0.215.0) Round eight (the owner's 67 songs; shapes only, our names).
                     { "WHOLE-NOTE LINE FALLING, FOUR ON THE FLOOR", "The key's chord held while the bass falls a half step a bar under it, the kick on every quarter and no snare", RiffFlavour::openChords, {}, pre },
                     // (0.236.0) Round nine (the owner's 17 industrial songs; shapes only, our names).
                     { "RING ONCE, THE BASS MOVES THE CHANGES", "The key's chord struck once and left to ring while the bass moves the changes in eighths under it", RiffFlavour::openChords, {}, pre },
                     { "GAPPED QUARTERS OVER THE DISCO HAT", "Short power chords on 1, 2 and 4 with a gap on 3, over four on the floor and disco hats: closed on the beat, open on the and", RiffFlavour::openChords, {}, pre },
                     { "HELD HORN LINE OVER THE RIDE GALLOP", "A brassy note held from the and of 2, a step lower each bar, over power-chord eighths and a galloping ride", RiffFlavour::openChords, {}, pre },
                     // (0.242.0) Round ten (the owner's 56 songs; shapes only, our names).
                     { "HALF-BAR CHANGES INTO A 3+2+3 STOP", "Chords changing every half bar, the next one arriving early, the last bar three held hits on 1, the and of 2 and 4 with the band", RiffFlavour::openChords, {}, pre },
                     { "STOP-TIME HITS AND FILLS", "A bar of the chord and the bass and kick on 1, the and of 2, the and of 3 and 4, then a bar left to a drum fill; soft dyads and a pickup to end", RiffFlavour::openChords, {}, pre },
                     { "TRIADS CLIMBING BY STEP", "The keys' triads in eighths climbing a scale step at a time into the chorus, the bass climbing under them, toms on the e's to end", RiffFlavour::openChords, {}, pre },
                     { "VERSE RIFF, THE HATS DO THE BUILD", "The verse's guitar and bass unchanged; the hats alone build, from quarters to eighths to sixteenths", RiffFlavour::openChords, {}, pre },
                     // (0.254.0) Round eleven (the owner's 61 more songs; shapes only, our names).
                     { "BASS AND GUITAR TRADE BEATS IN SIXTEENTHS", "The bass on beats 1 and 3 in sixteenths, a high guitar note on beats 2 and 4, the kit building from hats to crash eighths", RiffFlavour::openChords, {}, pre },
                     { "UNISON HITS ON ONE, ONE-AND, TWO, THREE, THREE-AND, FOUR-AND", "Guitars and bass held between unison hits, the kick and crash with them, the last bar toms in eighths", RiffFlavour::openChords, {}, pre },
                     { "HELD HIGH NOTE OVER THE BASS CLIMB", "One fifth dyad held over the bass's sixteenth climb, a high answer every other bar, a sixteenth build at the end", RiffFlavour::openChords, {}, pre },
                     { "MUTED CHORD EIGHTHS, THE CHANGE ON FOUR", "Muted power chords on every eighth, the last two already the next chord, a dotted line over them", RiffFlavour::openChords, {}, pre },
                     { "ADD-NINE CHORDS HELD, SOFT CRASH QUARTERS", "Open add-nine chords held, a chord every two bars, over a half-time beat with soft crash quarters", RiffFlavour::openChords, {}, pre },
                     { "SNARE ON EVERY BEAT, KICK ON THE ANDS, HELD CHORDS", "Big chords held two bars over the snare on every beat and the kick on every and, a root run into the chorus", RiffFlavour::openChords, {}, pre },
                     { "SNARE QUARTERS ALONE UNDER A RISING HALF-STEP PAD", "A pad rising a half step a bar, the snare's quarters alone for a stretch with no kick or cymbals, then the beat back", RiffFlavour::openChords, {}, pre },
                     { "THE BURSTS STAY, KICK ON ONE, THREE, THREE-AND", "Big strums on acoustic and distorted guitar in a pushed figure, the sidestick bursts kept, the kick on 1, 3 and 3-and", RiffFlavour::openChords, {}, pre },
                     { "POWER-CHORD SIXTEENTHS UP TO THE FLAT FIVE AND BACK", "Power chords on every sixteenth climbing root, minor third, four, flat five and back down, a stop at the end", RiffFlavour::openChords, {}, pre },
                     { "FLAT-TWO HITS, A TOM RUN UNDER THE SIXTEENTHS", "The flat two's power chord on 1 and 2, then six sixteenths through the flat seven and the root, a tom run under them", RiffFlavour::openChords, {}, pre },
                     { "HALF-BAR CHANGES, THEN TWO BARS HELD OVER THE GALLOP", "A gallop on every beat, the chords changing each half bar, then the five held two bars", RiffFlavour::openChords, {}, pre },
                     { "TWO BARS OF OCTAVE TREMOLO ON THE FLAT SIX", "The root and octave of the flat six on every sixteenth, the bass in sixteenths on the same note, one high keys note", RiffFlavour::openChords, {}, pre },
                     { "HITS ON ONE, TWO, TWO-AND, FOUR, FOUR-AND, CRASH QUARTERS", "Heavy hits on 1, 2, 2-and, 4 and 4-and held to the next, the kick with them, the snare on 3, crash quarters", RiffFlavour::openChords, {}, pre } };
            for (auto& t : sectionTypesFor ("Verse"))   // (0.118.0: the whole verse list, its new types too)
                types.push_back (std::move (t));
            return types;
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
                { "CHROMATIC WOBBLE", "Muted low sixteenths wobbling a half step in odd groups across the bar line", r, {}, i  },
                // (0.141.0) From the owner's 61 song files (shapes only; our names).
                { "DOWN TO THE LOOP", "Down to a loop: a one-note synth arpeggio, a sequenced synth bass, high guitar bursts, a broken beat", r, {}, i },
                { "HATS AND ROOTS", "Only busy closed hats and the chorus's roots in halves, two high notes a bar over them", r, {}, i },
                { "HITS ON FOUR", "The band hits once a bar, on beat 4, and lets it ring into the next", r, {}, i },
                { "UNISON CHROMATIC RUNS", "Everyone on sixteenth runs climbing a half step at a time in odd groups, closed by quarter-note hits", r, {}, i },
                { "DOTTED NOTE OVER THE RIFF", "One high note every three sixteenths over the bass's sixteenth pedal riff", r, {}, i },
                { "SNARE ONLY ON FOUR AND FOUR-AND", "Chords held two bars with high stabs answering, the snare only on beat 4 and its and", r, {}, i },
                // (0.149.0) Round four (shapes only, our names).
                { "OFF-BEAT LOW DYADS UNDER A HIGH DRONE", "Muted low dyads on off-beat sixteenths, the root rung high, the bass busy round the root and flat seven", r, {}, i },
                { "CLAPS AND ONE GUITAR", "No kick: the snare and claps on every beat with a tambourine, one guitar's hook figure; keys and bass come in late", r, {}, i },
                { "BASS RIFF ALONE UNDER A HELD HIGH NOTE", "The bass's riff alone under one held high note, then the guitar joins", r, {}, i },
                // (0.170.0) Round five (the owner's 12 songs; shapes only, our names).
                { "NINTH CLASH IN EIGHTHS", "A root-and-ninth cluster on every eighth with the bass a whole step above it, crashes on the beats", r, {}, i },
                { "ONE CHORD RUNG OVER THE INTRO BEAT", "One open chord struck and rung eight bars over a half-time hat groove, the bass holding the root", r, {}, i },
                { "HELD SECOND CLUSTER, SIXTEENTH PICKUP", "A ninth-and-third cluster held each bar, a sixteenth pickup into the next chord, a tom groove", r, {}, i },
                // (0.196.0) Round seven (the owner's 70 songs; shapes only, our names): three led by the ACOUSTIC lane
                // (applying one switches it on), then four electric.
                { "ACOUSTIC UNDER THE RIFF", "An acoustic's chord-to-pedal strum under a low single-note riff, the bass doubling the riff and alone every fourth bar", r, {}, i },
                { "STRUM, THEN THE LOW-STRING WALK", "An acoustic's chord on 1 and its own bass line one bar, a climb of single notes and a pickup strum the next", r, {}, i },
                { "HIGH THIRDS OVER THE ARPEGGIO", "An acoustic's arch arpeggio over the chord's fifth, a second acoustic's high thirds in eighths over it, a ride bell", r, {}, i },
                { "BURST ON ONE UNDER HELD TRIADS", "A five-sixteenth tremolo burst on beat 1, then silence, under a high triad held each bar; kick, snare and open hat together on 2 and 4", r, {}, i },
                { "BELL ON THE DOTTED QUARTERS, GUITAR ALONE", "One guitar's power eighths over a ride bell on the dotted quarters, the bass out; the band takes it from halfway", r, {}, i },
                { "BASS BREAK, KICK ON THE QUARTERS", "The bass alone in sixteenth pairs over the kick on the quarters and closed hats, no snare, then a unison fill", r, {}, i },
                { "ALTERNATING BLAST UNDER TREMOLO CHORDS", "Tremolo-picked chords a bar each, the snare on the eighths and the kick between them, then kick sixteenths, a kick roll and two stabs", r, {}, i },
                // (0.215.0) Round eight (the owner's 67 songs; shapes only, our names).
                { "LOW PEDAL EVERY THIRD SIXTEENTH, CHROMATIC PAIRS ABOVE", "The key's low root on a 3+3+2 grid, chromatic pairs an octave up between, the bass on every sixteenth, the ride's bell on the eighths", r, {}, i },
                // (0.242.0) Round ten.
                { "ANTHEM HITS ON TWO, THREE-AND, FOUR-AND", "One chord a bar hit on 2, the and of 3 and the and of 4 by the whole band, root eighths under it, the last chord held two bars", r, {}, i },
                // (0.254.0) Round eleven.
                { "LOW AND HIGH IN 3+3+2, UP A HALF STEP", "A low power chord, its high octave and low again in 3+3+2, rising a half step each half bar, a crash on every hit", r, {}, i },
                { "PICKED MELODY FALLING BY HALF STEPS", "An acoustic picking one line in eighths, a top note falling by half steps in pairs to the chord, the verse's kit under it", r, {}, i },
                { "KEYS CHORD EIGHTHS OVER A FALLING BASS, BAND OUT", "The band out but the keys: chords on every eighth with the bass note falling under them, a snare pickup at the end", r, {}, i },
                { "HALF-NOTE DYADS, AN ORGAN ANSWER", "Root-fifth dyads on 1, 2-and and 4-and, an organ answering in the second half of every other bar, four on the floor", r, {}, i },
                { "CHORD ON ONE, PUSHES ON THE A OF ONE AND THREE", "A chord on 1 then pushes on the a of 1 and 3 and their neighbours, the next bar the same a step down, crash quarters", r, {}, i },
                { "UNISON STAB FIGURE ON ONE, SHAKER AFTER", "Every lane on one stab figure on 1 (1, 1-and, 1-a, 2-e), then only the shaker; a groove bar between", r, {}, i },
                { "UNISON LINE, KICK AND CRASH ON EVERY NOTE", "Four bars: the hook over a held chord, a unison line down the scale with kick and crash on every note, the five held, a tom bar", r, {}, i },
                { "SNARE OUT, TWIN TRILL OVER SIXTEENTH HATS", "Two guitars trilling root and the step above in sixteenths, the second a sixteenth late, over hats with no snare", r, {}, i },
                { "THREE CHORD PAIRS, THEN A LEAD BAR", "A bar of three chord pairs on eighths, then a bar left to the lead with only a drum pickup", r, {}, i } };
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
                         // (0.124.0) From the owner's rock songs (shapes only; our names).
                         { "BASS RIFF WITH TOMS", "The bass alone on the riff with the toms doubling it, a high tick on the quarters; the kick and snare late", r, {}, b },
                         { "PEDAL SEESAW, DOUBLE KICK", "Eighth chugs rocking from a low pedal to the notes above it over a double kick, a burst every fourth bar", r, {}, b },
                         { "TWO CELLS IN THREES", "A three-note cell in sixteenths on the guitar following the chords, a stepping keys cell over it that doesn't", r, {}, b },
                         { "KIT HITS IN PAIRS", "The kit and the guitars together in pairs of sixteenths on the dotted grid, then a bar of space", r, {}, b },
                         { "GAPPED PEDAL, BUILD INTO THE VERSE", "The low root in sixteenths with gaps on 2 and 4, the kick on every chug, a two-bar build into the verse", r, {}, b },
                         // (0.143.0) From the owner's 61 song files (shapes only; our names).
                         { "HOOK KEPT, BAND BUILT UNDER IT", "A hook alone that carries on unchanged while the drums, then the bass, then the riff come in under it", r, {}, b },
                         { "RIFF GROWS FROM THE RIDE", "The ride alone in sixteenths, then a high stab, then half the riff, then all of it with the kit", r, {}, b },
                         { "TOM GROOVE ALONE, THEN THE RIFF", "The toms and the kick in 3+3+2 with high stabs after the beat, then the band on the riff", r, {}, b },
                         { "TOMS ON THE CELL", "A six-sixteenth cell laid 6+6+4: the toms on its first two, the kick, snare and the guitar's dyad on the rest", r, {}, b },
                         { "PADS, THEN THE MACHINE LOOP", "Pads alone, then a piano ostinato and a machine loop with the pedal hat on every sixteenth", r, {}, b },
                         { "HEARTBEAT KICK", "The kick alone like a heartbeat, a soft tremolo note coming in, then the toms building", r, {}, b },
                         { "DRONE, DRUMS, THEN THE RIFF", "One clashing note held alone, then the drums alone, then the riff with the band", r, {}, b },
                         { "KEYS ALONE, THEN THE RIFF", "A sixteenth keys figure alone, then the riff and the band come in under it", r, {}, b },
                         // (0.151.0) From the owner's 54 new song files (round four; shapes only, our names).
                         { "SNARE-ROLL GROOVE, THE BAND JOINS", "The drums alone with the snare on every sixteenth, then the riff, then octaves in sixteenths over it, a stop to end", r, {}, b },
                         { "HYMN, THEN DOUBLE KICK UNDER IT", "Organ chords alone with a timpani roll, then hits and pickups over the kick on every sixteenth", r, {}, b },
                         { "QUARTER STRUMS OVER AN OFF-SIXTEENTH SNARE", "Full chords strummed on the beats, a high tone on 2, the snare off the sixteenths over the pedal hat", r, {}, b },
                         { "SIDE STICK AND HATS ON THE RIFF", "No kick or snare: the side stick and closed hats exactly on the riff, a shaker in sixteenths", r, {}, b },
                         { "HAND PERCUSSION ALONE, THEN THE RIFF", "Claps, tambourine and cowbell alone, then the muted riff, then the kit and the bass", r, {}, b },
                         { "ONE CHORD STRUMMED 3+3+3+3+4, RIFF JOINS", "The key's chord strummed on 3+3+3+3+4 alone, then the low riff and the bass under it", r, {}, b },
                         { "RIFF ALONE, SIXTEENTH ROOT PEDAL JOINS", "The riff alone, then the bass's root in sixteenths, then the hats, then the kick", r, {}, b },
                         { "SWELLS, THEN A FIXED HIGH FIGURE, THE KIT LAST", "Clean chords swelling, then a fixed high three-note figure with bass eighths, the hats, the kit last", r, {}, b },
                         // (0.185.0) Round six (the owner's 25 songs; shapes only, our names).
                         { "THE SAME CELL IN CANON", "One guitar on a short single-note cell round the root, a second (and third) guitar playing it three and six sixteenths later: a rolling sixteenth mesh", r, {}, b },
                         { "TOM BURSTS UNDER THE HALF-STEP RIFF", "A sixteenth cell rocking between the root and the half step over it, the floor tom and bursts of rack tom under it, no kick or snare", r, {}, b },
                         { "OSTINATO WITH GUITAR-AND-KICK HITS", "A three-note chromatic keys ostinato in eighths, the guitars, kick and splash together on a few scattered beats", r, {}, b },
                         { "CLAP CLAVE OVER SIXTEENTH HATS", "The drums alone: hats on every sixteenth, a clap clave, the kick on 1 and late in the bar; the band out or in later", r, {}, b },
                         { "HORN LINE ALONE, CRASH ON ONE", "A keys line in octaves alone, stepping up in eighths into a held note, one crash on the first 1; the band in on beat 3 of the last bar", r, {}, b },
                         { "TWO ROOTS AND A RUNG CHORD", "Two low roots and a ringing add-nine chord on every beat, the shape fixed over the pedal, a pickup on the and of 4 every fourth bar", r, {}, b },
                         // (0.198.0) Round seven (the owner's 70 songs; shapes only, our names): five led by the ACOUSTIC lane
                         // (applying one switches it on), then two electric.
                         { "ACOUSTIC ALONE, SHAKER AND HATS JOIN", "The verse's acoustic part alone, then closed hats and a shaker join halfway, a kick every four bars, strings swelling at the end", r, {}, b },
                         { "CHORUS CHORDS STRUMMED, THEN A TONIC LINK", "The chorus's chords strummed in growing fifth stacks with the band out, then two bars of the key's root and fifth in eighths alone", r, {}, b },
                         { "STRUMMED TRIADS IN SIXES, ALONE", "Close triads strummed in six-sixteenth cells (6+6+4 across the bar), a chord a bar, alone", r, {}, b },
                         { "NYLON DOUBLES THE INTRO, THEN LEAVES", "A picked arpeggio in 3+3+2 under a fixed top, a nylon doubling the electric for the first half, then leaving it alone", r, {}, b },
                         { "EMBELLISHED STRUM ALONE", "A two-bar one-chord strum with flicks alone, a slow line climbing the chord over it; the bass and a side stick from halfway", r, {}, b },
                         { "HIGH ONE-NOTE PULSE OVER A TRITONE BASS", "The key's note high in steady eighths, the bass holding the flat five with the root on 4-and, the ride and a kick, no snare", r, {}, b },
                         { "HELD TRIADS STEPPING DOWN, TAMBOURINE", "High triads held a bar, stepping down the key, a choir with them; the kick on 1 and 3, the ride, a tambourine off the beat", r, {}, b },
                         // (0.216.0) Round eight (the owner's 67 songs; shapes only, our names).
                         { "ROOT-AND-THIRD EIGHTHS ALONE, THE KIT IN STAGES", "A guitar's eighths rocking from the low root to the key's third alone, then hats, then the kick, then kick eighths, then everyone", r, {}, b },
                         { "BASS FLICKS UNDER A KEYS RUN", "The bass alone with three-sixteenth flicks on 1 and 3, then an organ's eight-note sixteenth run over it", r, {}, b },
                         // (0.236.0) Round nine (the owner's 17 industrial songs; shapes only, our names).
                         { "MUSIC-BOX OSTINATO, THE KIT BUILDS UNDER", "A high music-box figure on every eighth alone, then a side stick, a kick and sixteenth hats join a layer at a time", r, {}, b },
                         { "TRIBAL KICK EIGHTHS, FALLING BASS CELL", "The kick on every eighth under accented toms, the bass falling three notes each beat, a metallic stab every two bars", r, {}, b },
                         { "SPARSE BASS AND A HIGH RUB OVER KICK QUARTERS", "The kick on the quarters alone, a sparse bass root and fifth, a high note held against the half step above it", r, {}, b },
                         // (0.243.0) Round ten (the owner's 56 songs; shapes only, our names).
                         { "TWO GUITARS INTERLOCK, NO DRUMS", "Two clean guitars interlocking on the chords, one on the beats and one between, the bass and drums out", r, {}, b },
                         { "A CAPPELLA HOOK, THEN THE BAND", "The hook sung alone over nothing, the band's pickup on the last bar's 4 into the next section", r, {}, b },
                         { "PAD ALONE, THEN THE BASS PEDAL, THEN THE BAND", "A pad holding the chords alone, the bass coming in on the key's root a third of the way in, the band for the last third", r, {}, b },
                         { "PIANO ONE-NOTE PEDAL INTRO", "The piano repeating one note in sixteenths, the strings holding it, a clean guitar and a kick from halfway", r, {}, b },
                         { "INSTRUMENTAL VERSE ON A BROKEN TRIAD", "The verse's groove played with a guitar breaking each triad off the one, before anyone sings", r, {}, b },
                         { "A KIT HIT ON THE AND OF FOUR", "Almost silence: the hats counting, then the whole band hitting the last bar's and of 4 into the next", r, {}, b },
                         // (0.257.0) Round eleven (the owner's 61 more songs; shapes only, our names).
                         { "THE RIFF, A KICK ON ONE, THEN EVERYONE", "The guitar's muted riff alone, then the bass's root and a lone kick on 1 joining halfway, a held high note to end", r, {}, b },
                         { "ONE BAND HIT, THEN THE ARPEGGIO ALONE", "One band hit rung, a kick and crash, a lone bass note, then a clean sixteenth arpeggio alone in two octaves", r, {}, b },
                         { "SLOW CHROMATIC DESCENT, HIGH DYAD PEDAL OVER", "The bass alone falling a half step at a time, power chords joining on the same roots, a high root-and-fifth dyad on the eighths to finish", r, {}, b },
                         { "PICKED SIXTEENTH FIGURE UNDER A HIGH DYAD STAB", "One guitar alone: a high two-note stab, a rest, then a picked sixteenth figure round the low root and fifth", r, {}, b },
                         { "KEYS CHORD EIGHTHS ALONE, THEN THE PICKUP", "The keys alone on chord eighths with the bass note falling under them, the drums out but a pickup on the last bar", r, {}, b },
                         { "SNARE ON EVERY CHUG, THEN THE KICK", "Root chugs with a chord on 4-and, the snare on exactly every guitar hit, the kick joining on 1 and 3 from halfway", r, {}, b },
                         { "THE RIFF ALONE, QUARTER HITS, A TOM FILL", "Four-bar cycles: the riff alone for two bars, a bar of band hits on the quarters, then a sixteenth snare-and-tom fill", r, {}, b },
                         { "TWO-SIXTEENTH STAB OVER HAND PERCUSSION", "Two sixteenths on beat 1 with the kick and crash, the rest of the bar silent but congas in sixteenths", r, {}, b },
                         { "HELD CHORD OVER HAT SIXTEENTHS, THEN THE RIFF ALONE", "The low tonic chord held over closed-hat sixteenths, then the riff alone over open-hat eighths, a snare pickup into the band", r, {}, b },
                         { "HIGH NOTE IN THREES AND A FOUR", "One high root note on 3+3+3+3+4 sixteenths, held every fourth bar, the band out", r, {}, b },
                         { "INNER LINE OVER A HELD ROOT, FOUR ON THE FLOOR", "A clean low root held while a second note moves in half notes, root quarters on the bass, four on the floor with open hats", r, {}, b },
                         { "PAD BAR, PIANO OCTAVE ANSWER BAR", "A sus chord held on a pad, then a bar of the piano in octaves falling through four notes, in turn", r, {}, b },
                         { "BRASS HOOK OVER HELD STRINGS, NO DRUMS", "Strings holding the chord a bar with a short hook on top, a bass pedal, the drums out but a tom pickup in the last bars", r, {}, b },
                         droning, open };
            }
            case 1:
            {
                // (0.118.0) Ten more from the owner's song files (shapes only, all in 4/4; our names), before DRONING
                // and SPARSE.
                const auto v = SectionType::Band::verse;
                const auto r = RiffFlavour::openChords;
                return { staccato, split, clean, muted,
                         { "PICKED LINE, KICK IN THREES", "One guitar picking single notes round the root on every eighth, the kick three times a bar (3+3+2) with the snare between", r, {}, v },
                         { "PICKED OSTINATO", "The power chords out: a picked single-note cell (or a high tremolo) over a held-back full beat", r, {}, v },
                         { "SKANK LINE", "A single-note line on every eighth over a pedal, the skank beat under it", r, {}, v },
                         { "LINE OVER HITS", "The skank line over a chord struck on each bar's one, the kick and crash only on the hits", r, {}, v },
                         { "THREE BARS AND A STOP", "Three bars of sixteenth chugs over four on the floor, the fourth one hit and silence", r, {}, v },
                         { "SIDESTICK PULSE", "Low single eighths and a side stick on the beats, the bass coming in halfway", r, {}, v },
                         { "OFF-BEAT SNARE", "A chord rung every other bar (or low bursts), the snare and an open hat on the off-beats, no backbeat", r, {}, v },
                         { "CHUGS, THEN DOTTED HITS", "Sixteenth chugs with holes for three bars, then dotted-quarter hits, the kick displaced under them", r, {}, v },
                         { "PEDAL RIFF IN SIXTEENTHS", "One guitar's sixteenth riff on a low pedal climbing the minor pentatonic, a plain rock beat under it", r, {}, v },
                         { "TRESILLO RIFF", "Power chords on a 3+3+2 figure in the first half bar, a high ostinato over it, hats and a sparse kick", r, {}, v },
                         // (0.139.0) Twelve more from the owner's 61 song files (shapes only, all in 4/4; our names).
                         { "BASS RIFF, HIGH NOTES RUNG", "The bass carries the riff in sixteenths, the guitar only rings high notes over it", r, {}, v },
                         { "RIFF BAR, HOOK BAR", "One bar the bass and the kick on a 3+3+2 figure, the next bar only a keys hook", r, {}, v },
                         { "DRUMS, BASS, GUITAR IN TURN", "The drums alone, then the bass, then the guitar's chugs, then a line: each a quarter of the verse", r, {}, v },
                         { "TOMS AND OCTAVE BASS", "No snare: the toms stepping down the kit, the bass jumping the octave, the guitar one rung chord", r, {}, v },
                         { "TOM FIGURE VERSE", "A two-bar figure on the toms with the snare on 3, the guitars out, then chugging on the tom hits", r, {}, v },
                         { "SIX-SIXTEENTH CELLS", "Root-and-tritone cells of six sixteenths laid 6+6+4 in each bar, the bass with them, the kick in pairs", r, {}, v },
                         { "HOLED SIXTEENTH PEDAL", "The muted root on a sixteenth stream with two holes, the kick (and toms) on the same stream", r, {}, v },
                         { "GUITAR CALLS, BASS ANSWERS", "The guitar's riff for three beats, then the bass answers alone in the gap", r, {}, v },
                         { "THREE-BAR CYCLE", "Nine chugs and three rests in a cell that runs across the bar lines, coming round every three bars, no backbeat", r, {}, v },
                         { "THREE-BAR RIFF, KICK AND BASS ON THE BEATS", "A guitar riff three bars long over the kick and the bass on beats 1 to 3, the snare only on 3", r, {}, v },
                         { "KICK AND BASS IN UNISON", "The guitars out: the kick and the bass on one syncopated line, the guitars coming in with bursts after 2 and 4", r, {}, v },
                         { "BASS GROOVE, GUITAR SQUEALS", "The bass's sixteenth groove over a funk kick, the guitar only a high squeal one bar and a burst the next", r, {}, v },
                         // (0.146.0) The guitar ideas from the owner's 61 songs as types of their own (shapes only; our names).
                         { "OCTAVE-JUMP PEDAL", "The low root in sixteenths jumping to its octave and the half step over it, hats on every sixteenth", r, {}, v },
                         { "PEDAL 7+3+3+3", "The low root on seven sixteenths, then three groups of three, a power chord opening each group and moving with it", r, {}, v },
                         { "DRIVEN ARPEGGIO", "A distorted broken chord in eighths over two octaves, the bass on 3+3+2 eighths, ride quarters", r, {}, v },
                         { "CHUG PAIRS, HIGH PINGS", "Low muted chug pairs, two short high notes in the gaps of beats 3 and 4", r, {}, v },
                         { "MUTED PEDAL, ACCENTS IN THREES", "A muted root pedal in sixteenths, accented fifth dyads every three (3+3+2+3+3+2)", r, {}, v },
                         // (0.147.0) Round four (the owner's 54 more songs; shapes only, our names).
                         { "HOOK OVER THE BEAT, GUITARS OUT", "The guitars out: a keys hook fixed over every chord, the bass on whole notes, the chorus's groove thinned", r, {}, v },
                         { "SEQUENCER VERSE, NO KIT", "No kit: clicks, an arpeggiator, whole-note bass and one guitar chord held three bars", r, {}, v },
                         { "HALF-STEP DROP PAIRS", "Two-sixteenth stabs, the chord a half step above dropping onto the root, the bass on its own sixteenth riff", r, {}, v },
                         { "TOM BURST WITH THE CELL, THEN SILENCE", "Three hits a dotted eighth apart with the toms rolling under them, then a bar of silence", r, {}, v },
                         { "TWO-NOTE SEESAW IN EIGHTHS", "Eighths rocking between a high chord tone and the fifth under it, over a bass and kick riff, the snare on 3", r, {}, v },
                         { "OFF-BEAT STABS WITH THE SNARE", "Short chords on every off-eighth with the snare doubling them", r, {}, v },
                         { "TRILL HOOK OVER HATS", "A high two-note trill alone over a hats-only groove", r, {}, v },
                         { "HELD CHORD A BAR, TWO HITS EVERY FOURTH", "A chord rung each bar over the riff's groove; every fourth bar two hits and a fill", r, {}, v },
                         { "STRINGS ALONE VERSE", "The band out: strings over a walking pad", r, {}, v },
                         { "TREMOLO LINE IN GROUPS", "A single-note line, each note a sixteenth burst in groups of 4, 3, 3 and 6, over root sixteenths", r, {}, v },
                         { "RIFF BARS AND THIN BARS IN TURN", "Two bars of a chug cell, two thin bars, a tom rumble every fourth pair", r, {}, v },
                         // (0.168.0) Round five (the owner's 12 songs; shapes only, our names).
                         { "PEDAL STUTTER UNDER OCTAVE THIRDS", "The low root on sixteenths broken up by a high two-octave dyad of the chord's third, chords rung over it", r, {}, v },
                         { "CHOKED EIGHTHS, HIGH COLOUR TONE HELD", "Short choked chords on every eighth, a high colour note held over each chord, a half-time kit", r, {}, v },
                         { "BASS RUNS UNDER HELD CHORDS", "One power chord held a bar with three high notes answering, the bass running sixteenths round the root", r, {}, v },
                         { "ONE RIFF, THE KIT CHANGES", "One double-picked riff the same every bar; the drums and the bass's density do the changing", r, {}, v },
                         { "OPEN OCTAVE BAR, FALLING TRITONE ANSWER", "An open-voiced bar with two beats left empty, answered by a bar stepping down to the tritone", r, {}, v },
                         { "PEDAL CELL WITH A HIGH ANSWER", "A low pedal on a 3+3+2 cell for half the bar, high notes answering it, falling a step a bar", r, {}, v },
                         // (0.181.0) Round six (the owner's 25 songs; shapes only, our names).
                         { "POWER CHORD PAIRS, MUTED OCTAVES IN THE GAPS", "Low power chords struck in pairs (a half step over the root, then the root), muted high octave notes filling the gaps", r, {}, v },
                         { "OCTAVE LINE THROUGH DEAD STRUMS", "An unbroken sixteenth stream: an octave line's notes in turn with dead strums of the whole chord, the bass walking its own line", r, {}, v },
                         { "BASS HOLDS, LEAD EIGHTHS, SNARE ON THE ONE", "The guitars out: the bass holds a dyad and drops a half step, a high line in the keys, the snare only on 1", r, {}, v },
                         { "ONE-STAB CALL OVER A SINGLE-NOTE RIFF", "A single-note riff, a second guitar calling with one chord every two bars, four on the floor", r, {}, v },
                         { "HIGH STACCATO CHORD EIGHTHS, KICK ON ONE AND THREE", "Short high triads on every eighth, the bass out, the kick on 1 and 3 and no snare", r, {}, v },
                         { "LOW ROOT EVERY THIRD EIGHTH, HALF TIME", "Single-note eighths, the low root every third one across the bar line, over a half-time kit", r, {}, v },
                         { "CLEAN RIFF, HELD BASS, GHOSTED HIP-HOP BEAT", "A clean sixteenth figure alone, the bass's roots held, a hip-hop beat with ghost notes", r, {}, v },
                         { "TWO CHORDS, TWO OPEN PEDALS", "Sixteenths in cells of four: the chord twice, the open low root twice; the chords move over the pedal", r, {}, v },
                         { "DOTTED CHORDS, SNARE ON TWO, THREE-E AND FOUR-A", "Power chords on the dotted eighths, root sixteenths to end the bar, the snare on 2, the e of 3 and the a of 4", r, {}, v },
                         { "CLEAN TWO-NOTE SIXTEENTHS OVER A BASS RIFF", "A clean guitar rocking two notes in sixteenths while the bass plays its own riff", r, {}, v },
                         { "HIGH OPEN-PEDAL RIFF OVER A SIXTEENTH ROOT", "The low root then high notes on the eighths, a second guitar's root on every sixteenth, the snare on the ands", r, {}, v },
                         { "KICK AND HAT EIGHTHS, BASS ALONE", "The guitars out, the bass's sixteenth riff alone over the kick and hats on every eighth", r, {}, v },
                         { "CLIPPED EIGHTH CHORDS UNDER A SINGLE-NOTE LINE", "Chords clipped to an eighth on, an eighth off, a second guitar's single-note line over them, kick runs on 1 and 3", r, {}, v },
                         { "FIVE PICKS AND A GAP", "Five sixteenths picked and one left out, the cell of six running across the bar line", r, {}, v },
                         // (0.194.0) Round seven (the owner's 70 songs; shapes only, our names): ten led by the ACOUSTIC
                         // lane (applying one switches it on), then eight electric.
                         { "CHORD TO OPEN PEDAL, TAMBOURINE ONLY", "An acoustic strums the chord, then lets its bass string fall to the open tonic under the top strings; no kit, a tambourine on 2 and 4", r, {}, v },
                         { "OFFBEAT STRUMS OVER A TOM TRESILLO", "High acoustic shapes struck off the beat, the bass and the floor tom on one 3+3+2 figure, no kick", r, {}, v },
                         { "LOW CHORD AND HIGH CLUSTER, BAR BY BAR", "An acoustic's low chord one bar and a high cluster over the same root the next, on a 3+3+2 lilt; the pedal hat and a dotted kick", r, {}, v },
                         { "ACOUSTIC JOINS HALFWAY ON THE RIFF", "The guitars out, the bass's sixteenth riff over sixteenth hats; halfway an acoustic strums the riff's rhythm, a build in the last bar", r, {}, v },
                         { "TWIN ACOUSTIC ARPEGGIOS", "Two acoustics picking one rhythm on different chord tones, one strum ringing under the start, whole-note bass from the second chord", r, {}, v },
                         { "FINGERPICKING, SHAKER AND HATS", "An acoustic picking eighths over a held bass note, its top fixed from the key; a shaker and hats, the kick rare", r, {}, v },
                         { "HIGH ROLLED TRIADS OVER A UNISON LINE", "An acoustic's high triads rolled, a breath, then picked, over the bass and a clean guitar on one slow line; half-time drums", r, {}, v },
                         { "STAB CELL ON THE WEAK SIXTEENTHS", "An acoustic's dyad stabs on the weak sixteenths one bar, silent the next, the bass's sixteenth riff owning the downbeats", r, {}, v },
                         { "DRONE PAIR AND HAMMER-ONS", "An acoustic's low root held under two notes a whole step apart, a hammer-on into each bass note; sixteenth hats and a shaker", r, {}, v },
                         { "CLEAN STRUM WITH FLICKS", "A soft strum with a down-up flick before the backbeat, on an acoustic (the clean electric with it if you like), a light backbeat", r, {}, v },
                         { "HIGH OSTINATO, LOW RIFF EVERY OTHER BAR", "A plucked high ostinato in octaves that never stops, a short low riff only every other bar, the bass with it", r, {}, v },
                         { "RIFF AFTER THE ONE, FOUR ON THE FLOOR", "The guitars and the bass in unison eighths from the and of 1, the downbeat always empty, the second half dropping to the tritone; four on the floor", r, {}, v },
                         { "STRINGS PULSE, GUITARS OUT", "The guitars and the bass out, strings on the root in staccato sixteenths over a dotted kick; a muted guitar cell from halfway", r, {}, v },
                         { "SIX CHUGS, TWO HITS, A BAR OF RING", "Six muted chugs, two hits and a chord rung through a bar where nothing is struck; the kick bursting on 1", r, {}, v },
                         { "PEDAL EIGHTHS, SUS4-TO-3 DYAD", "A muted root pedal with a high fourth-and-fifth dyad resolving to the third, the bass with the pedal, a snare pickup into the backbeat", r, {}, v },
                         { "GAPPED CHUGS, TREMOLO IN THE GAPS", "A chug bar with gaps, then three bars of two hits and a pickup while a second guitar tremolo-picks root and fifth in the gaps", r, {}, v },
                         { "GALLOP THEN TREMOLO NOTE", "A galloping chord cell, six sixteenths on one high note, then held chords; the kick on the gallop, crash eighths", r, {}, v },
                         { "CALL BAR, BASS ANSWER BAR", "A chord rung on 1 and three stabs late in the bar, then a bar left empty for the bass to answer", r, {}, v },
                         // (0.213.0) Round eight (the owner's 67 songs; shapes only, our names).
                         { "ROOT PAIRS IN THREES, TOP NOTE TURNS", "Sixteenth pairs on the low root in threes, a seventh then the octave over it, the last pair's top turning bar by bar", r, {}, v },
                         { "ROOT AND FLAT-SEVEN DYAD GROOVE", "The low root and the flat seventh over it on a busy sixteenth groove, the sixth every other two bars, the bass on its own groove", r, {}, v },
                         { "TWO POWER CHORDS TRADED ACROSS THE BAR", "Two power chords traded on 1, 2, its and, the a of 2, 3 and its and, the bass doubling", r, {}, v },
                         { "OFF-BEAT DYADS, SCRATCHES IN THE HOLES", "Root-and-fifth dyads off the beat in a two-bar figure, muted scratches filling only where they rest, the kick off the beat", r, {}, v },
                         { "THE DOWNBEAT LEFT OUT, BAR BY BAR", "One bar comes in on 2, the next on the and of 1, the eighths rocking to the chord a step under, the bass doubling", r, {}, v },
                         { "TWO EIGHTHS AND A REST, A LINE IN THE GAPS", "Two eighths and a rest each half bar with the bass, a clean guitar answering in the gaps", r, {}, v },
                         { "CHORD, THEN A SIXTEENTH RUN TO THE BAR LINE", "The low root and the octave chord, then a pentatonic run of sixteenths up to the bar line, the bass doubling it", r, {}, v },
                         { "SIX ON, TWO OFF", "A four-bar riff, its first two bars again, then two bars left empty while the drums go on", r, {}, v },
                         { "SEVEN-EIGHTH CYCLE OVER FOUR-FOUR", "A seven-eighth loop and groove running across the 4/4 bars over notes held fourteen eighths each; it lines up again every seven bars", r, {}, v },
                         { "OCTAVE GUITAR DOUBLES THE WALKING BASS RIFF", "A four-bar walking eighth line on the bass, the guitar doubling it in octaves, a sparse half-time kit", r, {}, v },
                         { "SHORT, SHORT, LONG UNISON, EIGHTHS TO END", "The guitar, the bass and the kick together on 1, the and of 2, the and of 3 and 4; the eighth bar in straight eighths", r, {}, v },
                         { "LINE BAR, CHORDS FROM ONE-AND", "Two-bar cycles: a single-note line bar, then power-chord eighths from the and of 1 on another degree", r, {}, v },
                         { "BASS ALONE, A HALF-STEP TOUCH, QUARTER HATS", "The guitars out, the bass's root eighths touching the half step above every other bar, quarter hats and a rare kick", r, {}, v },
                         { "PEDAL TRIAD WITH CLIMBING INNER VOICES", "The keys hold the key's root with two voices over it climbing and falling back, the bass and the kick on one pedal figure", r, {}, v },
                         { "UNISON LOW SIXTEENTHS OVER A TOM GROOVE", "The guitar and the bass on the low string in sixteenths, moving at the half bars, over a three-bar tom groove", r, {}, v },
                         // (0.234.0) Round nine (the owner's 17 industrial songs; shapes only, our names).
                         { "MACHINE LOOP, BASS RIFF, GUITARS OUT", "The guitars out: a two-bar quarter-note bass riff over a machine loop of kick eighths and sixteenth hats that never fills", r, {}, v },
                         { "BASS ON THE CHORUS RIFF, FOUR ON THE FLOOR", "The guitars out but a feedback chord or late triad stabs, the bass keeping the chorus's eighth riff over four on the floor with no snare", r, {}, v },
                         { "HIT ON ONE, PICKUP CHORDS ON FOUR", "The chord struck on 1 and left, two power chords pushed on 4 and its and into the next bar, over a glam stomp", r, {}, v },
                         { "SYNTH OCTAVE BURSTS, GUITARS AND HATS OUT", "The guitars and hats out: a synth octave burst on 1, the bass rocking root and flat seven, the riff's kick and snare", r, {}, v },
                         { "RIDE ONLY, BOOMS AND NOISE", "No kick or snare, only ride eighths and the pedal hat; low synth booms and noise stabs off the sixteenths over quarter roots", r, {}, v },
                         { "BASS AND ORGAN QUARTER RIFF, TWO STABS", "The bass and an organ on a quarter-note riff, short stabs on 1 and 2 every other bar, four on the floor with no hats", r, {}, v },
                         { "HIGH GUITARS CARRY IT, BASS OUT", "The bass and the low guitar out, two high guitars on three-note chords in eighths, closed hats, a sparse bar and a busy bar", r, {}, v },
                         { "SYNTH-BASS LOOP UNDER HALF TIME", "An interlocking two-bar synth bass under a half-time loop, strings holding one note every two bars, the guitars out", r, {}, v },
                         { "BASS AND ONE MUTED CHUG, DRUMS OUT", "The drums out: one guitar's muted sixteenths rocking a half step, the bass's roots under it", r, {}, v },
                         { "SOFT CHORD BAR, DOTTED OCTAVE PINGS", "A soft chord bar, then a bar of octave pings every three sixteenths; ghosted snares and loud-soft sixteenth hats", r, {}, v },
                         // (0.238.0) Round nine's swung types: twelve triplet eighths a bar, in 4/4 (shapes only, our names).
                         { "TRIPLET STOMP, TRITONE CHUG", "Everything in triplets: power chords on the root and the flat five over a two-beat stomp of snare, three kicks and a snare between", r, {}, v },
                         { "SHUFFLE STOMP", "A swung stomp: a low single-note shuffle on 1 and the a of each beat, the kick and floor tom together, no hats", r, {}, v },
                         // (0.240.0) Round ten's verses (the owner's 56 songs: arena rock, ballads, acoustic, alt-rock; shapes only, our names).
                         { "PIANO BALLAD VERSE", "A quiet piano verse: the root on 1 and a broken triad in eighths over it, the drums and bass out until halfway", r, {}, v },
                         { "POWER-BALLAD VERSE, GUITAR ANSWERS", "Piano whole-note chords under a ballad beat, a clean guitar answering in the gap at the end of every two bars", r, {}, v },
                         { "BAND HIT EVERY FOUR BARS UNDER THE PIANO", "Piano dyads on the eighths carry it; the guitar, the bass, the kick and a crash hit together on the first bar of every four", r, {}, v },
                         { "LOOP VERSE, BASS CLIMB EVERY BAR", "A loop that never fills, a chord rung then muted, the bass holding each root and climbing by step into the next", r, {}, v },
                         { "CHORD ON ONE, MUTED EIGHTH PEDAL", "A chord on 1, then the low root muted on the eighths, an organ stab with it, four on the floor under the ride", r, {}, v },
                         { "RIFF BAR, HITS BAR", "Two-bar cycles: a muted sixteenth pedal riff in groups of six, six and four, then a bar of wide chord hits", r, {}, v },
                         { "QUIET VERSE: MUTED DYADS, NO SNARE", "Muted dyads and a busy quiet bass, hats and the pedal hat with no snare at all, toms to end every fourth bar", r, {}, v },
                         { "BASS AND STRINGS STABS IN TURN UNDER PIANO EIGHTHS", "The piano's triads on every eighth, the bass stabbing the front of the bar and the strings the back, the guitars and drums out", r, {}, v },
                         { "PIANO HALF NOTES, BASS STEPPING BY INVERSIONS", "Wide piano chords in half notes, the bass stepping to the nearest chord tone, a half-time beat with the snare on 3", r, {}, v },
                         { "TOM-GROOVE VERSE, BROKEN POWER CHORDS", "The floor tom carries the groove with the kick, no snare or hats, the power chords broken into eighths over it", r, {}, v },
                         { "CHUNK BAR, SINGLE-NOTE ANSWER BAR", "Two-bar cycles: power-chord chunks on 1, its and and 2, then a bar of single notes answering on the eighths", r, {}, v },
                         { "PEDAL EIGHTHS, A TRIAD PULSE IN THE SECOND HALF", "The low root muted on the eighths throughout, a high triad pulse on the quarters (and the keys' eighths) from halfway", r, {}, v },
                         { "FOUR ON THE FLOOR, ONE CHIMING NOTE", "One high note chimed in eighths across the changes over the kick and hats on every quarter, no snare, a tom run every four bars", r, {}, v },
                         // (0.250.0) Round eleven's verses, part one (the owner's 61 more songs; shapes only, our names).
                         { "THREE SNARES AFTER A LONE KICK", "A lone kick on 1, the snare on 2, 3 and 4, floor tom pickups into the next bar; the guitars out, the keys rocking a minor third", r, {}, v },
                         { "PICKED SEVENTHS OVER A FALLING LINE", "An acoustic picking chord tones while the bass falls a half step a bar under the same chord, split hats and ride", r, {}, v },
                         { "ONE KEY NOTE IN 3+3+2, A STEP LOWER EACH BAR", "One keys note repeated in threes and twos, a scale step lower each bar, the bass in the same rhythm, the guitars out", r, {}, v },
                         { "LOW OCTAVE EIGHTHS UNDER A SHAKER", "The guitar's low root and octave on every eighth, a two-note climb every other bar, shaker sixteenths over a plain beat", r, {}, v },
                         { "SIX-BEAT RIFF ACROSS THE BAR", "A power-chord riff six beats long, so it starts two beats later each bar and comes back to the bar line every three", r, {}, v },
                         { "FIFTH AND FOURTH ROCKING OVER THE PEDAL", "A low pedal under fifth and fourth dyads in turn, the bar's last hit a half step above the root, crash quarters", r, {}, v },
                         { "FOUR ON, TWO OFF, UNISON SIXTEENTHS", "Guitars and bass in sixteenths in groups of four split by two rests, the last group a half step away, pairs in the fourth bar", r, {}, v },
                         { "MUTED EIGHTHS, STRUMS WITH AN OPEN-STRING TAIL", "A half-time verse: soft muted power-chord eighths, an acoustic strumming three times then the open strings on 4 and its and", r, {}, v },
                         { "CHROMATIC BASS CLIMB, KICK EVERY OTHER BAR", "The bass in sixteenths climbing by half steps over two bars, the kick on 1 of every other bar, the guitars out", r, {}, v },
                         { "STABS, THEN A PICKED RUN IN THE SAME BAR", "Power-chord stabs through the first half of the bar, then single notes picked through the chord's colours in the second", r, {}, v },
                         { "TWO TOMS FOR THE BACKBEAT, ROCKING TOP", "Two toms together on 2 and 4 in place of the snare, no cymbals, a clean top note rocking a half step on the quarters", r, {}, v },
                         { "TWO KICKS UNDER RINGING DYADS AND SHAKER", "Only two kicks a bar, on 1-and and 2, under shaker eighths and clean sus dyads ringing on the eighths", r, {}, v },
                         { "3+3 GROUPS, BEAT FOUR LEFT OPEN", "The kit in two groups of three eighths, beat 4 only an open hat; an acoustic picking root, root, fifth twice and ringing over 4", r, {}, v },
                         { "STICK LOOP, BASS ROOT PAIRS", "No kit: a stick and cabasa loop, the bass's root and octave on 1 and its and, a walk into every other bar", r, {}, v },
                         { "CHORD ON ONE, THE NEXT PUSHED ON FOUR, DOTTED LINE OVER", "A clean chord on 1, the next bar's chord struck on 4 and tied over, the keys on four dotted eighths above", r, {}, v },
                         { "BASS GROOVE, GUITAR ONLY ON THE PICKUPS", "A two-bar bass groove of held notes carries it, the guitar silent but three muted pickups every other bar", r, {}, v },
                         { "PIANO EIGHTHS AND PIZZICATO OCTAVES, NO BACKBEAT", "Piano chords on the beats and the root on the ands, no kick or snare, only hat eighths and a ride bell on 3-and", r, {}, v },
                         // (0.251.0) Round eleven's verses, part two.
                         { "TWO BARS OF RIFF, TWO OF KICK UNDER THE VOICE", "Two bars of a three-chord riff, then two bars of only the kick under the voice, a power chord pushed on 4 into the next", r, {}, v },
                         { "SIX-BAR RIFF, FOUR THE SAME AND TWO TO ANSWER", "A single-note riff doubled in octaves over six bars: four the same, then two answering on the fourth, crossing the four-bar phrases", r, {}, v },
                         { "CHORD RUNG, THREE EIGHTHS INTO THE NEXT", "An open chord struck on 1 and left to ring, then three eighths into the next, the bass in the same rhythm, brushed drums", r, {}, v },
                         { "DOTTED SYNTH BASS UNDER SIXTEENTH CHUGS", "Muted sixteenth chugs with accents over a dotted synth bass line (root, octave, flat seven, five, four, five), four on the floor", r, {}, v },
                         { "SIDESTICK BURSTS OVER FOUR ON THE FLOOR", "Four on the floor with sidestick bursts and sixteenth hats, an acoustic picking open strings, quiet muted chug runs", r, {}, v },
                         { "SHUFFLED HALF-BAR CHORDS WITH A MUTED SCRATCH", "A shuffle: a chord each half bar, struck twice, a muted scratch on the last triplet before the next", r, {}, v },
                         { "NYLON DYAD FLICKS OVER A BALLAD KICK", "A nylon guitar flicking a high fifth-and-octave dyad, a clean guitar seesawing two dyads, a ballad kick under a soft pad", r, {}, v },
                         { "TONIC HELD, THEN EIGHTHS ON THE OTHER THREE", "A bar on the tonic chord, then a bar of eighths through three more chords, the last held to the bar line", r, {}, v },
                         { "TWO GALLOP BARS, TWO BARS OF RING", "Two bars of a galloping root riff with the kick doubling it, then a high triad rung and a near-empty bar", r, {}, v },
                         { "SHORT CHORD STABS ON TWO, TWO-AND AND FOUR", "Muted full chords stabbed short on 2, its and and 4, piano syncopations under them, a tambourine on the quarters", r, {}, v },
                         { "SLAP OCTAVE BASS UNDER A SIXTEENTH ARPEGGIO", "A slap bass figure from root to octave and back under a synth arpeggio on every sixteenth, the kick on 1 only", r, {}, v },
                         { "SIXTEENTH BASS RIFF, A RUN TO THE BAR LINE", "The bass's sixteenth riff with a run to every bar line, a guitar dyad on 1 and three hits after it, hand drums", r, {}, v },
                         { "THE CHANGE PUSHED, THE NEXT DOWNBEAT EMPTY", "Two-bar cells: the next chord pushed in on the and of 4, the downbeat after it left empty, guitar and bass together", r, {}, v },
                         { "CANON LINES OVER A LAID-BACK SIXTEENTH KIT", "Two clean guitars on one falling sixteenth line, the second an eighth behind, over a laid-back sixteenth kit", r, {}, v },
                         { "ONE GUITAR'S EIGHTHS, A PEDAL HAT, THEN A DOUBLE", "A staged verse: one guitar's power-chord eighths, the pedal hat from halfway, a second guitar doubling from three quarters", r, {}, v },
                         { "TWO HELD EIGHTHS, FOUR SIXTEENTHS", "Each half bar the chord on two held eighths, then another chord in four sixteenths", r, {}, v },
                         { "OCTAVE STABS OVER DOUBLE-KICK PAIRS", "Root octave stabs in a pushed rhythm over double-kick pairs, the last group a step under", r, {}, v },
                         { "TWO-SHAPE SIXTEENTH SEESAW, BASS ROCKING UNDER", "Ten sixteenths seesawing between the root shape and the one a step up, then a held chord, the bass rocking under", r, {}, v },
                         { "SIXTEENTH CALL, DOTTED ANSWER", "A bar of sixteenth power chords calling, then a bar answering on dotted eighths from beat 3", r, {}, v },
                         { "KICK PICKUPS INTO EVERY BEAT, OCTAVE PICKUPS", "A kick sixteenth before every beat, quarter power chords, a clean second guitar's octaves with the same pickups", r, {}, v },
                         { "SHAKER SIXTEENTHS, CHORDS UNDER ONE TOP NOTE", "No kick or snare: shaker sixteenths and quarter hats, held keys chords that all keep one top note", r, {}, v },
                         { "HALF-NOTE CHORDS, KICK ON ONE AND TWO-AND", "Power chords with the third added in half notes, the kick on 1 and 2-and, open hats on the quarters", r, {}, v },
                         // (0.264.0) Round twelve's verses (the owner's ten more songs; shapes only, our names).
                         { "TREMOLO ROOT, THEN THE TRITONE, HALF A BAR EACH", "Sixteenth tremolo half a bar on the low root and half on the flat five, then a bar of the root, a double-time beat with a blast at the end", r, {}, v },
                         { "CHORDS CLIMB ON THE BEATS, A FOUR-NOTE CELL OVER", "Power chords climbing on 1, 2 and 3 and a flat-seven chug pair on 4, a lead's four-note sixteenth cell moved onto every chord", r, {}, v },
                         { "TRAIN BEAT UNDER A PICKED ACOUSTIC WITH RUNS", "An acoustic picking chord tones with a scale run into each change, the bass walking up, a train beat with the snare rolling", r, {}, v },
                         { "STAB, PUSH, SIXTEENTH CHUGS, A FLURRY BAR", "A chord stabbed on 1, the root's chord pushed on 2-and, root sixteenths from 3-and, every fourth bar a flurry round the root", r, {}, v },
                         { "FALLING QUARTER LINE OVER DYAD EIGHTHS", "A lead falling a half step in quarters over the rhythm guitar's eighths, the fifth dyad and the root in turn", r, {}, v },
                         { "LOW ROOTS WITH CHORD GRABS", "A ballad verse: the low root on the beats with the chord grabbed on 3 and its and and a higher chord on 4-and, ride eighths", r, {}, v },
                         { "TWIN THIRDS OVER TIED EIGHTH CHORDS", "Power-chord eighths with every change struck early on 4-and and tied over, twin leads in thirds over them", r, {}, v },
                         { "CHORD, CHUGS, THEN A FALLING LINE IN THIRDS", "A chord on 1 and 2-and with root sixteenths between, then the bar's end in sixteenth pairs falling in thirds, crash eighths", r, {}, v },
                         { "FIVE-HIT CLUSTER, THEN A PEDAL LEAD", "Five hits early in the bar and the rest left ringing, the kick on them, a high pedal lead from halfway", r, {}, v },
                         droning, sparse };
            }
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
                         { "CHUG BAR, CHORD BAR", "A bar of chugs on the low root answered by a bar of struck chords, a high squeal at the end of every other bar", r, {}, c },
                         // (0.140.0) 23 more from the owner's 61 song files (shapes only; our names).
                         { "DOTTED STREAM", "Power chords on 3+3+3+3+2+2 every bar, the bass and the kick on the dotted hits, open hats on the eighths", r, {}, c },
                         { "NEIGHBOUR TRILL WALL", "Full chords on every sixteenth, the bass trilling the root with the note under it, a plain backbeat, a run up the scale into the next part", r, {}, c },
                         { "TWO RUNG CHORDS A BAR", "A chord struck on the one and another on the and of 2, both rung, a crash on each", r, {}, c },
                         { "THREE DOTTED PUSHES", "A chord rung, then three chords pushed on 1, the a of 1 and the and of 2, the strings doubling them high", r, {}, c },
                         { "FOUR ON THE FLOOR, DYAD RIFF", "A high sixteenth dyad riff over four on the floor, the changes on 3+3+2 every other bar, open hats on the off-beats", r, {}, c },
                         { "SPLIT ON THE AND OF THREE", "Two chords a bar, the next one pushed in from the and of 3, the kick on the accents, the ride on the eighths", r, {}, c },
                         { "TREMOLO DYADS OVER RUNG CHORDS", "Chords rung in halves under high tremolo-picked dyads, the floor tom on 1, 1-and, 3 and 3-and", r, {}, c },
                         { "DOTTED CHORDS, HIGH TREMOLO NOTE", "Full chords on a dotted grid under one high chord note tremolo-picked, a crash on every eighth", r, {}, c },
                         { "THE RIFF IS THE CHORUS", "Every guitar and the bass in unison on a low chromatic riff, the kick on its hits, a china on the beats", r, {}, c },
                         { "HIGH TREMOLO DYAD OVER THE RIFF", "The low root and tritone on a gapped grid under a high dissonant dyad tremolo-picked over every chord", r, {}, c },
                         { "HELD CHORDS IN 3+3+2", "Three long chords rung across each bar on 3+3+2, the kick and a crash with each", r, {}, c },
                         { "DOTTED PEDAL CELL", "A six-sixteenth cell running 3 over 4: the low root moving with the chords, its top notes fixed", r, {}, c },
                         { "FIXED SHAPE, CREEPING ROOT", "A ringing arpeggio whose upper notes never change, only its root following the chords, a high two-note hook", r, {}, c },
                         { "CHROMATIC CHORDS UNDER A FIXED TREMOLO", "Power chords struck for half a bar and gone for the other half, a high chord tremolo-picked over all of it", r, {}, c },
                         { "HALF-NOTE CHORDS, OFF-BEAT OPEN HATS", "Chords held in halves, the open hat only on the off-beats, the fourth bar's changes pushed with a tom fill", r, {}, c },
                         { "HIT, HOLD, BURST INTO THE NEXT", "A chord struck and held, then three sixteenths of the next chord bursting into its bar", r, {}, c },
                         { "PUSH BAR, CELL BAR", "One bar a held chord and sixteenths pushing into the next, the next bar pairs of sixteenths", r, {}, c },
                         { "SYNCOPATED CHORDS WITH A PUSH BAR", "Syncopated chords one bar, the next a held chord and a sixteenth push up three chords over the toms", r, {}, c },
                         { "THREE-SIXTEENTH CELLS AFTER A HELD ONE", "A chord held a beat, then three sixteenths on each of the other beats, rocking the root and the half step", r, {}, c },
                         { "SHRINKING GROUPS", "Four bars of eighths in groups of 9, 8, 7 and 8, each a held chord and sixteenth pairs, the accents crossing the bar lines", r, {}, c },
                         { "HELD ROOTS CLIMBING, PUSHED LATE", "One chord a bar struck late and held into the next, a kick and a crash with each, the toms rolling", r, {}, c },
                         { "FALLING CELLS IN SIXES", "Three-note cells falling a half step at a time into the root, every six sixteenths, low", r, {}, c },
                         { "FOUR EIGHTHS, THEN A PUSH", "Four eighth-note chords, a gap, then pushes into the next bar, a high piano pulse over it", r, {}, c },
                         // (0.148.0) Round four (shapes only, our names).
                         { "ACOUSTIC STRUM CHORUS", "Full chords strummed in sixteenths, whole-note bass, strings holding; the drums out, toms alone, toms over four on the floor, or a dotted kick", r, {}, c },
                         { "BURST & HOLD, THEN A CARPET IN 3+2+3", "A unison burst into a held chord, then kick sixteenths under 3+2+3 chords, then a half-time riff", r, {}, c },
                         { "BREAKBEAT UNDER HELD CHORDS", "A double-time broken beat under a fixed keys ostinato and a clean 3+3+2 figure, whole-note bass", r, {}, c },
                         { "STOMP IN 3+3+2", "Every lane on 3+3+2 over a kick and floor tom stomp, a soft snare on 2 and 4", r, {}, c },
                         { "FALLING PUSHED CHORDS", "Two root chugs on 1, then three chords pushed onto the ands, each a step lower", r, {}, c },
                         { "HITS, THEN A WALL THE BASS LEAVES", "Two stabs, then a sixteenth wall; the bass only on the stabs; crash and splash eighths", r, {}, c },
                         { "RING THE ONE, RUN THE REST", "An open chord rung on 1, power chords running down the scale through the bar, a china on the quarters", r, {}, c },
                         { "BASS SIXTEENTHS, GUITARS OUT", "The bass on root sixteenths under a keys ostinato and a hip-hop break; the guitars out, stabbing or in a wall", r, {}, c },
                         { "THREE CHORDS, THEN A HELD BAR", "Three chords on 1, the and of 2 and 4, the next bar holding the last; the snare on 3 only", r, {}, c },
                         { "HELD CHORD WITH A RINGING TOP, BUSY KICK", "A chord struck and held with its ninth ringing on top, pushed into the next, over a busy sixteenth kick", r, {}, c },
                         { "CHANGES ON THREE", "Open eighths with each chord arriving on beat 3 over four on the floor", r, {}, c },
                         // (0.169.0) Round five (the owner's 12 songs; shapes only, our names).
                         { "HALF-STEP GRIND IN THREES, HIGH TONIC TREMOLO", "Low power chords grinding in threes between the root and the half step over it, the key's root high in sixteenths over them", r, {}, c },
                         { "FIXED DISSONANT CHORD, BASS ROCKS UNDER IT", "One dissonant cluster struck on every beat all chorus, the bass moving under it, a china on the eighths", r, {}, c },
                         { "LOW CHORD, HIGH CLASH, WALK BAR", "A low power chord answered by a high triad a half step over it, every third bar a chromatic walk down", r, {}, c },
                         { "RINGING, THEN DRIVING", "Four-bar phrases going from a rung chord to a syncopated figure to driving eighths, half time under them", r, {}, c },
                         { "TONIC BAR, CHROMATIC FALL BAR", "A bar of the key's root on 3+3+2, then a bar falling by half steps, a high seesaw over it", r, {}, c },
                         { "CALL, AIR, DRIVE", "A bar of hits, a bar of air with one soft high chord, two driving bars", r, {}, c },
                         { "RING THE OCTAVE OVER A MOVING BASS", "One high octave rung a bar, every fourth bar empty, the bass doing the moving, a half-time ride", r, {}, c },
                         // (0.182.0) Round six (the owner's 25 songs; shapes only, our names).
                         { "KICK AND TOMS 3+3+2 UP FRONT, BELL QUARTERS", "The kick and a low tom on the 3+3+2 front of the bar, the back half empty, the ride's bell on the quarters over a sixteenth power chord stream", r, {}, c },
                         { "BAND IN QUARTERS", "The whole band struck on every quarter, one chord a bar, crashes on the quarters", r, {}, c },
                         { "KICK ON ONE AND TWO, SNARE ON THREE", "Open chords on every eighth over the kick on 1 and 2 and the snare and a clap on 3, nothing on 4", r, {}, c },
                         { "POWER CHORDS IN THE THIRD-EIGHTH CYCLE", "Power chords on every eighth, the root's chord every third one across the bar line, the others round it; the kick on 1, the snare on 3", r, {}, c },
                         { "CHORDS ON ONE AND THE AND OF TWO, HIP-HOP KICK", "Two chords a bar struck on 1 and the and of 2 and held, over a hip-hop kick", r, {}, c },
                         { "HELD CHORDS IN 6+6+4 OVER A CLEAN ARPEGGIO", "Pushed power chords and a second guitar ringing on the 6+6+4 over a clean sixteenth arpeggio, the snare only on 4", r, {}, c },
                         { "KICK ON THE BEATS, SNARE ON THE ANDS UNDER 3+3+3+3+2+2", "Sixteenth power chords grouped 3+3+3+3+2+2 rocking a whole step, the bass stabbing each group, the kick on the beats and the snare on the ands", r, {}, c },
                         { "POWER CHORD AND MAJOR CHORD IN FOURS", "Sixteenths in fours: the power chord, then the same chord with its major third, over kick runs of four", r, {}, c },
                         { "UNISON HITS ON ONE, TWO AND THE LAST TRIPLET, HALF TIME", "The guitars and the bass in unison on the beat, the next beat and its last sixteenth, two chords a bar, over a half-time kit", r, {}, c },
                         { "KICK TRIPLE-SIXTEENTHS, SNARE AND CHINA ON THE ANDS", "Power chord pairs every six sixteenths, the kick in threes around the ands, the snare and the china on every and", r, {}, c },
                         // (0.195.0) Round seven (the owner's 70 songs; shapes only, our names): nine led by the ACOUSTIC
                         // lane (applying one switches it on), then nine electric.
                         { "BIG STRUMS UNDER POWER-CHORD QUARTERS", "An acoustic's full strums on 1 and 3 with light top strings between, a distorted guitar's power chords over it, half time with a tambourine", r, {}, c },
                         { "FIFTH STACKS GROWING IN THE BAR", "An acoustic strumming every eighth on root-fifth stacks that grow from the low strings to all five, a new chord's downbeat left empty", r, {}, c },
                         { "STRUM, EACH CHANGE AN EIGHTH EARLY", "An acoustic's root-fifth-octave strum, every new chord struck on the last eighth before its bar and tied over, the band on the pushes", r, {}, c },
                         { "SIXTEENTH STRUM, CHANGE ON THE E OF THREE", "An acoustic strumming in sixteenths with gaps, the next chord pushed onto the e of 3, over an overdriven chug if you like", r, {}, c },
                         { "BASS NOTE AND STRUM UNDER THE BAND", "An acoustic's bass note alone on 1, the upper strings strummed on the eighths after it, the full band under it with a crash every bar", r, {}, c },
                         { "TWO ACOUSTICS, TWO STRUMS", "One acoustic's bass note and strum, a second's syncopated full strums over it, strings added one by one", r, {}, c },
                         { "LILT IN THREES, POWER CHORDS ON TWO AND THREE", "An acoustic's strum in threes across the bar (3+3+3+3+4 sixteenths), distorted power chords on beats 2 and 3 only, the kick on 3", r, {}, c },
                         { "BASS-STRING STRUM UNDER THE CHUGS", "An acoustic's low strings and full strums tied every other bar, under palm-muted chugs: a sixteenth burst on 1, then eighths", r, {}, c },
                         { "THE VERSE'S RHYTHM AS POWER CHORDS", "The verse's flick strum played as low power chords: the same rhythm, distorted, the acoustic quietly under it", r, {}, c },
                         { "OPEN-STRING CHORD HITS", "A stacked-fourths chord a half step over the root held two beats, then every open string struck on 3, its and and 4", r, {}, c },
                         { "SYNCOPATED OPEN STRUM OVER THE EIGHTHS", "Power chords in straight eighths, a second guitar's open chords strummed on 1, 2, 2-a, 3-e, 3-and and 4 over them", r, {}, c },
                         { "WALTZ GROOVE ACROSS THREE BARS", "Chords struck every dotted quarter across the bar lines, the kit's twelve-sixteenth groove four times in three bars", r, {}, c },
                         { "DOTTED CHORDS, THEN A BAR OF ROOT EIGHTHS", "Three chords on the dotted quarters, then a bar of root eighths with a sixteenth push; the last bar lifted a half step", r, {}, c },
                         { "HELD CHORD, PUSHED PAIRS", "A chord held, then pairs of hits pushed later each bar, a fourth bar of eighths ringing out", r, {}, c },
                         { "HIT, THEN EIGHTHS OFF TWO-AND", "A chord on 1 rung three eighths, then power-chord eighths from the and of 2; every fourth bar straight eighths", r, {}, c },
                         { "IN ON TWO, ANSWERS ON THE ANDS", "A bar of eighths coming in on beat 2, answered by a full bar; the others stab the ands of 3 and 4; half time", r, {}, c },
                         { "ONE, TWO, THEN TWO PUSHES", "Every lane on 1 and 2, then two pushes on the a of 3 and the and of 4; the kit opens up for the second half", r, {}, c },
                         { "HALF TIME, KICK ON TWO, HELD PAIRS", "A power chord held and a short chord a minor third up in turn, then off-beat eighths; the kick on 2 and the a of 4, the snare on 3", r, {}, c },
                         // (0.214.0) Round eight (the owner's 67 songs; shapes only, our names).
                         { "SIX-HIT CHORD BAR OVER THE ROOTS", "Wide chords and the bass's roots on six hits a bar, the ride on the eighths from the and of 1", r, {}, c },
                         { "SIXTEENTH OCTAVES ON THE FLAT SEVEN OVER A ROOT RIFF", "The key's flat seventh, then its sixth, in octaves, eighths then sixteenths, over a root-and-seventh riff and a four-kick burst", r, {}, c },
                         { "OCTAVES CLIMB A STEP A BAR", "Octaves in eighths climbing the key's scale a step a bar with the bass, power chords under, the kick on every eighth", r, {}, c },
                         { "HALF-STEP SHOVE ON FOUR-AND", "Power-chord eighths, every second bar's last eighth shoved a half step up and held, the kick and bass with it", r, {}, c },
                         { "CHORDS ON ONE, THREE, FOUR, FOUR-AND", "High chords on 1, 3, 4 and its and, the next bar filling in, the bass after them, a half-time kit", r, {}, c },
                         { "AND-OF-ONE UNISON PUSHES", "The guitar, the bass and the kick together on the and of 1, the and of 2, 3, its and and the and of 4: nothing on 1", r, {}, c },
                         { "UNISON PUSH RIFF, PAIRED-SNARE BAR EVERY FOURTH", "A unison push riff on the bar's chord, every fourth bar busier with the snare in pairs and a splash", r, {}, c },
                         { "TWO HITS, THEN AIR", "Power chords on 1 and 2 only, the back half empty, the kick on 1, 2 and 4 and the snare on 3 and its answer", r, {}, c },
                         { "HALF-STEP POWER CHORDS EACH HALF BAR, BASS ROOT-FIFTH-OCTAVE-FIFTH", "Power-chord eighths a half bar each stepping a half step down and back, the bass on root, fifth, octave, fifth", r, {}, c },
                         { "UNISON HITS, THE KICK ITS OWN WAY", "The guitar and the bass on seven syncopated hits, the last held, the kick on its own pattern against them", r, {}, c },
                         // (0.235.0) Round nine (the owner's 17 industrial songs; shapes only, our names).
                         { "RING BAR, SIXTEENTH CHORD STREAM", "A two-bar cycle: the chord rung, then full chords on every sixteenth changing every half bar; the kick in clusters in the stream bar", r, {}, c },
                         { "HOLD BAR, CHUG BAR, HALF-TIME SNARE", "Three power chords held in one bar, the chord chugged in muted eighths the next; the snare half time in the hold bar, a crash every bar", r, {}, c },
                         { "WHOLE-BAR CHORDS FALLING BY HALF STEPS, CHOIR PEDAL", "One power chord a bar falling a half step each bar, a choir holding the key's fifth on top, a kick and snare build to end", r, {}, c },
                         { "HELD POWER CHORDS OVER THE LOOP", "A power chord held two bars is the only guitar, over the verse's synth-bass and half-time loop; the size from sustain, not rhythm", r, {}, c },
                         { "TWO CHORDS HELD OVER STACCATO ROOT QUARTERS", "Short root quarters on the bass, an organ and a choir holding each chord two bars, four on the floor with no hats", r, {}, c },
                         { "PAIRS STEPPING DOWN A STEP A BAR", "The chord, a high dyad stab and the chord a whole step lower traded each bar, a crash on every quarter", r, {}, c },
                         { "RIFF BAR, HALF-STEP SLIDE BAR", "A low riff bar, then a power chord a whole step up and one a half step up sliding back to the root, over a stomp", r, {}, c },
                         { "PUNK EIGHTHS IN 3+3+2, SNARE ON THE DOTS", "Power-chord eighths changing in groups of 3, 3 and 2, the snare on the dotted quarters and the kicks between, a skank bar every fourth", r, {}, c },
                         { "CHANT STOP: GUITAR ALONE", "The drums and the bass out: the guitar alone, the fifth held two half notes, then short root hits, two pickups at the end", r, {}, c },
                         // (0.238.0) The swung types, a chorus's too (one list serves both).
                         { "TRIPLET STOMP, TRITONE CHUG", "Everything in triplets: power chords on the root and the flat five over a two-beat stomp of snare, three kicks and a snare between", r, {}, c },
                         { "SHUFFLE STOMP", "A swung stomp: a low single-note shuffle on 1 and the a of each beat, the kick and floor tom together, no hats", r, {}, c },
                         // (0.241.0) Round ten's choruses (shapes only, our names).
                         { "POWER BALLAD CHORUS", "A big ballad chorus: full chords rung, the kick on 1, the and of 2 and 3, the bass walking into each change on 4, an organ or piano under it", r, {}, c },
                         { "THE FOURTH BAR HIT ON TWO, THREE, FOUR", "A plain chorus whose every fourth bar the whole band hits on 1, 2, 3 and 4 together", r, {}, c },
                         { "ANTHEM PUSH CHORUS", "A chord on 1 rung to the and of 3, the band pushing on 4 and its and into the next, open hats, brass answering in each second half", r, {}, c },
                         { "FIRST CHORUS UNPLUGGED", "The first chorus on acoustics alone: full strums on the beats, a second acoustic's triads on the ands, strings holding, no bass or kit", r, {}, c },
                         { "HALF-TIME BALLAD CHORUS, PUSH ON FOUR", "A two-bar half-time kit with the snare on 3, power chords on 1 and a push on 4, the bass doubling them", r, {}, c },
                         { "HOOK HALF, THEN FOUR ON THE FLOOR", "The first half held chords over a backbeat, the second half eighths over four on the floor with open hats", r, {}, c },
                         { "CHANT BARS", "Every other bar the band hits the four quarters together for a chant, claps on 2 and 4, a chord rung between", r, {}, c },
                         { "BASS DROPS OUT FOR THE CHORUS", "The bass and the low guitar out for the chorus: strings held every two bars over the verse's loop", r, {}, c },
                         { "ORCHESTRAL PULSE, NO SNARE", "No snare: the kick on the eighths under ride and pedal hat, string stabs on 1, the and of 2 and 3, a run up into every bar", r, {}, c },
                         { "TWO-BAR STAB CHORUS", "Short stabs on 1 and the and of 2, then on 1 and 3, piano triads on the eighths under them, the ride bell on the quarters", r, {}, c },
                         { "BALLAD CHORUS, MUTED SIXTEENTHS THROUGH THE CHORD", "Two palm-muted sixteenth runs a bar through each chord's notes over a ballad groove, the keys lifting on 2, its and and 3", r, {}, c },
                         { "HELD CHORDS UNDER PIANO TRIADS", "Power chords held under the piano's eighth triads, pushes on the and of 3 and the and of 4, the bass leaving beat 3 empty", r, {}, c },
                         // (0.252.0) Round eleven's choruses, part one (shapes only, our names).
                         { "HELD CHORDS OVER THE STOMP, STABS AROUND THE BEATS", "Wide chords held a bar over a stomp of a lone kick and snares on 2, 3 and 4, high octave dyads on every sixteenth but the beats", r, {}, c },
                         { "RING ON ONE, PUSHES ON TWO-A AND THREE-AND", "A power chord rung on 1, then pushed hits on 2, 2-a, 3-and, 4 and 4-e, each held to the next, the kick with them", r, {}, c },
                         { "FOUR CHORDS IN THE FIRST HALF, PEDAL BAR ANSWERS", "Four chords on the eighths of beats 1 and 2 with the back half silent, then a bar of low root pedal answering", r, {}, c },
                         { "QUARTER CHORDS, PUSH ON FOUR-AND, CRASH QUARTERS", "Half time: power chords on every quarter with the next pushed on 4-and, the crash on every quarter, an acoustic on the same quarters", r, {}, c },
                         { "EIGHTH CHORDS MOVING BY THIRDS AND HALF STEPS", "Guitar and bass in unison on one power chord a bar in eighths, the kick in pairs and the snare on every and", r, {}, c },
                         { "PHRYGIAN STABS OVER A DOUBLE-KICK PEDAL", "Root sixteenths with the flat two's power chord struck on 1, 3 and 4-and, the kick on every sixteenth", r, {}, c },
                         { "ROLLING SNARE BAR, BACKBEAT BAR, STRUMMED EIGHTHS", "Chords strummed on every eighth over a bar of rolling snares on 2, 3, 4 and its and, then a plain backbeat bar", r, {}, c },
                         { "CHORDS AND KICKS ON THE ANDS", "Power chords only on the ands with a kick and open hat on each, closed hats on the beats, a picked bar every other", r, {}, c },
                         { "UNISON SIXTEENTH RIFF, KICK ON THE A'S", "Guitar and bass in a sixteenth unison riff, the next chord in its last four, the kick on 1 and the a's of 1, 2 and 3", r, {}, c },
                         { "CRASH WASH OVER TRITONE STABS", "The root, then power chords on the flat five in a pushed figure, a walk back into the root in the second bar, the crash in broken sixteenths", r, {}, c },
                         { "HOOK IN BLOCK CHORDS OVER ROOT EIGHTHS", "A hook played as close triads moving together in eighths over the bass's root on every eighth, a fill every eight bars", r, {}, c },
                         // (0.253.0) Round eleven's choruses, part two (shapes only, our names).
                         { "ONE CHORD A BAR, TIED OVER EVERY OTHER", "A chord a bar in eighths, every other bar the last chord tied over its 1 before the new one comes in, crash quarters", r, {}, c },
                         { "OCTAVE LEAP, CLIMB TO THE FLAT SEVEN", "Guitar and bass in unison single notes over the tonic: the root, its octave, then a climb through the fifth and sixth to the flat seven", r, {}, c },
                         { "TRIPLE STABS UNDER A HORN LINE", "Muted unison stabs of two sixteenths and an eighth on 1 and 2-and, a horn line on the keys, four on the floor", r, {}, c },
                         { "TWO-BAR BROKEN BACKBEAT UNDER HELD PUSHES", "Chords held between pushes on 1, 2-and, 3, 4 and 4-and over a two-bar broken backbeat and accented sixteenth hats", r, {}, c },
                         { "SUS CHORD PICKED OVER THE BALLAD KICK", "A clean chord with its second in place of the third, picked in threes and twos, over a ballad kick and the ride", r, {}, c },
                         { "HALF TIME, KICK BURSTS WITH THE CHUGS", "Half time: root chugs with a push on 4-and, then two power chords, the kick bursting with them, the snare on 3", r, {}, c },
                         { "RIFF PAIR, A TOM RUN CLOSING EVERY SECOND BAR", "Two-bar cells: a low root riff, then pushed chords, a tom run closing the second bar", r, {}, c },
                         { "LOW-HIGH OCTAVE CHUGS IN THREES", "Groups of three sixteenths, the low root then its high octave held, three times, then five sixteenths on the fourth's power chord", r, {}, c },
                         { "TWICE-STRUCK OCTAVE ARPEGGIO BALLAD", "A synth arpeggio in octaves striking each chord tone twice, a pushed piano figure, clean chords on 1 and 3", r, {}, c },
                         { "PUSHED CHORD BAR THRICE, OCTAVE RUN BAR", "Three bars of a pushed chord rhythm, then a bar running the root in low and high octaves", r, {}, c },
                         { "FOUR ON THE FLOOR, HORN ANSWER EVERY OTHER BAR", "Four on the floor with open hats on the beats, quarter power chords, a horn answer of rising chord tones every other bar", r, {}, c },
                         { "ROOT BURST, THEN TWO CHORDS SWAPPING", "A burst on the root, two sixteenths on the fifth, then two chords swapping in sixteenths to 4-and", r, {}, c },
                         { "GALLOP OVER A 3+3+2 KICK", "A gallop on every beat over a kick in threes and twos, strings holding the chord with a hook on top", r, {}, c },
                         { "HALF TIME, THE ONE LEFT OUT EVERY OTHER BAR", "Half time with the crash on every quarter: every other bar the band and the kick leave beat 1 out", r, {}, c },
                         { "KICK ON ONE, TWO-AND AND THREE-AND, ARPEGGIO ABOVE", "A kick on 1, 2-and and 3-and under a sixteenth synth arpeggio and a slap bass figure, two or three picked guitar notes a bar", r, {}, c },
                         { "ROLLING PIANO OCTAVES UNDER CHOPPED STABS", "Piano octaves broken on the sixteenths under short muted chord stabs, a busy kick and a tambourine", r, {}, c } };
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
                         // (0.122.0) The rock songs' bridges (the owner's first 22 MIDI files, shapes only; our names).
                         { "ONE GUITAR RUNS, THE BAND HITS", "One guitar runs on in eighths while the band only hits together, late in the bar, with an empty bar in every four", r, {}, b },
                         { "TWIN TREMOLO RUSH", "Two guitars tremolo-picking a third apart, turning round each bar, the kick and snare together on a gapped sixteenth grid", r, {}, b },
                         { "RING & CHUG SPLIT", "Chugs on a gapped grid with the kick in 3+3+2, then halfway the chords ring whole bars over crash quarters", r, {}, b },
                         { "DRUM BURSTS & PULSE", "No cymbals: kick and snare bursts and bars of sixteenths, a chord rung every two bars over a one-note quarter pulse", r, {}, b },
                         { "LAYERED FLOOR BUILD", "The kick on every eighth and toms closing each bar; the guitars come in by stages and it builds to a snare roll", r, {}, b },
                         { "UPPER-FIFTH CHORDS OVER A MOVING BASS", "Power chords on each chord's third held two bars in eighths while the bass moves under them every bar", r, {}, b },
                         { "CHROMATIC CREEP, NO BACKBEAT", "The low root creeping to its two half steps above, a flick at the bar's end, sparse kicks and no backbeat", r, {}, b },
                         { "CLIMB UNDER A TRILL", "The chords voiced higher bar by bar, the bass walking chromatically into them, the lead trilling two notes", r, {}, b },
                         { "KEYS CARRY THE CHORUS ALONE", "Everyone out but the keys: the chords with their pushes low and a high pedal stepping on top", r, {}, b },
                         // (0.142.0) From the owner's 61 song files (shapes only; our names).
                         { "CLEAN FIGURE, THEN DISTORTED", "A clean figure over the kick alone, then the distorted guitars take the same figure, hats in, a roll into the chorus", r, {}, b },
                         { "TOM OSTINATO, BAND THIN", "No cymbals: two toms in an eighth ostinato, the bass coming in by stages, a high held note over it", r, {}, b },
                         { "TOMS DOUBLE THE PEDAL", "Muted pedal bursts with the toms exactly on them, no kick or cymbals, a slow line falling in later", r, {}, b },
                         { "DOMINANT PEDAL BUILD", "The whole bridge on the key's fifth, building in three stages: quiet, sixteenths, then a carpet and a roll", r, {}, b },
                         { "TREMOLO CLUSTER OVER OPEN CHORDS", "An unbroken high half-step cluster tremolo over open ninth chords, the bass up high, a broken beat", r, {}, b },
                         { "PIANO AND CHOIR ALONE", "Everyone out but the piano's rising quarters and the choir holding the chord, a snare roll back in", r, {}, b },
                         { "TRESILLO BOTH HALVES", "Chords on a 3+3+2 figure in each half bar, the kick on the first half, the snare and a crash on the second", r, {}, b },
                         { "BASS ALONE, DOTTED", "The bass alone in double stops on a dotted hemiola, the drums coming in tribal", r, {}, b },
                         { "RIFF ON THE FOUR, NO BACKBEAT, THEN HOME", "The riff on the key's fourth with the kick and the ride, no snare, then on the tonic with the backbeat", r, {}, b },
                         { "BASS HOLDS THE GRID, THE BAND CARVES IT", "The bass keeps every pair of sixteenths while the band plays only a few of them", r, {}, b },
                         { "DRUMS DROP OUT, THE BAND CARRIES", "Chords in eighths growing every four bars, the drums stopping and the band carrying it alone", r, {}, b },
                         // (0.150.0) From the owner's 54 new song files (round four; shapes only, our names).
                         { "PARALLEL CLASH OVER THE BASS", "The bass alone round its root, then a three-note clash a half step over it on every eighth, over half time", r, {}, b },
                         { "ORCHESTRA ALONE, THEN THE WALL", "Strings and choir alone for the first half, then a wall of root sixteenths with kick pairs in threes", r, {}, b },
                         { "SNARE CARPET OVER THE FLOOR", "The guitars and bass out, a one-note keys loop, the snare on every sixteenth over four on the floor", r, {}, b },
                         { "LONG PULSE BUILD", "The kick on every beat, the snare coming in by stages, a high dyad in eighths over it", r, {}, b },
                         { "PAIRS EVERY THREE UNDER A HELD CHORD", "Low pairs every three sixteenths under a high chord held, a tom fill to end", r, {}, b },
                         { "THE HOOK'S RHYTHM ON LOW CHORDS", "Power chords on the hook's rhythm, on the root and the half step over it, then the chords in eighths", r, {}, b },
                         { "DOUBLED HITS ON ONE AND TWO, THEN HOLD", "Doubled hits on 1 and 2, a hit on the and of 2, then the chord held", r, {}, b },
                         { "BURSTS OF FIVE, LAYERED", "Bursts of five sixteenths, then fours, then a 3+3+3+3+4 strum laid over them", r, {}, b },
                         { "BELL GROOVE UNDER A TWO-CHORD DRONE", "Root, fifth and ninth rung on two chords in turn over a ride bell groove", r, {}, b },
                         { "HIGH CHROMATIC FALL OVER THE CHUG CELL", "The bass alone on a chug cell, then two high notes falling a half step each half bar over it", r, {}, b },
                         { "TRIBAL TOMS, DOTTED OCTAVES", "No snare or cymbals: tribal toms over the kick, root octaves on two figures in turn", r, {}, b },
                         { "UNISON STABS OVER HATS ONLY", "Everyone stabbing root octaves together over only the hats, three hits to end", r, {}, b },
                         { "EIGHTHS WITH AIR AT BOTH ENDS", "Bars in pairs: power chords in eighths, then eighths with air at both ends of the bar", r, {}, b },
                         { "ARPEGGIO UP AN OCTAVE, BASS STEPPING DOWN", "The drums out, an arpeggio an octave up, the bass falling a scale step a bar", r, {}, b },
                         // (0.161.0, round four's versions: All I Need's bridge) the pre-chorus's RISING BASS as a bridge too
                         { "RISING BASS", "Chords held or tremolo picked while the bass climbs a step a bar underneath", RiffFlavour::openChords, {}, SectionType::Band::preChorus },
                         // (0.171.0) Round five (the owner's 12 songs; shapes only, our names).
                         { "PEDAL CELL, FALLING TOP, CHORDS IN THE GAP", "A pedal cell whose top note falls each bar, then high chords rung in the gap it leaves", r, {}, b },
                         { "BASS AND RIDE ALONE, THE GUITAR DRIFTS IN", "The bass's four-bar cell and the ride alone, then single held clashing notes drifting in", r, {}, b },
                         { "LOW HIT, HIGH MAJOR-7TH DYAD, CHROMATIC CLIMB", "Low hits answered by a high dissonant dyad, then power chords climbing by half steps", r, {}, b },
                         { "FLAT-SEVEN OCTAVES, SUS TO MAJOR TOP", "Octave dyads round the flat seven, the top note turning from suspended to major", r, {}, b },
                         { "FLAT-TWO DRIVE TO THE SEVEN", "Eighths on the chord a half step over the root, landing on the flat seven", r, {}, b },
                         { "GUITAR ALONE: A BAR OF LINE, A BAR RUNG", "One guitar alone: a bar of single-note line, then a held figure and a rung chord", r, {}, b },
                         // (0.184.0) Round six (the owner's 25 songs; shapes only, our names).
                         { "BASS ARPEGGIO IN THREES, THE KIT CREEPS IN", "The bass alone on a root, fifth and octave in threes, the kit coming in by stages, a sixteenth guitar line late", r, {}, b },
                         { "ONE HIT, PICKED SINGLE LINE, HATS ONLY", "One hit together, then one guitar picking a line on a pedal over only hats and ride; the band back halfway", r, {}, b },
                         { "STICKS IN CLUSTERS, KICK QUARTERS, NO SNARE", "Side-stick clusters on every beat over a four-on-the-floor kick, a single-note riff, a stab bar every fourth", r, {}, b },
                         { "KICK AND SIDESTICK ON ONE AND THREE, GUITARS OUT", "The guitars out: the kick and a side stick on 1 and 3, root half notes, the keys carrying a line", r, {}, b },
                         { "BASS A STEP UNDER, CLUSTER TREMOLO JOINS", "The bass alone a whole step under the key, then a three-note cluster tremolo over it, stopping on beat 3", r, {}, b },
                         { "HELD CHORDS OVER A SIXTEENTH BASS PEDAL, NO SNARE", "A chord rung each bar over a sixteenth bass pedal, root stabs under it, no snare: the kick, ride and crash pushes", r, {}, b },
                         { "OCTAVES IN 2+1 CELLS OVER SIXTEENTH ROOTS", "Octaves in cells of two and one over the bass's sixteenth roots, crash eighths", r, {}, b },
                         { "RIDE AND BASS SIXTEENTHS UNDER A CLEAN ARPEGGIO", "No kick or snare: the ride and the bass in sixteenths under a clean arpeggio and a high held note, the groove back later", r, {}, b },
                         // (0.197.0) Round seven (the owner's 70 songs; shapes only, our names): three led by the ACOUSTIC lane
                         // (applying one switches it on), then six electric.
                         { "DOWN TO THE ACOUSTIC", "The band out: an acoustic alone, then hats and a shaker join, the last bar one chord rung", r, {}, b },
                         { "HIGH ARPEGGIO OVER A HELD STRUM", "The tonic strummed once every four bars under a high picked figure, then the acoustic's tied strum alone; no drums", r, {}, b },
                         { "PICKED UNISON ARPEGGIO, DRUMS OUT", "Two acoustics in unison: the bass note held on 1, then five eighths of chord tones over it; the drums out", r, {}, b },
                         { "KIT-LED BRIDGE, TWO-NOTE STABS", "The kit carries it: four on the floor and open hats, no snare, toms from halfway, then a backbeat and a roll; two-note stabs every other bar", r, {}, b },
                         { "FIXED HIGH NOTE OVER A WALKING LOW NOTE", "The key's note high on every eighth over the guitar's own low note walking the chords; the drums build in thirds", r, {}, b },
                         { "DISPLACED HALF-TIME BACKBEAT, CHROMATIC CLIMB", "A chord held a bar over the snare on 2-and and 4-and, then a unison figure climbing a half step a bar", r, {}, b },
                         { "TOM CARPET, SNARE ON THE OFF-BEATS", "No kick or backbeat: floor toms in groups of three, the snare and a crash off the beat, high tremolo triads over them", r, {}, b },
                         { "REPEATED-NOTE LINE IN 3+3+2", "A line of repeated notes in groups of 3, 3 and 2 eighths stepping through the chords, answered, over power-chord eighths", r, {}, b },
                         { "CHORDS UNDER A HELD HIGH THIRD", "High power chords on 1, 3 and the a of 4, then 2, 3 and 4, the chord's third held high a bar over them; half time on the ride", r, {}, b },
                         // (0.215.0) Round eight (the owner's 67 songs; shapes only, our names).
                         { "THREE-BAR DOTTED CYCLE UNDER A HIGH PEDAL", "A high root-and-fourth pedal in eighths over a three-bar cycle of the bass and the kick and crash every six sixteenths", r, {}, b },
                         { "HELD THIRDS CLUSTER, KICK AND HATS BUILD", "The key's minor and major third rung together for bars at a time over the kick and hats, the snare from halfway", r, {}, b },
                         { "SNARE ON TWO AND THREE, BASS ON ONE AND FOUR", "The snare on 2 and 3, the bass only on 1 and on 4 a half step under the next root, high root eighths, a held note from halfway", r, {}, b },
                         { "DOTTED CRASHES, CRASHES ON THE ANDS, A SNARE ROLL BAR", "Three-bar cycles of cymbals on the dotted quarters, then the ands, then a snare roll bar, over root and octave eighths", r, {}, b },
                         { "HATS ALONE, BASS PUSHES ON FOUR, TOM BUILD", "Open and pedal hats alone, the bass's one note on 4 held over the bar, a high line, then a kick-and-floor-tom build", r, {}, b },
                         { "BASS, HATS AND TOMS, THEN THE GUITAR AND FLOOR TOM", "The guitars out over the bass, hats and toms, then power-chord eighths with the floor tom on every eighth", r, {}, b },
                         { "HIGH DYAD EIGHTHS OVER SOFT TOM-AND-SNARE EIGHTHS", "A high dyad in eighths alone over soft snare and tom eighths, the bass only in a pickup, a build to end", r, {}, b },
                         { "BROKEN FOUR-BAR BEAT UNDER STEADY EIGHTHS", "Steady power-chord eighths over a four-bar broken beat, two eighths of bass a bar, the last two bars held", r, {}, b },
                         { "WHOLE-BAR CHORDS IN THE FLAT KEY, CHROMATIC SLIDE BACK", "Major chords borrowed from the minor key a bar each, then a single-note slide down into the key's fifth", r, {}, b },
                         { "BASS NEIGHBOUR SIXTEENTHS UNDER LATE DYADS", "The bass in sixteenths round the root and its flat seven, high dyads late in each bar, soft kick and snare under the ride", r, {}, b },
                         { "QUARTER STABS OVER A HELD BASS, KICK 1, 2-A, 3, 3-AND", "Full chords short on every quarter over the bass's held notes, the kick on 1, the a of 2, 3 and its and", r, {}, b },
                         { "RISING FIFTHS ON 1, 1-A, 2-AND, BAND OUT", "The band out: low fifths on 1, the a of 1 and the and of 2 rising up the key, the kick and an open hat with them, the kit back after four bars", r, {}, b },
                         // (0.236.0) Round nine (the owner's 17 industrial songs; shapes only, our names).
                         { "LOOP ALONE, THEN THE KEYS, THEN THE BASS", "A drum loop that never fills plays alone, the keys come in after a quarter, the bass from halfway, a guitar chord only at the end", r, {}, b },
                         // (0.242.0) Round ten (the owner's 56 songs; shapes only, our names).
                         { "QUIET BRIDGE, THEN A SNARE-ON-FOUR REBUILD", "Soft clean dyads with the bass and drums out, then the kick back with the snare only on 4, then the backbeat and a snare fill", r, {}, b },
                         { "UNISON PICKUPS INTO EACH DOWNBEAT", "The guitars and the bass in unison on a four-note pickup into each bar, then held, brass answering, a crash on 1, 2 and 3 to end", r, {}, b },
                         { "TWO-CHORD VAMP, OCTAVE BASS", "The bass carries a vamp in octaves on a syncopated figure, the guitar only stabbing on 4, a snare pickup every other bar", r, {}, b },
                         { "BOLERO CRESCENDO", "No backbeat: a dotted ostinato on the guitar and bass, floor tom and tambourine on its rhythm, the keys joining, everything louder bar by bar, up a fourth to end", r, {}, b },
                         { "TONIC PEDAL, ONE CHORD HELD", "The key's root on the bass's eighths and its chord held on the keys whatever the chords, one long guitar note every four bars, a snare build to end", r, {}, b },
                         { "HITS BAR, EIGHTHS BAR OVER A PEDAL", "Two-bar cycles of power-chord hits on 1, 3, 4 and its and, then a bar of eighths, over the key's root held on the bass", r, {}, b },
                         // (0.255.0) Round eleven (the owner's 61 more songs; shapes only, our names).
                         { "HELD CLUSTERS, RIDE ROLLS ONLY", "Three-note close clusters held a bar or two, no kick or snare, only ride rolls swelling in sixteenths", r, {}, b },
                         { "RUNG SEVENTHS, SNARE ON ONE ONLY", "A seventh chord struck once and rung two bars, an arpeggio up in the fourth bar, the snare on 1 only and the kick on 3 and 4-and", r, {}, b },
                         { "DOTTED SINGLE NOTES OVER THE ARPEGGIO", "An acoustic's ringing sixteenth arpeggio, high single chord tones over it on a dotted quarter then quarters", r, {}, b },
                         { "HIGH AND LOW TRIADS IN SIXTEENTHS", "The chord's high triad and low triad in turn in sixteenths, one high then two low, the kick on the quarters and no snare", r, {}, b },
                         { "QUARTER CHORDS ROCKING BY HALF STEPS", "Power chords on the quarters rocking a half and a whole step round the root, the snare on every beat and the kick between", r, {}, b },
                         { "ROOT, FIFTH, FOURTH EIGHTH CELL, HALF TIME", "An eighth cell of root, fifth and fourth with a gap, a strummed chord rung every other bar, a half-time beat", r, {}, b },
                         { "3+3+2 CHUG UNDER A FALLING OCTAVE LINE", "The root and the flat two on a 3+3+2 kick, a second guitar's octave chords falling a half step a bar from halfway", r, {}, b },
                         { "OPEN ADD-NINE ARPEGGIO, DOTTED CRASHES", "Open add-nine shapes picked in eighths, a chord a bar, crashes on 1, 2-and and 4", r, {}, b },
                         { "NO DOWNBEAT, SNARE RUN INTO FOUR", "Octave pairs falling a step every three sixteenths, the kick only on 2 and 3, a snare run into beat 4", r, {}, b },
                         { "BASS WALK, THEN TOMS, THE KIT, A TREMOLO WALL", "Four stages: the bass alone walking a two-chord pair, then floor toms, then the kick and picked dyads, then a tremolo wall", r, {}, b },
                         { "CLEAN RIFF, KICK EVERY THREE BEATS", "The chords picked quietly over a kick every three beats across the bar lines, quarter hats, no snare until a hit at the end", r, {}, b },
                         { "PIANO ROCKING, A UNISON CLIMB IN EACH BAR'S SECOND HALF", "Piano rocking the low root and the chord, the bass and strings holding the root then climbing into the next chord", r, {}, b },
                         { "BASS RIFF OVER KICK QUARTERS, THEN TOMS, THEN THE ROLL", "A walking bass riff alone over kick quarters, then toms under it, then a snare roll building into the next section", r, {}, b },
                         { "RIDE GALLOP ALONE OVER QUARTER CHORDS", "No kick or snare, the ride in a gallop over quarter power chords stepping down two bars a chord, long held tones above", r, {}, b },
                         { "THREE PUSHED HITS, A BAR OF RIDE", "The band on three pushed hits (1, 2-and, 4-and) with the kick and crash, then a bar of only the ride", r, {}, b },
                         { "CLEAN EIGHTH ARPEGGIO, SNARE ON TWO, THREE-AND, FOUR, FOUR-AND", "A clean root, fifth and octave in eighths over a kick on 1, 1-and, 2-and and 3 and a snare on 2, 3-and, 4 and 4-and", r, {}, b },
                         { "SHARP-FOUR ARPEGGIO, A HELD CHORD BAR", "A bar of an arpeggio falling from the raised fourth to the root, then the tonic chord held a bar, over the ride", r, {}, b },
                         { "HELD POWER CHORD, OCTAVE PICKUP INTO EACH BAR", "A power chord held each bar, an octave pair on the last sixteenths into the next, root eighths under", r, {}, b },
                         { "KIT IN EIGHTHS, THEN CRASH HITS AND TOM BARS", "Kick, snare and floor tom together on every eighth, then pairs of a crash-hit bar and a tom bar, the bass walking into the hits", r, {}, b },
                         { "SNARE ON TWO ONLY, TOM PAIR ON THREE-AND AND FOUR", "The snare on 2 only, a floor tom pair on 3-and and 4, the band pushing each change in on the and of 4", r, {}, b },
                         { "EVERYTHING ON THREE-AND AND FOUR-AND", "The kick, the bass and the strings only on 3-and and 4-and, the snare on 2, sixteenth hats", r, {}, b },
                         { "A NOTE CLIMBING A STEP A BEAT OVER A HELD OCTAVE", "One guitar holding the key's fifth in octaves, a second climbing a scale step every beat in sixteenths", r, {}, b },
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
                         // (0.122.0) The rock songs' breakdowns (the owner's first 22 MIDI files, shapes only; our names).
                         { "ONE BAR OF CHUGS, THREE OF AIR", "A bar of sixteenth chugs, a bar of two hits, two bars of air with only the ride keeping time", r, {}, d },
                         { "CHUGS UNDER SPARSE HITS", "Unbroken low eighths with their half-step neighbours under one or two big drum hits a bar, building at the end", r, {}, d },
                         { "DOUBLE-HIT UNISON RIFF", "The guitars and bass in unison: double hits, held notes and sixteenth runs, the kick on every double hit", r, {}, d },
                         { "ARPEGGIO, THEN THE RIFF UNDER IT", "The drums and guitars out under a keys arpeggio, then the riff and the band come back under it halfway", r, {}, d },
                         // (0.142.0) From the owner's 61 song files (shapes only; our names).
                         { "FOUR ON THE FLOOR, NO SNARE", "The kick on every beat and no snare, the guitars nearly out, the bass's 3+3+2, a build with open hats and a roll", r, {}, d },
                         { "OCTAVE PUMP WALL", "The root and its octave pumped in eighths, the kick on every eighth, the snare and a crash on every beat", r, {}, d },
                         { "BASS RIFF ALONE, GHOST GROOVE JOINS", "The bass alone on a 3+3+2 riff, a ghost-note groove without a backbeat joining, high pings on 3", r, {}, d },
                         { "SEVEN-BEAT RIFF ACROSS THE BAR", "A riff seven beats long running across the 4/4 bar lines over a steady backbeat", r, {}, d },
                         { "KEYS ALONE, THEN RINGING HITS", "The keys alone, then chords rung two bars each over half time, a 3+3+2 pickup to end", r, {}, d },
                         // (0.150.0) From the owner's 54 new song files (round four; shapes only, our names).
                         { "HALF-BAR CHUGS, HALF STEP EVERY FOURTH", "Low chugs in the first half bar, every fourth bar a half step up, the kick doubling them", r, {}, d },
                         { "PAIR STABS, THEN THE THREES", "Pair stabs on 1 and 3, then tremolo chords with the snare and kick in threes against each other", r, {}, d },
                         { "QUIET FALLING CELL OVER A LAMENT BASS", "The drums out: a quiet three-note cell falling a step a bar over a lament bass", r, {}, d },
                         { "HIGH NOTE TREMOLO OVER A HALF-TIME CHUG", "A low unison chug over half time, a high ninth tremolo-picked over it", r, {}, d },
                         // (0.184.0) Round six (the owner's 25 songs; shapes only, our names).
                         { "EIGHTH CHORDS WALKING THE HALF STEPS", "Power chords on the eighths with the bass, walking the root, its minor third, the half step over it and the flat fifth", r, {}, d },
                         { "FIVE-SIXTEENTH BURSTS UNDER A PICKED ARPEGGIO", "Bursts of five sixteenths with the bass and the kick under a picked high arpeggio, the snare on every beat", r, {}, d },
                         { "HALF-STEP FALL OVER THE OPEN ROOT", "Sixteenths a beat at a time: a chord, the same a half step down, the open root twice; a second guitar doubling", r, {}, d },
                         // (0.197.0) Round seven (the owner's 70 songs; shapes only, our names): one led by the ACOUSTIC lane
                         // (applying it switches it on), then four electric.
                         { "ONE ACOUSTIC ALONE", "Everyone out but one acoustic's tied strum, the kit down to a kick and tom with a snare and clap on 3", r, {}, d },
                         { "HELD HIT, UNISON SIXTEENTH PICKUP", "Every lane on one hit held three beats, then four sixteenths together on 4; then a stream of sixteenths and one last hit", r, {}, d },
                         { "THE BAND ANSWERS ON THE LAST EIGHTH", "The guitar's riff alone, the band answering together on the last eighths of every other bar", r, {}, d },
                         { "TOMS, SIDESTICK AND CLAPS", "The guitars and bass out: kick, snare flams, tom sixteenths, a side stick and claps, then muted chugs come back halfway", r, {}, d },
                         { "WIDE CHORD RUNG OVER THE CHUG", "The chug goes on while a second guitar rings a wide open chord on the first bar of every four", r, {}, d },
                         // (0.215.0) Round eight (the owner's 67 songs; shapes only, our names).
                         { "UNISON CHROMATIC LINE, A BAR OF ROOT TREMOLO", "A chromatic eighth line with the bass, then a bar of the root in sixteenths, open hat eighths with no backbeat", r, {}, d },
                         { "ONE HIT ON ONE-AND, EVERY OTHER BAR", "One chord on the and of 1, then silence and a silent bar, the kick and a crash with the hits, a walk up to the root to end", r, {}, d },
                         // (0.236.0) Round nine (the owner's 17 industrial songs; shapes only, our names).
                         { "DRONE AND HALF-STEP CALL OVER THE MACHINE", "A chord held two bars under a high half-step call, a drone in the keys and the bass, over a machine beat that never fills", r, {}, d },
                         { "HALF-STEP TICK OVER A LONE KICK", "Muted quarters on 2, 3 and 4 ticking between the root and the half step above, a lone kick every other bar under ride eighths", r, {}, d },
                         { "KEYS OVER THE MACHINE PULSE", "The bass on every sixteenth and a kick-and-clap machine pulse, a synth chord or arpeggio over it, the guitar out", r, {}, d },
                         { "STABS EVERY OTHER BAR OVER A CLICK", "Short stabs on 1 and its and every other bar with the bass and a kick, nothing between but a click", r, {}, d },
                         { "BASS PULSE UNDER HELD CHORDS, DRUMS OUT", "The drums out: the bass pulsing even eighths under chords held two bars", r, {}, d },
                         { "STOMP ALONE, THEN THE PULSE", "A stomp beat alone with claps, then the guitar's and the bass's muted eighths from halfway", r, {}, d },
                         // (0.242.0) Round ten (the owner's 56 songs; shapes only, our names).
                         { "DRUMS AND VOICES BREAKDOWN", "The band out but one chord: the groove alone for the voices, with claps or the floor tom on the beats", r, {}, d },
                         { "SUNG CHORUS OVER TOMS", "Toms alone under the voices, then the kit with a cowbell on 2 and 4 and the bass returning, a bar of snare sixteenths to end", r, {}, d },
                         { "HIT, THEN THE HOOK ALONE", "The band hits a chord and holds it a bar, then the keys' hook alone over silence", r, {}, d },
                         // (0.256.0) Round eleven (the owner's 61 more songs; shapes only, our names).
                         { "SNARE PAIRS ON THE HALF-STEP CELL", "The low root and the half step above in cells of three sixteenths, the snare in pairs every six sixteenths and the kick in the gaps", r, {}, d },
                         { "UNISON STAB BAR, BLAST BAR, IN TURN", "A bar of unison stabs with kick, crash and china on them, then a bar of the same chords as sixteenth singles over a blast", r, {}, d },
                         { "ONE CHORD RUNG FOUR BARS, SYNCOPATED HALF TIME", "One power chord struck and rung four bars, the bass pushing round the root and the flat three, a syncopated half-time kick", r, {}, d },
                         { "KICK ONE BAR, SNARE THE NEXT, CRASH QUARTERS", "Quarter power chords all through, the kick on 1 one bar and the snare on 1 the next, a crash on every quarter", r, {}, d },
                         { "DOTTED SNARE OVER SIXTEENTH KICKS", "Sixteenth kicks under a snare on 1, 2-and and 4, the guitar and bass tremolo on the root with the flat seven, a stutter cell every fourth pair of bars", r, {}, d },
                         { "FOUR CHUGS, THEN SIX BEATS RUNG", "Two-bar cells: four chugs with kick and crash, then the power chord rung for six beats over kick quarters", r, {}, d },
                         { "TOMS AND KICK ON THE HITS, NO CYMBALS", "Muted power-chord hits on 1, 2-a, 3 and 3-and, the kick and floor tom on exactly them, no snare and no cymbals", r, {}, d },
                         { "BAND HITS ON SNARE AND CRASH, NO KICK", "Every lane hitting together in a two-bar figure, the drums only snare and crash on the hits, no kick", r, {}, d },
                         { "DRUMS OUT, DOTTED SYNTH BASS AND A HIGH THIRD", "The drums out at first: a dotted octave bass riff and a high root and third on the keys, the chug and four on the floor from halfway", r, {}, d },
                         { "SYNCOPATED CHUG, GHOST SNARES", "Held power chords one bar, root chugs from 1-and the next, a syncopated kick and the snare on 3 with ghost notes", r, {}, d },
                         { "FOUR-SIXTEENTH BURST ON ONE, THEN SILENCE", "Four sixteenths on the root and a chord on 2, then the bar left silent, pushed chords into the next every fourth bar", r, {}, d },
                         { "HOOK OVER HELD ROOTS, KICK ON ONE AND FOUR-AND", "The root on 1 and the fifth on 4-and held under a hook on the keys, the kick on 1 and 4-and, open hats, the backbeat in the second half", r, {}, d },
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
                         { "FALLING OCTAVE PEDAL", "The riff under octaves on every eighth, a step lower every four bars, three stabs to end", r, {}, b },   // (0.117.0)
                         // (0.124.0) From the owner's rock songs (shapes only; our names).
                         { "EVERY OTHER BAR EMPTY", "The chorus thinned: a chord a bar and a bar of space in turn, a high arpeggio over it, the last chord held", r, {}, b },
                         { "SINGLE-NOTE BAR, CHORD BAR", "A bar of low single notes, then a bar of power chords, in turn over half time with crash quarters", r, {}, b },
                         // (0.143.0) From the owner's 61 song files (shapes only; our names).
                         { "UNISON LINE TO THE END", "A single-note line on every eighth answered by held chords, power chords to end on one held", r, {}, b },
                         { "DOTTED-EIGHTH HIT CHAIN", "The band hits on the one, then every three sixteenths across the bar lines, the last chord held", r, {}, b },
                         { "UNISON CLIMB TO THE END", "Everyone climbing in sixteenths a step a beat, the kick filling in to every sixteenth, a hit to end", r, {}, b },
                         { "THREE DOTTED PUSHES, THEN RING", "Chords in eighths, then three pushes on 1, the a of 1 and the and of 2, the last chord rung", r, {}, b },
                         { "HITS AT BOTH ENDS", "The band only on 1, 1-and, 4 and 4-and, the last bar one hit", r, {}, b },
                         { "CHANT OVER A PEDAL BURST", "A low pedal burst a bar, a choir climbing a note a bar over it, crashes on the dotted eighths to end", r, {}, b },
                         { "BAND DROPS TO PADS AND PIANO", "The band stops on a splash, the key's note held, then strings pulsing and high piano to the end", r, {}, b },
                         { "TOM WALL UNDER THE LAST CHORD", "The riff, then the last chord held over the kick and floor tom on every sixteenth", r, {}, b },
                         // (0.151.0) From the owner's 54 new song files (round four; shapes only, our names).
                         { "BURSTS OFF THE BEAT TO THE END", "Bursts of five sixteenths off the beat with octave stabs between them, ending on one burst held", r, {}, b },
                         { "BASS RIFF ALONE TO THE END", "The bass alone on its riff to the end, everyone else out", r, {}, b },
                         { "CALL BAR, ANSWER BAR, CUT", "A bar of doubled hits answered by a bar of eighths, the last bar cut after beat 2", r, {}, b },
                         { "OSTINATO AND BEAT TO THE END", "A two-note keys figure and a plain beat to the end, the band out", r, {}, b },
                         // (0.185.0) Round six (the owner's 25 songs; shapes only, our names; also intro types).
                         { "HORN LINE ALONE, CRASH ON ONE", "A keys line in octaves alone, stepping up in eighths into a held note, one crash on the first 1; the band in on beat 3 of the last bar", r, {}, b },
                         { "TWO ROOTS AND A RUNG CHORD", "Two low roots and a ringing add-nine chord on every beat, the shape fixed over the pedal, a pickup on the and of 4 every fourth bar", r, {}, b },
                         // (0.198.0) Round seven (the owner's 70 songs; shapes only, our names): all led by the ACOUSTIC lane.
                         { "ROLLED LAST CHORD, FALLING TOP", "The strum to the last bar, its chord rolled low, middle, top, then three single notes falling from its top, ringing", r, {}, b },
                         { "STRUM OFF THE ONE TO THE END", "Full chords strummed off the beat, each coming in on beat 3, a crash and a tom fill every four bars; the last chord held", r, {}, b },
                         { "INTRO PICKING RETURNS, ONE CHORD HELD", "The intro's picked arpeggio back, alone, ending on one chord held a bar", r, {}, b },
                         { "DOTTED COUNTER-LINES OVER THE ARPEGGIO", "A picked arpeggio with a twin under it, a dotted counter-line stepping down over it, a second a third under from halfway", r, {}, b },
                         { "CHORD TO PEDAL, LAST CHORD HELD", "The chord to the open pedal strummed, a tambourine on 2 and 4, the bass walking up every other bar; the last chord held", r, {}, b },
                         // (0.236.0) Round nine (the owner's 17 industrial songs; shapes only, our names).
                         { "MACHINE STOMP OUTRO", "Power-chord eighths over the kick on every eighth and the snare and a splash on every quarter, the last bar one hit", r, {}, b },
                         { "HALF-TIME CHORDS OVER A MARCH", "Big chords on 1 and 3 over a snare march with no kick, the bass with the chords", r, {}, b },
                         // (0.243.0) Round ten (the owner's 56 songs; shapes only, our names).
                         { "VAMP TO A FADE", "The last chords looped, the band getting quieter bar by bar to the end", r, {}, b },
                         { "RIFF VAMP ON THE RELATIVE MINOR", "A new sixteenth pentatonic riff on the key's relative minor with the bass, a new groove, syncopated keys", r, {}, b },
                         { "SOLO OVER SNARE QUARTERS, BASS CLIMBING", "The snare on every quarter, the bass climbing a step a bar, chords rung under a solo to the end", r, {}, b },
                         { "ONE HIT A BAR TO THE END", "The band hits each bar's 1 together and lets go", r, {}, b },
                         { "EVERY BEAT HIT, THEN THE GUITAR ALONE", "The whole band hits every beat for the first half, then the guitar carries on alone", r, {}, b },
                         { "STRINGS SIXTEENTHS, ENDING ON THE FOUR", "Strings in sixteenths over whole-note bass, a tom roll, the last bar on the key's four chord", r, {}, b },
                         { "ENDS ON FLAT-SIX TO FLAT-SEVEN", "The last two bars the key's flat six and flat seven, the last struck once and held: unresolved", r, {}, b },
                         { "TOM BUILD INTO A DOUBLE-TIME CODA", "A tom build with hits on 3, then a double-time coda of eighths in unison, a snare stop to end", r, {}, b },
                         { "MAJOR TONIC TO END", "A pedal and strings in sixteenths building over a tom roll, the last chord the key's tonic as a major chord", r, {}, b },
                         { "HAND PERCUSSION, BAND OUT", "The band out: congas and bongos in sparse sixteenths and one soft pad note under the voices", r, {}, b },
                         { "THE KIT THINS, THE LAST CHORD RINGS", "The kit thinning to the kick on 1 and the ride, the guitar thinning with it, the last chord rung to the end", r, {}, b },
                         // (0.257.0) Round eleven (the owner's 61 more songs; shapes only, our names).
                         { "WHOLE-BAR CHORDS OVER A BASS ON ONE, THREE AND FOUR", "A chord a bar on one guitar, power chords on 1, 3 and 4 on the other and the bass, a half-time beat under crash quarters, one hit to end", r, {}, b },
                         { "EIGHTHS ON ONE AND FIVE IN TURN", "The tonic in eighths from the and of 1, then the fifth in eighths, in turn to the end, long high notes over it, the last bar rung", r, {}, b },
                         { "THE RIFF, THREE HITS AND OUT", "The riff over kick quarters, then the last bar's hits on 2, 2-and and 3 and nothing after", r, {}, b },
                         { "GUITAR ALONE, THEN THREE HITS RUNG TWICE", "The guitar alone, then three band hits on 1, 3 and 4-and rung a bar, twice, the last chord held to the end", r, {}, b },
                         { "OCTAVE TREMOLO CREEPING UP", "Root octaves in sixteenths climbing a half step every two beats, kick sixteenths and crash quarters under them, a hit to end", r, {}, b },
                         { "SNARE QUARTERS, KICK ON THREE-AND AND FOUR-AND", "The snare on every quarter, the kick only on 3-and and 4-and, hats on the ands, a slap bass figure and a keys arpeggio", r, {}, b },
                         droning, stabs };
            }
            case 6: return { driving, droning };
            default: break;
        }
        return {};
    }

    static const std::vector<SectionType>& cachedTypesFor (const juce::String& sectionName)
    {
        static std::recursive_mutex lock;
        static std::map<juce::String, std::vector<SectionType>> kept;
        const std::lock_guard<std::recursive_mutex> l (lock);
        auto found = kept.find (sectionName);
        if (found == kept.end())
            found = kept.emplace (sectionName, sectionTypesUncached (sectionName)).first;
        return found->second;   // (never erased: the reference stays good)
    }

    std::vector<SectionType> sectionTypesFor (const juce::String& sectionName) { return cachedTypesFor (sectionName); }

    std::optional<SectionType> sectionTypeNamed (const juce::String& sectionName, const juce::String& type)
    {
        for (const auto& t : cachedTypesFor (sectionName))
            if (t.name.equalsIgnoreCase (type.trim()))
                return t;
        return std::nullopt;
    }

    static std::optional<SectionType> sectionTypeCalledUncached (const juce::String& type)
    {
        if (type.isEmpty())
            return std::nullopt;
        for (const auto* name : { "Intro", "Verse", "Chorus", "Bridge", "Breakdown", "Ending", "Solo", "Pre-Chorus", "Interlude" })
            if (auto t = sectionTypeNamed (name, type))
                return t;
        return std::nullopt;
    }

    std::optional<SectionType> sectionTypeCalled (const juce::String& type)
    {
        if (type.isEmpty())
            return std::nullopt;
        static std::mutex lock;
        static std::map<juce::String, std::optional<SectionType>> kept;
        const std::lock_guard<std::mutex> l (lock);
        auto found = kept.find (type);
        if (found == kept.end())
            found = kept.emplace (type, sectionTypeCalledUncached (type)).first;
        return found->second;
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
                                                      "SLIDES INTO THE CHANGES", "STRUMMED FULL CHORDS", "MOVING POWER CHORDS",
                                                      "PUSHED FOUR-CHORD TURNAROUND" };   // (0.117.0)
                case ComposerLayer::bass: return { "ROOT EIGHTHS", "DOUBLES THE GUITAR", "WALKING INTO THE CHANGES", "OCTAVE EIGHTHS",
                                                    "DOMINANT PEDAL ENDING", "LOCKED WITH THE KICK" };   // (0.117.0: the pedal before the kick)
                case ComposerLayer::chords: return { "HELD PADS", "ANSWERS IN BARS 3 AND 4", "HIGH EIGHTH PULSE", "SILENT" };
                case ComposerLayer::lead: return { "RINGING ARPEGGIO", "HELD NOTES & A SQUEAL", "DOUBLES THE RHYTHM AN OCTAVE UP",
                                                    "FALLING OCTAVE PEDAL" };   // (0.117.0)
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
                case ComposerLayer::drums: return { "PLAIN BACKBEAT", "DRIVING KICK", "HALF TIME", "FOUR ON THE FLOOR" };   // (0.118.0)
                case ComposerLayer::melody: break;
            }
        if (flavour == RiffFlavour::syncopatedDrive)
            switch (layer)
            {
                // (0.99.0, the owner's Another Life chorus and Faster's syncopated bar; our names)
                case ComposerLayer::guitar: return { "STABS, THEN EIGHTHS", "SYNCOPATED THIRD BAR", "PUSHED CHANGES", "STABS, THEN PUSHES",
                                                      "PUSHED FOUR-CHORD TURNAROUND" };   // (0.117.0)
                case ComposerLayer::bass: return { "LOCKED SYNCOPATION", "ROOT EIGHTHS", "DOUBLES THE GUITAR", "DOMINANT PEDAL ENDING",
                                                    "LOCKED WITH THE KICK" };
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
        if (isRound3Type (type))
            return round3Approaches (type, layer);
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
        // (0.122.0) The rock songs' bridges (Composer.cpp's RockSection writes them).
        struct Lists { const char* name; juce::StringArray guitar, bass, keys, drums, lead; };
        static const Lists lists[] {
            { "ONE GUITAR RUNS, THE BAND HITS", { "DYADS RUN, HITS ON TOP", "DYADS RUN ONLY" }, { "THE HITS ONLY", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "ONE HELD NOTE" }, { "KICK & SNARE ON THE HITS", "HITS & A RIDE" }, {} },
            { "TWIN TREMOLO RUSH", { "TREMOLO IN THIRDS", "TREMOLO, ONE VOICE" }, { "ROOTS ON THE GRID", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "KICK & SNARE ON THE GRID", "GRID, OPEN HATS BETWEEN" }, {} },
            { "RING & CHUG SPLIT", { "CHUGS, THEN RING", "CHUGS ALL THROUGH" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "3+3+2, THEN CRASH QUARTERS", "3+3+2 ALL THROUGH" }, {} },
            { "DRUM BURSTS & PULSE", { "A CHORD EVERY TWO BARS", "A CHORD EVERY BAR" }, { "ROOTS HELD", "SILENT", "LOCKED WITH THE KICK" },
              { "QUARTER PULSE", "LONG NOTES", "SILENT" }, { "BURSTS, THEN SIXTEENTHS OR SILENCE", "BURSTS ONLY" }, {} },
            { "LAYERED FLOOR BUILD", { "OUT, OSTINATO, THEN TREMOLO", "OSTINATO FROM HALFWAY" }, { "ROOT QUARTERS", "DOTTED QUARTERS", "LOCKED WITH THE KICK" },
              { "ONE HELD NOTE A BAR", "DOTTED QUARTERS UP HIGH", "SILENT" }, { "FLOOR BUILD INTO A ROLL", "FLOOR BUILD, NO ROLL" }, {} },
            { "UPPER-FIFTH CHORDS OVER A MOVING BASS", { "EIGHTHS ON THE UPPER FIFTH", "HELD ON THE UPPER FIFTH" }, { "MOVING EACH BAR IN EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "HELD PADS", "SILENT" }, { "DRIVING BEAT", "HALF TIME" }, {} },
            { "CHROMATIC CREEP, NO BACKBEAT", { "CREEP & HIGH STABS", "CREEP ONLY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "LOW DRONE" }, { "NO BACKBEAT, OPEN HATS", "NO BACKBEAT, RIDE" }, { "LONG NOTES AT THE END", "SILENT" } },
            { "CLIMB UNDER A TRILL", { "CHORDS CLIMBING", "EIGHTHS CLIMBING" }, { "WALKING INTO EACH CHORD", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "DRIVING BEAT", "HALF TIME" }, { "TRILL IN EIGHTHS", "TRILL IN SIXTEENTHS" } },
            { "KEYS CARRY THE CHORUS ALONE", { "GUITARS OUT" }, { "BASS OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "PUSHED CHORDS & HIGH PEDAL", "PUSHED CHORDS ONLY" }, { "DRUMS OUT", "A KICK ON ONE" }, { "LEAD OUT", "LONG NOTES" } },
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
                    case L::melody: return {};
                }
        return {};
    }

    // (0.111.0) The intro and outro types' options, lane by lane (the bass's last always LOCKED WITH THE KICK; only
    // DOWNBEAT HITS UNDER A LEAD has a lead list).
    juce::StringArray verseApproaches (const juce::String& type, ComposerLayer layer)
    {
        if (isRound3Type (type))   // (0.146.0)
            return round3Approaches (type, layer);
        // (0.118.0) The verse types' options (the bass's last always LOCKED WITH THE KICK; no lead lists: the lead plays
        // its own styles; the keys and the drums their own).
        using L = ComposerLayer;
        struct Lists { const char* name; juce::StringArray guitar, bass, keys, drums; };
        static const Lists lists[] {
            { "PICKED LINE, KICK IN THREES", { "PICKED LINE", "PICKED LINE OVER CHORDS" }, { "ROOT EIGHTHS", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "3+3+2", "RUNNING THREES", "STRAIGHT ALTERNATION" } },
            { "PICKED OSTINATO", { "HIGH PICKED CELL", "LOW PICKED LINE", "HIGH TREMOLO", "OUT" }, { "CELLS OF THREE", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "SIXTEENTH ARPEGGIO" }, { "LIGHT FULL BEAT", "KICK AND HATS ONLY" } },
            { "SKANK LINE", { "UNISON LINE", "LINE WITH A THIRD ABOVE" }, { "DOUBLES THE GUITAR", "PEDAL ONLY", "LOCKED WITH THE KICK" },
              { "SILENT", "LOW PEDAL PAD" }, { "SKANK BEAT", "SKANK ON THE RIDE" } },
            { "LINE OVER HITS", { "LINE OVER RUNG CHORDS", "LINE ALONE" }, { "CHORD HITS", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "HITS, THEN DOTTED", "CRASH ON ONE ONLY" } },
            { "THREE BARS AND A STOP", { "PEDAL + MOVING TOP", "SIXTEENTH STREAM" }, { "SIXTEENTH ROOTS", "ROOT EIGHTHS", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "FOUR ON THE FLOOR", "PLAIN BACKBEAT" } },
            { "SIDESTICK PULSE", { "LOW EIGHTHS", "CHORD RUNG A BAR" }, { "IN HALFWAY", "ROOT EIGHTHS", "LOCKED WITH THE KICK" },
              { "HIGH PEDAL", "SILENT" }, { "SIDESTICK QUARTERS", "SIDESTICK AND HATS" } },
            { "OFF-BEAT SNARE", { "RUNG BAR, SILENT BAR", "LOW BURSTS" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "OFF-BEAT SNARE", "OFF-BEAT SNARE, CRASH EIGHTHS" } },
            { "CHUGS, THEN DOTTED HITS", { "CHUGS, THEN DOTTED HITS", "CHUGS ONLY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "DISPLACED KICK", "KICK WITH THE CHUGS" } },
            { "PEDAL RIFF IN SIXTEENTHS", { "PEDAL EVERY OTHER NOTE", "PEDAL IN PAIRS" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "PLAIN ROCK BEAT", "KICK WITH THE RIFF" } },
            { "TRESILLO RIFF", { "TRESILLO, THEN REST", "RIFF, THEN PULSE" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "HIGH FIFTH OSTINATO", "SILENT" }, { "HATS AND SPARSE KICK", "SNARE ON THE RIFF" } },
            // (0.139.0)
            { "BASS RIFF, HIGH NOTES RUNG", { "HIGH OCTAVES RUNG", "ONE HIGH NOTE A BAR", "HIGH SUS CHORD RUNG" }, { "SIXTEENTH RIFF", "RIFF WITH GAPS", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "KICK WITH THE BASS", "PLAIN BACKBEAT" } },
            { "RIFF BAR, HOOK BAR", { "OUT", "DOUBLES THE RIFF BAR", "OUT, THEN DOUBLES THE RIFF BAR" }, { "RIFF BAR, SILENT BAR", "RIFF EVERY BAR", "LOCKED WITH THE KICK" },
              { "HOOK IN THE GAP BARS", "HOOK EVERY BAR" }, { "KICK WITH THE RIFF BAR, TOMS IN THE GAP", "HATS IN THE GAP" } },
            { "DRUMS, BASS, GUITAR IN TURN", { "CHUGS, THEN A LINE", "CHUGS TO THE END" }, { "RIFF FROM THE SECOND QUARTER", "ROOTS FROM THE SECOND QUARTER", "LOCKED WITH THE KICK" },
              { "SILENT", "PADS IN THE LAST QUARTER" }, { "TOMS, THEN THE BEAT", "HATS, THEN THE BEAT" } },
            { "TOMS AND OCTAVE BASS", { "ONE CHORD RUNG EVERY TWO BARS", "OUT" }, { "OCTAVE EIGHTHS", "ROOT EIGHTHS", "LOCKED WITH THE KICK" },
              { "HELD PADS", "SILENT" }, { "TOMS DOWN THE KIT", "TOMS AND HATS" } },
            { "TOM FIGURE VERSE", { "OUT, THEN CHUGS ON THE TOMS", "CHUGS ON THE TOMS" }, { "WITH THE CHUGS", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "TWO-BAR TOM FIGURE", "TOM FIGURE WITH HATS" } },
            { "SIX-SIXTEENTH CELLS", { "ROOT AND TRITONE CELLS", "ROOT AND FIFTH CELLS" }, { "DOUBLES THE GUITAR", "ROOTS ON THE CELLS", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "KICK PAIRS ON THE CELLS", "TOMS ON THE CELL" } },
            { "HOLED SIXTEENTH PEDAL", { "MUTED ROOT ON THE STREAM", "STREAM WITH A HIGH SIREN" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "HALF-STEP SIREN" }, { "KICK AND TOMS ON THE STREAM", "KICK ON THE STREAM, BACKBEAT" } },
            { "GUITAR CALLS, BASS ANSWERS", { "THREE-BEAT CALL", "TWO-BEAT CALL" }, { "ANSWERS IN THE GAP", "ANSWERS AND DOUBLES", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "KICK ON THE CALL, SNARE ON THE ANSWER", "PLAIN BACKBEAT" } },
            { "THREE-BAR CYCLE", { "MUTED CYCLE", "POWER CHORD CYCLE" }, { "THE CYCLE, AN OCTAVE DOWN", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "SNARE EVERY SIX, NO BACKBEAT", "PLAIN BACKBEAT" } },
            { "THREE-BAR RIFF, KICK AND BASS ON THE BEATS", { "THREE-BAR RIFF", "THREE-BAR RIFF, CHORDS" }, { "BEATS ONE TO THREE", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "HIT ON THREE" }, { "KICK ON ONE TO THREE, SNARE ON THREE", "PLAIN BACKBEAT" } },
            { "KICK AND BASS IN UNISON", { "OUT, THEN BURSTS AFTER TWO AND FOUR", "OUT" }, { "SYNCOPATED LINE WITH THE KICK", "ROOT ON ONE, THEN THE LINE", "LOCKED WITH THE KICK" },
              { "STRINGS HOLD THE ROOT", "HIGH BELL NOTES" }, { "KICK WITH THE BASS, SNARE LATE", "KICK WITH THE BASS, RIDE" } },
            { "BASS GROOVE, GUITAR SQUEALS", { "SQUEAL BAR, BURST BAR", "BURSTS ONLY" }, { "SIXTEENTH GROOVE", "ROOT EIGHTHS", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "FUNK KICK", "PLAIN BACKBEAT" } },
        };
        const auto t = type.trim().toUpperCase();
        for (const auto& list : lists)
            if (t == list.name)
                switch (layer)
                {
                    case L::guitar: return list.guitar;
                    case L::bass: return list.bass;
                    case L::chords: return list.keys;
                    case L::drums: return list.drums;
                    default: return {};
                }
        return {};
    }

    juce::StringArray bookendApproaches (const juce::String& type, ComposerLayer layer)
    {
        if (isRound3Type (type))
            return round3Approaches (type, layer);
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
            { "FALLING OCTAVE PEDAL", { "RIFF, THEN THE STAB BAR", "EIGHTHS, THEN THE STAB BAR" }, {}, { "SILENT", "HELD PADS" }, { "KICK EIGHTHS, CRASH QUARTERS", "DRIVING BACKBEAT" }, { "FALLING OCTAVES", "SILENT" } },   // (0.117.0)
            // (0.124.0) The rock songs' intros and outros (written by RockSection in Composer.cpp).
            { "BASS RIFF WITH TOMS", { "HIGH TICK ON THE QUARTERS", "OUT, THEN THE RIFF" }, { "THE RIFF ALONE", "ROOTS HELD", "LOCKED WITH THE KICK" }, { "SILENT", "LOW DRONE" }, { "TOMS WITH THE BASS, KICK AND SNARE LATE", "TOMS ONLY" }, {} },
            { "PEDAL SEESAW, DOUBLE KICK", { "SEESAW CHUGS", "PEDAL CHUGS" }, {}, { "SILENT", "LOW DRONE" }, { "DOUBLE KICK, SNARE QUARTERS", "HALF TIME, CRASH QUARTERS" }, { "HIGH TWO-VOICE LINE", "SILENT" } },
            { "TWO CELLS IN THREES", { "ROOT-ROOT-FIFTH IN THREES", "WITH A FLICK, THEN STRUMMED" }, {}, { "STEPPING CELL IN THREES", "SILENT" }, { "SYNCOPATED KICK, OFF-SIXTEENTH HATS", "DRUMS OUT" }, {} },
            { "KIT HITS IN PAIRS", { "POWER CHORDS IN PAIRS", "WALL OF EIGHTHS" }, {}, { "DOTTED-QUARTER HIGH NOTES", "SILENT" }, { "KIT HITS IN PAIRS", "CRASH QUARTERS" }, {} },
            { "GAPPED PEDAL, BUILD INTO THE VERSE", { "GAPPED SIXTEENTHS", "GAPPED SIXTEENTHS, RUNS" }, {}, { "SILENT", "HELD PADS" }, { "KICK ON EVERY CHUG, THEN THE BUILD", "HALF TIME, THEN THE BUILD" }, {} },
            { "EVERY OTHER BAR EMPTY", { "CHORD HELD, BAR EMPTY", "CHORD STRUCK IN EIGHTHS, BAR EMPTY" }, { "ROOT EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" }, { "HELD PADS", "SILENT" }, { "GROOVE, DOWNBEAT-ONLY BAR", "HALF TIME, DOWNBEAT-ONLY BAR" }, { "HIGH QUARTER ARPEGGIO", "SILENT" } },
            { "SINGLE-NOTE BAR, CHORD BAR", { "SINGLE NOTES, THEN CHORDS", "CHORDS ALL THROUGH" }, {}, { "DOTTED-QUARTER HIGH NOTES", "SILENT" }, { "HALF TIME, SYNCOPATED KICK", "DRIVING BACKBEAT" }, {} },
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
        if (isRound3Type (type))
            return round3Approaches (type, layer);
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
        if (isRound3Type (type))   // (0.148.0)
            return round3Approaches (type, layer);
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
            // (0.140.0)
            { "DOTTED STREAM", { "DOTTED STREAM", "PUSHED THREES" }, { "ON THE DOTTED HITS", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SOFT SIXTEENTH ARPEGGIO", "SILENT" }, { "KICK ON THE DOTTED HITS", "KICK WITH THE GUITAR" }, { "LINE ON THE DOTTED GRID", "SILENT" } },
            { "NEIGHBOUR TRILL WALL", { "FULL CHORDS EVERY SIXTEENTH", "CHORDS IN EIGHTHS, TRILL ON TOP" }, { "TRILL ROOT AND STEP BELOW", "ROOT SIXTEENTHS", "LOCKED WITH THE KICK" },
              { "HELD PADS", "SILENT" }, { "PLAIN BACKBEAT, OPEN HATS", "DOTTED KICK, OPEN HATS" }, { "TRILL ROOT AND STEP BELOW", "TRILL ROOT AND THIRD" } },
            { "TWO RUNG CHORDS A BAR", { "TWO RUNG CHORDS", "SECOND A THIRD UP" }, { "HELD ROOTS, PICKUP INTO EACH PHRASE", "WITH THE STRIKES", "LOCKED WITH THE KICK" },
              { "STRINGS DOUBLE THE CHORDS HIGH", "SILENT" }, { "CRASH ON BOTH STRIKES", "RIDE, CRASH ON ONE" }, {} },
            { "THREE DOTTED PUSHES", { "RING, THEN THREE PUSHES", "THREE PUSHES EVERY BAR" }, { "ON THE PUSHES", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "STRINGS HIGH, HELD", "SILENT" }, { "KICK ON THE PUSHES, RIDE OFF-BEATS", "HALF TIME" }, {} },
            { "FOUR ON THE FLOOR, DYAD RIFF", { "DYAD RIFF, 3+3+2 CHANGES", "DYAD RIFF EVERY BAR" }, { "SYNCOPATED SIXTEENTHS", "ROOT EIGHTHS", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "FOUR ON THE FLOOR, OPEN HATS OFF-BEAT", "FOUR ON THE FLOOR, SNARE FILL EVERY EIGHT" }, {} },
            { "SPLIT ON THE AND OF THREE", { "SPLIT ON THE AND OF THREE", "SPLIT, SECOND HALF HELD" }, { "WITH THE GUITAR, WALK AT THE END", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "STRINGS CHANGE WITH THE GUITAR", "SILENT" }, { "KICK ON THE ACCENTS, RIDE EIGHTHS", "DOUBLED KICK" }, {} },
            { "TREMOLO DYADS OVER RUNG CHORDS", { "CHORDS RUNG IN HALVES", "CHORDS RUNG, THEN QUARTERS" }, { "ROOTS HELD", "ROOT EIGHTHS", "LOCKED WITH THE KICK" },
              { "CHOIR NOTE HELD", "SILENT" }, { "FLOOR TOM BEAT", "FLOOR TOM BEAT, CRASH ON ONE" }, { "TREMOLO DYADS", "TREMOLO ON THE ROOT" } },
            { "DOTTED CHORDS, HIGH TREMOLO NOTE", { "DOTTED CHORDS", "DOTTED CHORDS, RUNS AT THE END" }, { "DOTTED QUARTERS", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "CRASH EIGHTHS, DOTTED KICK", "RIDE EIGHTHS, DOTTED KICK" }, { "HIGH TREMOLO NOTE", "SILENT" } },
            { "THE RIFF IS THE CHORUS", { "CHROMATIC UNISON RIFF", "RIFF WITH STOPS" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "LOW DRONE" }, { "KICK ON THE RIFF, CHINA ACCENTS", "KICK ON THE RIFF, BACKBEAT" }, {} },
            { "HIGH TREMOLO DYAD OVER THE RIFF", { "GAPPED ROOT AND TRITONE", "GAPPED POWER CHORDS" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "LOW DRONE" }, { "KICK ON THE RIFF, HAT FOOT WITH IT", "KICK ON THE RIFF, BACKBEAT" }, { "HIGH DISSONANT TREMOLO", "HIGH TREMOLO FIFTH" } },
            { "HELD CHORDS IN 3+3+2", { "THREE HELD CHORDS", "THREE HELD, ALL ON THE BAR'S CHORD" }, { "WITH THE CHORDS", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "HELD PADS", "SILENT" }, { "KICK ON THE 3+3+2, CRASH", "HALF TIME" }, {} },
            { "DOTTED PEDAL CELL", { "SIX-SIXTEENTH CELL, FIXED TOP", "CELL, MUTED" }, { "ROOT PAIRS", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "TWO VOICINGS A BAR", "SILENT" }, { "KICK ON THE PAIRS, OPEN HATS OFF-BEAT", "RIDE BELL EIGHTHS" }, {} },
            { "FIXED SHAPE, CREEPING ROOT", { "RINGING ARPEGGIO, FIXED TOP", "FIXED TOP, CHORD ON ONE" }, { "WHOLE-BAR ROOTS", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "CHINA ON ONE, OPEN HATS", "CRASH AND KICK ON ONE" }, { "HIGH TWO-NOTE HOOK", "SILENT" } },
            { "CHROMATIC CHORDS UNDER A FIXED TREMOLO", { "HALF BAR STRUCK, HALF BAR GAP", "STRUCK, THEN HELD" }, { "WITH THE HITS", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "KICK WITH THE HITS, SNARE ON THREE", "KICK WITH THE HITS, BACKBEAT" }, { "FIXED HIGH TREMOLO CHORD", "FIXED HIGH TREMOLO NOTE" } },
            { "HALF-NOTE CHORDS, OFF-BEAT OPEN HATS", { "CHORDS ON ONE AND THREE", "WHOLE-BAR CHORDS" }, { "HALVES", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "STRINGS LINE ON TOP", "SILENT" }, { "OFF-BEAT OPEN HATS", "OFF-BEAT OPEN HATS, RIDE" }, {} },
            { "HIT, HOLD, BURST INTO THE NEXT", { "HOLD, THEN THREE INTO THE NEXT", "HOLD, THEN FOUR INTO THE NEXT" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "HELD PADS", "SILENT" }, { "SYNCOPATED KICK, OPEN HAT EIGHTHS", "HALF TIME" }, {} },
            { "PUSH BAR, CELL BAR", { "PUSH BAR, CELL BAR", "CELL BARS ONLY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "KICK ON EVERY HIT, OPEN HATS OFF-BEAT", "STRAIGHT ROCK BEAT" }, {} },
            { "SYNCOPATED CHORDS WITH A PUSH BAR", { "SYNCOPATED, THEN A PUSH", "SYNCOPATED EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "TOMS UNDER THE PUSH", "CRASH ON ONE, STRAIGHT" }, {} },
            { "THREE-SIXTEENTH CELLS AFTER A HELD ONE", { "HELD, THEN THREES", "HELD, THEN THREES ON THE ROOT" }, { "BASS CELLS", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "LOW DRONE" }, { "KICK WITH THE CELLS", "STRAIGHT ROCK BEAT" }, {} },
            { "SHRINKING GROUPS", { "SHRINKING GROUPS", "SHRINKING GROUPS, HELD CHORDS ONLY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "HELD PADS" }, { "KICK ON THE GROUPS, CRASH EACH", "BACKBEAT UNDER THE GROUPS" }, {} },
            { "HELD ROOTS CLIMBING, PUSHED LATE", { "ONE ROOT A BAR ON THREE", "ONE ROOT A BAR ON THE AND OF TWO" }, { "WITH THE GUITAR LATE", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "HIGH DOUBLE, THREE OCTAVES UP", "SILENT" }, { "CRASH ON EACH STRIKE, TOMS", "HALF TIME" }, {} },
            { "FALLING CELLS IN SIXES", { "FALLING THREE-NOTE CELLS", "FALLING CELLS, POWER CHORDS" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
              { "SILENT", "LOW DRONE" }, { "PEDAL HAT, KICK ON THE CELLS", "CHINA ON THE CELLS" }, {} },
            { "FOUR EIGHTHS, THEN A PUSH", { "FOUR EIGHTHS, THEN A PUSH", "FOUR EIGHTHS, THEN HOLD" }, { "FIRST-HALF EIGHTHS", "ROOT AND OCTAVE EIGHTHS", "LOCKED WITH THE KICK" },
              { "HIGH PIANO PULSE", "STRINGS IN 3+3+2" }, { "FOUR EIGHTHS, KICK WITH THE PUSH", "HALF TIME" }, {} },
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
        if (isRound3Type (type))
            return round3Approaches (type, layer);
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
            // (0.122.0) The rock songs' breakdowns (Composer.cpp's RockSection writes them).
            { "ONE BAR OF CHUGS, THREE OF AIR", { "CHUGS, HITS, AIR", "AND A ROCKING CELL THE SECOND TIME" }, { "SILENT", "LOW DRONE" }, { "RIDE KEEPS TIME, THEN A BUILD", "RIDE KEEPS TIME" }, {} },
            { "CHUGS UNDER SPARSE HITS", { "EIGHTHS & NEIGHBOURS, HIGH FIGURE", "EIGHTHS & NEIGHBOURS" }, { "SILENT", "LOW DRONE" }, { "HITS IN TURN, THEN A BUILD", "HITS IN TURN" }, {} },
            { "DOUBLE-HIT UNISON RIFF", { "DOUBLE HITS & RUNS", "DOUBLE HITS ONLY" }, { "SILENT", "LOW DRONE" }, { "KICK WITH THE HITS, OPEN HATS", "KICK WITH THE HITS, HALF TIME" }, { "TRILL IN THE SECOND HALF", "SILENT" } },
            { "ARPEGGIO, THEN THE RIFF UNDER IT", { "OUT, THEN THE RIFF HALFWAY", "THE RIFF FROM A QUARTER IN" }, { "ARPEGGIO IN EIGHTHS", "ARPEGGIO IN SIXTEENTHS" }, { "OUT, THEN WITH THE RIFF", "OUT, THEN HALF TIME" }, { "ARPEGGIO AN OCTAVE UP", "SILENT" } },
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
    namespace
    {
        struct Round3Lists { const char* name; juce::StringArray guitar, bass, keys, drums, lead, acoustic; };   // (0.194.0: acoustic)
        const std::vector<Round3Lists>& round3Lists()
        {
            static const std::vector<Round3Lists> lists {
                // (0.141.0) Pre-choruses.
                { "KICK OUT, LOW TREMOLO", { "LOW TREMOLO, ROCKING HALF STEP", "LOW TREMOLO ON THE ROOT" }, { "TWO HELD ROOTS A BAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "NO KICK, HATS AND SNARE", "NO KICK, RIDE AND SNARE" }, {} },
                { "UNISON ROCKING RIFF", { "ROCKING RIFF", "ROCKING RIFF IN OCTAVES" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "ROCK BEAT, OPEN HATS", "STRAIGHT ROCK, CLOSED HATS" }, {} },
                { "RING & STAB UNDER CYMBAL EIGHTHS", { "RING AND STAB", "RING ONLY" }, { "ROOTS HELD", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "CYMBAL EIGHTHS, SPARSE KICK", "RIDE EIGHTHS, SPARSE KICK" }, { "HIGH BENT NOTE", "SILENT" } },
                { "UNISON EIGHTHS BUILD", { "FULL CHORDS IN EIGHTHS", "CHORDS IN EIGHTHS, HIGH DYADS AT THE END" }, { "ROOT EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK, SNARE AND FLOOR TOM ON EVERY EIGHTH", "SNARE EIGHTHS, THEN SIXTEENTHS" }, {} },
                { "TREMOLO PEDAL, THEN OCTAVE PUMP", { "TREMOLO, CHORDS, THEN THE PUMP", "OCTAVE PUMP ONLY" }, { "HELD, THEN THE PUMP", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SWELLS, TOMS, THEN THE PUMP", "PUMP ALL THROUGH" }, {} },
                { "PAIRS IN 3+3+2, RING BAR", { "PAIRS IN 3+3+2, THEN A RING BAR", "PAIRS EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON THE PAIRS", "HALF TIME, SNARE ON THREE" }, {} },
                { "OCTAVES ROCKING TO THE TRITONE", { "OCTAVES TO THE TRITONE", "OCTAVES ON THE ROOT" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "PUSHED KICK, OPEN HAT EIGHTHS", "CRASH ON ONE, OPEN HATS" }, {} },
                // (0.141.0) Post-choruses and interludes.
                { "DOWN TO THE LOOP", { "HIGH OCTAVE BURSTS", "OUT" }, { "TRESILLO ROOTS", "OUT", "LOCKED WITH THE KICK" },
                  { "ONE-NOTE ARPEGGIO WITH A RISING TAG", "SEQUENCED SYNTH BASS" }, { "INDUSTRIAL BROKEN BEAT", "CLOSED HATS ONLY" }, {} },
                { "HATS AND ROOTS", { "TWO HIGH NOTES A BAR", "OUT" }, { "CHORUS ROOTS IN HALVES", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "BUSY CLOSED HATS ONLY", "HATS AND A KICK ON ONE" }, {} },
                { "HITS ON FOUR", { "HIT ON FOUR, RING", "HIT ON ONE AND FOUR" }, { "WITH THE HITS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND CRASH ON FOUR, HATS", "KICK AND CRASH ON FOUR, RIDE" }, {} },
                { "UNISON CHROMATIC RUNS", { "CHROMATIC SIXTEENTH RUNS", "RUNS, THEN QUARTER HITS" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK PAIRS, DOTTED SNARE", "KICK WITH THE RUNS" }, {} },
                { "DOTTED NOTE OVER THE RIFF", { "HIGH NOTE EVERY DOTTED EIGHTH", "DOTTED HIGH DYADS" }, { "SIXTEENTH PEDAL RIFF", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIDE BELL, BACKBEAT", "CHINA AND SPLASH" }, {} },
                { "SNARE ONLY ON FOUR AND FOUR-AND", { "HELD TWO BARS, HIGH STABS ANSWER", "HELD TWO BARS" }, { "EIGHTHS WITH A SKIP", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE ON FOUR AND FOUR-AND", "SNARE ON FOUR, CRASH ON ONE" }, {} },
                // (0.142.0) Bridges.
                { "CLEAN FIGURE, THEN DISTORTED", { "CLEAN FIGURE, THEN DISTORTED", "DISTORTED FROM THE START" }, { "ROOT EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "CLEAN FIGURE ON THE KEYS", "SILENT" }, { "KICK ALONE, THEN HATS, THEN A ROLL", "FOUR ON THE FLOOR, HATS" }, {} },
                { "TOM OSTINATO, BAND THIN", { "HIGH HELD NOTE, LOW STAB EVERY FOUR", "OUT" }, { "IN THREE STAGES", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SLOW TWO-NOTE LINE", "SILENT" }, { "TWO-TOM OSTINATO", "TOMS AND KICK ONLY" }, {} },
                { "TOMS DOUBLE THE PEDAL", { "MUTED PEDAL BURSTS", "BURSTS, SLOW LINE FROM HALFWAY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "TOMS ON THE BURSTS, THEN THE KIT", "TOMS ON THE BURSTS ONLY" }, {} },
                { "DOMINANT PEDAL BUILD", { "PEDAL ON FIVE IN THREE STAGES", "PEDAL ON FIVE IN SIXTEENTHS" }, { "FIVE HELD, THEN SIXTEENTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SOFT HIGH TRILL IN THE MIDDLE", "SILENT" }, { "HALF TIME, SIXTEENTHS, THEN CARPET AND ROLL", "BUILDING SNARE" }, {} },
                { "TREMOLO CLUSTER OVER OPEN CHORDS", { "OPEN ADD-NINE CHORDS RUNG", "SUS CHORDS RUNG" }, { "HIGH BASS IN QUARTERS AND EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "BROKEN BEAT, OUT IN THE SECOND HALF", "BROKEN BEAT ALL THROUGH" }, { "HIGH HALF-STEP CLUSTER TREMOLO", "SILENT" } },
                { "PIANO AND CHOIR ALONE", { "OUT", "ONE CHORD TO END" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PIANO QUARTERS, THEN A REST", "CHOIR HOLDS THE CHORD" }, { "OUT, A SNARE ROLL TO END", "OUT" }, {} },
                { "TRESILLO BOTH HALVES", { "TRESILLO BOTH HALVES", "TRESILLO, NEXT CHORD IN THE SECOND HALF" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON THE FIRST HALF, SNARE AND CRASH ON THE SECOND", "HALF TIME, RIDE" }, {} },
                { "BASS ALONE, DOTTED", { "OUT", "CHORD ON THE ONE EVERY FOUR" }, { "DOTTED DOUBLE STOPS", "PAIRS IN 3+3+2", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT, THEN TRIBAL", "TRIBAL ALL THROUGH" }, {} },
                { "RIFF ON THE FOUR, NO BACKBEAT, THEN HOME", { "RIFF ON THE FOUR, THEN HOME", "RIFF ON THE BAR'S CHORD" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON THE RIFF, RIDE, THEN THE BACKBEAT", "BACKBEAT ALL THROUGH" }, {} },
                { "BASS HOLDS THE GRID, THE BAND CARVES IT", { "PAIRS, THEN CARVED", "CARVED ALL THROUGH" }, { "PAIRS ALL THROUGH", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE WITH THE GUITAR, KICK IN THE HOLES", "KICK WITH THE GUITAR" }, {} },
                { "DRUMS DROP OUT, THE BAND CARRIES", { "CHORDS IN EIGHTHS, GROWING", "CHORDS IN EIGHTHS" }, { "ROOT EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "BEAT, THEN OUT AFTER TWO AND A HALF BARS", "BEAT ALL THROUGH, OUT IN THE LAST BAR" }, {} },
                // (0.142.0) Breakdowns.
                { "FOUR ON THE FLOOR, NO SNARE", { "NEARLY OUT: HELD NOTE, THEN A MUTED PULSE", "HELD HIGH NOTES ONLY" }, { "WHOLE NOTE, THEN 3+3+2", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "FOUR ON THE FLOOR, NO SNARE", "FOUR ON THE FLOOR, HATS ONLY" }, {} },
                { "OCTAVE PUMP WALL", { "OCTAVE PUMP", "OCTAVE PUMP IN SIXTEENTHS" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK EIGHTHS, SNARE AND CRASH ON EVERY BEAT", "KICK SIXTEENTHS, CRASH QUARTERS" }, {} },
                { "BASS RIFF ALONE, GHOST GROOVE JOINS", { "OUT, HIGH PINGS ON THREE", "OUT" }, { "3+3+2 RIFF ALONE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT, THEN A GHOST GROOVE", "GHOST GROOVE ALL THROUGH" }, {} },
                { "SEVEN-BEAT RIFF ACROSS THE BAR", { "SEVEN-BEAT RIFF", "FOUR AND THREE" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "STEADY BACKBEAT UNDER IT", "KICK WITH THE RIFF" }, {} },
                { "KEYS ALONE, THEN RINGING HITS", { "OUT, THEN CHORDS RUNG TWO BARS", "CHORDS RUNG FROM THE START" }, { "OUT, THEN ROOTS HELD", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "KEYS ALONE, THEN UNDER THE BAND", "KEYS ALONE, THEN OUT" }, { "A HIT, THEN HALF TIME", "HALF TIME FROM THE START" }, {} },
                // (0.143.0) Intros.
                { "HOOK KEPT, BAND BUILT UNDER IT", { "OUT, THEN THE RIFF AT THE LANDING", "LOW PEDAL, THEN THE RIFF" }, { "WHOLE NOTES, THEN A SIXTEENTH PEDAL", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HOOK ALONE, KEPT ON TOP", "HIGH EIGHTHS HOOK" }, { "IN BEFORE THE BASS, THEN 3+3+2", "HATS FIRST, THEN THE GROOVE" }, {} },
                { "RIFF GROWS FROM THE RIDE", { "STAB, HALF THE RIFF, THEN ALL OF IT", "RIFF FROM HALFWAY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIDE SIXTEENTHS, THEN THE KIT", "RIDE SIXTEENTHS ONLY" }, {} },
                { "TOM GROOVE ALONE, THEN THE RIFF", { "STABS AFTER THE BEAT, THEN THE RIFF", "RIFF FROM THE START" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "TOMS AND KICK IN 3+3+2, THEN THE BEAT", "TOMS ALL THROUGH" }, {} },
                { "TOMS ON THE CELL", { "ROOT AND DYAD ON THE CELL", "DYADS ONLY ON THE CELL" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "TOMS ON THE CELL", "TOMS ON THE CELL, THEN THE BEAT" }, {} },
                { "PADS, THEN THE MACHINE LOOP", { "OUT", "ONE SWELLED CHORD AT THE END" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PADS, THEN A PIANO OSTINATO", "PADS ALL THROUGH" }, { "OUT, THEN THE LOOP", "OUT, THE LOOP IN THE LAST BAR" }, {} },
                { "HEARTBEAT KICK", { "QUIET SWELLS", "OUT" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "TREMOLO NOTE FROM A QUARTER IN", "LOW DRONE" }, { "HEARTBEAT, THEN TOMS", "HEARTBEAT ONLY" }, {} },
                { "DRONE, DRUMS, THEN THE RIFF", { "HELD CLASH, THEN THE RIFF", "RIFF FROM HALFWAY" }, { "OUT, THEN WITH THE RIFF", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT, HATS IN SIXTEENTHS, THEN THE BEAT", "HATS, THEN THE BEAT" }, {} },
                { "KEYS ALONE, THEN THE RIFF", { "OUT, THEN THE RIFF", "OUT, THEN RUNG CHORDS" }, { "OUT, THEN ROOTS HELD", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SIXTEENTH KEYS FIGURE, THEN UNDER THE BAND", "SIXTEENTH KEYS FIGURE, THEN OUT" }, { "OUT, THEN RIDE AND HATS", "OUT, THEN THE BEAT" }, {} },
                // (0.143.0) Outros.
                { "UNISON LINE TO THE END", { "UNISON LINE, ANSWERED BY CHORDS", "LINE ALL THROUGH" }, { "DOUBLES, THEN HELD ROOTS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "BACKBEAT, CRASHES ON THE ANSWERS", "BACKBEAT, TOM FILL TO END" }, {} },
                { "DOTTED-EIGHTH HIT CHAIN", { "HITS EVERY THREE SIXTEENTHS", "CHAIN IN THE LAST BARS ONLY" }, { "WITH THE PUSHES", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND CRASH ON EVERY HIT", "KICK ON THE HITS, TOMS BETWEEN" }, {} },
                { "UNISON CLIMB TO THE END", { "CLIMB IN SIXTEENTHS", "CLIMB IN EIGHTHS" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON EVERY SIXTEENTH BY THE END", "KICK EIGHTHS, CRASHES ON THE BEATS" }, {} },
                { "THREE DOTTED PUSHES, THEN RING", { "RIFF, THEN THREE PUSHES AND RING", "TWO PUSHES, THEN RING" }, { "WITH THE PUSHES", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRINGS HOLD THE LAST CHORD", "SILENT" }, { "CRASH ON EACH PUSH", "RIDE, THEN CRASH ON EACH PUSH" }, {} },
                { "HITS AT BOTH ENDS", { "HITS ON 1, 1-AND, 4, 4-AND", "HITS, THEN A HELD CHORD" }, { "WITH THE HITS AT BOTH ENDS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND SNARE WITH THE HITS", "KICK WITH THE HITS, SNARE ON THREE" }, {} },
                { "CHANT OVER A PEDAL BURST", { "PEDAL BURST", "PEDAL BURST, OCTAVES" }, { "WITH THE BURST", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "CHOIR CLIMBS A NOTE A BAR", "SILENT" }, { "SAMPLE HITS ON 1 AND 3, CRASH FILL", "BACKBEAT UNDER THE BURST" }, {} },
                { "BAND DROPS TO PADS AND PIANO", { "STOP ON A SPLASH, THEN THE TONIC HELD", "OUT" }, { "TONIC HELD, THEN OUT", "OUT", "LOCKED WITH THE KICK" },
                  { "STRINGS PULSE AND HIGH PIANO", "PADS ALONE" }, { "A SPLASH, THEN OUT", "OUT" }, {} },
                { "TOM WALL UNDER THE LAST CHORD", { "RIFF, THEN THE LAST CHORD HELD", "LAST CHORD HELD ALL THROUGH" }, { "RIFF, THEN HELD", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND FLOOR TOM ON EVERY SIXTEENTH", "TOM WALL ALL THROUGH" }, {} },
                // (0.151.0) Round four's intros and outros.
                { "SNARE-ROLL GROOVE, THE BAND JOINS", { "RIFF, THEN OCTAVE SIXTEENTHS", "RIFF FROM A QUARTER IN" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE SIXTEENTHS, THEN THE GROOVE", "SNARE SIXTEENTHS ALL THROUGH" }, {} },
                { "HYMN, THEN DOUBLE KICK UNDER IT", { "OUT, THEN HITS AND PICKUPS", "HITS AND PICKUPS" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HYMN CHORDS", "HELD PADS" }, { "TIMPANI, THEN DOUBLE KICK", "DOUBLE KICK ALL THROUGH" }, {} },
                { "QUARTER STRUMS OVER AN OFF-SIXTEENTH SNARE", { "QUARTER STRUMS, A HIGH TONE ON TWO", "QUARTER STRUMS" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OFF-SIXTEENTH SNARE, PEDAL HAT", "OFF-SIXTEENTH SNARE, RIDE" }, {} },
                { "SIDE STICK AND HATS ON THE RIFF", { "THE RIFF ON THE ROOT", "THE RIFF IN OCTAVES" }, { "OUT", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SIDESTICK AND CLOSED HATS ON THE RIFF, SHAKER", "SIDESTICK AND CLOSED HATS ON THE RIFF" }, {} },
                { "HAND PERCUSSION ALONE, THEN THE RIFF", { "OUT, THEN THE MUTED RIFF", "THE MUTED RIFF" }, { "OUT, THEN WITH THE KIT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HAND PERCUSSION, THEN THE KIT", "HAND PERCUSSION ONLY" }, {} },
                { "ONE CHORD STRUMMED 3+3+3+3+4, RIFF JOINS", { "STRUM, THEN THE RIFF UNDER IT", "STRUM ALL THROUGH" }, { "BASS OUT, THEN THE RIFF", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT, THEN THE GROOVE", "STRUM ACCENTS ON THE KICK" }, {} },
                { "RIFF ALONE, SIXTEENTH ROOT PEDAL JOINS", { "THE RIFF, ALONE AT FIRST", "THE RIFF, AN OCTAVE ADDED LATE" }, { "BASS OUT, THEN SIXTEENTH ROOTS", "ROOT SIXTEENTHS FROM THE START", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HATS, THEN THE KICK", "HATS AND SNARE ON FOUR" }, {} },
                { "SWELLS, THEN A FIXED HIGH FIGURE, THE KIT LAST", { "SWELLS, THEN THE HIGH FIGURE", "HIGH FIGURE ALL THROUGH" }, { "OUT, THEN EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HATS, THEN THE KIT LAST", "HATS FROM A QUARTER IN" }, {} },
                { "BURSTS OFF THE BEAT TO THE END", { "BURSTS OFF THE BEAT, OCTAVE STABS", "BURSTS OFF THE BEAT" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND CRASH ON THE BURSTS", "KICK ON THE BURSTS, BACKBEAT" }, {} },
                { "BASS RIFF ALONE TO THE END", { "OUT", "ONE CHORD HELD OVER IT" }, { "THE BASS RIFF", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT", "A KICK ON EACH ONE" }, {} },
                { "CALL BAR, ANSWER BAR, CUT", { "CALL AND ANSWER, CUT", "CALL BARS, CUT" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "CALL, ANSWER, SNARE EIGHTHS", "CALL AND ANSWER, CRASH" }, {} },
                { "OSTINATO AND BEAT TO THE END", { "OUT", "CHORDS ON ONE" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "TWO-NOTE EIGHTHS TO THE END", "TWO-NOTE EIGHTHS, ALONE AT THE END" }, { "THE GROOVE TO THE END", "THE GROOVE, OUT AT THE END" }, {} },
                // (0.150.0) Round four's bridges and breakdowns.
                { "PARALLEL CLASH OVER THE BASS", { "CLASH SHAPE A HALF STEP OVER THE BASS", "CLASH SHAPE ON THE BEATS" }, { "CHROMATIC EIGHTHS ROUND THE ROOT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF TIME, PUSHED KICK, CRASH EIGHTHS", "HALF TIME, PUSHED KICK, RIDE" }, {} },
                { "ORCHESTRA ALONE, THEN THE WALL", { "OUT, THEN ROOT SIXTEENTHS", "ROOT SIXTEENTHS ALL THROUGH" }, { "BASS OUT, THEN SIXTEENTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRINGS AND CHOIR, THEN THE PEDAL", "HELD PADS" }, { "OUT, THEN KICK PAIRS IN THREES", "KICK PAIRS IN THREES" }, {} },
                { "SNARE CARPET OVER THE FLOOR", { "OUT", "CHORDS FROM HALFWAY" }, { "BASS OUT, THEN ROOTS HALFWAY", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "ONE-NOTE SIXTEENTH LOOP, PIANO HALFWAY", "ONE-NOTE SIXTEENTH LOOP" }, { "SNARE CARPET, KICK EIGHTHS AT THE END", "SNARE CARPET, FOUR ON THE FLOOR" }, {} },
                { "LONG PULSE BUILD", { "HIGH DYAD EIGHTHS, MOVING LOWER VOICE", "HIGH DYAD EIGHTHS, FIXED" }, { "OUT, WHOLE NOTES, THEN EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "OUT, THEN HELD", "SILENT" }, { "KICK QUARTERS, SNARE BY STAGES", "KICK QUARTERS ONLY" }, {} },
                { "PAIRS EVERY THREE UNDER A HELD CHORD", { "LOW PAIRS UNDER A HIGH CHORD", "LOW PAIRS ONLY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "KICK ON THE PAIRS, SNARE ON 3", "KICK ON THE PAIRS, BACKBEAT" }, {} },
                { "THE HOOK'S RHYTHM ON LOW CHORDS", { "HOOK RHYTHM ON I AND FLAT II", "HOOK RHYTHM ON THE CHORDS" }, { "EIGHTHS UNDER THE HOOK", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK 1 AND 3-AND, GHOSTED BACKBEAT", "CRASH QUARTERS, BACKBEAT" }, {} },
                { "DOUBLED HITS ON ONE AND TWO, THEN HOLD", { "DOUBLED HITS, THEN HOLD", "DOUBLED HITS, HIGH OCTAVE" }, { "QUARTERS ON ONE AND TWO", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HITS WITH THE GUITAR, A SNARELESS PAIR", "HITS WITH THE GUITAR" }, {} },
                { "BURSTS OF FIVE, LAYERED", { "BURSTS, FOURS, THEN THE STRUM", "BURSTS OF FIVE ONLY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON THE BURSTS, BACKBEAT LATE", "KICK ON THE BURSTS, SNARE ON 3" }, {} },
                { "BELL GROOVE UNDER A TWO-CHORD DRONE", { "ROOT-FIVE-NINE RUNG ON ONE", "ROOT-FIVE-NINE IN EIGHTHS" }, { "WHOLE NOTES ON THE DRONE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIDE BELL GROOVE", "RIDE BELL, KICK ON ONE" }, {} },
                { "HIGH CHROMATIC FALL OVER THE CHUG CELL", { "BASS FIRST, THEN THE HIGH FALL", "HIGH FALL ALL THROUGH" }, { "THE CHUG CELL", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PAD ON THE CELL", "SILENT" }, { "KICK ON THE CELL, SNARE MID-BAR", "KICK ON THE CELL, BACKBEAT" }, {} },
                { "TRIBAL TOMS, DOTTED OCTAVES", { "ROOT OCTAVES, TWO FIGURES IN TURN", "DOTTED OCTAVES" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HIGH TONES ON THE DOTTED QUARTERS", "SILENT" }, { "KICK QUARTERS, TRIBAL TOMS", "TRIBAL TOMS ONLY" }, {} },
                { "UNISON STABS OVER HATS ONLY", { "UNISON STABS, TWO FIGURES", "UNISON STABS ON THE FIRST FIGURE" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STABS WITH THE GUITAR", "SILENT" }, { "HATS ONLY, THREE HITS TO END", "PEDAL AND CLOSED HATS ONLY" }, {} },
                { "EIGHTHS WITH AIR AT BOTH ENDS", { "EIGHTHS, THEN EIGHTHS WITH AIR", "EIGHTHS WITH AIR EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK 1, 3, 3-AND, CRASH EVERY TWO", "KICK 1, 3, 3-AND, HATS" }, {} },
                { "ARPEGGIO UP AN OCTAVE, BASS STEPPING DOWN", { "ARPEGGIO UP AN OCTAVE", "ARPEGGIO IN SIXTEENTHS UP AN OCTAVE" }, { "STEPPING DOWN A BAR AT A TIME", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRINGS HELD", "SILENT" }, { "OUT, A SNARE PICKUP TO END", "OUT" }, {} },
                { "HALF-BAR CHUGS, HALF STEP EVERY FOURTH", { "HALF-BAR CHUGS", "HALF-BAR CHUGS, A HIGH FIGURE ON TOP" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "DOUBLE KICK WITH THE CHUGS, CRASHES", "KICK WITH THE CHUGS, SNARE ON 3" }, {} },
                { "PAIR STABS, THEN THE THREES", { "PAIR STABS, THEN TREMOLO CHORDS", "PAIR STABS ONLY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "OCTAVE TREMOLO ON TOP", "SILENT" }, { "PAIRS, THEN SNARE AND KICK IN THREES", "PAIRS WITH THE GUITAR" }, {} },
                { "QUIET FALLING CELL OVER A LAMENT BASS", { "FALLING CELL IN SIXTEENTHS", "FALLING CELL IN EIGHTHS" }, { "LAMENT, A NOTE A BAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "THE CELL IN EIGHTHS", "SILENT" }, { "OUT", "SOFT KICK ON ONE" }, {} },
                { "HIGH NOTE TREMOLO OVER A HALF-TIME CHUG", { "HALF-TIME CHUG, HIGH TREMOLO", "HALF-TIME CHUG ONLY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF TIME WITH THE CHUG, TOM PICKUPS", "HALF TIME WITH THE CHUG" }, {} },
                // (0.185.0) Round six's intros and outros.
                { "THE SAME CELL IN CANON", { "THE CELL IN CANON, THREE VOICES", "THE CELL IN CANON, TWO VOICES" }, { "OUT TWO BARS, THEN ROOT STABS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HAT SIXTEENTHS, KICK EVERY OTHER BAR", "HAT SIXTEENTHS, BACKBEAT FROM HALFWAY" }, {} },
                { "TOM BURSTS UNDER THE HALF-STEP RIFF", { "ROCKING HALF-STEP CELL", "ROCKING CELL, POWER CHORDS FROM HALFWAY" }, { "OUT, THEN WITH THE RIFF", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "TOM BURSTS, NO KICK OR SNARE", "TOM BURSTS, THEN THE FULL KIT" }, {} },
                { "OSTINATO WITH GUITAR-AND-KICK HITS", { "HITS WITH THE KICK, THEN ONE CHORD HELD", "SCATTERED HITS WITH THE KICK" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "THREE-NOTE CHROMATIC OSTINATO", "CHROMATIC OSTINATO IN OCTAVES" }, { "KICK AND SPLASH ON THE HITS, THEN HALF TIME", "KICK AND SPLASH ON THE HITS" }, {} },
                { "CLAP CLAVE OVER SIXTEENTH HATS", { "OUT", "CHORDS FROM HALFWAY" }, { "OUT", "BASS PICKUP INTO THE NEXT", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "CLAP CLAVE, SIXTEENTH HATS", "CLAP CLAVE, THE KIT FROM HALFWAY" }, {} },
                { "HORN LINE ALONE, CRASH ON ONE", { "OUT", "OUT, THE BAND IN AT THE END" }, { "OUT", "IN ON THREE OF THE LAST BAR", "LOCKED WITH THE KICK" },
                  { "HORN LINE IN OCTAVES", "HORN LINE, ONE OCTAVE" }, { "CRASH ON THE FIRST ONE ONLY", "CRASH ON ONE, THE KIT IN AT THE END" }, {} },
                { "TWO ROOTS AND A RUNG CHORD", { "TWO ROOTS AND A FIXED ADD9", "TWO ROOTS AND A CHORD THAT FOLLOWS" }, { "BASS OUT, THEN ROOTS HALFWAY", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT", "CLOSED HATS ONLY" }, {} },
                // (0.184.0) Round six's bridges and breakdowns.
                { "BASS ARPEGGIO IN THREES, THE KIT CREEPS IN", { "OUT, THEN A SIXTEENTH LINE", "SIXTEENTH LINE ALL THROUGH" }, { "ARPEGGIO IN THREES", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "CRASH, THEN RIDE CREEPS IN", "RIDE EIGHTHS, KICK IN THREES" }, {} },
                { "ONE HIT, PICKED SINGLE LINE, HATS ONLY", { "PICKED PEDAL LINE, THE SECOND JOINS", "PICKED PEDAL LINE ALONE" }, { "ONE HIT, THEN EIGHTHS HALFWAY", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HATS AND RIDE ONLY, THEN TOMS CLIMB", "HATS AND RIDE EIGHTHS ONLY" }, {} },
                { "STICKS IN CLUSTERS, KICK QUARTERS, NO SNARE", { "SINGLE-NOTE RIFF, STAB BAR EVERY FOURTH", "THE SINGLE-NOTE LINE ALONE" }, { "ROOT QUARTERS UNDER THE RIFF", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "STICK CLUSTERS, KICK QUARTERS", "KICK QUARTERS, HATS, NO SNARE" }, {} },
                { "KICK AND SIDESTICK ON ONE AND THREE, GUITARS OUT", { "OUT", "ONE CHORD A BAR, SOFT" }, { "ROOT HALF NOTES", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "THE LINE IN OCTAVES", "HELD PADS" }, { "KICK AND SIDESTICK ON ONE AND THREE", "KICK AND SIDESTICK, HATS EIGHTHS" }, {} },
                { "BASS A STEP UNDER, CLUSTER TREMOLO JOINS", { "OUT, THEN A CLUSTER TREMOLO", "CLUSTER TREMOLO ALL THROUGH" }, { "A STEP UNDER THE KEY", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "BROKEN KICK, SNARE ON TWO AND FOUR, STOP", "BACKBEAT, CLOSED HATS" }, {} },
                { "HELD CHORDS OVER A SIXTEENTH BASS PEDAL, NO SNARE", { "CHORD RUNG, ROOT STABS UNDER IT", "CHORD RUNG EACH BAR" }, { "SIXTEENTH PEDAL, FOURTH BAR MOVES", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON ONE, RIDE AND CRASH PUSHES, NO SNARE", "KICK ON ONE, RIDE QUARTERS, NO SNARE" }, {} },
                { "OCTAVES IN 2+1 CELLS OVER SIXTEENTH ROOTS", { "TONIC OCTAVES IN 2+1 CELLS", "OCTAVES IN 2+1 CELLS ON EACH ROOT" }, { "ROOT SIXTEENTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK FIGURE, CRASH EIGHTHS, SNARE TWO AND FOUR", "BACKBEAT, CLOSED HATS" }, {} },
                { "RIDE AND BASS SIXTEENTHS UNDER A CLEAN ARPEGGIO", { "CLEAN ARPEGGIO, A HIGH NOTE HELD", "CLEAN SIXTEENTH ARPEGGIO" }, { "ROOT SIXTEENTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIDE SIXTEENTHS, THEN THE GROOVE", "RIDE SIXTEENTHS, CRASH ON ONE" }, {} },
                { "EIGHTH CHORDS WALKING THE HALF STEPS", { "EIGHTH POWER CHORDS WALKING HALF STEPS", "THE WALK AS MUTED SINGLES" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND CRASH WITH THE WALK", "BACKBEAT, CLOSED HATS" }, {} },
                { "FIVE-SIXTEENTH BURSTS UNDER A PICKED ARPEGGIO", { "BURSTS OF FIVE, PICKED ARPEGGIO OVER", "FIVE-SIXTEENTH BURSTS ALONE" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK BURSTS, SNARE ON EVERY BEAT, CRASH EIGHTHS", "KICK BURSTS, BACKBEAT" }, {} },
                { "HALF-STEP FALL OVER THE OPEN ROOT", { "HALF-STEP FALL, THE SECOND GUITAR DOUBLES", "HALF-STEP FALL, ONE GUITAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF TIME, KICK ON THE OPEN ROOTS", "BACKBEAT, CLOSED HATS" }, {} },
                // (0.197.0) Round seven's bridges and breakdowns: the acoustic-led ones list the ACOUSTIC lane's options last.
                { "DOWN TO THE ACOUSTIC", { "OUT", "ONE CHORD RUNG IN THE LAST BAR" }, { "OUT, THEN THE ROOT ON ONE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRINGS SWELL OVER THE LAST TWO BARS", "SILENT" }, { "HATS AND SHAKER JOIN, KICK EVERY FOURTH BAR", "SHAKER ALONE FROM HALFWAY" }, {},
                  { "FINGERPICKING, THE LAST BAR RUNG", "SOFT FLICK STRUM, THE LAST BAR RUNG" } },
                { "HIGH ARPEGGIO OVER A HELD STRUM", { "THE HIGH PICKING ON THE ELECTRIC", "OUT" }, { "OUT, ROOTS IN THE SECOND HALF", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "CRASH ON EACH TONIC STRUM ONLY", "SHAKER EIGHTHS IN THE SECOND HALF" }, {},
                  { "TONIC STRUM, HIGH PICKING, THEN THE TIED STRUM", "TONIC STRUM, HIGH PICKING ALL THROUGH" } },
                { "PICKED UNISON ARPEGGIO, DRUMS OUT", { "OUT", "SOFT CHORD ON EACH ONE" }, { "OUT", "THE BASS NOTE WITH IT", "LOCKED WITH THE KICK" },
                  { "STRINGS HOLD THE CHORD", "SILENT" }, { "SOFT SWELL INTO THE NEXT", "RIDE BELL ON ONE" }, {},
                  { "THE ARPEGGIO, TWO IN UNISON", "THE ARPEGGIO ALONE" } },
                { "KIT-LED BRIDGE, TWO-NOTE STABS", { "TWO-NOTE STABS EVERY OTHER BAR", "TWO-NOTE STABS EVERY BAR" }, { "ROOT ON THE STABS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "FOUR ON THE FLOOR, TOMS, THEN A BACKBEAT AND A ROLL", "FOUR ON THE FLOOR, OPEN HATS, NO SNARE" }, {} },
                { "FIXED HIGH NOTE OVER A WALKING LOW NOTE", { "HIGH TONIC EIGHTHS OVER THE LOW ROOT", "HIGH TONIC EIGHTHS, A SECOND HOLDS THE ROOT" }, { "THE LOW NOTE WITH IT", "OUT", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT, RIDE ON ONE, THEN SNARE AND TOM EIGHTHS", "RIDE ON ONE, THEN SNARE AND TOM EIGHTHS" }, {} },
                { "DISPLACED HALF-TIME BACKBEAT, CHROMATIC CLIMB", { "HELD CHORDS, THEN THE CLIMB IN UNISON", "THE FIGURE ALL THROUGH, CLIMBING AT THE END" }, { "THE FIGURE, CLIMBING WITH THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE ON TWO-AND AND FOUR-AND, A TOM FLURRY EVERY FOUR", "THE DISPLACED BACKBEAT, NO FLURRIES" }, {} },
                { "TOM CARPET, SNARE ON THE OFF-BEATS", { "HIGH TREMOLO TRIADS", "HIGH TREMOLO TRIADS, A SECOND ON THE ROOT" }, { "ROOT SIXTEENTHS UNDER THE TOMS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "FLOOR TOM CARPET, SNARE AND CRASH OFF THE BEAT", "TOM CARPET, KICK UNDER THE TOMS" }, {} },
                { "REPEATED-NOTE LINE IN 3+3+2", { "POWER EIGHTHS, A SECOND PLAYS THE LINE", "POWER EIGHTHS UNDER THE LINE" }, { "ROOT EIGHTHS UNDER THE LINE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "THE REPEATED LINE ON THE KEYS" }, { "KICK EIGHTHS UNDER THE LINE, SNARE ON THREE", "RIDE EIGHTHS, SNARE ON TWO AND FOUR" },
                  { "REPEATED NOTES IN 3+3+2, THEN THE ANSWER", "REPEATED NOTES, AN OCTAVE UP" } },
                { "CHORDS UNDER A HELD HIGH THIRD", { "HIGH CHORDS ON 1-3-4A, THEN 2-3-4", "HIGH CHORDS, A SECOND HOLDS THE THIRD" }, { "DOUBLES THE HIGH CHORDS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "THE HIGH THIRD HELD ON THE KEYS", "HELD PADS" }, { "HALF TIME ON THE RIDE, KICK ON ONE OR TWO AND FOUR-A", "RIDE QUARTERS, KICK WITH THE CHORDS" },
                  { "THE HIGH THIRD HELD A BAR", "THE HIGH THIRD, AN OCTAVE UP" } },
                { "ONE ACOUSTIC ALONE", { "OUT", "LOW CHORD EVERY FOURTH BAR" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND TOM, SNARE AND CLAP ON THREE", "CLAP ON THREE ONLY" }, {},
                  { "THE TIED STRUM ALONE", "THE TIED STRUM, A SECOND AS THE BASS" } },
                { "HELD HIT, UNISON SIXTEENTH PICKUP", { "HELD HIT, FOUR SIXTEENTHS, THEN THE STREAM", "HELD HIT AND PICKUP EVERY BAR" }, { "WITH THE HITS AND THE STREAM", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE SIXTEENTHS UNDER THE HOLD, KICK ON THE PICKUP", "CRASH ON THE HIT, KICK WITH THE STREAM" }, {} },
                { "THE BAND ANSWERS ON THE LAST EIGHTH", { "THE RIFF ALONE, A SECOND ON THE ANSWERS", "THE RIFF, THE ANSWERS IN IT TOO" }, { "ONLY THE ANSWERS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND CRASH ON THE ANSWERS ONLY", "HATS UNDER THE RIFF, KICK AND CRASH ON THE ANSWERS" }, {} },
                { "TOMS, SIDESTICK AND CLAPS", { "OUT, THEN MUTED CHUGS HALFWAY", "MUTED CHUGS ALL THROUGH" }, { "OUT, THEN ROOT EIGHTHS HALFWAY", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "TOMS, SIDESTICK AND CLAP GROOVE", "TOMS AND CLAPS, KICK ON THE BEATS" }, {} },
                { "WIDE CHORD RUNG OVER THE CHUG", { "THE CHUG, A SECOND RINGS A WIDE CHORD EVERY FOURTH BAR", "THE CHUG, THE WIDE CHORD ON ITS OWN BARS" }, { "DOUBLES THE CHUG", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK WITH THE CHUG, SNARE ON THREE", "KICK WITH THE CHUG, BACKBEAT" }, {} },
                // (0.198.0) Round seven's intros and outros: the acoustic-led ones list the ACOUSTIC lane's options last.
                { "ACOUSTIC ALONE, SHAKER AND HATS JOIN", { "OUT", "A SOFT CHORD EACH FOUR BARS ONCE THE KIT JOINS" }, { "THE ROOT ON ONE ONCE THE KIT JOINS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRINGS SWELL INTO THE NEXT BLOCK", "SILENT" }, { "HATS AND A SHAKER HALFWAY, A KICK EACH FOUR BARS", "A SHAKER HALFWAY, A SOFT SNARE AT THE END" }, {},
                  { "THE VERSE'S PICKING ALONE", "THE VERSE'S SOFT STRUM ALONE" } },
                { "CHORUS CHORDS STRUMMED, THEN A TONIC LINK", { "OUT", "THE TONIC LINK ON THE ELECTRIC" }, { "OUT", "ROOT WHOLE NOTES UNTIL THE LINK", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SHAKER EIGHTHS, LOUDER ON THE ANDS, OUT FOR THE LINK", "KICK EIGHTHS, SNARE ON THREE, CRASH QUARTERS, OUT FOR THE LINK" }, {},
                  { "FIFTH STACKS, THEN THE TONIC DYAD LINK", "FIFTH STACKS AND A SECOND'S SEESAW, THEN THE LINK" } },
                { "STRUMMED TRIADS IN SIXES, ALONE", { "OUT", "THE TRIAD CELLS HIGH ON THE ELECTRIC" }, { "OUT", "THE ROOT ON EACH CELL", "LOCKED WITH THE KICK" },
                  { "SILENT", "THE TRIAD CELLS ON THE KEYS" }, { "A CYMBAL SWELL ON THE LAST BAR ONLY", "THE BELL ON EACH CELL" }, {},
                  { "TRIADS STRUMMED IN CELLS OF SIX", "TRIAD CELLS, A SECOND'S HIGH THIRDS" } },
                { "NYLON DOUBLES THE INTRO, THEN LEAVES", { "THE ARPEGGIO ON THE ELECTRIC, ALL THROUGH", "THE ARPEGGIO ON THE ELECTRIC FROM HALFWAY" }, { "ROOTS WITH THE LOW CHANGES", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "THE BELL ON THE BEATS ONCE THE NYLON LEAVES", "A CRASH SWELL INTO THE BAND" },
                  { "A SWELLED NOTE EVERY OTHER BAR", "SILENT" },
                  { "NYLON DOUBLES THE FIRST HALF", "NYLON DOUBLES IT ALL THROUGH" } },
                { "EMBELLISHED STRUM ALONE", { "OUT", "THE CLIMBING LINE ON THE ELECTRIC" }, { "OUT", "ROOT WHOLE NOTES FROM HALFWAY", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "A SIDE STICK ON THE BEATS FROM HALFWAY", "A SIDE STICK ON TWO AND FOUR FROM HALFWAY" }, {},
                  { "THE EMBELLISHED FIGURE ALONE", "THE FIGURE, A SECOND CLIMBING THE CHORD" } },
                { "HIGH ONE-NOTE PULSE OVER A TRITONE BASS", { "THE KEY'S NOTE HIGH IN EIGHTHS", "THE HIGH PULSE, A SECOND HOLDS THE ROOT" }, { "FLAT FIVE HELD, THE ROOT ON FOUR-AND", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIDE EIGHTHS, KICK ON ONE AND FOUR-AND, NO SNARE", "RIDE EIGHTHS, A KICK ON ONE, TOMS AT THE END" }, {} },
                { "HELD TRIADS STEPPING DOWN, TAMBOURINE", { "HIGH TRIADS STEPPING DOWN THE KEY", "HIGH TRIADS ON THE CHORDS" }, { "OUT", "THE TRIAD'S ROOT HELD", "LOCKED WITH THE KICK" },
                  { "A CHOIR HOLDS THE TRIADS", "SILENT" }, { "KICK ON ONE AND THREE, RIDE, TAMBOURINE OFF THE BEAT", "TAMBOURINE OFF THE BEAT, A KICK ON ONE" }, {} },
                { "ROLLED LAST CHORD, FALLING TOP", { "OUT", "A FIFTH STACK HELD UNDER THE LAST CHORD" }, { "ROOTS HELD", "OUT", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SHAKER EIGHTHS, NOTHING UNDER THE LAST CHORD", "A CRASH WITH THE LAST CHORD ONLY" }, {},
                  { "THE FIFTH STACKS, THEN THE ROLLED CHORD", "THE ARCH PICKING, THEN THE ROLLED CHORD" } },
                { "STRUM OFF THE ONE TO THE END", { "OUT", "POWER CHORDS COMING IN ON THREE" }, { "THE DOWNBEAT ALONE, CHANGING ON THREE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "A CRASH ON ONE, A TOM FILL EVERY FOURTH BAR", "A CRASH ON EACH ONE, NO FILLS" }, {},
                  { "OFF THE ONE, THE LAST CHORD HELD", "OFF THE ONE, A SECOND AS THE BASS, THE LAST HELD" } },
                { "INTRO PICKING RETURNS, ONE CHORD HELD", { "OUT", "THE LAST CHORD HELD ON THE ELECTRIC" }, { "OUT", "ROOTS ON ONE, THE LAST CHORD HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "A SIDE STICK ON TWO AND FOUR AND A SHAKER", "THE BELL ON EACH ONE, A CRASH ON THE LAST CHORD" }, {},
                  { "THE ADD9 ARPEGGIO, THE LAST CHORD HELD", "THE NYLON PICKING, THE LAST CHORD HELD" } },
                { "DOTTED COUNTER-LINES OVER THE ARPEGGIO", { "TWO COUNTER-LINES, THE SECOND IN HALFWAY", "ONE DOTTED COUNTER-LINE" }, { "ROOTS HELD", "OUT", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SOFT BACKBEAT, RIDE EIGHTHS, A CRASH TO END", "A SHAKER ON THE EIGHTHS, A KICK ON EACH ONE" }, {},
                  { "THE ARPEGGIO, A SECOND'S TWIN ARPEGGIO", "THE ARPEGGIO, ONE ACOUSTIC" } },
                { "CHORD TO PEDAL, LAST CHORD HELD", { "OUT", "A CLEAN CHORD RUNG EVERY TWO BARS" }, { "ROOT, PASSING NOTE, PEDAL, WALK-UPS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "TAMBOURINE ON TWO AND FOUR, A CRASH AT EACH END", "TAMBOURINE EIGHTHS, A CRASH ON THE LAST CHORD" }, {},
                  { "THE PEDAL STRUM, THE LAST CHORD HELD", "THE PEDAL STRUM, A SECOND'S FILLS, THE LAST HELD" } },
                // (0.183.0) Round six's pre-choruses.
                { "PIANO EIGHTHS, CLAPS ON THE BEATS, GUITARS RING", { "CHORD RUNG, SIXTEENTH PICKUP PAIR", "MUTED EIGHTHS UNDER THE CLAPS" }, { "ROOTS HELD", "ROOT HELD, PICKUP PAIR", "LOCKED WITH THE KICK" },
                  { "PIANO CHORD EIGHTHS", "HELD PADS" }, { "CLAPS ON EVERY BEAT, CRASH ON ONE", "CLAPS ON THE BEATS, SPARSE KICK" }, {} },
                { "QUARTER CHORDS OVER KICK EIGHTHS", { "FULL CHORD QUARTERS, LAST PUSHED", "POWER CHORD QUARTERS, LAST PUSHED" }, { "EIGHTHS WALKING THE CHORD", "ROOT EIGHTHS", "LOCKED WITH THE KICK" },
                  { "PIANO CHORD EIGHTHS", "HELD PADS" }, { "KICK EIGHTHS, SOFT QUARTER CLAPS", "KICK EIGHTHS, BACKBEAT" }, {} },
                { "KICK EIGHTHS, THEN SNARE EIGHTHS", { "CHORD HELD A BAR", "HELD CHORD, EIGHTHS IN THE BACK HALF" }, { "ROOT EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK EIGHTHS FRONT, SNARE EIGHTHS BACK", "HALVES, SNARE ROLL TO END" }, {} },
                { "TRITONE CHORDS BETWEEN ROOT SINGLES, SNARE CLIMB", { "TRITONE AND HALF-STEP CHORDS BETWEEN ROOTS", "TRITONE CHORDS ALONE" }, { "WITH THE TRITONE CHORDS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE CLIMB, CRASH ON ONE AND TWO-AND", "BACKBEAT, CLOSED HATS" }, {} },
                { "DOTTED KICK, SNARE PAIRS, MUTED ROOT STABS", { "MUTED ROOT STABS", "STABS AS POWER CHORDS" }, { "ROOTS HELD", "ON THE MUTED STABS", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "DOTTED KICK, SNARE PAIRS", "BACKBEAT, CLOSED HATS" }, {} },
                { "SIXTEENTHS UP A HALF STEP AFTER SIX", { "SIX ON THE ROOT, TEN A HALF STEP UP", "SIX AND TEN, MUTED SINGLES" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIDE SIXTEENTHS, SNARE EIGHTHS, STOP", "BACKBEAT, CLOSED HATS" }, {} },
                // (0.149.0) Round four's pre-choruses and post-choruses.
                { "STOP BARS IN TURN WITH THE RIFF", { "STOP BARS AND THE RIFF", "STOP BARS ONLY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "STOPS, RIFF, BUILD", "STOPS AND A BACKBEAT" }, {} },
                { "MARCHING SNARE OVER HELD CHORDS", { "OUT", "HELD CHORDS" }, { "WHOLE NOTES", "OUT", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "MARCHING SNARE", "MARCHING SNARE WITH A KICK ON ONE" }, {} },
                { "DRUMS OUT, KEYS PULSE AND PUSHES", { "PUSHES IN THE SECOND HALF", "OUT" }, { "ROOTS HELD", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
                  { "CHORDS IN HALVES", "HELD PADS" }, { "OUT, THEN BACK HALFWAY", "OUT, A HIT TO END" }, {} },
                { "SIXTEENTH ROOTS STEPPING DOWN", { "STREAM, BURSTS, CLASHES", "STREAM ALL THROUGH" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON THE BEATS, CRASH EACH STAGE", "HALF TIME" }, {} },
                { "SIDESTICK DROP", { "ROOT OCTAVES ON THE ANCHORS", "ANCHORS, THEN A STOP" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "TWO HIGH NOTES, A STEP APART", "HELD PADS" }, { "KICK AND SIDE STICK", "SIDE STICK ONLY" }, {} },
                { "HIGH PEDAL IN 3+3+2 OVER A MOVING BASS", { "HIGH PEDAL IN 3+3+2", "HIGH PEDAL, TOP STEPPING EACH BAR" }, { "SIXTEENTHS ON EACH ROOT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD ROOT", "HELD PADS" }, { "VERSE GROOVE, SNARE TRIPLE TO END", "HIP-HOP BREAK" }, {} },
                { "OFF-BEAT LOW DYADS UNDER A HIGH DRONE", { "OFF-BEAT DYADS, HIGH ROOT RUNG", "OFF-BEAT DYADS ONLY" }, { "BUSY ROOT AND FLAT SEVEN", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "PEDAL HAT QUARTERS, SYNCOPATED KICK", "KICK WITH THE DYADS" }, {} },
                { "CLAPS AND ONE GUITAR", { "THE HOOK FIGURE ALONE", "HOOK FIGURE AND CHORDS" }, { "OUT, THEN HELD ROOTS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "OUT, THEN TRIADS HALFWAY", "HELD PADS" }, { "SNARE AND CLAPS ON EVERY BEAT", "CLAPS, TAMBOURINE AND A KICK" }, {} },
                { "BASS RIFF ALONE UNDER A HELD HIGH NOTE", { "HELD HIGH NOTE, THEN THE RIFF", "HELD NOTE, THEN TREMOLO IN TURN" }, { "THE RIFF ALONE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT, THEN THE RIDE", "RIDE ONLY" }, {} },
                // (0.148.0) Round four's choruses.
                { "ACOUSTIC STRUM CHORUS", { "SIXTEENTH STRUMS", "STRUMS WITH POWER CHORDS ON THE ACCENTS" }, { "WHOLE NOTES", "DOTTED WITH THE KICK", "LOCKED WITH THE KICK" },
                  { "STRINGS ON THE ROOT", "HELD PADS" }, { "NO DRUMS", "TOMS ALONE IN 3+3+3+3+4", "TOMS OVER FOUR ON THE FLOOR", "DOTTED KICK, BACKBEAT" }, {} },
                { "BURST & HOLD, THEN A CARPET IN 3+2+3", { "BURST, CARPET, RIFF", "BURST AND HOLD ALL THROUGH" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "BURST, CARPET, HALF TIME", "BURST AND HOLD DRUMS" }, {} },
                { "BREAKBEAT UNDER HELD CHORDS", { "CLEAN 3+3+2 FIGURE", "OUT" }, { "WHOLE NOTES", "EIGHTHS", "LOCKED WITH THE KICK" },
                  { "FIXED FOUR-NOTE OSTINATO", "HELD PADS" }, { "DOUBLE-TIME BREAKBEAT", "BREAKBEAT, KICK ON ONE" }, {} },
                { "STOMP IN 3+3+2", { "3+3+2 STOMP", "OCTAVE WALL OVER THE 3+3+2" }, { "ON THE 3+3+2", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "THE GRID AN OCTAVE UP", "HELD PADS" }, { "KICK AND FLOOR TOM STOMP", "STOMP WITH CRASH EIGHTHS" }, {} },
                { "FALLING PUSHED CHORDS", { "FALLING PUSHES", "FALLING PUSHES IN OCTAVES" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "DOUBLE-KICK GROOVE", "BACKBEAT, CLOSED HATS" }, {} },
                { "HITS, THEN A WALL THE BASS LEAVES", { "TWO HITS, THEN A SIXTEENTH WALL", "WALL ALL THROUGH" }, { "ON THE HITS ONLY", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE ON TWO, ITS A AND FOUR, CRASH EIGHTHS", "PUSHED KICK, CRASH EIGHTHS" }, {} },
                { "RING THE ONE, RUN THE REST", { "RING AND RUN", "RING AND PUSH" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "CHINA QUARTERS, TOMS IN THE BACK HALF", "CHINA QUARTERS, PLAIN" }, {} },
                { "BASS SIXTEENTHS, GUITARS OUT", { "OUT", "OFF-BEAT STABS", "SIXTEENTH WALL" }, { "ROOT SIXTEENTHS", "EIGHTHS", "LOCKED WITH THE KICK" },
                  { "FIXED-TOP OSTINATO", "PAD DOUBLING THE BASS" }, { "HIP-HOP BREAK", "HIP-HOP BREAK, CRASH EVERY TWO BARS" }, {} },
                { "THREE CHORDS, THEN A HELD BAR", { "ON THE HITS", "HITS, THEN EIGHTHS" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STABS ON THE HITS", "HELD PADS" }, { "SNARE ON THREE, KICK INTO THE NEXT", "SNARE ON THREE, SHAKER EIGHTHS" }, {} },
                { "HELD CHORD WITH A RINGING TOP, BUSY KICK", { "STRUCK, RINGING TOP, PICKUP", "STRUCK, RINGING TOP, TONIC BARS IN EIGHTHS" }, { "ROOTS HELD", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "BUSY KICK, RIDE BELL, CHINA ON FOUR", "BUSY KICK, CRASH EVERY BAR" }, {} },
                { "CHANGES ON THREE", { "OPEN EIGHTHS, CHANGING ON THREE", "WITH A PASSING CHORD ON TWO" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "FOUR ON THE FLOOR, BACKBEAT", "FOUR ON THE FLOOR, PICKUP SIXTEENTH" }, {} },
                // (0.171.0) Round five's bridges.
                { "PEDAL CELL, FALLING TOP, CHORDS IN THE GAP", { "FALLING TOP, CHORDS IN THE GAP", "PEDAL CELL, FALLING TOP" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SOFT CRASH QUARTERS, SNARE ON THREE", "HALF TIME, SPARSE HATS" }, {} },
                { "BASS AND RIDE ALONE, THE GUITAR DRIFTS IN", { "OUT, THEN HELD CLASH TONES", "HELD CLASH TONES ALL THROUGH" }, { "FOUR-BAR CELL WITH EMPTY BARS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIDE QUARTERS, CHINA EVERY EIGHT BARS", "RIDE QUARTERS, SNARE ON THREE" }, {} },
                { "LOW HIT, HIGH MAJOR-7TH DYAD, CHROMATIC CLIMB", { "LOW HITS, HIGH DYAD, CLIMB", "LOW HITS AND HIGH DYAD ONLY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "CRASH, CHINA AND KICK TOGETHER", "BACKBEAT, CLOSED HATS" }, {} },
                { "FLAT-SEVEN OCTAVES, SUS TO MAJOR TOP", { "OCTAVES ROUND THE FLAT SEVEN", "OCTAVES ROUND THE FLAT SEVEN, NO TOP" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF TIME, OPEN HATS, THEN CRASH QUARTERS", "BACKBEAT, CLOSED HATS" }, {} },
                { "FLAT-TWO DRIVE TO THE SEVEN", { "FLAT TWO IN EIGHTHS, THEN THE SEVEN", "FLAT TWO IN EIGHTHS ALL THROUGH" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "CRASH EVERY TWO BARS, DRIVING", "DRIVING EIGHTHS BEAT" }, {} },
                { "GUITAR ALONE: A BAR OF LINE, A BAR RUNG", { "A BAR OF LINE, A BAR RUNG", "LINE AND RUNG BARS IN TURN" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT", "OUT, A SNARE ROLL TO END" }, {} },
                // (0.170.0) Round five's post-choruses and interludes.
                { "NINTH CLASH IN EIGHTHS", { "NINTH CLUSTER, TRITONE ENDING", "NINTH CLUSTER IN EIGHTHS" }, { "A WHOLE STEP ABOVE THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HIGH FIFTH EVERY TWO BARS", "SILENT" }, { "CRASH QUARTERS, KICK OFF THE BEAT", "SNARE ON THE DOTTED EIGHTHS" }, {} },
                { "ONE CHORD RUNG OVER THE INTRO BEAT", { "ONE CHORD RUNG EIGHT BARS", "ONE CHORD RUNG FOUR BARS" }, { "ROOT HELD UNDER IT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF-TIME HAT QUARTERS, KICK CYCLE", "RIDE QUARTERS, SNARE ON THREE" }, {} },
                { "HELD SECOND CLUSTER, SIXTEENTH PICKUP", { "HELD SECOND CLUSTER, PICKUP", "HELD SECOND CLUSTER ONLY" }, { "ROOT HELD, THEN THE PICKUP", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "TOM GROOVE, SPLASH QUARTERS", "BACKBEAT, CLOSED HATS" }, {} },
                // (0.169.0) Round five's choruses.
                { "HALF-STEP GRIND IN THREES, HIGH TONIC TREMOLO", { "GRIND IN THREES, HIGH TONIC TREMOLO", "GRIND IN THREES ONLY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HIGH TONIC PEDAL" }, { "KICK ON THE GRIND, CRASH PAIRS", "BACKBEAT, OPEN HAT OFF-BEATS" }, {} },
                { "FIXED DISSONANT CHORD, BASS ROCKS UNDER IT", { "FIXED CLUSTER ON THE BEATS", "FIXED CLUSTER, PLAIN VOICING UNDER" }, { "WHOLE NOTES, FIFTH THEN ROOT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "CLUSTER TOP AS A PAD" }, { "CHINA EIGHTHS, FLOOR TOM ACCENTS", "BACKBEAT, CLOSED HATS" }, {} },
                { "LOW CHORD, HIGH CLASH, WALK BAR", { "LOW CHORD, HIGH CLASH, WALK BAR", "LOW CHORD AND HIGH CLASH ONLY" }, { "ROOTS UNDER THE CLASH", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "CRASH ON THE KICKS, RIDE BETWEEN", "BACKBEAT, CLOSED HATS" }, {} },
                { "RINGING, THEN DRIVING", { "RINGING, THEN DRIVING", "DRIVING EIGHTHS ALL THROUGH" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF TIME, OPEN HATS, THEN CRASH QUARTERS", "BACKBEAT, CLOSED HATS" }, {} },
                { "TONIC BAR, CHROMATIC FALL BAR", { "TONIC BAR, CHROMATIC FALL BAR", "TONIC BAR EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SEESAW OVER IT", "SILENT" }, { "RIDE EIGHTHS, CRASH ON THE FALL", "BACKBEAT, CLOSED HATS" }, {} },
                { "CALL, AIR, DRIVE", { "CALL, AIR, DRIVE", "CALL AND DRIVE, NO AIR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "CALL, AIR, INTERLOCKING DRIVE", "BACKBEAT, CLOSED HATS" }, {} },
                { "RING THE OCTAVE OVER A MOVING BASS", { "HIGH OCTAVE RUNG A BAR", "HIGH OCTAVE IN EIGHTHS" }, { "WHOLE AND HALF NOTES, MOVING", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF-TIME RIDE QUARTERS", "BACKBEAT, CLOSED HATS" }, {} },
                // (0.182.0) Round six's choruses.
                { "KICK AND TOMS 3+3+2 UP FRONT, BELL QUARTERS", { "ROOT POWER CHORD STREAM", "POWER CHORDS ON THE 3+3+2 FRONT" }, { "ROOTS HELD", "ROOT ON THE 3+3+2 FRONT", "LOCKED WITH THE KICK" },
                  { "SIXTEENTH CHORD FIGURE", "HELD PADS" }, { "KICK AND TOM 3+3+2 FRONTS, BELL QUARTERS", "BACKBEAT, CLOSED HATS" }, {} },
                { "BAND IN QUARTERS", { "FULL CHORDS ON THE QUARTERS", "POWER CHORDS ON THE QUARTERS" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "CHORD QUARTERS", "HELD PADS" }, { "CRASH QUARTERS, BACKBEAT, SNARE PICKUP", "BACKBEAT, CLOSED HATS" }, {} },
                { "KICK ON ONE AND TWO, SNARE ON THREE", { "OPEN CHORDS ON EVERY EIGHTH", "OPEN CHORDS, OCTAVES DOUBLED" }, { "ROOT EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRING OCTAVES IN EIGHTHS", "HELD PADS" }, { "KICK ON ONE AND TWO, SNARE AND CLAP ON THREE", "BACKBEAT, CLOSED HATS" }, {} },
                { "POWER CHORDS IN THE THIRD-EIGHTH CYCLE", { "CYCLE OF CHORDS ROUND THE ROOT", "ROOT CHORD IN THE CYCLE, MUTED BETWEEN" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON ONE, SNARE ON THREE, CRASH HALVES", "BACKBEAT, CLOSED HATS" }, {} },
                { "CHORDS ON ONE AND THE AND OF TWO, HIP-HOP KICK", { "CHORDS ON ONE AND TWO-AND, HELD", "CHORDS ON ONE AND TWO-AND, CLIPPED" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HIP-HOP KICK, SPLASH QUARTERS", "BACKBEAT, CLOSED HATS" }, {} },
                { "HELD CHORDS IN 6+6+4 OVER A CLEAN ARPEGGIO", { "CHORDS IN 6+6+4, PUSHED HITS", "CHORDS RUNG IN 6+6+4" }, { "ROOT ON THE PUSHED HITS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "CLEAN ARPEGGIO SIXTEENTHS", "HELD PADS" }, { "KICK ON ONE AND TWO-AND, SNARE ON FOUR", "BACKBEAT, CLOSED HATS" }, {} },
                { "KICK ON THE BEATS, SNARE ON THE ANDS UNDER 3+3+3+3+2+2", { "3+3+3+3+2+2, ROOT AND FLAT SEVEN", "3+3+3+3+2+2 ON ONE CHORD" }, { "STABS AT EACH GROUP'S START", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON THE BEATS, SNARE ON THE ANDS", "BACKBEAT, CLOSED HATS" }, {} },
                { "POWER CHORD AND MAJOR CHORD IN FOURS", { "POWER CHORD FOUR, MAJOR CHORD FOUR", "POWER AND MAJOR IN EIGHTHS" }, { "FIVE SIXTEENTHS AND A REST", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK FOURS, HELD SNARE, OPEN HATS", "BACKBEAT, CLOSED HATS" }, {} },
                { "UNISON HITS ON ONE, TWO AND THE LAST TRIPLET, HALF TIME", { "UNISON HITS, A WHOLE STEP UP HALFWAY", "UNISON HITS ON ONE CHORD" }, { "WITH THE UNISON HITS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF TIME, SNARE ROLL INTO THE CHANGE", "HALF TIME" }, {} },
                { "KICK TRIPLE-SIXTEENTHS, SNARE AND CHINA ON THE ANDS", { "PAIRS EVERY SIX SIXTEENTHS", "PAIRS EVERY SIX, RUNG" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK TRIPLETS, SNARE AND CHINA ON THE ANDS", "BACKBEAT, CLOSED HATS" }, {} },
                // (0.196.0) Round seven's pre-choruses, post-choruses and interludes: the acoustic-led ones list the ACOUSTIC lane's options last.
                { "STRUMS AND STRINGS, DRUMS OUT", { "OUT", "SOFT CHORD RUNG EACH HALF BAR" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HIGH STRINGS ON ONE AND THREE", "SILENT" }, { "CYMBAL SWELL ON THE LAST BAR", "OUT" }, {},
                  { "ACCENTS AND LIGHT UPS, RISING TO THE CHORUS", "ACCENTS AND LIGHT UPS, A SECOND'S HIGH THIRDS" } },
                { "ACOUSTIC EIGHTHS, THEN A STUTTER", { "OUT", "POWER EIGHTHS WITH THE STUTTER" }, { "ROOT EIGHTHS WITH THE STUTTER", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND OPEN HATS ON EVERY EIGHTH, NO SNARE", "KICK EIGHTHS, SNARE ON THE STUTTER" }, {},
                  { "FULL EIGHTHS, THEN THE STUTTER BAR", "FULL EIGHTHS AND THE STUTTER, A SECOND'S HIGH THIRDS" } },
                { "FULL ACOUSTIC EIGHTHS, 3+3+2 HIT, STOP", { "OUT", "POWER CHORDS ON THE HITS" }, { "ON THE HITS, HELD BETWEEN", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIDE QUARTERS, SNARE ON THREE, A ROLL INTO THE STOP", "KICK AND CRASH ON THE HITS ONLY" }, {},
                  { "EIGHTHS, A 3+3+2 BAR, THE LAST CHORD HELD", "EIGHTHS AND THE HIT BAR, A SECOND AS THE BASS" } },
                { "OFF-THE-ONE EIGHTHS, BASS ON THE EDGES", { "QUARTER CHORDS, THEN EIGHTHS OFF THE ONE", "EIGHTHS OFF THE ONE EVERY BAR" }, { "ONLY THE ENTRY AND THE PICKUP", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK BEFORE EACH SNARE, THEN KICK ON ONE-AND", "BACKBEAT, HATS ON THE EIGHTHS" }, {} },
                { "FOUR-STAGE BUILD", { "ROOT EIGHTHS, A SECOND'S CELL JOINS", "POWER EIGHTHS ALL THROUGH THE BUILD" }, { "A HIT EVERY TWO BARS, THEN THE CELL, THEN EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "PADS FROM THE THIRD STAGE" }, { "QUARTERS, CELL, BACKBEAT, SNARE QUARTERS", "BACKBEAT FROM HALFWAY" }, {} },
                { "ACOUSTIC UNDER THE RIFF", { "LOW SINGLE-NOTE RIFF, A BAR OFF IN FOUR", "THE LOW RIFF IN OCTAVES" }, { "THE RIFF, ALONE EVERY FOURTH BAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "BACKBEAT, RIDE EIGHTHS, OUT IN THE BASS BAR", "HALF TIME, THE KICK ON THE RIFF" }, {},
                  { "THE PEDAL STRUM UNDER THE RIFF", "THE PEDAL STRUM, A SECOND AS THE BASS" } },
                { "STRUM, THEN THE LOW-STRING WALK", { "OUT", "ONE CHORD RUNG ON THE STRUM BARS" }, { "OUT", "ON ONE ONLY", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "GROOVE, THEN A DOWNBEAT-ONLY BAR", "OUT" }, {},
                  { "CHORD AND BASS LINE, THEN THE CLIMB", "THE WALK, A SECOND'S HIGH ANSWERS" } },
                { "HIGH THIRDS OVER THE ARPEGGIO", { "OUT", "SOFT CHORD RUNG EVERY TWO BARS" }, { "WHOLE NOTES", "OUT", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIDE BELL ON THE QUARTERS", "OUT" }, {},
                  { "THE ARCH, HIGH THIRDS OVER IT", "THE ARCH, A HIGH SEESAW OVER IT" } },
                { "BURST ON ONE UNDER HELD TRIADS", { "FIVE-SIXTEENTH BURST ON ONE", "THE BURST, A SECOND ON THE ROOT" }, { "ROOT AND FIFTH HELD", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HIGH TRIAD AND OCTAVE HELD", "SILENT" }, { "KICK, SNARE AND OPEN HAT TOGETHER ON TWO AND FOUR", "THE STOMP, A CRASH EVERY FOUR BARS" }, {} },
                { "BELL ON THE DOTTED QUARTERS, GUITAR ALONE", { "POWER EIGHTHS, ALONE AT FIRST", "POWER EIGHTHS, ACCENTS ON THE BELL" }, { "OUT, THE ROOTS FROM HALFWAY", "OUT", "LOCKED WITH THE KICK" },
                  { "SILENT", "PADS FROM HALFWAY" }, { "RIDE BELL ON THE DOTTED QUARTERS, THE KIT FROM HALFWAY", "RIDE BELL ON THE DOTTED QUARTERS ALONE" }, {} },
                { "BASS BREAK, KICK ON THE QUARTERS", { "OUT, A UNISON FILL AT THE END", "OUT" }, { "SIXTEENTH PAIRS, A FILL AT THE END", "ROOT EIGHTHS, A FILL AT THE END", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK QUARTERS, CLOSED HATS, NO SNARE", "KICK QUARTERS, RIDE EIGHTHS, NO SNARE" }, {} },
                { "ALTERNATING BLAST UNDER TREMOLO CHORDS", { "TREMOLO CHORDS, A SECOND A THIRD UP", "TREMOLO CHORDS ALONE" }, { "ROOT SIXTEENTHS UNDER THE TREMOLO", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "ALTERNATING BLAST, THEN KICK SIXTEENTHS", "ALTERNATING BLAST ALL THROUGH" }, {} },
                // (0.195.0) Round seven's choruses: the acoustic-led ones list the ACOUSTIC lane's options last.
                { "BIG STRUMS UNDER POWER-CHORD QUARTERS", { "POWER CHORDS ON BEATS TWO AND THREE", "POWER CHORDS ON ONE AND THREE, RUNG" }, { "ROOT ON ONE AND THE A OF FOUR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "HALF TIME, RIDE QUARTERS, TAMBOURINE ON THE ANDS", "HALF TIME, CRASH ON EVERY BAR" }, {},
                  { "BIG STRUMS, LIGHT TOPS ON THE ANDS", "BIG STRUMS, A SECOND'S HIGH SEESAW" } },
                { "FIFTH STACKS GROWING IN THE BAR", { "OUT", "POWER EIGHTHS UNDER THE STACKS" }, { "ROOT EIGHTHS WITH THE KICK", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SHAKER EIGHTHS, LOUDER ON THE ANDS", "KICK EIGHTHS, SNARE ON THREE, CRASH QUARTERS" }, {},
                  { "FIFTH STACKS ON EVERY EIGHTH", "FIFTH STACKS, A SECOND'S HIGH SEESAW" } },
                { "STRUM, EACH CHANGE AN EIGHTH EARLY", { "OUT", "POWER CHORDS ON THE PUSHES" }, { "ROOTS ON THE PUSHES", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND CRASH ON EVERY PUSH", "SIDESTICK BACKBEAT, SHAKER EIGHTHS" }, {},
                  { "THE PUSHED STRUM", "THE PUSHED STRUM, A SECOND'S HIGH THIRDS" } },
                { "SIXTEENTH STRUM, CHANGE ON THE E OF THREE", { "OUT", "OVERDRIVEN SIXTEENTH CHUG UNDER IT" }, { "ROOT SIXTEENTHS FOLLOWING THE PUSH", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "BACKBEAT, CRASH ON TWO AND FOUR", "OUT" }, {},
                  { "SIXTEENTH STRUM, THE CHANGE PUSHED", "SIXTEENTH STRUM, A SECOND'S FILLS" } },
                { "BASS NOTE AND STRUM UNDER THE BAND", { "OUT", "POWER CHORD RUNG EACH BAR" }, { "ROOT, THEN A HALF STEP UNDER THE NEXT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ONE, TWO-AND, THREE, FOUR-AND, CRASH EVERY BAR", "OUT" }, {},
                  { "BASS NOTE, THEN THE STRUM", "BASS NOTE AND STRUM, HIGH ANSWERS OVER IT" } },
                { "TWO ACOUSTICS, TWO STRUMS", { "OUT", "CLEAN SLOW LINE ON THE CHORD TONES" }, { "ROOT, THEN FIFTH ON THREE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SIDESTICK QUARTERS, KICK ON ONE", "OUT" }, {},
                  { "BASS NOTE AND STRUM, SYNCOPATED STRUMS OVER", "SYNCOPATED FULL STRUMS, STRINGS ADDED ONE BY ONE" } },
                { "LILT IN THREES, POWER CHORDS ON TWO AND THREE", { "POWER CHORDS ON BEATS TWO AND THREE", "OUT" }, { "ROOT ON EACH THREE'S START", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON THREE, RIDE, OPEN HATS", "CONGAS ON THE THREES, KICK ON THREE" }, {},
                  { "THE STRUM IN THREES", "THE STRUM IN THREES, A SECOND'S HIGH THIRDS" } },
                { "BASS-STRING STRUM UNDER THE CHUGS", { "MUTED BURST, THEN EIGHTHS", "MUTED BURST, A WALK-UP EVERY OTHER BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK WITH THE BURST, BACKBEAT, CRASH QUARTERS", "SHAKER EIGHTHS, SOFT KICK ON ONE" }, {},
                  { "BASS-STRING STRUM, TIED OVER", "BASS-STRING STRUM, A SECOND AS THE BASS" } },
                { "THE VERSE'S RHYTHM AS POWER CHORDS", { "FLICK EIGHTHS AS POWER CHORDS", "HALF-BAR CHORDS, A FLICK INTO EACH" }, { "ROOT EIGHTHS UNDER THE STRUM", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "BACKBEAT, RIDE EIGHTHS, PEDAL HAT ON THE BEATS", "HALF TIME, OPEN HAT QUARTERS UNDER THE STRUM" }, {},
                  { "THE FLICK STRUM, QUIET UNDER IT", "THE LIGHT-UPS STRUM, QUIET UNDER IT" } },
                { "OPEN-STRING CHORD HITS", { "FOURTHS CHORD, THEN THE OPEN STRINGS", "OPEN STRINGS ON EVERY HIT" }, { "HALF STEP OVER, THEN THE ROOT ON THE HITS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "LOW ROOT PAD" }, { "KICK ON EVERY HIT, SNARE ON THREE", "BACKBEAT UNDER THE OPEN HITS" }, {} },
                { "SYNCOPATED OPEN STRUM OVER THE EIGHTHS", { "POWER EIGHTHS, A SECOND STRUMS OVER THEM", "POWER EIGHTHS ALONE" }, { "ROOT WITH THE STRUM'S BACK HALF", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK WITH THE STRUM'S BACK HALF, OPEN HAT ANDS", "BACKBEAT, OPEN HAT ON THE ANDS" }, {} },
                { "WALTZ GROOVE ACROSS THREE BARS", { "POWER EIGHTHS, ACCENTS EVERY DOTTED QUARTER", "CHORDS RUNG EVERY DOTTED QUARTER" }, { "ROOTS ON THE DOTTED QUARTERS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "TWELVE-SIXTEENTH GROOVE ACROSS THE BAR LINES", "KICK EVERY DOTTED QUARTER, BACKBEAT" }, {} },
                { "DOTTED CHORDS, THEN A BAR OF ROOT EIGHTHS", { "THREE DOTTED CHORDS, THEN ROOT EIGHTHS", "DOTTED CHORDS AND EIGHTHS, A HIGH TREMOLO DYAD OVER" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "DOTTED KICK, OPEN HAT EIGHTHS UNDER THE CHORDS", "BACKBEAT, CRASH QUARTERS UNDER THE DOTS" }, {} },
                { "HELD CHORD, PUSHED PAIRS", { "HELD CHORD, PAIRS PUSHED LATER EACH BAR", "PUSHED PAIRS, AN OCTAVE UP OVER THEM" }, { "ROOTS ON THE PUSHED PAIRS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF TIME, KICKS WITH THE PAIRS", "RIDE QUARTERS, SNARE ON THREE, KICK ON ONE" }, {} },
                { "HIT, THEN EIGHTHS OFF TWO-AND", { "HIT RUNG, EIGHTHS FROM TWO-AND", "HIT AND EIGHTHS, A SIXTH ABOVE ON A SECOND" }, { "ROOT ON ONE, THEN TWO-AND, THREE, THREE-AND, FOUR-AND", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ONE, TWO-AND, THREE, FOUR-AND, SNARE ON THREE", "SNARE ON THREE, RIDE QUARTERS" }, {} },
                { "IN ON TWO, ANSWERS ON THE ANDS", { "IN ON TWO, A FULL BAR ANSWERS", "IN ON TWO, A SECOND STABS THE ANDS" }, { "STABS ON THREE-AND AND FOUR-AND, THEN UNISON", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF TIME, KICK ON ONE-AND AND THE STABS", "OPEN HAT QUARTERS, STRAIGHT BACKBEAT" }, {} },
                { "ONE, TWO, THEN TWO PUSHES", { "ONE, TWO, TWO PUSHES", "ONE, TWO, TWO PUSHES, DOWN A HALF STEP" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OPEN HATS, THEN CRASHES ON ONE, TWO AND FOUR", "BACKBEAT, KICK ON THE PUSHES" }, {} },
                { "HALF TIME, KICK ON TWO, HELD PAIRS", { "HELD PAIRS AND OFF-BEAT EIGHTHS", "HELD PAIRS ON SINGLE LOW NOTES" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE ON THREE, KICK ON TWO AND FOUR-A", "KICK AND SNARE ONLY, HATS OUT" }, {} },
                // (0.168.0) Round five's verses.
                { "PEDAL STUTTER UNDER OCTAVE THIRDS", { "STUTTER UNDER OCTAVE THIRDS", "STUTTER, CHORDS RUNG OVER IT" }, { "SIXTEENTH ROOTS WITH TWO HOLES", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SYNCOPATED KICK PAIR, OPEN HATS", "CHINA EIGHTHS, SYNCOPATED KICK" }, {} },
                { "CHOKED EIGHTHS, HIGH COLOUR TONE HELD", { "CHOKED EIGHTHS", "CHOKED EIGHTHS, HIGH COLOUR TONE" }, { "ROOTS HELD", "EIGHTHS", "LOCKED WITH THE KICK" },
                  { "HIGH COLOUR TONE", "SILENT" }, { "HALF TIME, FOUR-BAR KICK CYCLE", "BACKBEAT, CLOSED HATS" }, {} },
                { "BASS RUNS UNDER HELD CHORDS", { "HELD CHORD, THREE HIGH ANSWERS", "HELD CHORD ONLY" }, { "SIXTEENTHS CIRCLING THE ROOT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "BACKBEAT, HAT RUFF EVERY SECOND BAR", "BACKBEAT, CLOSED HATS" }, {} },
                { "ONE RIFF, THE KIT CHANGES", { "DOUBLE-PICKED RIFF", "DOUBLE-PICKED RIFF, OUT FOR FOUR BARS" }, { "THE RIFF IN EIGHTHS, THEN SIXTEENTHS", "THE RIFF IN SIXTEENTHS", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIDE SIXTEENTHS, KICK PAIRS", "KICK RUN, CRASH QUARTERS" }, {} },
                { "OPEN OCTAVE BAR, FALLING TRITONE ANSWER", { "OPEN OCTAVE BAR, TRITONE ANSWER", "OPEN OCTAVE BAR EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF-TIME HAT QUARTERS, KICK CYCLE", "BACKBEAT, CLOSED HATS" }, {} },
                { "PEDAL CELL WITH A HIGH ANSWER", { "PEDAL CELL, HIGH ANSWER", "PEDAL CELL ONLY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF TIME, SPARSE HATS", "HALF TIME, OPEN HAT QUARTERS" }, {} },
                // (0.194.0) Round seven's verses: the acoustic-led ones list the ACOUSTIC lane's options last.
                { "CHORD TO OPEN PEDAL, TAMBOURINE ONLY", { "OUT", "CLEAN HIGH DYADS OVER THE PEDAL" }, { "ROOT, PASSING NOTE, TONIC ON TWO-AND", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "TAMBOURINE BACKBEAT, CRASH EVERY FOUR BARS", "TAMBOURINE BACKBEAT, THE KIT FROM HALFWAY" }, {},
                  { "THE CHORD, THEN THE OPEN PEDAL", "PEDAL STRUM, A SECOND AS THE BASS", "PEDAL STRUM, FILLS OVER IT" } },
                { "OFFBEAT STRUMS OVER A TOM TRESILLO", { "OUT", "PICKED ROOTS ON THE TRESILLO" }, { "TRESILLO ROOT AND OCTAVE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "FLOOR TOM ON THE TRESILLO, SNARE ON TWO AND FOUR", "FLOOR TOM TRESILLO, KICK ON ONE" }, {},
                  { "HIGH SHAPES OFF THE BEAT", "HIGH SHAPES, HIGH THIRDS OVER THEM" } },
                { "LOW CHORD AND HIGH CLUSTER, BAR BY BAR", { "OUT", "LOW POWER CHORD ON THE LILT" }, { "ROOT PICKUPS ONLY", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "PEDAL HAT, DOTTED KICK, CHINA AT THE PHRASE END", "DOTTED KICK, LATE SNARE, NO HATS" }, {},
                  { "LOW CHORD, HIGH CLUSTER", "LOW CHORD, HIGH CLUSTER, A SECOND ANSWERS" } },
                { "ACOUSTIC JOINS HALFWAY ON THE RIFF", { "OUT", "MUTED ROOTS ON THE RIFF FROM HALFWAY" }, { "THE BASS'S SIXTEENTH RIFF", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SIXTEENTH HATS, KICK ON ONE-E AND THREE-E, A BUILD TO END", "SIXTEENTH HATS, PLAIN BACKBEAT" }, {},
                  { "OUT, THEN THE RIFF'S RHYTHM STRUMMED", "THE RIFF'S RHYTHM STRUMMED THROUGHOUT" } },
                { "TWIN ACOUSTIC ARPEGGIOS", { "OUT", "CLEAN STRUM ONCE EVERY FOUR BARS" }, { "WHOLE NOTES FROM THE SECOND CHORD", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT", "SHAKER EIGHTHS, SIDESTICK ON FOUR" }, {},
                  { "TWO ARPEGGIOS, ONE RHYTHM", "ONE ARPEGGIO, A STRUM RINGING UNDER" } },
                { "FINGERPICKING, SHAKER AND HATS", { "OUT", "QUIET ELECTRIC DOUBLE OF THE PICKING" }, { "HELD ROOT, A REPEAT ON FOUR EVERY OTHER BAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SHAKER AND HATS, KICK EVERY OTHER BAR", "SHAKER ALONE, THEN THE HATS" }, {},
                  { "PICKING OVER THE HELD ROOT", "PICKING, A SECOND ON A HIGH SEESAW" } },
                { "HIGH ROLLED TRIADS OVER A UNISON LINE", { "SLOW LINE WITH THE BASS", "OUT" }, { "SLOW UNISON LINE, WALKS INTO THE CHANGES", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF TIME, KICK ONE, TWO-AND, FOUR, RIDE ON THE BEATS", "HALF TIME, SNARE ON THREE, CLOSED HATS" }, {},
                  { "ROLLED TRIADS, A BREATH, THEN PICKED", "ROLLED TRIADS, A SECOND'S FILLS" } },
                { "STAB CELL ON THE WEAK SIXTEENTHS", { "ON-BEAT STABS UNDER THE CELL", "OUT" }, { "SIXTEENTH RIFF ON THE DOWNBEATS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SOFT RIDE SIXTEENTHS, THEN THE GROOVE", "KICK ON ONE, ONE-AND, TWO-A, THREE, THREE-AND, FOUR-AND" }, {},
                  { "STAB CELL, A BAR OFF", "STAB CELL, A SECOND AS THE BASS" } },
                { "DRONE PAIR AND HAMMER-ONS", { "OUT", "CLEAN ROOT AND FIFTH ON ONE AND THREE" }, { "ROOT ON ONE, PUSHED INTO THREE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HIGH STRINGS ON ONE AND THREE", "SILENT" }, { "SIXTEENTH HATS AND SHAKER, KICK ONE, THREE, THREE-AND", "PEDAL HAT QUARTERS, RIDE EIGHTHS, BACKBEAT" }, {},
                  { "HAMMER-ONS UNDER THE DRONE PAIR", "HAMMER-ONS, HIGH THIRDS OVER THEM" } },
                { "CLEAN STRUM WITH FLICKS", { "OUT", "THE SAME STRUM ON A CLEAN ELECTRIC" }, { "ROOTS ON ONE AND THREE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SOFT BACKBEAT, HAT EIGHTHS", "SIDESTICK BACKBEAT, HAT EIGHTHS" }, {},
                  { "SOFT QUARTERS, FLICK INTO TWO AND FOUR", "SOFT EIGHTHS, FLICK INTO EACH HALF BAR", "SOFT ACCENTS, LIGHT UPS" } },
                { "HIGH OSTINATO, LOW RIFF EVERY OTHER BAR", { "LOW RIFF EVERY OTHER BAR, THE OSTINATO OVER IT", "LOW RIFF EVERY OTHER BAR ALONE" }, { "THE LOW RIFF, EVERY OTHER BAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PLUCKED OSTINATO IN OCTAVES", "SILENT" }, { "HATS ON THE BEATS, THE KICK SWAPS EACH BAR", "HATS ON THE BEATS, PLAIN BACKBEAT" }, {} },
                { "RIFF AFTER THE ONE, FOUR ON THE FLOOR", { "UNISON LINE OFF THE ONE, TRITONE UNDER", "UNISON LINE OFF THE ONE, FOURTH UNDER" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "FOUR ON THE FLOOR, SNARE FROM HALFWAY", "FOUR ON THE FLOOR, OPEN AND PEDAL HATS, BACKBEAT" }, {} },
                { "STRINGS PULSE, GUITARS OUT", { "OUT, THEN A MUTED CELL FROM HALFWAY", "OUT" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRINGS ROOT SIXTEENTHS IN OCTAVES", "STRINGS ROOT SIXTEENTHS, ACCENTS IN THREES" }, { "DOTTED KICK, BACKBEAT, OPEN HAT QUARTERS", "DOTTED KICK, BACKBEAT, A DOTTED HIT TO END" }, {} },
                { "SIX CHUGS, TWO HITS, A BAR OF RING", { "SIX CHUGS, TWO HITS, RING A BAR", "SIX CHUGS, TWO HITS, HIGH NOTES IN THE GAP" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK BURST, CRASH QUARTERS, CHINA ON FOUR", "BACKBEAT UNDER CRASH QUARTERS" }, {} },
                { "PEDAL EIGHTHS, SUS4-TO-3 DYAD", { "MUTED PEDAL, SUS4 TO THIRD DYADS", "MUTED PEDAL ALONE" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE PICKUP INTO TWO AND FOUR, PEDAL KICK", "BACKBEAT, QUARTER HATS, PEDAL KICK" }, {} },
                { "GAPPED CHUGS, TREMOLO IN THE GAPS", { "GAPPED CHUGS, TREMOLO SECOND GUITAR", "GAPPED CHUGS ALONE" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "DOTTED SNARE BAR, THEN OPEN HAT EIGHTHS", "KICK ON THE CHUGS, BACKBEAT, OPEN HATS" }, {} },
                { "GALLOP THEN TREMOLO NOTE", { "GALLOP CELL, HIGH THIRD IN SIXTEENTHS", "GALLOP CELL, A SECOND GUITAR ON THE FIFTH" }, { "WITH THE GALLOP", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK GALLOP, SNARE ON TWO-AND, CRASH EIGHTHS", "KICK GALLOP, BACKBEAT, RIDE" }, {} },
                { "CALL BAR, BASS ANSWER BAR", { "CHORD RUNG, THREE STABS, A BAR OFF", "CHORD RUNG, THREE STABS, EVERY BAR" }, { "ANSWERS IN THE EMPTY BAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HITS WITH THE CALL, GROOVE IN THE ANSWER", "HALF TIME UNDER THE CALL" }, {} },
                // (0.216.0) Round eight's intros.
                { "ROOT-AND-THIRD EIGHTHS ALONE, THE KIT IN STAGES", { "ROOT AND THE KEY'S THIRD IN EIGHTHS, A SECOND'S HITS", "ROOT AND THE KEY'S THIRD IN EIGHTHS ALONE" }, { "WITH THE SECOND GUITAR'S HITS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HATS ON TWO AND FOUR, THEN KICK, THEN KICK EIGHTHS", "HATS ON TWO AND FOUR, THEN A BACKBEAT" }, {} },
                { "BASS FLICKS UNDER A KEYS RUN", { "OUT", "A HIGH ROOT EVERY BAR FROM HALFWAY" }, { "FLICKS ON ONE AND THREE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "EIGHT-NOTE RUN FROM HALFWAY", "EIGHT-NOTE RUN THROUGHOUT" }, { "CLOSED HAT QUARTERS ONLY", "CLOSED HATS, KICK ON ONE" }, {} },
                // (0.215.0) Round eight's pre-chorus, interlude, bridges and breakdowns.
                { "WHOLE-NOTE LINE FALLING, FOUR ON THE FLOOR", { "TONIC HELD OVER THE FALLING LINE", "THE CHORD OVER EACH BASS NOTE" }, { "WHOLE NOTES FALLING FROM THE ROOT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "FOUR ON THE FLOOR, SIXTEENTH HATS, NO SNARE", "FOUR ON THE FLOOR, EIGHTH HATS, NO SNARE" }, {} },
                { "LOW PEDAL EVERY THIRD SIXTEENTH, CHROMATIC PAIRS ABOVE", { "LOW PEDAL IN THREES, CHROMATIC PAIRS OVER", "LOW PEDAL IN THREES ALONE" }, { "ROOT SIXTEENTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIDE BELL EIGHTHS, KICK ONE AND THREE", "RIDE BELL EIGHTHS, BACKBEAT ONLY" }, {} },
                { "THREE-BAR DOTTED CYCLE UNDER A HIGH PEDAL", { "HIGH ROOT AND FOURTH IN EIGHTHS", "HIGH ROOT AND FIFTH IN EIGHTHS" }, { "THE THREE-BAR DOTTED CYCLE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "CRASH AND KICK EVERY SIX SIXTEENTHS", "KICK EVERY SIX SIXTEENTHS, RIDE QUARTERS" }, {} },
                { "HELD THIRDS CLUSTER, KICK AND HATS BUILD", { "THIRDS CLUSTER RUNG FOUR BARS", "THIRDS CLUSTER EVERY TWO BARS" }, { "ROOT ON THE DRIVING FIGURE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND HATS, THE SNARE FROM HALFWAY", "KICK AND HATS, NO SNARE" }, {} },
                { "SNARE ON TWO AND THREE, BASS ON ONE AND FOUR", { "HIGH ROOT EIGHTHS, A HELD NOTE FROM HALFWAY", "HIGH ROOT EIGHTHS" }, { "ROOT ON ONE, THE HALF STEP UNDER ON FOUR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE ON TWO AND THREE, OPEN HAT QUARTERS", "SNARE ON TWO AND THREE, RIDE EIGHTHS" }, {} },
                { "DOTTED CRASHES, CRASHES ON THE ANDS, A SNARE ROLL BAR", { "ROOT AND OCTAVE EIGHTHS, THE ROLL BAR FROM ONE-AND", "ROOT AND OCTAVE EIGHTHS ALL THROUGH" }, { "ROOT EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "DOTTED CRASHES, ANDS, SNARE ROLL BAR", "DOTTED CRASHES, ANDS, A BACKBEAT UNDER" }, {} },
                { "HATS ALONE, BASS PUSHES ON FOUR, TOM BUILD", { "HIGH ROOT EIGHTHS, HALF STEP UNDER ON ONE AND THREE", "OUT" }, { "ONE NOTE ON FOUR, HELD OVER", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OPEN HATS ON TWO TO FOUR, PEDAL ON THE ANDS, TOM BUILD", "HATS ONLY, NO BUILD" }, {} },
                { "BASS, HATS AND TOMS, THEN THE GUITAR AND FLOOR TOM", { "OUT, THEN POWER-CHORD EIGHTHS", "MUTED EIGHTHS, THEN OPEN" }, { "ROOT EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "TOMS AND HATS, THEN THE FLOOR TOM EIGHTHS", "TOMS AND HATS ALL THROUGH" }, {} },
                { "HIGH DYAD EIGHTHS OVER SOFT TOM-AND-SNARE EIGHTHS", { "HIGH DYAD EIGHTHS, THE LOWER NOTE MOVING", "HIGH DYAD EIGHTHS, THE LOWER NOTE FIXED" }, { "OUT, THEN A PICKUP", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SOFT SNARE AND TOM EIGHTHS, A BUILD TO END", "SOFT TOM EIGHTHS ONLY" }, {} },
                { "BROKEN FOUR-BAR BEAT UNDER STEADY EIGHTHS", { "POWER-CHORD EIGHTHS, HELD TO END", "POWER-CHORD EIGHTHS ALL THROUGH" }, { "TWO EIGHTHS ON ONE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "BROKEN FOUR-BAR BEAT", "BROKEN FOUR-BAR BEAT, RIDE" }, {} },
                { "WHOLE-BAR CHORDS IN THE FLAT KEY, CHROMATIC SLIDE BACK", { "BORROWED CHORDS RUNG, A SLIDE BACK", "BORROWED CHORDS IN EIGHTHS, A SLIDE BACK" }, { "THE BORROWED ROOTS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "BORROWED CHORDS HELD", "SILENT" }, { "RIDE EIGHTHS, QUARTER HATS, A TOM FILL UNDER THE SLIDE", "BACKBEAT, A TOM FILL UNDER THE SLIDE" }, {} },
                { "BASS NEIGHBOUR SIXTEENTHS UNDER LATE DYADS", { "LATE HIGH DYADS, A HELD DYAD ON ONE AND TWO", "LATE HIGH DYADS ALONE" }, { "NEIGHBOUR SIXTEENTHS ROUND THE ROOT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIDE EIGHTHS, SOFT KICK AND SNARE", "RIDE QUARTERS, SOFT KICK AND SNARE" }, {} },
                { "QUARTER STABS OVER A HELD BASS, KICK 1, 2-A, 3, 3-AND", { "SHORT CHORDS ON EVERY QUARTER", "SHORT POWER CHORDS ON EVERY QUARTER" }, { "ROOT ON ONE, HELD FROM ONE-AND, AGAIN ON THREE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "KICK ONE, TWO-A, THREE, THREE-AND, OPEN HAT ANDS", "KICK ONE, TWO-A, THREE, THREE-AND, CLOSED EIGHTHS" }, {} },
                { "RISING FIFTHS ON 1, 1-A, 2-AND, BAND OUT", { "OUT", "THE RISING FIFTHS ON THE GUITAR" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "LOW RISING FIFTHS", "HIGH RISING FIFTHS" }, { "KICK AND OPEN HAT WITH THE FIFTHS, THE KIT BACK AFTER FOUR", "KICK AND OPEN HAT WITH THE FIFTHS ALL THROUGH" }, {} },
                { "UNISON CHROMATIC LINE, A BAR OF ROOT TREMOLO", { "CHROMATIC LINE, THEN HARMONIC FLURRY", "CHROMATIC LINE, THEN OUT" }, { "CHROMATIC LINE, THEN ROOT TREMOLO", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OPEN HAT EIGHTHS, KICK AND SNARE ON THE LINE", "OPEN HAT EIGHTHS ONLY" }, {} },
                { "ONE HIT ON ONE-AND, EVERY OTHER BAR", { "ONE HIT ON ONE-AND, A BAR OF SILENCE", "ONE HIT ON ONE-AND EVERY BAR" }, { "WITH THE HITS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND CRASH WITH THE HITS", "KICK WITH THE HITS, NO CYMBALS" }, {} },
                // (0.214.0) Round eight's choruses.
                { "SIX-HIT CHORD BAR OVER THE ROOTS", { "WIDE CHORDS ON SIX HITS", "POWER CHORDS ON SIX HITS" }, { "ROOTS ON THE SIX HITS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIDE EIGHTHS FROM ONE-AND, KICK ONE AND THREE", "CRASH EVERY OTHER BAR, CLOSED HAT EIGHTHS" }, {} },
                { "SIXTEENTH OCTAVES ON THE FLAT SEVEN OVER A ROOT RIFF", { "OCTAVES ON THE SEVENTH OVER THE ROOT RIFF", "OCTAVES ON THE SEVENTH ALONE" }, { "THE ROOT GROOVE, SEVENTH AND SIXTH TAILS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "FOUR-KICK BURST, HAT EIGHTHS, CRASH EVERY TWO", "FOUR-KICK BURST, RIDE QUARTERS" }, {} },
                { "OCTAVES CLIMB A STEP A BAR", { "POWER CHORD EIGHTHS, THE OCTAVES CLIMBING OVER", "THE OCTAVES CLIMBING ALONE" }, { "THE CLIMB IN OCTAVES", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK EIGHTHS, SNARE QUARTERS, CRASH EVERY BAR", "KICK EIGHTHS, BACKBEAT, HAT QUARTERS" }, {} },
                { "HALF-STEP SHOVE ON FOUR-AND", { "EIGHTHS, A HALF-STEP SHOVE ON FOUR-AND", "EIGHTHS, THE SHOVE AND A BROKEN SECOND GUITAR" }, { "ROOT EIGHTHS, THE SHOVE ON FOUR-AND", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK WITH THE SHOVE, CRASH QUARTERS", "KICK WITH THE SHOVE, OPEN HAT QUARTERS" }, {} },
                { "CHORDS ON ONE, THREE, FOUR, FOUR-AND", { "HIGH CHORDS ON ONE, THREE, FOUR, FOUR-AND", "POWER CHORDS ON ONE, THREE, FOUR, FOUR-AND" }, { "ROOT ON ONE, THEN AFTER THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF TIME, KICK ON FOUR-AND, RIDE EIGHTHS", "HALF TIME, KICK ON FOUR-AND, QUARTER HATS" }, {} },
                { "AND-OF-ONE UNISON PUSHES", { "UNISON PUSHES OFF THE ONE", "UNISON PUSHES, THE ONE STRUCK EVERY FOURTH BAR" }, { "ROOT EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON THE PUSHES, OPEN HATS ON TWO, THREE AND FOUR", "KICK ON THE PUSHES, RIDE EIGHTHS" }, {} },
                { "UNISON PUSH RIFF, PAIRED-SNARE BAR EVERY FOURTH", { "THE PUSH RIFF, A BUSIER FOURTH BAR", "THE PUSH RIFF EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK WITH THE RIFF, PAIRED SNARES EVERY FOURTH", "KICK WITH THE RIFF, PLAIN BACKBEAT" }, {} },
                { "TWO HITS, THEN AIR", { "POWER CHORDS ON ONE AND TWO, HIGH QUARTERS OVER", "POWER CHORDS ON ONE AND TWO ALONE" }, { "ON ONE AND TWO", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ONE, TWO AND FOUR, SNARE THREE AND FOUR-AND", "KICK ONE, TWO AND FOUR, PLAIN SNARE" }, {} },
                { "HALF-STEP POWER CHORDS EACH HALF BAR, BASS ROOT-FIFTH-OCTAVE-FIFTH", { "HALF-STEP POWER CHORDS EACH HALF BAR", "HALF-STEP POWER CHORDS EACH BAR" }, { "ROOT, FIFTH, OCTAVE, FIFTH EACH HALF BAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE ON TWO, TWO-A AND FOUR, OPEN QUARTERS", "SNARE ON TWO, TWO-A AND FOUR, CRASH QUARTERS" }, {} },
                { "UNISON HITS, THE KICK ITS OWN WAY", { "UNISON HITS, THE LAST HELD", "UNISON HITS, A CHORD EACH HALF BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "THE KICK ITS OWN WAY, OPEN HATS ON TWO, THREE AND FOUR", "THE KICK ITS OWN WAY, CLOSED HAT EIGHTHS" }, {} },
                // (0.236.0) Round nine's pre-choruses, intros, bridge, breakdowns and outros.
                { "RING ONCE, THE BASS MOVES THE CHANGES", { "THE KEY'S CHORD RUNG EVERY TWO BARS", "THE KEY'S CHORD RUNG EVERY BAR" }, { "THE CHANGES IN EIGHTHS UNDER THE RING", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF-TIME SNARE, RIDE EIGHTHS", "BACKBEAT, CLOSED HAT EIGHTHS" }, {} },
                { "GAPPED QUARTERS OVER THE DISCO HAT", { "POWER CHORDS ON 1, 2, 4, A GAP ON 3", "POWER CHORDS ON EVERY QUARTER, SHORT" }, { "ROOT EIGHTHS WITH GAPS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "STABS WITH THE GAPPED QUARTERS" }, { "DISCO HAT: CLOSED ON THE BEAT, GHOST ON THE E, OPEN ON THE AND", "DISCO HAT, NO GHOSTS" }, {} },
                { "HELD HORN LINE OVER THE RIDE GALLOP", { "POWER-CHORD EIGHTHS UNDER THE HORN", "OUT" }, { "ROOT EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HORN NOTE FROM TWO-AND, FALLING A STEP A BAR", "HORN NOTE IN OCTAVES" }, { "RIDE GALLOP, FOUR ON THE FLOOR", "RIDE GALLOP, BACKBEAT" }, {} },
                { "MUSIC-BOX OSTINATO, THE KIT BUILDS UNDER", { "OUT", "HIGH ROOT RUNG FROM HALFWAY" }, { "ROOTS FROM HALFWAY", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "MUSIC-BOX EIGHTHS IN OCTAVES", "MUSIC-BOX EIGHTHS, ONE OCTAVE" }, { "SIDESTICK, THEN KICK, THEN SIXTEENTH HATS", "SIDESTICK QUARTERS ALL THROUGH" }, {} },
                { "TRIBAL KICK EIGHTHS, FALLING BASS CELL", { "METALLIC DYAD STAB EVERY TWO BARS", "OUT" }, { "THREE-NOTE FALL EACH BEAT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK EIGHTHS, ACCENTED TOMS", "KICK EIGHTHS, FLOOR TOM ONLY" }, {} },
                { "SPARSE BASS AND A HIGH RUB OVER KICK QUARTERS", { "OUT", "A HIGH NOTE RUBBING A HALF STEP" }, { "ONE ROOT A BAR, THE FIFTH ON FOUR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "A HIGH HALF-STEP RUB HELD", "A HIGH NOTE HELD" }, { "KICK QUARTERS, NOTHING ELSE", "KICK QUARTERS, CLOSED HAT EIGHTHS" }, {} },
                { "LOOP ALONE, THEN THE KEYS, THEN THE BASS", { "OUT", "POWER CHORD HELD IN THE LAST QUARTER" }, { "IN FROM THE SECOND HALF, ROOT EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "IN FROM THE SECOND QUARTER, PULSED CHORDS", "IN FROM THE SECOND QUARTER, HELD" }, { "TOM LOOP, KICK QUARTERS, NO FILLS", "STRAIGHT GROOVE, CLOSED EIGHTHS" }, {} },
                { "DRONE AND HALF-STEP CALL OVER THE MACHINE", { "THE CHORD HELD, A HALF-STEP CALL OVER", "THE CHORD HELD ALONE" }, { "ROOT DRONE IN SIXTEENTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "DRONE ON THE KEY'S ROOT", "SILENT" }, { "MACHINE: KICK EIGHTHS, SNARE TWO AND FOUR, SIXTEENTH HATS", "MACHINE WITHOUT HATS" }, {} },
                { "HALF-STEP TICK OVER A LONE KICK", { "ROOT AND THE HALF STEP ABOVE ON TWO, THREE, FOUR", "ROOT ON TWO, THREE, FOUR" }, { "ROOT ON ONE, FIFTH ON THREE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK EVERY OTHER BAR, RIDE EIGHTHS", "KICK EVERY BAR, RIDE EIGHTHS" }, {} },
                { "KEYS OVER THE MACHINE PULSE", { "OUT", "MUTED ROOT EIGHTHS" }, { "SEQUENCED ROOT SIXTEENTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "A SYNTH CHORD A BAR OVER THE PULSE", "A SYNTH ARPEGGIO OVER THE PULSE" }, { "KICK QUARTERS, CLAP TWO AND FOUR, SIXTEENTH HATS", "KICK QUARTERS, CLAP TWO AND FOUR" }, {} },
                { "STABS EVERY OTHER BAR OVER A CLICK", { "STABS ON ONE AND ONE-AND, EVERY OTHER BAR", "A STAB ON ONE, EVERY OTHER BAR" }, { "WITH THE STABS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "ORCHESTRA HIT WITH THE STABS" }, { "A CLICK ON THE QUARTERS", "A CLICK ON THE EIGHTHS" }, {} },
                { "BASS PULSE UNDER HELD CHORDS, DRUMS OUT", { "THE CHORD HELD TWO BARS", "THE CHORD HELD A BAR" }, { "ROOT EIGHTHS PULSING", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "OUT", "A CRASH EVERY FOUR BARS" }, {} },
                { "STOMP ALONE, THEN THE PULSE", { "OUT, THEN MUTED ROOT EIGHTHS FROM HALFWAY", "OUT" }, { "OUT, THEN ROOT EIGHTHS FROM HALFWAY", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "STOMP: KICK ONE AND THREE, SNARE TWO AND FOUR, NO CYMBALS", "STOMP WITH A TAMBOURINE" }, {} },
                { "MACHINE STOMP OUTRO", { "POWER-CHORD EIGHTHS, ONE HIT TO END", "THE CHORD HELD, ONE HIT TO END" }, { "ROOT AND OCTAVE EIGHTHS, ONE HIT TO END", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK EIGHTHS, SNARE AND SPLASH QUARTERS, ONE HIT", "KICK EIGHTHS, SNARE QUARTERS, ONE HIT" }, {} },
                { "HALF-TIME CHORDS OVER A MARCH", { "A CHORD EACH HALF BAR", "A CHORD EACH BAR" }, { "ROOTS EACH HALF BAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "MARCH, NO KICK", "MARCH WITH A HALF-TIME KICK" }, {} },
                // (0.238.0) Round nine's swung types (verse and chorus).
                { "TRIPLET STOMP, TRITONE CHUG", { "TRIPLET POWER CHORDS, ROOT AND FLAT FIVE", "ONE TRIPLET STAB EVERY OTHER BAR", "MUTED TRIPLET CHUGS ON THE ROOT" },
                  { "DOUBLES THE GUITAR", "CARRIES THE TRIPLETS ALONE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SAW DOUBLES THE TRIPLETS", "SILENT", "HELD PADS" }, { "TRIPLET STOMP CELL", "TRIPLET STOMP, RIDE ON THE BEATS", "HALF-TIME TRIPLET STOMP" }, {} },
                { "SHUFFLE STOMP", { "LOW SHUFFLE RIFF, TURNAROUND EVERY FOURTH BAR", "SHUFFLED POWER CHORDS" },
                  { "SHUFFLE ON EVERY TRIPLET, DRONE FIRST", "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "FLAT-SEVEN SHUFFLE PULSE", "SILENT", "HELD PADS" }, { "SHUFFLE STOMP, KICK AND FLOOR, NO HATS", "SHUFFLE BACKBEAT, SHUFFLED RIDE" }, {} },
                // (0.235.0) Round nine's choruses.
                { "RING BAR, SIXTEENTH CHORD STREAM", { "RING BAR, THEN A CHORD ON EVERY SIXTEENTH", "THE STREAM EVERY BAR" }, { "WITH THE RING AND THE STREAM", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK CLUSTERS IN THE STREAM BAR, OPEN EIGHTHS", "STRAIGHT KICK, OPEN EIGHTHS" }, {} },
                { "HOLD BAR, CHUG BAR, HALF-TIME SNARE", { "HOLD BAR, THEN A CHUG BAR", "HOLD BAR, THEN A CHUG BAR, A SECOND GUITAR'S SIXTEENTHS" }, { "ROOTS UNDER THE HOLDS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF-TIME SNARE IN THE HOLD BAR, CRASH EVERY BAR", "PLAIN BACKBEAT, CRASH EVERY BAR" }, {} },
                { "WHOLE-BAR CHORDS FALLING BY HALF STEPS, CHOIR PEDAL", { "A CHORD A BAR, FALLING A HALF STEP", "A CHORD A BAR ON THE CHORDS" }, { "WHOLE-BAR ROOTS FALLING A HALF STEP", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "CHOIR ON THE KEY'S FIFTH, A VOICE FOLLOWING", "CHOIR CHORDS" }, { "RIDE QUARTERS, CRASH MOST BARS, A BUILD TO END", "RIDE QUARTERS, PLAIN" }, {} },
                { "HELD POWER CHORDS OVER THE LOOP", { "A POWER CHORD HELD TWO BARS", "A POWER CHORD HELD EACH BAR" }, { "INTERLOCKING TWO-BAR LOOP", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "A HIGH FIFTH HELD OVER THE CHORDS", "HELD PADS" }, { "HALF-TIME LOOP, SPLASH ON THREE", "HALF-TIME LOOP, QUARTER HATS" }, {} },
                { "TWO CHORDS HELD OVER STACCATO ROOT QUARTERS", { "OUT", "CHORDS HELD TWO BARS" }, { "SHORT ROOT QUARTERS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "ORGAN AND CHOIR HOLD EACH CHORD", "HELD PADS" }, { "FOUR ON THE FLOOR, NO HATS, A FILL TO END", "FOUR ON THE FLOOR, NO HATS" }, {} },
                { "PAIRS STEPPING DOWN A STEP A BAR", { "CHORD, DYAD, A STEP DOWN AND BACK", "THE SAME, 2+3+3 BARS TO END" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "CRASH QUARTERS, OPEN HATS ON THE ANDS", "RIDE QUARTERS, OPEN HATS ON THE ANDS" }, {} },
                { "RIFF BAR, HALF-STEP SLIDE BAR", { "RIFF BAR, THEN II AND FLAT II", "RIFF BAR, THEN A RUNG CHORD" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "STOMP, CRASH ON THE RIFF BAR", "STOMP, OPEN EIGHTHS" }, {} },
                { "PUNK EIGHTHS IN 3+3+2, SNARE ON THE DOTS", { "EIGHTHS CHANGING IN 3+3+2", "EIGHTHS ON THE BAR'S CHORD" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE ON THE DOTS, KICKS BETWEEN, A SKANK BAR", "SNARE ON THE DOTS, KICKS BETWEEN" }, {} },
                { "CHANT STOP: GUITAR ALONE", { "FIFTH HELD, THEN FOUR SHORT ROOTS", "FIFTH HELD, THEN TWO SHORT ROOTS" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT", "A KICK ON THE LAST BAR'S ONE" }, {} },
                // (0.213.0) Round eight's verses.
                { "ROOT PAIRS IN THREES, TOP NOTE TURNS", { "ROOT PAIRS, THE TOP TURNING", "ROOT PAIRS, THE OCTAVE ONLY" }, { "UNDER THE PAIRS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OPEN HAT QUARTERS, KICK ROUND THE PAIRS", "CLOSED HATS, KICK ROUND THE PAIRS" }, {} },
                { "ROOT AND FLAT-SEVEN DYAD GROOVE", { "SEVENTH DYADS, SIXTHS EVERY OTHER TWO BARS", "SEVENTH DYADS ONLY" }, { "THE ROOT GROOVE, SEVENTH AND SIXTH TAILS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON THE GROOVE, OFF-BEAT HATS", "KICK ON THE GROOVE, RIDE QUARTERS" }, {} },
                { "TWO POWER CHORDS TRADED ACROSS THE BAR", { "TRADED WITH THE CHORD A THIRD UNDER", "TRADED WITH THE NEXT CHORD" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "ALTERNATING KICK, OPEN HAT EIGHTHS, CRASH QUARTERS LATE", "ALTERNATING KICK, CLOSED HAT EIGHTHS" }, {} },
                { "OFF-BEAT DYADS, SCRATCHES IN THE HOLES", { "OFF-BEAT DYADS, A SECOND'S SCRATCHES", "OFF-BEAT DYADS ALONE" }, { "WITH THE DYADS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK OFF THE BEAT, SNARE LATE, OPEN HAT OFF-BEATS", "KICK OFF THE BEAT, PLAIN BACKBEAT" }, {} },
                { "THE DOWNBEAT LEFT OUT, BAR BY BAR", { "LATE ENTRIES, ROCKING TO THE FLAT SEVEN", "LATE ENTRIES ON THE BAR'S CHORD" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "CRASH ON TWO, OPEN HATS ON THREE AND FOUR", "BACKBEAT, OPEN HATS ON THREE AND FOUR" }, {} },
                { "TWO EIGHTHS AND A REST, A LINE IN THE GAPS", { "TWO EIGHTHS AND A REST, A CLEAN ANSWER", "TWO EIGHTHS AND A REST ALONE" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "DRIVING KICK, OPEN HAT EIGHTHS", "DRIVING KICK, CLOSED HAT EIGHTHS" }, {} },
                { "CHORD, THEN A SIXTEENTH RUN TO THE BAR LINE", { "CHORD, THEN THE RUN UP", "CHORD, THE RUN UP, THEN DOWN" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SYNCOPATED KICK, SIXTEENTH HATS", "SYNCOPATED KICK, EIGHTH HATS" }, {} },
                { "SIX ON, TWO OFF", { "THE RIFF SIX BARS, TWO EMPTY", "THE RIFF EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF TIME, GHOST ON FOUR-A, CRASH ON THE RETURN", "HALF TIME, QUARTER HATS, NO GHOSTS" }, {} },
                { "SEVEN-EIGHTH CYCLE OVER FOUR-FOUR", { "SEVEN-EIGHTH LOOP OVER HELD NOTES", "SEVEN-EIGHTH LOOP ALONE" }, { "HELD NOTES FOURTEEN EIGHTHS EACH", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SEVEN-EIGHTH GROOVE", "STRAIGHT BACKBEAT UNDER THE LOOP" }, {} },
                { "OCTAVE GUITAR DOUBLES THE WALKING BASS RIFF", { "THE WALKING LINE IN OCTAVES", "THE WALKING LINE, SINGLE NOTES" }, { "THE WALKING LINE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SPARSE HALF TIME", "HALF TIME, RIDE EIGHTHS UNDER THE LINE" }, {} },
                { "SHORT, SHORT, LONG UNISON, EIGHTHS TO END", { "SHORT, SHORT, LONG, THE EIGHTH BAR IN EIGHTHS", "SHORT, SHORT, LONG EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK WITH THE HITS, RIDE QUARTERS", "KICK WITH THE HITS, CLOSED HAT EIGHTHS" }, {} },
                { "LINE BAR, CHORDS FROM ONE-AND", { "A LINE BAR, THEN CHORDS FROM ONE-AND", "CHORDS FROM ONE-AND EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "FLOOR TOM LINE BAR, BACKBEAT CHORD BAR", "BACKBEAT EVERY BAR, CRASH ON FOUR-A" }, {} },
                { "BASS ALONE, A HALF-STEP TOUCH, QUARTER HATS", { "OUT", "A CHROMATIC LINE FROM HALFWAY" }, { "ROOT EIGHTHS, A HALF-STEP TOUCH", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "QUARTER HATS, A KICK EVERY FOUR BARS", "QUARTER HATS, SIDESTICK BACKBEAT" }, {} },
                { "PEDAL TRIAD WITH CLIMBING INNER VOICES", { "OUT, THEN ROOT EIGHTHS", "ROOT EIGHTHS THROUGHOUT" }, { "THE PEDAL FIGURE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "CLIMBING VOICES OVER THE PEDAL", "THE CHORDS OVER THE PEDAL" }, { "KICK ON THE BASS, NO CYMBALS", "KICK ON THE BASS, QUARTER HATS" }, {} },
                { "UNISON LOW SIXTEENTHS OVER A TOM GROOVE", { "LOW SIXTEENTHS ON THE THREE-BAR PLAN", "LOW SIXTEENTHS ON THE CHORDS" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "THREE-BAR TOM GROOVE, NO CYMBALS", "THREE-BAR TOM GROOVE, HATS ON THE BEATS" }, {} },
                // (0.243.0) Round ten's intros and outros.
                { "TWO GUITARS INTERLOCK, NO DRUMS", { "TWO CLEAN GUITARS INTERLOCKING", "ONE CLEAN ARPEGGIO" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT", "A CYMBAL SWELL INTO THE END" }, {} },
                { "A CAPPELLA HOOK, THEN THE BAND", { "OUT UNTIL THE LAST BAR'S PICKUP", "OUT" }, { "A PICKUP ON FOUR OF THE LAST BAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "A HELD NOTE FOR THE VOICE" }, { "A SNARE PICKUP ON FOUR OF THE LAST BAR", "STICK CLICKS ON THE LAST BAR" }, {} },
                { "PAD ALONE, THEN THE BASS PEDAL, THEN THE BAND", { "IN FOR THE LAST THIRD, POWER CHORDS RUNG", "IN FOR THE LAST THIRD, EIGHTHS" }, { "IN AT A THIRD ON THE KEY'S ROOT, THEN THE ROOTS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PAD CHORDS THROUGHOUT", "PAD CHORDS, A HIGH NOTE ADDED" }, { "IN FOR THE LAST THIRD, A BACKBEAT", "CYMBALS FROM A THIRD, THE BEAT FOR THE LAST" }, {} },
                { "PIANO ONE-NOTE PEDAL INTRO", { "A CLEAN NOTE A BAR FROM HALFWAY", "OUT" }, { "OUT, THEN ROOTS FROM HALFWAY", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PIANO ONE-NOTE SIXTEENTHS ON THE CHORD'S FIFTH", "PIANO SIXTEENTH PEDAL, STRING FIFTH HELD" }, { "OUT", "OUT, THEN A KICK ON ONE FROM HALFWAY" }, {} },
                { "INSTRUMENTAL VERSE ON A BROKEN TRIAD", { "A BROKEN TRIAD OFF THE ONE", "BROKEN TRIAD OFF THE ONE, A RUNG CHORD UNDER" }, { "ROOT EIGHTHS UNDER THE BROKEN TRIAD", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND HAT QUARTERS, TAMBOURINE, A TOM RUN EVERY FOUR", "EIGHTH HATS, KICK ONE AND THREE" }, {} },
                { "A KIT HIT ON THE AND OF FOUR", { "ONE HIT ON THE LAST BAR'S FOUR-AND", "A HELD CHORD, THEN THE HIT" }, { "THE HIT ON THE LAST BAR'S FOUR-AND", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HATS COUNT QUARTERS, A FULL-KIT HIT ON FOUR-AND", "SILENT, A FULL-KIT HIT ON FOUR-AND" }, {} },
                { "VAMP TO A FADE", { "A CHORD RUNG A BAR, FADING", "EIGHTHS, FADING" }, { "ROOT EIGHTHS, FADING", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "THE LOOP, A FILL EVERY FOUR, FADING", "RIDE AND SHAKER SIXTEENTHS, A STOP BAR EVERY FOUR, FADING" }, {} },
                { "RIFF VAMP ON THE RELATIVE MINOR", { "MUTED PENTATONIC RIFF ON THE SIXTH", "THE PENTATONIC RIFF, OPEN" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SYNCOPATED SIXTEENTH DYADS", "SILENT" }, { "KICK OFF THE BEAT, OPEN HATS ON THE E'S", "KICK OFF THE BEAT, CLOSED HATS" }, {} },
                { "SOLO OVER SNARE QUARTERS, BASS CLIMBING", { "RUNG CHORDS UNDER THE SOLO", "OUT" }, { "CLIMBING BY STEP EACH BAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "SNARE ON EVERY QUARTER, KICK ON ONE AND THREE", "SNARE QUARTERS, RIDE EIGHTHS" }, {} },
                { "ONE HIT A BAR TO THE END", { "ONE CHORD HIT ON EACH BAR'S ONE", "ONE HIT A BAR, THE LAST HELD" }, { "THE ROOT ON EACH BAR'S ONE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND CRASH ON EACH BAR'S ONE", "KICK AND CRASH ON ONE, HATS BETWEEN" }, {} },
                { "EVERY BEAT HIT, THEN THE GUITAR ALONE", { "EVERY BEAT HIT, THEN A CHORD ALONE A BAR", "EVERY BEAT HIT, THEN EIGHTHS ALONE" }, { "EVERY BEAT, THEN OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "CHORDS ON EVERY BEAT, THEN OUT" }, { "KICK, SNARE AND CRASH ON EVERY BEAT, THEN OUT", "KICK ON EVERY BEAT, THEN OUT" }, {} },
                { "STRINGS SIXTEENTHS, ENDING ON THE FOUR", { "OUT", "A RUNG CHORD EVERY TWO BARS" }, { "WHOLE NOTES ON THE ROOT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRING SIXTEENTHS, THE LAST BAR ON THE FOUR CHORD", "STRING SIXTEENTHS" }, { "THE TOMS ROLL TO THE END", "OUT" }, {} },
                { "ENDS ON FLAT-SIX TO FLAT-SEVEN", { "STRUMMED, THE LAST TWO BARS FLAT SIX, FLAT SEVEN", "POWER CHORDS, ENDING FLAT SIX, FLAT SEVEN" }, { "ROOTS, ENDING FLAT SIX, FLAT SEVEN", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD, ENDING FLAT SIX, FLAT SEVEN", "SILENT" }, { "BACKBEAT, THE LAST CHORD STRUCK ONCE", "HALF TIME, THE LAST CHORD STRUCK ONCE" }, {} },
                { "TOM BUILD INTO A DOUBLE-TIME CODA", { "HITS ON THREE, THEN A UNISON EIGHTH RIFF", "HITS, THEN POWER-CHORD EIGHTHS" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "TOM BUILD, THEN DOUBLE TIME", "TOM BUILD, THEN A BACKBEAT" }, {} },
                { "MAJOR TONIC TO END", { "RUNG CHORDS, THE LAST THE MAJOR TONIC", "EIGHTH PULSE, THE LAST THE MAJOR TONIC" }, { "PEDAL, THE LAST THE TONIC", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRING SIXTEENTHS, ENDING ON THE MAJOR TONIC", "HELD PADS" }, { "EIGHTH PULSE, A TOM ROLL, A CRASH TO END", "TOM ROLL TO THE CRASH" }, {} },
                { "HAND PERCUSSION, BAND OUT", { "OUT", "ONE SOFT NOTE EVERY FOUR BARS" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "ONE SOFT PAD NOTE", "SILENT" }, { "CONGAS AND BONGOS IN SPARSE SIXTEENTHS", "CONGAS AND A SHAKER" }, {} },
                { "THE KIT THINS, THE LAST CHORD RINGS", { "RUNG CHORDS, THE LAST HELD TO THE END", "EIGHTHS THINNING, THE LAST HELD" }, { "ROOTS, THE LAST HELD", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "THINNING TO KICK ON ONE AND RIDE", "THINNING TO THE RIDE ALONE" }, {} },
                // (0.242.0) Round ten's pre-choruses, post-chorus, bridges and breakdowns.
                { "HALF-BAR CHANGES INTO A 3+2+3 STOP", { "HALF-BAR CHORDS, THE LAST BAR 3+2+3", "HALF-BAR EIGHTHS, THE LAST BAR 3+2+3" }, { "HALF-BAR ROOTS, THE LAST BAR 3+2+3", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "BUILDING EIGHTHS, THE LAST BAR 3+2+3", "SNARE QUARTERS BUILDING, THE LAST BAR 3+2+3" }, {} },
                { "STOP-TIME HITS AND FILLS", { "CHORD ON ONE HELD, THEN A BAR OUT", "CHORD ON ONE, PUSH ON TWO-AND" }, { "WITH THE KICK: ONE, TWO-AND, THREE-AND, FOUR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "STOP TIME: A HIT BAR, THEN A FILL BAR", "STOP TIME, TOMS IN THE FILL BAR" }, {} },
                { "TRIADS CLIMBING BY STEP", { "OUT", "SINGLE-NOTE ANSWERS TO THE KEYS" }, { "ROOT EIGHTHS CLIMBING, A WALK DOWN TO END", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "TRIAD EIGHTHS CLIMBING BY STEP", "TRIADS CLIMBING, HALF NOTES" }, { "EIGHTH HATS, TOMS ON THE E'S TO END", "BACKBEAT, A SNARE FILL ON THE E'S TO END" }, {} },
                { "VERSE RIFF, THE HATS DO THE BUILD", { "THE VERSE'S MUTED EIGHTHS", "THE VERSE'S RUNG CHORDS" }, { "ROOT EIGHTHS UNDER THE BUILD", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HATS FROM QUARTERS TO EIGHTHS TO SIXTEENTHS", "OPEN HATS JOINING BY BARS" }, {} },
                { "ANTHEM HITS ON TWO, THREE-AND, FOUR-AND", { "TRIADS ON TWO, THREE-AND, FOUR-AND", "POWER CHORDS ON TWO, THREE-AND, FOUR-AND" }, { "ROOT EIGHTHS ON THE ANTHEM HITS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "THE ANTHEM TRIADS HELD", "SILENT" }, { "BACKBEAT, KICK QUARTERS, A STOP AFTER TWO", "BACKBEAT, OPEN HATS" }, {} },
                { "QUIET BRIDGE, THEN A SNARE-ON-FOUR REBUILD", { "SOFT CLEAN DYAD EIGHTHS", "SOFT DYADS, POWER CHORDS IN THE LAST QUARTER" }, { "OUT, THEN IN WITH THE REBUILD", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "OUT, THEN SNARE ON FOUR, THEN THE BACKBEAT", "OUT, THEN THE BACKBEAT" }, {} },
                { "UNISON PICKUPS INTO EACH DOWNBEAT", { "A UNISON PICKUP RUN INTO EACH BAR, THEN HELD", "THE PICKUP RUN EVERY OTHER BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "BRASS ANSWERS IN EIGHTHS", "SILENT" }, { "KICK ON ONE AND THE A OF TWO, TAMBOURINE", "BACKBEAT, CRASHES TO END" }, {} },
                { "TWO-CHORD VAMP, OCTAVE BASS", { "A STAB ON FOUR EVERY OTHER BAR", "RUNG CHORDS, A STAB ON FOUR" }, { "OCTAVES IN A SYNCOPATED FIGURE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "BACKBEAT, A SNARE PICKUP EVERY OTHER BAR", "RIDE BACKBEAT, A SNARE PICKUP" }, {} },
                { "BOLERO CRESCENDO", { "THE DOTTED OSTINATO, GROWING", "THE OSTINATO, CHORDS FROM HALFWAY" }, { "THE DOTTED OSTINATO", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "IN AT A QUARTER, A HIGH LINE FROM HALFWAY", "SILENT" }, { "NO BACKBEAT: FLOOR TOM AND TAMBOURINE ON THE OSTINATO, GROWING", "THE OSTINATO ON CONGAS, A ROLL EVERY FOUR" }, {} },
                { "TONIC PEDAL, ONE CHORD HELD", { "ONE LONG NOTE EVERY FOUR BARS", "THE TONIC CHORD RUNG EVERY TWO BARS" }, { "TONIC EIGHTHS, BEATS ACCENTED", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "THE TONIC CHORD HELD THROUGHOUT", "THE TONIC CHORD, A SECOND VOICE STEPPING" }, { "FULL GROOVE, A SNARE BUILD IN THE LAST BAR", "FULL GROOVE, CRASH EVERY EIGHT" }, {} },
                { "HITS BAR, EIGHTHS BAR OVER A PEDAL", { "HITS ON ONE, THREE, FOUR AND FOUR-AND, THEN EIGHTHS", "HITS ON ONE, THREE, FOUR AND FOUR-AND" }, { "THE TONIC PEDAL UNDER THE HITS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "BACKBEAT UNDER THE HITS, OPEN HAT ON FOUR-AND", "THE KICK WITH THE HITS" }, {} },
                { "DRUMS AND VOICES BREAKDOWN", { "OUT", "ONE CHORD ON THE FIRST BAR" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "ONE HELD PAD" }, { "THE GROOVE ALONE, CLAPS ON TWO AND FOUR", "THE GROOVE ALONE, FLOOR TOM ON THE BEATS" }, {} },
                { "SUNG CHORUS OVER TOMS", { "OUT", "BACK FOR THE LAST BAR" }, { "OUT, THEN ROOTS ON ONE AND THREE FROM HALFWAY", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "TOMS, THEN KIT AND COWBELL, A SNARE BAR", "TOMS THROUGHOUT" }, {} },
                { "HIT, THEN THE HOOK ALONE", { "A HIT HELD, THEN OUT", "A HIT HELD EVERY FOUR BARS" }, { "THE HIT HELD WITH THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "THE HOOK ALONE AFTER THE HIT", "THE HOOK, AN OCTAVE UP" }, { "A HIT AND A CRASH, THEN OUT", "A HIT EVERY FOUR BARS, HATS BETWEEN" }, {} },
                // (0.241.0) Round ten's choruses.
                { "POWER BALLAD CHORUS", { "A FULL CHORD RUNG EACH BAR", "CHORDS ON ONE AND THREE, THE LAST BAR SPLIT" }, { "ONE, TWO-AND, THREE, A WALK ON FOUR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "ORGAN TRIAD HELD, A VOICE MOVING ON THREE", "PIANO BROKEN TRIAD UNDER THE CHORUS" }, { "BALLAD CHORUS: KICK ONE, TWO-AND, THREE", "BALLAD CHORUS ON THE RIDE" }, {},
                  { "OPEN EIGHTH STRUMS UNDER THE BALLAD", "BIG STRUMS ON ONE AND THREE UNDER THE BALLAD" } },
                { "THE FOURTH BAR HIT ON TWO, THREE, FOUR", { "RUNG CHORDS, THE FOURTH BAR HIT ON TWO, THREE, FOUR", "EIGHTHS, THE FOURTH BAR HIT ON TWO, THREE, FOUR" }, { "ROOT EIGHTHS, THE FOURTH BAR'S HITS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "BACKBEAT, THE FOURTH BAR'S HITS", "HALF TIME, THE FOURTH BAR'S HITS" }, {} },
                { "ANTHEM PUSH CHORUS", { "CHORD ON ONE, HITS ON FOUR AND FOUR-AND", "THE PUSHES WITH A HIGH CHORD OVER THEM" }, { "ROOT EIGHTHS, LOCKED ON FOUR AND FOUR-AND", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PAD HELD, BRASS ANSWERS IN THE SECOND HALF", "PAD HELD UNDER THE PUSHES" }, { "ANTHEM BEAT, OPEN HAT EIGHTHS", "ANTHEM BEAT, KICK ON THREE-AND EVERY OTHER BAR" }, {} },
                { "FIRST CHORUS UNPLUGGED", { "OUT", "ONE CLEAN CHORD RUNG A BAR" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRINGS HOLD THE CHORDS", "SILENT" }, { "OUT", "A TAMBOURINE ON TWO AND FOUR" }, {},
                  { "FULL STRUMS ON THE BEATS, TRIADS ON THE ANDS", "FULL STRUMS ON THE BEATS" } },
                { "HALF-TIME BALLAD CHORUS, PUSH ON FOUR", { "POWER CHORD ON ONE, A PUSH ON FOUR", "POWER CHORD ON ONE, THE SECOND BAR ON THREE" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "TWO-BAR HALF-TIME KIT, PEDAL HAT QUARTERS", "TWO-BAR HALF-TIME KIT, OPEN HAT ON FOUR" }, {} },
                { "HOOK HALF, THEN FOUR ON THE FLOOR", { "RUNG CHORDS, THEN EIGHTHS FROM HALFWAY", "EIGHTHS THROUGHOUT, OPEN FROM HALFWAY" }, { "ROOTS HELD, THEN EIGHTHS FROM HALFWAY", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "BACKBEAT, THEN FOUR ON THE FLOOR FROM HALFWAY", "HALF TIME, THEN FOUR ON THE FLOOR FROM HALFWAY" }, {} },
                { "CHANT BARS", { "QUARTER HITS EVERY OTHER BAR, RUNG BETWEEN", "QUARTER HITS EVERY BAR" }, { "THE QUARTER HITS WITH THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND SNARE ON THE QUARTERS, CLAPS ON TWO AND FOUR", "STOMP: KICK ON ONE AND THREE, SNARE AND CLAP ON TWO AND FOUR" }, {} },
                { "BASS DROPS OUT FOR THE CHORUS", { "OUT", "A HIGH RUNG CHORD EVERY TWO BARS" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRINGS HELD EVERY TWO BARS", "STRINGS HELD, PIANO ON ONE" }, { "THE VERSE'S LOOP WITH WOODBLOCK SIXTEENTHS", "THE VERSE'S LOOP" }, {} },
                { "ORCHESTRAL PULSE, NO SNARE", { "A LOW HELD LINE, A NOTE A BAR", "OUT" }, { "THE ORCHESTRAL PULSE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRINGS STAB ONE, TWO-AND, THREE, A GLISSANDO INTO EACH BAR", "STRING STABS ON ONE, TWO-AND, THREE" }, { "KICK ON THE EIGHTHS, PEDAL HAT AND RIDE, NO SNARE", "CYMBALS ONLY, NO KICK OR SNARE" }, {} },
                { "TWO-BAR STAB CHORUS", { "STABS ON ONE AND TWO-AND, THEN ONE AND THREE", "THE STABS, THE LAST CHORD PUSHED ON FOUR-AND" }, { "ROOTS ON THE STABS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PIANO EIGHTH TRIADS UNDER THE STABS", "HELD PADS" }, { "RIDE BELL QUARTERS, KICK INTO EVERY OTHER BAR", "RIDE BELL QUARTERS, A THIRD SNARE EVERY FOURTH BAR" }, {} },
                { "BALLAD CHORUS, MUTED SIXTEENTHS THROUGH THE CHORD", { "MUTED SIXTEENTH RUNS THROUGH THE CHORD", "MUTED RUNS, A SECOND GUITAR RINGING" }, { "ROOT EIGHTHS WITH CHROMATIC APPROACHES", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "KEYS ON TWO, TWO-AND, THREE", "ELECTRIC PIANO QUARTERS" }, { "BALLAD GROOVE, KICK ON ONE AND THE A OF FOUR", "BALLAD GROOVE, A TOM PICKUP AT THE PHRASE ENDS" }, {} },
                { "HELD CHORDS UNDER PIANO TRIADS", { "HELD CHORD, PUSHES ON THREE-AND AND FOUR-AND", "HELD CHORD, ONE PUSH ON FOUR-AND" }, { "EIGHTHS WITH BEAT THREE EMPTY", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PIANO EIGHTH TRIADS UNDER THE PUSHES", "HELD PADS" }, { "OPEN HAT ON TWO, THREE AND FOUR", "BALLAD RIDE ON THE ANDS" }, {} },
                // (0.257.0) Round eleven's intros and outros.
                { "THE RIFF, A KICK ON ONE, THEN EVERYONE", { "MUTED RIFF ALL THROUGH", "MUTED RIFF, A HIGH NOTE HELD TO END" }, { "OUT, THEN THE ROOT ON ONE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT, THEN A KICK ON ONE", "OUT, THEN A KICK AND CRASH ON ONE" }, {} },
                { "ONE BAND HIT, THEN THE ARPEGGIO ALONE", { "A HIT, THEN A SIXTEENTH ARPEGGIO IN TWO OCTAVES", "A HIT, THEN THE ARPEGGIO, ONE OCTAVE" }, { "A HIT, A LONE ROOT, THEN OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "A HIT, KICK AND CRASH ON ONE, THEN OUT", "A HIT, THEN OUT" }, {} },
                { "SLOW CHROMATIC DESCENT, HIGH DYAD PEDAL OVER", { "OUT, THEN POWER CHORDS ON THE FALLING ROOTS", "POWER CHORDS ON THE FALLING ROOTS" }, { "ROOTS FALLING BY HALF STEPS, A BAR THEN TWO HALVES", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "A HIGH ROOT-AND-FIFTH DYAD ON THE EIGHTHS IN THE LAST QUARTER", "SILENT" }, { "KICK BAR, SNARE BAR, RIDE EIGHTHS", "KICK BAR, SNARE BAR, OPEN HAT SIXTEENTHS FROM HALFWAY" }, {} },
                { "PICKED SIXTEENTH FIGURE UNDER A HIGH DYAD STAB", { "A HIGH STAB, THEN A PICKED SIXTEENTH FIGURE", "THE PICKED FIGURE, A SECOND GUITAR ON THE STAB" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT", "CLOSED HATS ON THE SIXTEENTHS" }, {} },
                { "KEYS CHORD EIGHTHS ALONE, THEN THE PICKUP", { "OUT", "ONE HELD NOTE A BAR" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "CHORD EIGHTHS, THE BASS NOTE FALLING", "CHORD EIGHTHS, A CHORD A BAR" }, { "OUT BUT A PICKUP ON THREE-AND, FOUR AND FOUR-AND", "OUT BUT A SNARE ROLL IN THE LAST BEAT" }, {} },
                { "SNARE ON EVERY CHUG, THEN THE KICK", { "ROOT CHUGS, A CHORD ON FOUR-AND", "ROOT CHUGS, CHORDS FROM TWO TO THREE-AND" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE ON EVERY GUITAR HIT, THE KICK FROM HALFWAY", "SNARE ON EVERY GUITAR HIT ALONE" }, {} },
                { "THE RIFF ALONE, QUARTER HITS, A TOM FILL", { "THE RIFF TWO BARS, THEN QUARTER HITS", "THE RIFF ALL THROUGH" }, { "IN ON THE THIRD BAR OF EACH FOUR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT, QUARTER HITS, THEN A SIXTEENTH TOM FILL", "OUT, THEN QUARTER HITS" }, {} },
                { "TWO-SIXTEENTH STAB OVER HAND PERCUSSION", { "TWO SIXTEENTHS ON ONE, A CHORD EVERY FIFTH BAR", "TWO SIXTEENTHS ON ONE, THEN SILENCE" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "CONGA SIXTEENTHS, KICK AND CRASH ON THE STAB", "CONGA SIXTEENTHS ALONE" }, {} },
                { "HELD CHORD OVER HAT SIXTEENTHS, THEN THE RIFF ALONE", { "A HELD CHORD, THEN THE RIFF FROM HALFWAY", "THE RIFF ALONE ALL THROUGH" }, { "OUT, THEN THE RIFF'S ROOTS IN THE LAST BAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HAT SIXTEENTHS, THEN OPEN HAT EIGHTHS AND A PICKUP", "HAT SIXTEENTHS ALL THROUGH" }, {} },
                { "HIGH NOTE IN THREES AND A FOUR", { "A HIGH NOTE ON 3+3+3+3+4, HELD EVERY FOURTH BAR", "A HIGH NOTE ON 3+3+3+3+4" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT", "CLOSED HATS ON THE PULSE" }, {} },
                { "INNER LINE OVER A HELD ROOT, FOUR ON THE FLOOR", { "A HELD ROOT, A SECOND NOTE MOVING IN HALF NOTES", "A HELD ROOT, THE INNER LINE IN QUARTERS" }, { "ROOT QUARTERS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ONE, TWO, THREE, THREE-A, OPEN HATS ON THE BEATS", "FOUR ON THE FLOOR, OPEN HATS, CRASH EVERY TWO BARS" }, {} },
                { "PAD BAR, PIANO OCTAVE ANSWER BAR", { "OUT", "ONE HELD NOTE A BAR" }, { "A LOW ROOT ON ONE OF THE PAD BARS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "A SUS PAD BAR, THEN A FALLING PIANO OCTAVE BAR", "THE SUS PAD EVERY BAR" }, { "PEDAL HAT ON TWO AND FOUR", "OUT" }, {} },
                { "BRASS HOOK OVER HELD STRINGS, NO DRUMS", { "OUT", "ONE HELD NOTE A BAR" }, { "A PEDAL A BAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "A HOOK OVER HELD STRINGS", "HELD PADS" }, { "OUT, THEN A TOM PICKUP IN THE LAST TWO BARS", "OUT UNTIL THE LAST BAR'S FILL" }, {} },
                { "WHOLE-BAR CHORDS OVER A BASS ON ONE, THREE AND FOUR", { "A CHORD A BAR, POWER CHORDS ON ONE, THREE AND FOUR", "A CHORD A BAR, ONE HIT TO END" }, { "ROOTS ON ONE, THREE AND FOUR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "HALF TIME, KICK ONE AND TWO, CRASH QUARTERS", "HALF TIME, RIDE ON THREE AND FOUR" }, {} },
                { "EIGHTHS ON ONE AND FIVE IN TURN", { "THE TONIC'S EIGHTHS, THEN THE FIFTH'S, IN TURN", "THE TONIC'S EIGHTHS, THEN THE FIFTH'S, THE LAST BAR RUNG" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "LONG HIGH NOTES ON THE TONIC AND THIRD", "SILENT" }, { "KICK AND SNARE QUARTERS", "OUT" }, {} },
                { "THE RIFF, THREE HITS AND OUT", { "THE RIFF, THEN HITS ON TWO, TWO-AND AND THREE", "THE RIFF, THEN ONE HIT ON TWO" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK QUARTERS, THE LAST BAR'S THREE HITS", "BACKBEAT, THE LAST BAR'S THREE HITS" }, {} },
                { "GUITAR ALONE, THEN THREE HITS RUNG TWICE", { "ALONE, THREE HITS RUNG TWICE, THE LAST CHORD HELD", "ALONE, THEN THE LAST CHORD HELD" }, { "THE THREE HITS, THEN THE ROOT HELD", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND CRASH ON THE THREE HITS", "OUT UNTIL THE LAST BAR'S FILL" }, {} },
                { "OCTAVE TREMOLO CREEPING UP", { "OCTAVE SIXTEENTHS UP A HALF STEP EVERY TWO BEATS", "OCTAVE SIXTEENTHS UP A HALF STEP EVERY BAR" }, { "THE CREEP AN OCTAVE UNDER", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK SIXTEENTHS, CRASH QUARTERS, BACKBEAT", "KICK SIXTEENTHS, BACKBEAT, RIDE" }, {} },
                { "SNARE QUARTERS, KICK ON THREE-AND AND FOUR-AND", { "OUT", "TWO PICKED NOTES A BAR" }, { "SLAP FIGURE, ROOT TO OCTAVE AND BACK", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "A SIXTEENTH ARPEGGIO, ROOT, FIFTH AND OCTAVE", "HELD PADS" }, { "SNARE QUARTERS, KICK THREE-AND AND FOUR-AND, HATS ON THE ANDS", "SNARE QUARTERS, KICK THREE-AND AND FOUR-AND, RIDE" }, {} },
                // (0.256.0) Round eleven's breakdowns.
                { "SNARE PAIRS ON THE HALF-STEP CELL", { "HALF-STEP CELL IN THREES", "HALF-STEP CELL, NO RUN" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE PAIRS EVERY SIX SIXTEENTHS, KICK BETWEEN", "SNARE PAIRS, NO HATS" }, {} },
                { "UNISON STAB BAR, BLAST BAR, IN TURN", { "STABS, THEN THE CHORDS AS SIXTEENTH SINGLES", "UNISON STABS EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "STABS WITH KICK, CRASH AND CHINA, THEN A BLAST BAR", "STABS WITH KICK AND CRASH, A BACKBEAT BAR" }, {} },
                { "ONE CHORD RUNG FOUR BARS, SYNCOPATED HALF TIME", { "ONE CHORD STRUCK AND RUNG FOUR BARS", "ONE CHORD RUNG TWO BARS" }, { "ROOT PUSHES WITH THE FLAT THREE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF-TIME SYNCOPATED KICK, RIDE QUARTERS", "HALF-TIME SYNCOPATED KICK, CLOSED HATS" }, {} },
                { "KICK ONE BAR, SNARE THE NEXT, CRASH QUARTERS", { "QUARTER POWER CHORDS, A CHORD A BAR", "QUARTER POWER CHORDS, EVERY FOURTH BAR HELD" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON ONE, THEN SNARE ON ONE, CRASH QUARTERS", "KICK AND SNARE IN TURN, RIDE QUARTERS" }, {} },
                { "DOTTED SNARE OVER SIXTEENTH KICKS", { "TREMOLO ROOT WITH THE FLAT SEVEN, A STUTTER CELL EVERY FOURTH PAIR", "TREMOLO ROOT WITH THE FLAT SEVEN" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "A LOW STRING CHORD ON ONE AND TWO-AND", "SILENT" }, { "SIXTEENTH KICKS, SNARE ON ONE, TWO-AND AND FOUR", "SIXTEENTH KICKS, SNARE ON ONE, TWO-AND AND FOUR, OPEN HATS" }, {} },
                { "FOUR CHUGS, THEN SIX BEATS RUNG", { "FOUR CHUGS, THE THIRD ON THE FLAT TWO, THEN RUNG", "FOUR CHUGS ON THE ROOT, THEN RUNG" }, { "THE CHUGS, THEN THE ROOT HELD", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND CRASH ON THE CHUGS, THEN KICK QUARTERS UNDER OPEN HATS", "KICK AND CRASH ON THE CHUGS, THEN A HALF-TIME BACKBEAT" }, {} },
                { "TOMS AND KICK ON THE HITS, NO CYMBALS", { "MUTED HITS ON ONE, TWO-A, THREE, THREE-AND", "MUTED HITS, A PUSH ON FOUR-AND EVERY OTHER BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND FLOOR TOM ON THE HITS, NOTHING ELSE", "KICK AND FLOOR TOM ON THE HITS, A TOM PAIR TO END" }, {} },
                { "BAND HITS ON SNARE AND CRASH, NO KICK", { "SUS AND ADD-NINE HITS IN A TWO-BAR FIGURE", "POWER-CHORD HITS IN A TWO-BAR FIGURE" }, { "THE TWO-BAR FIGURE'S ROOTS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "CHORD HITS WITH THE BAND", "SILENT" }, { "SNARE AND CRASH ON THE HITS, NO KICK", "SNARE AND CRASH ON THE HITS, A SNARE SWELL TO END" }, {} },
                { "DRUMS OUT, DOTTED SYNTH BASS AND A HIGH THIRD", { "OUT, THEN THE SIXTEENTH CHUG FROM HALFWAY", "OUT" }, { "DOTTED OCTAVE RIFF ALONE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HIGH ROOT AND THIRD ON ONE AND ONE-A, AN ANSWER EVERY OTHER BAR", "HIGH ROOT AND THIRD ON ONE AND ONE-A" }, { "OUT, THEN FOUR ON THE FLOOR FROM HALFWAY", "OUT UNTIL THE LAST BAR'S FILL" }, {} },
                { "SYNCOPATED CHUG, GHOST SNARES", { "HELD CHORDS, THEN ROOT CHUGS FROM ONE-AND", "ROOT CHUGS FROM ONE-AND EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SYNCOPATED KICK, SNARE ON THREE WITH GHOSTS", "SYNCOPATED KICK, SNARE ON THREE, NO GHOSTS" }, {} },
                { "FOUR-SIXTEENTH BURST ON ONE, THEN SILENCE", { "THE BURST, PUSHED CHORDS EVERY FOURTH BAR", "THE BURST EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK WITH THE BURST, SNARE ON TWO, OPEN HATS ON ONE AND TWO", "KICK WITH THE BURST, SNARE ON TWO" }, {} },
                { "HOOK OVER HELD ROOTS, KICK ON ONE AND FOUR-AND", { "ROOT ON ONE, FIFTH ON FOUR-AND", "OUT" }, { "ROOT ON ONE, FIFTH ON FOUR-AND, HELD", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "A HOOK OVER HELD STRINGS", "HELD PADS" }, { "KICK ON ONE AND FOUR-AND, OPEN HATS, BACKBEAT IN THE SECOND HALF", "KICK ON ONE AND FOUR-AND, OPEN HATS" }, {} },
                // (0.255.0) Round eleven's bridges.
                { "HELD CLUSTERS, RIDE ROLLS ONLY", { "THREE-NOTE CLUSTERS HELD", "CLUSTERS HELD TWO BARS" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIDE ROLLS SWELLING, NO KICK OR SNARE", "RIDE ROLLS, THEN THE KIT BUILDS BACK" }, {} },
                { "RUNG SEVENTHS, SNARE ON ONE ONLY", { "A SEVENTH CHORD RUNG TWO BARS, AN ARPEGGIO UP IN THE FOURTH", "A SEVENTH CHORD RUNG TWO BARS" }, { "WITH THE KICK ON THREE AND FOUR-AND", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE ON ONE, KICK THREE AND FOUR-AND, TOMS EVERY FOURTH BAR", "SNARE ON ONE, KICK THREE AND FOUR-AND, RIDE QUARTERS" }, {} },
                { "DOTTED SINGLE NOTES OVER THE ARPEGGIO", { "HIGH CHORD TONES ON A DOTTED QUARTER, THEN QUARTERS", "OUT" }, { "ROOTS HELD", "ROOT EIGHTHS", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "THE VERSE GROOVE: KICK ONE, ONE-AND, TWO-A, THREE-AND", "THE VERSE GROOVE UNDER THE RIDE" }, {}, { "A RINGING SIXTEENTH ARPEGGIO" } },
                { "HIGH AND LOW TRIADS IN SIXTEENTHS", { "HIGH TRIAD, THEN LOW TRIAD TWICE, IN SIXTEENTHS", "HIGH AND LOW TRIADS, A ONE-NOTE OCTAVE LEAD FROM HALFWAY" }, { "DOTTED QUARTERS, A SLOW LINE BETWEEN ROOTS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK QUARTERS, SIXTEENTH HATS, NO SNARE, A ROLL AT THE END", "KICK QUARTERS, SIXTEENTH HATS, NO SNARE" }, {} },
                { "QUARTER CHORDS ROCKING BY HALF STEPS", { "QUARTER POWER CHORDS ROCKING BY HALF STEPS", "QUARTER POWER CHORDS ON THE ROOT" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE ON EVERY BEAT, KICK BETWEEN, DOUBLED IN BARS THREE AND FOUR", "SNARE ON EVERY BEAT, KICK BETWEEN, CRASH QUARTERS" }, {} },
                { "ROOT, FIFTH, FOURTH EIGHTH CELL, HALF TIME", { "ROOT, FIFTH, FOURTH CELL, A STRUMMED BAR EVERY OTHER", "THE ROOT, FIFTH, FOURTH CELL EVERY BAR" }, { "ROOTS HELD", "ROOT EIGHTHS", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "HALF TIME, KICK ONE, TWO-AND AND FOUR-AND, QUARTER HATS", "HALF TIME, KICK ONE, TWO-AND AND FOUR-AND, A SNARE BAR TO END" }, {} },
                { "3+3+2 CHUG UNDER A FALLING OCTAVE LINE", { "ROOT AND FLAT TWO ON THE 3+3+2, A FALLING OCTAVE LINE FROM HALFWAY", "ROOT AND FLAT TWO ON THE 3+3+2" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "3+3+2 KICK, SNARE ON THREE, RIDE, THEN OPEN SIXTEENTHS", "3+3+2 KICK, SNARE ON THREE, RIDE" }, {} },
                { "OPEN ADD-NINE ARPEGGIO, DOTTED CRASHES", { "OPEN ADD-NINE SHAPES PICKED IN EIGHTHS", "ADD-NINE SHAPES PICKED IN QUARTERS" }, { "ROOTS ON ONE, TWO-AND AND FOUR, THEN WHOLE NOTES", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "CRASH ON ONE, TWO-AND AND FOUR, QUARTER HATS", "RIDE EIGHTHS, KICK ON ONE, TWO-AND AND FOUR" }, {} },
                { "NO DOWNBEAT, SNARE RUN INTO FOUR", { "OCTAVE PAIRS FALLING A STEP EVERY THREE SIXTEENTHS", "THE FALLING OCTAVES, A SECOND GUITAR OFF THE BEAT" }, { "ROOTS ON ONE, TWO AND THREE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "A BROKEN PIANO CHORD ON ONE, ONE-AND AND FOUR-AND", "SILENT" }, { "KICK ON TWO AND THREE, CRASH ON TWO, A SNARE RUN INTO FOUR", "KICK ON TWO AND THREE, CLOSED EIGHTHS" }, {} },
                { "BASS WALK, THEN TOMS, THE KIT, A TREMOLO WALL", { "OUT, SPARSE NOTES, PICKED DYADS, THEN A TREMOLO WALL", "OUT UNTIL THE LAST QUARTER, THEN A TREMOLO WALL" }, { "A TWO-CHORD PAIR IN EIGHTHS, THEN TREMOLO", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT, FLOOR TOM EIGHTHS, THE KICK, THEN THE WALL", "FLOOR TOM EIGHTHS, THEN THE KICK" }, {} },
                { "CLEAN RIFF, KICK EVERY THREE BEATS", { "THE CHORDS PICKED QUIETLY", "OUT" }, { "OUT, THEN WHOLE NOTES", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "KICK EVERY THREE BEATS, QUARTER HATS, A HIT AT THE END", "KICK EVERY THREE BEATS, QUARTER HATS" }, {} },
                { "PIANO ROCKING, A UNISON CLIMB IN EACH BAR'S SECOND HALF", { "OUT", "ONE HELD NOTE A BAR" }, { "ROOT HELD, THEN A CLIMB INTO THE NEXT CHORD", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PIANO ROCKING: THE LOW ROOT ON THE BEATS, THE CHORD ON THE ANDS", "PIANO ROCKING, STRINGS CLIMBING WITH THE BASS" }, { "BALLAD KICK, AN OPEN HAT EVERY OTHER BAR", "BALLAD KICK UNDER THE RIDE" }, {} },
                { "BASS RIFF OVER KICK QUARTERS, THEN TOMS, THEN THE ROLL", { "OUT, ONE CHORD RUNG IN THE LAST BAR", "OUT" }, { "A WALKING RIFF, TWO NOTES A BEAT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK QUARTERS, THEN TOMS, THEN A SNARE ROLL", "KICK QUARTERS, THEN TOMS" }, {} },
                { "RIDE GALLOP ALONE OVER QUARTER CHORDS", { "QUARTER POWER CHORDS STEPPING DOWN, TWO BARS A CHORD", "QUARTER POWER CHORDS, A CHORD A BAR" }, { "ROOT QUARTERS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "LONG TONES RISING AND FALLING BY STEP", "SILENT" }, { "RIDE GALLOP ALONE, A CRASH AND KICK TO START", "RIDE GALLOP ALONE" }, {} },
                { "THREE PUSHED HITS, A BAR OF RIDE", { "THREE PUSHED HITS, THEN A BAR OUT", "THREE PUSHED HITS EVERY BAR" }, { "THE HITS' ROOTS HELD", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND CRASH ON THE HITS, THEN A BAR OF RIDE", "KICK AND CRASH ON THE HITS, RIDE BETWEEN" }, {} },
                { "CLEAN EIGHTH ARPEGGIO, SNARE ON TWO, THREE-AND, FOUR, FOUR-AND", { "CLEAN ROOT, FIFTH, OCTAVE EIGHTHS", "CLEAN EIGHTHS, A FIFTH DYAD HELD FROM HALFWAY" }, { "OUT, THEN ROOTS FROM HALFWAY", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE ON TWO, THREE-AND, FOUR AND FOUR-AND, NO HATS", "THE SAME KICK, BACKBEAT, NO HATS" }, {} },
                { "SHARP-FOUR ARPEGGIO, A HELD CHORD BAR", { "A SHARP-FOUR ARPEGGIO FALLING, THEN THE TONIC HELD", "THE ARPEGGIO EVERY BAR" }, { "THE TONIC ON THE CHORD BARS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIDE QUARTERS, KICK ON ONE, NO SNARE", "RIDE EIGHTHS, KICK ON ONE, NO SNARE" }, {} },
                { "HELD POWER CHORD, OCTAVE PICKUP INTO EACH BAR", { "A POWER CHORD HELD, AN OCTAVE PAIR INTO THE NEXT", "A POWER CHORD HELD A BAR" }, { "ROOT EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ONE, TWO-A, THREE, THREE-AND, OPEN HAT QUARTERS", "KICK ONE, TWO-A, THREE, THREE-AND, CLOSED HATS, CRASH EVERY FOUR" }, {} },
                { "KIT IN EIGHTHS, THEN CRASH HITS AND TOM BARS", { "ROOT EIGHTHS, THEN HITS AND A LONE NOTE", "ROOT EIGHTHS ALL THROUGH" }, { "ROOT EIGHTHS, THEN HELD AND A WALK", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK, SNARE AND FLOOR TOM ON EVERY EIGHTH, THEN CRASH HITS AND TOM BARS", "KICK, SNARE AND FLOOR TOM ON EVERY EIGHTH, ALL THROUGH" }, {} },
                { "SNARE ON TWO ONLY, TOM PAIR ON THREE-AND AND FOUR", { "CLEAN CHORDS PUSHED, THE NEXT ONE EMPTY", "POWER CHORDS PUSHED, THE NEXT ONE EMPTY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SNARE ON TWO ONLY, A TOM PAIR, HAT EIGHTHS", "SNARE ON TWO ONLY, HAT EIGHTHS" }, {} },
                { "EVERYTHING ON THREE-AND AND FOUR-AND", { "OUT", "A CHORD ON THREE-AND AND FOUR-AND" }, { "THREE-AND ON THE ROOT, FOUR-AND ON THE NEXT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRINGS STRUCK ON FOUR-AND, TIED OVER", "SILENT" }, { "KICK ON THREE-AND AND FOUR-AND, SNARE ON TWO, SIXTEENTH HATS", "KICK ON THREE-AND AND FOUR-AND, SNARE ON TWO, RIDE" }, {} },
                { "A NOTE CLIMBING A STEP A BEAT OVER A HELD OCTAVE", { "A HELD OCTAVE, A SECOND GUITAR CLIMBING A STEP A BEAT", "THE CLIMB ALONE" }, { "ONE, TWO-A, THREE-AND AND FOUR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ONE, TWO-A, THREE-AND, RIDE EIGHTHS", "KICK ONE, TWO-A, THREE-AND, CLOSED EIGHTHS" }, {} },
                // (0.254.0) Round eleven's pre-choruses and post-choruses / interludes.
                { "BASS AND GUITAR TRADE BEATS IN SIXTEENTHS", { "HIGH THIRD IN SIXTEENTHS ON TWO AND FOUR", "HIGH THIRD ON TWO AND FOUR, A LOW STOP AT THE END" }, { "ROOT SIXTEENTHS ON ONE AND THREE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HATS AND RIDE, THEN KICK, SNARE PICKUPS AND CRASH EIGHTHS", "CRASH EIGHTHS, KICK AND SNARE PICKUPS ALL THROUGH" }, {} },
                { "UNISON HITS ON ONE, ONE-AND, TWO, THREE, THREE-AND, FOUR-AND", { "HELD UNISON HITS", "UNISON HITS, SHORT" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND CRASH WITH THE HITS, TOM EIGHTHS TO END", "KICK WITH THE UNISON HITS, BACKBEAT" }, {} },
                { "HELD HIGH NOTE OVER THE BASS CLIMB", { "ONE FIFTH DYAD HELD, A HIGH ANSWER EVERY OTHER BAR", "ONE FIFTH DYAD HELD" }, { "SIXTEENTHS CLIMBING BY HALF STEPS OVER TWO BARS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK IN THREES, CRASH EVERY BAR, A SIXTEENTH BUILD AT THE END", "KICK IN THREES, BACKBEAT, HAT EIGHTHS" }, {} },
                { "MUTED CHORD EIGHTHS, THE CHANGE ON FOUR", { "MUTED EIGHTHS, THE NEXT CHORD ON FOUR", "MUTED EIGHTHS ON THE BAR'S CHORD" }, { "ROOT HELD, THE NEXT ON FOUR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "FOUR NOTES ON DOTTED EIGHTHS", "SILENT" }, { "KICK ONE AND TWO-A, SIXTEENTH HATS WITH A HOLE", "KICK ONE AND TWO-A, CLOSED EIGHTHS" }, {} },
                { "ADD-NINE CHORDS HELD, SOFT CRASH QUARTERS", { "OPEN ADD-NINE CHORDS HELD, A CHORD EVERY TWO BARS", "ADD-NINE CHORDS HELD A BAR" }, { "ROOTS HELD", "ROOT QUARTERS", "LOCKED WITH THE KICK" },
                  { "ADD-NINE PADS", "SILENT" }, { "HALF TIME, SOFT CRASH QUARTERS", "HALF TIME, RIDE QUARTERS, SNARE ON THREE" }, {} },
                { "SNARE ON EVERY BEAT, KICK ON THE ANDS, HELD CHORDS", { "BIG CHORDS HELD TWO BARS, A ROOT RUN ON FOUR", "BIG CHORDS HELD TWO BARS" }, { "ROOTS HELD", "ROOT EIGHTHS", "LOCKED WITH THE KICK" },
                  { "A PAD EVERY TWO BARS", "SILENT" }, { "SNARE ON EVERY BEAT, KICK ON THE ANDS, OPEN HAT QUARTERS", "SNARE ON EVERY BEAT, KICK ON THE ANDS, RIDE" }, {} },
                { "SNARE QUARTERS ALONE UNDER A RISING HALF-STEP PAD", { "ONE CHORD RUNG, OUT, THEN CHUGS AT THE END", "OUT" }, { "DOTTED LINE: ROOT, OCTAVE, FLAT SEVEN, FIVE, FOUR, FIVE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "A PAD NOTE RISING A HALF STEP A BAR", "SILENT" }, { "KICK AND SNARE QUARTERS, THEN SNARE QUARTERS ALONE, THEN THE BEAT", "SNARE QUARTERS ALONE" }, {} },
                { "THE BURSTS STAY, KICK ON ONE, THREE, THREE-AND", { "BIG DISTORTED STRUMS WITH THE ACOUSTIC", "OUT" }, { "ROOTS ON ONE AND TWO, THEN EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SIDESTICK BURSTS, KICK ONE, THREE AND THREE-AND", "KICK ONE, THREE AND THREE-AND, SIXTEENTH HATS" }, {}, { "BIG STRUMS IN A PUSHED FIGURE" } },
                { "POWER-CHORD SIXTEENTHS UP TO THE FLAT FIVE AND BACK", { "SIXTEENTHS UP TO THE FLAT FIVE AND BACK", "SIXTEENTHS UP TO THE FOUR AND BACK" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK SIXTEENTHS ON ONE AND TWO, CRASH QUARTERS, A STOP AT THE END", "KICK SIXTEENTHS ON ONE AND TWO, BACKBEAT" }, {} },
                { "FLAT-TWO HITS, A TOM RUN UNDER THE SIXTEENTHS", { "FLAT-TWO HITS, THEN SIXTEENTHS", "FLAT-TWO HITS, SIXTEENTHS TO THE END EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "TOM RUN UNDER THE SIXTEENTHS, OPEN EIGHTHS", "THE PUSHED KICK, OPEN EIGHTHS, NO TOMS" }, {} },
                { "HALF-BAR CHANGES, THEN TWO BARS HELD OVER THE GALLOP", { "GALLOP, HALF-BAR CHANGES, THEN THE FIVE HELD", "GALLOP, A CHORD A BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRINGS FOLLOW, THEN HOLD", "HELD PADS" }, { "3+3+2 KICK, CRASH EVERY BAR, A SNARE PICKUP AT THE END", "3+3+2 KICK, BACKBEAT" }, {} },
                { "TWO BARS OF OCTAVE TREMOLO ON THE FLAT SIX", { "OCTAVE TREMOLO ON THE FLAT SIX", "OCTAVE TREMOLO ON THE FLAT THREE" }, { "SIXTEENTHS ON THE SAME NOTE, THE OCTAVE AT THE END", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "ONE HIGH NOTE", "SILENT" }, { "KICK ONE, TWO-A, THREE-AND, BACKBEAT, CRASH ON ONE", "KICK ONE, TWO-A, THREE-AND, A SNARE BUILD" }, {} },
                { "HITS ON ONE, TWO, TWO-AND, FOUR, FOUR-AND, CRASH QUARTERS", { "HEAVY HITS HELD TO THE NEXT", "HEAVY HITS, SHORT" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK WITH THE HITS, SNARE ON THREE, CRASH QUARTERS", "KICK WITH THE HITS, SNARE ON THREE, RIDE" }, {} },
                { "LOW AND HIGH IN 3+3+2, UP A HALF STEP", { "LOW, HIGH, LOW IN 3+3+2, UP A HALF STEP EACH HALF BAR", "LOW AND HIGH IN 3+3+2 ON THE ROOT" }, { "THE LOW HITS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "CRASH ON EVERY HIT, SNARE A SIXTEENTH EARLY", "KICK ONE, TWO-AND, THREE, FOUR-AND, SNARE EARLY" }, {} },
                { "PICKED MELODY FALLING BY HALF STEPS", { "OUT", "A HIGH NOTE HELD EVERY FOUR BARS" }, { "ROOTS HELD", "ROOT EIGHTHS", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SPLIT HATS AND RIDE, A SNARE PICKUP EVERY EIGHT", "SIDESTICK BALLAD, RIDE ON THE ANDS" }, {}, { "A LINE FALLING BY HALF STEPS IN PAIRS" } },
                { "KEYS CHORD EIGHTHS OVER A FALLING BASS, BAND OUT", { "OUT", "ONE CHORD RUNG EVERY FOUR BARS" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "CHORD EIGHTHS, THE BASS NOTE FALLING UNDER", "CHORD EIGHTHS" }, { "NOTHING, A SNARE PICKUP AT THE END", "A SOFT KICK ON ONE" }, {} },
                { "HALF-NOTE DYADS, AN ORGAN ANSWER", { "ROOT-FIFTH DYADS ON ONE, TWO-AND AND FOUR-AND", "ROOT-FIFTH DYADS ON ONE AND TWO-AND" }, { "ROOT QUARTERS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "AN ORGAN ANSWER EVERY OTHER BAR", "SILENT" }, { "FOUR ON THE FLOOR, BACKBEAT, CRASH ON EVERY ONE", "FOUR ON THE FLOOR, BACKBEAT, OPEN EIGHTHS" }, {} },
                { "CHORD ON ONE, PUSHES ON THE A OF ONE AND THREE", { "PUSHED CHORD BAR, THEN A STEP DOWN", "PUSHED CHORD BAR EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ONE, ONE-A, TWO-AND, THREE-AND, FOUR-AND, CRASH QUARTERS", "KICK ONE, ONE-A, TWO-AND, THREE-AND, FOUR-AND, SNARE PICKUPS" }, {} },
                { "UNISON STAB FIGURE ON ONE, SHAKER AFTER", { "A STAB FIGURE ON ONE, THEN A GROOVE BAR", "THE STAB FIGURE ON ONE EVERY BAR" }, { "THE STAB FIGURE, THEN ROOTS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "THE STAB FIGURE ON THE CHORD", "SILENT" }, { "THE FIGURE ON KICK AND SNARE, SHAKER AFTER", "THE FIGURE, THEN A GROOVE BAR" }, {} },
                { "UNISON LINE, KICK AND CRASH ON EVERY NOTE", { "HOOK, A UNISON LINE DOWN THE SCALE, FIVE HELD, TOM BAR", "THE UNISON LINE EVERY OTHER BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "THE HOOK OVER A HELD CHORD", "SILENT" }, { "KICK AND CRASH ON EVERY NOTE OF THE LINE, A TOM BAR", "KICK AND CRASH ON EVERY NOTE OF THE LINE" }, {} },
                { "SNARE OUT, TWIN TRILL OVER SIXTEENTH HATS", { "TWIN TRILL IN SIXTEENTHS, ONE A SIXTEENTH LATE", "ONE TRILL IN SIXTEENTHS" }, { "WITH THE KICK'S PUSHES", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SIXTEENTH HATS, NO SNARE, THEN THE SNARE BACK", "SIXTEENTH HATS, NO SNARE" }, {} },
                { "THREE CHORD PAIRS, THEN A LEAD BAR", { "THREE CHORD PAIRS, THEN A BAR OUT", "THREE CHORD PAIRS EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "BUSY KICK BAR, THEN A PICKUP BAR", "BUSY KICK EVERY BAR, OPEN EIGHTHS" }, {} },
                // (0.253.0) Round eleven's choruses, part two.
                { "ONE CHORD A BAR, TIED OVER EVERY OTHER", { "EIGHTHS, EVERY OTHER BAR TIED OVER THE ONE", "THE TIED EIGHTHS, A FALLING TOP OVER A PEDAL" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK TIED WITH THE BAND, CRASH QUARTERS", "KICK TIED WITH THE BAND, CLOSED EIGHTHS" }, {} },
                { "OCTAVE LEAP, CLIMB TO THE FLAT SEVEN", { "OCTAVE LEAP, CLIMB TO THE FLAT SEVEN, ON THE TONIC", "THE OCTAVE CLIMB AN OCTAVE UP" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRINGS HOLD THE ROOT AN OCTAVE UP", "SILENT" }, { "KICK ONE AND TWO-AND, OPEN HAT ON THREE-AND, A SNARE FILL IN BAR FOUR", "KICK ONE AND TWO-AND, CLOSED EIGHTHS, NO FILL" }, {} },
                { "TRIPLE STABS UNDER A HORN LINE", { "TRIPLE STABS ON ONE AND TWO-AND, A HALF-STEP TAG", "TRIPLE STABS ON ONE AND TWO-AND" }, { "OUT", "THE STABS ON THE ROOT", "LOCKED WITH THE KICK" },
                  { "A HORN LINE AROUND THE FIFTH AND FLAT SEVEN", "SILENT" }, { "FOUR ON THE FLOOR, BACKBEAT, CRASH ON EVERY ONE", "FOUR ON THE FLOOR, BACKBEAT, OPEN EIGHTHS" }, {} },
                { "TWO-BAR BROKEN BACKBEAT UNDER HELD PUSHES", { "HELD PUSHES ON ONE, TWO-AND, THREE, FOUR AND FOUR-AND", "THE PUSHES, A CHORD EVERY TWO BARS" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "TWO-BAR BROKEN BACKBEAT, ACCENTED SIXTEENTH HATS", "TWO-BAR BROKEN BACKBEAT, RIDE EIGHTHS" }, {} },
                { "SUS CHORD PICKED OVER THE BALLAD KICK", { "THE CHORD WITH ITS SECOND PICKED IN THREES AND TWOS", "OUT" }, { "LONG ROOTS ON ONE AND THREE-AND", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRINGS HOLD THE CHORD, A HIGH WHOLE-NOTE LINE", "HELD PADS" }, { "BALLAD KICK, RIDE EIGHTHS, CRASH EVERY TWO BARS", "BALLAD KICK, CLOSED EIGHTHS" }, {} },
                { "HALF TIME, KICK BURSTS WITH THE CHUGS", { "ROOT CHUGS WITH A PUSH ON FOUR-AND, THEN TWO POWER CHORDS", "ROOT CHUGS EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF TIME, KICK BURSTS WITH THE CHUGS, OPEN HAT QUARTERS", "HALF TIME, KICK BURSTS, CLOSED EIGHTHS" }, {} },
                { "RIFF PAIR, A TOM RUN CLOSING EVERY SECOND BAR", { "LOW ROOT RIFF, THEN PUSHED CHORDS", "THE LOW ROOT RIFF EVERY BAR" }, { "ROOT EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIFF BAR, THEN A PUSHED BAR WITH A TOM RUN", "RIFF BAR EVERY BAR, NO TOM RUN" }, {} },
                { "LOW-HIGH OCTAVE CHUGS IN THREES", { "LOW ROOT, HIGH OCTAVE HELD, IN THREES, THEN THE FOURTH", "LOW-HIGH OCTAVES IN THREES ALL BAR" }, { "ROOT SIXTEENTHS, THE FLAT SEVEN ON FOUR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK IN THREES, A SNARE DRAG, RIDE EIGHTHS", "KICK IN THREES, BACKBEAT, RIDE EIGHTHS" }, {} },
                { "TWICE-STRUCK OCTAVE ARPEGGIO BALLAD", { "CLEAN CHORDS ON ONE AND THREE", "OUT" }, { "ONE AND THREE, FOUR EVERY OTHER BAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "OCTAVE ARPEGGIO, EVERY NOTE TWICE", "A PUSHED PIANO FIGURE" }, { "KICK ONE AND THREE, QUARTER HATS, HAND PERCUSSION", "KICK ONE AND THREE, QUARTER HATS" }, {} },
                { "PUSHED CHORD BAR THRICE, OCTAVE RUN BAR", { "THREE PUSHED BARS, THEN A LOW-HIGH OCTAVE RUN", "THE PUSHED BAR EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK WITH THE HITS, CRASH QUARTERS, THE RUN BAR WITH THE GUITAR", "KICK WITH THE HITS, CLOSED EIGHTHS" }, {} },
                { "FOUR ON THE FLOOR, HORN ANSWER EVERY OTHER BAR", { "QUARTER POWER CHORDS", "OUT" }, { "ROOT QUARTERS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "A HORN ANSWER EVERY OTHER BAR", "SILENT" }, { "FOUR ON THE FLOOR WITH A THREE-AND PAIR, OPEN HATS ON THE BEATS", "PLAIN FOUR ON THE FLOOR, OPEN HATS ON THE BEATS" }, {} },
                { "ROOT BURST, THEN TWO CHORDS SWAPPING", { "ROOT BURST, THE FIFTH, THEN TWO CHORDS SWAPPING", "ROOT BURST, THEN THE FIFTH HELD" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ONE, ONE-A, TWO-AND, THREE, THREE-A, FOUR-AND, CRASH QUARTERS", "THE SAME KICK, BACKBEAT, RIDE EIGHTHS" }, {} },
                { "GALLOP OVER A 3+3+2 KICK", { "GALLOP ON EVERY BEAT", "GALLOP, POWER CHORDS ON THE BEATS" }, { "GALLOP ON ONE AND THREE, EIGHTHS BETWEEN", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRINGS HELD, A HOOK ON TOP", "HELD PADS" }, { "3+3+2 KICK, OPEN HAT ON ONE, TWO-AND AND THREE", "3+3+2 KICK, CRASH EVERY TWO BARS" }, {} },
                { "HALF TIME, THE ONE LEFT OUT EVERY OTHER BAR", { "THE ONE LEFT OUT, THEN EIGHTHS", "EIGHTHS EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HALF TIME, CRASH ON EVERY QUARTER, THE ONE LEFT OUT", "HALF TIME, RIDE QUARTERS, THE ONE LEFT OUT" }, {} },
                { "KICK ON ONE, TWO-AND AND THREE-AND, ARPEGGIO ABOVE", { "TWO OR THREE PICKED NOTES A BAR", "OUT" }, { "SLAP FIGURE, ROOT TO OCTAVE AND BACK", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SIXTEENTH ARPEGGIO, STRINGS HELD", "SIXTEENTH ARPEGGIO, ROOT FIFTH OCTAVE" }, { "KICK ONE, TWO-AND AND THREE-AND, SIXTEENTH HATS, OPEN ON ONE", "KICK ONE, TWO-AND AND THREE-AND, CLOSED EIGHTHS" }, {} },
                { "ROLLING PIANO OCTAVES UNDER CHOPPED STABS", { "MUTED FULL-CHORD STABS ON TWO, TWO-AND AND FOUR", "OUT" }, { "ROOT EIGHTHS, THE LOW OCTAVE ON THREE-AND AND FOUR-AND", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PIANO OCTAVES BROKEN ON THE SIXTEENTHS", "HELD PADS" }, { "BUSY KICK, BACKBEAT, TAMBOURINE", "BUSY KICK, BACKBEAT, CLOSED EIGHTHS" }, {} },
                // (0.252.0) Round eleven's choruses, part one.
                { "HELD CHORDS OVER THE STOMP, STABS AROUND THE BEATS", { "HIGH OCTAVE DYADS ON EVERY SIXTEENTH BUT THE BEATS", "OUT" }, { "ROOTS HELD", "ROOT EIGHTHS", "LOCKED WITH THE KICK" },
                  { "WIDE HELD CHORDS, A BAR EACH", "HELD PADS" }, { "THE STOMP, HATS ON TWO AND FOUR, A HIT ON THREE TO START", "THE STOMP, CRASH ON ONE" }, {} },
                { "RING ON ONE, PUSHES ON TWO-A AND THREE-AND", { "RING ON ONE, PUSHES THROUGH THE BAR", "RING ON ONE, PUSHES, THE NEXT CHORD HALFWAY" }, { "ROOTS WITH THE KICK", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ONE, TWO-A, THREE-AND, FOUR-E, RIDE FROM ONE-AND", "THE PUSHED KICK, CLOSED EIGHTHS" }, {} },
                { "FOUR CHORDS IN THE FIRST HALF, PEDAL BAR ANSWERS", { "FOUR CHORDS ON THE EIGHTHS, THEN A PEDAL BAR", "FOUR CHORDS, THEN THE PEDAL MUTED" }, { "FOUR ROOTS, THEN ROOT AND TENTH SIXTEENTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "A HELD HIGH NOTE", "SILENT" }, { "SYNCOPATED KICK, SNARE PAIRS ROUND THE BACKBEAT, OPEN EIGHTHS", "BACKBEAT, OPEN EIGHTHS" }, {} },
                { "QUARTER CHORDS, PUSH ON FOUR-AND, CRASH QUARTERS", { "QUARTER POWER CHORDS, THE NEXT PUSHED ON FOUR-AND", "QUARTER POWER CHORDS, A SECOND GUITAR AN OCTAVE UP" }, { "ROOT QUARTERS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "WHOLE-NOTE CHORDS", "HELD PADS" }, { "HALF TIME: KICK ONE AND TWO, SNARE THREE, CRASH QUARTERS", "HALF TIME: KICK ONE AND TWO, SNARE THREE, RIDE QUARTERS" }, {}, { "THE SAME QUARTERS, STRUMMED" } },
                { "EIGHTH CHORDS MOVING BY THIRDS AND HALF STEPS", { "ONE POWER CHORD A BAR IN EIGHTHS", "POWER CHORDS IN EIGHTHS, OCTAVES ON TOP" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK IN PAIRS, SNARE ON EVERY AND, OPEN EIGHTHS", "KICK IN PAIRS, BACKBEAT" }, {} },
                { "PHRYGIAN STABS OVER A DOUBLE-KICK PEDAL", { "ROOT SIXTEENTHS, THE FLAT TWO STRUCK ON ONE, THREE AND FOUR-AND", "ROOT SIXTEENTHS, THE MINOR THIRD'S HALF BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "A HELD HIGH FIFTH" }, { "KICK ON EVERY SIXTEENTH, SNARE ONE, THREE AND FOUR-AND, CHINA EIGHTHS", "KICK ON EVERY SIXTEENTH, BACKBEAT" }, {} },
                { "ROLLING SNARE BAR, BACKBEAT BAR, STRUMMED EIGHTHS", { "CHORDS STRUMMED ON EVERY EIGHTH, THE HALF STEP ROCKING ON TOP", "CHORDS STRUMMED ON EVERY EIGHTH" }, { "ROOT EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "A ROLLING SNARE BAR, THEN A BACKBEAT BAR", "THE BACKBEAT BAR EVERY BAR" }, {} },
                { "CHORDS AND KICKS ON THE ANDS", { "POWER CHORDS ON THE ANDS, A PICKED BAR EVERY OTHER", "POWER CHORDS ON THE ANDS" }, { "WITH THE GUITAR'S ANDS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "AN OCTAVE LINE ON THE ANDS", "SILENT" }, { "KICK AND OPEN HAT ON EVERY AND, BACKBEAT", "KICK ON EVERY AND, CLOSED HATS ON THE BEATS" }, {} },
                { "UNISON SIXTEENTH RIFF, KICK ON THE A'S", { "SIXTEENTH UNISON RIFF, THE NEXT CHORD IN THE LAST FOUR", "THE SIXTEENTH RIFF ON ONE CHORD" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "A LEADING-TONE GRACE ON TWO-A AND FOUR-A", "HELD PADS" }, { "KICK ONE, ONE-A, TWO-A AND THREE-A, ACCENTED RIDE", "THE SAME KICK ON THE A'S, CLOSED EIGHTHS" }, {} },
                { "CRASH WASH OVER TRITONE STABS", { "ROOT, THEN TRITONE STABS, A WALK BACK IN THE SECOND BAR", "ROOT AND TRITONE STABS EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "AN OCTAVE STAB WITH EACH HIT", "SILENT" }, { "CRASH IN BROKEN SIXTEENTHS, KICK WITH THE STABS", "CHINA IN BROKEN SIXTEENTHS, KICK WITH THE STABS" }, {} },
                { "HOOK IN BLOCK CHORDS OVER ROOT EIGHTHS", { "THE HOOK IN CLOSE TRIADS", "A CHORD HELD A BAR" }, { "ROOT EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "THE HOOK IN CLOSE TRIADS, IN EIGHTHS", "HELD PADS" }, { "KICK ONE, THREE AND THREE-AND, HAT KNOCKS, A FILL EVERY EIGHT", "KICK ONE, THREE AND THREE-AND, CLOSED EIGHTHS" }, {} },
                // (0.251.0) Round eleven's verses, part two.
                { "TWO BARS OF RIFF, TWO OF KICK UNDER THE VOICE", { "TWO BARS OF POWER CHORDS, TWO OUT, A PUSH ON FOUR", "TWO BARS OF POWER CHORDS, TWO OUT" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "TWO BARS OF BEAT, TWO OF KICK QUARTERS", "TWO BARS OF BEAT, TWO OF KICK AND HATS" }, {} },
                { "SIX-BAR RIFF, FOUR THE SAME AND TWO TO ANSWER", { "SINGLE-NOTE RIFF, SIX BARS A CYCLE", "THE SIX-BAR RIFF AN OCTAVE UP" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "ONE HIGH SYNTH HIT ON FOUR", "SILENT" }, { "KICK ONE, TWO-AND AND THREE-AND, CRASH ON FOUR IN THE ANSWER", "KICK ONE AND TWO-AND, RIDE EIGHTHS" }, {} },
                { "CHORD RUNG, THREE EIGHTHS INTO THE NEXT", { "CHORD RUNG, THREE EIGHTHS AT THE END", "CHORD RUNG, EIGHTHS FROM TWO-AND" }, { "ROOTS IN THE GUITAR'S RHYTHM", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "BRUSHED: KICK ON ONE, SNARE ON THREE, RIDE QUARTERS", "SIDESTICK ON THREE, RIDE QUARTERS" }, {}, { "OPEN CHORD RUNG, THREE EIGHTHS IN" } },
                { "DOTTED SYNTH BASS UNDER SIXTEENTH CHUGS", { "MUTED SIXTEENTH CHUGS, ACCENTS, A HALF STEP AT THE END", "ROOT EIGHTHS, A SIXTEENTH PAIR ON TWO-AND" }, { "DOTTED LINE: ROOT, OCTAVE, FLAT SEVEN, FIVE, FOUR, FIVE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "FOUR ON THE FLOOR, HAND DRUM EIGHTHS, CRASH ON EVERY ONE", "FOUR ON THE FLOOR, BACKBEAT, CLOSED EIGHTHS" }, {} },
                { "SIDESTICK BURSTS OVER FOUR ON THE FLOOR", { "QUIET MUTED CHUG RUNS ON THE ROOT", "OUT" }, { "ROOT HELD A BEAT, THEN EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SIDESTICK BURSTS, SIXTEENTH HATS, FOUR ON THE FLOOR", "FOUR ON THE FLOOR, SIXTEENTH HATS, PEDAL HAT" }, {}, { "OPEN-STRING FIGURE IN SIXTEENTHS", "SYNCOPATED STRUMS UNDER THE FIGURE" } },
                { "SHUFFLED HALF-BAR CHORDS WITH A MUTED SCRATCH", { "HALF-BAR CHORDS, A MUTED SCRATCH BEFORE EACH", "HALF-BAR CHORDS, NO SCRATCH" }, { "ROOTS ON THE HALF BARS, PUSHED", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SHUFFLE: OPEN HAT ON THE BEATS", "SHUFFLE: CLOSED HATS ON THE TRIPLETS" }, {} },
                { "NYLON DYAD FLICKS OVER A BALLAD KICK", { "TWO DYAD SHAPES IN SIXTEENTHS, A LONG NOTE EVERY SECOND BAR", "OUT" }, { "ONE, THREE AND THREE-AND, LONG", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "BALLAD KICK, OPEN HAT ON FOUR-AND EVERY SECOND BAR", "BALLAD KICK, SIDESTICK BACKBEAT" }, {}, { "HIGH FIFTH-AND-OCTAVE DYAD FLICKS" } },
                { "TONIC HELD, THEN EIGHTHS ON THE OTHER THREE", { "TONIC BAR, THEN THREE CHORDS IN EIGHTHS", "TONIC BAR, THEN THE THREE CHORDS RUNG" }, { "ROOTS AND THE FIFTH UNDER, THEN THE GUITAR'S RHYTHM", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OPEN HAT ON EVERY EIGHTH BUT ONE, FLOOR TOM ON ONE", "CLOSED EIGHTHS, CRASH ON ONE" }, {} },
                { "TWO GALLOP BARS, TWO BARS OF RING", { "TWO GALLOP BARS, A HIGH TRIAD RUNG, A NEAR-EMPTY BAR", "GALLOP ON EVERY BAR" }, { "THE GALLOP, THEN THE ROOT HELD", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SYNTH DOUBLES THE GALLOP", "SILENT" }, { "GALLOP KICK, OPEN HAT QUARTERS, A FILL IN BAR FOUR", "GALLOP KICK, CRASH EVERY TWO BARS" }, {} },
                { "SHORT CHORD STABS ON TWO, TWO-AND AND FOUR", { "MUTED FULL-CHORD STABS ON TWO, TWO-AND AND FOUR", "STABS ON TWO AND FOUR ONLY" }, { "ROOT EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PIANO ROOT AND FIFTH SYNCOPATIONS", "HELD PADS" }, { "KICK ONE, TWO-AND, THREE AND FOUR-AND, TAMBOURINE QUARTERS", "THE SAME KICK, CLOSED EIGHTHS" }, {} },
                { "SLAP OCTAVE BASS UNDER A SIXTEENTH ARPEGGIO", { "CHORD PUSHED ON TWO-AND, RUNG TO THE BAR LINE", "OUT" }, { "SLAP FIGURE, ROOT TO OCTAVE AND BACK", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SIXTEENTH ARPEGGIO, ROOT FIFTH OCTAVE", "SIXTEENTH ARPEGGIO, STRINGS HELD" }, { "KICK ON ONE ONLY, BACKBEAT, SIXTEENTH HATS", "KICK ON ONE, A SNARE RUN EVERY FOURTH BAR" }, {} },
                { "SIXTEENTH BASS RIFF, A RUN TO THE BAR LINE", { "ROOT AND HIGH THIRD ON ONE, THEN ONE-A, TWO AND TWO-AND", "OUT" }, { "SIXTEENTH PEDAL, A RUN TO THE BAR LINE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "AN ORGAN LINE OF LONG NOTES", "SILENT" }, { "KICK ONE AND THREE, SNARE TWO, TWO-A AND FOUR, HAND DRUMS", "KICK ONE AND THREE, SNARE TWO, TWO-A AND FOUR, CLOSED EIGHTHS" }, {} },
                { "THE CHANGE PUSHED, THE NEXT DOWNBEAT EMPTY", { "CLEAN CHORDS PUSHED, THE NEXT ONE EMPTY", "POWER CHORDS PUSHED, THE NEXT ONE EMPTY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "A PAD TIED THE SAME WAY", "SILENT" }, { "KICK PUSHED WITH THE BAND, HAT EIGHTHS", "KICK PUSHED WITH THE BAND, RIDE EIGHTHS" }, {} },
                { "CANON LINES OVER A LAID-BACK SIXTEENTH KIT", { "A FALLING SIXTEENTH LINE IN CANON", "THE FALLING LINE ALONE" }, { "ROOT AND FIFTH SIXTEENTH GROOVE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "AN ORGAN IN THE SECOND HALF", "SILENT" }, { "LAID-BACK KIT, SNARE ON TWO, ITS AND, AND FOUR", "LAID-BACK KIT, NO HATS" }, {} },
                { "ONE GUITAR'S EIGHTHS, A PEDAL HAT, THEN A DOUBLE", { "POWER-CHORD EIGHTHS, DOUBLED FROM THREE QUARTERS", "POWER-CHORD EIGHTHS ALONE" }, { "OUT", "OUT, THEN ROOTS FROM HALFWAY", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT, THEN PEDAL HAT QUARTERS FROM HALFWAY", "PEDAL HAT QUARTERS" }, {} },
                { "TWO HELD EIGHTHS, FOUR SIXTEENTHS", { "TWO HELD EIGHTHS, FOUR SIXTEENTHS ON THE FLAT SEVEN", "TWO HELD EIGHTHS, FOUR SIXTEENTHS ON THE FOUR" }, { "DOUBLES THE GUITAR", "ROOT EIGHTHS", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "NO KICK, OPEN HATS SKIPPING TWO AND FOUR", "PLAIN BACKBEAT, OPEN EIGHTHS" }, {} },
                { "OCTAVE STABS OVER DOUBLE-KICK PAIRS", { "ROOT OCTAVE STABS, THE LAST GROUP A STEP UNDER", "ROOT OCTAVE STABS" }, { "LOW ROOT, THEN ITS OCTAVE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "DOUBLE-KICK PAIRS, OPEN HATS OFF THE BEAT", "DOUBLE-KICK PAIRS, CLOSED EIGHTHS" }, {} },
                { "TWO-SHAPE SIXTEENTH SEESAW, BASS ROCKING UNDER", { "TEN SIXTEENTHS SEESAWING TWO SHAPES, THEN A HELD CHORD", "THE SEESAW, ALWAYS A HELD CHORD" }, { "ROOT AND FLAT SEVEN, THEN FIFTH AND FOURTH ROCKING", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK IN THREES, SNARE PICKUPS, CRASH EIGHTHS", "KICK IN THREES, CLOSED EIGHTHS" }, {} },
                { "SIXTEENTH CALL, DOTTED ANSWER", { "SIXTEENTH CALL, DOTTED-EIGHTH ANSWER", "THE CALL EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON THE CALL, HATS HALVED IN THE ANSWER", "KICK ON THE CALL, CLOSED EIGHTHS" }, {} },
                { "KICK PICKUPS INTO EVERY BEAT, OCTAVE PICKUPS", { "QUARTER POWER CHORDS, CLEAN OCTAVE PICKUPS", "QUARTER POWER CHORDS" }, { "ROOT QUARTERS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "AN ANSWER EVERY FOUR BARS" }, { "KICK PICKUPS INTO EVERY BEAT, SNARE PICKUPS", "KICK PICKUPS, PLAIN BACKBEAT" }, {} },
                { "SHAKER SIXTEENTHS, CHORDS UNDER ONE TOP NOTE", { "OUT", "A HELD CLEAN CHORD ON ONE" }, { "ONE AND THREE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "CHORDS UNDER ONE TOP NOTE, A PIANO FRAGMENT", "CHORDS UNDER ONE TOP NOTE" }, { "HAND PERCUSSION SIXTEENTHS, QUARTER HATS, A RIM TAP", "SHAKER SIXTEENTHS ONLY" }, {} },
                { "HALF-NOTE CHORDS, KICK ON ONE AND TWO-AND", { "HALF-NOTE POWER CHORDS WITH THE THIRD", "HALF-NOTE POWER CHORDS WITH THE FOURTH" }, { "TWO HITS: ONE AND TWO-AND", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "KICK ON ONE AND TWO-AND, OPEN HAT QUARTERS", "KICK ON ONE AND TWO-AND, RIDE QUARTERS" }, {} },
                // (0.264.0) Round twelve's verses.
                { "TREMOLO ROOT, THEN THE TRITONE, HALF A BAR EACH", { "ROOT AND FLAT FIVE TREMOLO, HALF A BAR EACH", "ROOT AND FIFTH TREMOLO, HALF A BAR EACH" }, { "SIXTEENTHS WITH THE TREMOLO", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "ROOT AND FLAT SEVEN ROCKING, A THIRD UP ON THREE-AND", "SILENT" }, { "DOUBLE TIME, A BLAST IN THE LAST TWO BARS", "BLAST ALL THE WAY" }, {} },
                { "CHORDS CLIMB ON THE BEATS, A FOUR-NOTE CELL OVER", { "CHORDS CLIMB ON ONE, TWO, THREE, CHUG PAIRS ON FOUR", "CHUG PAIRS ON TWO AND FOUR ONLY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ONE AND THREE, OPEN HAT EIGHTHS, A TOM-EIGHTHS BAR EVERY EIGHT", "KICK ONE AND THREE, OPEN HAT EIGHTHS" }, { "A FOUR-NOTE SIXTEENTH CELL ON EACH CHORD", "SILENT" } },
                { "TRAIN BEAT UNDER A PICKED ACOUSTIC WITH RUNS", { "OUT", "A CLEAN FILL EVERY OTHER BAR" }, { "ROOT, A WALK UP, A PICKUP INTO THE NEXT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "TRAIN BEAT, A THREE-TOM PICKUP EVERY EIGHT", "TRAIN BEAT, RIDE IN PLACE OF HATS" }, {}, { "PICKED CHORD TONES, A RUN INTO EACH CHANGE" } },
                { "STAB, PUSH, SIXTEENTH CHUGS, A FLURRY BAR", { "STAB, PUSH, CHUGS, A NEIGHBOUR FLURRY EVERY FOURTH", "STAB, PUSH, SIXTEENTH CHUGS" }, { "ROOT CHUGS WITH THE STABS", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ONE AND THREE, OPEN QUARTERS, A DOUBLE-KICK FOURTH BAR", "KICK ONE AND THREE, OPEN QUARTERS, A CRASH EVERY FOUR" }, {} },
                { "FALLING QUARTER LINE OVER DYAD EIGHTHS", { "FIFTH DYAD AND ROOT IN EIGHTHS", "ROOT SIXTEENTHS, A DYAD ON ONE AND THREE" }, { "ROOTS HELD", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON ONE, ONE-AND, TWO-AND, THREE, THREE-AND, FOUR-AND", "THE PUSHED KICK, SNARE ON ONE, TWO-AND AND THREE" }, { "QUARTERS FALLING A HALF STEP", "SILENT" } },
                { "LOW ROOTS WITH CHORD GRABS", { "ROOT AND CHORD GRABS", "ROOT AND CHORD GRABS, A SIXTEENTH FILL EVERY FOURTH BAR" }, { "ROOTS WITH THE GRABS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "BALLAD KICK, RIDE EIGHTHS, A SNARE PAIR EVERY FOURTH BAR", "BALLAD KICK, CLOSED HAT EIGHTHS, NO PAIR" }, { "A RINGING DYAD AT EACH BAR'S END", "SILENT" } },
                { "TWIN THIRDS OVER TIED EIGHTH CHORDS", { "EIGHTH CHORDS, EVERY CHANGE TIED IN FROM FOUR-AND", "EIGHTH CHORDS ON THE BAR LINE" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK WITH THE PUSH, SNARE ON THREE, OPEN QUARTERS", "DOUBLE KICK EVERY FOURTH BAR, RIDE QUARTERS" }, { "TWIN MELODY IN THIRDS", "SILENT" } },
                { "CHORD, CHUGS, THEN A FALLING LINE IN THIRDS", { "CHORD, ROOT SIXTEENTHS, PAIRS FALLING IN THIRDS", "CHORD AND ROOT SIXTEENTHS ONLY" }, { "FOLLOWS THE LOWER VOICE", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK FIGURE UNDER CRASH EIGHTHS, A DOUBLE-KICK PAIR EVERY EIGHT", "THE KICK FIGURE, RIDE EIGHTHS" }, {} },
                { "FIVE-HIT CLUSTER, THEN A PEDAL LEAD", { "FIVE-HIT CLUSTER", "FIVE-HIT CLUSTER, A STEP UP EVERY OTHER BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK WITH THE CLUSTER, QUARTER HATS, CRASH EIGHTHS FROM HALFWAY", "KICK WITH THE CLUSTER, QUARTER HATS" }, { "A HIGH PEDAL LEAD FROM HALFWAY", "SILENT" } },
                // (0.250.0) Round eleven's verses, part one.
                { "THREE SNARES AFTER A LONE KICK", { "OUT", "ONE MUTED ROOT ON ONE" }, { "ROOT HELD, THREE SIXTEENTHS INTO THE NEXT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "MINOR THIRD ROCKING ON EACH BEAT AND ITS A", "HELD PADS" }, { "LONE KICK, SNARES ON TWO, THREE AND FOUR", "LONE KICK, HATS ON TWO AND FOUR" }, {} },
                { "PICKED SEVENTHS OVER A FALLING LINE", { "OUT", "A HIGH NOTE HELD EVERY FOUR BARS" }, { "THE FALLING LINE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SPLIT HATS AND RIDE, A SNARE PICKUP EVERY EIGHT", "SIDESTICK BALLAD, RIDE ON THE ANDS" }, {}, { "PICKED CHORD TONES OVER THE FALLING BASS" } },
                { "ONE KEY NOTE IN 3+3+2, A STEP LOWER EACH BAR", { "OUT", "A SOFT CHORD ON ONE EVERY TWO BARS" }, { "ROOTS IN 3+3+2 WITH A PASSING NOTE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "ONE NOTE IN 3+3+2, A STEP LOWER EACH BAR", "HELD PADS" }, { "SIXTEENTH HATS WITH GAPS, OPEN ON THREE-AND AND FOUR-AND", "PLAIN BACKBEAT, CLOSED SIXTEENTHS" }, {} },
                { "LOW OCTAVE EIGHTHS UNDER A SHAKER", { "ROOT AND OCTAVE EIGHTHS, A CLIMB EVERY OTHER BAR", "OCTAVE EIGHTHS, A SECOND ON THE FLAT SEVEN" }, { "ROOTS HELD", "ROOT EIGHTHS", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SHAKER SIXTEENTHS, KICK ONE AND THREE", "SHAKER EIGHTHS, OPEN HAT ON FOUR-AND" }, {} },
                { "SIX-BEAT RIFF ACROSS THE BAR", { "SIX-BEAT POWER-CHORD RIFF", "SIX-BEAT RIFF ON SINGLE NOTES" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "CRASH QUARTERS, KICK WITH THE RIFF'S PHRASE", "THE SNARE EVERY SIX SIXTEENTHS" }, {} },
                { "FIFTH AND FOURTH ROCKING OVER THE PEDAL", { "PEDAL UNDER FIFTH AND FOURTH DYADS, A HALF STEP UP AT THE END", "PEDAL AND FIFTH DYADS ONLY" }, { "ROOT ON THE RIFF'S HITS, THE HALF STEP TOO", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON THE RIFF, CRASH QUARTERS", "KICK ON THE RIFF, CLOSED EIGHTHS" }, {} },
                { "FOUR ON, TWO OFF, UNISON SIXTEENTHS", { "SIXTEENTH GROUPS OF FOUR, TWO OFF", "GROUPS OF FOUR ON THE ROOT ONLY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK WITH THE GROUPS, SNARE AND CRASH ON THE ANDS IN BAR FOUR", "KICK WITH THE GROUPS, BACKBEAT" }, {} },
                { "MUTED EIGHTHS, STRUMS WITH AN OPEN-STRING TAIL", { "SOFT MUTED POWER-CHORD EIGHTHS", "OUT" }, { "ROOT ON ONE HELD, THEN THREE AND FOUR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HALF-NOTE CHORDS", "SILENT" }, { "HALF TIME: KICK ONE, TWO AND FOUR, SNARE ON THREE", "HALF TIME, RIDE QUARTERS" }, {}, { "THREE STRUMS, OPEN STRINGS ON FOUR AND FOUR-AND" } },
                { "CHROMATIC BASS CLIMB, KICK EVERY OTHER BAR", { "OUT", "ONE DYAD HELD FROM HALFWAY" }, { "SIXTEENTHS CLIMBING BY HALF STEPS OVER TWO BARS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "LOW SYNTH ON THE BASS'S NOTES", "SILENT" }, { "KICK EVERY OTHER BAR, BACKBEAT, QUARTER HATS", "KICK EVERY OTHER BAR, RIDE EIGHTHS" }, {} },
                { "STABS, THEN A PICKED RUN IN THE SAME BAR", { "STABS, THEN A PICKED RUN", "STABS AND RUN, A SECOND GUITAR AN OCTAVE UP" }, { "ROOT ON THE STABS, OUT FOR THE RUN", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK WITH THE STABS, BACKBEAT, HAT EIGHTHS", "KICK WITH THE STABS, RIDE EIGHTHS" }, {} },
                { "TWO TOMS FOR THE BACKBEAT, ROCKING TOP", { "TOP NOTE ROCKING A HALF STEP, A HIGH NOTE ON TWO AND FOUR", "TOP NOTE ROCKING A HALF STEP" }, { "ROOT EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HELD PADS", "SILENT" }, { "TWO TOMS ON TWO AND FOUR, NO SNARE OR CYMBALS", "TWO TOMS ON TWO AND FOUR, SHAKER EIGHTHS" }, {} },
                { "TWO KICKS UNDER RINGING DYADS AND SHAKER", { "RINGING SUS DYADS ON THE EIGHTHS", "RINGING SUS DYADS, A PICKED BAR EVERY OTHER" }, { "TWO NOTES WITH THE KICK", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON ONE-AND AND TWO, SHAKER EIGHTHS", "KICK ON ONE-AND AND TWO, SHAKER AND HAT EIGHTHS" }, {} },
                { "3+3 GROUPS, BEAT FOUR LEFT OPEN", { "OUT", "A HIGH NOTE HELD A BAR" }, { "ROOTS ON ONE AND TWO-AND", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "ONE HIGH BELL NOTE A BAR", "SILENT" }, { "3+3 GROUPS, AN OPEN HAT ON FOUR", "THE KICK'S GROUPS ONLY" }, {}, { "ROOT, ROOT, FIFTH TWICE, RINGING OVER FOUR" } },
                { "STICK LOOP, BASS ROOT PAIRS", { "OUT", "A HIGH CHORD TONE EVERY FOUR BARS" }, { "ROOT AND OCTAVE ON ONE AND ONE-AND, A WALK EVERY OTHER BAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SOFT PAD, A HIGH FLOURISH EVERY OTHER BAR", "HELD PADS" }, { "STICK LOOP AND CABASA, NO KIT", "STICK LOOP, KICK ON ONE" }, {} },
                { "CHORD ON ONE, THE NEXT PUSHED ON FOUR, DOTTED LINE OVER", { "CHORD ON ONE, THE NEXT PUSHED ON FOUR", "CHORD ON ONE, A CLUSTER COLOUR, THE NEXT ON FOUR" }, { "ROOTS ON ONE AND ON THE PUSH", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "FOUR NOTES ON DOTTED EIGHTHS", "SILENT" }, { "OUT, THEN KICK ONE, ONE-A, TWO-A AND THREE-A", "SPARSE HATS AND BACKBEAT, OPEN HAT ON FOUR-AND" }, {} },
                { "BASS GROOVE, GUITAR ONLY ON THE PICKUPS", { "THREE MUTED PICKUPS EVERY OTHER BAR", "OUT" }, { "TWO-BAR GROOVE, HELD NOTES ON ONE AND TWO-AND", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ONE AND THREE-AND, BUSY HATS", "KICK ONE AND THREE-AND, RIDE" }, {} },
                { "PIANO EIGHTHS AND PIZZICATO OCTAVES, NO BACKBEAT", { "OUT", "PICKED TRIADS A BAR" }, { "TWO NOTES A BAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PIANO CHORDS ON THE BEATS, THE ROOT ON THE ANDS", "PIZZICATO OCTAVES BOUNCING" }, { "HAT EIGHTHS, RIDE BELL ON THREE-AND, NO KICK OR SNARE", "HAT EIGHTHS ONLY" }, {} },
                // (0.240.0) Round ten's verses.
                { "PIANO BALLAD VERSE", { "A CLEAN DYAD EVERY OTHER BAR", "OUT" }, { "OUT, THEN ROOTS FROM HALFWAY", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PIANO BROKEN TRIAD EIGHTHS", "PIANO BROKEN TRIAD, CHORD ON THE LAST EIGHTH" }, { "OUT, THEN A KICK ON ONE FROM HALFWAY", "SOFT SNARE TEXTURE, KICK FROM HALFWAY" }, {} },
                { "POWER-BALLAD VERSE, GUITAR ANSWERS", { "CLEAN ANSWERS IN THE GAPS", "A RUNG CHORD EVERY TWO BARS" }, { "WHOLE NOTES ON THE ROOT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PIANO WHOLE-NOTE CHORDS", "PIANO WHOLE NOTES, A PICKUP ON FOUR" }, { "BALLAD KICK AND SIDESTICK", "RIDE QUARTERS, KICK ON ONE" }, {} },
                { "BAND HIT EVERY FOUR BARS UNDER THE PIANO", { "A HIT ON ONE AND ONE-AND EVERY FOUR BARS", "A HIT ON ONE EVERY FOUR BARS, RUNG" }, { "HITS EVERY FOUR BARS WITH THE BAND", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PIANO DYADS IN EIGHTHS", "PIANO DYADS, STRINGS HELD" }, { "KICK AND CRASH EVERY FOUR BARS", "KICK AND CRASH EVERY FOUR BARS, HATS BETWEEN" }, {} },
                { "LOOP VERSE, BASS CLIMB EVERY BAR", { "RUNG CHORD, MUTED EIGHTHS AFTER", "OUT" }, { "ROOT, THEN A CLIMB TO THE NEXT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "LOOP: KICK ONE, TWO-AND, THREE", "LOOP: KICK ONE AND THREE, SIXTEENTH HATS" }, {} },
                { "CHORD ON ONE, MUTED EIGHTH PEDAL", { "DYAD ON ONE, THEN MUTED ROOT EIGHTHS", "POWER CHORD ON ONE, THEN MUTED ROOT EIGHTHS" }, { "ROOT HELD, A PICKUP ON FOUR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "ORGAN STAB ON ONE", "SILENT" }, { "FOUR ON THE FLOOR, RIDE EIGHTHS, SNARE FILL EVERY OTHER BAR", "FOUR ON THE FLOOR, RIDE EIGHTHS" }, {} },
                { "RIFF BAR, HITS BAR", { "SIXTEENTH PEDAL RIFF, THEN CHORD HITS", "PEDAL RIFF, THEN ONE HELD CHORD" }, { "PEDAL EIGHTHS, THEN THE HITS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON THE GROUPS, THEN THE HITS", "BACKBEAT UNDER BOTH" }, {} },
                { "QUIET VERSE: MUTED DYADS, NO SNARE", { "MUTED DYADS ON ONE, ONE-AND AND THE BACK HALF", "ONE CLEAN CHORD RUNG A BAR" }, { "ONE, ONE-AND, TWO-AND, THREE, THREE-AND, FOUR-AND", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "NO SNARE: KICK AND HATS, TOMS EVERY FOURTH BAR", "NO SNARE: HATS AND PEDAL HAT" }, {} },
                { "BASS AND STRINGS STABS IN TURN UNDER PIANO EIGHTHS", { "OUT", "A SOFT HIGH FIFTH EACH BAR" }, { "STABS ON ONE AND ONE-AND", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PIANO EIGHTH TRIADS, STRINGS ANSWERING", "PIANO EIGHTH TRIADS" }, { "OUT", "SWELLS ONLY, NO BEAT" }, {} },
                { "PIANO HALF NOTES, BASS STEPPING BY INVERSIONS", { "OUT", "POWER CHORDS ON THE LAST BARS" }, { "HALF NOTES STEPPING BY INVERSIONS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "PIANO WIDE HALF NOTES", "PIANO HALF NOTES, PADS HELD" }, { "HALF TIME: KICK ON ONE, SNARE ON THREE, QUARTER HATS", "HALF TIME, KICK ON FOUR-AND EVERY OTHER BAR" }, {} },
                { "TOM-GROOVE VERSE, BROKEN POWER CHORDS", { "BROKEN POWER CHORDS IN EIGHTHS", "BROKEN POWER CHORDS, MUTED" }, { "ROOT EIGHTHS UNDER THE TOMS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "FLOOR TOM GROOVE, NO SNARE OR HATS", "FLOOR TOM GROOVE, SNARE ON FOUR" }, {} },
                { "CHUNK BAR, SINGLE-NOTE ANSWER BAR", { "CHUNKS, THEN A SINGLE-NOTE ANSWER", "CHUNKS, THEN THE ANSWER AN OCTAVE UP" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK WITH THE CHUNKS, BACKBEAT", "RIDE BELL QUARTERS, KICK WITH THE CHUNKS" }, {} },
                { "PEDAL EIGHTHS, A TRIAD PULSE IN THE SECOND HALF", { "ROOT PEDAL EIGHTHS, A HIGH TRIAD FROM HALFWAY", "ROOT PEDAL EIGHTHS" }, { "ROOT EIGHTHS, ACCENTED BEATS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "TRIAD PULSE FROM HALFWAY", "SILENT" }, { "EIGHTH HATS, KICK ONE AND THREE", "TAMBOURINE AND KICK QUARTERS" }, {} },
                { "FOUR ON THE FLOOR, ONE CHIMING NOTE", { "ONE CHIMING NOTE IN EIGHTHS", "ONE CHIMING NOTE, A SECOND GUITAR OFF THE ONE" }, { "ROOT EIGHTHS UNDER THE CHIME", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND HAT QUARTERS, TAMBOURINE, A TOM RUN EVERY FOUR", "KICK QUARTERS, EIGHTH HATS" }, {} },
                // (0.234.0) Round nine's verses.
                { "MACHINE LOOP, BASS RIFF, GUITARS OUT", { "OUT", "A FEEDBACK NOTE HELD" }, { "QUARTER-NOTE RIFF, TWO BARS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "ONE DRONE NOTE, THEN OUT", "SILENT" }, { "MACHINE LOOP, HOLES FILLED", "MACHINE LOOP, PLAIN SIXTEENTHS" }, {} },
                { "BASS ON THE CHORUS RIFF, FOUR ON THE FLOOR", { "OUT, TRIAD STABS FROM HALFWAY", "A FEEDBACK CHORD, FOUR BARS" }, { "THE RIFF IN EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "A HIGH SWELL EVERY FOUR BARS" }, { "FLOOR AND SIXTEENTH HATS, NEVER A FILL", "FLOOR AND EIGHTH HATS, NEVER A FILL" }, {} },
                { "HIT ON ONE, PICKUP CHORDS ON FOUR", { "HIT ON ONE, TWO PUSHED CHORDS ON FOUR", "HIT ON ONE, ONE PUSH" }, { "BUSY ROOTS WITH THE KICK", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "ORGAN ON THE HITS", "SILENT" }, { "GLAM STOMP, OPEN HAT EIGHTHS", "GLAM STOMP, CLOSED HATS" }, {} },
                { "SYNTH OCTAVE BURSTS, GUITARS AND HATS OUT", { "OUT", "HIGH NOTE ON TWO AND FOUR" }, { "ROOT AND FLAT SEVEN ROCKING", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "OCTAVE BURSTS ON ONE", "OCTAVE BURSTS ON ONE AND THREE" }, { "KICK AND SNARE, NO HATS", "KICK AND SNARE, RIDE BELL QUARTERS" }, {} },
                { "RIDE ONLY, BOOMS AND NOISE", { "CLEAN LONG CHORD TONES, THEN SHORT EIGHTHS", "OUT" }, { "QUARTER ROOTS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "LOW BOOMS AND NOISE STABS", "LOW BOOMS ONLY" }, { "RIDE EIGHTHS, PEDAL HAT ON ONE AND THREE", "RIDE EIGHTHS ALONE" }, {} },
                { "BASS AND ORGAN QUARTER RIFF, TWO STABS", { "OUT", "STABS WITH THE SYNTH" }, { "QUARTER RIFF, THE LAST CHANGE PUSHED", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "ORGAN DOUBLES, STABS ON ONE AND TWO", "ORGAN DOUBLES ONLY" }, { "FOUR ON THE FLOOR, NO HATS, CLAPS ON THE STABS", "FOUR ON THE FLOOR, NO HATS" }, {} },
                { "HIGH GUITARS CARRY IT, BASS OUT", { "HIGH CHORDS IN EIGHTHS, TWO GUITARS", "HIGH CHORDS IN EIGHTHS" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SPARSE BAR, BUSY BAR, CLOSED HATS", "STRAIGHT KICK, CLOSED HATS" }, {} },
                { "SYNTH-BASS LOOP UNDER HALF TIME", { "OUT", "SPARSE HELD NOTES" }, { "INTERLOCKING TWO-BAR LOOP", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STRINGS ONE NOTE PER TWO BARS", "HELD PADS" }, { "HALF-TIME LOOP, QUARTER HATS", "HALF-TIME LOOP, OPEN QUARTERS" }, {} },
                { "BASS AND ONE MUTED CHUG, DRUMS OUT", { "MUTED SIXTEENTHS ROCKING A HALF STEP", "MUTED SIXTEENTHS ON THE ROOT" }, { "ROOTS ON ONE, ONE-AND, THREE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "OUT", "ONE KICK A BAR" }, {} },
                { "SOFT CHORD BAR, DOTTED OCTAVE PINGS", { "SOFT CHORD, THEN OCTAVE PINGS", "OCTAVE PINGS EVERY BAR" }, { "ROOTS IN THE CHORD BAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SPARSE KICK, GHOSTS, LOUD-SOFT HATS", "SPARSE KICK, PLAIN HATS" }, {} },
                // (0.181.0) Round six's verses.
                { "POWER CHORD PAIRS, MUTED OCTAVES IN THE GAPS", { "CHORD PAIRS, MUTED OCTAVES", "CHORD PAIRS ONLY" }, { "PAIRS UNDER THE CHORDS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK PAIRS, CRASH ON EVERY KICK", "KICK PAIRS, BACKBEAT ONLY" }, {} },
                { "OCTAVE LINE THROUGH DEAD STRUMS", { "OCTAVES AND DEAD STRUMS", "OCTAVE LINE ALONE" }, { "WALKING DOWN TO THE FIFTH", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SYNCOPATED KICK, SNARE ANSWERS", "BACKBEAT, CLOSED HATS" }, {} },
                { "BASS HOLDS, LEAD EIGHTHS, SNARE ON THE ONE", { "OUT", "CHORD ON ONE, LET RING" }, { "DYAD HELD, A HALF STEP DOWN", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "HIGH LINE, QUARTERS THEN EIGHTHS", "HELD PADS" }, { "SNARE ON THE ONE", "SNARE ON ONE, PEDAL HAT EIGHTHS" }, {} },
                { "ONE-STAB CALL OVER A SINGLE-NOTE RIFF", { "SINGLE-NOTE RIFF, STAB CALL", "SINGLE-NOTE RIFF ALONE" }, { "ROOT QUARTERS, THEN EIGHTH PAIRS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "FOUR ON THE FLOOR, CRASH ON BEAT FOUR", "FOUR ON THE FLOOR, BACKBEAT" }, {} },
                { "HIGH STACCATO CHORD EIGHTHS, KICK ON ONE AND THREE", { "HIGH STACCATO TRIADS", "HIGH TRIADS ON THE OFF-BEATS" }, { "OUT", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "STEPWISE LINE", "HELD PADS" }, { "KICK ON ONE AND THREE, NO SNARE", "KICK HALF NOTES, RIDE BACKBEAT" }, {} },
                { "LOW ROOT EVERY THIRD EIGHTH, HALF TIME", { "LOW ROOT EVERY THIRD EIGHTH", "THIRD-EIGHTH CYCLE, POWER CHORDS" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "DOTTED KICK, SNARE ON THREE, PEDAL HAT", "HALF TIME" }, {} },
                { "CLEAN RIFF, HELD BASS, GHOSTED HIP-HOP BEAT", { "CLEAN SIXTEENTH FIGURE", "CLEAN FIGURE, SECOND BAR RUNG" }, { "ROOTS ON ONE AND TWO-AND", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "GHOSTED HIP-HOP BEAT", "HIP-HOP BEAT ON THE RIDE" }, {} },
                { "TWO CHORDS, TWO OPEN PEDALS", { "CHORD, CHORD, PEDAL, PEDAL", "CHORD AND PEDAL IN EIGHTHS" }, { "CHORD ROOTS IN EIGHTHS", "TONIC PEDAL HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "BACKBEAT, CLOSED HATS", "ROCK BEAT, CLOSED HATS" }, {} },
                { "DOTTED CHORDS, SNARE ON TWO, THREE-E AND FOUR-A", { "DOTTED CHORDS, ROOT SIXTEENTHS", "DOTTED CHORDS, FLAT-SIX ANSWER" }, { "WITH THE DOTTED CHORDS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "DOTTED KICK, SNARE ON TWO, THREE-E, FOUR-A", "BACKBEAT, CLOSED HATS" }, {} },
                { "CLEAN TWO-NOTE SIXTEENTHS OVER A BASS RIFF", { "CLEAN ROCKING SIXTEENTHS", "CLEAN ROCKING EIGHTHS" }, { "HELD ROOT, FIVE SHORT NOTES", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SIXTEENTH HATS, OPEN ON TWO-A", "BACKBEAT, CLOSED HATS" }, {} },
                { "HIGH OPEN-PEDAL RIFF OVER A SIXTEENTH ROOT", { "HIGH RIFF OVER THE PEDAL", "HIGH RIFF ALONE" }, { "OUT", "ROOT SIXTEENTHS UNDER IT", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON ONE-E, SNARE ON THE ANDS", "BACKBEAT, CLOSED HATS" }, {} },
                { "KICK AND HAT EIGHTHS, BASS ALONE", { "OUT", "OUT, THEN EIGHTHS FROM HALFWAY" }, { "ROOT SIXTEENTHS STEPPING DOWN", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK AND HATS ON EVERY EIGHTH", "KICK EIGHTHS, THEN THE BACKBEAT" }, {} },
                { "CLIPPED EIGHTH CHORDS UNDER A SINGLE-NOTE LINE", { "CLIPPED CHORDS, SINGLE-NOTE LINE", "CLIPPED CHORDS ALONE" }, { "ROOT SIXTEENTHS STEPPING DOWN", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK RUNS ON ONE AND THREE, BACKBEAT", "BACKBEAT, CLOSED HATS" }, {} },
                { "FIVE PICKS AND A GAP", { "FIVE PICKS, SINGLE NOTES", "FIVE PICKS, POWER CHORDS" }, { "WITH THE FIRST OF EACH FIVE", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON THE FIVES, SNARE ON TWO AND THREE", "HALF TIME" }, {} },
                // (0.147.0) Round four's verses.
                { "HOOK OVER THE BEAT, GUITARS OUT", { "OUT", "PICKUP CHORD EVERY FOURTH BAR" }, { "WHOLE NOTES", "PICKUPS ONLY", "LOCKED WITH THE KICK" },
                  { "TWO-NOTE EIGHTH OSTINATO", "STAB HOOK ON 4+2+4+2+4", "HELD PADS" }, { "SPARSE KICK BACKBEAT", "SNARE ON TWO AND THE AND OF THREE" }, {} },
                { "SEQUENCER VERSE, NO KIT", { "ONE CHORD HELD THREE BARS", "OUT" }, { "WHOLE NOTES", "EIGHTHS", "LOCKED WITH THE KICK" },
                  { "ARPEGGIATOR", "HELD PADS" }, { "CLICKS ONLY", "KICK QUARTERS AND TOMS, NO SNARE" }, {} },
                { "HALF-STEP DROP PAIRS", { "DROP PAIRS", "ANCHORS AND A FLAT-TWO BURST" }, { "OWN SIXTEENTH RIFF", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK BURSTS, PEDAL HAT", "KICK ON THE ANCHORS" }, {} },
                { "TOM BURST WITH THE CELL, THEN SILENCE", { "THREE-HIT CELL, THEN SILENCE", "CELL EVERY BAR" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "TOMS UNDER THE CELL", "DISPLACED, RIDE ON THE OFF-BEATS" }, {} },
                { "TWO-NOTE SEESAW IN EIGHTHS", { "SEESAW", "SEESAW OVER A HELD DYAD" }, { "KICK RIFF", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK WITH THE BASS RIFF, SNARE ON THREE", "SIDESTICK AND HATS ON THE RIFF" }, {} },
                { "OFF-BEAT STABS WITH THE SNARE", { "OFF-BEAT STABS", "STABS ON THE ANDS AND THE FOUR" }, { "EIGHTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "OSTINATO OVER", "SILENT" }, { "SNARE WITH THE STABS", "HIP-HOP BREAK" }, {} },
                { "TRILL HOOK OVER HATS", { "TRILL HOOK", "TRILL OVER THE CHUG CELL" }, { "OUT", "DOUBLES THE GUITAR", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "HATS ONLY", "KICK ON THE CHUGS" }, {} },
                { "HELD CHORD A BAR, TWO HITS EVERY FOURTH", { "HELD A BAR, HITS EVERY FOURTH", "RUNG TWO BARS AT A TIME" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIFF GROOVE UNDER, TOM FILL", "SPARSE KICK BACKBEAT" }, {} },
                { "STRINGS ALONE VERSE", { "OUT", "PICKUP CHORD EVERY FOURTH BAR" }, { "OUT", "PICKUP INTO THE NEXT", "LOCKED WITH THE KICK" },
                  { "STRINGS AND A WALKING PAD", "HELD PADS" }, { "OUT, KICK AND TOM ON THREE LAST BAR", "SOFT SNARE QUARTERS" }, {} },
                { "TREMOLO LINE IN GROUPS", { "TREMOLO LINE", "TREMOLO LINE, AN OCTAVE UP" }, { "ROOT SIXTEENTHS", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SYNCOPATED KICK", "HALF TIME" }, {} },
                { "RIFF BARS AND THIN BARS IN TURN", { "RIFF TWO BARS, OUT TWO", "RIFF EVERY BAR" }, { "THE CELL", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "THIN BARS AND TOM RUMBLE", "CELL ALL THROUGH" }, {} },
                // (0.146.0) Verses: the guitar ideas as types of their own.
                { "OCTAVE-JUMP PEDAL", { "OCTAVE-JUMP PEDAL", "OCTAVE-JUMP PEDAL, HIGH HELD LINE" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "SIXTEENTH HATS, KICK WITH THE PEDAL", "HALF TIME, SIXTEENTH HATS" }, {} },
                { "PEDAL 7+3+3+3", { "PEDAL 7+3+3+3, MOVING CHORDS", "PEDAL 7+3+3+3, ONE CHORD" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK WITH THE GROUPS", "ROCK BEAT, CLOSED HATS" }, {} },
                { "DRIVEN ARPEGGIO", { "DRIVEN ARPEGGIO", "DRIVEN ARPEGGIO, LAST BAR RUNG" }, { "3+3+2 EIGHTHS", "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "RIDE QUARTERS, BACKBEAT", "HALF-TIME RIDE" }, {} },
                { "CHUG PAIRS, HIGH PINGS", { "CHUG PAIRS, HIGH PINGS", "CHUG PAIRS ONLY" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK WITH THE CHUG PAIRS", "BACKBEAT, CLOSED HATS" }, {} },
                { "MUTED PEDAL, ACCENTS IN THREES", { "ACCENTS IN THREES", "ACCENTS IN THREES OVER THE KEY'S ROOT" }, { "DOUBLES THE GUITAR", "ROOTS HELD", "LOCKED WITH THE KICK" },
                  { "SILENT", "HELD PADS" }, { "KICK ON THE ACCENTS", "DRIVING EIGHTHS BEAT" }, {} },
            };
            return lists;
        }
    }

    bool isRound3Type (const juce::String& type)
    {
        const auto t = type.trim().toUpperCase();
        for (const auto& list : round3Lists())
            if (t == list.name)
                return true;
        return false;
    }

    juce::StringArray round3Approaches (const juce::String& type, ComposerLayer layer)
    {
        using L = ComposerLayer;
        const auto t = type.trim().toUpperCase();
        for (const auto& list : round3Lists())
            if (t == list.name)
                switch (layer)
                {
                    case L::guitar: return list.guitar;
                    case L::bass: return list.bass;
                    case L::chords: return list.keys;
                    case L::drums: return list.drums;
                    case L::lead: return list.lead;
                    case L::acoustic: return list.acoustic;   // (0.194.0: the acoustic-led types)
                    default: return {};
                }
        return {};
    }

    juce::StringArray preChorusApproaches (const juce::String& type, ComposerLayer layer)
    {
        if (isRound3Type (type))
            return round3Approaches (type, layer);
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
        {
            // (0.123.0) The rock songs' pre-choruses.
            static const juce::StringArray bassList { "DOUBLES THE GUITAR", "ROOT QUARTERS", "LOCKED WITH THE KICK" };
            struct Lists { const char* name; juce::StringArray guitar, keys, drums, lead; };
            static const std::vector<Lists> lists {
                { "SHIFTING BURSTS, KICK JOINS", { "SHIFTING BURSTS", "BURSTS EVERY BAR" }, { "SILENT", "LOW DRONE" },
                  { "HALF TIME, THEN THE KICK JOINS", "KICK WITH THE BURSTS" }, { "SILENT", "LONG NOTES" } },
                { "SNARE CARPET BUILD", { "ROOT EIGHTHS", "HELD CHORDS" }, { "SILENT", "HELD PADS" },
                  { "SNARE CARPET, THEN THE GROOVE", "SNARE CARPET ALL THROUGH" }, {} },
                { "PEDAL WITH OPEN-HAT BEATS", { "SIXTEENTH PEDAL, CLIMBING", "EIGHTH PEDAL" }, { "SILENT", "LOW DRONE" },
                  { "OPEN HATS ON THE BEATS", "HALF TIME" }, {} },
                { "CHROMATIC CHUG, TREMOLO TRIAD", { "CHUG WITH THE HALF STEP", "CHUG ON THE ROOT" }, { "SILENT", "HELD PADS" },
                  { "GROOVE, THEN OFF-BEAT KICKS", "OFF-BEAT KICKS ALL THROUGH" }, { "TREMOLO TRIAD", "SILENT" } },
                { "OFF-BEAT SNARE, LOW BURSTS", { "LOW BURSTS", "RUNG BAR, SILENT BAR" }, { "SILENT", "HELD PADS" },
                  { "SNARE ON THE OFF-BEATS", "WITH EIGHTH CRASHES" }, {} },
                { "THREES ACROSS THE BAR", { "QUARTER AND FOUR EIGHTHS", "PEDAL WITH A FALLING NOTE" }, { "SILENT", "HELD PADS" },
                  { "THREE-BEAT CELLS", "STRAIGHT HALF TIME" }, {} } };
            for (const auto& t : lists)
                if (type == t.name)
                    switch (layer)
                    {
                        case L::guitar: return t.guitar;
                        case L::bass: return bassList;
                        case L::chords: return t.keys;
                        case L::drums: return t.drums;
                        case L::lead: return t.lead;
                        case L::melody: break;
                    }
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

    static juce::StringArray ownSectionApproaches (const ComposerState& s, ComposerLayer layer)
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
        if (const auto t = sectionTypeCalled (s.sectionType); t.has_value() && t->band == SectionType::Band::verse)
            return verseApproaches (t->name, layer);
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

    // (0.161.0, the owner: "add the type-bound versions to their own types too") Round four's versions that only make
    // sense inside one type (the readers' notes on the owner's songs, shapes only, our names), added to that type's own
    // list: a bass option in front of LOCKED WITH THE KICK, any other after the type's own. Written by
    // `typeVersionLayer` (Composer.cpp), before the type's own writer.
    struct TypeVersion { const char* type; ComposerLayer layer; const char* name; };
    static const std::vector<TypeVersion>& typeVersions()
    {
        static const std::vector<TypeVersion> list {
            { "INTRO RETURNS, LAST CHORD RINGS", ComposerLayer::guitar, "THEME, THEN ONE LAST HIT" },
            { "INTRO RETURNS, LAST CHORD RINGS", ComposerLayer::chords, "THE HYMN ALONE AT THE END" },
            { "DEAD STOP", ComposerLayer::guitar, "FOUR EIGHTHS, THEN SILENCE" },
            { "HOOK ALONE TO THE END", ComposerLayer::chords, "HOOK WITH A ONE-BAR BREAK" },
            { "RIFF ALONE, THEN THE BAND", ComposerLayer::drums, "GHOST NOTES UNDER THE RIFF" },
            { "RING, THEN ANSWER", ComposerLayer::guitar, "ANSWER THROUGH THREE CHANGES" },
            { "DOTTED STREAM", ComposerLayer::guitar, "DOTTED, ENDING 3+3+3+3+4" },
            { "HELD CHORDS IN 3+3+2", ComposerLayer::guitar, "THREE CHORDS, THEN A HELD BAR" },
            { "CHROMATIC CHORDS UNDER A FIXED TREMOLO", ComposerLayer::drums, "DOTTED KICKS, CRASH QUARTERS" },
            { "HOOK KEPT, BAND BUILT UNDER IT", ComposerLayer::guitar, "THE BAND IN AT ONCE" },
            { "HOOK KEPT, BAND BUILT UNDER IT", ComposerLayer::guitar, "CHUG CELL OPENING ON A CLIMB" },
            { "KEYS ALONE, THEN RINGING HITS", ComposerLayer::bass, "IN AFTER THE DRUMS" },
            { "OCTAVE-JUMP PEDAL", ComposerLayer::guitar, "OCTAVE-JUMP PEDAL, FLAT-SEVEN FLIPS" },
            { "HOLED SIXTEENTH PEDAL", ComposerLayer::drums, "KICK ON EVERY NOTE, TOMS IN THE HOLES" },
            { "SNARE CARPET BUILD", ComposerLayer::drums, "SNARE CARPET, NO CRASH" },
            { "UNISON ROCKING RIFF", ComposerLayer::guitar, "UP A HALF STEP IN 2+1+3" },
            // (0.195.0) Round seven: the chorus's power-chord eighths with full chords on the first, fourth and seventh.
            { "DRIVING", ComposerLayer::guitar, "FULL CHORDS ON 1, 4 AND 7" },
            // (0.199.0) Round seven: each chord pushed onto the and of four before its bar and rung the whole bar.
            { "DRONING", ComposerLayer::guitar, "PUSHED WHOLE-BAR CHORDS" },
        };
        return list;
    }

    juce::StringArray typeVersionsOf (const juce::String& type, ComposerLayer layer)
    {
        juce::StringArray out;
        for (const auto& v : typeVersions())
            if (v.layer == layer && type.trim().equalsIgnoreCase (v.type))
                out.add (v.name);
        return out;
    }

    static juce::StringArray baseSectionApproachesUncached (const ComposerState& s, ComposerLayer layer)
    {
        // (0.219.0) A lane in another WORLD lists that world's parts, whatever the type.
        if (layer == ComposerLayer::chords && s.lane (layer).world == (int) LaneWorld::cyberpunk)
            return cyberpunkKeysOptions();
        auto names = ownSectionApproaches (s, layer);
        if (names.isEmpty())
            return names;
        const auto versions = typeVersionsOf (s.sectionType, layer);
        if (versions.isEmpty())
            return names;
        if (layer == ComposerLayer::bass && names[names.size() - 1] == "LOCKED WITH THE KICK")
        {
            names.remove (names.size() - 1);
            names.addArray (versions);
            names.add ("LOCKED WITH THE KICK");
            return names;
        }
        names.addArray (versions);
        return names;
    }

    static juce::StringArray extraApproachesUncached (ComposerLayer layer)
    {
        // (0.145.0) From the owner's 61 song files (shapes only, our names), offered under every named section type after
        // its own options.
        if (layer == ComposerLayer::drums)
            return { "DOUBLE-TIME PUNK BEAT", "SNARE ON EVERY BEAT, KICKS BETWEEN", "CRASH EIGHTHS, FULL TIME", "PHRASE-END SNARE RUN",
                     "ELECTRONIC HALF-TIME LOOP", "TOM RUN EVERY BAR", "TOM ROLL CARPET", "INDUSTRIAL BROKEN BEAT", "DISPLACED BACKBEAT",
                     // (0.155.0) Round four, the owner's 54 more song files.
                     "HIP-HOP BREAK", "SNARE ON TWO AND THE AND OF THREE", "KIT ON ONE AND THREE", "SNARE MARCH, CRASH EVERY BAR",
                     "SOFT SNARE QUARTERS", "CRASH QUARTERS BUILD", "OFF-SIXTEENTH SNARE", "BREAKBEAT",
                     "FOUR ON THE FLOOR, CRASH ON TWO THREE FOUR", "HIP-HOP BACKBEAT WITH A GHOST", "PEDAL HAT SIXTEENTHS UNDER THE BEAT",
                     // (0.159.0) Round four's new versions of drum parts we have.
                     "DOTTED KICK CHAIN, CRASH QUARTERS", "PULSE, SNARE CREEPS IN", "SPLASH QUARTERS, SNARE ON THREE",
                     "LOOP AND SIDESTICK SIXTEENTHS", "DISPLACED SNARE, RIDE BELL", "BROKEN BEAT, CRASH EIGHTHS",
                     "LAYERED DOUBLE KICK", "THE STOMP JOINS", "CARVED KICK, CHINA SIXTEENTHS",
                     "QUARTER HATS, BUSY KICK", "TOM GROOVE, SNARE ON THREE", "SNARE ON THE A OF TWO, CRASH EIGHTHS",
                     // (0.172.0) Round five (the owner's 12 songs).
                     "CRASH ON THE KICKS, RIDE BETWEEN", "HAND ON THE BEATS, FOOT ON THE ANDS", "SNARE ON THE DOTTED CHAIN, KICKS BETWEEN",
                     "SNARE DOUBLES THE UNISON CELL", "BACKBEAT, HAT RUFF EVERY SECOND BAR", "CRASH AND CHINA EIGHTHS, SNARE THICKENS",
                     "RIDE ON THE DOTTED LINE",
                     // (0.186.0) Round six (the owner's 25 songs); two are other types' own options, shared as they are.
                     "FOUR ON THE FLOOR, CRASH ON BEAT FOUR", "KICK AND TOM QUARTERS, EIGHTHS ON THREE", "SHAKER THREE-EIGHTH PAIRS",
                     "BACKBEAT DOUBLED ON THE E", "KICK BURSTS, BACKBEAT", "SNARE ON TWO, THREE-E, FOUR, FOUR-A",
                     "HALF-TIME HATS WITH A FOUR-BAR KICK CYCLE",
                     // (0.193.0) Round seven (the owner's 70 songs): under acoustic guitars.
                     "TAMBOURINE ON TWO AND FOUR ONLY", "SHAKER AND HATS, KICK EVERY FOURTH BAR", "HALF TIME WITH TAMBOURINE AND SHAKER",
                     "SHAKER SIXTEENTHS, CABASA, BACKBEAT", "RIDE EIGHTHS, PEDAL HAT, SHAKER", "FLOOR TOM IN 3+3+2, NO KICK",
                     "PEDAL HAT, DOTTED KICK, LATE SNARE", "MARACAS AND CONGAS IN THREES", "SIDESTICK AND SHAKER",
                     "BONGOS ON THE GUITAR'S RHYTHM", "TAMBOURINE EIGHTHS OVER THE BACKBEAT",
                     // (0.195.0) Round seven's choruses: the kick with an acoustic's strum.
                     "KICK ON THE STRUM",
                     // (0.199.0) Round seven's electric songs. Five are other types' own options (the same notes), shared as they are.
                     "KICK BURST, CRASH QUARTERS, CHINA ON FOUR", "KICK, SNARE AND OPEN HAT TOGETHER ON TWO AND FOUR",
                     "KICK AND HATS, BACKBEAT MOSTLY GONE", "SNARE QUARTERS, KICK ON THE LAST TWO ANDS", "KICK A SIXTEENTH BEFORE EACH SNARE",
                     "CYMBAL STABS WITH THE HIGH STABS", "KICK AND OPEN HATS ON EVERY EIGHTH, NO SNARE", "DANCE-PUNK BEAT",
                     "SNARE PICKUP INTO TWO AND FOUR, PEDAL KICK", "KICK AND TOM, SNARE AND CLAP ON THREE",
                     // (0.218.0) Round eight (the owner's 67 songs).
                     "OPEN HATS ON TWO, THREE AND FOUR, KICK ON THE RIFF", "SNARE ON ONE, TWO-AND AND FOUR", "CRASH AND SNARE ON THE DOTS, TOMS IN THE GAPS",
                     "KICK AND SNARE TOGETHER ON EVERY EIGHTH", "TWO CLOSED, ONE OPEN HAT EACH BEAT", "KICK PAIRS ON THE AND-A",
                     "SNARE ON TWO, A SIXTEENTH TRIPLE INTO THREE", "TOM PAIR INTO EVERY BAR", "TWO FLOOR TOMS, PEDAL HAT, NO KICK",
                     "TAMBOURINE ON THE ANDS, SNARE ON THREE", "HALF TIME, KICK ON TWO AND ITS AND, A PICKUP EVERY BAR", "SNARE ON TWO, THREE-E AND FOUR, CRASH EIGHTHS",
                     "KICK STREAM, TOMS, SNARE ON THE LATE ANDS", "KICK AND SNARE TOGETHER, TOMS ANSWER", "GHOSTS EITHER SIDE OF THREE",
                     "SNARE SIXTEENTHS AROUND TWO AND FOUR", "SNARE ON TWO, TWO-A, THREE-AND AND FOUR", "GROOVE BAR, FILL BAR",
                     "SNARE FOUR-STROKES ON TWO AND FOUR", "GHOST PAIRS INTO THE BACKBEAT AND THE ONE",
                     // (0.237.0) Round nine (the owner's 17 industrial songs); three are other types' own, shared as they are.
                     "SNARE ON THE DOTS, KICKS BETWEEN", "HATS IN THREES", "CRASH QUARTERS, KICK 2+3+3", "TWO-SOUND SIXTEENTH LOOP, NO FILLS",
                     "KICK EIGHTHS, TOMS ON ONE, TWO AND FOUR", "KICK EIGHTHS, TOMS IN THE BACK HALF", "KICK DOUBLED BY THE FLOOR TOM, BUSY",
                     "TOM LOOP, MOVING SNARE CELL", "SPLASH QUARTERS, A ROLL EVERY EIGHT",
                     "MACHINE LOOP, HOLES FILLED", "DISCO HAT: CLOSED ON THE BEAT, GHOST ON THE E, OPEN ON THE AND",
                     // (0.245.0) Round ten (the owner's 56 songs); three are other types' own, shared as they are.
                     "MACHINE BALLAD GROOVE", "SIXTEENTH HATS ACCENTED ON THE ANDS, CONGAS AND SHAKER", "SNARE AND THREE GHOSTS AFTER EACH BACKBEAT",
                     "KICK QUARTERS, SNARE ONLY ON THREE", "KICK AND BELL IN TWOS", "WOODBLOCK SIXTEENTHS", "SNARE AND CLAPS ON TWO, THREE AND FOUR",
                     "COWBELL BACKBEAT", "PEDAL HAT ON THE BEATS, CLOSED PAIRS", "KICK ON ONE AND ITS AND, SNARE ON THREE",
                     "FLOOR-TOM DRIVE UNDER OPEN-HAT EIGHTHS", "SIDESTICK ON TWO, CABASA SIXTEENTHS", "BALLAD HAT ON TWO AND FOUR ONLY",
                     "BALLAD GROOVE, KICK ON ONE AND THE A OF FOUR", "OPEN HAT ON TWO, THREE AND FOUR", "BALLAD RIDE ON THE ANDS",
                     // (0.259.0) Round eleven (the owner's 61 more songs); some are this round's types' own, shared as they are.
                     "HATS ON THE EIGHTHS, RIDE ON TWO-AND AND FOUR-AND", "LAID-BACK LOOP, OPEN HAT ON THREE-AND AND FOUR-AND", "SNARE A SIXTEENTH BEFORE TWO AND FOUR",
                     "TWO TOMS TOGETHER ON TWO AND FOUR", "ONE KICK EVERY TWO BARS, BACKBEAT, QUARTER HATS", "OPEN HAT AND CRASH EIGHTHS, CHINA ON TWO AND FOUR",
                     "SYNCOPATED KICK, SNARE PAIRS AROUND THE BACKBEAT", "HAT KNOCKS ON ONE-A, TWO AND FOUR", "CONGA AND WOODBLOCK ON TWO AND FOUR",
                     "FLOOR TOM WITH THE SNARE ON FOUR", "KICK BURST BAR, THEN KICKS ON THE ANDS", "KICK ON ONE AND THE A'S, RIDE", "CRASH WASH IN BROKEN SIXTEENTHS",
                     "TOMS ON TWO, TWO-AND AND FOUR, NO SNARE", "SIDESTICK BURSTS OF FIVE ON ONE AND THREE", "BALLAD KICK, A PICKUP INTO THREE, SIXTEENTH HATS",
                     "NO KICK, OPEN HATS SKIPPING TWO AND FOUR", "KICK PICKUPS INTO EVERY BEAT, SNARE ECHO", "DOUBLE-KICK PAIRS ON ONE AND THREE",
                     "BACKBEAT, A TOM RUN EVERY SECOND BAR", "KICK ON ONE ONLY, BACKBEAT, SIXTEENTH HATS", "OPEN HAT ON THE A-AND-ONE PAIRS",
                     "LAID-BACK KIT, SNARE ON TWO, ITS AND, AND FOUR", "DOUBLE KICK ON ONE, A TOM ON THREE-A", "CRASH ON NEARLY EVERY EIGHTH" };
        if (layer == ComposerLayer::bass)   // (0.160.0) round four's bass versions: put in front of LOCKED WITH THE KICK
            return { "DOUBLES THE GUITAR'S HIGH NOTES, OCTAVES DOWN", "PLAYS ONLY THE HEAD", "KEEPS THE CHORUS RHYTHM", "CARVED SIXTEENTH LINE",
                     "EIGHTHS, PICKUP INTO EACH BAR", "A NOTE A BAR, FALLING FROM THE KEY'S NOTE", "CHROMATIC EIGHTHS AROUND THE ROOT",
                     "DOTTED FIGURE WITH THE KICK",
                     // (0.186.0) Round six.
                     "DOTTED HALF AND TWO EIGHTHS", "STABS AT EACH GROUP'S START",
                     // (0.193.0) Round seven.
                     "ROOT ON ONE HELD, REPEAT ON FOUR", "ROOT-FIFTH TRESILLO WITH THE TOM", "LATE PUSHES UNDER HELD CHORDS",
                     "WALK-UPS EVERY SECOND BAR",
                     // (0.218.0) Round eight (the owner's 67 songs).
                     "OFF THE ONE, THE NEXT ROOT HELD FROM THREE", "EIGHT-NOTE SIXTEENTH OSTINATO ON THE KEY'S ROOT", "FUNK BASS, TWO SIXTEENTH CLUSTERS",
                     "HELD FROM TWO-AND", "EIGHTHS WITH A FIVE-SIXTEENTH RUN", "ROOT, A BURST, THEN OCTAVE EIGHTHS",
                     // (0.237.0) Round nine; ROOT AND FLAT SEVEN ROCKING is a type's own, shared as it is.
                     "DRONE PULSE, THEN THE RIFF", "ROOT AND FLAT SEVEN ROCKING",
                     // (0.245.0) Round ten; EIGHTHS WITH BEAT THREE EMPTY is a type's own, shared as it is.
                     "ONE, TWO-AND, THREE, FOUR", "LOCKED TO ONE, ONE-AND, THREE", "EIGHTHS WITH BEAT THREE EMPTY",
                     // (0.259.0) Round eleven (the owner's 61 more songs); some are this round's types' own, shared as they are.
                     "ROOT HELD, THREE-NOTE WALK DOWN ON FOUR", "ROOT AND TENTH SIXTEENTHS", "ROOT PAIR ON ONE, THREE NOTES INTO THE NEXT", "ON ONE AND TWO-AND, HELD",
                     "TWO-BAR GROOVE, CHROMATIC LANDING ON THE ROOT", "TWO-CHORD WALKING PAIRS", "CHROMATIC SLIDE DOWN TO THE FIFTH AT THE END",
                     "LOW ROOT AND HIGH OCTAVE IN THREES", "GALLOP ON ONE AND THREE, EIGHTHS BETWEEN", "ROOT HELD A BEAT, THEN EIGHTHS",
                     "SIXTEENTH PEDAL, A RUN TO THE BAR LINE" };
        if (layer == ComposerLayer::guitar)   // (0.158.0) round four's riff kinds
            return { "CHUGS, THEN THREE CHORDS", "HIT, THEN BURSTS ON TWO AND FOUR", "PUSHED POWER CHORDS WITH A FLAT-TWO STAB",
                     "ROOT TWICE, TRITONE PUSHED IN", "SUS2 SPLIT BETWEEN TWO GUITARS", "TONIC WALL UNDER MOVING CHORDS",
                     // (0.159.0) Round four's new versions of riffs we have.
                     "CHORDS IN SIXTEENTHS UNDER A FIXED HIGH NOTE", "TRESILLO FOURTH DYADS", "FIXED LOW CLUSTER IN SIXTEENTHS",
                     "HIGH TRITONE EIGHTHS OVER RUNG CHORDS", "FOURTH DYADS ROCKING A HALF STEP", "CHUGS WITH A SIXTH",
                     "ROOT TO FIFTH IN THE BAR", "RIFF CELL TWICE A BAR", "DYAD STREAM, ROOT STEPPING EACH BAR",
                     "SIXTEENTH PEDAL OVER LATE HITS", "RUNG SYNCOPATION, TONIC AND HALF STEP", "DOTTED CHORDS, HIGH OCTAVE FIGURE",
                     "PEDAL WALL, CHORDS MOVING ON TOP", "ONE-NOTE SCRATCH RHYTHM", "PEDAL-TOP FINGERPICKING",
                     "FIXED FIGURE OVER HALF TIME", "TWO GUITARS IN TWO RHYTHMS", "ROCKING UP A STEP IN THREES",
                     "UNISON CELL ACROSS THE BAR", "DOUBLE HITS WITH SCRATCHES", "PEDAL IN THREES, CLIMBING",
                     // (0.172.0) Round five's versions of riffs we have.
                     "HALF-STEP DROP IN THREES", "SIXTEENTH NEIGHBOURS, CLASH AT THE END", "PEDAL RIFF WITH A CHROMATIC PICKUP",
                     "FLAT-FIVE CHORD HAMMERED", "CHROMATIC QUARTET INTO THE TONIC", "OCTAVE POWER CHORDS IN 3+3+2",
                     "CLEAN HELD CHORD, PUSHED INTO FOUR", "FLAT-THREE OCTAVE BURST EVERY FOURTH BAR", "RING THE FLAT TWO",
                     "HIGH NOTE SEESAW, TWO BARS EACH", "FOUR-NOTE CHROMATIC CELL, OCTAVE DYADS", "OUT, THEN THE BASS'S ROCKING LINE",
                     // (0.186.0) Round six's versions of riffs we have.
                     "ROOT TREMOLO EVERY OTHER BAR", "TRADED SINGLES BETWEEN TWO GUITARS", "HIGH DYAD TREMOLO OVER STAGGERED PAIRS",
                     // (0.193.0) Round seven: an electric guitar over an acoustic's strum.
                     "POWER-CHORD QUARTERS OVER THE STRUM", "THE VERSE'S STRUM, DISTORTED",
                     // (0.217.0) Round eight (the owner's 67 songs): riff shapes, clean parts and high lines for the guitar lane.
                    "ROOT HELD, SEVEN-NOTE RUN DOWN", "HIGH SECOND DYAD, THREE EIGHTH RESTRIKES", "OPEN ROOT SIXTEENTHS BETWEEN TWO SHAPES",
                    "TRIADS UP THE SCALE IN QUARTERS", "THE CHORD'S THIRD AND FIFTH ON THREE", "BROKEN POWER CHORD IN EIGHTHS",
                    "CHORD, CHORD, TOP, FIFTH", "EIGHTHS WITHOUT ONE AND TWO-AND", "TENTH DYAD EIGHTHS", "DRONE CHORD, TOP NOTE CLIMBING",
                    "A LINE AND ITS ECHO A BEAT LATER", "EIGHTHS WITH BEAT THREE EMPTY, THE NEXT BAR TIED", "HIGH ENTRIES A BEAT APART",
                    "CHORD STRUM, HIGH SEVENTH DYAD RINGS", "A QUARTER, THEN FIVES", "CHANGE ON TWO-A", "HIGH LINE STEPPING DOWN OVER EIGHTHS",
                    "FOUR CHORDS ON ONE AND THE THREE ANDS", "SIXTEENTH STRUM, A PASSING CHORD ON FOUR", "THREE-NOTE CELLS UNDER A FIXED TOP",
                    "HIGH ROOT ON THE BEATS, HALF STEP UNDER", "HIGH ROOT AND NINTH-FIFTH IN SIXTEENTHS", "PICKED 2+1 CELL INTO A HELD OCTAVE",
                    "ROOT-AND-THIRD STRUM IN SIXTEENTHS", "SIXTEENTH CHAIN, CHANGES ON THE E'S", "POWER-CHORD EIGHTHS, ADD-NINE CLUSTER ON FOUR",
                    "UNISON ON ONE, TWO, TWO-A, THREE-AND, FOUR, FOUR-E", "PUSHED CHORD RUNG THREE BARS", "TWO CHORDS EVERY THREE SIXTEENTHS",
                    "ROOT SIXTEENTHS, THE CHORD AT THE BAR'S END", "ROOT-FIFTH PICKED EIGHTHS, TWO-BAR ACCENTS", "HITS ON 1, 2, 2-AND, 4, 4-AND",
                    "LATE-ENTRY ARPEGGIO", "HIGH NINTH-ELEVENTH DYAD, A PEDAL BAR", "SIX-SIXTEENTH CELL, TOP AGAINST THE PEDAL",
                    "HIGH TREMOLO FOR HALF, THEN ROOT EIGHTHS", "TWO SHAPES IN EIGHTHS, HOLES MOVING", "CHORD, ROOT RUN, FLAT-FIVE TAIL",
                    "THREE CHORD HITS, A LINE DOWN", "CHROMATIC POWER CHORDS ROUND THE ROOT IN SIXTEENTHS", "PEDAL AND FALLING TOP, ROCKING",
                    "OPEN TOP STRINGS OVER A MOVING LOW NOTE", "WHOLE-STEP-UNDER BRACKET CHORDS", "ROOT, FLAT THREE, FOUR, FLAT FIVE WALK",
                    "HIGH THREE-NOTE CELLS OVER THE RIFF", "DYADS STRUCK THREE TIMES, STEPPING DOWN",
                    // (0.237.0) Round nine (the owner's 17 industrial songs); HIGH NOTE ON TWO AND FOUR is a type's own, shared as it is.
                    "POWER-CHORD BAR, BROKEN-OCTAVE BAR", "LOW CHUG ON THE BASS FIGURE", "CHORD, HIGH NOTE, MINOR DYAD IN EIGHTHS",
                    "TRIAD STABS ON TWO AND FOUR", "HOLED SINGLE-NOTE LINE, OCTAVE HELD EVERY FOURTH BAR", "TAKES OVER THE KEYS' OSTINATO",
                    "HIGH NOTE ON TWO AND FOUR",
                    // (0.245.0) Round ten (the owner's 56 songs); three are types' own, shared as they are.
                    "CHORDS ON TWO AND FOUR OVER THE BASS", "FULL CHORDS: EIGHTHS, THEN THE ANDS", "RING, THEN EIGHTHS, TWO-BAR CELL",
                    "RING ON ONE, MUTED CHUGS OFF THE BEAT", "HITS ON ONE, THREE, FOUR AND FOUR-AND", "CHUNKS, THEN A SINGLE-NOTE ANSWER",
                    "MUTED SIXTEENTH RUNS THROUGH THE CHORD",
                     // (0.259.0) Round eleven (the owner's 61 more songs); some are this round's types' own, shared as they are.
                     "HIGH DYADS AROUND THE BEATS", "LOW OCTAVES ON THE EIGHTHS, A TWO-NOTE CLIMB", "ROOT, FIFTH, HELD OCTAVE, IN FOURS", "OCTAVE FLIPS IN FOURS",
                     "QUIET MUTED CHUG RUNS ON THE ROOT", "PUSHED EIGHTH CHORDS, A REST ON THREE", "SIXTEENTH CHUG WITH ACCENTED PAIRS",
                     "STABS ON ONE, TWO, TWO-AND, THREE, FOUR, FOUR-AND", "OCTAVE PICKUPS INTO EVERY BEAT", "CHORD PUSHED ON TWO-AND, RUNG TO THE BAR LINE",
                     "MUTED FULL-CHORD STABS ON TWO, TWO-AND AND FOUR" };
        if (layer == ComposerLayer::chords)
            return { "SYNTH BASS SIXTEENTHS", "STRINGS ON THE QUARTERS", "PIANO ARPEGGIO IN 3+3+2", "PIZZICATO ARPEGGIO", "TIMPANI ROLL RISER",
                     // (0.155.0) Round four.
                     "ONE-NOTE SIXTEENTH LOOP", "ECHOING PLUCK", "DYAD PULSE IN SIXTEENTHS", "RIGHT HAND AFTER THE DOWNBEAT",
                     "LEFT HAND ON TWO AND FOUR", "HALF-BAR BROKEN CHORD", "SYNCOPATED PIANO FIGURE", "STRINGS EIGHTHS OVER A HELD PAD",
                     "TWO SYNTHS IN CALL AND ANSWER", "OFF-BEAT SYNTH STABS", "STRINGS SIXTEENTH PEDAL IN THREES",
                     "CHOIR STABS ON THE DOTTED GRID", "TRIAD ARPEGGIO IN SIXTEENTHS", "HIGH STAB ON TWO AND FOUR", "FIXED HIGH DYAD IN SIXTEENTHS",
                     // (0.159.0) Round four's new versions of keys parts we have.
                     "FIXED TOP, ROOT UNDER IT", "FALLING FOUR-NOTE CELL", "CHOIR STABS ON THE ACCENTS",
                     "STRINGS QUARTER, EIGHTH, EIGHTH", "STRINGS DOTTED-QUARTER LINE", "HIGH STAB HOOK, FIXED",
                     "FIVE-NOTE CHORDS ON ONE AND THREE", "HIGH STRINGS MOTIF", "TONIC AND STEP SIXTEENTH LOOP",
                     "PIANO TRIADS ON THE QUARTERS", "TONIC EIGHTH PULSE", "PAD CHORDS IN HALF NOTES",
                     // (0.172.0) Round five.
                     "HIGH COLOUR TONE", "HIGH TONIC PEDAL",
                     // (0.186.0) Round six.
                     "SIXTEENTH PIANO PULSE", "STEPPING EIGHTHS INTO A HELD NOTE, OCTAVES", "DOTTED CALL, PUSHED ANSWER",
                     "STACCATO ONE-NOTE EIGHTHS OVER OCTAVE ROOTS", "PUMP BETWEEN TWO CHORDS", "OCTAVE ROOT AND TOP, CHORDS ON THE ANDS",
                     // (0.193.0) Round seven.
                     "STRINGS HALF NOTES WITH THE STRUM", "OCTAVE COUNTERLINE", "CELLO DOUBLES THE RIFF", "STRINGS PULSE ON THE ROOT",
                     "BELL DOUBLES THE GUITAR",
                     // (0.199.0) Round seven: a type's own plucked ostinato, shared as it is.
                     "PLUCKED OSTINATO IN OCTAVES",
                     // (0.218.0) Round eight (the owner's 67 songs).
                     "FALLING SCALE EIGHTHS, A HELD TOP", "PIANO STABS IN 3+2+3", "PULSE ON EVERY EIGHTH BUT THE ONE",
                     "OFF-BEAT BELLS, FOUR VOICES ROTATING", "DYAD PICKUP PHRASE EVERY FOUR BARS", "BROKEN TRIAD EACH HALF BAR",
                     "SIXTEEN-SIXTEEN-EIGHTH CELL, FIXED NOTES", "LEFT HAND HELD, RIGHT-HAND FIVE-NOTE RUN EACH HALF",
                     // (0.237.0) Round nine (the owner's 17 industrial songs); LOW BOOMS AND NOISE STABS is a type's own, shared as it is.
                     "SAW DOUBLES THE GUITAR CHUG", "NOISE HITS ON TWO AND FOUR", "ORCHESTRA HIT BURSTS", "MUSIC BOX UP-ARPEGGIO",
                     "THE BAR'S ROOT IN EIGHTHS, HIGH", "SYNTH CALLS ON 2, 3, 3-AND, 4-AND", "TRITONE SEESAW IN EIGHTHS",
                     "QUARTER-NOTE CHORD-TONE CYCLE, HIGH", "LOW BOOMS AND NOISE STABS",
                     // (0.245.0) Round ten (the owner's 56 songs); KEYS ON TWO, TWO-AND, THREE is a type's own, shared as it is.
                     "BALLAD CHORUS PIANO PUSHES", "TALK-BOX HOOK", "REVERSE SWELLS INTO EVERY OTHER BAR", "ORGAN CHORD EIGHTHS",
                     "ORGAN SIXTEENTH STAB PULSE", "BALLAD PIANO 3+3+2 WITH TOP-LINE FILLS", "SYNTH BRASS STABS", "BALLAD PIANO ROCKING COMP",
                     "KEYS ON TWO, TWO-AND, THREE",
                     // (0.259.0) Round eleven (the owner's 61 more songs); some are this round's types' own, shared as they are.
                     "TWO-NOTE ROCK ON EACH BEAT AND ITS A", "ONE-NOTE STAB LINE, A NEW NOTE EACH BAR", "BROKEN CHORD FROM BEAT TWO",
                     "QUARTER OCTAVES AND FIFTHS, A MELODY FROM TWO-A", "POUNDED QUARTER CHORDS OVER A BROKEN LEFT HAND", "PIZZICATO OCTAVE BOUNCE",
                     "HIGH FLOURISH EVERY OTHER BAR", "LEADING-TONE GRACE ON TWO-A AND FOUR-A", "ONE HIGH SYNTH HIT ON EVERY FOUR", "BRASS LINE AROUND THE FIFTH",
                     "OCTAVE ARPEGGIO, EVERY NOTE STRUCK TWICE", "TWO SIXTEENTH STABS ON ONE", "PIANO OCTAVES BROKEN ON THE SIXTEENTHS" };
        return {};
    }

    // (0.173.0) Built once (the lists never change): every lane idea under a type asked for them, strings and all.
    juce::StringArray extraApproaches (ComposerLayer layer)
    {
        static const juce::StringArray drums = extraApproachesUncached (ComposerLayer::drums), bass = extraApproachesUncached (ComposerLayer::bass),
                                       guitar = extraApproachesUncached (ComposerLayer::guitar), chords = extraApproachesUncached (ComposerLayer::chords);
        switch (layer)
        {
            case ComposerLayer::drums: return drums;
            case ComposerLayer::bass: return bass;
            case ComposerLayer::guitar: return guitar;
            case ComposerLayer::chords: return chords;
            default: return {};
        }
    }

    // (0.263.0) Both lists depend only on the type, the layer, the riff flavour, written riffs and the keys' world:
    // kept per those (they were built again from about a hundred names on every call, many times per compose).
    using ApproachKey = std::tuple<juce::String, int, int, bool, bool, bool>;
    static ApproachKey approachKey (const ComposerState& s, ComposerLayer layer, bool full)
    {
        return { s.sectionType, (int) layer, s.riffFlavour, s.writtenRiffs,
                 layer == ComposerLayer::chords && s.lane (layer).world == (int) LaneWorld::cyberpunk, full };
    }

    static juce::StringArray keptApproaches (const ComposerState& s, ComposerLayer layer, bool full);

    juce::StringArray baseSectionApproaches (const ComposerState& s, ComposerLayer layer) { return keptApproaches (s, layer, false); }
    juce::StringArray sectionApproaches (const ComposerState& s, ComposerLayer layer) { return keptApproaches (s, layer, true); }

    static juce::StringArray sectionApproachesUncached (const ComposerState& s, ComposerLayer layer)
    {
        if (layer == ComposerLayer::chords && s.lane (layer).world == (int) LaneWorld::cyberpunk)   // (0.219.0)
            return cyberpunkKeysOptions();
        auto names = baseSectionApproachesUncached (s, layer);
        if (names.isEmpty() || ! sectionTypeCalled (s.sectionType).has_value())
            return names;
        // (0.160.0) The bass's go in front of LOCKED WITH THE KICK, which stays last (the composer finds it there);
        // a bass list that doesn't end with it takes none.
        if (layer == ComposerLayer::bass)
        {
            if (names[names.size() - 1] != "LOCKED WITH THE KICK")
                return names;
            names.remove (names.size() - 1);
            for (const auto& n : extraApproaches (layer))
                names.addIfNotAlreadyThere (n);
            names.add ("LOCKED WITH THE KICK");
            return names;
        }
        for (const auto& n : extraApproaches (layer))
            names.addIfNotAlreadyThere (n);
        return names;
    }

    static juce::StringArray keptApproaches (const ComposerState& s, ComposerLayer layer, bool full)
    {
        static std::recursive_mutex lock;
        static std::map<ApproachKey, juce::StringArray> kept;
        const auto key = approachKey (s, layer, full);
        const std::lock_guard<std::recursive_mutex> l (lock);
        auto found = kept.find (key);
        if (found == kept.end())
        {
            if (kept.size() > 4096)
                kept.clear();
            found = kept.emplace (key, full ? sectionApproachesUncached (s, layer) : baseSectionApproachesUncached (s, layer)).first;
        }
        return found->second;   // (a copy, under the lock)
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
        // (0.194.0) A type that writes the ACOUSTIC lane (round seven's acoustic-led verses) switches it on.
        if (! round3Approaches (s.sectionType, ComposerLayer::acoustic).isEmpty())
            s.lane (ComposerLayer::acoustic).on = true;
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
            // (0.144.0) From the owner's 61 song files (shapes only; our names).
            { "3+3+2 PICKUP, CUT ON THREE", "The band chugs two beats with the snare on 3+3+2, hits beat 3 together, then silence" },
            { "DOTTED-EIGHTH FILL", "The chords ring while the snare and toms hit every three sixteenths across the last bar" },
            { "RIFF ALONE BAR", "The last bar the guitar's riff alone, a kick and snare on its last two sixteenths" },
            { "BURST PICKUP", "Everyone on four sixteenths over the last beat, a snare fill under them" },
            { "SCALE RUN UP", "The bass and guitar run up the scale in sixteenths into the next chord, the snare rolling" },
            { "BAND EIGHTH RAMP", "The last two bars everyone on the next chord in eighths, the snare and toms on every eighth, sixteenths to end" },
            { "HALF-STEP PICKUP STABS", "Two power chords on the last beat, the root then a half step up, a crash on the last sixteenth" },
            { "HELD BAR", "The guitars ring one chord the whole last bar, the bass out, the drums keep time" },
            { "TREMOLO HOLD", "The root tremolo-picked over a held bass note for the last bar" },
            { "RIFF CUT SHORT", "Everyone stops after the last bar's twelfth sixteenth" },
            { "TWO SILENT BARS", "The last two bars silent" },
            { "DROP TO BASS AND KICK", "The last bar only the bass and the kick" },
            { "TOM CASCADE", "Four falling groups of tom hits across the last bar, high to floor" },
            { "STRINGS RUN, THEN HOLD", "The drums out, the keys running up in sixteenths then holding the chord, the band holding it" },
            { "HIT, TWO PUSHES, SILENCE", "A hit on the one, two stabs on the a of 2 and the e of 3, then silence" },
            { "SNARE BURST, HIT AND HOLD", "A burst of sixteenth snares on beat 2, then everyone hits beat 3 and holds" },
            { "LONG STOP, BELL COUNT-IN", "A chord held a bar, then a bar of silence with the ride's bell counting in" },
            { "UNISON EIGHTHS BAR", "The last bar the kick, snare and floor tom on every eighth with the band" },
            { "CYMBAL ROLL BAR", "The last bar the bass alone under hats, then crashes in sixteenths" },
            { "GROWING CHORD", "The last bar the guitar in sixteenths, its chord growing from two notes to five over toms" },
            { "KICK & SNARE ROLL", "The last half bar the kick and snare in turn on every sixteenth" },
            // (0.154.0) Round four, from the owner's 54 more song files (shapes only; our names).
            { "RISER INTO THE NEXT", "The last bar the guitar tremolos the next chord's root and the crashes swell, no kick or snare" },
            { "UNISON SYNCOPATED BAR", "The last bar everyone hits together on 1, the a of 1, the and of 2, the and of 3 and the a of 3" },
            { "CLAP BREAK", "The last bar the band out, claps on every beat" },
            { "DOTTED HITS, THEN A SILENT BAR", "Everyone on dotted eighths across the bar before last, then a silent bar" },
            { "THREE STABS AND A HOLD, TWICE", "The last two bars three eighth stabs, then the chord held to the bar line" },
            { "TIMPANI ROLL", "The band holds the chord while the floor tom rolls in sixteenths, getting louder" },
            { "HIT ON ONE, BASS FIGURE", "Everyone hits the last bar's one, then the bass alone walks up the scale into the next chord" },
            { "THREE BEATS OF SILENCE", "Everyone stops for the last three beats" },
            { "TRITONE HIT, RUNG BAR", "The last bar a power chord on the tritone struck once and rung, the bass under it" },
            { "CHORD ON ONE, SNARE PICKUP", "The chord struck on the one and held, a sixteenth snare pickup on beat 4" },
            { "GALLOP BAR, SNARE ON THE ANDS", "The last bar the guitar, bass and kick gallop, the snare on every and" },
            { "TWO HITS AND TOMS", "Hits on 1 and the and of 1, then the toms fall in sixteenths to the bar line" },
            { "PUSHED STOP", "Everyone pushes the next bar's hit onto the and of 4 and holds it, then silence" },
            { "HELD BAR, DRUMS PICK UP", "The band rings one chord the last bar, the drums out until a sixteenth pickup over beats 3 and 4" },
            { "STRINGS CLIMB, TOM ROLL", "The keys climb the chord in eighths over a floor tom roll, the band holding the chord" },
            { "HIT, THEN MUTED EIGHTHS IN", "A hit on the one rung two beats, then palm-muted eighths on the next chord's root" },
            { "DROP TO CLEAN, SNARE BUILD", "The last bar a clean picked chord, the bass out, the snare building eighths into sixteenths" },
            { "STOP, THREE-SNARE PICKUP", "A hit on the one, silence, then three snares on the last three sixteenths" },
            { "HITS ON TWO AND THREE, TOMS UNDER", "The band hits beats 2 and 3 while the toms roll sixteenths under them" },
            { "TOM RUMBLE BARS", "The last two bars the chord held each bar over toms on every sixteenth, ghosted" },
            { "HALF-STEP BURST BAR", "The last bar sixteenth bursts on the root, then a half step up, in turn each beat" },
            { "RIFF ALONE, TWO BARS", "The last two bars the guitar's riff alone" },
            { "UNISON SIXTEENTHS BAR", "The last bar everyone on every sixteenth, the kick with them, the snare on the beats" },
            { "SIXTEENTH SWELL, CUT SHORT", "The last bar everyone on sixteenths swelling, cut after the twelfth" },
            { "SIX-SIXTEENTH TURNAROUND", "The last bar a six-sixteenth cell twice, then a hit held to the bar line" },
            { "DROP TO KEYS AND LEAD", "The last bar only the keys and the lead" },
            { "CUT AT THE HALF BAR", "Everyone stops halfway through the last bar" },
            // (0.172.0) Round five (the owner's 12 songs; shapes only, our names).
            { "UNISON HIT BAR A HALF STEP UNDER", "The last bar everyone hitting together in threes and twos on the chord a half step under, the snare and toms on every hit" },
            { "A HIT, TWO SILENT BARS, KICK PICKUP", "One hit on the last two bars' first beat, then silence, then a lone kick on the last sixteenth" },
            { "TWO STABS AND A SILENT BAR", "The last bar two stabs on its first two sixteenths, then nothing" },
            { "HALF-BAR TOM FILL", "The band stops halfway through the last bar, the drums filling the first half on the snare and toms" },
            { "MUTED BREATH BAR", "The last bar only a palm-muted root in eighths, a snare on 1 and a hat on 3" },
            { "HIGH SOFT NOTES, BAND OUT", "The last bar the band out, four soft high notes held a beat each" },
            // (0.186.0) Round six (the owner's 25 songs; shapes only, our names).
            { "BAND HITS ON THE SNARE BUILD", "The last bar silent for half a bar, then everyone hits together on the snare's sixteenths" },
            { "MARCHING SNARE STOP, 2 BARS", "The band stops after one hit, the snare alone on a marching figure for the last two bars, claps on the beats" },
            { "STOP FOR ONE BEAT", "The last bar thinned to one hit rung three beats, a kick and an open hat, then a beat of silence" },
            { "ONE GUITAR'S SIXTEENTHS, TWO BARS", "The last two bars one guitar alone on the root in sixteenths" },
            { "STUTTERED STOP ON FOUR", "The last bar everyone hits 1, 2 and 3, a snare on the a before 2 and 3, beat 4 silent" },
            { "BAND IN ON BEAT THREE", "The last bar empty until beat 3, then the band and the kit in on eighths" },
            { "SNARE PICKUP ON THE AND OF THREE", "The band holds the last bar's one, the snare picks up from the and of 3 into the next" },
            { "SNARE EIGHTHS TO THE BAR'S END", "The last bar the band holds its chord, the snare on 1 then on every eighth from beat 3" },
            { "TOM SIXTEENTHS ON BEAT FOUR", "The kick, the toms and the snare together on the last beat's sixteenths" },
            { "ONE BAR OF THE RIFF ALONE, HATS HELD", "The last bar the guitar's riff alone with closed hats keeping the eighths" },
            { "KICK HELD A BAR, THEN THE RIFF ALONE", "The bar before last only the kick on the quarters, then the guitar's riff alone" },
            { "CHORD RUNG A BAR, LAST-16TH KICK PICKUP", "The last bar one chord rung, a lone kick on its last sixteenth" },
            { "UNISON HITS, THEN HELD SILENCE", "The bar before last everyone on the same hits (1, the and of 1, the and of 2, the and of 3 and 4), then a silent bar" },
            { "BAND HIT HELD TWO AND A HALF BEATS, RIDE PICKUP", "A hit on the last bar's one held two and a half beats, then the ride picks up on the eighths" },
            { "SNARE SIXTEENTHS ON THREE INTO THE CHANGE", "The last bar's second half a snare fill in sixteenths, the band holding the chord" },
            // (0.199.0) Round seven (the owner's 70 songs; shapes only, our names; an acoustic lane that's on strums with the band).
            { "CHORD TIED OVER THE BAR LINE", "The next section's first chord struck on the last eighth and held over the bar line, so the next section starts on a rest" },
            { "RING THE BAR, THEN SILENCE", "The bar before last one chord struck and rung the whole bar, then a silent bar" },
            { "BAND HIT, EMPTY BAR", "The bar before last a short hit on its one, then nothing until the next section" },
            { "SILENT BAR, HALF-BAR STRUM PICKUP", "A silent bar, then half a bar of silence and a two-beat strum in eighths into the next" },
            { "STOP AFTER TWO BEATS", "The last bar played to beat 2, a hit there, then two beats of silence" },
            { "HATS ALONE BREAK", "The last two bars only the closed hats on the eighths (a china on the first one, two splashes), a snare pickup on the last beat" },
            { "DRUMS ALONE, BLAST, THEN THREE CHORDS", "The bar before last the drums alone, kick and snare taking turns on the sixteenths; then three chords with the kit on 1, 3 and 4" },
            { "CHROMATIC POWER-CHORD FALL", "The last bar power chords falling a half step a sixteenth from an octave over the next chord's root down to it, toms falling under them" },
            { "PUSHED STRUM STUTTER", "The last bar a stuttered push (1, the a of 1, 2, the e of 2, the e and the and of 3), then sixteenths on 4 with the next chord" },
            { "OFF-EIGHTH KICKS INTO A STOP", "The last bar the band and the kick on the off eighths (1-and, 2-and, 3-and), one hit on 4, then silence" },
            // (0.216.0) Round eight (the owner's 67 songs; shapes only, our names).
            { "ROOT HELD, HALF-STEP NEIGHBOURS ON FOUR", "The last bar's chord held to 4, then a half step over, a half step under and back on the last sixteenths, toms under them" },
            { "SNARE AND TOMS TRADING SIXTEENTHS", "The last bar every sixteenth on the snare or a tom, no kick or cymbal, the band holding its chord" },
            { "BLUES WALK DOWN IN THREES", "The last bar the root held, then the guitar and the bass walking down the blues scale every three sixteenths, crashes and splashes with it" },
            { "TWO QUARTER HITS AND AIR", "The last bar two hits on 1 and 2, a whole step under the next chord and then on it, then two beats of air" },
            { "FOURTH DYADS CLIMBING ON THE DOTTED GRID", "The last bar the root, then four fourth dyads climbing a half step each on the dotted sixteenths up to just under the next chord" },
            { "TWO CHORDS IN THREE EIGHTHS, BASS ON THE ANDS", "The last bar two chords, three eighths each, this one and the next, the bass answering on the ands" },
            { "HELD CHORD, THREE STEPS UP ON TOP", "The last bar one chord rung, three high notes climbing on 2, 3 and 4 over it, the bass walking up and a small fill" },
            { "CHROMATIC BASS FALL INTO THE NEXT", "The last bar the bass falling a half step a sixteenth into the next chord's root, the snare and floor tom under it" },
            { "A BAR OF HITS, HALF A BAR OF AIR, EIGHTHS", "The bar before last five hits, then half a bar of air with the bass walking up, then eighths on the next chord" },
            { "OFF-BEAT STABS BAR, KICK AND CRASH WITH THEM", "The last bar every lane only on the off-beats (the and of each beat), the kick and a crash with them" },
            { "SIXTEENTH PAIRS ON EVERY BEAT", "The last bar the guitar and the bass on a pair of sixteenths each beat, the snare and floor tom on the eighths, no cymbal" },
            { "A BAR OF DYAD TREMOLO, THEN THE CHORD HELD", "The bar before last two dyads tremolo-picked in turn, then the chord struck and held, the snare and a crash on 1 and 2" },
            { "ALL-LANE SIXTEENTH CELL, KICK AND OPEN HAT QUARTERS", "The last bar the guitar, the bass and the keys on one sixteenth cell (root, root, the seventh and the octave high), the kick and an open hat on the quarters" },
            { "EIGHTHS TO BEAT THREE, THEN SILENCE", "The last bar the chord on the first five eighths, then silence until the next section, the kick and a crash only on 1" },
            { "THREE EVEN STRUMS FALLING", "The last bar three chords held five, six and five sixteenths, falling from the key's flat six to its five and four" },
            // (0.237.0) Round nine (the owner's 17 industrial songs; shapes only, our names).
            { "QUARTER-NOTE TRIPLET HITS INTO THE NEXT", "The last bar's second half three hits as quarter-note triplets, the kick and snare with them" },
            { "KICK AND OPEN HAT ON THE LAST EIGHTH", "Everything stops an eighth early; the next chord pushed onto the last eighth with a kick and an open hat" },
            { "KICK SIXTEENTHS FOR THE LAST HALF BAR", "The band plays on; the kick on every sixteenth for the last half bar, getting louder" },
            { "TWO BARS CLIMBING A HALF STEP", "The last two bars the band in eighths, the second bar a half step higher, the kick under every eighth" },
            { "FEEDBACK RING IN", "The last bar the band out but one high guitar note left to feed back, a soft crash and a held keys note" },
            { "KICK THREE, SNARE BURSTS", "The last bar the band and the kick on 1, 2 and 3, then a sixteenth snare burst on 4" },
            { "SNARE ON EVERY BEAT INTO THE STOP", "The last two bars the band, the kick and the snare on every beat, the last beat left silent" },
            // (0.244.0) Round ten (the owner's 56 songs; shapes only, our names).
            { "STOP BAR BEFORE THE NEXT", "The last bar every lane hits 1 and 2 together and stops, two keys stabs on 3 and 4 in the silence" },
            { "DRUM-DROP BAR", "The last bar the drums drop out while everyone else plays on" },
            { "FLAT-SIX LAUNCH, TWO BARS", "The last two bars on the key's flat six, the guitar and bass on a sixteenth figure, the kit coming in on a snare run" },
            { "THINNING ROLL BUILD", "The last two bars a snare roll that thins into groups of three over kick quarters, the keys out, the bass walking up to the five" },
            { "UNISON 3+3+3+3+4 BAR", "The last bar the band in unison on 3+3+3+3 sixteenths, then four sixteenths straight into the next" },
            { "TWO-BEAT PICKUP BAR", "The last bar's first two beats silent, then a pickup of eighths on the next chord with snare eighths" },
            { "TOM EIGHTHS UNDER A RINGING CHORD", "The last bar one chord struck and left ringing, toms on the eighths from beat 2, the bass walking into the next root" },
            { "KICK QUARTERS, THEN KICK AND SNARE QUARTERS", "The last two bars the kick on every quarter, the snare joining it on every quarter in the last bar" },
            { "PAIRED-SIXTEENTH BUILD BAR", "The last bar snare sixteenths in pairs, then four straight, over kick quarters" },
            { "BAND HITS FROM TWO-AND", "The last bar everyone hits together on every eighth from the and of 2" },
            { "KEYS GLISSANDO INTO THE DOWNBEAT", "Twelve short keys notes sweeping up two octaves over the last beat into the next downbeat" },
            { "EVERYTHING ON ONE, HELD", "The last bar every lane strikes its chord on 1 together and holds it the bar" },
            { "BALLAD TOM FILL BAR", "The band plays on; the last half bar a tom fill falling from the high tom to the floor" },
            { "ROOT SIXTEENTHS BUILD", "The last bar the guitar and bass on the root in sixteenths and the snare with them, all getting louder" },
            { "STOP, TWO HITS ON FOUR", "The last bar one hit on 1, silence, then two hits on 4 and its and on the next chord" },
            { "TWO BARS OF KIT ALONE", "The last two bars the drums alone, the kit's groove going on and a fill over the last two beats" },
            // (0.258.0) Round eleven (the owner's 61 more songs; shapes only, our names).
            { "BAND OUT FROM THREE-AND, THE BASS RINGS ACROSS", "The last bar the kick on 1 and the snare on 2 and 3, then everything stops from 3-and but the bass's root, left ringing over the bar line" },
            { "FLOOR TOMS, THEN A CRASH SIXTEENTH RUSH", "The last bar every lane out, floor toms on 2-a, 3-e and 3-and, then crash sixteenths rushing to the bar line" },
            { "KICK AND CRASH SIXTEENTHS, NO SNARE", "The last bar the kick and crash on every sixteenth, no snare, the guitar and bass on the root in sixteenths" },
            { "KICK AND CRASH EIGHTHS, NO SNARE", "The last bar the kick and crash on every eighth, no snare, the guitar and bass on root eighths" },
            { "SNARE AND CRASH ON ONE-AND AND TWO-AND", "The last bar the snare and crash on 1-and and 2-and, the kick on 1, 2, 3-and and 4-and, the bass on the next chord's root and fifth in eighths" },
            { "DOTTED KICKS INTO TWO SNARE SIXTEENTHS", "The last bar the kick in threes then eighths, the band on the kicks, the snare on 2 and 4 then two sixteenths into the next" },
            { "DRUMS OUT, THEN SNARE SIXTEENTHS FROM TWO-A", "The drums out a bar under a held root, then snare sixteenths from the a of 2 to the bar line, a crash on 4-and" },
            { "KICK AND TOM PAIRS, THEN THREE DOTTED HITS", "A bar of kick-and-tom pairs with the band on them, then a bar silent at first and three dotted hits on 3, 3-a and 4-and" },
            { "QUARTER HITS, THEN A TOM PICKUP", "A bar of band hits on every quarter with snare and crash, then one hit on 1 and a tom pickup on 3, 3-and and 4" },
            { "CHORD RUNG, THE CYMBALS KEEP TIME", "The chord struck once and rung two bars, no kick or snare, the open hat and ride keeping the eighths" },
            { "FLAT-TWO SIXTEENTHS IN UNISON", "The last two beats the guitar, bass and kick in unison sixteenths on the flat two of the next chord, the snare only on 2" },
            { "SNARE AND TOM EIGHTHS SWELLING, TWO BARS", "The last two bars the snare and floor tom on every eighth swelling from soft to full, the band re-striking the chord with them" },
            { "KICK AND SNARE IN UNISON SIXTEENTHS", "The last bar the kick and snare together on a broken sixteenth pickup, the guitar and bass on it too" },
            { "KICK QUARTERS ALONE, A BAND HIT ON FOUR", "A bar of kick quarters alone, then the kick on 1, 2 and 3 and the whole band hitting the flat six on 4" },
            { "TWO HELD CHORDS A HALF STEP APART, DRUMS OUT", "The drums and bass out, the five rung a bar and then the flat six a bar" },
            { "A SIXTEENTH ARPEGGIO BAR, THEN THE CHORD RUNG", "A bar of the guitars in sixteenths through the chord, then the next chord rung with a tom pickup on 4 and 4-a" },
            { "ONE HIT, THEN A LONE SNARE ON FOUR", "The last bar the whole band on 1, nothing on 2 and 3, the snare alone on 4" },
            { "SNARE AND TOM SIXTEENTHS, TWO KICKS UNDER", "The last bar the snare and toms in broken sixteenths, two kicks under them, the guitar's root muted on a pickup" },
            { "SNARE IN THREES, THEN A SIXTEENTH ROLL", "A bar of the snare in threes with a gap, then a bar of sixteenths, the guitar and bass chugging with it" },
            { "SNARE, THEN TOMS, THEN KICK SIXTEENTHS", "The last bar sixteenths passing from the snare to the toms to the kick under one held chord" },
            { "THREE BAND HITS ON TWO, THREE AND FOUR", "The last bar one chord held on 1, then the whole band with the snare and crash on 2, 3 and 4" },
            { "A BASS-ALONE BAR AFTER A TOM PICKUP", "A bar with a tom pickup, then the band out but the bass playing a riff alone, one crash on 1" },
            { "SNARE AND OPEN HAT QUARTERS, A HIT AND STOP", "A bar of the snare and open hat on every quarter with the kick between, then one hit on 1 and silence" },
            { "SNARE GALLOP ALONE, BAND OUT", "The last bar the band stops; the snare alone in a gallop with one kick on 1" },
            { "KIT IN EIGHTHS, A TOM ROLL TO THE CHANGE", "The kick, snare and floor tom together on every eighth over a bar and a half, then a tom roll to the change" },
            { "KICK AND TOMS IN THREES, A SNARE TAG", "The last bar the kick and toms together in threes, a snare tag at the end, the band holding one chord" },
            { "THE BASS SLIDES DOWN ALONE, A SNARE BREAK BAR", "One band hit, the bass alone sliding down by half steps and holding, then a bar of snare and kick breaks with guitar stabs on 2 and 4" },
            { "THREE DOTTED HITS, THEN SILENCE", "The last bar three band hits a dotted eighth apart from 1, then silence to the bar line" },
            { "KICK PAIRS UNDER A SIXTEENTH CHORD BURST", "The last bar kick pairs, the snare on 2 and 3, the guitar's chord in sixteenths from 3 to the bar line, a crash on the last sixteenth" },
            { "FOUR CRASH QUARTERS, THEN A HELD CHORD", "A bar of the band, kick and crash on every quarter, then one chord held a bar with the drums out" },
            { "SYNCOPATED HIT BAR, A LONE SNARE AND TOM ON FOUR", "A bar of syncopated band hits with the snare and crash on each, then silence but a snare and tom on 4" },
            { "BAND EIGHTHS FROM ONE-AND, TOMS WITH THEM", "The last bar every lane on the eighths from 1-and, the kick and floor tom with them, no snare" },
            { "TWO-BAR ROLL IN THREES", "Two bars of snare and toms in threes from a kick and crash on 1, the band holding one chord" },
            { "THREE DOTTED-QUARTER TOM HITS", "The last bar the snare and floor tom on three dotted quarters, the kick between, the guitar and bass with the toms" },
            { "TOM SIXTEENTHS, KICK PAIRS IN THE GAPS", "The last bar toms in sixteenths falling to the floor tom, the kick in pairs in their gaps" },
            { "CHORD PAIRS WITH THE SNARE, A SIXTEENTH RUN IN", "The last bar chord pairs with the kick, the snare on 1, 2-and and 4, then a sixteenth snare run into the next" },
            { "A BAR AND A HALF OF SNARE SIXTEENTHS", "Snare sixteenths for a bar and a half over the guitar's chugs, then the kick and crashes on 3, 4 and 4-and, the band on the next chord at 3" },
            { "ONE HELD HIT, THEN A HALF-BAR SNARE FILL", "One band hit with the kick and crash held two bars, hats counting the last bar, a snare fill in its second half" },
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
