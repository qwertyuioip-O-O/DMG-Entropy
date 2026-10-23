# R19 reproducible benchmark

One Python entry point compares the original fixed scratch+LTO anchor and the selected g10static implementation. Uses Python 3 standard library on Linux/macOS and the native C++ executables; no Python packages required. The supplied executables are Linux builds. Rebuild them for another machine/OS as needed.

## Full run

From the project directory:

```sh
python3 tools/benchmark_r19.py --out benchmark-full --cpu 1
```

Default: all three held-out bundles, mode 3, four alternating ABBA/BAAB blocks per dataset, two consecutive native CLI calls averaged per observation. This is **96 native speed calls**. Each native invocation performs warmup, byte determinism and roundtrip checks plus its original three timed encode/decode repetitions. Do not run builds or other benchmarks alongside it.

`--cpu` is optional. Omit it for normal scheduler placement, or choose an allowed core on Linux. On systems without affinity support, omit it. The output directory must be new or empty, so previous measurements cannot silently be overwritten.

## Quick smoke check

```sh
python3 tools/benchmark_r19.py --out benchmark-smoke --rounds 1 --batch 1 --cpu 1
```

This is 12 native calls. Use it to verify the runner and data, not to establish a performance gain. The included `benchmarks-r19-runner-smoke-20261023` is this check; the earlier accepted study remains in `benchmarks-goal10-static-20261023/confirmation-batched-cpu1`.

## Rebuild or supply your own inputs

```sh
python3 tools/build_r19_scratch.py --out build-anchor
python3 tools/build_r19_current.py --out build-current
python3 tools/benchmark_r19.py --baseline build-anchor/r19_scratch --candidate build-current/g10static --out benchmark-rebuilt
python3 tools/benchmark_r19.py --dataset my-games=/absolute/path/games.uci-bundle --out benchmark-custom
```

Repeat `--dataset LABEL=PATH` to compare multiple datasets. It replaces the default dataset list. `--mode` accepts 0 through 4; tables use builtin. These candidates preserve the same model and must produce identical framed archives. A mismatch aborts before timing. On another CPU/compiler, rebuilt binary hashes can differ from the frozen anchor; the report records that fact and does not treat them as the exact original goal anchor.

## Outputs

| File | Contents |
|---|---|
| `report.md` | Per-dataset plies/s, framed bytes and bits/ply, paired gains and range gate |
| `summary.json` | Same numerical results and measurement quality details |
| `native.csv` | Every individual CLI call, phase times, total process CPU/wall time |
| `units.csv` | Declared fixed-size batch means, without dropping samples |
| `protocol.json` | Binary/input paths and SHA256, host architecture, affinity, ordering and settings declared before timing |
| `archive-*-*.d19` | Baseline and candidate byte-identity checks |

Throughput columns are medians of per-observation plies/second. Gains instead use paired block time ratios and equal-dataset geometric means, so dividing two displayed medians does not necessarily reproduce the gain column. Archive size includes framing and CRC; it is not the raw entropy loss metric. CLI phase times exclude startup/input parsing, while process CPU/wall columns include them as diagnostics.

The range gate checks max/min <=1.25 on the declared measurement units. When `--batch 2` is used, this is a gate on two-call means, not a claim that each individual native call is stable. Inspect `native.csv` for short-window variation. No samples are removed. A passing range gate and a point estimate above 10% do not establish a confidence lower bound above 10%, nor a guarantee for every dataset or CPU.
