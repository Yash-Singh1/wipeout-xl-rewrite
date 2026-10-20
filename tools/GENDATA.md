# Global data of the game (tools/gendata.py)

`game/` holds the decompiled C. The decompilation never linked a program of its own: every global
lives in assembly files (`asm/data/*.s`, written by splat) and the sources only `extern` them,
or define them tentatively (`int attractCycles;`, which the console build left to the assembly).
`tools/gendata.py` writes the definitions, typed with the game's own declarations and initialised
from the retail executable, to `game/data/*_data.c`.

```
python3 tools/gendata.py                  # (re)write game/data/*_data.c and game/include/data_alias.h
python3 tools/gendata.py --check          # compile, compare with the executable, build and run the alias tests
python3 tools/gendata.py --report         # counts, declarations that disagree, adjacency, literals
python3 tools/gendata.py --evidence NAME... [--span N]   # one name or 0xADDRESS: declarations, labels, every access
python3 tools/gendata.py --conform        # lay every access of the machine code over the chosen types
python3 tools/gendata.py --hazards        # declarations whose size differs from the definition (32 and 64 bit)
python3 tools/gendata.py --apply-aliases  # drop the declarations of alias names from game/src (see below)
python3 tools/gendata.py --externalize [--dry-run]   # make the tentative definitions in game/src extern
python3 tools/gendata.py --rescan         # drop the cached clang ASTs (also refreshed when a header changes)
```

Needs `clang` (any, with the x86 or arm back end) and Python 3. Reads, never writes, the decompilation
checkout (`--decomp`, default `../wipeout-xl/decomp`): `asm/data/*.s`, `symbol_addrs.txt`,
`scus_943.51.yaml`, `SCUS_943.51.ld`, `asm/**/*.s` for the function names, and `../extracted/SCUS_943.51`
(read as bytes, never run). The `tools/mipsscan.py` scanner reads the executable's code.

## What it does

