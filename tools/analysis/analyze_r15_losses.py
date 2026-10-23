#!/usr/bin/env python3
import argparse,csv
from collections import defaultdict
from pathlib import Path
r=Path(__file__).resolve().parents[2];out=r/'benchmarks-r15'
ap=argparse.ArgumentParser();ap.add_argument('--directory',type=Path,default=out);out=ap.parse_args().directory
names=['baseline','rate1','candidate'];traces=[list(csv.DictReader((out/f'dev-loss-{name}.csv').open())) for name in names]
assert len({len(x) for x in traces})==1
groups=defaultdict(list)
for rows in zip(*traces):
    assert len({(x['game'],x['ply'],x['uci']) for x in rows})==1
    x=rows[0];phase=['opening','middle','end'][int(x['phase'])];piece=['?','pawn','knight','bishop','rook','queen','king'][int(x['piece'])];action='capture' if int(x['capture']) else 'quiet'
    values=tuple(float(y['bits']) for y in rows)
    for key in [phase,f'{phase}/{piece}/{action}']:groups[key].append(values)
with (out/'loss-groups.csv').open('w') as f:
    w=csv.writer(f);w.writerow(['group','moves','bits252','split_capture_rate1','split_capture_rate4','split_only_delta','capture_rate_delta','total_delta'])
    for key,vs in sorted(groups.items()):
        a,b,c=[sum(x[i] for x in vs) for i in range(3)];w.writerow([key,len(vs),a,b,c,b-a,c-b,c-a])
print('PASS paired development loss analysis',len(traces[0]),'moves')
