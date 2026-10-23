# Development tools

Run commands from the repository root. Each script has one canonical location;
the previous `tools/<filename>` compatibility launchers have been removed.
Update local automation to the paths below.

| Directory | Contents |
|---|---|
| `analysis/` | Dataset audits, loss analysis and measurement quality checks |
| `benchmarking/` | Performance runs, cohort evaluation and confirmation batches |
| `build/` | Compiler drivers for research variants |
| `data-preparation/` | PGN conversion and cohort preparation |
| `generation/` | Code/table generators and their JSON inputs |
| `native/` | C++ CLI, benchmark and research sources with companion headers |
| `reporting/` | Summaries and comparison report generation |
| `reproduction/` | Historical experiment reproduction |
| `training/` | Model training and control runs |
| `validation/` | Python checks and the shell validation runner |

Examples:

```sh
python tools/build/build_r19.py --help
python tools/analysis/audit_goal10_measurements.py --help
python tools/validation/test_goal10_measurement_guard.py --out build/measurement-guard.json
bash tools/validation/run_v8_validation.sh
```

CMake targets and executable names are unchanged. See the [source layout](../src/README.md)
for core, experimental and generated files in `src/`;
public headers in `include/`, C++ regression tests in `tests/`, and published
benchmark data in `benchmarks/`. Recorded benchmark commands are historical
provenance and retain the paths used when measurements were taken; use this guide
and the updated reports for current commands.
