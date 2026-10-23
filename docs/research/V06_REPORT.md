# DMG-Entropy v0.6 | Reproducible results

## Scope, data and metrics

Used the **unchanged 23 pre-existing human game files**, `examples/heldout/*.uci`, 1,718 legal plies. The collection is hand-selected and includes historical famous games and a few online games; **it is not a random sample of modern chess games**. No new external training data or pretrained model was used.

Measured whole compressed binary output, performed exact UCI-roundtrip decoding, and timed encoding and decoding on the same runtime. Payload bits/ply excludes header, trailer and byte alignment; total bytes includes all framing, padding and CRC. Latencies are three timing runs (median) with two traversals each, excluding file I/O, so values will vary with CPU and load.

| model | payload bits/ply | total bytes | whole-file bits/ply | encode million plies/s | decode million plies/s |
|---|---:|---:|---:|---:|---:|
| v0.5 legacy (one file/game) | 4.30035 | 1299 | 6.04889 | 0.315 | 0.301 |
| v0.6 baseline, shared archive | 4.28754 | 958 | 4.46100 | 0.316 | 0.326 |
| v0.6 hierarchical | 4.65076 | 1036 | 4.82421 | 0.311 | 0.307 |
| v0.6 mixture | 4.30792 | 963 | 4.48428 | 0.278 | 0.286 |
| **v0.6 dictionary** | **4.23458** | **947** | **4.40978** | 0.259 | 0.272 |
| v0.6 all experts | 4.25262 | 951 | 4.42841 | 0.227 | 0.220 |

On this particular corpus, the winning dictionary cuts total stored bytes by **27.10%** relative to v0.5, primarily because the fixed 16-byte-per-game headers disappear. Its *payload-only* reduction is **1.53%** relative to v0.5. The new continuous arithmetic stream also amortizes end-of-stream coder overhead: v0.6 archive baseline beats the sum of separately coded v0.5 streams by 22 bits despite using the same candidate weights.

**No model gets near the 3.x bits/ply target on this corpus.** As tested, neither hierarchical nor naive mixture outperforms the dictionary for the human corpus. Improvements of a few dozen bits are sensitive to the small dataset and manually selected parameters.

## Distribution shift: randomized legal chess

Randomly chosen legal moves: 48 games, 3,794 plies (seed 20261020).

| model | payload bits/ply | total bytes |
|---|---:|---:|
| legacy v0.5 | 5.43753 | 3368 |
| shared baseline | 5.42567 | 2636 |
| hierarchical | 5.52267 | 2682 |
| **mixture** | **4.90538** | **2389** |
| dictionary | 5.43200 | 2639 |
| all | 4.91065 | 2391 |

The mixture's uniform expert protects against the badly mismatched tactical heuristic. The dictionary has few repeated exact states in independent randomized games.

## Extreme repetition sensitivity

The *same* 23 human games repeated 8 times, in the same order: 184 games and 13,744 plies. These are duplicated moves, not an independent heldout sample.

| model | payload bits/ply | total bytes |
|---|---:|---:|
| legacy v0.5 | 4.30035 | 10392 |
| shared baseline | 4.28682 | 7572 |
| hierarchical | 4.65068 | 8197 |
| mixture | 4.30741 | 7608 |
| **dictionary** | **1.45700** | **2711** |
| all | 1.48327 | 2756 |

This confirms that shared-state compression can exploit repeated positions and frequent continuation moves. These exceptionally low values **must not** be presented as representative of independent games.

## Implementation notes and ablations

- **Hierarchical:** Current-game 11-category type counts adjust weights within the existing tactical candidate alphabet, with fixed prior. It performed worse on every benchmark here. It is an experiment, not a recommended default.
- **Mixture:** We mix tactical, uniform and hierarchical distributions, using per-game integer posterior updates from decoded moves. Good hedge under random play, slightly worse than tactical alone on the selected human corpus. No extra per-ply mode bits.
- **Dictionary:** Exact state keys include side, castling and en-passant rights. Already-seen successor moves receive extra smoothed weight; data shared across game boundaries, not sent externally. Its physical implementation builds 67-byte state keys and an unordered_map; memory grows with unique positions.
- **All:** Four-model mixture with dictionary as the fourth expert. It was slightly worse than the standalone dictionary in both compression and throughput on repeated and human samples.
- **Container:** Single stream, explicit game lengths using varints, plus CRC32. Per-game random access is lost. The binary alphabet and arithmetic algorithm are deterministic, and each mode is marked in the format header.

## Validation

- `ctest` Release **2/2 passed**, including previous v0.5 test suite and new v0.6 tests.
- `ctest` ASan + UBSan **2/2 passed**.
- Entire human23, random48 and repeated184 corpora round-trip losslessly in *all five v0.6 modes* and original v0.5 baseline. The same input+mode is verified to produce identical bytes on repeated runs.
- Every bitflip across each byte in the short v0.6 fixture is rejected by CRC32. General corruption protection is error detecting but not cryptographically secure.
- Manual command-line round-trip for all 23 UCI games passed token-by-token comparison.

## Decision

For **multiple related human chess games**, select **dictionary + shared archive** as the experimental winner. For **unpredictable/random legal move sources**, the mixture model is the winner in this sample. Keep archive baseline as safe reference; don't enable hierarchical or `all` automatically without new evidence.

Next: larger randomly sampled online PGN corpus, measured on train/tuning vs genuinely held-out tests, and implement a robust mode selector per archive or small block (with measured mode signaling cost). Optimize dictionary context memory and source portability before a Rust translation.
