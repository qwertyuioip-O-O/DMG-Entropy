# DMG-Entropy Phase/Mid R2 — measured experiments (2026-10-20)

## Executive result

A DMT2 experimental codec extends the exact byte-compatible accelerated DMT1 code with **8 opening choice features**, different middlegame residual-learning rates, and **2 exchange-magnitude (SEE) features**. We ran 16 new codec profiles (IDs 17–32) in addition to 17 pre-existing DMT1 profiles (33 total). The strongest candidate is **`opening-mid-see-value-rate4` (profile 32)**. It does **not** meet the independent-human-corpus 3.89 bits/ply goal: the corpus here has been used for development, and independent human games were not acquired.

### What was measured

- Human developer 23: 23 games, 1,718 plies (opening 550, middlegame 893, endgame 275), sorted file names. All three phase boundaries are computed from live game context, **not** from independently encoded FEN fragments.
- Human developer 4: 4 previously used games, 496 plies; *not* independent evaluation.
- Random legal stress: 48 deterministic randomly selected legal-move games, 4,607 plies, seed 20261021, generated in `tools/native/random_corpus.cpp`. This tests robustness, **not human generalization**.
- Three archive-order changes on the original 23 games. The codec's online state learns across games, so order can matter.
- Every row in the CSV is an actual encoded file, decoded with the matching profile, and checked for exact move-sequence equality.

| Dataset | Model | Payload bpp | Opening CDF | Middle CDF | End CDF | File bytes |
|---|---|---:|---:|---:|---:|---:|
| human23 | baseline | 3.9913 | 4.0076 | 4.0691 | 3.7032 | 895 |
| human23 | all-tactics-guard | 3.9395 | 4.0076 | 3.9694 | 3.7032 | 883 |
| human23 | opening-mid-guard-rate4 | 3.9278 | 3.9771 | 3.9656 | 3.7032 | 881 |
| human23 | mid-see-value | 3.9307 | 4.0076 | 3.9531 | 3.7032 | 882 |
| human23 | opening-mid-see-value-rate4 | 3.9179 | 3.9771 | 3.9465 | 3.7032 | 879 |
| human4 | baseline | 4.0625 | 4.2465 | 4.3430 | 3.4139 | 271 |
| human4 | all-tactics-guard | 4.0323 | 4.2465 | 4.2829 | 3.4139 | 269 |
| human4 | opening-mid-guard-rate4 | 4.0181 | 4.2328 | 4.2619 | 3.4139 | 269 |
| human4 | mid-see-value | 4.0302 | 4.2465 | 4.2814 | 3.4139 | 269 |
| human4 | opening-mid-see-value-rate4 | 4.0141 | 4.2328 | 4.2588 | 3.4139 | 268 |
| random48 | baseline | 5.1480 | 4.9981 | 5.2044 | 4.6709 | 3027 |
| random48 | all-tactics-guard | 5.1100 | 4.9981 | 5.1530 | 4.6709 | 3005 |
| random48 | opening-mid-guard-rate4 | 5.1033 | 4.9619 | 5.1563 | 4.6709 | 3001 |
| random48 | mid-see-value | 5.1098 | 4.9981 | 5.1528 | 4.6709 | 3005 |
| random48 | opening-mid-see-value-rate4 | 5.1031 | 4.9619 | 5.1560 | 4.6709 | 3001 |


**Developer human23**: DMG8 baseline 6857 payload bits / 895 bytes, DMT1 all-tactics-guard 6768 bits / 883 bytes, and DMT2 profile 32 **6731 bits / 879 bytes**. Relative to baseline, DMT2 saves 126 actual payload bits and 16 total bytes. Best total storage is **4.0931 bits/ply**, higher than the payload 3.9179. Relative to DMT1, savings are 37 bits and 4 bytes.

Phase performance of best candidate in developer human23: opening 3.9771; middlegame 3.9465; endgame 3.7032. **None of these overall benchmark comparisons establish future 3.89 bpp generalization**, and opening and middlegame do not meet the requested target even here.

### Order sensitivity

| Archive order | Baseline | DMT1 all-tactics-guard | DMT2 best |
|---|---:|---:|---:|
| human23 | 3.99127 | 3.93946 | 3.91793 |
| reverse | 3.99476 | 3.94237 | 3.92142 |
| shuffle19 | 3.99127 | 3.93830 | 3.91676 |
| shuffle99 | 3.99010 | 3.93714 | 3.91502 |


This is a consistency check over the *same selected games*, not an independent confidence interval.

## Codec implementation

Source: `src/entropy.cpp`, API in `include/dmg_mid.hpp`, runner `tools/native/mid_cli.cpp`, benchmarks in `tools/native/bench_mid.cpp` and `tools/native/mid_speed.cpp`.

