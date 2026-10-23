#!/usr/bin/env python3
"""Freeze new evaluation cohorts without examining codec metrics. Requires python-chess."""
import argparse,collections,gzip,hashlib,io,json,random,re,sys,time,urllib.request,urllib.parse
from pathlib import Path
import chess,chess.pgn
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'data-cohorts-20261023';SRC=OUT/'sources';SRC.mkdir(parents=True,exist_ok=True)
def sha(b):return hashlib.sha256(b).hexdigest()
def prior():
 p=OUT/'prior-game-hashes.json'
 if p.exists():return set(json.loads(p.read_text()))
 old=set()
 for f in ROOT.rglob('*'):
  if f.is_file() and OUT not in f.parents and (f.suffix=='.uci' or f.name.endswith('.uci-bundle')):
   old.update(sha(' '.join(x.split()).encode()) for x in f.read_text(errors='replace').splitlines() if x.strip())
 p.write_text(json.dumps(sorted(old)));return old
def get(url,dest):
 if dest.exists():
  b=gzip.decompress(dest.read_bytes());meta=dest.with_suffix(dest.suffix+'.source.json')
  if not meta.exists():meta.write_text(json.dumps({'url':url,'raw_sha256':sha(b),'raw_bytes':len(b)}))
  return b
 for attempt in range(3):
  try:
   with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'DMG-Entropy-evaluation/1.0'}),timeout=60) as f:b=f.read()
   dest.write_bytes(gzip.compress(b,mtime=0));dest.with_suffix(dest.suffix+'.source.json').write_text(json.dumps({'url':url,'raw_sha256':sha(b),'raw_bytes':len(b)}));return b
  except urllib.error.HTTPError as e:
   if e.code==429 and attempt<2:time.sleep(15);continue
   raise
 raise RuntimeError(url)
def candidates(blob,kind,source,old):
 stream=io.StringIO(blob.decode('utf-8-sig',errors='replace'));rows=[];reject=collections.Counter();raw=0
 class HeaderFilter(chess.pgn.GameBuilder):
  def end_headers(self):
   h=self.game.headers
   try:elo=[int(h.get(k,'?')) for k in ['WhiteElo','BlackElo']]
   except ValueError:return chess.pgn.SKIP
   if kind=='elite' and (min(elo)<2600):return chess.pgn.SKIP
   if kind=='general' and (not all(800<=x<=2200 for x in elo) or any(h.get(k) for k in ['WhiteTitle','BlackTitle'])):return chess.pgn.SKIP
   return super().end_headers()
 while (g:=chess.pgn.read_game(stream,Visitor=HeaderFilter)) is not None:
  raw+=1;h=dict(g.headers)
  try:ratings=[int(h.get(k,'?')) for k in ['WhiteElo','BlackElo']]
  except ValueError:reject['missing_rating']+=1;continue
  if kind=='general' and (not all(800<=x<=2200 for x in ratings) or any(h.get(k) for k in ['WhiteTitle','BlackTitle'])):reject['rating_or_title']+=1;continue
  if kind=='general' and (not h.get('TimeControl','').split('+')[0].isdigit() or int(h['TimeControl'].split('+')[0])<60):reject['clock_under_60s_or_unknown']+=1;continue
  if kind=='elite' and (min(ratings)<2600):reject['rating_below_2600']+=1;continue
  if g.errors or g.headers.get('Variant','Standard') not in ['Standard','Chess'] or g.board().fen()!=chess.STARTING_FEN:reject['parse_or_nonstandard']+=1;continue
  moves=' '.join(m.uci() for m in g.mainline_moves());n=len(moves.split());key=sha(moves.encode())
  if n<16 or h.get('Result') not in ['1-0','0-1','1/2-1/2']:reject['short_or_unfinished']+=1;continue
  if key in old:reject['prior_exact_game']+=1;continue
  rows.append({'source':source,'headers':h,'plies':n,'sha256':key,'uci':moves})
 rows.sort(key=lambda x:x['sha256']);return rows[:1000],{'raw':raw,'eligible_before_cap':len(rows),'cap':1000,'rejected':dict(reject)}
