# CPU projection lookup optimization

The old resolver could compare every candidate of one ambiguous corner with every candidate of another corner. Each corner retains up to sixteen projections. Those chains are already sorted by decreasing projection index.

The new resolver walks nearest-candidate cursors forward across those sorted chains. With no exact anchor, matching work is linear in the retained candidate counts rather than a repeated Cartesian search. With anchors, a binary search finds the nearest neighbors. Equal-distance ties still choose the newer projection; equal group spreads still choose the first group. Fully bound packets skip heuristic matching but retain the original lower-median spread filter. Cached results still read current projection values.

No GTE hardware arithmetic, clipping, depth rules, interpolation, or game update rates changed. This optimization applies to all graphics backends.

## CPU-stage measurements

Apple M5 Pro, 128 samples per workload, 1,024 quads per sample. Fixture construction and GPU work are excluded. Each sample invalidates the resolution generation to represent newly built packets. These are resolver microbenchmarks, not whole-game FPS.

| Packet type | Before, ns per primitive | After, ns per primitive |
| --- | ---: | ---: |
| bound | 22.46 | 20.51 |
| ambiguous | 952.64 | 179.69 |
| mixed | 153.32 | 83.01 |

The largest gain is in densely ambiguous corners. Full-game timing includes audio, packet traversal, the rest of the game and GPU work; alternating end-to-end runs showed much smaller differences amid run-to-run variation. No large whole-game FPS increase is claimed. A proposed alternative ordering-table cycle guard did not improve measured latency and was discarded.

## Correctness checks

The test keeps an independent frozen copy of the exhaustive resolver. Over 1.7 million public resolutions compare masks, bound status and complete returned projection values against that reference. Cases include all sixteen candidate entries, irregular collision chains, nearest/group ties, flat and stale bindings, missing corners, spread rejection, changed packet words, live projection edits, cache collisions and frame changes. AddressSanitizer and UndefinedBehaviorSanitizer checks pass.

Sampled pictures from a Japanese Saturn race and PAL/NTSC PlayStation runs match the preceding renderer byte for byte when running identical game code. Full OpenGL/Metal regression suites, live switching with recording, macOS, Windows and Web builds are checked. New start-light palette/animation fixes from main are included separately.

Reproduce the differential check with `sh tools/bugtest/run.sh`. The standalone benchmark is `tools/bugtest/test_geometry_perf.c`; define GTE_SOURCE to compare an older implementation. Raw measurements, CPU samples, picture hashes and representative PNGs are in the local ignored `build/geometry-opt` directory.

The parent decompilation byte-match gate remains unavailable because `orig/D3D.EXE` is absent. The modern port checks above are separate from that gate.
