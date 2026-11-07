# WebGL disc loading and rendering

iOS 16.2 Safari in the iPhone 8 simulator could not allocate the contiguous WASM heap needed for the full Japanese disc. The loader now keeps audio-only raw tracks in 1 MiB JavaScript chunks. The CD reader copies the requested 2352-byte sector into a separate audio buffer. Data files and files shared by data and audio tracks retain their full WASM mappings. WAV files also retain full mappings for their RIFF parser. Audio bytes and rendering resolution are unchanged.

With the full Japanese disc, the observed WASM heap falls from 827,719,680 bytes to 268,435,456 bytes. This is contiguous WASM memory, not total process memory. JavaScript still holds the raw music. A single BIN containing both data and music still requires its full mapping. Cache writes may temporarily retain additional copies.

The loader rejects truncated or oversized streams and releases native allocations after a failed load so a retry does not leak them. Audio has its own sector buffer because the mixer can retain that pointer while the same thread reads cooked data sectors.

WebGL texture uploads no longer call glGetError after each upload. Allocation and shader checks remain, and native OpenGL retains its upload checks. A browser sample of the prior renderer recorded tens of thousands of these polls during a race. A two-bucket decoded-page cache experiment passed rendering checks but was left out because background-browser throttling prevented a reliable full-game performance comparison.

Muted web runs skip opening an audio device. Their clock already follows video fields, so SDL's dummy audio timer is unnecessary. The earlier dummy timer repeatedly raised a function-signature exception. This change does not establish that normal audio playback is fixed. Development sessions remain muted.

## Validation and limits

The full disc loads and reaches a race in Safari on the iPhone 8 iOS 16.2 simulator. Touching the canvas reveals the touch controls. Simulator results establish compatibility only, not physical iPhone GPU throughput.

Desktop checks use the built-in browser with the Apple M5 Pro ANGLE Metal renderer and 3x internal resolution. Early steady samples reach approximately 60 presentations and 30 game updates per second. Long background-browser samples develop roughly one-second timer delays and are unsuitable for throughput comparisons. No sustained device FPS or end-to-end speedup is claimed from those samples. Test pages use separate versioned JS/WASM URLs to avoid accidentally profiling cached builds.

`sh tools/webtest/run.sh` checks the actual page's classification and loading functions, fragmented streams, allocation failures, retry cleanup and RIFF fallback. Its WASM test exercises the real C-to-JavaScript sector bridge across chunk boundaries and verifies that cooked data reads preserve a held audio sector. The native bug suite includes OpenGL and Metal comparisons for palette changes, cache overflow, wrapped pages, mixed passes, meshes, HUDs, presentation and readback. The merge from main also checks that disk-backed recorder images remain visible in its new presented-frame filter. A final simulator repeat was unavailable after device support reported that it could not communicate with the device service.

The parent PC matching gate could not run because the Docker daemon's socket was unavailable. No parent decompilation source was edited.
