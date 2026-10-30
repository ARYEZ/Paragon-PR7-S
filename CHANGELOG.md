# Changelog

Each version's notes appear on its GitHub Release page.

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
