#!/usr/bin/env python3
"""GCC PGO build. Compiler profiles use new_dev + random48, never new_sealed."""
import argparse,subprocess,json
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--out',default='build-r12-pgo');ap.add_argument('--cxx',default='g++');args=ap.parse_args()
r=Path(__file__).resolve().parents[1];out=Path(args.out).resolve();out.mkdir(parents=True,exist_ok=True);profiles=out/'profiles';profiles.mkdir(exist_ok=True)
for old in profiles.rglob('*.gcda'):old.unlink()
flags=[args.cxx,'-std=c++20','-O3','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_FEATURE_LUT','-I'+str(r/'include')]
objects=[out/'entropy.o',out/'rmc.o',out/'speed.o'];sources=[r/'src/entropy.cpp',r/'src/rmc.cpp',r/'tools/r12_speed.cpp']
def build(profile_flag):
 for source,obj in zip(sources,objects):subprocess.run(flags+[profile_flag,'-c',str(source),'-o',str(obj)],check=True)
 subprocess.run(flags+[profile_flag,*map(str,objects),'-o',str(out/'r12_speed')],check=True)
build('-fprofile-generate='+str(profiles))
(out/'r12_speed').chmod((out/'r12_speed').stat().st_mode|0o111)
training=[(234,r/'data-r12/new_dev.uci-bundle'),(250,r/'data-r12/new_dev.uci-bundle'),(234,r/'benchmarks-r2/random48.uci-bundle'),(250,r/'benchmarks-r2/random48.uci-bundle')]
for profile,bundle in training:subprocess.run([out/'r12_speed',str(profile),bundle],check=True)
build('-fprofile-use='+str(profiles))
for name,source in [('dmg_mid','tools/mid_cli.cpp'),('dmt11_audit','tools/dmt11_audit.cpp'),('dmt12_tests','tests/dmt12_tests.cpp')]:
 # New wrapper translation units are intentionally not profile-trained.
 subprocess.run(flags+['-DRMC_SOURCE_DIR="'+str(r)+'"',str(r/source),str(out/'entropy.o'),str(out/'rmc.o'),'-o',str(out/name)],check=True)
(out/'training.json').write_text(json.dumps({'compiler_profiles_only':True,'new_sealed_used':False,'training':[(id,str(path.relative_to(r))) for id,path in training]},indent=2))
for name in ['r12_speed','dmg_mid','dmt11_audit','dmt12_tests']:(out/name).chmod((out/name).stat().st_mode|0o111)
print('PGO build ready:',out)
