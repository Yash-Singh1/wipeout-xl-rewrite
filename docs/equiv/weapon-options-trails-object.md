# The four equivalent-only functions in weapon.c, options.c, trails.c, object.c

`GetNewWeapon`, `InGameReset`, `DrawFlare`, `DrawEnvMap`. The 32-bit offsets below are the
console's (MIPS o32); each was matched against `python3 tools/layouts.py show TYPE`. Natively
all the types involved are pointer-free (their host layout equals the console one, except where
a table says otherwise), so no raw offset survives.

Results: `tools/diag.py` prints nothing for weapon.c, options.c, trails.c, object.c and ayrace.c
(the only other file touched); `tools/xdecl.py` has no entry for these functions.

Oracle method. `python3 tools/oracle32.py diff B0 ...` and `ORACLE_OPT=-O0 ... diff B0-O0 ...` compare
the new text with the original, compiled for MIPS o32. Besides the tool's output I used three
private checks (scratch scripts in /tmp, not in the repository):

1. a second copy of the previous text (with the one planned change applied, old headers) compiled
   beside the new text and compared instruction for instruction (branch targets are function-relative);
2. the same comparison with frame offsets (`N($sp)`, `N($fp)`) and branch targets normalised, to read
   -O0 differences;
3. for DrawEnvMap, a variant of the previous text with only the `StaticRVert` change applied, to isolate
   the pointer-cursor change.

## GetNewWeapon (weapon.c)

Text unchanged by me: it already uses only real types and fields (`ShipData::weapon`, `lastWeapon`,
`ConfigData::weaponsOn`, `actTrackNum`, the globals `playerPosition`, `remoteRacePos`, `shipDamage[]`,
`weaponChance[][11]`). No private type, no raw offset, no alias variable. `weaponChance` is `u_char`
with an 11-byte row (10 thresholds and a zero), the same natively.

Oracle:
* -O2: identical to B0 (no CHANGED entry).
* -O0: identical except two loads, `lhu` becomes `lh` for `playerPosition` and `remoteRacePos`. The
  original text declared them `u16` locally; they are now the shared `short` of globals.h. Behaviour
  is the same: the loaded value is only used as `(short)position == 1`, `(u_short)(position - 2) < 3`,
  `(u_short)(position - 5) < 4`, `(u_short)(position - 9) < 3` and `(short)position >= 12`, which look
  only at the low 16 bits.

Uninitialised reads kept (clang reports exactly these three):
| variable | when it is read uninitialised | reachable? |
|---|---|---|
| `position` | `shipNo` is neither `ownShip` nor `remoteShip` and is below 12 | Not from the game: the callers (pad.c cheat code with `ownShip`; weapon.c `CycleWeaponGrids`, guarded by `i == ownShip \|\| i == remoteShip`) never do it. |
| `group` | `position` (as a short) is below 1: none of the five tests (`== 1`, 2-4, 5-8, 9-11, `>= 12`) holds. Then `position = (short)group` is garbage and indexes `weaponChance` out of range. | Only if `playerPosition` / `remoteRacePos` is still 0 or negative when a pick-up is taken (the globals start at 0 and are set by the HUD / the link). On the console this read stack garbage from the 5 x 11 byte table's neighbourhood; natively it is an undefined stack value, so a huge index could fault. Not made safe, to stay faithful. |
| `weapon` | `retry` loop: `chance` at or above every threshold of the row | Not with the real table. Rows (column 9 is the last tested threshold): `{15,30,40,45,65,70,75,80,85,100}`, `{15,25,35,45,60,65,70,75,85,100}`, `{15,30,35,45,55,60,65,75,90,100}`, `{15,30,35,40,45,50,55,70,90,100}`, `{25,35,40,45,45,50,55,80,100,100}`. `chance` is `randWep() % 100` (-99..99), so some branch always assigns `weapon`. |

Other quirk kept: `shipNo >= 12` clears `shipIndex[shipNo].weapon` (the global array), not `ships[shipNo]`.

## InGameReset (options.c)

Private types removed from options.h: `PauseState` and `OptionsPolys`. The function now takes the real
types: `InGameReset(struct PauseMenu *pause, ConfigData *game, struct InGameMenu *polys)`.

### PauseState -> PauseMenu (ayrace.h), 32-bit offsets

| PauseState field (offset, size) | real field | used by InGameReset |
|---|---|---|
| `choice` (0, 4) | `PauseMenu::selection` (0, int) | yes |
| `cdPause` (4, 4) | `musicPaused` (4, int) | yes |
| `pad08[8]` (8..15) | `command` (8), `count` (12) | no |
| `counter` (16, 4, unsigned) | `frameCount` (16, **int**) | yes |
| `pad14[3]` (20..22) | `pad14` (20), `active` (21), `showOptions` (22) | no |
| `machinePaused` (23, 1) | `stage` (23, u_char) | yes |
| `confirmed` (24, 1) | `confirmed` (24) | no |
| `selection` (25, 1) | `confirmChoice` (25) | no |

