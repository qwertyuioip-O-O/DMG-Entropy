#!/usr/bin/env python3
"""Reproduce the frozen fused-model experiments; build/tests precede timing."""
import argparse,subprocess,csv,json,hashlib,platform
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',default='build-r19');p.add_argument('--timing',action='store_true');p.add_argument('--blocks',action='store_true');p.add_argument('--retrain',action='store_true');p.add_argument('--controls',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[2];o=Path(a.out).resolve();results=o/'results';results.mkdir(parents=True,exist_ok=True)
subprocess.run(['python3',str(r/'tools/build/build_r19.py'),'--out',str(o)],check=True);exe=o/'r19_prior'
def run(args,path):
 with path.open('w') as f:subprocess.run([str(exe),*map(str,args)],stdout=f,check=True)
run(['test',r/'benchmarks-r4/human4.uci-bundle','builtin'],results/'tests.log')
run(['analyze',r/'benchmarks-r4/human4.uci-bundle','builtin'],results/'human-oracle.csv')
for name,path in [('new','data-r19/heldout.uci-bundle'),('wcc','benchmarks-r4/wcc2024_01_13.uci-bundle'),('r18','data-r18/heldout.uci-bundle'),('r17','data-r17/heldout.uci-bundle'),('r16confirm','data-r16-confirm/heldout.uci-bundle'),('human','benchmarks-r4/human4.uci-bundle')]:run(['eval',r/path,'builtin','0,1,3'],results/(name+'.csv'))
for bundle in sorted((r/'data-r19').glob('source*.uci-bundle')):run(['eval',bundle,'builtin','0,1,3'],results/(bundle.stem+'.csv'))
if a.retrain:
 subprocess.run([str(exe),'export',str(r/'data-r13/dev.uci-bundle'),str(results/'train.bin')],check=True)
 subprocess.run(['python3',str(r/'tools/training/train_r19.py'),str(results/'train.bin'),str(results/'priors')],check=True)
if a.controls:
 for name,path in [('new','data-r19/heldout.uci-bundle'),('wcc','benchmarks-r4/wcc2024_01_13.uci-bundle'),('r18','data-r18/heldout.uci-bundle')]:
  for label,mode,file in [('plain',2,'m2-reg0.0001.txt'),('geometry',4,'m4-reg0.001.txt')]:run(['eval',r/path,r/'benchmarks-r19/priors'/file,'0,'+str(mode)],results/(label+'-'+name+'.csv'))
if a.blocks:
 run(['blocks',r/'data-r19/heldout.uci-bundle','builtin'],results/'blocks-raw.csv')
 with (results/'blocks-raw.csv').open() as f:reader=csv.DictReader(f);fields=reader.fieldnames;block_rows=[x for x in reader if x['mode'] in ['0','3']]
 with (results/'blocks.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(block_rows)
if a.timing:
 with (results/'speed.csv').open('w') as f:
  w=csv.writer(f);w.writerow(['dataset','round','mode','plies','encode_seconds','decode_seconds']);f.flush()
  for dataset,path in [('new','data-r19/heldout.uci-bundle'),('r18','data-r18/heldout.uci-bundle')]:
   for rnd in range(6):
    modes=[0,1,3];k=rnd%3;modes=modes[k:]+modes[:k]
    if rnd>=3:modes.reverse()
    for mode in modes:
     s=subprocess.check_output([str(exe),'speed',str(r/path),'builtin',str(mode)],text=True).strip().split(',');w.writerow([dataset,rnd,*s]);f.flush()
    print('timing',dataset,rnd+1,'/6',flush=True)
 (results/'timing.json').write_text(json.dumps({'rounds':6,'variants':[0,1,3],'repetitions':3,'warm_roundtrip':True,'balanced_order':True,'no_parallel_build_or_tests':True,'platform':platform.platform(),'source_sha256':{str(x.relative_to(r)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [r/'tools/native/r19_prior.cpp',r/'tools/native/r19_model.hpp',r/'src/generated/dmt19_exp.hpp',r/'src/generated/dmt19_prior.hpp']}},indent=2))
