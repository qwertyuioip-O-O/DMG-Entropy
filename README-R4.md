# DMT4 — context-dependent midgame modeling research

**Experimental, not a proven general-purpose sub-3.9 bits/ply codec.** DMT3 profile 42 remains frozen. No pretrained weights, opening books or external model dependencies.

## New DMT4 profile IDs

- `45 r4-control`: mathematically identical DMT3 profile 42, new `DMT4` magic/profile. Golden stream confirmed identical after 6-byte format prefix and CRC.
- `46 r4-context`: four online context residual models over the same 10 tactical signals (zero capture candidates; 1–2 captures; >=3 captures; currently in check). At every position, context is fully determined from legal moves and the board *before* the actual move.
- `47 r4-rank`: online residual over eight relative-rank flags, ranking cheap position and existing SEE/threat signals *across the actual legal candidate set*; no oracle selection.
- `48 r4-guard`: adaptive uniform guard, based only on previous within-context low-probability observations.
- `49 r4-context-rank`: best human test profile, combines `46 + 47`.
- `50 r4-context-guard`, `51 r4-rank-guard`, `52 r4-all`, `53 r4-all-mild`: ablations / combinations.

For performance, relative rank counts are computed with a bounded integer histogram rather than O(M^2) pairwise candidate comparisons. The optimized output is exactly the same as the original ranking algorithm on the provided WCC13 trace.

### Build / test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure
./build/dmg_mid_matrix human23 benchmarks-r4/human23.uci-bundle
./build/dmg_mid_matrix wcc13 benchmarks-r4/wcc2024_01_13.uci-bundle
./build/dmg_mid_matrix random48 benchmarks-r2/random48.uci-bundle
./build/dmg_mid_trace 42 benchmarks-r4/wcc2024_01_13.uci-bundle base_trace.csv
./build/dmg_mid_trace 49 benchmarks-r4/wcc2024_01_13.uci-bundle new_trace.csv
./build/dmg_mid_speed 49 --bundle benchmarks-r4/wcc2024_01_13.uci-bundle
```

Older DMT1/2/3, DMG6/7/8/9 goldens remain supported. DMT4 requires its own magic; the decoder explicitly rejects invalid model/version combinations.

## Scope and limitations

WCC13 was a previously held-out validation set but **was subsequently examined to formulate this experiment**, and is therefore now development data. The historical 23+4 game sets are also development data. Random move sequences are stress tests, NOT human chess. This project DOES NOT include a fresh large independent sample of human games. No blanket 3.89-bpp claim is supported.

The target 60K plies/s is met by the winning profile on repeated human benchmarks in the current machine, but random legal games sometimes fall below 60K encoding throughput. On the 23 games, payload is 3.8289 bpp but whole-file cost is about 4.00 bpp.

For the full numerical matrix and diagnostic findings, see `RESULTS-R4.md`.
