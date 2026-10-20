# Group 4: collide.c, combat.c, control.c, crack.c, credits.c, display.c, draw.c, droid.c, drone.c, dynam.c, error.c

Status when I stopped:

* `tools/diag.py` prints nothing for control.c, combat.c, crack.c, credits.c, display.c, draw.c, droid.c,
  drone.c, dynam.c and error.c. For collide.c it prints 14 warnings, all in `collideLineToPlane`
  (lines 154-157 and 161-164, the `(ShipData *)(shipNo << 4)` ... `+ (int)shipIndex` chain; the function
  is on the untouchable list and its text is unchanged).
* `tools/oracle32.py diff B0 collide.c combat.c control.c crack.c credits.c display.c draw.c droid.c drone.c dynam.c error.c`
  listed 18 functions; every one is explained in "Oracle changes" below. (Someone has since added
  `#include "../../platform/trace.h"` and `PortTraceBoundary` calls to display.c; the oracle cannot compile that include
  under `-nostdinc`, so it now reports a compile failure for display.c. The three display.c entries below were
  checked before that edit, and `diag.py` is still clean for the file.) collide.c, crack.c, credits.c,
  draw.c, droid.c, dynam.c and error.c have no oracle change at all.
* `tools/xdecl.py`: the conflicts that still involve my files are listed under "Left as is".

## Read this first (decisions for the coordinator)

1. **Untouchable functions: edits I could not avoid.** Both come from the shared declarations other groups
   put in globals.h (group 3: `cameraPtr` as `TrackCamera *`; group 5: `playerControl` as `ControlInput[]`).
   The old text no longer compiles against them.
   * `DrawLinkSign` (combat.c lines 85-90, the five `(*cameraPtr)->...` expressions): now
     `cameraPtr->camPos->...`. Same value (the console read the first field of the `TrackCamera` through a
     `Skeleton **`). The oracle shows no change for the function.
   * `doSerialGame` (combat.c line 308): `send.control = playerControl;` is now
     `send.control = playerControl[0].head;` (the console read the first halfword of the controller record).
   * Nothing else in `collideLineToPlane`, `DrawLinkSign`, `doSerialGame` or `UpdateDroneShip` was touched.
     Declarations at the top of combat.c / drone.c that they use were changed only where the shared type is
     the same value at another width or sign (see "Declarations the untouchable functions use").
2. **Names the console used for the same storage (the decompiler split one object into several symbols).**
   Natively each name would be a separate variable, so these are real bugs, not style. I fixed the ones
   in my touchable code and list the rest:
   * `racingLine` is `raceLines + 4` (0x800EF860 vs 0x800EF85C): `racingLine[n].vx/vy/vz` is
     `raceLines[n].x/y/z`. Fixed in `followRaceLine` (drone.c). `UpdateDroneShip` does not use it.
   * `shipOrder` is `shipRank + 2`: `shipOrder[n][0]` is `shipRank[n].shipNo`. Fixed in `getShipInfrontLAP`
     and `getShipBehindLAP` (drone.c).
   * `dispOffX0`, `dispOffY0` are `DispEnv0.screen.x/y` (DispEnv0 = 0x800D0620, dispOffX0 = +8, dispOffY0 = +10)
     and `dispOffX1`, `dispOffY1` are `DispEnv1.screen.x/y` (DispEnv1 = 0x800D0634). Fixed in display.c
     (`InitMenuDisplay`, `InitGameDisplay`). **Still wrong in effects.c (lines 33-36 and 1881-1897) and menus.c (lines 134-138,
     1446-1483, 3471-3474, 3546-3549)**: they still declare and write the four names, which natively are four
     separate variables, so the menu's screen-position setting would never reach the display environments.
     I added `extern DISPENV DispEnv0, DispEnv1;` to globals.h (group 4 section); those files should write
     `DispEnv0.screen.x` etc.
   * `linkPadHead/Buttons/Pressed/Aux0/Aux1` are the fields of `playerControl[0]` and
     `remotePadHead/Buttons/Pressed/Aux0/Aux1` the fields of `remoteControl[0]` (`ControlInput` is
     {head +0, buttons +2, pressed +4, pad6 +6, thrust +8, steering +9}; the exe has linkPadHead = playerControl+2,
     linkPadButtons +4, linkPadPressed +6, linkPadAux0 +8, linkPadAux1 +9, and the same offsets from remoteControl).
     `doSerialGame` (untouchable) sends and receives the whole 10-byte record through those names, so natively it sends
     garbage/zeros and never updates the remote controller. For the rewrite: `padHead = ctl->buttons`,
     `padButtons = ctl->pressed`, `padPressed = *(u_short *)ctl->pad6`, `padAux0 = ctl->thrust`,
     `padAux1 = ctl->steering`, `control = ctl->head`, with `ctl = &playerControl[0]` (send) and
     `&remoteControl[0]` (receive). ghost.c has the same pattern: `ghostButtons` (+2), `ghostButtonsPressed` (+4),
     `ghostButtonsPrev` (+6), `ghostStickX` (+8), `ghostStickY` (+9) all lie inside `ghostControl[0]` (0x800F9B44).
   * `hiMedCrackPolysB` is `hiMedCrackPolysA + 64 bytes`: the console had one array of 128-byte rows
     (two screens of `A`, then two of `B`); the two names are kept in crack.c as two tables of 8 rows, which
     behaves identically because nothing else touches them (see sizes below).
