# Wipeout XL

The PlayStation game (USA, SCUS-94351) running natively on macOS, Linux and
Windows. The game is the console's own code: the decompiled C of the retail
executable, compiled for the PC, with the PlayStation libraries replaced by the
platform layer (OpenGL drawing at any resolution, the sound chip, the CD drive,
pads and memory cards). It plays like the console, frame for frame: see
[docs/LOCKSTEP.md](docs/LOCKSTEP.md) for the comparison with recordings of the
real game.

## What you need

- Your copy of the PlayStation disc as a `.cue` with its `.bin` tracks
  (Wipeout XL USA). The music plays from the disc's audio tracks.
- Optional: the Windows release's CD. Its `MPEG` folder gives the intro and
  ending movies.
- To build: CMake, a C compiler, SDL3 (and zlib for the test traces).

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build
```

## Play

```sh
build/wipeoutxl --disc "path/to/Wipeout XL (USA).cue" --pc-data path/to/pc-cd
```

The whole track is drawn, as far as the camera sees, where the console let
scenery, track and other ships pop in at a fixed distance; nothing the game
computes changes (`WIPEOUT_FAR_VIEW=0` brings back the console's view).
Far away, from 64000 to 96000 units, everything fades smoothly into the sky,
as in wipeout-rewrite (`WIPEOUT_FADE=near,far` sets the distances,
`WIPEOUT_FADE=0` draws without a fade).
3D geometry is also depth tested per pixel, where the console only sorted
whole polygons: nothing pokes through what is in front of it any more
(`WIPEOUT_DEPTH=0` draws in the console's order alone). The
disc is read as fast as the image allows, so loading takes a moment instead
of the console's seconds (`WIPEOUT_CD_SPEED=1` for the console's drive).

`--window 1920x1440` sets the window size, `--fullscreen` starts full screen;
Alt+Enter or F11 switches while playing. The environment variables
`WIPEOUT_XL_DISC` and `WIPEOUT_XL_PC_DATA` can stand in for the two paths.

## Controls

Keyboard (the Windows release's layout):

| | Menus | Race |
|---|---|---|
| Enter | accept | pause |
| Escape | back | pause |
| Arrow keys | move | steer, nose up and down |
| S | | thrust |
| Space | | fire |
| Left Alt | | drop weapon |
| A / D | | left / right air brake |
| F1 | | change view |

Gamepads work as the PlayStation pad, by button position. The on-screen
buttons follow whatever you used last: key caps for the keyboard, A/B/X/Y for
Xbox pads, B/A/Y/X for Nintendo pads, the console's symbols for PlayStation
pads. The in-game controller screen rebinds the race controls for both.
[docs/CONTROLS.md](docs/CONTROLS.md) has the details.

## Two players

ONE ON ONE and ARCADE LINK (RACE TYPE on the main menu) were the console's
link-cable modes; here they are two players on one machine, the window split
top and bottom. The lobby shows the first player's controller and ship on the
left; the second player presses START (or ENTER on the keyboard) on another
controller to join on the right. Each chooses a team with left and right and
confirms with their accept button; when both are ready, the first player
chooses the class and track and the race starts. The second player runs a
second copy of the game behind the scenes, linked to the first as the two
consoles were by the cable.

## Saves

The game saves to memory cards, as on the console. Card 1 is
`memcard0.mcr` in the user's data folder (macOS:
`~/Library/Application Support/Psygnosis/WipeoutXL/`), in the raw format
emulators use.

## How it is built

[docs/PLAN.md](docs/PLAN.md) describes the design; [docs/IO.md](docs/IO.md)
the sound, CD, pad and card side.
