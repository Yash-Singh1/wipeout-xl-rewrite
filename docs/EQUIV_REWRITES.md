# The 13 functions that were only equivalent to the retail code

Thirteen functions of the decompilation had C that compiled to the retail result only through private
copies of the game's structures (`GameData`, `Skeleton_eq1`, `PauseState`, `OptionsPolys`...) and through
`*(int *)(p + 0x44)` style offsets or `__asm__` aliases that named other files' globals under other types.
Each has been rewritten with the shared types and real field names, statement order and arithmetic unchanged.

| Function | File | Detail (offset -> field tables, uninitialised reads, oracle residue) |
|---|---|---|
| `race` | `racectrl.c` | `equiv/racectrl.md` |
| `MenuControl`, `CopyGameData`, `DisplayCardBack` | `menus.c` | `equiv/menus.md` |
| `collideLineToPlane` | `collide.c` | `equiv/collide-drone-combat-sfx.md` |
| `UpdateDroneShip` | `drone.c` | `equiv/collide-drone-combat-sfx.md` |
| `DrawLinkSign`, `doSerialGame` | `combat.c` | `equiv/collide-drone-combat-sfx.md` |
| `SfxChainId` | `sfx.c` | `equiv/collide-drone-combat-sfx.md` |
| `GetNewWeapon` | `weapon.c` | `equiv/weapon-options-trails-object.md` |
| `InGameReset` | `options.c` | `equiv/weapon-options-trails-object.md` |
| `DrawFlare` | `trails.c` | `equiv/weapon-options-trails-object.md` |
| `DrawEnvMap` | `object.c` | `equiv/weapon-options-trails-object.md` |

## Evidence

1. **Offsets.** Every private field that became a real field is listed with its 32-bit (PlayStation) offset in the
   detail file, taken from `python3 tools/layouts.py show TYPE` (clang's layout of the type for MIPS o32). A field is
   only replaced when both offsets and the field's width agree; where the old text read a field with another
   signedness the file says so and why the values cannot differ.
2. **MIPS oracle.** `tools/oracle32.py` compiles the old text and the new text for MIPS o32 with the same flags
   (`ORACLE_OPT=-O0 ORACLE_NOSP=1` turns off the optimisations and the stack protector so that only the C's own
   structure shows) and compares the two assembly listings, ignoring only stack slot numbers, branch labels and
   relocation addresses. Every remaining difference is listed and explained in the detail file ("Not literal",
   "Oracle residue").
3. **Retail code, run.** For `MenuControl`, `CopyGameData` and `DisplayCardBack` the new text was also run against the
   retail machine code (the executable's own instructions in an emulator, same inputs) on random states: the
   results and the memory they write are equal. This found one real difference in the old text of
   `DisplayCardBack` (below).
4. The whole tree compiles natively without cast warnings for the replaced expressions, and the boot under
   AddressSanitizer/UBSan runs the menus, the race loader and the attract-mode races (`docs/PORT_RULES.md`).

## What the rewrites found

* `DisplayCardBack`: the old text added an int to an `s32 *`, so the first `strncmp` compared bytes at four times the
  intended offset (up to 10484 bytes into a 2700-byte array). The retail code scales by bytes. The new text does, and
  matches the retail code on every random trial (the old text failed 75 of 100).
* `race` read `raceLines[-1]`, `shipIndex[remoteShip]` with `remoteShip == -1` and a null `track` on the console as
  well (all kept, the null `track` and `raceLines[-1]` read are guarded because natively they fault; see the
  surprises in the final report).
* The `linkPad*`, `remotePad*`, `dispOff*`, `smoke*` and `checkCells` names the old text used were pieces of one
  object each (`playerControl[0]`, `remoteControl[0]`, `DispEnv0/1.screen`, one `CVECTOR`, a table of 6-byte
  cells): the data generator knows the real objects (`docs/GENDATA_DECISIONS.md`).
* Division by zero in `cdMusicIndex = (cdMusicIndex + 1) % (cdMusicCount - 1)` (two sites): faults natively on x86;
  not guarded (the console's MIPS code would not trap).
