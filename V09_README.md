# DMG-Entropy v0.9 research prototype

This archive retains the full v0.8 code and historical tests, **integrates the original-byte-compatible DMG8-Fast performance patch**, and implements a separate experimental **DMG9 signed relation-delta codec**. No external pretrained weights.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE='-O3 -DNDEBUG'
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Five tests must pass.

## Encode and decode

```bash
# Fast, frozen v8 relation-joint model (recommended for current human development data):
./build/dmg_v8 pack relation-joint human23.dmg8 examples/heldout/*.uci
./build/dmg_v9 unpack human23.dmg8 restored/

# Experimental v9 model with signed 16-feature relation deltas:
./build/dmg_v9 pack delta-opportunity human23.dmg9 examples/heldout/*.uci
./build/dmg_v9 unpack human23.dmg9 restored_v9/

# Highest compression of the implemented choices; much slower encode:
./build/dmg_v9 pack auto best.dmg9 examples/heldout/*.uci
./build/dmg_v9 unpack best.dmg9 restored_best/

# Benchmarks with cold archive resets and matched datasets:
./build/dmg_v9_bench examples/heldout/*.uci
./build/dmg_v9_bench --random
./build/dmg_v9_bench --repeat 3 examples/human/*.uci
```

**Note:** `auto` may emit `DMG6`, `DMG7`, `DMG8`, or `DMG9` magic. This is intended. Its name represents encoder-only archive selection, not always DMG9 bytes. `dmg_v9 unpack` dispatches all of these formats.

## Performance / defaults

- Prefer `dmg_v8 pack relation-joint` for balanced speed/compression on the included development human sample.
- `dmg_v9 pack delta-opportunity` is a new model experiment and does **not** currently beat DMG8-Fast on this sample.
- `dmg_v9 pack auto` saves the most bytes among the candidates but needs multiple full encoding runs.
- `DMG9_INCREMENTAL_ATTACKS` is an optional experimental kernel **OFF by default** because it measured slower on the included human sample. To verify the kernel:

```bash
cmake -S . -B oracle -DCMAKE_BUILD_TYPE=Release \
  -DRMC_VERIFY_ATTACKS=ON -DRMC_VERIFY_LEGAL_FAST=ON -DDMG9_INCREMENTAL_ATTACKS=ON
cmake --build oracle --target dmg_v9_tests dmg_v8_tests -j
ctest --test-dir oracle -R 'dmg_v[89]_' --output-on-failure
```

## Formats and compatibility

`DMG9`: magic `DMG9`, version byte 1, model byte (`0` delta-opportunity, `1` delta-no-pattern), game count (ULEB128), individual game lengths (ULEB128), payload bit length, arithmetic payload, CRC32-IEEE. Standard starting position for each game. Data stream encodes the **moves**, not PGN tags, comments, source text formatting or arbitrary FEN. The legacy DMG6/7/8 decoder routines are preserved.

The new signed feature encoding is a **first R1 prototype**, not the full multi-chunk/profile/index design outlined in `DMG-Entropy-v0.9-Research-Spec.md`. See `RESULTS-v0.9.md` for exact profile assumptions and limitations.

Golden byte streams: `examples/golden/human23-v8-fast.dmg8` and `examples/golden/human23-delta-opportunity.dmg9`.

## Research takeaway

23 famous/historical/online development games are too few and have been repeatedly used in prior development; observed `<4` payload bits/ply does **not** establish robust performance on diverse human chess data. Unlike a new feature claim, the byte-identical DMG8-Fast acceleration is a demonstrated engineering result.
