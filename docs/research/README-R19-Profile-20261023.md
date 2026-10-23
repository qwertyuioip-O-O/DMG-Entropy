# R19 CLI profiling and initialization candidate — 2026-10-23

The frozen R19 codec and learned tables remain unchanged. This experiment adds `tools/r19_fast.cpp` and `tools/r19_fast_model.hpp`, an isolated candidate differing only in how `base19` initializes its `PriorStep` base object. Instead of default-constructing the large object and assigning its `MidStep` base, it aggregate-initializes the base directly from the returned `MidStep`. Member default initializers still initialize the two extra weight snapshots. Archive format and model semantics are unchanged.

## Evidence and scope

GNU gprof was available; perf was unavailable. An `-O3 -pg -g` build ran the standard speed command on new422, mode3, pinned to CPU0. The profile includes warm-up, three encode and three decode iterations, loading and checks. It reports 4.43 sampled self CPU seconds: `v8_relations` 14.90%, `MidStep::MidStep()` 10.38%, `MidResidual::observe` 10.16%, and `mid_step_legacy` 7.90%. The raw flat profile and call graph are retained under `benchmarks-r19-fast-20261023/profile/`.

Instrumentation changes code generation and call costs. These percentages identify candidates for investigation, not production cost fractions or a promised 10% saving. Only one redundant initialization site is changed; many other constructors remain.

## Reproduction

```sh
python3 tools/build_r19.py --out build-r19
python3 tools/build_r19_fast.py --out build-r19-fast
python3 tools/benchmark_r19_fast.py --baseline build-r19/r19_prior --candidate build-r19-fast/r19_fast --out fresh-fast-study
```

Both timed executables use the standard `-O3` flags without profiling instrumentation, the same compiler and the same `rmc.o` in the recorded run. The candidate build script checks that its source difference is limited to the specified initialization and include routing.

The benchmark predeclares CPU0, all three datasets and 4/2/2 alternating ABBA/BAAB blocks before timing. It first checks complete archive-byte equality across all five modes on all three datasets, with each pack command checking its own roundtrip. It also runs the candidate's existing CRC/truncation/wrong-model/empty/LUT/reference tests. Timed calls use the actual CLI speed command with warm-up and three iterations per direction. Ratios compare sums within each four-process block and then use a geometric mean. There are 32 timed processes across eight blocks. Results are exploratory on this host; the short series does not establish statistical significance or other-hardware performance. The baseline here is original R19, not original R18; percentages must not be combined with the earlier speed study.

## Measured outcome

| Dataset | Encode gain vs original R19 | Decode gain vs original R19 | Blocks |
|---|---:|---:|---:|
| r19 | +5.680% | +5.761% | 4 |
| r18 | -4.542% | -5.695% | 2 |
| r17 | +4.795% | +2.432% | 2 |

All 15 dataset/mode archive comparisons are byte-identical. The candidate passed 1,226 existing checks. Frozen codec/model files match the original research archive. The change is not consistently faster across the three datasets, so it remains an isolated experimental candidate and is not promoted to the default. No 60–70K throughput claim is made. Block variation and the two-block old-dataset samples require caution; neither a universal gain nor the mechanism behind the regression is established. Next substantive work should reduce the large intermediate-state construction/copying across the whole pipeline or inspect relation calculations, rather than assuming this single initialization site is sufficient.
