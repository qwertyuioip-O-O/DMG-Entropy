# DMG-Entropy

DMG-Entropy is a C++20 research project for lossless compression of chess move sequences.

This repository contains the DMT19 research snapshot: codec sources, historical compatibility tests, examples, benchmark tools, and experiment reports. The archive represents move sequences; it does not preserve PGN tags, comments, variations, or text formatting.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Current results

- [15,000-game cohort report](README-Cohorts-20261023-KO.md)
- [Lichess and Chessbook comparison](benchmarks-chess-comparison-20261023/README-Lichess-Chessbook-Compare-20261023-KO.md)
- [Current speed baseline](CURRENT-SPEED-BASELINE.md)
- [DMT19 research report](README-R19.md)
- [Goal10 static-magic implementation report](README-Goal10-Magic-Static-20261023.md)

Results depend on the dataset, hardware, compiler, and measurement path. Read each report's methodology and limitations before comparing numbers.

Large raw game corpora, local benchmark binaries, and build outputs are not included. Historical source and experiment notes are retained for reproducibility and context.
