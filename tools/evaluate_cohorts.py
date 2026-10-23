#!/usr/bin/env python3
"""Evaluate frozen general / elite / uniform-legal cohorts, without model tuning."""
import argparse,csv,hashlib,json,math,os,platform,random,resource,statistics,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def csvwrite(p,rows):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
def main():
 p=argparse.ArgumentParser();p.add_argument('--data',type=Path,default=ROOT/'data-cohorts-20261023');p.add_argument('--out',type=Path,required=True);p.add_argument('--native',type=Path,default=ROOT/'benchmarks-goal10-static-20261023/selected/g10static');p.add_argument('--phase',type=Path,required=True);p.add_argument('--cpu',type=int,default=1);a=p.parse_args()
 data=a.data.resolve();out=a.out.resolve();native=a.native.resolve();phase=a.phase.resolve()
 if out.exists() and any(out.iterdir()):p.error('output must be empty/new')
 policy=json.loads((data/'precommit.json').read_text());assert sha(native)==policy['binary_sha256'];os.sched_setaffinity(0,{a.cpu});out.mkdir(parents=True,exist_ok=True)
 inputs={};manifests={}
 for name in ['general','elite','random']:
  manifest=json.loads((data/name/'manifest.json').read_text());assert manifest['games']==5000 and len(manifest['blocks'])==20;manifests[name]=manifest
  inputs[name]=[]
  for x in manifest['blocks']:
   path=data/x['path'];assert sha(path)==x['sha256'];lines=path.read_text().splitlines();assert len(lines)==250
   inputs[name].append({'path':path,'sha256':sha(path),'plies':sum(len(line.split()) for line in lines)})
 protocol={'native_sha256':sha(native),'phase_binary_sha256':sha(phase),'cpu':a.cpu,'machine':platform.machine(),'policy':policy,'native_calls':120,'phase_calls':60,'phase_repetitions':2,'native_repetitions':'two independent CLI calls per block; each CLI warms and times 3 repeats','schedule':'phase calls interleave cohort by block; native round0 forward and round1 reverse block/cohort order','report':'phase CDF costs and diagnostic phase wall time; untouched release native whole-file throughput; no model changes','intervals':'10000 fixed-seed percentile bootstrap resamples of 20 entire blocks; conditional on sampled data, not a population/source-independent CI','manifests':{name:sha(data/name/'manifest.json') for name in manifests}}
 protocol['cpu_model']=next((line.split(':',1)[1].strip() for line in Path('/proc/cpuinfo').read_text().splitlines() if line.startswith('model name')),'unknown')
 (out/'protocol.json').write_text(json.dumps(protocol,indent=2));(out/'manifests.json').write_text(json.dumps(manifests,indent=2))
 # Byte identity for the clock-instrumented implementation against frozen release.
 oracles=[]
 for name in inputs:
  blobs=[]
  for label,binary in [('native',native),('phase',phase)]:
   dest=out/f'{name}-{label}-sample.d19';subprocess.run([str(binary),'pack',str(inputs[name][0]['path']),'builtin','3',str(dest)],check=True,capture_output=True,text=True);blobs.append(dest.read_bytes())
  assert blobs[0]==blobs[1];oracles.append({'cohort':name,'block':0,'bytes':len(blobs[0]),'sha256':hashlib.sha256(blobs[0]).hexdigest()})
 (out/'instrumentation-byte-oracles.json').write_text(json.dumps(oracles,indent=2))
 phase_rows=[];native_rows=[]
 for block in range(20):
  for name in inputs:
   x=inputs[name][block];run=subprocess.run([str(phase),'phase',str(x['path']),'builtin','2'],check=True,capture_output=True,text=True)
   rows=list(csv.DictReader(run.stdout.splitlines()));assert len(rows)==6
   for iteration in [0,1]:assert sum(int(y['plies']) for y in rows if int(y['iteration'])==iteration)==x['plies']
   for row in rows:phase_rows.append({'cohort':name,'block':block,**row})
   csvwrite(out/'phase.csv',phase_rows)
  print('phase blocks',block+1,'/20',flush=True)
 for rep in range(2):
  order=range(20) if rep==0 else reversed(range(20));cohorts=list(inputs) if rep==0 else list(reversed(inputs))
  for block in order:
   for name in cohorts:
    x=inputs[name][block];before=resource.getrusage(resource.RUSAGE_CHILDREN);t=time.monotonic()
    run=subprocess.run([str(native),'speed',str(x['path']),'builtin','3'],check=True,capture_output=True,text=True)
    elapsed=time.monotonic()-t;after=resource.getrusage(resource.RUSAGE_CHILDREN);mode,plies,enc,dec=run.stdout.strip().split(',');assert int(mode)==3 and int(plies)==x['plies']
    native_rows.append({'cohort':name,'block':block,'rep':rep,'plies':int(plies),'encode':float(enc),'decode':float(dec),'process_wall':elapsed,'process_cpu':after.ru_utime+after.ru_stime-before.ru_utime-before.ru_stime});csvwrite(out/'native.csv',native_rows)
   print('native',rep+1,'block',block+1,flush=True)
 for name in inputs:
  for x in inputs[name]:assert sha(x['path'])==x['sha256']
 assert sha(native)==protocol['native_sha256'] and sha(phase)==protocol['phase_binary_sha256']
 summarize(out,manifests,phase_rows,native_rows)
