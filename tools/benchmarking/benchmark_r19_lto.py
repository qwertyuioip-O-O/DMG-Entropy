#!/usr/bin/env python3
"""Sequential pinned ABBA/BAAB CLI test of link-time optimization of the direct-output baseline."""
import argparse,csv,hashlib,json,math,os,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--baseline',required=True);p.add_argument('--candidate',required=True);p.add_argument('--out',required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[2];out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True)
bins={k:str(Path(v).resolve()) for k,v in [('baseline',a.baseline),('candidate',a.candidate)]}
data={k:r/('data-'+k)/'heldout.uci-bundle' for k in ['r19','r18','r17']}
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
protocol={'date':'2026-10-23','cpu':0,'binaries':{k:{'path':v,'sha256':sha(v)} for k,v in bins.items()},'datasets':{k:sha(v) for k,v in data.items()},'rounds':{'r19':4,'r18':4,'r17':4},'order':'ABBA/BAAB alternating','metric':'paired CLI phase wall-time throughput ratio; geometric mean','note':'Exploratory sequential same-host test; no generalization or statistical significance claim'}
(out/'protocol.json').write_text(json.dumps(protocol,indent=2));os.sched_setaffinity(0,{0})
# Independent binaries must agree on all supported modes for all three datasets.
checks=[]
for d,bundle in data.items():
 for mode in range(5):
  files=[]
  for label,binary in bins.items():
   f=out/f'{d}-{mode}-{label}.d19';subprocess.run([binary,'pack',str(bundle),'builtin',str(mode),str(f)],check=True);files.append(f)
  assert files[0].read_bytes()==files[1].read_bytes(),(d,mode)
  checks.append({'dataset':d,'mode':mode,'bytes':files[0].stat().st_size,'sha256':sha(files[0])})
 print('identity',d,'all five modes PASS',flush=True)
res=subprocess.run([bins['candidate'],'test',str(data['r19']),'builtin'],check=True,text=True,capture_output=True)
(out/'validation.json').write_text(json.dumps({'byte_identity':checks,'test_stdout':res.stdout},indent=2))
rows=[];summary={}
for d,bundle in data.items():
 gains={'encode':[],'decode':[]}
 for block in range(protocol['rounds'][d]):
  order=['baseline','candidate','candidate','baseline'] if block%2==0 else ['candidate','baseline','baseline','candidate']
  current=[]
  for ix,label in enumerate(order):
   run=subprocess.run([bins[label],'speed',str(bundle),'builtin','3'],check=True,capture_output=True,text=True)
   mode,plies,enc,dec=run.stdout.strip().split(',');row={'dataset':d,'block':block,'position':ix,'label':label,'plies':int(plies),'encode':float(enc),'decode':float(dec)};rows.append(row);current.append(row)
  for direction in gains:
   x=sum(v[direction] for v in current if v['label']=='baseline');y=sum(v[direction] for v in current if v['label']=='candidate');gains[direction].append(math.log(x/y))
  with (out/'timings.csv').open('w') as f:
   w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
  print(d,'block',block+1,'PASS',flush=True)
 summary[d]={k:{'gain_percent':100*math.expm1(sum(v)/len(v)),'faster_blocks':sum(x>0 for x in v),'blocks':len(v),'block_gains_percent':[100*math.expm1(x) for x in v]} for k,v in gains.items()}
(out/'summary.json').write_text(json.dumps(summary,indent=2))
for k,v in bins.items():assert sha(v)==protocol['binaries'][k]['sha256']
print(json.dumps(summary,indent=2),flush=True)
