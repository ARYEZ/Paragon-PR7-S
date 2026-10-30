# Paragon PR7-S: notes for Claude

**Paragon PR7-S** is the Paragon PR7 family's **studio**: a standalone DAW (AGPLv3, JUCE, a program, not a plugin) with a workflow **inspired by Cubase** and the PR7 family's look. The owner chose the name (S for studio).

| Member | Repo |
|---|---|
| PR7-G (guitar) | `ARYEZ/Paragon-X1` (now Paragon-PR7-G) |
| PR7-B (bass) | `ARYEZ/Paragon-PR7-B` |
| PR7-V (vocals) | `ARYEZ/Paragon-PR7-V` |
| PR7-C (channel strip) | `ARYEZ/Paragon-PR7-C` |

## The plan (agreed with the owner; one release each)

1. **0.1.0, done: MIDI and instruments** (the owner's choice to start). Instrument tracks (the built-in Paragon Synth or VST3 instruments), MIDI recording from a keyboard or the on-screen one, the arrangement, transport, click, cycle, songs saved as `.pr7s`, undo.
2. **0.2.0, done: the piano roll (key editor):** draw, move, copy, resize, delete notes; velocity lane; quantize; in the lower zone, following the selected part.
3. **0.3.0: the mixer:** channel strips, inserts (VST3 effects, including our PR7 plugins), master bus.
4. **0.4.0: audio tracks:** record guitar through the interface, clips, trimming, fades.
5. **0.5.0: automation and export** (a mixdown to WAV).
6. Later: takes and comping, MIDI controller lanes, drum editor, markers... as the owner asks.

Be honest about scale: Cubase took a large team decades. Say where it stands.

## Names

**Never** "Cubase", "Steinberg" or their graphics in the app, repo or release notes. "Inspired by Cubase's workflow" is fine. The look is the PR7 family's own.

## Code layout

- `source/engine/`: no screen needed, fully testable.
  - `Project`: the song as plain data (tempo, signature, cycle, click, tracks, parts, notes in **beats** = quarter notes), XML save/open, bar/beat formatting.
  - `Engine`: the audio device callback. Plays each track's events through its instrument (`AudioPluginInstance`), the cycle, the click, mix (volume, balance pan, mute, solo), live MIDI (devices via `MidiMessageCollector`, the on-screen `MidiKeyboardState`) into the live track, and records note on/off with their beats into a lock-free FIFO. **Threads:** the audio thread only `ScopedTryLock`s (a missed lock = one silent block); the message thread prepares everything outside the lock and swaps under it (`update`, `setInstrument`, `removeTrack`). No allocation on the audio thread.
  - `ParagonSynth`: the built-in instrument (an `AudioPluginInstance`, so it's played like a VST3).
  - `PluginHost`: VST3 format manager and the known-plugin list (kept in the settings file).
  - `Project` also has `quantize` (note starts to the *song's* grid; same-pitch twins merge) and `noteName` (middle C = C3, as in Cubase).
  - `Studio`: message-thread controller; owns project, engine, host, `AudioDeviceManager`, settings; every action; turns recordings into parts; gives every part an `id` (not saved; `findPart`) so the key editor keeps its part when indices change; `setNotes` replaces a part's notes (one undo step per editor gesture); undo (parts, song settings, track names and mix; not adding/removing tracks); `onInstrumentGoing` lets the UI close an instrument's window **before** the instance is deleted (a plugin editor must never outlive its processor).
- `source/ui/`: `MainView` (header, `TransportBar`, `TrackList` | `ArrangeView`, `KeyEditor` below them when open, on-screen keyboard; keys go to the editor after a click in it), `Windows` (instrument editor, settings, plugins), `Pearl.h` (the pearl unit surface). From PR7-C: `ParagonLookAndFeel`, `Finish` (settings file "Paragon PR7-S"), `LedDisplay`, `Logo` (the mark with "-S"), `LevelMeter`.
  - `KeyEditor`: the piano roll. Works on its own copy of the notes during a drag and calls `Studio::setNotes` once on release. Tests drive it with real `MouseEvent`s (`tests/UiTests.cpp`).
- `PR7SCore` (CMake) is a static library with everything but `Main.cpp`, so the app and the tests run the same code.

## Build, test, check

- **Build:** `cmake -B build && cmake --build build -j8`. JUCE 9.0.4 and Catch2 are fetched by CMake (locally you can point `FETCHCONTENT_SOURCE_DIR_JUCE` at another checkout).
- **Tests:** `xvfb-run -a build/tests/ParagonPR7STests`. All must pass. Engine tests use `Probe` (tests/TestHelpers.h), an instrument that logs every MIDI event with its sample: check timing to the sample, not just "it played".
- **Render:** `PARAGON_SNAPSHOT=studio.png xvfb-run -a build/tests/ParagonPR7STests "[snapshot]"` (a demo song, with the key editor open on the lead part). `PARAGON_SNAPSHOT_WIDTH=3000` for 2x, `PARAGON_SNAPSHOT_FINISH=black|neon`. Look, crop and zoom before claiming a visual change is right.
- No pluginval: it's an app. Windows is built with the static C++ runtime.

## Releases

- Bump `project(ParagonPR7S VERSION x.y.z)` and add a `## x.y.z` CHANGELOG section, then push. CI builds Windows, macOS and Linux, runs the tests, and publishes a release with `ParagonPR7-S-vX.Y.Z-<OS>.zip` (the app itself).
- About 15 minutes after pushing, check the release has all three zips **and its tag is your commit**.
- The owner installs on **Windows** and works in **Cubase** and BeatMaker.

## Open questions (ask, don't guess)

- **ASIO on Windows** needs Steinberg's ASIO SDK (check its current licence and whether it can be fetched in CI) before turning on `JUCE_ASIO`. Until then: WASAPI.
- A **Cubase Operation Manual** from the owner helps for behaviour and shortcuts (never for names or graphics).

## How the owner likes to work

- **Explain the why**, specifically: what was wrong, what changed, and the trade-offs. Plain language; the owner is a musician, not a programmer.
- **Be honest about limits:** say when something is our design, when a detail is a guess, and what can't be checked here (audio by ear, real devices, Windows).
- **Show it:** a rendered screenshot of every visual change, in all three finishes.
- **"merge it"** means: open a PR from the working branch to main and merge it (merge commit), then reset the working branch to the new main.
- Commit messages explain what and why.
