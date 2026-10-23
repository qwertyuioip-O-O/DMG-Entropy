# DMG-Entropy Phase-R1 — 국면별 압축 연구 (C++20)

**Main remains DMG8-Fast.** This is an experimental phase-aware adaptive codec fork of the v0.9 source. See [`RESULTS-Phase-R1.md`](RESULTS-Phase-R1.md) for actual benchmarks, caveats, and phase definitions.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

## Use

```sh
./build/dmg_phase_bench examples/heldout/*.uci
./build/dmg_phase_bench examples/human/*.uci
./build/dmg_phase_bench --random
./build/dmg_phase pack endgame-only endgame-experiment.dmp examples/heldout/*.uci
./build/dmg_phase pack opening-mixture-endgame hybrid.dmp examples/heldout/*.uci
./build/dmg_phase unpack hybrid.dmp restored_games/
```

Modes: `shared`, `residual-1`, `residual-2`, `residual-4`, `residual-6`, `endgame-only`, `opening-mixture-endgame`.

The new `DMP1` file does not claim compatibility with the frozen `DMG8` decoder, but the original `dmg_v8` CLI and binary format remain available and unchanged. Full PGN text is not supported. All games assumed to start from the standard position; encoder/decoder remain lossless for valid move sequences.

The previous development 23+4 games were **not independent validation**. Phase figures are exact CDF logloss, not literally separated arithmetic payload bits. Current phase-aware model is experimental, not the new default.
