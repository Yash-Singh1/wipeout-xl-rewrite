# Records that grow on a 64-bit host

The PlayStation compiler (MIPS o32) laid structures out with 4-byte pointers. Natively every structure that holds a
pointer grows, and so does the offset of every later field. `python3 tools/layouts.py audit` lists them: 63 of the
game's 360 types differ in size or layout (the C names appear twice, as `struct X` and `X`).

## What matters

A structure that is only in memory needs nothing: the compiler lays it out and every access is by field name
(the audit for the code is `tools/xdecl.py` and `tools/layouts.py`; the hazards that remain are the ones where the
code uses a size or offset of the console: `python3 tools/gendata.py --hazards`).

A structure that is read from, or written to, a file or the memory card must keep the file's layout. Three kinds of
file record hold pointers (the pointer slots in the file are meaningless and are overwritten by the loader):

| Record | File | Console size | Native size | Where widened |
|---|---|---|---|---|
| `Object` | `*.PRM` (the scene's objects) | 64 | 104 | `LoadPrmCommon` (`scene.c`) via `PortWidenPrmImage` |
| `Skeleton` | `*.PRM` (one per object) | 80 | 96 | same |
| `TrackSection` | `TRACK.TRS` | 156 | 240 | `WADInitSections` (`track.c`) via `PortWidenHeapRecords` |

`Face`, `VECTOR`, the PRM primitives (`F3`..`GT4`, `SPR`), `ConfigData`, `CardEntry`, `HiScore` and the other records
that the game reads from the disc or the card are pointer-free and have the same layout natively; the memory card
files are byte-identical to the console's.

## How the widening is done

* `tools/genrecords.py` reads the layouts clang reports for both targets (`tools/layouts.py dump`) and writes
  `game/src/port_records.c`: for each record type, one row per field with its file offset, native offset, width and
  whether it is a pointer slot. `game/include/port_records.h` is the matching header.
* `game/src/port_widen.c` applies the tables: `PortWidenRecords` (file image to native array, pointer slots
  zero-extended), `PortNarrowRecords` (the other way, used by the tests), `PortWidenHeapRecords` (allocates the native
  block), `PortWidenPrmImage` (walks a PRM image and widens each `Object` and `Skeleton`, starting every native
  `Object` on an 8-byte boundary; `PortAlignPrm` skips that padding when the loader walks the image).
* The tables are generated, never edited by hand; the build checks that `port_records.c` is up to date
  (`port_records: up to date` in the test output).

## Tests

`python3 tools/recordtest/run.py` (about 90 s, 182 checks):

* An independent reader (Python, from the two layout dumps, not from the generated tables) builds the expected native
  record for every `Object`/`Skeleton` of every PRM on the disc (2712 objects, standalone and inside the WADs) and every
  `TrackSection` of all tracks (3472), and for random records. The C widening must equal it byte for byte, including
  padding. `narrow(widen(x)) == x` is checked too.
* The game's own loaders run (`tools/recordtest/loadtest`, the real `game/src/*.c` with a recording allocator) through the
  start-up, the menu's loading and every race's loading (8 tracks x 4 classes): the objects, sections, vertices and faces
  the game builds are compared with the files' contents (38795 records), and the heap sanitiser reports any write outside
  a block.

## Heap sizes

The console's heaps (`MAIN_HEAP_SIZE` 0x18F04 bytes, `RESERVE_HEAP_SIZE` 0xE1000) hold the loaded files. Natively the same
loads need more: the allocator's block header is 32 bytes instead of 20 (blocks are multiples of 8 instead of 4), and
the objects, skeletons and sections above are bigger. The load test measured, per sequence, the console's footprint
against the native one: the start-up needs 1.054 times the console's main heap, the menu 0.31, the worst race (track 13)
1.287 times the reserve heap. `PORT_HEAP_SCALE` in `game/include/malloc.h` is 2 (main 204296 bytes, reserve 1843200),
so every loading sequence fits with room; it is not a tight bound. Heap calls on the main heap during the menus' and
races' loading: none. Two heap call sites are not reached by any of the sequences (`display.c:33`, `ttf.c:20`).

## Other native layouts the platform must honour

`DIRENTRY` (memory card file system, 40 -> 48 bytes) is filled by the platform's `firstfile`/`nextfile`. `HeapBlock`
(20 -> 32). `GhostReplay` (25460 -> 25480): it holds a `ShipData` with native pointers, is only copied between the
record and replay buffers in memory and never written to a file; `progress.c` reuses `recordBuffer` as the WAD cache
and its size is 20 bytes bigger.
