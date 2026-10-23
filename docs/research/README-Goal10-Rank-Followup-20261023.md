# Portable rank follow-up — 2026-10-23

Two alternatives start from the retained counted-rank build, without model or archive changes and without CPU-native flags.

Index candidate: combine score/class/histogram construction, store candidate indices and classes in uint8_t, scores in int8_t, and bin offsets in uint16_t. Preserve the entire 0..256 candidate domain: index255 is valid and offsets may reach256. Only initialized array prefixes are read. Two constexpr tables replace exactly the geometry and seven tactical-score additions. Half centralization values are exact because both original coordinate sums are even. This candidate has no threshold histograms.

Threshold candidate: retain the original arithmetic score calculation, but discard the ordered candidate list entirely. Global and class score histograms yield only the observable rank predicates. The global cutoff is the score containing zero-based order statistic floor(n/4); the class cutoff uses floor(class_size/3). Taking whole tied-score groups at the cutoff reproduces the original strictly-better comparisons. Maxima reproduce the two best-score predicates. Counts cover scores -15..26 and use uint16_t to support 256 identical entries. Empty input reads no histogram entries. This alternative does not include the index candidate's feature tables.

Both candidates passed comparison against the original comparator-sort function on 1,044,976 synthetic candidates in 8,192 sets, sizes0..256, arbitrary tactical/capture flags and ties. Each also passed 15 three-arm mode/dataset archive byte identities and all 1,226 codec checks. There were 108 screening CLI calls in total, followed by 48 fresh confirmation calls for one selected candidate. Every speed invocation warms and checks repeated bytes and roundtrip.

First CPU0 screen, index versus frozen anchor: encode +11.820128%, decode +8.093353%. Versus retained counted ranks in the same blocks: +1.052476% and +0.571218%.

Second CPU0 screen used the same frozen anchor, index as stage1 and threshold as candidate. Paired equal-dataset aggregate gains versus anchor:

| Candidate | Encode | Decode |
|---|---:|---:|
| index | +10.278005% | +9.370086% |
| threshold | +4.839205% | +3.948158% |

The predeclared balanced-gain rule selected index for CPU1 confirmation. Fresh confirmation versus the same fixed anchor: encode +11.519372%, decode +7.175028%, whole-process CPU throughput +9.654765%. Adopted: True. Both +10% on this confirmation: False. Other candidate was not independently confirmed.

| Dataset | Encode | Decode |
|---|---:|---:|
| r19 | +8.898% | +7.159% |
| r18 | +13.218% | +7.425% |
| r17 | +12.491% | +6.942% |

All percentages are throughput gains from paired timings, with equal dataset weight. Gains from different studies are not multiplied. Timings are noisy, from one host, using CPU0 for screening and CPU1 for confirmation. Portable instruction targeting is retained; actual execution on other hardware was not tested. Raw protocols, samples, source/binary hashes and oracle records are in the two benchmark directories.
