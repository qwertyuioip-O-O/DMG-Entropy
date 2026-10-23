# Goal10: grouped exact own-attack unions — 2026-10-23

This cumulative candidate starts from the confirmed paged-lookup build and preserves its LTO and scratch changes. It does not include the unselected rank, mobility, compact-key or division experiments. The original goal anchor remains unchanged.

For each position, source pieces receive small IDs. For each group of at most four sources, all sixteen subset attack unions are precomputed. A reverse geometric-ray index identifies sliders potentially affected by occupancy changes. For a candidate, the unaffected subset unions are combined with at most four table loads, then affected surviving pieces and newly occupied own squares are recomputed with the original piece-attack routine.

Removed own squares handle ordinary moves and castling. Added own squares handle ordinary destinations, castling rook destinations and promotions. All occupancy changes, including en-passant removal, participate in slider invalidation. Overlapping attacks remain correct because the unchanged union excludes whole affected sources and the recomputed maps are ORed back; no overlapping target mask is subtracted. Every position gets fresh caches.

Reachable standard positions have at most sixteen own pieces. A full-recomputation fallback handles synthetic positions with more pieces, rather than overflowing the small-ID representation. The standalone oracle includes a seventeen-own-piece FEN in addition to castling, en-passant, promotion, check positions, two-ply expansions and seeded random walks.

The full attack mask and all relation flags passed 102,924 direct comparisons against full recomputation (440 castlings, 8 en-passants, 684 promotions). The release comparison also passed fifteen three-way archive-identity cases (all modes × all datasets), each pack's roundtrip, and the existing 1,226-check suite. Model updates, feature meanings, candidate order and framing are unchanged.

CPU0 screening uses three rotated mirrored blocks per dataset, comparing the fixed anchor, retained pages and grouped candidate: 54 CLI processes. Aggregate gains against the anchor were +5.837% encoding and +7.862% decoding; against retained pages they were +5.844% and +5.072%. All nine anchor-paired blocks were positive in both directions. A separate CPU1 paired confirmation supplies cumulative progress; the screening gains are not compounded with earlier measurements. These remain exploratory single-host results without a statistical-significance or universal-speed claim.

Reproduce with `build_goal10_group.py`, `benchmark_goal10_group.py` and `confirm_goal10.py`, retaining the original anchor executable. Normal binaries use the same `-O3 -flto=1` flags without profile instrumentation, PGO, fast-math or host-specific ISA.

## CPU1 confirmation

| Dataset | Encode gain vs fixed anchor | Decode gain vs fixed anchor |
|---|---:|---:|
| r19 | +8.726% | +6.371% |
| r18 | +8.446% | +6.329% |
| r17 | +3.358% | +3.071% |

Confirmed aggregate encode +6.815%, decode +5.246%, whole-process CPU +6.213%. Both directions improve on every dataset mean; 11/12 encoding and 9/12 decoding blocks are positive. The smaller direction gain exceeds that of the previous confirmed pages stage, so retain grouped attacks as the next balanced working stage. The encoder estimates across separate confirmations are nearly flat; do not compound or interpret that difference as a direct stage-to-stage comparison. The +10% goal is not yet reached.
