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

- Your copy of the PlayStation disc (Wipeout XL USA, or WipEout 2097 Europe):
  a `.cue` with its `.bin` tracks, a `.bin` without its `.cue`, or an archive
  of either (`.7z`, `.zip`, `.rar`, `.tar`, `.gz` and the like). The music
  plays from the disc's audio tracks, the intro and ending movies from its
  movie files.
- To build: CMake, Ninja, a C compiler (Clang or GCC), and SDL3 3.2 or later
  (zlib for the test traces is optional). Where CMake does not find SDL3 it
  downloads and builds it (`-DWIPEOUT_BUILD_SDL=ON` always does). The same
  goes for libarchive (`libarchive-dev` on Linux, Homebrew's `libarchive` on
  macOS; `-DWIPEOUT_BUILD_LIBARCHIVE=ON` always builds it).

## Build

Each build has its own folder under `build/`. macOS (Homebrew's `sdl3`):

```sh
cmake -S . -B build/mac -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build/mac
```

It also makes `build/mac/Wipeout XL.app`, with the Mac version's icon and the
Homebrew libraries it uses inside (copy it to `/Applications`).

Linux (for example Debian 13 or Ubuntu 25.04: `libsdl3-dev libgl-dev`): the
same two commands with `build/linux`.

Windows, with MSYS2's MinGW-w64 (UCRT64 or CLANG64 shell, packages `cmake`
`ninja` `gcc` or `clang`): the same two commands with `build/windows`. From
macOS or Linux, with the MinGW-w64 cross compiler, use the toolchain file:

```sh
cmake -S . -B build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
ninja -C build/windows
```

`wipeoutxl.exe` is a single file, with SDL3 and the C runtime linked in and the
PC version's icon.

The web, with Emscripten:

```sh
emcmake cmake -S . -B build/web -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DWIPEOUT_WEB_DISC="path/to/Wipeout XL (USA).cue"
ninja -C build/web
```

This makes `wipeoutxl.html`, `wipeoutxl.js` and `wipeoutxl.wasm`, and, as
wipeout-rewrite's web build has its data beside the page, the disc the page
loads by itself: `WIPEOUT_WEB_DISC` (your own copy) links the `.cue` and its
`.bin` tracks into `build/web/disc/` and writes `wipeoutxl-data.json`, which
names it. Put `build/web` on any web server with the links followed
(`cp -RL build/web site`), or try it with `python3 -m http.server` in
`build/web` and open `http://localhost:8000/wipeoutxl.html`. A host can also
put a disc there by hand: `disc/Wipeout XL (USA).cue` with its tracks is found
without a `wipeoutxl-data.json`, and the JSON (`{ "disc": "url/of/the.cue" }`)
can point anywhere on the site.

## Phones and tablets (iOS, Android)

The same game runs on iPhone, iPad and Android, landscape and full screen (OpenGL ES 3), with touch controls
(below) and widescreen on from the start. SDL3 and libarchive are downloaded and built into the app.

iOS (Xcode; the simulator needs no signing):

```sh
cmake -S . -B build/ios -G Ninja -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator \
      -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build/ios wipeoutxl
codesign --force --sign - "build/ios/Wipeout XL.app"
xcrun simctl install booted "build/ios/Wipeout XL.app"
```

For a device, configure with `-G Xcode -DCMAKE_SYSTEM_NAME=iOS -DWIPEOUT_IOS_TEAM=<your team id>
-DWIPEOUT_IOS_BUNDLE_ID=<an id of yours>`, open the project in Xcode and run it on the device (the first
configure takes a while: Xcode checks every one of libarchive's tests). Put the disc image (the `.cue` with its
`.bin` files, or a `.zip`/`.7z` of them) in the Files app, On My iPhone (or iPad) > Wipeout XL; the game finds
it there, or says where to put it.

Android (the Android SDK with NDK 28 and Gradle 8 with JDK 17 or 21; `platform/android`):

