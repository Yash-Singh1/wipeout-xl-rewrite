# Menu rendering and recorder stalls

The reported menu slowdown comes primarily from recording work blocking presentation. Live samples show the main thread waiting for a full JPEG job queue while four workers run the portable encoder. Recording also waited synchronously for the previous GPU readback. Both waits affect Metal and OpenGL.

Native builds now use the static libjpeg-turbo SIMD codec when available. Compression keeps quality 95, full image dimensions, 4:4:4 chroma, and the accurate DCT. Encoded bytes can differ because the codecs round coefficients differently. Tests compare quantization tables, component sampling, odd image dimensions, and reconstruction error against the source. Minimal builds and WebGL retain the portable encoder. Release builds require the SIMD codec; Mac releases build each architecture separately before joining the static archives.

Identical raw captures share a pending or completed encoding job. Each frame retains its own metadata and token, and recycled slots cannot receive an old image. One cached raw image remains available while recording and is released when drained or cleared. Completed CPU buffers can serve subsequent captures. Up to eight encoder workers run while reserving two logical cores.

Normal recording polls GPU readbacks without waiting. A pending GPU ticket or a full encoder queue skips the next preview image instead of stalling rendering. Geometry and VRAM inspection records remain available. The Recorder tab identifies skipped previews, and exported info.json counts them. Captured previews retain full resolution and quality; the configured capture rate is an upper bound. Explicit save/freeze operations still drain outstanding work.

Metal caches one completed readback buffer up to 64 MiB. Its callback and GPU command must finish before reuse. Recorder trimming releases the buffer. OpenGL already pools readbacks.

Flat menu packets previously cleared and shaded the full 12x image for every extra presentation between console updates. Extra presentations now reuse that image until new tables, direct draws/transfers, graphics settings, scale, widescreen margin, or texture epochs invalidate it. Any projected geometry disables reuse. Races and linked games retain the existing drawing path. Recorded intermediate frames rebuild their inspection data.

## Measurements

Apple M5 Pro, USA disc, the same scripted route, hidden 1920x1200 surface, 12x rendering, widescreen, full-resolution rolling recording at up to 60 captures/second, presentation cap 144. Runs are sequential. The user's pre-fix game stays open during the tests. Menu samples cover seconds 15 through 22; race samples cover seconds 33 through 39. Startup and menu/race transitions are excluded. Both revisions include CPU profiler overhead in the same sampling windows.

| Metal workload | Before average FPS | After average FPS | Before p99 interval | After p99 interval |
| --- | ---: | ---: | ---: | ---: |
| Animated menu | 43.9 | 144.0 | 70.37 ms | 10.64 ms |
| Race | 33.1 | 141.0 | 144.79 ms | 13.40 ms |

The final race's maximum observed interval is 18.85 ms. These local results do not imply zero scheduling jitter. Raw logs and scripts are in ignored build/menu-perf; the final runs are async-menu-metal and async-race-metal, with the reference runs under verified-menu-before and verified-race-before.

Flat title screens separately exceed 2,000 repeated presentations/second in the uncapped hidden run. The console animation still updates about 30 times/second. This does not establish 1,000 FPS for animated 3D menus or visible macOS windows.

The final hidden OpenGL race reaches 65.6 FPS with a 32.53 ms p99 interval. Its sample shows remaining macOS OpenGL driver presentation/GPU completion waits. Removing recorder waits does not remove those driver/GPU limits. The final trace is under async-race-gl.

## Validation

All 32 reference RGB captures match on each backend, for 64 comparisons total. The rendering comparison uses the previous commit, the same disc and scripted menu/race route, fixed console steps, three interpolated pictures per field, 12x rendering, and widescreen. This comparison precedes the separate startup and rumble changes merged from main. Lossy JPEG codec output is checked independently.

Native regression suites cover both codecs, shared pending jobs, recycled frame tokens, disk-backed pictures, export data, and menu-image invalidation. Overload tests fill the encoder queue without a consumer and leave a GPU ticket unfinished. Normal capture returns without waiting, marks the skipped preview, and preserves inspection data. Clearing/reusing slots resets the mark. Ownership tests pass ASan/UBSan for project code; the existing stb signed-shift exception remains excluded from UBSan.

Real OpenGL and Metal tests cover color/depth, blends, palettes, meshes, presentation and readbacks. macOS, Windows with static SIMD JPEG, WebGL with the portable encoder, and the universal Mac release build pass. The universal app retains its metallib, JPEG notices, and valid signature. Intel hardware runtime was not available.

WIPEOUT_FRAME_TIMING_LOG=path.csv records individual presentation intervals separately from the existing once-per-second WIPEOUT_TIMING_LOG. Both are buffered and disabled by default. Hidden real-time tests use WIPEOUT_INTERPOLATE=1 to exercise extra presentations. Otherwise hidden runs intentionally show only console fields. Unset WIPEOUT_FRAMECAP to disable hidden recording; its presence forces recording even with the text 0.

The parent PC matching gate remains unavailable because the xlpc compiler container is missing. No parent decompilation source or header is edited.
