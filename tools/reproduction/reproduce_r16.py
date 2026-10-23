#!/usr/bin/env python3
"""Replay both frozen hypotheses; never fit parameters on confirmation data."""
import argparse,json,subprocess,sys
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--out',default='build-r16');ap.add_argument('--full',action='store_true');ap.add_argument('--timing',action='store_true');args=ap.parse_args()
r=Path(__file__).resolve().parents[2];out=Path(args.out).resolve();results=out/'results';results.mkdir(parents=True,exist_ok=True);dense=out/'dense-reference'
def run(command,path=None):
    p=subprocess.run(list(map(str,command)),capture_output=path is not None,text=True,check=True)
    if path:Path(path).write_text(p.stdout)
run([sys.executable,r/'tools/build/build_r16.py','--out',out,'--dense-updates'])
for name,tool in [('data-audit','audit_r16_data.py'),('confirmation-data-audit','audit_r16_confirm_data.py')]:run([sys.executable,r/'tools'/tool],results/(name+'.json'))
for test in ['dmt_default_tests','dmt12_tests','dmt13_tests','dmt14_tests','dmt15_tests','dmt16_tests','dmt16_u32_tests']:
    (out/test).chmod(0o755);run([out/test],results/(test+'.log'))
if args.full:
    dense.mkdir(parents=True,exist_ok=True)
    frozen_flags=['g++','-std=c++20','-O3','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_FEATURE_LUT','-DDMT_DENSE_CATEGORY_UPDATES','-I'+str(r/'include'),'-I'+str(r/'src'),'-I'+str(r/'src')]
    run(frozen_flags+['-c',r/'benchmarks-r16/frozen-entropy.cpp','-o',dense/'entropy.o'])
    for name in ['r16_audit','r16_dump']:
        run(frozen_flags+[r/f'tools/native/{name}.cpp',dense/'entropy.o',out/'rmc.o','-o',dense/name]);(dense/name).chmod(0o755)
if args.full or args.timing:
    ref=r/'reference-r15';flags=['g++','-std=c++20','-O3','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_FEATURE_LUT','-I'+str(ref/'include'),'-I'+str(ref/'src')]
    for name in ['entropy','rmc']:run(flags+['-c',ref/f'src/{name}.cpp','-o',out/f'reference-{name}.o'])
    previous_speed=out/'previous252-speed';run(flags+[r/'tools/native/r16_reference_speed.cpp',out/'reference-entropy.o',out/'reference-rmc.o','-o',previous_speed]);previous_speed.chmod(0o755)
if args.full:
    run([out/'dmg_mid_tests'],results/'mid-tests.log')
    dev=','.join(['0:1:32']+[f'{layout}:{rate}:{cap}' for layout in [1,2,3,4,5] for rate in [1,2] for cap in [32,64]])
    caps=','.join(f'{layout}:{rate}:128' for layout in [0,1,5] for rate in [1,2])
    tasks=[('dev','data-r13/dev.uci-bundle',dev),('dev-controls','data-r13/dev.uci-bundle','0:1:64,0:2:32,0:2:64'),('dev-cap','data-r13/dev.uci-bundle',caps),('select','data-r13/select.uci-bundle','1:1:128,5:1:128,1:2:128,0:1:128,0:2:128,0:1:32'),('heldout','data-r16/heldout.uci-bundle','1:2:128,0:2:128,0:1:32'),('confirm','data-r16-confirm/heldout.uci-bundle','0:2:128,1:2:128,0:1:32')]
    for name,bundle,configs in tasks:run([out/'r16_matrix',name,r/bundle,configs],results/(name+'.csv'))
    for folder,prefix in [('data-r16','source'),('data-r16-confirm','confirm-source')]:
        m=json.loads((r/folder/'manifest.json').read_text())
        for source in sorted(m['source_stats']):run([out/'r16_matrix',source,r/folder/(source+'.uci-bundle'),'1:2:128,0:2:128,0:1:32'],results/f'{prefix}-{source}.csv')
    for name,bundle in [('wcc13','benchmarks-r4/wcc2024_01_13.uci-bundle'),('human4','benchmarks-r4/human4.uci-bundle'),('r15_heldout','data-r15/heldout.uci-bundle'),('r14_heldout','data-r14/heldout.uci-bundle'),('r13_sealed','data-r13/sealed.uci-bundle')]:run([out/'r16_matrix',name,r/bundle,'1:2:128,0:2:128,0:1:32'],results/f'regression-{name}.csv')
    run([out/'r16_blocks',r/'data-r16/heldout.uci-bundle'],results/'block-sizes.csv')
    exe=out/'reference-audit';run(flags+[r/'tools/native/dmt11_audit.cpp',out/'reference-entropy.o',out/'reference-rmc.o','-o',exe]);exe.chmod(0o755);count=exact=0
    for name,bundle in [('human23','benchmarks-r4/human23.uci-bundle'),('random48','benchmarks-r2/random48.uci-bundle')]:
        old=out/('old-'+name);new=out/('new-'+name);dense_dir=out/('dense-'+name)
        run([exe,r/bundle,old,254]);run([out/'dmt11_audit',r/bundle,new,255]);run([dense/'r16_audit',r/bundle,dense_dir,249,254])
        for profile in range(254):assert (old/f'{profile}.dmt').read_bytes()==(new/f'{profile}.dmt').read_bytes();count+=1
        for profile in range(249,254):assert (dense_dir/f'{profile}.dmt').read_bytes()==(new/f'{profile}.dmt').read_bytes();exact+=1
        for layout in [0,1]:
            a=out/(name+f'-layout{layout}-candidate.dmt');b=out/(name+f'-layout{layout}-dense.dmt');run([out/'r16_dump',r/bundle,a,layout,2,128]);run([dense/'r16_dump',r/bundle,b,layout,2,128]);assert a.read_bytes()==b.read_bytes();exact+=1
    (results/'compatibility-counts.json').write_text(json.dumps({'legacy_byte_identical':count,'dense_sparse_byte_identical':exact}))
if args.timing:
    # Every compiler, test and codec validation above has exited before timing.
    run([sys.executable,r/'tools/benchmarking/benchmark_r16.py','--candidate',out/'r16_speed','--dense',previous_speed,'--kernel','dense_final','--rounds','6','--directory',results/'final-default'])
run([sys.executable,r/'tools/reporting/summarize_r16.py','--directory',results])
if args.timing:run([sys.executable,r/'tools/reporting/summarize_r16.py','--directory',results/'final-default'])
print('PASS; results:',results)
