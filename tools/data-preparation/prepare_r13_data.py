import sys,io,json,hashlib,collections
from pathlib import Path
import chess,chess.pgn
import argparse
ap=argparse.ArgumentParser();ap.add_argument('pgn_directory',type=Path);args=ap.parse_args()
r=Path(__file__).resolve().parents[2];out=r/'data-r13';old=set()
for f in r.rglob('*'):
 if 'data-r13' not in f.parts and f.is_file() and (f.name.endswith('.uci') or f.name.endswith('.uci-bundle')):
  for line in f.read_text(errors='ignore').splitlines():
   if line.strip():old.add(' '.join(line.split()))
held={'bullet_open_b','bullet1700_b'};records=[];seen=set();stats={};groups=collections.defaultdict(list)
for p in sorted(args.pgn_directory.glob('*.pgn')):
 src=p.stem;stream=io.StringIO(p.read_text(errors='replace'));valid=[];reasons=collections.Counter();raw=0
 while(g:=chess.pgn.read_game(stream)) is not None:
  raw+=1
  if g.errors or g.headers.get('Variant','Standard') not in ['Standard','Chess'] or g.board().fen()!=chess.STARTING_FEN:reasons['variant_or_parse']+=1;continue
  moves=' '.join(m.uci() for m in g.mainline_moves());plies=len(moves.split())
  if plies<16 or g.headers.get('Result') not in ['1-0','0-1','1/2-1/2']:reasons['short_or_unfinished']+=1;continue
  if moves in old or moves in seen:reasons['duplicate']+=1;continue
  seen.add(moves);h=hashlib.sha256(moves.encode()).hexdigest();valid.append({'source':src,'sha256':h,'plies':plies,'headers':dict(g.headers),'uci':moves})
 valid=sorted(valid,key=lambda x:x['sha256'])[:200]
 if src in held:groups['source_holdout']+=valid
 else:
  n=len(valid);a=3*n//5;b=4*n//5;groups['dev']+=valid[:a];groups['select']+=valid[a:b];groups['sealed']+=valid[b:]
 stats[src]={'raw':raw,'usable_before_cap':raw-sum(reasons.values()),'selected':len(valid),'rejections':dict(reasons)}
 for x in valid:records.append(x)
# Deterministic order, kept consistent across model comparisons.
manifest={'split_policy':'source holdout: bullet_open_b and bullet1700_b. Other sources: SHA256(UCI) sorted, 60/20/20 per source; cap200 per source before split','prior_archive_overlap_removed':True,'candidate_metrics_not_used_for_split':True,'source_stats':stats,'source_downloads':json.loads((args.pgn_directory/'download-log.json').read_text()),'groups':{}}
if not any(x['name']=='blitz1300' for x in manifest['source_downloads']):
 manifest['source_downloads'].append({'name':'blitz1300','id':'z7wVLKDI','url':'https://lichess.org/api/tournament/z7wVLKDI/games?clocks=false&evals=false&opening=false','bytes':(args.pgn_directory/'blitz1300.pgn').stat().st_size,'ok':True})
for name in ['dev','select','sealed','source_holdout']:
 gs=groups[name];gs.sort(key=lambda x:(x['source'],x['sha256']));(out/(name+'.uci-bundle')).write_text('\n'.join(x['uci'] for x in gs)+'\n');manifest['groups'][name]=[{k:v for k,v in x.items() if k!='uci'} for x in gs];print(name,len(gs),sum(x['plies'] for x in gs))
players={name:{x['headers'][side] for x in gs for side in ['White','Black']} for name,gs in groups.items()}
manifest['unique_players']=len(set.union(*players.values()));manifest['player_overlap']={a+'__'+b:len(players[a]&players[b]) for a,b in [('dev','select'),('dev','sealed'),('dev','source_holdout')]}
manifest['total_selected']=len(records);manifest['total_plies']=sum(x['plies'] for x in records)
(out/'manifest.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False));print('total',len(records),'players',manifest['unique_players'],'overlap',manifest['player_overlap'])
