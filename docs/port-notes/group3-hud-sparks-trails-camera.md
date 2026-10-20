# Group 3: hud.c, sparks.c, trails.c, camera.c

Result: `tools/diag.py` prints nothing for all four files (also clean with `-Wall`).
`tools/xdecl.py` lists no VAR/FUNC/DEF/IMPL entry that involves these files except the
typedef-spelling pair `cdMusicTable` (`u_char[]` vs `unsigned char[]`, same type) and the
sign-only differences of other workers' files listed below. `DrawFlare` (trails.c) is
unchanged; its text is byte-identical.

## How the oracle was read

`tools/oracle32.py diff B0 ...` lists many functions as CHANGED that did not change: its
normalisation makes `jal` targets relative to the function start, so every function that
follows a function whose code size changed shows `jal @-xxxx` differences. After
normalising `jal` targets (and, at -O0, branch offsets) the real changes are the ones
below. I also compiled the base tree (commit 24b0988, the B0 baseline) and the working tree
at -O0 with the oracle's flags: at -O0 the optimiser cannot reorder or CSE, so the diff
shows exactly what the edit does. Result: every pointer-form edit differs at -O0 only in
the order in which two independent loads (base pointer, index) are emitted. The -O2 diffs
of those functions are instruction scheduling / CSE differences, because LLVM treats a
`getelementptr` access differently from the old `inttoptr(int + int)` access when it decides
whether a store may alias `CurrentScreen`.

## hud.c

Edits:
- Pointer/integer casts (rule 1). `(POLY_FT4 *)(quadIndex * 40 + (int)quads)` became
  `(POLY_FT4 *)((char *)quads + quadIndex * 40)` (40-byte pointer-free primitive, byte
  arithmetic kept): AddNumber, AddJustifyNumber, AddDelEnd, CentreText, CentreText2.
  `SPEEDO_QUAD(part)` is now `speedo->part[CurrentScreen]` (fill_speedo).
  `DisplayContinues` uses `CONTINUE_QUAD(i)` (= `&continuePolys[i][CurrentScreen]`).
- Undefined symbols. The stores through `quadClutField` / `quadTPageField` (symbols of the
  console image that lay inside `quadPool`, at byte 14 and 22 of a quad; the data tree
  defines them as `u_short [4]` and `[11989]`, ending exactly at the end of the 600-quad
  pool) are now `((POLY_FT4 *)((char *)quads + quadOffset))->clut` / `->tpage`. Their
  externs are removed. AddNumber, AddJustifyNumber, AddDelEnd, CentreText, CentreText2.
- Prototypes (rule 5). `__asm__("humanToDroneSpeed")` / `__asm__("Print")` aliases removed;
  UpdateRaceHud calls `humanToDroneSpeed(shipIndex, i)`, `humanToDroneSpeed(shipIndex,
  ownShip)` (third argument dropped: the callee has two parameters), `Print(trackNameText,
  16, y, 8)` (fifth argument `1` dropped: Print has four) and `PrintCX(msgRearLock, 168, 68)`
  (fourth argument `8` dropped: the definition takes three). Each is commented `Port:`.
- `UpdateRaceHud`: `nearTrkSect->junction.ptr == (TrackSection *)-1` (two tests) became
  `junction.index == -1`. Both mean "the 32-bit slot is -1"; natively the pointer member is
  8 bytes and the loader (track.c) only guarantees the low 32 bits are -1 (it tests
  `junction.index != -1` itself). Object code is identical.
- Declarations: removed the local externs that globals.h now declares (see below), and
  `continueAnimFrame` is `short` (ayrace.c, the owner, defines it `short`; hud only reads
  `32 - continueAnimFrame`, value 0..30).

Oracle (-O2, jal-normalised) real changes:
- AddNumber, AddJustifyNumber, AddDelEnd, CentreText, CentreText2: the clut/tpage store was
  two `sb` (the console compiler could not assume alignment of a cast symbol + offset) and
  is now one `sh` at quad+0xE / +0x16 (little endian, same bytes, address always even); the
  reloc `quadClutField`/`quadTPageField` is gone; the rest is load order and register
  allocation. Confirmed at -O0: the only store differences are exactly those stores.
- fill_speedo: 379 -> 788 lines. The old `(Speedo *)(CurrentScreen*40 + (int)speedo)` was
  computed once per run of stores; with `speedo->part[CurrentScreen]` LLVM reloads
  `CurrentScreen` after every halfword store (it cannot prove the store misses the global)
  and no longer merges the `x0,y0` stores into one word. At -O0 the sequence of
  non-stack stores and calls is identical to the base.
