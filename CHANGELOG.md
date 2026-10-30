# Changelog

Each version's notes appear on its GitHub Release page.

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
