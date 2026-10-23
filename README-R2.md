# Phase/Midgame R2

Experimental DMT2 profiles 17–29 vary middlegame tactical-residual update rates, guard strength, and introduce 8 lightweight opening-choice residual features. DMT1 codes 0–16 and DMG8 are frozen. DMT2 is not an independent-data compression breakthrough and must be validated outside the existing developer corpus.

Build: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4 && ctest --test-dir build --output-on-failure`.

Benchmark: `./build/dmg_mid_bench examples/heldout/*.uci > results.csv`. For paired timing use `./build/dmg_mid_speed 27 examples/heldout/*.uci`.

Benchmark a new human-PGN converted UCI bundle: `./build/dmg_mid_bench --bundle independent.uci-bundle > independent.csv`. This reports **all three phase** log losses for each profile and verifies archive roundtrip.
