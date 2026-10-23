# DMT10/234 — main-profile promotion verification

Date: 2026-10-22

## Main decision

The project had *no implicit default* in `dmg_mid pack`: callers had to pass a named profile. The old `rmc` CLI has an unrelated hybrid codec default. On this research branch, `DMT10/234` is now the **preferred main DMT encoder** for newly encoded archives, with `DMT10/231` retained as a compromise for nonhuman inputs. `DMT8/119` and `DMT9/182` remain unchanged and readable.

This is a **selector-level promotion**, not a change to the mathematical probability model. `DMT10/234` is the best on the previously tuned human development corpora but was only slightly better on small external human sets; 60–70K plies/s remains unmet. The change is NOT a production performance claim.

## Changes

1. `include/dmg_mid.hpp`: named `MID_DEFAULT_PROFILE = R10ComboTact4Full` (234); zero-argument profile selection available from the C++ `mid_encode(games)` API.
2. `tools/mid_cli.cpp`: `dmg_mid main` shows current main; `dmg_mid pack output.dmt *.uci` now defaults to 234; `pack best` and `pack default` alias the main; explicitly specified profile names continue to work, including 119, 182, and 231. Unknown profile strings fail rather than silently selecting 234.
3. `tests/default_profile_tests.cpp`: assert 234, implicit-vs-explicit byte equivalence, archive profile marker, old profile decoding and CRC rejection.
4. `CMakeLists.txt`: register fast `dmt_default_profile` test.
5. Existing `DMT1`–`DMT10` encoding code and format markers are unchanged. The decoder still trusts *encoded* version/profile, not the current default.

## Actual local verification

- CMake Release build: PASS (`dmg_mid`, `dmt_default_tests`, `dmt10_tests`, `dmt9_experiments`).
- `dmt_default_tests`: PASS (default = explicit 234; roundtrip; 119/182/231/234; CRC corruption rejected).
- CLI compress Human23 23 games via implicit default and explicit `r10-combo-tact4-full`: **byte-for-byte identical**, 6,497 payload bits, 850 whole-file bytes.
- CLI `pack best`: same bytes; `unpack`: 23 games reconstructed.
- CLI malformed profile selector: nonzero exit, no silent fallback.
- `dmt10_tests`: **PASS**, 64 representative/new profiles on Human23, random8 and empty; 9 old golden archives verified exactly; CRC and deterministic re-encode.
- Fresh benchmark of Human23 and WCC13: all 119/182/231/234 roundtrips PASS; see `benchmarks-r10/main234_promotion_check.csv`.

| Corpus | 119 payload bits | 182 | 231 | **234** |
|---|---:|---:|---:|---:|
| Human23, 1718 plies | 6561 | 6530 | 6504 | **6497** |
| WCC13, 1155 plies | 4621 | 4584 | 4553 | **4548** |

## Next research branch

- Keep `MID_DEFAULT_PROFILE = 234` fixed as the measurable baseline.
- Compare `action type → origin → destination` against `action type → destination → origin` and a hierarchical destination-first model, using symmetric online context and integer CDF construction.
- Use holdout games never examined during tuning; measure actual arithmetic payload bits, full bytes, phase CDF log loss and encode/decode throughput.
- Reject variants with inconsistent decoder output, significant random-legal regression, or unacceptable speed regression. No new algorithmic improvements were claimed in *this* promotion-only change.
