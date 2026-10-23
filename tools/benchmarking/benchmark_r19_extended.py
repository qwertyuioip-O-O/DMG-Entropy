#!/usr/bin/env python3
"""Fixed-protocol, sequential speed study. Never changes models or codec code."""
import argparse,csv,datetime,hashlib,io,json,os,platform,resource,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--binaries',required=True);p.add_argument('--r18',required=True);p.add_argument('--out',required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[2];out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True)
pair=Path(a.binaries).resolve()/'r19_pair_speed';native19=Path(a.binaries).resolve()/'r19_prior';native18=Path(a.r18).resolve();allowed=sorted(os.sched_getaffinity(0));cpus=[];cores=set()
for cpu in allowed:
 base=Path(f'/sys/devices/system/cpu/cpu{cpu}/topology')
 key=tuple((base/n).read_text().strip() if (base/n).exists() else str(cpu) for n in ['physical_package_id','core_id'])
 if key not in cores:cpus.append(cpu);cores.add(key)
 if len(cpus)==2:break
if len(cpus)!=2:raise RuntimeError('Need two distinct allowed cores for the fixed protocol')
datasets={'new':r/'data-r19/heldout.uci-bundle','r18':r/'data-r18/heldout.uci-bundle','r17':r/'data-r17/heldout.uci-bundle'}
sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
def counters():
 path=Path('/sys/fs/cgroup/cpu.stat');return dict(line.split() for line in path.read_text().splitlines()) if path.exists() else {}
def atomic(path,text):
 tmp=path.with_suffix(path.suffix+'.tmp');tmp.write_text(text);tmp.replace(path)
def csvtext(fields,rows):
 buf=io.StringIO();w=csv.DictWriter(buf,fieldnames=fields);w.writeheader();w.writerows(rows);return buf.getvalue()
protocol={'client_date':'2026-10-23','platform':platform.platform(),'allowed_cpus':allowed,'chosen_cpus':cpus,'selection':'first two distinct cores, selected before measurements','in_process_rounds':8,'in_process_order':'ABBA/BAAB alternating per direction/round','native_cli_rounds':6,'native_cli_order':'ABBA/BAAB alternating each round','native_cli_dataset':'new','native_cli_cpu':cpus[0],'primary_metric':'geometric mean of paired CPU-time throughput ratios, per fixed dataset/core stratum','secondary_metrics':['paired wall-time ratios','actual CLI direction times','end-to-end native CLI child CPU time'],'decision':'No global speed-win claim unless both directions improve in all six CPU/dataset strata and actual CLI comparison is positive; exploratory intervals do not establish other-hardware performance','no_parallel_benchmark_build_training_or_tests':True,'source_sha256':sha(__file__),'binaries':{str(x):sha(x) for x in [pair,native19,native18]},'datasets':{k:{'path':str(v),'sha256':sha(v),'games':len(v.read_text().splitlines()),'plies':sum(len(x.split()) for x in v.read_text().splitlines())} for k,v in datasets.items()}}
protocol_path=out/'protocol.json'
if protocol_path.exists():
 if json.loads(protocol_path.read_text())!=protocol:raise RuntimeError('Existing study protocol differs; choose a new output path')
else:atomic(protocol_path,json.dumps(protocol,indent=2)+'\n')
state_path=out/'sessions.json';state=json.loads(state_path.read_text()) if state_path.exists() else []
schedule=[(cpus[0],'new'),(cpus[1],'r18'),(cpus[0],'r17'),(cpus[1],'new'),(cpus[0],'r18'),(cpus[1],'r17')]
original=os.sched_getaffinity(0)
try:
 for index,(cpu,ds) in enumerate(schedule):
  key=f'cpu{cpu}-{ds}';target=out/(key+'.csv')
  if target.exists():
   rows=list(csv.DictReader(target.open()));assert len(rows)==64;assert any(x['session']==key for x in state);print('resume verified',key,flush=True);continue
  print('START',index+1,'/6',key,flush=True);os.sched_setaffinity(0,{cpu});before=counters();start=time.time()
  got=subprocess.run([str(pair),str(datasets[ds])],check=True,text=True,stdout=subprocess.PIPE)
  rows=list(csv.DictReader(io.StringIO(got.stdout)));assert len(rows)==64
  for rnd in range(8):
   for direction in ['encode','decode']:
    group=[x for x in rows if int(x['round'])==rnd and x['direction']==direction];assert len(group)==4
    for mode in ['0','3']:assert sum(x['mode']==mode for x in group)==2
  assert all(int(x['plies'])==protocol['datasets'][ds]['plies'] and float(x['cpu_seconds'])>0 and float(x['wall_seconds'])>0 for x in rows)
  record={'session':key,'cpu':cpu,'dataset':ds,'start_utc':datetime.datetime.fromtimestamp(start,datetime.timezone.utc).isoformat(),'elapsed_seconds':time.time()-start,'cgroup_before':before,'cgroup_after':counters(),'records':len(rows),'sha256':hashlib.sha256(got.stdout.encode()).hexdigest()};atomic(target,got.stdout);state.append(record);atomic(state_path,json.dumps(state,indent=2)+'\n');print('DONE',key,record['elapsed_seconds'],flush=True)
 # Direct comparison of the original released executables, same dataset/core.
 os.sched_setaffinity(0,{cpus[0]});native_path=out/'native-cli.csv';native=list(csv.DictReader(native_path.open())) if native_path.exists() else []
 fields=['round','slot','variant','mode','plies','encode_seconds','decode_seconds','process_wall_seconds','process_cpu_seconds','nr_throttled_delta','throttled_usec_delta']
 for rnd in range(6):
  if sum(int(x['round'])==rnd for x in native)==4:print('resume native',rnd,flush=True);continue
  assert not any(int(x['round'])==rnd for x in native)
  order=['r18','r19','r19','r18'] if rnd%2==0 else ['r19','r18','r18','r19'];block=[]
  for slot,variant in enumerate(order):
   exe,mode=(native18,'9') if variant=='r18' else (native19,'3');before=counters();u0=resource.getrusage(resource.RUSAGE_CHILDREN);w0=time.perf_counter();got=subprocess.run([str(exe),'speed',str(datasets['new']),'builtin',mode],check=True,text=True,stdout=subprocess.PIPE);wall=time.perf_counter()-w0;u1=resource.getrusage(resource.RUSAGE_CHILDREN);after=counters();values=got.stdout.strip().split(',');assert len(values)==4 and values[0]==mode and int(values[1])==protocol['datasets']['new']['plies']
   row=dict(zip(['mode','plies','encode_seconds','decode_seconds'],values));row.update({'round':rnd,'slot':slot,'variant':variant,'process_wall_seconds':wall,'process_cpu_seconds':u1.ru_utime+u1.ru_stime-u0.ru_utime-u0.ru_stime,'nr_throttled_delta':int(after.get('nr_throttled',0))-int(before.get('nr_throttled',0)),'throttled_usec_delta':int(after.get('throttled_usec',0))-int(before.get('throttled_usec',0))});block.append(row)
  native.extend(block);atomic(native_path,csvtext(fields,native));print('native',rnd+1,'/6',flush=True)
finally:os.sched_setaffinity(0,original)
atomic(out/'COMPLETE.json',json.dumps({'sessions':len(state),'paired_records':384,'native_records':len(native),'codec_and_models_unchanged':True,'binary_sha256_verified':all(sha(Path(k))==v for k,v in protocol['binaries'].items())},indent=2)+'\n');print('STUDY COMPLETE',flush=True)
