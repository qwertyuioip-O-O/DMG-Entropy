# DMG-Entropy DMT5 — Quiet Positional Move Research

This C++20 project retains the DMT1–DMT4 frozen decoders and adds **DMT5**
profiles 54–67. The main experimental mode is `r5-all-quiet-fast` (profile 61).

The results are research/development measurements, **not independently validated
3.89 bits-per-ply performance**. Read [RESULTS-R5.md](RESULTS-R5.md) first.

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
build/dmg_mid_matrix wcc13 benchmarks-r4/wcc2024_01_13.uci-bundle
build/dmg_mid_speed 61 --bundle benchmarks-r4/human23.uci-bundle
```

Old snapshots are in examples/golden. Binary encoder and decoder share the
same deterministic CDF generation. DMT5 magic is not backward-mislabelled DMT4.
