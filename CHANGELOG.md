# Changelog

Each version's notes appear on its GitHub Release page.

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
