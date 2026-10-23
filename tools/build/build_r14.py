#!/usr/bin/env python3
"""CMake-free reproducible release build. Run from any working directory."""
import argparse,subprocess
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--out',default='build-r14');ap.add_argument('--cxx',default='g++');ap.add_argument('--verify-reuse',action='store_true');ap.add_argument('--no-lut',action='store_true');ap.add_argument('--opportunity-cache',action='store_true');ap.add_argument('--reference-candidates',action='store_true');args=ap.parse_args()
root=Path(__file__).resolve().parents[2];out=Path(args.out).resolve();out.mkdir(parents=True,exist_ok=True)
flags=[args.cxx,'-std=c++20','-O3','-DNDEBUG','-DDMT_FAST_TACTICAL','-I'+str(root/'include'),'-I'+str(root/'src')]
if args.verify_reuse:flags+=['-DDMT_VERIFY_REUSE','-DDMT_VERIFY_JOINT_CANDIDATES','-DRMC_VERIFY_LEGAL_FAST']
if not args.no_lut:flags+=['-DDMT_FEATURE_LUT']
if args.opportunity_cache:flags+=['-DDMT_CACHE_OPPORTUNITY']
if args.reference_candidates:flags+=['-DDMT_REFERENCE_JOINT_CANDIDATES']
def run(*items):subprocess.run([*flags,*map(str,items)],check=True)
for name in ['entropy','rmc']:run('-c',root/'src'/f'{name}.cpp','-o',out/f'{name}.o')
objects=[out/'entropy.o',out/'rmc.o']
for name,source in [('dmg_mid','tools/native/mid_cli.cpp'),('dmg_mid_speed','tools/native/mid_speed.cpp'),('dmt9_experiments','tools/native/experiment_matrix.cpp'),('dmt11_audit','tools/native/dmt11_audit.cpp'),('dmt_default_tests','tests/default_profile_tests.cpp'),('dmt10_tests','tests/dmt10_tests.cpp'),('dmt11_tests','tests/dmt11_tests.cpp'),('dmg_mid_tests','tests/mid_tests.cpp'),('dmt12_tests','tests/dmt12_tests.cpp'),('r12_search','tools/native/r12_search.cpp'),('r12_speed','tools/native/r12_speed.cpp')]:
 run('-DRMC_SOURCE_DIR="'+str(root)+'"',root/source,*objects,'-o',out/name)
run(root/'tests/dmt11_kernel_tests.cpp',out/'rmc.o','-o',out/'dmt11_kernel_tests')
for name,source in [('r13_matrix','tools/native/r13_matrix.cpp'),('r13_speed','tools/native/r13_speed.cpp'),('r13_paired_speed','tools/native/r13_paired_speed.cpp'),('r13_loss_trace','tools/native/r13_loss_trace.cpp'),('dmt13_tests','tests/dmt13_tests.cpp')]:
 run('-DRMC_SOURCE_DIR="'+str(root)+'"',root/source,*objects,'-o',out/name)
 (out/name).chmod(0o755)
for name,source in [('r14_matrix','tools/native/r14_matrix.cpp'),('r14_loss_trace','tools/native/r14_loss_trace.cpp'),('r14_paired_speed','tools/native/r14_paired_speed.cpp'),('dmt14_tests','tests/dmt14_tests.cpp')]:
 run('-DRMC_SOURCE_DIR="'+str(root)+'"',root/source,*objects,'-o',out/name)
 (out/name).chmod(0o755)
for name in ['dmg_mid','dmg_mid_speed','dmt9_experiments','dmt11_audit','dmt_default_tests','dmt10_tests','dmt11_tests','dmg_mid_tests','dmt12_tests','r12_search','r12_speed','dmt11_kernel_tests']:
 (out/name).chmod((out/name).stat().st_mode|0o111)
print('Built:',out)
