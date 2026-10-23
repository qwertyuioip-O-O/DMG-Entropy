#!/usr/bin/env python3
"""Reproduce the frozen split experiment without tuning on heldout results."""
import argparse,json,subprocess,sys
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--out',default='build-r15');ap.add_argument('--full',action='store_true');ap.add_argument('--timing',action='store_true');args=ap.parse_args()
r=Path(__file__).resolve().parents[2];out=Path(args.out).resolve();results=out/'results';results.mkdir(parents=True,exist_ok=True)
def run(command,path=None):
    p=subprocess.run(list(map(str,command)),capture_output=path is not None,text=True,check=True)
    if path:Path(path).write_text(p.stdout)
run([sys.executable,r/'tools/build/build_r15.py','--out',out])
run([sys.executable,r/'tools/analysis/audit_r15_data.py'],results/'data-audit.json')
for test in ['dmt_default_tests','dmt12_tests','dmt13_tests','dmt14_tests','dmt15_tests']:
    (out/test).chmod(0o755);run([out/test],results/(test+'.log'))
if args.full:
    run([out/'dmg_mid_tests'],results/'mid-tests.log')
    dev=','.join(['0:1','1:1','6:1']+[f'{m}:{c}' for m in [2,3,4,5] for c in [1,2,4]])
    bank=','.join(f'{m}:{c}' for m in [7,8,9] for c in [1,2,4])
    for name,bundle,configs in [('dev','data-r13/dev.uci-bundle',dev),('dev-bank','data-r13/dev.uci-bundle',bank),('select','data-r13/select.uci-bundle','7:2,7:4,2:1,0:1'),('heldout','data-r15/heldout.uci-bundle','7:4,0:1')]:
        run([out/'r15_matrix',name,r/bundle,configs],results/(name+'.csv'))
    for source in ['blitz1700','blitz_open','rapid1500','superblitz_open']:
        run([out/'r15_matrix',source,r/f'data-r15/{source}.uci-bundle','7:4,0:1'],results/f'source-{source}.csv')
    for name,bundle in [('wcc13','benchmarks-r4/wcc2024_01_13.uci-bundle'),('human4','benchmarks-r4/human4.uci-bundle'),('r14_heldout','data-r14/heldout.uci-bundle'),('r13_sealed','data-r13/sealed.uci-bundle')]:
        run([out/'r15_matrix',name,r/bundle,'7:4,0:1'],results/f'regression-{name}.csv')
    ref=r/'reference-r14';flags=['g++','-std=c++20','-O3','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_FEATURE_LUT','-I'+str(ref/'include')]
    for name in ['entropy','rmc']:run(flags+['-c',ref/f'src/{name}.cpp','-o',out/f'reference-{name}.o'])
    exe=out/'reference-audit';run(flags+[r/'tools/native/dmt11_audit.cpp',out/'reference-entropy.o',out/'reference-rmc.o','-o',exe]);exe.chmod(0o755)
    count=0
    for name,bundle in [('human23','benchmarks-r4/human23.uci-bundle'),('random48','benchmarks-r2/random48.uci-bundle')]:
        old=out/('old-'+name);new=out/('new-'+name);run([exe,r/bundle,old,253]);run([out/'dmt11_audit',r/bundle,new,253])
        for profile in range(253):assert (old/f'{profile}.dmt').read_bytes()==(new/f'{profile}.dmt').read_bytes();count+=1
    (results/'compatibility.json').write_text(json.dumps({'byte_identical':count}))
    for selector,name in [('252','baseline'),('7:1','rate1'),('7:4','candidate')]:
        run([out/'r15_loss_trace',selector,r/'data-r13/dev.uci-bundle',results/f'dev-loss-{name}.csv'])
    run([sys.executable,r/'tools/analysis/analyze_r15_losses.py','--directory',results])
if args.timing:
    for name,bundle in [('select','data-r13/select.uci-bundle'),('heldout','data-r15/heldout.uci-bundle')]:run([out/'r15_paired_speed',r/bundle],results/f'speed-{name}.csv')
run([sys.executable,r/'tools/reporting/summarize_r15.py','--directory',results])
print('PASS; results:',results)
