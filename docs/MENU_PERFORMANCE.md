# Menu rendering and recorder stalls

Live CPU samples from the reported session show the main thread waiting for the recorder job queue while four workers run the portable JPEG encoder. A separate title/main-menu reproduction shows the same queue backpressure. This blocks presentation on both Metal and OpenGL. Rendering FPS and the game update rate are separate; these changes keep the console update clock.

The recorder now uses libjpeg-turbo SIMD compression in native builds that provide its static CMake target. It keeps quality 95, full image dimensions, 4:4:4 chroma, and the accurate DCT. Encoded bytes can differ because the two codecs round coefficients differently. A regression compares their quantization tables, component sampling, odd image dimensions, and reconstruction error against the source. The portable encoder remains available for minimal builds and WebGL builds. Releases require and bundle the SIMD codec. Mac releases build arm64 and x86_64 SIMD libraries separately and join the static archives.

Identical raw captures share a pending or completed encoding job before compression. Each frame retains its own metadata and token. Slot reuse cannot attach an old image to a new frame. One cached raw image is retained while recording, then released when drained or cleared. A completed buffer can serve the next capture without another large CPU allocation. The encoder can use up to eight workers while reserving two logical cores. The queue remains bounded; it still applies backpressure if compression cannot keep up with sustained unique captures. This change does not silently discard recording frames.

Metal also caches one completed readback buffer up to 64 MiB. Its CPU callback must finish and its GPU command must complete before reuse. Recorder trimming releases this cache. OpenGL already pools readback buffers.

Flat menus previously cleared and shaded the full 12x image for every extra presentation between console updates. Their extra presentations now reuse the rendered image. New tables, direct draws/transfers, graphics settings, scale, widescreen margin, or texture epochs invalidate it. Any projected geometry disables reuse. Races and linked games retain the existing drawing path. Recorded intermediate pictures always rebuild the full inspection data.

`WIPEOUT_FRAME_TIMING_LOG=path.csv` records presentation intervals separately from the existing once-per-second `WIPEOUT_TIMING_LOG`. Logs are buffered and disabled by default. Hidden uncapped tests use `WIPEOUT_INTERPOLATE=1` to exercise the normal extra-presentation path, otherwise hidden runs intentionally show only console fields. Unset `WIPEOUT_FRAMECAP` to disable recording in a hidden test; its presence forces recording even if its text is `0`.

## Validation

The native regression suites cover both codecs, shared pending jobs, recycled frame tokens, saved-file decoding, export data, and menu-image invalidation. Recorder ownership tests pass ASan/UBSan for project code; the existing stb signed-shift exception remains excluded from UBSan. Color/depth, blending, palette, mesh, presentation, and readback tests cover real OpenGL and Metal devices.

All 32 captured RGB images match the reference on each backend, for 64 images total. Reference image comparisons use the previous commit, the same USA disc, fixed console steps, three interpolated pictures per field, 12x rendering, widescreen, and the same scripted menu/race route. Comparisons include the displayed RGB bytes, independently of the lossy recorder codec.

The parent PC matching gate is unavailable because its `xlpc` compiler container is missing. No parent decompilation source or header is edited here.