def summarize(out,manifests,phase_rows,native_rows):
 rng=random.Random(20261023);draws=[[rng.randrange(20) for _ in range(20)] for _ in range(10000)];result={}
 def metric(blocks,num,den):
  def get(indices):
   a=sum(blocks[i][num] for i in indices);b=sum(blocks[i][den] for i in indices);return a/b if b else None
  point=get(range(20));values=sorted(v for d in draws if (v:=get(d)) is not None)
  return {'value':point,'ci95':[values[249],values[9749]] if values else [None,None]}
 for name,m in manifests.items():
  blocks=[];stages=[[] for _ in range(3)];repeat_ratios=[]
  for block in range(20):
   pp=[x for x in phase_rows if x['cohort']==name and int(x['block'])==block];nn=[x for x in native_rows if x['cohort']==name and int(x['block'])==block];assert len(pp)==6 and len(nn)==2
   plies=sum(int(x['plies']) for x in pp if int(x['iteration'])==0);first=pp[0]
   blocks.append({'plies':plies,'payload_bits':int(first['payload_bits']),'archive_bits':8*int(first['archive_bytes']),'encode_seconds':statistics.mean(float(x['encode']) for x in nn),'decode_seconds':statistics.mean(float(x['decode']) for x in nn)})
   for k in ['encode','decode']:repeat_ratios.append({'block':block,'phase':k,'max_over_min':max(float(x[k]) for x in nn)/min(float(x[k]) for x in nn)})
   for stage in range(3):
    rows=[x for x in pp if int(x['stage'])==stage];assert len(rows)==2 and rows[0]['plies']==rows[1]['plies'] and rows[0]['cdf_bits']==rows[1]['cdf_bits']
    stages[stage].append({'plies':int(rows[0]['plies']),'cdf_bits':float(rows[0]['cdf_bits']),'encode_seconds':statistics.mean(float(x['encode_seconds']) for x in rows),'decode_seconds':statistics.mean(float(x['decode_seconds']) for x in rows)})
  overall={k:metric(blocks,*fields) for k,fields in {'payload_bit_per_ply':('payload_bits','plies'),'framed_bit_per_ply':('archive_bits','plies'),'native_encode_plies_per_second':('plies','encode_seconds'),'native_decode_plies_per_second':('plies','decode_seconds')}.items()}
  phase={label:{'plies':sum(x['plies'] for x in stages[s]),'cdf_bit_per_ply':metric(stages[s],'cdf_bits','plies'),'diagnostic_encode_plies_per_second':metric(stages[s],'plies','encode_seconds'),'diagnostic_decode_plies_per_second':metric(stages[s],'plies','decode_seconds')} for s,label in enumerate(['opening','middlegame','endgame'])}
  assert sum(v['plies'] for v in phase.values())==m['plies']
  result[name]={'games':m['games'],'plies':m['plies'],'sources':m['sources'],'overall':overall,'phases':phase,'native_repeat_max_over_min':repeat_ratios,'native_repeat_groups_over_1_25':sum(x['max_over_min']>1.25 for x in repeat_ratios),'blocks':blocks}
 (out/'summary.json').write_text(json.dumps(result,indent=2))
 lines=['# Three-cohort R19 evaluation — 2026-10-23','','Frozen R19 builtin mode3 / g10static. 5,000 games per cohort; 20 equal files of 250 games. No retraining or candidate selection from these outcomes. Native overall speed uses the untouched selected executable; phase speeds use the separately instrumented same-model build.','','## Whole-file results','','| Cohort | Games | Plies | Actual payload bit/ply | Framed file bit/ply | Native encode plies/s | Native decode plies/s |','|---|---:|---:|---:|---:|---:|---:|']
 for name,v in result.items():
  x=v['overall'];lines.append(f"| {name} | {v['games']:,} | {v['plies']:,} | {x['payload_bit_per_ply']['value']:.4f} | {x['framed_bit_per_ply']['value']:.4f} | {x['native_encode_plies_per_second']['value']:,.0f} | {x['native_decode_plies_per_second']['value']:,.0f} |")
 lines+=['','## Phase results','','CDF costs below are -log2 of the actual integer coding probabilities, not separately framed per-phase byte streams. Diagnostic phase timers cover move prediction, arithmetic coding, learning and board advancement; per-game initialization and framing/CRC are outside those phase timers. Timestamp calls occur only on phase changes and game ends; the per-ply counter/branch remains. Do not directly equate a diagnostic phase speed with untouched native whole-file speed.','','| Cohort | Phase | Plies | CDF bit/ply (95% block interval) | Diagnostic encode plies/s | Diagnostic decode plies/s |','|---|---|---:|---:|---:|---:|']
 for name,v in result.items():
  for stage,x in v['phases'].items():
   c=x['cdf_bit_per_ply'];lines.append(f"| {name} | {stage} | {x['plies']:,} | {c['value']:.4f} ({c['ci95'][0]:.4f}–{c['ci95'][1]:.4f}) | {x['diagnostic_encode_plies_per_second']['value']:,.0f} | {x['diagnostic_decode_plies_per_second']['value']:,.0f} |")
 lines+=['','## Interpretation and limitations','','- Original small sets were 1,317 online arena games / 89,230 plies, not a three-population validation. Earlier 3.2-bit middlegame figures must not be generalized to elite or random play.','- General cohort: untitled rated online users, both online Elo 800..2200, base clock >=60s, arena exports plus deterministic participant-history supplementation. This is not a random sample of all humans or classical chess.','- Elite cohort: real broadcast tournament games plus public world-leading-player PGN archives from kinairi/chess-data, frozen at a Git commit; both players must have PGN-reported Elo >=2600; GM/FIDE-ID tag coverage is reported because historical exports omit tags. All qualifying games in each selected archive/event are candidates; max1000 per source before global dedup and source balancing. The rating/time-control context differs from online arena ratings; event/player-archive selection, secondary mirror provenance and historical date range remain sampling limitations.','- Random cohort: exactly uniform legal-move sampling per ply with independent deterministic seeds; automatic endings or 512-ply cap. Capped games remain included. This tests a specified synthetic process, not every imaginable random distribution.','- Exact prior UCI games and within-cohort duplicates are excluded. Players/events can be correlated, and rating/title/identity tags are source-provided and some identity tags are missing.','- All 120 native calls and 360 instrumented phase rows are retained; no slow runs trimmed. Repeated native run differences are reported in JSON and can include scheduling/frequency noise.','- Intervals resample entire 250-game blocks (10,000 fixed-seed draws). They describe variation conditional on these selected cohorts, not independent-event/player population confidence. One host/core is tested.','- Phase definition matches the codec: opening if ply<24 and weighted material phase >=18; endgame if material phase <=8; otherwise middlegame. Material phase = min(24, knights + bishops + 2*rooks + 4*queens), both sides.','- Every native speed call checks deterministic encoding and exact roundtrip. Every phase invocation checks all repeated bytes/roundtrips and phase counts. Byte identity with the selected native executable is explicitly checked on the first block of each cohort; source proof confirms removal of only timing hooks restores the original wrapper.','','Frozen inputs, hashes, rating/source metadata and exclusions are in `data-cohorts-20261023`. Detailed intervals and all times are in `summary.json`, `phase.csv`, `native.csv`, and `protocol.json`.']
 (out/'report.md').write_text('\n'.join(lines)+'\n');print(json.dumps({name:v['overall'] for name,v in result.items()},indent=2),flush=True)
if __name__=='__main__':main()
