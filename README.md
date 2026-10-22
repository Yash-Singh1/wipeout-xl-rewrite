# Wipeout XL

The PlayStation game (USA, SCUS-94351) running natively on macOS, Linux and
Windows, and in a web browser. One code base builds all four (the same game, the
same platform layer over SDL3 and OpenGL; the web uses WebGL 2). The game is the console's own code: the decompiled C of the retail
executable, compiled for the PC, with the PlayStation libraries replaced by the
platform layer (OpenGL drawing at any resolution, the sound chip, the CD drive,
pads and memory cards). It plays like the console, frame for frame: see
[docs/LOCKSTEP.md](docs/LOCKSTEP.md) for the comparison with recordings of the
real game.

## What you need

- Your copy of the PlayStation disc as a `.cue` with its `.bin` tracks
  (Wipeout XL USA). The music plays from the disc's audio tracks, the intro
  and ending movies from its movie files.
- To build: CMake, Ninja, a C compiler (Clang or GCC), and SDL3 3.2 or later
  (zlib for the test traces is optional). Where CMake does not find SDL3 it
  downloads and builds it (`-DWIPEOUT_BUILD_SDL=ON` always does).

## Build

macOS (Homebrew's `sdl3`) and Linux (for example Debian 13 or Ubuntu 25.04:
`libsdl3-dev libgl-dev`):

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build
```

Windows, with MSYS2's MinGW-w64 (UCRT64 or CLANG64 shell, packages `cmake`
`ninja` `gcc` or `clang`): the same two commands. From macOS or Linux, with the
MinGW-w64 cross compiler, use the toolchain file:

```sh
cmake -S . -B build-windows -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
ninja -C build-windows
```

`wipeoutxl.exe` is a single file, with SDL3 and the C runtime linked in.

The web, with Emscripten:

```sh
emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build-web
```

This makes `wipeoutxl.html`, `wipeoutxl.js` and `wipeoutxl.wasm`; put the three
on any web server (or `python3 -m http.server` in `build-web` and open
`http://localhost:8000/wipeoutxl.html`).

## Play

```sh
build/wipeoutxl --disc "path/to/Wipeout XL (USA).cue"
```

Started without `--disc` (from the desktop, say), the game opens the disc
chosen last time, or asks for the `.cue` with a file dialog.

In the browser, the page asks for the disc: choose or drop the `.cue` together
with all its `.bin` tracks. They are read into the page's memory (about 750 MB,
so a desktop browser with WebGL 2: Chrome, Edge, Firefox or Safari) and never
leave the computer. A disc image on the same server can be given in the
address instead: `wipeoutxl.html?disc=path/to/Wipeout%20XL%20(USA).cue`, with
the tracks beside the `.cue`. Gamepads work through the browser's Gamepad API
(press a button for the browser to make the gamepad known). Settings and memory
cards are kept in the browser's storage for the site. The two-player modes need
the second copy of the game the desktop starts, so the web has one player.

The whole track is drawn, as far as the camera sees, where the console let
scenery, track and other ships pop in at a fixed distance; nothing the game
computes changes (`WIPEOUT_FAR_VIEW=0` brings back the console's view).
Far away, from 64000 to 96000 units, everything fades smoothly into the sky,
as in wipeout-rewrite (`WIPEOUT_FADE=near,far` sets the distances,
`WIPEOUT_FADE=0` draws without a fade).
Polygons are drawn in the console's order, as the game sorts them. The
disc is read as fast as the image allows, so loading takes a moment instead
of the console's seconds (`WIPEOUT_CD_SPEED=1` for the console's drive).

The game runs at its 30 frames a second, as on the console, but the picture is
drawn as often as the computer can: between two of the game's frames every
model, the track and the camera move smoothly from one to the next (the game's
own frames are not changed, only shown more often). The debug console's
Graphics tab sets the frame rate: uncapped (the default), the display's refresh
(VSync), a cap in pictures a second, or the console's 60 fields a second
without the frames in between (`WIPEOUT_FRAME_RATE=uncapped|vsync|console|N`).

Widescreen (the debug console's Graphics tab, or `WIPEOUT_WIDESCREEN=1`): in a
window wider than 4:3 a race fills the whole width, showing more of the scene at
the sides, with the HUD at the window's edges. In the menus the backdrop is
enlarged to fill the width while the menus themselves keep the console's 4:3;
loading screens and movies stay 4:3. The game computes exactly what it did.

`--window 1920x1440` sets the window size, `--fullscreen` starts full screen;
Alt+Enter or F11 switches while playing. The environment variables
The environment variable `WIPEOUT_XL_DISC` can stand in for the disc's path.

## Debug console

The ~ key (below Esc) opens a console over the whole game window: frame rates and a
frame-rate overlay, the draw distance (the console's or the whole track) and
its fade, the frame rate, widescreen, skipping the intro movie, full screen, the
frame recorder (Tab: the last 3 seconds, or from one Tab to the next, with every
primitive behind any pixel), and where the settings and the memory cards are. Changes are saved at once to
`wipeoutxl.ini` in the user's data folder; a new saves folder takes effect at
the next start. Dear ImGui (`platform/third_party/imgui`, MIT) draws it.

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
left; the second player presses START on another controller (or ENTER on the
keyboard, when the first player uses a gamepad) to join on the right. Each
chooses a team with left and right and confirms with their accept button; when
both are ready, the first player chooses the class and track and the race
starts. With widescreen on, each half fills the window's width (the half-size
HUD at its edges). After the race the lobby comes back with the same players,
for another.

The second player runs a second copy of the game behind the scenes, linked to
the first as the two consoles were by the cable. It is started from the same
program file, so after a rebuild the game must be restarted before two players
can link again (the lobby says so).

## Saves

The game saves to memory cards, as on the console. Card 1 is
`memcard0.mcr` in the user's data folder (macOS:
`~/Library/Application Support/Psygnosis/WipeoutXL/`, Linux:
`~/.local/share/Psygnosis/WipeoutXL/`, Windows:
`%APPDATA%\Psygnosis\WipeoutXL\`), in the raw format emulators use.

## How it is built

[docs/PLAN.md](docs/PLAN.md) describes the design; [docs/IO.md](docs/IO.md)
the sound, CD, pad and card side.
