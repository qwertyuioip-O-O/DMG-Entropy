# DMG-Entropy DMT3 — Opening transpositions and middlegame action-continuity experiments

2026-10-20 · **Experimental development result, not independent universal 3.8-bit performance.**

## Executive assessment

Starting from frozen **DMT2 profile 32** (opening-mid-see-value-rate4), DMT3 adds an online exact-position opening transposition dictionary and six cheap middlegame own-turn continuity signals. DMT1 profiles 0–16, DMT2 profiles 17–32, the original DMG8 decoder, and their golden streams stay unchanged. The new bitstream uses magic **`DMT3`** and profile IDs 33–44.

The best developer-corpus result is **DMT3 profile 42 (`r3-both-strong-rate4`)**, **6,647 payload bits / 1,718 plies = 3.86903 bits/ply**, **868 total bytes = 4.04191 total bits/ply**. It improves the existing DMT2 profile 32 by 84 payload bits and 11 total bytes, but **does not meet the per-stage 3.89 bits/ply goal in the middlegame**. All human game samples previously used by this project remain development sets; one freshly transcribed human game is a smoke test, not a statistically meaningful independent corpus.

## Paired benchmark results

**Human development 23**: 23 games / 1,718 plies; stages: opening 550, middlegame 893, endgame 275. The per-stage numbers below are realized quantized-CDF log losses (not separate arithmetic file segments). Entire payload and file sizes are *actual* encoded streams.

| Profile | Actual payload bpp | Opening CDF bpp | Middle CDF bpp | Endgame CDF bpp | Actual bytes |
|---|---:|---:|---:|---:|---:|
| DMT2 profile 32 (control) | 3.91793 | 3.97709 | 3.94654 | 3.70318 | 879 |
| DMT3 profile 33 (byte-identical probabilities) | 3.91793 | 3.97709 | 3.94654 | 3.70318 | 879 |
| DMT3 34 opening position dictionary | 3.88882 | 3.88677 | 3.94654 | 3.70318 | 873 |
| DMT3 38 middlegame plan, rate4 | 3.90570 | 3.97709 | 3.92346 | 3.70318 | 876 |
| DMT3 39 dictionary + middlegame plan | 3.87718 | 3.88677 | 3.92346 | 3.70318 | 870 |
| DMT3 40 stronger opening dictionary | 3.88126 | 3.86177 | 3.94654 | 3.70318 | 871 |
| **DMT3 42 strong dictionary + plan rate4** | **3.86903** | **3.86177** | **3.92346** | **3.70318** | **868** |
| DMT3 44 aggressive dictionary + plan | 3.86962 | 3.85918 | 3.92587 | 3.70318 | 868 |

The opening and endgame developer **stage** targets (≤3.89) are met by profile 42; the middlegame is not. Profile 42 is the selected *development-corpus* winner among twelve new candidate profiles. The underlying parent project already explored many models, so these estimates have selection bias. Full archive overhead for 23 games is **297 bits**, or ~0.173 bits/ply; the actual file target <3.9 remains unmet.

**Previously used human 4** (4 games / 496 plies, NOT independently held-out):

| Profile | Payload bpp | Opening | Middle | Endgame | Total bytes |
|---|---:|---:|---:|---:|---:|
| DMT2 profile 32 | 4.01411 | 4.23279 | 4.25881 | 3.41392 | 268 |
| DMT3 profile 42 | **4.00403** | **4.19702** | **4.24826** | 3.41392 | 268 |

**Random legal** (48 games / 4,607 plies, deterministic seed and corpus, no claim that this is human):

| Profile | Payload bpp | Opening | Middle | Endgame | Total bytes |
|---|---:|---:|---:|---:|---:|
| DMT2 profile 32 | **5.10310** | **4.96186** | **5.15598** | 4.67087 | **3001** |
| DMT3 profile 42 | 5.11309 | 4.99181 | 5.15916 | 4.67087 | 3007 |

The extra human-preference dictionary **harms uniform-random legal games**, which is expected for a human-specialized model; no universal gain is claimed.

**Order sensitivity**, all on the SAME 23 development games (not extra independent samples):

| Archive input order | DMT2 profile 32 payload bpp | DMT3 profile 42 payload bpp |
|---|---:|---:|
| Filename order | 3.91793 | **3.86903** |
| Reverse | 3.92142 | **3.87253** |
| Shuffle seed 19 | 3.91676 | **3.86903** |
| Shuffle seed 99 | 3.92026 | **3.87369** |

