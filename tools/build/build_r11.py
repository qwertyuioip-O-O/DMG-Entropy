#!/usr/bin/env python3
"""CMake-free reproducible release build. Run from any working directory."""
import argparse,subprocess
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--out',default='build-r11');ap.add_argument('--cxx',default='g++');ap.add_argument('--verify-reuse',action='store_true');args=ap.parse_args()
root=Path(__file__).resolve().parents[2];out=Path(args.out).resolve();out.mkdir(parents=True,exist_ok=True)
flags=[args.cxx,'-std=c++20','-O3','-DNDEBUG','-DDMT_FAST_TACTICAL','-I'+str(root/'include'),'-I'+str(root/'src')]
if args.verify_reuse:flags+=['-DDMT_VERIFY_REUSE']
def run(*items):subprocess.run([*flags,*map(str,items)],check=True)
for name in ['entropy','rmc']:run('-c',root/'src'/f'{name}.cpp','-o',out/f'{name}.o')
objects=[out/'entropy.o',out/'rmc.o']
for name,source in [('dmg_mid','tools/native/mid_cli.cpp'),('dmg_mid_speed','tools/native/mid_speed.cpp'),('dmt9_experiments','tools/native/experiment_matrix.cpp'),('dmt11_audit','tools/native/dmt11_audit.cpp'),('dmt_default_tests','tests/default_profile_tests.cpp'),('dmt10_tests','tests/dmt10_tests.cpp'),('dmt11_tests','tests/dmt11_tests.cpp')]:
 run('-DRMC_SOURCE_DIR="'+str(root)+'"',root/source,*objects,'-o',out/name)
run(root/'tests/dmt11_kernel_tests.cpp',out/'rmc.o','-o',out/'dmt11_kernel_tests')
print('Built:',out)
