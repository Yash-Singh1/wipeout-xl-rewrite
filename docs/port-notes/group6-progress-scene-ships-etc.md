# Group 6: progress.c, raceinit.c, radscn.c, scene.c, search.c, sfx.c, ships.c, skeleton.c, sound.c, tim.c, track.c, trackasm.c, ttf.c, utils.c, wadfile.c

Result
- `tools/diag.py` prints nothing for all fifteen files, except `SfxChainId` (sfx.c lines 495-496,
  `return;` in a non-void function; the function is equivalent-only and its text is
  untouched, `git diff` has no hunk inside it). The files are also clean with
  `-Wall -Wuninitialized -Wsometimes-uninitialized -Wconditional-uninitialized -O2` except the
  five uninitialised reads listed under rule 10.
- `tools/xdecl.py` lists no VAR/FUNC/DEF entry that involves these files.
- `tools/oracle32.py diff B0` over the fifteen files lists 24 functions; every one is
  explained below (no unexplained change; the other 130 or so functions are byte-identical
  on the MIPS o32 oracle).
- I read the oracle diffs with `jal` targets normalised too (a function after a changed one
  shows `jal @-xxxx` offset differences): the official tool and the normalised one list the
  same 24 functions here.

## Shared headers touched (small exact-string edits)

- `game/include/globals.h`, group 6 section: `loadFrame` (short), `currentWad`,
  `lastWadCurrent`, `loadTicks`, `linePrims` (`LINE_F2 *`), `waveIndex`,
  `screenFills[2]` (`BLK_FILL *`), `particleX` (`s32 *`), `gClutX/gClutY/gTextureX/gTextureY`
  (all `short`), `cdMusicTable[]` (`u_char`). I did not add `undercover` (racectrl.c has its
  own `extern u_char_eq1 undercover`, which I may not edit; see below).
- `game/include/functions.h`: `WDclose(int handle)`, `WDopen(char *name, ...)`,
  `openwadfile(char *name)`, `openwadfilenvc(char *name)`, `InitWeaponGrids(WeaponGrid *,
  TrackData *, int *, short *, SpecialGrid *)`, and the new prototype `AsmDrawObjectLit(Object *,
  Skeleton *, int roll, CVECTOR *colour)` in the object.c section (ships.c had a private
  `void *` copy, animover.c and droid.c have identical private copies that can now go).
- `game/include/trackasm.h`: `extern POLY_FT4 *TrackLoPolys[2], *TrackMedPolys[2],
  *TrackHiPolys[2]` (see trackasm.c).
- No struct layout changed. `include/skeleton.h` is dead (nothing includes it; it has a
  second, different `Skeleton`/`MATRIX`/`rsin`); I left it, the coordinator can delete it.

## Cross-group notes for the coordinator

- `waveIndex`: defined in draw.c (`int waveIndex;`, `waveIndex++` in TrackDirection) and was
  also defined in progress.c (load-screen loop counter); the console linked them as one
  variable. I removed progress.c's definition and left draw.c's. If group 4 also removes its
  own definition, nobody defines it.
- `screenFills`: menus.c (group 1) defines `s32 screenFills;` and has `extern s32 screenFill1;`
  and assigns `(s32)BlockFill(...)` to both. They are adjacent words, the array
  `screenFills[2]` of `BLK_FILL *` that progress.c now uses (globals.h). menus.c has to use
  `screenFills[0]`/`screenFills[1]` and drop `screenFill1`. The symbol `screenFill1` no longer
  exists on my side.
- `loadFrame`, `currentWad`, `lastWadCurrent`, `loadTicks`, `linePrims`: progress.c defines
  them (it owns the load screen); wadfile.c only uses them (its duplicate definitions removed,
  rule "one definition"). Types unified: `loadFrame` is `short` (progress.c had `u_short`,
  wadfile.c `s16`; progress.c always casts `(short)loadFrame` and wadfile.c tests `< 0`, so
  the signed type is the one every use assumes; no oracle change), `linePrims` is
  `LINE_F2 *` (wadfile.c had `char *` and passes it to DFree with a cast).
