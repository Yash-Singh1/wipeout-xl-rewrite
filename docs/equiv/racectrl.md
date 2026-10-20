# race() (game/src/racectrl.c)

The only C for `race` was a version proven equivalent to the retail code, written with private types
(`GameData`, `ShipData_eq1`, `Skeleton_eq1`, `Object_eq1`, `TrackSection_eq1`, `RaceLocals`,
`TrackCache`, `CameraSections`) and with `__asm__` aliases (`__v1`, `__eq1`) that name other files'
globals under other types. It is rewritten with the shared types. Statement order and arithmetic are
unchanged except where listed under "Not literal".

Evidence: `ORACLE_NOSP=1 ORACLE_OPT=-O0 python3 tools/oracle32.py diff B0-O0-nosp racectrl.c -v` compiles the old and the new
text for MIPS o32 without optimisation and ignores only stack slot numbers and branch targets. Every
remaining difference is explained in the tables below. The proofs of the field mappings are the 32-bit
(`ps1`) offsets of `python3 tools/layouts.py show TYPE`.

## Private view -> real field (32-bit offset)

| Private type, field (offset) | Real field | Proof |
|---|---|---|
| `GameData` `team` (4) | `ConfigData.trackNum` | offset 4. (Index of `checkCells[team][class]`; control.c indexes the same table with `trackNum`.) |
| `GameData` `trackNo` (5) | `ConfigData.actTrackNum` | offset 5 (the track number: 1, 2, 6, 7, 8, 13, 17, 20) |
| `GameData` `shipClass` (8) | `ConfigData.shipClass` | offset 8 |
| `GameData` `raceCount` (18) | `ConfigData.raceInfo` | offset 18 (short) |
| `GameData` `finished` (20) | `ConfigData.complete` | offset 20 (short) |
| `GameData` `weapons` (40) | `ConfigData.weaponsOn` | offset 40 (char; the old text read it unsigned, the test is `!= 0`) |
| `GameData` `venomComplete`, `rapierComplete` (57, 58) | same names | offsets 57, 58 |
| `TrackSection_eq1` `centerY` (16) | `TrackSection.centre.vy` | offset 16 |
| `TrackSection_eq1` `sectionNo` (152) | `TrackSection.secNo` | offset 152 (short; the old text read it unsigned: only used in `(unsigned)(secNo - N) < M` range tests on small values) |
| `Skeleton_eq1` `x,y,z` (20,24,28) | `Skeleton.relative.t[0..2]` | offsets 20, 24, 28 |
| `Skeleton_eq1` `dirty` (64) | `Skeleton.update` | offset 64 (short) |
| `Skeleton_eq1` `parent` (68) | `Skeleton.super` | offset 68 |
| `Object_eq1` `skeleton` (48) | `Object.skeleton` | offset 48 |
| `ShipData_eq1` `nearTrkSect` (4), `attr` (12), `ppivot`/`vpivot`/`unitVecNose`, `hdg..vhdg` (112..118) | same names | layouts.py |
| `ShipData_eq1` `state.half` (172, volatile u_short) | `ShipData.combatAttr` | offset 172 |
| `ShipData_eq1` `state.word` (172, 32-bit) | `combatAttr` (172) + `numToSpeed` (174) | the only use is `(state.word & 0x8004) != 0x8004`: the mask keeps only bits of `combatAttr`, so the test is `(combatAttr & 0x8004) != 0x8004`. The oracle shows `lw` -> `lhu`. |
| `ShipData_eq1` `weapon` (186) | `ShipData.weapon` | offset 186 |
| `ShipData_eq1` `timer` (194) | `ShipData.autopilotCount` | offset 194 (short) |
| `ShipData_eq1` `update` (236) | `ShipData.update` | offset 236 |
| `CameraSections` (`section` + 5 words) | `TrackCamera` fields after `camPos` | see below |
| `TrackCache` (`count` 0x9000, `draw` 0x9002, 0x9004, `mode` 0x9006, `active` 0x9008, 34 bytes) | `AutoMeshData` (`quadCount`, `triCount`, `attemptedAutos`) + new `mode`, `active`, `reserved[34]` | track.h; 0x902C bytes in all |
| `RaceLocals.inputPacket[32]` | `ResetData` (7 pointers = 28 bytes on the console, 56 natively) | `Restart` and `InitRaceResetData` take `ResetData *` |
| `RaceLocals.cdState[80]` | `Object *crowdObjects[20]` | `InitCrowdSound(prm, n, Object **destination, count)` stores `Object *` into it, `AdjustCrowdSound(Object **, count)` reads them. 80 bytes = 20 console pointers. No track has more than 7 crowd objects ("lost*" names in the WADs: 0 for tracks 1, 2, 4, 7, 13, 17; 3 for tracks 6 and 8; 7 for track 20) |
| `RaceLocals.cdLocations[336]` | `union { WipeCdStatus; u_char result[8]; }` | `SetInGameCDTrack` passes it to `CdControlB(CdlGetlocP)`, which returns 8 bytes |
| `RaceLocals.inGameMenu[3312]` | `InGameMenu` (3208 bytes) | the other 104 bytes are not referenced by any callee (`PerformPauseOperations` views it as `SelectBox`, i.e. its first fields) |
| `RaceLocals.cdTrack` (short, passed as `signed char *`) | `signed char cdTrack` | `SetInGameCDTrack` writes one byte |
| `RaceLocals.frameCount` | `tileCount` | `WADLoadTtfFile(wad, short *count)` and `AsmGenerateTiles(tiles, tileCount)` |
| `RaceLocals.checkpoint`, `checkpointLap` | `noWeaponGrids` (int), `specialCount` (short) | `InitWeaponGrids(grids, track, int *noWeaponGrids, short *specialCount, SpecialGrid *)`; `CycleWeaponGrids(.., noWeaponGrids)` |
| `RaceLocals.lightCountdown` (u_short) | `skyIndex` | returned by `WADInitScene` (index of the sky object in `prmObjects`) and used as `prmObjects[(short)skyIndex]` |
| `RaceLocals.needEndText`, `needWinLose` | `rainTimer`, `thunderAt` | names only |
| `RaceLocals.scene` | the parameter `scene` | the copy was only read once |

