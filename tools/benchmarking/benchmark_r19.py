#!/usr/bin/env python3
"""Reproducible native R19 comparison; no third-party Python dependencies."""
import argparse,csv,hashlib,json,math,os,platform,resource,statistics,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write_csv(path,rows):
 with path.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--baseline',type=Path,default=ROOT/'benchmarks-goal10-20261023/anchor/r19_scratch')
 p.add_argument('--candidate',type=Path,default=ROOT/'benchmarks/goal10-static-20261023/selected/g10static')
 p.add_argument('--dataset',action='append',metavar='LABEL=PATH',help='Repeat to override the three bundled held-out sets')
 p.add_argument('--out',type=Path,required=True);p.add_argument('--rounds',type=int,default=4);p.add_argument('--batch',type=int,default=2)
 p.add_argument('--cpu',type=int);p.add_argument('--mode',type=int,choices=range(5),default=3)
 a=p.parse_args()
 if a.rounds<1 or a.batch<1:p.error('rounds and batch must be positive')
 out=a.out.resolve()
 if out.exists() and any(out.iterdir()):p.error('output directory must be empty or new')
 bins={k:getattr(a,k).resolve(strict=True) for k in ['baseline','candidate']}
 data={}
 for spec in a.dataset or [f'{k}={ROOT}/data-{k}/heldout.uci-bundle' for k in ['r19','r18','r17']]:
  if '=' not in spec:p.error('dataset must be LABEL=PATH')
  label,path=spec.split('=',1)
  if not label or label in data:p.error('dataset labels must be nonempty and unique')
  data[label]=Path(path).resolve(strict=True)
 affinity=None
 if a.cpu is not None:
  if not hasattr(os,'sched_setaffinity'):p.error('--cpu is unavailable on this OS; omit it')
  os.sched_setaffinity(0,{a.cpu});affinity=sorted(os.sched_getaffinity(0))
 out.mkdir(parents=True,exist_ok=True)
 protocol={'system':platform.system(),'machine':platform.machine(),'affinity':affinity,'mode':a.mode,'rounds':a.rounds,'native_calls_per_unit':a.batch,'native_speed_calls':len(data)*a.rounds*4*a.batch,'order':'alternating ABBA / BAAB','metric':'equal-dataset geometric mean of paired phase wall-time throughput ratios','unit':'arithmetic mean of consecutive native CLI speed calls; each CLI warms and checks three encode/decode iterations','repeat_range_threshold':1.25,'outliers_removed':0,'binaries':{k:{'path':str(v),'sha256':sha(v)} for k,v in bins.items()},'datasets':{k:{'path':str(v),'sha256':sha(v)} for k,v in data.items()},'interpretation':'point estimates on this host; quality gate is not a confidence interval or an every-dataset/CPU guarantee'}
 fixed=json.loads((ROOT/'benchmarks-goal10-20261023/goal.json').read_text())['anchor_binary_sha256']
 protocol['original_goal_anchor_matches']=sha(bins['baseline'])==fixed
 (out/'protocol.json').write_text(json.dumps(protocol,indent=2))
 sizes={}
 for number,(label,bundle) in enumerate(data.items()):
  packs=[]
  for name,binary in bins.items():
   dest=out/f'archive-{number}-{name}.d19'
   subprocess.run([str(binary),'pack',str(bundle),'builtin',str(a.mode),str(dest)],check=True,capture_output=True,text=True)
   packs.append(dest.read_bytes())
  if packs[0]!=packs[1]:raise RuntimeError(f'compressed byte mismatch: {label}')
  sizes[label]={'archive_bytes':len(packs[0]),'archive_sha256':hashlib.sha256(packs[0]).hexdigest(),'source_bytes':bundle.stat().st_size}
 raw=[];units=[]
 start=time.monotonic()
 for label,bundle in data.items():
  for block in range(a.rounds):
   order=['baseline','candidate','candidate','baseline'] if block%2==0 else ['candidate','baseline','baseline','candidate']
   for position,name in enumerate(order):
    samples=[]
    for native in range(a.batch):
     before=resource.getrusage(resource.RUSAGE_CHILDREN);t=time.monotonic()
     result=subprocess.run([str(bins[name]),'speed',str(bundle),'builtin',str(a.mode)],check=True,capture_output=True,text=True)
     elapsed=time.monotonic()-t;after=resource.getrusage(resource.RUSAGE_CHILDREN)
     mode,plies,enc,dec=result.stdout.strip().split(',')
     if int(mode)!=a.mode or int(plies)<=0 or min(float(enc),float(dec))<=0:raise RuntimeError('invalid speed result')
     row={'dataset':label,'block':block,'position':position,'label':name,'native':native,'plies':int(plies),'encode':float(enc),'decode':float(dec),'process_cpu':after.ru_utime+after.ru_stime-before.ru_utime-before.ru_stime,'process_wall':elapsed}
     samples.append(row);raw.append(row);write_csv(out/'native.csv',raw)
    unit={k:samples[0][k] for k in ['dataset','block','position','label','plies']}
    unit.update({k:statistics.mean(s[k] for s in samples) for k in ['encode','decode','process_cpu','process_wall']});units.append(unit);write_csv(out/'units.csv',units)
   print(f'{label}: {block+1}/{a.rounds}',flush=True)
 summary={};ranges=[];gains={k:[] for k in ['encode','decode']}
 for label in data:
  rows=[x for x in units if x['dataset']==label];summary[label]={**sizes[label],'plies':rows[0]['plies'],'binaries':{}}
  for name in bins:
   rr=[x for x in rows if x['label']==name];summary[label]['binaries'][name]={}
   for phase in gains:
    times=[x[phase] for x in rr];ratio=max(times)/min(times)
    ranges.append({'dataset':label,'label':name,'phase':phase,'samples':len(times),'max_over_min':ratio})
    summary[label]['binaries'][name][phase+'_plies_per_second']=statistics.median(x['plies']/x[phase] for x in rr)
  for phase in gains:
   logs=[]
   for block in range(a.rounds):
    sums={name:sum(x[phase] for x in rows if x['block']==block and x['label']==name) for name in bins}
    logs.append(math.log(sums['baseline']/sums['candidate']))
   log=statistics.mean(logs);gains[phase].append(log);summary[label][phase+'_gain_percent']=100*math.expm1(log)
  summary[label]['archive_bits_per_ply']=8*sizes[label]['archive_bytes']/rows[0]['plies']
 aggregate={k:100*math.expm1(statistics.mean(v)) for k,v in gains.items()}
 quality={'unit_range_passed':all(x['max_over_min']<=1.25 for x in ranges),'threshold':1.25,'unit_ranges':ranges,'raw_rows':len(raw),'unit_rows':len(units),'rows_removed':0,'scope':'range gate on declared batch means; raw native times remain available; not proof of a small effect'}
 for k,v in bins.items():assert sha(v)==protocol['binaries'][k]['sha256']
 for k,v in data.items():assert sha(v)==protocol['datasets'][k]['sha256']
 result={'datasets':summary,'aggregate_gain_percent':aggregate,'quality':quality,'original_goal_point_target_observed':protocol['original_goal_anchor_matches'] and min(aggregate.values())>=10,'elapsed_seconds':time.monotonic()-start}
 (out/'summary.json').write_text(json.dumps(result,indent=2))
 lines=['# R19 benchmark','','Native CLI phase wall-time. Throughput columns are medians; gains are paired geometric means. Framed archive size includes headers and checksum.','','| Dataset | Plies | Bytes | Bits/ply | Encode baseline → candidate (plies/s) | Decode baseline → candidate (plies/s) | Encode gain | Decode gain |','|---|---:|---:|---:|---:|---:|---:|---:|']
 for label,v in summary.items():
  b=v['binaries']['baseline'];c=v['binaries']['candidate'];safe=label.replace('|','\\|').replace('\n',' ')
  lines.append(f"| {safe} | {v['plies']:,} | {v['archive_bytes']:,} | {v['archive_bits_per_ply']:.4f} | {b['encode_plies_per_second']:,.0f} → {c['encode_plies_per_second']:,.0f} | {b['decode_plies_per_second']:,.0f} → {c['decode_plies_per_second']:,.0f} | {v['encode_gain_percent']:+.2f}% | {v['decode_gain_percent']:+.2f}% |")
 lines += ['',f"Aggregate paired gain: encode {aggregate['encode']:+.2f}%, decode {aggregate['decode']:+.2f}%.",f"Range gate on {a.batch}-call mean units: {'PASS' if quality['unit_range_passed'] else 'FAIL'}. {len(raw)} native calls; none removed.",'','These are exploratory point estimates. Short smoke runs verify the runner, not a reliable performance claim. A passing range gate does not establish a confidence bound above 10%. See native.csv for individual variation and protocol.json for exact binaries, inputs and ordering.']
 (out/'report.md').write_text('\n'.join(lines)+'\n');print(json.dumps({'aggregate_gain_percent':aggregate,'unit_range_passed':quality['unit_range_passed'],'report':str(out/'report.md')},indent=2))
if __name__=='__main__':main()
