# DMG-Entropy v0.6 — compression research prototype

C++20 reproducible implementations of four independently measurable changes to v0.5:

1. **Hierarchical**: online per-game category frequencies, conditioned on category and then on within-category tactical move weights.
2. **Mixture**: an integer, causal Bayesian-style weighted ensemble of tactical, uniform, and hierarchical candidate distributions. All weights evolve from previously decoded moves only.
3. **Dictionary**: transposition-aware exact chess state key (64 signed-square contents, side to move, castling flags, en passant square) with previous-move frequency counts accumulated across games. Existing v0.5 tactical distribution is smoothed with dictionary counts.
4. **Shared archive**: single arithmetic-coded bitstream across games, varint per-game ply counts, common 6-byte magic/version/mode prefix and CRC32. No 16-byte header per game.

`all` combines mixture and dictionary (four experts), measured separately. `baseline` uses the old v0.5 check-signal-light tactical model with the **new archive**, to isolate gains from framing versus modeling.

## Building and testing

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/dmg_v6_bench examples/heldout/*.uci
./build/dmg_v6_bench --random
./build/dmg_v6_bench --repeat 8 examples/heldout/*.uci
```

To actually use the new codec:

```bash
./build/dmg_v6 pack dictionary archive.dmg6 examples/heldout/*.uci
./build/dmg_v6 unpack archive.dmg6 restored_uci/
```

Permitted `pack` modes: `baseline`, `hierarchical`, `mixture`, `dictionary`, `all`.

**Format** `DMG6` byte magic, `0x01` version, one mode byte, varuint game count, varuint each game length (plies), varuint encoded bit count, continuous arithmetic payload, CRC32 of preceding bytes. As in previous versions, start position is standard chess, files encode only UCI move sequences (not PGN tags, comments, or annotations). The archive stores game order and lengths, but not original input names. No independent random access to later games; decoding requires earlier games to update the dictionary. There is no external pretrained model.

## Findings

The combined model was *not* best. See `V06_REPORT.md` and `results-v0.6/`. 23 historical/modern/online selected games (1,718 plies) are a small nonrepresentative dataset. Repeating these games eightfold is a **synthetic extreme redundancy test**, not a realistic dataset estimate. A randomized legal-move generator exposes distribution shift. Comparison timings are sample-specific.

A 32-bit arithmetic coder is used with per-step totals less than 32768; integer deterministic probabilities and positional exact-byte dictionary keys avoid floating-point synchronization or hash-only key collisions. The container has CRC32 for accidental corruption only, **not a cryptographic authenticator**.

## Notes

- No Rust port yet; this is a validated C++ experiment.
- v0.5's `rmc` CLI and old format are kept intact, with the new bundle CLI `dmg_v6` alongside.
- Causally updating the dictionary from already decoded moves preserves exact decoding without any side information. It works best for collections of overlapping or repeated games.
- `all` costs more CPU and may increase compressed bytes compared to dictionary alone; choosing experts is an empirical design question.
