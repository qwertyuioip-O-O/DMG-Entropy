#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
mkdir -p results-v0.8
./build/dmg_v8_bench examples/heldout/*.uci > results-v0.8/human23.csv
./build/dmg_v8_bench examples/human/*.uci > results-v0.8/human4.csv
./build/dmg_v8_bench --random > results-v0.8/random16.csv
./build/dmg_v8_bench --repeat 3 examples/human/*.uci > results-v0.8/repeated3.csv
sha256sum examples/golden/italian-relational.dmg8
