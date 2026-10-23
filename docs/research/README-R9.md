# DMT9 — iterative deterministic lossless-chess model research

**2026-10-21.** Independent experimental extension of the attached DMT8 project.

**Status: exploratory, not a verified general-purpose improvement.** Original DMT8 profile 119 and every earlier DMT1–DMT8 wire format remain byte-for-byte frozen. New experiment profiles 125–188 are encoded under magic `DMT9`, format version `1`, one-byte profile ID, game-length framing, and CRC32. The new decoder also handles earlier DMT files. This version does **not** change the `DMG8` or `DMG9` codecs (names can be confused with the `DMT9` container).

## Iteration sequence

1. **Round 1 — 17 profiles (125–141):** adjust 119's expert blend, fixed priors, update rates, and scoring temperature. A simple tactical+piece mixture (134) beat 119 on the reused three human corpora.
2. **Round 2 — 15 profiles (142–156):** vary update's expected distribution (base vs specialist), and recompose existing candidate tactical signals. Profile 152 (fusing a new tactical feature mask with quiet-piece features) was the strongest aggregate improvement.
3. **Round 3 — 11 profiles (157–167):** add separate, causal, archive-online opening/endgame residuals, using information already available from legal-move candidates. Profile 165 reduced payload size on all three human sets and random-legal data.
4. **Round 4 — 10 profiles (168–177):** phase-specific learning rates and alternate 10-feature masks. Profile 168 (`r9-both-rate6`) performed best on three human sets combined.
5. **Round 5 — 11 profiles (178–188):** retune the midgame fused signal, rates, and temperature while retaining phase rate 6. Profile 182 (`r9-mix-sharp`) yielded the smallest summed human payload on these reused corpora. Profiles 168 and 180 remain useful alternatives.

All experiments use information available from the current pre-move position or past decoded moves; no book weights, engine calls, pretrained files, time leakage, or untransmitted side data. Static integer LUT and exact quantization ensure decoder symmetry. No adaptive on-disk profile switching is performed.

## Headline measurements

| Corpus | Plies | DMT6/75 bits | DMT8/119 bits | DMT9/168 bits | DMT9/182 bits | 119 bytes | 182 bytes |
|---|---:|---:|---:|---:|---:|---:|---:|
| Human23 | 1718 | 6567 | 6561 | 6526 | 6530 | 858 | 854 |
| Human4 | 496 | 1963 | 1964 | 1958 | 1958 | 265 | 264 |
| WCC13 | 1155 | 4638 | 4621 | 4592 | 4584 | 606 | 601 |
| Random48 (nonhuman control) | 4607 | 23522 | 23492 | 23467 | 23472 | 2999 | 2996 |

Human datasets total: 119 **13,146 bits**, 168 **13,076 bits**, 182 **13,072 bits** (-74 bits, ~0.56% relative to 119). This is **actual arithmetic-coded payload**, not hypothetical entropy alone.

WCC13 `-log2` of integer CDF chosen probabilities, in bit/ply:

| Phase | 119 | 168 | 182 |
|---|---:|---:|---:|
| Opening | 3.9465197 | 3.9338086 | 3.9338086 |
| Middlegame | 4.1686152 | 4.1509542 | 4.1371673 |
| Endgame | 3.7054509 | 3.6482194 | 3.6482194 |

On Human23, 168 beats 182, and on Random48, 168 also beats 182. Do not assume 182 dominates all corpora. Reversed / shuffled **the same** human games favored 182 in total, but these are *not independent held-out chess games*.

### Speed

WCC13 repeated speed utility, median of **three complete invocations**, each performing 7 trials × 8 repeated archives, same host:

| Profile | Encode plies/s | Decode plies/s |
|---|---:|---:|
| DMT6/75 | 61,783 | 62,454 |
| DMT8/119 | 57,902 | 59,802 |
| DMT9/168 | 57,958 | 58,833 |
| DMT9/182 | 58,990 | 57,841 |

These are noisy performance observations, not a statistical CPU benchmark; **60–70K sustained speed target is not established for new candidates**. The user's target of opening/endgame <= 3.8 / middlegame approximately 3.9 across diverse human PGNs is **not met** (notably WCC13 middlegame 4.137).

### Tests

- `ctest --test-dir build --output-on-failure` **8/8 PASS** in this environment, including original roundtrip and DMT1–DMT8 golden archives.
- `dmg_mid_tests` loops all **189 profiles** through archive roundtrip, deterministic exact re-encoding, CRC corruption detection, human23, random8, and empty archives; DMT9 added a new golden stream and invalid cross-version ID rejection.
- Human23, Human4, WCC13, Random48 profile matrices all completed actual `mid_encode`→`mid_decode` equality with roundtrip `PASS`.
- `benchmarks-r9/reordered_corpora.csv` reports order sensitivity using original games reversed and shuffled with fixed seed 11/67. These do **not** expand the game population.
- Full independent human corpus, multi-CPU determinism, and full sanitizer validation of new profiles **not completed**.

### Reproduce

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE='-O3 -DNDEBUG -march=native'
cmake --build build -j4
ctest --test-dir build --output-on-failure
build/dmt9_experiments wcc13 benchmarks-r4/wcc2024_01_13.uci-bundle 75,119,168,182
build/dmt9_experiments human23 benchmarks-r4/human23.uci-bundle 75,119,168,182
build/dmt9_experiments human4 benchmarks-r4/human4.uci-bundle 75,119,168,182
build/dmt9_experiments random48 benchmarks-r2/random48.uci-bundle 75,119,168,182
build/dmg_mid_speed 182 --bundle benchmarks-r4/wcc2024_01_13.uci-bundle
build/dmg_mid pack r9-mix-sharp /tmp/sample.dmt examples/heldout/*.uci
build/dmg_mid unpack /tmp/sample.dmt /tmp/dmt9_recovered
```

Recommendation: **keep DMT8/119 and DMT6/75 intact**. Treat 182 as a best-on-these-human-corpora research contender and 168 as a slightly more conservative alternative. A genuinely new, non-overlapping diverse human test set and CPU performance profiling are required before production adoption. Further microscopic retuning of these same three corpora risks selection bias. A new architecture or fresh data is more credible for closing the remaining ~0.24 bit/ply WCC midgame gap.
