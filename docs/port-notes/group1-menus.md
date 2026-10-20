# Group 1: game/src/menus.c

State: `python3 tools/diag.py game/src/menus.c` prints only diagnostics located inside the three
untouchable functions (listed below). Nothing outside them is reported. `tools/xdecl.py` no longer
lists any conflict that menus.c causes (remaining ones involving a name menus.c shares are other files'
declarations, listed under "Other files must change").

Files touched: `game/src/menus.c`, `game/include/globals.h` (group 1 section), `game/include/functions.h`
(six menus.c prototypes), `game/include/menus.h` (removed the `CardFlags` typedef).

## 1. Things the coordinator must do (MenuControl / CopyGameData / DisplayCardBack are unchanged)

menus.c cannot be compiled by `tools/oracle32.py` or the native build until `MenuControl` and
`DisplayCardBack` are rewritten: they contain hard errors.

MenuControl (error lines in the current file are 544, 595-597, 614, 652, 654, 667, 669, 693, 793):
- `PrintCX(msg, x, y, 8)` x3: PrintCX now takes 3 arguments (functions.h); drop the `, 8`.
- `VSync();` (in the final `for` loop): VSync needs an argument. The console passed whatever was in $a0.
- `savedKeyConfig` is used as `u8[]` (`(KeyConfig *)savedKeyConfig`, passed to CopyGameData and
  MenuFunction); globals.h (group 5) declares it `struct CardKeys savedKeyConfig`. Use `&savedKeyConfig`.
  I deleted menus.c's own `extern u8 savedKeyConfig[]` because it conflicts with globals.h. `CardKeys` and
  `KeyConfig` are both 62 bytes (checked with layouts.py).
- `MenuFunction(..., (int)&work.musicScreen)`: the last parameter of MenuFunction is now `void *musicScreen`
  (functions.h and the definition). Pass `&work.musicScreen`.
- `work.musicTexture` is an `int` that stores a `Texture *` (`(int)textureTable[menuTextures]`,
  `(Texture *)work.musicTexture`, `DrawDrBox(work.musicTexture, ...)`); make the field `Texture *`.
  `MenuWorkspace` is at the top of menus.c and only MenuControl uses it.
- `CopyGameData(gameData, work.selection, ...)` passes `char[192]`/`char[72]` buffers for
  `MenuSelection *` / `char *` (`work.selection` is a `MenuSelection`; `work.cardState` a `HiScoreData`
  of 72 bytes); `MenuFunction`'s `selection`/`cardState` stay `void *` for that reason.
- Uses the file-local `__asm__` aliases `raceTrack` (raceTracks) and `padBufs` (padBuf1). Replace by
  `raceTracks[0]` and `padBuf1`, then delete the two aliases above `InitMenuBox` in menus.c. (On a Mach-O
  host an `__asm__("name")` label lacks the `_` prefix, so these aliases do not alias.)
- Raw offsets into pointer-bearing structs: `(MATRIX *)((char *)(*setupObject)->skeleton + 32)`. Offset 32
  of `Skeleton` is `absolute.m` on both layouts (the first 64 bytes of Skeleton do not move), so it is correct.
- `InitDynamicMem(reserveHeap, RESERVE_HEAP_SIZE)`, `DAlloc(reserveHeap, 0x6A70)` (24 `MenuEntry` of 1134
  bytes + 32) and `DAlloc(reserveHeap, 0x10760)` (24 `MenuLayout` of 2808 bytes + 32): the sizes are of
  pointer-free structs, correct natively.
- `quadIndex`, the `Swap`/`ClearOTagR(OT2...)` casts now compile because OT/OT2 are in globals.h as
  `u_long *[]`; the remaining casts `(u_long *)OT[...]` are identity casts.

CopyGameData: no diagnostic. Uses two `__asm__` aliases defined between functions in menus.c:
`int cdFlagWord __asm__("cdFlag")` (written `cdFlagWord = 1`, i.e. `cdFlag[0] = 1`) and
`int defaultCardNameWord __asm__("defaultCardName")` (6-byte copy of the "EMPTY" label). Replace by
`cdFlag[0]` and `defaultCardName`, then delete both. They *define* symbols called `cdFlag`/`defaultCardName`
on ELF hosts, so they can clash with the data definitions at link time.

