# DMG-Entropy DMT6 — Quiet Queen & Pawn Predictor Experiment

**Date:** 2026-10-20 · **C++20** · **Development-only evaluation; no new independent human games.**

## Executive summary

- Implemented 12 DMT6 profiles (68–79): isolated queen signals, pawn signals, joint feature ablations, unconditional versus quiet-conditional updates, and within-piece-only probability redistribution. Profile 68 is a byte-identical DMT5-61 control after the 6-byte format/version prefix and CRC.
- Best measured new profile on the previously used WCC13 corpus: **75 `r6-queen-pawn-quiet-rate4`**. WCC13 middle CDF log loss **4.223079 → 4.198044 bits/ply**, total archive payload **4653 → 4638 bits**, file **610 → 608 bytes**.
- This is a **modest improvement** and it does not establish general human-data compression <=3.89 bits/ply. The previously used human23 corpus gains only **2 payload bits**.
- The original DMT5 source was independently compiled and checked against the updated code across **all original 68 profiles** on human23; every complete archive matched byte-for-byte (64-bit FNV-1a fingerprint + length + payload count).
- The 60,000–70,000 plies/s requirement is **not verified/satisfied on the current virtual CPU**. See the paired speed measurements below.

## Approach

Prediction baseline: DMT5-61, which itself layers tactical, context/rank, opening-transposition and quiet positional residuals on frozen DMG8. New DMT6 parameters start at zero and update online **only from already decoded moves**. No external opening book, pre-trained parameters or hidden metadata.

Candidate features are only applied in phase 1 (middlegame). The 10 proposed binary candidate signals are:

0. Queen retreat from an attacked source square to a square not geometrically attacked in the *pre-move* position.
1. Queen quiet move to a square not attacked in the pre-move position when the source also was not attacked.
2. Queen move creates a ray attack on an enemy rook or queen not attacked by that queen from its previous square.
3. Queen move increases ray coverage of the enemy king ring.
4. Queen move gains at least three pseudo-available ray squares.
5. Pawn move newly supports an own bishop or knight.
6. Pawn move newly attacks an enemy rook or queen.
7. Pawn move newly attacks an enemy pawn or minor piece.
8. Pawn move changes the pawn from unsupported to supported by a friendly pawn.
9. Pawn push reveals a rook/queen geometric ray to enemy material on its starting file.

All are **heuristic approximation features**, not strict chess evaluation claims. The queen destination safety check uses the current board; discovered attack rays after the hypothetical move can differ. Legality remains strictly checked by the existing codec.

R6 profiles 69–79 vary the feature mask, residual learning rate, and whether mass is redistributed across all legal moves, all noncapture moves, or only moves by that same piece type. Every candidate retains a positive arithmetic-coder frequency. DMT6 uses a separate file magic; legacy DMT1–DMT5 decoders are frozen.

## Actual file compression and phase-matched CDF log losses

Each corpus is processed as a single cold-started archive. `payload` is the arithmetic coder payload; `mid` is the pre-update quantized-CDF log loss average; `bytes` includes container overhead.

### Developer historical/modern/low-rated 23 games, 1718 plies

| Model | Midgame CDF bits/ply | Payload bits | Payload bits/ply | Total bytes |
|---|---:|---:|---:|---:|
| r5-all-quiet-fast | 3.83678 | 6569 | 3.82363 | 859 |
| r6-control | 3.83678 | 6569 | 3.82363 | 859 |
| r6-queen | 3.83417 | 6567 | 3.82247 | 858 |
| r6-pawn | 3.83814 | 6571 | 3.82480 | 859 |
| r6-queen-pawn | 3.83506 | 6568 | 3.82305 | 858 |
| r6-queen-pawn-quiet | 3.83533 | 6568 | 3.82305 | 858 |
| r6-queen-quiet | 3.83416 | 6567 | 3.82247 | 858 |
| r6-pawn-quiet | 3.83782 | 6570 | 3.82421 | 859 |
| r6-queen-pawn-quiet-rate4 | 3.83406 | 6567 | 3.82247 | 858 |
| r6-queen-safety | 3.83414 | 6567 | 3.82247 | 858 |
| r6-pawn-pressure | 3.83735 | 6570 | 3.82421 | 859 |
| r6-queen-piece | 3.84450 | 6577 | 3.82829 | 860 |
| r6-pawn-piece | 3.84682 | 6578 | 3.82887 | 860 |

### Older development set, 4 games, 496 plies

