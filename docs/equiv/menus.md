# menus.c: MenuControl, CopyGameData, DisplayCardBack

State: `python3 tools/diag.py game/src/menus.c` prints one warning, in `GetTrackName` (no `return` after the
`switch`, already listed in docs/port-notes/group1-menus.md; not one of these three functions). None of the three
functions produces a diagnostic. No `__asm__` alias, `register`, private padded type or raw `(s32)`/`(int)` address
arithmetic is left in them.

Header edits (both exact-string): `game/include/functions.h`: `void DrawDrBox(Texture *unused, DrBox *box)` (was
`int unused`; the caller passes the texture pointer, the callee never reads it) and `CopyGameData(..., HiScoreData *cardState)`
(was `char *cardState`). The definitions in menus.c changed to match. Nothing else calls either function.

## How this was verified (three kinds of evidence)

1. **Oracle, B0 baseline** (`tools/oracle32.py diff B0 menus.c`, `ORACLE_OPT=-O0 ... B0-O0`), plus a scratch comparison that
   renames stack slots by first use (so a frame that shrank by 4 bytes does not show as a difference) and normalises
   `$gp`/`lui+%lo` addressing of globals. Reported below per function.
2. **Differential run against the retail machine code.** B0 is the imported text compiled by clang. It is not the retail
   code, and for `DisplayCardBack` the two disagree (see there). So every function was also run, inside a MIPS emulator
   (Unicorn, R3000, little endian), against the retail instructions of the PlayStation executable and against the new text
   compiled for MIPS o32 (clang, `-O2` and `-O0`, linked at fixed addresses). Both start from identical random memory
   (globals, shared buffers, a zero or random stack), callees are stubs that log their arguments (pointers are logged as
   `symbol+offset`), and the comparison covers: every logged call and argument in order, every global that the function
   references, and every shared buffer. Result lines are in "Differential results" at the end. The runner was made
   mutation-sensitive: changing one constant in the new text (a stride, a field index, a bit mask ...) is detected, see the
   list per function. The scripts are scratch (`/tmp/xc/diff/*.py`, not in the repository).
3. `layouts.py show` for every type named below (32-bit offsets, `ps1` column).

## MenuControl

Edits, all representation only:

| Before | After | Proof |
|---|---|---|
| `MenuWorkspace.selection` `char[192]` | `MenuSelection selection` (188 bytes) | sizes: `layouts.py` MenuSelection 188. The last 4 bytes of the old array were never addressed (every use is through a `MenuSelection`/`SaveSelection` pointer, both 188 bytes). This is the only frame-layout change: slots after it move by -4 (`addiu $x,$sp,0x1a58` -> `0x1a54`, `0x1aa0` -> `0x1a9c`, `0x1e90` -> `0x1e8c`, `0x2320` -> `0x231c`; frame 0x2500 -> 0x24f8); with 4 padding bytes added back in a scratch copy the O0 code is instruction-for-instruction the old one. |
| `char cardState[72]` + `*(short *)(work.cardState + 38) = 180` | `HiScoreData cardState`, `work.cardState.charX = 180` | HiScoreData is 72 bytes; `charX` is `short` at ps1 offset 38 (exact). |
| `volatile int musicTexture` holding `(int)textureTable[..]` | `Texture *volatile musicTexture` | the int held a pointer. |
| `(MATRIX *)skeleton`, `(MATRIX *)((char *)skeleton + 32)` | `&skeleton->relative`, `&skeleton->absolute` | Skeleton `relative` at ps1 0, `absolute` at ps1 32 (and host 0, 32). |
| `raceTrack` (`__asm__("raceTracks")` byte) | `raceTracks[0]` | same symbol, first byte. |
| `padBufs` (`__asm__("padBuf1")`) | `padBuf1` | same symbol (`padBuf1[0]` as the array decays). |
| `(KeyConfig *)savedKeyConfig` etc. | `(KeyConfig *)&savedKeyConfig` | `savedKeyConfig` is a `CardKeys`, the callees take `KeyConfig`. Both are 62 bytes, align 2: `KeyConfig.keys[2]` ps1 0 = `CardKeys.pad[2][9]` (36 bytes), `KeyConfig.active`+`pad25` ps1 36..37 = `CardKeys.reserved` (u16 at 36), `KeyConfig.calibration[2]` ps1 38 (24 bytes) = `CardKeys.other[2][6]` ps1 38 (24 bytes). The cast stays: it is a view of one object by its two owners (card.c, menus.h). |
| `linkPadHead`, `linkPadAux0`, `linkPadAux1` (three externs) | `playerControl[0].buttons`, `.thrust`, `.steering` | per the coordinator; checked: ControlInput ps1 offsets buttons 2 (u16), thrust 8 (u8), steering 9 (u8); the console symbols are playerControl+2, +8, +9. Externs deleted. |
| `residentSkeletons[0]` | `raceCamera.camPos` | done by the coordinator (globals.h); the O2/O0 diff shows only the symbol name. |
| `((void (*)(int))CdMusicPlay)(musicIndex)` | `CdMusicPlay(musicIndex, 0)` | the second parameter `unused` is never read (sound.c). Old code left a stale `$a1`. Oracle: one extra `li $a1,0`. |
| `((void (*)(void))UpdateNotes)()` | `UpdateNotes(0)` | `UpdateNotes(int unused)` ignores its argument (sound.c). |
| `VSync()` | `VSync(0)` | **uncertain, see below.** |
| `PrintCX(.., 8)` x3 | `PrintCX(.., x, y)` | PrintCX has 3 parameters; the 4th (size) was never read. |
| `(int)&work.musicScreen` | `&work.musicScreen` | `MenuFunction` last parameter is `void *`. |
| `DrawDrBox(work.musicTexture, ..)` | unchanged text; prototype now `Texture *` | see header edits. |
| `savedKeyConfig`, `work.selection`, `work.cardState` passed to `CopyGameData`/`MenuFunction`/`doSerialMenu`/`InitTempData`/`ShowMenu` | `&savedKeyConfig` (cast), `&work.selection`, `&work.cardState` | typed now; no cast needed except the KeyConfig one. |
| identity casts `(HeapBlock *)reserveHeap`, `(u_long *)OT[..]`, `(Skeleton *)upperSkeleton`, `(Object **)shipShapeTable` | removed | the globals already have those types. |

