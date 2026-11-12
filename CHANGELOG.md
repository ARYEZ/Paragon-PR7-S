# Changelog

Each version's notes appear on its GitHub Release page.

## 0.95.0

**STACCATO CHUG, from your three verses** (Sevendust's Black and Waffle, Disturbed's Stricken). I read the MIDI for its shapes only; nothing is copied, and your files aren't in the repo.

What all three verses share, now built into STACCATO CHUG:
- **The bass doubles the guitar's chugs**: same rhythm, same notes, an octave down (Stricken) or in unison (Waffle).
- **The kick plays every chug.** The snare stays on 2 and 4, the hats or ride play quarters, and every fourth bar turns.

The bass's and drums' approaches under STACCATO CHUG (and HEAVY CHUG): DOUBLES THE GUITAR / ROOTS ON THE ACCENTS, and KICK ON EVERY CHUG / KICK ON THE ACCENTS.

**Three new rhythm guitar riff kinds**, first in STACCATO CHUG's guitar approaches:
- **PEDAL & STABS** (Black's shape): the low string chugged in sixteenths with a power chord stabbed every third sixteenth, so the stabs run in threes across the bar. The stab climbs every bar or two (4th, 5th...), and the phrase's last bar ends on a rest and two muted chords.
- **CHUGS IN THREES** (Stricken's shape): a rest and two muted power chords over and over, with a chromatic walk up on the bar's last sixteenths. The second bar walks twice, and every fourth bar climbs on ringing power chords.
- **BURST & SILENCE** (Waffle's shape): a hit, a gap, a burst of four chugs (the last held), a pair, then silence. A dissonant cluster stab fills the silence every other bar, and the phrase's last bar picks up into the next.

TRITONE CHUG, CHUGS, GALLOP and STOP-START stay in the list after them. AUTOMATIC goes through all seven, one per take. The three are also in the guitar's Riff Flavour menu for any section.

Notes:
- The third file was **Waffle**, not My Last Serenade. I used Waffle, since it's what you sent.
- I can't hear it here. The tests check that the bass and the kick land on the guitar's chugs, not how it sounds.
- The keys, lead and melody keep their usual styles in a STACCATO CHUG verse; none of the examples had them.

## 0.94.0

**Section types** (you asked: a chorus is either DRIVING or DRONING, that decides what every instrument does, and each instrument has its own options within it).

- **Every block has a TYPE.** Right-click a block on the SECTIONS row: **Type** lists that section's types. A new block starts as **STANDARD**, which is the composer as before (every lane its own styles). The block shows its type ("CHORUS 1 - DRONING"), linked copies share it, and it's saved with the song.
- **The types:**
  - **Chorus:** DRIVING and DRONING (yours).
  - **Verse: STACCATO CHUG** (yours): one verse type whose rhythm guitar approaches are its chug kinds: TRITONE CHUG, CHUGS, GALLOP, STOP-START. Black, My Last Serenade and Stricken will flesh it out once I have their parts.
  - **The other sections:** my suggestions, taken from the guitar riffs we already have. Keep or remove any of them.
    - Intro: DRONING, OPEN CHORDS.
    - Verse (besides STACCATO CHUG): DRONING, SPARSE (stabs, octaves or ringing chords, as approaches).
    - Bridge: CREEP, OPEN CHORDS, DRONING.
    - Breakdown: HEAVY CHUG (chugs, tritone grind, stop-start, stabs), STACCATO CHUG.
    - Ending: DRONING, STABS.
    - Solo: DRIVING, DRONING.
  - A section plays as its kind: a Pre-Chorus has the verse's types, a Hook the chorus's, and so on.
- **The type decides what the instruments do.** It sets the rhythm guitar's riff, and under DRONING the whole band follows: the bass doubles the ringing chords and the keys, lead and drums carry the movement (Crawling, Clown, Circle With Me).
- **Each instrument's options within the type.** In the composer, a lane's style menu now starts with **Approach**. Under STACCATO CHUG, HEAVY CHUG and SPARSE the rhythm guitar picks its riff kind there. Under DRONING:
  - **Rhythm guitar:** rung whole, pushed a beat early, moving top note, struck twice, sliding down.
  - **Bass:** held with the guitar, or held and pulsed at the loop's end.
  - **Keys:** high pulse, climbing motif, held third.
  - **Lead:** octaves on the thirds, octaves on the roots, muted pedal in threes.
  - **Drums:** open hats, half time, locked to the pushes.

  AUTOMATIC lets the style and the take choose, as before. ROLL and GENERATE give a new take within the approach you chose.
- **The composer's TYPE key** (next to SECTION) shows the block's type and changes it.
- **Changing a built block's type builds it again at once.** The lanes you haven't touched are written again for the new type; your own edits stay. It's one undo step. **HUB SETTINGS** has "Ask before changing a built section's type", off by default.

Honest gaps:
- Only DRONING has a band part of its own. Under DRIVING, STACCATO CHUG and the suggested types, the guitar plays the type's riff and the bass, keys, lead and drums play their usual styles.
- To give DRIVING and STACCATO CHUG their own band, send full MIDI or every instrument's tab (for STACCATO CHUG: Black, My Last Serenade, Stricken's verses), as you did for the droning choruses.

## 0.93.0

**The composer works block by block** (your song engine overhaul, part four, the last). With a SECTIONS row in the song, the composer follows it:

- **The SONG strip shows your blocks.** Each block of the SECTIONS row is a chip in the composer's SONG strip, with its bar, its length and BUILT once it has music from the composer. The block you're composing is lit; click any chip to compose that block.
- **The composer opens on the first block not yet built**, with its section, bars and chords.
- **BUILD moves on by itself.** After BUILD INTO a block, the composer goes to the next block not yet built, with its section set, ready for the next BUILD. The song is built front to back, part by part.
- **NEXT became + ADD.** The suggested next parts (a chorus after a verse, a bridge after a second chorus, an ending...) add a block of that kind at the end of the row and aim the composer at it. A name already in the song arrives as a linked copy, with its music.
- **Songs without a SECTIONS row** keep the SONG strip as it was: sections built one after another at the playhead.

That finishes the four parts we agreed:
1. **Blocks on the SECTIONS row** that carry their music when moved.
2. **Linked copies.**
3. **Chords rewriting the music.**
4. **The composer working block by block.**

## 0.92.0

**Change a section's chords and its music follows** (your song engine overhaul, part three). When the chords inside a block on the SECTIONS row change, the music in that block is rewritten to fit. That covers a chord swapped by hand on the chord track, a new progression, or the same chords in a different order.

- **What the composer built is built again.** When you BUILD into a block, the block remembers how: the composer's settings and which lane went onto which track.
  - Change the block's chords and each of those lanes is built again over the new chords from the same settings: the same rhythm, feel, riff shape and take, with the notes on the new chords.
  - This only happens for a lane you haven't changed since, so your own work is never thrown away.
- **Everything else follows the chords.** Any track with pitched notes in the block moves its notes onto the new chords, keeping your rhythm. That covers parts you played or edited, and built lanes you've changed by hand. It uses Follow Chord Track's "Chords & Scales" rule: a chord's third goes to the new chord's third, its fifth to its fifth, and other notes move along the scale.
- **Drums are left alone.**
- **Linked copies follow too.** Change the chords in one chorus and every linked chorus is rewritten the same way.
- **One undo step** takes back the chord change and the rewrite together.
- **Moving a block, or changing its length, doesn't count as a chord change**, so nothing is rewritten then. Undo never rewrites anything either.
- The recipe is saved with the song, so a block you built last week still rebuilds when you change its chords.

**Limits, honestly:**
- A lane rebuilt from the recipe is built as the lane's own style. An idea you took from the IDEAS tab, or your pasted riff, comes back as the lane's own style, not the taken idea.
- Notes that follow the chords keep their octave and rhythm. A followed line can land a little higher or lower than you'd have written it by hand.
- I can't hear the results here, so please try it on a real section.

## 0.91.0

**Linked copies on the SECTIONS row** (your song engine overhaul, part two: "repeated parts should be a linked copy").

- **Repeats are linked.** On the SECTIONS row:
  - **Duplicate** now makes a linked copy.
  - **Adding a section whose name is already in the song** gives a linked copy of the last one, with its length and its music. Add a second Chorus and you get your chorus again.
  - Linked blocks show a small chain at their top right.
- **Edit one, they all change.** Whatever you change inside a linked block is copied into its linked copies:
  - notes in the MIDI editor;
  - a part added, moved or deleted, on any track;
  - quantize or humanize;
  - the composer building into it;
  - its chords on the chord track.

  It happens as part of the same edit, so one Undo takes back the edit and its copies together.
- **Linked blocks keep one length and one name.** Changing the length or the section of one changes them all.
- **Making a block different:**
  - **Unlink** in a block's menu makes it its own from then on. Its music stays as it is, ready to change into a variation.
  - **Duplicate as Its Own Copy** makes an unlinked copy.
- Links are saved with the song.

**Not linked:** tempo and time-signature changes, markers, automation, and frozen tracks. A block's tempo, markers and automation are its own.

**One limit:** if a single edit changes two linked blocks in different ways, the first in the song wins and is copied into the other. Dragging a part from Chorus 1 into Chorus 2 is an example. Undo puts it back.

## 0.90.0

**The SECTIONS row: the song as blocks** (your song engine overhaul, part one). A row at the top of the arrangement, above the chord track, where the song is laid out as its parts in order: intro, verse, pre-chorus, chorus, bridge, breakdown, solo, outro and the rest (21 names).

- **Adding the row:** + ADD TRACK > Sections row. Or right-click it later for "Add a Section at the End".
- **Adding blocks:** + SECTION on the row (or a double-click on the empty row after the last block) adds one after the last, 8 bars long.
  - It names the bars that follow, so music already there isn't moved.
  - The blocks always sit end to end from bar 1: no gaps, no overlaps.
- **Each block's menu (right-click):**
  - **Section:** change what it is.
  - **Length:** 1 to 32 bars, or any number up to 64 with "Other...".
  - **Insert a Section Before / After.**
  - **Duplicate.**
  - **Set the Locators to It** (to loop it).
  - **Delete.**
- **Repeats are numbered:** a name that comes back reads Chorus 1, Chorus 2...
- **Drag a block to rearrange the song, and its music goes with it.** Everything inside the block moves to its new place:
  - the parts on every track (a part crossing a block's edge is cut there);
  - its chords and scales on the chord track;
  - its markers and automation;
  - its tempo and time-signature changes. A block in 3/4 stays in 3/4 wherever it lands.

  The other blocks slide to make room, and the whole move is one undo step.
- **What follows a block moves with the block's length:**
  - **Inserting** a block before others moves them later.
  - **Lengthening** a block moves what follows later; **shortening** it moves what follows sooner and drops what was past its new end.
  - **Deleting** a block takes its music and closes the gap.
- **Double-click a block to open the composer on it.**
  - **SECTION** is set from the block's kind: Pre-Chorus plays as a verse, Post-Chorus as a chorus, Outro as an ending.
  - **LENGTH** is the block's bars, and the chords are cut at its end.
  - **HEAR** plays the block's place in the song, and the key reads BUILD INTO CHORUS 2 (or whichever block).
  - **BUILD** writes into the block.
  - Double-click another block to work on that one.

**Not yet (the next releases, as we agreed):**
- **Linked copies:** a repeated chorus that changes when any one of them changes. A block copied with Duplicate gets its own copy of the music for now.
- **Changed chords rewriting the music** in their blocks.
- **The composer working block by block**, with the SECTIONS row taking over from its SONG strip.

**Two notes:**
- **Frozen tracks don't move with their blocks** (their parts are locked). Unfreeze a track before rearranging.
- **Duplicate makes a plain copy, not a linked one.** If you change one chorus, the other doesn't follow yet.

## 0.89.1

**Circle With Me's chorus in the droning band** (every instrument's tab you sent, read for how each part supports the ringing guitar; nothing of the song is copied). Its chorus confirmed what 0.89.0 does: the bass doubles the ringing guitar's roots, held, pushes and all, and the drums lock to the pushes. It also showed four more ways to carry the movement, so each is now one of the band's choices:

- **Lead, third style: a muted pedal in threes.** Palm-muted eighths on the key's root, octave and fifth, in a cell of three notes against a bar of eight, so it runs across the bar lines. It stays on the key's note under every chord (Circle With Me's second guitar, the main motion in its chorus). Every fourth bar slips a whole step down and back at its end.
- **Keys, third style: a held note a chord.** Each chord's third, held over it and moving by the nearest step, struck with the guitar, pushes included. In the second half of every eight bars, each new chord comes in from two scale steps above, a quarter at a time, down onto its third.
- **Drums, a third take (ROLL): locked to the pushes.** Half time for a fast song:
  - Cymbals on the half notes.
  - The snare once every two bars.
  - The kick on the downbeat and on every chord the guitar pushes early, with a crash.
  - The fourth bar's kicks on beats 2, 3 and 4, the eighth bar's in eighths.
- **Bass, every other take (ROLL):** the last bar of every eight is pulsed in eighths, to drive into the loop's start. The rest stays held.

In Circle With Me's chorus the second keys and both clean guitars stay silent. That's a good sign the droning chorus wants fewer layers: you may want to switch the melody lane off for it.

## 0.89.0

**The droning chorus's band** (your next step after 0.88.0: with the rhythm guitar ringing its chords, "most of the rhythmic or interesting stuff comes from the lead guitar the bass guitar the keyboards in the drums"). From the full MIDI you sent for Crawling and Clown, read for how each instrument supports the ringing guitar; nothing of either song is copied.

- **The band follows on its own.** When the composer's guitar lane is set to **Riff Flavour > DRONING**, the other lanes play their droning-chorus parts. Set the guitar to any other flavour (or WRITTEN RIFFS, or your own riff) and they go back to their usual styles.
- **Bass:** doubles the guitar's low strikes and holds each as long as the guitar rings it. It follows the slides down and the chords pushed early, like the bass in both songs. The guitar's top notes, ringing over a held chord, are left to the guitar.
- **Keys (the chords lane):** up high on the key's fifth, over every chord.
  - Its first style is a steady pulse (Crawling's), in sixteenths or eighths: BUSY decides, and the beats lean in a little louder.
  - Its second style is a short two-bar figure round the fifth (Clown's clean part), climbing a step at a time through the last bar of every four.
  - Each ROLL draws another figure.
- **Lead:** a shimmer of octaves in sixteenths on each chord's third. Its second style uses the root instead. The key's own note sits on top in the first half of every four bars, then drops out. BUSY under the low end plays eighths.
- **Drums:** each ROLL draws one of two beats.
  - **Open hats** (Crawling's): open hat on the eighths, kick on 1 and 3, snare on 2 and the and of 3. Every fourth bar pushes with extra kicks and a crash on its last beat.
  - **Half time** (Clown's): closed hats on the beats, the snare on 3, the kick moving between bars, and a short fill closing every four bars.
  - Both crash on the first beat of every four bars.
- **The melody lane** plays as before (it's the singer's).
- **SYNC still wins:** with SYNC on, the band follows the sync lane as in 0.86.0.
- **A fix to 0.86.0's SYNC:** the chord stabs, the doubled bass and the guitar's chugs looked up the chord as if the section started at the song's beginning. For a section built later in the song (a second chorus after a verse), they could play the wrong chord's notes. They read the right chord now.

## 0.88.0

**DRONING: the ringing chorus** (your second chorus kind, from your examples: Without Me, Capricorn, Clown and Circle With Me, the rhythm guitar part you sent after). Read for their shapes only; nothing copied.

- **A new riff flavour for the composer's guitar:** in the guitar lane's style menu, pick **Riff Flavour > DRONING**. The guitar strikes a chord and lets it ring across the bar, leaving the space to the lead, bass, keys and drums.
- **Every GENERATE or ROLL draws one of these shapes:**
  - **A chord a bar, rung whole** (Without Me's chord-a-bar shape). Big full chords with the third, or power chords.
  - **Struck, then the next chord pushed a beat early** (Circle With Me). The chord rings through two bars, then the next chord is struck on beat 4 of the bar before (sometimes on the and of 4) and tied across the bar line.
  - **The power chord held, a top note moving over it** (Capricorn). The low notes ring the whole bar while a single high note steps between the octave, the third and the fifth.
  - **Struck again halfway.**
  - **Rung, then the chord sliding down a half step at a time into the next bar** (Clown).
  - **The last bar stepping into the next chord** on its last beat.
- **The rings last.** A chord rings until the next thing the guitar strikes, never past its own chord. If the chord changes on a bar line where a push was meant to ring, the guitar strikes the new chord there instead of leaving a silent gap.
- **BUSY** changes how often it strikes, not the riff. Over the middle, some whole-bar chords are struck again halfway; under it, struck-twice and moving bars ring whole instead.
- **In a verse** it rings power chords only and keeps the moving top note for the chorus.
- DRONING is never picked by ANY, like DRIVING EIGHTHS. ANY's riffs are the same as before.
- **A fix to 0.87.0:** in DRIVING EIGHTHS' picked bars, the high notes (the octave and the fifth above it) were played at the palm-mute velocity. They ring open now.

For now this is the guitar's part only. The rest of the droning chorus (the lead, bass, keys and drums carrying the movement) is the next step. See the note in my reply.

## 0.87.0

**DRIVING EIGHTHS: the hard driving chorus** (your idea, from your nine examples: This Fire Burns, The End of Heartache, Faint, Faster, Tears Don't Fall, This Calling, Left Behind, Come Clarity and Parallels). I read each tab for its shape only; nothing of any of them is copied.

- **A new riff flavour for the composer's guitar.** In the guitar lane's style menu, pick **Riff Flavour > DRIVING EIGHTHS**. It's built for the chorus: power chords on every eighth note, left to ring, changing with the chords.
- **Every GENERATE or ROLL draws one of these chorus shapes** (each from your examples):
  - **The wall, then a muted stretch and a walk** (This Fire Burns). Open chords, then palm-muted ones, then single notes stepping down into the next chord.
  - **All open, with the chord itself walking** (The End of Heartache). The whole power chord steps a whole step and a half step into the next chord.
  - **Power chords moving inside the bar** (Faint). The eighths in groups of 3+3+2 (or 3+2+3), each group a power chord on another note over the root.
  - **A punch bar, then a bar split in half** (Faster). Ringing hits with space between them, then the root for half the bar and another chord for the other half.
  - **A chord a bar, the last bar split in half** (Left Behind).
  - **Full chords strummed down and up** (Tears Don't Fall). Bigger chords with the chord's third, the top note moving halfway through the bar, and sometimes a dead-string scratch on the beat.
  - **Big chords rung for whole bars** (This Calling: at a fast tempo the drums do the driving).
  - **The chord picked one note an eighth** (Come Clarity's pre-chorus; its chorus has no guitar in that file), or **the muted low string as a pedal under high notes** (Parallels' double-time chorus).
- **The walk only happens before a chord change.** If the next bar is the same chord, it plays muted chords there instead of walking to the chord it's already on.
- **In other sections:**
  - **Verse:** the muted stretch comes first, and the strummed and ringing bars are played muted.
  - **Breakdown:** the same shapes in quarter notes.
- **The whole band on it:** turn **SYNC** on with the guitar leading, and DOUBLE THE SYNC TRACK on the bass and drums. The kick lands on every eighth and the bass doubles the riff, walk included. Without DOUBLE, the kick and bass take the accents.
- DRIVING EIGHTHS is never picked by ANY: it's a whole chorus shape you choose on purpose. ANY's riffs are the same as before.

These are our own rules; tell me which shapes sound right and which don't, and I'll weight them.

## 0.86.0

**SYNC in the composer** (your idea: find a riff you love, then write the whole band to it).

- **SYNC** in the composer's header: pick the lane that leads (any lane: guitar, drums, bass, chords, melody or lead). Its lane is locked so the riff you found stays, and it's marked SYNC LEADER. Every other lane is then written to its rhythm, and each lane says what it's doing (FOLLOWS THE GUITAR, DOUBLES THE GUITAR).
- **It stays on.** Unlock the leader and roll it, and everything follows its new rhythm. GENERATE rolls the others, still following it. SYNC: OFF puts every lane back on its own.
- **How each lane follows.** The leader's rhythm is every hit it plays; its accents are the ones that stand out (open notes, held notes and the first hit of each bar; with the drums leading, the kicks on the beat).
  - **Drums:** the kick on the leader's accents, with the snare, cymbals and fills still the section's own (verse, chorus, breakdown...).
  - **Bass:** the chords' roots on the accents.
  - **Chords (piano):** a stab of the chord on each accent, held to the next.
  - **Guitar** (when another lane leads): palm-muted chugs on every hit, power chords let ring on the accents.
  - **Melody and lead:** looser, as you suggested. They keep their own line, and any note within a sixteenth of an accent moves onto it, so they land with the band without copying every chug.
- **DOUBLE THE SYNC TRACK** (in the bass's and drums' style menus): every hit, metal style.
  - The drums put a kick on every chug.
  - The bass doubles the riff's notes, playing the lowest note of each hit in its own low octave and staying near the note before (no octave jumps between power chords). Under a high stab (like the tritone grind over the chugs) it holds the chord's root instead of doubling the stab.
  - With the drums leading, the bass plays on every kick.

What you hear is our own rule set; tell me where it sounds wrong and I'll change it.

## 0.85.3

**Your three tweaks.**

- **One take by default.** Recording again over a part now replaces it: one take, recorded over again, no lanes. That goes for audio, MIDI and cycle recording; in a cycle the last whole lap is kept. Takes on lanes are still there when you want them: right-click the record key and tick **Keep Every Take on a Lane of Its Own** (or pick each mode below it, as before). The choice is saved with the song. Songs that were left on the old default now open on the new one.
- **The studio's X goes back to the Hub.** Closing the studio window asks to save (if anything's unsaved), closes the song and shows the Hub on its own. The Hub's X then quits the program. Opening or creating a song from the Hub brings the studio back with it. The studio now starts that way too: just the Hub, so its X quits straight away. A HUB opened over a song still only closes when you click its X. Quitting from Windows itself (the taskbar's Close window, logging off) still quits.
- **Chords on the chord track work like clips.**
  - **Move:** drag a chord or a selection, snapped; Alt-drag copies. Where a chord lands, it cuts out the chords underneath, and where it came from is left empty (a gap, no chord) instead of the chord before running on.
  - **Lengthen or shorten:** drag either edge (the pointer turns into the left-right arrow). Grown over the next chord, that one gives way; shortened, a gap is left. The last chord can be any length now, not just a bar.
  - **Cut:** the SPLIT tool cuts a chord in two where you click, as does Split Here in its menu, or Alt+X at the playhead. GLUE joins a chord to the next one.
  - **Copy, cut, paste and duplicate:** Ctrl+C, Ctrl+X, Ctrl+V (at the playhead) and Ctrl+D, and the same in the chord's right-click menu. Delete leaves a gap, as with clips.
  - Each edit is one undo step. Chords from the progression library or the composer now each get their own length, so a two-bar last chord lasts two bars.
- **It tells you when you go out of key.**
  - The key you pick in the composer now goes onto the chord track's key strip with its chords. If you pick E minor and write the chords, the chord track knows it's E minor.
  - A chord outside the key in force now has a pink edge (and OUT OF KEY where there's room), as well as the pink numeral it had before.
  - After a move, a stretch, a paste, a split or a chord change that leaves the key, a note over the chord track says so for a few seconds, for example "OUT OF KEY: C# (BAR 5) ISN'T IN E MINOR".
  - With the key strip following the chords (AUTO SCALES), it says when an edit moves the key instead.

## 0.85.2

**Speed, round four** (you asked for another round). Three reviewers read the code, one each for the sound, editing and the screen. I checked each finding before changing anything. Nothing sounds or looks different; things just take less time.

- **Auto-save no longer holds up the screen.** Every few minutes the backup copy wrote the whole song out as text on the screen's own thread, the plug-ins' saved settings included. It now only checks whether anything changed (quickly, without writing the song out), and the writing happens in the background, as SAVE already does. On a test song with 100 MB of plug-in settings, the screen used to wait well over a second; now it waits 70 to 80 ms. A fader you dragged still counts as a change.
- **The composer works again only when it has to.** Any edit to the song (a note moved anywhere, a part renamed) made the composer rewrite all six lanes while its window was open, and read the DRUMMER profile from disk each time. Now it keeps the lanes unless something they're made from changed (the key, chords, styles, settings, tempo, profiles, your riff), and keeps the drummer profile until its file changes.
- **Play from a new place, jumps and the cycle coming round:** the instruments are brought up to date with the song's controllers (sustain, mod wheel, bends) from those alone, not by walking every note of the track. This happens as the sound starts, so a long, busy track starts sooner.
- **Sends:** each block, the engine finds a track's send lanes in one pass, not one pass for each of the eight slots, twice.
- **PERFORM on the pads:** the pattern player no longer works out every step's notes every 2 ms; only the steps due now.
- **KEY CHECK:** each note is checked against the key without building the key's scale each time.
- **Screen:**
  - With Stationary Cursor on, the whole arrangement moves under the play bar every frame. While playing it now redraws 30 times a second, not 60: the bar stands still there, so smoothness isn't lost.
  - The MIDI editor's controller lanes draw only the part that's on screen. A long recorded mod-wheel or pitch-bend lane was built and drawn end to end for every move of the play bar.
  - Mixer strips repaint only when something they show changes (name, colour, selection, link group, pan, slots). Before, any edit repainted every strip, and docked in the wide layout they span the whole window.

**Not done yet** (found, but bigger or riskier):

- While recording, the whole arrangement is still redrawn with each new note.
- The arrangement isn't drawn once and moved under a Stationary Cursor.
- Waveforms are still drawn a pixel at a time.
- Folder parts are worked out on every frame.
- The EQ window's curve is redrawn even when nothing changed.
- Saving still copies the song (the plug-ins' settings with it) before writing it in the background.
- A chord edit with Follow Chord Track on is still about 5 ms.

## 0.85.1

**TRITONE CHUG learns your third example** (Flaw's Whole, the breakdown at 2:20; its shape only, nothing copied).

- **The grind last.** The hammer kind can now put the grind on the bar's last eighth, ringing into the next bar, after the chugs instead of before them.
- **The grind over a power chord.** The chugs can be a muted power chord in pairs of eighths, as in that breakdown, not only the open low string. Over the chugged chord the grind is its flat ninth and ninth, half a step apart: the same rub as Ten Ton Hammer's flat fifth and fifth.
- **A new answer bar:** every fourth bar can strike the grind in straight eighths, where the others walk power chords.
- Breakdowns lean to this shape. In a test run of 300 groove riffs, about 1 in 10 came out this way, for example three bars of muted power-chord chugs with the grind on the last eighth, then a bar of the grind in eighths.

Everything else is as in 0.85.0: pick **Riff Flavour > TRITONE CHUG** in the guitar lane's style menu, then GENERATE.

## 0.85.0

**The rhythm guitar generator, first round: TRITONE CHUG** (you: GENERATE gave the same riff over and over; you're redesigning the song engine around the rhythm guitar, with the tritone chug riff first).

- **A new riff every time.** The composer's guitar no longer picks from six written riffs a style. Every riff is built from scratch, so GENERATE or ROLL gives a brand-new one. A thousand GENERATEs give at least 950 different riffs (tested), and it never hands back any of the last 200 it gave you.
- **The riff's type ("flavour"):** in the guitar lane's style menu, **Riff Flavour**. **TRITONE CHUG** is the one to try. The others (CHUGS, GALLOP, CREEP, STABS, OCTAVES, STOP-START, OPEN CHORDS) still use the first, rougher rules; ANY picks a different type each time. A chosen type stays until you change it.
- **TRITONE CHUG is built from your two examples**, their shapes only (nothing of either song copied). It writes one of two kinds:
  - **The hammer kind (Ten Ton Hammer's intro):** a high chord of the flat fifth and the fifth together, two octaves up (the half-step grind you love), struck and left to ring over the open low string, answered by chugs. The bar plays three times, then an answer bar: power chords walking dark notes (the flat second, flat third, fourth, flat fifth, fifth, flat sixth), a stop, a half-step creep, or a run of chugs. Each GENERATE draws all of it again: the grind's voicing (mostly yours; sometimes the flat fifth doubled, or the root and flat fifth up high, or the grind an octave lower), how long it rings, whether it lands on the downbeat, the "and" or after the chugs, the chug rhythm (kept the same bar to bar, as a riff repeats), a half-step chug or not, and the answer.
  - **The djent kind (Wildfire's riffs):** the chug is the tritone itself (the low string a half step up under the open fifth and octave), palm-muted in groups of two, three or four sixteenths that run on across the bar line, pinch harmonics on the same shape, a bar of open chugs with low notes bent a half step, or the chord let ring at the end.
  - Metalcore and nu-metal lean to the djent kind, groove and thrash to the hammer kind.
- **The section shapes it:** a breakdown rings the grind longer with sparser chugs; a chorus leans to the bigger voicings.
- **BUSY now thickens or thins the same riff.** Before this round it could swap the riff for a different one.
- **Written Riffs** in the same menu brings back the old six of each style.

This is a first round to judge by ear. Tell me what's off, from "the grind should ring longer" to "never use that answer bar", and send more riffs you love; I'll tune the rules. Then your questions about the other kinds of rhythm.

## 0.84.1

**Your three reports about selecting and dragging.**

- **A big select box lagged.** Every move of the mouse redrew the whole arrangement, and your window spans three screens. Now only what changed is redrawn: the box's old and new edges and the strip it newly covers or leaves. Tested: a box over most of the view, grown by a few pixels, redraws under a tenth of the view (it was all of it).
- **Selected clips on different tracks didn't all move.** Most likely you picked them with Ctrl-click, the other DAW's way. Ctrl-click only selected the one clicked, so the earlier clips dropped out of the selection (the track rows stayed lit, so it looked as if all three were selected), and only one moved. Now **Ctrl-click adds a clip or part to the selection**, and Ctrl-click on a selected one takes it out again. Shift-click and a box already worked and still do. Tested with three audio clips on three tracks, chosen each of the three ways: dragging one moves all three.
- **Takes on lanes:** dragging several selected takes drew them all on the track's own row, stacked on top of each other, so it looked as if only one was moving. They're now drawn on their own lanes while you drag. (They did all move when you let go.)
- **The select box scrolls at the edges of the view.** Before, the view only scrolled once the mouse went *past* its edge, and on the right-hand screen's edge the mouse can't go any further. Now holding the box within about 24 pixels of any edge scrolls the view that way, and the further in you go, the faster it scrolls. It works across the song and down the tracks.

If the clips still don't all move together, tell me how you selected them (Ctrl-click, Shift-click, a box, or clicking the tracks in the track list) and I'll look again.

## 0.84.0

**KEY CHECK: every note and chord outside the key, in one list** (you asked how to make sure everything in a song is in the right key; your rules: the same rule the chord track uses, and skip the drums).

- **Open it** from a part's right-click menu or the chord track's menu (Key Check: Notes Outside the Key...), or the Window > Key Check command (no key given; set one in KEYS if you want).
- **What counts as in the key** is exactly the chord track's rule: the key's seven notes, and in a minor key also the raised seventh (G# in A minor, as the harmonic minor's V and V7 use). It's tested against the chord track's own check for every chord in all 30 keys.
- **Which key:** AUTOMATIC uses the chord track's keys where the chord track has them (a key change part way through is followed), else the key guessed from the song's notes (the same guess as + MY RIFF). Or pick any of the 30 keys in the KEY box.
- **What's checked:** every note on instrument tracks, as you hear it (a part's TRANSPOSE counted), and every chord on the chord track. Drum tracks are skipped (a drum map, the drum editor, MIDI channel 10, or "drum"/"perc" in the name). Muted parts and muted notes are skipped too.
- **The list:** each note's bar, its track, the note, and the nearest note of the key (a half step away; a tie goes down). Click a row and the MIDI editor opens on that part with the note selected and the playhead on it. FIX moves that one note; FIX ALL NOTES moves every listed note. Each is one Undo. Chords outside the key are listed but not changed for you: swap them on the chord track. The list checks again whenever the song changes.
- **In the MIDI editor:** while KEY CHECK is open, every note outside the key has a pearl-white ring. (A ring, not a colour: the notes' colours already show their velocities.)
- A note outside the key isn't always a mistake (a passing note, a blues third, a borrowed chord): FIX only the ones you didn't mean.

## 0.83.4

**Drag a riff into the composer and it builds the section** (you asked: drag a MIDI part from the main window into MY RIFF and have it build that part).

- **From the arrangement:** drag a MIDI part out of the main window and over the composer's window. The composer lights up in the family's gradient and says LET GO TO BUILD A SECTION AROUND THIS RIFF. Let go and it asks THIS RIFF IS MY... (intro, verse, chorus, bridge, breakdown, ending, solo). Pick one and that whole section is built: your riff becomes MY RIFF, its key and chords are read from it, the guitar plays it, and the drums, bass and everything else that's switched on are built around it, after the last section you built. The part you dragged stays where it was in the arrangement.
- **MIDI files** dropped on the composer from the desktop or a folder do the same: the section question, then the build. (Before, a drop only set the riff and you had to click BUILD yourself.)
- **+ MY RIFF's menu** still only takes the riff (From the Selected Part / From a MIDI File), so you can change the lanes before building.
- The drag stays inside the studio (the part is handed to the composer directly), so it doesn't depend on Windows' file dragging. Dropped anywhere else, a part moves (or drags out as a file) as before.
- One Undo takes the built section back.

What can't be checked here: the drag on your Windows screens (tested with the mouse moves simulated, not a real hand on a real mouse).

## 0.83.3

**LEARN MY BAND: fast picking on the rhythm guitar** (you said the record's fast picking lost notes). The reader found each pick by a jump in loudness, smoothed over 40 ms so a low string's own waves wouldn't count. On a ringing string, picked fast, the loudness hardly moves between picks, so most picks were never seen. Measured on test DIs made the way a picked string sounds (each pick starts the string again, a short bright pick noise, the string ringing on):

| Playing | Before | Now |
|---|---|---|
| Open 16ths at 180 BPM | 5 of 24 picks | 24 of 24 |
| Tremolo 32nds at 140 BPM (power chord) | 1 of 36 | 36 of 36 |
| Power-chord 16ths at 160 BPM | 1 of 20 | 20 of 20 |
| Palm-muted 16th chugs at 200 BPM | 32 of 32 | 32 of 32 |

- **The pick's click:** every pick starts with a short bright noise (the plectrum on the string), heard even when the loudness doesn't change. The reader now listens for it above 1.5 kHz as well as for the loudness jump. Only clicks about as loud as your own picks count, within 40 dB of the guitar's sound at that moment: a held chord's dying high end wobbles too, and that mustn't become extra notes (tested: a chord held 1.5 s stays one note, slow riffs keep their hits).
- **Muted or open on fast picks:** "how fast does it die" was measured with a 25 ms window that reached into the next pick, so fast palm mutes read as open and some fast open picks as muted. It's now measured on each note's own cycle, just after the pick to just before the next one. On the tests: none of 112 fast notes read the wrong way (7 of 32 mutes were wrong before).
- **Pitch on very short notes:** a pick 50 ms long is too short to tell a low A's harmonics apart, so tremolo notes were found and then thrown away. The pitch is now read over at least 80 ms, and a fast pick that still can't be read takes the note of the pick before.

**What I can't check here:** your real DIs. These are made-up test guitars built to behave like picked strings. Read your songs again with READ THE SONGS, compare with the DI, and tell me what's still off.

- **Reading a song again keeps the review song before it:** it was written over, so corrections you'd made in it were lost. Now the old one is kept beside the new one as "<song> before <date and time>.pr7s". If you can share one DI with your corrected MIDI, I can measure the reader on your own playing.

## 0.83.2

**Speed, round three's leftovers** (you asked: fix the remaining slow ones too). The places 0.83.1 left, each checked to give the same result as before:

- **Chord edits with Follow Chord Track:** every note worked out its chord's key and notes again, one note at a time. Now notes under the same chords share one table of answers. A chord edit on the heavy test song: 11 ms -> 4.4 ms (the same pitches, tested note by note).
- **Every undoable edit:** each one compared the whole song with the step before to name it for the Edit History. The names are now worked out only when the Edit History is opened (the same names). An edit with an undo step: about 1.9 ms -> 1.0 ms. One small difference: if more than MAX STEPS edits pass without the window being opened, the oldest step kept reads "Earliest Kept" (the copy it would be compared with has gone).
- **Opening a song:** the recordings are read several at a time, on their own threads, while the plug-ins load (they were read one after another before the first plug-in started). How much faster depends on your disk and how many recordings the song has.
- **The mixer and the track list while playing:** a mixer strip whose meter is the only thing moving now redraws just the meter, not the whole strip; track-list rows are drawn as solid (nothing behind them is painted first), and their state text only when it's in the strip being redrawn.
- **The composer while you HEAR:** only the thin strip the playhead passes through is redrawn in each lane, not the whole lane every frame.

Left as it is: **saving.** The file is already written in the background; what stays on the screen's thread is asking each plug-in for its settings (plug-ins must be asked there) and one copy of the song. That copy could only be avoided by taking the settings away from the song while the file is written, which disabled and frozen tracks rely on, so it isn't worth the risk.

What can't be checked here: timings on your machine (these are this one's), and the song-open speed with your recordings and disk.

## 0.83.1

**Speed, round three** (you asked: the studio running even more efficiently). The studio's own speed tests and three reviews of the code (the audio engine, what happens on each edit, the screen's redraws) found the places where time went; each fix below was checked to give the same sound and picture as before.

- **Fader, pan, send, EQ and BOOSTER drags are much lighter.** Every step of a drag used to rebuild every track's notes and clips in the engine (and the EQ and BOOSTER redrew every view). Now a drag's steps only set the levels; letting go does the full update and the undo step, as before. On a 30-track test song a drag's step went from 0.22 ms to 0.003 ms (more with warped audio).
- **The composer while you hear it:** every change (a fader step too) copied every plug-in's saved settings, Kontakt-sized ones included. It no longer copies them: they were never needed for playing.
- **The cores take the heaviest tracks first.** Each audio block hands the tracks to your computer's cores in turn; a heavy instrument late in the track list (your Kontakt bass) was started last, so the whole block waited on it. Now the tracks that usually take longest start first. The sound is the same to the last bit; the spikes from one heavy plug-in should be shorter.
- **The transport redraws about twice as fast.** Half its time went on painting the pearl surface behind the screens (a large textured picture scaled down 30 times a second); it's now drawn once and reused. Measured here: the whole transport 9.5 ms -> 4.9 ms; the whole window 40 ms -> 19 ms (5760 wide: 50 -> 24).
- **Meters and the MIDI editor while playing:** the track list's and mixer's meters redraw only when a lit segment changes, not on every tiny fall (about half the frames on the track list's meters, fewer on the tall mixer meters); the automation lanes' values only when the number changes; the MIDI editor draws only the strip the playhead passes through, and where following notes sound is worked out once, not on every redraw.
- **Smaller ones:** solo with several soloed tracks (the composer solos up to six) worked out in one pass; the output meters measure the master once, not six times; warped audio clips looked up without writing their markers out as text on every update.

What can't be checked here: how much smoother it feels on your three screens with software drawing; the timings above are this machine's.

## 0.83.0

**The Edit History** (you asked: as the other DAW's Edit > History). Every edit you can undo is listed in a window, oldest at the top: what it was (ACTION), when (TIME), whether it's in force (STATE: DONE or UNDONE) and DETAILS (which track and part, or a note you type).

- **The bar:** a separator in the family's gradient sits under the step the song is at. Drag it **up** to undo back to any point, **down** to redo; the song changes as you drag, so you can listen at each step. Everything under the bar is undone (dimmed) until you make a new edit, which replaces those steps, as Undo always has.
- **Opening it:** right-click **UNDO** or **REDO** in the header, or the **Edit > History** command in KEYS (no key at first: give it one there or on your Stream Deck).
- **Also:** Up / Down move the bar a step; a click at the very left of a row puts the bar under it; double-click DETAILS to type a note ("the good take"). **MAX STEPS** is how many edits are kept (100 as before; up to 1000; the oldest go first).
- **The names are worked out by the studio** from what changed between one step and the next ("Move Part", "Volume", "Edit Notes", "Tempo", "Split"...), with the track and part in DETAILS. When one edit changed several things it reads as the first and names the rest. That's our own wording: a guess at what you did, not your exact words.
- **The same limits as Undo:** going back puts back what Undo always has (parts, notes, the mix, automation, tempo, signature, markers, chords...); adding or removing whole tracks and plug-in settings inside a plug-in aren't undone, so those steps show but going past them leaves the tracks as they are. The list starts again when a song is opened or started.

## 0.82.6

Your two asks from the export talk:

- **Mono export.** The export page has a **Mono** box beside "Include the click": left and right are averaged into one channel. It works for the main mix and for a batch (every channel's file, every cycle marker's file comes out mono), and it's remembered for next time.
- **Glue joins any audio clips.** Before, the glue tool only joined pieces of one recording lying end to end (a split put back); clips from different recordings refused. Now those are joined too: the clips as you hear them (their volumes, fades, warps and the crossfades between them, silence in any gaps) are written into one new file, "<track> glued 01.wav" beside the song's recordings, and one clip of it takes their place. Your original recordings stay untouched on disk, and Undo brings the separate clips back. A split glued back to its own recording still becomes one clip of the same file, with no new file. The glued clip starts at 0 dB with no fades (they're printed in).



**Your keyboard files as you named them** ("keys 1.midi", "pad2.midi", "lead.midi": a file per part). A MIDI file is now the drums only when its name says drums (drum, drums, kit, perc...); any MIDI file named for something a keyboard player plays (keys, piano, pad, lead, synth, bass, arp, strings, organ...) is a keyboard part. Before, "lead.midi" would have been taken for the drums ("lead" only meant the lead guitar's DI) and a synth bass's "bass.midi" likewise. A MIDI file named nothing we know (just the song's name) is still the drums, and a WAV named lead is still the lead guitar's DI. In the review song each keyboard part's track is named after its file ("KEYS: pad2", "KEYS: lead"), so the part kinds stay told apart for the learning.



**The keyboards join LEARN MY BAND** (you have every keyboard part as the players' own MIDI). Nothing has to be heard: the notes, their timing, how hard each was played and the sustain pedal are read exactly as programmed.

- **Finding them:** a MIDI file whose name says keys, keyboard(s), piano, synth, pad(s), organ, Rhodes, strings or keytar is the keyboards ("Song 3 keys.mid", "Song 3 Piano.mid"); any other MIDI file is still the drums. Each keyboard file shows on the song's row with a menu (KEYS / NOT USED), so a file named something else can be put right. A second MIDI file that isn't obviously the drums waits on that menu.
- **In the review song:** a track per part in the file ("KEYS: Piano", "KEYS: Pad"...), each with its own MIDI channel, the notes and controllers (sustain pedal, mod wheel, pitch bend) as they are.
- **Lining them up:** your drum MIDI started 8 bars before the DIs, so the keyboards' MIDI is checked the same way. If the drums' move lines its notes up with the guitars and bass about as well as anything, it gets the same move (files exported from one session). Otherwise it's placed by its own best fit against the DIs. The page says which.
- **Not checked on your files yet:** send a song's keyboard MIDI along with its DIs and drums and I'll check the placement as I did the drums.



**Your songs broken into their parts** (you asked). Every review song LEARN MY BAND writes now opens with its sections marked: a "Sections" marker track with a cycle marker per part, named and numbered (Intro, Verse 1, Pre-Chorus 1, Chorus 1, Post-Chorus 1, Verse 2, ... Solo, Breakdown, Outro).

- **How it finds them:** the drum learner's section finder reads your drummer's MIDI (where the groove changes, crashes, fills into a change, the hand moving between hats, ride and china, half time). Where the rhythm guitar's and bass's riff changes at the same bar line, that tips a close call. Boundaries go onto four-bar lines when they're within a bar of one, pieces under two bars join a neighbour, and neighbouring parts that play alike are one part.
- **How it names them:** the drums give intro, verse, chorus, bridge, breakdown and outro (the part that comes back most is the verse, the bigger repeated one the chorus). Then the band's words: a short part between a verse and a chorus that plays like neither is a **pre-chorus**; a short part after a chorus that plays like no verse is a **post-chorus** (an **interlude** when it plays the intro's groove again); where the lead DI plays far busier than anywhere else, a **solo**. Parts that come back are numbered in order (the third verse is Verse 3).
- **Every name you listed, and more, to choose from:** right-click a cycle marker, **Name as a Song Section**: Intro, Verse, Pre-Chorus, Chorus, Post-Chorus, Hook, Refrain, Bridge, Middle 8, Breakdown, Build-Up, Drop, Solo, Instrumental, Interlude, Riff, Break, Tag, Outro, Ending, Coda. The numbers redo themselves: name a part Verse and the verses are numbered 1, 2, 3 in order again. Drag a marker's edges to move a section; double-click its name to type your own.
- **On your first song:** 17 sections, starting with an 18-bar Verse 1 (no intro found: if the song has one, rename and move it), choruses at bars 83, 99 and 133, a breakdown at 91, the outro at 149. It's a guess from patterns, not from hearing the song as you do: expect to rename and move a few. The corrected sections are what 0.83.0 will learn from, so each player's verse habits and chorus habits are kept apart.



**Djinn Bass 2 tuned to your bass: drop A, A E A D G** (you asked). The profile had A D A D G from the screenshot in 0.62.0; its second string is now E. Your saved profile is updated once when you open the Hub's INSTRUMENTS page or the composer, and everything else in it (keyswitches, range, bend) stays as you set it. If you change the tuning by hand later, it's kept. What changes in use: the composer's bass on Djinn works out strings and frets with an E string, so ONLY STRING 2 and the low-string choices put notes where your bassist would.



**LEARN MY BAND knows your band's tunings** (you told me: every song on a 7-string guitar and a 5-string bass, both in drop A). The reader now only listens for notes those instruments can play: nothing under the low A string, nothing past the 24th fret.

- **Why it matters:** the hardest mistake for a pitch reader is the octave. A low, heavy note can look like the note an octave under it, because the sound repeats at both. On your first song, 264 of the rhythm guitar's 1,883 hits (14 %) came out under the guitar's low A, which a drop A 7-string can't play: those were octave slips. Now a root under the low string is looked for an octave up instead, where it really was.
- **The bass:** a note read a half step or a whole step under the low A is taken as the low A itself (a slack string, or read a hair flat); further under, it goes up an octave.
- **The tunings it assumes:** guitar A E A D G B E (low to high), bass A E A D G, 24 frets. If the bass is strung differently (your Djinn profile has A D A D G), tell me: only the lowest string matters for this release, and that's the A either way.



**LEARN MY BAND, part one: reading your record.** The idea engine is going to learn from what your band actually played. This first part reads the record and shows you what it heard, so you can put it right before anything is learned from it.

- **Where:** the composer's **+ MY RIFF** menu, "Learn My Band: Read Our Record...", or the Window.LearnMyBand command in KEYS.
- **CHOOSE FOLDER...** reads your record's folder: a folder per song, or every song's files together (their names tell them apart). For each song it finds the drum MIDI and the DIs, and guesses who played each DI from its name ("bass", "rhythm", "lead", "gtr 1" / "gtr 2"). Each DI has a menu to put that right. The tempo comes from the drum MIDI (type another if it's wrong).
- **READ THE SONGS** listens to every DI and writes a review song per song (in Documents/Paragon PR7-S/Band Record/<record>/<song>): the drummer's MIDI on a drum track, each DI on an audio track, and under each DI an instrument track with what was heard on it. **OPEN** opens it (asking to save your song first). Put wrong notes right in the MIDI editor, with the DI beside them to compare, and save: those corrected songs are what the engine learns from next.
- **What it hears:** bass and lead lines note by note, down to the bass's low A at 27.5 Hz, repeated picks on one note split, bends read as bends; rhythm guitar hit by hit, its lowest note (a power chord's root, not an octave under), power chords, palm mutes. Palm mutes go in your Shreddage mute layer, open notes in the sustain layer, by how hard they were played.
- **Lining up the drums (from your first song):** your song's drum MIDI started 8 bars before the DIs, and the DIs ran 0.23 % slow against 120 BPM (120 on the click plays as 119.72 in the files: a sample rate a hair off somewhere, inaudible but 0.7 s by the end). The reader finds both by itself, moves the drums into place and fits the DIs to the click, so the song stays at your 120 BPM. It says what it did on the page. On your song, 666 of the drummer's 951 kicks then have a bass or guitar note within 30 ms.
- **Honest limits:** checked on your bass DI, rhythm DI and drum MIDI, and on test sounds; no lead DI yet. Rhythm guitar is the hardest part (chords): power chords and single notes read best, bigger chords less well. Expect to correct some notes. A five-minute song with two DIs takes about a minute and a half to read.


Your four asks from this round:

- **Set the cycle from the MIDI editor** so a passage loops while you edit. The editor's ruler now has the arrangement's cycle strip along its top. Drag in the strip to make a new loop (CYCLE turns on), drag either end's flag to move just that end, drag the range to slide it, and click inside it to turn CYCLE on or off. Ctrl-click anywhere in the ruler sets the left locator and Alt-click the right one, as in the arrangement. **P** (after a click in the editor) puts the loop round the notes you've selected (rounded out to the grid), or round the whole part when none are selected. **Alt+P** does that, turns CYCLE on and plays. The loop also shows faintly down the notes.
- **SNAP on and off inside the MIDI editor.** A new SNAP key sits under the grid box (J after a click in the editor). Lit, notes you move, draw, resize or split land on the grid as before. Off, they go exactly where you put them. Holding Ctrl still gives one free drag with SNAP lit. It's the editor's own setting and is remembered; the header's SNAP is still the arrangement's.
- **The SPLIT tool is a straight razor** (key and pointer, in the arrangement and the MIDI editor): a solid blade with an outlined handle on its pivot. The pointer cuts at the middle of the blade's edge.
- **Stereo renders look stereo.** I checked Render in Place of instrument tracks: the files it writes are stereo (two channels), and a new test proves the left and right sides come through apart. But the arrangement drew every audio clip as one combined waveform, so a stereo file looked mono. Now a stereo recording is drawn with each side in its own half, left over right, when the track is tall enough (at least 24 pixels of waveform), with a faint line between. Mono files still draw as one. One honest note: a guitar plug-in that plays in the middle (both sides the same) renders as a stereo file with two identical halves; its pan isn't in the file, because Render in Place (with Channel Settings) leaves the fader and pan out of the render and puts them on the new track instead.


**Selected automation points are circles** (you asked): the lit points are now pearl-white circles with a dark edge instead of squares, a touch bigger than the unselected points so they stand out. Unselected points stay as they were; where you click to grab a point hasn't changed.

## 0.80.1

**A fix for the freeze in your 0.79.0 log (after Render in Place disabled your second guitar track).** That track was the last one playing PR7-I and PR7-G, so the plug-in host let go of their files. When a plug-in's file is let go, Windows runs the plug-in's own clean-up while it holds a lock every other thread has to wait for; your log shows that clean-up starting the graphics card there, and the NVIDIA driver waiting on that lock for good. Now a plug-in's file, once loaded, stays in memory until the studio closes: disabling, removing or rendering a track still unloads its plug-ins (their sound, memory for samples and CPU go), but the file itself is never let go mid-session, so that clean-up never runs. The session log notes each file kept. A plug-in you've used and removed stays a few megabytes in memory until you close the studio.

The other freeze in the same log (21:34, 11 seconds, then it carried on) was inside PR7-D's own window code, waiting on something of PR7-D's: that one is PR7-D's to fix, not the studio's.

## 0.80.0

Three tweaks you asked for.

**Selected automation points light up, with no box around them (as the other DAW does).** With two or more points selected, the arrangement drew a box round them with seven little handles (tilt, compress, scale, stretch), each a place the mouse could catch. Now only the selected points change: they're pearl white and a size bigger, and nothing else is drawn. The box's tools are still there if you want them: right-click the lane, **Editing Box for Selected Points (Tilt, Scale, Stretch)** switches it on (and off again); the choice is remembered.

**TRACK COLOURS in the Hub: a default colour for each type of track, and its track-list colour.** A new tab under INSTRUMENTS. For each type (instrument, audio, FX channel, group channel, folder):
- **COLOUR:** click it for the family's 75 colours, or AUTOMATIC (red, pink and orange in turn by place, as before). Every new track of that type starts with it, and SET TRACK COLOUR TO DEFAULT in the colour menu gives it back.
- **COLOUR IN THE TRACK LIST** and **HOW MUCH** (1-100 %): new tracks of that type start with their colour over their row (the step further you asked for).
- **APPLY TO THIS SONG'S TRACKS** gives the open song's tracks their type's look now (Undo takes it back); **ALL AUTOMATIC** puts every type back.
- COLOUR TRACKS IN ORDER (the colour menu) still wins while it's on. MIDI, sampler, drum and VCA types don't exist here: drums and samplers are instrument tracks.

**The BOOSTER's values typed in.** Double-click the TRIM knob or the level fader (or the number under either) and type a value, as on the other faders: "-3", "+2.5 dB", "-inf" for silence; Enter sets it, one undo step. Ctrl-click puts it back to 0 dB (a double-click used to; it now opens the box, as everywhere else).

## 0.79.0

**COLOUR IN ORDER: tracks coloured by their number (you asked).** The colour menu's colours are numbered as they're laid out: 1-3 are the wide three (pink, red, orange), then the grid row by row, left to right, 75 in all. Switch on COLOUR TRACKS IN ORDER at the foot of the menu and track 1 takes colour 1, track 2 colour 2 and so on; track 76 starts again at colour 1. While it's on, each swatch shows its number.
- **The range:** FROM TRACK and TO TRACK set which track numbers it covers (all the way up is "to the last", however many tracks come later).
- **Track types:** INSTRUMENT, AUDIO, GROUP, FX and FOLDER can each be ticked or unticked. Unticked tracks keep their own colour and aren't counted, so the coloured ones run through the colours with no gaps (the first track in the range takes the FROM number's colour).
- **It keeps matching:** move a track, add or remove one, undo: the colours follow the numbers. While it's on it wins over a colour picked by hand for those tracks (the menu says so). Hidden tracks and tracks inside a folded folder have no number in the track list, so they aren't counted while they're out of sight.
- It's saved with the song.

## 0.78.3

**The play bar no longer stutters while playing (you reported it).** The play bar took its place from the audio engine, which only says where the song is once per block of sound. At your buffer size (2048 samples) that's about 21 times a second, in jumps of 46 ms, and the screen drew it 30 times a second: some frames it stood still, others it leapt. Now, between blocks, the play bar carries on by the clock at the song's tempo (never more than one block ahead, never past the cycle's end, never stepping back), and it's drawn 60 times a second while playing, in the arrangement and the MIDI editor. Only the play bar's thin strip is redrawn each frame (about 0.3 ms). Its glow was never the cause: it's three thin rectangles.

## 0.78.2

**The Inspector opens laid out as you asked.** The track's basics, INSERTS, EQUALIZERS and BOOSTER are open; ROUTING, CHORDS, SENDS, FADER and QUICK CONTROLS are folded (click a title to open one). It's put in place once when 0.78.2 first starts; after that, whatever you open or fold is kept as before.

**A default template for every new song.** HUB SETTINGS has a new box, DEFAULT TEMPLATE (under DEFAULT DRUM MAP):

- **NONE** (as before): a new song is empty.
- **Any of your templates** (Documents/Paragon PR7-S/Templates): NEW and CREATE EMPTY start from it: its tracks, instruments, effects and their settings, the mixer, the tempo. It's still a new song of its own: saving asks where (or, with CREATE EMPTY, it's saved in its own new folder), and the template itself is never changed.
- **SAVE THIS SONG AS A TEMPLATE...** beside the box saves the song that's open as a template. The naming box has a new choice, **Save as the default**, which saves it and makes it the default in one go.
- If the chosen template is moved or deleted, new songs start empty and the box shows it as NOT FOUND.

## 0.78.1

**The ruler's zoom flipped (you asked).** Drag down in the ruler to zoom in, up to zoom out, in the arrangement and the MIDI editor. The bar you grabbed still stays under the mouse, and a click still moves the playhead.

**Another freeze after Render in Place, inside the graphics card's driver.** Your 0.78.0 log showed the studio stuck 10 seconds after Render in Place disabled the track, on the screen's thread, inside the NVIDIA driver (d3d11), with DRAW WITHOUT THE GRAPHICS CARD on. What was still using the card: JUCE's display sync (it times every window's redraws to the screen's refresh) opened a Direct3D connection to each graphics adapter for every window, even windows drawn by the processor, and closed and reopened it on the screen's thread whenever Windows announced a change. That's where the studio stopped. With DRAW WITHOUT THE GRAPHICS CARD on, the display sync now only listens for the screen's refresh and no Direct3D connection is made at all: nothing in the studio talks to the graphics card's driver. (Switching the setting off makes the connections again for drawing with the card.) I can't make your NVIDIA driver hang here, so this is the fix for what the log shows, not a reproduced case: if it ever freezes again, the log will say where.

## 0.78.0

**Drag the ruler down to zoom out.** Hold the left mouse button in the ruler (the time bar along the top of the arrangement) and drag down: the view zooms out. Drag up: it zooms in. Every 80 pixels halves (or doubles) the zoom, and the bar you grabbed stays under the mouse. A click, or a drag sideways, still moves the playhead as before. The MIDI editor's ruler does the same. Note: the other DAW's manual has it the other way round (down zooms in); this is built the way you asked. Say if you'd like it flipped.

**SOLO EDITOR in the MIDI editor.** A new SOLO EDITOR key in the editor's panel, under INSTRUMENT. While it's lit, only the track you're editing is heard, so you can work on a part without the rest of the song. It follows the editor: open another track's part and that track is the soloed one; close the editor and everything is heard again (the key stays lit for next time). It never touches the tracks' own S buttons, isn't saved with the song, and exports and renders always hear the whole song. There's also a key command for it (MIDI > Solo Editor, no key given yet), for the Stream Deck.

**Render in Place keeps mono tracks mono.** A mono audio track now renders to a mono file on a new mono track; a stereo track (and every instrument) still renders in stereo. Before, every render was a stereo file on a stereo track. The two sides are averaged into one, so a centred mono track comes out at the level it played.

**The BOOSTER.** A new section in the Inspector, right under EQUALIZERS, that works like PR7-C's OUTPUT TRIM:

- **TRIM:** a knob from -12 to +12 dB, in 0.1 dB steps.
- **The fader:** PR7-C's chrome channel fader with its scale (10, 5, 0, -5, -10, -20, -30, -40, -inf), from +10 dB down to silence at the bottom. Shift-drag for fine moves.
- The two add together, as in PR7-C. The boost comes after the channel's EQ and before its sends and fader, so it raises (or lowers) what goes into everything after it. Moves are smoothed (no clicks while you drag).
- Double-click either for 0 dB. Each drag is one undo step. It's saved with the song and carried in track presets. The master has one too.

## 0.77.4

**VISIBILITY lights the selected track.** Click a track in the track list (or the mixer, or move with Up/Down) and its row in the left zone's VISIBILITY tab lights up in the accent colour, and the list scrolls to it if it's out of view. Before, nothing there showed which track was selected.

**The MIDI editor's horizontal scroll bar moved up.** It now sits right under the notes, above the velocity lane and the other controller lanes. Before, it was at the very bottom of the editor, under the velocities, and in a window taller than the screen it ended up off the bottom. The lanes now reach the editor's bottom edge, and the line you drag to make the lanes taller is just above the scroll bar.

## 0.77.3

**A default drum map for every song.** HUB SETTINGS has a new box, DEFAULT DRUM MAP (under DRAG HIGHLIGHT):

- **AUTOMATIC** (as before): the GM Map, or the PR7-D Map on tracks playing PR7-D.
- **GM MAP**, **PR7-D MAP**, or **any map in your Drum Maps folder** (Documents/Paragon PR7-S/Drum Maps, such as a .drm exported from another program): that one map everywhere a drum map is chosen for you. That means new drum tracks from the composer and IDEAS, the drum editor's first open on a track, and Change Instrument. Songs opening also get it, on any track that was on the GM Map or the PR7-D Map.
- A track on a drum map of your own keeps it. If the chosen map is no longer in the folder, AUTOMATIC is used until it's back.

**The horizontal scroll bar always shows.** It was always there, under the MIDI editor and the arrangement, but when the whole song fit on the screen it had no handle and looked missing. Now there's always half a screen of room past what's in view, so the handle always shows and you can scroll on to the right.

**The MIDI editor's tools look like the main window's.** SELECT, DRAW, SPLIT, ERASE, MUTE and LINE in the editor's panel are the same keys as the TOOLS row's, with the same icons. ERASE and MUTE also show the main window's eraser and mute pointers over the notes. Leaving the velocity lane now puts the tool's pointer back; before, it stayed the plain arrow.

## 0.77.2

**The MIDI editor names PR7-D's drums as PR7-D does.** PR7-D starts on its GROOVE LIBRARY note layout (SETTINGS > MAP), where the notes aren't General MIDI's: note 50 is Cymbal 2's mute hit, 27 China 1, 41 the second floor tom, and so on up to 127. The MIDI editor only knew the General MIDI names, so many drum rows were named after the wrong drum (and notes under 27 or over 87 had no name at all).

- **A new built-in drum map, the PR7-D Map:** every note from 6 to 127 named exactly as PR7-D names it (copied from PR7-D's own table). It changes names only: every note still goes to PR7-D as it is, so nothing sounds different.
- **Chosen by itself:** a track playing PR7-D gets the PR7-D Map when the drum editor first needs a map, when you change a track's instrument to PR7-D, and when a song opens whose PR7-D track is on the GM Map (as songs from before this version are). If PR7-D itself is set to GENERAL MIDI, the GM Map stays. A drum map of your own is never swapped.
- **In the MAP menus** (the MIDI editor's and the Inspector's DRUMS): "PR7-D Map (built in)" beside the GM Map.

Not covered yet: PR7-D's ROOT and CUSTOM layouts (choose the map by hand there, or make one in the Drum Map Setup), and switching PR7-D's MAP while the song is open (the names follow the next time the song opens, or pick the map in the menu).

## 0.77.1

**The left side can be grabbed too.**

- **MIDI parts in the arrangement:** the left edge now drags the part's start (the pointer turns into the two-way arrow there, as on the right edge). The notes stay where they are in the song; drag it left to make room before them, right to cut the beginning off (notes before the new start are left out). Before, only audio clips had a left edge; a MIDI part's left side just moved the whole part.
- **Notes in the MIDI editor:** a note's left end drags its start, its end staying put (several selected notes move together; not past the part's start, and each keeps at least a grid step). Before, only the right end lengthened a note.
- The middle still moves, Alt still copies, and each drag is one undo step.

**The MIDI editor's MUTE tool** (as the arrangement's): MUTE in the editor's panel, or key 7. Click a note to mute it (again to unmute; with several selected, the selection), or drag a box: everything it touches is muted, or unmuted when it all was. Muted notes stay in the part, drawn faded with a pale outline, and aren't played (nor exported to MIDI files or dragged out); they're saved with the song. Humanize leaves them where they are.

**The mixer's numbers follow straight away:** moving a fader or typing a value (double-click) now shows the new dB (and pan) under it at once. Before, the number was redrawn only when something else made the strip redraw, such as the next click.

## 0.77.0

**Render in Place always mutes, disables and hides the original, however it's started.**

- **Before:** rendering a whole track (from the track list, the Inspector's RENDER) muted, disabled and hid it, but rendering a stretch (a range over part of a take, selected parts, Alt+R with parts selected) only muted the parts rendered and left the track playing, enabled and in view.
- **Now:** the same every time: the parts rendered are muted, and the track is muted, disabled (its plug-ins unloaded: no CPU) and hidden once everything has been rendered. VISIBILITY shows it again and ENABLE TRACK loads its plug-ins back.
- **The Render in Place page no longer asks** what to do with the original (keep, mute, disable, remove, hide): it says what will happen instead.
- **A warning on the page** when a track has parts outside what you're rendering: disabling the track silences them too.
- Several stretches of one track rendered together (two parts selected) each render with sound first; the track is disabled only after the last one.

## 0.76.1

**A freeze after Render in Place (with Mute or Disable the source) when drawing without the graphics card.**

- **What the log showed:** the render itself finished and the drums track was disabled, then ten seconds later the screen froze inside the NVIDIA driver (Direct2D), the same place as the earlier freezes, even though DRAW WITHOUT THE GRAPHICS CARD was on.
- **Why the setting didn't cover it:** on Windows the framework the studio is built on gives every new window (a menu, the render's progress box) a graphics-card drawing context first, and makes every image (shadows, the screens' glow, icons) on the graphics card, whatever the window draws with. The setting only switched windows over afterwards, so each window that opened or closed (the progress box, right where this froze) still went through the graphics card's driver.
- **Now, with the setting on, nothing is made on the graphics card at all:** new windows are made with the processor's drawing from the start, and images are made in memory. The studio's copy of the framework is patched for this (two lines, the same switch the framework already uses when it runs under Wine).
- It's set before the first window opens, so the whole session is drawn by the processor.

## 0.76.0

**Mixing while the composer plays.**

- **What was wrong:** the composer plays its layers through a copy of the song. Any change to the song, a fader moved in the mixer included, threw that copy away and stopped the playback, so you couldn't hear what you were fixing.
- **Now the composer keeps playing** through faders, pan, inserts, EQ, mute and solo: each change is played straight into what you're hearing. It only stops when its tracks can't play it any more (a track removed), or with the transport's stop.
- **Mute and solo in the mixer count while composing:** a muted layer's track goes silent; soloing one of the layers' tracks plays it alone (the rest of the song stays silent as before).
- **A VOLUME bar on every composer lane:** the fader of the track that lane is heard on, the same one as in the mixer (they move together). Double-click it for 0 dB. Each drag is one undo step.
- **MIXER** (in the composer's header) opens the mixer in its own window in front of the composer, for inserts, EQ and the rest, while it keeps playing.

## 0.75.0

**Your own riff in the composer, the key locked for the song, and chords per section from the key.**

- **+ MY RIFF** (beside SECTION): paste in a riff from a selected MIDI part, or from a MIDI file (or drop a .mid on the composer), then say which section it is ("this is my verse riff"). The composer then:
  - **guesses the key** from it (the riff's low pedal and roots count most). Change KEY if it's wrong before you build: the chords and every suggestion follow;
  - **reads its chords** (a chord every half bar: the low notes' root, its third if you play one, else the key's own chord on it). A chord outside the key (a flat second or flat fifth, say) is kept as you played it and shown **pink**;
  - **the guitar plays your riff** (MY RIFF in its style menu), repeated over the section, exactly as written while RIFF SETTINGS are in the middle;
  - **the drums lock their kick to your riff's hits**, and the bass (locked to the kick) follows them.
- **The next sections come from your riff:** after your verse, the chorus suggested is your riff opened up (fewer palm mutes, looser), a breakdown tightens it into chugs, a second verse brings it back exactly; a new riff for contrast is offered too. The NEXT keys now say which: (MY RIFF), (SAME RIFF) or (NEW RIFF).
- **The key is locked for the whole song** once a section is built (KEY greys out; the summary says "(locked)"). Right-click the SONG strip to start the song again. The composer never changes the song's tempo: it's shown beside the key.
- **Chords change per section, only from the key:** the PROGRESSION menu now offers only progressions whose chords all fit the key, and any chord on the composer's chord ruler can be clicked and swapped for one of the key's own chords (swapped ones are underlined; "Back to..." puts it back). The chords you hear are the ones written onto the chord track when you build.
- The composer is 1700 wide to fit it all.

## 0.74.0

**The composer is now the songwriting suite: IDEAS lives inside it, and a song is built a section at a time.**

- **One window, two tabs: COMPOSE and IDEAS.** Everything the IDEAS window had (piano, guitar, bass, patterns, melody and drums cards; tuning, velocities, humanize, riff, drum and melody settings, solo styles, the drummer, library grooves, BUILD SECTION) is the composer's IDEAS tab. Every way into IDEAS (the chord track's IDEAS key, the pads, the menus) now opens the composer on that tab; the separate IDEAS window is gone.
- **IDEAS deals for the composer's chords**, placed where the next section will go, so a card's USE IT onto a track lands in the right bars. Opened from chosen chords on the chord track, it uses those until the composer's chords change.
- **A card into the composer:** USE IT's menu has "Into the Composer's ... lane". The card goes into its lane (piano and patterns into CHORDS, melodies into MELODY, rhythm into GUITAR, and so on), locked so GENERATE keeps it; the lane reads FROM IDEAS. ROLL or its style menu gives the lane its own again, and so does changing the chords (a card made for other chords wouldn't fit).
- **SECTION** (beside BUILD): intro, verse, chorus, bridge, breakdown, ending or solo. The drums play that section's beat; SOLO also switches the lead on to the whole solo.
- **BUILD <SECTION>** writes the lanes that are on as that section onto their tracks, *after the last section built* (the first at the playhead's bar), with the chords on the chord track: one undo step. The next section is then chosen for you (what usually comes next, with its riff opened up or tightened, as BUILD SECTION suggested before) and the status line says why. Change it with SECTION or the NEXT keys.
- **SONG strip:** the sections built so far in order (where each starts and how long it is), and up to three NEXT keys. Right-click the strip to forget the sections (the song's parts stay; the next BUILD goes at the playhead again).
- USE ALL still writes at the playhead's bar, as before. HEAR now plays where BUILD would put the section.

## 0.73.4

- **Menus no longer flicker with DRAW WITHOUT THE GRAPHICS CARD on.** Every new window (a menu, a submenu, a question box) is made by JUCE with the graphics card's drawing, and the studio switched new windows to the processor's drawing only once a second, so a menu was first drawn one way and redrawn the other a moment later: the "mini glitches". Menus and question boxes are now switched the moment they're made, before they're first drawn, and anything else is checked four times a second instead of once.

## 0.73.3

- **The session log names a plug-in that freezes while loading** (your Kontakt 8 log). Loading a plug-in's file, and giving a plug-in its saved settings, are now marked steps, so if the studio freezes in the middle of either, the freeze report says so under "Under way" ("Loading the instrument Kontakt 8 (...)", or "Giving Kontakt 8 its settings"), with how long it had been at it. Before, they were plain notes and the report said "nothing marked".
  - This doesn't fix the freeze itself: your log shows it stopped inside Kontakt's own start-up, waiting for something else. It makes the next report say exactly which step it was in, which tells us whether it's the loading or the settings.
  - The log now writes each load as "Starting: ..." and "done: ... (n ms)", so you can also see how long every plug-in takes to load.

## 0.73.2

- **OUT holds the loudest peak since you last clicked it** (you asked), as a mixer's max readout does. It only ever goes up; **click OUT** to start again (it reads -INF until the next sound). It turns red over 0 dB, so a clip anywhere in a pass stays on screen until you click it away. The screen now reads **DB MAX**, and its tooltip says so.
  - It's still read 30 times a second (no peak is missed), and it's redrawn only when a louder peak comes, so it's the lightest it's been on the CPU.

## 0.73.1

- **OUT is one number now** (you asked): the two L/R bars are gone; the screen shows the output's peak in dB, large, as CPU shows its percent (red over 0 dB, -INF in silence).
- **It flickers less** (you: it moved more than it needs to). Before, the number was worked out 30 times a second and showed the peak held for 1.5 seconds, then fell with the meter, changing every 30th of a second as it fell. Now the output's peaks are still read 30 times a second (so no peak is missed), but the number is worked out only about **four times a second**, and it's the **loudest peak of the last second** (left and right together). So a hit holds for about a second, and the number steps rather than slides.
  - It also redraws its screen at most four times a second (before: up to 30), so it's lighter on the CPU than 0.73.0's.

## 0.73.0

- **TAP, M and S on the header's top row** (you asked), right of the quantize box:
  - **TAP** taps the tempo as before; right-click it for the tap modes.
  - **M** and **S** turn every mute and every solo off at once (they were in the track list's header). Each lights while something is muted or soloed.
- **The output meter is an LCD now** (you asked), the same size as the CPU screen and right beside it, so there are two screens either side of POSITION and TIME. It reads **OUT**, the held peak in dB (red over 0 dB), and a bar each for L and R.
  - **CPU:** it costs less than the old meter, not more. It's only redrawn when the level it shows changes (the old meters were two components redrawn on their own), and it reads the same peaks the old meter did.
- **Tracks are numbered** (you asked): 1 at the top, counting down. The number belongs to the place, not the track: move the third track to the seventh place and it reads 7. Folder tracks and FX/group channels count as places too. Folded-away and hidden tracks aren't numbered (they aren't in the list).
- **Double-click a track's number and type another to move it there** (you asked). Type 5 on track 97: it goes to place 5, and the tracks from 5 to 96 move down one. Moving down works the same way. A folder takes its tracks with it. Undo puts it back.
- **The track's colour in the track list** (you asked, as the other DAW's): in the colour menu (the colour key in the track list's header, or the track menu's Colour), **COLOUR IN THE TRACK LIST** and a bar from **1 % to 100 %**. It's per track: only the tracks the menu was opened for (the selected ones) change. Off, the row stays as it was: dark, with the colour down the left edge. Saved with the song.

## 0.72.0

- **AUTO-SAVE** (you asked), on **HUB SETTINGS** under HUMANIZE:
  - **How often:** OFF, or every 1, 2, 5, 10, 15 or 30 minutes (every 5 to begin with).
  - **How many kept:** the newest 3, 5, 10, 20 or 50 copies of each song (10 to begin with); older ones are deleted.
  - **SHOW BACKUPS** opens the folder the current song's backups go into.
  - **Backups are copies:** your song's own file changes only when you SAVE. They go into a **Backups** folder beside the song (Documents/Paragon PR7-S/Backups for a song not saved yet), named with the song and the date and time ("Riff backup 2026-11-07 14-05-00").
  - A copy is made only while the song has unsaved changes, never while recording or rendering, and not again when nothing changed since the last one.
  - **To go back to one:** OPEN it (from the Hub's CHOOSE FILE... or the studio's OPEN), then SAVE AS. A backup remembers where the song's recordings are, so it opens with them; SAVE AS copies them beside the new song.
- **An X on every automation lane** (you couldn't find how to delete one): in the track list, beside the lane's M. It removes that lane and its points only; Undo puts it back. (It was already in the lane's menu, as Remove Parameter: click the lane's name, or right-click the lane.)

## 0.71.1

- **Clicking the info line's VALUE (or START) opens it for typing now** (you: 50 clicks, nothing). The info line refreshes ten times a second; each time it rebuilt its fields but, when their text hadn't changed, didn't give them their places on screen again, so a click found no field there (a right-click doesn't look for one: that's why HIDE worked). They're placed on every refresh now. A test now does exactly what you did (the line refreshes, then a click on VALUE) and failed before the fix.
- **Bebas Neue everywhere** (you: unless lowercase is needed). The rule is now built in: any text without a font of its own is Bebas, so JUCE's own boxes are too (SETTINGS' DEVICE / SAMPLE RATE..., the EQ window's value boxes and its scale, text boxes, tooltips), along with every page that still used Inter (the EQ window, the chord pads' key list and suggestions, IDEAS' cards, the MediaBay and Media rack, the composer, the progressions, LEARN A DRUMMER).
  - **Kept in Inter because small letters matter:** chord names and Roman numerals (Cm7 is minor, CM7 major; ii and II are different chords), and a secondary dominant's reason, which names its numeral. The LED screens' figures stay as they were.
  - **Keys are spelled out** in the key list and scale names ("B-FLAT MAJOR"): Bebas has no small letters, so "Bb" would read "BB", and neither font has a ♭ sign.
  - Two suggestion reasons are now in words ("the natural minor's minor five: a softer pull than the major five"; "deceptive: the five goes to the minor six instead of home") so they read right in capitals.

## 0.71.0

- **The info line opens for typing with one click** (you asked: double-click VALUE to type). A click (or a double-click) on VALUE or START opens it as a clear box, white on black with an orange edge; type, Enter. This worked on the test machine with real mouse clicks, even after clicking a point first, so we couldn't see why it didn't for you on Windows. The box now also takes the keyboard back if something steals it just as it opens, which is our best guess at what happened. Tell us if it still doesn't open.
- **Mixer faders move in half-dB steps when dragged** (you asked). Typed values (double-click the fader) take any value, and automation isn't rounded.
- **The cycle strip's red / pink / orange gradient in place of the plain orange** (you asked) on the faders (orange at the foot, pink at the top), the pan knobs, every value bar (the EQ window's GAIN, FREQ, Q, OUTPUT; the Inspector's VOL and PAN...), the lit lights (MULTI-CORE AUDIO, the channel tick boxes) and the item under the mouse in every drop-down menu.
- **Drop-down menus in the studio's look:** the dark panel with the question boxes' red edge, and the item under the mouse lit in the gradient. We checked every menu in PR7-S goes through this look. **The menu in your PR7-D picture is PR7-D's own** (a plug-in draws its own menus inside its window): it needs the same change in PR7-D.
- **EQ presets in the Inspector** (you asked): EQUALIZERS has **BYPASS | PRESETS | EDIT**. PRESETS (also in the EQ window and on the section's title, right-click) now has **Starting Points**, our 20 common moves by source (lead vocal, backing vocals, scream, telephone voice, kick, snare, toms, overheads, drum bus, bass, rhythm and lead guitar, acoustic, piano, pads, master, a low cut), as well as your own saved presets. They're starting points: adjust by ear.
- **The EQ curve shows the ordinary pointer** (you asked), not a cross-hair; the frequency, note and level readout stays at the top right.
- **What a song opened without can be removed** (you asked). The "Some instruments, effects or recordings are missing" box now has **REMOVE THEM** and **KEEP THEM**. REMOVE THEM empties the missing effects' slots, deletes the clips whose recording is missing, and gives a track whose instrument is missing the studio's synth (its MIDI parts stay). KEEP THEM leaves the song as it was, for when they're installed. Undo puts the clips back (plug-in changes aren't undo steps anywhere in the studio).
- **The mixer, MIDI editor and chord pads open docked at the height you showed:** the ruler, the master's row and five full tracks above them (at your vertical zoom). The heights dragged before are reset to this once; drag the line to change it again.
- **SETTINGS and KEYS in the Hub's look** (you asked), as PLUGINS: the pearl header with the mark, and the page in the Hub's red-edged panel.

## 0.70.0

- **Solo works by routing** (you asked). Solo a track and you hear only that track, everything routed into it, and the channels it goes on through to the master. Solo your vocal group and you hear only the vocals routed into it: no other vocals, no instruments. Groups into groups work too: solo a bus and every group and track feeding it plays.
  - A soloed track still feeds its own group and the FX channels it sends to.
  - Soloing an FX channel (a reverb) keeps the tracks that send into it playing, or the reverb would have nothing to play. That's our choice; the other DAW's manual doesn't say.
  - Tracks routed into a soloed group lose their sends to FX channels outside the group (the reverb isn't on the group's path), so a soloed group plays dry unless the group itself sends to the reverb.
- **Top and Bottom** (the other DAW's Navigate > Top / Bottom): select the first or last track in the track list and scroll to it. They're in KEYS under NAVIGATE, with no key yet: the other DAW uses Home and End, but Home is your Go to Start, so set your own (End is free).
- **The mixer and the track list follow each other:** click a channel in the mixer and the track list (and arrangement) scrolls to that track and highlights it. Pick a track in the track list (or with Up/Down, Top/Bottom, or a new track) and the mixer scrolls to its channel.
- **The composer's lead lane** has **Solo Style** at the bottom of its style menu (the same setting as IDEAS' SOLO box). Choosing one switches the lane to the solo so you hear it.

## 0.69.1

- **The SOLO box is where you can see it** (you couldn't find it). It's now on the right of the **LEAD** column's title, on IDEAS' guitar tab. In 0.66.0 to 0.69.0 it sat in the row of tabs, and an IDEAS window narrower than 1600 (yours was about 1360) squeezed it to nothing. A test now checks it's visible, readable and uncovered at widths from 1000 to 1600.
- The IDEAS window opens 1360 wide again.

## 0.69.0

- **Solo styles, round four, the last of your list** (the names are ours). Four more in SOLO: on IDEAS' guitar tab (and the composer's lead lane), sixteen in all plus BY STYLE:
  - **GYPSY SHRED:** melody first, phrases landing on the chords' thirds, gypsy-jazz minor colour (natural minor's 2 and b6, harmonic minor's raised 7th), the higher phrases harmonised a third above, picked sweeps of each chord's arpeggio in triplets, octave bends and a whammy dip. No legato (the player calls it his weak spot).
  - **TENSION & RELEASE:** note choice over speed. High, clean phrases, each one's last note approached from a dissonant half step above, clean picked runs held back for later.
  - **TORTURED BENDS:** short and singable, a scene change in the song. Slow bends held, some left a quarter tone short so they rub against the chord, wide dark vibrato, a phrase harmonised a third above, one quick burst.
  - **GRUNGE MELODY:** sparse and simple. The opening tune played again through the solo (an octave up in the second half), the way a vocal melody comes back; a chromatic passing note now and then; vibrato only on the last note; no whammy at all. One take in four (MORE IDEAS) is a noisy one, with a burst of notes thrown around the scale.
- **The research** (`docs/solo-styles`): two of these players are well covered in their own words; the other two less so (interviews only, or writers and bandmates), and their sheets say so. As before, note counts, bend sizes and vibrato are mostly our estimates: tune by ear.
- **What a MIDI part can't do:** the wah, feedback, noise and amp sounds these players use. The solos give the notes, bends and vibrato; the tone is yours.

## 0.68.0

- **Solo styles, round three** (you asked; the names are ours). Four more in SOLO: on IDEAS' guitar tab (and the composer's lead lane):
  - **GLAM FLASH:** sustained notes and long bends held with vibrato, lyrical double stops in thirds, melodic answers, and the one fast picked run saved for the very end.
  - **SLEAZE BLUES:** a bluesman's melody you could hum. Two-bar phrases and their answers, a bend held on most bars, room left for a singer, one whammy dip (down a semitone and back, "like a voice"), a quick run near the end and a long held note to finish.
  - **TOP HAT MELODY:** melody first. Slow, loose bends and held notes laid back a little behind the beat, a motif played again higher, then a wilder second half of four-note runs climbing the scale. Major pentatonic over major chords.
  - **PURPLE SCREAM:** restrained, then soaring. The blue notes (b3, b7) over major chords, a motif coming back higher each time, bends that grow (a half step, then a whole step, then a step and a half), rising flurries, one outburst outside the key at the peak, and a screaming high note to end.
- **The research was thinner this round** (the sheets in `docs/solo-styles` say so at the top): one player's habits come mostly from a magazine feature and lesson descriptions, and another rarely spoke about guitar, so bandmates and writers fill in. These four lean more on our own estimates than the earlier eight: tune by ear.

## 0.67.0

- **Solo styles, round two** (you asked; the names are ours). Four more in SOLO: on IDEAS' guitar tab (and the composer's lead lane):
  - **SCREAM & DIVE:** aggressive groove-metal lead. The blues scale with the major 3rd and 6th, slow half-step bends from below, a lick shifted up a fret every beat (outside the scale as it climbs), a harmonic screamed up a minor third with the bar, and a dive bomb to end.
  - **THRASH FIRE:** comes in on an upbeat with a vocal-like pentatonic melody, then a picked triplet lick three beats long, repeated so it starts somewhere new in each bar. Wide vibrato, and it ends high.
  - **PINCH SQUEAL:** a pentatonic melody first, then picked six-note runs down the boxes (each group a step lower, drifting against the beat), pinch-harmonic squeals shaken wide in the gaps (one every two bars at most), very wide vibrato on phrase ends. Major pentatonic over major chords.
  - **PROG PRECISION:** natural minor (its 2 and b6 heard), singing in-tune bends, then picked six-note scale sequences climbing two steps at a time and seventh-chord arpeggios up and back down, building to a chromatic climb at the climax.
- **The research** is in `docs/solo-styles` (a sheet per player, 16-21 sources each, several from the players' own magazine columns). Techniques only, never their solos. As before, note counts, bend sizes and vibrato speeds are mostly our estimates: tune by ear.
- **Every style's fast passages now come later** in a short solo (they're placed counting back from the end), so even an 8-bar solo builds.
- **Dives and screams** need the bend range set as for 0.66.0: Shreddage's Pitch Bend Range and the INSTRUMENTS profile's BEND RANGE the same, 12 or more.

## 0.66.0

- **Solo styles, round one** (you asked: solos in the manner of players you love). On IDEAS' guitar tab, **SOLO:** chooses how the whole solo (the last lead card, and BUILD SECTION > SOLO) is written:
  - **BY STYLE:** as in 0.65.0, made from your STYLE's six leads.
  - **SOARING BENDS:** slow and spacious. A singing motif that keeps coming back, long notes bent slowly into their pitch, restrained vibrato, climbing to big bends and one short fast run near the end.
  - **ROCK 'N' ROLL SWAGGER:** loose, swung bluesy rock. Short call-and-response phrases, the major 6th and both thirds, 6th double stops, repeated licks midway, a tremolo-picked accent.
  - **TAP & DIVE:** melodic phrases traded with tapped arpeggios that follow the chords, legato runs, a harmonic now and then, big bends, ending on a pinch harmonic and a whammy dive.
  - **BLUES HAMMER:** major and minor mixed, flat thirds curled sharp, a six-note lick repeated so it lands somewhere new in each bar, big over-bends near the climax.
- **How a solo is built** (the same for every style, as players build one): a short motif is stated, answered (the same rhythm, a new ending), stated higher, then developed. The style's signature passage comes in the later phrases. Notes get busier and higher through the solo, chord tones sit on the downbeats and phrase ends, and the last note is the last chord's root, held.
- **The names are ours.** The research behind each style is in the repository (`docs/solo-styles`): a technique sheet per player, from guitar lessons and interviews. No player's solo is copied; only habits (scales, phrasing, bends, vibrato, techniques). Note counts and some details are our estimates, marked as such in the sheets: tune by ear.
- **Bends by size:** a bend can now be a half step, a whole step, or 1.5 to 2.5 steps. It's written in proportion to your INSTRUMENTS profile's BEND RANGE. A bend bigger than the range is written higher and bent only as far as the range reaches, so it still lands on its note.
- **Dive bombs:** the whammy dive is pitch bend down an octave, so it needs a big range. Set **Shreddage's Pitch Bend Range and the profile's BEND RANGE to the same number, 12 or more**. With the default 2, the dive only drops a whole step.
- **Shreddage's technique keys:** Tapping (A-1), Tremolo (F#-1) and Harmonics (G#-1) are pressed before the notes that use them, then Sustain (C-1) again. Staccato (D-1), Choke (G-1) and FX (A#-1) are listed too. **To get them, add Jupiter or Rogue again from + NEW in INSTRUMENTS** (a profile you already have keeps its old keys). Each key's USE menu has the new uses: Tapping, Tremolo, Harmonics.
- **Vibrato depth** now varies by style (some styles hold back on high notes).
- **The composer:** the LEAD lane's Solo style follows SOLO.
- The IDEAS window is wider (1600) to fit the new box.

## 0.65.0

- **A SOLO section for guitars** (you asked). SOLO is now a song section next to INTRO, VERSE, CHORUS, BRIDGE, BREAKDOWN and ENDING.
- **BUILD SECTION > SOLO** writes:
  - **Lead Guitar: a whole solo.** It's made from your STYLE's own six leads, ordered from the calmest to the busiest, a lead every two bars. So it starts melodic and climbs to the style's fastest playing at the end:
    - GROOVE and NU-METAL: octave melodies, sirens and pinch pings.
    - THRASH: tremolo and chromatic runs.
    - SHRED: sweeps, legato and tapping.
    - ROCK: pentatonic and blues.
  - **Bends and vibrato:** each two-bar phrase's longest note is bent up a whole step into its note, other long notes get vibrato, and the last note rings to the end with vibrato. On a track these play as pitch bend and the mod wheel, as in 0.64.0.
  - **Guitar: your riff, thinned out under the solo**, so the lead has room.
  - **Drums: a SOLO beat**, driven on the ride. A learned drummer plays the section you marked SOLO on LEARN A DRUMMER, else their chorus.
  - **Bass:** locked to the kick, as in the other sections.
- **NEXT suggests a solo** after the second chorus or after a bridge, and after the solo, the chorus again, a breakdown or the ending. One solo per song.
- **The solo is also the seventh lead card** on IDEAS' guitar tab: HEAR it, or USE IT anywhere.
- **The composer:** the LEAD lane's style menu has **Solo**, and the DRUMS lane has the **SOLO** beat.
- **LEARN A DRUMMER** can now mark a section of a song as SOLO.

## 0.64.0

- **A new guitar STYLE: GROOVE** (you asked, after Machine Head's "Ten Ton Hammer" and Kittie's "Brackish"). Palm-muted chugs on the low string, answered by a high, dissonant two-note **stab** two octaves up. Six riffs, all ours, written in the spirit of those songs rather than copied:
  - **Tritone stab:** a ringing tritone with vibrato, then four chugs (the Ten Ton Hammer idea). On your 7-string in A: A2 + D#3 over the low A.
  - **Bent stab:** a chug, then a whole step (the flat 7th and the octave) bent up a whole step, the phrase ending on a ringing unison note (the Brackish idea). In A: G2 + A2 bent up to A2 + B2, then B2.
  - **Backbeat stabs** (on beats two and four, with the snare), **Off-beat stabs**, **Stab gallop**, **Half-time stabs** (the breakdown).
  - The stab sits over the riff's low note (the key's note on the lowest string), so it follows the key you're in.
- **Bends and vibrato are written into the part** when a riff goes onto a track (USE IT, BUILD SECTION, the composer, dragging a lane out as MIDI):
  - **A bend** is pitch bend: straight at the attack, up a whole step over the first half of the note (an 8th at most), back to none as the note ends, before the next chug. It uses the BEND RANGE in the guitar's INSTRUMENTS profile (2 for Jupiter and Rogue, Shreddage's default: so the bend goes all the way up), else 2.
  - **Vibrato** is the mod wheel (CC 1), Shreddage's vibrato controller: none for the first third of the note, rising after, back to 0 as it ends.
  - A pitch bend bends the whole channel. The riffs never start a chug while a stab is bent, so only the stab bends.
- **RIFF SETTINGS and the composer's ROLL leave the stabs alone:** PALM MUTES never mute them, BUSYNESS never drops them, and LOW STRING never folds them down.
- **The rest of GROOVE** borrows from NU-METAL: its leads (octave melodies, sirens, pinch pings), drum beats, beat generator rules and fills. FROM MY LIBRARY looks for grooves named groove, Pantera, sludge, nu-metal or bounce.
- **Not yet:**
  - IDEAS' HEAR previews play the notes without the bend or vibrato. The part on a track, and the composer's HEAR, play them.
  - A unison is written as one note: two of the same note on one channel would cut each other off.

## 0.63.1

- **Nothing faster or shorter than a 16th on your libraries** (you asked: for Shreddage and Djinn Bass the shortest a note should be is a 16th, and 16th chugs should be rare). Each INSTRUMENTS profile now has two settings:
  - **SHORTEST NOTE** (any, 1/32, 1/16, 1/8; 1/16 for Djinn, Jupiter and Rogue):
    - A hit starting less than that after the one before is left out. A hit is a note, or notes played together.
    - Every note is at least that long, unless the next hit or the end of the part comes first.
    - A few milliseconds early still counts (HUMANIZE moves notes slightly), so a humanized 16th isn't dropped.
  - **16TH CHUGS IN A ROW** (any, 1-4; 2 for Jupiter and Rogue): palm-muted hits (velocity 1-59) a 16th apart may come at most that many in a row. Later ones in the run are left out until a longer gap or an open note breaks it.
    - So a gallop (an 8th then two 16ths) and a pair of 16ths still play.
    - Machine-gun runs of 16th chugs get thinned to pairs.
    - The 32nd- and 16th-triplet bursts in the metal styles' fills are mostly left out on these guitars.
- **Where:** the composer's BASS, GUITAR and LEAD lanes, and IDEAS' bass and guitar tabs, whenever the lane's INSTRUMENTS profile is used. The bass gets the shortest note only; it has no chug limit.
- **Your saved profiles:** Djinn Bass 2 from 0.62.0 picks up 1/16 by itself. Guitars added before this version pick up 1/16 and two chugs.
- **Also in this release (0.63.0):** power chords played by Shreddage's power chord keyswitch (C#-1), single notes back on Sustain (C-1).

## 0.63.0

- **Power chords played by Shreddage's power chord articulation** (you asked: its recorded power chords sound better than three notes on Sustain). For a guitar with an INSTRUMENTS profile (your Jupiter and Rogue):
  - **Each power chord is written as its root alone**, after your Power Chords key (C#-1), so Shreddage plays its own power chord sample. A power chord here means notes starting together: a root and its fifth, with octaves of either.
  - **Single notes and other chords go back to Sustain (C-1).** The keys stay on until changed, so the studio adds a key only where the riff switches between power chords and single notes, plus one before the first note so the guitar starts in the right mode.
  - **A key sits on the same beat as its note.** The manual says Shreddage takes keys first, so nothing is nudged.
- **Velocities carry over:** the root keeps the loudest velocity of the chord it replaces. You told me C#-1 has the same velocity layers as C-1, so palm-muted chugs stay palm-muted power chords (1-59) and open ones stay open (60-119).
- **Left as written:** octave chords, the tritone dyads, the higher chord shapes and strummed shapes. They aren't power chords (a strum's notes don't start together), so they stay on Sustain as full notes.
- **Where:** the composer's GUITAR and LEAD lanes, and IDEAS' guitar tab (rhythm and lead), whenever the INSTRUMENTS profile used for that lane has keys marked **Power chords** and **Single notes**. The keys are written onto the track and into dragged-out MIDI files.
- **INSTRUMENTS page:** a keyswitch can now be marked "Power chords (a root alone)" or "Single notes".
  - Jupiter and Rogue from + NEW come marked.
  - **If you added them before this version**, add them again (or mark C#-1 and C-1 yourself), or no keys will be written.
- **Worth knowing:**
  - Shreddage's power chords are root, fifth and octave, so they'll sound a little bigger than the root-and-fifth chords written before.
  - IDEAS' HEAR plays its preview through the selected track's instrument. On a track that isn't Shreddage, the two keys (C-1, C#-1) sound as very low notes.

## 0.62.2

- **Shreddage Jupiter and Rogue set up from the Jupiter manual and your map.** The presets on + NEW now have:
  - **Your articulations:** Sustain on C-1 (palm mute 1-59, sustain 60-119, rakes 120-126, pinch harmonics 127) and Power Chords on **C#-1**. 0.62.1 had Power Chords on C-2: that was a mix-up.
  - **The manual's force-string keys** (its Behavioral Keyswitches, page 34): C-2 forces the low A string, and each key up forces the next string, to F#-2 for the high E. G-2 turns forcing off.
  - **Pitch bend range 2**, the manual's default.
- **ONLY STRING on the guitar lane now drives Shreddage's string:** the string's force key is held from the part's start. Then **G-2 is pressed just after the part's last note starts**. Shreddage's force keys latch (stay on until turned off), so without G-2 the guitar would stay stuck on that string for everything played after the part.
- **A keyswitch can now be marked "Turns forcing off"** on the INSTRUMENTS page, for any library that has such a key.
- **Already in your profile list?** If you added Jupiter or Rogue with 0.62.1, delete it and add it again from + NEW to get these keys.
- **Not checked:** Rogue's map is assumed to be the same as Jupiter's (the same Shreddage 3.5 engine); the manual sent was Jupiter's. If Rogue differs, change its keys on the INSTRUMENTS page, or tell me.

## 0.62.1

- **Shreddage Jupiter and Rogue in INSTRUMENTS** (as you gave them). Each is one click on **+ NEW** in the Hub's INSTRUMENTS tab, set up as follows:
  - **Tuning:** 7-string drop A, A0 E1 A1 D2 G2 B2 E3.
  - **Frets:** 24, so either guitar is written from A0 to E5.
  - **Velocity layers:** 1-59 palm mute, 60-119 sustain, 120-126 rakes, 127 pinch harmonics. They start from your VELOCITIES values, so any by-ear changes you made there come along.
  - **Keyswitches:** Sustain on C-1 and Power Chord Sustain on C-2. Both sit below the lowest string, so the composer never writes a note on them.
- **Which guitar plays which lane:** mark one for the composer's GUITAR lane and the other (or the same one) for the LEAD lane. Each lane then uses that guitar's range and velocity layers.
- **Not yet:**
  - Neither guitar has force-string keys, so ONLY STRING on the guitar lane moves the notes onto that string without a keyswitch.
  - The two articulation keyswitches are kept, but nothing switches them yet. A part written for Sustain plays as written, as long as Shreddage is on Sustain.

## 0.62.0

- **INSTRUMENTS in the Hub** (you asked: rules for each instrument you use). A third tab beside PROJECTS and HUB SETTINGS, with a profile per sample library. Each profile holds:
  - **name** and kind (bass or guitar);
  - **strings**, low to high, typed as notes;
  - **frets**;
  - **lowest and highest note** it may play (empty: from the lowest string to the top string's last fret);
  - **bend range**;
  - **keyswitches**: each a name, its note, and whether it's an articulation or forces a string;
  - for guitars, **velocity layers**;
  - which composer lane it's **used for** (BASS, GUITAR or LEAD lane; each lane uses one).
- **Djinn Bass 2 is built in**, set from your screenshot: your tuning A0 D1 A1 D2 G2, 24 frets, bend range 4, and its keyswitch map. The articulations Djinn leaves out (shown grey there) aren't listed. Its five Force String keys force strings 1 to 5. It's used for the composer's BASS lane from the start, so the bass is written only from A0 to G4 and never lands on a keyswitch key.
- **Note names:** Djinn names notes as the studio does (key 0 is C-2, middle C is C3), so your A0 is the same note in both: MIDI note 33.
- **The composer's string menu** (the BASS and GUITAR lanes; it was LOW STRING) has three choices:
  - **Any string.**
  - **Low string:** as much as it can.
  - **String 1 to 5 only:** the whole part on that string, one note at a time; chords keep their lowest note. With an INSTRUMENTS profile, that string's keyswitch is held from the part's start to its end. It's written onto the track and into dragged-out MIDI files, so Djinn plays the string you chose whether its keyswitches latch or only last while held. The lane says which keyswitch it holds.
- **The bass tunings now show their notes** (e.g. "4-string standard (E0 A0 D1 G1)"), so it's clear they sit an octave under where Djinn puts its strings. When an INSTRUMENTS profile is used for a lane, its tuning and range win.
- **Not yet:**
  - The other keyswitches (slap, pop, palm mute...) are kept but no rule uses them yet.
  - IDEAS doesn't use the profiles yet; only the composer does.
  - Next: profiles for your Shreddage Jupiter and Rogue, as you asked.
- **Worth checking by ear:** play an A0 into Djinn from the MIDI editor. It should sound your open low string. If it sounds an octave off, change the strings on the INSTRUMENTS page.

## 0.61.0

- **The composer's ROLL makes a new take and keeps the approach** (you asked). On BASS with Root and fifth chosen, ROLL now plays another Root and fifth part over the same chords: another rhythm, other notes between the roots, other lead-ins into the next chord. The approach only changes when you choose it in the menu beside ROLL. GENERATE does the same for every unlocked lane, and both keep the PROGRESSION too (choose another in its menu).
  - **Why the bass and the chords were rebuilt:** IDEAS' bass and piano parts had only one to six written versions of each approach, so rolling kept landing on the same riffs. In the composer they're now made from rules per approach (where its notes may fall in a bar, which notes it may use, how it gets to the next chord), and each roll draws a new part within them. Parts repeat like a player's would: a bar, the same bar or a small change of it, the bar again, and a fill leading into the next phrase. In our tests ten rolls of every approach gave at least seven different parts, and every chord's first bass note stays on its root.
  - **Guitar, lead, melody and drums:** melody, rock leads and drums already drew new parts per roll. Rhythm guitar now also changes a few hits per take (a root moved to its octave or fifth, or in the metal styles to the flat second or minor third, and an accent turned palm muted or open), at the same places in every bar so the riff still repeats. The other styles' leads start somewhere else in their line, higher or lower, and swap their two rhythms.
- **BUSY on every lane:** from sparse to busy, kept when you roll. 50% is the approach as it usually goes. On the bass and chords it sets how many notes each bar draws. On the guitar it's RIFF SETTINGS' busyness, on the melody its density and on the drums the generator's density (the kicks).
- **LOW STRING on the bass and guitar lanes:** played on the lowest string as much as it can be (from the open string to the 12th fret). On the bass the roots go there and fifths go under the root where they fit. Only the Octaves approach's high notes leave the low string, because they are the approach. The key's menu also holds the tuning. **The bass now has a tuning of its own:** 4-string standard, 5-string standard, 4-string drop D, 4-string drop C, 5-string drop A or 6-string. It sets the bass's range (the lowest string to two octaves over it), so nothing is written below your bass's lowest note.
- **Not yet (0.62.0, as planned):** the Hub's INSTRUMENTS profiles (each instrument's range, tuning, velocity layers and keyswitches), and a riff played on one chosen string through keyswitches. Those wait for Djinn Bass 2's keyswitch list.

## 0.60.0

- **Routing with no limits but loops, as the other DAW's** (you asked: any track to any track, never itself). Any track that makes sound (audio, instrument, group channel, FX channel, a folder with its group channel) can now have its output go to the master, **any group channel, or any FX channel**. Groups into FX channels, FX channels into groups, and FX channels into other FX channels all work.
  - **FX channels have an OUT and sends of their own now** (in the mixer, the Inspector and their right-click menu), so an FX channel can send to another FX channel too.
  - **The one rule left:** nothing may lead back into itself, by its output or by a send. The menus leave out (or grey, marked "would loop back") a destination that would make a loop. A song with a loop in it opens with the loop broken.
  - **Order:** the mixer works out the order the channels are mixed in whenever routing changes, so every channel is mixed after everything feeding it, whatever order the tracks sit in, on one core or several.
  - **What the other DAW doesn't do either:** an audio or instrument track isn't an output destination there (its output goes to busses, groups or FX channels; a group can feed an audio track's *input* for recording a submix). That's left out here too; say if you want it.
- **NEW froze the studio (your log): a switch to test the likely cause.** The log shows the screen's thread waiting inside the graphics card's driver (NVIDIA's DirectX driver) just after the new song was set up: not in the studio's own steps, which had all finished. Drawing the whole studio at your three screens' width right after NEW takes 30 ms here and nothing in it is drawn oversized, so it couldn't be reproduced without your graphics card. New in SETTINGS: **DRAW WITHOUT THE GRAPHICS CARD**, which draws every studio window with the processor instead. If NEW stops freezing with it on, the driver is to blame (updating the NVIDIA driver may then fix it with the switch off again); if it still freezes, the log will say where now.

## 0.59.0

- **Groups into groups** (you asked: "John verse vox" and "John chorus vox" both into a "John vox" group, and that one into the master). A group channel now has an OUTPUT of its own: in the mixer (OUT on its strip), in the Inspector, and in its right-click menu. Choose the master or any other group, as deep as you like (verse vox into John vox into All vox).
  - **Loops are refused:** a group can't go to itself, or to a group that leads back to it. Those choices aren't offered, and a song saved with a loop opens with it broken (that group back to the master).
  - **Mixing order:** groups are mixed in the right order whatever order they sit in: a group is mixed only once everything feeding it is in, on one core or several.
  - **Removing a group** sends what went into it back to the master, as before.
  - FX channels still go to the master only.
- **A composer lane dragged out as a MIDI file:** press on a lane's notes and drag (a click still plays and stops it). Drop it on a track in the arrangement, on the desktop, into another program, or onto a plug-in's window. The file is the lane's notes from bar 1, with the song's tempo there. The drums are on channel 10 (General MIDI's drum channel); the other lanes on their track's channel. The file is named "Composer bass - Walking.mid" and so on.

## 0.58.0

- **THE COMPOSER, a new page with all the layers stacked** (you asked: one page, a lock and a re-roll on each layer, one GENERATE button). Open it from IDEAS (COMPOSER, top right), from the chord pads (COMPOSER, second row), or give the "Composer" command a key in KEYS.
  - **Six lanes over the same bars:** CHORDS, MELODY, BASS, GUITAR, LEAD and DRUMS, each drawn as a little piano roll under the chord names.
  - **Each lane has ON** (heard and used), **LOCK** (kept through GENERATE), **ROLL** (another style and other choices for that lane alone), its **style** (a menu of its six: the same ideas as IDEAS' tabs) and its **TRACK**.
  - **GENERATE** rolls every unlocked lane at once. ROLL on CHORDS picks another progression from the library too.
  - **The chords** come from our progression library, chosen by **KEY, GENRE and MOOD**, repeated to **LENGTH** (4, 8 or 16 bars); pick one by name with PROGRESSION. Or use **CHORD TRACK** to write the layers under the chord track's chords.
  - **STYLE** (rock, thrash, nu-metal, metalcore, shred) drives the guitar, lead and drums. The bass's seventh style plays the roots on every kick of the drums lane, so the two lock together. IDEAS' settings (riff, drum, melody, humanize, velocities, tuning, drummer) all apply.
  - **A locked lane** keeps its style and choices. With the chords locked as well, its notes stay exactly the same; when the chords change, it plays the same idea over the new chords (notes that no longer fit can't be kept).
  - **HEAR** plays every lane that's on, together and looped, **through your song's own tracks**, so the bass sounds on your bass instrument and the drums on your drums. Those tracks play alone, from the playhead's bar, and **the song itself isn't changed** while you listen. Rolling, locking or switching lanes while it plays carries on from where it is. STOP, Space, or any change to the song ends it, and the playhead goes back to where it was.
  - **Which track each lane uses:** the one you choose in its TRACK menu; else a track named for it (Bass, Drums, Guitar, Lead Guitar, Piano, Melody); else, while you listen, the selected track. The drums are only heard on a track of their own (on a piano they'd be noise), and the TRACK menu can make that track for you.
  - **USE ALL** writes the lanes that are on onto their tracks at the playhead's bar (one undo step), and the library's chords onto the chord track (+ CHORD TRACK). USE ALL's menu writes one lane alone.
  - **Our own page and rules:** the layers are the ideas IDEAS already makes. Not yet: dragging a lane out as a MIDI file, and sounds of its own (it plays your tracks' instruments).

## 0.57.6

- **The Audio Alignment panel works the way you asked:** two lanes, REFERENCE on top and TARGET under it, each with its own **+** and CLEAR at its left. Select an audio event in the arrangement and click **+** beside REFERENCE; select the events to line up and click **+** beside TARGET; then ALIGN AUDIO. An empty lane says what to do. Each lane's header names what it holds (with several targets, a box under TARGET picks the one shown). The SELECTION row across the top is gone: the lanes' keys do its job.

## 0.57.5

**The spike report names each plug-in's own time** (you asked, after your 0.57.3 log showed the Bass track, Kontakt 8 with PR7-B, as the slow one).
- Each slow track in a SPIKE line now lists every plug-in on it with its own time in that block and its usual time, slowest first. For example: `"Bass" 42.9 ms (usually 16.1 ms) [Kontakt 8 30.1 ms (usually 11.0 ms), Paragon PR7-B 12.4 ms (usually 4.9 ms)]` (made-up numbers to show the shape).
- So we'll see whether it's Kontakt or PR7-B that jumps. If PR7-B is a big part of it, that's our own plug-in, and I can speed it up in PR7-B.
- Timing each plug-in costs a few hundred nanoseconds per plug-in per block. Nothing else in the sound changes.

**Please** play the song until it clicks a few times (with your usual settings, so we compare like with like), close the studio, and send me the newest log from `%APPDATA%\Paragon PR7-S\Logs`.

## 0.57.4

**Question boxes now have the progress box's clean corners** (you saw squared-off ends at the corners of CREATE EMPTY and the other boxes).
- **What was wrong:** the box is drawn as a rounded rectangle, but Windows held it in a solid square window, so the square's corners showed outside the rounded edge. On top of that, the toolkit drew its own square drop shadow round the window.
- **The fix:** every question and naming box (CREATE EMPTY, Rename, Save as Template, tempo, markers, signatures, presets, drum names, MediaBay attributes, key-command macros, and the standard yes/no boxes) is now a see-through window with no square shadow, as the progress box has always been.
- **Not checked here:** this cloud machine doesn't draw see-through windows the way Windows does. The test confirms every box is now made see-through with the square shadow off, but please tell me how they look on your screen.

**About your clicks** (your log from 0.57.3, thank you): every click was a block that took 42–56 ms when it lasts 46 ms. There were no silent blocks from the studio and no late calls from your interface. In every spike:
- the Bass track (Kontakt 8 with PR7-B) took 40–55 ms, though it usually takes 13–20 ms;
- every other track was 2 to 3 times slower too, at the same moment.

When everything slows down together like that, the processor usually had less to give at that moment. Things to try: Windows' **High performance** power plan, and **Multiprocessing** off in Kontakt's Engine options. The first spikes also came within a second of opening a plug-in window. Next, the spike report will give each plug-in's own time, so we can see whether PR7-B or Kontakt is the heavy one.

## 0.57.3

**Finding your clicks** (you hear clicks, and PEAK and OVER jump, with the CPU meter at about 29% and a buffer of 2048). At 2048 samples each audio block lasts about 43 ms. When OVER lights, one block took longer than all 43 ms, while the average shows 29%. Something occasionally holds the audio up for tens of milliseconds, and the meter can't say what.

- **The spike report.** The session log now gets a line for every audio block that took 90% or more of its time. It also gets a line when a block played silence because the engine was busy taking a change in (that one never showed on the meter at all), and when the audio driver called later than it should. Each line gives:
  - how long the block took against its length;
  - the tracks' and the master's time, and how many helper cores were working;
  - the slowest tracks, with their plug-ins and how long each usually takes. For example: `SPIKE: a block took 17.1 ms of its 10.7 ms (160%); ... slowest: "Rhythm Left (D)" [Kontakt] 14.2 ms (usually 1.10 ms)`.
  - It tells apart a plug-in that suddenly takes far longer than usual, a block of silence from the studio itself, and a driver or Windows calling late (the computer, not the studio).
  - At most one line a second; the spikes in between are counted. Nothing is allocated or waited for on the audio thread to do this.
- **The POSITION and TIME screens are cheaper to redraw** (they redraw 30 times a second while playing). Each redraw blurred the screens' shadows again and asked the system's fonts for the width of the figures. Both are now made once and kept. **Measured: 5.8 ms to 3.0 ms per redraw.** I compared the transport before and after in all three finishes, at 100% and 150% display scaling: every pixel is the same.

**Please play the song until it clicks a few times, close the studio, and send me the newest log** from `%APPDATA%\Paragon PR7-S\Logs` (paste `%APPDATA%` into Explorer's address bar). The SPIKE lines say where the time went.

## 0.57.2

**Speed, round two: audio tracks and VariAudio.** All 424 tests pass, and the sound is unchanged.

- **Audio tracks keep their clips when only the mix changes.** Every change in the song, even a fader nudge, made the studio work out every audio track's clips again: which takes are heard, the auto crossfades, where each clip lands in samples. MIDI tracks stopped doing this in 0.34.2. Now an audio track remembers what its clips were made from (its clips, which lane is soloed, the auto fades, the tempo, and the sounds they play) and keeps them while all of that is the same.
  - **Measured:** on 24 audio tracks of 120 comped clips each, a fader move went from **6.8 ms to 0.12 ms**, and a clip edit from 6.6 ms to 0.5 ms.
  - A VariAudio or warp edit gives the clip a new sound, so its clips are made again.
- **VariAudio edits are checked more cheaply.** On every change in the song, each edited recording's VariAudio edits were written out as text (every number of every edited note) to check whether its edited sound was still current. Now that text is written only when the edits change.
  - **Measured:** a vocal with 60 edited notes: **0.27 ms to 0.004 ms** per change. The old cost grew with every edited note.
- **The MediaBay's search** keeps each file's searchable text (name, folder, tags...) between keystrokes, instead of building it for every file on every key you type. It's made again when the files, their tags or their ratings change. Not timed here; it matters most on a big library.

**Looked at and left as it was:** editing a chord while 8 tracks follow the chord track takes about 7 ms on a big test song. I tried two changes: answers worked out once and reused, and quicker searches through the chords. They cut the work counted by a profiler by a quarter, but not the time on the clock. Most of what's left is sorting the rebuilt notes. It's one click, and 7 ms is under a single screen frame, so I took those changes back out rather than keep extra code that didn't measurably help.

**Next:**
- the screen: the transport's screens and the meters drawn more cheaply;
- recordings loaded in the background when a song opens;
- VariAudio and warping rendered in the background.

## 0.57.1

**Speed, round one** (you asked to make the studio run more efficiently). Three reviewers went through everything added since the last speed pass (0.34.5): the audio, the screen, and the waits. I checked each finding against the code before changing anything, and all 421 tests still pass. The sound is unchanged: the audio changes give exactly the same samples.

**Audio and plug-ins**
- **Automated plug-in knobs are now set only when they move.** Before, every change in the song (even a fader nudge) made the studio send every automated knob to its plug-in again, though nothing had moved. Plug-ins such as Kontakt take a parameter event as a reason to do work, so this was extra load on every change.
- **Comped audio tracks are much cheaper to update.** Working out the auto crossfades looked at every other clip again for every clip. The cost grew with the cube of the number of clips, and it ran on every change, fader moves included. On a track of 100 comped or sliced clips that was tens of thousands of steps per change; now it is worked out once per track.
- **The channel EQ** works on left and right together: the same samples, quicker.

**The screen** (your window spans three monitors, so every full redraw is big)
- **While recording,** the screen used to redraw everything 30 times a second whether or not anything arrived. Now it redraws when notes come in, and otherwise five times a second (so a growing audio recording still shows).
- **The MIDI editor, the Inspector and the chord pads** no longer redraw at all on a fader, pan or send move. The arrangement and mixer already worked this way.
- **The arrangement's playhead** redraws only a thin strip each frame, but the grid, ruler ticks, bar numbers and markers were still worked out across the whole width every frame. Now only the strip is.
- **The info line** no longer re-sorts the selected parts ten times a second (with Ctrl+A on a big song, that was millions of checks a second while idle).

**Waits**
- **IDEAS** makes only the tab you're looking at (and what BUILD SECTION uses). It made every tab on each change, and read your library's groove files each time.
- **LEARN A DRUMMER** reads your folder in the background, counting the songs as it goes, so the window doesn't freeze on a big folder.
- **MediaBay ratings and tags** are saved once you stop clicking. Every star or tag rewrote the whole database file, which is megabytes for a big sample library.

**Measured here** (a test song of 32 tracks with 2,000 notes each, EQ on every channel, Follow Chord Track and automation):
- an engine update after a fader move: 0.2 ms;
- one audio block: 0.44 ms of 10.7 ms;
- IDEAS after a change: 0.3 ms (making every tab, as before: 1.3 ms);
- reading 40 drum songs: 38 ms.

These are on this cloud machine with stand-in instruments, not your plug-ins.

**Next (0.57.2):**
- audio tracks keeping their clips when only the mix changed;
- VariAudio and warping rendered in the background;
- recordings loaded in the background when a song opens;
- the MediaBay search;
- the transport's screens and the meters drawn more cheaply.

## 0.57.0

**Build a whole section** (you chose BUILD SECTION in IDEAS, and asked that it suggest what comes next as the chord suggestions do)
- **BUILD SECTION** (in IDEAS' header; also **Build a Section...** in the chord track's menu and the pads' PLAYER menu) writes a whole section onto tracks in one click, over the chords IDEAS is showing:
  - **DRUMS:** the beat for that section: your learned drummer's, or the style's rules.
  - **BASS:** locked to that beat's kick, on the chords' roots.
  - **GUITAR:** the riff you last played or used on the guitar tab, in your STYLE, tuning and RIFF SETTINGS.
  - **LEAD, PIANO:** the cards you last played or used on their tabs. **MELODY** too if you light it (off at first).
- **Each part goes onto its own track** (Drums, Bass, Guitar, Lead Guitar, Piano, Melody). The same tracks are used section after section, so the whole song lands on one band; a new Drums track gets the GM drum map.
- **Building the same chords again replaces what's there.** All the parts are one undo step.
- **NEXT: WHAT COULD FOLLOW.** Once a section is built, the panel suggests what comes next, scored, each with how it relates to what you've built:
  - after a verse: a **chorus** from the verse's riff opened up (fewer palm mutes, looser, a fill into it), or a chorus with a new riff for contrast;
  - after a chorus: back to the **verse** (its riff and settings exactly);
  - two choruses in: a **breakdown** (the same riff tightened into chugs, half time; metal styles) or a **bridge** (a new, looser riff);
  - after a bridge or breakdown: the **chorus** again, then an **ending**.
  - The drums keep the same family of grooves throughout.
- **One click on a suggestion builds it right after the last section.** It uses the chord track's chords there if you have some; otherwise it repeats these chords and writes them onto the chord track, so the song keeps its chords.
- What's been built is remembered while the song is open, and forgotten when another song is opened.

**What I can't check here:** how it sounds through your instruments. The order of sections and the riff changes are our own rules, not taken from a manual; tell me where they don't fit your songs.

## 0.56.0

**Learn a drummer** (you asked: beats trained on real drummers, from a folder of whole songs of drum MIDI with no section names)
- On IDEAS' DRUMS tab, **DRUMMER** (over the generated column) > **Learn a Drummer from My Songs...** opens **LEARN A DRUMMER**. **CHOOSE FOLDER...** reads every MIDI file in the folder and the folders under it.
- **Each song's sections are found from the drums alone:**
  - It looks for the places where the playing changes: where the kick and snare land, the cymbal (hats, ride, china...), how busy it is, half time.
  - It also looks at what drummers do at a change: a fill in the bar before, a crash on the first beat.
  - A fill and a crash every four bars inside a verse don't split it.
  - Sections that play alike are grouped, then named:
    - half time is a **breakdown** (a quiet one, a **bridge**);
    - a short first section is the **intro**, a short last one the **ending**;
    - of the groups played more than once, the bigger one is the **chorus**, the other the **verse**;
    - something played once later on is a **bridge**.
  - Any kit's notes are read by what they are (Toontrack's hat notes as hats, for example). If a file has other instruments, only its drum channel (10) is read.
- **It's a guess from the drums, so you check it.** Each song is a row of coloured blocks:
  - **Click a block:** change what it is, **Leave Out of Learning**, **Split Here** (at the bar you clicked), or **Join With the Next**.
  - **Drag a block's left edge** to move where it starts.
  - **Double-click** a block to hear it (through the selected track, so put a drum instrument there).
  - **The tick** on each row: learn from that song or not.
- **SAVE PROFILE** (after naming the drummer) learns, for each section:
  - the grooves they played most, exactly as played (notes, places, velocities);
  - their fills;
  - how likely a kick, snare or ghost note is on each sixteenth;
  - their cymbal, how often a phrase starts on a crash, and half time;
  - their feel: how far ahead of or behind the beat the kick, snare and cymbal sit.
- **DRUMMER > the name:** the six generated beats are now played as that drummer.
  - Each section uses one of their grooves, kept and repeated as a drummer does. The third bar of each phrase is another of their grooves, or a kick moved where they would put one.
  - Their fills, their crashes, their feel.
  - A section they never played takes the nearest one they did (the card says so).
  - **DRUM SETTINGS** still shape it; at 50% it's as learned.
  - **The Style's Rules** goes back to 0.55.0's generator.
- **Check ...'s Songs and Sections...** opens a saved drummer again: the songs are read again, with the sections as you left them, so you can fix and save again. **Delete** removes the profile; your songs stay as they are.
- Profiles are saved in Documents/Paragon PR7-S/Drummer Profiles (a `.pr7drummer` file each).
- **Next (0.57.0):** BUILD SECTION: a whole section onto tracks.

**What I can't check here:** your real drum songs. The section finder is tested on songs written for the tests: verses, choruses, breakdowns, bridges, fills, crashes. Real drummers vary much more, and some sections will need putting right on the review page. Tell me which kinds it gets wrong and I can tune it. I also can't hear the learned beats through your drum library.

## 0.55.0

**The beat generator** (you asked: beats made new, not only the written ones)
- The DRUMS tab's left column is now **GENERATED: ONE PER SECTION**: an **Intro, Verse, Chorus, Bridge, Breakdown** and **Ending** beat for the STYLE you pick, built new from rules each time.
- **MORE IDEAS** deals six new beats every time.
- **How a beat is built:**
  - Each style has its habits: where a kick is likely on each sixteenth, the snare's backbone (two and four, half time, or thrash's skank), the cymbal and how fast it's played, where ghost notes go, how much double kick.
  - Each section changes them: the intro sparser, the chorus bigger (crashes, ride or china), the bridge on the ride bell in half time, the breakdown in half time with kicks in chug bursts and the china, the ending with a fill and a last big hit.
  - A section's groove is drawn once and played again, as a drummer keeps a groove; the third bar of each phrase is slightly varied, and a crash marks where phrases begin.
- **DRUM SETTINGS** (the key beside STYLE):
  - **DENSITY:** fewer or more kicks.
  - **DOUBLE KICK:** how much the feet fill in; near 100% in a metal style, constant sixteenths.
  - **GHOST NOTES:** soft snare notes between the backbeats.
  - **FILLS:** off, or every 2, 4 or 8 bars, one or two beats long.
  - **CYMBAL:** each section's own, or hats, ride or china throughout.
  - **LOCK KICK TO RIFF:** the kick on every hit of the guitar riff you last played or used.
  - At 50% everything is the style's own.
- The written beats from 0.54.0 are still there: **WRITTEN BEATS** switches the column to them.
- **Next (0.56.0):** learning drummer profiles from your folder of song MIDI, finding each song's sections (intro, verse, chorus, bridge, breakdown, ending) for you to check, then driving this generator with what it learned.

**What I can't check here:** how the beats feel played through your drum library. The rules and the notes they make are tested, not heard.

## 0.54.0

**A DRUMS tab in IDEAS** (you asked: beats generated from scratch, and the grooves in your library)
- **GENERATED:** six beats for the STYLE you pick (the same STYLE box as the guitar tab; it now shows on the guitar, drums and bass tabs):
  - **Rock:** straight rock, four on the floor, half-time, ride groove, ghost-note groove.
  - **Thrash:** skank beat, double-kick gallop, sixteenth double kick, blast beat, half-time breakdown.
  - **Nu-metal:** bounce groove, syncopated stomp, hip-hop swing, stop-start, tom groove.
  - **Metalcore:** breakdown, double-time verse, gallop double kick, half-time chorus, blast into breakdown.
  - **Shred:** power metal double kick, galloping double kick, driving rock, neoclassical march, half-time ballad.
  - And in every style, **Locked to the riff**: the kick on every hit of the guitar riff you last played or used on the guitar tab (the kick and the chugs as one), the snare on the backbeat (on three in nu-metal and metalcore).
  - Each beat has a crash at the start of every four-bar phrase and a fill on its last beat, in the style: snare to toms, snare rolls, kick-and-snare, tom triplets, toms down.
  - Written for General MIDI drums (kick 36, snare 38, hats 42/46, ride 51, china 52, crash 49/57, toms), which most drum libraries follow.
- **FROM MY LIBRARY:** the MIDI grooves in your MediaBay folders (files with a drum word in their name or folder: drum, groove, beat, GGD, Toontrack, EZdrummer, Superior...), best first: the style's words in them, a tempo near the song's, fills last. Six at a time; **MORE GROOVES** shows the next six. Each groove is laid over your chords from the first bar and repeated.
- **USE IT** puts a beat on a new "Drums" track with the GM drum map (the MIDI editor shows the drums' names), or onto your selected track.

**Two more bass ideas** on the BASS tab
- **Locked to the kick:** the chord's root on every kick of the drum beat you last played or used.
- **Following the riff:** the chord's root on every hit of the guitar riff.

**What I can't check here:** how the beats feel through your drum library. If yours isn't laid out like General MIDI, tell me which it is and I'll add its map.

## 0.53.1

**Drag a part out of the arrangement as a file** (you asked, for PR7-D's GROOVES page)
- **How:** drag a part (one at a time) out of the arrangement onto another window: a plug-in's window (such as PR7-D's), File Explorer, the desktop or another program. It arrives there as a file, and the part stays where it was in your song.
  - Over a plug-in's window, or anywhere that isn't the studio's main window, the drag becomes a file drag by itself.
  - If Explorer (or another program) sits on top of the studio's window, hold **Alt** while dragging (a copy): the moment the part leaves the arrangement it becomes a file. (The studio can't tell another program's window from its own underneath it.)
  - Dropped back onto the studio, nothing happens.
- **A MIDI part** becomes `<track> - <part>.mid`:
  - the part's start is the file's beat 0;
  - the song's tempo there and any tempo changes inside the part, and the time signature;
  - the notes as you hear them (the part's Transpose and Velocity applied), on the track's MIDI channel, 480 ticks a beat.
- **An audio event** becomes `<track> - <part> <tempo> BPM.wav` (for example `Rhythm Gtr - Verse riff 140 BPM.wav`):
  - what the event plays: from its offset, for its length, through its warp markers, with its own volume and fades (not the track's fader or effects);
  - 32-bit float at the recording's rate, at most five minutes (what PR7-D reads);
  - the tempo also in the file's ACID tag, which loop software reads.
- The files go in a "Paragon PR7-S Drag Out" folder in Windows' temporary folder, emptied each time the studio starts.

**Releases are Windows and Linux only for now** (you asked): the Mac build is switched off, which also makes each release quicker. It can be switched back on whenever you want.

**To check on Windows:**
1. With PR7-D's window open, drag any .wav or .mid from File Explorer onto its GROOVES page. If PR7-D takes it, drops reach plug-in windows in the studio.
2. Then drag a part from the arrangement onto the same page.

## 0.53.0

**Guitar ideas in five styles** (you asked for better guitar ideas)
- IDEAS' GUITAR tab has a **STYLE** box: **ROCK** (the twelve ideas as before), **THRASH**, **NU-METAL**, **METALCORE** and **SHRED**. Each style has six rhythm parts and six leads, all on your chord track's chords, in your TUNING and in your guitar library's VELOCITIES (palm mutes, sustains, pinch harmonics).
- **Thrash:** downpicked gallop, tremolo pedal, chromatic riff, skank beat, open-string stabs, stop-time break. Leads: chromatic tremolo, harmonic minor sixes, diminished climb, screaming tremolo, pedal-point lick, twin-guitar thirds.
- **Nu-metal:** bounce riff, stop-start, tritone chug, groove and octave, half-step creep, dissonant ring. Leads: octave melody, siren, pinch pings, dissonant high line, tremolo swell, single-note hook.
- **Metalcore:** breakdown, chug and stop, melodic octave riff, squeal breakdown, triplet burst, open chorus chords. Leads: twin harmony lead, octave chorus melody, tapped arpeggio, melodic sixteenth run, pinch and hold, call and answer.
- **Shred:** power metal gallop, neoclassical pedal, driving eighths, double-time sixteenths, held chords, arpeggiated power chords under sweep arpeggios, legato runs, harmonic minor fours, diminished arpeggios, string skipping and pedal-tone tapping.
- The heavier styles chug on the key's low note on your lowest string (the pedal), with the chord's power chords above it, the way these riffs are played.

**RIFF SETTINGS** (the key beside STYLE; for every style's rhythm parts)
- **LENGTH:** a riff of 1, 2 or 4 bars before it repeats. Two bars play A then B; four bars play A B A and a turnaround.
- **PALM MUTES:** more palm mutes against open notes, or fewer (mutes let ring).
- **TIGHTNESS:** tighter mutes (lower velocity, shorter) or looser ones.
- **BUSYNESS:** from sparse (weak hits left out, more space) to busy (palm-muted chugs added in the gaps).
- **FILLS:** a fill on the last beat of each phrase, as the style plays it: a chromatic run into the next chord, a burst of triplet chugs, a pinch-harmonic squeal or a stop.
- At 50% everything is as written, so ROCK's ideas are exactly as before until you move a control.
- MORE IDEAS still deals other versions: in the new styles the leads turn the other way and some riffs change their first bar.

**What I can't check here:** how they sound through Shreddage. The riffs, rhythms and velocity layers are tested, but not heard.

## 0.52.3

**The studio's own icon** (you asked: for the taskbar and your Stream Deck)
- **The design:** a "7S" monogram on a pearl onyx tile edged with the family's red glow.
  - The 7 is in the family's pink-to-orange and the S in pearl white, both in the PR7 mark's slanted block letters with its two speed cuts.
  - At larger sizes the full PR7-S mark sits underneath, with the red-to-orange stripe between.
- **Where it shows:** the taskbar, the window's title bar, Alt+Tab and the .exe file itself. The Mac and Linux apps use it too.
  - Small sizes (16 and 32 pixels) show just the monogram, which stays readable there; 48 pixels and up add the full mark.
- **Stream Deck:** every download now has a **"Stream Deck icons"** folder beside the app:
  - `stream-deck-288.png`: the icon with the name, at the size the Stream Deck uses for sharp keys.
  - `stream-deck-144.png`: the same, smaller.
  - `stream-deck-7s-288.png`: the monogram alone.
  - In the Stream Deck app, click a key's icon and choose **Set from File** to use one.
- Windows remembers icons for programs it has seen. If the taskbar still shows the old one after updating, unpin and pin the studio again, or restart Explorer.

## 0.52.2

**Plug-in windows remember where you put them** (you asked)
- Move a plug-in's window and it opens there again next time, even after closing the studio. The place is remembered per plug-in: wherever you leave Kontakt, Kontakt opens there, on any track.
- A plug-in you haven't placed yet opens centred on the right-hand screen (0.52.1).
- If the remembered place is no longer on any screen (for example, a monitor unplugged), it opens on the right-hand screen instead.

**The PLUGINS window in the Hub's look** (your screenshot)
- The Hub's pearl header with the PR7-S mark, with how many plug-ins were found.
- The list sits in the Hub's red-edged panel:
  - Names in large Bebas.
  - Kind, maker and version dimmed.
  - The chosen row lit as in the Hub.
  - The column titles on a dark strip, the sorted one in orange.
- Everything works as before: SCAN / OPTIONS, the scan, Delete, the right-click menu, sorting by clicking a column.
- Every other table now has the same column titles: the MediaBay's results, the Markers window and the Drum Map Setup.

**The studio staying in Task Manager after closing**
- **What was wrong:** after the window had gone and everything was saved, Windows still had to unload each plug-in's file as the program ended. A plug-in that doesn't let go at that point keeps the process alive with no window. This happens often with big instruments.
- **Now:**
  - Once the song and settings are saved, every plug-in is unloaded and the log is closed, the studio ends its own process at once instead of waiting on Windows.
  - If closing itself gets stuck earlier (a plug-in that won't unload), 0.52.1's log writes what was being closed at 30 seconds. At 90 seconds the studio ends itself and the log says so. Next time you start, it offers to show you that log.
- I can't reproduce this here, since it needs your plug-ins on Windows. If it still happens, the log will now say which plug-in it was.

## 0.52.1

Two fixes from your reports, and two changes you asked for.

**The export window, bigger** (you asked: about twice the size)
- **Audio Mixdown and MIDI File** open with everything twice the size: text, boxes, keys.
- **Limited by your screen:** at its tallest (with the channels listed) the page is about 600 points high, and twice that won't fit a 1080-high screen. So it's as big as the screen it opens on allows: about 1.65 times on yours, twice on a taller screen.
- **It grows with its list** as before, still at the bigger size.

**Plug-in windows open centred on the right-hand screen** (you asked: your third monitor)
- Every instrument's and effect's window now opens in the middle of the screen furthest to the right. With one screen, that's the one.
- They open there every time (a window you move isn't remembered yet: say if you'd like that).

**Render in Place: Disable and Hide were greyed out while CYCLE was on**
- **What was wrong:** since 0.31.0, a whole track rendered with CYCLE on is rendered over the cycle only (as you asked then). But the studio then treated it as "a stretch of a track", which can only mute the parts it rendered. So "Mute and disable the track" and "Hide the track afterwards" were greyed out, and Render with the last settings (Alt+R) only ever muted.
- **Now:** a track you render (the track menu, the Inspector's RENDER, Alt+R on selected tracks) stays a whole-track render. It's still rendered over the cycle only, but the original is muted, disabled and hidden by default, as you wanted.
- **One thing to know:** if the track has parts outside the cycle, disabling it silences those too. The page now says so in a line above its note (for example "CYCLE is on: only bars 5 to 8 will be rendered. Bass has parts outside the cycle..."). Turn CYCLE off to render the whole track.
- A **range** selected with the Range tool still renders just that stretch and mutes the parts rendered, as before: there the stretch is what you chose.

**The session log: what the studio was doing when it hangs**
- **What was wrong:** when the studio froze, the log ended with something that had finished ("rendered", "disabled a track"), never what came next. There were three reasons:
  1. The freeze report itself could get stuck. Before writing a word, it asked Windows for the names and debugging information of every loaded module. A plug-in that hangs while it's loaded or unloaded (exactly what disabling a track or setting up a render does) holds a Windows lock those requests need. So the report waited forever and nothing was written.
  2. Steps were only written once they'd finished.
  3. A hang while closing wasn't watched at all, and neither was a render that got stuck on its own thread (the screen still answers then).
- **Now:**
  - **Every step that can hang is written as it starts** ("Starting: Unloading the instrument Kontakt 8") **and when it ends** ("done: ... (1250 ms)"). That covers loading, preparing, switching to offline and unloading each plug-in, a plug-in's window opening and closing, asking a plug-in for its settings, disabling or enabling a track, setting up and ending a render, the audio device, and closing.
  - **A FROZEN entry is written at once**, with the steps under way and for how long, before anything else is tried.
  - **Then what every thread is doing (Windows):** the studio's, the audio's, and every plug-in's and driver's own threads. For each, which module it's in (for example "Kontakt 8.vst3 + 0x1a2b3c") and the modules found on its stack. This is read from a list of modules the studio keeps up to date in advance, without asking Windows anything a stuck plug-in could be holding. Last of all come the calls by name. If that last part gets stuck too, everything above it is already saved.
  - **A render that stops moving for 30 seconds** writes a STUCK entry with the same report, even though the screen still answers.
  - **The watch stays on while closing:** a close that hangs for 30 seconds writes FROZEN "(while closing)" with the plug-in being unloaded.
- **What helps:** when it hangs, wait about 30 seconds before ending it in Task Manager, so the report has time to be written. Then send the log: it should name the plug-in or the step.
- I could only run the Windows parts through the build machines' tests (a deliberate freeze in the test program, which checks that the report lists every thread). I can't reproduce your hang here, so the next log is the real test.

## 0.52.0

**Auto fades and crossfades** (the last of your four; the manual's Auto Fades dialog). Until now, where two comped takes met, playback jumped straight from one take to the other. If the sound wasn't at the same point in both waves, that left a small click at every cut. Now the studio crossfades there as it plays.

**Crossfades where takes meet (on by default)**
- **At every comp cut,** and wherever one clip in front hides another, the take going out fades down while the take coming in fades up. The default is 10 ms.
- **Centred on the cut:** half before it, half after, using each recording's audio just past its own edge. If a take's recording starts exactly at the cut, so there's nothing of it before, the whole crossfade moves after the cut. Both takes agree on it, so the two fades always match.
- **Not between the two halves of one split recording:** that audio simply carries on, so nothing changes there.
- **Nothing is drawn on the events,** as the manual says: auto fades are made as the audio plays. Your own fade handles stay as they were.

**The Auto Fades dialog** (right-click a track > **Auto Fades Settings...** for an audio track, or **Song's Auto Fades Settings...**; the key command Project > Auto Fades Settings):
- **AUTO FADE IN / AUTO FADE OUT:** a short fade at every event's start and end. Off at first. An event with its own fade there keeps its own.
- **AUTO CROSSFADES:** the crossfades above. On at first.
- **LENGTH:** 1 to 500 ms.
- **EQUAL POWER or EQUAL GAIN:**
  - Equal Power keeps the loudness steady when the two takes are different performances. This is the usual choice and the default.
  - Equal Gain keeps the level steady when they're the same sound. With identical audio, Equal Power swells by up to 3 dB in the middle of the crossfade, which is how it's meant to work.
- **USE PROJECT SETTINGS** (a track's dialog only): untick it to give that track its own settings.
- **Saved with the song.** Changing them isn't an undo step (like the record modes).

**Bounce** (0.30.0) writes the crossfades into its new file, so the bounced take sounds exactly as the comp played.

**Honest limits:**
- I checked the crossfades by measuring the output: no jump bigger than a few thousandths at a cut, and steady levels either side. I can't listen to them here.
- **Not yet:**
  - The manual's curve type and shape buttons (spline / damped spline); our curves are a sine for Equal Power and a straight line for Equal Gain.
  - "As Default".
  - Event-based crossfades you draw and edit (the manual's Crossfade editor and the X key).
  - A cut between two warped clips still cuts directly, because a warped clip's stretched sound has nothing past its edges. With only one of them warped, the crossfade moves onto the other's side.

## 0.51.0

**Export, round two** (the third of your four): stems, cycle-marker ranges and MIDI files. EXPORT now opens a small menu: **Audio Mixdown...** or **MIDI File...**

**Audio Mixdown: CHANNELS**
- **"The main mix"** works as before: everything through the master.
- **"Channels: a file each (stems)":** tick any tracks, group channels (folder groups too) and FX channels. ALL and NONE tick or clear them all. You choose a folder (it starts in a "Mixdown" folder beside the song).
- **Each file is that channel's own sound,** as the manual describes: its effects, EQ, fader and pan.
  - A track's file doesn't go through its group.
  - A group's file holds what's routed into it.
  - An FX channel's file holds what every track sends to it, the return only.
  - Everything plays just as in the song while it's written, but only that channel reaches the file.
- **The master's effects, EQ and fader are left out,** unless you tick "Include the master's effects, EQ and fader".
- **Files are named** "Song - Channel.wav". If the name is taken, a number is added (Song - Bass 2.wav), so nothing is overwritten.

**Audio Mixdown: WHAT > Cycle markers (a file each)**
- **Tick cycle markers** (from 0.49.0's marker track) and each one's range is written to its own file, named after the marker ("Song - Chorus.wav").
- **Works with channels too:** every channel over every chosen marker.

**MIDI File...** (the manual's Export Options):
- **What's in the file:** every instrument track's parts as a standard MIDI file, each track named and on its MIDI channel, with the tempo (the tempo track's changes; a ramp written as steps every sixteenth, since a MIDI file has no ramps), the time signatures and the markers.
- **Notes as you hear them:** a part's Transpose and Velocity (0.47.0) are applied, and muted parts are left out.
- **Options:**
  - Export Markers.
  - Export as Type 0 (all on one track).
  - Resolution: 48 to 960 ticks per quarter note; 480 at first.
  - Export Locator Range: only between the locators, starting at 0.
  - The tracks' Volume/Pan as CC 7 and CC 10.
  - Your choices are kept for next time.
- **Checked by reading it back:** the tests export a song and import the file again (notes, tempo changes, signatures, markers).

**Also:** one older test checked a note recorded from the on-screen keyboard too tightly. On a slow machine JUCE can place that note up to one audio block later, which is what failed 0.50.0's first build. Its tolerance is now one block. Nothing about recording changed.

**Not yet:**
- **No export queue.** You make one export at a time.
- **No naming-scheme dialog.**
- **Audio files are WAV only** (no MP3 / FLAC / Ogg export).
- **MIDI export can't include your automation lanes as controllers**, and you can't drag a part out to the desktop as a file.

## 0.50.0

**Folder tracks** (the second of your four). It follows the manual's Folder Tracks.

**Making folders:**
- **+ ADD TRACK > Folder track** (or with its group channel, below it).
- **Select tracks, then the track menu's Track Folding > Move Selected Tracks to New Folder** (or "... with Group Channel"). The folder appears where the first of them was, and they go inside it.
- **Drag tracks** in the track list: drop one just under an open folder's row and it goes in; drop it above or outside the folder and it comes out. **Track Folding > Move into Folder / Move out of Its Folder** do the same from the menu.
- **Folders can hold folders.** Tracks inside are indented, with a faint line for each folder around them.

**Using them:**
- **The arrow beside a folder's name** folds its tracks away or shows them again. Folded tracks still play; they're also hidden in the mixer, as the manual keeps the two in step.
- **The folder's M and S** mute or solo every track in it.
- **The folder's row shows its tracks' parts** as blocks, a strip for each track. **Click a block** to select every part in it (Shift adds); **drag it** to move them all together (Alt copies). Right-click > Track Folding > Show Data on Folder Tracks: always, never, or hidden while the folder is open.
- **Enable Group Channel** (Track Folding menu): the folder becomes a group channel, so its tracks are summed into one fader, with inserts, sends and EQ, as any group. Every track in it is routed to it, and any track moved in later too. A track moved out goes back to the master. Switching the group channel off sends them all back to the master.
- **Removing a folder keeps its tracks.** They move to the folder it was in, or to the top level. This is our choice; the manual removes them too.

**DAWproject import:** folders from your other DAW's projects now come across as folder tracks. A group holding tracks becomes a folder with its group channel. Before, folders were flattened away.

**Undo** brings back what was in which folder. Folders are saved with the song.

**Not yet:**
- **Group Editing Mode** (editing a part edits the same place on every track in the folder).
- **Folder track colours applied to their tracks automatically.**
- **Double-clicking a folder block to open the editors.**
- **A folder's group channel inside another folder's** still goes straight to the master, not into the outer group.

## 0.49.0

**Markers** (the first of your four: then folder tracks, export round two, crossfades for comping). It follows the manual's Markers chapter.

**The marker track** (+ ADD TRACK > Marker track, or just press Insert):
- **Position markers** keep a place: an orange flag with their number and name.
- **Cycle markers** keep a range, such as a verse or a chorus: a pink bar, with its number in brackets.
- **The row's keys:**
  - **+ POS** adds a marker at the playhead.
  - **+ CYC** adds a cycle marker over the locators (Shift-click names it too).
  - **LOCK** stops changes; you can still go to the markers.
  - **GO TO** goes to a marker, sets the locators to a cycle marker, or zooms to one.
- **Marker lines** run faintly down the whole arrangement, so you can see where sections start. Switch them off in the right-click menu.

**On the marker track:**
- **Alt-click** adds a position marker (Shift+Alt: and type its name). **Ctrl-drag** draws a cycle marker (Shift+Ctrl: and name it).
- **Drag** a marker to move it (it snaps). Drag a cycle marker's edges to change its start or end.
- **Double-click a cycle marker:** the locators go to it, ready to play round. **Alt + double-click:** it zooms to it too.
- **Double-click a position marker:** type its name.
- **With the Range tool,** double-click between two markers to select that whole stretch on every track.
- **DRAW** adds and **ERASE** removes (Alt + ERASE: every marker from there on). **Delete** removes the selected.

**Keys (the manual's own):**
- **Insert:** a marker at the playhead.
- **Shift+N / Shift+B:** the next / previous marker.
- **Shift+1 to Shift+9** (and the number pad's 3 to 9): to marker 1 to 9.
- **Ctrl+1 to Ctrl+9:** set marker 1 to 9 at the playhead.
- **Ctrl+M:** the Markers window.
- All of them can be changed in KEYS (they're in its Transport and Window lists), so a Stream Deck can press them.

**The Markers window (Ctrl+M):**
- **The list:** every marker with its ID, type, position, end, length and description. An arrow marks the one at the playhead; click the arrow column to go to a marker.
- **Editing:** double-click any cell to type. Click a column's title to sort by it.
- **Keys in the list:** Up/Down move through it and Return goes to the chosen marker.
- **FUNCTIONS:** Move Markers to Cursor; Reassign Position / Cycle Marker IDs (numbers them again in time order).
- **Settings:** SHOW IDS; CYCLE FOLLOWS (going to a marker also sets the locators to it); LINES; AUTO-SCROLL.

**MIDI files:** markers in a MIDI file come onto the marker track when you import it.

**Each marker change is one undo step.** Markers are saved with the song.

**Not yet:**
- **One marker track:** the manual allows ten.
- **No marker lines in the MIDI editor.**
- **No markers on the info line.**
- **No WAV-file markers.**
- **No marker export:** it comes with MIDI file export in 0.51.0.

## 0.48.0

**The neon finishes get PR7-G 0.59.0's look: black keys in neon rings** (you asked; your design).

**On Pearl Black and Pearl White + Neon:**
- **Every key is black**, the same as on Pearl White. No more pink-to-orange keys.
- **The gradient moves to the rings round them.** Each key's surround is now pink to red to orange, lit along the top and darker at the front, as the chrome was.
- **Lettering on the keys is light, as on Pearl White's black keys.** A key that's on lights its lettering (or its LED stripe) orange. Before, on the neon finishes a key that was on turned black among the neon ones. Now every key is black, so "on" shows the way it always has on Pearl White.

**Pearl White doesn't change at all.** I checked the whole window pixel by pixel, with the MIDI editor open and with the mixer open: identical to 0.47.0.

**What PR7-G has that the studio doesn't:**
- **No footswitches, slot keys or expression pedal** here, so there are no bezels or ribs to see.
- **The big knob isn't used in the studio.** The mixer's pan controls are small arc dials, which look the same in every finish.
- **Both are updated anyway in the code the family shares:** the big bezel becomes the gradient and the big knob turns black with a gradient ring. They match PR7-G if the studio ever shows them.

**SETTINGS:** the FINISH box has a tooltip describing each finish.

## 0.47.0

**The info line for parts and events** (next on your list). The strip over the track list and the arrangement showed only automation points. Now it also shows the selected part or audio event, and you can type into it.

**What it shows:**
- **An audio event:** FILE, NAME, START, END, LENGTH, OFFSET, FADE-IN, FADE-OUT, VOLUME, MUTE.
- **A MIDI part:** NAME, START, END, LENGTH, TRANSPOSE, VELOCITY, MUTE.
- **Several selected:** the earliest one's values, in pink, with how many are selected.
- **Automation points still come first:** when points are selected, it shows them as before. A click on a part shows the part.

**How you change things (the manual's rules):**
- **Double-click a value and type**, then press Enter. Every selected part changes by the same amount as the first one. The manual's example: two events 1 and 2 bars long, LENGTH typed as 3 bars, become 3 and 4 bars.
- **Ctrl+Enter** sets them all to the value you typed: both become 3 bars.
- **Clicking MUTE** mutes or unmutes them all. **NAME** names them all.
- **Each change is one undo step.**
- **How to type values:** START and END as positions (5.1.1.0). LENGTH in bars.beats.16ths.ticks (3.0.0.0 is three bars). OFFSET and the fades in seconds ("1.5", or "250 ms"). VOLUME in dB ("-6", or "-inf" for silent). TRANSPOSE and VELOCITY as numbers ("+5", "-20").
- **Limits:** a part can't start before the song does. An event can't run past the end of its recording. Fades stay inside the event. A frozen track's parts are left alone.

**Three new things a part can hold**, set from the info line:
- **VOLUME on an audio event:** its own level before the track's fader, -INF to +6 dB. The waveform grows or shrinks with it, and bounces include it.
- **TRANSPOSE on a MIDI part:** semitones, -48 to +48.
- **VELOCITY on a MIDI part:** added to every note's velocity (kept within 1-127).
- **TRANSPOSE and VELOCITY are for playback only, as in the manual:** the notes in the MIDI editor stay as you wrote them. Glue writes them into the notes (the glued part starts from 0). Detect Chords names the transposed chords, since that's what you hear.

**Not yet:**
- **No TRANSPOSE for audio events:** it needs pitch shifting, which only VariAudio does here so far.
- **No Lock or Fine-Tune.**
- **No info line for chord, tempo or signature events.**
- **Nothing in the info line's right-click menu to choose which fields show.**

## 0.46.1

Two more automation fixes you asked for:

**A point anywhere you click, between points too.**
- **Before:** a click between two points added nothing. The manual drops a point that doesn't leave the line, so 0.45.1 still followed that rule.
- **Now:** a point is added on the line where you click, between points or not, and it's selected so you can drag it up or down. (It still won't add a second point exactly where one is.)

**Delete moves on to the next point.**
- **What happens now:** delete a point (or several) and the next point after them is selected, so pressing Delete again takes that one, and so on along the lane. At the end of the lane, the one before is selected.
- **Undo:** each Delete is its own undo step.

## 0.46.0

**The channel EQ, as four bands of the other DAW's Frequency 2** (you asked; you chose the core with the spectrum, in place of the old one). Every channel's EQ (the Inspector's EQUALIZERS and the EQ window) works like this now:

**Every band has the same shapes:**
- **Low Shelf, Peak, High Shelf, Notch.** A Notch is a very narrow Peak.
- **Band 1 can also be a low Cut** and band 4 a high Cut, each 6, 12, 24, 48 or 96 dB per octave (pick it from the band's type menu).
- **Q** is how wide a Peak or Notch is. On a shelf, Q above 0.71 adds the dip and bump at its corner. On a Cut, it adds a resonance at the frequency (there's none for Cut 6).
- **INVERT** beside each band's gain turns a boost into the same cut (Invert Gain).
- **OUTPUT** sets the EQ's level after the four bands.

**The EQ window:**
- **SPECTRUM:** the channel's sound after the EQ, live, behind the curve.
- **KEYS:** a keyboard under the curve with each band's mark on it. Drag a mark and the band's frequency follows; let go and it sits exactly on that key (A3 = 440 Hz).
- **AUTO LISTEN:** while you drag a band's point, the channel plays only what that band works on (around a Peak or Notch, under a low shelf or cut, over a high one). It stops when you let go.
- **The mouse wheel** over a point sets its Q. Ctrl-drag still moves only the gain, Alt-drag only the frequency, Shift-drag the Q, as the manual has it.
- **The Pre section** (gain, phase, its own cuts) stays as it was.

**Your songs:** EQs saved before open sounding the same. The old Parametric is the new Peak, and the old cuts were 12 dB per octave, which they stay.

**Also fixed:** an EQ edit made right after Undo could bring back what was undone. The EQ window edited from what it showed a moment before, not the song as it is now.

**Not here (your choice for now):** dynamic EQ, side-chain, Mid/Side or left/right per band, linear phase, eight bands, single-band view.

## 0.45.1

Two automation fixes you reported:

**A click on an automation lane puts the point on the line as it is.**
- **Before:** a click with the Object Selection tool took the height you clicked at. Four clicks along a flat line gave four different values.
- **Now:** a new point takes the line's value where you click, so the line stays where it was. On an empty lane it takes the control's current value (the fader's, for volume).
- **To set a different value:** drag the point up or down, type it on the info line, or draw with the DRAW tool, which still follows the mouse's height.
- **Between two points,** a click still adds nothing, since a point on the line changes nothing (as the manual has it).

**Ctrl+Enter on the info line works.**
- **What it does:** type a value and press Ctrl+Enter, and every selected point is set to that value, as the manual's info line rules say.
- **What was wrong:** the typing box only answered a plain Enter. With Ctrl held the key went past it, and nothing was set. My test had called the setting directly rather than pressing the keys, so it missed this.
- **Also fixed:** the MIDI editor's info line had the same bug.

## 0.45.0

**Tempo and signature** (the third of your four choices). Three parts, from the manual's tempo and signature chapters:

**Time signature changes along the song.** Until now a song had one signature. Now it can change at any bar: a bar of 7/8, a 3/4 bridge, a 6/8 outro.
- **The signature track** (Add Track > Signature track). Each change shows at its bar.
  - **Adding one:** with the DRAW tool, click where it goes. Or double-click an empty bar. Type the signature (7/8) and the bar.
  - **Editing:** drag a change along the bars. Double-click it to retype it. ERASE or Delete removes it.
  - **The first signature:** at bar 1, it's the song's own. You can change it but not move or delete it.
  - **LOCK** stops changes. Right-click the track for its menu.
- **The Tempo Track Editor** (Ctrl+T) shows every change in its signature lane. DRAW and ERASE work there too.
- **Insert Time Signature Event** puts one at the bar the cursor is in. It's on the transport's signature menu, and in KEYS with no key yet, as the manual has it.
- **What follows the bars:** the ruler, the grid, the click (the accent on each bar's first beat), the position screen, typed positions, the MIDI editor, and the bar and beat your plug-ins are told.
- **The transport's SIGNATURE screen** shows the signature where the playhead is.
- **What stays put:** parts, notes and automation stay where they are in the song. A change moves the bar lines, not your music, as the manual describes.
- **MIDI files** with signature changes bring them in when imported as a new song.

**Tap tempo.** A **TAP** key beside the tempo. Tap the beat on it, or press **Shift+Space** (the manual's key).
- **The tempo:** the average of your last eight taps, shown as TAP TEMPO on the screen while you tap. A two-second gap starts again.
- **Right-click TAP for the Tap Tempo Mode:**
  - **Automatic:** what's selected, else where playback started, else the cursor; with the tempo track off, the song's tempo.
  - **The selection's start, the last playback start, the cursor, or the project tempo.**
  - **Display only:** shows the tempo without changing anything.
- **Undo:** a run of taps is one undo step, so one Undo puts the tempo back as it was.

**Tempo detection.** Right-click an audio event or a MIDI part (7 seconds or more) > **Tempo Detection...**, then **ANALYZE**. The tempo track then follows the beats you played without a click, drift and all.
- **How it finds the beats:** it listens for where sounds start (for MIDI, the notes), finds the overall tempo, then tracks each beat. It follows a drummer pushing or pulling, not sudden jumps.
- **The result:** a tempo event on every beat. The first beat lands on a whole beat of the song.
- **Everything keeps its place in time:** other parts, notes, automation, chords and the locators move to the beats they now sound at. That's the manual's linear time base during analysis.
- **Corrections, as the manual's panel:**
  - **X 2 and / 2:** twice or half as many beats.
  - **X 4/3 and X 3/4:** for triplet or dotted feels.
  - **OFFBEAT:** half a beat on, when the offbeat was taken for the beat.
  - **SMOOTH TEMPO:** steadies an uneven result.
  - **RESET:** goes back to the tempo before the analysis. Undo works too.
- **Honestly:** it finds clear beats well; my tests have clicks drifting from 100 to 112 BPM found within 15 ms. Sparse or very rubato playing can need the corrections or a hand edit. I can't test it here on your recordings.

**Our choices:**
- The signature stays as it is after a detection (the manual sets 1/4).
- Removing the signature track keeps its changes: they're the song's bars, and the Tempo Track Editor still shows them.

**Not yet:**
- click patterns per signature;
- Process Bars (insert, delete, reinterpret bars);
- tempo recording;
- the Time Warp tool after a detection;
- the panel's reanalysis direction.

## 0.44.0

**The chord tool, round two** (you chose it). Five parts, from the manual's chord pads and chord track chapters:

**Recording chords onto the chord track.** The chord track's row has a **REC** key (Record Enable). While you record:
- **Pads:** each pad you press becomes a chord event at that beat.
- **Your keyboard:** with no pads pressed, each chord you play becomes one. Three or more different notes starting together count; single notes and two-note intervals don't.
- **Placement:** on the grid when SNAP is on. What was on the chord track where you recorded is replaced.
- **What's needed:** as the manual says, an instrument track armed or monitoring, so your keys get through.

**Sections mode for the pads** (MODE > Sections, or REMOTE). A pad's key picks the chord; the **section keys** play its notes one at a time.
- **Section keys:** C0 to E0 at first. Section 1 plays the lowest note (the bass), section 2 the next up, and so on.
- **Subsection keys:** F0 to A0 at first. They play the same notes an octave up. That's our reading of the manual's "transposed by an offset".

**REMOTE opens the pads' remote settings,** as the manual's Chord Pads Remote Settings. Click a box, then press the key on your MIDI keyboard (MIDI Learn):
- **The pads' range:** press the first key, then the last.
- **Latch Chords.**
- **Sections mode and the ten section keys.**
- **The chord modifiers:** next and previous voicing, more and fewer tensions, transpose up and down. ACTIVATE turns them on. They change the last pad you played and play it, so you hear the change.
  - Their keys are C2 to F2 at first, our choice. They're off until you activate them, so they don't take any of your playing keys.

**Your own patterns.** Right-click a MIDI part > **Make a Pattern from This Part**.
- **How it reads the part:** each different note becomes a voice, counted from the lowest. The lowest is the bass when it sits a fifth or more under the rest.
- **Where it appears:** under **MY PATTERNS** in IDEAS' PATTERNS tab, the pads' PERFORM, and the Inspector's PERFORM. Your patterns are kept between sessions.

**A track performing the chord track.** In the Inspector's CHORDS section, **PERFORM** picks a pattern. The instrument track then plays it over the chord track's chords as the song plays, beside its own parts.
- **Live:** change a chord and the performance follows.
- **Undo:** each choice is an undo step, and it's saved with the song.

**Also:** the EQ window's Pre gain bar now reads "GAIN +0.0 dB" from the start (it read "0.0" in 0.43.0).

**Not yet:**
- choosing which MIDI input the pads listen to (they take all);
- velocity from the pattern or from your keyboard;
- several players on several tracks;
- the pads' modifiers for all pads at once (pitchbend or controller).

## 0.43.0

**The channel EQ** (you chose it: "EQ later", now). Every channel has a built-in EQ and Pre section, as the manual's channel does: tracks, FX and group channels, and the master. No plug-in is needed.
- **Pre section:** input gain (±24 dB), phase invert, a low cut (20 Hz to 20 kHz) and a high cut (50 Hz to 20 kHz). Each cut is 6, 12, 24, 36 or 48 dB per octave (12 at first).
- **EQ:** four bands, each on or off, with gain (±24 dB), frequency (20 Hz to 20 kHz) and Q.
  - Bands 1 and 4 can be parametric, a shelf or a cut. Bands 2 and 3 are always parametric.
  - BYPASS takes all four bands out; the Pre section stays in.
- **Where it plays:** Pre first, then the inserts before the fader, then the EQ, then the fader, as the manual's channel. Pre-fader sends hear the EQ'd sound.
- **The curve:** it shows exactly what plays, the EQ and the Pre's cuts and gain together. A bypassed EQ is drawn dashed in grey.
  - **Click** an empty spot: the nearest band that's off comes on there.
  - **Drag** a point: frequency and gain. **Ctrl:** gain only. **Alt:** frequency only. **Shift:** its Q (up is narrower).
  - **Double-click** a point: that band off.
  - **Right-click:** Save Preset, Load Preset, Invert EQ Settings, Reset, Bypass.
  - **In the EQ window,** the mouse's frequency, note and level show at the top.
- **Typing a frequency:** Hz, kHz, or a note with cents, as the manual: "A3" is 440 Hz, "A5 -23", "C4 +49".
- **Where to find it:**
  - The Inspector's new **EQUALIZERS** section: the curve, edited right there, plus BYPASS and EDIT EQ. Right-click its header for presets.
  - The **EQ window**, with every control: from EDIT EQ, from a mixer channel's right-click (Channel EQ...), or a key command (Window > Channel EQ) for your Stream Deck.
- **It's kept:** saved with the song, undoable (one step per drag), carried by track presets (as the manual's "Loading Inserts and EQ from Track Presets"), and saved as EQ presets of its own (.pr7eq in Documents/Paragon PR7-S/EQ Presets).
- **Our choices:** the bands' starting points (a low shelf at 100 Hz, 500 Hz, 2 kHz, a high shelf at 8 kHz, all off) and the filters (the standard "audio EQ cookbook" ones; Butterworth cuts).
- **Not yet:** automating the EQ's knobs, the spectrum behind the curve, and comparing two channels' curves.

## 0.42.0

**Chord detection** (you asked, as your chord tool has: chords read from MIDI and from audio). Right-click a part (MIDI or audio, one or several selected) > **Detect Chords**:
- **Onto the Chord Track:** the chords it plays, from the part's start. What was on the chord track over those parts is replaced, as one undo step.
- **Onto the Chord Pads:** each chord once, in order (locked pads are skipped).
- **A chord at most every Beat / Half Bar / Bar:** how often a chord may change.
- **Plain Chords Only (major, minor):** on at first. Turn it off to get sevenths, sus chords and the rest.

**How it reads:** in each beat, half bar or bar, it measures how strongly each of the twelve notes sounds and which note is lowest. Every chord is scored against that: its notes heard, notes outside it, its root in the bass.
- **No flicker from melody notes:** a last pass changes the chord only where the notes clearly say so. Before, a melody note passing through could switch the chord for a beat.
- **Inversions:** when the lowest note is another note of the chord, it's written as one (C/E).
- **Silence:** carries the chord before it on.

**From audio:** the sound is split into how strong each of the twelve notes is, beat by beat, following your tempo track.
- **Overtones:** each note's overtones are partly taken out, so a C doesn't also read as G and E, and a minor chord doesn't turn major.
- **The bass:** read from 40 to 260 Hz.
- **What it's good at:** clear chordal parts, like a guitar or a piano, read well (in the tests, a played C Am F G comes back exactly at 120 and at 90 BPM).
- **What it's not:** a full mix with vocals and drums is a best guess, and tuning far from A = 440 isn't handled. That's why the menu says "a best guess" on audio tracks.

**The MIDI editor's info line names the chord** of the selected notes (CHORD Am7, beside VELOCITY), in the key of the chord track there.

## 0.41.0

**Better melodies** (you asked, as your chord tool has: melodies you can shape). IDEAS has a new **MELODY** tab: six melodies composed for your chord track's chords, each in a different form. The way they're composed and the form names are our own.
- **Song form (AABA):** a phrase, the same again, something new, then the first one home.
- **Question and answer (ABAB):** a phrase that asks, one that answers, and both again.
- **Hook (AAAB):** one catchy phrase three times, then a turn.
- **Two answers (ABAC):** the same question, answered differently each time.
- **Climbing motif:** one motif, each time higher in the key.
- **Always new:** every phrase different.

**How they're built:** each phrase's rhythm and shape (a motif) is made once. When its letter comes back, the motif is bent to the chords under it.
- Beats 1 and 3 land on chord tones. The notes between walk through the key, avoiding any note a half step against the chord.
- A leap is followed by a step back the other way, as singers do.
- Phrases rise, then fall.
- The melody ends on the last chord's root.

**MELODY SETTINGS** (the key at the right of the tabs; the six melodies change as you move a control):
- **DENSITY:** from a note or two a bar to busy runs.
- **LOWEST / HIGHEST:** the range, as note names (C3 to G4 at first, middle C = C3).
- **LEAPS:** from steps only (smooth and singable) to mostly leaps. Thirds and fourths at first; fifths and sixths past 60%.
- **PHRASE:** 1, 2 or 4 bars, which is how long the motif is.
- **REPEAT:** how closely a returning phrase copies the first time. At 100% the rhythm returns exactly; lower changes a few notes.
- **RESTS:** room to breathe. Weak notes are left out and the ends of phrases are shortened.
- **SYNCOPATION:** notes pushed off the beat.
- **DEFAULTS:** back to the starting settings.

Your settings are kept. HEAR, USE IT (onto the selected track or a new "Melody" track), MORE IDEAS and HUMANIZE work as on the other tabs. The PIANO tab's six melody styles are still there as before.

## 0.40.0

**The progression library** (you asked, as your chord tool has: progressions by genre and mood). A new PROGRESSIONS window holds 157 chord progressions in 14 genres and 10 moods. The progressions, their names, and how they're sorted are our own.
- **Genres:** pop, rock, metal (18, mostly minor, Phrygian and harmonic minor, for your 7-strings), blues, jazz, R&B & soul, gospel & worship, EDM, hip-hop & trap, lo-fi, film & game, folk & country, Latin, classical.
- **Moods:** happy, sad, uplifting, epic, dark, tense, dreamy, chill, romantic, nostalgic. A progression can have two.
- **Where to open it:** the PROGRESSIONS key on the chord pads (beside IDEAS), the chord track's right-click menu ("Progression Library..."), or a key command (Window > Progression Library) you can put on your Stream Deck.

**In any key:** pick the key at the top (it starts in your song's key; FROM THE SONG goes back to it). Every progression is spelled in that key, with its numerals under the chords.
- **The other mode's progressions:** a major progression in A minor is played in C major, and a minor one in D major is played in B minor. Every chord stays in the key's notes. KEY'S MODE ONLY hides these.
- **Chords written with a flat or sharp keep it:** the "Tritone dread" in A minor is Am Bb Eb Am, not D#.
- **Chord lengths:** a chord can last two bars or half a bar, and its box is drawn as wide as it lasts.

**Each row:**
- **HEAR:** plays it at the song's tempo, with the chord that's sounding lit.
- **TO TRACK:** puts it on the chord track from the playhead's bar (one undo step).
- **TO PADS:** puts it on the chord pads (locked pads are skipped).
- **IDEAS:** puts it on the chord track, then opens IDEAS (piano, guitar, bass and patterns) for it.
- **The mouse:** click the chords to hear it, double-click to put it on the chord track, or drag it onto the chord track's lane to drop it at that bar. Right-click for all of these.
- **The star:** marks a favourite. FAVORITES shows only those.

**Finding one:** pick a genre and a mood at the left (each shows how many it holds), and search by name, genre, mood, a chord ("Bb") or a numeral ("bVII").

**The chord pads' second row of keys fits on one screen:** when the pads sit beside the Inspector on a single screen, the keys now shrink to fit. Before, IDEAS ran off the right edge (and the new PROGRESSIONS key would have too). On your three screens they keep their full size.

## 0.39.0

**Performances** (you asked, as the chord tool you use has them): rhythm and arpeggio patterns that play any chords. There are 24 of them in four groups. The patterns and their names are our own design.
- **Arpeggios:** Up, Down, Up and down, Sixteenths, Broken thirds, Alberti.
- **Strums & pulses:** Held, Quarter pulse, Eighth pulse, Pop push, Off-beat chops, Strummed (down and up strokes spread like a guitar strum).
- **Ballad & broken:** Ballad, Oom-pah, Stride, Gospel, Rolled, Pad and bell.
- **Rhythmic riffs:** Tresillo, Synthwave, Gallop, Funk stabs, Driving octaves, Montuno (two bars long).

**IDEAS' new PATTERNS tab:** a box at the right picks the group, and its six patterns appear as cards, each playing the chord track's chords.
- Chords move as little as they can from one to the next, with the root in the bass.
- HEAR plays a card; USE IT writes it onto the selected track or a new one.
- In other time signatures the patterns fit the bar (in 3/4 a quarter pulse falls on 0, ¾, 1½ and 2¼ of a beat).

**PERFORM on the chord pads** (the PLAYER menu): pick a pattern, and a pad you hold plays its chord through it at the song's tempo.
- **Song playing:** it locks to the bars.
- **Song stopped:** it starts from your press.
- **Changing chords:** another pad switches chord without losing the beat.
- **Stopping:** letting go stops it. "Off" brings plain chords back.
- **The key's label:** it shows the pattern ("PERFORM: GALLOP").
- **Recording:** what the pads perform is recorded onto an armed track, as plain chords are.

## 0.38.0

**The MIDI editor's info line** (you asked: as the automation's, for notes). A strip along the top of the MIDI editor, above its ruler, shows the selected notes: START, END, LENGTH, PITCH and VELOCITY.
- **Which values it shows:** those of the first selected note, meaning the earliest, or the lowest if several start together. With several selected, the values are pink and it says how many.
- **Changing them:** double-click a value and type. Every selected note changes by what the first one changes, so the selection keeps its shape:
  - **Pitch:** your example works exactly so. D#-1 typed as D-2 takes every selected note 13 semitones down, an octave and a half step. Notes are named as the rest of the studio names them (middle C is C3), with sharps or flats ("Eb3"), or as a MIDI number. If a note would fall off the keyboard, the whole selection moves only as far as it can, so its shape is kept.
  - **Velocity:** all up or down by the same amount.
  - **Start:** all moved in time together.
  - **End:** each note's end moves by the same amount, so its length changes and its start stays.
  - **Length:** each note gets longer or shorter by the same amount. It's written bars.beats.16ths.ticks from zero: 0.1.0.0 is one beat.
- **Ctrl as you press Enter** sets them all to exactly the value you typed (the manual's modifier), as on the automation info line.
- **Undo:** each change is one undo step.

## 0.37.0

Two things you asked for: an info line for automation points, as in your screenshot of the other DAW, and the last two Follow Chord Track modes, Voicings and Single Voice.

**The info line** (the manual's Info Line). A strip over the track list and the arrangement now shows the automation points you've selected: TYPE (the lane), START, VALUE and TERMINATOR.
- **Selecting:** drag a box round the points, or click them; Shift or Ctrl adds more.
- **Several points selected:** the line shows the first one's values in pink, with how many are selected.
- **Changing VALUE:** double-click it and type a new value. Every selected point moves by the same amount the first one moves, in the control's own units: dB for volume, L/R for pan, % for plug-in knobs. Your examples work exactly as you described:
  - Four points at -3.0 dB, typed as 5.0: all become +5.0 (each +8).
  - Points at -5, -1, 0 and +1 dB, the first typed as 0.0: they become 0, +4, +5 and +6.
- **Ctrl as you press Enter** sets them all to the value you typed instead. That's the manual's modifier for absolute changes.
- **START** moves them all in time by the same amount.
- **A click on TERMINATOR** switches it for all of them.
- **Delete** removes every selected point at once, as before.
- **Hiding it:** right-click the line and choose Hide Info Line. It comes back with the key command "Show/Hide Info Line" in KEYS (no key at first; give it one there).

**Box-selecting points over a high curve** now works. Dragging a box from just above a curve near the top of its lane (-3 dB sits there) used to grab the quick-scaling handle and squash the curve instead of selecting. That handle is now a small spot over the middle of each segment, so a box can start almost anywhere.

**Follow Chord Track: Voicings and Single Voice** (the manual's last two modes).
- **Voicings:** chords you played take the chord track's voicing of each new chord, with your rhythm kept: your lowest note becomes the voicing's lowest, and so on. A single note moves to the matching voice of the new chord, keeping its octave.
- **Single Voice:** every note becomes one voice of the voicing: SOPRANO (the top), ALTO, TENOR or BASS (the lowest). Any part becomes a harmony line or a bass line in your rhythm.
- **The voicing libraries.** The manual names the libraries and their styles but doesn't publish the voicings, so these are ours:
  - **Piano:** the root in the left hand and the right hand close. Styles: Triads; Triads with 9 (no root); 4-Note (no root); 4-Note Open Jazz; 5-Note. Options: Add Root Note, Duplicate Root, Fatten Up.
  - **Guitar:** barre shapes with the root on the 6th or 5th string, 4-note shapes with the seventh, or 3-string triads.
  - **Basic:** close from C3, with an octave offset.
  - **Adaptive Voicings** (on at first): each chord moves as little as it can from the one before.
- **Where to set them:** the Inspector's CHORDS section has VOICE (Single Voice) and VOICINGS. VOICINGS sets the track's own library, or uses the chord track's, and it also sets the chord track's library for every track that follows it.

## 0.36.1

**Longer track names in the track list** (you asked): a name now runs right up to the level meter. The row kept a 180-pixel strip on the right from when MON and LANES sat there. They moved to the Inspector in 0.35.0, but the strip stayed, so names were cut short with "..." while the space beside them stood empty. The strip is now exactly as wide as the meter and the volume knob, and the name takes the rest.

## 0.36.0

Follow Chord Track: a MIDI track's notes follow the chord track. Change a chord and the part plays along with the new one, without being played again.

**Turning it on** (the Inspector's new CHORDS section, on instrument tracks)
- **FOLLOW** chooses how the notes follow: CHORDS & SCALES, CHORDS, SCALES or ROOT NOTES, or OFF.
- **The first time**, a dialog asks how the notes relate to the chord track. These are the manual's three choices:
  - **Follow Directly:** the notes already go with the chord track's chords (made from them, or played over them).
  - **Analyze Chords:** the notes have nothing to do with the chord track. Their own chords are found in them first, as Create Chord Events does.
  - **Apply a Known Chord:** the notes stay on one chord all the way; you say which.
- After that, switching modes needs no dialog. **Set Up Again...** in the same menu asks again.

**What each mode does.** The manual says what each mode is for, not its exact rules, so these rules are our own:
- **Root Notes:** every note moves by as much as the chord's root moved, the short way (up to 5 semitones up or 6 down). It's like a transpose track, and good for bass lines.
- **Scales:** the notes move by steps of the scale. C E G over C, followed to F in C major, becomes F A C, with no notes from outside the key. When the scale changes (C major to A minor), each note keeps its place in the scale.
- **Chords:** the notes move with the root, then each one goes onto the nearest note of the new chord.
- **Chords & Scales:** a chord note stays the same chord note. The third stays the third and the seventh stays the seventh, if the new chord has one. The other notes move by scale steps.
- **Doubles:** when two notes land on the same pitch at the same moment, the softer one isn't played. The manual warns that some notes may be muted.

**Your notes stay as you played them.** Following changes what you hear, not what's written. In the MIDI editor, a pearl outline shows where each note actually sounds, linked to the note it came from. The track's row says FOLLOWS and the mode. Mixdowns, Render in Place and Freeze all use what you hear.

**LIVE IN** (the manual's Live Input): the keys you play into the track go onto the chord track's chord (CHORDS) or scale (SCALES) at the playhead. They're recorded that way too. Each key's note-off goes to the same note its note-on went to, so nothing hangs.

**SHIFT** (the manual's Shift Chords; it's the chord track's own setting): a chord can take over a little early, so notes played just before it (negative ticks) still take it. It also works the other way for notes played late (positive ticks). There are 480 ticks in a beat.

**Map to Chord Track** (a MIDI part's menu): the part's notes are written moved onto the chord track, in the mode you pick. The part's own chords are found first; if none are found, C is assumed, as the manual has it. This is one undo step.

**Not in this release:** the Voicings and Single Voice modes (the manual's voicing libraries aren't published, so they would need our own), and audio tracks (VariAudio segments) following the chord track. If you play the chord pads into a track with LIVE IN on, the pads' notes are moved to the chord track's chord too.

## 0.35.0

VariAudio, part two: the rest of the manual's VariAudio, from editing the segments themselves to harmony voices. Also, as you asked, LANES and MON have moved into the Inspector.

**The Inspector: LANES and MON** are in a new row directly beneath RENDER and PRESET. An audio track shows LANES and MON side by side; an instrument track shows LANES across the row. They're no longer on the track rows, and MON has left the M S R W REC row, so it isn't shown twice.

**Editing the segments** (the manual's Segment Editing)
- **Split:** click the segment's bottom edge where you want it cut, or Alt-click anywhere on it.
- **Glue:** the small orange controls below a segment's corners glue it to the one before or after. With several neighbouring segments selected, they all become one.
- **Move a segment's start or end:** hold Alt and drag the control on the lower half of its left or right side. It stops at the next segment; segments never overlap.
- **Delete** (or Backspace) removes the selected segments, as the manual has it, for breaths and consonants that need no pitch editing.
- After each of these, the segment's average pitch is found again from its curve. As the manual warns, a changed segment can therefore move in pitch slightly.

**The pitch curve** (SMART CONTROLS: ALL, the manual's Show All Smart Controls)
- **Tilt:** drag the white triangles above the corners up or down to tilt the start or the end of the curve.
- **Rotate:** with Alt, the whole curve turns round its anchor.
- **The anchor:** the pink mark on the top edge, in the middle by default. Drag it to move it; Alt-click it to put it back.
- **Straighten a range:** the two thin marks on the top edge set where Straighten works. Inside the segment it eases in and out over 20 ms, so the move into the next note stays natural.

**Formants** (FORMANT, from -100 to +100 %, and the F control above a segment): the voice's colour, darker or brighter, with its pitch kept. 100 % moves the formants by 12 semitones; that scale is our choice.

**Timing** (Warp Start and Warp End): drag the control on the lower half of a segment's side to move its start or end in time. The segment and the one beside it stretch to fit, using warp markers on the clip, as the manual has it.

**The Functions menu**
- **Extract MIDI:** a note per segment at its pitch now. Pitchbend: none, one per segment, or following the curve, with a range of 1 to 24 to set on your instrument. Loudness: one velocity, each segment's own, or a controller curve (CC 7 unless you choose another). It goes onto the first selected instrument track, replacing that track's earlier extraction from the same clip, or onto a new track.
- **Generate Harmony Voices:** up to four new tracks named Soprano, Alto, Tenor and Bass. Each has its own copy of the clip and of its recording, so their edits stay their own. With chords on the chord track, the voices follow the chord at each note. Without, they're 3 semitones up and 3, 6 and 9 down, as the manual has it. They're also offered in an audio clip's menu.
- **Resets** for pitch, curve, formant, volume, warp, or all of them, on the selected segments or every one; and **Reanalyze Audio**.

**MIDI INPUT** (and STEP): play a key, and the selected segment moves to that note. With several selected, the rest move as far. With STEP, the next segment is then selected. The range is E0 to C5, as the manual gives.

**Display:** a **MIDI reference track**, whose notes show as lines to sing against (saved with the song), and the segment **colours**: Auto, the track's own colour, by pitch, or by whether each note fits the scale. All are in the family's colours.

**The panel** scrolls when the window is short, because it has grown.

**Honestly:**
- I can't hear the formant shifts or the harmony voices here. The tests check that the pitch is kept, that the sound changes where it should, and that each voice is heard at its note. Whether a formant shift sounds natural on your voice is for your ears.
- The harmony voicing over chords follows our own rules: the chord's next note at least a whole step above, the ones below, the chord's bass underneath.
- Not done: Note Expression extraction, the Project Clipboard as an extraction target, and a separate Show Default / Show All per segment.
- Fixed in passing: a VariAudio segment's times now come back from a saved song exactly. They came back a hair off before, too little to hear.

## 0.34.5

**Multi-core audio.** Until now every track, instrument and effect was processed one after another on a single core of your processor, however many it has. Now tracks are processed side by side on all of them, the way Cubase does it. Heavy instruments and effects (Kontakt, Superior Drummer, amp sims) get far more room before the CPU meter fills up and the sound breaks up.

- **How it works.** In each block of audio (a few milliseconds), every track's own work, its instrument or clips, its insert effects, its fader and pan, is shared among helper threads, one per core besides the audio thread's own. Group channels come next, then FX channels, because they need the tracks' sound first; then the master. Each track's sound is still added into its group, its sends and the master on one thread, in the same order as before.
- **The sound is identical, to the last bit.** Adding numbers in a different order can change the last digit of a sample. Keeping the adding in the old order means the output is exactly the same as on one core. The tests check this with a 24-track song (groups, pre- and post-fader sends, automation, fader moves, effects that remember what they played), comparing every sample. They also run the whole test suite with every studio on four cores.
- **How much faster:** on the 4-core machine here, that 24-track song took 708 ms on one core, 405 ms on two and 256 ms on four, so 2.8 times faster. Your PC's gain depends on its cores and your plug-ins. A song with one very heavy track gains least, because one track still runs on one core.
- **The audio thread never waits for a helper to start.** It takes work itself until none is left, then waits only for what helpers have already begun. A helper that Windows is slow to wake costs nothing but its help. On Windows the helpers use the system's Pro Audio scheduling (MMCSS), as audio threads do; nothing else in the studio is raised.
- **SETTINGS has a MULTI-CORE AUDIO switch (on).** It shows how many cores are in use. Switch it off if a plug-in misbehaves when run beside others; a few older ones assume they are alone. Everything then runs on one core, as before. The session log notes which way it was on each start.
- **Exports, Render in Place and Freeze** use the cores too.
- **Honestly:** I can't measure your PC from here. I can't promise every plug-in is happy on a helper thread either; running plug-ins this way is how every modern DAW does it, and it's the reason the switch exists. Not done: plug-in delay compensation, which is a separate thing; macOS's Apple-Silicon "audio workgroups" (the Mac build doesn't join them yet).

**Also in this release:** 0.34.4, which never published. Its new strip-by-strip drawing test failed on macOS by one pixel: a Mac shades the edge where a redraw cuts a line slightly differently. The test now draws with JUCE's own renderer everywhere. The app was fine; Windows and Linux had passed.

## 0.34.4 (released inside 0.34.5: its macOS test failed by one pixel)

Running better, the screen's second part. Nothing looks different. Every screenshot in all three finishes, and the arrangement and transport drawn strip by strip the way the playhead redraws them, came out the same as before, pixel for pixel.

- **Automation lanes draw only where the screen is being redrawn.** As the playhead moves, the studio redraws a thin strip of the arrangement 30 to 60 times a second. Each time, every automation lane rebuilt its curve over the whole song, every point and every bend, and drew it all, for the screen to keep a few pixels of it. Now each lane builds only the part of its curve in that strip, plus a margin either side, so the lines crossing into the strip are the same lines as before. The same goes for Trim's played curve, the dotted value lines and the tempo track's line. Lanes outside the strip altogether aren't drawn at all. On a long song with dense automation, this was the biggest cost of the playhead moving.
- **The arrangement works out its row layout once per redraw.** It used to work it out again for each track, each take lane and each part it drew, and building that list once means a song with many tracks doesn't cost many times over.
- **The transport's screens.** The position screen changes 30 times a second while playing, and each time all five screens (position, time, CPU, tempo, signature) were drawn again, each with its blurred shadow, though only one had changed. Now only the screen being redrawn is drawn.
- **Honestly:** I can't measure the gain on your PC from here, only the work removed. It matters most for you because your window spans three screens: a lane's curve is drawn across 5,760 pixels.
- Still to come in the speed work: reading back controllers on a jump (it scans from the song's start), and smaller items. After that, VariAudio part two, when you say.

## 0.34.3

DAWproject import fixes (your report from a Cubase song), and the first part of the screen work.

**DAWproject import**
- **A new track after an import no longer fails or turns up as the wrong instrument.** The import never told the song which track number came next, so the next new track got the same number as the first imported one. When the studio then looked for the new track, it found the old one: the new track could fail, or come in with that track's instrument (Superior Drummer, for example). New tracks are now numbered after the imported ones. As a safety net, any song that opens checks this too.
- **Rack instruments come across as instrument tracks.** Cubase writes a rack instrument as two tracks: a MIDI track holding the notes, and an audio channel holding the instrument. PR7-S made the channel an audio track and put the instrument (Superior Drummer) in its effect slots, while the MIDI track got PR7-K. Now they become one instrument track: your instrument with its settings, the MIDI track's parts, the MIDI track's name and place, and the instrument channel's own mix (its fader, pan and effects: what you heard). When several MIDI tracks play into one instrument (a multi-part Kontakt), their parts all go onto that one instrument track and the report says so: PR7-S has no MIDI tracks playing into another track's instrument yet. Each MIDI track's MIDI channel isn't kept, so a multitimbral set-up needs its parts moved by hand.
- **No second master.** A song's output came in as an ordinary track beside PR7-S's own master whenever the file didn't label it as the master. Now the channel the other tracks play into, which itself plays into nothing and has no clips on it, becomes the master: its level, effects and automation. If the file gives no routing at all, an empty channel named Stereo Out, Main Out, Master or Output is used.
- **Honestly:** I haven't seen a DAWproject file written by Cubase itself, only the format's specification and reports of how Cubase lays these out, and the tests use files built to that shape. If something still comes across wrong, send me the .dawproject (or the project.xml inside it; it's a zip) and I'll match the import to it exactly.

**The screen (the first part of the speed work's third step; nothing looks different, checked pixel for pixel in all three finishes)**
- **Moving a fader no longer redraws everything.** Each step of a fader, pan or send drag redrew the whole arrangement (across three screens on your set-up), the track list, the mixer and the transport. Now a change that only moves the mix is left to the meters and faders that show it.
- **Mixer and Inspector send slots** redraw only when what they show changes, not every slot on every frame.
- **A closed MIDI editor or mixer** stops following the playhead and the meters.
- Still to come: drawing only the visible part of automation lanes, and the transport's and arrangement's other redraws.

## 0.34.2

Running better, part two: the playback engine. As with 0.34.1, nothing you hear changes. The tests check the output is the same sample for sample, not just close.

- **Moving a fader no longer rebuilds the whole song.** Every change, including each mouse movement of a fader, a pan, a send, a knob or Touch automation writing, rebuilt every MIDI track's playback list from scratch: every note, every controller ramp written out every 1/128 beat, then all of it sorted again.
  - Now each track remembers exactly what its list was built from: its parts, MIDI channel, soloed lane and drum map. If none of those changed, the list is kept as it is.
  - On a heavy test song (16 tracks, 2,000 notes each, mod-wheel ramps), one update after a fader move went from **8.5 ms to 0.07 ms**. A fader drag makes about 60 a second, so that was about half a second of work every second on the screen's thread, and it's now almost nothing.
- **Audio clips mix faster.** Each audio clip was added in one sample at a time, with its fades and bounds checked on every sample, and its position worked out again from the tempo map for every clip in every block, even clips minutes away.
  - Positions are now worked out once per edit.
  - The middle of each clip, where no fade applies, is added in one fast block operation; the fade edges go a sample at a time as before.
  - The test renders a clip with an offset, fades ending between samples and a tempo ramp under it, in three different block sizes. It matches the old way exactly at every sample.
- **Automation is read more cheaply.** Every 32 samples, finding a track's volume, pan and mute read every lane on it, plug-in knobs and sends included, then threw those results away. Now it only reads the lanes that matter. The meters' readout at the end of each block skips the lanes entirely on tracks without volume, pan or mute automation.
- **Faders, the master and sends that aren't moving** are applied in one block operation instead of sample by sample (the same gain, so the same result).
- **Fewer spikes after an edit while playing.** After an edit during playback, the engine checks that every held note still has an ending ahead. That check walked the track's whole event list once per held note. It now walks once for all of them and stops as soon as every note is settled, so dense controller lanes no longer make an edit cost a spike.
- **Honestly:**
  - The 8.5 ms to 0.07 ms is measured here on that test song; the clip and automation gains I can't measure on your PC, only estimate from the work removed.
  - Not done (yet): reading back controllers on a jump still scans from the song's start (only on play, jump and cycle laps), and the screen still redraws more than it needs to after each change. That's next, in 0.34.3.

## 0.34.1

Running better, part one: the safe fixes from a review of the whole studio for speed (you asked for one before VariAudio part two). Nothing you hear or see changes. These remove wasted work, one possible cause of clicks, and memory that kept growing.

- **No more memory allocation while audio plays when Trim is on.** Reading a trimmed automation curve made a temporary copy of the trim's points, many times per audio block. Asking for memory on the audio thread can make it wait, and that is a classic cause of clicks and dropouts. It now reads the curve in place: the same numbers, worked out the same way, with no copy.
- **Quiet tails no longer cost extra processor time.** When a reverb tail, a fade or a filter decays towards silence, the numbers get so tiny (below about -760 dB) that a processor slows down working on them, often 10 to 100 times. That's the "CPU spikes when the song goes quiet or stops" effect. The studio now treats them as zero, as nearly every DAW does, while playing, rendering and stretching. It covers our own processing and any plug-in that relies on the host for it. Nothing that small can be heard.
- **Undo steps no longer copy every plug-in's settings.** Each undo step copied the whole song, every plug-in's saved settings included, then threw those settings away (undo never used them). After opening or saving a song, that could be tens of MB per edit with Kontakt or Superior Drummer. Now the settings are set aside while the copy is made. The drum maps and hand-edited hitpoints are left out of the copy too, because undo never put them back either.
- **Memory is freed when you change song.** The hitpoint, VariAudio pitch and edited-sound stores were never emptied. Every recording ever analysed stayed in memory, two copies of it, until you quit. They're now emptied with the recordings whenever a song is opened or created, the sample rate changes, or a recording is written again.
  - **This also fixes a bug:** hitpoints were stored by file name only, so a second song with a "Guitar 01.wav" of its own could show the first song's hitpoints.
- **The warped clips' store has a size limit.** Each marker drag or tempo change made a new copy of a whole warped clip, up to 64 of them (about 4 GB of three-minute clips). It's now limited by size, about 400 MB, like the stretches it's made from. Anything dropped is simply made again when it's needed.
- **The track list doesn't redraw on sideways scrolls.** With Auto-Scroll on Stationary Cursor, the arrangement scrolls every frame while playing. The track list was laid out and drawn again 30 times a second each time, although it only follows up and down scrolling.
- **Honestly:** I can't measure the gains on your PC from here. How much they help depends on your songs: Trim, plug-ins with long tails, how long you work in one session, and warped clips.
  - Tested: the trim values come out identical to the last digit, undo still works with a song holding 1 MB of plug-in settings (kept untouched), a same-named recording in a new song finds its own hitpoints, and the warp store counts what it holds. All 291 tests pass.
- **Next, 0.34.2:** the bigger changes from the review. Moving a fader will stop rebuilding the whole song's playback, audio clips will mix faster, and automation will be read more cheaply, each proven to give the same output sample for sample.

## 0.34.0

VariAudio, part one: the third of the four. It's pitch editing for a sung (or any one-note-at-a-time) recording. You can tune a flat note, take the wobble out of a held one, move a note to another pitch, or snap a whole line onto the song's scale. Part two (0.35.0) adds splitting and gluing segments, formants, tilting curves, timing per note and MIDI from the voice.

- **Open it:** double-click an audio clip (or its menu: **VariAudio...**). It opens in its own window; double-clicking another clip switches it to that one.
- **EDIT VARIAUDIO** finds the notes and splits the recording into **segments**, one block per note. Each block sits at the pitch it's sung at on the keyboard down the side, with its **pitch curve** drawn inside it. Breaths, rests and most consonants fall in the gaps between segments.
- **Moving a note:** drag a segment up or down, or use **Up / Down**. **PITCH SNAP** sets how it lands:
  - **ABSOLUTE:** exactly on a semitone.
  - **RELATIVE:** by whole semitones, keeping how sharp or flat it was.
  - **OFF:** anywhere.
  - Hold **Shift** for off, **Ctrl** for absolute or **Alt** for relative while dragging (the manual's keys). **Shift+Up/Down** moves a single cent.
- **Smart controls** on the note under the mouse, as in the manual's default set:
  - the white handle at the top **straightens the curve** (drag up: less vibrato and drift);
  - the red handle at the bottom **corrects the pitch** toward the nearest semitone.
- **The panel's sliders** do the same for the selected notes (all of them if none is selected): **CORRECT PITCH**, **STRAIGHTEN CURVE**, and **VOLUME** for each note's own level.
- **Scale:** use the **EDITOR SCALE** (chosen in the box, or from **SUGGEST**, which lists the scales that fit the notes best) or the **CHORD TRACK**'s scale at each note.
  - **SNAP TO SCALE:** moved notes land on the scale's notes.
  - **SCALE GUIDES:** notes outside the scale are shown darker.
  - **QUANTIZE PITCHES:** the selected notes onto the nearest scale note.
- **Selecting:** click, Shift or Ctrl-click to add, or drag a box. **Left / Right** step through the notes. **Ctrl-click a piano key** selects every note on it; **Shift+double-click** a note selects it and every later one on the same note.
- **Hearing it:** **AUDITION** plays the selected notes as changed. **BYPASS** plays the recording as it was, to compare. **RESET** removes it all.
- Every change is one undo step and is saved with the song. Playback, export, Render in Place, bounce, warping and Audio Alignment all hear the edited recording. Nothing in it changes the recording file itself.
- **Honestly:**
  - How it hears pitch: the YIN method, every 5 ms, 60 Hz to 1100 Hz. That covers a low bass voice to a high soprano, but it's for one note at a time: chords and distorted guitars won't analyse well, as the manual also warns.
  - A new note starts where the pitch settles somewhere else for 50 ms. A vibrato or a short scoop doesn't make one, but two notes on the same pitch sung legato stay as one segment until part two lets you split it.
  - The pitch is moved with the same stretcher as warping, **keeping the voice's formants** (no chipmunk effect). Small moves sound natural; the further a note moves, the more it sounds processed (the manual says the same).
  - Tested with sung-like notes made in code (each found within 0.12 semitones; moved notes analysed again land within 0.1; the stretches between the notes are the recording sample for sample), not by ear on your vocals.
  - Not yet (part two): splitting, gluing and resizing segments, formant shift, tilt and rotate, straightening only part of a note, timing per note (warp start/end), MIDI from the voice, Step / MIDI input, the color schemes, harmony voices.

## 0.33.0

(Includes 0.32.0, hitpoints and audio warping, if its own release didn't appear: its builds passed on Windows and Linux, but GitHub had no Mac machine free to build it. Its notes are in the changelog under 0.32.0.)

Audio Alignment: the second of the four (then VariAudio in two parts). It lines up a double, a harmony or a second guitar with the take it should follow, without you dragging each note by hand. The result is ordinary warp markers (0.32.0), so anything it gets wrong you fix with the WARP tool.

- **Open it:** right-click an audio clip or a range, **Audio Alignment...** (or give the command "Open/Close Audio Alignment Panel" a key in KEYS).
- **The reference** (the take to follow): select its clip, or a range on its track, then **SET REFERENCE**.
- **The targets** (what moves): select clips on other tracks, or a range across them, then **ADD TARGETS**. With several, the box beside it chooses the one shown.
- **The view:** the reference above, the target below, the stretch where they overlap lit.
  - **Drag from the reference down to the target:** a sync point. That moment of the target lands exactly there. It turns red when it can't (too far for MAX SHIFT, or out of order with another).
  - **Ctrl-drag on the target:** a protected range. That stretch is moved but never stretched (a held note, a breath you like).
  - **Shift-click** either one to remove it.
- **Settings** (kept between songs):
  - **MATCH WORDS** (the manual's): the sound's shape counts more than its loudness. Best when the words are the same.
  - **PREFER TIME SHIFTING** (the manual's): when a plain move is close enough, the audio is moved, not stretched, to the sample. This is for two microphones on one performance (a guitar's close and room mics: the phase lines up).
  - **PRECISION** (the manual's Alignment Precision): at 100 % every moment lands within 2 ms; lower keeps a looser, more natural feel with fewer markers.
  - **ONLY WHERE BOTH PLAY**, **FLEXIBILITY** (low, normal, high) and **MAX SHIFT** (10 to 500 ms) are our own, after the way a dedicated vocal-alignment tool works. They set where to match, how freely the pace may change, and how far any moment may move.
- **ALIGN AUDIO:** each target is split where it stops overlapping the reference (short 10 ms crossfades at the cuts, as the manual says) and only that piece is aligned. The report line says how far apart they were on average before and after. It's one undo step.
- **Honestly:**
  - How it matches: both are measured every 10 ms (loudness, attacks, and the spectrum's shape, which follows syllables). The best path between them is found within MAX SHIFT, then each marker is placed to the sample on the attack nearby. The "before / after" figures are measured on that 10 ms grid, so "3 ms after" can mean closer than that.
  - Tested with notes made in code (a different voice on the target, timing wandering by up to 60 ms: every note lands within 5 ms), not by ear on your vocals or guitars. Real doubles that differ a lot in their words or articulation may need a few sync points.
  - The stretcher softens attacks it stretches a little (about 10 % in the tests). Notes that didn't need stretching are copied untouched.
  - Not built:
    - pitch matching (making a double follow the lead's pitch): after VariAudio;
    - aligning to several references at once;
    - aligning clips that are already warped. Their markers are replaced, and the report says so.

## 0.32.0

Hitpoints and audio warping: the first of the four releases you asked for (then Audio Alignment, then VariAudio in two parts). This is the groundwork the others stand on, and it's useful by itself: fixing the timing inside a recording without cutting it up.

- **Hitpoints** (the manual's: "musically relevant positions in audio files"): where each note, hit or consonant starts, found in every recording automatically. They're drawn on the selected audio clip when you're zoomed in far enough. Right-click a clip, **Hitpoints and Warp > Hitpoint Settings...**:
  - **Threshold:** quieter ones dropped.
  - **Minimum Length:** none closer together than this.
  - **By hand, with WARP:** Alt-click adds a hitpoint, Shift-click one removes it. These edits are kept with the song for each recording.
- **Alt+N / Alt+B** on an audio track: the playhead to the next or previous hitpoint (the manual's keys). On other tracks they stay the MIDI editor's random velocities.
- **Warp markers and the WARP tool** (the manual's Free Warp in the arrangement):
  - Click on an audio clip where a beat is. The marker catches the hitpoint nearby (Ctrl: exactly where you clicked).
  - Drag it where the beat belongs: the audio before and after it is stretched or squeezed to fit, its pitch kept. The grid pulls it in when it's near (Ctrl: free).
  - Drag a marker to move it again. Hold a modifier key and click a marker to remove it.
  - Each marker shows how much the audio before it is stretched (over 1.00 stretched, under squeezed; plain 1.00 isn't shown).
- **Create Warp Markers from Hitpoints** (the clip's menu): a marker at every hitpoint, ready to drag.
- **AudioWarp Quantize** (the header's quantize menu, at the bottom): Q on an audio clip then stretches its markers to the quantize value. As the manual describes, each grid line takes the marker nearest it, and the markers in between keep their spacing. With it off, Q moves the clip's start to the grid. Q works on MIDI and audio clips selected together.
- **Reset Warp:** the clip at its own speed again.
- **Flatten:** the warped clip written as it sounds to a new 32-bit file, its markers gone. Your original recording is kept.
- **Warped clips stay right when edited:** cut, trimmed, resized or comped, the audio stays where it was in the song. Bounce and Render in Place play them warped. Glue refuses warped clips (Flatten them first).
- **Honestly:**
  - Hitpoints are found from sudden changes in the sound, the usual method. They're best on picked guitar, drums and clear consonants, as the manual says. Smooth legato or a heavily distorted sustain may need hand edits.
  - The stretching is done ahead of time, not while playing. Moving a marker remakes only the two stretches beside it, but a long clip with many stretched markers takes a moment the first time.
  - Not built:
    - the manual's Musical Mode (a clip following tempo changes by itself);
    - the Beats filter;
    - slices, regions, groove and MIDI notes from hitpoints;
    - "Sizing Applies Time Stretch".
  - Tested with recordings made in code (plucks at known times: found within 3 ms, a moved hit heard within 10 ms of where it was put, the pitch kept within 3 %), not by ear on your guitar.

## 0.31.0

Render in Place as you work, Q from anywhere, and the instrument from the editors (all four asked for).

- **Render in Place's new defaults:**
  - **32-bit float:** nothing over 0 dB is ever cut off, and you can turn it down later with nothing lost. Files are about a third bigger than 24-bit.
  - **The original track is muted, disabled (its plug-ins unloaded: no CPU) and hidden.** VISIBILITY shows it again; enable it to change it.
  - Your saved Render in Place settings were moved to these once, since what was saved were the old defaults rather than choices. Anything you pick in the Render in Place window from now on stays.
- **Only the cycle or the range:**
  - Rendering a whole track (the Inspector's RENDER, the track menu, Alt+R) with a range set renders the range: its tracks and its stretch.
  - Otherwise, with CYCLE on, it renders only the cycle's stretch.
  - Before, it always rendered the whole track.
  - A stretch never disables or hides the track, since that would silence the rest of it: only the parts rendered there are muted, and the rest plays on.
- **Alt+R: Render in Place, as last time.** It renders the range if there is one, else the selected parts, else the selected tracks (over the cycle while CYCLE is on). It's a key command (KEYS: Edit > Render in Place), so you can change it or put it on the Stream Deck. **Read All automation no longer has a key:** use the header's R, or give it one in KEYS.
- **Q quantizes from anywhere**, to the quantize value on the header (1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64), as the manual's Quantize does:
  - in the MIDI or drum editor: the selected notes, or all of them with none selected;
  - in the arrangement: every note of the selected MIDI parts.
  - Each note moves to the nearest grid line and keeps its length.
  - Before, Q only worked right after a click inside the editor, and used the editor's own grid. The editor's QUANTIZE key now shows the value it uses ("QUANTIZE 1/16").
- **INSTRUMENT** at the top of the MIDI and drum editors' left panel opens the track's instrument window, which stays above the studio while you edit.
- **Honestly:**
  - Q quantizes note starts only (lengths stay), with nothing like the manual's Quantize Panel options (swing, soft quantize, groove) or Reset Quantize.
  - The INSTRUMENT key was tested with a stand-in synth that has no window of its own: the test checks that the key asks for the right track's window, not the window itself. With PR7-K or your VST3 instruments it opens their window, as the Inspector's EDIT does.

## 0.30.0

Takes and comping, part two: the rest of the manual's Assembling Operations and Assembling a Perfect Take.

- **The COMP tool, all of it:**
  - **Ctrl-click and hold:** audition. Plays from there, only this track (on a lane: only that lane), until you let go; then back to where you were. The song isn't changed (the manual's Speaker tool).
  - **Drag a split** (where two takes meet) left or right: it moves on every take cut there, so your comp's boundary moves as one.
  - **Drag a take's edge:** every take with that start or end is resized together. It's held so it never runs into another take on its lane, shortens a take to nothing, or runs past the end of its recording.
  - **Alt-click:** every take on the track is cut there (the Comp tool works on all lanes at once).
  - **Drag on the track's own row:** the takes stacked there move together.
- **Slip Event** (Alt+Shift-drag a take, with SELECT or COMP): what plays inside the take slides earlier or later while the take stays put, for fixing the timing of a part-take. Audio stays within its recording.
- **The comp handle:** a small tab on the lower edge of any take that overlaps another. With LANES on, a click brings that take to the front (again: back). With lanes hidden, it opens the list of stacked takes to choose from (the manual's Handling Overlapping Audio).
- **When the comp is done** (right-click a take):
  - **Bounce Selection** (audio): what's heard of the selected takes becomes one new clip, written to a new file in the song's audio folder, fades included.
  - **Bounce MIDI:** the heard notes of the selected takes become one part.
  - **Delete Overlaps:** of the selected takes only what's heard stays, all on one lane.
  - **Merge MIDI in Loop:** the heard MIDI between the locators goes onto a new track (a copy of this one, same instrument).
- **Honestly:**
  - Our choices:
    - the edge and split drags snap to the grid (SNAP off: free);
    - Ctrl in COMP is audition, so it doesn't also turn snapping off;
    - the Slip drag snaps its amount to the grid;
    - a bounced clip is a 32-bit file.
  - Not built: gluing splits by double-clicking a range (bringing a range to the front glues already), auto fades and crossfades between comped takes (the manual suggests them after comping; not in PR7-S yet), Track Versions (you said you didn't want them).
  - Checked with test recordings and real mouse drags in code, not by ear with your guitar takes.

## 0.29.0

Takes and comping, part one (the owner's last item; the manual's Lanes, Takes, and Overlapping Events and Assembling a Perfect Take).

- **Takes on lanes:** a track's parts now sit on lanes. **LANES** on a track's row (or its right-click menu: Show Lanes) shows each lane under it, with an empty one at the bottom to drag a take onto. The track's own row then shows what's heard: the comp.
- **One clip at a time on an audio track** (the manual's rule): where audio clips overlap, only the one in front plays. Until now they all played together. **Songs with overlapping clips will sound different:** the clip added last is in front, and you can bring another forward with To Front.
- **To Front:** right-click a part. Audio: the clip comes to the front. MIDI (overlapping parts all play): the part is unmuted and the ones it overlaps on other lanes are muted.
- **The COMP tool** (in TOOLS, after SELECT):
  - Click a take on a lane: it comes to the front. Click it again: the take it hid comes back.
  - Drag across a lane: every take is cut there and that stretch of this lane comes to the front. Pieces of the same recording that meet are joined again.
  - Shift-click selects, Alt-click splits, and on the track's own row it moves parts as SELECT does.
- **Drag a take to another lane** (SELECT or COMP on the track's row).
- **Lane solo:** S on a lane's row. Only that lane is heard; solo the track too to hear it alone.
- **Record modes** (right-click REC):
  - Audio:
    - **Keep History** (the default, as the manual's): what's there stays, and each take goes on a lane.
    - **Cycle History + Replace:** the recording replaces what's there, and every lap of a cycle is kept.
    - **Replace:** only the last take is kept, in place of what was there.
  - In a cycle, **every lap is now a take of its own on its own lane**, the last one in front. Before, only the last complete lap was kept. A last lap stopped part-way is kept if a beat or more was played.
  - MIDI:
    - **New Parts:** the new part goes beside what's there.
    - **Merge:** the new notes go into the part that's there.
    - **Replace:** what was there in the recording's stretch goes.
  - MIDI in a cycle:
    - **Mix:** as before.
    - **Overwrite:** playing in a lap replaces what earlier laps left from that point on.
    - **Keep Last:** the last lap played through to its end.
    - **Stacked:** each lap a part on its own lane, all but the last muted.
    - **Mix-Stacked:** the same, none muted.
- **Clean Up Lanes** (track or part menu): what isn't heard is removed (audio: the hidden stretches; MIDI: muted parts under heard ones) and empty lanes go. **Create Tracks from Lanes** (track menu): each lane after the first becomes a track of its own.
- **Honestly:**
  - Not in this release, coming in the next:
    - auditioning with Ctrl in COMP (the manual's speaker);
    - the comp handle on a take's lower edge;
    - dragging a split between takes;
    - constrained resizing and Slip Event in COMP;
    - Bounce and Delete Overlaps.
  - Not built: Track Versions (you said you didn't want them).
  - Our choices, not the manual's:
    - Show Lanes turns itself on when a recording stacks takes (in the manual you click it);
    - the record modes are saved with the song;
    - each audio lap is a clip of its own (where the manual makes one event with regions);
    - clicking a heard audio take again brings back the one it hid.
  - Checked with test recordings rendered in code, not with a real guitar through your interface.

## 0.28.0

The Automation panel (F6), the rest of automation as the manual has it (the owner asked for all of it).

- **The panel** (F6, or the key command Open/Close Automation Panel): **READ** and **WRITE** for every track, **SUSPEND** (all reading and writing), the mode, **TRIM**, **FUNCTIONS**; then **FILL**, **SUSPEND READ**, **SUSPEND WRITE**, **SHOW**; and a **SETTINGS** tab. Everything in it is saved with the song.
- **The modes:** **Touch** (as before: it writes while you hold a control, and goes back to what was there when you let go), **Auto-Latch** (let go, and its last value goes on being written until you stop, the cycle comes round, or W goes off), **Cross-Over** (as Auto-Latch; touch it again and move it back across the old curve, and the pass ends where it crosses, with no jump). On/off switches (mute) always latch, as the manual says.
- **Fill** (what happens when a pass ends): **To Punch** (the value you ended on, from where you started: what you did while searching for it is gone), **To Start** (from the song's start), **To End** (to the end, no return), **Loop** (between the locators), **Gaps** (virgin territory's gaps filled). A click: once, for the next pass; a second click: locked, every pass; a third: off.
- **Suspend Read / Write** by kind: volume, pan, mute, sends, plug-ins (SET ALL): a suspended read leaves the control where you put it by hand; a suspended write keeps it out of a pass (the manual's example: mute your tracks to listen without the mutes being written).
- **Trim** (volume and send levels): with TRIM on, a pass (or editing the lane) writes a **trim curve** on top of the automation, up or down to 12 dB, keeping the automation as it was. The lane shows the automation light, what plays darker, and the trim curve. **Freeze Trim** (functions menu, a lane's menu, or by itself: on pass end, or on leaving trim mode) turns it into the automation.
- **Virgin territory** (SETTINGS): no curve before a lane's first point or after its last, and **gaps** (the control stays as you set it there). Deleting points makes a gap; a point can be a **terminator** (the lane's menu: a gap after it). Fill Gaps (the functions) fills them.
- **Functions:** Delete All Automation in Project, of the Selected Tracks, in Range (the locators); Fill Gaps on Selected Tracks (with its section's value, or the control's value now); Freeze All Trim / Freeze Trim of Selected Tracks; Copy and Paste Automation of All / Selected Tracks.
- **Show:** every track's VOLUME, PAN, MUTE, SENDS or PLUG-INS lanes (USED ONLY: only those with automation), SHOW USED, HIDE ALL.
- **Settings:** Use Virgin Territory, Continue Writing on Transport Jump (a jump starts a new pass instead of ending writing), Reveal Parameter on Write (the lane opens when it's written), Return Time (Touch's way back, 0 to 1000 ms), Reduction Level (how much a pass is thinned: 0% only repeats; the manual's default 50%), Freeze Trim.
- **Key commands** (KEYS, Automation): the three modes, Trim, Suspend All, Fill To End / To Punch / Loop: for the Stream Deck.
- **Honestly:** not built: Spike Detection Range, the Last Touched Parameter options, Show Data on Tracks, the panel in the right zone (a window here), drawing with Fill (fills act on passes, not on drawing), a mode key on the toolbar (the header has no room left at the smallest window: F6 and the key commands instead), the per-track automation mode. Trim's range (12 dB either way) is our choice. Checked with test passes written by the code, not with your hands on a controller.

## 0.27.0

Automation editing, the first part of automation as the manual has it (the owner asked: type a node's value, select several nodes, and everything else; the Automation panel's modes, Trim, Fill and Suspend come next, in 0.28.0).

- **Typing a value (the owner asked):** double-click a point on an automation lane: a box with its **VALUE** as its control reads it (volume and sends in dB: -6, +3 dB, -inf; pan L30 / C / R20; mute ON / OFF; a plug-in's knob in %) and **AT** where it is (bar.beat.16th.tick). Enter or SET. With several points selected, all of them take the value. Also in the lane's right-click menu: Type a Value.
- **Selecting several points (the owner asked):** Shift-click or **Ctrl-click** adds a point to the selection (or takes it out); drag a box round several (with Shift or Ctrl held, added to what's selected); the lane's menu has Select All Events.
- **The Automation Event Editor** (the manual's): two or more points selected on a ramp lane get a box with handles. The top edge moves them all up or down; the top's middle scales them; the top corners tilt the left or right end (Alt: compress it); the right edge's middle scales them round the lane's middle (Alt: round their own middle); the bottom edge stretches them along the song.
- **Curved ramps** (the manual's Bézier curves): hover over a ramp's middle and a handle appears; drag it up or down to bend the ramp into a curve; double-click the handle (or the menu's Straighten the Selected Ramps) to make it straight again. Playback follows the curve.
- **Quick vertical scaling** (the manual's): at the lane's top edge over a ramp, a small handle scales that ramp's two points.
- **The tools on a lane:** DRAW draws points, ERASE removes the points it's dragged across, and a new **LINE** tool (in the tools row, after DRAW; click it again for its shapes) draws a **Line** (a ramp), a **Parabola** (Ctrl: curved the other way), a **Sine**, **Triangle** or **Square** (one cycle per grid step, swinging round where you start, as far as you drag).
- **Copy and paste automation** (the manual's Copy Automation to Other Tracks; the lane's right-click menu): copy what's between the locators, of all tracks, the selected tracks, or this parameter, and paste it between the locators somewhere else: all tracks onto the same tracks, the selected tracks onto other selected tracks in order, a parameter onto another lane. What was there is replaced; a shorter range cuts it, a longer one holds the last value to the right locator; the curve either side carries on. One undo step.
- **Also in the lane's menu:** Show All Used Automation, Remove Unused Parameters.
- **Honestly:** the curves are our shape (a power curve between the two points), drawn and played the same, rather than a true Bézier; LINE has no key (the manual gives it none); the event editor works on one lane at a time (the manual's Ctrl for several tracks isn't built), and selecting points with the RANGE tool isn't either. On the wide layout the tools' keys are a little narrower so ten fit where nine did. A MediaBay test that depends on timing failed once here and passed on every rerun; I'm watching it. Next (0.28.0): the Automation panel (F6): Touch, Auto-Latch and Cross-Over, Trim, the Fill options, Suspend Read/Write, Show, its functions, virgin territory and its settings.

## 0.26.0

The drum editor, with drum maps (the owner asked: a drum editor with the map built in that says what drum each note plays; as the manual's Drum Editor and Drum Maps).

- **DRUMS** (in the MIDI editor's panel) turns the editor into the drum editor: the piano gives way to the **drum sound list**, a row per drum with its note and its **name** (BASS DRUM 1, ACOUSTIC SNARE, CLOSED HI-HAT...), in the map's order. Hits are drawn as **diamonds**; LENGTHS draws them as boxes as long as they are instead. The editor opens on the first drum the part plays.
- **The GM Map is built in:** the General MIDI drum kit's names (35 Acoustic Bass Drum to 81 Open Triangle, plus General MIDI 2's extra sounds). Choosing DRUMS on a track with no map gives it the GM Map; choosing a map for a track turns the drum editor on by itself (the manual's "Use Drum Editor when Drum Map is assigned").
- **Your kits' maps:** MAP (in the panel) and **DRUMS** in the Inspector's Routing choose the track's map: the GM Map, the song's maps, or any **.drm** drum map file in **Documents/Paragon PR7-S/Drum Maps** (Open the Drum Map Folder shows it). **Load a Drum Map (.drm)** reads the drum map files the other DAW uses, which drum instruments such as Superior Drummer, EZdrummer or Addictive Drums can export (check your instrument's mapping page); Save as a .drm writes one.
- **The drum list:** click a drum to hear it; **M** mutes it (its hits go hollow and silent); double-click its name to rename it; right-click for Select All Notes of This Sound, its I-note, O-note and channel, and which drums are listed (the named ones and those with hits, only those with hits, or all 128).
- **The drumstick** (DRAW in the drum editor): a click puts a hit on the grid step; dragging right puts a hit on every step (the manual's Inserting Multiple Drum Note Events). Moving hits up and down moves them from drum to drum along the list (Up and Down too).
- **What the map does when playing** (the manual's I-notes and O-notes): each drum is sent to the instrument as its **O-note** (so a pattern written for one kit plays the right drums on another: change the map, not the notes), on its own **channel** if it has one, and **muted** drums are silent. What you play in goes through the **I-notes**: the key set for a drum plays and records that drum.
- **Drum Map Setup** (from MAP, the drum list's menu or the Inspector): the song's maps (NEW MAP, NEW COPY, REMOVE, LOAD, SAVE, rename), each drum's name, I-note, O-note, channel and mute, the list's order (UP / DOWN), and FOR THE TRACK. Changes act at once on every track using the map and are saved with the song.
- **The piano roll** names the drums too, when its track has a map (in the panel's foot and on notes wide enough).
- **Honestly:** the .drm reader follows the format as we know it (the files are XML: a list of 128 drums with their I-note, O-note, channel, mute and name, and their order); it's checked with files we made in that layout, not with one exported from another program, so if one of your kit's files doesn't load, send it and I'll fix the reader. Not built: per-drum Snap and Insert Length (the editor's grid is used), the score settings (noteheads, voices, display notes), Create Drum Map from Instrument, O-Note Conversion, Solo Instrument, drum maps on MIDI outputs (instrument tracks only). Not checked by ear here: the mapping is checked to the note with a test instrument.

## 0.25.0

MIDI controller lanes in the MIDI editor (the owner asked; as the manual's Controller Display): modulation, pitchbend, expression, sustain, any of the 128 controllers, aftertouch, poly pressure and program changes, drawn, edited, played, recorded and imported.

- **Lanes under the notes:** the velocity lane is now one lane among several. **LANES** (in the editor's panel) adds one (Create Controller Lane: the most used first, Modulation, Pitchbend, Expression, Volume, Sustain, Pan, Cutoff..., then all 128 by number, Aftertouch, Program Change, Poly Pressure), shows a lane for every kind the part already has (Show Used Controllers), goes back to velocity alone (Show Velocity Lane Only) or hides them all. Each track keeps its own lanes, saved with the song. Drag the line above the lanes to make them taller.
- **Each lane's name** (at its left) opens its menu: what it shows, Select All Events on Lane, Remove This Lane, Cut / Copy / Paste / Paste at Origin, the selected events' curve (step or ramp), Type a Value. "(USED)" marks the kinds that already have events. Above the name: the value at the playhead.
- **Drawing:** DRAW clicks an event onto the grid step (Ctrl: anywhere), or draws a curve as you drag. ERASE clicks or drags across events. SELECT with Alt draws too.
- **LINE** (a new tool in the panel; click it again for its modes): **Line** (as a ramp, only its two ends), **Parabola** (Ctrl: curved the other way), **Sine**, **Triangle** and **Square** (one period a grid step, swinging round where you start the drag, as high as you drag; Ctrl: the other phase), **Paint** (freehand). On the velocity lane it sets the notes' velocities along the line (the selected notes, when some are).
- **Selecting and editing:** SELECT clicks an event (Shift or Ctrl: add or remove), drags a box round a group, drags events in time and value. Two or more selected get the same handles as the velocities: tilt either end, scale, move, squash towards their middle. Delete removes them; Ctrl+C / X / V copy, cut and paste them (at the playhead, onto the lane last clicked: another controller's lane takes them as its own).
- **Typing a value (the owner asked):** double-click an event: type its value as the lane shows it (64; pitchbend +4096 or in semitones, "2st"; pan L12 / C / R12; sustain ON / OFF), where it is (bar.beat.16th.tick) and whether it ramps. With several selected, all of them get the value. The panel's foot shows the selected event.
- **Steps or ramps:** new events ramp to the next by default (LANES > Type of New Controller Events: Ramp or Step). Bank select, sustain, the RPN/NRPN numbers, the mode messages and program changes are always steps (the manual's list).
- **Pitchbend's grid:** the pitchbend lane's menu shows a semitone grid and snaps to it (Snap Pitchbend Events), with its range up and down (1 to 96 semitones, 2 at first, kept per track). Set your instrument's bend range to match.
- **Playing:** each event at its beat on the track's MIDI channel, before a note at the same beat (so a note starts already bent). A ramp is sent as a run of values. **Chasing:** pressing PLAY, jumping, or the cycle coming round first sends each controller's value there, so a sound starts with the pedal, the bend and the controllers it has at that place. **Stopping** puts the bend back to the middle and lifts the sustain pedal (nothing rings on or stays out of tune).
- **Recording:** what you play on the mod wheel, the bend, the pedal and aftertouch goes into the part with the notes (a part with only controllers is kept too). In a cycle, each controller keeps the last pass you played it in.
- **MIDI files** bring their controllers in (CCs, pitchbend, aftertouch, poly pressure, program changes). **Split and Glue** carry them; a ramp cut in two still gets where it was going.
- **Honestly:** not built from the manual's chapter: SysEx, articulations and dynamics lanes, note-off velocity, velocity variance and play probability, Bézier curves (ramps are straight), Auto Select Controllers (moving notes doesn't move their controllers), lane presets, the MIDI Controller Setup dialog (every controller is offered instead), and automation tracks for MIDI controllers (the next releases' automation work). Moving notes with the keyboard doesn't move controllers. No default key for LINE (the manual gives it none we could find): give it one in KEYS if you like. Not checked by ear here: the playing and the chasing are checked to the sample with a test instrument, not with your Kontakt or Shreddage.

## 0.24.0

VST presets in the MediaBay and the Media rack (the third of the three additions the owner chose): your Shreddage, Kontakt or PR7 presets found, tagged and loaded like any other media.

- **VST PRESET** is a new kind of file in the MediaBay (`.vstpreset`, the VST3 preset files plug-ins save, usually in "VST3 Presets" folders). Scanned like the rest: tick the folder holding them in the FILE BROWSER. Each shows its own name (from inside the file), its plug-in (in FORMAT), and the tags it carries: many presets say their instrument (Guitar / Rhythm), style (Metal / Djent) and character, and those become its tags (yours always win, as before).
- **PRESETS** (the type key, and the Media rack's Presets page) now shows both kinds: track presets and VST presets.
- **Double-click or USE IT:** an instrument's preset makes a new instrument track with that plug-in and the preset loaded, named after the preset; an effect's preset makes a new FX channel with it.
- **Drag onto a track:** an instrument's preset onto an instrument track changes its instrument to that plug-in with the preset (or, when it's already that plug-in, just loads the preset into it); an effect's preset loads into that effect when the track has it, otherwise it's added to the track's inserts. Onto a track it can't go on: a new track, as a double-click.
- **The Media rack:** right-click a VST preset: Add a Track with It, or Load on the selected track (or on the master, for an effect's preset).
- **Its plug-in** is found among your scanned plug-ins by the preset's plug-in id (as the plug-in reports it), else by its name; when it isn't scanned, the message says which plug-in it needs (PLUGINS > SCAN / OPTIONS).
- **Honestly:** the preset is loaded as the plug-in's whole state, as the VST3 standard does; a few plug-ins keep their sounds outside their presets (some samplers ask for their library again). Not checked here with real plug-ins (none can run in the cloud tests): checked with made-up presets, read and matched exactly as the real ones are. No preview for presets yet (the manual plays a few notes through the instrument: a later step); no FX-chain or strip presets (the studio doesn't have them).

## 0.23.0

The MediaBay's previews in time with the song (the second of the three additions the owner chose), as the manual's Audio Files Previewer.

- **LINK TEMPO:** a loop is stretched to the song's tempo at the playhead, keeping its pitch: a 140 BPM riff previews at your song's 120. It needs the loop's tempo (from its name, its loop info or your tag); one-shots and files with no tempo play as they are.
- **WAIT FOR PLAY:** the preview starts and stops with the song. With LINK TEMPO too, a loop **locks to the song's bars** from the left locator (put the left locator on a bar's start, as the manual says), and keeps going round in time, the cycle included: pick loops one after another while the song plays and each one drops in on the beat. Waiting, the previewer says so.
- **TRANSPOSE:** up or down to 12 semitones. **TO KEY:** moves the preview to a root key, the short way (a loop in A minor to C: up three; to E: down five), when the file's key is known.
- **1/2X and 2X:** half or double speed, the pitch kept.
- **Where:** in the MediaBay window's previewer (LINK TEMPO, WAIT FOR PLAY, 1/2X, 2X, TRANSPOSE, TO KEY) and in the Media rack's (LINK, WAIT, and PITCH for the rest). They share one set of options, kept for next time. A sound being heard is heard again with a change at once.
- **MIDI files** take the transposition and the speed too (they always played at the song's tempo).
- **The stretching** is done by Signalsmith Stretch (an open-source library, MIT licence, now part of the build), on the whole sound before it plays, so nothing heavy runs while the audio does.
- **Honestly:** the stretch follows the tempo at the playhead when you press PLAY (a tempo track's later changes aren't followed while it plays); files longer than 90 seconds play unstretched; the beat lock assumes the loop is a whole number of beats (it rounds to the nearest when the length is that close); not built: Link to Transpose Track (the studio has no transpose track yet), tapemachine style, and putting a stretched loop into the song stretched (it goes in as the file is: that needs audio that follows the tempo, a later release).

## 0.22.2

Two things the owner asked for: the MediaBay opens where you left it, and it reads tempo and key from inside your files (the first of the three additions the owner chose: this one, then previews in time with the song, then plug-in presets).

- **The MediaBay opens where you left it** (the owner's report: every time it opened, the folder tree was back to the start). The folders you had open in the FILE BROWSER are open again, the folder you'd chosen is chosen again (scrolled to), and the results show its files. Kept in the settings, so it holds after closing the studio too.
- **Tempo and key from inside the files**, not only from their names:
  - **WAV loops with ACID info** (most commercial loop packs): the tempo, the length in beats and the root note. **AIFF Apple loops:** the beats, the root note, and whether it's major or minor. A loop without a tempo but with its beats gets its tempo worked out from its length. One-shots (a kick, a hit) show no tempo.
  - **What wins:** your own tempo and key first; then for tempo the file's loop info, then the name's; for key the name's (a name like "Riff_Em" says minor), then the file's root note (ACID doesn't say major or minor: the key shows as the plain letter, "A").
  - **Tags the other DAW's MediaBay wrote into WAV files** (in their iXML): category and sub category, style and sub style, character, tempo, key and comment become the file's tags here the first time it's read, so loops you tagged there arrive tagged. Yours always win: a tag you change stays as you set it.
  - **A WAV's description, genre, comments and keywords** are searched too (a pack's "Djent" genre finds its loops).
  - Files the MediaBay already knew are read again once, so your scanned library gets all this without anything to do (the scan runs in the background, as usual).
- **Honestly:** the other DAW doesn't publish the names of the tags it writes into files; ours reads any of its attributes whose names hold "Category", "Style", "Character", "Tempo", "Key" or "Comment", which is our best reading of them (checked against the layout its files use, not against your own files yet). MP3 and FLAC files' tags aren't read; nothing is ever written into your files.

## 0.22.1

The MediaBay's folder tree (the owner's screenshot of the other DAW's: the File Browser with its folders and subfolders, each with a box to scan it).

- **FILE BROWSER** takes LOCATIONS' place at the left of the MediaBay window: a tree of folders. Its roots: **SCANNED FOLDERS** (what you've ticked, at a glance: our addition), **FAVORITES** (the same favorites as the Media rack's), **THIS COMPUTER** (Home, Desktop, Documents and every drive) and **USER CONTENT** (the Paragon PR7-S folder). The arrow opens a folder (its subfolders are read then); double-clicking a folder opens it too.
- **The box beside each folder scans it**, as the manual's Scanning Folders: a **tick** means it and all its subfolders are scanned; a **square** means some of it is (a subfolder left out, or only a subfolder ticked); **empty** means none of it. Tick a folder deep in a drive (F:\Samples\GHOSTHACK) and only that folder is scanned; untick one subfolder inside a scanned folder and only that one is left out, its files leaving the results at once (their ratings and tags kept).
- **Click a folder** to see its files in the results (with SUBFOLDERS lit, its subfolders' files too); click a root heading for every scanned file. A folder that isn't scanned says so in the title line.
- **Folder colours:** pearl white when scanned, dim when not, the accent colour while a scan is running.
- **Right-click a folder:** Scan This Folder, Don't Scan This Folder, **Refresh Views** (its subfolders read again and a rescan, the manual's way to see files added while the studio was closed), Add Favorite.
- **ADD FOLDER...** (a folder picked from the system's chooser, for ones the tree can't reach easily) and **REFRESH** (the selected folder's Refresh Views) under the tree.
- A favorite added in the MediaBay window shows in the Media rack too.
- **Honestly:** our design where the manual leaves room: SCANNED FOLDERS is our root (the manual's has VST Sound and Factory Content instead; the studio has neither); left-out folders are remembered in MediaBay.xml; the arrow shows on every folder until it's opened (reading every folder's subfolders up front would be slow on big drives).

## 0.22.0

The Media rack (the owner asked for 0.22.0 as planned: the manual's Media Rack in the Right Zone), the last of the MediaBay's three parts: your sounds, instruments, effects and presets in a panel at the right of the arrangement, so you can see your tracks while you drag things into the song.

- **Ctrl+Alt+R** shows or hides it (the manual's Show/Hide Right Zone key; also IMPORT > Media Rack, and in KEYS as Show/Hide Right Zone). It runs the whole height on the right, as the Inspector does on the left; drag its left edge to make it wider or narrower. It stays shown (or hidden) for next time.
- **HOME** has the manual's tiles: **VST INSTRUMENTS** (the studio's synth and yours, by category or by maker), **VST EFFECTS**, **LOOPS & SAMPLES** (the audio and MIDI files in the MediaBay's folders), **PRESETS** (your track presets), **FAVORITES** and **FILE BROWSER**. BACK and HOME find your way back; the address line shows where you are.
- **Search** at the top looks through the page (on HOME: every file), by name or by your tags. **ALL** shows every result.
- **Results pages** have the type keys (AUDIO, MIDI, PRESETS, SONGS), SHUFFLE, and **FILTER**: two attribute filter columns (CATEGORY and STYLE at first; click a title for another), as in the MediaBay window. **CLEAR** lights up while a filter is set. Each file shows its type, tempo, key, category and style.
- **FAVORITES:** ADD FAVORITE picks a folder; it becomes a tile, and its files go into the MediaBay (as the manual's). Click a tile for its files; right-click to delete the favorite (the folder stays).
- **FILE BROWSER:** your favorites, your home folder, desktop and drives, and the Paragon PR7-S folder. Double-click folders to go in (UP to come out); only media files are listed. ADD FAVORITE takes the selected folder (or the one you're in); right-click a folder for it too.
- **The previewer** at the bottom: PLAY / STOP and AUTO PLAY, with the sound's shape and playhead (MIDI through the selected track's instrument).
- **Into the song:** double-click, or drag onto the track list or the arrangement. An **instrument** makes a new instrument track with it; an **effect** a new FX channel (right-click an effect to insert it on the selected track or the master instead; right-click an instrument to change the selected track's instrument). Audio and MIDI land as from the MediaBay window. A **track preset** dropped on a track of its kind is applied to it (the manual's Applying Track Presets); dropped anywhere else it makes a new track.
- **Honestly:** our version of the manual's rack. Only the MEDIA tab (no VSTi rack, control room or meter tabs), no plug-in pictures, collections or Plug-in Manager, no content sets, one Presets page for track presets (no strip, FX chain or pattern presets: the studio doesn't have them), and effects dropped on the Inspector's or mixer's insert slots aren't taken yet (right-click > Insert on... does it).

## 0.21.1

Tags for the MediaBay (the owner asked for 0.21.1 as planned: the manual's Attribute Inspector and Attribute Filter), so you can find a sound by what it is, not only by its name.

- **The attribute inspector** (at the right; INSPECTOR in the title row shows or hides it): the selected files' attributes in groups. MEDIA shows what the files are (name, type, length, format, size, date, folder). MUSICAL holds your tags: **Category** and **Sub Category** (Guitar / Riff, Drums / Kick...), **Style** and **Sub Style** (Metal / Djent...), **Character**, **Tempo** and **Key**. INFO holds the **Rating** and a **Comment**. YOUR ATTRIBUTES holds the ones you add.
- **Several files at once:** select them in the results (Shift- or Ctrl-click), then click a value: it's set on all of them. Where they differ the inspector says (MIXED). Right-click a value: Remove Attribute.
- **Clicking a value:** Category and Style open their lists; Sub Category and Sub Style list only the subs of the files' category or style (as the manual's: they're linked; a new category drops a sub that isn't its own). "Add a category..." puts your own value in the list (and a sub under it), then sets it. Character opens pairs of opposites (Clean / Distorted, Dark / Bright, Tight / Loose, Calm / Aggressive...): one side of each, or neither. Key opens the 24 keys; Tempo and Comment ask for text; the stars set the rating.
- **Your tempo and key** win over what the name says; remove them and the name's are back.
- **User attributes** (+ USER ATTRIBUTE): an attribute of your own, holding text, a number (with the decimals you choose) or yes/no ("Session", "Strings", "Used in a song"). Right-click its name in the inspector to delete it from every file.
- **DYNAMIC / DEFINED:** only the attributes the selected files have, or all of them.
- **The attribute filter** (above the results; FILTERS shows or hides it): four columns, CATEGORY, SUB CATEGORY, STYLE and CHARACTER at first; click a column's title for another attribute (key, tempo, rating, or one of yours). Each lists the values your files have with how many files have each. Click a value to see only those files, click it again for all; **Ctrl-click** another value in the same column for either one. Columns work together, and the sub category column shows only the chosen categories' subs. Character values always all apply (as the manual says). RESET clears them.
- **The results** have CATEGORY, STYLE and CHARACTER columns (sort by category or style), and the **search** looks in your tags too ("djent", "aggressive", a comment's words).
- **Honestly:** our design where it differs from the manual. Tags are kept in the MediaBay's own file (MediaBay.xml beside the settings), **not written into your audio files**: your samples stay exactly as they are, but the tags don't travel with a file copied to another computer. The lists of categories, styles and characters are our own words (add yours). Not here: the Defined attributes' configuration mode (which attributes show per media type), editing values directly in the results list, the logical filter, write protection.

## 0.21.0

The MediaBay (the owner asked: as the other DAW's MediaBay). The first part of three, as agreed: the MediaBay window now; tags and the attribute inspector next (0.21.1); the Media rack docked at the right of the project window after (0.22.0).

- **F5 opens it** (or IMPORT > MediaBay...). A window of its own, kept where you put it.
- **LOCATIONS:** the folders it scans for media, each with its subfolders. The first time, your Documents' Paragon PR7-S folder (your songs and track presets) is in; ADD FOLDER... for your sample libraries, REMOVE to stop scanning one. Click one to see only its files (SUBFOLDERS: with or without its subfolders), or ALL LOCATIONS. Scanning runs in the background and **waits while the song plays or records**, as the manual's does; RESCAN looks again for new, changed and removed files (unchanged files aren't read again). What it found, and your ratings, are kept in MediaBay.xml beside the settings, so it opens straight away next time.
- **The results:** audio files (WAV, AIFF, FLAC, Ogg, MP3), MIDI files, track presets and songs, with their rating, name, type, length (MIDI: bars), tempo, key, format (sample rate and mono/stereo; MIDI: its notes), folder, size and date. Click a heading to sort. Tempo and key come from the file's name ("Drop A Chug 140bpm", "Riff Am", "Pad F#m"; a MIDI file's own tempo if its name has none).
- **Finding things:** the search box looks in the name, the folder, the type, the tempo and the key, with the manual's boolean search: words all found (a space, "and" or +), either ("or" or a comma), none ("not" or a minus at a word's start), parentheses, and "a phrase" in quotation marks. For example: `drum (kick, snare) -808`. AUDIO, MIDI, PRESETS and SONGS show only those types (none lit: all). The rating filter shows files rated that or better. SHUFFLE mixes the list; RESET clears the filters.
- **Rating:** click a star in the first column (the same star again: none).
- **The previewer:** select a file and PLAY. Audio plays over the output whether the song plays or not, at the previewer's LEVEL, with its shape drawn and a playhead; CYCLE loops it; AUTO PLAY plays each file as you select it. MIDI files play through the selected track's instrument (your guitar VST if it's selected) at the song's tempo.
- **Into the song:** double-click (or USE IT): audio onto a new audio track at the playhead, a MIDI file onto a new instrument track, a track preset as a new track, a song opened (after asking about unsaved changes). Or **drag** files from the list onto the arrangement: they land as files dragged from Explorer do, snapped, on the track under them (a MIDI file onto an instrument track, lit in the gradient while you drag).
- **Honestly:** our own design where the manual leaves room: the locations are whole folders, not a tree of ticked subfolders; tempo and key come from names, not from listening to the audio. The previewer doesn't stretch audio to the song's tempo (Link to Project Tempo needs time stretching, which the studio doesn't have yet), nor follow a transpose track, nor play at half or double speed; the level applies to MIDI previews only through their velocities. Not yet: tags, categories and the attribute inspector (0.21.1), the logical and attribute filters, plug-in presets (.vstpreset), the docked Media rack (0.22.0), Wait for Project Play.

## 0.20.2

- **IDEAS from the chord pads** (the owner looked for the piano and guitar parts in the pads and couldn't find them). The pads' toolbar has an **IDEAS** key on its second row, after FROM CHORD TRACK: it opens the IDEAS window (piano, guitar and bass parts, VELOCITIES, TUNING, HUMANIZE) for the chord track's chords, those in the cycle when it's on, else all of them. The PLAYER menu's old "(Piano and guitar players: a later release)" line pointed the wrong way: it's now "Piano, Guitar and Bass Ideas..." and opens IDEAS too.
- **Where IDEAS works from:** the chords on the **chord track**. With chords only on the pads, put them on the chord track first: drag pads onto the chord lane, double-click a popular progression in the circle panel, or right-click a progression > Ideas for It (that does both at once).
- **Honestly:** the pads' PLAYER menu is still the basic close-chord player (the pads' own sound when you press them); the piano and guitar parts live in IDEAS as parts you add, not as a way the pads play.

## 0.20.1

A key for Humanize, and its settings in the Hub (the owner asked).

- **Alt+H humanizes.** Humanize is now a key command (KEYS, MIDI > Humanize), on Alt+H to start (H for humanize; plain H is still zoom). With notes selected in the MIDI editor it loosens them; otherwise it loosens **every note of the parts selected in the arrangement** (whole parts, which 0.20.0 couldn't do). One undo step either way. Like any command it can be put on a Stream Deck button or in a macro.
- **HUMANIZE in the Hub.** HUB SETTINGS has a HUMANIZE section: the key (SET KEY, then press the key you want, with Ctrl, Alt or Shift as you like; Escape cancels; CLEAR for none), AMOUNT (5-100%) and "Keep velocities in their layers". It's the same key as in KEYS (change it in either and the other shows it) and the same amount and layers as the MIDI editor's HUMANIZE SET (its key follows the amount).
- **Honestly:** a key set in the Hub that another command already had is taken from that command, as KEYS does; check KEYS if a key you used for something else stops working. The IDEAS window keeps its own HUMANIZE amount (it loosens the ideas before you add them), separate from this one.

## 0.20.0

Humanize, in the IDEAS window and the MIDI editor (the owner asked): small, controlled differences in timing, velocity and length so parts sound played rather than programmed.

- **How it loosens a part.**
  - **Timing:** each note lands up to 12 ms early or late at full amount. It's in milliseconds, as a player's timing is, not a fraction of the beat. Small moves are likely and big ones rare.
  - **Downbeats move least:** a bar's first beat moves least (a third as much), the other beats a little more, and the notes between them most. Players lock the "one", so it sounds played, not sloppy.
  - **Chords stay chords:** notes that start together (a chord, or a strum's six strings) move as one, and a strum's speed varies a little from strum to strum.
  - **Velocity:** up to 10 softer or harder, **always inside its layer.** With your Shreddage layout a palm mute stays a palm mute (1-59), a sustain stays a sustain (60-119), a rake stays a rake (120-126) and a pinch harmonic stays exactly 127.
  - **Length:** up to 12% longer or shorter (how soon a palm mute is choked, for instance).
- **In IDEAS:** HUMANIZE in the header (every tab) shows the amount ("HUMANIZE OFF" to start, so nothing changes until you turn it up) and opens a panel with AMOUNT (0-100%) and FEEL. FEEL gives each part a lean, growing with the amount: rhythm guitar and bass push slightly ahead of the beat (up to 3 ms), leads and melodies lay back slightly behind it (up to 5 ms), piano sits on it. Each idea is loosened its own way but always the same way, so what HEAR plays is exactly what USE IT adds; MORE IDEAS deals new performances too.
- **In the MIDI editor:** HUMANIZE 50% under the LOW/MID/HIGH velocity keys loosens the selected notes. It works on anything (parts you played in, imported or drew), one undo step each time, and the notes stay selected so you can press it again to loosen further. HUMANIZE SET (the arrow) chooses the amount (10/25/50/75/100%) and "Keep velocities in their layers" (on at first; turn it off for a piano or drums, where there are no layers to keep).
- **Honestly:** the rules are our own, from how players play; how much sounds right is for your ears (tight djent chugs want little, strums and leads more). Shreddage already varies its samples on repeated notes (round-robins); humanize adds timing and feel, which round-robins don't. Not yet: a groove taken from a recording, a key for HUMANIZE (it can be given one in KEYS once it's a command), or humanizing a whole part from the arrangement.

## 0.19.3

The guitar ideas' velocities, set by ear (the owner asked, to match them to Shreddage at home without waiting for a new version).

- **VELOCITIES.** The IDEAS window's GUITAR tab has a VELOCITIES key beside TUNING. It opens a panel with the eight velocities the guitar ideas use: palm mute tight / normal / loose, sustain soft / normal / hard, rake, and pinch harmonic, each with its own bar (drag it, or double-click to type a number) and the range Shreddage puts that articulation in beside it. Every change deals the guitar ideas again at once, so you can HEAR a card, move a bar and HEAR it again. RESET (SHREDDAGE) puts back 34 / 44 / 54, 80 / 96 / 110, 123 and 127. Your values are kept with your settings.
- **Nothing slips into another layer.** Where an idea plays a note a little softer or harder than the main sustain value (the notes between a lick's accents, say), it now stays between your SUSTAIN, SOFT and SUSTAIN, HARD, so a sustain range set narrow can't push a note into the palm-mute layer.
- **Honestly:** the panel changes the ideas you add from now on; parts already added keep their velocities (edit those in the MIDI editor's velocity lane). The ranges shown are Shreddage's, as you described them; with another guitar library, set the bars to its layers.

## 0.19.2

Rakes and pinch harmonics apart (the owner's layout for Shreddage: 120-126 rakes, 127 pinch harmonics).

- **Pinch harmonics at 127.** 0.19.1 sent its pinch harmonics at 124, which in this layout is a rake. They're now at 127: the Pedal-point riff's every second bar and the Pentatonic box's last held note.
- **Rakes (123), into the start of a phrase,** as a player rakes into a lick: the Pentatonic box and the Blues scale run rake into the first note of every second bar (bars 1, 3, 5...), the Repeating triplet lick into its very first note. Not in the chord parts (a rake goes into a single note) or the fast picking ideas (it would blur them).
- **Honestly:** 123 is the middle of the rake range; where in 120-126 the library gives its strongest rake is for your ears (say if it should move). Which notes get a rake is our choice; MORE IDEAS doesn't change it.

## 0.19.1

The guitar ideas' velocities in the layout of the owner's guitars (Shreddage Jupiter and Rogue: velocity chooses the articulation; 1-59 palm muting, tighter the lower; 60-119 sustain; 120-127 pinch harmonics and rakes).

- **Palm muting now mutes.** In 0.19.0 the "palm-muted" notes went out at velocity 70 (sustain on Shreddage), and the chugs' accents at 121, which fired a pinch harmonic or rake on every accent. Now: the Palm-muted chugs' chugs at 44 (eighths) or 34 (sixteenths: tighter, as fast chugging is played), their accents open at 110; the Gallop palm muted throughout (the eighth at 54, the two sixteenths at 44; MORE IDEAS' odd variations open each bar's first hit as a ringing chord); the Pedal-point riff's low pedal muted (54 on the bar's first, 34 after), its stabs open at 110.
- **Pinch harmonics where metal uses them.** The Pedal-point riff ends every second bar with a pinch harmonic (124) on the chord's root, and the Pentatonic box's last held note is a pinch-harmonic squeal.
- **Everything else sits in the sustain range:** power chords 96-110, octave chords 84-104, strums 80-96, leads 88-110. Nothing else reaches 120.
- **The preview.** Guitar ideas now preview alone, without the chords underneath: piano chord shapes played on a guitar library smear the riff (HEAR CHORDS still plays the chords). The chords under bass, piano and melody ideas now go out at 64 instead of 57, which was in the palm-mute layers if your guitar track was the selected one.
- **Honestly:** the numbers are chosen for the ranges you gave (each a little inside its range, so it lands in the right layer); I can't hear Shreddage here, so how tight 34 and 44 sound is for your ears; say if they should be tighter or looser. On a guitar library without velocity layers these are only louder and softer notes. The keyswitches still aren't written (the velocity layers do the articulations here); rakes aren't written either (a rake is the strings dragged into a note, and 120-127 on its own would only give the pinch or rake the library picks).

## 0.19.0

Piano and guitar ideas, apart (the owner asked: ideas "geared towards guitars in one category and piano in another", power chords and octave chords as in rock and metal, and leads from the tried and true ways of rock and metal solos; the owner plays Shreddage Jupiter and Rogue, 7-string guitars in drop A).

- **IDEAS, in three tabs.** BASS & MELODY is now IDEAS (the same places open it: the key on the chord track's row, "Piano, Guitar and Bass Ideas..." in the chord lane's menu, a popular progression's menu). Its tabs: **PIANO** (CHORDS and MELODY), **GUITAR** (RHYTHM and LEAD) and **BASS** (the six bass lines across). The tab you used last opens next time. MORE IDEAS deals new ones in every tab.
- **Piano chords (six).** Block chords (each chord voiced to move as little as it can from the last); Broken chords (the classical low-high-middle-high pattern); Ballad arpeggio (the root held low, the chord rising and falling above it on eighths); Stride (bass on one and three, the chord on two and four); Octaves and chords (left-hand octaves, the chord on every beat); Pop pulse (eighth-note chords over the root). Every note is in its chord, with its root at the bottom on each chord's first beat.
- **Guitar rhythm (six).** Power chords (root, fifth and octave: no third, so one shape fits major and minor; on a diminished chord its own flat fifth); Octave chords (root and octave on eighths, higher up the neck); Palm-muted chugs (short, quiet root-and-fifth chugs with the full power chord ringing on the strong beats); Gallop (an eighth and two sixteenths on every beat, the classic heavy metal rhythm); Pedal-point riff (the key's low note chugging under the chord's power chord, as in thrash, djent and metalcore); Open strummed shapes (full chord shapes as a guitar holds them on the top six strings, strummed down, down-up, up-down-up, each string a moment after the last).
- **Guitar lead (six).** Pentatonic box; Blues scale run (triplets through the "blue" note); Repeating triplet lick (a three-note cell a step lower every beat); Descending fours (groups of four sixteenths, each a step lower: a metal sequence); Neoclassical run (up the chord, down the harmonic minor scale, in sixteenths); Tremolo picking (one note picked on every sixteenth, the line moving through the chords). Every bar starts on a note of its chord and the last note lands on the last chord, so the licks resolve.
- **TUNING.** The guitar tab has the guitar's tuning: 7-string drop A (the default: A E A D G B E, lowest A0 with middle C = C3), 7-string standard, 6-string standard, drop D, drop C, 8-string standard and 8-string drop E. No guitar idea plays a note below the lowest string (guitar libraries like Shreddage keep their keyswitches down there, so a note below would switch articulations instead of sounding) or above the top string's 24th fret. Power chords and riffs sit on the lowest strings, as they're played. The tuning is kept with your settings.
- **USE IT onto your track.** When the selected track is an instrument track (your guitar VST, say), USE IT asks: onto that track, or onto a new one. New tracks are named for the part: Piano, Guitar, Lead Guitar, Bass, Melody. One undo step either way.
- **Honestly:** the rules are ours, from common rock, metal and piano practice, not taken from another product; they're starting points to pick from and edit. **No bends, slides or vibrato**: parts don't hold pitch bend yet, so leads are straight notes (a guitar VST's own legato and slide keyswitches still work on what you add). Palm muting is suggested by short, soft notes; whether it sounds muted depends on the guitar VST (Shreddage chooses articulations by velocity and keyswitches; we don't write keyswitches). The preview plays through the selected track's instrument, so select your guitar track to hear the guitar ideas on the guitar. Not yet: writing keyswitches, tablature, drop tunings for bass, drum ideas.

## 0.18.2

- **The drag highlight is stronger, and you set how strong.** The place a dragged-in MIDI file will land now lights at 60% by default (0.18.1's was 35%), with a thicker gradient edge, and the file's notes are drawn inside it in pearl white so you can see what's arriving. The strength is a setting: the Hub's HUB SETTINGS page has DRAG HIGHLIGHT, from 10% (a faint wash) to 100% (solid); the edge thickens with it. It's kept with your settings.

## 0.18.1

Dragging MIDI files onto tracks (the owner's report and asks).

- **A MIDI file dropped on an instrument track lands on that track.** Dragging a .mid from Explorer onto an instrument track made a new track instead: every MIDI drop went through the import that gives each of the file's tracks a new track of its own. Now the file goes onto the instrument track it's dropped on (one that isn't frozen or disabled). A file with several tracks: its first onto that track, the others onto new tracks below. Dropped on an audio track, a channel or below the tracks, it makes new tracks as before.
- **It follows snap.** Where it goes in now follows the snap setting, as moving a part does: with snap on and 1/1, it moves a whole bar at a time as you drag; with 1/4 a beat at a time; with snap off, exactly where you let go. (Before, it always went to the start of the bar under the mouse.) Dropped audio files follow snap the same way.
- **You see where it will land.** While you drag a MIDI file over an instrument track, the place it will land lights up on that track in the family's red, pink and orange gradient, as long as the file, with the file's name in it (and "+N more on new tracks" when the file has several tracks). It moves with the snap as you drag. Over anywhere else nothing lights (it would make new tracks).

## 0.18.0

Bass and melody ideas for your chords (the owner asked: "I select a chord progression and then I get suggestions for bass lines and also for melodies").

- **BASS & MELODY.** Put chords on the chord track (draw them, take a popular progression, or play them in from the pads), then open BASS & MELODY: the key on the chord track's row, "Bass and Melody Ideas..." in the chord lane's right-click menu, or a popular progression's right-click "Bass and Melody Ideas for It" (which puts it on the chord track first). It follows the chords you've selected on the chord track, else those in the cycle when it's on, else all of them, each in the key the chord track has there.
- **Six bass ideas.** Roots; Root and fifth; Driving eighths (the last one leading into the next chord); Octaves; Walking (a note on every beat, through the chord and the key, stepping a half step into each next chord's root); Syncopated (R&B and funk). Each chord starts on its root (or a slash chord's bass note), in a bass range, each root in the octave nearest the one before so the line doesn't jump around.
- **Six melody ideas.** Long tones (one held note a bar, moving as little as it can); Singable (steps through the key, chord notes on the strong beats, rising then falling); Hook (a short motif repeated every bar and bent to fit each chord); Arpeggio; Call and response; Syncopated pop (pushed off the beat, the next chord's note arriving early). Chord notes fall on each bar's first beat, the notes between come from the key, a key note that would rub a half step against the chord is left out (no G over E major in A minor), and the last note lands on the last chord.
- **Hear them, use them.** Each idea is drawn as a little piano roll over the chord names. Click it (or HEAR) to hear it with the chords played softly underneath, at the song's tempo, through the selected track's instrument; HEAR CHORDS plays the chords alone; STOP stops. USE IT puts the idea on a new track as a MIDI part under the chords (one undo step), ready to edit in the MIDI editor. MORE IDEAS deals six new ones of each (the same styles, other choices: up or down, which chord note, the hook's shape).
- **Honestly:** the rules are ours (common pop, rock, R&B and jazz practice), not taken from another product, and good-sounding is a matter of taste: these are starting points to pick from and edit, not finished parts. The preview plays everything through one instrument (the selected track's); the part you add plays through the new track's (the studio's synth, PR7-K). The ideas follow the chord track's key per chord; a progression like Am F C G is read as C major unless you set the scale to A minor on the chord track's scale strip. Not yet: drum ideas, counter-melodies, choosing a register, or ideas from audio.

## 0.17.5

- **The MIDI editor's playhead glides too** (the owner asked). As in the arrangement since 0.17.4: while stopped, a click in the editor's ruler puts the playhead on the grid and it slides there in about a tenth of a second instead of jumping; while playing or recording it stays exactly on the music, and a long jump (more than half the editor's width) goes straight there. The same speed as the arrangement's, so the two move together.

## 0.17.4

The playhead moves with some life (the owner asked: it felt static next to the other DAW's).

- **The playhead glides into place.** While the song is stopped, clicking or dragging in the ruler still puts the playhead on the nearest grid line (the quantize value, as before), but now it slides there quickly, easing in, in about a tenth of a second, instead of jumping. Dragging along the ruler, it steps from grid line to grid line with that same short slide, so you can feel the snapping. While playing or recording it stays exactly on the music (no glide: the timing matters more), and a long jump (Home, or more than half the screen) still goes straight there.
- **A marker shows where a click will land.** Hovering over the lower part of the ruler shows a small pink marker on the grid line where a click would put the playhead. It steps along the grid as the mouse moves. Hold Ctrl to place it freely (no snap), as before; with Ctrl or Alt held for the locators, the marker steps aside.
- Honestly: the glide's speed is our choice (a little under half the way each frame, 60 frames a second), and only the arrangement's playhead glides; the MIDI editor's stays as it was. How it feels can only be judged on your screen.

## 0.17.3

A fix for Change Instrument, a colour menu, and note ticks on the ruler.

- **Change Instrument works again.** Choosing one of your plug-in instruments from a track's menu (the Inspector's instrument key or the track list's right-click) did nothing; only PR7-K worked. The menu numbers its instruments from 100000 up, and the code that runs the choice first asked "is it over 30000?", the numbers it gives the Output choices, so an instrument was taken for an output to a group that doesn't exist and nothing changed. Track presets with another instrument worked because they take another road. The Output check now stops below the instruments; a test pins it down. (We can't load a real VST3 instrument in the cloud tests, so the last step is yours to try on Windows.)
- **A colour menu, only the family's colours.** A small palette key in the track list's header (beside M and S), beside DISABLE TRACK in the Inspector, and "Colour..." in the track's right-click menu open it. On top, the gradient's three: pink, red and orange. Under them, every tint of them: nine hues from the pink through the red to the orange, each in eight shades from deep to pale, and nothing else (no yellow, green, blue or violet; a test checks every swatch). A click colours the selected tracks (or the one you opened it from) and leaves the menu open to try another; the track's colour is outlined. SET TRACK COLOUR TO DEFAULT gives them their automatic colour back (red, pink, orange in turn by their place). Colouring is an undo step, and the tints are kept when the song is saved and opened (other colours still open as the nearest of the three).
- **Quarter, eighth and sixteenth note ticks on the ruler.** Along the foot of the ruler, between the bar lines: quarter notes tallest, then eighths, then sixteenths. Each only shows when you're zoomed in enough for them to be at least 5 pixels apart, so zoomed out the ruler stays clean.

## 0.17.2

Popular progressions beside the suggestions, in place of 0.17.1's single chords (what the owner meant).

- **The ten most popular progressions in the pads' key.** On the right screen, beside "What could come next", the column now lists ten well-known chord progressions, spelled in the key you've chosen, each chord with its numeral under it. In a major key: the pop anthem (I-V-vi-IV), the fifties (I-vi-IV-V), three chords (I-IV-V-I), sad pop (vi-IV-I-V), the jazz cadence (ii-V7-I), the turnaround (I-vi-ii-V), Pachelbel's Canon, mixolydian rock (I-bVII-IV-I), the ballad (I-IV-vi-V) and the minor plagal (I-IV-iv-I). In a minor key: epic minor (i-VI-III-VII), minor rock (i-VII-VI-VII), the natural and the harmonic minor cadences (i-iv-v-i, i-iv-V-i), the Andalusian (i-VII-VI-V), the minor ballad (i-VI-VII-i), the minor jazz cadence (ii°-V7-i), rising minor (i-III-VII-VI), dramatic (i-VI-iv-V) and the Dorian vamp (i-IV). Chords from outside the key are shown in orange.
- **Hear it, use it.** Click a progression to hear it, a chord every half bar at the song's tempo, each chord lit as it sounds (click again to stop). Double-click to put it on the chord track from the playhead's bar, one chord a bar, replacing what was there (the chord track appears if it was hidden; one undo step). Right-click for those and "Onto the Pads": the chords on the pads from the selected pad on (locked pads are skipped).
- Honestly: the list is ours, from common pop, rock, R&B, jazz and worship practice, not counted from a database of songs. It shows when there's room (the right screen).

## 0.17.1

The Inspector's keys as the owner asked, and the key's most used chords beside the suggestions.

- **RENDER in the Inspector, in FREEZE's place.** RENDER renders the whole track in place, with the settings you used last time (the same as the track menu's "Render in Place (as last time)"): a new audio track under it. It's greyed out when the track has no parts, or is disabled or frozen. Freeze isn't gone: it's still in the track's right-click menu (Freeze, its tail sizes, Unfreeze). For the full Render in Place page, use the track menu's "Render in Place...".
- **DISABLE / ENABLE TRACK always there.** The key at the top of the Inspector's first section now shows on every instrument and audio track: DISABLE TRACK on a track that's on, ENABLE TRACK (on red) on a disabled one. Before, it only appeared once a track was disabled. FX and group channels can't be disabled, so they don't show it. On a frozen track it's greyed out (its plugins are already unloaded: unfreeze it first).
- **The ten most used chords in the key.** On the right screen, beside "What could come next", a new column lists the ten chords most used in the pads' key, most used first, each with its numeral and a few words on its role. In C major: C, G, F, Am, Dm, G7, Em, then three favourite visitors from outside the key: Bb (bVII), Fm (the minor iv) and D (V of V). In a minor key: i, VI, VII, iv, the major V, III, the minor v, V7, ii° and the Dorian IV. Chords from outside the key are shown in orange. Click one to hear it (the suggestions then follow it), double-click to put it on the selected pad, or drag it onto a pad or the chord track. Honestly: the ranking is ours, from common pop, rock, R&B and worship practice, not counted from a database of songs. The column appears only when there's room (the right screen); with a single screen the circle and suggestions stay as they were.

## 0.17.0

The chord pads use the screens either side, and tracks can be linked.

### The chord pads on three screens
- **Every key on the left screen.** When the studio spans more than one screen, the part of the chord pads zone on the left screen now lists all 30 keys in large type. Each row is a key signature: the major key, its signature (for example "3 flats"), and its relative minor (the same notes). C comes first, then the sharp keys, then the flat keys. Click one and the pads take that key, as the KEY menu does (filling the pads with its chords, unless you chose transpose or rename there). The key in use is lit. It works whether the pads are docked or in their own maximized window.
- **The circle of fifths always on the right screen.** The circle and its scored list of what could come next now sit on the right screen whenever there is one, and their type grows with the room (up to about twice the size). The middle screen keeps the pads and the chord editor. The CIRCLE OF 5THS key is greyed out while the circle has its own screen. On a single screen everything works as before.

### Link groups (as the manual's Linking Channels)
- **Link Selected Channels.** Select two or more channels (click, Ctrl-click, Shift-click in the mixer or the track list), then click **LINK** in the mixer's bar or right-click and choose Link Selected Channels. A box asks for a name and which settings are linked: volume, pan, sends, inserts, mute/solo, automation read/write, routing, selection and monitoring. The manual's defaults are ticked: volume, sends, routing and selection.
- **What linking does.** A change to one channel goes to the others in its group, for the settings ticked. Volume, pan and send levels move by the same amount, so the channels keep their differences; with **ABS** on they are set to the same value. Selecting one channel selects the whole group (unless Selection is off). Inserts link slot by slot: adding, replacing, removing or bypassing an effect in slot 3 does the same in slot 3 on the others, and turning a knob in an effect's window turns the same knob of the same effect in that slot on the others.
- **Each on its own for a moment.** Hold **Alt** while you change something, or turn on **SUS**, and only that channel changes.
- **Q-Link.** With **Q-LINK** on (or while holding **Shift+Alt**), whatever you change on a selected channel happens on every selected channel, linked or not.
- **Seeing and changing groups.** A linked channel shows its group's number and name above its name in the mixer and the Inspector, and "LINKED: <name>" under its name in the track list. **EDIT LINK** (or the right-click menu) changes a group's name and settings. With a group's channels selected, LINK becomes **UNLINK**. Right-click a channel, "Included in Link Group", to move it into another group or out of all of them. A group left with one channel goes. Groups are saved with the song, and making, changing and removing them are undo steps.
- **Where it differs from the manual (our choices, honestly):** there is no EQ or Dynamics to link (the studio has no channel EQ or dynamics yet), and no VCA faders. Record Enable isn't linked because the studio records one track at a time; Monitor is. At the top or bottom of a fader's travel the other channels stop there, so a channel pushed to the end loses its difference from the others. When linked channels have W on and you move a linked fader while playing, every linked channel writes its automation. A duplicated track isn't in its original's group.

### Also
- The chord pads crash fix from 0.16.1 is included.

## 0.16.1

A fix for a crash in the chord pads.

- **The chord pads crashed the studio.** It happened when the suggestions were worked out after certain chords: a chord from outside the key, in a major key, that could be a "secondary dominant". The check for that read past the end of the key's scale into memory that wasn't there, and Windows stopped the studio. It now reads the scale properly. A new test asks for suggestions after every chord (all 12 roots, six chord types, plain and seventh) in all 30 keys, so this path is checked on every build. The crash only showed on Windows; Linux, where the tests ran, happened to read harmless memory instead.
- **Secondary dominants named correctly.** The same line named the chord it pulls to wrongly: in C, E pulls to vi (A minor), but the suggestion said "VI". It now names the key's own chord on that note.
- **The session log has one line per entry again.** Since 0.9.14 the log's entries after the header ran together on one long line (every entry was written without a line end), which made it hard to read. Each entry now has its own line; a test checks it.

## 0.16.0

**The chord track** (your choice after the circle of fifths; the manual's "Chord Functions"). It's a lane under the tempo track (Add Track > Chord track) holding your song's chords as a lead sheet does: each chord until the next one starts.

**On the lane:**
- **Each chord is a block** with its name and its numeral, both in the scale in force there. A chord from outside the key has its numeral in pink.
- **The scales sit in a strip underneath:** the key in force from each point on.
- **Adding a chord:** **double-click** an empty spot (or click with the DRAW tool, or Alt-click). It adds **the chord the one before suggests**, the best of the scored suggestions (the key's I if nothing comes before). Right-click an empty spot for "Add <the suggestion> Here" or any chord of the key.
- **Editing:**
  - Click a chord to select it and hear it (Shift adds more).
  - Drag to move (snapped; the selected ones together).
  - Delete removes it; the ERASE tool removes one.
  - Undo works for all of it.
- **The editor box (double-click a chord):**
  - its root, type, tension and bass, spelled in the key, each choice heard;
  - **after the chord before:** its six best suggestions with their scores, one click to take one;
  - DELETE.
  - Right-click a chord for **Replace with a Suggestion** and the rest.
- **Drop a chord pad** (or a chord from the circle of fifths) onto the lane: it lands at the bar.

**The scales follow the chords:**
- The key changes only when a chord no longer fits the key in force. G7 before E♭ is still C major's V7. Then the new key is the one holding that chord and the most of what follows.
- Right-click the strip to set the scale from a point on yourself. From then on the scales stay as you set them, until "Scales Follow the Chords" is turned back on (also AUTO SCALES on the chord track's row).

**To and from MIDI, and the pads:**
- **Chords to MIDI:** TO MIDI on the row, or the menus. All the chords (or the selected ones) become a MIDI part on the selected instrument track (or a new one if that's an audio track). Each chord lasts until the next, voiced so the voices move the least, as the pads do.
- **MIDI to chords:** right-click a MIDI part, "Create Chord Events". The chords it plays are written onto the chord track, replacing those over its span.
  - It listens beat by beat, weighing each note by how long and loud it sounds, and prefers plain chords. A passing note doesn't turn C into Cadd9; C with E in the bass reads C/E; a broken-chord (arpeggio) part still reads its chord. The tests check all three.
- **Assign from Chord Track:** TO PADS on the row, or FROM CHORD TRACK on the pads. The chord track's chords, each once, go onto the pads in order, and the pads take its key. It asks first, because the pads' chords are replaced (locked ones kept), as the manual warns.
- The chord row shows the chord and key at the playhead.

**Saved with the song.**

**Not yet:**
- **Follow Chord Track:** tracks whose notes follow the chords (root notes for bass, chords, scales). This is its own release.
- Recording onto the chord track from the pads; step input.
- **Chords from audio:** not promised; it's hard to make reliable.
- The scales are worked out from chords, not from what's played.

## 0.15.0

**Chords you can trust, in a real key** (you asked: the chords must be accurate, mostly plain major and minor; pick a key like E-flat minor or A major and get all its chords and voicings; the circle of fifths suggesting what comes next, scored).

**KEY** (it was ROOT) offers every key there is: 15 major and 15 minor, round the circle of fifths, each with its signature ("E-flat minor (6 flats)").
- **Choosing a key fills the pads with its chords.** On the keyboard layout:
  - **White keys**, the key's seven chords: I ii iii IV V vi vii° in major; i ii° III iv v VI VII in minor.
  - **Black keys:** in major, I7 ii7 IV7 V7 vi7. In minor, i7, IIImaj7, iv7, **the major V** (on the black key right beside the minor v, as you asked: both), and V7.
  - Locked pads keep their chords.
- The menu can make a key **transpose your pads** to it, or **only name them** in it, instead of filling them.
- **Spelled as the key spells them.** In E-flat minor the sixth chord is **C♭**, not B; in F-sharp major the seventh's chord is **E♯dim**, not F; B♭7 is **B♭ D F A♭**, never A♯.
  - Every chord of all 30 keys is checked by a test against the textbook spelling.
  - Roman numerals follow the key's own scale: minor keys read i ii° III iv v VI VII, and the harmonic minor's V and vii° (the raised seventh) carry no sign.
  - A chord from outside the key gets its flat or sharp (bVII, #iv°), and the plain name rather than F♭ or C♭.
- **Note names in Inter** wherever they appear (the chord editor's keys, the circle, the pads), because Bebas has no small letters and "Eb" would read "EB". The key's name on the toolbar is written out ("E-FLAT MINOR").

**Voicings by name**, on each pad's right-click menu under **Voicing**, with their notes spelled ("1st inversion: Gb3 Bb3 Eb4"):
- **Close position:** root position, then 1st, 2nd and (for sevenths) 3rd inversion, in two octaves.
- **Open (drop 2):** the second voice from the top an octave lower.
- **Spread:** the root low, then the fifth (or seventh), the third an octave up: a tenth, a pianist's two hands.
- Each one holds exactly the chord's notes, nothing added and nothing missing; a test checks that for every chord of every key.
- Adaptive voicing still picks among the close ones, so the voices move least.

**CIRCLE OF 5THS** (in the editor's place, at the right of the pads):
- **The circle:** the twelve major chords round the outside in fifths, their relative minors inside, the key's tonic at the top. The key's own chords are lit with their numerals, the tonic in orange.
- **Click a chord** to hear it and see what could come next. Double-click it, or drag it onto a pad, to use it.
- **What could come next:** after the chord clicked (or the selected pad's), up to a dozen chords, best first, each with a **score from 1 to 99**, a bar along the family's gradient, and why. Hover a row for the whole reason; double-click or drag it onto a pad.
- **The scoring is our own weighting of standard harmony:**
  - **Function:** V to I is the strongest, then IV or ii to V, vi to ii or IV, the deceptive V to vi, and the plagal IV to I.
  - **The circle:** down a fifth adds to the score.
  - **Shared notes:** each note in common adds to it.
  - **Outside the key:** borrowed chords (iv, bVII, bVI, bIII in major; IV and bII in minor) and secondary dominants score lower and are named as such.
  - **In a minor key the major V outscores the minor v.**
  - Scores of 80 and up are "strong", 60 to 79 "good", below 60 "adventurous". These are the rules a theory book gives, not the other DAW's formula (it isn't published), so we can tune them by ear.

**The pads on the middle screen.** When the studio spans your three screens, the chord pads' controls stay on the screen under the middle of the window, as the docked mixer leaves its empty width dark. On a screen we can't read, they're kept to one screen's width, centred.

## 0.14.0

**Chord pads** (you asked for them as the other DAW has them). They're laid out as the manual's "Using Chord Pads" chapter describes. **F4** opens them in the lower zone, beside the MIDI editor and the mixer (one docked at a time); WINDOW puts them in a window of their own. F4 is our choice: the manual gives the pads no key. "Show/Hide Chord Pads" and "Chord Pads Docked / Full Screen" are also in KEYS, for your Stream Deck.

**The pads:**
- **The layout.** Twelve pads laid out like an octave of a keyboard (DISPLAY: 24 pads, or a grid of 2x4 up to 4x4).
- **Playing them.** Click a pad, or press its key on your MIDI keyboard: **C1 to B1** at first, which REMOTE can move to another octave. The chord plays on the armed track's instrument, and records like anything you play. Each pad shows its key in its corner.
  - The trigger keys play the pads **only while the pads are open**, so a drum part's C1 is still a kick when they're put away. That's our choice: the manual has them work even with the pads hidden.
- **What a pad shows:** its chord (Dm7), the chord's function in the key (ii7), its voicing (V 3/5), whether adaptive voicing is on (A), a padlock if locked, and REF on the reference pad.
- **The keyboard underneath** shows the notes playing, the pads' trigger keys, and the key of the pad under the mouse.

**Changing chords:**
- **The chord editor** (EDITOR, a double-click, or Return): the selected pad's root, type (maj, min, dim, aug, sus2, sus4), tension (6, 7, maj7, dim7, add9, 9, maj9, 6/9, 11, 13, 7b9, 7#9, 7#11) and bass note. Each choice plays the chord.
- **FROM MIDI** (Assign from MIDI Input): play a chord on your keyboard, and the selected pad takes it, voicing and all. The next selected pad takes the next chord.
- **Modifiers**, on the selected pads:
  - TRANSPOSE - +, TENSIONS - + and VOICING < > (the first selected pad plays).
  - The mouse wheel over a pad: the voicing; with Shift it transposes; with Alt it changes the tensions.
  - LOCK (Ctrl+Shift+L) stops a pad being changed. REMOVE (Delete) asks first, because, as in the manual, it can't be undone.
- **Moving pads:** drag a pad onto another to swap them; Alt-drag copies.
- **Making parts:** drag a pad onto a track in the arrangement and it becomes a **one-bar MIDI part** at that bar.
- **Adaptive voicing:** each pad's voicing is chosen as it's played, so the voices move as little as they can from the chord before. Choosing a voicing by hand turns it off for that pad; ADAPTIVE turns it on again. **REFERENCE** keeps the other pads' voicings near one pad's.

**The toolbar:**
- **ROOT:** the key the chords' functions are named in. With "Transpose Pads", every chord moves to the new key. "Flats" spells Bb rather than A#.
- **PRESETS:** the seven scale presets in the root key (Major, Natural Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian), and your own, saved with **SAVE** and stepped through with < >. Choose to load the chords and the player, the chords only, or the player only.
  - On the keyboard layout we put the scale's chords on the white keys (C to B: I to vii) and its sevenths on the black keys between them. That arrangement is our choice.
- **PLAYER:** the Basic player (close chords) and its octave.
- **PLAIN CHORDS:** what happens when you play a chord while holding another: Hold First, Legato or Stop First.
- **DISPLAY:** chord symbols as note names, roman numerals or Nashville numbers (a first and a second way), the layout, the start note, and the voicing indicators.
- **REMOTE:** where the trigger keys sit, and Latch Chords (a pad plays until you press it again).

**The chords are kept with the song.** A new song starts with the major scale's chords in C.

**Honest notes:**
- **Voicings and voice leading are our own rules:** close chords, with the voices moving as little as possible. The other DAW's voicing libraries aren't published, so they won't match note for note.
- **Fonts:** chord symbols are drawn in Inter, because Bebas has no small letters, and "Dm" would have read as "DM".
- **Not yet, in later releases:**
  - the chord track (chord events, step input, Assign from Chord Track);
  - the Chord Assistant (List, Proximity, Circle of Fifths);
  - the piano and guitar players;
  - Pattern and Sections modes;
  - the remote settings dialog (MIDI Learn, the modifier keys, aftertouch, pitch bend);
  - recorded chords split across MIDI channels by voice.

## 0.13.1

**What you reported:**
- **Freeze and Render in Place put other tracks into the file.** A track frozen earlier plays its freeze file instead of its parts, and the render only cleared the other tracks' parts. So freezing tracks one after another put every earlier one into each new file, and Render in Place picked them up too. Now every track but the one rendered is fully silenced for the render: no parts, no freeze file, muted (which also silences an instrument that plays patterns of its own while the song runs). FX and group channels still carry the rendered track's sends and output.
- **The Hub opens centred on your middle screen:** the screen under the middle of the studio's window, which spans all three. Windows used to put it on the main display, your left one.
- **The POSITION screen no longer pulses or jumps.** Its value changed length as it counted (the ticks went from "0" to "119", and the bar number grew), so it moved sideways, and when it got too wide the font shrank to fit. Each field now has a fixed number of figure-wide cells, so it stays still, as TIME does.
- **Group channels sit at the right of the mixer, next to the master.** The other tracks still start from the left.
- **Question boxes (Delete track and the rest) look like the progress box:** the deep black body, the red glow along the edge, the PR7-S mark with a large title, the message under it. The keys are still placed in the middle by the box itself.

**Fixes from a review of the whole code, before chord pads.** Four reviewers read the code in four parts (the engine, the song and saving, the screen, the imports and newer windows). Every finding was checked against the code before anything was changed, and each fix has a test that failed before it.

**Sound and renders:**
- **A note no longer hangs after an edit during playback.** If you deleted, muted, moved or transposed a note (or its part) while it was sounding, its note-off went with it, and the note rang until you stopped. Now the studio ends any sounding note the song no longer ends. A note started on one MIDI channel also ends on that channel if you change the track's channel meanwhile.
- **Renders start clean.** A note left sounding by the playback, or by the previous stretch of a Render in Place or a Freeze of several tracks, could carry into the next file. Every render now starts with all notes ended.
- **Export Mixdown's CANCEL is safe.** The export had its own progress window. Its CANCEL waited for the render on the screen's thread, so an instrument loading samples (Kontakt) could hang the studio, and after 10 seconds the render was killed in the middle of its work. The export now runs like Render in Place: on its own thread, in the studio's progress box. CANCEL there (or Escape) stops it at the next block and deletes the unfinished file. **Render in Place and Freeze have the CANCEL key too.**
- **Nothing changes a render while it runs.** Turning a knob in a plug-in's window during a render, with W on, used to swap the live song back into the engine halfway through the file.
- **Plug-ins' own meters no longer make the audio thread post messages.** Gain reduction and other output meters are reported from the audio thread, and the studio was posting a message (which takes memory) for each one. It also deletes its listener only after the engine has let go of the plug-in.

**Your work:**
- **A new song from a template keeps the template's recordings.** They stayed in the template's folder: the song couldn't find them after reopening, and a frozen track went silent.
- **Save As copies the recordings that undo can bring back.** Delete a clip, Save As somewhere new, undo: the clip now has its file.
- **Save As over a song whose folder already has a file of the same name** copies ours beside it under a new name (and the song points to it), instead of keeping the other song's file.
- **Duplicating a frozen track** makes a live copy (instrument and effects loaded, parts editable). The copy used to share the original's freeze file, so unfreezing it with "delete the file" silenced the original.
- **Parts can't be moved or pasted onto a frozen track** (they used to vanish), **or moved off one** (they used to be copied instead). Its parts are locked until you unfreeze it. Audio imported onto a frozen track goes onto a new track.
- **Undo no longer brings back the solo of a hidden or disabled track** (it silenced every other track).
- **Parts shorter than a sixteenth keep their length** when the song is saved and opened (they grew to a sixteenth).
- **Removing a send removes its automation and Quick Control,** so a send put in that slot later doesn't play the old send's lane.
- **"Match the file's tempo" works when the song follows its tempo track** (the file's tempo goes onto the tempo track).

**The screen:**
- **While a mouse button is held, only Space works.** A key pressed during a drag could delete or undo the part being dragged (a crash, or the wrong part changed), a MIDI editor drag undid the key's change on release, and Down during an Inspector fader drag moved the next track's fader.
- **The selected part is kept by itself, not by its place in the list.** After an undo, the arrangement could show one part selected while Delete acted on another.
- **Saving from a "Save the song?" box** (closing the studio, or opening a song from the Hub) now opens the file chooser after the box has gone. On Windows it used to close together with the box.
- **The track menu's instruments:** with more than 100 VST3 instruments, choosing the 101st or later changed the audio input instead.
- **The CPU meter's tooltip** no longer resets the peak the meter shows.
- **The Hub:** Rename and Delete are off for the song that's open. Rename refuses a name whose song or recordings folder exists (it could pick up a deleted song's recordings), and puts the song back if its recordings folder can't move.
- **The Tempo Track Editor:** the selection stays on its events when one before them is deleted or undone (it used to slide onto others). **Process Tempo's PROCESS** takes a value just typed (it used to ignore it unless you pressed Return first).

**Imports and keys:**
- **DAWproject:** a clip that contains itself used to crash the studio, and references nested in each other could hang it. A damaged .vstpreset could crash it too. Plug-ins are matched by name, then vendor, then kind (instrument or effect), so a same-named plug-in from another maker or of the other kind is no longer taken first. A file referenced outside the DAWproject must be an ordinary file of a sane size. A missing audio file's name is never given to a real one.
- **MIDI files whose first tempo comes after the start** play at MIDI's own 120 until then (they took the later tempo from the start).
- **Key commands:** a key on the comma (Ctrl+,) is kept when saved. Macros that call each other stop after 1000 steps. On a Mac, the Ctrl key is kept apart from Cmd (shown as MacCtrl). A key you took away from an arrangement command still works in the MIDI editor after a click there.

**Left as they are (honestly):**
- If the studio crashes from a stack overflow on Windows, the crash log may not be written. The handler itself runs short of room.
- Moving the playhead in the very instant after Stop can occasionally be undone by the last audio block.
- An effect that sends MIDI out has room for 4096 bytes of it per block before memory is taken on the audio thread.

## 0.13.0

**The Tempo Track Editor.** It's laid out as the manual describes Cubase's: a window of its own for the tempo track, opened with **Ctrl+T**. It's also in the tempo track's right-click menu and the transport tempo's, and in KEYS. Ctrl+T again closes it, and it reopens where you left it (the three screens included).

**The toolbar:**
- **ACTIVATE:** Activate Tempo Track (the song follows the curve, or its fixed tempo).
- **AUTO-SCROLL:** the view follows the playhead.
- **The tools:** SELECT (1), DRAW (8), ERASE (5) and ZOOM (6). A ZOOM click zooms in, an Alt-click zooms out.
- **RAMP / STEP / AUTO:** the Type of New Tempo Points.
- **CURRENT TEMPO:** the fixed tempo. As in the manual, you change it here only while the tempo track is off: double-click it and type.
- **SNAP:** the studio's own snap.
- **PROCESS TEMPO...**
- **INFO:** shows or hides the info line.

**The info line:** the selected event's **Position**, **Value** (BPM) and **Type** (Step or Ramp). Double-click Position or Value to type a new one. The first event's position can't move, as in the manual.

**The editor itself, as the manual describes it:**
- **The ruler:** click or drag to move the playhead.
- **The time signature display.**
- **The tempo scale:** BPM down the left.
- **The curve display:** edited just as on the tempo track in the arrangement.
  - Click the curve to add an event.
  - Select events (Shift-click, or drag a box round them) and drag them. Ctrl keeps a drag to one direction.
  - Draw and erase events with those tools.
  - **Delete** removes the selected events (never the first).
  - Double-click an event to type its tempo.
- **Zoom:** H and G, or Ctrl+wheel. The scroll bar or the wheel moves along the song.

**The tempo events as a list,** beside the curve: each event's position, BPM and type. Click to select (Ctrl/Shift for several). The manual puts this list in the tempo track's Inspector; here it's beside the curve, so everything about the tempo is in one window (our choice).

**Process Tempo** (the manual's dialog): set the left and right locators around a stretch of the song, then fit it to a new length or end time. The dialog shows:
- the bar range (from the locators);
- that range in seconds or minutes:seconds;
- **New End Position** and **New Length** (type either one);
- the **Equivalent Tempo Scaling**;
- **PROCESS**.

The tempo inside the range is scaled, ramps included, so it lasts exactly the new length. Before and after the range the tempo stays as it was. It's one undo step.

**Not yet:**
- **Time signature changes:** the signature display shows the song's one signature.
- **Tap tempo, tempo recording and tempo detection.**
- **Process Bars.**
- **The tempo event editor's smart controls** (tilt, compress, scale).

## 0.12.1

**Render in Place and Freeze no longer leave the studio stuck.** You found that Render in Place and Freeze could make PR7-S stop answering until you forced it to quit, while the log showed everything going normally.

**What was wrong:** the export always rendered on a thread of its own. Render in Place and Freeze rendered on the studio's screen thread, the one that runs the windows, so nothing else could happen while they ran. That isn't safe with big instruments like Kontakt. Rendering offline, they wait for their own background threads to load samples, and those threads can wait for the screen thread to answer. On the screen thread itself, that wait never ends.

**What changed:**
- Render in Place and Freeze now render on a thread of their own, as the export always did. The screen thread carries on meanwhile.
- The progress box takes the clicks and keys while they work, so nothing can change the song mid-render.
- Closing the studio waits until the render is done.
- The sound is the same: a test renders the same part both ways and checks every sample matches.

**Honest notes:**
- **A strong lead, not proof:** Kontakt's inner workings can't be seen from here. If it ever sticks again, send me the newest log in `%APPDATA%\Paragon PR7-S\Logs`. A **FROZEN** entry in it shows exactly what the screen thread was waiting for.
- **No cancel yet:** you can't cancel a Render in Place or Freeze partway. The export still has CANCEL.

## 0.12.0

**The Hub: a start window.** It works like the Hub you know, in the PR7 family's look: the pearl unit across the top with the PR7-S mark, and your songs below. It opens when the studio starts, and any time from the new **HUB** key in the header (or a key you give it in KEYS).

**Audio, at the top:** the audio driver (your AXE I/O's ASIO driver, for example), the stereo output pair, **DRIVER PANEL** (the driver's own panel) and **SETTINGS** for the rest.

**PROJECTS:**
- **+ CREATE EMPTY:** asks for a name, then makes a new song in a folder of its own at your project location, and saves it straight away. Its recordings go into that folder from the very first take.
- **The project location**, on the page:
  - **Use Default Location:** off, Create Empty just starts a new song, and saving asks where.
  - **The folder:** Documents\Paragon PR7-S\Projects at first; CHOOSE... picks another.
  - **Folder Name:** what a new song is called at first ("Untitled").
- **Your songs**, in sections (click a title to fold it):
  - **RECENT:** the songs you've opened or saved lately, newest first.
  - **TEMPLATES:** songs saved as starting points. Use **Save as Template...**, a command in KEYS under File.
  - **IN PROJECTS** (and each other folder you add): every song in that folder, including those in their own folders, newest first.

  Each song shows its name and when it was last changed.
- **Search** narrows every section as you type.
- **Opening:** double-click a song (or choose it and press **OPEN**) to open it. Double-click a template to start a new song from it. **CHOOSE FILE...** opens any song.
- **Right-click a song:** Open, Remove from the List, Show in Explorer, Rename... (its recordings folder is renamed with it), Delete... (to the Recycle Bin) and Clear the List.
- If the song you have open has unsaved changes, you're asked first (Save, Don't save or Cancel). Once a song is open, the Hub closes.

**HUB SETTINGS:** the project location and folder name, **user locations** (more folders whose songs the Hub lists: ADD A FOLDER..., REMOVE THE FOLDER), show the songs' file paths, show the project location on the projects page, and open the Hub when the studio starts.

**Not included:** the parts of the other Hub that link to its maker's website (deals, tutorials, account), the project previews, and the test tone.

**Also in this release: four more of your requests** (they were to be 0.11.1, which didn't go out: one of its new tests failed on the Mac build, the test's fault, not the studio's).

**A progress window for long jobs.** Render in Place and Freeze used to show nothing until they were finished. Now a box over the studio shows:
- what's being done, for example "Rendering KEYS (2 of 3)";
- a bar drawn as one of the studio's horizontal meters, lit orange to pink-red as it goes;
- the percentage.

Opening a song shows it too while its plug-ins load ("Loading Kontakt 8 (Keys)"), since big instruments take a while. The export window's bar is drawn as a meter now too, and so is any other progress bar in the studio (scanning plug-ins, for example).

While a render runs, Windows no longer greys the studio out as "Not Responding". The crash log doesn't mistake a long render for a freeze, either.

**The select box scrolls.** Drag the select box past the edge of the arrangement (left, right, above the tracks or below them) and the view keeps scrolling that way. The further past the edge, the faster it goes, and it carries on while you hold the mouse still. The box stays anchored where you started, so it can take in parts that were off the screen.

**SAVE lights up.** When the song has been saved (Ctrl+S, SAVE or SAVE AS), the SAVE key turns orange and reads SAVED for a moment. It lights once the song is actually on the disk, not when you press the key.

**Bigger inserts and sends in the mixer's own window.** In its window (Alt+F3) the insert and send slots are twice as tall, with names twice as big, and the channels are wider (140 instead of 100) so the names fit. Docked, the mixer is as before.

**Honest notes:**
- **Cancelling:** you can't cancel a Render in Place or Freeze partway yet. The export still has its CANCEL.
- **Part look:** parts keep today's look, as you chose.

## 0.11.0

**Key commands and macros: your own shortcuts, and your Stream Deck.** The last of the five requests: any action in the studio can now have a key, or several, and you can chain actions into macros. Everything is set in a new **KEYS** window (the key is in the header, beside PLUGINS), laid out as Cubase's Key Commands dialog.

**Using it with an Elgato Stream Deck**, as you do with Cubase:
1. In KEYS, choose a command (or a macro).
2. Click **CLICK HERE, THEN PRESS A KEY**, and press a combination nothing else uses, such as Ctrl+Alt+F13.
3. In the Stream Deck app, add a **Hotkey** action to a button, set to the same combination.

That button then runs the command. It works while a plug-in window is in front too, because those windows pass their keys to the studio.

**The KEYS window:**
- **The commands, by category:** Transport, File, Edit, Range, MIDI, Tool, Zoom, Project, Window, and your macros last. Each shows its keys, and a pink dot marks the ones you've changed. Actions that had no key before have a command now too: Play, Stop, Export, the imports, adding each kind of track, Add Tempo Track, Activate Tempo Track, Insert Tempo Event at Cursor, Plug-ins, Settings and KEYS itself.
- **Search** by name, or **PRESS KEY** and press a key to see what it does.
- **Filters:** All, Assigned, Customized or Unassigned.
- **For the chosen command:**
  - Its keys, each with **REMOVE**.
  - **CLICK HERE, THEN PRESS A KEY** adds another key, and the other keys stay. If another command had that key, it moves to this one and the window says which command lost it.
  - **RESET** puts its keys back to the defaults.
- **DEFAULTS** sets every command back to its defaults (your macros stay). **CLEAR ALL** takes every key away. Both ask first.
- **PRESETS:** save your set of keys and macros under a name, load one, or open the presets folder. Presets are XML files in Documents\Paragon PR7-S\Key Commands.
- **EXPORT LIST** writes a page listing every command and its keys, to print or keep next to your Stream Deck's layout.

**Macros:**
- **NEW**, **DUPLICATE**, **DELETE** and **RENAME** (a double-click renames too).
- **ADD COMMAND** puts the command chosen on the left into the macro, after the step chosen in the macro.
- **REMOVE**, **UP** and **DOWN** edit the steps.
- Choose a macro, then give it a key just like a command. Pressing that key runs its commands one after another. For example: Locators to Selection, Cycle, Click, Start.

**What stays the same:** every key you already use works exactly as before, including the keys that depend on where you last clicked (the MIDI editor, ranges, parts). A key you give to a command runs it first. A default key you take away does nothing until you give it to something else. Your keys and macros are kept between sessions.

**Honest notes:**
- **Not tried with a real Stream Deck.** It's built on the Stream Deck's Hotkey action pressing keys, which works with any program. I couldn't try it with a real one here, and which keys your Stream Deck app can send (F13 and above, for example) depends on its version. If a combination doesn't arrive, pick another.
- **Commands only:** macros run commands; they can't pause or wait between steps (Cubase's can't either).
- **Not in KEYS:** keys inside the MIDI editor (its tools and Q, for example) are still its own.

## 0.10.0

**The tempo track.** Songs can now change tempo as they go: a jump at a bar, or a gradual speed-up or slow-down, as Cubase's tempo track does it.

**Adding it:** right-click the track list, **Add Track > Tempo track** (there's one per song, as in Cubase), or right-click the transport's tempo, **Add the Tempo Track**. It appears under the master's row with:
- **ON (Activate Tempo Track):** on, the song follows the tempo track's curve; off, the song plays at its fixed tempo (the transport's), and the curve is greyed out with the fixed tempo as a dotted line. The transport's right-click has the same switch.
- **LOCK:** the tempo events can't be changed.
- **RAMP / STEP / AUTO, the Type of New Tempo Points:** **Ramp** changes gradually from the point before; **Step** jumps there; **Auto** takes the type of the point before. Click the key to go round them.
- **The tempo where the cursor is.** Double-click it to type a tempo: the event in effect at the cursor changes.

**Editing the curve** (in the arrangement, on the tempo track's row):
- **Select tool:** click the curve to add an event there (it snaps when SNAP is on, at the tempo you clicked). Click an event to select it, Shift-click to select more, or drag a rectangle round several. Drag selected events along the song and up or down; Ctrl keeps to one direction. The first event always stays at the song's start.
- **Draw tool:** click or drag to draw events.
- **Erase tool:** click an event to remove it.
- **Delete** removes the selected events. As in Cubase, the first one can't be removed.
- **Double-click an event** to type its tempo and choose Step or Ramp.
- **Right-click the row** for: Activate, Lock, the type of new points, **Insert Tempo Event at Cursor**, Select All Events, Remove Selected Events, the **visible tempo range** (60 to 180 BPM at first) and Remove Tempo Track.
- Every edit can be undone.

**Everything follows it:** notes, audio clips, the click, the cycle, recording (MIDI and audio), automation, renders, Render in Place and Freeze. Your plug-ins are told the tempo where the song is, so tempo-synced delays follow. The transport shows the tempo at the playhead (labelled TEMPO TRACK), and TIME is worked out through every change. A ramp's timing is exact, not a staircase of small steps. Audio clips keep their place in the bars and play at their own speed (nothing is stretched, as before).

**Imports bring tempo changes in:**
- **MIDI files:** a file's tempo changes go onto the tempo track. As a new song, always. Into the song you have open, when you choose **Match** (the question now also comes up when the file's tempo changes partway, even if it starts at the song's tempo): its changes go on from where it's dropped, and the song's tempo before that stays.
- **DAWproject** (songs from Cubase): tempo changes come across as the tempo track, ramps included. Clips timed in seconds land in the right place after a tempo change.

**Not yet** (say which you'd like next):
- The separate **Tempo Track Editor** window (Ctrl+T) with its info line.
- The tempo track's Inspector **event list**.
- **Process Tempo**, tempo recording, tap tempo and tempo detection.
- The tempo event editor's **smart controls** (tilt, compress, scale).
- **Time signature changes:** a signature track; songs keep one signature.
- **Musical-mode audio** that stretches with the tempo.

**Our choices** (the manual doesn't say):
- The tempo track sits under the master's row.
- Adding it turns it on, with one event at the song's start at its current tempo.
- New points are Ramp at first.
- Removing the track keeps its events, so adding it again brings them back.

## 0.9.14

Three of your requests. The tempo track and key commands with macros are bigger jobs and come next, each in a release of its own.

**The mixer's own window shows everything.** In its window (WINDOW, or Alt+F3), every channel now shows its inserts and, under a line marked SENDS, its eight sends, all the time. There's room for both, so INSERTS and SENDS leave the bar there. Docked under the arrangement it's still your choice, and it now starts with only the faders.

**MIDI files and the song's tempo.** When you import a MIDI file into the song you have open and its tempo isn't the song's, you're asked:
- **Match to (the file's tempo):** the song changes to the file's tempo, so the notes play at the speed they were written at. It's an undo step of its own. Audio already in the song doesn't stretch with it.
- **Keep (the song's tempo):** the notes go on the same beats, played at the song's speed (as before).
- **Cancel:** nothing is imported.

A file at the song's tempo isn't asked about, and importing as a new song always takes the file's tempo. The time signature stays the song's.

**A log you can read, for when PR7-S shuts down on its own.** PR7-S now writes a plain text file as it runs (Notepad opens it), in `%APPDATA%\Paragon PR7-S\Logs` (the newest 30 are kept). It records:
- **The set-up:** the version, your computer (Windows version, processor, memory) and your audio device (driver, sample rate, buffer size, latency, and every change).
- **What it did:** a line for each song opened, saved or imported, each plug-in loaded (name, maker, version, file) and given its settings, each plug-in window opened, PLUGINS, renders, freezes and RELOAD DRIVER. Each line goes onto the disk at once, so a crash right after it can't lose it.
- **A crash:** what went wrong (for example "access violation: it tried to read address 0x0, a pointer that was empty"), on which thread (the screen's or the audio's), in which file, and the chain of calls that led there. The file is the useful part: if it's a plug-in's `.vst3`, that plug-in crashed. The studio's own code is named down to the source file and line, from `Paragon PR7-S.pdb`, which now comes in the Windows zip: keep it beside the .exe.
- **A freeze:** if the screen stops answering for 10 seconds, a FROZEN entry records what it was doing, and later how long until it answered again. A freeze that you end in Task Manager still leaves this behind.

The next time PR7-S starts after a crash, a freeze that was ended, or the computer going off, it says so and offers **Show the log**. Send me the file and I can find the cause.

**Honest notes:**
- **Not tried on Windows yet.** The crash entry's Windows part (the calls and their names) can't be tried here: it's built and checked by the Windows build, but a real crash on your computer is its first real test.
- **Hidden crashes.** A plug-in that catches crashes itself (a few do) can keep one from reaching the log. The lines before it still show what was going on.
- **macOS and Linux** write the log too, with fewer names in a crash entry.
- **Size:** the Windows zip is larger by the .pdb.

## 0.9.13

**Import DAWproject: your Cubase songs in PR7-S.** Cubase can't hand its .cpr files to other programs, but since version 14 it exports **DAWproject**, an open format made for moving songs between studios.

**How:**
1. In Cubase: **File > Export > DAWproject**, and save the file.
2. In PR7-S: **IMPORT > DAWproject as a new song...**, or drop the `.dawproject` file on the studio.

It opens as a new song, not saved yet. If the song you have open has unsaved changes, you're asked first. A box then lists anything that didn't come across.

**What comes across:**
- **Song settings:** the tempo and time signature.
- **Tracks:** instrument, audio, FX and group tracks, with their names. Colours become the nearest of the family's three.
- **Mix:** volume, pan, mute and solo. Outputs to groups, and sends to FX channels (pre or post, level, on or off).
- **Plug-ins:** your VST3 instruments (Kontakt and so on) and insert effects, with their settings, as long as they're installed and found by PLUGINS > SCAN. A plug-in that was switched off comes across bypassed.
- **Parts:** MIDI parts with their notes (looped parts are written out in full), and audio clips with their start point in the file and their fades. The audio files are copied into the song's audio folder.
- **The master:** its volume and its effects.
- **Automation:** volume, pan, mute, sends and plug-in knobs.

**What doesn't come across yet** (the box lists it for each song):
- **Tempo and time signature changes:** the song keeps the first tempo and signature.
- **Cubase's built-in channel strip** (its EQ and dynamics): they aren't VST3 plug-ins, so PR7-S has nothing to load for them.
- **Other plug-in kinds:** VST2, AU and CLAP plug-ins, and MIDI effects.
- **Folders:** their tracks come across, flattened. A folder that's a group channel becomes a group.
- **Markers, note expressions, MIDI controller automation, the clip launcher.**
- **Time-stretched audio** plays at its own speed, so it may not line up.
- **A missing plug-in:** a plug-in that isn't on this computer keeps its place and its settings, but stays silent until it's installed and scanned.

**Honest note:** this was built from the format's published specification and tested with DAWproject files made to it, including the shapes the format's own examples use. I don't have Cubase here to export a real file, so please try one of your songs. If something comes across wrong, send me the `.dawproject` (or tell me what's off) and I'll fix it.

## 0.9.12

Your six tweaks.

**Plug-in windows stay open.** Clicking the studio didn't close an instrument's window: it went behind the studio's window, which spans your three screens, so it looked closed.
- Plug-in windows now float above the studio while it's the program in front, and stay until you close them, as Cubase's do.
- When you switch to another program, they stop floating, so they don't sit over your browser.
- The studio's keys (Space, R, F2...) work while a plug-in window is in front, unless the plug-in uses that key itself.

**The master at the very top.** The MASTER row is the first row, above every track, with its automation lanes under it.
- **Click it to show the master in the Inspector:** its R and W, its volume (type a value as on a track), its 16 insert slots and its fader with the output level.
- MIDI, routing, sends and Quick Controls don't apply to the master, so they're not shown.
- Clicking any track goes back to that track.

**Only the gradient's three colours.** Track colours are exactly the gradient's red `#D42E2A`, pink `#DE0639` and orange `#CD6619`, in turn. Songs saved earlier open with each track's colour moved to the nearest of the three.
- An honest note: the gradient's pink is a deep crimson-pink, so next to its red the two look close. If you'd like a lighter pink to tell them apart, say so (it would then not be exactly the gradient's).

**MIDI notes coloured by velocity.**
- **Orange:** velocities below MID's range (below 70 by default), which covers the LOW range and anything under it.
- **Pink:** MID's range (70 to 99).
- **Red:** HIGH's range (100 and up).
- The velocity bars use the same colours. If you change the ranges (RANGES), the colours follow.

**The play bar** is pearl white with a pink glow, in the arrangement and in the MIDI editor. It's still red while recording.

**The track's right-click menu is in alphabetical order** (A to Z, no dividers). An audio track's inputs moved into a **Record From** submenu, so they stay together.

## 0.9.11

Your seven tweaks.

**PR7-K is the studio's synth.** The Paragon Synth is gone from the menus; new tracks play PR7-K.
- **How it gets there:** PR7-K comes in the download: `Paragon PR7-K.vst3`, beside the app. Keep them in the same folder. If you've installed PR7-K in your VST3 folder too, the newer of the two is used, so updating PR7-K updates the studio.
- **Old songs:** "built-in synth" tracks open on PR7-K. They'll sound different (PR7-K's own patch) until you pick a PR7-K sound.
- **If PR7-K can't be found,** the old synth stands in, so a track never comes up silent. The add-track menu says so ("PR7-K not found").
- **Why not built into the program itself:** PR7-K is a whole plugin with its own screens, presets and releases, and it's still changing. Shipping its VST3 keeps the studio on the latest PR7-K without the two drifting apart.

**The cycle bar** has the keys' pink-red-orange gradient instead of a flat orange.

**SPLIT's ghost lines are gone.** The cut line is drawn on the grid, but the screen cleaned up only round the mouse pointer. Since 0.9.10 the grid is the quantize value, so the line was often further from the pointer than that, and the old lines stayed behind. Now the cleaning follows the line.

**Alt+F2 / Alt+F3: across all three screens.** The MIDI editor's and the mixer's own window now spans all the screens, as SPAN ALL SCREENS does the studio.

**SAVE no longer stops the screen.** Saving asks every plugin for its settings, then writes the song.
- Writing turned out to be the slow part: about 19 ms per megabyte of plugin settings, and Kontakt's can be large. Your second-long hang was that.
- Now the writing happens on a thread of its own and the studio carries on. The song is written to a temporary file and swapped in at the end, so it's never half-saved.
- **Still on the screen's time:** each plugin packing up its settings, which VST3 requires. Kontakt takes what it takes there.

**Quieter parts.** A part's body is about 15% as bright as before, with a thin edge in the track's colour. The title strip, the notes and the waveforms are as bright as they were, so they stand out.

**No + ADD TRACK key.** Right-click a track for **Add Track** at the foot of its menu (after Hide Track), or right-click the empty space under the tracks. Double-click there still adds one like the selected track.

## 0.9.10

Your six tweaks.

**Question boxes in the family's look.** The boxes that ask before something happens (Delete track, New song, and so on) are black pearl with the family's red glow round the edge. The text is white Bebas, and the PR7-S mark takes the place of the round question mark.

**Two main screens: POSITION and TIME.** The middle screen is split into two identical screens: bars and beats on the left, minutes and seconds on the right. Both are in Bebas, larger than the other screens since they're the main values. Each figure has a cell of its own width, so the numbers don't jiggle as they count.

**F1, F2, F3.** The INSPECTOR, KEY EDITOR and MIXER keys are gone from the header.
- **F1:** the Inspector.
- **F2:** the MIDI editor (the key editor's new name), opened or closed where it was last.
- **F3:** the mixer, the same way.
- **Alt+F2 / Alt+F3:** move the MIDI editor or the mixer between docked and its own window. The window fills the screen the studio is on (the middle one of your three).
- **Ctrl+E:** still opens the MIDI editor.
- **Changed:** F3 used to open the mixer's window and Alt+F3 the docked mixer (the manual's keys). They now follow your scheme.

**SNAP and the quantize value, on the header** (as the manual's Snap and Quantize Presets on its toolbar).
- **SNAP (J):** when lit, parts, ranges, the playhead and automation points snap to the quantize value. Off, nothing snaps.
- **The value key (1/16 ▾):** 1/1, 1/2, 1/4, 1/8, 1/16, 1/32 or 1/64. Both are saved with the song.
- **Changed:** the arrangement's grid used to follow the zoom (beats when zoomed in, bars when out). It's now the quantize value, like the manual's "Use Quantize" grid type. Ctrl while dragging still turns snapping off for that drag.
- The MIDI editor keeps its own grid, as the manual's editor has its own.

**The MIDI editor's panel: half as wide, one column.**
- Every key is the same size, in one line: WINDOW, CLOSE, SELECT, DRAW, SPLIT, ERASE, the grid, QUANTIZE, AUTO-SCROLL, SCROLL MODE, the size, LOW, MID, HIGH, RANGES. The selected notes' details are at the foot.
- When the editor is shorter than the column (docked at 200%), the column scrolls with the mouse wheel.

**Velocity handles** (the manual's Controller Event Editor). Select two or more notes and a box appears round their velocity bars, with five handles:
- **The top edge:** drag up or down to move them all together.
- **The middle of the top edge:** scale them, so the loud ones change more than the quiet ones.
- **A top corner:** tilt that end, for a crescendo or a fade. The other end stays.
- **The middle of the right edge:** squash them towards their middle, or spread them out.
- Each drag is one undo step. Velocities stop at 127 and at 1.
- **Not done:** the manual's Alt variants (compress left/right, scale around the relative centre) and Stretch (that one moves notes in time).

## 0.9.9

Fixes for the sticking, slow renders and silent tracks you reported, plus two changes you asked for.

**Clicks in the ruler and the cycle key stuck for about a second.** Two causes, both fixed:
- **The main one:** after every change (moving the playhead, the cycle, any edit), the Inspector asked the selected instrument whether it has a window of its own, to light its EDIT key. For a VST3, the only way to answer is for the plugin to build its editor's view, and Kontakt is slow at that. Now each plugin is asked once and the answer is remembered. My tests use the built-in synth, which answers at once, so they never showed this.
- **The second:** the screen read every track's level and every effect slot through the same lock the audio thread holds while it plays a block. With your 2048-sample buffer and heavy plugins, each read could wait for the block to finish. The screen now reads them without the lock. That's safe because those values are made to be read while the audio plays. A test with a heavy plugin playing on its own thread: a click took 10.6 ms before, 0.2 ms now.

**A render hung before it started.** Before rendering, plugins are switched to offline mode, which needs each one set up again (0.8.6's fix for Kontakt rendering silence). It was doing that to **every** plugin in the song, then setting up **everything** again, twice per render and for every stretch of a Render in Place. Now:
- Only the plugins that will be heard in the render are switched: the tracks being rendered, the FX and group channels they reach, and the master's.
- Only the plugins that switched are set up again.
- Render in Place and freezing several tracks switch the plugins once for the whole batch, not once per stretch.

**A track not soloed acted as if muted.** A soloed track silences every track that isn't soloed, which is what solo does. The bug was solos you couldn't see:
- When you rendered a soloed track in place and the original was hidden or disabled, it stayed soloed, out of sight, and silenced everything, including the new rendered track.
- **Now:** hiding or disabling a track clears its solo, and a disabled track's solo doesn't count (for songs saved before this). When Render in Place mutes, hides, disables or removes a soloed source, the solo moves to the rendered track.
- **New, from the manual:** **M** and **S** keys in the track list's header (Deactivate All Mute States / Deactivate All Solo States). They light while any track is muted or soloed, hidden ones included. Click to turn them all off.
- **Ctrl-click S** in the Inspector or the mixer: exclusive solo, which solos that track and turns every other solo off (the manual's).

**The track rows: no M, S, REC, R, W** (you asked; they're in the Inspector and the mixer).
- The row keeps its name, level, volume knob and automation arrow, plus MON on audio tracks (it wasn't on your list).
- Under the name, the row says what's on: SOLO, MUTED, REC, READ or WRITE, FROZEN.
- The MASTER row keeps its R and W, since the master has no Inspector.

**Typing a value:** double-click a fader or a pan knob, type, and press Enter.
- **Where:** in the mixer (the master's fader too), in the Inspector, and the track's volume knob. Also on the numbers: the one under each mixer fader and pan, and the Inspector's VOL and PAN readouts.
- **Volume:** -6, +3 dB, -12.5, or -inf.
- **Pan:** L30, R15 or C, or a number (-30 is left, 30 is right).
- A value it can't read shows "?" so you can try again. Esc or a click elsewhere leaves it as it was. Each typed value is one undo step.
- **Changed:** double-click no longer resets to 0 dB or the centre. **Ctrl-click** does, as before.

Not checked here: your machine. I found the slow plugin question by reading JUCE's code, not by running Kontakt (there are no real plugins in the cloud). So please check that clicks in the ruler and the cycle key no longer stick, and how long a Render in Place takes to start.

## 0.9.8

Freeze and track presets (the two extras you chose for the Inspector).

**Freeze** (as the manual's Freeze Channel): it turns a heavy instrument track into a plain audio file, so the computer doesn't have to run the instrument and its effects any more.
- **How:** the Inspector's new **FREEZE** key, or right-click the track > **Freeze Track** (with several tracks selected: **Freeze Selected Tracks**). You choose the **tail**: none, 2, 5, 10 or 30 seconds of sound left ringing after the last part (a reverb's or a long release's).
- **What happens:**
  - The instrument and its insert effects are rendered from the song's start to the last part's end (plus the tail), into a **Freeze** folder beside the song's recordings.
  - Then the instrument and the effects are unloaded, freeing their memory and CPU. Their settings are kept.
  - The track plays the file instead. The fader, pan, mute, solo and sends still work live, and so does their automation.
- **While frozen:**
  - FREEZE is lit, and the track says FROZEN.
  - Its parts are veiled and stamped FROZEN. They can't be edited: any change to them is put back.
  - The instrument and the effects can't be changed, and the track can't be armed.
- **Unfreeze:** click FREEZE again (or right-click > **Unfreeze Track**). You choose to **keep** or **delete** the freeze file, as the manual has it. The instrument and effects load again with their settings.
- Saved with the song: a frozen track opens frozen, without loading its plugins. Save As copies the freeze files to the new place.
- Audio tracks can be frozen too (their effects rendered in).

**Track presets** (as the manual's Track Presets): a track's sound saved to a file and used again.
- **What's in one:**
  - The instrument and every insert effect, with their settings as they are now (a Kontakt patch, an amp's knobs).
  - The fader and pan, the colour and the Quick Controls.
  - The MIDI channel, or an audio track's input.
- **What isn't in one:** the parts, the automation, the sends and the output. Those belong to the song.
- **Save:** right-click a track > **Save Track Preset...**, or the Inspector's **PRESET** key. Presets go in Documents/Paragon PR7-S/Track Presets (`.pr7track` files).
- **Load:** right-click > **Load Track Preset...** (or PRESET). It replaces the track's instrument, effects and mix, and keeps its parts and name (as the manual has it).
  - It only goes onto a track of the same kind: an instrument preset onto an instrument track, an audio one onto an audio track.
  - It isn't an undo step, as in the manual.
- **New track:** + ADD TRACK > **Track from a preset...**. The new track goes under the selected one, with the preset's name.

**Also fixed:** after a render (Export, Render in Place, Freeze), the first playback no longer glides from the render's fader levels to the song's.

**Our choices, where the manual differs or says nothing:**
- Freeze always takes the inserts in and always unloads the instrument. The manual makes both of these options.
- Freeze renders all the effects before the live fader, including those after the fader.
- Change the tempo after freezing and the frozen file won't follow it: unfreeze and freeze again.
- No multi-track presets and no MediaBay browser: a file chooser instead.

Not checked here: how much CPU freezing saves with your own plugins (Kontakt, SD3), so please freeze one of them and watch the CPU screen.

## 0.9.7

Importing into a song that already has something in it: "This song" did nothing.

- **Why:** IMPORT asked a question ("New song" or "This song"), and the file chooser opened from inside that question's answer. On Windows the chooser opened as the question box closed and went with it, so nothing happened.
- **Now:** there's no question box. The IMPORT menu has every choice in it, and the file chooser opens after the menu has gone:
  - **MIDI files into this song...**: each track of each file becomes a new track, starting at bar 1.
  - **MIDI files as a new song...**: the first file's tempo and time signature.
  - **Audio files, a track each...**: each file on its own track, all starting at the playhead.
  - **Audio files, one after another on one track...**: on the selected audio track (else a new one), from the playhead.
- Several audio files dropped on the arrangement give a small menu where they're dropped: **A track each** or **One after another on one track**.
- Not checked here: Windows' own file chooser (there's no Windows in the cloud), so please try IMPORT > MIDI files into this song twice in a row.

## 0.9.6

Quick Controls: 8 per track, in the Inspector (as the manual's Track Quick Controls).

- **A new Inspector section, QUICK CONTROLS**, at the bottom: 8 slots per track, QC 1 to QC 8.
- **Click a slot's name** to choose what it controls: any of the track's controls, the same list the automation lanes offer.
  - The track's volume, pan and mute, and its sends.
  - Every knob of its instrument (Kontakt's, for example).
  - Every knob of each of its effects.
  - **None** empties the slot.
- **Drag its bar** to move that control, the same as moving it in the mixer or in the plugin's own window. Shift-drag moves it finely. The bar follows the control when the automation or the plugin's window moves it.
- **Automation:** with the track's **W** on while playing, moving a Quick Control writes automation, as moving the control itself does. R plays it back.
- **Right-click the QUICK CONTROLS title** for:
  - **Show Automated QC Assignments** (the manual's): opens the automation lanes of the slots' controls that have automation.
  - **The instrument's first 8 knobs**: fills the slots with them.
  - **Clear all**.
- Saved with the song. Choosing a slot's control is an undo step. Removing an effect empties the slots that controlled it.
- **Different from the manual:**
  - It fills an instrument track's slots with the instrument's main parameters automatically. Here that's a menu item, because many instruments' first knobs aren't their main ones.
  - Assigning from a plugin's own window (right-click a knob) isn't possible here: that menu is the plugin's.
  - Remote controllers and QC presets aren't here yet.

## 0.9.5

Importing several files (you could only import one MIDI file at a time).

- **Why:** IMPORT > MIDI file's file chooser only let you pick **one** file. And several audio files always went onto one track, one after another, never onto separate tracks.
- **MIDI files:** IMPORT > MIDI file now lets you pick as many as you like (Ctrl-click or Shift-click in the file chooser). Each track in each file becomes a new track, as before.
  - Into a new song or this one: asked once, as before. For a new song, the first file sets its tempo and signature, and the rest go into it.
  - Dropping several .mid files on the arrangement imports them all, at the bar they're dropped on.
- **Audio files:** with several picked (or dropped), PR7-S asks, as the manual's import options do (Import Files on One Track):
  - **A track each:** every file on a track of its own, named after the file, all starting at the playhead (or the bar dropped on).
  - **One track:** one after another, as before.
  - One file goes on the selected audio track at the playhead (else a new track), as the manual has it.
- The manual's File > Import > MIDI File takes one file per import. Picking several at once is our addition, because you asked for it.
- Not checked here: Windows' own file chooser. The tests import five MIDI files in a row and three audio files a track each.

## 0.9.4

Random velocities in the key editor (your idea): three ranges, three keys.

- **Select some notes, then press:**
  - **Alt+B (LOW)**, **Alt+N (MID)** or **Alt+M (HIGH)**.
  - Or click LOW, MID or HIGH in the key editor's panel.
- Each selected note gets its own velocity, picked at random within that range.
- **The ranges start at** 40-70, 70-100 and 100-127. Each key shows its range.
- **Changing a range:** click the arrow beside HIGH. Each range has a slider with two handles (low and high, 1 to 127). PR7-S remembers your ranges for every song.
- **With no notes selected, nothing changes** (as you chose).
- The notes stay selected, so pressing the key again gives a new roll. Each roll is one undo step.
- Not in the manual: this is our design, from your description.

## 0.9.3

The key editor: twice the size, a pencil, a split tool, and whole and half note grids (as you asked).

- **Everything in the key editor is twice the size:** the text, the piano keys, the notes, the velocities, the ruler and the buttons.
  - A new **SIZE** box in the editor (100%, 150%, 200%) changes it; it starts at 200% and PR7-S remembers your choice.
  - **The trade-off:** docked, it shows fewer notes at once. Zoom out with G (Shift+G for shorter rows), give it more room by dragging the line above it, use its own WINDOW, or choose 150%.
- **DRAW shows a pencil** as the pointer instead of crosshairs. SPLIT shows scissors, as the arrangement's tools do.
- **SPLIT (3)** in the key editor, as the manual has it:
  - Click a note to cut it in two there, on the grid (hold Ctrl to cut anywhere).
  - If it's one of several selected notes, all the selected ones that cross that point are cut.
  - Each cut is one undo step.
- **GRID** now has **1/1** (whole note) and **1/2** (half note) as well as 1/4 down to 1/32. QUANTIZE uses whatever grid is chosen.

## 0.9.2

The lag fixed, several parts selected at once, and your Inspector and track list tweaks.

- **Fixed: everything lagged by seconds** (editing notes, moving the playhead, clicking anything).
  - **Why:** the arrangement redrew itself whole 30 times a second, all the time, even with nothing moving. So did the key editor. On a window across three screens, with a song full of parts, that kept the main thread (the part of the program that answers your mouse and keys) busy almost all the time, so every click waited in a queue.
  - **Now:** only the thin strip where the playhead was and where it is goes, when it moves; nothing when it doesn't. Redrawing also skips the parts and notes outside that strip.
  - **Measured here** on a 12-track song (40 parts a track) at three screens' width: 18.5 ms per frame before, 0.18 ms after, 30 times a second.
  - **Also:** the transport's position and CPU screens redraw only when what they show changes. The Inspector no longer asks Windows for the MIDI devices on every change (it was doing so on every playhead move); it asks when you open MIDI IN.
  - Not measured here: your computer (Windows, your song). Tell me if anything still lags, and what you were doing.
- **Select several parts** (as the manual's Object Selection tool):
  - Drag over empty space to draw a rectangle: every part it touches is selected. With Shift, they're added to the selection.
  - **Shift-click** adds a part or takes it away. **Ctrl+A** selects all.
  - **Drag one and they all move together**, keeping their places relative to each other, across tracks too. Alt-drag copies them; a part can only go to a track of the same kind, as the manual has it.
  - **Delete** deletes them all. **Ctrl+D** duplicates them as one block straight after.
  - **Ctrl+X / Ctrl+C** cut or copy them. **Ctrl+V** pastes at the playhead, from the selected track down. **Alt+V** pastes where they came from (the manual's Paste at Origin).
  - The right-click menu (Duplicate, Cut, Copy, Paste, Delete, Mute, Render in Place) works on all of them too.
  - Each of these is one undo step.
- **The Inspector:**
  - Drag its right edge to make it wider or narrower; PR7-S remembers the width.
  - **INSERTS shows 5 slots**; scroll for the other 11.
- **The track list:** the track's name is 1.7 times bigger, and the instrument's name is gone (it's in the Inspector). Double-click the name to rename it, as before. Right-click a track for its menu (instrument, Output, disable, hide...).
- **Not yet:** Quick Controls (next).

## 0.9.1

Disable and hide tracks, the Visibility tab, and Render in Place that disables and hides the original (as you asked, from the Cubase 15 manual).

- **Disable a track** (right-click it, or its instrument key: **Disable Track**, or **Disable Selected Tracks** for several):
  - It goes silent, and its instrument and effects are unloaded, so it uses no CPU and no memory.
  - Their settings are kept.
  - In the track list and arrangement it turns grey and its key says **DISABLED**. Its mixer strip goes away (as the manual has it), and it can't be armed.
- **Enable it again** with **Enable Track** in the same menu, or the **ENABLE TRACK** key in its Inspector. Its instrument and effects load again with their settings.
- Audio and instrument tracks can be disabled. FX and group channels can't, as in the manual.
- **Hide a track** (right-click: **Hide Track** / **Hide Selected Tracks**, or the Visibility tab):
  - It leaves the track list, the arrangement and the mixer, but it's still heard unless it's disabled.
  - Up and Down skip it, and pastes and ranges go past it.
- **The left zone's tabs: INSPECTOR | VISIBILITY.**
  - VISIBILITY lists every track with a box: ticked means shown, unticked means hidden.
  - Disabled tracks are marked DISABLED. **SHOW ALL** brings every hidden track back.
  - PR7-S remembers which tab was open.
- **Render in Place**, as the manual's Render dialog has it, for whole tracks:
  - **The original:** a new choice, **Disable the track**.
  - A new option, **Hide the track afterwards**.
  - So: render, then the original is disabled and hidden, and only the render shows. To change it: VISIBILITY, tick it, Enable Track, edit it, render again.
- Disabled and hidden are saved with the song, but they aren't undo steps.
- **Our choices:**
  - Hidden tracks also leave the mixer. The manual makes that a setting (Sync Visibility of Project and MixConsole); here it's always on.
  - A copy of a disabled track is enabled and shown.
- **Quick Controls:** 0.9.2 (after these).

## 0.9.0

The Inspector: the selected track's settings, left of the track list (as you asked, after the left zone in Cubase 15).

- **Where:** a panel on the left, the whole height of the window (beside the mixer too, when it's docked). It shows the track you last clicked.
- **Show or hide it:** the header's new **INSPECTOR** key, or **Ctrl+Alt+L** (the manual's Show/Hide Left Zone). PR7-S remembers whether it was open.
- **Sections:** click a section's title to fold it; folded sections stay folded next time.
  - **The track** (its name, in its colour): M, S, R, W and REC; MON on an audio track. Volume and pan (Ctrl-click or double-click resets them; Shift-drag for fine moves). The instrument: click it for the same menu as in the track list (open its window, change it, Output, delete); **EDIT** opens its window straight away.
  - **ROUTING:** for an instrument track, **MIDI IN** and **CHANNEL** (new, below); for an audio track, its **INPUT** (IN 1, IN 1+2...). **OUT:** the master or a group channel.
  - **INSERTS:** the track's 16 effect slots, the same as the mixer's; those below the orange line come after the fader.
  - **SENDS:** its 8 sends, the same as the mixer's (FX channels have none).
  - **FADER:** its mixer strip, with fader, pan, M S R W and its level meter.
- **New: MIDI IN.** Choose **ALL MIDI INPUTS** (as before) or one keyboard. Only that keyboard then plays, and records, when the track is armed or selected. A keyboard the song asks for but that isn't plugged in stays listed, marked NOT CONNECTED.
- **New: CHANNEL.** This is the MIDI channel (1 to 16) the track's notes go to its instrument on, for an instrument that listens on one channel only, such as each part of a Kontakt multi. What you play live goes out on that channel too. It's 1 unless you change it, as before.
  - In my question I described it as a filter on what comes in. Cubase's Routing channel is what the instrument receives, so I built it that way.
- Both are saved with the song, and Undo covers them.
- **Not in this version:**
  - Quick Controls (0.9.1).
  - Track presets and Freeze (0.9.2).
  - The Equalizer (later, as you said).
  - Track Versions and MIDI Inserts (you crossed them out).
- Not checked here: real MIDI keyboards (there are none in the cloud). The tests drive the filter with device names.

## 0.8.6

Render in Place (and EXPORT) with Kontakt, Superior Drummer and other plug-ins.

- **Fixed: plug-in instruments rendered silence.** A render runs faster than real time. PR7-S tells every plug-in so ("offline"), so it can take its time, for example to load samples from disk.
  - A plug-in reads that only when it's set up, and PR7-S didn't set them up again. So Kontakt and Superior Drummer were told "offline" while still set up for real time, and they played nothing.
  - Paragon Synth doesn't care about the difference, which is why its tracks rendered fine.
  - Now each plug-in is set up again for the render, and again for playing afterwards.
- Not checked here: Kontakt and Superior Drummer themselves (they aren't in the cloud). A test instrument that behaves the way they did (silent unless set up for the mode it's told) renders silence without the fix and sound with it.

## 0.8.5

The buffer size: how to get 2048 samples with your interface.

- **Why the buffer was stuck small:** a program can only offer the buffer sizes the driver gives it.
  - **Windows Audio** (what PR7-S chose the first time) runs at the one size Windows sets, about 10 ms (480 samples), and it can't be changed.
  - An **ASIO** driver gives its own list. Many, the AXE I/O's among them, give only the size set in their own control panel.
- **ASIO the first time:** on a first start, PR7-S now picks **ASIO** when an ASIO driver is installed. If you already chose a driver, it stays: pick ASIO in SETTINGS > Audio device type.
- **SETTINGS says what the buffer is and how to raise it:**
  - The size now, in samples and milliseconds, and what your driver type allows.
  - For ASIO: **CONTROL PANEL** opens the driver's own panel. Choose 2048 there and close it.
  - **RELOAD DRIVER** (new) opens the device again, so the new size is read even if the driver doesn't say it changed.
  - Changing the size in the interface's own app while PR7-S is open may not work, because the driver is in use.
- **SETTINGS' device choices scroll** inside the page, so the CONTROL PANEL key at their foot can't end up under the FINISH row with many inputs on.
- Not checked here: a real AXE I/O on Windows (there's no Windows or audio interface in the cloud). The tests use a stand-in ASIO driver that reports only its panel's size, as the AXE I/O's is said to.

ASIO is a trademark and software of Steinberg Media Technologies GmbH.

## 0.8.4

Render in Place, tool icons, and the transport rearranged as you asked.

- **Render in Place** (as the Cubase 15 manual's Rendering Audio and MIDI): right-click a part, a range or a track for **Render in Place...** (or **as last time**, without the page). What's chosen becomes audio on a **new audio track straight under its track**, the clips where the parts were (the files in the song's audio folder).
  - **Clips:** a clip for each part, parts end to end joined into blocks, or one clip over them all.
  - **What's in it:** **Dry** (the instrument alone; the new track gets its effects, fader, pan and sends), **Channel settings** (through its effects and sends; the new track gets the fader and pan), **Complete signal path** (fader and pan too), or that **plus the master's** effects and fader.
  - **Tail:** none, 1 to 4 bars or 1 to 10 seconds, so reverbs and delays ring out (it stops early once silent). **Bits:** 16, 24 or 32-bit float.
  - **The original:** kept, muted (the parts rendered, or the track), or removed (a track rendered whole). Your choices are kept for next time.
- **Fixed: an export started at the levels as set.** If a fader (or a send, or the master) had just moved, the first moments of an exported file still glided from where it was. Now everything is at its setting from the first sample.
- **Tool icons:** each tool's key has its picture (an arrow, a range, scissors, glue, an eraser, a magnifier, an X, a pencil, a speaker), and over the tracks the **mouse pointer becomes the tool's icon**, so you always see which tool you're using.
- **Keys that are on turn black** on the black and neon finishes (they used to keep their red and light pale gold, which was hard to see). On the white finish the keys are black already: a lit one has orange lettering.
- **The transport:** **AUTO-SCROLL** (and its arrow) moved onto it, next to HOME, with F stamped under it. The **CPU meter** is now a small screen like TEMPO, between CLICK and the position screen: the average load in big figures, the peak under it, **OVER** lit red after a dropout (click to reset).
- **+ ADD TRACK** moved to the top of the track list.
- **Ctrl-click / Alt-click anywhere in the ruler** set the left / right locator (before, only the ruler's top strip did; lower down, the playhead moved).
- **Spanning your screens** now opens the **mixer docked, as short as it goes, its inserts and sends hidden**. Change it as you like; it's only set as the window goes wide.
- **Drop-down lists, menus and text boxes** (the export page, right-click menus) are in the family's colours now (they were a blue-grey).
- **Not there yet** (from Render in Place): disabling or hiding the source tracks, a folder of your choice for the files, and mixing several tracks down to one file.

## 0.8.3

Audio file import, a CPU meter, and a layout made for a window spanning three screens.

- **Import audio files:** **IMPORT** now asks **MIDI file** or **Audio files**. Choose one or more **WAV, AIFF, FLAC, Ogg or MP3** files: they go in at the playhead on the selected audio track (or a new one, stereo if the file is), one after another. Or **drag them from Explorer** onto the arrangement: onto an audio track, at the bar you drop them on (anywhere else: a new audio track).
  - Each file is **copied into the song's audio folder** ("<song> Audio", as your recordings are), so the song keeps working if the original moves. FLAC, Ogg and MP3 are written there as WAV files.
  - A file at another sample rate than your interface's plays at the right speed (converted as it plays, as recordings are).
  - **Not there yet:** imported audio doesn't follow tempo changes (no time-stretching), and a stereo file isn't split into two mono tracks. WMA isn't read.
- **CPU meter** (in the transport, beside OUT, as the manual's Audio Performance meter): how much of each audio block's time the processing takes. The left column is the **average**, the right one the **peak**; at the top, all the time there is. The light above them turns **red for an overload** (a block that took longer than it lasts: a dropout, a click or gap in the sound) and stays lit until you click the meter. Hover over it for the numbers. If it goes red: a bigger buffer in SETTINGS, or fewer effects and instruments playing at once.
- **Spanning three screens:** from 2400 pixels wide (SPAN ALL SCREENS), the window lays itself out for it:
  - The **position screen** stops stretching (540 pixels), with the **transport keys** on its left and the **tempo, signature and the TOOLS** on its right (the tools row under the transport goes, giving the tracks its room). They sit together in the middle, so on three 1920-pixel screens they're all on the middle one, none cut by a screen's edge.
  - The **header's keys** (NEW to R/W) are in the middle too. The PR7-S mark stays on the left, PLUGINS and SETTINGS on the right, OUT and CPU at the right end of the transport.
  - On one screen it's as before.

## 0.8.2

Moving tracks around, setting the cycle with the Cubase 15 manual's clicks and keys, and the studio's window across all your screens.

- **The cycle (the left and right locators):**
  - **Ctrl-click** in the top strip of the ruler sets the left locator there; **Alt-click** sets the right one; **Ctrl+Alt-click** sets both round the grid step you click in. (Past each other, they swap.)
  - **Each locator has a handle** (a small white flag at each end of the cycle): drag one to move just that end. Dragging inside the cycle still moves the whole thing.
  - **P** sets the locators round the selected part or range; **Alt+P** does that and plays it round and round.
  - **Number pad 1 / 2** go to the left / right locator; **Ctrl + number pad 1 / 2** set it to the playhead.
- **Tracks:**
  - **Drag a track up or down** in the track list (by its name or an empty spot): an orange line shows where it will go. The mixer's strips follow. **Ctrl+Z** puts the order back.
  - **Select several:** Ctrl-click adds or removes one, Shift-click selects a run (Shift-click a selected one to drop it). Dragging one of them moves them all, keeping their order.
  - **Up / Down arrows** step through the tracks (and, if a part was selected, select the first part on the track).
  - **The track's right-click menu:** Duplicate track (straight under it, with its parts, instrument, effects and their settings, sends, automation and mix), Move track (to the top, up one, down one, to the bottom), Delete the selected tracks, Remove empty tracks.
  - **Shift+Delete** deletes the selected tracks (it asks first if any has something on it: deleting tracks can't be undone yet).
  - **Double-click the empty space under the tracks** for a new track like the selected one (an audio track mono or stereo as it is, a group or FX channel), else one on the Paragon Synth.
- **Your window across all your screens:** SETTINGS > **SPAN ALL SCREENS** stretches the studio across every monitor (Windows' maximize only fills one), as tall as they all are; click it again (it then reads **BACK TO ONE SCREEN**) to put it back. And the window now opens where you left it, on one screen or across three.
- **Our choices:** a duplicated track is named "<name> copy"; Ctrl+Alt-click's range is one grid step.
- **Not there yet** (from the same chapters): typing the locator positions (Shift+L / Shift+R / Shift+D), Fill Loop, cycle markers, folder tracks, a track's own height (dragging its bottom edge), disabling tracks, Find Track. Ask when you want any of them.

## 0.8.1

The Range Selection tool: select a stretch of time across tracks, whatever the parts, and cut, copy, move or delete what's in it, as the Cubase 15 manual's Range Editing chapter describes.

- **RANGE (2)** in the TOOLS row. **Drag** across the tracks to select a range: a stretch of time on the tracks you drag over (snapped to the grid; Ctrl for anywhere). It's shaded, with orange edges.
  - **Double-click a part** for a range over it, or **between parts** for the gap.
  - **Drag its edges** to change it; **Shift-drag** outside it to stretch it there (and to that track).
  - **Drag inside it** to move what's in it somewhere else, on these tracks or others (**Alt-drag** copies it). The range goes with it.
  - **Right-click it** for its menu. **Esc**, or a click elsewhere with RANGE, clears it. Choosing another tool keeps it.
- **What you can do with it** (the manual's keys):
  - **Shift+X:** split the parts at its edges.
  - **Delete:** remove what's in it, leaving the gap. **Shift+Backspace (Delete Time):** remove it and close the gap (everything after moves back).
  - **Ctrl+X / Ctrl+C:** cut / copy what's in it. **Ctrl+V:** paste at the playhead, on the selected track and the ones below it. **Ctrl+Shift+X (Cut Time)** closes the gap; **Ctrl+Shift+V (Paste Time)** makes room first.
  - **Ctrl+D:** duplicate it straight after itself. **Ctrl+Shift+E (Insert Silence):** everything from its start moves on by its length.
  - **Crop** (in its menu): parts crossing its edges are cut down to it.
- **Parts crossing its edges** are cut there first, so only what's inside is touched (a note across an edge is split in two, as SPLIT does).
- **Ctrl+C / X / V** go to the range, or to the key editor's notes after a click in the editor; Ctrl+V pastes whichever you copied last.
- **Each edit is one undo step.**
- **Not there yet:** automation points in a range (Delete Time and Insert Silence move the parts, not the automation under them); adding or leaving out single tracks with Ctrl-click; the Range Start / End boxes of the manual's info line; snapping to zero crossings. Ask when you want them.

## 0.8.0

Editing tools for the arrangement, with the Cubase 15 manual's keys: cut, join, mute, erase and draw parts.

- **A TOOLS row over the arrangement:** click a tool, or press its number key (as the manual has them). **F9 / F10** go to the previous / next tool. What a click on a track does depends on the tool chosen; the ruler, automation lanes and right-click menus work as before.
  - **SELECT (1):** as before (select, move, Alt-drag to copy, resize). New: **Alt-click** a part to split it there; **Alt-drag** on an empty stretch of an instrument track to draw a new part.
  - **SPLIT (3):** click a part to cut it in two there (on the grid; hold **Ctrl** for anywhere). A line shows where it will cut. **Alt-click:** into equal pieces that long, all the way along.
  - **GLUE (4):** click a part to join it to the next one on its track. **Alt-click:** to all the parts after it. The gaps between them stay silent.
  - **ERASE (5):** click a part to delete it. **Alt-click:** it and every part after it on its track.
  - **ZOOM (6):** click to zoom in there (Alt-click to zoom out), or drag across a stretch to fill the view with it.
  - **MUTE (7):** click a part to mute it (click again to unmute), or drag a rectangle over several. Muted parts stay in the song, greyed out and marked MUTED, but aren't played (or exported). They're saved with the song.
  - **DRAW (8):** drag on an instrument track to draw a new empty part (whole grid steps).
  - **PLAY (9):** hold the mouse down on the tracks to play from there; let go to stop, and the playhead goes back where it was.
- **Alt+X (Split at Cursor):** splits every part the playhead is in, on all tracks, or only the selected part if one is selected.
- **The part's right-click menu** has Mute / Unmute, Split at the playhead and Glue to the next part.
- **Splitting through a note** cuts the note in two, so nothing is lost or left hanging past the end of its part (the manual's "Split MIDI Events" option, which we've chosen to always use). **Audio clips** split cleanly: the second half carries on exactly where the first stops, and the fade-in stays with the first half and the fade-out with the second.
- **Gluing audio:** a clip glues to the one cut from the same recording right after it (a split glued back together). Gluing different recordings would need an audio part that holds several clips, which PR7-S doesn't have yet, so it says so instead.
- **The docked mixer** opens tall enough for its inserts and a full fader at first (the new tools row took some height); drag the line above it as before.
- **Not there yet:** the **Range Selection** tool (2), with Split Range and cutting or deleting a stretch across tracks; selecting several parts at once (Shift-click, a rectangle) for the other tools and Edit menu commands; the right-click toolbox. Ask when you want them: Range Selection would be the natural next step.

## 0.7.3

The rest of the Cubase 15 manual's zoom and Auto-Scroll options.

- **Taller or shorter rows: Shift+H and Shift+G** (or **Ctrl+Shift+mouse wheel**), as the manual has them. In the arrangement, every row grows or shrinks together, so tracks with automation lanes open keep their proportions. They go from half height to three times. In the key editor (after a click in it), the note rows get taller or shorter, the note in the middle staying put.
- **Short tracks show one line:** below full height, a track's row in the track list keeps its name, M, S and REC, and puts away its instrument, R/W and level until you zoom back in.
- **Auto-Scroll's settings:** the small **arrow** beside each **AUTO-SCROLL** key (over the track list, and in the key editor) opens the manual's menu:
  - **Page Scroll** (as before): the page turns when the playhead reaches the right edge.
  - **Stationary Cursor:** while playing, the playhead stays in the middle and the song moves under it.
  - **Suspend Auto-Scroll when Editing:** a click in the tracks (or the notes) while playing stops the view following, so it doesn't move while you edit. It carries on when playback stops, or when you click AUTO-SCROLL (or press F). Meanwhile the key reads **SUSPENDED** (and lights pink on the white finish).
  - The arrangement and the key editor each have their own settings, and both are remembered.
- **Our choices:** the row heights' range, and the zoom steps (a quarter taller each Shift+H). The zooms aren't saved with the song yet.

## 0.7.2

Getting around the song: zooming and following the playhead with the Cubase 15 manual's keys, scroll bars, and a lower zone you can resize.

- **Zoom with H and G** (the manual's keys: **H zooms in, G zooms out**), a step at a time, centred on the playhead. **Shift+F** zooms out to show the whole song. They work in the arrangement, or in the key editor after you've clicked in it (each has its own zoom). Ctrl+mouse wheel still zooms round the mouse.
- **F turns Auto-Scroll on and off:** while it's on, the view turns the page when the playhead reaches the right edge (the manual's Page Scroll), and follows the playhead when it jumps somewhere out of sight (Home, for example). The arrangement and the key editor each have their own, as in the manual, with an **AUTO-SCROLL** key that's lit while it's on (over the track list, and in the key editor's panel). Both start on, as before, and are remembered.
- **Scroll bars** under and beside the arrangement and the key editor: drag them to go left and right along the song, and up and down the tracks (or the notes). The mouse wheel still scrolls too (Shift+wheel across). The bar goes 8 bars past the last part (at least 32 bars; our choice, as there's no project length to set yet).
- **Resize the lower zone:** drag the line above the docked mixer or key editor up or down. Each remembers its own height. The arrangement always keeps some room.
- **Hide the mixer's racks:** click the lit **INSERTS** (or **SENDS**) key in the mixer's bar to hide them. Then only the faders, pan and M/S/R/W show, and you can drag the docked mixer much shorter. Click **INSERTS** or **SENDS** to show them again (remembered). This is our design: the manual sets this up in a Window Layout pane.
- **Fixed:** the mixer's scroll bar was JUCE's default blue. Scroll bars are now in the family's orange.
- **Not there yet** (from the same chapters): vertical zoom (Shift+H / Shift+G: taller or shorter tracks), the Stationary Cursor mode (the playhead stays in the middle and the song scrolls under it), and Suspend Auto-Scroll When Editing. Ask when you want any of these.

## 0.7.1

MIDI file import: bring in a song, a drum groove or a bass line from a .mid file, as the Cubase 15 manual's File > Import > MIDI File does.

- **IMPORT** (in the header, after SAVE AS): choose a .mid file. If your song already has something in it, it asks first: **New song** (with the file's tempo and time signature) or **This song** (at bar 1, keeping your tempo).
- **Or drag a .mid file** from Explorer onto the arrangement: it goes into this song at the bar you drop it on.
- **Each track in the file becomes an instrument track** on the Paragon Synth (switch it to any of your VST3 instruments from its button), named as in the file, its notes in one part over the bars they're in. Timing, pitch and velocity come across as they are.
- **A file with everything on one track** (a type-0 file) is split into a track per MIDI channel (as the manual's "Auto Dissolve Format 0"), so the drums (channel 10) get a track of their own.
- **Our choices** (the manual leaves these to preferences):
  - Into this song, your tempo is kept and the notes land on its beats (the manual's "Ignore Master Track Events on Merge").
  - Into a new song, it takes the file's first tempo and time signature.
  - A dropped file makes a track for each of its tracks, at the bar it's dropped on.
- **Not there yet:** controllers (mod wheel, sustain, volume: PR7-S has no MIDI controller lanes yet), tempo changes partway through (a song has one tempo), program changes, markers and lyrics. MIDI export is not there either. Ask when you want any of these.

## 0.7.0

Sends, FX channels and group channels: one reverb (or delay) that every track can send some of its sound to, and submixes with one fader, as the Cubase 15 manual describes.

- **FX channels:** **+ ADD TRACK** > **FX channel** (empty, or **FX channel with** one of your effects, which goes in its first insert). It's named as the manual's are: "FX 1-<effect>". It takes what the tracks' sends give it, through its inserts, then its own fader (the effect's return) into the master.
- **Sends:** in the mixer, **SENDS** (next to **INSERTS** in its bar) shows each channel's 8 sends instead of its inserts.
  - Click an empty send to choose an FX channel, or **Add FX Channel to Send** to make a new one and send to it in one go.
  - The **dot** turns a send on or off. **Drag** across it for its level (**Shift** for fine moves). **Ctrl-click** sets 0 dB.
  - Right-click for its FX channel, **Move to Pre-Fader** or **Move to Post-Fader**, **Remove**, and **Bypass All Sends of This Channel**. **PRE** marks one before the fader.
  - After the fader (the default), turning the track down turns its send down too. Before the fader, the send stays as it is, which is handy for a reverb you want to keep while the dry track fades.
  - A new send starts on, after the fader, at 0 dB. The manual leaves these to a preference, so they're our choice.
  - Send levels can be automated and written like faders: each one has a lane ("Send 1: FX 1-...").
- **Group channels:** **+ ADD TRACK** > **Group channel**. Route tracks to it with **OUT** under their name in the mixer, or **Output** in their menu in the track list. They go through the group's effects and fader together (a drum bus, the backing vocals). A group can send to FX channels too.
- **Solo:** soloing a track doesn't silence the FX and group channels it goes through, so you still hear its reverb.
- FX and group channels have no parts and record nothing (no REC). Their rows show what they are. An FX channel's button opens its effect's window.
- **Export** includes everything.
- **Not there yet:** sends' own pan, FX channels feeding a group (they go to the master), groups feeding other groups, and side-chains. The number of sends (8) is our choice: the manual doesn't give one.

## 0.6.0

Automation: volume, pan, mute and any plugin knob moving on their own as the song plays, written by hand or drawn, as the Cubase 15 manual's automation chapter describes.

- **R and W** on every track (in the track list and the mixer) and on the master:
  - **R (read)** plays its automation back. The fader, pan and mute move with it.
  - **W (write)** records what you do to its controls while playing. W turns R on too.
  - **R and W in the header** turn them on or off for every track (**Alt+R**, **Alt+W**). They're lit while any track's are on.
  - Lit colours: R pink, W red, W orange while it's writing.
  - Record-arm on the track list is now **REC** (R is automation's, as in the manual).
- **Writing (Touch):** with W on, play and move a fader, a pan knob, or a knob in a plugin's own window (the PR7 plugins too). It's written while you hold it. Let go and the control returns to what it was (over 0.1 s: our choice, the manual gives no default).
  - **Mute** is an on/off switch: a click on M is written from there until you stop (as the manual's Auto-Latch).
  - Writing also stops when you stop, turn W off, the cycle comes round, or you click in the ruler.
  - Each pass is one undo step.
- **Automation lanes** open under their track:
  - The small arrow at a track's lower left shows and hides them; right-click a track for **Show/Hide Automation** and **Show All Used Automation**.
  - Each lane shows one control: click its name for any other (volume, pan, mute, the instrument's knobs, each insert's knobs). The control it showed keeps its points, hidden (marked * in the list). **Remove Parameter** and **Remove Unused Parameters** are there too.
  - **M** mutes a lane (Mute Automation). **+** at a lane's lower left adds a lane for the next control.
  - The lane shows its value at the playhead.
- **Drawing and editing**, in the arrangement:
  - **Click** a lane to add a point (R comes on). A click on the line between two points adds nothing.
  - **Alt-drag** draws several points.
  - **Drag from an empty spot** to select points in a rectangle; **Shift-click** adds or removes one.
  - **Drag** selected points to move them (snapped to the grid). **Ctrl:** one direction only. **Ctrl and another key:** no snapping. **Alt-drag** copies them.
  - **Delete** removes the selected points. **Esc** while dragging puts them back. Right-click: **Select All Events**.
  - A lane with no points shows a dotted line at the control's setting. With R off, or the lane muted, its curve is grey.
  - Volume, pan and plugin knobs ramp in straight lines between points; mute jumps.
- **The master** has a row at the bottom of the track list: its R, W and lanes (its volume, its effects' knobs), for a fade-out at the end of the song, for example.
- **Export** includes the automation.
- **Not there yet:** curved (Bézier) lines between points, the Line tool's shapes (parabola, sine...), Trim, Fill, Suspend, copying automation between tracks, Auto-Latch and Cross-Over for faders (Touch only for now), and the Automation Panel (F6). Ask for any of these when you want them.

## 0.5.1

Checked against the Cubase Pro 15 manual (the owner's request), and the family's colours only.

- **Colours:** tracks now take only the Paragon family's colours: shades of red, pink and orange (red, hot pink, orange, rose, red-orange, deep pink, peach, crimson). No more blue, lime, violet or aqua. Songs saved with those colours open with their new ones.
- **Mixer: 16 insert slots** (was 8), each effect in a slot of its own: removing one leaves its slot empty, and the others stay where they are. At least 8 are shown, more as you fill them.
- **Inserts after the fader:** click an effect > **Set as Last Pre-Fader Slot**, and the slots below it come after the fader and pan (below an orange line, tinted). Use it for a limiter on the master, for example. **All Slots Before the Fader** puts it back. By default, every slot is before the fader.
- **Faders and pan:** **Ctrl-click** resets them (0 dB, centre); **Shift-drag** moves them finely. Double-click still resets.
- **Arrangement:**
  - **Ctrl-drag** keeps a part to one direction (along the song, or to another track). Only **Alt-drag** copies now.
  - **Ctrl** turns snapping off while resizing, trimming or fading (**Ctrl+Shift** while moving). Shift no longer picks a finer grid.
  - In the ruler's top strip, **click inside the cycle's range** to turn CYCLE on or off, and **drag inside it** to move it.
  - **Double-click between the locators** for an empty part from the left locator to the right.
  - The last track can no longer hide under + ADD TRACK: scrolling goes far enough.
- **Key editor:**
  - **Ctrl** (not Shift) turns snapping off. Ctrl while moving notes keeps them to one direction (in time, or in pitch).
  - **Double-click a note** to delete it.
  - **Ctrl+V** always pastes at the playhead; if that's outside the part, the part grows to hold the notes.
  - **Ctrl+D** duplicates the selected notes straight after themselves (what pasting again and again used to do).
  - **Quantize** keeps every note: two of the same pitch that land together both stay.
- **Recording audio in a cycle** keeps the **last** complete pass (we kept the first). Stop during the first pass and you get what there is of it. The earlier passes stay in the file for takes and comping later.
- **Export:**
  - **32-bit float** WAV is offered (best for more work in a DAW).
  - The whole song now rings out after its end **until the sound has died away** (reverb and delay tails, a held note's release), up to 30 seconds, then stops. The 2-second box is gone.
  - Exporting the cycle stops at its end, as the manual's locator export does.
- **Not changed (your choice):** the keys stay as they are (R records, / cycles, Home goes to the start).

## 0.5.0

Export: your mix as a WAV file, to send, upload or master.

- **EXPORT** (in the header, after SAVE AS) asks what to export, then where to save it, then writes it behind a progress bar (**CANCEL** stops it and leaves no half-written file).
- **What:** the whole song (from bar 1 to the end of its last part), or the cycle. It opens on the cycle when CYCLE is on.
- **Everything you hear goes in:** every track through its effects, fader and pan, then the master's effects and fader. Mute and solo count, as they do when playing. The cycle doesn't repeat in the export: it's played once, straight through.
- **Sample rate:** your interface's, or 44.1, 48, 88.2 or 96 kHz. Recordings are converted to the rate you pick.
- **16 or 24 bit.** 24-bit is best for mastering or more work; 16-bit is CD quality.
- **The click** is left out unless you tick it.
- **Let effects ring out:** two more seconds after the end, so reverb and delay tails aren't cut off. Nothing new starts in them: notes and clips stop at the end, and only what's already sounding carries on.
- **Faster than real time:** a song is written in a fraction of its length. The interface is quiet while it writes, and everything (where you were, the sample rate) is as it was afterwards. Instruments and effects are told it's an offline render, as other DAWs do, so samplers wait for their samples instead of dropping notes.
- **Not there yet:** dithering for 16-bit (it's rounded, which is fine for most mixes but not quite mastering-grade), exporting each track to its own file (stems), MP3 or FLAC, and a note held from before the start of the cycle isn't heard in a cycle export (only notes that start inside it). Ask for any of these when you want them.

## 0.4.0

Audio tracks: record your guitar (or anything) through your interface, and ASIO on Windows.

- **Audio tracks:** + ADD TRACK > **Audio track (mono)** for a guitar or a voice, or **(stereo)** for a pair of inputs. Where an instrument track shows its instrument, an audio track shows its **input** (IN 1, or IN 1+2): click it to choose another (or to delete the track).
- **MON** (on audio tracks): hear what comes in, through the track's effects and fader. Put PR7-G in its inserts and play through it.
- **Recording:** arm the audio track (R, or just select it) and press RECORD. What comes in becomes a **clip** on the track, shown as it records. It lands **exactly where you played it**: the studio places it at the sample recording started, and moves it earlier by your interface's own delay (in and out, as the driver reports it), so it lines up with what you were hearing. In a cycle, the first pass is kept (takes and comping come later).
- **The recordings** are WAV files (24-bit, at your interface's sample rate). Until a song is saved they go in Documents > Paragon PR7-S > Unsaved Recordings; **saving** puts them in a folder beside the song, "<song name> Audio" (Save As copies them to the new place). A song whose recordings are missing still opens and says which; their clips stay, silent, until the files are back.
- **Clips** show their waveform. Move them (onto another audio track too), Alt-drag to copy, Ctrl+D to duplicate, Delete. Drag a clip's **left edge** to trim its start (the audio stays where it is in the song), its **right edge** to trim its end (no further than the recording goes). Drag the small **handles** at its top corners to **fade** it in and out. Each edit is one undo step.
- **ASIO on Windows:** SETTINGS > **ASIO**, then your interface's ASIO driver (for the AXE I/O: IK Multimedia's, installed from IK's site), for the lowest delay between playing and hearing. Windows Audio is still there.
- **SETTINGS:** switch on the inputs you record from (up to 8). Audio tracks count them IN 1, IN 2... in the order they're on.
- **Fixed:** stopping, moving the position and pressing play straight away could carry on from where it stopped instead.
- **Not there yet:** takes and comping (a cycle keeps the first pass), splitting clips, clip volume, time-stretching (a clip plays at its own speed if you change the tempo), and very long recordings are held in memory whole (fine for songs; an hour of stereo takes about 1 GB). Ask for any of these when you want them.

## 0.3.2

The mixer in its own window with F3, as in Cubase; and PR7-C's meters.

- **F3** opens the mixer in a **window of its own**, full height, so the faders are long. Press F3 again to close it. The first time, it fills the screen the studio is on; after that it opens where you left it (another monitor, maximized, stretched).
- **Alt+F3**, or **MIXER** in the header, opens it **docked** under the arrangement, as before. Pressed again, it closes.
- Either key moves it if it's open the other way: F3 on the docked mixer moves it into its window, and Alt+F3 on the window brings it back under the arrangement. The WINDOW / DOCK button still works too.
- **PR7-C's meters**, everywhere in the studio: the mixer's channels and master, the track list and the transport's OUT. Fine LED segments on black glass, orange at the bottom shading to pink-red at the top (left to right in the track list), and the highest recent peak held lit for a moment. They read from -60 to +6 dB (digital full scale is 0), the same range as the faders. PR7-C's own meter reads like a console (0 there is 20 dB below full scale); the studio's is plain digital, so a meter near the top means near clipping.
- They keep their fine segments however tall the mixer is.

## 0.3.1

A new look for the lettering.

- **Bebas Neue everywhere:** the studio's text is now in Bebas Neue (tall, narrow capitals), from the buttons and track names to the note names, the menus and the GRID list. It's drawn a little larger than the old font so it reads at the same size. It's built into the app (free, under the SIL Open Font License), so it looks the same on every computer.
- Because Bebas has only capitals, names show in capitals: a track called "Synth 1" reads SYNTH 1. They're saved as you typed them.
- What stays as it was: the LED screens (position, tempo, signature), so they match their digits; and the tooltips.
- **White text** in the track list and the mixer, where it was orange: the instrument names, the effects in the inserts, the mixer's title and WINDOW button. Bypassed effects stay grey.

## 0.3.0

The mixer, and more room: the on-screen keyboard at the bottom of the window has gone, so the lower zone (the key editor, and now the mixer) reaches the bottom.

- **The mixer** (MIXER in the header, or **F3**), inspired by Cubase's MixConsole, in the PR7 look. It opens under the arrangement, where the key editor goes; one of the two is there at a time (opening one puts the other away). Its **WINDOW** button puts it in a window of its own (maximize it, or move it to another screen; it remembers where), and **DOCK** brings it back, as the key editor's does.
- **A channel strip for every track:** its colour and name, **8 inserts**, pan, **M** and **S**, the volume **fader** and its level. Click a strip to select its track. Double-click a fader for 0 dB, or the pan for the centre. A fader drag is one undo step, and you hear it as you move it.
- **The MASTER strip** on the right: every track goes into it, through its own 8 inserts, then its fader, to your speakers. Its level shows both sides.
- **Inserts are your VST3 effects**, including our PR7 plugins. Click an empty slot (the one with +) and pick an effect. Click one that's there to open its window, **Bypass** it, **Replace** it with another, or **Remove** it. The sound goes through a channel's inserts from top to bottom, and **then** its fader and pan, as on a mixing desk. A bypassed effect shows grey.
- **PLUGINS** (SCAN / OPTIONS) now finds your VST3 effects as well as instruments. If an effect's menu says none were found, scan first.
- **Mono effects** (some guitar plugins are) get both sides of the track mixed into their one input, and their output goes to both sides.
- **Saved with the song:** each effect and all its settings, whether it's bypassed, and the master's volume. A song opened on a computer that doesn't have one of its effects still opens and says which. The effect keeps its place (shown grey) with its settings, ready for when it's there, and the sound goes past it meanwhile. Songs from 0.2 open with the master at 0 dB and no effects.
- **The click** goes straight to your speakers, not through the master's effects, so a limiter there never squashes it.
- **Undo:** faders, pan, mute, solo and the master's volume. Adding, replacing, removing and bypassing effects aren't undone (as with instruments).
- **Fixed:** after adding a track, the first change to it (its volume, say) couldn't be undone (since 0.1.0).
- **The on-screen keyboard has gone**, as asked. To hear a note without a MIDI keyboard, click the key editor's piano keys. Recording needs a MIDI keyboard now.
- **Not there yet:** sends and effect (FX) channels, groups, a channel's EQ and dynamics built in, delay compensation for effects that delay the sound (lookahead limiters, linear-phase EQs: they'd be slightly late against the rest, by their own delay, usually a few milliseconds), and moving inserts by dragging. Ask for any of these when you want them.

## 0.2.2

Two additions to the key editor: its own window (as big as your screens), and copy and paste. (0.2.1 was never released: one of its tests failed on macOS, so everything is here.)

### The key editor in a window of its own

- **WINDOW** (next to CLOSE in the key editor) moves the editor out from under the arrangement into its own window, with the usual minimize, maximize and close buttons. Maximize it, or drag its edges across all your screens. The arrangement gets its full height back meanwhile.
- **DOCK** (the same button, in the window) puts it back under the arrangement.
- It remembers: where the window was and how big (or that it was maximized), and whether you last used it in its window. Close it while it's in its window, and next time you open the editor (double-click a part, KEY EDITOR, Ctrl+E) it opens in its window again, in the same place.
- Its title shows the part: "Key Editor: Clean Guitar".
- The studio's keys work in it as in the main window (Space to play and stop, R to record, and the editor's own: Delete, Up and Down, Q, 1 / 8 / 5). Escape closes it.
- When it moves to or from its window, it zooms to show the whole part across the new width.
- On Windows, the maximize button fills one screen (the one the window is on). To cover all three monitors, drag the window's edges across them once; it opens that size from then on. (If your graphics card joins the screens into one wide display, as NVIDIA Surround or AMD Eyefinity do, maximize covers all of them.)

### Copy and paste

- **Ctrl+C** copies the selected notes, **Ctrl+X** cuts them (copies, then removes), **Ctrl+V** pastes. The pasted notes keep their pitches, lengths, velocities and their timing against one another, and are selected afterwards, so you can move them straight away.
- **Where they go:** at the playhead, if it's inside the part (as in Cubase; click the editor's ruler to put it there). If it isn't, they go on the beat after the notes you copied, so pressing Ctrl+V again and again repeats a phrase in time. Notes that would land past the part's end aren't pasted (they wouldn't be heard).
- **Into another part:** copy, select another part in the arrangement (on any track), Ctrl+V: the notes go in at the playhead, or at that part's start.
- **Shift+A** selects all the notes (as Ctrl+A does).
- While the key editor is open, these four keys always go to it, even after a click in the arrangement (which has no copy and paste of its own yet).
- Each paste or cut is one undo step.

## 0.2.0

The piano roll: the **key editor**, for drawing and editing a part's notes, inspired by Cubase's key editor in its lower zone, in the PR7 look.

- **Opening it:** double-click a part (or right-click it, **Edit notes**), or press **KEY EDITOR** in the header (Ctrl+E). It opens below the arrangement and shows the part you last selected there; select another part and it follows. **CLOSE** or Escape closes it.
- **The piano down the side:** click a key to hear it. Keys light up while a note sounds (from the editor, the on-screen keyboard or your MIDI keyboard). Notes are named with middle C as **C3**, as Cubase counts.
- **Three tools** (on the left, or keys 1, 8 and 5):
  - **SELECT:** click a note to select it (Shift or Ctrl adds or removes); drag a box round several. Drag notes to move them in time and pitch; **Alt-drag** to copy them; drag a note's right end to make it longer or shorter (every selected note changes by the same amount). Double-click empty space for a new note.
  - **DRAW:** click for a new note, one grid step long; drag right as you click to make it longer. Click a note to remove it.
  - **ERASE:** click a note, or drag across several, to remove them.
  - You hear each note as you click or move it, through the track's instrument.
- **The grid:** 1/4, 1/8, 1/16, 1/32, 1/8 triplet or 1/16 triplet. Notes snap to it, a new note is one step long, and QUANTIZE uses it. Hold **Shift** while dragging for no snapping.
- **The velocity lane** under the notes: a bar for each note, as tall as it's played (0 to 127; a new note is 100). Click a bar, or drag across several, to set them. If any of the notes under the mouse are selected, only the selected ones change (so you can set one note of a chord). Notes are drawn brighter the harder they're played.
- **QUANTIZE** (or Q) moves the starts of the selected notes (all of them, with none selected) to the grid. It lines up with the song's bars, even in a part that doesn't start on a bar. Lengths stay as they were. Two notes of the same pitch that land on the same spot become one (the longer one).
- **Keys** after a click in the editor: Delete removes the selected notes, Up and Down move them a semitone (Shift: an octave), Ctrl+A selects them all, Q quantizes. After a click in the editor, Delete never removes the part (click the arrangement first for that).
- **Undo:** each edit (a drag, a click, a quantize) is one undo step.
- The editor has its own zoom and scroll: the wheel scrolls up and down the notes, Shift+wheel along the song, Ctrl+wheel zooms. Click its ruler to move the playhead; it turns the page while playing, like the arrangement.
- Notes outside the part are shaded and aren't heard (as in 0.1.0). A note can't be dragged out of its part.
- **Not there yet:** quantizing note lengths or ends, quantize strength or swing, controller lanes (pitch bend, modulation, sustain), the drum editor, and step entry from a MIDI keyboard. Ask for any of these when you want them. Songs from 0.1.0 open unchanged (the file format is the same).

## 0.1.0

The first Paragon PR7-S: the PR7 family's recording studio, a program of its own (Windows, macOS and Linux) with a workflow inspired by Cubase, in the PR7 look. This first stage records and plays MIDI with instruments.

- **Instrument tracks:** each plays the **Paragon Synth** (built in: a warm polyphonic synth, so there's sound before you install anything) or one of your **VST3 instruments**. **PLUGINS** finds them (SCAN / OPTIONS), and the instrument's own window opens from the track. Each track has its colour, name (double-click to rename), **M** mute, **S** solo, **R** record-arm, a volume knob and a level light.
- **Recording:** arm a track (selecting it arms it), press **RECORD** (or R) and play your MIDI keyboard or the on-screen one. What you play becomes a part on the track, from the bar you started in to the end of the bar you stopped in. In a cycle, every pass goes into one part. Press RECORD again while recording to stop recording and keep playing.
- **The arrangement:** parts show their notes. Move them (to another track too), Alt-drag to copy, drag the right edge to resize, double-click an empty lane for a new empty part, Delete to delete, Ctrl+D to duplicate. They snap to the beat, or the bar when zoomed out; Shift for sixteenths. Ctrl+wheel zooms, Shift+wheel scrolls along the song.
- **The transport:** a pearl PR7 unit with START, STOP, PLAY, RECORD, CYCLE and CLICK keys, and LED screens for the position (bars, beats, sixteenths and ticks, and minutes and seconds), the tempo (drag it, or double-click to type) and the time signature (click for the list). Click the ruler to move the playhead; drag across its top strip to set the cycle. Stop twice to go back to where playing started.
- **The click:** on every beat, higher on the first of the bar.
- **Songs** save and open as `.pr7s` files, with each instrument's settings. A song whose VST3 instrument isn't on this computer still opens, and says which tracks are silent.
- **Undo and redo** for part edits, recordings and the song's settings (adding and deleting tracks aren't undoable yet; deleting asks first).
- **Shortcuts** (each transport key shows its own under it): Space play/stop, R record, Home to the start, / cycle, C click, Delete, Ctrl+D duplicate, Ctrl+Z / Ctrl+Shift+Z undo / redo, Ctrl+N / O / S / Shift+S.
- **SETTINGS:** your audio interface, its sample rate and buffer size, the MIDI inputs (all on to start with) and the finish (Pearl White, White + Neon, Pearl Black).
- **Not there yet** (the next stages): the piano roll (0.2.0), the mixer with effects (0.3.0), audio tracks for recording guitar (0.4.0), and automation and exporting a mixdown (0.5.0). On Windows the audio goes through Windows Audio (WASAPI) for now; ASIO comes later.
