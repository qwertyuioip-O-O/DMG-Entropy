# Shared observation statistics — 2026-10-23

A portable candidate caches the weighted expected additional-feature counts within one observation. The middle residual and context residual keep independent learner state, update rates and decay schedules, but share identical distribution-derived statistics. Other observers keep the original implementation. The cache is local, with no persistent invalidation requirement.

An optional DMT_VERIFY_SHARED oracle runs the original update and checks all theta/rest/seen/steps fields after each changed observer call. It passed mode3 evaluation on all three heldout datasets. Release validation passed all 15 three-arm mode/dataset archive identities and the existing 1,226 codec checks.

CPU0 exploratory screening used 54 rotated/mirrored CLI calls. Equal-dataset geometric mean throughput gains versus the fixed anchor: encode +3.851939%, decode +5.592096%. Against the retained grouped-attack build in the same experiment: encode -0.853623%, decode -0.927513%. The change is not selected. Reducing duplicate arithmetic did not improve whole-codec throughput in this screen; no causal compiler/layout explanation has been established. No independent confirmation was run for this losing candidate. See benchmarks-goal10-shared-20261023 for raw results and hashes.

The generated candidate is intentionally isolated. Its explicit local cache adds control and storage; fewer mathematical operations alone do not establish faster execution. The performance result is empirical, not a proof that all possible fused-update implementations lose.
