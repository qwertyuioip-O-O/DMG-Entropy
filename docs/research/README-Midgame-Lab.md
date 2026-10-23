# DMG-Entropy Midgame Lab (DMT1)

Experimental C++20 codec for middle-game move probability. The production baseline remains DMG8-Fast. No external neural weights or opening books are included.

New APIs: `include/dmg_mid.hpp` (`mid_encode`, `mid_decode`, `MidProfile`, `MidDiagnostics`). Implementation lives in `src/entropy.cpp` under `// DMT1`, using the frozen DMG8 predictors and its deterministic CDF rules. All new models operate only in the middlegame according to existing `V7Patterns::phase()`.

17 profiles: baseline; three deterministic integer power temperatures; 5% and 12% uniform probability guards; adaptive mixture of baseline and mild temperature; conditional temperature; greedy bounded legal exchange continuation (SEE, up to 8 captures); new high-value threats; previous same-side move continuity; responses to opponent's most recent move; adaptive online residual training; combined tactics; combined tactics with 5% guard; cheap single-recapture proxy; and combined fast proxy.

Every model has a distinct `DMT1` profile byte. Legacy DMG6, DMG7, DMG8, DMG9, and DMP1 formats remain unchanged. This code experimentally measures differences and does not claim general 3.9-bit/ply performance.

Build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/dmg_mid_bench examples/heldout/*.uci
./build/dmg_mid pack all-tactics-guard result.dmt examples/heldout/*.uci
./build/dmg_mid unpack result.dmt restored/
```

Results and experiment qualification: `RESULTS-Midgame-17-Profiles.md`. Raw benchmark CSV files: `results-mid/`. Golden stream: `examples/golden/human23-mid-all.dmt`. All human-game sets included in this repository are previously used development data, not independent holdout evaluation.
