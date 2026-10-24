# The PC CD-ROM version's data

The Windows version of Wipeout XL / WipEout 2097 (Psygnosis, 1997; `D3D.EXE`) is the PlayStation game
with Direct3D drawing, keyboard and DirectSound. Its CD holds the same game's data in another
arrangement. The port plays from it with the PC version's own front end: its title, menus, HUD and
text, taken from the PC decompilation (`wipeout-xl/decomp-release/pc`). The race underneath is the
PlayStation code the port runs for every disc.

## What the PC CD holds

| Folder | Holds | Against the PlayStation disc's `\WIPEOUT2` |
|---|---|---|
| `COMMON` | ships, weapons, droid models (`.prm`, `.cmp`), `MENU.DAT` | most byte-identical; `MENU.DAT` is the PC's; no menu or attract models |
| `TRACKnn` | `TRACK.WAD`, `TRACK.TEX`, checkpoints, `TRACKTEX.CMP` | the track's geometry and textures identical; some scenery differs |
| `TEXTURES` | a few TIMs and CMPs (shadows, smoke, explosions, birds) | identical; the other 80 or so (menus, HUD, fonts, logos, effects) are not there |
| `SOUND` | each sound effect as a WAV | the PlayStation has them in one `samples.wad` |
| `PICS\HI-RES`, `PICS\LO-RES` | the 2D and per-track texture atlases (TGA, 256x256 pages) | the PC front end's and races' texture pages, see below |
| `BLITSURF` | the start and copyright pictures (PPM) | |
| `MPEG` | the intro and the four endings (MPEG-1) | the PlayStation's are its own movie format |
| CD audio | the music | other tracks than the PlayStation's |

The PC version draws everything from 256x256 texture pages addressed by number (`d3dpoly.c`:
`AdjustTexturePage`; a PlayStation texture page code 5-15 or 21-31 becomes page 0-10 or 11-21).
`NRACEnn.TGA` is a track's eleven pages (`racectrl.c`: `Load16BitBitmapTextures(0, "nrace0N.tga", 0,
11)`), `SKYnn.TGA` its sky (page 17), `MENUHF.TGA` the menus' eleven; `BACKGRND`, `TESTMENU`, `MENUKEY`,
`HUD2`, `XLLOGO`, `YFONT` the front end's (pages 0, 1-3, 6-7, 22-24, 25, 26; `main.c`). The pages are
the PC's own arrangement of the textures, not the PlayStation's video memory, and the PC's race loads
other files than the PlayStation's for some of them (track 1's Red Bull boards are `TEXTURES\TH.TIM`,
a track's whole textures `TRACKnn\TRACKTEX.CMP`, 128x128 each, in place of the tiles of `library.cmp`).

## How the port uses it

- `platform/pc.c` recognises the PC data (in a disc image, or a folder holding the CD's files) and
  answers the game's file layer for it (`PortPcFileInfo`, `PortPcRead`, as `saturn.c` does for the
  Saturn's disc): a file the PC has under the same name is passed through, `samples.wad` is made
  from the WAVs.
- `portPc` (`game/include/port_platform.h`) is set when the PC data is in use. `PsxMain` hands over
  to `PcMain` (`game/src/pc/`), the PC version's flow: copyright picture, intro, title, the PC menus,
  then the race. Where the PC's race loads other files or draws its HUD and text differently, the PC
  decompilation's version of that function runs (`game/src/pc/`); the race itself (ships, physics,
  drones, weapons) is the code every disc runs.
- The PC front end (`game/src/pc/`) is the PC decompilation's code for it, kept to its shape, with
  its drawing calls going to `pcdraw.c`. That turns the PC's textured quads (PlayStation `POLY_FT4`s
  in the console's 320x240 space, textured from a page of an atlas) into the port's packets,
  textured from an atlas page that the GPU layer holds as a full-colour image.
- The controllers stay the port's: the PC's keyboard prompts become the device's buttons and names
  (keyglyphs.c), as on the PlayStation disc.

## Race textures, loading, movies and music

- `startwad.wad` (the PlayStation keeps the weapons, start droid, explosions and lights there) is made by `pc.c` from
  the PC's own files in the same order (`MakeStartWad`); what the PC draws from its own pages (HUD gauges, fonts) is
  empty. The PC arranges video memory differently from the PlayStation, so a PC texture file is moved to the
  rectangles the PlayStation's file of the same name has (`PlaceFile`), or it would overwrite the track's textures
  (the PC's `LIGHT.CMP`, `CHROME.TIM`, `TH.TIM`).
- The PC race loads one picture for the Red Bull boards of track 1 (`TH.TIM`; `pc.c` serves it as `alphared.cmp`) and
  animates nothing on the other tracks (their boards, video screens and torches stay as the track's wad draws them);
  `racectrl.c`'s `InitRedBull` does as the PC does.
- The loading screen is the PC's text and bars (`game/src/pc/pcload.c`), drawn by `VsyncFunc`.
- The movies are the PC's MPEG-1 files (`platform/pc_movie.c`, pl_mpeg for the pictures). The sound is not MPEG audio:
  it is in the user data after each picture header, as 18-byte frames of differential codes.
- The music is the CD's audio tracks after the data track, played as the PlayStation's are; the song names are the
  PC's (`game/src/pc/pcmusic.c`). A copy without audio tracks has no music (`cdMusicCount` 0, as the PC's own check).

## Copies people have

Redump's BIN/CUE (data and audio tracks), a bare ISO of the data track, CloneCD (`.ccd`/`.img`),
Alcohol 120% (`.mds`/`.mdf`), CHD, and any of these in a ZIP, 7z or RAR; or a folder with the CD's
files (an installed copy, or the CD copied). Without the audio tracks the game plays without music.
