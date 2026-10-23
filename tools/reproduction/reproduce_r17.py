#!/usr/bin/env python3
"""Rebuild, verify, and optionally retrain/time the frozen R17 experiment."""
import argparse,subprocess,csv,json,hashlib,platform
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',default='build-r17');p.add_argument('--retrain',action='store_true');p.add_argument('--timing',action='store_true');p.add_argument('--blocks',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[2];o=Path(a.out).resolve();results=o/'results';results.mkdir(parents=True,exist_ok=True)
subprocess.run(['python3',str(r/'tools/build_r17.py'),'--out',str(o)],check=True);exe=o/'r17_prior'
def run(args,path):
 with path.open('w') as f:subprocess.run([str(exe),*map(str,args)],stdout=f,check=True)
run(['test',r/'benchmarks-r4/human4.uci-bundle','builtin'],results/'tests.log')
for name,path in [('new','data-r17/heldout.uci-bundle'),('wcc','benchmarks-r4/wcc2024_01_13.uci-bundle'),('r16confirm','data-r16-confirm/heldout.uci-bundle'),('r15','data-r15/heldout.uci-bundle')]:run(['eval',r/path,'builtin'],results/(name+'.csv'))
if a.retrain:
 subprocess.run([str(exe),'export',str(r/'data-r13/dev.uci-bundle'),str(results/'train.bin')],check=True)
 subprocess.run(['python3',str(r/'tools/train_r17.py'),str(results/'train.bin'),str(results/'priors')],check=True)
if a.blocks:run(['blocks',r/'data-r17/heldout.uci-bundle','builtin'],results/'blocks.csv')
if a.timing:
 rows=[]
 # All builds/tests precede this loop. No parallel benchmark processes.
 for dataset,path in [('new','data-r17/heldout.uci-bundle'),('r16confirm','data-r16-confirm/heldout.uci-bundle')]:
  for round in range(6):
   for mode in ([0,2] if round%2==0 else [2,0]):
    s=subprocess.check_output([str(exe),'speed',str(r/path),'builtin',str(mode)],text=True).strip().split(',');rows.append([dataset,round,*s]);print(dataset,round,mode,flush=True)
 with (results/'speed.csv').open('w') as f:w=csv.writer(f);w.writerow(['dataset','round','mode','plies','encode_seconds','decode_seconds']);w.writerows(rows)
 (results/'timing.json').write_text(json.dumps({'rounds':6,'repetitions':3,'warm_roundtrip':True,'balanced_order':True,'platform':platform.platform(),'source_sha256':hashlib.sha256((r/'tools/r17_prior.cpp').read_bytes()).hexdigest()},indent=2))
