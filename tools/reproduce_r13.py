#!/usr/bin/env python3
"""Rebuild, verify corpus/codec, optionally reproduce frozen comparisons/timing."""
import argparse,json,subprocess,sys
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--out',default='build-r13');ap.add_argument('--full',action='store_true');ap.add_argument('--timing',action='store_true');args=ap.parse_args()
r=Path(__file__).resolve().parents[1];out=Path(args.out).resolve();results=out/'results';results.mkdir(parents=True,exist_ok=True)
def run(command,path=None):
    if path:
        with Path(path).open('w') as f:subprocess.run(list(map(str,command)),stdout=f,check=True)
    else:subprocess.run(list(map(str,command)),check=True)
run([sys.executable,r/'tools/build_r13.py','--out',out])
run([sys.executable,r/'tools/audit_r13_data.py'],results/'data-audit.json')
for test in ['dmt_default_tests','dmt12_tests','dmt13_tests']:
    (out/test).chmod(0o755);run([out/test],results/(test+'.log'))
if args.full:
    run([out/'dmg_mid_tests'],results/'mid-tests.log')
    for name,masks in [('dev','127,126,125,123,119,111,95,63,255,103,99,98'),('select','95,111,126'),('sealed','95'),('source_holdout','95')]:
        run([out/'r13_matrix',name,r/f'data-r13/{name}.uci-bundle',masks],results/(name+'-structure.csv'))
    # Standalone immutable DMT12 implementation, same compilation settings.
    ref=r/'reference-r12';flags=['g++','-std=c++20','-O3','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_FEATURE_LUT','-I'+str(ref/'include')]
    for name in ['entropy','rmc']:run(flags+['-c',ref/f'src/{name}.cpp','-o',out/f'reference-{name}.o'])
    exe=out/'reference-audit';run(flags+[r/'tools/dmt11_audit.cpp',out/'reference-entropy.o',out/'reference-rmc.o','-o',exe]);exe.chmod(0o755)
    count=0
    for name,bundle in [('human23','benchmarks-r4/human23.uci-bundle'),('random48','benchmarks-r2/random48.uci-bundle')]:
        old=out/('old-'+name);new=out/('new-'+name)
        run([exe,r/bundle,old,251]);run([out/'dmt11_audit',r/bundle,new,251])
        for profile in range(251):
            assert (old/f'{profile}.dmt').read_bytes()==(new/f'{profile}.dmt').read_bytes();count+=1
    (results/'compatibility.json').write_text(json.dumps({'byte_identical':count}))
if args.timing:
    # Runs after every compiler and correctness process has finished.
    run([out/'r13_paired_speed',r/'data-r13/select.uci-bundle'],results/'paired-speed.csv')
    run([out/'r13_paired_speed',r/'data-r13/source_holdout.uci-bundle'],results/'paired-speed-holdout.csv')
    run([sys.executable,r/'tools/summarize_r13_speed.py','--directory',results])
    run([sys.executable,r/'tools/summarize_r13_speed.py','--directory',results,'--suffix=-holdout','--label','별도 대회 285판/19,942수'])
print('PASS; results:',results)