DisplayCardBack (92 diagnostics; two errors, lines 1891 and 1902):
- raw `(s32)polys + ...` arithmetic everywhere (`pointer-to-int`/`int-to-pointer`), `selection = (s32)selectionArg`,
  `slots = (s32 *)cardSlots` and `strncmp(... slots ...)` on integers; rewrite with `cardSlots[card].entry[i]`
  fields and `polys->background[row][screen]` / `polys->backdrop[row][screen]`.
- line 1891 `AddPrim(otEntry, primBase + screenOffset40)` passes ints; line 1902 passes a third argument
  (`CurrentScreen`) that AddPrim never reads.
- strides used: 0x5A (CardEntry, 90 bytes), 0x546 (CardSlot, 1350), 0x18/0x28/0x30/0x50 (POLY_F4/FT4 pairs):
  all pointer-free types, so the arithmetic is correct once done on pointers.
- falls off the end of a non-void function (`int DisplayCardBack`); LoadSave ignores the result.
- depends on the type of `cardIconInfo` (now `Texture *`, globals.h group 5; I deleted menus.c's duplicate
  definition `Texture *cardIconInfo;` that sat above the function; card.c defines it).

## 2. Edits by category (outside the three functions)

Rule 1 (pointer/integer casts):
- DoButton: the `(int)polys` / `(int)menuButtons` arithmetic is now `polys[buttonCount][CurrentScreen]` and
  `menuButtons[glyphNo][frame]` (plain 2-D array indexes).
- EnterFileName, ShipParameters, DrawHighLight, PrintCX: byte offsets into polygons are added to
  `(uintptr_t)` instead of `(int)`. I tried `(char *)p + offset` and `&p[n]` first; the oracle then showed
  extra instructions (e.g. EnterFileName 366 -> 420 because CurrentScreen is reloaded after each store), so I
  kept the exact integer expression with a pointer-sized type: byte-identical on the 32-bit oracle, correct natively.
  All the types involved are pointer-free (POLY_F4, POLY_G4, POLY_FT4).
- InitCard: see "split arrays" below.

Rule 2 (integer variables holding pointers):
- `heap`, `reserveHeap`, `mainheapcopy` -> `HeapBlock *`; `residentSkeletons`/`upperSkeleton` -> `Skeleton`,
  `shipShapeTable` -> `Object *[]`, `teamShip` -> `Object *`, `textureWad` -> `WADFILE *`, `OT`/`OT2` -> `u_long *[]`
  (all through globals.h). `DrawTeamShips` compared `obj == teamShip` against an `s32`.
- `screenFills` + `screenFill1` were one `BLK_FILL *screenFills[2]` (adjacent words, symbols 0x800949E4/E8):
  `InitResidentMenu` now assigns `screenFills[0]` and `screenFills[1]`. progress.c still writes `screenFill1`
  (its own extern); it must use `screenFills[1]`.
- `fontTexture` was a `FontTexture *` (padded private copy of Texture); it is a `Texture *` now
  (`clut`->`cba`, `tx`->`textureX`, `ty`->`textureY`). The two byte fields `u0`/`v0` were read with `lbu`; the
  Texture fields are `short`, holding values below 256 (tim.c derives them from `x << 2` / `y` inside a tpage), so
  the same values result.
- `loadPolys0/1` are `POLY_FT4[]` (they were `u8[]` passed to `DisplayTexture(..., POLY_FT4 *)`);
  `VertexNormal` is `SVECTOR[]` (menus.c is its only user; it was `s32[]` passed as `SVECTOR *`).
- Removed the unused private typedefs `POLY_FT4__L`, `Password__L`, `POLY_G4__L`.

Rule 3 / layout aliases (split arrays): `cardUsed`, `cardFileNames`, `cardNames` and `cardFlags` were names of
single fields inside the 90-byte `CardEntry` records of `cardSlots` (symbol addresses: cardSlots 0x800E8FF4,
cardUsed +10, cardFileNames +11, cardNames +43, cardFlags +75; same offsets as `used`, `filename`, `label.bytes`,
`type` in layouts.py). They were indexed up to 2700 bytes, which is an overflow natively unless they are the same object.
They are now accessed as `cardSlots[card].entry[slot].used/.filename/.label/.type`
(`InitCard`, `IsTeamCardEmpty`, `CheckCardFull`, `LoadSave`: `SelectedCardFlags()` macro, `Print(...label.bytes...)`).
The `CardFlags` typedef (menus.h) and the four extern arrays are gone; the generated data must not define them.

