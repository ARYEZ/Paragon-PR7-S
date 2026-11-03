# Changelog

Each version's notes appear on its GitHub Release page.

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
