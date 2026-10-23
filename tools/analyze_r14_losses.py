#!/usr/bin/env python3
import csv
from collections import defaultdict
from pathlib import Path
r=Path(__file__).resolve().parents[1];out=r/'benchmarks-r14'
names=['control95','rate1','candidate'];traces=[list(csv.DictReader((out/f'dev-loss-{name}.csv').open())) for name in names]
groups=defaultdict(list)
for rows in zip(*traces):
    assert len({(x['game'],x['ply'],x['uci']) for x in rows})==1
    row=rows[-1];phase=['opening','middle','end'][int(row['phase'])];piece=['?','pawn','knight','bishop','rook','queen','king'][int(row['piece'])]
    action='capture' if int(row['capture']) else 'quiet';values=tuple(float(x['bits']) for x in rows)
    for group in [phase,f'{phase}/{piece}/{action}']:groups[group].append(values)
assert len({len(x) for x in traces})==1
with (out/'loss-groups.csv').open('w') as f:
    w=csv.writer(f);w.writerow(['group','moves','bits95','bits_rate1','bits_candidate','feature_delta','total_delta','candidate_mean','tail6_candidate'])
    for key,vs in sorted(groups.items()):
        sums=[sum(x[i] for x in vs) for i in range(3)]
        w.writerow([key,len(vs),*sums,sums[2]-sums[1],sums[2]-sums[0],sums[2]/len(vs),sum(x[2]>=6 for x in vs)])
print('PASS paired development loss audit:',len(traces[0]),'moves')
