# DMG-Entropy v0.8 — measured research report (2026-10-20)

## Question / falsifiable hypotheses

The prior research report recommended (1) removing all illegal candidates (control), (2) a relation-based online model conditioned on the actual legal choice set, and (3) an improved expert mixture. The encoder/decoder and comparison benchmarks have now been implemented in C++20. These results are **tests on previously used development corpora**, not new independent evaluation.

## Implementation details

- `legal-opportunity`: reference v0.7 expected-opportunity square model with every pseudo-legal candidate validated before normalization. Baseline candidate weights preserve v0.7 scoring, so the effect of legal-only normalization is isolated.
- `relation-ae`: same baseline, 24 binary features; each feature is independently corrected using `(actual + 12*4096)/(expected + 12*4096)`, with clipping and a bounded additive correction.
- `relation-joint`: for each legal candidate `m`, compute 24 relation/position flags. Model `q(m) ∝ q_base(m) × exp2(clip(sum theta[f], -128, 128)/64)`. `theta` is updated **after** observing the real move by a fixed-point accumulation of `6*(phi(real)-sum_m q_joint(m)*phi(m))`, where expectations use the model's own quantized pre-update weights. Every 64 observations, theta parameters decay by one tick toward zero. Theta bounded `[-48,48]`. Runtime exponent lookup is a frozen 257-entry fixed integer table.
- `relation-mixture` / `relation-mixture-reset`: Bayesian likelihood update + 1/64 share of prior across five models: legal baseline, A/E, joint, uniform and exact-position dictionary. One mode persists expert responsibilities across games, the other resets only the responsibility vector at boundaries. Dictionaries, relational weights and opportunities remain archive-global in both.
- `auto` encodes all candidate versions and chooses the smallest complete file, including header; decoder runs only the chosen model.
- Deterministic ordering, fixed-point LUT, quantization, arithmetic coder, CRC32, varint lengths and bitstream version are specified by the shipped code. No startup weights trained on another corpus.
- 24 features include: moving an attacked piece, escape to safety, risky destination, defended destination, capture/recapture, relative capture value, capture of defended piece, checking move, attack on enemy high-value pieces, defense of own high-value pieces, pawn advancement, initial minor-piece development, castling, promotion, and multi-piece attacks. Some features are occupancy properties rather than strict pre/post deltas: further relation-graph research remains open.

## Development corpus: 23 previously reused games / 1,718 plies

| Mode | Payload bit count | Payload bits/ply | Total bytes | Total bits/ply | Encode Mply/s | Decode Mply/s |
|---|---:|---:|---:|---:|---:|---:|
| v7-opportunity-square | 7085 | 4.12398 | 923 | 4.29802 | 0.19274 | 0.20384 |
| v7-auto | 7085 | 4.12398 | 923 | 4.29802 | 0.01319 | 0.20339 |
| legal-opportunity | 7034 | 4.09430 | 917 | 4.27008 | 0.14977 | 0.15365 |
| relation-ae | 6959 | 4.05064 | 907 | 4.22352 | 0.05782 | 0.05693 |
| **relation-joint** | **6857** | **3.99127** | **895** | **4.16764** | **0.05707** | **0.05865** |
| relation-mixture | 6861 | 3.99360 | 895 | 4.16764 | 0.05607 | 0.05741 |
| relation-mixture-reset | 6874 | 4.00116 | 897 | 4.17695 | 0.05814 | 0.05850 |
| auto | 6857 | 3.99127 | 895 | 4.16764 | 0.00655 | 0.05977 |

**Interpretation:** v0.7 → full legal gives 51-bit gain; A/E gives another 75 bits; conditionally learned joint features further save 102 bits on this corpus. Best v0.8 joint vs v0.7: 228 payload bits saved or 28 full-file bytes. The v0.8 decoder is approximately 3.5× slower than v0.7. This currently falls short of the desired typical CPU throughput despite achieving sub-4.0 **payload** bits/ply on development data.

## Other corpora (all round-trip verified)

| Dataset | v7-auto payload bits/ply | v8 joint | v8 mixture | v8 auto | Notes |
|---|---:|---:|---:|---:|---|
| Prior 4 human games, 496 plies | 4.25000 | 4.06250 | 4.08266 | 4.06250 | Previously studied, not independent |
| 16 seeded random legal games, 1,024 plies | 4.91602 | 5.14258 | **4.86133** | 4.86133 | Uniform reference is strong; mixture protects against bad human priors |
| Four prior human games repeated 3×, 1,488 plies | 2.56586 | 3.78024 | **2.49462** | 2.49462 | Deliberate repetition; high dictionary reuse |

For exact CSV incl timing and total file bytes, see `results-v0.8/`. No independent modern online-game corpus was fetched in this session. Treat 3.991 as a **proof of implementation and improvement on existing dev cases only**.

## Verification carried out

1. Release C++20 compilation + 4 ctest suites PASS.
2. All v8 modes: empty archives, repeated move sequences and 8 generated legal games; deterministic byte-for-byte encoding.
3. Special rules: legal moves with castling, en passant capture, promotion to queen are included in dedicated fixtures.
4. CRC32 alteration test rejects modified data; legacy DMG6 and DMG7 archives decode correctly through the new dispatcher.
5. Frozen 20-byte DMG8 sample roundtrips and encodes to identical bytes: SHA256 `9d6cdb4e7880211ee368d67d0f71f3c23a450738f8b6c71c3946ece6fc137af9`.
6. AddressSanitizer + UndefinedBehaviorSanitizer v0.8 tests PASS.
7. Identical benchmark sets, release flags and CPU used for v0.7 and v0.8; encoded payload, total file size and both directions of throughput recorded.

## Research interpretation & next steps

- The central online learner improving over independent A/E is a **promising structural result**; it is not a patent novelty claim and not evidence of 3.9 bits/ply on unseen human games.
- Near-optimal arithmetic coder overhead in the attached research report implies that better conditional probabilities, not switching entropy coder brand, are likely to be most valuable.
- Relation calculation is the runtime bottleneck; attack masks once per resulting position sped it up, but v0.8 joint encode is still around 0.057 Mply/s vs 0.193 for v0.7. Optimize legal-move and candidate attack calculation next.
- Acquire stratified independent humans: large opening/middlegame/endgame, player skill and time controls; compare multiple independently reset archives and permuted archive order. Reject/accept improvements on unseen data, using paired game/block differences, not single developer average.
- Examine joint feature ablations and losses of extremely unlikely real moves, not only average top-1 accuracy. Do not tweak weights on the final holdout set.
- If deployed, formalize integer overflow proofs and perform golden-stream cross-platform Rust and ARM tests. `auto` multiplies encoding time; don't apply it to time-sensitive streaming without a policy.
