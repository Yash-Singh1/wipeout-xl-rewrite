# Group 2: game/src/weapon.c and game/src/effects.c

Result: `tools/diag.py game/src/weapon.c` and `.../effects.c` print nothing. `GetNewWeapon` is
unchanged (its text; the only diff nearby is the removal of the two `extern u16 ...` lines above it, and
its generated code is identical in the oracle). Nothing was committed.

`tools/oracle32.py diff B0 weapon.c effects.c` reports 27 changed functions (15 in weapon.c, 12 in
effects.c), each explained below; I read every `-v` diff. Note on the tool: it normalises `jal` targets
relative to the function start, and an unresolved `jal` (relocation, target 0) therefore shows a different
"address" whenever any earlier function in the file changed size. Midway through my edits it listed 35
functions for weapon.c alone where only 11 really differed. I checked with a private filter that blanks
`jal @...` (scratch script, not in the repo); at the end both agree on 27. `tools/oracle32.py` should
normalise `jal` lines that carry an `R_MIPS_26` relocation to the symbol name.

HEAD (not B0) already had the `PsxScratchpad` edit in `TransformTransparentObject`, so that function
shows `lui $2, 0x1f80` -> `PsxScratchpad` against B0; that is the coordinator's edit, not mine.

## Edits, weapon.c

Pointer/integer casts (rule 1) and pointer arithmetic by 32-bit strides:
* `InsertWeapon`: `(int)weapon < (int)(weapons + 30)` -> `weapon < weapons + 30`.
* `InitMinigun`, `DamageCollide`: `(ShipData *)(n * sizeof(ShipData) + (int)ships)` -> `&ships[n]` / `&shipIndex[n]`.
* `ClearWeaponTable`: same, `&weaponIndex[i]`.
* `CycleWeaponGrids`: loop end `(s32)(grids+k) < noGrids*16 + (s32)grids` -> `grids + k < grids + noGrids`
  (the literal 16 was the 32-bit size of `WeaponGridZone`; natively 24).
* `UpdateQuake` (two places): `junction != (void *)-1` -> `section->junction.index != -1`, with a `Port:` comment. The track
  loader widens a 4-byte slot into the low 4 bytes of the 8-byte pointer slot (port_widen.h), so a stored -1 reads
  as 0x00000000FFFFFFFF as a pointer; the `index` member of the union reads -1. (hud.c uses the same test.)

Integers/mis-typed pointers (rule 2), globals (shared declarations):
* `cameraPtr` was `int *` here (and `Skeleton **` in effects.c/combat.c): it is `TrackCamera *cameraPtr` (globals.h group 3,
  also what ayrace/droid/sound/sparks use). `*cameraPtr` (the camera skeleton) is `cameraPtr->camPos` (offset 0 in both
  layouts). Changed in `UpdateMissile`, `UpdateMine`, `UpdateElecBolt`, `UpdateRocket`, `UpdateSmartBomb`, `UpdateShield`.
* `UpdateSmartBomb`: `int *camera; cameraValue = *(volatile s32 *)camera;` -> `TrackCamera *camera; Skeleton *cameraValue = camera->camPos;`
  (the volatile load only pinned the order of two independent loads; the oracle shows them swapped).
* `nextPrim`: weapon.c declared it as the union `ShadedPtr`, display.c as `PrimPtr {char *ptr}`, object.c as `char *`. It is one
  pointer; globals.h now declares `PrimPtr nextPrim` and `TransformTransparentObject` views it through locals
  `ShadedTri *tri` / `ShadedQuad *quad`. `ShadedPtr` was removed from weapon.h (no other user). The local caches the pointer
  across the packet writes (the oracle shows one load of `nextPrim` instead of several; the packet buffer cannot alias it).
