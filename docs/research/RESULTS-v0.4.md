# DMG-Entropy — Out-of-sample validation v0.4

**Date:** 2026-10-20  
**Model:** Unchanged C++ v0.3 model. No parameter tuning and no pretrained model.  
**Evaluation:** Entirely new 23-game, 1,718-ply set, excluding the original four model-development games.  
**Code:** C++20; release compilation + ASan/UBSan tests; deterministic byte streams.

## Key result

The frozen `tactical-check` model achieves **4.36321 payload bits/ply** on 23 independently sourced game scores, compared with:

| Encoding mode | Payload bit/ply | Whole-file bit/ply | Encode Mply/s | Decode Mply/s |
|---|---:|---:|---:|---:|
| SMC source/destination index | 6.091 | 7.851 | 5.869 | 6.911 |
| DMG truncated binary | 5.087 | 6.845 | 1.806 | 2.021 |
| Uniform arithmetic | 5.047 | 6.822 | 1.691 | 1.899 |
| v0.2 static structure | 4.859 | 6.636 | 0.838 | 0.870 |
| v0.2 online adaptive | 4.922 | 6.682 | 0.821 | 0.852 |
| Tactical only | 4.616 | 6.398 | 0.740 | 0.774 |
| Check filtering only | 4.517 | 6.286 | 0.669 | 0.714 |
| **v0.3 tactical + check filtering** | **4.363** | **6.128** | **0.621** | **0.624** |
| Best-of-all per game (encode every mode) | **4.338** | **6.095** | 0.084 | 0.639 |

*The throughput measurements exclude file I/O, use five trials with eight repetitions per trial and report the median. Single CPU/host measurements, not portable guaranteed speeds.*

*Whole-file includes the `RMC1` format's 16-byte header **per game**, plus byte padding. The file-level cost is **6.128 bits/ply**, NOT 4.363. The compact payload is best interpreted on long games or when a future block container amortizes headers.*

Compared with uniform arithmetic, the frozen tactical-check mode saves **13.55%** of payload bits; versus the prior static mode, **10.21%**; versus SMC **28.37%**. However it is about **9.5× slower at encoding than SMC** on this small corpus, so the performance goal remains unmet. No 3-bit/ply result was achieved.

## Disjoint corpus and subgroup comparison

| Subgroup | Games | Plies | Uniform | Static | Tactical-check |
|---|---:|---:|---:|---:|---:|
| Historic celebrated games, 1852–1925 | 15 | 921 | 5.103 | 4.882 | **4.406** |
| High-level modern games, 1956–2024 | 5 | 671 | 4.963 | 4.844 | **4.306** |
| Small online amateur sample, 2021 | 3 | 126 | 5.087 | 4.778 | **4.357** |
| **Total** | **23** | **1718** | **5.047** | **4.859** | **4.363** |

All 23 games improve over **uniform arithmetic** under tactical-check; improvements per game range from ~0.295 to ~1.098 bits/ply. The 2021 World Championship Game 6 alone contributes 271 plies and should not be confused with 271 distinct games.

A game-level bootstrap of the 23 selected games gives a **95% resampling interval of 0.586–0.795 bits/ply savings** for uniform versus tactical-check. **This is descriptive within the selected set, not a population confidence interval** because the corpus was not randomly sampled and is heavily enriched with celebrated tactical games.

## Information-theoretic and random control

The 23-game mean `log2(pseudo_legal_move_count)` is **5.0328** bits/ply; mean `log2(legal_move_count)` is **4.6869**; the mean pseudo/legal gap is **0.3459** bits/ply. These are uniform-per-position alphabet statistics, **not** entropy lower bounds for human game distributions.

A separate corpus of 120 games/9,508 plies, uniformly sampled from legal moves, produced:

| Mode | Payload bits/ply |
|---|---:|
| Uniform arithmetic | **5.016** |
| v0.2 static | 5.298 |
| Tactical-check | 5.506 |

The tactical model is **worse** on truly random legal play. This is expected: its fixed, learning-free weights aim to represent human choices, not uniform move selection.

## Verification

- **23/23** games converted from SAN using fully legal move disambiguation and/or independently replayed by the chess engine.
- **253/253** corpus/game/mode combinations (23 games × 11 methods) passed binary compression -> byte stream -> complete UCI move-by-move recovery.
- `tactical-check` compressed output was checked for **byte-for-byte determinism** on all 23 games.
- `ctest` release: passed. `ctest` AddressSanitizer + UndefinedBehaviorSanitizer debug: passed.
- v0.3 wire format / decoder modes unchanged. Original v0.1/v0.2 fixtures remain part of the base test project.

**Important scope limitations:** This does not include a checksum and is not corruption-proof. This codec preserves UCI moves, not complete PGN metadata, comments, ratings or move clocks. SAN test corpus from public sources is manually transcribed; source links are in `examples/heldout/SOURCES.csv`.

## Representativeness warning

This is a more informative check than the previous **four development games**, but **23 games are still not remotely sufficient** to infer performance across real online chess. Fifteen were selected from a 'brilliant/immortal games' collection; five more are elite tournament games; three amateur games come from one shared collection and are not independently sampled. The model was not tuned to these samples, but dataset selection still favors rich tactical play. Long quiet draws, rating strata, opening-heavy games, Chess960 and large-scale random sampling remain untested. Attempts to download larger public PGN collections were blocked in the current runtime.

## Next algorithmic recommendation

1. **Do not add more ad-hoc tactical bonuses based on these 23 games.** Freeze coefficients until a statistically meaningful, held-out and stratified dataset is available.
2. **Reduce coding overhead** using a multi-game container (amortize 16 bytes per game) and optional periodic checkpoints, while preserving losslessness. This affects *file bits per ply*, not payload entropy.
3. **Target richer context modeling** (phase/material, source square threatened, checking move, exchanges, pawn structure), but only benchmark on disjoint development and validation sets. Track speed impact.
4. **Optimize the tactical-check scorer** by caching attacked squares and not re-evaluating duplicate per-move quantities. Current ~0.62Mply/s encode is considerably slower than the initial SMC ~5.87Mply/s.
5. For production-like benchmarking, use >=10,000 randomly sampled human games across Elo/rating bands, time controls and game stages and give payload/whole-file measurements with paired results.

All raw and reproduced data is included in `results/` with `tools/validation/validate_heldout.py`, and the fixed example SAN/UCI scores and provenance are in `examples/heldout/`.
