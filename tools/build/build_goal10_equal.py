#!/usr/bin/env python3
"""Portable index ranks plus equal-counter opportunity shortcut."""
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',default='build-goal10-equal');p.add_argument('--verify',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[2];o=Path(a.out).resolve();o.mkdir(parents=True,exist_ok=True)
subprocess.run(['python3',str(r/'tools/generation/generate_goal10_equal.py')],check=True)
f=['g++','-std=c++20','-O3','-flto=1','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_DENSE_CATEGORY_UPDATES','-DDMT_FEATURE_LUT','-I'+str(r/'include')]
if a.verify:f+=['-DDMT_VERIFY_COUNT']
subprocess.run([*f,str(r/'tools/native/r19_g10equal.cpp'),str(r/'src/rmc.cpp'),'-o',str(o/'g10equal')],check=True)
print(o/'g10equal')