* Removed the file-local externs that globals.h now owns or that clashed: `OT`, `cheatDamage`, `CameraUpdate`, `linkSendFlag`,
  `shipTargetIndex`, `ambientVoice2` (was `short`, sound.c defines `int`), `particleSlots`, `numTrackSections`,
  `TextureTableCount`, `textureWad`, `heap`, `cameraPtr`, `cheatMinigun`, `weaponIndex`, `weaponBars`, `tempGameData`, `gameType`,
  `nextPrim`, `UpdateCameraAttractDestroyed` (in functions.h), the u16 `remoteRacePos`/`playerPosition` declared before
  `GetNewWeapon` (both are `short`; positions are 1..15 so `GetNewWeapon` behaves the same, and its object code is identical),
  and the unprototyped `RotTransPersN/NormalClip/AverageZ3/AverageZ4` (psyq.h has them).
* `perfectLap` is defined here as `u_char` now (was `signed char`; ayrace/hud use `unsigned char`; it only holds 0/1).
* `StaticDepth`, `StaticFlags`: spelled `unsigned short[]` as object.c does (xdecl treated `u_short[]` as different).
* `fadeTileRed/Green/Blue` (names for bytes inside `fadeTile[CurrentScreen]`, stride 16 = `sizeof(TILE)`):
  `UpdateFadeScreen` now writes `fadeTile[CurrentScreen].r0/.g0/.b0`; the three alias externs are gone (the data generator
  defines them as separate objects, which would not alias `fadeTile`).

Prototypes (rule 5):
* `NormalClip(DVECTOR, DVECTOR, DVECTOR)` (struct by value) vs `NormalClip(int,int,int)` in psyq.h: the three screen points are
  packed with a file-local `PackScreenPoint()` (x low half, y high half; what the console passed in one register). Clang's MIPS
  output still uses the same `lwl/lwr` pair, so the packing costs nothing there.
* `SfxEffectLRPan__UpdateShield` / `SfxEffectFBPan__UpdateShield` (`__asm__` aliases): removed, the calls use `SfxEffectLRPan/FBPan`
  (functions.h, `u_char` pan). The oracle shows a `srl`/`andi 0xff` where the old code passed the full int; the callee takes the pan as a byte.