Total size 28 in both. The only type difference is `counter` (unsigned) against `frameCount` (int). The
function takes `counter % 30` (5 sites), so the text writes `(u_int)pause->frameCount % 30`; the other
uses (`& 1`, `++`) do not depend on the sign. Without the cast a negative count (after 2^31 frames) would
use a signed remainder.

### OptionsPolys -> InGameMenu (hud.h), 32-bit offsets

| OptionsPolys field (offset) | real field (offset, size) | notes |
|---|---|---|
| `pad0[232]` (0) | `pad0` (0, 232) | the first bytes are the `struct SelectBox` that `DeSelectScreen` uses; unused here |
| `box[2]` POLY_G4 (232, 2 x 36) | `footer[2]` POLY_G4 (232) | equal |
| `tile[2]` TILE_1 (304, 2 x 12) | `textureWindow[2][12]` (304, 24) | `InitInGameMenu` sets these bytes up as `DR_MODE` (12 bytes); the old view called them TILE_1. `AddPrim` only reads/writes the first word (the tag), so passing `textureWindow[CurrentScreen]` is the same |
| `musicBars[20][2]` POLY_G4 (328) | `red[20][2]` (328) | passed to `UpdateMusicBar` |
| `sfxBars[20][2]` POLY_G4 (1768) | `green[20][2]` (1768) | passed to `UpdateSfxBar` |

Total 3208 in both. No access past the struct and no ambiguous view remains. The block that is passed
in is `RaceLocals::inGameMenu` (a `u_char[3312]` in racectrl.c, coordinator): it is 104 bytes longer
than `InGameMenu`, and it must be 4-aligned natively (the packets hold `u_long` tags). I did not check
that offset.

### Other edits
* `PrintCX(..., 8)` is called with three arguments (3 sites); the callee takes three and never read the 8.
* ayrace.c `PerformPauseOperations`: the call now reads `InGameReset(pause, config, (struct InGameMenu *)box)`
  (the old call cast to the two private types). `box` (declared `struct SelectBox *`) is the start of the same
  in-game menu block. This is the only edit outside my four files besides the headers.
* functions.h: prototype and the two forward declarations; options.c includes ayrace.h and hud.h.

### Oracle
* -O2 against B0: CHANGED, and the only instruction differences are the dropped `addiu $7, $zero, 8` at the
  three `PrintCX` calls (the `addiu $6` moves into the delay slot) plus function-relative branch offsets.
* -O0 against B0: CHANGED, same cause. The frame is 8 bytes smaller because at -O0 the fourth argument was
  saved in a stack slot across the call (`sw $7` / `lw $7`, two of the three sites), and every local offset
  moves by that; with offsets normalised the only differences are those lines.
* Strongest check: the previous text with only the `PrintCX` fourth argument removed (old headers) compiles to
  **identical** code to the new text, at -O2 (1301 lines) and -O0 (2070 lines). So the type change alone is
  invisible to the machine code.

### Kept, for the report
* `cdMusicIndex = (cdMusicIndex + 1) % (cdMusicCount - 1)` (two sites): division by zero if the disc reports
  one track. x86 traps, MIPS did not. Not guarded.
* `const unsigned char pauseMenuGap;` and `s32 restartText[2];` are zero-initialised tentative definitions in
  options.c. On the console they hold 12 (`pauseMenuGap`, the row spacing) and the bytes "RESTART\0". They
  must come from the data generator, or all four menu rows would be drawn at the same y and the second
  entry would be empty. I did not change them.
* `options[4]` (`MenuText`) is filled by `memcpy` of 9, 8, 15 and 13 bytes from strings; the rest of each 16
  bytes is never read before the terminating zero.

## DrawFlare (trails.c)

Text unchanged. Identical to B0 at -O2 and -O0 (not listed as CHANGED).

* `flare` is `POLY_G4 flare[19][12][2]` (POLY_G4 is 36 bytes, no pointers: the same natively). The function
  addresses element (ship, k, CurrentScreen) as `(char *)&flare[0][k][0] + CurrentScreen * sizeof(POLY_G4) +
  ship * sizeof(flare[0])` for the writes, and `&flare[0][k][0] + ship * 24 + CurrentScreen` for `AddPrim`.
  `sizeof(flare[0])` = 24 x 36 = 864, so both forms equal `&flare[ship][k][CurrentScreen]`; I checked this
  with the host sizes. Both are right natively.