Views that remain (all on pointer-free data, identical on both layouts):
`(DrBox *)&work.musicScreen` (DrBox is 528 bytes, `coords` ps1 0, `rows` ps1 48; it is the first 528 bytes of the 1008-byte
`MusicScreen`, whose `pad0[528]` covers it); `(DrBox *)work.displayScreen` (`char[1168]`, only a DrBox is ever used);
`(MenuPrims *)&work.menuPolys` (`MenuPrims` is an incomplete type; `InitMenuPolys` casts it back to `MenuPolys`);
`(struct WipeCdStatus *)work.cdState` (the 8-byte buffer is the CD controller's result buffer; WipeCdStatus is 5 bytes;
the buffer is kept at 8 bytes on purpose); `(int *)&work.cdTrack`. `DAlloc(.., 0x6A70)` is 24*1134+32 and
`DAlloc(.., 0x10760)` is 24*2808+32 (`MenuEntry` 1134, `MenuLayout` 2808, pointer-free, equal on both layouts).
`RESERVE_HEAP_SIZE` is the coordinator's scaled macro (O0 diff: `lui 0xe` -> `0x1c`).

Reads of memory past a struct end: none in these views. `work.musicX..musicHeight` / `screenX..screenHeight` are stored
but never read (as on the console).

### Uninitialised reads and behaviour kept
* `work.cdState` is passed uninitialised to `SetInGameCDTrack`, which reads `position->time` before `CdControlB` fills the
  buffer (first iteration), exactly as the console did.
* `work.selection` and `work.cardState` are only partially initialised by `InitTempData`/`InitScoreTabData`; later code
  reads the rest as the console did.
* **`VSync(0)`**: the console called `VSync` with whatever `$a0` held after `UpdateNotes`/`SfxDoSfx` returned (the retail
  code does not set `$a0`; the same holds for `UpdateNotes`). It cannot be recovered statically (it depends on
  `SfxDoSfx` state). `0` (wait for the next vertical blank) is the PsyQ idiom and makes the loop wait five frames; I could
  not prove it equals what the console passed. This is the one place where I chose a value.
* The dead locals `gap[2]` and `trackList` (`trackList = defaultTrackList[0]` is a 15-byte copy that is never read) stay.
* `*(volatile short *)&menu[1].selection = ...` and the other `volatile` accesses of `work.*` stay (no effect natively).