- DMT1 profiles 0–16 retain the original magic `DMT1`, original CDF/learning/packing semantics, and original golden bytes. DMG8/DMG7/DMG6 and previous-format decoders are unchanged.
- DMT2 profiles 17–32 use magic `DMT2` with a byte encoding the model profile; unknown version or profile is rejected. The arithmetic coder, game framing and CRC32 remain as in DMT1, but probability or learned state differs.
- Middlegame optimization candidates include uniform escape/guard rates of 2%, 5%, 10%, and alternative fixed or exposure-adaptive residual learning rates. These are online model options, never encoder-only free-choice per move.
- Opening models include 8 binary features: minor-piece first development; knight development; central two-square pawn pushes; d/e pawn advance; castling; central pawn captures; minor central landing; repeat move of same piece as previous own turn. Shared online weights start at zero and learn **only from already decoded moves**, without opening books.
- New SEE magnitude features mark exchange gains >=300 centipawns (relative integer material scale) and losses <=-300. They reuse the already computed bounded legal-capture exchange continuation, so adding features does not introduce another full board search per candidate.
- Profile 32 applies the 8 opening features and 5% guard in the opening, and the original 8 tactical features + 2 SEE magnitude flags, 5% guard and residual update rate 4 in the middlegame. Existing endgame path is unchanged.
- All candidates continue to receive positive probability; no move is discarded by model preference. The encoder and decoder compute identical fixed-point CDFs and online updates.

This is a **developer-corpus model search**, and the selected profile may be optimistic because 33 models were compared on small repeated data.

## Throughput (cold archives, single-thread Release -O3)

The benchmark runner `tools/native/mid_speed.cpp` performs 7 trials, 8 repeated cold-state encodes and decodes per trial, returns the median separately for encoding and decoding. Text input parsing and file I/O are excluded. CPU shown in this runtime was an AMD EPYC 9V74 shared virtual machine; CPU affinity was not pinned.

| Dataset | Model | Encode plies/s | Decode plies/s |
|---|---|---:|---:|
| human23 | DMT1 all-tactics-guard | 90,788 | 91,315 |
| human23 | DMT2 opening-mid-guard-rate4 | 86,098 | 85,229 |
| human23 | **DMT2 profile 32** | **80,331** | **83,637** |
| human4 | DMT2 profile 32 | 89,461 | 90,140 |
| random48 | DMT2 profile 32 | 69,983 | 70,086 |

**These measurements clear the revised 60K/second design threshold on tested corpora.** They do not guarantee a universal speed floor on arbitrary games or hardware. There is room for further compression work at the expense of speed, but more tuning without fresh humans is not justified.

## Correctness and format validation

- `ctest` 7/7 passed, including golden DMG8 and original DMT1, all **33 DMT1/DMT2 profiles** on 23 human games and deterministic random legal games, byte-identical double encoding, exact move reconstruction, CRC corruption rejection and explicit invalid DMT2 profile rejection.
- ASan+UBSan-instrumented `dmg_mid_tests` passed with `ASAN_OPTIONS=detect_leaks=0`.
- Golden DMT2: `examples/golden/human23-r2-best.dmt`, 879 bytes, SHA256 `f3f50dedcd581b2219e87dcbe99fad9f147e0c29d7a2e955951b95e8e8dab551`. Existing DMT1 golden SHA256 `ab0d07509453a1cc7a175777410365ed0120393d41216ca19fb4be396a643128` remains unchanged.
- All 48 random-legal games passed per-profile compression and roundtrip checks in the full `bench_mid` matrix.

## Reproduce

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDMT_FAST_TACTICAL=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/dmg_mid_bench examples/heldout/*.uci > human23.csv
./build/dmg_mid_bench examples/human/*.uci > human4.csv
./build/dmg_mid_speed 32 examples/heldout/*.uci
./build/dmg_mid pack opening-mid-see-value-rate4 out.dmt examples/heldout/*.uci
./build/dmg_mid unpack out.dmt decoded/
./build/dmg_random_corpus 48 96 20261021 > random.uci-bundle
./build/dmg_mid_speed 32 --bundle random.uci-bundle
./build/dmg_mid_bench --bundle random.uci-bundle > random-all-phases.csv
```

Full profiling matrices and deterministic random UCI bundle are included under `benchmarks-r2/`. Do not use the old 23 and 4 human games as an untouched test set; obtain new standard human PGNs and convert them with `tools/data-preparation/prepare_pgn.py`. Neither the historical games nor random games substitute for genuinely new human testing.

## Decision

**Keep DMG8-Fast as frozen production mainline; DMT2 profile 32 is the most promising experimental branch under the new 60K/s budget.** A promotion requires genuinely new human games, separately reported opening/middle/endgame costs, stable compression improvement across cohorts, deterministic byte-equivalent roundtrips, and acceptable throughput. The present result is a measured development-corpus improvement, not proof of universal 3.8x in all three phases.