- racectrl.c (coordinator's file) calls my functions through unprototyped `__eq1()` aliases
  and needs attention:
  - `openwadfile__eq1()` is `int`-returning: it truncates the `WADFILE *` natively. It also
    passes `scenePath` (a `char[]`) where the old prototype had `int`; it is a `char *` now.
  - `InitWeaponGrids__eq1(weaponGrids, track, &locals.checkpoint, &locals.checkpointLap,
    CHECKPOINTS)` matches the new prototype (WeaponGrid *, TrackData *, int *, short *,
    SpecialGrid *): the old `int state/int config/int mode` parameters were these pointers.
  - `InitRaceResetData__eq1(locals.inputPacket, ...)` and `Restart__eq1(locals.inputPacket)`:
    `inputPacket` is `u_char[32]` in `RaceLocals`, the storage of a `ResetData` (7 pointers:
    28 bytes on the PS1, 56 natively). It overflows into `inGameMenu`, the next member.
  - `DrawShips__eq1()` passes a `TrackCamera` by value (pointer-bearing struct, 40 bytes
    natively); fine as long as the call site sees the right type.
- `undercover`: sound.c now declares it `unsigned char` (it was `signed char`; sound.c only
  stores 0 and 1, racectrl.c only tests `!undercover`), so xdecl is clean.
- `DAlloc` alignment: blocks that hold native Object/Skeleton/TrackSection/WADFILE/pointer
  tables (NewInstance, NewSkeleton, trackObjects, track->sections, the widened PRM images)
  need 8-byte alignment; unaligned pointer members work on x86-64 and arm64 but are
  formally undefined. Group 5 (malloc.c) owns the allocator.
- Data generator (ps1-data): names/types it should expect from these files: `screenFill1`,
  `panGainFR`, `TrackLoPolysAlt`, `TrackMedPolysAlt`, `TrackHiPolysAlt` no longer exist as
  separate symbols (`screenFills[2]`, `pbStereoLookup[30]`, `TrackLoPolys[2]` ...);
  `frameValues` is `void *` (it was `int *`); `loadFrame` is `short`.
- `abs` (utils.c) defines the game's own `int abs(int)`, which is also the C library
  name (and a compiler builtin). It links and behaves the same (INT_MIN wraps, `-fwrapv`);
  noting it in case the platform link has two definitions.
- Platform implementer: `SfxInit`/`SetGlobalVolume` call `SpuSetCommonAttr` with `mask = 0`
  and valid `mvol`/`cd.volume` fields (the PsyQ rule: mask 0 sets everything).
  `SfxPlay`/`SfxPlayId` call `SpuSetVoiceAttr` with `mask = 0x90` (pitch, start address) and
  leave the other fields of the stack `SpuVoiceAttr` uninitialised.

## Rule 9: file data (do not widen, listed for the coordinator)

PRM images (Object and Skeleton records inside the file; scene.c, used by ships.c,
radscn.c via objects and by weapon.c/object.c callers):
- `scene.c` `LoadPrmCommon` (lines 221-245): `LoadFile(name, start)` at 237 (file branch)
  and `readnextfile(wad, start)` at 245 (WAD branch) read the whole PRM into `start`.
  `DAlloc(heap, length + 32)` (227, 243) and `finish = start + length` (228, 244) are in file
  bytes. Walk: `prm.ptr = start; while (prm.ptr < finish)` (251), `object = prm.object++` (252)
  and `object->skeleton = prm.skeleton++` (258), then `prm.svector += vertexCount/normalCount`,
  `prm.primitive`, `prm.f3 += 1` ... (primitives are pointer-free and keep their size).
  Pointer slots: `Object` (file 64 bytes, native 104): `vertices` 20, `normals` 28,
  `primitives` 36, `bspTree` 44, `skeleton` 48, `next` 60. `Skeleton` (file 80, native 96):
  `super` 68, `sub` 72, `next` 76. The loader overwrites `vertices`, `normals`, `primitives`,
  `skeleton`, `bspTree` and `next`; it does not touch `Skeleton.super/sub/next` (ships.c and
  others set `super`; `sub`/`next` keep the file value, read by no code of mine).
  `length`/`finish` must be the widened length natively, and each record is
  `Object` then `Skeleton` then data, so the walk needs the widened image (see
  port_widen.h `PortWidenPrm`).
- `scene.c` `NewInstance` allocates `sizeof(Object)` / `sizeof(Skeleton)` (was 0x40 / 0x50) and
  copies fields individually; fine under any layout.
- Callers of LoadPrmCommon in my files: `LoadPrm` (scene.c), `WADLoadPrm` (scene.c),
  ships.c `loadShipData` lines 353, 380, 390 (`LoadPrm(shipFile.name, ...)`,
  `LoadPrm("c:\\wipeout2\\common\\rescu.prm", ...)`), which walk the result via `obj->next`
  (list built by the loader, no stride).

Track sections (track.c, `TrackSection`: file 156 bytes, native 240):
- `WADInitSections` (track.c 205-214): `track->sections = (TrackSection *)DAlloc(heap, length +
  32)` (205), `readnextfile(wad, (char *)track->sections)` (210), `track->sectionCount = length /
  sizeof(TrackSection)` (211: the divisor must be the 156-byte file record natively; with
  the native 240 the count is wrong), then `IntelSection()` (byte-swaps each record in place:
  `junction.index`, `prevSection.index`, `nextSection.index`, `centre`, `version`, `pad`, the
  `*ViewCount` arrays, `high`, `med`, `faceStart`, `faceCount`, `globalRadius`,
  `localRadius`, `flags`, `secNo`), `IndexToPointers()`, `CheckVersion()`.
  Pointer slots in the file record (file offsets): `junction` 0, `prevSection` 4,
  `nextSection` 8 (the three `SectionPtr` unions: the file holds the 32-bit `index`),
  `objects` 28, `northViewSection` 36, `southViewSection` 48, `eastViewSection` 60,
  `westViewSection` 72, `allViewSection` 84 (three slots each). Only the `SectionPtr`s carry
  file data; `objects` and the view pointers hold garbage in the file and are always
  overwritten (`WADInitViewList` sets the 15 view pointers per section, `SceneRadiusCheck`
  sets `objects`).
- `WADInitVertices` (155-162) and `WADInitFaces` (175-181): `VECTOR` and `Face` (20 bytes)
  are pointer-free, `length / sizeof(...)` is right.
- `WADInitViewList` (229-236): `short` list, pointer-free; then indexes `track->viewList +
  total` into the native pointer slots (correct natively).

`SectionPtr` usage, precisely:
- File phase: `.index` is the 32-bit file value (the loader writes `-1` for "no junction").
  `IntelSection` swaps its four bytes in place (track.c 54-56).
- `IndexToPointers` (track.c 107-128): `prevSection.ptr = sections + prevSection.index` and
  `nextSection.ptr = ...` (always valid); `junction.ptr = sections + junction.index` when
  `junction.index != -1`, else (my edit) `junction.ptr = (TrackSection *)-1`. The console
  slot already held 0xFFFFFFFF there, so this stores the same value, but natively a
  zero-extended file value (0x00000000FFFFFFFF) would not equal `(TrackSection *)-1`,
  which collide.c (395-546), drone.c (360, 379), weapon.c (634, 690) still compare against.
  The oracle shows IndexToPointers (and WADInitSections, which inlines it) with one more
  store.
- Pointer phase (after IndexToPointers): `.ptr` of prev/next is dereferenced everywhere
  (search.c 20-66, 96-144; radscn.c 73, 91, 198, 216, 260-266; raceinit.c 214-258, 437-496;
  ships.c 441-446, 604). The "is there a junction" test in my files is
  `junction.index != -1` (search.c 27, 34, 104, 110; raceinit.c 214, 235, 437, 448, 457;
  radscn.c 256) and `junction.ptr` is dereferenced only after it. This reads the low 32
  bits of the pointer slot through the union's `int` member: it equals -1 only for the
  sentinel, because a valid pointer is at least 4-byte aligned. It depends on a
  little-endian host (the union overlays `index` on the low half) and on the sentinel
  keeping its low half = -1 (guaranteed by the zero-extension or my store).
- `PointersToIndex` (track.c 130-147) is never called; its `(int)` casts keep it
  warning-free; it leaves the upper half of each slot as it was.
- Group 3 already changed hud.c to test `junction.index == -1`.

Other file reads, all pointer-free (nothing to widen):
- tim.c `loadtexcom` (TimClut4 / TimClut8 views of the TIM, `LoadImage` of `tc + 1`),
  `LoadTexture` (LoadFile 168), `WADLoadTexture` (readnextfile 188); ttf.c `LoadTtfFile`
  (LoadFile 29, `TexTemplate` = 21 shorts) and `WADLoadTtfFile` (readnextfile 60);
  raceinit.c `InitTrackTextures` (LoadFile 55, bytes) and `InitCheckPoints` (LoadFile 100;
  `__builtin_memcpy(checkpoints, fileBuffer, 36)` = 6 `CheckPoints` of 6 bytes, pointer-free);
  sound.c `LoadSample` (readnextfile 46, a VAG into `sampleBuffer`, `VagHeader`
  pointer-free); wadfile.c (`WADFILE` header: `count`, `sizes[]`, and `WadEntryHeader`
  through `WDread`, pointer-free; `DAlloc(heap, 0x108)` = 8 + 64 sizes).
- No file writes or `write()` of structs in these files.

## Per file

### progress.c
- Rule 1/2: `WDopen(int fileNo, ...)` is `WDopen(char *name, ...)` (`iso_open((char *)fileNo)`);
  `DrawLine`: `line = &linePrims[lineat]` (was `(lineat * sizeof(LINE_F2)) + (s32)linePrims`).
- Rule 6: `WDcloseFile() __asm__("iso_close")` and `BLK_FILL *screenFillTop __asm__("screenFills")`
  removed. `WDclose` takes the handle and calls `iso_close(handle)` (it used to ignore
  its argument and leave `$a0` to the alias; wadfile.c passes `wad->handle`). `InitPop`
  assigns `screenFills[0]` / `screenFills[1]` (was `screenFillTop` / `screenFill1`).
- Shared declarations: duplicate definitions `lastWadCurrent`, `loadTicks`, `waveIndex`
  removed or kept as above; local externs that globals.h now declares removed (`OT`, `OT2`,
  `tempGameData`, `reserveHeap`, `upperSkeleton`, `residentSkeletons`, `shipShapeTable`).
- Sizes: `DAlloc` sizes are for pointer-free `s32` / `POLY_F3` / `LINE_F2` arrays (0x200 =
  128 words, 0xA00 = 128 POLY_F3, 0x2000 for 256 LINE_F2); unchanged.
- Oracle: `DrawLine` 94 -> 102 lines, `DrawBox` 268 -> 300, `VsyncFunc` 1922 -> 2071 (both
  inline DrawLine). Reason: `line` now derives from the loaded `linePrims`, so LLVM reloads
  `lineat` and `linePrims` after the `sb` stores through `line` (it cannot prove they do not
  alias the globals); the old `inttoptr` form let it keep them in registers. Semantically
  identical. (`(LINE_F2 *)((lineat * sizeof(LINE_F2)) + (uintptr_t)linePrims)` gives zero diff
  but is the int-cast form the rules want rewritten.) `InitPop`: `sw $2, screenFill1` became
  `sw $2, 4(screenFills)`.

### wadfile.c
- Rule 6: `WDclose__wadfile ... __asm__("WDclose")` removed (`WDclose(wad->handle)`).
- Rule 2: `openwadfile` / `openwadfilenvc` take `char *name` (it was `int fileNo`, reused as the
  loop counter, now a separate `entry`; also fixes `printf("%s", fileNo)`).
- Definitions of the load-screen globals removed (see above). `DFree(reserveHeap,
  (char *)linePrims)`.
- Oracle: no change.

### raceinit.c
- Rule 1: `InitAttractMode`: `ship = &ships[ownShip]` (was `ownShip * 0xF0 + (int)ships`).
  `InitInGameMenu`: the row pointers computed from byte offsets 328 / 1768 / 232 + n * 72 / 36
  are `&menu->red[group][slot]`, `&menu->green[group][slot]`, `&menu->footer[row]` (hud.h's
  `InGameMenu`; the layout is pointer-free and the offsets match the old literals).
  Oracle: identical code.
- Rule 5: `strcat(&path, ...)`, `FileLength(&path)`, `LoadFile(&path, ...)` -> `path`.
  `InitWeaponGrids(int state, TrackData *context, int *scene, int config, int mode)`
  -> `(WeaponGrid *weaponGrids, TrackData *context, int *noWeaponGrids, short *specialCount,
  SpecialGrid *specialGrid)`: the ints were the pointers SetTrackAttributes takes; the
  `SetTrackAttributesFunc` cast-pointer type is gone and the calls are direct. Text of
  `InitAttractMode` / `InitInGameMenu` / `InitRaceData` parameters made identical to
  functions.h (xdecl). Prototype changed in functions.h (report).
- Oracle: `InitAttractMode`: commutative operand order of one `addu`. `InitWeaponGrids`: the
  three calls now convert `number` (int) to the callee's `short sectionNo` (`sll/sra`; the
  value is a section number, < 1000), plus register renaming.

