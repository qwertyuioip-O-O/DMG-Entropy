# DMT6-Fast — byte-identical performance engineering experiment

**Goal:** optimize existing DMT6 profile 75 without modifying probabilities, learning rules, header, or format. This is not a new codec and does **not** claim independent compression improvements.

## Changes

1. `r4_rank_features`: replace initialization and 129-bin scan of 12 class histograms with strict higher-score ranking of the observed candidates only. Scores tied with the candidate are *not* counted as better, matching the original algorithm.
2. `r6_positional_features`: cache pre-move queen origin rays and original-board attacked-square queries within each ply. This cache is local to each position; it is never reused after board changes.
3. `r5_positional_features`: cache bishop starting mobility and pawn-occupied file bitmasks for the position.
4. `Position::square_attacked`: preserve geometric attack semantics, replacing per-square directional stepping with compile-time ray tables and bitboard first-blocker search. Includes blockers, leapers, pawn direction, and all current piece bitboards.

**Important:** All changes must preserve `DMT6` file bytes exactly. Existing DMT1–DMT5 decoders and all profile IDs are unchanged.

## Build

Portable baseline (no CPU-specific instructions):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE='-O3 -DNDEBUG'
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

Native CPU build (only use a comparable baseline built with the **same** flags):

```sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE='-O3 -DNDEBUG -march=native'
cmake --build build-native -j4
ctest --test-dir build-native --output-on-failure
build-native/dmg_mid_speed 75 --bundle benchmarks-r4/wcc2024_01_13.uci-bundle
build-native/dmt6_fast_attack_tests
build-native/dmt6_fast_compat_80 benchmarks-r4/human23.uci-bundle
```

`dmt6_fast_compat_80` prints the actual payload bit count, file length, and FNV1a-64 digest of all encoded bytes for every profile 0–79. Compare against **unmodified** DMT6 using the same tool built against the original library. FNV digest comparison is a convenient regression check, **not** a cryptographic guarantee or a substitute for direct byte comparison.

## Observed results (2026-10-20, Intel Xeon Platinum 8573C virtualized CPU)

- Unmodified profile 75 at `-O3 -DNDEBUG -march=native`: 54,407 encoding / 54,827 decoding plies/s in one same-machine consecutive comparison.
- Optimized profile 75, same flags: 62,338 encoding / 62,102 decoding plies/s in that comparison; about 1.15×/1.13×.
- Additional alternating WCC13 runs showed substantial shared-host load variation: unmodified ~44.9–51.6K encode plies/s, optimized ~54.6–59.9K. Thus **60K/s is not a guaranteed minimum**.
- For WCC13 profile 75, unchanged: 4,638 payload bits, 608 total bytes, middle CDF 4.1980439 bits/ply.
- Full existing CTest suite **7/7**, fast attack reference test: 17,967 legal positions, 2,299,776 side/square queries.
- 80/80 profile digest comparisons against original DMT6 matched on each of human23, human4 and WCC13 development archives. A full 80-profile comparison of the random48 archive was interrupted due to runtime; do not count it as passed. The existing DMT6 tests include randomized game roundtrip tests.

**Scope:** The existing human archives are developer test material. No new independent human dataset was collected. CPU timing and acceleration are environment-specific; `-march=native` is not a portable deployment flag. ASan/UBSan were not rerun across all 80 profiles in this optimization session.