* I tried writing them as `&flare[ship][k][CurrentScreen]`; the machine code is no longer identical
  (the per-site multiplications are no longer shared), so I left the proven text.
* Nothing is read past an array for `ship` 0..18; `ship` comes from the trail loops (I did not trace the maximum).
  `ship < 15` selects `&shipIndex[ship]` (a real array index). No uninitialised reads.

## DrawEnvMap (object.c)

Edits:
* The primitive cursors were 32-bit integers. They are now pointers: `EnvMapWork::nextPrimitive` is
  `F4 *volatile` (was `volatile u32`), and the locals `primitiveAddress` (renamed `primitiveCursor`) and
  `triangleNext` are `F4 *`. `F3` and `F4` are both 16 bytes (checked), so `+ sizeof(F4)` became `+ 1`
  and `((Primitive *)primitiveAddress)->type` became `primitiveCursor->type` (`type` is the first short of
  both). `(F3 *)ws.nextPrimitive - 1` and `(F4 *)ws.nextPrimitive - 1` are unchanged.
* The `#define nextPrim nextPrim.ptr` / `#undef` bridge is deleted; the function uses `nextPrim.ptr`.
* `StaticRVertY[i][0]` / `StaticRVertZ[i][0]` (their externs are deleted) are `StaticRVert[i].vy` /
  `.vz`. `VECTOR` is 16 bytes with `vy` at 4 and `vz` at 8, which is the old alias arithmetic.
* A comment on `EnvMapWork`. Its `gapNNN` members are never referenced (they stand for unused frame slots),
  and it is a local, so its native size (272 against 260) and the moved pointer members do not matter.

Field mapping table:

| old | new | 32-bit offset / size |
|---|---|---|
| `EnvMapWork::nextPrimitive` (volatile u32) | `F4 *volatile nextPrimitive` | 256, 4 on the console (a stack local) |
| `u32 primitiveAddress`, `u32 triangleNext` | `F4 *primitiveCursor`, `F4 *triangleNext` | locals |
| `StaticRVertY[i][0]` | `StaticRVert[i].vy` | element stride 16, field offset 4, int |
| `StaticRVertZ[i][0]` | `StaticRVert[i].vz` | element stride 16, field offset 8, int |
| `nextPrim` (via the macro) | `nextPrim.ptr` (`PrimPtr::ptr`, `char *`) | 0, 4 |

Oracle:
* The pointer-cursor change alone: a copy of the previous text with only the `StaticRVert` change, compiled
  next to the new text, is **identical** at -O2 (1657 lines) and -O0 (2352 lines).
* The `StaticRVert` change alone, at -O0: the only differences are that `lui/addiu StaticRVertY`+`lw 0($1)`
  becomes the shared `StaticRVert` base plus `lw 4($1)` (and `lw 8($1)` for z), with the loads rescheduled
  around it. The three symbols are the same storage (the base address plus 4 and 8, as the console's symbol
  addresses show). At -O2 the shared base also removes some register pressure, so the frame shrinks from 0x178
  to 0x160 bytes: this is the expected scheduling change.
* Against B0, the remaining differences are not mine: `lui 0x1f80` becomes `PsxScratchpad` (coordinator),
  and the removal of the `volatile` on `nextPrim` (the earlier group-5 edit).

Kept as they are on the console (report):
* `StaticScreen[140]`, `StaticFlags[140]`, `StaticRVert[140]` (16 bytes each) and `StaticDepth[148]` are the
  sizes of those arrays in the console's memory map (gaps between neighbouring symbols). An object with more
  than 140 vertices overruns them (and `StaticRVert` / `envMapU` / `envMapV` are written for every vertex, even
  when the 128-vertex scratchpad is used). The native arrays must have at least the same sizes; the data
  generator defines them.
* A primitive that is neither type 1 (`F3`) nor type 3 (`F4`) is skipped without advancing the cursor, so the
  loop processes that same entry again `primitiveCount` times (harmless but kept).
* The scratchpad path (`count <= 128`) splits `PsxScratchpad` into 128 `DVECTOR` at 0, 128 `u_short` at 0x200,
  128 `u_short` at 0x300 (fits the 1024-byte array).
* The volatile accesses on the locals and the `*(volatile short *)` stores on the packets are as in the proven
  text; they do not change native behaviour. The function reads the `u3` byte of `ws.lastVertex` through a
  byte pointer (little-endian assumed, as on the console and the host).
* There are no uninitialised reads that matter: all locals the code reads are assigned first.

## Files edited
weapon.c: none. trails.c: none. options.c (InGameReset, includes), object.c (DrawEnvMap, externs, comment),
include/options.h (private types removed), include/functions.h (InGameReset prototype, two forward
declarations removed), ayrace.c (one call and its comment).