### Oracle result
`-O0` (stack slots renamed): identical to B0 except (a) `InitDynamicMem` size (`RESERVE_HEAP_SIZE` scaled x2), (b) the
`residentSkeletons` -> `raceCamera` symbol name (same `lw`), (c) `li $a1,0` added before `CdMusicPlay`, (d) `UpdateNotes(0)`
and `VSync(0)` now load `$a0` (one spill/reload at O0), and, without the 4-byte pad, (e) the -4 frame shifts listed above.
`-O2`: the same, plus register-allocation order differences after the `CdMusicPlay` call. Nothing else differs.

## CopyGameData

| Before | After | Proof |
|---|---|---|
| `cdFlagWord` (`__asm__("cdFlag")` int) = 1 | `cdFlag[0] = 1` | same symbol, `int cdFlag[]` (menus.c / main.c). |
| `__builtin_memcpy(&selection->scoreName, &defaultCardNameWord, 6)` | `__builtin_memcpy(&selection->scoreName, defaultCardName, 6)` | `defaultCardName` is the 6-byte `Label6`. **The 6 bytes span two fields**: `MenuSelection.scoreName` (int, ps1 128) and `scoreNameEnd` (short, ps1 132), contiguous, no padding, 6 bytes = exactly `Label6`. Kept as a 6-byte copy (the header names the two fields separately; nothing else uses them). |
| `char *cardState` | `HiScoreData *cardState` | `strcpy(cardState, selection->fileName)` writes at offset 0 = `HiScoreData.nameEntered` (u8[9], same size as `MenuSelection.fileName[9]`); `InitDefaultName(gameData, (HiScoreData *)cardState, 1)` already took the type. |
| `SetVolume(*(short *)&ambientVoice1, 0)` | `SetVolume(ambientVoice1, 0)` | `SetVolume(short, int)`: the conversion int->short is the same low-half value as the little-endian view; O0 code identical. |
| (already real types) `selection->...`, `gameData->...`, `keyConfig->keys[]` etc | unchanged | MenuSelection, ConfigData, KeyConfig are pointer-free and equal on both layouts. |

Not touched but outside this function's rewrite (done by earlier groups, they show in the diff against B0): `replayDisabled`
-> `replayBuffer->disabled` (ps1 offset 14, `sh 0xe`), `dispOffX0/Y0/X1/Y1` -> `DispEnv0/1.screen.x/y` (ps1 offsets 8, 10).

Uninitialised/UB: the locals `tmp942`, `tmpc1`, `tmpc2` (the first unused) are untouched. `case 10/22` with
`saveSelection != 0` does nothing, as before. `saveSelection` is `short` as in the console.

Oracle result: `-O0` identical to B0 except `replayBuffer`/`DispEnv0/1` addressing (earlier groups), and the 6-byte copy
(byte loads and `lwl/lwr` because `Label6` has alignment 1; B0 had an int-typed alias and used `lw`+`lhu`): same bytes copied.
`-O2`: same plus scheduling.

## DisplayCardBack

This function could not be checked against B0 instruction by instruction: the old text kept precomputed byte offsets in
`s32` locals, which have no real-type equivalent, so the new code (which indexes the arrays) has a different instruction
shape (O0: 2000 -> 2271 listing lines; O2: 649 -> 954, clang reloads `CurrentScreen` after every byte store). Verified by the
differential run against the retail machine code instead.

### Finding: the old text disagrees with the retail code
The imported text declared `s32 *slots = (s32 *)cardSlots` and computed the file names for the first `strncmp` as
`cardOffset + (((cursorTimes2 + cursorSlot) * 0x1E) + slots) + 0xB`. In C, adding an int to an `s32 *` scales by 4, so
that call compared bytes at `cardSlots + 4*(90*cursor + 1350*card) + 44`, not `cardSlots[card].entry[cursor].filename`
(offset 90*cursor + 1350*card + 11). The retail instructions compute `cardSlots + 90*cursor + 1350*card + 0xB`
(no `sll 2`), and the second `strncmp` of the same text (`(char *)cardSlots`) is byte-scaled. The old text compiled by
clang for MIPS does the 4x scaling (visible in B0: `sll $3,$3,2`). Running the old text against the retail code
mismatches in 75 of 100 random trials (first difference: the `strncmp` arguments); the new text matches in all trials.
The new text uses the byte scaling of the retail code. Native consequence: the old text would have read up to 4*(90*14+1350)+44 = 10484 bytes from `cardSlots`, past the
2700-byte array.

### Offset -> field mapping
`polys` is a `MenuPolys *` (5888 bytes; pointer-free, equal on both layouts).

