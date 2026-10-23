# DMT10 destination-calibration experiments (2026-10-21)

Research continuation of DMT9, retaining all earlier DMT1–DMT9 profiles (IDs 0–188) unchanged. **Experimental results, not a production replacement.**

## Overview

- New DMT10 profiles: **189–242**; 54 profiles, 243 total.
- New archive magic version marker: `DMTA` (`DMT` + `A`, format-version byte `1`, profile ID at offset 5), CRC32. DMT1–DMT9 decoders remain supported. The `DMG8/DMG9` archive formats are unrelated and unchanged.
- `r10_target_features`: online destination-square calibration based on file, relative rank, proximity to opponent king, the number of legal origins sharing a destination square, and (in some variants) tactical / previous-ply metadata. All are available to both encoder and decoder before the move; neither pretrained weights nor transmitted side models are required.
- DMT10/228 learns separate geometry residuals for opening and endgame, preserving DMT9/182 for middlegame.
- DMT10/234 combines this phase-specific improvement with a stronger tactical marginal correction in middlegame, integrated after the DMT9/182 prediction.
- DMT10/231 performs milder middlegame correction; on these tested corpora it is a better compromise when random-legal-move data matters.

## Reproduce

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE='-O3 -DNDEBUG -march=native'
cmake --build build -j4
./build/dmt10_tests
./build/dmt9_experiments wcc13 benchmarks-r4/wcc2024_01_13.uci-bundle 119,168,182,228,231,233,234
./build/dmt9_experiments human23 benchmarks-r4/human23.uci-bundle 119,168,182,228,231,233,234
./build/dmt9_experiments human4 benchmarks-r4/human4.uci-bundle 119,168,182,228,231,233,234
./build/dmt9_experiments random48 benchmarks-r2/random48.uci-bundle 119,168,182,228,231,233,234
./build/dmg_mid_speed 234 --bundle benchmarks-r4/wcc2024_01_13.uci-bundle
./build/dmg_mid pack r10-combo-tact4-full /tmp/a.dmt examples/heldout/*.uci
./build/dmg_mid unpack /tmp/a.dmt /tmp/recovered
```

Detailed numerical results are in `DMT10-Research-Results.md` in the parent deliverable and `benchmarks-r10/` here.

## Test status and limitations

- Full CTest original suite: 7 other tests passed; the legacy `dmg_mid_roundtrip` test timed out under the execution limit with the expanded profile count. Do not report this as 9/9 or 8/8 success.
- New `dmt10_tests`: 64 profiles (including all 54 DMT10 experiments) each tested on original human23, 8 fresh pseudorandom legal games, and empty; exact deterministic re-encode, CRC corruption rejection, and **9 original golden archives** checked.
- `dmt9_experiments` completed encode/decode validation across all **243 profile IDs on Human23**.
- Profiling is limited to noisy single-machine medians; no 60–70K plies/s production claim.
- Only 8 extra human games (Fischer-era book selection, first five repeatedly inspected during iteration and final three evaluated at end). These are small and nonrandom. Wider independent generalization is unproven.
- The provided development corpora are reused heavily. Avoid interpreting <0.1 bit/ply changes there as statistical evidence of broad gains.

## Data origins

A separate validation set of 8 historical tournament games was derived as legal UCI moves from publicly readable PGN: <https://github.com/brianerdelyi/ChessPGN> (`My Memorable 60.pgn`). Kept `.uci-bundle` and conversion audit in `benchmarks-r10`; original PGN is omitted from the deliverable. The first five and final three have distinct games. The data are *not* a representative random sample of modern human play.
