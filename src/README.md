# Source layout

| Location | Purpose |
|---|---|
| `rmc.cpp`, `entropy.cpp`, `entropy.hpp` | Core chess and codec implementation |
| `main.cpp` | Main CLI |
| `dmt14_features.hpp`, `dmt15_features.hpp`, `dmt16_features.hpp` | Shared feature implementations |
| `experiments/goal10/` | Goal10 codec variants and experimental table implementations |
| `experiments/r19/` | R19 attack, inplace, ray and scratch variants |
| `generated/` | Generated lookup data and frozen model coefficients |

Public API headers remain in `include/`. Experimental implementations use internal
includes relative to the source root; custom compiler commands need both
`-Iinclude` and `-Isrc`. The scripts in `tools/build/` already supply these paths.
Run generators from `tools/generation/`; their outputs follow this layout.

This is a file organization change. Codec algorithms, model coefficients, public
APIs, executable names and archive formats are unchanged.
