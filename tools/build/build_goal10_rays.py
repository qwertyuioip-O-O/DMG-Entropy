#!/usr/bin/env python3
from pathlib import Path
import subprocess,argparse
p=argparse.ArgumentParser();p.add_argument('--out',default='build-goal10-rays');a=p.parse_args()
r=Path(__file__).resolve().parents[2];o=Path(a.out).resolve();o.mkdir(parents=True,exist_ok=True)
subprocess.run(['python3',str(r/'tools/generation/generate_goal10_rays.py')],check=True)
f=['g++','-std=c++20','-O3','-flto=1','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_DENSE_CATEGORY_UPDATES','-DDMT_FEATURE_LUT','-I'+str(r/'include')]
subprocess.run([*f,str(r/'tools/native/r19_g10index.cpp'),str(r/'src/rmc_g10rays.cpp'),'-o',str(o/'g10rays')],check=True);print(o/'g10rays')
