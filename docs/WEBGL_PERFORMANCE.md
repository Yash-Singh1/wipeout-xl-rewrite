# WebGL disc loading and rendering

iOS 16.2 Safari in the iPhone 8 simulator could not allocate the contiguous WASM heap needed for the full Japanese disc. The loader now keeps audio-only raw tracks in 1 MiB JavaScript chunks. The CD reader copies the requested 2352-byte sector into a separate audio buffer. Data files and files shared by data and audio tracks retain their full WASM mappings. WAV files also retain full mappings for their RIFF parser. Audio bytes and rendering resolution are unchanged.

With the full Japanese disc, the observed WASM heap falls from 827,719,680 bytes to 268,435,456 bytes. This is contiguous WASM memory, not total process memory. JavaScript still holds the raw music. A single BIN containing both data and music still requires its full mapping. Cache writes may temporarily retain additional copies.

The loader rejects truncated or oversized streams and releases native allocations after a failed load so a retry does not leak them. Audio has its own sector buffer because the mixer can retain that pointer while the same thread reads cooked data sectors.

WebGL texture uploads no longer call glGetError after each upload. Allocation and shader checks remain, and native OpenGL retains its upload checks. A browser sample of the prior renderer recorded tens of thousands of these polls during a race. A two-bucket decoded-page cache experiment passed rendering checks but was left out because background-browser throttling prevented a reliable full-game performance comparison.

Muted web runs skip opening an audio device. Their clock already follows video fields, so SDL's dummy audio timer is unnecessary. The earlier dummy timer repeatedly raised a function-signature exception. This change does not establish that normal audio playback is fixed. Development sessions remain muted.

## Validation and limits

The full disc loads and reaches a race in Safari on the iPhone 8 iOS 16.2 simulator. Touching the canvas reveals the touch controls. Simulator results establish compatibility only, not physical iPhone GPU throughput.

Desktop checks use the built-in browser with the Apple M5 Pro ANGLE Metal renderer and 3x internal resolution. Early steady samples reach approximately 60 presentations and 30 game updates per second. Long background-browser samples develop roughly one-second timer delays and are unsuitable for throughput comparisons. No sustained device FPS or end-to-end speedup is claimed from those samples. Test pages use separate versioned JS/WASM URLs to avoid accidentally profiling cached builds.

`sh tools/webtest/run.sh` checks the actual page's classification and loading functions, fragmented streams, allocation failures, retry cleanup and RIFF fallback. Its WASM test exercises the real C-to-JavaScript sector bridge across chunk boundaries and verifies that cooked data reads preserve a held audio sector. The native bug suite includes OpenGL and Metal comparisons for palette changes, cache overflow, wrapped pages, mixed passes, meshes, HUDs, presentation and readback. The merge from main also checks that disk-backed recorder images remain visible in its new presented-frame filter.

The parent PC matching gate could not run because the Docker daemon's socket was unavailable. No parent decompilation source was edited.


## iOS 16 race draw stalls

A foreground iPhone 8 / iOS 16.2 Safari simulator test exposed a separate, severe race bottleneck. The animated menu reached 60 FPS, but racing fell into single digits and sometimes below one presentation per second. This was reproducible after clearing accumulated Wipeout test tabs.

Native samples showed WebContent waiting in canvas preparation for the graphics process. The graphics process was mostly inside ANGLE's drawArrays implementation, mapping a generated buffer and waiting for Metal command completion. The same trace included provoking-vertex index generation. Our game shader's flat integer varyings trigger this older ANGLE Metal path.

WebGL now sends its per-triangle metadata through high-precision floating-point varyings, then rounds and converts it back to the original integer types in the fragment shader. Every vertex receives identical texture, palette, window, HD and blend metadata from gpu_renderer.c:Add. Per-vertex geometry, colours, UVs and exact-depth/back-facing flags retain their original behaviour. Constant metadata therefore remains constant across each triangle, while the shader no longer requires provoking-vertex emulation. Internal resolution, depth passes, blending and draw order are unchanged. Desktop OpenGL retains flat varyings; Metal's shaders are unchanged.

A native GL test build can select the WebGL interface with PORT_TEST_WEBGL_METADATA. The GPU regression runner compares all 512 colour/depth output hash pairs with the original flat interface. They match byte for byte, including differing per-vertex depth flags, palettes, texture windows, wrapped pages, HD replacements, mixed transparency, overlap and rendering scales. Full OpenGL/Metal regression suites and macOS/Web builds also pass.

The final compiled WebGL build is tested directly in Safari, with no JavaScript shader override. Local logs and native samples are in the ignored build/web-opt/metadata and build/web-opt/simulator directories. These measurements describe the simulator on an M5 Pro Mac, not physical iPhone throughput. Test audio remains disabled.

The active-race interval from seconds 15 to 55 averages 60.0 FPS over 40 seconds. The longer run also includes the end-of-race screen because the unattended ship eventually times out. The compiled simulator run averaged 60.001 displayed FPS and 29.975 game updates/second over 59.999 seconds, with 3,600 presentation intervals. The p99 interval was 25 ms, the maximum was 30 ms, and none exceeded 33.3 ms. Settings were 3x rendering, widescreen, foreground Safari, recording off and muted audio. An earlier matched-setting foreground race sample averaged 5.656 displayed FPS and 6.953 updates/second; native traces showed repeated draw-time Metal waits. The shader trial separately held roughly 60 FPS for more than two minutes before the final compiled build was tested.


## Muted phone loading over the LAN

The full Japanese disc was 662,967,648 bytes, including 571,307,856 bytes of music. The old HTTP phone page downloaded every track and kept raw music in JavaScript even while muted. A physical iOS 16.3 Chrome report described a crash when the race loaded. That makes memory pressure a plausible cause, but no physical-device crash log was available to establish it.

`tools/serve_web.py` now generates the muted phone page with the existing data-track-only disc support. It retains the complete 91,659,792-byte data track and all game graphics. The virtual silent music tracks are supplied by the existing disc reader. The full-disc web page is unchanged.

The helper serves data in fingerprinted 1 MiB chunks, with gzip and immutable HTTP caching. The Japanese track transfers 59,891,626 compressed bytes, plus the game JS/WASM. Four requests run at a time, and decoded chunks are copied into WASM in order. The loader does not retain the entire track in JavaScript or make a full-track Cache API copy before starting the game. The bounded HTTP cache works on a plain LAN HTTP address, where the browser Cache API is unavailable. The simulator's cached reload issued zero additional chunk requests.

Validation checks gzip and identity HTTP responses, content lengths, the complete original data SHA-256, invalidation when data changes, ordered loading, the four-request bound, truncated chunks, HTTP failure and cancellation cleanup. Existing web disc-sector tests pass. Rendering remains 3x widescreen, recording off and audio muted. Physical iOS 16.3 needs a retry on the updated page to confirm whether this resolves the reported crash.
