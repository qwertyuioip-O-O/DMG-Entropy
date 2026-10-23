# R19 incremental slider-ray experiment — 2026-10-23

This isolated candidate extends the attack-union implementation. Previously, if any occupancy change intersected a slider's rays, all of that bishop/rook/queen's directions were recalculated. `updated_slider` instead starts from the cached attack mask and replaces only ray directions intersecting the occupancy delta. Unaffected directions remain unchanged. Moved, castled or promoted sliders whose piece identity changed at their new square use the original complete calculation.

For a fixed square, the eight directional rays do not overlap. Consequently replacing `(old_mask & ~full_ray) | new_ray` preserves the other directional contributions exactly. This operation is applied to one piece's mask before merging masks of different pieces; overlap between different pieces is preserved. Captures on a still-occupied destination do not alter its blocker status. En passant and castling retain all occupancy-delta bits. No learned model, weight quantization, archive field or observation order changes.

`tools/generate_r19_rays.py` deterministically produces isolated sources from the retained attack-union snapshot. The frozen original R19, direct-output baseline and attack-union sources are kept intact.

## Correctness

`tools/test_r19_rays.cpp` compares the entire updated attack mask and all relation flags against full recomputation. Its special-position, two-ply expansion and seeded random-walk checks passed 102,324 candidate comparisons, including 440 castlings, 8 en-passants and 684 promotions. Counts refer to compared candidates, not unique positions. The baseline is the independent full `AttackCache` and the no-reuse relation path, rather than the previous incremental implementation alone.

The CLI comparison also packs all five modes on all three datasets with all three implementations, checks exact archive-byte identity, and checks each pack's own roundtrip. The new candidate runs the existing 1,226-check suite, including malformed inputs, CRC, truncation, wrong model, empty streams, LUT invariants and R18 payload identity.

## Three-way timing protocol

The experiment compares:

- `baseline`: the previously validated direct-output candidate;
- `attacks`: the optional attack-union candidate;
- `candidate`: this per-ray update candidate.

The protocol is written before validation/timing. CPU0 is pinned. Per dataset, the three six-process blocks are `ABC CBA`, `BCA ACB`, and `CAB BAC`. Each implementation occupies each mirrored position pair once. Three datasets give 54 timed CLI processes. Each CLI call warms and performs three iterations in each direction with validation. Two same-label phase durations are summed per block; paired throughput ratios are combined geometrically. Primary comparison is candidate versus direct-output baseline; secondary comparison isolates the change versus the attack-union implementation.

These are actual CLI wall-time phase measurements, using standard uninstrumented `-O3` executables and the same `rmc.o`. No builds, tests, training or other benchmarks run concurrently. This is a short sequential study on one host/core, not a statistical-significance or other-hardware claim. Do not multiply gains from separate studies or treat individual iterations inside a CLI invocation as independent samples.

## Reproduce

```sh
python3 tools/build_r19_inplace.py --out build-r19-inplace
python3 tools/build_r19_attacks.py --out build-r19-attacks
python3 tools/build_r19_rays.py --out build-r19-rays
python3 tools/benchmark_r19_rays.py --baseline build-r19-inplace/r19_inplace --attacks build-r19-attacks/r19_attacks --candidate build-r19-rays/r19_rays --out fresh-rays-study
```

Compile `tools/test_r19_rays.cpp` with the standard flags, include path and `rmc.o` to reproduce the standalone attack-mask oracle. Source diffs, hashes, validation output and raw timing rows are included under `benchmarks-r19-rays-20261023/`.

## Measured outcome

| Dataset | Encode vs direct-output | Decode vs direct-output | Encode vs attack-union | Decode vs attack-union |
|---|---:|---:|---:|---:|
| r19 | +0.675% | +0.437% | -4.692% | +7.792% |
| r18 | +9.249% | +6.636% | +2.415% | +4.347% |
| r17 | -3.023% | +3.809% | -5.141% | +3.415% |

The decoder mean improved on all three datasets against both references, but the encoder regressed on R17 heldout439 versus direct-output and on two datasets versus attack-union. The 30.3% encoder outlier on one R18 block further limits the interpretation of its three-block average. This candidate is optional and is not promoted as a both-direction replacement. All 15 three-way archive comparisons and 1,226 existing checks passed. The full attack-mask oracle also passed.
