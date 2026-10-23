#!/usr/bin/env python3
"""Three-arm mirrored/rotated CLI comparison, fixed before measurement."""
import argparse,csv,hashlib,json,math,os,subprocess,statistics
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--baseline',required=True);p.add_argument('--stage1',required=True);p.add_argument('--candidate',required=True);p.add_argument('--out',required=True);p.add_argument('--cpu',type=int,default=1);a=p.parse_args()
r=Path(__file__).resolve().parents[1];out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True)
bins={k:str(Path(getattr(a,k)).resolve()) for k in ['baseline','stage1','candidate']};sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
data={k:r/('data-'+k)/'heldout.uci-bundle' for k in ['r19','r18','r17']}
orders=[['baseline','stage1','candidate','candidate','stage1','baseline'],['stage1','candidate','baseline','baseline','candidate','stage1'],['candidate','baseline','stage1','stage1','baseline','candidate']]
protocol={'date':'2026-10-23','cpu':a.cpu,'binaries':{k:{'path':v,'sha256':sha(v)} for k,v in bins.items()},'datasets':{k:sha(v) for k,v in data.items()},'orders':orders,'blocks_per_dataset':3,'primary_comparison':'candidate vs frozen goal10 scratch+LTO anchor','secondary_comparison':'candidate vs retained counted-rank stage1','metric':'paired sum-of-two CLI phase wall-time throughput ratios; geometric mean','scope':'exploratory, one host/core, serial data sessions; no significance claim'}
protocol['goal']='at least 10 percent aggregate throughput gain in BOTH encode and decode versus fixed anchor; do not compound gains from different studies'
protocol['selection']='compare retained counted ranks and compact index rank candidate directly to fixed anchor; favor larger minimum aggregate gain; screen on requested core, then confirm selected candidate on other core against fixed anchor'
(out/'protocol.json').write_text(json.dumps(protocol,indent=2));os.sched_setaffinity(0,{a.cpu})
checks=[]
for d,bundle in data.items():
 for mode in range(5):
  files=[]
  for label,binary in bins.items():
   f=out/f'{d}-{mode}-{label}.d19';subprocess.run([binary,'pack',str(bundle),'builtin',str(mode),str(f)],check=True);files.append(f)
  assert len({f.read_bytes() for f in files})==1,(d,mode)
  checks.append({'dataset':d,'mode':mode,'bytes':files[0].stat().st_size,'sha256':sha(files[0])})
 print('identity',d,'all modes, all three binaries PASS',flush=True)
result=subprocess.run([bins['candidate'],'test',str(data['r19']),'builtin'],check=True,capture_output=True,text=True)
(out/'validation.json').write_text(json.dumps({'three_arm_byte_identity':checks,'existing_tests':result.stdout},indent=2))
rows=[];summary={}
for d,bundle in data.items():
 gains={label:{k:[] for k in ['encode','decode']} for label in ['baseline','stage1']}
 for block,order in enumerate(orders):
  current=[]
  for pos,label in enumerate(order):
   result=subprocess.run([bins[label],'speed',str(bundle),'builtin','3'],check=True,capture_output=True,text=True)
   mode,plies,enc,dec=result.stdout.strip().split(',');row={'dataset':d,'block':block,'position':pos,'label':label,'plies':int(plies),'encode':float(enc),'decode':float(dec)};current.append(row);rows.append(row)
  for label in gains:
   for direction in gains[label]:
    x=sum(v[direction] for v in current if v['label']==label);y=sum(v[direction] for v in current if v['label']=='candidate');gains[label][direction].append(math.log(x/y))
  tmp=out/'timings.tmp'
  with tmp.open('w') as f:
   w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
  tmp.replace(out/'timings.csv');print(d,'block',block+1,'PASS',flush=True)
 summary[d]={label:{k:{'gain_percent':100*math.expm1(sum(v)/len(v)),'faster_blocks':sum(x>0 for x in v),'blocks':len(v),'block_gains_percent':[100*math.expm1(x) for x in v]} for k,v in directions.items()} for label,directions in gains.items()}
(out/'summary.json').write_text(json.dumps(summary,indent=2))
speeds={d:{label:{direction:statistics.median(int(x['plies'])/x[direction] for x in rows if x['dataset']==d and x['label']==label) for direction in ['encode','decode']} for label in bins} for d in data}
(out/'throughput-medians.json').write_text(json.dumps(speeds,indent=2))
for k,v in bins.items():assert sha(v)==protocol['binaries'][k]['sha256']
print(json.dumps(summary,indent=2),flush=True)