- UpdateRaceHud: only the dropped argument setups (one stack store for Print's fifth
  argument, three argument register loads). Verified by restoring the old call forms: the
  function then compiles identically to the base.
- DisplayContinues (changed at -O0 only): operand evaluation order.

## sparks.c

Edits:
- DrawBirds: `(Bird *)(n * sizeof(Bird) + (int)birds)` -> `&birds[n]`;
  `(POLY_FT4 *)((n*2 + CurrentScreen) * sizeof(POLY_FT4) + (int)birdPrims)` ->
  `&birdPrims[n * 2 + CurrentScreen]` (twice).
- `reserveHeap` was `void *` (rule 2): now `HeapBlock *` via globals.h. Local externs of
  `heap`, `cameraPtr`, `OT`, `TextureTableCount`, `textureWad` removed (globals.h).

Oracle: DrawBirds (pointer-form edit; at -O0 only load order differs), LoadBirdGFX and
LoadExplodeGFX (`TextureTableCount` is `unsigned short` in globals.h, owner ships.c: `lh`
became `lhu`; the count is below 32768).

Checked and left: all DAlloc sizes already use `sizeof`; the primitive/explosion pools are
pointer-free (`explodePrims` is addressed by `sizeof(POLY_FT4)` steps, correct natively).
`SParticle` (68), `Explosion` (24), `Ring` (24), `Bird` (32), `Electric` (156) have the same
size and offsets on both layouts (`layouts.py show`).

## trails.c

Edits:
- `DAlloc(reserveHeap, 0x8700)` -> `15 * sizeof(Trail)` (0x8700 = 15 * 2304 exactly, no
  slack in the original; `Trail` is 2304 bytes on both layouts).
- DrawFlashNozzle: `(ShipData *)(shipNum * sizeof(ShipData) + (int)shipIndex)` (a real bug
  natively: ShipData is 256 bytes there) -> `&shipIndex[shipNum]`.
- The local `RotTransPers3` prototype duplicated psyq.h: removed. Local externs of `OT`,
  `reserveHeap`, `sillyShips` removed (globals.h).

Oracle: DrawFlashNozzle 936 -> 1179 lines at -O2 (same cause as fill_speedo: the pointer
is now a GEP so loads of the ship's vectors are no longer CSE'd across the stores into the
local `ws`); at -O0 only the load order of the base and index differs.

## camera.c

No code change. Removed the local externs of `CameraUpdate` and `tempGameData` (globals.h).
No oracle difference. Audited: no sizes, no raw offsets; TrackCamera/TrackSection/ShipData
are only used through fields.

## globals.h (group 3 section)

Added: `CameraUpdate`, `cameraPtr` (`TrackCamera *`), `finishPosition` (short),
`linkSendFlag` (int), `OT`/`OT2` (`u_long *[]`), `pauseState` (int), `playerPosition` (short),
`reserveHeap` (`HeapBlock *`), `teamShip` (`Object *`), `tempGameData` (`ConfigData *`),
`textureWad` (`WADFILE *`).
Other workers' files that still disagree and will now fail to compile until they drop their
local extern: racectrl.c (`CameraUpdate`, `tempGameData`, `cameraPtr`, `teamShip`,
`pauseState`, `OT`, `OT2`: coordinator's), menus.c (`teamShip` is `int`, `textureWad` `int`,
`OT2` `s32[]`), combat.c (`finishPosition`/`playerPosition` are `unsigned char`,
`linkSendFlag` `unsigned char`, `cameraPtr`/`OT` types), effects.c (`textureWad` `void *`,
`cameraPtr`), weapon.c (`cameraPtr` `int *`, `playerPosition` `unsigned short`), progress.c
(`reserveHeap`). Remaining sign-only disagreements, left alone: `numTrackSections`
(drone.c, ships.c unsigned), `remoteRacePos`/`playerPosition` (weapon.c unsigned).

## functions.h

One shared edit: `int PrintCX();` -> `void PrintCX(u_char *text, int x, int y);`. The
definition (menus.c `PrintCentred`, label `PrintCX`) takes three parameters and returns
nothing. **Every caller in menus.c and options.c passes a fourth argument (`8`) and now
errors until it is dropped** (the same `Port:` drop I made in hud.c). The menus.c alias
`PrintCX__L` / `PrintCentred` still needs resolving by its owner.

## Rules 7-10

