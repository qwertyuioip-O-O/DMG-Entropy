# DMG-Entropy DMT7: human-intention features and conditional calibration

**Date:** 2026-10-20  
**Baseline:** DMT6-Fast profile75. **Experiment:** DMT7 new profiles 80–97.  
**Primary goal:** Human middlegame <=3.99 payload-equivalent CDF bits/ply on *new diverse human games* while encoding/decoding >=60K plies/s.  
**Verdict:** **Not met; no new default should be selected.** This is a useful negative experiment and reusable test harness.

## What was implemented

1. **Quiet-intention features (ten bits, pre-move only):** minor-piece first development, two-step pawn-space gain, central pawn push, deep pawn advance, escape from danger, defended safe square, penetration toward enemy half, pawn threat, opponent's vacated square, movement of same piece as previous own move. Reuse the previously computed DMG8 candidate tactical flags for safety indicators; the early independent attack-map version was slow and removed before final comparison.
2. **Online probabilities:** DMT6 profile75 unchanged followed by (a) global or four-context online residual training, (b) quiet-only conditional reshuffling versus all-candidate normalization, (c) fixed 50/75% or online integer-confidence mixing with the baseline distribution.
3. **Lightweight conditional calibration:** safety-guarded redistribution on quiet-only boards, tactical boards, or contexts with recent tail misprediction. All states are derived from previously decoded plies and fixed-point probabilities.
4. **Independent magic `DMT7`:** profiles 80–97, legacy formats 1–6 kept compatible. Profile80 is a byte-equivalent payload control for DMT6 profile75, except header/profile/CRC.

The 18 new profiles are `r7-control` (80), `r7-intent` (81), `r7-intent-quiet` (82), `r7-intent-strong` (83), `r7-context` (84), `r7-context-quiet` (85), `r7-mix50` (86), `r7-mix75` (87), `r7-adaptive` (88), `r7-adaptive-context` (89), `r7-guard` (90), `r7-context-strong` (91), `r7-calm-guard2` (92), `r7-calm-guard5` (93), `r7-tactical-guard2` (94), `r7-tactical-guard5` (95), `r7-tail-calibrate` (96), `r7-quiet-choice-guard` (97).

## Middlegame loss: honest comparison

All table cells are average **integer-CDF log loss** in bits per middlegame ply. They are not whole-file bpp.

| Profile | Existing human 23 games (893 middle plies) | Prior human 4 games (259 middle plies) | 2024 WCC games 1–13 (579 middle plies) |
|---|---:|---:|---:|
| **DMT6-Fast p75 baseline** | **3.8341** | 4.1601 | 4.1980 |
| DMT7 p80 control | 3.8341 | 4.1601 | 4.1980 |
| DMT7 p82 intention / quiet | 3.8364 (worse) | **4.1553** | **4.1943** |
| DMT7 p83 intention / stronger learning | 3.8391 (worse) | 4.1564 | **4.1924** |
| DMT7 p93 calm-context guard 5% | 3.8317 | 4.1582 | 4.1981 (worse) |
| DMT7 p95 tactical-context guard 5% | 3.8319 | 4.1663 (worse) | 4.2028 (worse) |
| DMT7 p96 tail-rate calibration | 3.8316 | 4.1640 (worse) | 4.2025 (worse) |

The *most optimistic* result on already-used 2024 WCC is 4.1980 → **4.1924** (improvement 0.0056 bits/ply) using p83, while the existing 23-game average regresses by 0.0050. There is no uniformly improving human-data profile. This is **not** independent validation, since all corpora were used during previous model development and this experiment's selection.

### Actual bitstream sizes

| Corpus | DMT6 payload/bytes | p82 payload/bytes | p83 payload/bytes |
|---|---:|---:|---:|
| Human 23 games, 1718 plies | 6567 bits / 858 bytes | 6569 / 859 | 6572 / 859 |
| Human 4 games, 496 plies | 1963 / 265 | 1962 / 265 | 1962 / 265 |
| 2024 WCC 13 games, 1155 plies | 4638 / 608 | 4637 / 608 | 4636 / 608 |
| 48 random-legal games | 23522 bits baseline | 23508 | 23525 |

