#!/usr/bin/env python3
"""Prepare a new, source-disjoint test set without examining model metrics."""
import argparse,collections,hashlib,io,json
from pathlib import Path
import chess,chess.pgn
ap=argparse.ArgumentParser();ap.add_argument('pgn_directory',type=Path);args=ap.parse_args()
r=Path(__file__).resolve().parents[2];out=r/'data-r17';out.mkdir(exist_ok=True);old=set()
for f in r.rglob('*'):
    if f.is_file() and 'data-r17' not in f.parts and (f.name.endswith('.uci') or f.name.endswith('.uci-bundle')):
        old.update(' '.join(x.split()) for x in f.read_text(errors='replace').splitlines() if x.strip())
selected=[];seen=set();stats={}
for p in sorted(args.pgn_directory.glob('*.pgn')):
    stream=io.StringIO(p.read_text(errors='replace'));raw=0;valid=[];rejected=collections.Counter()
    while (g:=chess.pgn.read_game(stream)) is not None:
        raw+=1
        if g.errors or g.board().fen()!=chess.STARTING_FEN or g.headers.get('Variant','Standard') not in ['Standard','Chess']:
            rejected['parse_or_variant']+=1;continue
        moves=' '.join(m.uci() for m in g.mainline_moves());n=len(moves.split())
        if n<16 or g.headers.get('Result') not in ['1-0','0-1','1/2-1/2']:
            rejected['short_or_unfinished']+=1;continue
        if moves in old or moves in seen:rejected['duplicate']+=1;continue
        seen.add(moves);valid.append({'source':p.stem,'headers':dict(g.headers),'plies':n,'sha256':hashlib.sha256(moves.encode()).hexdigest(),'uci':moves})
    valid.sort(key=lambda x:x['sha256']);chosen=valid[:120];selected+=chosen
    stats[p.stem]={'raw':raw,'usable_before_cap':len(valid),'selected':len(chosen),'rejections':dict(rejected)}
selected.sort(key=lambda x:(x['source'],x['sha256']))
(out/'heldout.uci-bundle').write_text('\n'.join(x['uci'] for x in selected)+'\n')
players={x['headers'][side] for x in selected for side in ['White','Black']}
prior=json.loads((r/'data-r13/manifest.json').read_text());overlap={}
for name in ['dev','select']:
    ps={x['headers'][side] for x in prior['groups'][name] for side in ['White','Black']};overlap[name]=len(players&ps)
manifest={'policy':'Completed standard games >=16 plies; prior exact UCI sequences excluded; at most120/source by SHA256; all selected tournament sources held out; no candidate metrics used for selection','source_downloads':json.loads((args.pgn_directory/'download-log.json').read_text()),'source_stats':stats,'games':len(selected),'plies':sum(x['plies'] for x in selected),'players':len(players),'player_overlap':overlap,'source_disjoint':True,'player_disjoint':False,'entries':[{k:v for k,v in x.items() if k!='uci'} for x in selected]}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False));print(json.dumps({k:v for k,v in manifest.items() if k not in ['entries','source_downloads']},indent=2))