## Names that stood for storage inside other objects (the symbol addresses are in ../wipeout-xl/decomp/symbol_addrs.txt)

| Console symbols | Real object |
|---|---|
| `residentSkeletons` (0x800BD2E0) + `cameraSections` (0x800BD2E4, 24 bytes) | one `TrackCamera` (28 bytes), now `raceCamera` in globals.h. `cameraPtr` points at it, `CameraUpdate(camera, ...)`, `DrawShips(.., TrackCamera camera, ..)` (by value: the callee's 7 words are `residentSkeletons` + the 6 words of `cameraSections`), `InitCamera`, `AdjustEngineSound`, `DrawLandscape`, `DrawAllTrack`... all take it. `residentSkeletons[0]` is `raceCamera.camPos` everywhere in the game; `cameraSections.section` is `raceCamera.section`. |
| `raceCheckpointResults` (0x800A4012), `raceCheckpointFlags` (0x800A4014) and `checkCells` (0x800A4010) | `CheckCell.bonusSeconds` (+2), `.done` (+4), `.section` (+0) of the 6-byte `checkCells` records |
| `smokeColour`, `smokeColourGreen`, `smokeColourBlue` (0x80094C3C..3E) | `CVECTOR smokeColour` (.r .g .b) |
| `rescueData` + `rescueUpdateFunc` (the word right behind it) | `RescueData` (76 bytes); `rescueUpdateFunc` is `RescueData.droidUpdate` (offset 72) |
| `monoRailFlag` | the first field (`index`) of `MonoRailData monoRail` (48 bytes) |
| `pauseState[0]`, `pauseState[1]` | `PauseMenu pauseState` fields `selection`, `musicPaused` (RaceData was a second copy of PauseMenu and is gone) |
| `teamShip` | array `Object *teamShip[]` (`LoadModel(Object **, ...)` stores the model's objects in it) |
| `weaponGrids` (int) | `WeaponGrid *` |
| `lightDroid` (int) | `Object *` |

## Not literal (each verified in the oracle diff)

* `divideSix(value, magic)` (a 64-bit multiply by `locals.divisionMagic = 0x2AAAAAAB` and a high-word
  extraction) is `value / 6`. The retail code is `mult; mfhi; sra 31; subu` (race.s), which is how GCC divides by 6; clang
  generates the same sequence for `/ 6` (the oracle shows `lui 0x2aaa / ori 0xaaab / mult / mfhi / srl 31 / addu`). The
  local `divisionMagic` is gone.
* `volatile` pointer-cast reads (`*(Skeleton_eq1 * volatile *)&obj->skeleton`, `*(ShipData_eq1 *volatile *)&shipIndex`,
  `volatile u_char firstTime`): plain reads (single-threaded; they existed to pin GCC's scheduling).
* `*(Object_eq1 **)((char *)prmObjects + (int)((u_int)(u_short)x << 16) >> 14)`: `prmObjects[(short)x]` (the 4-byte stride is
  the console's pointer size).
* Calls made with arguments the callee never reads are made without them (`PerformPauseOperations`, `InitiateTwoPlayerPause`,
  `Perform2PlayerPauseCommand`, `RaceComplete`, `InitCheckPoints`, `WADLoadTtfFile`, `rescueData.droidUpdate`), and
  `DrawLinkSign(shipIndex)` passes `$a0`, which held `shipIndex` in the retail code (`lw $a0, shipIndex` just before the call).
  `CheckGamePause(.., &keysPressed, ..)` passes the address of `keysPressed` (the callee writes 0 through it).
* `trackCache` is allocated with `sizeof(AutoMeshData)` (was `0x902C`).
* `AddRain`/`AddSnow` read the camera position from the first three words of `camPos->relative.t`, passed as `VECTOR *`
  (`pad` is never read).

## Oracle residue (all explained)

`lbu` -> `lb` (`weaponsOn`), `lhu` -> `lh` (`secNo`): values far below 0x80. `lwl/lwr` -> `lw` on the `prmObjects` slot (the old cast
made the compiler assume a misaligned pointer). `AddSmallSpurtExplode(.., i)`: the `ship` parameter is a `short`, so the new text
sign-extends `i` (0..14). Everything else in the diff is the renamed symbol (`raceCamera`, `monoRail`, `smokeColour+1/+2`,
`checkCells+2/+4`).

## Uninitialised reads and faithful behaviour

* `race` has no uninitialised local. `locals.crowdSample`, `rainTimer`, `thunderAt` are set at the top.
* `AdjustCrowdSound`, `InitCrowdSound`: the crowd array is 20 entries; more than 20 "lost*" objects in a scene would overrun it
  (as on the console, where the next member was `cdLocations`).
* `shipTargetIndex[i]` is read for `i < 15` and `ownShip`/`remoteShip` may be -1 (`shipIndex[remoteShip]` with `remoteShip == -1` in
  single-player reads the 16 bytes before the array: `gameType != 2` guards the two uses).
* `cardNum`/`heading` (the menu ship's rotation) are decremented every frame of the race, as on the console.
