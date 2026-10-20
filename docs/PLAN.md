# Wipeout XL on modern systems, from the PlayStation code

Goal: the PS1 game (SCUS-94351), playing exactly as it does on a
PlayStation, frame for frame, on PC/Mac/Linux, with:
- sharp, non-wobbling 3D at any resolution;
- keyboard and gamepad with proper glyphs (the PC version's keyboard art);
- saves in files.

## The game code is the PlayStation's code

`game/` is the PS1 decompilation (`wipeout-xl/decomp`), every one of its 585
functions:
- 572 compile to the retail bytes;
- the other 13 are proven equivalent.

It is imported by `tools/import.py` on the `decomp` branch and merged into
`main`. Nothing in it is re-imagined or re-translated.

Port edits are limited to what a 64-bit native compiler needs:
- 32-bit `long`, done by the import;
- prototypes that the PlayStation compiler tolerated;
- file records that hold pointer slots, widened on load;
- data that was in assembly files, now generated as C;
- the 13 proven-equivalent functions, whose C uses raw 32-bit struct offsets,
  rewritten with the real struct fields.

Every edit is a small commented diff, and the ledger lists each one.

Timing: the game runs its own loop with `VSync`. The platform delivers
vertical blanks at the NTSC rate the game was written for, so the game steps
exactly as on the console.

## The platform replaces the PlayStation libraries, not the hardware

The game calls the PsyQ libraries:
- libgte, libgpu, libspu, libcd, libpad, libcard, libapi, libetc.

`platform/` implements those calls:
- **libgte:** exact integer results, as the console produces them, since the
  game logic uses them. Every projection also keeps a full-precision
  (float) copy of the vertex, keyed by its screen position for the frame. The
  renderer uses it in place of the integer one, so geometry doesn't snap to
  pixels or wobble, and texturing is perspective-correct.
- **libgpu:** the primitives and ordering tables the game builds are drawn in
  ordering-table order with OpenGL, at the window's resolution:
  - flat, Gouraud and textured polygons, sprites, tiles, lines;
  - the four PlayStation blend modes and texture windows;
  - textures uploaded with `LoadImage` as 4/8/15-bit pages and CLUTs,
    converted on demand.

  There is no pixel-level emulation of the console's GPU.
- **libspu:** the sound processor's voices (ADPCM samples, pitch, volume,
  ADSR envelopes) and its reverb, mixed for the sound card.
- **libcd:** reads files and plays the soundtrack from the user's PS1 disc
  image (cue/bin).
- **libpad:** keyboard and gamepads, as a PlayStation pad.
- **libcard:** memory card saves become files.
- **Intro movies:** the same videos from the PC release's MPEG files.

## Verification

- **Lockstep against the console:**
  - recordings of the real game in DuckStation (`reference-runs/`):
    - pad input;
    - RAM states;
    - the GPU command stream;
    - SPU writes;
  - the port replays the same input and compares game state every frame.
- **Scripted playthroughs** under AddressSanitizer:
  - boot, intros, every menu, races on every track and class, weapons,
    pit stops, results, saves;
  - keys and pad buttons pressed as a player would;
  - screenshots reviewed.
- Nothing goes to the user without both.
