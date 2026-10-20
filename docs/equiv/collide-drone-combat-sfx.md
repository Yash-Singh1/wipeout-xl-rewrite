# Equivalent-only functions: collideLineToPlane, UpdateDroneShip, DrawLinkSign, doSerialGame, SfxChainId

Files: `game/src/collide.c`, `drone.c`, `combat.c`, `sfx.c`. One shared header line added:
`extern ControlInput remoteControl[];` in `game/include/globals.h` (next to `playerControl`; `control.c`
declares the same thing locally, same type, no conflict, the local line can go).

Evidence used:
* `python3 tools/layouts.py show ...` for the PS1 (o32) offsets.
* The retail machine code in `../wipeout-xl/decomp/asm/nonmatchings/{collide,drone,combat,sfx}/*.s` (offsets,
  byte/halfword widths, which register is live) and the console symbol addresses in
  `../wipeout-xl/decomp/symbol_addrs.txt`.
* The oracle (`tools/oracle32.py`, MIPS o32, snapshots B0 and B0-O0, i.e. the previous text of these functions).
  `diag.py` prints nothing for the four files. For the -O0 comparisons I also compared the listings with all register
  numbers, branch targets and `$sp/$fp` offsets masked (script kept outside the repository): that leaves only the
  differences listed below, so the sequence of loads, stores and calls is identical, instruction for instruction.

## collideLineToPlane (collide.c)

Only the two address chains were wrong natively; the function has no other raw offset, no private type, no alias.

| old text | new text | proof |
|---|---|---|
| `(ShipData *)(((n << 4) - n) << 4) + (int)shipIndex` (twice: `shipNo1`, `shipNo2`) | `&shipIndex[shipNo1]`, `&shipIndex[shipNo2]` | `layouts.py`: ShipData is 240 bytes on the PS1 (host 256). 240 = ((n<<4)-n)<<4, so the console stride was `sizeof(ShipData)`. |

Fields used through those pointers are all real names already: `highResTableIndex` (ps1 0), `lowResTableIndex` (2),
`ppivot` (64), `hdg` (112), `pitch` (114), `roll` (116), and `shipIndex[shipNo1].highResTableIndex/lowResTableIndex`.
`Poly`/`Primitive` pointers are walked with `poly.f3++` etc. (pointer-free primitives, same size natively). The VECTORs
passed "by value" are the real functions `MinusVector`, `ScalarProduct`, `CrossProduct`, `GetAng`, `assign` (utils.c),
not macros; VECTOR is 16 bytes, pointer free: nothing to change.

Memory past a struct end / ambiguous views: none.

Oracle: -O2: one CHANGED function, hunks are only `addu $20,$1,$7` vs `addu $20,$7,$1` (operand order of the address add,
twice). -O0: the old chain (`sll 4; sw; lw; subu; sw; ...` through a stack temp) is replaced by `n*16, n*256, subu` =
n*240, then the same `addu` with the same `lw 0x48(..)` (highResTableIndex) afterwards; everything else identical (branch
targets shift by the shorter prologue).

Uninitialised reads / bugs kept (not new):
* `p1`, `p2`, `p3` are only assigned for TypeF3/G3/FT3/GT3. For any other primitive type the `default: break;` neither
  assigns them nor advances `poly.prim`, so the same primitive is tested again with stale (first iteration:
  uninitialised) points, `primitiveCount` times. Presumably the low-resolution ship models contain triangles only.
* `skeletonWorkspace[40]`, `shifted` are unused locals (kept, no effect).

## UpdateDroneShip (drone.c)

