#include "engine/SectionTypes.h"

#include "engine/SongSections.h"
#include <map>
#include <mutex>

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
                     { "SIXTEENTHS UP A HALF STEP AFTER SIX", "Sixteenths with the bass: six on the root, ten a half step up, every bar, over ride sixteenths and snare eighths", RiffFlavour::openChords, {}, pre } };
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
                { "HELD SECOND CLUSTER, SIXTEENTH PICKUP", "A ninth-and-third cluster held each bar, a sixteenth pickup into the next chord, a tom groove", r, {}, i } };
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
                         { "KICK TRIPLE-SIXTEENTHS, SNARE AND CHINA ON THE ANDS", "Power chord pairs every six sixteenths, the kick in threes around the ands, the snare and the china on every and", r, {}, c } };
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
        struct Round3Lists { const char* name; juce::StringArray guitar, bass, keys, drums, lead; };
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

    juce::StringArray baseSectionApproaches (const ComposerState& s, ComposerLayer layer)
    {
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
                     "BONGOS ON THE GUITAR'S RHYTHM", "TAMBOURINE EIGHTHS OVER THE BACKBEAT" };
        if (layer == ComposerLayer::bass)   // (0.160.0) round four's bass versions: put in front of LOCKED WITH THE KICK
            return { "DOUBLES THE GUITAR'S HIGH NOTES, OCTAVES DOWN", "PLAYS ONLY THE HEAD", "KEEPS THE CHORUS RHYTHM", "CARVED SIXTEENTH LINE",
                     "EIGHTHS, PICKUP INTO EACH BAR", "A NOTE A BAR, FALLING FROM THE KEY'S NOTE", "CHROMATIC EIGHTHS AROUND THE ROOT",
                     "DOTTED FIGURE WITH THE KICK",
                     // (0.186.0) Round six.
                     "DOTTED HALF AND TWO EIGHTHS", "STABS AT EACH GROUP'S START",
                     // (0.193.0) Round seven.
                     "ROOT ON ONE HELD, REPEAT ON FOUR", "ROOT-FIFTH TRESILLO WITH THE TOM", "LATE PUSHES UNDER HELD CHORDS",
                     "WALK-UPS EVERY SECOND BAR" };
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
                     "POWER-CHORD QUARTERS OVER THE STRUM", "THE VERSE'S STRUM, DISTORTED" };
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
                     "BELL DOUBLES THE GUITAR" };
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

    juce::StringArray sectionApproaches (const ComposerState& s, ComposerLayer layer)
    {
        auto names = baseSectionApproaches (s, layer);
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
