#!/usr/bin/env python3
import hashlib,json
from pathlib import Path
r=Path(__file__).resolve().parents[2];m=json.loads((r/'data-r15/manifest.json').read_text());old=set()
for p in r.rglob('*'):
    if p.is_file() and 'data-r15' not in p.parts and (p.name.endswith('.uci') or p.name.endswith('.uci-bundle')):
        old.update(' '.join(x.split()) for x in p.read_text(errors='replace').splitlines() if x.strip())
lines=(r/'data-r15/heldout.uci-bundle').read_text().splitlines();assert len(lines)==len(m['entries'])==m['games'];seen=set();plies=0
for line,e in zip(lines,m['entries']):
    assert line==' '.join(line.split()) and line not in old and line not in seen
    seen.add(line);assert hashlib.sha256(line.encode()).hexdigest()==e['sha256'] and len(line.split())==e['plies'];plies+=e['plies']
assert plies==m['plies']
prior=json.loads((r/'data-r13/manifest.json').read_text());old_ids={x['id'] for x in prior['source_downloads']}|{x['id'] for x in json.loads((r/'data-r14/manifest.json').read_text())['source_downloads']};new_ids={x['id'] for x in m['source_downloads']};assert not old_ids&new_ids
for source in m['source_stats']:
    path=r/f'data-r15/{source}.uci-bundle'
    if path.exists():assert path.read_text().splitlines()==[line for line,e in zip(lines,m['entries']) if e['source']==source]
print(json.dumps({'games':len(lines),'plies':plies,'players':m['players'],'prior_full_game_overlap':0,'source_ids_disjoint':True,'player_disjoint':False,'player_overlap':m['player_overlap'],'hashes_and_strata':'PASS'},indent=2))
