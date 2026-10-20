# Rules for editing the game sources (native 64-bit port)

`game/` is the PlayStation decompilation's C. The console was ILP32 (MIPS o32, little endian):
`int`, `long` and pointers were 4 bytes, structs holding pointers had 4-byte pointer slots. The
port compiles it natively (64-bit pointers, host struct layouts). Every edit must leave the
program doing exactly what it did on the console, on a 64-bit host, and must be small and
commented `Port:` where it changes a representation.

Never add `#ifdef`s for the old behaviour, never leave `#define` aliases for renamed things,
never add inline assembly, never mention where names came from.

## Tools (run from the repository root)

| Command | What |
|---|---|
| `python3 tools/diag.py game/src/F.c` | every compiler diagnostic of one file, one line each with the source text. The goal is **no output**. |
| `python3 tools/oracle32.py diff B0 F.c` | compile the working tree for MIPS o32 and compare the machine code of each function in F.c with the original (snapshot `B0`). Port edits that are representation-only must show **no change**. Each function listed as CHANGED must be explained (see below). `-v` prints the instruction diff. |
| `python3 tools/xdecl.py` | cross-file declaration check: globals declared with different types in different files, functions with different prototypes, names defined twice. |
| `python3 tools/layouts.py show TYPE...` | the PlayStation layout (`ps1`) and the host layout (`host`) of a struct side by side: field offsets, sizes, which fields move. `audit` lists every type that differs. |

`tools/diag.py` must be clean and `tools/oracle32.py diff B0` must only list functions whose
change you can explain, before you stop. Do not commit (the coordinator commits). Do not run
anything from `orig/` or `pc-xl/`. Never `git stash`. Keep CPU use modest: one tool run at a time.

## What the host compiler rejects or what is wrong natively

1. **Pointer/integer casts** (`-Wpointer-to-int-cast`, `-Wint-to-pointer-cast`, `int-conversion`
   errors). The decompiler wrote address arithmetic as `(int)p + offset`. Rewrite it as pointer
   arithmetic that moves the same number of **bytes**: `(T *)((char *)p + offset)`; where the
   expression is plainly an array index (`n * sizeof(T) + (int)base`) write `&base[n]` / `base + n`.
   If the stride is a literal that equals a **32-bit** struct size (e.g. `* 0xF0` for `ShipData`,
   `* 156` for `TrackSection`), that struct has pointers and is bigger natively: use the array
   index / `sizeof`, never the literal (check with `tools/layouts.py show`). If the stride belongs
   to a pointer-free struct (polygon primitives: `POLY_FT4` is 40 bytes everywhere) the byte
   arithmetic is fine as is.
2. **Integer variables/fields/globals that hold pointers** (`int heap; int weaponGrids;` assigned
   from `DAlloc`, `u_long`/`s32` locals or struct fields storing addresses). Give them the real
   pointer type. If the same global is declared with different types in several files, see
   "Shared declarations".
3. **Raw byte offsets into structs that contain pointers** (`*(short *)((char *)ship + 0xAC)`, padded
   private copies of structs like `unsigned char pad0[4]; ...`). On a 64-bit host the offset is
   different. Replace by the real field, found by looking the 32-bit offset up with
   `tools/layouts.py show` (the `ps1` column). Raw offsets into pointer-free structs are correct
   natively; replace them with real field names when the field is obvious and the struct is
   shared, otherwise leave them.
4. **Sizes**: `DAlloc(heap, N)` and `memcpy`/`memset`/loop counts that are sizes of arrays of
   pointer-bearing structs, struct copies done as `GhostShip { s32 data[60]; }`, buffers sized
   by a struct's 32-bit size. Use `sizeof` / element counts. Keep the original's slack bytes
   (the original often allocated count*size + 32): write `count * sizeof(T) + 32`.
   A function in the exe that overran a buffer by a few bytes must still not overrun natively.
5. **Prototypes**: unprototyped `int f();` declarations hide wrong argument/return types. A function
   returning a pointer MUST have a full prototype (an implicit `int` return truncates it).
   Remove local re-declarations that clash with `functions.h`/`psyq.h`; fix prototypes to match the
   *definition*. Calls that pass more arguments than the callee declares: drop the extra
   arguments (the callee never reads them) and say so in a comment. Calls that pass too few: look
   at what the callee reads; report.
   PsyQ library functions are declared once, in `game/include/psyq.h` (libgte, libgpu, libetc,
   libcd, libspu, libpad, libcard, libapi); the platform implements them with exactly those
   prototypes, so a change there must be reported. Do not declare PsyQ functions in `.c` files.
