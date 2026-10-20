# The renderer's hand-written assembly, in C

Seven routines of the console game were assembly (asm/800.s of the decomp, no C source existed): the three
track-tile drawers, the three object drawers and the particle drawer. They are now C, in `game/src`:

| Routine | File |
|---|---|
| `AsmTrackLo`, `AsmTrackMed`, `AsmTrackHi` | `trackdraw.c` |
| `AsmDrawObject`, `AsmDrawObjectLit`, `AsmDrawObjectFixed` | `objdraw.c` |
| `AsmDrawParticles` | `partdraw.c` |

They run on the GTE of the platform (`GteExecute`/`GteWriteData`/`GteReadData`, `platform/psx/gte.c`) and link their
packets with `AddPrim`, so nothing in them depends on the console's memory map: the scratchpad grids and the vertex
area are ordinary arrays, the 24-bit links are the platform's. The tile surfaces, ordering tables and primitive
buffers are the game's own data.

## How they were verified

`tools/asmtest/render_test.c` runs the **retail routine itself** (the executable's machine code, in the R3000
interpreter `tools/asmtest/mips.c`, with the same GTE) and the C port on the same randomly generated scene, then compares
the packets (every byte), the ordering table chains (which packet at which depth, in which order), the counters
and where the primitive buffer ends. Scenes: all eight tracks' real sections, vertices and faces with cameras
from far to inside the track, random tile textures (so a wrong texture page or mirror shows), objects of 1 to 250
vertices with all primitive types, random flags and sprites with random roll, particle tables with every type. Run
`tools/asmtest/build.sh && /tmp/render_test EXE SYMBOLS DISCROOT` (an executable of the game and its symbol list are
needed; it builds with AddressSanitizer/UBSan). Result: 11200 checks, about 155000 packets, 0 failures. Mutations of the
ports (a swapped corner, a wrong shift, a changed constant, a missing flag test) all make it fail.

## What the original does that a C programmer would not have written (kept)

* Low detail, a mirrored face: the texture page and palette (one word) were loaded in the delay slot of the test for the
  mirror flag, before moving to the mirrored surface, so they always come from the plain surface, while the texture
  coordinates come from the mirrored one.
* Medium and high detail: a coordinate that does not fit in 16 bits is truncated, not rejected (the low detail
  routine rejects the face); the flag bits that reject a point are `0x466` of the upper half of the GTE flag word;
  the high detail routine cuts a partly visible cell into four and draws them from the general primitive buffer
  (`nextPrim`), uncounted in `hiTileCount`.
* Object drawers: the "all vertices rejected" early exit tests the wrong register and never fires; objects end at the
  first primitive of an unknown type; a primitive count (or vertex count) of 0 still processes one primitive (one vertex): the loops test after the body.
* `AsmDrawObjectLit`: sprites (types 10 and 11) are linked through a register that is zero there, so their length byte
  is cleared and the GPU skips them. Lit objects never show sprites.
* `AsmDrawObjectFixed` (the sky) has no sprites and always applies the facing test.
* `AsmDrawParticles` draws 127 of the table's 128 entries; a particle's first frame draws nothing (no previous
  position); a particle that leaves the screen or the 16-bit range is freed.
* The tile surface table is 24 textures of two surfaces (252 bytes each). The 3584 bytes after it, which the
  decompilation's data listing counted as more surfaces, are the object drawers' projected vertices (448 of 8 bytes).
