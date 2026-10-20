# Lockstep tests against the console

`tools/trace/lockstep.sh RECORDING` runs the port hidden, muted and in
fixed-step time against a recording of the real game in DuckStation
(`reference-runs/retail-trace-*`, made by the recorder in
`wipeout-xl-port/tools/trace`), then `tools/trace/lockstep.py` compares what the
two drew.

## What the port replays

| From the recording | Port | Why |
|---|---|---|
| `pads.txt` | `WIPEOUT_PADS`: injected at the fixed-pad read and the game-key read, by read count | the recorder injects at both reads |
| scripted writes | `WIPEOUT_WRITES` (`writes.py` maps console offsets to the native fields) | the weapons and music scenarios set weapons, laps, the CD track |
| GsGetVcount reads | `WIPEOUT_VCOUNTS` (`vcounts.py`) | frame lengths, slow frames, random seeds come from the scanline counter |
| loading-screen lengths | `WIPEOUT_LOADS` (`loads.py`) | the loading screen runs from the vertical-blank callback and advances the random seed |
| infinite autopilot | `WIPEOUT_AUTOPILOT` | the recorder's autopilot patch |
| fresh memory cards | `WIPEOUT_SAVE_DIR` | |

## What the comparison leaves out or allows

- The boot ROM's drawing before the game starts, and the movie player's.
- Loading-screen frames, and which of the two display buffers a frame is
  drawn into (both depend on how long the console took to load).
- Colours of raw-textured primitives (the GPU does not read them).
- The console's GPU reads an ordering table while the game builds the next
  frame. Primitives the game changes or relinks meanwhile may be drawn as
  they were or as changed, partly torn; the port records both states and the
  comparer accepts what the console could have read (frames "as the console
  GPU read the tables").
- Frames that differ only in vertex colours are counted separately.

## Results (2026-10-20)

| Recording | Frames | Packet for packet | GPU timing | Different |
|---|---|---|---|---|
| boot-intro-title | 375 | 375 | 0 | 0 |
| menus-slow (every menu page, load/save, passwords, hi-scores) | 6977 | 5920 | 1057 | 0 |
| arcade-track01-weapons-manual (all eleven weapons) | 3225 | 2323 | 90 | 812, colours only |
| track01-music-loop (autopilot, CD music loop) | 12419 | 3287 | 90 | 9042, colours only |

The colour-only differences are the weapon pickup pads' colour cycle: its
angle (`WeaponGridZone.angle`) is never initialised, so it starts from
whatever the heap held, on the console as in the port.

## Known gaps

- Loads: the port reads the disc and decompresses faster than the console,
  so in play the loading screens are shorter (the tests replay the console's
  length).
- `MenuControl`'s `VSync` argument after `UpdateNotes` is whatever the
  console's sound update left in a register; no recording reaches it yet.
