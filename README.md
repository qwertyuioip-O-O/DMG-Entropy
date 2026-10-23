# DMG-Entropy

DMG-Entropy is a C++20 research project for lossless compression of chess move sequences.

## Download source

The [DMT19 source and research bundle](DMG-Entropy-DMT19-Source.zip) contains the codec sources, compatibility tests, examples, benchmark tools, research notes, and selected aggregate results.

After extracting the archive:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The codec stores move sequences. It does not preserve PGN tags, comments, variations, nonstandard starting positions, or text formatting. Raw game corpora, local benchmark binaries, and build outputs are excluded.

Benchmark results depend on the dataset, hardware, compiler, and measurement path. Review the dated reports in the archive for methodology and limitations.