| old | new | mapping / proof |
|---|---|---|
| private `held` struct with `pad04`, `pad0C`, `pad12[3]`, `pad1A[3]` (frame slots sp+0xB8..0xD8) | plain locals `lateral`, `along`, `weapons` (`WeaponData *volatile`), `humanMoveDist`, `boostStep`, `humanNo` (volatile as before) | They were stack slots, not fields of any object: the padding only imitated the console's frame (32-bit pointer slot at +8). The only two address-taken ones are `lateral`/`along`, passed to `calcMyXPos`, which stores each separately (`*lateral`, `*along`), so their relative placement is irrelevant. The volatiles are no-ops natively (no other writer); kept so the code shape is the one that was verified. |
| `*(short *)&held.humanNo = *(u_short *)&remoteShip` / `&ownShip` | `humanNo = remoteShip;` / `ownShip` | `remoteShip` (0x80094A18) and `ownShip` (0x80094B94) are 4-byte ints (all files declare `int`; `main.c` assigns `ownShip = newRand() % 14`). The console read the low halfword (`lhu`) and stored it with `sh`; conversion int -> short keeps exactly those 16 bits. No value outside 0..14 can occur anyway. |
| `*(volatile u_char *)&held.humanNo` (twice, `ship->targetShip = ...`) | `(u_char)humanNo` | low byte of the short, little endian, same value. |
| `RELOAD(held.humanMoveDist)` | `humanMoveDist` declared `volatile short` | every use but the store was through `RELOAD`; same reads. Macro removed. |
| `sarResidue(ship->vhdg, 2, oldPitch)` with `int sarResidue(int,int,int) __asm__("sar")`, `oldPitch = ship->pitch` inside the `vpitch` expression | `sar(ship->vhdg, 2)`; `oldPitch` removed (the expression is `-ratan2(...) - ship->pitch`) | `sar` is the two-parameter function of utils.c (`int sar(int a, int n)`, declared in functions.h; its body reads only `a` and `n`). The retail call has `lhu $a2, 0x72($s1)` (the old pitch) still in `$a2` by accident: the register is not an argument. `ratan2` cannot change `ship->pitch`, so reading it after the call is the same value. |

Other fields accessed (all by name already): `ship->numToSpeed`, `combatAttr`, `remoteMax`, `speed`, `remoteMagnitude`,
`autopilotCount`, `nearTrkSect`, `nearSecNo`, `moveDist`, `weapon`, `targetShip`, `attr`, `apivot`, `vpivot`, `ppivot`,
`vhdg/hdg/ahdg`, `vpitch/pitch`, `vroll/roll`, `electroCount`, `unitVecNose`, `update`. They move natively but are
accessed by name. `(TrackSection *)-1` is the sentinel track.c itself stores in `junction.ptr`. Data tables
(`droneInitVector`, `classSpeedLimit`, `classSpeedMargin`, `classAccel`, `classSteer`, `pitLaneSection1/2`,
`pitLaneDirection`, `shipDamage`, `raceLines`) are externs of pointer-free data defined by the data side (`classAccel`, `classSteer`, `droneInitVector` are consecutive
4-short tables at 0x80094920/28/30 in the console).