Rule 4 (sizes): all `DAlloc` sizes and `memcpy` lengths in the file are of pointer-free data (checked:
`MenuLayout`/`MenuEntry`/`MenuFile` sizes are equal on both layouts; every `__builtin_memcpy(label, string, n)`
has `n <= sizeof(string)` against the data). No change needed. `sizeof` is only used for POLY types.

Rule 5 (prototypes):
- removed local re-declarations of `LoadCard`, `LoadCardFiles`, `CardContentStatus`, `ClearEventHW`, `CentreText`,
  `CentreText2`, `AddText`, `format`, `delete`, `buildGameKeyPad`, `PreCalcNormals`, `DrawEnvMap`,
  `SetSkeletonPosition`, `AsmDrawObject`, `newRand`, `_comb_control`, `PrintCX__L`.
- functions.h: `DisplayCardBack` and `EnterFileName` got the definitions' parameters; `EstablishLink(void)`;
  `MenuFunction`'s last parameter is `void *`; `DispHiScores(UnlockData *, void *unused, unsigned char)` (the
  second argument was a pointer passed to an `int` parameter that is never read).
- `PrintCentred(...) __asm__("PrintCX")` is now simply `PrintCX(u_char *text, int x, int y)` (the 3-argument
  prototype that is in functions.h). All 75 calls in menus.c outside MenuControl dropped the unread 4th argument
  (size, 7 or 8). `Print(u_char *, int, int, int)` matches its prototype.
- `EstablishLink()` is called with `(gameData, keyConfig)` by MenuFunction; dropped (comment in code).
- `AsmDrawObject(obj, 0)` in DrawTeamShips has to match the 3-parameter prototype now in functions.h:
  it passes `0` for `roll` (the console left a stale $a2). The routine only reads `roll` for sprite primitives
  (functions.h comment); team-ship models have none, as far as I can tell, but I did not see the assembly.
- `Load16BitTexture` file-not-found path (`return;` after `Error(..., 0)`): returns 0 now (rule 7, below).
- format-security: `printf(msg)`/`sprintf(errorString, msg)` -> `printf("%s", msg)`/`sprintf(errorString, "%s", msg)`
  in `setupBackgrndPolys`, `LoadSaveIcons`, `LoadMenuLineData`, `Load16BitTexture`. I checked the strings in the
  generated data: none contains `%`, so the output is unchanged.
- Casts that remain: `LoadImage(&rect, (u_long *)imageData)` and `DFree(reserveHeap, (char *)timHeader)` (Load16BitTexture).

Rule 6 (extensions): removed `raceTracksSmall`, `gpTeamNameAuricom/Qirex/Feisar/Piranha` (they were definitions with
`__asm__("teamName...")` labels whose only purpose was $gp-relative addressing on the console), `rankName0-2`,
`arcadeLabel`, `linkLabel` (aliases of `hiScoreRank0-2`, `msgArcade`, `msgLink`; `hiScoreRank0-2` carry
`ALIGN2` so the 7-byte copies keep their halfword expansion), `msgArcade`/`msgLink` are `extern` (they were
zero-initialised tentative definitions of string data). Remaining `__asm__` aliases (used only by the unchanged functions):
`raceTrack`, `padBufs`, `cdFlagWord`, `defaultCardNameWord`.

