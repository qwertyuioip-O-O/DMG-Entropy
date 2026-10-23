# Portable combination screening — 2026-10-23

Grouped own-attack unions plus compact opportunity pages was screened against both the fixed goal anchor and grouped attacks. CPU0, 54 timed CLI calls, mirrored and rotated three-arm blocks, three datasets. All 15 mode/dataset three-arm archive identities and the existing 1,226 codec checks passed; the grouped attack full-mask test also passed 102,924 comparisons.

Equal-dataset geometric mean throughput gains: versus the fixed anchor, encode +8.026116%, decode +4.359127%; versus grouped attacks measured in the same blocks, encode +3.123277%, decode +1.164897%. This is exploratory screening, with no independent confirmation. It is not the selected current build and does not satisfy the two-direction +10% goal. See `benchmarks-goal10-combo-20261023` for protocol and raw samples.

A separate compiler-native binary was built outside the deliverable but never benchmarked or selected. The user requires CPU portability; its builder was removed. All retained builders use the portable compiler target, without `-march=native` or required optional ISA extensions. Different architectures require rebuilding the C++ sources with an appropriate compiler; no cross-CPU hardware validation is claimed.
