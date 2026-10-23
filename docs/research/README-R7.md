# DMG-Entropy DMT7: intention + conditional probability calibration (experimental)

2026-10-20. Source: DMT6-Fast; **old profiles 0–79 and their bitstreams are frozen**.

## Research question

Can low-cost non-capture intention features or context-conditioned probability calibration improve human middlegame coding without pretrained models, and maintain roughly 60K plies/s for both encode/decode?

**Outcome: not yet.** Individual experimental variants improve one reused human corpus but regress on others. The gains are small and speed is not reliably >=60K on the shared host. Do **not** promote to default over DMT6-Fast.

## Exact implementation

- DMT7 new magic `DMT7`, version 1, IDs **80–97** (18 experimental profiles). Legacy DMT1–DMT6 headers remain distinct, and old profile payload bytes are unchanged.
- ID 80 control embeds exactly the DMT6 profile 75 probabilities and on-line updates.
- IDs 81–91: 10 candidate intention bits for quiet development, central/advanced pawn pushes, retreat from an attacked source, defended safe destination, forward penetration, threats from advanced pawns, opponent-vacated squares, consecutive same-piece movement. Signals use DMT6/DMG8's **already computed relation flags** to avoid extra attack scans.
- Alternative online models: unconditional/4-context `MidResidual`, update rates, quiet-only normalization, fixed 50/75% mixtures, integer-only confidence control based on historical selected probability, and small uniform guards.
- IDs 92–97: context-dependent probability guards, including an online tail-rate guard; no new positional features.
- Decoder sees only pre-move board and previously decoded moves; all model updates are deterministic fixed-point arithmetic. No model weights or chess databases are bundled.

## Reproduce

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE='-O3 -DNDEBUG -march=native'
cmake --build build -j4
ctest --test-dir build --output-on-failure
build/dmt7_matrix wcc13 benchmarks-r4/wcc2024_01_13.uci-bundle
build/dmt7_matrix human23 benchmarks-r4/human23.uci-bundle
build/dmt7_matrix human4 benchmarks-r4/human4.uci-bundle
build/dmt7_matrix random48 benchmarks-r2/random48.uci-bundle
build/dmg_mid_speed 75 --bundle benchmarks-r4/wcc2024_01_13.uci-bundle
build/dmg_mid_speed 82 --bundle benchmarks-r4/wcc2024_01_13.uci-bundle
build/dmg_mid pack r7-intent-quiet sample.dmt examples/heldout/*.uci
build/dmg_mid unpack sample.dmt unpacked/
```

`dmt7_matrix` prints full profile-by-profile measurements; see `benchmarks-r7/` and `RESULTS-R7.md` for more detail. `-march=native` is CPU-specific and must not be used to claim portable speed.

## Validation scope

- Full existing CTest 8/8 (includes 98-profile human + random8 roundtrips/determinism/CRC corruption checks and legacy golden files).
- DMT7 control payload compared to DMT6 profile75 byte-for-byte excluding distinct magic/profile/CRC; matches.
- DMT7 profile82 golden archive saved in `examples/golden/human23-r7-intent-quiet.dmt`.
- 4-game ASan+UBSan matrix of all 18 new profiles passes. The **complete 98-profile sanitizer suite was not completed** within the current run.
- 48 random-legal games/profile matrix passes roundtrip and measures tradeoffs; this is not a proxy for real human games.
- All three human corpora were already used in developing previous versions. **No new independent human holdout for DMT7**, no claim of out-of-sample 3.99bpp.
