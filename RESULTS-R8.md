# DMT8: online expert mixture experiment — actual C++20 results

Date: 2026-10-20. Source: DMT6-Fast → DMT7 → new DMT8. This is an **exploratory** experiment, not an independent benchmark. All human corpora have been repeatedly used in preceding versions, so optimizing against them risks selection bias. Each benchmark is a whole archive with model adaptation persisting across games. Per-phase results are mean integer-CDF `-log2(p)` diagnostics, while payload bits come from actual arithmetic coding.

## Headline result

**Fused independent specialists with fixed mixture weights** (profile **119**, `r8-fused-uniform`) performed best on the 2024 WCC development set. This result does not demonstrate that the adaptive *gate* outperforms fixed weights; that claim would be false.

| Corpus | Plies | Model | Actual payload bits | File bytes | Middlegame bpp |
|---|---:|---|---:|---:|---:|
| Human23 (reused) | 1,718 | DMT6-Fast profile75 | 6,567 | 858 | 3.8340590 |
| | | DMT8 profile119 | **6,561** | 858 | **3.8275784** |
| Human4 (reused) | 496 | DMT6-Fast profile75 | **1,963** | 265 | **4.1601377** |
| | | DMT8 profile119 | 1,964 | 265 | 4.1621429 |
| WCC13 (reused) | 1,155 | DMT6-Fast profile75 | 4,638 | 608 | 4.1980439 |
| | | DMT8 profile119 | **4,621** | **606** | **4.1686152** |

For the WCC13 midgame, the model reduces average CDF log loss by **0.0294287 bit/ply** (~0.70%). It still exceeds the user target of **3.99 bit/ply** by **0.1786 bit/ply**. In human4 it slightly regresses.

The light two-specialist alternative **profile 123** (`r8-fused-tactical-plan`) achieved human23 **3.8265395**, human4 **4.1586233**, WCC13 **4.1855185** bit/ply. Although less strong on WCC13, it avoids the regression on human4; that difference is too small to prove generalization.

## What did the adaptation experiment show?

### Untrained ensemble of existing stage experts

For WCC13, the five *existing* stage-level expert distributions have mean log losses:

- Initial relational baseline 4.5464 bit/ply.
- Tactical correction 4.4202.
- Context/rank correction 4.2621.
- Positional correction 4.2228.
- Final DMT6 4.1980.

A fixed equal blend worsened to 4.2749. Global and contextual Bayesian gates were 4.2017 and 4.2033, also worse than DMT6. Adaptive expert switching mostly returned to DMT6; it provided little meaningful gain. This negative result shows that the best final model does not automatically benefit from reusing its weaker intermediate distributions as experts.

### Independently online-trained specialized heads

Each specialist has its own `MidResidual` parameters, updated by selected-minus-expected feature frequency (integer fixed-point), based on previously known human moves only.

- **Tactical**: capture, exchange value, threats, top-ranked choices.
- **Quiet piece-choice**: choosing pawn, minor, rook, queen, king and existing positional quality flags.
- **Plan/continuity**: own-previous-move support/continuation, opponent response, existing positional/rank flags.

New head distributions and final mixed CDFs are deterministic. Adaptive gates weight each expert using probability assigned to the actual preceding move, with forgetting priors. However, **fixed mixture weights usually won** on these reused sets.

On WCC13, a three-head fixed blend quantized at each intermediate step (`r8-special-uniform`, profile 113) achieved **4.1745078**. Fused one-quantization variant (profile 119) achieved **4.1686152**. The latter changes the *new DMT8 experimental probabilities*, not the frozen DMT1–DMT7 files.

## Opening and endgame

For WCC13, profile119 leaves opening **3.9465197** and endgame **3.7054509** bpp unchanged from DMT6. For the prior human23, they remain **3.8617707** and **3.7031774**, respectively. Independent stable targets remain unproven for the opening.

## Speed and random-legal stress

- Same-host WCC13 median benchmark (`dmg_mid_speed`, 7 trials × 8 repeated archives): DMT6 profile75 approximately **59.2K encode / 58.4K decode** plies/s; DMT8 fused profile119 approximately **55.9K encode / 55.0K decode** plies/s. CPU load and frequency cause variance. **60K sustained target not attained for DMT8** in these runs.
- Random-legal 48 games/4,607 plies repeated benchmark: DMT6 profile75 **23,522 payload bits, 3,003 bytes, 5.1493 middlegame bpp, 50.1K/50.8K encode/decode**; profile119 **23,492 bits, 2,999 bytes, 5.1405 middlegame bpp, 47.8K/47.1K**. Profile123 was worse at 23,533 bits. Do not infer human generalization from random legal data.
- WCC13 1,155 plies: DMT6 file 608 bytes, profile119 606 bytes; the gain is small.

## Correctness and compatibility

- Built with C++20, `-O3 -march=native`. Full **CTest 8/8 passed** after adding final profiles.
- `dmg_mid_tests` loops over **all 125 profiles (0–124)** for 23 human games, 8 generated random-legal games, empty/multi-empty archives. For each: encode/decode roundtrip, same-byte re-encode, CRC-corruption rejection. It also checks DMT1–DMT7 frozen golden samples, DMT8 profile98/112 control bytes vs DMT6 profile75, and the new DMT8 profile119 golden archive.
- Sanitizer: **ASan+UBSan smoke passed for profiles 75,98,107,112–121** on the four-game corpus *before* the last two-expert variants were added. A rebuild of the sanitizer test for final profiles 122–124 was stopped by the environment time limit. **Do not call a complete 125-profile sanitizer pass verified.**
- The new stream header is `DMT8`, version 1, profile byte 98–124. Decoding of older profile headers stays isolated.

## Limitations and decision

1. **Do not promote profile119 to production** yet: improved two reused corpora, regressed one reused corpus, missed 60K target, no new human holdout.
2. Online *specialist learning* produced modest gains, but online **adaptive mixture gating** did not convincingly beat a fixed blend.
3. DMT6-Fast profile75 remains the stable comparison/production candidate.
4. Next experiment should test a more distinct set of probability heads on new diverse human PGNs and optimize fused feature updates or the DMT6 shared path to recover ~60–70K plies/s.

Raw CSVs are in `benchmarks-r8/` and reproducible tools in `tools/`.
