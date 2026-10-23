# DMT6: Quiet queen and pawn predictors

This C++20 research branch extends DMT5 and freezes DMT1–DMT5 streams.
Profiles **68–79** are the DMT6 research profiles. The control is 68, queen only 69,
pawn only 70 and the best measured development-corpus profile is 75.

**No broad independent proof of 3.89 bit/ply or >=60K plies/s:**
Read [RESULTS-R6.md](RESULTS-R6.md) and the saved `benchmarks-r6` outputs.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
build/dmg_mid_matrix_r6 wcc13 benchmarks-r4/wcc2024_01_13.uci-bundle
build/dmg_mid_speed 75 --bundle benchmarks-r4/wcc2024_01_13.uci-bundle
```

All earlier profile IDs are retained. DMT6 uses its own version magic.
The data is the same development material reused in earlier research; the archive
contains no private pretraining, opening books or neural model weights.
