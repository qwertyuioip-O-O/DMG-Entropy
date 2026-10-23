# Disjoint-mask phase counting — 2026-10-23

Starting from the retained grouped-attack build, this candidate unions disjoint piece masks that carry equal phase weights. Eight population counts become three, and the phase classifier reuses one computed value. No optional CPU ISA is required. This candidate does not include the losing shared-statistics change.

DMT_VERIFY_SHARED enables a phase oracle that compares the complete pre-clamp phase with the original eight-count formula. It passed mode3 evaluation on all three datasets. All 15 three-arm mode/dataset archive identities and the existing 1,226 codec checks passed.

CPU0 screening: 54 rotated/mirrored CLI calls, three datasets, fixed anchor and retained grouped build. Equal-dataset geometric mean throughput gains versus the fixed anchor: encode +5.651169%, decode +4.961081%. Versus the grouped build measured in the same blocks: encode -0.312343%, decode +0.822397%. This mixed, small screen does not justify replacement; the candidate is not adopted and no independent confirmation was run. Raw per-dataset results and samples are in benchmarks-goal10-phase-20261023.

The portable grouped-attack build remains selected, with independent CPU1 confirmation of +6.814764% encode and +5.245610% decode versus the fixed goal anchor. Both directions have not reached +10%. Smaller operation counts do not guarantee faster machine code.

Further investigation can specialize the fixed R19 feature/model path with generated code, fuse repeated candidate passes, or change table layout without changing probabilities. These are proposed directions, not measured gains. Current profiling evidence was collected on the older fixed anchor; fresh profiling of the retained build should precede large specialization work.