| Model | Midgame CDF bits/ply | Payload bits | Payload bits/ply | Total bytes |
|---|---:|---:|---:|---:|
| r5-all-quiet-fast | 4.16130 | 1964 | 3.95968 | 265 |
| r6-control | 4.16130 | 1964 | 3.95968 | 265 |
| r6-queen | 4.16164 | 1963 | 3.95766 | 265 |
| r6-pawn | 4.15999 | 1963 | 3.95766 | 265 |
| r6-queen-pawn | 4.16079 | 1963 | 3.95766 | 265 |
| r6-queen-pawn-quiet | 4.16060 | 1963 | 3.95766 | 265 |
| r6-queen-quiet | 4.16209 | 1963 | 3.95766 | 265 |
| r6-pawn-quiet | 4.15995 | 1963 | 3.95766 | 265 |
| r6-queen-pawn-quiet-rate4 | 4.16014 | 1963 | 3.95766 | 265 |
| r6-queen-safety | 4.16155 | 1963 | 3.95766 | 265 |
| r6-pawn-pressure | 4.16027 | 1962 | 3.95565 | 265 |
| r6-queen-piece | 4.18426 | 1969 | 3.96976 | 266 |
| r6-pawn-piece | 4.18772 | 1970 | 3.97177 | 266 |

### 2024 World Championship 1–13 games, 1155 plies

| Model | Midgame CDF bits/ply | Payload bits | Payload bits/ply | Total bytes |
|---|---:|---:|---:|---:|
| r5-all-quiet-fast | 4.22308 | 4653 | 4.02857 | 610 |
| r6-control | 4.22308 | 4653 | 4.02857 | 610 |
| r6-queen | 4.20365 | 4642 | 4.01905 | 609 |
| r6-pawn | 4.22336 | 4654 | 4.02944 | 610 |
| r6-queen-pawn | 4.20451 | 4643 | 4.01991 | 609 |
| r6-queen-pawn-quiet | 4.20423 | 4642 | 4.01905 | 609 |
| r6-queen-quiet | 4.20360 | 4642 | 4.01905 | 609 |
| r6-pawn-quiet | 4.22344 | 4653 | 4.02857 | 610 |
| r6-queen-pawn-quiet-rate4 | 4.19804 | 4638 | 4.01558 | 608 |
| r6-queen-safety | 4.20626 | 4643 | 4.01991 | 609 |
| r6-pawn-pressure | 4.22254 | 4653 | 4.02857 | 610 |
| r6-queen-piece | 4.26181 | 4676 | 4.04848 | 613 |
| r6-pawn-piece | 4.25562 | 4671 | 4.04416 | 612 |

### Generated random legal 48 games, 4607 plies

| Model | Midgame CDF bits/ply | Payload bits | Payload bits/ply | Total bytes |
|---|---:|---:|---:|---:|
| r5-all-quiet-fast | 5.15475 | 23541 | 5.10983 | 3005 |
| r6-control | 5.15475 | 23541 | 5.10983 | 3005 |
| r6-queen | 5.15161 | 23530 | 5.10745 | 3004 |
| r6-pawn | 5.15420 | 23539 | 5.10940 | 3005 |
| r6-queen-pawn | 5.15125 | 23528 | 5.10701 | 3003 |
| r6-queen-pawn-quiet | 5.15093 | 23528 | 5.10701 | 3003 |
| r6-queen-quiet | 5.15098 | 23528 | 5.10701 | 3003 |
| r6-pawn-quiet | 5.15467 | 23541 | 5.10983 | 3005 |
| r6-queen-pawn-quiet-rate4 | 5.14930 | 23522 | 5.10571 | 3003 |
| r6-queen-safety | 5.15291 | 23534 | 5.10831 | 3004 |
| r6-pawn-pressure | 5.15392 | 23538 | 5.10918 | 3005 |
| r6-queen-piece | 5.16087 | 23561 | 5.11417 | 3008 |
| r6-pawn-piece | 5.16518 | 23576 | 5.11743 | 3009 |

## Quiet-move decomposition — profile 61 versus 75

Per-piece comparisons are **middlegame quiet (noncapture) selected moves only** and measured from identical game sequences. Positive `bits saved` means DMT6 uses less quantized-CDF log loss:

| Data | Piece | Quiet plies | Log-loss bits saved |
|---|---|---:|---:|
| wcc13 | pawn | 61 | -2.05 |
| wcc13 | knight | 79 | -2.90 |
| wcc13 | bishop | 55 | -1.74 |
| wcc13 | rook | 96 | -4.05 |
| wcc13 | queen | 113 | +26.55 |
| wcc13 | king | 32 | -0.95 |
| human23 | pawn | 112 | +0.85 |
| human23 | knight | 95 | -2.96 |
| human23 | bishop | 95 | -3.22 |
| human23 | rook | 142 | -2.75 |
| human23 | queen | 108 | +12.43 |
| human23 | king | 95 | -1.48 |
| human4 | pawn | 31 | +0.58 |
| human4 | knight | 54 | -0.33 |
| human4 | bishop | 42 | -0.24 |
| human4 | rook | 43 | -0.29 |
| human4 | queen | 23 | +0.79 |
| human4 | king | 18 | -0.22 |