### radscn.c
- Rule 1: `RadiusControl`: `camera->section = (TrackSection *)junctionIndex` is
  `camera->section->junction.ptr`; the `.index` test is kept for "has a junction".
- Oracle: `RadiusControl`: register renaming only.

### scene.c
- Rule 1/4: `NewInstance`: `while ((int)name < (int)(copy->name + 16))` -> `name < copy->name + 16`;
  `DAlloc(heap, 0x40)` -> `sizeof(Object)`, `DAlloc(heap, 0x50)` -> `sizeof(Skeleton)`.
- `LoadPrmCommon`: the "Bad primitive type %x in $%x" message prints `name` with `%p`.
- Oracle: `NewInstance`: with the pointer compare LLVM unrolls the 16-byte name copy (same
  bytes copied); stack-frame/register differences.

### search.c, utils.c
- No edits needed (diag clean, no oracle change). Audit: both walk `SectionPtr.ptr` only after
  IndexToPointers; `abs` see above.

### sfx.c
- Rule 1: `SfxContinue`: the two `*(s32 *)((s32)&voiceList[0].left + voiceOffset)` stores
  (`voiceOffset` += 0x1C = sizeof(SfxVoice)) are `voiceList[voice].left/right`. `SfxPause`:
  `((int *)pwVolumeStore)[voice * 2 (+ 1)]` -> `pwVolumeStore[voice][0/1]`.
  `SetQSoundVolume`: `pbQPanLookup[0][pan * 4 + k]` -> `pbQPanLookup[pan][k]` and
  `panGainFR[-pan]` -> `pbStereoLookup[30 - pan]` (`panGainFR` was the name of
  `pbStereoLookup[30]`; the extern is removed).