3. **`AsmDrawObject` has three parameters** (the console routine is hand-written assembly; it moves `$a2` to
   `$s3` at entry and passes it to `rcos`/`rsin` in its sprite-primitive path). functions.h now declares
   `void AsmDrawObject(Object *object, Skeleton *camera, int roll)`. `DrawLandscape` (draw.c) passes the ship's
   roll as before; the other callers (group 2 and 1) pass 0 where the console left `$a2` stale.
4. **`ShipData.update` got a full prototype** in wipeout.h
   (`void (*update)(ShipData *, Object **, int, WeaponData *, ControlInput *)`), so the five update
   functions now have exactly that signature: `UpdatePlayerShip`, `UpdatePlayerShipStart` (`void *` became
   `Object **` / `WeaponData *`), `UpdateRescueDroid` (the `int` arguments that really carried pointers became
   `Object **` / `WeaponData *`), `UpdateDroneShipStart` (gained the two unused trailing parameters, because it
   is called with five arguments), `UpdateDroneShip` (already matched). No oracle change from this.
5. **`doSerialMenu(ConfigData *, MenuSelection *)`**: the second parameter was a `u_char *` read at byte offsets
   0x12, 0x14, 0x15; these are `MenuSelection.actTrackNum`, `.shipClass`, `.ship` (layouts.py: identical in
   both layouts, 188 bytes). **menus.c lines 704 and 751 still pass `(u_char *)work.selection`** and now warn
   (incompatible pointer types); they should pass `(MenuSelection *)work.selection`.
6. **racectrl.c (coordinator)**: `weaponGrids = DAlloc(heap, 0x420)` (line 742) is 64 `WeaponGrid` entries of 16 bytes
   plus 32; natively `WeaponGrid` is 24 bytes (`SetTrackAttributes`, dynam.c, writes `weaponGrid[n]` and there is no
   bound check), so it needs `64 * sizeof(WeaponGrid) + 32`. Group 2's note arrives at the same count.
7. **credits.c** defines 134 `char *` globals (`Mds`, `DirDev`, ... `RedB2`) as tentative definitions with no
   initialiser. On the console they were initialised data (consecutive 4-byte pointers at 0x80094538-0x80094798,
   each pointing at a string). If the data worker emits them too, the link has duplicate definitions; if it does not,
   every credit line is a null pointer. Decide who owns them (I left the definitions).
8. **tools**: `diag.py` for display.c no longer shows the `sprintf` deprecation (cc.sh was changed by someone).

## Edits by category

### Rule 1 / 3: pointer arithmetic and raw offsets
* control.c `UpdateShips`, `SortAllShips`: `(ShipData *)(n * sizeof(ShipData) + (int)ships)` -> `&shipIndex[n]`;
  `(ShipRank *)(i * sizeof(ShipRank) + (int)rank)` -> `&rank[i]`; the `<< 4`, `- n`, `<< 4` chain for `ShipData`
  (the literal 0xF0 stride) -> `&shipIndex[current->shipNo]`; `buildGhostReplayData((int)shipIndex)` -> `(shipIndex)`.