| Old raw expression (bytes) | Real field | layouts.py (ps1, size) |
|---|---|---|
| `polys + slot*0x30 + screen*0x18 (+0x650)` | `polys->background[slot][screen]` | `background` POLY_F4[30][2] at 1616 = 0x650; POLY_F4 24 bytes |
| `polys + slot*0x50 + screen*0x28 (+0xBF0)`, `+ fadeSprite*0x28 + entry*0x50` | `polys->backdrop[slot][screen]`, `polys->backdrop[entry][sprite]` | `backdrop` POLY_FT4[30][2] at 3056 = 0xBF0; POLY_FT4 40 bytes |
| `entry + 0xF` row | `backdrop[entry + 0xF]`: card 1's rows | 15 slots per card; max row 29 < 30 |
| `+0xBF8,0xBFA,0xC00,0xC02,0xC08,0xC0A,0xC10,0xC12` | `x0,y0,x1,y1,x2,y2,x3,y3` | POLY_FT4: x0 8, y0 10, x1 16, y1 18, x2 24, y2 26, x3 32, y3 34 |
| `+0xBFC, 0xBFD, 0xC04, 0xC05, 0xC0C, 0xC0D, 0xC14, 0xC15` | `u0, v0, u1, v1, u2, v2, u3, v3` | u0 12, v0 13, u1 20, v1 21, u2 28, v2 29, u3 36, v3 37 (relative to 0xBF0) |
| `screenOffset40 + base + 0xC05` and `+ 0xBFD` where `screenOffset40` carried an extra `+1` | `tpage` (0xC06 - 0xBF0 = 22), `clut` (0xBFE - 0xBF0 = 14) | exact |
| background `+8..+0x16` | `x0 y0 x1 y1 x2 y2 x3 y3`, `r0 g0 b0` | POLY_F4: r0 4, x0 8 ... y3 22 |
| `selection->selected`, `->timer` | unchanged names | SaveSelection `selected` 164 (short), `timer` 184 (short) |
| `cardSlots + card*0x546 + entry*0x5A` | `cardSlots[card].entry[entry]` | CardSlot 1350, CardEntry 90 (both pointer-free) |
| `+ 0xB` (strncmp operand) | `.filename` | CardEntry `filename` ps1 11, char[32]; compared with n = 0x14 |
| `->type` (`+0x4B`), `->multi` (`+0x4D`), `icon[0..7]` (`+0x4E`), `x` (`+0x56`), `y` (`+0x58`) | same names | CardEntry type 75, multi 77, icon 78 (8 bytes), x 86, y 88 |
| card 1 entries: `cardSlots + 1350 + (slot - 15) * 0x5A` | `cardSlots[1].entry[slot - 0xF]` | |
| `cardCount`/`rowCount`/... cells of `grid[]` | unchanged (see below) | |

The strides of every pointer-free type used (POLY_F4 24, POLY_FT4 40, CardEntry 90, CardSlot 1350) are identical on both
layouts, so no size changed. Index ranges: `slot` 0..29, `entry` 0..14, `cursorSlot` 0..14 (LoadSave clamps `fadescreen`
to 0..14), so `cardSlots[activeCard].entry[cursorSlot]` stays inside the array; `prevCard` is the previous `activeCard`
(0 or 1; it is initially 0).

### Other edits
* `grid[]` (the 12-cell stack array that the old text used for locals `cursorSlot`, `activeCard`, `selection`, ...) stays,
  without the two cells that held `cardTimes2`/`cursorTimes2` (they only fed the removed byte-offset arithmetic:
  `cardTimes2 = 2*activeCard`, `cursorTimes2 = 2*cursorSlot`, never modified). Cell 2 held `selection` as an `s32`; it is
  now a `SaveSelection *volatile` member of the cell union (`save`). `grid[9].padding[0]` (the low byte of the 16-bit
  `fadeValue`, little-endian only) is now `(u8)fadeValue` (same value).
* Removed dead state: `gridExtra` and `slots` (the old text stored `slots` into `gridExtra` only to spill the register that
  the retail code spills across the `strncmp` call; the retail code also passes it in `$a3` as an unread extra argument),
  and all precomputed byte-offset locals (`backgroundRow`, `backdropRow`, `polyBase*`, `polyOffset*`, `cardOffset`,
  `entryOffset`, `cardUnits*`, `slotOffset*`, `primBase`, `clutOffset`, `screenOffset40`, `otOffset`, ...).
