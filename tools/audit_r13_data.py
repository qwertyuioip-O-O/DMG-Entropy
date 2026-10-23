#!/usr/bin/env python3
import hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
m=json.loads((root/'data-r13/manifest.json').read_text()); seen=set();old=set();games=plies=0
for p in root.rglob('*'):
    if p.is_file() and 'data-r13' not in p.parts and (p.name.endswith('.uci') or p.name.endswith('.uci-bundle')):
        old.update(' '.join(x.split()) for x in p.read_text(errors='replace').splitlines() if x.strip())
sources={}
for name,entries in m['groups'].items():
    lines=(root/f'data-r13/{name}.uci-bundle').read_text().splitlines();assert len(lines)==len(entries)
    sources[name]={e['source'] for e in entries}
    for line,e in zip(lines,entries):
        assert line==' '.join(line.split()) and line not in old
        h=hashlib.sha256(line.encode()).hexdigest();assert h==e['sha256'] and h not in seen
        seen.add(h);assert len(line.split())==e['plies'];games+=1;plies+=e['plies']
assert not sources['source_holdout'] & (sources['dev']|sources['select']|sources['sealed'])
assert games==m['total_selected'] and plies==m['total_plies']
print(json.dumps({'games':games,'plies':plies,'players':m['unique_players'],'unique_game_hashes':len(seen),'prior_games_overlap':0,'source_holdout_disjoint':True,'player_disjoint':False,'manifest_player_overlap':m['player_overlap']},indent=2))
