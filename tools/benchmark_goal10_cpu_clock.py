#!/usr/bin/env python3
"""CPU-phase diagnostic using frozen binaries; wall-goal evidence stays separate."""
from pathlib import Path
import argparse,csv,json,math,os,subprocess,hashlib,time,resource
p=argparse.ArgumentParser()
for k in ['baseline','stage1','candidate','clock-library','out']:p.add_argument('--'+k,required=True)
p.add_argument('--cpu',type=int,default=1);p.add_argument('--blocks',type=int,default=1,choices=[1,3]);a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=Path(a.out).resolve();o.mkdir(parents=True,exist_ok=True)
bins={k:str(Path(getattr(a,k)).resolve()) for k in ['baseline','stage1','candidate']};lib=str(Path(a.clock_library).resolve());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
data={k:r/('data-'+k)/'heldout.uci-bundle' for k in ['r19','r18','r17']}
orders=[['baseline','stage1','candidate','candidate','stage1','baseline'],['stage1','candidate','baseline','baseline','candidate','stage1'],['candidate','baseline','stage1','stage1','baseline','candidate']][:a.blocks]
protocol={'metric':'process CPU seconds for encode/decode phases through child-only clock_gettime interposition; NOT wall phase seconds','fixed_binaries':{k:{'path':v,'sha256':sha(v)} for k,v in bins.items()},'clock_library':{'path':lib,'sha256':sha(lib)},'cpu':a.cpu,'orders':orders,'datasets':{k:sha(v) for k,v in data.items()},'scope':'exploratory diagnostic; no production adoption on this sample alone; original wall-clock +10 percent goal is not changed','validation':'same executable hashes as full suite; clock probe; one preloaded mode3 pack per binary compared with saved native-clock pack bytes; each speed call still warm/repeated-byte/roundtrip checks'}
(o/'protocol.json').write_text(json.dumps(protocol,indent=2));os.sched_setaffinity(0,{a.cpu})
env=os.environ.copy();env['LD_PRELOAD']=lib+(':'+env['LD_PRELOAD'] if env.get('LD_PRELOAD') else '')
checks=[]
for label,binary in bins.items():
 f=o/(label+'.d19');subprocess.run([binary,'pack',str(data['r19']),'builtin','3',str(f)],check=True,env=env)
 native=r/'benchmarks-goal10-ratio-20261023'/f'r19-3-{label}.d19';assert f.read_bytes()==native.read_bytes(),label
 checks.append({'label':label,'sha256':sha(f),'bytes':f.stat().st_size})
(o/'clock-byte-identity.json').write_text(json.dumps(checks,indent=2));rows=[];gains={}
for d,bundle in data.items():
 gains[d]={label:{k:[] for k in ['encode_cpu','decode_cpu','process_cpu']} for label in ['baseline','stage1']}
 for block,order in enumerate(orders):
  q=[]
  for pos,label in enumerate(order):
   before=resource.getrusage(resource.RUSAGE_CHILDREN);start=time.monotonic()
   run=subprocess.run([bins[label],'speed',str(bundle),'builtin','3'],env=env,check=True,capture_output=True,text=True)
   after=resource.getrusage(resource.RUSAGE_CHILDREN);elapsed=time.monotonic()-start
   mode,plies,enc,dec=run.stdout.strip().split(',')
   row={'dataset':d,'block':block,'position':pos,'label':label,'plies':int(plies),'encode_cpu':float(enc),'decode_cpu':float(dec),'process_cpu':after.ru_utime+after.ru_stime-before.ru_utime-before.ru_stime,'process_wall':elapsed};q.append(row);rows.append(row)
  for label in ['baseline','stage1']:
   for k in gains[d][label]:gains[d][label][k].append(math.log(sum(x[k] for x in q if x['label']==label)/sum(x[k] for x in q if x['label']=='candidate')))
  with (o/'timings.csv').open('w') as f:
   w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
  print(d,'CPU diagnostic block',block+1,'PASS',flush=True)
summary={'metric':'CPU phase time, separate from wall-clock goal','per_dataset':{d:{label:{k:100*math.expm1(sum(v)/len(v)) for k,v in directions.items()} for label,directions in comparisons.items()} for d,comparisons in gains.items()},'aggregate_gain_percent':{label:{k:100*math.expm1(sum(sum(gains[d][label][k])/len(gains[d][label][k]) for d in gains)/len(gains)) for k in ['encode_cpu','decode_cpu','process_cpu']} for label in ['baseline','stage1']}}
for label,binary in bins.items():assert sha(binary)==protocol['fixed_binaries'][label]['sha256']
(o/'summary.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary,indent=2),flush=True)