**One previously unused human-game pilot**: Ulf Andersson–Karl Robatsch (Munich 1979) PGN reconstructed from the published annotated mainline (source: https://raw.githubusercontent.com/ValdemarOrn/Chess/master/Annotated%20Games/GM_games.pgn). 65 plies: DMT2 and DMT3 both encode **266 payload bits / 48 bytes (4.09231 payload bpp)**; middlegame log loss changes **4.06573 → 4.05612** and other stages are unchanged. A single manual-transcription sample is an engineering smoke test; **it is not independent validation of compression gains**, and its source has no reliable player-rating/time-control cohort metadata. The isolated single game starts from a cold empty opening dictionary, so that dictionary cannot help until it sees the same opening position again.

## Throughput

Single-thread Release `-O3`, same virtual machine, 7 trials × 8 cold archives, independent median encode/decode (PGN/UCI parsing and disk I/O excluded). Rates may vary in a shared VM.

| Dataset | DMT3 profile 42 encode | DMT3 profile 42 decode |
|---|---:|---:|
| Human developer 23 | **78,610 plies/s** | **80,369 plies/s** |
| Human developer 4 | **89,401 plies/s** | **89,793 plies/s** |
| Random legal 48 | **68,535 plies/s** | **69,412 plies/s** |

All *measured* datasets clear the user-selected 60k/s threshold; arbitrary other chess inputs and CPUs might not.

## Algorithm and strict state synchronization

### Opening transposition dictionary

- At opening stage only, build a **full chess position key** from the 64 squares, side to move, castling rights, and en-passant status. If the en-passant target has no geometrically adjacent pawn for the side to move, normalize that irrelevant EP value to `-1`; if a potential capturer exists, keep the distinct value, even when pinned. This permits transpositions that differ only in immaterial en-passant metadata.
- Record observed **move codes (from/to/promotion)** and counts for previously decoded opening moves at each exact position. This is a shared state across games **within** the same archive; it never uses external opening books. The dictionary is deterministic, full-key compared, bounded to 32,768 positions with insertion-order FIFO eviction; per-position statistics are deterministically aged at count ≥1,000.
- For a historical outgoing count c(m) and total n, the integer unnormalized probability weighting is:

  `raw(m) = base_cdf_freq(m) * ((n+k)*100 - alpha*n) + c(m)*base_cdf_total*alpha`.

  `alpha=75` and `k=2` for profile 42; other profiles test weaker and stronger settings. This smooths unseen legal moves and never assigns them zero mass.
- Update the dictionary **only after** encoding/decoding the true move. Encoder and decoder call identical functions and apply moves in the same sequence.

### Middlegame two-own-turn continuity features

A separate ten-slot integer online residual is used; the additional six active flags are (0) moving another piece supported by the previous own moved piece, (1) landing at a square supported by it, (2) capturing a target it threatened, (3) moving a piece attacked by the opponent's last moved piece, (4) landing on the opponent's vacated source square, and (5) landing adjacent to the previous own destination with a different piece. All geometric attack masks correctly **include a friendly blocker** and treat pawn diagonals as attacked squares rather than pseudo-legal pawn advances. Features remain machine-readable, using the same `MidResidual` online quantized-CDF update routine; rate 4 for profile 42. There is no hidden oracle or per-move free encoder choice.

### Backward compatibility

- Original `DMT1` profiles 0–16 and `DMT2` profiles 17–32 are frozen. Magic `DMT3` plus an explicitly encoded profile ID identifies newly added profiles 33–44. CRC32 is checked before decoding, and a profile ID paired with an incorrect DMT generation is rejected.
- The DMT3 control profile 33 yields identical payload bytes and ideal phase predictions to DMT2 profile 32, apart from its *intentional* magic/profile/CRC change.
- The first 32-bit arithmetic coder, positive probabilities, update order, and game framing are retained. Encoder-only information is never used as if the decoder already knew it.

## Correctness

- `ctest`: **7/7 PASS**, including **all 45** DMT1/DMT2/DMT3 profiles on 23 human developer and 8 random games; exact roundtrip, deterministic double-encoding, CRC errors, original DMT1/DMT2/DMG8 goldens, new DMT3 golden, invalid cross-generation profiles, and a transposed opening-order pair.
- ASan + UBSan `dmg_mid_tests` with `ASAN_OPTIONS=detect_leaks=0`: **PASS**.
- DMT3 profile 42 golden archive: `examples/golden/human23-r3-best.dmt`, **868 bytes**, SHA-256 `289012c7989396abc531fb6181a6b1ce80aeef52c28d44da63c679607fd07f78`.
- Random legal 48 and order-shuffled human archive matrices: actual encoded bytes were decoded to exact UCI move sequences. CSV outputs under `benchmarks-r3/`.
- No cross-platform C++/Rust binary-equivalence test was performed; the serializer uses deterministic integer arithmetic and exact key comparison, but portability tests remain future work.

## Reproduce

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDMT_FAST_TACTICAL=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/dmg_mid_bench examples/heldout/*.uci > human23.csv
./build/dmg_mid_bench examples/human/*.uci > human4.csv
./build/dmg_mid_bench --bundle benchmarks-r2/random48.uci-bundle > random48.csv
./build/dmg_mid_speed 42 examples/heldout/*.uci
./build/dmg_mid_speed 42 examples/human/*.uci
./build/dmg_mid_speed 42 --bundle benchmarks-r2/random48.uci-bundle
./build/dmg_mid pack r3-both-strong-rate4 result.dmt examples/heldout/*.uci
./build/dmg_mid unpack result.dmt decoded/
```

New human PGNs can be processed with `tools/data-preparation/prepare_pgn.py`. The large official Lichess game database is publicly documented at https://database.lichess.org/standard/, but download access was unavailable from this computation container. No large independent human corpus was obtained here.

## Research decision

Keep **DMG8-Fast frozen as production reference**, DMT2 profile 32 as the ablation control, and **DMT3 profile 42 as the new experimental candidate**. The exact opening transposition dictionary contributed most of the observed development-set gain; the middlegame own-turn history contributed a smaller additional improvement. A robust three-stage ≤3.89 claim requires substantial genuinely new human games in different rating and time-control strata. At the present tested speed budget, the next model research should attack **middlegame 3.92346 → ≤3.89** through genuinely novel low-cost conditional evidence, while not sacrificing performance on the new independent corpus.