1. **Declarations.** Every `game/src/*.c` is parsed with clang (`-ast-dump=json`, `-DDATA_ALIAS_H` so the
   alias macros stay out of the way) for a 32-bit little-endian target with the MIPS o32 record layout:
   `armv7-unknown-linux-gnueabi` (4-byte pointers and long, `double` and `long long` aligned to 8, signed
   char). `tools/gendata_stubs/` stands in for the C library headers `port.h` includes. All declarations
   of each labelled symbol are collected, also those made under another identifier with
   `type name __asm__("symbol")` (clang's `mangledName`).
2. **Which symbols.** The labels of `asm/data/*.s` (rodata, data, bss) plus the names of `symbol_addrs.txt`
   that have no label (names inside a bigger label's range: `bigGrid`). Of those, the ones the game
   sources declare and that are not PsyQ runtime data are defined. Unnamed labels (`D_8008E982`) are
   addresses the code mentions, mostly fields or elements of bigger objects: they never end an object,
   and the sources never use one. Data only the libraries touch is left out; so are the jump tables and
   literals of the functions compiled from C (the compiler makes its own): only data the C code names.
3. **Type.** Among the declarations of a name: the ones that need no other width than the code uses at
   that address, then pointers over integers of the same size (a pointer is 8 bytes natively), then
   arrays and records over scalars (the scalar `u16 linkPadHead`-style declarations are the decompiler's
   word-wise views), then the larger element, then the complete over the incomplete array, then headers
   over `.c` files and the majority. Declarations under another identifier count only when they are an
   aggregate that fits the object. The length of an incomplete array comes from the extent: the distance to
   the next name the game declares (the names of the aliases below do not end an object), cut at the end of
   an assembler rodata file (what follows are the literals of C functions), counted in elements of the
   32-bit layout: an array of pointers has as many elements as the console had, each natively 8 bytes.
   Strings are `char x[] = "..."`, padded with the zero bytes up to the next word.
4. **Values.** Positional C89 initialisers from the executable. A pointer is recognised by its type, found
   in the assembler output (a `.word` naming a label, or a raw `0x8xxxxxxx` the splitter found no label
   for) or by its value, and written as `&symbol`, `symbol[i].field`, a function name, or an equal string
   literal when it points into data nobody names (the table of CD track names). Uninitialised data is a
   plain definition.
5. **Files.** One file per source file, symbols in address order, chosen by who references the name and by
   the link order of the object files (`SCUS_943.51.ld`); a best guess at the original file, not a fact.
   Types a `.c` file declares privately are repeated verbatim in its data file, and the objects an
   initialiser points to are declared at the top.
6. **Tentative definitions in `game/src`.** 501 symbols that the data files define are defined as well by a
   tentative definition in a `.c` file (533 declaration lines; zero-filled there). `--externalize` makes
   those lines `extern`, mechanically and idempotently, and is the one change to `game/src` that the data
   files need beyond the aliases; run it once the build branch is merged (it is not applied on this
   branch, so that the build branch's edits of the same lines do not conflict). Four initialised
   definitions in `game/src` (`startStateTable`, three statics) are the game's own data; `--check`
   compares `startStateTable` with the executable.

## Check

`--check` compiles every file for the 32-bit target (one section per symbol, `-fdata-sections`), reads the
ELF object files, and compares each symbol with the executable: every byte that is not a pointer must
equal, every pointer must relocate to the same target (symbol plus offset, or a string literal with the
same text), and the layout model's size must be clang's. It compiles the same files natively (64-bit,
`-std=gnu11`; they also build with `-std=gnu89`), checks that no array of pointer-bearing elements was
sized by native bytes, that no object is bigger than the next declared name unless a decision says why
(an alias, `TRUNCATE`), and builds and runs the alias tests.

Shared storage. `tools/gendata_decisions.py` lists the names that are really inside a bigger object (an
element, a field, a table that continues into the next global), with the evidence from the machine code.
`game/include/data_alias.h` (generated, included at the end of `functions.h`) has one macro per name onto
the real storage, the declarations of the alias names in `game/src` are removed (`--apply-aliases`), so
the data files define the container only. `--check` verifies each decision twice: for the 32-bit
layout, that the storage the macro reaches is exactly where the executable has the name (a typedef per
alias), and natively, that the 64-bit layout keeps it inside the container, that the paired elements are
the same object and that the executable's initial bytes show through the alias (`tools/aliastest/*.c`,
built, linked with the data files and run). A few uses had to be rewritten (`REWRITES` in the decisions,
each a line or two marked "Port:").

## The machine-code evidence

`tools/mipsscan.py` follows the registers of every function of the executable as `constant + sum(k * opaque)`
(`lui`/`addiu`/`ori`/`addu`/`sll`, gp-relative small data, `lwl`/`lwr` pairs) and records each load and store
through such an address with its width and the strides of the index. `--evidence` prints them next to the
labels and the declarations; `--conform` lays them over the chosen types: of 8,500 accesses of the game's
code to defined data, none lies outside the object it falls in except those of the libraries'
own tables. Code the game's authors wrote in assembly (`asm/800.s`) uses `$gp` as a scratch register and is
read without it.

## 64-bit notes

* Arrays whose elements contain pointers are sized by the element count the 32-bit layout gives
  (extent / 32-bit element size); native, they simply take more bytes. `--check` verifies that none of
  them was sized any other way without a recorded reason.
* `--hazards` lists the declarations in the sources whose size differs from the definition's natively
  although the console's did not: a variable declared `int` in one file and as a pointer in the others
  (`heap`, `reserveHeap`, `mainheapcopy`, `weaponIndex`, `OT`...). The definition is the pointer; the
  integer declarations read half of it and must be fixed in the sources.
* Byte arenas that code cuts into structures with pointers (`ARENAS` in the decisions): none was found
  in static data. The primitive buffers hold GPU packets (no pointers), `dirCache` disc records, the
  ghost takes (`recordBuffer`, `replayBuffer`) a header with a 240-byte copy of the first words of a
  `ShipData` (see the report).

## Adding a decision

When a new name inside an object turns up (`--check` says "needs an alias or TRUNCATE decision" for a
declared type that does not fit before the next declared name): run `--evidence`, decide from the
accesses and the C text, add an `A(...)` to `ALIASES` (or `TRUNCATE`, `SIZES`, `ALIGN`, `PREFER`,
`ABSOLUTE`), run `--apply-aliases` and `--check`.
