# DMG-Entropy current research models — 2026-10-22

Default encoder: **234 / r10-combo-tact4-full**, with output-preserving performance changes.
Compression-first candidate: **250 / r12-joint-all**. The default has not been switched.

Read **README-R12.md** for the current results, limitations, format, build and reproduction instructions. Earlier reports are historical records.

- 42 probability configurations explored; 50 new games for development, 25 for selection, 25 held out until the preset was frozen.
- Existing IDs 0–248 remain byte-compatible. New configurable stream: DMTC/249; fixed preset: DMTD/250.
- Normal build enables exact feature-score tables; opportunity-node cache is experimental and OFF.
- LTO is optional; PGO was tested and is not a default recommendation.
- Research only: no claim that all phase compression or 60–70K speed targets are met.
