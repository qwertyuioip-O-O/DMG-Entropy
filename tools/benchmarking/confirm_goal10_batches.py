#!/usr/bin/env python3
"""Fresh paired confirmation against fixed goal anchor; no repeated full suite."""
import argparse,csv,hashlib,json,math,os,resource,subprocess,time,statistics
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--baseline',required=True);p.add_argument('--candidate',required=True);p.add_argument('--out',required=True);p.add_argument('--cpu',type=int,default=1);a=p.parse_args()
r=Path(__file__).resolve().parents[2];out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True)
bins={k:str(Path(getattr(a,k)).resolve()) for k in ['baseline','candidate']};sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
assert sha(bins['baseline'])==json.loads((r/'benchmarks-goal10-20261023/goal.json').read_text())['anchor_binary_sha256']
data={k:r/('data-'+k)/'heldout.uci-bundle' for k in ['r19','r18','r17']}
def cg():
 p=Path('/sys/fs/cgroup/cpu.stat');return p.read_text() if p.exists() else 'unavailable'
protocol={'date':'2026-10-23','cpu':a.cpu,'rounds':4,'order':'alternating ABBA/BAAB','binaries':{k:{'path':v,'sha256':sha(v)} for k,v in bins.items()},'datasets':{k:sha(v) for k,v in data.items()},'goal':'both equal-dataset geometric mean gains >=10% versus fixed goal anchor; exploratory same-host confirmation only','validation':'reuse completed full suite and byte-identity screening for identical hashes; each CLI invocation still warms, verifies repeated bytes and roundtrip','cgroup_before':cg()}
protocol.update({'measurement_unit':'arithmetic mean of exactly two consecutive native CLI speed calls; every call retained','native_calls':96,'quality_unit':'48 two-call means, max repeat ratio 1.25; separately audit all native calls without trimming','scope':'prospective longer-window confirmation of unchanged phase wall-time objective on another core'})
(out/'protocol.json').write_text(json.dumps(protocol,indent=2));os.sched_setaffinity(0,{a.cpu})
raw=[];rawdir=out/'raw';rawdir.mkdir(exist_ok=True)
rows=[];summary={}
for d,bundle in data.items():
 gains={k:[] for k in ['encode','decode','process_cpu']}
 for block in range(4):
  order=['baseline','candidate','candidate','baseline'] if block%2==0 else ['candidate','baseline','baseline','candidate'];current=[]
  for pos,label in enumerate(order):
   samples=[]
   for native in range(2):
    before=resource.getrusage(resource.RUSAGE_CHILDREN);t0=time.monotonic()
    run=subprocess.run([bins[label],'speed',str(bundle),'builtin','3'],check=True,capture_output=True,text=True)
    after=resource.getrusage(resource.RUSAGE_CHILDREN);elapsed=time.monotonic()-t0
    mode,plies,enc,dec=run.stdout.strip().split(',');record={'dataset':d,'block':block,'position':pos,'native':native,'label':label,'plies':int(plies),'encode':float(enc),'decode':float(dec),'process_cpu':after.ru_utime+after.ru_stime-before.ru_utime-before.ru_stime,'process_wall':elapsed};raw.append(record);samples.append(record)
   row={'dataset':d,'block':block,'position':pos,'label':label,'plies':int(plies),**{k:sum(x[k] for x in samples)/2 for k in ['encode','decode','process_cpu','process_wall']}};rows.append(row);current.append(row)
  for k in gains:gains[k].append(math.log(sum(x[k] for x in current if x['label']=='baseline')/sum(x[k] for x in current if x['label']=='candidate')))
  tmp=out/'timings.tmp'
  with tmp.open('w') as f:
   w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
  tmp.replace(out/'timings.csv')
  with (rawdir/'timings.csv').open('w') as f:
   w=csv.DictWriter(f,fieldnames=raw[0]);w.writeheader();w.writerows(raw)
  print(d,'confirmation',block+1,'PASS',flush=True)
 summary[d]={k:{'gain_percent':100*math.expm1(sum(v)/len(v)),'faster_blocks':sum(x>0 for x in v),'block_gains_percent':[100*math.expm1(x) for x in v]} for k,v in gains.items()}
avg={k:100*math.expm1(sum(math.log1p(summary[d][k]['gain_percent']/100) for d in data)/len(data)) for k in ['encode','decode','process_cpu']}
result={'datasets':summary,'aggregate_gain_percent':avg,'both_10_percent_target_on_this_confirmation':avg['encode']>=10 and avg['decode']>=10,'cgroup_after':cg()}
for k,v in bins.items():assert sha(v)==protocol['binaries'][k]['sha256']
(out/'summary.json').write_text(json.dumps(result,indent=2))
(out/'throughput-medians.json').write_text(json.dumps({d:{label:{k:statistics.median(int(x['plies'])/x[k] for x in rows if x['dataset']==d and x['label']==label) for k in ['encode','decode']} for label in bins} for d in data},indent=2))
print(json.dumps(result,indent=2),flush=True)

subprocess.run(['python3',str(r/'tools/analysis/audit_goal10_measurements.py'),'--dir',str(rawdir)],check=True,stdout=subprocess.DEVNULL)
subprocess.run(['python3',str(r/'tools/analysis/audit_goal10_measurements.py'),'--dir',str(out),'--strict'],check=True)
