# R19 extended speed study — 2026-10-23

R19 has not demonstrated a speed recovery against the original R18 executable on this host. Its internal paired comparison is mildly positive, but the actual executable comparison is approximately 3% slower. Keep R19 as a compression research option; the general pack default remains profile234. No codec or learned model was changed for this study.

## Results

Positive percentages mean higher R19 throughput. Ratios use paired total times, followed by a geometric mean; percentages are not differences between separately reported medians.

| Comparison | Encode | Decode |
|---|---:|---:|
| In-process CPU time, six equally weighted sessions | +2.262% | +3.247% |
| Exploratory 95% interval | +0.390% to +4.204% | +0.627% to +6.307% |
| Actual original R18 CLI vs R19 CLI | −2.979% | −3.077% |
| Actual CLI blocks won by R19 | 2/6 | 1/6 |

Actual CLI median throughput: R18 45,531 encode / 44,981 decode ply/s; R19 44,085 / 43,785 ply/s. The 60–70K target was not confirmed on this host. Whole-process child CPU throughput was 3.365% lower for R19, including loading, warm-up and validation.

| Fixed core / dataset | Encode CPU gain | Decode CPU gain |
|---|---:|---:|
| CPU0 / new422 | +4.004% | +6.760% |
| CPU1 / R18 heldout456 | −0.267% | −0.545% |
| CPU0 / R17 heldout439 | +4.059% | +3.537% |
| CPU1 / new422 | +2.539% | +2.031% |
| CPU0 / R18 heldout456 | +1.666% | +7.533% |
| CPU1 / R17 heldout439 | +1.637% | +0.427% |

Both directions improved in only five of six strata, so the predeclared all-strata decision rule also failed. There is substantial block-to-block variation. These observations do not establish performance on other hardware or future workloads.

## Protocol and verification

The full evidence is in `benchmarks-r19-speed-20261023/`. `protocol.json` was written before measurements and records binary, source and dataset hashes. The two first distinct exposed cores, CPU0 and CPU1, were selected before timing. Six sequential sessions cover two cores × three datasets: 422 games / 28,975 plies; 456 / 30,319; and 439 / 29,936.

Each in-process session has eight alternating ABBA/BAAB blocks per direction, comparing embedded exact R18 mode0 against R19 mode3. There are 384 individual timing rows and 48 blocks per direction. Warm validation, repeated compressed-byte equality and roundtrip checks occur outside the measured interval. CPU and wall time are recorded. No build, training, tests or other benchmark ran concurrently.

The separate actual-executable test uses original `r18_prior` mode9 and `r19_prior` mode3 on CPU0/new422: six alternating ABBA/BAAB blocks, 24 processes, each with three timed iterations per direction. CLI phase timings are wall time; per-child CPU and wall totals additionally cover the whole process. This comparison is kept separate from the diagnostic in-process results.

The exploratory intervals use 20,000 hierarchical bootstrap draws with seed20261023, resampling six sessions and then whole blocks. Calls inside each ABBA block are not treated as independent samples. Serial sessions on fixed data and hardware limit the interpretation of these intervals.

Binary hashes before and after agree, and frozen codec/model hashes agree. All benchmark validation checks passed. Output CSV row counts were checked before atomic writes. The recorded cgroup quota is eight CPUs; observed throttling deltas are zero. CPU frequency/governor information was unavailable, so a specific hardware or compiler cause is not established.

## Posthoc baseline control

After the primary result, a separate four-block ABBA/BAAB control compared the *same R18 model* in original R18 mode9 and embedded R19 mode0, on CPU0/new422. Both produce 12,350-byte containers; game lengths, payload bit counts and payload bytes agree after excluding the format header and CRC.

The embedded R18 path was 4.142% slower encoding and 8.695% slower decoding, winning respectively 1/4 and 0/4 blocks. Whole-process CPU throughput was 5.586% lower. See `reference-control-protocol.json`, `reference-control.csv` and `reference-control-summary.json`.

This control shows that equivalent model/payload behavior did not provide an equivalent performance baseline across these binaries in this measurement. It helps explain the conflicting internal and executable results, but does not identify the cause or supersede the primary actual-CLI result. No correction factor is applied.

## Reproduce

From the project root, choose a fresh output directory:

```sh
python3 tools/build_r18.py --out build-r18
python3 tools/build_r19.py --out build-r19 --paired
python3 tools/benchmark_r19_extended.py --binaries build-r19 --r18 build-r18/r18_prior --out speed-study-new
python3 tools/summarize_r19_extended.py speed-study-new
python3 tools/benchmark_r19_reference_control.py speed-study-new
```

The existing results use the binary paths and hashes recorded in the protocol, rather than claiming that later rebuilds have identical performance. Prior compression evidence remains in `README-R19.md`; this study does not repeat or broaden its validation scope.
