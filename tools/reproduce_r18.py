#!/usr/bin/env python3
"""Portable frozen-model reproduction. Complete all tests before serial timing."""
import argparse,subprocess,csv,json,hashlib,platform
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',default='build-r18');p.add_argument('--timing',action='store_true');p.add_argument('--blocks',action='store_true');p.add_argument('--retrain',action='store_true');p.add_argument('--controls',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=Path(a.out).resolve();results=o/'results';results.mkdir(parents=True,exist_ok=True)
subprocess.run(['python3',str(r/'tools/build_r18.py'),'--out',str(o)],check=True);exe=o/'r18_prior'
def run(args,path):
 with path.open('w') as f:subprocess.run([str(exe),*map(str,args)],stdout=f,check=True)
run(['test',r/'benchmarks-r4/human4.uci-bundle','builtin'],results/'tests.log')
for name,path in [('new','data-r18/heldout.uci-bundle'),('wcc','benchmarks-r4/wcc2024_01_13.uci-bundle'),('r17','data-r17/heldout.uci-bundle'),('r16confirm','data-r16-confirm/heldout.uci-bundle'),('r15','data-r15/heldout.uci-bundle'),('human','benchmarks-r4/human4.uci-bundle')]:run(['eval',r/path,'builtin','0,9'],results/(name+'.csv'))
for bundle in sorted((r/'data-r18').glob('source*.uci-bundle')):run(['eval',bundle,'builtin','0,9'],results/(bundle.stem+'.csv'))
if a.retrain:
 subprocess.run([str(exe),'export',str(r/'data-r13/dev.uci-bundle'),str(results/'train.bin')],check=True)
 subprocess.run(['python3',str(r/'tools/train_r18.py'),str(results/'train.bin'),str(results/'priors')],check=True)
 if a.controls:subprocess.run(['python3',str(r/'tools/train_r18_control.py'),str(results/'train.bin'),str(results/'controls')],check=True)
if a.controls:
 for name,path in [('new','data-r18/heldout.uci-bundle'),('wcc','benchmarks-r4/wcc2024_01_13.uci-bundle'),('r17','data-r17/heldout.uci-bundle')]:run(['eval',r/path,r/'benchmarks-r18/control-frozen.txt','0,9'],results/('control-'+name+'.csv'))
if a.blocks:run(['blocks',r/'data-r18/heldout.uci-bundle','builtin'],results/'blocks.csv')
if a.timing:
 with (results/'speed.csv').open('w') as f:
  w=csv.writer(f);w.writerow(['dataset','round','mode','plies','encode_seconds','decode_seconds']);f.flush()
  for dataset,path in [('new','data-r18/heldout.uci-bundle'),('r17','data-r17/heldout.uci-bundle')]:
   for rnd in range(6):
    for mode in ([0,9] if rnd%2==0 else [9,0]):
     s=subprocess.check_output([str(exe),'speed',str(r/path),'builtin',str(mode)],text=True).strip().split(',');w.writerow([dataset,rnd,*s]);f.flush()
    print('timing',dataset,rnd+1,'/6',flush=True)
 (results/'timing.json').write_text(json.dumps({'rounds':6,'repetitions':3,'warm_roundtrip':True,'balanced_order':True,'no_parallel_build_or_tests':True,'platform':platform.platform(),'source_sha256':hashlib.sha256((r/'tools/r18_prior.cpp').read_bytes()).hexdigest()},indent=2))
