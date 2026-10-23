# Lichess comparison variants

Two Lichess Java source snapshots are deliberately retained for reproducible comparisons:

- `lichess-java/src/`: baseline snapshot.
- `lichess-java-currentport/`: current-port comparison variant, including variant-specific `game/Encoder.java` and `game/MoveList.java`.

Many unchanged files are byte-identical between these snapshots; they are not necessarily accidental duplicates. Git internally shares identical blob objects by hash. Do not delete files from one snapshot without first reworking and testing its standalone compile command and source layout.
