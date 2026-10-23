#!/usr/bin/env python3
"""Supplement real broadcast games with immutable public master-player PGN archives."""
import gzip,json,re,urllib.request
from pathlib import Path
from prepare_cohort_eval import ROOT,OUT,SRC,prior,get,sha,candidates,freeze
repo='kinairi/chess-data'
meta=OUT/'elite-archive-commit.json'
if not meta.exists():
 info=json.load(urllib.request.urlopen(f'https://api.github.com/repos/{repo}/commits/master',timeout=30));meta.write_text(json.dumps({'repository':repo,'commit':info['sha'],'commit_date':info['commit']['committer']['date'],'archive_players':['Carlsen','Caruana','Anand','Aronian','Nakamura','Giri','Ding','VachierLagrave'],'chosen_before_codec_metrics':True},indent=2))
plan=json.loads(meta.read_text());old=prior();pools={};logs=[]
for dest in sorted(SRC.glob('elite-*.pgn.gz')):
 ident=dest.name[len('elite-'):-len('.pgn.gz')]
 if ident.startswith('archive-'):continue
 b=gzip.decompress(dest.read_bytes());u=f'https://lichess.org/api/broadcast/{ident}.pgn';side=dest.with_suffix(dest.suffix+'.source.json')
 if side.exists():u=json.loads(side.read_text())['url']
 pools[ident],stats=candidates(b,'elite',ident,old);logs.append({'id':ident,'url':u,'raw_bytes':len(b),'raw_sha256':sha(b),**stats});print('broadcast cached',ident,len(pools[ident]),flush=True)
for player in plan['archive_players']:
 ident='archive-'+player;u=f'https://raw.githubusercontent.com/{repo}/{plan["commit"]}/data/players/{player}.pgn'
 try:
  b=get(u,SRC/f'elite-{ident}.pgn.gz');pools[ident],stats=candidates(b,'elite',ident,old);logs.append({'id':ident,'url':u,'mirror_repository':repo,'commit':plan['commit'],'raw_bytes':len(b),'raw_sha256':sha(b),**stats});print('archive',player,len(pools[ident]),flush=True)
 except Exception as e:logs.append({'id':ident,'url':u,'error':str(e)});print('archive failed',player,str(e),flush=True)
 (OUT/'elite-archive-downloads.json').write_text(json.dumps(logs,indent=2))
freeze('elite',pools,logs)
