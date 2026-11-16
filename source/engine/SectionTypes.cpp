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
                     { "OCTAVES ROCKING TO THE TRITONE", "Low octaves rocking between the root and its tritone, the bass with them", RiffFlavour::openChords, {}, pre } };
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
                { "SNARE ONLY ON FOUR AND FOUR-AND", "Chords held two bars with high stabs answering, the snare only on beat 4 and its and", r, {}, i } };
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
                         { "FOUR EIGHTHS, THEN A PUSH", "Four eighth-note chords, a gap, then pushes into the next bar, a high piano pulse over it", r, {}, c } };
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
