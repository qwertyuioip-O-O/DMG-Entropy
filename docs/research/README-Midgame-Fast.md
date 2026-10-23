# DMG Midgame Fast — exact DMT1 tactical kernel optimization

This code is a **byte-compatible speed-up**, not a new probabilistic compression model. It keeps the DMG8-Fast baseline and the previously experimented DMT1 profiles available. It **does not** claim 3.8 bits/ply on independent opening/middlegame/endgame samples.

## Implemented

- `mid_target_attackers`: look up pawn/knight/king attackers by reverse attack masks; find nearest rook/bishop/queen on each of eight target rays. No scanning all pieces and calling `pseudo_destinations()` for each capture exchange step. The legal recapture check remains in the bounded SEE routine.
- `mid_landing_capture_targets`: compute moved piece attacks from a small occupancy overlay, without cloning and applying the position per candidate. Precisely account for promotions and en passant. DMT1 only consults these attacks on *opponent-occupied* target squares; ordinary pawn pushes and attacks on friendly-occupied squares are deliberately omitted.
- SEE avoids one redundant `square_attacked()` check and reuses the already-checked best recapture's board state instead of applying that same move again.
- Optional `-DDMT_VERIFY_FAST=ON` cross-checks fast reverse attackers and landing threats against the original `Position` oracle for every generated candidate. For profiling use `OFF`.
- The existing 17 DMT1 probabilistic profiles and byte layout are not modified. Existing DMG6, DMG7, DMG8, DMG9, DMP1 decoders are untouched.

## Reproduce

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDMT_FAST_TACTICAL=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/dmg_mid_speed 14 examples/heldout/*.uci
# To test original exact behavior, build with -DDMT_FAST_TACTICAL=OFF
# To detect semantic mistakes, build with -DDMT_VERIFY_FAST=ON
./build/dmg_random_corpus 48 96 20261021 > random.uci-bundle
./build/dmg_mid_speed 14 --bundle random.uci-bundle
```

## Prepare future independent real human-game evaluation

This environment could not retrieve the public PGN export, **so no independent human corpus was evaluated**. The following reproducible pipeline is included for when a new PGN is available:

```bash
python3 tools/data-preparation/prepare_pgn.py unseen_games.pgn independent.uci-bundle \
  --converter "$(pwd)/build/san_to_uci" --limit 250 --min-plies 40
./build/dmg_mid_speed 0 --bundle independent.uci-bundle
./build/dmg_mid_speed 14 --bundle independent.uci-bundle
```

The converter filters nonstandard variants, custom FEN starts, marked bots, and short games; strips PGN comments, annotations, and variations; uses the actual C++ chess move checker to convert SAN to legal UCI; outputs one game per line; records input SHA256 and filters. **The sampling plan must still be specified independently, and a supplied PGN should be checked for bias.** A public candidate source is https://database.lichess.org/standard/ .

See `RESULTS-Midgame-Fast.md` for numbers and limitations.