- Rule 5/7: the three `printf(msg...)` in `SfxTransEffect` are `printf("%s", msg)`;
  `SfxPlayId`'s two `return;` return 0: in the retail code `$v0` is the 0 that was just tested
  (`sfxInitialized` / `sfxEnabled`), so 0 is exactly the value the console returned. The callers
  (sound.c `StartControlledSound`) store it as a sound id.
- `SfxChainId` untouched. Its two `return;` hold: `!sfxInitialized` -> 0, `sfxEnabled`
  (tested non-zero) -> 1 in `$v0` (also its inverted `if (sfxEnabled) return;` test is the
  known equivalent-only behaviour). No caller in the game.
- Oracle: `SetQSoundVolume` (index arithmetic and register allocation), `SfxContinue` (the
  value is `(s16)leftValue` instead of a reload of `attr.volume.left`), `SfxPause` (pointer
  induction variables swapped), `SfxPlayId` (shrink-wrapped early return of a constant),
  `SfxTransEffect` (the three `printf` calls take the extra "%s" argument), `SfxStop`
  (only the numbering/order of its two string literals moved; no code change).

### ships.c
- Rule 1: `initShipData`: `ship = &shipIndex[i]` (was `i * sizeof(ShipData) + (int)shipIndex`).
- Rule 5: private `AsmDrawObjectLit(..., void *win)` and `RotTrans` declarations removed; the
  shared prototype (functions.h) takes a `CVECTOR *`, `win` is `CVECTOR *` (`(CVECTOR
  *)&face->red`, as droid.c does).
