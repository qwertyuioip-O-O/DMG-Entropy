# Goal10: exact paged opportunity lookup — 2026-10-23

The rank/mobility candidate's encoder gain did not reproduce on CPU1, so it is not included here. This candidate starts directly from the same fixed goal anchor: accepted LTO + audited scratch initialization, SHA256 `7c466d55510493aa2366aa310f50583083a161f88304f64e2941089ca9333918`. The +10% encoder and decoder goal is unchanged; no baseline reset or compounding of prior percentages occurs.

## Data structure change

`V7Patterns::geom_op` and `square_op` previously used `unordered_map<uint32_t,Opportunity>`. Their integer keys are packed from bounded phase, piece kind, source/destination geometry, capture and promotion fields. Square keys are below 3×8×64×64×2×8 = 1,572,864, and geometry keys are smaller. The paged table allows keys below 2^21 and rejects out-of-range keys.

`Goal10OpTable` uses an initially empty page directory, allocated on first insertion. Each page holds 256 eight-byte opportunity values plus 256 presence bits. Lookup splits the key into page index and cell offset with shifts/masks, avoiding hash bucket division and chained-node traversal. Pages are allocated only when touched. The maximal fully populated page storage is approximately 16.25 MiB plus a 64 KiB directory per table; two tables could reach approximately 32.5 MiB plus directories. This is a memory/performance tradeoff, not a universal improvement for tiny streams.

Presence bits preserve the distinction between an absent key and an inserted zero value. Pages never relocate, so pointers cached for observe remain stable across insertions. Copy/assignment deep-copy allocated pages, while move preserves ownership. No table iteration order is used by the codec. All original count updates, saturation, priors, quantization, candidate order and archive framing remain unchanged. Other map types are not replaced.

## Validation

`tools/native/test_goal10_pages.cpp` differentially compares the paged container against `unordered_map` over 100,000 seeded random insert/read/update operations across the full key range. It passed 297,620 checks including zero-value presence, stable pointers, deep copy, assignment, move and upper-bound rejection.

Release validation checks byte-identical archives against the fixed anchor for all five modes × all three datasets, with each pack's roundtrip, and runs the existing 1,226-check suite. The generator changes only the two opportunity-table types and adds the table header; model/source snapshots remain preserved.

## Timing

CPU0 screening uses four alternating ABBA/BAAB blocks per dataset: 48 actual CLI processes, 12 blocks per direction. Phase wall times come from the ordinary speed command's warm-up and three iterations per direction. Ratios sum two same-label durations within each block and combine geometrically, with equal dataset weighting. A promising candidate is then measured against the same anchor in a separate CPU1 paired confirmation using `confirm_goal10.py`. That confirmation, rather than multiplied screening gains, supplies the reported cumulative progress.

No builds, tests, training or other benchmarks run concurrently with timing. Raw data, hashes and all unfavorable results are retained. These are exploratory fixed-host/core/data measurements without statistical significance or universal performance claims. Both directions must reach +10% against the fixed anchor to mark the goal achieved; small partial gains can be retained for later accumulation, with regressions disclosed.

## Reproduce

```sh
python3 tools/build/build_goal10_pages.py --out build-goal10-pages
python3 tools/benchmarking/benchmark_goal10_pages.py --baseline benchmarks-goal10-20261023/anchor/r19_scratch --candidate build-goal10-pages/g10pages --out fresh-pages-screen
python3 tools/benchmarking/confirm_goal10.py --baseline benchmarks-goal10-20261023/anchor/r19_scratch --candidate build-goal10-pages/g10pages --cpu 1 --out fresh-pages-confirm
```

Build `tools/native/test_goal10_pages.cpp` separately with C++20 to reproduce the container oracle. Both codec executables retain the same `-O3 -flto=1` flags with no PGO, fast-math or host-specific ISA. The general pack profile234 remains separate from the research speed build.

## CPU0 screening

| Dataset | Encode gain vs anchor | Decode gain vs anchor |
|---|---:|---:|
| r19 | +1.415% | +5.911% |
| r18 | +7.026% | +3.892% |
| r17 | +1.019% | +1.048% |

Equal-dataset aggregates: encode +3.117%, decode +3.598%. All three dataset means improve in both directions, with 9/12 encoding and 10/12 decoding blocks positive. Separate CPU1 confirmation follows; this is not yet a cumulative-goal claim.

## CPU1 confirmation

| Dataset | Encode gain vs fixed anchor | Decode gain vs fixed anchor |
|---|---:|---:|
| r19 | +6.384% | +4.278% |
| r18 | +8.547% | +4.433% |
| r17 | +5.845% | +1.792% |

Confirmed aggregate encode +6.919%, decode +3.494%, whole-process CPU +4.893%. All three dataset means improve in both directions; 11/12 encoding and 10/12 decoding blocks are positive. Retain this as the first confirmed cumulative step. Both +10% thresholds are not yet reached. This is same-host/core confirmation, not independent-hardware or statistical-significance proof.
