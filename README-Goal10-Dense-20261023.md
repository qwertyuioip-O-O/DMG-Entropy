# Compact opportunity pages over index ranks — 2026-10-23

The retained index-rank core was changed only by replacing the opportunity-table header and two table type names. Reverse substitution reproduces the original core exactly. Ordinary promotion-zero keys store key>>3; nonzero low-three-bit keys retain their full ID in a separate directory, preventing collisions. Pages and presence semantics remain exact. Ordinary cells become denser, but whole-process memory is not universally eightfold smaller and worst-case allocation can increase.

The table and its differential oracle are SHA256-identical to the earlier 297,620-check validation; that result was reused, not rerun. Current whole-codec validation passed all 15 three-arm mode/dataset archive byte identities and all 1,226 codec checks.

CPU0 screen used 54 serial, mirrored/rotated CLI calls. Equal-dataset geometric mean throughput gains versus frozen anchor: encode +13.682810%, decode +12.259556%. Against the retained index-rank build in the same blocks: encode -0.327513%, decode -3.248830%. The retained control was better overall, so this candidate is not adopted and no independent confirmation was run. Baseline-relative gains alone do not justify replacement. Blocks were variable; these are exploratory same-host measurements, not a claim that compact pages always lose.

All flags remain portable O3+LTO, with no CPU-native target. Features, coefficients, update arithmetic and archive semantics are unchanged. Raw samples, protocols, hashes, host information and reused-validation provenance are in benchmarks-goal10-dense-20261023.