- Shared declarations: local externs now in globals.h removed (`shipTargetIndex`, `gameType`,
  `sillyShips`, `TextureTableCount`, `finishPosition`, `shipShapeTable`), and
  `numTrackSections` / `highResTable`, which group 2 / group 1 declared `short` in globals.h
  (ships.c had `u_short`): ships.c only compares `(short)numTrackSections / 2`,
  adds it and stores small table values, so the signed type changes nothing (no oracle
  change).
- Audit: `ShipStartLocals` (a private local struct with 15 `TrackSection *` and padding) is
  only accessed by field; no strides left. `DrawShips` passes `TrackCamera` by value.
- Oracle: no change.

### skeleton.c
- Rule 5: `printf(msgSkeletonAllocFailed)` -> `printf("%s", ...)`. Oracle: `NewSkeleton`: the
  extra argument. `skeleton.h` is dead (see above).

### sound.c
- Rule 1/2: `openwadfilenvc((int)"...samples.wad")` passes the literal; the two
  `(ShipData *)(shipNo * sizeof(ShipData) + (int)shipIndex)` in `AdjustDopplerEngine` are
  `&shipIndex[shipNo]`. `undercover` is `unsigned char` (above). Local externs now in
  globals.h removed (`cameraPtr`, `CameraUpdate`, `reserveHeap`).
