#!/usr/bin/env python3
"""GCC LTO release build, exact same codec/model settings as ordinary build."""
import argparse,subprocess
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--out',default='build-r12-lto');ap.add_argument('--cxx',default='g++');args=ap.parse_args()
r=Path(__file__).resolve().parents[2];out=Path(args.out).resolve();out.mkdir(parents=True,exist_ok=True)
flags=[args.cxx,'-std=c++20','-O3','-DNDEBUG','-flto=auto','-DDMT_FAST_TACTICAL','-DDMT_FEATURE_LUT','-I'+str(r/'include'),'-I'+str(r/'src')]
for name in ['entropy','rmc']:subprocess.run(flags+['-c',str(r/'src'/f'{name}.cpp'),'-o',str(out/f'{name}.o')],check=True)
for name,source in [('r12_speed','tools/native/r12_speed.cpp'),('dmt11_audit','tools/native/dmt11_audit.cpp'),('dmt12_tests','tests/dmt12_tests.cpp'),('dmg_mid','tools/native/mid_cli.cpp')]:
 subprocess.run(flags+['-DRMC_SOURCE_DIR="'+str(r)+'"',str(r/source),str(out/'entropy.o'),str(out/'rmc.o'),'-o',str(out/name)],check=True)
for name in ['r12_speed','dmt11_audit','dmt12_tests','dmg_mid']:(out/name).chmod((out/name).stat().st_mode|0o111)
print('LTO build ready:',out)
