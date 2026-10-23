# Counted relative ranks — 2026-10-23

A fresh gprof sample of the retained grouped-attack build attributed about 10% of sampled self time to relative-rank features. Instrumentation, inlining and symbol attribution limit the precision of that estimate; the profile is motivation, not speed evidence.

The candidate replaces comparator sorting of at most 256 RankItems with a stable counting scatter over 42 scores. Centralization is in [-6,6]. The remaining additions give a conservative total range [-15,26]: minimum -6-6-3, maximum 6+3+6+3+4+2+2. Counts and descending prefix offsets determine contiguous output runs. Input index order within tied scores is preserved, and the original strictly-better/class counters and output flag construction remain unchanged. Only the initialized prefix of the scratch array is copied. No ISA-specific target flag is used.

Verification: the optional reference-sort oracle passed 1,044,976 synthetic candidate comparisons in 8,192 sets, sizes 0..256, with arbitrary tactical/capture flags including combinations absent in typical games. All 15 mode/dataset three-arm archive identity cases and 1,226 existing codec checks passed. Learned coefficients, distributions and archive framing are unchanged.

CPU0 screen: 54 serial, rotated/mirrored CLI calls on three datasets. Equal-dataset geometric mean gains versus the same frozen anchor were encode +10.309092%, decode +11.044061%; versus the grouped build measured in the same blocks, +3.104389% and +5.210507%. Fresh confirmation used CPU1, 48 serial ABBA/BAAB calls and the same binary hashes; every CLI speed invocation still validates repeated archive bytes and roundtrip.

Confirmation versus fixed anchor: encode +10.209134%, decode +6.615225%, whole-process CPU throughput +7.824860%. The two-direction +10% goal is not yet met in both directions. The portable counted-rank candidate is selected because its balanced confirmed progress exceeds the prior stage. Percentages from different experiments are never compounded.

| Dataset | Encode gain | Decode gain |
|---|---:|---:|
| r19 | +13.627% | +7.320% |
| r18 | +14.090% | +2.635% |
| r17 | +3.258% | +10.022% |

These are measurements on one host, with two cores used across screening and confirmation. They are not a guarantee of the same speedup on other processors. Compiler-native builds remain excluded; other architectures require rebuilding the portable C++ sources. Raw samples, protocol, hashes, oracle results and the selected binary are retained in this report directory.