* drone.c `calcMyXPos`, `humanToDroneSpeed`: `&ships[shipNo]`.
* error.c, display.c: `(char *)OT[n] + 0x7FFC` -> `OT[n] + 0x1FFF`, `(char *)OT2[n] + 0x18FC` -> `OT2[n] + 0x63F`
  (the entries are 4-byte `u_long` in both layouts, so the byte offsets were right; the index form is the readable one).
* collide.c `Ship2Track`: the `SectionAttributes` union (`localRadius` and `flags` viewed as one `u_long`, tested
  with `& 0x180000`) is gone; the four tests are `nearTrkSect->flags & 0x18` (same bits; the old form depended on
  little-endian field order and 4-byte alignment of `localRadius`, offset 228 natively).
* combat.c `doSerialMenu`: byte offsets -> `MenuSelection` fields (above). `sendSerialPacket`, `receiveSerialPacket`:
  `(u_char *)(len + (int)p)` and `(int)p < (int)end` -> `p + len` and `p < end`.
* dynam.c `SetTrackAttributes`: Face colours. `Face.red/green/blue` are `u_char` now (wipeout.h; no layout change). The
  assignments of 255, 238, 128 were all -Wconstant-conversion warnings; every reader stores the byte into a `u_char`
  (crack.c) or views it as bytes (`(CVECTOR *)&face->red`, droid.c).

### Rule 2 / 5: declarations and prototypes
* Local externs replaced by globals.h (`OT`, `OT2`, `cameraPtr`, `CameraUpdate`, `tempGameData`,
  `reserveHeap`, `heap`, `nextPrim`, `gameType`, `menuSendFlags`, `menuSendKey`, `textureWad`, `finishPosition`,
  `linkSendFlag`, `playerPosition`, `remoteRacePos`, `shipTargetIndex`, `raceCompleteFlag`, `playerControl`,
  `numTrackSections`, `shipClassIndex`, `residentSkeletons`, `upperSkeleton`, `particleSlots`, `waveIndex`):
  collide.c, combat.c, control.c, crack.c, display.c, droid.c, drone.c, dynam.c, error.c.
* Added to globals.h (group 4 section): `activateOT2` (`signed char`, defined in main.c; display.c had `u_char`),
  `clipLevel` (`u_short`, draw.c and drone.c), `controlledSfxId` (`int`, defined in sound.c; droid.c had `short`),
  `DispEnv0`, `DispEnv1`.
* display.c: removed the local PsyQ declarations (`SetGraphDebug` returned `void`, clashing with psyq.h; `InitGeom`,
  `SetVideoMode`, `DrawPrim`, `SetBlockFill`) and `SfxDoSfx`; `menuSendFlags` is defined `u_char` (was `char`; the
  other files use `unsigned char`; it only receives `vcount / 262`).
* collide.c, dynam.c: removed the local `OuterProduct12` / `VectorNormal` declarations (psyq.h). error.c: removed
  `exit` (stdlib). combat.c: removed `_comb_control` (psyq.h).
* `GsClearVcount(0)` -> `GsClearVcount()` (display.c `Swap`; the library function takes no argument).
* `SectionSearchResidue(ship, track)` -> `SectionSearch(ship)` (drone.c `UpdateDroneShipStart`; the callee reads only `$a0`).
* combat.c `_error` returns `int` (0): `OpenEvent` takes `int (*)(void)`; the event service ignored the console
  handler's result.
* combat.c `memcpy_80089544` (declared locally, also in weapon.c) -> `memcpy`. This is the console's `memcpy`
  called by address; I identified it from its call sites (dst, src, byte count, result unused), not from the exe.
* draw.c: removed the second definition of `waveIndex` (progress.c owns it; globals.h declares it).

### Rule 4: sizes
* display.c: ordering tables `otBuffer0/1[0x2000]`, `ot2Buffer0/1[0x640]` (from `ClearOTagR(.., 0x2000)`,
  `ClearOTagR(.., 0x640)`, the last-entry offsets and `z % 1600` in object.c; the console reserved 0x2100 entries
  for the first pair, 33792 bytes between `otBuffer0` and `otBuffer1`), `PrimitiveBuffer0/1[0xAC00]` (44032 bytes,
  the distance between the console symbols), `doubleBuffer[2]`, `primBuffers[]`.
