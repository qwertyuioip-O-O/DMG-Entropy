# DMG-Entropy DMT1 speed recovery experiment — October 20, 2026

## Outcome

**Byte-exact acceleration achieved; the 97K plies/s threshold has not been reliably recovered for the full 8-feature model.** This does not improve compression ratios. The DMG8-Fast model remains the recommended stable mainline; DMT1 is still experimental.

### Developer human corpus (23 games / 1718 plies)

| Metric | Frozen DMG8-Fast | DMT1 all-tactics-guard — original code | DMT1 all-tactics-guard — fast code |
|---|---:|---:|---:|
| Payload bits | 6857 | 6768 | **6768** |
| Total file bytes | 895 | 883 | **883** |
| Payload bits per ply | 3.9913 | 3.9395 | **3.9395** |
| Middlegame CDF log loss (893 plies) | 4.0691 | 3.969415 | **3.969415** |

Other phase CDF log losses are unaffected by DMT1, since its adjustment is applied only in the middlegame.

### CPU speed

Single-thread, C++20, `-O3` Release, same shared VM, measured without reading input files. The first speed comparison used 7 trials with 8 cold-state encode/decode repetitions each; trials reported at their median. In one paired trial on the 23-game corpus:

| Profile | Old encoder plies/s | Fast encoder plies/s | Relative change |
|---|---:|---:|---:|
| SEE only | 92,032 | 96,574 | +4.9% |
| **All tactics + 5% guard** | **76,297** | **84,968** | **+11.4%** |

A separate one-pass 17-profile matrix gave all-tactics-guard **82,279 → 87,745 plies/s (+6.6%)**. Variations reflect shared VM contention, short workloads, cache/clock differences. CPU affinity was not locked and confidence intervals are not estimated. The baseline mode itself varied substantially; these are **engineering measurements rather than strong throughput guarantees**. No claim of a sustained 97K plies/s for the full DMT1 model.

### Same-file and safety checks

- Built with `-DDMT_VERIFY_FAST=ON`, compared every fast reverse-attacker and destination-threat query with the original C++ oracle while running the existing suite: all **7/7 CTest** passed.
- The 23-game **golden** output matches byte for byte: SHA256 `ab0d07509453a1cc7a175777410365ed0120393d41216ca19fb4be396a643128` (883 bytes).
- Across **all 17** model profiles, payload bit counts, full byte lengths, and per-phase log loss were identical in paired 23-game benchmarks (51 phase rows); the golden test and identical SHA additionally confirm actual byte equality for the recommended profile.
- 48 new deterministic *random-legal* games, **4607 plies**, seeds and generator in `tools/random_corpus.cpp`: original and fast archives byte-identical for SEE, all-tactics-guard, all-fast. They are stress tests, **not human compression evaluation**.
- ASan/UBSan `dmg_mid_tests` passed with `ASAN_OPTIONS=detect_leaks=0` and verification enabled.
- Old DMT1 encoder/decoder remains readable; old DMG8 streams and other legacy format decoders are unchanged.

### Independent human dataset: outstanding

The official Lichess public-rated-game PGN archives were located at https://database.lichess.org/standard/ , but this execution environment could not download the archive. **There is no independent human-game generalization measurement in this run.** The provided `tools/prepare_pgn.py` enables a reproducible future PGN→standard/legal UCI dataset conversion, with source SHA256 and filter audit. The current 23 historical/modern games and the other 4 previously used human games are **development samples** and must not be relabeled as held-out.

## Decision

- Retain DMG8-Fast as stable production baseline.
- Retain DMT1 optimized tactical experimental branch because it improves middlegame log loss but remains slower than the desired ~97K plies/s and short of the ≤3.89 bpp middlegame target.
- Next critical result is genuinely fresh Elo/time-control-stratified human PGN, evaluated as cold archives by opening/middle/endgame; only then decide whether tactical model merits promotion.
