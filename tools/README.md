# Development tools

The repository's tooling covers multiple historical codec versions and experiment families. Files at this level deliberately retain stable entry-point paths referenced by `CMakeLists.txt`, documentation, and historical reproduction commands.

- [`analysis/`](analysis/): loss breakdowns, input audits and measurement validation; old `tools/analyze_*.py` and `tools/audit_*.py` commands still work via compatibility launchers.
- `build_*.py`, `generate_*.py`, `train_*.py`: experiment/model generation.
- `benchmark_*.py`, `bench_*.cpp`, `r*_speed.cpp`: timing and comparison harnesses.
- `prepare_*.py`, `reproduce_*.py`: dataset preparation and historical reproduction.
- `summarize_*.py`, `validate_*.py`, `test_*.py`: experiment reporting and validation.
- `r*_model.hpp` and `r*.cpp`: research implementations; these remain colocated because of relative include paths.

Avoid blind deduplication of version-specific implementations. Preserve paths referenced by CMake and published experiments unless corresponding references are updated together.
