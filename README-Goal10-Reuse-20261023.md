# Opportunity entry reuse experiment — 2026-10-23

The restored execution environment completed the new portable candidate and all checks. The current default remains g10index; no performance gain is added to its historical ledger.

## Change

Enable the existing DMT_CACHE_OPPORTUNITY path on the retained paged lookup and compact index ranking implementation. Keep the 256-pointer array value-initialized so copying complete step objects remains defined. Prediction retains stable cell pointers for the matching observation updates. No model, format, baseline ISA, or fast-math changes.

## Validation

- Verify-only pointer checks compare every cached active entry with the lookup from the actual observation context. Mode 3 passes all three held-out datasets with roundtrip.
- All 15 dataset/mode combinations are byte-identical across frozen anchor, current index, and reuse candidate.
- Existing 1,226 tests pass.
- Preflight: six current-binary CPU1 runs; encode max/min 1.05965, decode 1.09484.
- Full comparison: 54 CLI calls, three datasets, mirrored rotated orders on CPU1. The predeclared 1.25 repeat max/min gate passes all 18 groups. All rows remain; none removed.

## Results

Equal-dataset geometric means of paired phase wall-time throughput ratios:

| Comparison | Encode | Decode |
|---|---:|---:|
| Reuse vs fixed original anchor | +9.0967% | +8.0968% |
| Reuse vs current index, same study | -2.0449% | -0.1624% |

The conservative variation gate is not proof of a small effect. This screen provides no reason to promote reuse; no separate confirmation was run. Both +10% goals remain unachieved. Historical current-index gains remain historical estimates, not replaced by a cross-study calculation.

The extra initialized pointer array and step copying are possible costs, not isolated causal findings. A subsequent experiment could avoid full-capacity initialization using a wrapper with explicit active-prefix copy semantics; removing initialization from a raw pointer array alone is not a valid optimization when whole steps are copied.

## Reproduce

```sh
python3 tools/build_goal10_reuse.py --out ../goal10-reuse-build
python3 tools/build_goal10_reuse.py --verify --out ../goal10-reuse-oracle
python3 tools/benchmark_goal10_guarded.py --baseline benchmarks-goal10-20261023/anchor/r19_scratch --stage1 benchmarks-goal10-index-20261023/selected/g10index --candidate ../goal10-reuse-build/g10reuse --out benchmarks-goal10-reuse-20261023-new --cpu 1
```

Raw data, validation, pointer oracle, protocol, executable and source hashes are in `benchmarks-goal10-reuse-20261023`. Preflight data is retained in `benchmarks-goal10-select-20261023` (directory naming predates candidate selection).