Shared declarations (globals.h group 1 section): `heap`, `mainheapcopy`, `residentSkeletons`, `upperSkeleton`,
`shipShapeTable`, `TextureTableCount` (`unsigned short`), `highResTable` (`short`), `cheatWeapon/Damage/Rain/Time/Minigun`
(`unsigned char`), `continuesLeft`, `gameType`, `sillyShips`, `menuSendFlags` (`unsigned char`), `menuSendKey` (`short`),
`menuTextures` (`short`), `hiScoreTable` (`HiScore[][4]`). I deleted menus.c's local declarations of everything that
globals.h now has (also `OT`, `OT2`, `reserveHeap`, `teamShip`, `tempGameData`, `textureWad`, `playerControl`,
`keysPressed`, `cdMusicTable`, `cardIconInfo`, `screenFills` that other groups added). The sign choices follow the
majority of the other files; menus.c only stores constants into the cheat flags and `continuesLeft`, compares
`gameType`/`menuSendKey`, and uses `TextureTableCount`, `highResTable` as non-negative values.
`trackMedals`, `cdMusicTable`, `keyDefine*` use `u_char`/`u_short` spellings like the other files; `padBuf1` is
`extern struct PadPacket padBuf1[]` (it was a scalar `PadPacket` used as `padBuf1.steering`; now `padBuf1[0].steering`).
`MenuFunction`'s `gameData`, `keyConfig`, `selection`, `cardState` stay `void *` (MenuControl passes buffers).

## 3. Other files must change (declarations that now disagree with globals.h)

- main.c: `sillyShips` (signed char), `TextureTableCount` (short), `menuTextures` (declared `u_short[]` and
  passed to `InitMenuTextures`; should be `&menuTextures`), `gameType` (signed char per xdecl), `continuesLeft` (s8).
- racectrl.c (coordinator): `heap`, `mainheapcopy`, `reserveHeap` are `int` there.
- progress.c: `screenFill1` -> `screenFills[1]`.
- display.c: `menuSendFlags` is `char`; combat.c: `menuSendKey` is `unsigned short`; ships.c: `highResTable` is `u_short[]`;
  sparks.c: `TextureTableCount`. card.c: `hiScoreTable` is declared `u_char[]` (a raw view of `HiScore[16][4]`).
- The generated data (`menus_data.c` in the data worktree) must drop `cardUsed`, `cardFileNames`, `cardNames`,
  `cardFlags` (they are fields of `cardSlots`), `screenFill1`, and make `cardSlots` 2 x `CardSlot` (it is). It also
  defines `VertexNormal` as `s32[280]` (1120 bytes = 140 `SVECTOR`); menus.c declares `SVECTOR VertexNormal[]`.
  `loadPolys0/1` must be `POLY_FT4[2]` (they are `u8[80]`, the same 80 bytes). `msgArcade`/`msgLink` must be defined
  there with their strings (menus.c no longer defines them).
- Pointer data the platform has to supply: `loadModeLabels` (`LabelList`, three `char *`), `keyFunctionNames`
  (`char *[]`), `menuTexturePtrs*`, `endTextures`, `textureTable`, `menuModels`, `shipModels`, `doubleBuffer` (`DB[]`).

## 4. Oracle (tools/oracle32.py diff B0 menus.c)

The tool cannot compile menus.c at the moment (errors inside MenuControl, above). I ran the same comparison
(same flags, same B0 baseline, branch targets and `.rodata` offsets normalised, relocation lines kept) on a copy where only
those MenuControl/DisplayCardBack lines were patched; the three unchanged functions are excluded from the list.
`MenuControl` differs (the heap size macro `RESERVE_HEAP_SIZE`, my patched `VSync(0)`), `DisplayCardBack`
differs only in one register choice for the `CurrentScreen` load (an effect of the OT/OT2 declaration change), `CopyGameData` is identical.

Changed functions and why (17):
- `DoButton`: reorder-only (same instruction multiset; one commutative `addu`, three stores scheduled differently) from the 2-D index form.
- `EnterFileName`, `ShipParameters`, `DrawHighLight`, `ShowMenu`, `MenuFunction`, `DispHiScores`, `ClassSelect`, `AudioConfig`,
  `InitMenus`, all other drawing code: identical (zero change).
- `InitCard` (43 -> 26 instructions), `setInputDevices` (it inlines InitCard), `IsTeamCardEmpty`, `CheckCardFull`, `LoadSave`:
  split arrays replaced by `cardSlots` fields; every `cardFlags` access is `cardSlots + 0x4b` now (checked in the diff: all
  `lbu` immediates +0x4b), `Print(cardNames + n*90)` is `cardSlots[..].label` (+0x2b / +0x571).
- `AddTextBits`, `TeamSelect`: `teamNameAuricom/Qirex/Feisar/Piranha` are addressed with lui/addiu instead of $gp (the removed
  `gpTeamName*` definitions made clang use small data for the whole file).
