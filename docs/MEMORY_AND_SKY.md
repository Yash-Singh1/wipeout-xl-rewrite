# Recorder memory and missing sky faces

The Phenitia Park recording supplied on 2026-11-06 exposed the black screen clear at pixel 192, 3. No sky face covered it. Fixed-background object rendering rejected whole faces when a corner exceeded the GTE projection limits. Nearby scenery already had a camera-space clipping fallback; the sky had none.

Rejected sky faces now keep their original corners and diagonal and use the GPU clipping path. They retain the fixed background ordering slot, background bindings, opaque sky behavior, and each game's flat or Gouraud colors. Unrejected faces still use the existing path. A regression uses the real GTE rejection flags and verifies that the replacement face covers the reported pixel without leaking background state.

## Memory ownership

The user's running process had a 3.5 GB physical footprint and a 4.7 GB peak. The saved 179-frame recording contained 588.4 MiB of JPEGs, which the recorder also retained in RAM. GPU resources and allocator caches accounted for further memory. These measurements describe the old running process, not the final build.

Changes preserve render scale, JPEG quality, image dimensions, recording rate, and history duration:

- Successfully saved pictures release their compressed RAM copies. Browsing loads and decodes the selected picture from its saved file. Failed writes retain the RAM copy. Re-saving protects backing files whose frame indices have changed.
- Consecutive byte-identical JPEGs share one compressed allocation while resident.
- VRAM snapshots share unchanged 4 KiB pages. Inspection and export reconstruct the original full image into one reusable buffer. Export names and the version 3 binary format remain unchanged.
- Texture arrays retain CPU copies only for uploaded layers. Large RGBA textures retain GPU storage and a temporary upload rectangle instead of a second full CPU image. Backend changes read current GPU pixels before migration, including partial updates.
- Metal releases completed command references and trims retired upload buffers when the recorder freezes, saves, or clears. On macOS, those transitions also return free allocator pages. Live rendering resources stay intact.

Saved pictures now depend on their export files while the recording remains loaded. Removing those files externally makes those pictures unavailable to the recorder.

## Measurement

A matched hidden title/menu run on Apple M5 Pro used Metal, 12x rendering, no downsampling, the recorder console, and the same Japan PlayStation disc. It froze at presented picture 500. Both builds exported 179 frames, 1,608 inspection records, and 321.189 MiB of JPEG files. The sampled JPEG was byte-identical. Memory was sampled two seconds after the save completed.

| Physical footprint reported by vmmap | Before | After |
| --- | ---: | ---: |
| Frozen recorder | 1.8 GB | 1.4 GB |
| Peak | 2.0 GB | 1.6 GB |

Numbers are rounded by vmmap and vary with driver and allocator state. This compares the same isolated workload. It does not establish the final footprint of the user's previously running race. Local results and the reproduction script are under ignored `build/memory-fix/verified-*` and `check_final.py`.

12x color and depth storage itself is large. The current 1024x512 VRAM targets use 576 MiB, plus 216 MiB for 384x512 widescreen targets. Those allocations remain at the requested quality. Rolling recordings also need compressed history and pending readbacks until saved or cleared.

## Validation

The full native regression suite and real Metal GPU suite pass. Coverage includes GTE sky rejection, scenery clipping, guest sky colors, snapshot reconstruction, JPEG sharing, failed saves, disk-backed decoding, binary/VRAM export, sparse texture-array migration, partial large-image uploads, backend rollback, pending reads, palettes, blends, depth, and presentation modes.

Recorder tests pass AddressSanitizer and UndefinedBehaviorSanitizer for project code. The pre-existing signed shift in vendored stb_image_write is excluded from UBSan; vendor code remains under ASan. Renderer migration tests also pass the sanitizers.

Native macOS, Windows, WebGL, and universal arm64/x86_64 Mac release builds pass. The universal app contains the compiled metallib and passes code-signature verification. Intel runtime was not available for testing.

The parent PC decompilation gate cannot run in this checkout because its compiler container `xlpc` is unavailable. This change does not edit that repository's matching source or headers.