6. **Extensions**: `__asm__("name")` aliases, `register ... asm`, `extern inline`, `__inline`,
   `__attribute__((aligned))` on typedefs (see "Shared declarations" for the aliases).
7. **`return;` in a non-void function** (the console returned whatever was left in `$v0`): check
   every caller; if none uses the value return 0 (or -1 where the callers test `< 0`) and say so;
   if a caller uses it, report it as an uninitialised read and keep the closest behaviour.
8. **Console memory**: no absolute console addresses (`0x80xxxxxx`, `0x1F800000`) and no code that
   depends on the 2 MB RAM map. Report any you find (the coordinator has defined `PsxScratchpad`,
   `MAIN_HEAP_SIZE`/`RESERVE_HEAP_SIZE` and `PsxPlayMovie` for the known ones).
9. **File data**: any place that reads a file straight into a struct, walks a buffer as an array
   of structs, or writes a struct to a file/memory card with `write()`/`memcpy` of `sizeof`.
   If the struct contains pointers the layout is wrong natively. Do NOT fix these yourself: list
   them in your report (file, line, struct, what the file holds). Pointer-free structs are fine.
10. **MIPS behaviour the host does not share**: unaligned `lw`-style access through casts, shifts by
    >= 32 or negative, signed overflow (`-fwrapv` is on), integer division by zero (the MIPS
    does not trap: result is unpredictable) and `INT_MIN / -1`, reading uninitialised locals
    the exe really reads, `char` signedness (`-fsigned-char` is on), `float`/`double` (the PS1
    has none; if you see any, report). Do not "fix" the game's own bugs: reproduce them, but
    report each one you notice.

## Shared declarations

* A global that two files declare with different types is a bug. `tools/xdecl.py` lists them.
  Declare it once in `game/include/globals.h` (add it under your own section marker; if the
  declaration is already there, delete your local `extern`) with the **defining** file's type.
  Same-size differences of signedness (`short` vs `unsigned short` for a small counter) are
  harmless; unify them only if every use is insensitive to the sign, otherwise leave them and
  list them.
* A declaration under another C name (`extern TrackData *track__dynam __asm__("track");`)
  becomes a use of the real name; if the file viewed the bytes as another type, keep the
  view with a cast at the use, never a second declaration.
* Two globals of the same name defined (not `extern`) in two files (`loadFrame` in
  `progress.c` and `wadfile.c`): the console linked them as one common symbol. Keep exactly one
  definition (the one in the file that owns the variable) and make the other an `extern`; list it.
* Struct types: the real definitions live in headers. Private copies with padding bytes
  (`typedef struct { unsigned char pad0[4]; ...}`) for pointer-free data are replaced by the real
  type only when it exists and the offsets are proven equal (`layouts.py show`); do not edit the
  functions listed as "equivalent-only" (below) at all.
* Symbols with several names for adjacent storage (a decompiler splitting an array into
  `savedLapTimes`, `raceResetWord0`...) are fixed when you see them.

## Do not touch

The thirteen functions whose only C is a proven-equivalent version are rewritten separately by
the coordinator. Leave **all** of their text unchanged (they may still produce diagnostics):
`race` (racectrl.c), `MenuControl`, `CopyGameData`, `DisplayCardBack` (menus.c),
`UpdateDroneShip` (drone.c), `collideLineToPlane` (collide.c), `DrawLinkSign`, `doSerialGame`
(combat.c), `DrawEnvMap` (object.c), `SfxChainId` (sfx.c), `GetNewWeapon` (weapon.c),
`InGameReset` (options.c), `DrawFlare` (trails.c). Declarations at the top of racectrl.c are the
coordinator's; do not edit racectrl.c at all.

## Reporting

At the end write `docs/port-notes/<your group>.md` (create the directory if needed) and return a
short summary. The note lists, per file: every edit by category (with function names), every
oracle-reported change and its reason, every item for rules 7-10 (file, function, one line each),
every raw-offset/size dependence you found that you did not fix, and every place where you were
unsure. Be precise: the coordinator relies on it, and wrong guesses cost more than gaps.