- Oracle: `AdjustDopplerEngine`: operand order of two `addu`.

### tim.c
- Rule 7: `LoadTexture` returns 0 when the file is missing (see rule 7 below). The private
  `readnextfile` declaration removed (functions.h has it), the `gClutX...gTextureY` externs
  are in globals.h (`short`; init.c / main.c declare `unsigned short` and `short`).
- Oracle: `LoadTexture`: `return 0`.

### track.c
- Rule 1/5: `PointersToIndex`: `(int)(ptr - sections)`; `printf("verts at : %p\n", ...)`;
  `printf("%s", msgNewline)`. `IndexToPointers`: the `else` store (see SectionPtr above).
- Oracle: `IndexToPointers`, `WADInitSections` (inlined it): one more store;
  `WADInitViewList`: the `printf("%s", ...)`; `CheckCracks`: only the numbering of its
  string literals.

### trackasm.c
- Rule 5: private `OpenEvent`/`EnableEvent`/`CloseEvent`/`_card_info` declarations removed
  (psyq.h has them; `OpenEvent` had a conflicting signature).
- Rule 2 and aliases: `TrackLoPolys`/`...Alt`, `TrackMedPolys`/`...Alt`, `TrackHiPolys`/`...Alt`
  are the pairs `POLY_FT4 *TrackLoPolys[2]` etc. (adjacent words indexed by `CurrentScreen`
  in the assembly tile drawers; declared in trackasm.h for them). `AsmNextFrame`:
  `frameValues = lastWadCurrent` / `particleX` (the `int[]` aliases of progress.c's variables)
  is `frameValues = &lastWadCurrent` / `&particleX` with `void *frameValues`: the console stored
  the addresses of those two words and nothing reads `frameValues` back (grep over the tree).
- Oracle: `AsmGenerateTiles` (symbols are `TrackLoPolys+4` ... instead of `...Alt`),
  `AsmNextFrame` (the two variables are small data now, so `$gp`-relative addresses instead of
  `lui/addiu`).

### ttf.c
- Rule 10 (game bug, reproduced as far as possible): the out-of-memory path of `LoadTtfFile`
  ran `sprintf("Ttf.c:...for file %s\n", file)`: the *file name* was the format and the message
  literal itself the destination. A literal is read-only natively; the formatted name now goes
  to a local `message[256]` with `snprintf` (nothing reads either). `errorString` stays unchanged
  for the `Error(errorString, Fatal)` that follows, as on the console. Oracle: frame size and
  that call.

## Rule 7 (`return;` in a non-void function)

- sfx.c `SfxPlayId` (two): returns 0; matches the retail `$v0` (0, see above).
- sfx.c `SfxChainId` (two): untouched (equivalent-only).
- tim.c `LoadTexture` (file not found, after `Error(errorString, 0)`): the console returned
  whatever `$v0` held, which is `sprintf`'s length (`Error` with level 0 does nothing).
  The callers use the result as a `Texture *` (card.c `cardIconInfo`, effects.c
  `shadowTexture`), so that was a small integer used as a pointer: a crash on the console too.
  Returns 0 (null) now.

## Rule 8
- None: no absolute console addresses, no `float`/`double` in these files.

## Rule 10: MIPS behaviour the host does not share (reported, not fixed)

- ships.c `initShipData` line 457: `slot` (short) is read uninitialised when `gameType != 2`
  and `raceType` is neither 2 nor 3 (`locals.starts[slot + 1] = locals.starts[slot]`). In the
  retail code it is `$t0` (never written; checked in the retail object code). I believe the
  path is unreachable (`gameType != 2` is a linked game, which always has `raceType` 2 or
  3) but could not prove it; natively it would write at a garbage index of a stack array.
- progress.c `VsyncFunc`: `tintR/tintG/tintB` are set only by the `switch` on `actTrackNum` for
  tracks 1, 2, 6, 7, 8, 13, 17, 20 (no default); any other track number reads uninitialised
  stack in the scan bars' colour. I believe these eight are the only values in the game;
  unverified. Cosmetic if wrong.
- progress.c `WDread`: `(remaining * 100) / total` with `total = wadSize`; a zero-length wad
  would divide by zero (the MIPS does not trap). `remaining * 100` wraps for wads over 21 MB
  (`-fwrapv`).
- wadfile.c `closewadfile`: `while (loadFrame < 0);` spins until the vblank callback
  increments `loadFrame` (u16 field counter) past 0x7FFF back to a non-negative value. With the
  platform calling VsyncFunc only from inside VSync/DrawSync the loop cannot terminate if
  entered with `loadFrame < 0` (that needs 32768 fields = over 9 minutes of loading screen), and
  the optimiser may drop the empty loop. Not changed.
- sfx.c `SfxStop`: `mask` is uninitialised when `voiceList[voice].play != sound`
  (`SpuSetKey(0, mask)`), and `1 << voice` and `voiceList[voice]` use `voice = -1` when the
  play slot was never assigned (`playList[i].voice` is initialised to -1 in `SfxInit`). Natively
  that is a read of `voiceList[-1]` and a shift by -1. `TerminateSound` (sound.c) calls
  `SfxStop` with the ids `StartControlledSound` stored, which are 0 when `SfxPlayId` returned
  early (sfx disabled).
- sfx.c `SfxEffectPitch`: `right = playList[sound].voiceRight; if (right != 0)` - the unused
  value is -1, so it also reads `voiceList[-1].play` and shifts by -1 when equal. The game's own
  bug (should test `!= -1`); kept.
- sfx.c `SfxPlay`/`SfxPlayId`/`SfxChainId`: `effectList[sample]` with `sample` = `sampleHandle[]`,
  which holds -1..-4 for a sample that failed to load (`SfxTransEffect` error codes): reads
  before `effectList`.
- sound.c `LoadSample`: builds `path` (never used, as on the console) and reads a wad entry into
  a fixed `0xC800` byte buffer: a VAG larger than that overruns it, as on the console.
- raceinit.c `InitCDAudio`: `next % count` with `count = cdMusicCount - 1` is a zero division
  when the disc has one track (cdMusicCount 1). `InitShufflePlay` fills `cdMusicTable`
  (16 bytes in the data) with `cdMusicCount - 1` entries, as on the console.
- raceinit.c `InitRaceLine`: `short sections[632]` is filled without a bound check.
- scene.c `NewInstance`: the new object's `index` (and `scratch` aside) are not copied
  (uninitialised heap bytes, as on the console).
- radscn.c `CalcNewRadius`: first thing it does is `DFree(heap, section->objects)`; only the
  debug pad path (`RadiusControl`) calls it, after `SceneRadiusCheck` has set `objects`.
- Shifts: `1 << voice` / `1 << i` are the only variable shifts (voice 0..23 except the -1 cases
  above). The `% 21` and `/ 15` divisors are constants. No `INT_MIN / -1`.

## Things I could not fully verify

- That `slot` (ships.c) and `tintR/G/B` (progress.c) are never read uninitialised in the real
  game (reasoning above, no run).
- The `IndexToPointers` sentinel store: correct for every comparison I found in the tree,
  but collide.c / drone.c / weapon.c are other groups' files; they rely on it.
- `loadFrame` as `short`: the oracle shows no change in progress.c or wadfile.c, and the
  value is a frame counter; not checked against other-group users (none exist).
- The oracle is clang MIPS, not the retail compiler: "no change" means the edit does not alter
  what clang generates for the 32-bit target, not byte equality with the retail code.
