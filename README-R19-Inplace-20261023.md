# R19 direct-output pipeline experiment — 2026-10-23

This experiment extends the previous single-site initialization candidate across the intermediate-state pipeline. It preserves the frozen R19 implementation, learned tables and archive format. Generated isolated sources add `mid_step_legacy_into` and `mid_step_into`, which fill a caller-supplied, freshly initialized `MidStep` base. R17's wrapper and R19's middle-game base call these output functions directly. Compatibility wrappers still return `MidStep` for unchanged callers.

The change removes temporary aggregate construction/assignment across the legacy → calibrated → prior wrappers where direct-output callers are used. A fresh initialized result is still required for every call: flags and inactive fields are not left over from the previous position. This experiment does not remove arbitrary initialization or assume inactive members are safe to leave uninitialized. State reuse across plies is deferred pending a complete read/write audit.

`tools/generate_r19_inplace.py` deterministically derives isolated files from frozen sources using exact matched substitutions. The model operations and observation order are preserved. The earlier `r19_fast` candidate is not used as the baseline; comparisons are against the original R19 executable.

## Validation and measurement protocol

The CLI benchmark writes its protocol before validation/timing. It pins CPU0, compares all five supported modes on three datasets byte-for-byte, and checks roundtrip within each pack operation. The existing candidate test suite covers empty inputs, malformed containers, CRC, truncation, wrong model, LUT invariants and R18 payload identity.

Timing uses four alternating ABBA/BAAB blocks for each dataset: 12 blocks and 48 actual CLI processes. Each process warms up and runs three encode and three decode iterations with validation. Paired direction-time ratios use totals within blocks and then a geometric mean. Phase durations are CLI wall time, not separately instrumented CPU time. Builds, training and other benchmarks are not run concurrently. All binaries use the same standard optimization flags and the recorded run reuses the original `rmc.o`.

This is a short sequential experiment on one pinned host core. It does not establish performance on independent hardware or statistical significance. Individual block results, binary/dataset hashes and verification results are retained under `benchmarks-r19-inplace-20261023/`. This experiment does not reproduce the earlier compression generalization study.

## Reproduce

```sh
python3 tools/build_r19.py --out build-r19
python3 tools/build_r19_inplace.py --out build-r19-inplace
python3 tools/benchmark_r19_inplace.py --baseline build-r19/r19_prior --candidate build-r19-inplace/r19_inplace --out fresh-inplace-study
```

## Measured outcome

| Dataset | Encode gain | Decode gain | Candidate median encode/decode ply/s |
|---|---:|---:|---:|
| r19 | +13.585% | +13.163% | 47,973 / 49,018 |
| r18 | +7.840% | +10.027% | 49,136 / 51,472 |
| r17 | +6.505% | +7.050% | 49,438 / 48,711 |

The candidate wins all 12 paired blocks in each direction in this run. The three equal-dataset geometric mean gains are 9.268% encode and 10.052% decode. These exploratory summaries do not establish significance or performance elsewhere. All 15 archive-byte comparisons and 1,226 existing checks passed. Frozen source/model hashes and binary hashes are unchanged. The candidate is retained as a promising optional research implementation; general pack profile234 and the original R19 remain unchanged. No 60–70K or universal speed claim is made.
