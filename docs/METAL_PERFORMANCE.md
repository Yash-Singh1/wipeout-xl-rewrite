# Metal optimization validation

Tested on Apple M5 Pro. Baseline driver was commit 43a3dcf. The benchmark uses tools/bugtest/test_gpu_perf.c with its optimized sampling path, 1,024 textured triangles, warmup, then explicit GPU completion for each measured picture. Two runs per binary supply 128 samples per scale. These timings measure a synthetic renderer workload, not whole-game FPS. Other machine activity caused run-to-run variance; both runs improved at every scale.

| Render scale | Before median, ms | After median, ms |
| --- | ---: | ---: |
| 1 | 2.567 | 1.763 |
| 6 | 3.703 | 2.214 |
| 12 | 6.328 | 4.805 |

Changes reuse upload/vertex buffers only after GPU completion, cap retained buffers, reuse full-clear encoders, and cache bindings within each encoder. Offline shader compilation removes source compilation at release startup, while pipeline specialization still happens on the player's GPU.

Eight-second full-game CPU samples before and after show substantial time in game packet processing and GteResolvePrecise. Changing graphics APIs does not remove those CPU costs. No claim of a universal Mac FPS target follows from this test.

Validation: the optimized compiled-library renderer preserves all 512 previous Metal color/depth hashes, including under Metal API Validation. Full native Metal suites cover palettes, blends, occlusion, mesh holes, Box/Adaptive presentation and texture updates. A moving race completed five Metal/OpenGL switches with recording. Universal release packaging is checked for arm64/x86_64, the compiled library and code signing. Apple Silicon runtime was tested; Intel runtime requires an Intel Mac.

Raw measurements and CPU samples are in the local ignored build/metal-opt directory. The committed benchmark remains available for future hardware comparisons. See RENDER_BACKENDS.md for build options.
