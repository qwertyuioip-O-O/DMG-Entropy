# DMG-Entropy

DMG-Entropy is a C++20 research project for lossless compression of chess move sequences.

The current research snapshot is DMT19. It includes the codec implementation, compatibility and round-trip tests, benchmark tooling, and research reports. The codec stores move sequences; PGN tags, comments, and formatting are outside its payload.

This repository is being populated from the working DMT19 snapshot. Benchmark numbers are dataset- and machine-specific; see the dated reports before comparing results.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Current snapshot

- Core implementation: `src/`, `include/`
- Tests: `tests/`
- Tools: `tools/`
- Reports: `README-*.md`, `benchmarks-*/`

Large raw game corpora and local build artifacts are not included in this public repository.
