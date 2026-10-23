# Portable sliding lookup and constant piece tables — 2026-10-23

The new research default is g10static. The fixed original scratch+LTO anchor is unchanged. Goal completion below refers to observed aggregate point estimates on this host, not a confidence bound or a promise for every dataset/CPU.

## Selected change

- Replace square-by-square bishop/rook/queen sliding generation with collision-checked unsigned multiply/shift table lookup. No PEXT, BMI, native target, fast-math or optional ISA is required. The immutable attack payload adds 861,184 bytes (841 KiB), plus descriptors.
- Compute knight/king masks at compile time. The two corresponding runtime initialization guard symbols disappear.
- Retain the current index probability model, its coefficients, learning semantics and archive framing. Original src/rmc.cpp remains intact; the selected research builder compiles generated src/rmc_g10static.cpp.

## Results

| Study | Encode vs fixed anchor | Decode vs fixed anchor | Quality |
|---|---:|---:|---|
| Single-call screen, CPU0, 54 calls | +13.705804% | +9.159030% | All 18 phase groups pass max/min <=1.25 |
| Fresh confirmation, CPU1, 96 native calls | +10.938466% | +10.146912% | All 12 two-call-mean phase groups pass <=1.25 |

Direct same-screen gain versus previous g10index: encode +2.674355%, decode +2.904227%. These are not compounded with historical gains. Whole-process CPU throughput gain in confirmation: +10.633136%.

| Confirmation dataset | Encode | Decode |
|---|---:|---:|
| r19 | +6.336266% | +7.040395% |
| r18 | +14.839796% | +11.011971% |
| r17 | +11.807941% | +12.460359% |

The original goal is an equal-dataset geometric mean, not >=10% on every individual dataset. Both encode and decode are faster in all 12 confirmation blocks; R19 alone remains below 10%. Decode aggregate clears the 10% point target by only about 0.147 percentage points. This is a narrow observed margin.

## Measurement protocol and limits

Screening uses 54 original native CLI invocations and the existing predeclared 1.25 repeat-range gate. The selected screen passes. Earlier variable trials fail and remain archived.
Before fresh confirmation, a longer measurement unit was declared: arithmetic mean of exactly two consecutive original CLI speed calls, each retaining its unchanged three timed encode/decode iterations, warmup and roundtrip/byte checks. ABBA/BAAB remains four blocks per dataset on another core. Thus 48 units contain all 96 native calls. The objective remains phase wall-time throughput against the same frozen anchor.
The 1.25 gate applies prospectively to those longer units. All 12 unit groups pass. Separately auditing individual native calls still fails the short-window range gate; averaging does not make that raw variation disappear. Both native and unit CSVs are preserved. No outliers are trimmed. audit_goal10_batches.py independently reconstructs all 48 means and all paired aggregate gains directly from all native rows.
No confidence interval establishes a lower bound above 10%. Do not reinterpret this point estimate as a universal or robust >=10% guarantee. Other physical CPU hardware was not available for testing.

## Correctness

- 15 dataset/mode combinations are byte-identical across original anchor, previous index and selected candidate; existing 1,226 checks pass.
- Magic lookup is exhaustively checked over every relevant occupancy subset, plus empty/full/single-blocker and random full-board masks: 1,189,760 exact comparisons.
- General core cases, promotions, en passant, castling, invalid inputs, and 5,983 synthetic legal plies pass.
- Attack oracle passes 17,967 positions and 2,299,776 square/side queries.
- Original model/core source and selected source/binary hashes are preserved with each study.

## Other attempts in this iteration

| Candidate | Encode vs previous index | Decode vs previous index | Single-call gate |
|---|---:|---:|---|
| prefix | +0.0515% | -3.8215% | FAIL |
| equal | +0.1019% | -0.8122% | FAIL |
| guard | +1.3449% | +1.3597% | FAIL |
| ratio-recheck | -0.7980% | -2.8154% | FAIL |
| fused | -1.9897% | -4.0745% | FAIL |
| hoist | -3.0600% | -2.6112% | FAIL |
| read | -1.8987% | -4.8836% | FAIL |
| rays | -1.1788% | +0.6142% | FAIL |
| magic | +0.9942% | +2.6953% | FAIL |
| static | +2.6744% | +2.9042% | PASS |

Only the last candidate is selected. All 540 screening native calls, the 48 earlier magic confirmation calls, and the 96 selected confirmation calls are retained (684 total speed invocations). Failed gates and regressions are not removed. Synthetic checks for rejected prefix/guard/read candidates are also archived. Prefix ASan/UBSan runs disable LeakSanitizer because that runtime cannot inspect the sandbox process tree; memory/address and undefined-behavior instrumentation remain enabled.

## Reproduce

```sh
python3 tools/build/build_r19_current.py --out build-current
# Optional deterministic regeneration of the frozen magic table:
g++ -std=c++20 -O3 tools/native/generate_goal10_magic.cpp -o /tmp/goal10-magic-generator
/tmp/goal10-magic-generator src/goal10_magic_data.hpp
python3 tools/validation/test_goal10_magic.py
python3 tools/benchmarking/confirm_goal10_batches.py --baseline benchmarks-goal10-20261023/anchor/r19_scratch --candidate build-current/g10static --out fresh-confirmation --cpu 1
python3 tools/analysis/audit_goal10_batches.py --dir fresh-confirmation
```

Selected executable: benchmarks/goal10-static-20261023/selected/g10static. Fallback: tools/build/build_goal10_index.py. General pack profile234 remains the separate model default.