* `SetVolume(*(short *)&ambientVoice1, n)` -> `SetVolume(ambientVoice1, n)` (a `short` parameter).
* `AsmDrawObject`: functions.h now has the 3-argument prototype (someone else's edit, with the roll comment); the eight calls in
  weapon.c and effects.c pass no roll, so they pass `0` with a comment (menus.c does the same). Objects drawn by these calls are
  weapons, shields, start lights, droids and mono-rail carriages; none has sprite primitives, the only primitives that read roll.
* `printf(msgWeaponTableFull)` (format-security) -> `printf("%s", msgWeaponTableFull)`; the text has no `%`, output is identical.
* `memcpy_80089544` (the decompiler's name for the C library's memcpy, defined nowhere) -> `memcpy` in `initWeapons`.
* `initWeapons` and `UpdateSmartBomb` etc. keep their signatures; functions.h `initWeapons()` now has its real prototype
  `(WeaponShape *, Skeleton *, WeaponData *)` (racectrl.c passes a fourth argument, `weaponInitArg`, which the callee never reads).

Sizes/file data (rules 4 and 9):
* `initWeapons` / `CLONE_SHAPES`: `length = textureWad->sizes[textureWad->current]` (the PRM file size) became
  `length = portLastPrmImageLength` read after `WADLoadPrm`, as port_widen.h documents ("the weapon code copies whole images"):
  the clones are copies of the widened master image (`DAlloc(heap, length + 32)` and `memcpy(.., length)` keep the original slack).
  **This depends on `LoadPrmCommon` (scene.c, not mine) calling `PortWidenPrmImage` for WAD loads, which it did not yet when I wrote this.**
  Until it does, `length` is 0 (or a stale value) and the clones are empty. The pointer fix-ups in the macro (`prm.object++`,
  `prm.skeleton++`, `prm.svector += ...`) are by pointer types and need no change.
* No other `DAlloc`, `sizeof(struct with pointers)` or literal size remains in weapon.c.

## Edits, effects.c

* `AddEffects`: two pointer compares `(int)ship < (int)shipEnd`, `(int)weapon < (int)(weapons + 30)` -> pointer compares.
* `Video`: `*(short **)((signed char *)*objp + 0x24)` (Object.primitives at its 32-bit offset 0x24) -> `(*objp)->primitives`
  (native offset 56); `objp` is `Object **`; `tempGameData + 5` -> `tempGameData->actTrackNum`. The writes through `cursor`
  into quad primitives (offsets 2..9, `0x20`/`0x2C` strides) are pointer-free GT4/FT4 bytes; left as they are.
* `MoveMonoRail`: rewritten without raw offsets. Parameter `int ships` is `ShipData *ships` (prototype in functions.h changed;
  the caller in racectrl.c passes the ship array). `ship + 0xC8` (short) is `nearSecNo`, `+0xC6` is `lap`, `mono + 0/4/8/0xC` are
  `index/stepX/stepY/stepZ`, `obj + 0x30` is `skeleton`, `skel + 0x14/0x18/0x1C` are `relative.t[]`, `+0x40` is `update`.
  `(ownShip * 0xF0) + ships` (240 = 32-bit `sizeof(ShipData)`, natively 256) is `ships + ownShip`. Same statements and order.
* `ResetZeppelin`, `MoveZeppelin`: `zeppelinObj` is `Object *` (was `void *volatile`, cast at every use). The volatile made every
  use reload the pointer; the oracle shows fewer loads, same behaviour.
* `MoveMonoRail`, `UpdateLightStart`, `UpdateStartDroid`, `DrawShadow`: `cameraPtr[0]` / `*camera` -> `cameraPtr->camPos`; the
  `AsmDrawObject` calls pass roll 0 (see above); `DrawShadow` passes `(int *)&screen[n]` to `RotTransPers` (psyq.h wants the packed
  `int *`; the platform writes one packed word per point).
* `InitShadow`: `DAlloc(heap, 0x4B20)` -> `15 * sizeof(Shadow) + 32` (same number: `Shadow` is 1280 bytes of POLY_FT4, no pointers);
  `rowBase` is a `u_char *` instead of an `int` holding an address. `DrawShadow`: quad address built from `(u_char *)shadowQuads`.
  `DrawWeaponIcons`: `&icons[CurrentScreen]`.
* `DrawShadow`: `*(int *)&ship->combatAttr & 0x8080` -> `ship->combatAttr & 0x8080` (the mask only selects bits of the u_short;
  the old word load also read the next field, which the mask discards).
* `WaterFall`, `InitWaterFall`: `waterfallBuf`, `waterfallSaved` and `waterfallRow` are one 32-byte pixel row (16 u_short, the
  RECT is 16x1): the executable has `waterfallSaved = waterfallBuf + 8` and `waterfallRow = waterfallBuf + 0x1E`. Only `waterfallBuf`
  is declared now, as `u_short[16]`: `first = waterfallBuf[15]`, shift `[4..14]` up by one starting at `&waterfallBuf[15]`, `waterfallBuf[4] = first`,
  `LoadImage(&rect, (u_long *)waterfallBuf)`. The old call went through a cast `(int (*)(RECT*, u_short*, u_short))LoadImage` with an extra
  argument (`first`) that LoadImage never reads; dropped.
* `monoRailFlag` is `short monoRailFlag[]` in globals.h and `ResetMonoRail` writes `monoRailFlag[0]`. See "for the coordinator".
* Prototypes in functions.h: `MoveMonoRail` (ShipData *), `InitShadow(void)`, `ResetStartBoom(void)`, `LoadStartDroid(Object **, char *names, Skeleton *)`,
  `InitDroidThings(Object **, char *names, Skeleton *parent, Checkpoint *, ConfigData *)`, `UpdateLightStart(Object **, ShipData *, DroidData *, short, ConfigData *, int)`.
  The `unused` parameters were `int` but receive pointers from racectrl.c (names, skeleton, ships, checkpoints, game data); the
  types are what the callers pass. The functions never read them, so no code changes (not in the oracle list for that reason).
  `InitWeaponIcons`/`DrawWeaponIcons` now spell `Texture **` (xdecl compared the spelling `struct Texture **`).
* Removed externs now in globals.h: `particleSlots`, `textureWad` (was `void *`), `TextureTableCount`, `classLaps`, `numTrackSections`,
  `tempGameData`, `monoRailObjs`, `posMonoRail`, `monoRailFlag`, `zeppelinObj`, `cameraPtr`, `OT` (was `int *[]`), `sillyShips`, `heap`, `weaponIconPrims`.

## globals.h (group 2 block) and functions.h

Added to globals.h: `ambientVoice2 classLaps monoRailFlag[] monoRailObjs[] nextPrim numTrackSections particleSlots perfectLap posMonoRail[]
remoteRacePos shipTargetIndex[] weaponBars weaponIconPrims[] weaponIndex zeppelinObj`. (`heap`, `TextureTableCount`, `cheat*`, `sillyShips`
were already declared by group 1; `OT`, `cameraPtr`, `linkSendFlag`, `playerPosition`, `tempGameData`, `textureWad` by group 3.)
functions.h: the prototypes listed above, and `AsmDrawObject` (not mine, already 3-argument when I finished).
weapon.h: removed `ShadedPtr`.

Remaining `tools/xdecl.py` conflicts involving these names are all in files I don't own: racectrl.c (`heap`, `weaponIndex`, `weaponBars`,
`particleSlots`, `monoRailObjs`, `posMonoRail`, `weaponIconPrims`, `zeppelinObj`, `cameraPtr`, `OT`, `tempGameData`, `sillyShips`, `TextureTableCount`, `shipTargetIndex`),
combat.c (`cameraPtr` as `Skeleton **`, `OT`, `linkSendFlag` as u_char, `playerPosition` as u_char, `shipTargetIndex`), object.c (`nextPrim` as `char *`,
`RotTransPersN` spelled `unsigned short *`), drone.c (`numTrackSections` unsigned), main.c (`TextureTableCount` short, `sillyShips` signed char), hiscores.c (`u8 classLaps[]`).