**Interpretation:** On WCC13, 113 quiet queen moves contribute about 26.55 saved bits, partially offset by lost probability on other quiet moves, leaving about 14.5 bits of middlegame log-loss reduction. Pawn-only features are weak; an entire piece-class-conditional queen-only variant (profile 78) is **worse than the DMT5 control**. The useful signal is partly **whether to move the queen**, not merely which square the queen should move to. These analyses reuse model-development data and therefore cannot establish out-of-sample advantage.

## Matched speed tests

`-O3 -DNDEBUG` single-thread timing, input parsing and disk I/O excluded, 7 trials × 8 independent cold-start encode/decode repeats. Same Intel Xeon Platinum 8573C virtual machine, subject to clock scheduling variability.

| Corpus | Profile | Encode plies/s | Decode plies/s |
|---|---|---:|---:|
| WCC13 | DMT5-61 | 55,115 | 55,536 |
| WCC13 | DMT6-69 queen | 52,996 | 52,648 |
| WCC13 | DMT6-75 joint | 46,855 | 46,933 |
| Human23 | DMT5-61 | 55,187 | 55,779 |
| Human23 | DMT6-69 queen | 53,111 | 53,099 |
| Human23 | DMT6-75 joint | 52,544 | 53,018 |
| Human4 | DMT5-61 | 58,411 | 59,680 |
| Human4 | DMT6-69 queen | 57,619 | 57,265 |
| Human4 | DMT6-75 joint | 56,994 | 57,231 |

A supplementary `-march=native` test on WCC13 recorded baseline ~56.0K, queen ~53.8K and joint ~52.9K plies/s. **None proves the 60K speed floor** in this execution environment; higher speeds measured in previous sessions were on other shared-machine conditions and cannot be silently substituted. This is a research candidate, not mainline-ready.

## Exactness and compatibility

- Built with C++20 Release and passed CTest **7/7** after updating DMT6 tests.
- DMT6 golden archive generated from sorted human23 `.uci` files, profile 75, 858 bytes / 6567 payload bits. SHA256: a9aac6ba5e621856938e7b6e7acb87c254e0aaa50a392e4d93d040dfd21fe719.
- DMT6 tests require byte-identical repeat encoding, exact decode, 80 profile roundtrips on the development archive and random generated games, CRC damage rejection, proper magic dispatch, and old DMT1–DMT5 golden streams.
- Independently compiled **original DMT5 release** versus modified DMT6 tree and checked the complete binary archive for **all 68 existing profiles** on human23. All fingerprints and sizes matched exactly. This is a strong regression check on this corpus, not a mathematical proof for all possible board states.
- Sanitizer-enabled full 80-profile test exceeded the execution timeout. Replaced with a bounded ASan+UBSan smoke test for all **12 new DMT6 profiles** using several legal games and random positions. It passed with strict UBSan halt and leak detection disabled. Do **not** describe the timed-out full suite as passed.
- Cross-architecture Rust/ARM/WASM byte equivalence and a large external human dataset were **not tested**.

## Decision and next steps

1. **Keep DMG8-Fast and DMT5 as stable baselines.** Do not promote DMT6-75 yet.
2. DMT6-69 is the more conservative **queen-only** candidate: most of the WCC13 improvement with less extra computation. DMT6-75 makes WCC13 slightly smaller but requires more compute and exhibits modest overfitting risk.
3. Prioritize learning `move queen vs do not move queen`, not solely queen-destination rankings. Conditional pawn-only and queen-only destination classes worsen in current tests.
4. Avoid more absolute pawn heuristics until the precise prediction error is characterized. Investigate opponent reply/structure consequences and selective, contextual features with a clear cost/benefit model.
5. Acquire new broad, deduplicated human games (several thousand or more) from diverse player ratings/time controls. Freeze the profile before final evaluation; WCC13 and the older 23/4 games are now all development data.
6. Optimize compute only if compression gains justify the cost, and verify the >=60K plies/s requirement on the target CPU under matched benchmarks.

## Reproduction

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
build/dmg_mid_matrix_r6 wcc13 benchmarks-r4/wcc2024_01_13.uci-bundle
build/dmg_mid_matrix_r6 human23 benchmarks-r4/human23.uci-bundle
build/dmg_mid_speed 61 --bundle benchmarks-r4/wcc2024_01_13.uci-bundle
build/dmg_mid_speed 75 --bundle benchmarks-r4/wcc2024_01_13.uci-bundle
build/dmg_r6_piece_analysis benchmarks-r4/wcc2024_01_13.uci-bundle 61 75 queen_pawn_delta.csv
```

**Warning:** Future edits to any DMT6 probability or learning rule must use another profile/version ID. Do not silently reinterpret an already-written `DMT6` binary.
