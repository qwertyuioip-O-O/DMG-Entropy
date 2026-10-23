# R19 audited raw-scratch initialization experiment — 2026-10-23

The accepted LTO direct-output build is retained as the baseline. A PGO-only attempt on top of it regressed on every timing dataset, so this is a separate source-level candidate using ordinary LTO flags, without PGO, attack-union or incremental-ray changes.

## Audited change

Four temporary `std::array<uint64_t,256>` scratch buffers no longer receive unconditional zero initialization:

- `mid_residual_cdf`: every element `[0,n)` is assigned a calibrated weight before quantization; the inactive-model path returns before the scratch buffer is needed;
- `r7_mix`: alpha endpoints return directly; otherwise every element `[0,n)` is assigned the combined mass;
- `mid_step_into`: every element `[0,n)` is assigned from the corresponding coded weight and calibration score before the final quantization;
- `step19`: every actual move index receives a complete assignment in either the mode1 or mode2/3/4 branch; mode0/non-middle returns before the scratch buffer is used.

The unchanged `v6_quantize` reads only `[0,n)` in its mass sum and output loop. These buffers are passed by const reference, not copied wholesale or serialized. No accumulation depends on an initial zero. Unused slots are not evaluated. All persistent model state, feature arrays and intermediate-state flags retain their existing initialization. No whole-structure or arbitrary-array initialization is removed.

`tools/generate_r19_scratch.py` generates isolated variants from the retained direct-output sources, matching each exact function and declaration. Source diffs and checksums document the limited change. Compiler optimization may already remove some redundant stores; a measured gain would not establish that every source-level zeroing change independently contributed.

## Validation, timing and selection

The candidate must pass exact archive-byte identity against LTO on all five modes × all three datasets, including each pack's roundtrip, and the existing 1,226-check suite. This source audit supplies the write-before-read argument; ordinary byte-equality checks alone are not a proof against all uninitialized-memory errors.

Timing and aggregation follow the accepted aggregate-tradeoff preference: pin CPU0; four alternating ABBA/BAAB blocks per dataset; 48 actual CLI processes; 12 paired blocks per direction. Each process warms then runs three timed iterations per direction with validation. Sum two same-label CLI phase wall durations within a block; take geometric mean throughput ratios, then weight the three datasets equally. Retain the candidate as next research working build only if aggregate encode and decode point estimates both improve. Disclose dataset regressions and preserve LTO fallback.

The protocol and rule are recorded before timing. Builds, training, tests and other benchmarks do not run concurrently. This short single-host/core study does not establish significance or other-hardware performance; do not multiply its percentage gains by prior studies.

## Reproduce

```sh
python3 tools/build_r19_lto.py --out build-r19-lto
python3 tools/build_r19_scratch.py --out build-r19-scratch
python3 tools/benchmark_r19_scratch.py --baseline build-r19-lto/r19_lto --candidate build-r19-scratch/r19_scratch --out fresh-scratch-study
```

Both builds use `-O3 -flto=1` and the same model/source baseline. General pack profile234 remains a separate default. The current research build is identified in `CURRENT-SPEED-BASELINE.md`.

## Measured outcome

| Dataset | Encode gain vs LTO | Decode gain vs LTO | Candidate median encode/decode ply/s |
|---|---:|---:|---:|
| r19 | -2.884% | +3.204% | 49,236 / 50,083 |
| r18 | +3.711% | +7.569% | 50,368 / 49,989 |
| r17 | -0.123% | +2.685% | 49,529 / 50,028 |

Equal-dataset geometric mean gain: encode +0.198%, decode +4.463%. Encoding is effectively flat at this precision and measurement variability; its small positive aggregate is not a demonstrated stable gain. Decode improves in all three dataset means and in 9/12 blocks. Encoding regresses on new422 (−2.884%) and R17 heldout439 (−0.123%), with 5/12 blocks positive overall. Under the user's aggregate-tradeoff preference and predeclared point-estimate rule, the candidate is retained as the next research working build, with LTO kept as fallback. This is an experimental selection, not a significance or universal-performance claim. All 15 archive-identity cases and 1,226 existing checks passed. No 60–70K target claim is made.
