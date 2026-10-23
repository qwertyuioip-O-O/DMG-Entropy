# Three-cohort R19 evaluation — 2026-10-23

Frozen R19 builtin mode3 / g10static. 5,000 games per cohort; 20 equal files of 250 games. No retraining or candidate selection from these outcomes. Native overall speed uses the untouched selected executable; phase speeds use the separately instrumented same-model build.

## Whole-file results

| Cohort | Games | Plies | Actual payload bit/ply | Framed file bit/ply | Native encode plies/s | Native decode plies/s |
|---|---:|---:|---:|---:|---:|---:|
| general | 5,000 | 347,515 | 3.3470 | 3.4749 | 78,025 | 77,816 |
| elite | 5,000 | 455,147 | 3.5029 | 3.6099 | 77,231 | 76,925 |
| random | 5,000 | 1,744,692 | 4.4612 | 4.5075 | 101,053 | 102,856 |

## Phase results

CDF costs below are -log2 of the actual integer coding probabilities, not separately framed per-phase byte streams. Diagnostic phase timers cover move prediction, arithmetic coding, learning and board advancement; per-game initialization and framing/CRC are outside those phase timers. Timestamp calls occur only on phase changes and game ends; the per-ply counter/branch remains. Do not directly equate a diagnostic phase speed with untouched native whole-file speed.

| Cohort | Phase | Plies | CDF bit/ply (95% block interval) | Diagnostic encode plies/s | Diagnostic decode plies/s |
|---|---|---:|---:|---:|---:|
| general | opening | 116,925 | 3.5363 (3.5225–3.5489) | 86,199 | 86,640 |
| general | middlegame | 161,076 | 3.3246 (3.3148–3.3348) | 60,849 | 61,190 |
| general | endgame | 69,514 | 3.0802 (3.0589–3.1027) | 144,545 | 145,147 |
| elite | opening | 118,208 | 3.1861 (3.1709–3.2021) | 82,793 | 83,433 |
| elite | middlegame | 222,406 | 3.7573 (3.7449–3.7699) | 57,732 | 57,386 |
| elite | endgame | 114,533 | 3.3358 (3.3233–3.3483) | 138,015 | 138,230 |
| random | opening | 119,741 | 4.9242 (4.9180–4.9308) | 87,113 | 88,252 |
| random | middlegame | 632,869 | 5.3452 (5.3417–5.3487) | 61,406 | 62,002 |
| random | endgame | 992,082 | 3.8414 (3.8317–3.8512) | 164,374 | 166,415 |

## Interpretation and limitations

- Original small sets were 1,317 online arena games / 89,230 plies, not a three-population validation. Earlier 3.2-bit middlegame figures must not be generalized to elite or random play.
- General cohort: untitled rated online users, both online Elo 800..2200, base clock >=60s, arena exports plus deterministic participant-history supplementation. This is not a random sample of all humans or classical chess.
- Elite cohort: real broadcast tournament games plus public world-leading-player PGN archives from kinairi/chess-data, frozen at a Git commit; both players must have PGN-reported Elo >=2600; GM/FIDE-ID tag coverage is reported because historical exports omit tags. All qualifying games in each selected archive/event are candidates; max1000 per source before global dedup and source balancing. The rating/time-control context differs from online arena ratings; event/player-archive selection, secondary mirror provenance and historical date range remain sampling limitations.
- Random cohort: exactly uniform legal-move sampling per ply with independent deterministic seeds; automatic endings or 512-ply cap. Capped games remain included. This tests a specified synthetic process, not every imaginable random distribution.
- Exact prior UCI games and within-cohort duplicates are excluded. Players/events can be correlated, and rating/title/identity tags are source-provided and some identity tags are missing.
- All 120 native calls and 360 instrumented phase rows are retained; no slow runs trimmed. Repeated native run differences are reported in JSON and can include scheduling/frequency noise.
- Intervals resample entire 250-game blocks (10,000 fixed-seed draws). They describe variation conditional on these selected cohorts, not independent-event/player population confidence. One host/core is tested.
- Phase definition matches the codec: opening if ply<24 and weighted material phase >=18; endgame if material phase <=8; otherwise middlegame. Material phase = min(24, knights + bishops + 2*rooks + 4*queens), both sides.
- Every native speed call checks deterministic encoding and exact roundtrip. Every phase invocation checks all repeated bytes/roundtrips and phase counts. Byte identity with the selected native executable is explicitly checked on the first block of each cohort; source proof confirms removal of only timing hooks restores the original wrapper.

Frozen inputs, hashes, rating/source metadata and exclusions are in `data-cohorts-20261023`. Detailed intervals and all times are in `summary.json`, `phase.csv`, `native.csv`, and `protocol.json`.

## Dataset audit and provenance

| Cohort | Sources | Named players | Recorded rating range | Both GM tags | Both FIDE ID tags | Secondary archive games |
|---|---:|---:|---:|---:|---:|---:|
| general | 33 | 3328 | 801–2200 | 0 | 0 | 0 |
| elite | 25 | 442 | 2600–2909 | 750 | 708 | 4041 |
| random | 1 | 0 | N/A | 0 | 0 | 0 |

Missing historical identity/title tags were not invented or silently counted as verified. Elite names/ratings are source metadata; every selected player-archive game contains the archive focal player name. All selected UCI sequences are unique, disjoint from prior stored game sequences, and mutually disjoint between these three cohorts. Players and opening positions can still recur.

Master archive mirror: https://github.com/kinairi/chess-data/tree/d9f3ccb8f3335d3d7c4465c252533ba0431062f2/data/players . This is a secondary public archive, not an official FIDE certification. Both reported ratings must be >=2600. Recorded year distributions and time-control coverage, including missing fields, are in data-cohorts-20261023/audit.json. General online and elite archive rating systems/time controls are not interchangeable.

Source sequences encode standard-start move lists only. Original PGN player/date/rating/clock/comment tags are not encoded. Framed bytes include the codec model ID, game lengths, payload and checksum. Whole-file throughput is total plies divided by total mean phase time over the equal-sized blocks, not a median across different datasets.

Once these results are inspected for future model changes, this corpus becomes a regression set; a future independent generalization claim needs new data.

CPU: AMD EPYC 9V74 80-Core Processor, one allowed logical core (CPU1). No frequency lock or other physical CPU replication is claimed. Native repeat max/min >1.25 counts: general: 1/40, elite: 0/40, random: 0/40. All timings retained.