- Rule 7: none (every `return;` is in a `void` function).
- Rule 8: none.
- Rule 9: none in these files. They do rely on the loaded track being native: `section->
  nextSection.ptr`, `prevSection.ptr`, `centre`, `snow`, `junction.index == -1` (hud.c
  UpdateRaceHud, AlterCheckPoint; sparks.c AddSnow/AddRain; camera.c) and on `shipIndex[i]`
  being an array of native `ShipData`.
- Rule 10:
  - sparks.c, division by the projected depth: DrawBirds `51200 / depth`, DrawRing
    `51200 / depth`, DrawMissileFlare / DrawRocketFlare `51200 / depth`, DrawExplode
    `(0x19000 >> size) / depth`. All come after `if (flag & 0x80460000) continue/return`;
    depth 0 sets GTE flag bit 17/18 so these are safe only if the platform's
    `RotTransPers` returns exact GTE flags.
  - hud.c fill_speedo: `ships[ownShip].thrust / (short)(ships[ownShip].maxThrust / 104)`
    divides by zero if `maxThrust < 104`; `speed / dividers.value[shipClass]` divides by
    table data. The console did not trap.
  - hud.c AddTime / AddText / AddNumber / AddJustifyNumber / CentreText / CentreText2: the
    `switch (size)` has no default, so `charSet` / `charTexture` are uninitialised for a size
    other than 8, 12, 16 (faithful; all callers pass those).
  - hud.c AddTime with a negative time (checkTimer <= 0): `%` and `/` give negative digits,
    `font[digit + 26]` reads glyphs below index 26 (faithful, no out-of-bounds).
  - sparks.c DrawBirds clamps `vy` to +-160 when it exceeds +-320 (the other axes clamp to
    +-320). Looks like the original's behaviour; kept.
  - trails.c UpdateTarget: `if (((u_int)value * 32) & 31) radius++;` is never true (dead).
  - hud.c: `quadIndex` is never checked against the 600-quad pool.
  - No float/double, no shifts >= 32, no unaligned casts.

## Things the coordinator must decide / watch

1. Duplicate and zero-initialised definitions. These files define as tentative `int x;`
   globals that the data tree (`wipeout-xl-ps1-data/game/data/*_data.c`) also defines, some with
   non-zero initial values that this tree would zero:
   hud.c: `speedoFrameTex`, `speedoSpeedTex`, `speedoShieldTex`, `speedoMarkerTex`
   (hud_data.c), `raceInfoFrame`, `classNameVector[7]`/`classNameVenom[6]`/
   `classNameRapier[7]`/`classNamePhantom[8]` (raceinit_data.c initialises them to "VECTOR",
   "VENOM", "RAPIER", "PHANTOM"; here they are zero, so the class name in the award text
   would be empty), `bonusFlashFrame`, `bonusFrames`, `trackNameScroll` (**data: -9999**),
   `energyBlinkFrame`, `trackNameOffset` (**-9999**), `hudFrame`.
   sparks.c: `snowCount`, `birdDelay`, `birdSectionA/B`, `birdPrims`, `birds`, `missflareat`,
   `rockflareat`, `rockang`, `pitsparkat`, `exdelay`, `partpos` (sparks_data.c).
   trails.c: `shipTrails`, `plasmaPhaseA/B/C`, `flashNozzleShip` (**-1**), `plasmaShotBusy`,
   `lastTargetShip` (**-1**), `targetBank`, `targetSpin`, `targetvalid` (trails_data.c).
   camera.c: `cameraTarget`, `raceExternalVel`, `droidViewVel` (ships_data.c).
   When the data tree is merged these must become one definition each (I did not change
   them: which side owns them is your call). Until then `trackNameScroll`, `flashNozzleShip`
   and `lastTargetShip` start at 0 instead of -9999 / -1.
2. Data definitions generated with a too-small size that the game indexes further:
   `quadPool[1]` (hud.c indexes 600), `fontGlyphs16[1]`, `fontGlyphs8[1]` (40 glyphs each;
   `fontGlyphs12[40]` is right), `deleteTextures[1]` (indexed by `hudTexIndexAlt`), and the
   `quadClutField` / `quadTPageField` objects, which are no longer referenced and can be
   dropped from hud_data.c.
3. `__attribute__` uses left: `classNameVector` etc. carry `aligned(4)` (variables, not
   typedefs); `HudTextRows` is `__attribute__((packed))` (four shorts copied from
   `classRowY[0]` in UpdateRaceHud). Neither changes behaviour natively; removing them would
   change the oracle's code (lwl/lwr versus lh), so I left them.
4. A tool note: `tools/oracle32.py` should compare `jal` targets by relocation instead of
   `@offset`, otherwise any size change flags every later function in the file.