## Oracle: changed functions and why

weapon.c, address-expression/loop-form only (same loads and stores, different register allocation or order):
`ClearWeaponTable` (commutative `addu`), `CycleWeaponGrids` (loop bound), `DamageCollide`, `FireWeapon` and `InsertWeapon` (pointer-compare loop form;
FireWeapon inlines InsertWeapon; the `printf` argument), `InitMinigun` (commutative `addu`).
weapon.c, intended:
* `UpdateFadeScreen`: the three `fadeTile*` byte stores now address `fadeTile[CurrentScreen]+4/5/6` instead of three separate symbols (+16 stride).
* `UpdateShield`: `ambientVoice1` loaded as the int it is (was `lh` of the low half) and the byte pan argument is masked (`andi 0xff`).
* `UpdateSmartBomb`: load of `cameraPtr` moved below the `shapes[...]` loads (the volatile pinned it).
* `TransformTransparentObject`: `nextPrim` cached in a local; `PsxScratchpad` (coordinator's edit, see above).
* `initWeapons`: `memcpy` symbol, `length` from `portLastPrmImageLength`.
* `UpdateMissile`, `UpdateMine`, `UpdateElecBolt`, `UpdateRocket`, `UpdateSmartBomb`: one extra instruction, the zero roll argument.
effects.c: `AddEffects` (loop form), `DrawShadow` (quad address; `combatAttr`), `DrawWeaponIcons` (`CurrentScreen` reloaded after the byte store),
`InitShadow` (scheduling of the u/v averages, base register of the row stores), `MoveMonoRail`/`UpdateLightStart`/`UpdateStartDroid` (roll argument, field
accesses; `MoveMonoRail` otherwise commutative adds), `MoveZeppelin`/`ResetZeppelin`/`ResetAnimatedObjects`/`ResetMonoRail` (`zeppelinObj` no longer volatile;
`monoRailFlag` is not a `$gp` object any more because it is an array: `lui/addiu` instead of `$gp`), `WaterFall` (one object, not three symbols).

## Rule 7-10 items

7. No `return;` in a non-void function in either file.
8. Absolute console address: `TransformTransparentObject` (weapon.c) uses `PsxScratchpad` (coordinator's edit; offsets 0x200 and 0x300 inside it for depth and flags).
   The pointer cast `(DVECTOR *)PsxScratchpad` needs the array to be at least 2-byte aligned (4 if the platform packs points with an `int` load).
9. File data:
   * weapon.c `initWeapons`/`CLONE_SHAPES`: walks and copies PRM images (Object and Skeleton records, vertices, primitives) loaded by `WADLoadPrm`;
     see the dependency on `portLastPrmImageLength` above.
   * effects.c `LoadTrain` (`LoadPrm` of the train PRM), `LoadStartDroid` (`WADLoadPrm`), `InitStartBoom`/`InitRedLight`/`InitObject`/... walk the
     `Object` chains the loader built: no raw record access, nothing to widen here.
   * `CycleWeaponGrids` indexes the `WeaponGridZone` array (24 bytes natively) that the track loader (`SetTrackAttributes`, dynam.c) fills; `WeaponGrid` and
     `WeaponGridZone` must have the same native size (both are 24, same field offsets, checked with `layouts.py show`).
   * `UpdateStartDroid`/`InitDroidThings` index `Checkpoint`/`DroidData` arrays (6 bytes, no pointers) from racectrl.c data.
   * `InitWaterFall`/`InitShadowCluts`/`WaterFall` move VRAM rows through `u_short` buffers cast to `u_long *` (platform reads them as 16-bit pixels).
10. Behaviour the host does not share, and reads of uninitialised variables the executable really performs (kept):
   * `GetNewWeapon` (equivalent-only, untouched): `position` is read uninitialised when `shipNo` is neither `ownShip` nor `remoteShip` (and < 12);
     `group` is uninitialised when `position` is none of the tested ranges; `weapon` is uninitialised in the `retry` loop when `chance` is above every
     `weaponChance[position][..]` threshold (clang: conditional-uninitialized at `weapon`, `group`, `position`).
   * `GetNewDroneWeapon`: `weapon` read uninitialised when `chance` is >= `weaponDrone[group+5]`.
   * `DamageCollide`: `scaledDamage` is uninitialised for a `damageType` outside the switch (0xC,0xD,0xE,1,2,0xF,3,5,0x80,0x81,0x82).
   * `ColourStartBoom` (effects.c): `r,g,b` uninitialised for `noToLight` outside 1..3.
   * `InitPitCycle`/`InitTrainLights`: `sec++`/`p++` loops can run past the arrays if the data is not as expected (no bounds checks in the original).
   * Integer divisions by `mag = SquareRoot0(path.vx^2 + path.vz^2)`: `UpdateMissile` (1 site, two divisions by `mag`), `UpdateRocket` (2 sites),
     `UpdatePlasma` (2 sites). If two consecutive track points coincide in x/z, `mag` is 0: the console gave an unpredictable value, the host raises SIGFPE.
     Not guarded (the game's own behaviour); report only.
   * `<< 16` of possibly negative values (`alpha = ((..)/mag) << 16`), `1 << shipNo` (shipNo < 15): fine with `-fwrapv` in practice.
   * Float/double: none in either file.
   * `*(volatile ...)` / unaligned casts: none left in weapon.c; effects.c has none.

## Raw offsets/size dependences I did not fix

* effects.c `Video`: byte writes through `cursor` at offsets 2..9 and `-2`/`0` and strides `0x20`/`0x2C` into GT4/FT4 primitives, and the `prim += 16/22`
  short-pointer strides. Primitives are pointer-free (GT4 = 44 bytes, FT4 = 32 bytes everywhere), so they are correct natively.
* effects.c `InitTrainLights`, `AnimateTexture`, `RedBullAnimate`: primitive walks by `0x18`, `0x1C`, `sizeof(GT4/FT4)` (pointer-free).
* effects.c `*(u_short *)&gameData->screenX` (ConfigData is pointer-free).
* weapon.c `InsertWeapon`: `(u_char *)damage + owner*4` on `int` arrays (stride is `sizeof(int)`).
* `UpdateWeapons`/`ClearWeaponTable`/`initWeapons` use the counts 30 (weapons), 31 (shapes), 192x2 (DR_MODE); fine natively.

## For the coordinator (racectrl.c and friends)

* racectrl.c allocations of group-2 types, with the arithmetic that reproduces the original literals:
  `weaponIndex = DAlloc(heap, 30 * sizeof(WeaponData) + 32)` (0x890 = 30 * 72 + 32; the real `sizeof(WeaponData)` is 72 = 0x48 on the console, 80 natively, not 76),
  `weaponBars = DAlloc(heap, 31 * sizeof(WeaponShape) + 32)` (0x118 = 31 * 8 + 32; natively 16 bytes each),
  `weaponGrids = DAlloc(heap, 64 * sizeof(WeaponGridZone) + 32)` (0x420 = 64 * 16 + 32; 64 is the only count that fits exactly, not confirmed from the code),
  `shipData = DAlloc(heap, 17 * sizeof(ShipData))` (0xFF0 = 17 * 240).
  Also its call `initWeapons(weaponBars, residentSkeletons[0], weaponPool, weaponInitArg)` has a fourth argument the callee ignores;
  `InitShadow(residentSkeletons[0])`, `LoadStartDroid(...)` (5 args, 3 declared) likewise; `MoveZeppelin(zeppelinObj, ...)` must pass `&zeppelinObj`
  now that `zeppelinObj` is one `Object *` (as `InitObject(.., &zeppelinObj__v1, ..)` already does).
* `monoRailFlag` is the first field (`index`, a short) of a 0x30-byte `MonoRailData` record: racectrl.c passes `&monoRailFlag[0]` as the `MonoRailData *` to
  `InitMonoData` and `MoveMonoRail`, which write `totalX..Z` and `stepX..Z` at offsets 4..0x2C. The data definition must be at least `sizeof(MonoRailData)` bytes
  (48; pointer-free) with 4-byte alignment; `short monoRailFlag[1]` (the data generator's current guess) is too small. Cleaner: define a `MonoRailData monoRail`
  container and make `monoRailFlag` its `index`; I did not rename anything.
* Duplicate definitions with the data generator: weapon.c defines `fadeAmount fadeActive perfectLap olddam fadeRed fadeGreen fadeBlue seedWep missileLockTimer
  rearLockCycle minigunAmmo minigunFrame` without initialisers, effects.c defines `pitCycleFlag startBoomObjs trainLightSeed neonSignTick redBullTick redBullFrame
  spriteTick spriteFrame lightPhase advertAngle radarAngle lightUpdateTick lightChaseStep monoRailSettle zeppelinDrift shakeAmount droidBlinkTick startDroidSection`.
  The data generator's weapon_data.c/effects_data.c define the same names (e.g. `seedWep = 0x12345678`, `minigunFrame`); the executable's initial values
  (notably `seedWep`) are only in the generated files, so the definitions in these two .c files must be dropped (`--externalize`) when the data files are linked.
  Its guesses `int weaponIndex`, `int weaponBars`, `u_char waterfallBuf[8]`, `u_short waterfallSaved[11]`, `u_short waterfallRow[1]`, `void *volatile zeppelinObj`,
  `s16 remoteRacePos` must follow globals.h / the waterfall note above (`waterfallBuf` is 32 bytes).
* `tools/oracle32.py` `jal` normalisation (see the top).

## Unsure

* `AsmDrawObject` roll `0`: I do not know what the console's stale `$a2` held at each call; I checked that these objects have no sprite primitives, but
  `MoveMonoRail` draws the six mono-rail carriages (`monoRailObjs`) and the start-light and droid objects, whose primitive types I did not enumerate.
* `portLastPrmImageLength` (see above) is the only dependency on code I did not see in its final form.
* `TrackCamera *cameraPtr` as the single declaration: racectrl.c assigns `cameraPtr = residentSkeletons` (an array of `Skeleton *`), so `cameraPtr->camPos` is
  `residentSkeletons[0]` (the camera) in both layouts, but `cameraPtr->section` etc. (ayrace, droid, sparks) alias `residentSkeletons[1..]`. That is the
  coordinator's `race` function; the 32-bit and native layouts agree for the first two members only if `residentSkeletons` has 8-byte slots (it does natively).
