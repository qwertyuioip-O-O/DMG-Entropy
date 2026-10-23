#!/usr/bin/env python3
"""Audited scratch initialization optimization with retained LTO flags."""
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',default='build-r19-scratch');a=p.parse_args()
r=Path(__file__).resolve().parents[2];o=Path(a.out).resolve();o.mkdir(parents=True,exist_ok=True)
subprocess.run(['python3',str(r/'tools/generation/generate_r19_scratch.py')],check=True)
f=['g++','-std=c++20','-O3','-flto=1','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_DENSE_CATEGORY_UPDATES','-DDMT_FEATURE_LUT','-I'+str(r/'include'),'-I'+str(r/'src')]
subprocess.run([*f,str(r/'tools/native/r19_scratch.cpp'),str(r/'src/rmc.cpp'),'-o',str(o/'r19_scratch')],check=True)
print(o/'r19_scratch')
