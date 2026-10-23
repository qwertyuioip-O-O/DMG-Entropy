#!/usr/bin/env python3
"""Reproduce the frozen DMT14 experiment; does not tune on heldout results."""
import argparse,json,subprocess,sys
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--out',default='build-r14');ap.add_argument('--full',action='store_true');ap.add_argument('--timing',action='store_true');args=ap.parse_args()
r=Path(__file__).resolve().parents[2];out=Path(args.out).resolve();results=out/'results';results.mkdir(parents=True,exist_ok=True)
def run(command,path=None):
    p=subprocess.run(list(map(str,command)),capture_output=path is not None,text=True,check=True)
    if path:Path(path).write_text(p.stdout)
run([sys.executable,r/'tools/build/build_r14.py','--out',out])
run([sys.executable,r/'tools/analysis/audit_r14_data.py'],results/'data-audit.json')
for test in ['dmt_default_tests','dmt12_tests','dmt13_tests','dmt14_tests']:
    (out/test).chmod(0o755);run([out/test],results/(test+'.log'))
if args.full:
    run([out/'dmg_mid_tests'],results/'mid-tests.log')
    dev=','.join(['0:4']+[f'{m}:{rate}' for m in [1,2,3,5,6,7] for rate in [2,4,8]])
    additional=','.join(['0:1','0:2','0:8']+[f'{m}:1' for m in [1,2,3,5,6,7]])
    for name,bundle,configs in [('dev','data-r13/dev.uci-bundle',dev),('dev-additional','data-r13/dev.uci-bundle',additional),('select','data-r13/select.uci-bundle','7:1,3:1,2:1,0:1'),('heldout','data-r14/heldout.uci-bundle','7:1,0:1')]:
        run([out/'r14_matrix',name,r/bundle,configs],results/(name+'.csv'))
    for source in ['blitz1500','rapid1300','superblitz1700','superblitz_open']:
        run([out/'r14_matrix',source,r/f'data-r14/{source}.uci-bundle','7:1,0:1'],results/f'source-{source}.csv')
    for name,bundle in [('wcc13','benchmarks-r4/wcc2024_01_13.uci-bundle'),('human4','benchmarks-r4/human4.uci-bundle'),('r13_sealed','data-r13/sealed.uci-bundle'),('r13_source_holdout','data-r13/source_holdout.uci-bundle'),('r12_sealed','data-r12/new_sealed.uci-bundle')]:
        run([out/'r14_matrix',name,r/bundle,'7:1,0:1'],results/f'regression-{name}.csv')
    ref=r/'reference-r13';flags=['g++','-std=c++20','-O3','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_FEATURE_LUT','-I'+str(ref/'include')]
    for name in ['entropy','rmc']:run(flags+['-c',ref/f'src/{name}.cpp','-o',out/f'reference-{name}.o'])
    exe=out/'reference-audit';run(flags+[r/'tools/native/dmt11_audit.cpp',out/'reference-entropy.o',out/'reference-rmc.o','-o',exe]);exe.chmod(0o755)
    count=0
    for name,bundle in [('human23','benchmarks-r4/human23.uci-bundle'),('random48','benchmarks-r2/random48.uci-bundle')]:
        old=out/('old-'+name);new=out/('new-'+name);run([exe,r/bundle,old,252]);run([out/'dmt11_audit',r/bundle,new,252])
        for profile in range(252):assert (old/f'{profile}.dmt').read_bytes()==(new/f'{profile}.dmt').read_bytes();count+=1
    (results/'compatibility.json').write_text(json.dumps({'byte_identical':count}))
if args.timing:
    for name,bundle in [('select','data-r13/select.uci-bundle'),('heldout','data-r14/heldout.uci-bundle')]:run([out/'r14_paired_speed',r/bundle],results/f'speed-{name}.csv')
run([sys.executable,r/'tools/reporting/summarize_r14.py','--directory',results])
print('PASS; results:',results)
