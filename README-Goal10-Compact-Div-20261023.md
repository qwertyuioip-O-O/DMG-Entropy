# Goal10: compact pages and invariant division — 2026-10-23

The fixed anchor remains the initial LTO + scratch executable. The retained paged-lookup stage reached confirmed CPU1 aggregate gains of +6.919% encode and +3.494% decode against it. This continuation keeps that implementation available and investigates two further exact changes. Earlier screen/confirmation gains are never compounded.

## Compact ordinary-move keys

Both opportunity-key layouts end in three promotion bits. Most moves have all three bits zero. The compact table routes those keys into an ordinary directory using `key >> 3`; nonzero low bits use a separate directory retaining the full original key. This split is injective across the full supported key range, including the test's arbitrary promotion-bit values. Presence, zero-valued records, stable pointers and deep-copy semantics are preserved.

The ordinary directory has 1,024 pointers (8 KiB on this host), rather than 8,192 (64 KiB), and eight times fewer possible ordinary cells. Promotion storage is allocated lazily and retains its original capacity. Worst-case fully allocated storage can therefore increase: approximately 2.03125 MiB ordinary pages plus 16.25 MiB uncommon pages and 72 KiB directories per table. Common-case key density improves; this is not a blanket claim of eightfold whole-process memory reduction. The 297,620-check map differential oracle passed again for the compact container.

The CPU0 compact screening compares fixed anchor, retained pages and compact pages with three rotated mirrored blocks per dataset (54 CLI processes). All three archives must match on all five modes and all three datasets; the candidate runs the existing 1,226-check suite.

## Hoisted exact division

The cumulative `g10div` candidate adds hoisted invariant denominators to compact pages in two functions:

- `V8Learn::observe`: construct exact dividers for `joint.total` and `base.total` once before the feature loop, replacing repeated native divisions;
- `v09_cached_observe`: construct one exact divider for the candidate total before its candidate loop.

This uses the existing `ExactDivisor` implementation already used by the codec's quantizer and residual updates. It computes a 128-bit reciprocal-product quotient plus integer remainder correction; the portable fallback uses ordinary integer division. No floating-point approximation is introduced. The denominators are nonzero on these valid model-update paths. Count updates, rounding terms and saturation order remain identical.

`test_goal10_div.py` extracts the ACTUAL `ExactDivisor` definition from the generated source and compares it with native division. It passed 2,196,608 cases covering small denominators, maximum-integer edges, full-width random inputs and model-style numerators. This is not a second reimplementation of the optimized algorithm. Full byte/roundtrip/error validation is also required for the new codec binary.

## Combined comparison and cumulative progress

`benchmark_goal10_div.py` compares the fixed anchor, the retained pages implementation and compact+division on CPU1 with the same three-way mirrored/rotated block design: 54 actual CLI processes over three datasets. Each process warms and runs three measured repetitions in each direction, checking determinism and roundtrip. Phase wall durations are summed within each block, and geometric throughput ratios are equally weighted by dataset. The goal remains at least +10% in each direction versus the fixed anchor, not versus the previous stage.

A promising cumulative candidate is then confirmed against the same anchor on CPU0 using `confirm_goal10.py`, reusing full correctness validation for unchanged binary hashes. The confirmation provides the primary reported cumulative gain and goal status. Per-dataset regressions and unfavorable experiments remain in the evidence. Selection and measurements are exploratory on this host; no universal or statistical-significance claim is made. No build, oracle test or other benchmark runs concurrently with timing.

## Reproduce

```sh
python3 tools/build_goal10_pages.py --out build-pages
python3 tools/build_goal10_compact.py --out build-compact
python3 tools/build_goal10_div.py --base compact --out build-div
python3 tools/test_goal10_div.py --out test-div
python3 tools/benchmark_goal10_div.py --baseline benchmarks-goal10-20261023/anchor/r19_scratch --stage1 build-pages/g10pages --candidate build-div/g10div --cpu 1 --out fresh-div-comparison
```

All retain ordinary `-O3 -flto=1`, without PGO, fast-math or host-specific ISA. Model coefficients and archive framing are unchanged. General pack profile234 remains a separate default.

## Compact + division CPU1 result

| Reference | Encode aggregate gain | Decode aggregate gain |
|---|---:|---:|
| baseline | +5.239% | +3.127% |
| stage1 | +0.239% | -0.569% |

Large dataset/block variation remains, including a new422 decoder regression. The combined candidate does not improve both aggregate directions against the retained pages stage, so it is not selected. Its correctness validations passed. The compact-only CPU0 result remains an unconfirmed optional candidate; no unsupported cumulative gain is assigned to it.