```sh
cd platform/android
gradle assembleDebug        # app/build/outputs/apk/debug/app-debug.apk
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

At the first start the game looks for the disc image in its folder on the device's storage
(`Android/data/com.example.wipeoutxl/files`, reached from a computer over USB, or `adb push`); without one
it asks, and "Choose files..." picks the `.cue` with its `.bin` files (or a `.zip`/`.7z`) with the system's
picker and copies them in. On both, the debug console (the button at the top left) changes the disc later; the
new one is used from the next start. The two-player link modes need a second program, which a phone cannot
start: they find no partner, as on the web.

## Play

```sh
build/mac/wipeoutxl --disc "path/to/Wipeout XL (USA).cue"
```

Started without `--disc` (from the desktop, say), the game opens the disc
chosen last time, or asks for the `.cue` with a file dialog. The debug console's
Files tab changes it: Browse... (the system's file picker; where the system has
none, the console's own) or a typed path, then "Restart with it now"; the game
starts with that disc from then on.

WipEout 2097 (Europe, SLES-00327) works too: the same game on a disc with its
own program, whose data is Wipeout XL's file for file but for the pieces the
program chooses by region. With that disc the game shows them
(`platform/region.c`): the WipEout 2097 title screen, copyright and Red Bull
screens, the menus' backdrop and logo, the piracy warning, and the movies at 25
frames a second. Their pictures were made for the European television's 256
lines and are fitted to the 240 the game draws, keeping their shape.

The disc image can be given as it is kept (`platform/discimage.c`):

- a `.cue` with its `.bin` tracks;
- a `.bin` (or `.img`) without its `.cue`: when a `.cue` beside it names it,
  that one; otherwise a `.cue` is made for it, with the audio tracks beside it
  when they are named as Redump names them (`Game (Track 02).bin` and on);
- an archive of either: `.7z`, `.zip`, `.rar`, `.tar`, and single files or
  tars compressed with gzip, bzip2, xz or zstd (`--disc "Wipeout 2097
  (Europe).7z"`, or the Files tab). Archives are known by their contents, not
  their names. The archive is unpacked once into the settings folder
  (`discs/<its name>/`, about 700 MB) and used from there while it stays the
  same. The disc image in it is found by what it is (a PlayStation disc's data
  track), not by a checksum, so a modified disc works as well. 7z archives are
  read with the LZMA SDK's decoder (public domain, `platform/third_party/lzma`),
  the others with libarchive (BSD).

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
(VSync), a cap in frames per second, or the PS1's original 30 frames a second
without the frames in between (`WIPEOUT_FRAME_RATE=uncapped|vsync|console|N`).

A depth buffer (on by default; the Graphics tab's Depth buffer, or
`WIPEOUT_DEPTH_BUFFER=0|1`) keeps far polygons from being drawn over nearer
ones: the console sorted each polygon by its average depth, so a long wall or
a hillside could show through the track in front of it. Polygons at about the
same depth (a shadow or a pad on the track, a sign on its board) keep the
game's own order.

Widescreen (the debug console's Graphics tab, or `WIPEOUT_WIDESCREEN=1`): in a
window wider than 4:3 a race fills the whole width, showing more of the scene at
the sides, with the HUD at the window's edges. In the menus the backdrop is
enlarged to fill the width; the menus keep the console's 4:3 layout, and their
panels slide out to the window's edges and back between screens.
The loading screens fill the width with their background, their text and
pictures in the middle; the title screens and movies stay 4:3. The game
computes exactly what it did.

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
its fade, the frame rate, widescreen, the resolution, HD textures, skipping the intro movie and the start-up logos, full screen, the
frame recorder (off by default; up to 240 pictures a second, the ones between the game's frames too; Tab: the last 3 seconds, or from one Tab to the next, with every
primitive behind any pixel; while recording, its frames come in as they are drawn, `,` and `.` step back and
forward a frame, and − / + zoom the picture, kept at the resolution it was drawn at), and where the settings and the memory cards are. Changes are saved at once to
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

Touch controls (a phone or tablet, a touch screen, or the web page on one): a stick on the left that appears
where the thumb lands (steering, the nose, the menus' cursor); on the right in a race THRUST, FIRE, the L and R
air brakes, DROP and VIEW (a thumb between two presses both), in the menus the four PlayStation buttons; at
the top pause (Start) and the debug console. Each race button performs its action whatever button the
controller screen bound it to. The debug console's Game tab sets them Automatic (on a phone from the start,
elsewhere from the first touch; a key or gamepad hides them until the next touch), On or Off, and their size
and opacity (`WIPEOUT_TOUCH=0|1|2` for a run).

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
starts. Each half shows the lower part of its player's view, with the whole
ship, and a smaller HUD in its corners; with widescreen on, each half fills the
window's width. Both halves are drawn between the game's frames as a single
player's race is (the frame rate setting). After the race the lobby comes back
with the same players, for another.

The second player runs a second copy of the game behind the scenes, linked to
the first as the two consoles were by the cable. It is started from the same
program file, so after a rebuild the game must be restarted before two players
can link again (the lobby says so).

## The original Wipeout's tracks

The original Wipeout's seven circuits (Altima VII, Karbonis V, Terramax,
Korodera, Arridos IV, Silverstream, Firestar), each in its Venom and Rapier
version, can be raced in XL. In the debug console's Files tab, choose your copy
of the original PlayStation game: its disc image (a `.cue` with its `.bin`, or an archive of it), or
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