def freeze(kind,pools,logs):
 if (OUT/kind/'manifest.json').exists():raise RuntimeError('cohort is already frozen; do not overwrite existing evaluation inputs')
 chosen=[];seen=prior();queues={k:list(v) for k,v in sorted(pools.items())}
 # Round-robin across fixed sources; per-source rows sorted by hash, no codec metrics.
 while len(chosen)<5000:
  advanced=False
  for source,q in queues.items():
   while q and q[0]['sha256'] in seen:q.pop(0)
   if q:
    x=q.pop(0);chosen.append(x);seen.add(x['sha256']);advanced=True
    if len(chosen)==5000:break
  if not advanced:break
 if len(chosen)<5000:raise RuntimeError(f'{kind}: only {len(chosen)} eligible distinct games; preserve download logs and extend sources')
 chosen.sort(key=lambda x:sha(('cohort-order-v1:'+x['sha256']).encode()))
 folder=OUT/kind;blocks=folder/'blocks';blocks.mkdir(parents=True,exist_ok=True)
 lines=[x['uci'] for x in chosen];(folder/'all.uci-bundle').write_text('\n'.join(lines)+'\n')
 for block in range(20):(blocks/f'{block:02}.uci-bundle').write_text('\n'.join(lines[block*250:(block+1)*250])+'\n')
 entries=[{k:v for k,v in x.items() if k!='uci'} for x in chosen]
 (folder/'entries.jsonl.gz').write_bytes(gzip.compress(('\n'.join(json.dumps(x,ensure_ascii=False) for x in entries)+'\n').encode(),mtime=0))
 rates=[int(x['headers'][side]) for x in chosen for side in ['WhiteElo','BlackElo'] if side in x.get('headers',{}) and x['headers'][side].isdigit()]
 players={x['headers'][side] for x in chosen for side in ['White','Black'] if side in x.get('headers',{})}
 manifest={'kind':kind,'games':len(chosen),'plies':sum(x['plies'] for x in chosen),'players':len(players),'sources':dict(collections.Counter(x['source'] for x in chosen)),'rating_min':min(rates) if rates else None,'rating_max':max(rates) if rates else None,'all_uci_sha256':sha((folder/'all.uci-bundle').read_bytes()),'source_downloads':logs,'prior_exact_game_overlap':0,'duplicates':0,'blocks':[{'path':str(p.relative_to(OUT)),'sha256':sha(p.read_bytes()),'games':250} for p in sorted(blocks.glob('*.uci-bundle'))]}
 (folder/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2));print('FROZEN',kind,len(chosen),manifest['plies'],flush=True)
def prepare_general():
 old=prior();listing=json.loads(Path(sys.argv[2]).read_text());pools={};logs=[]
 for t in sorted(listing['finished']+listing['started'],key=lambda x:x['id']):
  if t['variant']['key']!='standard' or not t.get('rated') or t['clock']['limit']<60:continue
  ident=t['id'];u=f'https://lichess.org/api/tournament/{ident}/games?clocks=false&evals=false&opening=false';dest=SRC/f'general-{ident}.pgn.gz'
  try:
   b=get(u,dest);pools[ident],stats=candidates(b,'general',ident,old);logs.append({'id':ident,'url':u,'snapshot_tournament':t,'raw_bytes':len(b),'raw_sha256':sha(b),**stats});print('general',ident,len(pools[ident]),flush=True)
  except Exception as e:logs.append({'id':ident,'url':u,'error':str(e)});print('source failed',ident,str(e),flush=True)
  (OUT/'general-downloads.json').write_text(json.dumps(logs,ensure_ascii=False,indent=2))
 # Supplement with predetermined public archives of arena participants, not old evaluation games.
 participants=sorted({x['headers'][side] for rows in pools.values() for x in rows for side in ['White','Black']},key=lambda x:sha(('participant-v1:'+x.lower()).encode()))[:16]
 (OUT/'general-participant-supplement.json').write_text(json.dumps({'reason':'initial arena snapshot has fewer than 5000 eligible unique games','planned_users':participants,'max_games_per_archive':150,'rated':True,'minimum_base_seconds':60,'chosen_before_any_codec_metrics':True},indent=2))
 for user in participants:
  ident='user-'+user;u='https://lichess.org/api/games/user/'+urllib.parse.quote(user)+'?max=150&rated=true&perfType=bullet,blitz,rapid,classical&clocks=false&evals=false&opening=false'
  try:
   b=get(u,SRC/f'general-{ident}.pgn.gz');pools[ident],stats=candidates(b,'general',ident,old);logs.append({'id':ident,'url':u,'raw_bytes':len(b),'raw_sha256':sha(b),**stats});print('general archive',ident,len(pools[ident]),flush=True)
  except Exception as e:logs.append({'id':ident,'url':u,'error':str(e)});print('source failed',ident,str(e),flush=True)
  (OUT/'general-downloads.json').write_text(json.dumps(logs,ensure_ascii=False,indent=2))
 freeze('general',pools,logs)
