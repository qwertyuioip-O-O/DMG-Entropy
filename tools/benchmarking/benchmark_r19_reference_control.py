#!/usr/bin/env python3
"""Posthoc diagnosis: original R18 vs identical R18 path embedded in R19."""
import argparse,csv,hashlib,io,json,os,resource,subprocess,time,math,statistics
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('study');a=p.parse_args();d=Path(a.study).resolve();protocol=json.loads((d/'protocol.json').read_text());bins={Path(k).name:Path(k) for k in protocol['binaries']};r18=bins['r18_prior'];r19=bins['r19_prior'];bundle=Path(protocol['datasets']['new']['path']);cpu=protocol['native_cli_cpu'];original=os.sched_getaffinity(0)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert all(sha(k)==v for k,v in protocol['binaries'].items())
meta={'type':'posthoc diagnosis after primary results; not used to replace primary decision','comparison':'original R18 mode9 vs identical R18 reference path in R19 mode0','rounds':4,'order':'ABBA/BAAB alternating','cpu':cpu,'source_sha256':sha(__file__)};(d/'reference-control-protocol.json').write_text(json.dumps(meta,indent=2))
rows=[]
try:
 os.sched_setaffinity(0,{cpu})
 subprocess.run([str(r18),'pack',str(bundle),'builtin','9',str(d/'control-native-r18.d18')],check=True)
 subprocess.run([str(r19),'pack',str(bundle),'builtin','0',str(d/'control-embedded-r18.d19')],check=True)
 old=(d/'control-native-r18.d18').read_bytes();embedded=(d/'control-embedded-r18.d19').read_bytes();assert old[10:-4]==embedded[10:-4];meta['game_lengths_payload_bits_and_payload_identical']=True;meta['native_bytes']=len(old);meta['embedded_bytes']=len(embedded)
 for rnd in range(4):
  order=['native','embedded','embedded','native'] if rnd%2==0 else ['embedded','native','native','embedded']
  for slot,label in enumerate(order):
   exe,mode=(r18,'9') if label=='native' else (r19,'0');u0=resource.getrusage(resource.RUSAGE_CHILDREN);w0=time.perf_counter();got=subprocess.check_output([str(exe),'speed',str(bundle),'builtin',mode],text=True);wall=time.perf_counter()-w0;u1=resource.getrusage(resource.RUSAGE_CHILDREN);values=got.strip().split(',');assert len(values)==4 and values[0]==mode
   row=dict(zip(['mode','plies','encode_seconds','decode_seconds'],values));row.update({'round':rnd,'slot':slot,'variant':label,'process_wall_seconds':wall,'process_cpu_seconds':u1.ru_utime+u1.ru_stime-u0.ru_utime-u0.ru_stime});rows.append(row)
  buf=io.StringIO();w=csv.DictWriter(buf,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows);tmp=d/'reference-control.csv.tmp';tmp.write_text(buf.getvalue());tmp.replace(d/'reference-control.csv');print('reference control',rnd+1,'/4',flush=True)
finally:os.sched_setaffinity(0,original)
summary={}
for metric in ['encode_seconds','decode_seconds','process_cpu_seconds','process_wall_seconds']:
 logs=[]
 for rnd in range(4):
  group=[x for x in rows if x['round']==rnd];base=sum(float(x[metric]) for x in group if x['variant']=='native');candidate=sum(float(x[metric]) for x in group if x['variant']=='embedded');logs.append(math.log(base/candidate))
 summary[metric]={'embedded_gain_geomean_percent':100*math.expm1(statistics.mean(logs)),'faster_blocks':sum(x>0 for x in logs),'each_block_percent':[100*math.expm1(x) for x in logs]}
(d/'reference-control-protocol.json').write_text(json.dumps(meta,indent=2));(d/'reference-control-summary.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary,indent=2))
