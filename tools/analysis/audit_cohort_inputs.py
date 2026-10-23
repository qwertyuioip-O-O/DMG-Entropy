#!/usr/bin/env python3
import collections,csv,gzip,hashlib,json,re,statistics
from pathlib import Path
R=Path(__file__).resolve().parents[2];D=R/'data-cohorts-20261023';old=set(json.loads((D/'prior-game-hashes.json').read_text()));sets={};report={}
for kind in ['general','elite','random']:
 folder=D/kind;m=json.loads((folder/'manifest.json').read_text());lines=folder.joinpath('all.uci-bundle').read_text().splitlines();entries=[json.loads(x) for x in gzip.decompress((folder/'entries.jsonl.gz').read_bytes()).decode().splitlines()]
 assert len(lines)==len(entries)==5000
 hashes=[hashlib.sha256(x.encode()).hexdigest() for x in lines];assert len(set(hashes))==5000 and not(set(hashes)&old)
 assert hashes==[x['sha256'] for x in entries];assert sum(len(x.split()) for x in lines)==m['plies'];assert all(len(s.split())==e['plies'] for s,e in zip(lines,entries))
 joined=[]
 for b in m['blocks']:
  p=D/b['path'];assert hashlib.sha256(p.read_bytes()).hexdigest()==b['sha256'];part=p.read_text().splitlines();assert len(part)==250;joined.extend(part)
 assert joined==lines
 ratings=[];times=collections.Counter();years=collections.Counter();gm=0;fide=0;focus=0;archives=0
 for e in entries:
  h=e['headers']
  if kind in ['general','elite']:
   rr=[int(h[k]) for k in ['WhiteElo','BlackElo']];ratings.extend(rr)
   if kind=='general':assert all(800<=x<=2200 for x in rr) and all(not h.get(k) for k in ['WhiteTitle','BlackTitle']) and int(h['TimeControl'].split('+')[0])>=60
   else:assert min(rr)>=2600
   assert h['Result'] in ['1-0','0-1','1/2-1/2'];assert e['plies']>=16
   gm+=int(all(h.get(k)=='GM' for k in ['WhiteTitle','BlackTitle']));fide+=int(all(h.get(k,'').isdigit() for k in ['WhiteFideId','BlackFideId']))
   times[h.get('TimeControl','not supplied')]+=1
   if re.fullmatch(r'\d{4}\.\d{2}\.\d{2}',h.get('Date','')):years[h['Date'][:4]]+=1
   if e['source'].startswith('archive-'):
    archives+=1;name=e['source'][8:].lower();assert any(name in re.sub('[^a-z]','',h[side].lower()) for side in ['White','Black']);focus+=1
  else:assert e['termination'] in ['cap512','checkmate','stalemate','insufficient_material','75_move','fivefold']
 report[kind]={'games':5000,'plies':m['plies'],'source_count':len(m['sources']),'players':m['players'],'no_prior_exact_game_overlap':True,'unique_whole_uci_games':5000,'all_block_hashes_and_counts_match':True,'rating_min':min(ratings) if ratings else None,'rating_median':statistics.median(ratings) if ratings else None,'rating_max':max(ratings) if ratings else None,'both_GM_tags_games':gm,'both_FIDE_ID_tags_games':fide,'secondary_player_archive_games':archives,'archive_focus_name_matches':focus,'time_controls':dict(times),'years':dict(sorted(years.items()))}
 sets[kind]=set(hashes)
report['cross_cohort_exact_overlap']={a+'_'+b:len(sets[a]&sets[b]) for a,b in [('general','elite'),('general','random'),('elite','random')]}
assert not any(report['cross_cohort_exact_overlap'].values())
(D/'audit.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:{x:v[x] for x in ['games','plies','players','source_count','rating_min','rating_max','both_GM_tags_games','both_FIDE_ID_tags_games','secondary_player_archive_games']} for k,v in report.items() if k!='cross_cohort_exact_overlap'},indent=2))
