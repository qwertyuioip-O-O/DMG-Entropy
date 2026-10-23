#!/usr/bin/env python3
"""Build isolated incremental slider-ray candidate, preserving frozen R19."""
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',default='build-r19-rays');a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=Path(a.out).resolve();o.mkdir(parents=True,exist_ok=True)
subprocess.run(['python3',str(r/'tools/generate_r19_rays.py')],check=True)
f=['g++','-std=c++20','-O3','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_DENSE_CATEGORY_UPDATES','-DDMT_FEATURE_LUT','-I'+str(r/'include')]
subprocess.run([*f,'-c',str(r/'src/rmc.cpp'),'-o',str(o/'rmc.o')],check=True)
subprocess.run([*f,str(r/'tools/r19_rays.cpp'),str(o/'rmc.o'),'-o',str(o/'r19_rays')],check=True)
print(o/'r19_rays')