* credits.c `creditEntries[35]` (`InitCredits` fills 0 to 34; the console reserves 4200 = 35 * 120 bytes). Natively the
  `CreditEntry` is 240 bytes (pointers) and is only filled at run time, so any definition of the right element
  count works.
* crack.c `medLoCrackPolys[8][2]`, `hiMedCrackPolysA[8][4]`, `hiMedCrackPolysB[8][4]`: derived from the console symbol
  spacing (512 bytes after `medLoCrackPolys`; 64 + 1040 bytes for the two `hiMed` names). Nothing in the code bounds
  the face count by 8. Each of the two `hiMed` tables must really have 8 rows of 4 `PolyFT3` (1024 bytes), not the 64
  bytes between the console names.
* draw.c `clipNearTable1/2`, `clipFarTable1/2[8]`, `visibleObjects[128]` (the loop clears 128; the console data has 8 entries
  per table, `clipLevel` indexes them).
* No `DAlloc`, `memcpy` or loop count in my files is the size of a pointer-bearing struct. The packet sizes in combat.c
  are byte counts of pointer-free data: `LinkPacket` (66 bytes sent of 68) and the 10-byte menu packet.

### Rule 6: extensions
* collide.c: `track__dynam` ("track") and `tempGameData__dynam` ("tempGameData") aliases: both unused anywhere; deleted.
* drone.c: `SectionSearchResidue` ("SectionSearch"): used only by `UpdateDroneShipStart`; removed (above).
  `sarResidue` ("sar"): kept; it is used by `UpdateDroneShip` (line 448, `ship->vroll += sarResidue(ship->vhdg, 2, oldPitch)`).
  It is `sar(int a, int n)` called with a third register-residue argument (`oldPitch`); the host passes the extra argument
  in a register and `sar` ignores it, so the call is correct as written.
* No other `__asm__`, `__attribute__`, `extern inline` or `register ... asm` in these files. `register int a_mag`
  (dynam.c `GetTargetPos`) and `__builtin_abs` (droid.c) are plain C/builtins and left.

### Rule 7-10 findings

Rule 7 (`return;` in non-void): none.

Rule 8 (console memory): none in these files. `memcpy_80089544` was the only address-bearing name.

Rule 9 (file data): no file or memory card access in these files. combat.c reads and writes the serial port with
`PsyqRead/PsyqWrite` on byte buffers; the structs involved (`LinkPacket`, menu packet) are pointer-free and the same
size in both layouts. `sioDevice` is a data string.

Rule 10, uninitialised reads that the console performs and the port keeps:
* error.c `Error`: `screenPrim[]` is never assigned and is passed to `AddPrim` (Fatal errors only). Natively that is an
  arbitrary 8-byte value linked into the table; it will probably crash instead of showing the error screen.
* dynam.c `ShipTrkReaction`: `boost` (line 22) stays unassigned for a negative `thrustCount` (not reachable: the
  counter is only decremented while positive); `mass` (line 88) is compared with 110 in the airborne branch although it is
  only assigned in the ground branch (reachable: when `altitudeDifference > 704`).
* control.c `UpdatePlayerShip` (lines 403-417): `maxThrust` is unassigned when `raceType == 3` and `shipNo` is neither
  `ownShip` nor `remoteShip`.
* dynam.c `GetTargetPos`: `*result` is left unwritten when `a_mag` or `b_mag` is 0; the callers (`ShipTrkReaction`,
  `UpdateDroneShip`) read the uninitialised `targetPos` / `bestPath`.
* collide.c `NewCollideLineToPlane`: `p4` is read (to build `vec3`, only used for quads) after a triangle, and `p1`..`p3`
  are unset for a primitive type outside the eight handled; `collideLineToPlane` (untouchable) has the same for `p1`..`p3`.
* drone.c `UpdateDroneShip` (untouchable): `face` is passed uninitialised to `moveDownCentre` in the flying branch (the
  callee ignores it).
* combat.c `receiveSerialPacket`: if `PsyqRead` returns fewer bytes than asked, the checksum runs over uninitialised bytes.