Memory past a struct end / ambiguous views: none left. (`speedBonus.value[shipClassIndex]` indexes a 4-short copy with
`shipClassIndex`, the same as the console; the index range is the game's.)

Uninitialised reads / UB kept:
* `face` is not assigned in the flying branch and is passed to `moveDownCentre` (it ignores its `segment` argument).
  clang warns (`-Wuninitialized`); the passed value is never used.
* `GetTargetPos` leaves `*result` unwritten when a magnitude is zero (callee side): `bestPath` is then read
  uninitialised (group 4 note).
* `unused` (VECTOR) is an unused local.
* Divisions by `magnitude` and `alpha` are unchecked, as on the console (`alpha` is at least 50 in the track branch;
  `magnitude` is zero only for coincident section centres).
* Pre-existing, from group 4 and not part of this task: `numTrackSections` is `short` (was `u_short`); the oracle shows
  `lhu` -> `lh` for it (always < 0x8000).

Oracle: -O0 (masked) differences, all explained: `lhu`->`lw` of `ownShip`/`remoteShip` (above); `lhu`->`lh`
`numTrackSections` (3x, group 4); `lbu`->`lhu` of `humanNo` (2x; the following `sb` stores the low byte either way);
`&along` computed as `$fp+off` instead of `($fp+off)|2`; the `oldPitch` temp (stack store/reload) is gone. Everything else
identical, and the frame-slot mapping between old and new is one-to-one for the 86 slots used. -O2: register allocation
and scheduling differ everywhere (expected, the frame changed and `shipOrder`/`shipRank` and `numToSpeed` types were
edited by group 4); not instruction-comparable.

## DrawLinkSign (combat.c)

No edit in this task: the only representation problem, `(*cameraPtr)->...` (a `Skeleton **` on the console), was already
rewritten by group 4 to `cameraPtr->camPos->...`. Proved against the retail code: `lw $a0, cameraPtr; lw $v0, 0($a0)`
(first word of the `TrackCamera` = `camPos`) then `lw 0x14/0x18/0x1C($v0)` = `Skeleton.relative.t[0..2]` (ps1 20, 24, 28),
`lw 0x44($v0)` = `Skeleton.super` (ps1 68, moves natively, accessed by name), `sh 1, 0x40(super)` = `.update` (ps1 64).
`ShipData`: `lw 0xC($v1)` = `attr` (12), `0x40/0x44/0x48` = `ppivot` (64/68/72), stride `sll 4; subu; sll 4` = 240 =
`&ships[remoteShip]`. `linkSign[]`, `OT[]` entries and the `POLY_FT4` are pointer-free, 4-byte `u_long` entries.
Oracle: no change at all, -O2 and -O0.

Kept / reported:
* `RotTransPers(&svec, (int *)sxy, ...)` stores a 4-byte value into `short sxy[2]` (2-byte aligned): the same pattern as
  crack.c/sparks.c/effects.c; accepted by x86-64 and arm64.
* `size = 0x6400 / depth`: `depth` is zero only if the GTE rejected the point, which the preceding flag test excludes.
* The ordering-table index `depth >> 1` is not bounded (as on the console).

## doSerialGame (combat.c)

The five `linkPad*` and six `remotePad*`/`remoteControl` names were not variables: they were fields of
`playerControl[0]` and `remoteControl[0]`. Symbol addresses (symbol_addrs.txt) against `ControlInput`
(`layouts.py`: head 0, buttons 2, pressed 4, pad6 6 [u_char[2]], thrust 8, steering 9; size 10):

| old name | console address | = base + | real field (offset, size) |
|---|---|---|---|
| `playerControl` (u16) | 0x800F1DDC | 0 | `playerControl[0].head` (0, 2) |
| `linkPadHead` | 0x800F1DDE | 2 | `playerControl[0].buttons` (2, 2) |
| `linkPadButtons` | 0x800F1DE0 | 4 | `playerControl[0].pressed` (4, 2) |
| `linkPadPressed` | 0x800F1DE2 | 6 | `playerControl[0].pad6` (6, 2 bytes, read as one `u_short`) |
| `linkPadAux0` | 0x800F1DE4 | 8 | `playerControl[0].thrust` (8, 1) |
| `linkPadAux1` | 0x800F1DE5 | 9 | `playerControl[0].steering` (9, 1) |
| `remoteControl` (u16) | 0x800F9A48 | 0 | `remoteControl[0].head` |
| `remotePadHead` | 0x800F9A4A | 2 | `remoteControl[0].buttons` |
| `remotePadButtons` | 0x800F9A4C | 4 | `remoteControl[0].pressed` |
| `remotePadPressed` | 0x800F9A4E | 6 | `remoteControl[0].pad6` (as `u_short`) |
| `remotePadAux0` | 0x800F9A50 | 8 | `remoteControl[0].thrust` |
| `remotePadAux1` | 0x800F9A51 | 9 | `remoteControl[0].steering` |

The packet therefore carries the 10 bytes of the controller record, shifted nowhere: `LinkPacket.control, padHead,
padButtons, padPressed, padAux0, padAux1` are renamed `head, buttons, pressed, pad6, thrust, steering` (same types and
order; the old names were each one field off). The `pad6` halfword is accessed as `*(u_short *)record.pad6`, the way
pad.c already does (`pad.c:98, 217`, it holds the previous-frame buttons); the field is 2-aligned. Locals were renamed
to the same field names. `LinkPacket` is pointer-free: offsets 0..65 (`posX` 0, `bestLap` 12, `attractShip` 20, `seedWep` 24,
`hdg` 28, `head` 34, `buttons` 36, `pressed` 38, `pad6` 40, `thrust` 42, `steering` 43, `state` 44 .. `raceDone` 50,
`targets` 51), size 68, identical natively and on the console (checked by compiling it); 0x42 = 66 bytes are exchanged,
the last two bytes of `send`/`recv` are padding (`send` leaves them uninitialised, not transmitted).

Other views of 4-byte slots: `raceEndState` 0x80094A0C, `remoteRaceDone` 0x80094A50, `linkRaceDone` 0x80094D44,
`linkRaceEnd` 0x80094D4C, `linkSyncB` 0x80094D58 each own a 4-byte slot on the console (next symbols 4 bytes later) and
the retail code reads/writes them with `lbu`/`sb`. They are declared `u_char` in every file that uses them (ayrace.c,
hud.c, racectrl.c, combat.c; there is no shared declaration and no definition in `game/src` yet). The `u8` externs of
combat.c became `u_char` (same type). No change of meaning: whoever defines them must make them at least one byte; a
wider definition is also fine (little endian low byte, and nothing writes more than a byte). Same remark for
`raceCompleteFlag` (globals.h, `u_char`, 0x80094A14). `linkSendFlag` is read by the console with `lbu`, now an `int`
(globals.h) whose low byte goes into `send.sendFlag`: same value (group 4 change).

Oracle: -O0 with registers masked: the six loads of `playerControl` (+4, +6, +8, +9, +0, +2) replace the six `linkPad*`
absolute loads in the same order, the six stores into `remoteControl` (+0, +2, +4, +6, +8, +9) replace the `remote*`
stores in the same order (one `lui/addiu` for the base appears once per side); `linkSendFlag` `lbu`->`lw`
(group 4); the rest is identical. -O2: clang merges the adjacent `pressed`+`pad6` loads into `lwl/lwr` and the stores
into `swl/swr` (same bytes) and reorders; not instruction-comparable.

Uninitialised / quirks kept: the `send` padding bytes; `recv` bytes after a short read (`PsyqRead` result unchecked in
`receiveSerialPacket`, not in this function); `recv.targets[i] == -1` entries keep the old `shipTargetIndex`.

Callers that still use the old names (other workers' files; natively these would be separate, unresolved variables):
`menus.c` lines 425-427, 707-713 (`linkPadHead` = `playerControl[0].buttons`, `linkPadAux0` = `.thrust`, `linkPadAux1` =
`.steering`), `ayrace.c` lines 50-51, 177 (`linkPadAux0`, `linkPadAux1`). `ghost.c` has the same pattern with
`ghostControl` (group 4 note).

## SfxChainId (sfx.c)

No private type, raw offset or alias. Changes: the two bare `return;` in an `int` function.

| test | retail code | new |
|---|---|---|
| `!sfxInitialized` | `lbu $v0, sfxInitialized; beqz $v0, exit` : `$v0` = 0 | `return 0;` |
| `sfxEnabled` | `lbu $v0, sfxEnabled; bnez $v0, exit` : `$v0` = the flag just found non-zero | `return sfxEnabled;` (only 0 and 1 are ever stored; the group 6 note says 1: same) |

The inverted `if (sfxEnabled) return;` is the retail test (`bnez`; `SfxPlayId` has `beqz`), kept; the function does
nothing while sound is enabled. It has no caller in the game. Everything else is unchanged and pointer-free
(`SfxSound`, `SfxVoice`, `SfxEffect` have identical layouts natively). Known, kept: `effectList[sample]` with a negative
`sample` (error codes of `SfxTransEffect`) reads before the table; `attr` (SpuVoiceAttr) fields other than
voice/mask/addr/pitch are uninitialised, as on the console, and the platform reads only the masked fields.

Oracle: -O0: only the two early returns now store 0 / the flag into the result slot (the old text left it undefined);
other hunks are branch target shifts. -O2: clang compiled the old bare `return;` as undefined and restructured the
function around it, so it is not comparable; the -O0 evidence is the relevant one.

## Unsure / for the coordinator

* The volatile qualifiers and `(volatile u_char *)&ship->weapon` reads in UpdateDroneShip are kept as they were; they
  are semantically empty natively.
* `racectrl.c:378` has `UpdateDroneShip__v1[] __asm__("UpdateDroneShip")` (coordinator's file).
* `menus.c`/`ayrace.c` uses of `linkPad*` above must change.
* Nothing in these five functions depends on the console memory map.