def prepare_elite():
 old=prior();pools={};logs=[];seen_tours=set();links={}
 # Discover fixed public FIDE profile index pages, prioritizing larger events by title.
 for fid,slug in [('2020009','Caruana_Fabiano'),('1503014','Carlsen_Magnus')]:
  for page in range(1,7):
   profile=f'https://lichess.org/fide/{fid}/{slug}?page={page}'
   cache=SRC/(f'elite-index-{page}.html.gz' if fid=='2020009' else f'elite-index-{fid}-{page}.html.gz')
   html=get(profile,cache).decode()
   for link in re.findall(r'href="(/broadcast/[^"?]+)"',html):
    if re.search(r'/[A-Za-z0-9]{8}$',link) and link.count('/')>=4:links.setdefault(link,profile)
   print('elite index',fid,page,'events',len(links),flush=True)
 def priority(link):
  low=link.lower()
  return (0 if any(x in low for x in ['olympiad','world-rapid','world-blitz','world-team','grand-swiss','bundesliga','world-cup']) else 1,link)
 (OUT/'elite-discovery.json').write_text(json.dumps({'profiles':['https://lichess.org/fide/2020009/Caruana_Fabiano','https://lichess.org/fide/1503014/Carlsen_Magnus'],'pages_per_profile':6,'events':links,'order':'larger event-name categories first, then URL; fixed before model metrics','stop':'at least6500 unique eligible games or exhaust available candidates'},indent=2))
 for link in sorted(links,key=priority):
  try:
   profile=links[link];roundid=link.rsplit('/',1)[1];body=get('https://lichess.org'+link,SRC/f'elite-round-{roundid}.html.gz').decode();m=re.search(r'"tour":\{"id":"([A-Za-z0-9]{8})"',body)
   if not m:continue
   tour=m.group(1)
   if tour in seen_tours:continue
   seen_tours.add(tour);dest=SRC/f'elite-{tour}.pgn.gz';u=f'https://lichess.org/api/broadcast/{tour}.pgn'+('' if dest.exists() else '?clocks=false&comments=false');b=get(u,dest);u=json.loads(dest.with_suffix(dest.suffix+'.source.json').read_text())['url']
   pools[tour],stats=candidates(b,'elite',tour,old);logs.append({'id':tour,'url':u,'discovered_from':profile,'round_page':'https://lichess.org'+link,'raw_bytes':len(b),'raw_sha256':sha(b),**stats});print('elite',tour,len(pools[tour]),'total',sum(len(x) for x in pools.values()),flush=True)
  except Exception as e:logs.append({'round_page':link,'error':str(e)});print('source failed',link,str(e),flush=True)
  (OUT/'elite-downloads.json').write_text(json.dumps(logs,ensure_ascii=False,indent=2))
  if len({x['sha256'] for rows in pools.values() for x in rows})>=6500:break
 freeze('elite',pools,logs)
def prepare_random():
 rows=[];old=prior();seen=set();term=collections.Counter()
 for i in range(5000):
  seed=int(sha(f'DMG-cohort-random-v1:{i}'.encode()),16);rng=random.Random(seed);b=chess.Board();moves=[];counts=collections.Counter({b._transposition_key():1});reason='cap512'
  for ply in range(512):
   legal=list(b.legal_moves)
   if not legal:reason='checkmate' if b.is_check() else 'stalemate';break
   if b.is_insufficient_material():reason='insufficient_material';break
   if b.halfmove_clock>=150:reason='75_move';break
   if counts[b._transposition_key()]>=5:reason='fivefold';break
   m=legal[rng.randrange(len(legal))];moves.append(m.uci());b.push(m);counts[b._transposition_key()]+=1
  uci=' '.join(moves);key=sha(uci.encode());assert key not in old and key not in seen;seen.add(key);term[reason]+=1
  rows.append({'source':'uniform-legal-v1','headers':{},'plies':len(moves),'sha256':key,'seed_index':i,'termination':reason,'uci':uci})
  if (i+1)%250==0:print('random',i+1,flush=True)
 freeze('random',{'uniform':rows},[{'algorithm':'python-chess legal moves + exact uniform random.randrange; independent SHA256-derived seed per game','python_chess_version':chess.__version__,'max_plies':512,'terminations':dict(term)}])
if __name__=='__main__':
 command=sys.argv[1]
 {'general':prepare_general,'elite':prepare_elite,'random':prepare_random}[command]()
