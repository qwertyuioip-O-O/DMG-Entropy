# DMG-Entropy DMT8 — Online Expert Models (experimental)

**2026-10-20.** This C++20 research branch extends DMT7's existing 98 profiles with 27 additional DMT8 profiles (**98–124**, total **125**). Existing DMT1–DMT7 wire formats remain frozen and independently decodable. The new container uses `DMT8`, version 1, profile byte.

## Design

All models are deterministic, archive-local, and trained exclusively from previously coded/decoded moves: **no external weights, network calls, opening books, hidden side information, or Elo labels**.

**Stage ensemble (98–111):** Independent probability estimates are taken at five existing prediction checkpoints (relation baseline; tactical correction; rank/context correction; positional correction; complete DMT6 correction). Compare fixed weights, exponentially updated global/context weights, and conservative switch-to-expert rules. This is cheap but underperforms the final DMT6 expert on most human corpora.

**Trained specialists (112–118):** Separately update three `MidResidual` classifiers for (1) tactical exchange/capture and relative rank, (2) quiet piece-type choice, and (3) move-history/plan continuity. Inputs reuse already available DMT6 candidate flags; there are no fresh chess searches. Compare static expert weights and online Bayesian-style global/context weights with bounded integer smoothing toward a prior.

**Fused trained specialists (119–124):** Avoid intermediate 16,000-count CDF re-quantization by calculating the three independent specialist raw scores, normalizing each to a deterministic fixed-point scale, combining, then quantizing **once**. Profile **119** mixes all three specialists with fixed equal weights, while each specialist is independently updated online. Profiles **120–121** learn the mixture weights online globally/by tactical context. Profiles **122–124** explore two-expert mixes.

**IMPORTANT:** The strongest profile, **119**, has a fixed equal expert blend, *not* adaptive expert selection, although all three expert probability distributions **are learned online**. Adaptive expert weight selection is implemented in 100–105/114–115/120–121 but was weaker in the used human corpora. Do not claim successful online context-adaptive selection based on profile 119's performance.

## Reproduce

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE='-O3 -DNDEBUG -march=native'
cmake --build build -j4
ctest --test-dir build --output-on-failure
build/dmt8_matrix wcc13 benchmarks-r4/wcc2024_01_13.uci-bundle
build/dmt8_matrix human23 benchmarks-r4/human23.uci-bundle
build/dmt8_matrix human4 benchmarks-r4/human4.uci-bundle
build/dmt8_matrix random48 benchmarks-r2/random48.uci-bundle
build/dmt8_expert_analysis wcc13 benchmarks-r4/wcc2024_01_13.uci-bundle
build/dmg_mid_speed 75 --bundle benchmarks-r4/wcc2024_01_13.uci-bundle
build/dmg_mid_speed 119 --bundle benchmarks-r4/wcc2024_01_13.uci-bundle
build/dmg_mid pack r8-fused-uniform compressed.dmt examples/heldout/*.uci
build/dmg_mid unpack compressed.dmt recovered/
```

Profiles are explicitly encoded; decoder reproduces each exact online update from historical decoded moves. Floating point is **not** used in probability construction or expert updates, only for the *diagnostic log-loss*. The compressed message includes the archive's framing and CRC checksum.

See `RESULTS-R8.md` for complete testing limitations, measured results, and next steps.


> Follow-up research: experimental DMT9 extension now documented in `README-R9.md`. Original DMT8 profile119 remains frozen.
