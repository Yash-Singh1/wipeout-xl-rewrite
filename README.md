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
emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DWIPEOUT_WEB_DISC="path/to/Wipeout XL (USA).cue"
ninja -C build-web
```

This makes `wipeoutxl.html`, `wipeoutxl.js` and `wipeoutxl.wasm`, and, as
wipeout-rewrite's web build has its data beside the page, the disc the page
loads by itself: `WIPEOUT_WEB_DISC` (your own copy) links the `.cue` and its
`.bin` tracks into `build-web/disc/` and writes `wipeoutxl-data.json`, which
names it. Put `build-web` on any web server with the links followed
(`cp -RL build-web site`), or try it with `python3 -m http.server` in
`build-web` and open `http://localhost:8000/wipeoutxl.html`. A host can also
put a disc there by hand: `disc/Wipeout XL (USA).cue` with its tracks is found
without a `wipeoutxl-data.json`, and the JSON (`{ "disc": "url/of/the.cue" }`)
can point anywhere on the site.

## Play

```sh
build/wipeoutxl --disc "path/to/Wipeout XL (USA).cue"
```

Started without `--disc` (from the desktop, say), the game opens the disc
chosen last time, or asks for the `.cue` with a file dialog. The debug console's
Files tab changes it: Browse... (the system's file picker; where the system has
none, the console's own) or a typed path, then "Restart with it now"; the game
starts with that disc from then on.

In the browser, the page loads the disc the site hosts by itself and the game
starts: the first visit downloads it (about 700 MB, so a desktop browser with
WebGL 2: Chrome, Edge, Firefox or Safari), and the browser keeps it, so later
visits start in a moment. Another disc image on the site can be given in the
address: `wipeoutxl.html?disc=path/to/Wipeout%20XL%20(USA).cue`, with the
tracks beside the `.cue`. Only a site without a disc asks for the player's own:
choose or drop the `.cue` together with all its `.bin` tracks, read into the
page's memory, never uploaded. Gamepads work through the browser's Gamepad API
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

Resolution (the debug console's Graphics tab, or `WIPEOUT_RENDER_SCALE=N`):
the game is drawn at a whole multiple of the console's 320x240, as many as the
window's height needs (Auto), or as many as you choose, up to 12x where the
graphics card allows. Above the window's own it is smoothed down to fit
(supersampling). The change takes effect at once.

`--window 1920x1440` sets the window size, `--fullscreen` starts full screen;
Alt+Enter or F11 switches while playing. The environment variables
The environment variable `WIPEOUT_XL_DISC` can stand in for the disc's path.

## Debug console

The ~ key (below Esc) opens a console over the whole game window: frame rates and a
frame-rate overlay, the draw distance (the console's or the whole track) and
its fade, the frame rate, widescreen, the resolution, HD textures, skipping the intro movie, full screen, the
frame recorder (Tab: the last 3 seconds, or from one Tab to the next, with every
primitive behind any pixel; while recording, its frames come in as they are drawn, `,` and `.` step back and
forward a frame, and − / + zoom the picture), and where the settings and the memory cards are. Changes are saved at once to
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

## The original Wipeout's tracks

The original Wipeout's seven circuits (Altima VII, Karbonis V, Terramax,
Korodera, Arridos IV, Silverstream, Firestar), each in its Venom and Rapier
version, can be raced in XL. In the debug console's Files tab, choose your copy
of the original PlayStation game: its disc image (a `.cue` with its `.bin`), or
a folder of its data (the disc's `WIPEOUT` folder). The RACING CLASS menu then
has a WIPEOUT line under the classes (arcade races and time trials); left and
right go through the tracks, each raced at the class it was made for. The race
is XL's: its ships, weapons, HUD and checkpoints (gates a third of a lap apart,
with time for a lap shared out among them). The original's track and scenery
files are turned into XL's as the race loads (`platform/original.c`); its scenery's
textures go into a second half of video memory, as the 2 MB development
PlayStation had, since they do not fit beside XL's own. Races on these tracks
keep no records or medals.

## Your own music

The debug console's Game tab can play a folder of your own music instead of the
game's soundtrack: MP3, FLAC, Ogg Vorbis and WAV files, in the folder and the
folders in it (`WIPEOUT_MUSIC=folder` for a run). Each time the game would start a
piece of its music (the menus', a race's) the next song plays, from its start;
when a song ends, the one after it. In the folder's order or shuffled; "Next song"
skips one. The game still runs its CD audio as before (pausing pauses the song,
its music volume sets the song's), only what is heard changes
(`platform/music.c`). The files are decoded as they play with miniaudio
(`platform/third_party/miniaudio`, public domain or MIT-0) and stb_vorbis
(public domain).

## HD textures

The Graphics tab's HD textures draw a texture pack made for DuckStation's
texture replacement in place of the console's textures. It was made for
**NR74W's WipEout 2097 / XL texture pack**
([github.com/NR74W/WipEout-Mods](https://github.com/NR74W/WipEout-Mods)), which
redraws the menus, fonts and HUD at 16 times the detail. The pack is NR74W's
work and does not come with the game: download it from there, then choose its
`.zip` as downloaded (or the folder you extracted it to, or DuckStation's
`textures/SCUS-94351` folder) with "Choose the pack's .zip..." and turn HD
textures on. Other packs for Wipeout XL (USA), SCUS-94351, made the same way
work as well (`WIPEOUT_HD_PACK=path` for a run).

A pack names each image after the texture it replaces, by hashes of the
texture's pixels and palette as DuckStation computes them; `platform/hdtex.c`
computes the same hashes for what the game uploads to video memory and draws
each image where its texture was, at the game's resolution (images are
decoded on a thread of their own the first time they are seen). PNG and JPEG
images are read with stb_image (`platform/third_party/stb`, public domain),
hashed with xxHash (`platform/third_party/xxhash`, BSD 2-clause). Images the
pack has in other formats (WebP, DDS) are not read. The pack's fonts carry the
console's own tiny ® marks inside some letters, drawn as crisply as the rest;
DuckStation shows them the same way.

## Wipeout 64's tracks

Wipeout 64's seven tracks (Klies Bridge, Sokana, Dyroness, Machaon II, Terafumos,
Velocitar and Super Combo) can be raced too. In the debug console's Files tab,
choose your Wipeout 64 cartridge image (`.z64`, `.n64` or `.v64`; nothing of it
comes with the game). Its tracks follow the original's on the RACING CLASS menu's
WIPEOUT line, at Rapier class. The Nintendo 64 game kept the PlayStation's file
formats, so each track's files come from the cartridge almost as they are
(`platform/original.c`): its two scenery parts (common, and for one player) are
made one, its textures get places in video memory (the N64 has none) in the
second half, palettes too, and each 64x64 surface tile becomes the 21 pieces XL
draws a tile from. The start line is the end of the track's starting grid; the
sky's height, which the N64 game keeps in its code, is the middle of the track's
heights. Which name goes with which track is assumed from the order the
cartridge holds them in.

## Saves

The game saves itself. The save in use is loaded at start-up (the one used
last; the first time, the first save on the cards, as the console did), and
whenever what a save holds changes (records and medals at the end of a race or
a lap, options, progress, unlocked classes) it is written again; with no save
yet the first change makes one, AUTOSAVE (`game/src/portsave.c`). The game's
LOAD/SAVE screen still chooses the save in use: load one, or save under a new
name. The debug console's Saves tab turns the autosave off and on, lists the
cards' saves (with "Use this one"), and imports saves from other emulators:
it finds Wipeout XL saves (and WipEout 2097's) in DuckStation's, RetroArch's
cores', OpenEmu's, Mednafen's, ePSXe's, PCSX-R's and PCSX-Redux's memory cards
(macOS, Linux and Windows places), or in a card or save file you choose: raw
images (.mcd, .mcr, .srm ...), DexDrive .gme, Connectix .mem/.vgs, the PSP's
.vmp, single saves (.mcs); an import copies the save onto card 1 and loads it
(`platform/saves.c`).

The memory cards themselves are as on the console. Card 1 is
`memcard0.mcr` in the user's data folder (macOS:
`~/Library/Application Support/Psygnosis/WipeoutXL/`, Linux:
`~/.local/share/Psygnosis/WipeoutXL/`, Windows:
`%APPDATA%\Psygnosis\WipeoutXL\`), in the raw format emulators use.

## How it is built

[docs/PLAN.md](docs/PLAN.md) describes the design; [docs/IO.md](docs/IO.md)
the sound, CD, pad and card side.
