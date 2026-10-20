# Group 5: card, ayrace, hiscores, ghost, init, iso9660, lzss, file, malloc, object, options, pad

State at the end: `tools/diag.py` prints nothing for card, ayrace, hiscores, ghost, init, iso9660, lzss,
file, malloc, pad. Remaining diagnostics are only inside the two frozen functions:

* `DrawEnvMap` (object.c:367-): 15 pointer/int-cast warnings, lines 412, 418, 420-423, 427, 428, 515-521
  (`u32 primitiveAddress`, `volatile u32 nextPrimitive` in `EnvMapWork`: the primitive cursor is a 32-bit
  integer; natively it truncates the pointer, so the function cannot run until the coordinator's rewrite).
* `InGameReset` (options.c:62-): three **errors**, lines 190, 213, 240 (`PrintCX(..., 8)`: functions.h now
  declares `void PrintCX(u_char *text, int x, int y)`, set by another group, so the fourth argument must go).
  Because of these errors options.c does not compile at all until InGameReset is rewritten; oracle32 cannot
  compile it either (see "Verification").

Oracle: B0 is the pre-coordinator snapshot, so ayrace.c differs from B0 by the coordinator's own RaceState ->
ConfigData edit. I took a baseline of HEAD (plus the missing `#include "hiscores.h"`) to isolate my edits; the
result is in "Verification". Function changes caused only by the shifted `jal` targets of earlier functions
are not listed (they are the same code).

## Edits by file

### malloc.c
* **Alignment (rule 4/10)** `InitDynamicMem`: usable size rounded down to a multiple of 8.
  `DAlloc`: `size += 4; size = (size | 3) ^ 3;` (round down to a multiple of 4 on the console) became
  `size += 4; size = (size + 7) & ~7;` The block header is 32 bytes natively (20 on the console), blocks hold
  64-bit pointers (`ShipData`, `Object`, ...) so every block address (and the pointer handed out) must be a
  multiple of 8: header 32 + sizes multiple of 8 + heap base multiple of 8 (main.c's `portHeapArea` is a
  `long long` union and `MAIN_HEAP_SIZE`/`RESERVE_HEAP_SIZE` are multiples of 8, so the reserve heap is aligned
  too). Result is never smaller than the console's rounded size, allocation policy otherwise untouched.
* **printf format** `printf(msg)` -> `printf("%s", msg)` (two sites). The strings are plain texts without `%`.
* Removed the local `extern HeapBlock *mainheapcopy` (globals.h has it).
* Oracle: DAlloc, DFree CHANGED (rounding; `%s` argument).

### pad.c
* Removed the local PsyQ prototypes `InitPAD/StartPAD/ChangeClearPAD` (psyq.h has them; `StartPAD` returns int
  there) and the local externs now in globals.h (`gameType`, `padType`, `keysPressed`, `tempGameData`,
  `CameraUpdate`). `padType` was `short` here and `unsigned short` elsewhere: now `u_short`; every use
  compares with 0/1/2 or stores 0/1/2, sign-insensitive.
* `padBuf1`/`padBuf2` are `struct PadPacket padBuf1[]` (the type main.c and menus.c use; `InitPAD` is
  given the 8-byte buffers of one packet each); they were `u_char[8]`.
* `buildGameKeyPad` definition spelled `struct PadPacket *` as its prototype.
* Oracle: InitController CHANGED (padBuf1/padBuf2 are no longer `$gp` relative: the extern array has no size).

### iso9660.c
* Removed the local re-declarations of `OpenEvent/EnableEvent/CloseEvent/WaitEvent/CdGetToc2/CdControlF/
  CdRead/CdReadSync` (psyq.h declares them). Call sites cast to the psyq.h parameter types:
  `CdGetToc2(1, (CdlLOC *)toc_loc)`, `CdControlF(0x15, (u_char *)&loc)` (5 sites), `CdRead(..., (u_long *)buf, ...)`.
* `my_error_handler` returns `int` (OpenEvent's callback type is `int (*)(void)`); never returns anyway
  (`exit(-1)`). functions.h prototype changed accordingly.
* `iso_init`: the sector buffer is a union `{ u_char bytes[2048]; u_int words[512]; }` so it is word aligned
  (it is read as words at 0x84/0x8C and handed to `CdRead`).
* `NextPath`: `(int)next & -(cond)` -> `(uintptr_t)next & -(uintptr_t)(cond)` (same branch-free form).
* `strlen`/pointer difference results cast to `int` (two sites).
* Oracle: no change at all.
* Rule 9: PVD fields (path table size/location at 0x84/0x8C) are read as `int` from a word-aligned buffer;
  directory records and path-table records are walked with byte offsets and `u_short` reads at even offsets
  (record lengths are even), not as structs. `IsoFile`/`IsoFileInfo` are pointer-free (16/8 bytes both layouts).

### init.c
* **`TIMlist.memOffset` (task item)**: it held *offsets* that `LoadCompressedTextureSequence` and
  `WADLoadCompressedTextureSequence` turned into addresses with `+= (int)memBase`. I did not make it a pointer
  array: the offsets stay offsets (the two loops that added `memBase` are gone) and `LoadCmpFiles` /
  `GhostLoadCmpFiles` form the address with `(Tim *)(timPtr->memBase + timPtr->memOffset[i])`. Nothing else
  reads `memOffset` (grep over src/). TIMlist layout is unchanged; a comment in wipeout.h states the meaning.
  Oracle: the two loaders lose the loop; the two `*LoadCmpFiles` gain an add; InitScene/WADInitScene (inlined
  copies) show the same.
* **Rule 4/overrun** `getPath`: `char trackNumText[2]` -> `[4]`. `fmtTrackNumPadded` is `"%02d"` and
  `fmtTrackNum` `"%2d"`: both write 3 bytes (2 digits + NUL) into a 2-byte buffer, even for the console
  (it overran by one into the frame padding). A `u_char` track number needs up to 3 digits + NUL.
* Removed local externs now in globals.h (`TextureTableCount`, `heap`, `reserveHeap`) and the
  `gClutX/gClutY/gTextureX/gTextureY` externs: they are now the shared `short` (tim.c/globals.h) instead of
  `unsigned short`; values stay below 512 and the code casts to `short` before comparing, so identical.
* `NewLoadTexture` definition spelled `struct Tim *` as its prototype.
* Rule 9: `Tim`, `TimClut4`, `TimClut8` overlay the TIM file bytes; all pointer-free, same size both layouts
  (8, 64, 544). `Texture` (38 bytes) also pointer-free. `*(int *)&tc4->textureX` reads two shorts as one word
  at a 4-aligned offset (TIMs are placed at 4-aligned offsets by the sequence sizes).

### lzss.c
No edit. Audit: pointer free; sizes are `short` window indexes; reads `window[]` before it is written (rule 10,
below).

### file.c
No edit. `LoadFile` reads whole sectors into the caller's buffer and copies the last partial sector as
`u_int` words, rounded up to 4 bytes: it writes up to 3 bytes past `length` (as on the console) and needs a
4-aligned buffer (DAlloc blocks are 8-aligned now).

### card.c
* DIRENTRY (40 bytes on the console, 48 natively because of the `next` pointer): `LoadFirstFile` used a
  `char record[40]` cast to `(DIRENTRY *)` and reused `slot` as a `char *` to the record (`slot = (int)returned`).
  Now a real `DIRENTRY record`, `firstfile(path, &record)`, `record.name`. (`LoadCardFiles` already used a
  `DIRENTRY` local.) **Platform:** `firstfile`/`nextfile` must fill the host `DIRENTRY` of card.h
  (name[20], attr, size, next, then 8 bytes).
* `cardIconInfo` is a `Texture *` (what `LoadTexture` returns). `CardImage` (a second private view of
  `Texture`: `image` = clutX..clutH, `cx..ch` = textureX/Y/W/H) is gone from card.h and `GetHeader` reads the
  real fields. Declared in globals.h; defined in card.c. Oracle: `lhu` of the old `cx/cy` vs `lh`, same value
  (assigned to `short`).
* `SLOT_ENTRIES(cards, slot)` (`slot * sizeof(CardSlot) + (char *)cards`) and the `(int)cards` entry
  pointer in `LoadCardFiles` -> `cards[slot].entry[...]` / `&cards[slot].entry[i]` (CardSlot is pointer-free,
  1350 bytes both layouts).
* `CardHeader` (card.h, new): the 512-byte file header as a union with `u_long align[128]`, used for the
  header buffers of `SaveCardFile`, `RenameFile1`, `RenameFile`: `GetHeader` hands word pointers at +96 and
  +128 to `StoreImage`, a `u_char[512]` local is not guaranteed to be word aligned natively.
* `LoadCardData`: `u_char data[0x100]` -> `u_short data[0x80]` (it is read as `u_short`s at even offsets).
* Removed local prototypes of `OpenEvent/EnableEvent/CloseEvent/_card_info` (psyq.h) and the externs now in
  globals.h (`hiScoreTable`, `heap`, `mainheapcopy`, `reserveHeap`, `cdMusicTable`, `savedKeyConfig`).
* Oracle: FirstSavedGame (inlines LoadFirstFile), LoadCardFiles, LoadFirstFile CHANGED (the above:
  `record` is a struct, the entry address is computed as `cards + slot*1350 + i*90` in a different order).
  LoadCardData/LoadCardInitData/WriteFile are the same code.
* Rule 9 (memory card files): `WriteFile` writes (via `PsyqWrite`) the 512-byte header (bytes), `hiScoreTable`
  (0x600 = `HiScore[16][4]`, 24-byte pointer-free records), and a 256-byte block built field by field.
  `LoadCardData`/`LoadCardInitData` read the same three parts (`hiScoreTable` straight into the struct array).
  Everything pointer-free, same layout natively; the card file format is identical to the console's.

### hiscores.c
* `ship = (ShipData *)(shipNum * 240 + (int)ships)` -> `&ships[shipNum]`; same for `own`; the `+196` byte
  offset read -> `own->lapTimes[own->lap - 2]` (lapTimes is at 204 on the console); `(HiScore *)((char *)
  hiScoreTable + ...)->lapShip = 15` -> `hiScoreTable[track][cls].lapShip = 15`.
* Removed externs now in globals.h (`classLaps`, `raceCompleteFlag` (now `u_char`, was `s8`: only set to 1
  and compared with 0), `hiScoreTable`).
* Oracle: CheckForHiScore CHANGED (operand order of two commutative `addu`).

### ghost.c / ghost.h
* **`GhostReplay` is a real struct** (it was `header[0x104]` + keys, with `record*`/`replay*` as separate
  globals): `attractSeed, keyCount, keyTotal, control, disabled, magic, trackNum, shipClass, ShipData ship,
  keys[GHOST_KEYS]`. This is exactly what the console had: the "separate globals" `recordKeyCount ... recordShipClass`
  were the 16 header bytes at +4..+19 (sizes 4,4,2,2,2,1,1 fill them; `storeGhostTrail` copies the whole
  `replayBuffer[0] = recordBuffer[0]`, which only works if they are part of the buffer; natively separate
  globals would not be copied and the ghost would never replay). The oracle confirms the offsets (`+4`
  keyCount, `+8` keyTotal, `+0xc` control, `+0xe` disabled, `0x104` keys). On the console `sizeof` is 25460,
  natively 25480 (the ship needs 8-byte alignment: offset 24).
* The copies of the player's ship use `memcpy(..., sizeof(ShipData))` instead of the hard-coded
  0x14/0xF0/0x104/60-word copies; the `combatAttr` access at `+0xAC` is the field. `GHOST_SHIP_AT`,
  `GHOST_KEYS_AT`, `GHOST_SEED`, `GHOST_SHIP`, `GHOST_KEY` are gone (indexing `take->keys[n]`).
* `storeGhostTrail(ConfigData *config, HiScoreData *score)` reads `score->flag & 2` (was `config->savedTrackNum & 2`
  on the second argument). functions.h prototype changed.
* `volatile` on `recordKeyCount` went with the globals.
* **Cross-file consequence (for the coordinator)**: other files declare `record/replayDisabled` as separate
  `short` globals; they must use the fields:
  `dynam.c:8` (`extern short recordDisabled`) and `:141` -> `recordBuffer->disabled = 1`;
  `menus.c:1255` (`extern short replayDisabled`) and `:1408`, `:1426` -> `replayBuffer->disabled = 1`
  (`#include "ghost.h"`). ayrace.c (mine) is done. The data generator's aliases for `record*/replay*` and its
  `recordBuffer[1] __attribute__((aligned(4)))` (the type now has alignment 8) must go.
* Oracle: all four functions CHANGED (struct fields instead of globals; the key halfwords are at `0x104`
  as before).

### ayrace.c
* `#include "hiscores.h"`, `"options.h"`, `"ghost.h"`.
* `InGameReset(pause, config, box)` -> `InGameReset((PauseState *)pause, config, (OptionsPolys *)box)`:
  InGameReset's private views (PauseState = PauseMenu field for field, OptionsPolys = the polygon block that
  `struct SelectBox` is the start of); both pointer-free. Kept as a cast with a comment (InGameReset is frozen).
* `storeGhostTrail(gameData, scoreData)` now matches the signature. `recordDisabled = 1` ->
  `recordBuffer->disabled = 1`.
* `played:` label before `}` -> `played:;` (C23 extension warning).
* Pause functions' definitions spelled `struct PauseMenu *` as their prototypes.
* `continuesTexture` is `Texture *` (was `void *`; defined as `Texture *` in racectrl.c, passed to
  `DisplayContinues(Texture *, ...)`). Removed locals now in globals.h (`perfectLap`, `raceCompleteFlag`,
  `gameType`, `linkSendFlag`, `keysPressed`, `padType`, `finishPosition`, `continuesLeft`, `cdMusicTable`,
  `CameraUpdate`, `cameraPtr`, `shipTargetIndex`).
* Oracle (against HEAD baseline): only CheckGamePause (`recordBuffer->disabled`). Against B0, CheckShipDestroyed
  and RaceComplete also differ: those are the coordinator's RaceState/ScoreData -> ConfigData/HiScoreData edit.

### object.c
* `nextPrim` was defined here as `char *volatile nextPrim`; the shared declaration (display.c, weapon.c,
  globals.h) is `PrimPtr nextPrim`. object.c now defines `PrimPtr nextPrim;` and the local `extern int *OT2[]`
  and the unprototyped `RotTransPersN/NormalClip/AverageZ3/AverageZ4/OuterProduct12` declarations are removed
  (psyq.h has them).
* **Bridge for DrawEnvMap (please delete with its rewrite):** DrawEnvMap's text uses `nextPrim` as a bare
  pointer. I put `#define nextPrim nextPrim.ptr` before it and `#undef nextPrim` after it, with a comment,
  so the text is unchanged and the file compiles. This is the one `#define` alias I left; the rewrite should use
  `nextPrim.ptr` and drop both lines. The volatile is gone (compiled code of DrawEnvMap changed accordingly).
* The other functions are unchanged (no diagnostics, no oracle change).
* Also belonging to DrawEnvMap but outside it: `extern int StaticRVertY[][4]; StaticRVertZ[][4]` (lines ~325)
  are the decompiler's names for the `vy`/`vz` of `StaticRVert[]` (stride 16); the data generator aliases them.
  Natively separate arrays would break DrawEnvMap; the rewrite should index `StaticRVert[i].vy/.vz`.
  `StaticScreen[140]`/`StaticDepth[148]` are overrun by objects with more than 140 vertices (as on the console).

### options.c
* Only the externs now in globals.h removed (`gameType`, `padType`, `OT`, `OT2` (were `u_long *OT[]` / `int *OT2[]`),
  `cdMusicTable`). The rest is unchanged. Everything except InGameReset compiles to the same code as before
  (checked with a temporary copy with the `PrintCX` argument dropped; InGameReset differs only by the three
  `li $7, 8` of the dropped argument). InGameReset diagnostics: see top.
* `PrintCX` in functions.h (not mine) is `void PrintCX(u_char *text, int x, int y)`, the function in menus.c
  takes three arguments; InGameReset passes `8` as a fourth argument it never reads. Drop it when rewriting.

## globals.h (group 5 section)
`cardIconInfo`, `continuesTexture`, `keysPressed`, `padType`, `playerControl` (`ControlInput[]`),
`raceCompleteFlag`, `savedKeyConfig` (`struct CardKeys`), `scenePath`, `shipClassIndex` (all with the defining
file's type, per the data definitions). `padBuf1` cannot go there (`PadPacket` is incomplete at that point).
Conflicting local declarations that must be deleted by their owners (they are now hard redeclaration errors):
racectrl.c:297 `shipClassIndex` (`short`), :325 `keysPressed`, :363 `playerControl`, :369 `scenePath`
(all `u_char_eq1[]`/`short`).

## Items for rules 7-10

Rule 7: none (no `return;` in a non-void function in these files).

Rule 8: none in my files. `PsxScratchpad` use in DrawEnvMap (coordinator's), `RESERVE/MAIN_HEAP_SIZE` unchanged.

Rule 9 (file data read/written as structs; all pointer-free, nothing to fix):
* card.c `WriteFile` (header 512 bytes, `hiScoreTable` 0x600, block 0x100 built bytewise), `LoadCardData`,
  `LoadCardInitData` (same parts; `hiScoreTable` read straight into `HiScore[16][4]`).
* iso9660.c `iso_init` (PVD as words at 0x84/0x8C), `search`/`find_file` (directory records by byte offsets),
  `iso_get_file_info` (path table by byte offsets).
* init.c `NewLoadTexture` (Tim/TimClut4/TimClut8 over the file image), `LoadCompressedTextureSequence`
  and `WADLoadCompressedTextureSequence` (int table at the start of the buffer).
* file.c `LoadFile` (raw sectors), lzss.c (raw bytes).
* ghost.c: the take (`GhostReplay`) holds a copy of a `ShipData` **with native pointers**
  (`nearTrkSect`, `prevShipSect`, `update`). It is not saved to a file, only copied from `recordBuffer` to
  `replayBuffer` and back over `ships[ghostShip]` in the next race; the pointers are valid as long as the
  track sections are at the same heap addresses in the next race, as on the console.
  progress.c:89 reuses `recordBuffer` as the WAD cache (its size is now 25480 bytes, 20 more than the console).
* (Not file data) `DIRENTRY` in card.c: now a typed local; the platform's `firstfile` must fill the host layout.

Rule 10 (faithful behaviour, not changed):
* lzss.c `ExpandData`: `window[]` (8192 bytes on the stack) is never initialised; a back reference to a slot
  not yet written reads stack garbage (as on the console; fine for data from this encoder).
* card.c `GetHeader`: only the first 256 bytes of the 512-byte header are cleared; bytes 256-511 are
  uninitialised stack and are written to the card file by `WriteFile` (via `SaveCardFile`, `RenameFile1`,
  `RenameFile`). `QuickUnFormat` writes a `sector[128]` of which only the first two bytes are set.
* card.c `FirstSavedGame`: `if (slot == 0) _card_clear(0); else _card_clear(16);` tests `slot` (initially -1),
  not `i`, so port 1's clear is always used (bug kept).
* card.c:593 and options.c:168,183 (InGameReset): `% (cdMusicCount - 1)`: a division by zero if the disc
  reports one CD track; on x86 it traps, on MIPS it did not (arm64 does not either).
* ayrace.c `RaceComplete`: `race` is uninitialised if `venomComplete` and `rapierComplete` are both non-zero
  but neither equals 1 (`crapArray[race]`).
* hiscores.c `CheckForHiScore`: `ship->lapTimes[lapNum - 2]` with `lapNum == 1` would read the 4 bytes before
  `lapTimes` (nearSecNo/prevSecNo in both layouts).
* iso9660.c `iso_get_file_info`: `dirName[8][10]` and `tmpString[13]` overflow for paths with more than 8
  directory levels or an over-long last component (the game only uses fixed paths); `match_file` reads
  `testname[test_len - 2]`.
* iso9660.c `my_error_handler`: `exit(-1)` ends the process on a CD error (the platform may prefer to handle it).
* card.c `CheckCardEvents`/`CheckHWCardEvents`: spin forever on `TestEvent` (the platform must deliver
  the card events or these hang).
* object.c `PreCalcNormals`: `VECTOR faceNormals[256]` is overrun by objects with more than 256 faces.
* No `float`/`double`, no shifts by >= 32, no unaligned accesses left in these files.

## Raw offsets / sizes found and not fixed
* card.c `((u_char *)settings + i)[0x45]` (3 sites: WriteFile, LoadCardData, LoadCardInitData): the 15 bytes
  `generated.digits[13]`, `generatedCheck`, `pad53` of `ConfigData`. ConfigData is pointer-free (100 bytes in
  both layouts), so the offset is right natively.
* pad.c `*(u_short *)control->pad6` (ControlInput offset 6, 2-aligned).
* options.h `PauseState` and `OptionsPolys` (`pad0[232]`): pointer-free private views of the pause menu and its
  polygon block (field offsets of PauseState equal PauseMenu's); only InGameReset uses them.
* object.c `SetupLights`: `char scratch[256]` unused local kept from the console frame.
* iso9660.c keeps unused LZSS `#define`s and a stray `void exit(int)` (copy artefacts); lzss.c keeps `void exit(int)`.

## Verification
* `tools/diag.py`: clean for 10 files, see the top for object.c/options.c.
* `tools/xdecl.py`: no remaining VAR/FUNC conflict that is in my files except spelling-only
  `InGameReset` (frozen), `padBuf1` (`struct PadPacket` vs `PadPacket`, owners: menus.c/main.c) and
  nothing else.
* `tools/oracle32.py diff B0` for my files lists exactly: ayrace (CheckGamePause, CheckShipDestroyed, RaceComplete:
  the last two are the coordinator's edit), card (FirstSavedGame, LoadCardFiles, LoadFirstFile), ghost (4),
  hiscores (CheckForHiScore), init (GhostLoadCmpFiles, LoadCmpFiles, LoadCompressedTextureSequence,
  WADLoadCompressedTextureSequence, InitScene, WADInitScene, getPath), malloc (DAlloc, DFree),
  object (DrawEnvMap), pad (InitController), and options does not compile (see top). Everything else, and
  iso9660/lzss/file, are identical.

## Unsure
* The `#define nextPrim` bridge in object.c (alternatives: compile errors in DrawEnvMap).
* `GhostReplay` as real fields: it breaks the data generator's alias plan and the three external uses listed.
* `CdRead` in psyq.h takes `u_long *`; the I/O worker's libcd.c takes `void *`. My casts work with either.
* DAlloc's slightly larger blocks vs the console's (rounding to 8 instead of 4): the 2x heap scale covers it,
  but free-memory figures that the game prints or tests (`head->free`, `lowTideMark`) differ by a few bytes.
