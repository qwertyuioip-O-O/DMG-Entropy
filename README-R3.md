# DMG-Entropy Phase/Midgame R3 (DMT3 research branch)

New candidate **DMT3** adds archive-local opening transposition history and a small online middlegame action-continuity model to DMT2's existing 10-feature tactical/SEE residual. Existing `DMT1` and `DMT2` files stay decodable unchanged. **This is not a production mainline merge, and the selected model has not been validated on a sufficiently large independent human corpus.**

- New profile IDs **33–44**, magic `DMT3`. Selected development candidate: **42 = `r3-both-strong-rate4`**.
- Developer human23 actual payload: **6,647 bits / 1,718 plies = 3.8690 bits/ply**, 868 bytes total. Opening 3.8618, middlegame 3.9235, endgame 3.7032 mean quantized-CDF log loss. Individual middlegame goal remains unmet.
- 7/7 CTest passed; ASan+UBSan new suite passed. DMT3 golden and all legacy goldens retained.
- Full benchmarks, caveats, performance, algorithms, and commands: **`RESULTS-R3.md`**.
- New one-game published-PGN pilot is in `benchmarks-r3/`; treat it as an engineering sanity check only.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDMT_FAST_TACTICAL=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/dmg_mid_speed 42 examples/heldout/*.uci
./build/dmg_mid_bench examples/heldout/*.uci > comparison.csv
./build/dmg_mid pack r3-both-strong-rate4 new.dmt examples/heldout/*.uci
./build/dmg_mid unpack new.dmt output/
```
