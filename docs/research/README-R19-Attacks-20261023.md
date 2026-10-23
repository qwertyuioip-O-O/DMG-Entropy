# R19 attack-union experiment — 2026-10-23

Baseline: the previous R19 direct-output candidate (`r19_inplace`), not the original R19. This study retains the baseline source, model and general pack defaults. The new isolated `r19_attacks` candidate caches the union of occupancy-independent pawn/knight/king attacks once per position.

## Change

The existing `OwnAttackReuse` already cached individual attacks but still traversed every own piece for every legal candidate. This candidate builds prefix/suffix unions for the leapers (pawns, knights and kings). For each move it retrieves the union excluding the moving piece, adds that piece's new attack mask when it remains a leaper, and traverses only the bishops/rooks/queens after the move. The existing occupancy-delta ray checks and piece-identity checks remain for sliders.

Exclusion uses unions, not subtraction/XOR: overlapping attacks must remain present. Captures do not remove other friendly pieces. Castling changes the king's leaper contribution and the rook's slider contribution. Promotion removes the pawn contribution and adds the promoted piece through either the leaper or slider path. En passant changes occupancy and therefore retains the existing slider-ray invalidation.

`tools/generate_r19_attacks.py` derives this isolated candidate from the retained direct-output sources. No learned coefficients, integer quantization operations, observation order or container framing are changed.

## Profiling and correctness

A fresh `-O3 -pg -g` profile of the direct-output baseline, CPU0/new422/mode3, still lists `v8_relations` as the largest sampled self-time function (16.83%); `MidStep` construction is 6.10%. Instrumentation changes generated code and overhead, so these shares are diagnostic and are not production-time estimates. Raw profile and CLI output are in `benchmarks-r19-attacks-20261023/profile/`.

`test_r19_attacks.cpp` compares both the complete attack mask and all relation flags against the full-recomputation path, using special FEN positions, two-ply expansions and 24 seeded random walks (up to 120 plies each). It passed 102,324 candidate comparisons, including 440 castlings, 8 en-passants and 684 promotions. These are comparison counts, not counts of distinct games or positions. Captures, checks, pins and overlapping attacks also arise in these legal-move tests.

A separate `DMT_VERIFY_REUSE` executable checks each cached attack mask against full recomputation during mode3 evaluation on all three datasets, including encode and decode. All three evaluations passed. The release benchmark then checks archive-byte identity for all five modes × all three datasets and runs the existing 1,226-check suite.

## Timing protocol

The protocol is recorded before validation/timing. CPU0 is pinned. Four alternating ABBA/BAAB blocks per dataset give 12 blocks and 48 actual CLI processes. Each CLI call warms up and performs three iterations per direction with validation. Release executables use identical standard `-O3` flags, no profiling/verification instrumentation, and the same original `rmc.o`. No build, test, training or other benchmark runs concurrently with timing.

Paired throughput gain is baseline time divided by candidate time, minus one; direction times are the CLI phase wall durations. Two same-label calls are summed within each block, then ratios are combined geometrically. This short sequential, single-host/single-core study does not establish significance or general hardware performance. Do not multiply gains by the earlier study's gains or compare unmatched medians across sessions.

## Reproduce

```sh
python3 tools/build_r19_inplace.py --out build-r19-inplace
python3 tools/build_r19_attacks.py --out build-r19-attacks
python3 tools/benchmark_r19_attacks.py --baseline build-r19-inplace/r19_inplace --candidate build-r19-attacks/r19_attacks --out fresh-attacks-study
```

For the standalone attack oracle, compile `tools/test_r19_attacks.cpp` with the standard flags, include path and `rmc.o`. For dataset mask verification, compile `tools/r19_attacks.cpp` with the additional `-DDMT_VERIFY_REUSE`, then run `eval BUNDLE builtin 3`. The generator requires the retained direct-output source snapshot; build it first as above.

## Measured outcome

| Dataset | Encode gain vs direct-output baseline | Decode gain | Candidate median encode/decode ply/s |
|---|---:|---:|---:|
| r19 | +5.068% | +5.345% | 51,784 / 51,544 |
| r18 | -0.544% | +7.111% | 47,849 / 52,288 |
| r17 | +6.517% | +10.934% | 46,783 / 47,467 |

Equal-dataset geometric means: encode +3.635%, decode +7.772%. Decode has positive mean gain on all three datasets (9/12 paired blocks), while encode is positive on two (9/12 blocks) and regresses 0.544% on R18 heldout456. The candidate is promising especially for decoding, but fails an all-dataset, both-direction improvement requirement. Retain it as an optional experimental implementation; do not supersede the direct-output baseline or general pack default234. No statistical significance, other-host replication, or 60–70K target claim is made. Timing variation does not establish why the encoder regressed.

All 15 archive-byte comparisons and 1,226 existing checks passed. Full-mask/relation special-position/random-walk checks and three instrumented corpus evaluations passed. Previous baseline and learned-model files match their saved release; source and binary SHA256 values are verified.