The WCC 1–2 bit actual payload reductions **do not change the compressed file size (608 bytes)**. Quantization means small CDF log-loss changes are not linearly reflected in whole archive bit counts.

## Why the new model does not generalize

On 2024 WCC middle plies, compare profile75 versus p82 in candidate-level CSV logs:

- 436 non-captures: ~**2.893 log-loss bits saved** in total.
- 143 captures: ~**0.748 bits lost** in total.
- Net: ~2.145 bit log-loss saving across 579 middle plies = 0.0037 bit/ply.

The same p82 feature model on original 23 games loses ~1.88 bits on 647 non-captures and ~0.20 bits on 246 captures. This is a sign that fixed low-dimensional intent indicators compete for probability mass but do **not** reliably identify humans' chosen quiet move across distributions.

Individual move gains/losses vary greatly (+0.36 / -0.43 bits on WCC), which is more significant than the tiny mean benefit. Broad fixed guards can protect random-legal sequences but cause human WCC penalties. The prior DMT6 already encodes many of these indicators; adding a second small residual can be redundant.

## Throughput

All measurements on the shared virtual CPU under `-O3 -DNDEBUG -march=native` (not a dedicated pinned host):

- Repeated WCC13 median warm benchmark: **DMT6 p75 ~56.7K encode / 56.3K decode**; **DMT7 p82 ~59.8K / 59.7K**; **DMT7 p83 ~51.5K / 53.8K** plies/s. Run-to-run noise is material, and the p82 59.8K result is **not a stable 60K pass**.
- Cold single-run measurements varied widely; do not treat a single >60K result as a guarantee.
- Random-legal48 single-run p82: **~44.9K encode / 47.8K decode**, versus baseline ~45.3K / 52.2K. Both are under 60K in this environment.

A cold source-level attack-cache pass gave much lower throughput; the final fast R7 uses already-computed DMG8 candidate relations to avoid those extra queries.

## Safety and regression

- Portable/source project CTest **8/8 pass**, includes DMT1–DMT7 legacy-golden, 98 profiles roundtrip/determinism/CRC mutation tests, transpositions, random8, and attack oracle.
- Profile80's encoded payload after the 6-byte prefix and before its CRC matches p75's byte-for-byte. A golden p82 file is included for the historical 23-game development test.
- `dmt7_matrix`: human23, human4, WCC13 and random48 all profiles 75 + 80–97 encode and losslessly decode successfully.
- **ASan+UBSan 18 new profiles** on human4 corpus: pass. The full 98-profile sanitizer suite was attempted but timed out; do not mark it passed.
- New **independent** diverse human data: not obtained during this iteration. Any progress in the reported reused human corpora is exploratory.

## Next research recommendation

1. Preserve DMT6-Fast p75 as production and all DMT7 IDs as experimental.
2. Study *pairwise residual predicted loss conditional on candidate category* before spending more compute on new positional signals. If a new expert wins only on captures/quiet or one corpus, gate its contribution using **pre-move** estimated reliability rather than fitting to observed answers.
3. Better still, collect thousands of new human games with train/dev/test separation by tournament, player, rating and time control. Do not pick profiles on the final test set; report phase CDF log loss, payload bits, total bytes, speed and bootstrap game-level confidence intervals.
4. Treat target `middle <=3.99` as **aspirational for varied human games**, not established by these 23+4+13 games. Opening <=3.89 and end <=3.89 similarly need independent phase-specific verification.

## Reproduction

See `README-R7.md` in the C++ package. CSV matrices and move-level traces are in `benchmarks-r7/`. Build with CMake; run `dmt7_matrix` and `dmg_mid_speed` using the packaged UCI bundles. Values were obtained with a deterministic codec and no external pretrained models.