Rule 10, division: none by an unchecked zero in reachable game states, but the divisors are data-dependent:
`PlaneLineCollision` (collide.c:757, `norm_dot_product`: zero only for a face normal shorter than 64 on every axis; also
the `INT_MIN / -1` case that the empty `if` after it is the console's check for), `GetMagnitude` results in drone.c
(397-399, 481, 484: zero only for coincident section centres), `friction` and `skid + (brake >> 2)` in dynam.c (74-76,
98-102, 159-163: positive for the ship tables), `alpha` in dynam.c:78-80 (at least 75) and drone.c:429-431 (at least 50),
`result.vz` in draw.c (greater than the near-clip table, which is at least 0x800), `depth` in combat.c:102 (the GTE flag test
rejects a zero depth). `turnDivisor` is forced to 1 when 0. Host division by zero is a SIGFPE; the console's was unpredictable.

Rule 10, other: shifts are all constant (`shift` is 4) or `sar`; no float; `char` signedness only matters for
`Face.texture` (index, below 24) and `Face.flags & 0x80` (works with the signed `char`, as on the console). Two
`RotTransPers` results are written through `(int *)` of a 2-byte-aligned object: crack.c `sxyA`..`sxyE` (five `DVECTOR`
locals) and combat.c `DrawLinkSign` `short sxy[2]` (untouchable). x86-64 and arm64 accept the unaligned store; strictly it
is undefined behaviour. `screen[3][2]` in `CrackLocals` is 4-aligned.

Ordering-table index not bounded: crack.c `OT[CurrentScreen] + (depth & 65535)`, combat.c `OT[...] + (depth >> 1)`:
`depth` can exceed the 0x2000 entries (the clip tables allow z up to 0x9538, i.e. OTZ 9550). The console wrote past the
table the same way.

### Declarations the untouchable functions use (what each is, how the bytes are viewed)

* collide.c: `collideLineToPlane` uses no alias. (`track__dynam` and `tempGameData__dynam` were unused.)
* drone.c `sarResidue` = `sar`, see above. `UpdateDroneShip` also views `ownShip` / `remoteShip` (an `int`) through
  `*(u_short *)&remoteShip` (line 156, 158): the low halfword on a little-endian host, which is the console's view.
  It reads `shipClassIndex`, `numTrackSections` (now `short`, was `u_short`; always below 0x8000), `playerPosition`,
  `remoteRacePos`, `gameType`, `clipLevel` as the shared types.
* combat.c `doSerialGame` declarations (lines 235-264): all are views of the console's variables.
  `playerControl`, `remoteControl`: `u16` = first halfword of `ControlInput[0]` (`remoteControl` is still the `u16` view; control.c
  declares `ControlInput remoteControl[]`; `playerControl` now uses the shared array). `linkPad*` / `remotePad*`: aliases
  inside those records (above). `raceEndState`, `remoteRaceDone`, `linkRaceDone`, `linkRaceEnd`, `linkSyncB`: `u8` views
  of variables that occupy 4-byte slots on the console (0x80094A0C, 0x80094A50, 0x80094D44, 0x80094D4C, 0x80094D58): the low
  byte on a little-endian host. If the data worker defines them as single bytes the reads are still right.
  `linkRaceFlag`, `attractShip`, `seedWep`, `myBestLap`, `myBestRace`, `remoteBestLap`, `remoteBestRace`: `s32`.
  `finishPosition` (shared `short`), `linkSendFlag` (shared `int`), `playerPosition` (shared `short`) are now read at
  their real width and narrowed by the assignment to a `u_char` field: the same low byte.
* combat.c `DrawLinkSign`: `cameraPtr` see item 1; `linkSign[]` (`POLY_FT4[2]`, 80 bytes on the console) and `linkSignTexture`.

## Oracle changes (18 functions)

* combat.c `_error`: one `li $v0, 0` added (int return).
* combat.c `doSerialGame`: `playerControl` is now an array (address built with lui/lhu instead of `$gp`-relative);
  `linkSendFlag` is loaded as a word instead of a byte (the byte is then stored, same value); branch targets move by
  4 bytes. (`finishPosition` and `playerPosition` compile to the same byte loads.)
* combat.c `sendSerialPacket`, `receiveSerialPacket`, and `doSerialTransfer` (which inlines both): `memcpy` instead of
  `memcpy_80089544`; the loop test on the end pointer is unsigned (`sltu`) instead of `(int)` signed (`slt`) (same result
  for any buffer that does not straddle 0x80000000); one commutative `addu` order; one fewer move.
* control.c `SortAllShips`, `UpdateShips`: operand order of one `addu` in `&array[i]`.
* display.c `Swap`: the ignored argument of `GsClearVcount` is not set (the store moves into the delay slot).
  `InitGameDisplay`, `InitMenuDisplay`: the screen offsets are stores to `DispEnv0/1 + 8, +10` instead of four `$gp`
  symbols; different registers.
* drone.c `UpdateDroneShipStart`: the `lw track` for the second argument of `SectionSearch` is gone.
  `calcMyXPos`, `humanToDroneSpeed`: same computation, `&ships[shipNo]` gives other registers and operand/sum order.
  `followRaceLine`, `lookForWayPast` (inlines it), `getShipInfrontLAP`, `getShipBehindLAP`, `UpdateDroneShip` (inlines all
  of these): `racingLine` -> `raceLines + 4`, `shipOrder` -> `shipRank + 2` (relocation symbol and +4 / +2 offsets).

`tools/oracle32.py` compares against the first function of the file; when an earlier function in a file changes
size, every later `jal` immediate differs. The final list above has no such artefacts (checked with the `jal`
immediates ignored).

## SectionPtr (`.ptr` / `.index`) uses

There is no `.index` use in these files and nothing writes a `SectionPtr`; every use reads `.ptr` after
`IndexToPointers` has run (race time), so it reads the full 8-byte slot.
* `nextSection.ptr`, `prevSection.ptr`: collide.c (Ship2Track), control.c (SortAllShips, SetUpFirstCheckPoint), draw.c
  (TrackDirection), droid.c (all droid updates, SetDroidVectors, InitRescueDroid), dynam.c (ShipTrkReaction), drone.c
  (UpdateDroneShip, 18 uses). These need real pointers.
* `junction.ptr != (TrackSection *)-1` (the "no junction" test): collide.c lines 395, 409, 432, 489, 505, 531 (Ship2Track), drone.c lines
  352 and 371 (UpdateDroneShip). This needs the slot to hold the full-width -1; track.c `IndexToPointers` stores
  `(TrackSection *)-1` for index -1 (a zero-extended file value 0x00000000FFFFFFFF would not compare equal).
* `junction.ptr != 0` (collide.c lines 380 and 473): true both for a junction and for the -1 "none" value, as on the console.
* `junction.ptr->flags`: a real pointer (only reached when the slot is not -1).
* `ShipData.nearTrkSect` / `prevShipSect` are plain `TrackSection *`.

## Raw-offset / size dependences found and not fixed

* Items 2, 5, 6, 7 above (other files, or the data worker).
* `ControlInput` / `LinkPacket` and the packet layouts are pointer-free and identical; `doSerialGame` reads
  `ControlInput` records at 10 bytes per record from `playerControl` (one record) and `remoteControl` (one record).
* combat.c `shipTargetIndex[i]` for `i < 15` and the `LinkPacket.targets[15]` bytes: 15 ships, as `SetUnitVectors`
  (`i < 15`) and `UpdateShips` (`i < 15`).
* display.c `PrimPtr` is `struct { char *ptr; }`; object.c declares the same object as `char *volatile nextPrim`, weapon.c as `ShadedPtr`:
  one pointer in all three views.
* The platform must give `AddPrim` / `DrawOTag` the 4-byte `u_long` table entries (`OT` entries are `u_long`, not pointers).

## Left as is (xdecl, or unclear)

* `CameraUpdate`, `clipLevel` (racectrl.c `short`), `OT`/`OT2` (racectrl.c `unsigned int *`, same type; animover.c local),
  `cameraPtr`: only racectrl.c / the coordinator's files disagree with globals.h.
* `remoteControl`: combat.c `u16` view vs control.c `ControlInput[]` (cross-file only, above).
* `numTrackSections` is `short` in globals.h; ships.c still has `u_short`.
* Unsure: the sizes derived from the console symbol spacing (OT 0x2100 entries, 8-row crack tables, 44032-byte
  primitive pools) come from address differences between symbols, not from code; the 0x2000 / 0x640 entry counts and
  the 35 credit entries are confirmed by code.
