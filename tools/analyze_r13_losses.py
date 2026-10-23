#!/usr/bin/env python3
"""Paired development-only loss audit; never reads sealed traces."""
import csv,json,math
from collections import defaultdict
from pathlib import Path
root=Path(__file__).resolve().parents[1]; out=root/'benchmarks-r13'
m=json.loads((root/'data-r13/manifest.json').read_text())
a=list(csv.DictReader((out/'dev_loss234.csv').open()))
b=list(csv.DictReader((out/'dev_loss250.csv').open()))
assert len(a)==len(b)
groups=defaultdict(list)
for x,y in zip(a,b):
    assert (x['game'],x['ply'],x['uci'])==(y['game'],y['ply'],y['uci'])
    h=m['groups']['dev'][int(y['game'])]; phase=['opening','middle','end'][int(y['phase'])]
    piece=['?','pawn','knight','bishop','rook','queen','king'][int(y['piece'])]
    capture='capture' if int(y['capture']) else 'quiet'
    elo=(int(h['headers'].get('WhiteElo',0))+int(h['headers'].get('BlackElo',0)))//2
    for category,key in [('phase',phase),('source',h['source']),('time',h['headers']['TimeControl']),('rating',str(elo//500*500)),('phase_piece_action',f'{phase}/{piece}/{capture}')]:
        groups[(category,key)].append((float(x['bits']),float(y['bits']),int(y['candidates'])))
with (out/'loss-groups.csv').open('w') as f:
    w=csv.writer(f);w.writerow(['category','group','moves','bits234','bits250','mean250','delta250_minus234','tail6_count','uniform_bits'])
    for (category,key),v in sorted(groups.items()):
        p=sum(x[0] for x in v);q=sum(x[1] for x in v)
        w.writerow([category,key,len(v),p,q,q/len(v),q-p,sum(x[1]>=6 for x in v),sum(math.log2(x[2]) for x in v)])
with (out/'top-loss-moves.csv').open('w') as f:
    w=csv.writer(f);w.writerow(['game','ply_zero_based','site','source','uci','bits250','bits234','candidates','rank','capture','in_check','gives_check'])
    pairs=sorted(zip(a,b),key=lambda p:float(p[1]['bits']),reverse=True)
    for x,y in pairs[:100]:
        h=m['groups']['dev'][int(y['game'])]
        w.writerow([y['game'],y['ply'],h['headers']['Site'],h['source'],y['uci'],y['bits'],x['bits'],y['candidates'],y['rank'],y['capture'],y['in_check'],y['gives_check']])
print('Paired loss audit:',len(a),'moves;',len(groups),'groups')
