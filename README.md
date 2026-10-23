# DMG-Entropy

**A C++20 research codec for lossless chess move-sequence compression.**

DMG-Entropy models legal moves as they are played and entropy-codes the resulting sequence. It is an experimental codec and benchmark project: the repository includes versioned formats, compatibility tests, reproducible tools, and a large set of research reports.

> **Current focus:** portable encoding and decoding speed, measured on ordinary, elite, and uniformly random legal games. This is research software, not yet a drop-in replacement for general PGN archives.

## What it stores

The codec preserves standard chess move sequences, including move order, game boundaries, castling, en passant, and promotion. It does **not** preserve PGN tags, comments, variations, whitespace, or arbitrary starting positions/FENs. It is not a PGN parser or a search-ready opening-book format.

The runtime model is deterministic and integer-based. Encoding and decoding use the same model updates, so a decoder can reconstruct the move stream without a pretrained neural model or external chess engine.

## Current benchmark

The latest large three-cohort evaluation covers **15,000 games and 2,547,354 plies** (5,000 games per cohort). It reports complete framed archive size and single-core CLI throughput for the selected portable build.

| Game cohort | Framed bits/ply ↓ | Encode plies/s ↑ | Decode plies/s ↑ |
|---|---:|---:|---:|
| Ordinary human games | 3.475 | 78,025 | 77,816 |
| Elite games | 3.610 | 77,231 | 76,925 |
| Uniformly random legal games | 4.508 | 101,053 | 102,856 |

Measurements were run on one pinned logical CPU on an AMD EPYC 9V74 host. Results are specific to this corpus, host, compiler, and CLI measurement path; they are not universal performance claims. The random cohort is a diagnostic distribution, not a proxy for human games.

For cohort definitions, data handling, exact commands, and limitations, see the [full benchmark report](README-Cohorts-20261023-KO.md). The [current speed baseline](CURRENT-SPEED-BASELINE.md) documents the selected build and its confirmation protocol. Compression figures include archive framing; older reports may quote payload-only bits per ply, which are not directly comparable.

## Download and build

Download the [DMT19 source and research bundle](DMG-Entropy-DMT19-Source.zip), extract it, then build and run the test suite:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The bundle includes source code, tests, examples, benchmark tools, and selected reports. It excludes raw game corpora, local benchmark binaries, and build outputs.

## Project contents

- **Versioned codecs and compatibility tests** — current and legacy formats with regression coverage.
- **Benchmark tools and reports** — cohort-level compression and speed measurements, plus controlled optimization experiments.
- **Research history** — model and implementation variants, including rejected candidates and their measured trade-offs.

Start with the [cohort benchmark report](README-Cohorts-20261023-KO.md) for the broad evaluation, then consult the dated experiment reports for implementation details.

## Scope and limitations

- Input is a standard-start chess move sequence; this project does not currently encode the full PGN document.
- No claim is made that DMG-Entropy beats every chess-specific or general-purpose compressor. Comparisons are meaningful only when corpus, framing, and measurement procedure match.
- Speed and size vary with game distribution, hardware, compiler, and build configuration.
- Experimental formats and model choices are versioned; use the compatibility tests and format-specific tools when exchanging archives.

## License

See the source bundle for the license and notices that apply to the included code.