* `AddPrim(otEntry, primBase + screenOffset40)` -> `AddPrim(otEntry, iconPoly)`; the last `AddPrim` lost its third argument
  (`CurrentScreen`, never read by `AddPrim`).
* The `(s8)(u16)` casts when copying the card icon's UVs into `u_char` fields are gone (plain narrowing).
* All `goto`s and labels (`unhighlight`, `unhighlightDone`, `setClut`, `drawIcon`, `drawError`, `rowRefresh`, `rowDone`)
  are kept, with the same flow.
* Non-void function that ends without `return`: now `return 0;` with a comment. The only caller (LoadSave) ignores it.

### Uninitialised reads / UB kept
None of the removed locals was read before being written. The `volatile` stack accesses of `grid[]` remain (no effect
natively). `fadeValue = cardFlash % 255` can be negative after `cardFlash` wraps (as on the console; `cardFlash` grows by
20 per call, so only after about 100 million calls).

## Differential results

(N = random trials; `new` = the working-tree menus.c compiled for MIPS o32; "retail" = the game executable's code.)

| function | clang -O2 | clang -O0 |
|---|---|---|
| DisplayCardBack | 0 of 1500 trials differ from retail | 0 of 400 |
| CopyGameData | 0 of 2000 | 0 of 500 |
| MenuControl | 0 of 400 | 0 of 160 |

Coverage notes: DisplayCardBack covers both cards, highlighted and non-highlighted slots, all `type` bit combinations
(icon from `cardIconInfo`, memory-card icon, error text), `selected`/`timer`/`card1Exists`/`cardPresent1` on and off, and
equal and different file names. CopyGameData covers every `currMenu` case (1, 2, 4-10, 12, 15, 16, 21-23, others) with
both values of `saveSelection`. MenuControl covers the attract-mode exit (return value 1 in about 10% of the trials), the
error-message path, linked mode, `CopyGameData` calls on screen changes and the 900-frame idle counter; the callees
(DAlloc, GetMenuKey, Swap, doSerialMenu, all drawing and init routines) are stubs, their arguments are compared, and the
stack is zero-filled so that uninitialised locals read the same value on both sides. The only normalised difference is the
scaled `RESERVE_HEAP_SIZE` argument of `InitDynamicMem` (x2 natively). The `Print` stub ignores the unread `size` argument
(`Print` never reads it).

Mutation checks (one change in the new text, trials that disagree with retail):
* DisplayCardBack, 300 trials: `slotNow - 0xF` -> `0xE` 114; `entryIndex + 0xF` -> `0xE` 111; `prevCard` -> `activeCard` in the
  second `strncmp` 122; `r0 = 0x27` -> `0x28` 236; `multi == 1` -> `== 0` 127; clut shift `0x14` -> `0x13` 145; `icon[3]` -> `icon[2]` 120;
  `% 255` -> `% 254` 222; `b0 = 0x80` -> `0x81` 64; `iconX += 19` -> `20` 300.
* CopyGameData, 400-600 trials: `cdFlag[0] = 2` 22; track selection `menu[8]` -> `menu[7]` 3; memcpy of 5 bytes 8;
  `nc < 128` -> `< 127` 5; `SetVolume(.., 1)` 22; `InitMenuDisplay(121, 0)` 13; wrong `highScoreScratch` field 8.
* MenuControl, 400 trials: `absolute` -> `relative` 400; `CdMusicPlay(musicIndex + 1)` 400; `cardNum -= steps * 31` 383; `PrintCX(.., 127)` 16;
  `nextMenu = 21` 123; `cheatMinigun` -> `cheatRain` reset 274; `ownShip = ...ship + 1` 163; `raceType = 98` on the attract exit 21;
  `thrust >= 32` and `steering - 120 >= 46` (the idle-detection boundaries) are caught only in rare trials (1 in 120 and 1 in 300),
  so those two comparisons are weakly tested.

## Unsure / for the coordinator
* `VSync(0)` (above).
* `InitMenuPolys((MenuPrims *)&work.menuPolys)`: the prototype could take `MenuPolys *` (the definition casts it back); left
  because `MenuPrims` is declared in functions.h and the definition is not one of the three functions.
* `grid[]` in DisplayCardBack is still a decompiler artefact (named cells through macros). It is kept because the task says to
  keep statement order and control flow; it could be replaced by plain locals, which would change nothing observable.
* `GetTrackName` still falls off the end (warning, not mine).
