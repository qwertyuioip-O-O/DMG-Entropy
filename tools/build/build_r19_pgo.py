#!/usr/bin/env python3
"""LTO+PGO using dev games disjoint from the three timing datasets."""
import argparse,hashlib,json,subprocess,os
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',default='build-r19-pgo');a=p.parse_args()
r=Path(__file__).resolve().parents[2];o=Path(a.out).resolve();o.mkdir(parents=True,exist_ok=True)
profiles=o/'profiles';profiles.mkdir(exist_ok=True)
# Fresh training run; never merge profiles from an earlier experiment.
for f in profiles.rglob('*.gcda'):f.unlink()
normalize=lambda line:' '.join(line.split())
heldout=set()
for k in ['r19','r18','r17']:heldout.update(normalize(x) for x in (r/f'data-{k}/heldout.uci-bundle').read_text().splitlines() if x.strip())
source=r/'data-r13/dev.uci-bundle'
selected=sorted({normalize(x) for x in source.read_text().splitlines() if x.strip()}-heldout,key=lambda x:hashlib.sha256(x.encode()).hexdigest())[:160]
assert selected and not set(selected)&heldout
training=o/'pgo-training.uci-bundle';training.write_text('\n'.join(selected)+'\n')
manifest={'source':str(source),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'selection':'first 160 SHA256-sorted unique normalized dev13 games after excluding all exact full-game sequences in three timing datasets','training_games':len(selected),'training_plies':sum(len(x.split()) for x in selected),'training_sha256':hashlib.sha256(training.read_bytes()).hexdigest(),'exact_game_overlap_with_timing_data':0,'purpose':'compiler execution profile only; no coefficient fitting','compiler':subprocess.check_output(['g++','--version'],text=True).splitlines()[0]}
(o/'training-manifest.json').write_text(json.dumps(manifest,indent=2))
binary=o/'r19_pgo'
base=['g++','-std=c++20','-O3','-flto=1','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_DENSE_CATEGORY_UPDATES','-DDMT_FEATURE_LUT','-I'+str(r/'include'),'-I'+str(r/'src')]
sources=[str(r/'tools/native/r19_inplace.cpp'),str(r/'src/rmc.cpp')]
commands=[]
cmd=[*base,'-fprofile-generate='+str(profiles),*sources,'-o',str(binary)];commands.append(cmd);subprocess.run(cmd,check=True)
print('instrumented build complete',flush=True)
run=subprocess.run([str(binary),'speed',str(training),'builtin','3'],check=True,text=True,capture_output=True)
(o/'training-run.txt').write_text(run.stdout);assert list(profiles.rglob('*.gcda'))
print('dev-only profile complete',flush=True)
cmd=[*base,'-fprofile-use='+str(profiles),'-Werror=missing-profile','-Werror=coverage-mismatch',*sources,'-o',str(binary)];commands.append(cmd);subprocess.run(cmd,check=True)
(o/'build-commands.json').write_text(json.dumps(commands,indent=2))
print('profile-use build complete',binary,flush=True)