- `DrawTeamShips`: `AsmDrawObject(obj, 0, 0)` loads the extra argument.
- `InitResidentMenu`: `screenFills[1]` store (base + 4) instead of `screenFill1`; `lh` -> `lhu` of `TextureTableCount`.
  `LoadButtons`: `lh` -> `lhu` of `TextureTableCount` (values are below 800).
- `Print`, `PrintCX`: `fontTexture` fields are halfword loads (`lhu`) instead of byte loads; `PrintCX` also the `uintptr_t` quad address (unchanged code shape).
- `Load16BitTexture` (return 0 and the `"%s"` argument), `LoadMenuLineData`, `LoadSaveIcons`, `setupBackgrndPolys`:
  the extra `"%s"` argument of `printf`/`sprintf`.

## 5. Rules 7-10, file data, things I noticed

Rule 7:
- `Load16BitTexture` returned with `return;` (value left in $v0 by `Error`) when the file is missing. Its caller
  `main.c:210` stores the result in `menuTexture` and passes it to `ShowMenuBackground`, so this is an uninitialised
  read in the original. Only reachable when `rstartxl.tim`/the intro images are missing. I return 0 (a NULL
  dereference natively instead of a stale pointer).
- `GetTrackName` has no `return` after the `switch` (when `draw != 0` or `type` is not in the table). The callers
  that use the value (DispHiScores, ClassSelect, AddTextBits, progress.c:383) always pass `draw == 0` and a type from
  `raceTracks`, which the switch covers. Left as is.
- `DisplayCardBack` (int) never returns a value; unused.

Rule 8: none (no console addresses, no `0x80xxxxxx`, no scratchpad use).

Rule 9 (struct file/memory-card I/O):
- menus.c reads no file into a struct that contains pointers. Reads: `MENU.DAT` into `menuLayout` (24 x `MenuLayout`, 67392
  bytes, pointer-free; contains `LINE_F2` primitives whose 32-bit `tag` words are rewritten by AddPrim),
  TIM images in `Load16BitTexture` (header read through the pointer-free `TimImageBlock`; pixels at +0x14),
  the 512-byte memory-card icon header in `LoadSaveIcons` (palette at +0x60, icon at +0x80). Game data goes to the card
  through card.c (`LoadCardData`/`SaveCardFile` with `ConfigData`, pointer-free, 100 bytes on both layouts).
- `LoadModel` calls `LoadPrm` (object.c), which is where the PRM file becomes pointer-bearing `Object`s;
  `shipModels[8]`/`menuModels[10]` are arrays of `Object *`, filled by the loop in `LoadModel`.

Rule 10 / unaligned or endian-dependent casts I left (all on pointer-free data, aligned, little endian):
- `*(s32 *)&gameData->rapierChallengeDone & 0xFF00FF00` (TeamSelect, InitChallenges, tempGameData): bytes 61 and 63 of
  `ConfigData` = `teamCheatActive || class2Unlocked`. `*(s32 *)&gameData->entered.digits[12] & 0xFFFF0000` (ClassSelect x3):
  `trackCheatActive || phantomTrackCheatActive` (bytes 98/99). Both are 4-aligned.
- `DispHiScores` takes `UnlockData` (a padded view of `ConfigData`; `unlockedTracks` is at 96).
- `*(short *)&ambientVoice1` (AudioConfig x2, CopyGameData): the low half of an `int`, little endian only.
- `*(Str20 *)modelFile = pathCommonDir;` copies 20 bytes into a `char[256]` local through a 4-aligned type.
- `port += ((u32)initialStatus << 5) & 31;` in `setInputDevices` always adds 0 (the original does this).
- Conditionally uninitialised locals that clang reports and the code never reads wrongly: `result` (MakePassword),
  `sumResult` (DecodePassword), `changedA`/`changedB` (EstablishLink, only read when `linkState > 0`).
- `Print`'s glyph for characters outside the tables stays `code - 0x41`, which can be negative; it only indexes UVs.
- No `float`/`double`, no division by a variable, no shift by a variable amount in the file.

Unsure about:
- the `AsmDrawObject` third argument (above);
- the choice of signedness for `TextureTableCount`, `menuSendKey`, `highResTable` in globals.h (majority/defining type; every
  use in menus.c is insensitive to it).
